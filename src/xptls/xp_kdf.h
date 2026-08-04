#ifndef _XP_KDF_H
#define _XP_KDF_H

#include <stddef.h>
#include <stdint.h>

#include "xp_crypto.h"
#include "xp_digest.h"

#if defined(__cplusplus)
extern "C" {
#endif

enum xp_kdf_algorithm {
	XP_KDF_PBKDF2 = 1,
	XP_KDF_SCRYPT,
};

/* Per-derivation resource bound.  This matches OpenSSL's documented
 * default and is deliberately not a process-wide memory budget. */
#define XP_KDF_SCRYPT_MAX_MEMORY (1025ULL * 1024ULL * 1024ULL)

DLLEXPORT int xp_kdf_pbkdf2(
	enum xp_digest_algorithm digest,
	const void *password, size_t password_len,
	const void *salt, size_t salt_len,
	uint64_t iterations, void *out, size_t out_len);

DLLEXPORT int xp_kdf_scrypt(
	const void *password, size_t password_len,
	const void *salt, size_t salt_len,
	uint64_t n, uint32_t r, uint32_t p,
	void *out, size_t out_len);

#if defined(__cplusplus)
}
#endif
#endif
