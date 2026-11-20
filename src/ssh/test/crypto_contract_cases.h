#ifndef DSSH_CRYPTO_CONTRACT_CASES_H
#define DSSH_CRYPTO_CONTRACT_CASES_H

#include "dssh_test_alloc.h"
#include "dssh_test_ossl.h"

static const struct {
	const char *name;
	size_t length;
	const char *binary;
	const char *million;
} crypto_answers[] = {
	{ "SHA-256", 32,
	  "40aff2e9d2d8922e47afd4648e6967497158785fbd1da870e7110266bf944880",
	  "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0" },
	{ "SHA-512", 64,
	  "1e7b80bc8edc552c8feeb2780e111477e5bc70465fac1a77b29b35980c3f0ce4a036a6c9462036824bd56801e62af7e9feba5c22ed8a5af877bf7de117dcac6d",
	  "e718483d0ce769644e2e42c7bc15b4638e1f98b13b2044285632a803afa973ebde0ff244877ea60a4cb0432ce577c31beb009c5c2c49aa2e4eadb217ad8cc09b" },
};

static bool
crypto_filled(const uint8_t *buf, size_t length, uint8_t byte)
{
	for (size_t i = 0; i < length; i++) {
		if (buf[i] != byte)
			return false;
	}
	return true;
}

static bool
crypto_answer(const uint8_t *out, const char *text, size_t length)
{
	static const char digits[] = "0123456789abcdef";
	if (strlen(text) != length * 2)
		return false;
	for (size_t i = 0; i < length; i++) {
		const char *high = strchr(digits, text[2 * i]);
		const char *low = strchr(digits, text[2 * i + 1]);
		if (high == NULL || low == NULL)
			return false;
		size_t h = (size_t)(high - digits);
		size_t l = (size_t)(low - digits);
		uint8_t value = (uint8_t)(h * 16 + l);
		if (out[i] != value)
			return false;
	}
	return true;
}

