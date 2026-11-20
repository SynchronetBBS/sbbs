/* Native generation ownership and failure-preserving module transactions. */
#include "dssh_test.h"
#include "dssh_test_internal.h"
#include "dssh_test_ossl.h"
#include "deucessh-algorithms.h"
#ifdef DSSH_CRYPTO_OPENSSL
#include <openssl/evp.h>
#include <openssl/params.h>
#include <stdatomic.h>
#endif

struct generation_ops {
	int (*reg)(void);
	int (*load)(const char *, dssh_pem_password_cb, void *);
	int (*generate)(unsigned int);
	const char *name;
	const char *cache;
	int native_points;
};

static int
ed25519_generate(unsigned int unused)
{
	(void)unused;
	return dssh_ed25519_generate_key();
}

static const struct generation_ops operations[] = {
	{dssh_register_ssh_ed25519, dssh_ed25519_load_key_file, ed25519_generate,
	    "ssh-ed25519", "DSSH_TEST_ED25519_KEY", 3},
	{dssh_register_rsa_sha2_256, dssh_rsa_sha2_256_load_key_file, dssh_rsa_sha2_256_generate_key,
	    "rsa-sha2-256", "DSSH_TEST_RSA_KEY", 4},
	{dssh_register_rsa_sha2_512, dssh_rsa_sha2_512_load_key_file, dssh_rsa_sha2_512_generate_key,
	    "rsa-sha2-512", "DSSH_TEST_RSA_KEY", 4},
};

static bool
snapshot(dssh_key_algo key, uint8_t *copy, size_t *length)
{
	const uint8_t *blob = NULL;
	size_t count = 0;
	if (key->pubkey(&blob, &count, key->ctx) != 0 || blob == NULL || count == 0 || count > 512)
		return false;
	memcpy(copy, blob, count);
	*length = count;
	return true;
}

static bool
signable(dssh_key_algo key, const uint8_t *blob, size_t length)
{
	const uint8_t message[] = {'g', 'e', 'n'};
	struct dssh_bytes_result signature = {0};
	int ret = key->sign(&signature, message, sizeof(message), key->ctx);
	bool valid = ret == 0 && signature.bytes.data != NULL && signature.bytes.len != 0
	    && signature.lease.release != NULL;
	if (valid)
		valid = key->verify(blob, length, signature.bytes.data, signature.bytes.len,
		    message, sizeof(message)) == 0;
	dssh_result_release(&signature.lease);
	return valid;
}

static dssh_key_algo
start(const struct generation_ops *ops)
{
	dssh_test_ossl_reset();
	dssh_test_reset_global_config();
	const char *cache = getenv(ops->cache);
	if (cache == NULL || ops->reg() != 0 || ops->load(cache, NULL, NULL) != 0)
		return NULL;
	return find_key_algo(ops->name);
}

static int
test_invalid_sizes(void)
{
	bool valid = true;
	unsigned int rejected = 0;
	const unsigned int sizes[] = {0, 1};
	for (size_t i = 1; i < sizeof(operations) / sizeof(operations[0]); i++) {
		const struct generation_ops *ops = &operations[i];
		dssh_key_algo key = start(ops);
		uint8_t before[512], after[512];
		size_t before_len = 0, after_len = 0;
		if (key == NULL || !snapshot(key, before, &before_len)) {
			valid = false;
			continue;
		}
		dssh_key_algo_ctx *prior = key->ctx;
		for (size_t j = 0; j < sizeof(sizes) / sizeof(sizes[0]); j++) {
			int ret = ops->generate(sizes[j]);
			bool one = ret < 0 && key->ctx == prior && snapshot(key, after, &after_len)
			    && after_len == before_len && memcmp(before, after, before_len) == 0
			    && signable(key, before, before_len);
			rejected += one ? 1U : 0U;
			valid = one && valid;
		}
	}
	dssh_test_reset_global_config();
	printf("key generation sizes: families=2 sizes=2 rejected=%u valid=%u\n", rejected, valid ? 1U : 0U);
	ASSERT_TRUE(valid && rejected == 4);
	return TEST_PASS;
}

