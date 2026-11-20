/* Compile the production arithmetic for signed/unsigned and floating time_t
 * models, without changing the system headers or copying the implementation. */
#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include "dssh_test.h"

typedef DSSH_TIME_MODEL_TYPE model_time;
struct model_timespec {
	model_time tv_sec;
	long tv_nsec;
};
#define time_t model_time
#define timespec model_timespec
#include "ssh-time.h"
#undef timespec
#undef time_t

static int
test_time_normalization(void)
{
	struct model_timespec ts = { .tv_sec = 100, .tv_nsec = 999500000L };
	ASSERT_TRUE(dssh_timespec_add_ms(&ts, 1));
	ASSERT_TRUE(ts.tv_sec == 101);
	ASSERT_EQ(ts.tv_nsec, 500000L);
	ASSERT_TRUE(dssh_timespec_add_ms(&ts, 1234));
	ASSERT_TRUE(ts.tv_sec == 102);
	ASSERT_EQ(ts.tv_nsec, 234500000L);
	return TEST_PASS;
}

static int
test_time_rejections(void)
{
	struct model_timespec ts = { .tv_sec = 100, .tv_nsec = 123 };
	ASSERT_FALSE(dssh_timespec_add_ms(&ts, 0));
	ASSERT_FALSE(dssh_timespec_add_ms(&ts, -1));
	ASSERT_TRUE(ts.tv_sec == 100);
	ASSERT_EQ(ts.tv_nsec, 123);
	ts.tv_nsec = -1;
	ASSERT_FALSE(dssh_timespec_add_ms(&ts, 1));
	ASSERT_EQ(ts.tv_nsec, -1);
	ts.tv_nsec = 1000000000L;
	ASSERT_FALSE(dssh_timespec_add_ms(&ts, 1));
	ASSERT_EQ(ts.tv_nsec, 1000000000L);
	return TEST_PASS;
}

static int
test_time_bounds(void)
{
	struct model_timespec ts = { .tv_sec = DSSH_TIME_MODEL_MAX, .tv_nsec = 0 };
	ASSERT_FALSE(dssh_timespec_add_ms(&ts, 1000));
	ASSERT_TRUE(ts.tv_sec == DSSH_TIME_MODEL_MAX);
	ASSERT_EQ(ts.tv_nsec, 0);
	ASSERT_TRUE(dssh_timespec_add_ms(&ts, 999));
	ASSERT_TRUE(ts.tv_sec == DSSH_TIME_MODEL_MAX);
	ASSERT_EQ(ts.tv_nsec, 999000000L);
	ASSERT_FALSE(dssh_timespec_add_ms(&ts, 1));
	ASSERT_TRUE(ts.tv_sec == DSSH_TIME_MODEL_MAX);
	ASSERT_EQ(ts.tv_nsec, 999000000L);
#if !DSSH_TIME_MODEL_FLOAT
	ts.tv_sec = DSSH_TIME_MODEL_MAX - 1;
	ts.tv_nsec = 0;
	ASSERT_TRUE(dssh_timespec_add_ms(&ts, 1000));
	ASSERT_TRUE(ts.tv_sec == DSSH_TIME_MODEL_MAX);
	ASSERT_EQ(ts.tv_nsec, 0);
#endif
#if DSSH_TIME_MODEL_SIGNED
	ts.tv_sec = -2;
	ts.tv_nsec = 0;
	ASSERT_TRUE(dssh_timespec_add_ms(&ts, 1500));
	ASSERT_TRUE(ts.tv_sec == -1);
	ASSERT_EQ(ts.tv_nsec, 500000000L);
	ASSERT_TRUE(dssh_timespec_add_ms(&ts, 1500));
	ASSERT_TRUE(ts.tv_sec == 1);
	ASSERT_EQ(ts.tv_nsec, 0);
#if !DSSH_TIME_MODEL_FLOAT
	ts.tv_sec = DSSH_TIME_MODEL_MIN;
	ASSERT_TRUE(dssh_timespec_add_ms(&ts, 1000));
	ASSERT_TRUE(ts.tv_sec == DSSH_TIME_MODEL_MIN + 1);
#endif
#endif
#if DSSH_TIME_MODEL_FLOAT
	ts.tv_sec = INFINITY;
	ASSERT_FALSE(dssh_timespec_add_ms(&ts, 1));
	ts.tv_sec = NAN;
	ASSERT_FALSE(dssh_timespec_add_ms(&ts, 1));
#endif
	return TEST_PASS;
}

