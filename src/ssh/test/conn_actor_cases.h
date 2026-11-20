/* Exercise the original hosts, retaining only scalar data after they return. */
#ifndef DSSH_TEST_CONN_ACTOR_CASES_H
#define DSSH_TEST_CONN_ACTOR_CASES_H

static const struct {
	const char *name;
	int (*function)(void);
	unsigned workers;
	bool pair;
} conn_actor_consumers[] = {
	{"reject", test_open_exec_rejected, 2, false},
	{"two", test_two_channels, 1, false},
	{"accept", test_accept_reject, 1, false},
	{"independent", test_channel_independence, 1, false},
	{"concurrent", test_concurrent_write_read, 4, true},
	{"terminate", test_terminate_during_io, 3, true},
	{"drop", test_connection_drop_during_read, 3, true},
	{"failure", test_open_failure_roundtrip, 1, false},
	{"blocking", test_accept_timeout_negative, 1, false},
	{"callback", test_setup_exec_rejected_by_callback, 2, false},
	{"credit", test_replenish_queued_accounting, 3, true},
	{"deadline", test_poll_deadline_not_reset, 3, true},
};

enum conn_actor_fault {ACTOR_NATIVE, ACTOR_CREATE, ACTOR_STAGE, ACTOR_PARTIAL, ACTOR_CORRUPT};
static enum conn_actor_fault conn_actor_fault;
static atomic_bool conn_actor_io_fired;
static bool conn_actor_stage_fired;
static unsigned conn_actor_retired;
static bool conn_actor_retired_valid;

static bool
conn_actor_control_stage(void)
{
	if (conn_actor_fault != ACTOR_STAGE)
		return true;
	if (conn_actor_stage_fired)
		abort();
	conn_actor_stage_fired = true;
	return false;
}

static void
conn_actor_control_retire(const struct channel_workers *workers)
{
	if (conn_actor_retired >= 2)
		abort();
	conn_actor_retired++;
	conn_actor_retired_valid = conn_actor_retired_valid && !workers->created[0]
	    && !workers->created[1] && workers->owner->workers == NULL
	    && workers->owner->client != NULL && workers->owner->server != NULL;
	if (workers->release == conn_actor_release_tx) {
		const struct tx_hold_state *state = workers->resource;
		conn_actor_retired_valid = conn_actor_retired_valid && atomic_load(&state->release);
	}
	if (workers->release == conn_actor_stop_spammer) {
		const struct spam_cnd_arg *state = workers->resource;
		conn_actor_retired_valid = conn_actor_retired_valid && atomic_load(&state->stop);
	}
}

static int64_t
conn_actor_control_read(dssh_channel channel, int stream, uint8_t *data, size_t capacity)
{
	size_t requested = conn_actor_fault == ACTOR_PARTIAL && capacity > 7 ? 7 : capacity;
	int64_t ret = dssh_chan_read(channel, stream, data, requested);
	size_t count;
	if (!conn_zc_io_count(ret, requested, &count) || count == 0)
		return ret;
	if (conn_actor_fault == ACTOR_PARTIAL && requested < capacity)
		atomic_store(&conn_actor_io_fired, true);
	if (conn_actor_fault == ACTOR_CORRUPT && !atomic_exchange(&conn_actor_io_fired, true))
		data[0] ^= 1;
	return ret;
}

static int64_t
conn_actor_control_write(dssh_channel channel, int stream, const uint8_t *data, size_t length)
{
	size_t requested = conn_actor_fault == ACTOR_PARTIAL && length > 17 ? 17 : length;
	return dssh_chan_write(channel, stream, data, requested);
}