static int
test_contract_hash_binary(void)
{
	uint8_t input[256];
	for (size_t i = 0; i < sizeof(input); i++) {
		uint8_t byte = (uint8_t)i;
		input[i] = byte;
	}
	const size_t cuts[] = { 1, 7, 64, 255 };
	bool valid = true;
	for (size_t a = 0; a < sizeof(crypto_answers) / sizeof(crypto_answers[0]); a++) {
		dssh_hash_ctx *ctx = NULL;
		size_t length = 0;
		bool matched = dssh_hash_init(&ctx, crypto_answers[a].name, &length) == 0
		    && ctx != NULL && length == crypto_answers[a].length;
		for (size_t n = 0; matched && n < sizeof(cuts) / sizeof(cuts[0]); n++) {
			uint8_t frame[96];
			memset(frame, 0xa5, sizeof(frame));
			matched = dssh_hash_update(ctx, input, cuts[n]) == 0
			    && dssh_hash_update(ctx, input + cuts[n], sizeof(input) - cuts[n]) == 0
			    && dssh_hash_final(ctx, frame + 8, length) == 0
			    && crypto_answer(frame + 8, crypto_answers[a].binary, length)
			    && crypto_filled(frame, 8, 0xa5)
			    && crypto_filled(frame + 8 + length, sizeof(frame) - 8 - length, 0xa5);
		}
		dssh_hash_free(ctx);
		uint8_t frame[96];
		memset(frame, 0xa5, sizeof(frame));
		matched = dssh_hash_oneshot(crypto_answers[a].name, input, sizeof(input), frame + 8,
		    crypto_answers[a].length) == 0 && matched;
		matched = matched && crypto_answer(frame + 8, crypto_answers[a].binary, crypto_answers[a].length)
		    && crypto_filled(frame, 8, 0xa5)
		    && crypto_filled(frame + 8 + crypto_answers[a].length,
		        sizeof(frame) - 8 - crypto_answers[a].length, 0xa5);
		valid = valid && matched;
	}
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

static int
test_contract_hash_million(void)
{
	uint8_t input[1024];
	memset(input, 'a', sizeof(input));
	bool valid = true;
	for (size_t a = 0; a < sizeof(crypto_answers) / sizeof(crypto_answers[0]); a++) {
		dssh_hash_ctx *ctx = NULL;
		size_t length = 0;
		bool matched = dssh_hash_init(&ctx, crypto_answers[a].name, &length) == 0
		    && ctx != NULL && length == crypto_answers[a].length;
		for (unsigned n = 0; n < 976 && matched; n++)
			matched = dssh_hash_update(ctx, input, sizeof(input)) == 0;
		uint8_t frame[96];
		memset(frame, 0xa5, sizeof(frame));
		matched = matched && dssh_hash_update(ctx, input, 576) == 0
		    && dssh_hash_final(ctx, frame + 8, crypto_answers[a].length) == 0
		    && crypto_answer(frame + 8, crypto_answers[a].million, crypto_answers[a].length)
		    && crypto_filled(frame, 8, 0xa5)
		    && crypto_filled(frame + 8 + crypto_answers[a].length,
		        sizeof(frame) - 8 - crypto_answers[a].length, 0xa5);
		dssh_hash_free(ctx);
		valid = valid && matched;
	}
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

static int
test_contract_hash_rejections(void)
{
	bool valid = true;
	const uint8_t abc[] = { 'a', 'b', 'c' };
	for (size_t a = 0; a < sizeof(crypto_answers) / sizeof(crypto_answers[0]); a++) {
		dssh_hash_ctx *ctx = NULL;
		size_t length = 0;
		bool matched = dssh_hash_init(&ctx, crypto_answers[a].name, &length) == 0
		    && ctx != NULL && length == crypto_answers[a].length;
		for (unsigned repeat = 0; repeat < 4 && matched; repeat++) {
			uint8_t out[96];
			memset(out, 0xa5, sizeof(out));
#ifdef DSSH_TEST_LAYOUT
			/* Only the instrumented library redirects its allocations. */
			dssh_test_alloc_fail_after(0);
#endif
			matched = dssh_hash_update(ctx, abc, sizeof(abc)) == 0
			    && dssh_hash_update(ctx, NULL, 1) == DSSH_ERROR_INVALID
			    && dssh_hash_final(ctx, out + 8, length - 1) == DSSH_ERROR_INVALID
			    && crypto_filled(out, sizeof(out), 0xa5)
			    && dssh_hash_final(ctx, out + 8, length) == 0;
#ifdef DSSH_TEST_LAYOUT
			matched = matched && dssh_test_alloc_count() == 0;
			dssh_test_alloc_reset();
#endif
			uint8_t expected[64];
			matched = matched && dssh_hash_oneshot(crypto_answers[a].name, abc, sizeof(abc),
			    expected, sizeof(expected)) == 0 && memcmp(out + 8, expected, length) == 0
			    && crypto_filled(out, 8, 0xa5)
			    && crypto_filled(out + 8 + length, sizeof(out) - 8 - length, 0xa5);
		}
#ifdef DSSH_TEST_LAYOUT
		dssh_test_alloc_reset();
#endif
		dssh_hash_free(ctx);
		valid = valid && matched;
	}
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

static int
test_contract_base64(void)
{
	const char *inputs[] = { "", "f", "fo", "foo", "foob", "fooba", "foobar" };
	const char *outputs[] = { "", "Zg==", "Zm8=", "Zm9v", "Zm9vYg==", "Zm9vYmE=", "Zm9vYmFy" };
	for (size_t i = 0; i < sizeof(inputs) / sizeof(inputs[0]); i++) {
		const uint8_t *input = (const uint8_t *)inputs[i];
		size_t expected = strlen(outputs[i]);
		uint8_t frame[32];
		memset(frame, 0xa5, sizeof(frame));
		char *output = (char *)&frame[4];
		int ret = dssh_base64_encode(input, strlen(inputs[i]), output, expected);
		ASSERT_EQ(ret, DSSH_ERROR_TOOLONG);
		ASSERT_TRUE(crypto_filled(frame, sizeof(frame), 0xa5));
		ret = dssh_base64_encode(input, strlen(inputs[i]), output, expected + 1);
		ASSERT_EQ_U(ret, expected);
		ASSERT_TRUE(memcmp(output, outputs[i], expected + 1) == 0
		    && crypto_filled(frame, 4, 0xa5)
		    && crypto_filled(frame + 4 + expected + 1, sizeof(frame) - 5 - expected, 0xa5));
	}
	return TEST_PASS;
}

static int
test_contract_zero_lengths(void)
{
	dssh_hash_ctx *ctx = NULL;
	size_t length = 0;
	bool valid = dssh_hash_init(&ctx, "SHA-256", &length) == 0
	    && ctx != NULL && length == sizeof(sha256_empty);
#ifdef DSSH_CRYPTO_OPENSSL
	dssh_test_ossl_fail_after(0);
#endif
	valid = valid && dssh_hash_update(ctx, NULL, 0) == 0;
#ifdef DSSH_CRYPTO_OPENSSL
	valid = valid && dssh_test_ossl_count() == 0;
	dssh_test_ossl_reset();
#endif
	uint8_t out[32];
	valid = valid && dssh_hash_final(ctx, out, sizeof(out)) == 0
	    && memcmp(out, sha256_empty, sizeof(out)) == 0;
	dssh_hash_free(ctx);
	uint8_t value = 7;
	valid = valid && dssh_random(&value, 0) == 0 && value == 7
	    && dssh_crypto_memcmp(NULL, NULL, 0) == 0 && dssh_crypto_memcmp(NULL, &value, 0) == 0;
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

#ifdef DSSH_CRYPTO_OPENSSL
static int
test_contract_encoded_lengths(void)
{
	const int reports[] = { -1, 0, 3, 4, 8 };
	const uint8_t input[] = { 'f', 'o', 'o' };
	for (size_t i = 0; i < sizeof(reports) / sizeof(reports[0]); i++) {
		uint8_t frame[16];
		memset(frame, 0xa5, sizeof(frame));
		char *out = (char *)(frame + 4);
		dssh_test_ossl_encoded_length(reports[i]);
		int ret = dssh_base64_encode(input, sizeof(input), out, 5);
		dssh_test_ossl_reset();
		ASSERT_EQ(ret, reports[i] == 4 ? 4 : DSSH_ERROR_INIT);
		ASSERT_TRUE(crypto_filled(frame, 4, 0xa5) && crypto_filled(frame + 9, 7, 0xa5));
	}
	return TEST_PASS;
}

static bool
crypto_dead(dssh_hash_ctx *ctx)
{
	const uint8_t value = 7;
	uint8_t out[64];
	dssh_test_ossl_fail_after(0);
	int update = dssh_hash_update(ctx, &value, 1);
	int final = dssh_hash_final(ctx, out, sizeof(out));
	bool valid = update == DSSH_ERROR_INVALID && final == DSSH_ERROR_INVALID
	    && dssh_test_ossl_count() == 0;
	dssh_test_ossl_reset();
	return valid;
}

static int
test_contract_hash_failures(void)
{
	bool valid = true;
	const uint8_t value = 7;
	for (unsigned stage = 0; stage < 4; stage++) {
		dssh_hash_ctx *ctx = NULL;
		size_t length = 0;
		bool matched = dssh_hash_init(&ctx, "SHA-256", &length) == 0
		    && ctx != NULL && length == sizeof(sha256_empty);
		uint8_t out[64];
		unsigned failure = stage > 1 ? stage - 1 : 0;
		int failure_int = (int)failure; /* Proven at most two. */
		dssh_test_ossl_fail_after(failure_int);
		int ret = stage == 0 ? dssh_hash_update(ctx, &value, 1) : dssh_hash_final(ctx, out, sizeof(out));
		matched = matched && ret == DSSH_ERROR_INIT && dssh_test_ossl_count() == failure_int + 1;
		dssh_test_ossl_reset();
		matched = crypto_dead(ctx) && matched;
		dssh_hash_free(ctx);
		valid = valid && matched;
	}
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

static int
test_contract_hash_reported_lengths(void)
{
	bool valid = true;
	const unsigned int reports[] = { 0, 31, 33, UINT_MAX };
	const uint8_t input = 7;
	for (size_t i = 0; i < sizeof(reports) / sizeof(reports[0]); i++) {
		dssh_hash_ctx *ctx = NULL;
		size_t length = 0;
		bool matched = dssh_hash_init(&ctx, "SHA-256", &length) == 0
		    && ctx != NULL && length == sizeof(sha256_empty);
		uint8_t frame[64];
		memset(frame, 0xa5, sizeof(frame));
		dssh_test_ossl_digest_length(reports[i]);
		matched = matched && dssh_hash_final(ctx, frame + 8, 32) == DSSH_ERROR_INIT
		    && crypto_filled(frame, 8, 0xa5) && crypto_filled(frame + 40, 24, 0xa5);
		dssh_test_ossl_reset();
		matched = crypto_dead(ctx) && matched;
		dssh_hash_free(ctx);
		dssh_test_ossl_digest_length(reports[i]);
		matched = dssh_hash_oneshot("SHA-256", &input, 1, frame + 8, 32) == DSSH_ERROR_INIT && matched;
		dssh_test_ossl_reset();
		valid = valid && matched;
	}
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

static int
test_contract_hash_publication(void)
{
	const int expected[] = { DSSH_ERROR_INIT, DSSH_ERROR_ALLOC, DSSH_ERROR_INIT,
	    DSSH_ERROR_ALLOC, DSSH_ERROR_INIT, DSSH_ERROR_INIT };
	bool valid = true;
	for (unsigned mode = 0; mode < 6; mode++) {
		dssh_hash_ctx *original = NULL;
		size_t length = 0;
		bool matched = dssh_hash_init(&original, "SHA-256", &length) == 0
		    && original != NULL && length == sizeof(sha256_empty);
		dssh_hash_ctx *candidate = original;
		size_t sentinel = 17;
		if (mode < 3) {
			int call = (int)mode;
			dssh_test_ossl_fail_after(call);
		}
		else if (mode == 3)
			dssh_test_alloc_fail_after(0);
		else
			dssh_test_ossl_md_size(mode == 4 ? 0 : -1);
		int ret = dssh_hash_init(&candidate, "SHA-256", &sentinel);
		matched = matched && ret == expected[mode] && candidate == original && sentinel == 17;
		if (mode < 3) {
			unsigned wanted = mode + 1;
			matched = matched && dssh_test_ossl_count() == wanted;
		}
		if (mode == 3)
			matched = matched && dssh_test_alloc_count() == 1;
		dssh_test_ossl_reset();
		dssh_test_alloc_reset();
		if (candidate != original)
			dssh_hash_free(candidate);
		uint8_t out[32];
		matched = matched && dssh_hash_final(original, out, sizeof(out)) == 0
		    && memcmp(out, sha256_empty, sizeof(out)) == 0;
		dssh_hash_free(original);
		valid = valid && matched;
	}
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

static int
test_contract_oneshot_rng_errors(void)
{
	uint8_t out[64];
	const uint8_t input = 7;
	for (int stage = 0; stage < 2; stage++) {
		memset(out, 0xa5, sizeof(out));
		dssh_test_ossl_fail_after(stage);
		int ret = dssh_hash_oneshot("SHA-256", &input, 1, out, sizeof(out));
		bool valid = ret == DSSH_ERROR_INIT && crypto_filled(out, sizeof(out), 0xa5)
		    && dssh_test_ossl_count() == stage + 1;
		dssh_test_ossl_reset();
		ASSERT_TRUE(valid);
	}
	dssh_test_ossl_fail_after(0);
	int ret = dssh_random(out, sizeof(out));
	bool valid = ret == DSSH_ERROR_INIT && dssh_test_ossl_count() == 1
	    && crypto_filled(out, sizeof(out), 0xa5);
	dssh_test_ossl_reset();
#if SIZE_MAX > INT_MAX
	size_t too_long = INT_MAX;
	too_long++;
	dssh_test_ossl_fail_after(0);
	valid = valid && dssh_random(out, too_long) == DSSH_ERROR_INVALID && dssh_test_ossl_count() == 0;
	dssh_test_ossl_reset();
#endif
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

static int
test_contract_rng_request(void)
{
	uint8_t frame[66];
	memset(frame, 0xa5, sizeof(frame));
	dssh_test_ossl_reset();
	bool valid = dssh_random(NULL, 32) == DSSH_ERROR_INVALID
	    && dssh_random(frame + 1, 0) == 0;
#if SIZE_MAX > INT_MAX
	size_t too_long = INT_MAX;
	too_long++;
	valid = valid && dssh_random(frame + 1, too_long) == DSSH_ERROR_INVALID;
#endif
	valid = valid && !dssh_test_ossl_random_observed(NULL, NULL, NULL)
	    && dssh_test_ossl_count() == 0 && crypto_filled(frame, sizeof(frame), 0xa5);
	for (unsigned int mode = 0; mode < 3; mode++) {
		dssh_test_ossl_reset();
		dssh_test_ossl_fail_after(INT_MAX);
		memset(frame, 0xa5, sizeof(frame));
		if (mode == 1)
			dssh_test_ossl_fail_after(0);
		if (mode == 2)
			dssh_test_ossl_random_post_failure(true);
		int ret = dssh_random(frame + 1, 64);
		size_t length = 0;
		unsigned int strength = 0;
		bool completed = false;
		bool observed = dssh_test_ossl_random_observed(&length, &strength, &completed);
		int expected = mode == 0 ? 0 : DSSH_ERROR_INIT;
		valid = valid && ret == expected && observed && length == 64
		    && strength == 256 && completed == (mode != 1)
		    && dssh_test_ossl_count() == 1 && frame[0] == 0xa5 && frame[65] == 0xa5;
		if (mode == 1)
			valid = valid && crypto_filled(frame + 1, 64, 0xa5);
	}
	dssh_test_ossl_reset();
	valid = valid && !dssh_test_ossl_random_observed(NULL, NULL, NULL);
	ASSERT_TRUE(valid);
	return TEST_PASS;
}
#endif

#endif
