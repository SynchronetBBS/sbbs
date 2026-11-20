/*
 * test_conn.c -- Connection protocol tests for DeuceSSH.
 *
 * Tier 2 layer tests with mock I/O.  Covers session start/stop,
 * channel open/accept, stream and subsystem channel I/O, signals,
 * window change, close/exit, multiple channels, and thread safety.
 *
 * Uses the dssh_chan_* API throughout.
 */

#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <threads.h>

#include "dssh_test.h"
#include "dssh_test_wait.h"
#include "deucessh.h"
#include "deucessh-algorithms.h"
#include "deucessh-auth.h"
#include "deucessh-conn.h"
#include "ssh-trans.h"
#include "ssh-internal.h"
#include "dssh_test_internal.h"
#include "auth_discovery_fixture.h"
#include "dssh_test_alloc.h"
#include "dssh_test_ossl.h"
#include "mock_io.h"
#include "test_dhgex_provider.h"
#include "test_comp.h"
#include "compression_fixture.h"


/* ================================================================
 * Dispatch callbacks for two-sided tests.
 * ================================================================ */

#include "auth_result_fixture.h"

static _Thread_local int (*setup_tx_test_hook)(dssh_session sess);
static _Thread_local int (*accept_phase_test_hook)(dssh_session sess);
static _Thread_local int (*window_tx_return_test_hook)(dssh_session sess);
static bool window_test_gather;
static int (*conn_mac_registration)(void);
static _Thread_local int (*gather_vector_test_hook)(const struct dssh_tx_iov *, size_t, dssh_session);

static int
mock_tx_dispatch(uint8_t *buf, size_t bufsz,
    dssh_session sess, void *cbdata)
{
	if (accept_phase_test_hook != NULL) {
		int ret = accept_phase_test_hook(sess);
		if (ret < 0)
			return ret;
	}
	if (setup_tx_test_hook != NULL) {
		int (*hook)(dssh_session) = setup_tx_test_hook;
		setup_tx_test_hook = NULL;
		return hook(sess);
	}
	int ret = sess->trans.client ? mock_tx_client(buf, bufsz, sess, cbdata)
	    : mock_tx_server(buf, bufsz, sess, cbdata);
	if (ret == 0 && window_tx_return_test_hook != NULL) {
		int (*hook)(dssh_session) = window_tx_return_test_hook;
		window_tx_return_test_hook = NULL;
		ret = hook(sess);
	}
	return ret;
}

static int
window_mock_gather(const struct dssh_tx_iov *iov, size_t count,
    dssh_session sess, void *cbdata)
{
	if (gather_vector_test_hook != NULL) {
		int ret = gather_vector_test_hook(iov, count, sess);
		if (ret != 0)
			return ret;
	}
	for (size_t i = 0; i < count; i++) {
		/* The library owns mutable packet storage; the fixture only reads it. */
		uint8_t *bytes = (uint8_t *)(uintptr_t)iov[i].base;
		int ret = mock_tx_dispatch(bytes, iov[i].len, sess, cbdata);
		if (ret < 0)
			return ret;
	}
	return 0;
}

struct conn_rx_gate {
	dssh_session sess;
	atomic_bool empty;
	atomic_bool resume;
};

/* Published before the demux starts and retired after it joins. */
static struct conn_rx_gate *conn_rx_gate;

static void
conn_pause_empty_read(void *opaque)
{
	struct conn_rx_gate *gate = opaque;
	atomic_store(&gate->empty, true);
	while (!atomic_load(&gate->resume) && !gate->sess->terminate)
		thrd_yield();
}

static int
mock_rx_dispatch(uint8_t *buf, size_t bufsz,
    dssh_session sess, void *cbdata)
{
	bool observe = conn_rx_gate != NULL && conn_rx_gate->sess == sess;
	if (observe)
		mock_io_set_read_retry_hook(conn_pause_empty_read, conn_rx_gate);
	int ret = sess->trans.client ? mock_rx_client(buf, bufsz, sess, cbdata)
	    : mock_rx_server(buf, bufsz, sess, cbdata);
	if (observe)
		mock_io_set_read_retry_hook(NULL, NULL);
	return ret;
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
	if (window_test_gather) {
		res = dssh_transport_set_tx_gather(window_mock_gather);
		if (res < 0)
			return res;
	}
	res = test_register_kex();
	if (res < 0)
		return res;
	res = test_register_key_algos();
	if (res < 0)
		return res;
	res = dssh_register_aes256_ctr();
	if (res < 0)
		return res;
	res = conn_mac_registration != NULL ? conn_mac_registration() : test_register_mac_algos();
	if (res < 0)
		return res;
	res = dssh_test_register_optional_comp();
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
    struct dssh_bytes_result *output_result,
    void *cbdata)
{
	uint8_t *fixture_output = NULL;
	size_t fixture_length = 0;
	uint8_t **change_prompt = &fixture_output;
	size_t *change_prompt_len = &fixture_length;
	(void)change_prompt;
	(void)change_prompt_len;

	return test_auth_output_finish(output_result, fixture_output, fixture_length, DSSH_AUTH_SUCCESS);
}

static struct dssh_auth_server_cbs auth_cbs = {
	.methods_str = "password",
	.password_cb = auth_password_accept_all,
};

/* ================================================================
 * Server accept callbacks (dssh_chan_accept_cbs)
 * ================================================================ */

static int pty_received;
static char pty_term[64];
static uint32_t pty_cols, pty_rows;
static atomic_uint reject_exec_calls;
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
	atomic_store(&reject_exec_calls, 0);
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

struct conn_zc_fixture;
struct channel_workers;
struct conn_ctx {
	struct mock_io_state io;
	bool io_initialized;
	dssh_session client;
	dssh_session server;
	struct conn_zc_fixture *zc_fixture;
	struct channel_workers *workers;
};

/* The assertion hook cleans up before the owning stack frame returns. */
static struct conn_ctx *g_active_ctx;

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
	if (ctx->result != 0) {
		mock_io_close_c2s(ctx->io);
		mock_io_close_s2c(ctx->io);
	}
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
	if (ctx->result != 0) {
		mock_io_close_c2s(ctx->io);
		mock_io_close_s2c(ctx->io);
	}
	return 0;
}

/*
 * Set up a full connection: handshake + auth + session_start on both sides.
 * Returns 0 on success, -1 on failure.
 */
static void conn_cleanup(struct conn_ctx *ctx);
static void conn_zc_release(struct conn_ctx *ctx);
static void conn_zc_join(struct conn_ctx *ctx);
static void conn_zc_retire(struct conn_ctx *ctx);
static void channel_workers_cleanup_cancel(struct conn_ctx *ctx);
static void channel_workers_cleanup_join(struct conn_ctx *ctx);

/* Overridden only by the controlled setup-ownership cases. */
static int (*conn_fixture_thread_create)(thrd_t *, thrd_start_t, void *) = thrd_create;
static int (*conn_fixture_thread_join)(thrd_t, int *) = thrd_join;
static int (*conn_fixture_session_start)(dssh_session) = dssh_session_start;

static int
conn_setup_mode(struct conn_ctx *ctx, bool start)
{
	ctx->client = NULL;
	ctx->server = NULL;
	ctx->io_initialized = false;
	ctx->zc_fixture = NULL;
	ctx->workers = NULL;
	reset_callbacks();
	dssh_test_reset_global_config();

	if (register_all_algorithms() < 0)
		goto setup_fail;
	if (test_generate_host_key() < 0)
		goto setup_fail;

	/* All four descriptors are initialized even on acquisition failure. */
	ctx->io_initialized = true;
	if (mock_io_init(&ctx->io, 0) < 0)
		goto setup_fail;

	int ret = dssh_transport_set_callbacks(mock_tx_dispatch, mock_rx_dispatch,
	    mock_rxline_dispatch, mock_extra_line_cb);
	if (ret != 0)
		goto setup_fail;

	ctx->client = dssh_session_init(true, 0);
	if (ctx->client == NULL)
		goto setup_fail;
	ret = dssh_session_set_cbdata(ctx->client, &ctx->io, &ctx->io,
	    &ctx->io, &ctx->io);
	if (ret != 0)
		goto setup_fail;
	ret = dssh_session_set_hostkey_verify_cb(ctx->client, dssh_test_accept_hostkey, NULL);
	if (ret != 0)
		goto setup_fail;

	ctx->server = init_server_session();
	if (ctx->server == NULL)
		goto setup_fail;
	ret = dssh_session_set_cbdata(ctx->server, &ctx->io, &ctx->io,
	    &ctx->io, &ctx->io);
	if (ret != 0)
		goto setup_fail;

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
	if (conn_fixture_thread_create(&ct, client_handshake_auth_thread, &client_ha) != thrd_success)
		goto setup_fail;
	if (conn_fixture_thread_create(&st, server_handshake_auth_thread, &server_ha) != thrd_success) {
		dssh_session_terminate(ctx->client);
		mock_io_close_c2s(&ctx->io);
		mock_io_close_s2c(&ctx->io);
		if (conn_fixture_thread_join(ct, NULL) != thrd_success)
			abort();
		goto setup_fail;
	}
	if (conn_fixture_thread_join(ct, NULL) != thrd_success)
		abort();
	if (conn_fixture_thread_join(st, NULL) != thrd_success)
		abort();

	if (client_ha.result != 0 || server_ha.result != 0
	    || (start && !compression_fixture_selected(ctx->client, ctx->server)))
		goto setup_fail;
	if (start && (conn_fixture_session_start(ctx->client) != 0
	    || conn_fixture_session_start(ctx->server) != 0))
		goto setup_fail;

	g_active_ctx = ctx;
	return 0;
setup_fail:
	conn_cleanup(ctx);
	return -1;
}

static int
conn_setup(struct conn_ctx *ctx)
{
	return conn_setup_mode(ctx, true);
}

static void
conn_cleanup(struct conn_ctx *ctx)
{
	if (g_active_ctx == ctx)
		g_active_ctx = NULL;
	conn_zc_release(ctx);
	channel_workers_cleanup_cancel(ctx);

	/* Shutdown precedes every demux join; free closes the descriptors only
	 * after all session-owned workers have retired. */
	if (ctx->io_initialized) {
		mock_io_close_c2s(&ctx->io);
		mock_io_close_s2c(&ctx->io);
	}
	conn_zc_join(ctx);
	channel_workers_cleanup_join(ctx);
	if (ctx->server != NULL) {
		dssh_session_cleanup(ctx->server);
		ctx->server = NULL;
	}
	if (ctx->client != NULL) {
		dssh_session_cleanup(ctx->client);
		ctx->client = NULL;
	}
	conn_zc_retire(ctx);
	if (ctx->io_initialized) {
		mock_io_free(&ctx->io);
		ctx->io_initialized = false;
	}
	dssh_test_reset_global_config();
}

/* ================================================================
 * Helper: open an exec channel concurrently (client+server threads)
 * ================================================================ */

static int
fail_with_active_connection(void)
{
	struct conn_ctx ctx;

	if (conn_setup(&ctx) < 0)
		return TEST_FAIL;
	ASSERT_TRUE(false);
	return TEST_PASS;
}

static int
test_assertion_cleanup(void)
{
	ASSERT_EQ(fail_with_active_connection(), TEST_FAIL);
	ASSERT_NULL(g_active_ctx);
	return TEST_PASS;
}

/* Channel setup starts with a zero receive window on both sides.  Each side
 * opens its window asynchronously after setup, so a completed open/accept
 * does not by itself guarantee that an immediate non-blocking write works. */
static int (*channel_fixture_poll)(dssh_channel, int, int) = dssh_chan_poll;

static int
wait_stream_writable(dssh_channel ch)
{
	if (ch == NULL)
		return -1;
	if (ch->io_model != DSSH_IO_STREAM)
		return 0;
	int ev = channel_fixture_poll(ch, DSSH_POLL_WRITE, 5000);
	return ev == DSSH_POLL_WRITE ? 0 : -1;
}

struct open_exec_ctx {
	dssh_session client;
	dssh_session server;
	dssh_channel client_ch;
	dssh_channel server_ch;
	const char *command;
	const struct dssh_chan_accept_cbs *cbs;
	int accept_timeout;
};

/* A successful builder may produce a NULL handle when the peer rejects. */
static int
conn_client_open(dssh_session session, enum dssh_chan_type type,
    const char *command, dssh_channel *channel)
{
	*channel = NULL;
	struct dssh_chan_params params;
	int ret = dssh_chan_params_init(&params, type);
	if (ret != 0)
		return ret;
	if (type == DSSH_CHAN_EXEC)
		ret = dssh_chan_params_set_command(&params, command);
	if (ret == 0)
		ret = dssh_chan_params_set_pty(&params, false);
	if (ret == 0)
		*channel = dssh_chan_open(session, &params);
	dssh_chan_params_free(&params);
	return ret;
}

static int
client_open_exec_thread(void *arg)
{
	struct open_exec_ctx *ctx = arg;
	int ret = conn_client_open(ctx->client, DSSH_CHAN_EXEC, ctx->command, &ctx->client_ch);
	return ret == 0 ? wait_stream_writable(ctx->client_ch) : ret;
}

static int
client_open_rejected_thread(void *arg)
{
	struct open_exec_ctx *ctx = arg;
	int ret = conn_client_open(ctx->client, DSSH_CHAN_EXEC, ctx->command, &ctx->client_ch);
	return ret == 0 && ctx->client_ch == NULL ? 0 : -1;
}

static int
server_accept_cancelled_thread(void *arg)
{
	struct open_exec_ctx *ctx = arg;
	ctx->server_ch = dssh_chan_accept(ctx->server, ctx->cbs, ctx->accept_timeout);
	return ctx->server_ch == NULL && dssh_session_is_terminated(ctx->server) ? 0 : -1;
}

static int
server_accept_exec_thread(void *arg)
{
	struct open_exec_ctx *ctx = arg;
	ctx->server_ch = dssh_chan_accept(ctx->server, ctx->cbs,
	    ctx->accept_timeout);
	return ctx->server_ch == NULL ? DSSH_ERROR_INIT : wait_stream_writable(ctx->server_ch);
}

/*
 * Open an exec channel concurrently.  Returns 0 on success, -1 on failure.
 * Populates ctx->client_ch and ctx->server_ch.
 */
static int (*channel_fixture_thread_create)(thrd_t *, thrd_start_t, void *) = thrd_create;
static int (*channel_fixture_thread_join)(thrd_t, int *) = thrd_join;
static _Thread_local struct conn_ctx *channel_fixture_owner;

struct channel_workers;
struct channel_worker_call {
	struct channel_workers *workers;
	thrd_start_t function;
	void *data;
};
struct channel_workers {
	struct conn_ctx *owner;
	thrd_t threads[2];
	bool created[2];
	int results[2];
	struct channel_worker_call calls[2];
	void (*release)(void *);
	void (*retire)(void *);
	void *resource;
};

static bool (*conn_actor_stage_control)(void);
static void (*conn_actor_retirement_observer)(const struct channel_workers *);

static bool
conn_actor_checkpoint(void)
{
	return conn_actor_stage_control == NULL || conn_actor_stage_control();
}

static void
channel_workers_cancel(struct channel_workers *workers)
{
	dssh_session_terminate(workers->owner->client);
	dssh_session_terminate(workers->owner->server);
	mock_io_close_c2s(&workers->owner->io);
	mock_io_close_s2c(&workers->owner->io);
}

static int
channel_worker_run(void *data)
{
	struct channel_worker_call *call = data;
	int ret = call->function(call->data);
	if (ret != 0)
		channel_workers_cancel(call->workers);
	return ret;
}

static void
channel_workers_init(struct channel_workers *workers, struct conn_ctx *owner)
{
	if (owner->workers != NULL)
		abort();
	workers->owner = owner;
	workers->release = NULL;
	workers->retire = NULL;
	workers->resource = NULL;
	owner->workers = workers;
	for (size_t i = 0; i < 2; i++) {
		workers->created[i] = false;
		workers->results[i] = 0;
	}
	channel_fixture_owner = owner;
}

static int
channel_worker_start(struct channel_workers *workers, unsigned index,
    thrd_start_t function, void *data)
{
	if (index >= 2 || workers->created[index])
		abort();
	struct channel_worker_call *call = &workers->calls[index];
	call->workers = workers;
	call->function = function;
	call->data = data;
	int ret = channel_fixture_thread_create(&workers->threads[index], channel_worker_run, call);
	if (ret != thrd_success) {
		channel_workers_cancel(workers);
		return -1;
	}
	workers->created[index] = true;
	return 0;
}

static void
channel_worker_join(struct channel_workers *workers, unsigned index)
{
	if (index >= 2)
		abort();
	if (!workers->created[index])
		return;
	if (channel_fixture_thread_join(workers->threads[index], &workers->results[index]) != thrd_success)
		abort();
	workers->created[index] = false;
}

static void
channel_workers_finish(struct channel_workers *workers)
{
	channel_worker_join(workers, 0);
	channel_worker_join(workers, 1);
	if (workers->owner->workers != workers)
		abort();
	workers->owner->workers = NULL;
	if (workers->retire != NULL)
		workers->retire(workers->resource);
	if (conn_actor_retirement_observer != NULL)
		conn_actor_retirement_observer(workers);
	channel_fixture_owner = NULL;
}

/* An assertion failure runs this while the host's argument objects are live. */
static void
channel_workers_cleanup_cancel(struct conn_ctx *ctx)
{
	struct channel_workers *workers = ctx->workers;
	if (workers == NULL)
		return;
	if (workers->release != NULL)
		workers->release(workers->resource);
	channel_workers_cancel(workers);
}

static void
channel_workers_cleanup_join(struct conn_ctx *ctx)
{
	if (ctx->workers != NULL)
		channel_workers_finish(ctx->workers);
}

static void
conn_actor_unwatch(void *unused)
{
	(void)unused;
	dssh_test_thrd_watch_condition(NULL);
}

/* The wait observation precedes the native wait while its mutex is owned.
 * Acquiring that mutex proves the worker has released it before cancellation. */
static bool
conn_actor_waiting(const atomic_bool *entered, mtx_t *mutex)
{
	if (!dssh_test_wait_bool(entered, true) || mtx_lock(mutex) != thrd_success)
		return false;
	if (mtx_unlock(mutex) != thrd_success)
		abort();
	return true;
}

/* Both worker argument objects and output slots stay live through finish. */
static int
open_channel_pair(struct conn_ctx *owner,
    thrd_start_t client_function, void *client_data, dssh_channel *client_ch,
    thrd_start_t server_function, void *server_data, dssh_channel *server_ch)
{
	*client_ch = NULL;
	*server_ch = NULL;
	struct channel_workers workers;
	channel_workers_init(&workers, owner);
	if (channel_worker_start(&workers, 0, client_function, client_data) != 0
	    || channel_worker_start(&workers, 1, server_function, server_data) != 0) {
		channel_workers_finish(&workers);
		return -1;
	}
	channel_worker_join(&workers, 0);
	if (workers.results[0] != 0 || *client_ch == NULL)
		channel_workers_cancel(&workers);
	channel_workers_finish(&workers);
	if (workers.results[0] != 0 || workers.results[1] != 0
	    || *client_ch == NULL || *server_ch == NULL)
		return -1;
	return 0;
}

static int
open_exec_channel(struct conn_ctx *owner, struct open_exec_ctx *ctx)
{
	if (owner == NULL || !owner->io_initialized || ctx->client != owner->client
	    || ctx->server != owner->server)
		return -1;
	return open_channel_pair(owner, client_open_exec_thread, ctx, &ctx->client_ch,
	    server_accept_exec_thread, ctx, &ctx->server_ch);
}

#include "conn_zc_fixture.h"
#include "conn_callback_fixture.h"
#include "conn_wire_fixture.h"
#include "conn_flow_fixture.h"
#include "conn_lifecycle_fixture.h"
#include "conn_storage_fixture.h"
#include "conn_consume_fixture.h"
#include "conn_eof_fixture.h"

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
	int ret = dssh_chan_params_init(&p, DSSH_CHAN_SHELL);
	if (ret != 0)
		return ret;
	ret = dssh_chan_params_set_term(&p, ctx->term);
	if (ret == 0)
		ret = dssh_chan_params_set_size(&p, ctx->cols, ctx->rows, ctx->wpx, ctx->hpx);
	if (ret == 0)
		ctx->client_ch = dssh_chan_open(ctx->client, &p);
	dssh_chan_params_free(&p);
	return ret == 0 ? wait_stream_writable(ctx->client_ch) : ret;
}

static int
server_accept_shell_thread(void *arg)
{
	struct open_shell_ctx *ctx = arg;
	ctx->server_ch = dssh_chan_accept(ctx->server, ctx->cbs, 30000);
	return wait_stream_writable(ctx->server_ch);
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
	int ret = dssh_chan_params_init(&p, DSSH_CHAN_SUBSYSTEM);
	if (ret != 0)
		return ret;
	ret = dssh_chan_params_set_subsystem(&p, ctx->subsystem);
	if (ret == 0)
		ret = dssh_chan_params_set_pty(&p, false);
	if (ret == 0)
		ctx->client_ch = dssh_chan_open(ctx->client, &p);
	dssh_chan_params_free(&p);
	return ret == 0 ? wait_stream_writable(ctx->client_ch) : ret;
}

static int
server_accept_subsys_thread(void *arg)
{
	struct open_subsys_ctx *ctx = arg;
	ctx->server_ch = dssh_chan_accept(ctx->server, &accept_cbs_all, 30000);
	return wait_stream_writable(ctx->server_ch);
}


/* ================================================================
 * Session start/stop (~3 tests)
 * ================================================================ */

static int
test_session_start(void)
{
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_FAIL;

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
		return TEST_FAIL;

	/* Close pipes so demux threads' read() returns EOF */
	mock_io_close_c2s(&ctx.io);
	mock_io_close_s2c(&ctx.io);

	dssh_session_stop(ctx.client);
	ASSERT_FALSE(ctx.client->demux_running);

	dssh_session_stop(ctx.server);
	ASSERT_FALSE(ctx.server->demux_running);

	conn_cleanup(&ctx);
	return TEST_PASS;
}

static int
test_session_start_stop(void)
{
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_FAIL;

	ASSERT_TRUE(ctx.client->demux_running);
	ASSERT_TRUE(ctx.server->demux_running);

	mock_io_close_c2s(&ctx.io);
	mock_io_close_s2c(&ctx.io);
	dssh_session_stop(ctx.client);
	dssh_session_stop(ctx.server);

	ASSERT_FALSE(ctx.client->demux_running);
	ASSERT_FALSE(ctx.server->demux_running);

	conn_cleanup(&ctx);
	return TEST_PASS;
}

static int
test_session_stop_open_channel(void)
{
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_FAIL;
	struct open_exec_ctx oc = {
		.client = ctx.client, .server = ctx.server,
		.command = "stop-with-owned-parameters", .cbs = &accept_cbs_all,
		.accept_timeout = 30000,
	};
	ASSERT_EQ(open_exec_channel(&ctx, &oc), 0);
	ASSERT_NOT_NULL(oc.client_ch);
	ASSERT_NOT_NULL(oc.server_ch);
	ASSERT_STR_EQ(oc.client_ch->params.command, oc.command);
	ASSERT_STR_EQ(oc.server_ch->params.command, oc.command);
	ASSERT_NOT_NULL(oc.client_ch->events.entries);
	ASSERT_NOT_NULL(oc.server_ch->events.entries);
	/* Leave both handles registered. Stop releases their owned queues,
	 * buffers and parameter snapshots before session cleanup. */
	mock_io_close_c2s(&ctx.io);
	mock_io_close_s2c(&ctx.io);
	dssh_session_stop(ctx.client);
	dssh_session_stop(ctx.server);
	ASSERT_NULL(ctx.client->channels);
	ASSERT_NULL(ctx.server->channels);
	ASSERT_EQ_U(ctx.client->channel_count, 0);
	ASSERT_EQ_U(ctx.server->channel_count, 0);
	conn_cleanup(&ctx);
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
		return TEST_FAIL;

	struct open_exec_ctx oc = {
		.client = ctx.client,
		.server = ctx.server,
		.command = "echo hello",
		.cbs = &accept_cbs_all,
		.accept_timeout = 30000,
	};

	if (open_exec_channel(&ctx, &oc) < 0) {
		conn_cleanup(&ctx);
		return TEST_FAIL;
	}

	ASSERT_NOT_NULL(oc.client_ch);
	ASSERT_NOT_NULL(oc.server_ch);

	ASSERT_EQ(dssh_chan_close(oc.client_ch, 0), 0);
	ASSERT_EQ(dssh_chan_close(oc.server_ch, 0), 0);
	conn_cleanup(&ctx);
	return TEST_PASS;
}

static int
test_open_exec_command(void)
{
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_FAIL;

	struct open_exec_ctx oc = {
		.client = ctx.client,
		.server = ctx.server,
		.command = "ls -la /tmp",
		.cbs = &accept_cbs_all,
		.accept_timeout = 30000,
	};

	if (open_exec_channel(&ctx, &oc) < 0) {
		conn_cleanup(&ctx);
		return TEST_FAIL;
	}

	/* Both owned snapshots survive the worker's builder release. */
	ASSERT_EQ(dssh_chan_get_type(oc.client_ch), DSSH_CHAN_EXEC);
	ASSERT_STR_EQ(dssh_chan_get_command(oc.client_ch), "ls -la /tmp");
	ASSERT_EQ(dssh_chan_get_type(oc.server_ch), DSSH_CHAN_EXEC);
	ASSERT_NOT_NULL(dssh_chan_get_command(oc.server_ch));
	ASSERT_STR_EQ(dssh_chan_get_command(oc.server_ch), "ls -la /tmp");

	ASSERT_EQ(dssh_chan_close(oc.client_ch, 0), 0);
	ASSERT_EQ(dssh_chan_close(oc.server_ch, 0), 0);
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
	atomic_fetch_add(&reject_exec_calls, 1);
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
		return TEST_FAIL;
	struct open_exec_ctx oc = {.client = ctx.client, .server = ctx.server,
	    .command = "rejected_cmd", .cbs = &reject_exec_cbs, .accept_timeout = 30000};
	struct channel_workers workers;
	channel_workers_init(&workers, &ctx);
	ASSERT_EQ(channel_worker_start(&workers, 0, client_open_rejected_thread, &oc), 0);
	ASSERT_EQ(channel_worker_start(&workers, 1, server_accept_cancelled_thread, &oc), 0);
	ASSERT_TRUE(conn_actor_checkpoint());
	channel_worker_join(&workers, 0);
	ASSERT_EQ(workers.results[0], 0);
	ASSERT_NULL(oc.client_ch);
	ASSERT_EQ_U(atomic_load(&reject_exec_calls), 1);
	ASSERT_FALSE(dssh_session_is_terminated(ctx.client));
	ASSERT_FALSE(dssh_session_is_terminated(ctx.server));
	/* A rejecting accept resumes waiting for another channel. */
	dssh_session_terminate(ctx.server);
	channel_workers_finish(&workers);
	ASSERT_EQ(workers.results[1], 0);
	ASSERT_NULL(oc.server_ch);
	conn_cleanup(&ctx);
	return TEST_PASS;
}


static int
test_open_shell(void)
{
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_FAIL;

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

	ASSERT_EQ(open_channel_pair(&ctx, client_open_shell_thread, &oc,
	    &oc.client_ch, server_accept_shell_thread, &oc,
	    &oc.server_ch), 0);

	ASSERT_NOT_NULL(oc.client_ch);
	ASSERT_NOT_NULL(oc.server_ch);
	ASSERT_EQ(dssh_chan_get_type(oc.server_ch), DSSH_CHAN_SHELL);
	ASSERT_TRUE(pty_received);
	ASSERT_STR_EQ(pty_term, "xterm-256color");
	ASSERT_EQ_U(pty_cols, 80);
	ASSERT_EQ_U(pty_rows, 24);

	ASSERT_EQ(dssh_chan_close(oc.client_ch, 0), 0);
	ASSERT_EQ(dssh_chan_close(oc.server_ch, 0), 0);
	conn_cleanup(&ctx);
	return TEST_PASS;
}

#include "conn_stream_fixture.h"

static int
test_open_subsystem(void)
{
	ASSERT_EQ(stream_body_run(STREAM_SUBSYS_OPEN), TEST_PASS);
	return TEST_PASS;
}

/* ================================================================
 * Session I/O (~8 tests)
 * ================================================================ */

static int
test_session_write_read(void)
{
	return stream_body_run(STREAM_FORWARD);
}

static int
test_session_read_write_reverse(void)
{
	return stream_body_run(STREAM_REVERSE);
}

static int
test_session_write_ext(void)
{
	return stream_body_run(STREAM_STDERR_REVERSE);
}

static int
test_session_read_ext(void)
{
	return stream_body_run(STREAM_STDERR_FORWARD);
}

static int
test_session_poll_read(void)
{
	return stream_body_run(STREAM_POLL_READ);
}

static int
test_session_poll_timeout(void)
{
	return stream_body_run(STREAM_POLL_TIMEOUT);
}

static int
test_session_large_write(void)
{
	return stream_body_run(STREAM_LARGE);
}

static int
test_session_multiple_writes(void)
{
	return stream_body_run(STREAM_MULTIPLE);
}

/* ================================================================
 * Subsystem channel I/O (~4 tests)
 * ================================================================ */

