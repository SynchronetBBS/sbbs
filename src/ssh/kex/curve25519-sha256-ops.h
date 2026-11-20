/*
 * curve25519-sha256-ops.h -- Crypto operations interface for
 * curve25519-sha256 key exchange.
 *
 * Private header shared between the backend-specific files
 * (curve25519-sha256-openssl.c, curve25519-sha256-botan.c) and the
 * shared protocol implementation (curve25519-sha256.c).
 */

#ifndef DSSH_C25519_OPS_H
#define DSSH_C25519_OPS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "deucessh-portable.h"
#include "x25519-ops.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SHA256_DIGEST_LEN   32
#define C25519_KEX_NAME     "curve25519-sha256"
#define C25519_KEX_NAME_LEN 17

struct dssh_kex_context;
struct dssh_kex_result;

DSSH_PRIVATE int curve25519_init(bool client, void *config, void **state);
DSSH_PRIVATE void curve25519_cleanup(void *state);

/* Same-owner reset retains scratch/result/hash/backend capacity. Backend tables
 * are private immutable definitions with session lifetime at least this state. */
DSSH_PRIVATE int curve25519_init_impl(bool client, void *config, void **state,
    const struct dssh_x25519_ops *ops);

/*
 * Shared protocol handler.  Called by backend-specific wrappers.
 * DSSH_PRIVATE: library-internal, not exported to consumers.
 */
DSSH_PRIVATE int curve25519_handler_impl(const struct dssh_kex_context *kctx, struct dssh_kex_result *out, const struct dssh_x25519_ops *ops);

#ifdef __cplusplus
}
#endif

#endif /* DSSH_C25519_OPS_H */
