/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright Rob Swindell - http://www.synchro.net/copyright.html			*
 *																			*
 * This library is free software; you can redistribute it and/or			*
 * modify it under the terms of the GNU Lesser General Public License		*
 * as published by the Free Software Foundation; either version 2			*
 * of the License, or (at your option) any later version.					*
 ****************************************************************************/

/*
 * xp_crypt — Botan 3 backend.
 *
 * PBKDF2-HMAC-SHA256 via Botan::PasswordBasedKeyDerivationFamily, and
 * symmetric encrypt/decrypt via Botan::Cipher_Mode (block, with
 * NoPadding) or Botan::StreamCipher.  Byte-compatible with the
 * OpenSSL backend — same KDF output and on-disk IV layout.
 *
 * Ciphers absent from Botan 3 (IDEA, RC2) fall through to the
 * decrypt-only registry, where consumers register their own
 * reference implementations via xp_crypt_register_legacy_decrypt().
 */

#include <cstdint>
#include <cstring>
#include <memory>
#include <string>

#include <botan/auto_rng.h>
#include <botan/cipher_mode.h>
#include <botan/pwdhash.h>
#include <botan/stream_cipher.h>

#include "xp_crypt.h"

/* -------------------------------------------------------------- legacy */

static xp_crypt_legacy_decrypt_fn_t legacy_decrypt_tab[XP_CRYPT_ALGO_MAX];

extern "C" void
xp_crypt_register_legacy_decrypt(int algo, xp_crypt_legacy_decrypt_fn_t fn)
{
	if (algo > 0 && algo < XP_CRYPT_ALGO_MAX)
		legacy_decrypt_tab[algo] = fn;
}

/* ---------------------------------------------------- cipher mapping */

struct CipherSpec {
	const char *block_name;		/* Botan Cipher_Mode spec; NULL if stream */
	const char *stream_name;	/* Botan StreamCipher name; NULL if block */
	int         block_size;		/* bytes */
	int         iv_size;		/* bytes */
};

/*
 * Pick a Botan cipher spec for (algo, key_bits).  Returns {NULL,NULL,...}
 * when the combination isn't representable — callers fall through to
 * the legacy decrypt registry.
 */
static CipherSpec
cipher_spec_for(int algo, int key_bits)
{
	switch (algo) {
		case XP_CRYPT_ALGO_AES: {
			switch (key_bits) {
				case 128: return { "AES-128/CBC/NoPadding", nullptr, 16, 16 };
				case 192: return { "AES-192/CBC/NoPadding", nullptr, 16, 16 };
				case 256: return { "AES-256/CBC/NoPadding", nullptr, 16, 16 };
			}
			break;
		}
		case XP_CRYPT_ALGO_CHACHA20:
			return { nullptr, "ChaCha(20)", 1, 16 };
		case XP_CRYPT_ALGO_3DES:
			return { "TripleDES/CBC/NoPadding", nullptr, 8, 8 };
		case XP_CRYPT_ALGO_CAST:
			return { "CAST-128/CBC/NoPadding", nullptr, 8, 8 };
		case XP_CRYPT_ALGO_RC4:
			return { nullptr, "RC4", 1, 0 };
		default:
			break;
	}
	return { nullptr, nullptr, 0, 0 };
}

static int
default_key_bytes(int algo)
{
	switch (algo) {
		case XP_CRYPT_ALGO_AES:      return 32;
		case XP_CRYPT_ALGO_CHACHA20: return 32;
		case XP_CRYPT_ALGO_3DES:     return 24;
		case XP_CRYPT_ALGO_CAST:     return 16;
		case XP_CRYPT_ALGO_RC4:      return 16;
		case XP_CRYPT_ALGO_RC2:      return 16;
		case XP_CRYPT_ALGO_IDEA:     return 16;
		default:                     return 0;
	}
}

/* --------------------------------------------------------- context */

struct xp_crypt_ctx {
	int algo;
	int key_bits;
	bool encrypt;

