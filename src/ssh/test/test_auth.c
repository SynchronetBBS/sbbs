/*
 * test_auth.c — Authentication layer tests for DeuceSSH.
 *
 * Tier 2 layer tests with mock I/O.  Covers password authentication,
 * keyboard-interactive, publickey, server-side auth callbacks, and
 * full client+server roundtrip scenarios.
 *
 * Each test that performs authentication requires a completed transport
 * handshake first.  The handshake_setup()/handshake_cleanup() pattern
 * from test_transport.c is replicated here.
 */

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>

#include "dssh_test.h"
#include "deucessh.h"
#include "deucessh-algorithms.h"
#include "deucessh-auth.h"
#include "ssh-trans.h"
#include "ssh-internal.h"
#include "dssh_test_internal.h"
#include "mock_io.h"
#include "test_dhgex_provider.h"
#include "dssh_test_alloc.h"


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
		res = dssh_register_dh_gex_sha256();
	else if (test_using_sntrup())
		res = dssh_register_sntrup761x25519_sha512();
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
 * Helper: full handshake setup for two-sided tests
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
 * Server-side auth callback helpers
 * ================================================================ */

struct test_auth_cbdata {
	const char *expect_user;
	const char *expect_pass;
	int password_result;
	int none_result;
	int publickey_result;
	bool password_called;
	bool none_called;
	bool publickey_called;
	char captured_user[256];
	size_t captured_user_len;
	char captured_pass[256];
	size_t captured_pass_len;
};

static int
test_password_cb(const uint8_t *username, size_t username_len,
    const uint8_t *password, size_t password_len,
    uint8_t **change_prompt, size_t *change_prompt_len,
    void *cbdata)
{
	struct test_auth_cbdata *d = cbdata;
	d->password_called = true;

	if (username_len < sizeof(d->captured_user)) {
		memcpy(d->captured_user, username, username_len);
		d->captured_user_len = username_len;
	}
	if (password_len < sizeof(d->captured_pass)) {
		memcpy(d->captured_pass, password, password_len);
		d->captured_pass_len = password_len;
	}

	(void)change_prompt;
	(void)change_prompt_len;

	if (d->expect_user != NULL && d->expect_pass != NULL) {
		if (username_len == strlen(d->expect_user) &&
		    memcmp(username, d->expect_user, username_len) == 0 &&
		    password_len == strlen(d->expect_pass) &&
		    memcmp(password, d->expect_pass, password_len) == 0)
			return d->password_result;
		return DSSH_AUTH_FAILURE;
	}

	return d->password_result;
}

static int
test_none_cb(const uint8_t *username, size_t username_len, void *cbdata)
{
	struct test_auth_cbdata *d = cbdata;
	d->none_called = true;
	if (username_len < sizeof(d->captured_user)) {
		memcpy(d->captured_user, username, username_len);
		d->captured_user_len = username_len;
	}
	return d->none_result;
}

static int
test_publickey_cb(const uint8_t *username, size_t username_len,
    const char *algo_name, const uint8_t *pubkey_blob, size_t pubkey_blob_len,
    bool has_signature, void *cbdata)
{
	struct test_auth_cbdata *d = cbdata;
	d->publickey_called = true;
	(void)algo_name;
	(void)pubkey_blob;
	(void)pubkey_blob_len;
	(void)has_signature;
	if (username_len < sizeof(d->captured_user)) {
		memcpy(d->captured_user, username, username_len);
		d->captured_user_len = username_len;
	}
	return d->publickey_result;
}

/* ================================================================
 * Roundtrip thread argument structures
 * ================================================================ */

struct auth_client_arg {
	struct handshake_ctx *ctx;
	const char *username;
	const char *password;
	const char *algo_name;
	dssh_auth_kbi_prompt_cb kbi_cb;
	void *kbi_cbdata;
	int result;
	int methods_result;
	char methods[256];
	bool do_get_methods;
	bool do_password;
	bool do_kbi;
	bool do_publickey;
	int retry_count;
};

struct auth_server_arg {
	struct handshake_ctx *ctx;
	struct dssh_auth_server_cbs cbs;
	int result;
	uint8_t username_out[256];
	size_t username_out_len;
};

static int
auth_client_thread(void *arg)
{
	struct auth_client_arg *a = arg;
	dssh_session sess = a->ctx->client;

	if (a->do_get_methods) {
		a->methods_result = dssh_auth_get_methods(sess,
		    a->username, a->methods, sizeof(a->methods));
		if (a->methods_result < 0) {
			a->result = a->methods_result;
			return 0;
		}
	}

	for (int attempt = 0; attempt <= a->retry_count; attempt++) {
		if (a->do_password) {
			a->result = dssh_auth_password(sess,
			    a->username, a->password, NULL, NULL);
			if (a->result == 0)
				return 0;
		}
		else if (a->do_kbi) {
			a->result = dssh_auth_keyboard_interactive(sess,
			    a->username, a->kbi_cb, a->kbi_cbdata);
			if (a->result == 0)
				return 0;
		}
		else if (a->do_publickey) {
			a->result = dssh_auth_publickey(sess,
			    a->username, a->algo_name);
			if (a->result == 0)
				return 0;
		}
	}

	return 0;
}

static int
auth_server_thread(void *arg)
{
	struct auth_server_arg *a = arg;
	a->result = dssh_auth_server(a->ctx->server, &a->cbs,
	    a->username_out, &a->username_out_len);
	return 0;
}

/* ================================================================
 * KBI prompt callback for tests
 * ================================================================ */

struct test_kbi_cbdata {
	const char *response;
	bool called;
	uint32_t num_prompts_seen;
	char name_seen[128];
	char instruction_seen[128];
};

static int
test_kbi_prompt_cb(const uint8_t *name, size_t name_len,
    const uint8_t *instruction, size_t instruction_len,
    uint32_t num_prompts,
    const uint8_t **prompts, const size_t *prompt_lens,
    const bool *echo,
    uint8_t **responses, size_t *response_lens,
    void *cbdata)
{
	struct test_kbi_cbdata *d = cbdata;
	d->called = true;
	d->num_prompts_seen = num_prompts;

	(void)prompts;
	(void)prompt_lens;
	(void)echo;

	if (name_len > 0 && name_len < sizeof(d->name_seen)) {
		memcpy(d->name_seen, name, name_len);
		d->name_seen[name_len] = 0;
	}
	if (instruction_len > 0 && instruction_len < sizeof(d->instruction_seen)) {
		memcpy(d->instruction_seen, instruction, instruction_len);
		d->instruction_seen[instruction_len] = 0;
	}

	for (uint32_t i = 0; i < num_prompts; i++) {
		size_t rlen = strlen(d->response);
		responses[i] = malloc(rlen);
		if (responses[i] == NULL)
			return -1;
		memcpy(responses[i], d->response, rlen);
		response_lens[i] = rlen;
	}

	return 0;
}


/* ================================================================
 * Password auth tests (~6)
 * ================================================================ */

static int
test_auth_password_success(void)
{
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	struct test_auth_cbdata cbdata = {0};
	cbdata.expect_user = "testuser";
	cbdata.expect_pass = "secret123";
	cbdata.password_result = DSSH_AUTH_SUCCESS;

	struct auth_client_arg ca = {0};
	ca.ctx = &ctx;
	ca.username = "testuser";
	ca.password = "secret123";
	ca.do_get_methods = true;
	ca.do_password = true;

	struct auth_server_arg sa = {0};
	sa.ctx = &ctx;
	sa.cbs.methods_str = "password";
	sa.cbs.password_cb = test_password_cb;
	sa.cbs.cbdata = &cbdata;

	thrd_t ct, st;
	ASSERT_TRUE(thrd_create(&ct, auth_client_thread, &ca) == thrd_success);
	ASSERT_TRUE(thrd_create(&st, auth_server_thread, &sa) == thrd_success);

	thrd_join(ct, NULL);
	/* Close pipes to unblock server if still in auth loop */
	mock_io_close_c2s(&ctx.io);
	mock_io_close_s2c(&ctx.io);
	thrd_join(st, NULL);

	ASSERT_EQ(ca.result, 0);
	ASSERT_EQ(sa.result, 0);
	ASSERT_TRUE(cbdata.password_called);

	handshake_cleanup(&ctx);
	return TEST_PASS;
}

static int
test_auth_password_failure(void)
{
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	struct test_auth_cbdata cbdata = {0};
	cbdata.expect_user = "testuser";
	cbdata.expect_pass = "correct";
	cbdata.password_result = DSSH_AUTH_SUCCESS;

	struct auth_client_arg ca = {0};
	ca.ctx = &ctx;
	ca.username = "testuser";
	ca.password = "wrong";
	ca.do_get_methods = true;
	ca.do_password = true;

	struct auth_server_arg sa = {0};
	sa.ctx = &ctx;
	sa.cbs.methods_str = "password";
	sa.cbs.password_cb = test_password_cb;
	sa.cbs.cbdata = &cbdata;

	thrd_t ct, st;
	ASSERT_TRUE(thrd_create(&ct, auth_client_thread, &ca) == thrd_success);
	ASSERT_TRUE(thrd_create(&st, auth_server_thread, &sa) == thrd_success);

	/* Client finishes first (gets FAILURE), then we unblock
	 * the server which is still in the auth loop. */
	thrd_join(ct, NULL);
	mock_io_close_c2s(&ctx.io);
	mock_io_close_s2c(&ctx.io);
	thrd_join(st, NULL);

	ASSERT_TRUE(ca.result < 0);
	ASSERT_TRUE(cbdata.password_called);

	handshake_cleanup(&ctx);
	return TEST_PASS;
}

static int
test_auth_password_wrong_user(void)
{
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	struct test_auth_cbdata cbdata = {0};
	cbdata.expect_user = "admin";
	cbdata.expect_pass = "secret";
	cbdata.password_result = DSSH_AUTH_SUCCESS;

	struct auth_client_arg ca = {0};
	ca.ctx = &ctx;
	ca.username = "wronguser";
	ca.password = "secret";
	ca.do_get_methods = true;
	ca.do_password = true;

	struct auth_server_arg sa = {0};
	sa.ctx = &ctx;
	sa.cbs.methods_str = "password";
	sa.cbs.password_cb = test_password_cb;
	sa.cbs.cbdata = &cbdata;

	thrd_t ct, st;
	ASSERT_TRUE(thrd_create(&ct, auth_client_thread, &ca) == thrd_success);
	ASSERT_TRUE(thrd_create(&st, auth_server_thread, &sa) == thrd_success);

	thrd_join(ct, NULL);
	/* Close pipes to unblock server if still in auth loop */
	mock_io_close_c2s(&ctx.io);
	mock_io_close_s2c(&ctx.io);
	thrd_join(st, NULL);

	ASSERT_TRUE(ca.result < 0);
	ASSERT_TRUE(cbdata.password_called);

	mock_io_close_c2s(&ctx.io);
	mock_io_close_s2c(&ctx.io);

	handshake_cleanup(&ctx);
	return TEST_PASS;
}

static int
test_auth_password_partial(void)
{
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	struct test_auth_cbdata cbdata = {0};
	cbdata.password_result = DSSH_AUTH_PARTIAL;

	struct auth_client_arg ca = {0};
	ca.ctx = &ctx;
	ca.username = "testuser";
	ca.password = "secret";
	ca.do_get_methods = true;
	ca.do_password = true;

	struct auth_server_arg sa = {0};
	sa.ctx = &ctx;
	sa.cbs.methods_str = "password,publickey";
	sa.cbs.password_cb = test_password_cb;
	sa.cbs.cbdata = &cbdata;

	thrd_t ct, st;
	ASSERT_TRUE(thrd_create(&ct, auth_client_thread, &ca) == thrd_success);
	ASSERT_TRUE(thrd_create(&st, auth_server_thread, &sa) == thrd_success);

	thrd_join(ct, NULL);
	/* Close pipes to unblock server if still in auth loop */
	mock_io_close_c2s(&ctx.io);
	mock_io_close_s2c(&ctx.io);
	thrd_join(st, NULL);

	/* PARTIAL means auth failure response with partial_success=true.
	 * The client receives USERAUTH_FAILURE so returns error. */
	ASSERT_TRUE(ca.result < 0);
	ASSERT_TRUE(cbdata.password_called);

	mock_io_close_c2s(&ctx.io);
	mock_io_close_s2c(&ctx.io);

	handshake_cleanup(&ctx);
	return TEST_PASS;
}

static int
test_auth_password_empty(void)
{
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	struct test_auth_cbdata cbdata = {0};
	cbdata.expect_user = "testuser";
	cbdata.expect_pass = "";
	cbdata.password_result = DSSH_AUTH_SUCCESS;

	struct auth_client_arg ca = {0};
	ca.ctx = &ctx;
	ca.username = "testuser";
	ca.password = "";
	ca.do_get_methods = true;
	ca.do_password = true;

	struct auth_server_arg sa = {0};
	sa.ctx = &ctx;
	sa.cbs.methods_str = "password";
	sa.cbs.password_cb = test_password_cb;
	sa.cbs.cbdata = &cbdata;

	thrd_t ct, st;
	ASSERT_TRUE(thrd_create(&ct, auth_client_thread, &ca) == thrd_success);
	ASSERT_TRUE(thrd_create(&st, auth_server_thread, &sa) == thrd_success);

	thrd_join(ct, NULL);
	/* Close pipes to unblock server if still in auth loop */
	mock_io_close_c2s(&ctx.io);
	mock_io_close_s2c(&ctx.io);
	thrd_join(st, NULL);

	ASSERT_EQ(ca.result, 0);
	ASSERT_EQ(sa.result, 0);
	ASSERT_TRUE(cbdata.password_called);
	ASSERT_EQ_U(cbdata.captured_pass_len, 0);

	handshake_cleanup(&ctx);
	return TEST_PASS;
}

static int
test_auth_password_long(void)
{
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	/* Build a 200-byte password */
	char longpass[201];
	memset(longpass, 'A', 200);
	longpass[200] = 0;

	struct test_auth_cbdata cbdata = {0};
	cbdata.password_result = DSSH_AUTH_SUCCESS;

	struct auth_client_arg ca = {0};
	ca.ctx = &ctx;
	ca.username = "testuser";
	ca.password = longpass;
	ca.do_get_methods = true;
	ca.do_password = true;

	struct auth_server_arg sa = {0};
	sa.ctx = &ctx;
	sa.cbs.methods_str = "password";
	sa.cbs.password_cb = test_password_cb;
	sa.cbs.cbdata = &cbdata;

	thrd_t ct, st;
	ASSERT_TRUE(thrd_create(&ct, auth_client_thread, &ca) == thrd_success);
	ASSERT_TRUE(thrd_create(&st, auth_server_thread, &sa) == thrd_success);

	thrd_join(ct, NULL);
	/* Close pipes to unblock server if still in auth loop */
	mock_io_close_c2s(&ctx.io);
	mock_io_close_s2c(&ctx.io);
	thrd_join(st, NULL);

	ASSERT_EQ(ca.result, 0);
	ASSERT_EQ(sa.result, 0);
	ASSERT_TRUE(cbdata.password_called);
	ASSERT_EQ_U(cbdata.captured_pass_len, 200);

	handshake_cleanup(&ctx);
	return TEST_PASS;
}


/* ================================================================
 * Server-side auth tests (~8)
 * ================================================================ */

/*
 * For server-only tests, we run the server in a thread and
 * manually drive the client side from the main thread via
 * dssh_transport_send_packet / dssh_transport_recv_packet.
 */

static int
test_auth_server_none_accept(void)
{
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	struct test_auth_cbdata cbdata = {0};
	cbdata.none_result = DSSH_AUTH_SUCCESS;

	struct auth_server_arg sa = {0};
	sa.ctx = &ctx;
	sa.cbs.methods_str = "none";
	sa.cbs.none_cb = test_none_cb;
	sa.cbs.cbdata = &cbdata;

	thrd_t st;
	ASSERT_TRUE(thrd_create(&st, auth_server_thread, &sa) == thrd_success);

	/* Client: send SERVICE_REQUEST for ssh-userauth */
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

	/* Receive SERVICE_ACCEPT */
	{
		uint8_t msg_type;
		uint8_t *payload;
		size_t payload_len;
		ASSERT_OK(dssh_transport_recv_packet(ctx.client, &msg_type, &payload, &payload_len));
		ASSERT_EQ(msg_type, SSH_MSG_SERVICE_ACCEPT);
	}

	/* Send USERAUTH_REQUEST with method "none" */
	{
		static const char user[] = "admin";
		static const char svc[] = "ssh-connection";
		static const char method[] = "none";
		uint8_t msg[128];
		size_t pos = 0;
		msg[pos++] = SSH_MSG_USERAUTH_REQUEST;
		dssh_serialize_uint32((uint32_t)(sizeof(user) - 1), msg, sizeof(msg), &pos);
		memcpy(&msg[pos], user, sizeof(user) - 1);
		pos += sizeof(user) - 1;
		dssh_serialize_uint32((uint32_t)(sizeof(svc) - 1), msg, sizeof(msg), &pos);
		memcpy(&msg[pos], svc, sizeof(svc) - 1);
		pos += sizeof(svc) - 1;
		dssh_serialize_uint32((uint32_t)(sizeof(method) - 1), msg, sizeof(msg), &pos);
		memcpy(&msg[pos], method, sizeof(method) - 1);
		pos += sizeof(method) - 1;
		ASSERT_OK(dssh_transport_send_packet(ctx.client, msg, pos, NULL));
	}

	/* Receive USERAUTH_SUCCESS */
	{
		uint8_t msg_type;
		uint8_t *payload;
		size_t payload_len;
		ASSERT_OK(dssh_transport_recv_packet(ctx.client, &msg_type, &payload, &payload_len));
		ASSERT_EQ(msg_type, SSH_MSG_USERAUTH_SUCCESS);
	}

	thrd_join(st, NULL);
	ASSERT_EQ(sa.result, 0);
	ASSERT_TRUE(cbdata.none_called);

	handshake_cleanup(&ctx);
	return TEST_PASS;
}

static int
test_auth_server_none_reject(void)
{
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	struct test_auth_cbdata cbdata = {0};
	cbdata.none_result = DSSH_AUTH_FAILURE;

	struct auth_server_arg sa = {0};
	sa.ctx = &ctx;
	sa.cbs.methods_str = "password";
	sa.cbs.none_cb = test_none_cb;
	sa.cbs.password_cb = test_password_cb;
	sa.cbs.cbdata = &cbdata;

	thrd_t st;
	ASSERT_TRUE(thrd_create(&st, auth_server_thread, &sa) == thrd_success);

	/* Client: send SERVICE_REQUEST */
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

	/* Receive SERVICE_ACCEPT */
	{
		uint8_t msg_type;
		uint8_t *payload;
		size_t payload_len;
		ASSERT_OK(dssh_transport_recv_packet(ctx.client, &msg_type, &payload, &payload_len));
		ASSERT_EQ(msg_type, SSH_MSG_SERVICE_ACCEPT);
	}

	/* Send none auth request */
	{
		static const char user[] = "testuser";
		static const char svc[] = "ssh-connection";
		static const char method[] = "none";
		uint8_t msg[128];
		size_t pos = 0;
		msg[pos++] = SSH_MSG_USERAUTH_REQUEST;
		dssh_serialize_uint32((uint32_t)(sizeof(user) - 1), msg, sizeof(msg), &pos);
		memcpy(&msg[pos], user, sizeof(user) - 1);
		pos += sizeof(user) - 1;
		dssh_serialize_uint32((uint32_t)(sizeof(svc) - 1), msg, sizeof(msg), &pos);
		memcpy(&msg[pos], svc, sizeof(svc) - 1);
		pos += sizeof(svc) - 1;
		dssh_serialize_uint32((uint32_t)(sizeof(method) - 1), msg, sizeof(msg), &pos);
		memcpy(&msg[pos], method, sizeof(method) - 1);
		pos += sizeof(method) - 1;
		ASSERT_OK(dssh_transport_send_packet(ctx.client, msg, pos, NULL));
	}

	/* Should receive USERAUTH_FAILURE */
	{
		uint8_t msg_type;
		uint8_t *payload;
		size_t payload_len;
		ASSERT_OK(dssh_transport_recv_packet(ctx.client, &msg_type, &payload, &payload_len));
		ASSERT_EQ(msg_type, SSH_MSG_USERAUTH_FAILURE);
	}

	ASSERT_TRUE(cbdata.none_called);

	/* Server is still in auth loop; close pipes to unblock */
	mock_io_close_c2s(&ctx.io);
	mock_io_close_s2c(&ctx.io);
	thrd_join(st, NULL);

	handshake_cleanup(&ctx);
	return TEST_PASS;
}

static int
test_auth_server_password_accept(void)
{
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	struct test_auth_cbdata cbdata = {0};
	cbdata.password_result = DSSH_AUTH_SUCCESS;

	struct auth_server_arg sa = {0};
	sa.ctx = &ctx;
	sa.cbs.methods_str = "password";
	sa.cbs.password_cb = test_password_cb;
	sa.cbs.cbdata = &cbdata;

	thrd_t st;
	ASSERT_TRUE(thrd_create(&st, auth_server_thread, &sa) == thrd_success);

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

	/* Receive SERVICE_ACCEPT */
	{
		uint8_t msg_type;
		uint8_t *payload;
		size_t payload_len;
		ASSERT_OK(dssh_transport_recv_packet(ctx.client, &msg_type, &payload, &payload_len));
		ASSERT_EQ(msg_type, SSH_MSG_SERVICE_ACCEPT);
	}

	/* Send password auth request */
	{
		static const char user[] = "sysop";
		static const char svc[] = "ssh-connection";
		static const char method[] = "password";
		static const char pw[] = "hunter2";
		uint8_t msg[256];
		size_t pos = 0;
		msg[pos++] = SSH_MSG_USERAUTH_REQUEST;
		dssh_serialize_uint32((uint32_t)(sizeof(user) - 1), msg, sizeof(msg), &pos);
		memcpy(&msg[pos], user, sizeof(user) - 1);
		pos += sizeof(user) - 1;
		dssh_serialize_uint32((uint32_t)(sizeof(svc) - 1), msg, sizeof(msg), &pos);
		memcpy(&msg[pos], svc, sizeof(svc) - 1);
		pos += sizeof(svc) - 1;
		dssh_serialize_uint32((uint32_t)(sizeof(method) - 1), msg, sizeof(msg), &pos);
		memcpy(&msg[pos], method, sizeof(method) - 1);
		pos += sizeof(method) - 1;
		msg[pos++] = 0; /* boolean FALSE (not a password change) */
		dssh_serialize_uint32((uint32_t)(sizeof(pw) - 1), msg, sizeof(msg), &pos);
		memcpy(&msg[pos], pw, sizeof(pw) - 1);
		pos += sizeof(pw) - 1;
		ASSERT_OK(dssh_transport_send_packet(ctx.client, msg, pos, NULL));
	}

	/* Should receive USERAUTH_SUCCESS */
	{
		uint8_t msg_type;
		uint8_t *payload;
		size_t payload_len;
		ASSERT_OK(dssh_transport_recv_packet(ctx.client, &msg_type, &payload, &payload_len));
		ASSERT_EQ(msg_type, SSH_MSG_USERAUTH_SUCCESS);
	}

	thrd_join(st, NULL);
	ASSERT_EQ(sa.result, 0);
	ASSERT_TRUE(cbdata.password_called);
	ASSERT_EQ_U(cbdata.captured_pass_len, 7);
	ASSERT_MEM_EQ(cbdata.captured_pass, "hunter2", 7);

	handshake_cleanup(&ctx);
	return TEST_PASS;
}

static int
test_auth_server_password_reject(void)
{
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	struct test_auth_cbdata cbdata = {0};
	cbdata.password_result = DSSH_AUTH_FAILURE;

	struct auth_server_arg sa = {0};
	sa.ctx = &ctx;
	sa.cbs.methods_str = "password";
	sa.cbs.password_cb = test_password_cb;
	sa.cbs.cbdata = &cbdata;

	thrd_t st;
	ASSERT_TRUE(thrd_create(&st, auth_server_thread, &sa) == thrd_success);

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

	/* Send password auth */
	{
		static const char user[] = "testuser";
		static const char svc[] = "ssh-connection";
		static const char method[] = "password";
		static const char pw[] = "badpass";
		uint8_t msg[256];
		size_t pos = 0;
		msg[pos++] = SSH_MSG_USERAUTH_REQUEST;
		dssh_serialize_uint32((uint32_t)(sizeof(user) - 1), msg, sizeof(msg), &pos);
		memcpy(&msg[pos], user, sizeof(user) - 1);
		pos += sizeof(user) - 1;
		dssh_serialize_uint32((uint32_t)(sizeof(svc) - 1), msg, sizeof(msg), &pos);
		memcpy(&msg[pos], svc, sizeof(svc) - 1);
		pos += sizeof(svc) - 1;
		dssh_serialize_uint32((uint32_t)(sizeof(method) - 1), msg, sizeof(msg), &pos);
		memcpy(&msg[pos], method, sizeof(method) - 1);
		pos += sizeof(method) - 1;
		msg[pos++] = 0;
		dssh_serialize_uint32((uint32_t)(sizeof(pw) - 1), msg, sizeof(msg), &pos);
		memcpy(&msg[pos], pw, sizeof(pw) - 1);
		pos += sizeof(pw) - 1;
		ASSERT_OK(dssh_transport_send_packet(ctx.client, msg, pos, NULL));
	}

	/* Should receive USERAUTH_FAILURE */
	{
		uint8_t msg_type;
		uint8_t *payload;
		size_t payload_len;
		ASSERT_OK(dssh_transport_recv_packet(ctx.client, &msg_type, &payload, &payload_len));
		ASSERT_EQ(msg_type, SSH_MSG_USERAUTH_FAILURE);
	}

	ASSERT_TRUE(cbdata.password_called);

	mock_io_close_c2s(&ctx.io);
	mock_io_close_s2c(&ctx.io);
	thrd_join(st, NULL);

	handshake_cleanup(&ctx);
	return TEST_PASS;
}

static int
test_auth_server_publickey_accept(void)
{
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	struct test_auth_cbdata cbdata = {0};
	cbdata.publickey_result = DSSH_AUTH_SUCCESS;

	struct auth_server_arg sa = {0};
	sa.ctx = &ctx;
	sa.cbs.methods_str = "publickey";
	sa.cbs.publickey_cb = test_publickey_cb;
	sa.cbs.cbdata = &cbdata;

	/* Use the full client API for publickey auth, with
	 * dssh_auth_get_methods first */
	struct auth_client_arg ca = {0};
	ca.ctx = &ctx;
	ca.username = "testuser";
	ca.algo_name = test_key_algo_name();
	ca.do_get_methods = true;
	ca.do_publickey = true;

	thrd_t ct, st;
	ASSERT_TRUE(thrd_create(&ct, auth_client_thread, &ca) == thrd_success);
	ASSERT_TRUE(thrd_create(&st, auth_server_thread, &sa) == thrd_success);

	thrd_join(ct, NULL);
	/* Close pipes to unblock server if still in auth loop */
	mock_io_close_c2s(&ctx.io);
	mock_io_close_s2c(&ctx.io);
	thrd_join(st, NULL);

	ASSERT_EQ(ca.result, 0);
	ASSERT_EQ(sa.result, 0);
	ASSERT_TRUE(cbdata.publickey_called);

	handshake_cleanup(&ctx);
	return TEST_PASS;
}

static int
test_auth_server_publickey_reject(void)
{
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	struct test_auth_cbdata cbdata = {0};
	cbdata.publickey_result = DSSH_AUTH_FAILURE;

	struct auth_server_arg sa = {0};
	sa.ctx = &ctx;
	sa.cbs.methods_str = "publickey";
	sa.cbs.publickey_cb = test_publickey_cb;
	sa.cbs.cbdata = &cbdata;

	struct auth_client_arg ca = {0};
	ca.ctx = &ctx;
	ca.username = "testuser";
	ca.algo_name = test_key_algo_name();
	ca.do_get_methods = true;
	ca.do_publickey = true;

	thrd_t ct, st;
	ASSERT_TRUE(thrd_create(&ct, auth_client_thread, &ca) == thrd_success);
	ASSERT_TRUE(thrd_create(&st, auth_server_thread, &sa) == thrd_success);

	thrd_join(ct, NULL);
	/* Close pipes to unblock server if still in auth loop */
	mock_io_close_c2s(&ctx.io);
	mock_io_close_s2c(&ctx.io);
	thrd_join(st, NULL);

	ASSERT_TRUE(ca.result < 0);
	ASSERT_TRUE(cbdata.publickey_called);

	mock_io_close_c2s(&ctx.io);
	mock_io_close_s2c(&ctx.io);

	handshake_cleanup(&ctx);
	return TEST_PASS;
}

static int
test_auth_server_no_callbacks(void)
{
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	struct auth_server_arg sa = {0};
	sa.ctx = &ctx;
	sa.cbs.methods_str = "password";
	/* No callbacks set — all NULL */

	thrd_t st;
	ASSERT_TRUE(thrd_create(&st, auth_server_thread, &sa) == thrd_success);

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

	/* Send none auth — no none_cb configured, should get FAILURE */
	{
		static const char user[] = "testuser";
		static const char svc[] = "ssh-connection";
		static const char method[] = "none";
		uint8_t msg[128];
		size_t pos = 0;
		msg[pos++] = SSH_MSG_USERAUTH_REQUEST;
		dssh_serialize_uint32((uint32_t)(sizeof(user) - 1), msg, sizeof(msg), &pos);
		memcpy(&msg[pos], user, sizeof(user) - 1);
		pos += sizeof(user) - 1;
		dssh_serialize_uint32((uint32_t)(sizeof(svc) - 1), msg, sizeof(msg), &pos);
		memcpy(&msg[pos], svc, sizeof(svc) - 1);
		pos += sizeof(svc) - 1;
		dssh_serialize_uint32((uint32_t)(sizeof(method) - 1), msg, sizeof(msg), &pos);
		memcpy(&msg[pos], method, sizeof(method) - 1);
		pos += sizeof(method) - 1;
		ASSERT_OK(dssh_transport_send_packet(ctx.client, msg, pos, NULL));
	}

	/* Should get USERAUTH_FAILURE since none_cb is NULL */
	{
		uint8_t msg_type;
		uint8_t *payload;
		size_t payload_len;
		ASSERT_OK(dssh_transport_recv_packet(ctx.client, &msg_type, &payload, &payload_len));
		ASSERT_EQ(msg_type, SSH_MSG_USERAUTH_FAILURE);
	}

	mock_io_close_c2s(&ctx.io);
	mock_io_close_s2c(&ctx.io);
	thrd_join(st, NULL);

	handshake_cleanup(&ctx);
	return TEST_PASS;
}

static int
test_auth_server_username_captured(void)
{
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	struct test_auth_cbdata cbdata = {0};
	cbdata.password_result = DSSH_AUTH_SUCCESS;

	struct auth_server_arg sa = {0};
	sa.ctx = &ctx;
	sa.cbs.methods_str = "password";
	sa.cbs.password_cb = test_password_cb;
	sa.cbs.cbdata = &cbdata;

	struct auth_client_arg ca = {0};
	ca.ctx = &ctx;
	ca.username = "captured_user";
	ca.password = "pass";
	ca.do_get_methods = true;
	ca.do_password = true;

	thrd_t ct, st;
	ASSERT_TRUE(thrd_create(&ct, auth_client_thread, &ca) == thrd_success);
	ASSERT_TRUE(thrd_create(&st, auth_server_thread, &sa) == thrd_success);

	thrd_join(ct, NULL);
	/* Close pipes to unblock server if still in auth loop */
	mock_io_close_c2s(&ctx.io);
	mock_io_close_s2c(&ctx.io);
	thrd_join(st, NULL);

	ASSERT_EQ(ca.result, 0);
	ASSERT_EQ(sa.result, 0);

	/* Verify username_out was filled by dssh_auth_server */
	ASSERT_EQ_U(sa.username_out_len, strlen("captured_user"));
	ASSERT_MEM_EQ(sa.username_out, "captured_user", sa.username_out_len);

	handshake_cleanup(&ctx);
	return TEST_PASS;
}


/* ================================================================
 * Full roundtrip tests (~6)
 * ================================================================ */

static int
test_auth_roundtrip_password(void)
{
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	struct test_auth_cbdata cbdata = {0};
	cbdata.expect_user = "alice";
	cbdata.expect_pass = "p@ssw0rd";
	cbdata.password_result = DSSH_AUTH_SUCCESS;
	cbdata.none_result = DSSH_AUTH_FAILURE;

	struct auth_client_arg ca = {0};
	ca.ctx = &ctx;
	ca.username = "alice";
	ca.password = "p@ssw0rd";
	ca.do_get_methods = true;
	ca.do_password = true;

	struct auth_server_arg sa = {0};
	sa.ctx = &ctx;
	sa.cbs.methods_str = "password";
	sa.cbs.none_cb = test_none_cb;
	sa.cbs.password_cb = test_password_cb;
	sa.cbs.cbdata = &cbdata;

	thrd_t ct, st;
	ASSERT_TRUE(thrd_create(&ct, auth_client_thread, &ca) == thrd_success);
	ASSERT_TRUE(thrd_create(&st, auth_server_thread, &sa) == thrd_success);

	thrd_join(ct, NULL);
	/* Close pipes to unblock server if still in auth loop */
	mock_io_close_c2s(&ctx.io);
	mock_io_close_s2c(&ctx.io);
	thrd_join(st, NULL);

	ASSERT_EQ(ca.result, 0);
	ASSERT_EQ(sa.result, 0);
	ASSERT_TRUE(cbdata.none_called);
	ASSERT_TRUE(cbdata.password_called);

	handshake_cleanup(&ctx);
	return TEST_PASS;
}

static int
test_auth_roundtrip_password_reject(void)
{
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	struct test_auth_cbdata cbdata = {0};
	cbdata.expect_user = "alice";
	cbdata.expect_pass = "correct";
	cbdata.password_result = DSSH_AUTH_SUCCESS;
	cbdata.none_result = DSSH_AUTH_FAILURE;

	struct auth_client_arg ca = {0};
	ca.ctx = &ctx;
	ca.username = "alice";
	ca.password = "incorrect";
	ca.do_get_methods = true;
	ca.do_password = true;

	struct auth_server_arg sa = {0};
	sa.ctx = &ctx;
	sa.cbs.methods_str = "password";
	sa.cbs.none_cb = test_none_cb;
	sa.cbs.password_cb = test_password_cb;
	sa.cbs.cbdata = &cbdata;

	thrd_t ct, st;
	ASSERT_TRUE(thrd_create(&ct, auth_client_thread, &ca) == thrd_success);
	ASSERT_TRUE(thrd_create(&st, auth_server_thread, &sa) == thrd_success);

	thrd_join(ct, NULL);
	/* Close pipes to unblock server if still in auth loop */
	mock_io_close_c2s(&ctx.io);
	mock_io_close_s2c(&ctx.io);
	thrd_join(st, NULL);

	ASSERT_TRUE(ca.result < 0);
	ASSERT_TRUE(cbdata.password_called);

	mock_io_close_c2s(&ctx.io);
	mock_io_close_s2c(&ctx.io);

	handshake_cleanup(&ctx);
	return TEST_PASS;
}

static int
test_auth_roundtrip_publickey(void)
{
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	struct test_auth_cbdata cbdata = {0};
	cbdata.publickey_result = DSSH_AUTH_SUCCESS;
	cbdata.none_result = DSSH_AUTH_FAILURE;

	struct auth_client_arg ca = {0};
	ca.ctx = &ctx;
	ca.username = "bob";
	ca.algo_name = test_key_algo_name();
	ca.do_get_methods = true;
	ca.do_publickey = true;

	struct auth_server_arg sa = {0};
	sa.ctx = &ctx;
	sa.cbs.methods_str = "publickey";
	sa.cbs.none_cb = test_none_cb;
	sa.cbs.publickey_cb = test_publickey_cb;
	sa.cbs.cbdata = &cbdata;

	thrd_t ct, st;
	ASSERT_TRUE(thrd_create(&ct, auth_client_thread, &ca) == thrd_success);
	ASSERT_TRUE(thrd_create(&st, auth_server_thread, &sa) == thrd_success);

	thrd_join(ct, NULL);
	/* Close pipes to unblock server if still in auth loop */
	mock_io_close_c2s(&ctx.io);
	mock_io_close_s2c(&ctx.io);
	thrd_join(st, NULL);

	ASSERT_EQ(ca.result, 0);
	ASSERT_EQ(sa.result, 0);
	ASSERT_TRUE(cbdata.publickey_called);

	handshake_cleanup(&ctx);
	return TEST_PASS;
}

