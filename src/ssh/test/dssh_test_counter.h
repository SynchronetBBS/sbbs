/* Exact fault accounting must stop before its public int count is exhausted.
 * Width substitutions below exercise this same body, not a narrow-host ABI. */
#ifndef DSSH_TEST_COUNTER_H
#define DSSH_TEST_COUNTER_H

#include <limits.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef DSSH_TEST_COUNTER_TYPE
 #define DSSH_TEST_COUNTER_TYPE int
 #define DSSH_TEST_COUNTER_MAX INT_MAX
#endif

typedef DSSH_TEST_COUNTER_TYPE dssh_test_counter_value;
typedef _Atomic(dssh_test_counter_value) dssh_test_counter;

static inline _Noreturn void
dssh_test_counter_stop(const char *message)
{
	if (fputs(message, stderr) < 0 || fflush(stderr) != 0)
		_Exit(7);
	abort();
}

static inline dssh_test_counter_value
dssh_test_counter_next(dssh_test_counter *counter)
{
	dssh_test_counter_value value = atomic_load(counter);
	for (;;) {
		if (value < 0)
			dssh_test_counter_stop("DSSH_TEST_COUNTER_INVALID\n");
		if (value == DSSH_TEST_COUNTER_MAX)
			dssh_test_counter_stop("DSSH_TEST_COUNTER_EXHAUSTED\n");
		dssh_test_counter_value next = (dssh_test_counter_value)(value + 1);
		if (atomic_compare_exchange_weak(counter, &value, next))
			return value;
	}
}

#endif
