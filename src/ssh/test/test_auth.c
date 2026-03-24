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
	    (void *)test_banner_cb, NULL);

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
	{ "auth/server/pk_sig_truncated",    test_server_parse_publickey_sig_truncated },
	{ "auth/server/pk_sig_trunc_algo",   test_server_parse_publickey_sig_trunc_algo },
	{ "auth/server/pk_sig_trunc_blob",   test_server_parse_publickey_sig_trunc_blob },
	{ "auth/server/pk_verify_bad_sig",   test_server_publickey_verify_bad_sig },
	{ "auth/pw_changereq_loop",          test_password_changereq_loop },
};

DSSH_TEST_MAIN(tests)