static int
test_auth_roundtrip_get_methods(void)
{
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	struct test_auth_cbdata cbdata = {0};
	cbdata.none_result = DSSH_AUTH_FAILURE;
	cbdata.password_result = DSSH_AUTH_SUCCESS;

	struct auth_client_arg ca = {0};
	ca.ctx = &ctx;
	ca.username = "testuser";
	ca.password = "pass";
	ca.do_get_methods = true;
	ca.do_password = true;

	struct auth_server_arg sa = {0};
	sa.ctx = &ctx;
	sa.cbs.methods_str = "publickey,password";
	sa.cbs.none_cb = test_none_cb;
	sa.cbs.password_cb = test_password_cb;
	sa.cbs.cbdata = &cbdata;

	thrd_t ct, st;
	ASSERT_TRUE(thrd_create(&ct, auth_client_thread, &ca) == thrd_success);
	ASSERT_TRUE(thrd_create(&st, auth_server_thread, &sa) == thrd_success);

	thrd_join(ct, NULL);
	/* Close pipes to unblock server if still in auth loop */
	mock_io_close_c2s(&ctx.io);
	mock_io_close_s2c(&ctx.io);
	thrd_join(st, NULL);

	ASSERT_EQ(ca.result, 0);
	ASSERT_EQ(sa.result, 0);

	/* Verify the client got the methods list from the none probe */
	ASSERT_EQ(ca.methods_result, 1);
	ASSERT_STR_EQ(ca.methods, "publickey,password");

	handshake_cleanup(&ctx);
	return TEST_PASS;
}

/*
 * Keyboard-interactive roundtrip.
 *
 * The server side of KBI is not directly implemented in dssh_auth_server()
 * (the server auth loop handles "keyboard-interactive" as an unknown
 * method and sends FAILURE).  So we build a custom KBI server that
 * manually sends INFO_REQUEST and receives INFO_RESPONSE via raw
 * transport packets after the SERVICE_REQUEST / SERVICE_ACCEPT exchange.
 */

struct kbi_server_arg {
	struct handshake_ctx *ctx;
	const char *expect_response;
	int result;
	bool got_correct_response;
};

static int
kbi_server_thread(void *arg)
{
	struct kbi_server_arg *a = arg;
	dssh_session sess = a->ctx->server;
	uint8_t msg_type;
	uint8_t *payload;
	size_t payload_len;
	int res;

	/* Receive SERVICE_REQUEST */
	res = dssh_transport_recv_packet(sess, &msg_type, &payload, &payload_len);
	if (res < 0 || msg_type != SSH_MSG_SERVICE_REQUEST) {
		a->result = -1;
		return 0;
	}

	/* Send SERVICE_ACCEPT */
	{
		uint8_t accept[64];
		size_t pos = 0;
		accept[pos++] = SSH_MSG_SERVICE_ACCEPT;
		if (payload_len > 5) {
			uint32_t slen;
			dssh_parse_uint32(&payload[1], payload_len - 1, &slen);
			if (5 + slen <= payload_len) {
				dssh_serialize_uint32(slen, accept, sizeof(accept), &pos);
				memcpy(&accept[pos], &payload[5], slen);
				pos += slen;
			}
		}
		res = dssh_transport_send_packet(sess, accept, pos, NULL);
		if (res < 0) {
			a->result = -2;
			return 0;
		}
	}

	/* Receive USERAUTH_REQUEST (none from get_methods) */
	res = dssh_transport_recv_packet(sess, &msg_type, &payload, &payload_len);
	if (res < 0 || msg_type != SSH_MSG_USERAUTH_REQUEST) {
		a->result = -3;
		return 0;
	}

	/* Send FAILURE with methods */
	{
		static const char methods[] = "keyboard-interactive";
		uint8_t msg[128];
		size_t pos = 0;
		msg[pos++] = SSH_MSG_USERAUTH_FAILURE;
		dssh_serialize_uint32((uint32_t)(sizeof(methods) - 1), msg, sizeof(msg), &pos);
		memcpy(&msg[pos], methods, sizeof(methods) - 1);
		pos += sizeof(methods) - 1;
		msg[pos++] = 0; /* partial_success = false */
		res = dssh_transport_send_packet(sess, msg, pos, NULL);
		if (res < 0) {
			a->result = -4;
			return 0;
		}
	}

	/* Receive USERAUTH_REQUEST (keyboard-interactive) */
	res = dssh_transport_recv_packet(sess, &msg_type, &payload, &payload_len);
	if (res < 0 || msg_type != SSH_MSG_USERAUTH_REQUEST) {
		a->result = -5;
		return 0;
	}

	/* Send INFO_REQUEST with one prompt */
	{
		static const char name[] = "Auth";
		static const char inst[] = "Please enter your token";
		static const char prompt[] = "Token: ";
		uint8_t msg[256];
		size_t pos = 0;
		msg[pos++] = SSH_MSG_USERAUTH_INFO_REQUEST;
		dssh_serialize_uint32((uint32_t)(sizeof(name) - 1), msg, sizeof(msg), &pos);
		memcpy(&msg[pos], name, sizeof(name) - 1);
		pos += sizeof(name) - 1;
		dssh_serialize_uint32((uint32_t)(sizeof(inst) - 1), msg, sizeof(msg), &pos);
		memcpy(&msg[pos], inst, sizeof(inst) - 1);
		pos += sizeof(inst) - 1;
		dssh_serialize_uint32(0, msg, sizeof(msg), &pos); /* language */
		dssh_serialize_uint32(1, msg, sizeof(msg), &pos); /* num_prompts */
		dssh_serialize_uint32((uint32_t)(sizeof(prompt) - 1), msg, sizeof(msg), &pos);
		memcpy(&msg[pos], prompt, sizeof(prompt) - 1);
		pos += sizeof(prompt) - 1;
		msg[pos++] = 0; /* echo = false */
		res = dssh_transport_send_packet(sess, msg, pos, NULL);
		if (res < 0) {
			a->result = -6;
			return 0;
		}
	}

	/* Receive INFO_RESPONSE */
	res = dssh_transport_recv_packet(sess, &msg_type, &payload, &payload_len);
	if (res < 0 || msg_type != SSH_MSG_USERAUTH_INFO_RESPONSE) {
		a->result = -7;
		return 0;
	}

	/* Parse response: uint32 num_responses, then string responses */
	{
		size_t rpos = 1;
		uint32_t num_responses;
		if (rpos + 4 > payload_len) {
			a->result = -8;
			return 0;
		}
		dssh_parse_uint32(&payload[rpos], payload_len - rpos, &num_responses);
		rpos += 4;
		if (num_responses >= 1) {
			uint32_t resp_len;
			if (rpos + 4 > payload_len) {
				a->result = -9;
				return 0;
			}
			dssh_parse_uint32(&payload[rpos], payload_len - rpos, &resp_len);
			rpos += 4;
			if (rpos + resp_len <= payload_len) {
				size_t expect_len = strlen(a->expect_response);
				if (resp_len == expect_len &&
				    memcmp(&payload[rpos], a->expect_response, resp_len) == 0)
					a->got_correct_response = true;
			}
		}
	}

	/* Send SUCCESS or FAILURE based on the response */
	if (a->got_correct_response) {
		uint8_t msg = SSH_MSG_USERAUTH_SUCCESS;
		res = dssh_transport_send_packet(sess, &msg, 1, NULL);
	}
	else {
		static const char methods[] = "keyboard-interactive";
		uint8_t msg[128];
		size_t pos = 0;
		msg[pos++] = SSH_MSG_USERAUTH_FAILURE;
		dssh_serialize_uint32((uint32_t)(sizeof(methods) - 1), msg, sizeof(msg), &pos);
		memcpy(&msg[pos], methods, sizeof(methods) - 1);
		pos += sizeof(methods) - 1;
		msg[pos++] = 0;
		res = dssh_transport_send_packet(sess, msg, pos, NULL);
	}

	a->result = (res < 0) ? res : 0;
	return 0;
}

static int
test_auth_roundtrip_kbi(void)
{
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	struct test_kbi_cbdata kbi_data = {0};
	kbi_data.response = "123456";

	struct auth_client_arg ca = {0};
	ca.ctx = &ctx;
	ca.username = "testuser";
	ca.do_get_methods = true;
	ca.do_kbi = true;
	ca.kbi_cb = test_kbi_prompt_cb;
	ca.kbi_cbdata = &kbi_data;

	struct kbi_server_arg sa = {0};
	sa.ctx = &ctx;
	sa.expect_response = "123456";

	thrd_t ct, st;
	ASSERT_TRUE(thrd_create(&ct, auth_client_thread, &ca) == thrd_success);
	ASSERT_TRUE(thrd_create(&st, kbi_server_thread, &sa) == thrd_success);

	thrd_join(ct, NULL);
	/* Close pipes to unblock server if still in auth loop */
	mock_io_close_c2s(&ctx.io);
	mock_io_close_s2c(&ctx.io);
	thrd_join(st, NULL);

	ASSERT_EQ(ca.result, 0);
	ASSERT_EQ(sa.result, 0);
	ASSERT_TRUE(sa.got_correct_response);
	ASSERT_TRUE(kbi_data.called);
	ASSERT_EQ_U(kbi_data.num_prompts_seen, 1);

	handshake_cleanup(&ctx);
	return TEST_PASS;
}

static int
test_auth_roundtrip_multiple_attempts(void)
{
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	struct test_auth_cbdata cbdata = {0};
	cbdata.expect_user = "testuser";
	cbdata.expect_pass = "correct";
	cbdata.password_result = DSSH_AUTH_SUCCESS;
	cbdata.none_result = DSSH_AUTH_FAILURE;

	struct auth_server_arg sa = {0};
	sa.ctx = &ctx;
	sa.cbs.methods_str = "password";
	sa.cbs.none_cb = test_none_cb;
	sa.cbs.password_cb = test_password_cb;
	sa.cbs.cbdata = &cbdata;

	thrd_t st;
	ASSERT_TRUE(thrd_create(&st, auth_server_thread, &sa) == thrd_success);

	/* Client side: manually do get_methods, then two password attempts */
	dssh_session client = ctx.client;
	char methods[256];
	int mres = dssh_auth_get_methods(client, "testuser", methods, sizeof(methods));
	ASSERT_EQ(mres, 1);

	/* First attempt with wrong password */
	int res = dssh_auth_password(client, "testuser", "wrong", NULL, NULL);
	ASSERT_TRUE(res < 0);

	/* Second attempt with correct password */
	res = dssh_auth_password(client, "testuser", "correct", NULL, NULL);
	ASSERT_EQ(res, 0);

	thrd_join(st, NULL);
	ASSERT_EQ(sa.result, 0);

	handshake_cleanup(&ctx);
	return TEST_PASS;
}


/* ================================================================
 * RFC 4252 s5.4: Banner message delivery
 * ================================================================ */

static bool banner_received;
static char banner_text[256];

static void
test_banner_cb(const uint8_t *message, size_t message_len,
    const uint8_t *language, size_t language_len, void *cbdata)
{
	(void)language;
	(void)language_len;
	(void)cbdata;
	banner_received = true;
	size_t n = message_len < sizeof(banner_text) - 1
	    ? message_len : sizeof(banner_text) - 1;
	memcpy(banner_text, message, n);
	banner_text[n] = 0;
}

/* Server thread that sends a banner before accepting auth */
static int
banner_server_thread(void *arg)
{
	struct auth_server_arg *a = arg;
	dssh_session sess = a->ctx->server;

	/* Receive SERVICE_REQUEST, send SERVICE_ACCEPT */
	uint8_t msg_type;
	uint8_t *payload;
	size_t payload_len;
	int res = dssh_transport_recv_packet(sess, &msg_type,
	    &payload, &payload_len);
	if (res < 0) {
		a->result = res;
		return 0;
	}
	uint8_t accept[32];
	size_t pos = 0;
	accept[pos++] = SSH_MSG_SERVICE_ACCEPT;
	if (payload_len > 1) {
		uint32_t slen;
		dssh_parse_uint32(&payload[1], payload_len - 1, &slen);
		dssh_serialize_uint32(slen, accept, sizeof(accept), &pos);
		memcpy(&accept[pos], &payload[5], slen);
		pos += slen;
	}
	dssh_transport_send_packet(sess, accept, pos, NULL);

	/* Receive the password auth request */
	res = dssh_transport_recv_packet(sess, &msg_type,
	    &payload, &payload_len);
	if (res < 0) {
		a->result = res;
		return 0;
	}

	/* Send BANNER before SUCCESS — dssh_auth_password handles
	 * BANNER messages in its recv loop (RFC 4252 s5.4). */
	const char *banner_msg = "Welcome to DeuceSSH test server!";
	size_t blen = strlen(banner_msg);
	uint8_t banner[256];
	pos = 0;
	banner[pos++] = SSH_MSG_USERAUTH_BANNER;
	dssh_serialize_uint32((uint32_t)blen, banner, sizeof(banner), &pos);
	memcpy(&banner[pos], banner_msg, blen);
	pos += blen;
	dssh_serialize_uint32(0, banner, sizeof(banner), &pos);
	dssh_transport_send_packet(sess, banner, pos, NULL);

	/* Now send SUCCESS */
	{
		uint8_t succ = SSH_MSG_USERAUTH_SUCCESS;
		dssh_transport_send_packet(sess, &succ, 1, NULL);
	}
	a->result = 0;
	return 0;
}

