/* Each numbered path is one actual production wait site. Fixtures initialize
 * real synchronization objects and a real session, but no fake joinable demux
 * thread or protocol transcript. Controller operations use native test calls. */
struct wait_contract_fixture {
	struct wake_fixture wake;
	struct dssh_test_wait_probe probe;
	unsigned path;
	unsigned mode;
	int timeout_ms;
	uint8_t packet[9];
};

static bool
wait_contract_timed(unsigned path)
{
	return path == 3 || path == 5 || path == 7 || path == 9 || path == 11 || path == 13;
}

static int
wait_contract_worker(void *opaque)
{
	struct wait_contract_fixture *fixture = opaque;
	struct wake_fixture *wake = &fixture->wake;
	struct dssh_channel_s *ch = &wake->channel;
	dssh_test_wait_probe(&fixture->probe);
	switch (fixture->path) {
		case 0:
		case 1:
			wake->result = dssh_test_demux_dispatch(wake->sess, fixture->packet[0],
			    fixture->packet, fixture->path == 0 ? 5 : sizeof(fixture->packet));
			break;
		case 2:
		case 3:
			wake->result = accept_incoming(wake->sess, &wake->incoming,
			    fixture->path == 2 ? -1 : fixture->timeout_ms);
			break;
		case 4:
		case 5:
			wake->result = wait_open_response(wake->sess, ch);
			break;
		case 6:
		case 7:
			wake->result = wait_channel_request_response(wake->sess, ch);
			break;
		case 8:
		case 9: {
			uint8_t type;
			uint8_t *payload;
			size_t len;
			wake->result = setup_recv(wake->sess, ch, &type, &payload, &len);
			break;
		}
		case 10:
		case 11:
			wake->result = dssh_chan_poll(ch, DSSH_POLL_READ,
			    fixture->path == 10 ? -1 : fixture->timeout_ms);
			break;
		case 12:
		case 13:
			wake->result = tx_acquire(wake->sess, SSH_MSG_USERAUTH_REQUEST);
			break;
		case 14: {
			static const uint8_t ignore[] = {SSH_MSG_IGNORE, 0, 0, 0, 0};
			wake->result = send_to_slot(wake->sess, &wake->slot, ignore, sizeof(ignore));
			break;
		}
		default:
			wake->result = DSSH_ERROR_INVALID;
			break;
	}
	dssh_test_wait_probe(NULL);
	atomic_store(&wake->done, true);
	return 0;
}

static bool
wait_contract_tick(const struct timespec *begin)
{
	struct timespec now;
	if (timespec_get(&now, TIME_UTC) != TIME_UTC)
		return false;
	long double current = (long double)now.tv_sec;
	long double start = (long double)begin->tv_sec;
	if (current < start || current - start >= 5)
		return false;
	thrd_yield();
	return true;
}

