/*
 * test_thread_errors.c -- C11 threading failure injection tests.
 *
 * Recoverable lock/condition failures use the independent thread countdown.
 * Unrecoverable owned-mutex release failures use provenance-checked fresh-exec
 * fatal tests, including retained ownership and error after a real release.
 */

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
#include "dssh_test_ossl.h"
#include "dssh_test_alloc.h"
#include "mock_io.h"
#include "test_dhgex_provider.h"

/* ================================================================
 * Mock I/O dispatch
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
		return mock_rxline_client(buf, bufsz, bytes_received,
		    sess, cbdata);
	else
		return mock_rxline_server(buf, bufsz, bytes_received,
		    sess, cbdata);
}

/* ================================================================
 * Algorithm registration + server helpers
 * ================================================================ */

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
 * Handshake helpers
 * ================================================================ */

struct handshake_ctx {
	struct mock_io_state io;
	bool io_initialized;
	dssh_session client;
	dssh_session server;
	int client_result;
	int server_result;
};

static struct handshake_ctx *active_handshake;
static int (*fixture_thread_create)(thrd_t *, thrd_start_t, void *) = thrd_create;

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
	if (register_all_algorithms() < 0 || test_generate_host_key() < 0)
		return -1;
	ctx->io_initialized = true;
	if (mock_io_init(&ctx->io, 0) < 0)
		return -1;
	if (dssh_transport_set_callbacks(mock_tx_dispatch, mock_rx_dispatch,
	    mock_rxline_dispatch, mock_extra_line_cb) != 0)
		return -1;
	ctx->client = dssh_session_init(true, 0);
	if (ctx->client == NULL)
		return -1;
	if (dssh_session_set_cbdata(ctx->client, &ctx->io, &ctx->io, &ctx->io, &ctx->io) != 0
	    || dssh_session_set_hostkey_verify_cb(ctx->client, dssh_test_accept_hostkey, NULL) != 0)
		return -1;
	ctx->server = dssh_session_init(false, 0);
	if (ctx->server == NULL)
		return -1;
	if (dssh_session_set_cbdata(ctx->server, &ctx->io, &ctx->io, &ctx->io, &ctx->io) != 0)
		return -1;

	thrd_t client, server;
	if (fixture_thread_create(&client, handshake_client_thread, ctx) != thrd_success)
		return -1;
	if (fixture_thread_create(&server, handshake_server_thread, ctx) != thrd_success) {
		dssh_session_terminate(ctx->client);
		mock_io_close_c2s(&ctx->io);
		mock_io_close_s2c(&ctx->io);
		if (thrd_join(client, NULL) != thrd_success)
			abort();
		return -1;
	}
	if (thrd_join(client, NULL) != thrd_success || thrd_join(server, NULL) != thrd_success)
		abort();
	if (ctx->client_result != 0 || ctx->server_result != 0)
		return -1;
	active_handshake = ctx;
	return 0;
}

static void
handshake_cleanup(struct handshake_ctx *ctx)
{
	if (active_handshake == ctx)
		active_handshake = NULL;
	dssh_test_ossl_reset();
	if (ctx->io_initialized) {
		mock_io_close_c2s(&ctx->io);
		mock_io_close_s2c(&ctx->io);
	}
	if (ctx->server != NULL) {
		dssh_session_cleanup(ctx->server);
		ctx->server = NULL;
	}
	if (ctx->client != NULL) {
		dssh_session_cleanup(ctx->client);
		ctx->client = NULL;
	}
	if (ctx->io_initialized) {
		mock_io_free(&ctx->io);
		ctx->io_initialized = false;
	}
	dssh_test_reset_global_config();
}

static int
fixture_auth_none(const uint8_t *username, size_t length, void *data)
{
	(void)data;
	static const uint8_t expected[] = "thread-fixture";
	return username != NULL && length == sizeof(expected) - 1
	    && memcmp(username, expected, length) == 0 ? DSSH_AUTH_SUCCESS : DSSH_AUTH_FAILURE;
}

static int
fixture_auth_server(void *opaque)
{
	struct handshake_ctx *ctx = opaque;
	const struct dssh_auth_server_cbs callbacks = { .methods_str = "", .none_cb = fixture_auth_none };
	uint8_t username[32];
	size_t length = sizeof(username);
	ctx->server_result = dssh_auth_server(ctx->server, &callbacks, username, &length);
	if (ctx->server_result != DSSH_AUTH_SUCCESS) {
		mock_io_close_c2s(&ctx->io);
		mock_io_close_s2c(&ctx->io);
	}
	return 0;
}

static int
authenticated_setup(struct handshake_ctx *ctx)
{
	if (handshake_setup(ctx) != 0)
		return -1;
	thrd_t server;
	if (fixture_thread_create(&server, fixture_auth_server, ctx) != thrd_success)
		return -1;
	ctx->client_result = dssh_auth_begin(ctx->client, "thread-fixture");
	if (ctx->client_result != DSSH_AUTH_SUCCESS) {
		mock_io_close_c2s(&ctx->io);
		mock_io_close_s2c(&ctx->io);
	}
	if (thrd_join(server, NULL) != thrd_success)
		abort();
	return ctx->client_result == DSSH_AUTH_SUCCESS && ctx->server_result == DSSH_AUTH_SUCCESS ? 0 : -1;
}

