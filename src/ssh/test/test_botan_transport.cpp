/*
 * test/test_botan_transport.cpp -- Botan-specific enc/mac tests.
 *
 * Covers gaps left by OpenSSL-specific injection tests:
 * - AES-256-CTR and HMAC-SHA-256/512 round-trip verification
 * - NULL ctx handling
 * - Enc/MAC through registered function pointers
 */

#include <cstdlib>
#include <cstring>

extern "C" {
#include "dssh_test.h"
#include "deucessh.h"
#include "deucessh-algorithms.h"
#include "deucessh-crypto.h"
}

/*
 * We can't include deucessh-enc.h / deucessh-mac.h / ssh-trans.h from
 * C++ because they use flexible array members and _Atomic.  Forward-
 * declare what we need.
 */
typedef struct dssh_enc_ctx dssh_enc_ctx;
typedef struct dssh_mac_ctx dssh_mac_ctx;

/* Reset function from test library */
extern "C" void dssh_test_reset_global_config(void);

/* Declared in enc/aes256-ctr-botan.cpp */
extern "C" int dssh_botan_aes256ctr_init(const uint8_t *key,
    const uint8_t *iv, bool encrypt_dir, dssh_enc_ctx **ctx);
extern "C" int dssh_botan_aes256ctr_crypt(uint8_t *buf, size_t bufsz,
    dssh_enc_ctx *ctx);
extern "C" void dssh_botan_aes256ctr_cleanup(dssh_enc_ctx *ctx);

/* Declared in mac/hmac-sha2-256-botan.cpp */
extern "C" int dssh_botan_hmac256_init(const uint8_t *key,
    dssh_mac_ctx **out);
extern "C" int dssh_botan_hmac256_generate(const uint8_t *buf,
    size_t bufsz, uint8_t *outbuf, dssh_mac_ctx *ctx);
extern "C" void dssh_botan_hmac256_cleanup(dssh_mac_ctx *ctx);

/* Declared in mac/hmac-sha2-512-botan.cpp */
extern "C" int dssh_botan_hmac512_init(const uint8_t *key,
    dssh_mac_ctx **out);
extern "C" int dssh_botan_hmac512_generate(const uint8_t *buf,
    size_t bufsz, uint8_t *outbuf, dssh_mac_ctx *ctx);
extern "C" void dssh_botan_hmac512_cleanup(dssh_mac_ctx *ctx);

/* ================================================================
 * AES-256-CTR tests
 * ================================================================ */

static int
test_aes256_ctr_roundtrip(void)
{
	uint8_t key[32], iv[16];
	memset(key, 0x42, sizeof(key));
	memset(iv, 0x01, sizeof(iv));

	/* Encrypt */
	dssh_enc_ctx *enc_ctx = nullptr;
	ASSERT_EQ(dssh_botan_aes256ctr_init(key, iv, true, &enc_ctx), 0);
	ASSERT_NOT_NULL(enc_ctx);

	uint8_t plain[64];
	memset(plain, 0xAA, sizeof(plain));
	uint8_t buf[64];
	memcpy(buf, plain, sizeof(buf));

	ASSERT_EQ(dssh_botan_aes256ctr_crypt(buf, sizeof(buf), enc_ctx), 0);
	/* Ciphertext must differ from plaintext */
	ASSERT_TRUE(memcmp(buf, plain, sizeof(buf)) != 0);
	dssh_botan_aes256ctr_cleanup(enc_ctx);

	/* Decrypt with fresh ctx (same key/iv, CTR is symmetric) */
	dssh_enc_ctx *dec_ctx = nullptr;
	ASSERT_EQ(dssh_botan_aes256ctr_init(key, iv, false, &dec_ctx), 0);
	ASSERT_NOT_NULL(dec_ctx);

	ASSERT_EQ(dssh_botan_aes256ctr_crypt(buf, sizeof(buf), dec_ctx), 0);
	ASSERT_MEM_EQ(buf, plain, sizeof(buf));
	dssh_botan_aes256ctr_cleanup(dec_ctx);

	return TEST_PASS;
}

static int
test_aes256_ctr_null_ctx(void)
{
	uint8_t buf[16] = {0};
	ASSERT_EQ(dssh_botan_aes256ctr_crypt(buf, sizeof(buf), nullptr),
	    DSSH_ERROR_INIT);
	return TEST_PASS;
}

static int
test_aes256_ctr_cleanup_null(void)
{
	/* Must not crash */
	dssh_botan_aes256ctr_cleanup(nullptr);
	return TEST_PASS;
}

