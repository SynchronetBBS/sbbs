/*
 * test_conn.c -- Connection protocol tests for DeuceSSH.
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
#include "test_dhgex_provider.h"


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
 * Connection setup context -- handshake + auth + session_start
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
	if (ctx->result < 0) {
		mock_io_close_c2s(ctx->io);
		mock_io_close_s2c(ctx->io);
		return 0;
	}
	ctx->result = dssh_auth_password(ctx->sess, "testuser", "testpass",
	    NULL, NULL);
	return 0;
}

static int
server_handshake_auth_thread(void *arg)
{
	struct handshake_auth_ctx *ctx = arg;
	ctx->result = dssh_transport_handshake(ctx->sess);
	if (ctx->result < 0) {
		mock_io_close_c2s(ctx->io);
		mock_io_close_s2c(ctx->io);
		return 0;
	}
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
		mock_io_close_c2s(&ctx->io);
		mock_io_close_s2c(&ctx->io);
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
	/*
	 * Close the write-ends of both pipes first so that any demux
	 * thread blocked in read() gets EOF and can exit.  With
	 * socketpair-based mock I/O, condvar broadcasts alone cannot
	 * unblock a blocking read() -- only closing the peer fd does.
	 */
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

	/* Close pipes so demux threads' read() returns EOF */
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
 * Channel open -- exec (~5 tests)
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
	ASSERT_THRD_CREATE(&ct, client_open_exec_thread, &oc);
	ASSERT_THRD_CREATE(&st, server_accept_exec_thread, &oc);
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
	ASSERT_THRD_CREATE(&ct, client_open_shell_thread, &oc);
	ASSERT_THRD_CREATE(&st, server_accept_shell_thread, &oc);
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
	ASSERT_THRD_CREATE(&ct, client_open_subsys_thread, &oc);
	ASSERT_THRD_CREATE(&st, server_accept_subsys_thread, &oc);
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
	ASSERT_THRD_CREATE(&ct, client_open_subsys_thread, &oc);
	ASSERT_THRD_CREATE(&st, server_accept_subsys_thread, &oc);
	thrd_join(ct, NULL);
	thrd_join(st, NULL);

	if (oc.client_ch == NULL || oc.server_ch == NULL) {
		conn_cleanup(&ctx);
		return TEST_SKIP;
	}

	/* Verify subsystem name arrived */
	ASSERT_NOT_NULL(oc.req_type);
	ASSERT_STR_EQ(oc.req_type, "subsystem");

	/* Both sides are raw channels -- use channel I/O */
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
	ASSERT_THRD_CREATE(&ct, client_open_subsys_thread, &oc);
	ASSERT_THRD_CREATE(&st, server_accept_subsys_thread, &oc);
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
	ASSERT_THRD_CREATE(&ct, client_open_subsys_thread, &oc);
	ASSERT_THRD_CREATE(&st, server_accept_subsys_thread, &oc);
	thrd_join(ct, NULL);
	thrd_join(st, NULL);

	if (oc.client_ch == NULL || oc.server_ch == NULL) {
		conn_cleanup(&ctx);
		return TEST_SKIP;
	}

	/* No data yet -- poll should timeout */
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
	ASSERT_THRD_CREATE(&ct, client_open_subsys_thread, &oc);
	ASSERT_THRD_CREATE(&st, server_accept_subsys_thread, &oc);
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
	ASSERT_THRD_CREATE(&ct, client_open_shell_thread, &oc);
	ASSERT_THRD_CREATE(&st, server_accept_shell_thread, &oc);
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
	ASSERT_THRD_CREATE(&ct, client_open_subsys_thread, &oc);
	ASSERT_THRD_CREATE(&st, server_accept_subsys_thread, &oc);
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
	ASSERT_THRD_CREATE(&st, server_accept_two_channels, &sctx);

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
	ASSERT_THRD_CREATE(&st, server_reject_first_accept_second, &sctx);

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
	ASSERT_THRD_CREATE(&st, server_accept_two_channels, &sctx);

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
	ASSERT_THRD_CREATE(&rt, reader_thread_func, &rc);
	ASSERT_THRD_CREATE(&wt, writer_thread_func, &wc);

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
	ASSERT_THRD_CREATE(&rt, reader_thread_func, &rc);

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
	ASSERT_THRD_CREATE(&rt, reader_thread_func, &rc);

	/* Brief delay to let reader block */
	struct timespec ts = { .tv_sec = 0, .tv_nsec = 50000000 };
	thrd_sleep(&ts, NULL);

	/* Kill the connection -- close both pipes.
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
 * without prior terminate -- the application just calls cleanup
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
	 * terminate -- dssh_session_cleanup calls session_stop
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
 * Parse helper unit tests (Category 1)
 * ================================================================ */

static int
test_parse_pty_req_valid(void)
{
	/* Build: string "xterm" + uint32 cols + uint32 rows +
	 *        uint32 wpx + uint32 hpx + string modes */
	uint8_t data[128];
	size_t pos = 0;
	dssh_serialize_uint32(5, data, sizeof(data), &pos);
	memcpy(&data[pos], "xterm", 5);
	pos += 5;
	dssh_serialize_uint32(80, data, sizeof(data), &pos);
	dssh_serialize_uint32(24, data, sizeof(data), &pos);
	dssh_serialize_uint32(640, data, sizeof(data), &pos);
	dssh_serialize_uint32(480, data, sizeof(data), &pos);
	/* modes: 2 bytes */
	dssh_serialize_uint32(2, data, sizeof(data), &pos);
	data[pos++] = 0x01;
	data[pos++] = 0x00;

	struct dssh_pty_req pty;
	memset(&pty, 0, sizeof(pty));
	ASSERT_EQ(dssh_parse_pty_req_data(data, pos, &pty), 0);
	ASSERT_TRUE(memcmp(pty.term, "xterm", 5) == 0);
	ASSERT_EQ_U(pty.cols, 80);
	ASSERT_EQ_U(pty.rows, 24);
	ASSERT_EQ_U(pty.wpx, 640);
	ASSERT_EQ_U(pty.hpx, 480);
	ASSERT_NOT_NULL(pty.modes);
	ASSERT_EQ_U(pty.modes_len, 2);
	return TEST_PASS;
}

static int
test_parse_pty_req_no_modes(void)
{
	/* Valid pty-req but no modes string at all */
	uint8_t data[64];
	size_t pos = 0;
	dssh_serialize_uint32(4, data, sizeof(data), &pos);
	memcpy(&data[pos], "vt52", 4);
	pos += 4;
	dssh_serialize_uint32(40, data, sizeof(data), &pos);
	dssh_serialize_uint32(24, data, sizeof(data), &pos);
	dssh_serialize_uint32(0, data, sizeof(data), &pos);
	dssh_serialize_uint32(0, data, sizeof(data), &pos);
	/* No modes string at all -- exactly at the boundary */

	struct dssh_pty_req pty;
	memset(&pty, 0, sizeof(pty));
	ASSERT_EQ(dssh_parse_pty_req_data(data, pos, &pty), 0);
	ASSERT_TRUE(memcmp(pty.term, "vt52", 4) == 0);
	ASSERT_EQ_U(pty.cols, 40);
	ASSERT_EQ_U(pty.rows, 24);
	ASSERT_NULL(pty.modes);
	ASSERT_EQ_U(pty.modes_len, 0);
	return TEST_PASS;
}

static int
test_parse_pty_req_modes_truncated(void)
{
	/* modes length says 10 but data is shorter */
	uint8_t data[64];
	size_t pos = 0;
	dssh_serialize_uint32(4, data, sizeof(data), &pos);
	memcpy(&data[pos], "vt52", 4);
	pos += 4;
	dssh_serialize_uint32(80, data, sizeof(data), &pos);
	dssh_serialize_uint32(24, data, sizeof(data), &pos);
	dssh_serialize_uint32(0, data, sizeof(data), &pos);
	dssh_serialize_uint32(0, data, sizeof(data), &pos);
	dssh_serialize_uint32(10, data, sizeof(data), &pos);
	/* only 2 bytes of modes, but length says 10 */
	data[pos++] = 0x01;
	data[pos++] = 0x00;

	struct dssh_pty_req pty;
	memset(&pty, 0, sizeof(pty));
	ASSERT_EQ(dssh_parse_pty_req_data(data, pos, &pty), 0);
	/* modes should be NULL because mlen exceeds remaining data */
	ASSERT_NULL(pty.modes);
	ASSERT_EQ_U(pty.modes_len, 0);
	return TEST_PASS;
}

static int
test_parse_pty_req_truncated(void)
{
	/* Too short to even contain terminal string length + dimensions */
	uint8_t data[8];
	size_t pos = 0;
	dssh_serialize_uint32(100, data, sizeof(data), &pos);

	struct dssh_pty_req pty;
	ASSERT_EQ(dssh_parse_pty_req_data(data, pos, &pty), DSSH_ERROR_PARSE);
	return TEST_PASS;
}

static int
test_parse_pty_req_empty(void)
{
	struct dssh_pty_req pty;
	ASSERT_EQ(dssh_parse_pty_req_data(NULL, 0, &pty), DSSH_ERROR_PARSE);
	return TEST_PASS;
}

static int
test_parse_env_valid(void)
{
	uint8_t data[64];
	size_t pos = 0;
	dssh_serialize_uint32(4, data, sizeof(data), &pos);
	memcpy(&data[pos], "TERM", 4);
	pos += 4;
	dssh_serialize_uint32(5, data, sizeof(data), &pos);
	memcpy(&data[pos], "xterm", 5);
	pos += 5;

	const uint8_t *name, *value;
	size_t name_len, value_len;
	ASSERT_EQ(dssh_parse_env_data(data, pos,
	    &name, &name_len, &value, &value_len), 0);
	ASSERT_EQ_U(name_len, 4);
	ASSERT_TRUE(memcmp(name, "TERM", 4) == 0);
	ASSERT_EQ_U(value_len, 5);
	ASSERT_TRUE(memcmp(value, "xterm", 5) == 0);
	return TEST_PASS;
}

static int
test_parse_env_truncated_name(void)
{
	/* name length exceeds data */
	uint8_t data[8];
	size_t pos = 0;
	dssh_serialize_uint32(100, data, sizeof(data), &pos);

	const uint8_t *name, *value;
	size_t name_len, value_len;
	ASSERT_EQ(dssh_parse_env_data(data, pos,
	    &name, &name_len, &value, &value_len), DSSH_ERROR_PARSE);
	return TEST_PASS;
}

static int
test_parse_env_truncated_value(void)
{
	/* name OK but value length exceeds data */
	uint8_t data[16];
	size_t pos = 0;
	dssh_serialize_uint32(1, data, sizeof(data), &pos);
	data[pos++] = 'X';
	dssh_serialize_uint32(100, data, sizeof(data), &pos);

	const uint8_t *name, *value;
	size_t name_len, value_len;
	ASSERT_EQ(dssh_parse_env_data(data, pos,
	    &name, &name_len, &value, &value_len), DSSH_ERROR_PARSE);
	return TEST_PASS;
}

static int
test_parse_env_empty(void)
{
	const uint8_t *name, *value;
	size_t name_len, value_len;
	ASSERT_EQ(dssh_parse_env_data(NULL, 0,
	    &name, &name_len, &value, &value_len), DSSH_ERROR_PARSE);
	return TEST_PASS;
}

static int
test_parse_env_missing_value_header(void)
{
	/* name present but no room for value length */
	uint8_t data[8];
	size_t pos = 0;
	dssh_serialize_uint32(1, data, sizeof(data), &pos);
	data[pos++] = 'X';

	const uint8_t *name, *value;
	size_t name_len, value_len;
	ASSERT_EQ(dssh_parse_env_data(data, pos,
	    &name, &name_len, &value, &value_len), DSSH_ERROR_PARSE);
	return TEST_PASS;
}

static int
test_parse_exec_valid(void)
{
	uint8_t data[64];
	size_t pos = 0;
	dssh_serialize_uint32(7, data, sizeof(data), &pos);
	memcpy(&data[pos], "/bin/sh", 7);
	pos += 7;

	const uint8_t *command;
	size_t command_len;
	ASSERT_EQ(dssh_parse_exec_data(data, pos,
	    &command, &command_len), 0);
	ASSERT_EQ_U(command_len, 7);
	ASSERT_TRUE(memcmp(command, "/bin/sh", 7) == 0);
	return TEST_PASS;
}

static int
test_parse_exec_truncated(void)
{
	uint8_t data[8];
	size_t pos = 0;
	dssh_serialize_uint32(100, data, sizeof(data), &pos);

	const uint8_t *command;
	size_t command_len;
	ASSERT_EQ(dssh_parse_exec_data(data, pos,
	    &command, &command_len), DSSH_ERROR_PARSE);
	return TEST_PASS;
}

static int
test_parse_exec_empty(void)
{
	const uint8_t *command;
	size_t command_len;
	ASSERT_EQ(dssh_parse_exec_data(NULL, 0,
	    &command, &command_len), DSSH_ERROR_PARSE);
	return TEST_PASS;
}

static int
test_parse_exec_zero_length(void)
{
	uint8_t data[4];
	size_t pos = 0;
	dssh_serialize_uint32(0, data, sizeof(data), &pos);

	const uint8_t *command;
	size_t command_len;
	ASSERT_EQ(dssh_parse_exec_data(data, pos,
	    &command, &command_len), 0);
	ASSERT_EQ_U(command_len, 0);
	return TEST_PASS;
}

