/*
 * test_conn.c -- Connection protocol tests for DeuceSSH.
 *
 * Tier 2 layer tests with mock I/O.  Covers session start/stop,
 * channel open/accept, stream and subsystem channel I/O, signals,
 * window change, close/exit, multiple channels, and thread safety.
 *
 * Uses the new dssh_chan_* API throughout.
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <threads.h>

#include "dssh_test.h"
#include "deucessh.h"
#include "deucessh-algorithms.h"
#include "deucessh-auth.h"
#include "deucessh-conn.h"
#include "ssh-trans.h"
#include "ssh-internal.h"
#include "dssh_test_internal.h"
#include "dssh_test_alloc.h"
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
	res = test_register_mac_algos();
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
 * Server accept callbacks (new dssh_chan_accept_cbs)
 * ================================================================ */

static int pty_received;
static char pty_term[64];
static uint32_t pty_cols, pty_rows;
static int wc_received;
static uint32_t wc_cols, wc_rows, wc_wpx, wc_hpx;

static int
accept_all_shell(dssh_channel ch, const struct dssh_chan_params *p,
    struct dssh_chan_accept_result *result, void *cbdata)
{
	(void)ch;
	(void)p;
	(void)result;
	(void)cbdata;
	return 0;
}

static int
accept_all_exec(dssh_channel ch, const struct dssh_chan_params *p,
    struct dssh_chan_accept_result *result, void *cbdata)
{
	(void)ch;
	(void)p;
	(void)result;
	(void)cbdata;
	return 0;
}

static int
accept_all_subsystem(dssh_channel ch, const struct dssh_chan_params *p,
    struct dssh_chan_accept_result *result, void *cbdata)
{
	(void)ch;
	(void)p;
	(void)result;
	(void)cbdata;
	return 0;
}

static int
accept_pty_cb(dssh_channel ch, const struct dssh_chan_params *p,
    void *cbdata)
{
	(void)ch;
	(void)cbdata;
	pty_received = 1;
	size_t n = strlen(p->term);
	if (n >= sizeof(pty_term))
		n = sizeof(pty_term) - 1;
	memcpy(pty_term, p->term, n);
	pty_term[n] = 0;
	pty_cols = p->cols;
	pty_rows = p->rows;
	return 0;
}

static struct dssh_chan_accept_cbs accept_cbs_all = {
	.pty_req = accept_pty_cb,
	.shell = accept_all_shell,
	.exec = accept_all_exec,
	.subsystem = accept_all_subsystem,
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
	size_t username_len = sizeof(username);
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
	const struct dssh_chan_accept_cbs *cbs;
	int accept_timeout;
};

static int
client_open_exec_thread(void *arg)
{
	struct open_exec_ctx *ctx = arg;
	struct dssh_chan_params p;
	dssh_chan_params_init(&p, DSSH_CHAN_EXEC);
	dssh_chan_params_set_command(&p, ctx->command);
	dssh_chan_params_set_pty(&p, false);
	ctx->client_ch = dssh_chan_open(ctx->client, &p);
	dssh_chan_params_free(&p);
	return 0;
}

static int
server_accept_exec_thread(void *arg)
{
	struct open_exec_ctx *ctx = arg;
	ctx->server_ch = dssh_chan_accept(ctx->server, ctx->cbs,
	    ctx->accept_timeout);
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
	const char *term;
	uint32_t cols, rows, wpx, hpx;
	const struct dssh_chan_accept_cbs *cbs;
};

static int
client_open_shell_thread(void *arg)
{
	struct open_shell_ctx *ctx = arg;
	struct dssh_chan_params p;
	dssh_chan_params_init(&p, DSSH_CHAN_SHELL);
	dssh_chan_params_set_term(&p, ctx->term);
	dssh_chan_params_set_size(&p, ctx->cols, ctx->rows, ctx->wpx, ctx->hpx);
	ctx->client_ch = dssh_chan_open(ctx->client, &p);
	dssh_chan_params_free(&p);
	return 0;
}

static int
server_accept_shell_thread(void *arg)
{
	struct open_shell_ctx *ctx = arg;
	ctx->server_ch = dssh_chan_accept(ctx->server, ctx->cbs, 30000);
	return 0;
}

/* ================================================================
 * Helper: open a subsystem channel concurrently
 * ================================================================ */

struct open_subsys_ctx {
	dssh_session client;
	dssh_session server;
	dssh_channel client_ch;
	dssh_channel server_ch;
	const char *subsystem;
};

static int
client_open_subsys_thread(void *arg)
{
	struct open_subsys_ctx *ctx = arg;
	struct dssh_chan_params p;
	dssh_chan_params_init(&p, DSSH_CHAN_SUBSYSTEM);
	dssh_chan_params_set_subsystem(&p, ctx->subsystem);
	dssh_chan_params_set_pty(&p, false);
	ctx->client_ch = dssh_chan_open(ctx->client, &p);
	dssh_chan_params_free(&p);
	return 0;
}

static int
server_accept_subsys_thread(void *arg)
{
	struct open_subsys_ctx *ctx = arg;
	ctx->server_ch = dssh_chan_accept(ctx->server, &accept_cbs_all, 30000);
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
		.cbs = &accept_cbs_all,
		.accept_timeout = 30000,
	};

	if (open_exec_channel(&oc) < 0) {
		conn_cleanup(&ctx);
		return TEST_FAIL;
	}

	ASSERT_NOT_NULL(oc.client_ch);
	ASSERT_NOT_NULL(oc.server_ch);

	dssh_chan_close(oc.client_ch, 0);
	dssh_chan_close(oc.server_ch, 0);
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
		.cbs = &accept_cbs_all,
		.accept_timeout = 30000,
	};

	if (open_exec_channel(&oc) < 0) {
		conn_cleanup(&ctx);
		return TEST_FAIL;
	}

	/* Verify the command arrived on the server side */
	ASSERT_EQ(dssh_chan_get_type(oc.server_ch), DSSH_CHAN_EXEC);
	ASSERT_NOT_NULL(dssh_chan_get_command(oc.server_ch));
	ASSERT_STR_EQ(dssh_chan_get_command(oc.server_ch), "ls -la /tmp");

	dssh_chan_close(oc.client_ch, 0);
	dssh_chan_close(oc.server_ch, 0);
	conn_cleanup(&ctx);
	return TEST_PASS;
}

/* Reject callback for exec -- returns -1 */
static int
reject_exec_cb(dssh_channel ch, const struct dssh_chan_params *p,
    struct dssh_chan_accept_result *result, void *cbdata)
{
	(void)ch;
	(void)p;
	(void)result;
	(void)cbdata;
	return -1;
}

static struct dssh_chan_accept_cbs reject_exec_cbs = {
	.shell = accept_all_shell,
	.exec = reject_exec_cb,
	.subsystem = accept_all_subsystem,
};

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
		.cbs = &reject_exec_cbs,
		.accept_timeout = 30000,
	};

	thrd_t ct, st;
	ASSERT_THRD_CREATE(&ct, client_open_exec_thread, &oc);
	ASSERT_THRD_CREATE(&st, server_accept_exec_thread, &oc);
	thrd_join(ct, NULL);

	/* After rejection, dssh_chan_accept loops back to wait for the
	 * next CHANNEL_OPEN.  Terminate to unblock it. */
	session_set_terminate(ctx.server);
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

	struct open_shell_ctx oc = {
		.client = ctx.client,
		.server = ctx.server,
		.term = "xterm-256color",
		.cols = 80,
		.rows = 24,
		.wpx = 0,
		.hpx = 0,
		.cbs = &accept_cbs_all,
	};

	thrd_t ct, st;
	ASSERT_THRD_CREATE(&ct, client_open_shell_thread, &oc);
	ASSERT_THRD_CREATE(&st, server_accept_shell_thread, &oc);
	thrd_join(ct, NULL);
	thrd_join(st, NULL);

	ASSERT_NOT_NULL(oc.client_ch);
	ASSERT_NOT_NULL(oc.server_ch);
	ASSERT_EQ(dssh_chan_get_type(oc.server_ch), DSSH_CHAN_SHELL);
	ASSERT_TRUE(pty_received);
	ASSERT_STR_EQ(pty_term, "xterm-256color");
	ASSERT_EQ_U(pty_cols, 80);
	ASSERT_EQ_U(pty_rows, 24);

	dssh_chan_close(oc.client_ch, 0);
	dssh_chan_close(oc.server_ch, 0);
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

	dssh_chan_close(oc.client_ch, -1);
	dssh_chan_close(oc.server_ch, -1);
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
		.cbs = &accept_cbs_all,
		.accept_timeout = 30000,
	};
	if (open_exec_channel(&oc) < 0) {
		conn_cleanup(&ctx);
		return TEST_SKIP;
	}

	const uint8_t msg[] = "Hello, server!";
	int64_t written = dssh_chan_write(oc.client_ch, 0,
	    msg, sizeof(msg) - 1);
	ASSERT_TRUE(written > 0);

	int ev = dssh_chan_poll(oc.server_ch, DSSH_POLL_READ, 5000);
	ASSERT_TRUE(ev & DSSH_POLL_READ);

	uint8_t buf[256];
	int64_t nread = dssh_chan_read(oc.server_ch, 0,
	    buf, sizeof(buf));
	ASSERT_TRUE(nread > 0);
	ASSERT_EQ(nread, (int64_t)(sizeof(msg) - 1));
	ASSERT_MEM_EQ(buf, msg, sizeof(msg) - 1);

	dssh_chan_close(oc.client_ch, 0);
	dssh_chan_close(oc.server_ch, 0);
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
		.cbs = &accept_cbs_all,
		.accept_timeout = 30000,
	};
	if (open_exec_channel(&oc) < 0) {
		conn_cleanup(&ctx);
		return TEST_SKIP;
	}

	const uint8_t msg[] = "Hello, client!";
	int64_t written = dssh_chan_write(oc.server_ch, 0,
	    msg, sizeof(msg) - 1);
	ASSERT_TRUE(written > 0);

	int ev = dssh_chan_poll(oc.client_ch, DSSH_POLL_READ, 5000);
	ASSERT_TRUE(ev & DSSH_POLL_READ);

	uint8_t buf[256];
	int64_t nread = dssh_chan_read(oc.client_ch, 0,
	    buf, sizeof(buf));
	ASSERT_TRUE(nread > 0);
	ASSERT_EQ(nread, (int64_t)(sizeof(msg) - 1));
	ASSERT_MEM_EQ(buf, msg, sizeof(msg) - 1);

	dssh_chan_close(oc.client_ch, 0);
	dssh_chan_close(oc.server_ch, 0);
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
		.cbs = &accept_cbs_all,
		.accept_timeout = 30000,
	};
	if (open_exec_channel(&oc) < 0) {
		conn_cleanup(&ctx);
		return TEST_SKIP;
	}

	const uint8_t stderr_msg[] = "error output";
	int64_t written = dssh_chan_write(oc.server_ch, 1,
	    stderr_msg, sizeof(stderr_msg) - 1);
	ASSERT_TRUE(written > 0);

	int ev = dssh_chan_poll(oc.client_ch, DSSH_POLL_READEXT, 5000);
	ASSERT_TRUE(ev & DSSH_POLL_READEXT);

	uint8_t buf[256];
	int64_t nread = dssh_chan_read(oc.client_ch, 1,
	    buf, sizeof(buf));
	ASSERT_TRUE(nread > 0);
	ASSERT_EQ(nread, (int64_t)(sizeof(stderr_msg) - 1));
	ASSERT_MEM_EQ(buf, stderr_msg, sizeof(stderr_msg) - 1);

	dssh_chan_close(oc.client_ch, 0);
	dssh_chan_close(oc.server_ch, 0);
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
		.cbs = &accept_cbs_all,
		.accept_timeout = 30000,
	};
	if (open_exec_channel(&oc) < 0) {
		conn_cleanup(&ctx);
		return TEST_SKIP;
	}

	const uint8_t stderr_msg[] = "client stderr";
	int64_t written = dssh_chan_write(oc.client_ch, 1,
	    stderr_msg, sizeof(stderr_msg) - 1);
	ASSERT_TRUE(written > 0);

	int ev = dssh_chan_poll(oc.server_ch, DSSH_POLL_READEXT, 5000);
	ASSERT_TRUE(ev & DSSH_POLL_READEXT);

	uint8_t buf[256];
	int64_t nread = dssh_chan_read(oc.server_ch, 1,
	    buf, sizeof(buf));
	ASSERT_TRUE(nread > 0);
	ASSERT_MEM_EQ(buf, stderr_msg, sizeof(stderr_msg) - 1);

	dssh_chan_close(oc.client_ch, 0);
	dssh_chan_close(oc.server_ch, 0);
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
		.cbs = &accept_cbs_all,
		.accept_timeout = 30000,
	};
	if (open_exec_channel(&oc) < 0) {
		conn_cleanup(&ctx);
		return TEST_SKIP;
	}

	const uint8_t msg[] = "data";
	dssh_chan_write(oc.client_ch, 0, msg, sizeof(msg) - 1);

	int ev = dssh_chan_poll(oc.server_ch, DSSH_POLL_READ, 5000);
	ASSERT_TRUE(ev & DSSH_POLL_READ);

	dssh_chan_close(oc.client_ch, 0);
	dssh_chan_close(oc.server_ch, 0);
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
		.cbs = &accept_cbs_all,
		.accept_timeout = 30000,
	};
	if (open_exec_channel(&oc) < 0) {
		conn_cleanup(&ctx);
		return TEST_SKIP;
	}

	int ev = dssh_chan_poll(oc.server_ch, DSSH_POLL_READ, 100);
	ASSERT_EQ(ev, 0);

	dssh_chan_close(oc.client_ch, 0);
	dssh_chan_close(oc.server_ch, 0);
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
		.cbs = &accept_cbs_all,
		.accept_timeout = 30000,
	};
	if (open_exec_channel(&oc) < 0) {
		conn_cleanup(&ctx);
		return TEST_SKIP;
	}

	size_t total = 32768;
	uint8_t *data = malloc(total);
	if (data == NULL) {
		dssh_chan_close(oc.client_ch, 0);
		dssh_chan_close(oc.server_ch, 0);
		conn_cleanup(&ctx);
		return TEST_SKIP;
	}
	for (size_t i = 0; i < total; i++)
		data[i] = (uint8_t)(i & 0xFF);

	int64_t written = dssh_chan_write(oc.client_ch, 0, data, total);
	ASSERT_TRUE(written > 0);

	uint8_t *readbuf = malloc(total);
	if (readbuf == NULL) {
		free(data);
		dssh_chan_close(oc.client_ch, 0);
		dssh_chan_close(oc.server_ch, 0);
		conn_cleanup(&ctx);
		return TEST_SKIP;
	}

	size_t got = 0;
	while (got < (size_t)written) {
		int ev = dssh_chan_poll(oc.server_ch, DSSH_POLL_READ, 5000);
		if (ev <= 0)
			break;
		int64_t n = dssh_chan_read(oc.server_ch, 0,
		    &readbuf[got], total - got);
		if (n <= 0)
			break;
		got += (size_t)n;
	}

	ASSERT_EQ_U(got, (size_t)written);
	ASSERT_MEM_EQ(readbuf, data, got);

	free(data);
	free(readbuf);
	dssh_chan_close(oc.client_ch, 0);
	dssh_chan_close(oc.server_ch, 0);
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
		.cbs = &accept_cbs_all,
		.accept_timeout = 30000,
	};
	if (open_exec_channel(&oc) < 0) {
		conn_cleanup(&ctx);
		return TEST_SKIP;
	}

	const uint8_t m1[] = "AAA";
	const uint8_t m2[] = "BBB";
	const uint8_t m3[] = "CCC";
	dssh_chan_write(oc.client_ch, 0, m1, 3);
	dssh_chan_write(oc.client_ch, 0, m2, 3);
	dssh_chan_write(oc.client_ch, 0, m3, 3);

	uint8_t buf[256];
	size_t got = 0;
	while (got < 9) {
		int ev = dssh_chan_poll(oc.server_ch, DSSH_POLL_READ, 5000);
		if (ev <= 0)
			break;
		int64_t n = dssh_chan_read(oc.server_ch, 0,
		    &buf[got], sizeof(buf) - got);
		if (n <= 0)
			break;
		got += (size_t)n;
	}
	ASSERT_EQ_U(got, 9);
	ASSERT_MEM_EQ(buf, "AAABBBCCC", 9);

	dssh_chan_close(oc.client_ch, 0);
	dssh_chan_close(oc.server_ch, 0);
	conn_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Subsystem channel I/O (~4 tests)
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
	ASSERT_EQ(dssh_chan_get_type(oc.server_ch), DSSH_CHAN_SUBSYSTEM);

	/* Both sides use stream API now */
	const uint8_t msg[] = "subsystem message";
	int64_t written = dssh_chan_write(oc.client_ch, 0,
	    msg, sizeof(msg) - 1);
	ASSERT_TRUE(written > 0);

	int ev = dssh_chan_poll(oc.server_ch, DSSH_POLL_READ, 5000);
	ASSERT_TRUE(ev & DSSH_POLL_READ);

	uint8_t buf[256];
	int64_t nread = dssh_chan_read(oc.server_ch, 0,
	    buf, sizeof(buf));
	ASSERT_TRUE(nread > 0);
	ASSERT_EQ(nread, (int64_t)(sizeof(msg) - 1));
	ASSERT_MEM_EQ(buf, msg, sizeof(msg) - 1);

	dssh_chan_close(oc.client_ch, -1);
	dssh_chan_close(oc.server_ch, -1);
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

	/* Stream API: writes are concatenated, not message-delimited */
	dssh_chan_write(oc.client_ch, 0,
	    (const uint8_t *)"first", 5);
	dssh_chan_write(oc.client_ch, 0,
	    (const uint8_t *)"second", 6);
	dssh_chan_write(oc.client_ch, 0,
	    (const uint8_t *)"third", 5);

	uint8_t buf[256];
	size_t got = 0;

	while (got < 16) {
		int ev = dssh_chan_poll(oc.server_ch, DSSH_POLL_READ, 5000);
		if (ev <= 0)
			break;
		int64_t n = dssh_chan_read(oc.server_ch, 0,
		    &buf[got], sizeof(buf) - got);
		if (n <= 0)
			break;
		got += (size_t)n;
	}
	ASSERT_EQ_U(got, 16);
	ASSERT_MEM_EQ(buf, "firstsecondthird", 16);

	dssh_chan_close(oc.client_ch, -1);
	dssh_chan_close(oc.server_ch, -1);
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
	int ev = dssh_chan_poll(oc.server_ch, DSSH_POLL_READ, 100);
	ASSERT_EQ(ev, 0);

	/* Send data, poll should return readable */
	dssh_chan_write(oc.client_ch, 0,
	    (const uint8_t *)"x", 1);
	ev = dssh_chan_poll(oc.server_ch, DSSH_POLL_READ, 5000);
	ASSERT_TRUE(ev & DSSH_POLL_READ);

	uint8_t buf[8];
	dssh_chan_read(oc.server_ch, 0, buf, sizeof(buf));

	dssh_chan_close(oc.client_ch, -1);
	dssh_chan_close(oc.server_ch, -1);
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

	int64_t written = dssh_chan_write(oc.client_ch, 0,
	    data, msglen);
	ASSERT_TRUE(written > 0);

	uint8_t *readbuf = malloc(msglen);
	ASSERT_NOT_NULL(readbuf);

	size_t got = 0;
	while (got < (size_t)written) {
		int ev = dssh_chan_poll(oc.server_ch, DSSH_POLL_READ, 5000);
		if (ev <= 0)
			break;
		int64_t n = dssh_chan_read(oc.server_ch, 0,
		    &readbuf[got], msglen - got);
		if (n <= 0)
			break;
		got += (size_t)n;
	}
	ASSERT_TRUE(got > 0);
	ASSERT_MEM_EQ(readbuf, data, got);

	free(data);
	free(readbuf);
	dssh_chan_close(oc.client_ch, -1);
	dssh_chan_close(oc.server_ch, -1);
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
		.cbs = &accept_cbs_all,
		.accept_timeout = 30000,
	};
	if (open_exec_channel(&oc) < 0) {
		conn_cleanup(&ctx);
		return TEST_SKIP;
	}

	int res = dssh_chan_send_signal(oc.client_ch, "INT");
	ASSERT_OK(res);

	int ev = dssh_chan_poll(oc.server_ch,
	    DSSH_POLL_EVENT, 5000);
	ASSERT_TRUE(ev & DSSH_POLL_EVENT);

	struct dssh_chan_event event;
	res = dssh_chan_read_event(oc.server_ch, &event);
	ASSERT_OK(res);
	ASSERT_EQ(event.type, DSSH_EVENT_SIGNAL);
	ASSERT_NOT_NULL(event.signal.name);
	ASSERT_STR_EQ(event.signal.name, "INT");

	dssh_chan_close(oc.client_ch, 0);
	dssh_chan_close(oc.server_ch, 0);
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
		.cbs = &accept_cbs_all,
		.accept_timeout = 30000,
	};
	if (open_exec_channel(&oc) < 0) {
		conn_cleanup(&ctx);
		return TEST_SKIP;
	}

	/* Send data, then a signal */
	const uint8_t msg[] = "before signal";
	dssh_chan_write(oc.client_ch, 0, msg, sizeof(msg) - 1);
	dssh_chan_send_signal(oc.client_ch, "TERM");

	/* Drain data first */
	uint8_t buf[256];
	size_t got = 0;
	while (got < sizeof(msg) - 1) {
		int ev = dssh_chan_poll(oc.server_ch,
		    DSSH_POLL_READ, 5000);
		if (ev <= 0)
			break;
		int64_t n = dssh_chan_read(oc.server_ch, 0,
		    &buf[got], sizeof(buf) - got);
		if (n <= 0)
			break;
		got += (size_t)n;
	}
	ASSERT_EQ_U(got, sizeof(msg) - 1);

	/* Now event should be ready */
	int ev = dssh_chan_poll(oc.server_ch,
	    DSSH_POLL_EVENT, 5000);
	ASSERT_TRUE(ev & DSSH_POLL_EVENT);

	struct dssh_chan_event event;
	dssh_chan_read_event(oc.server_ch, &event);
	ASSERT_EQ(event.type, DSSH_EVENT_SIGNAL);
	ASSERT_NOT_NULL(event.signal.name);
	ASSERT_STR_EQ(event.signal.name, "TERM");

	dssh_chan_close(oc.client_ch, 0);
	dssh_chan_close(oc.server_ch, 0);
	conn_cleanup(&ctx);
	return TEST_PASS;
}