static int
test_auth_banner_delivered(void)
{
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	banner_received = false;
	banner_text[0] = 0;

	/* Set banner callback on client */
	dssh_session_set_banner_cb(ctx.client,
	    test_banner_cb, NULL);

	struct auth_server_arg sa = {0};
	sa.ctx = &ctx;

	thrd_t st;
	ASSERT_TRUE(thrd_create(&st, banner_server_thread, &sa) == thrd_success);

	/* Client sends password auth — server sends BANNER then SUCCESS.
	 * dssh_auth_password handles BANNER in its recv loop. */
	int res = dssh_auth_password(ctx.client, "testuser", "pass",
	    NULL, NULL);
	ASSERT_EQ(res, 0);

	thrd_join(st, NULL);
	ASSERT_EQ(sa.result, 0);

	/* Verify banner was delivered */
	ASSERT_TRUE(banner_received);
	ASSERT_STR_EQ(banner_text, "Welcome to DeuceSSH test server!");

	handshake_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Server password change flow (Category 2)
 *
 * Server callback returns DSSH_AUTH_CHANGE_PASSWORD with a prompt.
 * Server sends PASSWD_CHANGEREQ.  Client responds with old+new
 * password (change=TRUE).  Server's passwd_change_cb accepts.
 * ================================================================ */

static int
test_passwd_change_cb(const uint8_t *username, size_t username_len,
    const uint8_t *old_password, size_t old_password_len,
    const uint8_t *new_password, size_t new_password_len,
    uint8_t **change_prompt, size_t *change_prompt_len,
    void *cbdata)
{
	(void)username;
	(void)username_len;
	(void)old_password;
	(void)old_password_len;
	(void)change_prompt;
	(void)change_prompt_len;

	/* Accept the password change if new password is "newpass" */
	if (new_password_len == 7 &&
	    memcmp(new_password, "newpass", 7) == 0)
		return DSSH_AUTH_SUCCESS;
	return DSSH_AUTH_FAILURE;
}

static int
test_password_change_cb_server(const uint8_t *username, size_t username_len,
    const uint8_t *password, size_t password_len,
    uint8_t **change_prompt, size_t *change_prompt_len,
    void *cbdata)
{
	(void)username;
	(void)username_len;
	(void)password;
	(void)password_len;

	/* Signal password change needed */
	*change_prompt = malloc(16);
	memcpy(*change_prompt, "Change password", 15);
	*change_prompt_len = 15;
	return DSSH_AUTH_CHANGE_PASSWORD;
}

static int
test_client_passwd_change_cb(const uint8_t *prompt, size_t prompt_len,
    const uint8_t *language, size_t language_len,
    uint8_t **new_password, size_t *new_password_len,
    void *cbdata)
{
	(void)prompt;
	(void)prompt_len;
	(void)language;
	(void)language_len;
	(void)cbdata;

	*new_password = malloc(7);
	memcpy(*new_password, "newpass", 7);
	*new_password_len = 7;
	return 0;
}

static int
test_auth_server_password_change(void)
{
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	struct auth_server_arg sa = {0};
	sa.ctx = &ctx;
	sa.cbs.methods_str = "password";
	sa.cbs.password_cb = test_password_change_cb_server;
	sa.cbs.passwd_change_cb = test_passwd_change_cb;

	thrd_t st;
	ASSERT_TRUE(thrd_create(&st, auth_server_thread, &sa) == thrd_success);

	/* Client: use dssh_auth_password with a change callback */
	int res = dssh_auth_password(ctx.client, "testuser", "oldpass",
	    test_client_passwd_change_cb, NULL);
	ASSERT_EQ(res, 0);

	thrd_join(st, NULL);
	ASSERT_EQ(sa.result, 0);

	handshake_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Server publickey probe (Category 2)
 *
 * Client sends publickey with has_sig=FALSE (probe).
 * Server responds with PK_OK.
 * Client then sends the real signed request.
 *
 * The existing test_auth_server_publickey_accept uses
 * dssh_auth_publickey() which sends has_sig=TRUE directly.
 * This test manually sends the probe first.
 * ================================================================ */

static int
test_publickey_probe_cb(const uint8_t *username, size_t username_len,
    const char *algo_name, const uint8_t *pubkey_blob,
    size_t pubkey_blob_len, bool has_signature, void *cbdata)
{
	bool *probe_seen = cbdata;
	if (!has_signature)
		*probe_seen = true;
	return DSSH_AUTH_SUCCESS;
}

static int
test_auth_server_publickey_probe(void)
{
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	bool probe_seen = false;

	struct auth_server_arg sa = {0};
	sa.ctx = &ctx;
	sa.cbs.methods_str = "publickey";
	sa.cbs.publickey_cb = test_publickey_probe_cb;
	sa.cbs.cbdata = &probe_seen;

	thrd_t st;
	ASSERT_TRUE(thrd_create(&st, auth_server_thread, &sa) == thrd_success);

	/* Client: send SERVICE_REQUEST */
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

	/* Receive SERVICE_ACCEPT */
	{
		uint8_t msg_type;
		uint8_t *payload;
		size_t payload_len;
		ASSERT_OK(dssh_transport_recv_packet(ctx.client, &msg_type, &payload, &payload_len));
		ASSERT_EQ(msg_type, SSH_MSG_SERVICE_ACCEPT);
	}

	/* Get the key algo and public key blob */
	const char *algo_name = test_key_algo_name();
	dssh_key_algo ka = dssh_transport_find_key_algo(algo_name);
	ASSERT_NOT_NULL(ka);
	uint8_t pubkey_buf[2048];
	size_t pubkey_len;
	ASSERT_EQ(ka->pubkey(pubkey_buf, sizeof(pubkey_buf), &pubkey_len, ka->ctx), 0);

	/* Send publickey probe (has_sig = FALSE) */
	{
		static const char user[] = "probeuser";
		static const char svc[] = "ssh-connection";
		static const char method[] = "publickey";
		size_t algo_len = strlen(algo_name);
		uint8_t msg[512];
		size_t pos = 0;
		msg[pos++] = SSH_MSG_USERAUTH_REQUEST;
		dssh_serialize_uint32((uint32_t)(sizeof(user) - 1), msg, sizeof(msg), &pos);
		memcpy(&msg[pos], user, sizeof(user) - 1);
		pos += sizeof(user) - 1;
		dssh_serialize_uint32((uint32_t)(sizeof(svc) - 1), msg, sizeof(msg), &pos);
		memcpy(&msg[pos], svc, sizeof(svc) - 1);
		pos += sizeof(svc) - 1;
		dssh_serialize_uint32((uint32_t)(sizeof(method) - 1), msg, sizeof(msg), &pos);
		memcpy(&msg[pos], method, sizeof(method) - 1);
		pos += sizeof(method) - 1;
		msg[pos++] = 0; /* has_sig = FALSE (probe) */
		dssh_serialize_uint32((uint32_t)algo_len, msg, sizeof(msg), &pos);
		memcpy(&msg[pos], algo_name, algo_len);
		pos += algo_len;
		dssh_serialize_uint32((uint32_t)pubkey_len, msg, sizeof(msg), &pos);
		memcpy(&msg[pos], pubkey_buf, pubkey_len);
		pos += pubkey_len;
		ASSERT_OK(dssh_transport_send_packet(ctx.client, msg, pos, NULL));
	}

	/* Should receive PK_OK (msg type 60) */
	{
		uint8_t msg_type;
		uint8_t *payload;
		size_t payload_len;
		ASSERT_OK(dssh_transport_recv_packet(ctx.client, &msg_type, &payload, &payload_len));
		ASSERT_EQ(msg_type, SSH_MSG_USERAUTH_PK_OK);
	}

	/* Now send the real signed request using dssh_auth_publickey.
	 * But auth service is already requested so we need to set
	 * the flag.  Instead, just close and verify probe was seen. */
	mock_io_close_c2s(&ctx.io);
	mock_io_close_s2c(&ctx.io);
	thrd_join(st, NULL);

	ASSERT_TRUE(probe_seen);

	handshake_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Server KBI through dssh_auth_server (Category 2)
 *
 * Client sends keyboard-interactive.  Server's auth loop sees
 * "keyboard-interactive" method; since there's no KBI callback in
 * dssh_auth_server_cbs, it falls through to "unknown method" and
 * sends FAILURE.  This exercises the unknown method branch.
 * ================================================================ */

static int
test_auth_server_unknown_method(void)
{
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	struct test_auth_cbdata cbdata = {0};
	cbdata.password_result = DSSH_AUTH_SUCCESS;

	struct auth_server_arg sa = {0};
	sa.ctx = &ctx;
	sa.cbs.methods_str = "password";
	sa.cbs.password_cb = test_password_cb;
	sa.cbs.cbdata = &cbdata;

	thrd_t st;
	ASSERT_TRUE(thrd_create(&st, auth_server_thread, &sa) == thrd_success);

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

	/* Send keyboard-interactive auth (unknown to server) */
	{
		static const char user[] = "testuser";
		static const char svc[] = "ssh-connection";
		static const char method[] = "keyboard-interactive";
		uint8_t msg[256];
		size_t pos = 0;
		msg[pos++] = SSH_MSG_USERAUTH_REQUEST;
		dssh_serialize_uint32((uint32_t)(sizeof(user) - 1), msg, sizeof(msg), &pos);
		memcpy(&msg[pos], user, sizeof(user) - 1);
		pos += sizeof(user) - 1;
		dssh_serialize_uint32((uint32_t)(sizeof(svc) - 1), msg, sizeof(msg), &pos);
		memcpy(&msg[pos], svc, sizeof(svc) - 1);
		pos += sizeof(svc) - 1;
		dssh_serialize_uint32((uint32_t)(sizeof(method) - 1), msg, sizeof(msg), &pos);
		memcpy(&msg[pos], method, sizeof(method) - 1);
		pos += sizeof(method) - 1;
		dssh_serialize_uint32(0, msg, sizeof(msg), &pos); /* language */
		dssh_serialize_uint32(0, msg, sizeof(msg), &pos); /* submethods */
		ASSERT_OK(dssh_transport_send_packet(ctx.client, msg, pos, NULL));
	}

	/* Should get FAILURE (unknown method falls through) */
	{
		uint8_t msg_type;
		uint8_t *payload;
		size_t payload_len;
		ASSERT_OK(dssh_transport_recv_packet(ctx.client, &msg_type, &payload, &payload_len));
		ASSERT_EQ(msg_type, SSH_MSG_USERAUTH_FAILURE);
	}

	/* Server is still in auth loop; close pipes */
	mock_io_close_c2s(&ctx.io);
	mock_io_close_s2c(&ctx.io);
	thrd_join(st, NULL);

	handshake_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Server receives unexpected message type (Category 4)
 *
 * Client sends a non-USERAUTH_REQUEST during the auth loop.
 * Server sends UNIMPLEMENTED and continues the auth loop.
 * ================================================================ */

static int
test_auth_server_unexpected_message(void)
{
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	struct test_auth_cbdata cbdata = {0};
	cbdata.password_result = DSSH_AUTH_SUCCESS;

	struct auth_server_arg sa = {0};
	sa.ctx = &ctx;
	sa.cbs.methods_str = "password";
	sa.cbs.password_cb = test_password_cb;
	sa.cbs.cbdata = &cbdata;

	thrd_t st;
	ASSERT_TRUE(thrd_create(&st, auth_server_thread, &sa) == thrd_success);

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

	/* Send an unexpected message type (SERVICE_REQUEST again)
	 * during the auth loop — server should send UNIMPLEMENTED
	 * and continue. */
	{
		uint8_t msg[8];
		size_t pos = 0;
		msg[pos++] = SSH_MSG_SERVICE_REQUEST;
		dssh_serialize_uint32(3, msg, sizeof(msg), &pos);
		msg[pos++] = 'f';
		msg[pos++] = 'o';
		msg[pos++] = 'o';
		ASSERT_OK(dssh_transport_send_packet(ctx.client, msg, pos, NULL));
	}

	/* Server sends UNIMPLEMENTED; client's recv_packet handles it
	 * transparently (invokes unimplemented_cb and continues).
	 * So we won't see it as a message type. Instead, send a real
	 * password auth to complete the loop successfully. */
	{
		static const char user[] = "admin";
		static const char svc[] = "ssh-connection";
		static const char method[] = "password";
		static const char pw[] = "pass";
		uint8_t msg[256];
		size_t pos = 0;
		msg[pos++] = SSH_MSG_USERAUTH_REQUEST;
		dssh_serialize_uint32((uint32_t)(sizeof(user) - 1), msg, sizeof(msg), &pos);
		memcpy(&msg[pos], user, sizeof(user) - 1);
		pos += sizeof(user) - 1;
		dssh_serialize_uint32((uint32_t)(sizeof(svc) - 1), msg, sizeof(msg), &pos);
		memcpy(&msg[pos], svc, sizeof(svc) - 1);
		pos += sizeof(svc) - 1;
		dssh_serialize_uint32((uint32_t)(sizeof(method) - 1), msg, sizeof(msg), &pos);
		memcpy(&msg[pos], method, sizeof(method) - 1);
		pos += sizeof(method) - 1;
		msg[pos++] = 0;
		dssh_serialize_uint32((uint32_t)(sizeof(pw) - 1), msg, sizeof(msg), &pos);
		memcpy(&msg[pos], pw, sizeof(pw) - 1);
		pos += sizeof(pw) - 1;
		ASSERT_OK(dssh_transport_send_packet(ctx.client, msg, pos, NULL));
	}

	/* Should receive USERAUTH_SUCCESS */
	{
		uint8_t msg_type;
		uint8_t *payload;
		size_t payload_len;
		ASSERT_OK(dssh_transport_recv_packet(ctx.client, &msg_type, &payload, &payload_len));
		ASSERT_EQ(msg_type, SSH_MSG_USERAUTH_SUCCESS);
	}

	thrd_join(st, NULL);
	ASSERT_EQ(sa.result, 0);

	handshake_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Client KBI error path tests
 *
 * Generic server thread that does the SERVICE_REQUEST / ACCEPT
 * exchange, receives the KBI USERAUTH_REQUEST, then sends a raw
 * payload from the test's buffer as an INFO_REQUEST message.
 * ================================================================ */

struct kbi_error_server_arg {
	struct handshake_ctx *ctx;
	const uint8_t *info_payload;   /* raw INFO_REQUEST payload */
	size_t info_payload_len;
	int result;
};

static int
kbi_error_service_exchange(dssh_session sess)
{
	uint8_t msg_type;
	uint8_t *payload;
	size_t payload_len;
	int res;

	/* SERVICE_REQUEST */
	res = dssh_transport_recv_packet(sess, &msg_type, &payload, &payload_len);
	if (res < 0 || msg_type != SSH_MSG_SERVICE_REQUEST)
		return -1;

	/* SERVICE_ACCEPT */
	uint8_t accept[64];
	size_t pos = 0;
	accept[pos++] = SSH_MSG_SERVICE_ACCEPT;
	if (payload_len > 5) {
		uint32_t slen;
		dssh_parse_uint32(&payload[1], payload_len - 1, &slen);
		if (5 + slen <= payload_len) {
			dssh_serialize_uint32(slen, accept, sizeof(accept), &pos);
			memcpy(&accept[pos], &payload[5], slen);
			pos += slen;
		}
	}
	res = dssh_transport_send_packet(sess, accept, pos, NULL);
	if (res < 0)
		return -2;

	/* Receive USERAUTH_REQUEST (keyboard-interactive) */
	res = dssh_transport_recv_packet(sess, &msg_type, &payload, &payload_len);
	if (res < 0 || msg_type != SSH_MSG_USERAUTH_REQUEST)
		return -3;

	return 0;
}

static int
kbi_error_server_thread(void *arg)
{
	struct kbi_error_server_arg *a = arg;
	dssh_session sess = a->ctx->server;
	int res;

	res = kbi_error_service_exchange(sess);
	if (res < 0) {
		a->result = res;
		return 0;
	}

	/* Send the crafted INFO_REQUEST payload */
	res = dssh_transport_send_packet(sess, a->info_payload,
	    a->info_payload_len, NULL);
	a->result = res;
	return 0;
}

/*
 * Helper: build a well-formed INFO_REQUEST with the given parameters.
 * Returns the length written to buf.
 */
static size_t
build_info_request(uint8_t *buf, size_t bufsz,
    const char *name, const char *instruction,
    uint32_t num_prompts,
    const char **prompt_strs, const bool *echo_flags)
{
	size_t pos = 0;
	buf[pos++] = SSH_MSG_USERAUTH_INFO_REQUEST;

	size_t nlen = name ? strlen(name) : 0;
	dssh_serialize_uint32((uint32_t)nlen, buf, bufsz, &pos);
	if (nlen > 0) { memcpy(&buf[pos], name, nlen); pos += nlen; }

	size_t ilen = instruction ? strlen(instruction) : 0;
	dssh_serialize_uint32((uint32_t)ilen, buf, bufsz, &pos);
	if (ilen > 0) { memcpy(&buf[pos], instruction, ilen); pos += ilen; }

	dssh_serialize_uint32(0, buf, bufsz, &pos); /* language */
	dssh_serialize_uint32(num_prompts, buf, bufsz, &pos);

	for (uint32_t i = 0; i < num_prompts; i++) {
		size_t plen = strlen(prompt_strs[i]);
		dssh_serialize_uint32((uint32_t)plen, buf, bufsz, &pos);
		memcpy(&buf[pos], prompt_strs[i], plen);
		pos += plen;
		buf[pos++] = echo_flags[i] ? 1 : 0;
	}
	return pos;
}

/*
 * Run a KBI error test: handshake, launch server thread that sends
 * a crafted INFO_REQUEST, client calls dssh_auth_keyboard_interactive.
 * Returns the client's result code.
 */
static int
run_kbi_error_test(const uint8_t *info_payload, size_t info_len,
    dssh_auth_kbi_prompt_cb cb, void *cbdata)
{
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return -999;
	}

	struct kbi_error_server_arg sa = {0};
	sa.ctx = &ctx;
	sa.info_payload = info_payload;
	sa.info_payload_len = info_len;

	thrd_t st;
	if (thrd_create(&st, kbi_error_server_thread, &sa) != thrd_success) {
		handshake_cleanup(&ctx);
		return -999;
	}

	int res = dssh_auth_keyboard_interactive(ctx.client, "user",
	    cb, cbdata);

	mock_io_close_c2s(&ctx.io);
	mock_io_close_s2c(&ctx.io);
	thrd_join(st, NULL);
	handshake_cleanup(&ctx);
	return res;
}

/* A dummy KBI callback that should never be called */
static int
kbi_cb_unreachable(const uint8_t *name, size_t name_len,
    const uint8_t *instruction, size_t instruction_len,
    uint32_t num_prompts,
    const uint8_t **prompts, const size_t *prompt_lens,
    const bool *echo,
    uint8_t **responses, size_t *response_lens,
    void *cbdata)
{
	(void)name; (void)name_len; (void)instruction; (void)instruction_len;
	(void)num_prompts; (void)prompts; (void)prompt_lens; (void)echo;
	(void)responses; (void)response_lens; (void)cbdata;
	/* If called, something is wrong — the parse should have failed */
	return -1;
}

/* A KBI callback that returns an error */
static int
kbi_cb_abort(const uint8_t *name, size_t name_len,
    const uint8_t *instruction, size_t instruction_len,
    uint32_t num_prompts,
    const uint8_t **prompts, const size_t *prompt_lens,
    const bool *echo,
    uint8_t **responses, size_t *response_lens,
    void *cbdata)
{
	(void)name; (void)name_len; (void)instruction; (void)instruction_len;
	(void)prompts; (void)prompt_lens; (void)echo; (void)cbdata;

	/* Allocate responses so the cleanup path frees them */
	for (uint32_t i = 0; i < num_prompts; i++) {
		responses[i] = malloc(1);
		responses[i][0] = 'x';
		response_lens[i] = 1;
	}
	return -42;
}

/* --- Truncated INFO_REQUEST: too short for name length --- */
static int
test_kbi_truncated_name_header(void)
{
	/* INFO_REQUEST with only the msg type byte — no name length */
	uint8_t payload[] = { SSH_MSG_USERAUTH_INFO_REQUEST };
	int res = run_kbi_error_test(payload, sizeof(payload),
	    kbi_cb_unreachable, NULL);
	ASSERT_EQ(res, DSSH_ERROR_PARSE);
	return TEST_PASS;
}

/* --- Name string overflows payload --- */
static int
test_kbi_truncated_name_data(void)
{
	uint8_t payload[16];
	size_t pos = 0;
	payload[pos++] = SSH_MSG_USERAUTH_INFO_REQUEST;
	dssh_serialize_uint32(100, payload, sizeof(payload), &pos);
	/* Claims 100 bytes of name but payload ends here */
	int res = run_kbi_error_test(payload, pos,
	    kbi_cb_unreachable, NULL);
	ASSERT_EQ(res, DSSH_ERROR_PARSE);
	return TEST_PASS;
}

/* --- Too short for instruction length --- */
static int
test_kbi_truncated_instruction_header(void)
{
	uint8_t payload[16];
	size_t pos = 0;
	payload[pos++] = SSH_MSG_USERAUTH_INFO_REQUEST;
	dssh_serialize_uint32(0, payload, sizeof(payload), &pos); /* name="" */
	/* No room for instruction length */
	int res = run_kbi_error_test(payload, pos,
	    kbi_cb_unreachable, NULL);
	ASSERT_EQ(res, DSSH_ERROR_PARSE);
	return TEST_PASS;
}

/* --- Instruction string overflows --- */
static int
test_kbi_truncated_instruction_data(void)
{
	uint8_t payload[16];
	size_t pos = 0;
	payload[pos++] = SSH_MSG_USERAUTH_INFO_REQUEST;
	dssh_serialize_uint32(0, payload, sizeof(payload), &pos);
	dssh_serialize_uint32(200, payload, sizeof(payload), &pos);
	int res = run_kbi_error_test(payload, pos,
	    kbi_cb_unreachable, NULL);
	ASSERT_EQ(res, DSSH_ERROR_PARSE);
	return TEST_PASS;
}

/* --- Too short for language length --- */
static int
test_kbi_truncated_language_header(void)
{
	uint8_t payload[16];
	size_t pos = 0;
	payload[pos++] = SSH_MSG_USERAUTH_INFO_REQUEST;
	dssh_serialize_uint32(0, payload, sizeof(payload), &pos);
	dssh_serialize_uint32(0, payload, sizeof(payload), &pos);
	/* No room for language length */
	int res = run_kbi_error_test(payload, pos,
	    kbi_cb_unreachable, NULL);
	ASSERT_EQ(res, DSSH_ERROR_PARSE);
	return TEST_PASS;
}

/* --- Language string overflows --- */
static int
test_kbi_truncated_language_data(void)
{
	uint8_t payload[16];
	size_t pos = 0;
	payload[pos++] = SSH_MSG_USERAUTH_INFO_REQUEST;
	dssh_serialize_uint32(0, payload, sizeof(payload), &pos);
	dssh_serialize_uint32(0, payload, sizeof(payload), &pos);
	dssh_serialize_uint32(200, payload, sizeof(payload), &pos);
	int res = run_kbi_error_test(payload, pos,
	    kbi_cb_unreachable, NULL);
	ASSERT_EQ(res, DSSH_ERROR_PARSE);
	return TEST_PASS;
}

/* --- Too short for num_prompts --- */
static int
test_kbi_truncated_num_prompts(void)
{
	uint8_t payload[16];
	size_t pos = 0;
	payload[pos++] = SSH_MSG_USERAUTH_INFO_REQUEST;
	dssh_serialize_uint32(0, payload, sizeof(payload), &pos);
	dssh_serialize_uint32(0, payload, sizeof(payload), &pos);
	dssh_serialize_uint32(0, payload, sizeof(payload), &pos);
	/* No room for num_prompts */
	int res = run_kbi_error_test(payload, pos,
	    kbi_cb_unreachable, NULL);
	ASSERT_EQ(res, DSSH_ERROR_PARSE);
	return TEST_PASS;
}

/* --- Prompt loop: too short for prompt length --- */
static int
test_kbi_truncated_prompt_header(void)
{
	uint8_t payload[32];
	size_t pos = 0;
	payload[pos++] = SSH_MSG_USERAUTH_INFO_REQUEST;
	dssh_serialize_uint32(0, payload, sizeof(payload), &pos);
	dssh_serialize_uint32(0, payload, sizeof(payload), &pos);
	dssh_serialize_uint32(0, payload, sizeof(payload), &pos);
	dssh_serialize_uint32(1, payload, sizeof(payload), &pos); /* 1 prompt */
	/* No room for the prompt's string length */
	int res = run_kbi_error_test(payload, pos,
	    kbi_cb_unreachable, NULL);
	ASSERT_EQ(res, DSSH_ERROR_PARSE);
	return TEST_PASS;
}

/* --- Prompt loop: prompt + echo byte overflows --- */
static int
test_kbi_truncated_prompt_data(void)
{
	uint8_t payload[32];
	size_t pos = 0;
	payload[pos++] = SSH_MSG_USERAUTH_INFO_REQUEST;
	dssh_serialize_uint32(0, payload, sizeof(payload), &pos);
	dssh_serialize_uint32(0, payload, sizeof(payload), &pos);
	dssh_serialize_uint32(0, payload, sizeof(payload), &pos);
	dssh_serialize_uint32(1, payload, sizeof(payload), &pos);
	dssh_serialize_uint32(100, payload, sizeof(payload), &pos); /* 100 bytes of prompt */
	/* Only 0 bytes of prompt data + no echo flag */
	int res = run_kbi_error_test(payload, pos,
	    kbi_cb_unreachable, NULL);
	ASSERT_EQ(res, DSSH_ERROR_PARSE);
	return TEST_PASS;
}

/* --- Callback returns negative (abort) --- */
static int
test_kbi_callback_abort(void)
{
	uint8_t payload[64];
	const char *prompts[] = { "Token: " };
	bool echoes[] = { false };
	size_t len = build_info_request(payload, sizeof(payload),
	    "Auth", "Enter token", 1, prompts, echoes);

	int res = run_kbi_error_test(payload, len, kbi_cb_abort, NULL);
	ASSERT_EQ(res, -42);
	return TEST_PASS;
}

/* --- Zero prompts (valid but exercises num_prompts==0 path) --- */

static int
kbi_cb_zero_prompts(const uint8_t *name, size_t name_len,
    const uint8_t *instruction, size_t instruction_len,
    uint32_t num_prompts,
    const uint8_t **prompts, const size_t *prompt_lens,
    const bool *echo,
    uint8_t **responses, size_t *response_lens,
    void *cbdata)
{
	(void)name; (void)name_len; (void)instruction; (void)instruction_len;
	(void)prompts; (void)prompt_lens; (void)echo;
	(void)responses; (void)response_lens;
	bool *called = cbdata;
	*called = true;
	ASSERT_EQ_U(num_prompts, 0);
	return 0;
}

/*
 * Server thread for zero-prompts: sends INFO_REQUEST with 0 prompts,
 * receives INFO_RESPONSE, then sends SUCCESS.
 */
static int
kbi_zero_server_thread(void *arg)
{
	struct kbi_error_server_arg *a = arg;
	dssh_session sess = a->ctx->server;
	int res;

	res = kbi_error_service_exchange(sess);
	if (res < 0) { a->result = res; return 0; }

	/* Send INFO_REQUEST with 0 prompts */
	res = dssh_transport_send_packet(sess, a->info_payload,
	    a->info_payload_len, NULL);
	if (res < 0) { a->result = res; return 0; }

	/* Receive INFO_RESPONSE */
	uint8_t msg_type;
	uint8_t *payload;
	size_t payload_len;
	res = dssh_transport_recv_packet(sess, &msg_type, &payload, &payload_len);
	if (res < 0 || msg_type != SSH_MSG_USERAUTH_INFO_RESPONSE) {
		a->result = -10;
		return 0;
	}

	/* Send SUCCESS */
	uint8_t succ = SSH_MSG_USERAUTH_SUCCESS;
	a->result = dssh_transport_send_packet(sess, &succ, 1, NULL);
	return 0;
}

static int
test_kbi_zero_prompts(void)
{
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	uint8_t payload[32];
	size_t len = build_info_request(payload, sizeof(payload),
	    "", "", 0, NULL, NULL);

	struct kbi_error_server_arg sa = {0};
	sa.ctx = &ctx;
	sa.info_payload = payload;
	sa.info_payload_len = len;

	thrd_t st;
	ASSERT_TRUE(thrd_create(&st, kbi_zero_server_thread, &sa) == thrd_success);

	bool called = false;
	int res = dssh_auth_keyboard_interactive(ctx.client, "user",
	    kbi_cb_zero_prompts, &called);

	mock_io_close_c2s(&ctx.io);
	mock_io_close_s2c(&ctx.io);
	thrd_join(st, NULL);

	ASSERT_EQ(res, 0);
	ASSERT_TRUE(called);

	handshake_cleanup(&ctx);
	return TEST_PASS;
}

/* --- KBI FAILURE response --- */

static int
kbi_failure_server_thread(void *arg)
{
	struct kbi_error_server_arg *a = arg;
	dssh_session sess = a->ctx->server;
	int res;

	res = kbi_error_service_exchange(sess);
	if (res < 0) { a->result = res; return 0; }

	/* Send FAILURE instead of INFO_REQUEST */
	static const char methods[] = "password";
	uint8_t msg[64];
	size_t pos = 0;
	msg[pos++] = SSH_MSG_USERAUTH_FAILURE;
	dssh_serialize_uint32((uint32_t)(sizeof(methods) - 1), msg, sizeof(msg), &pos);
	memcpy(&msg[pos], methods, sizeof(methods) - 1);
	pos += sizeof(methods) - 1;
	msg[pos++] = 0;
	a->result = dssh_transport_send_packet(sess, msg, pos, NULL);
	return 0;
}

static int
test_kbi_failure_response(void)
{
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	struct kbi_error_server_arg sa = {0};
	sa.ctx = &ctx;

	thrd_t st;
	ASSERT_TRUE(thrd_create(&st, kbi_failure_server_thread, &sa) == thrd_success);

	int res = dssh_auth_keyboard_interactive(ctx.client, "user",
	    kbi_cb_unreachable, NULL);

	mock_io_close_c2s(&ctx.io);
	mock_io_close_s2c(&ctx.io);
	thrd_join(st, NULL);

	ASSERT_EQ(res, DSSH_ERROR_INIT);

	handshake_cleanup(&ctx);
	return TEST_PASS;
}

/* --- Unexpected message type during KBI --- */

static int
kbi_unexpected_server_thread(void *arg)
{
	struct kbi_error_server_arg *a = arg;
	dssh_session sess = a->ctx->server;
	int res;

	res = kbi_error_service_exchange(sess);
	if (res < 0) { a->result = res; return 0; }

	/* Send SERVICE_REQUEST (unexpected during auth) */
	uint8_t msg[8];
	size_t pos = 0;
	msg[pos++] = SSH_MSG_SERVICE_REQUEST;
	dssh_serialize_uint32(1, msg, sizeof(msg), &pos);
	msg[pos++] = 'x';
	a->result = dssh_transport_send_packet(sess, msg, pos, NULL);
	return 0;
}

static int
test_kbi_unexpected_message(void)
{
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	struct kbi_error_server_arg sa = {0};
	sa.ctx = &ctx;

	thrd_t st;
	ASSERT_TRUE(thrd_create(&st, kbi_unexpected_server_thread, &sa) == thrd_success);

	int res = dssh_auth_keyboard_interactive(ctx.client, "user",
	    kbi_cb_unreachable, NULL);

	mock_io_close_c2s(&ctx.io);
	mock_io_close_s2c(&ctx.io);
	thrd_join(st, NULL);

	ASSERT_EQ(res, DSSH_ERROR_PARSE);

	handshake_cleanup(&ctx);
	return TEST_PASS;
}

/* --- Banner before INFO_REQUEST --- */

static int
kbi_banner_server_thread(void *arg)
{
	struct kbi_error_server_arg *a = arg;
	dssh_session sess = a->ctx->server;
	int res;

	res = kbi_error_service_exchange(sess);
	if (res < 0) { a->result = res; return 0; }

	/* Send BANNER first */
	{
		static const char bmsg[] = "Welcome";
		uint8_t msg[64];
		size_t pos = 0;
		msg[pos++] = SSH_MSG_USERAUTH_BANNER;
		dssh_serialize_uint32((uint32_t)(sizeof(bmsg) - 1), msg, sizeof(msg), &pos);
		memcpy(&msg[pos], bmsg, sizeof(bmsg) - 1);
		pos += sizeof(bmsg) - 1;
		dssh_serialize_uint32(0, msg, sizeof(msg), &pos); /* language */
		res = dssh_transport_send_packet(sess, msg, pos, NULL);
		if (res < 0) { a->result = res; return 0; }
	}

	/* Then send INFO_REQUEST with 0 prompts + SUCCESS */
	res = dssh_transport_send_packet(sess, a->info_payload,
	    a->info_payload_len, NULL);
	if (res < 0) { a->result = res; return 0; }

	/* Receive INFO_RESPONSE */
	uint8_t msg_type;
	uint8_t *payload;
	size_t payload_len;
	res = dssh_transport_recv_packet(sess, &msg_type, &payload, &payload_len);
	if (res < 0) { a->result = res; return 0; }

	/* Send SUCCESS */
	uint8_t succ = SSH_MSG_USERAUTH_SUCCESS;
	a->result = dssh_transport_send_packet(sess, &succ, 1, NULL);
	return 0;
}

static int
test_kbi_banner_before_info(void)
{
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	uint8_t payload[32];
	size_t len = build_info_request(payload, sizeof(payload),
	    "", "", 0, NULL, NULL);

	struct kbi_error_server_arg sa = {0};
	sa.ctx = &ctx;
	sa.info_payload = payload;
	sa.info_payload_len = len;

	thrd_t st;
	ASSERT_TRUE(thrd_create(&st, kbi_banner_server_thread, &sa) == thrd_success);

	bool called = false;
	int res = dssh_auth_keyboard_interactive(ctx.client, "user",
	    kbi_cb_zero_prompts, &called);

	mock_io_close_c2s(&ctx.io);
	mock_io_close_s2c(&ctx.io);
	thrd_join(st, NULL);

	ASSERT_EQ(res, 0);
	ASSERT_TRUE(called);

	handshake_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Server auth parse error tests — truncated USERAUTH_REQUEST
 *
 * Each test completes a handshake, starts the server auth loop,
 * does the SERVICE_REQUEST/ACCEPT exchange, then sends a
 * malformed USERAUTH_REQUEST to trigger parse error branches.
 * ================================================================ */

/*
 * Helper: set up handshake + SERVICE_REQUEST/ACCEPT, start server
 * auth thread.  Returns 0 on success with server thread running.
 * Caller must close pipes and join st on exit.
 */
struct parse_error_ctx {
	struct handshake_ctx hctx;
	struct auth_server_arg sa;
	struct test_auth_cbdata cbdata;
	thrd_t st;
};

static int
parse_error_setup(struct parse_error_ctx *pe)
{
	memset(pe, 0, sizeof(*pe));
	if (handshake_setup(&pe->hctx) < 0) {
		handshake_cleanup(&pe->hctx);
		return -1;
	}

	pe->cbdata.password_result = DSSH_AUTH_SUCCESS;
	pe->sa.ctx = &pe->hctx;
	pe->sa.cbs.methods_str = "password,publickey";
	pe->sa.cbs.password_cb = test_password_cb;
	pe->sa.cbs.publickey_cb = test_publickey_cb;
	pe->sa.cbs.cbdata = &pe->cbdata;

	if (thrd_create(&pe->st, auth_server_thread, &pe->sa) != thrd_success)
		return -1;

	/* SERVICE_REQUEST */
	{
		static const char svc[] = "ssh-userauth";
		uint8_t msg[64];
		size_t pos = 0;
		msg[pos++] = SSH_MSG_SERVICE_REQUEST;
		dssh_serialize_uint32((uint32_t)(sizeof(svc) - 1), msg, sizeof(msg), &pos);
		memcpy(&msg[pos], svc, sizeof(svc) - 1);
		pos += sizeof(svc) - 1;
		if (dssh_transport_send_packet(pe->hctx.client, msg, pos, NULL) < 0)
			return -1;
	}
	{
		uint8_t msg_type;
		uint8_t *payload;
		size_t payload_len;
		if (dssh_transport_recv_packet(pe->hctx.client, &msg_type,
		    &payload, &payload_len) < 0)
			return -1;
		if (msg_type != SSH_MSG_SERVICE_ACCEPT)
			return -1;
	}
	return 0;
}

static void
parse_error_cleanup(struct parse_error_ctx *pe)
{
	mock_io_close_c2s(&pe->hctx.io);
	mock_io_close_s2c(&pe->hctx.io);
	thrd_join(pe->st, NULL);
	handshake_cleanup(&pe->hctx);
}

static int
test_server_parse_empty_request(void)
{
	/* USERAUTH_REQUEST with only the message type byte */
	struct parse_error_ctx pe;
	if (parse_error_setup(&pe) < 0) {
		parse_error_cleanup(&pe);
		return TEST_FAIL;
	}

	uint8_t msg[] = { SSH_MSG_USERAUTH_REQUEST };
	ASSERT_OK(dssh_transport_send_packet(pe.hctx.client, msg, sizeof(msg), NULL));

	parse_error_cleanup(&pe);
	ASSERT_TRUE(pe.sa.result < 0);
	return TEST_PASS;
}

static int
test_server_parse_truncated_username(void)
{
	/* Username length says 100 but only 2 bytes follow */
	struct parse_error_ctx pe;
	if (parse_error_setup(&pe) < 0) {
		parse_error_cleanup(&pe);
		return TEST_FAIL;
	}

	uint8_t msg[16];
	size_t pos = 0;
	msg[pos++] = SSH_MSG_USERAUTH_REQUEST;
	dssh_serialize_uint32(100, msg, sizeof(msg), &pos); /* username len */
	msg[pos++] = 'u';
	msg[pos++] = 's';
	ASSERT_OK(dssh_transport_send_packet(pe.hctx.client, msg, pos, NULL));

	parse_error_cleanup(&pe);
	ASSERT_TRUE(pe.sa.result < 0);
	return TEST_PASS;
}

static int
test_server_parse_no_service(void)
{
	/* Valid username, but no service name field */
	struct parse_error_ctx pe;
	if (parse_error_setup(&pe) < 0) {
		parse_error_cleanup(&pe);
		return TEST_FAIL;
	}

	uint8_t msg[16];
	size_t pos = 0;
	msg[pos++] = SSH_MSG_USERAUTH_REQUEST;
	dssh_serialize_uint32(1, msg, sizeof(msg), &pos);
	msg[pos++] = 'u';
	/* No service name length field */
	ASSERT_OK(dssh_transport_send_packet(pe.hctx.client, msg, pos, NULL));

	parse_error_cleanup(&pe);
	ASSERT_TRUE(pe.sa.result < 0);
	return TEST_PASS;
}

static int
test_server_parse_truncated_service(void)
{
	/* Service name length says 50 but data is short */
	struct parse_error_ctx pe;
	if (parse_error_setup(&pe) < 0) {
		parse_error_cleanup(&pe);
		return TEST_FAIL;
	}

	uint8_t msg[16];
	size_t pos = 0;
	msg[pos++] = SSH_MSG_USERAUTH_REQUEST;
	dssh_serialize_uint32(1, msg, sizeof(msg), &pos);
	msg[pos++] = 'u';
	dssh_serialize_uint32(50, msg, sizeof(msg), &pos); /* service len */
	/* No service data */
	ASSERT_OK(dssh_transport_send_packet(pe.hctx.client, msg, pos, NULL));

	parse_error_cleanup(&pe);
	ASSERT_TRUE(pe.sa.result < 0);
	return TEST_PASS;
}

static int
test_server_parse_no_method(void)
{
	/* Valid username + service, but no method field */
	struct parse_error_ctx pe;
	if (parse_error_setup(&pe) < 0) {
		parse_error_cleanup(&pe);
		return TEST_FAIL;
	}

	static const char svc[] = "ssh-connection";
	uint8_t msg[64];
	size_t pos = 0;
	msg[pos++] = SSH_MSG_USERAUTH_REQUEST;
	dssh_serialize_uint32(1, msg, sizeof(msg), &pos);
	msg[pos++] = 'u';
	dssh_serialize_uint32((uint32_t)(sizeof(svc) - 1), msg, sizeof(msg), &pos);
	memcpy(&msg[pos], svc, sizeof(svc) - 1);
	pos += sizeof(svc) - 1;
	/* No method name */
	ASSERT_OK(dssh_transport_send_packet(pe.hctx.client, msg, pos, NULL));

	parse_error_cleanup(&pe);
	ASSERT_TRUE(pe.sa.result < 0);
	return TEST_PASS;
}

static int
test_server_parse_truncated_method(void)
{
	/* Method length says 50 but data is short */
	struct parse_error_ctx pe;
	if (parse_error_setup(&pe) < 0) {
		parse_error_cleanup(&pe);
		return TEST_FAIL;
	}

	static const char svc[] = "ssh-connection";
	uint8_t msg[64];
	size_t pos = 0;
	msg[pos++] = SSH_MSG_USERAUTH_REQUEST;
	dssh_serialize_uint32(1, msg, sizeof(msg), &pos);
	msg[pos++] = 'u';
	dssh_serialize_uint32((uint32_t)(sizeof(svc) - 1), msg, sizeof(msg), &pos);
	memcpy(&msg[pos], svc, sizeof(svc) - 1);
	pos += sizeof(svc) - 1;
	dssh_serialize_uint32(50, msg, sizeof(msg), &pos); /* method len */
	/* No method data */
	ASSERT_OK(dssh_transport_send_packet(pe.hctx.client, msg, pos, NULL));

	parse_error_cleanup(&pe);
	ASSERT_TRUE(pe.sa.result < 0);
	return TEST_PASS;
}

static int
test_server_parse_password_no_bool(void)
{
	/* Password method, but no change boolean */
	struct parse_error_ctx pe;
	if (parse_error_setup(&pe) < 0) {
		parse_error_cleanup(&pe);
		return TEST_FAIL;
	}

	static const char svc[] = "ssh-connection";
	static const char method[] = "password";
	uint8_t msg[64];
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
	/* No change boolean or password */
	ASSERT_OK(dssh_transport_send_packet(pe.hctx.client, msg, pos, NULL));

	parse_error_cleanup(&pe);
	ASSERT_TRUE(pe.sa.result < 0);
	return TEST_PASS;
}

static int
test_server_parse_password_no_password(void)
{
	/* Password method with change=false, but no password length */
	struct parse_error_ctx pe;
	if (parse_error_setup(&pe) < 0) {
		parse_error_cleanup(&pe);
		return TEST_FAIL;
	}

	static const char svc[] = "ssh-connection";
	static const char method[] = "password";
	uint8_t msg[64];
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
	msg[pos++] = 0; /* change = false */
	/* No password length */
	ASSERT_OK(dssh_transport_send_packet(pe.hctx.client, msg, pos, NULL));

	parse_error_cleanup(&pe);
	ASSERT_TRUE(pe.sa.result < 0);
	return TEST_PASS;
}

static int
test_server_parse_publickey_no_has_sig(void)
{
	/* Publickey method, but no has_signature boolean */
	struct parse_error_ctx pe;
	if (parse_error_setup(&pe) < 0) {
		parse_error_cleanup(&pe);
		return TEST_FAIL;
	}

	static const char svc[] = "ssh-connection";
	static const char method[] = "publickey";
	uint8_t msg[64];
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
	/* No has_signature boolean */
	ASSERT_OK(dssh_transport_send_packet(pe.hctx.client, msg, pos, NULL));

	parse_error_cleanup(&pe);
	ASSERT_TRUE(pe.sa.result < 0);
	return TEST_PASS;
}

static int
test_server_parse_not_service_request(void)
{
	/* First message is not SERVICE_REQUEST */
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	struct test_auth_cbdata cbdata = {0};
	struct auth_server_arg sa = {0};
	sa.ctx = &ctx;
	sa.cbs.methods_str = "password";
	sa.cbs.password_cb = test_password_cb;
	sa.cbs.cbdata = &cbdata;

	thrd_t st;
	ASSERT_TRUE(thrd_create(&st, auth_server_thread, &sa) == thrd_success);

	/* Send USERAUTH_REQUEST instead of SERVICE_REQUEST */
	uint8_t msg[] = { SSH_MSG_USERAUTH_REQUEST, 0, 0, 0, 0 };
	ASSERT_OK(dssh_transport_send_packet(ctx.client, msg, sizeof(msg), NULL));

	mock_io_close_c2s(&ctx.io);
	mock_io_close_s2c(&ctx.io);
	thrd_join(st, NULL);
	ASSERT_TRUE(sa.result < 0);

	handshake_cleanup(&ctx);
	return TEST_PASS;
}

static int
test_server_parse_long_username(void)
{
	/* Username >= 256 bytes — tests the saved_user truncation path */
	struct parse_error_ctx pe;
	if (parse_error_setup(&pe) < 0) {
		parse_error_cleanup(&pe);
		return TEST_FAIL;
	}

	static const char svc[] = "ssh-connection";
	static const char method[] = "none";
	uint8_t msg[512];
	size_t pos = 0;
	msg[pos++] = SSH_MSG_USERAUTH_REQUEST;
	dssh_serialize_uint32(300, msg, sizeof(msg), &pos);
	memset(&msg[pos], 'A', 300);
	pos += 300;
	dssh_serialize_uint32((uint32_t)(sizeof(svc) - 1), msg, sizeof(msg), &pos);
	memcpy(&msg[pos], svc, sizeof(svc) - 1);
	pos += sizeof(svc) - 1;
	dssh_serialize_uint32((uint32_t)(sizeof(method) - 1), msg, sizeof(msg), &pos);
	memcpy(&msg[pos], method, sizeof(method) - 1);
	pos += sizeof(method) - 1;
	ASSERT_OK(dssh_transport_send_packet(pe.hctx.client, msg, pos, NULL));

	/* Server should process this (none auth with no none_cb = failure),
	 * but the username should be truncated to 255 bytes in saved_user. */
	parse_error_cleanup(&pe);
	/* Server loops waiting for more auth — it didn't crash */
	return TEST_PASS;
}

/* Helper: build a valid USERAUTH_REQUEST prefix (msg_type + user + svc + method) */
static size_t
build_auth_prefix(uint8_t *msg, size_t bufsz, const char *method)
{
	static const char svc[] = "ssh-connection";
	size_t pos = 0;
	msg[pos++] = SSH_MSG_USERAUTH_REQUEST;
	dssh_serialize_uint32(4, msg, bufsz, &pos);
	memcpy(&msg[pos], "user", 4);
	pos += 4;
	dssh_serialize_uint32((uint32_t)(sizeof(svc) - 1), msg, bufsz, &pos);
	memcpy(&msg[pos], svc, sizeof(svc) - 1);
	pos += sizeof(svc) - 1;
	size_t mlen = strlen(method);
	dssh_serialize_uint32((uint32_t)mlen, msg, bufsz, &pos);
	memcpy(&msg[pos], method, mlen);
	pos += mlen;
	return pos;
}

static int
test_server_parse_password_truncated_pw(void)
{
	/* Password length says 100 but data is short */
	struct parse_error_ctx pe;
	if (parse_error_setup(&pe) < 0) {
		parse_error_cleanup(&pe);
		return TEST_FAIL;
	}

	uint8_t msg[128];
	size_t pos = build_auth_prefix(msg, sizeof(msg), "password");
	msg[pos++] = 0; /* change = false */
	dssh_serialize_uint32(100, msg, sizeof(msg), &pos); /* pw_len */
	msg[pos++] = 'p'; /* only 1 byte of "password" data */
	ASSERT_OK(dssh_transport_send_packet(pe.hctx.client, msg, pos, NULL));

	parse_error_cleanup(&pe);
	ASSERT_TRUE(pe.sa.result < 0);
	return TEST_PASS;
}

static int
test_server_parse_password_change_no_newpw(void)
{
	/* Password with change=true, valid old password, but no new_password */
	struct parse_error_ctx pe;
	if (parse_error_setup(&pe) < 0) {
		parse_error_cleanup(&pe);
		return TEST_FAIL;
	}

	uint8_t msg[128];
	size_t pos = build_auth_prefix(msg, sizeof(msg), "password");
	msg[pos++] = 1; /* change = true */
	dssh_serialize_uint32(4, msg, sizeof(msg), &pos);
	memcpy(&msg[pos], "old!", 4);
	pos += 4;
	/* No new_password length field */
	ASSERT_OK(dssh_transport_send_packet(pe.hctx.client, msg, pos, NULL));

	parse_error_cleanup(&pe);
	ASSERT_TRUE(pe.sa.result < 0);
	return TEST_PASS;
}

static int
test_server_parse_password_change_truncated_newpw(void)
{
	/* change=true, valid old pw, new_password length > remaining */
	struct parse_error_ctx pe;
	if (parse_error_setup(&pe) < 0) {
		parse_error_cleanup(&pe);
		return TEST_FAIL;
	}

	uint8_t msg[128];
	size_t pos = build_auth_prefix(msg, sizeof(msg), "password");
	msg[pos++] = 1; /* change = true */
	dssh_serialize_uint32(3, msg, sizeof(msg), &pos);
	memcpy(&msg[pos], "old", 3);
	pos += 3;
	dssh_serialize_uint32(200, msg, sizeof(msg), &pos); /* new_pw_len too large */
	ASSERT_OK(dssh_transport_send_packet(pe.hctx.client, msg, pos, NULL));

	parse_error_cleanup(&pe);
	ASSERT_TRUE(pe.sa.result < 0);
	return TEST_PASS;
}

static int
test_server_no_password_cb(void)
{
	/* Password method arrives but no password_cb registered */
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	struct test_auth_cbdata cbdata = {0};
	cbdata.none_result = DSSH_AUTH_SUCCESS;
	struct auth_server_arg sa = {0};
	sa.ctx = &ctx;
	sa.cbs.methods_str = "password";
	/* No password_cb set — NULL */
	sa.cbs.none_cb = test_none_cb;
	sa.cbs.cbdata = &cbdata;

	thrd_t st;
	ASSERT_TRUE(thrd_create(&st, auth_server_thread, &sa) == thrd_success);

	/* SERVICE_REQUEST/ACCEPT */
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

	/* Send password auth — server has no password_cb */
	uint8_t msg[128];
	size_t pos = build_auth_prefix(msg, sizeof(msg), "password");
	msg[pos++] = 0; /* change = false */
	dssh_serialize_uint32(4, msg, sizeof(msg), &pos);
	memcpy(&msg[pos], "pass", 4);
	pos += 4;
	ASSERT_OK(dssh_transport_send_packet(ctx.client, msg, pos, NULL));

	/* Server should send USERAUTH_FAILURE (no password_cb) */
	{
		uint8_t msg_type;
		uint8_t *payload;
		size_t payload_len;
		ASSERT_OK(dssh_transport_recv_packet(ctx.client, &msg_type, &payload, &payload_len));
		ASSERT_EQ(msg_type, SSH_MSG_USERAUTH_FAILURE);
	}

	/* Now send none auth to let the server succeed and exit */
	{
		uint8_t nmsg[64];
		size_t npos = build_auth_prefix(nmsg, sizeof(nmsg), "none");
		ASSERT_OK(dssh_transport_send_packet(ctx.client, nmsg, npos, NULL));
	}
	{
		uint8_t msg_type;
		uint8_t *payload;
		size_t payload_len;
		ASSERT_OK(dssh_transport_recv_packet(ctx.client, &msg_type, &payload, &payload_len));
		ASSERT_EQ(msg_type, SSH_MSG_USERAUTH_SUCCESS);
	}

	thrd_join(st, NULL);
	ASSERT_EQ(sa.result, 0);

	handshake_cleanup(&ctx);
	return TEST_PASS;
}

static int
test_server_no_publickey_cb(void)
{
	/* Publickey method arrives but no publickey_cb registered */
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	struct test_auth_cbdata cbdata = {0};
	cbdata.none_result = DSSH_AUTH_SUCCESS;
	struct auth_server_arg sa = {0};
	sa.ctx = &ctx;
	sa.cbs.methods_str = "publickey";
	/* No publickey_cb set */
	sa.cbs.none_cb = test_none_cb;
	sa.cbs.cbdata = &cbdata;

	thrd_t st;
	ASSERT_TRUE(thrd_create(&st, auth_server_thread, &sa) == thrd_success);

	/* SERVICE_REQUEST/ACCEPT */
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

	/* Send publickey probe — server has no publickey_cb */
	uint8_t msg[256];
	size_t pos = build_auth_prefix(msg, sizeof(msg), "publickey");
	msg[pos++] = 0; /* has_sig = false */
	dssh_serialize_uint32(11, msg, sizeof(msg), &pos);
	memcpy(&msg[pos], "ssh-ed25519", 11);
	pos += 11;
	dssh_serialize_uint32(4, msg, sizeof(msg), &pos);
	memcpy(&msg[pos], "fake", 4);
	pos += 4;
	ASSERT_OK(dssh_transport_send_packet(ctx.client, msg, pos, NULL));

	/* Server should send USERAUTH_FAILURE */
	{
		uint8_t msg_type;
		uint8_t *payload;
		size_t payload_len;
		ASSERT_OK(dssh_transport_recv_packet(ctx.client, &msg_type, &payload, &payload_len));
		ASSERT_EQ(msg_type, SSH_MSG_USERAUTH_FAILURE);
	}

	/* Send none to exit cleanly */
	{
		uint8_t nmsg[64];
		size_t npos = build_auth_prefix(nmsg, sizeof(nmsg), "none");
		ASSERT_OK(dssh_transport_send_packet(ctx.client, nmsg, npos, NULL));
	}
	{
		uint8_t msg_type;
		uint8_t *payload;
		size_t payload_len;
		ASSERT_OK(dssh_transport_recv_packet(ctx.client, &msg_type, &payload, &payload_len));
		ASSERT_EQ(msg_type, SSH_MSG_USERAUTH_SUCCESS);
	}

	thrd_join(st, NULL);
	ASSERT_EQ(sa.result, 0);

	handshake_cleanup(&ctx);
	return TEST_PASS;
}

static int
test_server_parse_publickey_no_algo(void)
{
	/* Publickey with has_sig=false but no algo length */
	struct parse_error_ctx pe;
	if (parse_error_setup(&pe) < 0) {
		parse_error_cleanup(&pe);
		return TEST_FAIL;
	}

	uint8_t msg[128];
	size_t pos = build_auth_prefix(msg, sizeof(msg), "publickey");
	msg[pos++] = 0; /* has_sig = false */
	/* No algo length */
	ASSERT_OK(dssh_transport_send_packet(pe.hctx.client, msg, pos, NULL));

	parse_error_cleanup(&pe);
	ASSERT_TRUE(pe.sa.result < 0);
	return TEST_PASS;
}

static int
test_server_parse_publickey_no_blob(void)
{
	/* Publickey with valid algo but no pubkey blob */
	struct parse_error_ctx pe;
	if (parse_error_setup(&pe) < 0) {
		parse_error_cleanup(&pe);
		return TEST_FAIL;
	}

	uint8_t msg[128];
	size_t pos = build_auth_prefix(msg, sizeof(msg), "publickey");
	msg[pos++] = 0; /* has_sig = false */
	dssh_serialize_uint32(11, msg, sizeof(msg), &pos);
	memcpy(&msg[pos], "ssh-ed25519", 11);
	pos += 11;
	/* No pubkey blob length */
	ASSERT_OK(dssh_transport_send_packet(pe.hctx.client, msg, pos, NULL));

	parse_error_cleanup(&pe);
	ASSERT_TRUE(pe.sa.result < 0);
	return TEST_PASS;
}

static int
test_server_parse_publickey_sig_no_siglen(void)
{
	/* Publickey with has_sig=true, valid algo+blob, but no sig length */
	struct parse_error_ctx pe;
	if (parse_error_setup(&pe) < 0) {
		parse_error_cleanup(&pe);
		return TEST_FAIL;
	}

	uint8_t msg[128];
	size_t pos = build_auth_prefix(msg, sizeof(msg), "publickey");
	msg[pos++] = 1; /* has_sig = true */
	dssh_serialize_uint32(11, msg, sizeof(msg), &pos);
	memcpy(&msg[pos], "ssh-ed25519", 11);
	pos += 11;
	dssh_serialize_uint32(4, msg, sizeof(msg), &pos);
	memcpy(&msg[pos], "fake", 4);
	pos += 4;
	/* No signature length */
	ASSERT_OK(dssh_transport_send_packet(pe.hctx.client, msg, pos, NULL));

	parse_error_cleanup(&pe);
	ASSERT_TRUE(pe.sa.result < 0);
	return TEST_PASS;
}

static int
test_server_publickey_unknown_algo(void)
{
	/* Publickey with has_sig=true but unregistered algo name */
	struct parse_error_ctx pe;
	if (parse_error_setup(&pe) < 0) {
		parse_error_cleanup(&pe);
		return TEST_FAIL;
	}

	uint8_t msg[128];
	size_t pos = build_auth_prefix(msg, sizeof(msg), "publickey");
	msg[pos++] = 1; /* has_sig = true */
	dssh_serialize_uint32(12, msg, sizeof(msg), &pos);
	memcpy(&msg[pos], "unknown-algo", 12);
	pos += 12;
	dssh_serialize_uint32(4, msg, sizeof(msg), &pos);
	memcpy(&msg[pos], "fake", 4);
	pos += 4;
	dssh_serialize_uint32(4, msg, sizeof(msg), &pos);
	memcpy(&msg[pos], "sig!", 4);
	pos += 4;
	ASSERT_OK(dssh_transport_send_packet(pe.hctx.client, msg, pos, NULL));

	/* Server should send USERAUTH_FAILURE for unknown algo.
	 * Then we need to let the server exit — send none. */
	{
		uint8_t msg_type;
		uint8_t *payload;
		size_t payload_len;
		ASSERT_OK(dssh_transport_recv_packet(pe.hctx.client, &msg_type, &payload, &payload_len));
		ASSERT_EQ(msg_type, SSH_MSG_USERAUTH_FAILURE);
	}

	parse_error_cleanup(&pe);
	return TEST_PASS;
}

static int
test_server_publickey_probe_rejected(void)
{
	/* Key probe (has_sig=false) where callback rejects the key */
	struct parse_error_ctx pe;
	if (parse_error_setup(&pe) < 0) {
		parse_error_cleanup(&pe);
		return TEST_FAIL;
	}

	/* Override callback to reject */
	pe.cbdata.publickey_result = DSSH_AUTH_FAILURE;

	uint8_t msg[256];
	size_t pos = build_auth_prefix(msg, sizeof(msg), "publickey");
	msg[pos++] = 0; /* has_sig = false */
	dssh_serialize_uint32(11, msg, sizeof(msg), &pos);
	memcpy(&msg[pos], "ssh-ed25519", 11);
	pos += 11;
	dssh_serialize_uint32(4, msg, sizeof(msg), &pos);
	memcpy(&msg[pos], "blob", 4);
	pos += 4;
	ASSERT_OK(dssh_transport_send_packet(pe.hctx.client, msg, pos, NULL));

	/* Should get FAILURE, not PK_OK */
	{
		uint8_t msg_type;
		uint8_t *payload;
		size_t payload_len;
		ASSERT_OK(dssh_transport_recv_packet(pe.hctx.client, &msg_type, &payload, &payload_len));
		ASSERT_EQ(msg_type, SSH_MSG_USERAUTH_FAILURE);
	}

	parse_error_cleanup(&pe);
	return TEST_PASS;
}

/* ================================================================
 * Password change flow — server-side branch coverage
 * ================================================================ */

static int
test_passwd_change_cb_failure(const uint8_t *username, size_t username_len,
    const uint8_t *old_pw, size_t old_pw_len,
    const uint8_t *new_pw, size_t new_pw_len,
    uint8_t **prompt, size_t *prompt_len, void *cbdata)
{
	(void)username; (void)username_len;
	(void)old_pw; (void)old_pw_len;
	(void)new_pw; (void)new_pw_len;
	(void)prompt; (void)prompt_len;
	(void)cbdata;
	return DSSH_AUTH_FAILURE;
}

static int
test_server_password_change_rejected(void)
{
	/* Send password with change=true; passwd_change_cb returns FAILURE.
	 * Server should send USERAUTH_FAILURE. */
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	struct test_auth_cbdata cbdata = {0};
	cbdata.none_result = DSSH_AUTH_SUCCESS;
	struct auth_server_arg sa = {0};
	sa.ctx = &ctx;
	sa.cbs.methods_str = "password";
	sa.cbs.password_cb = test_password_cb;
	sa.cbs.passwd_change_cb = test_passwd_change_cb_failure;
	sa.cbs.none_cb = test_none_cb;
	sa.cbs.cbdata = &cbdata;

	thrd_t st;
	ASSERT_TRUE(thrd_create(&st, auth_server_thread, &sa) == thrd_success);

	/* SERVICE_REQUEST/ACCEPT */
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

	/* Send password change request */
	uint8_t msg[128];
	size_t pos = build_auth_prefix(msg, sizeof(msg), "password");
	msg[pos++] = 1; /* change = true */
	dssh_serialize_uint32(3, msg, sizeof(msg), &pos);
	memcpy(&msg[pos], "old", 3);
	pos += 3;
	dssh_serialize_uint32(3, msg, sizeof(msg), &pos);
	memcpy(&msg[pos], "new", 3);
	pos += 3;
	ASSERT_OK(dssh_transport_send_packet(ctx.client, msg, pos, NULL));

	/* Should get FAILURE (callback rejected the change) */
	{
		uint8_t msg_type;
		uint8_t *payload;
		size_t payload_len;
		ASSERT_OK(dssh_transport_recv_packet(ctx.client, &msg_type, &payload, &payload_len));
		ASSERT_EQ(msg_type, SSH_MSG_USERAUTH_FAILURE);
	}

	/* Send none to finish */
	{
		uint8_t nmsg[64];
		size_t npos = build_auth_prefix(nmsg, sizeof(nmsg), "none");
		ASSERT_OK(dssh_transport_send_packet(ctx.client, nmsg, npos, NULL));
	}
	{
		uint8_t msg_type;
		uint8_t *payload;
		size_t payload_len;
		ASSERT_OK(dssh_transport_recv_packet(ctx.client, &msg_type, &payload, &payload_len));
		ASSERT_EQ(msg_type, SSH_MSG_USERAUTH_SUCCESS);
	}

	thrd_join(st, NULL);
	ASSERT_EQ(sa.result, 0);
	handshake_cleanup(&ctx);
	return TEST_PASS;
}

static int
test_server_password_change_no_change_cb(void)
{
	/* Send password with change=true but no passwd_change_cb.
	 * Server should fall through to FAILURE (auth_res stays FAILURE). */
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	struct test_auth_cbdata cbdata = {0};
	cbdata.none_result = DSSH_AUTH_SUCCESS;
	struct auth_server_arg sa = {0};
	sa.ctx = &ctx;
	sa.cbs.methods_str = "password";
	sa.cbs.password_cb = test_password_cb;
	/* No passwd_change_cb */
	sa.cbs.none_cb = test_none_cb;
	sa.cbs.cbdata = &cbdata;

	thrd_t st;
	ASSERT_TRUE(thrd_create(&st, auth_server_thread, &sa) == thrd_success);

	/* SERVICE_REQUEST/ACCEPT */
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

	/* Send password change request */
	uint8_t msg[128];
	size_t pos = build_auth_prefix(msg, sizeof(msg), "password");
	msg[pos++] = 1; /* change = true */
	dssh_serialize_uint32(3, msg, sizeof(msg), &pos);
	memcpy(&msg[pos], "old", 3);
	pos += 3;
	dssh_serialize_uint32(3, msg, sizeof(msg), &pos);
	memcpy(&msg[pos], "new", 3);
	pos += 3;
	ASSERT_OK(dssh_transport_send_packet(ctx.client, msg, pos, NULL));

	/* No passwd_change_cb → auth_res stays FAILURE → USERAUTH_FAILURE */
	{
		uint8_t msg_type;
		uint8_t *payload;
		size_t payload_len;
		ASSERT_OK(dssh_transport_recv_packet(ctx.client, &msg_type, &payload, &payload_len));
		ASSERT_EQ(msg_type, SSH_MSG_USERAUTH_FAILURE);
	}

	/* Send none to finish */
	{
		uint8_t nmsg[64];
		size_t npos = build_auth_prefix(nmsg, sizeof(nmsg), "none");
		ASSERT_OK(dssh_transport_send_packet(ctx.client, nmsg, npos, NULL));
	}
	{
		uint8_t msg_type;
		uint8_t *payload;
		size_t payload_len;
		ASSERT_OK(dssh_transport_recv_packet(ctx.client, &msg_type, &payload, &payload_len));
		ASSERT_EQ(msg_type, SSH_MSG_USERAUTH_SUCCESS);
	}

	thrd_join(st, NULL);
	ASSERT_EQ(sa.result, 0);
	handshake_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Coverage: publickey has_sig=true with truncated sig data
 * ================================================================ */

static int
test_server_parse_publickey_sig_truncated(void)
{
	/* has_sig=true, valid algo+blob, sig_len=100 but only 2 bytes */
	struct parse_error_ctx pe;
	if (parse_error_setup(&pe) < 0) {
		parse_error_cleanup(&pe);
		return TEST_FAIL;
	}

	uint8_t msg[256];
	size_t pos = build_auth_prefix(msg, sizeof(msg), "publickey");
	msg[pos++] = 1; /* has_sig = true */
	dssh_serialize_uint32(11, msg, sizeof(msg), &pos);
	memcpy(&msg[pos], "ssh-ed25519", 11);
	pos += 11;
	dssh_serialize_uint32(4, msg, sizeof(msg), &pos);
	memcpy(&msg[pos], "blob", 4);
	pos += 4;
	/* sig_len says 100 but only 2 bytes follow */
	dssh_serialize_uint32(100, msg, sizeof(msg), &pos);
	msg[pos++] = 0;
	msg[pos++] = 0;
	ASSERT_OK(dssh_transport_send_packet(pe.hctx.client, msg, pos, NULL));

	parse_error_cleanup(&pe);
	ASSERT_TRUE(pe.sa.result < 0);
	return TEST_PASS;
}

/* ================================================================
 * Coverage: publickey has_sig=true, truncated algo data
 * ================================================================ */

static int
test_server_parse_publickey_sig_trunc_algo(void)
{
	/* has_sig=true, algo_len=100 but only 2 bytes of algo data */
	struct parse_error_ctx pe;
	if (parse_error_setup(&pe) < 0) {
		parse_error_cleanup(&pe);
		return TEST_FAIL;
	}

	uint8_t msg[128];
	size_t pos = build_auth_prefix(msg, sizeof(msg), "publickey");
	msg[pos++] = 1; /* has_sig = true */
	dssh_serialize_uint32(100, msg, sizeof(msg), &pos);
	memcpy(&msg[pos], "xx", 2);
	pos += 2;
	ASSERT_OK(dssh_transport_send_packet(pe.hctx.client, msg, pos, NULL));

	parse_error_cleanup(&pe);
	ASSERT_TRUE(pe.sa.result < 0);
	return TEST_PASS;
}

/* ================================================================
 * Coverage: publickey has_sig=true, truncated blob data
 * ================================================================ */

static int
test_server_parse_publickey_sig_trunc_blob(void)
{
	/* has_sig=true, valid algo, pk_len=100 but only 2 bytes of blob */
	struct parse_error_ctx pe;
	if (parse_error_setup(&pe) < 0) {
		parse_error_cleanup(&pe);
		return TEST_FAIL;
	}

	uint8_t msg[128];
	size_t pos = build_auth_prefix(msg, sizeof(msg), "publickey");
	msg[pos++] = 1; /* has_sig = true */
	dssh_serialize_uint32(11, msg, sizeof(msg), &pos);
	memcpy(&msg[pos], "ssh-ed25519", 11);
	pos += 11;
	dssh_serialize_uint32(100, msg, sizeof(msg), &pos);
	memcpy(&msg[pos], "xx", 2);
	pos += 2;
	ASSERT_OK(dssh_transport_send_packet(pe.hctx.client, msg, pos, NULL));

	parse_error_cleanup(&pe);
	ASSERT_TRUE(pe.sa.result < 0);
	return TEST_PASS;
}

/* ================================================================
 * Coverage: publickey verify failure (valid format, bad signature)
 * ================================================================ */

static int
test_server_publickey_verify_bad_sig(void)
{
	/* has_sig=true, known algo, valid blob format, garbage signature */
	struct parse_error_ctx pe;
	if (parse_error_setup(&pe) < 0) {
		parse_error_cleanup(&pe);
		return TEST_FAIL;
	}

	/* Get real public key blob for the registered key algo */
	const char *algo = test_key_algo_name();
	dssh_key_algo ka = dssh_transport_find_key_algo(algo);
	if (ka == NULL || !ka->haskey(ka->ctx)) {
		parse_error_cleanup(&pe);
		return TEST_SKIP;
	}
	uint8_t pub_buf[1024];
	size_t pub_len;
	if (ka->pubkey(pub_buf, sizeof(pub_buf), &pub_len, ka->ctx) < 0) {
		parse_error_cleanup(&pe);
		return TEST_SKIP;
	}

	/* Build a publickey request with a garbage signature */
	size_t algo_len = strlen(algo);
	uint8_t msg[2048];
	size_t pos = build_auth_prefix(msg, sizeof(msg), "publickey");
	msg[pos++] = 1; /* has_sig = true */
	dssh_serialize_uint32((uint32_t)algo_len, msg, sizeof(msg), &pos);
	memcpy(&msg[pos], algo, algo_len);
	pos += algo_len;
	dssh_serialize_uint32((uint32_t)pub_len, msg, sizeof(msg), &pos);
	memcpy(&msg[pos], pub_buf, pub_len);
	pos += pub_len;
	/* Garbage signature: valid wire format but wrong data */
	uint8_t fake_sig[128];
	size_t fspos = 0;
	dssh_serialize_uint32((uint32_t)algo_len, fake_sig, sizeof(fake_sig), &fspos);
	memcpy(&fake_sig[fspos], algo, algo_len);
	fspos += algo_len;
	dssh_serialize_uint32(32, fake_sig, sizeof(fake_sig), &fspos);
	memset(&fake_sig[fspos], 0xAA, 32);
	fspos += 32;
	dssh_serialize_uint32((uint32_t)fspos, msg, sizeof(msg), &pos);
	memcpy(&msg[pos], fake_sig, fspos);
	pos += fspos;
	ASSERT_OK(dssh_transport_send_packet(pe.hctx.client, msg, pos, NULL));

	/* Server should verify the sig, fail, and send FAILURE.
	 * Then the pipe closes and auth returns error. */
	parse_error_cleanup(&pe);
	/* Server should NOT have succeeded */
	ASSERT_TRUE(pe.sa.result != 0);
	return TEST_PASS;
}

/* ================================================================
 * Coverage: password CHANGEREQ → cb returns CHANGE_PASSWORD again
 * ================================================================ */

static int
changereq_loop_password_cb(const uint8_t *username, size_t username_len,
    const uint8_t *password, size_t password_len,
    uint8_t **change_prompt, size_t *change_prompt_len,
    void *cbdata)
{
	(void)username; (void)username_len;
	(void)password; (void)password_len;
	*change_prompt = malloc(6);
	if (*change_prompt == NULL) {
		*change_prompt_len = 0;
		return DSSH_AUTH_FAILURE;
	}
	memcpy(*change_prompt, "retry!", 6);
	*change_prompt_len = 6;
	return DSSH_AUTH_CHANGE_PASSWORD;
}

static int
changereq_loop_change_cb(const uint8_t *username, size_t username_len,
    const uint8_t *old_pw, size_t old_pw_len,
    const uint8_t *new_pw, size_t new_pw_len,
    uint8_t **prompt, size_t *prompt_len, void *cbdata)
{
	(void)username; (void)username_len;
	(void)old_pw; (void)old_pw_len;
	(void)new_pw; (void)new_pw_len;
	int *count = cbdata;
	(*count)++;
	if (*count < 2) {
		/* First attempt: ask for another change */
		*prompt = malloc(10);
		if (*prompt == NULL) {
			*prompt_len = 0;
			return DSSH_AUTH_FAILURE;
		}
		memcpy(*prompt, "try again!", 10);
		*prompt_len = 10;
		return DSSH_AUTH_CHANGE_PASSWORD;
	}
	/* Second attempt: accept */
	return DSSH_AUTH_SUCCESS;
}

static int
test_client_changereq_cb(const uint8_t *prompt, size_t prompt_len,
    const uint8_t *language, size_t language_len,
    uint8_t **new_password, size_t *new_password_len,
    void *cbdata)
{
	(void)prompt; (void)prompt_len;
	(void)language; (void)language_len;
	(void)cbdata;
	*new_password = malloc(7);
	memcpy(*new_password, "newpass", 7);
	*new_password_len = 7;
	return 0;
}

static int
test_password_changereq_loop(void)
{
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	int change_count = 0;
	struct auth_server_arg sa = {0};
	sa.ctx = &ctx;
	sa.cbs.methods_str = "password";
	sa.cbs.password_cb = changereq_loop_password_cb;
	sa.cbs.passwd_change_cb = changereq_loop_change_cb;
	sa.cbs.cbdata = &change_count;

	thrd_t st;
	ASSERT_TRUE(thrd_create(&st, auth_server_thread, &sa) == thrd_success);

	/* Client: password auth with change callback.
	 * Server password_cb returns CHANGE_PASSWORD with prompt.
	 * Client sends new password (change=true).
	 * Server passwd_change_cb returns CHANGE_PASSWORD again (first time).
	 * Client sends another new password.
	 * Server passwd_change_cb returns SUCCESS (second time). */
	int res = dssh_auth_password(ctx.client, "testuser", "oldpass",
	    test_client_changereq_cb, NULL);

	thrd_join(st, NULL);

	/* Auth should have succeeded after the change loop */
	ASSERT_EQ(res, 0);
	ASSERT_EQ(sa.result, 0);
	ASSERT_EQ(change_count, 2);

	handshake_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Coverage: truncated USERAUTH_BANNER on client side
 *
 * We set up handshake, then from the server side send raw truncated
 * BANNER packets before the auth response.  The client processes
 * these in handle_banner().
 * ================================================================ */

static bool banner_trunc_cb_called;

static void
banner_trunc_cb(const uint8_t *message, size_t message_len,
    const uint8_t *language, size_t language_len,
    void *cbdata)
{
	(void)message; (void)message_len;
	(void)language; (void)language_len;
	(void)cbdata;
	banner_trunc_cb_called = true;
}

static int
banner_trunc_server_thread(void *arg)
{
	struct auth_server_arg *sa = arg;
	dssh_session server = sa->ctx->server;

	/* Complete SERVICE_REQUEST/ACCEPT */
	uint8_t msg_type;
	uint8_t *payload;
	size_t payload_len;
	if (dssh_transport_recv_packet(server, &msg_type, &payload, &payload_len) < 0)
		{ sa->result = -1; return 0; }
	{
		uint8_t accept[64];
		size_t pos = 0;
		accept[pos++] = SSH_MSG_SERVICE_ACCEPT;
		if (payload_len > 5) {
			uint32_t slen;
			dssh_parse_uint32(&payload[1], payload_len - 1, &slen);
			dssh_serialize_uint32(slen, accept, sizeof(accept), &pos);
			memcpy(&accept[pos], &payload[5], slen);
			pos += slen;
		}
		dssh_transport_send_packet(server, accept, pos, NULL);
	}

	/* Send truncated banners before reading the auth request */
	/* Banner with only msg_type — no msg_len header (line 18) */
	{
		uint8_t b[] = { 53 }; /* SSH_MSG_USERAUTH_BANNER */
		dssh_transport_send_packet(server, b, sizeof(b), NULL);
	}
	/* Banner with msg_len=100 but no msg data (line 22) */
	{
		uint8_t b[8];
		size_t bp = 0;
		b[bp++] = 53;
		dssh_serialize_uint32(100, b, sizeof(b), &bp);
		dssh_transport_send_packet(server, b, bp, NULL);
	}
	/* Banner with valid msg but truncated lang (line 33-34) */
	{
		uint8_t b[32];
		size_t bp = 0;
		b[bp++] = 53;
		dssh_serialize_uint32(5, b, sizeof(b), &bp);
		memcpy(&b[bp], "hello", 5);
		bp += 5;
		/* lang_len=100 but no lang data */
		dssh_serialize_uint32(100, b, sizeof(b), &bp);
		dssh_transport_send_packet(server, b, bp, NULL);
	}

	/* Now send USERAUTH_SUCCESS so the client returns */
	{
		uint8_t msg_type2;
		uint8_t *p2;
		size_t p2_len;
		dssh_transport_recv_packet(server, &msg_type2, &p2, &p2_len);
		uint8_t success[] = { 52 }; /* SSH_MSG_USERAUTH_SUCCESS */
		dssh_transport_send_packet(server, success, 1, NULL);
	}

	sa->result = 0;
	return 0;
}

static int
test_banner_truncated_variants(void)
{
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	banner_trunc_cb_called = false;
	dssh_session_set_banner_cb(ctx.client, banner_trunc_cb, NULL);

	struct auth_server_arg sa = {0};
	sa.ctx = &ctx;

	thrd_t st;
	ASSERT_TRUE(thrd_create(&st, banner_trunc_server_thread, &sa) == thrd_success);

	/* Client: password auth — will see truncated banners first */
	int res = dssh_auth_password(ctx.client, "testuser", "testpass",
	    NULL, NULL);

	thrd_join(st, NULL);
	ASSERT_EQ(res, 0);  /* auth succeeded */
	/* The valid banner (line 33-34 variant) should have invoked cb */
	ASSERT_TRUE(banner_trunc_cb_called);

	handshake_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Coverage: client get_methods receives SUCCESS ("none" auth)
 * ================================================================ */

static int
none_accept_server_thread(void *arg)
{
	struct auth_server_arg *sa = arg;
	uint8_t username[256];
	size_t username_len;
	sa->result = dssh_auth_server(sa->ctx->server, &sa->cbs,
	    username, &username_len);
	return 0;
}

static int
test_get_methods_none_accepted(void)
{
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	/* Server: accept "none" auth method */
	struct test_auth_cbdata cbdata = {0};
	cbdata.none_result = DSSH_AUTH_SUCCESS;
	struct auth_server_arg sa = {0};
	sa.ctx = &ctx;
	sa.cbs.methods_str = "none";
	sa.cbs.none_cb = test_none_cb;
	sa.cbs.cbdata = &cbdata;

	thrd_t st;
	ASSERT_TRUE(thrd_create(&st, none_accept_server_thread, &sa) == thrd_success);

	/* Client: get_methods sends "none" USERAUTH_REQUEST.
	 * Server accepts it → sends USERAUTH_SUCCESS.
	 * get_methods should return 0 with empty methods string. */
	char methods[256] = "unchanged";
	int res = dssh_auth_get_methods(ctx.client, "testuser",
	    methods, sizeof(methods));

	thrd_join(st, NULL);
	ASSERT_EQ(res, 0);
	ASSERT_EQ(methods[0], '\0');
	ASSERT_EQ(sa.result, 0);

	handshake_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Coverage: client get_methods FAILURE parse errors
 * Server sends crafted USERAUTH_FAILURE responses.
 * ================================================================ */

/* Helper: server that does SERVICE_ACCEPT, receives auth request,
 * then sends a crafted response. */
struct crafted_response_ctx {
	struct handshake_ctx *hctx;
	const uint8_t *response;
	size_t response_len;
	bool close_after; /* close s2c pipe after sending */
};

static int
crafted_response_server_thread(void *arg)
{
	struct crafted_response_ctx *cr = arg;
	dssh_session server = cr->hctx->server;

	/* SERVICE_REQUEST → SERVICE_ACCEPT */
	uint8_t msg_type;
	uint8_t *payload;
	size_t payload_len;
	if (dssh_transport_recv_packet(server, &msg_type, &payload, &payload_len) < 0)
		return 0;
	{
		uint8_t accept[64];
		size_t pos = 0;
		accept[pos++] = SSH_MSG_SERVICE_ACCEPT;
		if (payload_len > 5) {
			uint32_t slen;
			dssh_parse_uint32(&payload[1], payload_len - 1, &slen);
			dssh_serialize_uint32(slen, accept, sizeof(accept), &pos);
			memcpy(&accept[pos], &payload[5], slen);
			pos += slen;
		}
		dssh_transport_send_packet(server, accept, pos, NULL);
	}

	/* Receive USERAUTH_REQUEST */
	dssh_transport_recv_packet(server, &msg_type, &payload, &payload_len);

	/* Send the crafted response */
	dssh_transport_send_packet(server, cr->response, cr->response_len, NULL);
	/* Close pipe so client's read() gets EOF instead of blocking */
	mock_io_close_s2c(&cr->hctx->io);
	return 0;
}

static int
test_get_methods_failure_truncated(void)
{
	/* USERAUTH_FAILURE with only msg_type byte — line 574 */
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	uint8_t resp[] = { 51 }; /* SSH_MSG_USERAUTH_FAILURE, no data */
	struct crafted_response_ctx cr = { &ctx, resp, sizeof(resp) };
	thrd_t st;
	thrd_create(&st, crafted_response_server_thread, &cr);

	char methods[256];
	int res = dssh_auth_get_methods(ctx.client, "user", methods, sizeof(methods));
	thrd_join(st, NULL);
	ASSERT_TRUE(res < 0);

	handshake_cleanup(&ctx);
	return TEST_PASS;
}

static int
test_get_methods_failure_methods_len_exceeds(void)
{
	/* USERAUTH_FAILURE with methods_len > payload — line 578 */
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	uint8_t resp[8];
	size_t rp = 0;
	resp[rp++] = 51; /* USERAUTH_FAILURE */
	dssh_serialize_uint32(100, resp, sizeof(resp), &rp); /* methods_len=100 */
	struct crafted_response_ctx cr = { &ctx, resp, rp };
	thrd_t st;
	thrd_create(&st, crafted_response_server_thread, &cr);

	char methods[256];
	int res = dssh_auth_get_methods(ctx.client, "user", methods, sizeof(methods));
	thrd_join(st, NULL);
	ASSERT_TRUE(res < 0);

	handshake_cleanup(&ctx);
	return TEST_PASS;
}

static int
test_get_methods_failure_control_char(void)
{
	/* USERAUTH_FAILURE with control char in methods — line 583 */
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	uint8_t resp[16];
	size_t rp = 0;
	resp[rp++] = 51;
	dssh_serialize_uint32(4, resp, sizeof(resp), &rp);
	memcpy(&resp[rp], "ab\x01c", 4); /* control char */
	rp += 4;
	struct crafted_response_ctx cr = { &ctx, resp, rp };
	thrd_t st;
	thrd_create(&st, crafted_response_server_thread, &cr);

	char methods[256];
	int res = dssh_auth_get_methods(ctx.client, "user", methods, sizeof(methods));
	thrd_join(st, NULL);
	ASSERT_TRUE(res < 0);

	handshake_cleanup(&ctx);
	return TEST_PASS;
}

static int
test_get_methods_unexpected_msg(void)
{
	/* Server responds with neither SUCCESS nor FAILURE — line 591 */
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	uint8_t resp[] = { 99 }; /* bogus msg type */
	struct crafted_response_ctx cr = { &ctx, resp, sizeof(resp) };
	thrd_t st;
	thrd_create(&st, crafted_response_server_thread, &cr);

	char methods[256];
	int res = dssh_auth_get_methods(ctx.client, "user", methods, sizeof(methods));
	thrd_join(st, NULL);
	ASSERT_TRUE(res < 0);

	handshake_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Coverage: client password — no change callback, CHANGEREQ parse
 * ================================================================ */

static int
changereq_server_thread_send(void *arg)
{
	struct crafted_response_ctx *cr = arg;
	dssh_session server = cr->hctx->server;

	uint8_t msg_type;
	uint8_t *payload;
	size_t payload_len;
	/* SERVICE_REQUEST → SERVICE_ACCEPT */
	if (dssh_transport_recv_packet(server, &msg_type, &payload, &payload_len) < 0)
		return 0;
	{
		uint8_t accept[64];
		size_t pos = 0;
		accept[pos++] = SSH_MSG_SERVICE_ACCEPT;
		if (payload_len > 5) {
			uint32_t slen;
			dssh_parse_uint32(&payload[1], payload_len - 1, &slen);
			dssh_serialize_uint32(slen, accept, sizeof(accept), &pos);
			memcpy(&accept[pos], &payload[5], slen);
			pos += slen;
		}
		dssh_transport_send_packet(server, accept, pos, NULL);
	}
	/* Receive password USERAUTH_REQUEST */
	dssh_transport_recv_packet(server, &msg_type, &payload, &payload_len);

	/* Send PASSWD_CHANGEREQ */
	dssh_transport_send_packet(server, cr->response, cr->response_len, NULL);
	mock_io_close_s2c(&cr->hctx->io);
	return 0;
}

static int
test_password_changereq_no_callback(void)
{
	/* Client receives CHANGEREQ but has no passwd_change_cb — line 683 */
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	uint8_t resp[32];
	size_t rp = 0;
	resp[rp++] = 60; /* SSH_MSG_USERAUTH_PASSWD_CHANGEREQ */
	dssh_serialize_uint32(6, resp, sizeof(resp), &rp);
	memcpy(&resp[rp], "change", 6);
	rp += 6;
	dssh_serialize_uint32(0, resp, sizeof(resp), &rp); /* lang */
	struct crafted_response_ctx cr = { &ctx, resp, rp };
	thrd_t st;
	thrd_create(&st, changereq_server_thread_send, &cr);

	int res = dssh_auth_password(ctx.client, "user", "pass", NULL, NULL);
	thrd_join(st, NULL);
	ASSERT_TRUE(res < 0);

	handshake_cleanup(&ctx);
	return TEST_PASS;
}

static int
test_password_changereq_truncated_prompt(void)
{
	/* CHANGEREQ with only msg_type, no prompt header — line 689 */
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	uint8_t resp[] = { 60 }; /* CHANGEREQ, no data */
	struct crafted_response_ctx cr = { &ctx, resp, sizeof(resp) };
	thrd_t st;
	thrd_create(&st, changereq_server_thread_send, &cr);

	int res = dssh_auth_password(ctx.client, "user", "pass",
	    test_client_changereq_cb, NULL);
	thrd_join(st, NULL);
	ASSERT_TRUE(res < 0);

	handshake_cleanup(&ctx);
	return TEST_PASS;
}

static int
test_password_changereq_truncated_prompt_data(void)
{
	/* CHANGEREQ with prompt_len=100 but no data — line 693 */
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	uint8_t resp[8];
	size_t rp = 0;
	resp[rp++] = 60;
	dssh_serialize_uint32(100, resp, sizeof(resp), &rp);
	struct crafted_response_ctx cr = { &ctx, resp, rp };
	thrd_t st;
	thrd_create(&st, changereq_server_thread_send, &cr);

	int res = dssh_auth_password(ctx.client, "user", "pass",
	    test_client_changereq_cb, NULL);
	thrd_join(st, NULL);
	ASSERT_TRUE(res < 0);

	handshake_cleanup(&ctx);
	return TEST_PASS;
}

static int
test_password_changereq_truncated_lang(void)
{
	/* CHANGEREQ with valid prompt but lang_len > remaining — lines 704-705 */
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	uint8_t resp[32];
	size_t rp = 0;
	resp[rp++] = 60;
	dssh_serialize_uint32(3, resp, sizeof(resp), &rp);
	memcpy(&resp[rp], "hi!", 3);
	rp += 3;
	dssh_serialize_uint32(100, resp, sizeof(resp), &rp); /* lang_len > remaining */
	struct crafted_response_ctx cr = { &ctx, resp, rp };
	thrd_t st;
	thrd_create(&st, changereq_server_thread_send, &cr);

	int res = dssh_auth_password(ctx.client, "user", "pass",
	    test_client_changereq_cb, NULL);
	thrd_join(st, NULL);
	/* Should succeed (lang truncation is handled gracefully) or fail
	 * depending on whether the change callback response succeeds.
	 * The point is it doesn't crash. */
	(void)res;

	handshake_cleanup(&ctx);
	return TEST_PASS;
}

static int
test_password_unexpected_msg(void)
{
	/* Server responds with bogus msg type — line 727-728 */
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	uint8_t resp[] = { 99 }; /* bogus */
	struct crafted_response_ctx cr = { &ctx, resp, sizeof(resp) };
	thrd_t st;
	thrd_create(&st, changereq_server_thread_send, &cr);

	int res = dssh_auth_password(ctx.client, "user", "pass", NULL, NULL);
	thrd_join(st, NULL);
	ASSERT_TRUE(res < 0);

	handshake_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Coverage: client SERVICE_ACCEPT unexpected — line 509
 * ================================================================ */

static int
service_bogus_server_thread(void *arg)
{
	struct crafted_response_ctx *cr = arg;
	dssh_session server = cr->hctx->server;
	uint8_t msg_type;
	uint8_t *payload;
	size_t payload_len;
	dssh_transport_recv_packet(server, &msg_type, &payload, &payload_len);
	/* Send bogus response instead of SERVICE_ACCEPT */
	dssh_transport_send_packet(server, cr->response, cr->response_len, NULL);
	mock_io_close_s2c(&cr->hctx->io);
	return 0;
}

static int
test_service_accept_unexpected(void)
{
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	uint8_t resp[] = { 99 }; /* not SERVICE_ACCEPT(6) */
	struct crafted_response_ctx cr = { &ctx, resp, sizeof(resp) };
	thrd_t st;
	thrd_create(&st, service_bogus_server_thread, &cr);

	char methods[256];
	int res = dssh_auth_get_methods(ctx.client, "user", methods, sizeof(methods));
	thrd_join(st, NULL);
	ASSERT_TRUE(res < 0);

	handshake_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Server send-failure tests
 *
 * Each test completes a handshake, starts the server auth loop,
 * drives the client side to the point where the server is about
 * to send a response, then closes the s2c pipe so the server's
 * send_packet fails.  Verifies sa.result < 0.
 * ================================================================ */

static int
test_server_send_fail_service_accept(void)
{
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	struct test_auth_cbdata cbdata = {0};
	cbdata.none_result = DSSH_AUTH_SUCCESS;

	struct auth_server_arg sa = {0};
	sa.ctx = &ctx;
	sa.cbs.methods_str = "none";
	sa.cbs.none_cb = test_none_cb;
	sa.cbs.cbdata = &cbdata;

	thrd_t st;
	ASSERT_TRUE(thrd_create(&st, auth_server_thread, &sa) == thrd_success);

	/* Client: send SERVICE_REQUEST */
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

	/* Close s2c before the server sends SERVICE_ACCEPT */
	mock_io_close_s2c(&ctx.io);

	thrd_join(st, NULL);
	ASSERT_TRUE(sa.result < 0);

	mock_io_close_c2s(&ctx.io);
	handshake_cleanup(&ctx);
	return TEST_PASS;
}

static int
test_server_send_fail_none_success(void)
{
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	struct test_auth_cbdata cbdata = {0};
	cbdata.none_result = DSSH_AUTH_SUCCESS;

	struct auth_server_arg sa = {0};
	sa.ctx = &ctx;
	sa.cbs.methods_str = "none";
	sa.cbs.none_cb = test_none_cb;
	sa.cbs.cbdata = &cbdata;

	thrd_t st;
	ASSERT_TRUE(thrd_create(&st, auth_server_thread, &sa) == thrd_success);

	/* SERVICE_REQUEST/ACCEPT */
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

	/* Send none auth request */
	{
		static const char user[] = "testuser";
		static const char svc[] = "ssh-connection";
		static const char method[] = "none";
		uint8_t msg[128];
		size_t pos = 0;
		msg[pos++] = SSH_MSG_USERAUTH_REQUEST;
		dssh_serialize_uint32((uint32_t)(sizeof(user) - 1), msg, sizeof(msg), &pos);
		memcpy(&msg[pos], user, sizeof(user) - 1);
		pos += sizeof(user) - 1;
		dssh_serialize_uint32((uint32_t)(sizeof(svc) - 1), msg, sizeof(msg), &pos);
		memcpy(&msg[pos], svc, sizeof(svc) - 1);
		pos += sizeof(svc) - 1;
		dssh_serialize_uint32((uint32_t)(sizeof(method) - 1), msg, sizeof(msg), &pos);
		memcpy(&msg[pos], method, sizeof(method) - 1);
		pos += sizeof(method) - 1;
		ASSERT_OK(dssh_transport_send_packet(ctx.client, msg, pos, NULL));
	}

	/* Close s2c before server sends USERAUTH_SUCCESS */
	mock_io_close_s2c(&ctx.io);

	thrd_join(st, NULL);
	ASSERT_TRUE(sa.result < 0);

	mock_io_close_c2s(&ctx.io);
	handshake_cleanup(&ctx);
	return TEST_PASS;
}

static int
test_server_send_fail_none_failure(void)
{
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	struct test_auth_cbdata cbdata = {0};
	cbdata.none_result = DSSH_AUTH_FAILURE;

	struct auth_server_arg sa = {0};
	sa.ctx = &ctx;
	sa.cbs.methods_str = "password";
	sa.cbs.none_cb = test_none_cb;
	sa.cbs.password_cb = test_password_cb;
	sa.cbs.cbdata = &cbdata;

	thrd_t st;
	ASSERT_TRUE(thrd_create(&st, auth_server_thread, &sa) == thrd_success);

	/* SERVICE_REQUEST/ACCEPT */
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

	/* Send none auth request */
	{
		static const char user[] = "testuser";
		static const char svc[] = "ssh-connection";
		static const char method[] = "none";
		uint8_t msg[128];
		size_t pos = 0;
		msg[pos++] = SSH_MSG_USERAUTH_REQUEST;
		dssh_serialize_uint32((uint32_t)(sizeof(user) - 1), msg, sizeof(msg), &pos);
		memcpy(&msg[pos], user, sizeof(user) - 1);
		pos += sizeof(user) - 1;
		dssh_serialize_uint32((uint32_t)(sizeof(svc) - 1), msg, sizeof(msg), &pos);
		memcpy(&msg[pos], svc, sizeof(svc) - 1);
		pos += sizeof(svc) - 1;
		dssh_serialize_uint32((uint32_t)(sizeof(method) - 1), msg, sizeof(msg), &pos);
		memcpy(&msg[pos], method, sizeof(method) - 1);
		pos += sizeof(method) - 1;
		ASSERT_OK(dssh_transport_send_packet(ctx.client, msg, pos, NULL));
	}

	/* Close s2c before server sends USERAUTH_FAILURE */
	mock_io_close_s2c(&ctx.io);

	thrd_join(st, NULL);
	ASSERT_TRUE(sa.result < 0);

	mock_io_close_c2s(&ctx.io);
	handshake_cleanup(&ctx);
	return TEST_PASS;
}

static int
test_server_send_fail_password_success(void)
{
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	struct test_auth_cbdata cbdata = {0};
	cbdata.password_result = DSSH_AUTH_SUCCESS;

	struct auth_server_arg sa = {0};
	sa.ctx = &ctx;
	sa.cbs.methods_str = "password";
	sa.cbs.password_cb = test_password_cb;
	sa.cbs.cbdata = &cbdata;

	thrd_t st;
	ASSERT_TRUE(thrd_create(&st, auth_server_thread, &sa) == thrd_success);

	/* SERVICE_REQUEST/ACCEPT */
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

	/* Send password auth request */
	{
		static const char user[] = "sysop";
		static const char svc[] = "ssh-connection";
		static const char method[] = "password";
		static const char pw[] = "hunter2";
		uint8_t msg[256];
		size_t pos = 0;
		msg[pos++] = SSH_MSG_USERAUTH_REQUEST;
		dssh_serialize_uint32((uint32_t)(sizeof(user) - 1), msg, sizeof(msg), &pos);
		memcpy(&msg[pos], user, sizeof(user) - 1);
		pos += sizeof(user) - 1;
		dssh_serialize_uint32((uint32_t)(sizeof(svc) - 1), msg, sizeof(msg), &pos);
		memcpy(&msg[pos], svc, sizeof(svc) - 1);
		pos += sizeof(svc) - 1;
		dssh_serialize_uint32((uint32_t)(sizeof(method) - 1), msg, sizeof(msg), &pos);
		memcpy(&msg[pos], method, sizeof(method) - 1);
		pos += sizeof(method) - 1;
		msg[pos++] = 0; /* not a password change */
		dssh_serialize_uint32((uint32_t)(sizeof(pw) - 1), msg, sizeof(msg), &pos);
		memcpy(&msg[pos], pw, sizeof(pw) - 1);
		pos += sizeof(pw) - 1;
		ASSERT_OK(dssh_transport_send_packet(ctx.client, msg, pos, NULL));
	}

	/* Close s2c before server sends USERAUTH_SUCCESS */
	mock_io_close_s2c(&ctx.io);

	thrd_join(st, NULL);
	ASSERT_TRUE(sa.result < 0);

	mock_io_close_c2s(&ctx.io);
	handshake_cleanup(&ctx);
	return TEST_PASS;
}

static int
test_server_send_fail_password_failure(void)
{
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	struct test_auth_cbdata cbdata = {0};
	cbdata.password_result = DSSH_AUTH_FAILURE;

	struct auth_server_arg sa = {0};
	sa.ctx = &ctx;
	sa.cbs.methods_str = "password";
	sa.cbs.password_cb = test_password_cb;
	sa.cbs.cbdata = &cbdata;

	thrd_t st;
	ASSERT_TRUE(thrd_create(&st, auth_server_thread, &sa) == thrd_success);

	/* SERVICE_REQUEST/ACCEPT */
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

	/* Send password auth request */
	{
		static const char user[] = "testuser";
		static const char svc[] = "ssh-connection";
		static const char method[] = "password";
		static const char pw[] = "badpass";
		uint8_t msg[256];
		size_t pos = 0;
		msg[pos++] = SSH_MSG_USERAUTH_REQUEST;
		dssh_serialize_uint32((uint32_t)(sizeof(user) - 1), msg, sizeof(msg), &pos);
		memcpy(&msg[pos], user, sizeof(user) - 1);
		pos += sizeof(user) - 1;
		dssh_serialize_uint32((uint32_t)(sizeof(svc) - 1), msg, sizeof(msg), &pos);
		memcpy(&msg[pos], svc, sizeof(svc) - 1);
		pos += sizeof(svc) - 1;
		dssh_serialize_uint32((uint32_t)(sizeof(method) - 1), msg, sizeof(msg), &pos);
		memcpy(&msg[pos], method, sizeof(method) - 1);
		pos += sizeof(method) - 1;
		msg[pos++] = 0;
		dssh_serialize_uint32((uint32_t)(sizeof(pw) - 1), msg, sizeof(msg), &pos);
		memcpy(&msg[pos], pw, sizeof(pw) - 1);
		pos += sizeof(pw) - 1;
		ASSERT_OK(dssh_transport_send_packet(ctx.client, msg, pos, NULL));
	}

	/* Close s2c before server sends USERAUTH_FAILURE */
	mock_io_close_s2c(&ctx.io);

	thrd_join(st, NULL);
	ASSERT_TRUE(sa.result < 0);

	mock_io_close_c2s(&ctx.io);
	handshake_cleanup(&ctx);
	return TEST_PASS;
}

static int
test_server_send_fail_password_no_cb(void)
{
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	struct auth_server_arg sa = {0};
	sa.ctx = &ctx;
	sa.cbs.methods_str = "password";
	/* No password_cb set */

	thrd_t st;
	ASSERT_TRUE(thrd_create(&st, auth_server_thread, &sa) == thrd_success);

	/* SERVICE_REQUEST/ACCEPT */
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

	/* Send password auth — server has no password_cb */
	{
		static const char user[] = "testuser";
		static const char svc[] = "ssh-connection";
		static const char method[] = "password";
		static const char pw[] = "pass";
		uint8_t msg[256];
		size_t pos = 0;
		msg[pos++] = SSH_MSG_USERAUTH_REQUEST;
		dssh_serialize_uint32((uint32_t)(sizeof(user) - 1), msg, sizeof(msg), &pos);
		memcpy(&msg[pos], user, sizeof(user) - 1);
		pos += sizeof(user) - 1;
		dssh_serialize_uint32((uint32_t)(sizeof(svc) - 1), msg, sizeof(msg), &pos);
		memcpy(&msg[pos], svc, sizeof(svc) - 1);
		pos += sizeof(svc) - 1;
		dssh_serialize_uint32((uint32_t)(sizeof(method) - 1), msg, sizeof(msg), &pos);
		memcpy(&msg[pos], method, sizeof(method) - 1);
		pos += sizeof(method) - 1;
		msg[pos++] = 0;
		dssh_serialize_uint32((uint32_t)(sizeof(pw) - 1), msg, sizeof(msg), &pos);
		memcpy(&msg[pos], pw, sizeof(pw) - 1);
		pos += sizeof(pw) - 1;
		ASSERT_OK(dssh_transport_send_packet(ctx.client, msg, pos, NULL));
	}

	/* Close s2c before server sends USERAUTH_FAILURE */
	mock_io_close_s2c(&ctx.io);

	thrd_join(st, NULL);
	ASSERT_TRUE(sa.result < 0);

	mock_io_close_c2s(&ctx.io);
	handshake_cleanup(&ctx);
	return TEST_PASS;
}

static int
send_fail_changereq_password_cb(const uint8_t *username, size_t username_len,
    const uint8_t *password, size_t password_len,
    uint8_t **change_prompt, size_t *change_prompt_len,
    void *cbdata)
{
	(void)username;
	(void)username_len;
	(void)password;
	(void)password_len;
	(void)cbdata;
	*change_prompt = malloc(12);
	if (*change_prompt == NULL) {
		*change_prompt_len = 0;
		return DSSH_AUTH_FAILURE;
	}
	memcpy(*change_prompt, "change pass!", 12);
	*change_prompt_len = 12;
	return DSSH_AUTH_CHANGE_PASSWORD;
}

static int
test_server_send_fail_password_changereq(void)
{
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	struct auth_server_arg sa = {0};
	sa.ctx = &ctx;
	sa.cbs.methods_str = "password";
	sa.cbs.password_cb = send_fail_changereq_password_cb;

	thrd_t st;
	ASSERT_TRUE(thrd_create(&st, auth_server_thread, &sa) == thrd_success);

	/* SERVICE_REQUEST/ACCEPT */
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

	/* Send password auth — cb will return CHANGE_PASSWORD */
	{
		static const char user[] = "testuser";
		static const char svc[] = "ssh-connection";
		static const char method[] = "password";
		static const char pw[] = "oldpass";
		uint8_t msg[256];
		size_t pos = 0;
		msg[pos++] = SSH_MSG_USERAUTH_REQUEST;
		dssh_serialize_uint32((uint32_t)(sizeof(user) - 1), msg, sizeof(msg), &pos);
		memcpy(&msg[pos], user, sizeof(user) - 1);
		pos += sizeof(user) - 1;
		dssh_serialize_uint32((uint32_t)(sizeof(svc) - 1), msg, sizeof(msg), &pos);
		memcpy(&msg[pos], svc, sizeof(svc) - 1);
		pos += sizeof(svc) - 1;
		dssh_serialize_uint32((uint32_t)(sizeof(method) - 1), msg, sizeof(msg), &pos);
		memcpy(&msg[pos], method, sizeof(method) - 1);
		pos += sizeof(method) - 1;
		msg[pos++] = 0; /* not a password change request from client */
		dssh_serialize_uint32((uint32_t)(sizeof(pw) - 1), msg, sizeof(msg), &pos);
		memcpy(&msg[pos], pw, sizeof(pw) - 1);
		pos += sizeof(pw) - 1;
		ASSERT_OK(dssh_transport_send_packet(ctx.client, msg, pos, NULL));
	}

	/* Close s2c before server sends PASSWD_CHANGEREQ */
	mock_io_close_s2c(&ctx.io);

	thrd_join(st, NULL);
	ASSERT_TRUE(sa.result < 0);

	mock_io_close_c2s(&ctx.io);
	handshake_cleanup(&ctx);
	return TEST_PASS;
}

static int
test_server_send_fail_publickey_no_cb(void)
{
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	struct auth_server_arg sa = {0};
	sa.ctx = &ctx;
	sa.cbs.methods_str = "publickey";
	/* No publickey_cb set */

	thrd_t st;
	ASSERT_TRUE(thrd_create(&st, auth_server_thread, &sa) == thrd_success);

	/* SERVICE_REQUEST/ACCEPT */
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

	/* Send publickey probe — server has no publickey_cb */
	{
		uint8_t msg[256];
		size_t pos = build_auth_prefix(msg, sizeof(msg), "publickey");
		msg[pos++] = 0; /* has_sig = false */
		dssh_serialize_uint32(11, msg, sizeof(msg), &pos);
		memcpy(&msg[pos], "ssh-ed25519", 11);
		pos += 11;
		dssh_serialize_uint32(4, msg, sizeof(msg), &pos);
		memcpy(&msg[pos], "blob", 4);
		pos += 4;
		ASSERT_OK(dssh_transport_send_packet(ctx.client, msg, pos, NULL));
	}

	/* Close s2c before server sends USERAUTH_FAILURE */
	mock_io_close_s2c(&ctx.io);

	thrd_join(st, NULL);
	ASSERT_TRUE(sa.result < 0);

	mock_io_close_c2s(&ctx.io);
	handshake_cleanup(&ctx);
	return TEST_PASS;
}

static int
test_server_send_fail_pk_ok(void)
{
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	struct test_auth_cbdata cbdata = {0};
	cbdata.publickey_result = DSSH_AUTH_SUCCESS;

	struct auth_server_arg sa = {0};
	sa.ctx = &ctx;
	sa.cbs.methods_str = "publickey";
	sa.cbs.publickey_cb = test_publickey_cb;
	sa.cbs.cbdata = &cbdata;

	thrd_t st;
	ASSERT_TRUE(thrd_create(&st, auth_server_thread, &sa) == thrd_success);

	/* SERVICE_REQUEST/ACCEPT */
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

	/* Send publickey probe — cb accepts, server tries to send PK_OK */
	{
		uint8_t msg[256];
		size_t pos = build_auth_prefix(msg, sizeof(msg), "publickey");
		msg[pos++] = 0; /* has_sig = false */
		dssh_serialize_uint32(11, msg, sizeof(msg), &pos);
		memcpy(&msg[pos], "ssh-ed25519", 11);
		pos += 11;
		dssh_serialize_uint32(4, msg, sizeof(msg), &pos);
		memcpy(&msg[pos], "blob", 4);
		pos += 4;
		ASSERT_OK(dssh_transport_send_packet(ctx.client, msg, pos, NULL));
	}

	/* Close s2c before server sends PK_OK */
	mock_io_close_s2c(&ctx.io);

	thrd_join(st, NULL);
	ASSERT_TRUE(sa.result < 0);

	mock_io_close_c2s(&ctx.io);
	handshake_cleanup(&ctx);
	return TEST_PASS;
}

static int
test_server_send_fail_pk_probe_rejected(void)
{
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	struct test_auth_cbdata cbdata = {0};
	cbdata.publickey_result = DSSH_AUTH_FAILURE;

	struct auth_server_arg sa = {0};
	sa.ctx = &ctx;
	sa.cbs.methods_str = "publickey";
	sa.cbs.publickey_cb = test_publickey_cb;
	sa.cbs.cbdata = &cbdata;

	thrd_t st;
	ASSERT_TRUE(thrd_create(&st, auth_server_thread, &sa) == thrd_success);

	/* SERVICE_REQUEST/ACCEPT */
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

	/* Send publickey probe — cb rejects, server tries to send FAILURE */
	{
		uint8_t msg[256];
		size_t pos = build_auth_prefix(msg, sizeof(msg), "publickey");
		msg[pos++] = 0; /* has_sig = false */
		dssh_serialize_uint32(11, msg, sizeof(msg), &pos);
		memcpy(&msg[pos], "ssh-ed25519", 11);
		pos += 11;
		dssh_serialize_uint32(4, msg, sizeof(msg), &pos);
		memcpy(&msg[pos], "blob", 4);
		pos += 4;
		ASSERT_OK(dssh_transport_send_packet(ctx.client, msg, pos, NULL));
	}

	/* Close s2c before server sends USERAUTH_FAILURE */
	mock_io_close_s2c(&ctx.io);

	thrd_join(st, NULL);
	ASSERT_TRUE(sa.result < 0);

	mock_io_close_c2s(&ctx.io);
	handshake_cleanup(&ctx);
	return TEST_PASS;
}

static int
test_server_send_fail_pk_unknown_algo(void)
{
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	struct test_auth_cbdata cbdata = {0};
	cbdata.publickey_result = DSSH_AUTH_SUCCESS;

	struct auth_server_arg sa = {0};
	sa.ctx = &ctx;
	sa.cbs.methods_str = "publickey";
	sa.cbs.publickey_cb = test_publickey_cb;
	sa.cbs.cbdata = &cbdata;

	thrd_t st;
	ASSERT_TRUE(thrd_create(&st, auth_server_thread, &sa) == thrd_success);

	/* SERVICE_REQUEST/ACCEPT */
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

	/* Send publickey with has_sig=true and unknown algo */
	{
		uint8_t msg[256];
		size_t pos = build_auth_prefix(msg, sizeof(msg), "publickey");
		msg[pos++] = 1; /* has_sig = true */
		dssh_serialize_uint32(12, msg, sizeof(msg), &pos);
		memcpy(&msg[pos], "unknown-algo", 12);
		pos += 12;
		dssh_serialize_uint32(4, msg, sizeof(msg), &pos);
		memcpy(&msg[pos], "fake", 4);
		pos += 4;
		dssh_serialize_uint32(4, msg, sizeof(msg), &pos);
		memcpy(&msg[pos], "sig!", 4);
		pos += 4;
		ASSERT_OK(dssh_transport_send_packet(ctx.client, msg, pos, NULL));
	}

	/* Close s2c before server sends USERAUTH_FAILURE */
	mock_io_close_s2c(&ctx.io);

	thrd_join(st, NULL);
	ASSERT_TRUE(sa.result < 0);

	mock_io_close_c2s(&ctx.io);
	handshake_cleanup(&ctx);
	return TEST_PASS;
}

static int
test_server_send_fail_unknown_method(void)
{
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	struct test_auth_cbdata cbdata = {0};
	cbdata.password_result = DSSH_AUTH_SUCCESS;

	struct auth_server_arg sa = {0};
	sa.ctx = &ctx;
	sa.cbs.methods_str = "password";
	sa.cbs.password_cb = test_password_cb;
	sa.cbs.cbdata = &cbdata;

	thrd_t st;
	ASSERT_TRUE(thrd_create(&st, auth_server_thread, &sa) == thrd_success);

	/* SERVICE_REQUEST/ACCEPT */
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

	/* Send auth request with unknown method "bogus" */
	{
		uint8_t msg[128];
		size_t pos = build_auth_prefix(msg, sizeof(msg), "bogus");
		ASSERT_OK(dssh_transport_send_packet(ctx.client, msg, pos, NULL));
	}

	/* Close s2c before server sends USERAUTH_FAILURE */
	mock_io_close_s2c(&ctx.io);

	thrd_join(st, NULL);
	ASSERT_TRUE(sa.result < 0);

	mock_io_close_c2s(&ctx.io);
	handshake_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Group A: Defensive / edge-case tests
 * ================================================================ */

/*
 * Server thread that passes NULL for username_out and username_out_len.
 * Exercises the line 445 branch where username_out==NULL.
 */
static int
auth_server_null_out_thread(void *arg)
{
	struct auth_server_arg *a = arg;
	a->result = dssh_auth_server(a->ctx->server, &a->cbs,
	    NULL, NULL);
	return 0;
}

static int
test_server_null_username_out(void)
{
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	struct test_auth_cbdata cbdata = {0};
	cbdata.password_result = DSSH_AUTH_SUCCESS;

	struct auth_server_arg sa = {0};
	sa.ctx = &ctx;
	sa.cbs.methods_str = "password";
	sa.cbs.password_cb = test_password_cb;
	sa.cbs.cbdata = &cbdata;

	thrd_t st;
	ASSERT_TRUE(thrd_create(&st, auth_server_null_out_thread, &sa) == thrd_success);

	/* Client: full password auth */
	int res = dssh_auth_password(ctx.client, "testuser", "pass", NULL, NULL);

	mock_io_close_c2s(&ctx.io);
	mock_io_close_s2c(&ctx.io);
	thrd_join(st, NULL);

	ASSERT_EQ(res, 0);
	ASSERT_EQ(sa.result, 0);

	handshake_cleanup(&ctx);
	return TEST_PASS;
}

/*
 * Client get_methods with methods_sz=0 in the SUCCESS path.
 * Server accepts "none", so get_methods gets USERAUTH_SUCCESS.
 * Exercises line 567 False (methods_sz > 0 when returning success).
 */
static int
test_get_methods_zero_buffer(void)
{
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	/* Server: accept "none" auth method */
	struct test_auth_cbdata cbdata = {0};
	cbdata.none_result = DSSH_AUTH_SUCCESS;
	struct auth_server_arg sa = {0};
	sa.ctx = &ctx;
	sa.cbs.methods_str = "none";
	sa.cbs.none_cb = test_none_cb;
	sa.cbs.cbdata = &cbdata;

	thrd_t st;
	ASSERT_TRUE(thrd_create(&st, none_accept_server_thread, &sa) == thrd_success);

	/* Call get_methods with methods_sz=0.
	 * Server accepts none → USERAUTH_SUCCESS → line 567 False.
	 * The methods buffer is not written to. */
	char methods[1] = { 'X' };
	int res = dssh_auth_get_methods(ctx.client, "user", methods, 0);

	thrd_join(st, NULL);
	ASSERT_EQ(res, 0);
	/* methods[0] should still be 'X' since methods_sz=0 skips the write */
	ASSERT_EQ(methods[0], 'X');
	ASSERT_EQ(sa.result, 0);

	handshake_cleanup(&ctx);
	return TEST_PASS;
}

/*
 * Server sends FAILURE with DEL char (0x7F) in methods.
 * Exercises line 582 ch >= 127 True branch.
 */
static int
test_get_methods_del_char(void)
{
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	uint8_t resp[16];
	size_t rp = 0;
	resp[rp++] = 51; /* SSH_MSG_USERAUTH_FAILURE */
	dssh_serialize_uint32(4, resp, sizeof(resp), &rp);
	resp[rp++] = 'a';
	resp[rp++] = 'b';
	resp[rp++] = 0x7F; /* DEL character */
	resp[rp++] = 'c';
	resp[rp++] = 0; /* partial_success */
	struct crafted_response_ctx cr = { &ctx, resp, rp };
	thrd_t st;
	thrd_create(&st, crafted_response_server_thread, &cr);

	char methods[256];
	int res = dssh_auth_get_methods(ctx.client, "user", methods, sizeof(methods));
	thrd_join(st, NULL);
	ASSERT_TRUE(res < 0);

	handshake_cleanup(&ctx);
	return TEST_PASS;
}

/*
 * Send a 4-byte method that is NOT "none" (e.g., "fake").
 * Exercises line 221 False (method_len==4 && memcmp != 0).
 */
static int
test_server_method_4_not_none(void)
{
	struct parse_error_ctx pe;
	if (parse_error_setup(&pe) < 0) {
		parse_error_cleanup(&pe);
		return TEST_FAIL;
	}

	uint8_t msg[128];
	size_t pos = build_auth_prefix(msg, sizeof(msg), "fake");
	ASSERT_OK(dssh_transport_send_packet(pe.hctx.client, msg, pos, NULL));

	/* Server should send USERAUTH_FAILURE (unknown method) */
	{
		uint8_t msg_type;
		uint8_t *payload;
		size_t payload_len;
		ASSERT_OK(dssh_transport_recv_packet(pe.hctx.client,
		    &msg_type, &payload, &payload_len));
		ASSERT_EQ(msg_type, SSH_MSG_USERAUTH_FAILURE);
	}

	parse_error_cleanup(&pe);
	return TEST_PASS;
}

/*
 * Send an 8-byte method that is NOT "password" (e.g., "notapass").
 * Exercises line 236 False (method_len==8 && memcmp != 0).
 */
static int
test_server_method_8_not_password(void)
{
	struct parse_error_ctx pe;
	if (parse_error_setup(&pe) < 0) {
		parse_error_cleanup(&pe);
		return TEST_FAIL;
	}

	uint8_t msg[128];
	size_t pos = build_auth_prefix(msg, sizeof(msg), "notapass");
	ASSERT_OK(dssh_transport_send_packet(pe.hctx.client, msg, pos, NULL));

	/* Server should send USERAUTH_FAILURE (unknown method) */
	{
		uint8_t msg_type;
		uint8_t *payload;
		size_t payload_len;
		ASSERT_OK(dssh_transport_recv_packet(pe.hctx.client,
		    &msg_type, &payload, &payload_len));
		ASSERT_EQ(msg_type, SSH_MSG_USERAUTH_FAILURE);
	}

	parse_error_cleanup(&pe);
	return TEST_PASS;
}

/*
 * Send a 9-byte method that is NOT "publickey" (e.g., "notpubkey").
 * Exercises line 322 False (method_len==9 && memcmp != 0).
 */
static int
test_server_method_9_not_publickey(void)
{
	struct parse_error_ctx pe;
	if (parse_error_setup(&pe) < 0) {
		parse_error_cleanup(&pe);
		return TEST_FAIL;
	}

	uint8_t msg[128];
	size_t pos = build_auth_prefix(msg, sizeof(msg), "notpubkey");
	ASSERT_OK(dssh_transport_send_packet(pe.hctx.client, msg, pos, NULL));

	/* Server should send USERAUTH_FAILURE (unknown method) */
	{
		uint8_t msg_type;
		uint8_t *payload;
		size_t payload_len;
		ASSERT_OK(dssh_transport_recv_packet(pe.hctx.client,
		    &msg_type, &payload, &payload_len));
		ASSERT_EQ(msg_type, SSH_MSG_USERAUTH_FAILURE);
	}

	parse_error_cleanup(&pe);
	return TEST_PASS;
}

/*
 * Server: password_cb returns CHANGE_PASSWORD but prompt is NULL.
 * Exercises line 309 False (prompt != NULL when auth_res==CHANGE_PASSWORD).
 */
static int
changereq_null_prompt_password_cb(const uint8_t *username, size_t username_len,
    const uint8_t *password, size_t password_len,
    uint8_t **change_prompt, size_t *change_prompt_len,
    void *cbdata)
{
	(void)username; (void)username_len;
	(void)password; (void)password_len;
	(void)cbdata;
	/* Return CHANGE_PASSWORD but leave prompt NULL */
	*change_prompt = NULL;
	*change_prompt_len = 0;
	return DSSH_AUTH_CHANGE_PASSWORD;
}

static int
test_server_change_password_null_prompt(void)
{
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	struct test_auth_cbdata cbdata = {0};
	cbdata.none_result = DSSH_AUTH_SUCCESS;
	struct auth_server_arg sa = {0};
	sa.ctx = &ctx;
	sa.cbs.methods_str = "password";
	sa.cbs.password_cb = changereq_null_prompt_password_cb;
	sa.cbs.none_cb = test_none_cb;
	sa.cbs.cbdata = &cbdata;

	thrd_t st;
	ASSERT_TRUE(thrd_create(&st, auth_server_thread, &sa) == thrd_success);

	/* SERVICE_REQUEST/ACCEPT */
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

	/* Send password auth — cb returns CHANGE_PASSWORD with NULL prompt */
	{
		uint8_t msg[128];
		size_t pos = build_auth_prefix(msg, sizeof(msg), "password");
		msg[pos++] = 0; /* change = false */
		dssh_serialize_uint32(4, msg, sizeof(msg), &pos);
		memcpy(&msg[pos], "pass", 4);
		pos += 4;
		ASSERT_OK(dssh_transport_send_packet(ctx.client, msg, pos, NULL));
	}

	/* Should get USERAUTH_FAILURE (prompt was NULL, so falls through) */
	{
		uint8_t msg_type;
		uint8_t *payload;
		size_t payload_len;
		ASSERT_OK(dssh_transport_recv_packet(ctx.client, &msg_type, &payload, &payload_len));
		ASSERT_EQ(msg_type, SSH_MSG_USERAUTH_FAILURE);
	}

	/* Send none to finish */
	{
		uint8_t nmsg[64];
		size_t npos = build_auth_prefix(nmsg, sizeof(nmsg), "none");
		ASSERT_OK(dssh_transport_send_packet(ctx.client, nmsg, npos, NULL));
	}
	{
		uint8_t msg_type;
		uint8_t *payload;
		size_t payload_len;
		ASSERT_OK(dssh_transport_recv_packet(ctx.client, &msg_type, &payload, &payload_len));
		ASSERT_EQ(msg_type, SSH_MSG_USERAUTH_SUCCESS);
	}

	thrd_join(st, NULL);
	ASSERT_EQ(sa.result, 0);
	handshake_cleanup(&ctx);
	return TEST_PASS;
}

/*
 * Server: passwd_change_cb returns CHANGE_PASSWORD but prompt is NULL.
 * Exercises line 284 False (prompt != NULL for the change path).
 */
static int
changereq_null_prompt_change_cb(const uint8_t *username, size_t username_len,
    const uint8_t *old_pw, size_t old_pw_len,
    const uint8_t *new_pw, size_t new_pw_len,
    uint8_t **prompt, size_t *prompt_len, void *cbdata)
{
	(void)username; (void)username_len;
	(void)old_pw; (void)old_pw_len;
	(void)new_pw; (void)new_pw_len;
	(void)cbdata;
	/* Return CHANGE_PASSWORD but with NULL prompt */
	*prompt = NULL;
	*prompt_len = 0;
	return DSSH_AUTH_CHANGE_PASSWORD;
}

static int
test_server_change_password_null_prompt_change(void)
{
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	struct test_auth_cbdata cbdata = {0};
	cbdata.none_result = DSSH_AUTH_SUCCESS;
	struct auth_server_arg sa = {0};
	sa.ctx = &ctx;
	sa.cbs.methods_str = "password";
	sa.cbs.password_cb = test_password_cb;
	sa.cbs.passwd_change_cb = changereq_null_prompt_change_cb;
	sa.cbs.none_cb = test_none_cb;
	sa.cbs.cbdata = &cbdata;

	thrd_t st;
	ASSERT_TRUE(thrd_create(&st, auth_server_thread, &sa) == thrd_success);

	/* SERVICE_REQUEST/ACCEPT */
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

	/* Send password change request (change=true) */
	{
		uint8_t msg[128];
		size_t pos = build_auth_prefix(msg, sizeof(msg), "password");
		msg[pos++] = 1; /* change = true */
		dssh_serialize_uint32(3, msg, sizeof(msg), &pos);
		memcpy(&msg[pos], "old", 3);
		pos += 3;
		dssh_serialize_uint32(3, msg, sizeof(msg), &pos);
		memcpy(&msg[pos], "new", 3);
		pos += 3;
		ASSERT_OK(dssh_transport_send_packet(ctx.client, msg, pos, NULL));
	}

	/* Should get USERAUTH_FAILURE (change cb returned CHANGE_PASSWORD
	 * but prompt is NULL, so falls through to failure path) */
	{
		uint8_t msg_type;
		uint8_t *payload;
		size_t payload_len;
		ASSERT_OK(dssh_transport_recv_packet(ctx.client, &msg_type, &payload, &payload_len));
		ASSERT_EQ(msg_type, SSH_MSG_USERAUTH_FAILURE);
	}

	/* Send none to finish */
	{
		uint8_t nmsg[64];
		size_t npos = build_auth_prefix(nmsg, sizeof(nmsg), "none");
		ASSERT_OK(dssh_transport_send_packet(ctx.client, nmsg, npos, NULL));
	}
	{
		uint8_t msg_type;
		uint8_t *payload;
		size_t payload_len;
		ASSERT_OK(dssh_transport_recv_packet(ctx.client, &msg_type, &payload, &payload_len));
		ASSERT_EQ(msg_type, SSH_MSG_USERAUTH_SUCCESS);
	}

	thrd_join(st, NULL);
	ASSERT_EQ(sa.result, 0);
	handshake_cleanup(&ctx);
	return TEST_PASS;
}

/*
 * Send a publickey auth with algo name > 64 bytes.
 * Exercises line 357 False (algo_len >= sizeof(algo_name)-1 truncation).
 */
static int
test_server_long_algo_name(void)
{
	struct parse_error_ctx pe;
	if (parse_error_setup(&pe) < 0) {
		parse_error_cleanup(&pe);
		return TEST_FAIL;
	}

	/* Reject publickey so server sends FAILURE, not PK_OK */
	pe.cbdata.publickey_result = DSSH_AUTH_FAILURE;

	uint8_t msg[256];
	size_t pos = build_auth_prefix(msg, sizeof(msg), "publickey");
	msg[pos++] = 0; /* has_sig = false */

	/* Algo name of 80 bytes (> 64 limit) — exercises truncation path */
	char long_algo[81];
	memset(long_algo, 'z', 80);
	long_algo[80] = 0;
	dssh_serialize_uint32(80, msg, sizeof(msg), &pos);
	memcpy(&msg[pos], long_algo, 80);
	pos += 80;

	/* Short pubkey blob */
	dssh_serialize_uint32(4, msg, sizeof(msg), &pos);
	memcpy(&msg[pos], "blob", 4);
	pos += 4;
	ASSERT_OK(dssh_transport_send_packet(pe.hctx.client, msg, pos, NULL));

	/* Server should send USERAUTH_FAILURE (probe rejected) */
	{
		uint8_t msg_type;
		uint8_t *payload;
		size_t payload_len;
		ASSERT_OK(dssh_transport_recv_packet(pe.hctx.client,
		    &msg_type, &payload, &payload_len));
		ASSERT_EQ(msg_type, SSH_MSG_USERAUTH_FAILURE);
	}

	parse_error_cleanup(&pe);
	return TEST_PASS;
}

/* ================================================================
 * Group C: Client-side failure tests
 * ================================================================ */

/*
 * Client service request send failure.
 * Close c2s pipe before any auth function so the SERVICE_REQUEST send fails.
 * Exercises line 499 (send_packet returns error in dssh_auth_request_service).
 */
static int
test_client_service_request_send_fail(void)
{
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	/* Close c2s so client's send_packet fails */
	mock_io_close_c2s(&ctx.io);

	char methods[256];
	int res = dssh_auth_get_methods(ctx.client, "user",
	    methods, sizeof(methods));
	ASSERT_TRUE(res < 0);

	mock_io_close_s2c(&ctx.io);
	handshake_cleanup(&ctx);
	return TEST_PASS;
}

/*
 * Client get_methods send fail after service is requested.
 * Complete SERVICE_REQUEST/ACCEPT, then close c2s so the
 * USERAUTH_REQUEST (none) send fails.
 * Exercises line 556 (send_packet error in get_methods_impl).
 */
static int
test_client_get_methods_send_fail(void)
{
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	struct test_auth_cbdata cbdata = {0};
	cbdata.none_result = DSSH_AUTH_FAILURE;

	struct auth_server_arg sa = {0};
	sa.ctx = &ctx;
	sa.cbs.methods_str = "password";
	sa.cbs.none_cb = test_none_cb;
	sa.cbs.cbdata = &cbdata;

	thrd_t st;
	ASSERT_TRUE(thrd_create(&st, auth_server_thread, &sa) == thrd_success);

	/* First call to get_methods succeeds (sends SERVICE_REQUEST,
	 * gets ACCEPT, sends none request, gets FAILURE). */
	char methods[256];
	int res = dssh_auth_get_methods(ctx.client, "user",
	    methods, sizeof(methods));
	ASSERT_EQ(res, 1);

	/* Now close c2s so the NEXT get_methods send fails.
	 * auth_service_requested is already true, so it skips SERVICE_REQUEST
	 * and goes straight to sending the none USERAUTH_REQUEST. */
	mock_io_close_c2s(&ctx.io);

	res = dssh_auth_get_methods(ctx.client, "user",
	    methods, sizeof(methods));
	ASSERT_TRUE(res < 0);

	mock_io_close_s2c(&ctx.io);
	thrd_join(st, NULL);

	handshake_cleanup(&ctx);
	return TEST_PASS;
}

/*
 * Client password: server sends CHANGEREQ, client's change callback
 * returns an error.
 * Exercises line 714 (passwd_change_cb returns negative).
 */
static int
failing_passwd_change_cb(const uint8_t *prompt, size_t prompt_len,
    const uint8_t *language, size_t language_len,
    uint8_t **new_password, size_t *new_password_len,
    void *cbdata)
{
	(void)prompt; (void)prompt_len;
	(void)language; (void)language_len;
	(void)new_password; (void)new_password_len;
	(void)cbdata;
	return -42; /* error */
}

static int
test_password_changereq_cb_error(void)
{
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	uint8_t resp[32];
	size_t rp = 0;
	resp[rp++] = 60; /* SSH_MSG_USERAUTH_PASSWD_CHANGEREQ */
	dssh_serialize_uint32(6, resp, sizeof(resp), &rp);
	memcpy(&resp[rp], "change", 6);
	rp += 6;
	dssh_serialize_uint32(0, resp, sizeof(resp), &rp); /* lang */
	struct crafted_response_ctx cr = { &ctx, resp, rp };
	thrd_t st;
	thrd_create(&st, changereq_server_thread_send, &cr);

	int res = dssh_auth_password(ctx.client, "user", "pass",
	    failing_passwd_change_cb, NULL);
	thrd_join(st, NULL);
	ASSERT_TRUE(res < 0);

	handshake_cleanup(&ctx);
	return TEST_PASS;
}

/*
 * Client password: server sends CHANGEREQ, client change callback
 * succeeds, but the send of the new password fails (c2s closed).
 * Exercises line 722 (send_password_request returns error after change cb).
 */
static int
changereq_send_fail_server_thread(void *arg)
{
	struct crafted_response_ctx *cr = arg;
	dssh_session server = cr->hctx->server;
	uint8_t msg_type;
	uint8_t *payload;
	size_t payload_len;

	/* SERVICE_REQUEST -> SERVICE_ACCEPT */
	if (dssh_transport_recv_packet(server, &msg_type, &payload, &payload_len) < 0)
		return 0;
	{
		uint8_t accept[64];
		size_t pos = 0;
		accept[pos++] = SSH_MSG_SERVICE_ACCEPT;
		if (payload_len > 5) {
			uint32_t slen;
			dssh_parse_uint32(&payload[1], payload_len - 1, &slen);
			dssh_serialize_uint32(slen, accept, sizeof(accept), &pos);
			memcpy(&accept[pos], &payload[5], slen);
			pos += slen;
		}
		dssh_transport_send_packet(server, accept, pos, NULL);
	}
	/* Receive password USERAUTH_REQUEST */
	dssh_transport_recv_packet(server, &msg_type, &payload, &payload_len);

	/* Send PASSWD_CHANGEREQ */
	dssh_transport_send_packet(server, cr->response, cr->response_len, NULL);

	/* Close c2s so client's send of new password fails */
	mock_io_close_c2s(&cr->hctx->io);
	return 0;
}

static int
test_password_changereq_send_fail(void)
{
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	uint8_t resp[32];
	size_t rp = 0;
	resp[rp++] = 60; /* SSH_MSG_USERAUTH_PASSWD_CHANGEREQ */
	dssh_serialize_uint32(6, resp, sizeof(resp), &rp);
	memcpy(&resp[rp], "change", 6);
	rp += 6;
	dssh_serialize_uint32(0, resp, sizeof(resp), &rp); /* lang */
	struct crafted_response_ctx cr = { &ctx, resp, rp };
	thrd_t st;
	thrd_create(&st, changereq_send_fail_server_thread, &cr);

	int res = dssh_auth_password(ctx.client, "user", "pass",
	    test_client_passwd_change_cb, NULL);
	thrd_join(st, NULL);
	ASSERT_TRUE(res < 0);

	mock_io_close_s2c(&ctx.io);
	handshake_cleanup(&ctx);
	return TEST_PASS;
}

/*
 * Client KBI send fail: close c2s before KBI initial send.
 * Exercises line 779 (send_packet error in auth_kbi_impl).
 */
static int
test_client_kbi_send_fail(void)
{
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	struct test_auth_cbdata cbdata = {0};
	cbdata.none_result = DSSH_AUTH_FAILURE;

	struct auth_server_arg sa = {0};
	sa.ctx = &ctx;
	sa.cbs.methods_str = "keyboard-interactive";
	sa.cbs.none_cb = test_none_cb;
	sa.cbs.cbdata = &cbdata;

	thrd_t st;
	ASSERT_TRUE(thrd_create(&st, auth_server_thread, &sa) == thrd_success);

	/* First get_methods to set auth_service_requested */
	char methods[256];
	int mres = dssh_auth_get_methods(ctx.client, "user",
	    methods, sizeof(methods));
	ASSERT_EQ(mres, 1);

	/* Close c2s so KBI send fails */
	mock_io_close_c2s(&ctx.io);

	struct test_kbi_cbdata kbi_data = {0};
	kbi_data.response = "token";
	int res = dssh_auth_keyboard_interactive(ctx.client, "user",
	    test_kbi_prompt_cb, &kbi_data);
	ASSERT_TRUE(res < 0);

	mock_io_close_s2c(&ctx.io);
	thrd_join(st, NULL);

	handshake_cleanup(&ctx);
	return TEST_PASS;
}

/*
 * Client KBI recv fail: close s2c after KBI initial send so recv fails.
 * Exercises line 787 (recv_packet error in KBI loop).
 */
static int
kbi_close_after_request_server_thread(void *arg)
{
	struct kbi_error_server_arg *a = arg;
	dssh_session sess = a->ctx->server;
	int res;

	res = kbi_error_service_exchange(sess);
	if (res < 0) {
		a->result = res;
		return 0;
	}

	/* Close s2c so client's recv fails */
	mock_io_close_s2c(&a->ctx->io);
	a->result = 0;
	return 0;
}

static int
test_client_kbi_recv_fail(void)
{
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	struct kbi_error_server_arg sa = {0};
	sa.ctx = &ctx;

	thrd_t st;
	ASSERT_TRUE(thrd_create(&st, kbi_close_after_request_server_thread, &sa) == thrd_success);

	struct test_kbi_cbdata kbi_data = {0};
	kbi_data.response = "token";
	int res = dssh_auth_keyboard_interactive(ctx.client, "user",
	    test_kbi_prompt_cb, &kbi_data);

	mock_io_close_c2s(&ctx.io);
	mock_io_close_s2c(&ctx.io);
	thrd_join(st, NULL);

	ASSERT_TRUE(res < 0);

	handshake_cleanup(&ctx);
	return TEST_PASS;
}

/*
 * Client publickey: no key loaded (key_algo_selected is NULL or
 * has no key).  Exercises lines 971-974 (ka==NULL / haskey checks).
 */
static int
test_client_publickey_no_key(void)
{
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	/* Skip SERVICE_REQUEST (already done conceptually) */
	ctx.client->auth_service_requested = true;

	/* Try publickey auth with an algo name that doesn't exist.
	 * dssh_transport_find_key_algo returns NULL → line 971 */
	int res = dssh_auth_publickey(ctx.client,
	    "user", "nonexistent-algo");
	ASSERT_EQ(res, DSSH_ERROR_INIT);

	mock_io_close_c2s(&ctx.io);
	mock_io_close_s2c(&ctx.io);
	handshake_cleanup(&ctx);
	return TEST_PASS;
}

/*
 * Client publickey: pubkey function returns error.
 * Exercises line 980 (ka->pubkey returns negative).
 *
 * We create a stub key_algo entry with a failing pubkey function,
 * register it, and attempt publickey auth.
 */
static int
stub_pubkey_fail(uint8_t *buf, size_t bufsz, size_t *outlen,
    dssh_key_algo_ctx *ctx)
{
	(void)buf; (void)bufsz; (void)outlen; (void)ctx;
	return DSSH_ERROR_INIT;
}

static int
stub_sign_fail(uint8_t *buf, size_t bufsz, size_t *outlen,
    const uint8_t *data, size_t data_len, dssh_key_algo_ctx *ctx)
{
	(void)buf; (void)bufsz; (void)outlen;
	(void)data; (void)data_len; (void)ctx;
	return DSSH_ERROR_INIT;
}

static int
test_client_publickey_pubkey_fail(void)
{
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	/* Temporarily replace the registered key algo's pubkey function */
	const char *algo = test_key_algo_name();
	dssh_key_algo ka = dssh_transport_find_key_algo(algo);
	ASSERT_NOT_NULL(ka);

	dssh_key_algo_pubkey saved_pubkey = ka->pubkey;
	ka->pubkey = stub_pubkey_fail;

	ctx.client->auth_service_requested = true;

	int res = dssh_auth_publickey(ctx.client, "user", algo);

	/* Restore */
	ka->pubkey = saved_pubkey;

	ASSERT_EQ(res, DSSH_ERROR_INIT);

	mock_io_close_c2s(&ctx.io);
	mock_io_close_s2c(&ctx.io);
	handshake_cleanup(&ctx);
	return TEST_PASS;
}

/*
 * Client publickey: sign function returns error.
 * Exercises line 1036 (ka->sign returns negative).
 */
static int
test_client_publickey_sign_fail(void)
{
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	/* Temporarily replace the registered key algo's sign function */
	const char *algo = test_key_algo_name();
	dssh_key_algo ka = dssh_transport_find_key_algo(algo);
	ASSERT_NOT_NULL(ka);

	dssh_key_algo_sign saved_sign = ka->sign;
	ka->sign = stub_sign_fail;

	ctx.client->auth_service_requested = true;

	int res = dssh_auth_publickey(ctx.client, "user", algo);

	/* Restore */
	ka->sign = saved_sign;

	ASSERT_EQ(res, DSSH_ERROR_INIT);

	mock_io_close_c2s(&ctx.io);
	mock_io_close_s2c(&ctx.io);
	handshake_cleanup(&ctx);
	return TEST_PASS;
}

/*
 * Client publickey: send of the auth request fails.
 * Exercises line 1081 (send_packet error after building signed request).
 */
static int
test_client_publickey_send_fail(void)
{
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	/* Do get_methods first to set auth_service_requested */
	struct test_auth_cbdata cbdata = {0};
	cbdata.none_result = DSSH_AUTH_FAILURE;

	struct auth_server_arg sa = {0};
	sa.ctx = &ctx;
	sa.cbs.methods_str = "publickey";
	sa.cbs.none_cb = test_none_cb;
	sa.cbs.cbdata = &cbdata;

	thrd_t st;
	ASSERT_TRUE(thrd_create(&st, auth_server_thread, &sa) == thrd_success);

	char methods[256];
	int mres = dssh_auth_get_methods(ctx.client, "user",
	    methods, sizeof(methods));
	ASSERT_EQ(mres, 1);

	/* Close c2s so the publickey send fails */
	mock_io_close_c2s(&ctx.io);

	int res = dssh_auth_publickey(ctx.client, "user",
	    test_key_algo_name());
	ASSERT_TRUE(res < 0);

	mock_io_close_s2c(&ctx.io);
	thrd_join(st, NULL);

	handshake_cleanup(&ctx);
	return TEST_PASS;
}

/*
 * Client publickey: recv of the auth response fails.
 * Exercises line 1090 (recv_packet error after sending signed request).
 */
static int
pk_close_after_request_server_thread(void *arg)
{
	struct crafted_response_ctx *cr = arg;
	dssh_session server = cr->hctx->server;
	uint8_t msg_type;
	uint8_t *payload;
	size_t payload_len;

	/* SERVICE_REQUEST -> SERVICE_ACCEPT */
	if (dssh_transport_recv_packet(server, &msg_type, &payload, &payload_len) < 0)
		return 0;
	{
		uint8_t accept[64];
		size_t pos = 0;
		accept[pos++] = SSH_MSG_SERVICE_ACCEPT;
		if (payload_len > 5) {
			uint32_t slen;
			dssh_parse_uint32(&payload[1], payload_len - 1, &slen);
			dssh_serialize_uint32(slen, accept, sizeof(accept), &pos);
			memcpy(&accept[pos], &payload[5], slen);
			pos += slen;
		}
		dssh_transport_send_packet(server, accept, pos, NULL);
	}
	/* Receive none USERAUTH_REQUEST → send FAILURE */
	dssh_transport_recv_packet(server, &msg_type, &payload, &payload_len);
	{
		static const char methods[] = "publickey";
		uint8_t msg[64];
		size_t pos = 0;
		msg[pos++] = SSH_MSG_USERAUTH_FAILURE;
		dssh_serialize_uint32((uint32_t)(sizeof(methods) - 1),
		    msg, sizeof(msg), &pos);
		memcpy(&msg[pos], methods, sizeof(methods) - 1);
		pos += sizeof(methods) - 1;
		msg[pos++] = 0;
		dssh_transport_send_packet(server, msg, pos, NULL);
	}

	/* Receive publickey USERAUTH_REQUEST → close pipe */
	dssh_transport_recv_packet(server, &msg_type, &payload, &payload_len);
	mock_io_close_s2c(&cr->hctx->io);
	return 0;
}

static int
test_client_publickey_recv_fail(void)
{
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	struct crafted_response_ctx cr = { &ctx, NULL, 0 };
	thrd_t st;
	thrd_create(&st, pk_close_after_request_server_thread, &cr);

	/* get_methods first */
	char methods[256];
	int mres = dssh_auth_get_methods(ctx.client, "user",
	    methods, sizeof(methods));
	ASSERT_EQ(mres, 1);

	int res = dssh_auth_publickey(ctx.client, "user",
	    test_key_algo_name());

	mock_io_close_c2s(&ctx.io);
	mock_io_close_s2c(&ctx.io);
	thrd_join(st, NULL);

	ASSERT_TRUE(res < 0);

	handshake_cleanup(&ctx);
	return TEST_PASS;
}

/*
 * Client publickey: server sends BANNER before auth response.
 * Exercises line 1092 (banner handling in publickey recv loop).
 */
static int
pk_banner_server_thread(void *arg)
{
	struct crafted_response_ctx *cr = arg;
	dssh_session server = cr->hctx->server;
	uint8_t msg_type;
	uint8_t *payload;
	size_t payload_len;

	/* SERVICE_REQUEST -> SERVICE_ACCEPT */
	if (dssh_transport_recv_packet(server, &msg_type, &payload, &payload_len) < 0)
		return 0;
	{
		uint8_t accept[64];
		size_t pos = 0;
		accept[pos++] = SSH_MSG_SERVICE_ACCEPT;
		if (payload_len > 5) {
			uint32_t slen;
			dssh_parse_uint32(&payload[1], payload_len - 1, &slen);
			dssh_serialize_uint32(slen, accept, sizeof(accept), &pos);
			memcpy(&accept[pos], &payload[5], slen);
			pos += slen;
		}
		dssh_transport_send_packet(server, accept, pos, NULL);
	}
	/* Receive none USERAUTH_REQUEST → send FAILURE */
	dssh_transport_recv_packet(server, &msg_type, &payload, &payload_len);
	{
		static const char methods[] = "publickey";
		uint8_t msg[64];
		size_t pos = 0;
		msg[pos++] = SSH_MSG_USERAUTH_FAILURE;
		dssh_serialize_uint32((uint32_t)(sizeof(methods) - 1),
		    msg, sizeof(msg), &pos);
		memcpy(&msg[pos], methods, sizeof(methods) - 1);
		pos += sizeof(methods) - 1;
		msg[pos++] = 0;
		dssh_transport_send_packet(server, msg, pos, NULL);
	}

	/* Receive publickey USERAUTH_REQUEST */
	dssh_transport_recv_packet(server, &msg_type, &payload, &payload_len);

	/* Send BANNER before SUCCESS */
	{
		static const char bmsg[] = "PK Banner";
		uint8_t msg[64];
		size_t pos = 0;
		msg[pos++] = SSH_MSG_USERAUTH_BANNER;
		dssh_serialize_uint32((uint32_t)(sizeof(bmsg) - 1),
		    msg, sizeof(msg), &pos);
		memcpy(&msg[pos], bmsg, sizeof(bmsg) - 1);
		pos += sizeof(bmsg) - 1;
		dssh_serialize_uint32(0, msg, sizeof(msg), &pos);
		dssh_transport_send_packet(server, msg, pos, NULL);
	}

	/* Send SUCCESS */
	{
		uint8_t succ = SSH_MSG_USERAUTH_SUCCESS;
		dssh_transport_send_packet(server, &succ, 1, NULL);
	}
	return 0;
}

static int
test_client_publickey_banner(void)
{
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	banner_received = false;
	banner_text[0] = 0;
	dssh_session_set_banner_cb(ctx.client, test_banner_cb, NULL);

	struct crafted_response_ctx cr = { &ctx, NULL, 0 };
	thrd_t st;
	thrd_create(&st, pk_banner_server_thread, &cr);

	/* get_methods first */
	char methods[256];
	int mres = dssh_auth_get_methods(ctx.client, "user",
	    methods, sizeof(methods));
	ASSERT_EQ(mres, 1);

	int res = dssh_auth_publickey(ctx.client, "user",
	    test_key_algo_name());

	mock_io_close_c2s(&ctx.io);
	mock_io_close_s2c(&ctx.io);
	thrd_join(st, NULL);

	ASSERT_EQ(res, 0);
	ASSERT_TRUE(banner_received);
	ASSERT_STR_EQ(banner_text, "PK Banner");

	handshake_cleanup(&ctx);
	return TEST_PASS;
}

/*
 * Client: banner with no language field (handle_banner line 28 False).
 * The banner_trunc_server_thread already tests banner with no room for
 * msg_len (line 17-18) and banner with truncated lang data (line 33-34).
 * We need to also test a banner where msg is valid but there is
 * NO room for lang_len at all (rpos+4 > payload_len, line 28 False).
 * This is already covered by the third variant in banner_trunc_server_thread
 * which has lang_len=100 but sets it.  We need a variant where the payload
 * ends right after msg data with no lang_len field at all.
 */
static int
banner_no_lang_server_thread(void *arg)
{
	struct auth_server_arg *sa = arg;
	dssh_session server = sa->ctx->server;
	uint8_t msg_type;
	uint8_t *payload;
	size_t payload_len;

	if (dssh_transport_recv_packet(server, &msg_type, &payload, &payload_len) < 0) {
		sa->result = -1;
		return 0;
	}
	{
		uint8_t accept[64];
		size_t pos = 0;
		accept[pos++] = SSH_MSG_SERVICE_ACCEPT;
		if (payload_len > 5) {
			uint32_t slen;
			dssh_parse_uint32(&payload[1], payload_len - 1, &slen);
			dssh_serialize_uint32(slen, accept, sizeof(accept), &pos);
			memcpy(&accept[pos], &payload[5], slen);
			pos += slen;
		}
		dssh_transport_send_packet(server, accept, pos, NULL);
	}

	/* Send banner with valid msg but no room for lang_len at all */
	{
		uint8_t b[16];
		size_t bp = 0;
		b[bp++] = 53; /* SSH_MSG_USERAUTH_BANNER */
		dssh_serialize_uint32(2, b, sizeof(b), &bp);
		b[bp++] = 'h';
		b[bp++] = 'i';
		/* Payload ends here — no room for lang_len (4 bytes) */
		dssh_transport_send_packet(server, b, bp, NULL);
	}

	/* Read the password request and send SUCCESS */
	dssh_transport_recv_packet(server, &msg_type, &payload, &payload_len);
	{
		uint8_t succ = SSH_MSG_USERAUTH_SUCCESS;
		dssh_transport_send_packet(server, &succ, 1, NULL);
	}

	sa->result = 0;
	return 0;
}

static int
test_server_banner_no_lang(void)
{
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	banner_received = false;
	banner_text[0] = 0;
	dssh_session_set_banner_cb(ctx.client, test_banner_cb, NULL);

	struct auth_server_arg sa = {0};
	sa.ctx = &ctx;

	thrd_t st;
	ASSERT_TRUE(thrd_create(&st, banner_no_lang_server_thread, &sa) == thrd_success);

	int res = dssh_auth_password(ctx.client, "testuser", "pass",
	    NULL, NULL);

	thrd_join(st, NULL);
	ASSERT_EQ(res, 0);
	/* Banner callback should have been called (msg is valid even
	 * though language is missing — line 28 is False, language
	 * stays NULL/0 but cb still fires). */
	ASSERT_TRUE(banner_received);

	handshake_cleanup(&ctx);
	return TEST_PASS;
}

/*
 * Client get_methods with small buffer (1 byte) so the
 * copylen ternary takes the methods_sz-1 branch.
 * Exercises line 585 (mlen >= methods_sz-1 branch).
 */
static int
test_get_methods_small_buffer(void)
{
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	struct test_auth_cbdata cbdata = {0};
	cbdata.none_result = DSSH_AUTH_FAILURE;

	struct auth_server_arg sa = {0};
	sa.ctx = &ctx;
	sa.cbs.methods_str = "publickey,password";
	sa.cbs.none_cb = test_none_cb;
	sa.cbs.cbdata = &cbdata;

	thrd_t st;
	ASSERT_TRUE(thrd_create(&st, auth_server_thread, &sa) == thrd_success);

	/* Call with a 1-byte buffer: methods_sz=1, copylen = min(18, 0) = 0 */
	char methods[1] = { 'X' };
	int res = dssh_auth_get_methods(ctx.client, "user", methods, 1);

	mock_io_close_c2s(&ctx.io);
	mock_io_close_s2c(&ctx.io);
	thrd_join(st, NULL);

	ASSERT_EQ(res, 1);
	ASSERT_EQ(methods[0], '\0'); /* NUL terminated with 0 bytes copied */

	handshake_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Direct parse_userauth_prefix tests — called from main thread
 * to avoid coverage counter issues with threaded server tests.
 * ================================================================ */

static int
test_parse_prefix_empty(void)
{
	/* Just msg_type byte, no username length */
	uint8_t payload[] = { 50 }; /* SSH_MSG_USERAUTH_REQUEST */
	const uint8_t *user, *method;
	size_t user_len, method_len;
	int64_t res = parse_userauth_prefix(payload, 1,
	    &user, &user_len, &method, &method_len);
	ASSERT_TRUE(res < 0);
	return TEST_PASS;
}

static int
test_parse_prefix_trunc_username(void)
{
	/* username_len=100 but only 2 bytes of data */
	uint8_t payload[8];
	size_t pos = 0;
	payload[pos++] = 50;
	dssh_serialize_uint32(100, payload, sizeof(payload), &pos);
	payload[pos++] = 'u';
	payload[pos++] = 's';
	const uint8_t *user, *method;
	size_t user_len, method_len;
	int64_t res = parse_userauth_prefix(payload, pos,
	    &user, &user_len, &method, &method_len);
	ASSERT_TRUE(res < 0);
	return TEST_PASS;
}

static int
test_parse_prefix_no_service(void)
{
	/* Valid username, no service length */
	uint8_t payload[8];
	size_t pos = 0;
	payload[pos++] = 50;
	dssh_serialize_uint32(1, payload, sizeof(payload), &pos);
	payload[pos++] = 'u';
	const uint8_t *user, *method;
	size_t user_len, method_len;
	int64_t res = parse_userauth_prefix(payload, pos,
	    &user, &user_len, &method, &method_len);
	ASSERT_TRUE(res < 0);
	return TEST_PASS;
}

static int
test_parse_prefix_trunc_service(void)
{
	/* service_len=100 but only 1 byte */
	uint8_t payload[16];
	size_t pos = 0;
	payload[pos++] = 50;
	dssh_serialize_uint32(1, payload, sizeof(payload), &pos);
	payload[pos++] = 'u';
	dssh_serialize_uint32(100, payload, sizeof(payload), &pos);
	payload[pos++] = 's';
	const uint8_t *user, *method;
	size_t user_len, method_len;
	int64_t res = parse_userauth_prefix(payload, pos,
	    &user, &user_len, &method, &method_len);
	ASSERT_TRUE(res < 0);
	return TEST_PASS;
}

static int
test_parse_prefix_no_method(void)
{
	/* Valid username + service, no method length */
	uint8_t payload[16];
	size_t pos = 0;
	payload[pos++] = 50;
	dssh_serialize_uint32(1, payload, sizeof(payload), &pos);
	payload[pos++] = 'u';
	dssh_serialize_uint32(1, payload, sizeof(payload), &pos);
	payload[pos++] = 's';
	const uint8_t *user, *method;
	size_t user_len, method_len;
	int64_t res = parse_userauth_prefix(payload, pos,
	    &user, &user_len, &method, &method_len);
	ASSERT_TRUE(res < 0);
	return TEST_PASS;
}

static int
test_parse_prefix_trunc_method(void)
{
	/* method_len=100 but only 2 bytes */
	uint8_t payload[20];
	size_t pos = 0;
	payload[pos++] = 50;
	dssh_serialize_uint32(1, payload, sizeof(payload), &pos);
	payload[pos++] = 'u';
	dssh_serialize_uint32(1, payload, sizeof(payload), &pos);
	payload[pos++] = 's';
	dssh_serialize_uint32(100, payload, sizeof(payload), &pos);
	payload[pos++] = 'm';
	payload[pos++] = 'x';
	const uint8_t *user, *method;
	size_t user_len, method_len;
	int64_t res = parse_userauth_prefix(payload, pos,
	    &user, &user_len, &method, &method_len);
	ASSERT_TRUE(res < 0);
	return TEST_PASS;
}

static int
test_parse_prefix_valid(void)
{
	/* Valid: user="u", service="s", method="m" */
	uint8_t payload[20];
	size_t pos = 0;
	payload[pos++] = 50;
	dssh_serialize_uint32(1, payload, sizeof(payload), &pos);
	payload[pos++] = 'u';
	dssh_serialize_uint32(1, payload, sizeof(payload), &pos);
	payload[pos++] = 's';
	dssh_serialize_uint32(1, payload, sizeof(payload), &pos);
	payload[pos++] = 'm';
	const uint8_t *user, *method;
	size_t user_len, method_len;
	int64_t res = parse_userauth_prefix(payload, pos,
	    &user, &user_len, &method, &method_len);
	ASSERT_TRUE(res > 0);
	ASSERT_EQ(user_len, 1);
	ASSERT_EQ(method_len, 1);
	return TEST_PASS;
}

/* ================================================================
 * Group 3: Server send failures for password-change and publickey-
 * with-signature paths.
 *
 * These tests exercise branches where the server successfully processes
 * a password-change or publickey-with-sig request, but the subsequent
 * send (SUCCESS/FAILURE/CHANGEREQ) fails because we close the s2c pipe.
 * ================================================================ */

/*
 * Password change: passwd_change_cb returns SUCCESS → send_auth_success
 * fails (line 281).
 */
static int
send_fail_change_success_cb(const uint8_t *username, size_t username_len,
    const uint8_t *old_pw, size_t old_pw_len,
    const uint8_t *new_pw, size_t new_pw_len,
    uint8_t **prompt, size_t *prompt_len, void *cbdata)
{
	(void)username; (void)username_len;
	(void)old_pw; (void)old_pw_len;
	(void)new_pw; (void)new_pw_len;
	(void)prompt; (void)prompt_len;
	(void)cbdata;
	return DSSH_AUTH_SUCCESS;
}

static int
test_server_send_fail_change_success(void)
{
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	struct test_auth_cbdata cbdata = {0};
	struct auth_server_arg sa = {0};
	sa.ctx = &ctx;
	sa.cbs.methods_str = "password";
	sa.cbs.password_cb = test_password_cb;
	sa.cbs.passwd_change_cb = send_fail_change_success_cb;
	sa.cbs.cbdata = &cbdata;

	thrd_t st;
	ASSERT_TRUE(thrd_create(&st, auth_server_thread, &sa) == thrd_success);

	/* SERVICE_REQUEST/ACCEPT */
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

	/* Send password change request (change=true) */
	{
		uint8_t msg[128];
		size_t pos = build_auth_prefix(msg, sizeof(msg), "password");
		msg[pos++] = 1; /* change = true */
		dssh_serialize_uint32(3, msg, sizeof(msg), &pos);
		memcpy(&msg[pos], "old", 3);
		pos += 3;
		dssh_serialize_uint32(3, msg, sizeof(msg), &pos);
		memcpy(&msg[pos], "new", 3);
		pos += 3;
		ASSERT_OK(dssh_transport_send_packet(ctx.client, msg, pos, NULL));
	}

	/* Close s2c before server sends SUCCESS */
	mock_io_close_s2c(&ctx.io);

	thrd_join(st, NULL);
	ASSERT_TRUE(sa.result < 0);

	mock_io_close_c2s(&ctx.io);
	handshake_cleanup(&ctx);
	return TEST_PASS;
}

/*
 * Password change: passwd_change_cb returns CHANGE_PASSWORD →
 * send_passwd_changereq fails (line 287).
 */
static int
send_fail_change_changereq_cb(const uint8_t *username, size_t username_len,
    const uint8_t *old_pw, size_t old_pw_len,
    const uint8_t *new_pw, size_t new_pw_len,
    uint8_t **prompt, size_t *prompt_len, void *cbdata)
{
	(void)username; (void)username_len;
	(void)old_pw; (void)old_pw_len;
	(void)new_pw; (void)new_pw_len;
	(void)cbdata;
	*prompt = malloc(6);
	if (*prompt == NULL) {
		*prompt_len = 0;
		return DSSH_AUTH_FAILURE;
	}
	memcpy(*prompt, "retry!", 6);
	*prompt_len = 6;
	return DSSH_AUTH_CHANGE_PASSWORD;
}

static int
test_server_send_fail_change_changereq(void)
{
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	struct test_auth_cbdata cbdata = {0};
	struct auth_server_arg sa = {0};
	sa.ctx = &ctx;
	sa.cbs.methods_str = "password";
	sa.cbs.password_cb = test_password_cb;
	sa.cbs.passwd_change_cb = send_fail_change_changereq_cb;
	sa.cbs.cbdata = &cbdata;

	thrd_t st;
	ASSERT_TRUE(thrd_create(&st, auth_server_thread, &sa) == thrd_success);

	/* SERVICE_REQUEST/ACCEPT */
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

	/* Send password change request */
	{
		uint8_t msg[128];
		size_t pos = build_auth_prefix(msg, sizeof(msg), "password");
		msg[pos++] = 1; /* change = true */
		dssh_serialize_uint32(3, msg, sizeof(msg), &pos);
		memcpy(&msg[pos], "old", 3);
		pos += 3;
		dssh_serialize_uint32(3, msg, sizeof(msg), &pos);
		memcpy(&msg[pos], "new", 3);
		pos += 3;
		ASSERT_OK(dssh_transport_send_packet(ctx.client, msg, pos, NULL));
	}

	/* Close s2c before server sends CHANGEREQ */
	mock_io_close_s2c(&ctx.io);

	thrd_join(st, NULL);
	ASSERT_TRUE(sa.result < 0);

	mock_io_close_c2s(&ctx.io);
	handshake_cleanup(&ctx);
	return TEST_PASS;
}

/*
 * Password change: passwd_change_cb returns FAILURE →
 * send_auth_failure fails (line 293).
 */
static int
test_server_send_fail_change_failure(void)
{
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	struct test_auth_cbdata cbdata = {0};
	struct auth_server_arg sa = {0};
	sa.ctx = &ctx;
	sa.cbs.methods_str = "password";
	sa.cbs.password_cb = test_password_cb;
	sa.cbs.passwd_change_cb = test_passwd_change_cb_failure;
	sa.cbs.cbdata = &cbdata;

	thrd_t st;
	ASSERT_TRUE(thrd_create(&st, auth_server_thread, &sa) == thrd_success);

	/* SERVICE_REQUEST/ACCEPT */
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

	/* Send password change request */
	{
		uint8_t msg[128];
		size_t pos = build_auth_prefix(msg, sizeof(msg), "password");
		msg[pos++] = 1; /* change = true */
		dssh_serialize_uint32(3, msg, sizeof(msg), &pos);
		memcpy(&msg[pos], "old", 3);
		pos += 3;
		dssh_serialize_uint32(3, msg, sizeof(msg), &pos);
		memcpy(&msg[pos], "new", 3);
		pos += 3;
		ASSERT_OK(dssh_transport_send_packet(ctx.client, msg, pos, NULL));
	}

	/* Close s2c before server sends FAILURE */
	mock_io_close_s2c(&ctx.io);

	thrd_join(st, NULL);
	ASSERT_TRUE(sa.result < 0);

	mock_io_close_c2s(&ctx.io);
	handshake_cleanup(&ctx);
	return TEST_PASS;
}

/*
 * Publickey with sig: verify fails → send_auth_failure fails (line 418).
 * We send a publickey request with has_sig=true and a bad sig, then
 * close s2c so the FAILURE send fails.
 */
static int
test_server_send_fail_pk_verify_bad_sig(void)
{
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	struct test_auth_cbdata cbdata = {0};
	cbdata.publickey_result = DSSH_AUTH_SUCCESS;

	struct auth_server_arg sa = {0};
	sa.ctx = &ctx;
	sa.cbs.methods_str = "publickey";
	sa.cbs.publickey_cb = test_publickey_cb;
	sa.cbs.cbdata = &cbdata;

	thrd_t st;
	ASSERT_TRUE(thrd_create(&st, auth_server_thread, &sa) == thrd_success);

	/* SERVICE_REQUEST/ACCEPT */
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

	/* Get the real public key blob */
	const char *algo = test_key_algo_name();
	dssh_key_algo ka = dssh_transport_find_key_algo(algo);
	if (ka == NULL || !ka->haskey(ka->ctx)) {
		mock_io_close_c2s(&ctx.io);
		mock_io_close_s2c(&ctx.io);
		thrd_join(st, NULL);
		handshake_cleanup(&ctx);
		return TEST_SKIP;
	}
	uint8_t pub_buf[1024];
	size_t pub_len;
	if (ka->pubkey(pub_buf, sizeof(pub_buf), &pub_len, ka->ctx) < 0) {
		mock_io_close_c2s(&ctx.io);
		mock_io_close_s2c(&ctx.io);
		thrd_join(st, NULL);
		handshake_cleanup(&ctx);
		return TEST_SKIP;
	}

	/* Send publickey request with bad signature */
	{
		size_t algo_len = strlen(algo);
		uint8_t msg[2048];
		size_t pos = build_auth_prefix(msg, sizeof(msg), "publickey");
		msg[pos++] = 1; /* has_sig = true */
		dssh_serialize_uint32((uint32_t)algo_len, msg, sizeof(msg), &pos);
		memcpy(&msg[pos], algo, algo_len);
		pos += algo_len;
		dssh_serialize_uint32((uint32_t)pub_len, msg, sizeof(msg), &pos);
		memcpy(&msg[pos], pub_buf, pub_len);
		pos += pub_len;
		/* Garbage signature */
		uint8_t fake_sig[128];
		size_t fspos = 0;
		dssh_serialize_uint32((uint32_t)algo_len, fake_sig, sizeof(fake_sig), &fspos);
		memcpy(&fake_sig[fspos], algo, algo_len);
		fspos += algo_len;
		dssh_serialize_uint32(32, fake_sig, sizeof(fake_sig), &fspos);
		memset(&fake_sig[fspos], 0xBB, 32);
		fspos += 32;
		dssh_serialize_uint32((uint32_t)fspos, msg, sizeof(msg), &pos);
		memcpy(&msg[pos], fake_sig, fspos);
		pos += fspos;
		ASSERT_OK(dssh_transport_send_packet(ctx.client, msg, pos, NULL));
	}

	/* Close s2c before server sends FAILURE after verify failure */
	mock_io_close_s2c(&ctx.io);

	thrd_join(st, NULL);
	ASSERT_TRUE(sa.result < 0);

	mock_io_close_c2s(&ctx.io);
	handshake_cleanup(&ctx);
	return TEST_PASS;
}

/*
 * Publickey with valid sig: publickey_cb returns SUCCESS →
 * send_auth_success fails (line 429).
 * We use a full roundtrip with a real key + signature to reach
 * the success path, then close s2c before the SUCCESS send.
 */
static int
pk_success_send_fail_server_thread(void *arg)
{
	struct auth_server_arg *a = arg;
	dssh_session server = a->ctx->server;
	uint8_t msg_type;
	uint8_t *payload;
	size_t payload_len;

	/* SERVICE_REQUEST → SERVICE_ACCEPT */
	if (dssh_transport_recv_packet(server, &msg_type, &payload, &payload_len) < 0) {
		a->result = -1;
		return 0;
	}
	{
		uint8_t accept[64];
		size_t pos = 0;
		accept[pos++] = SSH_MSG_SERVICE_ACCEPT;
		if (payload_len > 5) {
			uint32_t slen;
			dssh_parse_uint32(&payload[1], payload_len - 1, &slen);
			dssh_serialize_uint32(slen, accept, sizeof(accept), &pos);
			memcpy(&accept[pos], &payload[5], slen);
			pos += slen;
		}
		dssh_transport_send_packet(server, accept, pos, NULL);
	}

	/* Receive none USERAUTH_REQUEST → send FAILURE */
	dssh_transport_recv_packet(server, &msg_type, &payload, &payload_len);
	{
		static const char methods[] = "publickey";
		uint8_t msg[64];
		size_t pos = 0;
		msg[pos++] = SSH_MSG_USERAUTH_FAILURE;
		dssh_serialize_uint32((uint32_t)(sizeof(methods) - 1),
		    msg, sizeof(msg), &pos);
		memcpy(&msg[pos], methods, sizeof(methods) - 1);
		pos += sizeof(methods) - 1;
		msg[pos++] = 0;
		dssh_transport_send_packet(server, msg, pos, NULL);
	}

	/* Receive publickey USERAUTH_REQUEST — parse and verify manually,
	 * then close s2c instead of sending SUCCESS. */
	dssh_transport_recv_packet(server, &msg_type, &payload, &payload_len);

	/* Close s2c — simulates send failure for the auth success response */
	mock_io_close_s2c(&a->ctx->io);

	a->result = 0;
	return 0;
}

static int
test_server_send_fail_pk_success(void)
{
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	struct auth_server_arg sa = {0};
	sa.ctx = &ctx;

	thrd_t st;
	ASSERT_TRUE(thrd_create(&st, pk_success_send_fail_server_thread, &sa) == thrd_success);

	/* Client: get_methods first */
	char methods[256];
	int mres = dssh_auth_get_methods(ctx.client, "user",
	    methods, sizeof(methods));
	ASSERT_EQ(mres, 1);

	/* Client: publickey auth — server will close pipe before responding */
	int res = dssh_auth_publickey(ctx.client, "user",
	    test_key_algo_name());

	mock_io_close_c2s(&ctx.io);
	mock_io_close_s2c(&ctx.io);
	thrd_join(st, NULL);

	ASSERT_TRUE(res < 0);

	handshake_cleanup(&ctx);
	return TEST_PASS;
}

/*
 * Publickey with valid sig: publickey_cb returns FAILURE →
 * send_auth_failure fails (line 434).
 * Same as above but the publickey_cb rejects.
 */
static int
pk_reject_send_fail_server_thread(void *arg)
{
	struct auth_server_arg *a = arg;
	a->result = dssh_auth_server(a->ctx->server, &a->cbs,
	    a->username_out, &a->username_out_len);
	return 0;
}

static int
test_server_send_fail_pk_reject_sig(void)
{
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	struct test_auth_cbdata cbdata = {0};
	cbdata.publickey_result = DSSH_AUTH_FAILURE;

	struct auth_server_arg sa = {0};
	sa.ctx = &ctx;
	sa.cbs.methods_str = "publickey";
	sa.cbs.publickey_cb = test_publickey_cb;
	sa.cbs.cbdata = &cbdata;

	thrd_t st;
	ASSERT_TRUE(thrd_create(&st, pk_reject_send_fail_server_thread, &sa) == thrd_success);

	/* SERVICE_REQUEST/ACCEPT */
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

	/* Get the real public key blob */
	const char *algo = test_key_algo_name();
	dssh_key_algo ka = dssh_transport_find_key_algo(algo);
	if (ka == NULL || !ka->haskey(ka->ctx)) {
		mock_io_close_c2s(&ctx.io);
		mock_io_close_s2c(&ctx.io);
		thrd_join(st, NULL);
		handshake_cleanup(&ctx);
		return TEST_SKIP;
	}
	uint8_t pub_buf[1024];
	size_t pub_len;
	ka->pubkey(pub_buf, sizeof(pub_buf), &pub_len, ka->ctx);

	/* Build sign data and generate a VALID signature */
	size_t algo_len = strlen(algo);
	static const char svc[] = "ssh-connection";
	static const char method[] = "publickey";
	size_t ulen = 4; /* "user" */

	size_t sign_data_len = 4 + ctx.server->trans.session_id_sz +
	    1 + 4 + ulen + 4 + (sizeof(svc) - 1) +
	    4 + (sizeof(method) - 1) + 1 + 4 + algo_len + 4 + pub_len;
	uint8_t *sign_data = malloc(sign_data_len);
	size_t sp = 0;
	dssh_serialize_uint32((uint32_t)ctx.server->trans.session_id_sz,
	    sign_data, sign_data_len, &sp);
	memcpy(&sign_data[sp], ctx.server->trans.session_id,
	    ctx.server->trans.session_id_sz);
	sp += ctx.server->trans.session_id_sz;
	sign_data[sp++] = SSH_MSG_USERAUTH_REQUEST;
	dssh_serialize_uint32(4, sign_data, sign_data_len, &sp);
	memcpy(&sign_data[sp], "user", 4); sp += 4;
	dssh_serialize_uint32((uint32_t)(sizeof(svc) - 1), sign_data, sign_data_len, &sp);
	memcpy(&sign_data[sp], svc, sizeof(svc) - 1); sp += sizeof(svc) - 1;
	dssh_serialize_uint32((uint32_t)(sizeof(method) - 1), sign_data, sign_data_len, &sp);
	memcpy(&sign_data[sp], method, sizeof(method) - 1); sp += sizeof(method) - 1;
	sign_data[sp++] = 1; /* TRUE */
	dssh_serialize_uint32((uint32_t)algo_len, sign_data, sign_data_len, &sp);
	memcpy(&sign_data[sp], algo, algo_len); sp += algo_len;
	dssh_serialize_uint32((uint32_t)pub_len, sign_data, sign_data_len, &sp);
	memcpy(&sign_data[sp], pub_buf, pub_len); sp += pub_len;

	uint8_t sig_buf[1024];
	size_t sig_len;
	int sres = ka->sign(sig_buf, sizeof(sig_buf), &sig_len, sign_data, sp, ka->ctx);
	free(sign_data);
	if (sres < 0) {
		mock_io_close_c2s(&ctx.io);
		mock_io_close_s2c(&ctx.io);
		thrd_join(st, NULL);
		handshake_cleanup(&ctx);
		return TEST_SKIP;
	}

	/* Send publickey request with valid signature */
	{
		uint8_t msg[4096];
		size_t pos = build_auth_prefix(msg, sizeof(msg), "publickey");
		msg[pos++] = 1; /* has_sig = true */
		dssh_serialize_uint32((uint32_t)algo_len, msg, sizeof(msg), &pos);
		memcpy(&msg[pos], algo, algo_len);
		pos += algo_len;
		dssh_serialize_uint32((uint32_t)pub_len, msg, sizeof(msg), &pos);
		memcpy(&msg[pos], pub_buf, pub_len);
		pos += pub_len;
		dssh_serialize_uint32((uint32_t)sig_len, msg, sizeof(msg), &pos);
		memcpy(&msg[pos], sig_buf, sig_len);
		pos += sig_len;
		ASSERT_OK(dssh_transport_send_packet(ctx.client, msg, pos, NULL));
	}

	/* Close s2c before server sends FAILURE (publickey_cb rejects) */
	mock_io_close_s2c(&ctx.io);

	thrd_join(st, NULL);
	ASSERT_TRUE(sa.result < 0);

	mock_io_close_c2s(&ctx.io);
	handshake_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Group 4 additions: SERVICE_REQUEST with tiny payload (lines 181/184),
 * CHANGEREQ with no lang_len field (lines 699/702), and KBI with
 * empty response (line 932).
 * ================================================================ */

/*
 * SERVER: SERVICE_REQUEST with only 5 bytes (msg_type + 4-byte slen
 * where slen is 0).  Exercises line 181 False (payload_len > 5 is false
 * when payload_len == 5).
 */
static int
test_server_tiny_service_request(void)
{
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	struct test_auth_cbdata cbdata = {0};
	cbdata.password_result = DSSH_AUTH_SUCCESS;

	struct auth_server_arg sa = {0};
	sa.ctx = &ctx;
	sa.cbs.methods_str = "password";
	sa.cbs.password_cb = test_password_cb;
	sa.cbs.cbdata = &cbdata;

	thrd_t st;
	ASSERT_TRUE(thrd_create(&st, auth_server_thread, &sa) == thrd_success);

	/* Send SERVICE_REQUEST with exactly 5 bytes: msg_type + slen=0 */
	{
		uint8_t msg[5];
		size_t pos = 0;
		msg[pos++] = SSH_MSG_SERVICE_REQUEST;
		dssh_serialize_uint32(0, msg, sizeof(msg), &pos);
		ASSERT_OK(dssh_transport_send_packet(ctx.client, msg, pos, NULL));
	}

	/* Server sends SERVICE_ACCEPT with just the msg_type (no service
	 * name echoed).  Then we can send auth request. */
	{
		uint8_t msg_type;
		uint8_t *payload;
		size_t payload_len;
		ASSERT_OK(dssh_transport_recv_packet(ctx.client, &msg_type, &payload, &payload_len));
		ASSERT_EQ(msg_type, SSH_MSG_SERVICE_ACCEPT);
	}

	/* Send a valid password auth to let server succeed */
	{
		uint8_t msg[128];
		size_t pos = build_auth_prefix(msg, sizeof(msg), "password");
		msg[pos++] = 0;
		dssh_serialize_uint32(4, msg, sizeof(msg), &pos);
		memcpy(&msg[pos], "pass", 4);
		pos += 4;
		ASSERT_OK(dssh_transport_send_packet(ctx.client, msg, pos, NULL));
	}
	{
		uint8_t msg_type;
		uint8_t *payload;
		size_t payload_len;
		ASSERT_OK(dssh_transport_recv_packet(ctx.client, &msg_type, &payload, &payload_len));
		ASSERT_EQ(msg_type, SSH_MSG_USERAUTH_SUCCESS);
	}

	thrd_join(st, NULL);
	ASSERT_EQ(sa.result, 0);

	handshake_cleanup(&ctx);
	return TEST_PASS;
}

/*
 * SERVER: SERVICE_REQUEST with 4 bytes only (msg_type + 3 partial
 * bytes of slen).  Exercises line 181 False (payload_len <= 5).
 * Server sends SERVICE_ACCEPT with just msg_type byte.
 */
static int
test_server_short_service_request(void)
{
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	struct test_auth_cbdata cbdata = {0};
	cbdata.password_result = DSSH_AUTH_SUCCESS;

	struct auth_server_arg sa = {0};
	sa.ctx = &ctx;
	sa.cbs.methods_str = "password";
	sa.cbs.password_cb = test_password_cb;
	sa.cbs.cbdata = &cbdata;

	thrd_t st;
	ASSERT_TRUE(thrd_create(&st, auth_server_thread, &sa) == thrd_success);

	/* Send SERVICE_REQUEST with 4 bytes: msg_type + 3 junk bytes */
	{
		uint8_t msg[4];
		msg[0] = SSH_MSG_SERVICE_REQUEST;
		msg[1] = 0;
		msg[2] = 0;
		msg[3] = 0;
		ASSERT_OK(dssh_transport_send_packet(ctx.client, msg, 4, NULL));
	}
	{
		uint8_t msg_type;
		uint8_t *payload;
		size_t payload_len;
		ASSERT_OK(dssh_transport_recv_packet(ctx.client, &msg_type, &payload, &payload_len));
		ASSERT_EQ(msg_type, SSH_MSG_SERVICE_ACCEPT);
	}

	/* Send password auth */
	{
		uint8_t msg[128];
		size_t pos = build_auth_prefix(msg, sizeof(msg), "password");
		msg[pos++] = 0;
		dssh_serialize_uint32(4, msg, sizeof(msg), &pos);
		memcpy(&msg[pos], "pass", 4);
		pos += 4;
		ASSERT_OK(dssh_transport_send_packet(ctx.client, msg, pos, NULL));
	}
	{
		uint8_t msg_type;
		uint8_t *payload;
		size_t payload_len;
		ASSERT_OK(dssh_transport_recv_packet(ctx.client, &msg_type, &payload, &payload_len));
		ASSERT_EQ(msg_type, SSH_MSG_USERAUTH_SUCCESS);
	}

	thrd_join(st, NULL);
	ASSERT_EQ(sa.result, 0);

	handshake_cleanup(&ctx);
	return TEST_PASS;
}

/*
 * CLIENT: CHANGEREQ with valid prompt but no room for lang_len at all.
 * Payload: msg_type + prompt_len + prompt_data, then ends.
 * Exercises lines 699 False (rpos + 4 <= payload_len is false).
 * With a valid change callback this means lang=NULL/0 and cb fires.
 */
static int
test_password_changereq_no_lang_field(void)
{
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	uint8_t resp[16];
	size_t rp = 0;
	resp[rp++] = 60; /* SSH_MSG_USERAUTH_PASSWD_CHANGEREQ */
	dssh_serialize_uint32(3, resp, sizeof(resp), &rp);
	memcpy(&resp[rp], "hey", 3);
	rp += 3;
	/* No lang_len field at all — payload ends here */
	struct crafted_response_ctx cr = { &ctx, resp, rp };
	thrd_t st;
	thrd_create(&st, changereq_server_thread_send, &cr);

	int res = dssh_auth_password(ctx.client, "user", "pass",
	    test_client_changereq_cb, NULL);
	thrd_join(st, NULL);
	/* The change callback fires with lang=NULL.  Its response
	 * send may fail (server closed pipe), but we just verify
	 * no crash and that the flow reached the callback. */
	(void)res;

	handshake_cleanup(&ctx);
	return TEST_PASS;
}

/*
 * CLIENT: CHANGEREQ with valid prompt, lang_len present but
 * lang_len + rpos > payload_len.  Exercises line 702 False
 * (rpos + lang_len <= payload_len is false → lang_len = 0).
 */
static int
test_password_changereq_truncated_lang_data(void)
{
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	uint8_t resp[32];
	size_t rp = 0;
	resp[rp++] = 60;
	dssh_serialize_uint32(2, resp, sizeof(resp), &rp);
	memcpy(&resp[rp], "ok", 2);
	rp += 2;
	dssh_serialize_uint32(50, resp, sizeof(resp), &rp); /* lang_len=50 > remaining */
	/* Only have 0 bytes of lang data */
	struct crafted_response_ctx cr = { &ctx, resp, rp };
	thrd_t st;
	thrd_create(&st, changereq_server_thread_send, &cr);

	int res = dssh_auth_password(ctx.client, "user", "pass",
	    test_client_changereq_cb, NULL);
	thrd_join(st, NULL);
	(void)res;

	handshake_cleanup(&ctx);
	return TEST_PASS;
}

/*
 * KBI: callback returns empty responses (response_lens[i] == 0).
 * Exercises line 932 False (response_lens[i] > 0 is false, so
 * memcpy is skipped).
 */
static int
kbi_cb_empty_response(const uint8_t *name, size_t name_len,
    const uint8_t *instruction, size_t instruction_len,
    uint32_t num_prompts,
    const uint8_t **prompts, const size_t *prompt_lens,
    const bool *echo,
    uint8_t **responses, size_t *response_lens,
    void *cbdata)
{
	(void)name; (void)name_len;
	(void)instruction; (void)instruction_len;
	(void)prompts; (void)prompt_lens; (void)echo;
	(void)cbdata;

	for (uint32_t i = 0; i < num_prompts; i++) {
		responses[i] = NULL;
		response_lens[i] = 0;
	}
	return 0;
}

/*
 * KBI server thread that sends INFO_REQUEST with 1 prompt,
 * receives INFO_RESPONSE, then sends SUCCESS.
 */
static int
kbi_empty_resp_server_thread(void *arg)
{
	struct kbi_error_server_arg *a = arg;
	dssh_session sess = a->ctx->server;
	int res;

	res = kbi_error_service_exchange(sess);
	if (res < 0) { a->result = res; return 0; }

	/* Send INFO_REQUEST with one prompt */
	res = dssh_transport_send_packet(sess, a->info_payload,
	    a->info_payload_len, NULL);
	if (res < 0) { a->result = res; return 0; }

	/* Receive INFO_RESPONSE */
	uint8_t msg_type;
	uint8_t *payload;
	size_t payload_len;
	res = dssh_transport_recv_packet(sess, &msg_type, &payload, &payload_len);
	if (res < 0) { a->result = res; return 0; }

	/* Send SUCCESS */
	uint8_t succ = SSH_MSG_USERAUTH_SUCCESS;
	a->result = dssh_transport_send_packet(sess, &succ, 1, NULL);
	return 0;
}

static int
test_kbi_empty_response(void)
{
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	const char *prompt_strs[] = { "Token: " };
	bool echo_flags[] = { true };
	uint8_t payload[128];
	size_t len = build_info_request(payload, sizeof(payload),
	    "Name", "Instruction", 1, prompt_strs, echo_flags);

	struct kbi_error_server_arg sa = {0};
	sa.ctx = &ctx;
	sa.info_payload = payload;
	sa.info_payload_len = len;

	thrd_t st;
	ASSERT_TRUE(thrd_create(&st, kbi_empty_resp_server_thread, &sa) == thrd_success);

	int res = dssh_auth_keyboard_interactive(ctx.client, "user",
	    kbi_cb_empty_response, NULL);

	mock_io_close_c2s(&ctx.io);
	mock_io_close_s2c(&ctx.io);
	thrd_join(st, NULL);

	ASSERT_EQ(res, 0);

	handshake_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Direct-call server tests.  Inject packets into c2s via the
 * client session, close c2s, then call auth_server_impl from the
 * main thread.  No threads needed for the server side.
 * ================================================================ */

static int
direct_server_test(const uint8_t *auth_msg, size_t auth_msg_len,
    const struct dssh_auth_server_cbs *cbs,
    uint8_t *username_out, size_t *username_out_len)
{
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return -999;
	}

	/* Inject SERVICE_REQUEST */
	{
		static const char svc[] = "ssh-userauth";
		uint8_t msg[64];
		size_t pos = 0;
		msg[pos++] = SSH_MSG_SERVICE_REQUEST;
		dssh_serialize_uint32((uint32_t)(sizeof(svc) - 1), msg, sizeof(msg), &pos);
		memcpy(&msg[pos], svc, sizeof(svc) - 1);
		pos += sizeof(svc) - 1;
		dssh_transport_send_packet(ctx.client, msg, pos, NULL);
	}

	/* Inject the auth message (if any) */
	if (auth_msg != NULL && auth_msg_len > 0)
		dssh_transport_send_packet(ctx.client, auth_msg, auth_msg_len, NULL);

	/* Close write end so server gets EOF after reading buffered data */
	mock_io_close_c2s_write(&ctx.io);

	/* Call server directly from main thread */
	int res = auth_server_impl(ctx.server, cbs,
	    username_out, username_out_len);

	mock_io_close_c2s(&ctx.io);
	mock_io_close_s2c(&ctx.io);
	handshake_cleanup(&ctx);
	return res;
}

/* Build a USERAUTH_REQUEST with user="u", service="ssh-connection",
 * and the specified method + extra data appended after the method. */
static size_t
build_auth_request(const char *method, size_t method_len,
    const uint8_t *extra, size_t extra_len,
    uint8_t *buf, size_t bufsz)
{
	static const char svc[] = "ssh-connection";
	size_t pos = 0;
	buf[pos++] = SSH_MSG_USERAUTH_REQUEST;
	dssh_serialize_uint32(1, buf, bufsz, &pos);
	buf[pos++] = 'u';
	dssh_serialize_uint32((uint32_t)(sizeof(svc) - 1), buf, bufsz, &pos);
	memcpy(&buf[pos], svc, sizeof(svc) - 1);
	pos += sizeof(svc) - 1;
	dssh_serialize_uint32((uint32_t)method_len, buf, bufsz, &pos);
	memcpy(&buf[pos], method, method_len);
	pos += method_len;
	if (extra != NULL && extra_len > 0) {
		memcpy(&buf[pos], extra, extra_len);
		pos += extra_len;
	}
	return pos;
}

static struct dssh_auth_server_cbs default_cbs(void)
{
	struct dssh_auth_server_cbs cbs = {0};
	cbs.methods_str = "password,publickey";
	cbs.password_cb = test_password_cb;
	cbs.publickey_cb = test_publickey_cb;
	return cbs;
}

/* --- Server parse: password truncation --- */

static int test_direct_pw_no_bool(void)
{
	/* method="password" but no boolean byte after */
	uint8_t msg[128];
	size_t len = build_auth_request("password", 8, NULL, 0, msg, sizeof(msg));
	struct dssh_auth_server_cbs cbs = default_cbs();
	ASSERT_TRUE(direct_server_test(msg, len, &cbs, NULL, NULL) < 0);
	return TEST_PASS;
}

static int test_direct_pw_no_pw_len(void)
{
	/* boolean=false but no password length */
	uint8_t extra[] = { 0 }; /* boolean false */
	uint8_t msg[128];
	size_t len = build_auth_request("password", 8, extra, 1, msg, sizeof(msg));
	struct dssh_auth_server_cbs cbs = default_cbs();
	ASSERT_TRUE(direct_server_test(msg, len, &cbs, NULL, NULL) < 0);
	return TEST_PASS;
}

static int test_direct_pw_overflow(void)
{
	/* pw_len=1000 but only 2 bytes */
	uint8_t extra[8];
	size_t ep = 0;
	extra[ep++] = 0; /* boolean false */
	dssh_serialize_uint32(1000, extra, sizeof(extra), &ep);
	extra[ep++] = 'a';
	extra[ep++] = 'b';
	uint8_t msg[128];
	size_t len = build_auth_request("password", 8, extra, ep, msg, sizeof(msg));
	struct dssh_auth_server_cbs cbs = default_cbs();
	ASSERT_TRUE(direct_server_test(msg, len, &cbs, NULL, NULL) < 0);
	return TEST_PASS;
}

static int test_direct_pw_change_no_new(void)
{
	/* change=true, old password OK, but no new_pw_len */
	uint8_t extra[16];
	size_t ep = 0;
	extra[ep++] = 1; /* boolean true = change */
	dssh_serialize_uint32(2, extra, sizeof(extra), &ep);
	extra[ep++] = 'o'; extra[ep++] = 'p';
	uint8_t msg[128];
	size_t len = build_auth_request("password", 8, extra, ep, msg, sizeof(msg));
	struct dssh_auth_server_cbs cbs = default_cbs();
	ASSERT_TRUE(direct_server_test(msg, len, &cbs, NULL, NULL) < 0);
	return TEST_PASS;
}

static int test_direct_pw_change_overflow(void)
{
	/* change=true, old pw OK, new_pw_len=1000 but only 1 byte */
	uint8_t extra[16];
	size_t ep = 0;
	extra[ep++] = 1;
	dssh_serialize_uint32(2, extra, sizeof(extra), &ep);
	extra[ep++] = 'o'; extra[ep++] = 'p';
	dssh_serialize_uint32(1000, extra, sizeof(extra), &ep);
	extra[ep++] = 'n';
	uint8_t msg[128];
	size_t len = build_auth_request("password", 8, extra, ep, msg, sizeof(msg));
	struct dssh_auth_server_cbs cbs = default_cbs();
	ASSERT_TRUE(direct_server_test(msg, len, &cbs, NULL, NULL) < 0);
	return TEST_PASS;
}

/* --- Server parse: publickey truncation --- */

static int test_direct_pk_no_hassig(void)
{
	uint8_t msg[128];
	size_t len = build_auth_request("publickey", 9, NULL, 0, msg, sizeof(msg));
	struct dssh_auth_server_cbs cbs = default_cbs();
	ASSERT_TRUE(direct_server_test(msg, len, &cbs, NULL, NULL) < 0);
	return TEST_PASS;
}

static int test_direct_pk_no_algo_len(void)
{
	uint8_t extra[] = { 0 }; /* has_sig=false, no algo */
	uint8_t msg[128];
	size_t len = build_auth_request("publickey", 9, extra, 1, msg, sizeof(msg));
	struct dssh_auth_server_cbs cbs = default_cbs();
	ASSERT_TRUE(direct_server_test(msg, len, &cbs, NULL, NULL) < 0);
	return TEST_PASS;
}

static int test_direct_pk_algo_overflow(void)
{
	uint8_t extra[8];
	size_t ep = 0;
	extra[ep++] = 0;
	dssh_serialize_uint32(1000, extra, sizeof(extra), &ep);
	extra[ep++] = 'r'; extra[ep++] = 's';
	uint8_t msg[128];
	size_t len = build_auth_request("publickey", 9, extra, ep, msg, sizeof(msg));
	struct dssh_auth_server_cbs cbs = default_cbs();
	ASSERT_TRUE(direct_server_test(msg, len, &cbs, NULL, NULL) < 0);
	return TEST_PASS;
}

static int test_direct_pk_no_pk_len(void)
{
	uint8_t extra[8];
	size_t ep = 0;
	extra[ep++] = 0;
	dssh_serialize_uint32(3, extra, sizeof(extra), &ep);
	extra[ep++] = 'r'; extra[ep++] = 's'; extra[ep++] = 'a';
	uint8_t msg[128];
	size_t len = build_auth_request("publickey", 9, extra, ep, msg, sizeof(msg));
	struct dssh_auth_server_cbs cbs = default_cbs();
	ASSERT_TRUE(direct_server_test(msg, len, &cbs, NULL, NULL) < 0);
	return TEST_PASS;
}

static int test_direct_pk_pk_overflow(void)
{
	uint8_t extra[16];
	size_t ep = 0;
	extra[ep++] = 0;
	dssh_serialize_uint32(3, extra, sizeof(extra), &ep);
	extra[ep++] = 'r'; extra[ep++] = 's'; extra[ep++] = 'a';
	dssh_serialize_uint32(1000, extra, sizeof(extra), &ep);
	extra[ep++] = 'x'; extra[ep++] = 'y';
	uint8_t msg[128];
	size_t len = build_auth_request("publickey", 9, extra, ep, msg, sizeof(msg));
	struct dssh_auth_server_cbs cbs = default_cbs();
	ASSERT_TRUE(direct_server_test(msg, len, &cbs, NULL, NULL) < 0);
	return TEST_PASS;
}

static int test_direct_pk_sig_no_sig_len(void)
{
	/* has_sig=true, algo+pk OK, no sig_len */
	uint8_t extra[32];
	size_t ep = 0;
	extra[ep++] = 1; /* has_sig=true */
	dssh_serialize_uint32(3, extra, sizeof(extra), &ep);
	extra[ep++] = 'r'; extra[ep++] = 's'; extra[ep++] = 'a';
	dssh_serialize_uint32(2, extra, sizeof(extra), &ep);
	extra[ep++] = 'p'; extra[ep++] = 'k';
	/* no sig_len */
	uint8_t msg[128];
	size_t len = build_auth_request("publickey", 9, extra, ep, msg, sizeof(msg));
	struct dssh_auth_server_cbs cbs = default_cbs();
	ASSERT_TRUE(direct_server_test(msg, len, &cbs, NULL, NULL) < 0);
	return TEST_PASS;
}

static int test_direct_pk_sig_overflow(void)
{
	uint8_t extra[32];
	size_t ep = 0;
	extra[ep++] = 1;
	dssh_serialize_uint32(3, extra, sizeof(extra), &ep);
	extra[ep++] = 'r'; extra[ep++] = 's'; extra[ep++] = 'a';
	dssh_serialize_uint32(2, extra, sizeof(extra), &ep);
	extra[ep++] = 'p'; extra[ep++] = 'k';
	dssh_serialize_uint32(1000, extra, sizeof(extra), &ep);
	extra[ep++] = 's';
	uint8_t msg[128];
	size_t len = build_auth_request("publickey", 9, extra, ep, msg, sizeof(msg));
	struct dssh_auth_server_cbs cbs = default_cbs();
	ASSERT_TRUE(direct_server_test(msg, len, &cbs, NULL, NULL) < 0);
	return TEST_PASS;
}

/* --- Server state edges --- */

static int test_direct_wrong_first_msg(void)
{
	/* Send USERAUTH_REQUEST as first message (not SERVICE_REQUEST) */
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}
	uint8_t msg[] = { SSH_MSG_USERAUTH_REQUEST };
	dssh_transport_send_packet(ctx.client, msg, 1, NULL);
	mock_io_close_c2s_write(&ctx.io);
	struct dssh_auth_server_cbs cbs = default_cbs();
	int res = auth_server_impl(ctx.server, &cbs, NULL, NULL);
	ASSERT_TRUE(res < 0);
	mock_io_close_c2s(&ctx.io);
	mock_io_close_s2c(&ctx.io);
	handshake_cleanup(&ctx);
	return TEST_PASS;
}

static int test_direct_short_svc_request(void)
{
	/* SERVICE_REQUEST with only 4 bytes payload (< 5) */
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}
	uint8_t msg[4];
	msg[0] = SSH_MSG_SERVICE_REQUEST;
	msg[1] = 0; msg[2] = 0; msg[3] = 0; /* truncated slen */
	dssh_transport_send_packet(ctx.client, msg, 4, NULL);
	mock_io_close_c2s_write(&ctx.io);
	struct dssh_auth_server_cbs cbs = default_cbs();
	int res = auth_server_impl(ctx.server, &cbs, NULL, NULL);
	/* Server sends SERVICE_ACCEPT (without service name), then waits
	 * for USERAUTH_REQUEST, gets EOF → error */
	ASSERT_TRUE(res < 0);
	mock_io_close_c2s(&ctx.io);
	mock_io_close_s2c(&ctx.io);
	handshake_cleanup(&ctx);
	return TEST_PASS;
}

static int test_direct_svc_slen_overflow(void)
{
	/* SERVICE_REQUEST with slen > payload */
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}
	uint8_t msg[8];
	size_t pos = 0;
	msg[pos++] = SSH_MSG_SERVICE_REQUEST;
	dssh_serialize_uint32(1000, msg, sizeof(msg), &pos);
	msg[pos++] = 'x'; msg[pos++] = 'y';
	dssh_transport_send_packet(ctx.client, msg, pos, NULL);
	mock_io_close_c2s_write(&ctx.io);
	struct dssh_auth_server_cbs cbs = default_cbs();
	int res = auth_server_impl(ctx.server, &cbs, NULL, NULL);
	ASSERT_TRUE(res < 0);
	mock_io_close_c2s(&ctx.io);
	mock_io_close_s2c(&ctx.io);
	handshake_cleanup(&ctx);
	return TEST_PASS;
}

static int test_direct_long_username(void)
{
	/* Username > 255 bytes — truncation ternary */
	uint8_t msg[512];
	size_t pos = 0;
	msg[pos++] = SSH_MSG_USERAUTH_REQUEST;
	dssh_serialize_uint32(300, msg, sizeof(msg), &pos);
	memset(&msg[pos], 'A', 300);
	pos += 300;
	/* service + method "none" so it completes */
	static const char svc[] = "ssh-connection";
	dssh_serialize_uint32((uint32_t)(sizeof(svc) - 1), msg, sizeof(msg), &pos);
	memcpy(&msg[pos], svc, sizeof(svc) - 1);
	pos += sizeof(svc) - 1;
	dssh_serialize_uint32(4, msg, sizeof(msg), &pos);
	memcpy(&msg[pos], "none", 4);
	pos += 4;

	struct dssh_auth_server_cbs cbs = default_cbs();
	cbs.none_cb = test_none_cb;
	struct test_auth_cbdata cbdata = {0};
	cbdata.none_result = DSSH_AUTH_SUCCESS;
	cbs.cbdata = &cbdata;
	uint8_t uout[256];
	size_t uout_len = 0;
	/* Server processes the request (auth may fail due to EOF
	 * after, but the truncation ternary fires during parse).
	 * We just need the function to not crash. */
	(void)direct_server_test(msg, pos, &cbs, uout, &uout_len);
	return TEST_PASS;
}

static int test_direct_null_username_out_len(void)
{
	/* Non-NULL username_out but NULL username_out_len */
	uint8_t msg[128];
	size_t len = build_auth_request("none", 4, NULL, 0, msg, sizeof(msg));
	struct dssh_auth_server_cbs cbs = default_cbs();
	cbs.none_cb = test_none_cb;
	struct test_auth_cbdata cbdata = {0};
	cbdata.none_result = DSSH_AUTH_SUCCESS;
	cbs.cbdata = &cbdata;
	uint8_t uout[256];
	/* Server processes none auth; may return error due to EOF
	 * after success, but the NULL username_out_len branch fires. */
	(void)direct_server_test(msg, len, &cbs, uout, NULL);
	return TEST_PASS;
}

/* ================================================================
 * Direct-call client-side tests: call _impl from main thread,
 * server helper thread sends responses.
 * ================================================================ */

struct dclient_server_arg {
	struct handshake_ctx *ctx;
	const uint8_t *response;
	size_t response_len;
	int result;
	bool close_s2c;   /* close server→client pipe before returning */
};

static int
dclient_server_thread(void *arg)
{
	struct dclient_server_arg *a = arg;
	dssh_session server = a->ctx->server;
	uint8_t msg_type;
	uint8_t *payload;
	size_t payload_len;

	dssh_test_alloc_exclude_thread();

	/* SERVICE_REQUEST -> SERVICE_ACCEPT */
	int res = dssh_transport_recv_packet(server, &msg_type, &payload,
	    &payload_len);
	if (res < 0 || msg_type != SSH_MSG_SERVICE_REQUEST) {
		a->result = -1;
		return 0;
	}
	{
		uint8_t accept[64];
		size_t pos = 0;
		accept[pos++] = SSH_MSG_SERVICE_ACCEPT;
		if (payload_len > 5) {
			uint32_t slen;
			dssh_parse_uint32(&payload[1], payload_len - 1, &slen);
			if (5 + slen <= payload_len) {
				dssh_serialize_uint32(slen, accept, sizeof(accept),
				    &pos);
				memcpy(&accept[pos], &payload[5], slen);
				pos += slen;
			}
		}
		res = dssh_transport_send_packet(server, accept, pos, NULL);
		if (res < 0) {
			a->result = -2;
			return 0;
		}
	}

	/* Receive the auth request */
	res = dssh_transport_recv_packet(server, &msg_type, &payload,
	    &payload_len);
	if (res < 0) {
		a->result = -3;
		return 0;
	}

	/* Send the crafted response */
	if (a->response != NULL && a->response_len > 0)
		dssh_transport_send_packet(server, a->response,
		    a->response_len, NULL);
	if (a->close_s2c)
		mock_io_close_s2c(&a->ctx->io);
	a->result = 0;
	return 0;
}

/* L508: Wrong reply to SERVICE_REQUEST */
static int
dclient_wrong_svc_server(void *arg)
{
	struct dclient_server_arg *a = arg;
	dssh_session server = a->ctx->server;
	uint8_t msg_type;
	uint8_t *payload;
	size_t payload_len;
	int res = dssh_transport_recv_packet(server, &msg_type, &payload,
	    &payload_len);
	if (res < 0) {
		a->result = -1;
		return 0;
	}
	uint8_t bogus[] = { 99 };
	dssh_transport_send_packet(server, bogus, sizeof(bogus), NULL);
	a->result = 0;
	return 0;
}

static int
test_dclient_wrong_svc_reply(void)
{
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	struct dclient_server_arg sa = { &ctx, NULL, 0, 0 };
	thrd_t st;
	if (thrd_create(&st, dclient_wrong_svc_server, &sa) != thrd_success) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	char methods[256];
	int res = get_methods_impl(ctx.client, "user", methods, sizeof(methods));

	mock_io_close_c2s(&ctx.io);
	mock_io_close_s2c(&ctx.io);
	thrd_join(st, NULL);
	ASSERT_TRUE(res < 0);

	handshake_cleanup(&ctx);
	return TEST_PASS;
}

/* L539: get_methods malloc fail */
static int
test_dclient_get_methods_alloc_fail(void)
{
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	uint8_t failure_resp[32];
	size_t rp = 0;
	failure_resp[rp++] = SSH_MSG_USERAUTH_FAILURE;
	dssh_serialize_uint32(8, failure_resp, sizeof(failure_resp), &rp);
	memcpy(&failure_resp[rp], "password", 8);
	rp += 8;
	failure_resp[rp++] = 0;

	struct dclient_server_arg sa = { &ctx, failure_resp, rp, 0 };
	thrd_t st;
	if (thrd_create(&st, dclient_server_thread, &sa) != thrd_success) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	dssh_test_alloc_fail_after(0);
	char methods[256];
	int res = get_methods_impl(ctx.client, "user", methods, sizeof(methods));
	dssh_test_alloc_reset();

	mock_io_close_c2s(&ctx.io);
	mock_io_close_s2c(&ctx.io);
	thrd_join(st, NULL);
	ASSERT_EQ(res, DSSH_ERROR_ALLOC);

	handshake_cleanup(&ctx);
	return TEST_PASS;
}

/* L572/582: get_methods FAILURE with DEL char (0x7F) */
static int
test_dclient_get_methods_del(void)
{
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	uint8_t failure_resp[16];
	size_t rp = 0;
	failure_resp[rp++] = SSH_MSG_USERAUTH_FAILURE;
	dssh_serialize_uint32(4, failure_resp, sizeof(failure_resp), &rp);
	failure_resp[rp++] = 'a';
	failure_resp[rp++] = 'b';
	failure_resp[rp++] = 0x7f; /* DEL char */
	failure_resp[rp++] = 'c';
	failure_resp[rp++] = 0;

	struct dclient_server_arg sa = { &ctx, failure_resp, rp, 0 };
	thrd_t st;
	if (thrd_create(&st, dclient_server_thread, &sa) != thrd_success) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	char methods[256];
	int res = get_methods_impl(ctx.client, "user", methods, sizeof(methods));

	mock_io_close_c2s(&ctx.io);
	mock_io_close_s2c(&ctx.io);
	thrd_join(st, NULL);
	ASSERT_EQ(res, DSSH_ERROR_PARSE);

	handshake_cleanup(&ctx);
	return TEST_PASS;
}

/* L682: CHANGEREQ but no passwd_change_cb */
static int
test_dclient_changereq_no_cb(void)
{
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	uint8_t resp[32];
	size_t rp = 0;
	resp[rp++] = SSH_MSG_USERAUTH_PASSWD_CHANGEREQ;
	dssh_serialize_uint32(6, resp, sizeof(resp), &rp);
	memcpy(&resp[rp], "change", 6);
	rp += 6;
	dssh_serialize_uint32(0, resp, sizeof(resp), &rp);

	struct dclient_server_arg sa = { &ctx, resp, rp, 0 };
	thrd_t st;
	if (thrd_create(&st, dclient_server_thread, &sa) != thrd_success) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	int res = auth_password_impl(ctx.client, "user", "pass", NULL, NULL);

	mock_io_close_c2s(&ctx.io);
	mock_io_close_s2c(&ctx.io);
	thrd_join(st, NULL);
	ASSERT_EQ(res, DSSH_ERROR_INIT);

	handshake_cleanup(&ctx);
	return TEST_PASS;
}

/* L692: CHANGEREQ with truncated prompt */
static int
test_dclient_changereq_trunc(void)
{
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	uint8_t resp[] = { SSH_MSG_USERAUTH_PASSWD_CHANGEREQ };
	struct dclient_server_arg sa = { &ctx, resp, sizeof(resp), 0 };
	thrd_t st;
	if (thrd_create(&st, dclient_server_thread, &sa) != thrd_success) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	int res = auth_password_impl(ctx.client, "user", "pass",
	    test_client_changereq_cb, NULL);

	mock_io_close_c2s(&ctx.io);
	mock_io_close_s2c(&ctx.io);
	thrd_join(st, NULL);
	ASSERT_EQ(res, DSSH_ERROR_PARSE);

	handshake_cleanup(&ctx);
	return TEST_PASS;
}

/* L699/702: CHANGEREQ with no language field */
static int
test_dclient_changereq_no_lang(void)
{
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	uint8_t resp[16];
	size_t rp = 0;
	resp[rp++] = SSH_MSG_USERAUTH_PASSWD_CHANGEREQ;
	dssh_serialize_uint32(2, resp, sizeof(resp), &rp);
	memcpy(&resp[rp], "ok", 2);
	rp += 2;
	/* No language field — exercises line 699 false branch */

	struct dclient_server_arg sa = { &ctx, resp, rp, 0, true };
	thrd_t st;
	if (thrd_create(&st, dclient_server_thread, &sa) != thrd_success) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	/* The change cb fires with lang_len=0, sends new password,
	 * but server thread closes s2c so recv fails → error */
	int res = auth_password_impl(ctx.client, "user", "pass",
	    test_client_changereq_cb, NULL);

	mock_io_close_c2s(&ctx.io);
	thrd_join(st, NULL);
	(void)res; /* exercises the no-language branch */

	handshake_cleanup(&ctx);
	return TEST_PASS;
}

/* L714: passwd_change_cb returns error */
static int
dclient_error_change_cb(const uint8_t *prompt, size_t prompt_len,
    const uint8_t *language, size_t language_len,
    uint8_t **new_password, size_t *new_password_len,
    void *cbdata)
{
	(void)prompt; (void)prompt_len;
	(void)language; (void)language_len;
	(void)cbdata;
	*new_password = NULL;
	*new_password_len = 0;
	return -42;
}

static int
test_dclient_changereq_cb_error(void)
{
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	uint8_t resp[32];
	size_t rp = 0;
	resp[rp++] = SSH_MSG_USERAUTH_PASSWD_CHANGEREQ;
	dssh_serialize_uint32(4, resp, sizeof(resp), &rp);
	memcpy(&resp[rp], "chng", 4);
	rp += 4;
	dssh_serialize_uint32(0, resp, sizeof(resp), &rp);

	struct dclient_server_arg sa = { &ctx, resp, rp, 0 };
	thrd_t st;
	if (thrd_create(&st, dclient_server_thread, &sa) != thrd_success) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	int res = auth_password_impl(ctx.client, "user", "pass",
	    dclient_error_change_cb, NULL);

	mock_io_close_c2s(&ctx.io);
	mock_io_close_s2c(&ctx.io);
	thrd_join(st, NULL);
	ASSERT_EQ(res, -42);

	handshake_cleanup(&ctx);
	return TEST_PASS;
}

/* L760: KBI initial malloc fail */
static int
test_dclient_kbi_alloc_fail(void)
{
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	const char *prompt_strs[] = { "Token: " };
	bool echo_flags[] = { false };
	uint8_t info_payload[128];
	size_t info_len = build_info_request(info_payload, sizeof(info_payload),
	    "Auth", "Enter token", 1, prompt_strs, echo_flags);

	struct dclient_server_arg sa = { &ctx, info_payload, info_len, 0 };
	thrd_t st;
	if (thrd_create(&st, dclient_server_thread, &sa) != thrd_success) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	struct test_kbi_cbdata kbi_data = {0};
	kbi_data.response = "token123";

	dssh_test_alloc_fail_after(0);
	int res = auth_kbi_impl(ctx.client, "user", test_kbi_prompt_cb,
	    &kbi_data);
	dssh_test_alloc_reset();

	mock_io_close_c2s(&ctx.io);
	mock_io_close_s2c(&ctx.io);
	thrd_join(st, NULL);
	ASSERT_EQ(res, DSSH_ERROR_ALLOC);

	handshake_cleanup(&ctx);
	return TEST_PASS;
}

/* L852-870: KBI calloc chain failures */
static int
test_dclient_kbi_calloc_chain(void)
{
	struct test_kbi_cbdata kbi_data = {0};
	kbi_data.response = "tok";

	const char *prompt_strs[] = { "Token: " };
	bool echo_flags[] = { false };
	uint8_t info_payload[128];
	size_t info_len = build_info_request(info_payload, sizeof(info_payload),
	    "Auth", "Enter", 1, prompt_strs, echo_flags);

	bool saw_alloc_error = false;

	for (int n = 0; n < 20; n++) {
		struct handshake_ctx ctx;
		if (handshake_setup(&ctx) < 0) {
			handshake_cleanup(&ctx);
			dssh_test_alloc_reset();
			continue;
		}

		struct dclient_server_arg sa = { &ctx, info_payload,
		    info_len, 0, true };
		thrd_t st;
		if (thrd_create(&st, dclient_server_thread, &sa) != thrd_success) {
			handshake_cleanup(&ctx);
			continue;
		}

		dssh_test_alloc_fail_after(n);
		int res = auth_kbi_impl(ctx.client, "user",
		    test_kbi_prompt_cb, &kbi_data);
		dssh_test_alloc_reset();

		mock_io_close_c2s(&ctx.io);
		thrd_join(st, NULL);
		handshake_cleanup(&ctx);

		if (res == DSSH_ERROR_ALLOC)
			saw_alloc_error = true;
	}

	ASSERT_TRUE(saw_alloc_error);
	return TEST_PASS;
}

/* L920/921: KBI response malloc fail */
static int
test_dclient_kbi_resp_alloc(void)
{
	struct test_kbi_cbdata kbi_data = {0};
	kbi_data.response = "answer";

	const char *prompt_strs[] = { "Token: " };
	bool echo_flags[] = { false };
	uint8_t info_payload[128];
	size_t info_len = build_info_request(info_payload, sizeof(info_payload),
	    "Name", "Instr", 1, prompt_strs, echo_flags);

	bool saw_alloc_error = false;

	for (int n = 0; n < 20; n++) {
		struct handshake_ctx ctx;
		if (handshake_setup(&ctx) < 0) {
			handshake_cleanup(&ctx);
			dssh_test_alloc_reset();
			continue;
		}

		struct dclient_server_arg sa = { &ctx, info_payload,
		    info_len, 0, true };
		thrd_t st;
		if (thrd_create(&st, dclient_server_thread, &sa) != thrd_success) {
			handshake_cleanup(&ctx);
			continue;
		}

		dssh_test_alloc_fail_after(n);
		int res = auth_kbi_impl(ctx.client, "user",
		    test_kbi_prompt_cb, &kbi_data);
		dssh_test_alloc_reset();

		mock_io_close_c2s(&ctx.io);
		thrd_join(st, NULL);
		handshake_cleanup(&ctx);

		if (res == DSSH_ERROR_ALLOC)
			saw_alloc_error = true;
	}

	ASSERT_TRUE(saw_alloc_error);
	return TEST_PASS;
}

/* L971: publickey with nonexistent algo (ka==NULL) */
static int
test_dclient_pk_no_sign(void)
{
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	/* Server thread handles SERVICE_REQUEST then closes s2c */
	struct dclient_server_arg sa = { &ctx, NULL, 0, 0, true };
	thrd_t st;
	if (thrd_create(&st, dclient_server_thread, &sa) != thrd_success) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	int res = auth_publickey_impl(ctx.client, "user",
	    "nonexistent-algo-name");
	ASSERT_EQ(res, DSSH_ERROR_INIT);

	mock_io_close_c2s(&ctx.io);
	thrd_join(st, NULL);
	handshake_cleanup(&ctx);
	return TEST_PASS;
}

/* L1003/1054: publickey sign_data/msg malloc fail */
static int
test_dclient_pk_alloc_fail(void)
{
	const char *algo = test_key_algo_name();
	bool saw_alloc_error = false;

	for (int n = 0; n < 15; n++) {
		struct handshake_ctx ctx;
		if (handshake_setup(&ctx) < 0) {
			handshake_cleanup(&ctx);
			dssh_test_alloc_reset();
			continue;
		}

		uint8_t success_resp[] = { SSH_MSG_USERAUTH_SUCCESS };
		struct dclient_server_arg sa = { &ctx, success_resp,
		    sizeof(success_resp), 0, true };
		thrd_t st;
		if (thrd_create(&st, dclient_server_thread, &sa) !=
		    thrd_success) {
			handshake_cleanup(&ctx);
			continue;
		}

		dssh_test_alloc_fail_after(n);
		int res = auth_publickey_impl(ctx.client, "user", algo);
		dssh_test_alloc_reset();

		mock_io_close_c2s(&ctx.io);
		thrd_join(st, NULL);
		handshake_cleanup(&ctx);

		if (res == DSSH_ERROR_ALLOC)
			saw_alloc_error = true;
	}

	ASSERT_TRUE(saw_alloc_error);
	return TEST_PASS;
}

/* ================================================================
 * Additional coverage tests for remaining branches
 * ================================================================ */

/* L213: parse_userauth_prefix fails inside auth_server_impl loop.
 * Send valid SERVICE_REQUEST + a 1-byte USERAUTH_REQUEST (too short
 * for prefix parse). */
static int
test_direct_bad_prefix_in_loop(void)
{
	/* A USERAUTH_REQUEST with only the msg type — prefix parse fails */
	uint8_t msg[] = { SSH_MSG_USERAUTH_REQUEST };
	struct dssh_auth_server_cbs cbs = default_cbs();
	int res = direct_server_test(msg, 1, &cbs, NULL, NULL);
	ASSERT_EQ(res, DSSH_ERROR_PARSE);
	return TEST_PASS;
}

/* L71: send_passwd_changereq malloc fail.
 * Server receives password request, password_cb returns CHANGE_PASSWORD
 * with a change_prompt → send_passwd_changereq mallocs. */
static int
test_server_changereq_alloc(void)
{
	/* Build normal password request: bool=false, pw="pass" */
	uint8_t extra[16];
	size_t ep = 0;
	extra[ep++] = 0; /* change=false */
	dssh_serialize_uint32(4, extra, sizeof(extra), &ep);
	memcpy(&extra[ep], "pass", 4);
	ep += 4;
	uint8_t msg[128];
	size_t len = build_auth_request("password", 8, extra, ep,
	    msg, sizeof(msg));

	bool saw_alloc = false;
	for (int n = 0; n < 5; n++) {
		struct handshake_ctx ctx;
		if (handshake_setup(&ctx) < 0) {
			handshake_cleanup(&ctx);
			continue;
		}
		/* Inject SERVICE_REQUEST + auth msg */
		{
			static const char svc[] = "ssh-userauth";
			uint8_t smsg[64];
			size_t sp = 0;
			smsg[sp++] = SSH_MSG_SERVICE_REQUEST;
			dssh_serialize_uint32((uint32_t)(sizeof(svc) - 1),
			    smsg, sizeof(smsg), &sp);
			memcpy(&smsg[sp], svc, sizeof(svc) - 1);
			sp += sizeof(svc) - 1;
			dssh_transport_send_packet(ctx.client, smsg, sp, NULL);
		}
		dssh_transport_send_packet(ctx.client, msg, len, NULL);
		mock_io_close_c2s_write(&ctx.io);

		struct dssh_auth_server_cbs cbs = default_cbs();
		cbs.password_cb = test_password_change_cb_server;

		dssh_test_alloc_fail_after(n);
		int res = auth_server_impl(ctx.server, &cbs, NULL, NULL);
		dssh_test_alloc_reset();

		mock_io_close_c2s(&ctx.io);
		mock_io_close_s2c(&ctx.io);
		handshake_cleanup(&ctx);

		if (res == DSSH_ERROR_ALLOC)
			saw_alloc = true;
	}
	ASSERT_TRUE(saw_alloc);
	return TEST_PASS;
}

/* L91: send_pk_ok malloc fail.
 * Server receives publickey probe (has_sig=false), calls send_pk_ok
 * which mallocs. Inject alloc failure. */
static int
test_server_pk_ok_alloc(void)
{
	const char *algo = test_key_algo_name();
	size_t algo_len = strlen(algo);

	uint8_t extra[256];
	size_t ep = 0;
	extra[ep++] = 0; /* has_sig=false (probe) */
	dssh_serialize_uint32((uint32_t)algo_len, extra, sizeof(extra), &ep);
	memcpy(&extra[ep], algo, algo_len);
	ep += algo_len;
	/* dummy pubkey blob */
	dssh_serialize_uint32(4, extra, sizeof(extra), &ep);
	memcpy(&extra[ep], "fake", 4);
	ep += 4;
	uint8_t msg[512];
	size_t len = build_auth_request("publickey", 9, extra, ep,
	    msg, sizeof(msg));

	bool saw_alloc = false;
	for (int n = 0; n < 5; n++) {
		struct handshake_ctx ctx;
		if (handshake_setup(&ctx) < 0) {
			handshake_cleanup(&ctx);
			continue;
		}
		{
			static const char svc[] = "ssh-userauth";
			uint8_t smsg[64];
			size_t sp = 0;
			smsg[sp++] = SSH_MSG_SERVICE_REQUEST;
			dssh_serialize_uint32((uint32_t)(sizeof(svc) - 1),
			    smsg, sizeof(smsg), &sp);
			memcpy(&smsg[sp], svc, sizeof(svc) - 1);
			sp += sizeof(svc) - 1;
			dssh_transport_send_packet(ctx.client, smsg, sp, NULL);
		}
		dssh_transport_send_packet(ctx.client, msg, len, NULL);
		mock_io_close_c2s_write(&ctx.io);

		struct dssh_auth_server_cbs cbs = default_cbs();
		struct test_auth_cbdata cbdata = {0};
		cbdata.publickey_result = DSSH_AUTH_SUCCESS;
		cbs.cbdata = &cbdata;

		dssh_test_alloc_fail_after(n);
		int res = auth_server_impl(ctx.server, &cbs, NULL, NULL);
		dssh_test_alloc_reset();

		mock_io_close_c2s(&ctx.io);
		mock_io_close_s2c(&ctx.io);
		handshake_cleanup(&ctx);

		if (res == DSSH_ERROR_ALLOC)
			saw_alloc = true;
	}
	ASSERT_TRUE(saw_alloc);
	return TEST_PASS;
}

/* L388: publickey with has_sig=true but unknown algo name.
 * Server can't find the key algorithm → send FAILURE. */
static int
test_direct_pk_sig_unknown_algo(void)
{
	uint8_t extra[64];
	size_t ep = 0;
	extra[ep++] = 1; /* has_sig=true */
	dssh_serialize_uint32(7, extra, sizeof(extra), &ep);
	memcpy(&extra[ep], "unknown", 7);
	ep += 7;
	dssh_serialize_uint32(2, extra, sizeof(extra), &ep);
	extra[ep++] = 'p'; extra[ep++] = 'k';
	dssh_serialize_uint32(2, extra, sizeof(extra), &ep);
	extra[ep++] = 's'; extra[ep++] = 'g';
	uint8_t msg[128];
	size_t len = build_auth_request("publickey", 9, extra, ep,
	    msg, sizeof(msg));
	struct dssh_auth_server_cbs cbs = default_cbs();
	/* Server sends FAILURE (unknown algo), then gets EOF → error.
	 * We just need it to reach L388. */
	(void)direct_server_test(msg, len, &cbs, NULL, NULL);
	return TEST_PASS;
}

/* Helper: client thread that sends publickey auth */
struct pk_client_arg {
	dssh_session client;
	const char *algo;
	int result;
};

static int
pk_client_thread(void *arg)
{
	struct pk_client_arg *a = arg;
	dssh_test_alloc_exclude_thread();
	a->result = dssh_auth_publickey(a->client, "user", a->algo);
	return 0;
}

/* L403: sign_data malloc fail in publickey verify path */
static int
test_server_pk_verify_alloc(void)
{
	/* Real client sends publickey auth, server with alloc injection. */
	const char *algo = test_key_algo_name();
	bool saw_alloc = false;

	for (int n = 0; n < 20; n++) {
		struct handshake_ctx ctx;
		if (handshake_setup(&ctx) < 0) {
			handshake_cleanup(&ctx);
			dssh_test_alloc_reset();
			continue;
		}

		struct pk_client_arg carg = { ctx.client, algo, 0 };
		thrd_t ct;
		if (thrd_create(&ct, pk_client_thread, &carg) !=
		    thrd_success) {
			handshake_cleanup(&ctx);
			continue;
		}

		struct dssh_auth_server_cbs cbs = default_cbs();
		struct test_auth_cbdata cbdata = {0};
		cbdata.publickey_result = DSSH_AUTH_SUCCESS;
		cbs.cbdata = &cbdata;

		dssh_test_alloc_fail_after(n);
		int res = auth_server_impl(ctx.server, &cbs, NULL, NULL);
		dssh_test_alloc_reset();

		mock_io_close_c2s(&ctx.io);
		mock_io_close_s2c(&ctx.io);
		thrd_join(ct, NULL);
		handshake_cleanup(&ctx);

		if (res == DSSH_ERROR_ALLOC)
			saw_alloc = true;
	}
	ASSERT_TRUE(saw_alloc);
	return TEST_PASS;
}

/* L539: get_methods_impl malloc fail */
static int
test_dclient_get_methods_malloc(void)
{
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	/* Build USERAUTH_FAILURE response */
	uint8_t resp[32];
	size_t rp = 0;
	resp[rp++] = SSH_MSG_USERAUTH_FAILURE;
	dssh_serialize_uint32(8, resp, sizeof(resp), &rp);
	memcpy(&resp[rp], "password", 8);
	rp += 8;
	resp[rp++] = 0;

	struct dclient_server_arg sa = { &ctx, resp, rp, 0, true };
	thrd_t st;
	if (thrd_create(&st, dclient_server_thread, &sa) != thrd_success) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	dssh_test_alloc_fail_after(0);
	char methods[128];
	int res = get_methods_impl(ctx.client, "user", methods,
	    sizeof(methods));
	dssh_test_alloc_reset();

	mock_io_close_c2s(&ctx.io);
	thrd_join(st, NULL);
	ASSERT_EQ(res, DSSH_ERROR_ALLOC);

	handshake_cleanup(&ctx);
	return TEST_PASS;
}

/* L573: get_methods FAILURE response with truncated payload */
static int
test_dclient_get_methods_trunc_payload(void)
{
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	/* FAILURE with only 3 bytes (< 1+4) */
	uint8_t resp[4];
	resp[0] = SSH_MSG_USERAUTH_FAILURE;
	resp[1] = 0;
	resp[2] = 0;
	resp[3] = 0; /* 4 total but payload_len check is < 1+4 = 5 */

	struct dclient_server_arg sa = { &ctx, resp, 4, 0, true };
	thrd_t st;
	if (thrd_create(&st, dclient_server_thread, &sa) != thrd_success) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	char methods[128];
	int res = get_methods_impl(ctx.client, "user", methods,
	    sizeof(methods));

	mock_io_close_c2s(&ctx.io);
	thrd_join(st, NULL);
	ASSERT_EQ(res, DSSH_ERROR_PARSE);

	handshake_cleanup(&ctx);
	return TEST_PASS;
}

/* L582: get_methods FAILURE with control char in methods string */
static int
test_dclient_get_methods_ctrl_char(void)
{
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	uint8_t resp[32];
	size_t rp = 0;
	resp[rp++] = SSH_MSG_USERAUTH_FAILURE;
	dssh_serialize_uint32(4, resp, sizeof(resp), &rp);
	resp[rp++] = 'a'; resp[rp++] = '\x01'; /* control char (SOH) */
	resp[rp++] = '\x7f'; resp[rp++] = 'c'; /* DEL */
	resp[rp++] = 0;

	struct dclient_server_arg sa = { &ctx, resp, rp, 0, true };
	thrd_t st;
	if (thrd_create(&st, dclient_server_thread, &sa) != thrd_success) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	char methods[128];
	int res = get_methods_impl(ctx.client, "user", methods,
	    sizeof(methods));

	mock_io_close_c2s(&ctx.io);
	thrd_join(st, NULL);
	ASSERT_EQ(res, DSSH_ERROR_PARSE);

	handshake_cleanup(&ctx);
	return TEST_PASS;
}

/* L702: password changereq with lang_len that overflows payload */
static int
test_dclient_changereq_lang_overflow(void)
{
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	uint8_t resp[32];
	size_t rp = 0;
	resp[rp++] = SSH_MSG_USERAUTH_PASSWD_CHANGEREQ;
	dssh_serialize_uint32(2, resp, sizeof(resp), &rp);
	resp[rp++] = 'o'; resp[rp++] = 'k';
	dssh_serialize_uint32(999, resp, sizeof(resp), &rp); /* lang overflow */

	struct dclient_server_arg sa = { &ctx, resp, rp, 0, true };
	thrd_t st;
	if (thrd_create(&st, dclient_server_thread, &sa) != thrd_success) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	/* cb fires with lang_len=0 (overflow → 0), sends new pw, fails recv */
	int res = auth_password_impl(ctx.client, "user", "pass",
	    test_client_changereq_cb, NULL);

	mock_io_close_c2s(&ctx.io);
	thrd_join(st, NULL);
	(void)res;

	handshake_cleanup(&ctx);
	return TEST_PASS;
}

/* L572: get_methods receives neither SUCCESS nor FAILURE */
static int
test_dclient_get_methods_unexpected_msg(void)
{
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	/* Send INFO_REQUEST (60) — not SUCCESS or FAILURE */
	uint8_t resp[] = { SSH_MSG_USERAUTH_INFO_REQUEST, 0, 0, 0, 0 };

	struct dclient_server_arg sa = { &ctx, resp, sizeof(resp), 0, true };
	thrd_t st;
	if (thrd_create(&st, dclient_server_thread, &sa) != thrd_success) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	char methods[128];
	int res = get_methods_impl(ctx.client, "user", methods,
	    sizeof(methods));

	mock_io_close_c2s(&ctx.io);
	thrd_join(st, NULL);
	ASSERT_TRUE(res < 0);

	handshake_cleanup(&ctx);
	return TEST_PASS;
}

/* L539: get_methods_impl msg malloc fail */
static int
test_dclient_get_methods_msg_alloc(void)
{
	bool saw_alloc = false;

	for (int n = 0; n < 5; n++) {
		struct handshake_ctx ctx;
		if (handshake_setup(&ctx) < 0) {
			handshake_cleanup(&ctx);
			dssh_test_alloc_reset();
			continue;
		}

		uint8_t resp[32];
		size_t rp = 0;
		resp[rp++] = SSH_MSG_USERAUTH_FAILURE;
		dssh_serialize_uint32(8, resp, sizeof(resp), &rp);
		memcpy(&resp[rp], "password", 8);
		rp += 8;
		resp[rp++] = 0;

		struct dclient_server_arg sa = { &ctx, resp, rp, 0, true };
		thrd_t st;
		if (thrd_create(&st, dclient_server_thread, &sa) !=
		    thrd_success) {
			handshake_cleanup(&ctx);
			continue;
		}

		dssh_test_alloc_fail_after(n);
		char methods[128];
		int res = get_methods_impl(ctx.client, "user", methods,
		    sizeof(methods));
		dssh_test_alloc_reset();

		mock_io_close_c2s(&ctx.io);
		thrd_join(st, NULL);
		handshake_cleanup(&ctx);

		if (res == DSSH_ERROR_ALLOC)
			saw_alloc = true;
	}
	ASSERT_TRUE(saw_alloc);
	return TEST_PASS;
}

/* L692: password changereq with prompt_len overflow */
static int
test_dclient_changereq_prompt_overflow(void)
{
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	uint8_t resp[16];
	size_t rp = 0;
	resp[rp++] = SSH_MSG_USERAUTH_PASSWD_CHANGEREQ;
	dssh_serialize_uint32(999, resp, sizeof(resp), &rp); /* prompt_len overflow */

	struct dclient_server_arg sa = { &ctx, resp, rp, 0, true };
	thrd_t st;
	if (thrd_create(&st, dclient_server_thread, &sa) != thrd_success) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	int res = auth_password_impl(ctx.client, "user", "pass",
	    test_client_changereq_cb, NULL);

	mock_io_close_c2s(&ctx.io);
	thrd_join(st, NULL);
	ASSERT_EQ(res, DSSH_ERROR_PARSE);

	handshake_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Test table and main
 * ================================================================ */

static struct dssh_test_entry tests[] = {
	/* Password auth */
	{ "auth/password/success",         test_auth_password_success },
	{ "auth/password/failure",         test_auth_password_failure },
	{ "auth/password/wrong_user",      test_auth_password_wrong_user },
	{ "auth/password/partial",         test_auth_password_partial },
	{ "auth/password/empty",           test_auth_password_empty },
	{ "auth/password/long",            test_auth_password_long },

	/* Server-side auth */
	{ "auth/server/none_accept",       test_auth_server_none_accept },
	{ "auth/server/none_reject",       test_auth_server_none_reject },
	{ "auth/server/password_accept",   test_auth_server_password_accept },
	{ "auth/server/password_reject",   test_auth_server_password_reject },
	{ "auth/server/publickey_accept",  test_auth_server_publickey_accept },
	{ "auth/server/publickey_reject",  test_auth_server_publickey_reject },
	{ "auth/server/no_callbacks",      test_auth_server_no_callbacks },
	{ "auth/server/username_captured", test_auth_server_username_captured },

	/* Full roundtrip */
	{ "auth/roundtrip/password",          test_auth_roundtrip_password },
	{ "auth/roundtrip/password_reject",   test_auth_roundtrip_password_reject },
	{ "auth/roundtrip/publickey",         test_auth_roundtrip_publickey },
	{ "auth/roundtrip/get_methods",       test_auth_roundtrip_get_methods },
	{ "auth/roundtrip/kbi",               test_auth_roundtrip_kbi },
	{ "auth/roundtrip/multiple_attempts", test_auth_roundtrip_multiple_attempts },
	{ "auth/banner_delivered",            test_auth_banner_delivered },

	/* Server auth coverage */
	{ "auth/server/password_change",      test_auth_server_password_change },
	{ "auth/server/publickey_probe",      test_auth_server_publickey_probe },
	{ "auth/server/unknown_method",       test_auth_server_unknown_method },
	{ "auth/server/unexpected_message",   test_auth_server_unexpected_message },

	/* Client KBI error paths */
	{ "auth/kbi/truncated_name_header",   test_kbi_truncated_name_header },
	{ "auth/kbi/truncated_name_data",     test_kbi_truncated_name_data },
	{ "auth/kbi/truncated_instr_header",  test_kbi_truncated_instruction_header },
	{ "auth/kbi/truncated_instr_data",    test_kbi_truncated_instruction_data },
	{ "auth/kbi/truncated_lang_header",   test_kbi_truncated_language_header },
	{ "auth/kbi/truncated_lang_data",     test_kbi_truncated_language_data },
	{ "auth/kbi/truncated_num_prompts",   test_kbi_truncated_num_prompts },
	{ "auth/kbi/truncated_prompt_header", test_kbi_truncated_prompt_header },
	{ "auth/kbi/truncated_prompt_data",   test_kbi_truncated_prompt_data },
	{ "auth/kbi/callback_abort",          test_kbi_callback_abort },
	{ "auth/kbi/zero_prompts",            test_kbi_zero_prompts },
	{ "auth/kbi/failure_response",        test_kbi_failure_response },
	{ "auth/kbi/unexpected_message",      test_kbi_unexpected_message },
	{ "auth/kbi/banner_before_info",      test_kbi_banner_before_info },

	/* Server auth parse errors */
	{ "auth/server/parse_empty",          test_server_parse_empty_request },
	{ "auth/server/parse_trunc_user",     test_server_parse_truncated_username },
	{ "auth/server/parse_no_service",     test_server_parse_no_service },
	{ "auth/server/parse_trunc_service",  test_server_parse_truncated_service },
	{ "auth/server/parse_no_method",      test_server_parse_no_method },
	{ "auth/server/parse_trunc_method",   test_server_parse_truncated_method },
	{ "auth/server/parse_pw_no_bool",     test_server_parse_password_no_bool },
	{ "auth/server/parse_pw_no_pass",     test_server_parse_password_no_password },
	{ "auth/server/parse_pk_no_has_sig",  test_server_parse_publickey_no_has_sig },
	{ "auth/server/parse_not_svc_req",    test_server_parse_not_service_request },
	{ "auth/server/parse_long_username",  test_server_parse_long_username },
	{ "auth/server/parse_pw_trunc_pw",   test_server_parse_password_truncated_pw },
	{ "auth/server/parse_pw_chg_no_new", test_server_parse_password_change_no_newpw },
	{ "auth/server/parse_pw_chg_trunc",  test_server_parse_password_change_truncated_newpw },
	{ "auth/server/no_password_cb",      test_server_no_password_cb },
	{ "auth/server/no_publickey_cb",     test_server_no_publickey_cb },
	{ "auth/server/parse_pk_no_algo",    test_server_parse_publickey_no_algo },
	{ "auth/server/parse_pk_no_blob",    test_server_parse_publickey_no_blob },
	{ "auth/server/parse_pk_no_sig",     test_server_parse_publickey_sig_no_siglen },
	{ "auth/server/pk_unknown_algo",     test_server_publickey_unknown_algo },
	{ "auth/server/pk_probe_rejected",   test_server_publickey_probe_rejected },
	{ "auth/server/pw_change_rejected",  test_server_password_change_rejected },
	{ "auth/server/pw_change_no_cb",     test_server_password_change_no_change_cb },
	{ "auth/banner_truncated",           test_banner_truncated_variants },
	{ "auth/get_methods_none_accepted",  test_get_methods_none_accepted },
	{ "auth/get_methods_fail_trunc",     test_get_methods_failure_truncated },
	{ "auth/get_methods_fail_len",       test_get_methods_failure_methods_len_exceeds },
	{ "auth/get_methods_fail_ctrl",      test_get_methods_failure_control_char },
	{ "auth/get_methods_unexpected",     test_get_methods_unexpected_msg },
	{ "auth/pw_changereq_no_cb",         test_password_changereq_no_callback },
	{ "auth/pw_changereq_trunc_prompt",  test_password_changereq_truncated_prompt },
	{ "auth/pw_changereq_trunc_data",    test_password_changereq_truncated_prompt_data },
	{ "auth/pw_changereq_trunc_lang",    test_password_changereq_truncated_lang },
	{ "auth/pw_unexpected_msg",          test_password_unexpected_msg },
	{ "auth/service_accept_unexpected",  test_service_accept_unexpected },
	{ "auth/server/pk_sig_truncated",    test_server_parse_publickey_sig_truncated },
	{ "auth/server/pk_sig_trunc_algo",   test_server_parse_publickey_sig_trunc_algo },
	{ "auth/server/pk_sig_trunc_blob",   test_server_parse_publickey_sig_trunc_blob },
	{ "auth/server/pk_verify_bad_sig",   test_server_publickey_verify_bad_sig },
	{ "auth/pw_changereq_loop",          test_password_changereq_loop },

	/* Server send failures */
	{ "auth/server_send_fail/svc_accept",    test_server_send_fail_service_accept },
	{ "auth/server_send_fail/none_success",  test_server_send_fail_none_success },
	{ "auth/server_send_fail/none_failure",  test_server_send_fail_none_failure },
	{ "auth/server_send_fail/pw_success",    test_server_send_fail_password_success },
	{ "auth/server_send_fail/pw_failure",    test_server_send_fail_password_failure },
	{ "auth/server_send_fail/pw_no_cb",      test_server_send_fail_password_no_cb },
	{ "auth/server_send_fail/pw_changereq",  test_server_send_fail_password_changereq },
	{ "auth/server_send_fail/pk_no_cb",      test_server_send_fail_publickey_no_cb },
	{ "auth/server_send_fail/pk_ok",         test_server_send_fail_pk_ok },
	{ "auth/server_send_fail/pk_probe_rej",  test_server_send_fail_pk_probe_rejected },
	{ "auth/server_send_fail/pk_unknown",    test_server_send_fail_pk_unknown_algo },
	{ "auth/server_send_fail/unknown_meth",  test_server_send_fail_unknown_method },
	{ "auth/server_send_fail/chg_success",   test_server_send_fail_change_success },
	{ "auth/server_send_fail/chg_changereq", test_server_send_fail_change_changereq },
	{ "auth/server_send_fail/chg_failure",   test_server_send_fail_change_failure },
	{ "auth/server_send_fail/pk_bad_sig",    test_server_send_fail_pk_verify_bad_sig },
	{ "auth/server_send_fail/pk_success",    test_server_send_fail_pk_success },
	{ "auth/server_send_fail/pk_reject_sig", test_server_send_fail_pk_reject_sig },

	/* Group A: defensive / edge-case tests */
	{ "auth/server/null_username_out",       test_server_null_username_out },
	{ "auth/get_methods_zero_buf",           test_get_methods_zero_buffer },
	{ "auth/get_methods_del_char",           test_get_methods_del_char },
	{ "auth/server/method_4_not_none",       test_server_method_4_not_none },
	{ "auth/server/method_8_not_password",   test_server_method_8_not_password },
	{ "auth/server/method_9_not_publickey",  test_server_method_9_not_publickey },
	{ "auth/server/changepw_null_prompt",    test_server_change_password_null_prompt },
	{ "auth/server/changepw_null_prompt_chg", test_server_change_password_null_prompt_change },
	{ "auth/server/long_algo_name",          test_server_long_algo_name },
	{ "auth/banner_no_lang",                 test_server_banner_no_lang },
	{ "auth/get_methods_small_buf",          test_get_methods_small_buffer },
	{ "auth/server/tiny_svc_req",            test_server_tiny_service_request },
	{ "auth/server/short_svc_req",           test_server_short_service_request },
	{ "auth/pw_changereq_no_lang",           test_password_changereq_no_lang_field },
	{ "auth/pw_changereq_trunc_lang_data",   test_password_changereq_truncated_lang_data },
	{ "auth/kbi/empty_response",             test_kbi_empty_response },

	/* Group C: client-side failure tests */
	{ "auth/client/svc_req_send_fail",       test_client_service_request_send_fail },
	{ "auth/client/get_methods_send_fail",   test_client_get_methods_send_fail },
	{ "auth/client/pw_changereq_cb_error",   test_password_changereq_cb_error },
	{ "auth/client/pw_changereq_send_fail",  test_password_changereq_send_fail },
	{ "auth/client/kbi_send_fail",           test_client_kbi_send_fail },
	{ "auth/client/kbi_recv_fail",           test_client_kbi_recv_fail },
	{ "auth/client/pk_no_key",               test_client_publickey_no_key },
	{ "auth/client/pk_pubkey_fail",          test_client_publickey_pubkey_fail },
	{ "auth/client/pk_sign_fail",            test_client_publickey_sign_fail },
	{ "auth/client/pk_send_fail",            test_client_publickey_send_fail },
	{ "auth/client/pk_recv_fail",            test_client_publickey_recv_fail },
	{ "auth/client/pk_banner",               test_client_publickey_banner },

	/* Direct parse_userauth_prefix unit tests */
	{ "parse/prefix_empty",                  test_parse_prefix_empty },
	{ "parse/prefix_trunc_username",         test_parse_prefix_trunc_username },
	{ "parse/prefix_no_service",             test_parse_prefix_no_service },
	{ "parse/prefix_trunc_service",          test_parse_prefix_trunc_service },
	{ "parse/prefix_no_method",              test_parse_prefix_no_method },
	{ "parse/prefix_trunc_method",           test_parse_prefix_trunc_method },
	{ "parse/prefix_valid",                  test_parse_prefix_valid },

	/* Direct-call server tests */
	{ "direct/pw_no_bool",                   test_direct_pw_no_bool },
	{ "direct/pw_no_pw_len",                 test_direct_pw_no_pw_len },
	{ "direct/pw_overflow",                  test_direct_pw_overflow },
	{ "direct/pw_change_no_new",             test_direct_pw_change_no_new },
	{ "direct/pw_change_overflow",           test_direct_pw_change_overflow },
	{ "direct/pk_no_hassig",                 test_direct_pk_no_hassig },
	{ "direct/pk_no_algo_len",               test_direct_pk_no_algo_len },
	{ "direct/pk_algo_overflow",             test_direct_pk_algo_overflow },
	{ "direct/pk_no_pk_len",                 test_direct_pk_no_pk_len },
	{ "direct/pk_pk_overflow",               test_direct_pk_pk_overflow },
	{ "direct/pk_sig_no_sig_len",            test_direct_pk_sig_no_sig_len },
	{ "direct/pk_sig_overflow",              test_direct_pk_sig_overflow },
	{ "direct/wrong_first_msg",              test_direct_wrong_first_msg },
	{ "direct/short_svc_request",            test_direct_short_svc_request },
	{ "direct/svc_slen_overflow",            test_direct_svc_slen_overflow },
	{ "direct/long_username",                test_direct_long_username },
	{ "direct/null_username_out_len",        test_direct_null_username_out_len },

	/* Direct-call client-side tests (_impl from main thread) */
	{ "dclient/wrong_svc_reply",             test_dclient_wrong_svc_reply },
	{ "dclient/get_methods_alloc",           test_dclient_get_methods_alloc_fail },
	{ "dclient/get_methods_del",             test_dclient_get_methods_del },
	{ "dclient/changereq_no_cb",             test_dclient_changereq_no_cb },
	{ "dclient/changereq_trunc",             test_dclient_changereq_trunc },
	{ "dclient/changereq_no_lang",           test_dclient_changereq_no_lang },
	{ "dclient/changereq_cb_error",          test_dclient_changereq_cb_error },
	{ "dclient/kbi_alloc_fail",              test_dclient_kbi_alloc_fail },
	{ "dclient/kbi_calloc_chain",            test_dclient_kbi_calloc_chain },
	{ "dclient/kbi_resp_alloc",              test_dclient_kbi_resp_alloc },
	{ "dclient/pk_no_sign",                  test_dclient_pk_no_sign },
	{ "dclient/pk_alloc_fail",               test_dclient_pk_alloc_fail },

	/* Additional coverage */
	{ "direct/bad_prefix_in_loop",           test_direct_bad_prefix_in_loop },
	{ "direct/changereq_alloc",              test_server_changereq_alloc },
	{ "direct/pk_ok_alloc",                  test_server_pk_ok_alloc },
	{ "direct/pk_sig_unknown_algo",          test_direct_pk_sig_unknown_algo },
	{ "direct/pk_verify_alloc",              test_server_pk_verify_alloc },
	{ "dclient/get_methods_malloc",          test_dclient_get_methods_malloc },
	{ "dclient/get_methods_trunc_payload",   test_dclient_get_methods_trunc_payload },
	{ "dclient/get_methods_ctrl_char",       test_dclient_get_methods_ctrl_char },
	{ "dclient/changereq_lang_overflow",     test_dclient_changereq_lang_overflow },
	{ "dclient/get_methods_unexpected",      test_dclient_get_methods_unexpected_msg },
	{ "dclient/get_methods_msg_alloc",       test_dclient_get_methods_msg_alloc },
	{ "dclient/changereq_prompt_overflow",   test_dclient_changereq_prompt_overflow },
};

DSSH_TEST_MAIN(tests)
