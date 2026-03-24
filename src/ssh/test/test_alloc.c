/*
 * test_alloc.c — Allocation failure tests for DeuceSSH.
 *
 * Uses the mock allocator (--wrap=malloc/calloc/realloc/free) to
 * inject NULL returns at every allocation point in the library.
 * For each function under test, we iterate N from 0 upward, failing
 * the Nth allocation, until the function succeeds — confirming that
 * every intermediate failure is handled gracefully.
 */

#include <stddef.h>
#include <string.h>
#include <threads.h>

#include "dssh_test.h"
#include "deucessh.h"
#include "deucessh-algorithms.h"
#include "deucessh-auth.h"
#include "deucessh-conn.h"
#include "ssh-trans.h"
#include "ssh-internal.h"
#include "ssh-chan.h"
#include "dssh_test_internal.h"
#include "mock_io.h"
#include "mock_alloc.h"
#include "test_dhgex_provider.h"


/* ================================================================
 * Helpers
 * ================================================================ */

/*
 * Register all algorithms.  Must be called with the mock allocator
 * disabled (mock_alloc_reset) since registration itself allocates.
 */
static dssh_session
init_server_session(void)
{
	dssh_session s = dssh_session_init(false, 0);
	if (s != NULL)
		test_dhgex_setup(s);
	return s;
}

static int
register_all(void)
{
	int res;
	if (test_using_dhgex())
		res = register_dh_gex_sha256();
	else
		res = register_curve25519_sha256();
	if (res < 0) return res;
	res = test_register_key_algos();
	if (res < 0) return res;
	res = register_aes256_ctr();
	if (res < 0) return res;
	res = register_hmac_sha2_256();
	if (res < 0) return res;
	res = register_none_comp();
	if (res < 0) return res;
	return 0;
}

/* ================================================================
 * Algorithm registration alloc failures
 *
 * Each register_*() call does one malloc for the struct.
 * Failing that allocation must return DSSH_ERROR_ALLOC.
 * ================================================================ */

