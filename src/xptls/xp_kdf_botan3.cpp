#include "xp_kdf.h"

#include <botan/pwdhash.h>

#include <limits>
#include <memory>
#include <string>

static const char *
digest_name(enum xp_digest_algorithm digest)
{
	switch (digest) {
		case XP_DIGEST_SHA256: return "SHA-256";
		case XP_DIGEST_SHA384: return "SHA-384";
		case XP_DIGEST_SHA512: return "SHA-512";
	}
	return nullptr;
}

static bool
scrypt_valid(uint64_t n, uint32_t r, uint32_t p)
{
	return n >= 2 && (n & (n - 1)) == 0 && r != 0 && p != 0;
}

static bool
scrypt_fits(uint64_t n, uint32_t r, uint32_t p)
{
	const uint64_t limit = XP_KDF_SCRYPT_MAX_MEMORY;
	const uint64_t unit = UINT64_C(128) * r;
	const uint64_t blocks = limit / unit;
	/* Match OpenSSL's Blen + Vlen accounting:
	 * 128 * r * p + 128 * r * (N + 2). */
	if (n > blocks || blocks - n < 2)
		return false;
	return p <= blocks - n - 2;
}

extern "C" int
xp_kdf_pbkdf2(enum xp_digest_algorithm digest,
	const void *password, size_t password_len,
	const void *salt, size_t salt_len,
	uint64_t iterations, void *out, size_t out_len)
{
	const char *name = digest_name(digest);
	if ((password == nullptr && password_len != 0)
	    || (salt == nullptr && salt_len != 0) || (out == nullptr && out_len != 0)
	    || iterations == 0 || iterations > std::numeric_limits<size_t>::max())
		return XP_CRYPTO_ERR_INVALID;
	if (name == nullptr)
		return XP_CRYPTO_ERR_UNSUPPORTED;
	try {
		auto family = Botan::PasswordHashFamily::create_or_throw(
		    std::string("PBKDF2(HMAC(") + name + "))");
		auto hash = family->from_iterations(static_cast<size_t>(iterations));
		hash->derive_key(static_cast<uint8_t *>(out), out_len,
		    static_cast<const char *>(password), password_len,
		    static_cast<const uint8_t *>(salt), salt_len);
		return XP_CRYPTO_OK;
	}
	catch (...) {
		return XP_CRYPTO_ERR;
	}
}

extern "C" int
xp_kdf_scrypt(const void *password, size_t password_len,
	const void *salt, size_t salt_len, uint64_t n, uint32_t r, uint32_t p,
	void *out, size_t out_len)
{
	if ((password == nullptr && password_len != 0)
	    || (salt == nullptr && salt_len != 0) || (out == nullptr && out_len != 0)
	    || !scrypt_valid(n, r, p) || n > std::numeric_limits<size_t>::max())
		return XP_CRYPTO_ERR_INVALID;
	if (!scrypt_fits(n, r, p))
		return XP_CRYPTO_ERR_POLICY;
	try {
		auto family = Botan::PasswordHashFamily::create_or_throw("Scrypt");
		auto hash = family->from_params(static_cast<size_t>(n), r, p);
		hash->derive_key(static_cast<uint8_t *>(out), out_len,
		    static_cast<const char *>(password), password_len,
		    static_cast<const uint8_t *>(salt), salt_len);
		return XP_CRYPTO_OK;
	}
	catch (...) {
		return XP_CRYPTO_ERR;
	}
}
