/* Native allocation-gap controls for the private signature-result pool.
 * Link wrappers pause the real allocation boundary, without changing pool code.
 * This internal stress test does not permit public concurrent key retirement. */
#include <stdatomic.h>
#include <threads.h>
#include <limits.h>

#include "dssh_test.h"
#include "dssh_test_alloc.h"
#include "dssh_test_wait.h"
#include "key_algo/signature-result.h"

void *__real_dssh_test_malloc(size_t size);
void __real_dssh_test_free(void *pointer);

struct gap_state {
	struct dssh_signature_pool *pool;
	struct dssh_bytes_result result;
	uint8_t *bytes;
	int result_code;
	atomic_bool entered;
	atomic_bool resume;
	atomic_bool published;
	atomic_bool finish;
};
static _Thread_local struct gap_state *allocation_gap;

/* Pointer watches change only before thread creation, across explicit release/
 * acquire handoffs, or after join. Free-owner phases never overlap. In
 * particular, old watches are removed before their addresses can be reused. */
static struct {
	void *pool;
	void *idle;
	void *old;
	void *fresh;
	const uint8_t *idle_bytes;
	const uint8_t *old_bytes;
	const uint8_t *fresh_bytes;
	atomic_uint pools;
	atomic_uint nodes;
	atomic_uint allocations;
	atomic_bool cleared;
} watch;

void *
__wrap_dssh_test_malloc(size_t size)
{
	struct gap_state *gap = allocation_gap;
	if (gap != NULL) {
		atomic_fetch_add(&watch.allocations, 1);
		atomic_store_explicit(&gap->entered, true, memory_order_release);
		while (!atomic_load_explicit(&gap->resume, memory_order_acquire))
			thrd_yield();
	}
	return __real_dssh_test_malloc(size);
}

static void
observe_node(const uint8_t *bytes, size_t capacity)
{
	for (size_t i = 0; i < capacity; i++) {
		if (bytes[i] != 0)
			atomic_store(&watch.cleared, false);
	}
	atomic_fetch_add(&watch.nodes, 1);
}

void
__wrap_dssh_test_free(void *pointer)
{
	if (pointer != NULL) {
		if (pointer == watch.pool) {
			watch.pool = NULL;
			atomic_fetch_add(&watch.pools, 1);
		}
		else if (pointer == watch.idle) {
			observe_node(watch.idle_bytes, 8);
			watch.idle = NULL;
			watch.idle_bytes = NULL;
		}
		else if (pointer == watch.old) {
			observe_node(watch.old_bytes, 32);
			watch.old = NULL;
			watch.old_bytes = NULL;
		}
		else if (pointer == watch.fresh) {
			observe_node(watch.fresh_bytes, 64);
			watch.fresh = NULL;
			watch.fresh_bytes = NULL;
		}
	}
	__real_dssh_test_free(pointer);
}

static int
worker(void *data)
{
	struct gap_state *gap = data;
	allocation_gap = gap;
	gap->result_code = dssh_signature_buffer_acquire(gap->pool, 64,
	    &gap->result, &gap->bytes);
	allocation_gap = NULL;
	if (gap->result_code == 0)
		memset(gap->bytes, 0x93, 64);
	atomic_store_explicit(&gap->published, true, memory_order_release);
	while (!atomic_load_explicit(&gap->finish, memory_order_acquire))
		thrd_yield();
	dssh_result_release(&gap->result.lease);
	return 0;
}

void
dssh_test_failure_cleanup(void)
{
	/* No assertion runs until the worker is joined and every owner retired. */
	dssh_test_alloc_reset();
}

