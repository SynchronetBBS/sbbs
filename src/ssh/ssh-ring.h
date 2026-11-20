/* Private ring-index arithmetic. Production uses size_t; model builds compile
 * this same body with narrower unsigned types. No storage is accessed here. */
#ifndef DSSH_RING_H
#define DSSH_RING_H

#include <stddef.h>
#include <stdint.h>

#ifndef DSSH_RING_SIZE_TYPE
 #define DSSH_RING_SIZE_TYPE size_t
#endif
typedef DSSH_RING_SIZE_TYPE dssh_ring_size;

/* Internal callers guarantee capacity > 0, index < capacity, count <= capacity.
 * Subtract the distance to the end before wrapping. In the other branch,
 * count < capacity - index proves that the sum is smaller than capacity. */
static inline dssh_ring_size
dssh_ring_advance(dssh_ring_size index, dssh_ring_size count,
    dssh_ring_size capacity)
{
	dssh_ring_size remaining = (dssh_ring_size)(capacity - index);
	if (count >= remaining) {
		dssh_ring_size wrapped = (dssh_ring_size)(count - remaining);
		return wrapped;
	}
	dssh_ring_size advanced = (dssh_ring_size)(index + count);
	return advanced;
}

#endif