static int
test_subsys_write_read(void)
{
	ASSERT_EQ(stream_body_run(STREAM_SUBSYS_FORWARD), TEST_PASS);
	return TEST_PASS;
}

static int
test_subsys_multiple_writes(void)
{
	ASSERT_EQ(stream_body_run(STREAM_SUBSYS_MULTIPLE), TEST_PASS);
	return TEST_PASS;
}

static int
test_subsys_poll(void)
{
	ASSERT_EQ(stream_body_run(STREAM_SUBSYS_POLL), TEST_PASS);
	return TEST_PASS;
}

static int
test_subsys_large_write(void)
{
	ASSERT_EQ(stream_body_run(STREAM_SUBSYS_LARGE), TEST_PASS);
	return TEST_PASS;
}

/* ================================================================
 * Signals (~3 tests)
 * ================================================================ */

#include "conn_event_fixture.h"

static int
test_send_signal(void)
{
	ASSERT_EQ(event_body_run(EVENT_BODY_SIGNAL), TEST_PASS);
	return TEST_PASS;
}

static int
test_signal_after_data(void)
{
	ASSERT_EQ(event_body_run(EVENT_BODY_AFTER_DATA), TEST_PASS);
	return TEST_PASS;
}

static int
test_window_change(void)
{
	ASSERT_EQ(event_body_run(EVENT_BODY_WINDOW), TEST_PASS);
	return TEST_PASS;
}

/* ================================================================
 * Close and exit (~5 tests)
 * ================================================================ */

static int
test_session_close_exit_code(void)
{
	ASSERT_EQ(event_body_run(EVENT_BODY_EXIT), TEST_PASS);
	return TEST_PASS;
}

static int
test_channel_close_subsys(void)
{
	ASSERT_EQ(stream_body_run(STREAM_SUBSYS_CLOSE), TEST_PASS);
	return TEST_PASS;
}

static int
test_eof_detection(void)
{
	return stream_body_run(STREAM_CLOSE);
}

static int
test_eof_half_close(void)
{
	return stream_body_run(STREAM_HALF_CLOSE);
}

/*
 * Stream I/O follows non-blocking POSIX semantics:
 *   read returns 0 only on real EOF; NOMORE for empty-no-EOF.
 *   write returns 0 only when bufsz == 0; NOMORE for window-full.
 *   Peek (buf == NULL) returns the buffered count, including zero.
 */
static int
test_read_nomore_vs_eof(void)
{
	return stream_body_run(STREAM_NOMORE);
}

