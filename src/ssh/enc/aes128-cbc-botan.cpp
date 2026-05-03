/*
 * enc/aes128-cbc-botan.cpp -- AES-128-CBC encryption via Botan3 native C++ API.
 *
 * Implements the crypto operations.  Registration lives in the
 * companion .c file which includes the C module header.
 */

#include <botan/cipher_mode.h>

#include <cstdlib>
#include <memory>

#include "deucessh.h"

#define AES128_CBC_BLOCK_SIZE 16
#define AES128_CBC_KEY_SIZE   16

struct dssh_enc_ctx {
	std::unique_ptr<Botan::Cipher_Mode> cipher;
};

extern "C" int
dssh_botan_aes128cbc_init(const uint8_t *key, const uint8_t *iv, bool encrypt_dir, dssh_enc_ctx **ctx)
{
	try {
		Botan::Cipher_Dir dir = encrypt_dir ? Botan::Cipher_Dir::Encryption
		                                    : Botan::Cipher_Dir::Decryption;
		auto cipher = Botan::Cipher_Mode::create_or_throw("AES-128/CBC/NoPadding", dir);
		cipher->set_key(key, AES128_CBC_KEY_SIZE);
		cipher->start(iv, AES128_CBC_BLOCK_SIZE);

		dssh_enc_ctx *cbd = new (std::nothrow) dssh_enc_ctx;

		if (cbd == NULL)
			return DSSH_ERROR_ALLOC;
		cbd->cipher = std::move(cipher);
		*ctx        = cbd;
		return 0;
	}
	catch (...) {
		return DSSH_ERROR_INIT;
	}
}

extern "C" int
dssh_botan_aes128cbc_crypt(uint8_t *buf, size_t bufsz, dssh_enc_ctx *ctx)
{
	if (ctx == NULL)
		return DSSH_ERROR_INIT;
	if (bufsz % AES128_CBC_BLOCK_SIZE != 0)
		return DSSH_ERROR_INVALID;

	try {
		size_t written = ctx->cipher->process(buf, bufsz);

		if (written != bufsz)
			return DSSH_ERROR_INIT;
		return 0;
	}
	catch (...) {
		return DSSH_ERROR_INIT;
	}
}

extern "C" void
dssh_botan_aes128cbc_cleanup(dssh_enc_ctx *ctx)
{
	try {
		delete ctx;
	}
	catch (...) {
	}
}
