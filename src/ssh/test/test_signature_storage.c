/* Reusable storage for built-in producer-owned signature results. */
#include <stdatomic.h>
#include <threads.h>
#include <string.h>

#include "dssh_test.h"
#include "deucessh.h"
#include "key_algo/signature-result.h"
#include "dssh_test_alloc.h"

/* Every worker is joined before an assertion can return. Drop any observer
 * borrowing this test's stack before the framework proceeds after failure. */
void
dssh_test_failure_cleanup(void)
{
	dssh_test_alloc_reset();
}

static bool
bytes_zero(const uint8_t *bytes, size_t length)
{
	for (size_t i = 0; i < length; i++) {
		if (bytes[i] != 0)
			return false;
	}
	return true;
}

static int
test_independent_results(void)
{
	struct dssh_signature_pool *pool = NULL;
	ASSERT_OK(dssh_signature_pool_create(&pool));
	struct dssh_bytes_result a = {0};
	struct dssh_bytes_result b = {0};
	uint8_t *a_bytes = NULL;
	uint8_t *b_bytes = NULL;
	ASSERT_OK(dssh_signature_buffer_acquire(pool, 64, &a, &a_bytes));
	ASSERT_OK(dssh_signature_buffer_acquire(pool, 64, &b, &b_bytes));
	ASSERT_TRUE(a_bytes != b_bytes);
	memset(a_bytes, 0x31, 64);
	memset(b_bytes, 0x32, 64);
	a.bytes.len = 3;
	b.bytes.len = 7;
	dssh_result_release(&a.lease);
	dssh_result_release(&a.lease);
	/* Capacity, not the producer's reported length, controls clearing. The
	 * cache keeps this object alive, so the saved byte pointer remains valid. */
	ASSERT_TRUE(bytes_zero(a_bytes, 64));
	for (size_t i = 0; i < 64; i++)
		ASSERT_EQ(b_bytes[i], 0x32);
	struct dssh_bytes_result c = {0};
	uint8_t *c_bytes = NULL;
	dssh_test_alloc_fail_after(0);
	int ret = dssh_signature_buffer_acquire(pool, 8, &c, &c_bytes);
	int allocations = dssh_test_alloc_count();
	dssh_test_alloc_reset();
	ASSERT_OK(ret);
	ASSERT_EQ(allocations, 0);
	ASSERT_TRUE(c_bytes == a_bytes);
	ASSERT_TRUE(bytes_zero(c_bytes, 64));
	memset(c_bytes, 0x33, 8);
	/* Even malformed reported extents do not drive producer cleanup. */
	c.bytes.len = SIZE_MAX;
	dssh_result_release(&c.lease);
	dssh_result_release(&b.lease);
	dssh_signature_pool_destroy(pool);
	return TEST_PASS;
}

struct free_probe {
	unsigned calls;
	void *watch;
	const uint8_t *bytes;
	size_t capacity;
	bool seen;
	bool cleared;
};

static void
observe_free(const void *storage, void *data)
{
	struct free_probe *probe = data;
	probe->calls++;
	if (probe->watch != NULL && storage == probe->watch) {
		probe->seen = true;
		probe->cleared = bytes_zero(probe->bytes, probe->capacity);
		probe->watch = NULL;
		probe->bytes = NULL;
	}
}