	/* Botan-backed path (NULL when using legacy fallback). */
	std::unique_ptr<Botan::Cipher_Mode>  block;	/* set for block ciphers */
	std::unique_ptr<Botan::StreamCipher> stream;	/* set for stream ciphers */
	CipherSpec                           spec;

	/* Legacy fallback: derived key + IV held here, process() calls fn. */
	xp_crypt_legacy_decrypt_fn_t legacy_fn;
	uint8_t  key[XP_CRYPT_MAX_KEYSIZE];
	size_t   key_len;
	uint8_t  iv[XP_CRYPT_MAX_IVSIZE];
	size_t   iv_len;
};

/* ---------------------------------------------------------------- open */

/*
 * Derive `out_len` bytes of key material using the requested KDF.
 * Returns 0 on success, non-zero on error.
 */
static int
derive_key(const struct xp_crypt_kdf_params *kdf,
           const void *pass, size_t plen,
           const void *salt, size_t slen,
           uint8_t *out, size_t out_len)
{
	try {
		switch (kdf->kdf) {
			case XP_CRYPT_KDF_PBKDF2_HMAC_SHA256: {
				if (kdf->iterations < 1)
					return -1;
				auto family = Botan::PasswordHashFamily::create_or_throw(
				    "PBKDF2(HMAC(SHA-256))");
				auto hash = family->from_iterations(
				    static_cast<size_t>(kdf->iterations));
				hash->derive_key(out, out_len,
				    static_cast<const char *>(pass), plen,
				    static_cast<const uint8_t *>(salt), slen);
				return 0;
			}
			case XP_CRYPT_KDF_SCRYPT: {
				if (kdf->cost_log2 < 1 || kdf->cost_log2 > 31 ||
				    kdf->block_size < 1 || kdf->parallelism < 1)
					return -1;
				auto family = Botan::PasswordHashFamily::create_or_throw("Scrypt");
				/* Botan's Scrypt::from_params takes (N, r, p). */
				auto hash = family->from_params(
				    static_cast<size_t>(1ULL << kdf->cost_log2),
				    static_cast<size_t>(kdf->block_size),
				    static_cast<size_t>(kdf->parallelism));
				hash->derive_key(out, out_len,
				    static_cast<const char *>(pass), plen,
				    static_cast<const uint8_t *>(salt), slen);
				return 0;
			}
			default:
				return -1;
		}
	}
	catch (const std::exception &) {
		return -1;
	}
}

extern "C" xp_crypt_t
xp_crypt_open(int algo, int key_bits,
              const void *salt, size_t slen,
              const struct xp_crypt_kdf_params *kdf,
              const void *pass, size_t plen,
              bool encrypt)
{
	if (salt == nullptr || slen == 0 || kdf == nullptr || pass == nullptr)
		return nullptr;
	if (algo <= 0 || algo >= XP_CRYPT_ALGO_MAX)
		return nullptr;

	int kbytes = (key_bits == 0) ? default_key_bytes(algo) : (key_bits + 7) / 8;
	if (kbytes <= 0 || kbytes > XP_CRYPT_MAX_KEYSIZE)
		return nullptr;

	auto *ctx = new (std::nothrow) xp_crypt_ctx{};
	if (ctx == nullptr)
		return nullptr;
	ctx->algo     = algo;
	ctx->key_bits = kbytes * 8;
	ctx->encrypt  = encrypt;

	if (derive_key(kdf, pass, plen, salt, slen, ctx->key,
	               static_cast<size_t>(kbytes)) != 0) {
		delete ctx;
		return nullptr;
	}
	ctx->key_len = kbytes;

	ctx->spec = cipher_spec_for(algo, ctx->key_bits);

	if (ctx->spec.block_name != nullptr) {
		try {
			ctx->block = Botan::Cipher_Mode::create_or_throw(
			    ctx->spec.block_name,
			    encrypt ? Botan::Cipher_Dir::Encryption : Botan::Cipher_Dir::Decryption);
			ctx->block->set_key(ctx->key, ctx->key_len);
		}
		catch (const std::exception &) {
			delete ctx;
			return nullptr;
		}
		return ctx;
	}
	if (ctx->spec.stream_name != nullptr) {
		try {
			ctx->stream = Botan::StreamCipher::create_or_throw(ctx->spec.stream_name);
			ctx->stream->set_key(ctx->key, ctx->key_len);
		}
		catch (const std::exception &) {
			delete ctx;
			return nullptr;
		}
		return ctx;
	}

	/* Backend lacks this cipher — fall through to legacy registry. */
	if (encrypt) {
		delete ctx;
		return nullptr;
	}
	ctx->legacy_fn = legacy_decrypt_tab[algo];
	if (ctx->legacy_fn == nullptr) {
		delete ctx;
		return nullptr;
	}
	return ctx;
}

