/* Real native RSA cache construction and independent arithmetic/byte oracles. */
#include "dssh_test.h"
#include "dssh_test_internal.h"
#include "dssh_test_alloc.h"
#include "dssh_test_ossl.h"
#include "deucessh-algorithms.h"
#include "rsa_cache_reference.h"
#define DSSH_TESTING
#include "key_algo/key-rsa-cache.h"
#undef DSSH_TESTING
#ifdef DSSH_CRYPTO_OPENSSL
#include <openssl/core_names.h>
#include <openssl/param_build.h>
#include "key_algo/key-context-openssl.h"
#endif

static int
test_number_models(void)
{
	const size_t limits[] = {0, 1, 7, 255, 8191, 65535};
	unsigned long cases = 0;
	for (uint64_t bits = 0; bits <= 65536; bits++) {
		for (size_t i = 0; i < sizeof(limits) / sizeof(limits[0]); i++) {
			struct dssh_rsa_cache_number number = {42, false};
			int ret = dssh_rsa_cache_number_size(bits, limits[i], &number);
			uint64_t expected = (bits + 7) / 8;
			bool accept = bits != 0 && expected <= limits[i];
			ASSERT_TRUE(accept ? ret == 0 && number.bytes == expected && number.pad == (bits % 8 == 0)
			    : ret == DSSH_ERROR_INVALID && number.bytes == 42 && !number.pad);
			cases++;
		}
	}
	printf("RSA cache numbers: bits=65537 limits=6 cases=%lu\n", cases);
	return TEST_PASS;
}

static int
test_layout_models(void)
{
	const size_t limits[] = {0, 19, 31, 255, 65535};
	const uint32_t wire_limits[] = {255, 65535};
	unsigned long cases = 0;
	for (size_t bytes = 0; bytes < 65535; bytes++) {
		for (unsigned int pads = 0; pads < 4; pads++) {
			struct dssh_rsa_cache_number e = {bytes, (pads & 1) != 0};
			struct dssh_rsa_cache_number n = {3, (pads & 2) != 0};
			for (size_t a = 0; a < sizeof(limits) / sizeof(limits[0]); a++) {
				for (size_t b = 0; b < sizeof(wire_limits) / sizeof(wire_limits[0]); b++) {
					struct dssh_rsa_cache_layout out = {{42, false}, {42, false}, 42, 42, 42};
					int ret = dssh_rsa_cache_size(e, n, limits[a], wire_limits[b], &out);
					uint64_t ew = bytes + (pads & 1 ? UINT64_C(1) : UINT64_C(0));
					uint64_t nw = pads & 2 ? 4 : 3;
					uint64_t expected = UINT64_C(19) + ew + nw;
					bool accepted = bytes != 0 && ew <= wire_limits[b] && nw <= wire_limits[b]
					    && expected <= limits[a];
					ASSERT_TRUE(accepted ? ret == 0 && out.length == expected && out.exponent_wire == ew
					    && out.modulus_wire == nw : ret == DSSH_ERROR_INVALID && out.length == 42
					    && out.exponent_wire == 42 && out.modulus_wire == 42);
					cases++;
				}
			}
		}
	}
	struct dssh_rsa_cache_number edge = {SIZE_MAX, true}, one = {1, false};
	struct dssh_rsa_cache_layout out = {{0, false}, {0, false}, 0, 0, 42};
	ASSERT_EQ(dssh_rsa_cache_size(edge, one, SIZE_MAX, UINT32_MAX, &out), DSSH_ERROR_INVALID);
	ASSERT_EQ(out.length, 42);
	ASSERT_EQ(dssh_rsa_cache_size(one, edge, SIZE_MAX, UINT32_MAX, &out), DSSH_ERROR_INVALID);
	ASSERT_EQ(out.length, 42);
	size_t native_limit = SIZE_MAX;
#if SIZE_MAX > UINT32_MAX
	native_limit = UINT32_MAX;
#endif
	struct dssh_rsa_cache_number maximum = {(size_t)(native_limit - 20), false};
	ASSERT_EQ(dssh_rsa_cache_size_native(maximum, one, &out), 0);
	ASSERT_EQ(out.length, native_limit);
	maximum.bytes++;
	out.length = 42;
	ASSERT_EQ(dssh_rsa_cache_size_native(maximum, one, &out), DSSH_ERROR_INVALID);
	ASSERT_EQ(out.length, 42);
	printf("RSA cache layouts: sizes=65535 pads=4 buffers=5 wires=2 cases=%lu native-edges=4\n", cases);
	return TEST_PASS;
}

