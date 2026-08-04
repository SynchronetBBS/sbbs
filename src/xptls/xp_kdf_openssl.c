#include "xp_kdf.h"

#include <limits.h>
#include <stdbool.h>

#include <openssl/core_names.h>
#include <openssl/evp.h>
#include <openssl/kdf.h>
#include <openssl/params.h>

static const EVP_MD *
digest_for(enum xp_digest_algorithm digest)
{
	switch (digest) {
		case XP_DIGEST_SHA256: return EVP_sha256();
		case XP_DIGEST_SHA384: return EVP_sha384();
		case XP_DIGEST_SHA512: return EVP_sha512();
	}
	return NULL;
}

static bool
scrypt_fits(uint64_t n, uint32_t r, uint32_t p)
{
	const uint64_t limit = XP_KDF_SCRYPT_MAX_MEMORY;
	const uint64_t unit = UINT64_C(128) * r;
	const uint64_t blocks = limit / unit;
	if (n > blocks || blocks - n < 2)
		return false;
	return p <= blocks - n - 2;
}

int
xp_kdf_pbkdf2(enum xp_digest_algorithm digest,
	const void *password, size_t password_len,
	const void *salt, size_t salt_len,
	uint64_t iterations, void *out, size_t out_len)
{
	const EVP_MD *md = digest_for(digest);
	if ((password == NULL && password_len != 0)
	    || (salt == NULL && salt_len != 0) || (out == NULL && out_len != 0)
	    || iterations == 0 || iterations > INT_MAX || password_len > INT_MAX
	    || salt_len > INT_MAX || out_len > INT_MAX)
		return XP_CRYPTO_ERR_INVALID;
	if (md == NULL)
		return XP_CRYPTO_ERR_UNSUPPORTED;
	return PKCS5_PBKDF2_HMAC((const char *)password, (int)password_len,
	    (const unsigned char *)salt, (int)salt_len, (int)iterations, md,
	    (int)out_len, out) == 1 ? XP_CRYPTO_OK : XP_CRYPTO_ERR;
}

int
xp_kdf_scrypt(const void *password, size_t password_len,
	const void *salt, size_t salt_len, uint64_t n, uint32_t r, uint32_t p,
	void *out, size_t out_len)
{
	if ((password == NULL && password_len != 0)
	    || (salt == NULL && salt_len != 0) || (out == NULL && out_len != 0)
	    || n < 2 || (n & (n - 1)) != 0 || r == 0 || p == 0)
		return XP_CRYPTO_ERR_INVALID;
	if (!scrypt_fits(n, r, p))
		return XP_CRYPTO_ERR_POLICY;
	EVP_KDF *kdf = EVP_KDF_fetch(NULL, "SCRYPT", NULL);
	if (kdf == NULL)
		return XP_CRYPTO_ERR_UNSUPPORTED;
	EVP_KDF_CTX *context = EVP_KDF_CTX_new(kdf);
	EVP_KDF_free(kdf);
	if (context == NULL)
		return XP_CRYPTO_ERR;
	uint64_t max_memory = XP_KDF_SCRYPT_MAX_MEMORY;
	OSSL_PARAM params[] = {
		OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_PASSWORD,
		    (void *)password, password_len),
		OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_SALT,
		    (void *)salt, salt_len),
		OSSL_PARAM_construct_uint64(OSSL_KDF_PARAM_SCRYPT_N, &n),
		OSSL_PARAM_construct_uint32(OSSL_KDF_PARAM_SCRYPT_R, &r),
		OSSL_PARAM_construct_uint32(OSSL_KDF_PARAM_SCRYPT_P, &p),
		OSSL_PARAM_construct_uint64(OSSL_KDF_PARAM_SCRYPT_MAXMEM,
		    &max_memory),
		OSSL_PARAM_construct_end()
	};
	int result = EVP_KDF_derive(context, out, out_len, params) == 1
	    ? XP_CRYPTO_OK : XP_CRYPTO_ERR_POLICY;
	EVP_KDF_CTX_free(context);
	return result;
}
