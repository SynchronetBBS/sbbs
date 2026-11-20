/*
 * deucessh-comp.h -- Compression algorithm module interface.
 */

#ifndef DSSH_COMP_H
#define DSSH_COMP_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "deucessh-portable.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct dssh_comp_ctx dssh_comp_ctx;

/* Initialize/reset an independent directional context after each key exchange.
 * compress is true for local transmit and false for local receive. Initially
 * *ctx is NULL; when this module remains selected on rekey, it is the existing
 * context. Reset its stream state in place and reuse allocations when possible.
 * If replacing an existing context, the module releases it and updates *ctx.
 * On success *ctx may be NULL for a stateless implementation. On error return
 * a negative DSSH_ERROR_* and leave a valid context for cleanup, or NULL if all
 * resources were released. Allocation and release belong to the module. */
typedef int (*dssh_comp_init)(bool compress, dssh_comp_ctx **ctx);

/* Maximum compressed size for input_len bytes, including packet flush overhead.
 * The bound is at least input_len, nondecreasing, and valid for every input and future
 * stream state of this context. It must not underestimate or change during the
 * context's lifetime. Return 0 and set *output_bound, or a negative error.
 * DSSH_ERROR_TOOLONG means the bound is not representable in size_t.
 * The transport uses this callback at key setup to reserve enough wire space
 * for expansion and publish a safe writable input capacity to callers. */
typedef int (*dssh_comp_bound)(size_t input_len, size_t *output_bound, dssh_comp_ctx *ctx);

/* Transform one complete SSH payload, preserving any directional stream state.
 * input and output are disjoint library-owned byte spans, borrowed only for
 * this call. Never access beyond input_len/output_capacity. Return 0 and set
 * *output_len to the bytes written, or a negative DSSH_ERROR_* on failure.
 * Positive results and lengths above output_capacity are invalid. Insufficient
 * output capacity is DSSH_ERROR_TOOLONG. A transform failure terminates the
 * session because stream state may already have changed; it is not retried.
 * Callbacks run without library mutexes and must not reenter session/channel
 * APIs. Crypto utility functions may be used. Clear private/history storage
 * before releasing it in cleanup. The standard none method is an identity
 * operation; the transport may bypass its transform callbacks. */
typedef int (*dssh_comp_transform)(const uint8_t *input, size_t input_len,
    uint8_t *output, size_t output_capacity, size_t *output_len, dssh_comp_ctx *ctx);
typedef dssh_comp_transform dssh_comp_compress;
typedef dssh_comp_transform dssh_comp_uncompress;
typedef void (*dssh_comp_cleanup)(dssh_comp_ctx *ctx);

typedef struct dssh_comp_s {
	struct dssh_comp_s  *next; /* Registry link; must be NULL at registration. */
	dssh_comp_init       init;
	dssh_comp_bound      bound;
	dssh_comp_compress   compress;
	dssh_comp_uncompress uncompress;
	dssh_comp_cleanup    cleanup;
	char                 name[];
} *dssh_comp;

/* All five callbacks are required. The registered entry and name remain valid
 * and immutable for the global registry's lifetime (except its next link).
 * Failed registration leaves the entry with its caller. */
/* Application-serialized global setup; finish before the first session-init
 * attempt. Descriptor/name/code stay borrowed for process lifetime. */
DSSH_PUBLIC int dssh_transport_register_comp(dssh_comp comp) DSSH_ABI(dssh_transport_register_comp);

#ifdef __cplusplus
}
#endif

#endif /* DSSH_COMP_H */
