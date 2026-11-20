/* Actual heap-backed owners exercise the private pool, not fake crypto handles. */
#include <stdatomic.h>
#include <threads.h>
#include <time.h>
#include "dssh_test.h"
#include "dssh_test_alloc.h"
#include "deucessh.h"
#include "key_algo/native-key-pool.h"

struct controls {
	atomic_uint created, destroyed, factories;
	atomic_bool valid, clone_seen, clone_go, destroy_seen, destroy_go;
	atomic_int report;
	atomic_bool value;
	bool pause_clone;
	unsigned int pause_destroy;
};
struct factory { struct controls *control; };
struct value {
	struct factory *factory;
	unsigned int id;
	atomic_bool in_use;
};

static void
initialize(struct controls *control)
{
	atomic_init(&control->created, 0);
	atomic_init(&control->destroyed, 0);
	atomic_init(&control->factories, 0);
	atomic_init(&control->valid, true);
	atomic_init(&control->clone_seen, false);
	atomic_init(&control->clone_go, false);
	atomic_init(&control->destroy_seen, false);
	atomic_init(&control->destroy_go, false);
	atomic_init(&control->report, 0);
	atomic_init(&control->value, true);
	control->pause_clone = false;
	control->pause_destroy = 0;
}

static bool
wait_flag(atomic_bool *flag)
{
	struct timespec start, now;
	if (timespec_get(&start, TIME_UTC) != TIME_UTC)
		return false;
	while (!atomic_load(flag)) {
		if (timespec_get(&now, TIME_UTC) != TIME_UTC || difftime(now.tv_sec, start.tv_sec) > 5)
			return false;
		thrd_yield();
	}
	return true;
}

static int
clone_value(void *opaque, void **out)
{
	struct factory *factory = opaque;
	struct controls *control = factory->control;
	if (control->pause_clone) {
		atomic_store(&control->clone_seen, true);
		if (!wait_flag(&control->clone_go))
			return DSSH_ERROR_TIMEOUT;
	}
	if (atomic_load(&control->value)) {
		struct value *value = malloc(sizeof(*value));
		if (value == NULL)
			return DSSH_ERROR_ALLOC;
		value->factory = factory;
		value->id = atomic_fetch_add(&control->created, 1) + 1;
		atomic_init(&value->in_use, false);
		*out = value;
	}
	return atomic_load(&control->report);
}

static void
destroy_value(void *opaque)
{
	struct value *value = opaque;
	struct controls *control = value->factory->control;
	if (value->id == control->pause_destroy) {
		atomic_store(&control->destroy_seen, true);
		if (!wait_flag(&control->destroy_go))
			atomic_store(&control->valid, false);
	}
	if (atomic_load(&control->factories) != 0 || atomic_load(&value->in_use))
		atomic_store(&control->valid, false);
	atomic_fetch_add(&control->destroyed, 1);
	free(value);
}

static void
destroy_factory(void *opaque)
{
	struct factory *factory = opaque;
	struct controls *control = factory->control;
	if (atomic_load(&control->created) != atomic_load(&control->destroyed))
		atomic_store(&control->valid, false);
	atomic_fetch_add(&control->factories, 1);
	free(factory);
}

static bool
prepare(struct controls *control, struct factory **factory, void **first)
{
	initialize(control);
	*factory = malloc(sizeof(**factory));
	if (*factory == NULL)
		return false;
	(*factory)->control = control;
	*first = NULL;
	if (clone_value(*factory, first) != 0 || *first == NULL) {
		free(*factory);
		*factory = NULL;
		return false;
	}
	return true;
}

static struct dssh_native_key_pool *
create(struct controls *control)
{
	struct factory *factory = NULL;
	void *first = NULL;
	struct dssh_native_key_pool *pool = NULL;
	if (!prepare(control, &factory, &first))
		return NULL;
	int ret = dssh_native_key_pool_create(&pool, factory, first, clone_value,
	    destroy_value, destroy_factory);
	if (ret != 0) {
		destroy_value(first);
		destroy_factory(factory);
		return NULL;
	}
	return pool;
}

static void
release(struct dssh_native_key_result *result)
{
	dssh_result_release(&result->lease);
	result->value = NULL;
}

static bool
retired_clean(struct controls *control)
{
	return atomic_load(&control->valid) && atomic_load(&control->factories) == 1
	    && atomic_load(&control->created) == atomic_load(&control->destroyed);
}

