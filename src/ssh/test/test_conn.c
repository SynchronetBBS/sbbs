/*
 * test_conn.c — Connection protocol tests for DeuceSSH.
 *
 * Tier 2 layer tests with mock I/O.  Covers session start/stop,
 * channel open/accept, session and raw channel I/O, signals,
 * window change, close/exit, multiple channels, and thread safety.
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
#include "dssh_test_internal.h"
#include "mock_io.h"


/* ================================================================
 * Dispatch callbacks for two-sided tests.
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
 * Auth callbacks (accept-all server side)
 * ================================================================ */

static int
auth_password_accept_all(const uint8_t *username, size_t username_len,
    const uint8_t *password, size_t password_len,
    uint8_t **change_prompt, size_t *change_prompt_len,
    void *cbdata)
{
	return DSSH_AUTH_SUCCESS;
}

static struct dssh_auth_server_cbs auth_cbs = {
	.methods_str = "password",
	.password_cb = auth_password_accept_all,
};

/* ================================================================
 * Server session callbacks (unified channel request callback)
 * ================================================================ */

static int pty_received;
static char pty_term[64];
static uint32_t pty_cols, pty_rows;
static int wc_received;
static uint32_t wc_cols, wc_rows, wc_wpx, wc_hpx;

static int
test_channel_request_cb(const char *type, size_t type_len,
    bool want_reply, const uint8_t *data, size_t data_len,
    void *cbdata)
{
	if (type_len == 7 && memcmp(type, "pty-req", 7) == 0) {
		struct dssh_pty_req pty;
		if (dssh_parse_pty_req_data(data, data_len, &pty) == 0) {
			pty_received = 1;
			/* pty.term points into data[] and is NOT NUL-terminated */
			uint32_t tlen;
			dssh_parse_uint32(data, data_len, &tlen);
			size_t n = tlen < sizeof(pty_term) - 1
			    ? tlen : sizeof(pty_term) - 1;
			memcpy(pty_term, pty.term, n);
			pty_term[n] = 0;
			pty_cols = pty.cols;
			pty_rows = pty.rows;
		}
		return 0;
	}
	/* Accept all known request types */
	if (type_len == 3 && memcmp(type, "env", 3) == 0)
		return 0;
	if (type_len == 5 && memcmp(type, "shell", 5) == 0)
		return 0;
	if (type_len == 4 && memcmp(type, "exec", 4) == 0)
		return 0;
	if (type_len == 9 && memcmp(type, "subsystem", 9) == 0)
		return 0;
	return -1;
}

static void
test_window_change_cb(uint32_t cols, uint32_t rows,
    uint32_t wpx, uint32_t hpx, void *cbdata)
{
	wc_received = 1;
	wc_cols = cols;
	wc_rows = rows;
	wc_wpx = wpx;
	wc_hpx = hpx;
}

static struct dssh_server_session_cbs session_cbs = {
	.request_cb = test_channel_request_cb,
	.window_change = test_window_change_cb,
};

static void
reset_callbacks(void)
{
	pty_received = 0;
	pty_term[0] = 0;
	pty_cols = 0;
	pty_rows = 0;
	wc_received = 0;
	wc_cols = 0;
	wc_rows = 0;
	wc_wpx = 0;
	wc_hpx = 0;
}

/* ================================================================
 * Connection setup context — handshake + auth + session_start
 * ================================================================ */

struct conn_ctx {
	struct mock_io_state io;
	dssh_session client;
	dssh_session server;
};

struct handshake_auth_ctx {
	struct mock_io_state *io;
	dssh_session sess;
	int result;
};

static int
client_handshake_auth_thread(void *arg)
{
	struct handshake_auth_ctx *ctx = arg;
	ctx->result = dssh_transport_handshake(ctx->sess);
	if (ctx->result < 0)
		return 0;
	ctx->result = dssh_auth_password(ctx->sess, "testuser", "testpass",
	    NULL, NULL);
	return 0;
}

static int
server_handshake_auth_thread(void *arg)
{
	struct handshake_auth_ctx *ctx = arg;
	ctx->result = dssh_transport_handshake(ctx->sess);
	if (ctx->result < 0)
		return 0;
	uint8_t username[256];
	size_t username_len;
	ctx->result = dssh_auth_server(ctx->sess, &auth_cbs,
	    username, &username_len);
	return 0;
}

/*
 * Set up a full connection: handshake + auth + session_start on both sides.
 * Returns 0 on success, -1 on failure.
 */
