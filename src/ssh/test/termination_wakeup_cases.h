/* Native waiters around the real library predicates. These are isolated
 * synchronization fixtures, not protocol or platform interoperability tests. */
struct wake_fixture {
	dssh_session sess;
	struct dssh_channel_s channel;
	struct dssh_channel_id protocol_id;
	struct dssh_channel_s *entries[1];
	struct dssh_tx_slot slot;
	struct dssh_incoming_open incoming;
	mtx_t *mutex;
	cnd_t *condition;
	atomic_bool done;
	atomic_uint notices;
	unsigned kind;
	unsigned initialized;
	bool started;
	bool owner;
	thrd_t worker;
	int result;
};

static void
wake_notice(dssh_session sess, void *opaque)
{
	(void)sess;
	struct wake_fixture *fixture = opaque;
	atomic_fetch_add(&fixture->notices, 1);
}

static int
wake_waiter(void *opaque)
{
	struct wake_fixture *fixture = opaque;
	static const uint8_t ignore[] = {SSH_MSG_IGNORE, 0, 0, 0, 0};
	if (fixture->kind == 0)
		fixture->result = tx_acquire(fixture->sess, SSH_MSG_USERAUTH_REQUEST);
	else if (fixture->kind == 1)
		fixture->result = send_to_slot(fixture->sess, &fixture->slot, ignore, sizeof(ignore));
	else if (fixture->kind == 2)
		fixture->result = accept_incoming(fixture->sess, &fixture->incoming, -1);
	else
		fixture->result = dssh_chan_poll(&fixture->channel, DSSH_POLL_READ, -1);
	atomic_store(&fixture->done, true);
	return 0;
}

static bool
wake_finish(struct wake_fixture *fixture)
{
	dssh_test_ossl_reset();
	if (fixture->sess == NULL)
		return true;
	/* Capture assertions before this fallback: it must not mask a missed
	 * notification in the preserved before implementation. */
	session_set_terminate(fixture->sess);
	bool valid = true;
	if (fixture->started) {
		int result;
		if (thrd_join(fixture->worker, &result) != thrd_success)
			abort();
		valid = result == 0 && atomic_load(&fixture->done);
	}
	dssh_test_thrd_watch_condition(NULL);
	if (fixture->owner)
		tx_unlock_with_slots(fixture->sess);
	fixture->sess->channels = NULL;
	fixture->sess->channel_ids = NULL;
	fixture->sess->channel_count = 0;
	fixture->sess->channel_capacity = 0;
	atomic_store(&fixture->sess->conn_initialized, false);
	atomic_store(&fixture->sess->demux_running, false);
	if (fixture->initialized >= 6)
		cnd_destroy(&fixture->channel.poll_cnd);
	if (fixture->initialized >= 5)
		mtx_destroy(&fixture->channel.cb_mtx);
	if (fixture->initialized >= 4)
		mtx_destroy(&fixture->channel.buf_mtx);
	if (fixture->initialized >= 3)
		cnd_destroy(&fixture->sess->accept_cnd);
	if (fixture->initialized >= 2)
		mtx_destroy(&fixture->sess->channel_mtx);
	if (fixture->initialized >= 1)
		mtx_destroy(&fixture->sess->accept_mtx);
	free_tx_slot(&fixture->slot);
	dssh_session_cleanup(fixture->sess);
	fixture->sess = NULL;
	return valid;
}

static bool
wake_init(struct wake_fixture *fixture, unsigned kind)
{
	dssh_test_ossl_reset();
	fixture->kind = kind;
	atomic_init(&fixture->done, false);
	atomic_init(&fixture->notices, 0);
	atomic_init(&fixture->slot.ready, false);
	fixture->sess = dssh_session_init(false, 0);
	if (fixture->sess == NULL)
		return false;
	if (dssh_session_set_terminate_cb(fixture->sess, wake_notice, fixture) != 0)
		return false;
	if (mtx_init(&fixture->sess->accept_mtx, mtx_plain) != thrd_success)
		return false;
	fixture->initialized = 1;
	if (mtx_init(&fixture->sess->channel_mtx, mtx_plain) != thrd_success)
		return false;
	fixture->initialized = 2;
	if (cnd_init(&fixture->sess->accept_cnd) != thrd_success)
		return false;
	fixture->initialized = 3;
	fixture->protocol_id = (struct dssh_channel_id){.refs = 1,
	    .phase = DSSH_CHANNEL_ID_CONFIRMED, .local_assigned = true};
	fixture->channel.sess = fixture->sess;
	fixture->channel.protocol_id = &fixture->protocol_id;
	fixture->channel.open = true;
	fixture->channel.chan_type = DSSH_CHAN_SESSION;
	fixture->channel.io_model = DSSH_IO_STREAM;
	atomic_init(&fixture->channel.tx_refs, 0);
	atomic_init(&fixture->channel.terminate_woken, false);
	atomic_init(&fixture->channel.dispatch_state, DSSH_CHANNEL_DISPATCH_IDLE);
	atomic_init(&fixture->channel.remote_window, 0);
	if (mtx_init(&fixture->channel.buf_mtx, mtx_plain) != thrd_success)
		return false;
	fixture->initialized = 4;
	if (mtx_init(&fixture->channel.cb_mtx, mtx_plain) != thrd_success)
		return false;
	fixture->initialized = 5;
	if (cnd_init(&fixture->channel.poll_cnd) != thrd_success)
		return false;
	fixture->initialized = 6;
	fixture->entries[0] = &fixture->channel;
	fixture->sess->channels = fixture->entries;
	fixture->sess->channel_ids = &fixture->protocol_id;
	fixture->sess->channel_count = 1;
	fixture->sess->channel_capacity = 1;
	atomic_store(&fixture->sess->conn_initialized, true);
	/* No library worker is fabricated or joined. Only wake_waiter exists;
	 * the connection flag supplies accept_incoming's live-worker predicate. */
	atomic_store(&fixture->sess->demux_running, true);
	return true;
}