#ifdef DSSH_CRYPTO_OPENSSL
static EVP_PKEY *
native_key(const uint8_t *e, size_t esize, const uint8_t *n, size_t nsize)
{
	if (esize > INT_MAX || nsize > INT_MAX)
		return NULL;
	int elen = (int)esize;
	int nlen = (int)nsize;
	BIGNUM *ebn = BN_bin2bn(e, elen, NULL), *nbn = BN_bin2bn(n, nlen, NULL);
	OSSL_PARAM_BLD *builder = OSSL_PARAM_BLD_new();
	OSSL_PARAM *params = NULL;
	EVP_PKEY_CTX *context = NULL;
	EVP_PKEY *key = NULL;
	if (ebn == NULL || nbn == NULL || builder == NULL
	    || OSSL_PARAM_BLD_push_BN(builder, OSSL_PKEY_PARAM_RSA_E, ebn) != 1
	    || OSSL_PARAM_BLD_push_BN(builder, OSSL_PKEY_PARAM_RSA_N, nbn) != 1)
		goto done;
	params = OSSL_PARAM_BLD_to_param(builder);
	context = EVP_PKEY_CTX_new_from_name(NULL, "RSA", NULL);
	if (params == NULL || context == NULL || EVP_PKEY_fromdata_init(context) != 1
	    || EVP_PKEY_fromdata(context, &key, EVP_PKEY_PUBLIC_KEY, params) != 1) {
		EVP_PKEY_free(key);
		key = NULL;
	}
done:
	EVP_PKEY_CTX_free(context);
	OSSL_PARAM_free(params);
	OSSL_PARAM_BLD_free(builder);
	BN_free(ebn);
	BN_free(nbn);
	return key;
}

static bool
cache_case(unsigned int algorithm, const uint8_t *e, size_t esize,
    const uint8_t *n, size_t nsize, bool zero)
{
	dssh_test_alloc_reset();
	dssh_test_ossl_reset();
	dssh_test_reset_global_config();
	int ret = algorithm == 0 ? dssh_register_rsa_sha2_256() : dssh_register_rsa_sha2_512();
	dssh_key_algo key = find_key_algo(algorithm == 0 ? "rsa-sha2-256" : "rsa-sha2-512");
	struct dssh_openssl_key_context *context = NULL;
	bool valid = false;
	if (ret != 0 || key == NULL)
		goto done;
	context = calloc(1, sizeof(*context));
	if (context == NULL)
		goto done;
	context->pkey = native_key(e, esize, n, nsize);
	if (context->pkey == NULL)
		goto done;
	dssh_key_algo_ctx *opaque = (dssh_key_algo_ctx *)context;
	key->ctx = opaque; /* This actual module instance now owns its native key. */
	context = NULL;
	uint8_t marker = 0, expected[1024];
	const uint8_t *blob = &marker;
	size_t length = 42, expected_len = 0;
	if (!zero && !rsa_cache_reference(expected, sizeof(expected), e, esize, n, nsize, &expected_len))
		goto done;
	if (!zero) {
		dssh_test_alloc_fail_after(0);
		ret = key->pubkey(&blob, &length, key->ctx);
		struct dssh_openssl_key_context *actual = (struct dssh_openssl_key_context *)key->ctx;
		bool rejected = ret == DSSH_ERROR_ALLOC && dssh_test_alloc_count() == 1
		    && blob == &marker && length == 42 && actual->pub_blob == NULL && actual->pub_blob_len == 0;
		dssh_test_alloc_reset();
		if (!rejected)
			goto done;
	}
	dssh_test_alloc_reset();
	dssh_test_alloc_fail_after(INT_MAX);
	ret = key->pubkey(&blob, &length, key->ctx);
	int allocations = dssh_test_alloc_count();
	dssh_test_alloc_reset();
	if (zero)
		valid = ret == DSSH_ERROR_INVALID && allocations == 0 && blob == &marker && length == 42;
	else {
		valid = ret == 0 && allocations == 1 && blob != NULL && length == expected_len
		    && memcmp(blob, expected, length) == 0;
		const uint8_t *again = NULL;
		size_t again_len = 0;
		dssh_test_alloc_fail_after(0);
		ret = key->pubkey(&again, &again_len, key->ctx);
		valid = ret == 0 && again == blob && again_len == length && dssh_test_alloc_count() == 0 && valid;
		dssh_test_alloc_reset();
	}
done:
	if (context != NULL) {
		EVP_PKEY_free(context->pkey);
		free(context);
	}
	dssh_test_alloc_reset();
	dssh_test_ossl_reset();
	dssh_test_reset_global_config();
	return valid;
}

