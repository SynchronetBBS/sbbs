/*
 * mock_alloc.c — Countdown-based allocation failure injection.
 *
 * Provides __wrap_malloc, __wrap_calloc, __wrap_realloc, and
 * __wrap_free that are activated by the linker's --wrap option.
 * When armed via mock_alloc_fail_after(N), the (N+1)th allocation
 * returns NULL.  All other allocations pass through to the real
 * allocator (__real_*).
 */

#include "mock_alloc.h"

#include <stdlib.h>
#include <string.h>

/* Real allocator functions provided by the linker's --wrap mechanism */
extern void *__real_malloc(size_t);
extern void *__real_calloc(size_t, size_t);
extern void *__real_realloc(void *, size_t);
extern void  __real_free(void *);

static int fail_at = -1;    /* -1 = disabled */
static int alloc_num = 0;   /* current allocation index */

void
mock_alloc_reset(void)
{
	fail_at = -1;
	alloc_num = 0;
}

void
mock_alloc_fail_after(int n)
{
	fail_at = n;
	alloc_num = 0;
}

int
mock_alloc_count(void)
{
	return alloc_num;
}

void *
__wrap_malloc(size_t sz)
{
	if (fail_at >= 0 && alloc_num++ == fail_at)
		return NULL;
	return __real_malloc(sz);
}

void *
__wrap_calloc(size_t nmemb, size_t sz)
{
	if (fail_at >= 0 && alloc_num++ == fail_at)
		return NULL;
	return __real_calloc(nmemb, sz);
}

void *
__wrap_realloc(void *ptr, size_t sz)
{
	if (fail_at >= 0 && alloc_num++ == fail_at)
		return NULL;
	return __real_realloc(ptr, sz);
}

void
__wrap_free(void *ptr)
{
	__real_free(ptr);
}
