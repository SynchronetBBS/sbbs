/*
 * dssh_test_alloc.c -- Library-only allocation failure injection.
 *
 * Provides dssh_test_malloc/calloc/realloc that are redirected from
 * the standard allocators via macros in ssh-internal.h when compiled
 * with -DDSSH_TESTING.  Only library code (which includes
 * ssh-internal.h) goes through these; OpenSSL and other external
 * code calls the real allocators directly.
 *
 * The countdown mechanism is the same as mock_alloc.h but operates
 * on a separate counter so both can coexist if needed.
 */

#include <stdlib.h>
#include <stdatomic.h>
#include <stdbool.h>

#include "dssh_test_alloc.h"
#include "dssh_test_counter.h"

static atomic_int dssh_alloc_fail_at = -1;
static atomic_int dssh_alloc_num = 0;
static _Thread_local bool dssh_alloc_this_thread = true;
static atomic_bool dssh_alloc_exclude_new = false;
static atomic_bool malloc_size_enabled;
static atomic_bool malloc_size_failed;
static atomic_size_t malloc_failure_size;

static _Thread_local void *watched_ptr;
static _Thread_local size_t watched_len;
static _Thread_local bool watched_seen;
static _Thread_local bool watched_cleared;
static _Thread_local dssh_test_free_observer free_observer;
static _Thread_local void *free_observer_data;
static _Thread_local dssh_test_allocation_observer allocation_observer;
static _Thread_local void *allocation_observer_data;

void
dssh_test_alloc_observe_allocations(dssh_test_allocation_observer observer, void *data)
{
	allocation_observer = observer;
	allocation_observer_data = data;
}

void
dssh_test_alloc_observe_frees(dssh_test_free_observer observer, void *data)
{
	free_observer = observer;
	free_observer_data = data;
}

void
dssh_test_alloc_watch_free(void *ptr, size_t length)
{
	watched_ptr = ptr;
	watched_len = length;
	watched_seen = false;
	watched_cleared = false;
}

bool
dssh_test_alloc_watched_free(bool *cleared)
{
	if (cleared != NULL)
		*cleared = watched_cleared;
	return watched_seen;
}

void
dssh_test_free(void *ptr)
{
	if (free_observer != NULL)
		free_observer(ptr, free_observer_data);
	if (watched_ptr != NULL && ptr == watched_ptr) {
		const unsigned char *bytes = ptr;
		watched_cleared = true;
		for (size_t i = 0; i < watched_len; i++) {
			if (bytes[i] != 0)
				watched_cleared = false;
		}
		watched_seen = true;
		watched_ptr = NULL;
	}
	free(ptr);
}


void
dssh_test_alloc_reset(void)
{
	atomic_store(&dssh_alloc_fail_at, -1);
	atomic_store(&dssh_alloc_num, 0);
	atomic_store(&dssh_alloc_exclude_new, false);
	atomic_store(&malloc_size_enabled, false);
	atomic_store(&malloc_size_failed, false);
	dssh_alloc_this_thread = true;
	dssh_test_alloc_watch_free(NULL, 0);
	dssh_test_alloc_observe_frees(NULL, NULL);
	dssh_test_alloc_observe_allocations(NULL, NULL);
}

void
dssh_test_alloc_exclude_new_threads(void)
{
	atomic_store(&dssh_alloc_exclude_new, true);
}

bool
dssh_test_alloc_new_threads_excluded(void)
{
	return atomic_load(&dssh_alloc_exclude_new);
}

void
dssh_test_alloc_fail_after(int n)
{
	atomic_store(&dssh_alloc_num, 0);
	atomic_store(&dssh_alloc_fail_at, n);
}

void
dssh_test_malloc_fail_size(size_t size)
{
	atomic_store(&malloc_failure_size, size);
	atomic_store(&malloc_size_failed, false);
	atomic_store(&malloc_size_enabled, true);
}

bool
dssh_test_malloc_size_failed(void)
{
	return atomic_load(&malloc_size_failed);
}

void
dssh_test_alloc_exclude_thread(void)
{
	dssh_alloc_this_thread = false;
}

int
dssh_test_alloc_count(void)
{
	return atomic_load(&dssh_alloc_num);
}

static bool
should_fail(void)
{
	if (!dssh_alloc_this_thread)
		return false;
	int fa = atomic_load(&dssh_alloc_fail_at);
	if (fa < 0)
		return false;
	int n = dssh_test_counter_next(&dssh_alloc_num);
	return (n == fa);
}

void *
dssh_test_malloc(size_t sz)
{
	if (allocation_observer != NULL)
		allocation_observer(1, sz, allocation_observer_data);
	if (dssh_alloc_this_thread && atomic_load(&malloc_size_enabled)
	    && sz == atomic_load(&malloc_failure_size)
	    && !atomic_exchange(&malloc_size_failed, true))
		return NULL;
	if (should_fail())
		return NULL;
	return malloc(sz);
}

void *
dssh_test_calloc(size_t nmemb, size_t sz)
{
	if (allocation_observer != NULL)
		allocation_observer(nmemb, sz, allocation_observer_data);
	if (should_fail())
		return NULL;
	return calloc(nmemb, sz);
}

void *
dssh_test_realloc(void *ptr, size_t sz)
{
	if (allocation_observer != NULL)
		allocation_observer(1, sz, allocation_observer_data);
	if (should_fail())
		return NULL;
	return realloc(ptr, sz);
}
