/*
 * test_selftest.c -- Tier 3a integration tests: DeuceSSH client <-> server
 * over socketpair(AF_UNIX, SOCK_STREAM).
 *
 * Each test creates a Unix socketpair, runs client and server in
 * separate threads, and exercises the full SSH stack: handshake,
 * authentication, channel I/O, signals, window-change, rekey, and
 * graceful disconnect.
 */

#include <sys/socket.h>
#include <unistd.h>
#include <string.h>
#include <threads.h>

#include "dssh_test.h"
#include "deucessh.h"
#include "deucessh-auth.h"
#include "deucessh-conn.h"
#include "deucessh-algorithms.h"
#include "ssh-trans.h"
#include "ssh-internal.h"
#include "dssh_test_internal.h"
#include "test_dhgex_provider.h"

/* ================================================================
 * Socket I/O callbacks
 *
 * cbdata is a pointer to the socket fd for this session.
 * Both client and server use the same callback functions.
 * ================================================================ */

static int
socket_tx(uint8_t *buf, size_t bufsz, dssh_session sess, void *cbdata)
{
	int fd = *(int *)cbdata;
	size_t sent = 0;
	while (sent < bufsz) {
		if (dssh_session_is_terminated(sess))
			return -1;
		ssize_t n = send(fd, buf + sent, bufsz - sent, 0);
		if (n <= 0)
			return -1;
		sent += (size_t)n;
	}
	return 0;
}

static int
socket_rx(uint8_t *buf, size_t bufsz, dssh_session sess, void *cbdata)
{
	int fd = *(int *)cbdata;
	size_t got = 0;
	while (got < bufsz) {
		if (dssh_session_is_terminated(sess))
			return -1;
		ssize_t n = recv(fd, buf + got, bufsz - got, 0);
		if (n <= 0)
			return -1;
		got += (size_t)n;
	}
	return 0;
}

static int
socket_rxline(uint8_t *buf, size_t bufsz, size_t *bytes_received,
    dssh_session sess, void *cbdata)
{
	int fd = *(int *)cbdata;
	size_t have = 0;
	while (have < bufsz) {
		if (dssh_session_is_terminated(sess))
			return -1;
		ssize_t n = recv(fd, &buf[have], 1, 0);
		if (n <= 0)
			return -1;
		have++;
		if (buf[have - 1] == '\n') {
			*bytes_received = have;
			return 0;
		}
	}
	return -1;
}

/* ================================================================
 * Algorithm registration helper
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
 * Self-test context and setup/teardown
 * ================================================================ */

struct selftest_ctx {
	int client_fd;
	int server_fd;
	dssh_session client;
	dssh_session server;
	int client_hs_result;
	int server_hs_result;
	int client_auth_result;
	int server_auth_result;
};

static void
selftest_cleanup(struct selftest_ctx *ctx)
{
	/* Shutdown sockets first to unblock demux threads blocked in recv().
	 * shutdown() causes recv() to return 0 (EOF) without closing the fd. */
	if (ctx->client_fd >= 0)
		shutdown(ctx->client_fd, SHUT_RDWR);
	if (ctx->server_fd >= 0)
		shutdown(ctx->server_fd, SHUT_RDWR);
	if (ctx->server)
		dssh_session_cleanup(ctx->server);
	if (ctx->client)
		dssh_session_cleanup(ctx->client);
	if (ctx->client_fd >= 0)
		close(ctx->client_fd);
	if (ctx->server_fd >= 0)
		close(ctx->server_fd);
	dssh_test_reset_global_config();
}

/* Server auth callback: accept any password */
static int
auth_password_cb(const uint8_t *username, size_t username_len,
    const uint8_t *password, size_t password_len,
    uint8_t **change_prompt, size_t *change_prompt_len,
    void *cbdata)
{
	return DSSH_AUTH_SUCCESS;
}

static struct dssh_auth_server_cbs selftest_auth_cbs = {
	.methods_str = "password",
	.password_cb = auth_password_cb,
};

/* ================================================================
 * Handshake threads
 * ================================================================ */

static int
client_handshake_thread(void *arg)
{
	struct selftest_ctx *ctx = arg;
	ctx->client_hs_result = dssh_transport_handshake(ctx->client);
	if (ctx->client_hs_result != 0)
		shutdown(ctx->client_fd, SHUT_RDWR);
	return 0;
}

static int
server_handshake_thread(void *arg)
{
	struct selftest_ctx *ctx = arg;
	ctx->server_hs_result = dssh_transport_handshake(ctx->server);
	if (ctx->server_hs_result != 0)
		shutdown(ctx->server_fd, SHUT_RDWR);
	return 0;
}

/* ================================================================
 * Auth threads
 * ================================================================ */

static int
client_auth_thread(void *arg)
{
	struct selftest_ctx *ctx = arg;
	ctx->client_auth_result = dssh_auth_password(ctx->client,
	    "testuser", "testpass", NULL, NULL);
	return 0;
}

static int
server_auth_thread(void *arg)
{
	struct selftest_ctx *ctx = arg;
	uint8_t username[256];
	size_t ulen = sizeof(username);
	ctx->server_auth_result = dssh_auth_server(ctx->server,
	    &selftest_auth_cbs, username, &ulen);
	return 0;
}

/* ================================================================
 * selftest_setup: socketpair, register algos, handshake, auth
 *
 * Returns 0 on success with sessions ready for session_start().
 * On failure returns -1 and cleans up.
 * ================================================================ */

static int
selftest_setup(struct selftest_ctx *ctx)
{
	memset(ctx, 0, sizeof(*ctx));
	ctx->client_fd = -1;
	ctx->server_fd = -1;
	ctx->client = NULL;
	ctx->server = NULL;

	dssh_test_reset_global_config();

	if (register_all_algorithms() < 0)
		goto fail;
	if (test_generate_host_key() < 0)
		goto fail;

	int fds[2];
	if (socketpair(AF_UNIX, SOCK_STREAM, 0, fds) < 0)
		goto fail;
	ctx->client_fd = fds[0];
	ctx->server_fd = fds[1];

	dssh_transport_set_callbacks(socket_tx, socket_rx,
	    socket_rxline, NULL);

	ctx->client = dssh_session_init(true, 0);
	if (ctx->client == NULL)
		goto fail;
	dssh_session_set_cbdata(ctx->client,
	    &ctx->client_fd, &ctx->client_fd,
	    &ctx->client_fd, &ctx->client_fd);

	ctx->server = init_server_session();
	if (ctx->server == NULL)
		goto fail;
	dssh_session_set_cbdata(ctx->server,
	    &ctx->server_fd, &ctx->server_fd,
	    &ctx->server_fd, &ctx->server_fd);

	/* Run handshake in two threads */
	thrd_t ct, st;
	if (thrd_create(&ct, client_handshake_thread, ctx) != thrd_success)
		goto fail;
	if (thrd_create(&st, server_handshake_thread, ctx) != thrd_success) {
		dssh_session_terminate(ctx->client);
		thrd_join(ct, NULL);
		goto fail;
	}
	thrd_join(ct, NULL);
	thrd_join(st, NULL);

	if (ctx->client_hs_result != 0 || ctx->server_hs_result != 0) {
		goto fail;
	}

	/* Run auth in two threads */
	if (thrd_create(&ct, client_auth_thread, ctx) != thrd_success)
		goto fail;
	if (thrd_create(&st, server_auth_thread, ctx) != thrd_success) {
		dssh_session_terminate(ctx->client);
		thrd_join(ct, NULL);
		goto fail;
	}
	thrd_join(ct, NULL);
	thrd_join(st, NULL);

	if (ctx->client_auth_result != 0 || ctx->server_auth_result != 0)
		goto fail;

	return 0;

fail:
	selftest_cleanup(ctx);
	return -1;
}

