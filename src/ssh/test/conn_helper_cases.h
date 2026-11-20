#ifndef DSSH_TEST_CONN_HELPER_CASES_H
#define DSSH_TEST_CONN_HELPER_CASES_H

enum conn_setup_control {
	CONN_SETUP_REGISTRATION,
	CONN_SETUP_CLIENT,
	CONN_SETUP_SERVER,
	CONN_SETUP_CREATE_CLIENT,
	CONN_SETUP_CREATE_SERVER,
	CONN_SETUP_START_CLIENT,
	CONN_SETUP_START_SERVER,
	CONN_SETUP_IO,
	CONN_SETUP_AUTH,
	CONN_SETUP_SUCCESS,
};

struct conn_setup_worker {
	thrd_start_t function;
	void *data;
	bool fail_io;
	atomic_bool done;
	atomic_bool io_called;
};

struct conn_setup_probe {
	struct conn_ctx *ctx;
	enum conn_setup_control control;
	unsigned session_allocations;
	bool failed;
	bool io_acquired;
	dssh_session client;
	dssh_session server;
	unsigned client_frees;
	unsigned server_frees;
	unsigned create_calls;
	unsigned created;
	unsigned joined;
	unsigned start_calls;
	unsigned started;
	atomic_bool auth_called;
	struct conn_setup_worker workers[2];
};
static struct conn_setup_probe *conn_setup_probe;
static _Thread_local atomic_bool *conn_setup_io_called;

static void
conn_setup_capture(struct conn_setup_probe *probe)
{
	/* Each identity is cleared at its matching live pre-free callback.
	 * Never inspect a retired context field to reacquire that identity. */
	if (probe->client_frees == 0 && probe->client == NULL)
		probe->client = probe->ctx->client;
	if (probe->server_frees == 0 && probe->server == NULL)
		probe->server = probe->ctx->server;
	if (probe->ctx->io_initialized)
		probe->io_acquired = true;
}

static void
conn_setup_allocation(size_t count, size_t size, void *data)
{
	struct conn_setup_probe *probe = data;
	conn_setup_capture(probe);
	if (probe->control == CONN_SETUP_REGISTRATION && !probe->failed) {
		probe->failed = true;
		dssh_test_alloc_fail_after(0);
	}
	if (count == 1 && size == sizeof(struct dssh_session_s)) {
		probe->session_allocations++;
		if ((probe->control == CONN_SETUP_CLIENT && probe->session_allocations == 1)
		    || (probe->control == CONN_SETUP_SERVER && probe->session_allocations == 2)) {
			probe->failed = true;
			dssh_test_alloc_fail_after(0);
		}
	}
}

static void
conn_setup_free(const void *ptr, void *data)
{
	struct conn_setup_probe *probe = data;
	if (ptr != NULL && ptr == probe->client) {
		probe->client_frees++;
		probe->client = NULL;
	}
	if (ptr != NULL && ptr == probe->server) {
		probe->server_frees++;
		probe->server = NULL;
	}
}

static int
conn_setup_io_failure(dssh_session sess)
{
	(void)sess;
	atomic_store(conn_setup_io_called, true);
	return DSSH_ERROR_INIT;
}

static int
conn_setup_worker(void *data)
{
	struct conn_setup_worker *worker = data;
	if (worker->fail_io) {
		conn_setup_io_called = &worker->io_called;
		setup_tx_test_hook = conn_setup_io_failure;
	}
	int ret = worker->function(worker->data);
	setup_tx_test_hook = NULL;
	conn_setup_io_called = NULL;
	atomic_store(&worker->done, true);
	return ret;
}