struct fixture_worker {
	thrd_start_t function;
	void *data;
	atomic_bool finished;
};
struct fixture_creates {
	unsigned fail_at;
	unsigned calls;
	unsigned created;
	struct fixture_worker workers[3];
};
static struct fixture_creates *fixture_creates;

static int
fixture_worker(void *data)
{
	struct fixture_worker *worker = data;
	int result = worker->function(worker->data);
	atomic_store(&worker->finished, true);
	return result;
}

static int
fixture_controlled_create(thrd_t *thread, thrd_start_t function, void *data)
{
	struct fixture_creates *probe = fixture_creates;
	unsigned index = probe->calls++;
	if (index == probe->fail_at)
		return thrd_error;
	if (index >= 3)
		abort();
	struct fixture_worker *worker = &probe->workers[index];
	worker->function = function;
	worker->data = data;
	int ret = thrd_create(thread, fixture_worker, worker);
	if (ret == thrd_success)
		probe->created++;
	return ret;
}

static int
test_setup_ownership(void)
{
	for (unsigned target = 0; target < 3; target++) {
		struct fixture_creates probe = { .fail_at = target };
		for (unsigned i = 0; i < 3; i++)
			atomic_init(&probe.workers[i].finished, false);
		fixture_creates = &probe;
		fixture_thread_create = fixture_controlled_create;
		struct handshake_ctx ctx;
		int ret = authenticated_setup(&ctx);
		fixture_thread_create = thrd_create;
		fixture_creates = NULL;
		bool acquired = ctx.io_initialized;
		bool joined = true;
		for (unsigned i = 0; i < probe.created; i++)
			joined = joined && atomic_load(&probe.workers[i].finished);
		handshake_cleanup(&ctx);
		bool empty = ctx.client == NULL && ctx.server == NULL && !ctx.io_initialized
		    && active_handshake == NULL;
		bool closed = !acquired || (atomic_load(&ctx.io.c2s.rfd) == -1
		    && atomic_load(&ctx.io.c2s.wfd) == -1 && atomic_load(&ctx.io.s2c.rfd) == -1
		    && atomic_load(&ctx.io.s2c.wfd) == -1);
		handshake_cleanup(&ctx);
		ASSERT_TRUE(ret < 0 && joined && empty && closed && probe.calls == target + 1 && probe.created == target);
	}
	return TEST_PASS;
}

struct fixture_retirements {
	_Atomic(dssh_session) client;
	_Atomic(dssh_session) server;
	atomic_uint frees;
	atomic_uint worker_frees;
	bool expected_assertion;
};
static _Thread_local bool fixture_free_controller;

static void
fixture_observe_free(const void *pointer, void *data)
{
	struct fixture_retirements *probe = data;
	if (!fixture_free_controller) {
		atomic_fetch_add(&probe->worker_frees, 1);
		return;
	}
	if (pointer != NULL && pointer == atomic_load(&probe->client)) {
		atomic_store(&probe->client, NULL);
		atomic_fetch_add(&probe->frees, 1);
	}
	if (pointer != NULL && pointer == atomic_load(&probe->server)) {
		atomic_store(&probe->server, NULL);
		atomic_fetch_add(&probe->frees, 1);
	}
}

static int
fixture_worker_free(void *data)
{
	(void)data;
	void *memory = dssh_test_malloc(1);
	if (memory == NULL)
		return -1;
	dssh_test_free(memory);
	return 0;
}

static int
fixture_fail_with_active_sessions(struct fixture_retirements *probe)
{
	struct handshake_ctx ctx;
	if (authenticated_setup(&ctx) != 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}
	atomic_store(&probe->client, ctx.client);
	atomic_store(&probe->server, ctx.server);
	/* Publish before creating workers, and retain until all have joined.
	 * Only the controlling thread inspects session-pointer aliases; other
	 * threads return before loading them, including across final frees. */
	fixture_free_controller = true;
	dssh_test_alloc_observe_frees(fixture_observe_free, probe);
	ASSERT_OK(dssh_session_start(ctx.client));
	ASSERT_OK(dssh_session_start(ctx.server));
	thrd_t worker;
	ASSERT_EQ(thrd_create(&worker, fixture_worker_free, probe), thrd_success);
	int worker_result;
	if (thrd_join(worker, &worker_result) != thrd_success)
		abort();
	ASSERT_EQ(worker_result, 0);
	probe->expected_assertion = true;
	ASSERT_TRUE(false); /* The real assertion hook must release the stack owner. */
	return TEST_PASS;
}