static int
test_public_values(void)
{
	bool valid = true;
	unsigned int completed = 0;
	const size_t sizes[] = {1, 7, 8, 31, 32, 127, 128, 256};
	for (unsigned int algorithm = 0; algorithm < 2; algorithm++) {
		for (unsigned int pad = 0; pad < 4; pad++) {
			for (size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++) {
				uint8_t exponent[3] = {pad & 1 ? 128 : 127, 0, 3};
				uint8_t modulus[256];
				memset(modulus, 0xab, sizes[i]);
				modulus[0] = pad & 2 ? 128 : 127;
				bool one = cache_case(algorithm, exponent, sizeof(exponent), modulus, sizes[i], false);
				completed += one ? 1U : 0U;
				valid = one && valid;
			}
		}
	}
	printf("RSA native caches: algorithms=2 pads=4 sizes=8 completed=%u valid=%u\n", completed, valid ? 1U : 0U);
	ASSERT_TRUE(valid && completed == 64);
	return TEST_PASS;
}

static int
test_zero_modulus(void)
{
	const uint8_t exponent[] = {1, 0, 1}, empty[] = {0};
	bool a = cache_case(0, exponent, sizeof(exponent), empty, 0, true);
	bool b = cache_case(1, exponent, sizeof(exponent), empty, 0, true);
	printf("RSA zero modulus: algorithms=2 rejected=%u\n", (a ? 1U : 0U) + (b ? 1U : 0U));
	ASSERT_TRUE(a && b);
	return TEST_PASS;
}

static int
test_native_numbers(void)
{
	const int bits[] = {1, 7, 8, 9, 15, 16, 31, 32, 63, 64, 255, 256, 2047, 2048, 4096};
	BIGNUM *number = BN_new();
	bool valid = number != NULL;
	for (size_t i = 0; valid && i < sizeof(bits) / sizeof(bits[0]); i++) {
		BN_zero(number);
		if (BN_set_bit(number, bits[i] - 1) != 1) {
			valid = false;
			break;
		}
		struct dssh_rsa_cache_number output = {42, false};
		int ret = dssh_rsa_cache_native_number(number, &output);
		size_t expected = (size_t)((bits[i] + 7) / 8);
		valid = ret == 0 && output.bytes == expected && output.pad == (bits[i] % 8 == 0);
		BN_set_negative(number, 1);
		output = (struct dssh_rsa_cache_number){42, false};
		valid = dssh_rsa_cache_native_number(number, &output) == DSSH_ERROR_INVALID
		    && output.bytes == 42 && !output.pad && valid;
	}
	if (number != NULL) {
		BN_zero(number);
		struct dssh_rsa_cache_number output = {42, false};
		valid = dssh_rsa_cache_native_number(number, &output) == DSSH_ERROR_INVALID && output.bytes == 42 && valid;
	}
	BN_free(number);
	printf("RSA native integers: positive=15 negative=15 zero=1 valid=%u\n", valid ? 1U : 0U);
	ASSERT_TRUE(valid);
	return TEST_PASS;
}
static int
test_zero_exponent(void)
{
	const uint8_t empty[] = {0}, modulus[] = {127, 0xab, 1};
	bool a = cache_case(0, empty, 0, modulus, sizeof(modulus), true);
	bool b = cache_case(1, empty, 0, modulus, sizeof(modulus), true);
	printf("RSA zero exponent: algorithms=2 rejected=%u\n", (a ? 1U : 0U) + (b ? 1U : 0U));
	ASSERT_TRUE(a && b);
	return TEST_PASS;
}

