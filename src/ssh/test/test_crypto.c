/*
 * test_crypto.c -- Unit tests for deucessh-crypto.h backend-neutral API.
 */

#include "dssh_test.h"
#include "deucessh.h"
#include "deucessh-crypto.h"

#include <string.h>
#include <limits.h>

/* SHA-256("") = e3b0c44298fc1c14... */
static const uint8_t sha256_empty[32] = {
	0xe3, 0xb0, 0xc4, 0x42, 0x98, 0xfc, 0x1c, 0x14,
	0x9a, 0xfb, 0xf4, 0xc8, 0x99, 0x6f, 0xb9, 0x24,
	0x27, 0xae, 0x41, 0xe4, 0x64, 0x9b, 0x93, 0x4c,
	0xa4, 0x95, 0x99, 0x1b, 0x78, 0x52, 0xb8, 0x55,
};

/* SHA-512("") = cf83e1357eefb8bd... */
static const uint8_t sha512_empty[64] = {
	0xcf, 0x83, 0xe1, 0x35, 0x7e, 0xef, 0xb8, 0xbd,
	0xf1, 0x54, 0x28, 0x50, 0xd6, 0x6d, 0x80, 0x07,
	0xd6, 0x20, 0xe4, 0x05, 0x0b, 0x57, 0x15, 0xdc,
	0x83, 0xf4, 0xa9, 0x21, 0xd3, 0x6c, 0xe9, 0xce,
	0x47, 0xd0, 0xd1, 0x3c, 0x5d, 0x85, 0xf2, 0xb0,
	0xff, 0x83, 0x18, 0xd2, 0x87, 0x7e, 0xec, 0x2f,
	0x63, 0xb9, 0x31, 0xbd, 0x47, 0x41, 0x7a, 0x81,
	0xa5, 0x38, 0x32, 0x7a, 0xf9, 0x27, 0xda, 0x3e,
};

/* SHA-256("abc") = ba7816bf8f01cfea... */
static const uint8_t sha256_abc[32] = {
	0xba, 0x78, 0x16, 0xbf, 0x8f, 0x01, 0xcf, 0xea,
	0x41, 0x41, 0x40, 0xde, 0x5d, 0xae, 0x22, 0x23,
	0xb0, 0x03, 0x61, 0xa3, 0x96, 0x17, 0x7a, 0x9c,
	0xb4, 0x10, 0xff, 0x61, 0xf2, 0x00, 0x15, 0xad,
};

/* ---- Hash tests ---- */