static int
test_assertion_ownership(void)
{
	struct fixture_retirements probe;
	atomic_init(&probe.client, NULL);
	atomic_init(&probe.server, NULL);
	atomic_init(&probe.frees, 0);
	atomic_init(&probe.worker_frees, 0);
	probe.expected_assertion = false;
	int result = fixture_fail_with_active_sessions(&probe);
	dssh_test_alloc_observe_frees(NULL, NULL);
	fixture_free_controller = false;
	ASSERT_TRUE(result == TEST_FAIL && probe.expected_assertion && active_handshake == NULL
	    && atomic_load(&probe.client) == NULL && atomic_load(&probe.server) == NULL
	    && atomic_load(&probe.frees) == 2 && atomic_load(&probe.worker_frees) == 0);
	return TEST_PASS;
}

/* ================================================================
 * Tests
 * ================================================================ */

/*
 * send_packet mtx_lock failure: first thrd call in send_packet is
 * mtx_lock(tx_mtx).  Inject failure, verify session terminates.
 */
static int
test_send_lock_fail(void)
{
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	ASSERT_FALSE(ctx.client->terminate);

	dssh_test_thrd_fail_after(0);
	uint8_t msg[] = { SSH_MSG_SERVICE_REQUEST, 0x42 };
	int ret = send_packet(ctx.client, msg, sizeof(msg), NULL);
	dssh_test_thrd_reset();

	ASSERT_ERR(ret, DSSH_ERROR_TERMINATED);
	ASSERT_TRUE(ctx.client->terminate);

	handshake_cleanup(&ctx);
	return TEST_PASS;
}

/* Normal real release keeps the session usable and peer packet intact. */
static int
test_send_unlock_success(void)
{
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}
	uint8_t msg[] = {SSH_MSG_SERVICE_REQUEST, 0x42};
	int ret = send_packet(ctx.client, msg, sizeof(msg), NULL);
	uint8_t type = 0;
	uint8_t *payload = NULL;
	size_t len = 0;
	int received = ret == 0 ? recv_packet(ctx.server, &type, &payload, &len) : ret;
	bool valid = ret == 0 && received == 0 && !ctx.client->terminate
	    && type == SSH_MSG_SERVICE_REQUEST && len == sizeof(msg)
	    && memcmp(payload, msg, sizeof(msg)) == 0;
	handshake_cleanup(&ctx);
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

/* The demux is the sole RX owner, so recv_packet performs no C11 lock
 * operation.  An armed thread-failure injector must remain untouched. */
static int
test_recv_has_no_lock(void)
{
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	/* Send a packet from client so server has something to receive */
	uint8_t msg[] = { SSH_MSG_SERVICE_REQUEST, 0x42 };
	ASSERT_OK(send_packet(ctx.client, msg,
	    sizeof(msg), NULL));

	ASSERT_FALSE(ctx.server->terminate);

	dssh_test_thrd_fail_after(0);
	uint8_t mt;
	uint8_t *payload;
	size_t plen;
	int ret = recv_packet(ctx.server, &mt, &payload, &plen);
	dssh_test_thrd_reset();

	ASSERT_OK(ret);
	ASSERT_EQ(mt, SSH_MSG_SERVICE_REQUEST);
	ASSERT_FALSE(ctx.server->terminate);

	handshake_cleanup(&ctx);
	return TEST_PASS;
}

/*
 * Once termination finishes, repeated termination must perform no new
 * synchronization or callback operations.
 */