static int
test_parse_subsystem_valid(void)
{
	uint8_t data[32];
	size_t pos = 0;
	dssh_serialize_uint32(4, data, sizeof(data), &pos);
	memcpy(&data[pos], "sftp", 4);
	pos += 4;

	const uint8_t *name;
	size_t name_len;
	ASSERT_EQ(dssh_parse_subsystem_data(data, pos,
	    &name, &name_len), 0);
	ASSERT_EQ_U(name_len, 4);
	ASSERT_TRUE(memcmp(name, "sftp", 4) == 0);
	return TEST_PASS;
}

static int
test_parse_subsystem_truncated(void)
{
	uint8_t data[8];
	size_t pos = 0;
	dssh_serialize_uint32(100, data, sizeof(data), &pos);

	const uint8_t *name;
	size_t name_len;
	ASSERT_EQ(dssh_parse_subsystem_data(data, pos,
	    &name, &name_len), DSSH_ERROR_PARSE);
	return TEST_PASS;
}

static int
test_parse_subsystem_empty(void)
{
	const uint8_t *name;
	size_t name_len;
	ASSERT_EQ(dssh_parse_subsystem_data(NULL, 0,
	    &name, &name_len), DSSH_ERROR_PARSE);
	return TEST_PASS;
}

/* ================================================================
 * Coverage: session_start twice, accept timeout, reject with NULL
 * ================================================================ */

static int
test_session_start_twice(void)
{
	/* Calling session_start twice -- covers demux_running guard */
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_FAIL;

	/* Already started in conn_setup. Start again should fail. */
	int res = dssh_session_start(ctx.client);
	ASSERT_TRUE(res < 0);

	conn_cleanup(&ctx);
	return TEST_PASS;
}

static int
test_accept_timeout_zero(void)
{
	/* accept with timeout_ms=0 -- non-blocking, should return immediately */
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_FAIL;

	struct dssh_incoming_open *inc = NULL;
	int res = dssh_session_accept(ctx.server, &inc, 0);
	/* No channel open pending, so should return a timeout/error */
	ASSERT_TRUE(res < 0 || inc == NULL);

	conn_cleanup(&ctx);
	return TEST_PASS;
}

static int
test_accept_timeout_short(void)
{
	/* accept with short positive timeout -- should time out */
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_FAIL;

	struct dssh_incoming_open *inc = NULL;
	int res = dssh_session_accept(ctx.server, &inc, 50);  /* 50ms */
	ASSERT_TRUE(res < 0 || inc == NULL);

	conn_cleanup(&ctx);
	return TEST_PASS;
}

static int
test_reject_null_description(void)
{
	/* reject with NULL description -- covers the dlen==0 path */
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_FAIL;

	/* Open from client, reject on server with NULL description */
	struct open_exec_ctx oc = {
		.client = ctx.client,
		.server = ctx.server,
		.command = "echo test",
		.cbs = &session_cbs,
		.accept_timeout = 5000,
		.reject = true,
		.reject_reason = 1,
	};

	thrd_t ct;
	ASSERT_THRD_CREATE(&ct, client_open_exec_thread, &oc);

	/* Server side: accept then reject with NULL description */
	struct dssh_incoming_open *inc = NULL;
	int res = dssh_session_accept(ctx.server, &inc, 5000);
	if (res >= 0 && inc != NULL) {
		dssh_session_reject(ctx.server, inc, 1, NULL);
	}

	thrd_join(ct, NULL);
	/* Client should get NULL channel */
	ASSERT_NULL(oc.client_ch);

	conn_cleanup(&ctx);
	return TEST_PASS;
}

