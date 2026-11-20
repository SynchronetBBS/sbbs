/*
 * deucessh-key-algo.h -- Host key and client signing algorithm module interface.
 */

#ifndef DSSH_KEY_ALGO_H
#define DSSH_KEY_ALGO_H

#include <stddef.h>
#include <stdint.h>

#include "deucessh-portable.h"
#include "deucessh-result.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct dssh_key_algo_ctx dssh_key_algo_ctx;

/* data is borrowed only for this call and cannot back returned result storage.
 * out starts zeroed. On success publish a nonempty
 * SSH signature blob and a producer-owned release lease. The blob remains valid
 * and immutable until release; simultaneous live results need independent
 * storage. Retain producer allocations for reuse where possible. Return zero
 * or a negative error; positive status is invalid. Publish partial resources
 * through the lease on failure, or release them before returning. Consumers
 * release a present lease on every outcome without clearing/freeing the blob.
 * The common result contract governs release and callback restrictions. */
typedef int  (*dssh_key_algo_sign)(struct dssh_bytes_result *out, const uint8_t *data, size_t data_len,
    dssh_key_algo_ctx *ctx);
typedef int  (*dssh_key_algo_verify)(const uint8_t *key_blob, size_t key_blob_len, const uint8_t *sig_blob,
    size_t sig_blob_len, const uint8_t *data, size_t data_len);
typedef int  (*dssh_key_algo_pubkey)(const uint8_t **out, size_t *outlen, dssh_key_algo_ctx *ctx);
/* Positive: a usable private key is present; zero: unavailable;
 * negative: error. Client verification does not require a private key. */
typedef int  (*dssh_key_algo_haskey)(dssh_key_algo_ctx *ctx);
typedef void (*dssh_key_algo_cleanup)(dssh_key_algo_ctx *ctx);

/*
 * Return the key strength in bits from a public key blob (wire format).
 * For RSA: the modulus bit count.  For Ed25519: 256.
 * Returns 0 if the blob cannot be parsed.
 */
typedef unsigned int (*dssh_key_algo_keybits)(const uint8_t *key_blob, size_t key_blob_len);

#define DSSH_KEY_ALGO_FLAG_ENCRYPTION_CAPABLE UINT32_C(1 << 0)
#define DSSH_KEY_ALGO_FLAG_SIGNATURE_CAPABLE  UINT32_C(1 << 1)

typedef struct dssh_key_algo_s {
	struct dssh_key_algo_s *next; /* Registry link; must be NULL at registration. */
	dssh_key_algo_sign      sign;
	dssh_key_algo_verify    verify;
	dssh_key_algo_pubkey    pubkey;
	dssh_key_algo_haskey    haskey;
	dssh_key_algo_cleanup   cleanup;
	dssh_key_algo_keybits   key_bits;
	/* Global server/fallback key. Session-local contexts use the same signing,
	 * public-key and availability callbacks, with a separate producer lease
	 * installed by dssh_session_set_client_key_ctx(). That lease retires the
	 * local context; the library does not also call cleanup on it. */
	dssh_key_algo_ctx      *ctx;
	uint32_t                flags;
	char                    name[];
} *dssh_key_algo;

/* Application-serialized global setup; finish before the first session-init
 * attempt. Descriptor/name/code stay borrowed for process lifetime. */
DSSH_PUBLIC int dssh_transport_register_key_algo(dssh_key_algo key_algo) DSSH_ABI(dssh_transport_register_key_algo);

#ifdef __cplusplus
}
#endif

#endif /* DSSH_KEY_ALGO_H */
