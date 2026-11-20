/*
 * crypto/openssl.c -- OpenSSL 3.0+ implementation of deucessh-crypto.h.
 */

#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/rand.h>

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "deucessh-crypto.h"
#include "deucessh.h"

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
	    {"SHA-256", "SHA256"},
	    {"SHA-512", "SHA512"},
	};

	for (size_t i = 0; i < sizeof(table) / sizeof(table[0]); i++) {
		if (strcmp(name, table[i].in) == 0)
			return table[i].out;
	}
	return name;
}

DSSH_PUBLIC int
dssh_hash_init(dssh_hash_ctx **ctx, const char *name, size_t *out_digest_len)
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
#if INT_MAX > SIZE_MAX
	if (md_size > SIZE_MAX) {
		EVP_MD_free(md);
		return DSSH_ERROR_INVALID;
	}
#endif
	size_t digest_len = (size_t)md_size;

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

	hctx->mdctx      = mdctx;
	hctx->md         = md;
	hctx->digest_len = digest_len;

	*ctx            = hctx;
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
	if (len == 0)
		return 0;
	if (EVP_DigestUpdate(ctx->mdctx, data, len) != 1) {
		EVP_MD_CTX_free(ctx->mdctx);
		ctx->mdctx = NULL;
		return DSSH_ERROR_INIT;
	}
	return 0;
}

DSSH_PUBLIC int
dssh_hash_final(dssh_hash_ctx *ctx, uint8_t *out, size_t outlen)
{
	if (ctx == NULL || ctx->mdctx == NULL || out == NULL)
		return DSSH_ERROR_INVALID;
	if (outlen < ctx->digest_len)
		return DSSH_ERROR_INVALID;
	unsigned int actual = 0;
	if (EVP_DigestFinal_ex(ctx->mdctx, out, &actual) != 1 || actual != ctx->digest_len) {
		EVP_MD_CTX_free(ctx->mdctx);
		ctx->mdctx = NULL;
		return DSSH_ERROR_INIT;
	}

	/* Keep the wrapper, fetched method and EVP container, but retire the
	 * producer-owned algorithm state before reinitialization. Algorithm init
	 * alone need not erase every buffered input byte. The provider's cleanup
	 * handles its actual allocation; callers never inspect/clear opaque state. */
	if (EVP_MD_CTX_reset(ctx->mdctx) != 1 || EVP_DigestInit_ex(ctx->mdctx, ctx->md, NULL) != 1) {
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
dssh_hash_oneshot(const char *name, const uint8_t *data, size_t len, uint8_t *out, size_t outlen)
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
#if INT_MAX > SIZE_MAX
	if (md_size > SIZE_MAX) {
		EVP_MD_free(md);
		return DSSH_ERROR_INVALID;
	}
#endif

	size_t digest_len = (size_t)md_size;

	if (outlen < digest_len) {
		EVP_MD_free(md);
		return DSSH_ERROR_INVALID;
	}

	unsigned int actual = 0;
	int          ok     = EVP_Digest(data, len, out, &actual, md, NULL);

	EVP_MD_free(md);

	if (ok != 1 || actual != digest_len)
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
	if (RAND_priv_bytes_ex(NULL, buf, len, 256) != 1)
		return DSSH_ERROR_INIT;
	return 0;
}

DSSH_PUBLIC int
dssh_crypto_memcmp(const void *a, const void *b, size_t len)
{
	if (len == 0)
		return 0;
	if (a == NULL || b == NULL)
		return 1;
	return CRYPTO_memcmp(a, b, len);
}

DSSH_PUBLIC int
dssh_base64_encode(const uint8_t *in, size_t len, char *out, size_t outlen)
{
	if (in == NULL || out == NULL)
		return DSSH_ERROR_INVALID;

	/* Check the arithmetic and the API's int result before encoding. */
	if (len > SIZE_MAX - 2)
		return DSSH_ERROR_TOOLONG;
	size_t groups = (len + 2) / 3;
	if (groups > (SIZE_MAX - 1) / 4)
		return DSSH_ERROR_TOOLONG;
	size_t encoded_len = 4 * groups;
	if (encoded_len > INT_MAX)
		return DSSH_ERROR_TOOLONG;

	/* EVP_EncodeBlock output size: 4*ceil(len/3) + 1 for NUL */
	size_t needed = encoded_len + 1;

	if (outlen < needed)
		return DSSH_ERROR_TOOLONG;

	int len_i = (int)len;
	unsigned char *out_bytes = (unsigned char *)out;
	int encoded_i = (int)encoded_len;
	int ret = EVP_EncodeBlock(out_bytes, in, len_i);

	if (ret != encoded_i)
		return DSSH_ERROR_INIT;
	return ret;
}
