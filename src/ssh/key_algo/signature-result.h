/* Private storage support shared by the built-in signature producers. */
#ifndef DSSH_SIGNATURE_RESULT_H
#define DSSH_SIGNATURE_RESULT_H

#include "deucessh-result.h"

#ifdef __cplusplus
extern "C" {
#endif

struct dssh_signature_pool;

/* Construct before publishing a key context. *out starts NULL. */
DSSH_PRIVATE int dssh_signature_pool_create(struct dssh_signature_pool **out);

/* Release the key context's ownership. Idle storage is cleared/freed now;
 * outstanding result leases retain their independent storage and keep the
 * pool alive until their last release. No new acquire may begin after this
 * call. Serialize context retirement with producer calls, not lease release.
 * This is private key-context cleanup, not a registry shutdown facility. */
DSSH_PRIVATE void dssh_signature_pool_destroy(struct dssh_signature_pool *pool);

/* Borrow at least capacity writable bytes from this producer's cache. Capacity
 * must be nonzero and representable with allocation metadata. result starts
 * entirely zeroed. On success its data pointer and lease are set, len remains
 * zero, and *storage is writable for capacity bytes until result release.
 * The signer publishes its final checked length after writing. On failure
 * result and *storage are unchanged. Independent leases never alias storage.
 * Released allocations are cleared over their actual capacity and retained
 * for reuse, including when subsequent requested lengths are smaller.
 * No allocation, cleanup, callback or blocking operation runs under the
 * private pool lock. The private record/layout is never exposed to modules
 * using the installed public interface. */
DSSH_PRIVATE int dssh_signature_buffer_acquire(struct dssh_signature_pool *pool,
    size_t capacity, struct dssh_bytes_result *result, uint8_t **storage);

#ifdef __cplusplus
}
#endif

#endif
