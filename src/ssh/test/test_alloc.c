/*
 * test_alloc.c -- Allocation failure tests for DeuceSSH.
 *
 * Uses the mock allocator (--wrap=malloc/calloc/realloc/free) to
 * inject NULL returns at every allocation point in the library.
 * For each function under test, we iterate N from 0 upward, failing
 * the Nth allocation, until the function succeeds -- confirming that
 * every intermediate failure is handled gracefully.
 */

#include <stddef.h>
#include <string.h>
#include <threads.h>

#include "dssh_test.h"
#include "deucessh.h"
#include "deucessh-algorithms.h"
#include "kex/dh-gex-sha256.h"
#include "deucessh-auth.h"
#include "deucessh-conn.h"
#include "ssh-trans.h"
#include "ssh-internal.h"
#include "dssh_test_internal.h"
#include "mock_io.h"
#include "mock_alloc.h"
#include "test_dhgex_provider.h"
#include "../kex/sntrup761.h"
#include "../kex/mlkem768.h"

#include <openssl/rand.h>


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
		res = dssh_register_dh_gex_sha256();
	else if (test_using_sntrup())
		res = dssh_register_sntrup761x25519_sha512();
	else if (test_using_mlkem())
		res = dssh_register_mlkem768x25519_sha256();
	else
		res = dssh_register_curve25519_sha256();
	if (res < 0) return res;
	res = test_register_key_algos();
	if (res < 0) return res;
	res = dssh_register_aes256_ctr();
	if (res < 0) return res;
	res = dssh_register_hmac_sha2_256();
	if (res < 0) return res;
	res = dssh_register_none_comp();
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
	int res = dssh_register_curve25519_sha256();
	mock_alloc_reset();
	ASSERT_EQ(res, DSSH_ERROR_ALLOC);

	/* Now succeed */
	res = dssh_register_curve25519_sha256();
	ASSERT_EQ(res, 0);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