/* ================================================================
 * Session callbacks for server-side channel accept
 * ================================================================ */

/* Accept all channel requests */
static int
accept_all_requests(const char *type, size_t type_len,
    bool want_reply, const uint8_t *data, size_t data_len,
    void *cbdata)
{
	(void)type; (void)type_len; (void)want_reply;
	(void)data; (void)data_len; (void)cbdata;
	return 0;
}

static struct dssh_server_session_cbs server_session_cbs = {
	.request_cb = accept_all_requests,
};

/* Window-change callback with recording */
static uint32_t wc_cols, wc_rows, wc_wpx, wc_hpx;
static bool wc_received;

static void
window_change_record(uint32_t cols, uint32_t rows,
    uint32_t wpx, uint32_t hpx, void *cbdata)
{
	wc_cols = cols;
	wc_rows = rows;
	wc_wpx = wpx;
	wc_hpx = hpx;
	wc_received = true;
}

static struct dssh_server_session_cbs server_session_cbs_wc = {
	.request_cb = accept_all_requests,
	.window_change = window_change_record,
};

/* ================================================================
 * Server-side thread helpers
 * ================================================================ */

/* Server: accept channel, echo all input, close with exit_code 0 */
struct server_echo_arg {
	struct selftest_ctx *ctx;
	int result;
};

static int
server_echo_thread(void *arg)
{
	struct server_echo_arg *a = arg;
	struct selftest_ctx *ctx = a->ctx;
	a->result = -1;

	struct dssh_incoming_open *inc;
	if (dssh_session_accept(ctx->server, &inc, 5000) < 0)
		return 0;

	const char *req_type, *req_data;
	dssh_channel ch = dssh_session_accept_channel(ctx->server, inc,
	    &server_session_cbs, &req_type, &req_data);
	if (ch == NULL)
		return 0;

	uint8_t buf[4096];
	for (;;) {
		int ev = dssh_session_poll(ctx->server, ch,
		    DSSH_POLL_READ, 5000);
		if (ev <= 0)
			break;
		int64_t n = dssh_session_read(ctx->server, ch,
		    buf, sizeof(buf));
		if (n <= 0)
			break;
		int64_t w = dssh_session_write(ctx->server, ch, buf, (size_t)n);
		if (w < 0)
			break;
	}

	dssh_session_close(ctx->server, ch, 0);
	a->result = 0;
	return 0;
}

/* Server: accept channel, close immediately with specific exit code */
struct server_exit_arg {
	struct selftest_ctx *ctx;
	uint32_t exit_code;
	int result;
};

static int
server_exit_thread(void *arg)
{
	struct server_exit_arg *a = arg;
	struct selftest_ctx *ctx = a->ctx;
	a->result = -1;

	struct dssh_incoming_open *inc;
	if (dssh_session_accept(ctx->server, &inc, 5000) < 0)
		return 0;

	const char *req_type, *req_data;
	dssh_channel ch = dssh_session_accept_channel(ctx->server, inc,
	    &server_session_cbs, &req_type, &req_data);
	if (ch == NULL)
		return 0;

	dssh_session_close(ctx->server, ch, a->exit_code);
	a->result = 0;
	return 0;
}

/* Server: accept channel, write data, send signal, write more, close */
struct server_signal_arg {
	struct selftest_ctx *ctx;
	const char *signal_name;
	int result;
};

static int
server_signal_thread(void *arg)
{
	struct server_signal_arg *a = arg;
	struct selftest_ctx *ctx = a->ctx;
	a->result = -1;

	struct dssh_incoming_open *inc;
	if (dssh_session_accept(ctx->server, &inc, 5000) < 0)
		return 0;

	const char *req_type, *req_data;
	dssh_channel ch = dssh_session_accept_channel(ctx->server, inc,
	    &server_session_cbs, &req_type, &req_data);
	if (ch == NULL)
		return 0;

	/* Write data before the signal */
	const uint8_t msg[] = "before-signal";
	dssh_session_write(ctx->server, ch, msg, sizeof(msg) - 1);

	/* Send signal */
	dssh_session_send_signal(ctx->server, ch, a->signal_name);

	/* Write data after signal */
	const uint8_t msg2[] = "after-signal";
	dssh_session_write(ctx->server, ch, msg2, sizeof(msg2) - 1);

	dssh_session_close(ctx->server, ch, 0);
	a->result = 0;
	return 0;
}

/* Server: accept channel with window-change callback, read until EOF */
struct server_wc_arg {
	struct selftest_ctx *ctx;
	int result;
};

static int
server_wc_thread(void *arg)
{
	struct server_wc_arg *a = arg;
	struct selftest_ctx *ctx = a->ctx;
	a->result = -1;

	struct dssh_incoming_open *inc;
	if (dssh_session_accept(ctx->server, &inc, 5000) < 0)
		return 0;

	const char *req_type, *req_data;
	dssh_channel ch = dssh_session_accept_channel(ctx->server, inc,
	    &server_session_cbs_wc, &req_type, &req_data);
	if (ch == NULL)
		return 0;

	uint8_t buf[256];
	for (;;) {
		int ev = dssh_session_poll(ctx->server, ch,
		    DSSH_POLL_READ, 5000);
		if (ev <= 0)
			break;
		int64_t n = dssh_session_read(ctx->server, ch,
		    buf, sizeof(buf));
		if (n <= 0)
			break;
	}

	dssh_session_close(ctx->server, ch, 0);
	a->result = 0;
	return 0;
}

/* Server: accept two channels sequentially, echo one message each */
struct server_multi_arg {
	struct selftest_ctx *ctx;
	int channels_handled;
	int result;
};

static int
server_multi_thread(void *arg)
{
	struct server_multi_arg *a = arg;
	struct selftest_ctx *ctx = a->ctx;
	a->result = -1;
	a->channels_handled = 0;

	for (int i = 0; i < 2; i++) {
		struct dssh_incoming_open *inc;
		if (dssh_session_accept(ctx->server, &inc, 5000) < 0)
			return 0;

		const char *req_type, *req_data;
		dssh_channel ch = dssh_session_accept_channel(ctx->server,
		    inc, &server_session_cbs, &req_type, &req_data);
		if (ch == NULL)
			return 0;

		uint8_t buf[256];
		int ev = dssh_session_poll(ctx->server, ch,
		    DSSH_POLL_READ, 5000);
		if (ev > 0) {
			int64_t n = dssh_session_read(ctx->server, ch,
			    buf, sizeof(buf));
			if (n > 0)
				dssh_session_write(ctx->server, ch, buf, (size_t)n);
		}

		dssh_session_close(ctx->server, ch, 0);
		a->channels_handled++;
	}

	a->result = 0;
	return 0;
}

/* Server: accept channel, wait for disconnect */
struct server_disconnect_arg {
	struct selftest_ctx *ctx;
	int result;
};

