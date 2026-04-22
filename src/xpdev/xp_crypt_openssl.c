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
 * See the GNU Lesser General Public License for more details: lgpl.txt or	*
 * http://www.fsf.org/copyleft/lesser.html									*
 *																			*
 * For Synchronet coding style and modification guidelines, see				*
 * http://www.synchro.net/source.html										*
 ****************************************************************************/

/*
 * xp_crypt — OpenSSL backend.
 *
 * PBKDF2-HMAC-SHA256 via PKCS5_PBKDF2_HMAC() and symmetric encrypt/decrypt
 * via EVP_CIPHER_CTX. The legacy provider is loaded lazily the first time a
 * non-default-provider cipher is requested (CAST5, RC2, RC4, ...), so builds
 * against OpenSSL 3.x pick up legacy ciphers without runtime config.
 *
 * Ciphers unavailable in any OpenSSL provider (classically IDEA, sometimes
 * RC2 if legacy is missing) fall through to the decrypt-only registry —
 * consumers register handlers via xp_crypt_register_legacy_decrypt().
 */

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/provider.h>

#include "xp_crypt.h"

/* ------------------------------------------------------------------- legacy */

static xp_crypt_legacy_decrypt_fn_t legacy_decrypt_tab[XP_CRYPT_ALGO_MAX];

void
xp_crypt_register_legacy_decrypt(int algo, xp_crypt_legacy_decrypt_fn_t fn)
{
	if (algo > 0 && algo < XP_CRYPT_ALGO_MAX)
		legacy_decrypt_tab[algo] = fn;
}

/* --------------------------------------------------------- legacy provider */

/*
 * OpenSSL 3.x moves several older ciphers (CAST5, RC2, RC4, Blowfish, DES,
 * MD2, ...) into the "legacy" provider, which isn't loaded by default.
 * Load it on demand; once loaded it stays for process lifetime. A failure
 * to load legacy is non-fatal — the caller will then fall through to the
 * xp_crypt_register_legacy_decrypt() table.
 */
static bool legacy_provider_tried = false;
static OSSL_PROVIDER *legacy_provider = NULL;

static void
ensure_legacy_provider(void)
{
	if (legacy_provider_tried)
		return;
	legacy_provider_tried = true;
	legacy_provider = OSSL_PROVIDER_load(NULL, "legacy");
}

/* ---------------------------------------------------------- cipher lookup */

/*
 * Map xp_crypt_algo + key_bits to an OpenSSL cipher name. Returns NULL if
 * the combination isn't representable; callers fall back to the legacy
 * decrypt registry in that case.
 *
 * Note on AES: Cryptlib's INI header encodes only the key size (128/192/256),
 * implying CBC mode — that's what ini_file.c's IV-size handling assumes, and
 * it's the only AES mode Cryptlib's context-setup path exposes here.
 */
static const char *
cipher_name_for(int algo, int key_bits)
{
	switch (algo) {
		case XP_CRYPT_ALGO_AES:
			switch (key_bits) {
				case 128: return "AES-128-CBC";
				case 192: return "AES-192-CBC";
				case 256: return "AES-256-CBC";
			}
			return NULL;
		case XP_CRYPT_ALGO_CHACHA20:
			return "ChaCha20";
		case XP_CRYPT_ALGO_3DES:
			return "DES-EDE3-CBC";
		case XP_CRYPT_ALGO_CAST:
			return "CAST5-CBC";
		case XP_CRYPT_ALGO_RC2:
			return "RC2-CBC";
		case XP_CRYPT_ALGO_RC4:
			return "RC4";
		case XP_CRYPT_ALGO_IDEA:
			return "IDEA-CBC";	/* rarely present */
		default:
			return NULL;
	}
}

/*
 * Default key length in bytes for an algorithm, when the caller passes
 * key_bits = 0.
 */
static int
default_key_bytes(int algo)
{
	switch (algo) {
		case XP_CRYPT_ALGO_AES:      return 32;	/* AES-256 */
		case XP_CRYPT_ALGO_CHACHA20: return 32;
		case XP_CRYPT_ALGO_3DES:     return 24;
		case XP_CRYPT_ALGO_CAST:     return 16;
		case XP_CRYPT_ALGO_RC2:      return 16;
		case XP_CRYPT_ALGO_RC4:      return 16;
		case XP_CRYPT_ALGO_IDEA:     return 16;
		default:                     return 0;
	}
}