static int
run_gap(bool warm, bool fail)
{
	watch.pool = NULL;
	watch.idle = NULL;
	watch.old = NULL;
	watch.fresh = NULL;
	watch.idle_bytes = NULL;
	watch.old_bytes = NULL;
	watch.fresh_bytes = NULL;
	atomic_init(&watch.pools, 0);
	atomic_init(&watch.nodes, 0);
	atomic_init(&watch.allocations, 0);
	atomic_init(&watch.cleared, true);
	struct gap_state gap = {0};
	atomic_init(&gap.entered, false);
	atomic_init(&gap.resume, false);
	atomic_init(&gap.published, false);
	atomic_init(&gap.finish, false);
	struct dssh_bytes_result idle = {0};
	struct dssh_bytes_result old = {0};
	uint8_t *idle_bytes = NULL;
	uint8_t *old_bytes = NULL;
	bool valid = dssh_signature_pool_create(&gap.pool) == 0;
	bool retired = false;
	bool created = false;
	bool entered = false;
	int joined = 77;
	thrd_t thread;
	if (!valid)
		goto cleanup;
	watch.pool = gap.pool;
	if (warm) {
		valid = dssh_signature_buffer_acquire(gap.pool, 8, &idle, &idle_bytes) == 0;
		if (!valid)
			goto cleanup;
		watch.idle = idle.lease.owner;
		watch.idle_bytes = idle_bytes;
		memset(idle_bytes, 0x31, 8);
		valid = dssh_signature_buffer_acquire(gap.pool, 32, &old, &old_bytes) == 0;
		if (!valid)
			goto cleanup;
		watch.old = old.lease.owner;
		watch.old_bytes = old_bytes;
		memset(old_bytes, 0x73, 32);
		dssh_result_release(&idle.lease);
	}
	/* Count the actual library attempt on the worker only. Native thread
	 * construction uses its normal allocator and is outside this domain. */
	dssh_test_alloc_fail_after(fail ? 0 : INT_MAX);
	if (thrd_create(&thread, worker, &gap) != thrd_success) {
		valid = false;
		goto cleanup;
	}
	created = true;
	entered = dssh_test_wait_bool(&gap.entered, true);
	if (!entered) {
		valid = false;
		goto cleanup;
	}
	/* The pending producer reference must keep the pool alive after both
	 * the creator and an earlier live result have relinquished ownership. */
	dssh_signature_pool_destroy(gap.pool);
	retired = true;
	dssh_result_release(&old.lease);
	valid = atomic_load(&watch.pools) == 0
	    && atomic_load(&watch.nodes) == (warm ? 2u : 0u);
	atomic_store_explicit(&gap.resume, true, memory_order_release);
	if (!dssh_test_wait_bool(&gap.published, true)) {
		valid = false;
		goto cleanup;
	}
	valid = valid && gap.result_code == (fail ? DSSH_ERROR_ALLOC : 0);
	if (gap.result_code == 0) {
		watch.fresh = gap.result.lease.owner;
		watch.fresh_bytes = gap.bytes;
		valid = valid && gap.result.bytes.data == gap.bytes
		    && gap.result.bytes.len == 0 && gap.result.lease.release != NULL
		    && gap.result.lease.owner != NULL && atomic_load(&watch.pools) == 0;
		for (size_t i = 0; i < 64; i++)
			valid = valid && gap.bytes[i] == 0x93;
		struct dssh_bytes_result rejected = {0};
		uint8_t *unused = NULL;
		int result = dssh_signature_buffer_acquire(gap.pool, 1, &rejected, &unused);
		valid = valid && result == DSSH_ERROR_INVALID && unused == NULL
		    && rejected.bytes.data == NULL && rejected.bytes.len == 0
		    && rejected.lease.owner == NULL && rejected.lease.release == NULL;
	}
	else {
		valid = valid && gap.bytes == NULL && gap.result.bytes.data == NULL
		    && gap.result.bytes.len == 0 && gap.result.lease.owner == NULL
		    && gap.result.lease.release == NULL && atomic_load(&watch.pools) == 1;
	}

cleanup:
	atomic_store_explicit(&gap.resume, true, memory_order_release);
	atomic_store_explicit(&gap.finish, true, memory_order_release);
	if (created) {
		if (thrd_join(thread, &joined) != thrd_success)
			abort();
		valid = valid && joined == 0;
	}
	int allocations = dssh_test_alloc_count();
	dssh_test_alloc_reset();
	dssh_result_release(&idle.lease);
	dssh_result_release(&old.lease);
	if (!retired)
		dssh_signature_pool_destroy(gap.pool);
	valid = valid && entered && atomic_load(&watch.pools) == 1
	    && atomic_load(&watch.nodes) == (warm ? 2u : 0u) + (fail ? 0u : 1u)
	    && atomic_load(&watch.allocations) == 1 && allocations == 1
	    && atomic_load(&watch.cleared);
	printf("signature_gap warm=%u refused=%u entered=%u joined=%d attempts=%u pools=%u nodes=%u cleared=%u valid=%u\n",
	    warm ? 1u : 0u, fail ? 1u : 0u, entered ? 1u : 0u, joined,
	    atomic_load(&watch.allocations), atomic_load(&watch.pools),
	    atomic_load(&watch.nodes), atomic_load(&watch.cleared) ? 1u : 0u, valid ? 1u : 0u);
	return valid ? TEST_PASS : TEST_FAIL;
}

static int
cold_success(void)
{
	return run_gap(false, false);
}
static int
cold_refusal(void)
{
	return run_gap(false, true);
}
static int
warm_success(void)
{
	return run_gap(true, false);
}
static int
warm_refusal(void)
{
	return run_gap(true, true);
}

static struct dssh_test_entry tests[] = {
	{"signature_gap/cold_success", cold_success},
	{"signature_gap/cold_refusal", cold_refusal},
	{"signature_gap/warm_success", warm_success},
	{"signature_gap/warm_refusal", warm_refusal},
};
DSSH_TEST_MAIN(tests)
