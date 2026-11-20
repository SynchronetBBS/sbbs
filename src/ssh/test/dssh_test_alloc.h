/*
 * dssh_test_alloc.h -- Library-only allocation failure injection.
 *
 * Controls the countdown in dssh_test_malloc/calloc/realloc.
 * Include this from test code to arm/reset the injection.
 */

#ifndef DSSH_TEST_ALLOC_H
#define DSSH_TEST_ALLOC_H

#include <stdbool.h>
#include <stddef.h>

/* Reset: disable failure injection */
void dssh_test_alloc_reset(void);

/* Fail the Nth library allocation (0-based). */
void dssh_test_alloc_fail_after(int n);

/* Fail the first matching malloc size on a non-excluded thread. This is
 * independent of allocation-count injection; reset disables both modes. */
void dssh_test_malloc_fail_size(size_t size);
bool dssh_test_malloc_size_failed(void);

/* Exclude calling thread from alloc injection (pass through). */
void dssh_test_alloc_exclude_thread(void);

/* Exclude new threads (e.g. demux) from alloc injection.
 * Library code checks this at thread start.  Cleared by reset. */
void dssh_test_alloc_exclude_new_threads(void);

/* Query whether new-thread exclusion is active (called from library). */
bool dssh_test_alloc_new_threads_excluded(void);

/* Return the current allocation count. */
int dssh_test_alloc_count(void);

/* Observe the next free of ptr on this thread before forwarding to real free.
 * ptr must describe a live allocation of at least length bytes. NULL disables.
 * Reset clears this observation; query before resetting allocation injection. */
void dssh_test_alloc_watch_free(void *ptr, size_t length);
bool dssh_test_alloc_watched_free(bool *cleared);

/* Read-only notification immediately before library free on this thread.
 * The observer must not release ptr or reenter the allocator. Reset clears it. */
typedef void (*dssh_test_free_observer)(const void *ptr, void *data);
void dssh_test_alloc_observe_frees(dssh_test_free_observer observer, void *data);
/* Allocator forwarding for test modules without preprocessor redirection. */
void *dssh_test_malloc(size_t size);
void dssh_test_free(void *ptr);

/* Read-only notification before a library allocation attempt on this thread.
 * count/size are separate to avoid introducing unchecked observer arithmetic.
 * The observer must not allocate/free or perform blocking operations. */
typedef void (*dssh_test_allocation_observer)(size_t count, size_t size, void *data);
void dssh_test_alloc_observe_allocations(dssh_test_allocation_observer observer, void *data);

#endif