static int
test_window_change(void)
{
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_SKIP;

	struct open_shell_ctx oc = {
		.client = ctx.client,
		.server = ctx.server,
		.term = "vt100",
		.cols = 80,
		.rows = 24,
		.cbs = &accept_cbs_all,
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

	int res = dssh_chan_send_window_change(oc.client_ch,
	    132, 50, 1056, 800);
	ASSERT_OK(res);

	/* Poll for window-change event */
	int ev = dssh_chan_poll(oc.server_ch,
	    DSSH_POLL_EVENT, 5000);
	ASSERT_TRUE(ev & DSSH_POLL_EVENT);

	struct dssh_chan_event event;
	res = dssh_chan_read_event(oc.server_ch, &event);
	ASSERT_OK(res);
	ASSERT_EQ(event.type, DSSH_EVENT_WINDOW_CHANGE);
	ASSERT_EQ_U(event.window_change.cols, 132);
	ASSERT_EQ_U(event.window_change.rows, 50);
	ASSERT_EQ_U(event.window_change.wpx, 1056);
	ASSERT_EQ_U(event.window_change.hpx, 800);

	dssh_chan_close(oc.client_ch, 0);
	dssh_chan_close(oc.server_ch, 0);
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
		.cbs = &accept_cbs_all,
		.accept_timeout = 30000,
	};
	if (open_exec_channel(&oc) < 0) {
		conn_cleanup(&ctx);
		return TEST_SKIP;
	}

	/* Server closes with exit code 42 */
	int res = dssh_chan_close(oc.server_ch, 42);
	ASSERT_OK(res);

	/* Client should see close; give demux time */
	struct timespec ts = { .tv_sec = 0, .tv_nsec = 20000000 };
	thrd_sleep(&ts, NULL);

	/* Client reads should return 0 (EOF) */
	uint8_t buf[64];
	int ev = dssh_chan_poll(oc.client_ch,
	    DSSH_POLL_READ, 1000);
	if (ev & DSSH_POLL_READ) {
		int64_t n = dssh_chan_read(oc.client_ch, 0,
		    buf, sizeof(buf));
		ASSERT_EQ(n, 0);
	}

	dssh_chan_close(oc.client_ch, 0);
	conn_cleanup(&ctx);
	return TEST_PASS;
}

static int
test_channel_close_subsys(void)
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

	int res = dssh_chan_close(oc.client_ch, -1);
	ASSERT_OK(res);

	/* Server should see close */
	struct timespec ts = { .tv_sec = 0, .tv_nsec = 20000000 };
	thrd_sleep(&ts, NULL);

	dssh_chan_close(oc.server_ch, -1);
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
		.cbs = &accept_cbs_all,
		.accept_timeout = 30000,
	};
	if (open_exec_channel(&oc) < 0) {
		conn_cleanup(&ctx);
		return TEST_SKIP;
	}

	/* Server writes data, then closes (sends exit-status + EOF + CLOSE) */
	const uint8_t msg[] = "final output";
	dssh_chan_write(oc.server_ch, 0, msg, sizeof(msg) - 1);
	dssh_chan_close(oc.server_ch, 0);

	/* Client reads data */
	uint8_t buf[256];
	size_t got = 0;
	for (;;) {
		int ev = dssh_chan_poll(oc.client_ch,
		    DSSH_POLL_READ, 5000);
		if (ev <= 0)
			break;
		int64_t n = dssh_chan_read(oc.client_ch, 0,
		    buf + got, sizeof(buf) - got);
		if (n <= 0)
			break;
		got += (size_t)n;
	}
	ASSERT_TRUE(got >= sizeof(msg) - 1);
	ASSERT_MEM_EQ(buf, msg, sizeof(msg) - 1);

	dssh_chan_close(oc.client_ch, 0);
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
		.cbs = &accept_cbs_all,
		.accept_timeout = 30000,
	};
	if (open_exec_channel(&oc) < 0) {
		conn_cleanup(&ctx);
		return TEST_SKIP;
	}

	/* Server sends data */
	const uint8_t msg[] = "server data after client eof";
	dssh_chan_write(oc.server_ch, 0, msg, sizeof(msg) - 1);

	/* Client should be able to read */
	uint8_t buf[256];
	size_t got = 0;
	while (got < sizeof(msg) - 1) {
		int ev = dssh_chan_poll(oc.client_ch,
		    DSSH_POLL_READ, 1000);
		if (ev <= 0)
			break;
		int64_t n = dssh_chan_read(oc.client_ch, 0,
		    &buf[got], sizeof(buf) - got);
		if (n <= 0)
			break;
		got += (size_t)n;
	}
	ASSERT_EQ_U(got, sizeof(msg) - 1);
	ASSERT_MEM_EQ(buf, msg, sizeof(msg) - 1);

	dssh_chan_close(oc.client_ch, 0);
	dssh_chan_close(oc.server_ch, 0);
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
		.cbs = &accept_cbs_all,
		.accept_timeout = 30000,
	};
	if (open_exec_channel(&oc) < 0) {
		conn_cleanup(&ctx);
		return TEST_SKIP;
	}

	/* Client terminates the session */
	dssh_session_terminate(ctx.client);

	/* Give demux time to see termination */
	struct timespec ts = { .tv_sec = 0, .tv_nsec = 20000000 };
	thrd_sleep(&ts, NULL);

	/* Server poll should return error or see termination */
	int ev = dssh_chan_poll(oc.server_ch, DSSH_POLL_READ, 500);
	/* Either an error or 0 timeout is acceptable after peer disconnect */
	ASSERT_TRUE(ev <= 0 || (ev & DSSH_POLL_READ));

	/* Clean up without calling chan_close for already-terminated channels */
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
	const struct dssh_chan_accept_cbs *cbs;
};

static int
server_accept_two_channels(void *arg)
{
	struct two_channel_server_ctx *sctx = arg;

	sctx->ch1 = dssh_chan_accept(sctx->server, sctx->cbs, 30000);
	sctx->ch2 = dssh_chan_accept(sctx->server, sctx->cbs, 30000);

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
		.cbs = &accept_cbs_all,
	};

	thrd_t st;
	ASSERT_THRD_CREATE(&st, server_accept_two_channels, &sctx);

	struct dssh_chan_params p1;
	dssh_chan_params_init(&p1, DSSH_CHAN_EXEC);
	dssh_chan_params_set_command(&p1, "cmd1");
	dssh_chan_params_set_pty(&p1, false);
	dssh_channel c1 = dssh_chan_open(ctx.client, &p1);
	dssh_chan_params_free(&p1);

	struct dssh_chan_params p2;
	dssh_chan_params_init(&p2, DSSH_CHAN_EXEC);
	dssh_chan_params_set_command(&p2, "cmd2");
	dssh_chan_params_set_pty(&p2, false);
	dssh_channel c2 = dssh_chan_open(ctx.client, &p2);
	dssh_chan_params_free(&p2);

	thrd_join(st, NULL);

	if (c1 == NULL || c2 == NULL || sctx.ch1 == NULL || sctx.ch2 == NULL) {
		if (c1)
			dssh_chan_close(c1, 0);
		if (c2)
			dssh_chan_close(c2, 0);
		if (sctx.ch1)
			dssh_chan_close(sctx.ch1, 0);
		if (sctx.ch2)
			dssh_chan_close(sctx.ch2, 0);
		conn_cleanup(&ctx);
		return TEST_FAIL;
	}

	/* Write on channel 1, read on channel 1 */
	const uint8_t m1[] = "chan1data";
	dssh_chan_write(c1, 0, m1, 8);

	/* Write on channel 2, read on channel 2 */
	const uint8_t m2[] = "chan2data";
	dssh_chan_write(c2, 0, m2, 8);

	uint8_t buf[256];

	int ev = dssh_chan_poll(sctx.ch1, DSSH_POLL_READ, 5000);
	ASSERT_TRUE(ev & DSSH_POLL_READ);
	int64_t n = dssh_chan_read(sctx.ch1, 0, buf, sizeof(buf));
	ASSERT_EQ(n, 8);
	ASSERT_MEM_EQ(buf, "chan1data", 8);

	ev = dssh_chan_poll(sctx.ch2, DSSH_POLL_READ, 5000);
	ASSERT_TRUE(ev & DSSH_POLL_READ);
	n = dssh_chan_read(sctx.ch2, 0, buf, sizeof(buf));
	ASSERT_EQ(n, 8);
	ASSERT_MEM_EQ(buf, "chan2data", 8);

	dssh_chan_close(c1, 0);
	dssh_chan_close(c2, 0);
	dssh_chan_close(sctx.ch1, 0);
	dssh_chan_close(sctx.ch2, 0);
	conn_cleanup(&ctx);
	return TEST_PASS;
}

