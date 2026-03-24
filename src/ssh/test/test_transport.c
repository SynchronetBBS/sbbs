/*
 * test_transport.c — Transport layer tests for DeuceSSH.
 *
 * Tier 2 layer tests with mock I/O.  Covers version validators,
 * algorithm negotiation, key derivation, version exchange, binary
 * packet protocol, transport message handling, full handshake, and
 * rekeying.
 */

#include <stddef.h>
#include <string.h>
#include <time.h>
#include <threads.h>

#include "dssh_test.h"
#include "dssh_test_alloc.h"
#include "deucessh.h"
#include "deucessh-algorithms.h"
#include "ssh-trans.h"
#include "ssh-internal.h"
#include "dssh_test_internal.h"
#include "mock_io.h"
#include "test_dhgex_provider.h"


/* ================================================================
 * Dispatch callbacks for two-sided tests.
 *
 * The I/O callbacks are global — all sessions share the same
 * function pointers.  These dispatch functions inspect the session's
 * client flag to route I/O through the correct pipe direction.
 * ================================================================ */

static int
mock_tx_dispatch(uint8_t *buf, size_t bufsz,
    dssh_session sess, void *cbdata)
{
	if (sess->trans.client)
		return mock_tx_client(buf, bufsz, sess, cbdata);
	else
		return mock_tx_server(buf, bufsz, sess, cbdata);
}

static int
mock_rx_dispatch(uint8_t *buf, size_t bufsz,
    dssh_session sess, void *cbdata)
{
	if (sess->trans.client)
		return mock_rx_client(buf, bufsz, sess, cbdata);
	else
		return mock_rx_server(buf, bufsz, sess, cbdata);
}

static int
mock_rxline_dispatch(uint8_t *buf, size_t bufsz,
    size_t *bytes_received, dssh_session sess, void *cbdata)
{
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
	if (test_using_dhgex())
		res = register_dh_gex_sha256();
	else
		res = register_curve25519_sha256();
	if (res < 0)
		return res;
	res = test_register_key_algos();
	if (res < 0)
		return res;
	res = register_aes256_ctr();
	if (res < 0)
		return res;
	res = register_hmac_sha2_256();
	if (res < 0)
		return res;
	res = register_none_comp();
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
	ctx->client_result = dssh_transport_handshake(ctx->client);
	return 0;
}

static int
handshake_server_thread(void *arg)
{
	struct handshake_ctx *ctx = arg;
	ctx->server_result = dssh_transport_handshake(ctx->server);
	return 0;
}

/*
 * Set up and run a full handshake.  Returns 0 on success.
 * Caller must call handshake_cleanup() when done.
 */
static int
handshake_setup(struct handshake_ctx *ctx)
{
	memset(ctx, 0, sizeof(*ctx));

	dssh_test_reset_global_config();

	if (register_all_algorithms() < 0)
		return -1;
	if (test_generate_host_key() < 0)
		return -1;

	if (mock_io_init(&ctx->io, 0) < 0)
		return -1;

	dssh_transport_set_callbacks(mock_tx_dispatch, mock_rx_dispatch,
	    mock_rxline_dispatch, mock_extra_line_cb);

	ctx->client = dssh_session_init(true, 0);
	if (ctx->client == NULL) {
		mock_io_free(&ctx->io);
		return -1;
	}
	dssh_session_set_cbdata(ctx->client, &ctx->io, &ctx->io,
	    &ctx->io, &ctx->io);

	ctx->server = init_server_session();
	if (ctx->server == NULL) {
		dssh_session_cleanup(ctx->client);
		mock_io_free(&ctx->io);
		return -1;
	}
	dssh_session_set_cbdata(ctx->server, &ctx->io, &ctx->io,
	    &ctx->io, &ctx->io);

	thrd_t ct, st;
	if (thrd_create(&ct, handshake_client_thread, ctx) != thrd_success) {
		dssh_session_cleanup(ctx->server);
		dssh_session_cleanup(ctx->client);
		mock_io_free(&ctx->io);
		return -1;
	}
	if (thrd_create(&st, handshake_server_thread, ctx) != thrd_success) {
		dssh_session_terminate(ctx->client);
		mock_io_close_c2s(&ctx->io);
		mock_io_close_s2c(&ctx->io);
		thrd_join(ct, NULL);
		dssh_session_cleanup(ctx->server);
		dssh_session_cleanup(ctx->client);
		mock_io_free(&ctx->io);
		return -1;
	}

	thrd_join(ct, NULL);
	thrd_join(st, NULL);

	if (ctx->client_result != 0 || ctx->server_result != 0)
		return -1;

	return 0;
}

static void
handshake_cleanup(struct handshake_ctx *ctx)
{
	if (ctx->server)
		dssh_session_cleanup(ctx->server);
	if (ctx->client)
		dssh_session_cleanup(ctx->client);
	mock_io_free(&ctx->io);
	dssh_test_reset_global_config();
}

/* ================================================================
 * Version validators (~12 tests)
 * ================================================================ */

static int
test_has_nulls_true(void)
{
	uint8_t buf[] = "SSH-2.0\x00-test\r\n";
	ASSERT_TRUE(dssh_test_has_nulls(buf, sizeof(buf) - 1));
	return TEST_PASS;
}

static int
test_has_nulls_false(void)
{
	uint8_t buf[] = "SSH-2.0-test\r\n";
	ASSERT_FALSE(dssh_test_has_nulls(buf, sizeof(buf) - 1));
	return TEST_PASS;
}

static int
test_missing_crlf_short(void)
{
	uint8_t buf[] = "X";
	ASSERT_TRUE(dssh_test_missing_crlf(buf, 1));
	return TEST_PASS;
}

static int
test_missing_crlf_no_cr(void)
{
	uint8_t buf[] = "SSH-2.0-test\n";
	ASSERT_TRUE(dssh_test_missing_crlf(buf, sizeof(buf) - 1));
	return TEST_PASS;
}

static int
test_missing_crlf_no_lf(void)
{
	uint8_t buf[] = "SSH-2.0-test\r";
	ASSERT_TRUE(dssh_test_missing_crlf(buf, sizeof(buf) - 1));
	return TEST_PASS;
}

static int
test_missing_crlf_valid(void)
{
	uint8_t buf[] = "SSH-2.0-test\r\n";
	ASSERT_FALSE(dssh_test_missing_crlf(buf, sizeof(buf) - 1));
	return TEST_PASS;
}

static int
test_is_version_line_true(void)
{
	uint8_t buf[] = "SSH-2.0-test\r\n";
	ASSERT_TRUE(dssh_test_is_version_line(buf, sizeof(buf) - 1));
	return TEST_PASS;
}

static int
test_is_version_line_false_get(void)
{
	uint8_t buf[] = "GET / HTTP/1.1\r\n";
	ASSERT_FALSE(dssh_test_is_version_line(buf, sizeof(buf) - 1));
	return TEST_PASS;
}

static int
test_is_version_line_false_short(void)
{
	uint8_t buf[] = "SS";
	ASSERT_FALSE(dssh_test_is_version_line(buf, 2));
	return TEST_PASS;
}

static int
test_has_non_ascii_ctrl(void)
{
	uint8_t buf[] = "SSH-2.0-\x01test";
	ASSERT_TRUE(dssh_test_has_non_ascii(buf, sizeof(buf) - 1));
	return TEST_PASS;
}

static int
test_has_non_ascii_high(void)
{
	uint8_t buf[] = "SSH-2.0-\x80test";
	ASSERT_TRUE(dssh_test_has_non_ascii(buf, sizeof(buf) - 1));
	return TEST_PASS;
}

static int
test_has_non_ascii_false(void)
{
	uint8_t buf[] = "SSH-2.0-OpenSSH_9.0";
	ASSERT_FALSE(dssh_test_has_non_ascii(buf, sizeof(buf) - 1));
	return TEST_PASS;
}

static int
test_is_20_true(void)
{
	uint8_t buf[] = "SSH-2.0-test\r\n";
	ASSERT_TRUE(dssh_test_is_20(buf, sizeof(buf) - 1));
	return TEST_PASS;
}

static int
test_is_20_true_199(void)
{
	uint8_t buf[] = "SSH-1.99-compat\r\n";
	ASSERT_TRUE(dssh_test_is_20(buf, sizeof(buf) - 1));
	return TEST_PASS;
}

static int
test_is_20_false_10(void)
{
	uint8_t buf[] = "SSH-1.0-old\r\n";
	ASSERT_FALSE(dssh_test_is_20(buf, sizeof(buf) - 1));
	return TEST_PASS;
}