#ifdef DSSH_CRYPTO_OPENSSL
static int
test_native_errors(void)
{
	bool valid = true;
	unsigned int rejected = 0, recovered = 0;
	for (size_t i = 0; i < sizeof(operations) / sizeof(operations[0]); i++) {
		const struct generation_ops *ops = &operations[i];
		dssh_key_algo key = start(ops);
		uint8_t before[512], after[512];
		size_t before_len = 0, after_len = 0;
		if (key == NULL || !snapshot(key, before, &before_len)) {
			valid = false;
			continue;
		}
		dssh_key_algo_ctx *prior = key->ctx;
		for (int point = 0; point < ops->native_points; point++) {
			dssh_test_ossl_fail_after(point);
			int ret = ops->generate(2048);
			int calls = dssh_test_ossl_count();
			dssh_test_ossl_reset();
			int expected = point == 0 ? DSSH_ERROR_ALLOC : DSSH_ERROR_INIT;
			bool one = ret == expected && calls == point + 1 && key->ctx == prior
			    && snapshot(key, after, &after_len) && after_len == before_len
			    && memcmp(before, after, before_len) == 0 && signable(key, before, before_len);
			rejected += one ? 1U : 0U;
			valid = one && valid;
		}
		bool one = ops->generate(2048) == 0 && snapshot(key, after, &after_len)
		    && (after_len != before_len || memcmp(before, after, before_len) != 0)
		    && signable(key, after, after_len);
		recovered += one ? 1U : 0U;
		valid = one && valid;
	}
	dssh_test_ossl_reset();
	dssh_test_reset_global_config();
	printf("key generation native errors: families=3 rejected=%u recovered=%u valid=%u\n",
	    rejected, recovered, valid ? 1U : 0U);
	ASSERT_TRUE(valid && rejected == 11 && recovered == 3);
	return TEST_PASS;
}

static int
cancel_generation(EVP_PKEY_CTX *ctx)
{
	atomic_bool *seen = EVP_PKEY_CTX_get_app_data(ctx);
	if (seen != NULL)
		atomic_store(seen, true);
	return 0;
}

static int
test_native_cancel(void)
{
	/* These fixture calls reach the real native core/provider, not countdown
	 * wrappers. The module independently checks its direct failure calls. */
	EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new_from_name(NULL, "RSA", NULL);
	EVP_PKEY_CTX *validation = NULL;
	EVP_PKEY *key = NULL;
	unsigned int bits = 2048, reclaimed = 0;
	OSSL_PARAM params[] = {
		OSSL_PARAM_construct_uint("bits", &bits), OSSL_PARAM_construct_end(),
	};
	atomic_bool seen;
	atomic_init(&seen, false);
	bool valid = false, recovered = false;
	if (ctx == NULL || EVP_PKEY_keygen_init(ctx) != 1 || EVP_PKEY_CTX_set_params(ctx, params) != 1)
		goto done;
	EVP_PKEY_CTX_set_app_data(ctx, &seen);
	EVP_PKEY_CTX_set_cb(ctx, cancel_generation);
	valid = true;
	for (unsigned int i = 0; i < 3; i++) {
		atomic_store(&seen, false);
		int ret = EVP_PKEY_keygen(ctx, &key);
		bool one = ret <= 0 && key == NULL && atomic_load(&seen);
		reclaimed += one ? 1U : 0U;
		valid = one && valid;
		EVP_PKEY_free(key);
		key = NULL;
	}
	EVP_PKEY_CTX_set_cb(ctx, NULL);
	if (EVP_PKEY_keygen(ctx, &key) != 1 || key == NULL || !EVP_PKEY_is_a(key, "RSA")
	    || EVP_PKEY_get_bits(key) != 2048)
		goto done;
	validation = EVP_PKEY_CTX_new(key, NULL);
	recovered = validation != NULL && EVP_PKEY_pairwise_check(validation) == 1;
done:
	EVP_PKEY_CTX_free(validation);
	EVP_PKEY_free(key);
	EVP_PKEY_CTX_free(ctx);
	printf("key generation native cancellation: attempts=3 reclaimed=%u recovered=%u valid=%u\n",
	    reclaimed, recovered ? 1U : 0U, valid ? 1U : 0U);
	ASSERT_TRUE(valid && reclaimed == 3 && recovered);
	return TEST_PASS;
}
#endif

static struct dssh_test_entry tests[] = {
	{"invalid_sizes", test_invalid_sizes},
#ifdef DSSH_CRYPTO_OPENSSL
	{"native_errors", test_native_errors},
	{"native_cancel", test_native_cancel},
#endif
};
DSSH_TEST_NO_CLEANUP
DSSH_TEST_MAIN(tests)