/*
 * Accept-reject test: server calls dssh_chan_accept once with
 * reject_exec_cbs.  Client opens exec (rejected by callback),
 * then shell (accepted).  Accept loops internally past the
 * rejected channel and returns the shell channel.
 */
struct accept_reject_server_ctx {
	dssh_session server;
	dssh_channel ch;
};

static int
server_accept_loops_on_reject(void *arg)
{
	struct accept_reject_server_ctx *sctx = arg;

	/* Single accept: loops past rejected exec, returns shell */
	sctx->ch = dssh_chan_accept(sctx->server,
	    &reject_exec_cbs, 30000);

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
	};

	thrd_t st;
	ASSERT_THRD_CREATE(&st, server_accept_loops_on_reject, &sctx);

	/* First channel: exec, rejected by server callback */
	struct dssh_chan_params p1;
	dssh_chan_params_init(&p1, DSSH_CHAN_EXEC);
	dssh_chan_params_set_command(&p1, "rejected");
	dssh_chan_params_set_pty(&p1, false);
	dssh_channel c1 = dssh_chan_open(ctx.client, &p1);
	dssh_chan_params_free(&p1);

	/* Second channel: shell, accepted */
	struct dssh_chan_params p2;
	dssh_chan_params_init(&p2, DSSH_CHAN_SHELL);
	dssh_chan_params_set_pty(&p2, false);
	dssh_channel c2 = dssh_chan_open(ctx.client, &p2);
	dssh_chan_params_free(&p2);

	thrd_join(st, NULL);

	ASSERT_NULL(c1);
	ASSERT_NOT_NULL(c2);
	ASSERT_NOT_NULL(sctx.ch);
	ASSERT_EQ(DSSH_CHAN_SHELL, dssh_chan_get_type(sctx.ch));

	dssh_chan_close(c2, 0);
	dssh_chan_close(sctx.ch, 0);
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
		.cbs = &accept_cbs_all,
	};

	thrd_t st;
	ASSERT_THRD_CREATE(&st, server_accept_two_channels, &sctx);

	struct dssh_chan_params p1;
	dssh_chan_params_init(&p1, DSSH_CHAN_EXEC);
	dssh_chan_params_set_command(&p1, "cmd1");
	dssh_chan_params_set_pty(&p1, false);
	dssh_channel c1 = dssh_chan_open(ctx.client, &p1);
	dssh_chan_params_free(&p1);

	struct dssh_chan_params p2;
	dssh_chan_params_init(&p2, DSSH_CHAN_EXEC);
	dssh_chan_params_set_command(&p2, "cmd2");
	dssh_chan_params_set_pty(&p2, false);
	dssh_channel c2 = dssh_chan_open(ctx.client, &p2);
	dssh_chan_params_free(&p2);

	thrd_join(st, NULL);

	if (c1 == NULL || c2 == NULL || sctx.ch1 == NULL || sctx.ch2 == NULL) {
		if (c1)
			dssh_chan_close(c1, 0);
		if (c2)
			dssh_chan_close(c2, 0);
		if (sctx.ch1)
			dssh_chan_close(sctx.ch1, 0);
		if (sctx.ch2)
			dssh_chan_close(sctx.ch2, 0);
		conn_cleanup(&ctx);
		return TEST_FAIL;
	}

	/* Close channel 1 */
	dssh_chan_close(c1, 0);
	dssh_chan_close(sctx.ch1, 0);

	/* Channel 2 should still work */
	const uint8_t msg[] = "still alive";
	dssh_chan_write(c2, 0, msg, sizeof(msg) - 1);

	uint8_t buf[256];
	int ev = dssh_chan_poll(sctx.ch2, DSSH_POLL_READ, 5000);
	ASSERT_TRUE(ev & DSSH_POLL_READ);
	int64_t n = dssh_chan_read(sctx.ch2, 0, buf, sizeof(buf));
	ASSERT_TRUE(n > 0);
	ASSERT_MEM_EQ(buf, "still alive", (size_t)n);

	dssh_chan_close(c2, 0);
	dssh_chan_close(sctx.ch2, 0);
	conn_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Thread safety (~2 tests)
 * ================================================================ */

struct writer_ctx {
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
		int ev = dssh_chan_poll(wc->ch, DSSH_POLL_WRITE, 5000);
		if (ev < 0) {
			wc->result = ev;
			break;
		}
		if (!(ev & DSSH_POLL_WRITE))
			continue;
		int64_t w = dssh_chan_write(wc->ch, 0, buf, sizeof(buf));
		if (w < 0) {
			wc->result = (int)w;
			break;
		}
		wc->total_written += (size_t)w;
	}
	return 0;
}

struct reader_ctx {
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
		int ev = dssh_chan_poll(rc->ch, DSSH_POLL_READ, 2000);
		if (ev <= 0)
			break;
		if (!(ev & DSSH_POLL_READ))
			continue;
		int64_t n = dssh_chan_read(rc->ch, 0, buf, sizeof(buf));
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
		.cbs = &accept_cbs_all,
		.accept_timeout = 30000,
	};
	if (open_exec_channel(&oc) < 0) {
		conn_cleanup(&ctx);
		return TEST_SKIP;
	}

	struct writer_ctx wc = {
		.ch = oc.client_ch,
	};
	struct reader_ctx rc = {
		.ch = oc.server_ch,
	};

	thrd_t wt, rt;
	ASSERT_THRD_CREATE(&rt, reader_thread_func, &rc);
	ASSERT_THRD_CREATE(&wt, writer_thread_func, &wc);

	thrd_join(wt, NULL);

	/* After writer finishes, close the channel so reader sees EOF */
	dssh_chan_close(oc.client_ch, 0);

	thrd_join(rt, NULL);

	ASSERT_TRUE(wc.total_written > 0);
	ASSERT_TRUE(rc.total_read > 0);
	/* Reader should have received at least some of what was written */
	ASSERT_TRUE(rc.total_read <= wc.total_written);

	dssh_chan_close(oc.server_ch, 0);
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
		.cbs = &accept_cbs_all,
		.accept_timeout = 30000,
	};
	if (open_exec_channel(&oc) < 0) {
		conn_cleanup(&ctx);
		return TEST_SKIP;
	}

	/* Start a reader that will block on poll */
	struct reader_ctx rc = {
		.ch = oc.server_ch,
	};
	thrd_t rt;
	ASSERT_THRD_CREATE(&rt, reader_thread_func, &rc);

	/* Brief delay, then terminate */
	struct timespec ts = { .tv_sec = 0, .tv_nsec = 10000000 };
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
		.cbs = &accept_cbs_all,
		.accept_timeout = 30000,
	};
	if (open_exec_channel(&oc) < 0) {
		conn_cleanup(&ctx);
		return TEST_SKIP;
	}

	/* Start a reader blocked on poll */
	struct reader_ctx rc = {
		.ch = oc.server_ch,
	};
	thrd_t rt;
	ASSERT_THRD_CREATE(&rt, reader_thread_func, &rc);

	/* Brief delay to let reader block */
	struct timespec ts = { .tv_sec = 0, .tv_nsec = 10000000 };
	thrd_sleep(&ts, NULL);

	/* Kill the connection */
	mock_io_close_c2s(&ctx.io);
	mock_io_close_s2c(&ctx.io);

	thrd_join(rt, NULL);

	/* Session should be terminated */
	ASSERT_TRUE(dssh_session_is_terminated(ctx.server));

	/* Cleanup must not crash */
	conn_cleanup(&ctx);
	return TEST_PASS;
}

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
		.cbs = &accept_cbs_all,
		.accept_timeout = 30000,
	};
	if (open_exec_channel(&oc) < 0) {
		conn_cleanup(&ctx);
		return TEST_SKIP;
	}

	/* Kill the connection */
	mock_io_close_c2s(&ctx.io);
	mock_io_close_s2c(&ctx.io);

	/* Brief delay for demux to notice I/O error */
	struct timespec ts = { .tv_sec = 0, .tv_nsec = 100000000 };
	thrd_sleep(&ts, NULL);

	/* Write should fail (not hang, not crash) */
	const uint8_t data[] = "after drop";
	int64_t w = dssh_chan_write(oc.client_ch, 0,
	    data, sizeof(data) - 1);
	ASSERT_TRUE(w <= 0);

	/* Poll should indicate error or return immediately */
	int ev = dssh_chan_poll(oc.client_ch, DSSH_POLL_READ, 100);
	(void)ev;  /* any result is OK as long as it doesn't hang */

	conn_cleanup(&ctx);
	return TEST_PASS;
}

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
		.cbs = &accept_cbs_all,
		.accept_timeout = 30000,
	};
	if (open_exec_channel(&oc) < 0) {
		conn_cleanup(&ctx);
		return TEST_SKIP;
	}

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
 * Coverage: session_start twice, accept timeout
 * ================================================================ */

static int
test_session_start_twice(void)
{
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
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_FAIL;

	/* accept with timeout_ms=0 -- non-blocking, should return immediately */
	dssh_channel ch = dssh_chan_accept(ctx.server, &accept_cbs_all, 0);
	ASSERT_NULL(ch);

	conn_cleanup(&ctx);
	return TEST_PASS;
}

static int
test_accept_timeout_short(void)
{
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_FAIL;

	dssh_channel ch = dssh_chan_accept(ctx.server, &accept_cbs_all, 50);
	ASSERT_NULL(ch);

	conn_cleanup(&ctx);
	return TEST_PASS;
}

static int
test_session_poll_timeout_zero(void)
{
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_FAIL;

	struct open_exec_ctx oc = {
		.client = ctx.client,
		.server = ctx.server,
		.command = "echo polltest",
		.cbs = &accept_cbs_all,
		.accept_timeout = 30000,
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
	int events = dssh_chan_poll(oc.client_ch,
	    DSSH_POLL_READ | DSSH_POLL_WRITE, 0);
	/* Should return 0 (no events) or just DSSH_POLL_WRITE */
	ASSERT_TRUE(events >= 0);

	conn_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Auto-reject channel types -- covers demux_channel_open branches.
 * ================================================================ */

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
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_FAIL;

	uint8_t msg[64];
	size_t len = build_channel_open(msg, sizeof(msg), "x11", 0, 65536, 32768);
	ASSERT_OK(send_packet(ctx.server, msg, len, NULL));

	dssh_channel ch = dssh_chan_accept(ctx.client, &accept_cbs_all, 0);
	ASSERT_NULL(ch);

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
	ASSERT_OK(send_packet(ctx.server, msg, len, NULL));

	dssh_channel ch = dssh_chan_accept(ctx.client, &accept_cbs_all, 0);
	ASSERT_NULL(ch);

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
	ASSERT_OK(send_packet(ctx.server, msg, len, NULL));

	dssh_channel ch = dssh_chan_accept(ctx.client, &accept_cbs_all, 0);
	ASSERT_NULL(ch);

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
	ASSERT_OK(send_packet(ctx.server, msg, len, NULL));

	dssh_channel ch = dssh_chan_accept(ctx.client, &accept_cbs_all, 0);
	ASSERT_NULL(ch);

	conn_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * WINDOW_ADJUST from peer
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
		.cbs = &accept_cbs_all,
		.accept_timeout = 30000,
	};
	if (open_exec_channel(&oc) < 0 || oc.client_ch == NULL || oc.server_ch == NULL) {
		conn_cleanup(&ctx);
		return TEST_FAIL;
	}

	uint8_t msg[16];
	size_t pos = 0;
	msg[pos++] = SSH_MSG_CHANNEL_WINDOW_ADJUST;
	dssh_serialize_uint32(oc.client_ch->local_id, msg, sizeof(msg), &pos);
	dssh_serialize_uint32(1000, msg, sizeof(msg), &pos);
	ASSERT_OK(send_packet(ctx.server, msg, pos, NULL));

	/* Verify session still works */
	uint8_t data[] = "after window adjust";
	int64_t res = dssh_chan_write(oc.server_ch, 0,
	    data, sizeof(data));
	ASSERT_TRUE(res > 0);

	conn_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Data after EOF
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
		.cbs = &accept_cbs_all,
		.accept_timeout = 30000,
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
		ASSERT_OK(send_packet(ctx.server, msg, pos, NULL));
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
		ASSERT_OK(send_packet(ctx.server, msg, pos, NULL));
	}

	/* Client should have received nothing */
	uint8_t buf[32];
	int64_t got = dssh_chan_read(oc.client_ch, 0, buf, sizeof(buf));
	ASSERT_EQ(got, 0);

	conn_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Truncated channel messages
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
		.cbs = &accept_cbs_all,
		.accept_timeout = 30000,
	};
	if (open_exec_channel(&oc) < 0 || oc.client_ch == NULL || oc.server_ch == NULL) {
		conn_cleanup(&ctx);
		return TEST_FAIL;
	}

	uint8_t msg[8];
	size_t pos = 0;
	msg[pos++] = SSH_MSG_CHANNEL_DATA;
	dssh_serialize_uint32(oc.client_ch->local_id, msg, sizeof(msg), &pos);
	msg[pos++] = 0;
	ASSERT_OK(send_packet(ctx.server, msg, pos, NULL));

	/* Session should still be functional */
	uint8_t data[] = "still works";
	int64_t res = dssh_chan_write(oc.server_ch, 0,
	    data, sizeof(data));
	ASSERT_TRUE(res > 0);

	conn_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Extended data after EOF
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
		.cbs = &accept_cbs_all,
		.accept_timeout = 30000,
	};
	if (open_exec_channel(&oc) < 0 || oc.client_ch == NULL || oc.server_ch == NULL) {
		conn_cleanup(&ctx);
		return TEST_FAIL;
	}

	{
		uint8_t msg[8];
		size_t pos = 0;
		msg[pos++] = SSH_MSG_CHANNEL_EOF;
		dssh_serialize_uint32(oc.client_ch->local_id, msg, sizeof(msg), &pos);
		ASSERT_OK(send_packet(ctx.server, msg, pos, NULL));
	}

	{
		uint8_t msg[32];
		size_t pos = 0;
		msg[pos++] = SSH_MSG_CHANNEL_EXTENDED_DATA;
		dssh_serialize_uint32(oc.client_ch->local_id, msg, sizeof(msg), &pos);
		dssh_serialize_uint32(1, msg, sizeof(msg), &pos);
		dssh_serialize_uint32(5, msg, sizeof(msg), &pos);
		memcpy(&msg[pos], "stale", 5);
		pos += 5;
		ASSERT_OK(send_packet(ctx.server, msg, pos, NULL));
	}

	conn_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Truncated CHANNEL_EXTENDED_DATA
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
		.cbs = &accept_cbs_all,
		.accept_timeout = 30000,
	};
	if (open_exec_channel(&oc) < 0 || oc.client_ch == NULL || oc.server_ch == NULL) {
		conn_cleanup(&ctx);
		return TEST_FAIL;
	}

	uint8_t msg[12];
	size_t pos = 0;
	msg[pos++] = SSH_MSG_CHANNEL_EXTENDED_DATA;
	dssh_serialize_uint32(oc.client_ch->local_id, msg, sizeof(msg), &pos);
	dssh_serialize_uint32(1, msg, sizeof(msg), &pos);
	ASSERT_OK(send_packet(ctx.server, msg, pos, NULL));

	uint8_t data[] = "ok";
	int64_t res = dssh_chan_write(oc.server_ch, 0, data, sizeof(data));
	ASSERT_TRUE(res > 0);

	conn_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Channel request with want_reply=true
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
		.cbs = &accept_cbs_all,
		.accept_timeout = 30000,
	};
	if (open_exec_channel(&oc) < 0 || oc.client_ch == NULL || oc.server_ch == NULL) {
		conn_cleanup(&ctx);
		return TEST_FAIL;
	}

	{
		uint8_t msg[64];
		size_t pos = 0;
		msg[pos++] = SSH_MSG_CHANNEL_REQUEST;
		dssh_serialize_uint32(oc.client_ch->local_id, msg, sizeof(msg), &pos);
		dssh_serialize_uint32(7, msg, sizeof(msg), &pos);
		memcpy(&msg[pos], "unknown", 7);
		pos += 7;
		msg[pos++] = 1;
		ASSERT_OK(send_packet(ctx.server, msg, pos, NULL));
	}

	uint8_t data[] = "after request";
	int64_t res = dssh_chan_write(oc.server_ch, 0,
	    data, sizeof(data));
	ASSERT_TRUE(res > 0);

	conn_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Data with dlen > local_window
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
		.cbs = &accept_cbs_all,
		.accept_timeout = 30000,
	};
	if (open_exec_channel(&oc) < 0 || oc.client_ch == NULL || oc.server_ch == NULL) {
		conn_cleanup(&ctx);
		return TEST_FAIL;
	}

	{
		uint8_t msg[32];
		size_t pos = 0;
		msg[pos++] = SSH_MSG_CHANNEL_DATA;
		dssh_serialize_uint32(oc.client_ch->local_id, msg, sizeof(msg), &pos);
		dssh_serialize_uint32(0xFFFFFFFF, msg, sizeof(msg), &pos);
		memcpy(&msg[pos], "x", 1);
		pos += 1;
		ASSERT_OK(send_packet(ctx.server, msg, pos, NULL));
	}

	conn_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Coverage: poll WRITE readiness
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
		.cbs = &accept_cbs_all,
		.accept_timeout = 30000,
	};
	if (open_exec_channel(&oc) < 0 || oc.client_ch == NULL || oc.server_ch == NULL) {
		conn_cleanup(&ctx);
		return TEST_FAIL;
	}

	int ev = dssh_chan_poll(oc.client_ch, DSSH_POLL_WRITE, 0);
	ASSERT_TRUE(ev & DSSH_POLL_WRITE);

	ev = dssh_chan_poll(oc.client_ch, DSSH_POLL_WRITE, 100);
	ASSERT_TRUE(ev & DSSH_POLL_WRITE);

	dssh_chan_close(oc.server_ch, 0);
	dssh_chan_close(oc.client_ch, 0);
	conn_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Coverage: write edge cases
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
		.cbs = &accept_cbs_all,
		.accept_timeout = 30000,
	};
	if (open_exec_channel(&oc) < 0 || oc.client_ch == NULL || oc.server_ch == NULL) {
		conn_cleanup(&ctx);
		return TEST_FAIL;
	}

	oc.client_ch->eof_sent = true;
	int64_t w = dssh_chan_write(oc.client_ch, 0,
	    (const uint8_t *)"after", 5);
	ASSERT_TRUE(w < 0);

	oc.client_ch->eof_sent = false;
	dssh_chan_close(oc.server_ch, 0);
	dssh_chan_close(oc.client_ch, 0);
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
		.cbs = &accept_cbs_all,
		.accept_timeout = 30000,
	};
	if (open_exec_channel(&oc) < 0 || oc.client_ch == NULL || oc.server_ch == NULL) {
		conn_cleanup(&ctx);
		return TEST_FAIL;
	}

	oc.server_ch->eof_sent = true;
	int64_t w = dssh_chan_write(oc.server_ch, 1,
	    (const uint8_t *)"after", 5);
	ASSERT_TRUE(w < 0);

	oc.server_ch->eof_sent = false;
	dssh_chan_close(oc.server_ch, 0);
	dssh_chan_close(oc.client_ch, 0);
	conn_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Coverage: subsystem write edge cases
 * ================================================================ */

