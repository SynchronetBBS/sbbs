/*
 * dssh_test_alloc.c — Library-only allocation failure injection.
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

static atomic_int dssh_alloc_fail_at = -1;
static atomic_int dssh_alloc_num = 0;

void
dssh_test_alloc_reset(void)
{
	dssh_alloc_fail_at = -1;
	dssh_alloc_num = 0;
}

void
dssh_test_alloc_fail_after(int n)
{
	dssh_alloc_fail_at = n;
	dssh_alloc_num = 0;
}

int
dssh_test_alloc_count(void)
{
	return dssh_alloc_num;
}

void *
dssh_test_malloc(size_t sz)
{
	int fat = dssh_alloc_fail_at;
	if (fat >= 0 && dssh_alloc_num++ == fat)
		return NULL;
	return malloc(sz);
}

void *
dssh_test_calloc(size_t nmemb, size_t sz)
{
	int fat = dssh_alloc_fail_at;
	if (fat >= 0 && dssh_alloc_num++ == fat)
		return NULL;
	return calloc(nmemb, sz);
}

void *
dssh_test_realloc(void *ptr, size_t sz)
{
	int fat = dssh_alloc_fail_at;
	if (fat >= 0 && dssh_alloc_num++ == fat)
		return NULL;
	return realloc(ptr, sz);
}
