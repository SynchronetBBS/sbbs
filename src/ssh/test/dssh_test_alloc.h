/*
 * dssh_test_alloc.h — Library-only allocation failure injection.
 *
 * Controls the countdown in dssh_test_malloc/calloc/realloc.
 * Include this from test code to arm/reset the injection.
 */

#ifndef DSSH_TEST_ALLOC_H
#define DSSH_TEST_ALLOC_H

/* Reset: disable failure injection */
void dssh_test_alloc_reset(void);

/* Fail the Nth library allocation (0-based). */
void dssh_test_alloc_fail_after(int n);

/* Exclude calling thread from alloc injection (pass through). */
void dssh_test_alloc_exclude_thread(void);

/* Return the current allocation count. */
int dssh_test_alloc_count(void);

#endif
