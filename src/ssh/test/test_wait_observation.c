/* Actual native waits qualify the passive TLS observer before owner expiry. */
#include "dssh_test.h"
#include "ssh-internal.h"
#include "dssh_test_ossl.h"
#include <limits.h>

#ifdef DSSH_THREADS_SHIM
 #define REAL_WAIT __real_dssh_thread_cnd_timedwait
 #define WRAP_WAIT __wrap_dssh_thread_cnd_timedwait
#else
 #define REAL_WAIT __real_cnd_timedwait
 #define WRAP_WAIT __wrap_cnd_timedwait
#endif

int REAL_WAIT(cnd_t *, mtx_t *, const struct timespec *);
int dssh_test_cnd_timedwait(cnd_t *, mtx_t *, const struct timespec *);
static _Thread_local bool report_error;

int
WRAP_WAIT(cnd_t *condition, mtx_t *mutex, const struct timespec *until)
{
	int ret = REAL_WAIT(condition, mutex, until);
	return report_error ? thrd_error : ret;
}

struct observation_worker {
	mtx_t *mutex;
	cnd_t *condition;
	atomic_bool enter;
};

static int
observation_signal(void *data)
{
	struct observation_worker *worker = data;
	while (!atomic_load(&worker->enter))
		thrd_yield();
	if (mtx_lock(worker->mutex) != thrd_success)
		return -1;
	int ret = cnd_signal(worker->condition);
	if (mtx_unlock(worker->mutex) != thrd_success)
		abort();
	return ret == thrd_success ? 0 : -1;
}

static bool
observation_case(unsigned mode)
{
	mtx_t mutex;
	cnd_t condition;
	cnd_t other;
	if (mtx_init(&mutex, mtx_plain) != thrd_success)
		return false;
	if (cnd_init(&condition) != thrd_success) {
		mtx_destroy(&mutex);
		return false;
	}
	if (cnd_init(&other) != thrd_success) {
		cnd_destroy(&condition);
		mtx_destroy(&mutex);
		return false;
	}
	bool valid = false;
	bool active = false;
	bool locked = false;
	thrd_t thread;
	struct observation_worker worker = { .mutex = &mutex, .condition = &condition };
	atomic_init(&worker.enter, false);
	struct timespec until;
	if (timespec_get(&until, TIME_UTC) != TIME_UTC)
		goto done;
	if (mode == 6) {
		if (dssh_deadline_from_ms(&until, 5000) != 0)
			goto done;
		if (thrd_create(&thread, observation_signal, &worker) != thrd_success)
			goto done;
		active = true;
	}
	if (mtx_lock(&mutex) != thrd_success)
		goto done;
	locked = true;
	dssh_test_wait_observe_begin(mode == 9 ? NULL : mode == 2 ? &other : &condition,
	    mode == 1 ? &condition : NULL);
	if (mode == 3)
		dssh_test_thrd_fail_after(0);
	report_error = mode == 4;
	if (mode == 5)
		dssh_test_wait_observe_end();
	if (mode == 7)
		dssh_test_thrd_reset();
	if (mode == 8)
		dssh_test_ossl_reset();
	atomic_store(&worker.enter, true);
	int ret = dssh_test_cnd_timedwait(&condition, &mutex, &until);
	struct dssh_test_wait_observation observed = dssh_test_wait_observe_end();
	report_error = false;
	dssh_test_thrd_reset();
	bool native = mode == 0 || mode == 4 || mode == 6 || mode == 9;
	bool timed_out = mode == 0 || mode == 9;
	bool failed = mode == 4;
	int expected = mode == 6 ? thrd_success : mode == 3 || mode == 4 ? thrd_error : thrd_timedout;
	valid = ret == expected && observed.native == native
	    && observed.timed_out == timed_out && observed.failed == failed;
done:
	dssh_test_wait_observe_end();
	dssh_test_thrd_reset();
	report_error = false;
	if (locked && mtx_unlock(&mutex) != thrd_success)
		abort();
	atomic_store(&worker.enter, true);
	if (active) {
		int result;
		if (thrd_join(thread, &result) != thrd_success)
			abort();
		valid &= result == 0;
	}
	cnd_destroy(&other);
	cnd_destroy(&condition);
	mtx_destroy(&mutex);
	return valid;
}