static bool
wait_contract_init(struct wait_contract_fixture *fixture, unsigned path, unsigned mode)
{
	fixture->path = path;
	fixture->mode = mode;
	if (!wake_init(&fixture->wake, 3))
		return false;
	struct wake_fixture *wake = &fixture->wake;
	struct dssh_channel_s *ch = &wake->channel;
	bool timed = wait_contract_timed(path);
	fixture->timeout_ms = timed ? (mode == 2 || mode == 4 ? 10 : 30000) : 0;
	wake->sess->timeout_ms = fixture->timeout_ms;
	wake->mutex = &ch->buf_mtx;
	wake->condition = &ch->poll_cnd;
	if (path == 0) {
		ch->setup_mode = true;
		ch->setup_ready = true;
		fixture->packet[0] = SSH_MSG_CHANNEL_EOF;
	}
	else if (path == 1) {
		ch->window_adjust_inflight = 1;
		fixture->packet[0] = SSH_MSG_CHANNEL_DATA;
	}
	else if (path == 2 || path == 3) {
		wake->mutex = &wake->sess->accept_mtx;
		wake->condition = &wake->sess->accept_cnd;
	}
	else if (path == 4 || path == 5) {
		ch->open = false;
		ch->open_pending = true;
	}
	else if (path == 6 || path == 7)
		ch->request_pending = true;
	else if (path == 12 || path == 13) {
		atomic_store(&wake->sess->trans.rekey_in_progress, true);
		wake->mutex = &wake->sess->trans.tx_mtx;
		wake->condition = &wake->sess->trans.rekey_cnd;
	}
	else if (path == 14) {
		if (alloc_tx_slot(&wake->slot, 5, 8, 0) != 0)
			return false;
		atomic_store(&wake->slot.ready, true);
		if (tx_acquire(wake->sess, SSH_MSG_IGNORE) != 0)
			return false;
		wake->owner = true;
		wake->mutex = &wake->sess->trans.tx_queue_mtx;
		wake->condition = &wake->sess->trans.tx_slot_cnd;
	}
	struct dssh_test_wait_probe *probe = &fixture->probe;
	probe->condition = wake->condition;
	probe->mutex = wake->mutex;
	probe->complete_real = mode != 0;
	probe->until_timeout = mode == 2 || mode == 4;
	probe->result = mode == 3 ? thrd_success : mode == 4 ? thrd_timedout : thrd_error;
	atomic_init(&probe->calls, 0);
	atomic_init(&probe->native_calls, 0);
	atomic_init(&probe->native_result, thrd_busy);
	atomic_init(&probe->returning, false);
	atomic_init(&probe->release, false);
	return true;
}

/* Supply a native signal only after acquiring the mutex released by the real
 * wait. A permitted native unspecified wake can reach the return barrier first;
 * do not block acquiring the mutex from that paused returning waiter. */
static bool
wait_contract_signal(struct wait_contract_fixture *fixture, unsigned calls)
{
	struct timespec begin;
	if (timespec_get(&begin, TIME_UTC) != TIME_UTC)
		return false;
	struct dssh_test_wait_probe *probe = &fixture->probe;
	for (;;) {
		if (atomic_load(&probe->returning) && calls == 1)
			return true;
		if (atomic_load(&probe->native_calls) >= calls) {
			int ret = mtx_trylock(probe->mutex);
			if (ret == thrd_success) {
				bool valid = true;
				if (calls == 1)
					valid = cnd_broadcast(probe->condition) == thrd_success;
				if (mtx_unlock(probe->mutex) != thrd_success)
					abort();
				return valid;
			}
			if (ret != thrd_busy)
				return false;
		}
		if (atomic_load(&fixture->wake.done) || !wait_contract_tick(&begin))
			return false;
	}
}