static int
test_graceful_disconnect(void)
{
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_FAIL;

	struct open_exec_ctx oc = {
		.client = ctx.client,
		.server = ctx.server,
		.command = "cmd",
		.cbs = &accept_cbs_all,
		.accept_timeout = 30000,
	};
	if (open_exec_channel(&ctx, &oc) < 0) {
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
	if (wait_stream_writable(sctx->ch1) != 0)
		return -1;
	sctx->ch2 = dssh_chan_accept(sctx->server, sctx->cbs, 30000);
	return wait_stream_writable(sctx->ch2);
}


static int
test_two_channels(void)
{
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_FAIL;
	struct two_channel_server_ctx sctx = {.server = ctx.server, .cbs = &accept_cbs_all};
	struct channel_workers workers;
	channel_workers_init(&workers, &ctx);
	ASSERT_EQ(channel_worker_start(&workers, 0, server_accept_two_channels, &sctx), 0);
	ASSERT_TRUE(conn_actor_checkpoint());
	dssh_channel c1, c2;
	ASSERT_EQ(conn_client_open(ctx.client, DSSH_CHAN_EXEC, "cmd1", &c1), 0);
	ASSERT_NOT_NULL(c1);
	ASSERT_EQ(conn_client_open(ctx.client, DSSH_CHAN_EXEC, "cmd2", &c2), 0);
	ASSERT_NOT_NULL(c2);
	channel_workers_finish(&workers);
	ASSERT_EQ(workers.results[0], 0);
	ASSERT_NOT_NULL(sctx.ch1);
	ASSERT_NOT_NULL(sctx.ch2);
	/* Each complete payload must arrive on its own channel. */
	const uint8_t first[] = "chan1data";
	const uint8_t second[] = "chan2data";
	uint8_t received[sizeof(first)];
	ASSERT_TRUE(conn_zc_write_all(c1, first, sizeof(first), 5));
	ASSERT_TRUE(conn_zc_write_all(c2, second, sizeof(second), 5));
	ASSERT_TRUE(conn_zc_read_exact(sctx.ch1, received, sizeof(received), 5));
	ASSERT_MEM_EQ(received, first, sizeof(first));
	ASSERT_TRUE(conn_zc_read_exact(sctx.ch2, received, sizeof(received), 5));
	ASSERT_MEM_EQ(received, second, sizeof(second));
	ASSERT_EQ(dssh_chan_close(c1, 0), 0);
	ASSERT_EQ(dssh_chan_close(sctx.ch1, 0), 0);
	ASSERT_EQ(dssh_chan_close(c2, 0), 0);
	ASSERT_EQ(dssh_chan_close(sctx.ch2, 0), 0);
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
	sctx->ch = dssh_chan_accept(sctx->server, &reject_exec_cbs, 30000);
	return wait_stream_writable(sctx->ch);
}


static int
test_accept_reject(void)
{
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_FAIL;
	struct accept_reject_server_ctx sctx = {.server = ctx.server};
	struct channel_workers workers;
	channel_workers_init(&workers, &ctx);
	ASSERT_EQ(channel_worker_start(&workers, 0, server_accept_loops_on_reject, &sctx), 0);
	ASSERT_TRUE(conn_actor_checkpoint());
	dssh_channel rejected, accepted;
	ASSERT_EQ(conn_client_open(ctx.client, DSSH_CHAN_EXEC, "rejected", &rejected), 0);
	ASSERT_NULL(rejected);
	ASSERT_EQ_U(atomic_load(&reject_exec_calls), 1);
	ASSERT_FALSE(dssh_session_is_terminated(ctx.client));
	ASSERT_EQ(conn_client_open(ctx.client, DSSH_CHAN_SHELL, NULL, &accepted), 0);
	ASSERT_NOT_NULL(accepted);
	channel_workers_finish(&workers);
	ASSERT_EQ(workers.results[0], 0);
	ASSERT_NOT_NULL(sctx.ch);
	ASSERT_EQ(dssh_chan_get_type(sctx.ch), DSSH_CHAN_SHELL);
	ASSERT_EQ(dssh_chan_close(accepted, 0), 0);
	ASSERT_EQ(dssh_chan_close(sctx.ch, 0), 0);
	conn_cleanup(&ctx);
	return TEST_PASS;
}


static int
test_channel_independence(void)
{
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_FAIL;
	struct two_channel_server_ctx sctx = {.server = ctx.server, .cbs = &accept_cbs_all};
	struct channel_workers workers;
	channel_workers_init(&workers, &ctx);
	ASSERT_EQ(channel_worker_start(&workers, 0, server_accept_two_channels, &sctx), 0);
	ASSERT_TRUE(conn_actor_checkpoint());
	dssh_channel c1, c2;
	ASSERT_EQ(conn_client_open(ctx.client, DSSH_CHAN_EXEC, "cmd1", &c1), 0);
	ASSERT_NOT_NULL(c1);
	ASSERT_EQ(conn_client_open(ctx.client, DSSH_CHAN_EXEC, "cmd2", &c2), 0);
	ASSERT_NOT_NULL(c2);
	channel_workers_finish(&workers);
	ASSERT_EQ(workers.results[0], 0);
	ASSERT_NOT_NULL(sctx.ch1);
	ASSERT_NOT_NULL(sctx.ch2);
	ASSERT_EQ(dssh_chan_close(c1, 0), 0);
	ASSERT_EQ(dssh_chan_close(sctx.ch1, 0), 0);
	/* Closing the first pair must leave the complete second payload usable. */
	const uint8_t expected[] = "still alive";
	uint8_t received[sizeof(expected)];
	ASSERT_TRUE(conn_zc_write_all(c2, expected, sizeof(expected), 5));
	ASSERT_TRUE(conn_zc_read_exact(sctx.ch2, received, sizeof(received), 5));
	ASSERT_MEM_EQ(received, expected, sizeof(expected));
	ASSERT_EQ(dssh_chan_close(c2, 0), 0);
	ASSERT_EQ(dssh_chan_close(sctx.ch2, 0), 0);
	conn_cleanup(&ctx);
	return TEST_PASS;
}


/* ================================================================
 * Thread safety (~2 tests)
 * ================================================================ */

struct writer_ctx {
	dssh_channel ch;
	size_t total_written;
};

static int
writer_thread_func(void *arg)
{
	struct writer_ctx *wc = arg;
	uint8_t buffer[128];
	memset(buffer, 'W', sizeof(buffer));
	wc->total_written = 0;
	for (unsigned i = 0; i < 100; i++) {
		if (!conn_zc_write_all(wc->ch, buffer, sizeof(buffer), 5))
			return -1;
		/* Exactly 100 bounded complete writes; 12800 fits 16-bit size_t. */
		wc->total_written += sizeof(buffer);
	}
	return 0;
}


struct reader_ctx {
	dssh_channel ch;
	size_t total_read;
	bool eof;
};

static int
reader_thread_func(void *arg)
{
	struct reader_ctx *rc = arg;
	uint8_t buffer[256];
	rc->total_read = 0;
	rc->eof = false;
	for (;;) {
		if (dssh_chan_poll(rc->ch, DSSH_POLL_READ, 2000) != DSSH_POLL_READ)
			return -1;
		int64_t ret = conn_zc_read(rc->ch, 0, buffer, sizeof(buffer));
		if (ret == DSSH_ERROR_NOMORE)
			continue;
		size_t count;
		if (!conn_zc_io_count(ret, sizeof(buffer), &count)
		    || rc->total_read > 12800 || count > 12800 - rc->total_read)
			return -1;
		if (count == 0) {
			rc->eof = true;
			return 0;
		}
		for (size_t i = 0; i < count; i++) {
			if (buffer[i] != 'W')
				return -1;
		}
		rc->total_read += count;
	}
}


static int
test_concurrent_write_read(void)
{
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_FAIL;
	struct open_exec_ctx oc = {.client = ctx.client, .server = ctx.server,
	    .command = "cmd", .cbs = &accept_cbs_all, .accept_timeout = 30000};
	ASSERT_EQ(open_exec_channel(&ctx, &oc), 0);
	struct writer_ctx wc = {.ch = oc.client_ch};
	struct reader_ctx rc = {.ch = oc.server_ch};
	struct channel_workers workers;
	channel_workers_init(&workers, &ctx);
	ASSERT_EQ(channel_worker_start(&workers, 0, reader_thread_func, &rc), 0);
	ASSERT_EQ(channel_worker_start(&workers, 1, writer_thread_func, &wc), 0);
	ASSERT_TRUE(conn_actor_checkpoint());
	channel_worker_join(&workers, 1);
	ASSERT_EQ(workers.results[1], 0);
	ASSERT_EQ_U(wc.total_written, 12800);
	/* EOF follows the entire payload and leaves the reader's channel owned. */
	ASSERT_EQ(dssh_chan_shutwr(oc.client_ch), 0);
	channel_workers_finish(&workers);
	ASSERT_EQ(workers.results[0], 0);
	ASSERT_TRUE(rc.eof);
	ASSERT_EQ_U(rc.total_read, wc.total_written);
	ASSERT_EQ(dssh_chan_close(oc.client_ch, 0), 0);
	ASSERT_EQ(dssh_chan_close(oc.server_ch, 0), 0);
	conn_cleanup(&ctx);
	return TEST_PASS;
}


static int
test_terminate_during_io(void)
{
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_FAIL;
	struct open_exec_ctx oc = {.client = ctx.client, .server = ctx.server,
	    .command = "cmd", .cbs = &accept_cbs_all, .accept_timeout = 30000};
	ASSERT_EQ(open_exec_channel(&ctx, &oc), 0);
	struct reader_ctx rc = {.ch = oc.server_ch};
	struct channel_workers workers;
	channel_workers_init(&workers, &ctx);
	workers.retire = conn_actor_unwatch;
	atomic_bool *entered = dssh_test_thrd_watch_condition(&oc.server_ch->poll_cnd);
	ASSERT_EQ(channel_worker_start(&workers, 0, reader_thread_func, &rc), 0);
	ASSERT_TRUE(conn_actor_checkpoint());
	ASSERT_TRUE(conn_actor_waiting(entered, &oc.server_ch->buf_mtx));
	dssh_session_terminate(ctx.server);
	channel_workers_finish(&workers);
	ASSERT_EQ(workers.results[0], 0);
	ASSERT_TRUE(rc.eof);
	ASSERT_EQ_U(rc.total_read, 0);
	ASSERT_TRUE(dssh_session_is_terminated(ctx.server));
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
		return TEST_FAIL;
	struct open_exec_ctx oc = {.client = ctx.client, .server = ctx.server,
	    .command = "cmd", .cbs = &accept_cbs_all, .accept_timeout = 30000};
	ASSERT_EQ(open_exec_channel(&ctx, &oc), 0);
	struct reader_ctx rc = {.ch = oc.server_ch};
	struct channel_workers workers;
	channel_workers_init(&workers, &ctx);
	workers.retire = conn_actor_unwatch;
	atomic_bool *entered = dssh_test_thrd_watch_condition(&oc.server_ch->poll_cnd);
	ASSERT_EQ(channel_worker_start(&workers, 0, reader_thread_func, &rc), 0);
	ASSERT_TRUE(conn_actor_checkpoint());
	ASSERT_TRUE(conn_actor_waiting(entered, &oc.server_ch->buf_mtx));
	mock_io_close_c2s(&ctx.io);
	mock_io_close_s2c(&ctx.io);
	channel_workers_finish(&workers);
	ASSERT_EQ(workers.results[0], 0);
	ASSERT_TRUE(rc.eof);
	ASSERT_EQ_U(rc.total_read, 0);
	ASSERT_TRUE(dssh_session_is_terminated(ctx.server));
	conn_cleanup(&ctx);
	return TEST_PASS;
}


static int
test_connection_drop_during_write(void)
{
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_FAIL;

	struct open_exec_ctx oc = {
		.client = ctx.client,
		.server = ctx.server,
		.command = "cmd",
		.cbs = &accept_cbs_all,
		.accept_timeout = 30000,
	};
	if (open_exec_channel(&ctx, &oc) < 0) {
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
		return TEST_FAIL;

	struct open_exec_ctx oc = {
		.client = ctx.client,
		.server = ctx.server,
		.command = "cmd",
		.cbs = &accept_cbs_all,
		.accept_timeout = 30000,
	};
	if (open_exec_channel(&ctx, &oc) < 0) {
		conn_cleanup(&ctx);
		return TEST_SKIP;
	}

	mock_io_close_c2s(&ctx.io);
	mock_io_close_s2c(&ctx.io);

	/* This must not hang or crash */
	g_active_ctx = NULL;
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
	ASSERT_EQ(res, DSSH_ERROR_INIT);
	ASSERT_TRUE(ctx.client->demux_running);
	ASSERT_TRUE(ctx.server->demux_running);

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
	return stream_body_run(STREAM_NOWAIT);
}

/* ================================================================
 * Auto-reject channel types -- covers demux_channel_open branches.
 * ================================================================ */

static size_t
build_channel_open(uint8_t *buf, size_t bufsz, const char *type,
    uint32_t sender_ch, uint32_t window, uint32_t max_packet)
{
	size_t tlen = strlen(type);
	if (tlen > UINT32_MAX || tlen > SIZE_MAX - 17 || tlen + 17 > bufsz)
		return 0;
	uint32_t wire_len = (uint32_t)tlen;
	size_t pos = 0;
	buf[pos++] = SSH_MSG_CHANNEL_OPEN;
	DSSH_PUT_U32(wire_len, buf, &pos);
	memcpy(&buf[pos], type, tlen);
	pos += tlen;
	DSSH_PUT_U32(sender_ch, buf, &pos);
	DSSH_PUT_U32(window, buf, &pos);
	DSSH_PUT_U32(max_packet, buf, &pos);
	return pos;
}

/* Reserve TX so the reply remains inspectable in its normal queued form. */
static bool
expect_open_failure(dssh_session sess, uint8_t *msg, size_t len,
    uint32_t peer_channel, uint32_t reason)
{
	if (tx_acquire(sess, SSH_MSG_IGNORE) < 0)
		return false;
	int ret = dssh_test_demux_dispatch(sess, SSH_MSG_CHANNEL_OPEN, msg, len);
	int lock_ret = mtx_lock(&sess->trans.tx_queue_mtx);
	bool correct = false;
	if (lock_ret == thrd_success) {
		struct dssh_tx_slot *slot = &sess->trans.open_fail_slot;
		correct = ret == 0 && atomic_load(&slot->ready) && slot->payload_len == 17
		    && slot->buf[9] == SSH_MSG_CHANNEL_OPEN_FAILURE
		    && DSSH_GET_U32(&slot->buf[10]) == peer_channel
		    && DSSH_GET_U32(&slot->buf[14]) == reason
		    && DSSH_GET_U32(&slot->buf[18]) == 0
		    && DSSH_GET_U32(&slot->buf[22]) == 0;
		if (mtx_unlock(&sess->trans.tx_queue_mtx) != thrd_success)
			correct = false;
	}
	tx_unlock_with_slots(sess);
	return correct;
}

static uint32_t
open_state_zc_cb(dssh_channel ch, int stream, const uint8_t *data, size_t len, void *cbdata)
{
	(void)ch;
	(void)stream;
	(void)data;
	(void)len;
	(void)cbdata;
	return 0;
}

static int
test_channel_before_start(void)
{
	dssh_test_reset_global_config();
	ASSERT_OK(register_all_algorithms());
	dssh_session sess = init_server_session();
	ASSERT_NOT_NULL(sess);
	struct dssh_chan_params params;
	ASSERT_OK(dssh_chan_params_init(&params, DSSH_CHAN_EXEC));
	ASSERT_OK(dssh_chan_params_set_command(&params, "before-start"));
	dssh_test_alloc_fail_after(0);
	dssh_test_thrd_fail_after(0);
	dssh_channel accepted = dssh_chan_accept(sess, &accept_cbs_all, 0);
	dssh_channel stream = dssh_chan_open(sess, &params);
	dssh_channel zc = dssh_chan_zc_open(sess, &params, open_state_zc_cb, NULL);
	int allocations = dssh_test_alloc_count();
	int synchronization = dssh_test_thrd_count();
	dssh_test_alloc_reset();
	dssh_test_thrd_reset();
	bool terminated = dssh_session_is_terminated(sess);
	dssh_chan_params_free(&params);
	dssh_session_cleanup(sess);
	dssh_test_reset_global_config();
	ASSERT_NULL(accepted);
	ASSERT_NULL(stream);
	ASSERT_NULL(zc);
	ASSERT_EQ(allocations, 0);
	ASSERT_EQ(synchronization, 0);
	ASSERT_FALSE(terminated);
	return TEST_PASS;
}

static int
test_open_type_and_queue_policy(void)
{
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_FAIL;
	uint8_t msg[256];
	const char *unknown[] = { "", "unknown", "session@example.invalid" };
	for (size_t i = 0; i < sizeof(unknown) / sizeof(unknown[0]); i++) {
		size_t len = build_channel_open(msg, sizeof(msg), unknown[i], 60000, 123, 456);
		ASSERT_TRUE(expect_open_failure(ctx.server, msg, len, 60000, 3));
		ASSERT_TRUE(expect_open_failure(ctx.client, msg, len, 60000, 3));
	}
	char long_type[129];
	memset(long_type, 'a', sizeof(long_type) - 1);
	long_type[sizeof(long_type) - 1] = 0;
	size_t len = build_channel_open(msg, sizeof(msg), long_type, 60000, 123, 456);
	ASSERT_TRUE(expect_open_failure(ctx.server, msg, len, 60000, 3));
	const char *forbidden[] = { "x11", "forwarded-tcpip", "direct-tcpip" };
	for (size_t i = 0; i < sizeof(forbidden) / sizeof(forbidden[0]); i++) {
		len = build_channel_open(msg, sizeof(msg), forbidden[i], 60000, 123, 456);
		ASSERT_TRUE(expect_open_failure(ctx.server, msg, len, 60000, 1));
		ASSERT_TRUE(expect_open_failure(ctx.client, msg, len, 60000, 1));
	}
	len = build_channel_open(msg, sizeof(msg), "session", 60000, 123, 456);
	ASSERT_TRUE(expect_open_failure(ctx.client, msg, len, 60000, 1));
	ASSERT_EQ(mtx_lock(&ctx.server->accept_mtx), thrd_success);
	size_t queued = ctx.server->accept_queue.count;
	ASSERT_EQ(mtx_unlock(&ctx.server->accept_mtx), thrd_success);
	ASSERT_EQ_U(queued, 0);
	struct open_exec_ctx oc = {
		.client = ctx.client, .server = ctx.server,
		.command = "queue-progress", .cbs = &accept_cbs_all,
		.accept_timeout = 30000,
	};
	ASSERT_OK(open_exec_channel(&ctx, &oc));
	for (uint32_t i = 0; i < DSSH_ACCEPT_QUEUE_CAP; i++) {
		len = build_channel_open(msg, sizeof(msg), "session", i + 32, 123, 456);
		ASSERT_OK(dssh_test_demux_dispatch(ctx.server, SSH_MSG_CHANNEL_OPEN, msg, len));
	}
	len = build_channel_open(msg, sizeof(msg), "session", 60000, 123, 456);
	ASSERT_TRUE(expect_open_failure(ctx.server, msg, len, 60000, 4));
	static const uint8_t data[] = "queue is full";
	ASSERT_EQ(dssh_chan_write(oc.client_ch, 0, data, sizeof(data)), sizeof(data));
	ASSERT_TRUE(dssh_chan_poll(oc.server_ch, DSSH_POLL_READ, 5000) & DSSH_POLL_READ);
	uint8_t received[sizeof(data)];
	ASSERT_EQ(dssh_chan_read(oc.server_ch, 0, received, sizeof(received)), sizeof(data));
	ASSERT_MEM_EQ(received, data, sizeof(data));
	for (uint32_t i = 0; i < DSSH_ACCEPT_QUEUE_CAP; i++) {
		struct dssh_incoming_open inc;
		ASSERT_OK(accept_incoming(ctx.server, &inc, 0));
		ASSERT_EQ_U(inc.peer_channel, i + 32);
		ASSERT_EQ_U(inc.peer_window, 123);
		ASSERT_EQ_U(inc.peer_max_packet, 456);
		ASSERT_STR_EQ(inc.channel_type, "session");
	}
	struct dssh_incoming_open inc;
	ASSERT_EQ(accept_incoming(ctx.server, &inc, 0), DSSH_ERROR_NOMORE);
	ASSERT_OK(dssh_test_demux_dispatch(ctx.server, SSH_MSG_CHANNEL_OPEN, msg, len));
	ASSERT_OK(accept_incoming(ctx.server, &inc, 0));
	ASSERT_EQ_U(inc.peer_channel, 60000);
	ASSERT_EQ(mtx_lock(&ctx.server->channel_mtx), thrd_success);
	ctx.server->next_channel_id = UINT32_MAX;
	ASSERT_EQ(mtx_unlock(&ctx.server->channel_mtx), thrd_success);
	ASSERT_EQ(alloc_channel_id(ctx.server), UINT32_MAX);
	/* The real active channel owns ID 0, so allocation skips it after wrap. */
	ASSERT_EQ(alloc_channel_id(ctx.server), 1);
	ASSERT_FALSE(dssh_session_is_terminated(ctx.server));
	conn_cleanup(&ctx);
	return TEST_PASS;
}

static int
test_open_response_state(void)
{
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_FAIL;
	struct open_exec_ctx oc = {
		.client = ctx.client, .server = ctx.server,
		.command = "response-state", .cbs = &accept_cbs_all,
		.accept_timeout = 30000,
	};
	ASSERT_OK(open_exec_channel(&ctx, &oc));
	struct dssh_channel_s *channels[] = { oc.client_ch, oc.server_ch };
	for (size_t side = 0; side < 2; side++) {
		struct dssh_channel_s *ch = channels[side];
		uint8_t confirmation[18];
		uint8_t failure[23];
		size_t pos = 0;
		confirmation[pos++] = SSH_MSG_CHANNEL_OPEN_CONFIRMATION;
		DSSH_PUT_U32(ch->local_id, confirmation, &pos);
		DSSH_PUT_U32(UINT32_MAX, confirmation, &pos);
		DSSH_PUT_U32(7, confirmation, &pos);
		DSSH_PUT_U32(11, confirmation, &pos);
		confirmation[pos] = 1;
		uint32_t original_id = ch->remote_id;
		uint32_t original_packet = ch->remote_max_packet;
		uint_least32_t original_window = atomic_load(&ch->remote_window);
		ASSERT_TRUE(original_window <= UINT32_MAX);
		for (size_t len = 0; len <= sizeof(confirmation); len++)
			ASSERT_EQ(dssh_test_demux_dispatch(ch->sess, confirmation[0], confirmation, len),
			    DSSH_ERROR_PARSE);
		pos = 0;
		failure[pos++] = SSH_MSG_CHANNEL_OPEN_FAILURE;
		DSSH_PUT_U32(ch->local_id, failure, &pos);
		DSSH_PUT_U32(1, failure, &pos);
		DSSH_PUT_U32(3, failure, &pos);
		memcpy(&failure[pos], "bad", 3);
		pos += 3;
		DSSH_PUT_U32(2, failure, &pos);
		memcpy(&failure[pos], "en", 2);
		pos += 2;
		failure[pos] = 1;
		ASSERT_EQ(pos, 22);
		ASSERT_EQ(dssh_test_demux_dispatch(ch->sess, failure[0], failure, pos), DSSH_ERROR_PARSE);
		ASSERT_EQ_U(ch->remote_id, original_id);
		ASSERT_EQ_U(ch->remote_max_packet, original_packet);
		ASSERT_EQ_U(atomic_load(&ch->remote_window), original_window);
		ASSERT_TRUE(ch->open);
		ASSERT_FALSE(ch->close_received);
		ASSERT_FALSE(ch->open_pending);

		ASSERT_EQ(mtx_lock(&ch->buf_mtx), thrd_success);
		ch->open = false;
		ch->open_pending = true;
		ASSERT_EQ(mtx_unlock(&ch->buf_mtx), thrd_success);
		ASSERT_EQ(mtx_lock(&ch->sess->channel_mtx), thrd_success);
		ch->protocol_id->phase = DSSH_CHANNEL_ID_OPENING;
		ASSERT_EQ(mtx_unlock(&ch->sess->channel_mtx), thrd_success);
		uint8_t eof[5];
		size_t eof_pos = 0;
		eof[eof_pos++] = SSH_MSG_CHANNEL_EOF;
		DSSH_PUT_U32(ch->local_id, eof, &eof_pos);
		ASSERT_EQ(dssh_test_demux_dispatch(ch->sess, eof[0], eof, eof_pos), DSSH_ERROR_PARSE);
		ASSERT_FALSE(ch->eof_received);
		for (size_t len = 0; len <= sizeof(failure); len++) {
			if (len == pos)
				continue;
			ASSERT_EQ(dssh_test_demux_dispatch(ch->sess, failure[0], failure, len), DSSH_ERROR_PARSE);
		}
		uint8_t saved = failure[9];
		failure[9] = 255;
		ASSERT_EQ(dssh_test_demux_dispatch(ch->sess, failure[0], failure, pos), DSSH_ERROR_PARSE);
		failure[9] = saved;
		size_t reason_pos = 5;
		DSSH_PUT_U32(0, failure, &reason_pos);
		ASSERT_EQ(dssh_test_demux_dispatch(ch->sess, failure[0], failure, pos), DSSH_ERROR_PARSE);
		ASSERT_FALSE(ch->close_received);
		ASSERT_TRUE(ch->open_pending);
		reason_pos = 5;
		DSSH_PUT_U32(UINT32_MAX, failure, &reason_pos);
		ASSERT_OK(dssh_test_demux_dispatch(ch->sess, failure[0], failure, pos));
		ASSERT_FALSE(ch->open_pending);
		ASSERT_TRUE(ch->close_received);
		ASSERT_EQ(wait_open_response(ch->sess, ch), DSSH_ERROR_REJECTED);
		ASSERT_EQ(dssh_test_demux_dispatch(ch->sess, confirmation[0], confirmation, 17), DSSH_ERROR_PARSE);
		ASSERT_EQ_U(ch->remote_id, original_id);
		ASSERT_EQ_U(atomic_load(&ch->remote_window), original_window);

		ASSERT_EQ(mtx_lock(&ch->buf_mtx), thrd_success);
		ch->open_pending = true;
		ch->close_received = false;
		ch->close_sent = true;
		ASSERT_EQ(mtx_unlock(&ch->buf_mtx), thrd_success);
		ASSERT_EQ(mtx_lock(&ch->sess->channel_mtx), thrd_success);
		ch->protocol_id->phase = DSSH_CHANNEL_ID_OPENING;
		ASSERT_EQ(mtx_unlock(&ch->sess->channel_mtx), thrd_success);
		ASSERT_EQ(dssh_test_demux_dispatch(ch->sess, confirmation[0], confirmation, 17), DSSH_ERROR_PARSE);
		ASSERT_EQ(mtx_lock(&ch->buf_mtx), thrd_success);
		ch->close_sent = false;
		ASSERT_EQ(mtx_unlock(&ch->buf_mtx), thrd_success);
		/* A pending channel accepts exactly one well-formed confirmation. */
		pos = 5;
		DSSH_PUT_U32(original_id, confirmation, &pos);
		uint32_t wire_window = (uint32_t)original_window;
		DSSH_PUT_U32(wire_window, confirmation, &pos);
		DSSH_PUT_U32(original_packet, confirmation, &pos);
		ASSERT_OK(dssh_test_demux_dispatch(ch->sess, confirmation[0], confirmation, pos));
		ASSERT_FALSE(ch->open_pending);
		ASSERT_TRUE(ch->open);
		ASSERT_OK(wait_open_response(ch->sess, ch));
	}
	static const uint8_t data[] = "after response checks";
	ASSERT_EQ(dssh_chan_write(oc.client_ch, 0, data, sizeof(data)), sizeof(data));
	ASSERT_TRUE(dssh_chan_poll(oc.server_ch, DSSH_POLL_READ, 5000) & DSSH_POLL_READ);
	uint8_t received[sizeof(data)];
	ASSERT_EQ(dssh_chan_read(oc.server_ch, 0, received, sizeof(received)), sizeof(data));
	ASSERT_MEM_EQ(received, data, sizeof(data));
	conn_cleanup(&ctx);
	return TEST_PASS;
}

struct rejected_open_ctx {
	struct open_exec_ctx open;
	atomic_bool done;
};

static int
rejected_open_thread(void *arg)
{
	struct rejected_open_ctx *ctx = arg;
	int ret = client_open_rejected_thread(&ctx->open);
	atomic_store(&ctx->done, true);
	return ret;
}

static int
test_open_failure_roundtrip(void)
{
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_FAIL;
	struct rejected_open_ctx rejected = { .open = {
		.client = ctx.client, .command = "rejected-open",
	} };
	atomic_init(&rejected.done, false);
	struct channel_workers workers;
	channel_workers_init(&workers, &ctx);
	ASSERT_EQ(channel_worker_start(&workers, 0, rejected_open_thread, &rejected), 0);
	ASSERT_TRUE(conn_actor_checkpoint());
	struct dssh_incoming_open inc;
	int ret = accept_incoming(ctx.server, &inc, 5000);
	if (ret == 0) {
		uint8_t failure[17];
		size_t pos = 0;
		failure[pos++] = SSH_MSG_CHANNEL_OPEN_FAILURE;
		DSSH_PUT_U32(inc.peer_channel, failure, &pos);
		DSSH_PUT_U32(1, failure, &pos);
		DSSH_PUT_U32(0, failure, &pos);
		DSSH_PUT_U32(0, failure, &pos);
		ret = send_packet(ctx.server, failure, pos, NULL);
	}
	bool done = ret == 0 && dssh_test_wait_bool(&rejected.done, true);
	if (!done) {
		dssh_session_terminate(ctx.client);
		mock_io_close_c2s(&ctx.io);
		mock_io_close_s2c(&ctx.io);
	}
	channel_workers_finish(&workers);
	ASSERT_EQ(workers.results[0], 0);
	ASSERT_TRUE(done);
	ASSERT_NULL(rejected.open.client_ch);
	ASSERT_FALSE(dssh_session_is_terminated(ctx.client));
	ASSERT_EQ(mtx_lock(&ctx.client->channel_mtx), thrd_success);
	size_t remaining = ctx.client->channel_count;
	if (mtx_unlock(&ctx.client->channel_mtx) != thrd_success)
		abort();
	ASSERT_EQ_U(remaining, 0);
	conn_cleanup(&ctx);
	return TEST_PASS;
}

static int
test_open_sync_failures(void)
{
	for (int operation = 0; operation < 7; operation++) {
		/* Recoverable acquisitions/waits/signals only. Releases use the
		 * separate provenance-checked process-isolated fatal matrix. */
		int limit = operation == 6 ? 48
		    : (operation == 0 || operation == 1 || operation == 4) ? 2 : 1;
		bool completed = operation != 6;
		for (int n = 0; n < limit; n++) {
			struct conn_ctx ctx;
			if (conn_setup(&ctx) < 0)
				return TEST_FAIL;
			uint8_t msg[32];
			size_t len = build_channel_open(msg, sizeof(msg), "session", 60000, 123, 456);
			struct dssh_incoming_open inc;
			struct open_exec_ctx oc = {
				.client = ctx.client, .server = ctx.server,
				.command = "open-sync", .cbs = &accept_cbs_all,
				.accept_timeout = 30000,
			};
			if (operation == 3 || operation == 4)
				ASSERT_OK(open_exec_channel(&ctx, &oc));
			if (operation == 4) {
				ASSERT_EQ(mtx_lock(&oc.server_ch->buf_mtx), thrd_success);
				oc.server_ch->open = false;
				oc.server_ch->open_pending = true;
				ASSERT_EQ(mtx_unlock(&oc.server_ch->buf_mtx), thrd_success);
			}
			if (operation == 5)
				ASSERT_OK(dssh_test_demux_dispatch(ctx.server, SSH_MSG_CHANNEL_OPEN, msg, len));
			if (operation == 6)
				len = build_channel_open(msg, sizeof(msg), "unknown", 60000, 123, 456);
			dssh_test_thrd_fail_after(n);
			int64_t ret;
			if (operation == 0 || operation == 6)
				ret = dssh_test_demux_dispatch(ctx.server, SSH_MSG_CHANNEL_OPEN, msg, len);
			else if (operation == 1 || operation == 5)
				ret = accept_incoming(ctx.server, &inc, -1);
			else if (operation == 2)
				ret = alloc_channel_id(ctx.server);
			else
				ret = wait_open_response(ctx.server, oc.server_ch);
			int calls = dssh_test_thrd_count();
			dssh_test_thrd_reset();
			bool terminated = dssh_session_is_terminated(ctx.server);
			conn_cleanup(&ctx);
			if (operation != 6 || calls > n) {
				ASSERT_EQ(ret, DSSH_ERROR_TERMINATED);
				ASSERT_TRUE(terminated);
			}
			else {
				ASSERT_OK(ret);
				ASSERT_FALSE(terminated);
				completed = true;
				break;
			}
		}
		ASSERT_TRUE(completed);
	}
	return TEST_PASS;
}

static int
test_channel_io_validation(void)
{
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_FAIL;
	struct open_exec_ctx oc = {
		.client = ctx.client, .server = ctx.server,
		.command = "io-validation", .cbs = &accept_cbs_all,
		.accept_timeout = 30000,
	};
	ASSERT_OK(open_exec_channel(&ctx, &oc));
	uint8_t byte = 1;
	ASSERT_EQ(dssh_chan_read(oc.client_ch, -1, &byte, 1), DSSH_ERROR_INVALID);
	ASSERT_EQ(dssh_chan_read(oc.client_ch, 2, NULL, 0), DSSH_ERROR_INVALID);
	ASSERT_EQ(dssh_chan_write(oc.client_ch, -1, &byte, 0), DSSH_ERROR_INVALID);
	ASSERT_EQ(dssh_chan_write(oc.client_ch, 2, &byte, 1), DSSH_ERROR_INVALID);
	ASSERT_EQ(dssh_chan_poll(oc.client_ch, -1, 0), DSSH_ERROR_INVALID);
	ASSERT_EQ(dssh_chan_poll(oc.client_ch, 8, 0), DSSH_ERROR_INVALID);
	ASSERT_FALSE(dssh_session_is_terminated(ctx.client));
	ASSERT_TRUE(dssh_chan_poll(oc.client_ch, DSSH_POLL_WRITE, 0) & DSSH_POLL_WRITE);
	ASSERT_EQ(mtx_lock(&oc.client_ch->buf_mtx), thrd_success);
	uint32_t original_max = oc.client_ch->remote_max_packet;
	oc.client_ch->remote_max_packet = 0;
	ASSERT_EQ(mtx_unlock(&oc.client_ch->buf_mtx), thrd_success);
	ASSERT_EQ(dssh_chan_poll(oc.client_ch, DSSH_POLL_WRITE, 0), 0);
	ASSERT_EQ(dssh_chan_write(oc.client_ch, 0, &byte, 1), DSSH_ERROR_NOMORE);
	ASSERT_FALSE(tx_owner_is_current(ctx.client));
	ASSERT_EQ(mtx_lock(&oc.client_ch->buf_mtx), thrd_success);
	oc.client_ch->remote_max_packet = original_max;
	ASSERT_EQ(mtx_unlock(&oc.client_ch->buf_mtx), thrd_success);
	ASSERT_OK(dssh_chan_shutwr(oc.client_ch));
	ASSERT_EQ(dssh_chan_poll(oc.client_ch, DSSH_POLL_WRITE, 0), 0);
	ASSERT_EQ(dssh_chan_write(oc.client_ch, 0, &byte, 1), DSSH_ERROR_TERMINATED);
	ASSERT_FALSE(tx_owner_is_current(ctx.client));
	conn_cleanup(&ctx);
	return TEST_PASS;
}

static int
test_request_response_state(void)
{
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_FAIL;
	struct open_exec_ctx oc = {
		.client = ctx.client, .server = ctx.server,
		.command = "request-state", .cbs = &accept_cbs_all,
		.accept_timeout = 30000,
	};
	ASSERT_OK(open_exec_channel(&ctx, &oc));
	struct dssh_channel_s *ch = oc.client_ch;
	ASSERT_EQ(send_channel_request_wait(ctx.client, ch, "unknown-request", NULL, 0), DSSH_ERROR_REJECTED);
	ASSERT_FALSE(tx_owner_is_current(ctx.client));
	for (int success = 0; success < 2; success++) {
		ASSERT_EQ(mtx_lock(&ch->buf_mtx), thrd_success);
		ch->request_pending = true;
		ch->request_responded = true;
		ch->request_success = success != 0;
		ASSERT_EQ(mtx_unlock(&ch->buf_mtx), thrd_success);
		int result = wait_channel_request_response(ctx.client, ch);
		ASSERT_EQ(result, success ? 0 : DSSH_ERROR_REJECTED);
		ASSERT_FALSE(ch->request_pending);
		ASSERT_FALSE(ch->request_responded);
	}
	ASSERT_EQ(mtx_lock(&ch->buf_mtx), thrd_success);
	ch->request_pending = true;
	ch->request_responded = false;
	ASSERT_EQ(mtx_unlock(&ch->buf_mtx), thrd_success);
	ASSERT_EQ(send_channel_request_wait(ctx.client, ch, "unknown-request", NULL, 0), DSSH_ERROR_INVALID);
	ASSERT_TRUE(ch->request_pending);
	ASSERT_FALSE(tx_owner_is_current(ctx.client));
	ASSERT_EQ(mtx_lock(&ch->buf_mtx), thrd_success);
	ch->close_received = true;
	ASSERT_EQ(mtx_unlock(&ch->buf_mtx), thrd_success);
	ASSERT_EQ(wait_channel_request_response(ctx.client, ch), DSSH_ERROR_TERMINATED);
	ASSERT_FALSE(ch->request_pending);
	ASSERT_EQ(mtx_lock(&ch->buf_mtx), thrd_success);
	ch->close_received = false;
	ASSERT_EQ(mtx_unlock(&ch->buf_mtx), thrd_success);
	ASSERT_EQ(send_channel_request_wait(ctx.client, ch, "env", NULL, SIZE_MAX), DSSH_ERROR_INVALID);
	ASSERT_FALSE(tx_owner_is_current(ctx.client));
	ASSERT_FALSE(dssh_session_is_terminated(ctx.client));
	conn_cleanup(&ctx);
	return TEST_PASS;
}

static int
test_request_io_sync_failures(void)
{
	for (int operation = 0; operation < 8; operation++) {
		int count = operation == 7 ? 32 : (operation == 3 || operation == 5) ? 2 : 1;
		bool completed = operation != 7;
		for (int n = 0; n < count; n++) {
			struct conn_ctx ctx;
			if (conn_setup(&ctx) < 0)
				return TEST_FAIL;
			struct open_exec_ctx oc = {
				.client = ctx.client, .server = ctx.server,
				.command = "io-sync", .cbs = &accept_cbs_all,
				.accept_timeout = 30000,
			};
			ASSERT_OK(open_exec_channel(&ctx, &oc));
			struct dssh_channel_s *ch = oc.server_ch;
			if (operation == 5 || operation == 6) {
				ASSERT_EQ(mtx_lock(&ch->buf_mtx), thrd_success);
				ch->request_pending = true;
				ch->request_responded = operation == 6;
				ch->request_success = true;
				ASSERT_EQ(mtx_unlock(&ch->buf_mtx), thrd_success);
			}
			uint8_t byte = 0;
			uint8_t data[10];
			size_t pos = 0;
			data[pos++] = SSH_MSG_CHANNEL_DATA;
			DSSH_PUT_U32(ch->local_id, data, &pos);
			DSSH_PUT_U32(1, data, &pos);
			data[pos++] = 1;
			struct dssh_chan_event event;
			dssh_test_thrd_fail_after(n);
			int64_t ret;
			if (operation == 0)
				ret = dssh_chan_read(ch, 0, NULL, 0);
			else if (operation == 1)
				ret = dssh_chan_read(ch, 0, &byte, 1);
			else if (operation == 2 || operation == 3)
				ret = dssh_chan_poll(ch, DSSH_POLL_READ, operation == 2 ? 0 : -1);
			else if (operation == 4)
				ret = dssh_chan_read_event(ch, &event);
			else if (operation == 5 || operation == 6)
				ret = wait_channel_request_response(ctx.server, ch);
			else
				ret = dssh_test_demux_dispatch(ctx.server, data[0], data, pos);
			int calls = dssh_test_thrd_count();
			dssh_test_thrd_reset();
			bool terminated = dssh_session_is_terminated(ctx.server);
			conn_cleanup(&ctx);
			if (operation != 7 || calls > n) {
				ASSERT_EQ(ret, DSSH_ERROR_TERMINATED);
				ASSERT_TRUE(terminated);
			}
			else {
				ASSERT_OK(ret);
				ASSERT_FALSE(terminated);
				completed = true;
				break;
			}
		}
		ASSERT_TRUE(completed);
	}
	return TEST_PASS;
}

static int
test_request_tx_sync_sweep(void)
{
	/* Test the complete caller-thread sequence, including TX release.
	 * The demux threads opt out of the failure countdown. */
	for (int operation = 0; operation < 2; operation++) {
		int limit = 48;
		bool completed = false;
		for (int n = 0; n < limit; n++) {
			struct conn_ctx ctx;
			if (conn_setup(&ctx) < 0)
				return TEST_FAIL;
			struct open_exec_ctx oc = {
				.client = ctx.client, .server = ctx.server,
				.command = "tx-sync", .cbs = &accept_cbs_all,
				.accept_timeout = 30000,
			};
			ASSERT_OK(open_exec_channel(&ctx, &oc));
			uint8_t byte = 1;
			dssh_test_thrd_fail_after(n);
			int64_t ret = operation == 0 ? dssh_chan_write(oc.client_ch, 0, &byte, 1)
			    : send_channel_request_wait(ctx.client, oc.client_ch, "unknown-request", NULL, 0);
			int calls = dssh_test_thrd_count();
			dssh_test_thrd_reset();
			bool owner = tx_owner_is_current(ctx.client);
			bool terminated = dssh_session_is_terminated(ctx.client);
			if (owner)
				tx_unlock_with_slots(ctx.client);
			conn_cleanup(&ctx);
			ASSERT_FALSE(owner);
			if (calls > n) {
				ASSERT_TRUE(terminated);
				ASSERT_EQ(ret, DSSH_ERROR_TERMINATED);
			}
			else {
				ASSERT_FALSE(terminated);
				ASSERT_EQ(ret, operation == 0 ? 1 : DSSH_ERROR_REJECTED);
				completed = true;
				break;
			}
		}
		ASSERT_TRUE(completed);
	}
	return TEST_PASS;
}

static int
test_channel_registration_failures(void)
{
	/* Reuse a fully initialized channel to isolate registration ownership. */
	bool completed = false;
	for (int n = 0; n < 32; n++) {
		struct conn_ctx ctx;
		if (conn_setup(&ctx) < 0)
			return TEST_FAIL;
		struct open_exec_ctx oc = {
			.client = ctx.client, .server = ctx.server,
			.command = "registration-sync", .cbs = &accept_cbs_all,
			.accept_timeout = 30000,
		};
		ASSERT_OK(open_exec_channel(&ctx, &oc));
		struct dssh_channel_s *ch = oc.server_ch;
		ASSERT_OK(detach_channel(ctx.server, ch));
		atomic_store(&ch->closing, false);
		bool published = false;
		dssh_test_thrd_fail_after(n);
		int ret = register_channel(ctx.server, ch, &published);
		int calls = dssh_test_thrd_count();
		dssh_test_thrd_reset();
		bool owner = tx_owner_is_current(ctx.server);
		if (owner)
			tx_unlock_with_slots(ctx.server);
		ASSERT_EQ(mtx_lock(&ctx.server->channel_mtx), thrd_success);
		bool found = false;
		for (size_t i = 0; i < ctx.server->channel_count; i++) {
			if (ctx.server->channels[i] == ch)
				found = true;
		}
		ASSERT_EQ(mtx_unlock(&ctx.server->channel_mtx), thrd_success);
		if (!published)
			dssh_chan_close(ch, -1);
		bool terminated = dssh_session_is_terminated(ctx.server);
		conn_cleanup(&ctx);
		ASSERT_FALSE(owner);
		ASSERT_EQ(found, published);
		if (calls > n) {
			ASSERT_EQ(ret, DSSH_ERROR_TERMINATED);
			ASSERT_TRUE(terminated);
		}
		else {
			ASSERT_OK(ret);
			ASSERT_FALSE(terminated);
			ASSERT_TRUE(published);
			completed = true;
			break;
		}
	}
	ASSERT_TRUE(completed);
	return TEST_PASS;
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
	if (open_exec_channel(&ctx, &oc) < 0 || oc.client_ch == NULL || oc.server_ch == NULL) {
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
	if (open_exec_channel(&ctx, &oc) < 0 || oc.client_ch == NULL || oc.server_ch == NULL) {
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

	/* Client should observe EOF (poll surfaces READ-ready, read
	 * returns 0).  Polling here also guarantees the demux has
	 * processed both the EOF and the stale-data drop before we
	 * sample the bytebuf. */
	int ev = dssh_chan_poll(oc.client_ch, DSSH_POLL_READ, 5000);
	ASSERT_TRUE(ev & DSSH_POLL_READ);
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
	if (open_exec_channel(&ctx, &oc) < 0 || oc.client_ch == NULL || oc.server_ch == NULL) {
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
	if (open_exec_channel(&ctx, &oc) < 0 || oc.client_ch == NULL || oc.server_ch == NULL) {
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
	if (open_exec_channel(&ctx, &oc) < 0 || oc.client_ch == NULL || oc.server_ch == NULL) {
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
	if (open_exec_channel(&ctx, &oc) < 0 || oc.client_ch == NULL || oc.server_ch == NULL) {
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
	if (open_exec_channel(&ctx, &oc) < 0 || oc.client_ch == NULL || oc.server_ch == NULL) {
		conn_cleanup(&ctx);
		return TEST_FAIL;
	}

	{
		uint8_t msg[32];
		size_t pos = 0;
		msg[pos++] = SSH_MSG_CHANNEL_DATA;
		dssh_serialize_uint32(oc.client_ch->local_id, msg, sizeof(msg), &pos);
		dssh_serialize_uint32(UINT32_MAX, msg, sizeof(msg), &pos);
		memcpy(&msg[pos], "x", 1);
		pos += 1;
		ASSERT_OK(send_packet(ctx.server, msg, pos, NULL));
	}

	/* The extended-data parser has a different fixed header size and
	 * must reject the same overflow attempt independently. */
	{
		uint8_t msg[32];
		size_t pos = 0;
		msg[pos++] = SSH_MSG_CHANNEL_EXTENDED_DATA;
		dssh_serialize_uint32(oc.client_ch->local_id, msg, sizeof(msg), &pos);
		dssh_serialize_uint32(1, msg, sizeof(msg), &pos);
		dssh_serialize_uint32(UINT32_MAX, msg, sizeof(msg), &pos);
		msg[pos++] = 'x';
		ASSERT_OK(send_packet(ctx.server, msg, pos, NULL));
	}

	/* Malformed packets must not poison the channel. */
	uint8_t data[] = "still works";
	int64_t res = dssh_chan_write(oc.server_ch, 0, data, sizeof(data));
	ASSERT_TRUE(res > 0);

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
		return TEST_FAIL;

	struct open_exec_ctx oc = {
		.client = ctx.client,
		.server = ctx.server,
		.command = "echo poll_write",
		.cbs = &accept_cbs_all,
		.accept_timeout = 30000,
	};
	if (open_exec_channel(&ctx, &oc) < 0 || oc.client_ch == NULL || oc.server_ch == NULL) {
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
		return TEST_FAIL;

	struct open_exec_ctx oc = {
		.client = ctx.client,
		.server = ctx.server,
		.command = "echo eof_write",
		.cbs = &accept_cbs_all,
		.accept_timeout = 30000,
	};
	if (open_exec_channel(&ctx, &oc) < 0 || oc.client_ch == NULL || oc.server_ch == NULL) {
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
		return TEST_FAIL;

	struct open_exec_ctx oc = {
		.client = ctx.client,
		.server = ctx.server,
		.command = "echo ext_eof",
		.cbs = &accept_cbs_all,
		.accept_timeout = 30000,
	};
	if (open_exec_channel(&ctx, &oc) < 0 || oc.client_ch == NULL || oc.server_ch == NULL) {
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
	ASSERT_EQ(stream_body_run(STREAM_SUBSYS_AFTER_EOF), TEST_PASS);
	return TEST_PASS;
}

/* ================================================================
 * Coverage: subsystem poll edge cases
 * ================================================================ */

static int
test_subsys_poll_write(void)
{
	ASSERT_EQ(stream_body_run(STREAM_SUBSYS_POLL_WRITE), TEST_PASS);
	return TEST_PASS;
}

static int
test_subsys_poll_timeout_zero(void)
{
	ASSERT_EQ(stream_body_run(STREAM_SUBSYS_POLL_EMPTY), TEST_PASS);
	return TEST_PASS;
}

/* ================================================================
 * Coverage: signal read when empty, interleave tests
 * ================================================================ */

static int
test_read_event_empty(void)
{
	ASSERT_EQ(event_body_run(EVENT_BODY_EMPTY), TEST_PASS);
	return TEST_PASS;
}

static int
test_signal_interleave_read(void)
{
	ASSERT_EQ(event_body_run(EVENT_BODY_INTERLEAVE), TEST_PASS);
	return TEST_PASS;
}

static int
test_signal_interleave_read_ext(void)
{
	ASSERT_EQ(event_body_run(EVENT_BODY_INTERLEAVE_EXT), TEST_PASS);
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
	 * Test each truncation point individually. A received fatal reply may
	 * terminate the sender before its completed TX owner is released.
	 */
	/* Case 1: too short to read type_len -- disconnect only */
	{
		struct conn_ctx ctx;
		if (conn_setup(&ctx) < 0)
			return TEST_FAIL;

		uint8_t msg[2];
		msg[0] = SSH_MSG_CHANNEL_OPEN;
		msg[1] = 0;
		int ret = send_packet(ctx.client, msg, 2, NULL);
		ASSERT_TRUE(ret == 0 || ret == DSSH_ERROR_TERMINATED);

		ASSERT_TRUE(dssh_test_wait_bool(&ctx.server->demux_running, false));
		ASSERT_TRUE(dssh_session_is_terminated(ctx.server));
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
		ASSERT_OK(dssh_serialize_uint32(100, msg, sizeof(msg), &pos));
		int ret = send_packet(ctx.client, msg, pos, NULL);
		ASSERT_TRUE(ret == 0 || ret == DSSH_ERROR_TERMINATED);

		ASSERT_TRUE(dssh_test_wait_bool(&ctx.server->demux_running, false));
		ASSERT_TRUE(dssh_session_is_terminated(ctx.server));
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
		ASSERT_OK(dssh_serialize_uint32(4, msg, sizeof(msg), &pos));
		memcpy(&msg[pos], "test", 4);
		pos += 4;
		/* Include sender-channel so OPEN_FAILURE can be sent */
		ASSERT_OK(dssh_serialize_uint32(42, msg, sizeof(msg), &pos));
		int ret = send_packet(ctx.client, msg, pos, NULL);
		ASSERT_TRUE(ret == 0 || ret == DSSH_ERROR_TERMINATED);

		ASSERT_TRUE(dssh_test_wait_bool(&ctx.server->demux_running, false));
		ASSERT_TRUE(dssh_session_is_terminated(ctx.server));
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
	 * A malformed common header disconnects without a speculative channel
	 * response. Exact wire checks are in request_framing_cases.h.
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
	if (open_exec_channel(&ctx, &oc) < 0 || oc.client_ch == NULL || oc.server_ch == NULL) {
		conn_cleanup(&ctx);
		return TEST_FAIL;
	}

	{
		uint8_t msg[8];
		size_t pos = 0;
		msg[pos++] = SSH_MSG_CHANNEL_REQUEST;
		ASSERT_OK(dssh_serialize_uint32(oc.server_ch->local_id, msg, sizeof(msg), &pos));
		int ret = send_packet(ctx.client, msg, pos, NULL);
		/* The peer's fatal reply may be received before TX ownership is
		 * released, so the completed send can already report termination. */
		ASSERT_TRUE(ret == 0 || ret == DSSH_ERROR_TERMINATED);
	}

	ASSERT_TRUE(dssh_test_wait_bool(&ctx.server->demux_running, false));
	ASSERT_TRUE(dssh_session_is_terminated(ctx.server));

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
		return TEST_FAIL;

	struct open_exec_ctx oc = {
		.client = ctx.client,
		.server = ctx.server,
		.command = "echo poll_empty",
		.cbs = &accept_cbs_all,
		.accept_timeout = 30000,
	};
	if (open_exec_channel(&ctx, &oc) < 0 || oc.client_ch == NULL || oc.server_ch == NULL) {
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
		return TEST_FAIL;

	struct open_exec_ctx oc = {
		.client = ctx.client,
		.server = ctx.server,
		.command = "echo winzero",
		.cbs = &accept_cbs_all,
		.accept_timeout = 30000,
	};
	if (open_exec_channel(&ctx, &oc) < 0 || oc.client_ch == NULL || oc.server_ch == NULL) {
		conn_cleanup(&ctx);
		return TEST_FAIL;
	}

	oc.client_ch->remote_window = 0;
	int64_t w = dssh_chan_write(oc.client_ch, 0,
	    (const uint8_t *)"data", 4);
	ASSERT_EQ(w, DSSH_ERROR_NOMORE);

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
		return TEST_FAIL;

	struct open_exec_ctx oc = {
		.client = ctx.client,
		.server = ctx.server,
		.command = "echo extwinzero",
		.cbs = &accept_cbs_all,
		.accept_timeout = 30000,
	};
	if (open_exec_channel(&ctx, &oc) < 0 || oc.client_ch == NULL || oc.server_ch == NULL) {
		conn_cleanup(&ctx);
		return TEST_FAIL;
	}

	oc.server_ch->remote_window = 0;
	int64_t w = dssh_chan_write(oc.server_ch, 1,
	    (const uint8_t *)"data", 4);
	ASSERT_EQ(w, DSSH_ERROR_NOMORE);

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
	dssh_channel channel;
};

static int
accept_blocking_thread(void *arg)
{
	struct accept_blocking_ctx *bc = arg;
	bc->channel = dssh_chan_accept(bc->ctx->server, &accept_cbs_all, -1);
	return bc->channel == NULL && dssh_session_is_terminated(bc->ctx->server) ? 0 : -1;
}


static int
test_accept_timeout_negative(void)
{
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_FAIL;
	struct accept_blocking_ctx bc = {.ctx = &ctx};
	struct channel_workers workers;
	channel_workers_init(&workers, &ctx);
	workers.retire = conn_actor_unwatch;
	atomic_bool *entered = dssh_test_thrd_watch_condition(&ctx.server->accept_cnd);
	ASSERT_EQ(channel_worker_start(&workers, 0, accept_blocking_thread, &bc), 0);
	ASSERT_TRUE(conn_actor_checkpoint());
	ASSERT_TRUE(conn_actor_waiting(entered, &ctx.server->accept_mtx));
	ASSERT_FALSE(dssh_session_is_terminated(ctx.server));
	dssh_session_terminate(ctx.server);
	channel_workers_finish(&workers);
	ASSERT_EQ(workers.results[0], 0);
	ASSERT_NULL(bc.channel);
	conn_cleanup(&ctx);
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
		return TEST_FAIL;
	struct open_exec_ctx oc = {.client = ctx.client, .server = ctx.server,
	    .command = "rejected_exec", .cbs = &reject_exec_cbs, .accept_timeout = 30000};
	struct channel_workers workers;
	channel_workers_init(&workers, &ctx);
	ASSERT_EQ(channel_worker_start(&workers, 0, client_open_rejected_thread, &oc), 0);
	ASSERT_EQ(channel_worker_start(&workers, 1, server_accept_cancelled_thread, &oc), 0);
	ASSERT_TRUE(conn_actor_checkpoint());
	channel_worker_join(&workers, 0);
	ASSERT_EQ(workers.results[0], 0);
	ASSERT_NULL(oc.client_ch);
	ASSERT_EQ_U(atomic_load(&reject_exec_calls), 1);
	ASSERT_FALSE(dssh_session_is_terminated(ctx.client));
	ASSERT_FALSE(dssh_session_is_terminated(ctx.server));
	/* A rejecting accept resumes waiting for another channel. */
	dssh_session_terminate(ctx.server);
	channel_workers_finish(&workers);
	ASSERT_EQ(workers.results[1], 0);
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
	if (open_exec_channel(&ctx, &oc) < 0 || oc.client_ch == NULL || oc.server_ch == NULL) {
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
		return TEST_FAIL;

	struct open_exec_ctx oc = {
		.client = ctx.client,
		.server = ctx.server,
		.command = "echo double",
		.cbs = &accept_cbs_all,
		.accept_timeout = 30000,
	};
	if (open_exec_channel(&ctx, &oc) < 0 || oc.client_ch == NULL || oc.server_ch == NULL) {
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
	if (open_exec_channel(&ctx, &oc) < 0 || oc.client_ch == NULL || oc.server_ch == NULL) {
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

	/* Both packets had bogus dlen and were dropped by the demux,
	 * so no bytes are buffered and no EOF arrived.  The new read
	 * contract returns NOMORE for empty-no-EOF; old contract
	 * returned 0.  Accept either to validate "didn't tear down". */
	uint8_t buf[256];
	int64_t n = dssh_chan_read(oc.server_ch, 0, buf, sizeof(buf));
	ASSERT_TRUE(n >= 0 || n == DSSH_ERROR_NOMORE);

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
		return TEST_FAIL;

	struct open_exec_ctx oc = {
		.client = ctx.client,
		.server = ctx.server,
		.command = "echo chantype",
		.cbs = &accept_cbs_all,
		.accept_timeout = 30000,
	};
	if (open_exec_channel(&ctx, &oc) < 0 || oc.client_ch == NULL || oc.server_ch == NULL) {
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

	dssh_test_demux_dispatch(ctx.server, SSH_MSG_CHANNEL_DATA, payload, pos);

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
		return TEST_FAIL;

	struct open_exec_ctx oc = {
		.client = ctx.client, .server = ctx.server,
		.command = "echo eof2", .cbs = &accept_cbs_all,
		.accept_timeout = 30000,
	};
	if (open_exec_channel(&ctx, &oc) < 0 || !oc.client_ch || !oc.server_ch) {
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
		return TEST_FAIL;

	struct open_exec_ctx oc = {
		.client = ctx.client, .server = ctx.server,
		.command = "echo close2", .cbs = &accept_cbs_all,
		.accept_timeout = 30000,
	};
	if (open_exec_channel(&ctx, &oc) < 0 || !oc.client_ch || !oc.server_ch) {
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
		return TEST_FAIL;

	struct open_exec_ctx oc = {
		.client = ctx.client, .server = ctx.server,
		.command = "echo replenish", .cbs = &accept_cbs_all,
		.accept_timeout = 30000,
	};
	if (open_exec_channel(&ctx, &oc) < 0 || !oc.client_ch || !oc.server_ch) {
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
		return TEST_FAIL;

	struct open_exec_ctx oc = {
		.client = ctx.client, .server = ctx.server,
		.command = "echo replow", .cbs = &accept_cbs_all,
		.accept_timeout = 30000,
	};
	if (open_exec_channel(&ctx, &oc) < 0 || !oc.client_ch || !oc.server_ch) {
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

struct tx_hold_state {
	dssh_session sess;
	atomic_bool locked;
	atomic_bool release;
};

static int
hold_tx_until_released(void *arg)
{
	struct tx_hold_state *st = arg;
	if (tx_acquire(st->sess, SSH_MSG_CHANNEL_DATA) < 0)
		return -1;
	atomic_store(&st->locked, true);
	bool released = dssh_test_wait_bool(&st->release, true);
	tx_unlock_with_slots(st->sess);
	return released ? 0 : -1;
}


static void
conn_actor_release_tx(void *arg)
{
	struct tx_hold_state *st = arg;
	atomic_store(&st->release, true);
}

static int
test_replenish_queued_accounting(void)
{
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_FAIL;

	struct open_exec_ctx oc = {
		.client = ctx.client, .server = ctx.server,
		.command = "echo repqueue", .cbs = &accept_cbs_all,
		.accept_timeout = 30000,
	};
	ASSERT_EQ(open_exec_channel(&ctx, &oc), 0);

	struct tx_hold_state st = {.sess = ctx.server};
	atomic_init(&st.locked, false);
	atomic_init(&st.release, false);
	struct channel_workers workers;
	channel_workers_init(&workers, &ctx);
	workers.release = conn_actor_release_tx;
	workers.resource = &st;
	ASSERT_EQ(channel_worker_start(&workers, 0, hold_tx_until_released, &st), 0);
	ASSERT_TRUE(dssh_test_wait_bool(&st.locked, true));
	ASSERT_TRUE(conn_actor_checkpoint());

	ASSERT_EQ(mtx_lock(&oc.server_ch->buf_mtx), thrd_success);
	oc.server_ch->window_max = 32;
	oc.server_ch->local_window = 1;
	oc.server_ch->window_adjust_pending = 0;
	oc.server_ch->window_adjust_inflight = 0;
	if (mtx_unlock(&oc.server_ch->buf_mtx) != thrd_success)
		abort();

	ASSERT_EQ(maybe_replenish_window(ctx.server, oc.server_ch), 0);
	uint8_t fail[8];
	size_t fail_pos = 0;

	fail[fail_pos++] = SSH_MSG_CHANNEL_FAILURE;
	DSSH_PUT_U32(oc.server_ch->remote_id, fail, &fail_pos);
	ASSERT_EQ(send_to_slot(ctx.server, &oc.server_ch->chan_reply_slot, fail, fail_pos), 0);
	ASSERT_EQ(mtx_lock(&oc.server_ch->buf_mtx), thrd_success);
	uint32_t local = oc.server_ch->local_window;
	uint32_t pending = oc.server_ch->window_adjust_pending;
	uint32_t inflight = oc.server_ch->window_adjust_inflight;
	bool wa_ready = atomic_load(&oc.server_ch->wa_slot.ready);
	bool fail_ready = atomic_load(&oc.server_ch->chan_reply_slot.ready);
	if (mtx_unlock(&oc.server_ch->buf_mtx) != thrd_success)
		abort();
	ASSERT_EQ_U(local, 1);
	ASSERT_EQ_U(pending, 31);
	ASSERT_EQ_U(inflight, 0);
	ASSERT_TRUE(wa_ready);
	ASSERT_TRUE(fail_ready);

	/* Pending credit participates in the threshold calculation, so a
	 * second reader cannot queue a duplicate grant. */
	ASSERT_EQ(maybe_replenish_window(ctx.server, oc.server_ch), 0);
	ASSERT_EQ(mtx_lock(&oc.server_ch->buf_mtx), thrd_success);
	pending = oc.server_ch->window_adjust_pending;
	if (mtx_unlock(&oc.server_ch->buf_mtx) != thrd_success)
		abort();
	ASSERT_EQ_U(pending, 31);

	conn_actor_release_tx(&st);
	channel_workers_finish(&workers);
	ASSERT_EQ(workers.results[0], 0);

	ASSERT_EQ(mtx_lock(&oc.server_ch->buf_mtx), thrd_success);
	local = oc.server_ch->local_window;
	pending = oc.server_ch->window_adjust_pending;
	inflight = oc.server_ch->window_adjust_inflight;
	wa_ready = atomic_load(&oc.server_ch->wa_slot.ready);
	fail_ready = atomic_load(&oc.server_ch->chan_reply_slot.ready);
	if (mtx_unlock(&oc.server_ch->buf_mtx) != thrd_success)
		abort();
	ASSERT_EQ_U(local, 32);
	ASSERT_EQ_U(pending, 0);
	ASSERT_EQ_U(inflight, 0);
	ASSERT_FALSE(wa_ready);
	ASSERT_FALSE(fail_ready);

	ASSERT_EQ(dssh_chan_close(oc.server_ch, 0), 0);
	ASSERT_EQ(dssh_chan_close(oc.client_ch, 0), 0);
	conn_cleanup(&ctx);
	return TEST_PASS;
}

static uint32_t
zc_accounting_cb(dssh_channel ch, int stream, const uint8_t *data, size_t len, void *cbdata)
{
	(void)ch;
	(void)stream;
	(void)data;
	(void)len;
	return *(uint32_t *)cbdata;
}

static int
test_zc_consumption_clamped(void)
{
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_FAIL;

	struct open_exec_ctx oc = {
		.client = ctx.client, .server = ctx.server,
		.command = "echo zcclamp", .cbs = &accept_cbs_all,
		.accept_timeout = 30000,
	};
	if (open_exec_channel(&ctx, &oc) < 0 || !oc.client_ch || !oc.server_ch) {
		conn_cleanup(&ctx);
		return TEST_FAIL;
	}

	mtx_lock(&oc.server_ch->buf_mtx);
	uint32_t consumed = UINT32_MAX;

	oc.server_ch->io_model = DSSH_IO_ZC;
	oc.server_ch->zc_cb = zc_accounting_cb;
	oc.server_ch->zc_cbdata = &consumed;
	oc.server_ch->local_window = 4;
	oc.server_ch->window_max = 4;
	oc.server_ch->setup_mode = false;
	mtx_unlock(&oc.server_ch->buf_mtx);

	uint8_t payload[16];
	size_t pos = 0;
	payload[pos++] = SSH_MSG_CHANNEL_DATA;
	DSSH_PUT_U32(oc.server_ch->local_id, payload, &pos);
	DSSH_PUT_U32(4, payload, &pos);
	memcpy(&payload[pos], "four", 4);
	pos += 4;

	ASSERT_OK(dssh_test_demux_dispatch(ctx.server, SSH_MSG_CHANNEL_DATA, payload, pos));
	mtx_lock(&oc.server_ch->buf_mtx);
	uint32_t local = oc.server_ch->local_window;
	uint32_t pending = oc.server_ch->window_adjust_pending;
	uint32_t inflight = oc.server_ch->window_adjust_inflight;
	mtx_unlock(&oc.server_ch->buf_mtx);
	ASSERT_EQ_U(local, 4);
	ASSERT_EQ_U(pending, 0);
	ASSERT_EQ_U(inflight, 0);

	/* The full packet consumes peer credit even if the callback consumes
	 * only part of it; only the consumed bytes are granted back. */
	consumed = 2;
	ASSERT_OK(dssh_test_demux_dispatch(ctx.server, SSH_MSG_CHANNEL_DATA, payload, pos));
	mtx_lock(&oc.server_ch->buf_mtx);
	local = oc.server_ch->local_window;
	pending = oc.server_ch->window_adjust_pending;
	inflight = oc.server_ch->window_adjust_inflight;
	mtx_unlock(&oc.server_ch->buf_mtx);
	ASSERT_EQ_U(local, 2);
	ASSERT_EQ_U(pending, 0);
	ASSERT_EQ_U(inflight, 0);

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
	if (open_exec_channel(&ctx, &oc) < 0 || !oc.client_ch || !oc.server_ch) {
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
		return TEST_FAIL;

	struct open_exec_ctx oc = {
		.client = ctx.client, .server = ctx.server,
		.command = "echo woverflow", .cbs = &accept_cbs_all,
		.accept_timeout = 30000,
	};
	if (open_exec_channel(&ctx, &oc) < 0 || !oc.client_ch || !oc.server_ch) {
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
		return TEST_FAIL;

	struct open_exec_ctx oc = {
		.client = ctx.client,
		.server = ctx.server,
		.command = "echo nsec",
		.cbs = &accept_cbs_all,
		.accept_timeout = 30000,
	};
	if (open_exec_channel(&ctx, &oc) < 0 || oc.client_ch == NULL || oc.server_ch == NULL) {
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
	ASSERT_EQ(stream_body_run(STREAM_SUBSYS_ROUNDTRIP), TEST_PASS);
	return TEST_PASS;
}

/* ================================================================
 * Audit item 13: ZC accept — server accepts a zero-copy channel
 * ================================================================ */


static int
accept_subsys_zc(dssh_channel ch, const struct dssh_chan_params *p,
    struct dssh_chan_accept_result *result, void *cbdata)
{
	(void)ch;
	(void)p;
	result->zc_cb = conn_zc_receive;
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
	return ctx->server_ch == NULL ? DSSH_ERROR_INIT : 0;
}

static int
test_accept_zc(void)
{
	static const uint8_t input[] = "hello from client to zc";
	static const uint8_t reply[] = "zc reply from server";
	struct conn_ctx ctx;
	if (conn_setup(&ctx) != 0)
		return TEST_FAIL;
	struct conn_zc_fixture fixture;
	ASSERT_TRUE(conn_zc_init(&ctx, &fixture, sizeof(input), false));
	struct dssh_chan_accept_cbs cbs = {
		.subsystem = accept_subsys_zc, .cbdata = &fixture,
	};
	struct zc_accept_ctx server = {.server = ctx.server, .cbs = &cbs};
	struct open_subsys_ctx client = {
		.client = ctx.client, .server = ctx.server, .subsystem = "test-zc",
	};
	ASSERT_EQ(open_channel_pair(&ctx, client_open_subsys_thread, &client,
	    &client.client_ch, server_accept_zc_thread, &server, &server.server_ch), 0);
	ASSERT_TRUE(conn_zc_checkpoint(CONN_ZC_OPENED));
	ASSERT_EQ(server.server_ch->io_model, DSSH_IO_ZC);
	ASSERT_EQ(dssh_chan_get_type(server.server_ch), DSSH_CHAN_SUBSYSTEM);
	ASSERT_STR_EQ(dssh_chan_get_subsystem(server.server_ch), "test-zc");
	ASSERT_TRUE(conn_zc_write_all(client.client_ch, input, sizeof(input), 5));
	ASSERT_TRUE(conn_zc_wait_flag(&fixture, &fixture.complete, 1));
	ASSERT_TRUE(conn_zc_checkpoint(CONN_ZC_RECEIVED));
	ASSERT_EQ_U(fixture.used, sizeof(input));
	ASSERT_MEM_EQ(fixture.data, input, sizeof(input));

	uint8_t *buffer;
	size_t capacity;
	ASSERT_EQ(conn_zc_reserve(&fixture, server.server_ch, &buffer, &capacity), 0);
	ASSERT_NOT_NULL(buffer);
	ASSERT_TRUE(capacity >= sizeof(reply));
	memcpy(buffer, reply, sizeof(reply));
	ASSERT_EQ(conn_zc_send(&fixture, sizeof(reply)), 0);
	uint8_t received[sizeof(reply)];
	ASSERT_TRUE(conn_zc_read_exact(client.client_ch, received, sizeof(received), 1));
	ASSERT_MEM_EQ(received, reply, sizeof(reply));
	ASSERT_EQ(dssh_chan_close(client.client_ch, -1), 0);
	ASSERT_EQ(dssh_chan_close(server.server_ch, -1), 0);
	conn_cleanup(&ctx);
	ASSERT_TRUE(fixture.retired);
	ASSERT_FALSE(atomic_load(&fixture.failed));
	return TEST_PASS;
}

/* A channel close on another thread must wait while demux has dropped
 * buf_mtx to call a ZC consumer. */

static int
accept_subsys_blocking_zc(dssh_channel ch, const struct dssh_chan_params *p,
	struct dssh_chan_accept_result *result, void *cbdata)
{
	(void)ch;
	(void)p;
	result->zc_cb = conn_zc_receive;
	result->zc_cbdata = cbdata;
	return 0;
}

static int
test_close_during_zc_callback(void)
{
	static const uint8_t input[] = "hold callback";
	struct conn_ctx ctx;
	if (conn_setup(&ctx) != 0)
		return TEST_FAIL;
	struct conn_zc_fixture fixture;
	ASSERT_TRUE(conn_zc_init(&ctx, &fixture, sizeof(input), true));
	struct dssh_chan_accept_cbs cbs = {
		.subsystem = accept_subsys_blocking_zc, .cbdata = &fixture,
	};
	struct zc_accept_ctx server = {.server = ctx.server, .cbs = &cbs};
	struct open_subsys_ctx client = {
		.client = ctx.client, .server = ctx.server, .subsystem = "close-during-zc",
	};
	ASSERT_EQ(open_channel_pair(&ctx, client_open_subsys_thread, &client,
	    &client.client_ch, server_accept_zc_thread, &server, &server.server_ch), 0);
	ASSERT_TRUE(conn_zc_checkpoint(CONN_ZC_OPENED));
	ASSERT_TRUE(conn_zc_write_all(client.client_ch, input, sizeof(input), 5));
	ASSERT_TRUE(conn_zc_wait_flag(&fixture, &fixture.entered, 5));
	ASSERT_TRUE(conn_zc_checkpoint(CONN_ZC_BLOCKED));
	ASSERT_EQ(conn_zc_start_close(&ctx, &server.server_ch), 0);
	ASSERT_TRUE(conn_zc_wait_flag(&fixture, &fixture.close_waiting, 5));
	ASSERT_FALSE(atomic_load(&fixture.close_done));
	ASSERT_TRUE(conn_zc_checkpoint(CONN_ZC_CLOSING));
	atomic_store_explicit(&fixture.release, true, memory_order_release);
	conn_zc_join(&ctx);
	ASSERT_TRUE(atomic_load(&fixture.close_done));
	ASSERT_EQ(fixture.close_result, 0);
	ASSERT_TRUE(atomic_load_explicit(&fixture.complete, memory_order_acquire));
	ASSERT_EQ_U(fixture.used, sizeof(input));
	ASSERT_MEM_EQ(fixture.data, input, sizeof(input));
	ASSERT_FALSE(dssh_session_is_terminated(ctx.server));
	ASSERT_EQ(dssh_chan_close(client.client_ch, -1), 0);
	conn_cleanup(&ctx);
	ASSERT_TRUE(fixture.retired);
	ASSERT_FALSE(atomic_load(&fixture.failed));
	return TEST_PASS;
}

static int
test_dispatch_sync_failures(void)
{
	for (int n = 0; n < 3; n++) {
		struct conn_ctx ctx;
		if (conn_setup(&ctx) < 0)
			return TEST_FAIL;

		struct open_exec_ctx oc = {
			.client = ctx.client,
			.server = ctx.server,
			.command = "dispatch-sync-failure",
			.cbs = &accept_cbs_all,
			.accept_timeout = 30000,
		};
		if (open_exec_channel(&ctx, &oc) < 0 || oc.client_ch == NULL || oc.server_ch == NULL) {
			conn_cleanup(&ctx);
			return TEST_FAIL;
		}

		uint8_t payload[9];
		size_t pos = 0;

		payload[pos++] = SSH_MSG_CHANNEL_WINDOW_ADJUST;
		DSSH_PUT_U32(oc.server_ch->local_id, payload, &pos);
		DSSH_PUT_U32(1, payload, &pos);
		dssh_test_thrd_fail_after(n);
		int ret = dssh_test_demux_dispatch(ctx.server,
		    SSH_MSG_CHANNEL_WINDOW_ADJUST, payload, pos);

		dssh_test_thrd_reset();
		ASSERT_ERR(ret, DSSH_ERROR_TERMINATED);
		ASSERT_TRUE(dssh_session_is_terminated(ctx.server));
		/* conn_cleanup() joins the demux before freeing registered channels. */
		conn_cleanup(&ctx);
	}
	return TEST_PASS;
}

static int
test_detach_sync_failures(void)
{
	for (int n = 0; n < 2; n++) {
		struct conn_ctx ctx;
		if (conn_setup(&ctx) < 0)
			return TEST_FAIL;

		struct open_exec_ctx oc = {
			.client = ctx.client,
			.server = ctx.server,
			.command = "detach-sync-failure",
			.cbs = &accept_cbs_all,
			.accept_timeout = 30000,
		};
		if (open_exec_channel(&ctx, &oc) < 0 || oc.client_ch == NULL || oc.server_ch == NULL) {
			conn_cleanup(&ctx);
			return TEST_FAIL;
		}

		if (n == 0)
			dssh_test_thrd_fail_after(0);
		int ret = detach_channel(ctx.server, oc.server_ch);

		dssh_test_thrd_reset();
		if (n == 0)
			ASSERT_ERR(ret, DSSH_ERROR_TERMINATED);
		else
			ASSERT_OK(ret);
		ASSERT_EQ(dssh_session_is_terminated(ctx.server), n == 0);
		/* A successful detach made this channel unreachable by demux, so
		 * close the detached handle before session cleanup.  A failed detach
		 * leaves it registered for session_stop() to free after joining demux. */
		if (n == 1)
			ASSERT_OK(dssh_chan_close(oc.server_ch, -1));
		conn_cleanup(&ctx);
	}
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
		return TEST_FAIL;

	struct open_exec_ctx oc = {
		.client = ctx.client, .server = ctx.server,
		.command = "echo trunc", .cbs = &accept_cbs_all,
		.accept_timeout = 30000,
	};
	if (open_exec_channel(&ctx, &oc) < 0 || !oc.client_ch || !oc.server_ch) {
		conn_cleanup(&ctx);
		return TEST_FAIL;
	}

	bytebuf_free(&oc.server_ch->buf.stdout_buf);
	ASSERT_OK(bytebuf_init(&oc.server_ch->buf.stdout_buf, 16));
	oc.server_ch->window_max = 16;
	oc.server_ch->local_window = 16;

	uint8_t fill[12];
	memset(fill, 'A', sizeof(fill));
	bytebuf_write(&oc.server_ch->buf.stdout_buf, fill, 12);
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

	dssh_test_demux_dispatch(ctx.server, SSH_MSG_CHANNEL_DATA, payload, pos);

	/* The peer consumed ten bytes of window even though only four fit
	 * in the stream buffer; dropped bytes are not granted back. */
	ASSERT_EQ(oc.server_ch->local_window, (uint32_t)6);

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
		return TEST_FAIL;

	struct open_exec_ctx oc = {
		.client = ctx.client, .server = ctx.server,
		.command = "echo repcap", .cbs = &accept_cbs_all,
		.accept_timeout = 30000,
	};
	if (open_exec_channel(&ctx, &oc) < 0 || !oc.client_ch || !oc.server_ch) {
		conn_cleanup(&ctx);
		return TEST_FAIL;
	}

	bytebuf_free(&oc.server_ch->buf.stdout_buf);
	ASSERT_OK(bytebuf_init(&oc.server_ch->buf.stdout_buf, 32));
	bytebuf_free(&oc.server_ch->buf.stderr_buf);
	ASSERT_OK(bytebuf_init(&oc.server_ch->buf.stderr_buf, 32));
	oc.server_ch->window_max = 32;

	uint8_t fill[28];
	memset(fill, 'X', sizeof(fill));
	bytebuf_write(&oc.server_ch->buf.stdout_buf, fill, 28);

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
 * Universal rx-window truncation + DSSH_PARAM_ACCEPT_EARLY_DATA bypass
 * ================================================================ */

/*
 * Helper: open an exec channel and set up the server-side channel so
 * we can poke a synthetic CHANNEL_DATA at it via demux_dispatch.  The
 * setup_complete and accept_pre_window_data fields are set per-test
 * by the caller after this returns.
 */
static int
rx_window_setup(struct conn_ctx *ctx, struct open_exec_ctx *oc, uint32_t bufsz)
{
	oc->client = ctx->client;
	oc->server = ctx->server;
	oc->command = "rx-window";
	oc->cbs = &accept_cbs_all;
	oc->accept_timeout = 30000;
	if (open_exec_channel(ctx, oc) < 0 || !oc->client_ch || !oc->server_ch)
		return -1;

	bytebuf_free(&oc->server_ch->buf.stdout_buf);
	if (bytebuf_init(&oc->server_ch->buf.stdout_buf, bufsz) < 0)
		return -1;
	oc->server_ch->window_max = bufsz;
	mtx_lock(&oc->server_ch->buf_mtx);
	oc->server_ch->setup_mode = false;
	mtx_unlock(&oc->server_ch->buf_mtx);
	return 0;
}

/* Build a CHANNEL_DATA payload with N bytes of 'B' for the given channel. */
static size_t
build_channel_data(uint8_t *buf, size_t bufsz, uint32_t local_id, uint32_t dlen)
{
	size_t pos = 0;
	buf[pos++] = SSH_MSG_CHANNEL_DATA;
	dssh_serialize_uint32(local_id, buf, bufsz, &pos);
	dssh_serialize_uint32(dlen, buf, bufsz, &pos);
	memset(&buf[pos], 'B', dlen);
	pos += dlen;
	return pos;
}

/* Unused receive credit still needs buffer space even before it arrives.
 * Replenishing after a small read must not let the peer overrun either stream. */
static int
test_replenish_reserves_outstanding_space(void)
{
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_FAIL;

	struct open_exec_ctx oc = { 0 };
	if (rx_window_setup(&ctx, &oc, 32) < 0) {
		conn_cleanup(&ctx);
		return TEST_FAIL;
	}
	bytebuf_free(&oc.server_ch->buf.stderr_buf);
	ASSERT_OK(bytebuf_init(&oc.server_ch->buf.stderr_buf, 32));

	for (int stream = 0; stream < 2; stream++) {
		uint8_t fill[24];
		uint8_t result[32];
		uint8_t payload[32];
		size_t pos = 0;
		struct dssh_bytebuf *bb = stream == 0
		    ? &oc.server_ch->buf.stdout_buf : &oc.server_ch->buf.stderr_buf;

		memset(fill, 'A', sizeof(fill));
		mtx_lock(&oc.server_ch->buf_mtx);
		size_t filled = bytebuf_write(bb, fill, sizeof(fill));
		oc.server_ch->local_window = 8;
		mtx_unlock(&oc.server_ch->buf_mtx);
		ASSERT_EQ_U(filled, sizeof(fill));

		/* All eight free bytes are already promised to the peer. */
		ASSERT_OK(maybe_replenish_window(ctx.server, oc.server_ch));
		mtx_lock(&oc.server_ch->buf_mtx);
		uint32_t credit = oc.server_ch->local_window;
		mtx_unlock(&oc.server_ch->buf_mtx);
		ASSERT_EQ_U(credit, 8);
		ASSERT_EQ(dssh_chan_read(oc.server_ch, stream, result, 1), 1);
		mtx_lock(&oc.server_ch->buf_mtx);
		credit = oc.server_ch->local_window;
		mtx_unlock(&oc.server_ch->buf_mtx);
		ASSERT_EQ_U(credit, 9);

		payload[pos++] = stream == 0 ? SSH_MSG_CHANNEL_DATA
		    : SSH_MSG_CHANNEL_EXTENDED_DATA;
		DSSH_PUT_U32(oc.server_ch->local_id, payload, &pos);
		if (stream != 0) {
			DSSH_PUT_U32(1, payload, &pos);
		}
		DSSH_PUT_U32(9, payload, &pos);
		memset(&payload[pos], 'B', 9);
		pos += 9;
		ASSERT_OK(dssh_test_demux_dispatch(ctx.server, payload[0], payload, pos));
		ASSERT_EQ(dssh_chan_read(oc.server_ch, stream, result, sizeof(result)), 32);
		for (size_t i = 0; i < sizeof(result); i++) {
			ASSERT_EQ(result[i], i < 23 ? 'A' : 'B');
		}
	}

	dssh_chan_close(oc.server_ch, 0);
	dssh_chan_close(oc.client_ch, 0);
	conn_cleanup(&ctx);
	return TEST_PASS;
}

/* Without the flag and with a closed window, all early bytes are
 * clipped to len=0 -- the bytebuf stays empty and local_window stays 0. */
static int
test_rx_truncation_default(void)
{
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_FAIL;

	struct open_exec_ctx oc = { 0 };
	if (rx_window_setup(&ctx, &oc, 64) < 0) {
		conn_cleanup(&ctx);
		return TEST_FAIL;
	}

	/* No flag, no setup_complete, but local_window=0.  In the existing
	 * universal path (bypass==false), dlen gets clipped to 0. */
	oc.server_ch->accept_pre_window_data = false;
	atomic_store(&oc.server_ch->setup_complete, false);
	oc.server_ch->local_window = 0;

	uint8_t payload[64];
	size_t pos = build_channel_data(payload, sizeof(payload),
	    oc.server_ch->local_id, 10);

	dssh_test_demux_dispatch(ctx.server, SSH_MSG_CHANNEL_DATA, payload, pos);

	ASSERT_EQ(bytebuf_available(&oc.server_ch->buf.stdout_buf), (size_t)0);
	ASSERT_EQ(oc.server_ch->local_window, (uint32_t)0);

	dssh_chan_close(oc.server_ch, 0);
	dssh_chan_close(oc.client_ch, 0);
	conn_cleanup(&ctx);
	return TEST_PASS;
}

/* Universal rx-truncation: dlen exceeds local_window, bytes are clipped
 * to local_window before stream_zc_cb runs. */
static int
test_rx_truncation_clips_to_window(void)
{
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_FAIL;

	struct open_exec_ctx oc = { 0 };
	if (rx_window_setup(&ctx, &oc, 64) < 0) {
		conn_cleanup(&ctx);
		return TEST_FAIL;
	}

	oc.server_ch->accept_pre_window_data = false;
	atomic_store(&oc.server_ch->setup_complete, true);
	oc.server_ch->local_window = 4;

	uint8_t payload[64];
	size_t pos = build_channel_data(payload, sizeof(payload),
	    oc.server_ch->local_id, 10);

	dssh_test_demux_dispatch(ctx.server, SSH_MSG_CHANNEL_DATA, payload, pos);

	/* cb saw len=4, wrote 4 bytes; local_window depleted. */
	ASSERT_EQ(bytebuf_available(&oc.server_ch->buf.stdout_buf), (size_t)4);
	ASSERT_EQ(oc.server_ch->local_window, (uint32_t)0);

	dssh_chan_close(oc.server_ch, 0);
	dssh_chan_close(oc.client_ch, 0);
	conn_cleanup(&ctx);
	return TEST_PASS;
}

/* With the flag and !setup_complete, full dlen flows through despite
 * a closed window -- and local_window is NOT decremented (no credit). */
static int
test_rx_bypass_pre_setup(void)
{
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_FAIL;

	struct open_exec_ctx oc = { 0 };
	if (rx_window_setup(&ctx, &oc, 64) < 0) {
		conn_cleanup(&ctx);
		return TEST_FAIL;
	}

	oc.server_ch->accept_pre_window_data = true;
	atomic_store(&oc.server_ch->setup_complete, false);
	oc.server_ch->local_window = 0;

	uint8_t payload[64];
	size_t pos = build_channel_data(payload, sizeof(payload),
	    oc.server_ch->local_id, 10);

	dssh_test_demux_dispatch(ctx.server, SSH_MSG_CHANNEL_DATA, payload, pos);

	ASSERT_EQ(bytebuf_available(&oc.server_ch->buf.stdout_buf), (size_t)10);
	/* Window was not credited for these out-of-budget bytes. */
	ASSERT_EQ(oc.server_ch->local_window, (uint32_t)0);

	dssh_chan_close(oc.server_ch, 0);
	dssh_chan_close(oc.client_ch, 0);
	conn_cleanup(&ctx);
	return TEST_PASS;
}

/* Once setup_complete is set, bypass disengages even with the flag --
 * truncation kicks back in and a closed window clips everything. */
static int
test_rx_bypass_disengages_post_setup(void)
{
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_FAIL;

	struct open_exec_ctx oc = { 0 };
	if (rx_window_setup(&ctx, &oc, 64) < 0) {
		conn_cleanup(&ctx);
		return TEST_FAIL;
	}

	oc.server_ch->accept_pre_window_data = true;
	atomic_store(&oc.server_ch->setup_complete, true);
	oc.server_ch->local_window = 0;

	uint8_t payload[64];
	size_t pos = build_channel_data(payload, sizeof(payload),
	    oc.server_ch->local_id, 10);

	dssh_test_demux_dispatch(ctx.server, SSH_MSG_CHANNEL_DATA, payload, pos);

	ASSERT_EQ(bytebuf_available(&oc.server_ch->buf.stdout_buf), (size_t)0);
	ASSERT_EQ(oc.server_ch->local_window, (uint32_t)0);

	dssh_chan_close(oc.server_ch, 0);
	dssh_chan_close(oc.client_ch, 0);
	conn_cleanup(&ctx);
	return TEST_PASS;
}

/* Type-lock: dssh_chan_open with the flag set on a SUBSYSTEM channel
 * must return NULL without any wire activity. */
static int
test_chan_open_type_lock_subsystem(void)
{
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_FAIL;

	struct dssh_chan_params p;
	ASSERT_OK(dssh_chan_params_init(&p, DSSH_CHAN_SUBSYSTEM));
	ASSERT_OK(dssh_chan_params_set_subsystem(&p, "sftp"));
	ASSERT_OK(dssh_chan_params_set_accept_early_data(&p, true));

	dssh_channel ch = dssh_chan_open(ctx.client, &p);
	ASSERT_NULL(ch);

	dssh_chan_params_free(&p);
	conn_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * dssh_chan_poll — POSIX POLLHUP-style wake-up on session terminate
 * ================================================================ */

/* Terminate before poll: poll returns the requested data flags
 * immediately, even with no data and a long timeout. */
static int
test_poll_terminate_surfaces_data_flags(void)
{
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_FAIL;

	struct open_exec_ctx oc = {
		.client = ctx.client, .server = ctx.server,
		.command = "echo poll-terminate",
		.cbs = &accept_cbs_all, .accept_timeout = 30000,
	};
	if (open_exec_channel(&ctx, &oc) < 0 || !oc.client_ch || !oc.server_ch) {
		conn_cleanup(&ctx);
		return TEST_FAIL;
	}

	/* Drain any data the echo command produced, so READ would
	 * otherwise wait. */
	uint8_t scratch[64];
	while (dssh_chan_read(oc.client_ch, 0, scratch, sizeof(scratch)) > 0)
		;

	dssh_session_terminate(ctx.client);

	/* timeout=5000 ms; should return immediately. */
	int ev = dssh_chan_poll(oc.client_ch,
	    DSSH_POLL_READ | DSSH_POLL_READEXT | DSSH_POLL_WRITE, 5000);
	ASSERT_TRUE(ev & DSSH_POLL_READ);
	ASSERT_TRUE(ev & DSSH_POLL_READEXT);
	ASSERT_TRUE(ev & DSSH_POLL_WRITE);

	/* chan_read after terminate-induced wake returns 0 (EOF) once
	 * the bytebuf is drained. */
	int64_t r = dssh_chan_read(oc.client_ch, 0, scratch, sizeof(scratch));
	ASSERT_EQ(r, (int64_t)0);

	conn_cleanup(&ctx);
	return TEST_PASS;
}

/* DSSH_POLL_EVENT alone is NOT surfaced by terminate (no real event
 * is queued; caller should rely on data flags or the terminate cb). */
static int
test_poll_terminate_no_event_bit(void)
{
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_FAIL;

	struct open_exec_ctx oc = {
		.client = ctx.client, .server = ctx.server,
		.command = "echo poll-evt",
		.cbs = &accept_cbs_all, .accept_timeout = 30000,
	};
	if (open_exec_channel(&ctx, &oc) < 0 || !oc.client_ch || !oc.server_ch) {
		conn_cleanup(&ctx);
		return TEST_FAIL;
	}

	/* Drain any pending events queued by the echo. */
	struct dssh_chan_event tmp;
	while (dssh_chan_read_event(oc.client_ch, &tmp) == 0)
		;

	dssh_session_terminate(ctx.client);

	/* timeout=0 (peek): EVENT alone returns 0 because no real event
	 * is queued.  Using DSSH_POLL_EVENT plus DSSH_POLL_READ should
	 * surface only READ, not EVENT. */
	int ev = dssh_chan_poll(oc.client_ch,
	    DSSH_POLL_EVENT | DSSH_POLL_READ, 0);
	ASSERT_TRUE(ev & DSSH_POLL_READ);
	ASSERT_FALSE(ev & DSSH_POLL_EVENT);

	conn_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Data/ext after CLOSE
 * ================================================================ */

/* The marker follows the injected data in the same packet stream.  Observing
 * it proves the demux has dispatched the data, not merely the preceding CLOSE. */
static int
wait_after_close_packets(struct open_exec_ctx *oc)
{
	if (dssh_chan_send_signal(oc->server_ch, "after-stale") < 0)
		return -1;
	for (;;) {
		int events = dssh_chan_poll(oc->client_ch, DSSH_POLL_EVENT, 5000);
		struct dssh_chan_event event;

		if ((events & DSSH_POLL_EVENT) == 0
		    || dssh_chan_read_event(oc->client_ch, &event) < 0)
			return -1;
		if (event.type == DSSH_EVENT_SIGNAL
		    && dssh_test_bytes_equal(event.signal.name, "after-stale", 11))
			return 0;
	}
}

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
	if (open_exec_channel(&ctx, &oc) < 0 || oc.client_ch == NULL || oc.server_ch == NULL) {
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

	ASSERT_OK(wait_after_close_packets(&oc));
	int events = dssh_chan_poll(oc.client_ch, DSSH_POLL_READ, 5000);
	ASSERT_TRUE(events & DSSH_POLL_READ);

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
	if (open_exec_channel(&ctx, &oc) < 0 || oc.client_ch == NULL || oc.server_ch == NULL) {
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

	ASSERT_OK(wait_after_close_packets(&oc));
	int events = dssh_chan_poll(oc.client_ch, DSSH_POLL_READEXT, 5000);
	ASSERT_TRUE(events & DSSH_POLL_READEXT);

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
	bytebuf_init(&ch.buf.stdout_buf, 256);
	bytebuf_init(&ch.buf.stderr_buf, 256);
	ch.close_received = true;
	ch.window_max = 256;
	ch.local_window = 0;

	int r = maybe_replenish_window(s, &ch);
	ASSERT_EQ(r, 0);
	ASSERT_EQ_U(ch.local_window, 0);

	bytebuf_free(&ch.buf.stdout_buf);
	bytebuf_free(&ch.buf.stderr_buf);
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
	if (open_exec_channel(&ctx, &oc) < 0 || oc.client_ch == NULL || oc.server_ch == NULL) {
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
	return ctx->server_ch == NULL ? DSSH_ERROR_INIT : 0;
}

static int
test_open_exec_alloc_sweep(void)
{
	bool fault_seen = false;
	for (int n = 0; n < 50; n++) {
		struct conn_ctx ctx;
		dssh_test_alloc_exclude_new_threads();
		if (conn_setup(&ctx) != 0)
			return TEST_FAIL;
		struct open_exec_ctx oc = {
			.client = ctx.client,
			.server = ctx.server,
			.client_ch = NULL,
			.server_ch = NULL,
			.command = "echo alloc",
			.cbs = &accept_cbs_all,
		};
		struct channel_workers workers;
		channel_workers_init(&workers, &ctx);
		dssh_test_alloc_fail_after(n);
		int ret = channel_worker_start(&workers, 0, client_open_exec_thread, &oc);
		if (ret == 0)
			ret = channel_worker_start(&workers, 1, server_accept_alloc_thread, &oc);
		if (ret != 0) {
			channel_workers_finish(&workers);
			dssh_test_alloc_reset();
			conn_cleanup(&ctx);
			return TEST_FAIL;
		}
		channel_worker_join(&workers, 0);
		int count = dssh_test_alloc_count();
		dssh_test_alloc_reset();
		bool injected = count > n;
		fault_seen = fault_seen || injected;
		if (oc.client_ch == NULL)
			channel_workers_cancel(&workers);
		channel_workers_finish(&workers);
		bool complete = oc.client_ch != NULL && oc.server_ch != NULL
		    && workers.results[0] == 0 && workers.results[1] == 0;
		/* Session cleanup owns every remaining registered channel. */
		conn_cleanup(&ctx);
		if (!injected) {
			ASSERT_TRUE(complete && fault_seen);
			return TEST_PASS;
		}
	}
	return TEST_FAIL;
}

static int
client_open_shell_alloc_thread(void *arg)
{
	struct open_shell_ctx *ctx = arg;
	struct dssh_chan_params p;
	int ret = dssh_chan_params_init(&p, DSSH_CHAN_SHELL);
	if (ret != 0)
		return ret;
	if (ret == 0)
		ret = dssh_chan_params_set_term(&p, ctx->term);
	if (ret == 0)
		ret = dssh_chan_params_set_size(&p, ctx->cols, ctx->rows, ctx->wpx, ctx->hpx);
	if (ret == 0)
		ctx->client_ch = dssh_chan_open(ctx->client, &p);
	dssh_chan_params_free(&p);
	return ret;
}

static int
server_accept_shell_alloc_thread(void *arg)
{
	struct open_shell_ctx *ctx = arg;

	dssh_test_alloc_exclude_thread();

	ctx->server_ch = dssh_chan_accept(ctx->server, ctx->cbs, 1000);
	return ctx->server_ch == NULL ? DSSH_ERROR_INIT : 0;
}

static int
test_open_shell_alloc_sweep(void)
{
	bool fault_seen = false;
	for (int n = 0; n < 50; n++) {
		struct conn_ctx ctx;
		dssh_test_alloc_exclude_new_threads();
		if (conn_setup(&ctx) != 0)
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
		struct channel_workers workers;
		channel_workers_init(&workers, &ctx);
		dssh_test_alloc_fail_after(n);
		int ret = channel_worker_start(&workers, 0, client_open_shell_alloc_thread, &oc);
		if (ret == 0)
			ret = channel_worker_start(&workers, 1, server_accept_shell_alloc_thread, &oc);
		if (ret != 0) {
			channel_workers_finish(&workers);
			dssh_test_alloc_reset();
			conn_cleanup(&ctx);
			return TEST_FAIL;
		}
		channel_worker_join(&workers, 0);
		int count = dssh_test_alloc_count();
		dssh_test_alloc_reset();
		bool injected = count > n;
		fault_seen = fault_seen || injected;
		if (oc.client_ch == NULL)
			channel_workers_cancel(&workers);
		channel_workers_finish(&workers);
		bool complete = oc.client_ch != NULL && oc.server_ch != NULL
		    && workers.results[0] == 0 && workers.results[1] == 0;
		/* Session cleanup owns every remaining registered channel. */
		conn_cleanup(&ctx);
		if (!injected) {
			ASSERT_TRUE(complete && fault_seen);
			return TEST_PASS;
		}
	}
	return TEST_FAIL;
}

/* ================================================================
 * Subsystem alloc failure sweep
 * ================================================================ */

static int
client_open_subsys_alloc_thread(void *arg)
{
	struct open_subsys_ctx *ctx = arg;
	struct dssh_chan_params p;
	int ret = dssh_chan_params_init(&p, DSSH_CHAN_SUBSYSTEM);
	if (ret != 0)
		return ret;
	if (ret == 0)
		ret = dssh_chan_params_set_subsystem(&p, ctx->subsystem);
	if (ret == 0)
		ret = dssh_chan_params_set_pty(&p, false);
	if (ret == 0)
		ctx->client_ch = dssh_chan_open(ctx->client, &p);
	dssh_chan_params_free(&p);
	return ret;
}

static int
server_accept_subsys_alloc_thread(void *arg)
{
	struct open_subsys_ctx *ctx = arg;

	dssh_test_alloc_exclude_thread();

	ctx->server_ch = dssh_chan_accept(ctx->server, &accept_cbs_all, 1000);
	return ctx->server_ch == NULL ? DSSH_ERROR_INIT : 0;
}

static int
test_open_subsystem_alloc_sweep(void)
{
	bool fault_seen = false;
	for (int n = 0; n < 50; n++) {
		struct conn_ctx ctx;
		dssh_test_alloc_exclude_new_threads();
		if (conn_setup(&ctx) != 0)
			return TEST_FAIL;
		struct open_subsys_ctx oc = {
			.client = ctx.client,
			.server = ctx.server,
			.client_ch = NULL,
			.server_ch = NULL,
			.subsystem = "sftp",
		};
		struct channel_workers workers;
		channel_workers_init(&workers, &ctx);
		dssh_test_alloc_fail_after(n);
		int ret = channel_worker_start(&workers, 0, client_open_subsys_alloc_thread, &oc);
		if (ret == 0)
			ret = channel_worker_start(&workers, 1, server_accept_subsys_alloc_thread, &oc);
		if (ret != 0) {
			channel_workers_finish(&workers);
			dssh_test_alloc_reset();
			conn_cleanup(&ctx);
			return TEST_FAIL;
		}
		channel_worker_join(&workers, 0);
		int count = dssh_test_alloc_count();
		dssh_test_alloc_reset();
		bool injected = count > n;
		fault_seen = fault_seen || injected;
		if (oc.client_ch == NULL)
			channel_workers_cancel(&workers);
		channel_workers_finish(&workers);
		bool complete = oc.client_ch != NULL && oc.server_ch != NULL
		    && workers.results[0] == 0 && workers.results[1] == 0;
		/* Session cleanup owns every remaining registered channel. */
		conn_cleanup(&ctx);
		if (!injected) {
			ASSERT_TRUE(complete && fault_seen);
			return TEST_PASS;
		}
	}
	return TEST_FAIL;
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
	int ret = dssh_chan_params_init(&p, DSSH_CHAN_EXEC);
	if (ret != 0)
		return ret;
	ret = dssh_chan_params_set_command(&p, ctx->command);
	if (ret == 0)
		ret = dssh_chan_params_set_pty(&p, false);
	if (ret == 0)
		ctx->client_ch = dssh_chan_open(ctx->client, &p);
	dssh_chan_params_free(&p);
	return ret;
}

static int
server_accept_alloc_inject_thread(void *arg)
{
	struct open_exec_ctx *ctx = arg;
	ctx->server_ch = dssh_chan_accept(ctx->server, ctx->cbs, 1000);
	return ctx->server_ch == NULL ? DSSH_ERROR_INIT : 0;
}

static int
test_server_accept_alloc_sweep(void)
{
	bool fault_seen = false;
	for (int n = 0; n < 50; n++) {
		struct conn_ctx ctx;
		dssh_test_alloc_exclude_new_threads();
		if (conn_setup(&ctx) != 0)
			return TEST_FAIL;
		struct open_exec_ctx oc = {
			.client = ctx.client,
			.server = ctx.server,
			.client_ch = NULL,
			.server_ch = NULL,
			.command = "echo srvalloc",
			.cbs = &accept_cbs_all,
		};
		struct channel_workers workers;
		channel_workers_init(&workers, &ctx);
		int ret = channel_worker_start(&workers, 0, client_open_exec_nofail_thread, &oc);
		dssh_test_alloc_fail_after(n);
		if (ret == 0)
			ret = channel_worker_start(&workers, 1, server_accept_alloc_inject_thread, &oc);
		if (ret != 0) {
			channel_workers_finish(&workers);
			dssh_test_alloc_reset();
			conn_cleanup(&ctx);
			return TEST_FAIL;
		}
		channel_worker_join(&workers, 1);
		int count = dssh_test_alloc_count();
		dssh_test_alloc_reset();
		bool injected = count > n;
		fault_seen = fault_seen || injected;
		if (oc.server_ch == NULL)
			channel_workers_cancel(&workers);
		channel_workers_finish(&workers);
		bool complete = oc.client_ch != NULL && oc.server_ch != NULL
		    && workers.results[0] == 0 && workers.results[1] == 0;
		/* Session cleanup owns every remaining registered channel. */
		conn_cleanup(&ctx);
		if (!injected) {
			ASSERT_TRUE(complete && fault_seen);
			return TEST_PASS;
		}
	}
	return TEST_FAIL;
}

/* ================================================================
 * Subsystem: write zero bytes
 * ================================================================ */

static int
test_subsys_write_zero(void)
{
	ASSERT_EQ(stream_body_run(STREAM_SUBSYS_ZERO), TEST_PASS);
	return TEST_PASS;
}

/* ================================================================
 * Item 64: poll deadline must not reset on spurious wakeup
 * ================================================================ */

struct spam_cnd_arg {
	cnd_t *cnd;
	atomic_bool stop;
	atomic_uint signals;
};

static int
spam_cnd_thread(void *arg)
{
	struct spam_cnd_arg *sa = arg;
	const struct timespec duration = {.tv_nsec = 50000000L};
	while (!atomic_load(&sa->stop)) {
		int ret = thrd_sleep(&duration, NULL);
		if (ret != 0 && ret != -1)
			return -1;
		if (atomic_load(&sa->stop))
			break;
		if (cnd_broadcast(sa->cnd) != thrd_success)
			return -1;
		unsigned signals = atomic_load(&sa->signals);
		if (signals < UINT_MAX)
			atomic_store(&sa->signals, signals + 1);
	}
	return 0;
}


static void
conn_actor_stop_spammer(void *arg)
{
	struct spam_cnd_arg *sa = arg;
	atomic_store(&sa->stop, true);
}

static int
test_poll_deadline_not_reset(void)
{
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_FAIL;
	struct open_exec_ctx oc = {.client = ctx.client, .server = ctx.server,
	    .command = "cmd", .cbs = &accept_cbs_all, .accept_timeout = 30000};
	ASSERT_EQ(open_exec_channel(&ctx, &oc), 0);
	struct spam_cnd_arg sa = {.cnd = &oc.server_ch->poll_cnd};
	atomic_init(&sa.stop, false);
	atomic_init(&sa.signals, 0);
	struct channel_workers workers;
	channel_workers_init(&workers, &ctx);
	workers.release = conn_actor_stop_spammer;
	workers.resource = &sa;
	ASSERT_EQ(channel_worker_start(&workers, 0, spam_cnd_thread, &sa), 0);
	ASSERT_TRUE(conn_actor_checkpoint());
	struct timespec start, end;
	ASSERT_EQ(clock_gettime(CLOCK_MONOTONIC, &start), 0);
	ASSERT_TRUE(start.tv_nsec >= 0 && start.tv_nsec < 1000000000L);
	int events = dssh_chan_poll(oc.server_ch, DSSH_POLL_READ, 500);
	ASSERT_EQ(clock_gettime(CLOCK_MONOTONIC, &end), 0);
	ASSERT_TRUE(end.tv_nsec >= 0 && end.tv_nsec < 1000000000L);
	conn_actor_stop_spammer(&sa);
	channel_workers_finish(&workers);
	ASSERT_EQ(workers.results[0], 0);
	ASSERT_EQ(events, 0);
	ASSERT_TRUE(atomic_load(&sa.signals) > 0);
	long nanoseconds = end.tv_nsec - start.tv_nsec;
	double fraction = (double)nanoseconds;
	double elapsed = difftime(end.tv_sec, start.tv_sec) + fraction / 1000000000.0;
	ASSERT_TRUE(elapsed >= 0.4 && elapsed <= 1.2);
	ASSERT_EQ(dssh_chan_close(oc.client_ch, 0), 0);
	ASSERT_EQ(dssh_chan_close(oc.server_ch, 0), 0);
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

	ASSERT_ERR(parse_channel_request(buf, len,
	    &rtype, &rtype_len, &want_reply, &rdata, &rdata_len), DSSH_ERROR_PARSE);

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
 * conn_setup_no_start: handshake + auth but no session_start.
 * Allows setting session-level callbacks before demux thread runs.
 * ================================================================ */

static int
conn_setup_no_start(struct conn_ctx *ctx)
{
	return conn_setup_mode(ctx, false);
}

static void
conn_cleanup_no_start(struct conn_ctx *ctx)
{
	conn_cleanup(ctx);
}

/* ================================================================
 * Zero-copy client-side open (dssh_chan_zc_open)
 * ================================================================ */

/* ZC RX state for client-side receive */
static uint32_t
client_zc_data_cb(dssh_channel channel, int stream,
    const uint8_t *data, size_t length, void *opaque)
{
	return conn_zc_receive(channel, stream, data, length, opaque);
}

struct client_zc_open_ctx {
	dssh_session client;
	dssh_channel client_ch;
	dssh_chan_zc_cb zc_cb;
	void *zc_cbdata;
};

static int
client_zc_open_thread(void *arg)
{
	struct client_zc_open_ctx *ctx = arg;
	struct dssh_chan_params p;
	int ret = dssh_chan_params_init(&p, DSSH_CHAN_SUBSYSTEM);
	if (ret != 0)
		return ret;
	ret = dssh_chan_params_set_subsystem(&p, "test-zc-client");
	if (ret == 0)
		ret = dssh_chan_params_set_pty(&p, false);
	if (ret == 0)
		ctx->client_ch = dssh_chan_zc_open(ctx->client, &p,
		    ctx->zc_cb, ctx->zc_cbdata);
	dssh_chan_params_free(&p);
	if (ret != 0)
		return ret;
	return ctx->client_ch == NULL ? DSSH_ERROR_INIT : 0;
}

static int
test_zc_open_client(void)
{
	static const uint8_t input[] = "hello to zc client";
	static const uint8_t reply[] = "zc reply from client";
	struct conn_ctx ctx;
	if (conn_setup(&ctx) != 0)
		return TEST_FAIL;
	struct conn_zc_fixture fixture;
	ASSERT_TRUE(conn_zc_init(&ctx, &fixture, sizeof(input), false));
	struct client_zc_open_ctx client = {
		.client = ctx.client, .zc_cb = client_zc_data_cb, .zc_cbdata = &fixture,
	};
	struct open_subsys_ctx server = {
		.client = ctx.client, .server = ctx.server, .subsystem = "test-zc-client",
	};
	ASSERT_EQ(open_channel_pair(&ctx, client_zc_open_thread, &client,
	    &client.client_ch, server_accept_subsys_thread, &server, &server.server_ch), 0);
	ASSERT_TRUE(conn_zc_checkpoint(CONN_ZC_OPENED));
	ASSERT_EQ(client.client_ch->io_model, DSSH_IO_ZC);
	ASSERT_TRUE(conn_zc_write_all(server.server_ch, input, sizeof(input), 5));
	ASSERT_TRUE(conn_zc_wait_flag(&fixture, &fixture.complete, 5));
	ASSERT_TRUE(conn_zc_checkpoint(CONN_ZC_RECEIVED));
	ASSERT_EQ_U(fixture.used, sizeof(input));
	ASSERT_MEM_EQ(fixture.data, input, sizeof(input));

	uint8_t *buffer;
	size_t capacity;
	ASSERT_EQ(conn_zc_reserve(&fixture, client.client_ch, &buffer, &capacity), 0);
	ASSERT_NOT_NULL(buffer);
	ASSERT_TRUE(capacity >= sizeof(reply));
	memcpy(buffer, reply, sizeof(reply));
	ASSERT_EQ(conn_zc_send(&fixture, sizeof(reply)), 0);
	uint8_t received[sizeof(reply)];
	ASSERT_TRUE(conn_zc_read_exact(server.server_ch, received, sizeof(received), 5));
	ASSERT_MEM_EQ(received, reply, sizeof(reply));
	ASSERT_EQ(dssh_chan_close(client.client_ch, -1), 0);
	ASSERT_EQ(dssh_chan_close(server.server_ch, -1), 0);
	conn_cleanup(&ctx);
	ASSERT_TRUE(fixture.retired);
	ASSERT_FALSE(atomic_load(&fixture.failed));
	return TEST_PASS;
}

/* ================================================================
 * ZC cancel
 * ================================================================ */

static int
test_zc_cancel(void)
{
	static const uint8_t input[] = "after cancel";
	struct conn_ctx ctx;
	if (conn_setup(&ctx) != 0)
		return TEST_FAIL;
	struct conn_zc_fixture fixture;
	ASSERT_TRUE(conn_zc_init(&ctx, &fixture, 0, false));
	struct client_zc_open_ctx client = {
		.client = ctx.client, .zc_cb = client_zc_data_cb, .zc_cbdata = &fixture,
	};
	struct open_subsys_ctx server = {
		.client = ctx.client, .server = ctx.server, .subsystem = "test-zc-cancel",
	};
	ASSERT_EQ(open_channel_pair(&ctx, client_zc_open_thread, &client,
	    &client.client_ch, server_accept_subsys_thread, &server, &server.server_ch), 0);
	ASSERT_TRUE(conn_zc_checkpoint(CONN_ZC_OPENED));
	uint8_t *buffer;
	size_t capacity;
	ASSERT_EQ(conn_zc_reserve(&fixture, client.client_ch, &buffer, &capacity), 0);
	ASSERT_NOT_NULL(buffer);
	ASSERT_TRUE(capacity > 0);
	ASSERT_EQ(conn_zc_cancel(&fixture), 0);
	ASSERT_ERR(dssh_chan_zc_cancel(client.client_ch), DSSH_ERROR_INVALID);
	ASSERT_EQ(conn_zc_reserve(&fixture, client.client_ch, &buffer, &capacity), 0);
	ASSERT_TRUE(capacity < SIZE_MAX);
	ASSERT_ERR(conn_zc_send(&fixture, capacity + 1), DSSH_ERROR_TOOLONG);
	ASSERT_FALSE(atomic_load(&ctx.client->trans.tx_owner_active));
	ASSERT_FALSE(dssh_session_is_terminated(ctx.client));
	ASSERT_EQ(conn_zc_reserve(&fixture, client.client_ch, &buffer, &capacity), 0);
	ASSERT_ERR(dssh_chan_zc_getbuf(client.client_ch, 0, &buffer, &capacity), DSSH_ERROR_INVALID);
	ASSERT_EQ(conn_zc_cancel(&fixture), 0);
	ASSERT_EQ(conn_zc_reserve(&fixture, client.client_ch, &buffer, &capacity), 0);
	ASSERT_NOT_NULL(buffer);
	ASSERT_TRUE(capacity >= sizeof(input));
	memcpy(buffer, input, sizeof(input));
	ASSERT_EQ(conn_zc_send(&fixture, sizeof(input)), 0);
	uint8_t received[sizeof(input)];
	ASSERT_TRUE(conn_zc_read_exact(server.server_ch, received, sizeof(received), 5));
	ASSERT_MEM_EQ(received, input, sizeof(input));
	ASSERT_EQ(dssh_chan_close(client.client_ch, -1), 0);
	ASSERT_EQ(dssh_chan_close(server.server_ch, -1), 0);
	conn_cleanup(&ctx);
	ASSERT_TRUE(fixture.retired);
	ASSERT_FALSE(atomic_load(&fixture.failed));
	return TEST_PASS;
}

/* ================================================================
 * ZC NULL parameter guards
 * ================================================================ */

static int
test_zc_open_null(void)
{
	struct dssh_chan_params p;
	dssh_chan_params_init(&p, DSSH_CHAN_SUBSYSTEM);
	dssh_chan_params_set_subsystem(&p, "test");

	/* NULL session */
	ASSERT_NULL(dssh_chan_zc_open(NULL, &p, client_zc_data_cb, NULL));
	/* NULL params (sess=NULL triggers first, so this is a combined check) */
	ASSERT_NULL(dssh_chan_zc_open(NULL, NULL, client_zc_data_cb, NULL));
	/* NULL callback */
	ASSERT_NULL(dssh_chan_zc_open(NULL, &p, NULL, NULL));

	/* ZC getbuf/send/cancel NULL guards */
	uint8_t *buf;
	size_t max_len;
	ASSERT_ERR(dssh_chan_zc_getbuf(NULL, 0, &buf, &max_len),
	    DSSH_ERROR_INVALID);
	ASSERT_ERR(dssh_chan_zc_send(NULL, 0),
	    DSSH_ERROR_INVALID);
	ASSERT_ERR(dssh_chan_zc_cancel(NULL),
	    DSSH_ERROR_INVALID);

	dssh_chan_params_free(&p);
	return TEST_PASS;
}

/* ================================================================
 * Event callback coverage
 * ================================================================ */

/* Dummy event callback for verifying setter stores the pointer */
static void
test_event_cb(dssh_channel ch, const struct dssh_chan_event *event,
    void *cbdata)
{
	(void)ch;
	(void)event;
	(void)cbdata;
}

/* Dummy state for verifying cbdata storage */
struct event_cb_state {
	int dummy;
};

static int
test_session_set_event_cb(void)
{
	struct conn_ctx ctx;
	if (conn_setup_no_start(&ctx) < 0)
		return TEST_FAIL;

	/* NULL sess returns error */
	ASSERT_ERR(dssh_session_set_event_cb(NULL, test_event_cb, NULL),
	    DSSH_ERROR_INVALID);

	/* Set event callback before session_start -- should succeed */
	struct event_cb_state es;
	memset(&es, 0, sizeof(es));
	ASSERT_EQ(dssh_session_set_event_cb(ctx.client, test_event_cb, &es), 0);

	/* Verify the callback is stored on the session */
	ASSERT_TRUE(ctx.client->default_event_cb == test_event_cb);
	ASSERT_TRUE(ctx.client->default_event_cbdata == &es);

	/* Set to NULL (disable) */
	ASSERT_EQ(dssh_session_set_event_cb(ctx.client, NULL, NULL), 0);
	ASSERT_NULL(ctx.client->default_event_cb);
	ASSERT_NULL(ctx.client->default_event_cbdata);

	conn_cleanup_no_start(&ctx);
	return TEST_PASS;
}

static int
test_session_set_event_cb_toolate(void)
{
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_FAIL;

	/* After session_start, setting event callback should be refused */
	ASSERT_ERR(dssh_session_set_event_cb(ctx.client, test_event_cb, NULL),
	    DSSH_ERROR_TOOLATE);
	ASSERT_NULL(ctx.client->default_event_cb);
	ASSERT_NULL(ctx.client->default_event_cbdata);

	conn_cleanup(&ctx);
	return TEST_PASS;
}

static int
test_chan_set_event_cb(void)
{
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_FAIL;

	/* NULL guard */
	ASSERT_ERR(dssh_chan_set_event_cb(NULL, test_event_cb, NULL),
	    DSSH_ERROR_INVALID);

	/* Open a channel and set per-channel event callback */
	struct open_exec_ctx oc = {
		.client = ctx.client,
		.server = ctx.server,
		.command = "cmd",
		.cbs = &accept_cbs_all,
		.accept_timeout = 30000,
	};
	if (open_exec_channel(&ctx, &oc) < 0) {
		conn_cleanup(&ctx);
		return TEST_FAIL;
	}

	struct event_cb_state es;
	memset(&es, 0, sizeof(es));

	/* Set per-channel event callback -- should succeed */
	ASSERT_EQ(dssh_chan_set_event_cb(oc.client_ch, test_event_cb, &es), 0);

	/* Verify it's stored on the channel */
	ASSERT_TRUE(event_body_pair_is(oc.client_ch, test_event_cb, &es));

	/* Set to NULL (disable) */
	ASSERT_EQ(dssh_chan_set_event_cb(oc.client_ch, NULL, NULL), 0);
	ASSERT_TRUE(event_body_pair_is(oc.client_ch, NULL, NULL));

	ASSERT_EQ(dssh_chan_close(oc.client_ch, 0), 0);
	ASSERT_EQ(dssh_chan_close(oc.server_ch, 0), 0);
	conn_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Event queue limits coverage
 * ================================================================ */

static int
test_session_set_max_events(void)
{
	struct conn_ctx ctx;
	if (conn_setup_no_start(&ctx) < 0)
		return TEST_FAIL;

	/* NULL guard */
	ASSERT_ERR(dssh_session_set_max_events(NULL, 128),
	    DSSH_ERROR_INVALID);

	/* Set custom limit */
	ASSERT_EQ(dssh_session_set_max_events(ctx.client, 128), 0);
	ASSERT_EQ_U(ctx.client->default_max_events, 128);

	/* Disable the event-count cap; retained event memory stays bounded. */
	ASSERT_EQ(dssh_session_set_max_events(ctx.client, 0), 0);
	ASSERT_EQ_U(ctx.client->default_max_events, 0);

	conn_cleanup_no_start(&ctx);
	return TEST_PASS;
}

static int
test_session_set_max_events_toolate(void)
{
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_FAIL;

	ASSERT_ERR(dssh_session_set_max_events(ctx.client, 128),
	    DSSH_ERROR_TOOLATE);
	ASSERT_EQ_U(ctx.client->default_max_events, DSSH_DEFAULT_MAX_EVENTS);

	conn_cleanup(&ctx);
	return TEST_PASS;
}

static int
test_chan_set_max_events(void)
{
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_FAIL;

	/* NULL guard */
	ASSERT_ERR(dssh_chan_set_max_events(NULL, 32),
	    DSSH_ERROR_INVALID);

	struct open_exec_ctx oc = {
		.client = ctx.client,
		.server = ctx.server,
		.command = "cmd",
		.cbs = &accept_cbs_all,
		.accept_timeout = 30000,
	};
	if (open_exec_channel(&ctx, &oc) < 0) {
		conn_cleanup(&ctx);
		return TEST_FAIL;
	}

	/* Set per-channel limit */
	ASSERT_EQ(dssh_chan_set_max_events(oc.client_ch, 32), 0);
	ASSERT_TRUE(event_body_limit_is(oc.client_ch, 32));

	/* Disable the event-count cap; retained event memory stays bounded. */
	ASSERT_EQ(dssh_chan_set_max_events(oc.client_ch, 0), 0);
	ASSERT_TRUE(event_body_limit_is(oc.client_ch, 0));

	ASSERT_EQ(dssh_chan_close(oc.client_ch, 0), 0);
	ASSERT_EQ(dssh_chan_close(oc.server_ch, 0), 0);
	conn_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * dssh_chan_get_pty coverage
 * ================================================================ */

static int
test_chan_get_pty(void)
{
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_FAIL;

	/* Open shell with PTY */
	struct open_shell_ctx oc = {
		.client = ctx.client,
		.server = ctx.server,
		.term = "vt100",
		.cols = UINT32_C(0x01020304),
		.rows = 43,
		.wpx = 800,
		.hpx = 600,
		.cbs = &accept_cbs_all,
	};

	ASSERT_EQ(open_channel_pair(&ctx, client_open_shell_thread, &oc,
	    &oc.client_ch, server_accept_shell_thread, &oc,
	    &oc.server_ch), 0);

	ASSERT_NOT_NULL(oc.client_ch);
	ASSERT_NOT_NULL(oc.server_ch);

	/* Server channel should have PTY info */
	ASSERT_TRUE(dssh_chan_has_pty(oc.server_ch));
	const struct dssh_chan_params *pty = dssh_chan_get_pty(oc.server_ch);
	ASSERT_NOT_NULL(pty);
	ASSERT_STR_EQ(pty->term, "vt100");
	ASSERT_EQ_U(pty->cols, UINT32_C(0x01020304));
	ASSERT_EQ_U(pty->rows, 43);
	ASSERT_EQ_U(pty->wpx, 800);
	ASSERT_EQ_U(pty->hpx, 600);

	/* Client channel should also have PTY (it requested it) */
	ASSERT_TRUE(dssh_chan_has_pty(oc.client_ch));
	const struct dssh_chan_params *cpty = dssh_chan_get_pty(oc.client_ch);
	ASSERT_NOT_NULL(cpty);
	ASSERT_STR_EQ(cpty->term, "vt100");
	ASSERT_EQ_U(cpty->cols, UINT32_C(0x01020304));
	ASSERT_EQ_U(cpty->rows, 43);
	ASSERT_EQ_U(cpty->wpx, 800);
	ASSERT_EQ_U(cpty->hpx, 600);
	ASSERT_NULL(dssh_chan_get_command(oc.client_ch));
	ASSERT_NULL(dssh_chan_get_subsystem(oc.client_ch));

	/* Subsystem channel without PTY should return NULL */
	struct open_subsys_ctx sc = {
		.client = ctx.client,
		.server = ctx.server,
		.subsystem = "test-no-pty",
	};
	ASSERT_EQ(open_channel_pair(&ctx, client_open_subsys_thread, &sc,
	    &sc.client_ch, server_accept_subsys_thread, &sc,
	    &sc.server_ch), 0);

	ASSERT_FALSE(dssh_chan_has_pty(sc.client_ch));
	ASSERT_NULL(dssh_chan_get_pty(sc.client_ch));
	ASSERT_EQ(dssh_chan_get_type(sc.server_ch), DSSH_CHAN_SUBSYSTEM);
	ASSERT_STR_EQ(dssh_chan_get_subsystem(sc.server_ch), "test-no-pty");
	ASSERT_NULL(dssh_chan_get_command(sc.server_ch));
	ASSERT_FALSE(dssh_chan_has_pty(sc.server_ch));
	ASSERT_NULL(dssh_chan_get_pty(sc.server_ch));
	ASSERT_EQ(dssh_chan_close(sc.client_ch, -1), 0);
	ASSERT_EQ(dssh_chan_close(sc.server_ch, -1), 0);

	ASSERT_EQ(dssh_chan_close(oc.client_ch, 0), 0);
	ASSERT_EQ(dssh_chan_close(oc.server_ch, 0), 0);
	conn_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Environment variable roundtrip (exercises accept_parse_env +
 * parse_env_data on the server side)
 * ================================================================ */

static int
accept_env_cb(dssh_channel ch, const struct dssh_chan_params *p, void *cbdata)
{
	(void)p;
	(void)cbdata;
	const uint8_t *rtype;
	uint32_t rtype_len;
	bool want_reply;
	const uint8_t *data;
	size_t data_len;
	/* The occupied setup mailbox retains the actual peer request throughout
	 * this callback. Both accepted and rejected env requests must be silent. */
	int ret = parse_channel_request(ch->setup_payload, ch->setup_payload_len,
	    &rtype, &rtype_len, &want_reply, &data, &data_len);
	if (ret < 0 || want_reply || rtype_len != 3 || memcmp(rtype, "env", 3) != 0)
		return DSSH_ERROR_PARSE;
	return 0;
}

static int
client_open_exec_with_env_thread(void *arg)
{
	struct open_exec_ctx *ctx = arg;
	struct dssh_chan_params p;
	int ret = dssh_chan_params_init(&p, DSSH_CHAN_EXEC);
	if (ret != 0)
		return ret;
	if (ret == 0)
		ret = dssh_chan_params_set_command(&p, ctx->command);
	if (ret == 0)
		ret = dssh_chan_params_set_pty(&p, false);
	if (ret == 0)
		ret = dssh_chan_params_add_env(&p, "LANG", "en_US.UTF-8");
	if (ret == 0)
		ret = dssh_chan_params_add_env(&p, "TERM", "xterm");
	if (ret == 0)
		ctx->client_ch = dssh_chan_open(ctx->client, &p);
	dssh_chan_params_free(&p);
	return ret == 0 ? wait_stream_writable(ctx->client_ch) : ret;
}

static struct dssh_chan_accept_cbs accept_cbs_with_env = {
	.pty_req = accept_pty_cb,
	.env = accept_env_cb,
	.shell = accept_all_shell,
	.exec = accept_all_exec,
	.subsystem = accept_all_subsystem,
};

static int
test_env_roundtrip(void)
{
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_FAIL;

	struct open_exec_ctx oc = {
		.client = ctx.client,
		.server = ctx.server,
		.command = "env",
		.cbs = &accept_cbs_with_env,
		.accept_timeout = 30000,
	};

	ASSERT_EQ(open_channel_pair(&ctx, client_open_exec_with_env_thread, &oc,
	    &oc.client_ch, server_accept_exec_thread, &oc,
	    &oc.server_ch), 0);

	ASSERT_NOT_NULL(oc.client_ch);
	ASSERT_NOT_NULL(oc.server_ch);

	/* Server channel should have received the env vars */
	/* The client snapshot retains both requested entries independently of
	 * the server's callback selection. The builder is already released. */
	ASSERT_EQ_U(oc.client_ch->params.env_count, 2);
	ASSERT_NOT_NULL(oc.client_ch->params.env);
	ASSERT_STR_EQ(oc.client_ch->params.env[0].name, "LANG");
	ASSERT_STR_EQ(oc.client_ch->params.env[0].value, "en_US.UTF-8");
	ASSERT_STR_EQ(oc.client_ch->params.env[1].name, "TERM");
	ASSERT_STR_EQ(oc.client_ch->params.env[1].value, "xterm");
	ASSERT_NOT_NULL(oc.server_ch->params.env);
	ASSERT_EQ_U(oc.server_ch->params.env_count, 2);
	ASSERT_STR_EQ(oc.server_ch->params.env[0].name, "LANG");
	ASSERT_STR_EQ(oc.server_ch->params.env[0].value, "en_US.UTF-8");
	ASSERT_STR_EQ(oc.server_ch->params.env[1].name, "TERM");
	ASSERT_STR_EQ(oc.server_ch->params.env[1].value, "xterm");

	ASSERT_EQ(dssh_chan_close(oc.client_ch, 0), 0);
	ASSERT_EQ(dssh_chan_close(oc.server_ch, 0), 0);
	conn_cleanup(&ctx);
	return TEST_PASS;
}

static int
accept_selected_env_cb(dssh_channel ch, const struct dssh_chan_params *p, void *cbdata)
{
	int ret = accept_env_cb(ch, p, cbdata);
	if (ret < 0)
		return ret;
	if (p->env_count == 0)
		return DSSH_ERROR_INVALID;
	const char *name = p->env[p->env_count - 1].name;
	return strcmp(name, "TERM") == 0 ? 0 : DSSH_ERROR_REJECTED;
}

static int
test_env_callback_rejection(void)
{
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_FAIL;
	struct dssh_chan_accept_cbs cbs = accept_cbs_with_env;
	cbs.env = accept_selected_env_cb;
	struct open_exec_ctx oc = {
		.client = ctx.client,
		.server = ctx.server,
		.command = "env",
		.cbs = &cbs,
		.accept_timeout = 30000,
	};
	ASSERT_EQ(open_channel_pair(&ctx, client_open_exec_with_env_thread, &oc,
	    &oc.client_ch, server_accept_exec_thread, &oc,
	    &oc.server_ch), 0);
	ASSERT_NOT_NULL(oc.client_ch);
	ASSERT_NOT_NULL(oc.server_ch);
	/* The client snapshot retains both requested entries independently of
	 * the server's callback selection. The builder is already released. */
	ASSERT_EQ_U(oc.client_ch->params.env_count, 2);
	ASSERT_NOT_NULL(oc.client_ch->params.env);
	ASSERT_STR_EQ(oc.client_ch->params.env[0].name, "LANG");
	ASSERT_STR_EQ(oc.client_ch->params.env[0].value, "en_US.UTF-8");
	ASSERT_STR_EQ(oc.client_ch->params.env[1].name, "TERM");
	ASSERT_STR_EQ(oc.client_ch->params.env[1].value, "xterm");
	ASSERT_NOT_NULL(oc.server_ch->params.env);
	ASSERT_EQ_U(oc.server_ch->params.env_count, 1);
	ASSERT_STR_EQ(oc.server_ch->params.env[0].name, "TERM");
	ASSERT_STR_EQ(oc.server_ch->params.env[0].value, "xterm");
	ASSERT_EQ(dssh_chan_close(oc.client_ch, 0), 0);
	ASSERT_EQ(dssh_chan_close(oc.server_ch, 0), 0);
	conn_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Test table
 * ================================================================ */

/* Normal teardown joins exactly once; repeated stop does not join again. */
static int
test_lifecycle_join_once(void)
{
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_FAIL;
	struct open_exec_ctx oc = {
		.client = ctx.client, .server = ctx.server,
		.command = "join-once", .cbs = &accept_cbs_all, .accept_timeout = 30000,
	};
	ASSERT_OK(open_exec_channel(&ctx, &oc));
	dssh_session_terminate(ctx.client);
	mock_io_close_c2s(&ctx.io);
	mock_io_close_s2c(&ctx.io);
	ASSERT_TRUE(dssh_test_wait_bool(&ctx.client->demux_running, false));
	dssh_test_thrd_reset();
	dssh_session_stop(ctx.client);
	ASSERT_EQ(dssh_test_join_count(), 1);
	ASSERT_FALSE(ctx.client->conn_initialized);
	ASSERT_NULL(ctx.client->channels);
	dssh_test_join_fail_next();
	dssh_session_stop(ctx.client);
	int calls = dssh_test_join_count();
	dssh_test_thrd_reset();
	ASSERT_EQ(calls, 0);
	conn_cleanup(&ctx);
	return TEST_PASS;
}

/* Launched only by the separate process driver, before the normal runner.
 * Full handshake/auth/channel setup precedes the injected native join failure.
 * The worker is already stopped but remains a valid joinable owned thread. */
static int
run_join_fatal_child(const char *operation)
{
	if (strcmp(operation, "stop") != 0 && strcmp(operation, "cleanup") != 0)
		return 2;
	if (signal(SIGPIPE, SIG_IGN) == SIG_ERR)
		return 3;
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return 3;
	struct open_exec_ctx oc = {
		.client = ctx.client, .server = ctx.server,
		.command = "join-fatal", .cbs = &accept_cbs_all, .accept_timeout = 30000,
	};
	if (open_exec_channel(&ctx, &oc) != 0) {
		conn_cleanup(&ctx);
		return 3;
	}
	dssh_session_terminate(ctx.client);
	mock_io_close_c2s(&ctx.io);
	mock_io_close_s2c(&ctx.io);
	if (!dssh_test_wait_bool(&ctx.client->demux_running, false))
		return 3;
	dssh_test_join_fail_next();
	if (strcmp(operation, "cleanup") == 0)
		dssh_session_cleanup(ctx.client);
	else
		dssh_session_stop(ctx.client);
	/* A returned fatal path is a failed probe. Do not try uncertain cleanup.
	 * _Exit avoids normal-exit allocator diagnostics changing that status. */
	_Exit(4);
}

struct lifecycle_callback_state {
	dssh_session sess;
	bool cleanup;
	bool retained;
	atomic_bool done;
};

static void
lifecycle_unimplemented_cb(uint32_t seq, void *cbdata)
{
	(void)seq;
	struct lifecycle_callback_state *state = cbdata;

	if (state->cleanup)
		dssh_session_cleanup(state->sess);
	else
		dssh_session_stop(state->sess);
	state->retained = state->sess->trans.rx_packet != NULL;
	atomic_store(&state->done, true);
}

static int
test_lifecycle_demux_callback(void)
{
	for (int cleanup = 0; cleanup < 2; cleanup++) {
		struct conn_ctx ctx;
		if (conn_setup_no_start(&ctx) < 0)
			return TEST_FAIL;
		struct lifecycle_callback_state state = {
			.sess = ctx.server, .cleanup = cleanup != 0,
		};
		atomic_init(&state.done, false);
		ASSERT_OK(dssh_session_set_unimplemented_cb(ctx.server, lifecycle_unimplemented_cb, &state));
		ASSERT_OK(dssh_session_start(ctx.client));
		ASSERT_OK(dssh_session_start(ctx.server));
		ASSERT_OK(send_unimplemented(ctx.client, 0));
		ASSERT_TRUE(dssh_test_wait_bool(&state.done, true));
		ASSERT_TRUE(state.retained);
		ASSERT_TRUE(dssh_session_is_terminated(ctx.server));
		ASSERT_TRUE(ctx.server->conn_initialized);
		mock_io_close_c2s(&ctx.io);
		mock_io_close_s2c(&ctx.io);
		dssh_session_stop(ctx.server);
		ASSERT_FALSE(ctx.server->conn_initialized);
		conn_cleanup(&ctx);
	}
	return TEST_PASS;
}

static int
test_lifecycle_receive_callback(void)
{
	struct conn_ctx ctx;
	if (conn_setup_no_start(&ctx) < 0)
		return TEST_FAIL;
	struct lifecycle_callback_state state = { .sess = ctx.server, .cleanup = true };
	atomic_init(&state.done, false);
	ASSERT_OK(dssh_session_set_unimplemented_cb(ctx.server, lifecycle_unimplemented_cb, &state));
	ASSERT_OK(send_unimplemented(ctx.client, 0));
	uint8_t type;
	uint8_t *payload;
	size_t len;
	ASSERT_ERR(recv_packet(ctx.server, &type, &payload, &len), DSSH_ERROR_TERMINATED);
	ASSERT_TRUE(atomic_load(&state.done));
	ASSERT_TRUE(state.retained);
	ASSERT_TRUE(dssh_session_is_terminated(ctx.server));
	conn_cleanup_no_start(&ctx);
	return TEST_PASS;
}

/* Defensive handling of lifecycle calls from restricted callbacks. */
static void
lifecycle_terminate_cb(dssh_session sess, void *cbdata)
{
	struct lifecycle_callback_state *state = cbdata;

	dssh_session_cleanup(sess);
	state->retained = sess->trans.rx_packet != NULL;
	atomic_store(&state->done, true);
}

static dssh_log_action
lifecycle_log_cb(const struct dssh_log_record *record, void *cbdata)
{
	(void)record;
	struct lifecycle_callback_state *state = cbdata;

	dssh_session_cleanup(state->sess);
	state->retained = state->sess->trans.rx_packet != NULL;
	atomic_store(&state->done, true);
	return DSSH_LOG_LOCAL_ONLY;
}

static int
test_lifecycle_recursive_callbacks(void)
{
	for (int log = 0; log < 2; log++) {
		dssh_session sess = dssh_session_init(false, 0);
		ASSERT_NOT_NULL(sess);
		struct lifecycle_callback_state state = { .sess = sess };
		atomic_init(&state.done, false);
		if (log) {
			ASSERT_OK(dssh_session_set_log_cb(sess, lifecycle_log_cb, &state));
			DSSH_LOGF(sess, DSSH_LOG_ERROR, DSSH_ERROR_NONE, "Lifecycle callback test");
			ASSERT_OK(dssh_session_set_log_cb(sess, NULL, NULL));
		}
		else {
			ASSERT_OK(dssh_session_set_terminate_cb(sess, lifecycle_terminate_cb, &state));
			ASSERT_TRUE(dssh_session_terminate(sess));
		}
		ASSERT_TRUE(atomic_load(&state.done));
		ASSERT_TRUE(state.retained);
		ASSERT_TRUE(dssh_session_is_terminated(sess));
		dssh_session_cleanup(sess);
	}
	return TEST_PASS;
}

static int
test_lifecycle_tx_reservation(void)
{
	struct conn_ctx ctx;
	if (conn_setup_no_start(&ctx) < 0)
		return TEST_FAIL;
	ASSERT_OK(tx_acquire(ctx.client, SSH_MSG_IGNORE));
	dssh_session_cleanup(ctx.client);
	ASSERT_TRUE(tx_owner_is_current(ctx.client));
	ASSERT_TRUE(dssh_session_is_terminated(ctx.client));
	ASSERT_NOT_NULL(ctx.client->trans.tx_packet);
	tx_unlock_with_slots(ctx.client);
	conn_cleanup_no_start(&ctx);
	return TEST_PASS;
}

static int
test_clock_deadline(void)
{
	dssh_test_clock_reset();
	struct timespec ts = { .tv_sec = 42, .tv_nsec = 123 };
	dssh_test_clock_fail_after(0);
	int invalid = dssh_deadline_from_ms(&ts, 0);
	int failed = dssh_deadline_from_ms(&ts, 1);
	int calls = dssh_test_clock_count();
	dssh_test_clock_reset();
	ASSERT_ERR(invalid, DSSH_ERROR_INVALID);
	ASSERT_ERR(failed, DSSH_ERROR_INIT);
	ASSERT_EQ(calls, 1);
	ASSERT_TRUE(ts.tv_sec == 42);
	ASSERT_EQ(ts.tv_nsec, 123);

	struct timespec now = { .tv_sec = 100, .tv_nsec = 999500000L };
	dssh_test_clock_set(now);
	int ret = dssh_deadline_from_ms(&ts, 1);
	dssh_test_clock_reset();
	ASSERT_OK(ret);
	ASSERT_TRUE(ts.tv_sec == 101);
	ASSERT_EQ(ts.tv_nsec, 500000L);

	now.tv_sec = 100;
	now.tv_nsec = 123;
	dssh_test_clock_set(now);
	ret = dssh_deadline_from_ms(&ts, INTMAX_C(75000));
	dssh_test_clock_reset();
	ASSERT_OK(ret);
	ASSERT_TRUE(ts.tv_sec == 175);
	ASSERT_EQ(ts.tv_nsec, 123);

	now.tv_nsec = 1000000000L;
	dssh_test_clock_set(now);
	ret = dssh_deadline_from_ms(&ts, 1);
	dssh_test_clock_reset();
	ASSERT_ERR(ret, DSSH_ERROR_INVALID);
	ASSERT_TRUE(ts.tv_sec == 175);
	ASSERT_EQ(ts.tv_nsec, 123);
	return TEST_PASS;
}

static int
test_clock_poll_accept(void)
{
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_FAIL;
	struct open_exec_ctx oc = {
		.client = ctx.client, .server = ctx.server,
		.command = "clock-poll", .cbs = &accept_cbs_all,
		.accept_timeout = 30000,
	};
	ASSERT_OK(open_exec_channel(&ctx, &oc));
	dssh_test_clock_fail_after(0);
	int ret = dssh_chan_poll(oc.client_ch, DSSH_POLL_READ, 5000);
	dssh_test_clock_reset();
	ASSERT_ERR(ret, DSSH_ERROR_INIT);
	ASSERT_FALSE(dssh_session_is_terminated(ctx.client));
	ASSERT_EQ(dssh_chan_poll(oc.client_ch, DSSH_POLL_READ, 0), 0);

	struct dssh_incoming_open inc;
	dssh_test_clock_fail_after(0);
	ret = accept_incoming(ctx.server, &inc, 5000);
	dssh_test_clock_reset();
	ASSERT_ERR(ret, DSSH_ERROR_INIT);
	ASSERT_FALSE(dssh_session_is_terminated(ctx.server));

	dssh_test_clock_fail_after(0);
	ret = accept_incoming(ctx.server, &inc, 0);
	int clock_calls = dssh_test_clock_count();
	dssh_test_clock_reset();
	ASSERT_ERR(ret, DSSH_ERROR_NOMORE);
	ASSERT_EQ(clock_calls, 0);
	conn_cleanup(&ctx);
	return TEST_PASS;
}

static int
test_clock_pending_waits(void)
{
	for (int operation = 0; operation < 4; operation++) {
		struct conn_ctx ctx;
		if (conn_setup(&ctx) < 0)
			return TEST_FAIL;
		struct open_exec_ctx oc = {
			.client = ctx.client, .server = ctx.server,
			.command = "clock-pending", .cbs = &accept_cbs_all,
			.accept_timeout = 30000,
		};
		ASSERT_OK(open_exec_channel(&ctx, &oc));
		if (operation == 3) {
			ASSERT_EQ(mtx_lock(&oc.client_ch->buf_mtx), thrd_success);
			oc.client_ch->setup_mode = true;
			ASSERT_EQ(mtx_unlock(&oc.client_ch->buf_mtx), thrd_success);
		}
		dssh_test_clock_fail_after(0);
		int ret;
		if (operation == 0)
			ret = wait_open_response(ctx.client, oc.client_ch);
		else if (operation == 1)
			ret = wait_channel_request_response(ctx.client, oc.client_ch);
		else if (operation == 2)
			ret = tx_acquire(ctx.client, SSH_MSG_IGNORE);
		else {
			uint8_t type;
			uint8_t *payload;
			size_t len;
			ret = setup_recv(ctx.client, oc.client_ch, &type, &payload, &len);
		}
		int calls = dssh_test_clock_count();
		dssh_test_clock_reset();
		ASSERT_ERR(ret, DSSH_ERROR_TERMINATED);
		ASSERT_EQ(calls, 1);
		ASSERT_TRUE(dssh_session_is_terminated(ctx.client));
		ASSERT_FALSE(tx_owner_is_current(ctx.client));
		conn_cleanup(&ctx);
	}
	return TEST_PASS;
}

static int
test_clock_receive(void)
{
	struct conn_ctx ctx;
	if (conn_setup_no_start(&ctx) < 0)
		return TEST_FAIL;
	ASSERT_OK(dssh_session_set_rekey_seconds(ctx.server, 1));
	uint8_t message[] = { SSH_MSG_SERVICE_REQUEST };
	ASSERT_OK(send_packet(ctx.client, message, sizeof(message), NULL));
	uint8_t type;
	uint8_t *payload;
	size_t len;
	dssh_test_clock_fail_after(0);
	int ret = recv_packet(ctx.server, &type, &payload, &len);
	dssh_test_clock_reset();
	ASSERT_ERR(ret, DSSH_ERROR_TERMINATED);
	ASSERT_TRUE(dssh_session_is_terminated(ctx.server));
	conn_cleanup_no_start(&ctx);
	return TEST_PASS;
}

static int
test_extended_stream_types(void)
{
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_FAIL;
	struct open_exec_ctx oc = {
		.client = ctx.client, .server = ctx.server, .command = "extended-stream",
		.cbs = &accept_cbs_all, .accept_timeout = 30000,
	};
	ASSERT_OK(open_exec_channel(&ctx, &oc));
	ASSERT_EQ(mtx_lock(&oc.server_ch->buf_mtx), thrd_success);
	oc.server_ch->local_window = 8;
	oc.server_ch->window_max = 8;
	ASSERT_EQ(mtx_unlock(&oc.server_ch->buf_mtx), thrd_success);
	const uint32_t types[] = { 2, 32768, 65535, UINT32_MAX, UINT32_C(0xfe000000) };
	uint8_t packet[17];
	for (size_t i = 0; i < sizeof(types) / sizeof(types[0]); i++) {
		size_t pos = 0;
		packet[pos++] = SSH_MSG_CHANNEL_EXTENDED_DATA;
		DSSH_PUT_U32(oc.server_ch->local_id, packet, &pos);
		DSSH_PUT_U32(types[i], packet, &pos);
		DSSH_PUT_U32(4, packet, &pos);
		memcpy(&packet[pos], "drop", 4);
		pos += 4;
		ASSERT_OK(dssh_test_demux_dispatch(ctx.server, SSH_MSG_CHANNEL_EXTENDED_DATA, packet, pos));
		ASSERT_EQ(mtx_lock(&oc.server_ch->buf_mtx), thrd_success);
		uint32_t window = oc.server_ch->local_window;
		size_t stdout_bytes = oc.server_ch->buf.stdout_buf.used;
		size_t stderr_bytes = oc.server_ch->buf.stderr_buf.used;
		ASSERT_EQ(mtx_unlock(&oc.server_ch->buf_mtx), thrd_success);
		ASSERT_EQ_U(window, 8);
		ASSERT_EQ_U(stdout_bytes, 0);
		ASSERT_EQ_U(stderr_bytes, 0);
	}
	/* Type zero is malformed and must not become the ordinary stream. */
	size_t pos = 5;
	DSSH_PUT_U32(0, packet, &pos);
	ASSERT_ERR(dssh_test_demux_dispatch(ctx.server, SSH_MSG_CHANNEL_EXTENDED_DATA,
	    packet, sizeof(packet)), DSSH_ERROR_PARSE);
	/* A supported stderr message still reaches exactly its own stream. */
	pos = 5;
	DSSH_PUT_U32(1, packet, &pos);
	ASSERT_OK(dssh_test_demux_dispatch(ctx.server, SSH_MSG_CHANNEL_EXTENDED_DATA, packet, sizeof(packet)));
	uint8_t out[4];
	ASSERT_EQ(dssh_chan_read(oc.server_ch, 1, out, sizeof(out)), 4);
	ASSERT_MEM_EQ(out, "drop", 4);
	ASSERT_EQ(dssh_chan_read(oc.server_ch, 0, NULL, 0), 0);
	ASSERT_FALSE(dssh_session_is_terminated(ctx.server));
	conn_cleanup(&ctx);
	return TEST_PASS;
}

struct extended_probe {
	unsigned calls;
	int stream;
	size_t bytes;
};

static uint32_t
extended_probe_cb(dssh_channel ch, int stream, const uint8_t *data, size_t len, void *cbdata)
{
	(void)ch;
	(void)data;
	struct extended_probe *probe = cbdata;
	probe->calls++;
	probe->stream = stream;
	probe->bytes = len;
	uint32_t consumed = len > UINT32_MAX ? UINT32_MAX : (uint32_t)len;
	return consumed;
}

static int
accept_extended_probe(dssh_channel ch, const struct dssh_chan_params *params,
    struct dssh_chan_accept_result *result, void *cbdata)
{
	(void)ch;
	(void)params;
	result->max_window = 8;
	result->zc_cb = extended_probe_cb;
	result->zc_cbdata = cbdata;
	return 0;
}

static int
test_extended_zc_types(void)
{
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_FAIL;
	struct extended_probe probe = {0};
	struct dssh_chan_accept_cbs cbs = { .exec = accept_extended_probe, .cbdata = &probe };
	struct open_exec_ctx oc = {
		.client = ctx.client, .server = ctx.server, .command = "extended-zc",
		.cbs = &cbs, .accept_timeout = 30000,
	};
	ASSERT_OK(open_exec_channel(&ctx, &oc));
	uint8_t packet[17];
	size_t pos = 0;
	packet[pos++] = SSH_MSG_CHANNEL_EXTENDED_DATA;
	DSSH_PUT_U32(oc.server_ch->local_id, packet, &pos);
	DSSH_PUT_U32(UINT32_MAX, packet, &pos);
	DSSH_PUT_U32(4, packet, &pos);
	memcpy(&packet[pos], "data", 4);
	pos += 4;
	ASSERT_OK(dssh_test_demux_dispatch(ctx.server, SSH_MSG_CHANNEL_EXTENDED_DATA, packet, pos));
	ASSERT_EQ_U(probe.calls, 0);
	ASSERT_EQ(mtx_lock(&oc.server_ch->buf_mtx), thrd_success);
	uint32_t window = oc.server_ch->local_window;
	ASSERT_EQ(mtx_unlock(&oc.server_ch->buf_mtx), thrd_success);
	ASSERT_EQ_U(window, 8);
	pos = 5;
	DSSH_PUT_U32(1, packet, &pos);
	ASSERT_OK(dssh_test_demux_dispatch(ctx.server, SSH_MSG_CHANNEL_EXTENDED_DATA, packet, sizeof(packet)));
	ASSERT_EQ_U(probe.calls, 1);
	ASSERT_EQ(probe.stream, 1);
	ASSERT_EQ_U(probe.bytes, 4);
	conn_cleanup(&ctx);
	return TEST_PASS;
}

static int
test_data_framing(void)
{
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_FAIL;
	struct open_exec_ctx oc = {
		.client = ctx.client, .server = ctx.server, .command = "data-framing",
		.cbs = &accept_cbs_all, .accept_timeout = 30000,
	};
	ASSERT_OK(open_exec_channel(&ctx, &oc));
	for (int extended = 0; extended < 2; extended++) {
		uint8_t packet[18] = {0};
		size_t pos = 0;
		uint8_t type = extended ? SSH_MSG_CHANNEL_EXTENDED_DATA : SSH_MSG_CHANNEL_DATA;
		packet[pos++] = type;
		DSSH_PUT_U32(oc.server_ch->local_id, packet, &pos);
		if (extended)
			DSSH_PUT_U32(1, packet, &pos);
		size_t length_pos = pos;
		DSSH_PUT_U32(4, packet, &pos);
		size_t fixed = pos;
		memcpy(&packet[pos], "data", 4);
		pos += 4;
		ASSERT_ERR(dssh_test_demux_dispatch(ctx.server, type, packet, fixed - 1), DSSH_ERROR_PARSE);
		ASSERT_ERR(dssh_test_demux_dispatch(ctx.server, type, packet, pos - 1), DSSH_ERROR_PARSE);
		ASSERT_ERR(dssh_test_demux_dispatch(ctx.server, type, packet, pos + 1), DSSH_ERROR_PARSE);
		DSSH_PUT_U32(UINT32_MAX, packet, &length_pos);
		ASSERT_ERR(dssh_test_demux_dispatch(ctx.server, type, packet, pos), DSSH_ERROR_PARSE);
		ASSERT_EQ(dssh_chan_read(oc.server_ch, 0, NULL, 0), 0);
		ASSERT_EQ(dssh_chan_read(oc.server_ch, 1, NULL, 0), 0);
		ASSERT_FALSE(dssh_session_is_terminated(ctx.server));
	}
	const size_t max_data = 32768;
	const size_t total = 13 + 32768;
	uint8_t *packet = malloc(total);
	ASSERT_NOT_NULL(packet);
	size_t pos = 0;
	packet[pos++] = SSH_MSG_CHANNEL_EXTENDED_DATA;
	DSSH_PUT_U32(oc.server_ch->local_id, packet, &pos);
	DSSH_PUT_U32(1, packet, &pos);
	DSSH_PUT_U32(32768, packet, &pos);
	memset(&packet[pos], 0x5a, max_data);
	int dispatched = dssh_test_demux_dispatch(ctx.server, packet[0], packet, total);
	int64_t available = dssh_chan_read(oc.server_ch, 1, NULL, 0);
	int64_t received = dssh_chan_read(oc.server_ch, 1, packet, max_data);
	bool matched = received == 32768;
	for (size_t i = 0; matched && i < max_data; i++) {
		if (packet[i] != 0x5a)
			matched = false;
	}
	free(packet);
	ASSERT_OK(dispatched);
	ASSERT_EQ(available, 32768);
	ASSERT_TRUE(matched);
	conn_cleanup(&ctx);
	return TEST_PASS;
}

static int
test_replenish_sync_failures(void)
{
	for (int fail = 0; fail < 2; fail++) {
		struct conn_ctx ctx;
		if (conn_setup(&ctx) < 0)
			return TEST_FAIL;
		struct open_exec_ctx oc = {
			.client = ctx.client, .server = ctx.server, .command = "replenish-sync",
			.cbs = &accept_cbs_all, .accept_timeout = 30000,
		};
		ASSERT_OK(open_exec_channel(&ctx, &oc));
		ASSERT_EQ(mtx_lock(&oc.server_ch->buf_mtx), thrd_success);
		oc.server_ch->local_window = 0;
		oc.server_ch->window_max = 8;
		ASSERT_EQ(mtx_unlock(&oc.server_ch->buf_mtx), thrd_success);
		dssh_test_thrd_fail_after(fail);
		int ret = maybe_replenish_window(ctx.server, oc.server_ch);
		dssh_test_thrd_reset();
		ASSERT_ERR(ret, DSSH_ERROR_TERMINATED);
		ASSERT_TRUE(dssh_session_is_terminated(ctx.server));
		ASSERT_FALSE(tx_owner_is_current(ctx.server));
		conn_cleanup(&ctx);
	}
	return TEST_PASS;
}

struct unsupported_messages {
	uint32_t seq[3];
	atomic_int count;
};

static void
unsupported_reply_cb(uint32_t seq, void *cbdata)
{
	struct unsupported_messages *seen = cbdata;
	int count = atomic_load(&seen->count);
	if (count < 3) {
		seen->seq[count] = seq;
		atomic_store(&seen->count, count + 1);
	}
}

static int
test_unknown_message_replies(void)
{
	struct conn_ctx ctx;
	if (conn_setup_no_start(&ctx) < 0)
		return TEST_FAIL;
	struct unsupported_messages seen = {0};
	atomic_init(&seen.count, 0);
	ASSERT_OK(dssh_session_set_unimplemented_cb(ctx.client, unsupported_reply_cb, &seen));
	ASSERT_OK(dssh_session_start(ctx.client));
	ASSERT_OK(dssh_session_start(ctx.server));
	uint32_t seq[3];
	for (unsigned i = 0; i < 3; i++) {
		/* i < 3, so this value is in [200, 202]. */
		uint8_t unknown = (uint8_t)(200 + i);
		ASSERT_OK(send_packet(ctx.client, &unknown, 1, &seq[i]));
	}
	ASSERT_TRUE(dssh_test_wait_int(&seen.count, 3));
	for (unsigned i = 0; i < 3; i++)
		ASSERT_EQ_U(seen.seq[i], seq[i]);
	ASSERT_FALSE(dssh_session_is_terminated(ctx.server));
	conn_cleanup(&ctx);
	return TEST_PASS;
}

struct window_rekey_gate {
	dssh_session server;
	atomic_bool entered;
	atomic_bool release;
	atomic_bool expired;
};

/* Published by the observer's atomic function pointer, kept alive through
 * conn_cleanup's demux joins, then unpublished before leaving this frame. */
static struct window_rekey_gate *active_window_rekey_gate;

static void
window_rekey_observe(enum dssh_test_comp_stage stage)
{
	if (stage != DSSH_TEST_COMP_COMPRESS)
		return;
	struct window_rekey_gate *gate = active_window_rekey_gate;
	if (!session_on_demux_thread(gate->server)
	    || !atomic_load(&gate->server->trans.rekey_in_progress)
	    || atomic_exchange(&gate->entered, true))
		return;
	if (!dssh_test_wait_bool(&gate->release, true))
		atomic_store(&gate->expired, true);
}

static int
accept_window_rekey(dssh_channel ch, const struct dssh_chan_params *params,
    struct dssh_chan_accept_result *result, void *cbdata)
{
	(void)ch;
	(void)params;
	(void)cbdata;
	result->max_window = 8;
	return 0;
}

static int
test_compression_window_rekey(void)
{
	const char *method = getenv("DSSH_TEST_COMPRESSION");
	if (method == NULL || strcmp(method, "test-growth") != 0)
		return TEST_SKIP;
	struct conn_ctx ctx;
	if (conn_setup(&ctx) < 0)
		return TEST_FAIL;
	struct dssh_chan_accept_cbs cbs = { .exec = accept_window_rekey };
	struct open_exec_ctx oc = {
		.client = ctx.client, .server = ctx.server, .command = "window-rekey",
		.cbs = &cbs, .accept_timeout = 30000,
	};
	ASSERT_OK(open_exec_channel(&ctx, &oc));
	ASSERT_EQ_U(atomic_load(&oc.client_ch->remote_window), 8);
	unsigned resets_before = compression_fixture_reset_count();
	struct window_rekey_gate gate = { .server = ctx.server };
	atomic_init(&gate.entered, false);
	atomic_init(&gate.release, false);
	atomic_init(&gate.expired, false);
	active_window_rekey_gate = &gate;
	dssh_test_comp_observe(window_rekey_observe);
	/* Arm counting of the next real received packet. The data is delivered
	 * before the following receive initiates rekey and pauses at KEXINIT. */
	uint64_t threshold = ctx.server->trans.enc_c2s_selected->bytes_per_key;
	dssh_test_arm_rx_bytes_before_count(ctx.server, threshold);
	const uint8_t first[] = { 1, 2, 3, 4, 5, 6 };
	bool valid = dssh_chan_write(oc.client_ch, 0, first, sizeof(first)) == 6;
	valid = valid && dssh_test_wait_bool(&gate.entered, true);
	uint8_t received[8] = {0};
	if (valid)
		valid = dssh_chan_read(oc.server_ch, 0, received, sizeof(received)) == 6
		    && memcmp(received, first, sizeof(first)) == 0;
	if (valid) {
		int ret = mtx_lock(&oc.server_ch->buf_mtx);
		if (ret == thrd_success) {
			valid = oc.server_ch->local_window == 2
			    && oc.server_ch->window_adjust_pending == 6
			    && oc.server_ch->window_adjust_inflight == 0
			    && atomic_load(&oc.server_ch->wa_slot.ready);
			if (mtx_unlock(&oc.server_ch->buf_mtx) != thrd_success)
				abort();
		}
		else
			valid = false;
	}
	/* The peer has received no credit while the old compression stream is
	 * paused. Release it, require both new epochs, then observe real credit. */
	valid = valid && atomic_load(&oc.client_ch->remote_window) == 2;
	atomic_store(&gate.release, true);
	valid = valid && compression_fixture_wait_reset(resets_before);
	struct timespec start;
	if (valid)
		valid = clock_gettime(CLOCK_MONOTONIC, &start) == 0;
	while (valid && atomic_load(&oc.client_ch->remote_window) != 8)
		valid = dssh_test_wait_tick(&start);
	const uint8_t second[] = { 21, 22, 23, 24, 25, 26, 27, 28 };
	if (valid)
		valid = dssh_chan_write(oc.client_ch, 0, second, sizeof(second)) == 8;
	if (valid)
		valid = (dssh_chan_poll(oc.server_ch, DSSH_POLL_READ, 5000) & DSSH_POLL_READ) != 0
		    && dssh_chan_read(oc.server_ch, 0, received, sizeof(received)) == 8
		    && memcmp(received, second, sizeof(second)) == 0;
	valid = valid && !atomic_load(&gate.expired)
	    && !dssh_session_is_terminated(ctx.client) && !dssh_session_is_terminated(ctx.server);
	/* Always unblock and join before assertions can leave the gate's frame. */
	conn_cleanup(&ctx);
	dssh_test_comp_observe(NULL);
	active_window_rekey_gate = NULL;
	struct dssh_test_comp_stats stats;
	dssh_test_comp_stats(&stats);
	ASSERT_TRUE(valid);
	ASSERT_EQ_U(stats.live, 0);
	ASSERT_EQ_U(stats.owner_errors, 0);
	return TEST_PASS;
}

#include "event_delivery_cases.h"
#include "event_text_cases.h"
#include "conn_helper_cases.h"
#include "channel_worker_cases.h"
#include "channel_pair_cases.h"
#include "conn_zc_cases.h"
#include "conn_actor_cases.h"
#include "channel_id_cases.h"
#include "peer_channel_id_cases.h"
#include "post_close_cases.h"
#include "half_close_cases.h"
#include "setup_mailbox_cases.h"
#include "accept_failure_cases.h"
#include "request_framing_cases.h"
#include "control_framing_cases.h"
#include "window_credit_cases.h"
#include "queued_control_cases.h"
#include "tx_slot_cases.h"
#include "transmit_rekey_cases.h"
#include "accept_callback_cases.h"
#include "channel_params_cases.h"
#include "event_overflow_cases.h"
#include "close_outcome_cases.h"
#include "close_reservation_cases.h"
#include "termination_callback_cases.h"
#include "channel_control_cases.h"
#include "incoming_control_cases.h"
#include "break_handler_cases.h"
#include "terminal_mode_cases.h"
#include "notification_flag_cases.h"
#include "conn_callback_cases.h"
#include "conn_wire_cases.h"
#include "conn_flow_cases.h"
#include "conn_lifecycle_cases.h"
#include "conn_storage_cases.h"
#include "conn_consume_cases.h"
#include "conn_eof_cases.h"
#include "read_result_cases.h"
#include "stream_body_cases.h"
#include "event_body_cases.h"
#include "gather_full_cases.h"

static struct dssh_test_entry tests[] = {
	{ "event_body/controls", test_event_body_controls },
	{ "stream_body/controls", test_stream_body_controls },
	{ "read_result/deferred", test_read_result_deferred },
	{ "read_result/failures", test_read_result_failures },
	{ "read_result/retry", test_read_result_retry },
	{ "read_result/terminal", test_read_result_terminal },
	{ "read_result/provider", test_read_result_provider },
	{ "read_result/reservations", test_read_result_reservations },
	{ "read_result/poll", test_read_result_poll },
	{ "read_result/concurrent", test_read_result_concurrent },
	{ "gather/full_batch_sha512", test_gather_full_batch },
	{ "release/native_eof_probe", test_native_eof_probe },
	{ "release/native_eof_wire", test_native_eof_wire },
	{ "release/consume_owner_close", test_consume_owner_close },
	{ "release/consume_owner_reservation", test_consume_owner_reservation },
	{ "release/consume_owner_mode", test_consume_owner_mode },
	{ "release/terminal_capture_bounds", test_terminal_capture_bounds },
	{ "release/storage_owner_text", test_storage_owner_text },
	{ "release/storage_owner_params", test_storage_owner_params },
	{ "release/storage_owner_overflow", test_storage_owner_overflow },
	{ "release/storage_text_pattern", test_storage_text_pattern },
	{ "release/lifecycle_owner_ids", test_lifecycle_owner_ids },
	{ "release/lifecycle_owner_peer_ids", test_lifecycle_owner_peer_ids },
	{ "release/lifecycle_owner_half_close", test_lifecycle_owner_half_close },
	{ "release/lifecycle_owner_setup", test_lifecycle_owner_setup },
	{ "release/flow_bytes", test_flow_bytes },
	{ "release/flow_owner_tx", test_flow_owner_tx },
	{ "release/flow_owner_rekey", test_flow_owner_rekey },
	{ "release/flow_owner_window", test_flow_owner_window },
	{ "release/flow_owner_queue", test_flow_owner_queue },

	{ "release/wire_wait_contract", test_wire_wait_contract },
	{ "release/wire_owner_accept_failure", test_wire_owner_accept_failure },
	{ "release/wire_owner_request_frame", test_wire_owner_request_frame },
	{ "release/wire_owner_control_frame", test_wire_owner_control_frame },
	{ "release/wire_owner_accept_callback", test_wire_owner_accept_callback },

	{ "release/callback_incoming_lifetime", test_callback_incoming_lifetime },
	{ "release/callback_capture_bounds", test_callback_capture_bounds },
	{ "release/callback_packet_bounds", test_callback_packet_bounds },
	{ "release/callback_owner_close", test_callback_owner_close },
	{ "release/callback_owner_defaults", test_callback_owner_defaults },
	{ "release/callback_owner_atomic", test_callback_owner_atomic },
	{ "release/callback_owner_lifetime", test_callback_owner_lifetime },
	{ "release/callback_owner_queue", test_callback_owner_queue },
	{ "release/callback_owner_setup", test_callback_owner_setup },
	{ "release/callback_owner_replacement", test_callback_owner_replacement },
	{ "release/callback_owner_incoming", test_callback_owner_incoming },

	{ "release/actor_owner_reject", test_actor_owner_reject },
	{ "release/actor_owner_two", test_actor_owner_two },
	{ "release/actor_owner_accept", test_actor_owner_accept },
	{ "release/actor_owner_independent", test_actor_owner_independent },
	{ "release/actor_owner_concurrent", test_actor_owner_concurrent },
	{ "release/actor_owner_terminate", test_actor_owner_terminate },
	{ "release/actor_owner_drop", test_actor_owner_drop },
	{ "release/actor_owner_failure", test_actor_owner_failure },
	{ "release/actor_owner_blocking", test_actor_owner_blocking },
	{ "release/actor_owner_callback", test_actor_owner_callback },
	{ "release/actor_owner_credit", test_actor_owner_credit },
	{ "release/actor_owner_deadline", test_actor_owner_deadline },

	{ "release/notification_reply_flags", test_notification_reply_flags },
	{ "release/notification_flag_failures", test_notification_flag_failures },
	{ "release/terminal_mode_validation", test_terminal_mode_validation },
	{ "release/terminal_mode_roundtrip", test_terminal_mode_roundtrip },
	{ "release/terminal_mode_wire", test_terminal_mode_wire },
	{ "release/break_handler_outcomes", test_break_handler_outcomes },
	{ "release/break_handler_lifetime", test_break_handler_lifetime },
	{ "release/break_handler_failures", test_break_handler_failures },
	{ "release/break_handler_positions", test_break_handler_positions },
	{ "release/break_handler_queue", test_break_handler_queue },
	{ "release/break_handler_setup", test_break_handler_setup },
	{ "release/close_disposal_values", test_close_disposal_values },
	{ "release/close_disposal_send", test_close_disposal_send },
	{ "release/close_disposal_retirement", test_close_disposal_retirement },
	{ "release/close_reservation", test_close_reservation },
	{ "release/close_termination_callback", test_close_termination_callback },
	{ "release/control_wire", test_channel_control_wire },
	{ "release/control_after_eof", test_channel_control_after_eof },
	{ "release/control_refusal", test_channel_control_refusal },
	{ "release/control_failure", test_channel_control_failure },
	{ "release/half_close_retry", test_half_close_retry },
	{ "release/half_close_concurrent", test_half_close_concurrent },
	{ "release/half_close_failure", test_half_close_failure },
	{ "release/half_close_publication", test_half_close_publication },
	{ "release/tx_slot_admission", test_tx_slot_admission },
	{ "release/tx_slot_resize", test_tx_slot_resize },
	{ "release/tx_slot_refusal", test_tx_slot_refusal },
	{ "release/transmit_rekey_threshold", test_transmit_rekey_threshold },
	{ "release/transmit_rekey_inflight", test_transmit_rekey_inflight },
	{ "release/transmit_rekey_simultaneous", test_transmit_rekey_simultaneous },
	{ "release/transmit_rekey_failure", test_transmit_rekey_failure },
	{ "release/channel_id_lifetime", test_channel_id_lifetime },
	{ "release/channel_id_late_reply", test_channel_id_late_reply },
	{ "release/channel_id_backpressure", test_channel_id_backpressure },
	{ "release/peer_id_queued", test_peer_id_queued },
	{ "release/peer_id_popped", test_peer_id_popped },
	{ "release/peer_id_confirmed", test_peer_id_confirmed },
	{ "release/peer_id_late_confirmation", test_peer_id_late_confirmation },
	{ "release/peer_id_concurrent_accept", test_peer_id_concurrent_accept },
	{ "release/peer_id_ordered_terminal", test_peer_id_ordered_terminal },
	{ "release/post_close_direct", test_post_close_direct },
	{ "release/post_close_queued", test_post_close_queued },
	{ "release/post_close_buffered", test_post_close_buffered },
	{ "release/channel_worker_ownership", test_channel_worker_ownership },
	{ "release/zc_fixture_bounds", test_zc_fixture_bounds },
	{ "release/zc_owner_accept", test_zc_owner_accept },
	{ "release/zc_owner_client", test_zc_owner_client },
	{ "release/zc_owner_cancel", test_zc_owner_cancel },
	{ "release/zc_owner_close", test_zc_owner_close },
	{ "release/pair_chan_get_pty", pair_chan_get_pty },
	{ "release/pair_channel_close_subsys", pair_channel_close_subsys },
	{ "release/pair_env_callback_rejection", pair_env_callback_rejection },
	{ "release/pair_env_roundtrip", pair_env_roundtrip },
	{ "release/pair_open_shell", pair_open_shell },
	{ "release/pair_open_subsystem", pair_open_subsystem },
	{ "release/pair_subsys_large_write", pair_subsys_large_write },
	{ "release/pair_subsys_multiple_writes", pair_subsys_multiple_writes },
	{ "release/pair_subsys_poll", pair_subsys_poll },
	{ "release/pair_subsys_poll_timeout_zero", pair_subsys_poll_timeout_zero },
	{ "release/pair_subsys_poll_write", pair_subsys_poll_write },
	{ "release/pair_subsys_write_after_close", pair_subsys_write_after_close },
	{ "release/pair_subsys_write_read", pair_subsys_write_read },
	{ "release/pair_subsys_write_zero", pair_subsys_write_zero },
	{ "release/pair_subsystem_roundtrip", pair_subsystem_roundtrip },
	{ "release/pair_window_change", pair_window_change },
	{ "release/conn_setup_ownership", test_conn_setup_ownership },
	{ "release/conn_no_start_ownership", test_conn_no_start_ownership },
	{ "queue_retry/packet_stream_callback", test_queue_retry_packet_stream_callback },
	{ "queue_retry/gather_stream_callback", test_queue_retry_gather_stream_callback },
	{ "queue_retry/packet_ordinary", test_queue_retry_packet_ordinary },
	{ "queue_retry/packet_key_refusal", test_queue_retry_packet_key_refusal },
	{ "queue_retry/packet_size_refusal", test_queue_retry_packet_size_refusal },
	{ "queue_retry/packet_key_partial", test_queue_retry_packet_key_partial },
	{ "queue_retry/packet_size_partial", test_queue_retry_packet_size_partial },
	{ "queue_retry/packet_phase_pause", test_queue_retry_packet_phase_pause },
	{ "queue_retry/packet_concurrent_key", test_queue_retry_packet_concurrent_key },
	{ "queue_retry/packet_channel_key_partial", test_queue_retry_packet_channel_key_partial },
	{ "queue_retry/packet_window_size", test_queue_retry_packet_window_size },
	{ "queue_retry/packet_concurrent_size", test_queue_retry_packet_concurrent_size },
	{ "queue_retry/packet_key_rekey", test_queue_retry_packet_key_rekey },
	{ "queue_retry/packet_size_rekey", test_queue_retry_packet_size_rekey },
	{ "queue_retry/packet_key_probe_send", test_queue_retry_packet_key_probe_send },
	{ "queue_retry/packet_key_probe_write", test_queue_retry_packet_key_probe_write },
	{ "queue_retry/packet_key_probe_slot", test_queue_retry_packet_key_probe_slot },
	{ "queue_retry/packet_key_probe_rekey", test_queue_retry_packet_key_probe_rekey },
	{ "queue_retry/packet_size_probe_send", test_queue_retry_packet_size_probe_send },
	{ "queue_retry/packet_size_probe_write", test_queue_retry_packet_size_probe_write },
	{ "queue_retry/packet_size_probe_slot", test_queue_retry_packet_size_probe_slot },
	{ "queue_retry/packet_size_probe_rekey", test_queue_retry_packet_size_probe_rekey },
	{ "queue_retry/gather_ordinary", test_queue_retry_gather_ordinary },
	{ "queue_retry/gather_key_refusal", test_queue_retry_gather_key_refusal },
	{ "queue_retry/gather_size_refusal", test_queue_retry_gather_size_refusal },
	{ "queue_retry/gather_key_partial", test_queue_retry_gather_key_partial },
	{ "queue_retry/gather_size_partial", test_queue_retry_gather_size_partial },
	{ "queue_retry/gather_phase_pause", test_queue_retry_gather_phase_pause },
	{ "queue_retry/gather_concurrent_key", test_queue_retry_gather_concurrent_key },
	{ "queue_retry/gather_channel_key_partial", test_queue_retry_gather_channel_key_partial },
	{ "queue_retry/gather_window_size", test_queue_retry_gather_window_size },
	{ "queue_retry/gather_concurrent_size", test_queue_retry_gather_concurrent_size },
	{ "queue_retry/gather_key_rekey", test_queue_retry_gather_key_rekey },
	{ "queue_retry/gather_size_rekey", test_queue_retry_gather_size_rekey },
	{ "queue_retry/gather_key_probe_send", test_queue_retry_gather_key_probe_send },
	{ "queue_retry/gather_key_probe_write", test_queue_retry_gather_key_probe_write },
	{ "queue_retry/gather_key_probe_slot", test_queue_retry_gather_key_probe_slot },
	{ "queue_retry/gather_key_probe_rekey", test_queue_retry_gather_key_probe_rekey },
	{ "queue_retry/gather_size_probe_send", test_queue_retry_gather_size_probe_send },
	{ "queue_retry/gather_size_probe_write", test_queue_retry_gather_size_probe_write },
	{ "queue_retry/gather_size_probe_slot", test_queue_retry_gather_size_probe_slot },
	{ "queue_retry/gather_size_probe_rekey", test_queue_retry_gather_size_probe_rekey },


	{ "window_credit/packet_0_stream_small", test_window_credit_packet_0_stream_small },
	{ "window_credit/packet_0_stream_full", test_window_credit_packet_0_stream_full },
	{ "window_credit/packet_0_zc_small", test_window_credit_packet_0_zc_small },
	{ "window_credit/packet_0_zc_full", test_window_credit_packet_0_zc_full },
	{ "window_credit/packet_0_cancel", test_window_credit_packet_0_cancel },
	{ "window_credit/packet_0_oversize", test_window_credit_packet_0_oversize },
	{ "window_credit/packet_0_key_limit", test_window_credit_packet_0_key_limit },
	{ "window_credit/packet_0_io_failure", test_window_credit_packet_0_io_failure },
	{ "window_credit/packet_1_stream_small", test_window_credit_packet_1_stream_small },
	{ "window_credit/packet_1_stream_full", test_window_credit_packet_1_stream_full },
	{ "window_credit/packet_1_zc_small", test_window_credit_packet_1_zc_small },
	{ "window_credit/packet_1_zc_full", test_window_credit_packet_1_zc_full },
	{ "window_credit/packet_1_cancel", test_window_credit_packet_1_cancel },
	{ "window_credit/packet_1_oversize", test_window_credit_packet_1_oversize },
	{ "window_credit/packet_1_key_limit", test_window_credit_packet_1_key_limit },
	{ "window_credit/packet_1_io_failure", test_window_credit_packet_1_io_failure },
	{ "window_credit/gather_0_stream_small", test_window_credit_gather_0_stream_small },
	{ "window_credit/gather_0_stream_full", test_window_credit_gather_0_stream_full },
	{ "window_credit/gather_0_zc_small", test_window_credit_gather_0_zc_small },
	{ "window_credit/gather_0_zc_full", test_window_credit_gather_0_zc_full },
	{ "window_credit/gather_0_cancel", test_window_credit_gather_0_cancel },
	{ "window_credit/gather_0_oversize", test_window_credit_gather_0_oversize },
	{ "window_credit/gather_0_key_limit", test_window_credit_gather_0_key_limit },
	{ "window_credit/gather_0_io_failure", test_window_credit_gather_0_io_failure },
	{ "window_credit/gather_1_stream_small", test_window_credit_gather_1_stream_small },
	{ "window_credit/gather_1_stream_full", test_window_credit_gather_1_stream_full },
	{ "window_credit/gather_1_zc_small", test_window_credit_gather_1_zc_small },
	{ "window_credit/gather_1_zc_full", test_window_credit_gather_1_zc_full },
	{ "window_credit/gather_1_cancel", test_window_credit_gather_1_cancel },
	{ "window_credit/gather_1_oversize", test_window_credit_gather_1_oversize },
	{ "window_credit/gather_1_key_limit", test_window_credit_gather_1_key_limit },
	{ "window_credit/gather_1_io_failure", test_window_credit_gather_1_io_failure },
	{ "window_credit/reverse_packet_0_stream_small", test_window_credit_reverse_packet_0_stream_small },
	{ "window_credit/reverse_packet_0_stream_full", test_window_credit_reverse_packet_0_stream_full },
	{ "window_credit/reverse_packet_0_zc_small", test_window_credit_reverse_packet_0_zc_small },
	{ "window_credit/reverse_packet_0_zc_full", test_window_credit_reverse_packet_0_zc_full },
	{ "window_credit/reverse_packet_0_cancel", test_window_credit_reverse_packet_0_cancel },
	{ "window_credit/reverse_packet_0_oversize", test_window_credit_reverse_packet_0_oversize },
	{ "window_credit/reverse_packet_0_key_limit", test_window_credit_reverse_packet_0_key_limit },
	{ "window_credit/reverse_packet_0_io_failure", test_window_credit_reverse_packet_0_io_failure },
	{ "window_credit/reverse_packet_1_stream_small", test_window_credit_reverse_packet_1_stream_small },
	{ "window_credit/reverse_packet_1_stream_full", test_window_credit_reverse_packet_1_stream_full },
	{ "window_credit/reverse_packet_1_zc_small", test_window_credit_reverse_packet_1_zc_small },
	{ "window_credit/reverse_packet_1_zc_full", test_window_credit_reverse_packet_1_zc_full },
	{ "window_credit/reverse_packet_1_cancel", test_window_credit_reverse_packet_1_cancel },
	{ "window_credit/reverse_packet_1_oversize", test_window_credit_reverse_packet_1_oversize },
	{ "window_credit/reverse_packet_1_key_limit", test_window_credit_reverse_packet_1_key_limit },
	{ "window_credit/reverse_packet_1_io_failure", test_window_credit_reverse_packet_1_io_failure },
	{ "window_credit/reverse_gather_0_stream_small", test_window_credit_reverse_gather_0_stream_small },
	{ "window_credit/reverse_gather_0_stream_full", test_window_credit_reverse_gather_0_stream_full },
	{ "window_credit/reverse_gather_0_zc_small", test_window_credit_reverse_gather_0_zc_small },
	{ "window_credit/reverse_gather_0_zc_full", test_window_credit_reverse_gather_0_zc_full },
	{ "window_credit/reverse_gather_0_cancel", test_window_credit_reverse_gather_0_cancel },
	{ "window_credit/reverse_gather_0_oversize", test_window_credit_reverse_gather_0_oversize },
	{ "window_credit/reverse_gather_0_key_limit", test_window_credit_reverse_gather_0_key_limit },
	{ "window_credit/reverse_gather_0_io_failure", test_window_credit_reverse_gather_0_io_failure },
	{ "window_credit/reverse_gather_1_stream_small", test_window_credit_reverse_gather_1_stream_small },
	{ "window_credit/reverse_gather_1_stream_full", test_window_credit_reverse_gather_1_stream_full },
	{ "window_credit/reverse_gather_1_zc_small", test_window_credit_reverse_gather_1_zc_small },
	{ "window_credit/reverse_gather_1_zc_full", test_window_credit_reverse_gather_1_zc_full },
	{ "window_credit/reverse_gather_1_cancel", test_window_credit_reverse_gather_1_cancel },
	{ "window_credit/reverse_gather_1_oversize", test_window_credit_reverse_gather_1_oversize },
	{ "window_credit/reverse_gather_1_key_limit", test_window_credit_reverse_gather_1_key_limit },
	{ "window_credit/reverse_gather_1_io_failure", test_window_credit_reverse_gather_1_io_failure },

	{ "window_credit/arithmetic", test_window_credit_arithmetic },
	{ "window_credit/atomic", test_window_credit_atomic },

	{ "control_frame/wire_normal_window", test_control_frame_wire_normal_window },
	{ "control_frame/wire_normal_eof", test_control_frame_wire_normal_eof },
	{ "control_frame/wire_normal_close", test_control_frame_wire_normal_close },
	{ "control_frame/wire_normal_success", test_control_frame_wire_normal_success },
	{ "control_frame/wire_normal_failure", test_control_frame_wire_normal_failure },
	{ "control_frame/wire_setup_window", test_control_frame_wire_setup_window },
	{ "control_frame/wire_setup_eof", test_control_frame_wire_setup_eof },
	{ "control_frame/wire_setup_close", test_control_frame_wire_setup_close },
	{ "control_frame/wire_setup_success", test_control_frame_wire_setup_success },
	{ "control_frame/wire_setup_failure", test_control_frame_wire_setup_failure },
	{ "control_frame/wire_pending_success", test_control_frame_wire_pending_success },
	{ "control_frame/wire_pending_failure", test_control_frame_wire_pending_failure },

	{ "control_frame/stream_normal_window", test_control_frame_stream_normal_window },
	{ "control_frame/stream_normal_eof", test_control_frame_stream_normal_eof },
	{ "control_frame/stream_normal_close", test_control_frame_stream_normal_close },
	{ "control_frame/stream_normal_success", test_control_frame_stream_normal_success },
	{ "control_frame/stream_normal_failure", test_control_frame_stream_normal_failure },
	{ "control_frame/stream_normal_valid", test_control_frame_stream_normal_valid },
	{ "control_frame/stream_setup_window", test_control_frame_stream_setup_window },
	{ "control_frame/stream_setup_eof", test_control_frame_stream_setup_eof },
	{ "control_frame/stream_setup_close", test_control_frame_stream_setup_close },
	{ "control_frame/stream_setup_success", test_control_frame_stream_setup_success },
	{ "control_frame/stream_setup_failure", test_control_frame_stream_setup_failure },
	{ "control_frame/stream_setup_valid", test_control_frame_stream_setup_valid },
	{ "control_frame/zc_normal_window", test_control_frame_zc_normal_window },
	{ "control_frame/zc_normal_eof", test_control_frame_zc_normal_eof },
	{ "control_frame/zc_normal_close", test_control_frame_zc_normal_close },
	{ "control_frame/zc_normal_success", test_control_frame_zc_normal_success },
	{ "control_frame/zc_normal_failure", test_control_frame_zc_normal_failure },
	{ "control_frame/zc_normal_valid", test_control_frame_zc_normal_valid },
	{ "control_frame/zc_setup_window", test_control_frame_zc_setup_window },
	{ "control_frame/zc_setup_eof", test_control_frame_zc_setup_eof },
	{ "control_frame/zc_setup_close", test_control_frame_zc_setup_close },
	{ "control_frame/zc_setup_success", test_control_frame_zc_setup_success },
	{ "control_frame/zc_setup_failure", test_control_frame_zc_setup_failure },
	{ "control_frame/zc_setup_valid", test_control_frame_zc_setup_valid },
	{ "control_frame/unknown", test_control_frame_unknown },
	{ "control_frame/stale", test_control_frame_stale },

	{ "accept_callback/publish_before_return", test_accept_callback_publish_before_return },
	{ "accept_callback/terminate_deferred", test_accept_callback_terminate_deferred },
	{ "accept_callback/stop_deferred", test_accept_callback_stop_deferred },
	{ "accept_callback/cleanup_deferred", test_accept_callback_cleanup_deferred },

	{ "accept_callback/pty_silent_positive", test_accept_callback_pty_silent_positive },
	{ "accept_callback/pty_silent_negative", test_accept_callback_pty_silent_negative },
	{ "accept_callback/pty_reply_positive", test_accept_callback_pty_reply_positive },
	{ "accept_callback/pty_reply_negative", test_accept_callback_pty_reply_negative },
	{ "accept_callback/env_silent_positive", test_accept_callback_env_silent_positive },
	{ "accept_callback/env_silent_negative", test_accept_callback_env_silent_negative },
	{ "accept_callback/env_reply_positive", test_accept_callback_env_reply_positive },
	{ "accept_callback/env_reply_negative", test_accept_callback_env_reply_negative },
	{ "accept_callback/shell_silent_positive", test_accept_callback_shell_silent_positive },
	{ "accept_callback/shell_silent_negative", test_accept_callback_shell_silent_negative },
	{ "accept_callback/shell_reply_positive", test_accept_callback_shell_reply_positive },
	{ "accept_callback/shell_reply_negative", test_accept_callback_shell_reply_negative },
	{ "accept_callback/exec_silent_positive", test_accept_callback_exec_silent_positive },
	{ "accept_callback/exec_silent_negative", test_accept_callback_exec_silent_negative },
	{ "accept_callback/exec_reply_positive", test_accept_callback_exec_reply_positive },
	{ "accept_callback/exec_reply_negative", test_accept_callback_exec_reply_negative },
	{ "accept_callback/subsystem_silent_positive", test_accept_callback_subsystem_silent_positive },
	{ "accept_callback/subsystem_silent_negative", test_accept_callback_subsystem_silent_negative },
	{ "accept_callback/subsystem_reply_positive", test_accept_callback_subsystem_reply_positive },
	{ "accept_callback/subsystem_reply_negative", test_accept_callback_subsystem_reply_negative },
	{ "accept_callback/pty_request_only", test_accept_callback_pty_request_only },
	{ "accept_callback/env_request_only", test_accept_callback_env_request_only },

	{ "framing/name_byte_and_boolean", test_request_name_byte_and_boolean },
	{ "framing/setup_short_length", test_request_setup_short_length },
	{ "framing/setup_short_name", test_request_setup_short_name },
	{ "framing/setup_short_reply", test_request_setup_short_reply },
	{ "framing/setup_empty", test_request_setup_empty },
	{ "framing/setup_nonascii", test_request_setup_nonascii },
	{ "framing/setup_zero", test_request_setup_zero },
	{ "framing/setup_comma", test_request_setup_comma },
	{ "framing/setup_long", test_request_setup_long },
	{ "framing/normal_short_length", test_request_normal_short_length },
	{ "framing/normal_short_name", test_request_normal_short_name },
	{ "framing/normal_short_reply", test_request_normal_short_reply },
	{ "framing/normal_empty", test_request_normal_empty },
	{ "framing/normal_nonascii", test_request_normal_nonascii },
	{ "framing/normal_zero", test_request_normal_zero },
	{ "framing/normal_comma", test_request_normal_comma },
	{ "framing/normal_long", test_request_normal_long },
	{ "framing/break_short", test_request_break_short },
	{ "framing/break_trailer", test_request_break_trailer },
	{ "framing/shell_reply", test_request_shell_reply },
	{ "framing/shell_no_reply", test_request_shell_no_reply },
	{ "framing/setup_unknown", test_request_setup_unknown },
	{ "framing/normal_unknown", test_request_normal_unknown },
	{ "framing/event_lengths_stream", test_request_event_lengths_stream },
	{ "framing/event_lengths_zc", test_request_event_lengths_zc },
	{ "params/stream_sync", test_params_stream_sync },
	{ "params/zc_sync", test_params_zc_sync },
	{ "params/stream_validation", test_params_stream_validation },
	{ "params/zc_validation", test_params_zc_validation },
	{ "params/stream_snapshot", test_params_stream_snapshot },
	{ "params/stream_term", test_params_stream_term },
	{ "params/stream_command", test_params_stream_command },
	{ "params/stream_subsystem", test_params_stream_subsystem },
	{ "params/stream_modes", test_params_stream_modes },
	{ "params/stream_env_array", test_params_stream_env_array },
	{ "params/stream_env_name1", test_params_stream_env_name1 },
	{ "params/stream_env_value1", test_params_stream_env_value1 },
	{ "params/stream_env_name2", test_params_stream_env_name2 },
	{ "params/stream_env_value2", test_params_stream_env_value2 },
	{ "params/zc_snapshot", test_params_zc_snapshot },
	{ "params/zc_term", test_params_zc_term },
	{ "params/zc_command", test_params_zc_command },
	{ "params/zc_subsystem", test_params_zc_subsystem },
	{ "params/zc_modes", test_params_zc_modes },
	{ "params/zc_env_array", test_params_zc_env_array },
	{ "params/zc_env_name1", test_params_zc_env_name1 },
	{ "params/zc_env_value1", test_params_zc_env_value1 },
	{ "params/zc_env_name2", test_params_zc_env_name2 },
	{ "params/zc_env_value2", test_params_zc_env_value2 },
	{ "event/overflow_request_notification", test_overflow_request_notification },
	{ "event/overflow_eof_notification", test_overflow_eof_notification },
	{ "event/overflow_close_notification", test_overflow_close_notification },
	{ "event/overflow_request_close_failure", test_overflow_request_close_failure },
	{ "event/overflow_eof_close_failure", test_overflow_eof_close_failure },
	{ "event/overflow_close_close_failure", test_overflow_close_close_failure },
	{ "event/overflow_request_normal", test_overflow_request_normal },
	{ "event/overflow_eof_normal", test_overflow_eof_normal },
	{ "event/overflow_close_normal", test_overflow_close_normal },
	{ "accept_failure/notify_failure", test_accept_notify_failure },
	{ "accept_failure/success_send_failure", test_accept_success_send_failure },
	{ "accept_failure/window_send_failure", test_accept_window_send_failure },
	{ "accept_failure/close_deadline", test_accept_close_deadline },
	{ "accept_failure/channel_storage", test_accept_channel_storage },
	{ "accept_failure/event_storage", test_accept_event_storage },
	{ "accept_failure/param_storage", test_accept_param_storage },
	{ "accept_failure/stream_first", test_accept_stream_first },
	{ "accept_failure/stream_second", test_accept_stream_second },
	{ "accept_failure/exec_reject", test_accept_exec_reject },
	{ "accept_failure/exec_no_reply", test_accept_exec_no_reply },
	{ "accept_failure/peer_close", test_accept_peer_close },
	{ "accept_failure/shell_reject", test_accept_shell_reject },
	{ "accept_failure/subsystem_reject", test_accept_subsystem_reject },
	{ "accept_failure/buf_init", test_accept_buf_init },
	{ "accept_failure/cb_init", test_accept_cb_init },
	{ "accept_failure/cnd_init", test_accept_cnd_init },
	{ "accept_failure/slot_first", test_accept_slot_first },
	{ "accept_failure/slot_second", test_accept_slot_second },
	{ "accept_failure/stream_first_no_reply", test_accept_stream_first_no_reply },
	{ "accept_failure/stream_second_no_reply", test_accept_stream_second_no_reply },
	{ "setup/packet_bounds", test_setup_packet_bounds },
	{ "setup/confirmation_cleanup", test_setup_confirmation_cleanup },
	{ "setup/mailbox_no_allocation", test_setup_mailbox_no_allocation },
	{ "setup/gap_disable", test_setup_gap_disable },
	{ "setup/gap_detach", test_setup_gap_detach },
	{ "setup/gap_terminate", test_setup_gap_terminate },
	{ "setup/gap_reacquire", test_setup_gap_reacquire },
	{ "setup/mailbox_reuse", test_setup_mailbox_reuse },
	{ "setup/mailbox_growth", test_setup_mailbox_growth },
	{ "setup/mailbox_rollback", test_setup_mailbox_rollback },
	{ "setup/mailbox_bound", test_setup_mailbox_bound },
	{ "setup/terminal_storage", test_setup_terminal_storage },
	{ "setup/terminal_first_failure", test_setup_terminal_first_failure },
	{ "setup/terminal_second_failure", test_setup_terminal_second_failure },
	{ "event/text_poll_reuse", test_event_text_poll_reuse },
	{ "event/text_free_observer", test_event_text_free_observer },
	{ "event/text_growth_unlocked", test_event_text_growth_unlocked },
	{ "event/text_poll_race", test_event_text_poll_race },
	{ "event/text_binary_callback", test_event_text_binary_callback },
	{ "event/text_binary_poll", test_event_text_binary_poll },
	{ "event/text_empty_callback", test_event_text_empty_callback },
	{ "event/text_empty_poll", test_event_text_empty_poll },
	{ "event/text_packet_boundary", test_event_text_packet_boundary },
	{ "event/delivery_atomic_state", test_event_atomic_state },
	{ "event/delivery_self_disable", test_event_self_disable },
	{ "event/delivery_client_defaults", test_event_client_defaults },
	{ "event/delivery_lengths", test_event_lengths },
	{ "event/delivery_positions", test_event_positions },
	{ "event/delivery_reacquire", test_event_reacquire },
	{ "event/delivery_acquire", test_event_acquire },
	{ "event/delivery_queue_allocation", test_event_queue_allocation },
	{ "event/delivery_malformed", test_event_malformed },
	{ "event/delivery_close_race", test_event_close_race },
	{ "event/delivery_peer_stream", test_event_peer_stream },
	{ "event/delivery_peer_zc", test_event_peer_zc },
	{ "event/delivery_stress", test_event_stress },
	{ "event/delivery_switch", test_event_switch },
	{ "event/delivery_replacement", test_event_replacement },
	{ "event/delivery_lifecycle", test_event_lifecycle },
	{ "compression/window_rekey", test_compression_window_rekey },
	{ "test_assertion_cleanup", test_assertion_cleanup },
	/* Parse helpers */

	/* Session start/stop */
	{ "data/extended_stream_types", test_extended_stream_types },
	{ "data/extended_zc_types", test_extended_zc_types },
	{ "data/framing", test_data_framing },
	{ "data/replenish_sync", test_replenish_sync_failures },
	{ "control/unknown_replies", test_unknown_message_replies },
	{ "clock/deadline", test_clock_deadline },
	{ "clock/poll_accept", test_clock_poll_accept },
	{ "clock/pending_waits", test_clock_pending_waits },
	{ "clock/receive", test_clock_receive },
	{ "lifecycle/join_once", test_lifecycle_join_once },
	{ "lifecycle/demux_callback", test_lifecycle_demux_callback },
	{ "lifecycle/receive_callback", test_lifecycle_receive_callback },
	{ "lifecycle/recursive_callbacks", test_lifecycle_recursive_callbacks },
	{ "lifecycle/tx_reservation", test_lifecycle_tx_reservation },
	{ "test_session_start",              test_session_start },
	{ "test_session_stop",               test_session_stop },
	{ "test_session_start_stop",         test_session_start_stop },
	{ "test_session_stop_open_channel",  test_session_stop_open_channel },

	/* Channel open */
	{ "test_open_exec",                  test_open_exec },
	{ "open/before_start",                test_channel_before_start },
	{ "open/type_and_queue_policy",       test_open_type_and_queue_policy },
	{ "open/response_state",              test_open_response_state },
	{ "open/failure_roundtrip",           test_open_failure_roundtrip },
	{ "open/sync_failures",               test_open_sync_failures },
	{ "io/validation",                    test_channel_io_validation },
	{ "io/request_response_state",        test_request_response_state },
	{ "io/sync_failures",                 test_request_io_sync_failures },
	{ "io/tx_sync_sweep",                 test_request_tx_sync_sweep },
	{ "io/registration_failures",         test_channel_registration_failures },
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
	{ "test_read_nomore_vs_eof",         test_read_nomore_vs_eof },
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
	{ "test_replenish_queued_accounting",  test_replenish_queued_accounting },
	{ "test_zc_consumption_clamped",        test_zc_consumption_clamped },
	{ "test_window_underflow_to_zero",     test_demux_window_underflow_to_zero },
	{ "test_window_add_overflow",          test_window_add_overflow },

	/* Write guard coverage */
	{ "test_chan_write_not_open",           test_chan_write_not_open },
	{ "test_chan_write_close_recv",         test_chan_write_close_received },

	{ "test_subsystem_roundtrip",          test_subsystem_roundtrip },
	{ "test_accept_zc",                    test_accept_zc },
	{ "test_close_during_zc_callback",     test_close_during_zc_callback },
	{ "test_dispatch_sync_failures",       test_dispatch_sync_failures },
	{ "test_detach_sync_failures",         test_detach_sync_failures },

	/* Deterministic branch coverage -- profiling-unstable */
	{ "test_session_poll_nsec_overflow",   test_session_poll_nsec_overflow },
	{ "test_accept_nsec_overflow",         test_accept_nsec_overflow },

	/* Item 74: bytebuf truncation + window accounting */
	{ "test_demux_data_truncation",        test_demux_data_truncation_window },
	{ "test_replenish_caps_buf_space",     test_replenish_caps_to_buffer_space },
	{ "test_replenish_reserves_outstanding", test_replenish_reserves_outstanding_space },

	/* Universal rx-window truncation + ACCEPT_EARLY_DATA bypass */
	{ "rx_truncation/default",             test_rx_truncation_default },
	{ "rx_truncation/clips_to_window",     test_rx_truncation_clips_to_window },
	{ "rx_bypass/pre_setup",               test_rx_bypass_pre_setup },
	{ "rx_bypass/disengages_post_setup",   test_rx_bypass_disengages_post_setup },
	{ "early_data/type_lock_subsystem",    test_chan_open_type_lock_subsystem },
	{ "poll/terminate_surfaces_data",      test_poll_terminate_surfaces_data_flags },
	{ "poll/terminate_no_event_bit",       test_poll_terminate_no_event_bit },

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

	/* Zero-copy client-side open */
	{ "zc/open_client",                    test_zc_open_client },
	{ "zc/cancel",                         test_zc_cancel },
	{ "null/zc_open",                      test_zc_open_null },

	/* Event callback coverage */
	{ "event/session_set_cb",              test_session_set_event_cb },
	{ "event/session_set_cb_toolate",      test_session_set_event_cb_toolate },
	{ "event/chan_set_cb",                 test_chan_set_event_cb },

	/* Event queue limits */
	{ "event/session_set_max",             test_session_set_max_events },
	{ "event/session_set_max_toolate",     test_session_set_max_events_toolate },
	{ "event/chan_set_max",                test_chan_set_max_events },

	/* PTY getter and env roundtrip */
	{ "pty/get_pty",                       test_chan_get_pty },
	{ "env/roundtrip",                     test_env_roundtrip },
	{ "env/callback_rejection",            test_env_callback_rejection },
};

void
dssh_test_failure_cleanup(void)
{
	dssh_test_clock_reset();
	if (g_active_ctx != NULL)
		conn_cleanup(g_active_ctx);
}

#define main dssh_test_conn_main
DSSH_TEST_MAIN(tests)
#undef main

int
main(int argc, char *argv[])
{
	if (argc == 2 && strcmp(argv[1], "--audit-incoming-controls") == 0)
		return incoming_control_probe();
	if (argc == 2 && strcmp(argv[1], "--audit-close-outcomes") == 0)
		return close_outcome_probe();
	if (argc == 4 && strcmp(argv[1], "--channel-fatal") == 0
	    && strncmp(argv[2], "consume-", 8) == 0)
		return consume_fatal_child(argv[2] + 8, argv[3]);
	if (argc == 4 && strcmp(argv[1], "--channel-fatal") == 0
	    && strncmp(argv[2], "storage-", 8) == 0)
		return storage_fatal_child(argv[2] + 8, argv[3]);
	if (argc == 4 && strcmp(argv[1], "--channel-fatal") == 0
	    && strncmp(argv[2], "lifecycle-", 10) == 0)
		return lifecycle_fatal_child(argv[2] + 10, argv[3]);
	if (argc == 4 && strcmp(argv[1], "--channel-fatal") == 0
	    && strncmp(argv[2], "flow-", 5) == 0)
		return flow_fatal_child(argv[2] + 5, argv[3]);
	if (argc == 4 && strcmp(argv[1], "--channel-fatal") == 0
	    && strncmp(argv[2], "wire-", 5) == 0)
		return wire_fatal_child(argv[2] + 5, argv[3]);
	if (argc == 4 && strcmp(argv[1], "--channel-fatal") == 0
	    && strncmp(argv[2], "callback-", 9) == 0)
		return callback_fatal_child(argv[2] + 9, argv[3]);
	if (argc == 4 && strcmp(argv[1], "--channel-fatal") == 0
	    && strncmp(argv[2], "actor-", 6) == 0)
		return conn_actor_fatal_child(argv[2] + 6, argv[3]);
	if (argc == 4 && strcmp(argv[1], "--channel-fatal") == 0
	    && strncmp(argv[2], "zc-", 3) == 0)
		return conn_zc_fatal_child(argv[2] + 3, argv[3]);
	if (argc == 4 && strcmp(argv[1], "--channel-fatal") == 0
	    && strncmp(argv[2], "pair-", 5) == 0)
		return channel_pair_fatal_child(argv[2] + 5, argv[3]);
	if (argc == 4 && strcmp(argv[1], "--channel-fatal") == 0)
		return channel_worker_fatal_child(argv[2], argv[3]);
	if (argc == 3 && strcmp(argv[1], "--join-fatal") == 0)
		return run_join_fatal_child(argv[2]);
	return dssh_test_conn_main(argc, argv);
}
