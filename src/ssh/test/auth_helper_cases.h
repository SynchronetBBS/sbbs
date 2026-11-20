#ifndef DSSH_TEST_AUTH_HELPER_CASES_H
#define DSSH_TEST_AUTH_HELPER_CASES_H

struct auth_setup_allocation_probe {
	unsigned target;
	unsigned sessions;
	bool failed;
};

static void
auth_setup_fail_session(size_t count, size_t size, void *data)
{
	struct auth_setup_allocation_probe *probe = data;
	if (count == 1 && size == sizeof(struct dssh_session_s)) {
		probe->sessions++;
		if (probe->sessions == probe->target) {
			probe->failed = true;
			dssh_test_alloc_fail_after(0);
		}
	}
}

struct auth_setup_worker {
	thrd_start_t function;
	void *data;
	atomic_bool done;
};

struct auth_setup_thread_probe {
	unsigned fail_at;
	unsigned calls;
	unsigned created;
	struct auth_setup_worker workers[3];
};

static struct auth_setup_thread_probe *auth_setup_threads;

static int
auth_setup_worker(void *data)
{
	struct auth_setup_worker *worker = data;
	int ret = worker->function(worker->data);
	atomic_store(&worker->done, true);
	return ret;
}

static int
auth_setup_create(thrd_t *thread, thrd_start_t function, void *data)
{
	struct auth_setup_thread_probe *probe = auth_setup_threads;
	unsigned call = probe->calls++;
	if (call == probe->fail_at)
		return thrd_error;
	if (call >= 3)
		abort();
	struct auth_setup_worker *worker = &probe->workers[call];
	worker->function = function;
	worker->data = data;
	int ret = thrd_create(thread, auth_setup_worker, worker);
	if (ret == thrd_success)
		probe->created++;
	return ret;
}

static bool
auth_setup_empty(const struct handshake_ctx *ctx)
{
	return ctx->client == NULL && ctx->server == NULL && !ctx->io_initialized;
}

struct auth_setup_retirement {
	dssh_session client;
	dssh_session server;
	unsigned client_frees;
	unsigned server_frees;
	bool io_initialized;
	bool expect_client;
	bool expect_server;
};

static void
auth_setup_record_free(const void *ptr, void *data)
{
	struct auth_setup_retirement *probe = data;
	if (ptr != NULL && ptr == probe->client) {
		probe->client_frees++;
		probe->client = NULL;
	}
	if (ptr != NULL && ptr == probe->server) {
		probe->server_frees++;
		probe->server = NULL;
	}
}

static void
auth_setup_watch_retirement(const struct handshake_ctx *ctx, struct auth_setup_retirement *probe)
{
	*probe = (struct auth_setup_retirement){.client = ctx->client, .server = ctx->server,
	    .io_initialized = ctx->io_initialized,
	    .expect_client = ctx->client != NULL, .expect_server = ctx->server != NULL};
	dssh_test_alloc_observe_frees(auth_setup_record_free, probe);
}

static bool
auth_setup_retirement_ok(const struct handshake_ctx *ctx, const struct auth_setup_retirement *probe)
{
	dssh_test_alloc_observe_frees(NULL, NULL);
	bool released = auth_setup_empty(ctx)
	    && probe->client_frees == (probe->expect_client ? 1U : 0U)
	    && probe->server_frees == (probe->expect_server ? 1U : 0U)
	    && probe->client == NULL && probe->server == NULL;
	if (probe->io_initialized)
		released = released && atomic_load(&ctx->io.c2s.rfd) == -1
		    && atomic_load(&ctx->io.c2s.wfd) == -1
		    && atomic_load(&ctx->io.s2c.rfd) == -1
		    && atomic_load(&ctx->io.s2c.wfd) == -1;
	return released;
}