static int
test_hash_sha256(void)
{
	dssh_hash_ctx *ctx = NULL;
	size_t digest_len = 0;
	uint8_t out[32];
	bool valid = dssh_hash_init(&ctx, "SHA-256", &digest_len) == 0
	    && ctx != NULL && digest_len == sizeof(out)
	    && dssh_hash_final(ctx, out, sizeof(out)) == 0
	    && memcmp(out, sha256_empty, sizeof(out)) == 0;
	dssh_hash_free(ctx);
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

static int
test_hash_sha256_data(void)
{
	dssh_hash_ctx *ctx = NULL;
	size_t digest_len = 0;
	const uint8_t abc[] = { 'a', 'b', 'c' };
	uint8_t out[32];
	bool valid = dssh_hash_init(&ctx, "SHA-256", &digest_len) == 0
	    && ctx != NULL && digest_len == sizeof(out)
	    && dssh_hash_update(ctx, abc, sizeof(abc)) == 0
	    && dssh_hash_final(ctx, out, sizeof(out)) == 0
	    && memcmp(out, sha256_abc, sizeof(out)) == 0;
	dssh_hash_free(ctx);
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

static int
test_hash_final_short_buf(void)
{
	dssh_hash_ctx *ctx = NULL;
	size_t digest_len = 0;
	uint8_t short_out[] = {0xa5};
	uint8_t out[32];
	bool valid = dssh_hash_init(&ctx, "SHA-256", &digest_len) == 0
	    && ctx != NULL && digest_len == sizeof(out)
	    && dssh_hash_final(ctx, short_out, sizeof(short_out)) == DSSH_ERROR_INVALID
	    && short_out[0] == 0xa5
	    && dssh_hash_final(ctx, out, sizeof(out)) == 0
	    && memcmp(out, sha256_empty, sizeof(out)) == 0;
	dssh_hash_free(ctx);
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

static int
test_hash_null_params(void)
{
	dssh_hash_ctx *ctx = NULL;
	size_t digest_len = 0;
	const uint8_t data = 0;
	uint8_t out[32];
	bool valid = dssh_hash_init(NULL, "SHA-256", &digest_len) == DSSH_ERROR_INVALID
	    && digest_len == 0
	    && dssh_hash_init(&ctx, NULL, &digest_len) == DSSH_ERROR_INVALID
	    && ctx == NULL && digest_len == 0
	    && dssh_hash_init(&ctx, "SHA-256", NULL) == DSSH_ERROR_INVALID
	    && ctx == NULL
	    && dssh_hash_update(NULL, &data, 1) == DSSH_ERROR_INVALID
	    && dssh_hash_final(NULL, out, sizeof(out)) == DSSH_ERROR_INVALID;
	dssh_hash_free(ctx);
	dssh_hash_free(NULL);
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

static int
test_hash_final_null_out(void)
{
	dssh_hash_ctx *ctx = NULL;
	size_t digest_len = 0;
	uint8_t out[32];
	bool valid = dssh_hash_init(&ctx, "SHA-256", &digest_len) == 0
	    && ctx != NULL && digest_len == sizeof(out)
	    && dssh_hash_final(ctx, NULL, digest_len) == DSSH_ERROR_INVALID
	    && dssh_hash_final(ctx, out, sizeof(out)) == 0
	    && memcmp(out, sha256_empty, sizeof(out)) == 0;
	dssh_hash_free(ctx);
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

static int
test_hash_unknown_algo(void)
{
	dssh_hash_ctx *ctx = NULL;
	size_t digest_len = 0;
	bool valid = dssh_hash_init(&ctx, "NONSENSE", &digest_len) == DSSH_ERROR_INIT
	    && ctx == NULL && digest_len == 0;
	dssh_hash_free(ctx);
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

static int
test_hash_reuse(void)
{
	dssh_hash_ctx *ctx = NULL;
	size_t digest_len = 0;
	uint8_t out[32];
	const uint8_t abc[] = { 'a', 'b', 'c' };
	bool valid = dssh_hash_init(&ctx, "SHA-256", &digest_len) == 0
	    && ctx != NULL && digest_len == sizeof(out)
	    && dssh_hash_final(ctx, out, sizeof(out)) == 0
	    && memcmp(out, sha256_empty, sizeof(out)) == 0
	    && dssh_hash_update(ctx, abc, sizeof(abc)) == 0
	    && dssh_hash_final(ctx, out, sizeof(out)) == 0
	    && memcmp(out, sha256_abc, sizeof(out)) == 0;
	dssh_hash_free(ctx);
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

static int
test_hash_sha512(void)
{
	dssh_hash_ctx *ctx = NULL;
	size_t digest_len = 0;
	uint8_t out[64];
	bool valid = dssh_hash_init(&ctx, "SHA-512", &digest_len) == 0
	    && ctx != NULL && digest_len == sizeof(out)
	    && dssh_hash_final(ctx, out, sizeof(out)) == 0
	    && memcmp(out, sha512_empty, sizeof(out)) == 0;
	dssh_hash_free(ctx);
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

/* ---- Random tests ---- */

static int
test_random_basic(void)
{
	uint8_t frame[34];
	memset(frame, 0xa5, sizeof(frame));
	ASSERT_EQ(dssh_random(frame + 1, sizeof(frame) - 2), 0);
	ASSERT_EQ(frame[0], 0xa5);
	ASSERT_EQ(frame[sizeof(frame) - 1], 0xa5);
	/* This is a bounded native-call smoke test, not an entropy test. */
	return TEST_PASS;
}

static int
test_random_null(void)
{
	ASSERT_ERR(dssh_random(NULL, 32), DSSH_ERROR_INVALID);
	return TEST_PASS;
}

static int
test_random_zero_len(void)
{
	const uint8_t expected[] = {0xAA, 0xBB, 0xCC, 0xDD};
	uint8_t buf[sizeof(expected)];
	memcpy(buf, expected, sizeof(buf));
	ASSERT_EQ(dssh_random(buf, 0), 0);
	ASSERT_MEM_EQ(buf, expected, sizeof(buf));
	return TEST_PASS;
}

/* ---- Base64 tests ---- */

static int
test_base64_basic(void)
{
	const uint8_t input[] = { 'H', 'e', 'l', 'l', 'o' };
	char out[16];
	char untouched[sizeof(out) - sizeof("SGVsbG8=")];
	memset(out, 0x5a, sizeof(out));
	memset(untouched, 0x5a, sizeof(untouched));
	int ret = dssh_base64_encode(input, sizeof(input), out, sizeof(out));
	ASSERT_EQ(ret, 8);
	/* Compare the terminator with a fixed extent, without trusting output
	 * termination before inspecting it. The unused capacity stays intact. */
	ASSERT_MEM_EQ(out, "SGVsbG8=", sizeof("SGVsbG8="));
	ASSERT_MEM_EQ(out + sizeof("SGVsbG8="), untouched, sizeof(untouched));
	return TEST_PASS;
}

static int
test_base64_null(void)
{
	char out[16];
	const uint8_t in[4] = { 0 };

	ASSERT_ERR(dssh_base64_encode(NULL, 4, out, sizeof(out)),
	    DSSH_ERROR_INVALID);
	ASSERT_ERR(dssh_base64_encode(in, 4, NULL, 16),
	    DSSH_ERROR_INVALID);

	return TEST_PASS;
}

static int
test_base64_small_buf(void)
{
	const uint8_t input[] = { 'H', 'e', 'l', 'l', 'o' };
	char out[4]; /* Too small for "SGVsbG8=" (8 chars + NUL). */
	int ret = dssh_base64_encode(input, sizeof(input), out, sizeof(out));

	/* Should fail — output buffer too small. */
	ASSERT_ERR(ret, DSSH_ERROR_TOOLONG);

	return TEST_PASS;
}

static int
test_base64_result_range(void)
{
	const uint8_t input[] = { 0 };
	char output[] = { 'x' };
	/* These lengths must be rejected before any buffer access. */
	ASSERT_ERR(dssh_base64_encode(input, SIZE_MAX, output, SIZE_MAX), DSSH_ERROR_TOOLONG);
#if SIZE_MAX > INT_MAX
	size_t length = ((size_t)INT_MAX / 4) * 3 + 1;
	ASSERT_ERR(dssh_base64_encode(input, length, output, SIZE_MAX), DSSH_ERROR_TOOLONG);
#endif
	ASSERT_EQ(output[0], 'x');
	return TEST_PASS;
}

/* ---- Memcmp tests ---- */

static int
test_memcmp_equal(void)
{
	const uint8_t a[] = { 1, 2, 3, 4, 5 };
	const uint8_t b[] = { 1, 2, 3, 4, 5 };

	ASSERT_EQ(dssh_crypto_memcmp(a, b, 5), 0);
	return TEST_PASS;
}

static int
test_memcmp_differ(void)
{
	const uint8_t a[] = { 1, 2, 3, 4, 5 };
	const uint8_t b[] = { 1, 2, 3, 4, 6 };

	ASSERT_NE(dssh_crypto_memcmp(a, b, 5), 0);
	return TEST_PASS;
}

/* ---- Cleanse test ---- */

static int
test_cleanse(void)
{
	uint8_t buf[16];

	memset(buf, 0xAA, sizeof(buf));
	dssh_cleanse(buf, sizeof(buf));

	/* Verify all bytes zeroed.  (volatile read to prevent
	 * the compiler from optimizing away the check.) */
	volatile uint8_t *vbuf = buf;
	int any_nonzero = 0;

	for (size_t i = 0; i < sizeof(buf); i++) {
		if (vbuf[i] != 0) {
			any_nonzero = 1;
			break;
		}
	}
	ASSERT_FALSE(any_nonzero);

	/* NULL is safe. */
	dssh_cleanse(NULL, 16);
	dssh_cleanse(buf, 0);

	return TEST_PASS;
}

/* ---- Hash oneshot tests ---- */

static int
test_hash_oneshot_sha256(void)
{
	const uint8_t *input = (const uint8_t *)"abc";
	uint8_t out[32];

	ASSERT_EQ(dssh_hash_oneshot("SHA-256",
	    input, 3, out, sizeof(out)), 0);
	ASSERT_MEM_EQ(out, sha256_abc, 32);

	return TEST_PASS;
}

static int
test_hash_oneshot_sha512(void)
{
	const uint8_t *input = (const uint8_t *)"";
	uint8_t out[64];

	ASSERT_EQ(dssh_hash_oneshot("SHA-512",
	    input, 0, out, sizeof(out)), 0);
	ASSERT_MEM_EQ(out, sha512_empty, 64);

	return TEST_PASS;
}

static int
test_hash_oneshot_empty(void)
{
	const uint8_t *input = (const uint8_t *)"";
	uint8_t out[32];

	ASSERT_EQ(dssh_hash_oneshot("SHA-256",
	    input, 0, out, sizeof(out)), 0);
	ASSERT_MEM_EQ(out, sha256_empty, 32);

	return TEST_PASS;
}

static int
test_hash_oneshot_null_params(void)
{
	uint8_t out[32];
	const uint8_t data = 0;

	/* NULL name. */
	ASSERT_ERR(dssh_hash_oneshot(NULL, &data, 1, out, sizeof(out)),
	    DSSH_ERROR_INVALID);

	/* NULL out. */
	ASSERT_ERR(dssh_hash_oneshot("SHA-256", &data, 1, NULL, 32),
	    DSSH_ERROR_INVALID);

	/* NULL data with non-zero len. */
	ASSERT_ERR(dssh_hash_oneshot("SHA-256", NULL, 1, out, sizeof(out)),
	    DSSH_ERROR_INVALID);

	/* NULL data with zero len is OK. */
	ASSERT_EQ(dssh_hash_oneshot("SHA-256", NULL, 0, out, sizeof(out)), 0);
	ASSERT_MEM_EQ(out, sha256_empty, 32);

	return TEST_PASS;
}

static int
test_hash_oneshot_small_buf(void)
{
	const uint8_t *input = (const uint8_t *)"abc";
	uint8_t out[16]; /* Too small for SHA-256 (32 bytes). */

	ASSERT_ERR(dssh_hash_oneshot("SHA-256",
	    input, 3, out, sizeof(out)),
	    DSSH_ERROR_INVALID);

	return TEST_PASS;
}

static int
test_hash_oneshot_unknown_algo(void)
{
	const uint8_t *input = (const uint8_t *)"abc";
	uint8_t out[32];

	ASSERT_ERR(dssh_hash_oneshot("NONSENSE",
	    input, 3, out, sizeof(out)),
	    DSSH_ERROR_INIT);

	return TEST_PASS;
}

/* ---- hash_update NULL data ---- */

static int
test_hash_update_null_data(void)
{
	dssh_hash_ctx *ctx = NULL;
	size_t digest_len = 0;
	uint8_t out[32];
	bool valid = dssh_hash_init(&ctx, "SHA-256", &digest_len) == 0
	    && ctx != NULL && digest_len == sizeof(out)
	    && dssh_hash_update(ctx, NULL, 32) == DSSH_ERROR_INVALID
	    && dssh_hash_update(ctx, NULL, 0) == 0
	    && dssh_hash_final(ctx, out, sizeof(out)) == 0
	    && memcmp(out, sha256_empty, sizeof(out)) == 0;
	dssh_hash_free(ctx);
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

/* ---- memcmp NULL inputs ---- */

static int
test_memcmp_null_inputs(void)
{
	const uint8_t buf[] = { 1, 2, 3 };

	/* NULL a with non-zero len. */
	ASSERT_NE(dssh_crypto_memcmp(NULL, buf, 3), 0);

	/* NULL b with non-zero len. */
	ASSERT_NE(dssh_crypto_memcmp(buf, NULL, 3), 0);

	/* Both NULL with non-zero len. */
	ASSERT_NE(dssh_crypto_memcmp(NULL, NULL, 3), 0);

	/* Both NULL with zero len is equal. */
	ASSERT_EQ(dssh_crypto_memcmp(NULL, NULL, 0), 0);

	return TEST_PASS;
}

#include "crypto_contract_cases.h"

/* ---- Test table ---- */

static struct dssh_test_entry tests[] = {
	{ "contract/hash_binary", test_contract_hash_binary },
	{ "contract/hash_million", test_contract_hash_million },
	{ "contract/hash_rejections", test_contract_hash_rejections },
	{ "contract/base64_vectors", test_contract_base64 },
	{ "contract/zero_lengths", test_contract_zero_lengths },
#ifdef DSSH_CRYPTO_OPENSSL
	{ "contract/encoded_lengths", test_contract_encoded_lengths },
	{ "contract/hash_failures", test_contract_hash_failures },
	{ "contract/hash_reported_lengths", test_contract_hash_reported_lengths },
	{ "contract/hash_publication", test_contract_hash_publication },
	{ "contract/oneshot_rng_errors", test_contract_oneshot_rng_errors },
	{ "contract/rng_request", test_contract_rng_request },
#endif
	{ "hash/sha256",           test_hash_sha256 },
	{ "hash/sha256_data",      test_hash_sha256_data },
	{ "hash/final_short_buf",  test_hash_final_short_buf },
	{ "hash/null_params",      test_hash_null_params },
	{ "hash/final_null_out",   test_hash_final_null_out },
	{ "hash/unknown_algo",     test_hash_unknown_algo },
	{ "hash/reuse",            test_hash_reuse },
	{ "hash/sha512",           test_hash_sha512 },
	{ "random/basic",          test_random_basic },
	{ "random/null",           test_random_null },
	{ "random/zero_len",       test_random_zero_len },
	{ "base64/basic",          test_base64_basic },
	{ "base64/null",           test_base64_null },
	{ "base64/small_buf",      test_base64_small_buf },
	{ "base64/result_range",   test_base64_result_range },
	{ "memcmp/equal",          test_memcmp_equal },
	{ "memcmp/differ",         test_memcmp_differ },
	{ "cleanse",               test_cleanse },
	{ "hash_oneshot/sha256",       test_hash_oneshot_sha256 },
	{ "hash_oneshot/sha512",       test_hash_oneshot_sha512 },
	{ "hash_oneshot/empty",        test_hash_oneshot_empty },
	{ "hash_oneshot/null_params",  test_hash_oneshot_null_params },
	{ "hash_oneshot/small_buf",    test_hash_oneshot_small_buf },
	{ "hash_oneshot/unknown_algo", test_hash_oneshot_unknown_algo },
	{ "hash/update_null_data",     test_hash_update_null_data },
	{ "memcmp/null_inputs",        test_memcmp_null_inputs },
};

DSSH_TEST_NO_CLEANUP
DSSH_TEST_MAIN(tests)
