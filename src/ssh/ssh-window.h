/* Private SSH channel window arithmetic. RFC 4254 windows are uint32 values;
 * the implementation's least-width atomic value type may be wider. */
#ifndef DSSH_WINDOW_H
#define DSSH_WINDOW_H

#include <stdint.h>
#include <stdatomic.h>

static inline uint32_t
window_add(uint32_t current, uint32_t bytes)
{
	if (bytes > UINT32_MAX - current)
		return UINT32_MAX;
	uint32_t result = (uint32_t)(current + bytes);
	return result;
}

static inline void
window_atomic_add(atomic_uint_least32_t *win, uint32_t bytes)
{
	uint_least32_t old = atomic_load_explicit(win, memory_order_relaxed);
	for (;;) {
		uint_least32_t next = UINT32_MAX;
		if (old < UINT32_MAX && bytes <= UINT32_MAX - old) {
			uint_least32_t sum = (uint_least32_t)(old + bytes);
			next = sum;
		}
		if (atomic_compare_exchange_weak_explicit(win, &old, next,
		    memory_order_release, memory_order_relaxed))
			break;
	}
}

static inline void
window_atomic_sub(atomic_uint_least32_t *win, uint32_t bytes)
{
	uint_least32_t old = atomic_load_explicit(win, memory_order_relaxed);
	for (;;) {
		uint_least32_t next = (uint_least32_t)(bytes <= old ? old - bytes : 0);
		if (atomic_compare_exchange_weak_explicit(win, &old, next,
		    memory_order_release, memory_order_relaxed))
			break;
	}
}

#endif