static int
test_subsys_write_after_close(void)
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

	oc.server_ch->eof_sent = true;
	int64_t w = dssh_chan_write(oc.server_ch, 0,
	    (const uint8_t *)"after", 5);
	ASSERT_TRUE(w < 0);

	oc.server_ch->eof_sent = false;
	dssh_chan_close(oc.client_ch, -1);
	dssh_chan_close(oc.server_ch, -1);
	conn_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Coverage: subsystem poll edge cases
 * ================================================================ */

static int
test_subsys_poll_write(void)
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

	int ev = dssh_chan_poll(oc.client_ch, DSSH_POLL_WRITE, 0);
	ASSERT_TRUE(ev & DSSH_POLL_WRITE);

	ev = dssh_chan_poll(oc.client_ch, DSSH_POLL_WRITE, 100);
	ASSERT_TRUE(ev & DSSH_POLL_WRITE);

	dssh_chan_close(oc.client_ch, -1);
	dssh_chan_close(oc.server_ch, -1);
	conn_cleanup(&ctx);
	return TEST_PASS;
}

static int
test_subsys_poll_timeout_zero(void)
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

	int ev = dssh_chan_poll(oc.server_ch, DSSH_POLL_READ, 0);
	ASSERT_EQ(ev, 0);

	dssh_chan_close(oc.client_ch, -1);
	dssh_chan_close(oc.server_ch, -1);
	conn_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Coverage: signal read when empty, interleave tests
 * ================================================================ */

static int
test_read_event_empty(void)
{
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_SKIP;

	struct open_exec_ctx oc = {
		.client = ctx.client,
		.server = ctx.server,
		.command = "echo sig_empty",
		.cbs = &accept_cbs_all,
		.accept_timeout = 30000,
	};
	if (open_exec_channel(&oc) < 0 || oc.client_ch == NULL || oc.server_ch == NULL) {
		conn_cleanup(&ctx);
		return TEST_FAIL;
	}

	/* No event pending -- should return NOMORE */
	struct dssh_chan_event event;
	int res = dssh_chan_read_event(oc.server_ch, &event);
	ASSERT_EQ(res, DSSH_ERROR_NOMORE);

	dssh_chan_close(oc.server_ch, 0);
	dssh_chan_close(oc.client_ch, 0);
	conn_cleanup(&ctx);
	return TEST_PASS;
}

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
		.cbs = &accept_cbs_all,
		.accept_timeout = 30000,
	};
	if (open_exec_channel(&oc) < 0 || oc.client_ch == NULL || oc.server_ch == NULL) {
		conn_cleanup(&ctx);
		return TEST_FAIL;
	}

	/* Client sends data, then a signal, then more data */
	const uint8_t before[] = "BEFORE";
	int64_t w = dssh_chan_write(oc.client_ch, 0,
	    before, sizeof(before) - 1);
	ASSERT_TRUE(w > 0);

	int res = dssh_chan_send_signal(oc.client_ch, "INT");
	ASSERT_OK(res);

	const uint8_t after[] = "AFTER";
	w = dssh_chan_write(oc.client_ch, 0,
	    after, sizeof(after) - 1);
	ASSERT_TRUE(w > 0);

	int ev = dssh_chan_poll(oc.server_ch, DSSH_POLL_READ, 5000);
	ASSERT_TRUE(ev & DSSH_POLL_READ);

	/* Brief delay so signal + second data arrive through demux */
	struct timespec ts = { .tv_sec = 0, .tv_nsec = 10000000 };
	thrd_sleep(&ts, NULL);

	/* New API: data flows freely, events are separate.
	 * Read all data (BEFORE + AFTER), then read signal event. */
	uint8_t buf[64];
	size_t total_read = 0;

	for (int i = 0; i < 50; i++) {
		ev = dssh_chan_poll(oc.server_ch, DSSH_POLL_READ, 200);

		if (ev <= 0)
			break;
		if (!(ev & DSSH_POLL_READ))
			break;

		int64_t n = dssh_chan_read(oc.server_ch, 0,
		    buf + total_read, sizeof(buf) - total_read);

		if (n <= 0)
			break;
		total_read += (size_t)n;
		if (total_read >= sizeof(before) - 1 + sizeof(after) - 1)
			break;
	}
	ASSERT_EQ_U(total_read, sizeof(before) - 1 + sizeof(after) - 1);
	ASSERT_MEM_EQ(buf, "BEFOREAFTER", 11);

	/* Signal event available separately */
	ev = dssh_chan_poll(oc.server_ch, DSSH_POLL_EVENT, 5000);
	ASSERT_TRUE(ev & DSSH_POLL_EVENT);

	struct dssh_chan_event event;

	res = dssh_chan_read_event(oc.server_ch, &event);
	ASSERT_OK(res);
	ASSERT_EQ(event.type, DSSH_EVENT_SIGNAL);
	ASSERT_STR_EQ(event.signal.name, "INT");

	dssh_chan_close(oc.server_ch, 0);
	dssh_chan_close(oc.client_ch, 0);
	conn_cleanup(&ctx);
	return TEST_PASS;
}

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
		.cbs = &accept_cbs_all,
		.accept_timeout = 30000,
	};
	if (open_exec_channel(&oc) < 0 || oc.client_ch == NULL || oc.server_ch == NULL) {
		conn_cleanup(&ctx);
		return TEST_FAIL;
	}

	/* Server sends stderr data, then a signal, then more stderr data */
	const uint8_t before[] = "ERR1";
	int64_t w = dssh_chan_write(oc.server_ch, 1,
	    before, sizeof(before) - 1);
	ASSERT_TRUE(w > 0);

	int res = dssh_chan_send_signal(oc.server_ch, "TERM");
	ASSERT_OK(res);

	const uint8_t after[] = "ERR2";
	w = dssh_chan_write(oc.server_ch, 1,
	    after, sizeof(after) - 1);
	ASSERT_TRUE(w > 0);

	int ev = dssh_chan_poll(oc.client_ch, DSSH_POLL_READEXT, 5000);
	ASSERT_TRUE(ev & DSSH_POLL_READEXT);

	struct timespec ts = { .tv_sec = 0, .tv_nsec = 10000000 };
	thrd_sleep(&ts, NULL);

	/* New API: data flows freely, events separate.
	 * Read all stderr data, then read signal event. */
	uint8_t buf[64];
	size_t total_read = 0;

	for (int i = 0; i < 50; i++) {
		ev = dssh_chan_poll(oc.client_ch, DSSH_POLL_READEXT, 200);

		if (ev <= 0)
			break;
		if (!(ev & DSSH_POLL_READEXT))
			break;

		int64_t n = dssh_chan_read(oc.client_ch, 1,
		    buf + total_read, sizeof(buf) - total_read);

		if (n <= 0)
			break;
		total_read += (size_t)n;
		if (total_read >= sizeof(before) - 1 + sizeof(after) - 1)
			break;
	}
	ASSERT_EQ_U(total_read, sizeof(before) - 1 + sizeof(after) - 1);
	ASSERT_MEM_EQ(buf, "ERR1ERR2", 8);

	ev = dssh_chan_poll(oc.client_ch, DSSH_POLL_EVENT, 5000);
	ASSERT_TRUE(ev & DSSH_POLL_EVENT);

	struct dssh_chan_event event;

	res = dssh_chan_read_event(oc.client_ch, &event);
	ASSERT_OK(res);
	ASSERT_EQ(event.type, DSSH_EVENT_SIGNAL);
	ASSERT_STR_EQ(event.signal.name, "TERM");

	dssh_chan_close(oc.server_ch, 0);
	dssh_chan_close(oc.client_ch, 0);
	conn_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Coverage: truncated CHANNEL_OPEN
 * ================================================================ */

static int
test_truncated_channel_open(void)
{
	/*
	 * Malformed CHANNEL_OPEN now sends OPEN_FAILURE (when
	 * sender-channel is extractable) then disconnects.
	 * Test each truncation point individually.
	 */
	/* Case 1: too short to read type_len -- disconnect only */
	{
		struct conn_ctx ctx;
		if (conn_setup(&ctx) < 0)
			return TEST_FAIL;

		uint8_t msg[2];
		msg[0] = SSH_MSG_CHANNEL_OPEN;
		msg[1] = 0;
		ASSERT_OK(send_packet(ctx.client, msg, 2, NULL));

		struct timespec ts = { .tv_sec = 0, .tv_nsec = 50000000 };
		thrd_sleep(&ts, NULL);

		ASSERT_TRUE(!ctx.server->demux_running);
		conn_cleanup(&ctx);
	}

	/* Case 2: type_len exceeds payload -- disconnect only */
	{
		struct conn_ctx ctx;
		if (conn_setup(&ctx) < 0)
			return TEST_FAIL;

		uint8_t msg[8];
		size_t pos = 0;
		msg[pos++] = SSH_MSG_CHANNEL_OPEN;
		dssh_serialize_uint32(100, msg, sizeof(msg), &pos);
		ASSERT_OK(send_packet(ctx.client, msg, pos, NULL));

		struct timespec ts = { .tv_sec = 0, .tv_nsec = 50000000 };
		thrd_sleep(&ts, NULL);

		ASSERT_TRUE(!ctx.server->demux_running);
		conn_cleanup(&ctx);
	}

	/* Case 3: type OK but missing sender/window/maxpacket --
	 * sends OPEN_FAILURE (sender-channel extractable) then disconnects */
	{
		struct conn_ctx ctx;
		if (conn_setup(&ctx) < 0)
			return TEST_FAIL;

		uint8_t msg[16];
		size_t pos = 0;
		msg[pos++] = SSH_MSG_CHANNEL_OPEN;
		dssh_serialize_uint32(4, msg, sizeof(msg), &pos);
		memcpy(&msg[pos], "test", 4);
		pos += 4;
		/* Include sender-channel so OPEN_FAILURE can be sent */
		dssh_serialize_uint32(42, msg, sizeof(msg), &pos);
		ASSERT_OK(send_packet(ctx.client, msg, pos, NULL));

		struct timespec ts = { .tv_sec = 0, .tv_nsec = 50000000 };
		thrd_sleep(&ts, NULL);

		ASSERT_TRUE(!ctx.server->demux_running);
		conn_cleanup(&ctx);
	}

	return TEST_PASS;
}

/* ================================================================
 * Coverage: truncated CHANNEL_REQUEST
 * ================================================================ */