static bool
wait_contract_case(unsigned path, unsigned mode)
{
	struct wait_contract_fixture fixture = {0};
	bool valid = wait_contract_init(&fixture, path, mode);
	struct wake_fixture *wake = &fixture.wake;
	struct dssh_test_wait_probe *probe = &fixture.probe;
	if (!valid) {
		wake_finish(wake);
		return false;
	}
	if (thrd_create(&wake->worker, wait_contract_worker, &fixture) != thrd_success) {
		wake_finish(wake);
		return false;
	}
	wake->started = true;
	if (mode == 1 || mode == 3)
		valid = wait_contract_signal(&fixture, 1);
	struct timespec begin;
	if (timespec_get(&begin, TIME_UTC) != TIME_UTC)
		abort();
	while (valid && !atomic_load(&probe->returning))
		valid = !atomic_load(&wake->done) && wait_contract_tick(&begin);
	bool owned_at_return = false;
	if (valid) {
		int ret = mtx_trylock(probe->mutex);
		owned_at_return = ret == thrd_busy;
		valid = ret == thrd_busy && !atomic_load(&wake->done);
		if (ret == thrd_success && mtx_unlock(probe->mutex) != thrd_success)
			abort();
	}
	/* Always release the test barrier, even if an observation failed. */
	atomic_store(&probe->release, true);
	if (mode == 3) {
		valid = valid && wait_contract_signal(&fixture, 2)
		    && atomic_load(&probe->calls) >= 2 && !atomic_load(&wake->done);
		session_set_terminate(wake->sess);
	}
	if (timespec_get(&begin, TIME_UTC) != TIME_UTC)
		abort();
	while (valid && !atomic_load(&wake->done))
		valid = wait_contract_tick(&begin);
	if (valid) {
		valid = probe->releases >= 1 && probe->unlock_result == thrd_success
		    && probe->timed == wait_contract_timed(path);
		if (mode != 3)
			valid = valid && atomic_load(&probe->calls) == 1;
		if (mode == 1 || mode == 3)
			valid = valid && atomic_load(&probe->native_result) == thrd_success;
		if (mode == 0)
			valid = valid && atomic_load(&probe->native_calls) == 0;
		else
			valid = valid && atomic_load(&probe->native_calls) >= 1;
		if (mode == 2 || mode == 4)
			valid = valid && atomic_load(&probe->native_result) == thrd_timedout;
		int expected = DSSH_ERROR_TERMINATED;
		if (mode == 3 && (path == 10 || path == 11))
			expected = DSSH_POLL_READ;
		if (mode == 4 && path != 13)
			expected = path == 3 ? DSSH_ERROR_NOMORE : path == 11 ? 0 : DSSH_ERROR_TIMEOUT;
		valid = valid && wake->result == expected
		    && atomic_load(&wake->channel.dispatch_state) == DSSH_CHANNEL_DISPATCH_IDLE;
		bool terminated = mode != 4 || path == 13;
		valid = valid && atomic_load(&wake->sess->terminate) == terminated;
		if (terminated)
			valid = valid && wake_completed(wake);
		if (mtx_lock(&wake->channel.buf_mtx) != thrd_success)
			abort();
		if (path == 6 || path == 7)
			valid = valid && !wake->channel.request_pending && !wake->channel.request_responded;
		/* A timeout ends this wait. The peer's OPEN reply is still due. */
		if (mode == 4 && path == 5)
			valid = valid && wake->channel.open_pending;
		if (mtx_unlock(&wake->channel.buf_mtx) != thrd_success)
			abort();
		/* A completed operation has released the associated mutex. */
		int ret = mtx_trylock(probe->mutex);
		valid = valid && ret == thrd_success;
		if (ret == thrd_success && mtx_unlock(probe->mutex) != thrd_success)
			abort();
	}
	bool finished = wake_finish(wake);
	if (printf("wait-contract path=%u mode=%u timed=%u owned=%u calls=%u native=%u native_ret=%d releases=%u unlock_ret=%d result=%d valid=%u\n",
	    path, mode, probe->timed ? 1U : 0U, owned_at_return ? 1U : 0U,
	    atomic_load(&probe->calls), atomic_load(&probe->native_calls),
	    atomic_load(&probe->native_result), probe->releases, probe->unlock_result,
	    wake->result, valid && finished ? 1U : 0U) < 0)
		valid = false;
	if (!valid || !finished)
		fprintf(stderr, "wait contract path=%u mode=%u result=%d calls=%u native=%u releases=%u\n",
		    path, mode, wake->result, atomic_load(&probe->calls),
		    atomic_load(&probe->native_calls), probe->releases);
	return valid && finished;
}

static int
test_wait_contract_errors(void)
{
	for (unsigned path = 0; path < 15; path++) {
		for (unsigned mode = 0; mode < 2; mode++)
			ASSERT_TRUE(wait_contract_case(path, mode));
	}
	return TEST_PASS;
}

static int
test_wait_contract_spurious(void)
{
	for (unsigned path = 0; path < 15; path++)
		ASSERT_TRUE(wait_contract_case(path, 3));
	return TEST_PASS;
}

static int
test_wait_contract_timeout_errors(void)
{
	for (unsigned path = 0; path < 15; path++) {
		if (wait_contract_timed(path))
			ASSERT_TRUE(wait_contract_case(path, 2));
	}
	return TEST_PASS;
}

static int
test_wait_contract_timeouts(void)
{
	for (unsigned path = 0; path < 15; path++) {
		if (wait_contract_timed(path))
			ASSERT_TRUE(wait_contract_case(path, 4));
	}
	return TEST_PASS;
}