static int
server_disconnect_thread(void *arg)
{
	struct server_disconnect_arg *a = arg;
	struct selftest_ctx *ctx = a->ctx;
	a->result = -1;

	struct dssh_incoming_open *inc;
	if (dssh_session_accept(ctx->server, &inc, 5000) < 0) {
		/* Accept may fail when client disconnects -- OK */
		a->result = 0;
		return 0;
	}

	const char *req_type, *req_data;
	dssh_channel ch = dssh_session_accept_channel(ctx->server, inc,
	    &server_session_cbs, &req_type, &req_data);
	if (ch == NULL) {
		a->result = 0;
		return 0;
	}

	uint8_t buf[256];
	dssh_session_poll(ctx->server, ch, DSSH_POLL_READ, 5000);
	dssh_session_read(ctx->server, ch, buf, sizeof(buf));

	dssh_session_close(ctx->server, ch, 0);
	a->result = 0;
	return 0;
}

/* Server: accept channel, read all input, write reply, close */
struct server_halfclose_arg {
	struct selftest_ctx *ctx;
	int result;
};

static int
server_halfclose_thread(void *arg)
{
	struct server_halfclose_arg *a = arg;
	struct selftest_ctx *ctx = a->ctx;
	a->result = -1;

	struct dssh_incoming_open *inc;
	if (dssh_session_accept(ctx->server, &inc, 5000) < 0)
		return 0;

	const char *req_type, *req_data;
	dssh_channel ch = dssh_session_accept_channel(ctx->server, inc,
	    &server_session_cbs, &req_type, &req_data);
	if (ch == NULL)
		return 0;

	/* Read until EOF from client */
	uint8_t buf[256];
	for (;;) {
		int ev = dssh_session_poll(ctx->server, ch,
		    DSSH_POLL_READ, 5000);
		if (ev <= 0)
			break;
		int64_t n = dssh_session_read(ctx->server, ch,
		    buf, sizeof(buf));
		if (n <= 0)
			break;
	}

	/* After client EOF, server can still write */
	const uint8_t reply[] = "server-reply-after-eof";
	dssh_session_write(ctx->server, ch, reply, sizeof(reply) - 1);

	dssh_session_close(ctx->server, ch, 0);
	a->result = 0;
	return 0;
}

/* ================================================================
 * Test: Handshake with curve25519
 * ================================================================ */

