/* Controls call the original hosts; all retained post-return data is scalar. */
#ifndef DSSH_TEST_CONN_ZC_CASES_H
#define DSSH_TEST_CONN_ZC_CASES_H

static const struct {
	const char *name;
	int (*function)(void);
} conn_zc_consumers[] = {
	{"accept", test_accept_zc}, {"client", test_zc_open_client},
	{"cancel", test_zc_cancel}, {"close", test_close_during_zc_callback},
};

enum conn_zc_fault {
	ZC_NATIVE, ZC_CREATE, ZC_STAGE, ZC_CLOCK_NEGATIVE, ZC_CLOCK_POSITIVE,
	ZC_CLOCK_NSEC_LOW, ZC_CLOCK_NSEC_HIGH, ZC_SLEEP, ZC_CORRUPT,
	ZC_PARTIAL, ZC_READ_ZERO, ZC_READ_LARGE
};
static enum conn_zc_fault conn_zc_fault;
static enum conn_zc_stage conn_zc_selected_stage;
static unsigned conn_zc_selected_reservation;
static unsigned conn_zc_reservations;
static atomic_bool conn_zc_fired;
static _Thread_local bool conn_zc_owner_thread;
static struct conn_zc_owner_observation {
	unsigned retired;
	unsigned cleanup_cancels;
	bool valid;
	bool entered;
	bool complete;
	bool failed;
	bool close_done;
} conn_zc_observation;

static bool
conn_zc_control_stage(enum conn_zc_stage stage)
{
	unsigned occurrence = 0;
	if (stage == CONN_ZC_RESERVED) {
		if (conn_zc_reservations == UINT_MAX)
			abort();
		occurrence = conn_zc_reservations++;
	}
	if (conn_zc_fault == ZC_SLEEP && stage == CONN_ZC_BLOCKED
	    && !dssh_test_wait_bool(&conn_zc_fired, true))
		return false;
	if (conn_zc_fault != ZC_STAGE || stage != conn_zc_selected_stage
	    || occurrence != conn_zc_selected_reservation)
		return true;
	if (atomic_exchange(&conn_zc_fired, true))
		abort();
	return false;
}

static void
conn_zc_control_retire(const struct conn_zc_fixture *fixture)
{
	if (conn_zc_observation.retired != 0)
		abort();
	conn_zc_observation.retired++;
	conn_zc_observation.valid = fixture->retired && !fixture->close_active
	    && fixture->closing_channel == NULL && fixture->reserved == NULL
	    && !atomic_load(&fixture->active) && atomic_load(&fixture->release)
	    && (!atomic_load(&fixture->close_done) || fixture->close_result == 0)
	    && fixture->expected <= sizeof(fixture->data) && fixture->used <= fixture->expected;
	conn_zc_observation.cleanup_cancels = fixture->cleanup_cancels;
	conn_zc_observation.entered = atomic_load(&fixture->entered);
	conn_zc_observation.complete = atomic_load(&fixture->complete);
	conn_zc_observation.failed = atomic_load(&fixture->failed);
	conn_zc_observation.close_done = atomic_load(&fixture->close_done);
}

static int
conn_zc_control_clock(clockid_t clock, struct timespec *value)
{
	int ret = clock_gettime(clock, value);
	if (ret != 0 || !conn_zc_owner_thread
	    || conn_zc_fault < ZC_CLOCK_NEGATIVE || conn_zc_fault > ZC_CLOCK_NSEC_HIGH
	    || atomic_exchange(&conn_zc_fired, true))
		return ret;
	if (conn_zc_fault == ZC_CLOCK_NEGATIVE)
		return -1;
	if (conn_zc_fault == ZC_CLOCK_POSITIVE)
		return 1;
	value->tv_nsec = conn_zc_fault == ZC_CLOCK_NSEC_LOW ? -1 : 1000000000L;
	return ret;
}

static int
conn_zc_control_sleep(const struct timespec *duration, struct timespec *remaining)
{
	int ret = thrd_sleep(duration, remaining);
	if ((ret == 0 || ret == -1) && !conn_zc_owner_thread && conn_zc_fault == ZC_SLEEP
	    && !atomic_exchange(&conn_zc_fired, true))
		return -2;
	return ret;
}

