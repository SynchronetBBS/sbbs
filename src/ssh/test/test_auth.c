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
	ca.algo_name = "ssh-ed25519";
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
	ca.algo_name = "ssh-ed25519";
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
	ca.algo_name = "ssh-ed25519";
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
};

DSSH_TEST_MAIN(tests)