static int
conn_setup_create(thrd_t *thread, thrd_start_t function, void *data)
{
	struct conn_setup_probe *probe = conn_setup_probe;
	conn_setup_capture(probe);
	unsigned index = probe->create_calls++;
	if (index >= 2)
		abort();
	if ((probe->control == CONN_SETUP_CREATE_CLIENT && index == 0)
	    || (probe->control == CONN_SETUP_CREATE_SERVER && index == 1)) {
		probe->failed = true;
		return thrd_error;
	}
	struct conn_setup_worker *worker = &probe->workers[index];
	worker->function = function;
	worker->data = data;
	worker->fail_io = probe->control == CONN_SETUP_IO && index == 0;
	int ret = thrd_create(thread, conn_setup_worker, worker);
	if (ret == thrd_success)
		probe->created++;
	return ret;
}

static int
conn_setup_join(thrd_t thread, int *result)
{
	int ret = thrd_join(thread, result);
	if (ret == thrd_success)
		conn_setup_probe->joined++;
	return ret;
}

static int
conn_setup_start(dssh_session sess)
{
	struct conn_setup_probe *probe = conn_setup_probe;
	unsigned index = probe->start_calls++;
	if ((probe->control == CONN_SETUP_START_CLIENT && index == 0)
	    || (probe->control == CONN_SETUP_START_SERVER && index == 1)) {
		/* Model this API refusal without racing a process-wide countdown
		 * against the already-created demux. Other starts are native. */
		probe->failed = true;
		return DSSH_ERROR_INIT;
	}
	int ret = dssh_session_start(sess);
	if (ret == 0)
		probe->started++;
	return ret;
}

static int
conn_setup_auth_refuse(const uint8_t *user, size_t user_len,
    const uint8_t *password, size_t password_len,
    struct dssh_bytes_result *result, void *data)
{
	(void)user;
	(void)user_len;
	(void)password;
	(void)password_len;
	(void)result;
	(void)data;
	atomic_store(&conn_setup_probe->auth_called, true);
	return DSSH_AUTH_FAILURE;
}

