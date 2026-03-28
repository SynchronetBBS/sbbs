/*
 * test_thread_errors.c -- C11 threading failure injection tests.
 *
 * Tests that dssh_thrd_check() correctly terminates the session when
 * mtx_lock, mtx_unlock, cnd_wait, cnd_broadcast, or cnd_signal fail.
 * Uses the separate thrd countdown (dssh_test_thrd_fail_after).
 */

#include <string.h>
#include <threads.h>

#include "dssh_test.h"
#include "deucessh.h"
#include "deucessh-algorithms.h"
#include "deucessh-auth.h"
#include "deucessh-conn.h"
#include "ssh-trans.h"
#include "ssh-internal.h"
#include "dssh_test_internal.h"
#include "dssh_test_ossl.h"
#include "mock_io.h"
#include "test_dhgex_provider.h"

/* ================================================================
 * Mock I/O dispatch
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
		return mock_rxline_client(buf, bufsz, bytes_received,
		    sess, cbdata);
	else
		return mock_rxline_server(buf, bufsz, bytes_received,
		    sess, cbdata);
}

/* ================================================================
 * Algorithm registration + server helpers
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
		res = dssh_register_dh_gex_sha256();
	else if (test_using_sntrup())
		res = dssh_register_sntrup761x25519_sha512();
	else if (test_using_mlkem())
		res = dssh_register_mlkem768x25519_sha256();
	else
		res = dssh_register_curve25519_sha256();
	if (res < 0)
		return res;
	res = test_register_key_algos();
	if (res < 0)
		return res;
	res = dssh_register_aes256_ctr();
	if (res < 0)
		return res;
	res = dssh_register_hmac_sha2_256();
	if (res < 0)
		return res;
	res = dssh_register_none_comp();
	if (res < 0)
		return res;
	return 0;
}

/* ================================================================
 * Handshake helpers
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
	ctx->server_result = dssh_transport_handshake(ctx->server);
	if (ctx->server_result != 0) {
		mock_io_close_c2s(&ctx->io);
		mock_io_close_s2c(&ctx->io);
	}
	return 0;
}

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
 * Tests
 * ================================================================ */

/*
 * send_packet mtx_lock failure: first thrd call in send_packet is
 * mtx_lock(tx_mtx).  Inject failure, verify session terminates.
 */
static int
test_send_lock_fail(void)
{
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_SKIP;
	}

	ASSERT_FALSE(ctx.client->terminate);

	dssh_test_thrd_fail_after(0);
	uint8_t msg[] = { SSH_MSG_SERVICE_REQUEST, 0x42 };
	dssh_transport_send_packet(ctx.client, msg, sizeof(msg), NULL);
	dssh_test_thrd_reset();

	ASSERT_TRUE(ctx.client->terminate);

	handshake_cleanup(&ctx);
	return TEST_PASS;
}

/*
 * send_packet mtx_unlock failure: the last thrd call in send_packet
 * is mtx_unlock(tx_mtx).  After a successful send, inject the unlock
 * failure and verify session terminates.
 */
static int
test_send_unlock_fail(void)
{
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_SKIP;
	}

	ASSERT_FALSE(ctx.client->terminate);

	/* Count how many thrd calls a normal send does */
	dssh_test_thrd_fail_after(1000);
	uint8_t msg[] = { SSH_MSG_SERVICE_REQUEST, 0x42 };
	dssh_transport_send_packet(ctx.client, msg, sizeof(msg), NULL);
	int count = dssh_test_thrd_count();
	dssh_test_thrd_reset();

	ASSERT_FALSE(ctx.client->terminate);
	ASSERT_TRUE(count >= 2);

	/* Fail the last thrd call (mtx_unlock) */
	dssh_test_thrd_fail_after(count - 1);
	dssh_transport_send_packet(ctx.client, msg, sizeof(msg), NULL);
	dssh_test_thrd_reset();

	ASSERT_TRUE(ctx.client->terminate);

	handshake_cleanup(&ctx);
	return TEST_PASS;
}

/*
 * recv_packet mtx_lock failure: first thrd call in recv_packet_raw
 * is mtx_lock(rx_mtx).  Inject failure, verify session terminates.
 */
static int
test_recv_lock_fail(void)
{
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_SKIP;
	}

	/* Send a packet from client so server has something to receive */
	uint8_t msg[] = { SSH_MSG_SERVICE_REQUEST, 0x42 };
	ASSERT_OK(dssh_transport_send_packet(ctx.client, msg,
	    sizeof(msg), NULL));

	ASSERT_FALSE(ctx.server->terminate);

	dssh_test_thrd_fail_after(0);
	uint8_t mt;
	uint8_t *payload;
	size_t plen;
	dssh_transport_recv_packet(ctx.server, &mt, &payload, &plen);
	dssh_test_thrd_reset();

	ASSERT_TRUE(ctx.server->terminate);

	handshake_cleanup(&ctx);
	return TEST_PASS;
}

