/*
 * crypto/openssl.c -- OpenSSL 3.0+ implementation of deucessh-crypto.h.
 */

#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "deucessh.h"
#include "deucessh-crypto.h"

#ifdef DSSH_TESTING
#include "ssh-internal.h"
#endif

struct dssh_hash_ctx {
	EVP_MD_CTX *mdctx;
	EVP_MD     *md;
	size_t      digest_len;
};

/*
 * Map backend-neutral hash names to OpenSSL names.
 * OpenSSL uses "SHA256" / "SHA512" (no hyphen).
 * Unknown names pass through unchanged.
 */
static const char *
map_hash_name(const char *name)
{
	static const struct {
		const char *in;
		const char *out;
	} table[] = {
		{ "SHA-256", "SHA256" },
		{ "SHA-512", "SHA512" },
	};

	for (size_t i = 0; i < sizeof(table) / sizeof(table[0]); i++) {
		if (strcmp(name, table[i].in) == 0)
			return table[i].out;
	}
	return name;
}

DSSH_PUBLIC int
dssh_hash_init(dssh_hash_ctx **ctx, const char *name,
    size_t *out_digest_len)
{
	if (ctx == NULL || name == NULL || out_digest_len == NULL)
		return DSSH_ERROR_INVALID;

	const char *ossl_name = map_hash_name(name);

	EVP_MD *md = EVP_MD_fetch(NULL, ossl_name, NULL);

	if (md == NULL)
		return DSSH_ERROR_INIT;

	int md_size = EVP_MD_get_size(md);

	if (md_size <= 0) {
		EVP_MD_free(md);
		return DSSH_ERROR_INIT;
	}

	EVP_MD_CTX *mdctx = EVP_MD_CTX_new();

	if (mdctx == NULL) {
		EVP_MD_free(md);
		return DSSH_ERROR_ALLOC;
	}

	if (EVP_DigestInit_ex(mdctx, md, NULL) != 1) {
		EVP_MD_CTX_free(mdctx);
		EVP_MD_free(md);
		return DSSH_ERROR_INIT;
	}

	dssh_hash_ctx *hctx = malloc(sizeof(*hctx));

	if (hctx == NULL) {
		EVP_MD_CTX_free(mdctx);
		EVP_MD_free(md);
		return DSSH_ERROR_ALLOC;
	}

	hctx->mdctx = mdctx;
	hctx->md = md;
	hctx->digest_len = (size_t)md_size;

	*ctx = hctx;
	*out_digest_len = hctx->digest_len;
	return 0;
}

DSSH_PUBLIC int
dssh_hash_update(dssh_hash_ctx *ctx, const uint8_t *data, size_t len)
{
	if (ctx == NULL || ctx->mdctx == NULL)
		return DSSH_ERROR_INVALID;
	if (data == NULL && len > 0)
		return DSSH_ERROR_INVALID;
	if (EVP_DigestUpdate(ctx->mdctx, data, len) != 1)
		return DSSH_ERROR_INIT;
	return 0;
}

DSSH_PUBLIC int
dssh_hash_final(dssh_hash_ctx *ctx, uint8_t *out, size_t outlen)
{
	if (ctx == NULL || ctx->mdctx == NULL || out == NULL)
		return DSSH_ERROR_INVALID;
	if (outlen < ctx->digest_len)
		return DSSH_ERROR_INVALID;
	if (EVP_DigestFinal_ex(ctx->mdctx, out, NULL) != 1)
		return DSSH_ERROR_INIT;

	/* Re-initialize for reuse with the same algorithm.  If this
	 * fails, destroy the context so subsequent calls fail cleanly
	 * instead of using undefined EVP state. */
	if (EVP_DigestInit_ex(ctx->mdctx, ctx->md, NULL) != 1) {
		EVP_MD_CTX_free(ctx->mdctx);
		ctx->mdctx = NULL;
		return DSSH_ERROR_INIT;
	}

	return 0;
}

DSSH_PUBLIC void
dssh_hash_free(dssh_hash_ctx *ctx)
{
	if (ctx == NULL)
		return;
	EVP_MD_CTX_free(ctx->mdctx);
	EVP_MD_free(ctx->md);
	free(ctx);
}

DSSH_PUBLIC int
dssh_hash_oneshot(const char *name, const uint8_t *data, size_t len,
    uint8_t *out, size_t outlen)
{
	if (name == NULL || out == NULL)
		return DSSH_ERROR_INVALID;
	if (data == NULL && len > 0)
		return DSSH_ERROR_INVALID;

	const char *ossl_name = map_hash_name(name);

	EVP_MD *md = EVP_MD_fetch(NULL, ossl_name, NULL);

	if (md == NULL)
		return DSSH_ERROR_INIT;

	int md_size = EVP_MD_get_size(md);

	if (md_size <= 0) {
		EVP_MD_free(md);
		return DSSH_ERROR_INIT;
	}

	size_t digest_len = (size_t)md_size;

	if (outlen < digest_len) {
		EVP_MD_free(md);
		return DSSH_ERROR_INVALID;
	}

	unsigned int actual = 0;
	int ok = EVP_Digest(data, len, out, &actual, md, NULL);

	EVP_MD_free(md);

	if (ok != 1)
		return DSSH_ERROR_INIT;
	return 0;
}

DSSH_PUBLIC void
dssh_cleanse(void *buf, size_t len)
{
	if (buf != NULL && len > 0)
		OPENSSL_cleanse(buf, len);
}

DSSH_PUBLIC int
dssh_random(uint8_t *buf, size_t len)
{
	if (buf == NULL)
		return DSSH_ERROR_INVALID;
	if (len == 0)
		return 0;
#if SIZE_MAX > INT_MAX
	if (len > INT_MAX)
		return DSSH_ERROR_INVALID;
#endif
	int len_i = (int)len;

	if (RAND_bytes(buf, len_i) != 1)
		return DSSH_ERROR_INIT;
	return 0;
}

DSSH_PUBLIC int
dssh_crypto_memcmp(const void *a, const void *b, size_t len)
{
	if (len > 0 && (a == NULL || b == NULL))
		return 1;
	return CRYPTO_memcmp(a, b, len);
}

DSSH_PUBLIC int
dssh_base64_encode(const uint8_t *in, size_t len, char *out, size_t outlen)
{
	if (in == NULL || out == NULL)
		return DSSH_ERROR_INVALID;

#if SIZE_MAX > INT_MAX
	if (len > INT_MAX)
		return DSSH_ERROR_INVALID;
#endif

	/* Guard against size_t overflow in the formula below.
	 * On 32-bit, len near SIZE_MAX makes (len + 2) wrap. */
	if (len > (SIZE_MAX - 3) / 4)
		return DSSH_ERROR_TOOLONG;

	/* EVP_EncodeBlock output size: 4*ceil(len/3) + 1 for NUL */
	size_t needed = 4 * ((len + 2) / 3) + 1;

	if (outlen < needed)
		return DSSH_ERROR_TOOLONG;

	int len_i = (int)len;
	int ret = EVP_EncodeBlock((unsigned char *)out, in, len_i);

	if (ret < 0)
		return DSSH_ERROR_INIT;
	return ret;
}
