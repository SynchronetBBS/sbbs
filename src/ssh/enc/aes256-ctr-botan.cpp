/*
 * enc/aes256-ctr-botan.cpp -- AES-256-CTR encryption via Botan3 native C++ API.
 *
 * Implements the crypto operations.  Registration lives in the
 * companion .c file which includes the C module header.
 */

#include <cstdlib>
#include <memory>

#include <botan/cipher_mode.h>

#include "deucessh.h"

#define AES256_CTR_BLOCK_SIZE 16
#define AES256_CTR_KEY_SIZE 32

struct dssh_enc_ctx {
	std::unique_ptr<Botan::Cipher_Mode> cipher;
};

extern "C" int
dssh_botan_aes256ctr_init(const uint8_t *key, const uint8_t *iv,
    bool encrypt_dir, dssh_enc_ctx **ctx)
{
	(void)encrypt_dir;

	try {
		auto cipher = Botan::Cipher_Mode::create_or_throw(
		    "AES-256/CTR-BE", Botan::Cipher_Dir::Encryption);
		cipher->set_key(key, AES256_CTR_KEY_SIZE);
		cipher->start(iv, AES256_CTR_BLOCK_SIZE);

		dssh_enc_ctx *cbd = new (std::nothrow) dssh_enc_ctx;

		if (cbd == NULL)
			return DSSH_ERROR_ALLOC;
		cbd->cipher = std::move(cipher);
		*ctx = cbd;
		return 0;
	} catch (...) {
		return DSSH_ERROR_INIT;
	}
}

extern "C" int
dssh_botan_aes256ctr_crypt(uint8_t *buf, size_t bufsz, dssh_enc_ctx *ctx)
{
	if (ctx == NULL)
		return DSSH_ERROR_INIT;

	try {
		size_t written = ctx->cipher->process(buf, bufsz);

		if (written != bufsz)
			return DSSH_ERROR_INIT;
		return 0;
	} catch (...) {
		return DSSH_ERROR_INIT;
	}
}

extern "C" void
dssh_botan_aes256ctr_cleanup(dssh_enc_ctx *ctx)
{
	try {
		delete ctx;
	} catch (...) {
	}
}
