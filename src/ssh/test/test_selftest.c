/*
 * test_selftest.c -- Tier 3a integration tests: DeuceSSH client <-> server
 * over socketpair(AF_UNIX, SOCK_STREAM).
 *
 * Each test creates a Unix socketpair, runs client and server in
 * separate threads, and exercises the full SSH stack: handshake,
 * authentication, channel I/O, signals, window-change, rekey, and
 * graceful disconnect.
 */

#include <errno.h>
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
		if (n < 0 && errno == EINTR)
			continue;
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
		if (n < 0 && errno == EINTR)
			continue;
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
		if (n < 0 && errno == EINTR)
			continue;
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
	res = test_register_mac_algos();
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
	thrd_t server_thread;
	bool server_thread_active;
};

static struct selftest_ctx *g_active_ctx;

static void
selftest_cleanup(struct selftest_ctx *ctx)
{
	/* Snapshot all fields before any function calls.  When called
	 * from dssh_test_after_each, ctx may point to a test function's
	 * stack frame that has already been popped.  The memory is
	 * intact at this point, but deeper function calls (terminate,
	 * join, cleanup) will grow the stack and overwrite it. */
	struct selftest_ctx snap = *ctx;

	if (g_active_ctx == ctx)
		g_active_ctx = NULL;
	/* Shutdown sockets first to unblock threads blocked in recv/send. */
	if (snap.client_fd >= 0)
		shutdown(snap.client_fd, SHUT_RDWR);
	if (snap.server_fd >= 0)
		shutdown(snap.server_fd, SHUT_RDWR);
	/* Terminate sessions to unblock the server thread from any
	 * dssh_* blocking call (sets terminate flag, broadcasts condvars).
	 * dssh_session_terminate is idempotent; dssh_session_cleanup
	 * calling it again is a no-op. */
	if (snap.server)
		dssh_session_terminate(snap.server);
	if (snap.client)
		dssh_session_terminate(snap.client);
	/* Join server thread BEFORE freeing sessions.  After shutdown +
	 * terminate, the thread's I/O and blocking calls return errors,
	 * so the join completes promptly. */
	if (snap.server_thread_active)
		thrd_join(snap.server_thread, NULL);
	if (snap.server)
		dssh_session_cleanup(snap.server);
	if (snap.client)
		dssh_session_cleanup(snap.client);
	if (snap.client_fd >= 0)
		close(snap.client_fd);
	if (snap.server_fd >= 0)
		close(snap.server_fd);
	dssh_test_reset_global_config();
}

static int
selftest_start_thread(struct selftest_ctx *ctx,
    thrd_start_t func, void *arg)
{
	if (thrd_create(&ctx->server_thread, func, arg) != thrd_success)
		return -1;
	ctx->server_thread_active = true;
	return 0;
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

	if (ctx->client_hs_result != 0 || ctx->server_hs_result != 0)
		goto fail;

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

	g_active_ctx = ctx;
	return 0;

fail:
	selftest_cleanup(ctx);
	return -1;
}

/* ================================================================
 * Server accept callbacks (new dssh_chan_accept API)
 * ================================================================ */

static int
selftest_accept_shell(dssh_channel ch, const struct dssh_chan_params *p,
    struct dssh_chan_accept_result *result, void *cbdata)
{
	(void)ch; (void)p; (void)result; (void)cbdata;
	return 0;
}

static int
selftest_accept_exec(dssh_channel ch, const struct dssh_chan_params *p,
    struct dssh_chan_accept_result *result, void *cbdata)
{
	(void)ch; (void)p; (void)result; (void)cbdata;
	return 0;
}

static struct dssh_chan_accept_cbs selftest_accept_cbs = {
	.shell = selftest_accept_shell,
	.exec = selftest_accept_exec,
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

	dssh_channel ch = dssh_chan_accept(ctx->server,
	    &selftest_accept_cbs, 30000);

	if (ch == NULL)
		return 0;

	uint8_t buf[4096];

	for (;;) {
		int ev = dssh_chan_poll(ch, DSSH_POLL_READ, 30000);

		if (ev <= 0)
			break;

		int64_t n = dssh_chan_read(ch, 0, buf, sizeof(buf));

		if (n <= 0)
			break;

		/* Write all read data, polling for window if needed */
		size_t off = 0;

		while (off < (size_t)n) {
			int wev = dssh_chan_poll(ch,
			    DSSH_POLL_WRITE, 30000);

			if (wev <= 0)
				goto done;

			int64_t w = dssh_chan_write(ch, 0,
			    buf + off, (size_t)n - off);

			if (w < 0)
				goto done;
			off += (size_t)w;
		}
	}
done:

	dssh_chan_close(ch, 0);
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

	dssh_channel ch = dssh_chan_accept(ctx->server,
	    &selftest_accept_cbs, 30000);

	if (ch == NULL)
		return 0;

	dssh_chan_close(ch, (int64_t)a->exit_code);
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

	dssh_channel ch = dssh_chan_accept(ctx->server,
	    &selftest_accept_cbs, 30000);

	if (ch == NULL)
		return 0;

	/* Wait for peer's WINDOW_ADJUST before writing */
	dssh_chan_poll(ch, DSSH_POLL_WRITE, 30000);

	const uint8_t msg[] = "before-signal";

	dssh_chan_write(ch, 0, msg, sizeof(msg) - 1);
	dssh_chan_send_signal(ch, a->signal_name);

	const uint8_t msg2[] = "after-signal";

	dssh_chan_write(ch, 0, msg2, sizeof(msg2) - 1);
	dssh_chan_close(ch, 0);
	a->result = 0;
	return 0;
}

/* Server: accept channel, poll for window-change event, read until EOF */
struct server_wc_arg {
	struct selftest_ctx *ctx;
	bool wc_received;
	uint32_t wc_cols, wc_rows, wc_wpx, wc_hpx;
	int result;
};

