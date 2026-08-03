#include "xp_crypto.h"
#include "xp_ca.h"

#include <limits.h>
#include <openssl/crypto.h>
#include <openssl/rand.h>

const char *xp_crypto_provider_name(void) { return "OpenSSL"; }
const char *xp_crypto_provider_version(void) { return OpenSSL_version(OPENSSL_VERSION); }
const char *xp_crypto_last_error(void) { return xp_ca_last_error(); }

int
xp_crypto_random(void *out, size_t len)
{
	if ((out == NULL && len != 0) || len > INT_MAX)
		return XP_CRYPTO_ERR_INVALID;
	return len == 0 || RAND_bytes(out, (int)len) == 1
		? XP_CRYPTO_OK : XP_CRYPTO_ERR;
}
