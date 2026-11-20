/* Producer-owned callback results. No allocation ownership changes hands. */
#ifndef DSSH_RESULT_H
#define DSSH_RESULT_H

#include <stddef.h>
#include <stdint.h>
#include "deucessh-portable.h"

#ifdef __cplusplus
extern "C" {
#endif

struct dssh_bytes {
	const uint8_t *data;
	size_t len;
};

/* The consumer initializes results to zero before invoking their producer.
 * A nonnull returned data/array pointer requires release, even for empty or
 * static storage (use a no-op release for static storage). A null owner with
 * a real release function is valid. An owner without release is invalid.
 *
 * Storage remains valid and immutable until release(owner), called exactly
 * once after the final read on success, error or rejected output. The producer
 * knows its actual capacities and clears owned secrets before freeing or
 * reusing storage. Retain allocations for reuse where possible. No mutex may
 * remain held between returning a result and its release. Independent live
 * results require independent storage/lifetime. Never return storage whose
 * lifetime ends when the producer returns.
 * Outputs must not alias inputs borrowed only for the producer invocation.
 *
 * The consumer never clears/frees/reallocates returned storage. Release runs
 * without library mutexes, performs no I/O, and must not call session/channel,
 * lifecycle or configuration APIs. Crypto utilities may be used. C++ producers
 * must catch exceptions before returning through any C callback boundary.
 */
struct dssh_result_lease {
	void (*release)(void *owner);
	void *owner;
};

struct dssh_bytes_result {
	struct dssh_bytes bytes;
	struct dssh_result_lease lease;
};

/* Release and forget one lease. Clear it before entering producer code so
 * repeated cleanup cannot invoke it twice. Does not validate result metadata;
 * the operation's consumer validates metadata before reading any result bytes.
 * Safe with NULL or an empty lease. Does not free the result record itself.
 * The caller must serialize access to the lease and hold no library mutex.
 */
DSSH_PUBLIC void dssh_result_release(struct dssh_result_lease *lease) DSSH_ABI(dssh_result_release);

#ifdef __cplusplus
}
#endif

#endif