static bool
wake_start(struct wake_fixture *fixture, unsigned kind)
{
	if (!wake_init(fixture, kind))
		return false;
	if (kind == 0) {
		atomic_store(&fixture->sess->trans.rekey_in_progress, true);
		fixture->mutex = &fixture->sess->trans.tx_mtx;
		fixture->condition = &fixture->sess->trans.rekey_cnd;
	}
	else if (kind == 1) {
		if (alloc_tx_slot(&fixture->slot, 5, 8, 0) != 0)
			return false;
		atomic_store(&fixture->slot.ready, true);
		if (tx_acquire(fixture->sess, SSH_MSG_IGNORE) != 0)
			return false;
		fixture->owner = true;
		fixture->mutex = &fixture->sess->trans.tx_queue_mtx;
		fixture->condition = &fixture->sess->trans.tx_slot_cnd;
	}
	else if (kind == 2) {
		fixture->mutex = &fixture->sess->accept_mtx;
		fixture->condition = &fixture->sess->accept_cnd;
	}
	else {
		fixture->mutex = &fixture->channel.buf_mtx;
		fixture->condition = &fixture->channel.poll_cnd;
	}
	atomic_bool *entered = dssh_test_thrd_watch_condition(fixture->condition);
	if (thrd_create(&fixture->worker, wake_waiter, fixture) != thrd_success)
		return false;
	fixture->started = true;
	struct timespec begin;
	if (timespec_get(&begin, TIME_UTC) != TIME_UTC)
		return false;
	while (!atomic_load(entered) && !atomic_load(&fixture->done)) {
		struct timespec now;
		if (timespec_get(&now, TIME_UTC) != TIME_UTC || now.tv_sec < begin.tv_sec)
			return false;
		long double now_seconds = (long double)now.tv_sec;
		long double begin_seconds = (long double)begin.tv_sec;
		if (now_seconds - begin_seconds >= 2)
			return false;
		thrd_yield();
	}
	if (!atomic_load(entered))
		return false;
	/* The observation occurs before native cnd_wait. Acquiring the associated
	 * mutex establishes that the waiter really released it to wait. */
	if (mtx_lock(fixture->mutex) != thrd_success)
		return false;
	if (mtx_unlock(fixture->mutex) != thrd_success)
		abort();
	return !atomic_load(&fixture->done);
}

static bool
wake_completed(struct wake_fixture *fixture)
{
	return atomic_load(&fixture->sess->terminate)
	    && atomic_load(&fixture->sess->terminate_notified)
	    && atomic_load(&fixture->sess->terminate_wake_started)
	    && atomic_load(&fixture->notices) == 1;
}

static int
wake_case(unsigned kind, bool deferred)
{
	struct wake_fixture fixture = {.sess = NULL};
	bool valid = wake_start(&fixture, kind);
	if (valid) {
		if (deferred) {
			session_set_terminate_no_wake(fixture.sess);
			dssh_thrd_check(fixture.sess, thrd_error);
		}
		else
			dssh_session_terminate(fixture.sess);
		valid = wake_completed(&fixture);
	}
	bool finished = wake_finish(&fixture);
	ASSERT_TRUE(valid && finished);
	ASSERT_EQ(fixture.result, kind == 3 ? DSSH_POLL_READ : DSSH_ERROR_TERMINATED);
	return TEST_PASS;
}

#define WAKE_TEST(name, kind, deferred) \
	static int name(void) \
	{ \
		return wake_case(kind, deferred); \
	}
WAKE_TEST(test_wake_rekey, 0, false)
WAKE_TEST(test_wake_slot, 1, false)
WAKE_TEST(test_wake_accept, 2, false)
WAKE_TEST(test_wake_poll, 3, false)
WAKE_TEST(test_deferred_rekey, 0, true)
WAKE_TEST(test_deferred_slot, 1, true)
WAKE_TEST(test_deferred_accept, 2, true)
WAKE_TEST(test_deferred_poll, 3, true)
#undef WAKE_TEST