static int
test_is_20_false_short(void)
{
	uint8_t buf[] = "SSH-2.";
	ASSERT_FALSE(dssh_test_is_20(buf, 6));
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

static int
test_negotiate_first_client_match(void)
{
	struct test_algo_node c = { NULL, "aes256-ctr" };
	struct test_algo_node b = { &c, "aes128-ctr" };
	struct test_algo_node a = { &b, "chacha20" };

	size_t noff = offsetof(struct test_algo_node, name);
	void *result = dssh_test_negotiate_algo(
	    "aes128-ctr,aes256-ctr", "aes256-ctr,aes128-ctr",
	    &a, noff);
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
	void *result = dssh_test_negotiate_algo(
	    "aes256-ctr,chacha20", "3des-cbc,blowfish-cbc",
	    &a, noff);
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
	void *result = dssh_test_negotiate_algo(
	    "algo-B,algo-A", "algo-A,algo-B",
	    &a, noff);
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
	void *result = dssh_test_negotiate_algo(
	    "the-only-one", "the-only-one",
	    &a, noff);
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
	void *result = dssh_test_negotiate_algo(
	    "unregistered", "unregistered",
	    &a, noff);
	ASSERT_NULL(result);
	return TEST_PASS;
}

static int
test_build_namelist_single(void)
{
	struct test_algo_node a = { NULL, "aes256-ctr" };
	char buf[256];

	size_t noff = offsetof(struct test_algo_node, name);
	size_t len = dssh_test_build_namelist(&a, noff, buf, sizeof(buf));
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
	size_t len = dssh_test_build_namelist(&a, noff, buf, sizeof(buf));
	ASSERT_STR_EQ(buf, "aes256-ctr,hmac-sha2-256,none");
	ASSERT_EQ_U(len, strlen("aes256-ctr,hmac-sha2-256,none"));
	return TEST_PASS;
}

static int
test_build_namelist_empty(void)
{
	char buf[256];
	buf[0] = 'X';
	size_t len = dssh_test_build_namelist(NULL, 0, buf, sizeof(buf));
	ASSERT_EQ_U(len, 0);
	ASSERT_EQ(buf[0], '\0');
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
	ASSERT_OK(dssh_test_derive_key("SHA256", secret, sizeof(secret),
	    hash, sizeof(hash), 'A', session_id, sizeof(session_id),
	    out1, sizeof(out1)));
	ASSERT_OK(dssh_test_derive_key("SHA256", secret, sizeof(secret),
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
	ASSERT_OK(dssh_test_derive_key("SHA256", secret, sizeof(secret),
	    hash, sizeof(hash), 'A', session_id, sizeof(session_id),
	    out_a, sizeof(out_a)));
	ASSERT_OK(dssh_test_derive_key("SHA256", secret, sizeof(secret),
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
	ASSERT_OK(dssh_test_derive_key("SHA256", secret, sizeof(secret),
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
	ASSERT_OK(dssh_test_derive_key("SHA256", secret, sizeof(secret),
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

	int res = dssh_test_derive_key("NO_SUCH_HASH", secret, sizeof(secret),
	    hash, sizeof(hash), 'A', session_id, sizeof(session_id),
	    out, sizeof(out));
	ASSERT_TRUE(res < 0);
	return TEST_PASS;
}

/* ================================================================
 * Version exchange (~6 tests, mock I/O)
 * ================================================================ */

struct version_exchange_ctx {
	struct mock_io_state io;
	dssh_session client;
	dssh_session server;
	int client_result;
	int server_result;
};

static int
version_client_thread(void *arg)
{
	struct version_exchange_ctx *ctx = arg;
	ctx->client_result = dssh_transport_version_exchange(ctx->client);
	return 0;
}

static int
version_server_thread(void *arg)
{
	struct version_exchange_ctx *ctx = arg;
	ctx->server_result = dssh_transport_version_exchange(ctx->server);
	return 0;
}

static int
test_version_exchange_basic(void)
{
	dssh_test_reset_global_config();
	dssh_transport_set_callbacks(mock_tx_dispatch, mock_rx_dispatch,
	    mock_rxline_dispatch, mock_extra_line_cb);

	struct version_exchange_ctx ctx;
	memset(&ctx, 0, sizeof(ctx));
	ASSERT_OK(mock_io_init(&ctx.io, 0));

	/* Need to register at least something so session_init locks the registry */
	ASSERT_OK(register_none_comp());

	ctx.client = dssh_session_init(true, 0);
	ASSERT_NOT_NULL(ctx.client);
	dssh_session_set_cbdata(ctx.client, &ctx.io, &ctx.io, &ctx.io, &ctx.io);

	ctx.server = init_server_session();
	ASSERT_NOT_NULL(ctx.server);
	dssh_session_set_cbdata(ctx.server, &ctx.io, &ctx.io, &ctx.io, &ctx.io);

	thrd_t ct, st;
	ASSERT_TRUE(thrd_create(&ct, version_client_thread, &ctx) == thrd_success);
	ASSERT_TRUE(thrd_create(&st, version_server_thread, &ctx) == thrd_success);
	thrd_join(ct, NULL);
	thrd_join(st, NULL);

	ASSERT_OK(ctx.client_result);
	ASSERT_OK(ctx.server_result);

	dssh_session_cleanup(ctx.client);
	dssh_session_cleanup(ctx.server);
	mock_io_free(&ctx.io);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

static int
test_version_exchange_remote_stored(void)
{
	dssh_test_reset_global_config();
	dssh_transport_set_callbacks(mock_tx_dispatch, mock_rx_dispatch,
	    mock_rxline_dispatch, mock_extra_line_cb);

	struct version_exchange_ctx ctx;
	memset(&ctx, 0, sizeof(ctx));
	ASSERT_OK(mock_io_init(&ctx.io, 0));
	ASSERT_OK(register_none_comp());

	ctx.client = dssh_session_init(true, 0);
	ASSERT_NOT_NULL(ctx.client);
	dssh_session_set_cbdata(ctx.client, &ctx.io, &ctx.io, &ctx.io, &ctx.io);

	ctx.server = init_server_session();
	ASSERT_NOT_NULL(ctx.server);
	dssh_session_set_cbdata(ctx.server, &ctx.io, &ctx.io, &ctx.io, &ctx.io);

	thrd_t ct, st;
	ASSERT_TRUE(thrd_create(&ct, version_client_thread, &ctx) == thrd_success);
	ASSERT_TRUE(thrd_create(&st, version_server_thread, &ctx) == thrd_success);
	thrd_join(ct, NULL);
	thrd_join(st, NULL);

	ASSERT_OK(ctx.client_result);
	ASSERT_OK(ctx.server_result);

	/* Both should have captured each other's version */
	const char *client_remote = dssh_transport_get_remote_version(ctx.client);
	const char *server_remote = dssh_transport_get_remote_version(ctx.server);
	ASSERT_NOT_NULL(client_remote);
	ASSERT_NOT_NULL(server_remote);
	/* Both should start with SSH-2.0- */
	ASSERT_TRUE(strncmp(client_remote, "SSH-2.0-", 8) == 0);
	ASSERT_TRUE(strncmp(server_remote, "SSH-2.0-", 8) == 0);

	dssh_session_cleanup(ctx.client);
	dssh_session_cleanup(ctx.server);
	mock_io_free(&ctx.io);
	dssh_test_reset_global_config();
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
	ASSERT_OK(register_none_comp());

	dssh_session client = dssh_session_init(true, 0);
	ASSERT_NOT_NULL(client);
	dssh_session_set_cbdata(client, &io, &io, &io, &io);

	/* Inject a SSH-1.0 version string into the s2c pipe (what client reads) */
	const char *bad_ver = "SSH-1.0-OldServer\r\n";
	mock_io_inject(&io.s2c, (const uint8_t *)bad_ver, strlen(bad_ver));

	int res = dssh_transport_version_exchange(client);
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
	ASSERT_OK(register_none_comp());

	dssh_session client = dssh_session_init(true, 0);
	ASSERT_NOT_NULL(client);
	dssh_session_set_cbdata(client, &io, &io, &io, &io);

	/* Inject SSH-1.99 (compatible with 2.0) */
	const char *ver199 = "SSH-1.99-CompatServer\r\n";
	mock_io_inject(&io.s2c, (const uint8_t *)ver199, strlen(ver199));

	int res = dssh_transport_version_exchange(client);
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
	ASSERT_OK(register_none_comp());

	dssh_session client = dssh_session_init(true, 0);
	ASSERT_NOT_NULL(client);
	dssh_session_set_cbdata(client, &io, &io, &io, &io);

	/* RFC 4253 s4.2: server MAY send lines before version string */
	const char *banner = "Welcome to my server\r\n";
	const char *ver = "SSH-2.0-TestServer\r\n";
	mock_io_inject(&io.s2c, (const uint8_t *)banner, strlen(banner));
	mock_io_inject(&io.s2c, (const uint8_t *)ver, strlen(ver));

	int res = dssh_transport_version_exchange(client);
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
	ASSERT_OK(register_none_comp());

	dssh_session client = dssh_session_init(true, 0);
	ASSERT_NOT_NULL(client);
	dssh_session_set_cbdata(client, &io, &io, &io, &io);

	/* Version line with high byte — should be rejected */
	const char *bad = "SSH-2.0-Bad\x80Server\r\n";
	mock_io_inject(&io.s2c, (const uint8_t *)bad, strlen(bad));

	int res = dssh_transport_version_exchange(client);
	ASSERT_TRUE(res < 0);

	dssh_session_cleanup(client);
	mock_io_free(&io);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

/* ================================================================
 * Packet send/recv — unencrypted (~8 tests)
 * ================================================================ */

static int
test_packet_roundtrip(void)
{
	dssh_test_reset_global_config();
	dssh_transport_set_callbacks(mock_tx_dispatch, mock_rx_dispatch,
	    mock_rxline_dispatch, mock_extra_line_cb);
	ASSERT_OK(register_none_comp());

	struct mock_io_state io;
	ASSERT_OK(mock_io_init(&io, 0));

	dssh_session client = dssh_session_init(true, 0);
	ASSERT_NOT_NULL(client);
	dssh_session_set_cbdata(client, &io, &io, &io, &io);

	dssh_session server = init_server_session();
	ASSERT_NOT_NULL(server);
	dssh_session_set_cbdata(server, &io, &io, &io, &io);

	/* Client sends a payload */
	uint8_t payload[] = { SSH_MSG_IGNORE, 0x41, 0x42, 0x43 };
	uint32_t seq;
	ASSERT_OK(dssh_transport_send_packet(client, payload, sizeof(payload), &seq));
	ASSERT_EQ_U(seq, 0);

	/* Server receives it (use raw recv to get IGNORE too) */
	uint8_t msg_type;
	uint8_t *recv_payload;
	size_t recv_len;
	/* We need to receive raw because recv_packet skips IGNORE.
	 * Instead send a non-transport message type. */
	dssh_session_cleanup(client);
	dssh_session_cleanup(server);
	mock_io_free(&io);
	dssh_test_reset_global_config();

	/* Redo with a non-ignored message type */
	dssh_test_reset_global_config();
	dssh_transport_set_callbacks(mock_tx_dispatch, mock_rx_dispatch,
	    mock_rxline_dispatch, mock_extra_line_cb);
	ASSERT_OK(register_none_comp());

	ASSERT_OK(mock_io_init(&io, 0));

	client = dssh_session_init(true, 0);
	ASSERT_NOT_NULL(client);
	dssh_session_set_cbdata(client, &io, &io, &io, &io);

	server = init_server_session();
	ASSERT_NOT_NULL(server);
	dssh_session_set_cbdata(server, &io, &io, &io, &io);

	/* Use SSH_MSG_SERVICE_REQUEST (5) which is not auto-handled */
	uint8_t payload2[] = { SSH_MSG_SERVICE_REQUEST, 0xDE, 0xAD };
	ASSERT_OK(dssh_transport_send_packet(client, payload2, sizeof(payload2), &seq));
	ASSERT_EQ_U(seq, 0);

	ASSERT_OK(dssh_transport_recv_packet(server, &msg_type, &recv_payload, &recv_len));
	ASSERT_EQ(msg_type, SSH_MSG_SERVICE_REQUEST);
	ASSERT_EQ_U(recv_len, sizeof(payload2));
	ASSERT_MEM_EQ(recv_payload, payload2, sizeof(payload2));

	dssh_session_cleanup(client);
	dssh_session_cleanup(server);
	mock_io_free(&io);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

static int
test_packet_alignment(void)
{
	dssh_test_reset_global_config();
	dssh_transport_set_callbacks(mock_tx_dispatch, mock_rx_dispatch,
	    mock_rxline_dispatch, mock_extra_line_cb);
	ASSERT_OK(register_none_comp());

	struct mock_io_state io;
	ASSERT_OK(mock_io_init(&io, 0));

	dssh_session client = dssh_session_init(true, 0);
	ASSERT_NOT_NULL(client);
	dssh_session_set_cbdata(client, &io, &io, &io, &io);

	/* Send a small payload; verify wire format is aligned to block size (8) */
	uint8_t payload[] = { SSH_MSG_SERVICE_REQUEST };
	ASSERT_OK(dssh_transport_send_packet(client, payload, sizeof(payload), NULL));

	/* Drain the c2s pipe and inspect wire format */
	uint8_t wire[256];
	size_t drained = mock_io_drain(&io.c2s, wire, sizeof(wire));

	/* Total packet must be a multiple of block size (8, unencrypted) */
	/* Wire: 4 (packet_length) + padding_length(1) + payload + padding */
	uint32_t packet_length;
	dssh_parse_uint32(wire, 4, &packet_length);
	size_t total = 4 + packet_length;
	ASSERT_EQ_U(drained, total);
	ASSERT_TRUE(total % 8 == 0);

	dssh_session_cleanup(client);
	mock_io_free(&io);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

static int
test_packet_min_padding(void)
{
	dssh_test_reset_global_config();
	dssh_transport_set_callbacks(mock_tx_dispatch, mock_rx_dispatch,
	    mock_rxline_dispatch, mock_extra_line_cb);
	ASSERT_OK(register_none_comp());

	struct mock_io_state io;
	ASSERT_OK(mock_io_init(&io, 0));

	dssh_session client = dssh_session_init(true, 0);
	ASSERT_NOT_NULL(client);
	dssh_session_set_cbdata(client, &io, &io, &io, &io);

	uint8_t payload[] = { SSH_MSG_SERVICE_REQUEST, 0x01, 0x02 };
	ASSERT_OK(dssh_transport_send_packet(client, payload, sizeof(payload), NULL));

	uint8_t wire[256];
	size_t drained = mock_io_drain(&io.c2s, wire, sizeof(wire));
	ASSERT_TRUE(drained >= 8);

	/* padding_length field is at offset 4 */
	uint8_t padding_len = wire[4];
	ASSERT_TRUE(padding_len >= 4);

	dssh_session_cleanup(client);
	mock_io_free(&io);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

static int
test_packet_seq_increment(void)
{
	dssh_test_reset_global_config();
	dssh_transport_set_callbacks(mock_tx_dispatch, mock_rx_dispatch,
	    mock_rxline_dispatch, mock_extra_line_cb);
	ASSERT_OK(register_none_comp());

	struct mock_io_state io;
	ASSERT_OK(mock_io_init(&io, 0));

	dssh_session client = dssh_session_init(true, 0);
	ASSERT_NOT_NULL(client);
	dssh_session_set_cbdata(client, &io, &io, &io, &io);

	uint8_t payload[] = { SSH_MSG_SERVICE_REQUEST };
	uint32_t seq1, seq2, seq3;
	ASSERT_OK(dssh_transport_send_packet(client, payload, 1, &seq1));
	ASSERT_OK(dssh_transport_send_packet(client, payload, 1, &seq2));
	ASSERT_OK(dssh_transport_send_packet(client, payload, 1, &seq3));
	ASSERT_EQ_U(seq1, 0);
	ASSERT_EQ_U(seq2, 1);
	ASSERT_EQ_U(seq3, 2);

	dssh_session_cleanup(client);
	mock_io_free(&io);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

static int
test_packet_server_receives_client(void)
{
	dssh_test_reset_global_config();
	dssh_transport_set_callbacks(mock_tx_dispatch, mock_rx_dispatch,
	    mock_rxline_dispatch, mock_extra_line_cb);
	ASSERT_OK(register_none_comp());

	struct mock_io_state io;
	ASSERT_OK(mock_io_init(&io, 0));

	dssh_session client = dssh_session_init(true, 0);
	ASSERT_NOT_NULL(client);
	dssh_session_set_cbdata(client, &io, &io, &io, &io);

	dssh_session server = init_server_session();
	ASSERT_NOT_NULL(server);
	dssh_session_set_cbdata(server, &io, &io, &io, &io);

	/* Send multiple packets, verify all arrive in order */
	uint8_t p1[] = { SSH_MSG_SERVICE_REQUEST, 0x01 };
	uint8_t p2[] = { SSH_MSG_SERVICE_ACCEPT, 0x02 };
	ASSERT_OK(dssh_transport_send_packet(client, p1, sizeof(p1), NULL));
	ASSERT_OK(dssh_transport_send_packet(client, p2, sizeof(p2), NULL));

	uint8_t msg_type;
	uint8_t *recv_payload;
	size_t recv_len;

	ASSERT_OK(dssh_transport_recv_packet(server, &msg_type, &recv_payload, &recv_len));
	ASSERT_EQ(msg_type, SSH_MSG_SERVICE_REQUEST);
	ASSERT_EQ_U(recv_len, 2);
	ASSERT_EQ(recv_payload[1], 0x01);

	ASSERT_OK(dssh_transport_recv_packet(server, &msg_type, &recv_payload, &recv_len));
	ASSERT_EQ(msg_type, SSH_MSG_SERVICE_ACCEPT);
	ASSERT_EQ_U(recv_len, 2);
	ASSERT_EQ(recv_payload[1], 0x02);

	dssh_session_cleanup(client);
	dssh_session_cleanup(server);
	mock_io_free(&io);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

static int
test_packet_too_large(void)
{
	dssh_test_reset_global_config();
	dssh_transport_set_callbacks(mock_tx_dispatch, mock_rx_dispatch,
	    mock_rxline_dispatch, mock_extra_line_cb);
	ASSERT_OK(register_none_comp());

	struct mock_io_state io;
	ASSERT_OK(mock_io_init(&io, 0));

	dssh_session client = dssh_session_init(true, 0);
	ASSERT_NOT_NULL(client);
	dssh_session_set_cbdata(client, &io, &io, &io, &io);

	/* Allocate a payload that exceeds the packet buffer */
	size_t too_big = client->trans.packet_buf_sz + 1;
	uint8_t *big_payload = malloc(too_big);
	ASSERT_NOT_NULL(big_payload);
	memset(big_payload, 0x42, too_big);
	big_payload[0] = SSH_MSG_SERVICE_REQUEST;

	int res = dssh_transport_send_packet(client, big_payload, too_big, NULL);
	ASSERT_ERR(res, DSSH_ERROR_TOOLONG);

	free(big_payload);
	dssh_session_cleanup(client);
	mock_io_free(&io);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

static int
test_packet_bidirectional(void)
{
	dssh_test_reset_global_config();
	dssh_transport_set_callbacks(mock_tx_dispatch, mock_rx_dispatch,
	    mock_rxline_dispatch, mock_extra_line_cb);
	ASSERT_OK(register_none_comp());

	struct mock_io_state io;
	ASSERT_OK(mock_io_init(&io, 0));

	dssh_session client = dssh_session_init(true, 0);
	ASSERT_NOT_NULL(client);
	dssh_session_set_cbdata(client, &io, &io, &io, &io);

	dssh_session server = init_server_session();
	ASSERT_NOT_NULL(server);
	dssh_session_set_cbdata(server, &io, &io, &io, &io);

	/* Client to server */
	uint8_t c2s_payload[] = { SSH_MSG_SERVICE_REQUEST, 0xC0 };
	ASSERT_OK(dssh_transport_send_packet(client, c2s_payload, sizeof(c2s_payload), NULL));

	uint8_t msg_type;
	uint8_t *recv_payload;
	size_t recv_len;
	ASSERT_OK(dssh_transport_recv_packet(server, &msg_type, &recv_payload, &recv_len));
	ASSERT_EQ(msg_type, SSH_MSG_SERVICE_REQUEST);

	/* Server to client */
	uint8_t s2c_pay[] = { SSH_MSG_SERVICE_ACCEPT, 0xA0 };
	ASSERT_OK(dssh_transport_send_packet(server, s2c_pay, sizeof(s2c_pay), NULL));

	ASSERT_OK(dssh_transport_recv_packet(client, &msg_type, &recv_payload, &recv_len));
	ASSERT_EQ(msg_type, SSH_MSG_SERVICE_ACCEPT);
	ASSERT_EQ(recv_payload[1], 0xA0);

	dssh_session_cleanup(client);
	dssh_session_cleanup(server);
	mock_io_free(&io);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

static int
test_packet_empty_payload(void)
{
	dssh_test_reset_global_config();
	dssh_transport_set_callbacks(mock_tx_dispatch, mock_rx_dispatch,
	    mock_rxline_dispatch, mock_extra_line_cb);
	ASSERT_OK(register_none_comp());

	struct mock_io_state io;
	ASSERT_OK(mock_io_init(&io, 0));

	dssh_session client = dssh_session_init(true, 0);
	ASSERT_NOT_NULL(client);
	dssh_session_set_cbdata(client, &io, &io, &io, &io);

	dssh_session server = init_server_session();
	ASSERT_NOT_NULL(server);
	dssh_session_set_cbdata(server, &io, &io, &io, &io);

	/* Send a single-byte payload (just msg type) */
	uint8_t payload[] = { SSH_MSG_SERVICE_REQUEST };
	ASSERT_OK(dssh_transport_send_packet(client, payload, 1, NULL));

	uint8_t msg_type;
	uint8_t *recv_payload;
	size_t recv_len;
	ASSERT_OK(dssh_transport_recv_packet(server, &msg_type, &recv_payload, &recv_len));
	ASSERT_EQ(msg_type, SSH_MSG_SERVICE_REQUEST);
	ASSERT_EQ_U(recv_len, 1);

	dssh_session_cleanup(client);
	dssh_session_cleanup(server);
	mock_io_free(&io);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

/* ================================================================
 * Transport message handling (~5 tests)
 * ================================================================ */

static int
test_ignore_silently_skipped(void)
{
	dssh_test_reset_global_config();
	dssh_transport_set_callbacks(mock_tx_dispatch, mock_rx_dispatch,
	    mock_rxline_dispatch, mock_extra_line_cb);
	ASSERT_OK(register_none_comp());

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
	ASSERT_OK(dssh_transport_send_packet(client, ignore_msg, sizeof(ignore_msg), NULL));
	ASSERT_OK(dssh_transport_send_packet(client, real_msg, sizeof(real_msg), NULL));

	/* Server's recv_packet should skip IGNORE and return real message */
	uint8_t msg_type;
	uint8_t *recv_payload;
	size_t recv_len;
	ASSERT_OK(dssh_transport_recv_packet(server, &msg_type, &recv_payload, &recv_len));
	ASSERT_EQ(msg_type, SSH_MSG_SERVICE_REQUEST);
	ASSERT_EQ(recv_payload[1], 0xBE);

	dssh_session_cleanup(client);
	dssh_session_cleanup(server);
	mock_io_free(&io);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

static int
test_disconnect_sets_terminate(void)
{
	dssh_test_reset_global_config();
	dssh_transport_set_callbacks(mock_tx_dispatch, mock_rx_dispatch,
	    mock_rxline_dispatch, mock_extra_line_cb);
	ASSERT_OK(register_none_comp());

	struct mock_io_state io;
	ASSERT_OK(mock_io_init(&io, 0));

	dssh_session client = dssh_session_init(true, 0);
	ASSERT_NOT_NULL(client);
	dssh_session_set_cbdata(client, &io, &io, &io, &io);

	dssh_session server = init_server_session();
	ASSERT_NOT_NULL(server);
	dssh_session_set_cbdata(server, &io, &io, &io, &io);

	ASSERT_FALSE(dssh_session_is_terminated(server));

	/* Client sends disconnect */
	dssh_transport_disconnect(client, SSH_DISCONNECT_BY_APPLICATION, "goodbye");

	/* Server tries to recv — should get TERMINATED */
	uint8_t msg_type;
	uint8_t *recv_payload;
	size_t recv_len;
	int res = dssh_transport_recv_packet(server, &msg_type, &recv_payload, &recv_len);
	ASSERT_ERR(res, DSSH_ERROR_TERMINATED);
	ASSERT_TRUE(dssh_session_is_terminated(server));

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
	ASSERT_OK(register_none_comp());

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

	ASSERT_OK(dssh_transport_send_packet(client, debug_msg, pos, NULL));

	/* Send a real message after so recv_packet returns */
	uint8_t real_msg[] = { SSH_MSG_SERVICE_REQUEST, 0xFF };
	ASSERT_OK(dssh_transport_send_packet(client, real_msg, sizeof(real_msg), NULL));

	uint8_t msg_type;
	uint8_t *recv_payload;
	size_t recv_len;
	ASSERT_OK(dssh_transport_recv_packet(server, &msg_type, &recv_payload, &recv_len));
	ASSERT_EQ(msg_type, SSH_MSG_SERVICE_REQUEST);

	ASSERT_TRUE(test_debug_called);
	ASSERT_TRUE(test_debug_always_display);
	ASSERT_STR_EQ(test_debug_message, "test debug");

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
	ASSERT_OK(register_none_comp());

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

	ASSERT_OK(dssh_transport_send_packet(client, unimpl_msg, pos, NULL));

	/* Send a real message after */
	uint8_t real_msg[] = { SSH_MSG_SERVICE_REQUEST, 0x01 };
	ASSERT_OK(dssh_transport_send_packet(client, real_msg, sizeof(real_msg), NULL));

	uint8_t msg_type;
	uint8_t *recv_payload;
	size_t recv_len;
	ASSERT_OK(dssh_transport_recv_packet(server, &msg_type, &recv_payload, &recv_len));
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
	ASSERT_OK(register_none_comp());

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
	ASSERT_OK(dssh_transport_send_packet(client, ignore, 1, NULL));
	ASSERT_OK(dssh_transport_send_packet(client, ignore, 1, NULL));
	ASSERT_OK(dssh_transport_send_packet(client, ignore, 1, NULL));

	uint8_t real[] = { SSH_MSG_SERVICE_REQUEST, 0x77 };
	ASSERT_OK(dssh_transport_send_packet(client, real, sizeof(real), NULL));

	uint8_t msg_type;
	uint8_t *recv_payload;
	size_t recv_len;
	ASSERT_OK(dssh_transport_recv_packet(server, &msg_type, &recv_payload, &recv_len));
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

	const char *c_enc = dssh_transport_get_enc_name(ctx.client);
	const char *s_enc = dssh_transport_get_enc_name(ctx.server);
	ASSERT_NOT_NULL(c_enc);
	ASSERT_NOT_NULL(s_enc);
	ASSERT_STR_EQ(c_enc, s_enc);

	const char *c_mac = dssh_transport_get_mac_name(ctx.client);
	const char *s_mac = dssh_transport_get_mac_name(ctx.server);
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
	ASSERT_OK(dssh_transport_send_packet(ctx.client, payload, sizeof(payload), NULL));

	uint8_t msg_type;
	uint8_t *recv_payload;
	size_t recv_len;
	ASSERT_OK(dssh_transport_recv_packet(ctx.server, &msg_type, &recv_payload, &recv_len));
	ASSERT_EQ(msg_type, SSH_MSG_SERVICE_REQUEST);
	ASSERT_EQ_U(recv_len, sizeof(payload));
	ASSERT_MEM_EQ(recv_payload, payload, sizeof(payload));

	handshake_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Rekey (~9 tests)
 * ================================================================ */

static int
test_rekey_needed_false_initially(void)
{
	dssh_test_reset_global_config();
	dssh_transport_set_callbacks(mock_tx_dispatch, mock_rx_dispatch,
	    mock_rxline_dispatch, mock_extra_line_cb);
	ASSERT_OK(register_none_comp());

	struct mock_io_state io;
	ASSERT_OK(mock_io_init(&io, 0));

	dssh_session sess = dssh_session_init(true, 0);
	ASSERT_NOT_NULL(sess);

	ASSERT_FALSE(dssh_transport_rekey_needed(sess));

	dssh_session_cleanup(sess);
	mock_io_free(&io);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

static int
test_rekey_needed_tx_packets(void)
{
	dssh_test_reset_global_config();
	dssh_transport_set_callbacks(mock_tx_dispatch, mock_rx_dispatch,
	    mock_rxline_dispatch, mock_extra_line_cb);
	ASSERT_OK(register_none_comp());

	struct mock_io_state io;
	ASSERT_OK(mock_io_init(&io, 0));

	dssh_session sess = dssh_session_init(true, 0);
	ASSERT_NOT_NULL(sess);

	sess->trans.tx_since_rekey = DSSH_REKEY_SOFT_LIMIT;
	ASSERT_TRUE(dssh_transport_rekey_needed(sess));

	dssh_session_cleanup(sess);
	mock_io_free(&io);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

static int
test_rekey_needed_rx_packets(void)
{
	dssh_test_reset_global_config();
	dssh_transport_set_callbacks(mock_tx_dispatch, mock_rx_dispatch,
	    mock_rxline_dispatch, mock_extra_line_cb);
	ASSERT_OK(register_none_comp());

	struct mock_io_state io;
	ASSERT_OK(mock_io_init(&io, 0));

	dssh_session sess = dssh_session_init(true, 0);
	ASSERT_NOT_NULL(sess);

	sess->trans.rx_since_rekey = DSSH_REKEY_SOFT_LIMIT;
	ASSERT_TRUE(dssh_transport_rekey_needed(sess));

	dssh_session_cleanup(sess);
	mock_io_free(&io);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

static int
test_rekey_needed_bytes(void)
{
	dssh_test_reset_global_config();
	dssh_transport_set_callbacks(mock_tx_dispatch, mock_rx_dispatch,
	    mock_rxline_dispatch, mock_extra_line_cb);
	ASSERT_OK(register_none_comp());

	struct mock_io_state io;
	ASSERT_OK(mock_io_init(&io, 0));

	dssh_session sess = dssh_session_init(true, 0);
	ASSERT_NOT_NULL(sess);

	sess->trans.bytes_since_rekey = DSSH_REKEY_BYTES;
	ASSERT_TRUE(dssh_transport_rekey_needed(sess));

	dssh_session_cleanup(sess);
	mock_io_free(&io);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

static int
test_rekey_needed_time(void)
{
	dssh_test_reset_global_config();
	dssh_transport_set_callbacks(mock_tx_dispatch, mock_rx_dispatch,
	    mock_rxline_dispatch, mock_extra_line_cb);
	ASSERT_OK(register_none_comp());

	struct mock_io_state io;
	ASSERT_OK(mock_io_init(&io, 0));

	dssh_session sess = dssh_session_init(true, 0);
	ASSERT_NOT_NULL(sess);

	/* Set rekey_time to more than DSSH_REKEY_SECONDS ago */
	sess->trans.rekey_time = time(NULL) - DSSH_REKEY_SECONDS - 1;
	ASSERT_TRUE(dssh_transport_rekey_needed(sess));

	dssh_session_cleanup(sess);
	mock_io_free(&io);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

static int
test_rekey_needed_below_threshold(void)
{
	dssh_test_reset_global_config();
	dssh_transport_set_callbacks(mock_tx_dispatch, mock_rx_dispatch,
	    mock_rxline_dispatch, mock_extra_line_cb);
	ASSERT_OK(register_none_comp());

	struct mock_io_state io;
	ASSERT_OK(mock_io_init(&io, 0));

	dssh_session sess = dssh_session_init(true, 0);
	ASSERT_NOT_NULL(sess);

	/* Set values just below thresholds */
	sess->trans.tx_since_rekey = DSSH_REKEY_SOFT_LIMIT - 1;
	sess->trans.rx_since_rekey = DSSH_REKEY_SOFT_LIMIT - 1;
	sess->trans.bytes_since_rekey = DSSH_REKEY_BYTES - 1;
	sess->trans.rekey_time = time(NULL);
	ASSERT_FALSE(dssh_transport_rekey_needed(sess));

	dssh_session_cleanup(sess);
	mock_io_free(&io);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

struct rekey_thread_arg {
	struct handshake_ctx *ctx;
	int result;
};

static int
rekey_client_thread(void *arg)
{
	struct rekey_thread_arg *a = arg;
	a->result = dssh_transport_rekey(a->ctx->client);
	return 0;
}

static int
rekey_server_recv_thread(void *arg)
{
	struct rekey_thread_arg *a = arg;
	/* Server receives — recv_packet detects peer KEXINIT and
	 * handles rekey internally.  We need to send something from
	 * client after rekey so the server recv returns. */
	uint8_t msg_type;
	uint8_t *payload;
	size_t payload_len;
	a->result = dssh_transport_recv_packet(a->ctx->server,
	    &msg_type, &payload, &payload_len);
	return 0;
}

static int
test_rekey_after_handshake(void)
{
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	/* Save session ID for comparison after rekey */
	size_t sid_sz = ctx.client->trans.session_id_sz;
	uint8_t *sid_before = malloc(sid_sz);
	ASSERT_NOT_NULL(sid_before);
	memcpy(sid_before, ctx.client->trans.session_id, sid_sz);

	/* Run rekey: client initiates, server handles via recv_packet */
	struct rekey_thread_arg client_arg = { &ctx, -1 };
	struct rekey_thread_arg server_arg = { &ctx, -1 };

	thrd_t ct, st;
	ASSERT_TRUE(thrd_create(&ct, rekey_client_thread, &client_arg) == thrd_success);
	ASSERT_TRUE(thrd_create(&st, rekey_server_recv_thread, &server_arg) == thrd_success);

	/* After rekey, client sends a message so server recv returns */
	thrd_join(ct, NULL);
	ASSERT_EQ(client_arg.result, 0);

	uint8_t real_msg[] = { SSH_MSG_SERVICE_REQUEST, 0xEE };
	ASSERT_OK(dssh_transport_send_packet(ctx.client, real_msg, sizeof(real_msg), NULL));

	thrd_join(st, NULL);
	ASSERT_EQ(server_arg.result, 0);

	/* Session ID should be stable across rekey */
	ASSERT_EQ_U(ctx.client->trans.session_id_sz, sid_sz);
	ASSERT_MEM_EQ(ctx.client->trans.session_id, sid_before, sid_sz);

	/* Rekey counters should have been reset */
	ASSERT_TRUE(ctx.client->trans.tx_since_rekey < 10);
	ASSERT_TRUE(ctx.client->trans.rx_since_rekey < 10);

	free(sid_before);
	handshake_cleanup(&ctx);
	return TEST_PASS;
}

static int
test_rekey_session_id_stable(void)
{
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	size_t sid_sz = ctx.server->trans.session_id_sz;
	uint8_t *sid_before = malloc(sid_sz);
	ASSERT_NOT_NULL(sid_before);
	memcpy(sid_before, ctx.server->trans.session_id, sid_sz);

	struct rekey_thread_arg client_arg = { &ctx, -1 };
	struct rekey_thread_arg server_arg = { &ctx, -1 };

	thrd_t ct, st;
	ASSERT_TRUE(thrd_create(&ct, rekey_client_thread, &client_arg) == thrd_success);
	ASSERT_TRUE(thrd_create(&st, rekey_server_recv_thread, &server_arg) == thrd_success);

	thrd_join(ct, NULL);

	uint8_t msg[] = { SSH_MSG_SERVICE_REQUEST, 0x01 };
	ASSERT_OK(dssh_transport_send_packet(ctx.client, msg, sizeof(msg), NULL));

	thrd_join(st, NULL);

	/* Server session ID should be unchanged */
	ASSERT_EQ_U(ctx.server->trans.session_id_sz, sid_sz);
	ASSERT_MEM_EQ(ctx.server->trans.session_id, sid_before, sid_sz);

	free(sid_before);
	handshake_cleanup(&ctx);
	return TEST_PASS;
}

static int
test_rekey_encrypted_roundtrip(void)
{
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	/* Rekey */
	struct rekey_thread_arg client_arg = { &ctx, -1 };
	struct rekey_thread_arg server_arg = { &ctx, -1 };

	thrd_t ct, st;
	ASSERT_TRUE(thrd_create(&ct, rekey_client_thread, &client_arg) == thrd_success);
	ASSERT_TRUE(thrd_create(&st, rekey_server_recv_thread, &server_arg) == thrd_success);

	thrd_join(ct, NULL);
	ASSERT_EQ(client_arg.result, 0);

	/* Send a message after rekey so server recv returns */
	uint8_t payload[] = { SSH_MSG_SERVICE_REQUEST, 0xDE, 0xAD, 0xBE, 0xEF };
	ASSERT_OK(dssh_transport_send_packet(ctx.client, payload, sizeof(payload), NULL));
	thrd_join(st, NULL);
	ASSERT_EQ(server_arg.result, 0);

	/* Now send another message and verify roundtrip with new keys */
	uint8_t payload2[] = { SSH_MSG_SERVICE_ACCEPT, 0x12, 0x34 };
	ASSERT_OK(dssh_transport_send_packet(ctx.server, payload2, sizeof(payload2), NULL));

	uint8_t msg_type;
	uint8_t *recv_payload;
	size_t recv_len;
	ASSERT_OK(dssh_transport_recv_packet(ctx.client, &msg_type, &recv_payload, &recv_len));
	ASSERT_EQ(msg_type, SSH_MSG_SERVICE_ACCEPT);
	ASSERT_EQ_U(recv_len, sizeof(payload2));
	ASSERT_MEM_EQ(recv_payload, payload2, sizeof(payload2));

	handshake_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Session lifecycle tests
 * ================================================================ */

static int
test_session_init_cleanup(void)
{
	dssh_test_reset_global_config();
	dssh_transport_set_callbacks(mock_tx_dispatch, mock_rx_dispatch,
	    mock_rxline_dispatch, mock_extra_line_cb);
	ASSERT_OK(register_none_comp());

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
	ASSERT_OK(register_none_comp());

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
	int res = dssh_transport_send_packet(ctx.client, msg,
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
	ASSERT_OK(dssh_transport_send_packet(ctx.client, msg,
	    sizeof(msg), NULL));

	/* Set hard limit on server AFTER packet is in flight */
	ctx.server->trans.rx_since_rekey = DSSH_REKEY_HARD_LIMIT;

	uint8_t mt;
	uint8_t *payload;
	size_t plen;
	int res = dssh_transport_recv_packet(ctx.server, &mt,
	    &payload, &plen);
	ASSERT_EQ(res, DSSH_ERROR_REKEY_NEEDED);

	handshake_cleanup(&ctx);
	return TEST_PASS;
}

/* RFC 4253 s6.4-3: sequence numbers never reset across rekey */
static int
test_rekey_seq_preserved(void)
{
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	/* Send a few packets to advance sequence numbers */
	for (int i = 0; i < 5; i++) {
		uint8_t msg[] = { SSH_MSG_IGNORE, 0 };
		ASSERT_OK(dssh_transport_send_packet(ctx.client, msg,
		    sizeof(msg), NULL));
	}

	uint32_t pre_tx = ctx.client->trans.tx_seq;
	uint32_t pre_rx = ctx.client->trans.rx_seq;
	ASSERT_TRUE(pre_tx >= 5);

	/* Run rekey */
	struct rekey_thread_arg client_arg = { &ctx, -1 };
	struct rekey_thread_arg server_arg = { &ctx, -1 };
	thrd_t ct, st;
	thrd_create(&ct, rekey_client_thread, &client_arg);
	thrd_create(&st, rekey_server_recv_thread, &server_arg);
	thrd_join(ct, NULL);

	uint8_t msg2[] = { SSH_MSG_SERVICE_REQUEST, 0xEE };
	dssh_transport_send_packet(ctx.client, msg2, sizeof(msg2), NULL);
	thrd_join(st, NULL);

	/* Sequence numbers must NOT have been reset */
	ASSERT_TRUE(ctx.client->trans.tx_seq > pre_tx);
	ASSERT_TRUE(ctx.client->trans.rx_seq >= pre_rx);

	/* But per-key counters WERE reset */
	ASSERT_TRUE(ctx.client->trans.tx_since_rekey < pre_tx);

	handshake_cleanup(&ctx);
	return TEST_PASS;
}

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

	/* Digest size should be 32 (HMAC-SHA-256) */
	ASSERT_EQ(ctx.client->trans.mac_c2s_selected->digest_size, 32);
	ASSERT_EQ(ctx.client->trans.mac_s2c_selected->digest_size, 32);

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
	ASSERT_EQ(dssh_transport_set_version("MySSH-1.0", NULL), 0);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

static int
test_set_version_valid_comment(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_transport_set_version("MySSH-1.0", "FreeBSD"), 0);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

static int
test_set_version_null(void)
{
	/* NULL software_version keeps the default — should succeed */
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_transport_set_version(NULL, NULL), 0);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

static int
test_set_version_null_with_comment(void)
{
	/* NULL version + comment — sets comment with default version */
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
	ASSERT_EQ(dssh_transport_set_version("MySSH-1.0", ""),
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
	memset(&ctx, 0, sizeof(ctx));

	dssh_test_reset_global_config();
	if (register_all_algorithms() < 0)
		return TEST_FAIL;
	if (test_generate_host_key() < 0)
		return TEST_FAIL;

	if (mock_io_init(&ctx.io, 0) < 0)
		return TEST_FAIL;

	dssh_transport_set_callbacks(mock_tx_dispatch, mock_rx_dispatch,
	    mock_rxline_dispatch, mock_extra_line_cb);
	dssh_transport_set_version("DeuceSSH-0.0", "test-comment");

	ctx.client = dssh_session_init(true, 0);
	if (ctx.client == NULL) {
		mock_io_free(&ctx.io);
		dssh_test_reset_global_config();
		return TEST_FAIL;
	}
	dssh_session_set_cbdata(ctx.client, &ctx.io, &ctx.io,
	    &ctx.io, &ctx.io);

	ctx.server = init_server_session();
	if (ctx.server == NULL) {
		dssh_session_cleanup(ctx.client);
		mock_io_free(&ctx.io);
		dssh_test_reset_global_config();
		return TEST_FAIL;
	}
	dssh_session_set_cbdata(ctx.server, &ctx.io, &ctx.io,
	    &ctx.io, &ctx.io);

	thrd_t ct, st;
	thrd_create(&ct, handshake_client_thread, &ctx);
	thrd_create(&st, handshake_server_thread, &ctx);
	thrd_join(ct, NULL);
	thrd_join(st, NULL);

	ASSERT_EQ(ctx.client_result, 0);
	ASSERT_EQ(ctx.server_result, 0);

	/* The local id string should contain the comment */
	const char *id = ctx.client->trans.id_str;
	ASSERT_NOT_NULL(id);
	ASSERT_TRUE(strstr(id, "test-comment") != NULL);

	handshake_cleanup(&ctx);
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

	/* Close the s2c pipe before version exchange starts —
	 * the rxline callback will fail */
	mock_io_close_s2c(&io);

	int res = dssh_transport_handshake(sess);
	/* Should fail — either the tx succeeds but rx fails with pipe closed */
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
 * Packet recv with bad padding (Category 3)
 *
 * After handshake, corrupt a packet so padding_length >= packet_length.
 * ================================================================ */

static int
test_packet_recv_bad_padding(void)
{
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	/* We need to craft a raw packet with bad padding.
	 * The simplest approach: send a legitimate packet from client,
	 * then receive it on server side.  This just confirms the
	 * normal path works.  For bad padding, we'd need to inject
	 * raw bytes — but with encryption active, crafting raw is
	 * extremely hard.  Instead test pre-handshake. */
	handshake_cleanup(&ctx);

	/* Test with unencrypted session */
	dssh_test_reset_global_config();
	if (register_all_algorithms() < 0)
		return TEST_FAIL;
	if (test_generate_host_key() < 0)
		return TEST_FAIL;

	struct mock_io_state io;
	if (mock_io_init(&io, 0) < 0) {
		dssh_test_reset_global_config();
		return TEST_FAIL;
	}

	dssh_transport_set_callbacks(mock_tx_dispatch, mock_rx_dispatch,
	    mock_rxline_dispatch, mock_extra_line_cb);

	/* Create a server session (no handshake — unencrypted) */
	dssh_session sess = init_server_session();
	if (sess == NULL) {
		mock_io_free(&io);
		dssh_test_reset_global_config();
		return TEST_FAIL;
	}
	dssh_session_set_cbdata(sess, &io, &io, &io, &io);

	/* Inject a packet where padding_length >= packet_length.
	 * packet_length=2 (minimum for 1 byte padding + 1 byte payload),
	 * but set padding_length=255, which is >= packet_length. */
	uint8_t bad_packet[16];
	size_t pos = 0;
	/* packet_length = 2 */
	dssh_serialize_uint32(2, bad_packet, sizeof(bad_packet), &pos);
	/* padding_length = 255 (>= packet_length of 2) */
	bad_packet[pos++] = 255;
	/* payload byte */
	bad_packet[pos++] = 0x42;
	/* Pad to block size (8 bytes minimum) */
	while (pos < 8)
		bad_packet[pos++] = 0;

	mock_io_inject(&io.c2s, bad_packet, 8);

	uint8_t msg_type;
	uint8_t *payload;
	size_t payload_len;
	int res = dssh_transport_recv_packet(sess, &msg_type,
	    &payload, &payload_len);
	ASSERT_EQ(res, DSSH_ERROR_PARSE);

	dssh_session_cleanup(sess);
	mock_io_free(&io);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

/* ================================================================
 * Packet recv: packet too small (Category 3)
 * ================================================================ */

static int
test_packet_recv_too_small(void)
{
	dssh_test_reset_global_config();
	if (register_all_algorithms() < 0)
		return TEST_FAIL;
	if (test_generate_host_key() < 0)
		return TEST_FAIL;

	struct mock_io_state io;
	if (mock_io_init(&io, 0) < 0) {
		dssh_test_reset_global_config();
		return TEST_FAIL;
	}

	dssh_transport_set_callbacks(mock_tx_dispatch, mock_rx_dispatch,
	    mock_rxline_dispatch, mock_extra_line_cb);

	dssh_session sess = init_server_session();
	if (sess == NULL) {
		mock_io_free(&io);
		dssh_test_reset_global_config();
		return TEST_FAIL;
	}
	dssh_session_set_cbdata(sess, &io, &io, &io, &io);

	/* packet_length = 1, which is < 2 minimum */
	uint8_t bad_packet[8];
	size_t pos = 0;
	dssh_serialize_uint32(1, bad_packet, sizeof(bad_packet), &pos);
	bad_packet[pos++] = 0;
	while (pos < 8)
		bad_packet[pos++] = 0;

	mock_io_inject(&io.c2s, bad_packet, 8);

	uint8_t msg_type;
	uint8_t *payload;
	size_t payload_len;
	int res = dssh_transport_recv_packet(sess, &msg_type,
	    &payload, &payload_len);
	ASSERT_EQ(res, DSSH_ERROR_TOOLONG);

	dssh_session_cleanup(sess);
	mock_io_free(&io);
	dssh_test_reset_global_config();
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
 * Registration validation — covers TOOLATE, TOOLONG, MUST_BE_NULL
 * branches in register_kex/key_algo/enc/mac/comp/lang.
 * ================================================================ */

static int
test_register_kex_toolate(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(register_all_algorithms(), 0);
	dssh_transport_set_callbacks(mock_tx_dispatch, mock_rx_dispatch,
	    mock_rxline_dispatch, mock_extra_line_cb);

	dssh_session sess = dssh_session_init(true, 0);
	ASSERT_NOT_NULL(sess);

	/* gconf.used is now true — registration should fail */
	uint8_t kbuf[sizeof(struct dssh_kex_s) + 16];
	memset(kbuf, 0, sizeof(kbuf));
	struct dssh_kex_s *late_kex = (struct dssh_kex_s *)kbuf;
	strcpy(late_kex->name, "late-kex");
	ASSERT_EQ(dssh_transport_register_kex(late_kex), DSSH_ERROR_TOOLATE);

	dssh_session_cleanup(sess);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

static int
test_register_kex_empty_name(void)
{
	dssh_test_reset_global_config();

	uint8_t kbuf[sizeof(struct dssh_kex_s) + 4];
	memset(kbuf, 0, sizeof(kbuf));
	struct dssh_kex_s *bad = (struct dssh_kex_s *)kbuf;
	bad->name[0] = '\0';
	ASSERT_EQ(dssh_transport_register_kex(bad), DSSH_ERROR_TOOLONG);

	dssh_test_reset_global_config();
	return TEST_PASS;
}

static int
test_register_kex_next_not_null(void)
{
	dssh_test_reset_global_config();

	uint8_t kbuf[sizeof(struct dssh_kex_s) + 16];
	memset(kbuf, 0, sizeof(kbuf));
	struct dssh_kex_s *bad = (struct dssh_kex_s *)kbuf;
	bad->next = (struct dssh_kex_s *)0x1;  /* non-NULL */
	strcpy(bad->name, "bad-kex");
	ASSERT_EQ(dssh_transport_register_kex(bad), DSSH_ERROR_MUST_BE_NULL);

	dssh_test_reset_global_config();
	return TEST_PASS;
}

static int
test_register_enc_toolate(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(register_all_algorithms(), 0);
	dssh_transport_set_callbacks(mock_tx_dispatch, mock_rx_dispatch,
	    mock_rxline_dispatch, mock_extra_line_cb);

	dssh_session sess = dssh_session_init(true, 0);
	ASSERT_NOT_NULL(sess);

	uint8_t ebuf[sizeof(struct dssh_enc_s) + 16];
	memset(ebuf, 0, sizeof(ebuf));
	struct dssh_enc_s *late_enc = (struct dssh_enc_s *)ebuf;
	strcpy(late_enc->name, "late-enc");
	ASSERT_EQ(dssh_transport_register_enc(late_enc), DSSH_ERROR_TOOLATE);

	dssh_session_cleanup(sess);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

static int
test_register_mac_toolate(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(register_all_algorithms(), 0);
	dssh_transport_set_callbacks(mock_tx_dispatch, mock_rx_dispatch,
	    mock_rxline_dispatch, mock_extra_line_cb);

	dssh_session sess = dssh_session_init(true, 0);
	ASSERT_NOT_NULL(sess);

	uint8_t mbuf[sizeof(struct dssh_mac_s) + 16];
	memset(mbuf, 0, sizeof(mbuf));
	struct dssh_mac_s *late_mac = (struct dssh_mac_s *)mbuf;
	strcpy(late_mac->name, "late-mac");
	ASSERT_EQ(dssh_transport_register_mac(late_mac), DSSH_ERROR_TOOLATE);

	dssh_session_cleanup(sess);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

static int
test_register_comp_empty_name(void)
{
	dssh_test_reset_global_config();

	uint8_t cbuf[sizeof(struct dssh_comp_s) + 4];
	memset(cbuf, 0, sizeof(cbuf));
	struct dssh_comp_s *bad = (struct dssh_comp_s *)cbuf;
	bad->name[0] = '\0';
	ASSERT_EQ(dssh_transport_register_comp(bad), DSSH_ERROR_TOOLONG);

	dssh_test_reset_global_config();
	return TEST_PASS;
}

static int
test_register_lang_basic(void)
{
	/* Covers the entire register_lang function which was never called.
	 * Must heap-allocate since reset_global_config frees entries. */
	dssh_test_reset_global_config();

	size_t sz = sizeof(struct dssh_language_s) + 8;
	struct dssh_language_s *lang = calloc(1, sz);
	if (lang == NULL)
		return TEST_FAIL;
	strcpy(lang->name, "en");
	ASSERT_EQ(dssh_transport_register_lang(lang), 0);

	dssh_test_reset_global_config();
	return TEST_PASS;
}

static int
test_register_lang_empty_name(void)
{
	dssh_test_reset_global_config();

	uint8_t lbuf[sizeof(struct dssh_language_s) + 4];
	memset(lbuf, 0, sizeof(lbuf));
	struct dssh_language_s *bad = (struct dssh_language_s *)lbuf;
	bad->name[0] = '\0';
	ASSERT_EQ(dssh_transport_register_lang(bad), DSSH_ERROR_TOOLONG);

	dssh_test_reset_global_config();
	return TEST_PASS;
}

static int
test_register_lang_toolate(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(register_all_algorithms(), 0);
	dssh_transport_set_callbacks(mock_tx_dispatch, mock_rx_dispatch,
	    mock_rxline_dispatch, mock_extra_line_cb);

	dssh_session sess = dssh_session_init(true, 0);
	ASSERT_NOT_NULL(sess);

	uint8_t lbuf[sizeof(struct dssh_language_s) + 8];
	memset(lbuf, 0, sizeof(lbuf));
	struct dssh_language_s *lang = (struct dssh_language_s *)lbuf;
	strcpy(lang->name, "en");
	ASSERT_EQ(dssh_transport_register_lang(lang), DSSH_ERROR_TOOLATE);

	dssh_session_cleanup(sess);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

/* ================================================================
 * Additional registration validation: TOOLONG, MUST_BE_NULL, TOOLATE
 * for key_algo, enc, mac, comp.
 * ================================================================ */

static int
test_register_key_algo_toolate(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(register_all_algorithms(), 0);
	dssh_transport_set_callbacks(mock_tx_dispatch, mock_rx_dispatch,
	    mock_rxline_dispatch, mock_extra_line_cb);

	dssh_session sess = dssh_session_init(true, 0);
	ASSERT_NOT_NULL(sess);

	uint8_t buf[sizeof(struct dssh_key_algo_s) + 16];
	memset(buf, 0, sizeof(buf));
	struct dssh_key_algo_s *ka = (struct dssh_key_algo_s *)buf;
	strcpy(ka->name, "late-key");
	ASSERT_EQ(dssh_transport_register_key_algo(ka), DSSH_ERROR_TOOLATE);

	dssh_session_cleanup(sess);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

static int
test_register_key_algo_toolong(void)
{
	dssh_test_reset_global_config();

	uint8_t buf[sizeof(struct dssh_key_algo_s) + 128];
	memset(buf, 0, sizeof(buf));
	struct dssh_key_algo_s *ka = (struct dssh_key_algo_s *)buf;
	memset(ka->name, 'x', 65);
	ka->name[65] = '\0';
	ASSERT_EQ(dssh_transport_register_key_algo(ka), DSSH_ERROR_TOOLONG);

	/* Empty name */
	ka->name[0] = '\0';
	ASSERT_EQ(dssh_transport_register_key_algo(ka), DSSH_ERROR_TOOLONG);

	dssh_test_reset_global_config();
	return TEST_PASS;
}

static int
test_register_key_algo_next_not_null(void)
{
	dssh_test_reset_global_config();

	uint8_t buf[sizeof(struct dssh_key_algo_s) + 16];
	memset(buf, 0, sizeof(buf));
	struct dssh_key_algo_s *ka = (struct dssh_key_algo_s *)buf;
	strcpy(ka->name, "test-ka");
	ka->next = (void *)1;
	ASSERT_EQ(dssh_transport_register_key_algo(ka), DSSH_ERROR_MUST_BE_NULL);

	dssh_test_reset_global_config();
	return TEST_PASS;
}

static int
test_register_enc_toolong(void)
{
	dssh_test_reset_global_config();

	uint8_t buf[sizeof(struct dssh_enc_s) + 128];
	memset(buf, 0, sizeof(buf));
	struct dssh_enc_s *enc = (struct dssh_enc_s *)buf;
	memset(enc->name, 'x', 65);
	enc->name[65] = '\0';
	ASSERT_EQ(dssh_transport_register_enc(enc), DSSH_ERROR_TOOLONG);

	enc->name[0] = '\0';
	ASSERT_EQ(dssh_transport_register_enc(enc), DSSH_ERROR_TOOLONG);

	dssh_test_reset_global_config();
	return TEST_PASS;
}

static int
test_register_enc_next_not_null(void)
{
	dssh_test_reset_global_config();

	uint8_t buf[sizeof(struct dssh_enc_s) + 16];
	memset(buf, 0, sizeof(buf));
	struct dssh_enc_s *enc = (struct dssh_enc_s *)buf;
	strcpy(enc->name, "test-enc");
	enc->next = (void *)1;
	ASSERT_EQ(dssh_transport_register_enc(enc), DSSH_ERROR_MUST_BE_NULL);

	dssh_test_reset_global_config();
	return TEST_PASS;
}

static int
test_register_mac_toolong(void)
{
	dssh_test_reset_global_config();

	uint8_t buf[sizeof(struct dssh_mac_s) + 128];
	memset(buf, 0, sizeof(buf));
	struct dssh_mac_s *mac = (struct dssh_mac_s *)buf;
	memset(mac->name, 'x', 65);
	mac->name[65] = '\0';
	ASSERT_EQ(dssh_transport_register_mac(mac), DSSH_ERROR_TOOLONG);

	mac->name[0] = '\0';
	ASSERT_EQ(dssh_transport_register_mac(mac), DSSH_ERROR_TOOLONG);

	dssh_test_reset_global_config();
	return TEST_PASS;
}

static int
test_register_mac_next_not_null(void)
{
	dssh_test_reset_global_config();

	uint8_t buf[sizeof(struct dssh_mac_s) + 16];
	memset(buf, 0, sizeof(buf));
	struct dssh_mac_s *mac = (struct dssh_mac_s *)buf;
	strcpy(mac->name, "test-mac");
	mac->next = (void *)1;
	ASSERT_EQ(dssh_transport_register_mac(mac), DSSH_ERROR_MUST_BE_NULL);

	dssh_test_reset_global_config();
	return TEST_PASS;
}

static int
test_register_comp_toolate(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(register_all_algorithms(), 0);
	dssh_transport_set_callbacks(mock_tx_dispatch, mock_rx_dispatch,
	    mock_rxline_dispatch, mock_extra_line_cb);

	dssh_session sess = dssh_session_init(true, 0);
	ASSERT_NOT_NULL(sess);

	uint8_t buf[sizeof(struct dssh_comp_s) + 16];
	memset(buf, 0, sizeof(buf));
	struct dssh_comp_s *comp = (struct dssh_comp_s *)buf;
	strcpy(comp->name, "late-comp");
	ASSERT_EQ(dssh_transport_register_comp(comp), DSSH_ERROR_TOOLATE);

	dssh_session_cleanup(sess);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

static int
test_register_comp_next_not_null(void)
{
	dssh_test_reset_global_config();

	uint8_t buf[sizeof(struct dssh_comp_s) + 16];
	memset(buf, 0, sizeof(buf));
	struct dssh_comp_s *comp = (struct dssh_comp_s *)buf;
	strcpy(comp->name, "test-comp");
	comp->next = (void *)1;
	ASSERT_EQ(dssh_transport_register_comp(comp), DSSH_ERROR_MUST_BE_NULL);

	dssh_test_reset_global_config();
	return TEST_PASS;
}

static int
test_register_lang_next_not_null(void)
{
	dssh_test_reset_global_config();

	uint8_t buf[sizeof(struct dssh_language_s) + 16];
	memset(buf, 0, sizeof(buf));
	struct dssh_language_s *lang = (struct dssh_language_s *)buf;
	strcpy(lang->name, "en");
	lang->next = (void *)1;
	ASSERT_EQ(dssh_transport_register_lang(lang), DSSH_ERROR_MUST_BE_NULL);

	dssh_test_reset_global_config();
	return TEST_PASS;
}

/* ================================================================
 * Getter-before-handshake — covers NULL ternary branches in
 * get_kex_name, get_hostkey_name, get_enc_name, get_mac_name.
 * ================================================================ */

static int
test_get_names_before_handshake(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(register_all_algorithms(), 0);
	dssh_transport_set_callbacks(mock_tx_dispatch, mock_rx_dispatch,
	    mock_rxline_dispatch, mock_extra_line_cb);

	dssh_session sess = dssh_session_init(true, 0);
	ASSERT_NOT_NULL(sess);

	ASSERT_NULL(dssh_transport_get_kex_name(sess));
	ASSERT_NULL(dssh_transport_get_hostkey_name(sess));
	ASSERT_NULL(dssh_transport_get_enc_name(sess));
	ASSERT_NULL(dssh_transport_get_mac_name(sess));

	dssh_session_cleanup(sess);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

/* ================================================================
 * set_callbacks after session — covers TOOLATE branch.
 * ================================================================ */

static int
test_set_callbacks_after_session(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(register_all_algorithms(), 0);
	dssh_transport_set_callbacks(mock_tx_dispatch, mock_rx_dispatch,
	    mock_rxline_dispatch, mock_extra_line_cb);

	dssh_session sess = dssh_session_init(true, 0);
	ASSERT_NOT_NULL(sess);

	ASSERT_EQ(dssh_transport_set_callbacks(mock_tx_dispatch,
	    mock_rx_dispatch, mock_rxline_dispatch, mock_extra_line_cb),
	    DSSH_ERROR_TOOLATE);

	dssh_session_cleanup(sess);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

/* ================================================================
 * set_global_request_cb — covers ssh.c:98-103 (never called).
 * ================================================================ */

static int
test_set_global_request_cb(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(register_all_algorithms(), 0);
	dssh_transport_set_callbacks(mock_tx_dispatch, mock_rx_dispatch,
	    mock_rxline_dispatch, mock_extra_line_cb);

	dssh_session sess = dssh_session_init(true, 0);
	ASSERT_NOT_NULL(sess);

	int dummy = 42;
	dssh_session_set_global_request_cb(sess, (void *)0x1, &dummy);
	/* Just verify it doesn't crash — the callback pointer is
	 * stored in the session struct for later dispatch. */

	dssh_session_cleanup(sess);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

/* ================================================================
 * GLOBAL_REQUEST handling — covers ssh-trans.c:753-787 (8+ branches)
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

	/* Register global request callback on server */
	global_request_cb_result = 0;  /* accept */
	memset(global_request_name, 0, sizeof(global_request_name));
	dssh_session_set_global_request_cb(server,
	    (void *)mock_global_request_cb, NULL);

	/* Build GLOBAL_REQUEST: msg_type(1) + string("test-req") + want_reply(1) */
	uint8_t gr[64];
	size_t gp = 0;
	gr[gp++] = 80;  /* SSH_MSG_GLOBAL_REQUEST */
	dssh_serialize_uint32(8, gr, sizeof(gr), &gp);
	memcpy(&gr[gp], "test-req", 8);
	gp += 8;
	gr[gp++] = 1;  /* want_reply = true */

	/* Send GLOBAL_REQUEST from client */
	ASSERT_OK(dssh_transport_send_packet(client, gr, gp, NULL));

	/* Send a follow-up SERVICE_REQUEST so recv_packet returns */
	uint8_t follow[] = { SSH_MSG_SERVICE_REQUEST, 0x42 };
	ASSERT_OK(dssh_transport_send_packet(client, follow, sizeof(follow), NULL));

	/* Server recv_packet: processes GLOBAL_REQUEST internally, returns SERVICE_REQUEST */
	uint8_t msg_type;
	uint8_t *payload;
	size_t payload_len;
	ASSERT_OK(dssh_transport_recv_packet(server, &msg_type, &payload, &payload_len));
	ASSERT_EQ(msg_type, SSH_MSG_SERVICE_REQUEST);

	/* Verify callback was invoked */
	ASSERT_STR_EQ(global_request_name, "test-req");
	ASSERT_TRUE(global_request_want_reply);

	/* Server should have sent REQUEST_SUCCESS (81) reply.
	 * Client reads it. */
	ASSERT_OK(dssh_transport_recv_packet(client, &msg_type, &payload, &payload_len));
	ASSERT_EQ(msg_type, 81);  /* SSH_MSG_REQUEST_SUCCESS */

	dssh_session_cleanup(client);
	dssh_session_cleanup(server);
	mock_io_free(&io);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

static int
test_global_request_rejected(void)
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

	/* Callback returns -1 (reject) */
	global_request_cb_result = -1;
	dssh_session_set_global_request_cb(server,
	    (void *)mock_global_request_cb, NULL);

	uint8_t gr[64];
	size_t gp = 0;
	gr[gp++] = 80;
	dssh_serialize_uint32(4, gr, sizeof(gr), &gp);
	memcpy(&gr[gp], "deny", 4);
	gp += 4;
	gr[gp++] = 1;  /* want_reply */

	ASSERT_OK(dssh_transport_send_packet(client, gr, gp, NULL));

	uint8_t follow[] = { SSH_MSG_SERVICE_REQUEST, 0x43 };
	ASSERT_OK(dssh_transport_send_packet(client, follow, sizeof(follow), NULL));

	uint8_t msg_type;
	uint8_t *payload;
	size_t payload_len;
	ASSERT_OK(dssh_transport_recv_packet(server, &msg_type, &payload, &payload_len));
	ASSERT_EQ(msg_type, SSH_MSG_SERVICE_REQUEST);

	/* Server sent REQUEST_FAILURE (82) */
	ASSERT_OK(dssh_transport_recv_packet(client, &msg_type, &payload, &payload_len));
	ASSERT_EQ(msg_type, 82);  /* SSH_MSG_REQUEST_FAILURE */

	dssh_session_cleanup(client);
	dssh_session_cleanup(server);
	mock_io_free(&io);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

static int
test_global_request_no_reply(void)
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

	global_request_cb_result = 0;
	dssh_session_set_global_request_cb(server,
	    (void *)mock_global_request_cb, NULL);

	/* want_reply = false */
	uint8_t gr[64];
	size_t gp = 0;
	gr[gp++] = 80;
	dssh_serialize_uint32(5, gr, sizeof(gr), &gp);
	memcpy(&gr[gp], "quiet", 5);
	gp += 5;
	gr[gp++] = 0;  /* want_reply = false */

	ASSERT_OK(dssh_transport_send_packet(client, gr, gp, NULL));

	uint8_t follow[] = { SSH_MSG_SERVICE_REQUEST, 0x44 };
	ASSERT_OK(dssh_transport_send_packet(client, follow, sizeof(follow), NULL));

	uint8_t msg_type;
	uint8_t *payload;
	size_t payload_len;
	ASSERT_OK(dssh_transport_recv_packet(server, &msg_type, &payload, &payload_len));
	ASSERT_EQ(msg_type, SSH_MSG_SERVICE_REQUEST);

	/* No reply packet should be generated — client should not receive anything.
	 * We verify by the fact recv_packet returned the SERVICE_REQUEST directly. */

	dssh_session_cleanup(client);
	dssh_session_cleanup(server);
	mock_io_free(&io);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

static int
test_global_request_no_callback(void)
{
	/* No global_request_cb set — should auto-reject with FAILURE */
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
	/* No global_request_cb set */

	uint8_t gr[64];
	size_t gp = 0;
	gr[gp++] = 80;
	dssh_serialize_uint32(6, gr, sizeof(gr), &gp);
	memcpy(&gr[gp], "nocb-r", 6);
	gp += 6;
	gr[gp++] = 1;  /* want_reply = true */

	ASSERT_OK(dssh_transport_send_packet(client, gr, gp, NULL));

	uint8_t follow[] = { SSH_MSG_SERVICE_REQUEST, 0x45 };
	ASSERT_OK(dssh_transport_send_packet(client, follow, sizeof(follow), NULL));

	uint8_t msg_type;
	uint8_t *payload;
	size_t payload_len;
	ASSERT_OK(dssh_transport_recv_packet(server, &msg_type, &payload, &payload_len));
	ASSERT_EQ(msg_type, SSH_MSG_SERVICE_REQUEST);

	/* No callback → gr_res=-1 → FAILURE reply */
	ASSERT_OK(dssh_transport_recv_packet(client, &msg_type, &payload, &payload_len));
	ASSERT_EQ(msg_type, 82);  /* SSH_MSG_REQUEST_FAILURE */

	dssh_session_cleanup(client);
	dssh_session_cleanup(server);
	mock_io_free(&io);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

static int
test_global_request_truncated(void)
{
	/* Truncated GLOBAL_REQUEST — too short for name length field */
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

	/* Just the msg_type byte — not enough for name length */
	uint8_t gr[] = { 80 };
	ASSERT_OK(dssh_transport_send_packet(client, gr, sizeof(gr), NULL));

	/* Follow-up so recv_packet returns */
	uint8_t follow[] = { SSH_MSG_SERVICE_REQUEST, 0x46 };
	ASSERT_OK(dssh_transport_send_packet(client, follow, sizeof(follow), NULL));

	uint8_t msg_type;
	uint8_t *payload;
	size_t payload_len;
	ASSERT_OK(dssh_transport_recv_packet(server, &msg_type, &payload, &payload_len));
	/* Truncated GLOBAL_REQUEST is silently dropped (break from switch
	 * falls through to loop top), so we get the follow-up packet */
	ASSERT_EQ(msg_type, SSH_MSG_SERVICE_REQUEST);

	dssh_session_cleanup(client);
	dssh_session_cleanup(server);
	mock_io_free(&io);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

/* ================================================================
 * DEBUG edge cases — covers truncated/empty message branches
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

	/* DEBUG: type(4) + always_display(1) — no string */
	uint8_t dbg[] = { SSH_MSG_DEBUG, 1 };
	ASSERT_OK(dssh_transport_send_packet(client, dbg, sizeof(dbg), NULL));

	uint8_t follow[] = { SSH_MSG_SERVICE_REQUEST, 0x47 };
	ASSERT_OK(dssh_transport_send_packet(client, follow, sizeof(follow), NULL));

	uint8_t msg_type;
	uint8_t *payload;
	size_t payload_len;
	ASSERT_OK(dssh_transport_recv_packet(server, &msg_type, &payload, &payload_len));
	ASSERT_EQ(msg_type, SSH_MSG_SERVICE_REQUEST);

	/* Callback should have been called with msg_len=0 */
	ASSERT_TRUE(debug_cb_invoked);
	ASSERT_EQ_U(debug_msg_len, 0);

	dssh_session_cleanup(client);
	dssh_session_cleanup(server);
	mock_io_free(&io);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

static int
test_debug_no_callback(void)
{
	/* DEBUG received without debug_cb set — should be silently skipped */
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

	uint8_t dbg[] = { SSH_MSG_DEBUG, 0, 0, 0, 0, 5, 'h', 'e', 'l', 'l', 'o' };
	ASSERT_OK(dssh_transport_send_packet(client, dbg, sizeof(dbg), NULL));

	uint8_t follow[] = { SSH_MSG_SERVICE_REQUEST, 0x48 };
	ASSERT_OK(dssh_transport_send_packet(client, follow, sizeof(follow), NULL));

	uint8_t msg_type;
	uint8_t *payload;
	size_t payload_len;
	ASSERT_OK(dssh_transport_recv_packet(server, &msg_type, &payload, &payload_len));
	ASSERT_EQ(msg_type, SSH_MSG_SERVICE_REQUEST);

	dssh_session_cleanup(client);
	dssh_session_cleanup(server);
	mock_io_free(&io);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

static int
test_unimplemented_short_payload(void)
{
	/* UNIMPLEMENTED with payload < 5 bytes — should be silently skipped */
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
	/* Set callback so we can verify it was NOT invoked */
	dssh_session_set_unimplemented_cb(server,
	    (dssh_unimplemented_cb)(void *)&unimpl_invoked, NULL);

	/* UNIMPLEMENTED with just 2 bytes (needs 5 for seq number) */
	uint8_t unimp[] = { SSH_MSG_UNIMPLEMENTED, 0x00 };
	ASSERT_OK(dssh_transport_send_packet(client, unimp, sizeof(unimp), NULL));

	uint8_t follow[] = { SSH_MSG_SERVICE_REQUEST, 0x49 };
	ASSERT_OK(dssh_transport_send_packet(client, follow, sizeof(follow), NULL));

	uint8_t msg_type;
	uint8_t *payload;
	size_t payload_len;
	ASSERT_OK(dssh_transport_recv_packet(server, &msg_type, &payload, &payload_len));
	ASSERT_EQ(msg_type, SSH_MSG_SERVICE_REQUEST);

	dssh_session_cleanup(client);
	dssh_session_cleanup(server);
	mock_io_free(&io);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

/* ================================================================
 * max_packet_size clamping — covers ssh-trans.c:1419-1422
 * ================================================================ */

static int
test_init_small_packet_size(void)
{
	/* max_packet_size below minimum gets clamped up */
	dssh_test_reset_global_config();
	ASSERT_OK(register_all_algorithms());
	if (test_generate_host_key() < 0)
		return TEST_FAIL;
	dssh_transport_set_callbacks(mock_tx_dispatch, mock_rx_dispatch,
	    mock_rxline_dispatch, mock_extra_line_cb);

	dssh_session sess = dssh_session_init(true, 100);  /* way too small */
	ASSERT_NOT_NULL(sess);
	/* Should be clamped to SSH_BPP_PACKET_SIZE_MIN */
	ASSERT_TRUE(sess->trans.packet_buf_sz >= 33280);

	dssh_session_cleanup(sess);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

static int
test_init_large_packet_size(void)
{
	/* max_packet_size above maximum gets clamped down */
	dssh_test_reset_global_config();
	ASSERT_OK(register_all_algorithms());
	if (test_generate_host_key() < 0)
		return TEST_FAIL;
	dssh_transport_set_callbacks(mock_tx_dispatch, mock_rx_dispatch,
	    mock_rxline_dispatch, mock_extra_line_cb);

	dssh_session sess = dssh_session_init(true, (size_t)128 * 1024 * 1024);
	ASSERT_NOT_NULL(sess);
	/* Should be clamped to SSH_BPP_PACKET_SIZE_MAX (64 MiB) */
	ASSERT_TRUE(sess->trans.packet_buf_sz <= 64 * 1024 * 1024);

	dssh_session_cleanup(sess);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

/* ================================================================
 * build_namelist overflow — covers ssh-trans.c:818-823
 * ================================================================ */

static int
test_build_namelist_overflow(void)
{
	/* Build namelist into a tiny buffer using a mock linked list.
	 * This tests the comma-overflow and name-overflow branches. */
	dssh_test_reset_global_config();

	/* Create two fake kex entries with known names */
	uint8_t e1buf[sizeof(struct dssh_kex_s) + 16];
	uint8_t e2buf[sizeof(struct dssh_kex_s) + 16];
	memset(e1buf, 0, sizeof(e1buf));
	memset(e2buf, 0, sizeof(e2buf));
	struct dssh_kex_s *e1 = (struct dssh_kex_s *)e1buf;
	struct dssh_kex_s *e2 = (struct dssh_kex_s *)e2buf;
	strcpy(e1->name, "alpha");
	strcpy(e2->name, "bravo");
	e1->next = e2;
	e2->next = NULL;

	/* Buffer fits "alpha," but not "alpha,bravo".
	 * Comma is written but "bravo" is truncated. */
	char buf[8];
	size_t len = dssh_test_build_namelist(e1,
	    offsetof(struct dssh_kex_s, name), buf, sizeof(buf));
	/* "alpha," = 6 chars (comma written, name truncated) */
	ASSERT_STR_EQ(buf, "alpha,");
	ASSERT_EQ_U(len, 6);

	/* Buffer too small for comma after "alpha" — covers pos+1>=bufsz */
	char exact[6];  /* pos=5 after "alpha", 5+1>=6 so comma not written */
	len = dssh_test_build_namelist(e1,
	    offsetof(struct dssh_kex_s, name), exact, sizeof(exact));
	ASSERT_STR_EQ(exact, "alpha");
	ASSERT_EQ_U(len, 5);

	/* Buffer too small even for "alpha" */
	char tiny[4];
	len = dssh_test_build_namelist(e1,
	    offsetof(struct dssh_kex_s, name), tiny, sizeof(tiny));
	ASSERT_TRUE(len < sizeof(tiny));

	dssh_test_reset_global_config();
	return TEST_PASS;
}

/* ================================================================
 * Cleanup session that never handshaked — covers cleanup branches
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

	/* All *_selected should be NULL — cleanup should handle gracefully */
	ASSERT_NULL(sess->trans.kex_selected);
	ASSERT_NULL(sess->trans.enc_c2s_selected);
	ASSERT_NULL(sess->trans.mac_c2s_selected);

	dssh_session_cleanup(sess);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

/* ================================================================
 * set_version with long string — covers version_tx TOOLONG
 * ================================================================ */

static int
test_set_version_long_string(void)
{
	dssh_test_reset_global_config();

	/* 250-char version string — should succeed (just under 255 limit with SSH-2.0- prefix) */
	char long_ver[241];
	memset(long_ver, 'A', 240);
	long_ver[240] = 0;
	ASSERT_EQ(dssh_transport_set_version(long_ver, NULL), 0);

	dssh_test_reset_global_config();

	/* 249-char version + comment — exceeds 255 combined */
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
 * set_version high-byte validation — covers is_valid_sw_version
 * and is_valid_comment > 0x7E branches.
 * ================================================================ */

static int
test_set_version_high_byte_version(void)
{
	dssh_test_reset_global_config();

	/* Version with DEL (0x7F) — out of range */
	char bad_ver[] = { 't', 'e', 's', 't', 0x7F, 'v', 0 };
	ASSERT_EQ(dssh_transport_set_version(bad_ver, NULL), DSSH_ERROR_INVALID);

	dssh_test_reset_global_config();

	/* Version with 0x80 — out of range */
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
 * Version line parsing edge cases — covers short-circuit branches
 * in is_version_line and is_20.
 * ================================================================ */

static int
test_is_version_line_ss_not_ssh(void)
{
	/* "SSx-..." — starts with SS but not SSH */
	uint8_t buf[] = "SSx-2.0-test\r\n";
	ASSERT_FALSE(dssh_test_is_version_line(buf, sizeof(buf) - 1));
	return TEST_PASS;
}

static int
test_is_20_bad_minor(void)
{
	/* "SSH-2.x-test" — major is 2 but separator isn't ".0-" */
	uint8_t buf[] = "SSH-2.x-test\r\n";
	ASSERT_FALSE(dssh_test_is_20(buf, sizeof(buf) - 1));
	return TEST_PASS;
}

static int
test_is_20_199_partial(void)
{
	/* "SSH-1.9x-test" — starts like 1.99 but isn't */
	uint8_t buf[] = "SSH-1.9x-test\r\n";
	ASSERT_FALSE(dssh_test_is_20(buf, sizeof(buf) - 1));
	return TEST_PASS;
}

/* ================================================================
 * version_tx — defense-in-depth TOOLONG paths
 *
 * These are unreachable via the public API (set_version validates
 * length first), but the code exists and should be tested.
 * We bypass set_version by writing directly to the global config.
 * ================================================================ */

/* Access to gconf fields for test injection.  These are normally
 * set by dssh_transport_set_version; we bypass it here. */
extern const char *dssh_test_get_sw_version(void);
extern void dssh_test_set_sw_version(const char *v);
extern void dssh_test_set_version_comment(const char *c);

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
 * DH-GEX helpers — parse_bn_mpint and dh_value_valid
 * ================================================================ */

static int
test_parse_bn_mpint_valid(void)
{
	/* Valid mpint: length=1, value=42 */
	uint8_t buf[] = { 0, 0, 0, 1, 42 };
	BIGNUM *bn = NULL;
	int64_t ret = parse_bn_mpint(buf, sizeof(buf), &bn);
	ASSERT_EQ(ret, 5);
	ASSERT_NOT_NULL(bn);
	ASSERT_TRUE(BN_is_word(bn, 42));
	BN_free(bn);
	return TEST_PASS;
}

static int
test_parse_bn_mpint_short_header(void)
{
	/* bufsz < 4 */
	uint8_t buf[] = { 0, 0 };
	BIGNUM *bn = NULL;
	int64_t ret = parse_bn_mpint(buf, sizeof(buf), &bn);
	ASSERT_EQ(ret, (int64_t)DSSH_ERROR_PARSE);
	return TEST_PASS;
}

static int
test_parse_bn_mpint_truncated_data(void)
{
	/* Length says 10, only 2 bytes of data */
	uint8_t buf[] = { 0, 0, 0, 10, 1, 2 };
	BIGNUM *bn = NULL;
	int64_t ret = parse_bn_mpint(buf, sizeof(buf), &bn);
	ASSERT_EQ(ret, (int64_t)DSSH_ERROR_PARSE);
	return TEST_PASS;
}

static int
test_dh_value_valid_zero(void)
{
	BIGNUM *val = BN_new();
	BIGNUM *p = BN_new();
	BN_zero(val);
	BN_set_word(p, 23);
	ASSERT_FALSE(dh_value_valid(val, p));
	BN_free(val);
	BN_free(p);
	return TEST_PASS;
}

static int
test_dh_value_valid_negative(void)
{
	BIGNUM *val = BN_new();
	BIGNUM *p = BN_new();
	BN_set_word(val, 5);
	BN_set_negative(val, 1);
	BN_set_word(p, 23);
	ASSERT_FALSE(dh_value_valid(val, p));
	BN_free(val);
	BN_free(p);
	return TEST_PASS;
}

static int
test_dh_value_valid_equal_to_p(void)
{
	BIGNUM *val = BN_new();
	BIGNUM *p = BN_new();
	BN_set_word(val, 23);
	BN_set_word(p, 23);
	/* val == p is not in [1, p-1] */
	ASSERT_FALSE(dh_value_valid(val, p));
	BN_free(val);
	BN_free(p);
	return TEST_PASS;
}

static int
test_dh_value_valid_greater_than_p(void)
{
	BIGNUM *val = BN_new();
	BIGNUM *p = BN_new();
	BN_set_word(val, 24);
	BN_set_word(p, 23);
	ASSERT_FALSE(dh_value_valid(val, p));
	BN_free(val);
	BN_free(p);
	return TEST_PASS;
}

static int
test_dh_value_valid_ok(void)
{
	BIGNUM *val = BN_new();
	BIGNUM *p = BN_new();
	BN_set_word(val, 5);
	BN_set_word(p, 23);
	/* 5 is in [1, 22] */
	ASSERT_TRUE(dh_value_valid(val, p));
	BN_free(val);
	BN_free(p);
	return TEST_PASS;
}

static int
test_dh_value_valid_one(void)
{
	BIGNUM *val = BN_new();
	BIGNUM *p = BN_new();
	BN_set_word(val, 1);
	BN_set_word(p, 23);
	/* 1 is the minimum valid value */
	ASSERT_TRUE(dh_value_valid(val, p));
	BN_free(val);
	BN_free(p);
	return TEST_PASS;
}

static int
test_dh_value_valid_p_minus_one(void)
{
	BIGNUM *val = BN_new();
	BIGNUM *p = BN_new();
	BN_set_word(val, 22);
	BN_set_word(p, 23);
	/* p-1 is the maximum valid value */
	ASSERT_TRUE(dh_value_valid(val, p));
	BN_free(val);
	BN_free(p);
	return TEST_PASS;
}

/* ================================================================
 * None algorithm module coverage — call the no-op functions
 * directly to get 100% on comp/none.c, enc/none.c, mac/none.c.
 * ================================================================ */

/* ================================================================
 * aes256-ctr / hmac-sha2-256 edge cases — NULL ctx, alloc failure
 * ================================================================ */

static int
test_aes256_ctr_null_ctx(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(register_aes256_ctr(), 0);

	dssh_enc enc = gconf.enc_head;
	ASSERT_NOT_NULL(enc);

	uint8_t buf[16] = {0};
	ASSERT_EQ(enc->encrypt(buf, sizeof(buf), NULL), DSSH_ERROR_INIT);
	ASSERT_EQ(enc->decrypt(buf, sizeof(buf), NULL), DSSH_ERROR_INIT);

	dssh_test_reset_global_config();
	return TEST_PASS;
}

static int
test_aes256_ctr_alloc_fail(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(register_aes256_ctr(), 0);

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

static int
test_hmac_sha2_256_null_ctx(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(register_hmac_sha2_256(), 0);

	dssh_mac mac = gconf.mac_head;
	ASSERT_NOT_NULL(mac);

	uint8_t buf[16] = {0};
	uint8_t out[32];
	ASSERT_EQ(mac->generate(buf, sizeof(buf), out, NULL), DSSH_ERROR_INIT);

	dssh_test_reset_global_config();
	return TEST_PASS;
}

static int
test_hmac_sha2_256_alloc_fail(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(register_hmac_sha2_256(), 0);

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

/* ================================================================
 * None algorithm module coverage
 * ================================================================ */

static int
test_none_comp(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(register_none_comp(), 0);

	dssh_comp comp = gconf.comp_head;
	ASSERT_NOT_NULL(comp);

	uint8_t buf[16] = {0};
	size_t bufsz = sizeof(buf);
	ASSERT_EQ(comp->compress(buf, &bufsz, NULL), 0);
	ASSERT_EQ(comp->uncompress(buf, &bufsz, NULL), 0);

	dssh_test_reset_global_config();
	return TEST_PASS;
}

static int
test_none_enc(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(register_none_enc(), 0);

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
	ASSERT_EQ(register_none_mac(), 0);

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
	ASSERT_OK(dssh_transport_send_packet(client, dbg, sizeof(dbg), NULL));

	/* Send a follow-up so recv_packet returns */
	uint8_t follow[] = { SSH_MSG_SERVICE_REQUEST, 0x47 };
	ASSERT_OK(dssh_transport_send_packet(client, follow, sizeof(follow), NULL));

	uint8_t msg_type;
	uint8_t *payload;
	size_t payload_len;
	dssh_transport_recv_packet(server, &msg_type, &payload, &payload_len);

	/* Debug callback should have been invoked with msg_len clamped to 0 */
	ASSERT_TRUE(debug_cb_invoked);
	ASSERT_EQ(debug_msg_len, (size_t)0);

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

	/* GLOBAL_REQUEST: name_len=100 but only 2 bytes of name + no want_reply */
	uint8_t gr[16];
	size_t gp = 0;
	gr[gp++] = 80; /* SSH_MSG_GLOBAL_REQUEST */
	dssh_serialize_uint32(100, gr, sizeof(gr), &gp); /* name_len=100 */
	gr[gp++] = 'a';
	gr[gp++] = 'b';
	/* Missing: rest of name + want_reply byte */
	ASSERT_OK(dssh_transport_send_packet(client, gr, gp, NULL));

	/* Follow-up packet to flush */
	uint8_t follow[] = { SSH_MSG_SERVICE_REQUEST, 0x47 };
	ASSERT_OK(dssh_transport_send_packet(client, follow, sizeof(follow), NULL));

	uint8_t msg_type;
	uint8_t *payload;
	size_t payload_len;
	dssh_transport_recv_packet(server, &msg_type, &payload, &payload_len);

	dssh_session_cleanup(server);
	dssh_session_cleanup(client);
	mock_io_free(&io);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

/* ================================================================
 * Test table
 * ================================================================ */

static struct dssh_test_entry tests[] = {
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
	{ "packet/empty_payload",            test_packet_empty_payload },

	/* Transport message handling */
	{ "transport/ignore_skipped",        test_ignore_silently_skipped },
	{ "transport/disconnect_terminates", test_disconnect_sets_terminate },
	{ "transport/debug_callback",        test_debug_invokes_callback },
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
	{ "rekey/needed_time",               test_rekey_needed_time },
	{ "rekey/needed_below_threshold",    test_rekey_needed_below_threshold },
	{ "rekey/after_handshake",           test_rekey_after_handshake },
	{ "rekey/session_id_stable",         test_rekey_session_id_stable },
	{ "rekey/encrypted_roundtrip",       test_rekey_encrypted_roundtrip },

	/* RFC conformance */
	{ "rekey/hard_limit_send",           test_rekey_hard_limit_send },
	{ "rekey/hard_limit_recv",           test_rekey_hard_limit_recv },
	{ "rekey/seq_preserved",             test_rekey_seq_preserved },
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
	{ "register/kex_toolate",            test_register_kex_toolate },
	{ "register/kex_empty_name",         test_register_kex_empty_name },
	{ "register/kex_next_not_null",      test_register_kex_next_not_null },
	{ "register/enc_toolate",            test_register_enc_toolate },
	{ "register/mac_toolate",            test_register_mac_toolate },
	{ "register/comp_empty_name",        test_register_comp_empty_name },
	{ "register/key_algo_toolate",       test_register_key_algo_toolate },
	{ "register/key_algo_toolong",       test_register_key_algo_toolong },
	{ "register/key_algo_next_not_null", test_register_key_algo_next_not_null },
	{ "register/enc_toolong",            test_register_enc_toolong },
	{ "register/enc_next_not_null",      test_register_enc_next_not_null },
	{ "register/mac_toolong",            test_register_mac_toolong },
	{ "register/mac_next_not_null",      test_register_mac_next_not_null },
	{ "register/comp_toolate",           test_register_comp_toolate },
	{ "register/comp_next_not_null",     test_register_comp_next_not_null },
	{ "register/lang_basic",             test_register_lang_basic },
	{ "register/lang_empty_name",        test_register_lang_empty_name },
	{ "register/lang_toolate",           test_register_lang_toolate },
	{ "register/lang_next_not_null",     test_register_lang_next_not_null },

	/* Algorithm edge cases */
	{ "aes256_ctr/null_ctx",             test_aes256_ctr_null_ctx },
	{ "aes256_ctr/alloc_fail",           test_aes256_ctr_alloc_fail },
	{ "hmac_sha2_256/null_ctx",          test_hmac_sha2_256_null_ctx },
	{ "hmac_sha2_256/alloc_fail",        test_hmac_sha2_256_alloc_fail },

	/* None module coverage */
	{ "debug/msg_len_exceeds_payload",   test_debug_msg_len_exceeds_payload },
	{ "global_request/name_exceeds",     test_global_request_name_exceeds_payload },

	{ "none/comp",                       test_none_comp },
	{ "none/enc",                        test_none_enc },
	{ "none/mac",                        test_none_mac },

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

	/* DH-GEX parse/validate helpers */
	{ "dhgex/parse_bn_mpint_valid",      test_parse_bn_mpint_valid },
	{ "dhgex/parse_bn_mpint_short",      test_parse_bn_mpint_short_header },
	{ "dhgex/parse_bn_mpint_truncated",  test_parse_bn_mpint_truncated_data },
	{ "dhgex/dh_value_zero",             test_dh_value_valid_zero },
	{ "dhgex/dh_value_negative",         test_dh_value_valid_negative },
	{ "dhgex/dh_value_equal_p",          test_dh_value_valid_equal_to_p },
	{ "dhgex/dh_value_greater_p",        test_dh_value_valid_greater_than_p },
	{ "dhgex/dh_value_ok",              test_dh_value_valid_ok },
	{ "dhgex/dh_value_one",             test_dh_value_valid_one },
	{ "dhgex/dh_value_p_minus_one",      test_dh_value_valid_p_minus_one },
};

DSSH_TEST_MAIN(tests)