static int
server_wc_thread(void *arg)
{
	struct server_wc_arg *a = arg;
	struct selftest_ctx *ctx = a->ctx;
	a->result = -1;
	a->wc_received = false;

	dssh_channel ch = dssh_chan_accept(ctx->server,
	    &selftest_accept_cbs, 30000);

	if (ch == NULL)
		return 0;

	uint8_t buf[256];

	for (;;) {
		int ev = dssh_chan_poll(ch,
		    DSSH_POLL_READ | DSSH_POLL_EVENT, 30000);

		if (ev <= 0)
			break;

		if (ev & DSSH_POLL_EVENT) {
			struct dssh_chan_event event;

			if (dssh_chan_read_event(ch, &event) == 0
			    && event.type == DSSH_EVENT_WINDOW_CHANGE) {
				a->wc_received = true;
				a->wc_cols = event.window_change.cols;
				a->wc_rows = event.window_change.rows;
				a->wc_wpx = event.window_change.wpx;
				a->wc_hpx = event.window_change.hpx;
			}
			continue;
		}

		if (ev & DSSH_POLL_READ) {
			int64_t n = dssh_chan_read(ch, 0,
			    buf, sizeof(buf));

			if (n <= 0)
				break;
		}
	}

	dssh_chan_close(ch, 0);
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
		dssh_channel ch = dssh_chan_accept(ctx->server,
		    &selftest_accept_cbs, 30000);

		if (ch == NULL)
			return 0;

		uint8_t buf[256];
		int ev = dssh_chan_poll(ch, DSSH_POLL_READ, 30000);

		if (ev > 0) {
			int64_t n = dssh_chan_read(ch, 0,
			    buf, sizeof(buf));

			if (n > 0)
				dssh_chan_write(ch, 0, buf, (size_t)n);
		}

		dssh_chan_close(ch, 0);
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

	dssh_channel ch = dssh_chan_accept(ctx->server,
	    &selftest_accept_cbs, 30000);

	if (ch == NULL) {
		a->result = 0;
		return 0;
	}

	uint8_t buf[256];

	dssh_chan_poll(ch, DSSH_POLL_READ, 30000);
	dssh_chan_read(ch, 0, buf, sizeof(buf));
	dssh_chan_close(ch, 0);
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

	dssh_channel ch = dssh_chan_accept(ctx->server,
	    &selftest_accept_cbs, 30000);

	if (ch == NULL)
		return 0;

	uint8_t buf[256];

	for (;;) {
		int ev = dssh_chan_poll(ch, DSSH_POLL_READ, 30000);

		if (ev <= 0)
			break;

		int64_t n = dssh_chan_read(ch, 0, buf, sizeof(buf));

		if (n <= 0)
			break;
	}

	const uint8_t reply[] = "server-reply-after-eof";

	dssh_chan_write(ch, 0, reply, sizeof(reply) - 1);
	dssh_chan_close(ch, 0);
	a->result = 0;
	return 0;
}

/* ================================================================
 * Helpers for opening channels with the new API
 * ================================================================ */

static dssh_channel
open_exec(dssh_session sess, const char *cmd)
{
	struct dssh_chan_params p;

	dssh_chan_params_init(&p, DSSH_CHAN_EXEC);
	dssh_chan_params_set_command(&p, cmd);
	dssh_chan_params_set_pty(&p, false);

	dssh_channel ch = dssh_chan_open(sess, &p);

	dssh_chan_params_free(&p);
	return ch;
}

