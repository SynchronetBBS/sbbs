#include <stdlib.h>
#include "xp_digest.h"
#include <openssl/evp.h>

struct xp_digest_context {
	EVP_MD_CTX *native;
	const EVP_MD *algorithm;
};

static const EVP_MD *digest_algorithm(enum xp_digest_algorithm algorithm)
{
	switch (algorithm) {
		case XP_DIGEST_SHA256: return EVP_sha256();
		case XP_DIGEST_SHA384: return EVP_sha384();
		case XP_DIGEST_SHA512: return EVP_sha512();
		default: return NULL;
	}
}

size_t xp_digest_size(enum xp_digest_algorithm algorithm)
{
	const EVP_MD *md = digest_algorithm(algorithm);
	return md == NULL ? 0 : (size_t)EVP_MD_get_size(md);
}

xp_digest_t xp_digest_create(enum xp_digest_algorithm algorithm)
{
	const EVP_MD *md = digest_algorithm(algorithm);
	xp_digest_t result = md == NULL ? NULL : calloc(1, sizeof(*result));
	if (result == NULL)
		return NULL;
	result->native = EVP_MD_CTX_new();
	result->algorithm = md;
	if (result->native == NULL || EVP_DigestInit_ex(result->native, md, NULL) != 1) {
		xp_digest_free(result);
		return NULL;
	}
	return result;
}

int xp_digest_update(xp_digest_t context, const void *data, size_t len)
{
	return context != NULL && (data != NULL || len == 0)
		&& EVP_DigestUpdate(context->native, data, len) == 1 ? 0 : -1;
}

int xp_digest_final(xp_digest_t context, void *out, size_t *len)
{
	if (context == NULL || len == NULL)
		return -1;
	size_t required = (size_t)EVP_MD_get_size(context->algorithm);
	if (out == NULL) {
		*len = required;
		return 0;
	}
	if (*len < required) {
		*len = required;
		return -1;
	}
	unsigned int actual = 0;
	if (EVP_DigestFinal_ex(context->native, out, &actual) != 1)
		return -1;
	*len = actual;
	return 0;
}

void xp_digest_free(xp_digest_t context)
{
	if (context != NULL) {
		EVP_MD_CTX_free(context->native);
		free(context);
	}
}

int xp_digest(enum xp_digest_algorithm algorithm, const void *data, size_t len,
	          void *out, size_t *out_len)
{
	xp_digest_t context = xp_digest_create(algorithm);
	if (context == NULL)
		return -1;
	int status = xp_digest_update(context, data, len);
	if (status == 0)
		status = xp_digest_final(context, out, out_len);
	xp_digest_free(context);
	return status;
}
