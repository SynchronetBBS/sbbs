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

static atomic_int dssh_alloc_fail_at = -1;
static atomic_int dssh_alloc_num = 0;
static _Thread_local bool dssh_alloc_this_thread = true;
static atomic_bool dssh_alloc_exclude_new = false;

void
dssh_test_alloc_reset(void)
{
	atomic_store(&dssh_alloc_fail_at, -1);
	atomic_store(&dssh_alloc_num, 0);
	atomic_store(&dssh_alloc_exclude_new, false);
	dssh_alloc_this_thread = true;
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
	int n = atomic_fetch_add(&dssh_alloc_num, 1);
	return (n == fa);
}

void *
dssh_test_malloc(size_t sz)
{
	if (should_fail())
		return NULL;
	return malloc(sz);
}

void *
dssh_test_calloc(size_t nmemb, size_t sz)
{
	if (should_fail())
		return NULL;
	return calloc(nmemb, sz);
}

void *
dssh_test_realloc(void *ptr, size_t sz)
{
	if (should_fail())
		return NULL;
	return realloc(ptr, sz);
}