static int
test_truncated_channel_request(void)
{
	/*
	 * Malformed CHANNEL_REQUEST now sends CHANNEL_FAILURE
	 * then disconnects.  Test truncated type-length.
	 */
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_FAIL;

	struct open_exec_ctx oc = {
		.client = ctx.client,
		.server = ctx.server,
		.command = "echo chanreq",
		.cbs = &accept_cbs_all,
		.accept_timeout = 30000,
	};
	if (open_exec_channel(&oc) < 0 || oc.client_ch == NULL || oc.server_ch == NULL) {
		conn_cleanup(&ctx);
		return TEST_FAIL;
	}

	{
		uint8_t msg[8];
		size_t pos = 0;
		msg[pos++] = SSH_MSG_CHANNEL_REQUEST;
		dssh_serialize_uint32(oc.server_ch->local_id, msg, sizeof(msg), &pos);
		ASSERT_OK(send_packet(ctx.client, msg, pos, NULL));
	}

	struct timespec ts = { .tv_sec = 0, .tv_nsec = 50000000 };
	thrd_sleep(&ts, NULL);

	ASSERT_TRUE(!ctx.server->demux_running);

	conn_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Coverage: short payload
 * ================================================================ */

static int
test_demux_short_payload(void)
{
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_FAIL;

	{
		uint8_t msg[4];
		size_t pos = 0;
		msg[pos++] = SSH_MSG_CHANNEL_DATA;
		msg[pos++] = 0;
		msg[pos++] = 0;
		ASSERT_OK(send_packet(ctx.client, msg, pos, NULL));
	}

	{
		uint8_t msg[2];
		msg[0] = SSH_MSG_CHANNEL_CLOSE;
		msg[1] = 0;
		ASSERT_OK(send_packet(ctx.client, msg, 2, NULL));
	}

	struct timespec ts = { .tv_sec = 0, .tv_nsec = 10000000 };
	thrd_sleep(&ts, NULL);

	ASSERT_TRUE(ctx.server->demux_running);

	conn_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Coverage: poll READ when empty (timeout=0)
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
		.cbs = &accept_cbs_all,
		.accept_timeout = 30000,
	};
	if (open_exec_channel(&oc) < 0 || oc.client_ch == NULL || oc.server_ch == NULL) {
		conn_cleanup(&ctx);
		return TEST_FAIL;
	}

	int ev = dssh_chan_poll(oc.server_ch, DSSH_POLL_READ, 0);
	ASSERT_EQ(ev, 0);

	ev = dssh_chan_poll(oc.server_ch, DSSH_POLL_READEXT, 0);
	ASSERT_EQ(ev, 0);

	ev = dssh_chan_poll(oc.server_ch, DSSH_POLL_EVENT, 0);
	ASSERT_EQ(ev, 0);

	dssh_chan_close(oc.server_ch, 0);
	dssh_chan_close(oc.client_ch, 0);
	conn_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Coverage: write with window=0 and max_packet clamping
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
		.cbs = &accept_cbs_all,
		.accept_timeout = 30000,
	};
	if (open_exec_channel(&oc) < 0 || oc.client_ch == NULL || oc.server_ch == NULL) {
		conn_cleanup(&ctx);
		return TEST_FAIL;
	}

	oc.client_ch->remote_window = 0;
	int64_t w = dssh_chan_write(oc.client_ch, 0,
	    (const uint8_t *)"data", 4);
	ASSERT_EQ(w, 0);

	oc.client_ch->remote_window = 2;
	w = dssh_chan_write(oc.client_ch, 0,
	    (const uint8_t *)"data", 4);
	ASSERT_EQ(w, 2);

	oc.client_ch->remote_window = 1000;
	uint32_t orig_max = oc.client_ch->remote_max_packet;
	oc.client_ch->remote_max_packet = 3;
	w = dssh_chan_write(oc.client_ch, 0,
	    (const uint8_t *)"data", 4);
	ASSERT_EQ(w, 3);
	oc.client_ch->remote_max_packet = orig_max;

	dssh_chan_close(oc.server_ch, 0);
	dssh_chan_close(oc.client_ch, 0);
	conn_cleanup(&ctx);
	return TEST_PASS;
}

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
		.cbs = &accept_cbs_all,
		.accept_timeout = 30000,
	};
	if (open_exec_channel(&oc) < 0 || oc.client_ch == NULL || oc.server_ch == NULL) {
		conn_cleanup(&ctx);
		return TEST_FAIL;
	}

	oc.server_ch->remote_window = 0;
	int64_t w = dssh_chan_write(oc.server_ch, 1,
	    (const uint8_t *)"data", 4);
	ASSERT_EQ(w, 0);

	oc.server_ch->remote_window = 2;
	w = dssh_chan_write(oc.server_ch, 1,
	    (const uint8_t *)"data", 4);
	ASSERT_EQ(w, 2);

	oc.server_ch->remote_window = 1000;
	uint32_t orig_max = oc.server_ch->remote_max_packet;
	oc.server_ch->remote_max_packet = 3;
	w = dssh_chan_write(oc.server_ch, 1,
	    (const uint8_t *)"data", 4);
	ASSERT_EQ(w, 3);
	oc.server_ch->remote_max_packet = orig_max;

	dssh_chan_close(oc.server_ch, 0);
	dssh_chan_close(oc.client_ch, 0);
	conn_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Coverage: accept with negative timeout (blocking)
 * ================================================================ */

struct accept_blocking_ctx {
	struct conn_ctx *ctx;
	_Atomic bool entered;
};

static int
accept_blocking_thread(void *arg)
{
	struct accept_blocking_ctx *bc = arg;
	atomic_store(&bc->entered, true);
	dssh_chan_accept(bc->ctx->server, &accept_cbs_all, -1);
	return 0;
}

static int
test_accept_timeout_negative(void)
{
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_SKIP;

	struct accept_blocking_ctx bc = {
		.ctx = &ctx,
	};
	atomic_init(&bc.entered, false);

	thrd_t at;
	ASSERT_THRD_CREATE(&at, accept_blocking_thread, &bc);

	/* Wait for the accept thread to enter dssh_chan_accept */
	while (!atomic_load(&bc.entered))
		thrd_yield();

	conn_cleanup(&ctx);
	thrd_join(at, NULL);
	return TEST_PASS;
}

/* ================================================================
 * Coverage: server rejects exec request via callback
 * ================================================================ */

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
		.cbs = &reject_exec_cbs,
		.accept_timeout = 30000,
	};

	thrd_t ct, st;
	ASSERT_THRD_CREATE(&ct, client_open_exec_thread, &oc);
	ASSERT_THRD_CREATE(&st, server_accept_exec_thread, &oc);
	thrd_join(ct, NULL);

	/* After rejection, dssh_chan_accept loops back to wait for the
	 * next CHANNEL_OPEN.  Terminate to unblock it. */
	session_set_terminate(ctx.server);
	thrd_join(st, NULL);

	ASSERT_NULL(oc.client_ch);
	ASSERT_NULL(oc.server_ch);

	conn_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Coverage: truncated OPEN_CONFIRMATION
 * ================================================================ */

static int
test_truncated_open_confirmation(void)
{
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_FAIL;

	{
		uint8_t msg[8];
		size_t pos = 0;
		msg[pos++] = SSH_MSG_CHANNEL_OPEN_CONFIRMATION;
		dssh_serialize_uint32(0, msg, sizeof(msg), &pos);
		ASSERT_OK(send_packet(ctx.client, msg, pos, NULL));
	}

	struct timespec ts = { .tv_sec = 0, .tv_nsec = 10000000 };
	thrd_sleep(&ts, NULL);
	ASSERT_TRUE(ctx.server->demux_running);

	conn_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Coverage: OPEN_CONFIRMATION for unknown channel
 * ================================================================ */

static int
test_open_confirmation_unknown_channel(void)
{
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_FAIL;

	{
		uint8_t msg[20];
		size_t pos = 0;
		msg[pos++] = SSH_MSG_CHANNEL_OPEN_CONFIRMATION;
		dssh_serialize_uint32(9999, msg, sizeof(msg), &pos);
		dssh_serialize_uint32(0, msg, sizeof(msg), &pos);
		dssh_serialize_uint32(0x200000, msg, sizeof(msg), &pos);
		dssh_serialize_uint32(0x8000, msg, sizeof(msg), &pos);
		ASSERT_OK(send_packet(ctx.client, msg, pos, NULL));
	}

	struct timespec ts = { .tv_sec = 0, .tv_nsec = 10000000 };
	thrd_sleep(&ts, NULL);
	ASSERT_TRUE(ctx.server->demux_running);

	conn_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Coverage: CHANNEL_SUCCESS/FAILURE when no request pending
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
		.cbs = &accept_cbs_all,
		.accept_timeout = 30000,
	};
	if (open_exec_channel(&oc) < 0 || oc.client_ch == NULL || oc.server_ch == NULL) {
		conn_cleanup(&ctx);
		return TEST_FAIL;
	}

	{
		uint8_t msg[8];
		size_t pos = 0;
		msg[pos++] = SSH_MSG_CHANNEL_SUCCESS;
		dssh_serialize_uint32(oc.server_ch->local_id, msg, sizeof(msg), &pos);
		ASSERT_OK(send_packet(ctx.client, msg, pos, NULL));
	}
	{
		uint8_t msg[8];
		size_t pos = 0;
		msg[pos++] = SSH_MSG_CHANNEL_FAILURE;
		dssh_serialize_uint32(oc.server_ch->local_id, msg, sizeof(msg), &pos);
		ASSERT_OK(send_packet(ctx.client, msg, pos, NULL));
	}

	struct timespec ts = { .tv_sec = 0, .tv_nsec = 10000000 };
	thrd_sleep(&ts, NULL);
	ASSERT_TRUE(ctx.server->demux_running);

	dssh_chan_close(oc.server_ch, 0);
	dssh_chan_close(oc.client_ch, 0);
	conn_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Coverage: double EOF/CLOSE
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
		.cbs = &accept_cbs_all,
		.accept_timeout = 30000,
	};
	if (open_exec_channel(&oc) < 0 || oc.client_ch == NULL || oc.server_ch == NULL) {
		conn_cleanup(&ctx);
		return TEST_FAIL;
	}

	dssh_chan_close(oc.server_ch, 0);
	dssh_chan_close(oc.client_ch, 0);

	conn_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Coverage: DATA with dlen > actual payload bytes
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
		.cbs = &accept_cbs_all,
		.accept_timeout = 30000,
	};
	if (open_exec_channel(&oc) < 0 || oc.client_ch == NULL || oc.server_ch == NULL) {
		conn_cleanup(&ctx);
		return TEST_FAIL;
	}

	{
		uint8_t msg[32];
		size_t pos = 0;
		msg[pos++] = SSH_MSG_CHANNEL_DATA;
		dssh_serialize_uint32(oc.server_ch->local_id, msg, sizeof(msg), &pos);
		dssh_serialize_uint32(100, msg, sizeof(msg), &pos);
		memcpy(&msg[pos], "abc", 3);
		pos += 3;
		ASSERT_OK(send_packet(ctx.client, msg, pos, NULL));
	}

	{
		uint8_t msg[32];
		size_t pos = 0;
		msg[pos++] = SSH_MSG_CHANNEL_EXTENDED_DATA;
		dssh_serialize_uint32(oc.server_ch->local_id, msg, sizeof(msg), &pos);
		dssh_serialize_uint32(1, msg, sizeof(msg), &pos);
		dssh_serialize_uint32(100, msg, sizeof(msg), &pos);
		memcpy(&msg[pos], "xy", 2);
		pos += 2;
		ASSERT_OK(send_packet(ctx.client, msg, pos, NULL));
	}

	struct timespec ts = { .tv_sec = 0, .tv_nsec = 10000000 };
	thrd_sleep(&ts, NULL);

	ASSERT_TRUE(ctx.server->demux_running);

	uint8_t buf[256];
	int64_t n = dssh_chan_read(oc.server_ch, 0, buf, sizeof(buf));
	ASSERT_TRUE(n >= 0);

	dssh_chan_close(oc.server_ch, 0);
	dssh_chan_close(oc.client_ch, 0);
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
		.cbs = &accept_cbs_all,
		.accept_timeout = 30000,
	};
	if (open_exec_channel(&oc) < 0 || oc.client_ch == NULL || oc.server_ch == NULL) {
		conn_cleanup(&ctx);
		return TEST_FAIL;
	}

	int saved_type = oc.server_ch->chan_type;
	oc.server_ch->chan_type = 0;

	uint8_t payload[16];
	size_t pos = 0;
	payload[pos++] = SSH_MSG_CHANNEL_DATA;
	dssh_serialize_uint32(oc.server_ch->local_id, payload, sizeof(payload), &pos);
	dssh_serialize_uint32(1, payload, sizeof(payload), &pos);
	payload[pos++] = 'x';

	demux_dispatch(ctx.server, SSH_MSG_CHANNEL_DATA, payload, pos);

	oc.server_ch->chan_type = saved_type;

	dssh_chan_close(oc.server_ch, 0);
	dssh_chan_close(oc.client_ch, 0);
	conn_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Coverage: send_eof/close already-sent, window replenish
 * ================================================================ */

static int
test_send_eof_already_sent(void)
{
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_SKIP;

	struct open_exec_ctx oc = {
		.client = ctx.client, .server = ctx.server,
		.command = "echo eof2", .cbs = &accept_cbs_all,
		.accept_timeout = 30000,
	};
	if (open_exec_channel(&oc) < 0 || !oc.client_ch || !oc.server_ch) {
		conn_cleanup(&ctx);
		return TEST_FAIL;
	}

	ASSERT_OK(send_eof(ctx.client, oc.client_ch));
	ASSERT_TRUE(oc.client_ch->eof_sent);
	ASSERT_EQ(send_eof(ctx.client, oc.client_ch), 0);

	dssh_chan_close(oc.server_ch, 0);
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
		.command = "echo close2", .cbs = &accept_cbs_all,
		.accept_timeout = 30000,
	};
	if (open_exec_channel(&oc) < 0 || !oc.client_ch || !oc.server_ch) {
		conn_cleanup(&ctx);
		return TEST_FAIL;
	}

	ASSERT_OK(conn_close(ctx.client, oc.client_ch));
	ASSERT_TRUE(oc.client_ch->close_sent);
	ASSERT_EQ(conn_close(ctx.client, oc.client_ch), 0);

	dssh_chan_close(oc.server_ch, 0);
	conn_cleanup(&ctx);
	return TEST_PASS;
}

static int
test_maybe_replenish_after_eof(void)
{
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_SKIP;

	struct open_exec_ctx oc = {
		.client = ctx.client, .server = ctx.server,
		.command = "echo replenish", .cbs = &accept_cbs_all,
		.accept_timeout = 30000,
	};
	if (open_exec_channel(&oc) < 0 || !oc.client_ch || !oc.server_ch) {
		conn_cleanup(&ctx);
		return TEST_FAIL;
	}

	oc.server_ch->local_window = 0;
	oc.server_ch->eof_received = true;
	maybe_replenish_window(ctx.server, oc.server_ch);
	ASSERT_EQ(oc.server_ch->local_window, (uint32_t)0);
	oc.server_ch->eof_received = false;

	dssh_chan_close(oc.server_ch, 0);
	dssh_chan_close(oc.client_ch, 0);
	conn_cleanup(&ctx);
	return TEST_PASS;
}

