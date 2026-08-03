#include "xp_crypto.h"

const char *xp_crypto_provider_name(void) { return "none"; }
const char *xp_crypto_provider_version(void) { return "disabled"; }
const char *xp_crypto_last_error(void) { return "crypto support disabled"; }
int xp_crypto_random(void *out, size_t len)
{
	(void)out;
	(void)len;
	return XP_CRYPTO_ERR_DISABLED;
}