static int
test_auth_setup_ownership(void)
{
	/* Actual registration allocation, then each actual session allocation.
	 * Every failed setup remains safe for normal and repeated caller cleanup. */
	for (unsigned target = 0; target <= 2; target++) {
		struct auth_setup_allocation_probe probe = {.target = target};
		if (target == 0)
			dssh_test_alloc_fail_after(0);
		else
			dssh_test_alloc_observe_allocations(auth_setup_fail_session, &probe);
		struct handshake_ctx ctx;
		int ret = handshake_setup(&ctx);
		bool io_acquired = ctx.io_initialized;
		dssh_test_alloc_reset();
		struct auth_setup_retirement retirement;
		auth_setup_watch_retirement(&ctx, &retirement);
		handshake_cleanup(&ctx);
		bool empty = auth_setup_empty(&ctx);
		handshake_cleanup(&ctx);
		bool retired = auth_setup_retirement_ok(&ctx, &retirement);
		ASSERT_TRUE(ret < 0);
		ASSERT_TRUE(empty && retired);
		ASSERT_EQ_U(probe.sessions, target);
		ASSERT_EQ(probe.failed, target != 0);
		ASSERT_EQ(io_acquired, target != 0);
	}

	/* Native creation succeeds except for the selected first/second call.
	 * The real first worker must finish before failure is returned. */
	for (unsigned fail_at = 0; fail_at <= 2; fail_at++) {
		struct auth_setup_thread_probe probe = {.fail_at = fail_at};
		atomic_init(&probe.workers[0].done, false);
		atomic_init(&probe.workers[1].done, false);
		auth_setup_threads = &probe;
		auth_fixture_thread_create = auth_setup_create;
		struct handshake_ctx ctx;
		int ret = handshake_setup(&ctx);
		auth_fixture_thread_create = thrd_create;
		auth_setup_threads = NULL;
		bool complete = true;
		for (unsigned i = 0; i < probe.created; i++)
			complete = complete && atomic_load(&probe.workers[i].done);
		bool client_failed = fail_at != 1 || ctx.client_result < 0;
		bool both_succeeded = fail_at != 2 || (ctx.client_result == 0 && ctx.server_result == 0);
		struct auth_setup_retirement retirement;
		auth_setup_watch_retirement(&ctx, &retirement);
		handshake_cleanup(&ctx);
		handshake_cleanup(&ctx);
		bool retired = auth_setup_retirement_ok(&ctx, &retirement);
		ASSERT_EQ_U(probe.calls, fail_at == 0 ? 1 : 2);
		ASSERT_EQ_U(probe.created, fail_at);
		ASSERT_EQ(ret, fail_at == 2 ? 0 : -1);
		ASSERT_TRUE(complete && client_failed && both_succeeded);
		ASSERT_TRUE(retired);
	}
	return TEST_PASS;
}

static int
test_auth_nested_setup_ownership(void)
{
	for (unsigned target = 0; target <= 2; target++) {
		struct auth_setup_allocation_probe probe = {.target = target};
		if (target == 0)
			dssh_test_alloc_fail_after(0);
		else
			dssh_test_alloc_observe_allocations(auth_setup_fail_session, &probe);
		struct parse_error_ctx pe;
		int ret = parse_error_setup(&pe);
		dssh_test_alloc_reset();
		struct auth_setup_retirement retirement;
		auth_setup_watch_retirement(&pe.hctx, &retirement);
		parse_error_cleanup(&pe);
		parse_error_cleanup(&pe);
		bool retired = auth_setup_retirement_ok(&pe.hctx, &retirement);
		ASSERT_TRUE(ret < 0);
		ASSERT_EQ_U(probe.sessions, target);
		ASSERT_EQ(probe.failed, target != 0);
		ASSERT_TRUE(retired);
		ASSERT_FALSE(pe.thread_started);
	}
	for (unsigned fail_at = 0; fail_at <= 3; fail_at++) {
		struct auth_setup_thread_probe probe = {.fail_at = fail_at};
		for (size_t i = 0; i < 3; i++)
			atomic_init(&probe.workers[i].done, false);
		auth_setup_threads = &probe;
		auth_fixture_thread_create = auth_setup_create;
		struct parse_error_ctx pe;
		int ret = parse_error_setup(&pe);
		auth_fixture_thread_create = thrd_create;
		auth_setup_threads = NULL;
		struct auth_setup_retirement retirement;
		auth_setup_watch_retirement(&pe.hctx, &retirement);
		parse_error_cleanup(&pe);
		parse_error_cleanup(&pe);
		bool retired = auth_setup_retirement_ok(&pe.hctx, &retirement);
		bool complete = true;
		for (unsigned i = 0; i < probe.created; i++)
			complete = complete && atomic_load(&probe.workers[i].done);
		ASSERT_EQ_U(probe.calls, fail_at < 2 ? fail_at + 1 : 3);
		ASSERT_EQ_U(probe.created, fail_at);
		ASSERT_EQ(ret, fail_at == 3 ? 0 : -1);
		ASSERT_TRUE(complete && retired);
		ASSERT_FALSE(pe.thread_started);
	}
	return TEST_PASS;
}

#endif
