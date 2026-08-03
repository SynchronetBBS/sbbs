#include "xp_crypto.h"

const char *
xp_crypto_status_string(int status)
{
	switch (status) {
		case XP_CRYPTO_OK: return "success";
		case XP_CRYPTO_ERR: return "crypto provider failure";
		case XP_CRYPTO_ERR_FORMAT: return "invalid encoded format";
		case XP_CRYPTO_ERR_POLICY: return "operation rejected by policy";
		case XP_CRYPTO_ERR_VERIFY: return "verification failed";
		case XP_CRYPTO_ERR_DISABLED: return "crypto support disabled";
		case XP_CRYPTO_ERR_INVALID: return "invalid argument";
		case XP_CRYPTO_ERR_BUFFER_TOO_SMALL: return "output buffer too small";
		case XP_CRYPTO_ERR_UNSUPPORTED: return "operation not supported";
		case XP_CRYPTO_ERR_UNAVAILABLE: return "requested service unavailable";
		case XP_CRYPTO_ERR_AUTHORIZATION: return "authorization failed";
		case XP_CRYPTO_ERR_NOT_FOUND: return "object not found";
		case XP_CRYPTO_ERR_CONFLICT: return "object conflicts with existing data";
		case XP_CRYPTO_ERR_READ_ONLY: return "object is read-only";
		case XP_CRYPTO_ERR_NOT_EXPORTABLE: return "private key is not exportable";
		case XP_CRYPTO_ERR_IO: return "I/O failure";
		case XP_CRYPTO_ERR_BUSY: return "resource is busy";
		case XP_CRYPTO_ERR_MIGRATION_REQUIRED: return "legacy keyset requires migration";
		default: return "unknown crypto status";
	}
}
