/* Bounded predicate waits for observations without a library wait API. */
#ifndef DSSH_TEST_WAIT_H
#define DSSH_TEST_WAIT_H

#include <stdbool.h>
#include <stdatomic.h>
#include <threads.h>
#include <time.h>

static inline bool
dssh_test_wait_tick(const struct timespec *start)
{
	struct timespec now;
	const struct timespec pause = { .tv_nsec = 1000000 };

	if (clock_gettime(CLOCK_MONOTONIC, &now) != 0)
		return false;
	if (now.tv_sec - start->tv_sec > 5
	    || (now.tv_sec - start->tv_sec == 5 && now.tv_nsec >= start->tv_nsec))
		return false;
	return thrd_sleep(&pause, NULL) >= -1;
}

static inline bool
dssh_test_wait_bool(const atomic_bool *value, bool expected)
{
	struct timespec start;

	if (clock_gettime(CLOCK_MONOTONIC, &start) != 0)
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

	if (clock_gettime(CLOCK_MONOTONIC, &start) != 0)
		return false;
	while (atomic_load(value) != expected) {
		if (!dssh_test_wait_tick(&start))
			return false;
	}
	return true;
}

#endif