static int
test_retired_context(void)
{
	struct dssh_signature_pool *pool = NULL;
	ASSERT_OK(dssh_signature_pool_create(&pool));
	struct dssh_bytes_result idle = {0};
	struct dssh_bytes_result live = {0};
	struct dssh_bytes_result second = {0};
	uint8_t *idle_bytes = NULL;
	uint8_t *live_bytes = NULL;
	uint8_t *second_bytes = NULL;
	ASSERT_OK(dssh_signature_buffer_acquire(pool, 8, &idle, &idle_bytes));
	ASSERT_OK(dssh_signature_buffer_acquire(pool, 32, &live, &live_bytes));
	ASSERT_OK(dssh_signature_buffer_acquire(pool, 64, &second, &second_bytes));
	memset(idle_bytes, 0x51, 8);
	memset(live_bytes, 0x52, 32);
	memset(second_bytes, 0x53, 64);
	dssh_result_release(&idle.lease);
	live.bytes.len = 3;
	second.bytes.len = SIZE_MAX;
	struct free_probe probe = {.watch = live.lease.owner, .bytes = live_bytes, .capacity = 32};
	dssh_test_alloc_observe_frees(observe_free, &probe);
	dssh_signature_pool_destroy(pool);
	ASSERT_EQ_U(probe.calls, 1); /* Only the idle node is freed now. */
	for (size_t i = 0; i < 32; i++)
		ASSERT_EQ(live_bytes[i], 0x52);
	for (size_t i = 0; i < 64; i++)
		ASSERT_EQ(second_bytes[i], 0x53);
	struct dssh_bytes_result rejected = {0};
	uint8_t *unused = NULL;
	ASSERT_EQ(dssh_signature_buffer_acquire(pool, 1, &rejected, &unused), DSSH_ERROR_INVALID);
	ASSERT_NULL(unused);
	/* Pool remains alive while either independent lease is outstanding. */
	dssh_result_release(&live.lease);
	ASSERT_EQ_U(probe.calls, 2);
	ASSERT_TRUE(probe.seen);
	ASSERT_TRUE(probe.cleared);
	dssh_result_release(&second.lease);
	ASSERT_EQ_U(probe.calls, 4); /* Last node and pool are both released. */
	dssh_result_release(&second.lease);
	ASSERT_EQ_U(probe.calls, 4);
	dssh_test_alloc_reset();
	return TEST_PASS;
}

static int
test_failures_and_reuse(void)
{
	struct dssh_signature_pool *pool = NULL;
	dssh_test_alloc_fail_after(0);
	int ret = dssh_signature_pool_create(&pool);
	dssh_test_alloc_reset();
	ASSERT_EQ(ret, DSSH_ERROR_ALLOC);
	ASSERT_NULL(pool);
	ASSERT_OK(dssh_signature_pool_create(&pool));
	struct dssh_bytes_result result = {0};
	uint8_t *bytes = NULL;
	dssh_test_alloc_fail_after(0);
	ret = dssh_signature_buffer_acquire(pool, 8, &result, &bytes);
	dssh_test_alloc_reset();
	ASSERT_EQ(ret, DSSH_ERROR_ALLOC);
	ASSERT_NULL(result.lease.release);
	ASSERT_NULL(result.bytes.data);
	ASSERT_NULL(bytes);
	ASSERT_OK(dssh_signature_buffer_acquire(pool, 8, &result, &bytes));
	memset(bytes, 0x71, 8);
	dssh_result_release(&result.lease);
	struct dssh_bytes_result larger = {0};
	uint8_t *larger_bytes = NULL;
	dssh_test_alloc_fail_after(0);
	ret = dssh_signature_buffer_acquire(pool, 32, &larger, &larger_bytes);
	dssh_test_alloc_reset();
	ASSERT_EQ(ret, DSSH_ERROR_ALLOC);
	ASSERT_NULL(larger.lease.release);
	ASSERT_NULL(larger_bytes);
	struct dssh_bytes_result reused = {0};
	uint8_t *reused_bytes = NULL;
	dssh_test_alloc_fail_after(0);
	ret = dssh_signature_buffer_acquire(pool, 8, &reused, &reused_bytes);
	int allocations = dssh_test_alloc_count();
	dssh_test_alloc_reset();
	ASSERT_OK(ret);
	ASSERT_EQ(allocations, 0);
	ASSERT_TRUE(reused_bytes == bytes);
	ASSERT_TRUE(bytes_zero(reused_bytes, 8));
	dssh_result_release(&reused.lease);
	dssh_signature_pool_destroy(pool);
	return TEST_PASS;
}

static int
test_invalid_requests(void)
{
	struct dssh_signature_pool *pool = NULL;
	ASSERT_EQ(dssh_signature_pool_create(NULL), DSSH_ERROR_INVALID);
	ASSERT_OK(dssh_signature_pool_create(&pool));
	ASSERT_EQ(dssh_signature_pool_create(&pool), DSSH_ERROR_INVALID);
	struct dssh_bytes_result result = {0};
	uint8_t *bytes = NULL;
	ASSERT_EQ(dssh_signature_buffer_acquire(NULL, 8, &result, &bytes), DSSH_ERROR_INVALID);
	ASSERT_EQ(dssh_signature_buffer_acquire(pool, 8, NULL, &bytes), DSSH_ERROR_INVALID);
	ASSERT_EQ(dssh_signature_buffer_acquire(pool, 8, &result, NULL), DSSH_ERROR_INVALID);
	ASSERT_EQ(dssh_signature_buffer_acquire(pool, 0, &result, &bytes), DSSH_ERROR_INVALID);
	ASSERT_EQ(dssh_signature_buffer_acquire(pool, SIZE_MAX, &result, &bytes), DSSH_ERROR_INVALID);
	uint8_t marker = 0x7b;
	result.bytes.data = &marker;
	ASSERT_EQ(dssh_signature_buffer_acquire(pool, 8, &result, &bytes), DSSH_ERROR_INVALID);
	ASSERT_EQ(marker, 0x7b);
	result.bytes.data = NULL;
	result.bytes.len = 1;
	ASSERT_EQ(dssh_signature_buffer_acquire(pool, 8, &result, &bytes), DSSH_ERROR_INVALID);
	result.bytes.len = 0;
	result.lease.owner = &marker;
	ASSERT_EQ(dssh_signature_buffer_acquire(pool, 8, &result, &bytes), DSSH_ERROR_INVALID);
	result.lease.owner = NULL;
	ASSERT_NULL(bytes);
	dssh_signature_pool_destroy(pool);
	dssh_signature_pool_destroy(NULL);
	return TEST_PASS;
}