static int
test_session_poll_timeout_zero(void)
{
	/* session_poll with timeout_ms=0 -- non-blocking */
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_FAIL;

	struct open_exec_ctx oc = {
		.client = ctx.client,
		.server = ctx.server,
		.command = "echo polltest",
		.cbs = &session_cbs,
		.accept_timeout = 5000,
	};
	if (open_exec_channel(&oc) < 0) {
		conn_cleanup(&ctx);
		return TEST_FAIL;
	}
	if (oc.client_ch == NULL || oc.server_ch == NULL) {
		conn_cleanup(&ctx);
		return TEST_FAIL;
	}

	/* Poll with timeout=0 on client -- no data pending */
	int events = dssh_session_poll(ctx.client, oc.client_ch,
	    DSSH_POLL_READ | DSSH_POLL_WRITE, 0);
	/* Should return 0 (no events) or just DSSH_POLL_WRITE */
	ASSERT_TRUE(events >= 0);

	conn_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Auto-reject channel types -- covers demux_channel_open branches.
 *
 * Send CHANNEL_OPEN with forbidden types from server to client.
 * The client's demux thread should auto-reject with OPEN_FAILURE.
 * ================================================================ */

/* Helper: build a CHANNEL_OPEN packet */
static size_t
build_channel_open(uint8_t *buf, size_t bufsz, const char *type,
    uint32_t sender_ch, uint32_t window, uint32_t max_packet)
{
	size_t pos = 0;
	buf[pos++] = SSH_MSG_CHANNEL_OPEN;
	size_t tlen = strlen(type);
	dssh_serialize_uint32((uint32_t)tlen, buf, bufsz, &pos);
	memcpy(&buf[pos], type, tlen);
	pos += tlen;
	dssh_serialize_uint32(sender_ch, buf, bufsz, &pos);
	dssh_serialize_uint32(window, buf, bufsz, &pos);
	dssh_serialize_uint32(max_packet, buf, bufsz, &pos);
	return pos;
}

static int
test_auto_reject_x11(void)
{
	/* Server sends CHANNEL_OPEN "x11" to client.
	 * Client's demux auto-rejects; server's demux sees the
	 * OPEN_FAILURE but has no pending channel for it (harmless).
	 * Verify the client accept queue has nothing. */
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_FAIL;

	uint8_t msg[64];
	size_t len = build_channel_open(msg, sizeof(msg), "x11", 0, 65536, 32768);
	ASSERT_OK(dssh_transport_send_packet(ctx.server, msg, len, NULL));

	/* Give the demux threads time to process */

	/* Client's accept queue should be empty (auto-rejected, not queued) */
	struct dssh_incoming_open *inc = NULL;
	int res = dssh_session_accept(ctx.client, &inc, 0);
	ASSERT_TRUE(res < 0 || inc == NULL);

	conn_cleanup(&ctx);
	return TEST_PASS;
}

static int
test_auto_reject_forwarded_tcpip(void)
{
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_FAIL;

	uint8_t msg[64];
	size_t len = build_channel_open(msg, sizeof(msg), "forwarded-tcpip",
	    0, 65536, 32768);
	ASSERT_OK(dssh_transport_send_packet(ctx.server, msg, len, NULL));


	struct dssh_incoming_open *inc = NULL;
	int res = dssh_session_accept(ctx.client, &inc, 0);
	ASSERT_TRUE(res < 0 || inc == NULL);

	conn_cleanup(&ctx);
	return TEST_PASS;
}

static int
test_auto_reject_direct_tcpip(void)
{
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_FAIL;

	uint8_t msg[64];
	size_t len = build_channel_open(msg, sizeof(msg), "direct-tcpip",
	    0, 65536, 32768);
	ASSERT_OK(dssh_transport_send_packet(ctx.server, msg, len, NULL));


	struct dssh_incoming_open *inc = NULL;
	int res = dssh_session_accept(ctx.client, &inc, 0);
	ASSERT_TRUE(res < 0 || inc == NULL);

	conn_cleanup(&ctx);
	return TEST_PASS;
}

static int
test_auto_reject_session_from_server(void)
{
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_FAIL;

	uint8_t msg[64];
	size_t len = build_channel_open(msg, sizeof(msg), "session",
	    0, 65536, 32768);
	ASSERT_OK(dssh_transport_send_packet(ctx.server, msg, len, NULL));


	struct dssh_incoming_open *inc = NULL;
	int res = dssh_session_accept(ctx.client, &inc, 0);
	ASSERT_TRUE(res < 0 || inc == NULL);

	conn_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * WINDOW_ADJUST from peer -- covers demux_dispatch case
 * ================================================================ */

static int
test_window_adjust_from_peer(void)
{
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_FAIL;

	struct open_exec_ctx oc = {
		.client = ctx.client,
		.server = ctx.server,
		.command = "echo watest",
		.cbs = &session_cbs,
		.accept_timeout = 5000,
	};
	if (open_exec_channel(&oc) < 0 || oc.client_ch == NULL || oc.server_ch == NULL) {
		conn_cleanup(&ctx);
		return TEST_FAIL;
	}

	/* Server sends WINDOW_ADJUST to client's channel.
	 * Build: msg_type(1) + recipient_channel(4) + bytes_to_add(4) */
	uint8_t msg[16];
	size_t pos = 0;
	msg[pos++] = SSH_MSG_CHANNEL_WINDOW_ADJUST;
	dssh_serialize_uint32(oc.client_ch->local_id, msg, sizeof(msg), &pos);
	dssh_serialize_uint32(1000, msg, sizeof(msg), &pos);
	ASSERT_OK(dssh_transport_send_packet(ctx.server, msg, pos, NULL));

	/* Give demux time to process */

	/* Client's remote_window should have increased */
	/* (We can't easily check the exact value without reading the
	 * channel struct, but the test verifies the path doesn't crash
	 * and the session remains functional.) */

	/* Verify session still works */
	uint8_t data[] = "after window adjust";
	int64_t res = dssh_session_write(ctx.server, oc.server_ch,
	    data, sizeof(data));
	ASSERT_TRUE(res > 0);

	conn_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Data after EOF -- covers demux eof_received/close_received guards
 * ================================================================ */

static int
test_data_after_eof(void)
{
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_FAIL;

	struct open_exec_ctx oc = {
		.client = ctx.client,
		.server = ctx.server,
		.command = "echo eoftest",
		.cbs = &session_cbs,
		.accept_timeout = 5000,
	};
	if (open_exec_channel(&oc) < 0 || oc.client_ch == NULL || oc.server_ch == NULL) {
		conn_cleanup(&ctx);
		return TEST_FAIL;
	}

	/* Server sends EOF to client */
	{
		uint8_t msg[8];
		size_t pos = 0;
		msg[pos++] = SSH_MSG_CHANNEL_EOF;
		dssh_serialize_uint32(oc.client_ch->local_id, msg, sizeof(msg), &pos);
		ASSERT_OK(dssh_transport_send_packet(ctx.server, msg, pos, NULL));
	}


	/* Now server sends CHANNEL_DATA after EOF -- should be discarded */
	{
		uint8_t msg[32];
		size_t pos = 0;
		msg[pos++] = SSH_MSG_CHANNEL_DATA;
		dssh_serialize_uint32(oc.client_ch->local_id, msg, sizeof(msg), &pos);
		dssh_serialize_uint32(5, msg, sizeof(msg), &pos);
		memcpy(&msg[pos], "stale", 5);
		pos += 5;
		ASSERT_OK(dssh_transport_send_packet(ctx.server, msg, pos, NULL));
	}


	/* Client should have received nothing (data was discarded after EOF) */
	uint8_t buf[32];
	int64_t got = dssh_session_read(ctx.client, oc.client_ch, buf, sizeof(buf));
	ASSERT_EQ(got, 0);

	conn_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Truncated channel messages -- covers payload_len guards in demux
 * ================================================================ */

static int
test_truncated_channel_data(void)
{
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_FAIL;

	struct open_exec_ctx oc = {
		.client = ctx.client,
		.server = ctx.server,
		.command = "echo trunctest",
		.cbs = &session_cbs,
		.accept_timeout = 5000,
	};
	if (open_exec_channel(&oc) < 0 || oc.client_ch == NULL || oc.server_ch == NULL) {
		conn_cleanup(&ctx);
		return TEST_FAIL;
	}

	/* Send CHANNEL_DATA with only 6 bytes (needs 9 minimum:
	 * type(1) + channel(4) + data_len(4)) */
	uint8_t msg[8];
	size_t pos = 0;
	msg[pos++] = SSH_MSG_CHANNEL_DATA;
	dssh_serialize_uint32(oc.client_ch->local_id, msg, sizeof(msg), &pos);
	msg[pos++] = 0; /* partial data_len field */
	ASSERT_OK(dssh_transport_send_packet(ctx.server, msg, pos, NULL));


	/* Session should still be functional */
	uint8_t data[] = "still works";
	int64_t res = dssh_session_write(ctx.server, oc.server_ch,
	    data, sizeof(data));
	ASSERT_TRUE(res > 0);

	conn_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Extended data after EOF -- covers demux ext_data eof_received guard
 * ================================================================ */

static int
test_ext_data_after_eof(void)
{
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_FAIL;

	struct open_exec_ctx oc = {
		.client = ctx.client,
		.server = ctx.server,
		.command = "echo exteof",
		.cbs = &session_cbs,
		.accept_timeout = 5000,
	};
	if (open_exec_channel(&oc) < 0 || oc.client_ch == NULL || oc.server_ch == NULL) {
		conn_cleanup(&ctx);
		return TEST_FAIL;
	}

	/* Server sends EOF */
	{
		uint8_t msg[8];
		size_t pos = 0;
		msg[pos++] = SSH_MSG_CHANNEL_EOF;
		dssh_serialize_uint32(oc.client_ch->local_id, msg, sizeof(msg), &pos);
		ASSERT_OK(dssh_transport_send_packet(ctx.server, msg, pos, NULL));
	}


	/* Send EXTENDED_DATA (stderr) after EOF -- should be discarded */
	{
		uint8_t msg[32];
		size_t pos = 0;
		msg[pos++] = SSH_MSG_CHANNEL_EXTENDED_DATA;
		dssh_serialize_uint32(oc.client_ch->local_id, msg, sizeof(msg), &pos);
		dssh_serialize_uint32(1, msg, sizeof(msg), &pos); /* SSH_EXTENDED_DATA_STDERR */
		dssh_serialize_uint32(5, msg, sizeof(msg), &pos);
		memcpy(&msg[pos], "stale", 5);
		pos += 5;
		ASSERT_OK(dssh_transport_send_packet(ctx.server, msg, pos, NULL));
	}


	conn_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Truncated CHANNEL_EXTENDED_DATA -- covers payload_len < 13 guard
 * ================================================================ */

static int
test_truncated_ext_data(void)
{
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_FAIL;

	struct open_exec_ctx oc = {
		.client = ctx.client,
		.server = ctx.server,
		.command = "echo truncext",
		.cbs = &session_cbs,
		.accept_timeout = 5000,
	};
	if (open_exec_channel(&oc) < 0 || oc.client_ch == NULL || oc.server_ch == NULL) {
		conn_cleanup(&ctx);
		return TEST_FAIL;
	}

	/* Send EXTENDED_DATA with only 10 bytes (needs 13 minimum) */
	uint8_t msg[12];
	size_t pos = 0;
	msg[pos++] = SSH_MSG_CHANNEL_EXTENDED_DATA;
	dssh_serialize_uint32(oc.client_ch->local_id, msg, sizeof(msg), &pos);
	dssh_serialize_uint32(1, msg, sizeof(msg), &pos); /* data_type */
	/* Only 9 bytes -- no data_len or data */
	ASSERT_OK(dssh_transport_send_packet(ctx.server, msg, pos, NULL));


	/* Session should still work */
	uint8_t data[] = "ok";
	int64_t res = dssh_session_write(ctx.server, oc.server_ch, data, sizeof(data));
	ASSERT_TRUE(res > 0);

	conn_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Channel request with want_reply=true -- covers the FAILURE response
 * path in demux_dispatch for unhandled requests.
 * ================================================================ */

static int
test_channel_request_want_reply(void)
{
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_FAIL;

	struct open_exec_ctx oc = {
		.client = ctx.client,
		.server = ctx.server,
		.command = "echo reqtest",
		.cbs = &session_cbs,
		.accept_timeout = 5000,
	};
	if (open_exec_channel(&oc) < 0 || oc.client_ch == NULL || oc.server_ch == NULL) {
		conn_cleanup(&ctx);
		return TEST_FAIL;
	}

	/* Server sends an unknown channel request with want_reply=true
	 * to the client's channel. The demux should send CHANNEL_FAILURE. */
	{
		uint8_t msg[64];
		size_t pos = 0;
		msg[pos++] = SSH_MSG_CHANNEL_REQUEST;
		dssh_serialize_uint32(oc.client_ch->local_id, msg, sizeof(msg), &pos);
		dssh_serialize_uint32(7, msg, sizeof(msg), &pos);
		memcpy(&msg[pos], "unknown", 7);
		pos += 7;
		msg[pos++] = 1; /* want_reply = true */
		ASSERT_OK(dssh_transport_send_packet(ctx.server, msg, pos, NULL));
	}


	/* Session should still be functional */
	uint8_t data[] = "after request";
	int64_t res = dssh_session_write(ctx.server, oc.server_ch,
	    data, sizeof(data));
	ASSERT_TRUE(res > 0);

	conn_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Data with dlen > local_window -- covers window saturation to 0
 * ================================================================ */

static int
test_data_exceeds_window(void)
{
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_FAIL;

	struct open_exec_ctx oc = {
		.client = ctx.client,
		.server = ctx.server,
		.command = "echo wintest",
		.cbs = &session_cbs,
		.accept_timeout = 5000,
	};
	if (open_exec_channel(&oc) < 0 || oc.client_ch == NULL || oc.server_ch == NULL) {
		conn_cleanup(&ctx);
		return TEST_FAIL;
	}

	/* Send CHANNEL_DATA with a dlen larger than the local window
	 * (which is typically 2MB).  We fake the dlen field to be huge
	 * but send less actual data -- the demux clamps dlen to payload. */
	{
		uint8_t msg[32];
		size_t pos = 0;
		msg[pos++] = SSH_MSG_CHANNEL_DATA;
		dssh_serialize_uint32(oc.client_ch->local_id, msg, sizeof(msg), &pos);
		dssh_serialize_uint32(0xFFFFFFFF, msg, sizeof(msg), &pos); /* huge dlen */
		memcpy(&msg[pos], "x", 1);
		pos += 1;
		ASSERT_OK(dssh_transport_send_packet(ctx.server, msg, pos, NULL));
	}


	conn_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Coverage: session poll WRITE readiness
 * ================================================================ */

static int
test_session_poll_write(void)
{
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_SKIP;

	struct open_exec_ctx oc = {
		.client = ctx.client,
		.server = ctx.server,
		.command = "echo poll_write",
		.cbs = &session_cbs,
		.accept_timeout = 5000,
	};
	if (open_exec_channel(&oc) < 0 || oc.client_ch == NULL || oc.server_ch == NULL) {
		conn_cleanup(&ctx);
		return TEST_FAIL;
	}

	/* Fresh channel should have remote_window > 0, so WRITE is ready */
	int ev = dssh_session_poll(ctx.client, oc.client_ch,
	    DSSH_POLL_WRITE, 0);
	ASSERT_TRUE(ev & DSSH_POLL_WRITE);

	/* Also poll with timeout -- should return immediately */
	ev = dssh_session_poll(ctx.client, oc.client_ch,
	    DSSH_POLL_WRITE, 100);
	ASSERT_TRUE(ev & DSSH_POLL_WRITE);

	dssh_session_close(ctx.server, oc.server_ch, 0);
	dssh_session_close(ctx.client, oc.client_ch, 0);
	conn_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Coverage: session write edge cases
 * ================================================================ */

static int
test_session_write_after_eof(void)
{
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_SKIP;

	struct open_exec_ctx oc = {
		.client = ctx.client,
		.server = ctx.server,
		.command = "echo eof_write",
		.cbs = &session_cbs,
		.accept_timeout = 5000,
	};
	if (open_exec_channel(&oc) < 0 || oc.client_ch == NULL || oc.server_ch == NULL) {
		conn_cleanup(&ctx);
		return TEST_FAIL;
	}

	/* Mark EOF sent directly to test the write guard */
	oc.client_ch->eof_sent = true;
	int64_t w = dssh_session_write(ctx.client, oc.client_ch,
	    (const uint8_t *)"after", 5);
	ASSERT_TRUE(w < 0);

	/* Reset so close works normally */
	oc.client_ch->eof_sent = false;
	dssh_session_close(ctx.server, oc.server_ch, 0);
	dssh_session_close(ctx.client, oc.client_ch, 0);
	conn_cleanup(&ctx);
	return TEST_PASS;
}

static int
test_session_write_ext_after_eof(void)
{
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_SKIP;

	struct open_exec_ctx oc = {
		.client = ctx.client,
		.server = ctx.server,
		.command = "echo ext_eof",
		.cbs = &session_cbs,
		.accept_timeout = 5000,
	};
	if (open_exec_channel(&oc) < 0 || oc.client_ch == NULL || oc.server_ch == NULL) {
		conn_cleanup(&ctx);
		return TEST_FAIL;
	}

	/* Mark EOF sent directly to test the write_ext guard */
	oc.server_ch->eof_sent = true;
	int64_t w = dssh_session_write_ext(ctx.server, oc.server_ch,
	    (const uint8_t *)"after", 5);
	ASSERT_TRUE(w < 0);

	oc.server_ch->eof_sent = false;
	dssh_session_close(ctx.server, oc.server_ch, 0);
	dssh_session_close(ctx.client, oc.client_ch, 0);
	conn_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Coverage: raw channel write edge cases
 * ================================================================ */

static int
test_raw_channel_write_after_close(void)
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
	ASSERT_THRD_CREATE(&ct, client_open_subsys_thread, &oc);
	ASSERT_THRD_CREATE(&st, server_accept_subsys_thread, &oc);
	thrd_join(ct, NULL);
	thrd_join(st, NULL);

	if (oc.client_ch == NULL || oc.server_ch == NULL) {
		conn_cleanup(&ctx);
		return TEST_SKIP;
	}

	/* Mark EOF sent, then try to write from server */
	oc.server_ch->eof_sent = true;
	int res = dssh_channel_write(ctx.server, oc.server_ch,
	    (const uint8_t *)"after", 5);
	ASSERT_TRUE(res < 0);

	oc.server_ch->eof_sent = false;
	dssh_channel_close(ctx.client, oc.client_ch);
	dssh_channel_close(ctx.server, oc.server_ch);
	conn_cleanup(&ctx);
	return TEST_PASS;
}

static int
test_raw_channel_write_toolong(void)
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
	ASSERT_THRD_CREATE(&ct, client_open_subsys_thread, &oc);
	ASSERT_THRD_CREATE(&st, server_accept_subsys_thread, &oc);
	thrd_join(ct, NULL);
	thrd_join(st, NULL);

	if (oc.client_ch == NULL || oc.server_ch == NULL) {
		conn_cleanup(&ctx);
		return TEST_SKIP;
	}

	/* Allocate a buffer larger than remote_max_packet */
	size_t too_big = oc.client_ch->remote_max_packet + 1;
	uint8_t *buf = malloc(too_big);
	ASSERT_NOT_NULL(buf);
	memset(buf, 'X', too_big);

	int res = dssh_channel_write(ctx.client, oc.client_ch, buf, too_big);
	ASSERT_EQ(res, DSSH_ERROR_TOOLONG);
	free(buf);

	dssh_channel_close(ctx.client, oc.client_ch);
	dssh_channel_close(ctx.server, oc.server_ch);
	conn_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Coverage: raw channel poll edge cases
 * ================================================================ */

static int
test_raw_channel_poll_write(void)
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
	ASSERT_THRD_CREATE(&ct, client_open_subsys_thread, &oc);
	ASSERT_THRD_CREATE(&st, server_accept_subsys_thread, &oc);
	thrd_join(ct, NULL);
	thrd_join(st, NULL);

	if (oc.client_ch == NULL || oc.server_ch == NULL) {
		conn_cleanup(&ctx);
		return TEST_SKIP;
	}

	/* Fresh channel should have WRITE readiness */
	int ev = dssh_channel_poll(ctx.client, oc.client_ch,
	    DSSH_POLL_WRITE, 0);
	ASSERT_TRUE(ev & DSSH_POLL_WRITE);

	/* Poll with timeout -- should return immediately */
	ev = dssh_channel_poll(ctx.client, oc.client_ch,
	    DSSH_POLL_WRITE, 100);
	ASSERT_TRUE(ev & DSSH_POLL_WRITE);

	dssh_channel_close(ctx.client, oc.client_ch);
	dssh_channel_close(ctx.server, oc.server_ch);
	conn_cleanup(&ctx);
	return TEST_PASS;
}

static int
test_raw_channel_poll_timeout_zero(void)
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
	ASSERT_THRD_CREATE(&ct, client_open_subsys_thread, &oc);
	ASSERT_THRD_CREATE(&st, server_accept_subsys_thread, &oc);
	thrd_join(ct, NULL);
	thrd_join(st, NULL);

	if (oc.client_ch == NULL || oc.server_ch == NULL) {
		conn_cleanup(&ctx);
		return TEST_SKIP;
	}

	/* No data pending -- poll READ with timeout_ms=0 returns 0 */
	int ev = dssh_channel_poll(ctx.server, oc.server_ch,
	    DSSH_POLL_READ, 0);
	ASSERT_EQ(ev, 0);

	dssh_channel_close(ctx.client, oc.client_ch);
	dssh_channel_close(ctx.server, oc.server_ch);
	conn_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Coverage: accept_raw (raw channel accept path)
 * ================================================================ */

static int
server_accept_raw_thread(void *arg)
{
	struct open_subsys_ctx *ctx = arg;
	struct dssh_incoming_open *inc = NULL;
	int res = dssh_session_accept(ctx->server, &inc, 5000);
	if (res < 0)
		return 0;
	ctx->server_ch = dssh_channel_accept_raw(ctx->server, inc);
	return 0;
}

static int
test_accept_raw(void)
{
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_SKIP;

	/*
	 * Client opens subsystem, server accepts raw.
	 * The server's raw channel auto-rejects the "subsystem" request,
	 * so client_ch will be NULL.  But server_ch is valid and we can
	 * test the raw channel's read/write/poll from the server side.
	 */
	struct open_subsys_ctx oc = {
		.client = ctx.client,
		.server = ctx.server,
		.subsystem = "test-sub",
	};
	thrd_t ct, st;
	ASSERT_THRD_CREATE(&ct, client_open_subsys_thread, &oc);
	ASSERT_THRD_CREATE(&st, server_accept_raw_thread, &oc);
	thrd_join(ct, NULL);
	thrd_join(st, NULL);

	/* Client should fail (subsystem rejected), server should succeed */
	if (oc.server_ch == NULL) {
		conn_cleanup(&ctx);
		return TEST_SKIP;
	}

	/* Verify accept_raw returned a valid channel */
	ASSERT_TRUE(oc.server_ch->open || oc.server_ch->close_received);

	dssh_channel_close(ctx.server, oc.server_ch);
	conn_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Coverage: reject with long description (truncation path)
 * ================================================================ */

static int
test_reject_long_description(void)
{
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_SKIP;

	/* Client opens a channel, server rejects with a very long description */
	struct open_exec_ctx oc = {
		.client = ctx.client,
		.server = ctx.server,
		.command = "echo reject",
		.cbs = &session_cbs,
		.accept_timeout = 5000,
		.reject = true,
		.reject_reason = 1,
	};

	/* We need custom accept thread that rejects with long desc */
	thrd_t ct;
	ASSERT_THRD_CREATE(&ct, client_open_exec_thread, &oc);

	struct dssh_incoming_open *inc = NULL;
	int res = dssh_session_accept(ctx.server, &inc, 5000);
	if (res == 0 && inc != NULL) {
		/* Build a 300-byte description to trigger truncation (max 256 msg buf) */
		char desc[300];
		memset(desc, 'A', sizeof(desc) - 1);
		desc[sizeof(desc) - 1] = '\0';
		dssh_session_reject(ctx.server, inc, 1, desc);
	}

	thrd_join(ct, NULL);
	/* Client should see open failure */
	ASSERT_TRUE(oc.client_ch == NULL);

	conn_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Coverage: dssh_session_read_signal when no signal pending
 * ================================================================ */

static int
test_read_signal_empty(void)
{
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_SKIP;

	struct open_exec_ctx oc = {
		.client = ctx.client,
		.server = ctx.server,
		.command = "echo sig_empty",
		.cbs = &session_cbs,
		.accept_timeout = 5000,
	};
	if (open_exec_channel(&oc) < 0 || oc.client_ch == NULL || oc.server_ch == NULL) {
		conn_cleanup(&ctx);
		return TEST_FAIL;
	}

	/* No signal pending -- should return NOMORE */
	const char *signame = NULL;
	int res = dssh_session_read_signal(ctx.server, oc.server_ch, &signame);
	ASSERT_EQ(res, DSSH_ERROR_NOMORE);

	dssh_session_close(ctx.server, oc.server_ch, 0);
	dssh_session_close(ctx.client, oc.client_ch, 0);
	conn_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Coverage: signal interleaving -- data then signal then more data
 * The readable amount should be clamped to the signal mark.
 * ================================================================ */

static int
test_signal_interleave_read(void)
{
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_SKIP;

	struct open_exec_ctx oc = {
		.client = ctx.client,
		.server = ctx.server,
		.command = "echo siginterleave",
		.cbs = &session_cbs,
		.accept_timeout = 5000,
	};
	if (open_exec_channel(&oc) < 0 || oc.client_ch == NULL || oc.server_ch == NULL) {
		conn_cleanup(&ctx);
		return TEST_FAIL;
	}

	/* Client sends data, then a signal, then more data */
	const uint8_t before[] = "BEFORE";
	int64_t w = dssh_session_write(ctx.client, oc.client_ch,
	    before, sizeof(before) - 1);
	ASSERT_TRUE(w > 0);

	/* Send signal */
	int res = dssh_session_send_signal(ctx.client, oc.client_ch, "INT");
	ASSERT_OK(res);

	const uint8_t after[] = "AFTER";
	w = dssh_session_write(ctx.client, oc.client_ch,
	    after, sizeof(after) - 1);
	ASSERT_TRUE(w > 0);

	/* Wait for all three messages to arrive through the demux.
	 * We can't poll for SIGNAL because it isn't "ready" until
	 * stdout is consumed past the mark.  Instead, poll for READ
	 * then sleep briefly to let the signal and second data arrive
	 * before we call session_stdout_readable(). */
	int ev = dssh_session_poll(ctx.server, oc.server_ch,
	    DSSH_POLL_READ, 5000);
	ASSERT_TRUE(ev & DSSH_POLL_READ);

	/* Brief delay so signal + second data arrive through demux */
	struct timespec ts = { .tv_sec = 0, .tv_nsec = 100000000 };
	thrd_sleep(&ts, NULL);

	/* First read: signal mark at stdout_pos=6 should clamp the
	 * readable amount even though more data is in the buffer. */
	uint8_t buf[64];
	int64_t n = dssh_session_read(ctx.server, oc.server_ch,
	    buf, sizeof(buf));
	ASSERT_EQ(n, (int64_t)(sizeof(before) - 1));
	ASSERT_MEM_EQ(buf, "BEFORE", 6);

	/* Now stdout_consumed >= stdout_pos(6), so signal is ready */
	ev = dssh_session_poll(ctx.server, oc.server_ch,
	    DSSH_POLL_SIGNAL, 5000);
	ASSERT_TRUE(ev & DSSH_POLL_SIGNAL);

	/* Read signal */
	const char *signame = NULL;
	res = dssh_session_read_signal(ctx.server, oc.server_ch, &signame);
	ASSERT_OK(res);
	ASSERT_STR_EQ(signame, "INT");

	/* Now the rest of the data is readable */
	ev = dssh_session_poll(ctx.server, oc.server_ch,
	    DSSH_POLL_READ, 5000);
	ASSERT_TRUE(ev & DSSH_POLL_READ);

	n = dssh_session_read(ctx.server, oc.server_ch,
	    buf, sizeof(buf));
	ASSERT_EQ(n, (int64_t)(sizeof(after) - 1));
	ASSERT_MEM_EQ(buf, "AFTER", 5);

	dssh_session_close(ctx.server, oc.server_ch, 0);
	dssh_session_close(ctx.client, oc.client_ch, 0);
	conn_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Coverage: signal interleave with stderr (ext_data)
 * ================================================================ */

static int
test_signal_interleave_read_ext(void)
{
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_SKIP;

	struct open_exec_ctx oc = {
		.client = ctx.client,
		.server = ctx.server,
		.command = "echo sigext",
		.cbs = &session_cbs,
		.accept_timeout = 5000,
	};
	if (open_exec_channel(&oc) < 0 || oc.client_ch == NULL || oc.server_ch == NULL) {
		conn_cleanup(&ctx);
		return TEST_FAIL;
	}

	/* Server sends stderr data, then a signal, then more stderr data */
	const uint8_t before[] = "ERR1";
	int64_t w = dssh_session_write_ext(ctx.server, oc.server_ch,
	    before, sizeof(before) - 1);
	ASSERT_TRUE(w > 0);

	int res = dssh_session_send_signal(ctx.server, oc.server_ch, "TERM");
	ASSERT_OK(res);

	const uint8_t after[] = "ERR2";
	w = dssh_session_write_ext(ctx.server, oc.server_ch,
	    after, sizeof(after) - 1);
	ASSERT_TRUE(w > 0);

	/* Wait for ext data to arrive, then sleep to let signal + second
	 * ext arrive through demux before we read. */
	int ev = dssh_session_poll(ctx.client, oc.client_ch,
	    DSSH_POLL_READEXT, 5000);
	ASSERT_TRUE(ev & DSSH_POLL_READEXT);

	struct timespec ts = { .tv_sec = 0, .tv_nsec = 100000000 };
	thrd_sleep(&ts, NULL);

	/* First read_ext: signal mark at stderr_pos=4 clamps the read */
	uint8_t buf[64];
	int64_t n = dssh_session_read_ext(ctx.client, oc.client_ch,
	    buf, sizeof(buf));
	ASSERT_EQ(n, (int64_t)(sizeof(before) - 1));
	ASSERT_MEM_EQ(buf, "ERR1", 4);

	/* Now stderr_consumed >= stderr_pos(4), so signal is ready */
	ev = dssh_session_poll(ctx.client, oc.client_ch,
	    DSSH_POLL_SIGNAL, 5000);
	ASSERT_TRUE(ev & DSSH_POLL_SIGNAL);

	const char *signame = NULL;
	res = dssh_session_read_signal(ctx.client, oc.client_ch, &signame);
	ASSERT_OK(res);
	ASSERT_STR_EQ(signame, "TERM");

	/* Now the rest of stderr is readable -- poll first to ensure arrival */
	ev = dssh_session_poll(ctx.client, oc.client_ch,
	    DSSH_POLL_READEXT, 5000);
	ASSERT_TRUE(ev & DSSH_POLL_READEXT);

	n = dssh_session_read_ext(ctx.client, oc.client_ch,
	    buf, sizeof(buf));
	ASSERT_EQ(n, (int64_t)(sizeof(after) - 1));
	ASSERT_MEM_EQ(buf, "ERR2", 4);

	dssh_session_close(ctx.server, oc.server_ch, 0);
	dssh_session_close(ctx.client, oc.client_ch, 0);
	conn_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Coverage: truncated CHANNEL_OPEN (demux parse errors)
 * ================================================================ */

static int
test_truncated_channel_open(void)
{
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_FAIL;

	/* Send a CHANNEL_OPEN with truncated type length (only 1 byte after msg type) */
	{
		uint8_t msg[2];
		msg[0] = SSH_MSG_CHANNEL_OPEN;
		msg[1] = 0;
		ASSERT_OK(dssh_transport_send_packet(ctx.client, msg, 2, NULL));
	}

	/* Send a CHANNEL_OPEN with valid type_len but truncated type string */
	{
		uint8_t msg[8];
		size_t pos = 0;
		msg[pos++] = SSH_MSG_CHANNEL_OPEN;
		dssh_serialize_uint32(100, msg, sizeof(msg), &pos); /* type_len=100 */
		ASSERT_OK(dssh_transport_send_packet(ctx.client, msg, pos, NULL));
	}

	/* Send a CHANNEL_OPEN with valid type but truncated channel fields */
	{
		uint8_t msg[16];
		size_t pos = 0;
		msg[pos++] = SSH_MSG_CHANNEL_OPEN;
		dssh_serialize_uint32(4, msg, sizeof(msg), &pos);
		memcpy(&msg[pos], "test", 4);
		pos += 4;
		/* Missing: sender_channel, initial_window, max_packet (12 bytes) */
		ASSERT_OK(dssh_transport_send_packet(ctx.client, msg, pos, NULL));
	}

	/* Brief delay to let demux process the packets */
	struct timespec ts = { .tv_sec = 0, .tv_nsec = 100000000 };
	thrd_sleep(&ts, NULL);

	/* Session should still be operational */
	ASSERT_TRUE(ctx.server->demux_running);

	conn_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Coverage: truncated CHANNEL_REQUEST (demux parse errors)
 * ================================================================ */

static int
test_truncated_channel_request(void)
{
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_FAIL;

	struct open_exec_ctx oc = {
		.client = ctx.client,
		.server = ctx.server,
		.command = "echo chanreq",
		.cbs = &session_cbs,
		.accept_timeout = 5000,
	};
	if (open_exec_channel(&oc) < 0 || oc.client_ch == NULL || oc.server_ch == NULL) {
		conn_cleanup(&ctx);
		return TEST_FAIL;
	}

	/* Send CHANNEL_REQUEST with payload too short for request type */
	{
		uint8_t msg[8];
		size_t pos = 0;
		msg[pos++] = SSH_MSG_CHANNEL_REQUEST;
		dssh_serialize_uint32(oc.server_ch->local_id, msg, sizeof(msg), &pos);
		/* Missing: request type string */
		ASSERT_OK(dssh_transport_send_packet(ctx.client, msg, pos, NULL));
	}

	/* Send CHANNEL_REQUEST with valid type_len but truncated type+want_reply */
	{
		uint8_t msg[16];
		size_t pos = 0;
		msg[pos++] = SSH_MSG_CHANNEL_REQUEST;
		dssh_serialize_uint32(oc.server_ch->local_id, msg, sizeof(msg), &pos);
		dssh_serialize_uint32(100, msg, sizeof(msg), &pos); /* rtype_len=100 */
		/* Missing: actual type string + want_reply */
		ASSERT_OK(dssh_transport_send_packet(ctx.client, msg, pos, NULL));
	}

	struct timespec ts = { .tv_sec = 0, .tv_nsec = 100000000 };
	thrd_sleep(&ts, NULL);

	ASSERT_TRUE(ctx.server->demux_running);

	dssh_session_close(ctx.server, oc.server_ch, 0);
	dssh_session_close(ctx.client, oc.client_ch, 0);
	conn_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Coverage: demux_dispatch with payload < 5 bytes
 * ================================================================ */

static int
test_demux_short_payload(void)
{
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_FAIL;

	/* Send a channel message with only 3 bytes (less than the 5 needed
	 * to parse the channel ID) */
	{
		uint8_t msg[4];
		size_t pos = 0;
		msg[pos++] = SSH_MSG_CHANNEL_DATA;
		msg[pos++] = 0;
		msg[pos++] = 0;
		ASSERT_OK(dssh_transport_send_packet(ctx.client, msg, pos, NULL));
	}

	/* Also send a short CHANNEL_CLOSE */
	{
		uint8_t msg[2];
		msg[0] = SSH_MSG_CHANNEL_CLOSE;
		msg[1] = 0;
		ASSERT_OK(dssh_transport_send_packet(ctx.client, msg, 2, NULL));
	}

	struct timespec ts = { .tv_sec = 0, .tv_nsec = 100000000 };
	thrd_sleep(&ts, NULL);

	ASSERT_TRUE(ctx.server->demux_running);

	conn_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Coverage: session poll for READ when nothing ready (timeout=0)
 * Covers the timeout_ms==0 early return path in dssh_session_poll
 * ================================================================ */

static int
test_session_poll_read_empty(void)
{
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_SKIP;

	struct open_exec_ctx oc = {
		.client = ctx.client,
		.server = ctx.server,
		.command = "echo poll_empty",
		.cbs = &session_cbs,
		.accept_timeout = 5000,
	};
	if (open_exec_channel(&oc) < 0 || oc.client_ch == NULL || oc.server_ch == NULL) {
		conn_cleanup(&ctx);
		return TEST_FAIL;
	}

	/* No data sent yet -- poll READ with timeout=0 should return 0 */
	int ev = dssh_session_poll(ctx.server, oc.server_ch,
	    DSSH_POLL_READ, 0);
	ASSERT_EQ(ev, 0);

	/* Also poll READEXT with timeout=0 -- no stderr data */
	ev = dssh_session_poll(ctx.server, oc.server_ch,
	    DSSH_POLL_READEXT, 0);
	ASSERT_EQ(ev, 0);

	/* Poll SIGNAL with timeout=0 -- no signals */
	ev = dssh_session_poll(ctx.server, oc.server_ch,
	    DSSH_POLL_SIGNAL, 0);
	ASSERT_EQ(ev, 0);

	dssh_session_close(ctx.server, oc.server_ch, 0);
	dssh_session_close(ctx.client, oc.client_ch, 0);
	conn_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Coverage: session write with window=0 and max_packet clamping
 * ================================================================ */

static int
test_session_write_window_zero(void)
{
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_SKIP;

	struct open_exec_ctx oc = {
		.client = ctx.client,
		.server = ctx.server,
		.command = "echo winzero",
		.cbs = &session_cbs,
		.accept_timeout = 5000,
	};
	if (open_exec_channel(&oc) < 0 || oc.client_ch == NULL || oc.server_ch == NULL) {
		conn_cleanup(&ctx);
		return TEST_FAIL;
	}

	/* Force remote_window to 0 to test the window=0 return path */
	oc.client_ch->remote_window = 0;
	int64_t w = dssh_session_write(ctx.client, oc.client_ch,
	    (const uint8_t *)"data", 4);
	ASSERT_EQ(w, 0);

	/* Force window to small value to test clamping */
	oc.client_ch->remote_window = 2;
	w = dssh_session_write(ctx.client, oc.client_ch,
	    (const uint8_t *)"data", 4);
	ASSERT_EQ(w, 2);  /* clamped to window size */

	/* Force max_packet smaller than data to test that clamping */
	oc.client_ch->remote_window = 1000;
	uint32_t orig_max = oc.client_ch->remote_max_packet;
	oc.client_ch->remote_max_packet = 3;
	w = dssh_session_write(ctx.client, oc.client_ch,
	    (const uint8_t *)"data", 4);
	ASSERT_EQ(w, 3);  /* clamped to max_packet */
	oc.client_ch->remote_max_packet = orig_max;

	dssh_session_close(ctx.server, oc.server_ch, 0);
	dssh_session_close(ctx.client, oc.client_ch, 0);
	conn_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Coverage: session write_ext with window=0 and max_packet clamping
 * ================================================================ */

static int
test_session_write_ext_window_zero(void)
{
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_SKIP;

	struct open_exec_ctx oc = {
		.client = ctx.client,
		.server = ctx.server,
		.command = "echo extwinzero",
		.cbs = &session_cbs,
		.accept_timeout = 5000,
	};
	if (open_exec_channel(&oc) < 0 || oc.client_ch == NULL || oc.server_ch == NULL) {
		conn_cleanup(&ctx);
		return TEST_FAIL;
	}

	/* Force remote_window to 0 */
	oc.server_ch->remote_window = 0;
	int64_t w = dssh_session_write_ext(ctx.server, oc.server_ch,
	    (const uint8_t *)"data", 4);
	ASSERT_EQ(w, 0);

	/* Force window to small value */
	oc.server_ch->remote_window = 2;
	w = dssh_session_write_ext(ctx.server, oc.server_ch,
	    (const uint8_t *)"data", 4);
	ASSERT_EQ(w, 2);

	/* Force max_packet smaller */
	oc.server_ch->remote_window = 1000;
	uint32_t orig_max = oc.server_ch->remote_max_packet;
	oc.server_ch->remote_max_packet = 3;
	w = dssh_session_write_ext(ctx.server, oc.server_ch,
	    (const uint8_t *)"data", 4);
	ASSERT_EQ(w, 3);
	oc.server_ch->remote_max_packet = orig_max;

	dssh_session_close(ctx.server, oc.server_ch, 0);
	dssh_session_close(ctx.client, oc.client_ch, 0);
	conn_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Coverage: accept with negative timeout (blocking, must be
 * unblocked by terminate)
 * ================================================================ */

static int
accept_blocking_thread(void *arg)
{
	struct conn_ctx *ctx = arg;
	struct dssh_incoming_open *inc = NULL;
	/* Blocking accept -- should return when session is terminated */
	dssh_session_accept(ctx->server, &inc, -1);
	return 0;
}

static int
test_accept_timeout_negative(void)
{
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_SKIP;

	thrd_t at;
	ASSERT_THRD_CREATE(&at, accept_blocking_thread, &ctx);

	/* Brief delay so the accept thread enters cnd_wait */
	struct timespec ts = { .tv_sec = 0, .tv_nsec = 50000000 };
	thrd_sleep(&ts, NULL);

	/* Terminate should unblock the accept */
	conn_cleanup(&ctx);
	thrd_join(at, NULL);
	return TEST_PASS;
}

/* ================================================================
 * Coverage: server rejects the exec request during setup
 * Covers the is_terminal && cb_res < 0 path in accept_channel
 * ================================================================ */

static int
reject_request_cb(const char *type, size_t type_len,
    bool want_reply, const uint8_t *data, size_t data_len,
    void *cbdata)
{
	(void)type; (void)type_len; (void)want_reply;
	(void)data; (void)data_len; (void)cbdata;
	return -1;  /* reject everything */
}

static const struct dssh_server_session_cbs reject_session_cbs = {
	.request_cb = reject_request_cb,
};

static int
server_accept_reject_exec_thread(void *arg)
{
	struct open_exec_ctx *ctx = arg;
	struct dssh_incoming_open *inc = NULL;
	int res = dssh_session_accept(ctx->server, &inc, 5000);
	if (res < 0)
		return 0;
	const char *req_type = NULL;
	const char *req_data = NULL;
	/* Accept channel but use rejecting callback for exec request */
	ctx->server_ch = dssh_session_accept_channel(ctx->server, inc,
	    &reject_session_cbs, &req_type, &req_data);
	return 0;
}

static int
test_setup_exec_rejected_by_callback(void)
{
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_SKIP;

	struct open_exec_ctx oc = {
		.client = ctx.client,
		.server = ctx.server,
		.command = "rejected_exec",
		.cbs = &session_cbs,  /* client uses normal cbs */
		.accept_timeout = 5000,
	};

	thrd_t ct, st;
	ASSERT_THRD_CREATE(&ct, client_open_exec_thread, &oc);
	ASSERT_THRD_CREATE(&st, server_accept_reject_exec_thread, &oc);
	thrd_join(ct, NULL);
	thrd_join(st, NULL);

	/* Client should fail (exec rejected) */
	ASSERT_NULL(oc.client_ch);
	/* Server accept_channel should return NULL (setup failed) */
	ASSERT_NULL(oc.server_ch);

	conn_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Coverage: truncated OPEN_CONFIRMATION (line 641)
 * ================================================================ */

static int
test_truncated_open_confirmation(void)
{
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_FAIL;

	/* Send a short CHANNEL_OPEN_CONFIRMATION (needs 1+16 bytes) */
	{
		uint8_t msg[8];
		size_t pos = 0;
		msg[pos++] = SSH_MSG_CHANNEL_OPEN_CONFIRMATION;
		dssh_serialize_uint32(0, msg, sizeof(msg), &pos); /* local_id */
		/* Missing: remote_id, window, max_packet (12 bytes) */
		ASSERT_OK(dssh_transport_send_packet(ctx.client, msg, pos, NULL));
	}

	struct timespec ts = { .tv_sec = 0, .tv_nsec = 100000000 };
	thrd_sleep(&ts, NULL);
	ASSERT_TRUE(ctx.server->demux_running);

	conn_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Coverage: OPEN_CONFIRMATION for unknown channel (line 646)
 * ================================================================ */

static int
test_open_confirmation_unknown_channel(void)
{
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_FAIL;

	/* Send OPEN_CONFIRMATION for a channel ID that doesn't exist */
	{
		uint8_t msg[20];
		size_t pos = 0;
		msg[pos++] = SSH_MSG_CHANNEL_OPEN_CONFIRMATION;
		dssh_serialize_uint32(9999, msg, sizeof(msg), &pos); /* bogus id */
		dssh_serialize_uint32(0, msg, sizeof(msg), &pos);
		dssh_serialize_uint32(0x200000, msg, sizeof(msg), &pos);
		dssh_serialize_uint32(0x8000, msg, sizeof(msg), &pos);
		ASSERT_OK(dssh_transport_send_packet(ctx.client, msg, pos, NULL));
	}

	struct timespec ts = { .tv_sec = 0, .tv_nsec = 100000000 };
	thrd_sleep(&ts, NULL);
	ASSERT_TRUE(ctx.server->demux_running);

	conn_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Coverage: unknown msg type dispatched to channel (line 612)
 * -- CHANNEL_SUCCESS/FAILURE when no request pending
 * ================================================================ */

static int
test_channel_success_no_request(void)
{
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_FAIL;

	struct open_exec_ctx oc = {
		.client = ctx.client,
		.server = ctx.server,
		.command = "echo success",
		.cbs = &session_cbs,
		.accept_timeout = 5000,
	};
	if (open_exec_channel(&oc) < 0 || oc.client_ch == NULL || oc.server_ch == NULL) {
		conn_cleanup(&ctx);
		return TEST_FAIL;
	}

	/* Send CHANNEL_SUCCESS to the server channel (no request pending) */
	{
		uint8_t msg[8];
		size_t pos = 0;
		msg[pos++] = SSH_MSG_CHANNEL_SUCCESS;
		dssh_serialize_uint32(oc.server_ch->local_id, msg, sizeof(msg), &pos);
		ASSERT_OK(dssh_transport_send_packet(ctx.client, msg, pos, NULL));
	}
	/* Also CHANNEL_FAILURE */
	{
		uint8_t msg[8];
		size_t pos = 0;
		msg[pos++] = SSH_MSG_CHANNEL_FAILURE;
		dssh_serialize_uint32(oc.server_ch->local_id, msg, sizeof(msg), &pos);
		ASSERT_OK(dssh_transport_send_packet(ctx.client, msg, pos, NULL));
	}

	struct timespec ts = { .tv_sec = 0, .tv_nsec = 100000000 };
	thrd_sleep(&ts, NULL);
	ASSERT_TRUE(ctx.server->demux_running);

	dssh_session_close(ctx.server, oc.server_ch, 0);
	dssh_session_close(ctx.client, oc.client_ch, 0);
	conn_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Coverage: send EOF/CLOSE already-sent guards
 * ================================================================ */

static int
test_double_eof_close(void)
{
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_SKIP;

	struct open_exec_ctx oc = {
		.client = ctx.client,
		.server = ctx.server,
		.command = "echo double",
		.cbs = &session_cbs,
		.accept_timeout = 5000,
	};
	if (open_exec_channel(&oc) < 0 || oc.client_ch == NULL || oc.server_ch == NULL) {
		conn_cleanup(&ctx);
		return TEST_FAIL;
	}

	/* Close normally, which sends EOF + CLOSE internally */
	dssh_session_close(ctx.server, oc.server_ch, 0);

	/* Client: close its side too */
	dssh_session_close(ctx.client, oc.client_ch, 0);

	conn_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Coverage: demux DATA with dlen field > actual payload bytes
 * ================================================================ */

static int
test_data_dlen_exceeds_payload(void)
{
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_FAIL;

	struct open_exec_ctx oc = {
		.client = ctx.client,
		.server = ctx.server,
		.command = "echo dlen",
		.cbs = &session_cbs,
		.accept_timeout = 5000,
	};
	if (open_exec_channel(&oc) < 0 || oc.client_ch == NULL || oc.server_ch == NULL) {
		conn_cleanup(&ctx);
		return TEST_FAIL;
	}

	/* Send CHANNEL_DATA with dlen=100 but only 3 bytes of data */
	{
		uint8_t msg[32];
		size_t pos = 0;
		msg[pos++] = SSH_MSG_CHANNEL_DATA;
		dssh_serialize_uint32(oc.server_ch->local_id, msg, sizeof(msg), &pos);
		dssh_serialize_uint32(100, msg, sizeof(msg), &pos); /* dlen=100 */
		memcpy(&msg[pos], "abc", 3);
		pos += 3;
		ASSERT_OK(dssh_transport_send_packet(ctx.client, msg, pos, NULL));
	}

	/* Send CHANNEL_EXTENDED_DATA with dlen=100 but only 2 bytes */
	{
		uint8_t msg[32];
		size_t pos = 0;
		msg[pos++] = SSH_MSG_CHANNEL_EXTENDED_DATA;
		dssh_serialize_uint32(oc.server_ch->local_id, msg, sizeof(msg), &pos);
		dssh_serialize_uint32(1, msg, sizeof(msg), &pos); /* data_type=stderr */
		dssh_serialize_uint32(100, msg, sizeof(msg), &pos); /* dlen=100 */
		memcpy(&msg[pos], "xy", 2);
		pos += 2;
		ASSERT_OK(dssh_transport_send_packet(ctx.client, msg, pos, NULL));
	}

	/* Brief delay for demux to process */
	struct timespec ts = { .tv_sec = 0, .tv_nsec = 100000000 };
	thrd_sleep(&ts, NULL);

	/* Session should still be running -- dlen is clamped to payload */
	ASSERT_TRUE(ctx.server->demux_running);

	/* Read whatever arrived -- should be clamped data */
	uint8_t buf[256];
	int64_t n = dssh_session_read(ctx.server, oc.server_ch, buf, sizeof(buf));
	ASSERT_TRUE(n >= 0);

	dssh_session_close(ctx.server, oc.server_ch, 0);
	dssh_session_close(ctx.client, oc.client_ch, 0);
	conn_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Formerly-guarded paths -- send_data/ext overflow, chan_type==0
 * ================================================================ */

static int
test_send_extended_data_toolong(void)
{
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_SKIP;

	struct open_exec_ctx oc = {
		.client = ctx.client,
		.server = ctx.server,
		.command = "echo extlen",
		.cbs = &session_cbs,
		.accept_timeout = 5000,
	};
	if (open_exec_channel(&oc) < 0 || oc.client_ch == NULL || oc.server_ch == NULL) {
		conn_cleanup(&ctx);
		return TEST_FAIL;
	}

	/* Call send_extended_data directly with len > remote_window */
	uint8_t data[] = "x";
	oc.server_ch->remote_window = 0;
	int res = dssh_conn_send_extended_data(ctx.server, oc.server_ch,
	    1, data, 1, NULL);
	ASSERT_EQ(res, DSSH_ERROR_TOOLONG);

	/* Also len > remote_max_packet */
	oc.server_ch->remote_window = 0xFFFFFFFF;
	oc.server_ch->remote_max_packet = 0;
	res = dssh_conn_send_extended_data(ctx.server, oc.server_ch,
	    1, data, 1, NULL);
	ASSERT_EQ(res, DSSH_ERROR_TOOLONG);

	/* Restore for cleanup */
	oc.server_ch->remote_window = 0x200000;
	oc.server_ch->remote_max_packet = 0x8000;

	dssh_session_close(ctx.server, oc.server_ch, 0);
	dssh_session_close(ctx.client, oc.client_ch, 0);
	conn_cleanup(&ctx);
	return TEST_PASS;
}

static int
test_demux_dispatch_chan_type_zero(void)
{
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_SKIP;

	struct open_exec_ctx oc = {
		.client = ctx.client,
		.server = ctx.server,
		.command = "echo chantype",
		.cbs = &session_cbs,
		.accept_timeout = 5000,
	};
	if (open_exec_channel(&oc) < 0 || oc.client_ch == NULL || oc.server_ch == NULL) {
		conn_cleanup(&ctx);
		return TEST_FAIL;
	}

	/* Temporarily set chan_type to 0 and call demux_dispatch directly.
	 * It should return early without crashing. */
	int saved_type = oc.server_ch->chan_type;
	oc.server_ch->chan_type = 0;

	/* Build a CHANNEL_DATA payload targeting this channel */
	uint8_t payload[16];
	size_t pos = 0;
	payload[pos++] = SSH_MSG_CHANNEL_DATA;
	dssh_serialize_uint32(oc.server_ch->local_id, payload, sizeof(payload), &pos);
	dssh_serialize_uint32(1, payload, sizeof(payload), &pos);
	payload[pos++] = 'x';

	demux_dispatch(ctx.server, SSH_MSG_CHANNEL_DATA, payload, pos);

	oc.server_ch->chan_type = saved_type;

	dssh_session_close(ctx.server, oc.server_ch, 0);
	dssh_session_close(ctx.client, oc.client_ch, 0);
	conn_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Coverage: send_eof/close already-sent, window replenish,
 * window underflow to zero
 * ================================================================ */

static int
test_send_eof_already_sent(void)
{
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_SKIP;

	struct open_exec_ctx oc = {
		.client = ctx.client, .server = ctx.server,
		.command = "echo eof2", .cbs = &session_cbs,
		.accept_timeout = 5000,
	};
	if (open_exec_channel(&oc) < 0 || !oc.client_ch || !oc.server_ch) {
		conn_cleanup(&ctx);
		return TEST_FAIL;
	}

	/* Send EOF, then send it again -- second call returns 0 immediately */
	ASSERT_OK(dssh_conn_send_eof(ctx.client, oc.client_ch));
	ASSERT_TRUE(oc.client_ch->eof_sent);
	ASSERT_EQ(dssh_conn_send_eof(ctx.client, oc.client_ch), 0);

	dssh_session_close(ctx.server, oc.server_ch, 0);
	conn_cleanup(&ctx);
	return TEST_PASS;
}

static int
test_send_close_already_sent(void)
{
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_SKIP;

	struct open_exec_ctx oc = {
		.client = ctx.client, .server = ctx.server,
		.command = "echo close2", .cbs = &session_cbs,
		.accept_timeout = 5000,
	};
	if (open_exec_channel(&oc) < 0 || !oc.client_ch || !oc.server_ch) {
		conn_cleanup(&ctx);
		return TEST_FAIL;
	}

	ASSERT_OK(dssh_conn_close(ctx.client, oc.client_ch));
	ASSERT_TRUE(oc.client_ch->close_sent);
	ASSERT_EQ(dssh_conn_close(ctx.client, oc.client_ch), 0);

	dssh_session_close(ctx.server, oc.server_ch, 0);
	conn_cleanup(&ctx);
	return TEST_PASS;
}

static int
test_maybe_replenish_after_eof(void)
{
	/* replenish should be a no-op when eof_received is true */
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_SKIP;

	struct open_exec_ctx oc = {
		.client = ctx.client, .server = ctx.server,
		.command = "echo replenish", .cbs = &session_cbs,
		.accept_timeout = 5000,
	};
	if (open_exec_channel(&oc) < 0 || !oc.client_ch || !oc.server_ch) {
		conn_cleanup(&ctx);
		return TEST_FAIL;
	}

	/* Force low window + eof_received */
	oc.server_ch->local_window = 0;
	oc.server_ch->eof_received = true;
	/* Should return without sending WINDOW_ADJUST */
	maybe_replenish_window(ctx.server, oc.server_ch);
	/* Window should still be 0 */
	ASSERT_EQ(oc.server_ch->local_window, (uint32_t)0);
	oc.server_ch->eof_received = false;

	dssh_session_close(ctx.server, oc.server_ch, 0);
	dssh_session_close(ctx.client, oc.client_ch, 0);
	conn_cleanup(&ctx);
	return TEST_PASS;
}

static int
test_maybe_replenish_low_window(void)
{
	/* replenish should send WINDOW_ADJUST when window < max/2 */
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_SKIP;

	struct open_exec_ctx oc = {
		.client = ctx.client, .server = ctx.server,
		.command = "echo replow", .cbs = &session_cbs,
		.accept_timeout = 5000,
	};
	if (open_exec_channel(&oc) < 0 || !oc.client_ch || !oc.server_ch) {
		conn_cleanup(&ctx);
		return TEST_FAIL;
	}

	/* Force window below half of max */
	oc.server_ch->local_window = 1;
	maybe_replenish_window(ctx.server, oc.server_ch);
	/* Window should have been replenished */
	ASSERT_TRUE(oc.server_ch->local_window > 1);

	dssh_session_close(ctx.server, oc.server_ch, 0);
	dssh_session_close(ctx.client, oc.client_ch, 0);
	conn_cleanup(&ctx);
	return TEST_PASS;
}

static int
test_demux_window_underflow_to_zero(void)
{
	/* Send data that exactly equals local_window -> window becomes 0 */
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_FAIL;

	struct open_exec_ctx oc = {
		.client = ctx.client, .server = ctx.server,
		.command = "echo wunder", .cbs = &session_cbs,
		.accept_timeout = 5000,
	};
	if (open_exec_channel(&oc) < 0 || !oc.client_ch || !oc.server_ch) {
		conn_cleanup(&ctx);
		return TEST_FAIL;
	}

	/* Set server's local_window to a small value */
	oc.server_ch->local_window = 5;
	oc.server_ch->window_max = 5;

	/* Client sends exactly 5 bytes -- should drain window to 0 */
	const uint8_t data[] = "hello";
	dssh_session_write(ctx.client, oc.client_ch, data, 5);

	/* Wait for data to arrive */
	int ev = dssh_session_poll(ctx.server, oc.server_ch,
	    DSSH_POLL_READ, 5000);
	ASSERT_TRUE(ev & DSSH_POLL_READ);

	/* Now send more data that exceeds the 0 window -- should
	 * trigger the window underflow (local_window = 0) path */
	{
		uint8_t msg[32];
		size_t pos = 0;
		msg[pos++] = SSH_MSG_CHANNEL_DATA;
		dssh_serialize_uint32(oc.server_ch->local_id, msg, sizeof(msg), &pos);
		dssh_serialize_uint32(3, msg, sizeof(msg), &pos);
		memcpy(&msg[pos], "abc", 3);
		pos += 3;
		ASSERT_OK(dssh_transport_send_packet(ctx.client, msg, pos, NULL));
	}

	struct timespec ts = { .tv_sec = 0, .tv_nsec = 100000000 };
	thrd_sleep(&ts, NULL);

	dssh_session_close(ctx.server, oc.server_ch, 0);
	dssh_session_close(ctx.client, oc.client_ch, 0);
	conn_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Coverage: window_add overflow via send_window_adjust
 * ================================================================ */

static int
test_window_add_overflow(void)
{
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_SKIP;

	struct open_exec_ctx oc = {
		.client = ctx.client, .server = ctx.server,
		.command = "echo woverflow", .cbs = &session_cbs,
		.accept_timeout = 5000,
	};
	if (open_exec_channel(&oc) < 0 || !oc.client_ch || !oc.server_ch) {
		conn_cleanup(&ctx);
		return TEST_FAIL;
	}

	/* Set local_window near UINT32_MAX, then add enough to overflow */
	oc.server_ch->local_window = UINT32_MAX - 10;
	dssh_conn_send_window_adjust(ctx.server, oc.server_ch, 20);
	/* window_add should clamp to UINT32_MAX */
	ASSERT_EQ(oc.server_ch->local_window, UINT32_MAX);

	dssh_session_close(ctx.server, oc.server_ch, 0);
	dssh_session_close(ctx.client, oc.client_ch, 0);
	conn_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Coverage: defensive guard tests -- !open, close_received,
 * empty queue, eof/close in poll, signal mark <= consumed,
 * infinite-wait poll, empty read_ext
 * ================================================================ */

static int
test_session_write_not_open(void)
{
	dssh_test_reset_global_config();
	if (register_all_algorithms() < 0)
		return TEST_SKIP;
	dssh_session s = dssh_session_init(false, 0);
	if (s == NULL)
		return TEST_SKIP;

	struct dssh_channel_s ch = {0};
	ch.open = false;
	ch.eof_sent = false;
	ch.close_received = false;

	int64_t r = dssh_session_write(s, &ch, (const uint8_t *)"x", 1);
	ASSERT_EQ(r, DSSH_ERROR_TERMINATED);

	dssh_session_cleanup(s);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

static int
test_session_write_ext_not_open(void)
{
	dssh_test_reset_global_config();
	if (register_all_algorithms() < 0)
		return TEST_SKIP;
	dssh_session s = dssh_session_init(false, 0);
	if (s == NULL)
		return TEST_SKIP;

	struct dssh_channel_s ch = {0};
	ch.open = false;
	ch.eof_sent = false;
	ch.close_received = false;

	int64_t r = dssh_session_write_ext(s, &ch, (const uint8_t *)"x", 1);
	ASSERT_EQ(r, DSSH_ERROR_TERMINATED);

	dssh_session_cleanup(s);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

static int
test_session_write_close_received(void)
{
	dssh_test_reset_global_config();
	if (register_all_algorithms() < 0)
		return TEST_SKIP;
	dssh_session s = dssh_session_init(false, 0);
	if (s == NULL)
		return TEST_SKIP;

	struct dssh_channel_s ch = {0};
	ch.open = true;
	ch.eof_sent = false;
	ch.close_received = true;

	int64_t r = dssh_session_write(s, &ch, (const uint8_t *)"x", 1);
	ASSERT_EQ(r, DSSH_ERROR_TERMINATED);

	dssh_session_cleanup(s);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

static int
test_session_write_ext_close_received(void)
{
	dssh_test_reset_global_config();
	if (register_all_algorithms() < 0)
		return TEST_SKIP;
	dssh_session s = dssh_session_init(false, 0);
	if (s == NULL)
		return TEST_SKIP;

	struct dssh_channel_s ch = {0};
	ch.open = true;
	ch.eof_sent = false;
	ch.close_received = true;

	int64_t r = dssh_session_write_ext(s, &ch, (const uint8_t *)"x", 1);
	ASSERT_EQ(r, DSSH_ERROR_TERMINATED);

	dssh_session_cleanup(s);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

static int
test_channel_write_not_open(void)
{
	dssh_test_reset_global_config();
	if (register_all_algorithms() < 0)
		return TEST_SKIP;
	dssh_session s = dssh_session_init(false, 0);
	if (s == NULL)
		return TEST_SKIP;

	struct dssh_channel_s ch = {0};
	ch.open = false;
	ch.eof_sent = false;
	ch.close_received = false;

	int res = dssh_channel_write(s, &ch, (const uint8_t *)"x", 1);
	ASSERT_EQ(res, DSSH_ERROR_TERMINATED);

	dssh_session_cleanup(s);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

static int
test_channel_write_close_received(void)
{
	dssh_test_reset_global_config();
	if (register_all_algorithms() < 0)
		return TEST_SKIP;
	dssh_session s = dssh_session_init(false, 0);
	if (s == NULL)
		return TEST_SKIP;

	struct dssh_channel_s ch = {0};
	ch.open = true;
	ch.eof_sent = false;
	ch.close_received = true;

	int res = dssh_channel_write(s, &ch, (const uint8_t *)"x", 1);
	ASSERT_EQ(res, DSSH_ERROR_TERMINATED);

	dssh_session_cleanup(s);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

static int
test_channel_read_empty_queue(void)
{
	dssh_test_reset_global_config();
	if (register_all_algorithms() < 0)
		return TEST_SKIP;
	dssh_session s = dssh_session_init(false, 0);
	if (s == NULL)
		return TEST_SKIP;

	struct dssh_channel_s ch = {0};
	ch.chan_type = DSSH_CHAN_RAW;
	mtx_init(&ch.buf_mtx, mtx_plain);
	dssh_msgqueue_init(&ch.buf.raw.queue);

	uint8_t buf[64];
	int64_t n = dssh_channel_read(s, &ch, buf, sizeof(buf));
	ASSERT_EQ(n, (int64_t)0);

	dssh_msgqueue_free(&ch.buf.raw.queue);
	mtx_destroy(&ch.buf_mtx);
	dssh_session_cleanup(s);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

static int
test_channel_poll_eof(void)
{
	dssh_test_reset_global_config();
	if (register_all_algorithms() < 0)
		return TEST_SKIP;
	dssh_session s = dssh_session_init(false, 0);
	if (s == NULL)
		return TEST_SKIP;

	struct dssh_channel_s ch = {0};
	ch.chan_type = DSSH_CHAN_RAW;
	mtx_init(&ch.buf_mtx, mtx_plain);
	cnd_init(&ch.poll_cnd);
	dssh_msgqueue_init(&ch.buf.raw.queue);
	ch.eof_received = true;

	int r = dssh_channel_poll(s, &ch, DSSH_POLL_READ, 0);
	ASSERT_TRUE(r & DSSH_POLL_READ);

	dssh_msgqueue_free(&ch.buf.raw.queue);
	cnd_destroy(&ch.poll_cnd);
	mtx_destroy(&ch.buf_mtx);
	dssh_session_cleanup(s);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

static int
test_channel_poll_close_received(void)
{
	dssh_test_reset_global_config();
	if (register_all_algorithms() < 0)
		return TEST_SKIP;
	dssh_session s = dssh_session_init(false, 0);
	if (s == NULL)
		return TEST_SKIP;

	struct dssh_channel_s ch = {0};
	ch.chan_type = DSSH_CHAN_RAW;
	mtx_init(&ch.buf_mtx, mtx_plain);
	cnd_init(&ch.poll_cnd);
	dssh_msgqueue_init(&ch.buf.raw.queue);
	ch.close_received = true;

	int r = dssh_channel_poll(s, &ch, DSSH_POLL_READ, 0);
	ASSERT_TRUE(r & DSSH_POLL_READ);

	dssh_msgqueue_free(&ch.buf.raw.queue);
	cnd_destroy(&ch.poll_cnd);
	mtx_destroy(&ch.buf_mtx);
	dssh_session_cleanup(s);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

static int
test_session_poll_close_read(void)
{
	dssh_test_reset_global_config();
	if (register_all_algorithms() < 0)
		return TEST_SKIP;
	dssh_session s = dssh_session_init(false, 0);
	if (s == NULL)
		return TEST_SKIP;

	struct dssh_channel_s ch = {0};
	ch.chan_type = DSSH_CHAN_SESSION;
	mtx_init(&ch.buf_mtx, mtx_plain);
	cnd_init(&ch.poll_cnd);
	dssh_bytebuf_init(&ch.buf.session.stdout_buf, 256);
	dssh_bytebuf_init(&ch.buf.session.stderr_buf, 256);
	dssh_sigqueue_init(&ch.buf.session.signals);
	ch.close_received = true;

	int r = dssh_session_poll(s, &ch, DSSH_POLL_READ, 0);
	ASSERT_TRUE(r & DSSH_POLL_READ);

	dssh_sigqueue_free(&ch.buf.session.signals);
	dssh_bytebuf_free(&ch.buf.session.stdout_buf);
	dssh_bytebuf_free(&ch.buf.session.stderr_buf);
	cnd_destroy(&ch.poll_cnd);
	mtx_destroy(&ch.buf_mtx);
	dssh_session_cleanup(s);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

static int
test_session_poll_close_readext(void)
{
	dssh_test_reset_global_config();
	if (register_all_algorithms() < 0)
		return TEST_SKIP;
	dssh_session s = dssh_session_init(false, 0);
	if (s == NULL)
		return TEST_SKIP;

	struct dssh_channel_s ch = {0};
	ch.chan_type = DSSH_CHAN_SESSION;
	mtx_init(&ch.buf_mtx, mtx_plain);
	cnd_init(&ch.poll_cnd);
	dssh_bytebuf_init(&ch.buf.session.stdout_buf, 256);
	dssh_bytebuf_init(&ch.buf.session.stderr_buf, 256);
	dssh_sigqueue_init(&ch.buf.session.signals);
	ch.close_received = true;

	int r = dssh_session_poll(s, &ch, DSSH_POLL_READEXT, 0);
	ASSERT_TRUE(r & DSSH_POLL_READEXT);

	dssh_sigqueue_free(&ch.buf.session.signals);
	dssh_bytebuf_free(&ch.buf.session.stdout_buf);
	dssh_bytebuf_free(&ch.buf.session.stderr_buf);
	cnd_destroy(&ch.poll_cnd);
	mtx_destroy(&ch.buf_mtx);
	dssh_session_cleanup(s);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

static int
test_stdout_signal_mark_consumed(void)
{
	/*
	 * Test session_stdout_readable when a signal mark is at or before
	 * the consumed position -- the mark should not limit readable bytes.
	 * Tested indirectly via dssh_session_poll + dssh_session_read.
	 */
	dssh_test_reset_global_config();
	if (register_all_algorithms() < 0)
		return TEST_SKIP;
	dssh_session s = dssh_session_init(false, 0);
	if (s == NULL)
		return TEST_SKIP;

	struct dssh_channel_s ch = {0};
	ch.chan_type = DSSH_CHAN_SESSION;
	mtx_init(&ch.buf_mtx, mtx_plain);
	cnd_init(&ch.poll_cnd);
	dssh_bytebuf_init(&ch.buf.session.stdout_buf, 256);
	dssh_bytebuf_init(&ch.buf.session.stderr_buf, 256);
	dssh_sigqueue_init(&ch.buf.session.signals);

	/* Write 5 bytes to stdout */
	dssh_bytebuf_write(&ch.buf.session.stdout_buf, (const uint8_t *)"hello", 5);

	/* Push a signal with stdout mark at position 2 */
	dssh_sigqueue_push(&ch.buf.session.signals, "TERM", 2, 0);

	/* Set consumed past the mark */
	ch.stdout_consumed = 3;

	/* Poll should report data available (mark <= consumed, so not limiting) */
	int r = dssh_session_poll(s, &ch, DSSH_POLL_READ, 0);
	ASSERT_TRUE(r & DSSH_POLL_READ);

	/* Read should return all 5 available bytes (mark not limiting) */
	uint8_t buf[64];
	int64_t n = dssh_session_read(s, &ch, buf, sizeof(buf));
	ASSERT_EQ(n, (int64_t)5);

	dssh_sigqueue_free(&ch.buf.session.signals);
	dssh_bytebuf_free(&ch.buf.session.stdout_buf);
	dssh_bytebuf_free(&ch.buf.session.stderr_buf);
	cnd_destroy(&ch.poll_cnd);
	mtx_destroy(&ch.buf_mtx);
	dssh_session_cleanup(s);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

static int
test_stderr_signal_mark_consumed(void)
{
	/*
	 * Test session_stderr_readable when a signal mark is at or before
	 * the consumed position -- tested indirectly via dssh_session_read_ext.
	 */
	dssh_test_reset_global_config();
	if (register_all_algorithms() < 0)
		return TEST_SKIP;
	dssh_session s = dssh_session_init(false, 0);
	if (s == NULL)
		return TEST_SKIP;

	struct dssh_channel_s ch = {0};
	ch.chan_type = DSSH_CHAN_SESSION;
	mtx_init(&ch.buf_mtx, mtx_plain);
	cnd_init(&ch.poll_cnd);
	dssh_bytebuf_init(&ch.buf.session.stdout_buf, 256);
	dssh_bytebuf_init(&ch.buf.session.stderr_buf, 256);
	dssh_sigqueue_init(&ch.buf.session.signals);

	/* Write 5 bytes to stderr */
	dssh_bytebuf_write(&ch.buf.session.stderr_buf, (const uint8_t *)"error", 5);

	/* Push a signal with stderr mark at position 2 */
	dssh_sigqueue_push(&ch.buf.session.signals, "TERM", 0, 2);

	/* Set consumed past the mark */
	ch.stderr_consumed = 3;

	/* Poll should report extended data available */
	int r = dssh_session_poll(s, &ch, DSSH_POLL_READEXT, 0);
	ASSERT_TRUE(r & DSSH_POLL_READEXT);

	/* Read should return all 5 available bytes */
	uint8_t buf[64];
	int64_t n = dssh_session_read_ext(s, &ch, buf, sizeof(buf));
	ASSERT_EQ(n, (int64_t)5);

	dssh_sigqueue_free(&ch.buf.session.signals);
	dssh_bytebuf_free(&ch.buf.session.stdout_buf);
	dssh_bytebuf_free(&ch.buf.session.stderr_buf);
	cnd_destroy(&ch.poll_cnd);
	mtx_destroy(&ch.buf_mtx);
	dssh_session_cleanup(s);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

static int
test_channel_poll_infinite_data_ready(void)
{
	/*
	 * channel_poll with timeout_ms == -1 (infinite wait) but data
	 * already in the queue -- should return immediately.
	 */
	dssh_test_reset_global_config();
	if (register_all_algorithms() < 0)
		return TEST_SKIP;
	dssh_session s = dssh_session_init(false, 0);
	if (s == NULL)
		return TEST_SKIP;

	struct dssh_channel_s ch = {0};
	ch.chan_type = DSSH_CHAN_RAW;
	mtx_init(&ch.buf_mtx, mtx_plain);
	cnd_init(&ch.poll_cnd);
	dssh_msgqueue_init(&ch.buf.raw.queue);
	dssh_msgqueue_push(&ch.buf.raw.queue, (const uint8_t *)"x", 1);

	int r = dssh_channel_poll(s, &ch, DSSH_POLL_READ, -1);
	ASSERT_TRUE(r & DSSH_POLL_READ);

	dssh_msgqueue_free(&ch.buf.raw.queue);
	cnd_destroy(&ch.poll_cnd);
	mtx_destroy(&ch.buf_mtx);
	dssh_session_cleanup(s);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

static int
test_session_poll_infinite_data_ready(void)
{
	/*
	 * session_poll with timeout_ms == -1 (infinite wait) but data
	 * already in stdout -- should return immediately.
	 */
	dssh_test_reset_global_config();
	if (register_all_algorithms() < 0)
		return TEST_SKIP;
	dssh_session s = dssh_session_init(false, 0);
	if (s == NULL)
		return TEST_SKIP;

	struct dssh_channel_s ch = {0};
	ch.chan_type = DSSH_CHAN_SESSION;
	mtx_init(&ch.buf_mtx, mtx_plain);
	cnd_init(&ch.poll_cnd);
	dssh_bytebuf_init(&ch.buf.session.stdout_buf, 256);
	dssh_bytebuf_init(&ch.buf.session.stderr_buf, 256);
	dssh_sigqueue_init(&ch.buf.session.signals);
	dssh_bytebuf_write(&ch.buf.session.stdout_buf, (const uint8_t *)"hi", 2);

	int r = dssh_session_poll(s, &ch, DSSH_POLL_READ, -1);
	ASSERT_TRUE(r & DSSH_POLL_READ);

	dssh_sigqueue_free(&ch.buf.session.signals);
	dssh_bytebuf_free(&ch.buf.session.stdout_buf);
	dssh_bytebuf_free(&ch.buf.session.stderr_buf);
	cnd_destroy(&ch.poll_cnd);
	mtx_destroy(&ch.buf_mtx);
	dssh_session_cleanup(s);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

static int
test_session_read_ext_empty(void)
{
	/*
	 * session_read_ext on an empty stderr buffer should return 0.
	 * Covers the n > 0 False branch at line 1643.
	 */
	dssh_test_reset_global_config();
	if (register_all_algorithms() < 0)
		return TEST_SKIP;
	dssh_session s = dssh_session_init(false, 0);
	if (s == NULL)
		return TEST_SKIP;

	struct dssh_channel_s ch = {0};
	ch.chan_type = DSSH_CHAN_SESSION;
	mtx_init(&ch.buf_mtx, mtx_plain);
	cnd_init(&ch.poll_cnd);
	dssh_bytebuf_init(&ch.buf.session.stdout_buf, 256);
	dssh_bytebuf_init(&ch.buf.session.stderr_buf, 256);
	dssh_sigqueue_init(&ch.buf.session.signals);

	uint8_t buf[64];
	int64_t n = dssh_session_read_ext(s, &ch, buf, sizeof(buf));
	ASSERT_EQ(n, (int64_t)0);

	dssh_sigqueue_free(&ch.buf.session.signals);
	dssh_bytebuf_free(&ch.buf.session.stdout_buf);
	dssh_bytebuf_free(&ch.buf.session.stderr_buf);
	cnd_destroy(&ch.poll_cnd);
	mtx_destroy(&ch.buf_mtx);
	dssh_session_cleanup(s);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

/* ================================================================
 * Deterministic branch coverage -- profiling-unstable branches
 *
 * These tests exercise branches that flip between "covered" and
 * "missed" across runs due to non-deterministic thread scheduling.
 * Each test here is single-threaded with no timing dependency.
 * ================================================================ */

/*
 * session_poll nsec overflow (line 1606): ts.tv_nsec >= 1000000000L.
 * We use timeout_ms=999 which adds 999000000 nsec.  The current
 * nsec is almost always > 1000001 (~1ms), making the total
 * exceed 1 billion.  We call it in a loop to ensure the branch
 * fires at least once.
 */
static int
test_session_poll_nsec_overflow(void)
{
	dssh_test_reset_global_config();
	if (register_all_algorithms() < 0)
		return TEST_SKIP;
	dssh_session s = dssh_session_init(false, 0);
	if (s == NULL)
		return TEST_SKIP;

	struct dssh_channel_s ch = {0};
	ch.chan_type = DSSH_CHAN_SESSION;
	mtx_init(&ch.buf_mtx, mtx_plain);
	cnd_init(&ch.poll_cnd);
	dssh_bytebuf_init(&ch.buf.session.stdout_buf, 256);
	dssh_bytebuf_init(&ch.buf.session.stderr_buf, 256);
	dssh_sigqueue_init(&ch.buf.session.signals);

	/* Empty buffers + short timeout -> times out.
	 * timeout_ms=999 adds 999000000 nsec -- overflows with >99.9% probability. */
	for (int i = 0; i < 3; i++) {
		int r = dssh_session_poll(s, &ch, DSSH_POLL_READ, 1);
		(void)r;
	}
	/* Also test with 999 ms to maximize nsec addition */
	int r = dssh_session_poll(s, &ch, DSSH_POLL_READ, 999);
	ASSERT_EQ(r, 0);

	dssh_sigqueue_free(&ch.buf.session.signals);
	dssh_bytebuf_free(&ch.buf.session.stdout_buf);
	dssh_bytebuf_free(&ch.buf.session.stderr_buf);
	cnd_destroy(&ch.poll_cnd);
	mtx_destroy(&ch.buf_mtx);
	dssh_session_cleanup(s);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

/*
 * dssh_session_accept nsec overflow (line 871):
 * Same pattern -- short timeout with large millisecond fraction.
 */
static int
test_accept_nsec_overflow(void)
{
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_FAIL;

	struct dssh_incoming_open *inc = NULL;
	/* 999 ms timeout -- adds 999000000 nsec, triggers overflow */
	for (int i = 0; i < 3; i++) {
		int res = dssh_session_accept(ctx.server, &inc, 1);
		(void)res;
	}
	int res = dssh_session_accept(ctx.server, &inc, 999);
	ASSERT_TRUE(res < 0 || inc == NULL);

	conn_cleanup(&ctx);
	return TEST_PASS;
}

/*
 * session_stderr_readable: to_mark < avail branch (line 1556).
 * Write 10 bytes to stderr, push a signal with stderr_pos=5,
 * leave stderr_consumed=0.  session_stderr_readable returns
 * min(10, 5-0) = 5, and to_mark(5) < avail(10) fires.
 */
static int
test_stderr_signal_truncate(void)
{
	dssh_test_reset_global_config();
	if (register_all_algorithms() < 0)
		return TEST_SKIP;
	dssh_session s = dssh_session_init(false, 0);
	if (s == NULL)
		return TEST_SKIP;

	struct dssh_channel_s ch = {0};
	ch.chan_type = DSSH_CHAN_SESSION;
	mtx_init(&ch.buf_mtx, mtx_plain);
	cnd_init(&ch.poll_cnd);
	dssh_bytebuf_init(&ch.buf.session.stdout_buf, 256);
	dssh_bytebuf_init(&ch.buf.session.stderr_buf, 256);
	dssh_sigqueue_init(&ch.buf.session.signals);

	/* Write 10 bytes to stderr */
	dssh_bytebuf_write(&ch.buf.session.stderr_buf,
	    (const uint8_t *)"0123456789", 10);

	/* Push a signal with stderr mark at position 5 */
	dssh_sigqueue_push(&ch.buf.session.signals, "TERM", 0, 5);

	/* Leave stderr_consumed at 0 -- to_mark = 5-0 = 5 < avail = 10 */
	ch.stderr_consumed = 0;

	/* read_ext should return only 5 bytes (clamped by signal mark) */
	uint8_t buf[64];
	int64_t n = dssh_session_read_ext(s, &ch, buf, sizeof(buf));
	ASSERT_EQ(n, (int64_t)5);
	ASSERT_MEM_EQ(buf, "01234", 5);

	dssh_sigqueue_free(&ch.buf.session.signals);
	dssh_bytebuf_free(&ch.buf.session.stdout_buf);
	dssh_bytebuf_free(&ch.buf.session.stderr_buf);
	cnd_destroy(&ch.poll_cnd);
	mtx_destroy(&ch.buf_mtx);
	dssh_session_cleanup(s);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

/* ================================================================
 * Subsystem (raw channel) roundtrip
 * ================================================================ */

static int
server_accept_subsys_raw_thread(void *arg)
{
	struct open_subsys_ctx *ctx = arg;
	struct dssh_incoming_open *inc = NULL;
	int res = dssh_session_accept(ctx->server, &inc, 5000);

	if (res < 0)
		return 0;

	/* Accept as session channel -- handles the subsystem request,
	 * returns a raw channel since the terminal request is "subsystem" */
	ctx->server_ch = dssh_session_accept_channel(ctx->server, inc,
	    &session_cbs, &ctx->req_type, &ctx->req_data);
	return 0;
}

static int
test_subsystem_roundtrip(void)
{
	struct conn_ctx ctx;

	if (conn_setup(&ctx) < 0)
		return TEST_SKIP;

	struct open_subsys_ctx oc = {
		.client = ctx.client,
		.server = ctx.server,
		.subsystem = "test-echo",
	};

	thrd_t ct, st;

	ASSERT_THRD_CREATE(&ct, client_open_subsys_thread, &oc);
	ASSERT_THRD_CREATE(&st, server_accept_subsys_raw_thread, &oc);
	thrd_join(ct, NULL);
	thrd_join(st, NULL);

	if (oc.client_ch == NULL || oc.server_ch == NULL) {
		if (oc.client_ch != NULL)
			dssh_channel_close(ctx.client, oc.client_ch);
		if (oc.server_ch != NULL)
			dssh_channel_close(ctx.server, oc.server_ch);
		conn_cleanup(&ctx);
		return TEST_SKIP;
	}

	ASSERT_STR_EQ(oc.req_type, "subsystem");
	ASSERT_STR_EQ(oc.req_data, "test-echo");

	/* Client sends a message to server via raw channel */
	static const uint8_t msg1[] = "hello from client";
	int w = dssh_channel_write(ctx.client, oc.client_ch,
	    msg1, sizeof(msg1));
	ASSERT_EQ(w, 0);

	/* Server polls and reads */
	int ev = dssh_channel_poll(ctx.server, oc.server_ch,
	    DSSH_POLL_READ, 5000);
	ASSERT_TRUE(ev & DSSH_POLL_READ);

	uint8_t buf[256];
	int64_t r = dssh_channel_read(ctx.server, oc.server_ch,
	    buf, sizeof(buf));
	ASSERT_EQ_U(r, sizeof(msg1));
	ASSERT_TRUE(memcmp(buf, msg1, sizeof(msg1)) == 0);

	/* Server sends reply back */
	static const uint8_t msg2[] = "hello from server";
	w = dssh_channel_write(ctx.server, oc.server_ch,
	    msg2, sizeof(msg2));
	ASSERT_EQ(w, 0);

	/* Client polls and reads */
	ev = dssh_channel_poll(ctx.client, oc.client_ch,
	    DSSH_POLL_READ, 5000);
	ASSERT_TRUE(ev & DSSH_POLL_READ);

	r = dssh_channel_read(ctx.client, oc.client_ch,
	    buf, sizeof(buf));
	ASSERT_EQ_U(r, sizeof(msg2));
	ASSERT_TRUE(memcmp(buf, msg2, sizeof(msg2)) == 0);

	/* Clean close */
	dssh_channel_close(ctx.client, oc.client_ch);
	dssh_channel_close(ctx.server, oc.server_ch);
	conn_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Test table
 * ================================================================ */

static struct dssh_test_entry tests[] = {
	/* Parse helpers */
	{ "parse/pty_req_valid",             test_parse_pty_req_valid },
	{ "parse/pty_req_no_modes",          test_parse_pty_req_no_modes },
	{ "parse/pty_req_modes_truncated",   test_parse_pty_req_modes_truncated },
	{ "parse/pty_req_truncated",         test_parse_pty_req_truncated },
	{ "parse/pty_req_empty",             test_parse_pty_req_empty },
	{ "parse/env_valid",                 test_parse_env_valid },
	{ "parse/env_truncated_name",        test_parse_env_truncated_name },
	{ "parse/env_truncated_value",       test_parse_env_truncated_value },
	{ "parse/env_empty",                 test_parse_env_empty },
	{ "parse/env_missing_value_header",  test_parse_env_missing_value_header },
	{ "parse/exec_valid",               test_parse_exec_valid },
	{ "parse/exec_truncated",           test_parse_exec_truncated },
	{ "parse/exec_empty",               test_parse_exec_empty },
	{ "parse/exec_zero_length",         test_parse_exec_zero_length },
	{ "parse/subsystem_valid",           test_parse_subsystem_valid },
	{ "parse/subsystem_truncated",       test_parse_subsystem_truncated },
	{ "parse/subsystem_empty",           test_parse_subsystem_empty },

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

	/* Coverage: edge cases */
	{ "test_session_start_twice",          test_session_start_twice },
	{ "test_accept_timeout_zero",          test_accept_timeout_zero },
	{ "test_accept_timeout_short",         test_accept_timeout_short },
	{ "test_reject_null_description",      test_reject_null_description },
	{ "test_session_poll_timeout_zero",    test_session_poll_timeout_zero },

	/* Auto-reject channel types */
	{ "test_auto_reject_x11",              test_auto_reject_x11 },
	{ "test_auto_reject_forwarded_tcpip",  test_auto_reject_forwarded_tcpip },
	{ "test_auto_reject_direct_tcpip",     test_auto_reject_direct_tcpip },
	{ "test_auto_reject_session_server",   test_auto_reject_session_from_server },

	/* Demux edge cases */
	{ "test_window_adjust_from_peer",      test_window_adjust_from_peer },
	{ "test_data_after_eof",               test_data_after_eof },
	{ "test_truncated_channel_data",       test_truncated_channel_data },
	{ "test_ext_data_after_eof",           test_ext_data_after_eof },
	{ "test_truncated_ext_data",           test_truncated_ext_data },
	{ "test_channel_request_want_reply",   test_channel_request_want_reply },
	{ "test_data_exceeds_window",          test_data_exceeds_window },

	/* Coverage: additional edge cases */
	{ "test_session_poll_write",            test_session_poll_write },
	{ "test_session_write_after_eof",       test_session_write_after_eof },
	{ "test_session_write_ext_after_eof",   test_session_write_ext_after_eof },
	{ "test_raw_channel_write_after_close", test_raw_channel_write_after_close },
	{ "test_raw_channel_write_toolong",     test_raw_channel_write_toolong },
	{ "test_raw_channel_poll_write",        test_raw_channel_poll_write },
	{ "test_raw_channel_poll_timeout_zero", test_raw_channel_poll_timeout_zero },
	{ "test_accept_raw",                    test_accept_raw },
	{ "test_reject_long_description",       test_reject_long_description },
	{ "test_read_signal_empty",             test_read_signal_empty },
	{ "test_signal_interleave_read",        test_signal_interleave_read },
	{ "test_signal_interleave_read_ext",    test_signal_interleave_read_ext },
	{ "test_truncated_channel_open",        test_truncated_channel_open },
	{ "test_truncated_channel_request",     test_truncated_channel_request },
	{ "test_demux_short_payload",           test_demux_short_payload },
	{ "test_session_poll_read_empty",       test_session_poll_read_empty },
	{ "test_session_write_window_zero",     test_session_write_window_zero },
	{ "test_session_write_ext_window_zero", test_session_write_ext_window_zero },
	{ "test_accept_timeout_negative",       test_accept_timeout_negative },
	{ "test_setup_exec_rejected_by_callback", test_setup_exec_rejected_by_callback },
	{ "test_truncated_open_confirmation",   test_truncated_open_confirmation },
	{ "test_open_conf_unknown_channel",    test_open_confirmation_unknown_channel },
	{ "test_channel_success_no_request",   test_channel_success_no_request },
	{ "test_double_eof_close",              test_double_eof_close },
	{ "test_data_dlen_exceeds_payload",     test_data_dlen_exceeds_payload },
	{ "test_send_ext_data_toolong",        test_send_extended_data_toolong },
	{ "test_demux_chan_type_zero",          test_demux_dispatch_chan_type_zero },
	{ "test_send_eof_already_sent",        test_send_eof_already_sent },
	{ "test_send_close_already_sent",      test_send_close_already_sent },
	{ "test_replenish_after_eof",          test_maybe_replenish_after_eof },
	{ "test_replenish_low_window",         test_maybe_replenish_low_window },
	{ "test_window_underflow_to_zero",     test_demux_window_underflow_to_zero },
	{ "test_window_add_overflow",          test_window_add_overflow },

	/* Defensive guard coverage */
	{ "test_session_write_not_open",       test_session_write_not_open },
	{ "test_session_write_ext_not_open",   test_session_write_ext_not_open },
	{ "test_session_write_close_recv",     test_session_write_close_received },
	{ "test_session_write_ext_close_recv", test_session_write_ext_close_received },
	{ "test_channel_write_not_open",       test_channel_write_not_open },
	{ "test_channel_write_close_recv",     test_channel_write_close_received },
	{ "test_channel_read_empty_queue",     test_channel_read_empty_queue },
	{ "test_channel_poll_eof",             test_channel_poll_eof },
	{ "test_channel_poll_close_received",  test_channel_poll_close_received },
	{ "test_session_poll_close_read",      test_session_poll_close_read },
	{ "test_session_poll_close_readext",   test_session_poll_close_readext },
	{ "test_stdout_signal_mark_consumed",  test_stdout_signal_mark_consumed },
	{ "test_stderr_signal_mark_consumed",  test_stderr_signal_mark_consumed },
	{ "test_chan_poll_infinite_ready",      test_channel_poll_infinite_data_ready },
	{ "test_sess_poll_infinite_ready",     test_session_poll_infinite_data_ready },
	{ "test_session_read_ext_empty",       test_session_read_ext_empty },

	{ "test_subsystem_roundtrip",          test_subsystem_roundtrip },

	/* Deterministic branch coverage -- profiling-unstable */
	{ "test_session_poll_nsec_overflow",   test_session_poll_nsec_overflow },
	{ "test_accept_nsec_overflow",         test_accept_nsec_overflow },
	{ "test_stderr_signal_truncate",       test_stderr_signal_truncate },
};

DSSH_TEST_MAIN(tests)
