/*
 * deucessh-crypto.h -- Backend-neutral crypto primitives for DeuceSSH.
 *
 * Provides hash, random, and secure-wipe operations used by the core
 * library and available to third-party modules.  The actual crypto
 * backend (OpenSSL or Botan3) is selected at compile time.
 *
 * dssh_cleanse() is declared in deucessh.h (public API) and
 * implemented by the selected backend.
 */

#ifndef DSSH_CRYPTO_H
#define DSSH_CRYPTO_H

#include <stddef.h>
#include <stdint.h>

#include "deucessh-portable.h"

#ifdef __cplusplus
extern "C" {
#endif

/* mpint encoding: high bit indicates negative, so a leading 0x80 bit
 * in a positive value requires a zero-byte prefix (RFC 4251 s5). */
#define DSSH_MPINT_SIGN_BIT 0x80

/* Opaque hash context. Allocated by dssh_hash_init, reusable after successful
 * dssh_hash_final, and released only by dssh_hash_free. */
typedef struct dssh_hash_ctx dssh_hash_ctx;

/*
 * Initialize a hash context for the named algorithm.
 * Supported names: "SHA-256", "SHA-512".
 * Additional names depend on the selected backend and are not portable.
 * On success, *ctx is set to a new context and *out_digest_len is
 * set to the hash output size in bytes.
 * Returns 0 on success, negative DSSH_ERROR_* on failure.
 */
DSSH_PUBLIC int dssh_hash_init(dssh_hash_ctx **ctx, const char *name, size_t *out_digest_len) DSSH_ABI(dssh_hash_init);

/*
 * Feed data into the hash context.
 * NULL data with zero length is accepted; NULL with nonzero length is invalid.
 * Returns 0 on success, negative DSSH_ERROR_* on failure.
 * A provider failure invalidates the context; release it with dssh_hash_free.
 * Invalid arguments leave an otherwise usable context unchanged.
 */
DSSH_PUBLIC int dssh_hash_update(dssh_hash_ctx *ctx, const uint8_t *data, size_t len) DSSH_ABI(dssh_hash_update);

/*
 * Finalize the hash and write the digest to out, then reset the
 * context for reuse (equivalent to calling dssh_hash_init again
 * with the same algorithm, but without re-allocating).
 * out must be at least out_digest_len bytes (from dssh_hash_init).
 * Returns 0 on success, negative DSSH_ERROR_* on failure.
 * A provider/finalization failure invalidates the context; free it rather
 * than retrying. Argument/capacity rejection preserves the current hash.
 * Output may already have been modified when a provider fails.
 */
DSSH_PUBLIC int dssh_hash_final(dssh_hash_ctx *ctx, uint8_t *out, size_t outlen) DSSH_ABI(dssh_hash_final);

/*
 * Free a hash context.  Use when the context is no longer needed
 * (after dssh_hash_final, or for error cleanup when dssh_hash_final
 * will not be called).  Safe to call with NULL.
 */
DSSH_PUBLIC void dssh_hash_free(dssh_hash_ctx *ctx) DSSH_ABI(dssh_hash_free);

/*
 * One-shot hash: compute the digest of data[0..len-1] using the named
 * algorithm (e.g. "SHA-256", "SHA-512") and write the result to out.
 * out must be at least as large as the algorithm's digest length.
 * Returns 0 on success, negative DSSH_ERROR_* on failure.
 */
DSSH_PUBLIC int dssh_hash_oneshot(const char *name, const uint8_t *data, size_t len, uint8_t *out, size_t outlen) DSSH_ABI(dssh_hash_oneshot);

/*
 * Fill buf with len cryptographically secure random bytes.
 * buf must be nonnull, including for zero length. A nonnull zero-length call
 * succeeds without drawing bytes. Output may be partially modified on failure.
 * Returns 0 on success, negative DSSH_ERROR_* on failure.
 */
DSSH_PUBLIC int dssh_random(uint8_t *buf, size_t len) DSSH_ABI(dssh_random);

/*
 * Constant-time memory comparison.  Returns 0 if the first len bytes
 * of a and b are equal, non-zero otherwise.  Timing is independent of
 * the data content (safe for MAC/signature verification).
 * Zero length compares equal, including NULL pointers. A NULL pointer with
 * nonzero length compares unequal.
 */
DSSH_PUBLIC int dssh_crypto_memcmp(const void *a, const void *b, size_t len) DSSH_ABI(dssh_crypto_memcmp);

/*
 * Base64-encode in[0..len-1] into out.
 * out must hold at least 4*((len+2)/3)+1 bytes.
 * Returns the number of characters written (excluding NUL terminator),
 * or negative DSSH_ERROR_* on failure.
 * Successful output is NUL-terminated. Both pointers must be nonnull even
 * for empty input. Overlapping input/output is not guaranteed across backends.
 */
DSSH_PUBLIC int dssh_base64_encode(const uint8_t *in, size_t len, char *out, size_t outlen) DSSH_ABI(dssh_base64_encode);

#ifdef __cplusplus
}
#endif

#endif /* DSSH_CRYPTO_H */
