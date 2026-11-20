/* Provider-owned storage lifetime probe, not a cryptographic hash oracle.
 * Its initialization deliberately leaves earlier input storage untouched. */
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/core.h>
#include <openssl/core_dispatch.h>
#include <openssl/core_names.h>
#include <openssl/params.h>
#include <openssl/provider.h>
#include <openssl/evp.h>

#include "deucessh.h"
#include "deucessh-crypto.h"
#include "dssh_test_ossl.h"

struct owned_hash {
	uint8_t input[128];
	size_t used;
};

static unsigned created, initialized, finalized, retired;
static bool valid = true;

static void *
probe_new(void *provider)
{
	(void)provider;
	struct owned_hash *hash = malloc(sizeof(*hash));
	if (hash == NULL)
		return NULL;
	*hash = (struct owned_hash){0};
	created++;
	return hash;
}

static int
probe_init(void *opaque, const OSSL_PARAM parameters[])
{
	(void)parameters;
	struct owned_hash *hash = opaque;
	/* A new initialization must follow retirement, not merely finalization. */
	valid = valid && created == retired + 1;
	for (size_t i = 0; i < sizeof(hash->input); i++)
		valid = valid && hash->input[i] == 0;
	hash->used = 0;
	initialized++;
	return 1;
}

static int
probe_update(void *opaque, const unsigned char *input, size_t length)
{
	struct owned_hash *hash = opaque;
	if (length > sizeof(hash->input) - hash->used)
		return 0;
	memcpy(&hash->input[hash->used], input, length);
	hash->used += length;
	return 1;
}

static int
probe_final(void *opaque, unsigned char *out, size_t *length, size_t capacity)
{
	(void)opaque;
	if (capacity < 64)
		return 0;
	memset(out, 0x31, 64);
	*length = 64;
	finalized++;
	return 1;
}

static void
probe_free(void *opaque)
{
	struct owned_hash *hash = opaque;
	dssh_cleanse(hash, sizeof(*hash));
	/* Only this producer knows its true allocation/cleanup size. */
	const uint8_t *bytes = opaque;
	for (size_t i = 0; i < sizeof(*hash); i++)
		valid = valid && bytes[i] == 0;
	retired++;
	free(hash);
}

static int
probe_parameters(OSSL_PARAM parameters[])
{
	OSSL_PARAM *p = OSSL_PARAM_locate(parameters, OSSL_DIGEST_PARAM_SIZE);
	if (p != NULL && OSSL_PARAM_set_size_t(p, 64) != 1)
		return 0;
	p = OSSL_PARAM_locate(parameters, OSSL_DIGEST_PARAM_BLOCK_SIZE);
	if (p != NULL && OSSL_PARAM_set_size_t(p, 128) != 1)
		return 0;
	p = OSSL_PARAM_locate(parameters, OSSL_DIGEST_PARAM_XOF);
	if (p != NULL && OSSL_PARAM_set_int(p, 0) != 1)
		return 0;
	return 1;
}

static const OSSL_DISPATCH digest_functions[] = {
	{OSSL_FUNC_DIGEST_NEWCTX, (void (*)(void))probe_new},
	{OSSL_FUNC_DIGEST_INIT, (void (*)(void))probe_init},
	{OSSL_FUNC_DIGEST_UPDATE, (void (*)(void))probe_update},
	{OSSL_FUNC_DIGEST_FINAL, (void (*)(void))probe_final},
	{OSSL_FUNC_DIGEST_FREECTX, (void (*)(void))probe_free},
	{OSSL_FUNC_DIGEST_GET_PARAMS, (void (*)(void))probe_parameters},
	{0, NULL},
};
static const OSSL_ALGORITHM algorithms[] = {
	{"SHA512", "provider=clear-probe", digest_functions, "lifetime probe only"},
	{NULL, NULL, NULL, NULL},
};

static const OSSL_ALGORITHM *
probe_query(void *provider, int operation, int *no_cache)
{
	(void)provider;
	*no_cache = 0;
	return operation == OSSL_OP_DIGEST ? algorithms : NULL;
}

static const OSSL_DISPATCH provider_functions[] = {
	{OSSL_FUNC_PROVIDER_QUERY_OPERATION, (void (*)(void))probe_query},
	{0, NULL},
};

static int
probe_provider_init(const OSSL_CORE_HANDLE *handle, const OSSL_DISPATCH *input,
    const OSSL_DISPATCH **output, void **provider)
{
	(void)handle;
	(void)input;
	*output = provider_functions;
	*provider = NULL;
	return 1;
}

int
main(void)
{
	if (OSSL_PROVIDER_add_builtin(NULL, "clear-probe", probe_provider_init) != 1)
		return 1;
	OSSL_PROVIDER *provider = OSSL_PROVIDER_load(NULL, "clear-probe");
	if (provider == NULL)
		return 2;
	if (EVP_set_default_properties(NULL, "provider=clear-probe") != 1) {
		int ret = OSSL_PROVIDER_unload(provider);
		(void)ret;
		return 3;
	}
	for (unsigned failure = 0; failure < 3; failure++) {
		created = 0;
		initialized = 0;
		finalized = 0;
		retired = 0;
		dssh_hash_ctx *hash = NULL;
		size_t length = 0;
		int ret = dssh_hash_init(&hash, "SHA-512", &length);
		valid = valid && ret == 0 && length == 64;
		uint8_t input[64], out[64];
		memset(input, 0x61, sizeof(input));
		if (ret == 0) {
			ret = dssh_hash_update(hash, input, sizeof(input));
			valid = valid && ret == 0;
			if (failure != 0) {
				int index = (int)failure; /* One: reset; two: reinitialization. */
				dssh_test_ossl_fail_after(index);
			}
			ret = dssh_hash_final(hash, out, sizeof(out));
			dssh_test_ossl_reset();
			valid = valid && ret == (failure == 0 ? 0 : DSSH_ERROR_INIT)
			    && finalized == 1 && retired == 1 && created == (failure == 0 ? 2U : 1U)
			    && initialized == created;
			if (failure != 0)
				valid = valid && dssh_hash_update(hash, input, sizeof(input)) == DSSH_ERROR_INVALID;
		}
		dssh_hash_free(hash);
		valid = valid && created == retired;
	}
	valid = EVP_set_default_properties(NULL, "") == 1 && valid;
	valid = OSSL_PROVIDER_unload(provider) == 1 && valid;
	return valid ? 0 : 4;
}