static EVP_CIPHER *
fetch_cipher(int algo, int key_bits)
{
	const char *name = cipher_name_for(algo, key_bits);
	EVP_CIPHER *c;
	if (name == NULL)
		return NULL;
	c = EVP_CIPHER_fetch(NULL, name, NULL);
	if (c != NULL)
		return c;
	ensure_legacy_provider();
	return EVP_CIPHER_fetch(NULL, name, NULL);
}

/* -------------------------------------------------------------- context */

struct xp_crypt_ctx {
	int               algo;
	int               key_bits;
	bool              encrypt;

	/* Backend-resolved cipher path (NULL when using legacy fallback). */
	EVP_CIPHER       *cipher;
	EVP_CIPHER_CTX   *evp;

	/* Legacy fallback: derived key + IV held here, process() calls fn. */
	xp_crypt_legacy_decrypt_fn_t legacy_fn;
	unsigned char     key[XP_CRYPT_MAX_KEYSIZE];
	size_t            key_len;
	unsigned char     iv[XP_CRYPT_MAX_IVSIZE];
	size_t            iv_len;
};

/* ---------------------------------------------------------------- open */

xp_crypt_t
xp_crypt_open(int algo, int key_bits,
              const void *salt, size_t slen,
              int iters,
              const void *pass, size_t plen,
              bool encrypt)
{
	struct xp_crypt_ctx *ctx;
	int kbytes;

	if (salt == NULL || slen == 0 || iters < 1 || pass == NULL)
		return NULL;
	if (algo <= 0 || algo >= XP_CRYPT_ALGO_MAX)
		return NULL;

	if (key_bits == 0)
		kbytes = default_key_bytes(algo);
	else
		kbytes = (key_bits + 7) / 8;
	if (kbytes <= 0 || kbytes > XP_CRYPT_MAX_KEYSIZE)
		return NULL;

	ctx = calloc(1, sizeof(*ctx));
	if (ctx == NULL)
		return NULL;
	ctx->algo = algo;
	ctx->key_bits = kbytes * 8;
	ctx->encrypt = encrypt;

	if (PKCS5_PBKDF2_HMAC((const char *)pass, (int)plen,
	                      (const unsigned char *)salt, (int)slen,
	                      iters, EVP_sha256(),
	                      kbytes, ctx->key) != 1)
		goto fail;
	ctx->key_len = kbytes;

	ctx->cipher = fetch_cipher(algo, ctx->key_bits);
	if (ctx->cipher != NULL) {
		ctx->evp = EVP_CIPHER_CTX_new();
		if (ctx->evp == NULL)
			goto fail;
		/*
		 * Init without key/IV now; set_iv() (decrypt) or gen_iv()
		 * (encrypt) will supply the IV and re-init with the full
		 * parameter set before process() runs data through.
		 */
		if (EVP_CipherInit_ex2(ctx->evp, ctx->cipher, NULL, NULL,
		                       encrypt ? 1 : 0, NULL) != 1)
			goto fail;
		/*
		 * RC2 defaults to 40-bit effective key; for encrypted INI
		 * files Cryptlib uses the full key length, so force that.
		 */
		if (algo == XP_CRYPT_ALGO_RC2) {
			if (EVP_CIPHER_CTX_ctrl(ctx->evp, EVP_CTRL_SET_RC2_KEY_BITS,
			                        ctx->key_bits, NULL) != 1)
				goto fail;
		}
		EVP_CIPHER_CTX_set_padding(ctx->evp, 0);
		return ctx;
	}

	/* Backend lacks this cipher — legacy decrypt fallback? */
	if (encrypt) {
		/* Writing a legacy cipher is not supported; only reading. */
		goto fail;
	}
	ctx->legacy_fn = legacy_decrypt_tab[algo];
	if (ctx->legacy_fn == NULL)
		goto fail;
	return ctx;

fail:
	xp_crypt_close(ctx);
	return NULL;
}

/* ---------------------------------------------------------- attributes */

/*
 * Cryptlib uses the presence of an IV-size attribute to distinguish block
 * ciphers from stream ciphers. Mirror that: for stream ciphers that
 * genuinely carry an IV (ChaCha20) we still return the IV size, because
 * ini_file.c uses a non-zero IV size to decide whether the IV block is
 * present in the file.
 */