/* ---------------------------------------------------------- attributes */

extern "C" int
xp_crypt_blocksize(xp_crypt_t ctx)
{
	if (ctx == nullptr)
		return 0;
	return ctx->spec.block_size;	/* zero or 1 means stream */
}

extern "C" int
xp_crypt_ivsize(xp_crypt_t ctx)
{
	if (ctx == nullptr)
		return 0;
	return ctx->spec.iv_size;
}

extern "C" int
xp_crypt_set_iv(xp_crypt_t ctx, const void *iv, size_t n)
{
	if (ctx == nullptr || (iv == nullptr && n > 0))
		return -1;
	if (n > XP_CRYPT_MAX_IVSIZE)
		return -1;
	if (n > 0)
		std::memcpy(ctx->iv, iv, n);
	ctx->iv_len = n;
	try {
		if (ctx->block != nullptr) {
			ctx->block->start(ctx->iv, ctx->iv_len);
		}
		else if (ctx->stream != nullptr && ctx->iv_len > 0) {
			ctx->stream->set_iv(ctx->iv, ctx->iv_len);
		}
	}
	catch (const std::exception &) {
		return -1;
	}
	return 0;
}

extern "C" int
xp_crypt_gen_iv(xp_crypt_t ctx, void *out, size_t out_size)
{
	if (ctx == nullptr || out == nullptr)
		return -1;
	int n = xp_crypt_ivsize(ctx);
	if (n <= 0 || static_cast<size_t>(n) > out_size ||
	    static_cast<size_t>(n) > XP_CRYPT_MAX_IVSIZE)
		return -1;
	try {
		Botan::AutoSeeded_RNG rng;
		rng.randomize(ctx->iv, static_cast<size_t>(n));
	}
	catch (const std::exception &) {
		return -1;
	}
	ctx->iv_len = static_cast<size_t>(n);
	std::memcpy(out, ctx->iv, static_cast<size_t>(n));
	try {
		if (ctx->block != nullptr)
			ctx->block->start(ctx->iv, ctx->iv_len);
		else if (ctx->stream != nullptr)
			ctx->stream->set_iv(ctx->iv, ctx->iv_len);
	}
	catch (const std::exception &) {
		return -1;
	}
	return 0;
}

extern "C" int
xp_crypt_process(xp_crypt_t ctx, void *buf, size_t n)
{
	if (ctx == nullptr || buf == nullptr)
		return -1;
	if (ctx->legacy_fn != nullptr) {
		return ctx->legacy_fn(ctx->key_bits,
		    ctx->key, ctx->key_len,
		    ctx->iv, ctx->iv_len,
		    buf, n);
	}
	try {
		if (ctx->block != nullptr) {
			/* CBC/NoPadding: process block-aligned data in place.
			   update_granularity() is the block size, so process()
			   consumes exactly n bytes when n is a multiple. */
			ctx->block->process(static_cast<uint8_t *>(buf), n);
			return 0;
		}
		if (ctx->stream != nullptr) {
			ctx->stream->cipher1(static_cast<uint8_t *>(buf), n);
			return 0;
		}
	}
	catch (const std::exception &) {
		return -1;
	}
	return -1;
}

extern "C" void
xp_crypt_close(xp_crypt_t ctx)
{
	if (ctx == nullptr)
		return;
	std::memset(ctx->key, 0, sizeof(ctx->key));
	std::memset(ctx->iv,  0, sizeof(ctx->iv));
	delete ctx;
}
