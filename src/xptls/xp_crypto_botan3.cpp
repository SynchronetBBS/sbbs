#include "xp_crypto.h"
#include "xp_ca.h"

#include <botan/auto_rng.h>
#include <botan/version.h>

extern "C" const char *xp_crypto_provider_name(void) { return "Botan"; }
extern "C" const char *xp_crypto_provider_version(void)
{
	static const std::string version = Botan::version_string();
	return version.c_str();
}
extern "C" const char *xp_crypto_last_error(void) { return xp_ca_last_error(); }

extern "C" int
xp_crypto_random(void *out, size_t len)
{
	if (out == nullptr && len != 0)
		return XP_CRYPTO_ERR_INVALID;
	try {
		Botan::AutoSeeded_RNG rng;
		rng.randomize(static_cast<uint8_t *>(out), len);
		return XP_CRYPTO_OK;
	} catch (...) {
		return XP_CRYPTO_ERR;
	}
}