static int
test_maybe_replenish_low_window(void)
{
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_SKIP;

	struct open_exec_ctx oc = {
		.client = ctx.client, .server = ctx.server,
		.command = "echo replow", .cbs = &accept_cbs_all,
		.accept_timeout = 30000,
	};
	if (open_exec_channel(&oc) < 0 || !oc.client_ch || !oc.server_ch) {
		conn_cleanup(&ctx);
		return TEST_FAIL;
	}

	oc.server_ch->local_window = 1;
	maybe_replenish_window(ctx.server, oc.server_ch);
	ASSERT_TRUE(oc.server_ch->local_window > 1);

	dssh_chan_close(oc.server_ch, 0);
	dssh_chan_close(oc.client_ch, 0);
	conn_cleanup(&ctx);
	return TEST_PASS;
}

static int
test_demux_window_underflow_to_zero(void)
{
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_FAIL;

	struct open_exec_ctx oc = {
		.client = ctx.client, .server = ctx.server,
		.command = "echo wunder", .cbs = &accept_cbs_all,
		.accept_timeout = 30000,
	};
	if (open_exec_channel(&oc) < 0 || !oc.client_ch || !oc.server_ch) {
		conn_cleanup(&ctx);
		return TEST_FAIL;
	}

	oc.server_ch->local_window = 5;
	oc.server_ch->window_max = 5;

	const uint8_t data[] = "hello";
	dssh_chan_write(oc.client_ch, 0, data, 5);

	int ev = dssh_chan_poll(oc.server_ch, DSSH_POLL_READ, 5000);
	ASSERT_TRUE(ev & DSSH_POLL_READ);

	{
		uint8_t msg[32];
		size_t pos = 0;
		msg[pos++] = SSH_MSG_CHANNEL_DATA;
		dssh_serialize_uint32(oc.server_ch->local_id, msg, sizeof(msg), &pos);
		dssh_serialize_uint32(3, msg, sizeof(msg), &pos);
		memcpy(&msg[pos], "abc", 3);
		pos += 3;
		ASSERT_OK(send_packet(ctx.client, msg, pos, NULL));
	}

	struct timespec ts = { .tv_sec = 0, .tv_nsec = 10000000 };
	thrd_sleep(&ts, NULL);

	dssh_chan_close(oc.server_ch, 0);
	dssh_chan_close(oc.client_ch, 0);
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
		.command = "echo woverflow", .cbs = &accept_cbs_all,
		.accept_timeout = 30000,
	};
	if (open_exec_channel(&oc) < 0 || !oc.client_ch || !oc.server_ch) {
		conn_cleanup(&ctx);
		return TEST_FAIL;
	}

	oc.server_ch->local_window = UINT32_MAX - 10;
	send_window_adjust(ctx.server, oc.server_ch, 20);
	ASSERT_EQ(oc.server_ch->local_window, UINT32_MAX);

	dssh_chan_close(oc.server_ch, 0);
	dssh_chan_close(oc.client_ch, 0);
	conn_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Coverage: write not_open/close_received guards
 * ================================================================ */