static int
test_time_narrow_model(void)
{
#if DSSH_TIME_MODEL_NARROW
	struct model_timespec ts = { .tv_sec = 0, .tv_nsec = 0 };
	ASSERT_TRUE(dssh_timespec_add_ms(&ts, INT64_C(32767000)));
	ASSERT_TRUE(ts.tv_sec == 32767);
#if DSSH_TIME_MODEL_SIGNED
	ASSERT_FALSE(dssh_timespec_add_ms(&ts, 1000));
	ts.tv_sec = -32768;
	ASSERT_TRUE(dssh_timespec_add_ms(&ts, INT64_C(65535000)));
	ASSERT_TRUE(ts.tv_sec == 32767);
#else
	ASSERT_TRUE(dssh_timespec_add_ms(&ts, 1000));
	ASSERT_TRUE(ts.tv_sec == 32768);
	ts.tv_sec = 0;
	ASSERT_TRUE(dssh_timespec_add_ms(&ts, INT64_C(65535000)));
	ASSERT_TRUE(ts.tv_sec == 65535);
	ASSERT_FALSE(dssh_timespec_add_ms(&ts, 1000));
#endif
#endif
	return TEST_PASS;
}

static int
test_time_wide_intervals(void)
{
	const int64_t intervals[] = { INT64_C(75000), INT64_C(2147483648), INT64_MAX };
	for (size_t i = 0; i < sizeof(intervals) / sizeof(intervals[0]); i++) {
		struct model_timespec ts = { .tv_sec = 0, .tv_nsec = 999999999L };
		int64_t expected = intervals[i] / 1000 + 1;
		long fraction = (long)(intervals[i] % 1000);
		long expected_nanos = 999999999L + fraction * 1000000L;
		if (expected_nanos < 1000000000L)
			expected--;
		else
			expected_nanos -= 1000000000L;
#if DSSH_TIME_MODEL_FLOAT
		/* The oracle converts a value <= INT64_MAX/1000+1, so the
		 * rounded value also fits int64_t before this reverse cast. */
		model_time represented = (model_time)expected;
		int64_t recovered = (int64_t)represented;
		bool fits = recovered == expected;
#else
		bool fits = expected >= 0 && expected <= DSSH_TIME_MODEL_MAX;
#endif
		bool added = dssh_timespec_add_ms(&ts, intervals[i]);
		ASSERT_TRUE(added == fits);
		if (fits) {
			model_time expected_time = (model_time)expected;
			ASSERT_TRUE(ts.tv_sec == expected_time);
			ASSERT_EQ(ts.tv_nsec, expected_nanos);
		}
		else {
			ASSERT_TRUE(ts.tv_sec == 0);
			ASSERT_EQ(ts.tv_nsec, 999999999L);
		}
	}
	struct model_timespec ts = { .tv_sec = 100, .tv_nsec = 123 };
	ASSERT_FALSE(dssh_timespec_add_ms(&ts, INTMAX_MIN));
	ASSERT_TRUE(ts.tv_sec == 100);
	ASSERT_EQ(ts.tv_nsec, 123);
	return TEST_PASS;
}

static int
test_time_intervals(void)
{
	ASSERT_FALSE(dssh_time_interval_reached(100, 101, 1));
	ASSERT_TRUE(dssh_time_interval_reached(100, 100, 0));
	ASSERT_FALSE(dssh_time_interval_reached(109, 100, 10));
	ASSERT_TRUE(dssh_time_interval_reached(110, 100, 10));
	model_time maximum = DSSH_TIME_MODEL_MAX;
	ASSERT_TRUE(dssh_time_interval_reached(maximum, 0, 1));
	ASSERT_FALSE(dssh_time_interval_reached(0, maximum, 1));
#if !DSSH_TIME_MODEL_FLOAT
	model_time below = DSSH_TIME_MODEL_MAX - 1;
	ASSERT_TRUE(dssh_time_interval_reached(maximum, below, 1));
	ASSERT_FALSE(dssh_time_interval_reached(maximum, below, 2));
#endif
#if DSSH_TIME_MODEL_SIGNED
	model_time minimum = DSSH_TIME_MODEL_MIN;
	ASSERT_TRUE(dssh_time_interval_reached(0, -1, 1));
	ASSERT_TRUE(dssh_time_interval_reached(1, -1, 2));
	ASSERT_FALSE(dssh_time_interval_reached(1, -1, 3));
	ASSERT_TRUE(dssh_time_interval_reached(-1, -2, 1));
	ASSERT_TRUE(dssh_time_interval_reached(0, minimum, 1));
	ASSERT_TRUE(dssh_time_interval_reached(maximum, minimum, 1));
#if !DSSH_TIME_MODEL_NARROW
	ASSERT_TRUE(dssh_time_interval_reached(maximum, minimum, UINT32_MAX));
#endif
#endif
	return TEST_PASS;
}