static bool
signable(dssh_key_algo key, const uint8_t *public_blob, size_t public_len)
{
	const uint8_t message[] = {'c', 'a', 'c', 'h', 'e'};
	struct dssh_bytes_result result = {0};
	int ret = key->sign(&result, message, sizeof(message), key->ctx);
	bool valid = ret == 0 && result.bytes.data != NULL && result.bytes.len != 0 && result.lease.release != NULL;
	if (valid)
		valid = key->verify(public_blob, public_len, result.bytes.data, result.bytes.len, message, sizeof(message)) == 0;
	dssh_result_release(&result.lease);
	return valid;
}

static int
test_publication_errors(void)
{
	bool valid = true;
	unsigned int rejected = 0;
	const char *cache = getenv("DSSH_TEST_RSA_KEY");
	for (unsigned int algorithm = 0; algorithm < 2; algorithm++) {
		dssh_test_reset_global_config();
		int ret = algorithm == 0 ? dssh_register_rsa_sha2_256() : dssh_register_rsa_sha2_512();
		int (*load)(const char *, dssh_pem_password_cb, void *) = algorithm == 0 ? dssh_rsa_sha2_256_load_key_file : dssh_rsa_sha2_512_load_key_file;
		int (*generate)(unsigned int) = algorithm == 0 ? dssh_rsa_sha2_256_generate_key : dssh_rsa_sha2_512_generate_key;
		dssh_key_algo key = find_key_algo(algorithm == 0 ? "rsa-sha2-256" : "rsa-sha2-512");
		if (cache == NULL || ret != 0 || key == NULL || load(cache, NULL, NULL) != 0) {
			valid = false;
			continue;
		}
		uint8_t candidate[1024];
		const uint8_t *candidate_blob = NULL;
		size_t candidate_len = 0;
		if (key->pubkey(&candidate_blob, &candidate_len, key->ctx) != 0 || candidate_blob == NULL
		    || candidate_len == 0 || candidate_len > sizeof(candidate)) {
			valid = false;
			continue;
		}
		memcpy(candidate, candidate_blob, candidate_len);
		if (generate(2048) != 0) {
			valid = false;
			continue;
		}
		uint8_t before[1024];
		const uint8_t *blob = NULL;
		size_t length = 0;
		if (key->pubkey(&blob, &length, key->ctx) != 0 || blob == NULL || length == 0 || length > sizeof(before)) {
			valid = false;
			continue;
		}
		memcpy(before, blob, length);
		if (candidate_len == length && memcmp(candidate, before, length) == 0) {
			valid = false;
			continue;
		}
		dssh_key_algo_ctx *prior = key->ctx;
		for (int index = 0; index < 4; index++) {
			/* Imported keys now have two native validation calls first. */
			dssh_test_ossl_fail_after(index + 2);
			ret = load(cache, NULL, NULL);
			int calls = dssh_test_ossl_count();
			dssh_test_ossl_reset();
			const uint8_t *after = NULL;
			size_t after_len = 0;
			bool one = ret == DSSH_ERROR_INIT && calls == index + 3 && key->ctx == prior
			    && key->pubkey(&after, &after_len, key->ctx) == 0 && after != NULL
			    && after_len == length && memcmp(after, before, length) == 0 && signable(key, before, length);
			rejected += one ? 1U : 0U;
			valid = one && valid;
		}
	}
	dssh_test_ossl_reset();
	dssh_test_alloc_reset();
	dssh_test_reset_global_config();
	printf("RSA cache publication: algorithms=2 native-points=4 rejected=%u valid=%u\n", rejected, valid ? 1U : 0U);
	ASSERT_TRUE(valid && rejected == 8);
	return TEST_PASS;
}
#endif

static struct dssh_test_entry tests[] = {
	{"number_models", test_number_models},
	{"layout_models", test_layout_models},
#ifdef DSSH_CRYPTO_OPENSSL
	{"native_numbers", test_native_numbers},
	{"public_values", test_public_values},
	{"zero_modulus", test_zero_modulus},
	{"zero_exponent", test_zero_exponent},
	{"publication_errors", test_publication_errors},
#endif
};
DSSH_TEST_NO_CLEANUP
DSSH_TEST_MAIN(tests)