static int
test_chan_write_not_open(void)
{
	dssh_test_reset_global_config();
	if (register_all_algorithms() < 0)
		return TEST_SKIP;
	dssh_session s = dssh_session_init(false, 0);
	if (s == NULL)
		return TEST_SKIP;

	struct dssh_channel_s ch = {0};
	ch.sess = s;
	ch.io_model = DSSH_IO_STREAM;
	ch.open = false;
	ch.eof_sent = false;
	ch.close_received = false;
	mtx_init(&ch.buf_mtx, mtx_plain);

	int64_t r = dssh_chan_write(&ch, 0, (const uint8_t *)"x", 1);
	ASSERT_EQ(r, DSSH_ERROR_TERMINATED);

	mtx_destroy(&ch.buf_mtx);
	dssh_session_cleanup(s);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

static int
test_chan_write_close_received(void)
{
	dssh_test_reset_global_config();
	if (register_all_algorithms() < 0)
		return TEST_SKIP;
	dssh_session s = dssh_session_init(false, 0);
	if (s == NULL)
		return TEST_SKIP;

	struct dssh_channel_s ch = {0};
	ch.sess = s;
	ch.io_model = DSSH_IO_STREAM;
	ch.open = true;
	ch.eof_sent = false;
	ch.close_received = true;
	mtx_init(&ch.buf_mtx, mtx_plain);

	int64_t r = dssh_chan_write(&ch, 0, (const uint8_t *)"x", 1);
	ASSERT_EQ(r, DSSH_ERROR_TERMINATED);

	mtx_destroy(&ch.buf_mtx);
	dssh_session_cleanup(s);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

/* ================================================================
 * Coverage: nsec overflow tests
 * ================================================================ */

static int
test_session_poll_nsec_overflow(void)
{
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_SKIP;

	struct open_exec_ctx oc = {
		.client = ctx.client,
		.server = ctx.server,
		.command = "echo nsec",
		.cbs = &accept_cbs_all,
		.accept_timeout = 30000,
	};
	if (open_exec_channel(&oc) < 0 || oc.client_ch == NULL || oc.server_ch == NULL) {
		conn_cleanup(&ctx);
		return TEST_FAIL;
	}

	for (int i = 0; i < 3; i++) {
		int r = dssh_chan_poll(oc.server_ch, DSSH_POLL_READ, 1);
		(void)r;
	}
	int r = dssh_chan_poll(oc.server_ch, DSSH_POLL_READ, 999);
	ASSERT_EQ(r, 0);

	dssh_chan_close(oc.server_ch, 0);
	dssh_chan_close(oc.client_ch, 0);
	conn_cleanup(&ctx);
	return TEST_PASS;
}

static int
test_accept_nsec_overflow(void)
{
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_FAIL;

	for (int i = 0; i < 3; i++) {
		dssh_channel ch = dssh_chan_accept(ctx.server, &accept_cbs_all, 1);
		(void)ch;
	}
	dssh_channel ch = dssh_chan_accept(ctx.server, &accept_cbs_all, 999);
	ASSERT_NULL(ch);

	conn_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Subsystem roundtrip
 * ================================================================ */

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
	ASSERT_THRD_CREATE(&st, server_accept_subsys_thread, &oc);
	thrd_join(ct, NULL);
	thrd_join(st, NULL);

	if (oc.client_ch == NULL || oc.server_ch == NULL) {
		if (oc.client_ch != NULL)
			dssh_chan_close(oc.client_ch, -1);
		if (oc.server_ch != NULL)
			dssh_chan_close(oc.server_ch, -1);
		conn_cleanup(&ctx);
		return TEST_SKIP;
	}

	ASSERT_EQ(dssh_chan_get_type(oc.server_ch), DSSH_CHAN_SUBSYSTEM);
	ASSERT_NOT_NULL(dssh_chan_get_subsystem(oc.server_ch));
	ASSERT_STR_EQ(dssh_chan_get_subsystem(oc.server_ch), "test-echo");

	/* Client sends a message to server */
	static const uint8_t msg1[] = "hello from client";
	int64_t w = dssh_chan_write(oc.client_ch, 0,
	    msg1, sizeof(msg1));
	ASSERT_TRUE(w > 0);

	/* Server polls and reads */
	int ev = dssh_chan_poll(oc.server_ch, DSSH_POLL_READ, 5000);
	ASSERT_TRUE(ev & DSSH_POLL_READ);

	uint8_t buf[256];
	size_t got = 0;
	while (got < sizeof(msg1)) {
		int64_t r = dssh_chan_read(oc.server_ch, 0,
		    &buf[got], sizeof(buf) - got);
		if (r <= 0)
			break;
		got += (size_t)r;
	}
	ASSERT_EQ_U(got, sizeof(msg1));
	ASSERT_TRUE(memcmp(buf, msg1, sizeof(msg1)) == 0);

	/* Server sends reply back */
	static const uint8_t msg2[] = "hello from server";
	w = dssh_chan_write(oc.server_ch, 0,
	    msg2, sizeof(msg2));
	ASSERT_TRUE(w > 0);

	/* Client polls and reads */
	ev = dssh_chan_poll(oc.client_ch, DSSH_POLL_READ, 5000);
	ASSERT_TRUE(ev & DSSH_POLL_READ);

	got = 0;
	while (got < sizeof(msg2)) {
		int64_t r = dssh_chan_read(oc.client_ch, 0,
		    &buf[got], sizeof(buf) - got);
		if (r <= 0)
			break;
		got += (size_t)r;
	}
	ASSERT_EQ_U(got, sizeof(msg2));
	ASSERT_TRUE(memcmp(buf, msg2, sizeof(msg2)) == 0);

	/* Clean close */
	dssh_chan_close(oc.client_ch, -1);
	dssh_chan_close(oc.server_ch, -1);
	conn_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Audit item 13: ZC accept — server accepts a zero-copy channel
 * ================================================================ */

struct zc_rx_state {
	mtx_t    mtx;
	cnd_t    cnd;
	uint8_t  buf[256];
	size_t   len;
	bool     done;
};

static uint32_t
test_zc_data_cb(dssh_channel ch, int stream,
    const uint8_t *data, size_t len, void *cbdata)
{
	(void)ch;
	(void)stream;
	struct zc_rx_state *st = cbdata;
	mtx_lock(&st->mtx);
	size_t avail = sizeof(st->buf) - st->len;
	size_t n = len < avail ? len : avail;
	memcpy(&st->buf[st->len], data, n);
	st->len += n;
	st->done = true;
	cnd_signal(&st->cnd);
	mtx_unlock(&st->mtx);
	uint32_t n_u32 = (uint32_t)n;
	return n_u32;
}

static int
accept_subsys_zc(dssh_channel ch, const struct dssh_chan_params *p,
    struct dssh_chan_accept_result *result, void *cbdata)
{
	(void)ch;
	(void)p;
	result->zc_cb = test_zc_data_cb;
	result->zc_cbdata = cbdata;
	return 0;
}

struct zc_accept_ctx {
	dssh_session server;
	const struct dssh_chan_accept_cbs *cbs;
	dssh_channel server_ch;
};

static int
server_accept_zc_thread(void *arg)
{
	struct zc_accept_ctx *ctx = arg;
	ctx->server_ch = dssh_chan_accept(ctx->server, ctx->cbs, 30000);
	return 0;
}

static int
test_accept_zc(void)
{
	struct conn_ctx ctx;

	if (conn_setup(&ctx) < 0)
		return TEST_SKIP;

	/* Set up ZC receive state for the server */
	struct zc_rx_state zc_st;
	memset(&zc_st, 0, sizeof(zc_st));
	if (mtx_init(&zc_st.mtx, mtx_plain) != thrd_success) {
		conn_cleanup(&ctx);
		return TEST_SKIP;
	}
	if (cnd_init(&zc_st.cnd) != thrd_success) {
		mtx_destroy(&zc_st.mtx);
		conn_cleanup(&ctx);
		return TEST_SKIP;
	}

	struct dssh_chan_accept_cbs zc_cbs = {
		.subsystem = accept_subsys_zc,
		.cbdata = &zc_st,
	};

	struct zc_accept_ctx sa = {
		.server = ctx.server,
		.cbs = &zc_cbs,
		.server_ch = NULL,
	};

	/* Client opens a subsystem channel (stream mode) */
	struct open_subsys_ctx oc = {
		.client = ctx.client,
		.server = ctx.server,
		.subsystem = "test-zc",
	};

	thrd_t ct, st;
	ASSERT_THRD_CREATE(&ct, client_open_subsys_thread, &oc);
	ASSERT_THRD_CREATE(&st, server_accept_zc_thread, &sa);
	thrd_join(ct, NULL);
	thrd_join(st, NULL);

	if (oc.client_ch == NULL || sa.server_ch == NULL) {
		if (oc.client_ch != NULL)
			dssh_chan_close(oc.client_ch, -1);
		if (sa.server_ch != NULL)
			dssh_chan_close(sa.server_ch, -1);
		cnd_destroy(&zc_st.cnd);
		mtx_destroy(&zc_st.mtx);
		conn_cleanup(&ctx);
		return TEST_SKIP;
	}

	/* Verify server channel is ZC mode */
	ASSERT_EQ(sa.server_ch->io_model, DSSH_IO_ZC);
	ASSERT_EQ(dssh_chan_get_type(sa.server_ch), DSSH_CHAN_SUBSYSTEM);
	ASSERT_STR_EQ(dssh_chan_get_subsystem(sa.server_ch), "test-zc");

	/* Client sends data (stream mode) */
	static const uint8_t msg1[] = "hello from client to zc";
	int64_t w = dssh_chan_write(oc.client_ch, 0,
	    msg1, sizeof(msg1));
	ASSERT_TRUE(w > 0);

	/* Wait for ZC callback to fire on server */
	mtx_lock(&zc_st.mtx);
	struct timespec deadline;
	timespec_get(&deadline, TIME_UTC);
	deadline.tv_sec += 1;
	while (!zc_st.done)
		cnd_timedwait(&zc_st.cnd, &zc_st.mtx, &deadline);
	ASSERT_TRUE(zc_st.done);
	ASSERT_EQ_U(zc_st.len, sizeof(msg1));
	ASSERT_TRUE(memcmp(zc_st.buf, msg1, sizeof(msg1)) == 0);
	mtx_unlock(&zc_st.mtx);

	/* Server sends reply via ZC getbuf/send */
	static const uint8_t msg2[] = "zc reply from server";
	uint8_t *txbuf;
	size_t max_len;
	int res = dssh_chan_zc_getbuf(sa.server_ch, 0, &txbuf, &max_len);
	ASSERT_OK(res);
	ASSERT_TRUE(max_len >= sizeof(msg2));
	memcpy(txbuf, msg2, sizeof(msg2));
	res = dssh_chan_zc_send(sa.server_ch, sizeof(msg2));
	ASSERT_OK(res);

	/* Client reads reply (stream mode) */
	int ev = dssh_chan_poll(oc.client_ch, DSSH_POLL_READ, 1000);
	ASSERT_TRUE(ev & DSSH_POLL_READ);

	uint8_t rbuf[256];
	size_t got = 0;
	while (got < sizeof(msg2)) {
		int64_t r = dssh_chan_read(oc.client_ch, 0,
		    &rbuf[got], sizeof(rbuf) - got);
		if (r <= 0)
			break;
		got += (size_t)r;
	}
	ASSERT_EQ_U(got, sizeof(msg2));
	ASSERT_TRUE(memcmp(rbuf, msg2, sizeof(msg2)) == 0);

	/* Clean close */
	dssh_chan_close(oc.client_ch, -1);
	dssh_chan_close(sa.server_ch, -1);
	cnd_destroy(&zc_st.cnd);
	mtx_destroy(&zc_st.mtx);
	conn_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Item 74: bytebuf write truncation + window accounting
 * ================================================================ */

static int
test_demux_data_truncation_window(void)
{
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_SKIP;

	struct open_exec_ctx oc = {
		.client = ctx.client, .server = ctx.server,
		.command = "echo trunc", .cbs = &accept_cbs_all,
		.accept_timeout = 30000,
	};
	if (open_exec_channel(&oc) < 0 || !oc.client_ch || !oc.server_ch) {
		conn_cleanup(&ctx);
		return TEST_FAIL;
	}

	bytebuf_free(&oc.server_ch->buf.session.stdout_buf);
	ASSERT_OK(bytebuf_init(&oc.server_ch->buf.session.stdout_buf, 16));
	oc.server_ch->window_max = 16;
	oc.server_ch->local_window = 16;

	uint8_t fill[12];
	memset(fill, 'A', sizeof(fill));
	bytebuf_write(&oc.server_ch->buf.session.stdout_buf, fill, 12);
	oc.server_ch->local_window = 16;

	uint8_t payload[32];
	size_t pos = 0;
	payload[pos++] = SSH_MSG_CHANNEL_DATA;
	dssh_serialize_uint32(oc.server_ch->local_id, payload, sizeof(payload), &pos);
	dssh_serialize_uint32(10, payload, sizeof(payload), &pos);
	memset(&payload[pos], 'B', 10);
	pos += 10;

	mtx_lock(&oc.server_ch->buf_mtx);
	oc.server_ch->setup_mode = false;
	mtx_unlock(&oc.server_ch->buf_mtx);

	demux_dispatch(ctx.server, SSH_MSG_CHANNEL_DATA, payload, pos);

	ASSERT_EQ(oc.server_ch->local_window, (uint32_t)12);

	dssh_chan_close(oc.server_ch, 0);
	dssh_chan_close(oc.client_ch, 0);
	conn_cleanup(&ctx);
	return TEST_PASS;
}

static int
test_replenish_caps_to_buffer_space(void)
{
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_SKIP;

	struct open_exec_ctx oc = {
		.client = ctx.client, .server = ctx.server,
		.command = "echo repcap", .cbs = &accept_cbs_all,
		.accept_timeout = 30000,
	};
	if (open_exec_channel(&oc) < 0 || !oc.client_ch || !oc.server_ch) {
		conn_cleanup(&ctx);
		return TEST_FAIL;
	}

	bytebuf_free(&oc.server_ch->buf.session.stdout_buf);
	ASSERT_OK(bytebuf_init(&oc.server_ch->buf.session.stdout_buf, 32));
	bytebuf_free(&oc.server_ch->buf.session.stderr_buf);
	ASSERT_OK(bytebuf_init(&oc.server_ch->buf.session.stderr_buf, 32));
	oc.server_ch->window_max = 32;

	uint8_t fill[28];
	memset(fill, 'X', sizeof(fill));
	bytebuf_write(&oc.server_ch->buf.session.stdout_buf, fill, 28);

	oc.server_ch->local_window = 0;

	maybe_replenish_window(ctx.server, oc.server_ch);

	ASSERT_TRUE(oc.server_ch->local_window <= 4);
	ASSERT_TRUE(oc.server_ch->local_window > 0);

	dssh_chan_close(oc.server_ch, 0);
	dssh_chan_close(oc.client_ch, 0);
	conn_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Data/ext after CLOSE
 * ================================================================ */

static int
test_data_after_close(void)
{
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_FAIL;

	struct open_exec_ctx oc = {
		.client = ctx.client,
		.server = ctx.server,
		.command = "echo closetest",
		.cbs = &accept_cbs_all,
		.accept_timeout = 30000,
	};
	if (open_exec_channel(&oc) < 0 || oc.client_ch == NULL || oc.server_ch == NULL) {
		conn_cleanup(&ctx);
		return TEST_FAIL;
	}

	{
		uint8_t msg[8];
		size_t pos = 0;
		msg[pos++] = SSH_MSG_CHANNEL_CLOSE;
		DSSH_PUT_U32(oc.client_ch->local_id, msg, &pos);
		ASSERT_OK(send_packet(ctx.server, msg, pos, NULL));
	}

	{
		uint8_t msg[32];
		size_t pos = 0;
		msg[pos++] = SSH_MSG_CHANNEL_DATA;
		DSSH_PUT_U32(oc.client_ch->local_id, msg, &pos);
		DSSH_PUT_U32(5, msg, &pos);
		memcpy(&msg[pos], "stale", 5);
		pos += 5;
		ASSERT_OK(send_packet(ctx.server, msg, pos, NULL));
	}

	struct timespec ts = { .tv_sec = 0, .tv_nsec = 20000000 };
	thrd_sleep(&ts, NULL);

	uint8_t buf[32];
	int64_t got = dssh_chan_read(oc.client_ch, 0, buf, sizeof(buf));
	ASSERT_EQ(got, 0);

	conn_cleanup(&ctx);
	return TEST_PASS;
}

static int
test_ext_data_after_close(void)
{
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_FAIL;

	struct open_exec_ctx oc = {
		.client = ctx.client,
		.server = ctx.server,
		.command = "echo extclose",
		.cbs = &accept_cbs_all,
		.accept_timeout = 30000,
	};
	if (open_exec_channel(&oc) < 0 || oc.client_ch == NULL || oc.server_ch == NULL) {
		conn_cleanup(&ctx);
		return TEST_FAIL;
	}

	{
		uint8_t msg[8];
		size_t pos = 0;
		msg[pos++] = SSH_MSG_CHANNEL_CLOSE;
		DSSH_PUT_U32(oc.client_ch->local_id, msg, &pos);
		ASSERT_OK(send_packet(ctx.server, msg, pos, NULL));
	}

	{
		uint8_t msg[32];
		size_t pos = 0;
		msg[pos++] = SSH_MSG_CHANNEL_EXTENDED_DATA;
		DSSH_PUT_U32(oc.client_ch->local_id, msg, &pos);
		DSSH_PUT_U32(1, msg, &pos);
		DSSH_PUT_U32(5, msg, &pos);
		memcpy(&msg[pos], "stale", 5);
		pos += 5;
		ASSERT_OK(send_packet(ctx.server, msg, pos, NULL));
	}

	struct timespec ts = { .tv_sec = 0, .tv_nsec = 20000000 };
	thrd_sleep(&ts, NULL);

	uint8_t buf[32];
	int64_t got = dssh_chan_read(oc.client_ch, 1, buf, sizeof(buf));
	ASSERT_EQ(got, 0);

	conn_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Coverage: replenish window with close_received
 * ================================================================ */

static int
test_replenish_after_close(void)
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
	bytebuf_init(&ch.buf.session.stdout_buf, 256);
	bytebuf_init(&ch.buf.session.stderr_buf, 256);
	sigqueue_init(&ch.buf.session.signals);
	ch.close_received = true;
	ch.window_max = 256;
	ch.local_window = 0;

	int r = maybe_replenish_window(s, &ch);
	ASSERT_EQ(r, 0);
	ASSERT_EQ_U(ch.local_window, 0);

	sigqueue_free(&ch.buf.session.signals);
	bytebuf_free(&ch.buf.session.stdout_buf);
	bytebuf_free(&ch.buf.session.stderr_buf);
	cnd_destroy(&ch.poll_cnd);
	mtx_destroy(&ch.buf_mtx);
	dssh_session_cleanup(s);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

/* ================================================================
 * Coverage: truncated WINDOW_ADJUST
 * ================================================================ */

static int
test_truncated_window_adjust(void)
{
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_FAIL;

	struct open_exec_ctx oc = {
		.client = ctx.client,
		.server = ctx.server,
		.command = "echo winadj",
		.cbs = &accept_cbs_all,
		.accept_timeout = 30000,
	};
	if (open_exec_channel(&oc) < 0 || oc.client_ch == NULL || oc.server_ch == NULL) {
		conn_cleanup(&ctx);
		return TEST_FAIL;
	}

	{
		uint8_t msg[8];
		size_t pos = 0;
		msg[pos++] = SSH_MSG_CHANNEL_WINDOW_ADJUST;
		DSSH_PUT_U32(oc.client_ch->local_id, msg, &pos);
		msg[pos++] = 0;
		ASSERT_OK(send_packet(ctx.server, msg, pos, NULL));
	}

	struct timespec ts = { .tv_sec = 0, .tv_nsec = 10000000 };
	thrd_sleep(&ts, NULL);

	ASSERT_TRUE(ctx.client->demux_running);

	dssh_chan_close(oc.server_ch, 0);
	dssh_chan_close(oc.client_ch, 0);
	conn_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Allocation failure injection during channel open
 * ================================================================ */

static int
server_accept_alloc_thread(void *arg)
{
	struct open_exec_ctx *ctx = arg;

	dssh_test_alloc_exclude_thread();

	ctx->server_ch = dssh_chan_accept(ctx->server, ctx->cbs, 1000);
	return 0;
}

static int
test_open_exec_alloc_sweep(void)
{
	int any_failed = 0;

	for (int n = 0; n < 50; n++) {
		struct conn_ctx ctx;

		dssh_test_alloc_exclude_new_threads();

		if (conn_setup(&ctx) < 0)
			return TEST_FAIL;

		struct open_exec_ctx oc = {
			.client = ctx.client,
			.server = ctx.server,
			.client_ch = NULL,
			.server_ch = NULL,
			.command = "echo alloc",
			.cbs = &accept_cbs_all,
		};

		dssh_test_alloc_fail_after(n);

		thrd_t ct, st;
		ASSERT_THRD_CREATE(&ct, client_open_exec_thread, &oc);
		ASSERT_THRD_CREATE(&st, server_accept_alloc_thread, &oc);
		thrd_join(ct, NULL);

		int count = dssh_test_alloc_count();
		dssh_test_alloc_reset();

		if (oc.client_ch != NULL) {
			thrd_join(st, NULL);
			if (oc.server_ch != NULL)
				dssh_chan_close(oc.server_ch, 0);
			dssh_chan_close(oc.client_ch, 0);
			conn_cleanup(&ctx);

			ASSERT_TRUE(any_failed);
			return TEST_PASS;
		}

		any_failed = 1;
		session_set_terminate(ctx.server);
		thrd_join(st, NULL);

		if (oc.server_ch != NULL)
			dssh_chan_close(oc.server_ch, 0);

		conn_cleanup(&ctx);

		if (n >= count)
			break;
	}

	return any_failed ? TEST_PASS : TEST_FAIL;
}

static int
client_open_shell_alloc_thread(void *arg)
{
	struct open_shell_ctx *ctx = arg;
	struct dssh_chan_params p;
	dssh_chan_params_init(&p, DSSH_CHAN_SHELL);
	dssh_chan_params_set_term(&p, ctx->term);
	dssh_chan_params_set_size(&p, ctx->cols, ctx->rows, ctx->wpx, ctx->hpx);
	ctx->client_ch = dssh_chan_open(ctx->client, &p);
	dssh_chan_params_free(&p);
	return 0;
}

static int
server_accept_shell_alloc_thread(void *arg)
{
	struct open_shell_ctx *ctx = arg;

	dssh_test_alloc_exclude_thread();

	ctx->server_ch = dssh_chan_accept(ctx->server, ctx->cbs, 1000);
	return 0;
}

static int
test_open_shell_alloc_sweep(void)
{
	int any_failed = 0;

	for (int n = 0; n < 50; n++) {
		struct conn_ctx ctx;

		dssh_test_alloc_exclude_new_threads();

		if (conn_setup(&ctx) < 0)
			return TEST_FAIL;

		struct open_shell_ctx oc = {
			.client = ctx.client,
			.server = ctx.server,
			.client_ch = NULL,
			.server_ch = NULL,
			.term = "xterm",
			.cols = 80,
			.rows = 24,
			.cbs = &accept_cbs_all,
		};

		dssh_test_alloc_fail_after(n);

		thrd_t ct, st;
		ASSERT_THRD_CREATE(&ct, client_open_shell_alloc_thread, &oc);
		ASSERT_THRD_CREATE(&st, server_accept_shell_alloc_thread, &oc);
		thrd_join(ct, NULL);

		int count = dssh_test_alloc_count();
		dssh_test_alloc_reset();

		if (oc.client_ch != NULL) {
			thrd_join(st, NULL);
			if (oc.server_ch != NULL)
				dssh_chan_close(oc.server_ch, 0);
			dssh_chan_close(oc.client_ch, 0);
			conn_cleanup(&ctx);
			ASSERT_TRUE(any_failed);
			return TEST_PASS;
		}

		any_failed = 1;
		session_set_terminate(ctx.server);
		thrd_join(st, NULL);
		if (oc.server_ch != NULL)
			dssh_chan_close(oc.server_ch, 0);
		conn_cleanup(&ctx);

		if (n >= count)
			break;
	}

	return any_failed ? TEST_PASS : TEST_FAIL;
}

/* ================================================================
 * Subsystem alloc failure sweep
 * ================================================================ */

static int
client_open_subsys_alloc_thread(void *arg)
{
	struct open_subsys_ctx *ctx = arg;
	struct dssh_chan_params p;
	dssh_chan_params_init(&p, DSSH_CHAN_SUBSYSTEM);
	dssh_chan_params_set_subsystem(&p, ctx->subsystem);
	dssh_chan_params_set_pty(&p, false);
	ctx->client_ch = dssh_chan_open(ctx->client, &p);
	dssh_chan_params_free(&p);
	return 0;
}

static int
server_accept_subsys_alloc_thread(void *arg)
{
	struct open_subsys_ctx *ctx = arg;

	dssh_test_alloc_exclude_thread();

	ctx->server_ch = dssh_chan_accept(ctx->server, &accept_cbs_all, 1000);
	return 0;
}

static int
test_open_subsystem_alloc_sweep(void)
{
	int any_failed = 0;

	for (int n = 0; n < 50; n++) {
		struct conn_ctx ctx;

		dssh_test_alloc_exclude_new_threads();

		if (conn_setup(&ctx) < 0)
			return TEST_FAIL;

		struct open_subsys_ctx oc = {
			.client = ctx.client,
			.server = ctx.server,
			.client_ch = NULL,
			.server_ch = NULL,
			.subsystem = "sftp",
		};

		dssh_test_alloc_fail_after(n);

		thrd_t ct, st;
		ASSERT_THRD_CREATE(&ct, client_open_subsys_alloc_thread, &oc);
		ASSERT_THRD_CREATE(&st, server_accept_subsys_alloc_thread, &oc);
		thrd_join(ct, NULL);

		int count = dssh_test_alloc_count();
		dssh_test_alloc_reset();

		if (oc.client_ch != NULL) {
			thrd_join(st, NULL);
			if (oc.server_ch != NULL)
				dssh_chan_close(oc.server_ch, -1);
			dssh_chan_close(oc.client_ch, -1);
			conn_cleanup(&ctx);
			ASSERT_TRUE(any_failed);
			return TEST_PASS;
		}

		any_failed = 1;
		session_set_terminate(ctx.server);
		thrd_join(st, NULL);

		if (oc.server_ch != NULL)
			dssh_chan_close(oc.server_ch, -1);

		conn_cleanup(&ctx);

		if (n >= count)
			break;
	}

	return any_failed ? TEST_PASS : TEST_FAIL;
}

/* ================================================================
 * Server-side accept alloc sweep
 * ================================================================ */

static int
client_open_exec_nofail_thread(void *arg)
{
	struct open_exec_ctx *ctx = arg;
	dssh_test_alloc_exclude_thread();
	struct dssh_chan_params p;
	dssh_chan_params_init(&p, DSSH_CHAN_EXEC);
	dssh_chan_params_set_command(&p, ctx->command);
	dssh_chan_params_set_pty(&p, false);
	ctx->client_ch = dssh_chan_open(ctx->client, &p);
	dssh_chan_params_free(&p);
	return 0;
}

static int
server_accept_alloc_inject_thread(void *arg)
{
	struct open_exec_ctx *ctx = arg;
	ctx->server_ch = dssh_chan_accept(ctx->server, ctx->cbs, 1000);
	return 0;
}

static int
test_server_accept_alloc_sweep(void)
{
	int any_failed = 0;

	for (int n = 0; n < 50; n++) {
		struct conn_ctx ctx;

		dssh_test_alloc_exclude_new_threads();

		if (conn_setup(&ctx) < 0)
			return TEST_FAIL;

		struct open_exec_ctx oc = {
			.client = ctx.client,
			.server = ctx.server,
			.client_ch = NULL,
			.server_ch = NULL,
			.command = "echo srvalloc",
			.cbs = &accept_cbs_all,
		};

		thrd_t ct, st;
		ASSERT_THRD_CREATE(&ct, client_open_exec_nofail_thread, &oc);

		dssh_test_alloc_fail_after(n);

		ASSERT_THRD_CREATE(&st, server_accept_alloc_inject_thread, &oc);
		thrd_join(st, NULL);

		int count = dssh_test_alloc_count();
		dssh_test_alloc_reset();

		if (oc.server_ch != NULL) {
			thrd_join(ct, NULL);
			dssh_chan_close(oc.server_ch, 0);
			if (oc.client_ch != NULL)
				dssh_chan_close(oc.client_ch, 0);
			conn_cleanup(&ctx);
			ASSERT_TRUE(any_failed);
			return TEST_PASS;
		}

		any_failed = 1;
		session_set_terminate(ctx.client);
		thrd_join(ct, NULL);

		if (oc.client_ch != NULL)
			dssh_chan_close(oc.client_ch, 0);

		conn_cleanup(&ctx);

		if (n >= count)
			break;
	}

	return any_failed ? TEST_PASS : TEST_FAIL;
}

/* ================================================================
 * Subsystem: write zero bytes
 * ================================================================ */

static int
test_subsys_write_zero(void)
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

	if (oc.server_ch == NULL || oc.client_ch == NULL) {
		conn_cleanup(&ctx);
		return TEST_SKIP;
	}

	/* Write zero bytes should return 0 (no-op) */
	uint8_t dummy = 0;
	int64_t w = dssh_chan_write(oc.server_ch, 0, &dummy, 0);
	ASSERT_EQ(w, 0);

	int ev = dssh_chan_poll(oc.server_ch, DSSH_POLL_READ, 0);
	ASSERT_TRUE(ev >= 0);

	dssh_chan_close(oc.client_ch, -1);
	dssh_chan_close(oc.server_ch, -1);
	conn_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Item 64: poll deadline must not reset on spurious wakeup
 * ================================================================ */

struct spam_cnd_arg {
	cnd_t *cnd;
	mtx_t *mtx;
	_Atomic bool stop;
};

static int
spam_cnd_thread(void *arg)
{
	struct spam_cnd_arg *sa = arg;
	const struct timespec dur = { .tv_sec = 0, .tv_nsec = 50000000L };

	while (!sa->stop) {
		thrd_sleep(&dur, NULL);

		mtx_lock(sa->mtx);
		cnd_broadcast(sa->cnd);
		mtx_unlock(sa->mtx);
	}

	return 0;
}

static int
test_poll_deadline_not_reset(void)
{
	struct conn_ctx ctx;

	if (conn_setup(&ctx) < 0)
		return TEST_SKIP;

	struct open_exec_ctx oc = {
		.client = ctx.client,
		.server = ctx.server,
		.command = "cmd",
		.cbs = &accept_cbs_all,
		.accept_timeout = 30000,
	};
	if (open_exec_channel(&oc) < 0) {
		conn_cleanup(&ctx);
		return TEST_SKIP;
	}

	struct spam_cnd_arg sa = {
		.cnd = &oc.server_ch->poll_cnd,
		.mtx = &oc.server_ch->buf_mtx,
		.stop = false,
	};
	thrd_t spammer;

	ASSERT_THRD_CREATE(&spammer, spam_cnd_thread, &sa);

	struct timespec t0;

	timespec_get(&t0, TIME_UTC);

	int ev = dssh_chan_poll(oc.server_ch, DSSH_POLL_READ, 500);
	ASSERT_EQ(ev, 0);

	struct timespec t1;

	timespec_get(&t1, TIME_UTC);
	sa.stop = true;
	thrd_join(spammer, NULL);

	long elapsed_ms = (t1.tv_sec - t0.tv_sec) * 1000L
	    + (t1.tv_nsec - t0.tv_nsec) / 1000000L;

	if (elapsed_ms > 1200) {
		fprintf(stderr, "  poll took %ldms (expected ~500ms)\n",
		    elapsed_ms);
		dssh_chan_close(oc.client_ch, 0);
		dssh_chan_close(oc.server_ch, 0);
		conn_cleanup(&ctx);
		return TEST_FAIL;
	}

	dssh_chan_close(oc.client_ch, 0);
	dssh_chan_close(oc.server_ch, 0);
	conn_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Channel request parser unit tests
 * ================================================================ */

static size_t
build_channel_request(uint8_t *buf, size_t bufsz,
    uint32_t channel, const char *rtype, bool want_reply,
    const uint8_t *extra, size_t extra_len)
{
	size_t pos = 0;

	buf[pos++] = SSH_MSG_CHANNEL_REQUEST;
	DSSH_PUT_U32(channel, buf, &pos);
	uint32_t rtlen = (uint32_t)strlen(rtype);
	DSSH_PUT_U32(rtlen, buf, &pos);
	memcpy(&buf[pos], rtype, rtlen);
	pos += rtlen;
	buf[pos++] = want_reply ? 1 : 0;
	if (extra != NULL && extra_len > 0) {
		memcpy(&buf[pos], extra, extra_len);
		pos += extra_len;
	}
	(void)bufsz;
	return pos;
}

static int
test_chanreq_parse_valid(void)
{
	uint8_t buf[128];
	uint8_t extra[] = { 0x00, 0x00, 0x00, 0x04, 'b', 'a', 's', 'h' };
	size_t len = build_channel_request(buf, sizeof(buf),
	    42, "exec", true, extra, sizeof(extra));

	const uint8_t *rtype;
	uint32_t rtype_len;
	bool want_reply;
	const uint8_t *rdata;
	size_t rdata_len;

	ASSERT_OK(parse_channel_request(buf, len,
	    &rtype, &rtype_len, &want_reply, &rdata, &rdata_len));
	ASSERT_EQ(rtype_len, 4U);
	ASSERT_MEM_EQ(rtype, "exec", 4);
	ASSERT_TRUE(want_reply);
	ASSERT_EQ(rdata_len, sizeof(extra));
	ASSERT_MEM_EQ(rdata, extra, sizeof(extra));

	return TEST_PASS;
}

static int
test_chanreq_parse_empty_type(void)
{
	uint8_t buf[32];
	size_t len = build_channel_request(buf, sizeof(buf),
	    0, "", false, NULL, 0);

	const uint8_t *rtype;
	uint32_t rtype_len;
	bool want_reply;
	const uint8_t *rdata;
	size_t rdata_len;

	ASSERT_OK(parse_channel_request(buf, len,
	    &rtype, &rtype_len, &want_reply, &rdata, &rdata_len));
	ASSERT_EQ(rtype_len, 0U);
	ASSERT_FALSE(want_reply);
	ASSERT_EQ(rdata_len, 0U);

	return TEST_PASS;
}

static int
test_chanreq_parse_no_data(void)
{
	uint8_t buf[32];
	size_t len = build_channel_request(buf, sizeof(buf),
	    7, "shell", true, NULL, 0);

	const uint8_t *rtype;
	uint32_t rtype_len;
	bool want_reply;
	const uint8_t *rdata;
	size_t rdata_len;

	ASSERT_OK(parse_channel_request(buf, len,
	    &rtype, &rtype_len, &want_reply, &rdata, &rdata_len));
	ASSERT_EQ(rtype_len, 5U);
	ASSERT_MEM_EQ(rtype, "shell", 5);
	ASSERT_TRUE(want_reply);
	ASSERT_EQ(rdata_len, 0U);

	return TEST_PASS;
}

static int
test_chanreq_parse_truncated_type_len(void)
{
	uint8_t buf[5];

	buf[0] = SSH_MSG_CHANNEL_REQUEST;
	size_t pos = 1;
	DSSH_PUT_U32(0, buf, &pos);

	const uint8_t *rtype;
	uint32_t rtype_len;
	bool want_reply;
	const uint8_t *rdata;
	size_t rdata_len;

	ASSERT_ERR(parse_channel_request(buf, pos,
	    &rtype, &rtype_len, &want_reply, &rdata, &rdata_len),
	    DSSH_ERROR_PARSE);

	return TEST_PASS;
}

static int
test_chanreq_parse_truncated_type(void)
{
	uint8_t buf[16];
	size_t pos = 0;

	buf[pos++] = SSH_MSG_CHANNEL_REQUEST;
	DSSH_PUT_U32(0, buf, &pos);
	DSSH_PUT_U32(10, buf, &pos);
	buf[pos++] = 'a';
	buf[pos++] = 'b';
	buf[pos++] = 'c';

	const uint8_t *rtype;
	uint32_t rtype_len;
	bool want_reply;
	const uint8_t *rdata;
	size_t rdata_len;

	ASSERT_ERR(parse_channel_request(buf, pos,
	    &rtype, &rtype_len, &want_reply, &rdata, &rdata_len),
	    DSSH_ERROR_PARSE);

	return TEST_PASS;
}

static int
test_chanreq_parse_truncated_want_reply(void)
{
	uint8_t buf[16];
	size_t pos = 0;

	buf[pos++] = SSH_MSG_CHANNEL_REQUEST;
	DSSH_PUT_U32(0, buf, &pos);
	DSSH_PUT_U32(2, buf, &pos);
	buf[pos++] = 'o';
	buf[pos++] = 'k';

	const uint8_t *rtype;
	uint32_t rtype_len;
	bool want_reply;
	const uint8_t *rdata;
	size_t rdata_len;

	ASSERT_ERR(parse_channel_request(buf, pos,
	    &rtype, &rtype_len, &want_reply, &rdata, &rdata_len),
	    DSSH_ERROR_PARSE);

	return TEST_PASS;
}

/* ================================================================
 * NULL parameter validation
 * ================================================================ */

static int
test_chan_read_null(void)
{
	uint8_t buf[16];

	ASSERT_EQ(dssh_chan_read(NULL, 0, buf, sizeof(buf)),
	    DSSH_ERROR_INVALID);
	return TEST_PASS;
}

static int
test_chan_write_null(void)
{
	uint8_t buf[16] = {0};

	ASSERT_EQ(dssh_chan_write(NULL, 0, buf, sizeof(buf)),
	    DSSH_ERROR_INVALID);
	return TEST_PASS;
}

static int
test_chan_poll_null(void)
{
	ASSERT_EQ(dssh_chan_poll(NULL, DSSH_POLL_READ, 0),
	    DSSH_ERROR_INVALID);
	return TEST_PASS;
}

static int
test_chan_close_null(void)
{
	ASSERT_EQ(dssh_chan_close(NULL, 0),
	    DSSH_ERROR_INVALID);
	return TEST_PASS;
}

static int
test_chan_send_signal_null(void)
{
	ASSERT_EQ(dssh_chan_send_signal(NULL, "HUP"),
	    DSSH_ERROR_INVALID);
	return TEST_PASS;
}

static int
test_chan_send_window_change_null(void)
{
	ASSERT_EQ(dssh_chan_send_window_change(NULL, 80, 24, 0, 0),
	    DSSH_ERROR_INVALID);
	return TEST_PASS;
}

/* ================================================================
 * Test table
 * ================================================================ */

static struct dssh_test_entry tests[] = {
	/* Parse helpers */

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

	/* Subsystem channel I/O */
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
	{ "test_channel_close_subsys",       test_channel_close_subsys },
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
	{ "test_subsys_write_after_close",      test_subsys_write_after_close },
	{ "test_subsys_poll_write",             test_subsys_poll_write },
	{ "test_subsys_poll_timeout_zero",      test_subsys_poll_timeout_zero },
	{ "test_read_event_empty",              test_read_event_empty },
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
	{ "test_demux_chan_type_zero",          test_demux_dispatch_chan_type_zero },
	{ "test_send_eof_already_sent",        test_send_eof_already_sent },
	{ "test_send_close_already_sent",      test_send_close_already_sent },
	{ "test_replenish_after_eof",          test_maybe_replenish_after_eof },
	{ "test_replenish_low_window",         test_maybe_replenish_low_window },
	{ "test_window_underflow_to_zero",     test_demux_window_underflow_to_zero },
	{ "test_window_add_overflow",          test_window_add_overflow },

	/* Write guard coverage */
	{ "test_chan_write_not_open",           test_chan_write_not_open },
	{ "test_chan_write_close_recv",         test_chan_write_close_received },

	{ "test_subsystem_roundtrip",          test_subsystem_roundtrip },
	{ "test_accept_zc",                    test_accept_zc },

	/* Deterministic branch coverage -- profiling-unstable */
	{ "test_session_poll_nsec_overflow",   test_session_poll_nsec_overflow },
	{ "test_accept_nsec_overflow",         test_accept_nsec_overflow },

	/* Item 74: bytebuf truncation + window accounting */
	{ "test_demux_data_truncation",        test_demux_data_truncation_window },
	{ "test_replenish_caps_buf_space",     test_replenish_caps_to_buffer_space },

	/* NULL parameter validation */
	{ "null/chan_read",                    test_chan_read_null },
	{ "null/chan_write",                   test_chan_write_null },
	{ "null/chan_poll",                    test_chan_poll_null },
	{ "null/chan_close",                   test_chan_close_null },
	{ "null/chan_send_signal",             test_chan_send_signal_null },
	{ "null/chan_send_window_change",      test_chan_send_window_change_null },

	/* Allocation failure injection */
	{ "alloc/open_exec_sweep",             test_open_exec_alloc_sweep },
	{ "alloc/open_shell_sweep",            test_open_shell_alloc_sweep },
	{ "alloc/open_subsystem_sweep",        test_open_subsystem_alloc_sweep },
	{ "alloc/server_accept_sweep",         test_server_accept_alloc_sweep },
	{ "subsys/write_zero",                 test_subsys_write_zero },

	/* Additional branch coverage */
	{ "test_data_after_close",             test_data_after_close },
	{ "test_ext_data_after_close",         test_ext_data_after_close },
	{ "test_replenish_after_close",        test_replenish_after_close },
	{ "test_truncated_window_adjust",      test_truncated_window_adjust },

	/* Item 64: deadline must not reset on spurious wakeup */
	{ "test_poll_deadline_not_reset",      test_poll_deadline_not_reset },

	/* Channel request parser unit tests */
	{ "chanreq/parse_valid",               test_chanreq_parse_valid },
	{ "chanreq/parse_empty_type",          test_chanreq_parse_empty_type },
	{ "chanreq/parse_no_data",             test_chanreq_parse_no_data },
	{ "chanreq/parse_truncated_type_len",  test_chanreq_parse_truncated_type_len },
	{ "chanreq/parse_truncated_type",      test_chanreq_parse_truncated_type },
	{ "chanreq/parse_truncated_want_reply", test_chanreq_parse_truncated_want_reply },
};

DSSH_TEST_NO_CLEANUP
DSSH_TEST_MAIN(tests)
