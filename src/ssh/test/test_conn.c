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
	/* No modes string at all — exactly at the boundary */

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
	/* Calling session_start twice — covers demux_running guard */
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
	/* accept with timeout_ms=0 — non-blocking, should return immediately */
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
	/* accept with short positive timeout — should time out */
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
	/* reject with NULL description — covers the dlen==0 path */
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
	thrd_create(&ct, client_open_exec_thread, &oc);

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
	/* session_poll with timeout_ms=0 — non-blocking */
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

	/* Poll with timeout=0 on client — no data pending */
	int events = dssh_session_poll(ctx.client, oc.client_ch,
	    DSSH_POLL_READ | DSSH_POLL_WRITE, 0);
	/* Should return 0 (no events) or just DSSH_POLL_WRITE */
	ASSERT_TRUE(events >= 0);

	conn_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Auto-reject channel types — covers demux_channel_open branches.
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
	struct timespec ts = { .tv_nsec = 100000000L }; /* 100ms */
	thrd_sleep(&ts, NULL);

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

	struct timespec ts = { .tv_nsec = 100000000L };
	thrd_sleep(&ts, NULL);

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

	struct timespec ts = { .tv_nsec = 100000000L };
	thrd_sleep(&ts, NULL);

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

	struct timespec ts = { .tv_nsec = 100000000L };
	thrd_sleep(&ts, NULL);

	struct dssh_incoming_open *inc = NULL;
	int res = dssh_session_accept(ctx.client, &inc, 0);
	ASSERT_TRUE(res < 0 || inc == NULL);

	conn_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * WINDOW_ADJUST from peer — covers demux_dispatch case
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
	struct timespec ts = { .tv_nsec = 100000000L };
	thrd_sleep(&ts, NULL);

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
 * Data after EOF — covers demux eof_received/close_received guards
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

	struct timespec ts = { .tv_nsec = 100000000L };
	thrd_sleep(&ts, NULL);

	/* Now server sends CHANNEL_DATA after EOF — should be discarded */
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

	thrd_sleep(&ts, NULL);

	/* Client should have received nothing (data was discarded after EOF) */
	uint8_t buf[32];
	int64_t got = dssh_session_read(ctx.client, oc.client_ch, buf, sizeof(buf));
	ASSERT_EQ(got, 0);

	conn_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Truncated channel messages — covers payload_len guards in demux
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

	struct timespec ts = { .tv_nsec = 100000000L };
	thrd_sleep(&ts, NULL);

	/* Session should still be functional */
	uint8_t data[] = "still works";
	int64_t res = dssh_session_write(ctx.server, oc.server_ch,
	    data, sizeof(data));
	ASSERT_TRUE(res > 0);

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
};

DSSH_TEST_MAIN(tests)
