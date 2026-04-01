/*
 * dh-gex-sha256-ops.h -- Crypto operations interface for DH-GEX SHA-256.
 *
 * Private header shared between the backend-specific files and the
 * shared protocol implementation (dh-gex-sha256.c).
 *
 * All bignum operations happen inside the backend.  The shared handler
 * works exclusively with serialized SSH mpint byte arrays.
 */

#ifndef DSSH_DHGEX_OPS_H
#define DSSH_DHGEX_OPS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "deucessh-portable.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SHA256_DIGEST_LEN 32
#define DHGEX_KEX_NAME     "diffie-hellman-group-exchange-sha256"
#define DHGEX_KEX_NAME_LEN 36

#define SSH_MSG_KEX_DH_GEX_REQUEST UINT8_C(34)
#define SSH_MSG_KEX_DH_GEX_GROUP   UINT8_C(31)
#define SSH_MSG_KEX_DH_GEX_INIT    UINT8_C(32)
#define SSH_MSG_KEX_DH_GEX_REPLY   UINT8_C(33)

#define GEX_MIN 2048
#define GEX_N   4096
#define GEX_MAX 8192

struct dssh_kex_context;

/*
 * Backend-specific DH operations for DH-GEX.
 *
 * All mpint outputs are malloc'd SSH mpint wire format (4-byte length
 * prefix + data with sign-bit padding).  Caller frees via free().
 *
 * client_keygen: Parse p,g from GEX_GROUP payload (starting at first
 *   mpint, past message type byte).  Generate random x, compute
 *   e = g^x mod p.  Output: e_mpint.  Stores internal DH state in
 *   *dh_ctx for later client_derive.
 *   *consumed: total bytes of p + g mpints consumed from group_payload.
 *
 * client_derive: Parse f from GEX_REPLY (starting at f mpint position),
 *   validate f in [1, p-1], compute K = f^x mod p.
 *   Output: k_raw (raw bytes, for shared_secret), k_mpint (for hash).
 *   *f_consumed: bytes of f mpint consumed.
 *   Frees the secret exponent.
 *
 * server_keygen: Load p,g from raw byte arrays (from group provider).
 *   Generate random y, compute f = g^y mod p.
 *   Output: p_mpint, g_mpint (for GEX_GROUP message and hash),
 *           f_mpint (for GEX_REPLY message and hash).
 *   Stores internal DH state in *dh_ctx.
 *
 * server_derive: Parse e from GEX_INIT (past message type byte),
 *   validate e in [1, p-1], compute K = e^y mod p.
 *   Output: k_raw, k_mpint.  *e_consumed: bytes consumed.
 *   Frees the secret exponent.
 *
 * free_ctx: Free all backend state.  Must be safe with NULL.
 */
struct dhgex_ops {
	int (*client_keygen)(const uint8_t *group_payload, size_t group_len,
	        size_t *consumed,
	        uint8_t **e_mpint, size_t *e_mpint_len,
	        void **dh_ctx);

	int (*client_derive)(void *dh_ctx,
	        const uint8_t *f_buf, size_t f_bufsz,
	        int64_t *f_consumed,
	        uint8_t **k_raw, size_t *k_raw_len,
	        uint8_t **k_mpint, size_t *k_mpint_len);

	int (*server_keygen)(const uint8_t *p_bytes, size_t p_len,
	        const uint8_t *g_bytes, size_t g_len,
	        uint8_t **p_mpint, size_t *p_mpint_len,
	        uint8_t **g_mpint, size_t *g_mpint_len,
	        uint8_t **f_mpint, size_t *f_mpint_len,
	        void **dh_ctx);

	int (*server_derive)(void *dh_ctx,
	        const uint8_t *e_buf, size_t e_bufsz,
	        int64_t *e_consumed,
	        uint8_t **k_raw, size_t *k_raw_len,
	        uint8_t **k_mpint, size_t *k_mpint_len);

	void (*free_ctx)(void *dh_ctx);
};

/*
 * Shared protocol handler.  Called by backend-specific wrappers.
 */
DSSH_PRIVATE int dhgex_handler_impl(struct dssh_kex_context *kctx,
    const struct dhgex_ops *ops);

#ifdef __cplusplus
}
#endif

#endif /* DSSH_DHGEX_OPS_H */
