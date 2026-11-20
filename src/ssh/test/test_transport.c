/*
 * test_transport.c -- Transport layer tests for DeuceSSH.
 *
 * Tier 2 layer tests with mock I/O.  Covers version validators,
 * algorithm negotiation, key derivation, version exchange, binary
 * packet protocol, transport message handling, full handshake, and
 * rekeying.
 */

#include <stdatomic.h>
#include <stddef.h>
#include <string.h>
#include <time.h>
#include <threads.h>
#include <unistd.h>

#include "dssh_test.h"
#include "dssh_test_wait.h"
#include "dssh_test_alloc.h"
#include "dssh_test_ossl.h"
#include "deucessh.h"
#include "deucessh-algorithms.h"
#include "kex/dh-gex-sha256.h"
#include "kex/dh-gex-groups.h"
#include "kex/dh-gex-sha256-ops.h"
#include "kex/curve25519-sha256-ops.h"
#include "kex/hybrid-pq-kex-ops.h"
#include "ssh-trans.h"
#include "ssh-internal.h"
#include "dssh_test_internal.h"
#include "auth_discovery_fixture.h"
#include "mock_io.h"
#include "test_dhgex_provider.h"
#include "test_comp.h"
#include "test_stateless.h"

#include "auth_result_fixture.h"

#include "signature_copy_fixture.h"
#include "kex_result_fixture.h"

static bool transport_pair_case(unsigned kind);

static int (*extra_algorithm_registration)(void);
static const char *handshake_kex_filter;
static void (*dh_group_tx_observer)(dssh_session sess);
static _Thread_local dssh_session compression_callback_session;
static void (*compression_tx_observer)(dssh_session sess);
static dssh_hostkey_verify_cb handshake_verify_callback;
static void *handshake_verify_data;
static bool handshake_exclude_server;
static bool ext_info_fail_send;
static bool ext_info_send_failed;

struct receive_entry_io_probe {
	dssh_session sess;
	unsigned tx_calls;
	unsigned rx_calls;
};
static _Thread_local struct receive_entry_io_probe *receive_entry_io_probe;
static _Atomic(dssh_session) receive_entry_watched_rx;
static atomic_bool receive_entry_rx_entered;


static void *test_algo_next(void *node);

static void *
negotiate_text(const char *client, const char *server, void *head, size_t offset, const char *filter)
{
	struct dssh_name_list client_names = { (const uint8_t *)client, strlen(client) };
	struct dssh_name_list server_names = { (const uint8_t *)server, strlen(server) };
	return negotiate_algo(client_names, server_names, head, offset, test_algo_next, filter);
}

static size_t
first_name_text(const char *text, char *buf, size_t bufsz)
{
	struct dssh_name_list list = { (const uint8_t *)text, strlen(text) };
	return first_name(list, buf, bufsz);
}

/* ================================================================
 * Dispatch callbacks for two-sided tests.
 *
 * The I/O callbacks are global -- all sessions share the same
 * function pointers.  These dispatch functions inspect the session's
 * client flag to route I/O through the correct pipe direction.
 * ================================================================ */

struct disconnect_queue_state {
	dssh_session sess;
	atomic_bool entered;
	atomic_bool release;
	unsigned writes;
	int queued;
};
static struct disconnect_queue_state *disconnect_queue;

struct control_tx_failure {
	dssh_session sess;
	unsigned calls;
	unsigned fail_at;
};
static struct control_tx_failure *control_tx_failure;


static int
mock_tx_dispatch(uint8_t *buf, size_t bufsz,
    dssh_session sess, void *cbdata)
{
	if (ext_info_fail_send && !sess->trans.client && sess->trans.session_id != NULL
	    && sess->trans.ext_info_state == DSSH_EXT_INFO_INITIAL
	    && atomic_load(&sess->trans.tx_since_rekey) == 0) {
		ext_info_send_failed = true;
		return DSSH_ERROR_INIT;
	}
	if (receive_entry_io_probe != NULL && receive_entry_io_probe->sess == sess) {
		receive_entry_io_probe->tx_calls++;
		return DSSH_ERROR_INIT;
	}
	if (dh_group_tx_observer != NULL)
		dh_group_tx_observer(sess);
	if (compression_tx_observer != NULL)
		compression_tx_observer(sess);
	if (control_tx_failure != NULL && control_tx_failure->sess == sess) {
		unsigned call = control_tx_failure->calls++;
		if (call == control_tx_failure->fail_at)
			return DSSH_ERROR_INIT;
	}
	int ret = sess->trans.client ? mock_tx_client(buf, bufsz, sess, cbdata)
	    : mock_tx_server(buf, bufsz, sess, cbdata);
	if (disconnect_queue != NULL && disconnect_queue->sess == sess) {
		disconnect_queue->writes++;
		if (!atomic_exchange(&disconnect_queue->entered, true)
		    && !dssh_test_wait_bool(&disconnect_queue->release, true))
			return DSSH_ERROR_TERMINATED;
	}
	return ret;
}

static int
mock_rx_dispatch(uint8_t *buf, size_t bufsz,
    dssh_session sess, void *cbdata)
{
	if (receive_entry_io_probe != NULL && receive_entry_io_probe->sess == sess) {
		receive_entry_io_probe->rx_calls++;
		return DSSH_ERROR_INIT;
	}
	if (sess == atomic_load(&receive_entry_watched_rx))
		atomic_store(&receive_entry_rx_entered, true);
	if (sess->trans.client)
		return mock_rx_client(buf, bufsz, sess, cbdata);
	else
		return mock_rx_server(buf, bufsz, sess, cbdata);
}

static int
mock_rxline_dispatch(uint8_t *buf, size_t bufsz,
    size_t *bytes_received, dssh_session sess, void *cbdata)
{
	if (receive_entry_io_probe != NULL && receive_entry_io_probe->sess == sess) {
		receive_entry_io_probe->rx_calls++;
		return DSSH_ERROR_INIT;
	}
	if (sess->trans.client)
		return mock_rxline_client(buf, bufsz, bytes_received, sess, cbdata);
	else
		return mock_rxline_server(buf, bufsz, bytes_received, sess, cbdata);
}

/* ================================================================
 * Helper: register all algorithms needed for handshake
 * ================================================================ */

static dssh_session
init_server_session(void)
{
	dssh_session s = dssh_session_init(false, 0);
	if (s != NULL)
		test_dhgex_setup(s);
	return s;
}

static int
register_all_algorithms(void)
{
	int res;
	res = test_register_kex();
	if (res < 0)
		return res;
	res = test_register_key_algos();
	if (res < 0)
		return res;
	res = dssh_register_aes256_ctr();
	if (res < 0)
		return res;
	res = test_register_mac_algos();
	if (res < 0)
		return res;
	if (extra_algorithm_registration != NULL) {
		res = extra_algorithm_registration();
		if (res < 0)
			return res;
	}
	res = dssh_register_none_comp();
	if (res < 0)
		return res;
	return 0;
}

/* ================================================================
 * Helper: full handshake setup for two-sided tests
 *
 * Registers algorithms, generates an ed25519 host key, creates
 * client and server sessions with mock I/O, and runs handshake
 * in two threads.
 * ================================================================ */

struct handshake_ctx {
	struct mock_io_state io;
	dssh_session client;
	dssh_session server;
	int client_result;
	int server_result;
};

static int
handshake_client_thread(void *arg)
{
	struct handshake_ctx *ctx = arg;
	compression_callback_session = ctx->client;
	ctx->client_result = dssh_transport_handshake(ctx->client);
	compression_callback_session = NULL;
	if (ctx->client_result != 0) {
		mock_io_close_c2s(&ctx->io);
		mock_io_close_s2c(&ctx->io);
	}
	return 0;
}

static int
handshake_server_thread(void *arg)
{
	struct handshake_ctx *ctx = arg;
	if (handshake_exclude_server)
		dssh_test_ossl_exclude_thread();
	compression_callback_session = ctx->server;
	ctx->server_result = dssh_transport_handshake(ctx->server);
	compression_callback_session = NULL;
	if (ctx->server_result != 0) {
		mock_io_close_c2s(&ctx->io);
		mock_io_close_s2c(&ctx->io);
	}
	return 0;
}

/*
 * Set up and run a full handshake.  Returns 0 on success.
 * Caller must call handshake_cleanup() when done.
 */
static void handshake_cleanup(struct handshake_ctx *ctx);
static void handshake_cleanup_sessions(struct handshake_ctx *ctx);

static int
handshake_setup_configured(struct handshake_ctx *ctx, bool alternate_cipher, const char *compression,
    const char *enc, const char *mac, bool configure)
{
	memset(ctx, 0, sizeof(*ctx));
	/* Cleanup is safe even if registration fails before mock_io_init. */
	atomic_init(&ctx->io.c2s.rfd, -1);
	atomic_init(&ctx->io.c2s.wfd, -1);
	atomic_init(&ctx->io.s2c.rfd, -1);
	atomic_init(&ctx->io.s2c.wfd, -1);
	if (configure) {
		dssh_test_reset_global_config();
		if (register_all_algorithms() < 0)
			goto setup_fail;
		if (alternate_cipher && dssh_register_aes128_cbc() < 0)
			goto setup_fail;
		if (test_generate_host_key() < 0)
			goto setup_fail;
	}
	if (mock_io_init(&ctx->io, 0) < 0)
		goto setup_fail;
	if (configure && dssh_transport_set_callbacks(mock_tx_dispatch, mock_rx_dispatch,
	    mock_rxline_dispatch, mock_extra_line_cb) < 0)
		goto setup_fail;
	ctx->client = dssh_session_init(true, 0);
	if (ctx->client == NULL)
		goto setup_fail;
	if (dssh_session_set_cbdata(ctx->client, &ctx->io, &ctx->io, &ctx->io, &ctx->io) < 0)
		goto setup_fail;
	dssh_hostkey_verify_cb verify = handshake_verify_callback != NULL
	    ? handshake_verify_callback : dssh_test_accept_hostkey;
	if (dssh_session_set_hostkey_verify_cb(ctx->client, verify, handshake_verify_data) < 0)
		goto setup_fail;
	ctx->server = init_server_session();
	if (ctx->server == NULL)
		goto setup_fail;
	if (dssh_session_set_cbdata(ctx->server, &ctx->io, &ctx->io, &ctx->io, &ctx->io) < 0)
		goto setup_fail;
	if (compression != NULL
	    && (dssh_session_set_comp_filter(ctx->client, &compression, 1) < 0
	        || dssh_session_set_comp_filter(ctx->server, &compression, 1) < 0))
		goto setup_fail;
	if (enc != NULL
	    && (dssh_session_set_enc_filter(ctx->client, &enc, 1) < 0
	        || dssh_session_set_enc_filter(ctx->server, &enc, 1) < 0))
		goto setup_fail;
	if (mac != NULL
	    && (dssh_session_set_mac_filter(ctx->client, &mac, 1) < 0
	        || dssh_session_set_mac_filter(ctx->server, &mac, 1) < 0))
		goto setup_fail;
	if (handshake_kex_filter != NULL
	    && (dssh_session_set_kex_filter(ctx->client, &handshake_kex_filter, 1) < 0
	        || dssh_session_set_kex_filter(ctx->server, &handshake_kex_filter, 1) < 0))
		goto setup_fail;
	thrd_t ct, st;
	if (thrd_create(&ct, handshake_client_thread, ctx) != thrd_success)
		goto setup_fail;
	if (thrd_create(&st, handshake_server_thread, ctx) != thrd_success) {
		dssh_session_terminate(ctx->client);
		mock_io_close_c2s(&ctx->io);
		mock_io_close_s2c(&ctx->io);
		if (thrd_join(ct, NULL) != thrd_success)
			abort();
		goto setup_fail;
	}
	int cj = thrd_join(ct, NULL);
	int sj = thrd_join(st, NULL);
	if (cj != thrd_success || sj != thrd_success)
		abort();
	/* Protocol failures retain the sessions for caller inspection/cleanup. */
	return ctx->client_result == 0 && ctx->server_result == 0 ? 0 : -1;
setup_fail:
	if (configure)
		handshake_cleanup(ctx);
	else
		handshake_cleanup_sessions(ctx);
	return -1;
}

static int
handshake_setup_filtered(struct handshake_ctx *ctx, bool alternate_cipher, const char *compression,
    const char *enc, const char *mac)
{
	return handshake_setup_configured(ctx, alternate_cipher, compression, enc, mac, true);
}

static int
handshake_setup_options(struct handshake_ctx *ctx, bool alternate_cipher, const char *compression)
{
	return handshake_setup_filtered(ctx, alternate_cipher, compression, NULL, NULL);
}

static int
handshake_setup_with_cipher(struct handshake_ctx *ctx, bool alternate_cipher)
{
	return handshake_setup_options(ctx, alternate_cipher, NULL);
}

static int
handshake_setup(struct handshake_ctx *ctx)
{
	return handshake_setup_with_cipher(ctx, false);
}

static void
handshake_cleanup_sessions(struct handshake_ctx *ctx)
{
	compression_callback_session = ctx->server;
	if (ctx->server != NULL)
		dssh_session_cleanup(ctx->server);
	ctx->server = NULL;
	compression_callback_session = ctx->client;
	if (ctx->client != NULL)
		dssh_session_cleanup(ctx->client);
	ctx->client = NULL;
	compression_callback_session = NULL;
	mock_io_free(&ctx->io);
}

static void
handshake_cleanup(struct handshake_ctx *ctx)
{
	handshake_cleanup_sessions(ctx);
	dssh_test_reset_global_config();
}

/* ================================================================
 * Version validators (~12 tests)
 * ================================================================ */

static int
test_has_nulls_true(void)
{
	uint8_t buf[] = "SSH-2.0\x00-test\r\n";
	ASSERT_TRUE(has_nulls(buf, sizeof(buf) - 1));
	return TEST_PASS;
}

static int
test_has_nulls_false(void)
{
	uint8_t buf[] = "SSH-2.0-test\r\n";
	ASSERT_FALSE(has_nulls(buf, sizeof(buf) - 1));
	return TEST_PASS;
}

static int
test_missing_crlf_short(void)
{
	uint8_t buf[] = "X";
	ASSERT_TRUE(missing_crlf(buf, 1));
	return TEST_PASS;
}

static int
test_missing_crlf_no_cr(void)
{
	uint8_t buf[] = "SSH-2.0-test\n";
	ASSERT_TRUE(missing_crlf(buf, sizeof(buf) - 1));
	return TEST_PASS;
}

static int
test_missing_crlf_no_lf(void)
{
	uint8_t buf[] = "SSH-2.0-test\r";
	ASSERT_TRUE(missing_crlf(buf, sizeof(buf) - 1));
	return TEST_PASS;
}

static int
test_missing_crlf_valid(void)
{
	uint8_t buf[] = "SSH-2.0-test\r\n";
	ASSERT_FALSE(missing_crlf(buf, sizeof(buf) - 1));
	return TEST_PASS;
}

static int
test_is_version_line_true(void)
{
	uint8_t buf[] = "SSH-2.0-test\r\n";
	ASSERT_TRUE(is_version_line(buf, sizeof(buf) - 1));
	return TEST_PASS;
}

static int
test_is_version_line_false_get(void)
{
	uint8_t buf[] = "GET / HTTP/1.1\r\n";
	ASSERT_FALSE(is_version_line(buf, sizeof(buf) - 1));
	return TEST_PASS;
}

static int
test_is_version_line_false_short(void)
{
	uint8_t buf[] = "SS";
	ASSERT_FALSE(is_version_line(buf, 2));
	return TEST_PASS;
}

static int
test_has_non_ascii_ctrl(void)
{
	uint8_t buf[] = "SSH-2.0-\x01test";
	ASSERT_TRUE(has_non_ascii(buf, sizeof(buf) - 1));
	return TEST_PASS;
}

static int
test_has_non_ascii_high(void)
{
	uint8_t buf[] = "SSH-2.0-\x80test";
	ASSERT_TRUE(has_non_ascii(buf, sizeof(buf) - 1));
	return TEST_PASS;
}

static int
test_has_non_ascii_false(void)
{
	uint8_t buf[] = "SSH-2.0-OpenSSH_9.0";
	ASSERT_FALSE(has_non_ascii(buf, sizeof(buf) - 1));
	return TEST_PASS;
}

static int
test_is_20_true(void)
{
	uint8_t buf[] = "SSH-2.0-test\r\n";
	ASSERT_TRUE(is_20(buf, sizeof(buf) - 1));
	return TEST_PASS;
}

static int
test_is_20_true_199(void)
{
	uint8_t buf[] = "SSH-1.99-compat\r\n";
	ASSERT_TRUE(is_20(buf, sizeof(buf) - 1));
	return TEST_PASS;
}

static int
test_is_20_false_10(void)
{
	uint8_t buf[] = "SSH-1.0-old\r\n";
	ASSERT_FALSE(is_20(buf, sizeof(buf) - 1));
	return TEST_PASS;
}

static int
test_is_20_false_short(void)
{
	uint8_t buf[] = "SSH-2.";
	ASSERT_FALSE(is_20(buf, 6));
	return TEST_PASS;
}

/* ================================================================
 * Algorithm negotiation (~8 tests)
 * ================================================================ */

/*
 * For negotiation tests we need a linked list head.  The registry
 * is internal to gconf, but negotiate_algo takes an arbitrary linked
 * list.  Build small structs with the correct layout (next pointer
 * at offset 0, name at known offset).
 */

struct test_algo_node {
	struct test_algo_node *next;
	char name[64];
};

static void *
test_algo_next(void *node)
{
	struct test_algo_node *entry = node;
	return entry->next;
}

static int
test_negotiate_first_client_match(void)
{
	struct test_algo_node c = { NULL, "aes256-ctr" };
	struct test_algo_node b = { &c, "aes128-ctr" };
	struct test_algo_node a = { &b, "chacha20" };

	size_t noff = offsetof(struct test_algo_node, name);
	void *result = negotiate_text(
	    "aes128-ctr,aes256-ctr", "aes256-ctr,aes128-ctr",
	    &a, noff, NULL);
	ASSERT_NOT_NULL(result);
	/* Client prefers aes128-ctr and server has it: should match aes128-ctr */
	ASSERT_STR_EQ(((struct test_algo_node *)result)->name, "aes128-ctr");
	return TEST_PASS;
}

static int
test_negotiate_no_match(void)
{
	struct test_algo_node b = { NULL, "chacha20" };
	struct test_algo_node a = { &b, "aes256-ctr" };

	size_t noff = offsetof(struct test_algo_node, name);
	void *result = negotiate_text(
	    "aes256-ctr,chacha20", "3des-cbc,blowfish-cbc",
	    &a, noff, NULL);
	ASSERT_NULL(result);
	return TEST_PASS;
}

static int
test_negotiate_client_priority(void)
{
	struct test_algo_node b = { NULL, "algo-A" };
	struct test_algo_node a = { &b, "algo-B" };

	size_t noff = offsetof(struct test_algo_node, name);
	/* Client lists B first, server lists A first */
	void *result = negotiate_text(
	    "algo-B,algo-A", "algo-A,algo-B",
	    &a, noff, NULL);
	ASSERT_NOT_NULL(result);
	/* Client preference wins: algo-B */
	ASSERT_STR_EQ(((struct test_algo_node *)result)->name, "algo-B");
	return TEST_PASS;
}

static int
test_negotiate_single_match(void)
{
	struct test_algo_node a = { NULL, "the-only-one" };

	size_t noff = offsetof(struct test_algo_node, name);
	void *result = negotiate_text(
	    "the-only-one", "the-only-one",
	    &a, noff, NULL);
	ASSERT_NOT_NULL(result);
	ASSERT_STR_EQ(((struct test_algo_node *)result)->name, "the-only-one");
	return TEST_PASS;
}

static int
test_negotiate_not_registered(void)
{
	struct test_algo_node a = { NULL, "registered-algo" };

	size_t noff = offsetof(struct test_algo_node, name);
	/* Both lists agree on "unregistered" but it is not in the linked list */
	void *result = negotiate_text(
	    "unregistered", "unregistered",
	    &a, noff, NULL);
	ASSERT_NULL(result);
	return TEST_PASS;
}

static int
test_build_namelist_single(void)
{
	struct test_algo_node a = { NULL, "aes256-ctr" };
	char buf[256];

	size_t noff = offsetof(struct test_algo_node, name);
	int64_t len = build_namelist(&a, noff, test_algo_next, buf, sizeof(buf), NULL);
	ASSERT_STR_EQ(buf, "aes256-ctr");
	ASSERT_EQ_U(len, strlen("aes256-ctr"));
	return TEST_PASS;
}

static int
test_build_namelist_multiple(void)
{
	struct test_algo_node c = { NULL, "none" };
	struct test_algo_node b = { &c, "hmac-sha2-256" };
	struct test_algo_node a = { &b, "aes256-ctr" };
	char buf[256];

	size_t noff = offsetof(struct test_algo_node, name);
	int64_t len = build_namelist(&a, noff, test_algo_next, buf, sizeof(buf), NULL);
	ASSERT_STR_EQ(buf, "aes256-ctr,hmac-sha2-256,none");
	ASSERT_EQ_U(len, strlen("aes256-ctr,hmac-sha2-256,none"));
	return TEST_PASS;
}

static int
test_build_namelist_empty(void)
{
	char buf[256];
	buf[0] = 'X';
	int64_t len = build_namelist(NULL, 0, test_algo_next, buf, sizeof(buf), NULL);
	ASSERT_EQ_U(len, 0);
	ASSERT_EQ(buf[0], '\0');
	return TEST_PASS;
}

/* ================================================================
 * Per-session whitelist filter (build_namelist + setters)
 * ================================================================ */

/* Filter drops a registered algorithm from the offered namelist. */
static int
test_filter_drops_algo(void)
{
	struct test_algo_node b = { NULL, "aes128-cbc" };
	struct test_algo_node a = { &b, "aes256-ctr" };
	char   buf[64];

	size_t noff = offsetof(struct test_algo_node, name);
	int64_t len  = build_namelist(&a, noff, test_algo_next, buf, sizeof(buf), "aes256-ctr");
	ASSERT_STR_EQ(buf, "aes256-ctr");
	ASSERT_EQ_U(len, strlen("aes256-ctr"));
	return TEST_PASS;
}

/* Filter order overrides registration order. */
static int
test_filter_reorders(void)
{
	struct test_algo_node b = { NULL, "B" };
	struct test_algo_node a = { &b, "A" };
	char   buf[64];

	size_t noff = offsetof(struct test_algo_node, name);
	int64_t len  = build_namelist(&a, noff, test_algo_next, buf, sizeof(buf), "B,A");
	ASSERT_STR_EQ(buf, "B,A");
	ASSERT_EQ_U(len, 3);
	return TEST_PASS;
}

/* Filter names not in the registry are silently skipped. */
static int
test_filter_skips_unknown(void)
{
	struct test_algo_node a = { NULL, "aes256-ctr" };
	char   buf[64];

	size_t noff = offsetof(struct test_algo_node, name);
	int64_t len  = build_namelist(&a, noff, test_algo_next, buf, sizeof(buf),
	    "aes256-ctr,nonexistent");
	ASSERT_STR_EQ(buf, "aes256-ctr");
	ASSERT_EQ_U(len, strlen("aes256-ctr"));
	return TEST_PASS;
}

/* NULL filter restores full registration-order behaviour. */
static int
test_filter_null_uses_all(void)
{
	struct test_algo_node b = { NULL, "B" };
	struct test_algo_node a = { &b, "A" };
	char   buf[64];

	size_t noff = offsetof(struct test_algo_node, name);
	int64_t len  = build_namelist(&a, noff, test_algo_next, buf, sizeof(buf), NULL);
	ASSERT_STR_EQ(buf, "A,B");
	ASSERT_EQ_U(len, 3);
	return TEST_PASS;
}

/* name_in_filter primitive: hits, length-exact misses, NULL filter. */
static int
test_name_in_filter_basic(void)
{
	ASSERT_TRUE(name_in_filter("foo", 3, "foo"));
	ASSERT_TRUE(name_in_filter("bar", 3, "foo,bar,baz"));
	ASSERT_TRUE(name_in_filter("baz", 3, "foo,bar,baz"));
	/* prefix should not match (length-exact) */
	ASSERT_FALSE(name_in_filter("foo", 3, "foobar"));
	ASSERT_FALSE(name_in_filter("foobar", 6, "foo"));
	ASSERT_FALSE(name_in_filter("xyz", 3, "foo,bar,baz"));
	ASSERT_FALSE(name_in_filter("foo", 3, NULL));
	return TEST_PASS;
}

/* negotiate_algo respects the filter even if peer offers a filtered name. */
static int
test_negotiate_filter_blocks(void)
{
	struct test_algo_node b = { NULL, "B" };
	struct test_algo_node a = { &b, "A" };

	size_t noff = offsetof(struct test_algo_node, name);

	/* Peer offers both; filter only allows A.  A must win. */
	void *result = negotiate_text("A,B", "A,B", &a, noff, "A");
	ASSERT_NOT_NULL(result);
	ASSERT_STR_EQ(((struct test_algo_node *)result)->name, "A");

	/* Peer offers only B; filter only allows A.  No match. */
	result = negotiate_text("B", "B", &a, noff, "A");
	ASSERT_NULL(result);
	return TEST_PASS;
}

/* Setter rejects names containing commas (would break CSV parsing). */
static int
test_filter_setter_comma_rejected(void)
{
	dssh_session sess = dssh_session_init(true, 0);
	ASSERT_NOT_NULL(sess);

	const char *bad[] = { "ok", "bad,name" };

	ASSERT_EQ(dssh_session_set_enc_filter(sess, bad, 2),
	    DSSH_ERROR_INVALID);
	/* No partial state stored. */
	ASSERT_NULL(sess->enc_filter);

	dssh_session_cleanup(sess);
	return TEST_PASS;
}

/* Setter rejects NULL elements and empty strings. */
static int
test_filter_setter_null_and_empty_rejected(void)
{
	dssh_session sess = dssh_session_init(true, 0);
	ASSERT_NOT_NULL(sess);

	const char *with_null[]  = { "ok", NULL };
	const char *with_empty[] = { "ok", "" };

	ASSERT_EQ(dssh_session_set_enc_filter(sess, with_null, 2),
	    DSSH_ERROR_INVALID);
	ASSERT_EQ(dssh_session_set_enc_filter(sess, with_empty, 2),
	    DSSH_ERROR_INVALID);
	ASSERT_NULL(sess->enc_filter);

	dssh_session_cleanup(sess);
	return TEST_PASS;
}

/* Setter stores filter, second call frees old and replaces. */
static int
test_filter_setter_replace(void)
{
	dssh_session sess = dssh_session_init(true, 0);
	ASSERT_NOT_NULL(sess);

	const char *first[]  = { "aes256-ctr", "aes128-cbc" };
	const char *second[] = { "aes128-cbc" };

	ASSERT_OK(dssh_session_set_enc_filter(sess, first, 2));
	ASSERT_NOT_NULL(sess->enc_filter);
	ASSERT_STR_EQ(sess->enc_filter, "aes256-ctr,aes128-cbc");

	ASSERT_OK(dssh_session_set_enc_filter(sess, second, 1));
	ASSERT_STR_EQ(sess->enc_filter, "aes128-cbc");

	/* count == 0 clears */
	ASSERT_OK(dssh_session_set_enc_filter(sess, second, 0));
	ASSERT_NULL(sess->enc_filter);

	/* names == NULL also clears (after a non-NULL value) */
	ASSERT_OK(dssh_session_set_enc_filter(sess, first, 2));
	ASSERT_NOT_NULL(sess->enc_filter);
	ASSERT_OK(dssh_session_set_enc_filter(sess, NULL, 5));
	ASSERT_NULL(sess->enc_filter);

	dssh_session_cleanup(sess);
	return TEST_PASS;
}

/* Setter returns TOOLATE once demux_running is set (post-start). */
static int
test_filter_setter_toolate(void)
{
	dssh_session sess = dssh_session_init(true, 0);
	ASSERT_NOT_NULL(sess);

	/* Simulate a started session.  We don't run a real handshake;
	 * the TOOLATE check looks at sess->demux_running directly. */
	atomic_store(&sess->demux_running, true);

	const char *names[] = { "aes256-ctr" };

	ASSERT_EQ(dssh_session_set_kex_filter(sess, names, 1),
	    DSSH_ERROR_TOOLATE);
	ASSERT_EQ(dssh_session_set_key_algo_filter(sess, names, 1),
	    DSSH_ERROR_TOOLATE);
	ASSERT_EQ(dssh_session_set_enc_filter(sess, names, 1),
	    DSSH_ERROR_TOOLATE);
	ASSERT_EQ(dssh_session_set_mac_filter(sess, names, 1),
	    DSSH_ERROR_TOOLATE);
	ASSERT_EQ(dssh_session_set_comp_filter(sess, names, 1),
	    DSSH_ERROR_TOOLATE);

	/* Reset so cleanup doesn't try to join a non-existent thread. */
	atomic_store(&sess->demux_running, false);
	dssh_session_cleanup(sess);
	return TEST_PASS;
}

/* Setter returns INVALID for NULL session. */
static int
test_filter_setter_null_session(void)
{
	const char *names[] = { "aes256-ctr" };

	ASSERT_EQ(dssh_session_set_kex_filter(NULL, names, 1),
	    DSSH_ERROR_INVALID);
	ASSERT_EQ(dssh_session_set_key_algo_filter(NULL, names, 1),
	    DSSH_ERROR_INVALID);
	ASSERT_EQ(dssh_session_set_enc_filter(NULL, names, 1),
	    DSSH_ERROR_INVALID);
	ASSERT_EQ(dssh_session_set_mac_filter(NULL, names, 1),
	    DSSH_ERROR_INVALID);
	ASSERT_EQ(dssh_session_set_comp_filter(NULL, names, 1),
	    DSSH_ERROR_INVALID);
	return TEST_PASS;
}

/* ================================================================
 * Key derivation (~4 tests)
 * ================================================================ */

static int
test_derive_key_deterministic(void)
{
	uint8_t secret[] = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
	    0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f };
	uint8_t hash[32];
	memset(hash, 0xAA, sizeof(hash));
	uint8_t session_id[32];
	memset(session_id, 0xBB, sizeof(session_id));

	uint8_t out1[32], out2[32];
	ASSERT_OK(derive_key("SHA-256", secret, sizeof(secret),
	    hash, sizeof(hash), 'A', session_id, sizeof(session_id),
	    out1, sizeof(out1)));
	ASSERT_OK(derive_key("SHA-256", secret, sizeof(secret),
	    hash, sizeof(hash), 'A', session_id, sizeof(session_id),
	    out2, sizeof(out2)));
	ASSERT_MEM_EQ(out1, out2, sizeof(out1));
	return TEST_PASS;
}

static int
test_derive_key_different_letters(void)
{
	uint8_t secret[16];
	memset(secret, 0x11, sizeof(secret));
	uint8_t hash[32];
	memset(hash, 0x22, sizeof(hash));
	uint8_t session_id[32];
	memset(session_id, 0x33, sizeof(session_id));

	uint8_t out_a[32], out_b[32];
	ASSERT_OK(derive_key("SHA-256", secret, sizeof(secret),
	    hash, sizeof(hash), 'A', session_id, sizeof(session_id),
	    out_a, sizeof(out_a)));
	ASSERT_OK(derive_key("SHA-256", secret, sizeof(secret),
	    hash, sizeof(hash), 'B', session_id, sizeof(session_id),
	    out_b, sizeof(out_b)));
	/* Different letters must produce different output */
	ASSERT_TRUE(memcmp(out_a, out_b, sizeof(out_a)) != 0);
	return TEST_PASS;
}

static int
test_derive_key_extension_loop(void)
{
	/* Request more than 32 bytes (SHA256 digest size) to exercise
	 * the extension loop in derive_key. */
	uint8_t secret[16];
	memset(secret, 0x44, sizeof(secret));
	uint8_t hash[32];
	memset(hash, 0x55, sizeof(hash));
	uint8_t session_id[32];
	memset(session_id, 0x66, sizeof(session_id));

	uint8_t out[64];
	memset(out, 0, sizeof(out));
	ASSERT_OK(derive_key("SHA-256", secret, sizeof(secret),
	    hash, sizeof(hash), 'C', session_id, sizeof(session_id),
	    out, sizeof(out)));

	/* Verify the output is not all zeros (was actually written) */
	bool all_zero = true;
	for (size_t i = 0; i < sizeof(out); i++) {
		if (out[i] != 0) {
			all_zero = false;
			break;
		}
	}
	ASSERT_FALSE(all_zero);

	/* First 32 bytes should match a 32-byte derivation */
	uint8_t out32[32];
	ASSERT_OK(derive_key("SHA-256", secret, sizeof(secret),
	    hash, sizeof(hash), 'C', session_id, sizeof(session_id),
	    out32, sizeof(out32)));
	ASSERT_MEM_EQ(out, out32, 32);
	return TEST_PASS;
}

static int
test_derive_key_bad_hash(void)
{
	uint8_t secret[16] = {0};
	uint8_t hash[32] = {0};
	uint8_t session_id[32] = {0};
	uint8_t out[32];

	int res = derive_key("NO_SUCH_HASH", secret, sizeof(secret),
	    hash, sizeof(hash), 'A', session_id, sizeof(session_id),
	    out, sizeof(out));
	ASSERT_TRUE(res < 0);
	return TEST_PASS;
}

/* ================================================================
 * Version exchange (~6 tests, mock I/O)
 * ================================================================ */

static int
test_version_exchange_basic(void)
{
	ASSERT_TRUE(transport_pair_case(0));
	return TEST_PASS;
}

static int
test_version_exchange_remote_stored(void)
{
	ASSERT_TRUE(transport_pair_case(1));
	return TEST_PASS;
}