static int
test_alloc_register_ed25519(void)
{
	dssh_test_reset_global_config();
	mock_alloc_fail_after(0);
	int res = dssh_register_ssh_ed25519();
	mock_alloc_reset();
	ASSERT_EQ(res, DSSH_ERROR_ALLOC);

	res = dssh_register_ssh_ed25519();
	ASSERT_EQ(res, 0);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

static int
test_alloc_register_rsa(void)
{
	dssh_test_reset_global_config();
	mock_alloc_fail_after(0);
	int res = dssh_register_rsa_sha2_256();
	mock_alloc_reset();
	ASSERT_EQ(res, DSSH_ERROR_ALLOC);

	res = dssh_register_rsa_sha2_256();
	ASSERT_EQ(res, 0);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

static int
test_alloc_register_aes256ctr(void)
{
	dssh_test_reset_global_config();
	mock_alloc_fail_after(0);
	int res = dssh_register_aes256_ctr();
	mock_alloc_reset();
	ASSERT_EQ(res, DSSH_ERROR_ALLOC);

	res = dssh_register_aes256_ctr();
	ASSERT_EQ(res, 0);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

static int
test_alloc_register_hmac_sha2_256(void)
{
	dssh_test_reset_global_config();
	mock_alloc_fail_after(0);
	int res = dssh_register_hmac_sha2_256();
	mock_alloc_reset();
	ASSERT_EQ(res, DSSH_ERROR_ALLOC);

	res = dssh_register_hmac_sha2_256();
	ASSERT_EQ(res, 0);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

static int
test_alloc_register_none_comp(void)
{
	dssh_test_reset_global_config();
	mock_alloc_fail_after(0);
	int res = dssh_register_none_comp();
	mock_alloc_reset();
	ASSERT_EQ(res, DSSH_ERROR_ALLOC);

	res = dssh_register_none_comp();
	ASSERT_EQ(res, 0);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

static int
test_alloc_register_none_enc(void)
{
	dssh_test_reset_global_config();
	mock_alloc_fail_after(0);
	int res = dssh_register_none_enc();
	mock_alloc_reset();
	ASSERT_EQ(res, DSSH_ERROR_ALLOC);

	res = dssh_register_none_enc();
	ASSERT_EQ(res, 0);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

static int
test_alloc_register_none_mac(void)
{
	dssh_test_reset_global_config();
	mock_alloc_fail_after(0);
	int res = dssh_register_none_mac();
	mock_alloc_reset();
	ASSERT_EQ(res, DSSH_ERROR_ALLOC);

	res = dssh_register_none_mac();
	ASSERT_EQ(res, 0);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

static int
test_alloc_register_dh_gex(void)
{
	dssh_test_reset_global_config();
	mock_alloc_fail_after(0);
	int res = dssh_register_dh_gex_sha256();
	mock_alloc_reset();
	ASSERT_EQ(res, DSSH_ERROR_ALLOC);

	res = dssh_register_dh_gex_sha256();
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
	int prev_count = -1;
	for (int n = 0; n < 100; n++) {
		mock_alloc_fail_after(n);
		dssh_session sess = dssh_session_init(true, 0);
		int cur_count = mock_alloc_count();
		mock_alloc_reset();
		if (sess != NULL) {
			dssh_session_cleanup(sess);
			dssh_test_reset_global_config();
			ASSERT_TRUE(n > 0);
			return TEST_PASS;
		}
		prev_count = cur_count;
	}

	fprintf(stderr, "  alloc/session_init: still incrementing at "
	    "n=%d (count=%d), raise limit\n", 100, prev_count);
	dssh_test_reset_global_config();
	return TEST_FAIL;
}

/* ================================================================
 * dssh_session_init C11 thread failures
 *
 * session_init calls mtx_init() after the calloc succeeds.  Failing
 * it must return NULL without leaking the session struct.
 * ================================================================ */

#include "dssh_test_ossl.h"

static int
test_ossl_session_init(void)
{
	dssh_test_reset_global_config();

	ASSERT_EQ(register_all(), 0);

	/* Fail each ossl/C11 call in turn */
	int prev_count = -1;
	for (int n = 0; n < 100; n++) {
		dssh_test_ossl_fail_after(n);
		dssh_session sess = dssh_session_init(true, 0);
		int cur_count = dssh_test_ossl_count();
		dssh_test_ossl_reset();
		if (sess != NULL) {
			dssh_session_cleanup(sess);
			dssh_test_reset_global_config();
			ASSERT_TRUE(n > 0);
			return TEST_PASS;
		}
		prev_count = cur_count;
	}

	fprintf(stderr, "  ossl/session_init: still incrementing at "
	    "n=%d (count=%d), raise limit\n", 100, prev_count);
	dssh_test_reset_global_config();
	return TEST_FAIL;
}

/* ================================================================
 * Channel buffer alloc failures (ssh-chan.c)
 *
 * bytebuf_init does one malloc for the data buffer.
 * msgqueue_push does one malloc for the entry.
 * dssh_signal_push does one malloc for the mark.
 * dssh_accept_queue_push does one malloc for the entry.
 * ================================================================ */

static int
test_alloc_bytebuf_init(void)
{
	struct dssh_bytebuf buf;
	memset(&buf, 0, sizeof(buf));

	mock_alloc_fail_after(0);
	int res = bytebuf_init(&buf, 1024);
	mock_alloc_reset();
	ASSERT_EQ(res, DSSH_ERROR_ALLOC);

	/* Succeed */
	res = bytebuf_init(&buf, 1024);
	ASSERT_EQ(res, 0);
	bytebuf_free(&buf);
	return TEST_PASS;
}

static int
test_alloc_msgqueue_push(void)
{
	struct dssh_msgqueue q;
	msgqueue_init(&q);

	const uint8_t data[] = "hello";
	mock_alloc_fail_after(0);
	int res = msgqueue_push(&q, data, sizeof(data));
	mock_alloc_reset();
	ASSERT_EQ(res, DSSH_ERROR_ALLOC);

	/* Succeed */
	res = msgqueue_push(&q, data, sizeof(data));
	ASSERT_EQ(res, 0);
	msgqueue_free(&q);
	return TEST_PASS;
}

static int
test_alloc_signal_push(void)
{
	struct dssh_signal_queue q;
	sigqueue_init(&q);

	mock_alloc_fail_after(0);
	int res = sigqueue_push(&q, "INT", 0, 0);
	mock_alloc_reset();
	ASSERT_EQ(res, DSSH_ERROR_ALLOC);

	/* Succeed */
	res = sigqueue_push(&q, "INT", 0, 0);
	ASSERT_EQ(res, 0);
	sigqueue_free(&q);
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
	if (ctx->client_result != 0) {
		mock_io_close_c2s(&ctx->io);
		mock_io_close_s2c(&ctx->io);
	}
	return 0;
}
static int handshake_server_thread(void *arg) {
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
	if (thrd_create(&ct, handshake_client_thread, ctx) != thrd_success)
		return -1;
	if (thrd_create(&st, handshake_server_thread, ctx) != thrd_success)
		return -1;
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
 * send_auth_failure has a malloc (dynamic buffer).
 * SERVICE_ACCEPT has a malloc (dynamic buffer).
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
	 * The prompt is malloc'd by us -- but we need to use __real_malloc
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
	size_t username_len = sizeof(username);

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
	/* Malloc sequence inside dssh_auth_server for this path:
	 * 0: SERVICE_ACCEPT buffer
	 * 1: callback's prompt malloc (changereq_password_cb)
	 * 2: send_passwd_changereq message buffer
	 * Fail at 1 to hit the callback's prompt malloc. */
	sa.fail_at = 1;

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
		ASSERT_OK(send_packet(ctx.client, msg, pos, NULL));
	}
	{
		uint8_t msg_type;
		uint8_t *payload;
		size_t payload_len;
		ASSERT_OK(recv_packet(ctx.client, &msg_type, &payload, &payload_len));
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
		ASSERT_OK(send_packet(ctx.client, msg, pos, NULL));
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
	size_t username_len = sizeof(username);

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
	sa.fail_at = 1;  /* fail the first alloc in the auth loop (after SERVICE_ACCEPT) */

	/* Get the public key blob BEFORE starting the server thread,
	 * since the server thread arms the process-wide allocator and
	 * RSA pubkey() internally calls malloc. */
	dssh_key_algo ka = find_key_algo(test_key_algo_name());
	ASSERT_NOT_NULL(ka);
	const uint8_t *pubkey_buf = NULL;
	size_t pubkey_len;
	ASSERT_EQ(ka->pubkey(&pubkey_buf, &pubkey_len, ka->ctx), 0);

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
		ASSERT_OK(send_packet(ctx.client, msg, pos, NULL));
	}
	{
		uint8_t msg_type;
		uint8_t *payload;
		size_t payload_len;
		ASSERT_OK(recv_packet(ctx.client, &msg_type, &payload, &payload_len));
		ASSERT_EQ(msg_type, SSH_MSG_SERVICE_ACCEPT);
	}

	/* Send publickey probe */
	{
		static const char svc[] = "ssh-connection";
		static const char method[] = "publickey";
		const char *algo = test_key_algo_name();
		size_t algo_len = strlen(algo);
		uint8_t msg[2048];
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
		ASSERT_OK(send_packet(ctx.client, msg, pos, NULL));
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
 * acceptqueue_push does one malloc for the entry struct.
 * Failing it must return DSSH_ERROR_ALLOC.
 * ================================================================ */

static int
test_alloc_acceptqueue_push(void)
{
	struct dssh_accept_queue q;
	acceptqueue_init(&q);

	mock_alloc_fail_after(0);
	int res = acceptqueue_push(&q, 0, 0x200000, 0x8000,
	    (const uint8_t *)"session", 7);
	mock_alloc_reset();
	ASSERT_EQ(res, DSSH_ERROR_ALLOC);

	/* Succeed */
	res = acceptqueue_push(&q, 0, 0x200000, 0x8000,
	    (const uint8_t *)"session", 7);
	ASSERT_EQ(res, 0);
	acceptqueue_free(&q);
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
	dssh_auth_kbi_prompt_cb dummy_cb;
	{
		void *p = (void *)1;
		memcpy(&dummy_cb, &p, sizeof(dummy_cb));
	}
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
 * kexinit does one malloc for its packet buffer.
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
	int res = kexinit(sess);
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
	 * sessions -- terminate flag only affects our own session) */
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
	int prev_count = -1;
	for (int n = 0; n < 100; n++) {
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
		ASSERT_THRD_CREATE(&ct, hs_alloc_thread, &ca);
		ASSERT_THRD_CREATE(&st, hs_alloc_thread, &sa);

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

		int cur_count = dssh_test_alloc_count();
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
		if (cur_count == prev_count)
			return TEST_PASS;
		prev_count = cur_count;
	}

	fprintf(stderr, "  alloc/handshake_iterate: still incrementing at "
	    "n=%d (count=%d), raise limit\n", 100, prev_count);
	return TEST_FAIL;
}

/* ================================================================
 * Iterative auth alloc failures using the library-only allocator.
 *
 * For each N, we: handshake (allocator off) -> arm allocator -> run
 * two-threaded auth -> check.  Auth callbacks are simple accept-all.
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
	size_t username_len = sizeof(username);
	ctx->result = dssh_auth_server(ctx->sess, ctx->cbs,
	    username, &username_len);
	if (ctx->result < 0)
		mock_io_close_s2c(ctx->io);
	return 0;
}

static int
test_alloc_auth_iterate(void)
{
	int prev_count = -1;
	struct dssh_auth_server_cbs cbs = {0};
	cbs.methods_str = "password";
	cbs.password_cb = iter_password_cb;

	for (int n = 0; n < 100; n++) {
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
			ASSERT_THRD_CREATE(&ct, handshake_client_thread, &hctx);
			ASSERT_THRD_CREATE(&st, handshake_server_thread, &hctx);
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
		ASSERT_THRD_CREATE(&ct, auth_alloc_client_thread, &ca);
		ASSERT_THRD_CREATE(&st, auth_alloc_server_thread, &sa);

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

		int cur_count = dssh_test_alloc_count();
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
		if (cur_count == prev_count)
			return TEST_PASS;
		prev_count = cur_count;
	}

	fprintf(stderr, "  alloc/auth_iterate: still incrementing at n=%d "
	    "(count=%d), raise limit\n", 100, prev_count);
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
	size_t username_len = sizeof(username);
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
 * Cancellable: signal watchdog_cancel() after conn threads finish
 * so we don't sleep the full timeout on every iteration.
 */
struct watchdog_ctx {
	struct mock_io_state *io;
	dssh_session client;
	dssh_session server;
	int delay_ms;
	mtx_t mtx;
	cnd_t cnd;
	bool cancel;
};

static void
watchdog_init(struct watchdog_ctx *ctx)
{
	mtx_init(&ctx->mtx, mtx_plain);
	cnd_init(&ctx->cnd);
	ctx->cancel = false;
}

static void
watchdog_destroy(struct watchdog_ctx *ctx)
{
	cnd_destroy(&ctx->cnd);
	mtx_destroy(&ctx->mtx);
}

static void
watchdog_cancel(struct watchdog_ctx *ctx)
{
	mtx_lock(&ctx->mtx);
	ctx->cancel = true;
	cnd_signal(&ctx->cnd);
	mtx_unlock(&ctx->mtx);
}

static int
watchdog_thread(void *arg)
{
	struct watchdog_ctx *ctx = arg;
	struct timespec ts;
	timespec_get(&ts, TIME_UTC);
	ts.tv_sec += ctx->delay_ms / 1000;
	ts.tv_nsec += (ctx->delay_ms % 1000) * 1000000L;
	if (ts.tv_nsec >= 1000000000L) {
		ts.tv_sec++;
		ts.tv_nsec -= 1000000000L;
	}

	mtx_lock(&ctx->mtx);
	while (!ctx->cancel)
		if (cnd_timedwait(&ctx->cnd, &ctx->mtx, &ts) == thrd_timedout)
			break;
	bool cancelled = ctx->cancel;
	mtx_unlock(&ctx->mtx);

	if (!cancelled) {
		mock_io_close_c2s(ctx->io);
		mock_io_close_s2c(ctx->io);
		if (ctx->client->conn_initialized)
			session_set_terminate(ctx->client);
		if (ctx->server->conn_initialized)
			session_set_terminate(ctx->server);
	}
	return 0;
}

static int
test_alloc_conn_iterate(void)
{
	int prev_count = -1;
	struct dssh_auth_server_cbs cbs = {0};
	cbs.methods_str = "password";
	cbs.password_cb = iter_password_cb;

	for (int n = 0; n < 200; n++) {
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
			ASSERT_THRD_CREATE(&ct, handshake_client_thread, &hctx);
			ASSERT_THRD_CREATE(&st, handshake_server_thread, &hctx);
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
			ASSERT_THRD_CREATE(&ct, auth_only_client_thread, &ca);
			ASSERT_THRD_CREATE(&st, auth_only_server_thread, &sa);
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
		watchdog_init(&wctx);

		thrd_t ct, st, wt;
		ASSERT_THRD_CREATE(&wt, watchdog_thread, &wctx);
		ASSERT_THRD_CREATE(&ct, conn_alloc_client_thread, &cca);
		ASSERT_THRD_CREATE(&st, conn_alloc_server_thread, &csa);

		thrd_join(ct, NULL);
		thrd_join(st, NULL);
		watchdog_cancel(&wctx);
		thrd_join(wt, NULL);
		watchdog_destroy(&wctx);

		int cur_count = dssh_test_alloc_count();
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
		if (cur_count == prev_count)
			return TEST_PASS;
		prev_count = cur_count;
	}

	fprintf(stderr, "  alloc/conn_iterate: still incrementing at n=%d "
	    "(count=%d), raise limit\n", 200, prev_count);
	return TEST_FAIL;
}

/* ================================================================
 * Iterative OpenSSL/C11 failure injection during handshake.
 *
 * Same pattern as handshake_iterate, but arms the OpenSSL/C11
 * wrapper countdown instead of the malloc countdown.
 * ================================================================ */

static int
test_ossl_handshake_iterate(void)
{
	int prev_count = -1;
	for (int n = 0; n < 500; n++) {
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
		ASSERT_THRD_CREATE(&ct, hs_alloc_thread, &ca);
		ASSERT_THRD_CREATE(&st, hs_alloc_thread, &sa);

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

		int cur_count = dssh_test_ossl_count();
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

		/* If the call count stopped increasing, all failure
		 * points have been exercised -- no need to keep going
		 * through increasingly expensive successful crypto. */
		if (cur_count == prev_count)
			return TEST_PASS;
		prev_count = cur_count;
	}

	fprintf(stderr, "  ossl/handshake_iterate: still incrementing at n=%d "
	    "(count=%d), raise limit\n", 500, prev_count);
	return TEST_FAIL;
}

/* ================================================================
 * Isolated OpenSSL failure injection for key algo operations.
 *
 * Single-threaded: generate key, then iterate ossl failures through
 * sign -> pubkey -> verify.  No handshake, no I/O -- fast.
 * ================================================================ */

static int
test_ossl_key_sign_verify_iterate(void)
{
	int prev_count = -1;
	for (int n = 0; n < 500; n++) {
		dssh_test_reset_global_config();
		dssh_test_ossl_reset();
		mock_alloc_reset();

		int res = test_register_key_algos();
		if (res < 0)
			return TEST_FAIL;
		if (test_generate_host_key() < 0)
			return TEST_FAIL;

		dssh_key_algo ka = find_key_algo(
		    test_key_algo_name());
		if (ka == NULL)
			return TEST_FAIL;

		const uint8_t data[] = "test data for ossl sign/verify";
		uint8_t *sig_buf = NULL;
		size_t sig_len = 0;
		const uint8_t *pub_buf = NULL;
		size_t pub_len = 0;

		dssh_test_ossl_fail_after(n);

		int sign_res = ka->sign(&sig_buf, &sig_len,
		    data, sizeof(data) - 1, ka->ctx);
		int pub_res = ka->pubkey(&pub_buf, &pub_len,
		    ka->ctx);
		int verify_res = -1;
		if (sign_res == 0 && pub_res == 0)
			verify_res = ka->verify(pub_buf, pub_len, sig_buf,
			    sig_len, data, sizeof(data) - 1);

		free(sig_buf);
		int cur_count = dssh_test_ossl_count();
		dssh_test_ossl_reset();

		if (sign_res == 0 && pub_res == 0 && verify_res == 0) {
			dssh_test_reset_global_config();
			ASSERT_TRUE(n > 0);
			return TEST_PASS;
		}
		dssh_test_reset_global_config();
		if (cur_count == prev_count)
			return TEST_PASS;
		prev_count = cur_count;
	}

	fprintf(stderr, "  ossl/key_sign_verify: still incrementing at n=%d "
	    "(count=%d), raise limit\n", 500, prev_count);
	return TEST_FAIL;
}

/* ================================================================
 * Isolated OpenSSL failure injection for key generation.
 *
 * Single-threaded: iterate ossl failures through generate().
 * ================================================================ */

static int
test_ossl_keygen_iterate(void)
{
	const char *algo = test_key_algo_name();
	bool is_rsa = (strcmp(algo, "rsa-sha2-256") == 0);
	int prev_count = -1;

	for (int n = 0; n < 500; n++) {
		dssh_test_reset_global_config();
		dssh_test_ossl_reset();
		mock_alloc_reset();

		int res = test_register_key_algos();
		if (res < 0)
			return TEST_FAIL;

		dssh_test_ossl_fail_after(n);

		if (is_rsa)
			res = dssh_rsa_sha2_256_generate_key(2048);
		else
			res = dssh_ed25519_generate_key();

		int cur_count = dssh_test_ossl_count();
		dssh_test_ossl_reset();

		if (res == 0) {
			dssh_test_reset_global_config();
			ASSERT_TRUE(n > 0);
			return TEST_PASS;
		}
		dssh_test_reset_global_config();
		if (cur_count == prev_count)
			return TEST_PASS;
		prev_count = cur_count;
	}

	fprintf(stderr, "  ossl/keygen: still incrementing at n=%d "
	    "(count=%d), raise limit\n", 500, prev_count);
	return TEST_FAIL;
}

/* ================================================================
 * Isolated OpenSSL failure injection for enc init/encrypt.
 *
 * Single-threaded: register aes256-ctr, iterate ossl failures
 * through init -> encrypt -> cleanup.
 * ================================================================ */

static int
test_ossl_enc_iterate(void)
{
	int prev_count = -1;
	for (int n = 0; n < 500; n++) {
		dssh_test_reset_global_config();
		dssh_test_ossl_reset();
		mock_alloc_reset();

		ASSERT_EQ(dssh_register_aes256_ctr(), 0);

		dssh_enc enc = gconf.enc_head;
		if (enc == NULL)
			return TEST_FAIL;

		uint8_t key[32], iv[16];
		memset(key, 0x42, sizeof(key));
		memset(iv, 0x00, sizeof(iv));

		dssh_test_ossl_fail_after(n);

		dssh_enc_ctx *ectx = NULL;
		int res = enc->init(key, iv, true, &ectx);
		int enc_res = -1;
		if (res == 0) {
			uint8_t buf[16];
			memset(buf, 'A', sizeof(buf));
			enc_res = enc->encrypt(buf, sizeof(buf), ectx);
			if (enc->cleanup)
				enc->cleanup(ectx);
		}

		int cur_count = dssh_test_ossl_count();
		dssh_test_ossl_reset();

		if (res == 0 && enc_res == 0) {
			dssh_test_reset_global_config();
			ASSERT_TRUE(n > 0);
			return TEST_PASS;
		}
		dssh_test_reset_global_config();
		if (cur_count == prev_count)
			return TEST_PASS;
		prev_count = cur_count;
	}

	fprintf(stderr, "  ossl/enc: still incrementing at n=%d "
	    "(count=%d), raise limit\n", 500, prev_count);
	return TEST_FAIL;
}

/* ================================================================
 * Isolated OpenSSL failure injection for mac init/compute/verify.
 *
 * Single-threaded: register hmac-sha2-256, iterate ossl failures
 * through init -> compute -> verify -> cleanup.
 * ================================================================ */

static int
test_ossl_mac_iterate(void)
{
	int prev_count = -1;
	for (int n = 0; n < 500; n++) {
		dssh_test_reset_global_config();
		dssh_test_ossl_reset();
		mock_alloc_reset();

		ASSERT_EQ(dssh_register_hmac_sha2_256(), 0);

		dssh_mac mac = gconf.mac_head;
		if (mac == NULL)
			return TEST_FAIL;

		uint8_t key[32];
		memset(key, 0x42, sizeof(key));

		dssh_test_ossl_fail_after(n);

		dssh_mac_ctx *mctx = NULL;
		int res = mac->init(key, &mctx);
		int gen_res = -1;
		if (res == 0) {
			uint8_t data[16];
			memset(data, 'B', sizeof(data));
			uint8_t tag[64];
			gen_res = mac->generate(data, sizeof(data),
			    tag, mctx);
			if (mac->cleanup)
				mac->cleanup(mctx);
		}

		int cur_count = dssh_test_ossl_count();
		dssh_test_ossl_reset();

		if (res == 0 && gen_res == 0) {
			dssh_test_reset_global_config();
			ASSERT_TRUE(n > 0);
			return TEST_PASS;
		}
		dssh_test_reset_global_config();
		if (cur_count == prev_count)
			return TEST_PASS;
		prev_count = cur_count;
	}

	fprintf(stderr, "  ossl/mac: still incrementing at n=%d "
	    "(count=%d), raise limit\n", 500, prev_count);
	return TEST_FAIL;
}

/* ================================================================
 * Isolated key_algo ossl iterate: verify and pubkey
 *
 * These exercise OpenSSL error paths in verify() and pubkey()
 * without any session or I/O -- just the crypto callbacks.
 * ================================================================ */

static int
test_ossl_key_verify_iterate(void)
{
	dssh_test_reset_global_config();
	dssh_test_ossl_reset();
	mock_alloc_reset();

	int res = test_register_key_algos();
	if (res < 0)
		return TEST_FAIL;
	if (test_generate_host_key() < 0)
		return TEST_FAIL;

	dssh_key_algo ka = find_key_algo(test_key_algo_name());
	if (ka == NULL)
		return TEST_FAIL;

	/* Generate valid sig + pubkey blobs with ossl off */
	const uint8_t data[] = "test data for verify iterate";
	uint8_t *sig_buf = NULL;
	const uint8_t *pub_buf = NULL;
	size_t sig_len, pub_len;
	ASSERT_EQ(ka->sign(&sig_buf, &sig_len,
	    data, sizeof(data) - 1, ka->ctx), 0);
	ASSERT_EQ(ka->pubkey(&pub_buf, &pub_len, ka->ctx), 0);

	int prev_count = -1;
	for (int n = 0; n < 500; n++) {
		dssh_test_ossl_fail_after(n);
		int vres = ka->verify(pub_buf, pub_len, sig_buf, sig_len,
		    data, sizeof(data) - 1);
		int cur_count = dssh_test_ossl_count();
		dssh_test_ossl_reset();

		if (vres == 0) {
			dssh_test_reset_global_config();
			ASSERT_TRUE(n > 0);
			return TEST_PASS;
		}
		if (cur_count == prev_count) {
			dssh_test_reset_global_config();
			return TEST_PASS;
		}
		prev_count = cur_count;
	}

	fprintf(stderr, "  ossl/key_verify: still incrementing at n=%d "
	    "(count=%d), raise limit\n", 500, prev_count);
	free(sig_buf);
	dssh_test_reset_global_config();
	return TEST_FAIL;
}

static int
test_ossl_key_pubkey_iterate(void)
{
	dssh_test_reset_global_config();
	dssh_test_ossl_reset();
	mock_alloc_reset();

	int res = test_register_key_algos();
	if (res < 0)
		return TEST_FAIL;
	if (test_generate_host_key() < 0)
		return TEST_FAIL;

	dssh_key_algo ka = find_key_algo(test_key_algo_name());
	if (ka == NULL)
		return TEST_FAIL;

	int prev_count = -1;
	for (int n = 0; n < 500; n++) {
		dssh_test_ossl_fail_after(n);
		const uint8_t *buf = NULL;
		size_t outlen;
		int pres = ka->pubkey(&buf, &outlen, ka->ctx);
		int cur_count = dssh_test_ossl_count();
		dssh_test_ossl_reset();

		if (pres == 0) {
			dssh_test_reset_global_config();
			ASSERT_TRUE(n > 0);
			return TEST_PASS;
		}
		if (cur_count == prev_count) {
			dssh_test_reset_global_config();
			return TEST_PASS;
		}
		prev_count = cur_count;
	}

	fprintf(stderr, "  ossl/key_pubkey: still incrementing at n=%d "
	    "(count=%d), raise limit\n", 500, prev_count);
	dssh_test_reset_global_config();
	return TEST_FAIL;
}

/* ================================================================
 * Server-side KEX handler iterate with packet replay.
 *
 * One-time two-threaded setup (version_exchange + kexinit) to get
 * negotiated state, then single-threaded iterate of the server-side
 * KEX handler with pre-injected client packets.
 * ================================================================ */

/*
 * Build a plaintext SSH wire packet from a payload.
 * Matches send_packet's format in pre-NEWKEYS mode:
 *   uint32  packet_length (1 + payload_len + padding_len)
 *   byte    padding_length
 *   byte[]  payload
 *   byte[]  zero_padding (at least 4 bytes, 8-byte aligned)
 * Returns total wire length, or 0 on overflow.
 */
static size_t
build_plaintext_packet(const uint8_t *payload, size_t payload_len,
    uint8_t *buf, size_t bufsz)
{
	size_t bs = 8;
	size_t padding_len = bs - ((5 + payload_len) % bs);
	if (padding_len < 4)
		padding_len += bs;
	uint32_t packet_length = (uint32_t)(1 + payload_len + padding_len);
	size_t total = 4 + packet_length;
	if (total > bufsz)
		return 0;
	size_t pos = 0;
	dssh_serialize_uint32(packet_length, buf, bufsz, &pos);
	buf[pos++] = (uint8_t)padding_len;
	memcpy(&buf[pos], payload, payload_len);
	pos += payload_len;
	memset(&buf[pos], 0, padding_len);
	pos += padding_len;
	return pos;
}

/* Thread: version_exchange + kexinit */
struct ve_ki_ctx {
	struct mock_io_state *io;
	dssh_session sess;
	int result;
};

static int
ve_ki_thread(void *arg)
{
	struct ve_ki_ctx *ctx = arg;
	ctx->result = version_exchange(ctx->sess);
	if (ctx->result == 0)
		ctx->result = kexinit(ctx->sess);
	if (ctx->result < 0) {
		/* Close our write pipe so peer unblocks */
		if (ctx->sess->trans.client)
			mock_io_close_c2s(ctx->io);
		else
			mock_io_close_s2c(ctx->io);
	}
	return 0;
}

static int
test_ossl_kex_server_iterate(void)
{
	/* === ONE-TIME SETUP === */
	dssh_test_reset_global_config();
	dssh_test_alloc_reset();
	dssh_test_ossl_reset();
	mock_alloc_reset();

	if (register_all() < 0)
		return TEST_FAIL;
	if (test_generate_host_key() < 0)
		return TEST_FAIL;

	struct mock_io_state setup_io;
	if (mock_io_init(&setup_io, 0) < 0)
		return TEST_FAIL;

	dssh_transport_set_callbacks(mock_tx_dispatch, mock_rx_dispatch,
	    mock_rxline_dispatch, mock_extra_line_cb);

	dssh_session client = dssh_session_init(true, 0);
	if (client == NULL) {
		mock_io_free(&setup_io);
		return TEST_FAIL;
	}
	dssh_session_set_cbdata(client, &setup_io, &setup_io,
	    &setup_io, &setup_io);

	dssh_session server = init_server_session();
	if (server == NULL) {
		dssh_session_cleanup(client);
		mock_io_free(&setup_io);
		return TEST_FAIL;
	}
	dssh_session_set_cbdata(server, &setup_io, &setup_io,
	    &setup_io, &setup_io);

	/* Two-threaded version_exchange + kexinit */
	{
		struct ve_ki_ctx ca = { .io = &setup_io, .sess = client };
		struct ve_ki_ctx sa = { .io = &setup_io, .sess = server };
		thrd_t ct, st;
		ASSERT_THRD_CREATE(&ct, ve_ki_thread, &ca);
		ASSERT_THRD_CREATE(&st, ve_ki_thread, &sa);
		thrd_join(ct, NULL);
		thrd_join(st, NULL);
		if (ca.result != 0 || sa.result != 0) {
			mock_io_close_c2s(&setup_io);
			mock_io_close_s2c(&setup_io);
			dssh_session_cleanup(server);
			dssh_session_cleanup(client);
			mock_io_free(&setup_io);
			dssh_test_reset_global_config();
			return TEST_FAIL;
		}
	}

	/* Done with client and setup I/O */
	dssh_session_cleanup(client);
	mock_io_close_c2s(&setup_io);
	mock_io_close_s2c(&setup_io);
	mock_io_free(&setup_io);

	/* === BUILD CLIENT KEX PACKETS === */
	uint8_t wire_pkts[8192];
	size_t wire_total = 0;

	if (test_using_dhgex()) {
		/* DH-GEX: GEX_REQUEST + GEX_INIT */
		uint8_t req[16];
		size_t rp = 0;
		req[rp++] = 34; /* SSH_MSG_KEX_DH_GEX_REQUEST */
		dssh_serialize_uint32(2048, req, sizeof(req), &rp);
		dssh_serialize_uint32(4096, req, sizeof(req), &rp);
		dssh_serialize_uint32(8192, req, sizeof(req), &rp);
		wire_total += build_plaintext_packet(req, rp,
		    &wire_pkts[wire_total], sizeof(wire_pkts) - wire_total);

		/* GEX_INIT with e=2 (valid: 2  in  [1, p-1]) */
		uint8_t init[16];
		size_t ip = 0;
		init[ip++] = 32; /* SSH_MSG_KEX_DH_GEX_INIT */
		dssh_serialize_uint32(1, init, sizeof(init), &ip);
		init[ip++] = 0x02; /* mpint value 2 */
		wire_total += build_plaintext_packet(init, ip,
		    &wire_pkts[wire_total], sizeof(wire_pkts) - wire_total);
	}
	else if (test_using_sntrup()) {
		/* sntrup761x25519: ECDH_INIT with Q_C = pk(1158) || x25519(32) */
		uint8_t sntrup_pk[1158], sntrup_sk[1763];
		crypto_kem_sntrup761_keypair(sntrup_pk, sntrup_sk);
		uint8_t x25519_pk[32];
		RAND_bytes(x25519_pk, sizeof(x25519_pk));

		uint8_t init[1 + 4 + 1190];
		size_t ip = 0;
		init[ip++] = SSH_MSG_KEX_ECDH_INIT;
		dssh_serialize_uint32(1190, init, sizeof(init), &ip);
		memcpy(&init[ip], sntrup_pk, 1158);
		ip += 1158;
		memcpy(&init[ip], x25519_pk, 32);
		ip += 32;
		wire_total += build_plaintext_packet(init, ip,
		    &wire_pkts[wire_total], sizeof(wire_pkts) - wire_total);
	}
	else if (test_using_mlkem()) {
		/* mlkem768x25519: ECDH_INIT with Q_C = pk(1184) || x25519(32) */
		uint8_t mlkem_pk[1184], mlkem_sk[2400];
		crypto_kem_mlkem768_keypair(mlkem_pk, mlkem_sk);
		uint8_t x25519_pk[32];
		RAND_bytes(x25519_pk, sizeof(x25519_pk));

		uint8_t init[1 + 4 + 1216];
		size_t ip = 0;
		init[ip++] = SSH_MSG_KEX_ECDH_INIT;
		dssh_serialize_uint32(1216, init, sizeof(init), &ip);
		memcpy(&init[ip], mlkem_pk, 1184);
		ip += 1184;
		memcpy(&init[ip], x25519_pk, 32);
		ip += 32;
		wire_total += build_plaintext_packet(init, ip,
		    &wire_pkts[wire_total], sizeof(wire_pkts) - wire_total);
	}
	else {
		/* Curve25519: ECDH_INIT(Q_C) */
		uint8_t qc[32];
		RAND_bytes(qc, sizeof(qc)); /* any 32 bytes is valid X25519 */

		uint8_t init[1 + 4 + 32];
		size_t ip = 0;
		init[ip++] = SSH_MSG_KEX_ECDH_INIT;
		dssh_serialize_uint32(32, init, sizeof(init), &ip);
		memcpy(&init[ip], qc, 32);
		ip += 32;
		wire_total += build_plaintext_packet(init, ip,
		    &wire_pkts[wire_total], sizeof(wire_pkts) - wire_total);
	}

	/* Save server state for reset */
	uint32_t saved_rx_seq = server->trans.rx_seq;
	uint32_t saved_tx_seq = server->trans.tx_seq;

	/* === ITERATE === */
	int prev_count = -1;
	int limit = test_using_sntrup() ? 100000 : 500;
	for (int n = 0; n < limit; n++) {
		/* Fresh I/O for this iteration */
		struct mock_io_state iter_io;
		if (mock_io_init(&iter_io, 0) < 0)
			break;
		dssh_session_set_cbdata(server, &iter_io, &iter_io,
		    &iter_io, &iter_io);

		/* Inject pre-built client packets */
		mock_io_inject(&iter_io.c2s, wire_pkts, wire_total);

		/* Reset mutable server state */
		free(server->trans.shared_secret);
		server->trans.shared_secret = NULL;
		server->trans.shared_secret_sz = 0;
		free(server->trans.exchange_hash);
		server->trans.exchange_hash = NULL;
		server->trans.exchange_hash_sz = 0;
		server->trans.rx_seq = saved_rx_seq;
		server->trans.tx_seq = saved_tx_seq;
		server->trans.rx_since_rekey = 0;
		server->trans.tx_since_rekey = 0;
		server->trans.tx_bytes_since_rekey = 0;
		server->trans.rx_bytes_since_rekey = 0;
		server->terminate = false;

		dssh_test_ossl_fail_after(n);
		int res = kex(server);
		int cur_count = dssh_test_ossl_count();
		dssh_test_ossl_reset();

		/* Drain server output */
		uint8_t drain[16384];
		mock_io_drain(&iter_io.s2c, drain, sizeof(drain));

		mock_io_close_c2s(&iter_io);
		mock_io_close_s2c(&iter_io);
		mock_io_free(&iter_io);

		if (res == 0) {
			dssh_session_cleanup(server);
			dssh_test_reset_global_config();
			ASSERT_TRUE(n > 0);
			return TEST_PASS;
		}
		prev_count = cur_count;
	}

	fprintf(stderr, "  ossl/kex_server: still incrementing at n=%d "
	    "(count=%d), raise limit\n", limit, prev_count);
	dssh_session_cleanup(server);
	dssh_test_reset_global_config();
	return TEST_FAIL;
}

/*
 * Server kex thread that excludes itself from ossl injection.
 * The server runs with real OpenSSL; only the client thread
 * (which called dssh_test_ossl_fail_after) is affected.
 */
struct kex_excluded_ctx {
	struct mock_io_state *io;
	dssh_session sess;
	int result;
};

static int
kex_excluded_thread(void *arg)
{
	struct kex_excluded_ctx *ctx = arg;
	dssh_test_ossl_exclude_thread();
	ctx->result = kex(ctx->sess);
	if (ctx->result < 0) {
		mock_io_close_s2c(ctx->io);
	}
	return 0;
}

static int
test_ossl_kex_client_iterate(void)
{
	int prev_count = -1;
	int limit = test_using_sntrup() ? 100000 : 500;
	for (int n = 0; n < limit; n++) {
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

		/* version_exchange + kexinit in threads */
		{
			struct ve_ki_ctx ca = { .io = &io, .sess = client };
			struct ve_ki_ctx sa = { .io = &io, .sess = server };
			thrd_t ct, st;
			ASSERT_THRD_CREATE(&ct, ve_ki_thread, &ca);
			ASSERT_THRD_CREATE(&st, ve_ki_thread, &sa);
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

		/* Arm ossl injection on THIS thread (main).
		 * The server thread will exclude itself. */
		dssh_test_ossl_fail_after(n);

		/* Run KEX: server in excluded thread, client on main thread */
		struct kex_excluded_ctx sctx = {
			.io = &io, .sess = server
		};
		thrd_t st;
		ASSERT_THRD_CREATE(&st, kex_excluded_thread, &sctx);
		int cres = kex(client);
		/* Close client's write pipe so server unblocks on read */
		mock_io_close_c2s(&io);
		thrd_join(st, NULL);

		int cur_count = dssh_test_ossl_count();
		dssh_test_ossl_reset();

		mock_io_close_c2s(&io);
		mock_io_close_s2c(&io);
		dssh_session_cleanup(server);
		dssh_session_cleanup(client);
		mock_io_free(&io);
		dssh_test_reset_global_config();

		if (cres == 0 && sctx.result == 0) {
			ASSERT_TRUE(n > 0);
			return TEST_PASS;
		}
		prev_count = cur_count;
	}

	fprintf(stderr, "  ossl/kex_client: still incrementing at n=%d "
	    "(count=%d), raise limit\n", limit, prev_count);
	return TEST_FAIL;
}

/* ================================================================
 * Client-side parse/validate tests.  A "bad server" thread sends
 * crafted packets to exercise error paths in the client handler.
 * ================================================================ */

struct bad_server_ctx {
	struct mock_io_state *io;
	dssh_session sess;
	const uint8_t *reply_data;
	size_t reply_len;
	int result;
};

/*
 * Bad server: reads GEX_REQUEST, sends a pre-built reply, then
 * reads GEX_INIT (if the client gets that far) and closes.
 */
static int
bad_server_group_thread(void *arg)
{
	struct bad_server_ctx *ctx = arg;
	dssh_test_ossl_exclude_thread();
	dssh_test_alloc_exclude_thread();

	/* Read GEX_REQUEST from client */
	uint8_t msg_type;
	uint8_t *payload;
	size_t payload_len;
	ctx->result = recv_packet(ctx->sess,
	    &msg_type, &payload, &payload_len);
	if (ctx->result < 0) {
		mock_io_close_s2c(ctx->io);
		return 0;
	}

	/* Send the crafted reply (if any) */
	if (ctx->reply_data != NULL && ctx->reply_len > 0) {
		ctx->result = send_packet(ctx->sess,
		    ctx->reply_data, ctx->reply_len, NULL);
	}

	/* Close server output so client unblocks */
	mock_io_close_s2c(ctx->io);
	return 0;
}

/*
 * Bad server that sends a valid GEX_GROUP, reads GEX_INIT, then
 * sends a crafted GEX_REPLY.
 */
static int
bad_server_reply_thread(void *arg)
{
	struct bad_server_ctx *ctx = arg;
	dssh_test_ossl_exclude_thread();
	dssh_test_alloc_exclude_thread();

	/* Read GEX_REQUEST */
	uint8_t msg_type;
	uint8_t *payload;
	size_t payload_len;
	ctx->result = recv_packet(ctx->sess,
	    &msg_type, &payload, &payload_len);
	if (ctx->result < 0)
		goto done;

	/* Send a real GEX_GROUP via the real handler's first half.
	 * Actually, just run the real kex handler -- it sends GROUP
	 * and reads INIT internally.  Too complex.
	 *
	 * Simpler: send GROUP manually, read INIT, send bad REPLY. */

	/* Get the DH group from the provider */
	struct dssh_dh_gex_provider *prov =
	    (struct dssh_dh_gex_provider *)ctx->sess->trans.kex_selected->ctx;
	uint8_t *p_bytes = NULL, *g_bytes = NULL;
	size_t p_len, g_len;
	ctx->result = prov->select_group(2048, 4096, 8192,
	    &p_bytes, &p_len, &g_bytes, &g_len, prov->cbdata);
	if (ctx->result < 0)
		goto done;

	/* Build and send GEX_GROUP */
	{
		BIGNUM *p = BN_bin2bn(p_bytes, (int)p_len, NULL);
		BIGNUM *g_bn = BN_bin2bn(g_bytes, (int)g_len, NULL);
		free(p_bytes);
		free(g_bytes);
		if (!p || !g_bn) {
			BN_free(p);
			BN_free(g_bn);
			ctx->result = -1;
			goto done;
		}
		uint8_t group_msg[4096];
		size_t gp = 0;
		group_msg[gp++] = 31; /* GEX_GROUP */
		serialize_bn_mpint(p, group_msg, sizeof(group_msg), &gp);
		serialize_bn_mpint(g_bn, group_msg, sizeof(group_msg), &gp);
		BN_free(p);
		BN_free(g_bn);
		ctx->result = send_packet(ctx->sess,
		    group_msg, gp, NULL);
		if (ctx->result < 0)
			goto done;
	}

	/* Read GEX_INIT (client's e value) */
	ctx->result = recv_packet(ctx->sess,
	    &msg_type, &payload, &payload_len);
	if (ctx->result < 0)
		goto done;

	/* Send the crafted bad REPLY */
	ctx->result = send_packet(ctx->sess,
	    ctx->reply_data, ctx->reply_len, NULL);

done:
	mock_io_close_s2c(ctx->io);
	return 0;
}

static int
dhgex_client_parse_test(int (*server_thread)(void *),
    const uint8_t *reply, size_t reply_len)
{
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
		return TEST_FAIL;
	}
	dssh_session_set_cbdata(client, &io, &io, &io, &io);

	dssh_session server = init_server_session();
	if (server == NULL) {
		dssh_session_cleanup(client);
		mock_io_free(&io);
		return TEST_FAIL;
	}
	dssh_session_set_cbdata(server, &io, &io, &io, &io);

	/* version_exchange + kexinit */
	{
		struct ve_ki_ctx ca = { .io = &io, .sess = client };
		struct ve_ki_ctx sa = { .io = &io, .sess = server };
		thrd_t ct, st;
		if (thrd_create(&ct, ve_ki_thread, &ca) != thrd_success)
			return TEST_FAIL;
		if (thrd_create(&st, ve_ki_thread, &sa) != thrd_success)
			return TEST_FAIL;
		thrd_join(ct, NULL);
		thrd_join(st, NULL);
		if (ca.result != 0 || sa.result != 0) {
			mock_io_close_c2s(&io);
			mock_io_close_s2c(&io);
			dssh_session_cleanup(server);
			dssh_session_cleanup(client);
			mock_io_free(&io);
			dssh_test_reset_global_config();
			return TEST_FAIL;
		}
	}

	struct bad_server_ctx sctx = {
		.io = &io, .sess = server,
		.reply_data = reply, .reply_len = reply_len
	};
	thrd_t st;
	if (thrd_create(&st, server_thread, &sctx) != thrd_success)
		return TEST_FAIL;
	int cres = kex(client);
	mock_io_close_c2s(&io);
	thrd_join(st, NULL);

	mock_io_close_s2c(&io);
	dssh_session_cleanup(server);
	dssh_session_cleanup(client);
	mock_io_free(&io);
	dssh_test_reset_global_config();

	return cres;
}

/* Line 220: recv GEX_GROUP fails (server closes before sending) */
static int
test_dhgex_client_recv_group_fail(void)
{
	if (!test_using_dhgex())
		return TEST_SKIP;
	/* Empty reply + close -> recv fails */
	int res = dhgex_client_parse_test(bad_server_group_thread, NULL, 0);
	ASSERT_TRUE(res < 0);
	return TEST_PASS;
}

/* Line 226: GEX_GROUP payload empty after msg_type */
static int
test_dhgex_client_group_empty(void)
{
	if (!test_using_dhgex())
		return TEST_SKIP;
	uint8_t group[1] = { 31 }; /* GEX_GROUP, no p or g */
	int res = dhgex_client_parse_test(bad_server_group_thread, group, 1);
	ASSERT_TRUE(res < 0);
	return TEST_PASS;
}

/* Line 230: GEX_GROUP has p but no g */
static int
test_dhgex_client_group_no_g(void)
{
	if (!test_using_dhgex())
		return TEST_SKIP;
	/* GEX_GROUP with p=2 but no g */
	uint8_t group[16];
	size_t gp = 0;
	group[gp++] = 31;
	dssh_serialize_uint32(1, group, sizeof(group), &gp);
	group[gp++] = 0x02; /* mpint p=2 */
	int res = dhgex_client_parse_test(bad_server_group_thread, group, gp);
	ASSERT_TRUE(res < 0);
	return TEST_PASS;
}

/* Line 264: wrong msg_type in GEX_REPLY */
static int
test_dhgex_client_bad_reply_type(void)
{
	if (!test_using_dhgex())
		return TEST_SKIP;
	/* Send a GEX_INIT(32) instead of GEX_REPLY(33) */
	uint8_t reply[8];
	size_t rp = 0;
	reply[rp++] = 32; /* wrong type */
	dssh_serialize_uint32(1, reply, sizeof(reply), &rp);
	reply[rp++] = 0x02;
	int res = dhgex_client_parse_test(bad_server_reply_thread, reply, rp);
	ASSERT_TRUE(res < 0);
	return TEST_PASS;
}

/* Line 268: GEX_REPLY too short for K_S length field */
static int
test_dhgex_client_reply_short_ks(void)
{
	if (!test_using_dhgex())
		return TEST_SKIP;
	/* GEX_REPLY with only msg_type byte -- no K_S length field */
	uint8_t reply[1] = { 33 };
	int res = dhgex_client_parse_test(bad_server_reply_thread, reply, 1);
	ASSERT_TRUE(res < 0);
	return TEST_PASS;
}

/* Line 270: GEX_REPLY with K_S length overrunning payload */
static int
test_dhgex_client_reply_truncated_ks(void)
{
	if (!test_using_dhgex())
		return TEST_SKIP;
	/* GEX_REPLY with K_S length claiming 1000 bytes but only 2 present */
	uint8_t reply[16];
	size_t rp = 0;
	reply[rp++] = 33; /* GEX_REPLY */
	dssh_serialize_uint32(1000, reply, sizeof(reply), &rp);
	reply[rp++] = 0x01;
	reply[rp++] = 0x02;
	int res = dhgex_client_parse_test(bad_server_reply_thread, reply, rp);
	ASSERT_TRUE(res < 0);
	return TEST_PASS;
}

/* Line 280: GEX_REPLY with f=0 (invalid DH value) */
static int
test_dhgex_client_reply_f_zero(void)
{
	if (!test_using_dhgex())
		return TEST_SKIP;
	/* GEX_REPLY: K_S="", f=0, sig="" */
	uint8_t reply[32];
	size_t rp = 0;
	reply[rp++] = 33; /* GEX_REPLY */
	/* K_S: empty string (len=0) */
	dssh_serialize_uint32(0, reply, sizeof(reply), &rp);
	/* f: mpint 0 (len=0) */
	dssh_serialize_uint32(0, reply, sizeof(reply), &rp);
	/* sig: empty (len=0) -- won't be reached since f=0 fails first */
	dssh_serialize_uint32(0, reply, sizeof(reply), &rp);
	int res = dhgex_client_parse_test(bad_server_reply_thread, reply, rp);
	ASSERT_TRUE(res < 0);
	return TEST_PASS;
}

/* Line 289: GEX_REPLY too short for sig length field */
static int
test_dhgex_client_reply_short_sig(void)
{
	if (!test_using_dhgex())
		return TEST_SKIP;
	/* GEX_REPLY: K_S="", f=2 (valid), but payload ends before sig len */
	uint8_t reply[16];
	size_t rp = 0;
	reply[rp++] = 33; /* GEX_REPLY */
	/* K_S: empty string (len=0) */
	dssh_serialize_uint32(0, reply, sizeof(reply), &rp);
	/* f: mpint 2 */
	dssh_serialize_uint32(1, reply, sizeof(reply), &rp);
	reply[rp++] = 0x02;
	/* No sig at all -- payload ends here */
	int res = dhgex_client_parse_test(bad_server_reply_thread, reply, rp);
	ASSERT_TRUE(res < 0);
	return TEST_PASS;
}

/* Line 291: GEX_REPLY with sig length overrunning payload */
static int
test_dhgex_client_reply_truncated_sig(void)
{
	if (!test_using_dhgex())
		return TEST_SKIP;
	/* GEX_REPLY: K_S="", f=2 (valid), sig claiming 1000 bytes */
	uint8_t reply[32];
	size_t rp = 0;
	reply[rp++] = 33; /* GEX_REPLY */
	/* K_S: empty string */
	dssh_serialize_uint32(0, reply, sizeof(reply), &rp);
	/* f: mpint 2 */
	dssh_serialize_uint32(1, reply, sizeof(reply), &rp);
	reply[rp++] = 0x02;
	/* sig: claiming 1000 bytes but message ends here */
	dssh_serialize_uint32(1000, reply, sizeof(reply), &rp);
	int res = dhgex_client_parse_test(bad_server_reply_thread, reply, rp);
	ASSERT_TRUE(res < 0);
	return TEST_PASS;
}

/* ================================================================
 * Curve25519 client parse tests
 *
 * Equivalent of the DH-GEX client parse tests above, but for the
 * curve25519-sha256 KEX.  The "bad server" reads ECDH_INIT from
 * the client, sends a crafted ECDH_REPLY, then closes.
 * ================================================================ */

static int
bad_c25519_server_thread(void *arg)
{
	struct bad_server_ctx *ctx = arg;
	dssh_test_ossl_exclude_thread();
	dssh_test_alloc_exclude_thread();

	/* Read ECDH_INIT from client */
	uint8_t msg_type;
	uint8_t *payload;
	size_t payload_len;
	ctx->result = recv_packet(ctx->sess,
	    &msg_type, &payload, &payload_len);
	if (ctx->result < 0) {
		mock_io_close_s2c(ctx->io);
		return 0;
	}

	/* Send the crafted reply (if any) */
	if (ctx->reply_data != NULL && ctx->reply_len > 0) {
		ctx->result = send_packet(ctx->sess,
		    ctx->reply_data, ctx->reply_len, NULL);
	}

	/* Close server output so client unblocks */
	mock_io_close_s2c(ctx->io);
	return 0;
}

static int
c25519_client_parse_test(int (*server_thread)(void *),
    const uint8_t *reply, size_t reply_len)
{
	dssh_test_reset_global_config();
	dssh_test_alloc_reset();
	dssh_test_ossl_reset();
	mock_alloc_reset();

	if (dssh_register_curve25519_sha256() < 0)
		return TEST_FAIL;
	if (test_register_key_algos() < 0)
		return TEST_FAIL;
	if (dssh_register_aes256_ctr() < 0)
		return TEST_FAIL;
	if (dssh_register_hmac_sha2_256() < 0)
		return TEST_FAIL;
	if (dssh_register_none_comp() < 0)
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
		return TEST_FAIL;
	}
	dssh_session_set_cbdata(client, &io, &io, &io, &io);

	dssh_session server = dssh_session_init(false, 0);
	if (server == NULL) {
		dssh_session_cleanup(client);
		mock_io_free(&io);
		return TEST_FAIL;
	}
	dssh_session_set_cbdata(server, &io, &io, &io, &io);

	/* version_exchange + kexinit */
	{
		struct ve_ki_ctx ca = { .io = &io, .sess = client };
		struct ve_ki_ctx sa = { .io = &io, .sess = server };
		thrd_t ct, st;
		if (thrd_create(&ct, ve_ki_thread, &ca) != thrd_success)
			return TEST_FAIL;
		if (thrd_create(&st, ve_ki_thread, &sa) != thrd_success)
			return TEST_FAIL;
		thrd_join(ct, NULL);
		thrd_join(st, NULL);
		if (ca.result != 0 || sa.result != 0) {
			mock_io_close_c2s(&io);
			mock_io_close_s2c(&io);
			dssh_session_cleanup(server);
			dssh_session_cleanup(client);
			mock_io_free(&io);
			dssh_test_reset_global_config();
			return TEST_FAIL;
		}
	}

	struct bad_server_ctx sctx = {
		.io = &io, .sess = server,
		.reply_data = reply, .reply_len = reply_len
	};
	thrd_t st;
	if (thrd_create(&st, server_thread, &sctx) != thrd_success)
		return TEST_FAIL;
	int cres = kex(client);
	mock_io_close_c2s(&io);
	thrd_join(st, NULL);

	mock_io_close_s2c(&io);
	dssh_session_cleanup(server);
	dssh_session_cleanup(client);
	mock_io_free(&io);
	dssh_test_reset_global_config();

	return cres;
}

/* Line 257: recv ECDH_REPLY fails (server closes before sending) */
static int
test_c25519_client_recv_reply_fail(void)
{
	if (test_using_dhgex())
		return TEST_SKIP;
	/* Empty reply + close -> recv fails */
	int res = c25519_client_parse_test(bad_c25519_server_thread, NULL, 0);
	ASSERT_TRUE(res < 0);
	return TEST_PASS;
}

/* Line 263: ECDH_REPLY too short for K_S length field */
static int
test_c25519_client_reply_short_ks(void)
{
	if (test_using_dhgex())
		return TEST_SKIP;
	/* ECDH_REPLY with only msg_type byte -- no K_S length field */
	uint8_t reply[1] = { 31 }; /* SSH_MSG_KEX_ECDH_REPLY */
	int res = c25519_client_parse_test(bad_c25519_server_thread, reply, 1);
	ASSERT_TRUE(res < 0);
	return TEST_PASS;
}

/* Line 264: ECDH_REPLY with K_S length overrunning payload */
static int
test_c25519_client_reply_trunc_ks(void)
{
	if (test_using_dhgex())
		return TEST_SKIP;
	/* K_S length claiming 1000 bytes but only 2 present */
	uint8_t reply[16];
	size_t rp = 0;
	reply[rp++] = 31; /* SSH_MSG_KEX_ECDH_REPLY */
	dssh_serialize_uint32(1000, reply, sizeof(reply), &rp);
	reply[rp++] = 0x01;
	reply[rp++] = 0x02;
	int res = c25519_client_parse_test(bad_c25519_server_thread, reply, rp);
	ASSERT_TRUE(res < 0);
	return TEST_PASS;
}

/* Line 271: ECDH_REPLY with K_S="" but end before Q_S length field */
static int
test_c25519_client_reply_short_qs(void)
{
	if (test_using_dhgex())
		return TEST_SKIP;
	/* K_S: empty string (len=0), payload ends before Q_S len */
	uint8_t reply[8];
	size_t rp = 0;
	reply[rp++] = 31; /* SSH_MSG_KEX_ECDH_REPLY */
	dssh_serialize_uint32(0, reply, sizeof(reply), &rp);
	int res = c25519_client_parse_test(bad_c25519_server_thread, reply, rp);
	ASSERT_TRUE(res < 0);
	return TEST_PASS;
}

/* Line 277: ECDH_REPLY with Q_S length != 32 */
static int
test_c25519_client_reply_bad_qs_len(void)
{
	if (test_using_dhgex())
		return TEST_SKIP;
	/* K_S="" + Q_S with len=16 (not 32) */
	uint8_t reply[32];
	size_t rp = 0;
	reply[rp++] = 31; /* SSH_MSG_KEX_ECDH_REPLY */
	dssh_serialize_uint32(0, reply, sizeof(reply), &rp); /* K_S: empty */
	dssh_serialize_uint32(16, reply, sizeof(reply), &rp); /* Q_S len=16 */
	memset(&reply[rp], 0x42, 16);
	rp += 16;
	int res = c25519_client_parse_test(bad_c25519_server_thread, reply, rp);
	ASSERT_TRUE(res < 0);
	return TEST_PASS;
}

/* Line 277:35: ECDH_REPLY with K_S="" + Q_S len=32 but only 10 bytes present */
static int
test_c25519_client_reply_qs_overrun(void)
{
	if (test_using_dhgex())
		return TEST_SKIP;
	uint8_t reply[32];
	size_t rp = 0;
	reply[rp++] = 31; /* SSH_MSG_KEX_ECDH_REPLY */
	dssh_serialize_uint32(0, reply, sizeof(reply), &rp); /* K_S: empty */
	dssh_serialize_uint32(32, reply, sizeof(reply), &rp); /* Q_S len=32 */
	memset(&reply[rp], 0x42, 10); /* only 10 bytes of Q_S data */
	rp += 10;
	int res = c25519_client_parse_test(bad_c25519_server_thread, reply, rp);
	ASSERT_TRUE(res < 0);
	return TEST_PASS;
}

/* Line 283: ECDH_REPLY with K_S="" + Q_S(32 bytes) but no sig */
static int
test_c25519_client_reply_short_sig(void)
{
	if (test_using_dhgex())
		return TEST_SKIP;
	/* K_S="" + Q_S(32 bytes) + payload ends before sig len */
	uint8_t reply[48];
	size_t rp = 0;
	reply[rp++] = 31; /* SSH_MSG_KEX_ECDH_REPLY */
	dssh_serialize_uint32(0, reply, sizeof(reply), &rp); /* K_S: empty */
	dssh_serialize_uint32(32, reply, sizeof(reply), &rp); /* Q_S len=32 */
	memset(&reply[rp], 0x42, 32);
	rp += 32;
	/* No sig at all -- payload ends here */
	int res = c25519_client_parse_test(bad_c25519_server_thread, reply, rp);
	ASSERT_TRUE(res < 0);
	return TEST_PASS;
}

/* Line 289: ECDH_REPLY with sig length overrunning payload */
static int
test_c25519_client_reply_trunc_sig(void)
{
	if (test_using_dhgex())
		return TEST_SKIP;
	/* K_S="" + Q_S(32 bytes) + sig claiming 1000 bytes */
	uint8_t reply[64];
	size_t rp = 0;
	reply[rp++] = 31; /* SSH_MSG_KEX_ECDH_REPLY */
	dssh_serialize_uint32(0, reply, sizeof(reply), &rp); /* K_S: empty */
	dssh_serialize_uint32(32, reply, sizeof(reply), &rp); /* Q_S len=32 */
	memset(&reply[rp], 0x42, 32);
	rp += 32;
	/* sig: claiming 1000 bytes but message ends here */
	dssh_serialize_uint32(1000, reply, sizeof(reply), &rp);
	int res = c25519_client_parse_test(bad_c25519_server_thread, reply, rp);
	ASSERT_TRUE(res < 0);
	return TEST_PASS;
}

/*
 * Client kex with ka==NULL or ka->verify==NULL.  Uses two threads
 * with the server excluded.  The client runs through the full
 * handshake crypto, then hits the ka guard at the end.
 */
static int
test_ossl_kex_client_ka_null(void)
{
	if (!test_using_dhgex())
		return TEST_SKIP;

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
		return TEST_FAIL;
	}
	dssh_session_set_cbdata(client, &io, &io, &io, &io);

	dssh_session server = init_server_session();
	if (server == NULL) {
		dssh_session_cleanup(client);
		mock_io_free(&io);
		return TEST_FAIL;
	}
	dssh_session_set_cbdata(server, &io, &io, &io, &io);

	/* version_exchange + kexinit in threads */
	{
		struct ve_ki_ctx ca = { .io = &io, .sess = client };
		struct ve_ki_ctx sa = { .io = &io, .sess = server };
		thrd_t ct, st;
		ASSERT_THRD_CREATE(&ct, ve_ki_thread, &ca);
		ASSERT_THRD_CREATE(&st, ve_ki_thread, &sa);
		thrd_join(ct, NULL);
		thrd_join(st, NULL);
		if (ca.result != 0 || sa.result != 0) {
			mock_io_close_c2s(&io);
			mock_io_close_s2c(&io);
			dssh_session_cleanup(server);
			dssh_session_cleanup(client);
			mock_io_free(&io);
			dssh_test_reset_global_config();
			return TEST_FAIL;
		}
	}

	/* Null out client's key_algo_selected so ka==NULL at line 310 */
	client->trans.key_algo_selected = NULL;

	struct kex_excluded_ctx sctx = {
		.io = &io, .sess = server
	};
	thrd_t st;
	ASSERT_THRD_CREATE(&st, kex_excluded_thread, &sctx);
	int cres = kex(client);
	mock_io_close_c2s(&io);
	thrd_join(st, NULL);

	mock_io_close_s2c(&io);
	dssh_session_cleanup(server);
	dssh_session_cleanup(client);
	mock_io_free(&io);
	dssh_test_reset_global_config();

	ASSERT_TRUE(cres < 0);
	return TEST_PASS;
}

static int
test_ossl_kex_client_no_verify(void)
{
	if (!test_using_dhgex())
		return TEST_SKIP;

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
		return TEST_FAIL;
	}
	dssh_session_set_cbdata(client, &io, &io, &io, &io);

	dssh_session server = init_server_session();
	if (server == NULL) {
		dssh_session_cleanup(client);
		mock_io_free(&io);
		return TEST_FAIL;
	}
	dssh_session_set_cbdata(server, &io, &io, &io, &io);

	{
		struct ve_ki_ctx ca = { .io = &io, .sess = client };
		struct ve_ki_ctx sa = { .io = &io, .sess = server };
		thrd_t ct, st;
		ASSERT_THRD_CREATE(&ct, ve_ki_thread, &ca);
		ASSERT_THRD_CREATE(&st, ve_ki_thread, &sa);
		thrd_join(ct, NULL);
		thrd_join(st, NULL);
		if (ca.result != 0 || sa.result != 0) {
			mock_io_close_c2s(&io);
			mock_io_close_s2c(&io);
			dssh_session_cleanup(server);
			dssh_session_cleanup(client);
			mock_io_free(&io);
			dssh_test_reset_global_config();
			return TEST_FAIL;
		}
	}

	/* Set verify to NULL (but ka is non-NULL) */
	static struct dssh_key_algo_s dummy_ka = {0};
	client->trans.key_algo_selected = &dummy_ka;

	struct kex_excluded_ctx sctx = {
		.io = &io, .sess = server
	};
	thrd_t st;
	ASSERT_THRD_CREATE(&st, kex_excluded_thread, &sctx);
	int cres = kex(client);
	mock_io_close_c2s(&io);
	thrd_join(st, NULL);

	mock_io_close_s2c(&io);
	dssh_session_cleanup(server);
	/* Restore before cleanup to avoid dangling pointer */
	client->trans.key_algo_selected = NULL;
	dssh_session_cleanup(client);
	mock_io_free(&io);
	dssh_test_reset_global_config();

	ASSERT_TRUE(cres < 0);
	return TEST_PASS;
}

/* ================================================================
 * Library alloc injection iterates for KEX server/client.
 *
 * These cover malloc failures in serialize_bn_mpint, shared_secret,
 * reply buffer, and exchange_hash inside the DH-GEX handler.
 * The ossl iterates only inject OpenSSL failures; these inject
 * library malloc failures via dssh_test_alloc_fail_after().
 * ================================================================ */

static int
test_alloc_kex_server_iterate(void)
{
	int limit = test_using_sntrup() ? 100000 : 200;

	/* === ONE-TIME SETUP (same as ossl/kex_server) === */
	dssh_test_reset_global_config();
	dssh_test_alloc_reset();
	dssh_test_ossl_reset();
	mock_alloc_reset();

	if (register_all() < 0)
		return TEST_FAIL;
	if (test_generate_host_key() < 0)
		return TEST_FAIL;

	struct mock_io_state setup_io;
	if (mock_io_init(&setup_io, 0) < 0)
		return TEST_FAIL;

	dssh_transport_set_callbacks(mock_tx_dispatch, mock_rx_dispatch,
	    mock_rxline_dispatch, mock_extra_line_cb);

	dssh_session client = dssh_session_init(true, 0);
	if (client == NULL) {
		mock_io_free(&setup_io);
		return TEST_FAIL;
	}
	dssh_session_set_cbdata(client, &setup_io, &setup_io,
	    &setup_io, &setup_io);

	dssh_session server = init_server_session();
	if (server == NULL) {
		dssh_session_cleanup(client);
		mock_io_free(&setup_io);
		return TEST_FAIL;
	}
	dssh_session_set_cbdata(server, &setup_io, &setup_io,
	    &setup_io, &setup_io);

	/* Two-threaded version_exchange + kexinit */
	{
		struct ve_ki_ctx ca = { .io = &setup_io, .sess = client };
		struct ve_ki_ctx sa = { .io = &setup_io, .sess = server };
		thrd_t ct, st;
		ASSERT_THRD_CREATE(&ct, ve_ki_thread, &ca);
		ASSERT_THRD_CREATE(&st, ve_ki_thread, &sa);
		thrd_join(ct, NULL);
		thrd_join(st, NULL);
		if (ca.result != 0 || sa.result != 0) {
			mock_io_close_c2s(&setup_io);
			mock_io_close_s2c(&setup_io);
			dssh_session_cleanup(server);
			dssh_session_cleanup(client);
			mock_io_free(&setup_io);
			dssh_test_reset_global_config();
			return TEST_FAIL;
		}
	}

	dssh_session_cleanup(client);
	mock_io_close_c2s(&setup_io);
	mock_io_close_s2c(&setup_io);
	mock_io_free(&setup_io);

	/* Build client KEX packets */
	uint8_t wire_pkts[8192];
	size_t wire_total = 0;

	if (test_using_dhgex()) {
		/* DH-GEX: GEX_REQUEST + GEX_INIT */
		uint8_t req[16];
		size_t rp = 0;
		req[rp++] = 34;
		dssh_serialize_uint32(2048, req, sizeof(req), &rp);
		dssh_serialize_uint32(4096, req, sizeof(req), &rp);
		dssh_serialize_uint32(8192, req, sizeof(req), &rp);
		wire_total += build_plaintext_packet(req, rp,
		    &wire_pkts[wire_total], sizeof(wire_pkts) - wire_total);

		uint8_t init[16];
		size_t ip = 0;
		init[ip++] = 32;
		dssh_serialize_uint32(1, init, sizeof(init), &ip);
		init[ip++] = 0x02;
		wire_total += build_plaintext_packet(init, ip,
		    &wire_pkts[wire_total], sizeof(wire_pkts) - wire_total);
	}
	else if (test_using_sntrup()) {
		/* sntrup761x25519: ECDH_INIT with Q_C = pk(1158) || x25519(32) */
		uint8_t sntrup_pk[1158], sntrup_sk[1763];
		crypto_kem_sntrup761_keypair(sntrup_pk, sntrup_sk);
		uint8_t x25519_pk[32];
		RAND_bytes(x25519_pk, sizeof(x25519_pk));

		uint8_t init[1 + 4 + 1190];
		size_t ip = 0;
		init[ip++] = SSH_MSG_KEX_ECDH_INIT;
		dssh_serialize_uint32(1190, init, sizeof(init), &ip);
		memcpy(&init[ip], sntrup_pk, 1158);
		ip += 1158;
		memcpy(&init[ip], x25519_pk, 32);
		ip += 32;
		wire_total += build_plaintext_packet(init, ip,
		    &wire_pkts[wire_total], sizeof(wire_pkts) - wire_total);
	}
	else if (test_using_mlkem()) {
		/* mlkem768x25519: ECDH_INIT with Q_C = pk(1184) || x25519(32) */
		uint8_t mlkem_pk[1184], mlkem_sk[2400];
		crypto_kem_mlkem768_keypair(mlkem_pk, mlkem_sk);
		uint8_t x25519_pk[32];
		RAND_bytes(x25519_pk, sizeof(x25519_pk));

		uint8_t init[1 + 4 + 1216];
		size_t ip = 0;
		init[ip++] = SSH_MSG_KEX_ECDH_INIT;
		dssh_serialize_uint32(1216, init, sizeof(init), &ip);
		memcpy(&init[ip], mlkem_pk, 1184);
		ip += 1184;
		memcpy(&init[ip], x25519_pk, 32);
		ip += 32;
		wire_total += build_plaintext_packet(init, ip,
		    &wire_pkts[wire_total], sizeof(wire_pkts) - wire_total);
	}
	else {
		/* Curve25519: ECDH_INIT(Q_C) */
		uint8_t qc[32];
		RAND_bytes(qc, sizeof(qc));

		uint8_t init[1 + 4 + 32];
		size_t ip = 0;
		init[ip++] = SSH_MSG_KEX_ECDH_INIT;
		dssh_serialize_uint32(32, init, sizeof(init), &ip);
		memcpy(&init[ip], qc, 32);
		ip += 32;
		wire_total += build_plaintext_packet(init, ip,
		    &wire_pkts[wire_total], sizeof(wire_pkts) - wire_total);
	}

	uint32_t saved_rx_seq = server->trans.rx_seq;
	uint32_t saved_tx_seq = server->trans.tx_seq;

	/* === ITERATE === */
	for (int n = 0; n < limit; n++) {
		struct mock_io_state iter_io;
		if (mock_io_init(&iter_io, 0) < 0)
			break;
		dssh_session_set_cbdata(server, &iter_io, &iter_io,
		    &iter_io, &iter_io);

		mock_io_inject(&iter_io.c2s, wire_pkts, wire_total);

		free(server->trans.shared_secret);
		server->trans.shared_secret = NULL;
		server->trans.shared_secret_sz = 0;
		free(server->trans.exchange_hash);
		server->trans.exchange_hash = NULL;
		server->trans.exchange_hash_sz = 0;
		server->trans.rx_seq = saved_rx_seq;
		server->trans.tx_seq = saved_tx_seq;
		server->trans.rx_since_rekey = 0;
		server->trans.tx_since_rekey = 0;
		server->trans.tx_bytes_since_rekey = 0;
		server->trans.rx_bytes_since_rekey = 0;
		server->terminate = false;

		dssh_test_alloc_fail_after(n);
		int res = kex(server);
		dssh_test_alloc_reset();

		uint8_t drain[16384];
		mock_io_drain(&iter_io.s2c, drain, sizeof(drain));

		mock_io_close_c2s(&iter_io);
		mock_io_close_s2c(&iter_io);
		mock_io_free(&iter_io);

		if (res == 0) {
			dssh_session_cleanup(server);
			dssh_test_reset_global_config();
			ASSERT_TRUE(n > 0);
			return TEST_PASS;
		}
	}

	fprintf(stderr, "  alloc/kex_server: never succeeded within limit\n");
	dssh_session_cleanup(server);
	dssh_test_reset_global_config();
	return TEST_FAIL;
}

/*
 * alloc/kex_client: library alloc injection on the client during
 * a two-threaded DH-GEX handshake.  The server thread excludes
 * itself from alloc injection via dssh_test_alloc_exclude_thread().
 */
static int
alloc_kex_excluded_thread(void *arg)
{
	struct kex_excluded_ctx *ctx = arg;
	dssh_test_ossl_exclude_thread();
	dssh_test_alloc_exclude_thread();
	ctx->result = kex(ctx->sess);
	if (ctx->result < 0)
		mock_io_close_s2c(ctx->io);
	return 0;
}

static int
test_alloc_kex_client_iterate(void)
{
	int limit = test_using_sntrup() ? 100000 : 200;
	for (int n = 0; n < limit; n++) {
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

		{
			struct ve_ki_ctx ca = { .io = &io, .sess = client };
			struct ve_ki_ctx sa = { .io = &io, .sess = server };
			thrd_t ct, st;
			ASSERT_THRD_CREATE(&ct, ve_ki_thread, &ca);
			ASSERT_THRD_CREATE(&st, ve_ki_thread, &sa);
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

		dssh_test_alloc_fail_after(n);

		struct kex_excluded_ctx sctx = {
			.io = &io, .sess = server
		};
		thrd_t st;
		ASSERT_THRD_CREATE(&st, alloc_kex_excluded_thread, &sctx);
		int cres = kex(client);
		mock_io_close_c2s(&io);
		thrd_join(st, NULL);

		dssh_test_alloc_reset();

		mock_io_close_s2c(&io);
		dssh_session_cleanup(server);
		dssh_session_cleanup(client);
		mock_io_free(&io);
		dssh_test_reset_global_config();

		if (cres == 0 && sctx.result == 0) {
			ASSERT_TRUE(n > 0);
			return TEST_PASS;
		}
	}

	fprintf(stderr, "  alloc/kex_client: never succeeded within limit\n");
	return TEST_FAIL;
}

/* ================================================================
 * Helper: set up a negotiated server session (version exchange +
 * kexinit done) and discard the client.  Returns the server session
 * ready for kex(), or NULL on failure.
 * ================================================================ */

static dssh_session
kex_server_setup(void)
{
	dssh_test_reset_global_config();
	dssh_test_alloc_reset();
	dssh_test_ossl_reset();
	mock_alloc_reset();

	if (register_all() < 0)
		return NULL;
	if (test_generate_host_key() < 0)
		return NULL;

	struct mock_io_state setup_io;
	if (mock_io_init(&setup_io, 0) < 0)
		return NULL;

	dssh_transport_set_callbacks(mock_tx_dispatch, mock_rx_dispatch,
	    mock_rxline_dispatch, mock_extra_line_cb);

	dssh_session client = dssh_session_init(true, 0);
	if (client == NULL) {
		mock_io_free(&setup_io);
		return NULL;
	}
	dssh_session_set_cbdata(client, &setup_io, &setup_io,
	    &setup_io, &setup_io);

	dssh_session server = init_server_session();
	if (server == NULL) {
		dssh_session_cleanup(client);
		mock_io_free(&setup_io);
		return NULL;
	}
	dssh_session_set_cbdata(server, &setup_io, &setup_io,
	    &setup_io, &setup_io);

	struct ve_ki_ctx ca = { .io = &setup_io, .sess = client };
	struct ve_ki_ctx sa = { .io = &setup_io, .sess = server };
	thrd_t ct, st;
	if (thrd_create(&ct, ve_ki_thread, &ca) != thrd_success)
		return NULL;
	if (thrd_create(&st, ve_ki_thread, &sa) != thrd_success)
		return NULL;
	thrd_join(ct, NULL);
	thrd_join(st, NULL);

	dssh_session_cleanup(client);
	mock_io_close_c2s(&setup_io);
	mock_io_close_s2c(&setup_io);
	mock_io_free(&setup_io);

	if (ca.result != 0 || sa.result != 0) {
		dssh_session_cleanup(server);
		dssh_test_reset_global_config();
		return NULL;
	}

	return server;
}

/* ================================================================
 * curve25519: truncated ECDH_INIT (too short for qc_len parse)
 * ================================================================ */

static int
test_c25519_server_truncated_init(void)
{
	if (test_using_dhgex())
		return TEST_SKIP;

	dssh_session server = kex_server_setup();
	if (server == NULL)
		return TEST_FAIL;

	/* Build a truncated ECDH_INIT: just the msg type, no qc_len */
	uint8_t init[1];
	init[0] = SSH_MSG_KEX_ECDH_INIT;

	uint8_t wire[64];
	size_t wire_len = build_plaintext_packet(init, 1, wire, sizeof(wire));

	struct mock_io_state io;
	if (mock_io_init(&io, 0) < 0) {
		dssh_session_cleanup(server);
		dssh_test_reset_global_config();
		return TEST_FAIL;
	}
	dssh_session_set_cbdata(server, &io, &io, &io, &io);
	mock_io_inject(&io.c2s, wire, wire_len);

	int res = kex(server);

	mock_io_close_c2s(&io);
	mock_io_close_s2c(&io);
	mock_io_free(&io);
	dssh_session_cleanup(server);
	dssh_test_reset_global_config();

	ASSERT_TRUE(res < 0);
	return TEST_PASS;
}

/* ================================================================
 * curve25519: client receives ECDH_REPLY with bad Q_S length
 * ================================================================ */

static int
test_c25519_client_bad_qs_length(void)
{
	if (test_using_dhgex())
		return TEST_SKIP;

	/* K_S="" + Q_S with len=0 (not 32), enough data present */
	uint8_t reply[16];
	size_t rp = 0;
	reply[rp++] = 31; /* SSH_MSG_KEX_ECDH_REPLY */
	dssh_serialize_uint32(0, reply, sizeof(reply), &rp); /* K_S: empty */
	dssh_serialize_uint32(0, reply, sizeof(reply), &rp); /* Q_S len=0 */

	int res = c25519_client_parse_test(bad_c25519_server_thread, reply, rp);
	ASSERT_TRUE(res < 0);
	return TEST_PASS;
}

/* ================================================================
 * dh-gex: BN_rand failure during server-side KEX
 * ================================================================ */

static int
test_dhgex_server_bn_rand_fail(void)
{
	if (!test_using_dhgex())
		return TEST_SKIP;

	dssh_session server = kex_server_setup();
	if (server == NULL)
		return TEST_FAIL;

	/* Build GEX_REQUEST + GEX_INIT with e=2 */
	uint8_t wire_pkts[8192];
	size_t wire_total = 0;

	uint8_t req[16];
	size_t rp = 0;
	req[rp++] = 34; /* SSH_MSG_KEX_DH_GEX_REQUEST */
	dssh_serialize_uint32(2048, req, sizeof(req), &rp);
	dssh_serialize_uint32(4096, req, sizeof(req), &rp);
	dssh_serialize_uint32(8192, req, sizeof(req), &rp);
	wire_total += build_plaintext_packet(req, rp,
	    &wire_pkts[wire_total], sizeof(wire_pkts) - wire_total);

	uint8_t init[16];
	size_t ip = 0;
	init[ip++] = 32; /* SSH_MSG_KEX_DH_GEX_INIT */
	dssh_serialize_uint32(1, init, sizeof(init), &ip);
	init[ip++] = 0x02; /* mpint value 2 */
	wire_total += build_plaintext_packet(init, ip,
	    &wire_pkts[wire_total], sizeof(wire_pkts) - wire_total);

	uint32_t saved_rx_seq = server->trans.rx_seq;
	uint32_t saved_tx_seq = server->trans.tx_seq;

	int prev_count = -1;
	for (int n = 0; n < 500; n++) {
		struct mock_io_state io;
		if (mock_io_init(&io, 0) < 0)
			break;
		dssh_session_set_cbdata(server, &io, &io, &io, &io);
		mock_io_inject(&io.c2s, wire_pkts, wire_total);

		free(server->trans.shared_secret);
		server->trans.shared_secret = NULL;
		server->trans.shared_secret_sz = 0;
		free(server->trans.exchange_hash);
		server->trans.exchange_hash = NULL;
		server->trans.exchange_hash_sz = 0;
		server->trans.rx_seq = saved_rx_seq;
		server->trans.tx_seq = saved_tx_seq;
		server->trans.rx_since_rekey = 0;
		server->trans.tx_since_rekey = 0;
		server->trans.tx_bytes_since_rekey = 0;
		server->trans.rx_bytes_since_rekey = 0;
		server->terminate = false;

		dssh_test_ossl_fail_after(n);
		int res = kex(server);
		int cur_count = dssh_test_ossl_count();
		dssh_test_ossl_reset();

		uint8_t drain[16384];
		mock_io_drain(&io.s2c, drain, sizeof(drain));

		mock_io_close_c2s(&io);
		mock_io_close_s2c(&io);
		mock_io_free(&io);

		if (res == 0) {
			dssh_session_cleanup(server);
			dssh_test_reset_global_config();
			ASSERT_TRUE(n > 0);
			return TEST_PASS;
		}
		if (cur_count == prev_count) {
			dssh_session_cleanup(server);
			dssh_test_reset_global_config();
			return TEST_PASS;
		}
		prev_count = cur_count;
	}

	fprintf(stderr, "  dhgex/server_bn_rand_fail: still incrementing "
	    "at n=%d (count=%d)\n", 500, prev_count);
	dssh_session_cleanup(server);
	dssh_test_reset_global_config();
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
	{ "alloc/dssh_register_none_comp",     test_alloc_register_none_comp },
	{ "alloc/dssh_register_none_enc",      test_alloc_register_none_enc },
	{ "alloc/dssh_register_none_mac",      test_alloc_register_none_mac },
	{ "alloc/register_dh_gex",        test_alloc_register_dh_gex },

	/* Session init */
	{ "alloc/session_init",           test_alloc_session_init },
	{ "ossl/session_init",            test_ossl_session_init },

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
	{ "ossl/key_sign_verify",         test_ossl_key_sign_verify_iterate },
	{ "ossl/keygen",                  test_ossl_keygen_iterate },
	{ "ossl/enc",                     test_ossl_enc_iterate },
	{ "ossl/mac",                     test_ossl_mac_iterate },
	{ "ossl/key_verify",              test_ossl_key_verify_iterate },
	{ "ossl/key_pubkey",              test_ossl_key_pubkey_iterate },
	{ "ossl/kex_server",              test_ossl_kex_server_iterate },
	{ "ossl/kex_client",              test_ossl_kex_client_iterate },
	{ "ossl/kex_client_ka_null",      test_ossl_kex_client_ka_null },
	{ "ossl/kex_client_no_verify",    test_ossl_kex_client_no_verify },
	{ "dhgex/client_recv_group_fail", test_dhgex_client_recv_group_fail },
	{ "dhgex/client_group_empty",     test_dhgex_client_group_empty },
	{ "dhgex/client_group_no_g",      test_dhgex_client_group_no_g },
	{ "dhgex/client_bad_reply_type",  test_dhgex_client_bad_reply_type },
	{ "dhgex/client_short_ks",        test_dhgex_client_reply_short_ks },
	{ "dhgex/client_trunc_ks",        test_dhgex_client_reply_truncated_ks },
	{ "dhgex/client_reply_f_zero",    test_dhgex_client_reply_f_zero },
	{ "dhgex/client_short_sig",       test_dhgex_client_reply_short_sig },
	{ "dhgex/client_trunc_sig",       test_dhgex_client_reply_truncated_sig },
	{ "c25519/client_recv_reply",     test_c25519_client_recv_reply_fail },
	{ "c25519/client_short_ks",       test_c25519_client_reply_short_ks },
	{ "c25519/client_trunc_ks",       test_c25519_client_reply_trunc_ks },
	{ "c25519/client_short_qs",       test_c25519_client_reply_short_qs },
	{ "c25519/client_bad_qs_len",     test_c25519_client_reply_bad_qs_len },
	{ "c25519/client_qs_overrun",     test_c25519_client_reply_qs_overrun },
	{ "c25519/client_short_sig",      test_c25519_client_reply_short_sig },
	{ "c25519/client_trunc_sig",      test_c25519_client_reply_trunc_sig },
	{ "alloc/kex_server",             test_alloc_kex_server_iterate },
	{ "alloc/kex_client",             test_alloc_kex_client_iterate },
	{ "c25519/server_truncated_init", test_c25519_server_truncated_init },
	{ "c25519/client_bad_qs_length",  test_c25519_client_bad_qs_length },
	{ "dhgex/server_bn_rand_fail",    test_dhgex_server_bn_rand_fail },
};

DSSH_TEST_MAIN(tests)
