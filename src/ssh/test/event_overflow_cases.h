/* Exercise event overflow while another thread owns the actual packet engine.
 * No physical mutex is held across the dispatch or controller waits. */
struct overflow_job {
	struct event_fixture *fixture;
	int type;
	unsigned mode;
	int result;
	unsigned notify_hits;
	unsigned rejected_unlocks;
	atomic_bool done;
};

static int
overflow_worker(void *opaque)
{
	struct overflow_job *job = opaque;
	struct dssh_channel_s *ch = job->fixture->open.server_ch;
	if (job->mode < 2)
		dssh_test_wake_condition(&ch->poll_cnd, 1, job->mode == 1, false);
	else if (job->mode == 2)
		dssh_test_mtx_reject_once(&ch->sess->trans.tx_mtx);
	job->result = event_send(job->fixture, job->type, true);
	job->notify_hits = dssh_test_wake_hits();
	job->rejected_unlocks = dssh_test_mtx_rejected_unlocks();
	dssh_test_thrd_reset();
	atomic_store(&job->done, true);
	return 0;
}

static bool
overflow_case(int type, unsigned mode)
{
	struct event_fixture fixture = {0};
	if (!event_setup(&fixture, NULL, false)) {
		dssh_test_failure_cleanup();
		return false;
	}
	struct dssh_channel_s *ch = fixture.open.server_ch;
	dssh_session sess = fixture.ctx.server;
	struct overflow_job job = {.fixture = &fixture, .type = type, .mode = mode};
	atomic_init(&job.done, false);
	struct flow_fixture flow;
	flow_init(&flow, &fixture.ctx, NULL, NULL);
	ASSERT_TRUE(storage_checkpoint(ST_OVERFLOW_PREPARED));
	ASSERT_EQ(dssh_chan_set_max_events(ch, 1), 0);
	ASSERT_EQ(event_send(&fixture, DSSH_EVENT_SIGNAL, true), 0);
	ASSERT_EQ(tx_acquire(sess, SSH_MSG_IGNORE), 0);
	flow.tx.session = sess;
	flow.tx.held = true;
	ASSERT_TRUE(storage_checkpoint(ST_OVERFLOW_HELD));
	atomic_bool *waiting = dssh_test_thrd_watch_condition(&sess->trans.rekey_cnd);
	flow.watching = true;
	ASSERT_EQ(channel_worker_start(&flow.workers, 0, overflow_worker, &job), 0);
	ASSERT_TRUE(storage_checkpoint(ST_OVERFLOW_STARTED));
	struct timespec start;
	ASSERT_EQ(clock_gettime(CLOCK_MONOTONIC, &start), 0);
	bool valid = true;
	while (!atomic_load(&job.done) && !atomic_load(waiting)) {
		if (!dssh_test_wait_tick(&start)) {
			valid = false;
			break;
		}
	}
	bool done = atomic_load(&job.done);
	bool blocked = atomic_load(waiting);
	bool terminated = dssh_session_is_terminated(sess);
	if (mode == 3) {
		valid = valid && !done && blocked && !terminated;
		ASSERT_TRUE(valid);
		ASSERT_TRUE(conn_actor_waiting(waiting, &sess->trans.tx_mtx));
		ASSERT_TRUE(storage_checkpoint(ST_OVERFLOW_WAITING));
	}
	else {
		valid = valid && done && !blocked && terminated
		    && atomic_load(&sess->terminate_notified)
		    && atomic_load(&sess->terminate_wake_started);
		ASSERT_TRUE(valid);
		ASSERT_TRUE(storage_checkpoint(ST_OVERFLOW_DONE));
	}
	callback_release_tx(&flow.tx);
	flow_finish(&flow);
	ASSERT_EQ(flow.workers.results[0], 0);
	ASSERT_EQ(flow.tx.result, mode == 3 ? 0 : DSSH_ERROR_TERMINATED);
	if (mtx_lock(&ch->buf_mtx) != thrd_success)
		abort();
	bool close_sent = ch->close_sent;
	if (mtx_unlock(&ch->buf_mtx) != thrd_success)
		abort();
	valid = valid && job.rejected_unlocks == 0 && atomic_load(&job.done);
	if (mode < 2)
		valid = valid && job.result == DSSH_ERROR_TERMINATED
		    && job.notify_hits == 1 && !close_sent;
	else if (mode == 2)
		valid = valid && job.result == DSSH_ERROR_TERMINATED
		    && job.notify_hits == 0 && close_sent;
	else
		valid = valid && job.result == 0 && close_sent && !sess->terminate;
	valid = valid && atomic_load(&ch->dispatch_state) == DSSH_CHANNEL_DISPATCH_IDLE;
	ASSERT_TRUE(valid);
	ASSERT_TRUE(storage_checkpoint(ST_OVERFLOW_FINISHED));
	conn_cleanup(&fixture.ctx);
	return valid;
}

static int
overflow_notification(int type)
{
	bool valid = true;
	for (unsigned mode = 0; mode < 2; mode++) {
		bool result = overflow_case(type, mode);
		if (!result)
			return TEST_FAIL;
		valid = valid && result;
	}
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

#define OVERFLOW_TEST(name, type, mode) \
	static int name(void) \
	{ \
		ASSERT_TRUE(overflow_case(type, mode)); \
		return TEST_PASS; \
	}
OVERFLOW_TEST(test_overflow_request_close_failure, DSSH_EVENT_SIGNAL, 2)
OVERFLOW_TEST(test_overflow_eof_close_failure, DSSH_EVENT_EOF, 2)
OVERFLOW_TEST(test_overflow_close_close_failure, DSSH_EVENT_CLOSE, 2)
OVERFLOW_TEST(test_overflow_request_normal, DSSH_EVENT_SIGNAL, 3)
OVERFLOW_TEST(test_overflow_eof_normal, DSSH_EVENT_EOF, 3)
OVERFLOW_TEST(test_overflow_close_normal, DSSH_EVENT_CLOSE, 3)
#undef OVERFLOW_TEST

static int
test_overflow_request_notification(void)
{
	return overflow_notification(DSSH_EVENT_SIGNAL);
}

static int
test_overflow_eof_notification(void)
{
	return overflow_notification(DSSH_EVENT_EOF);
}

static int
test_overflow_close_notification(void)
{
	return overflow_notification(DSSH_EVENT_CLOSE);
}