static int
test_self_handshake_curve25519(void)
{
	struct selftest_ctx ctx;
	if (selftest_setup(&ctx) < 0)
		return TEST_FAIL;

	/* Verify negotiated algorithms */
	ASSERT_NOT_NULL(dssh_transport_get_kex_name(ctx.client));
	ASSERT_NOT_NULL(dssh_transport_get_hostkey_name(ctx.client));
	ASSERT_NOT_NULL(dssh_transport_get_enc_name(ctx.client));
	ASSERT_NOT_NULL(dssh_transport_get_mac_name(ctx.client));

	if (test_using_dhgex()) {
		ASSERT_STR_EQ(dssh_transport_get_kex_name(ctx.client),
		    "diffie-hellman-group-exchange-sha256");
		ASSERT_STR_EQ(dssh_transport_get_kex_name(ctx.server),
		    "diffie-hellman-group-exchange-sha256");
	}
	else if (test_using_sntrup()) {
		ASSERT_STR_EQ(dssh_transport_get_kex_name(ctx.client),
		    "sntrup761x25519-sha512");
		ASSERT_STR_EQ(dssh_transport_get_kex_name(ctx.server),
		    "sntrup761x25519-sha512");
	}
	else if (test_using_mlkem()) {
		ASSERT_STR_EQ(dssh_transport_get_kex_name(ctx.client),
		    "mlkem768x25519-sha256");
		ASSERT_STR_EQ(dssh_transport_get_kex_name(ctx.server),
		    "mlkem768x25519-sha256");
	}
	else {
		ASSERT_STR_EQ(dssh_transport_get_kex_name(ctx.client),
		    "curve25519-sha256");
		ASSERT_STR_EQ(dssh_transport_get_kex_name(ctx.server),
		    "curve25519-sha256");
	}
	ASSERT_STR_EQ(dssh_transport_get_hostkey_name(ctx.client),
	    test_key_algo_name());
	ASSERT_STR_EQ(dssh_transport_get_enc_name(ctx.client),
	    dssh_transport_get_enc_name(ctx.server));

	selftest_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Test: Password authentication succeeds
 * ================================================================ */

static int
test_self_auth_password(void)
{
	struct selftest_ctx ctx;
	if (selftest_setup(&ctx) < 0)
		return TEST_FAIL;

	/* Auth already passed in setup -- verify sessions are live */
	ASSERT_FALSE(dssh_session_is_terminated(ctx.client));
	ASSERT_FALSE(dssh_session_is_terminated(ctx.server));

	selftest_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Test: exec echo -- client exec, server echoes input
 * ================================================================ */

static int
test_self_exec_echo(void)
{
	struct selftest_ctx ctx;
	if (selftest_setup(&ctx) < 0)
		return TEST_FAIL;

	ASSERT_OK(dssh_session_start(ctx.client));
	ASSERT_OK(dssh_session_start(ctx.server));

	struct server_echo_arg sarg = { .ctx = &ctx };
	thrd_t st;
	ASSERT_TRUE(thrd_create(&st, server_echo_thread, &sarg) == thrd_success);

	dssh_channel ch = dssh_session_open_exec(ctx.client, "echo");
	ASSERT_NOT_NULL(ch);

	const uint8_t data[] = "Hello, DeuceSSH!";
	int64_t w = dssh_session_write(ctx.client, ch, data, sizeof(data) - 1);
	ASSERT_TRUE(w > 0);

	/* Read echo */
	uint8_t buf[256];
	size_t recvd = 0;
	for (int i = 0; i < 50; i++) {
		int ev = dssh_session_poll(ctx.client, ch,
		    DSSH_POLL_READ, 200);
		if (ev <= 0)
			break;
		int64_t n = dssh_session_read(ctx.client, ch,
		    buf + recvd, sizeof(buf) - recvd);
		if (n <= 0)
			break;
		recvd += (size_t)n;
		if (recvd >= sizeof(data) - 1)
			break;
	}
	ASSERT_EQ_U(recvd, sizeof(data) - 1);
	ASSERT_MEM_EQ(buf, data, sizeof(data) - 1);

	dssh_session_close(ctx.client, ch, 0);
	thrd_join(st, NULL);
	ASSERT_EQ(sarg.result, 0);

	selftest_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Test: exec with exit code -- server closes with exit_code=42
 * ================================================================ */

static int
test_self_exec_exit_code(void)
{
	struct selftest_ctx ctx;
	if (selftest_setup(&ctx) < 0)
		return TEST_FAIL;

	ASSERT_OK(dssh_session_start(ctx.client));
	ASSERT_OK(dssh_session_start(ctx.server));

	struct server_exit_arg sarg = { .ctx = &ctx, .exit_code = 42 };
	thrd_t st;
	ASSERT_TRUE(thrd_create(&st, server_exit_thread, &sarg) == thrd_success);

	dssh_channel ch = dssh_session_open_exec(ctx.client, "exit42");
	ASSERT_NOT_NULL(ch);

	/* Drain channel until closed */
	uint8_t buf[256];
	for (int i = 0; i < 50; i++) {
		int ev = dssh_session_poll(ctx.client, ch,
		    DSSH_POLL_READ, 200);
		if (ev <= 0)
			break;
		int64_t n = dssh_session_read(ctx.client, ch,
		    buf, sizeof(buf));
		if (n <= 0)
			break;
	}

	thrd_join(st, NULL);
	ASSERT_EQ(sarg.result, 0);

	dssh_session_close(ctx.client, ch, 0);
	selftest_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Test: shell echo -- client opens shell, server echoes
 * ================================================================ */

static int
test_self_shell_echo(void)
{
	struct selftest_ctx ctx;
	if (selftest_setup(&ctx) < 0)
		return TEST_FAIL;

	ASSERT_OK(dssh_session_start(ctx.client));
	ASSERT_OK(dssh_session_start(ctx.server));

	struct server_echo_arg sarg = { .ctx = &ctx };
	thrd_t st;
	ASSERT_TRUE(thrd_create(&st, server_echo_thread, &sarg) == thrd_success);

	struct dssh_pty_req pty = {
		.term = "xterm",
		.cols = 80, .rows = 24,
	};
	dssh_channel ch = dssh_session_open_shell(ctx.client, &pty);
	ASSERT_NOT_NULL(ch);

	const uint8_t data[] = "shell-test";
	dssh_session_write(ctx.client, ch, data, sizeof(data) - 1);

	uint8_t buf[256];
	size_t recvd = 0;
	for (int i = 0; i < 50; i++) {
		int ev = dssh_session_poll(ctx.client, ch,
		    DSSH_POLL_READ, 200);
		if (ev <= 0)
			break;
		int64_t n = dssh_session_read(ctx.client, ch,
		    buf + recvd, sizeof(buf) - recvd);
		if (n <= 0)
			break;
		recvd += (size_t)n;
		if (recvd >= sizeof(data) - 1)
			break;
	}
	ASSERT_EQ_U(recvd, sizeof(data) - 1);
	ASSERT_MEM_EQ(buf, data, sizeof(data) - 1);

	dssh_session_close(ctx.client, ch, 0);
	thrd_join(st, NULL);
	ASSERT_EQ(sarg.result, 0);

	selftest_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Test: shell large data -- send 64KB through shell channel
 * ================================================================ */

static int
test_self_shell_large_data(void)
{
	struct selftest_ctx ctx;
	if (selftest_setup(&ctx) < 0)
		return TEST_FAIL;

	ASSERT_OK(dssh_session_start(ctx.client));
	ASSERT_OK(dssh_session_start(ctx.server));

	struct server_echo_arg sarg = { .ctx = &ctx };
	thrd_t st;
	ASSERT_TRUE(thrd_create(&st, server_echo_thread, &sarg) == thrd_success);

	struct dssh_pty_req pty = {
		.term = "xterm",
		.cols = 80, .rows = 24,
	};
	dssh_channel ch = dssh_session_open_shell(ctx.client, &pty);
	ASSERT_NOT_NULL(ch);

	/* Build 64KB test pattern */
	size_t total = 65536;
	uint8_t *send_buf = malloc(total);
	uint8_t *recv_buf = malloc(total);
	ASSERT_NOT_NULL(send_buf);
	ASSERT_NOT_NULL(recv_buf);
	for (size_t i = 0; i < total; i++)
		send_buf[i] = (uint8_t)(i & 0xFF);

	/* Send all data in chunks */
	size_t sent = 0;
	while (sent < total) {
		int ev = dssh_session_poll(ctx.client, ch,
		    DSSH_POLL_WRITE, 5000);
		if (ev <= 0)
			break;
		size_t chunk = total - sent;
		if (chunk > 4096)
			chunk = 4096;
		int64_t w = dssh_session_write(ctx.client, ch,
		    send_buf + sent, chunk);
		if (w <= 0)
			break;
		sent += (size_t)w;
	}
	ASSERT_EQ_U(sent, total);

	/* Receive all echoed data */
	size_t recvd = 0;
	while (recvd < total) {
		int ev = dssh_session_poll(ctx.client, ch,
		    DSSH_POLL_READ, 5000);
		if (ev <= 0)
			break;
		int64_t n = dssh_session_read(ctx.client, ch,
		    recv_buf + recvd, total - recvd);
		if (n <= 0)
			break;
		recvd += (size_t)n;
	}
	ASSERT_EQ_U(recvd, total);
	ASSERT_MEM_EQ(send_buf, recv_buf, total);

	free(send_buf);
	free(recv_buf);

	dssh_session_close(ctx.client, ch, 0);
	thrd_join(st, NULL);
	ASSERT_EQ(sarg.result, 0);

	selftest_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Test: signal -- server sends signal, client receives after drain
 * ================================================================ */

static int
test_self_signal(void)
{
	struct selftest_ctx ctx;
	if (selftest_setup(&ctx) < 0)
		return TEST_FAIL;

	ASSERT_OK(dssh_session_start(ctx.client));
	ASSERT_OK(dssh_session_start(ctx.server));

	struct server_signal_arg sarg = {
		.ctx = &ctx,
		.signal_name = "TERM",
	};
	thrd_t st;
	ASSERT_TRUE(thrd_create(&st, server_signal_thread, &sarg) == thrd_success);

	dssh_channel ch = dssh_session_open_exec(ctx.client, "signal-test");
	ASSERT_NOT_NULL(ch);

	/* Read stdout and poll for signal */
	uint8_t buf[256];
	size_t total_read = 0;
	bool got_signal = false;
	for (int iter = 0; iter < 200; iter++) {
		int ev = dssh_session_poll(ctx.client, ch,
		    DSSH_POLL_READ | DSSH_POLL_SIGNAL, 100);
		if (ev <= 0)
			break;
		if (ev & DSSH_POLL_READ) {
			int64_t n = dssh_session_read(ctx.client, ch,
			    buf, sizeof(buf));
			if (n > 0)
				total_read += (size_t)n;
			else if (!(ev & DSSH_POLL_SIGNAL))
				break;
		}
		if (ev & DSSH_POLL_SIGNAL) {
			const char *signame;
			if (dssh_session_read_signal(ctx.client, ch,
			    &signame) == 0) {
				ASSERT_STR_EQ(signame, "TERM");
				got_signal = true;
			}
		}
	}

	ASSERT_TRUE(total_read > 0);
	ASSERT_TRUE(got_signal);

	dssh_session_close(ctx.client, ch, 0);
	thrd_join(st, NULL);
	ASSERT_EQ(sarg.result, 0);

	selftest_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Test: window change -- client sends window-change request
 * ================================================================ */

static int
test_self_window_change(void)
{
	struct selftest_ctx ctx;
	if (selftest_setup(&ctx) < 0)
		return TEST_FAIL;

	ASSERT_OK(dssh_session_start(ctx.client));
	ASSERT_OK(dssh_session_start(ctx.server));

	wc_received = false;

	struct server_wc_arg sarg = { .ctx = &ctx };
	thrd_t st;
	ASSERT_TRUE(thrd_create(&st, server_wc_thread, &sarg) == thrd_success);

	struct dssh_pty_req pty = {
		.term = "xterm",
		.cols = 80, .rows = 24,
	};
	dssh_channel ch = dssh_session_open_shell(ctx.client, &pty);
	ASSERT_NOT_NULL(ch);

	/* Send window-change */
	ASSERT_OK(dssh_session_send_window_change(ctx.client, ch,
	    132, 50, 1056, 800));

	/* Give server time to process */
	struct timespec ts = { .tv_nsec = 10000000 }; /* 10ms */
	thrd_sleep(&ts, NULL);

	dssh_session_close(ctx.client, ch, 0);
	thrd_join(st, NULL);

	ASSERT_TRUE(wc_received);
	ASSERT_EQ_U(wc_cols, 132);
	ASSERT_EQ_U(wc_rows, 50);
	ASSERT_EQ_U(wc_wpx, 1056);
	ASSERT_EQ_U(wc_hpx, 800);

	selftest_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Test: multiple channels -- open 2 channels sequentially
 * ================================================================ */

static int
test_self_multiple_channels(void)
{
	struct selftest_ctx ctx;
	if (selftest_setup(&ctx) < 0)
		return TEST_FAIL;

	ASSERT_OK(dssh_session_start(ctx.client));
	ASSERT_OK(dssh_session_start(ctx.server));

	struct server_multi_arg sarg = { .ctx = &ctx };
	thrd_t st;
	ASSERT_TRUE(thrd_create(&st, server_multi_thread, &sarg) == thrd_success);

	/* First channel */
	dssh_channel ch1 = dssh_session_open_exec(ctx.client, "cmd1");
	ASSERT_NOT_NULL(ch1);

	const uint8_t data1[] = "channel-one";
	dssh_session_write(ctx.client, ch1, data1, sizeof(data1) - 1);

	uint8_t buf[256];
	size_t recvd = 0;
	for (int i = 0; i < 50; i++) {
		int ev = dssh_session_poll(ctx.client, ch1,
		    DSSH_POLL_READ, 200);
		if (ev <= 0)
			break;
		int64_t n = dssh_session_read(ctx.client, ch1,
		    buf + recvd, sizeof(buf) - recvd);
		if (n <= 0)
			break;
		recvd += (size_t)n;
		if (recvd >= sizeof(data1) - 1)
			break;
	}
	ASSERT_EQ_U(recvd, sizeof(data1) - 1);
	ASSERT_MEM_EQ(buf, data1, sizeof(data1) - 1);

	dssh_session_close(ctx.client, ch1, 0);

	/* Second channel */
	dssh_channel ch2 = dssh_session_open_exec(ctx.client, "cmd2");
	ASSERT_NOT_NULL(ch2);

	const uint8_t data2[] = "channel-two";
	dssh_session_write(ctx.client, ch2, data2, sizeof(data2) - 1);

	recvd = 0;
	for (int i = 0; i < 50; i++) {
		int ev = dssh_session_poll(ctx.client, ch2,
		    DSSH_POLL_READ, 200);
		if (ev <= 0)
			break;
		int64_t n = dssh_session_read(ctx.client, ch2,
		    buf + recvd, sizeof(buf) - recvd);
		if (n <= 0)
			break;
		recvd += (size_t)n;
		if (recvd >= sizeof(data2) - 1)
			break;
	}
	ASSERT_EQ_U(recvd, sizeof(data2) - 1);
	ASSERT_MEM_EQ(buf, data2, sizeof(data2) - 1);

	dssh_session_close(ctx.client, ch2, 0);

	thrd_join(st, NULL);
	ASSERT_EQ(sarg.result, 0);
	ASSERT_EQ(sarg.channels_handled, 2);

	selftest_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Test: graceful disconnect -- client disconnects, server detects
 * ================================================================ */

static int
test_self_graceful_disconnect(void)
{
	struct selftest_ctx ctx;
	if (selftest_setup(&ctx) < 0)
		return TEST_FAIL;

	ASSERT_OK(dssh_session_start(ctx.client));
	ASSERT_OK(dssh_session_start(ctx.server));

	struct server_disconnect_arg sarg = { .ctx = &ctx };
	thrd_t st;
	ASSERT_TRUE(thrd_create(&st, server_disconnect_thread, &sarg) == thrd_success);

	dssh_channel ch = dssh_session_open_exec(ctx.client, "wait");
	ASSERT_NOT_NULL(ch);

	/* Disconnect gracefully */
	dssh_transport_disconnect(ctx.client,
	    SSH_DISCONNECT_BY_APPLICATION, "test done");

	thrd_join(st, NULL);

	ASSERT_TRUE(dssh_session_is_terminated(ctx.client));

	selftest_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Test: EOF half-close -- client closes, server processes
 * ================================================================ */

static int
test_self_eof_half_close(void)
{
	struct selftest_ctx ctx;
	if (selftest_setup(&ctx) < 0)
		return TEST_FAIL;

	ASSERT_OK(dssh_session_start(ctx.client));
	ASSERT_OK(dssh_session_start(ctx.server));

	struct server_halfclose_arg sarg = { .ctx = &ctx };
	thrd_t st;
	ASSERT_TRUE(thrd_create(&st, server_halfclose_thread, &sarg) == thrd_success);

	dssh_channel ch = dssh_session_open_exec(ctx.client, "halfclose");
	ASSERT_NOT_NULL(ch);

	/* Write some data then close */
	const uint8_t data[] = "client-data";
	dssh_session_write(ctx.client, ch, data, sizeof(data) - 1);
	dssh_session_close(ctx.client, ch, 0);

	thrd_join(st, NULL);
	ASSERT_EQ(sarg.result, 0);

	selftest_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Test: rekey during data -- send enough to trigger auto-rekey
 * ================================================================ */

static int
test_self_rekey_during_data(void)
{
	struct selftest_ctx ctx;
	if (selftest_setup(&ctx) < 0)
		return TEST_FAIL;

	ASSERT_OK(dssh_session_start(ctx.client));
	ASSERT_OK(dssh_session_start(ctx.server));

	struct server_echo_arg sarg = { .ctx = &ctx };
	thrd_t st;
	ASSERT_TRUE(thrd_create(&st, server_echo_thread, &sarg) == thrd_success);

	struct dssh_pty_req pty = {
		.term = "xterm",
		.cols = 80, .rows = 24,
	};
	dssh_channel ch = dssh_session_open_shell(ctx.client, &pty);
	ASSERT_NOT_NULL(ch);

	/* Force rx_bytes_since_rekey near threshold on the CLIENT.
	 * Auto-rekey triggers in recv_packet's default path when
	 * the client demux receives the echoed data. */
	ctx.client->trans.rx_bytes_since_rekey = DSSH_REKEY_BYTES - 100;

	uint8_t data[256];
	memset(data, 'R', sizeof(data));
	int64_t w = dssh_session_write(ctx.client, ch, data, sizeof(data));
	ASSERT_TRUE(w > 0);

	/* Wait for echo -- the client demux must receive a response,
	 * triggering the auto-rekey check. */
	uint8_t buf[256];
	size_t recvd = 0;
	for (int i = 0; i < 100; i++) {
		int ev = dssh_session_poll(ctx.client, ch,
		    DSSH_POLL_READ, 100);
		if (ev <= 0)
			continue;
		int64_t n = dssh_session_read(ctx.client, ch,
		    buf + recvd, sizeof(buf) - recvd);
		if (n <= 0)
			break;
		recvd += (size_t)n;
		if (recvd >= sizeof(data))
			break;
	}
	ASSERT_EQ_U(recvd, sizeof(data));
	ASSERT_MEM_EQ(buf, data, sizeof(data));

	/* Verify the connection survived rekey */
	ASSERT_FALSE(dssh_session_is_terminated(ctx.client));

	dssh_session_close(ctx.client, ch, 0);
	thrd_join(st, NULL);
	ASSERT_EQ(sarg.result, 0);

	selftest_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Test: manual rekey -- trigger via tx_since_rekey threshold
 * ================================================================ */

static int
test_self_rekey_manual(void)
{
	struct selftest_ctx ctx;
	if (selftest_setup(&ctx) < 0)
		return TEST_FAIL;

	ASSERT_OK(dssh_session_start(ctx.client));
	ASSERT_OK(dssh_session_start(ctx.server));

	struct server_echo_arg sarg = { .ctx = &ctx };
	thrd_t st;
	ASSERT_TRUE(thrd_create(&st, server_echo_thread, &sarg) == thrd_success);

	dssh_channel ch = dssh_session_open_exec(ctx.client, "rekey");
	ASSERT_NOT_NULL(ch);

	/* Force rx_bytes_since_rekey just below threshold on the client.
	 * A single encrypted CHANNEL_DATA packet adds ~60-80 bytes of
	 * wire overhead (packet header + padding + MAC), so set headroom
	 * small enough that one packet pushes it over. */
	/* Only force threshold on the client.  The client's demux
	 * will trigger rekey when it receives the echoed data. */
	ctx.client->trans.rx_bytes_since_rekey = DSSH_REKEY_BYTES - 50;

	const uint8_t data[] = "after-rekey";
	int64_t w = dssh_session_write(ctx.client, ch, data, sizeof(data) - 1);
	ASSERT_TRUE(w > 0);

	/* Wait for echo */
	uint8_t buf[256];
	size_t recvd = 0;
	for (int i = 0; i < 100; i++) {
		int ev = dssh_session_poll(ctx.client, ch,
		    DSSH_POLL_READ, 100);
		if (ev <= 0)
			continue;
		int64_t n = dssh_session_read(ctx.client, ch,
		    buf + recvd, sizeof(buf) - recvd);
		if (n <= 0)
			break;
		recvd += (size_t)n;
		if (recvd >= sizeof(data) - 1)
			break;
	}
	ASSERT_EQ_U(recvd, sizeof(data) - 1);

	/* Verify the echo itself worked -- the connection is alive
	 * and data survived the threshold being near-exceeded.
	 * Full auto-rekey with live demux threads is verified by
	 * test_self_rekey_during_data and test_self_rekey_preserves_channels. */
	ASSERT_FALSE(dssh_session_is_terminated(ctx.client));

	dssh_session_close(ctx.client, ch, 0);
	thrd_join(st, NULL);

	selftest_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Test: rekey preserves channels -- data survives rekey
 * ================================================================ */

static int
test_self_rekey_preserves_channels(void)
{
	struct selftest_ctx ctx;
	if (selftest_setup(&ctx) < 0)
		return TEST_FAIL;

	ASSERT_OK(dssh_session_start(ctx.client));
	ASSERT_OK(dssh_session_start(ctx.server));

	struct server_echo_arg sarg = { .ctx = &ctx };
	thrd_t st;
	ASSERT_TRUE(thrd_create(&st, server_echo_thread, &sarg) == thrd_success);

	struct dssh_pty_req pty = {
		.term = "xterm",
		.cols = 80, .rows = 24,
	};
	dssh_channel ch = dssh_session_open_shell(ctx.client, &pty);
	ASSERT_NOT_NULL(ch);

	/* Send data before rekey */
	const uint8_t before[] = "before-rekey-data";
	int64_t w = dssh_session_write(ctx.client, ch,
	    before, sizeof(before) - 1);
	ASSERT_TRUE(w > 0);

	/* Read echo */
	uint8_t buf[256];
	size_t recvd = 0;
	for (int i = 0; i < 50; i++) {
		int ev = dssh_session_poll(ctx.client, ch,
		    DSSH_POLL_READ, 200);
		if (ev <= 0)
			break;
		int64_t n = dssh_session_read(ctx.client, ch,
		    buf + recvd, sizeof(buf) - recvd);
		if (n <= 0)
			break;
		recvd += (size_t)n;
		if (recvd >= sizeof(before) - 1)
			break;
	}
	ASSERT_EQ_U(recvd, sizeof(before) - 1);
	ASSERT_MEM_EQ(buf, before, sizeof(before) - 1);

	/* Save session ID */
	size_t sid_sz = ctx.client->trans.session_id_sz;
	uint8_t sid_before[64];
	ASSERT_TRUE(sid_sz <= sizeof(sid_before));
	memcpy(sid_before, ctx.client->trans.session_id, sid_sz);

	/* Force rekey */
	ctx.client->trans.rx_bytes_since_rekey = DSSH_REKEY_BYTES - 100;

	/* Send data to trigger rekey */
	const uint8_t after[] = "after-rekey-data!";
	w = dssh_session_write(ctx.client, ch,
	    after, sizeof(after) - 1);
	ASSERT_TRUE(w > 0);

	/* Read echo of post-rekey data.  The rekey runs asynchronously
	 * in the demux thread, so allow extra time. */
	recvd = 0;
	for (int i = 0; i < 100; i++) {
		int ev = dssh_session_poll(ctx.client, ch,
		    DSSH_POLL_READ, 100);
		if (ev <= 0)
			continue;
		int64_t n = dssh_session_read(ctx.client, ch,
		    buf + recvd, sizeof(buf) - recvd);
		if (n <= 0)
			break;
		recvd += (size_t)n;
		if (recvd >= sizeof(after) - 1)
			break;
	}
	ASSERT_EQ_U(recvd, sizeof(after) - 1);
	ASSERT_MEM_EQ(buf, after, sizeof(after) - 1);

	/* Session ID must be unchanged after rekey */
	ASSERT_EQ_U(ctx.client->trans.session_id_sz, sid_sz);
	ASSERT_MEM_EQ(ctx.client->trans.session_id, sid_before, sid_sz);

	dssh_session_close(ctx.client, ch, 0);
	thrd_join(st, NULL);
	ASSERT_EQ(sarg.result, 0);

	selftest_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Test: rekey in-flight data -- data sent by peer before seeing
 * our KEXINIT must not be dropped (close item 69)
 * ================================================================ */

/* Server: accept channel, send burst of data, close */
struct server_burst_arg {
	struct selftest_ctx *ctx;
	int result;
};

static int
server_burst_thread(void *arg)
{
	struct server_burst_arg *a = arg;
	struct selftest_ctx *ctx = a->ctx;
	a->result = -1;

	struct dssh_incoming_open *inc;
	if (dssh_session_accept(ctx->server, &inc, 5000) < 0)
		return 0;

	const char *req_type, *req_data;
	dssh_channel ch = dssh_session_accept_channel(ctx->server, inc,
	    &server_session_cbs, &req_type, &req_data);
	if (ch == NULL)
		return 0;

	/* Send 10 chunks of 50 bytes each as fast as possible.
	 * Some will be in the socket buffer when the client
	 * starts its self-initiated rekey. */
	uint8_t chunk[50];
	memset(chunk, 'D', sizeof(chunk));
	for (int i = 0; i < 10; i++) {
		int64_t w = dssh_session_write(ctx->server, ch,
		    chunk, sizeof(chunk));
		if (w <= 0)
			break;
	}

	dssh_session_close(ctx->server, ch, 0);
	a->result = 0;
	return 0;
}

static int
test_self_rekey_inflight_data(void)
{
	struct selftest_ctx ctx;
	if (selftest_setup(&ctx) < 0)
		return TEST_FAIL;

	ASSERT_OK(dssh_session_start(ctx.client));
	ASSERT_OK(dssh_session_start(ctx.server));

	struct server_burst_arg sarg = { .ctx = &ctx };
	thrd_t st;
	ASSERT_TRUE(thrd_create(&st, server_burst_thread, &sarg) == thrd_success);

	dssh_channel ch = dssh_session_open_exec(ctx.client, "burst");
	ASSERT_NOT_NULL(ch);

	/* Force rx_bytes near threshold so the FIRST received packet
	 * triggers rekey_pending.  The client's demux dispatches that
	 * packet, then on the next recv_packet call starts rekey.
	 * Remaining packets in the socket buffer arrive during the
	 * kexinit wait loop. */
	ctx.client->trans.rx_bytes_since_rekey = DSSH_REKEY_BYTES - 50;

	/* Collect all data -- expect 500 bytes total */
	uint8_t buf[1024];
	size_t recvd = 0;
	const size_t expected = 10 * 50;
	for (int i = 0; i < 200; i++) {
		int ev = dssh_session_poll(ctx.client, ch,
		    DSSH_POLL_READ, 100);
		if (ev <= 0)
			continue;
		int64_t n = dssh_session_read(ctx.client, ch,
		    buf + recvd, sizeof(buf) - recvd);
		if (n <= 0)
			break;
		recvd += (size_t)n;
		if (recvd >= expected)
			break;
	}
	ASSERT_EQ_U(recvd, expected);

	/* Verify data integrity */
	for (size_t i = 0; i < recvd; i++)
		ASSERT_EQ(buf[i], 'D');

	ASSERT_FALSE(dssh_session_is_terminated(ctx.client));

	dssh_session_close(ctx.client, ch, 0);
	thrd_join(st, NULL);
	ASSERT_EQ(sarg.result, 0);

	selftest_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Test: unexpected connection drop -- socket dies mid-session
 * ================================================================ */

static int
test_self_connection_drop(void)
{
	struct selftest_ctx ctx;
	if (selftest_setup(&ctx) < 0)
		return TEST_FAIL;

	ASSERT_OK(dssh_session_start(ctx.client));
	ASSERT_OK(dssh_session_start(ctx.server));

	struct server_echo_arg sarg = { .ctx = &ctx };
	thrd_t st;
	ASSERT_TRUE(thrd_create(&st, server_echo_thread, &sarg) == thrd_success);

	dssh_channel ch = dssh_session_open_exec(ctx.client, "drop");
	ASSERT_NOT_NULL(ch);

	/* Write some data to confirm the channel works */
	const uint8_t data[] = "before drop";
	int64_t w = dssh_session_write(ctx.client, ch, data, sizeof(data) - 1);
	ASSERT_TRUE(w > 0);

	/* Wait for echo to confirm data arrived */
	uint8_t buf[64];
	size_t recvd = 0;
	for (int i = 0; i < 50; i++) {
		int ev = dssh_session_poll(ctx.client, ch,
		    DSSH_POLL_READ, 100);
		if (ev <= 0)
			continue;
		int64_t n = dssh_session_read(ctx.client, ch,
		    buf + recvd, sizeof(buf) - recvd);
		if (n <= 0)
			break;
		recvd += (size_t)n;
		if (recvd >= sizeof(data) - 1)
			break;
	}
	ASSERT_EQ_U(recvd, sizeof(data) - 1);

	/* Yank the socket out from under both sessions.
	 * This simulates a VPN drop, cable pull, or peer crash.
	 * The application detects the dead socket in its I/O
	 * callbacks (recv returns 0 / send returns EPIPE). */
	shutdown(ctx.client_fd, SHUT_RDWR);
	shutdown(ctx.server_fd, SHUT_RDWR);

	/* Give the demux threads time to see the error */
	struct timespec ts = { .tv_sec = 0, .tv_nsec = 20000000 };
	thrd_sleep(&ts, NULL);

	/* Both sessions should be terminated */
	ASSERT_TRUE(dssh_session_is_terminated(ctx.client));
	ASSERT_TRUE(dssh_session_is_terminated(ctx.server));

	/* Poll and write must not hang */
	int ev = dssh_session_poll(ctx.client, ch,
	    DSSH_POLL_READ, 100);
	(void)ev;

	w = dssh_session_write(ctx.client, ch,
	    (const uint8_t *)"x", 1);
	ASSERT_TRUE(w <= 0);

	/* Server echo thread should have exited */
	thrd_join(st, NULL);

	/* Cleanup must succeed without hanging */
	selftest_cleanup(&ctx);
	return TEST_PASS;
}

/*
 * Verify all public API functions in ssh.c handle NULL session gracefully.
 * Covers 7 NULL-guard branches + dssh_cleanse(NULL).
 */
static int
test_null_session_api(void)
{
	/* dssh_session_terminate(NULL) returns false */
	ASSERT_FALSE(dssh_session_terminate(NULL));

	/* dssh_session_is_terminated(NULL) returns true */
	ASSERT_TRUE(dssh_session_is_terminated(NULL));

	/* dssh_session_cleanup(NULL) is a no-op (must not crash) */
	dssh_session_cleanup(NULL);

	/* Setter functions with NULL session are no-ops (must not crash) */
	dssh_session_set_cbdata(NULL, NULL, NULL, NULL, NULL);
	dssh_session_set_debug_cb(NULL, NULL, NULL);
	dssh_session_set_unimplemented_cb(NULL, NULL, NULL);
	dssh_session_set_banner_cb(NULL, NULL, NULL);
	dssh_session_set_global_request_cb(NULL, NULL, NULL);

	/* dssh_cleanse(NULL, ...) is a no-op (must not crash) */
	dssh_cleanse(NULL, 10);

	return TEST_PASS;
}

/* ================================================================
 * Timeout tests (items 65 + 66)
 * ================================================================ */

/*
 * Verify dssh_session_set_timeout(NULL, ...) does not crash,
 * and that the default is 75000ms.
 */
static int
test_set_timeout(void)
{
	/* NULL session: no crash */
	dssh_session_set_timeout(NULL, 1000);

	/* Verify default */
	dssh_session sess = dssh_session_init(true, 0);
	ASSERT_NOT_NULL(sess);
	ASSERT_EQ(sess->timeout_ms, 75000);

	/* Set and verify */
	dssh_session_set_timeout(sess, 200);
	ASSERT_EQ(sess->timeout_ms, 200);

	/* Disable timeout */
	dssh_session_set_timeout(sess, 0);
	ASSERT_EQ(sess->timeout_ms, 0);

	dssh_session_cleanup(sess);
	return TEST_PASS;
}

/*
 * open_session_channel times out when the server never responds
 * with CHANNEL_OPEN_CONFIRMATION.  The server's demux thread runs
 * but dssh_session_accept() is never called, so the queued
 * CHANNEL_OPEN is never answered.
 */
static int
test_open_channel_timeout(void)
{
	struct selftest_ctx ctx;
	if (selftest_setup(&ctx) < 0)
		return TEST_SKIP;

	/* Short timeout so the test completes quickly */
	dssh_session_set_timeout(ctx.client, 200);

	ASSERT_OK(dssh_session_start(ctx.client));
	ASSERT_OK(dssh_session_start(ctx.server));

	struct timespec t0;
	timespec_get(&t0, TIME_UTC);

	/* Server never calls accept, so no CONFIRMATION is sent */
	dssh_channel ch = dssh_session_open_exec(ctx.client, "cmd");
	ASSERT_NULL(ch);

	struct timespec t1;
	timespec_get(&t1, TIME_UTC);
	long elapsed_ms = (t1.tv_sec - t0.tv_sec) * 1000L
	    + (t1.tv_nsec - t0.tv_nsec) / 1000000L;

	/* Should complete in ~200ms, allow generous slack but under 2s */
	ASSERT_TRUE(elapsed_ms < 2000);

	selftest_cleanup(&ctx);
	return TEST_PASS;
}

/*
 * setup_recv times out when the client opens a channel but never
 * sends any CHANNEL_REQUEST (pty-req, shell, exec, etc.).
 * The server's accept_channel -> setup_recv path should time out.
 */
struct setup_timeout_server_arg {
	struct selftest_ctx *ctx;
	dssh_channel ch;
	int result;
};

static int
setup_timeout_server_thread(void *arg)
{
	struct setup_timeout_server_arg *a = arg;
	struct selftest_ctx *ctx = a->ctx;
	a->ch = NULL;

	struct dssh_incoming_open *inc;
	a->result = dssh_session_accept(ctx->server, &inc, 5000);
	if (a->result < 0)
		return 0;

	const char *req_type, *req_data;
	a->ch = dssh_session_accept_channel(ctx->server, inc,
	    &server_session_cbs, &req_type, &req_data);
	a->result = (a->ch == NULL) ? 0 : -1;
	return 0;
}

static int
test_setup_recv_timeout(void)
{
	struct selftest_ctx ctx;
	if (selftest_setup(&ctx) < 0)
		return TEST_SKIP;

	/* Short timeout on the server (where setup_recv runs) */
	dssh_session_set_timeout(ctx.server, 200);

	ASSERT_OK(dssh_session_start(ctx.client));
	ASSERT_OK(dssh_session_start(ctx.server));

	/* Start server accept in a thread */
	struct setup_timeout_server_arg sarg = { .ctx = &ctx };
	thrd_t st;
	ASSERT_THRD_CREATE(&st, setup_timeout_server_thread, &sarg);

	/*
	 * Send a raw CHANNEL_OPEN from the client but never follow up
	 * with any CHANNEL_REQUEST.  The server accepts the channel open
	 * (sends CONFIRMATION) then blocks in setup_recv waiting for
	 * pty-req/shell/exec/subsystem -- which never arrive.
	 */
	dssh_session_set_timeout(ctx.client, 5000);

	struct timespec t0;
	timespec_get(&t0, TIME_UTC);

	/* Build and send a raw CHANNEL_OPEN packet */
	uint8_t msg[64];
	size_t pos = 0;
	static const char chan_session[] = "session";
	msg[pos++] = SSH_MSG_CHANNEL_OPEN;
	DSSH_PUT_U32(DSSH_STRLEN(chan_session), msg, &pos);
	memcpy(&msg[pos], chan_session, DSSH_STRLEN(chan_session));
	pos += DSSH_STRLEN(chan_session);
	DSSH_PUT_U32(0, msg, &pos);    /* sender channel */
	DSSH_PUT_U32(32768, msg, &pos); /* initial window */
	DSSH_PUT_U32(32768, msg, &pos); /* max packet */

	int res = dssh_transport_send_packet(ctx.client, msg, pos, NULL);
	ASSERT_OK(res);

	/* Wait for server thread to complete (it should time out) */
	thrd_join(st, NULL);

	struct timespec t1;
	timespec_get(&t1, TIME_UTC);
	long elapsed_ms = (t1.tv_sec - t0.tv_sec) * 1000L
	    + (t1.tv_nsec - t0.tv_nsec) / 1000000L;

	/* Server's setup_recv should have timed out in ~200ms */
	ASSERT_TRUE(elapsed_ms < 2000);
	ASSERT_EQ(sarg.result, 0); /* ch == NULL, result set to 0 */
	ASSERT_NULL(sarg.ch);

	selftest_cleanup(&ctx);
	return TEST_PASS;
}

/*
 * send_packet blocks during rekey; with a session timeout set,
 * the rekey wait should time out and terminate the session.
 * We simulate this by setting rekey_in_progress = true without
 * actually performing a rekey, then sending an application message.
 */
static int
test_rekey_send_timeout(void)
{
	struct selftest_ctx ctx;
	if (selftest_setup(&ctx) < 0)
		return TEST_SKIP;

	/* Short timeout */
	dssh_session_set_timeout(ctx.client, 200);

	/* Pretend a rekey is in progress (no actual KEX running) */
	ctx.client->trans.rekey_in_progress = true;

	struct timespec t0;
	timespec_get(&t0, TIME_UTC);

	/* Try to send an application-layer message (type >= 50).
	 * This should block on the rekey wait, time out, terminate,
	 * and return DSSH_ERROR_TERMINATED. */
	uint8_t msg[8];
	msg[0] = SSH_MSG_CHANNEL_DATA; /* type 94, above threshold */
	msg[1] = 0;
	msg[2] = 0;
	msg[3] = 0;
	msg[4] = 0;

	int res = dssh_transport_send_packet(ctx.client, msg, 5, NULL);
	ASSERT_EQ(res, DSSH_ERROR_TERMINATED);

	struct timespec t1;
	timespec_get(&t1, TIME_UTC);
	long elapsed_ms = (t1.tv_sec - t0.tv_sec) * 1000L
	    + (t1.tv_nsec - t0.tv_nsec) / 1000000L;

	ASSERT_TRUE(elapsed_ms < 2000);
	ASSERT_TRUE(dssh_session_is_terminated(ctx.client));

	selftest_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Test table and main
 * ================================================================ */

static struct dssh_test_entry tests[] = {
	{ "test_self_handshake_curve25519",     test_self_handshake_curve25519 },
	{ "test_self_auth_password",            test_self_auth_password },
	{ "test_self_exec_echo",               test_self_exec_echo },
	{ "test_self_exec_exit_code",          test_self_exec_exit_code },
	{ "test_self_shell_echo",              test_self_shell_echo },
	{ "test_self_shell_large_data",        test_self_shell_large_data },
	{ "test_self_signal",                  test_self_signal },
	{ "test_self_window_change",           test_self_window_change },
	{ "test_self_multiple_channels",       test_self_multiple_channels },
	{ "test_self_graceful_disconnect",     test_self_graceful_disconnect },
	{ "test_self_eof_half_close",          test_self_eof_half_close },
	{ "test_self_rekey_during_data",       test_self_rekey_during_data },
	{ "test_self_rekey_manual",            test_self_rekey_manual },
	{ "test_self_rekey_preserves_channels", test_self_rekey_preserves_channels },
	{ "test_self_rekey_inflight_data",     test_self_rekey_inflight_data },
	{ "test_self_connection_drop",         test_self_connection_drop },
	{ "test_null_session_api",             test_null_session_api },
	{ "test_set_timeout",                  test_set_timeout },
	{ "test_open_channel_timeout",         test_open_channel_timeout },
	{ "test_setup_recv_timeout",           test_setup_recv_timeout },
	{ "test_rekey_send_timeout",           test_rekey_send_timeout },
};

DSSH_TEST_MAIN(tests)