/*
 * set_terminate best-effort: verify set_terminate does not crash
 * when lock operations fail.  Fail the first lock call.
 */
static int
test_set_terminate_lock_fail(void)
{
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_SKIP;
	}

	/* Start the connection layer so set_terminate exercises
	 * accept_mtx, channel_mtx, and buf_mtx paths. */
	ASSERT_OK(dssh_session_start(ctx.client));
	ASSERT_OK(dssh_session_start(ctx.server));

	ASSERT_FALSE(ctx.client->terminate);

	/* Fail the first lock/broadcast call inside set_terminate.
	 * It should still set terminate and not crash. */
	dssh_test_thrd_fail_after(0);
	dssh_session_set_terminate(ctx.client);
	dssh_test_thrd_reset();

	ASSERT_TRUE(ctx.client->terminate);

	/* Clean up: close pipes so demux threads exit */
	mock_io_close_c2s(&ctx.io);
	mock_io_close_s2c(&ctx.io);
	dssh_session_stop(ctx.client);
	dssh_session_stop(ctx.server);
	dssh_session_cleanup(ctx.server);
	dssh_session_cleanup(ctx.client);
	mock_io_free(&ctx.io);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

/*
 * Sweep: iterate over all thrd calls in send_packet, failing each
 * in turn.  Verify the session always terminates.
 */
static int
test_send_sweep(void)
{
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_SKIP;
	}

	/* Count thrd calls in a normal send */
	dssh_test_thrd_fail_after(1000);
	uint8_t msg[] = { SSH_MSG_SERVICE_REQUEST, 0x42 };
	dssh_transport_send_packet(ctx.client, msg, sizeof(msg), NULL);
	int count = dssh_test_thrd_count();
	dssh_test_thrd_reset();
	ASSERT_TRUE(count >= 2);

	handshake_cleanup(&ctx);

	/* Now fail each thrd call in turn */
	for (int n = 0; n < count; n++) {
		if (handshake_setup(&ctx) < 0) {
			handshake_cleanup(&ctx);
			return TEST_SKIP;
		}

		dssh_test_thrd_fail_after(n);
		dssh_transport_send_packet(ctx.client, msg,
		    sizeof(msg), NULL);
		dssh_test_thrd_reset();

		ASSERT_TRUE(ctx.client->terminate);

		handshake_cleanup(&ctx);
	}

	return TEST_PASS;
}

/*
 * Sweep: iterate over all thrd calls in recv_packet.
 */
static int
test_recv_sweep(void)
{
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_SKIP;
	}

	/* Send a packet */
	uint8_t msg[] = { SSH_MSG_SERVICE_REQUEST, 0x42 };
	ASSERT_OK(dssh_transport_send_packet(ctx.client, msg,
	    sizeof(msg), NULL));

	/* Count thrd calls in a normal recv */
	dssh_test_thrd_fail_after(1000);
	uint8_t mt;
	uint8_t *payload;
	size_t plen;
	dssh_transport_recv_packet(ctx.server, &mt, &payload, &plen);
	int count = dssh_test_thrd_count();
	dssh_test_thrd_reset();
	ASSERT_TRUE(count >= 2);

	handshake_cleanup(&ctx);

	for (int n = 0; n < count; n++) {
		if (handshake_setup(&ctx) < 0) {
			handshake_cleanup(&ctx);
			return TEST_SKIP;
		}

		ASSERT_OK(dssh_transport_send_packet(ctx.client, msg,
		    sizeof(msg), NULL));

		dssh_test_thrd_fail_after(n);
		dssh_transport_recv_packet(ctx.server, &mt, &payload,
		    &plen);
		dssh_test_thrd_reset();

		ASSERT_TRUE(ctx.server->terminate);

		handshake_cleanup(&ctx);
	}

	return TEST_PASS;
}

/* ================================================================
 * Test list
 * ================================================================ */

static struct dssh_test_entry tests[] = {
	{ "thrd/send_lock_fail",      test_send_lock_fail },
	{ "thrd/send_unlock_fail",    test_send_unlock_fail },
	{ "thrd/recv_lock_fail",      test_recv_lock_fail },
	{ "thrd/set_terminate_fail",  test_set_terminate_lock_fail },
	{ "thrd/send_sweep",          test_send_sweep },
	{ "thrd/recv_sweep",          test_recv_sweep },
};

DSSH_TEST_MAIN(tests)