static int
test_reuse(void)
{
	struct controls control;
	struct dssh_native_key_pool *pool = create(&control);
	struct dssh_native_key_result a = {0}, b = {0};
	bool valid = pool != NULL;
	if (valid)
		valid = dssh_native_key_acquire(pool, &a) == 0 && dssh_native_key_acquire(pool, &b) == 0
		    && a.value != b.value && atomic_load(&control.created) == 2;
	release(&a);
	release(&b);
	dssh_test_alloc_fail_after(0);
	for (unsigned int i = 0; valid && i < 128; i++) {
		valid = dssh_native_key_acquire(pool, &a) == 0 && dssh_native_key_acquire(pool, &b) == 0
		    && a.value != NULL && b.value != NULL && a.value != b.value;
		release(&a);
		release(&b);
	}
	int allocations = dssh_test_alloc_count();
	dssh_test_alloc_reset();
	dssh_native_key_pool_destroy(pool);
	valid = valid && allocations == 0 && retired_clean(&control);
	printf("native key reuse: objects=2 rounds=128 allocations=%d valid=%u\n", allocations, valid ? 1U : 0U);
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

static int
test_failures(void)
{
	bool valid = true;
	unsigned int rejected = 0;
	for (int point = 0; point < 2; point++) {
		struct controls control;
		struct factory *factory = NULL;
		void *first = NULL;
		struct dssh_native_key_pool *pool = NULL;
		if (!prepare(&control, &factory, &first)) {
			valid = false;
			continue;
		}
		dssh_test_alloc_fail_after(point);
		int ret = dssh_native_key_pool_create(&pool, factory, first, clone_value, destroy_value, destroy_factory);
		int calls = dssh_test_alloc_count();
		dssh_test_alloc_reset();
		bool one = ret == DSSH_ERROR_ALLOC && calls == point + 1 && pool == NULL
		    && atomic_load(&control.destroyed) == 0 && atomic_load(&control.factories) == 0;
		if (pool == NULL) {
			destroy_value(first);
			destroy_factory(factory);
		}
		else
			dssh_native_key_pool_destroy(pool);
		rejected += one ? 1U : 0U;
		valid = one && retired_clean(&control) && valid;
	}
	const int reports[] = {DSSH_ERROR_INIT, DSSH_ERROR_ALLOC, 1, 0, DSSH_ERROR_INIT};
	const bool values[] = {false, true, true, false, true};
	for (size_t i = 0; i < sizeof(reports) / sizeof(reports[0]); i++) {
		struct controls control;
		struct dssh_native_key_pool *pool = create(&control);
		struct dssh_native_key_result a = {0}, b = {0};
		bool one = pool != NULL && dssh_native_key_acquire(pool, &a) == 0;
		if (one) {
			atomic_store(&control.report, reports[i]);
			atomic_store(&control.value, values[i]);
			int ret = dssh_native_key_acquire(pool, &b);
			int expected = reports[i] < 0 ? reports[i] : reports[i] > 0 ? DSSH_ERROR_INVALID : DSSH_ERROR_INIT;
			one = ret == expected && b.value == NULL && b.lease.release == NULL && b.lease.owner == NULL;
			atomic_store(&control.report, 0);
			atomic_store(&control.value, true);
			one = dssh_native_key_acquire(pool, &b) == 0 && b.value != a.value && one;
		}
		release(&a);
		release(&b);
		dssh_native_key_pool_destroy(pool);
		rejected += one ? 1U : 0U;
		valid = one && retired_clean(&control) && valid;
	}
	struct controls control;
	struct dssh_native_key_pool *pool = create(&control);
	struct dssh_native_key_result a = {0}, b = {0};
	bool one = pool != NULL && dssh_native_key_acquire(pool, &a) == 0;
	if (one) {
		dssh_test_alloc_fail_after(0);
		int ret = dssh_native_key_acquire(pool, &b);
		int calls = dssh_test_alloc_count();
		dssh_test_alloc_reset();
		one = ret == DSSH_ERROR_ALLOC && calls == 1 && b.value == NULL
		    && b.lease.release == NULL && atomic_load(&control.created) == 1;
	}
	dssh_native_key_pool_destroy(pool);
	release(&a);
	release(&b);
	rejected += one ? 1U : 0U;
	valid = one && retired_clean(&control) && valid;
	printf("native key rollback: cases=8 rejected=%u valid=%u\n", rejected, valid ? 1U : 0U);
	ASSERT_TRUE(valid && rejected == 8);
	return TEST_PASS;
}

struct pending {
	struct dssh_native_key_pool *pool;
	struct dssh_native_key_result result;
	int status;
};
static int
acquire_pending(void *opaque)
{
	struct pending *pending = opaque;
	pending->status = dssh_native_key_acquire(pending->pool, &pending->result);
	return 0;
}
static int
destroy_pending(void *opaque)
{
	dssh_native_key_pool_destroy(opaque);
	return 0;
}

static int
test_retirement(void)
{
	struct controls control;
	struct dssh_native_key_pool *pool = create(&control);
	struct dssh_native_key_result a = {0}, b = {0}, rejected = {0};
	bool valid = pool != NULL && dssh_native_key_acquire(pool, &a) == 0;
	thrd_t worker;
	bool started = false, retired = false;
	struct pending pending = {.pool = pool};
	if (valid) {
		control.pause_clone = true;
		started = thrd_create(&worker, acquire_pending, &pending) == thrd_success;
		valid = started && wait_flag(&control.clone_seen);
		if (valid) {
			dssh_native_key_pool_destroy(pool);
			retired = true;
			valid = dssh_native_key_acquire(pool, &rejected) == DSSH_ERROR_INVALID
			    && atomic_load(&control.factories) == 0;
			release(&a);
			valid = atomic_load(&control.factories) == 0 && valid;
		}
	}
	atomic_store(&control.clone_go, true);
	if (started && thrd_join(worker, NULL) != thrd_success)
		abort();
	valid = pending.status == 0 && pending.result.value != NULL && valid;
	release(&pending.result);
	release(&a);
	release(&rejected);
	if (!retired)
		dssh_native_key_pool_destroy(pool);
	valid = retired_clean(&control) && valid;

	pool = create(&control);
	bool one = pool != NULL && dssh_native_key_acquire(pool, &a) == 0 && dssh_native_key_acquire(pool, &b) == 0;
	started = false;
	if (one) {
		struct value *first = a.value;
		control.pause_destroy = first->id;
		release(&a);
		started = thrd_create(&worker, destroy_pending, pool) == thrd_success;
		one = started && wait_flag(&control.destroy_seen);
		if (one) {
			release(&b);
			one = atomic_load(&control.factories) == 0;
		}
	}
	atomic_store(&control.destroy_go, true);
	if (started && thrd_join(worker, NULL) != thrd_success)
		abort();
	release(&a);
	release(&b);
	if (!started)
		dssh_native_key_pool_destroy(pool);
	valid = one && retired_clean(&control) && valid;
	printf("native key retirement: pending-clone=1 draining-idle=1 valid=%u\n", valid ? 1U : 0U);
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

struct concurrent {
	struct dssh_native_key_pool *pool;
	atomic_uint ready, completed;
	atomic_bool go, valid;
};
static int
use_concurrently(void *opaque)
{
	struct concurrent *control = opaque;
	for (unsigned int round = 0; round < 32; round++) {
		struct dssh_native_key_result result = {0};
		if (dssh_native_key_acquire(control->pool, &result) != 0) {
			atomic_store(&control->valid, false);
			return 0;
		}
		struct value *value = result.value;
		bool owned = !atomic_exchange(&value->in_use, true);
		if (!owned)
			atomic_store(&control->valid, false);
		if (round == 0) {
			atomic_fetch_add(&control->ready, 1);
			if (!wait_flag(&control->go))
				atomic_store(&control->valid, false);
		}
		if (owned) {
			atomic_fetch_add(&control->completed, 1);
			atomic_store(&value->in_use, false);
		}
		release(&result);
	}
	return 0;
}

static int
test_concurrent(void)
{
	struct controls owners;
	struct dssh_native_key_pool *pool = create(&owners);
	struct concurrent control = {.pool = pool};
	atomic_init(&control.ready, 0);
	atomic_init(&control.completed, 0);
	atomic_init(&control.go, false);
	atomic_init(&control.valid, pool != NULL);
	thrd_t workers[8];
	unsigned int started = 0;
	if (pool != NULL) {
		for (; started < 8; started++) {
			if (thrd_create(&workers[started], use_concurrently, &control) != thrd_success)
				break;
		}
	}
	struct timespec start, now;
	bool valid = timespec_get(&start, TIME_UTC) == TIME_UTC;
	while (valid && atomic_load(&control.ready) != started) {
		valid = timespec_get(&now, TIME_UTC) == TIME_UTC && difftime(now.tv_sec, start.tv_sec) <= 5;
		thrd_yield();
	}
	valid = valid && started == 8 && atomic_load(&owners.created) == 8;
	dssh_test_alloc_fail_after(0);
	atomic_store(&control.go, true);
	for (unsigned int i = 0; i < started; i++) {
		if (thrd_join(workers[i], NULL) != thrd_success)
			abort();
	}
	int allocations = dssh_test_alloc_count();
	dssh_test_alloc_reset();
	unsigned int completed = atomic_load(&control.completed);
	valid = valid && atomic_load(&control.valid) && completed == 256 && allocations == 0;
	dssh_native_key_pool_destroy(pool);
	valid = valid && retired_clean(&owners);
	printf("native key concurrency: workers=%u owners=8 completed=%u warm-allocations=%d valid=%u\n",
	    started, completed, allocations, valid ? 1U : 0U);
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

static struct dssh_test_entry tests[] = {
	{"reuse", test_reuse},
	{"failures", test_failures},
	{"retirement", test_retirement},
	{"concurrent", test_concurrent},
};
DSSH_TEST_NO_CLEANUP
DSSH_TEST_MAIN(tests)
