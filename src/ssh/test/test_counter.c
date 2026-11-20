#include <stdbool.h>
#include <string.h>
#include <threads.h>

#include "dssh_test_counter.h"

enum { WORKERS = 4, EACH = 256, CALLS = WORKERS * EACH };

_Static_assert(DSSH_TEST_COUNTER_MAX >= CALLS, "counter model too narrow");

struct counter_worker {
	dssh_test_counter *counter;
	atomic_bool *seen;
};

static int
counter_worker(void *argument)
{
	struct counter_worker *worker = argument;
	bool valid = true;
	for (unsigned i = 0; i < EACH; i++) {
		dssh_test_counter_value index = dssh_test_counter_next(worker->counter);
		if (index < 0 || index >= CALLS)
			valid = false;
		else {
			size_t slot = (size_t)index;
			if (atomic_exchange(&worker->seen[slot], true))
				valid = false;
		}
	}
	return valid ? 0 : 1;
}

static int
counter_normal(void)
{
	dssh_test_counter counter = 0;
	bool valid = dssh_test_counter_next(&counter) == 0 && atomic_load(&counter) == 1;
	atomic_store(&counter, DSSH_TEST_COUNTER_MAX - 1);
	valid = valid && dssh_test_counter_next(&counter) == DSSH_TEST_COUNTER_MAX - 1
	    && atomic_load(&counter) == DSSH_TEST_COUNTER_MAX;
	atomic_store(&counter, 0);
	atomic_bool seen[CALLS];
	for (unsigned i = 0; i < CALLS; i++)
		atomic_init(&seen[i], false);
	struct counter_worker context = {&counter, seen};
	thrd_t workers[WORKERS];
	unsigned created = 0, joined = 0;
	for (unsigned i = 0; i < WORKERS; i++) {
		if (thrd_create(&workers[i], counter_worker, &context) != thrd_success) {
			valid = false;
			break;
		}
		created++;
	}
	for (unsigned i = 0; i < created; i++) {
		int result;
		if (thrd_join(workers[i], &result) != thrd_success)
			abort();
		joined++;
		valid = result == 0 && valid;
	}
	valid = valid && created == WORKERS && joined == WORKERS && atomic_load(&counter) == CALLS;
	for (unsigned i = 0; i < CALLS; i++)
		valid = atomic_load(&seen[i]) && valid;
	if (printf("counter model=%s calls=%d created=%u joined=%u boundary=1 unique=%u valid=%u\n",
	    DSSH_TEST_COUNTER_MODEL, CALLS, created, joined, valid ? 1U : 0U, valid ? 1U : 0U) < 0)
		return 1;
	return valid ? 0 : 1;
}

int
main(int argc, char **argv)
{
	if (argc != 2)
		return 2;
	if (strcmp(argv[1], "normal") == 0)
		return counter_normal();
	dssh_test_counter counter;
	if (strcmp(argv[1], "exhausted") == 0)
		atomic_init(&counter, DSSH_TEST_COUNTER_MAX);
	else if (strcmp(argv[1], "negative") == 0)
		atomic_init(&counter, -1);
	else
		return 2;
	dssh_test_counter_next(&counter);
	return 3;
}