static int
conn_setup_ownership_case(bool start)
{
	unsigned schedules = 0;
	for (enum conn_setup_control control = CONN_SETUP_REGISTRATION;
	    control <= CONN_SETUP_SUCCESS; control++) {
		if (!start && (control == CONN_SETUP_START_CLIENT || control == CONN_SETUP_START_SERVER))
			continue;
		dssh_test_alloc_reset();
		dssh_test_ossl_reset();
		struct conn_ctx ctx;
		struct conn_setup_probe probe = {.ctx = &ctx, .control = control};
		atomic_init(&probe.auth_called, false);
		for (size_t i = 0; i < 2; i++) {
			atomic_init(&probe.workers[i].done, false);
			atomic_init(&probe.workers[i].io_called, false);
		}
		conn_setup_probe = &probe;
		conn_fixture_thread_create = conn_setup_create;
		conn_fixture_thread_join = conn_setup_join;
		conn_fixture_session_start = conn_setup_start;
		auth_cbs.password_cb = control == CONN_SETUP_AUTH
		    ? conn_setup_auth_refuse : auth_password_accept_all;
		dssh_test_alloc_observe_allocations(conn_setup_allocation, &probe);
		dssh_test_alloc_observe_frees(conn_setup_free, &probe);
		int ret = start ? conn_setup(&ctx) : conn_setup_no_start(&ctx);
		conn_fixture_thread_create = thrd_create;
		conn_fixture_thread_join = thrd_join;
		conn_fixture_session_start = dssh_session_start;
		auth_cbs.password_cb = auth_password_accept_all;
		conn_setup_probe = NULL;
		dssh_test_alloc_fail_after(-1);
		dssh_test_alloc_observe_allocations(NULL, NULL);
		bool valid = ret == (control == CONN_SETUP_SUCCESS ? 0 : -1);
		if (ret != 0)
			valid = valid && ctx.client == NULL && ctx.server == NULL
			    && !ctx.io_initialized && g_active_ctx == NULL;
		else
			valid = valid && ctx.client != NULL && ctx.server != NULL
			    && ctx.io_initialized && g_active_ctx == &ctx;
		if (ret == 0) {
			const char *kex = dssh_transport_get_kex_name(ctx.client);
			dssh_key_algo key = atomic_load(&ctx.client->trans.key_algo_selected);
			valid = valid && kex != NULL && key != NULL;
			if (kex != NULL && key != NULL)
				printf("conn_setup_profile kex=%s key=%s\n", kex, key->name);
		}
		for (unsigned i = 0; i < probe.created; i++)
			valid = valid && atomic_load(&probe.workers[i].done);
		valid = valid && atomic_load(&probe.workers[0].io_called) == (control == CONN_SETUP_IO)
		    && !atomic_load(&probe.workers[1].io_called)
		    && atomic_load(&probe.auth_called) == (control == CONN_SETUP_AUTH);
		conn_cleanup(&ctx);
		conn_cleanup_no_start(&ctx);
		dssh_test_alloc_observe_frees(NULL, NULL);
		valid = valid && ctx.client == NULL && ctx.server == NULL
		    && !ctx.io_initialized && g_active_ctx == NULL
		    && probe.client == NULL && probe.server == NULL;
		unsigned expected_client = control <= CONN_SETUP_CLIENT ? 0 : 1;
		unsigned expected_server = control <= CONN_SETUP_SERVER ? 0 : 1;
		valid = valid && probe.client_frees == expected_client
		    && probe.server_frees == expected_server;
		if (probe.io_acquired)
			valid = valid && atomic_load(&ctx.io.c2s.rfd) == -1
			    && atomic_load(&ctx.io.c2s.wfd) == -1
			    && atomic_load(&ctx.io.s2c.rfd) == -1
			    && atomic_load(&ctx.io.s2c.wfd) == -1;
		valid = valid && probe.io_acquired == (control != CONN_SETUP_REGISTRATION);
		unsigned expected_allocations = control == CONN_SETUP_REGISTRATION ? 0
		    : control == CONN_SETUP_CLIENT ? 1 : 2;
		valid = valid && probe.session_allocations == expected_allocations;
		unsigned expected_creates = control <= CONN_SETUP_SERVER ? 0
		    : control == CONN_SETUP_CREATE_CLIENT ? 1 : 2;
		unsigned expected_created = control <= CONN_SETUP_CREATE_CLIENT ? 0
		    : control == CONN_SETUP_CREATE_SERVER ? 1 : 2;
		unsigned expected_starts = !start || control <= CONN_SETUP_CREATE_SERVER
		    || control == CONN_SETUP_IO || control == CONN_SETUP_AUTH ? 0
		    : control == CONN_SETUP_START_CLIENT ? 1 : 2;
		unsigned expected_started = expected_starts == 0 || control == CONN_SETUP_START_CLIENT ? 0
		    : control == CONN_SETUP_START_SERVER ? 1 : 2;
		valid = valid && probe.create_calls == expected_creates && probe.created == expected_created
		    && probe.start_calls == expected_starts && probe.started == expected_started;
		/* The library counter includes at most two native demux joins;
		 * this test TU uses native C11 calls for its own fixture workers. */
		int expected_joins = (int)probe.started;
		int native_joins = dssh_test_join_count();
		valid = valid && probe.joined == probe.created && native_joins == expected_joins;
		if (control <= CONN_SETUP_START_SERVER)
			valid = valid && probe.failed;
		dssh_test_alloc_reset();
		dssh_test_ossl_reset();
		if (!valid)
			fprintf(stderr, "conn setup control=%d start=%u ret=%d session-free=%u/%u "
			    "created=%u starts=%u/%u joins=%d\n", control, start ? 1U : 0U,
			    ret, probe.client_frees, probe.server_frees, probe.created,
			    probe.start_calls, probe.started, native_joins);
		ASSERT_TRUE(valid);
		schedules++;
	}
	printf("conn_setup_ownership start=%u schedules=%u\n", start ? 1U : 0U, schedules);
	return TEST_PASS;
}

static int
test_conn_setup_ownership(void)
{
	return conn_setup_ownership_case(true);
}

static int
test_conn_no_start_ownership(void)
{
	return conn_setup_ownership_case(false);
}

#endif