static int
wake_retry(bool deferred)
{
	struct wake_fixture fixture = {.sess = NULL};
	bool valid = wake_start(&fixture, 0);
	if (valid) {
		if (deferred)
			session_set_terminate_no_wake(fixture.sess);
		dssh_test_wake_condition(fixture.condition, 1, false, false);
		tx_wake_owners(fixture.sess);
		valid = wake_completed(&fixture) && dssh_test_wake_hits() == 1;
	}
	bool finished = wake_finish(&fixture);
	ASSERT_TRUE(valid && finished);
	ASSERT_ERR(fixture.result, DSSH_ERROR_TERMINATED);
	return TEST_PASS;
}

static int
test_wake_retry(void)
{
	return wake_retry(false);
}

static int
test_deferred_retry(void)
{
	return wake_retry(true);
}

static int
test_demux_exit_deferred(void)
{
	struct wake_fixture fixture = {.sess = NULL};
	bool valid = wake_start(&fixture, 0);
	if (valid) {
		session_set_terminate_no_wake(fixture.sess);
		int ret = demux_thread_func(fixture.sess);
		valid = ret == 0 && !fixture.sess->demux_running
		    && !session_on_demux_thread(fixture.sess) && wake_completed(&fixture);
	}
	bool finished = wake_finish(&fixture);
	ASSERT_TRUE(valid && finished);
	ASSERT_ERR(fixture.result, DSSH_ERROR_TERMINATED);
	return TEST_PASS;
}

static int
test_signal_retry(void)
{
	struct wake_fixture fixture = {.sess = NULL};
	bool valid = wake_start(&fixture, 2);
	if (valid) {
		uint8_t payload[32] = {SSH_MSG_CHANNEL_OPEN};
		size_t pos = 1;
		int ret = dssh_serialize_uint32(7, payload, sizeof(payload), &pos);
		if (ret == 0) {
			memcpy(payload + pos, "session", 7);
			pos += 7;
			ret = dssh_serialize_uint32(123, payload, sizeof(payload), &pos);
		}
		if (ret == 0)
			ret = dssh_serialize_uint32(32768, payload, sizeof(payload), &pos);
		if (ret == 0)
			ret = dssh_serialize_uint32(32768, payload, sizeof(payload), &pos);
		if (ret == 0) {
			dssh_test_wake_condition(fixture.condition, 1, false, false);
			ret = dssh_test_demux_dispatch(fixture.sess, SSH_MSG_CHANNEL_OPEN, payload, pos);
		}
		valid = ret == DSSH_ERROR_TERMINATED && wake_completed(&fixture)
		    && dssh_test_wake_hits() == 1;
	}
	bool finished = wake_finish(&fixture);
	ASSERT_TRUE(valid && finished);
	/* A legitimate spurious wake may consume the queued open before the
	 * controlling thread completes termination. Either outcome is valid. */
	ASSERT_TRUE(fixture.result == DSSH_ERROR_TERMINATED
	    || (fixture.result == 0 && fixture.incoming.peer_channel == 123));
	return TEST_PASS;
}

static int
wake_fatal_child(const char *name, const char *mode)
{
	bool after = strcmp(mode, "after") == 0;
	if (!after && strcmp(mode, "before") != 0)
		return 2;
	unsigned kind = 3;
	if (strcmp(name, "lock-tx") == 0 || strcmp(name, "notify-tx") == 0
	    || strcmp(name, "retry-notify") == 0)
		kind = 0;
	else if (strcmp(name, "lock-slot") == 0 || strcmp(name, "notify-slot") == 0)
		kind = 1;
	else if (strcmp(name, "lock-accept") == 0 || strcmp(name, "notify-accept") == 0)
		kind = 2;
	else if (strcmp(name, "lock-registry") != 0 && strcmp(name, "lock-channel") != 0
	    && strcmp(name, "notify-channel") != 0)
		return 2;
	struct wake_fixture fixture = {.sess = NULL};
	if (!wake_start(&fixture, kind))
		_Exit(3);
	if (strncmp(name, "lock-", 5) == 0) {
		mtx_t *target = strcmp(name, "lock-registry") == 0
		    ? &fixture.sess->channel_mtx : fixture.mutex;
		dssh_test_wake_lock(target, after);
	}
	else
		dssh_test_wake_condition(fixture.condition,
		    strcmp(name, "retry-notify") == 0 ? 2U : 1U, after, true);
	if (strcmp(name, "retry-notify") == 0)
		tx_wake_owners(fixture.sess);
	else
		dssh_session_terminate(fixture.sess);
	/* Do not clean up a before implementation with an owned failed mutex or
	 * an unwoken native waiter. Only the driver may judge this normal exit. */
	_Exit(4);
}