#define WORKERS 8
struct concurrent_state {
	struct dssh_signature_pool *pool;
	atomic_uint ready;
	atomic_bool go;
};

struct worker_state {
	struct concurrent_state *shared;
	unsigned index;
	struct dssh_bytes_result result;
	uint8_t *bytes;
	bool valid;
};

static int
concurrent_worker(void *data)
{
	struct worker_state *worker = data;
	worker->valid = dssh_signature_buffer_acquire(worker->shared->pool, 64,
	    &worker->result, &worker->bytes) == 0;
	uint8_t pattern = (uint8_t)(worker->index + 1);
	if (worker->valid)
		memset(worker->bytes, pattern, 64);
	atomic_fetch_add_explicit(&worker->shared->ready, 1, memory_order_acq_rel);
	while (!atomic_load_explicit(&worker->shared->go, memory_order_acquire))
		thrd_yield();
	if (worker->valid) {
		for (size_t i = 0; i < 64; i++) {
			if (worker->bytes[i] != pattern)
				worker->valid = false;
		}
	}
	dssh_result_release(&worker->result.lease);
	return 0;
}

static bool
concurrent_round(struct dssh_signature_pool *pool, bool retire)
{
	struct concurrent_state state;
	state.pool = pool;
	atomic_init(&state.ready, 0);
	atomic_init(&state.go, false);
	struct worker_state workers[WORKERS] = {0};
	thrd_t threads[WORKERS];
	unsigned count = 0;
	bool valid = true;
	for (unsigned i = 0; i < WORKERS; i++) {
		workers[i].shared = &state;
		workers[i].index = i;
		if (thrd_create(&threads[i], concurrent_worker, &workers[i]) != thrd_success) {
			valid = false;
			break;
		}
		count++;
	}
	while (atomic_load_explicit(&state.ready, memory_order_acquire) < count)
		thrd_yield();
	for (unsigned i = 0; i < count; i++) {
		valid = valid && workers[i].valid;
		for (unsigned j = 0; j < i; j++)
			valid = valid && workers[i].bytes != workers[j].bytes;
	}
	if (retire)
		dssh_signature_pool_destroy(pool);
	atomic_store_explicit(&state.go, true, memory_order_release);
	for (unsigned i = 0; i < count; i++) {
		if (thrd_join(threads[i], NULL) != thrd_success)
			abort(); /* A live worker would still own these stack arguments. */
		valid = valid && workers[i].valid;
	}
	return valid;
}

static int
test_concurrent_leases(void)
{
	struct dssh_signature_pool *pool = NULL;
	ASSERT_OK(dssh_signature_pool_create(&pool));
	bool valid = concurrent_round(pool, false);
	dssh_test_alloc_fail_after(0);
	bool reused = concurrent_round(pool, false);
	int allocations = dssh_test_alloc_count();
	dssh_test_alloc_reset();
	bool retired = concurrent_round(pool, true);
	ASSERT_TRUE(valid);
	ASSERT_TRUE(reused);
	ASSERT_EQ(allocations, 0);
	ASSERT_TRUE(retired);
	return TEST_PASS;
}

static struct dssh_test_entry tests[] = {
	{"storage/independent", test_independent_results},
	{"storage/retirement", test_retired_context},
	{"storage/failures", test_failures_and_reuse},
	{"storage/invalid", test_invalid_requests},
	{"storage/concurrent", test_concurrent_leases},
};

DSSH_TEST_MAIN(tests)
