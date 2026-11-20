/* Private retained native operation owners; not an installed module API. */
#ifndef DSSH_NATIVE_KEY_POOL_H
#define DSSH_NATIVE_KEY_POOL_H

#include "deucessh-result.h"

#ifdef __cplusplus
extern "C" {
#endif

struct dssh_native_key_pool;
struct dssh_native_key_result {
	void *value;
	struct dssh_result_lease lease;
};

typedef int (*dssh_native_key_clone)(void *factory, void **value);
typedef void (*dssh_native_key_destroy)(void *value);

/* *out starts NULL. On success the pool owns factory and first; on error
 * both remain with the caller. Clone publishes independently owned native
 * objects, returning zero or a negative error. A present partial value is
 * destroyed on every rejected clone report. Factory input is immutable for
 * simultaneous clone calls. All callbacks run without the private lock. */
DSSH_PRIVATE int dssh_native_key_pool_create(struct dssh_native_key_pool **out,
    void *factory, void *first, dssh_native_key_clone clone,
    dssh_native_key_destroy destroy, dssh_native_key_destroy destroy_factory);

/* Retire once, after preventing new acquire entries. In-flight acquisitions
 * and leases remain valid. Idle objects retire now; the factory retires after
 * all native objects, including objects being destroyed outside the lock.
 * This releases a private context owner, not a registry shutdown facility. */
DSSH_PRIVATE void dssh_native_key_pool_destroy(struct dssh_native_key_pool *pool);

/* result starts entirely zeroed. Success returns an exclusively borrowed
 * native object plus its producer lease. Release returns it to retained idle
 * storage, or destroys it if the owner has retired. An object is never shared
 * between live leases. Allocation/clone errors leave result unchanged. Context
 * retirement is serialized with new acquire entry, not with existing leases.
 * No allocation, callback, native operation or cleanup runs under the lock. */
DSSH_PRIVATE int dssh_native_key_acquire(struct dssh_native_key_pool *pool,
    struct dssh_native_key_result *result);

#ifdef __cplusplus
}
#endif

#endif
