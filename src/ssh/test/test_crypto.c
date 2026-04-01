/*
 * test_crypto.c -- Unit tests for deucessh-crypto.h backend-neutral API.
 */

#include "dssh_test.h"
#include "deucessh.h"
#include "deucessh-crypto.h"

#include <string.h>

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

	ASSERT_OK(dssh_hash_init(&ctx, "SHA-256", &digest_len));
	ASSERT_NOT_NULL(ctx);
	ASSERT_EQ_U(digest_len, 32);

	/* Hash empty string. */
	uint8_t out[32];

	ASSERT_OK(dssh_hash_final(ctx, out, sizeof(out)));
	ASSERT_MEM_EQ(out, sha256_empty, 32);

	dssh_hash_free(ctx);
	return TEST_PASS;
}

static int
test_hash_sha256_data(void)
{
	dssh_hash_ctx *ctx = NULL;
	size_t digest_len = 0;

	ASSERT_OK(dssh_hash_init(&ctx, "SHA-256", &digest_len));

	const uint8_t abc[] = { 'a', 'b', 'c' };

	ASSERT_OK(dssh_hash_update(ctx, abc, 3));

	uint8_t out[32];

	ASSERT_OK(dssh_hash_final(ctx, out, sizeof(out)));
	ASSERT_MEM_EQ(out, sha256_abc, 32);

	dssh_hash_free(ctx);
	return TEST_PASS;
}

static int
test_hash_final_short_buf(void)
{
	dssh_hash_ctx *ctx = NULL;
	size_t digest_len = 0;

	ASSERT_OK(dssh_hash_init(&ctx, "SHA-256", &digest_len));

	/* outlen (1) < digest_len (32) should fail. */
	uint8_t out[1];

	ASSERT_ERR(dssh_hash_final(ctx, out, 1), DSSH_ERROR_INVALID);

	dssh_hash_free(ctx);
	return TEST_PASS;
}

static int
test_hash_null_params(void)
{
	dssh_hash_ctx *ctx = NULL;
	size_t digest_len = 0;

	/* NULL ctx pointer. */
	ASSERT_ERR(dssh_hash_init(NULL, "SHA-256", &digest_len),
	    DSSH_ERROR_INVALID);

	/* NULL name. */
	ASSERT_ERR(dssh_hash_init(&ctx, NULL, &digest_len),
	    DSSH_ERROR_INVALID);

	/* NULL digest_len output. */
	ASSERT_ERR(dssh_hash_init(&ctx, "SHA-256", NULL),
	    DSSH_ERROR_INVALID);

	/* NULL ctx to update. */
	uint8_t data = 0;

	ASSERT_ERR(dssh_hash_update(NULL, &data, 1), DSSH_ERROR_INVALID);

	/* NULL ctx to final. */
	uint8_t out[32];

	ASSERT_ERR(dssh_hash_final(NULL, out, sizeof(out)),
	    DSSH_ERROR_INVALID);

	/* Free NULL is safe (no crash). */
	dssh_hash_free(NULL);

	return TEST_PASS;
}

static int
test_hash_final_null_out(void)
{
	dssh_hash_ctx *ctx = NULL;
	size_t digest_len = 0;

	ASSERT_OK(dssh_hash_init(&ctx, "SHA-256", &digest_len));

	/* NULL output buffer should return DSSH_ERROR_INVALID. */
	ASSERT_ERR(dssh_hash_final(ctx, NULL, digest_len),
	    DSSH_ERROR_INVALID);

	dssh_hash_free(ctx);
	return TEST_PASS;
}

static int
test_hash_unknown_algo(void)
{
	dssh_hash_ctx *ctx = NULL;
	size_t digest_len = 0;

	ASSERT_ERR(dssh_hash_init(&ctx, "NONSENSE", &digest_len),
	    DSSH_ERROR_INIT);
	ASSERT_NULL(ctx);

	return TEST_PASS;
}

static int
test_hash_reuse(void)
{
	dssh_hash_ctx *ctx = NULL;
	size_t digest_len = 0;

	ASSERT_OK(dssh_hash_init(&ctx, "SHA-256", &digest_len));

	/* First hash: empty → sha256_empty. */
	uint8_t out1[32];

	ASSERT_OK(dssh_hash_final(ctx, out1, sizeof(out1)));
	ASSERT_MEM_EQ(out1, sha256_empty, 32);

	/* Context should be reset — hash "abc" next. */
	const uint8_t abc[] = { 'a', 'b', 'c' };

	ASSERT_OK(dssh_hash_update(ctx, abc, 3));

	uint8_t out2[32];

	ASSERT_OK(dssh_hash_final(ctx, out2, sizeof(out2)));
	ASSERT_MEM_EQ(out2, sha256_abc, 32);

	dssh_hash_free(ctx);
	return TEST_PASS;
}