static int
test_observations(void)
{
	for (unsigned i = 0; i < 10; i++)
		ASSERT_TRUE(observation_case(i));
	if (puts("wait_observation cases=10 native_success=1 native_timeout=1 reported_error=1 excluded=1 other=1 injection=1 disabled=1 thread_reset=1 full_reset=1 wildcard=1 valid=1") < 0)
		return TEST_FAIL;
	return TEST_PASS;
}

DSSH_TEST_NO_CLEANUP
static int
test_channel_results(void)
{
	static const int results[] = {
		0, DSSH_ERROR_ALLOC, DSSH_ERROR_PARSE, DSSH_ERROR_TERMINATED,
		DSSH_ERROR_TIMEOUT, 42, INT_MIN, INT_MAX
	};
	static const enum dssh_test_channel_stage stages[] = {
		DSSH_TEST_CHANNEL_OPEN_WAIT, DSSH_TEST_CHANNEL_ACCEPT_WAIT
	};
	for (size_t stage = 0; stage < 2; stage++) {
		enum dssh_test_channel_stage target = stages[stage];
		enum dssh_test_channel_stage other = stages[1 - stage];
		for (size_t i = 0; i < sizeof(results) / sizeof(results[0]); i++) {
			struct dssh_test_channel_observation observed = dssh_test_channel_observe_end();
			ASSERT_FALSE(observed.seen);
			ASSERT_EQ(dssh_test_channel_observe_result(target, results[i]), results[i]);
			observed = dssh_test_channel_observe_end();
			ASSERT_FALSE(observed.seen);
			dssh_test_channel_observe_begin(target);
			ASSERT_EQ(dssh_test_channel_observe_result(other, DSSH_ERROR_TIMEOUT), DSSH_ERROR_TIMEOUT);
			ASSERT_EQ(dssh_test_channel_observe_result(target, results[i]), results[i]);
			ASSERT_EQ(dssh_test_channel_observe_result(target, DSSH_ERROR_TIMEOUT), DSSH_ERROR_TIMEOUT);
			observed = dssh_test_channel_observe_end();
			ASSERT_TRUE(observed.seen);
			ASSERT_EQ(observed.result, results[i]);
		}
		for (unsigned reset = 0; reset < 2; reset++) {
			dssh_test_channel_observe_begin(target);
			ASSERT_EQ(dssh_test_channel_observe_result(target, DSSH_ERROR_TIMEOUT), DSSH_ERROR_TIMEOUT);
			if (reset == 0)
				dssh_test_thrd_reset();
			else
				dssh_test_ossl_reset();
			ASSERT_EQ(dssh_test_channel_observe_result(target, 42), 42);
			struct dssh_test_channel_observation observed = dssh_test_channel_observe_end();
			ASSERT_FALSE(observed.seen);
		}
		dssh_test_channel_observe_begin(other);
		ASSERT_EQ(dssh_test_channel_observe_result(other, 42), 42);
		dssh_test_channel_observe_begin(target);
		struct dssh_test_channel_observation observed = dssh_test_channel_observe_end();
		ASSERT_FALSE(observed.seen);
	}
	for (unsigned flags = 0; flags < 16; flags++) {
		struct dssh_test_wait_observation native = {
			.native = (flags & 1) != 0,
			.timed_out = (flags & 2) != 0,
			.failed = (flags & 4) != 0
		};
		for (size_t i = 0; i < sizeof(results) / sizeof(results[0]); i++) {
			struct dssh_test_channel_observation channel = {
				.seen = (flags & 8) != 0, .result = results[i]
			};
			bool expected = flags == 11 && results[i] == DSSH_ERROR_TIMEOUT;
			ASSERT_EQ(dssh_test_channel_expired(native, channel), expected);
		}
	}
	if (puts("channel_observation flows=16 classifier_cases=128 reset_cases=4 restart_cases=2 valid=1") < 0)
		return TEST_FAIL;
	return TEST_PASS;
}

static struct dssh_test_entry tests[] = {
	{ "observations", test_observations },
	{ "channel_results", test_channel_results }
};
DSSH_TEST_MAIN(tests)