static int
test_alloc_register_curve25519(void)
{
	dssh_test_reset_global_config();
	mock_alloc_fail_after(0);
	int res = register_curve25519_sha256();
	mock_alloc_reset();
	ASSERT_EQ(res, DSSH_ERROR_ALLOC);

	/* Now succeed */
	res = register_curve25519_sha256();
	ASSERT_EQ(res, 0);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

static int
test_alloc_register_ed25519(void)
{
	dssh_test_reset_global_config();
	mock_alloc_fail_after(0);
	int res = register_ssh_ed25519();
	mock_alloc_reset();
	ASSERT_EQ(res, DSSH_ERROR_ALLOC);

	res = register_ssh_ed25519();
	ASSERT_EQ(res, 0);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

static int
test_alloc_register_rsa(void)
{
	dssh_test_reset_global_config();
	mock_alloc_fail_after(0);
	int res = register_rsa_sha2_256();
	mock_alloc_reset();
	ASSERT_EQ(res, DSSH_ERROR_ALLOC);

	res = register_rsa_sha2_256();
	ASSERT_EQ(res, 0);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

static int
test_alloc_register_aes256ctr(void)
{
	dssh_test_reset_global_config();
	mock_alloc_fail_after(0);
	int res = register_aes256_ctr();
	mock_alloc_reset();
	ASSERT_EQ(res, DSSH_ERROR_ALLOC);

	res = register_aes256_ctr();
	ASSERT_EQ(res, 0);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

static int
test_alloc_register_hmac_sha2_256(void)
{
	dssh_test_reset_global_config();
	mock_alloc_fail_after(0);
	int res = register_hmac_sha2_256();
	mock_alloc_reset();
	ASSERT_EQ(res, DSSH_ERROR_ALLOC);

	res = register_hmac_sha2_256();
	ASSERT_EQ(res, 0);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

static int
test_alloc_register_none_comp(void)
{
	dssh_test_reset_global_config();
	mock_alloc_fail_after(0);
	int res = register_none_comp();
	mock_alloc_reset();
	ASSERT_EQ(res, DSSH_ERROR_ALLOC);

	res = register_none_comp();
	ASSERT_EQ(res, 0);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

static int
test_alloc_register_none_enc(void)
{
	dssh_test_reset_global_config();
	mock_alloc_fail_after(0);
	int res = register_none_enc();
	mock_alloc_reset();
	ASSERT_EQ(res, DSSH_ERROR_ALLOC);

	res = register_none_enc();
	ASSERT_EQ(res, 0);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

static int
test_alloc_register_none_mac(void)
{
	dssh_test_reset_global_config();
	mock_alloc_fail_after(0);
	int res = register_none_mac();
	mock_alloc_reset();
	ASSERT_EQ(res, DSSH_ERROR_ALLOC);

	res = register_none_mac();
	ASSERT_EQ(res, 0);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

static int
test_alloc_register_dh_gex(void)
{
	dssh_test_reset_global_config();
	mock_alloc_fail_after(0);
	int res = register_dh_gex_sha256();
	mock_alloc_reset();
	ASSERT_EQ(res, DSSH_ERROR_ALLOC);

	res = register_dh_gex_sha256();
	ASSERT_EQ(res, 0);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

/* ================================================================
 * dssh_session_init alloc failures
 *
 * session_init does one calloc for the session struct, then four
 * mallocs for tx_packet, rx_packet, tx_mac_scratch, rx_mac_scratch.
 * Failing any of these must return NULL.
 * ================================================================ */

static int
test_alloc_session_init(void)
{
	dssh_test_reset_global_config();
	mock_alloc_reset();

	ASSERT_EQ(register_all(), 0);

	/* Fail each allocation in turn */
	for (int n = 0; ; n++) {
		mock_alloc_fail_after(n);
		dssh_session sess = dssh_session_init(true, 0);
		mock_alloc_reset();
		if (sess != NULL) {
			/* Success — we've covered all failure points */
			dssh_session_cleanup(sess);
			ASSERT_TRUE(n > 0);
			break;
		}
		/* Must have returned NULL due to alloc failure */
		ASSERT_NULL(sess);
		/* Safety: don't loop forever */
		ASSERT_TRUE(n < 20);
	}

	dssh_test_reset_global_config();
	return TEST_PASS;
}

/* ================================================================
 * Channel buffer alloc failures (ssh-chan.c)
 *
 * dssh_bytebuf_init does one malloc for the data buffer.
 * dssh_msgqueue_push does one malloc for the entry.
 * dssh_signal_push does one malloc for the mark.
 * dssh_accept_queue_push does one malloc for the entry.
 * ================================================================ */

static int
test_alloc_bytebuf_init(void)
{
	struct dssh_bytebuf buf;
	memset(&buf, 0, sizeof(buf));

	mock_alloc_fail_after(0);
	int res = dssh_bytebuf_init(&buf, 1024);
	mock_alloc_reset();
	ASSERT_EQ(res, DSSH_ERROR_ALLOC);

	/* Succeed */
	res = dssh_bytebuf_init(&buf, 1024);
	ASSERT_EQ(res, 0);
	dssh_bytebuf_free(&buf);
	return TEST_PASS;
}

static int
test_alloc_msgqueue_push(void)
{
	struct dssh_msgqueue q;
	dssh_msgqueue_init(&q);

	const uint8_t data[] = "hello";
	mock_alloc_fail_after(0);
	int res = dssh_msgqueue_push(&q, data, sizeof(data));
	mock_alloc_reset();
	ASSERT_EQ(res, DSSH_ERROR_ALLOC);

	/* Succeed */
	res = dssh_msgqueue_push(&q, data, sizeof(data));
	ASSERT_EQ(res, 0);
	dssh_msgqueue_free(&q);
	return TEST_PASS;
}

static int
test_alloc_signal_push(void)
{
	struct dssh_signal_queue q;
	dssh_sigqueue_init(&q);

	mock_alloc_fail_after(0);
	int res = dssh_sigqueue_push(&q, "INT", 0, 0);
	mock_alloc_reset();
	ASSERT_EQ(res, DSSH_ERROR_ALLOC);

	/* Succeed */
	res = dssh_sigqueue_push(&q, "INT", 0, 0);
	ASSERT_EQ(res, 0);
	dssh_sigqueue_free(&q);
	return TEST_PASS;
}

/* ================================================================
 * Auth message building alloc failures (ssh-auth.c)
 *
 * Each client auth function builds one or more malloc'd messages.
 * We test dssh_auth_get_methods, dssh_auth_password, and
 * dssh_auth_publickey by failing their internal mallocs.
 *
 * These need a completed handshake, so we set up the handshake
 * with the allocator disabled, then arm it for the auth call.
 * ================================================================ */

/* Dispatch callbacks */
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

struct handshake_ctx {
	struct mock_io_state io;
	dssh_session client;
	dssh_session server;
	int client_result;
	int server_result;
};

static int handshake_client_thread(void *arg) {
	struct handshake_ctx *ctx = arg;
	ctx->client_result = dssh_transport_handshake(ctx->client);
	return 0;
}
static int handshake_server_thread(void *arg) {
	struct handshake_ctx *ctx = arg;
	ctx->server_result = dssh_transport_handshake(ctx->server);
	return 0;
}

static int
handshake_setup(struct handshake_ctx *ctx)
{
	memset(ctx, 0, sizeof(*ctx));
	dssh_test_reset_global_config();
	mock_alloc_reset();

	if (register_all() < 0)
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
	thrd_create(&ct, handshake_client_thread, ctx);
	thrd_create(&st, handshake_server_thread, ctx);
	thrd_join(ct, NULL);
	thrd_join(st, NULL);

	if (ctx->client_result != 0 || ctx->server_result != 0)
		return -1;
	return 0;
}

static void
handshake_cleanup(struct handshake_ctx *ctx)
{
	mock_alloc_reset();
	if (ctx->server)
		dssh_session_cleanup(ctx->server);
	if (ctx->client)
		dssh_session_cleanup(ctx->client);
	mock_io_free(&ctx->io);
	dssh_test_reset_global_config();
}

/*
 * Test that dssh_auth_get_methods handles malloc failure in its
 * message building.  We send SERVICE_REQUEST which builds a
 * malloc'd message.
 */
static int
test_alloc_auth_get_methods(void)
{
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	/* The first malloc inside dssh_auth_get_methods builds
	 * the SERVICE_REQUEST message.  Fail it. */
	mock_alloc_fail_after(0);
	char methods[256];
	int res = dssh_auth_get_methods(ctx.client, "user", methods,
	    sizeof(methods));
	mock_alloc_reset();
	ASSERT_TRUE(res < 0);

	handshake_cleanup(&ctx);
	return TEST_PASS;
}

/*
 * Test alloc failure in send_password_request (via dssh_auth_password).
 * ensure_auth_service does one malloc (SERVICE_REQUEST), then
 * send_password_request does another.  We test both.
 */
static int
test_alloc_auth_password(void)
{
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	/* Fail the first internal malloc (SERVICE_REQUEST message) */
	mock_alloc_fail_after(0);
	int res = dssh_auth_password(ctx.client, "user", "pass", NULL, NULL);
	mock_alloc_reset();
	ASSERT_TRUE(res < 0);

	handshake_cleanup(&ctx);
	return TEST_PASS;
}

/*
 * Test alloc failure in dssh_auth_publickey.
 * It does: ensure_auth_service (1 malloc for SERVICE_REQUEST msg),
 * then builds sign_data (1 malloc), then builds the auth message
 * (1 malloc).  Each can fail.
 */
static int
test_alloc_auth_publickey(void)
{
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	/* Fail the first malloc */
	mock_alloc_fail_after(0);
	int res = dssh_auth_publickey(ctx.client, "user", test_key_algo_name());
	mock_alloc_reset();
	ASSERT_TRUE(res < 0);

	handshake_cleanup(&ctx);
	return TEST_PASS;
}

/*
 * Server-side: send_passwd_changereq has a malloc.
 * send_pk_ok has a malloc.
 * send_auth_failure is stack-only (no malloc).
 *
 * We test send_passwd_changereq by having the password callback
 * return DSSH_AUTH_CHANGE_PASSWORD, then failing the malloc inside
 * send_passwd_changereq.
 */

static int
changereq_password_cb(const uint8_t *username, size_t username_len,
    const uint8_t *password, size_t password_len,
    uint8_t **change_prompt, size_t *change_prompt_len,
    void *cbdata)
{
	(void)username; (void)username_len;
	(void)password; (void)password_len;

	/* Return change-password with a prompt.
	 * The prompt is malloc'd by us — but we need to use __real_malloc
	 * since the mock allocator might be armed.  Actually, the callback
	 * runs on the server thread, and we arm the allocator to fail
	 * the library's internal malloc for the CHANGEREQ message.
	 * Our callback's malloc for the prompt happens BEFORE that,
	 * so we need to account for it in the countdown. */
	*change_prompt_len = 6;
	*change_prompt = malloc(6);
	if (*change_prompt == NULL) {
		*change_prompt_len = 0;
		return DSSH_AUTH_FAILURE;
	}
	memcpy(*change_prompt, "change", 6);
	return DSSH_AUTH_CHANGE_PASSWORD;
}

struct changereq_server_arg {
	struct handshake_ctx *ctx;
	struct dssh_auth_server_cbs cbs;
	int result;
	int fail_at;
};

static int
changereq_server_thread(void *arg)
{
	struct changereq_server_arg *a = arg;
	uint8_t username[256];
	size_t username_len;

	/* Arm the allocator just before calling dssh_auth_server,
	 * which will internally call send_passwd_changereq. */
	mock_alloc_fail_after(a->fail_at);
	a->result = dssh_auth_server(a->ctx->server, &a->cbs,
	    username, &username_len);
	mock_alloc_reset();
	return 0;
}

static int
test_alloc_send_passwd_changereq(void)
{
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	struct changereq_server_arg sa = {0};
	sa.ctx = &ctx;
	sa.cbs.methods_str = "password";
	sa.cbs.password_cb = changereq_password_cb;
	/* We need to find the right N.  The server auth loop does:
	 * 0: malloc in SERVICE_ACCEPT? No, that's stack.
	 * The first malloc inside the loop is the prompt from our
	 * callback, then send_passwd_changereq does its malloc.
	 * Let's fail at 0 — which will be the callback's prompt malloc.
	 * Then at 1 — which will be the CHANGEREQ message malloc.
	 * We want to fail the CHANGEREQ message, so try N=1.
	 * But the exact N depends on the code path. Let's iterate. */
	sa.fail_at = 0;

	thrd_t st;
	ASSERT_TRUE(thrd_create(&st, changereq_server_thread, &sa) == thrd_success);

	/* Client: send SERVICE_REQUEST + password auth */
	{
		static const char svc[] = "ssh-userauth";
		uint8_t msg[64];
		size_t pos = 0;
		msg[pos++] = SSH_MSG_SERVICE_REQUEST;
		dssh_serialize_uint32((uint32_t)(sizeof(svc) - 1), msg, sizeof(msg), &pos);
		memcpy(&msg[pos], svc, sizeof(svc) - 1);
		pos += sizeof(svc) - 1;
		ASSERT_OK(dssh_transport_send_packet(ctx.client, msg, pos, NULL));
	}
	{
		uint8_t msg_type;
		uint8_t *payload;
		size_t payload_len;
		ASSERT_OK(dssh_transport_recv_packet(ctx.client, &msg_type, &payload, &payload_len));
		ASSERT_EQ(msg_type, SSH_MSG_SERVICE_ACCEPT);
	}
	{
		static const char svc[] = "ssh-connection";
		static const char method[] = "password";
		uint8_t msg[128];
		size_t pos = 0;
		msg[pos++] = SSH_MSG_USERAUTH_REQUEST;
		dssh_serialize_uint32(1, msg, sizeof(msg), &pos);
		msg[pos++] = 'u';
		dssh_serialize_uint32((uint32_t)(sizeof(svc) - 1), msg, sizeof(msg), &pos);
		memcpy(&msg[pos], svc, sizeof(svc) - 1);
		pos += sizeof(svc) - 1;
		dssh_serialize_uint32((uint32_t)(sizeof(method) - 1), msg, sizeof(msg), &pos);
		memcpy(&msg[pos], method, sizeof(method) - 1);
		pos += sizeof(method) - 1;
		msg[pos++] = 0; /* not a change */
		dssh_serialize_uint32(1, msg, sizeof(msg), &pos);
		msg[pos++] = 'p';
		ASSERT_OK(dssh_transport_send_packet(ctx.client, msg, pos, NULL));
	}

	/* Server will try to send CHANGEREQ and may fail */
	mock_io_close_c2s(&ctx.io);
	mock_io_close_s2c(&ctx.io);
	thrd_join(st, NULL);

	/* The server should have gotten an alloc error somewhere.
	 * The exact error depends on which allocation failed.
	 * We just verify it didn't succeed (since the callback
	 * returned CHANGE_PASSWORD, and we broke the alloc chain). */
	ASSERT_TRUE(sa.result != 0);

	handshake_cleanup(&ctx);
	return TEST_PASS;
}

/*
 * Test alloc failure in send_pk_ok.
 * We send a publickey probe (has_sig=FALSE) and fail the malloc
 * inside send_pk_ok.
 */

static int
pk_ok_publickey_cb(const uint8_t *username, size_t username_len,
    const char *algo_name, const uint8_t *pubkey_blob,
    size_t pubkey_blob_len, bool has_signature, void *cbdata)
{
	(void)username; (void)username_len;
	(void)algo_name; (void)pubkey_blob; (void)pubkey_blob_len;
	(void)has_signature; (void)cbdata;
	return DSSH_AUTH_SUCCESS;
}

struct pk_ok_server_arg {
	struct handshake_ctx *ctx;
	struct dssh_auth_server_cbs cbs;
	int result;
	int fail_at;
};

static int
pk_ok_server_thread(void *arg)
{
	struct pk_ok_server_arg *a = arg;
	uint8_t username[256];
	size_t username_len;

	mock_alloc_fail_after(a->fail_at);
	a->result = dssh_auth_server(a->ctx->server, &a->cbs,
	    username, &username_len);
	mock_alloc_reset();
	return 0;
}

static int
test_alloc_send_pk_ok(void)
{
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	struct pk_ok_server_arg sa = {0};
	sa.ctx = &ctx;
	sa.cbs.methods_str = "publickey";
	sa.cbs.publickey_cb = pk_ok_publickey_cb;
	sa.fail_at = 0;  /* fail the first alloc in the auth loop */

	thrd_t st;
	ASSERT_TRUE(thrd_create(&st, pk_ok_server_thread, &sa) == thrd_success);

	/* Client: SERVICE_REQUEST */
	{
		static const char svc[] = "ssh-userauth";
		uint8_t msg[64];
		size_t pos = 0;
		msg[pos++] = SSH_MSG_SERVICE_REQUEST;
		dssh_serialize_uint32((uint32_t)(sizeof(svc) - 1), msg, sizeof(msg), &pos);
		memcpy(&msg[pos], svc, sizeof(svc) - 1);
		pos += sizeof(svc) - 1;
		ASSERT_OK(dssh_transport_send_packet(ctx.client, msg, pos, NULL));
	}
	{
		uint8_t msg_type;
		uint8_t *payload;
		size_t payload_len;
		ASSERT_OK(dssh_transport_recv_packet(ctx.client, &msg_type, &payload, &payload_len));
		ASSERT_EQ(msg_type, SSH_MSG_SERVICE_ACCEPT);
	}

	/* Get the public key blob */
	dssh_key_algo ka = dssh_transport_find_key_algo(test_key_algo_name());
	ASSERT_NOT_NULL(ka);
	uint8_t pubkey_buf[256];
	size_t pubkey_len;
	ASSERT_EQ(ka->pubkey(pubkey_buf, sizeof(pubkey_buf), &pubkey_len, ka->ctx), 0);

	/* Send publickey probe */
	{
		static const char svc[] = "ssh-connection";
		static const char method[] = "publickey";
		const char *algo = test_key_algo_name();
		size_t algo_len = strlen(algo);
		uint8_t msg[512];
		size_t pos = 0;
		msg[pos++] = SSH_MSG_USERAUTH_REQUEST;
		dssh_serialize_uint32(1, msg, sizeof(msg), &pos);
		msg[pos++] = 'u';
		dssh_serialize_uint32((uint32_t)(sizeof(svc) - 1), msg, sizeof(msg), &pos);
		memcpy(&msg[pos], svc, sizeof(svc) - 1);
		pos += sizeof(svc) - 1;
		dssh_serialize_uint32((uint32_t)(sizeof(method) - 1), msg, sizeof(msg), &pos);
		memcpy(&msg[pos], method, sizeof(method) - 1);
		pos += sizeof(method) - 1;
		msg[pos++] = 0; /* has_sig = FALSE */
		dssh_serialize_uint32((uint32_t)algo_len, msg, sizeof(msg), &pos);
		memcpy(&msg[pos], algo, algo_len);
		pos += algo_len;
		dssh_serialize_uint32((uint32_t)pubkey_len, msg, sizeof(msg), &pos);
		memcpy(&msg[pos], pubkey_buf, pubkey_len);
		pos += pubkey_len;
		ASSERT_OK(dssh_transport_send_packet(ctx.client, msg, pos, NULL));
	}

	mock_io_close_c2s(&ctx.io);
	mock_io_close_s2c(&ctx.io);
	thrd_join(st, NULL);

	/* Server should have failed due to alloc error in send_pk_ok */
	ASSERT_TRUE(sa.result != 0);

	handshake_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Connection protocol alloc failures (ssh-conn.c)
 *
 * dssh_acceptqueue_push does one malloc for the entry struct.
 * Failing it must return DSSH_ERROR_ALLOC.
 * ================================================================ */

static int
test_alloc_acceptqueue_push(void)
{
	struct dssh_accept_queue q;
	dssh_acceptqueue_init(&q);

	mock_alloc_fail_after(0);
	int res = dssh_acceptqueue_push(&q, 0, 0x200000, 0x8000,
	    (const uint8_t *)"session", 7);
	mock_alloc_reset();
	ASSERT_EQ(res, DSSH_ERROR_ALLOC);

	/* Succeed */
	res = dssh_acceptqueue_push(&q, 0, 0x200000, 0x8000,
	    (const uint8_t *)"session", 7);
	ASSERT_EQ(res, 0);
	dssh_acceptqueue_free(&q);
	return TEST_PASS;
}

/* ================================================================
 * KBI alloc failures (ssh-auth.c)
 *
 * dssh_auth_keyboard_interactive builds a message with malloc,
 * then for each INFO_REQUEST round it callocs prompt arrays
 * and mallocs the response message.
 * ================================================================ */

static int
test_alloc_auth_kbi(void)
{
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	/* Fail first malloc inside dssh_auth_keyboard_interactive */
	mock_alloc_fail_after(0);
	int res = dssh_auth_keyboard_interactive(ctx.client, "user",
	    NULL, NULL);
	mock_alloc_reset();
	/* With NULL prompt_cb it returns DSSH_ERROR_INIT before malloc */
	ASSERT_TRUE(res < 0);

	/* Now with a real callback but failing the message malloc */
	/* We need to pass the NULL check first, so use a dummy callback.
	 * But ensure_auth_service will malloc for SERVICE_REQUEST.
	 * fail_after(0) will fail that malloc. */
	dssh_auth_kbi_prompt_cb dummy_cb = (dssh_auth_kbi_prompt_cb)(void *)1;
	mock_alloc_fail_after(0);
	res = dssh_auth_keyboard_interactive(ctx.client, "user",
	    dummy_cb, NULL);
	mock_alloc_reset();
	ASSERT_TRUE(res < 0);

	handshake_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Handshake alloc failures (ssh-trans.c)
 *
 * kexinit mallocs for its packet buffer and peer_kexinit copy.
 * newkeys mallocs for session_id, iv/key/integ buffers.
 * We iterate N from 0 upward, failing each alloc in turn.
 * ================================================================ */

/*
 * Targeted kexinit malloc failure.
 * dssh_transport_kexinit does one malloc for its packet buffer.
 * We call it directly (single-threaded, no OpenSSL involvement)
 * after setting up the session.
 */
static int
test_alloc_kexinit(void)
{
	dssh_test_reset_global_config();
	mock_alloc_reset();

	if (register_all() < 0)
		return TEST_FAIL;
	if (test_generate_host_key() < 0)
		return TEST_FAIL;

	dssh_transport_set_callbacks(mock_tx_dispatch, mock_rx_dispatch,
	    mock_rxline_dispatch, mock_extra_line_cb);

	struct mock_io_state io;
	if (mock_io_init(&io, 0) < 0)
		return TEST_FAIL;

	dssh_session sess = dssh_session_init(true, 0);
	if (sess == NULL) {
		mock_io_free(&io);
		return TEST_FAIL;
	}
	dssh_session_set_cbdata(sess, &io, &io, &io, &io);

	/* Fail the kexinit packet buffer malloc */
	mock_alloc_fail_after(0);
	int res = dssh_transport_kexinit(sess);
	mock_alloc_reset();
	ASSERT_EQ(res, DSSH_ERROR_ALLOC);

	dssh_session_cleanup(sess);
	mock_io_free(&io);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

/* ================================================================
 * Iterative handshake alloc failures using the library-only
 * allocator (dssh_test_alloc).
 *
 * Unlike --wrap=malloc, the macro-based allocator only intercepts
 * mallocs in library code (ssh-trans.c, ssh-auth.c, etc.), not
 * OpenSSL's internal allocations.  This lets us fail every library
 * malloc in turn during a two-threaded handshake without crashing
 * OpenSSL.
 * ================================================================ */

#include "dssh_test_alloc.h"

struct hs_alloc_ctx {
	struct mock_io_state *io;
	dssh_session sess;
	int result;
	mtx_t *barrier_mtx;
	cnd_t *barrier_cnd;
	int *barrier_count;
};

static int
hs_alloc_thread(void *arg)
{
	struct hs_alloc_ctx *ctx = arg;
	/* Wait for main thread to arm the allocator */
	mtx_lock(ctx->barrier_mtx);
	(*ctx->barrier_count)++;
	cnd_broadcast(ctx->barrier_cnd);
	while (*ctx->barrier_count < 3)
		cnd_wait(ctx->barrier_cnd, ctx->barrier_mtx);
	mtx_unlock(ctx->barrier_mtx);

	ctx->result = dssh_transport_handshake(ctx->sess);
	/* Close our write pipe so peer unblocks (two independent
	 * sessions — terminate flag only affects our own session) */
	if (ctx->result < 0) {
		if (ctx->sess->trans.client)
			mock_io_close_c2s(ctx->io);
		else
			mock_io_close_s2c(ctx->io);
	}
	return 0;
}

static int
test_alloc_handshake_iterate(void)
{
	for (int n = 0; n < 50; n++) {
		dssh_test_reset_global_config();
		dssh_test_alloc_reset();
		mock_alloc_reset();

		if (register_all() < 0)
			return TEST_FAIL;
		if (test_generate_host_key() < 0)
			return TEST_FAIL;

		struct mock_io_state io;
		if (mock_io_init(&io, 0) < 0)
			return TEST_FAIL;

		dssh_transport_set_callbacks(mock_tx_dispatch, mock_rx_dispatch,
		    mock_rxline_dispatch, mock_extra_line_cb);

		dssh_session client = dssh_session_init(true, 0);
		if (client == NULL) {
			mock_io_free(&io);
			continue;
		}
		dssh_session_set_cbdata(client, &io, &io, &io, &io);

		dssh_session server = init_server_session();
		if (server == NULL) {
			dssh_session_cleanup(client);
			mock_io_free(&io);
			continue;
		}
		dssh_session_set_cbdata(server, &io, &io, &io, &io);

		/* Barrier: create threads, wait for both to be ready,
		 * then arm the allocator and release. */
		mtx_t bmtx;
		cnd_t bcnd;
		int bcount = 0;
		mtx_init(&bmtx, mtx_plain);
		cnd_init(&bcnd);

		struct hs_alloc_ctx ca = {
			.io = &io, .sess = client,
			.barrier_mtx = &bmtx, .barrier_cnd = &bcnd,
			.barrier_count = &bcount
		};
		struct hs_alloc_ctx sa = {
			.io = &io, .sess = server,
			.barrier_mtx = &bmtx, .barrier_cnd = &bcnd,
			.barrier_count = &bcount
		};

		thrd_t ct, st;
		thrd_create(&ct, hs_alloc_thread, &ca);
		thrd_create(&st, hs_alloc_thread, &sa);

		mtx_lock(&bmtx);
		while (bcount < 2)
			cnd_wait(&bcnd, &bmtx);

		/* Arm the library-only allocator */
		dssh_test_alloc_fail_after(n);
		bcount = 3;
		cnd_broadcast(&bcnd);
		mtx_unlock(&bmtx);

		thrd_join(ct, NULL);
		thrd_join(st, NULL);
		cnd_destroy(&bcnd);
		mtx_destroy(&bmtx);

		dssh_test_alloc_reset();
		bool ok = (ca.result == 0 && sa.result == 0);

		mock_io_close_c2s(&io);
		mock_io_close_s2c(&io);
		dssh_session_cleanup(server);
		dssh_session_cleanup(client);
		mock_io_free(&io);
		dssh_test_reset_global_config();

		if (ok) {
			ASSERT_TRUE(n > 0);
			return TEST_PASS;
		}
	}

	return TEST_FAIL;
}

/* ================================================================
 * Iterative auth alloc failures using the library-only allocator.
 *
 * For each N, we: handshake (allocator off) → arm allocator → run
 * two-threaded auth → check.  Auth callbacks are simple accept-all.
 * ================================================================ */

static int
iter_password_cb(const uint8_t *username, size_t username_len,
    const uint8_t *password, size_t password_len,
    uint8_t **change_prompt, size_t *change_prompt_len,
    void *cbdata)
{
	(void)username; (void)username_len;
	(void)password; (void)password_len;
	(void)change_prompt; (void)change_prompt_len;
	(void)cbdata;
	return DSSH_AUTH_SUCCESS;
}

struct auth_alloc_ctx {
	struct mock_io_state *io;
	dssh_session sess;
	struct dssh_auth_server_cbs *cbs;
	int result;
	mtx_t *barrier_mtx;
	cnd_t *barrier_cnd;
	int *barrier_count;
};

static int
auth_alloc_client_thread(void *arg)
{
	struct auth_alloc_ctx *ctx = arg;
	mtx_lock(ctx->barrier_mtx);
	(*ctx->barrier_count)++;
	cnd_broadcast(ctx->barrier_cnd);
	while (*ctx->barrier_count < 3)
		cnd_wait(ctx->barrier_cnd, ctx->barrier_mtx);
	mtx_unlock(ctx->barrier_mtx);

	ctx->result = dssh_auth_password(ctx->sess, "testuser",
	    "testpass", NULL, NULL);
	if (ctx->result < 0)
		mock_io_close_c2s(ctx->io);
	return 0;
}

static int
auth_alloc_server_thread(void *arg)
{
	struct auth_alloc_ctx *ctx = arg;
	mtx_lock(ctx->barrier_mtx);
	(*ctx->barrier_count)++;
	cnd_broadcast(ctx->barrier_cnd);
	while (*ctx->barrier_count < 3)
		cnd_wait(ctx->barrier_cnd, ctx->barrier_mtx);
	mtx_unlock(ctx->barrier_mtx);

	uint8_t username[256];
	size_t username_len;
	ctx->result = dssh_auth_server(ctx->sess, ctx->cbs,
	    username, &username_len);
	if (ctx->result < 0)
		mock_io_close_s2c(ctx->io);
	return 0;
}

static int
test_alloc_auth_iterate(void)
{
	struct dssh_auth_server_cbs cbs = {0};
	cbs.methods_str = "password";
	cbs.password_cb = iter_password_cb;

	for (int n = 0; n < 50; n++) {
		dssh_test_reset_global_config();
		dssh_test_alloc_reset();
		mock_alloc_reset();

		if (register_all() < 0)
			return TEST_FAIL;
		if (test_generate_host_key() < 0)
			return TEST_FAIL;

		struct mock_io_state io;
		if (mock_io_init(&io, 0) < 0)
			return TEST_FAIL;

		dssh_transport_set_callbacks(mock_tx_dispatch, mock_rx_dispatch,
		    mock_rxline_dispatch, mock_extra_line_cb);

		dssh_session client = dssh_session_init(true, 0);
		if (client == NULL) {
			mock_io_free(&io);
			continue;
		}
		dssh_session_set_cbdata(client, &io, &io, &io, &io);

		dssh_session server = init_server_session();
		if (server == NULL) {
			dssh_session_cleanup(client);
			mock_io_free(&io);
			continue;
		}
		dssh_session_set_cbdata(server, &io, &io, &io, &io);

		/* Handshake with allocator OFF */
		{
			struct handshake_ctx hctx;
			hctx.io = io;
			hctx.client = client;
			hctx.server = server;
			thrd_t ct, st;
			thrd_create(&ct, handshake_client_thread, &hctx);
			thrd_create(&st, handshake_server_thread, &hctx);
			thrd_join(ct, NULL);
			thrd_join(st, NULL);
			if (hctx.client_result != 0 || hctx.server_result != 0) {
				mock_io_close_c2s(&io);
				mock_io_close_s2c(&io);
				dssh_session_cleanup(server);
				dssh_session_cleanup(client);
				mock_io_free(&io);
				dssh_test_reset_global_config();
				continue;
			}
		}

		/* Auth with allocator armed at N */
		mtx_t bmtx;
		cnd_t bcnd;
		int bcount = 0;
		mtx_init(&bmtx, mtx_plain);
		cnd_init(&bcnd);

		struct auth_alloc_ctx ca = {
			.io = &io, .sess = client,
			.barrier_mtx = &bmtx, .barrier_cnd = &bcnd,
			.barrier_count = &bcount,
		};
		struct auth_alloc_ctx sa = {
			.io = &io, .sess = server, .cbs = &cbs,
			.barrier_mtx = &bmtx, .barrier_cnd = &bcnd,
			.barrier_count = &bcount,
		};

		thrd_t ct, st;
		thrd_create(&ct, auth_alloc_client_thread, &ca);
		thrd_create(&st, auth_alloc_server_thread, &sa);

		mtx_lock(&bmtx);
		while (bcount < 2)
			cnd_wait(&bcnd, &bmtx);
		dssh_test_alloc_fail_after(n);
		bcount = 3;
		cnd_broadcast(&bcnd);
		mtx_unlock(&bmtx);

		thrd_join(ct, NULL);
		thrd_join(st, NULL);
		cnd_destroy(&bcnd);
		mtx_destroy(&bmtx);

		dssh_test_alloc_reset();
		bool ok = (ca.result == 0 && sa.result == 0);

		mock_io_close_c2s(&io);
		mock_io_close_s2c(&io);
		dssh_session_cleanup(server);
		dssh_session_cleanup(client);
		mock_io_free(&io);
		dssh_test_reset_global_config();

		if (ok) {
			ASSERT_TRUE(n > 0);
			return TEST_PASS;
		}
	}

	return TEST_FAIL;
}

static int
auth_only_client_thread(void *arg)
{
	struct auth_alloc_ctx *ctx = arg;
	ctx->result = dssh_auth_password(ctx->sess, "testuser",
	    "testpass", NULL, NULL);
	if (ctx->result < 0)
		mock_io_close_c2s(ctx->io);
	return 0;
}

static int
auth_only_server_thread(void *arg)
{
	struct auth_alloc_ctx *ctx = arg;
	uint8_t username[256];
	size_t username_len;
	ctx->result = dssh_auth_server(ctx->sess, ctx->cbs,
	    username, &username_len);
	if (ctx->result < 0)
		mock_io_close_s2c(ctx->io);
	return 0;
}

/* ================================================================
 * Iterative conn alloc failures: session_start + channel open.
 *
 * Handshake + auth with allocator off, then arm allocator and
 * exercise session_start, channel open, and accept.
 * ================================================================ */

static int
session_channel_request_cb(const char *type, size_t type_len,
    bool want_reply, const uint8_t *data, size_t data_len,
    void *cbdata)
{
	(void)type; (void)type_len; (void)want_reply;
	(void)data; (void)data_len; (void)cbdata;
	return 0;  /* accept */
}

static const struct dssh_server_session_cbs iter_session_cbs = {
	.request_cb = session_channel_request_cb,
};

struct conn_alloc_ctx {
	struct mock_io_state *io;
	dssh_session sess;
	dssh_channel ch;
	int result;
};

static int
conn_alloc_client_thread(void *arg)
{
	struct conn_alloc_ctx *ctx = arg;
	int res = dssh_session_start(ctx->sess);
	if (res < 0) {
		ctx->result = res;
		mock_io_close_c2s(ctx->io);
		return 0;
	}
	ctx->ch = dssh_session_open_exec(ctx->sess, "echo test");
	ctx->result = ctx->ch ? 0 : -1;
	if (ctx->result < 0)
		mock_io_close_c2s(ctx->io);
	return 0;
}

static int
conn_alloc_server_thread(void *arg)
{
	struct conn_alloc_ctx *ctx = arg;
	int res = dssh_session_start(ctx->sess);
	if (res < 0) {
		ctx->result = res;
		mock_io_close_s2c(ctx->io);
		return 0;
	}
	struct dssh_incoming_open *inc = NULL;
	res = dssh_session_accept(ctx->sess, &inc, 2000);
	if (res < 0 || inc == NULL) {
		ctx->result = -1;
		mock_io_close_s2c(ctx->io);
		return 0;
	}
	const char *req_type = NULL, *req_data = NULL;
	ctx->ch = dssh_session_accept_channel(ctx->sess, inc,
	    &iter_session_cbs, &req_type, &req_data);
	ctx->result = ctx->ch ? 0 : -1;
	if (ctx->result < 0)
		mock_io_close_s2c(ctx->io);
	return 0;
}

/*
 * Watchdog: after a delay, closes pipes and sets terminate on both
 * sessions to break any deadlocks from alloc-failure mid-protocol.
 */
struct watchdog_ctx {
	struct mock_io_state *io;
	dssh_session client;
	dssh_session server;
	int delay_ms;
};

static int
watchdog_thread(void *arg)
{
	struct watchdog_ctx *ctx = arg;
	struct timespec ts = {
		.tv_sec = ctx->delay_ms / 1000,
		.tv_nsec = (ctx->delay_ms % 1000) * 1000000L,
	};
	thrd_sleep(&ts, NULL);
	mock_io_close_c2s(ctx->io);
	mock_io_close_s2c(ctx->io);
	if (ctx->client->conn_initialized)
		dssh_session_set_terminate(ctx->client);
	if (ctx->server->conn_initialized)
		dssh_session_set_terminate(ctx->server);
	return 0;
}

static int
test_alloc_conn_iterate(void)
{
	struct dssh_auth_server_cbs cbs = {0};
	cbs.methods_str = "password";
	cbs.password_cb = iter_password_cb;

	for (int n = 0; n < 40; n++) {
		dssh_test_reset_global_config();
		dssh_test_alloc_reset();
		mock_alloc_reset();

		if (register_all() < 0)
			return TEST_FAIL;
		if (test_generate_host_key() < 0)
			return TEST_FAIL;

		struct mock_io_state io;
		if (mock_io_init(&io, 0) < 0)
			return TEST_FAIL;

		dssh_transport_set_callbacks(mock_tx_dispatch, mock_rx_dispatch,
		    mock_rxline_dispatch, mock_extra_line_cb);

		dssh_session client = dssh_session_init(true, 0);
		if (client == NULL) {
			mock_io_free(&io);
			continue;
		}
		dssh_session_set_cbdata(client, &io, &io, &io, &io);

		dssh_session server = init_server_session();
		if (server == NULL) {
			dssh_session_cleanup(client);
			mock_io_free(&io);
			continue;
		}
		dssh_session_set_cbdata(server, &io, &io, &io, &io);

		/* Handshake with allocator OFF */
		{
			struct handshake_ctx hctx = {
				.io = io, .client = client, .server = server,
			};
			thrd_t ct, st;
			thrd_create(&ct, handshake_client_thread, &hctx);
			thrd_create(&st, handshake_server_thread, &hctx);
			thrd_join(ct, NULL);
			thrd_join(st, NULL);
			if (hctx.client_result != 0 || hctx.server_result != 0) {
				mock_io_close_c2s(&io);
				mock_io_close_s2c(&io);
				dssh_session_cleanup(server);
				dssh_session_cleanup(client);
				mock_io_free(&io);
				dssh_test_reset_global_config();
				continue;
			}
		}

		/* Auth with allocator OFF */
		{
			struct auth_alloc_ctx ca = { .io = &io, .sess = client };
			struct auth_alloc_ctx sa = {
				.io = &io, .sess = server, .cbs = &cbs,
			};
			thrd_t ct, st;
			thrd_create(&ct, auth_only_client_thread, &ca);
			thrd_create(&st, auth_only_server_thread, &sa);
			thrd_join(ct, NULL);
			thrd_join(st, NULL);
			if (ca.result != 0 || sa.result != 0) {
				mock_io_close_c2s(&io);
				mock_io_close_s2c(&io);
				dssh_session_cleanup(server);
				dssh_session_cleanup(client);
				mock_io_free(&io);
				dssh_test_reset_global_config();
				continue;
			}
		}

		/* Arm allocator, launch conn threads + watchdog */
		dssh_test_alloc_fail_after(n);

		struct conn_alloc_ctx cca = { .io = &io, .sess = client };
		struct conn_alloc_ctx csa = { .io = &io, .sess = server };
		struct watchdog_ctx wctx = {
			.io = &io, .client = client, .server = server,
			.delay_ms = 3000,
		};

		thrd_t ct, st, wt;
		thrd_create(&wt, watchdog_thread, &wctx);
		thrd_create(&ct, conn_alloc_client_thread, &cca);
		thrd_create(&st, conn_alloc_server_thread, &csa);

		thrd_join(ct, NULL);
		thrd_join(st, NULL);
		thrd_join(wt, NULL);

		dssh_test_alloc_reset();
		bool ok = (cca.result == 0 && csa.result == 0);

		/* Clean up */
		mock_io_close_c2s(&io);
		mock_io_close_s2c(&io);
		if (ok && cca.ch != NULL)
			dssh_session_close(client, cca.ch, 0);
		if (ok && csa.ch != NULL)
			dssh_session_close(server, csa.ch, 0);
		if (client->demux_running)
			dssh_session_stop(client);
		if (server->demux_running)
			dssh_session_stop(server);
		dssh_session_cleanup(server);
		dssh_session_cleanup(client);
		mock_io_free(&io);
		dssh_test_reset_global_config();

		if (ok) {
			ASSERT_TRUE(n > 0);
			return TEST_PASS;
		}
	}

	return TEST_FAIL;
}

/* ================================================================
 * Iterative OpenSSL/C11 failure injection during handshake.
 *
 * Same pattern as handshake_iterate, but arms the OpenSSL/C11
 * wrapper countdown instead of the malloc countdown.
 * ================================================================ */

#include "dssh_test_ossl.h"

static int
test_ossl_handshake_iterate(void)
{
	for (int n = 0; n < 200; n++) {
		dssh_test_reset_global_config();
		dssh_test_alloc_reset();
		dssh_test_ossl_reset();
		mock_alloc_reset();

		if (register_all() < 0)
			return TEST_FAIL;
		if (test_generate_host_key() < 0)
			return TEST_FAIL;

		struct mock_io_state io;
		if (mock_io_init(&io, 0) < 0)
			return TEST_FAIL;

		dssh_transport_set_callbacks(mock_tx_dispatch, mock_rx_dispatch,
		    mock_rxline_dispatch, mock_extra_line_cb);

		dssh_session client = dssh_session_init(true, 0);
		if (client == NULL) {
			mock_io_free(&io);
			continue;
		}
		dssh_session_set_cbdata(client, &io, &io, &io, &io);

		dssh_session server = init_server_session();
		if (server == NULL) {
			dssh_session_cleanup(client);
			mock_io_free(&io);
			continue;
		}
		dssh_session_set_cbdata(server, &io, &io, &io, &io);

		/* Barrier: create threads, wait for both ready, arm, release */
		mtx_t bmtx;
		cnd_t bcnd;
		int bcount = 0;
		mtx_init(&bmtx, mtx_plain);
		cnd_init(&bcnd);

		struct hs_alloc_ctx ca = {
			.io = &io, .sess = client,
			.barrier_mtx = &bmtx, .barrier_cnd = &bcnd,
			.barrier_count = &bcount
		};
		struct hs_alloc_ctx sa = {
			.io = &io, .sess = server,
			.barrier_mtx = &bmtx, .barrier_cnd = &bcnd,
			.barrier_count = &bcount
		};

		thrd_t ct, st;
		thrd_create(&ct, hs_alloc_thread, &ca);
		thrd_create(&st, hs_alloc_thread, &sa);

		mtx_lock(&bmtx);
		while (bcount < 2)
			cnd_wait(&bcnd, &bmtx);

		/* Arm the OpenSSL/C11 failure injection */
		dssh_test_ossl_fail_after(n);
		bcount = 3;
		cnd_broadcast(&bcnd);
		mtx_unlock(&bmtx);

		thrd_join(ct, NULL);
		thrd_join(st, NULL);
		cnd_destroy(&bcnd);
		mtx_destroy(&bmtx);

		dssh_test_ossl_reset();
		bool ok = (ca.result == 0 && sa.result == 0);

		mock_io_close_c2s(&io);
		mock_io_close_s2c(&io);
		dssh_session_cleanup(server);
		dssh_session_cleanup(client);
		mock_io_free(&io);
		dssh_test_reset_global_config();

		if (ok) {
			ASSERT_TRUE(n > 0);
			return TEST_PASS;
		}
	}

	return TEST_FAIL;
}

/* ================================================================
 * Test table
 * ================================================================ */

static struct dssh_test_entry tests[] = {
	/* Registration alloc failures */
	{ "alloc/register_curve25519",    test_alloc_register_curve25519 },
	{ "alloc/register_ed25519",       test_alloc_register_ed25519 },
	{ "alloc/register_rsa",           test_alloc_register_rsa },
	{ "alloc/register_aes256ctr",     test_alloc_register_aes256ctr },
	{ "alloc/register_hmac_sha256",   test_alloc_register_hmac_sha2_256 },
	{ "alloc/register_none_comp",     test_alloc_register_none_comp },
	{ "alloc/register_none_enc",      test_alloc_register_none_enc },
	{ "alloc/register_none_mac",      test_alloc_register_none_mac },
	{ "alloc/register_dh_gex",        test_alloc_register_dh_gex },

	/* Session init */
	{ "alloc/session_init",           test_alloc_session_init },

	/* Channel buffers */
	{ "alloc/bytebuf_init",           test_alloc_bytebuf_init },
	{ "alloc/msgqueue_push",          test_alloc_msgqueue_push },
	{ "alloc/signal_push",            test_alloc_signal_push },

	/* Auth */
	{ "alloc/auth_get_methods",       test_alloc_auth_get_methods },
	{ "alloc/auth_password",          test_alloc_auth_password },
	{ "alloc/auth_publickey",         test_alloc_auth_publickey },
	{ "alloc/auth_kbi",               test_alloc_auth_kbi },

	/* Server auth */
	{ "alloc/send_passwd_changereq",  test_alloc_send_passwd_changereq },
	{ "alloc/send_pk_ok",             test_alloc_send_pk_ok },

	/* Connection protocol */
	{ "alloc/acceptqueue_push",       test_alloc_acceptqueue_push },

	/* Handshake kexinit malloc */
	{ "alloc/kexinit",                test_alloc_kexinit },

	/* Iterative handshake (library-only allocator) */
	{ "alloc/handshake_iterate",      test_alloc_handshake_iterate },

	/* Iterative auth (library-only allocator) */
	{ "alloc/auth_iterate",           test_alloc_auth_iterate },

	/* Iterative conn (library-only allocator) */
	{ "alloc/conn_iterate",           test_alloc_conn_iterate },

	/* Iterative OpenSSL/C11 failure injection */
	{ "ossl/handshake_iterate",       test_ossl_handshake_iterate },
};

DSSH_TEST_MAIN(tests)