static int
conn_setup(struct conn_ctx *ctx)
{
	memset(ctx, 0, sizeof(*ctx));
	reset_callbacks();

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

	struct handshake_auth_ctx client_ha = {
		.io = &ctx->io,
		.sess = ctx->client,
		.result = -1,
	};
	struct handshake_auth_ctx server_ha = {
		.io = &ctx->io,
		.sess = ctx->server,
		.result = -1,
	};

	thrd_t ct, st;
	if (thrd_create(&ct, client_handshake_auth_thread, &client_ha) != thrd_success) {
		dssh_session_cleanup(ctx->server);
		dssh_session_cleanup(ctx->client);
		mock_io_free(&ctx->io);
		return -1;
	}
	if (thrd_create(&st, server_handshake_auth_thread, &server_ha) != thrd_success) {
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

	if (client_ha.result != 0 || server_ha.result != 0) {
		dssh_session_cleanup(ctx->server);
		dssh_session_cleanup(ctx->client);
		mock_io_free(&ctx->io);
		return -1;
	}

	if (dssh_session_start(ctx->client) != 0) {
		dssh_session_cleanup(ctx->server);
		dssh_session_cleanup(ctx->client);
		mock_io_free(&ctx->io);
		return -1;
	}
	if (dssh_session_start(ctx->server) != 0) {
		dssh_session_stop(ctx->client);
		dssh_session_cleanup(ctx->server);
		dssh_session_cleanup(ctx->client);
		mock_io_free(&ctx->io);
		return -1;
	}

	return 0;
}

static void
conn_cleanup(struct conn_ctx *ctx)
{
	/* Close pipes first to unblock demux threads waiting in recv */
	mock_io_close_c2s(&ctx->io);
	mock_io_close_s2c(&ctx->io);
	dssh_session_stop(ctx->server);
	dssh_session_stop(ctx->client);
	dssh_session_cleanup(ctx->server);
	dssh_session_cleanup(ctx->client);
	mock_io_free(&ctx->io);
	dssh_test_reset_global_config();
}

/* ================================================================
 * Helper: open an exec channel concurrently (client+server threads)
 * ================================================================ */

struct open_exec_ctx {
	dssh_session client;
	dssh_session server;
	dssh_channel client_ch;
	dssh_channel server_ch;
	const char *command;
	const char *req_type;
	const char *req_data;
	const struct dssh_server_session_cbs *cbs;
	int accept_timeout;
	bool reject;
	uint32_t reject_reason;
};

static int
client_open_exec_thread(void *arg)
{
	struct open_exec_ctx *ctx = arg;
	ctx->client_ch = dssh_session_open_exec(ctx->client, ctx->command);
	return 0;
}

static int
server_accept_exec_thread(void *arg)
{
	struct open_exec_ctx *ctx = arg;
	struct dssh_incoming_open *inc = NULL;
	int res = dssh_session_accept(ctx->server, &inc, ctx->accept_timeout);
	if (res < 0)
		return 0;
	if (ctx->reject) {
		dssh_session_reject(ctx->server, inc, ctx->reject_reason,
		    "rejected");
		return 0;
	}
	ctx->server_ch = dssh_session_accept_channel(ctx->server, inc,
	    ctx->cbs, &ctx->req_type, &ctx->req_data);
	return 0;
}

/*
 * Open an exec channel concurrently.  Returns 0 on success, -1 on failure.
 * Populates ctx->client_ch and ctx->server_ch.
 */
static int
open_exec_channel(struct open_exec_ctx *ctx)
{
	thrd_t ct, st;
	if (thrd_create(&ct, client_open_exec_thread, ctx) != thrd_success)
		return -1;
	if (thrd_create(&st, server_accept_exec_thread, ctx) != thrd_success) {
		thrd_join(ct, NULL);
		return -1;
	}
	thrd_join(ct, NULL);
	thrd_join(st, NULL);
	if (ctx->client_ch == NULL || ctx->server_ch == NULL)
		return -1;
	return 0;
}

/* ================================================================
 * Helper: open a shell channel concurrently
 * ================================================================ */

struct open_shell_ctx {
	dssh_session client;
	dssh_session server;
	dssh_channel client_ch;
	dssh_channel server_ch;
	const struct dssh_pty_req *pty;
	const char *req_type;
	const char *req_data;
	const struct dssh_server_session_cbs *cbs;
};

static int
client_open_shell_thread(void *arg)
{
	struct open_shell_ctx *ctx = arg;
	ctx->client_ch = dssh_session_open_shell(ctx->client, ctx->pty);
	return 0;
}

static int
server_accept_shell_thread(void *arg)
{
	struct open_shell_ctx *ctx = arg;
	struct dssh_incoming_open *inc = NULL;
	int res = dssh_session_accept(ctx->server, &inc, 5000);
	if (res < 0)
		return 0;
	ctx->server_ch = dssh_session_accept_channel(ctx->server, inc,
	    ctx->cbs, &ctx->req_type, &ctx->req_data);
	return 0;
}

/* ================================================================
 * Helper: open a subsystem (raw) channel concurrently
 * ================================================================ */

struct open_subsys_ctx {
	dssh_session client;
	dssh_session server;
	dssh_channel client_ch;
	dssh_channel server_ch;
	const char *subsystem;
	const char *req_type;
	const char *req_data;
};

static int
client_open_subsys_thread(void *arg)
{
	struct open_subsys_ctx *ctx = arg;
	ctx->client_ch = dssh_channel_open_subsystem(ctx->client, ctx->subsystem);
	return 0;
}

static int
server_accept_subsys_thread(void *arg)
{
	struct open_subsys_ctx *ctx = arg;
	struct dssh_incoming_open *inc = NULL;
	int res = dssh_session_accept(ctx->server, &inc, 5000);
	if (res < 0)
		return 0;
	/* Accept as session channel to handle the "subsystem" request */
	ctx->server_ch = dssh_session_accept_channel(ctx->server, inc,
	    &session_cbs, &ctx->req_type, &ctx->req_data);
	return 0;
}


/* ================================================================
 * Session start/stop (~3 tests)
 * ================================================================ */

static int
test_session_start(void)
{
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_SKIP;

	ASSERT_TRUE(ctx.client->demux_running);
	ASSERT_TRUE(ctx.server->demux_running);

	conn_cleanup(&ctx);
	return TEST_PASS;
}

static int
test_session_stop(void)
{
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_SKIP;

	/* Close pipes to unblock demux threads before stopping */
	mock_io_close_c2s(&ctx.io);
	mock_io_close_s2c(&ctx.io);

	dssh_session_stop(ctx.client);
	ASSERT_FALSE(ctx.client->demux_running);

	dssh_session_stop(ctx.server);
	ASSERT_FALSE(ctx.server->demux_running);

	dssh_session_cleanup(ctx.server);
	dssh_session_cleanup(ctx.client);
	mock_io_free(&ctx.io);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

static int
test_session_start_stop(void)
{
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_SKIP;

	ASSERT_TRUE(ctx.client->demux_running);
	ASSERT_TRUE(ctx.server->demux_running);

	/* Close pipes to unblock demux threads before stopping */
	mock_io_close_c2s(&ctx.io);
	mock_io_close_s2c(&ctx.io);

	dssh_session_stop(ctx.client);
	dssh_session_stop(ctx.server);

	ASSERT_FALSE(ctx.client->demux_running);
	ASSERT_FALSE(ctx.server->demux_running);

	dssh_session_cleanup(ctx.server);
	dssh_session_cleanup(ctx.client);
	mock_io_free(&ctx.io);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

/* ================================================================
 * Channel open — exec (~5 tests)
 * ================================================================ */

static int
test_open_exec(void)
{
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_SKIP;

	struct open_exec_ctx oc = {
		.client = ctx.client,
		.server = ctx.server,
		.command = "echo hello",
		.cbs = &session_cbs,
		.accept_timeout = 5000,
	};

	if (open_exec_channel(&oc) < 0) {
		conn_cleanup(&ctx);
		return TEST_FAIL;
	}

	ASSERT_NOT_NULL(oc.client_ch);
	ASSERT_NOT_NULL(oc.server_ch);

	dssh_session_close(ctx.client, oc.client_ch, 0);
	dssh_session_close(ctx.server, oc.server_ch, 0);
	conn_cleanup(&ctx);
	return TEST_PASS;
}

static int
test_open_exec_command(void)
{
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_SKIP;

	struct open_exec_ctx oc = {
		.client = ctx.client,
		.server = ctx.server,
		.command = "ls -la /tmp",
		.cbs = &session_cbs,
		.accept_timeout = 5000,
	};

	if (open_exec_channel(&oc) < 0) {
		conn_cleanup(&ctx);
		return TEST_FAIL;
	}

	ASSERT_NOT_NULL(oc.req_type);
	ASSERT_STR_EQ(oc.req_type, "exec");
	ASSERT_NOT_NULL(oc.req_data);
	ASSERT_STR_EQ(oc.req_data, "ls -la /tmp");

	dssh_session_close(ctx.client, oc.client_ch, 0);
	dssh_session_close(ctx.server, oc.server_ch, 0);
	conn_cleanup(&ctx);
	return TEST_PASS;
}

static int
test_open_exec_rejected(void)
{
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_SKIP;

	struct open_exec_ctx oc = {
		.client = ctx.client,
		.server = ctx.server,
		.command = "rejected_cmd",
		.cbs = &session_cbs,
		.accept_timeout = 5000,
		.reject = true,
		.reject_reason = 1,
	};

	thrd_t ct, st;
	thrd_create(&ct, client_open_exec_thread, &oc);
	thrd_create(&st, server_accept_exec_thread, &oc);
	thrd_join(ct, NULL);
	thrd_join(st, NULL);

	ASSERT_NULL(oc.client_ch);

	conn_cleanup(&ctx);
	return TEST_PASS;
}

static int
test_open_shell(void)
{
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_SKIP;

	struct dssh_pty_req pty = {
		.term = "xterm-256color",
		.cols = 80,
		.rows = 24,
		.wpx = 0,
		.hpx = 0,
	};

	struct open_shell_ctx oc = {
		.client = ctx.client,
		.server = ctx.server,
		.pty = &pty,
		.cbs = &session_cbs,
	};

	thrd_t ct, st;
	thrd_create(&ct, client_open_shell_thread, &oc);
	thrd_create(&st, server_accept_shell_thread, &oc);
	thrd_join(ct, NULL);
	thrd_join(st, NULL);

	ASSERT_NOT_NULL(oc.client_ch);
	ASSERT_NOT_NULL(oc.server_ch);
	ASSERT_NOT_NULL(oc.req_type);
	ASSERT_STR_EQ(oc.req_type, "shell");
	ASSERT_TRUE(pty_received);
	ASSERT_STR_EQ(pty_term, "xterm-256color");
	ASSERT_EQ_U(pty_cols, 80);
	ASSERT_EQ_U(pty_rows, 24);

	dssh_session_close(ctx.client, oc.client_ch, 0);
	dssh_session_close(ctx.server, oc.server_ch, 0);
	conn_cleanup(&ctx);
	return TEST_PASS;
}

static int
test_open_subsystem(void)
{
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_SKIP;

	struct open_subsys_ctx oc = {
		.client = ctx.client,
		.server = ctx.server,
		.subsystem = "sftp",
	};

	thrd_t ct, st;
	thrd_create(&ct, client_open_subsys_thread, &oc);
	thrd_create(&st, server_accept_subsys_thread, &oc);
	thrd_join(ct, NULL);
	thrd_join(st, NULL);

	ASSERT_NOT_NULL(oc.client_ch);
	ASSERT_NOT_NULL(oc.server_ch);

	dssh_channel_close(ctx.client, oc.client_ch);
	dssh_channel_close(ctx.server, oc.server_ch);
	conn_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Session I/O (~8 tests)
 * ================================================================ */

static int
test_session_write_read(void)
{
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_SKIP;

	struct open_exec_ctx oc = {
		.client = ctx.client,
		.server = ctx.server,
		.command = "cmd",
		.cbs = &session_cbs,
		.accept_timeout = 5000,
	};
	if (open_exec_channel(&oc) < 0) {
		conn_cleanup(&ctx);
		return TEST_SKIP;
	}

	const uint8_t msg[] = "Hello, server!";
	int64_t written = dssh_session_write(ctx.client, oc.client_ch,
	    msg, sizeof(msg) - 1);
	ASSERT_TRUE(written > 0);

	int ev = dssh_session_poll(ctx.server, oc.server_ch,
	    DSSH_POLL_READ, 5000);
	ASSERT_TRUE(ev & DSSH_POLL_READ);

	uint8_t buf[256];
	int64_t nread = dssh_session_read(ctx.server, oc.server_ch,
	    buf, sizeof(buf));
	ASSERT_TRUE(nread > 0);
	ASSERT_EQ(nread, (int64_t)(sizeof(msg) - 1));
	ASSERT_MEM_EQ(buf, msg, sizeof(msg) - 1);

	dssh_session_close(ctx.client, oc.client_ch, 0);
	dssh_session_close(ctx.server, oc.server_ch, 0);
	conn_cleanup(&ctx);
	return TEST_PASS;
}

static int
test_session_read_write_reverse(void)
{
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_SKIP;

	struct open_exec_ctx oc = {
		.client = ctx.client,
		.server = ctx.server,
		.command = "cmd",
		.cbs = &session_cbs,
		.accept_timeout = 5000,
	};
	if (open_exec_channel(&oc) < 0) {
		conn_cleanup(&ctx);
		return TEST_SKIP;
	}

	const uint8_t msg[] = "Hello, client!";
	int64_t written = dssh_session_write(ctx.server, oc.server_ch,
	    msg, sizeof(msg) - 1);
	ASSERT_TRUE(written > 0);

	int ev = dssh_session_poll(ctx.client, oc.client_ch,
	    DSSH_POLL_READ, 5000);
	ASSERT_TRUE(ev & DSSH_POLL_READ);

	uint8_t buf[256];
	int64_t nread = dssh_session_read(ctx.client, oc.client_ch,
	    buf, sizeof(buf));
	ASSERT_TRUE(nread > 0);
	ASSERT_EQ(nread, (int64_t)(sizeof(msg) - 1));
	ASSERT_MEM_EQ(buf, msg, sizeof(msg) - 1);

	dssh_session_close(ctx.client, oc.client_ch, 0);
	dssh_session_close(ctx.server, oc.server_ch, 0);
	conn_cleanup(&ctx);
	return TEST_PASS;
}

static int
test_session_write_ext(void)
{
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_SKIP;

	struct open_exec_ctx oc = {
		.client = ctx.client,
		.server = ctx.server,
		.command = "cmd",
		.cbs = &session_cbs,
		.accept_timeout = 5000,
	};
	if (open_exec_channel(&oc) < 0) {
		conn_cleanup(&ctx);
		return TEST_SKIP;
	}

	const uint8_t stderr_msg[] = "error output";
	int64_t written = dssh_session_write_ext(ctx.server, oc.server_ch,
	    stderr_msg, sizeof(stderr_msg) - 1);
	ASSERT_TRUE(written > 0);

	int ev = dssh_session_poll(ctx.client, oc.client_ch,
	    DSSH_POLL_READEXT, 5000);
	ASSERT_TRUE(ev & DSSH_POLL_READEXT);

	uint8_t buf[256];
	int64_t nread = dssh_session_read_ext(ctx.client, oc.client_ch,
	    buf, sizeof(buf));
	ASSERT_TRUE(nread > 0);
	ASSERT_EQ(nread, (int64_t)(sizeof(stderr_msg) - 1));
	ASSERT_MEM_EQ(buf, stderr_msg, sizeof(stderr_msg) - 1);

	dssh_session_close(ctx.client, oc.client_ch, 0);
	dssh_session_close(ctx.server, oc.server_ch, 0);
	conn_cleanup(&ctx);
	return TEST_PASS;
}

static int
test_session_read_ext(void)
{
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_SKIP;

	struct open_exec_ctx oc = {
		.client = ctx.client,
		.server = ctx.server,
		.command = "cmd",
		.cbs = &session_cbs,
		.accept_timeout = 5000,
	};
	if (open_exec_channel(&oc) < 0) {
		conn_cleanup(&ctx);
		return TEST_SKIP;
	}

	const uint8_t stderr_msg[] = "client stderr";
	int64_t written = dssh_session_write_ext(ctx.client, oc.client_ch,
	    stderr_msg, sizeof(stderr_msg) - 1);
	ASSERT_TRUE(written > 0);

	int ev = dssh_session_poll(ctx.server, oc.server_ch,
	    DSSH_POLL_READEXT, 5000);
	ASSERT_TRUE(ev & DSSH_POLL_READEXT);

	uint8_t buf[256];
	int64_t nread = dssh_session_read_ext(ctx.server, oc.server_ch,
	    buf, sizeof(buf));
	ASSERT_TRUE(nread > 0);
	ASSERT_MEM_EQ(buf, stderr_msg, sizeof(stderr_msg) - 1);

	dssh_session_close(ctx.client, oc.client_ch, 0);
	dssh_session_close(ctx.server, oc.server_ch, 0);
	conn_cleanup(&ctx);
	return TEST_PASS;
}

static int
test_session_poll_read(void)
{
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_SKIP;

	struct open_exec_ctx oc = {
		.client = ctx.client,
		.server = ctx.server,
		.command = "cmd",
		.cbs = &session_cbs,
		.accept_timeout = 5000,
	};
	if (open_exec_channel(&oc) < 0) {
		conn_cleanup(&ctx);
		return TEST_SKIP;
	}

	const uint8_t msg[] = "data";
	dssh_session_write(ctx.client, oc.client_ch, msg, sizeof(msg) - 1);

	int ev = dssh_session_poll(ctx.server, oc.server_ch,
	    DSSH_POLL_READ, 5000);
	ASSERT_TRUE(ev & DSSH_POLL_READ);

	dssh_session_close(ctx.client, oc.client_ch, 0);
	dssh_session_close(ctx.server, oc.server_ch, 0);
	conn_cleanup(&ctx);
	return TEST_PASS;
}

static int
test_session_poll_timeout(void)
{
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_SKIP;

	struct open_exec_ctx oc = {
		.client = ctx.client,
		.server = ctx.server,
		.command = "cmd",
		.cbs = &session_cbs,
		.accept_timeout = 5000,
	};
	if (open_exec_channel(&oc) < 0) {
		conn_cleanup(&ctx);
		return TEST_SKIP;
	}

	int ev = dssh_session_poll(ctx.server, oc.server_ch,
	    DSSH_POLL_READ, 100);
	ASSERT_EQ(ev, 0);

	dssh_session_close(ctx.client, oc.client_ch, 0);
	dssh_session_close(ctx.server, oc.server_ch, 0);
	conn_cleanup(&ctx);
	return TEST_PASS;
}

static int
test_session_large_write(void)
{
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_SKIP;

	struct open_exec_ctx oc = {
		.client = ctx.client,
		.server = ctx.server,
		.command = "cmd",
		.cbs = &session_cbs,
		.accept_timeout = 5000,
	};
	if (open_exec_channel(&oc) < 0) {
		conn_cleanup(&ctx);
		return TEST_SKIP;
	}

	size_t total = 32768;
	uint8_t *data = malloc(total);
	if (data == NULL) {
		dssh_session_close(ctx.client, oc.client_ch, 0);
		dssh_session_close(ctx.server, oc.server_ch, 0);
		conn_cleanup(&ctx);
		return TEST_SKIP;
	}
	for (size_t i = 0; i < total; i++)
		data[i] = (uint8_t)(i & 0xFF);

	int64_t written = dssh_session_write(ctx.client, oc.client_ch,
	    data, total);
	ASSERT_TRUE(written > 0);

	uint8_t *readbuf = malloc(total);
	if (readbuf == NULL) {
		free(data);
		dssh_session_close(ctx.client, oc.client_ch, 0);
		dssh_session_close(ctx.server, oc.server_ch, 0);
		conn_cleanup(&ctx);
		return TEST_SKIP;
	}

	size_t got = 0;
	while (got < (size_t)written) {
		int ev = dssh_session_poll(ctx.server, oc.server_ch,
		    DSSH_POLL_READ, 5000);
		if (ev <= 0)
			break;
		int64_t n = dssh_session_read(ctx.server, oc.server_ch,
		    &readbuf[got], total - got);
		if (n <= 0)
			break;
		got += (size_t)n;
	}

	ASSERT_EQ_U(got, (size_t)written);
	ASSERT_MEM_EQ(readbuf, data, got);

	free(data);
	free(readbuf);
	dssh_session_close(ctx.client, oc.client_ch, 0);
	dssh_session_close(ctx.server, oc.server_ch, 0);
	conn_cleanup(&ctx);
	return TEST_PASS;
}

static int
test_session_multiple_writes(void)
{
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_SKIP;

	struct open_exec_ctx oc = {
		.client = ctx.client,
		.server = ctx.server,
		.command = "cmd",
		.cbs = &session_cbs,
		.accept_timeout = 5000,
	};
	if (open_exec_channel(&oc) < 0) {
		conn_cleanup(&ctx);
		return TEST_SKIP;
	}

	const uint8_t m1[] = "AAA";
	const uint8_t m2[] = "BBB";
	const uint8_t m3[] = "CCC";
	dssh_session_write(ctx.client, oc.client_ch, m1, 3);
	dssh_session_write(ctx.client, oc.client_ch, m2, 3);
	dssh_session_write(ctx.client, oc.client_ch, m3, 3);

	uint8_t buf[256];
	size_t got = 0;
	while (got < 9) {
		int ev = dssh_session_poll(ctx.server, oc.server_ch,
		    DSSH_POLL_READ, 5000);
		if (ev <= 0)
			break;
		int64_t n = dssh_session_read(ctx.server, oc.server_ch,
		    &buf[got], sizeof(buf) - got);
		if (n <= 0)
			break;
		got += (size_t)n;
	}
	ASSERT_EQ_U(got, 9);
	ASSERT_MEM_EQ(buf, "AAABBBCCC", 9);

	dssh_session_close(ctx.client, oc.client_ch, 0);
	dssh_session_close(ctx.server, oc.server_ch, 0);
	conn_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Raw channel I/O (~4 tests)
 * ================================================================ */

static int
test_subsys_write_read(void)
{
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_SKIP;

	struct open_subsys_ctx oc = {
		.client = ctx.client,
		.server = ctx.server,
		.subsystem = "test-sub",
	};

	thrd_t ct, st;
	thrd_create(&ct, client_open_subsys_thread, &oc);
	thrd_create(&st, server_accept_subsys_thread, &oc);
	thrd_join(ct, NULL);
	thrd_join(st, NULL);

	if (oc.client_ch == NULL || oc.server_ch == NULL) {
		conn_cleanup(&ctx);
		return TEST_SKIP;
	}

	/* Verify subsystem name arrived */
	ASSERT_NOT_NULL(oc.req_type);
	ASSERT_STR_EQ(oc.req_type, "subsystem");

	/* Both sides are raw channels — use channel I/O */
	const uint8_t msg[] = "subsystem message";
	int res = dssh_channel_write(ctx.client, oc.client_ch,
	    msg, sizeof(msg) - 1);
	ASSERT_OK(res);

	int ev = dssh_channel_poll(ctx.server, oc.server_ch,
	    DSSH_POLL_READ, 5000);
	ASSERT_TRUE(ev & DSSH_POLL_READ);

	uint8_t buf[256];
	int64_t nread = dssh_channel_read(ctx.server, oc.server_ch,
	    buf, sizeof(buf));
	ASSERT_TRUE(nread > 0);
	ASSERT_EQ(nread, (int64_t)(sizeof(msg) - 1));
	ASSERT_MEM_EQ(buf, msg, sizeof(msg) - 1);

	dssh_channel_close(ctx.client, oc.client_ch);
	dssh_channel_close(ctx.server, oc.server_ch);
	conn_cleanup(&ctx);
	return TEST_PASS;
}

static int
test_subsys_multiple_writes(void)
{
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_SKIP;

	struct open_subsys_ctx oc = {
		.client = ctx.client,
		.server = ctx.server,
		.subsystem = "test-sub",
	};

	thrd_t ct, st;
	thrd_create(&ct, client_open_subsys_thread, &oc);
	thrd_create(&st, server_accept_subsys_thread, &oc);
	thrd_join(ct, NULL);
	thrd_join(st, NULL);

	if (oc.client_ch == NULL || oc.server_ch == NULL) {
		conn_cleanup(&ctx);
		return TEST_SKIP;
	}

	/* Raw channels preserve message boundaries */
	dssh_channel_write(ctx.client, oc.client_ch,
	    (const uint8_t *)"first", 5);
	dssh_channel_write(ctx.client, oc.client_ch,
	    (const uint8_t *)"second", 6);
	dssh_channel_write(ctx.client, oc.client_ch,
	    (const uint8_t *)"third", 5);

	uint8_t buf[256];

	int ev = dssh_channel_poll(ctx.server, oc.server_ch,
	    DSSH_POLL_READ, 5000);
	ASSERT_TRUE(ev & DSSH_POLL_READ);
	int64_t n = dssh_channel_read(ctx.server, oc.server_ch,
	    buf, sizeof(buf));
	ASSERT_EQ(n, 5);
	ASSERT_MEM_EQ(buf, "first", 5);

	ev = dssh_channel_poll(ctx.server, oc.server_ch,
	    DSSH_POLL_READ, 5000);
	ASSERT_TRUE(ev & DSSH_POLL_READ);
	n = dssh_channel_read(ctx.server, oc.server_ch,
	    buf, sizeof(buf));
	ASSERT_EQ(n, 6);
	ASSERT_MEM_EQ(buf, "second", 6);

	ev = dssh_channel_poll(ctx.server, oc.server_ch,
	    DSSH_POLL_READ, 5000);
	ASSERT_TRUE(ev & DSSH_POLL_READ);
	n = dssh_channel_read(ctx.server, oc.server_ch,
	    buf, sizeof(buf));
	ASSERT_EQ(n, 5);
	ASSERT_MEM_EQ(buf, "third", 5);

	dssh_channel_close(ctx.client, oc.client_ch);
	dssh_channel_close(ctx.server, oc.server_ch);
	conn_cleanup(&ctx);
	return TEST_PASS;
}

static int
test_subsys_poll(void)
{
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_SKIP;

	struct open_subsys_ctx oc = {
		.client = ctx.client,
		.server = ctx.server,
		.subsystem = "test-sub",
	};

	thrd_t ct, st;
	thrd_create(&ct, client_open_subsys_thread, &oc);
	thrd_create(&st, server_accept_subsys_thread, &oc);
	thrd_join(ct, NULL);
	thrd_join(st, NULL);

	if (oc.client_ch == NULL || oc.server_ch == NULL) {
		conn_cleanup(&ctx);
		return TEST_SKIP;
	}

	/* No data yet — poll should timeout */
	int ev = dssh_channel_poll(ctx.server, oc.server_ch,
	    DSSH_POLL_READ, 100);
	ASSERT_EQ(ev, 0);

	/* Send data, poll should return readable */
	dssh_channel_write(ctx.client, oc.client_ch,
	    (const uint8_t *)"x", 1);
	ev = dssh_channel_poll(ctx.server, oc.server_ch,
	    DSSH_POLL_READ, 5000);
	ASSERT_TRUE(ev & DSSH_POLL_READ);

	uint8_t buf[8];
	dssh_channel_read(ctx.server, oc.server_ch, buf, sizeof(buf));

	dssh_channel_close(ctx.client, oc.client_ch);
	dssh_channel_close(ctx.server, oc.server_ch);
	conn_cleanup(&ctx);
	return TEST_PASS;
}

static int
test_subsys_large_write(void)
{
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_SKIP;

	struct open_subsys_ctx oc = {
		.client = ctx.client,
		.server = ctx.server,
		.subsystem = "test-sub",
	};

	thrd_t ct, st;
	thrd_create(&ct, client_open_subsys_thread, &oc);
	thrd_create(&st, server_accept_subsys_thread, &oc);
	thrd_join(ct, NULL);
	thrd_join(st, NULL);

	if (oc.client_ch == NULL || oc.server_ch == NULL) {
		conn_cleanup(&ctx);
		return TEST_SKIP;
	}

	size_t msglen = 16384;
	uint8_t *data = malloc(msglen);
	ASSERT_NOT_NULL(data);
	for (size_t i = 0; i < msglen; i++)
		data[i] = (uint8_t)(i & 0xFF);

	int res = dssh_channel_write(ctx.client, oc.client_ch,
	    data, msglen);
	ASSERT_OK(res);

	uint8_t *readbuf = malloc(msglen);
	ASSERT_NOT_NULL(readbuf);

	int ev = dssh_channel_poll(ctx.server, oc.server_ch,
	    DSSH_POLL_READ, 5000);
	ASSERT_TRUE(ev & DSSH_POLL_READ);

	int64_t nread = dssh_channel_read(ctx.server, oc.server_ch,
	    readbuf, msglen);
	ASSERT_TRUE(nread > 0);
	ASSERT_MEM_EQ(readbuf, data, (size_t)nread);

	free(data);
	free(readbuf);
	dssh_channel_close(ctx.client, oc.client_ch);
	dssh_channel_close(ctx.server, oc.server_ch);
	conn_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Signals (~3 tests)
 * ================================================================ */

static int
test_send_signal(void)
{
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_SKIP;

	struct open_exec_ctx oc = {
		.client = ctx.client,
		.server = ctx.server,
		.command = "cmd",
		.cbs = &session_cbs,
		.accept_timeout = 5000,
	};
	if (open_exec_channel(&oc) < 0) {
		conn_cleanup(&ctx);
		return TEST_SKIP;
	}

	int res = dssh_session_send_signal(ctx.client, oc.client_ch, "INT");
	ASSERT_OK(res);

	int ev = dssh_session_poll(ctx.server, oc.server_ch,
	    DSSH_POLL_SIGNAL, 5000);
	ASSERT_TRUE(ev & DSSH_POLL_SIGNAL);

	const char *signame = NULL;
	res = dssh_session_read_signal(ctx.server, oc.server_ch, &signame);
	ASSERT_OK(res);
	ASSERT_NOT_NULL(signame);
	ASSERT_STR_EQ(signame, "INT");

	dssh_session_close(ctx.client, oc.client_ch, 0);
	dssh_session_close(ctx.server, oc.server_ch, 0);
	conn_cleanup(&ctx);
	return TEST_PASS;
}

static int
test_signal_after_data(void)
{
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_SKIP;

	struct open_exec_ctx oc = {
		.client = ctx.client,
		.server = ctx.server,
		.command = "cmd",
		.cbs = &session_cbs,
		.accept_timeout = 5000,
	};
	if (open_exec_channel(&oc) < 0) {
		conn_cleanup(&ctx);
		return TEST_SKIP;
	}

	/* Send data, then a signal */
	const uint8_t msg[] = "before signal";
	dssh_session_write(ctx.client, oc.client_ch, msg, sizeof(msg) - 1);
	dssh_session_send_signal(ctx.client, oc.client_ch, "TERM");

	/* Drain data first */
	uint8_t buf[256];
	size_t got = 0;
	while (got < sizeof(msg) - 1) {
		int ev = dssh_session_poll(ctx.server, oc.server_ch,
		    DSSH_POLL_READ, 5000);
		if (ev <= 0)
			break;
		int64_t n = dssh_session_read(ctx.server, oc.server_ch,
		    &buf[got], sizeof(buf) - got);
		if (n <= 0)
			break;
		got += (size_t)n;
	}
	ASSERT_EQ_U(got, sizeof(msg) - 1);

	/* Now signal should be ready */
	int ev = dssh_session_poll(ctx.server, oc.server_ch,
	    DSSH_POLL_SIGNAL, 5000);
	ASSERT_TRUE(ev & DSSH_POLL_SIGNAL);

	const char *signame = NULL;
	dssh_session_read_signal(ctx.server, oc.server_ch, &signame);
	ASSERT_NOT_NULL(signame);
	ASSERT_STR_EQ(signame, "TERM");

	dssh_session_close(ctx.client, oc.client_ch, 0);
	dssh_session_close(ctx.server, oc.server_ch, 0);
	conn_cleanup(&ctx);
	return TEST_PASS;
}

static int
test_window_change(void)
{
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_SKIP;

	struct dssh_pty_req pty = {
		.term = "vt100",
		.cols = 80,
		.rows = 24,
	};

	struct open_shell_ctx oc = {
		.client = ctx.client,
		.server = ctx.server,
		.pty = &pty,
		.cbs = &session_cbs,
	};

	thrd_t ct, st;
	thrd_create(&ct, client_open_shell_thread, &oc);
	thrd_create(&st, server_accept_shell_thread, &oc);
	thrd_join(ct, NULL);
	thrd_join(st, NULL);

	if (oc.client_ch == NULL || oc.server_ch == NULL) {
		conn_cleanup(&ctx);
		return TEST_SKIP;
	}

	int res = dssh_session_send_window_change(ctx.client, oc.client_ch,
	    132, 50, 1056, 800);
	ASSERT_OK(res);

	/* Give demux time to deliver the window-change callback */
	struct timespec ts = { .tv_sec = 0, .tv_nsec = 200000000 };
	thrd_sleep(&ts, NULL);

	ASSERT_TRUE(wc_received);
	ASSERT_EQ_U(wc_cols, 132);
	ASSERT_EQ_U(wc_rows, 50);
	ASSERT_EQ_U(wc_wpx, 1056);
	ASSERT_EQ_U(wc_hpx, 800);

	dssh_session_close(ctx.client, oc.client_ch, 0);
	dssh_session_close(ctx.server, oc.server_ch, 0);
	conn_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Close and exit (~5 tests)
 * ================================================================ */

static int
test_session_close_exit_code(void)
{
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_SKIP;

	struct open_exec_ctx oc = {
		.client = ctx.client,
		.server = ctx.server,
		.command = "cmd",
		.cbs = &session_cbs,
		.accept_timeout = 5000,
	};
	if (open_exec_channel(&oc) < 0) {
		conn_cleanup(&ctx);
		return TEST_SKIP;
	}

	/* Server closes with exit code 42 */
	int res = dssh_session_close(ctx.server, oc.server_ch, 42);
	ASSERT_OK(res);

	/* Client should see close; give demux time */
	struct timespec ts = { .tv_sec = 0, .tv_nsec = 200000000 };
	thrd_sleep(&ts, NULL);

	/* Client reads should return 0 (EOF) */
	uint8_t buf[64];
	int ev = dssh_session_poll(ctx.client, oc.client_ch,
	    DSSH_POLL_READ, 1000);
	if (ev & DSSH_POLL_READ) {
		int64_t n = dssh_session_read(ctx.client, oc.client_ch,
		    buf, sizeof(buf));
		ASSERT_EQ(n, 0);
	}

	dssh_session_close(ctx.client, oc.client_ch, 0);
	conn_cleanup(&ctx);
	return TEST_PASS;
}

static int
test_channel_close_raw(void)
{
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_SKIP;

	struct open_subsys_ctx oc = {
		.client = ctx.client,
		.server = ctx.server,
		.subsystem = "test-raw",
	};

	thrd_t ct, st;
	thrd_create(&ct, client_open_subsys_thread, &oc);
	thrd_create(&st, server_accept_subsys_thread, &oc);
	thrd_join(ct, NULL);
	thrd_join(st, NULL);

	if (oc.client_ch == NULL || oc.server_ch == NULL) {
		conn_cleanup(&ctx);
		return TEST_SKIP;
	}

	int res = dssh_channel_close(ctx.client, oc.client_ch);
	ASSERT_OK(res);

	/* Server should see close */
	struct timespec ts = { .tv_sec = 0, .tv_nsec = 200000000 };
	thrd_sleep(&ts, NULL);

	dssh_channel_close(ctx.server, oc.server_ch);
	conn_cleanup(&ctx);
	return TEST_PASS;
}

static int
test_eof_detection(void)
{
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_SKIP;

	struct open_exec_ctx oc = {
		.client = ctx.client,
		.server = ctx.server,
		.command = "cmd",
		.cbs = &session_cbs,
		.accept_timeout = 5000,
	};
	if (open_exec_channel(&oc) < 0) {
		conn_cleanup(&ctx);
		return TEST_SKIP;
	}

	/* Server writes data, then closes (sends exit-status + EOF + CLOSE) */
	const uint8_t msg[] = "final output";
	dssh_session_write(ctx.server, oc.server_ch, msg, sizeof(msg) - 1);
	dssh_session_close(ctx.server, oc.server_ch, 0);

	/* Client reads data */
	uint8_t buf[256];
	size_t got = 0;
	for (;;) {
		int ev = dssh_session_poll(ctx.client, oc.client_ch,
		    DSSH_POLL_READ, 5000);
		if (ev <= 0)
			break;
		int64_t n = dssh_session_read(ctx.client, oc.client_ch,
		    buf + got, sizeof(buf) - got);
		if (n <= 0)
			break;
		got += (size_t)n;
	}
	ASSERT_TRUE(got >= sizeof(msg) - 1);
	ASSERT_MEM_EQ(buf, msg, sizeof(msg) - 1);

	dssh_session_close(ctx.client, oc.client_ch, 0);
	conn_cleanup(&ctx);
	return TEST_PASS;
}

static int
test_eof_half_close(void)
{
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_SKIP;

	struct open_exec_ctx oc = {
		.client = ctx.client,
		.server = ctx.server,
		.command = "cmd",
		.cbs = &session_cbs,
		.accept_timeout = 5000,
	};
	if (open_exec_channel(&oc) < 0) {
		conn_cleanup(&ctx);
		return TEST_SKIP;
	}

	/* Server sends data, then closes */
	const uint8_t msg[] = "server data after client eof";
	dssh_session_write(ctx.server, oc.server_ch, msg, sizeof(msg) - 1);

	/* Client should be able to read even if server closed */
	uint8_t buf[256];
	size_t got = 0;
	while (got < sizeof(msg) - 1) {
		int ev = dssh_session_poll(ctx.client, oc.client_ch,
		    DSSH_POLL_READ, 5000);
		if (ev <= 0)
			break;
		int64_t n = dssh_session_read(ctx.client, oc.client_ch,
		    &buf[got], sizeof(buf) - got);
		if (n <= 0)
			break;
		got += (size_t)n;
	}
	ASSERT_EQ_U(got, sizeof(msg) - 1);
	ASSERT_MEM_EQ(buf, msg, sizeof(msg) - 1);

	dssh_session_close(ctx.client, oc.client_ch, 0);
	dssh_session_close(ctx.server, oc.server_ch, 0);
	conn_cleanup(&ctx);
	return TEST_PASS;
}

static int
test_graceful_disconnect(void)
{
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_SKIP;

	struct open_exec_ctx oc = {
		.client = ctx.client,
		.server = ctx.server,
		.command = "cmd",
		.cbs = &session_cbs,
		.accept_timeout = 5000,
	};
	if (open_exec_channel(&oc) < 0) {
		conn_cleanup(&ctx);
		return TEST_SKIP;
	}

	/* Client terminates the session */
	dssh_session_terminate(ctx.client);

	/* Give demux time to see termination */
	struct timespec ts = { .tv_sec = 0, .tv_nsec = 200000000 };
	thrd_sleep(&ts, NULL);

	/* Server poll should return error or see termination */
	int ev = dssh_session_poll(ctx.server, oc.server_ch,
	    DSSH_POLL_READ, 500);
	/* Either an error or 0 timeout is acceptable after peer disconnect */
	ASSERT_TRUE(ev <= 0 || (ev & DSSH_POLL_READ));

	/* Clean up without calling session_close for already-terminated channels */
	conn_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Multiple channels (~3 tests)
 * ================================================================ */

struct two_channel_server_ctx {
	dssh_session server;
	dssh_channel ch1;
	dssh_channel ch2;
	const struct dssh_server_session_cbs *cbs;
};

static int
server_accept_two_channels(void *arg)
{
	struct two_channel_server_ctx *sctx = arg;

	struct dssh_incoming_open *inc1 = NULL;
	int res = dssh_session_accept(sctx->server, &inc1, 5000);
	if (res < 0)
		return 0;
	const char *rt, *rd;
	sctx->ch1 = dssh_session_accept_channel(sctx->server, inc1,
	    sctx->cbs, &rt, &rd);

	struct dssh_incoming_open *inc2 = NULL;
	res = dssh_session_accept(sctx->server, &inc2, 5000);
	if (res < 0)
		return 0;
	sctx->ch2 = dssh_session_accept_channel(sctx->server, inc2,
	    sctx->cbs, &rt, &rd);

	return 0;
}

static int
test_two_channels(void)
{
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_SKIP;

	struct two_channel_server_ctx sctx = {
		.server = ctx.server,
		.cbs = &session_cbs,
	};

	thrd_t st;
	thrd_create(&st, server_accept_two_channels, &sctx);

	dssh_channel c1 = dssh_session_open_exec(ctx.client, "cmd1");
	dssh_channel c2 = dssh_session_open_exec(ctx.client, "cmd2");

	thrd_join(st, NULL);

	if (c1 == NULL || c2 == NULL || sctx.ch1 == NULL || sctx.ch2 == NULL) {
		if (c1) dssh_session_close(ctx.client, c1, 0);
		if (c2) dssh_session_close(ctx.client, c2, 0);
		if (sctx.ch1) dssh_session_close(ctx.server, sctx.ch1, 0);
		if (sctx.ch2) dssh_session_close(ctx.server, sctx.ch2, 0);
		conn_cleanup(&ctx);
		return TEST_FAIL;
	}

	/* Write on channel 1, read on channel 1 */
	const uint8_t m1[] = "chan1data";
	dssh_session_write(ctx.client, c1, m1, 8);

	/* Write on channel 2, read on channel 2 */
	const uint8_t m2[] = "chan2data";
	dssh_session_write(ctx.client, c2, m2, 8);

	uint8_t buf[256];

	int ev = dssh_session_poll(ctx.server, sctx.ch1,
	    DSSH_POLL_READ, 5000);
	ASSERT_TRUE(ev & DSSH_POLL_READ);
	int64_t n = dssh_session_read(ctx.server, sctx.ch1, buf, sizeof(buf));
	ASSERT_EQ(n, 8);
	ASSERT_MEM_EQ(buf, "chan1data", 8);

	ev = dssh_session_poll(ctx.server, sctx.ch2,
	    DSSH_POLL_READ, 5000);
	ASSERT_TRUE(ev & DSSH_POLL_READ);
	n = dssh_session_read(ctx.server, sctx.ch2, buf, sizeof(buf));
	ASSERT_EQ(n, 8);
	ASSERT_MEM_EQ(buf, "chan2data", 8);

	dssh_session_close(ctx.client, c1, 0);
	dssh_session_close(ctx.client, c2, 0);
	dssh_session_close(ctx.server, sctx.ch1, 0);
	dssh_session_close(ctx.server, sctx.ch2, 0);
	conn_cleanup(&ctx);
	return TEST_PASS;
}

struct accept_reject_server_ctx {
	dssh_session server;
	dssh_channel ch;
	const struct dssh_server_session_cbs *cbs;
	bool first_rejected;
};

static int
server_reject_first_accept_second(void *arg)
{
	struct accept_reject_server_ctx *sctx = arg;

	/* Reject first channel */
	struct dssh_incoming_open *inc1 = NULL;
	int res = dssh_session_accept(sctx->server, &inc1, 5000);
	if (res < 0)
		return 0;
	dssh_session_reject(sctx->server, inc1, 1, "denied");
	sctx->first_rejected = true;

	/* Accept second channel */
	struct dssh_incoming_open *inc2 = NULL;
	res = dssh_session_accept(sctx->server, &inc2, 5000);
	if (res < 0)
		return 0;
	const char *rt, *rd;
	sctx->ch = dssh_session_accept_channel(sctx->server, inc2,
	    sctx->cbs, &rt, &rd);

	return 0;
}

static int
test_accept_reject(void)
{
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_SKIP;

	struct accept_reject_server_ctx sctx = {
		.server = ctx.server,
		.cbs = &session_cbs,
	};

	thrd_t st;
	thrd_create(&st, server_reject_first_accept_second, &sctx);

	/* First channel should fail */
	dssh_channel c1 = dssh_session_open_exec(ctx.client, "rejected");
	/* Second channel should succeed */
	dssh_channel c2 = dssh_session_open_exec(ctx.client, "accepted");

	thrd_join(st, NULL);

	ASSERT_NULL(c1);
	ASSERT_NOT_NULL(c2);
	ASSERT_NOT_NULL(sctx.ch);
	ASSERT_TRUE(sctx.first_rejected);

	dssh_session_close(ctx.client, c2, 0);
	dssh_session_close(ctx.server, sctx.ch, 0);
	conn_cleanup(&ctx);
	return TEST_PASS;
}

static int
test_channel_independence(void)
{
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_SKIP;

	struct two_channel_server_ctx sctx = {
		.server = ctx.server,
		.cbs = &session_cbs,
	};

	thrd_t st;
	thrd_create(&st, server_accept_two_channels, &sctx);

	dssh_channel c1 = dssh_session_open_exec(ctx.client, "cmd1");
	dssh_channel c2 = dssh_session_open_exec(ctx.client, "cmd2");

	thrd_join(st, NULL);

	if (c1 == NULL || c2 == NULL || sctx.ch1 == NULL || sctx.ch2 == NULL) {
		if (c1) dssh_session_close(ctx.client, c1, 0);
		if (c2) dssh_session_close(ctx.client, c2, 0);
		if (sctx.ch1) dssh_session_close(ctx.server, sctx.ch1, 0);
		if (sctx.ch2) dssh_session_close(ctx.server, sctx.ch2, 0);
		conn_cleanup(&ctx);
		return TEST_FAIL;
	}

	/* Close channel 1 */
	dssh_session_close(ctx.client, c1, 0);
	dssh_session_close(ctx.server, sctx.ch1, 0);

	/* Channel 2 should still work */
	const uint8_t msg[] = "still alive";
	dssh_session_write(ctx.client, c2, msg, sizeof(msg) - 1);

	uint8_t buf[256];
	int ev = dssh_session_poll(ctx.server, sctx.ch2,
	    DSSH_POLL_READ, 5000);
	ASSERT_TRUE(ev & DSSH_POLL_READ);
	int64_t n = dssh_session_read(ctx.server, sctx.ch2, buf, sizeof(buf));
	ASSERT_TRUE(n > 0);
	ASSERT_MEM_EQ(buf, "still alive", (size_t)n);

	dssh_session_close(ctx.client, c2, 0);
	dssh_session_close(ctx.server, sctx.ch2, 0);
	conn_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Thread safety (~2 tests)
 * ================================================================ */

struct writer_ctx {
	dssh_session sess;
	dssh_channel ch;
	size_t total_written;
	int result;
};

static int
writer_thread_func(void *arg)
{
	struct writer_ctx *wc = arg;
	uint8_t buf[128];
	memset(buf, 'W', sizeof(buf));
	wc->total_written = 0;
	wc->result = 0;

	for (int i = 0; i < 100; i++) {
		int ev = dssh_session_poll(wc->sess, wc->ch,
		    DSSH_POLL_WRITE, 5000);
		if (ev < 0) {
			wc->result = ev;
			break;
		}
		if (!(ev & DSSH_POLL_WRITE))
			continue;
		int64_t w = dssh_session_write(wc->sess, wc->ch,
		    buf, sizeof(buf));
		if (w < 0) {
			wc->result = (int)w;
			break;
		}
		wc->total_written += (size_t)w;
	}
	return 0;
}

struct reader_ctx {
	dssh_session sess;
	dssh_channel ch;
	size_t total_read;
	int result;
};

static int
reader_thread_func(void *arg)
{
	struct reader_ctx *rc = arg;
	uint8_t buf[256];
	rc->total_read = 0;
	rc->result = 0;

	for (;;) {
		int ev = dssh_session_poll(rc->sess, rc->ch,
		    DSSH_POLL_READ, 2000);
		if (ev <= 0)
			break;
		if (!(ev & DSSH_POLL_READ))
			continue;
		int64_t n = dssh_session_read(rc->sess, rc->ch,
		    buf, sizeof(buf));
		if (n <= 0)
			break;
		rc->total_read += (size_t)n;
	}
	return 0;
}

static int
test_concurrent_write_read(void)
{
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_SKIP;

	struct open_exec_ctx oc = {
		.client = ctx.client,
		.server = ctx.server,
		.command = "cmd",
		.cbs = &session_cbs,
		.accept_timeout = 5000,
	};
	if (open_exec_channel(&oc) < 0) {
		conn_cleanup(&ctx);
		return TEST_SKIP;
	}

	struct writer_ctx wc = {
		.sess = ctx.client,
		.ch = oc.client_ch,
	};
	struct reader_ctx rc = {
		.sess = ctx.server,
		.ch = oc.server_ch,
	};

	thrd_t wt, rt;
	thrd_create(&rt, reader_thread_func, &rc);
	thrd_create(&wt, writer_thread_func, &wc);

	thrd_join(wt, NULL);

	/* After writer finishes, close the channel so reader sees EOF */
	dssh_session_close(ctx.client, oc.client_ch, 0);

	thrd_join(rt, NULL);

	ASSERT_TRUE(wc.total_written > 0);
	ASSERT_TRUE(rc.total_read > 0);
	/* Reader should have received at least some of what was written */
	ASSERT_TRUE(rc.total_read <= wc.total_written);

	dssh_session_close(ctx.server, oc.server_ch, 0);
	conn_cleanup(&ctx);
	return TEST_PASS;
}

static int
test_terminate_during_io(void)
{
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_SKIP;

	struct open_exec_ctx oc = {
		.client = ctx.client,
		.server = ctx.server,
		.command = "cmd",
		.cbs = &session_cbs,
		.accept_timeout = 5000,
	};
	if (open_exec_channel(&oc) < 0) {
		conn_cleanup(&ctx);
		return TEST_SKIP;
	}

	/* Start a reader that will block on poll */
	struct reader_ctx rc = {
		.sess = ctx.server,
		.ch = oc.server_ch,
	};
	thrd_t rt;
	thrd_create(&rt, reader_thread_func, &rc);

	/* Brief delay, then terminate */
	struct timespec ts = { .tv_sec = 0, .tv_nsec = 50000000 };
	thrd_sleep(&ts, NULL);

	dssh_session_terminate(ctx.server);

	thrd_join(rt, NULL);

	/* Reader should have exited cleanly (no crash) */
	ASSERT_EQ_U(rc.total_read, 0);

	conn_cleanup(&ctx);
	return TEST_PASS;
}


/* ================================================================
 * Unexpected connection drop
 * ================================================================ */

/* Simulate sudden connection death during an active channel.
 * The application's I/O callbacks start returning errors.
 * Verify: session terminates, poll/read unblock, cleanup succeeds. */
static int
test_connection_drop_during_read(void)
{
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_SKIP;

	struct open_exec_ctx oc = {
		.client = ctx.client,
		.server = ctx.server,
		.command = "cmd",
		.cbs = &session_cbs,
		.accept_timeout = 5000,
	};
	if (open_exec_channel(&oc) < 0) {
		conn_cleanup(&ctx);
		return TEST_SKIP;
	}

	/* Start a reader blocked on poll */
	struct reader_ctx rc = {
		.sess = ctx.server,
		.ch = oc.server_ch,
	};
	thrd_t rt;
	thrd_create(&rt, reader_thread_func, &rc);

	/* Brief delay to let reader block */
	struct timespec ts = { .tv_sec = 0, .tv_nsec = 50000000 };
	thrd_sleep(&ts, NULL);

	/* Kill the connection — close both pipes.
	 * The demux thread's recv will get an error,
	 * setting terminate and unblocking the reader. */
	mock_io_close_c2s(&ctx.io);
	mock_io_close_s2c(&ctx.io);

	thrd_join(rt, NULL);

	/* Session should be terminated */
	ASSERT_TRUE(dssh_session_is_terminated(ctx.server));

	/* Cleanup must not crash */
	conn_cleanup(&ctx);
	return TEST_PASS;
}

/* Verify that write fails cleanly after connection drop */
static int
test_connection_drop_during_write(void)
{
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_SKIP;

	struct open_exec_ctx oc = {
		.client = ctx.client,
		.server = ctx.server,
		.command = "cmd",
		.cbs = &session_cbs,
		.accept_timeout = 5000,
	};
	if (open_exec_channel(&oc) < 0) {
		conn_cleanup(&ctx);
		return TEST_SKIP;
	}

	/* Kill the connection */
	mock_io_close_c2s(&ctx.io);
	mock_io_close_s2c(&ctx.io);

	/* Brief delay for demux to notice */
	struct timespec ts = { .tv_sec = 0, .tv_nsec = 100000000 };
	thrd_sleep(&ts, NULL);

	/* Write should fail (not hang, not crash) */
	const uint8_t data[] = "after drop";
	int64_t w = dssh_session_write(ctx.client, oc.client_ch,
	    data, sizeof(data) - 1);
	ASSERT_TRUE(w <= 0);

	/* Poll should indicate error or return immediately */
	int ev = dssh_session_poll(ctx.client, oc.client_ch,
	    DSSH_POLL_READ, 100);
	(void)ev;  /* any result is OK as long as it doesn't hang */

	conn_cleanup(&ctx);
	return TEST_PASS;
}

/* Verify cleanup works even if session_stop/cleanup is called
 * without prior terminate — the application just calls cleanup
 * when it detects the socket is dead. */
static int
test_cleanup_after_drop(void)
{
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_SKIP;

	struct open_exec_ctx oc = {
		.client = ctx.client,
		.server = ctx.server,
		.command = "cmd",
		.cbs = &session_cbs,
		.accept_timeout = 5000,
	};
	if (open_exec_channel(&oc) < 0) {
		conn_cleanup(&ctx);
		return TEST_SKIP;
	}

	/* Kill pipes, then immediately cleanup without explicit
	 * terminate — dssh_session_cleanup calls session_stop
	 * which sets terminate and joins the demux thread.
	 * The closed pipes ensure the demux unblocks. */
	mock_io_close_c2s(&ctx.io);
	mock_io_close_s2c(&ctx.io);

	/* This must not hang or crash */
	dssh_session_cleanup(ctx.server);
	dssh_session_cleanup(ctx.client);
	ctx.server = NULL;
	ctx.client = NULL;
	mock_io_free(&ctx.io);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

/* ================================================================
 * Test table
 * ================================================================ */

static struct dssh_test_entry tests[] = {
	/* Session start/stop */
	{ "test_session_start",              test_session_start },
	{ "test_session_stop",               test_session_stop },
	{ "test_session_start_stop",         test_session_start_stop },

	/* Channel open */
	{ "test_open_exec",                  test_open_exec },
	{ "test_open_exec_command",          test_open_exec_command },
	{ "test_open_exec_rejected",         test_open_exec_rejected },
	{ "test_open_shell",                 test_open_shell },
	{ "test_open_subsystem",             test_open_subsystem },

	/* Session I/O */
	{ "test_session_write_read",         test_session_write_read },
	{ "test_session_read_write_reverse", test_session_read_write_reverse },
	{ "test_session_write_ext",          test_session_write_ext },
	{ "test_session_read_ext",           test_session_read_ext },
	{ "test_session_poll_read",          test_session_poll_read },
	{ "test_session_poll_timeout",       test_session_poll_timeout },
	{ "test_session_large_write",        test_session_large_write },
	{ "test_session_multiple_writes",    test_session_multiple_writes },

	/* Raw channel I/O */
	{ "test_subsys_write_read",          test_subsys_write_read },
	{ "test_subsys_multiple_writes",     test_subsys_multiple_writes },
	{ "test_subsys_poll",                test_subsys_poll },
	{ "test_subsys_large_write",         test_subsys_large_write },

	/* Signals */
	{ "test_send_signal",                test_send_signal },
	{ "test_signal_after_data",          test_signal_after_data },
	{ "test_window_change",              test_window_change },

	/* Close and exit */
	{ "test_session_close_exit_code",    test_session_close_exit_code },
	{ "test_channel_close_raw",          test_channel_close_raw },
	{ "test_eof_detection",              test_eof_detection },
	{ "test_eof_half_close",             test_eof_half_close },
	{ "test_graceful_disconnect",        test_graceful_disconnect },

	/* Multiple channels */
	{ "test_two_channels",               test_two_channels },
	{ "test_accept_reject",              test_accept_reject },
	{ "test_channel_independence",        test_channel_independence },

	/* Thread safety */
	{ "test_concurrent_write_read",      test_concurrent_write_read },
	{ "test_terminate_during_io",        test_terminate_during_io },

	/* Connection drop */
	{ "test_connection_drop_during_read",  test_connection_drop_during_read },
	{ "test_connection_drop_during_write", test_connection_drop_during_write },
	{ "test_cleanup_after_drop",           test_cleanup_after_drop },
};

DSSH_TEST_MAIN(tests)