static int64_t
conn_zc_control_read(dssh_channel channel, int stream, uint8_t *data, size_t capacity)
{
	size_t requested = conn_zc_fault == ZC_PARTIAL && capacity > 1 ? 1 : capacity;
	int64_t ret = dssh_chan_read(channel, stream, data, requested);
	size_t count;
	if (!conn_zc_io_count(ret, requested, &count) || count == 0)
		return ret;
	if (conn_zc_fault == ZC_PARTIAL) {
		if (requested < capacity)
			atomic_store(&conn_zc_fired, true);
		return ret;
	}
	if (conn_zc_fault != ZC_CORRUPT && conn_zc_fault != ZC_READ_ZERO
	    && conn_zc_fault != ZC_READ_LARGE)
		return ret;
	if (atomic_exchange(&conn_zc_fired, true))
		return ret;
	if (conn_zc_fault == ZC_CORRUPT) {
		data[0] ^= 1;
		return ret;
	}
	if (conn_zc_fault == ZC_READ_ZERO)
		return 0;
	if (capacity > INT64_MAX - 1)
		abort();
	int64_t larger = (int64_t)capacity;
	return larger + 1;
}

static int64_t
conn_zc_control_write(dssh_channel channel, int stream, const uint8_t *data, size_t length)
{
	size_t requested = conn_zc_fault == ZC_PARTIAL && length > 1 ? 1 : length;
	int64_t ret = dssh_chan_write(channel, stream, data, requested);
	if (ret > 0 && requested < length)
		atomic_store(&conn_zc_fired, true);
	return ret;
}

static bool
conn_zc_schedule(size_t consumer, enum conn_zc_fault fault, unsigned parameter,
    enum conn_zc_stage stage)
{
	if (consumer >= sizeof(conn_zc_consumers) / sizeof(conn_zc_consumers[0])
	    || fault < ZC_NATIVE || fault > ZC_READ_LARGE
	    || stage < CONN_ZC_OPENED || stage > CONN_ZC_RESERVED)
		return false;
	conn_zc_fault = fault;
	conn_zc_selected_stage = stage;
	conn_zc_selected_reservation = parameter;
	conn_zc_reservations = 0;
	conn_zc_observation = (struct conn_zc_owner_observation){0};
	atomic_init(&conn_zc_fired, false);
	conn_zc_owner_thread = true;
	conn_zc_stage_control = conn_zc_control_stage;
	conn_zc_retirement_observer = conn_zc_control_retire;
	conn_zc_clock = conn_zc_control_clock;
	conn_zc_sleep = conn_zc_control_sleep;
	conn_zc_read = conn_zc_control_read;
	conn_zc_write = conn_zc_control_write;
	struct channel_worker_probe probe = {
		.fail_at = fault == ZC_CREATE ? parameter : 4,
		.cancelled_before_join = true,
	};
	channel_worker_probe_begin(&probe);
	int host = conn_zc_consumers[consumer].function();
	channel_worker_probe_end();
	conn_zc_stage_control = NULL;
	conn_zc_retirement_observer = NULL;
	conn_zc_clock = clock_gettime;
	conn_zc_sleep = thrd_sleep;
	conn_zc_read = dssh_chan_read;
	conn_zc_write = dssh_chan_write;
	conn_zc_owner_thread = false;
	bool positive = fault == ZC_NATIVE || fault == ZC_PARTIAL;
	bool fired = atomic_load(&conn_zc_fired);
	unsigned fault_number = (unsigned)fault;
	unsigned stage_number = (unsigned)stage;
	bool valid = host == (positive ? TEST_PASS : TEST_FAIL)
	    && probe.created == probe.joined && probe.calls <= 3
	    && conn_zc_observation.retired == 1 && conn_zc_observation.valid
	    && channel_fixture_owner == NULL && g_active_ctx == NULL;
	if (fault == ZC_CREATE) {
		valid = valid && probe.creation_failed && probe.cancelled_before_join
		    && probe.calls == parameter + 1 && probe.created == parameter && !fired;
	}
	else {
		valid = valid && !probe.creation_failed && fired == (fault != ZC_NATIVE);
		if (consumer != 3 || fault != ZC_SLEEP) {
			unsigned expected = consumer == 3 && (fault == ZC_NATIVE
			    || (fault == ZC_STAGE && stage == CONN_ZC_CLOSING)) ? 3U : 2U;
			valid = valid && probe.calls == expected;
		}
	}
	for (unsigned i = 0; i < probe.created; i++)
		valid = valid && atomic_load(&probe.jobs[i].done);
	unsigned cancels = fault == ZC_STAGE && stage == CONN_ZC_RESERVED ? 1U : 0U;
	valid = valid && conn_zc_observation.cleanup_cancels == cancels;
	if (fault == ZC_SLEEP)
		valid = valid && probe.calls >= 2 && conn_zc_observation.entered && conn_zc_observation.failed;
	if (printf("zc_owner_schedule consumer=%s fault=%u parameter=%u stage=%u host=%d fired=%u created=%u joined=%u retired=%u cancels=%u valid=%u\n",
	    conn_zc_consumers[consumer].name, fault_number, parameter, stage_number, host, fired ? 1U : 0U,
	    probe.created, probe.joined, conn_zc_observation.retired,
	    conn_zc_observation.cleanup_cancels, valid ? 1U : 0U) < 0)
		return false;
	return valid;
}