static int
test_version_exchange_reject_non_20(void)
{
	dssh_test_reset_global_config();
	dssh_transport_set_callbacks(mock_tx_dispatch, mock_rx_dispatch,
	    mock_rxline_dispatch, mock_extra_line_cb);

	struct mock_io_state io;
	ASSERT_OK(mock_io_init(&io, 0));
	ASSERT_OK(dssh_register_none_comp());

	dssh_session client = dssh_session_init(true, 0);
	ASSERT_NOT_NULL(client);
	dssh_session_set_cbdata(client, &io, &io, &io, &io);

	/* Inject a SSH-1.0 version string into the s2c pipe (what client reads) */
	const char *bad_ver = "SSH-1.0-OldServer\r\n";
	mock_io_inject(&io.s2c, (const uint8_t *)bad_ver, strlen(bad_ver));

	int res = version_exchange(client);
	ASSERT_TRUE(res < 0);

	dssh_session_cleanup(client);
	mock_io_free(&io);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

static int
test_version_exchange_accept_199(void)
{
	dssh_test_reset_global_config();
	dssh_transport_set_callbacks(mock_tx_dispatch, mock_rx_dispatch,
	    mock_rxline_dispatch, mock_extra_line_cb);

	struct mock_io_state io;
	ASSERT_OK(mock_io_init(&io, 0));
	ASSERT_OK(dssh_register_none_comp());

	dssh_session client = dssh_session_init(true, 0);
	ASSERT_NOT_NULL(client);
	dssh_session_set_cbdata(client, &io, &io, &io, &io);

	/* Inject SSH-1.99 (compatible with 2.0) */
	const char *ver199 = "SSH-1.99-CompatServer\r\n";
	mock_io_inject(&io.s2c, (const uint8_t *)ver199, strlen(ver199));

	int res = version_exchange(client);
	ASSERT_OK(res);

	const char *remote = dssh_transport_get_remote_version(client);
	ASSERT_NOT_NULL(remote);
	ASSERT_TRUE(strncmp(remote, "SSH-1.99-", 9) == 0);

	dssh_session_cleanup(client);
	mock_io_free(&io);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

static int
test_version_exchange_extra_lines(void)
{
	dssh_test_reset_global_config();
	dssh_transport_set_callbacks(mock_tx_dispatch, mock_rx_dispatch,
	    mock_rxline_dispatch, mock_extra_line_cb);

	struct mock_io_state io;
	ASSERT_OK(mock_io_init(&io, 0));
	ASSERT_OK(dssh_register_none_comp());

	dssh_session client = dssh_session_init(true, 0);
	ASSERT_NOT_NULL(client);
	dssh_session_set_cbdata(client, &io, &io, &io, &io);

	/* RFC 4253 s4.2: server MAY send lines before version string */
	const char *banner = "Welcome to my server\r\n";
	const char *ver = "SSH-2.0-TestServer\r\n";
	mock_io_inject(&io.s2c, (const uint8_t *)banner, strlen(banner));
	mock_io_inject(&io.s2c, (const uint8_t *)ver, strlen(ver));

	int res = version_exchange(client);
	ASSERT_OK(res);

	const char *remote = dssh_transport_get_remote_version(client);
	ASSERT_STR_EQ(remote, "SSH-2.0-TestServer");

	dssh_session_cleanup(client);
	mock_io_free(&io);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

static int
test_version_exchange_reject_non_ascii(void)
{
	dssh_test_reset_global_config();
	dssh_transport_set_callbacks(mock_tx_dispatch, mock_rx_dispatch,
	    mock_rxline_dispatch, mock_extra_line_cb);

	struct mock_io_state io;
	ASSERT_OK(mock_io_init(&io, 0));
	ASSERT_OK(dssh_register_none_comp());

	dssh_session client = dssh_session_init(true, 0);
	ASSERT_NOT_NULL(client);
	dssh_session_set_cbdata(client, &io, &io, &io, &io);

	/* Version line with high byte -- should be rejected */
	const char *bad = "SSH-2.0-Bad\x80Server\r\n";
	mock_io_inject(&io.s2c, (const uint8_t *)bad, strlen(bad));

	int res = version_exchange(client);
	ASSERT_TRUE(res < 0);

	dssh_session_cleanup(client);
	mock_io_free(&io);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

/* ================================================================
 * Packet send/recv -- unencrypted (~8 tests)
 * ================================================================ */

/* ================================================================
 * Transport message handling (~5 tests)
 * ================================================================ */

static int
test_ignore_silently_skipped(void)
{
	dssh_test_reset_global_config();
	dssh_transport_set_callbacks(mock_tx_dispatch, mock_rx_dispatch,
	    mock_rxline_dispatch, mock_extra_line_cb);
	ASSERT_OK(dssh_register_none_comp());

	struct mock_io_state io;
	ASSERT_OK(mock_io_init(&io, 0));

	dssh_session client = dssh_session_init(true, 0);
	ASSERT_NOT_NULL(client);
	dssh_session_set_cbdata(client, &io, &io, &io, &io);

	dssh_session server = init_server_session();
	ASSERT_NOT_NULL(server);
	dssh_session_set_cbdata(server, &io, &io, &io, &io);

	/* Send IGNORE, then a real message */
	uint8_t ignore_msg[] = { SSH_MSG_IGNORE, 0x00, 0x00, 0x00, 0x02, 'h', 'i' };
	uint8_t real_msg[] = { SSH_MSG_SERVICE_REQUEST, 0xBE };
	ASSERT_OK(send_packet(client, ignore_msg, sizeof(ignore_msg), NULL));
	ASSERT_OK(send_packet(client, real_msg, sizeof(real_msg), NULL));

	/* Server's recv_packet should skip IGNORE and return real message */
	uint8_t msg_type;
	uint8_t *recv_payload;
	size_t recv_len;
	ASSERT_OK(recv_packet(server, &msg_type, &recv_payload, &recv_len));
	ASSERT_EQ(msg_type, SSH_MSG_SERVICE_REQUEST);
	ASSERT_EQ(recv_payload[1], 0xBE);

	dssh_session_cleanup(client);
	dssh_session_cleanup(server);
	mock_io_free(&io);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

static int test_log_peer_count;
static dssh_log_source test_log_peer_source;
static uint32_t test_log_peer_reason;
static bool test_log_peer_always;
static char test_log_peer_message[DSSH_LOG_MESSAGE_MAX + 1];

static dssh_log_action
test_transport_log_cb(const struct dssh_log_record *record, void *cbdata)
{
	(void)cbdata;
	if (record->source != DSSH_LOG_SOURCE_LIBRARY) {
		test_log_peer_count++;
		test_log_peer_source = record->source;
		test_log_peer_reason = record->ssh_reason_code;
		test_log_peer_always = record->always_display;
		size_t len = record->message_len;

		if (len > DSSH_LOG_MESSAGE_MAX)
			len = DSSH_LOG_MESSAGE_MAX;
		if (len > 0)
			memcpy(test_log_peer_message, record->message, len);
		test_log_peer_message[len] = 0;
	}
	/* Must be ignored for peer-originated records. */
	return DSSH_LOG_SEND_DEBUG_ALWAYS_DISPLAY;
}

static int
test_disconnect_sets_terminate(void)
{
	dssh_test_reset_global_config();
	dssh_transport_set_callbacks(mock_tx_dispatch, mock_rx_dispatch,
	    mock_rxline_dispatch, mock_extra_line_cb);
	ASSERT_OK(dssh_register_none_comp());

	struct mock_io_state io;
	ASSERT_OK(mock_io_init(&io, 0));

	dssh_session client = dssh_session_init(true, 0);
	ASSERT_NOT_NULL(client);
	dssh_session_set_cbdata(client, &io, &io, &io, &io);

	dssh_session server = init_server_session();
	ASSERT_NOT_NULL(server);
	dssh_session_set_cbdata(server, &io, &io, &io, &io);
	test_log_peer_count = 0;
	ASSERT_OK(dssh_session_set_log_cb(server, test_transport_log_cb, NULL));

	ASSERT_FALSE(dssh_session_is_terminated(server));

	/* Client sends disconnect */
	dssh_transport_disconnect(client, SSH_DISCONNECT_BY_APPLICATION, "goodbye");

	/* Server tries to recv -- should get TERMINATED */
	uint8_t msg_type;
	uint8_t *recv_payload;
	size_t recv_len;
	int res = recv_packet(server, &msg_type, &recv_payload, &recv_len);
	ASSERT_ERR(res, DSSH_ERROR_TERMINATED);
	ASSERT_TRUE(dssh_session_is_terminated(server));
	ASSERT_EQ(test_log_peer_count, 1);
	ASSERT_EQ(test_log_peer_source, DSSH_LOG_SOURCE_PEER_DISCONNECT);
	ASSERT_EQ(test_log_peer_reason, SSH_DISCONNECT_BY_APPLICATION);
	ASSERT_STR_EQ(test_log_peer_message, "goodbye");
	ASSERT_EQ(server->log_forward->count, (size_t)0);

	dssh_session_cleanup(client);
	dssh_session_cleanup(server);
	mock_io_free(&io);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

static bool test_debug_called;
static bool test_debug_always_display;
static char test_debug_message[256];

static void
test_debug_cb(bool always_display, const uint8_t *message,
    size_t message_len, void *cbdata)
{
	(void)cbdata;
	test_debug_called = true;
	test_debug_always_display = always_display;
	if (message && message_len < sizeof(test_debug_message)) {
		memcpy(test_debug_message, message, message_len);
		test_debug_message[message_len] = 0;
	}
}

static int
test_debug_invokes_callback(void)
{
	dssh_test_reset_global_config();
	dssh_transport_set_callbacks(mock_tx_dispatch, mock_rx_dispatch,
	    mock_rxline_dispatch, mock_extra_line_cb);
	ASSERT_OK(dssh_register_none_comp());

	struct mock_io_state io;
	ASSERT_OK(mock_io_init(&io, 0));

	dssh_session client = dssh_session_init(true, 0);
	ASSERT_NOT_NULL(client);
	dssh_session_set_cbdata(client, &io, &io, &io, &io);

	dssh_session server = init_server_session();
	ASSERT_NOT_NULL(server);
	dssh_session_set_cbdata(server, &io, &io, &io, &io);

	test_debug_called = false;
	test_debug_message[0] = 0;
	dssh_session_set_debug_cb(server, test_debug_cb, NULL);
	test_log_peer_count = 0;
	ASSERT_OK(dssh_session_set_log_cb(server, test_transport_log_cb, NULL));
	ASSERT_OK(dssh_session_set_log_level(server, DSSH_LOG_DEBUG));

	/* Build SSH_MSG_DEBUG: type(1) + always_display(1) + string(4+N) + lang(4+0) */
	const char *dbg_text = "test debug";
	size_t text_len = strlen(dbg_text);
	uint8_t debug_msg[64];
	size_t pos = 0;
	debug_msg[pos++] = SSH_MSG_DEBUG;
	debug_msg[pos++] = 1; /* always_display = true */
	dssh_serialize_uint32((uint32_t)text_len, debug_msg, sizeof(debug_msg), &pos);
	memcpy(&debug_msg[pos], dbg_text, text_len);
	pos += text_len;
	dssh_serialize_uint32(0, debug_msg, sizeof(debug_msg), &pos); /* language tag */

	ASSERT_OK(send_packet(client, debug_msg, pos, NULL));

	/* Send a real message after so recv_packet returns */
	uint8_t real_msg[] = { SSH_MSG_SERVICE_REQUEST, 0xFF };
	ASSERT_OK(send_packet(client, real_msg, sizeof(real_msg), NULL));

	uint8_t msg_type;
	uint8_t *recv_payload;
	size_t recv_len;
	ASSERT_OK(recv_packet(server, &msg_type, &recv_payload, &recv_len));
	ASSERT_EQ(msg_type, SSH_MSG_SERVICE_REQUEST);

	ASSERT_TRUE(test_debug_called);
	ASSERT_TRUE(test_debug_always_display);
	ASSERT_STR_EQ(test_debug_message, "test debug");
	ASSERT_EQ(test_log_peer_count, 1);
	ASSERT_EQ(test_log_peer_source, DSSH_LOG_SOURCE_PEER_DEBUG);
	ASSERT_TRUE(test_log_peer_always);
	ASSERT_STR_EQ(test_log_peer_message, "test debug");
	ASSERT_EQ(server->log_forward->count, (size_t)0);

	dssh_session_cleanup(client);
	dssh_session_cleanup(server);
	mock_io_free(&io);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

static int
test_log_forwards_debug(void)
{
	dssh_test_reset_global_config();
	dssh_transport_set_callbacks(mock_tx_dispatch, mock_rx_dispatch,
	    mock_rxline_dispatch, mock_extra_line_cb);
	ASSERT_OK(dssh_register_none_comp());
	struct mock_io_state io;
	ASSERT_OK(mock_io_init(&io, 0));
	dssh_session client = dssh_session_init(true, 0);
	dssh_session server = init_server_session();
	ASSERT_NOT_NULL(client);
	ASSERT_NOT_NULL(server);
	dssh_session_set_cbdata(client, &io, &io, &io, &io);
	dssh_session_set_cbdata(server, &io, &io, &io, &io);
	ASSERT_OK(dssh_session_set_log_cb(client, test_transport_log_cb, NULL));
	ASSERT_OK(dssh_session_set_log_level(client, DSSH_LOG_DEBUG));
	test_debug_called = false;
	test_debug_message[0] = 0;
	ASSERT_OK(dssh_session_set_debug_cb(server, test_debug_cb, NULL));

	/* Version exchange normally establishes this before packet sends. */
	client->trans.remote_id_str_sz = 1;
	DSSH_LOGF(client, DSSH_LOG_WARNING, DSSH_ERROR_TIMEOUT, "forwarded diagnostic");
	ASSERT_EQ(client->log_forward->count, (size_t)1);
	uint8_t real_msg[] = { SSH_MSG_SERVICE_REQUEST, 0xAA };
	ASSERT_OK(send_packet(client, real_msg, sizeof(real_msg), NULL));

	uint8_t msg_type;
	uint8_t *recv_payload;
	size_t recv_len;
	ASSERT_OK(recv_packet(server, &msg_type, &recv_payload, &recv_len));
	ASSERT_EQ(msg_type, SSH_MSG_SERVICE_REQUEST);
	ASSERT_TRUE(test_debug_called);
	ASSERT_TRUE(test_debug_always_display);
	ASSERT_STR_EQ(test_debug_message, "forwarded diagnostic");
	ASSERT_EQ(client->log_forward->count, (size_t)0);

	dssh_session_cleanup(client);
	dssh_session_cleanup(server);
	mock_io_free(&io);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

static bool test_unimpl_called;
static uint32_t test_unimpl_seq;

static void
test_unimplemented_cb(uint32_t rejected_seq, void *cbdata)
{
	(void)cbdata;
	test_unimpl_called = true;
	test_unimpl_seq = rejected_seq;
}

static int
test_unimplemented_invokes_callback(void)
{
	dssh_test_reset_global_config();
	dssh_transport_set_callbacks(mock_tx_dispatch, mock_rx_dispatch,
	    mock_rxline_dispatch, mock_extra_line_cb);
	ASSERT_OK(dssh_register_none_comp());

	struct mock_io_state io;
	ASSERT_OK(mock_io_init(&io, 0));

	dssh_session client = dssh_session_init(true, 0);
	ASSERT_NOT_NULL(client);
	dssh_session_set_cbdata(client, &io, &io, &io, &io);

	dssh_session server = init_server_session();
	ASSERT_NOT_NULL(server);
	dssh_session_set_cbdata(server, &io, &io, &io, &io);

	test_unimpl_called = false;
	test_unimpl_seq = 0;
	dssh_session_set_unimplemented_cb(server, test_unimplemented_cb, NULL);

	/* Build SSH_MSG_UNIMPLEMENTED: type(1) + uint32 rejected_seq */
	uint8_t unimpl_msg[8];
	size_t pos = 0;
	unimpl_msg[pos++] = SSH_MSG_UNIMPLEMENTED;
	dssh_serialize_uint32(42, unimpl_msg, sizeof(unimpl_msg), &pos);

	ASSERT_OK(send_packet(client, unimpl_msg, pos, NULL));

	/* Send a real message after */
	uint8_t real_msg[] = { SSH_MSG_SERVICE_REQUEST, 0x01 };
	ASSERT_OK(send_packet(client, real_msg, sizeof(real_msg), NULL));

	uint8_t msg_type;
	uint8_t *recv_payload;
	size_t recv_len;
	ASSERT_OK(recv_packet(server, &msg_type, &recv_payload, &recv_len));
	ASSERT_EQ(msg_type, SSH_MSG_SERVICE_REQUEST);

	ASSERT_TRUE(test_unimpl_called);
	ASSERT_EQ_U(test_unimpl_seq, 42);

	dssh_session_cleanup(client);
	dssh_session_cleanup(server);
	mock_io_free(&io);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

static int
test_multiple_ignore_before_real(void)
{
	dssh_test_reset_global_config();
	dssh_transport_set_callbacks(mock_tx_dispatch, mock_rx_dispatch,
	    mock_rxline_dispatch, mock_extra_line_cb);
	ASSERT_OK(dssh_register_none_comp());

	struct mock_io_state io;
	ASSERT_OK(mock_io_init(&io, 0));

	dssh_session client = dssh_session_init(true, 0);
	ASSERT_NOT_NULL(client);
	dssh_session_set_cbdata(client, &io, &io, &io, &io);

	dssh_session server = init_server_session();
	ASSERT_NOT_NULL(server);
	dssh_session_set_cbdata(server, &io, &io, &io, &io);

	/* Send 3 IGNORE messages then one real message */
	uint8_t ignore[] = { SSH_MSG_IGNORE };
	ASSERT_OK(send_packet(client, ignore, 1, NULL));
	ASSERT_OK(send_packet(client, ignore, 1, NULL));
	ASSERT_OK(send_packet(client, ignore, 1, NULL));

	uint8_t real[] = { SSH_MSG_SERVICE_REQUEST, 0x77 };
	ASSERT_OK(send_packet(client, real, sizeof(real), NULL));

	uint8_t msg_type;
	uint8_t *recv_payload;
	size_t recv_len;
	ASSERT_OK(recv_packet(server, &msg_type, &recv_payload, &recv_len));
	ASSERT_EQ(msg_type, SSH_MSG_SERVICE_REQUEST);
	ASSERT_EQ(recv_payload[1], 0x77);

	/* Server rx_seq should be 4 (received 4 packets total) */
	ASSERT_EQ_U(server->trans.rx_seq, 4);

	dssh_session_cleanup(client);
	dssh_session_cleanup(server);
	mock_io_free(&io);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

/* ================================================================
 * Full handshake (~4 tests)
 * ================================================================ */

static int
test_handshake_completes(void)
{
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		fprintf(stderr, "  handshake_setup failed\n");
		return TEST_FAIL;
	}

	ASSERT_EQ(ctx.client_result, 0);
	ASSERT_EQ(ctx.server_result, 0);

	handshake_cleanup(&ctx);
	return TEST_PASS;
}

static int
test_handshake_negotiated_algorithms(void)
{
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	/* Both sides should agree on the same algorithm names */
	const char *c_kex = dssh_transport_get_kex_name(ctx.client);
	const char *s_kex = dssh_transport_get_kex_name(ctx.server);
	ASSERT_NOT_NULL(c_kex);
	ASSERT_NOT_NULL(s_kex);
	ASSERT_STR_EQ(c_kex, s_kex);

	const char *c_hk = dssh_transport_get_hostkey_name(ctx.client);
	const char *s_hk = dssh_transport_get_hostkey_name(ctx.server);
	ASSERT_NOT_NULL(c_hk);
	ASSERT_NOT_NULL(s_hk);
	ASSERT_STR_EQ(c_hk, s_hk);

	const char *c_enc = dssh_transport_get_enc_name(ctx.client, DSSH_CLIENT_TO_SERVER);
	const char *s_enc = dssh_transport_get_enc_name(ctx.server, DSSH_CLIENT_TO_SERVER);
	ASSERT_NOT_NULL(c_enc);
	ASSERT_NOT_NULL(s_enc);
	ASSERT_STR_EQ(c_enc, s_enc);

	const char *c_mac = dssh_transport_get_mac_name(ctx.client, DSSH_CLIENT_TO_SERVER);
	const char *s_mac = dssh_transport_get_mac_name(ctx.server, DSSH_CLIENT_TO_SERVER);
	ASSERT_NOT_NULL(c_mac);
	ASSERT_NOT_NULL(s_mac);
	ASSERT_STR_EQ(c_mac, s_mac);

	handshake_cleanup(&ctx);
	return TEST_PASS;
}

static int
test_handshake_session_id_set(void)
{
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	/* Session ID should be set after handshake */
	ASSERT_NOT_NULL(ctx.client->trans.session_id);
	ASSERT_TRUE(ctx.client->trans.session_id_sz > 0);
	ASSERT_NOT_NULL(ctx.server->trans.session_id);
	ASSERT_TRUE(ctx.server->trans.session_id_sz > 0);

	/* Session IDs should match between client and server */
	ASSERT_EQ_U(ctx.client->trans.session_id_sz,
	    ctx.server->trans.session_id_sz);
	ASSERT_MEM_EQ(ctx.client->trans.session_id,
	    ctx.server->trans.session_id,
	    ctx.client->trans.session_id_sz);

	handshake_cleanup(&ctx);
	return TEST_PASS;
}

static int
test_handshake_encrypted_roundtrip(void)
{
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	/* After handshake, encryption is active.  Send a packet
	 * from client to server and verify it arrives intact. */
	uint8_t payload[] = { SSH_MSG_SERVICE_REQUEST, 0xCA, 0xFE, 0xBA, 0xBE };
	ASSERT_OK(send_packet(ctx.client, payload, sizeof(payload), NULL));

	uint8_t msg_type;
	uint8_t *recv_payload;
	size_t recv_len;
	ASSERT_OK(recv_packet(ctx.server, &msg_type, &recv_payload, &recv_len));
	ASSERT_EQ(msg_type, SSH_MSG_SERVICE_REQUEST);
	ASSERT_EQ_U(recv_len, sizeof(payload));
	ASSERT_MEM_EQ(recv_payload, payload, sizeof(payload));

	handshake_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Rekey (~9 tests)
 * ================================================================ */

/* ================================================================
 * Session lifecycle tests
 * ================================================================ */

static int
test_session_init_cleanup(void)
{
	dssh_test_reset_global_config();
	dssh_transport_set_callbacks(mock_tx_dispatch, mock_rx_dispatch,
	    mock_rxline_dispatch, mock_extra_line_cb);
	ASSERT_OK(dssh_register_none_comp());

	dssh_session sess = dssh_session_init(true, 0);
	ASSERT_NOT_NULL(sess);
	ASSERT_TRUE(sess->trans.client);
	ASSERT_FALSE(dssh_session_is_terminated(sess));
	ASSERT_TRUE(sess->trans.packet_buf_sz >= SSH_BPP_PACKET_SIZE_MIN);
	ASSERT_NOT_NULL(sess->trans.tx_packet);
	ASSERT_NOT_NULL(sess->trans.rx_packet);

	dssh_session_cleanup(sess);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

static int
test_session_terminate(void)
{
	dssh_test_reset_global_config();
	dssh_transport_set_callbacks(mock_tx_dispatch, mock_rx_dispatch,
	    mock_rxline_dispatch, mock_extra_line_cb);
	ASSERT_OK(dssh_register_none_comp());

	dssh_session sess = dssh_session_init(true, 0);
	ASSERT_NOT_NULL(sess);

	ASSERT_FALSE(dssh_session_is_terminated(sess));
	ASSERT_TRUE(dssh_session_terminate(sess));
	ASSERT_TRUE(dssh_session_is_terminated(sess));
	/* Double terminate returns false */
	ASSERT_FALSE(dssh_session_terminate(sess));

	dssh_session_cleanup(sess);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

static int
test_session_cleanup_null(void)
{
	/* Should not crash */
	dssh_session_cleanup(NULL);
	return TEST_PASS;
}

/* ================================================================
 * RFC conformance gap tests
 * ================================================================ */

/* RFC 4253 s9 / 4251 s9.3.3: hard limit refuses send at 2^31 */
static int
test_rekey_hard_limit_send(void)
{
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	ctx.client->trans.tx_since_rekey = DSSH_REKEY_HARD_LIMIT;
	uint8_t msg[] = { SSH_MSG_SERVICE_REQUEST, 0x42 };
	int res = send_packet(ctx.client, msg,
	    sizeof(msg), NULL);
	ASSERT_EQ(res, DSSH_ERROR_REKEY_NEEDED);

	handshake_cleanup(&ctx);
	return TEST_PASS;
}

/* RFC 4253 s9 / 4251 s9.3.3: hard limit refuses recv at 2^31 */
static int
test_rekey_hard_limit_recv(void)
{
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	/* Client sends a packet before we set the limit */
	uint8_t msg[] = { SSH_MSG_SERVICE_REQUEST, 0x42 };
	ASSERT_OK(send_packet(ctx.client, msg,
	    sizeof(msg), NULL));

	/* Set hard limit on server AFTER packet is in flight */
	ctx.server->trans.rx_since_rekey = DSSH_REKEY_HARD_LIMIT;

	uint8_t mt;
	uint8_t *payload;
	size_t plen;
	int res = recv_packet(ctx.server, &mt,
	    &payload, &plen);
	ASSERT_EQ(res, DSSH_ERROR_REKEY_NEEDED);
	/* Session must remain usable (matching send_packet's behavior) */
	ASSERT_FALSE(dssh_session_is_terminated(ctx.server));

	handshake_cleanup(&ctx);
	return TEST_PASS;
}

/* RFC 4253 s6.4-3: sequence numbers never reset across rekey */

/* RFC 4253 s6.4-2: MAC active after handshake */
static int
test_handshake_mac_active(void)
{
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	/* Both directions should have MAC contexts */
	ASSERT_NOT_NULL(ctx.client->trans.mac_c2s_ctx);
	ASSERT_NOT_NULL(ctx.client->trans.mac_s2c_ctx);
	ASSERT_NOT_NULL(ctx.client->trans.mac_c2s_selected);
	ASSERT_NOT_NULL(ctx.client->trans.mac_s2c_selected);

	/* Digest size should be 64 (HMAC-SHA-512) or 32 (HMAC-SHA-256) */
	ASSERT_TRUE(ctx.client->trans.mac_c2s_selected->digest_size == 64 ||
	    ctx.client->trans.mac_c2s_selected->digest_size == 32);
	ASSERT_TRUE(ctx.client->trans.mac_s2c_selected->digest_size == 64 ||
	    ctx.client->trans.mac_s2c_selected->digest_size == 32);

	/* Encryption should also be active */
	ASSERT_NOT_NULL(ctx.client->trans.enc_c2s_ctx);
	ASSERT_NOT_NULL(ctx.client->trans.enc_s2c_ctx);

	handshake_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * dssh_transport_set_version validation
 * ================================================================ */

static int
test_set_version_valid(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_transport_set_version("MySSH_1.0", NULL), 0);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

static int
test_set_version_valid_comment(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_transport_set_version("MySSH_1.0", "FreeBSD"), 0);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

static int
test_set_version_null(void)
{
	/* NULL software_version keeps the default -- should succeed */
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_transport_set_version(NULL, NULL), 0);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

static int
test_set_version_null_with_comment(void)
{
	/* NULL version + comment -- sets comment with default version */
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_transport_set_version(NULL, "my-comment"), 0);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

static int
test_set_version_empty(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_transport_set_version("", NULL), DSSH_ERROR_PARSE);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

static int
test_set_version_empty_comment(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_transport_set_version("MySSH_1.0", ""),
	    DSSH_ERROR_PARSE);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

static int
test_set_version_space(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_transport_set_version("My SSH", NULL),
	    DSSH_ERROR_INVALID);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

static int
test_set_version_comment_space(void)
{
	dssh_test_reset_global_config();
	/* Spaces are allowed in comments per RFC 4253 s4.2 */
	ASSERT_EQ(dssh_transport_set_version("MySSH", "has space"), 0);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

static int
test_set_version_comment_ctrl(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_transport_set_version("MySSH", "bad\x01comment"),
	    DSSH_ERROR_INVALID);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

static int
test_set_version_ctrl(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_transport_set_version("My\x01SSH", NULL),
	    DSSH_ERROR_INVALID);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

static int
test_set_version_high(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_transport_set_version("My\x80SSH", NULL),
	    DSSH_ERROR_INVALID);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

static int
test_set_version_too_long(void)
{
	dssh_test_reset_global_config();
	/* 255 - 8 ("SSH-2.0-") - 2 (CRLF) = 245 max for version alone */
	char long_ver[250];
	memset(long_ver, 'A', sizeof(long_ver) - 1);
	long_ver[sizeof(long_ver) - 1] = 0;
	ASSERT_EQ(dssh_transport_set_version(long_ver, NULL),
	    DSSH_ERROR_TOOLONG);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

static int
test_set_version_after_session(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(register_all_algorithms(), 0);
	dssh_transport_set_callbacks(mock_tx_dispatch, mock_rx_dispatch,
	    mock_rxline_dispatch, mock_extra_line_cb);

	dssh_session sess = dssh_session_init(true, 0);
	ASSERT_NOT_NULL(sess);

	/* Session init sets gconf.used = true */
	ASSERT_EQ(dssh_transport_set_version("Late", NULL),
	    DSSH_ERROR_TOOLATE);

	dssh_session_cleanup(sess);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

/* ================================================================
 * Version exchange with comment (Category 3)
 * ================================================================ */

static int
test_version_exchange_with_comment(void)
{
	struct handshake_ctx ctx;
	bool valid = false;
	memset(&ctx, 0, sizeof(ctx));

	dssh_test_reset_global_config();
	if (register_all_algorithms() < 0)
		return TEST_FAIL;
	if (test_generate_host_key() < 0)
		return TEST_FAIL;

	if (mock_io_init(&ctx.io, 0) < 0)
		return TEST_FAIL;

	if (dssh_transport_set_callbacks(mock_tx_dispatch, mock_rx_dispatch,
	    mock_rxline_dispatch, mock_extra_line_cb) != 0
	    || dssh_transport_set_version("DeuceSSH_0.0", "test-comment") != 0)
		goto cleanup;

	ctx.client = dssh_session_init(true, 0);
	if (ctx.client == NULL)
		goto cleanup;
	if (dssh_session_set_cbdata(ctx.client, &ctx.io, &ctx.io,
	    &ctx.io, &ctx.io) != 0
	    || dssh_session_set_hostkey_verify_cb(ctx.client, dssh_test_accept_hostkey, NULL) != 0)
		goto cleanup;

	ctx.server = init_server_session();
	if (ctx.server == NULL)
		goto cleanup;
	if (dssh_session_set_cbdata(ctx.server, &ctx.io, &ctx.io,
	    &ctx.io, &ctx.io) != 0)
		goto cleanup;

	thrd_t ct, st;
	if (thrd_create(&ct, handshake_client_thread, &ctx) != thrd_success)
		goto cleanup;
	if (thrd_create(&st, handshake_server_thread, &ctx) != thrd_success) {
		dssh_session_terminate(ctx.client);
		mock_io_close_c2s(&ctx.io);
		mock_io_close_s2c(&ctx.io);
		if (thrd_join(ct, NULL) != thrd_success)
			abort();
		goto cleanup;
	}
	if (thrd_join(ct, NULL) != thrd_success)
		abort();
	if (thrd_join(st, NULL) != thrd_success)
		abort();

	/* The local id string should contain the comment. */
	const char *id = ctx.client->trans.id_str;
	valid = ctx.client_result == 0 && ctx.server_result == 0
	    && id != NULL && strstr(id, "test-comment") != NULL;
cleanup:
	handshake_cleanup(&ctx);
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

/* ================================================================
 * Version exchange: rx error (Category 3)
 * ================================================================ */

static int
test_version_exchange_rx_error(void)
{
	dssh_test_reset_global_config();
	if (register_all_algorithms() < 0)
		return TEST_FAIL;
	if (test_generate_host_key() < 0)
		return TEST_FAIL;

	struct mock_io_state io;
	if (mock_io_init(&io, 0) < 0)
		return TEST_FAIL;

	dssh_transport_set_callbacks(mock_tx_dispatch, mock_rx_dispatch,
	    mock_rxline_dispatch, mock_extra_line_cb);

	dssh_session sess = dssh_session_init(true, 0);
	if (sess == NULL) {
		mock_io_free(&io);
		dssh_test_reset_global_config();
		return TEST_FAIL;
	}
	dssh_session_set_cbdata(sess, &io, &io, &io, &io);
	dssh_session_set_hostkey_verify_cb(sess, dssh_test_accept_hostkey, NULL);

	/* Close the s2c pipe before version exchange starts --
	 * the rxline callback will fail */
	mock_io_close_s2c(&io);

	int res = dssh_transport_handshake(sess);
	/* Should fail -- either the tx succeeds but rx fails with pipe closed */
	ASSERT_TRUE(res < 0);

	dssh_session_cleanup(sess);
	mock_io_free(&io);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

/* ================================================================
 * Version exchange: terminate mid-rx (Category 3)
 * ================================================================ */

static int
test_version_exchange_terminate(void)
{
	dssh_test_reset_global_config();
	if (register_all_algorithms() < 0)
		return TEST_FAIL;
	if (test_generate_host_key() < 0)
		return TEST_FAIL;

	struct mock_io_state io;
	if (mock_io_init(&io, 0) < 0)
		return TEST_FAIL;

	dssh_transport_set_callbacks(mock_tx_dispatch, mock_rx_dispatch,
	    mock_rxline_dispatch, mock_extra_line_cb);

	dssh_session sess = dssh_session_init(true, 0);
	if (sess == NULL) {
		mock_io_free(&io);
		dssh_test_reset_global_config();
		return TEST_FAIL;
	}
	dssh_session_set_cbdata(sess, &io, &io, &io, &io);
	dssh_session_set_hostkey_verify_cb(sess, dssh_test_accept_hostkey, NULL);

	/* Set terminate flag before handshake */
	dssh_session_terminate(sess);

	int res = dssh_transport_handshake(sess);
	ASSERT_TRUE(res < 0);

	dssh_session_cleanup(sess);
	mock_io_free(&io);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

/* ================================================================
 * Disconnect with long description (Category 3)
 * ================================================================ */

static int
test_disconnect_long_description(void)
{
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	/* Description > 230 bytes should be truncated */
	char long_desc[300];
	memset(long_desc, 'A', sizeof(long_desc) - 1);
	long_desc[sizeof(long_desc) - 1] = 0;

	int res = dssh_transport_disconnect(ctx.client, 11, long_desc);
	ASSERT_EQ(res, 0);

	/* Client should be terminated now */
	ASSERT_TRUE(dssh_session_is_terminated(ctx.client));

	handshake_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Extra-line callback error (Category 3)
 * ================================================================ */

static int
mock_extra_line_cb_error(uint8_t *buf, size_t bufsz, void *cbdata)
{
	(void)buf;
	(void)bufsz;
	(void)cbdata;
	return -1;
}

static int
test_version_exchange_extra_line_error(void)
{
	dssh_test_reset_global_config();
	if (register_all_algorithms() < 0)
		return TEST_FAIL;
	if (test_generate_host_key() < 0)
		return TEST_FAIL;

	struct mock_io_state io;
	if (mock_io_init(&io, 0) < 0)
		return TEST_FAIL;

	dssh_transport_set_callbacks(mock_tx_dispatch, mock_rx_dispatch,
	    mock_rxline_dispatch, mock_extra_line_cb_error);

	dssh_session sess = dssh_session_init(true, 0);
	if (sess == NULL) {
		mock_io_free(&io);
		dssh_test_reset_global_config();
		return TEST_FAIL;
	}
	dssh_session_set_cbdata(sess, &io, &io, &io, &io);
	dssh_session_set_hostkey_verify_cb(sess, dssh_test_accept_hostkey, NULL);

	/* Inject a non-SSH line followed by the real version.
	 * The extra_line_cb will return -1, aborting version_rx. */
	const char *extra = "This is not SSH\r\n";
	mock_io_inject(&io.s2c, (const uint8_t *)extra, strlen(extra));

	int res = dssh_transport_handshake(sess);
	ASSERT_TRUE(res < 0);

	dssh_session_cleanup(sess);
	mock_io_free(&io);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

/* ================================================================
 * Registration validation -- covers TOOLATE, TOOLONG, MUST_BE_NULL
 * branches in register_kex/key_algo/enc/mac/comp.
 * ================================================================ */

#include "registration_cases.h"

static int
test_register_kex_toolate(void)
{
	ASSERT_TRUE(registration_case(REG_KEX, REG_LATE));
	return TEST_PASS;
}

static int
test_register_kex_empty_name(void)
{
	ASSERT_TRUE(registration_case(REG_KEX, REG_EMPTY));
	return TEST_PASS;
}

static int
test_register_kex_next_not_null(void)
{
	ASSERT_TRUE(registration_case(REG_KEX, REG_NEXT));
	return TEST_PASS;
}

static int
test_register_enc_toolate(void)
{
	ASSERT_TRUE(registration_case(REG_ENC, REG_LATE));
	return TEST_PASS;
}

static int
test_register_mac_toolate(void)
{
	ASSERT_TRUE(registration_case(REG_MAC, REG_LATE));
	return TEST_PASS;
}

static int
test_register_comp_empty_name(void)
{
	ASSERT_TRUE(registration_case(REG_COMP, REG_EMPTY));
	return TEST_PASS;
}







/* ================================================================
 * Additional registration validation: TOOLONG, MUST_BE_NULL, TOOLATE
 * for key_algo, enc, mac, comp.
 * ================================================================ */

static int
test_register_key_algo_toolate(void)
{
	ASSERT_TRUE(registration_case(REG_KEY, REG_LATE));
	return TEST_PASS;
}

static int
test_set_ctx_toolate(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(register_all_algorithms(), 0);
	dssh_transport_set_callbacks(mock_tx_dispatch, mock_rx_dispatch,
	    mock_rxline_dispatch, mock_extra_line_cb);

	/* Before session init -- should succeed */
	ASSERT_OK(dssh_key_algo_set_ctx("ssh-ed25519", NULL));

	dssh_session sess = dssh_session_init(true, 0);
	ASSERT_NOT_NULL(sess);

	/* After session init -- should be refused */
	ASSERT_EQ(dssh_key_algo_set_ctx("ssh-ed25519", NULL),
	    DSSH_ERROR_TOOLATE);

	dssh_session_cleanup(sess);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

static int
test_register_key_algo_toolong(void)
{
	ASSERT_TRUE(registration_case(REG_KEY, REG_LONG_EMPTY));
	return TEST_PASS;
}

static int
test_register_key_algo_next_not_null(void)
{
	ASSERT_TRUE(registration_case(REG_KEY, REG_NEXT));
	return TEST_PASS;
}

static int
test_register_enc_toolong(void)
{
	ASSERT_TRUE(registration_case(REG_ENC, REG_LONG_EMPTY));
	return TEST_PASS;
}

static int
test_register_enc_next_not_null(void)
{
	ASSERT_TRUE(registration_case(REG_ENC, REG_NEXT));
	return TEST_PASS;
}

static int
test_register_mac_toolong(void)
{
	ASSERT_TRUE(registration_case(REG_MAC, REG_LONG_EMPTY));
	return TEST_PASS;
}

static int
test_register_mac_next_not_null(void)
{
	ASSERT_TRUE(registration_case(REG_MAC, REG_NEXT));
	return TEST_PASS;
}

static int
test_register_comp_toolate(void)
{
	ASSERT_TRUE(registration_case(REG_COMP, REG_LATE));
	return TEST_PASS;
}

static int
test_register_comp_next_not_null(void)
{
	ASSERT_TRUE(registration_case(REG_COMP, REG_NEXT));
	return TEST_PASS;
}



/* ================================================================
 * Getter-before-handshake -- covers NULL ternary branches in
 * get_kex_name, get_hostkey_name, get_enc_name, get_mac_name.
 * ================================================================ */

/* ================================================================
 * set_callbacks after session -- covers TOOLATE branch.
 * ================================================================ */

/* ================================================================
 * set_global_request_cb -- covers ssh.c:98-103 (never called).
 * ================================================================ */

/* ================================================================
 * GLOBAL_REQUEST handling -- covers ssh-trans.c:753-787 (8+ branches)
 * ================================================================ */

static int global_request_cb_result = 0;
static char global_request_name[64];
static size_t global_request_name_len;
static bool global_request_want_reply;

static int
mock_global_request_cb(const uint8_t *name, size_t name_len,
    bool want_reply, const uint8_t *data, size_t data_len, void *cbdata)
{
	(void)data;
	(void)data_len;
	(void)cbdata;
	if (name_len < sizeof(global_request_name)) {
		memcpy(global_request_name, name, name_len);
		global_request_name[name_len] = 0;
	}
	global_request_name_len = name_len;
	global_request_want_reply = want_reply;
	return global_request_cb_result;
}

static int
test_global_request_with_reply(void)
{
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}
	dssh_session client = ctx.client;
	dssh_session server = ctx.server;

	/* Register global request callback on server */
	global_request_cb_result = 0;  /* accept */
	memset(global_request_name, 0, sizeof(global_request_name));
	ASSERT_OK(dssh_session_set_global_request_cb(server,
	    mock_global_request_cb, NULL));

	/* Build GLOBAL_REQUEST: msg_type(1) + string("test-req") + want_reply(1) */
	uint8_t gr[64];
	size_t gp = 0;
	gr[gp++] = SSH_MSG_GLOBAL_REQUEST;  /* SSH_MSG_GLOBAL_REQUEST */
	ASSERT_OK(dssh_serialize_uint32(8, gr, sizeof(gr), &gp));
	memcpy(&gr[gp], "test-req", 8);
	gp += 8;
	gr[gp++] = 1;  /* want_reply = true */

	/* Send GLOBAL_REQUEST from client */
	ASSERT_OK(send_packet(client, gr, gp, NULL));

	/* Send a follow-up SERVICE_REQUEST so recv_packet returns */
	uint8_t follow[] = { SSH_MSG_SERVICE_REQUEST, 0x42 };
	ASSERT_OK(send_packet(client, follow, sizeof(follow), NULL));

	/* Server recv_packet: processes GLOBAL_REQUEST internally, returns SERVICE_REQUEST */
	uint8_t msg_type;
	uint8_t *payload;
	size_t payload_len;
	ASSERT_OK(recv_packet(server, &msg_type, &payload, &payload_len));
	ASSERT_EQ(msg_type, SSH_MSG_SERVICE_REQUEST);

	/* Verify callback was invoked */
	ASSERT_STR_EQ(global_request_name, "test-req");
	ASSERT_TRUE(global_request_want_reply);

	/* Server should have sent REQUEST_SUCCESS (81) reply.
	 * Client reads it. */
	ASSERT_OK(recv_packet(client, &msg_type, &payload, &payload_len));
	ASSERT_EQ(msg_type, SSH_MSG_REQUEST_SUCCESS);  /* SSH_MSG_REQUEST_SUCCESS */

	handshake_cleanup(&ctx);
	return TEST_PASS;
}

static int
test_global_request_rejected(void)
{
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}
	dssh_session client = ctx.client;
	dssh_session server = ctx.server;

	/* Callback returns -1 (reject) */
	global_request_cb_result = -1;
	ASSERT_OK(dssh_session_set_global_request_cb(server,
	    mock_global_request_cb, NULL));

	uint8_t gr[64];
	size_t gp = 0;
	gr[gp++] = SSH_MSG_GLOBAL_REQUEST;
	ASSERT_OK(dssh_serialize_uint32(4, gr, sizeof(gr), &gp));
	memcpy(&gr[gp], "deny", 4);
	gp += 4;
	gr[gp++] = 1;  /* want_reply */

	ASSERT_OK(send_packet(client, gr, gp, NULL));

	uint8_t follow[] = { SSH_MSG_SERVICE_REQUEST, 0x43 };
	ASSERT_OK(send_packet(client, follow, sizeof(follow), NULL));

	uint8_t msg_type;
	uint8_t *payload;
	size_t payload_len;
	ASSERT_OK(recv_packet(server, &msg_type, &payload, &payload_len));
	ASSERT_EQ(msg_type, SSH_MSG_SERVICE_REQUEST);

	/* Server sent REQUEST_FAILURE (82) */
	ASSERT_OK(recv_packet(client, &msg_type, &payload, &payload_len));
	ASSERT_EQ(msg_type, SSH_MSG_REQUEST_FAILURE);  /* SSH_MSG_REQUEST_FAILURE */

	handshake_cleanup(&ctx);
	return TEST_PASS;
}

static int
test_global_request_no_reply(void)
{
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}
	dssh_session client = ctx.client;
	dssh_session server = ctx.server;

	global_request_cb_result = 0;
	ASSERT_OK(dssh_session_set_global_request_cb(server,
	    mock_global_request_cb, NULL));

	/* want_reply = false */
	uint8_t gr[64];
	size_t gp = 0;
	gr[gp++] = SSH_MSG_GLOBAL_REQUEST;
	ASSERT_OK(dssh_serialize_uint32(5, gr, sizeof(gr), &gp));
	memcpy(&gr[gp], "quiet", 5);
	gp += 5;
	gr[gp++] = 0;  /* want_reply = false */

	ASSERT_OK(send_packet(client, gr, gp, NULL));

	uint8_t follow[] = { SSH_MSG_SERVICE_REQUEST, 0x44 };
	ASSERT_OK(send_packet(client, follow, sizeof(follow), NULL));

	uint32_t before = server->trans.tx_seq;
	uint8_t msg_type;
	uint8_t *payload;
	size_t payload_len;
	ASSERT_OK(recv_packet(server, &msg_type, &payload, &payload_len));
	ASSERT_EQ(msg_type, SSH_MSG_SERVICE_REQUEST);

	ASSERT_STR_EQ(global_request_name, "quiet");
	ASSERT_FALSE(global_request_want_reply);
	ASSERT_EQ_U(server->trans.tx_seq, before);

	handshake_cleanup(&ctx);
	return TEST_PASS;
}

static int
test_global_request_no_callback(void)
{
	/* No global_request_cb set -- should auto-reject with FAILURE */
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}
	dssh_session client = ctx.client;
	dssh_session server = ctx.server;
	/* No global_request_cb set */

	uint8_t gr[64];
	size_t gp = 0;
	gr[gp++] = SSH_MSG_GLOBAL_REQUEST;
	ASSERT_OK(dssh_serialize_uint32(6, gr, sizeof(gr), &gp));
	memcpy(&gr[gp], "nocb-r", 6);
	gp += 6;
	gr[gp++] = 1;  /* want_reply = true */

	ASSERT_OK(send_packet(client, gr, gp, NULL));

	uint8_t follow[] = { SSH_MSG_SERVICE_REQUEST, 0x45 };
	ASSERT_OK(send_packet(client, follow, sizeof(follow), NULL));

	uint8_t msg_type;
	uint8_t *payload;
	size_t payload_len;
	ASSERT_OK(recv_packet(server, &msg_type, &payload, &payload_len));
	ASSERT_EQ(msg_type, SSH_MSG_SERVICE_REQUEST);

	/* No callback -> gr_res=-1 -> FAILURE reply */
	ASSERT_OK(recv_packet(client, &msg_type, &payload, &payload_len));
	ASSERT_EQ(msg_type, SSH_MSG_REQUEST_FAILURE);  /* SSH_MSG_REQUEST_FAILURE */

	handshake_cleanup(&ctx);
	return TEST_PASS;
}

static int
test_global_request_truncated(void)
{
	/* Truncated GLOBAL_REQUEST -- too short for name length field */
	dssh_test_reset_global_config();
	ASSERT_OK(register_all_algorithms());
	if (test_generate_host_key() < 0)
		return TEST_FAIL;
	dssh_transport_set_callbacks(mock_tx_dispatch, mock_rx_dispatch,
	    mock_rxline_dispatch, mock_extra_line_cb);

	struct mock_io_state io;
	ASSERT_OK(mock_io_init(&io, 0));

	dssh_session client = dssh_session_init(true, 0);
	ASSERT_NOT_NULL(client);
	dssh_session_set_cbdata(client, &io, &io, &io, &io);

	dssh_session server = init_server_session();
	ASSERT_NOT_NULL(server);
	dssh_session_set_cbdata(server, &io, &io, &io, &io);

	/* Just the msg_type byte -- not enough for name length.
	 * Malformed GLOBAL_REQUEST disconnects, so recv_packet returns an error. */
	uint8_t gr[] = { 80 };
	ASSERT_OK(send_packet(client, gr, sizeof(gr), NULL));

	uint8_t msg_type;
	uint8_t *payload;
	size_t payload_len;
	ASSERT_TRUE(recv_packet(server, &msg_type, &payload, &payload_len) < 0);

	dssh_session_cleanup(client);
	dssh_session_cleanup(server);
	mock_io_free(&io);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

/* ================================================================
 * DEBUG edge cases -- covers truncated/empty message branches
 * ================================================================ */

static bool debug_cb_invoked;
static size_t debug_msg_len;

static void
mock_debug_cb_track(bool always, const uint8_t *msg, size_t len, void *cbdata)
{
	(void)always;
	(void)msg;
	(void)cbdata;
	debug_cb_invoked = true;
	debug_msg_len = len;
}

static int
test_debug_truncated_payload(void)
{
	/* DEBUG with only 2 bytes (type + bool, no string) */
	dssh_test_reset_global_config();
	ASSERT_OK(register_all_algorithms());
	if (test_generate_host_key() < 0)
		return TEST_FAIL;
	dssh_transport_set_callbacks(mock_tx_dispatch, mock_rx_dispatch,
	    mock_rxline_dispatch, mock_extra_line_cb);

	struct mock_io_state io;
	ASSERT_OK(mock_io_init(&io, 0));

	dssh_session client = dssh_session_init(true, 0);
	ASSERT_NOT_NULL(client);
	dssh_session_set_cbdata(client, &io, &io, &io, &io);

	dssh_session server = init_server_session();
	ASSERT_NOT_NULL(server);
	dssh_session_set_cbdata(server, &io, &io, &io, &io);

	debug_cb_invoked = false;
	debug_msg_len = 999;
	dssh_session_set_debug_cb(server, mock_debug_cb_track, NULL);

	/* DEBUG: type(4) + always_display(1) -- no string */
	uint8_t dbg[] = { SSH_MSG_DEBUG, 1 };
	ASSERT_OK(send_packet(client, dbg, sizeof(dbg), NULL));

	uint8_t msg_type;
	uint8_t *payload;
	size_t payload_len;
	ASSERT_EQ(recv_packet(server, &msg_type, &payload, &payload_len), DSSH_ERROR_PARSE);

	ASSERT_FALSE(debug_cb_invoked);
	ASSERT_TRUE(dssh_session_is_terminated(server));

	dssh_session_cleanup(client);
	dssh_session_cleanup(server);
	mock_io_free(&io);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

static int
test_debug_no_callback(void)
{
	/* DEBUG received without debug_cb set -- should be silently skipped */
	dssh_test_reset_global_config();
	ASSERT_OK(register_all_algorithms());
	if (test_generate_host_key() < 0)
		return TEST_FAIL;
	dssh_transport_set_callbacks(mock_tx_dispatch, mock_rx_dispatch,
	    mock_rxline_dispatch, mock_extra_line_cb);

	struct mock_io_state io;
	ASSERT_OK(mock_io_init(&io, 0));

	dssh_session client = dssh_session_init(true, 0);
	ASSERT_NOT_NULL(client);
	dssh_session_set_cbdata(client, &io, &io, &io, &io);

	dssh_session server = init_server_session();
	ASSERT_NOT_NULL(server);
	dssh_session_set_cbdata(server, &io, &io, &io, &io);
	/* No debug_cb set */

	uint8_t dbg[] = { SSH_MSG_DEBUG, 0, 0, 0, 0, 5, 'h', 'e', 'l', 'l', 'o', 0, 0, 0, 0 };
	ASSERT_OK(send_packet(client, dbg, sizeof(dbg), NULL));

	uint8_t follow[] = { SSH_MSG_SERVICE_REQUEST, 0x48 };
	ASSERT_OK(send_packet(client, follow, sizeof(follow), NULL));

	uint8_t msg_type;
	uint8_t *payload;
	size_t payload_len;
	ASSERT_OK(recv_packet(server, &msg_type, &payload, &payload_len));
	ASSERT_EQ(msg_type, SSH_MSG_SERVICE_REQUEST);

	dssh_session_cleanup(client);
	dssh_session_cleanup(server);
	mock_io_free(&io);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

static void
unimplemented_flag_cb(uint32_t seq, void *cbdata)
{
	(void)seq;
	bool *called = cbdata;
	*called = true;
}

static int
test_unimplemented_short_payload(void)
{
	/* UNIMPLEMENTED with payload < 5 bytes -- should be silently skipped */
	dssh_test_reset_global_config();
	ASSERT_OK(register_all_algorithms());
	if (test_generate_host_key() < 0)
		return TEST_FAIL;
	dssh_transport_set_callbacks(mock_tx_dispatch, mock_rx_dispatch,
	    mock_rxline_dispatch, mock_extra_line_cb);

	struct mock_io_state io;
	ASSERT_OK(mock_io_init(&io, 0));

	dssh_session client = dssh_session_init(true, 0);
	ASSERT_NOT_NULL(client);
	dssh_session_set_cbdata(client, &io, &io, &io, &io);

	dssh_session server = init_server_session();
	ASSERT_NOT_NULL(server);
	dssh_session_set_cbdata(server, &io, &io, &io, &io);

	bool unimpl_invoked = false;
	ASSERT_OK(dssh_session_set_unimplemented_cb(server, unimplemented_flag_cb, &unimpl_invoked));

	/* UNIMPLEMENTED with just 2 bytes (needs 5 for seq number) */
	uint8_t unimp[] = { SSH_MSG_UNIMPLEMENTED, 0x00 };
	ASSERT_OK(send_packet(client, unimp, sizeof(unimp), NULL));

	uint8_t follow[] = { SSH_MSG_SERVICE_REQUEST, 0x49 };
	ASSERT_OK(send_packet(client, follow, sizeof(follow), NULL));

	uint8_t msg_type;
	uint8_t *payload;
	size_t payload_len;
	ASSERT_OK(recv_packet(server, &msg_type, &payload, &payload_len));
	ASSERT_EQ(msg_type, SSH_MSG_SERVICE_REQUEST);
	ASSERT_FALSE(unimpl_invoked);

	dssh_session_cleanup(client);
	dssh_session_cleanup(server);
	mock_io_free(&io);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

/* ================================================================
 * max_packet_size clamping -- covers ssh-trans.c:1419-1422
 * ================================================================ */

/* ================================================================
 * build_namelist overflow -- covers ssh-trans.c:818-823
 * ================================================================ */

static int
test_build_namelist_overflow(void)
{
	dssh_test_reset_global_config();
	struct dssh_kex_s *first = malloc(sizeof(*first) + sizeof("alpha"));
	struct dssh_kex_s *second = malloc(sizeof(*second) + sizeof("bravo"));
	if (first == NULL || second == NULL) {
		free(first);
		free(second);
		return TEST_FAIL;
	}
	*first = (struct dssh_kex_s){ .next = second };
	*second = (struct dssh_kex_s){0};
	memcpy(first->name, "alpha", sizeof("alpha"));
	memcpy(second->name, "bravo", sizeof("bravo"));

	/* A comma and complete next name must fit together. All three
	 * capacities fail, preserving the previously completed prefix. */
	char buf[8];
	int64_t len = build_namelist(first, offsetof(struct dssh_kex_s, name),
	    registry_next_kex, buf, sizeof(buf), NULL);
	bool valid = len == DSSH_ERROR_TOOLONG && strcmp(buf, "alpha") == 0;
	char exact[6];
	len = build_namelist(first, offsetof(struct dssh_kex_s, name),
	    registry_next_kex, exact, sizeof(exact), NULL);
	valid = valid && len == DSSH_ERROR_TOOLONG && strcmp(exact, "alpha") == 0;
	char tiny[4];
	len = build_namelist(first, offsetof(struct dssh_kex_s, name),
	    registry_next_kex, tiny, sizeof(tiny), NULL);
	valid = valid && len == DSSH_ERROR_TOOLONG && tiny[0] == 0;
	free(second);
	free(first);
	dssh_test_reset_global_config();
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

/* Complete names are published; insufficient capacity leaves no trailing
 * comma. Allocated descriptors provide the declared type and alignment. */
static int
test_build_namelist_truncation_no_trailing_comma(void)
{
	dssh_test_reset_global_config();
	struct dssh_kex_s *entries[3] = {NULL, NULL, NULL};
	for (size_t i = 0; i < 3; i++) {
		entries[i] = malloc(sizeof(*entries[i]) + sizeof("alpha"));
		if (entries[i] == NULL) {
			for (size_t j = 0; j < i; j++)
				free(entries[j]);
			return TEST_FAIL;
		}
		*entries[i] = (struct dssh_kex_s){0};
	}
	entries[0]->next = entries[1];
	memcpy(entries[0]->name, "alpha", sizeof("alpha"));
	memcpy(entries[1]->name, "bravo", sizeof("bravo"));
	char exact[12];
	int64_t len = build_namelist(entries[0], offsetof(struct dssh_kex_s, name),
	    registry_next_kex, exact, sizeof(exact), NULL);
	bool valid = len == 11 && strcmp(exact, "alpha,bravo") == 0;
	char short_buf[11];
	len = build_namelist(entries[0], offsetof(struct dssh_kex_s, name),
	    registry_next_kex, short_buf, sizeof(short_buf), NULL);
	valid = valid && len == DSSH_ERROR_TOOLONG && strcmp(short_buf, "alpha") == 0;

	/* Reuse the same objects for the three-entry boundary. */
	entries[1]->next = entries[2];
	memcpy(entries[0]->name, "aa", sizeof("aa"));
	memcpy(entries[1]->name, "bb", sizeof("bb"));
	memcpy(entries[2]->name, "cc", sizeof("cc"));
	char three[7];
	len = build_namelist(entries[0], offsetof(struct dssh_kex_s, name),
	    registry_next_kex, three, sizeof(three), NULL);
	valid = valid && len == DSSH_ERROR_TOOLONG && strcmp(three, "aa,bb") == 0;
	char tight[6];
	len = build_namelist(entries[0], offsetof(struct dssh_kex_s, name),
	    registry_next_kex, tight, sizeof(tight), NULL);
	valid = valid && len == DSSH_ERROR_TOOLONG && strcmp(tight, "aa,bb") == 0;
	for (size_t i = 0; i < 3; i++)
		free(entries[i]);
	dssh_test_reset_global_config();
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

/* ================================================================
 * Cleanup session that never handshaked -- covers cleanup branches
 * for NULL kex_selected/enc_selected/etc.
 * ================================================================ */

static int
test_cleanup_no_handshake(void)
{
	dssh_test_reset_global_config();
	ASSERT_OK(register_all_algorithms());
	if (test_generate_host_key() < 0)
		return TEST_FAIL;
	dssh_transport_set_callbacks(mock_tx_dispatch, mock_rx_dispatch,
	    mock_rxline_dispatch, mock_extra_line_cb);

	dssh_session sess = dssh_session_init(true, 0);
	ASSERT_NOT_NULL(sess);

	/* All *_selected should be NULL -- cleanup should handle gracefully */
	ASSERT_NULL(sess->trans.kex_selected);
	ASSERT_NULL(sess->trans.enc_c2s_selected);
	ASSERT_NULL(sess->trans.mac_c2s_selected);

	dssh_session_cleanup(sess);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

/* ================================================================
 * set_version with long string -- covers version_tx TOOLONG
 * ================================================================ */

static int
test_set_version_long_string(void)
{
	dssh_test_reset_global_config();

	/* 250-char version string -- should succeed (just under 255 limit with SSH-2.0- prefix) */
	char long_ver[241];
	memset(long_ver, 'A', 240);
	long_ver[240] = 0;
	ASSERT_EQ(dssh_transport_set_version(long_ver, NULL), 0);

	dssh_test_reset_global_config();

	/* 249-char version + comment -- exceeds 255 combined */
	char ver[242];
	memset(ver, 'B', 241);
	ver[241] = 0;
	int res = dssh_transport_set_version(ver, "comment");
	/* Should fail because "SSH-2.0-" + 241 + " " + "comment" > 255 */
	ASSERT_EQ(res, DSSH_ERROR_TOOLONG);

	dssh_test_reset_global_config();
	return TEST_PASS;
}

/* ================================================================
 * set_version high-byte validation -- covers is_valid_sw_version
 * and is_valid_comment > 0x7E branches.
 * ================================================================ */

static int
test_set_version_high_byte_version(void)
{
	dssh_test_reset_global_config();

	/* Version with DEL (0x7F) -- out of range */
	char bad_ver[] = { 't', 'e', 's', 't', 0x7F, 'v', 0 };
	ASSERT_EQ(dssh_transport_set_version(bad_ver, NULL), DSSH_ERROR_INVALID);

	dssh_test_reset_global_config();

	/* Version with 0x80 -- out of range */
	char bad_ver2[] = { 't', 'e', 's', 't', (char)0x80, 'v', 0 };
	ASSERT_EQ(dssh_transport_set_version(bad_ver2, NULL), DSSH_ERROR_INVALID);

	dssh_test_reset_global_config();
	return TEST_PASS;
}

static int
test_set_version_high_byte_comment(void)
{
	dssh_test_reset_global_config();

	/* Comment with DEL (0x7F) */
	char bad_cm[] = { 't', 'e', 's', 't', 0x7F, 'c', 0 };
	ASSERT_EQ(dssh_transport_set_version("DeuceSSH", bad_cm),
	    DSSH_ERROR_INVALID);

	dssh_test_reset_global_config();

	/* Comment with 0x80 */
	char bad_cm2[] = { 't', 'e', 's', 't', (char)0x80, 'c', 0 };
	ASSERT_EQ(dssh_transport_set_version("DeuceSSH", bad_cm2),
	    DSSH_ERROR_INVALID);

	dssh_test_reset_global_config();
	return TEST_PASS;
}

/* ================================================================
 * Version line parsing edge cases -- covers short-circuit branches
 * in is_version_line and is_20.
 * ================================================================ */

static int
test_is_version_line_ss_not_ssh(void)
{
	/* "SSx-..." -- starts with SS but not SSH */
	uint8_t buf[] = "SSx-2.0-test\r\n";
	ASSERT_FALSE(is_version_line(buf, sizeof(buf) - 1));
	return TEST_PASS;
}

static int
test_is_20_bad_minor(void)
{
	/* "SSH-2.x-test" -- major is 2 but separator isn't ".0-" */
	uint8_t buf[] = "SSH-2.x-test\r\n";
	ASSERT_FALSE(is_20(buf, sizeof(buf) - 1));
	return TEST_PASS;
}

static int
test_is_20_199_partial(void)
{
	/* "SSH-1.9x-test" -- starts like 1.99 but isn't */
	uint8_t buf[] = "SSH-1.9x-test\r\n";
	ASSERT_FALSE(is_20(buf, sizeof(buf) - 1));
	return TEST_PASS;
}

static int
test_is_20_199_short_buf(void)
{
	/* 8-byte buffer: "SSH-3.0-" -- fails buflen >= 9 on line 78 */
	uint8_t buf[] = "SSH-3.0-";
	ASSERT_FALSE(is_20(buf, 8));
	return TEST_PASS;
}

static int
test_is_20_199_bad_minor_digit(void)
{
	/* "SSH-1.88-test" -- buf[6]=='8' not '9' */
	uint8_t buf[] = "SSH-1.88-test\r\n";
	ASSERT_FALSE(is_20(buf, sizeof(buf) - 1));
	return TEST_PASS;
}

static int
test_is_20_199_no_dash(void)
{
	/* "SSH-1.99Xtest" -- buf[8]=='X' not '-' */
	uint8_t buf[] = "SSH-1.99Xtest\r\n";
	ASSERT_FALSE(is_20(buf, sizeof(buf) - 1));
	return TEST_PASS;
}

/* ================================================================
 * version_tx -- defense-in-depth TOOLONG paths
 *
 * These are unreachable via the public API (set_version validates
 * length first), but the code exists and should be tested.
 * We bypass set_version by writing directly to the global config.
 * ================================================================ */

/* dssh_test_set_sw_version/dssh_test_set_version_comment declared
 * in dssh_test_internal.h */

static int
test_version_tx_toolong_version(void)
{
	dssh_test_reset_global_config();
	ASSERT_OK(register_all_algorithms());
	if (test_generate_host_key() < 0)
		return TEST_FAIL;
	dssh_transport_set_callbacks(mock_tx_dispatch, mock_rx_dispatch,
	    mock_rxline_dispatch, mock_extra_line_cb);

	/* Inject a version string that's too long when combined with
	 * "SSH-2.0-" prefix + "\r\n" suffix (>255 total) */
	char long_ver[250];
	memset(long_ver, 'A', sizeof(long_ver) - 1);
	long_ver[sizeof(long_ver) - 1] = 0;
	dssh_test_set_sw_version(long_ver);

	struct mock_io_state io;
	ASSERT_OK(mock_io_init(&io, 0));
	dssh_session sess = dssh_session_init(true, 0);
	ASSERT_NOT_NULL(sess);
	dssh_session_set_cbdata(sess, &io, &io, &io, &io);

	int res = version_tx(sess);
	ASSERT_EQ(res, DSSH_ERROR_TOOLONG);

	dssh_session_cleanup(sess);
	mock_io_free(&io);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

static int
test_version_tx_toolong_comment(void)
{
	dssh_test_reset_global_config();
	ASSERT_OK(register_all_algorithms());
	if (test_generate_host_key() < 0)
		return TEST_FAIL;
	dssh_transport_set_callbacks(mock_tx_dispatch, mock_rx_dispatch,
	    mock_rxline_dispatch, mock_extra_line_cb);

	/* Version fits, but version + comment exceeds 255 */
	char long_comment[250];
	memset(long_comment, 'B', sizeof(long_comment) - 1);
	long_comment[sizeof(long_comment) - 1] = 0;
	dssh_test_set_sw_version("DeuceSSH");
	dssh_test_set_version_comment(long_comment);

	struct mock_io_state io;
	ASSERT_OK(mock_io_init(&io, 0));
	dssh_session sess = dssh_session_init(true, 0);
	ASSERT_NOT_NULL(sess);
	dssh_session_set_cbdata(sess, &io, &io, &io, &io);

	int res = version_tx(sess);
	ASSERT_EQ(res, DSSH_ERROR_TOOLONG);

	dssh_session_cleanup(sess);
	mock_io_free(&io);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

/* ================================================================
 * DH-GEX server handler -- targeted branch coverage tests.
 *
 * Each test uses the same one-time setup: version_exchange + kexinit
 * in threads to populate negotiated state, then injects specific
 * crafted packets to exercise individual error paths.
 * ================================================================ */

#include "dssh_test_ossl.h"

#include "direct_kex_fixture.h"

/* DH-GEX server handler infrastructure */
struct dhgex_server_ctx {
	struct test_session_pair pair;
	dssh_session server;
};

static int
dhgex_server_setup(struct dhgex_server_ctx *ctx)
{
	ctx->server = NULL;
	int ret = direct_kex_setup(&ctx->pair, true);
	if (ret == 0)
		ctx->server = ctx->pair.server;
	return ret;
}

static void
dhgex_server_teardown(struct dhgex_server_ctx *ctx)
{
	ctx->server = NULL;
	direct_kex_dispose(&ctx->pair);
}

static int group_test_alloc_failure = -1;

/* Run the server handler with specific injected packets */
static int
dhgex_server_run(struct dhgex_server_ctx *ctx,
    const uint8_t *wire, size_t wire_len, int *operation)
{
	return direct_kex_run(&ctx->pair, wire, wire_len, group_test_alloc_failure, operation);
}
/* end DH-GEX server handler infra */

static int
finish_transport_group(struct dssh_dh_gex_group_result *out,
    const uint8_t *p, size_t plen, const uint8_t *g, size_t glen, int status)
{
	out->p = (struct dssh_bytes){p, plen};
	out->g = (struct dssh_bytes){g, glen};
	return status;
}

static int
release_group_provider(uint32_t min, uint32_t preferred, uint32_t max,
    struct dssh_dh_gex_group_result *out, void *cbdata)
{
	uint8_t *p_bytes = NULL, *g_bytes = NULL;
	size_t p_length = 0, g_length = 0;
	uint8_t **p = &p_bytes, **g = &g_bytes;
	size_t *p_len = &p_length, *g_len = &g_length;
	(void)min;
	(void)preferred;
	(void)max;
	unsigned *which = cbdata;
	*p_len = rfc3526_groups[*which == 5 ? 2 : 0].p_len;
	*p = test_auth_lease_alloc(&out->lease, *p_len);
	*g_len = *which == 2 ? *p_len : 1;
	*g = test_auth_lease_alloc(&out->lease, *g_len);
	if (*p == NULL || *g == NULL) {
		*p = NULL;
		*g = NULL;
		return finish_transport_group(out, p_bytes, p_length, g_bytes, g_length, DSSH_ERROR_ALLOC);
	}
	memcpy(*p, rfc3526_groups[*which == 5 ? 2 : 0].p, *p_len);
	(*g)[0] = 2;
	if (*which == 0)
		(*g)[0] = 0;
	else if (*which == 1)
		(*g)[0] = 1;
	else if (*which == 2) {
		memcpy(*g, *p, *p_len);
		(*g)[*g_len - 1]--;
	}
	else if (*which == 3)
		(*p)[*p_len - 1] &= 0xfe;
	else if (*which == 4)
		(*p)[*p_len - 1] -= 2;
	else if (*which == 6)
		*p_len = 0;
	else if (*which == 7)
		return finish_transport_group(out, p_bytes, p_length, g_bytes, g_length, 1);
	else if (*which == 8) {
		*g = *p;
		*g_len = *p_len;
	}
	else if (*which == 9)
		(*g)[0] = 5;
	return finish_transport_group(out, p_bytes, p_length, g_bytes, g_length, 0);
}

static int
test_dhgex_group_selection(void)
{
	static const struct {
		uint32_t min, preferred, max;
		size_t bytes;
	} cases[] = {
		{ 2048, 2048, 8192, 256 },
		{ 2048, 3100, 8192, 512 },
		{ 2048, 5000, 8192, 768 },
		{ 2048, 8000, 8100, 768 },
		{ 2048, 4096, 8192, 512 },
		{ 2048, 8192, 8192, 1024 },
		{ 3072, 3072, 3072, 384 },
		{ 6144, 6144, 6144, 768 },
	};
	for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
		struct dssh_dh_gex_group_result group = {0};
		dssh_test_alloc_fail_after(0);
		int ret = default_select_group(cases[i].min, cases[i].preferred, cases[i].max, &group, NULL);
		int allocations = dssh_test_alloc_count();
		dssh_test_alloc_reset();
		bool generator_ok = group.g.len == 1 && group.g.data != NULL && *group.g.data == 2;
		size_t p_len = group.p.len;
		bool lease_ok = group.lease.release != NULL && group.lease.owner == NULL;
		dssh_result_release(&group.lease);
		dssh_result_release(&group.lease);
		ASSERT_OK(ret);
		ASSERT_EQ(allocations, 0);
		ASSERT_EQ_U(p_len, cases[i].bytes);
		ASSERT_TRUE(generator_ok && lease_ok);
	}
	struct dssh_dh_gex_group_result invalid = {0};
	ASSERT_ERR(default_select_group(4096, 2048, 8192, &invalid, NULL), DSSH_ERROR_INVALID);
	ASSERT_ERR(default_select_group(2048, 8192, 4096, &invalid, NULL), DSSH_ERROR_INVALID);
	ASSERT_ERR(default_select_group(1, 512, 1024, &invalid, NULL), DSSH_ERROR_INIT);
	ASSERT_ERR(default_select_group(2048, 4096, 8192, NULL, NULL), DSSH_ERROR_INVALID);
	ASSERT_NULL(invalid.p.data);
	ASSERT_NULL(invalid.g.data);
	ASSERT_NULL(invalid.lease.release);
	invalid.p.data = rfc3526_grp14_p;
	ASSERT_ERR(default_select_group(2048, 4096, 8192, &invalid, NULL), DSSH_ERROR_INVALID);
	ASSERT_TRUE(invalid.p.data == rfc3526_grp14_p);

	return TEST_PASS;
}

static int
test_dhgex_group_validation(void)
{
	struct dhgex_server_ctx ctx;
	if (dhgex_server_setup(&ctx) < 0)
		return TEST_FAIL;
	void *original = ctx.server->trans.kex_selected->config;
	unsigned which = 0;
	struct dssh_dh_gex_provider provider = { release_group_provider, &which };
	ctx.server->trans.kex_selected->config = &provider;
	uint8_t request[] = { 34, 0, 0, 8, 0, 0, 0, 8, 0, 0, 0, 8, 0 };
	uint8_t init[] = { 32, 0, 0, 0, 1, 2 };
	uint8_t wire[64];
	size_t len = 0;
	ASSERT_EQ(build_plaintext_packet_t(request, sizeof(request), wire, sizeof(wire), &len), 0);
	ASSERT_EQ(build_plaintext_packet_t(init, sizeof(init), wire, sizeof(wire), &len), 0);
	int results[10];
	for (which = 0; which < 10; which++)
		ASSERT_EQ(dhgex_server_run(&ctx, wire, len, &results[which]), 0);
	ctx.server->trans.kex_selected->config = original;
	bool complete = ctx.server->trans.kex_result.shared_secret.len > 0
	    && ctx.server->trans.kex_result.exchange_hash.len == 32;
	dhgex_server_teardown(&ctx);
	for (size_t i = 0; i < 9; i++)
		ASSERT_EQ(results[i], DSSH_ERROR_INVALID);
	ASSERT_OK(results[9]);
	ASSERT_TRUE(complete);
	return TEST_PASS;
}

static int
test_dhgex_peer_validation(void)
{
	struct dhgex_server_ctx ctx;
	if (dhgex_server_setup(&ctx) < 0)
		return TEST_FAIL;
	uint8_t request[] = { 34, 0, 0, 8, 0, 0, 0, 8, 0, 0, 0, 8, 0 };
	uint8_t init[] = { 32, 0, 0, 0, 1, 1 };
	uint8_t wire[64];
	size_t len = 0;
	ASSERT_EQ(build_plaintext_packet_t(request, sizeof(request), wire, sizeof(wire), &len), 0);
	ASSERT_EQ(build_plaintext_packet_t(init, sizeof(init), wire, sizeof(wire), &len), 0);
	int ret = DSSH_ERROR_INIT;
	ASSERT_EQ(dhgex_server_run(&ctx, wire, len, &ret), 0);
	bool absent = ctx.server->trans.kex_result.shared_secret.data == NULL;
	dhgex_server_teardown(&ctx);
	ASSERT_TRUE(ret < 0);
	ASSERT_TRUE(absent);
	return TEST_PASS;
}

/* RFC 9941 Appendix A publishes component secrets and encoded K, but no
 * private keys/seeds. This exercises the actual combiner and KDF wire encoder;
 * it is not a complete KEM/X25519 key-generation or exchange-hash vector. */
static bool
hybrid_rfc9941_vector(void)
{
	static const uint8_t pq[32] = {
		0x2c, 0x0c, 0x5a, 0x36, 0xe6, 0x77, 0x70, 0xb4,
		0xd8, 0xab, 0x38, 0x9a, 0x92, 0x96, 0x3a, 0xcd,
		0x10, 0x82, 0x38, 0x36, 0x40, 0xbe, 0x2d, 0x66,
		0x08, 0x02, 0xb8, 0x17, 0xcf, 0xeb, 0xb9, 0xbe,
	};
	static const uint8_t classical[32] = {
		0x9b, 0x73, 0x7d, 0x41, 0xd6, 0xcf, 0xbb, 0x12,
		0x56, 0xc5, 0x8c, 0xad, 0x0a, 0x6a, 0xe2, 0xc9,
		0xbf, 0x84, 0xa9, 0x0a, 0x72, 0x91, 0xeb, 0x52,
		0xe4, 0xc1, 0x81, 0xc8, 0xd2, 0x44, 0x7b, 0x56,
	};
	static const uint8_t encoded[68] = {
		0x00, 0x00, 0x00, 0x40, 0x42, 0x54, 0x58, 0x44,
		0x6f, 0x22, 0x75, 0x63, 0x04, 0xde, 0xd7, 0x5a,
		0x1f, 0x23, 0xfe, 0xf9, 0xb1, 0x8b, 0x36, 0xeb,
		0xe0, 0xe6, 0xe2, 0x60, 0xc3, 0x00, 0x12, 0x63,
		0xb0, 0x18, 0x3f, 0x42, 0x49, 0x07, 0xe6, 0xd8,
		0x22, 0xb3, 0xb7, 0x6c, 0x6c, 0x38, 0x37, 0xb5,
		0xb4, 0x1f, 0xb0, 0xd0, 0x76, 0x35, 0xc7, 0x57,
		0xe6, 0x5e, 0xfb, 0xef, 0xcb, 0x5b, 0xc3, 0x8a,
		0x1a, 0x15, 0xa9, 0x6d,
	};
	struct hybrid_pq_params params = { .hash_name = "SHA-512", .digest_len = 64, .pq_ss_len = 32 };
	dssh_hash_ctx *hash = NULL;
	size_t length = 0;
	uint8_t digest[66];
	memset(digest, 0x5a, sizeof(digest));
	int ret = dssh_hash_init(&hash, params.hash_name, &length);
	bool valid = ret == 0 && length == params.digest_len;
	if (valid) {
		dssh_test_alloc_fail_after(0);
		ret = hybrid_pq_compute_shared_secret(hash, &params, pq, classical, &digest[1], params.digest_len);
		int allocations = dssh_test_alloc_count();
		dssh_test_alloc_reset();
		valid = ret == 0 && allocations == 0 && digest[0] == 0x5a && digest[65] == 0x5a
		    && memcmp(&digest[1], &encoded[4], params.digest_len) == 0;
	}
	dssh_hash_free(hash);
	uint8_t *wire = NULL;
	size_t wire_length = 0;
	if (valid) {
		ret = encode_k_wire(&digest[1], params.digest_len, true, &wire, &wire_length);
		valid = ret == 0 && wire != NULL && wire_length == sizeof(encoded)
		    && memcmp(wire, encoded, sizeof(encoded)) == 0;
	}
	dssh_cleanse(wire, wire_length);
	free(wire);
	dssh_cleanse(digest, sizeof(digest));
	printf("hybrid_rfc9941 digest=64 encoded=68 valid=%d\n", valid);
	return valid;
}

static int
test_hybrid_shared_secret_validation(void)
{
	ASSERT_TRUE(hybrid_rfc9941_vector());
	static const uint8_t sha256[] = {
		0xad, 0x21, 0x1f, 0x23, 0xbb, 0x28, 0xb3, 0x62, 0x5e, 0xac, 0x51, 0xe5,
		0x9d, 0x4e, 0x10, 0xe5, 0x13, 0x9c, 0xfe, 0xde, 0x42, 0xed, 0xe3, 0x78,
		0x8e, 0x4d, 0xce, 0x0e, 0xcb, 0x41, 0x06, 0xfa,
	};
	static const uint8_t sha512[] = {
		0xc4, 0xe4, 0xd9, 0xe2, 0xf9, 0xc4, 0x08, 0x50, 0xe5, 0xd9, 0xe7, 0xa1,
		0x7d, 0x56, 0x17, 0xbe, 0xc4, 0x59, 0x14, 0x38, 0x4c, 0x2b, 0x9f, 0xd1,
		0x4b, 0x68, 0xef, 0x31, 0x08, 0xc6, 0xac, 0x08, 0xd1, 0x0e, 0x9f, 0x73,
		0x52, 0x6a, 0x41, 0x4e, 0x13, 0xc9, 0x3f, 0xcc, 0x17, 0xbe, 0x23, 0xa2,
		0x27, 0x61, 0xe4, 0x6b, 0x7d, 0x8a, 0xe7, 0xff, 0xe9, 0x0e, 0xdb, 0x6f,
		0x98, 0xcc, 0x23, 0x75,
	};

	const uint8_t *expected[] = { sha256, sha512 };
	uint8_t pq[32], classical[32];
	memset(pq, 0x11, sizeof(pq));
	memset(classical, 0x22, sizeof(classical));
	classical[0] = 0;
	for (size_t i = 0; i < 2; i++) {
		struct hybrid_pq_params params = {
			.hash_name = i == 0 ? "SHA-256" : "SHA-512",
			.digest_len = i == 0 ? 32 : 64,
			.pq_ss_len = 32,
		};
		dssh_hash_ctx *hash = NULL;
		size_t length = 0;
		ASSERT_OK(dssh_hash_init(&hash, params.hash_name, &length));
		uint8_t out[64];
		memset(out, 0x5a, sizeof(out));
		dssh_test_alloc_fail_after(0);
		int ret = hybrid_pq_compute_shared_secret(hash, &params, pq, classical, out, sizeof(out));
		int allocations = dssh_test_alloc_count();
		dssh_test_alloc_reset();
		dssh_hash_free(hash);
		bool matches = ret == 0 && allocations == 0 && length == params.digest_len
		    && memcmp(out, expected[i], length) == 0;
		for (size_t j = length; j < sizeof(out); j++)
			matches = matches && out[j] == 0x5a;
		dssh_cleanse(out, sizeof(out));
		ASSERT_TRUE(matches);
	}
	memset(classical, 0, sizeof(classical));
	struct hybrid_pq_params params = { .hash_name = "SHA-256", .digest_len = 32, .pq_ss_len = 32 };
	dssh_hash_ctx *hash = NULL;
	size_t length = 0;
	ASSERT_OK(dssh_hash_init(&hash, params.hash_name, &length));
	uint8_t out[64];
	memset(out, 0x5a, sizeof(out));
	int ret = hybrid_pq_compute_shared_secret(hash, &params, pq, classical, out, sizeof(out));
	int short_result = hybrid_pq_compute_shared_secret(hash, &params, pq, classical, out, 31);
	dssh_hash_free(hash);
	ASSERT_EQ(ret, DSSH_ERROR_INVALID);
	ASSERT_EQ(short_result, DSSH_ERROR_TOOLONG);
	for (size_t i = 0; i < sizeof(out); i++)
		ASSERT_EQ(out[i], 0x5a);
	return TEST_PASS;
}

static int
test_dhgex_mpint_validation(void)
{
	static const struct {
		uint8_t bytes[7];
		size_t len;
	} invalid[] = {
		{ { 0, 0, 0, 1, 0x80 }, 5 },
		{ { 0, 0, 0, 1, 0 }, 5 },
		{ { 0, 0, 0, 2, 0, 1 }, 6 },
		{ { 0, 0, 0, 2, 1 }, 5 },
		{ { 0, 0, 0 }, 3 },
	};
	const uint8_t *body;
	size_t body_len;
	for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++)
		ASSERT_EQ(dssh_dhgex_parse_mpint(invalid[i].bytes, invalid[i].len, &body, &body_len),
		    DSSH_ERROR_PARSE);
	uint8_t positive[] = { 0, 0, 0, 2, 0, 0x80 };
	ASSERT_EQ(dssh_dhgex_parse_mpint(positive, sizeof(positive), &body, &body_len), 6);
	ASSERT_EQ_U(body_len, 2);
	ASSERT_TRUE(body == positive + 4);
	return TEST_PASS;
}

#ifdef DSSH_CRYPTO_OPENSSL
static int
test_dhgex_group_failure_paths(void)
{
	const struct rfc3526_group *group = &rfc3526_groups[0];
	uint8_t generator = 5;
	BIGNUM *p = BN_bin2bn(group->p, 256, NULL);
	BIGNUM *g = BN_bin2bn(&generator, 1, NULL);
	BN_CTX *bnctx = BN_CTX_new();
	ASSERT_NOT_NULL(p);
	ASSERT_NOT_NULL(g);
	ASSERT_NOT_NULL(bnctx);
	int ret = DSSH_ERROR_INIT;
	int calls = 0;
	for (int n = 0; n < 32; n++) {
		dssh_test_ossl_fail_after(n);
		ret = dhgex_validate_group(p, g, bnctx, group->p, group->p_len, &generator, 1);
		calls = dssh_test_ossl_count();
		dssh_test_ossl_reset();
		if (ret == 0)
			break;
	}
	BN_free(p);
	BN_free(g);
	BN_CTX_free(bnctx);
	ASSERT_OK(ret);
	ASSERT_TRUE(calls >= 5);
	return TEST_PASS;
}

static int
test_dhgex_group_constants(void)
{
	BIGNUM *(*get_prime[])(BIGNUM *) = {
		BN_get_rfc3526_prime_2048, BN_get_rfc3526_prime_3072,
		BN_get_rfc3526_prime_4096, BN_get_rfc3526_prime_6144,
		BN_get_rfc3526_prime_8192
	};
	for (size_t i = 0; i < RFC3526_NGROUPS; i++) {
		BIGNUM *p = get_prime[i](NULL);
		ASSERT_NOT_NULL(p);
		uint8_t bytes[1024];
		int len = BN_bn2bin(p, bytes);
		BN_free(p);
		ASSERT_TRUE(len > 0);
		size_t size = (size_t)len;
		ASSERT_EQ_U(size, rfc3526_groups[i].p_len);
		ASSERT_MEM_EQ(bytes, rfc3526_groups[i].p, size);
	}
	return TEST_PASS;
}
#endif

static int
dummy_sign(struct dssh_bytes_result *output_result, const uint8_t *d,
    size_t dl, dssh_key_algo_ctx *c)
{
	uint8_t *fixture_output = NULL;
	size_t fixture_length = 0;
	uint8_t **b = &fixture_output;
	size_t *ol = &fixture_length;

	(void)b; (void)ol; (void)d; (void)dl; (void)c;
	return test_auth_output_finish(output_result, fixture_output, fixture_length, 0);
}

static int
dummy_pubkey(const uint8_t **b, size_t *ol, dssh_key_algo_ctx *c)
{
	(void)b; (void)ol; (void)c;
	return 0;
}

/* DH-GEX server handler tests */
static int
test_dhgex_server_null_pubkey_fn(void)
{
	struct dhgex_server_ctx ctx;
	if (dhgex_server_setup(&ctx) < 0)
		return TEST_FAIL;

	/* Set key_algo to a stub with NULL pubkey */
	struct dssh_key_algo_s dummy = {0};
	dummy.sign = dummy_sign;
	dssh_key_algo saved = ctx.server->trans.key_algo_selected;
	ctx.server->trans.key_algo_selected = &dummy;

	int res = DSSH_ERROR_INIT;
	ASSERT_EQ(dhgex_server_run(&ctx, NULL, 0, &res), 0);

	/* Restore before teardown */
	ctx.server->trans.key_algo_selected = saved;
	dhgex_server_teardown(&ctx);
	ASSERT_EQ(res, DSSH_ERROR_INIT);
	return TEST_PASS;
}

static int
test_dhgex_server_null_sign_fn(void)
{
	struct dhgex_server_ctx ctx;
	if (dhgex_server_setup(&ctx) < 0)
		return TEST_FAIL;

	struct dssh_key_algo_s dummy = {0};
	dummy.pubkey = dummy_pubkey;
	dssh_key_algo saved = ctx.server->trans.key_algo_selected;
	ctx.server->trans.key_algo_selected = &dummy;

	int res = DSSH_ERROR_INIT;
	ASSERT_EQ(dhgex_server_run(&ctx, NULL, 0, &res), 0);

	ctx.server->trans.key_algo_selected = saved;
	dhgex_server_teardown(&ctx);
	ASSERT_EQ(res, DSSH_ERROR_INIT);
	return TEST_PASS;
}

static int
test_dhgex_server_recv_fail(void)
{
	struct dhgex_server_ctx ctx;
	if (dhgex_server_setup(&ctx) < 0)
		return TEST_FAIL;

	/* No packets injected -- recv will fail immediately (pipe closed) */
	int res = DSSH_ERROR_INIT;
	ASSERT_EQ(dhgex_server_run(&ctx, NULL, 0, &res), 0);

	dhgex_server_teardown(&ctx);
	ASSERT_TRUE(res < 0);
	return TEST_PASS;
}

static int
test_dhgex_server_bad_request_type(void)
{
	struct dhgex_server_ctx ctx;
	if (dhgex_server_setup(&ctx) < 0)
		return TEST_FAIL;

	/* Inject a packet with wrong msg_type (GEX_INIT instead of GEX_REQUEST) */
	uint8_t pkt[16];
	size_t pp = 0;
	pkt[pp++] = 32; /* GEX_INIT, not GEX_REQUEST(34) */
	ASSERT_EQ(dssh_serialize_uint32(1, pkt, sizeof(pkt), &pp), 0);
	pkt[pp++] = 0x02;
	uint8_t wire[64];
	size_t wlen = 0;
	ASSERT_EQ(build_plaintext_packet_t(pkt, pp, wire, sizeof(wire), &wlen), 0);

	int res = DSSH_ERROR_INIT;
	ASSERT_EQ(dhgex_server_run(&ctx, wire, wlen, &res), 0);

	dhgex_server_teardown(&ctx);
	ASSERT_EQ(res, DSSH_ERROR_PARSE);
	return TEST_PASS;
}

static int
test_dhgex_server_short_request(void)
{
	struct dhgex_server_ctx ctx;
	if (dhgex_server_setup(&ctx) < 0)
		return TEST_FAIL;

	/* Inject a GEX_REQUEST with only 5 bytes (need 1+12=13) */
	uint8_t pkt[5] = { 34, 0, 0, 8, 0 };
	uint8_t wire[64];
	size_t wlen = 0;
	ASSERT_EQ(build_plaintext_packet_t(pkt, sizeof(pkt), wire, sizeof(wire), &wlen), 0);

	int res = DSSH_ERROR_INIT;
	ASSERT_EQ(dhgex_server_run(&ctx, wire, wlen, &res), 0);

	dhgex_server_teardown(&ctx);
	ASSERT_EQ(res, DSSH_ERROR_PARSE);
	return TEST_PASS;
}

static int
test_dhgex_server_null_provider(void)
{
	struct dhgex_server_ctx ctx;
	if (dhgex_server_setup(&ctx) < 0)
		return TEST_FAIL;

	/* Build a valid GEX_REQUEST */
	uint8_t req[16];
	size_t rp = 0;
	req[rp++] = 34;
	ASSERT_EQ(dssh_serialize_uint32(2048, req, sizeof(req), &rp), 0);
	ASSERT_EQ(dssh_serialize_uint32(4096, req, sizeof(req), &rp), 0);
	ASSERT_EQ(dssh_serialize_uint32(8192, req, sizeof(req), &rp), 0);
	uint8_t wire[64];
	size_t wlen = 0;
	ASSERT_EQ(build_plaintext_packet_t(req, rp, wire, sizeof(wire), &wlen), 0);

	/* Set provider with NULL select_group */
	struct dssh_dh_gex_provider bad_prov = { .select_group = NULL };
	void *saved = ctx.server->trans.kex_selected->config;
	ctx.server->trans.kex_selected->config = &bad_prov;

	int res = DSSH_ERROR_INIT;
	ASSERT_EQ(dhgex_server_run(&ctx, wire, wlen, &res), 0);

	ctx.server->trans.kex_selected->config = saved;
	dhgex_server_teardown(&ctx);
	ASSERT_EQ(res, DSSH_ERROR_INIT);
	return TEST_PASS;
}

static int
provider_error_cb(uint32_t min, uint32_t preferred, uint32_t max,
    struct dssh_dh_gex_group_result *out, void *cbdata)
{
	(void)min;
	(void)preferred;
	(void)max;
	(void)out;
	(void)cbdata;
	return DSSH_ERROR_INIT;
}

static int
test_dhgex_server_provider_error(void)
{
	struct dhgex_server_ctx ctx;
	if (dhgex_server_setup(&ctx) < 0)
		return TEST_FAIL;

	uint8_t req[16];
	size_t rp = 0;
	req[rp++] = 34;
	ASSERT_EQ(dssh_serialize_uint32(2048, req, sizeof(req), &rp), 0);
	ASSERT_EQ(dssh_serialize_uint32(4096, req, sizeof(req), &rp), 0);
	ASSERT_EQ(dssh_serialize_uint32(8192, req, sizeof(req), &rp), 0);
	uint8_t wire[64];
	size_t wlen = 0;
	ASSERT_EQ(build_plaintext_packet_t(req, rp, wire, sizeof(wire), &wlen), 0);

	struct dssh_dh_gex_provider err_prov = {
		.select_group = provider_error_cb
	};
	void *saved = ctx.server->trans.kex_selected->config;
	ctx.server->trans.kex_selected->config = &err_prov;

	int res = DSSH_ERROR_INIT;
	ASSERT_EQ(dhgex_server_run(&ctx, wire, wlen, &res), 0);

	ctx.server->trans.kex_selected->config = saved;
	dhgex_server_teardown(&ctx);
	ASSERT_EQ(res, DSSH_ERROR_INIT);
	return TEST_PASS;
}

static int
test_dhgex_server_bad_init_type(void)
{
	struct dhgex_server_ctx ctx;
	if (dhgex_server_setup(&ctx) < 0)
		return TEST_FAIL;

	/* GEX_REQUEST (valid) + wrong msg_type for INIT */
	uint8_t wire[256];
	size_t wlen = 0;

	uint8_t req[16];
	size_t rp = 0;
	req[rp++] = 34;
	ASSERT_EQ(dssh_serialize_uint32(2048, req, sizeof(req), &rp), 0);
	ASSERT_EQ(dssh_serialize_uint32(4096, req, sizeof(req), &rp), 0);
	ASSERT_EQ(dssh_serialize_uint32(8192, req, sizeof(req), &rp), 0);
	ASSERT_EQ(build_plaintext_packet_t(req, rp, wire, sizeof(wire), &wlen), 0);

	/* Wrong type: GEX_REQUEST(34) instead of GEX_INIT(32) */
	uint8_t bad[16];
	size_t bp = 0;
	bad[bp++] = 34; /* wrong: should be 32 */
	ASSERT_EQ(dssh_serialize_uint32(1, bad, sizeof(bad), &bp), 0);
	bad[bp++] = 0x02;
	ASSERT_EQ(build_plaintext_packet_t(bad, bp, wire, sizeof(wire), &wlen), 0);

	int res = DSSH_ERROR_INIT;
	ASSERT_EQ(dhgex_server_run(&ctx, wire, wlen, &res), 0);

	dhgex_server_teardown(&ctx);
	ASSERT_TRUE(res < 0);
	return TEST_PASS;
}

static int
test_dhgex_server_e_zero(void)
{
	struct dhgex_server_ctx ctx;
	if (dhgex_server_setup(&ctx) < 0)
		return TEST_FAIL;

	uint8_t wire[256];
	size_t wlen = 0;

	/* Valid GEX_REQUEST */
	uint8_t req[16];
	size_t rp = 0;
	req[rp++] = 34;
	ASSERT_EQ(dssh_serialize_uint32(2048, req, sizeof(req), &rp), 0);
	ASSERT_EQ(dssh_serialize_uint32(4096, req, sizeof(req), &rp), 0);
	ASSERT_EQ(dssh_serialize_uint32(8192, req, sizeof(req), &rp), 0);
	ASSERT_EQ(build_plaintext_packet_t(req, rp, wire, sizeof(wire), &wlen), 0);

	/* GEX_INIT with e=0 (invalid: not in [1, p-1]) */
	uint8_t init[16];
	size_t ip = 0;
	init[ip++] = 32; /* GEX_INIT */
	ASSERT_EQ(dssh_serialize_uint32(0, init, sizeof(init), &ip), 0); /* mpint len=0 -> value 0 */
	ASSERT_EQ(build_plaintext_packet_t(init, ip, wire, sizeof(wire), &wlen), 0);

	int res = DSSH_ERROR_INIT;
	ASSERT_EQ(dhgex_server_run(&ctx, wire, wlen, &res), 0);

	dhgex_server_teardown(&ctx);
	ASSERT_TRUE(res < 0);
	return TEST_PASS;
}

static int
test_dhgex_server_recv_init_fail(void)
{
	struct dhgex_server_ctx ctx;
	if (dhgex_server_setup(&ctx) < 0)
		return TEST_FAIL;

	/* Only inject GEX_REQUEST -- no GEX_INIT. Server sends GROUP
	 * then tries to recv INIT from a closed/empty pipe -> fail. */
	uint8_t req[16];
	size_t rp = 0;
	req[rp++] = 34;
	ASSERT_EQ(dssh_serialize_uint32(2048, req, sizeof(req), &rp), 0);
	ASSERT_EQ(dssh_serialize_uint32(4096, req, sizeof(req), &rp), 0);
	ASSERT_EQ(dssh_serialize_uint32(8192, req, sizeof(req), &rp), 0);
	uint8_t wire[64];
	size_t wlen = 0;
	ASSERT_EQ(build_plaintext_packet_t(req, rp, wire, sizeof(wire), &wlen), 0);

	int res = DSSH_ERROR_INIT;
	ASSERT_EQ(dhgex_server_run(&ctx, wire, wlen, &res), 0);

	dhgex_server_teardown(&ctx);
	ASSERT_TRUE(res < 0);
	return TEST_PASS;
}

static int
test_dhgex_server_ka_null(void)
{
	struct dhgex_server_ctx ctx;
	if (dhgex_server_setup(&ctx) < 0)
		return TEST_FAIL;

	/* Clear key_algo_selected entirely */
	dssh_key_algo saved = ctx.server->trans.key_algo_selected;
	ctx.server->trans.key_algo_selected = NULL;

	int res = DSSH_ERROR_INIT;
	ASSERT_EQ(dhgex_server_run(&ctx, NULL, 0, &res), 0);

	ctx.server->trans.key_algo_selected = saved;
	dhgex_server_teardown(&ctx);
	ASSERT_EQ(res, DSSH_ERROR_INIT);
	return TEST_PASS;
}
/* end DH-GEX server handler tests */

/* ================================================================
 * Curve25519 server handler targeted tests
 *
 * Equivalent of dhgex_server_* but for the curve25519-sha256 KEX.
 * Uses the same pattern: set up a server with negotiated state,
 * then inject specific packets to exercise error paths.
 * ================================================================ */

struct c25519_server_ctx {
	struct test_session_pair pair;
	dssh_session server;
};

static int
c25519_server_setup(struct c25519_server_ctx *ctx)
{
	ctx->server = NULL;
	int ret = direct_kex_setup(&ctx->pair, false);
	if (ret == 0)
		ctx->server = ctx->pair.server;
	return ret;
}

static void
c25519_server_teardown(struct c25519_server_ctx *ctx)
{
	ctx->server = NULL;
	direct_kex_dispose(&ctx->pair);
}

static int
c25519_server_run(struct c25519_server_ctx *ctx,
    const uint8_t *wire, size_t wire_len, int *operation)
{
	return direct_kex_run(&ctx->pair, wire, wire_len, -1, operation);
}

static int
test_c25519_server_ka_null(void)
{
	struct c25519_server_ctx ctx;
	if (c25519_server_setup(&ctx) < 0)
		return TEST_FAIL;

	/* Clear key_algo_selected entirely */
	dssh_key_algo saved = ctx.server->trans.key_algo_selected;
	ctx.server->trans.key_algo_selected = NULL;

	int res = DSSH_ERROR_INIT;
	ASSERT_EQ(c25519_server_run(&ctx, NULL, 0, &res), 0);

	ctx.server->trans.key_algo_selected = saved;
	c25519_server_teardown(&ctx);
	ASSERT_EQ(res, DSSH_ERROR_INIT);
	return TEST_PASS;
}

static int
test_c25519_server_null_pubkey_fn(void)
{
	struct c25519_server_ctx ctx;
	if (c25519_server_setup(&ctx) < 0)
		return TEST_FAIL;

	/* Set key_algo to a stub with NULL pubkey */
	struct dssh_key_algo_s dummy = {0};
	dummy.sign = dummy_sign;
	dssh_key_algo saved = ctx.server->trans.key_algo_selected;
	ctx.server->trans.key_algo_selected = &dummy;

	int res = DSSH_ERROR_INIT;
	ASSERT_EQ(c25519_server_run(&ctx, NULL, 0, &res), 0);

	/* Restore before teardown */
	ctx.server->trans.key_algo_selected = saved;
	c25519_server_teardown(&ctx);
	ASSERT_EQ(res, DSSH_ERROR_INIT);
	return TEST_PASS;
}

static int
test_c25519_server_null_sign_fn(void)
{
	struct c25519_server_ctx ctx;
	if (c25519_server_setup(&ctx) < 0)
		return TEST_FAIL;

	struct dssh_key_algo_s dummy = {0};
	dummy.pubkey = dummy_pubkey;
	dssh_key_algo saved = ctx.server->trans.key_algo_selected;
	ctx.server->trans.key_algo_selected = &dummy;

	int res = DSSH_ERROR_INIT;
	ASSERT_EQ(c25519_server_run(&ctx, NULL, 0, &res), 0);

	ctx.server->trans.key_algo_selected = saved;
	c25519_server_teardown(&ctx);
	ASSERT_EQ(res, DSSH_ERROR_INIT);
	return TEST_PASS;
}

static int
test_c25519_server_recv_fail(void)
{
	struct c25519_server_ctx ctx;
	if (c25519_server_setup(&ctx) < 0)
		return TEST_FAIL;

	/* No packets injected -- recv will fail immediately (pipe closed) */
	int res = DSSH_ERROR_INIT;
	ASSERT_EQ(c25519_server_run(&ctx, NULL, 0, &res), 0);

	c25519_server_teardown(&ctx);
	ASSERT_TRUE(res < 0);
	return TEST_PASS;
}

static int
test_c25519_server_bad_init_type(void)
{
	struct c25519_server_ctx ctx;
	if (c25519_server_setup(&ctx) < 0)
		return TEST_FAIL;

	/* Inject a packet with wrong msg_type (not ECDH_INIT=30) */
	uint8_t pkt[8];
	size_t pp = 0;
	pkt[pp++] = 31; /* ECDH_REPLY, not ECDH_INIT(30) */
	ASSERT_EQ(dssh_serialize_uint32(32, pkt, sizeof(pkt), &pp), 0);
	uint8_t wire[64];
	size_t wlen = 0;
	ASSERT_EQ(build_plaintext_packet_t(pkt, pp, wire, sizeof(wire), &wlen), 0);

	int res = DSSH_ERROR_INIT;
	ASSERT_EQ(c25519_server_run(&ctx, wire, wlen, &res), 0);

	c25519_server_teardown(&ctx);
	ASSERT_EQ(res, DSSH_ERROR_PARSE);
	return TEST_PASS;
}

static int
test_c25519_server_bad_qc_len(void)
{
	struct c25519_server_ctx ctx;
	if (c25519_server_setup(&ctx) < 0)
		return TEST_FAIL;

	/* Inject ECDH_INIT with qc_len=16 instead of 32 */
	uint8_t pkt[32];
	size_t pp = 0;
	pkt[pp++] = 30; /* SSH_MSG_KEX_ECDH_INIT */
	ASSERT_EQ(dssh_serialize_uint32(16, pkt, sizeof(pkt), &pp), 0);
	memset(&pkt[pp], 0x42, 16);
	pp += 16;
	uint8_t wire[64];
	size_t wlen = 0;
	ASSERT_EQ(build_plaintext_packet_t(pkt, pp, wire, sizeof(wire), &wlen), 0);

	int res = DSSH_ERROR_INIT;
	ASSERT_EQ(c25519_server_run(&ctx, wire, wlen, &res), 0);

	c25519_server_teardown(&ctx);
	ASSERT_EQ(res, DSSH_ERROR_PARSE);
	return TEST_PASS;
}

/* Line 388:35: ECDH_INIT with qc_len=32 but only 10 bytes of Q_C data */
static int
test_c25519_server_qc_overrun(void)
{
	struct c25519_server_ctx ctx;
	if (c25519_server_setup(&ctx) < 0)
		return TEST_FAIL;

	uint8_t pkt[32];
	size_t pp = 0;
	pkt[pp++] = 30; /* SSH_MSG_KEX_ECDH_INIT */
	ASSERT_EQ(dssh_serialize_uint32(32, pkt, sizeof(pkt), &pp), 0); /* claims 32 bytes */
	memset(&pkt[pp], 0x42, 10); /* only 10 bytes present */
	pp += 10;
	uint8_t wire[64];
	size_t wlen = 0;
	ASSERT_EQ(build_plaintext_packet_t(pkt, pp, wire, sizeof(wire), &wlen), 0);

	int res = DSSH_ERROR_INIT;
	ASSERT_EQ(c25519_server_run(&ctx, wire, wlen, &res), 0);

	c25519_server_teardown(&ctx);
	ASSERT_TRUE(res < 0);
	return TEST_PASS;
}

/* ================================================================
 * Curve25519 helper function tests
 * ================================================================ */

static int
test_c25519_encode_shared_secret_leading_zeros(void)
{
	const uint8_t raw[] = {0, 0, 0x42};
	const uint8_t expected[] = {0, 0, 0, 1, 0x42};
	struct dssh_bytes secret = {0};
	uint8_t mpint[37];
	size_t length = 0;
	ASSERT_OK(encode_shared_secret(raw, sizeof(raw), &secret, mpint, sizeof(mpint), &length));
	ASSERT_TRUE(secret.data == &raw[2] && secret.len == 1 && length == sizeof(expected));
	ASSERT_EQ(memcmp(mpint, expected, sizeof(expected)), 0);
	return TEST_PASS;
}

static int
test_c25519_secret_validation(void)
{
	uint8_t raw[33] = {0};
	struct dssh_bytes secret = {0};
	uint8_t mpint[37];
	size_t length = 0;
	ASSERT_EQ(encode_shared_secret(raw, 0, &secret, mpint, sizeof(mpint), &length), DSSH_ERROR_INVALID);
	ASSERT_EQ(encode_shared_secret(raw, 32, &secret, mpint, sizeof(mpint), &length), DSSH_ERROR_INVALID);
	ASSERT_EQ(encode_shared_secret(raw, sizeof(raw), &secret, mpint, sizeof(mpint), &length), DSSH_ERROR_INVALID);
	ASSERT_NULL(secret.data);
	ASSERT_EQ_U(secret.len, 0);
	ASSERT_EQ_U(length, 0);
	return TEST_PASS;
}

static int
test_c25519_peer_validation(void)
{
	struct c25519_server_ctx ctx;
	if (c25519_server_setup(&ctx) < 0)
		return TEST_FAIL;
	uint8_t pkt[37] = { SSH_MSG_KEX_ECDH_INIT, 0, 0, 0, 32 };
	uint8_t wire[64];
	size_t len = 0;
	ASSERT_EQ(build_plaintext_packet_t(pkt, sizeof(pkt), wire, sizeof(wire), &len), 0);
	int ret = DSSH_ERROR_INIT;
	ASSERT_EQ(c25519_server_run(&ctx, wire, len, &ret), 0);
	bool output_absent = ctx.server->trans.kex_result.shared_secret.data == NULL;
	c25519_server_teardown(&ctx);
	ASSERT_TRUE(ret < 0);
	ASSERT_TRUE(output_absent);
	return TEST_PASS;
}

static int
test_c25519_init_allocations(void)
{
	bool succeeded = false;
	for (int allocation = 0; allocation < 16; allocation++) {
		void *state = NULL;
		dssh_test_alloc_fail_after(allocation);
		int ret = curve25519_init(false, NULL, &state);
		dssh_test_alloc_reset();
		bool valid = ret == 0 ? state != NULL : ret == DSSH_ERROR_ALLOC && state == NULL;
		curve25519_cleanup(state);
		ASSERT_TRUE(valid);
		if (ret == 0) {
			succeeded = true;
			break;
		}
	}
	ASSERT_TRUE(succeeded);
	return TEST_PASS;
}

static int
test_c25519_encode_shared_secret_storage(void)
{
	/* Independently written wire prefixes; no serialize helper in the oracle. */
	const uint8_t raw[4][3] = {{0, 0, 0x7f}, {0, 0, 0x80}, {0, 1, 0x80}, {0x80, 0, 0}};
	const uint8_t expected[4][8] = {
		{0, 0, 0, 1, 0x7f}, {0, 0, 0, 2, 0, 0x80},
		{0, 0, 0, 2, 1, 0x80}, {0, 0, 0, 4, 0, 0x80, 0, 0},
	};
	const size_t lengths[] = {5, 6, 6, 8};
	const size_t secret_lengths[] = {1, 1, 2, 3};
	for (size_t i = 0; i < sizeof(raw) / sizeof(raw[0]); i++) {
		uint8_t encoded[40];
		memset(encoded, 0x5a, sizeof(encoded));
		struct dssh_bytes secret = {0};
		size_t length = 0;
		dssh_test_alloc_fail_after(0);
		int ret = encode_shared_secret(raw[i], sizeof(raw[i]), &secret,
		    &encoded[1], lengths[i], &length);
		int allocations = dssh_test_alloc_count();
		dssh_test_alloc_reset();
		ASSERT_OK(ret);
		ASSERT_EQ(allocations, 0);
		ASSERT_EQ_U(length, lengths[i]);
		ASSERT_TRUE(secret.len == secret_lengths[i] && secret.data == &raw[i][3 - secret_lengths[i]]);
		ASSERT_EQ(memcmp(&encoded[1], expected[i], length), 0);
		ASSERT_TRUE(encoded[0] == 0x5a && encoded[1 + length] == 0x5a);
		memset(encoded, 0x5a, sizeof(encoded));
		ret = encode_shared_secret(raw[i], sizeof(raw[i]), &secret,
		    &encoded[1], lengths[i] - 1, &length);
		ASSERT_EQ(ret, DSSH_ERROR_TOOLONG);
		ASSERT_NULL(secret.data);
		ASSERT_EQ_U(length, 0);
		for (size_t j = 0; j < sizeof(encoded); j++)
			ASSERT_EQ(encoded[j], 0x5a);
	}
	uint8_t maximum[32], encoded[39];
	memset(maximum, 0x80, sizeof(maximum));
	memset(encoded, 0x5a, sizeof(encoded));
	struct dssh_bytes secret = {0};
	size_t length = 0;
	ASSERT_OK(encode_shared_secret(maximum, sizeof(maximum), &secret, &encoded[1], 37, &length));
	const uint8_t prefix[] = {0, 0, 0, 33, 0};
	ASSERT_TRUE(length == 37 && secret.data == maximum && secret.len == sizeof(maximum));
	ASSERT_EQ(memcmp(&encoded[1], prefix, sizeof(prefix)), 0);
	ASSERT_EQ(memcmp(&encoded[6], maximum, sizeof(maximum)), 0);
	ASSERT_TRUE(encoded[0] == 0x5a && encoded[38] == 0x5a);
	ASSERT_EQ(encode_shared_secret(maximum, sizeof(maximum), &secret, &encoded[1], 36, &length),
	    DSSH_ERROR_TOOLONG);
	return TEST_PASS;
}

/* ================================================================
 * None algorithm module coverage -- call the no-op functions
 * directly to get 100% on comp/none.c, enc/none.c, mac/none.c.
 * ================================================================ */

/* ================================================================
 * aes256-ctr / hmac-sha2-256 edge cases -- NULL ctx, alloc failure
 * ================================================================ */

#ifdef DSSH_CRYPTO_OPENSSL /* OpenSSL alloc injection tests */
static int
test_aes256_ctr_alloc_fail(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_aes256_ctr(), 0);

	dssh_enc enc = gconf.enc_head;
	ASSERT_NOT_NULL(enc);

	uint8_t key[32], iv[16];
	memset(key, 0x42, sizeof(key));
	memset(iv, 0x00, sizeof(iv));

	/* Fail the first malloc (cbd struct) */
	dssh_test_alloc_fail_after(0);
	dssh_enc_ctx *ectx = NULL;
	int res = enc->init(key, iv, true, &ectx);
	dssh_test_alloc_reset();
	ASSERT_EQ(res, DSSH_ERROR_ALLOC);
	ASSERT_NULL(ectx);

	dssh_test_reset_global_config();
	return TEST_PASS;
}
#endif /* DSSH_CRYPTO_OPENSSL */

static int
test_hmac_sha2_256_null_ctx(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_hmac_sha2_256(), 0);

	dssh_mac mac = gconf.mac_head;
	ASSERT_NOT_NULL(mac);

	uint8_t buf[16] = {0};
	uint8_t out[32];
	ASSERT_EQ(mac->generate(buf, sizeof(buf), out, NULL), DSSH_ERROR_INIT);

	dssh_test_reset_global_config();
	return TEST_PASS;
}

#ifdef DSSH_CRYPTO_OPENSSL
static int
test_hmac_sha2_256_alloc_fail(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_hmac_sha2_256(), 0);

	dssh_mac mac = gconf.mac_head;
	ASSERT_NOT_NULL(mac);

	uint8_t key[32];
	memset(key, 0x42, sizeof(key));

	/* Fail the first calloc (cbd struct) */
	dssh_test_alloc_fail_after(0);
	dssh_mac_ctx *mctx = NULL;
	int res = mac->init(key, &mctx);
	dssh_test_alloc_reset();
	ASSERT_EQ(res, DSSH_ERROR_ALLOC);
	ASSERT_NULL(mctx);

	dssh_test_reset_global_config();
	return TEST_PASS;
}
#endif /* DSSH_CRYPTO_OPENSSL */

/* ================================================================
 * None algorithm module coverage
 * ================================================================ */

static int
test_none_comp(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_none_comp(), 0);

	dssh_comp comp = gconf.comp_head;
	ASSERT_NOT_NULL(comp);

	uint8_t buf[16] = {0};
	uint8_t output[16];
	size_t bufsz = 0;
	dssh_comp_ctx *ctx = NULL;
	ASSERT_OK(comp->init(true, &ctx));
	ASSERT_OK(comp->bound(sizeof(buf), &bufsz, ctx));
	ASSERT_EQ_U(bufsz, sizeof(buf));
	ASSERT_OK(comp->compress(buf, sizeof(buf), output, sizeof(output), &bufsz, ctx));
	ASSERT_EQ_U(bufsz, sizeof(buf));
	ASSERT_MEM_EQ(output, buf, sizeof(buf));
	ASSERT_OK(comp->uncompress(output, sizeof(output), buf, sizeof(buf), &bufsz, ctx));
	ASSERT_EQ_U(bufsz, sizeof(buf));
	comp->cleanup(ctx);

	dssh_test_reset_global_config();
	return TEST_PASS;
}

static int
test_none_enc(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_none_enc(), 0);

	dssh_enc enc = gconf.enc_head;
	ASSERT_NOT_NULL(enc);

	uint8_t buf[16] = {0};
	ASSERT_EQ(enc->encrypt(buf, sizeof(buf), NULL), 0);
	ASSERT_EQ(enc->decrypt(buf, sizeof(buf), NULL), 0);

	dssh_test_reset_global_config();
	return TEST_PASS;
}

static int
test_none_mac(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_none_mac(), 0);

	dssh_mac mac = gconf.mac_head;
	ASSERT_NOT_NULL(mac);

	uint8_t buf[16] = {0};
	uint8_t out[1];
	ASSERT_EQ(mac->generate(buf, sizeof(buf), out, NULL), 0);

	dssh_test_reset_global_config();
	return TEST_PASS;
}

/* ================================================================
 * Coverage: DEBUG with msg_len > actual payload (line 754 clamp)
 * ================================================================ */

static int
test_debug_msg_len_exceeds_payload(void)
{
	dssh_test_reset_global_config();
	ASSERT_OK(register_all_algorithms());
	if (test_generate_host_key() < 0)
		return TEST_FAIL;
	dssh_transport_set_callbacks(mock_tx_dispatch, mock_rx_dispatch,
	    mock_rxline_dispatch, mock_extra_line_cb);

	struct mock_io_state io;
	ASSERT_OK(mock_io_init(&io, 0));

	dssh_session client = dssh_session_init(true, 0);
	ASSERT_NOT_NULL(client);
	dssh_session_set_cbdata(client, &io, &io, &io, &io);

	dssh_session server = init_server_session();
	ASSERT_NOT_NULL(server);
	dssh_session_set_cbdata(server, &io, &io, &io, &io);

	debug_cb_invoked = false;
	dssh_session_set_debug_cb(server, mock_debug_cb_track, NULL);

	/* DEBUG: always_display(1) + msg_len=100(4) but no msg data */
	uint8_t dbg[6];
	dbg[0] = SSH_MSG_DEBUG;
	dbg[1] = 1; /* always_display */
	size_t dp = 2;
	dssh_serialize_uint32(100, dbg, sizeof(dbg), &dp); /* msg_len=100 */
	ASSERT_OK(send_packet(client, dbg, sizeof(dbg), NULL));

	uint8_t msg_type;
	uint8_t *payload;
	size_t payload_len;
	ASSERT_EQ(recv_packet(server, &msg_type, &payload, &payload_len), DSSH_ERROR_PARSE);

	ASSERT_FALSE(debug_cb_invoked);
	ASSERT_TRUE(dssh_session_is_terminated(server));

	dssh_session_cleanup(server);
	dssh_session_cleanup(client);
	mock_io_free(&io);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

/* ================================================================
 * Coverage: GLOBAL_REQUEST with name_len > payload (line 781)
 * ================================================================ */

static int
test_global_request_name_exceeds_payload(void)
{
	dssh_test_reset_global_config();
	ASSERT_OK(register_all_algorithms());
	if (test_generate_host_key() < 0)
		return TEST_FAIL;
	dssh_transport_set_callbacks(mock_tx_dispatch, mock_rx_dispatch,
	    mock_rxline_dispatch, mock_extra_line_cb);

	struct mock_io_state io;
	ASSERT_OK(mock_io_init(&io, 0));

	dssh_session client = dssh_session_init(true, 0);
	ASSERT_NOT_NULL(client);
	dssh_session_set_cbdata(client, &io, &io, &io, &io);

	dssh_session server = init_server_session();
	ASSERT_NOT_NULL(server);
	dssh_session_set_cbdata(server, &io, &io, &io, &io);

	/* GLOBAL_REQUEST: name_len=100 but only 2 bytes of name + no want_reply.
	 * Malformed GLOBAL_REQUEST disconnects. */
	uint8_t gr[16];
	size_t gp = 0;
	gr[gp++] = 80; /* SSH_MSG_GLOBAL_REQUEST */
	dssh_serialize_uint32(100, gr, sizeof(gr), &gp); /* name_len=100 */
	gr[gp++] = 'a';
	gr[gp++] = 'b';
	/* Missing: rest of name + want_reply byte */
	ASSERT_OK(send_packet(client, gr, gp, NULL));

	uint8_t msg_type;
	uint8_t *payload;
	size_t payload_len;
	ASSERT_TRUE(recv_packet(server, &msg_type, &payload, &payload_len) < 0);

	dssh_session_cleanup(server);
	dssh_session_cleanup(client);
	mock_io_free(&io);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

/* ================================================================
 * Formerly-guarded paths -- now live code, need unit tests
 * ================================================================ */

static int
test_ed25519_sign_basic(void)
{
	struct test_signature_copies fixture_copies = {0};

	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_ssh_ed25519(), 0);
	ASSERT_EQ(dssh_ed25519_generate_key(), 0);

	dssh_key_algo ka = find_key_algo("ssh-ed25519");
	ASSERT_NOT_NULL(ka);

	const uint8_t data[] = "test";
	uint8_t *out = NULL;
	size_t outlen;
	ASSERT_EQ(test_signature_copy(&fixture_copies, ka->sign, &out, &outlen, data, sizeof(data) - 1,
	    ka->ctx), 0);
	ASSERT_NOT_NULL(out);
	ASSERT_TRUE(outlen > 0);
	test_signature_copy_free(&fixture_copies, out);

	dssh_test_reset_global_config();
	return test_signature_copies_finish(&fixture_copies, TEST_PASS);
}

static int
test_ed25519_pubkey_basic(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_ssh_ed25519(), 0);
	ASSERT_EQ(dssh_ed25519_generate_key(), 0);

	dssh_key_algo ka = find_key_algo("ssh-ed25519");
	ASSERT_NOT_NULL(ka);

	const uint8_t *out = NULL;
	size_t outlen;
	ASSERT_EQ(ka->pubkey(&out, &outlen, ka->ctx), 0);
	ASSERT_NOT_NULL(out);
	ASSERT_TRUE(outlen > 0);

	dssh_test_reset_global_config();
	return TEST_PASS;
}

static int
test_rsa_sign_basic(void)
{
	struct test_signature_copies fixture_copies = {0};

	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_rsa_sha2_256(), 0);
	ASSERT_EQ(dssh_rsa_sha2_256_generate_key(2048), 0);

	dssh_key_algo ka = find_key_algo("rsa-sha2-256");
	ASSERT_NOT_NULL(ka);

	const uint8_t data[] = "test";
	uint8_t *out = NULL;
	size_t outlen;
	ASSERT_EQ(test_signature_copy(&fixture_copies, ka->sign, &out, &outlen, data, sizeof(data) - 1,
	    ka->ctx), 0);
	ASSERT_NOT_NULL(out);
	ASSERT_TRUE(outlen > 0);
	test_signature_copy_free(&fixture_copies, out);

	dssh_test_reset_global_config();
	return test_signature_copies_finish(&fixture_copies, TEST_PASS);
}

static int
test_rsa_pubkey_basic(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_rsa_sha2_256(), 0);
	ASSERT_EQ(dssh_rsa_sha2_256_generate_key(2048), 0);

	dssh_key_algo ka = find_key_algo("rsa-sha2-256");
	ASSERT_NOT_NULL(ka);

	const uint8_t *out = NULL;
	size_t outlen;
	ASSERT_EQ(ka->pubkey(&out, &outlen, ka->ctx), 0);
	ASSERT_NOT_NULL(out);
	ASSERT_TRUE(outlen > 0);

	dssh_test_reset_global_config();
	return TEST_PASS;
}


static int
test_ed25519_haskey_wrong_type(void)
{
	/* Load an RSA key into the ed25519 module's ctx -- haskey should
	 * return false because EVP_PKEY_id != EVP_PKEY_ED25519. */
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_ssh_ed25519(), 0);
	ASSERT_EQ(dssh_register_rsa_sha2_256(), 0);
	ASSERT_EQ(dssh_rsa_sha2_256_generate_key(2048), 0);

	dssh_key_algo ed = find_key_algo("ssh-ed25519");
	dssh_key_algo rsa = find_key_algo("rsa-sha2-256");
	ASSERT_NOT_NULL(ed);
	ASSERT_NOT_NULL(rsa);

	/* Swap: give ed25519 the RSA ctx */
	dssh_key_algo_ctx *saved = ed->ctx;
	ed->ctx = rsa->ctx;
	ASSERT_FALSE(ed->haskey(ed->ctx));
	ed->ctx = saved;

	dssh_test_reset_global_config();
	return TEST_PASS;
}

static int
test_rsa_haskey_wrong_type(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_ssh_ed25519(), 0);
	ASSERT_EQ(dssh_register_rsa_sha2_256(), 0);
	ASSERT_EQ(dssh_ed25519_generate_key(), 0);

	dssh_key_algo ed = find_key_algo("ssh-ed25519");
	dssh_key_algo rsa = find_key_algo("rsa-sha2-256");
	ASSERT_NOT_NULL(ed);
	ASSERT_NOT_NULL(rsa);

	/* Swap: give RSA the ed25519 ctx */
	dssh_key_algo_ctx *saved = rsa->ctx;
	rsa->ctx = ed->ctx;
	ASSERT_FALSE(rsa->haskey(rsa->ctx));
	rsa->ctx = saved;

	dssh_test_reset_global_config();
	return TEST_PASS;
}



/* ================================================================
 * Coverage: first_name helper, register tail->next, cleanup NULL
 * ================================================================ */

static int
test_first_name_basic(void)
{
	char buf[32];
	size_t len = first_name_text("curve25519-sha256,aes256-ctr", buf, sizeof(buf));
	ASSERT_EQ(len, (size_t)17);
	ASSERT_STR_EQ(buf, "curve25519-sha256");
	return TEST_PASS;
}

static int
test_first_name_single(void)
{
	char buf[32];
	size_t len = first_name_text("only-one", buf, sizeof(buf));
	ASSERT_EQ(len, (size_t)8);
	ASSERT_STR_EQ(buf, "only-one");
	return TEST_PASS;
}

static int
test_first_name_small_buf(void)
{
	char buf[4];
	size_t len = first_name_text("longname", buf, sizeof(buf));
	ASSERT_EQ(len, (size_t)3); /* clamped to bufsz-1 */
	ASSERT_STR_EQ(buf, "lon");
	return TEST_PASS;
}

static int
test_register_two_kex(void)
{
	dssh_test_reset_global_config();
	int first;
	int second = DSSH_ERROR_INIT;
	const char *head_name;
	const char *tail_name;
	if (test_using_dhgex()) {
		first = dssh_register_dh_gex_sha256();
		if (first == 0)
			second = dssh_register_curve25519_sha256();
		head_name = "diffie-hellman-group-exchange-sha256";
		tail_name = "curve25519-sha256";
	}
	else {
		first = dssh_register_curve25519_sha256();
		if (first == 0)
			second = dssh_register_dh_gex_sha256();
		head_name = "curve25519-sha256";
		tail_name = "diffie-hellman-group-exchange-sha256";
	}
	bool valid = first == 0 && second == 0 && gconf.kex_entries == 2
	    && gconf.kex_head != NULL && gconf.kex_tail != NULL
	    && gconf.kex_head->next == gconf.kex_tail && gconf.kex_tail->next == NULL
	    && strcmp(gconf.kex_head->name, head_name) == 0
	    && strcmp(gconf.kex_tail->name, tail_name) == 0;
	dssh_test_reset_global_config();
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

static int
test_register_two_comp(void)
{
	ASSERT_TRUE(registration_case(REG_COMP, REG_ACCEPT));
	return TEST_PASS;
}



/* ================================================================
 * KEXINIT parsing -- direct unit tests via parse_peer_kexinit
 * ================================================================ */

#include "transport_offer_cases.h"

/* ================================================================
 * K wire encoding -- direct unit tests via encode_k_wire
 * ================================================================ */

static int
test_encode_k_mpint_no_pad(void)
{
	/* High bit clear -- no sign padding needed */
	uint8_t raw[] = {0x01, 0x02};
	uint8_t *out = NULL;
	size_t out_sz = 0;

	ASSERT_EQ(encode_k_wire(raw, sizeof(raw), false,
	    &out, &out_sz), 0);
	ASSERT_EQ(out_sz, 6u);
	ASSERT_EQ(out[0], 0); ASSERT_EQ(out[1], 0);
	ASSERT_EQ(out[2], 0); ASSERT_EQ(out[3], 2);
	ASSERT_EQ(out[4], 0x01); ASSERT_EQ(out[5], 0x02);
	free(out);
	return TEST_PASS;
}

static int
test_encode_k_mpint_sign_pad(void)
{
	/* High bit set -- mpint needs sign padding */
	uint8_t raw[] = {0x80, 0x01};
	uint8_t *out = NULL;
	size_t out_sz = 0;

	ASSERT_EQ(encode_k_wire(raw, sizeof(raw), false,
	    &out, &out_sz), 0);
	ASSERT_EQ(out_sz, 7u);
	/* Length = 3 (2 data bytes + 1 padding) */
	ASSERT_EQ(out[0], 0); ASSERT_EQ(out[1], 0);
	ASSERT_EQ(out[2], 0); ASSERT_EQ(out[3], 3);
	ASSERT_EQ(out[4], 0x00); /* sign padding */
	ASSERT_EQ(out[5], 0x80); ASSERT_EQ(out[6], 0x01);
	free(out);
	return TEST_PASS;
}

static int
test_encode_k_mpint_empty(void)
{
	uint8_t *out = NULL;
	size_t out_sz = 0;

	ASSERT_EQ(encode_k_wire(NULL, 0, false,
	    &out, &out_sz), 0);
	ASSERT_EQ(out_sz, 4u);
	/* Length = 0 */
	ASSERT_EQ(out[0], 0); ASSERT_EQ(out[1], 0);
	ASSERT_EQ(out[2], 0); ASSERT_EQ(out[3], 0);
	free(out);
	return TEST_PASS;
}

static int
test_encode_k_string(void)
{
	/* String encoding -- no sign padding even with high bit */
	uint8_t raw[] = {0x80, 0x01};
	uint8_t *out = NULL;
	size_t out_sz = 0;

	ASSERT_EQ(encode_k_wire(raw, sizeof(raw), true,
	    &out, &out_sz), 0);
	ASSERT_EQ(out_sz, 6u);
	/* Length = 2 (no padding) */
	ASSERT_EQ(out[0], 0); ASSERT_EQ(out[1], 0);
	ASSERT_EQ(out[2], 0); ASSERT_EQ(out[3], 2);
	ASSERT_EQ(out[4], 0x80); ASSERT_EQ(out[5], 0x01);
	free(out);
	return TEST_PASS;
}

static int
test_encode_k_string_empty(void)
{
	uint8_t *out = NULL;
	size_t out_sz = 0;

	ASSERT_EQ(encode_k_wire(NULL, 0, true,
	    &out, &out_sz), 0);
	ASSERT_EQ(out_sz, 4u);
	ASSERT_EQ(out[0], 0); ASSERT_EQ(out[1], 0);
	ASSERT_EQ(out[2], 0); ASSERT_EQ(out[3], 0);
	free(out);
	return TEST_PASS;
}

/* ================================================================
 * Coverage: hmac-sha2-256 cleanup with NULL, MAC_final failure
 * ================================================================ */

static int
test_hmac_sha2_256_cleanup_null(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_hmac_sha2_256(), 0);

	dssh_mac mac = gconf.mac_head;
	ASSERT_NOT_NULL(mac);

	/* Cleanup with NULL ctx -- should not crash */
	mac->cleanup(NULL);

	dssh_test_reset_global_config();
	return TEST_PASS;
}

#ifdef DSSH_CRYPTO_OPENSSL /* OpenSSL MAC generate injection */
static int
test_hmac_sha2_256_generate_failure(void)
{
	/* Init succeeds, then generate fails at EVP_MAC operation */
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_hmac_sha2_256(), 0);

	dssh_mac mac = gconf.mac_head;
	ASSERT_NOT_NULL(mac);

	uint8_t key[32];
	memset(key, 0x42, sizeof(key));

	dssh_mac_ctx *mctx = NULL;
	ASSERT_EQ(mac->init(key, &mctx), 0);

	/* Arm ossl to fail the next call inside generate */
	dssh_test_ossl_fail_after(0);
	uint8_t data[16] = {0};
	uint8_t tag[64];
	int res = mac->generate(data, sizeof(data), tag, mctx);
	dssh_test_ossl_reset();
	ASSERT_TRUE(res < 0);

	mac->cleanup(mctx);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

#endif /* DSSH_CRYPTO_OPENSSL -- encrypt/MAC injection tests */

/* ================================================================
 * Registration toolong guards (kex, comp)
 * ================================================================ */

static int
test_register_kex_toolong(void)
{
	ASSERT_TRUE(registration_case(REG_KEX, REG_LONG));
	return TEST_PASS;
}

static int
test_register_comp_toolong(void)
{
	ASSERT_TRUE(registration_case(REG_COMP, REG_LONG));
	return TEST_PASS;
}



/* ================================================================
 * Registration toomany guards (all 6 register functions)
 * ================================================================ */

static int
test_register_kex_toomany(void)
{
	ASSERT_TRUE(registration_case(REG_KEX, REG_MANY));
	return TEST_PASS;
}

static int
test_register_key_algo_toomany(void)
{
	ASSERT_TRUE(registration_case(REG_KEY, REG_MANY));
	return TEST_PASS;
}

static int
test_register_enc_toomany(void)
{
	ASSERT_TRUE(registration_case(REG_ENC, REG_MANY));
	return TEST_PASS;
}

static int
test_register_mac_toomany(void)
{
	ASSERT_TRUE(registration_case(REG_MAC, REG_MANY));
	return TEST_PASS;
}

static int
test_register_comp_toomany(void)
{
	ASSERT_TRUE(registration_case(REG_COMP, REG_MANY));
	return TEST_PASS;
}



/* ================================================================
 * Blocksize clamping -- direct tx/rx_block_size tests
 * ================================================================ */

/* ================================================================
 * Cleanup with NULL cleanup function pointers
 * ================================================================ */

static int
test_cleanup_null_cleanup_fn(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(register_all_algorithms(), 0);
	dssh_transport_set_callbacks(mock_tx_dispatch, mock_rx_dispatch,
	    mock_rxline_dispatch, mock_extra_line_cb);

	struct mock_io_state io;
	ASSERT_OK(mock_io_init(&io, 0));
	dssh_session sess = dssh_session_init(true, 0);
	ASSERT_NOT_NULL(sess);
	dssh_session_set_cbdata(sess, &io, &io, &io, &io);

	/* Create dummy module structs with cleanup == NULL */
	struct dssh_kex_s dummy_kex = { 0 };
	struct dssh_enc_s dummy_enc = { 0 };
	struct dssh_mac_s dummy_mac = { 0 };
	struct dssh_comp_s dummy_comp = { 0 };

	/* Set selected modules with NULL cleanup -- transport_cleanup
	 * should skip the cleanup calls without crashing. */
	sess->trans.kex_selected = &dummy_kex;
	sess->trans.enc_c2s_selected = &dummy_enc;
	sess->trans.enc_s2c_selected = &dummy_enc;
	sess->trans.mac_c2s_selected = &dummy_mac;
	sess->trans.mac_s2c_selected = &dummy_mac;
	sess->trans.comp_c2s_selected = &dummy_comp;
	sess->trans.comp_s2c_selected = &dummy_comp;

	/* dssh_session_cleanup calls transport_cleanup internally */
	dssh_session_cleanup(sess);
	mock_io_free(&io);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

/* ================================================================
 * Deterministic branch coverage -- profiling-unstable branches
 *
 * These tests exercise branches that flip between "covered" and
 * "missed" across runs due to non-deterministic thread scheduling
 * in two-threaded iterate tests.  Each test here is single-threaded
 * with no timing dependency.
 * ================================================================ */

/*
 * is_20: SSH-1.99 compatibility version (line 78).
 * The 1.99 branch is exercised non-deterministically via
 * test_version_exchange_accept_199, but here we call is_20 directly.
 */
static int
test_is_20_version_199(void)
{
	uint8_t buf[] = "SSH-1.99-test\r\n";
	ASSERT_TRUE(is_20(buf, sizeof(buf) - 1));
	return TEST_PASS;
}

/*
 * version_rx: received > 255 branch (line 96).
 * Inject a 256-byte SSH version line into the s2c pipe and call
 * version_exchange as a client.  The line exceeds
 * the 255-byte RFC limit, so version_rx must reject it.
 */
static int
test_version_rx_too_long(void)
{
	dssh_test_reset_global_config();
	dssh_transport_set_callbacks(mock_tx_dispatch, mock_rx_dispatch,
	    mock_rxline_dispatch, mock_extra_line_cb);

	struct mock_io_state io;
	ASSERT_OK(mock_io_init(&io, 0));
	ASSERT_OK(dssh_register_none_comp());

	dssh_session client = dssh_session_init(true, 0);
	ASSERT_NOT_NULL(client);
	dssh_session_set_cbdata(client, &io, &io, &io, &io);

	/* Build a 258-byte version line: "SSH-2.0-" (8) + 248 padding + "\r\n" (2) = 258 */
	char long_line[259];
	memcpy(long_line, "SSH-2.0-", 8);
	memset(long_line + 8, 'x', 248);
	long_line[256] = '\r';
	long_line[257] = '\n';
	long_line[258] = '\0';
	mock_io_inject(&io.s2c, (const uint8_t *)long_line, 258);

	int res = version_exchange(client);
	ASSERT_TRUE(res < 0);

	dssh_session_cleanup(client);
	mock_io_free(&io);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

static int
test_version_rx_non_ascii(void)
{
	dssh_test_reset_global_config();
	dssh_transport_set_callbacks(mock_tx_dispatch, mock_rx_dispatch,
	    mock_rxline_dispatch, mock_extra_line_cb);

	struct mock_io_state io;
	ASSERT_OK(mock_io_init(&io, 0));
	ASSERT_OK(dssh_register_none_comp());

	dssh_session client = dssh_session_init(true, 0);
	ASSERT_NOT_NULL(client);
	dssh_session_set_cbdata(client, &io, &io, &io, &io);

	/* Version line with a high byte (0x80) -- non-ASCII */
	char bad_line[] = "SSH-2.0-\x80test\r\n";
	mock_io_inject(&io.s2c, (const uint8_t *)bad_line,
	    sizeof(bad_line) - 1);

	int res = version_exchange(client);
	ASSERT_TRUE(res < 0);

	dssh_session_cleanup(client);
	mock_io_free(&io);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

#ifdef DSSH_CRYPTO_OPENSSL /* derive_key + MAC ossl injection tests */
/*
 * derive_key: fail EVP_DigestUpdate(shared_secret) -- line 1208.
 * ossl calls in derive_key:
 *   0: EVP_MD_fetch
 *   1: EVP_MD_CTX_new
 *   2: EVP_DigestInit_ex
 *   3: EVP_DigestUpdate(shared_secret)  <-- target
 */
static int
test_derive_key_ossl_shared_secret(void)
{
	uint8_t shared_secret[32], hash[32], session_id[32], out[32];
	memset(shared_secret, 0x11, sizeof(shared_secret));
	memset(hash, 0x22, sizeof(hash));
	memset(session_id, 0x33, sizeof(session_id));

	dssh_test_ossl_fail_after(3);
	int res = derive_key("SHA-256",
	    shared_secret, sizeof(shared_secret),
	    hash, sizeof(hash),
	    'A',
	    session_id, sizeof(session_id),
	    out, sizeof(out));
	dssh_test_ossl_reset();
	ASSERT_TRUE(res < 0);

	return TEST_PASS;
}

/*
 * derive_key: fail EVP_DigestUpdate(hash) -- line 1209.
 * ossl call 4.
 */
static int
test_derive_key_ossl_hash(void)
{
	uint8_t shared_secret[32], hash[32], session_id[32], out[32];
	memset(shared_secret, 0x11, sizeof(shared_secret));
	memset(hash, 0x22, sizeof(hash));
	memset(session_id, 0x33, sizeof(session_id));

	dssh_test_ossl_fail_after(4);
	int res = derive_key("SHA-256",
	    shared_secret, sizeof(shared_secret),
	    hash, sizeof(hash),
	    'A',
	    session_id, sizeof(session_id),
	    out, sizeof(out));
	dssh_test_ossl_reset();
	ASSERT_TRUE(res < 0);

	return TEST_PASS;
}

/*
 * derive_key: fail EVP_DigestUpdate(&letter) -- line 1210.
 * ossl call 5.
 */
static int
test_derive_key_ossl_letter(void)
{
	uint8_t shared_secret[32], hash[32], session_id[32], out[32];
	memset(shared_secret, 0x11, sizeof(shared_secret));
	memset(hash, 0x22, sizeof(hash));
	memset(session_id, 0x33, sizeof(session_id));

	dssh_test_ossl_fail_after(5);
	int res = derive_key("SHA-256",
	    shared_secret, sizeof(shared_secret),
	    hash, sizeof(hash),
	    'A',
	    session_id, sizeof(session_id),
	    out, sizeof(out));
	dssh_test_ossl_reset();
	ASSERT_TRUE(res < 0);

	return TEST_PASS;
}

/*
 * derive_key: fail EVP_DigestUpdate(session_id) -- line 1211.
 * ossl call 6.
 */
static int
test_derive_key_ossl_session_id(void)
{
	uint8_t shared_secret[32], hash[32], session_id[32], out[32];
	memset(shared_secret, 0x11, sizeof(shared_secret));
	memset(hash, 0x22, sizeof(hash));
	memset(session_id, 0x33, sizeof(session_id));

	dssh_test_ossl_fail_after(6);
	int res = derive_key("SHA-256",
	    shared_secret, sizeof(shared_secret),
	    hash, sizeof(hash),
	    'A',
	    session_id, sizeof(session_id),
	    out, sizeof(out));
	dssh_test_ossl_reset();
	ASSERT_TRUE(res < 0);

	return TEST_PASS;
}

/*
 * derive_key: fail EVP_DigestFinal_ex -- line 1212.
 * ossl call 7.
 */
static int
test_derive_key_ossl_final(void)
{
	uint8_t shared_secret[32], hash[32], session_id[32], out[32];
	memset(shared_secret, 0x11, sizeof(shared_secret));
	memset(hash, 0x22, sizeof(hash));
	memset(session_id, 0x33, sizeof(session_id));

	dssh_test_ossl_fail_after(7);
	int res = derive_key("SHA-256",
	    shared_secret, sizeof(shared_secret),
	    hash, sizeof(hash),
	    'A',
	    session_id, sizeof(session_id),
	    out, sizeof(out));
	dssh_test_ossl_reset();
	ASSERT_TRUE(res < 0);

	return TEST_PASS;
}

/*
 * derive_key: fail EVP_DigestInit_ex -- line 1207.
 * ossl call 2.
 */
static int
test_derive_key_ossl_init(void)
{
	uint8_t shared_secret[32], hash[32], session_id[32], out[32];
	memset(shared_secret, 0x11, sizeof(shared_secret));
	memset(hash, 0x22, sizeof(hash));
	memset(session_id, 0x33, sizeof(session_id));

	dssh_test_ossl_fail_after(2);
	int res = derive_key("SHA-256",
	    shared_secret, sizeof(shared_secret),
	    hash, sizeof(hash),
	    'A',
	    session_id, sizeof(session_id),
	    out, sizeof(out));
	dssh_test_ossl_reset();
	ASSERT_TRUE(res < 0);

	return TEST_PASS;
}

/*
 * derive_key: need > md_len so the while(have < needed) loop fires.
 * Then fail the second EVP_DigestInit_ex (line 1222).
 * SHA256 produces 32 bytes; requesting 64 forces the extension loop.
 * First pass: calls 0-7 succeed (8 calls total).
 * Second pass loop: call 8 = EVP_DigestInit_ex.
 */
static int
test_derive_key_ossl_extension_loop(void)
{
	uint8_t shared_secret[32], hash[32], session_id[32], out[64];
	memset(shared_secret, 0x11, sizeof(shared_secret));
	memset(hash, 0x22, sizeof(hash));
	memset(session_id, 0x33, sizeof(session_id));

	/* Fail the EVP_DigestInit_ex in the extension loop (call 8) */
	dssh_test_ossl_fail_after(8);
	int res = derive_key("SHA-256",
	    shared_secret, sizeof(shared_secret),
	    hash, sizeof(hash),
	    'A',
	    session_id, sizeof(session_id),
	    out, sizeof(out));
	dssh_test_ossl_reset();
	ASSERT_TRUE(res < 0);

	return TEST_PASS;
}

/*
 * hmac-sha2-256 generate: cbd->ctx == NULL branch (line 63).
 * Init succeeds, then we fail the EVP_MAC_init inside generate
 * which leaves the ctx in a state where re-init fails.
 * Actually, the simplest approach: call generate with ctx == NULL,
 * which is already tested by test_hmac_sha2_256_null_ctx.
 *
 * For the cbd != NULL but cbd->ctx == NULL path: we need to init
 * successfully, then corrupt the context.  The ossl injection can
 * fail the EVP_MAC_CTX_new during init, but then init returns error
 * and doesn't set *out.  Instead, we test by failing at ossl call 1
 * during init (EVP_MAC_CTX_new), which makes init return error
 * without setting mctx -- so mctx stays NULL.
 *
 * The cbd->ctx == NULL check in generate is a safety net.
 * We exercise it by calling generate after a partial init that
 * created the cbd struct but didn't create the EVP_MAC_CTX.
 * However, the init function creates cbd first, then fetches
 * the MAC, then creates the ctx.  If ctx creation fails, init
 * frees cbd and returns error.
 *
 * The only way to reach generate with cbd->ctx == NULL is if
 * something goes very wrong.  Since the existing null_ctx test
 * covers cbd == NULL, let's test with ossl failure at call 2
 * during init (EVP_MAC_init) -- this creates cbd and ctx but
 * then MAC_init fails.  Init then frees ctx and cbd, returning
 * error.  So we can't reach generate this way either.
 *
 * Actually, re-reading the code: generate does
 *   EVP_MAC_init(cbd->ctx, NULL, 0, NULL)
 * This is a re-init with the same key.  If this ossl call fails
 * (line 67), generate returns DSSH_ERROR_INIT.  This is the
 * branch at line 67-68.  Let's test it with ossl injection.
 */
static int
test_hmac_sha2_256_reinit_failure(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_hmac_sha2_256(), 0);

	dssh_mac mac = gconf.mac_head;
	ASSERT_NOT_NULL(mac);

	uint8_t key[32];
	memset(key, 0x42, sizeof(key));

	dssh_mac_ctx *mctx = NULL;
	ASSERT_EQ(mac->init(key, &mctx), 0);

	/* The generate function makes these ossl calls:
	 *   0: EVP_MAC_init (re-init)
	 *   1: EVP_MAC_update
	 *   2: EVP_MAC_final
	 * Fail at call 0 to hit the re-init failure path (line 67). */
	dssh_test_ossl_fail_after(0);
	uint8_t data[16] = {0};
	uint8_t tag[64];
	int res = mac->generate(data, sizeof(data), tag, mctx);
	dssh_test_ossl_reset();
	ASSERT_TRUE(res < 0);

	/* Fail at call 1 to hit EVP_MAC_update failure (line 70) */
	dssh_test_ossl_fail_after(1);
	res = mac->generate(data, sizeof(data), tag, mctx);
	dssh_test_ossl_reset();
	ASSERT_TRUE(res < 0);

	mac->cleanup(mctx);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

/*
 * hmac-sha2-256 init: EVP_MAC_fetch failure (line 27-28).
 * ossl call 0 during init.
 */
static int
test_hmac_sha2_256_fetch_failure(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_hmac_sha2_256(), 0);

	dssh_mac mac = gconf.mac_head;
	ASSERT_NOT_NULL(mac);

	uint8_t key[32];
	memset(key, 0x42, sizeof(key));

	/* Fail the EVP_MAC_fetch (first ossl call in init) */
	dssh_test_ossl_fail_after(0);
	dssh_mac_ctx *mctx = NULL;
	int res = mac->init(key, &mctx);
	dssh_test_ossl_reset();
	ASSERT_TRUE(res < 0);

	dssh_test_reset_global_config();
	return TEST_PASS;
}

/*
 * hmac-sha2-256 init: EVP_MAC_init (HMAC key setup) failure (line 46-47).
 * ossl calls in init:
 *   0: EVP_MAC_fetch
 *   1: EVP_MAC_CTX_new
 *   2: EVP_MAC_init
 * Fail at call 2.
 */
static int
test_hmac_sha2_256_mac_init_failure(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_hmac_sha2_256(), 0);

	dssh_mac mac = gconf.mac_head;
	ASSERT_NOT_NULL(mac);

	uint8_t key[32];
	memset(key, 0x42, sizeof(key));

	/* calloc is call 0 for the library allocator, but ossl countdown
	 * only covers ossl wrappers.  The init function's ossl calls:
	 *   0: EVP_MAC_fetch
	 *   1: EVP_MAC_CTX_new
	 *   2: EVP_MAC_init
	 * Fail at call 2 to hit the EVP_MAC_init failure. */
	dssh_test_ossl_fail_after(2);
	dssh_mac_ctx *mctx = NULL;
	int res = mac->init(key, &mctx);
	dssh_test_ossl_reset();
	ASSERT_TRUE(res < 0);

	dssh_test_reset_global_config();
	return TEST_PASS;
}
#endif /* DSSH_CRYPTO_OPENSSL -- derive_key + MAC injection tests */

/*
 * is_20: "SSH-2X..." -- buf[4]=='2' true but buf[5]!='.' false.
 * Covers the short-circuit branch in the first condition at line 90.
 */
static int
test_is_20_20_bad_dot(void)
{
	uint8_t buf[] = "SSH-2X-test\r\n";
	ASSERT_FALSE(is_20(buf, sizeof(buf) - 1));
	return TEST_PASS;
}

/*
 * SSH_MSG_DEBUG with only 1 byte payload (just the message type byte).
 * The callback should NOT be invoked because payload_len < 2 (line 821).
 */
static int
test_debug_short_payload(void)
{
	dssh_test_reset_global_config();
	ASSERT_OK(register_all_algorithms());
	if (test_generate_host_key() < 0)
		return TEST_FAIL;
	dssh_transport_set_callbacks(mock_tx_dispatch, mock_rx_dispatch,
	    mock_rxline_dispatch, mock_extra_line_cb);

	struct mock_io_state io;
	ASSERT_OK(mock_io_init(&io, 0));

	dssh_session client = dssh_session_init(true, 0);
	ASSERT_NOT_NULL(client);
	dssh_session_set_cbdata(client, &io, &io, &io, &io);

	dssh_session server = init_server_session();
	ASSERT_NOT_NULL(server);
	dssh_session_set_cbdata(server, &io, &io, &io, &io);

	debug_cb_invoked = false;
	dssh_session_set_debug_cb(server, mock_debug_cb_track, NULL);

	/* 1-byte DEBUG: just the message type, no always_display bool */
	uint8_t dbg[] = { SSH_MSG_DEBUG };
	ASSERT_OK(send_packet(client, dbg, sizeof(dbg), NULL));

	uint8_t msg_type;
	uint8_t *payload;
	size_t payload_len;
	ASSERT_EQ(recv_packet(server, &msg_type, &payload, &payload_len), DSSH_ERROR_PARSE);

	ASSERT_FALSE(debug_cb_invoked);
	ASSERT_TRUE(dssh_session_is_terminated(server));

	dssh_session_cleanup(client);
	dssh_session_cleanup(server);
	mock_io_free(&io);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

/* ================================================================
 * kex_set_ctx / tx_gather coverage
 * ================================================================ */

/*
 * Helper: return the KEX algorithm name for the current test variant.
 */
static const char *
current_kex_name(void)
{
	return test_kex_algo_name();
}

static int
test_kex_set_ctx(void)
{
	dssh_test_reset_global_config();

	if (register_all_algorithms() < 0)
		return TEST_SKIP;

	/* Should succeed before any session is created */
	int dummy_ctx = 42;
	ASSERT_OK(dssh_kex_set_ctx(current_kex_name(), &dummy_ctx));

	/* NULL name is rejected */
	ASSERT_ERR(dssh_kex_set_ctx(NULL, &dummy_ctx),
	    DSSH_ERROR_INVALID);

	/* Unregistered name is rejected */
	ASSERT_ERR(dssh_kex_set_ctx("no-such-kex@example.com", &dummy_ctx),
	    DSSH_ERROR_INIT);

	/* Reset ctx back to NULL so it doesn't affect other tests */
	ASSERT_OK(dssh_kex_set_ctx(current_kex_name(), NULL));

	dssh_test_reset_global_config();
	return TEST_PASS;
}

static int
test_kex_set_ctx_toolate(void)
{
	dssh_test_reset_global_config();

	if (register_all_algorithms() < 0)
		return TEST_SKIP;
	if (test_generate_host_key() < 0)
		return TEST_SKIP;

	dssh_transport_set_callbacks(mock_tx_dispatch, mock_rx_dispatch,
	    mock_rxline_dispatch, mock_extra_line_cb);

	/* Before session init -- should succeed */
	ASSERT_OK(dssh_kex_set_ctx(current_kex_name(), NULL));

	dssh_session sess = dssh_session_init(true, 0);
	ASSERT_NOT_NULL(sess);

	/* After session init -- should be refused */
	ASSERT_ERR(dssh_kex_set_ctx(current_kex_name(), NULL),
	    DSSH_ERROR_TOOLATE);

	dssh_session_cleanup(sess);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

static _Atomic int gather_invoked;

static int
test_gather_cb(const struct dssh_tx_iov *iov, size_t iovcnt,
    dssh_session sess, void *cbdata)
{
	struct mock_io_state *io = cbdata;
	if (iov == NULL || iovcnt == 0)
		return DSSH_ERROR_INVALID;

	atomic_store(&gather_invoked, 1);

	/* Send each iov entry through the per-packet TX path */
	for (size_t i = 0; i < iovcnt; i++) {
		if (iov[i].base == NULL || iov[i].len == 0)
			return DSSH_ERROR_INVALID;
		/* The mock sender reads these borrowed wire bytes. */
		uint8_t *packet = (uint8_t *)iov[i].base;
		if (sess->trans.client) {
			if (mock_tx_client(packet,
			    iov[i].len, sess, io) != 0)
				return -1;
		}
		else {
			if (mock_tx_server(packet,
			    iov[i].len, sess, io) != 0)
				return -1;
		}
	}
	return 0;
}

static int
test_tx_gather_roundtrip(void)
{
	ASSERT_TRUE(transport_pair_case(2));
	return TEST_PASS;
}

/* ================================================================
 * rxline_from_rx fallback: handshake with rx_line = NULL
 * ================================================================ */

static int
test_rxline_from_rx_handshake(void)
{
	ASSERT_TRUE(transport_pair_case(3));
	return TEST_PASS;
}

#include "transport_pair_cases.h"
#include "transport_rekey_cases.h"

/* ================================================================
 * Test table
 * ================================================================ */

struct boundary_input {
	uint8_t packet[1024];
	size_t consumed;
	size_t limit;
	size_t reported;
	bool invalid_line;
};

static int
register_fixed_rekey_methods(void)
{
	const char *names[] = {"diffie-hellman-group14-sha256", "diffie-hellman-group16-sha512", "curve25519-sha256"};
	int (*constructors[])(void) = {dssh_register_dh_group14_sha256, dssh_register_dh_group16_sha512,
	    dssh_register_curve25519_sha256};
	for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
		bool found = false;
		for (dssh_kex entry = gconf.kex_head; entry != NULL; entry = entry->next)
			found = found || strcmp(entry->name, names[i]) == 0;
		if (!found) {
			int ret = constructors[i]();
			if (ret != 0)
				return ret;
		}
	}
	return 0;
}

#include "transport_transition_cases.h"
#include "direct_kex_cases.h"
#include "negotiation_pair_cases.h"
#include "transport_state_cases.h"
#include "transport_clock_cases.h"
#include "transport_frame_cases.h"
#include "packet_wire_cases.h"

static int
boundary_tx(uint8_t *buf, size_t bufsz, dssh_session sess, void *cbdata)
{
	(void)buf;
	(void)bufsz;
	(void)sess;
	(void)cbdata;
	return 0;
}

static int
boundary_rx(uint8_t *buf, size_t bufsz, dssh_session sess, void *cbdata)
{
	(void)sess;
	struct boundary_input *input = cbdata;
	if (input->consumed > input->limit || bufsz > input->limit - input->consumed)
		return DSSH_ERROR_TERMINATED;
	for (size_t i = 0; i < bufsz; i++)
		buf[i] = input->packet[(input->consumed + i) % sizeof(input->packet)];
	input->consumed += bufsz;
	return 0;
}

static int
boundary_line(uint8_t *buf, size_t bufsz, size_t *received, dssh_session sess, void *cbdata)
{
	(void)sess;
	(void)bufsz;
	struct boundary_input *input = cbdata;
	if (input->invalid_line) {
		buf[0] = 'X';
		*received = input->reported;
	}
	else {
		static const char version[] = "SSH-2.0-boundary\r\n";
		memcpy(buf, version, sizeof(version) - 1);
		*received = sizeof(version) - 1;
	}
	return 0;
}

static int
negotiation_setup(void)
{
	dssh_test_reset_global_config();
	if (dssh_transport_set_callbacks(boundary_tx, boundary_rx, boundary_line, NULL) < 0
	    || dssh_register_curve25519_sha256() < 0
	    || dssh_register_rsa_sha2_256() < 0 || dssh_register_ssh_ed25519() < 0
	    || dssh_ed25519_generate_key() < 0 || dssh_register_aes256_ctr() < 0
	    || dssh_register_hmac_sha2_256() < 0 || dssh_register_none_comp() < 0)
		return TEST_FAIL;
	return TEST_PASS;
}

static int
add_test_kex(const char *name, uint32_t flags)
{
	size_t len = strlen(name);
	struct dssh_kex_s *entry = malloc(sizeof(*entry) + len + 1);
	if (entry == NULL)
		return DSSH_ERROR_ALLOC;
	*entry = *gconf.kex_head;
	entry->next = NULL;
	entry->flags = flags;
	memcpy(entry->name, name, len + 1);
	int ret = dssh_transport_register_kex(entry);
	if (ret < 0)
		free(entry);
	return ret;
}

static size_t
long_offer_names(char *buf, size_t capacity, const char *tail)
{
	size_t pos = 0;
	for (unsigned i = 0; i < 24; i++) {
		int written = snprintf(&buf[pos], capacity - pos, "%060u,", i);
		if (written < 0)
			return 0;
		size_t len = (size_t)written;
		if (len >= capacity - pos)
			return 0;
		pos += len;
	}
	size_t len = strlen(tail);
	if (!dssh_buffer_has(capacity, pos, len + 1))
		return 0;
	memcpy(&buf[pos], tail, len + 1);
	return pos + len;
}

static int
test_negotiation_full_offers(void)
{
	for (unsigned role = 0; role < 2; role++) {
		ASSERT_EQ(negotiation_setup(), TEST_PASS);
		for (unsigned i = 0; i < 24; i++) {
			char name[65];
			int len = snprintf(name, sizeof(name), "registered-%052u", i);
			ASSERT_TRUE(len > 0 && len < 65);
			ASSERT_OK(add_test_kex(name, DSSH_KEX_FLAG_NEEDS_SIGNATURE_CAPABLE));
		}
		ASSERT_OK(add_test_kex("large-list-tail", DSSH_KEX_FLAG_NEEDS_SIGNATURE_CAPABLE));
		char exchange[2048], keys[2048];
		ASSERT_TRUE(long_offer_names(exchange, sizeof(exchange), "large-list-tail") > 1024);
		const char *tail = role == 0 ? "ssh-ed25519" : "rsa-sha2-256,ssh-ed25519";
		ASSERT_TRUE(long_offer_names(keys, sizeof(keys), tail) > 1024);
		const char *names[10] = { exchange, keys, "aes256-ctr", "aes256-ctr",
		    "hmac-sha2-256", "hmac-sha2-256", "none", "none", "", "" };
		uint8_t packet[8192];
		size_t len = make_peer_offer(names, 0, packet, sizeof(packet));
		ASSERT_TRUE(len > 0);
		dssh_session sess = dssh_session_init(role == 0, 0);
		ASSERT_NOT_NULL(sess);
		sess->trans.peer_kexinit = malloc(len);
		ASSERT_NOT_NULL(sess->trans.peer_kexinit);
		memcpy(sess->trans.peer_kexinit, packet, len);
		sess->trans.peer_kexinit_sz = len;
		int ret = kexinit(sess);
		bool chosen = ret == 0 && strcmp(sess->trans.kex_selected->name, "large-list-tail") == 0
		    && strcmp(sess->trans.key_algo_selected->name, "ssh-ed25519") == 0;
		struct dssh_name_list ours[10];
		bool follows;
		bool complete = ret == 0 && parse_peer_kexinit(sess->trans.our_kexinit,
		    sess->trans.our_kexinit_sz, ours, &follows) == 0 && ours[0].len > 1024;
		dssh_session_cleanup(sess);
		dssh_test_reset_global_config();
		ASSERT_OK(ret);
		ASSERT_TRUE(chosen);
		ASSERT_TRUE(complete);
	}
	return TEST_PASS;
}

static int
test_negotiation_key_capabilities(void)
{
	for (unsigned which = 0; which < 4; which++) {
		ASSERT_EQ(negotiation_setup(), TEST_PASS);
		ASSERT_OK(add_test_kex("needs-encryption", DSSH_KEX_FLAG_NEEDS_ENCRYPTION_CAPABLE));
		bool client = which < 2;
		bool same_guess = (which % 2) != 0;
		const char *peer_exchange = client && !same_guess
		    ? "curve25519-sha256,needs-encryption" : "needs-encryption,curve25519-sha256";
		const char *names[10] = { peer_exchange, "ssh-ed25519", "aes256-ctr", "aes256-ctr",
		    "hmac-sha2-256", "hmac-sha2-256", "none", "none", "", "" };
		uint8_t packet[512];
		size_t len = make_peer_offer(names, 0, packet, sizeof(packet));
		ASSERT_TRUE(len > 0);
		dssh_session sess = dssh_session_init(client, 0);
		ASSERT_NOT_NULL(sess);
		const char *order[2] = { "needs-encryption", "curve25519-sha256" };
		if (!client && !same_guess) {
			order[0] = "curve25519-sha256";
			order[1] = "needs-encryption";
		}
		ASSERT_OK(dssh_session_set_kex_filter(sess, order, 2));
		sess->trans.peer_kexinit = malloc(len);
		ASSERT_NOT_NULL(sess->trans.peer_kexinit);
		memcpy(sess->trans.peer_kexinit, packet, len);
		sess->trans.peer_kexinit_sz = len;
		int ret = kexinit(sess);
		bool chosen = ret == 0 && strcmp(sess->trans.kex_selected->name, "curve25519-sha256") == 0;
		dssh_session_cleanup(sess);
		dssh_test_reset_global_config();
		if (same_guess)
			ASSERT_EQ(ret, DSSH_ERROR_INVALID);
		else {
			ASSERT_OK(ret);
			ASSERT_TRUE(chosen);
		}
	}
	return TEST_PASS;
}

static int
test_registration_rejection(void)
{
	dssh_test_reset_global_config();
	int (*constructors[])(void) = {
		dssh_register_curve25519_sha256, dssh_register_dh_gex_sha256,
		dssh_register_dh_group14_sha256, dssh_register_dh_group16_sha512,
		dssh_register_sntrup761x25519_sha512, dssh_register_mlkem768x25519_sha256,
		dssh_register_ssh_ed25519, dssh_register_rsa_sha2_256, dssh_register_rsa_sha2_512,
		dssh_register_aes256_ctr, dssh_register_aes128_cbc, dssh_register_none_enc,
		dssh_register_hmac_sha2_256, dssh_register_hmac_sha2_512, dssh_register_none_mac,
		dssh_register_none_comp
	};
	for (size_t i = 0; i < sizeof(constructors) / sizeof(constructors[0]); i++) {
		ASSERT_OK(constructors[i]());
		ASSERT_EQ(constructors[i](), DSSH_ERROR_INVALID);
	}
	dssh_session sess = dssh_session_init(true, 0);
	ASSERT_NOT_NULL(sess);
	for (size_t i = 0; i < sizeof(constructors) / sizeof(constructors[0]); i++)
		ASSERT_EQ(constructors[i](), DSSH_ERROR_TOOLATE);
	dssh_session_cleanup(sess);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

static int
unavailable_key(dssh_key_algo_ctx *ctx)
{
	(void)ctx;
	return DSSH_ERROR_INIT;
}

static int
test_offer_haskey_error(void)
{
	ASSERT_EQ(negotiation_setup(), TEST_PASS);
	dssh_key_algo key = find_key_algo("ssh-ed25519");
	ASSERT_NOT_NULL(key);
	key->haskey = unavailable_key;
	dssh_session sess = dssh_session_init(false, 0);
	ASSERT_NOT_NULL(sess);
	int ret = kexinit(sess);
	bool unpublished = sess->trans.our_kexinit == NULL;
	dssh_session_cleanup(sess);
	dssh_test_reset_global_config();
	ASSERT_EQ(ret, DSSH_ERROR_INIT);
	ASSERT_TRUE(unpublished);
	return TEST_PASS;
}

static int
test_version_rx_callback_count(void)
{
	dssh_test_reset_global_config();
	ASSERT_OK(dssh_transport_set_callbacks(boundary_tx, boundary_rx,
	    boundary_line, mock_extra_line_cb));
	struct boundary_input input = { .invalid_line = true, .reported = SIZE_MAX };
	dssh_session sess = dssh_session_init(false, 0);
	ASSERT_NOT_NULL(sess);
	ASSERT_OK(dssh_session_set_cbdata(sess, NULL, &input, &input, NULL));
	ASSERT_EQ(version_exchange(sess), DSSH_ERROR_INVALID);
	dssh_session_cleanup(sess);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

static int
boundary_stale_line(uint8_t *buf, size_t bufsz, size_t *received, dssh_session sess, void *cbdata)
{
	(void)bufsz;
	(void)sess;
	size_t *calls = cbdata;
	static const char line[] = "banner\r\n";
	if ((*calls)++ == 0) {
		memcpy(buf, line, sizeof(line) - 1);
		*received = sizeof(line) - 1;
		return 0;
	}
	if (*calls == 2)
		return 0;
	return DSSH_ERROR_TERMINATED;
}

static int
test_version_rx_count_reset(void)
{
	dssh_test_reset_global_config();
	ASSERT_OK(dssh_transport_set_callbacks(boundary_tx, boundary_rx,
	    boundary_stale_line, NULL));
	size_t calls = 0;
	dssh_session sess = dssh_session_init(false, 0);
	ASSERT_NOT_NULL(sess);
	ASSERT_OK(dssh_session_set_cbdata(sess, NULL, NULL, &calls, NULL));
	int ret = version_exchange(sess);
	dssh_session_cleanup(sess);
	dssh_test_reset_global_config();
	ASSERT_EQ(ret, DSSH_ERROR_INVALID);
	ASSERT_EQ_U(calls, 2);
	return TEST_PASS;
}

static int
boundary_positive_io(uint8_t *buf, size_t bufsz, dssh_session sess, void *cbdata)
{
	(void)buf;
	(void)bufsz;
	(void)sess;
	(void)cbdata;
	return 1;
}

static int
boundary_positive_line(uint8_t *buf, size_t bufsz, size_t *received, dssh_session sess, void *cbdata)
{
	int ret = boundary_line(buf, bufsz, received, sess, cbdata);
	return ret == 0 ? 1 : ret;
}

static int
boundary_positive_extra(uint8_t *buf, size_t bufsz, void *cbdata)
{
	(void)buf;
	(void)bufsz;
	(void)cbdata;
	return 1;
}

static int
boundary_positive_gather(const struct dssh_tx_iov *iov, size_t count, dssh_session sess, void *cbdata)
{
	(void)iov;
	(void)count;
	(void)sess;
	(void)cbdata;
	return 1;
}

static int
test_callback_positive_results(void)
{
	for (unsigned which = 0; which < 5; which++) {
		dssh_test_reset_global_config();
		ASSERT_OK(dssh_transport_set_callbacks(which == 0 ? boundary_positive_io : boundary_tx,
		    which == 1 ? boundary_positive_io : boundary_rx,
		    which == 2 ? boundary_positive_line : boundary_line,
		    which == 3 ? boundary_positive_extra : NULL));
		if (which == 4)
			ASSERT_OK(dssh_transport_set_tx_gather(boundary_positive_gather));
		struct boundary_input input = { .invalid_line = which == 3, .reported = 1 };
		dssh_session sess = dssh_session_init(false, 0);
		ASSERT_NOT_NULL(sess);
		ASSERT_OK(dssh_session_set_cbdata(sess, NULL, &input, &input, NULL));
		int ret;
		uint8_t type;
		uint8_t *payload;
		size_t len;
		uint8_t message[] = { SSH_MSG_USERAUTH_SUCCESS };
		if (which == 1)
			ret = recv_packet(sess, &type, &payload, &len);
		else if (which == 4)
			ret = send_packet(sess, message, sizeof(message), NULL);
		else
			ret = version_exchange(sess);
		bool terminated = dssh_session_is_terminated(sess);
		dssh_session_cleanup(sess);
		dssh_test_reset_global_config();
		ASSERT_EQ(ret, DSSH_ERROR_INVALID);
		ASSERT_TRUE(terminated);
	}
	return TEST_PASS;
}

struct initial_phase_probe {
	unsigned calls;
	int result;
};

static int
initial_phase_global(const uint8_t *name, size_t name_len, bool want_reply,
    const uint8_t *data, size_t data_len, void *opaque)
{
	(void)name;
	(void)name_len;
	(void)want_reply;
	(void)data;
	(void)data_len;
	struct initial_phase_probe *probe = opaque;
	probe->calls++;
	return probe->result;
}

static int
test_initial_kex_rejects_out_of_phase(void)
{
	unsigned passed = 0;
	unsigned callbacks = 0;
	for (unsigned role = 0; role < 2; role++) {
		for (unsigned kind = 0; kind < 5; kind++) {
			dssh_test_reset_global_config();
			ASSERT_OK(register_all_algorithms());
			ASSERT_OK(test_generate_host_key());
			ASSERT_OK(dssh_transport_set_callbacks(boundary_tx, boundary_rx,
			    boundary_line, NULL));
			struct boundary_input input = { .limit = 1024 };
			input.packet[2] = 3;
			input.packet[3] = 252;
			input.packet[4] = 4;
			input.packet[5] = kind == 0 ? SSH_MSG_USERAUTH_REQUEST : SSH_MSG_GLOBAL_REQUEST;
			if (kind != 0) {
				size_t pos = 6;
				DSSH_PUT_U32(DSSH_STRLEN("phase-test"), input.packet, &pos);
				memcpy(input.packet + pos, "phase-test", DSSH_STRLEN("phase-test"));
				pos += DSSH_STRLEN("phase-test");
				input.packet[pos] = kind == 1 ? 0 : 1;
			}
			dssh_session sess = dssh_session_init(role == 0, 0);
			ASSERT_NOT_NULL(sess);
			struct initial_phase_probe probe = { .result = kind == 3 ? DSSH_ERROR_REJECTED : 0 };
			ASSERT_OK(dssh_session_set_cbdata(sess, NULL, &input, &input, NULL));
			if (role == 0)
				ASSERT_OK(dssh_session_set_hostkey_verify_cb(sess, dssh_test_accept_hostkey, NULL));
			if (kind != 4)
				ASSERT_OK(dssh_session_set_global_request_cb(sess, initial_phase_global, &probe));
			int ret = dssh_transport_handshake(sess);
			bool valid = ret == DSSH_ERROR_PARSE && probe.calls == 0
			    && sess->trans.rekey_queue_head == NULL && sess->trans.rekey_queue_count == 0
			    && sess->trans.rekey_queue_bytes == 0 && sess->trans.session_id == NULL
			    && input.consumed == sizeof(input.packet) && sess->trans.tx_seq == 1
			    && dssh_session_is_terminated(sess);
			callbacks += probe.calls;
			if (valid)
				passed++;
			else
				fprintf(stderr, "initial KEX phase role=%u kind=%u result=%d callbacks=%u tx=%" PRIu32 "\n",
				    role, kind, ret, probe.calls, sess->trans.tx_seq);
			dssh_session_cleanup(sess);
			dssh_test_reset_global_config();
		}
	}
	printf("initial KEX phases: 10 cases checked, %u passed, %u callbacks\n", passed, callbacks);
	ASSERT_EQ_U(passed, 10);
	return TEST_PASS;
}

static int
test_rekey_queue_bounded(void)
{
	dssh_test_reset_global_config();
	ASSERT_OK(register_all_algorithms());
	ASSERT_OK(test_generate_host_key());
	ASSERT_OK(dssh_transport_set_callbacks(boundary_tx, boundary_rx,
	    boundary_line, NULL));
	struct boundary_input input = { .limit = 1024 * (DSSH_REKEY_QUEUE_MAX_MESSAGES + 1) };
	input.packet[2] = 3;
	input.packet[3] = 252;
	input.packet[4] = 4;
	input.packet[5] = SSH_MSG_CHANNEL_DATA;
	dssh_session sess = dssh_session_init(false, 0);
	ASSERT_NOT_NULL(sess);
	ASSERT_OK(dssh_session_set_cbdata(sess, NULL, &input, &input, NULL));
	/* Established-session state, with old-key traffic arriving before
	 * the peer's KEXINIT. No cryptographic operations are needed here. */
	sess->trans.session_id = malloc(1);
	ASSERT_NOT_NULL(sess->trans.session_id);
	sess->trans.session_id_sz = 1;
	sess->trans.rekey_in_progress = true;
	ASSERT_EQ(kexinit(sess), DSSH_ERROR_TOOLONG);
	ASSERT_EQ_U(sess->trans.rekey_queue_count, DSSH_REKEY_QUEUE_MAX_MESSAGES);
	ASSERT_TRUE(sess->trans.rekey_queue_bytes <= DSSH_REKEY_QUEUE_MAX_BYTES);
	dssh_session_cleanup(sess);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

/* Explicit policy answers include saturated counters, the opt-out sentinel,
 * unequal directional limits and independent packet-count triggers. */
static int
test_rekey_optout_policy(void)
{
	static const struct {
		uint64_t tx_limit, rx_limit, tx_bytes, rx_bytes;
		uint_fast32_t tx_packets, rx_packets;
		bool expected;
	} cases[] = {
		{UINT64_MAX, UINT64_MAX, UINT64_MAX, UINT64_MAX, 0, 0, false},
		{UINT64_MAX, 1024, UINT64_MAX, 1023, 0, 0, false},
		{UINT64_MAX, 1024, UINT64_MAX, 1024, 0, 0, true},
		{1024, UINT64_MAX, 1023, UINT64_MAX, 0, 0, false},
		{1024, UINT64_MAX, 1024, UINT64_MAX, 0, 0, true},
		{UINT64_MAX - 1, UINT64_MAX, UINT64_MAX - 2, UINT64_MAX, 0, 0, false},
		{UINT64_MAX - 1, UINT64_MAX, UINT64_MAX - 1, UINT64_MAX, 0, 0, true},
		{UINT64_MAX, UINT64_MAX - 1, UINT64_MAX, UINT64_MAX - 2, 0, 0, false},
		{UINT64_MAX, UINT64_MAX - 1, UINT64_MAX, UINT64_MAX - 1, 0, 0, true},
		{0, UINT64_MAX, 0, UINT64_MAX, 0, 0, true},
		{UINT64_MAX, 0, UINT64_MAX, 0, 0, 0, true},
		{UINT64_MAX, UINT64_MAX, UINT64_MAX, UINT64_MAX, DSSH_REKEY_SOFT_LIMIT, 0, true},
		{UINT64_MAX, UINT64_MAX, UINT64_MAX, UINT64_MAX, 0, DSSH_REKEY_SOFT_LIMIT, true},
	};
	unsigned checked = 0, mismatches = 0;
	for (unsigned role = 0; role < 2; role++) {
		dssh_test_reset_global_config();
		dssh_session sess = dssh_session_init(role == 0, 0);
		if (sess == NULL)
			return TEST_FAIL;
		struct dssh_enc_s tx = {0}, rx = {0};
		sess->trans.enc_c2s_selected = role == 0 ? &tx : &rx;
		sess->trans.enc_s2c_selected = role == 0 ? &rx : &tx;
		for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
			tx.bytes_per_key = cases[i].tx_limit;
			rx.bytes_per_key = cases[i].rx_limit;
			atomic_store(&sess->trans.tx_bytes_since_rekey, cases[i].tx_bytes);
			sess->trans.rx_bytes_since_rekey = cases[i].rx_bytes;
			atomic_store(&sess->trans.tx_since_rekey, cases[i].tx_packets);
			sess->trans.rx_since_rekey = cases[i].rx_packets;
			bool actual = rekey_needed(sess);
			checked++;
			if (actual != cases[i].expected) {
				mismatches++;
				fprintf(stderr, "rekey optout role=%u case=%zu actual=%u expected=%u\n",
				    role, i, actual ? 1U : 0U, cases[i].expected ? 1U : 0U);
			}
		}
		dssh_session_cleanup(sess);
		dssh_test_reset_global_config();
	}
	fprintf(stderr, "rekey_optout_policy cases=%u mismatches=%u\n", checked, mismatches);
	ASSERT_EQ_U(checked, 26);
	ASSERT_EQ_U(mismatches, 0);
	return TEST_PASS;
}

static int
test_rekey_optout_packets(void)
{
	struct handshake_ctx ctx;
	extra_algorithm_registration = dssh_register_none_enc;
	int ret = handshake_setup_filtered(&ctx, false, NULL, "none", NULL);
	extra_algorithm_registration = NULL;
	bool valid = ret == 0;
	static const uint8_t packet[] = {SSH_MSG_SERVICE_REQUEST, 0, 0, 0, 1, 'x'};
	unsigned checked = 0;
	for (unsigned role = 0; valid && role < 2; role++) {
		dssh_session sender = role == 0 ? ctx.client : ctx.server;
		dssh_session receiver = role == 0 ? ctx.server : ctx.client;
		valid = sender->trans.enc_c2s_active != NULL && sender->trans.enc_s2c_active != NULL
		    && strcmp(sender->trans.enc_c2s_active->name, "none") == 0
		    && strcmp(sender->trans.enc_s2c_active->name, "none") == 0;
		if (!valid)
			break;
		/* Cross saturation through actual successful packet accounting. */
		atomic_store(&sender->trans.tx_bytes_since_rekey, UINT64_MAX - 1);
		sender->trans.rx_bytes_since_rekey = UINT64_MAX - 1;
		atomic_store(&receiver->trans.tx_bytes_since_rekey, UINT64_MAX - 1);
		receiver->trans.rx_bytes_since_rekey = UINT64_MAX - 1;
		ret = send_packet(sender, packet, sizeof(packet), NULL);
		valid = ret == 0 && atomic_load(&sender->trans.tx_bytes_since_rekey) == UINT64_MAX;
		uint8_t type, *payload;
		size_t len;
		if (valid) {
			ret = recv_packet(receiver, &type, &payload, &len);
			valid = ret == 0 && type == packet[0] && len == sizeof(packet)
			    && memcmp(payload, packet, sizeof(packet)) == 0
			    && receiver->trans.rx_bytes_since_rekey == UINT64_MAX;
		}
		if (valid)
			valid = !rekey_needed(sender) && !rekey_needed(receiver)
			    && !sender->trans.rekey_pending && !receiver->trans.rekey_pending
			    && !atomic_load(&sender->trans.rekey_in_progress)
			    && !atomic_load(&receiver->trans.rekey_in_progress)
			    && !dssh_session_is_terminated(sender) && !dssh_session_is_terminated(receiver);
		if (valid)
			checked++;
	}
	handshake_cleanup(&ctx);
	fprintf(stderr, "rekey_optout_packets directions=%u valid=%u\n", checked, valid ? 1U : 0U);
	ASSERT_TRUE(valid);
	ASSERT_EQ_U(checked, 2);
	return TEST_PASS;
}

static int
queue_during_disconnect(void *arg)
{
	struct disconnect_queue_state *state = arg;
	if (!dssh_test_wait_bool(&state->entered, true)) {
		state->queued = DSSH_ERROR_TIMEOUT;
		atomic_store(&state->release, true);
		return -1;
	}
	uint8_t reply = SSH_MSG_REQUEST_FAILURE;
	state->queued = send_to_slot(state->sess, &state->sess->trans.global_reply_slot, &reply, 1);
	atomic_store(&state->release, true);
	return 0;
}

static int
test_disconnect_queue_order(void)
{
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}
	struct disconnect_queue_state state = { .sess = ctx.client, .queued = -1 };
	atomic_init(&state.entered, false);
	atomic_init(&state.release, false);
	disconnect_queue = &state;
	thrd_t producer;
	int created = thrd_create(&producer, queue_during_disconnect, &state);
	if (created != thrd_success) {
		disconnect_queue = NULL;
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}
	int ret = dssh_transport_disconnect(ctx.client, SSH_DISCONNECT_BY_APPLICATION, "done");
	int joined = thrd_join(producer, NULL);
	disconnect_queue = NULL;
	ASSERT_EQ(joined, thrd_success);
	ASSERT_OK(ret);
	ASSERT_OK(state.queued);
	ASSERT_EQ_U(state.writes, 1);
	ASSERT_TRUE(atomic_load(&ctx.client->trans.global_reply_slot.ready));
	ASSERT_TRUE(dssh_session_is_terminated(ctx.client));
	ASSERT_FALSE(tx_owner_is_current(ctx.client));
	uint8_t type;
	uint8_t *payload;
	size_t len;
	ASSERT_ERR(recv_packet(ctx.server, &type, &payload, &len), DSSH_ERROR_TERMINATED);
	handshake_cleanup(&ctx);
	return TEST_PASS;
}

static int
test_disconnect_validation(void)
{
	for (int mode = 0; mode < 3; mode++) {
		struct handshake_ctx ctx;
		if (handshake_setup(&ctx) < 0) {
			handshake_cleanup(&ctx);
			return TEST_FAIL;
		}
		uint32_t seq = ctx.client->trans.tx_seq;
		ASSERT_ERR(dssh_transport_disconnect(ctx.client, 0, "invalid"), DSSH_ERROR_INVALID);
		ASSERT_FALSE(dssh_session_is_terminated(ctx.client));
		ASSERT_EQ_U(ctx.client->trans.tx_seq, seq);
		if (mode == 2) {
			mock_io_close_c2s(&ctx.io);
			ASSERT_TRUE(dssh_transport_disconnect(ctx.client, 11, "closed") < 0);
			ASSERT_TRUE(dssh_session_is_terminated(ctx.client));
		}
		else {
			test_log_peer_count = 0;
			ASSERT_OK(dssh_session_set_log_cb(ctx.server, test_transport_log_cb, NULL));
			if (mode == 0)
				ASSERT_OK(dssh_transport_disconnect(ctx.client, UINT32_MAX, "private"));
			else {
				uint8_t invalid[13] = { SSH_MSG_DISCONNECT };
				ASSERT_OK(send_packet(ctx.client, invalid, sizeof(invalid), NULL));
			}
			uint8_t type;
			uint8_t *payload;
			size_t len;
			ASSERT_ERR(recv_packet(ctx.server, &type, &payload, &len), DSSH_ERROR_TERMINATED);
			if (mode == 0) {
				ASSERT_EQ(test_log_peer_count, 1);
				ASSERT_EQ_U(test_log_peer_reason, UINT32_MAX);
			}
			else
				ASSERT_EQ(test_log_peer_count, 0);
		}
		handshake_cleanup(&ctx);
	}
	return TEST_PASS;
}

struct global_probe {
	dssh_session sess;
	unsigned calls;
	size_t name_len;
	size_t data_len;
	uint8_t data[3];
	bool terminate;
};

static int
global_probe_cb(const uint8_t *name, size_t name_len, bool want_reply,
    const uint8_t *data, size_t data_len, void *cbdata)
{
	(void)name;
	(void)want_reply;
	struct global_probe *probe = cbdata;
	probe->calls++;
	probe->name_len = name_len;
	probe->data_len = data_len;
	if (data_len == sizeof(probe->data))
		memcpy(probe->data, data, data_len);
	if (probe->terminate)
		dssh_session_terminate(probe->sess);
	return 0;
}

static int
test_global_request_validation(void)
{
	for (unsigned mode = 0; mode < 8; mode++) {
		struct handshake_ctx ctx;
		if (handshake_setup(&ctx) < 0) {
			handshake_cleanup(&ctx);
			return TEST_FAIL;
		}
		struct global_probe probe = { .sess = ctx.server };
		ASSERT_OK(dssh_session_set_global_request_cb(ctx.server, global_probe_cb, &probe));
		uint8_t name[65];
		memset(name, 'a', sizeof(name));
		size_t length = mode == 0 ? 0 : mode == 1 ? 65 : 1;
		if (mode >= 2 && mode <= 5) {
			const uint8_t invalid[] = { ' ', ',', 127, 255 };
			name[0] = invalid[mode - 2];
		}
		uint32_t declared = (uint32_t)length;
		if (mode == 7)
			declared = UINT32_MAX;
		uint8_t packet[72];
		size_t pos = 0;
		packet[pos++] = SSH_MSG_GLOBAL_REQUEST;
		DSSH_PUT_U32(declared, packet, &pos);
		memcpy(&packet[pos], name, length);
		pos += length;
		if (mode != 6)
			packet[pos++] = 1;
		ASSERT_OK(send_packet(ctx.client, packet, pos, NULL));
		uint8_t type;
		uint8_t *payload;
		size_t len;
		ASSERT_ERR(recv_packet(ctx.server, &type, &payload, &len), DSSH_ERROR_PARSE);
		ASSERT_EQ_U(probe.calls, 0);
		/* No request reply is permitted before a valid want_reply field. */
		ASSERT_ERR(recv_packet(ctx.client, &type, &payload, &len), DSSH_ERROR_TERMINATED);
		handshake_cleanup(&ctx);
	}
	return TEST_PASS;
}

static int
test_global_request_results(void)
{
	for (int mode = 0; mode < 3; mode++) {
		struct handshake_ctx ctx;
		if (handshake_setup(&ctx) < 0) {
			handshake_cleanup(&ctx);
			return TEST_FAIL;
		}
		struct global_probe probe = { .sess = ctx.server, .terminate = mode == 2 };
		ASSERT_OK(dssh_session_set_global_request_cb(ctx.server, global_probe_cb, &probe));
		uint8_t packet[73];
		size_t pos = 0;
		packet[pos++] = SSH_MSG_GLOBAL_REQUEST;
		DSSH_PUT_U32(64, packet, &pos);
		memset(&packet[pos], 'a', 64);
		pos += 64;
		packet[pos++] = mode == 0 ? 0 : 1;
		packet[pos++] = 0;
		packet[pos++] = 255;
		packet[pos++] = 1;
		ASSERT_OK(send_packet(ctx.client, packet, pos, NULL));
		if (mode == 0) {
			uint8_t follow = SSH_MSG_REQUEST_FAILURE;
			ASSERT_OK(send_packet(ctx.client, &follow, 1, NULL));
		}
		else if (mode == 1)
			mock_io_close_s2c(&ctx.io);
		uint32_t before = ctx.server->trans.tx_seq;
		uint8_t type;
		uint8_t *payload;
		size_t len;
		int ret = recv_packet(ctx.server, &type, &payload, &len);
		if (mode == 0) {
			ASSERT_OK(ret);
			ASSERT_EQ(type, SSH_MSG_REQUEST_FAILURE);
			ASSERT_EQ_U(ctx.server->trans.tx_seq, before);
		}
		else {
			ASSERT_TRUE(ret < 0);
			ASSERT_TRUE(dssh_session_is_terminated(ctx.server));
			if (mode == 2)
				ASSERT_EQ_U(ctx.server->trans.tx_seq, before);
		}
		ASSERT_EQ_U(probe.calls, 1);
		ASSERT_EQ_U(probe.name_len, 64);
		ASSERT_EQ_U(probe.data_len, 3);
		const uint8_t expected[] = { 0, 255, 1 };
		ASSERT_MEM_EQ(probe.data, expected, sizeof(expected));
		handshake_cleanup(&ctx);
	}
	return TEST_PASS;
}

static int
unused_control_prompt(const uint8_t *name, size_t name_len, const uint8_t *instruction,
    size_t instruction_len, uint32_t count, const uint8_t **prompts, const size_t *prompt_lens,
    const bool *echo, struct dssh_auth_responses *out, void *cbdata)
{
	(void)name;
	(void)name_len;
	(void)instruction;
	(void)instruction_len;
	(void)count;
	(void)prompts;
	(void)prompt_lens;
	(void)echo;
	(void)out;
	(void)cbdata;
	return DSSH_ERROR_INVALID;
}

static int
test_auth_control_reply_errors(void)
{
	for (unsigned mode = 0; mode < 6; mode++) {
		struct handshake_ctx ctx;
		if (handshake_setup(&ctx) < 0) {
			handshake_cleanup(&ctx);
			return TEST_FAIL;
		}
		bool server = mode < 2;
		dssh_session receiver = server ? ctx.server : ctx.client;
		dssh_session sender = server ? ctx.client : ctx.server;
		if (mode == 1) {
			uint8_t service[17];
			size_t pos = 0;
			service[pos++] = SSH_MSG_SERVICE_REQUEST;
			DSSH_PUT_U32(12, service, &pos);
			memcpy(&service[pos], "ssh-userauth", 12);
			pos += 12;
			ASSERT_OK(send_packet(sender, service, pos, NULL));
		}
		uint8_t unknown = server ? 59 : 200;
		ASSERT_OK(send_packet(sender, &unknown, 1, NULL));
		if (mode >= 3)
			receiver->auth_service_requested = true;
		struct control_tx_failure failure = {
			.sess = receiver, .fail_at = mode == 0 ? 0 : 1,
		};
		control_tx_failure = &failure;
		int ret;
		if (server) {
			struct dssh_auth_server_cbs cbs = { .methods_str = "password" };
			ret = auth_server_impl(receiver, &cbs, NULL, NULL);
		}
		else if (mode == 2) {
			char methods[32];
			ret = test_auth_discover_component(receiver, "user", methods, sizeof(methods));
		}
		else if (mode == 3)
			ret = auth_password_impl(receiver, "user", "password", NULL, NULL);
		else if (mode == 4)
			ret = auth_kbi_impl(receiver, "user", unused_control_prompt, NULL);
		else
			ret = auth_publickey_impl(receiver, "user", test_key_algo_name());
		control_tx_failure = NULL;
		ASSERT_ERR(ret, DSSH_ERROR_INIT);
		ASSERT_EQ_U(failure.calls, failure.fail_at + 1);
		ASSERT_TRUE(dssh_session_is_terminated(receiver));
		ASSERT_FALSE(tx_owner_is_current(receiver));
		handshake_cleanup(&ctx);
	}
	return TEST_PASS;
}

struct auth_state_probe {
	unsigned requests;
	unsigned none_calls;
	unsigned password_calls;
	unsigned kbi_calls;
	unsigned unimplemented_calls;
	uint32_t rejected[3];
	bool complete_kbi;
	bool valid_identity;
};

static int
auth_state_request(const uint8_t *user, size_t length, const uint8_t *method,
    size_t method_length, bool new_identity, void *cbdata)
{
	struct auth_state_probe *probe = cbdata;
	const char *expected = probe->requests == 0 ? "first" : "second";
	probe->valid_identity &= new_identity && length == strlen(expected)
	    && memcmp(user, expected, length) == 0 && method != NULL && method_length != 0;
	/* This fixture has no completed factors; allow its identity replacement. */
	probe->requests++;
	return 0;
}

static void
auth_state_unimplemented(uint32_t rejected, void *cbdata)
{
	struct auth_state_probe *probe = cbdata;
	if (probe->unimplemented_calls < 3)
		probe->rejected[probe->unimplemented_calls] = rejected;
	probe->unimplemented_calls++;
}

static int
auth_state_none(const uint8_t *user, size_t length, void *cbdata)
{
	struct auth_state_probe *probe = cbdata;
	probe->none_calls++;
	probe->valid_identity = probe->valid_identity && length == 6 && memcmp(user, "second", 6) == 0;
	return DSSH_AUTH_SUCCESS;
}

static int
auth_state_password(const uint8_t *user, size_t length, const uint8_t *password,
    size_t password_len, struct dssh_bytes_result *output_result, void *cbdata)
{
	uint8_t *fixture_output = NULL;
	size_t fixture_length = 0;
	uint8_t **prompt = &fixture_output;
	size_t *prompt_len = &fixture_length;
	(void)prompt;
	(void)prompt_len;

	(void)prompt;
	(void)prompt_len;
	(void)password;
	struct auth_state_probe *probe = cbdata;
	probe->password_calls++;
	probe->valid_identity = probe->valid_identity && length == 6 && memcmp(user, "second", 6) == 0 && password_len == 0;
	return test_auth_output_finish(output_result, fixture_output, fixture_length, DSSH_AUTH_SUCCESS);
}

static int
auth_state_kbi(const uint8_t *user, size_t length, uint32_t count,
    const uint8_t **responses, const size_t *response_lens, struct dssh_auth_challenge *out, void *cbdata)
{
	(void)responses;
	(void)response_lens;
	(void)out;
	struct auth_state_probe *probe = cbdata;
	probe->kbi_calls++;
	const char *expected = probe->kbi_calls == 1 || probe->complete_kbi ? "first" : "second";
	probe->valid_identity = probe->valid_identity && length == strlen(expected) && memcmp(user, expected, length) == 0 && count == 0;
	return probe->kbi_calls == 1 ? DSSH_AUTH_KBI_PROMPT : DSSH_AUTH_SUCCESS;
}

#include "auth_state_wire_cases.h"

static int
test_auth_kbi_request_state(void)
{
	const char *methods[] = { "none", "password", "keyboard-interactive" };
	for (unsigned mode = 0; mode < 6; mode++) {
		struct handshake_ctx ctx;
		if (handshake_setup(&ctx) < 0) {
			handshake_cleanup(&ctx);
			return TEST_FAIL;
		}
		struct auth_state_probe probe = { .complete_kbi = mode == 3, .valid_identity = true };
		struct dssh_auth_server_cbs cbs = {
			.methods_str = "password,keyboard-interactive", .none_cb = auth_state_none,
			.request_cb = auth_state_request,
			.password_cb = auth_state_password, .keyboard_interactive_cb = auth_state_kbi,
			.cbdata = &probe,
		};
		uint8_t before_service = 9; /* unassigned generic transport message */
		uint32_t rejected_service;
		ASSERT_OK(send_packet(ctx.client, &before_service, 1, &rejected_service));
		ASSERT_OK(queue_auth_service(ctx.client, false));
		ASSERT_OK(queue_auth_state_request(ctx.client, "first", "keyboard-interactive"));
		uint8_t unknown = 59;
		uint32_t rejected;
		ASSERT_OK(send_packet(ctx.client, &unknown, 1, &rejected));
		if (mode < 3)
			ASSERT_OK(queue_auth_state_request(ctx.client, "second", methods[mode]));
		else if (mode == 3) {
			const uint8_t response[5] = { SSH_MSG_USERAUTH_INFO_RESPONSE, 0, 0, 0, 0 };
			ASSERT_OK(send_packet(ctx.client, response, sizeof(response), NULL));
		}
		else {
			uint8_t response[9] = { SSH_MSG_USERAUTH_INFO_RESPONSE };
			size_t pos = 1;
			uint32_t count = mode == 4 ? 1 : UINT32_MAX;
			DSSH_PUT_U32(count, response, &pos);
			if (mode == 4)
				DSSH_PUT_U32(0, response, &pos);
			ASSERT_OK(send_packet(ctx.client, response, pos, NULL));
			ASSERT_OK(queue_auth_state_request(ctx.client, "second", "none"));
		}
		mock_io_close_c2s_write(&ctx.io);
		uint8_t username[16];
		size_t username_len = sizeof(username);
		ASSERT_OK(auth_server_impl(ctx.server, &cbs, username, &username_len));
		ASSERT_FALSE(ctx.server->server_auth_active);
		ASSERT_FALSE(dssh_session_is_terminated(ctx.server));
		ASSERT_TRUE(probe.valid_identity);
		ASSERT_EQ_U(probe.requests, mode == 3 ? 1 : 2);
		ASSERT_EQ_U(probe.none_calls, mode == 0 || mode > 3 ? 1 : 0);
		ASSERT_EQ_U(probe.password_calls, mode == 1 ? 1 : 0);
		ASSERT_EQ_U(probe.kbi_calls, mode == 2 || mode == 3 ? 2 : 1);
		const char *expected = mode == 3 ? "first" : "second";
		ASSERT_EQ_U(username_len, strlen(expected));
		ASSERT_MEM_EQ(username, expected, username_len);
		ASSERT_OK(dssh_session_set_unimplemented_cb(ctx.client, auth_state_unimplemented, &probe));
		uint8_t type;
		uint8_t *payload;
		size_t length;
		ASSERT_OK(recv_packet(ctx.client, &type, &payload, &length));
		ASSERT_EQ(type, SSH_MSG_SERVICE_ACCEPT);
		ASSERT_OK(recv_packet(ctx.client, &type, &payload, &length));
		ASSERT_EQ(type, SSH_MSG_USERAUTH_INFO_REQUEST);
		if (mode > 3) {
			ASSERT_OK(recv_packet(ctx.client, &type, &payload, &length));
			ASSERT_EQ(type, SSH_MSG_USERAUTH_FAILURE);
			ASSERT_TRUE(length >= 6);
			ASSERT_EQ_U(payload[length - 1], 0);
		}
		ASSERT_OK(recv_packet(ctx.client, &type, &payload, &length));
		ASSERT_EQ(type, SSH_MSG_USERAUTH_SUCCESS);
		ASSERT_EQ_U(probe.unimplemented_calls, 2);
		ASSERT_EQ_U(probe.rejected[0], rejected_service);
		ASSERT_EQ_U(probe.rejected[1], rejected);
		handshake_cleanup(&ctx);
	}
	return TEST_PASS;
}

static int
test_auth_server_phase_gate(void)
{
	const uint8_t messages[] = { 80, 81, 82, 90, 94, 200, 255 };
	for (unsigned stage = 0; stage < 3; stage++) {
		for (size_t i = 0; i < sizeof(messages); i++) {
			struct handshake_ctx ctx;
			if (handshake_setup(&ctx) < 0) {
				handshake_cleanup(&ctx);
				return TEST_FAIL;
			}
			struct auth_state_probe probe = { 0 };
			struct global_probe global = { .sess = ctx.server };
			ASSERT_OK(dssh_session_set_global_request_cb(ctx.server, global_probe_cb, &global));
			struct dssh_auth_server_cbs cbs = {
				.methods_str = "keyboard-interactive", .none_cb = auth_state_none,
				.keyboard_interactive_cb = auth_state_kbi, .cbdata = &probe,
			};
			if (stage > 0)
				ASSERT_OK(queue_auth_service(ctx.client, false));
			if (stage == 2)
				ASSERT_OK(queue_auth_state_request(ctx.client, "first", "keyboard-interactive"));
			uint8_t packet[10] = { messages[i] };
			size_t pos = 1;
			if (messages[i] == SSH_MSG_GLOBAL_REQUEST) {
				DSSH_PUT_U32(4, packet, &pos);
				memcpy(&packet[pos], "test", 4);
				pos += 4;
				packet[pos++] = 1;
			}
			ASSERT_OK(send_packet(ctx.client, packet, pos, NULL));
			mock_io_close_c2s_write(&ctx.io);
			ASSERT_ERR(auth_server_impl(ctx.server, &cbs, NULL, NULL), DSSH_ERROR_PARSE);
			ASSERT_FALSE(ctx.server->server_auth_active);
			ASSERT_TRUE(dssh_session_is_terminated(ctx.server));
			ASSERT_EQ_U(probe.none_calls, 0);
			ASSERT_EQ_U(probe.kbi_calls, stage == 2 ? 1 : 0);
			ASSERT_EQ_U(global.calls, 0);
			uint8_t type;
			uint8_t *payload;
			size_t length;
			if (stage > 0) {
				ASSERT_OK(recv_packet(ctx.client, &type, &payload, &length));
				ASSERT_EQ(type, SSH_MSG_SERVICE_ACCEPT);
			}
			if (stage == 2) {
				ASSERT_OK(recv_packet(ctx.client, &type, &payload, &length));
				ASSERT_EQ(type, SSH_MSG_USERAUTH_INFO_REQUEST);
			}
			ASSERT_ERR(recv_packet(ctx.client, &type, &payload, &length), DSSH_ERROR_TERMINATED);
			handshake_cleanup(&ctx);
		}
	}
	return TEST_PASS;
}

static int
test_auth_client_unknown_messages(void)
{
	for (unsigned method = 0; method < 4; method++) {
		struct handshake_ctx ctx;
		if (handshake_setup(&ctx) < 0) {
			handshake_cleanup(&ctx);
			return TEST_FAIL;
		}
		uint32_t rejected[3];
		uint8_t unknown[] = { 9, 59, 200 };
		ASSERT_OK(send_packet(ctx.server, &unknown[0], 1, &rejected[0]));
		ASSERT_OK(queue_auth_service(ctx.server, true));
		for (size_t i = 1; i < sizeof(unknown); i++)
			ASSERT_OK(send_packet(ctx.server, &unknown[i], 1, &rejected[i]));
		uint8_t success = SSH_MSG_USERAUTH_SUCCESS;
		ASSERT_OK(send_packet(ctx.server, &success, 1, NULL));
		mock_io_close_s2c_write(&ctx.io);
		int ret;
		char methods[64];
		if (method == 0)
			ret = test_auth_discover_component(ctx.client, "user", methods, sizeof(methods));
		else if (method == 1)
			ret = auth_password_impl(ctx.client, "user", "password", NULL, NULL);
		else if (method == 2)
			ret = auth_kbi_impl(ctx.client, "user", unused_control_prompt, NULL);
		else
			ret = auth_publickey_impl(ctx.client, "user", test_key_algo_name());
		ASSERT_OK(ret);
		ASSERT_FALSE(dssh_session_is_terminated(ctx.client));
		ASSERT_TRUE(ctx.client->auth_service_requested);
		mock_io_close_c2s_write(&ctx.io);
		struct auth_state_probe probe = { 0 };
		ASSERT_OK(dssh_session_set_unimplemented_cb(ctx.server, auth_state_unimplemented, &probe));
		uint8_t type;
		uint8_t *payload;
		size_t length;
		ASSERT_OK(recv_packet(ctx.server, &type, &payload, &length));
		ASSERT_EQ(type, SSH_MSG_SERVICE_REQUEST);
		ASSERT_OK(recv_packet(ctx.server, &type, &payload, &length));
		ASSERT_EQ(type, SSH_MSG_USERAUTH_REQUEST);
		ASSERT_EQ_U(probe.unimplemented_calls, 1);
		ASSERT_TRUE(recv_packet(ctx.server, &type, &payload, &length) < 0);
		ASSERT_TRUE(dssh_session_is_terminated(ctx.server));
		ASSERT_EQ_U(probe.unimplemented_calls, 3);
		for (size_t i = 0; i < 3; i++)
			ASSERT_EQ_U(probe.rejected[i], rejected[i]);
		handshake_cleanup(&ctx);
	}
	return TEST_PASS;
}

static int
test_auth_replayed_message_state(void)
{
	for (unsigned mode = 0; mode < 2; mode++) {
		struct handshake_ctx ctx;
		if (handshake_setup(&ctx) < 0) {
			handshake_cleanup(&ctx);
			return TEST_FAIL;
		}
		const uint8_t unknown[] = { 59, 79, 200 };
		uint32_t rejected[3];
		for (size_t i = 0; i < sizeof(unknown); i++)
			ASSERT_OK(send_packet(ctx.client, &unknown[i], 1, &rejected[i]));
		const char *names[10] = {
			ctx.client->trans.kex_selected->name, ctx.client->trans.key_algo_selected->name,
			ctx.client->trans.enc_c2s_selected->name, ctx.client->trans.enc_s2c_selected->name,
			ctx.client->trans.mac_c2s_selected->name, ctx.client->trans.mac_s2c_selected->name,
			ctx.client->trans.comp_c2s_selected->name, ctx.client->trans.comp_s2c_selected->name,
			"", "",
		};
		uint8_t offer[1024];
		size_t offer_len = make_peer_offer(names, 0, offer, sizeof(offer));
		ASSERT_TRUE(offer_len > 0);
		ASSERT_OK(send_packet(ctx.client, offer, offer_len, NULL));
		free(ctx.server->trans.peer_kexinit);
		ctx.server->trans.peer_kexinit = NULL;
		ctx.server->trans.peer_kexinit_sz = 0;
		ctx.server->trans.rekey_in_progress = true;
		ASSERT_OK(kexinit(ctx.server));
		ctx.server->trans.rekey_in_progress = false;
		ASSERT_EQ_U(ctx.server->trans.rekey_queue_count, 3);
		/* Only KEXINIT buffering/negotiation is exercised here. Consume the
		 * outgoing KEXINIT without initiating an unrelated full rekey. */
		uint8_t type;
		uint8_t *payload;
		size_t length;
		ctx.client->trans.rekey_in_progress = true;
		ASSERT_OK(recv_packet(ctx.client, &type, &payload, &length));
		ctx.client->trans.rekey_in_progress = false;
		ASSERT_EQ(type, SSH_MSG_KEXINIT);
		struct auth_state_probe probe = { 0 };
		ASSERT_OK(dssh_session_set_unimplemented_cb(ctx.client, auth_state_unimplemented, &probe));
		if (mode == 0) {
			for (size_t i = 0; i < sizeof(unknown); i++) {
				ASSERT_OK(recv_packet(ctx.server, &type, &payload, &length));
				ASSERT_EQ(type, unknown[i]);
				ASSERT_EQ_U(ctx.server->trans.last_rx_seq, rejected[i]);
				ASSERT_OK(send_unimplemented(ctx.server, ctx.server->trans.last_rx_seq));
			}
			uint8_t marker = SSH_MSG_USERAUTH_SUCCESS;
			ASSERT_OK(send_packet(ctx.server, &marker, 1, NULL));
			ASSERT_OK(recv_packet(ctx.client, &type, &payload, &length));
			ASSERT_EQ(type, marker);
			ASSERT_EQ_U(probe.unimplemented_calls, 3);
		}
		else {
			struct dssh_auth_server_cbs cbs = { .methods_str = "password" };
			ASSERT_ERR(auth_server_impl(ctx.server, &cbs, NULL, NULL), DSSH_ERROR_PARSE);
			ASSERT_TRUE(dssh_session_is_terminated(ctx.server));
			ASSERT_FALSE(ctx.server->server_auth_active);
			ASSERT_ERR(recv_packet(ctx.client, &type, &payload, &length), DSSH_ERROR_TERMINATED);
			ASSERT_EQ_U(probe.unimplemented_calls, 2);
		}
		for (unsigned i = 0; i < probe.unimplemented_calls; i++)
			ASSERT_EQ_U(probe.rejected[i], rejected[i]);
		ASSERT_EQ_U(ctx.server->trans.rekey_queue_count, 0);
		ASSERT_EQ_U(ctx.server->trans.rekey_queue_bytes, 0);
		handshake_cleanup(&ctx);
	}
	return TEST_PASS;
}

#include "dh_group_result_cases.h"
#include "transport_identification_cases.h"
#include "transport_query_cases.h"
#include "transport_wire_trace_cases.h"
#include "transport_compression_cases.h"
#include "kex_io_cases.h"
#include "transport_module_cases.h"

struct rekey_control_probe {
	struct handshake_ctx *pair;
	dssh_session peer;
	unsigned kind;
	unsigned debug_calls;
	unsigned unimplemented_calls;
	unsigned global_calls;
	unsigned requests;
	atomic_bool finished;
	atomic_bool timed_out;
	bool callback_valid;
	bool peer_valid;
	atomic_uint_least32_t before_sequence;
	atomic_uint_least32_t after_sequence;
};

static void
rekey_control_debug(bool display, const uint8_t *message, size_t length, void *opaque)
{
	struct rekey_control_probe *p = opaque;
	p->debug_calls++;
	p->callback_valid = p->callback_valid && display && length == DSSH_STRLEN("control")
	    && memcmp(message, "control", length) == 0;
}

static void
rekey_control_unimplemented(uint32_t rejected, void *opaque)
{
	struct rekey_control_probe *p = opaque;
	p->unimplemented_calls++;
	p->callback_valid = p->callback_valid && rejected == UINT32_C(0x12345678);
}

static int
rekey_control_global(const uint8_t *name, size_t name_len, bool want_reply,
    const uint8_t *data, size_t data_len, void *opaque)
{
	struct rekey_control_probe *p = opaque;
	p->global_calls++;
	p->callback_valid = p->callback_valid && name_len == DSSH_STRLEN("rekey-test")
	    && memcmp(name, "rekey-test", name_len) == 0
	    && (p->requests == 1 ? data_len == 0 : data_len == 1 && data[0] == p->global_calls)
	    && want_reply == (p->kind != 3);
	return p->kind == 5 && !(data_len == 1 && data[0] == 2) ? 0 : DSSH_ERROR_REJECTED;
}

static int
rekey_control_watchdog(void *opaque)
{
	struct rekey_control_probe *p = opaque;
	unsigned attempts = getenv("DSSH_TEST_REKEY_BASELINE") != NULL ? 1U : 6U;
	for (unsigned i = 0; i < attempts; i++) {
		if (dssh_test_wait_bool(&p->finished, true))
			return 0;
	}
	atomic_store(&p->timed_out, true);
	dssh_session_terminate(p->pair->client);
	dssh_session_terminate(p->pair->server);
	mock_io_close_c2s(&p->pair->io);
	mock_io_close_s2c(&p->pair->io);
	return 0;
}

static int
rekey_control_peer(void *opaque)
{
	struct rekey_control_probe *p = opaque;
	uint8_t control[32];
	size_t pos = 0;
	if (p->kind == 0) {
		control[pos++] = SSH_MSG_IGNORE;
		DSSH_PUT_U32(0, control, &pos);
	}
	else if (p->kind == 1) {
		control[pos++] = SSH_MSG_DEBUG;
		control[pos++] = 1;
		DSSH_PUT_U32(DSSH_STRLEN("control"), control, &pos);
		memcpy(control + pos, "control", DSSH_STRLEN("control"));
		pos += DSSH_STRLEN("control");
		DSSH_PUT_U32(0, control, &pos);
	}
	else if (p->kind == 2) {
		control[pos++] = SSH_MSG_UNIMPLEMENTED;
		DSSH_PUT_U32(UINT32_C(0x12345678), control, &pos);
	}
	else {
		control[pos++] = SSH_MSG_GLOBAL_REQUEST;
		DSSH_PUT_U32(DSSH_STRLEN("rekey-test"), control, &pos);
		memcpy(control + pos, "rekey-test", DSSH_STRLEN("rekey-test"));
		pos += DSSH_STRLEN("rekey-test");
		control[pos++] = p->kind == 3 ? 0 : 1;
	}
	const uint8_t before[] = {SSH_MSG_CHANNEL_DATA, 0, 0, 0, 0, 0, 0, 0, 1, 0x51};
	const uint8_t after[] = {SSH_MSG_CHANNEL_DATA, 0, 0, 0, 0, 0, 0, 0, 1, 0x52};
	bool valid = true;
	for (unsigned i = 0; i < p->requests && valid; i++) {
		size_t length = pos;
		if (p->requests > 1) {
			if (pos >= sizeof(control) || i >= UINT8_MAX) {
				valid = false;
				break;
			}
			uint8_t index = (uint8_t)(i + 1);
			control[length++] = index;
		}
		valid = send_packet(p->peer, control, length, NULL) == 0;
	}
	atomic_store(&p->before_sequence, p->peer->trans.tx_seq);
	if (valid)
		valid = send_packet(p->peer, before, sizeof(before), NULL) == 0;
	unsigned replies = 0;
	while (valid) {
		uint8_t type;
		uint8_t *packet;
		size_t length;
		int ret = recv_packet(p->peer, &type, &packet, &length);
		if (ret != 0) {
			valid = false;
			break;
		}
		if (type == SSH_MSG_REQUEST_FAILURE || type == SSH_MSG_REQUEST_SUCCESS) {
			replies++;
			valid = p->kind >= 4 && replies <= p->requests && length == 1
			    && type == (p->kind == 5 && !(p->requests > 1 && replies == 2)
		        ? SSH_MSG_REQUEST_SUCCESS : SSH_MSG_REQUEST_FAILURE);
			continue;
		}
		valid = type == SSH_MSG_CHANNEL_DATA && length == sizeof(before)
		    && packet[9] == 0x61 && replies == (p->kind >= 4 ? p->requests : 0U);
		break;
	}
	atomic_store(&p->after_sequence, p->peer->trans.tx_seq);
	if (valid)
		valid = send_packet(p->peer, after, sizeof(after), NULL) == 0;
	if (valid) {
		uint8_t type;
		uint8_t *packet;
		size_t length;
		valid = recv_packet(p->peer, &type, &packet, &length) == 0
		    && type == SSH_MSG_CHANNEL_DATA && length == sizeof(before) && packet[9] == 0x62;
	}
	p->peer_valid = valid;
	if (!valid) {
		dssh_session_terminate(p->pair->client);
		dssh_session_terminate(p->pair->server);
		mock_io_close_c2s(&p->pair->io);
		mock_io_close_s2c(&p->pair->io);
	}
	return 0;
}

static bool
rekey_control_exchange(struct handshake_ctx *pair, bool client, unsigned kind, unsigned requests)
{
	dssh_session receiver = client ? pair->client : pair->server;
	struct rekey_control_probe p = {.pair = pair, .peer = client ? pair->server : pair->client,
	    .kind = kind, .requests = requests, .callback_valid = true};
	atomic_init(&p.finished, false);
	atomic_init(&p.timed_out, false);
	atomic_init(&p.before_sequence, 0);
	atomic_init(&p.after_sequence, 0);
	bool valid = dssh_session_set_debug_cb(receiver, rekey_control_debug, &p) == 0
	    && dssh_session_set_unimplemented_cb(receiver, rekey_control_unimplemented, &p) == 0
	    && dssh_session_set_global_request_cb(receiver, rekey_control_global, &p) == 0;
	/* The sole receiver is not active yet. Alternate packet/byte triggers;
	 * the byte seam updates the actual RX counter at the next packet boundary. */
	if (kind % 2 == 0)
		receiver->trans.rx_since_rekey = DSSH_REKEY_SOFT_LIMIT - 1;
	else {
		dssh_enc enc = receiver->trans.client ? receiver->trans.enc_s2c_active : receiver->trans.enc_c2s_active;
		dssh_test_arm_rx_bytes_before_count(receiver, enc->bytes_per_key);
	}
	thrd_t thread, watchdog;
	bool watched = valid && thrd_create(&watchdog, rekey_control_watchdog, &p) == thrd_success;
	bool started = watched && thrd_create(&thread, rekey_control_peer, &p) == thrd_success;
	valid = valid && started;
	uint8_t type;
	uint8_t *packet;
	size_t length;
	if (valid)
		valid = recv_packet(receiver, &type, &packet, &length) == 0
		    && type == SSH_MSG_CHANNEL_DATA && length == 10 && packet[9] == 0x51
		    && receiver->trans.rx_since_rekey == 0
		    && receiver->trans.tx_since_rekey == (kind >= 4 ? requests - 1 : 0)
		    && !receiver->trans.rekey_pending && receiver->trans.rekey_queue_count == 0
		    && receiver->trans.last_rx_seq == atomic_load(&p.before_sequence);
	uint8_t ack[] = {SSH_MSG_CHANNEL_DATA, 0, 0, 0, 0, 0, 0, 0, 1, 0x61};
	if (valid)
		valid = send_packet(receiver, ack, sizeof(ack), NULL) == 0;
	if (valid)
		valid = recv_packet(receiver, &type, &packet, &length) == 0
		    && type == SSH_MSG_CHANNEL_DATA && length == 10 && packet[9] == 0x52
		    && receiver->trans.rx_since_rekey == 1
		    && receiver->trans.last_rx_seq == atomic_load(&p.after_sequence);
	ack[9] = 0x62;
	if (valid)
		valid = send_packet(receiver, ack, sizeof(ack), NULL) == 0;
	if (!valid) {
		mock_io_close_c2s(&pair->io);
		mock_io_close_s2c(&pair->io);
	}
	atomic_store(&p.finished, true);
	if (started && thrd_join(thread, NULL) != thrd_success)
		abort();
	if (watched && thrd_join(watchdog, NULL) != thrd_success)
		abort();
	valid = valid && !atomic_load(&p.timed_out) && p.peer_valid && p.callback_valid
	    && p.debug_calls == (kind == 1 ? 1U : 0U)
	    && p.unimplemented_calls == (kind == 2 ? 1U : 0U)
	    && p.global_calls == (kind >= 3 ? requests : 0U)
	    && !dssh_session_is_terminated(receiver) && !dssh_session_is_terminated(p.peer);
	valid = dssh_session_set_debug_cb(receiver, NULL, NULL) == 0 && valid;
	valid = dssh_session_set_unimplemented_cb(receiver, NULL, NULL) == 0 && valid;
	valid = dssh_session_set_global_request_cb(receiver, NULL, NULL) == 0 && valid;
	if (!valid)
		fprintf(stderr, "rekey control role=%u kind=%u peer=%u\n", client ? 1U : 0U, kind, p.peer_valid ? 1U : 0U);
	return valid;
}

static int
rekey_control_cases(bool pipeline)
{
	const char *compression = getenv("DSSH_TEST_REKEY_COMP");
	for (unsigned role = 0; role < 2; role++) {
		struct handshake_ctx ctx;
		int ret;
		if (compression != NULL && strcmp(compression, "test-growth") == 0) {
			ASSERT_OK(dssh_test_comp_reset());
			ret = compression_handshake(&ctx);
		}
		else if (compression != NULL && strcmp(compression, "zlib") == 0)
			ret = compression_handshake_with(&ctx, dssh_test_register_zlib_comp);
		else
			ret = handshake_setup(&ctx);
		bool valid = ret == 0;
		if (valid)
			valid = dssh_session_set_timeout(ctx.client, getenv("DSSH_TEST_REKEY_BASELINE") != NULL ? 1000 : 30000) == 0
			    && dssh_session_set_timeout(ctx.server, getenv("DSSH_TEST_REKEY_BASELINE") != NULL ? 1000 : 30000) == 0;
		for (unsigned kind = pipeline ? 3U : 0U; kind < 6 && valid; kind++)
			valid = rekey_control_exchange(&ctx, role == 0, kind, pipeline ? 3U : 1U);
		handshake_cleanup(&ctx);
		ASSERT_TRUE(valid);
	}
	printf("rekey controls: %u exchanges with ordered replay verified\n", pipeline ? 6U : 12U);
	return TEST_PASS;
}

static int
test_rekey_control_traffic(void)
{
	return rekey_control_cases(false);
}

static int
test_rekey_global_pipeline(void)
{
	return rekey_control_cases(true);
}

#include "key_material_cases.h"
#include "receive_entry_cases.h"
#include "transport_ext_info_cases.h"
#include "language_cases.h"
#include "key_borrow_cases.h"
#include "gather_batch_cases.h"

static struct dssh_test_entry tests[] = {
	{ "state/context_refusals", test_state_context_refusals },
	{ "negotiate/offer_replace", test_negotiation_offer_replace },
	{ "direct_kex/fixture", test_direct_kex_fixture },
	{ "direct_kex/builder", test_direct_packet_builder },
	{ "rekey/owned_roundtrip", test_rekey_owned_roundtrip },
	{ "rekey/sequence_oracle", test_rekey_sequence_oracle },
#if defined(DSSH_TRANSPORT_REKEY_CONTROLS)
	{ "fixture/ownership", test_transport_all_rekey_ownership },
#elif defined(DSSH_TRANSPORT_PAIR_CONTROLS)
	{ "fixture/ownership", test_clock_or_all_pair_ownership },
#endif
	{ "release/test_layout", test_transport_test_layout },
	{ "gather/batch", test_gather_batch },
	{ "gather/batch_budget", test_gather_batch_budget },
	{ "gather/batch_failures", test_gather_batch_failures },
	{ "gather/batch_allocation", test_gather_batch_allocation },
	{ "gather/batch_refill", test_gather_batch_refill },
	{ "release/key_borrow_kex", test_key_borrow_kex },
	{ "release/key_borrow_auth", test_key_borrow_auth },
	{ "language/local", test_language_local },
	{ "language/bounds", test_language_bounds },
	{ "language/concurrent", test_language_concurrent },
	{ "language/entry_failures", test_language_entry_failures },
	{ "language/storage", test_language_storage },
	{ "language/wire", test_language_wire },
	{ "language/handshake_rekey", test_language_handshake_rekey },
	{ "ext_info/codec", test_ext_info_codec },
	{ "ext_info/packet_bound", test_ext_info_packet_bound },
	{ "ext_info/offers", test_ext_info_offers },
	{ "ext_info/advertisement", test_ext_info_advertisement },
	{ "ext_info/receive_positions", test_ext_info_receive_positions },
	{ "ext_info/receive_rejections", test_ext_info_receive_rejections },
	{ "ext_info/send_failure", test_ext_info_send_failure },
	{ "receive_entry/notifications", test_receive_entry_notifications },
	{ "receive_entry/auth_callbacks", test_receive_entry_auth_callbacks },
	{ "receive_entry/hostkey", test_receive_entry_hostkey },
	{ "receive_entry/nested", test_receive_entry_nested },
	{ "receive_entry/demux", test_receive_entry_demux },
	{ "receive_entry/joinable", test_receive_entry_joinable },
	{ "key_material/encoding", test_key_material_encoding },
	{ "key_material/bounds", test_key_material_bounds },
	{ "key_material/zero_output", test_key_material_zero_output },
	{ "key_material/known_answers", test_key_material_known_answers },
	{ "key_material/paired_producer", test_key_material_paired_producer },
	{ "key_material/zero_module", test_key_material_zero_module },
	{ "key_material/hash_changes", test_key_material_hash_changes },
	{ "rekey/global_pipeline", test_rekey_global_pipeline },
	{ "rekey/control_traffic", test_rekey_control_traffic },
	{ "release/dhgex_provider_contract", test_dhgex_provider_contract },
	{ "release/dhgex_group_result_handshake", test_dhgex_group_result_handshake },
	{ "release/dhgex_group_result_reuse", test_dhgex_group_result_reuse },
	{ "modules/stateless", test_modules_stateless },
	{ "compression/expansion", test_compression_expansion },
	{ "compression/context_replacement", test_compression_context_replacement },
	{ "compression/auth_cleanup", test_compression_auth_cleanup },
	{ "compression/sensitive_failure", test_compression_sensitive_failure },
	{ "compression/teardown_cleanup", test_compression_teardown_cleanup },
	{ "compression/queued_rekey", test_compression_queued_rekey },
	{ "compression/scratch_failures", test_compression_scratch_failures },
	{ "compression/replay_cleanup", test_compression_replay_cleanup },
	{ "compression/callbacks", test_compression_callbacks },
	{ "compression/newkeys_framing", test_compression_newkeys_framing },
	{ "compression/mac_before_decode", test_compression_mac_before_decode },
	{ "compression/zlib_bounds", test_compression_zlib_bounds },
	{ "compression/gather_slots", test_compression_gather_slots },
	{ "compression/provider_changes", test_compression_provider_changes },
	{ "compression/setup_failures", test_compression_setup_failures },
	{ "compression/registration", test_compression_registration },
	{ "compression/zlib_allocations", test_compression_zlib_allocations },
	{ "compression/rekey_reuse", test_compression_rekey_reuse },
	{ "compression/transform_failures", test_compression_transform_failures },
	{ "compression/zlib", test_compression_zlib },
	{ "identification/grammar", test_identification_grammar },
	{ "identification/bytes", test_identification_bytes },
	{ "identification/bounds", test_identification_bounds },
	{ "identification/setters", test_identification_setters },
	{ "identification/output", test_identification_output },
	{ "release/query_arguments", test_query_arguments },
	{ "release/query_snapshot", test_query_snapshot },
	{ "release/auth_replayed_message_state", test_auth_replayed_message_state },
	{ "release/auth_kbi_request_state", test_auth_kbi_request_state },
	{ "auth_state/wire_builder", test_auth_state_builder },
	{ "auth_state/queue_wire", test_auth_state_queue_wire },
	{ "auth_state/queue_constructors", test_auth_state_queue_constructors },
	{ "release/auth_server_phase_gate", test_auth_server_phase_gate },
	{ "release/auth_client_unknown_messages", test_auth_client_unknown_messages },
	{ "release/auth_control_reply_errors", test_auth_control_reply_errors },
	{ "release/disconnect_validation", test_disconnect_validation },
	{ "release/disconnect_queue_order", test_disconnect_queue_order },
	{ "release/global_request_validation", test_global_request_validation },
	{ "release/global_request_results", test_global_request_results },
	{ "release/clock_transport", test_clock_transport },
	{ "clock/constructors", test_clock_constructors },
	{ "release/registration_rejection", test_registration_rejection },
	{ "release/offer_haskey_error", test_offer_haskey_error },
	{ "release/full_negotiation_offers", test_negotiation_full_offers },
	{ "release/key_capability_selection", test_negotiation_key_capabilities },
	{ "release/kexinit_complete_fields", test_kexinit_complete_fields },
	{ "kexinit/builder", test_peer_offer_builder },
	{ "release/dhgex_group_selection", test_dhgex_group_selection },
	{ "release/hybrid_shared_secret_validation", test_hybrid_shared_secret_validation },
	{ "release/dhgex_group_validation", test_dhgex_group_validation },
	{ "release/dhgex_peer_validation", test_dhgex_peer_validation },
	{ "release/dhgex_mpint_validation", test_dhgex_mpint_validation },
#ifdef DSSH_CRYPTO_OPENSSL
	{ "release/dhgex_group_constants", test_dhgex_group_constants },
	{ "release/dhgex_group_failure_paths", test_dhgex_group_failure_paths },
#endif
	{ "release/c25519_secret_validation", test_c25519_secret_validation },
	{ "release/c25519_peer_validation", test_c25519_peer_validation },
	{ "release/version_count_reset", test_version_rx_count_reset },
	{ "release/callback_results", test_callback_positive_results },
	{ "release/packet_framing", test_packet_framing_boundaries },
	{ "packet/framing_constructor_refusals", test_frame_constructor_refusals },
	{ "release/rekey_changes_algorithms", test_rekey_changes_algorithms },
	{ "release/fixed_rekey_methods", test_fixed_rekey_methods },
	{ "release/version_callback_count", test_version_rx_callback_count },
	{ "release/initial_kex_out_of_phase", test_initial_kex_rejects_out_of_phase },
	{ "release/rekey_queue_bounded", test_rekey_queue_bounded },
	{ "release/server_direction_limits", test_rekey_server_direction_limits },
	/* Version validators */
	{ "version/has_nulls_true",           test_has_nulls_true },
	{ "version/has_nulls_false",          test_has_nulls_false },
	{ "version/missing_crlf_short",       test_missing_crlf_short },
	{ "version/missing_crlf_no_cr",       test_missing_crlf_no_cr },
	{ "version/missing_crlf_no_lf",       test_missing_crlf_no_lf },
	{ "version/missing_crlf_valid",       test_missing_crlf_valid },
	{ "version/is_version_line_true",     test_is_version_line_true },
	{ "version/is_version_line_false_get", test_is_version_line_false_get },
	{ "version/is_version_line_false_short", test_is_version_line_false_short },
	{ "version/has_non_ascii_ctrl",       test_has_non_ascii_ctrl },
	{ "version/has_non_ascii_high",       test_has_non_ascii_high },
	{ "version/has_non_ascii_false",      test_has_non_ascii_false },
	{ "version/is_20_true",              test_is_20_true },
	{ "version/is_20_true_199",          test_is_20_true_199 },
	{ "version/is_20_false_10",          test_is_20_false_10 },
	{ "version/is_20_false_short",       test_is_20_false_short },

	/* Algorithm negotiation */
	{ "algo/negotiate_first_client_match", test_negotiate_first_client_match },
	{ "algo/negotiate_no_match",          test_negotiate_no_match },
	{ "algo/negotiate_client_priority",   test_negotiate_client_priority },
	{ "algo/negotiate_single_match",      test_negotiate_single_match },
	{ "algo/negotiate_not_registered",    test_negotiate_not_registered },
	{ "algo/build_namelist_single",       test_build_namelist_single },
	{ "algo/build_namelist_multiple",     test_build_namelist_multiple },
	{ "algo/build_namelist_empty",        test_build_namelist_empty },
	{ "filter/drops_algo",                test_filter_drops_algo },
	{ "filter/reorders",                  test_filter_reorders },
	{ "filter/skips_unknown",             test_filter_skips_unknown },
	{ "filter/null_uses_all",             test_filter_null_uses_all },
	{ "filter/name_in_filter_basic",      test_name_in_filter_basic },
	{ "filter/negotiate_blocks",          test_negotiate_filter_blocks },
	{ "filter/setter_comma_rejected",     test_filter_setter_comma_rejected },
	{ "filter/setter_null_empty_rejected", test_filter_setter_null_and_empty_rejected },
	{ "filter/setter_replace",            test_filter_setter_replace },
	{ "filter/setter_toolate",            test_filter_setter_toolate },
	{ "filter/setter_null_session",       test_filter_setter_null_session },

	/* Key derivation */
	{ "derive/deterministic",             test_derive_key_deterministic },
	{ "derive/different_letters",         test_derive_key_different_letters },
	{ "derive/extension_loop",            test_derive_key_extension_loop },
	{ "derive/bad_hash",                  test_derive_key_bad_hash },

	/* Version exchange */
	{ "vex/basic",                        test_version_exchange_basic },
	{ "vex/remote_stored",               test_version_exchange_remote_stored },
	{ "vex/reject_non_20",               test_version_exchange_reject_non_20 },
	{ "vex/accept_199",                  test_version_exchange_accept_199 },
	{ "vex/extra_lines",                 test_version_exchange_extra_lines },
	{ "vex/reject_non_ascii",            test_version_exchange_reject_non_ascii },

	/* Packet send/recv */
	{ "packet/roundtrip",                test_packet_roundtrip },
	{ "packet/alignment",                test_packet_alignment },
	{ "packet/min_padding",              test_packet_min_padding },
	{ "packet/seq_increment",            test_packet_seq_increment },
	{ "packet/server_receives_client",   test_packet_server_receives_client },
	{ "packet/too_large",                test_packet_too_large },
	{ "packet/bidirectional",            test_packet_bidirectional },
	{ "packet/minimum_payload",          test_packet_minimum_payload },
	{ "packet/wire_oracle", test_packet_wire_oracle },
	{ "packet/wire_constructor_refusals", test_packet_wire_constructor_refusals },

	/* Transport message handling */
	{ "transport/ignore_skipped",        test_ignore_silently_skipped },
	{ "transport/disconnect_terminates", test_disconnect_sets_terminate },
	{ "transport/debug_callback",        test_debug_invokes_callback },
	{ "transport/log_forwards_debug",    test_log_forwards_debug },
	{ "transport/wire_trace",            test_transport_wire_trace },
	{ "release/kex_io_arguments",        test_kex_io_arguments },
	{ "transport/unimplemented_callback", test_unimplemented_invokes_callback },
	{ "transport/multiple_ignore",       test_multiple_ignore_before_real },

	/* Full handshake */
	{ "handshake/completes",             test_handshake_completes },
	{ "handshake/negotiated_algorithms", test_handshake_negotiated_algorithms },
	{ "handshake/session_id_set",        test_handshake_session_id_set },
	{ "handshake/encrypted_roundtrip",   test_handshake_encrypted_roundtrip },

	/* Rekey */
	{ "rekey/needed_false_initially",    test_rekey_needed_false_initially },
	{ "rekey/needed_tx_packets",         test_rekey_needed_tx_packets },
	{ "rekey/needed_rx_packets",         test_rekey_needed_rx_packets },
	{ "rekey/needed_bytes",              test_rekey_needed_bytes },
	{ "rekey/optout_policy",             test_rekey_optout_policy },
	{ "rekey/optout_packets",            test_rekey_optout_packets },
	{ "rekey/needed_time",               test_rekey_needed_time },
	{ "rekey/seconds_disabled",          test_rekey_seconds_disabled },
	{ "rekey/needed_below_threshold",    test_rekey_needed_below_threshold },
	{ "rekey/bytes_per_direction",       test_rekey_bytes_per_direction },

	/* RFC conformance */
	{ "rekey/hard_limit_send",           test_rekey_hard_limit_send },
	{ "rekey/hard_limit_recv",           test_rekey_hard_limit_recv },
	{ "handshake/mac_active",            test_handshake_mac_active },

	/* Session lifecycle */
	{ "session/init_cleanup",            test_session_init_cleanup },
	{ "session/terminate",               test_session_terminate },
	{ "session/cleanup_null",            test_session_cleanup_null },

	/* set_version validation */
	{ "set_version/valid",               test_set_version_valid },
	{ "set_version/valid_with_comment",  test_set_version_valid_comment },
	{ "set_version/null_version",        test_set_version_null },
	{ "set_version/null_with_comment",   test_set_version_null_with_comment },
	{ "set_version/empty_version",       test_set_version_empty },
	{ "set_version/empty_comment",       test_set_version_empty_comment },
	{ "set_version/space_in_version",    test_set_version_space },
	{ "set_version/space_in_comment",    test_set_version_comment_space },
	{ "set_version/ctrl_in_comment",     test_set_version_comment_ctrl },
	{ "set_version/ctrl_in_version",     test_set_version_ctrl },
	{ "set_version/high_byte",           test_set_version_high },
	{ "set_version/too_long",            test_set_version_too_long },
	{ "set_version/after_session",       test_set_version_after_session },

	/* Error paths (coverage) */
	{ "vex/with_comment",                test_version_exchange_with_comment },
	{ "vex/rx_error",                    test_version_exchange_rx_error },
	{ "vex/terminate_mid_rx",            test_version_exchange_terminate },
	{ "vex/extra_line_error",            test_version_exchange_extra_line_error },
	{ "disconnect/long_description",     test_disconnect_long_description },
	{ "packet/recv_bad_padding",         test_packet_recv_bad_padding },
	{ "packet/recv_too_small",           test_packet_recv_too_small },

	/* Registration validation */
	{ "register/fixture_refusal", test_registration_fixture_refusal },
	{ "register/usable_probes", test_registration_usable_probes },
	{ "release/registration_module_allocation", test_registration_module_allocation },
	{ "register/kex_toolate",            test_register_kex_toolate },
	{ "register/kex_empty_name",         test_register_kex_empty_name },
	{ "register/kex_next_not_null",      test_register_kex_next_not_null },
	{ "register/enc_toolate",            test_register_enc_toolate },
	{ "register/mac_toolate",            test_register_mac_toolate },
	{ "register/comp_empty_name",        test_register_comp_empty_name },
	{ "register/key_algo_toolate",       test_register_key_algo_toolate },
	{ "register/set_ctx_toolate",        test_set_ctx_toolate },
	{ "register/key_algo_toolong",       test_register_key_algo_toolong },
	{ "register/key_algo_next_not_null", test_register_key_algo_next_not_null },
	{ "register/enc_toolong",            test_register_enc_toolong },
	{ "register/enc_next_not_null",      test_register_enc_next_not_null },
	{ "register/mac_toolong",            test_register_mac_toolong },
	{ "register/mac_next_not_null",      test_register_mac_next_not_null },
	{ "register/comp_toolate",           test_register_comp_toolate },
	{ "register/comp_next_not_null",     test_register_comp_next_not_null },
	{ "register/kex_toolong",            test_register_kex_toolong },
	{ "register/comp_toolong",           test_register_comp_toolong },
	{ "register/kex_toomany",            test_register_kex_toomany },
	{ "register/key_algo_toomany",       test_register_key_algo_toomany },
	{ "register/enc_toomany",            test_register_enc_toomany },
	{ "register/mac_toomany",            test_register_mac_toomany },
	{ "register/comp_toomany",           test_register_comp_toomany },

	/* Algorithm edge cases */
	{ "aes256_ctr/null_ctx",             test_aes256_ctr_null_ctx },
#ifdef DSSH_CRYPTO_OPENSSL
	{ "aes256_ctr/alloc_fail",           test_aes256_ctr_alloc_fail },
#endif
	{ "hmac_sha2_256/null_ctx",          test_hmac_sha2_256_null_ctx },
#ifdef DSSH_CRYPTO_OPENSSL
	{ "hmac_sha2_256/alloc_fail",        test_hmac_sha2_256_alloc_fail },
#endif

	/* None module coverage */
	{ "debug/msg_len_exceeds_payload",   test_debug_msg_len_exceeds_payload },
	{ "global_request/name_exceeds",     test_global_request_name_exceeds_payload },

	{ "none/comp",                       test_none_comp },
	{ "none/enc",                        test_none_enc },
	{ "none/mac",                        test_none_mac },

	/* Formerly-guarded paths */
	{ "guard/rekey_time_zero",           test_rekey_time_zero },

	{ "guard/blocksize_clamp_direct",    test_blocksize_clamp_direct },
	{ "guard/cleanup_null_cleanup_fn",   test_cleanup_null_cleanup_fn },
	{ "guard/ed25519_sign_basic",        test_ed25519_sign_basic },
	{ "guard/ed25519_pubkey_basic",      test_ed25519_pubkey_basic },
	{ "guard/rsa_sign_basic",            test_rsa_sign_basic },
	{ "guard/rsa_pubkey_basic",          test_rsa_pubkey_basic },
	{ "guard/ed25519_haskey_wrong_type", test_ed25519_haskey_wrong_type },
	{ "guard/rsa_haskey_wrong_type",     test_rsa_haskey_wrong_type },
	{ "guard/first_name_basic",          test_first_name_basic },
	{ "guard/first_name_single",         test_first_name_single },
	{ "guard/first_name_small_buf",      test_first_name_small_buf },
	{ "register/two_kex",               test_register_two_kex },
	{ "register/two_comp",              test_register_two_comp },
	{ "kexinit/parse_valid",            test_kexinit_parse_valid },
	{ "kexinit/parse_control_char",     test_kexinit_parse_control_char },
	{ "kexinit/parse_name_too_long",    test_kexinit_parse_name_too_long },
	{ "kexinit/parse_truncated",        test_kexinit_parse_truncated },
	{ "kexinit/parse_too_short",        test_kexinit_parse_too_short },
	{ "kexinit/parse_first_kex_follows", test_kexinit_parse_first_kex_follows },
	{ "kexinit/peer_trunc_namelist",    test_kexinit_peer_parse_truncated_namelist },

	/* K wire encoding */
	{ "encode_k/mpint_no_pad",          test_encode_k_mpint_no_pad },
	{ "encode_k/mpint_sign_pad",        test_encode_k_mpint_sign_pad },
	{ "encode_k/mpint_empty",           test_encode_k_mpint_empty },
	{ "encode_k/string_encoding",       test_encode_k_string },
	{ "encode_k/string_empty",          test_encode_k_string_empty },
#ifdef DSSH_CRYPTO_OPENSSL

#endif
#ifdef DSSH_CRYPTO_OPENSSL
	{ "aes256_ctr/encrypt_update_fail", test_aes256_ctr_encrypt_update_failure },
#endif
	{ "hmac_sha2_256/cleanup_null",     test_hmac_sha2_256_cleanup_null },
#ifdef DSSH_CRYPTO_OPENSSL
	{ "hmac_sha2_256/generate_failure", test_hmac_sha2_256_generate_failure },
#endif

	/* Getter before handshake */
	{ "getter/names_before_handshake",   test_get_names_before_handshake },

	/* Callbacks and session settings */
	{ "callbacks/set_after_session",     test_set_callbacks_after_session },
	{ "session/set_global_request_cb",   test_set_global_request_cb },

	/* GLOBAL_REQUEST handling */
	{ "global_request/with_reply",       test_global_request_with_reply },
	{ "global_request/rejected",         test_global_request_rejected },
	{ "global_request/no_reply",         test_global_request_no_reply },
	{ "global_request/no_callback",      test_global_request_no_callback },
	{ "global_request/truncated",        test_global_request_truncated },

	/* DEBUG/UNIMPLEMENTED edge cases */
	{ "debug/truncated_payload",         test_debug_truncated_payload },
	{ "debug/no_callback",              test_debug_no_callback },
	{ "unimplemented/short_payload",     test_unimplemented_short_payload },

	/* Version parsing edge cases */
	{ "version/is_version_ss_not_ssh",   test_is_version_line_ss_not_ssh },
	{ "version/is_20_bad_minor",         test_is_20_bad_minor },
	{ "version/is_20_199_partial",       test_is_20_199_partial },

	/* max_packet_size clamping */
	{ "init/small_packet_size",          test_init_small_packet_size },
	{ "init/large_packet_size",          test_init_large_packet_size },

	/* build_namelist overflow */
	{ "algo/build_namelist_overflow",    test_build_namelist_overflow },
	{ "algo/build_namelist_truncation",  test_build_namelist_truncation_no_trailing_comma },

	/* Cleanup partial session */
	{ "cleanup/no_handshake",            test_cleanup_no_handshake },

	/* Version exchange: long software version */
	{ "set_version/long_version_string", test_set_version_long_string },

	/* set_version high-byte validation */
	{ "set_version/high_byte_version",   test_set_version_high_byte_version },
	{ "set_version/high_byte_comment",   test_set_version_high_byte_comment },

	/* version_tx defense-in-depth */
	{ "vex/tx_toolong_version",          test_version_tx_toolong_version },
	{ "vex/tx_toolong_comment",          test_version_tx_toolong_comment },

	/* DH-GEX server handler targeted tests */
	{ "dhgex/server_null_pubkey_fn",     test_dhgex_server_null_pubkey_fn },
	{ "dhgex/server_null_sign_fn",       test_dhgex_server_null_sign_fn },
	{ "dhgex/server_recv_fail",          test_dhgex_server_recv_fail },
	{ "dhgex/server_bad_request_type",   test_dhgex_server_bad_request_type },
	{ "dhgex/server_short_request",      test_dhgex_server_short_request },
	{ "dhgex/server_null_provider",      test_dhgex_server_null_provider },
	{ "dhgex/server_provider_error",     test_dhgex_server_provider_error },
	{ "dhgex/server_bad_init_type",      test_dhgex_server_bad_init_type },
	{ "dhgex/server_e_zero",             test_dhgex_server_e_zero },
	{ "dhgex/server_recv_init_fail",     test_dhgex_server_recv_init_fail },
	{ "dhgex/server_ka_null",            test_dhgex_server_ka_null },

	/* Curve25519 server handler targeted tests */
	{ "c25519/server_ka_null",           test_c25519_server_ka_null },
	{ "c25519/server_null_pubkey_fn",    test_c25519_server_null_pubkey_fn },
	{ "c25519/server_null_sign_fn",      test_c25519_server_null_sign_fn },
	{ "c25519/server_recv_fail",         test_c25519_server_recv_fail },
	{ "c25519/server_bad_init_type",     test_c25519_server_bad_init_type },
	{ "c25519/server_bad_qc_len",        test_c25519_server_bad_qc_len },
	{ "c25519/server_qc_overrun",        test_c25519_server_qc_overrun },

	/* Negotiation failure -- deterministic NULL for each || leg */
	{ "negotiate/no_common_kex",         test_negotiate_no_common_kex },
	{ "negotiate/no_common_comp_s2c",    test_negotiate_no_common_comp_s2c },

#ifdef DSSH_CRYPTO_OPENSSL
	/* Deterministic derive_key ossl failure for each DigestUpdate */
	{ "derive_key/ossl_init",            test_derive_key_ossl_init },
	{ "derive_key/ossl_shared_secret",   test_derive_key_ossl_shared_secret },
	{ "derive_key/ossl_hash",            test_derive_key_ossl_hash },
	{ "derive_key/ossl_letter",          test_derive_key_ossl_letter },
	{ "derive_key/ossl_session_id",      test_derive_key_ossl_session_id },
	{ "derive_key/ossl_final",           test_derive_key_ossl_final },
	{ "derive_key/ossl_extension_loop",  test_derive_key_ossl_extension_loop },
#endif

	/* Deterministic version parsing */
	{ "version/is_20_199_valid",         test_is_20_version_199 },
	{ "version/is_20_199_short_buf",     test_is_20_199_short_buf },
	{ "version/is_20_199_bad_minor_d",   test_is_20_199_bad_minor_digit },
	{ "version/is_20_199_no_dash",       test_is_20_199_no_dash },
	{ "version/is_20_20_bad_dot",        test_is_20_20_bad_dot },
	{ "version/rx_too_long",             test_version_rx_too_long },
	{ "version/rx_non_ascii",            test_version_rx_non_ascii },
	{ "debug/short_payload",             test_debug_short_payload },

#ifdef DSSH_CRYPTO_OPENSSL
	/* HMAC-SHA2-256 deterministic ossl failures */
	{ "hmac_sha2_256/reinit_failure",    test_hmac_sha2_256_reinit_failure },
	{ "hmac_sha2_256/fetch_failure",     test_hmac_sha2_256_fetch_failure },
	{ "hmac_sha2_256/mac_init_failure",  test_hmac_sha2_256_mac_init_failure },
#endif

	/* Curve25519 helper function tests */
	{ "c25519/encode_ss_leading_zeros",  test_c25519_encode_shared_secret_leading_zeros },
	{ "c25519/init_allocations",          test_c25519_init_allocations },
	{ "c25519/encode_ss_storage",         test_c25519_encode_shared_secret_storage },

	/* kex_set_ctx and tx_gather coverage */
	{ "kex/set_ctx",                     test_kex_set_ctx },
	{ "kex/set_ctx_toolate",             test_kex_set_ctx_toolate },
	{ "gather/set",                      test_tx_gather_set },
	{ "gather/toolate",                  test_tx_gather_toolate },
	{ "gather/roundtrip",                test_tx_gather_roundtrip },

	/* rxline_from_rx fallback */
	{ "rxline/from_rx_handshake",        test_rxline_from_rx_handshake },
};

void
dssh_test_failure_cleanup(void)
{
	transport_pair_assertion_cleanup();
}

#define main transport_test_main
DSSH_TEST_MAIN(tests)
#undef main

int
main(int argc, char **argv)
{
#if defined(DSSH_TRANSPORT_PAIR_CONTROLS) || defined(DSSH_TRANSPORT_REKEY_CONTROLS)
	if (argc == 3 && strcmp(argv[1], "--fixture-assert") == 0) {
		if (strlen(argv[2]) != 1 || argv[2][0] < '0' || argv[2][0] > '2')
			return 2;
		unsigned phase = (unsigned)(argv[2][0] - '0');
#ifdef DSSH_TRANSPORT_REKEY_CONTROLS
		return transport_all_rekey_assertion_control(phase);
#else
		return clock_or_all_pair_assertion_control(phase);
#endif
	}
#endif
	return transport_test_main(argc, argv);
}
