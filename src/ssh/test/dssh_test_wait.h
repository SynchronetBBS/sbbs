/* Bounded predicate waits for observations without a library wait API. */
#ifndef DSSH_TEST_WAIT_H
#define DSSH_TEST_WAIT_H

#include <stdbool.h>
#include <stdatomic.h>
#include <threads.h>
#include <time.h>

/* The Windows fixture has no POSIX clock API. Its bounded observation wait
 * uses the standard clock; the POSIX path retains its monotonic deadline. */
static inline bool
dssh_test_wait_clock(struct timespec *value)
{
#ifdef _WIN32
	return timespec_get(value, TIME_UTC) == TIME_UTC;
#else
	return clock_gettime(CLOCK_MONOTONIC, value) == 0;
#endif
}

static inline bool
dssh_test_wait_tick(const struct timespec *start)
{
	struct timespec now;
	const struct timespec pause = { .tv_nsec = 1000000 };

	if (start == NULL || start->tv_nsec < 0 || start->tv_nsec >= 1000000000L)
		return false;
	if (!dssh_test_wait_clock(&now))
		return false;
	double seconds = difftime(now.tv_sec, start->tv_sec);
	if (!(seconds >= 0 && seconds <= 5) || now.tv_nsec < 0 || now.tv_nsec >= 1000000000L
	    || (seconds == 0 && now.tv_nsec < start->tv_nsec)
	    || (seconds == 5 && now.tv_nsec >= start->tv_nsec))
		return false;
	int ret = thrd_sleep(&pause, NULL);
	return ret == 0 || ret == -1;
}

static inline bool
dssh_test_wait_bool(const atomic_bool *value, bool expected)
{
	struct timespec start;

	if (!dssh_test_wait_clock(&start)
	    || start.tv_nsec < 0 || start.tv_nsec >= 1000000000L)
		return false;
	while (atomic_load(value) != expected) {
		if (!dssh_test_wait_tick(&start))
			return false;
	}
	return true;
}

static inline bool
dssh_test_wait_int(const atomic_int *value, int expected)
{
	struct timespec start;

	if (!dssh_test_wait_clock(&start)
	    || start.tv_nsec < 0 || start.tv_nsec >= 1000000000L)
		return false;
	while (atomic_load(value) != expected) {
		if (!dssh_test_wait_tick(&start))
			return false;
	}
	return true;
}

#endif
