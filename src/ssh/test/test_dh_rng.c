/* Fresh-process native DH adapter policy and retained-owner recovery.
 * This fixture uses private operations and is not a public API example. */
#include <stdio.h>
#include <string.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/rand.h>

#include "deucessh.h"
#include "deucessh-crypto.h"
#include "kex/dh-gex-groups.h"
#include "kex/dh-gex-sha256-ops.h"
#ifdef DSSH_DH_RNG_INJECT
#include "dssh_test_ossl.h"
#endif

#define CHECK(condition) do { \
	if (!(condition)) { \
		if (fprintf(stderr, "DH RNG check failed at line %d: %s\n", \
		    __LINE__, #condition) < 0) \
			status = 2; \
		goto done; \
	} \
} while (0)

static bool
unchanged(const uint8_t *data, size_t length)
{
	for (size_t i = 0; i < length; i++) {
		if (data[i] != 0xa5)
			return false;
	}
	return true;
}

static bool
valid_public(const uint8_t *data, size_t length, const struct rfc3526_group *group)
{
	if (length == 0 || length > group->p_len || data[0] == 0
	    || (length == 1 && data[0] <= 1))
		return false;
	if (length < group->p_len)
		return true;
	/* All RFC 3526 groups end in ff; compare directly against p-1. */
	if (group->p[group->p_len - 1] != 0xff)
		return false;
	int prefix = memcmp(data, group->p, length - 1);
	return prefix < 0 || (prefix == 0 && data[length - 1] < 0xfe);
}

int
main(int argc, char **argv)
{
	if (argc != 2)
		return 2;
	const char *cipher;
	unsigned int strength;
	unsigned mode = 0;
	if (strcmp(argv[1], "aes128") == 0) {
		cipher = "AES-128-CTR";
		strength = 128;
	}
	else if (strcmp(argv[1], "aes192") == 0) {
		cipher = "AES-192-CTR";
		strength = 192;
	}
	else if (strcmp(argv[1], "invalid") == 0) {
		cipher = "DEUCESSH-INVALID-CIPHER";
		strength = 0;
	}
	else if (strcmp(argv[1], "aes256") == 0) {
		cipher = "AES-256-CTR";
		strength = 256;
	}
#ifdef DSSH_DH_RNG_INJECT
	else if (strcmp(argv[1], "pre") == 0 || strcmp(argv[1], "post") == 0) {
		cipher = "AES-256-CTR";
		strength = 256;
		mode = strcmp(argv[1], "pre") == 0 ? 1U : 2U;
	}
#endif
	else {
		return 2;
	}
	int status = 1;
	void *state = NULL;
	uint8_t public_key[DHGEX_RAW_MAX + 2], secret[DHGEX_RAW_MAX + 2];
	static const uint8_t generator = 2;
	unsigned successes = 0, refusals = 0;
#ifdef DSSH_DH_RNG_INJECT
	bool watching = false;
	dssh_test_ossl_reset();
#endif
	CHECK(RAND_set_DRBG_type(NULL, "CTR-DRBG", NULL, cipher, NULL) == 1);
	uint8_t native[32];
	int result = RAND_priv_bytes_ex(NULL, native, sizeof(native), 0);
	dssh_cleanse(native, sizeof(native));
	EVP_RAND_CTX *context = RAND_get0_private(NULL);
	CHECK(strength == 0 ? result != 1 && context == NULL
	    : result == 1 && context != NULL && EVP_RAND_get_strength(context) == strength);
	ERR_clear_error();
	CHECK(dssh_dhgex_backend.init(&state) == 0 && state != NULL);
	for (size_t group_index = 0; group_index < RFC3526_NGROUPS; group_index++) {
		const struct rfc3526_group *group = &rfc3526_groups[group_index];
		for (unsigned phase = 0; phase < 2; phase++) {
			unsigned selected = phase == 0 ? mode : 0;
			bool success = strength == 256 && selected == 0;
			size_t public_length = SIZE_MAX, secret_length = SIZE_MAX;
			memset(public_key, 0xa5, sizeof(public_key));
			memset(secret, 0xa5, sizeof(secret));
#ifdef DSSH_DH_RNG_INJECT
			dssh_test_ossl_bn_random_begin(selected);
			watching = true;
#endif
			result = dssh_dhgex_backend.keygen(state, group->p, group->p_len,
			    &generator, 1, public_key + 1, group->p_len, &public_length);
			CHECK(result == (success ? 0 : DSSH_ERROR_INIT));
			if (success) {
				CHECK(valid_public(public_key + 1, public_length, group));
				CHECK(public_key[0] == 0xa5 && public_key[group->p_len + 1] == 0xa5);
				CHECK(unchanged(public_key + 1 + public_length,
				    sizeof(public_key) - 1 - public_length));
				/* With peer public value 2, K must equal our own 2^x. */
				CHECK(dssh_dhgex_backend.derive(state, &generator, 1,
				    secret + 1, group->p_len, &secret_length) == 0);
				CHECK(secret_length == public_length
				    && memcmp(secret + 1, public_key + 1, public_length) == 0);
				CHECK(secret[0] == 0xa5 && unchanged(secret + 1 + secret_length,
				    sizeof(secret) - 1 - secret_length));
				successes++;
			}
			else {
				CHECK(public_length == SIZE_MAX && unchanged(public_key, sizeof(public_key)));
				refusals++;
			}
#ifdef DSSH_DH_RNG_INJECT
			struct dssh_test_bn_random_stats observed = dssh_test_ossl_bn_random_end(true);
			watching = false;
			if (strength == 256) {
				CHECK(observed.calls == 1 && observed.strength == 256
				    && observed.range_bits > 0 && observed.context_present && observed.constant_time
				    && observed.native_called == (selected != 1)
				    && observed.native_completed == (selected != 1)
				    && observed.in_range == observed.native_completed && observed.cleared);
				unsigned int range_bits = (unsigned int)observed.range_bits;
				CHECK(range_bits == group->bits - 1);
			}
			else {
				CHECK(observed.calls == 0 && !observed.native_called && !observed.native_completed);
			}
#endif
			/* Both failure and successful derivation retire readiness. */
			memset(secret, 0xa5, sizeof(secret));
			secret_length = SIZE_MAX;
			CHECK(dssh_dhgex_backend.derive(state, &generator, 1,
			    secret + 1, group->p_len, &secret_length) == DSSH_ERROR_INVALID);
			CHECK(secret_length == SIZE_MAX && unchanged(secret, sizeof(secret)));
			dssh_dhgex_backend.reset(state);
			ERR_clear_error();
		}
	}
	CHECK(strength == 0 ? RAND_get0_private(NULL) == NULL
	    : EVP_RAND_get_strength(context) == strength);
	if (printf("dh_rng_policy mode=%s groups=5 attempts=10 successes=%u refusals=%u valid=1\n",
	    argv[1], successes, refusals) < 0 || fflush(stdout) != 0) {
		status = 2;
		goto done;
	}
	status = 0;
done:
#ifdef DSSH_DH_RNG_INJECT
	if (watching)
		(void)dssh_test_ossl_bn_random_end(true);
	dssh_test_ossl_reset();
#endif
	dssh_dhgex_backend.cleanup(state);
	dssh_cleanse(public_key, sizeof(public_key));
	dssh_cleanse(secret, sizeof(secret));
	return status;
}

#undef CHECK
