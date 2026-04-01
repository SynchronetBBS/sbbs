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

#include <stddef.h>
#include <stdint.h>

#include "deucessh-portable.h"

#ifdef __cplusplus
extern "C" {
#endif

#define X25519_KEY_LEN    32
#define SHA256_DIGEST_LEN 32
#define C25519_KEX_NAME     "curve25519-sha256"
#define C25519_KEX_NAME_LEN 17

struct dssh_kex_context;

/*
 * Backend-specific crypto operations for curve25519-sha256 KEX.
 *
 * keygen:     Generate an X25519 keypair.  Write the 32-byte public key
 *             to pub_out and store an opaque private key handle in
 *             *priv_ctx.  Returns 0 on success.
 *
 * derive:     Compute the shared secret from priv_ctx and peer_pub.
 *             On success, *secret is malloc'd and *secret_len set.
 *             Does NOT free priv_ctx -- caller must call free_priv.
 *             Returns 0 on success.
 *
 * exchange:   Combined keygen + derive (server-side helper).  Generates
 *             a keypair, writes our public key to our_pub, and derives
 *             the shared secret from peer_pub.  *secret is malloc'd.
 *             Returns 0 on success.
 *
 * free_priv:  Free the opaque private key handle.  Must be NULL-safe.
 */
struct dssh_c25519_ops {
	int  (*keygen)(uint8_t pub_out[X25519_KEY_LEN], void **priv_ctx);
	int  (*derive)(void *priv_ctx, const uint8_t *peer_pub,
	         size_t peer_pub_len, uint8_t **secret, size_t *secret_len);
	int  (*exchange)(const uint8_t *peer_pub, size_t peer_pub_len,
	         uint8_t *our_pub, uint8_t **secret, size_t *secret_len);
	void (*free_priv)(void *priv_ctx);
};

/*
 * Shared protocol handler.  Called by backend-specific wrappers.
 * DSSH_PRIVATE: library-internal, not exported to consumers.
 */
DSSH_PRIVATE int curve25519_handler_impl(struct dssh_kex_context *kctx,
    const struct dssh_c25519_ops *ops);

#ifdef __cplusplus
}
#endif

#endif /* DSSH_C25519_OPS_H */