int
xp_crypt_blocksize(xp_crypt_t ctx)
{
	if (ctx == NULL)
		return 0;
	if (ctx->cipher != NULL)
		return EVP_CIPHER_get_block_size(ctx->cipher);
	/* Legacy path: assume the caller knows it's block-mode; pull from
	   the algo.  CAST5/RC2/IDEA = 8, AES = 16, RC4/ChaCha20 = 1. */
	switch (ctx->algo) {
		case XP_CRYPT_ALGO_AES:      return 16;
		case XP_CRYPT_ALGO_3DES:     return 8;
		case XP_CRYPT_ALGO_CAST:     return 8;
		case XP_CRYPT_ALGO_RC2:      return 8;
		case XP_CRYPT_ALGO_IDEA:     return 8;
		case XP_CRYPT_ALGO_RC4:      return 1;
		case XP_CRYPT_ALGO_CHACHA20: return 1;
	}
	return 0;
}

int
xp_crypt_ivsize(xp_crypt_t ctx)
{
	if (ctx == NULL)
		return 0;
	if (ctx->cipher != NULL)
		return EVP_CIPHER_get_iv_length(ctx->cipher);
	/* Legacy fallback — IV sizes match Cryptlib's. */
	switch (ctx->algo) {
		case XP_CRYPT_ALGO_3DES:     return 8;
		case XP_CRYPT_ALGO_CAST:     return 8;
		case XP_CRYPT_ALGO_RC2:      return 8;
		case XP_CRYPT_ALGO_IDEA:     return 8;
		case XP_CRYPT_ALGO_AES:      return 16;
		case XP_CRYPT_ALGO_CHACHA20: return 16;
		case XP_CRYPT_ALGO_RC4:      return 0;
	}
	return 0;
}

static int
reload_key_iv(struct xp_crypt_ctx *ctx)
{
	if (EVP_CipherInit_ex2(ctx->evp, NULL, ctx->key, ctx->iv,
	                       ctx->encrypt ? 1 : 0, NULL) != 1)
		return -1;
	EVP_CIPHER_CTX_set_padding(ctx->evp, 0);
	return 0;
}

int
xp_crypt_set_iv(xp_crypt_t ctx, const void *iv, size_t n)
{
	if (ctx == NULL || (iv == NULL && n > 0))
		return -1;
	if (n > XP_CRYPT_MAX_IVSIZE)
		return -1;
	if (n > 0)
		memcpy(ctx->iv, iv, n);
	ctx->iv_len = n;
	if (ctx->evp != NULL)
		return reload_key_iv(ctx);
	return 0;	/* legacy path: fn consumes iv directly */
}

int
xp_crypt_gen_iv(xp_crypt_t ctx, void *out, size_t out_size)
{
	int n;

	if (ctx == NULL || out == NULL)
		return -1;
	n = xp_crypt_ivsize(ctx);
	if (n <= 0 || (size_t)n > out_size || (size_t)n > XP_CRYPT_MAX_IVSIZE)
		return -1;
	if (RAND_bytes(ctx->iv, n) != 1)
		return -1;
	ctx->iv_len = n;
	memcpy(out, ctx->iv, n);
	if (ctx->evp != NULL)
		return reload_key_iv(ctx);
	return 0;
}

int
xp_crypt_process(xp_crypt_t ctx, void *buf, size_t n)
{
	if (ctx == NULL || buf == NULL)
		return -1;
	if (n > INT_MAX)
		return -1;
	if (ctx->legacy_fn != NULL) {
		return ctx->legacy_fn(ctx->key_bits,
		                      ctx->key, ctx->key_len,
		                      ctx->iv,  ctx->iv_len,
		                      buf, n);
	}
	if (ctx->evp != NULL) {
		int out_len = 0;
		if (EVP_CipherUpdate(ctx->evp, buf, &out_len, buf, (int)n) != 1)
			return -1;
		/* Padding disabled, so out_len == n for block-aligned input. */
		return 0;
	}
	return -1;
}

void
xp_crypt_close(xp_crypt_t ctx)
{
	if (ctx == NULL)
		return;
	if (ctx->evp != NULL)
		EVP_CIPHER_CTX_free(ctx->evp);
	if (ctx->cipher != NULL)
		EVP_CIPHER_free(ctx->cipher);
	/* Scrub derived key material. */
	memset(ctx->key, 0, sizeof(ctx->key));
	memset(ctx->iv,  0, sizeof(ctx->iv));
	free(ctx);
}
