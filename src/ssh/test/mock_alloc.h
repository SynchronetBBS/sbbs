/*
 * mock_alloc.h -- Countdown-based allocation failure injection.
 *
 * Uses the --wrap linker option to intercept malloc/calloc/realloc
 * from library code.  When armed, the Nth allocation (counting from
 * zero) returns NULL; all others pass through to the real allocator.
 *
 * Only intercepts allocations from statically-linked library code.
 * Dynamically-linked libraries (OpenSSL, libc internals) are not
 * affected because their calls resolve through the PLT.
 */

#ifndef DSSH_MOCK_ALLOC_H
#define DSSH_MOCK_ALLOC_H

#include <stddef.h>

/*
 * Reset the mock allocator.  Disables failure injection and zeros
 * the allocation counter.
 */
void mock_alloc_reset(void);

/*
 * Arm the allocator to fail the Nth allocation (0-based).
 * Allocation 0 is the first malloc/calloc/realloc call after arming.
 * Pass -1 to disable (same as mock_alloc_reset).
 */
void mock_alloc_fail_after(int n);

/*
 * Return the number of allocations observed since the last reset.
 */
int mock_alloc_count(void);

#endif
