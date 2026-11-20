/* Private retained storage for a single session's built-in KEX implementation. */
#ifndef DSSH_KEX_WORKSPACE_H
#define DSSH_KEX_WORKSPACE_H

#include "deucessh-result.h"

#ifdef __cplusplus
extern "C" {
#endif

struct dssh_kex_workspace;

/* *out starts NULL. Slot count and requested capacities come from bounded
 * method-specific requirements; this storage helper cannot infer wire limits.
 * Serialize all operations on one workspace, including result release. Separate
 * sessions use separate workspaces. No lock or callback is needed to use slots. */
DSSH_PRIVATE int dssh_kex_workspace_create(size_t slots, struct dssh_kex_workspace **out);

/* End all scratch borrows and clear their actual capacities. Retain allocations.
 * Reset is rejected while a result lease is live or the owner is retired. */
DSSH_PRIVATE int dssh_kex_workspace_reset(struct dssh_kex_workspace *workspace);

/* Borrow at least capacity bytes. End any earlier borrow of this slot before
 * calling again. Successful acquisition clears prior contents; failed growth
 * preserves the previous allocation/contents and leaves *storage unchanged.
 * Drop clears the actual capacity but retains the allocation for reuse. */
DSSH_PRIVATE int dssh_kex_workspace_get(struct dssh_kex_workspace *workspace,
    size_t slot, size_t capacity, uint8_t **storage);
DSSH_PRIVATE int dssh_kex_workspace_drop(struct dssh_kex_workspace *workspace, size_t slot);

/* Acquire independent secret/hash storage under one initially empty lease.
 * Publish checked spans only after the method writes them. On failure the lease
 * and output pointers remain unchanged; partially grown storage belongs to the
 * workspace and remains reusable. One result may be live per workspace. Scratch
 * use/drop remains permitted while producing that result. Release clears both
 * actual capacities, independent of any consumer-facing result metadata. */
DSSH_PRIVATE int dssh_kex_workspace_result(struct dssh_kex_workspace *workspace,
    size_t secret_size, size_t hash_size, struct dssh_result_lease *lease,
    uint8_t **secret, uint8_t **hash);

/* End scratch borrows and retire the owner. Normally the consumer releases its
 * KEX result first. A still-live lease defensively retains its independent result
 * buffers/workspace until release, while scratch is cleared/freed now. No further
 * acquisition/reset is allowed. This is private session storage cleanup, not a
 * registry shutdown API. Destroy NULL is harmless; destroy each owner once. */
DSSH_PRIVATE void dssh_kex_workspace_destroy(struct dssh_kex_workspace *workspace);

/* Built-in method adapters: initialize/reset a typed opaque workspace and clean
 * it through its producer. The public KEX state remains an opaque void pointer. */
DSSH_PRIVATE int dssh_kex_workspace_init_slots(size_t slots, void **state);
DSSH_PRIVATE void dssh_kex_workspace_cleanup(void *state);

#ifdef __cplusplus
}
#endif

#endif