static bool
conn_actor_schedule(size_t consumer, enum conn_actor_fault fault, unsigned parameter)
{
	if (consumer >= sizeof(conn_actor_consumers) / sizeof(conn_actor_consumers[0]))
		return false;
	conn_actor_fault = fault;
	atomic_init(&conn_actor_io_fired, false);
	conn_actor_stage_fired = false;
	conn_actor_retired = 0;
	conn_actor_retired_valid = true;
	conn_actor_stage_control = conn_actor_control_stage;
	conn_actor_retirement_observer = conn_actor_control_retire;
	conn_zc_read = conn_actor_control_read;
	conn_zc_write = conn_actor_control_write;
	struct channel_worker_probe probe = {.fail_at = fault == ACTOR_CREATE ? parameter : 4,
	    .cancelled_before_join = true};
	channel_worker_probe_begin(&probe);
	int host = conn_actor_consumers[consumer].function();
	channel_worker_probe_end();
	conn_actor_stage_control = NULL;
	conn_actor_retirement_observer = NULL;
	conn_zc_read = dssh_chan_read;
	conn_zc_write = dssh_chan_write;
	unsigned workers = conn_actor_consumers[consumer].workers;
	unsigned retired = conn_actor_consumers[consumer].pair
	    && (fault != ACTOR_CREATE || parameter >= 2) ? 2U : 1U;
	bool positive = fault == ACTOR_NATIVE || fault == ACTOR_PARTIAL;
	bool valid = host == (positive ? TEST_PASS : TEST_FAIL)
	    && probe.created == probe.joined && probe.calls <= workers
	    && conn_actor_retired == retired && conn_actor_retired_valid
	    && channel_fixture_owner == NULL && g_active_ctx == NULL
	    && conn_actor_stage_fired == (fault == ACTOR_STAGE)
	    && atomic_load(&conn_actor_io_fired) == (fault == ACTOR_PARTIAL || fault == ACTOR_CORRUPT);
	if (fault == ACTOR_CREATE)
		valid = valid && probe.creation_failed && probe.cancelled_before_join
		    && probe.calls == parameter + 1 && probe.created == parameter;
	else
		valid = valid && !probe.creation_failed && probe.calls == workers;
	for (unsigned i = 0; i < probe.created; i++)
		valid = valid && atomic_load(&probe.jobs[i].done);
	unsigned fault_number = (unsigned)fault;
	if (printf("actor_schedule consumer=%s fault=%u parameter=%u host=%d created=%u joined=%u retired=%u valid=%u\n",
	    conn_actor_consumers[consumer].name, fault_number, parameter, host,
	    probe.created, probe.joined, conn_actor_retired, valid ? 1U : 0U) < 0)
		return false;
	return valid;
}

static int
conn_actor_owner_cases(size_t consumer)
{
	if (consumer >= sizeof(conn_actor_consumers) / sizeof(conn_actor_consumers[0]))
		return TEST_FAIL;
	bool valid = conn_actor_schedule(consumer, ACTOR_NATIVE, 0);
	unsigned workers = conn_actor_consumers[consumer].workers;
	unsigned schedules = 1;
	for (unsigned i = 0; i < workers; i++) {
		valid = conn_actor_schedule(consumer, ACTOR_CREATE, i) && valid;
		schedules++;
	}
	valid = conn_actor_schedule(consumer, ACTOR_STAGE, 0) && valid;
	schedules++;
	if (conn_actor_consumers[consumer].function == test_concurrent_write_read) {
		valid = conn_actor_schedule(consumer, ACTOR_PARTIAL, 0) && valid;
		valid = conn_actor_schedule(consumer, ACTOR_CORRUPT, 0) && valid;
		schedules += 2;
	}
	if (printf("actor_control consumer=%s schedules=%u valid=%u\n",
	    conn_actor_consumers[consumer].name, schedules, valid ? 1U : 0U) < 0)
		return TEST_FAIL;
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

static int
test_actor_owner_reject(void)
{
	return conn_actor_owner_cases(0);
}
static int
test_actor_owner_two(void)
{
	return conn_actor_owner_cases(1);
}
static int
test_actor_owner_accept(void)
{
	return conn_actor_owner_cases(2);
}
static int
test_actor_owner_independent(void)
{
	return conn_actor_owner_cases(3);
}
static int
test_actor_owner_concurrent(void)
{
	return conn_actor_owner_cases(4);
}
static int
test_actor_owner_terminate(void)
{
	return conn_actor_owner_cases(5);
}
static int
test_actor_owner_drop(void)
{
	return conn_actor_owner_cases(6);
}
static int
test_actor_owner_failure(void)
{
	return conn_actor_owner_cases(7);
}
static int
test_actor_owner_blocking(void)
{
	return conn_actor_owner_cases(8);
}
static int
test_actor_owner_callback(void)
{
	return conn_actor_owner_cases(9);
}
static int
test_actor_owner_credit(void)
{
	return conn_actor_owner_cases(10);
}
static int
test_actor_owner_deadline(void)
{
	return conn_actor_owner_cases(11);
}

/* The selected join always receives a real, successfully created handle. */
static int
conn_actor_fatal_child(const char *operation, const char *mode)
{
	if (strcmp(mode, "retained") != 0 && strcmp(mode, "released") != 0)
		return 2;
	for (size_t i = 0; i < sizeof(conn_actor_consumers) / sizeof(conn_actor_consumers[0]); i++) {
		size_t length = strlen(conn_actor_consumers[i].name);
		if (strncmp(operation, conn_actor_consumers[i].name, length) != 0
		    || operation[length] != '-' || operation[length + 1] < '1'
		    || operation[length + 1] > '4' || operation[length + 2] != 0)
			continue;
		unsigned join = (unsigned)(operation[length + 1] - '0');
		if (join > conn_actor_consumers[i].workers)
			return 2;
		struct channel_worker_probe probe = {.fail_at = 4,
		    .cancelled_before_join = true, .fatal_join = join,
		    .fatal_released = strcmp(mode, "released") == 0};
		channel_worker_probe_begin(&probe);
		conn_actor_consumers[i].function();
		channel_worker_probe_end();
		return 4;
	}
	return 2;
}

#endif