static int
test_aes256_ctr_multiple_blocks(void)
{
	uint8_t key[32], iv[16];
	dssh_random(key, sizeof(key));
	memset(iv, 0, sizeof(iv));

	dssh_enc_ctx *ctx = nullptr;
	ASSERT_EQ(dssh_botan_aes256ctr_init(key, iv, true, &ctx), 0);

	/* Encrypt in two separate calls (streaming CTR) */
	uint8_t buf1[16], buf2[16];
	memset(buf1, 0x11, sizeof(buf1));
	memset(buf2, 0x22, sizeof(buf2));

	ASSERT_EQ(dssh_botan_aes256ctr_crypt(buf1, sizeof(buf1), ctx), 0);
	ASSERT_EQ(dssh_botan_aes256ctr_crypt(buf2, sizeof(buf2), ctx), 0);
	dssh_botan_aes256ctr_cleanup(ctx);

	/* Decrypt as one 32-byte block with same key/iv */
	dssh_enc_ctx *dec = nullptr;
	ASSERT_EQ(dssh_botan_aes256ctr_init(key, iv, false, &dec), 0);

	uint8_t combined[32];
	memcpy(combined, buf1, 16);
	memcpy(combined + 16, buf2, 16);
	ASSERT_EQ(dssh_botan_aes256ctr_crypt(combined, 32, dec), 0);
	dssh_botan_aes256ctr_cleanup(dec);

	/* Verify plaintext recovered */
	uint8_t expected[32];
	memset(expected, 0x11, 16);
	memset(expected + 16, 0x22, 16);
	ASSERT_MEM_EQ(combined, expected, 32);

	return TEST_PASS;
}

/* ================================================================
 * HMAC-SHA-256 tests
 * ================================================================ */

static int
test_hmac256_roundtrip(void)
{
	uint8_t key[32];
	memset(key, 0x42, sizeof(key));

	dssh_mac_ctx *ctx = nullptr;
	ASSERT_EQ(dssh_botan_hmac256_init(key, &ctx), 0);
	ASSERT_NOT_NULL(ctx);

	uint8_t data[] = "test message for HMAC";
	uint8_t mac1[32], mac2[32];

	/* Generate twice — must be deterministic for same data */
	ASSERT_EQ(dssh_botan_hmac256_generate(data, sizeof(data),
	    mac1, ctx), 0);
	ASSERT_EQ(dssh_botan_hmac256_generate(data, sizeof(data),
	    mac2, ctx), 0);
	ASSERT_MEM_EQ(mac1, mac2, 32);

	/* Different data must produce different MAC */
	uint8_t data2[] = "different message";
	uint8_t mac3[32];
	ASSERT_EQ(dssh_botan_hmac256_generate(data2, sizeof(data2),
	    mac3, ctx), 0);
	ASSERT_TRUE(memcmp(mac1, mac3, 32) != 0);

	dssh_botan_hmac256_cleanup(ctx);
	return TEST_PASS;
}

static int
test_hmac256_null_ctx(void)
{
	uint8_t buf[16] = {0};
	uint8_t out[32];
	ASSERT_EQ(dssh_botan_hmac256_generate(buf, sizeof(buf), out, nullptr),
	    DSSH_ERROR_INIT);
	return TEST_PASS;
}

static int
test_hmac256_cleanup_null(void)
{
	dssh_botan_hmac256_cleanup(nullptr);
	return TEST_PASS;
}

/* ================================================================
 * HMAC-SHA-512 tests
 * ================================================================ */

static int
test_hmac512_roundtrip(void)
{
	uint8_t key[64];
	memset(key, 0x42, sizeof(key));

	dssh_mac_ctx *ctx = nullptr;
	ASSERT_EQ(dssh_botan_hmac512_init(key, &ctx), 0);
	ASSERT_NOT_NULL(ctx);

	uint8_t data[] = "test message for HMAC-512";
	uint8_t mac1[64], mac2[64];

	ASSERT_EQ(dssh_botan_hmac512_generate(data, sizeof(data),
	    mac1, ctx), 0);
	ASSERT_EQ(dssh_botan_hmac512_generate(data, sizeof(data),
	    mac2, ctx), 0);
	ASSERT_MEM_EQ(mac1, mac2, 64);

	dssh_botan_hmac512_cleanup(ctx);
	return TEST_PASS;
}

static int
test_hmac512_null_ctx(void)
{
	uint8_t buf[16] = {0};
	uint8_t out[64];
	ASSERT_EQ(dssh_botan_hmac512_generate(buf, sizeof(buf), out, nullptr),
	    DSSH_ERROR_INIT);
	return TEST_PASS;
}

static int
test_hmac512_cleanup_null(void)
{
	dssh_botan_hmac512_cleanup(nullptr);
	return TEST_PASS;
}


/* ================================================================
 * Test table
 * ================================================================ */

static struct dssh_test_entry tests[] = {
	/* AES-256-CTR */
	{ "aes256_ctr/roundtrip",       test_aes256_ctr_roundtrip },
	{ "aes256_ctr/null_ctx",        test_aes256_ctr_null_ctx },
	{ "aes256_ctr/cleanup_null",    test_aes256_ctr_cleanup_null },
	{ "aes256_ctr/multi_block",     test_aes256_ctr_multiple_blocks },

	/* HMAC-SHA-256 */
	{ "hmac256/roundtrip",          test_hmac256_roundtrip },
	{ "hmac256/null_ctx",           test_hmac256_null_ctx },
	{ "hmac256/cleanup_null",       test_hmac256_cleanup_null },

	/* HMAC-SHA-512 */
	{ "hmac512/roundtrip",          test_hmac512_roundtrip },
	{ "hmac512/null_ctx",           test_hmac512_null_ctx },
	{ "hmac512/cleanup_null",       test_hmac512_cleanup_null },

};

DSSH_TEST_NO_CLEANUP
DSSH_TEST_MAIN(tests)