static int
test_terminate_idempotent(void)
{
	struct handshake_ctx ctx;
	if (authenticated_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	/* Start the connection layer so set_terminate exercises
	 * accept_mtx, channel_mtx, and buf_mtx paths. */
	ASSERT_OK(dssh_session_start(ctx.client));
	ASSERT_OK(dssh_session_start(ctx.server));

	ASSERT_FALSE(ctx.client->terminate);

	session_set_terminate(ctx.client);
	dssh_test_thrd_fail_after(0);
	session_set_terminate(ctx.client);
	int calls = dssh_test_thrd_count();
	dssh_test_thrd_reset();
	ASSERT_EQ(calls, 0);

	ASSERT_TRUE(ctx.client->terminate);

	handshake_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Termination must synchronize its rekey wake with tx_mtx even when
 * another thread currently owns the mutex.  A trylock cannot tell
 * whether that owner is the terminating thread.
 * ================================================================ */

struct terminate_wakeup_report {
	dssh_session observed;
	unsigned created;
	unsigned joined;
	unsigned freed;
	bool expected_assertion;
};

struct terminate_wakeup_ctx {
	dssh_session sess;
	thrd_t worker;
	bool created;
	bool locked;
	atomic_bool configured;
	atomic_bool cancelled;
	atomic_bool done;
	atomic_bool *entered;
	struct terminate_wakeup_report *report;
};
static struct terminate_wakeup_ctx *active_wakeup;
static int (*wakeup_clock)(struct timespec *) = dssh_clock_read;
static int (*wakeup_sleep)(const struct timespec *, struct timespec *) = thrd_sleep;

/* No mutex is held while observing. The deadline and attempt ceiling bound
 * observations even if UTC moves backward or stops advancing. */
static bool
terminate_wakeup_observe(atomic_bool *flag, atomic_bool *done)
{
	if (atomic_load(flag))
		return true;
	if (atomic_load(done))
		return false;
	struct timespec begin;
	if (wakeup_clock(&begin) != 0)
		return false;
	struct timespec deadline = begin;
	if (!dssh_timespec_add_ms(&deadline, 2000))
		return false;
	for (unsigned attempt = 0; attempt < 2000; attempt++) {
		if (atomic_load(flag))
			return true;
		if (atomic_load(done))
			return false;
		struct timespec now;
		if (wakeup_clock(&now) != 0 || now.tv_sec < begin.tv_sec
		    || (now.tv_sec == begin.tv_sec && now.tv_nsec < begin.tv_nsec)
		    || now.tv_sec > deadline.tv_sec
		    || (now.tv_sec == deadline.tv_sec && now.tv_nsec >= deadline.tv_nsec))
			return false;
		const struct timespec delay = { .tv_nsec = 1000000L };
		int ret = wakeup_sleep(&delay, NULL);
		if (ret != 0 && ret != -1)
			return false;
	}
	return atomic_load(flag);
}

static int
terminate_wakeup_terminator(void *arg)
{
	struct terminate_wakeup_ctx *ctx = arg;
	/* This pause is local to this worker and precedes its native acquisition.
	 * Publish its process-lifetime observation flag before the controller reads it. */
	ctx->entered = dssh_test_thrd_pause_mutex_after(&ctx->sess->trans.tx_mtx, 0);
	atomic_store(&ctx->configured, true);
	/* Cleanup may run before this worker is scheduled. Releasing a pause before
	 * its configuration must not leave a later configuration parked at join. */
	if (atomic_load(&ctx->cancelled))
		dssh_test_thrd_resume_mutex();
	else
		session_set_terminate(ctx->sess);
	atomic_store(&ctx->done, true);
	return 0;
}

static void
terminate_wakeup_freed(const void *pointer, void *opaque)
{
	struct terminate_wakeup_report *report = opaque;
	if (pointer != NULL && pointer == report->observed) {
		report->observed = NULL;
		report->freed++;
	}
}

static bool
terminate_wakeup_cleanup(struct terminate_wakeup_ctx *ctx)
{
	if (active_wakeup == ctx)
		active_wakeup = NULL;
	/* Release the only possible controller-owned lock before resuming/joining. */
	if (ctx->locked) {
		if (mtx_unlock(&ctx->sess->trans.tx_mtx) != thrd_success)
			abort();
		ctx->locked = false;
	}
	atomic_store(&ctx->cancelled, true);
	dssh_test_thrd_resume_mutex();
	bool valid = true;
	if (ctx->created) {
		int result;
		if (thrd_join(ctx->worker, &result) != thrd_success)
			abort();
		ctx->created = false;
		ctx->report->joined++;
		valid = result == 0 && atomic_load(&ctx->done);
	}
	if (ctx->sess != NULL) {
		dssh_session_cleanup(ctx->sess);
		ctx->sess = NULL;
	}
	return valid;
}

static bool
terminate_wakeup_start(struct terminate_wakeup_ctx *ctx,
    struct terminate_wakeup_report *report)
{
	memset(ctx, 0, sizeof(*ctx));
	ctx->report = report;
	atomic_init(&ctx->configured, false);
	atomic_init(&ctx->cancelled, false);
	atomic_init(&ctx->done, false);
	ctx->sess = dssh_session_init(true, 0);
	if (ctx->sess == NULL)
		return false;
	report->observed = ctx->sess;
	dssh_test_alloc_observe_frees(terminate_wakeup_freed, report);
	active_wakeup = ctx;
	if (fixture_thread_create(&ctx->worker, terminate_wakeup_terminator, ctx) != thrd_success)
		return false;
	ctx->created = true;
	report->created++;
	return true;
}

static int
test_set_terminate_rekey_wakeup(void)
{
	struct terminate_wakeup_report report = {0};
	struct terminate_wakeup_ctx ctx;
	bool valid = terminate_wakeup_start(&ctx, &report);
	if (valid)
		valid = terminate_wakeup_observe(&ctx.configured, &ctx.done)
		    && terminate_wakeup_observe(ctx.entered, &ctx.done);
	struct timespec deadline;
	if (valid)
		valid = dssh_deadline_from_ms(&deadline, 2000) == 0;
	if (valid) {
		valid = mtx_lock(&ctx.sess->trans.tx_mtx) == thrd_success;
		ctx.locked = valid;
	}
	if (valid) {
		/* Only tx_mtx is held. The real C11 wait releases/reacquires it;
		 * termination's observed acquisition must serialize with that release. */
		dssh_test_thrd_resume_mutex();
		valid = cnd_timedwait(&ctx.sess->trans.rekey_cnd,
		    &ctx.sess->trans.tx_mtx, &deadline) == thrd_success;
	}
	bool finished = terminate_wakeup_cleanup(&ctx);
	dssh_test_alloc_observe_frees(NULL, NULL);
	ASSERT_TRUE(valid && finished && active_wakeup == NULL && ctx.sess == NULL
	    && !ctx.created && !ctx.locked && report.created == 1 && report.joined == 1
	    && report.freed == 1 && report.observed == NULL);
	return TEST_PASS;
}

static int
wakeup_refuse_create(thrd_t *thread, thrd_start_t function, void *data)
{
	(void)thread;
	(void)function;
	(void)data;
	return thrd_error;
}

static int
test_wakeup_setup_ownership(void)
{
	struct terminate_wakeup_report report = {0};
	struct terminate_wakeup_ctx ctx;
	fixture_thread_create = wakeup_refuse_create;
	bool started = terminate_wakeup_start(&ctx, &report);
	fixture_thread_create = thrd_create;
	bool finished = terminate_wakeup_cleanup(&ctx);
	bool repeated = terminate_wakeup_cleanup(&ctx);
	dssh_test_alloc_observe_frees(NULL, NULL);
	ASSERT_TRUE(!started && finished && repeated && active_wakeup == NULL
	    && ctx.sess == NULL && !ctx.created && !ctx.locked && report.created == 0
	    && report.joined == 0 && report.freed == 1 && report.observed == NULL);
	return TEST_PASS;
}

static int
wakeup_assert_with_active_owner(unsigned phase, struct terminate_wakeup_report *report)
{
	struct terminate_wakeup_ctx ctx;
	if (!terminate_wakeup_start(&ctx, report)) {
		if (!terminate_wakeup_cleanup(&ctx))
			abort();
		return TEST_FAIL;
	}
	if (phase != 0) {
		ASSERT_TRUE(terminate_wakeup_observe(&ctx.configured, &ctx.done));
		ASSERT_TRUE(terminate_wakeup_observe(ctx.entered, &ctx.done));
	}
	if (phase == 2) {
		int ret = mtx_lock(&ctx.sess->trans.tx_mtx);
		ctx.locked = ret == thrd_success;
		ASSERT_EQ(ret, thrd_success);
	}
	report->expected_assertion = true;
	ASSERT_TRUE(false); /* Cleanup must finish before this owner frame returns. */
	return TEST_PASS;
}

static int
test_wakeup_assertion_ownership(void)
{
	for (unsigned phase = 0; phase < 3; phase++) {
		struct terminate_wakeup_report report = {0};
		int ret = wakeup_assert_with_active_owner(phase, &report);
		dssh_test_alloc_observe_frees(NULL, NULL);
		ASSERT_TRUE(ret == TEST_FAIL && report.expected_assertion && active_wakeup == NULL
		    && report.created == 1 && report.joined == 1 && report.freed == 1
		    && report.observed == NULL);
	}
	return TEST_PASS;
}

struct wakeup_observation_script {
	unsigned mode;
	unsigned clocks;
	unsigned sleeps;
	atomic_bool flag;
};
static struct wakeup_observation_script *wakeup_script;

static int
wakeup_script_clock(struct timespec *now)
{
	struct wakeup_observation_script *script = wakeup_script;
	unsigned call = script->clocks++;
	if ((script->mode == 0 && call == 0) || (script->mode == 1 && call == 1))
		return DSSH_ERROR_INIT;
	*now = (struct timespec){ .tv_sec = 100 };
	if (call != 0 && script->mode == 2)
		now->tv_sec = 99;
	if (call != 0 && script->mode == 3)
		now->tv_sec = 102;
	return 0;
}

static int
wakeup_script_sleep(const struct timespec *duration, struct timespec *remaining)
{
	struct wakeup_observation_script *script = wakeup_script;
	if (duration->tv_sec != 0 || duration->tv_nsec != 1000000L || remaining != NULL)
		abort();
	script->sleeps++;
	if (script->mode == 4)
		return -2;
	if (script->mode == 5 || script->mode == 6)
		atomic_store(&script->flag, true);
	return script->mode == 5 ? -1 : 0;
}

static int
test_wakeup_observation_bounds(void)
{
	atomic_bool done;
	atomic_init(&done, false);
	for (unsigned mode = 0; mode < 8; mode++) {
		struct wakeup_observation_script script = { .mode = mode };
		atomic_init(&script.flag, false);
		wakeup_script = &script;
		wakeup_clock = wakeup_script_clock;
		wakeup_sleep = wakeup_script_sleep;
		bool observed = terminate_wakeup_observe(&script.flag, &done);
		wakeup_clock = dssh_clock_read;
		wakeup_sleep = thrd_sleep;
		wakeup_script = NULL;
		unsigned clocks = mode == 0 ? 1U : mode == 7 ? 2001U : 2U;
		unsigned sleeps = mode < 4 ? 0U : mode == 7 ? 2000U : 1U;
		ASSERT_TRUE(observed == (mode == 5 || mode == 6)
		    && script.clocks == clocks && script.sleeps == sleeps);
	}
	struct wakeup_observation_script script = {0};
	atomic_init(&script.flag, true);
	wakeup_script = &script;
	wakeup_clock = wakeup_script_clock;
	bool ready = terminate_wakeup_observe(&script.flag, &done);
	atomic_store(&script.flag, false);
	atomic_store(&done, true);
	bool stopped = terminate_wakeup_observe(&script.flag, &done);
	wakeup_clock = dssh_clock_read;
	wakeup_script = NULL;
	ASSERT_TRUE(ready && !stopped && script.clocks == 0 && script.sleeps == 0);
	return TEST_PASS;
}

/*
 * Sweep: iterate over all thrd calls in send_packet, failing each
 * in turn.  Verify the session always terminates.
 */
static int
test_send_sweep(void)
{
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	/* Count thrd calls in a normal send */
	dssh_test_thrd_fail_after(1000);
	uint8_t msg[] = { SSH_MSG_SERVICE_REQUEST, 0x42 };
	send_packet(ctx.client, msg, sizeof(msg), NULL);
	int count = dssh_test_thrd_count();
	dssh_test_thrd_reset();
	ASSERT_TRUE(count >= 2);

	handshake_cleanup(&ctx);

	/* Now fail each thrd call in turn */
	for (int n = 0; n < count; n++) {
		if (handshake_setup(&ctx) < 0) {
			handshake_cleanup(&ctx);
			return TEST_FAIL;
		}

		dssh_test_thrd_fail_after(n);
		send_packet(ctx.client, msg,
		    sizeof(msg), NULL);
		dssh_test_thrd_reset();

		ASSERT_TRUE(ctx.client->terminate);

		handshake_cleanup(&ctx);
	}

	return TEST_PASS;
}

/* Verify the single-owner RX path has no injectable C11 synchronization. */
static int
test_recv_sweep(void)
{
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	/* Send a packet */
	uint8_t msg[] = { SSH_MSG_SERVICE_REQUEST, 0x42 };
	ASSERT_OK(send_packet(ctx.client, msg,
	    sizeof(msg), NULL));

	/* Count thrd calls in a normal recv */
	dssh_test_thrd_fail_after(1000);
	uint8_t mt;
	uint8_t *payload;
	size_t plen;
	recv_packet(ctx.server, &mt, &payload, &plen);
	int count = dssh_test_thrd_count();
	dssh_test_thrd_reset();
	ASSERT_EQ(count, 0);

	handshake_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * session_start() init failure tests
 *
 * dssh_session_start() calls mtx_init × 2, cnd_init × 1,
 * thrd_create × 1.  These go through the ossl countdown
 * (not the thrd countdown), so we use dssh_test_ossl_fail_after.
 * We handshake first with no injection, then arm before start().
 * ================================================================ */

/*
 * Sweep all 4 init/create calls in dssh_session_start().
 * n=0: mtx_init(&channel_mtx) fails
 * n=1: mtx_init(&accept_mtx) fails
 * n=2: cnd_init(&accept_cnd) fails
 * n=3: thrd_create() fails
 */
static int
test_start_init_sweep(void)
{
	for (int n = 0; n < 4; n++) {
		struct handshake_ctx ctx;
		if (authenticated_setup(&ctx) < 0) {
			handshake_cleanup(&ctx);
			return TEST_FAIL;
		}

		/* Arm ossl countdown to fail the Nth init/create call */
		dssh_test_ossl_fail_after(n);
		int res = dssh_session_start(ctx.client);
		dssh_test_ossl_reset();

		ASSERT_ERR(res, DSSH_ERROR_INIT);

		/* session_start failed, so no demux thread to stop.
		 * Close pipes and clean up. */
		mock_io_close_c2s(&ctx.io);
		mock_io_close_s2c(&ctx.io);
		handshake_cleanup(&ctx);
	}

	return TEST_PASS;
}

/*
 * Verify session_start(NULL) returns DSSH_ERROR_INVALID.
 */
static int
test_start_null(void)
{
	ASSERT_ERR(dssh_session_start(NULL), DSSH_ERROR_INVALID);
	return TEST_PASS;
}

/*
 * Verify double-start returns DSSH_ERROR_INIT.
 */
static int
test_start_double(void)
{
	struct handshake_ctx ctx;
	if (authenticated_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	ASSERT_OK(dssh_session_start(ctx.client));
	ASSERT_ERR(dssh_session_start(ctx.client), DSSH_ERROR_INIT);

	/* Clean up: close pipes so demux thread exits */
	mock_io_close_c2s(&ctx.io);
	mock_io_close_s2c(&ctx.io);
	dssh_session_stop(ctx.client);
	dssh_session_stop(ctx.server);
	handshake_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Test list
 * ================================================================ */

static void
unused_event_cb(dssh_channel ch, const struct dssh_chan_event *event, void *data)
{
	(void)ch;
	(void)event;
	(void)data;
}

/* Check both the protected state and the acquisition/release discipline.
 * The targeted reject hook suppresses an invalid native unlock in the before
 * implementation, so the regression itself does not rely on undefined behavior. */
static int
channel_mutex_failure(unsigned operation)
{
	for (unsigned prior = 0; prior < 2; prior++) {
		dssh_test_ossl_reset();
		dssh_session sess = dssh_session_init(true, 0);
		if (sess == NULL)
			return TEST_FAIL;
		struct dssh_channel_s ch = {
			.sess = sess,
			.open = true,
			.event_cb = NULL,
			.event_cbdata = NULL,
		};
		ch.eof_sent = prior != 0;
		ch.close_sent = prior != 0;
		ch.events.count = prior != 0 ? 50 : 0;
		ch.events.max_events = 64;
		int ret = mtx_init(&ch.buf_mtx, mtx_plain);
		if (ret != thrd_success) {
			dssh_session_cleanup(sess);
			return TEST_FAIL;
		}
		ret = mtx_init(&ch.cb_mtx, mtx_plain);
		if (ret != thrd_success) {
			mtx_destroy(&ch.buf_mtx);
			dssh_session_cleanup(sess);
			return TEST_FAIL;
		}
		mtx_t *target = operation == 2 ? &ch.cb_mtx : &ch.buf_mtx;
		dssh_test_mtx_reject_once(target);
		if (operation == 0)
			ret = send_eof(sess, &ch);
		else if (operation == 1)
			ret = conn_close(sess, &ch);
		else if (operation == 2)
			ret = dssh_chan_set_event_cb(&ch, unused_event_cb, &ch);
		else
			ret = dssh_chan_set_max_events(&ch, 40);
		bool valid = ret == DSSH_ERROR_TERMINATED && sess->terminate
		    && dssh_test_mtx_rejected_unlocks() == 0;
		valid = valid && ch.eof_sent == (prior != 0)
		    && ch.close_sent == (prior != 0) && ch.open
		    && ch.event_cb == NULL && ch.event_cbdata == NULL
		    && ch.events.max_events == 64;
		dssh_test_ossl_reset();
		mtx_destroy(&ch.cb_mtx);
		mtx_destroy(&ch.buf_mtx);
		dssh_session_cleanup(sess);
		ASSERT_TRUE(valid);
	}
	return TEST_PASS;
}

static int
test_eof_acquire(void)
{
	return channel_mutex_failure(0);
}

static int
test_close_acquire(void)
{
	return channel_mutex_failure(1);
}

static int
test_event_acquire(void)
{
	return channel_mutex_failure(2);
}

static int
test_limit_acquire(void)
{
	return channel_mutex_failure(3);
}

#include "termination_wakeup_cases.h"
#include "condition_wait_cases.h"
#include "log_queue_cases.h"

static struct dssh_test_entry tests[] = {
	{ "thrd/log_queue_outcomes", test_log_queue_outcomes },
	{ "thrd/setup_ownership", test_setup_ownership },
	{ "thrd/assertion_ownership", test_assertion_ownership },
	{ "thrd/wakeup_setup_ownership", test_wakeup_setup_ownership },
	{ "thrd/wakeup_assertion_ownership", test_wakeup_assertion_ownership },
	{ "thrd/wakeup_observation_bounds", test_wakeup_observation_bounds },
	{ "thrd/wait_contract_errors", test_wait_contract_errors },
	{ "thrd/wait_contract_spurious", test_wait_contract_spurious },
	{ "thrd/wait_contract_timeout_errors", test_wait_contract_timeout_errors },
	{ "thrd/wait_contract_timeouts", test_wait_contract_timeouts },
	{ "thrd/wake_rekey", test_wake_rekey },
	{ "thrd/wake_slot", test_wake_slot },
	{ "thrd/wake_accept", test_wake_accept },
	{ "thrd/wake_poll", test_wake_poll },
	{ "thrd/deferred_rekey", test_deferred_rekey },
	{ "thrd/deferred_slot", test_deferred_slot },
	{ "thrd/deferred_accept", test_deferred_accept },
	{ "thrd/deferred_poll", test_deferred_poll },
	{ "thrd/wake_retry", test_wake_retry },
	{ "thrd/deferred_retry", test_deferred_retry },
	{ "thrd/signal_retry", test_signal_retry },
	{ "thrd/eof_acquire",          test_eof_acquire },
	{ "thrd/close_acquire",        test_close_acquire },
	{ "thrd/event_acquire",        test_event_acquire },
	{ "thrd/limit_acquire",        test_limit_acquire },
	{ "thrd/demux_exit_deferred",  test_demux_exit_deferred },
	{ "thrd/send_lock_fail",      test_send_lock_fail },
	{ "thrd/send_unlock_success", test_send_unlock_success },
	{ "thrd/recv_has_no_lock",    test_recv_has_no_lock },
	{ "thrd/terminate_idempotent", test_terminate_idempotent },
	{ "thrd/set_terminate_rekey_wakeup", test_set_terminate_rekey_wakeup },
	{ "thrd/send_sweep",          test_send_sweep },
	{ "thrd/recv_sweep",          test_recv_sweep },
	{ "thrd/start_init_sweep",    test_start_init_sweep },
	{ "thrd/start_null",          test_start_null },
	{ "thrd/start_double",        test_start_double },
};

static dssh_log_action
forward_test_log(const struct dssh_log_record *record, void *opaque)
{
	(void)record;
	(void)opaque;
	return DSSH_LOG_SEND_DEBUG;
}

/* Fresh exec only. An unexpected return exits without touching the retained
 * mutex. The driver requires both fault provenance and actual SIGABRT. */
static int
unlock_fatal_child(const char *name, const char *mode)
{
	bool release_real = strcmp(mode, "released") == 0;
	if (!release_real && strcmp(mode, "held") != 0)
		return 2;
	dssh_test_ossl_reset();
	if (strcmp(name, "primitive") == 0) {
		mtx_t mutex;
		if (mtx_init(&mutex, mtx_plain) != thrd_success || mtx_lock(&mutex) != thrd_success)
			return 3;
		dssh_test_unlock_fail_after(&mutex, 0, release_real);
		dssh_test_unlock_boundary(&mutex);
		_Exit(4);
	}
	bool transport = strcmp(name, "log-empty") == 0 || strcmp(name, "log-data") == 0
	    || strcmp(name, "wa-send") == 0;
	struct handshake_ctx ctx;
	dssh_session sess;
	if (transport) {
		if (handshake_setup(&ctx) != 0)
			return 3;
		sess = ctx.client;
	}
	else {
		sess = dssh_session_init(true, 0);
		if (sess == NULL)
			return 3;
	}
	if (strncmp(name, "log-", 4) == 0) {
		if (dssh_session_set_log_cb(sess, forward_test_log, NULL) != 0)
			return 3;
		if (strcmp(name, "log-data") == 0)
			sess->log_forward->count = 1;
		if (transport && tx_acquire(sess, SSH_MSG_DEBUG) != 0)
			return 3;
		dssh_test_unlock_fail_after(&sess->log_forward->mtx, 0, release_real);
		if (strcmp(name, "log-queue") == 0)
			dssh_log_emit(sess, DSSH_LOG_ERROR, DSSH_LOG_SOURCE_LIBRARY, 0, 0,
			    false, false, NULL, 0, NULL, 0);
		else if (transport)
			drain_log_forwards(sess);
		else
			return 2;
		_Exit(4);
	}
	struct dssh_channel_s ch = {.sess = sess, .open = true};
	if (mtx_init(&ch.buf_mtx, mtx_plain) != thrd_success
	    || mtx_init(&ch.cb_mtx, mtx_plain) != thrd_success
	    || cnd_init(&ch.poll_cnd) != thrd_success)
		return 3;
	if (strncmp(name, "wa-", 3) == 0) {
		bool idle = strcmp(name, "wa-idle") == 0;
		bool wire = strcmp(name, "wa-send") == 0;
		if (!idle && !wire && strcmp(name, "wa-zero") != 0)
			return 2;
		atomic_init(&ch.wa_slot.ready, !idle);
		if (wire) {
			ch.window_adjust_pending = 1;
			ch.wa_slot.buf = malloc(sess->trans.packet_buf_sz);
			if (ch.wa_slot.buf == NULL || tx_acquire(sess, SSH_MSG_CHANNEL_WINDOW_ADJUST) != 0)
				return 3;
		}
		dssh_test_unlock_fail_after(&ch.buf_mtx, wire ? 1U : 0U, release_real);
		bool progress = false;
		int status = drain_one_wa_slot(sess, &ch, &progress, false);
		_Exit(status == 0 ? 4 : 5);
	}
	mtx_t *target = strcmp(name, "event") == 0 ? &ch.cb_mtx : &ch.buf_mtx;
	dssh_test_unlock_fail_after(target, 0, release_real);
	if (strcmp(name, "event") == 0)
		dssh_chan_set_event_cb(&ch, unused_event_cb, NULL);
	else if (strcmp(name, "limit") == 0)
		dssh_chan_set_max_events(&ch, 1);
	else if (strcmp(name, "eof") == 0) {
		ch.eof_sent = true;
		send_eof(sess, &ch);
	}
	else if (strcmp(name, "close") == 0) {
		ch.close_sent = true;
		conn_close(sess, &ch);
	}
	else
		return 2;
	_Exit(4);
}

void
dssh_test_failure_cleanup(void)
{
	if (active_wakeup != NULL && !terminate_wakeup_cleanup(active_wakeup))
		abort();
	if (active_handshake != NULL)
		handshake_cleanup(active_handshake);
}

#define main dssh_test_thread_errors_main
DSSH_TEST_MAIN(tests)
#undef main

int
main(int argc, char **argv)
{
	if (argc == 4 && strcmp(argv[1], "--unlock-fatal") == 0)
		return unlock_fatal_child(argv[2], argv[3]);
	if (argc == 4 && strcmp(argv[1], "--wake-fatal") == 0)
		return wake_fatal_child(argv[2], argv[3]);
	return dssh_test_thread_errors_main(argc, argv);
}
