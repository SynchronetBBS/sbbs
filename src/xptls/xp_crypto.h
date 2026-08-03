#ifndef _XP_CRYPTO_H
#define _XP_CRYPTO_H

#include <stddef.h>

#include "wrapdll.h"

#if defined(__cplusplus)
extern "C" {
#endif

enum xp_crypto_status {
	XP_CRYPTO_OK = 0,
	XP_CRYPTO_ERR = -1,
	XP_CRYPTO_ERR_FORMAT = -2,
	XP_CRYPTO_ERR_POLICY = -3,
	XP_CRYPTO_ERR_VERIFY = -4,
	XP_CRYPTO_ERR_DISABLED = -5,
	XP_CRYPTO_ERR_INVALID = -6,
	XP_CRYPTO_ERR_BUFFER_TOO_SMALL = -7,
	XP_CRYPTO_ERR_UNSUPPORTED = -8,
	XP_CRYPTO_ERR_UNAVAILABLE = -9,
	XP_CRYPTO_ERR_AUTHORIZATION = -10,
	XP_CRYPTO_ERR_NOT_FOUND = -11,
	XP_CRYPTO_ERR_CONFLICT = -12,
	XP_CRYPTO_ERR_READ_ONLY = -13,
	XP_CRYPTO_ERR_NOT_EXPORTABLE = -14,
	XP_CRYPTO_ERR_IO = -15,
	XP_CRYPTO_ERR_BUSY = -16,
	XP_CRYPTO_ERR_MIGRATION_REQUIRED = -17,
};

/*
 * A secret callback is always a two-call, counted-byte interface.  The first
 * call has out == NULL and obtains the required size.  Secrets need not be
 * NUL terminated and are never retained beyond the operation invoking the
 * callback.
 */
typedef int (*xp_crypto_secret_callback_t)(
	void *context, void *out, size_t out_size, size_t *out_len);

DLLEXPORT const char *xp_crypto_status_string(int status);
DLLEXPORT const char *xp_crypto_provider_name(void);
DLLEXPORT const char *xp_crypto_provider_version(void);
DLLEXPORT int xp_crypto_random(void *out, size_t len);

/* Thread-local diagnostic for a constructor which could not return a handle. */
DLLEXPORT const char *xp_crypto_last_error(void);

#if defined(__cplusplus)
}
#endif

#endif
