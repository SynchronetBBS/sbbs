#ifndef DSSH_TEST_CHANNEL_WORKER_CASES_H
#define DSSH_TEST_CHANNEL_WORKER_CASES_H
#define CHANNEL_WORKER_PROBE_CAP 16

struct channel_worker_job {
	thrd_start_t function;
	void *data;
	atomic_bool done;
};
struct channel_worker_probe {
	unsigned calls;
	unsigned created;
	unsigned joined;
	bool cancelled_before_join;
	unsigned fail_at;
	bool creation_failed;
	unsigned fatal_join;
	bool fatal_released;
	struct channel_worker_job jobs[CHANNEL_WORKER_PROBE_CAP];
};
static struct channel_worker_probe *channel_worker_probe;

static int
channel_worker_probe_run(void *data)
{
	struct channel_worker_job *job = data;
	int ret = job->function(job->data);
	atomic_store(&job->done, true);
	return ret;
}

static int
channel_worker_probe_create(thrd_t *thread, thrd_start_t function, void *data)
{
	struct channel_worker_probe *probe = channel_worker_probe;
	if (probe->calls >= CHANNEL_WORKER_PROBE_CAP)
		abort();
	unsigned index = probe->calls++;
	if (index == probe->fail_at) {
		probe->creation_failed = true;
		return thrd_error;
	}
	if (index >= CHANNEL_WORKER_PROBE_CAP)
		abort();
	struct channel_worker_job *job = &probe->jobs[index];
	job->function = function;
	job->data = data;
	int ret = thrd_create(thread, channel_worker_probe_run, job);
	if (ret == thrd_success)
		probe->created++;
	return ret;
}

static int
channel_worker_probe_join(thrd_t thread, int *result)
{
	struct channel_worker_probe *probe = channel_worker_probe;
	struct conn_ctx *owner = channel_fixture_owner;
	if (probe->creation_failed) {
		bool cancelled = owner != NULL && dssh_session_is_terminated(owner->client)
		    && dssh_session_is_terminated(owner->server);
		probe->cancelled_before_join = probe->cancelled_before_join && cancelled;
		/* Retain the observation, then finish the before implementation
		 * without requiring a timed wait or retiring borrowed stack data. */
		if (!cancelled && owner != NULL) {
			dssh_session_terminate(owner->client);
			dssh_session_terminate(owner->server);
			mock_io_close_c2s(&owner->io);
			mock_io_close_s2c(&owner->io);
		}
	}
	if (probe->fatal_join != 0 && !probe->fatal_released
	    && probe->joined < UINT_MAX && probe->joined + 1 == probe->fatal_join) {
		fputs("DSSH_TEST_CHANNEL_JOIN_FAILURE\n", stderr);
		return thrd_error;
	}
	int ret = thrd_join(thread, result);
	if (ret == thrd_success) {
		if (probe->joined >= probe->created)
			abort();
		probe->joined++;
	}
	if (ret == thrd_success && probe->fatal_join != 0
	    && probe->joined == probe->fatal_join) {
		fputs("DSSH_TEST_CHANNEL_JOIN_FAILURE\n", stderr);
		return thrd_error;
	}
	return ret;
}

static void
channel_worker_probe_begin(struct channel_worker_probe *probe)
{
	for (size_t i = 0; i < CHANNEL_WORKER_PROBE_CAP; i++)
		atomic_init(&probe->jobs[i].done, false);
	channel_worker_probe = probe;
	channel_fixture_thread_create = channel_worker_probe_create;
	channel_fixture_thread_join = channel_worker_probe_join;
}

static void
channel_worker_probe_end(void)
{
	channel_fixture_thread_create = thrd_create;
	channel_fixture_thread_join = thrd_join;
	channel_worker_probe = NULL;
}