static int
test_hash_sha512(void)
{
	dssh_hash_ctx *ctx = NULL;
	size_t digest_len = 0;

	ASSERT_OK(dssh_hash_init(&ctx, "SHA-512", &digest_len));
	ASSERT_EQ_U(digest_len, 64);

	/* Just verify init/final/free lifecycle works for SHA-512. */
	uint8_t out[64];

	ASSERT_OK(dssh_hash_final(ctx, out, sizeof(out)));

	dssh_hash_free(ctx);
	return TEST_PASS;
}

/* ---- Random tests ---- */

static int
test_random_basic(void)
{
	uint8_t buf[32];

	memset(buf, 0, sizeof(buf));
	ASSERT_OK(dssh_random(buf, sizeof(buf)));

	/* At least one byte should be non-zero (probability of all-zero
	 * from a CSPRNG is astronomically low). */
	int any_nonzero = 0;

	for (size_t i = 0; i < sizeof(buf); i++) {
		if (buf[i] != 0) {
			any_nonzero = 1;
			break;
		}
	}
	ASSERT_TRUE(any_nonzero);
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
	uint8_t buf[4] = { 0xAA, 0xBB, 0xCC, 0xDD };

	ASSERT_OK(dssh_random(buf, 0));

	/* Buffer should be untouched. */
	ASSERT_EQ(buf[0], 0xAA);
	ASSERT_EQ(buf[1], 0xBB);

	return TEST_PASS;
}

/* ---- Base64 tests ---- */

static int
test_base64_basic(void)
{
	/* Base64("Hello") = "SGVsbG8=" */
	const uint8_t input[] = { 'H', 'e', 'l', 'l', 'o' };
	char out[16];

	int ret = dssh_base64_encode(input, sizeof(input), out, sizeof(out));

	ASSERT_TRUE(ret > 0);
	ASSERT_STR_EQ(out, "SGVsbG8=");
	ASSERT_EQ(ret, 8);

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
	ASSERT_TRUE(ret < 0);

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
	uint8_t out[32];

	ASSERT_OK(dssh_hash_oneshot("SHA-256",
	    (const uint8_t *)"abc", 3, out, sizeof(out)));
	ASSERT_MEM_EQ(out, sha256_abc, 32);

	return TEST_PASS;
}

static int
test_hash_oneshot_sha512(void)
{
	uint8_t out[64];

	ASSERT_OK(dssh_hash_oneshot("SHA-512",
	    (const uint8_t *)"", 0, out, sizeof(out)));
	ASSERT_MEM_EQ(out, sha512_empty, 64);

	return TEST_PASS;
}

static int
test_hash_oneshot_empty(void)
{
	uint8_t out[32];

	ASSERT_OK(dssh_hash_oneshot("SHA-256",
	    (const uint8_t *)"", 0, out, sizeof(out)));
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
	ASSERT_OK(dssh_hash_oneshot("SHA-256", NULL, 0, out, sizeof(out)));
	ASSERT_MEM_EQ(out, sha256_empty, 32);

	return TEST_PASS;
}

static int
test_hash_oneshot_small_buf(void)
{
	uint8_t out[16]; /* Too small for SHA-256 (32 bytes). */

	ASSERT_ERR(dssh_hash_oneshot("SHA-256",
	    (const uint8_t *)"abc", 3, out, sizeof(out)),
	    DSSH_ERROR_INVALID);

	return TEST_PASS;
}

static int
test_hash_oneshot_unknown_algo(void)
{
	uint8_t out[32];

	ASSERT_ERR(dssh_hash_oneshot("NONSENSE",
	    (const uint8_t *)"abc", 3, out, sizeof(out)),
	    DSSH_ERROR_INIT);

	return TEST_PASS;
}

/* ---- hash_update NULL data ---- */

static int
test_hash_update_null_data(void)
{
	dssh_hash_ctx *ctx = NULL;
	size_t digest_len = 0;

	ASSERT_OK(dssh_hash_init(&ctx, "SHA-256", &digest_len));

	/* NULL data with non-zero len must fail. */
	ASSERT_ERR(dssh_hash_update(ctx, NULL, 32), DSSH_ERROR_INVALID);

	/* NULL data with zero len is OK (no-op). */
	ASSERT_OK(dssh_hash_update(ctx, NULL, 0));

	/* Context still usable after the failed update. */
	uint8_t out[32];

	ASSERT_OK(dssh_hash_final(ctx, out, sizeof(out)));
	ASSERT_MEM_EQ(out, sha256_empty, 32);

	dssh_hash_free(ctx);
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

/* ---- Test table ---- */

static struct dssh_test_entry tests[] = {
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