static int
conn_zc_owner_cases(size_t consumer)
{
	bool valid = conn_zc_schedule(consumer, ZC_NATIVE, 0, CONN_ZC_OPENED);
	unsigned schedules = 1;
	unsigned workers = consumer == 3 ? 3U : 2U;
	for (unsigned i = 0; i < workers; i++) {
		valid = conn_zc_schedule(consumer, ZC_CREATE, i, CONN_ZC_OPENED) && valid;
		schedules++;
	}
	valid = conn_zc_schedule(consumer, ZC_STAGE, 0, CONN_ZC_OPENED) && valid;
	schedules++;
	if (consumer == 3) {
		valid = conn_zc_schedule(consumer, ZC_STAGE, 0, CONN_ZC_BLOCKED) && valid;
		valid = conn_zc_schedule(consumer, ZC_STAGE, 0, CONN_ZC_CLOSING) && valid;
		valid = conn_zc_schedule(consumer, ZC_SLEEP, 0, CONN_ZC_OPENED) && valid;
		schedules += 3;
	}
	else {
		if (consumer != 2) {
			valid = conn_zc_schedule(consumer, ZC_STAGE, 0, CONN_ZC_RECEIVED) && valid;
			schedules++;
		}
		unsigned reservations = consumer == 2 ? 4U : 1U;
		for (unsigned i = 0; i < reservations; i++) {
			valid = conn_zc_schedule(consumer, ZC_STAGE, i, CONN_ZC_RESERVED) && valid;
			schedules++;
		}
		const enum conn_zc_fault io[] = {ZC_CORRUPT, ZC_PARTIAL, ZC_READ_ZERO, ZC_READ_LARGE};
		for (size_t i = 0; i < sizeof(io) / sizeof(io[0]); i++) {
			valid = conn_zc_schedule(consumer, io[i], 0, CONN_ZC_OPENED) && valid;
			schedules++;
		}
	}
	for (unsigned i = ZC_CLOCK_NEGATIVE; i <= ZC_CLOCK_NSEC_HIGH; i++) {
		enum conn_zc_fault fault = (enum conn_zc_fault)i;
		valid = conn_zc_schedule(consumer, fault, 0, CONN_ZC_OPENED) && valid;
		schedules++;
	}
	if (printf("zc_owner_control consumer=%s schedules=%u valid=%u\n",
	    conn_zc_consumers[consumer].name, schedules, valid ? 1U : 0U) < 0)
		return TEST_FAIL;
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

static int
test_zc_owner_accept(void)
{
	return conn_zc_owner_cases(0);
}
static int
test_zc_owner_client(void)
{
	return conn_zc_owner_cases(1);
}
static int
test_zc_owner_cancel(void)
{
	return conn_zc_owner_cases(2);
}
static int
test_zc_owner_close(void)
{
	return conn_zc_owner_cases(3);
}

static int
test_zc_fixture_bounds(void)
{
	static const struct {
		int64_t result;
		size_t capacity;
		bool accepted;
		size_t count;
	} io[] = {
		{-1, 8, false, 0}, {INT64_MIN, 8, false, 0}, {0, 0, true, 0},
		{0, 8, true, 0}, {1, 0, false, 0}, {1, 1, true, 1},
		{4096, 4096, true, 4096}, {4097, 4096, false, 0},
		{INT64_MAX, 4096, false, 0},
	};
	for (size_t i = 0; i < sizeof(io) / sizeof(io[0]); i++) {
		size_t count = 42;
		bool ret = conn_zc_io_count(io[i].result, io[i].capacity, &count);
		ASSERT_EQ(ret, io[i].accepted);
		ASSERT_EQ_U(count, ret ? io[i].count : 42);
	}
	static const struct {
		struct timespec start, now;
		unsigned seconds;
		bool valid;
	} times[] = {
		{{10, 100}, {10, 100}, 1, true}, {{10, 100}, {11, 99}, 1, true},
		{{10, 100}, {11, 100}, 1, false}, {{10, 100}, {11, 101}, 1, false},
		{{10, 100}, {15, 99}, 5, true}, {{10, 100}, {15, 100}, 5, false},
		{{10, 100}, {16, 0}, 5, false}, {{10, 100}, {10, 99}, 5, false},
		{{10, 100}, {9, 999999999}, 5, false}, {{10, -1}, {10, 100}, 5, false},
		{{10, 1000000000}, {10, 100}, 5, false}, {{10, 100}, {10, -1}, 5, false},
		{{10, 100}, {10, 1000000000}, 5, false}, {{10, 100}, {10, 100}, 0, false},
		{{10, 100}, {10, 100}, 2, false}, {{10, 100}, {10, 100}, 6, false},
	};
	for (size_t i = 0; i < sizeof(times) / sizeof(times[0]); i++)
		ASSERT_EQ(conn_zc_time_valid(&times[i].start, &times[i].now,
		    times[i].seconds), times[i].valid);
	const uint8_t input[] = {1, 2, 3};
	struct conn_zc_fixture fixture;
	ASSERT_TRUE(conn_zc_prepare(&fixture, sizeof(input), false));
	ASSERT_EQ_U(conn_zc_receive(NULL, 0, input, 1, &fixture), 1);
	ASSERT_FALSE(atomic_load(&fixture.complete));
	ASSERT_EQ_U(conn_zc_receive(NULL, 0, input + 1, 2, &fixture), 2);
	ASSERT_TRUE(atomic_load(&fixture.complete));
	ASSERT_EQ_U(fixture.used, sizeof(input));
	ASSERT_MEM_EQ(fixture.data, input, sizeof(input));
	ASSERT_EQ_U(conn_zc_receive(NULL, 0, input, 1, &fixture), 1);
	ASSERT_TRUE(atomic_load(&fixture.failed));
	ASSERT_EQ_U(fixture.used, sizeof(input));
	ASSERT_MEM_EQ(fixture.data, input, sizeof(input));
	ASSERT_TRUE(conn_zc_prepare(&fixture, 2, false));
	ASSERT_EQ_U(conn_zc_receive(NULL, 0, input, sizeof(input), &fixture), sizeof(input));
	ASSERT_TRUE(atomic_load(&fixture.failed));
	ASSERT_EQ_U(fixture.used, 0);
	ASSERT_FALSE(atomic_load(&fixture.complete));
	ASSERT_FALSE(conn_zc_prepare(&fixture, sizeof(fixture.data) + 1, false));
	ASSERT_EQ_U(fixture.expected, 2);
	ASSERT_TRUE(conn_zc_prepare(&fixture, sizeof(input), false));
	ASSERT_EQ_U(conn_zc_receive(NULL, 1, input, sizeof(input), &fixture), sizeof(input));
	ASSERT_TRUE(atomic_load(&fixture.failed));
	ASSERT_EQ_U(fixture.used, 0);
	ASSERT_TRUE(conn_zc_prepare(&fixture, sizeof(input), false));
	atomic_store(&fixture.active, true);
	ASSERT_EQ_U(conn_zc_receive(NULL, 0, input, sizeof(input), &fixture), sizeof(input));
	ASSERT_TRUE(atomic_load(&fixture.failed));
	ASSERT_TRUE(atomic_load(&fixture.active));
	ASSERT_EQ_U(fixture.used, 0);
	atomic_store(&fixture.active, false);
#if SIZE_MAX > UINT32_MAX
	ASSERT_TRUE(conn_zc_prepare(&fixture, sizeof(input), false));
	size_t oversized = (size_t)UINT32_MAX;
	oversized++;
	ASSERT_EQ_U(conn_zc_receive(NULL, 0, input, oversized, &fixture), 0);
	ASSERT_TRUE(atomic_load(&fixture.failed));
	ASSERT_EQ_U(fixture.used, 0);
#endif
	if (puts("zc_fixture_bounds io=9 times=16 receive=1 valid=1") == EOF)
		return TEST_FAIL;
	return TEST_PASS;
}

static int
conn_zc_fatal_child(const char *operation, const char *mode)
{
	if (strcmp(mode, "retained") != 0 && strcmp(mode, "released") != 0)
		return 2;
	for (size_t i = 0; i < sizeof(conn_zc_consumers) / sizeof(conn_zc_consumers[0]); i++) {
		for (unsigned join = 1; join <= (i == 3 ? 3U : 2U); join++) {
			char name[32];
			int len = snprintf(name, sizeof(name), "%s-%u", conn_zc_consumers[i].name, join);
			if (len < 0)
				return 2;
			uintmax_t formatted = (uintmax_t)len;
			if (formatted >= sizeof(name))
				return 2;
			if (strcmp(name, operation) != 0)
				continue;
			struct channel_worker_probe probe = {.fail_at = 4,
			    .fatal_join = join, .fatal_released = strcmp(mode, "released") == 0};
			channel_worker_probe_begin(&probe);
			conn_zc_consumers[i].function();
			channel_worker_probe_end();
			return 4;
		}
	}
	return 2;
}
#endif