static int
channel_worker_consumer(unsigned kind)
{
	if (kind == 1)
		return test_open_exec_alloc_sweep();
	if (kind == 2)
		return test_open_shell_alloc_sweep();
	if (kind == 3)
		return test_open_subsystem_alloc_sweep();
	if (kind == 4)
		return test_server_accept_alloc_sweep();
	struct conn_ctx ctx;
	if (conn_setup(&ctx) != 0)
		return TEST_FAIL;
	struct open_exec_ctx open = {.client = ctx.client, .server = ctx.server,
	    .command = kind == 5 ? NULL : "worker-ownership",
	    .cbs = kind == 6 ? &reject_exec_cbs : &accept_cbs_all,
	    .accept_timeout = kind == 6 ? 0 : 30000};
	int ret = open_exec_channel(&ctx, &open);
	bool valid = ret == 0;
	if (ret == 0) {
		const uint8_t expected = 47;
		uint8_t received = 0;
		valid = dssh_chan_write(open.client_ch, 0, &expected, 1) == 1
		    && (dssh_chan_poll(open.server_ch, DSSH_POLL_READ, 5000) & DSSH_POLL_READ) != 0
		    && dssh_chan_read(open.server_ch, 0, &received, 1) == 1 && received == expected;
		const char *kex = dssh_transport_get_kex_name(ctx.client);
		const char *key = dssh_transport_get_hostkey_name(ctx.client);
		valid = valid && kex != NULL && key != NULL;
		if (kex != NULL && key != NULL)
			printf("channel_worker_profile kex=%s key=%s\n", kex, key);
	}
	conn_cleanup(&ctx);
	return valid ? TEST_PASS : TEST_FAIL;
}

static int
test_channel_worker_ownership(void)
{
	unsigned schedules = 0;
	for (unsigned kind = 0; kind < 5; kind++) {
		for (unsigned fail_at = 0; fail_at < 2; fail_at++) {
			struct channel_worker_probe probe = {.fail_at = fail_at, .cancelled_before_join = true};
			channel_worker_probe_begin(&probe);
			int ret = channel_worker_consumer(kind);
			channel_worker_probe_end();
			bool valid = ret == TEST_FAIL && probe.calls == fail_at + 1
			    && probe.created == fail_at && probe.joined == fail_at
			    && probe.creation_failed && probe.cancelled_before_join
			    && channel_fixture_owner == NULL && g_active_ctx == NULL;
			for (unsigned i = 0; i < probe.created; i++)
				valid = valid && atomic_load(&probe.jobs[i].done);
			if (!valid)
				fprintf(stderr, "channel worker kind=%u fail_at=%u ret=%d calls=%u created=%u joined=%u\n",
				    kind, fail_at, ret, probe.calls, probe.created, probe.joined);
			ASSERT_TRUE(valid);
			schedules++;
		}
	}
	/* Failed builders and immediate rejecting/empty accepts must wake the other
	 * actual worker. Ordinary success additionally exchanges a real byte. */
	for (unsigned kind = 5; kind <= 7; kind++) {
		struct channel_worker_probe probe = {.fail_at = 2, .cancelled_before_join = true};
		channel_worker_probe_begin(&probe);
		int ret = channel_worker_consumer(kind);
		channel_worker_probe_end();
		bool valid = ret == (kind == 7 ? TEST_PASS : TEST_FAIL)
		    && probe.calls == 2 && probe.created == 2 && probe.joined == 2
		    && atomic_load(&probe.jobs[0].done) && atomic_load(&probe.jobs[1].done)
		    && channel_fixture_owner == NULL && g_active_ctx == NULL;
		ASSERT_TRUE(valid);
		schedules++;
	}
	printf("channel_worker_ownership schedules=%u\n", schedules);
	return TEST_PASS;
}

static int
channel_worker_fatal_child(const char *operation, const char *mode)
{
	unsigned kind;
	if (strcmp(operation, "open") == 0)
		kind = 0;
	else if (strcmp(operation, "exec") == 0)
		kind = 1;
	else if (strcmp(operation, "shell") == 0)
		kind = 2;
	else if (strcmp(operation, "subsystem") == 0)
		kind = 3;
	else if (strcmp(operation, "accept") == 0)
		kind = 4;
	else
		return 2;
	if (strcmp(mode, "retained") != 0 && strcmp(mode, "released") != 0)
		return 2;
	struct channel_worker_probe probe = {.fail_at = 2, .cancelled_before_join = true,
	    .fatal_join = 1, .fatal_released = strcmp(mode, "released") == 0};
	channel_worker_probe_begin(&probe);
	channel_worker_consumer(kind);
	channel_worker_probe_end();
	return 4;
}

#endif
