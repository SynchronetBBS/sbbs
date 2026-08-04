#include "xp_kdf.h"

int xp_kdf_pbkdf2(enum xp_digest_algorithm digest,
	const void *password, size_t password_len, const void *salt, size_t salt_len,
	uint64_t iterations, void *out, size_t out_len)
{
	(void)digest; (void)password; (void)password_len; (void)salt;
	(void)salt_len; (void)iterations; (void)out; (void)out_len;
	return XP_CRYPTO_ERR_DISABLED;
}

int xp_kdf_scrypt(const void *password, size_t password_len,
	const void *salt, size_t salt_len, uint64_t n, uint32_t r, uint32_t p,
	void *out, size_t out_len)
{
	(void)password; (void)password_len; (void)salt; (void)salt_len;
	(void)n; (void)r; (void)p; (void)out; (void)out_len;
	return XP_CRYPTO_ERR_DISABLED;
}