static int
test_time_exhaustive_narrow(void)
{
#if DSSH_TIME_MODEL_NARROW
	const int64_t intervals[] = { 1, 999, 1000, 1234, INT64_C(75000),
	    INT64_C(32767000), INT64_C(65535000), INT_MAX, INT64_C(2147483648), INT64_MAX };
	const long nanos[] = { 0, 999999999L };
	for (int32_t value = DSSH_TIME_MODEL_MIN; value <= DSSH_TIME_MODEL_MAX; value++) {
		model_time original = (model_time)value;
		for (size_t i = 0; i < sizeof(intervals) / sizeof(intervals[0]); i++) {
			for (size_t j = 0; j < sizeof(nanos) / sizeof(nanos[0]); j++) {
				struct model_timespec ts = { .tv_sec = original, .tv_nsec = nanos[j] };
				/* A wider signed oracle needs none of the production
				 * helper's narrow/unsigned representation handling.
				 * Both sums fit: |value| <= 65535, interval/1000
				 * <= INT64_MAX/1000, fractional sum < 2*10^9. */
				int64_t fraction = nanos[j] + (intervals[i] % 1000) * INT64_C(1000000);
				int64_t expected = value + intervals[i] / INT64_C(1000)
				    + fraction / INT64_C(1000000000);
				bool fits = expected >= DSSH_TIME_MODEL_MIN && expected <= DSSH_TIME_MODEL_MAX;
				bool added = dssh_timespec_add_ms(&ts, intervals[i]);
				ASSERT_TRUE(added == fits);
				if (fits) {
					ASSERT_TRUE(ts.tv_sec == expected);
					ASSERT_TRUE(ts.tv_nsec == fraction % INT64_C(1000000000));
				}
				else {
					ASSERT_TRUE(ts.tv_sec == original);
					ASSERT_EQ(ts.tv_nsec, nanos[j]);
				}
			}
		}
	}
#endif
	return TEST_PASS;
}

static int
test_time_relative_wait(void)
{
	uint32_t ms = 42;
	const uint32_t cap = UINT32_C(0xfffffffe);
	ASSERT_TRUE(dssh_relative_wait_ms(0, 1, cap, &ms));
	ASSERT_EQ_U(ms, 1);
	ASSERT_TRUE(dssh_relative_wait_ms(1, -999999999L, cap, &ms));
	ASSERT_EQ_U(ms, 1);
	ASSERT_TRUE(dssh_relative_wait_ms(5, 1, cap, &ms));
	ASSERT_EQ_U(ms, 5001);
	ASSERT_TRUE(dssh_relative_wait_ms(UINT64_MAX, 0, cap, &ms));
	ASSERT_EQ_U(ms, cap);
	ASSERT_TRUE(dssh_relative_wait_ms(UINT64_C(4294968), -999999999L, cap, &ms));
	ASSERT_EQ_U(ms, UINT32_C(4294967001));
	ASSERT_TRUE(dssh_relative_wait_ms(UINT64_C(4294967), 294000000L, cap, &ms));
	ASSERT_EQ_U(ms, cap);
	ASSERT_TRUE(dssh_relative_wait_ms(UINT64_C(4294967), 293000001L, cap, &ms));
	ASSERT_EQ_U(ms, cap);
	ASSERT_TRUE(dssh_relative_wait_ms(0, 0, 0, &ms));
	ASSERT_EQ_U(ms, 0);
	ms = 42;
	ASSERT_FALSE(dssh_relative_wait_ms(0, -1, cap, &ms));
	ASSERT_EQ_U(ms, 42);
	ASSERT_FALSE(dssh_relative_wait_ms(1, 1000000000L, cap, &ms));
	ASSERT_EQ_U(ms, 42);
	return TEST_PASS;
}

static struct dssh_test_entry tests[] = {
	{ "time/wide_intervals", test_time_wide_intervals },
	{ "time/exhaustive_narrow", test_time_exhaustive_narrow },
	{ "time/relative_wait", test_time_relative_wait },
	{ "time/intervals", test_time_intervals },
	{ "time/normalization", test_time_normalization },
	{ "time/rejections", test_time_rejections },
	{ "time/bounds", test_time_bounds },
	{ "time/narrow_model", test_time_narrow_model },
};

DSSH_TEST_NO_CLEANUP
DSSH_TEST_MAIN(tests)
