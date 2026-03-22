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
#include "deucessh.h"
#include "deucessh-algorithms.h"
#include "ssh-trans.h"
#include "ssh-internal.h"
#include "dssh_test_internal.h"
#include "mock_io.h"


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

static int
register_all_algorithms(void)
{
	int res;
	res = register_curve25519_sha256();
	if (res < 0)
		return res;
	res = register_ssh_ed25519();
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
	if (ssh_ed25519_generate_key() < 0)
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

	ctx->server = dssh_session_init(false, 0);
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

	ctx.server = dssh_session_init(false, 0);
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

	ctx.server = dssh_session_init(false, 0);
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

	dssh_session server = dssh_session_init(false, 0);
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

	server = dssh_session_init(false, 0);
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

	dssh_session server = dssh_session_init(false, 0);
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

	dssh_session server = dssh_session_init(false, 0);
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

	dssh_session server = dssh_session_init(false, 0);
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

	dssh_session server = dssh_session_init(false, 0);
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

	dssh_session server = dssh_session_init(false, 0);
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

	dssh_session server = dssh_session_init(false, 0);
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

	dssh_session server = dssh_session_init(false, 0);
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

	dssh_session server = dssh_session_init(false, 0);
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

	/* Session lifecycle */
	{ "session/init_cleanup",            test_session_init_cleanup },
	{ "session/terminate",               test_session_terminate },
	{ "session/cleanup_null",            test_session_cleanup_null },
};

DSSH_TEST_MAIN(tests)
