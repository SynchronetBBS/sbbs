/* Bundled sntrup buffer lifetime and actual provider-error controls. */
#include <stdint.h>
#include <string.h>
#include <limits.h>

#include "dssh_test.h"
#include "dssh_test_ossl.h"
#include "deucessh-crypto.h"
#include "kex/sntrup761.h"

#ifdef DSSH_CRYPTO_OPENSSL
static bool
filled(const uint8_t *buffer, size_t length, uint8_t value)
{
	for (size_t i = 0; i < length; i++) {
		if (buffer[i] != value)
			return false;
	}
	return true;
}
#endif

static int
test_sntrup_roundtrip(void)
{
	uint8_t pk[crypto_kem_sntrup761_PUBLICKEYBYTES + 2];
	uint8_t sk[crypto_kem_sntrup761_SECRETKEYBYTES + 2];
	uint8_t ct[crypto_kem_sntrup761_CIPHERTEXTBYTES + 2];
	uint8_t ss[crypto_kem_sntrup761_BYTES + 2];
	uint8_t recovered[crypto_kem_sntrup761_BYTES + 2];
	memset(pk, 0xa5, sizeof(pk));
	memset(sk, 0xa5, sizeof(sk));
	memset(ct, 0xa5, sizeof(ct));
	memset(ss, 0xa5, sizeof(ss));
	memset(recovered, 0xa5, sizeof(recovered));
	dssh_test_ossl_reset();
	dssh_test_sntrup_clear_begin(761 * sizeof(uint32_t));
	bool valid = crypto_kem_sntrup761_keypair(pk + 1, sk + 1) == 0;
	if (valid)
		valid = crypto_kem_sntrup761_enc(ct + 1, ss + 1, pk + 1) == 0;
	if (valid)
		valid = crypto_kem_sntrup761_dec(recovered + 1, ct + 1, sk + 1) == 0
		    && memcmp(ss + 1, recovered + 1, crypto_kem_sntrup761_BYTES) == 0;
	if (valid) {
		ct[1] ^= 1;
		valid = crypto_kem_sntrup761_dec(recovered + 1, ct + 1, sk + 1) == 0
		    && memcmp(ss + 1, recovered + 1, crypto_kem_sntrup761_BYTES) != 0;
	}
	struct dssh_test_sntrup_clear_stats stats = dssh_test_sntrup_clear_end();
	valid = valid && stats.zero && stats.matches >= 3
	    && pk[0] == 0xa5 && pk[sizeof(pk) - 1] == 0xa5
	    && sk[0] == 0xa5 && sk[sizeof(sk) - 1] == 0xa5
	    && ct[0] == 0xa5 && ct[sizeof(ct) - 1] == 0xa5
	    && ss[0] == 0xa5 && ss[sizeof(ss) - 1] == 0xa5
	    && recovered[0] == 0xa5 && recovered[sizeof(recovered) - 1] == 0xa5;
	dssh_cleanse(pk, sizeof(pk));
	dssh_cleanse(sk, sizeof(sk));
	dssh_cleanse(ct, sizeof(ct));
	dssh_cleanse(ss, sizeof(ss));
	dssh_cleanse(recovered, sizeof(recovered));
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

#ifdef DSSH_CRYPTO_OPENSSL
static int
test_sntrup_rng_retirement(void)
{
	uint8_t pk[crypto_kem_sntrup761_PUBLICKEYBYTES];
	uint8_t sk[crypto_kem_sntrup761_SECRETKEYBYTES];
	uint8_t ct[crypto_kem_sntrup761_CIPHERTEXTBYTES];
	uint8_t ss[crypto_kem_sntrup761_BYTES];
	bool valid = true;
	for (unsigned int mode = 0; mode < 2; mode++) {
		dssh_test_ossl_reset();
		bool prepared = true;
		if (mode == 1)
			prepared = crypto_kem_sntrup761_keypair(pk, sk) == 0;
		else {
			memset(pk, 0xa5, sizeof(pk));
			memset(sk, 0xa5, sizeof(sk));
		}
		memset(ct, 0xa5, sizeof(ct));
		memset(ss, 0xa5, sizeof(ss));
		dssh_test_sntrup_clear_begin(761 * sizeof(uint32_t));
		dssh_test_ossl_random_post_failure(true);
		int ret = 0;
		if (prepared)
			ret = mode == 0 ? crypto_kem_sntrup761_keypair(pk, sk)
			    : crypto_kem_sntrup761_enc(ct, ss, pk);
		size_t length = 0;
		unsigned int strength = 0;
		bool completed = false;
		bool observed = dssh_test_ossl_random_observed(&length, &strength, &completed);
		struct dssh_test_sntrup_clear_stats stats = dssh_test_sntrup_clear_end();
		dssh_test_ossl_reset();
		valid = valid && prepared && ret == -1 && observed && completed
		    && length == 761 * sizeof(uint32_t) && strength == 256
		    && stats.matches == 1 && stats.zero;
		if (mode == 0)
			valid = valid && filled(pk, sizeof(pk), 0xa5) && filled(sk, sizeof(sk), 0xa5);
		valid = valid && filled(ct, sizeof(ct), 0xa5) && filled(ss, sizeof(ss), 0xa5);
		dssh_cleanse(pk, sizeof(pk));
		dssh_cleanse(sk, sizeof(sk));
		dssh_cleanse(ct, sizeof(ct));
		dssh_cleanse(ss, sizeof(ss));
	}
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

static int
test_sntrup_provider_errors(void)
{
	uint8_t pk[crypto_kem_sntrup761_PUBLICKEYBYTES];
	uint8_t sk[crypto_kem_sntrup761_SECRETKEYBYTES];
	uint8_t ct[crypto_kem_sntrup761_CIPHERTEXTBYTES];
	uint8_t ss[crypto_kem_sntrup761_BYTES];
	uint8_t candidate[crypto_kem_sntrup761_CIPHERTEXTBYTES + 2];
	uint8_t out[crypto_kem_sntrup761_BYTES + 2];
	dssh_test_ossl_reset();
	bool valid = crypto_kem_sntrup761_keypair(pk, sk) == 0;
	int calls[2] = {0};
	for (unsigned int mode = 0; valid && mode < 2; mode++) {
		dssh_test_ossl_fail_after(INT_MAX);
		int ret = mode == 0 ? crypto_kem_sntrup761_enc(ct, ss, pk)
		    : crypto_kem_sntrup761_dec(ss, ct, sk);
		calls[mode] = dssh_test_ossl_count();
		dssh_test_ossl_reset();
		valid = ret == 0 && calls[mode] > 0;
	}
	for (unsigned int mode = 0; mode < 2; mode++) {
		for (int stage = 0; stage < calls[mode]; stage++) {
			memset(candidate, 0xa5, sizeof(candidate));
			memset(out, 0xa5, sizeof(out));
			dssh_test_sntrup_clear_begin(64);
			dssh_test_ossl_fail_after(stage);
			int ret = mode == 0 ? crypto_kem_sntrup761_enc(candidate + 1, out + 1, pk)
			    : crypto_kem_sntrup761_dec(out + 1, ct, sk);
			int actual_calls = dssh_test_ossl_count();
			struct dssh_test_sntrup_clear_stats stats = dssh_test_sntrup_clear_end();
			dssh_test_ossl_reset();
			valid = valid && ret == -1 && actual_calls == stage + 1
			    && stats.matches > 0 && stats.zero
			    && candidate[0] == 0xa5 && candidate[sizeof(candidate) - 1] == 0xa5
			    && out[0] == 0xa5 && out[sizeof(out) - 1] == 0xa5;
		}
	}
	dssh_cleanse(pk, sizeof(pk));
	dssh_cleanse(sk, sizeof(sk));
	dssh_cleanse(ct, sizeof(ct));
	dssh_cleanse(ss, sizeof(ss));
	dssh_cleanse(candidate, sizeof(candidate));
	dssh_cleanse(out, sizeof(out));
	ASSERT_TRUE(valid);
	return TEST_PASS;
}
#endif

static struct dssh_test_entry tests[] = {
	{"sntrup/roundtrip", test_sntrup_roundtrip},
#ifdef DSSH_CRYPTO_OPENSSL
	{"sntrup/rng_retirement", test_sntrup_rng_retirement},
	{"sntrup/provider_errors", test_sntrup_provider_errors},
#endif
};

DSSH_TEST_NO_CLEANUP
DSSH_TEST_MAIN(tests)