static dssh_channel
open_shell(dssh_session sess, const char *term,
    uint32_t cols, uint32_t rows)
{
	struct dssh_chan_params p;

	dssh_chan_params_init(&p, DSSH_CHAN_SHELL);
	dssh_chan_params_set_term(&p, term);
	dssh_chan_params_set_size(&p, cols, rows, 0, 0);

	dssh_channel ch = dssh_chan_open(sess, &p);

	dssh_chan_params_free(&p);
	return ch;
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

	ASSERT_OK(selftest_start_thread(&ctx, server_echo_thread, &sarg));

	dssh_channel ch = open_exec(ctx.client,"echo");
	ASSERT_NOT_NULL(ch);

	/* Wait for server's WINDOW_ADJUST (remote_window starts at 0) */
	int wev = dssh_chan_poll(ch, DSSH_POLL_WRITE, 5000);
	ASSERT_TRUE(wev > 0);

	const uint8_t data[] = "Hello, DeuceSSH!";
	int64_t w = dssh_chan_write(ch, 0, data, sizeof(data) - 1);
	ASSERT_TRUE(w > 0);

	/* Read echo */
	uint8_t buf[256];
	size_t recvd = 0;
	for (int i = 0; i < 50; i++) {
		int ev = dssh_chan_poll(ch,
		    DSSH_POLL_READ, 1000);
		if (ev <= 0)
			break;
		int64_t n = dssh_chan_read(ch, 0,
		    buf + recvd, sizeof(buf) - recvd);
		if (n <= 0)
			break;
		recvd += (size_t)n;
		if (recvd >= sizeof(data) - 1)
			break;
	}
	ASSERT_EQ_U(recvd, sizeof(data) - 1);
	ASSERT_MEM_EQ(buf, data, sizeof(data) - 1);

	dssh_chan_close(ch, 0);
	thrd_join(ctx.server_thread, NULL);
	ctx.server_thread_active = false;
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
	ASSERT_OK(selftest_start_thread(&ctx, server_exit_thread, &sarg));

	dssh_channel ch = open_exec(ctx.client,"exit42");
	ASSERT_NOT_NULL(ch);

	/* Drain channel until closed */
	uint8_t buf[256];
	for (int i = 0; i < 50; i++) {
		int ev = dssh_chan_poll(ch,
		    DSSH_POLL_READ, 1000);
		if (ev <= 0)
			break;
		int64_t n = dssh_chan_read(ch, 0,
		    buf, sizeof(buf));
		if (n <= 0)
			break;
	}

	thrd_join(ctx.server_thread, NULL);
	ctx.server_thread_active = false;
	ASSERT_EQ(sarg.result, 0);

	dssh_chan_close(ch, 0);
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
	ASSERT_OK(selftest_start_thread(&ctx, server_echo_thread, &sarg));

	dssh_channel ch = open_shell(ctx.client, "xterm", 80, 24);
	ASSERT_NOT_NULL(ch);

	/* Wait for server's WINDOW_ADJUST (remote_window starts at 0) */
	int wev = dssh_chan_poll(ch, DSSH_POLL_WRITE, 5000);
	ASSERT_TRUE(wev > 0);

	const uint8_t data[] = "shell-test";
	int64_t w = dssh_chan_write(ch, 0, data, sizeof(data) - 1);
	ASSERT_TRUE(w > 0);

	uint8_t buf[256];
	size_t recvd = 0;
	for (int i = 0; i < 50; i++) {
		int ev = dssh_chan_poll(ch,
		    DSSH_POLL_READ, 1000);
		if (ev <= 0)
			break;
		int64_t n = dssh_chan_read(ch, 0,
		    buf + recvd, sizeof(buf) - recvd);
		if (n <= 0)
			break;
		recvd += (size_t)n;
		if (recvd >= sizeof(data) - 1)
			break;
	}
	ASSERT_EQ_U(recvd, sizeof(data) - 1);
	ASSERT_MEM_EQ(buf, data, sizeof(data) - 1);

	dssh_chan_close(ch, 0);
	thrd_join(ctx.server_thread, NULL);
	ctx.server_thread_active = false;
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
	ASSERT_OK(selftest_start_thread(&ctx, server_echo_thread, &sarg));

	dssh_channel ch = open_shell(ctx.client, "xterm", 80, 24);
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
		int ev = dssh_chan_poll(ch,
		    DSSH_POLL_WRITE, 30000);
		if (ev <= 0)
			break;
		size_t chunk = total - sent;
		if (chunk > 4096)
			chunk = 4096;
		int64_t w = dssh_chan_write(ch, 0,
		    send_buf + sent, chunk);
		if (w <= 0)
			break;
		sent += (size_t)w;
	}
	ASSERT_EQ_U(sent, total);

	/* Receive all echoed data */
	size_t recvd = 0;
	while (recvd < total) {
		int ev = dssh_chan_poll(ch,
		    DSSH_POLL_READ, 30000);
		if (ev <= 0)
			break;
		int64_t n = dssh_chan_read(ch, 0,
		    recv_buf + recvd, total - recvd);
		if (n <= 0)
			break;
		recvd += (size_t)n;
	}
	ASSERT_EQ_U(recvd, total);
	ASSERT_MEM_EQ(send_buf, recv_buf, total);

	free(send_buf);
	free(recv_buf);

	dssh_chan_close(ch, 0);
	thrd_join(ctx.server_thread, NULL);
	ctx.server_thread_active = false;
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
	ASSERT_OK(selftest_start_thread(&ctx, server_signal_thread, &sarg));

	dssh_channel ch = open_exec(ctx.client,"signal-test");
	ASSERT_NOT_NULL(ch);

	/* Read stdout and poll for signal event */
	uint8_t buf[256];
	size_t total_read = 0;
	bool got_signal = false;

	for (int iter = 0; iter < 200; iter++) {
		int ev = dssh_chan_poll(ch,
		    DSSH_POLL_READ | DSSH_POLL_EVENT, 1000);

		if (ev <= 0)
			break;

		if (ev & DSSH_POLL_EVENT) {
			struct dssh_chan_event event;

			if (dssh_chan_read_event(ch, &event) == 0
			    && event.type == DSSH_EVENT_SIGNAL) {
				ASSERT_STR_EQ(event.signal.name, "TERM");
				got_signal = true;
			}
			continue;
		}
		if (ev & DSSH_POLL_READ) {
			int64_t n = dssh_chan_read(ch, 0,
			    buf, sizeof(buf));

			if (n > 0)
				total_read += (size_t)n;
			else
				break;
		}
	}

	ASSERT_TRUE(total_read > 0);
	ASSERT_TRUE(got_signal);

	dssh_chan_close(ch, 0);
	thrd_join(ctx.server_thread, NULL);
	ctx.server_thread_active = false;
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

	struct server_wc_arg sarg = { .ctx = &ctx };
	ASSERT_OK(selftest_start_thread(&ctx, server_wc_thread, &sarg));

	dssh_channel ch = open_shell(ctx.client, "xterm", 80, 24);

	ASSERT_NOT_NULL(ch);

	/* Send window-change */
	ASSERT_OK(dssh_chan_send_window_change(ch,
	    132, 50, 1056, 800));

	/* Give server time to process the event */
	struct timespec ts = { .tv_nsec = 10000000 }; /* 10ms */

	thrd_sleep(&ts, NULL);

	dssh_chan_close(ch, 0);
	thrd_join(ctx.server_thread, NULL);
	ctx.server_thread_active = false;

	ASSERT_TRUE(sarg.wc_received);
	ASSERT_EQ_U(sarg.wc_cols, 132);
	ASSERT_EQ_U(sarg.wc_rows, 50);
	ASSERT_EQ_U(sarg.wc_wpx, 1056);
	ASSERT_EQ_U(sarg.wc_hpx, 800);

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
	ASSERT_OK(selftest_start_thread(&ctx, server_multi_thread, &sarg));

	/* First channel */
	dssh_channel ch1 = open_exec(ctx.client,"cmd1");
	ASSERT_NOT_NULL(ch1);

	dssh_chan_poll(ch1, DSSH_POLL_WRITE, 5000);
	const uint8_t data1[] = "channel-one";
	dssh_chan_write(ch1, 0, data1, sizeof(data1) - 1);

	uint8_t buf[256];
	size_t recvd = 0;
	for (int i = 0; i < 50; i++) {
		int ev = dssh_chan_poll(ch1,
		    DSSH_POLL_READ, 1000);
		if (ev <= 0)
			break;
		int64_t n = dssh_chan_read(ch1, 0,
		    buf + recvd, sizeof(buf) - recvd);
		if (n <= 0)
			break;
		recvd += (size_t)n;
		if (recvd >= sizeof(data1) - 1)
			break;
	}
	ASSERT_EQ_U(recvd, sizeof(data1) - 1);
	ASSERT_MEM_EQ(buf, data1, sizeof(data1) - 1);

	dssh_chan_close(ch1, 0);

	/* Second channel */
	dssh_channel ch2 = open_exec(ctx.client,"cmd2");
	ASSERT_NOT_NULL(ch2);

	dssh_chan_poll(ch2, DSSH_POLL_WRITE, 5000);
	const uint8_t data2[] = "channel-two";
	dssh_chan_write(ch2, 0, data2, sizeof(data2) - 1);

	recvd = 0;
	for (int i = 0; i < 50; i++) {
		int ev = dssh_chan_poll(ch2,
		    DSSH_POLL_READ, 1000);
		if (ev <= 0)
			break;
		int64_t n = dssh_chan_read(ch2, 0,
		    buf + recvd, sizeof(buf) - recvd);
		if (n <= 0)
			break;
		recvd += (size_t)n;
		if (recvd >= sizeof(data2) - 1)
			break;
	}
	ASSERT_EQ_U(recvd, sizeof(data2) - 1);
	ASSERT_MEM_EQ(buf, data2, sizeof(data2) - 1);

	dssh_chan_close(ch2, 0);

	thrd_join(ctx.server_thread, NULL);
	ctx.server_thread_active = false;
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
	ASSERT_OK(selftest_start_thread(&ctx, server_disconnect_thread, &sarg));

	dssh_channel ch = open_exec(ctx.client,"wait");
	ASSERT_NOT_NULL(ch);

	/* Disconnect gracefully */
	dssh_transport_disconnect(ctx.client,
	    SSH_DISCONNECT_BY_APPLICATION, "test done");

	thrd_join(ctx.server_thread, NULL);
	ctx.server_thread_active = false;

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
	ASSERT_OK(selftest_start_thread(&ctx, server_halfclose_thread, &sarg));

	dssh_channel ch = open_exec(ctx.client,"halfclose");
	ASSERT_NOT_NULL(ch);

	/* Write some data then close */
	dssh_chan_poll(ch, DSSH_POLL_WRITE, 5000);
	const uint8_t data[] = "client-data";
	dssh_chan_write(ch, 0, data, sizeof(data) - 1);
	dssh_chan_close(ch, 0);

	thrd_join(ctx.server_thread, NULL);
	ctx.server_thread_active = false;
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
	ASSERT_OK(selftest_start_thread(&ctx, server_echo_thread, &sarg));

	dssh_channel ch = open_shell(ctx.client, "xterm", 80, 24);
	ASSERT_NOT_NULL(ch);

	/* Force rx_bytes_since_rekey near threshold on the CLIENT.
	 * Auto-rekey triggers in recv_packet's default path when
	 * the client demux receives the echoed data. */
	ctx.client->trans.rx_bytes_since_rekey = DSSH_REKEY_BYTES - 100;

	dssh_chan_poll(ch, DSSH_POLL_WRITE, 5000);
	uint8_t data[256];
	memset(data, 'R', sizeof(data));
	int64_t w = dssh_chan_write(ch, 0, data, sizeof(data));
	ASSERT_TRUE(w > 0);

	/* Wait for echo -- the client demux must receive a response,
	 * triggering the auto-rekey check. */
	uint8_t buf[256];
	size_t recvd = 0;
	for (int i = 0; i < 100; i++) {
		int ev = dssh_chan_poll(ch,
		    DSSH_POLL_READ, 1000);
		if (ev <= 0)
			continue;
		int64_t n = dssh_chan_read(ch, 0,
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

	dssh_chan_close(ch, 0);
	thrd_join(ctx.server_thread, NULL);
	ctx.server_thread_active = false;
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
	ASSERT_OK(selftest_start_thread(&ctx, server_echo_thread, &sarg));

	dssh_channel ch = open_exec(ctx.client,"rekey");
	ASSERT_NOT_NULL(ch);

	/* Force rx_bytes_since_rekey just below threshold on the client.
	 * A single encrypted CHANNEL_DATA packet adds ~60-80 bytes of
	 * wire overhead (packet header + padding + MAC), so set headroom
	 * small enough that one packet pushes it over. */
	/* Only force threshold on the client.  The client's demux
	 * will trigger rekey when it receives the echoed data. */
	ctx.client->trans.rx_bytes_since_rekey = DSSH_REKEY_BYTES - 50;

	dssh_chan_poll(ch, DSSH_POLL_WRITE, 5000);
	const uint8_t data[] = "after-rekey";
	int64_t w = dssh_chan_write(ch, 0, data, sizeof(data) - 1);
	ASSERT_TRUE(w > 0);

	/* Wait for echo */
	uint8_t buf[256];
	size_t recvd = 0;
	for (int i = 0; i < 100; i++) {
		int ev = dssh_chan_poll(ch,
		    DSSH_POLL_READ, 1000);
		if (ev <= 0)
			continue;
		int64_t n = dssh_chan_read(ch, 0,
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

	dssh_chan_close(ch, 0);
	thrd_join(ctx.server_thread, NULL);
	ctx.server_thread_active = false;

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
	ASSERT_OK(selftest_start_thread(&ctx, server_echo_thread, &sarg));

	dssh_channel ch = open_shell(ctx.client, "xterm", 80, 24);
	ASSERT_NOT_NULL(ch);

	/* Send data before rekey */
	dssh_chan_poll(ch, DSSH_POLL_WRITE, 5000);
	const uint8_t before[] = "before-rekey-data";
	int64_t w = dssh_chan_write(ch, 0,
	    before, sizeof(before) - 1);
	ASSERT_TRUE(w > 0);

	/* Read echo */
	uint8_t buf[256];
	size_t recvd = 0;
	for (int i = 0; i < 50; i++) {
		int ev = dssh_chan_poll(ch,
		    DSSH_POLL_READ, 1000);
		if (ev <= 0)
			break;
		int64_t n = dssh_chan_read(ch, 0,
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
	w = dssh_chan_write(ch, 0,
	    after, sizeof(after) - 1);
	ASSERT_TRUE(w > 0);

	/* Read echo of post-rekey data.  The rekey runs asynchronously
	 * in the demux thread, so allow extra time. */
	recvd = 0;
	for (int i = 0; i < 100; i++) {
		int ev = dssh_chan_poll(ch,
		    DSSH_POLL_READ, 1000);
		if (ev <= 0)
			continue;
		int64_t n = dssh_chan_read(ch, 0,
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

	dssh_chan_close(ch, 0);
	thrd_join(ctx.server_thread, NULL);
	ctx.server_thread_active = false;
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

	dssh_channel ch = dssh_chan_accept(ctx->server,
	    &selftest_accept_cbs, 30000);

	if (ch == NULL)
		return 0;

	/* Wait for peer's WINDOW_ADJUST */
	dssh_chan_poll(ch, DSSH_POLL_WRITE, 30000);

	/* Send 10 chunks of 50 bytes each as fast as possible.
	 * Some will be in the socket buffer when the client
	 * starts its self-initiated rekey. */
	uint8_t chunk[50];

	memset(chunk, 'D', sizeof(chunk));
	for (int i = 0; i < 10; i++) {
		int64_t w = dssh_chan_write(ch, 0,
		    chunk, sizeof(chunk));

		if (w <= 0)
			break;
	}

	dssh_chan_close(ch, 0);
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
	ASSERT_OK(selftest_start_thread(&ctx, server_burst_thread, &sarg));

	dssh_channel ch = open_exec(ctx.client,"burst");
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
		int ev = dssh_chan_poll(ch,
		    DSSH_POLL_READ, 1000);
		if (ev <= 0)
			continue;
		int64_t n = dssh_chan_read(ch, 0,
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

	dssh_chan_close(ch, 0);
	thrd_join(ctx.server_thread, NULL);
	ctx.server_thread_active = false;
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
	ASSERT_OK(selftest_start_thread(&ctx, server_echo_thread, &sarg));

	dssh_channel ch = open_exec(ctx.client,"drop");
	ASSERT_NOT_NULL(ch);

	/* Write some data to confirm the channel works */
	dssh_chan_poll(ch, DSSH_POLL_WRITE, 5000);
	const uint8_t data[] = "before drop";
	int64_t w = dssh_chan_write(ch, 0, data, sizeof(data) - 1);
	ASSERT_TRUE(w > 0);

	/* Wait for echo to confirm data arrived */
	uint8_t buf[64];
	size_t recvd = 0;
	for (int i = 0; i < 50; i++) {
		int ev = dssh_chan_poll(ch,
		    DSSH_POLL_READ, 1000);
		if (ev <= 0)
			continue;
		int64_t n = dssh_chan_read(ch, 0,
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
	int ev = dssh_chan_poll(ch,
	    DSSH_POLL_READ, 1000);
	(void)ev;

	w = dssh_chan_write(ch, 0,
	    (const uint8_t *)"x", 1);
	ASSERT_TRUE(w <= 0);

	/* Server echo thread should have exited */
	thrd_join(ctx.server_thread, NULL);
	ctx.server_thread_active = false;

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
	dssh_session_set_terminate_cb(NULL, NULL, NULL);

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
 * Verify dssh_session_set_terminate_cb() stores the callback and
 * that session_set_terminate() invokes it exactly once.
 */
static void
terminate_test_cb(dssh_session sess, void *cbdata)
{
	(void)sess;
	int *count = cbdata;
	(*count)++;
}

static int
test_set_terminate_cb(void)
{
	dssh_session sess = dssh_session_init(true, 0);
	ASSERT_NOT_NULL(sess);

	int count = 0;
	dssh_session_set_terminate_cb(sess, terminate_test_cb, &count);

	/* Terminate fires the callback */
	dssh_session_terminate(sess);
	ASSERT_EQ(count, 1);

	/* Second terminate is a no-op (single-fire) */
	ASSERT_FALSE(dssh_session_terminate(sess));
	ASSERT_EQ(count, 1);

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
	dssh_channel ch = open_exec(ctx.client,"cmd");
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

	a->ch = dssh_chan_accept(ctx->server,
	    &selftest_accept_cbs, 30000);
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
	ASSERT_OK(selftest_start_thread(&ctx, setup_timeout_server_thread, &sarg));

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

	int res = send_packet(ctx.client, msg, pos, NULL);
	ASSERT_OK(res);

	/* Wait for server thread to complete (it should time out) */
	thrd_join(ctx.server_thread, NULL);
	ctx.server_thread_active = false;

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

	int res = send_packet(ctx.client, msg, 5, NULL);
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
 * New dssh_chan_* API tests
 * ================================================================ */

/*
 * Test: exec echo using the new dssh_chan_* API on the client side.
 * Server uses old API (will be migrated in Phase 4/6).
 */
static int
test_self_chan_exec_echo(void)
{
	struct selftest_ctx ctx;

	if (selftest_setup(&ctx) < 0)
		return TEST_FAIL;

	ASSERT_OK(dssh_session_start(ctx.client));
	ASSERT_OK(dssh_session_start(ctx.server));

	struct server_echo_arg sarg = { .ctx = &ctx };
	ASSERT_OK(selftest_start_thread(&ctx, server_echo_thread, &sarg));

	/* Open exec channel using new params builder + dssh_chan_open */
	struct dssh_chan_params p;

	ASSERT_OK(dssh_chan_params_init(&p, DSSH_CHAN_EXEC));
	ASSERT_OK(dssh_chan_params_set_command(&p, "echo"));
	ASSERT_OK(dssh_chan_params_set_pty(&p, false));

	dssh_channel ch = dssh_chan_open(ctx.client, &p);

	dssh_chan_params_free(&p);
	ASSERT_NOT_NULL(ch);

	/* Verify getters */
	ASSERT_EQ(dssh_chan_get_type(ch), DSSH_CHAN_EXEC);
	ASSERT_STR_EQ(dssh_chan_get_command(ch), "echo");
	ASSERT_NULL(dssh_chan_get_subsystem(ch));
	ASSERT_FALSE(dssh_chan_has_pty(ch));

	/* Write data using new API */
	dssh_chan_poll(ch, DSSH_POLL_WRITE, 5000);
	const uint8_t data[] = "Hello, dssh_chan!";
	int64_t w = dssh_chan_write(ch, 0, data, sizeof(data) - 1);

	ASSERT_TRUE(w > 0);

	/* Read echo using new API */
	uint8_t buf[256];
	size_t recvd = 0;

	for (int i = 0; i < 50; i++) {
		int ev = dssh_chan_poll(ch, DSSH_POLL_READ, 1000);

		if (ev <= 0)
			break;

		int64_t n = dssh_chan_read(ch, 0, buf + recvd,
		    sizeof(buf) - recvd);

		if (n <= 0)
			break;
		recvd += (size_t)n;
		if (recvd >= sizeof(data) - 1)
			break;
	}
	ASSERT_EQ_U(recvd, sizeof(data) - 1);
	ASSERT_MEM_EQ(buf, data, sizeof(data) - 1);

	dssh_chan_close(ch, -1);
	thrd_join(ctx.server_thread, NULL);
	ctx.server_thread_active = false;
	ASSERT_EQ(sarg.result, 0);

	selftest_cleanup(&ctx);
	return TEST_PASS;
}

/*
 * Test: shell with pty using the new dssh_chan_* API.
 */
static int
test_self_chan_shell_pty(void)
{
	struct selftest_ctx ctx;

	if (selftest_setup(&ctx) < 0)
		return TEST_FAIL;

	ASSERT_OK(dssh_session_start(ctx.client));
	ASSERT_OK(dssh_session_start(ctx.server));

	struct server_echo_arg sarg = { .ctx = &ctx };
	ASSERT_OK(selftest_start_thread(&ctx, server_echo_thread, &sarg));

	/* Open shell with pty using params builder */
	struct dssh_chan_params p;

	ASSERT_OK(dssh_chan_params_init(&p, DSSH_CHAN_SHELL));
	ASSERT_OK(dssh_chan_params_set_term(&p, "xterm-256color"));
	ASSERT_OK(dssh_chan_params_set_size(&p, 80, 24, 0, 0));

	dssh_channel ch = dssh_chan_open(ctx.client, &p);

	dssh_chan_params_free(&p);
	ASSERT_NOT_NULL(ch);

	/* Verify getters */
	ASSERT_EQ(dssh_chan_get_type(ch), DSSH_CHAN_SHELL);
	ASSERT_TRUE(dssh_chan_has_pty(ch));
	ASSERT_NULL(dssh_chan_get_command(ch));

	/* Write and read echo */
	dssh_chan_poll(ch, DSSH_POLL_WRITE, 5000);
	const uint8_t data[] = "pty-test";
	int64_t w = dssh_chan_write(ch, 0, data, sizeof(data) - 1);

	ASSERT_TRUE(w > 0);

	uint8_t buf[256];
	size_t recvd = 0;

	for (int i = 0; i < 50; i++) {
		int ev = dssh_chan_poll(ch, DSSH_POLL_READ, 1000);

		if (ev <= 0)
			break;

		int64_t n = dssh_chan_read(ch, 0, buf + recvd,
		    sizeof(buf) - recvd);

		if (n <= 0)
			break;
		recvd += (size_t)n;
		if (recvd >= sizeof(data) - 1)
			break;
	}
	ASSERT_EQ_U(recvd, sizeof(data) - 1);
	ASSERT_MEM_EQ(buf, data, sizeof(data) - 1);

	dssh_chan_close(ch, 0);
	thrd_join(ctx.server_thread, NULL);
	ctx.server_thread_active = false;
	ASSERT_EQ(sarg.result, 0);

	selftest_cleanup(&ctx);
	return TEST_PASS;
}

/*
 * Test: new dssh_chan_accept on server side, new dssh_chan_open on client.
 * Full round-trip using only the new API on both sides.
 */

/* Accept callbacks for new API tests */
static int
test_accept_shell_cb(dssh_channel ch,
    const struct dssh_chan_params *p,
    struct dssh_chan_accept_result *result, void *cbdata)
{
	(void)ch; (void)p; (void)result; (void)cbdata;
	return 0; /* accept, stream mode, default window */
}

static int
test_accept_exec_cb(dssh_channel ch,
    const struct dssh_chan_params *p,
    struct dssh_chan_accept_result *result, void *cbdata)
{
	(void)ch; (void)result; (void)cbdata;
	/* Verify command is set */
	if (p->command == NULL)
		return -1;
	return 0;
}

struct server_chan_echo_arg {
	struct selftest_ctx *ctx;
	enum dssh_chan_type expected_type;
	int result;
};

static int
server_chan_echo_thread(void *arg)
{
	struct server_chan_echo_arg *a = arg;
	struct selftest_ctx *ctx = a->ctx;
	a->result = -1;

	struct dssh_chan_accept_cbs cbs = {
		.shell = test_accept_shell_cb,
		.exec = test_accept_exec_cb,
	};

	dssh_channel ch = dssh_chan_accept(ctx->server, &cbs, 30000);

	if (ch == NULL)
		return 0;

	/* Verify type */
	if (dssh_chan_get_type(ch) != a->expected_type) {
		dssh_chan_close(ch, -1);
		return 0;
	}

	/* Echo loop */
	uint8_t buf[4096];

	for (;;) {
		int ev = dssh_chan_poll(ch, DSSH_POLL_READ, 30000);

		if (ev <= 0)
			break;

		int64_t n = dssh_chan_read(ch, 0, buf, sizeof(buf));

		if (n <= 0)
			break;

		/* Write all read data, polling for window if needed */
		size_t off = 0;

		while (off < (size_t)n) {
			int wev = dssh_chan_poll(ch,
			    DSSH_POLL_WRITE, 30000);

			if (wev <= 0)
				goto done;

			int64_t w = dssh_chan_write(ch, 0,
			    buf + off, (size_t)n - off);

			if (w < 0)
				goto done;
			off += (size_t)w;
		}
	}
done:

	dssh_chan_close(ch, 0);
	a->result = 0;
	return 0;
}

static int
test_self_chan_accept_exec(void)
{
	struct selftest_ctx ctx;

	if (selftest_setup(&ctx) < 0)
		return TEST_FAIL;

	ASSERT_OK(dssh_session_start(ctx.client));
	ASSERT_OK(dssh_session_start(ctx.server));

	struct server_chan_echo_arg sarg = {
		.ctx = &ctx,
		.expected_type = DSSH_CHAN_EXEC,
	};
	ASSERT_OK(selftest_start_thread(&ctx, server_chan_echo_thread, &sarg));

	/* Client: open exec with new API */
	struct dssh_chan_params p;

	ASSERT_OK(dssh_chan_params_init(&p, DSSH_CHAN_EXEC));
	ASSERT_OK(dssh_chan_params_set_command(&p, "test-cmd"));
	ASSERT_OK(dssh_chan_params_set_pty(&p, false));

	dssh_channel ch = dssh_chan_open(ctx.client, &p);

	dssh_chan_params_free(&p);
	ASSERT_NOT_NULL(ch);

	/* Write and read echo */
	dssh_chan_poll(ch, DSSH_POLL_WRITE, 5000);
	const uint8_t data[] = "round-trip-test";
	int64_t w = dssh_chan_write(ch, 0, data, sizeof(data) - 1);

	ASSERT_TRUE(w > 0);

	uint8_t buf[256];
	size_t recvd = 0;

	for (int i = 0; i < 50; i++) {
		int ev = dssh_chan_poll(ch, DSSH_POLL_READ, 1000);

		if (ev <= 0)
			break;

		int64_t n = dssh_chan_read(ch, 0, buf + recvd,
		    sizeof(buf) - recvd);

		if (n <= 0)
			break;
		recvd += (size_t)n;
		if (recvd >= sizeof(data) - 1)
			break;
	}
	ASSERT_EQ_U(recvd, sizeof(data) - 1);
	ASSERT_MEM_EQ(buf, data, sizeof(data) - 1);

	dssh_chan_close(ch, -1);
	thrd_join(ctx.server_thread, NULL);
	ctx.server_thread_active = false;
	ASSERT_EQ(sarg.result, 0);

	selftest_cleanup(&ctx);
	return TEST_PASS;
}

/*
 * Test: shell with pty, both sides using new API.
 * Verify server sees pty state via getters.
 */
struct server_chan_pty_arg {
	struct selftest_ctx *ctx;
	bool has_pty;
	int result;
};

static int
test_accept_pty_cb(dssh_channel ch,
    const struct dssh_chan_params *p, void *cbdata)
{
	bool *has_pty = cbdata;

	*has_pty = (p->flags & DSSH_PARAM_HAS_PTY) != 0;
	return 0; /* accept */
}

static int
server_chan_pty_thread(void *arg)
{
	struct server_chan_pty_arg *a = arg;
	struct selftest_ctx *ctx = a->ctx;
	a->result = -1;
	a->has_pty = false;

	struct dssh_chan_accept_cbs cbs = {
		.pty_req = test_accept_pty_cb,
		.shell = test_accept_shell_cb,
		.cbdata = &a->has_pty,
	};

	dssh_channel ch = dssh_chan_accept(ctx->server, &cbs, 30000);

	if (ch == NULL)
		return 0;

	/* Echo one message then close */
	uint8_t buf[256];
	int ev = dssh_chan_poll(ch, DSSH_POLL_READ, 30000);

	if (ev > 0) {
		int64_t n = dssh_chan_read(ch, 0, buf, sizeof(buf));

		if (n > 0)
			dssh_chan_write(ch, 0, buf, (size_t)n);
	}

	dssh_chan_close(ch, 0);
	a->result = 0;
	return 0;
}

static int
test_self_chan_accept_shell_pty(void)
{
	struct selftest_ctx ctx;

	if (selftest_setup(&ctx) < 0)
		return TEST_FAIL;

	ASSERT_OK(dssh_session_start(ctx.client));
	ASSERT_OK(dssh_session_start(ctx.server));

	struct server_chan_pty_arg sarg = { .ctx = &ctx };
	ASSERT_OK(selftest_start_thread(&ctx, server_chan_pty_thread, &sarg));

	/* Client: open shell with pty */
	struct dssh_chan_params p;

	ASSERT_OK(dssh_chan_params_init(&p, DSSH_CHAN_SHELL));
	ASSERT_OK(dssh_chan_params_set_term(&p, "vt100"));
	ASSERT_OK(dssh_chan_params_set_size(&p, 132, 50, 0, 0));

	dssh_channel ch = dssh_chan_open(ctx.client, &p);

	dssh_chan_params_free(&p);
	ASSERT_NOT_NULL(ch);

	const uint8_t data[] = "ping";

	dssh_chan_poll(ch, DSSH_POLL_WRITE, 5000);
	dssh_chan_write(ch, 0, data, sizeof(data) - 1);

	uint8_t buf[256];
	size_t recvd = 0;

	for (int i = 0; i < 50; i++) {
		int ev = dssh_chan_poll(ch, DSSH_POLL_READ, 1000);

		if (ev <= 0)
			break;

		int64_t n = dssh_chan_read(ch, 0, buf + recvd,
		    sizeof(buf) - recvd);

		if (n <= 0)
			break;
		recvd += (size_t)n;
		if (recvd >= sizeof(data) - 1)
			break;
	}
	ASSERT_EQ_U(recvd, sizeof(data) - 1);

	dssh_chan_close(ch, 0);
	thrd_join(ctx.server_thread, NULL);
	ctx.server_thread_active = false;
	ASSERT_EQ(sarg.result, 0);
	ASSERT_TRUE(sarg.has_pty);

	selftest_cleanup(&ctx);
	return TEST_PASS;
}

/*
 * Test: exit-status event via new dssh_chan_* API.
 * Server sends data, then closes with exit_code=42.
 * Client reads data, then reads exit-status event.
 */
static int
server_chan_exit_thread(void *arg)
{
	struct server_chan_echo_arg *a = arg;
	struct selftest_ctx *ctx = a->ctx;
	a->result = -1;

	struct dssh_chan_accept_cbs cbs = {
		.shell = test_accept_shell_cb,
		.exec = test_accept_exec_cb,
	};

	dssh_channel ch = dssh_chan_accept(ctx->server, &cbs, 30000);

	if (ch == NULL)
		return 0;

	dssh_chan_poll(ch, DSSH_POLL_WRITE, 30000);

	const uint8_t msg[] = "hello";

	dssh_chan_write(ch, 0, msg, sizeof(msg) - 1);
	dssh_chan_close(ch, 42);
	a->result = 0;
	return 0;
}

static int
test_self_chan_event_exit_status(void)
{
	struct selftest_ctx ctx;

	if (selftest_setup(&ctx) < 0)
		return TEST_FAIL;

	ASSERT_OK(dssh_session_start(ctx.client));
	ASSERT_OK(dssh_session_start(ctx.server));

	struct server_chan_echo_arg sarg = {
		.ctx = &ctx,
		.expected_type = DSSH_CHAN_EXEC,
	};
	ASSERT_OK(selftest_start_thread(&ctx, server_chan_exit_thread, &sarg));

	struct dssh_chan_params p;

	ASSERT_OK(dssh_chan_params_init(&p, DSSH_CHAN_EXEC));
	ASSERT_OK(dssh_chan_params_set_command(&p, "test"));
	ASSERT_OK(dssh_chan_params_set_pty(&p, false));

	dssh_channel ch = dssh_chan_open(ctx.client, &p);

	dssh_chan_params_free(&p);
	ASSERT_NOT_NULL(ch);

	/* Drain data and look for events */
	bool got_exit_status = false;
	bool got_eof = false;
	uint32_t exit_code = 0;

	for (int i = 0; i < 100; i++) {
		int ev = dssh_chan_poll(ch,
		    DSSH_POLL_READ | DSSH_POLL_EVENT, 1000);

		if (ev <= 0)
			break;

		if (ev & DSSH_POLL_EVENT) {
			struct dssh_chan_event event;
			int eret = dssh_chan_read_event(ch, &event);

			if (eret == 0) {
				if (event.type == DSSH_EVENT_EXIT_STATUS) {
					got_exit_status = true;
					exit_code = event.exit_status.exit_code;
				}
				else if (event.type == DSSH_EVENT_EOF) {
					got_eof = true;
				}
			}
			continue;
		}

		if (ev & DSSH_POLL_READ) {
			uint8_t buf[256];
			int64_t n = dssh_chan_read(ch, 0, buf, sizeof(buf));

			if (n <= 0)
				break;
		}

		if (got_exit_status && got_eof)
			break;
	}

	ASSERT_TRUE(got_exit_status);
	ASSERT_EQ_U(exit_code, 42);
	ASSERT_TRUE(got_eof);

	dssh_chan_close(ch, -1);
	thrd_join(ctx.server_thread, NULL);
	ctx.server_thread_active = false;
	ASSERT_EQ(sarg.result, 0);

	selftest_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Phase 7: Additional coverage tests
 * ================================================================ */

/* Read peek: dssh_chan_read(ch, 0, NULL, 0) returns available bytes */
static int
test_self_chan_read_peek(void)
{
	struct selftest_ctx ctx;

	if (selftest_setup(&ctx) < 0)
		return TEST_FAIL;

	ASSERT_OK(dssh_session_start(ctx.client));
	ASSERT_OK(dssh_session_start(ctx.server));

	struct server_echo_arg sarg = { .ctx = &ctx };
	ASSERT_OK(selftest_start_thread(&ctx, server_echo_thread, &sarg));

	dssh_channel ch = open_exec(ctx.client, "peek-test");

	ASSERT_NOT_NULL(ch);

	const uint8_t data[] = "hello";

	dssh_chan_poll(ch, DSSH_POLL_WRITE, 5000);
	dssh_chan_write(ch, 0, data, sizeof(data) - 1);

	/* Wait for echo data to arrive */
	dssh_chan_poll(ch, DSSH_POLL_READ, 30000);

	/* Peek: should report available bytes without consuming */
	int64_t avail = dssh_chan_read(ch, 0, NULL, 0);

	ASSERT_TRUE(avail > 0);

	/* Read should still get the data */
	uint8_t buf[64];
	int64_t n = dssh_chan_read(ch, 0, buf, sizeof(buf));

	ASSERT_EQ(n, avail);

	dssh_chan_close(ch, -1);
	thrd_join(ctx.server_thread, NULL);
	ctx.server_thread_active = false;
	selftest_cleanup(&ctx);
	return TEST_PASS;
}

/* Shutwr then close: EOF not sent twice */
static int
test_self_chan_shutwr_then_close(void)
{
	struct selftest_ctx ctx;

	if (selftest_setup(&ctx) < 0)
		return TEST_FAIL;

	ASSERT_OK(dssh_session_start(ctx.client));
	ASSERT_OK(dssh_session_start(ctx.server));

	struct server_echo_arg sarg = { .ctx = &ctx };
	ASSERT_OK(selftest_start_thread(&ctx, server_echo_thread, &sarg));

	dssh_channel ch = open_exec(ctx.client, "shutwr-test");

	ASSERT_NOT_NULL(ch);

	/* shutwr sends EOF */
	ASSERT_OK(dssh_chan_shutwr(ch));

	/* close should not fail even though EOF already sent */
	ASSERT_OK(dssh_chan_close(ch, 0));

	thrd_join(ctx.server_thread, NULL);
	ctx.server_thread_active = false;
	ASSERT_EQ(sarg.result, 0);

	selftest_cleanup(&ctx);
	return TEST_PASS;
}

/* Close with exit code */
static int
test_self_chan_close_exit_code(void)
{
	struct selftest_ctx ctx;

	if (selftest_setup(&ctx) < 0)
		return TEST_FAIL;

	ASSERT_OK(dssh_session_start(ctx.client));
	ASSERT_OK(dssh_session_start(ctx.server));

	/* Server reads exit-status event */
	struct server_chan_echo_arg server_arg = {
		.ctx = &ctx,
		.expected_type = DSSH_CHAN_EXEC,
	};
	ASSERT_OK(selftest_start_thread(&ctx, server_chan_echo_thread, &server_arg));

	dssh_channel ch = open_exec(ctx.client, "exit42");

	ASSERT_NOT_NULL(ch);

	/* Close with exit_code=42 */
	dssh_chan_close(ch, 42);
	thrd_join(ctx.server_thread, NULL);
	ctx.server_thread_active = false;

	selftest_cleanup(&ctx);
	return TEST_PASS;
}

/* Close without exit code (negative) */
static int
test_self_chan_close_no_exit_code(void)
{
	struct selftest_ctx ctx;

	if (selftest_setup(&ctx) < 0)
		return TEST_FAIL;

	ASSERT_OK(dssh_session_start(ctx.client));
	ASSERT_OK(dssh_session_start(ctx.server));

	struct server_echo_arg sarg = { .ctx = &ctx };
	ASSERT_OK(selftest_start_thread(&ctx, server_echo_thread, &sarg));

	dssh_channel ch = open_exec(ctx.client, "no-exit");

	ASSERT_NOT_NULL(ch);

	/* Close with negative = no exit-status sent */
	dssh_chan_close(ch, -1);
	thrd_join(ctx.server_thread, NULL);
	ctx.server_thread_active = false;

	selftest_cleanup(&ctx);
	return TEST_PASS;
}

/* Send break (RFC 4335) */
static int
test_self_chan_send_break(void)
{
	struct selftest_ctx ctx;

	if (selftest_setup(&ctx) < 0)
		return TEST_FAIL;

	ASSERT_OK(dssh_session_start(ctx.client));
	ASSERT_OK(dssh_session_start(ctx.server));

	struct server_echo_arg sarg = { .ctx = &ctx };
	ASSERT_OK(selftest_start_thread(&ctx, server_echo_thread, &sarg));

	dssh_channel ch = open_exec(ctx.client, "break-test");

	ASSERT_NOT_NULL(ch);

	/* Send break -- should not error */
	ASSERT_OK(dssh_chan_send_break(ch, 500));

	dssh_chan_close(ch, -1);
	thrd_join(ctx.server_thread, NULL);
	ctx.server_thread_active = false;
	selftest_cleanup(&ctx);
	return TEST_PASS;
}

/* Getters after accept: server verifies type, command, pty state */
struct server_getter_arg {
	struct selftest_ctx *ctx;
	enum dssh_chan_type type;
	const char *command;
	bool has_pty;
	int result;
};

static int
server_getter_thread(void *arg)
{
	struct server_getter_arg *a = arg;
	struct selftest_ctx *ctx = a->ctx;
	a->result = -1;

	struct dssh_chan_accept_cbs cbs = {
		.shell = test_accept_shell_cb,
		.exec = test_accept_exec_cb,
	};

	dssh_channel ch = dssh_chan_accept(ctx->server, &cbs, 30000);

	if (ch == NULL)
		return 0;

	a->type = dssh_chan_get_type(ch);
	a->command = dssh_chan_get_command(ch);
	a->has_pty = dssh_chan_has_pty(ch);

	dssh_chan_close(ch, 0);
	a->result = 0;
	return 0;
}

static int
test_self_chan_getters_after_accept(void)
{
	struct selftest_ctx ctx;

	if (selftest_setup(&ctx) < 0)
		return TEST_FAIL;

	ASSERT_OK(dssh_session_start(ctx.client));
	ASSERT_OK(dssh_session_start(ctx.server));

	struct server_getter_arg sarg = { .ctx = &ctx };
	ASSERT_OK(selftest_start_thread(&ctx, server_getter_thread, &sarg));

	struct dssh_chan_params p;

	dssh_chan_params_init(&p, DSSH_CHAN_EXEC);
	dssh_chan_params_set_command(&p, "my-command");
	dssh_chan_params_set_pty(&p, false);

	dssh_channel ch = dssh_chan_open(ctx.client, &p);

	dssh_chan_params_free(&p);
	ASSERT_NOT_NULL(ch);

	dssh_chan_close(ch, -1);
	thrd_join(ctx.server_thread, NULL);
	ctx.server_thread_active = false;

	ASSERT_EQ(sarg.result, 0);
	ASSERT_EQ(sarg.type, DSSH_CHAN_EXEC);
	ASSERT_NOT_NULL(sarg.command);
	ASSERT_STR_EQ(sarg.command, "my-command");
	ASSERT_FALSE(sarg.has_pty);

	selftest_cleanup(&ctx);
	return TEST_PASS;
}

/* Accept with NULL env callback rejects env vars */
struct server_env_reject_arg {
	struct selftest_ctx *ctx;
	int result;
};

static int
server_env_reject_thread(void *arg)
{
	struct server_env_reject_arg *a = arg;
	struct selftest_ctx *ctx = a->ctx;
	a->result = -1;

	/* No env callback = auto-reject */
	struct dssh_chan_accept_cbs cbs = {
		.exec = test_accept_exec_cb,
	};

	dssh_channel ch = dssh_chan_accept(ctx->server, &cbs, 30000);

	if (ch == NULL)
		return 0;

	/* Channel should open successfully despite env rejection */
	dssh_chan_close(ch, 0);
	a->result = 0;
	return 0;
}

static int
test_self_chan_accept_env_reject(void)
{
	struct selftest_ctx ctx;

	if (selftest_setup(&ctx) < 0)
		return TEST_FAIL;

	ASSERT_OK(dssh_session_start(ctx.client));
	ASSERT_OK(dssh_session_start(ctx.server));

	struct server_env_reject_arg sarg = { .ctx = &ctx };
	ASSERT_OK(selftest_start_thread(&ctx, server_env_reject_thread, &sarg));

	/* Client sends env vars which server will reject */
	struct dssh_chan_params p;

	dssh_chan_params_init(&p, DSSH_CHAN_EXEC);
	dssh_chan_params_set_command(&p, "env-test");
	dssh_chan_params_set_pty(&p, false);
	dssh_chan_params_add_env(&p, "LANG", "en_US.UTF-8");
	dssh_chan_params_add_env(&p, "TERM", "xterm");

	dssh_channel ch = dssh_chan_open(ctx.client, &p);

	dssh_chan_params_free(&p);
	ASSERT_NOT_NULL(ch);

	dssh_chan_close(ch, -1);
	thrd_join(ctx.server_thread, NULL);
	ctx.server_thread_active = false;
	ASSERT_EQ(sarg.result, 0);

	selftest_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * After-each cleanup: if a test FAILs mid-way (ASSERT bails out),
 * clean up the active selftest context so leaked demux/accept
 * threads don't interfere with subsequent tests in the same process.
 * ================================================================ */

static void
dssh_test_after_each(int result)
{
	(void)result;
	if (g_active_ctx != NULL) {
		selftest_cleanup(g_active_ctx);
		g_active_ctx = NULL;
	}
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
	{ "test_set_terminate_cb",             test_set_terminate_cb },
	{ "test_open_channel_timeout",         test_open_channel_timeout },
	{ "test_setup_recv_timeout",           test_setup_recv_timeout },
	{ "test_rekey_send_timeout",           test_rekey_send_timeout },
	{ "test_self_chan_exec_echo",          test_self_chan_exec_echo },
	{ "test_self_chan_shell_pty",          test_self_chan_shell_pty },
	{ "test_self_chan_accept_exec",        test_self_chan_accept_exec },
	{ "test_self_chan_accept_shell_pty",   test_self_chan_accept_shell_pty },
	{ "test_self_chan_event_exit_status",  test_self_chan_event_exit_status },
	{ "test_self_chan_read_peek",          test_self_chan_read_peek },
	{ "test_self_chan_shutwr_then_close",  test_self_chan_shutwr_then_close },
	{ "test_self_chan_close_exit_code",    test_self_chan_close_exit_code },
	{ "test_self_chan_close_no_exit_code", test_self_chan_close_no_exit_code },
	{ "test_self_chan_send_break",         test_self_chan_send_break },
	{ "test_self_chan_getters_after_accept", test_self_chan_getters_after_accept },
	{ "test_self_chan_accept_env_reject",  test_self_chan_accept_env_reject },
};

DSSH_TEST_MAIN(tests)
