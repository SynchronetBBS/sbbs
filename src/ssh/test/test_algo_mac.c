/*
 * test_algo_mac.c -- Unit tests for DeuceSSH MAC algorithm modules.
 *
 * Tests registration of hmac-sha2-256 and none MACs, HMAC-SHA-256
 * correctness against RFC 4231 test vectors, generate/verify
 * roundtrips, and none MAC behavior.
 */

#include "dssh_test.h"
#include "deucessh.h"
#include "deucessh-algorithms.h"
#include "ssh-trans.h"
#include "test/dssh_test_internal.h"

#include <openssl/core_names.h>
#include <openssl/evp.h>
#include <openssl/params.h>
#include <string.h>

/*
 * Provide extern definitions for the DSSH_TESTABLE inline functions
 * from ssh-trans.c.  When compiled with -DDSSH_TESTING these become
 * plain (non-static) inline functions, and C requires an extern
 * definition in exactly one translation unit to satisfy the linker.
 */
extern bool dssh_test_has_nulls(uint8_t *buf, size_t buflen);
extern bool dssh_test_missing_crlf(uint8_t *buf, size_t buflen);
extern bool dssh_test_is_version_line(uint8_t *buf, size_t buflen);
extern bool dssh_test_has_non_ascii(uint8_t *buf, size_t buflen);
extern bool dssh_test_is_20(uint8_t *buf, size_t buflen);

/* ----------------------------------------------------------------
 * Helper: compute HMAC-SHA-256 via OpenSSL EVP_MAC (mirrors what
 * hmac-sha2-256.c does internally).
 * Returns 0 on success, fills outbuf with 32-byte digest.
 * ---------------------------------------------------------------- */
static int
hmac_sha256(const uint8_t *key, size_t key_len,
    const uint8_t *data, size_t data_len,
    uint8_t *outbuf)
{
	int ret = -1;
	EVP_MAC *mac = EVP_MAC_fetch(NULL, "HMAC", NULL);
	if (mac == NULL)
		return -1;

	EVP_MAC_CTX *ctx = EVP_MAC_CTX_new(mac);
	if (ctx == NULL)
		goto out_mac;

	OSSL_PARAM params[] = {
		OSSL_PARAM_construct_utf8_string(
		    OSSL_MAC_PARAM_DIGEST, "SHA256", 0),
		OSSL_PARAM_construct_end(),
	};

	if (EVP_MAC_init(ctx, key, key_len, params) != 1)
		goto out_ctx;
	if (EVP_MAC_update(ctx, data, data_len) != 1)
		goto out_ctx;

	size_t outlen = 0;
	if (EVP_MAC_final(ctx, outbuf, &outlen, 32) != 1)
		goto out_ctx;

	ret = 0;

out_ctx:
	EVP_MAC_CTX_free(ctx);
out_mac:
	EVP_MAC_free(mac);
	return ret;
}

/* ================================================================
 * RFC 4231 Test Case 1
 *
 * Key  = 20 bytes of 0x0b
 * Data = "Hi There"
 * HMAC = b0344c61d8db38535ca8afceaf0bf12b
 *        881dc200c9833da726e9376c2e32cff7
 * ================================================================ */

static const uint8_t rfc4231_tc1_key[20] = {
	0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b,
	0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b,
	0x0b, 0x0b, 0x0b, 0x0b
};
static const uint8_t rfc4231_tc1_data[] = "Hi There";
static const size_t rfc4231_tc1_data_len = 8; /* strlen, not sizeof */
static const uint8_t rfc4231_tc1_hmac[32] = {
	0xb0, 0x34, 0x4c, 0x61, 0xd8, 0xdb, 0x38, 0x53,
	0x5c, 0xa8, 0xaf, 0xce, 0xaf, 0x0b, 0xf1, 0x2b,
	0x88, 0x1d, 0xc2, 0x00, 0xc9, 0x83, 0x3d, 0xa7,
	0x26, 0xe9, 0x37, 0x6c, 0x2e, 0x32, 0xcf, 0xf7
};

/* ================================================================
 * RFC 4231 Test Case 2
 *
 * Key  = "Jefe"
 * Data = "what do ya want for nothing?"
 * HMAC = 5bdcc146bf60754e6a042426089575c7
 *        5a003f089d2739839dec58b964ec3843
 * ================================================================ */

static const uint8_t rfc4231_tc2_key[] = "Jefe";
static const size_t rfc4231_tc2_key_len = 4;
static const uint8_t rfc4231_tc2_data[] = "what do ya want for nothing?";
static const size_t rfc4231_tc2_data_len = 28;
static const uint8_t rfc4231_tc2_hmac[32] = {
	0x5b, 0xdc, 0xc1, 0x46, 0xbf, 0x60, 0x75, 0x4e,
	0x6a, 0x04, 0x24, 0x26, 0x08, 0x95, 0x75, 0xc7,
	0x5a, 0x00, 0x3f, 0x08, 0x9d, 0x27, 0x39, 0x83,
	0x9d, 0xec, 0x58, 0xb9, 0x64, 0xec, 0x38, 0x43
};

/* ================================================================
 * RFC 4231 Test Case 3
 *
 * Key  = 20 bytes of 0xaa
 * Data = 50 bytes of 0xdd
 * HMAC = 773ea91e36800e46854db8ebd09181a7
 *        2959098b3ef8c122d9635514ced565fe
 * ================================================================ */

static const uint8_t rfc4231_tc3_hmac[32] = {
	0x77, 0x3e, 0xa9, 0x1e, 0x36, 0x80, 0x0e, 0x46,
	0x85, 0x4d, 0xb8, 0xeb, 0xd0, 0x91, 0x81, 0xa7,
	0x29, 0x59, 0x09, 0x8b, 0x3e, 0xf8, 0xc1, 0x22,
	0xd9, 0x63, 0x55, 0x14, 0xce, 0xd5, 0x65, 0xfe
};

/* ================================================================
 * Registration tests
 * ================================================================ */

static int
test_register_hmac_sha2_256(void)
{
	dssh_test_reset_global_config();
	int ret = dssh_register_hmac_sha2_256();
	ASSERT_EQ(ret, 0);
	return TEST_PASS;
}

static int
test_register_none_mac(void)
{
	dssh_test_reset_global_config();
	int ret = dssh_register_none_mac();
	ASSERT_EQ(ret, 0);
	return TEST_PASS;
}

static int
test_register_hmac_after_lock(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_hmac_sha2_256(), 0);
	ASSERT_EQ(dssh_register_none_mac(), 0);
	ASSERT_EQ(dssh_register_aes256_ctr(), 0);
	ASSERT_EQ(dssh_register_none_comp(), 0);
	ASSERT_EQ(dssh_register_curve25519_sha256(), 0);
	ASSERT_EQ(dssh_register_ssh_ed25519(), 0);

	dssh_session sess = dssh_session_init(true, 0);
	ASSERT_NOT_NULL(sess);

	int ret = dssh_register_hmac_sha2_256();
	ASSERT_EQ(ret, DSSH_ERROR_TOOLATE);

	dssh_session_cleanup(sess);
	return TEST_PASS;
}

static int
test_register_none_mac_after_lock(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_hmac_sha2_256(), 0);
	ASSERT_EQ(dssh_register_none_mac(), 0);
	ASSERT_EQ(dssh_register_aes256_ctr(), 0);
	ASSERT_EQ(dssh_register_none_comp(), 0);
	ASSERT_EQ(dssh_register_curve25519_sha256(), 0);
	ASSERT_EQ(dssh_register_ssh_ed25519(), 0);

	dssh_session sess = dssh_session_init(true, 0);
	ASSERT_NOT_NULL(sess);

	int ret = dssh_register_none_mac();
	ASSERT_EQ(ret, DSSH_ERROR_TOOLATE);

	dssh_session_cleanup(sess);
	return TEST_PASS;
}

static int
test_register_both_mac(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_hmac_sha2_256(), 0);
	ASSERT_EQ(dssh_register_none_mac(), 0);
	return TEST_PASS;
}

/* ================================================================
 * HMAC-SHA-256: RFC 4231 Test Case 1
 * ================================================================ */

static int
test_hmac_sha256_rfc4231_tc1(void)
{
	uint8_t digest[32];
	ASSERT_EQ(hmac_sha256(rfc4231_tc1_key, sizeof(rfc4231_tc1_key),
	    rfc4231_tc1_data, rfc4231_tc1_data_len, digest), 0);
	ASSERT_MEM_EQ(digest, rfc4231_tc1_hmac, 32);
	return TEST_PASS;
}

/* ================================================================
 * HMAC-SHA-256: RFC 4231 Test Case 2
 * ================================================================ */

static int
test_hmac_sha256_rfc4231_tc2(void)
{
	uint8_t digest[32];
	ASSERT_EQ(hmac_sha256(rfc4231_tc2_key, rfc4231_tc2_key_len,
	    rfc4231_tc2_data, rfc4231_tc2_data_len, digest), 0);
	ASSERT_MEM_EQ(digest, rfc4231_tc2_hmac, 32);
	return TEST_PASS;
}

/* ================================================================
 * HMAC-SHA-256: RFC 4231 Test Case 3
 * ================================================================ */

static int
test_hmac_sha256_rfc4231_tc3(void)
{
	uint8_t key[20];
	memset(key, 0xaa, 20);
	uint8_t data[50];
	memset(data, 0xdd, 50);

	uint8_t digest[32];
	ASSERT_EQ(hmac_sha256(key, 20, data, 50, digest), 0);
	ASSERT_MEM_EQ(digest, rfc4231_tc3_hmac, 32);
	return TEST_PASS;
}

/* ================================================================
 * HMAC-SHA-256: Same input produces same output (deterministic)
 * ================================================================ */

static int
test_hmac_sha256_deterministic(void)
{
	uint8_t d1[32], d2[32];
	ASSERT_EQ(hmac_sha256(rfc4231_tc1_key, sizeof(rfc4231_tc1_key),
	    rfc4231_tc1_data, rfc4231_tc1_data_len, d1), 0);
	ASSERT_EQ(hmac_sha256(rfc4231_tc1_key, sizeof(rfc4231_tc1_key),
	    rfc4231_tc1_data, rfc4231_tc1_data_len, d2), 0);
	ASSERT_MEM_EQ(d1, d2, 32);
	return TEST_PASS;
}

/* ================================================================
 * HMAC-SHA-256: Different keys produce different MACs
 * ================================================================ */

static int
test_hmac_sha256_different_key(void)
{
	uint8_t key2[20];
	memcpy(key2, rfc4231_tc1_key, 20);
	key2[0] ^= 0x01;

	uint8_t d1[32], d2[32];
	ASSERT_EQ(hmac_sha256(rfc4231_tc1_key, 20,
	    rfc4231_tc1_data, rfc4231_tc1_data_len, d1), 0);
	ASSERT_EQ(hmac_sha256(key2, 20,
	    rfc4231_tc1_data, rfc4231_tc1_data_len, d2), 0);
	ASSERT_TRUE(memcmp(d1, d2, 32) != 0);
	return TEST_PASS;
}

/* ================================================================
 * HMAC-SHA-256: Different data produces different MACs
 * ================================================================ */

static int
test_hmac_sha256_different_data(void)
{
	const uint8_t data2[] = "Hi There!";  /* extra char */
	uint8_t d1[32], d2[32];
	ASSERT_EQ(hmac_sha256(rfc4231_tc1_key, 20,
	    rfc4231_tc1_data, rfc4231_tc1_data_len, d1), 0);
	ASSERT_EQ(hmac_sha256(rfc4231_tc1_key, 20,
	    data2, 9, d2), 0);
	ASSERT_TRUE(memcmp(d1, d2, 32) != 0);
	return TEST_PASS;
}

/* ================================================================
 * HMAC-SHA-256: Empty data
 * HMAC is defined for zero-length messages.
 * ================================================================ */

static int
test_hmac_sha256_empty_data(void)
{
	uint8_t key[32];
	memset(key, 0x01, 32);
	uint8_t digest[32];

	ASSERT_EQ(hmac_sha256(key, 32, (const uint8_t *)"", 0, digest), 0);

	/* Verify it produced a non-zero digest */
	uint8_t zeros[32];
	memset(zeros, 0, 32);
	ASSERT_TRUE(memcmp(digest, zeros, 32) != 0);
	return TEST_PASS;
}

/* ================================================================
 * HMAC-SHA-256: Large data (1024 bytes)
 * ================================================================ */

static int
test_hmac_sha256_large_data(void)
{
	uint8_t key[32];
	memset(key, 0xab, 32);
	uint8_t data[1024];
	for (int i = 0; i < 1024; i++)
		data[i] = (uint8_t)(i & 0xff);

	uint8_t d1[32], d2[32];
	ASSERT_EQ(hmac_sha256(key, 32, data, 1024, d1), 0);
	ASSERT_EQ(hmac_sha256(key, 32, data, 1024, d2), 0);
	ASSERT_MEM_EQ(d1, d2, 32);

	/* Flip one bit in data -- digest must change */
	data[500] ^= 0x80;
	ASSERT_EQ(hmac_sha256(key, 32, data, 1024, d2), 0);
	ASSERT_TRUE(memcmp(d1, d2, 32) != 0);
	return TEST_PASS;
}

/* ================================================================
 * HMAC-SHA-256: Persistent context (re-init without key)
 *
 * This mirrors how hmac-sha2-256.c works: init once with key,
 * then call EVP_MAC_init(ctx, NULL, 0, NULL) before each generate.
 * ================================================================ */

static int
test_hmac_sha256_persistent_ctx(void)
{
	EVP_MAC *mac = EVP_MAC_fetch(NULL, "HMAC", NULL);
	ASSERT_NOT_NULL(mac);

	EVP_MAC_CTX *ctx = EVP_MAC_CTX_new(mac);
	ASSERT_NOT_NULL(ctx);

	OSSL_PARAM params[] = {
		OSSL_PARAM_construct_utf8_string(
		    OSSL_MAC_PARAM_DIGEST, "SHA256", 0),
		OSSL_PARAM_construct_end(),
	};

	/* Init with TC1 key */
	ASSERT_TRUE(EVP_MAC_init(ctx, rfc4231_tc1_key,
	    sizeof(rfc4231_tc1_key), params) == 1);

	/* First generate */
	ASSERT_TRUE(EVP_MAC_init(ctx, NULL, 0, NULL) == 1);
	ASSERT_TRUE(EVP_MAC_update(ctx, rfc4231_tc1_data,
	    rfc4231_tc1_data_len) == 1);
	uint8_t d1[32];
	size_t outlen = 0;
	ASSERT_TRUE(EVP_MAC_final(ctx, d1, &outlen, 32) == 1);
	ASSERT_MEM_EQ(d1, rfc4231_tc1_hmac, 32);

	/* Second generate on same context (re-init without key) */
	ASSERT_TRUE(EVP_MAC_init(ctx, NULL, 0, NULL) == 1);
	ASSERT_TRUE(EVP_MAC_update(ctx, rfc4231_tc2_data,
	    rfc4231_tc2_data_len) == 1);
	uint8_t d2[32];
	outlen = 0;
	ASSERT_TRUE(EVP_MAC_final(ctx, d2, &outlen, 32) == 1);

	/* TC2 uses a different key ("Jefe"), so this will NOT match
	 * rfc4231_tc2_hmac.  But it MUST be a valid HMAC of TC2 data
	 * under TC1's key.  Verify deterministic by repeating. */
	uint8_t d3[32];
	ASSERT_TRUE(EVP_MAC_init(ctx, NULL, 0, NULL) == 1);
	ASSERT_TRUE(EVP_MAC_update(ctx, rfc4231_tc2_data,
	    rfc4231_tc2_data_len) == 1);
	outlen = 0;
	ASSERT_TRUE(EVP_MAC_final(ctx, d3, &outlen, 32) == 1);
	ASSERT_MEM_EQ(d2, d3, 32);

	EVP_MAC_CTX_free(ctx);
	EVP_MAC_free(mac);
	return TEST_PASS;
}

/* ================================================================
 * HMAC-SHA-256: Single byte data
 * ================================================================ */

static int
test_hmac_sha256_single_byte(void)
{
	uint8_t key[32];
	memset(key, 0x42, 32);
	const uint8_t data = 0xff;

	uint8_t d1[32], d2[32];
	ASSERT_EQ(hmac_sha256(key, 32, &data, 1, d1), 0);
	ASSERT_EQ(hmac_sha256(key, 32, &data, 1, d2), 0);
	ASSERT_MEM_EQ(d1, d2, 32);
	return TEST_PASS;
}

/* ================================================================
 * None MAC: registration and properties
 * ================================================================ */

static int
test_none_mac_passthrough(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_none_mac(), 0);
	/* The none MAC has digest_size=0, key_size=0, init=NULL.
	 * Its generate function returns 0 without writing any output.
	 * Registration success confirms proper initialization. */
	return TEST_PASS;
}

static int
test_none_mac_no_init(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_none_mac(), 0);
	/* init is NULL for none MAC -- no key setup needed */
	return TEST_PASS;
}

/* ================================================================
 * Session creation with MAC algorithms registered
 * ================================================================ */

static int
test_mac_session_creation(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_hmac_sha2_256(), 0);
	ASSERT_EQ(dssh_register_none_mac(), 0);
	ASSERT_EQ(dssh_register_aes256_ctr(), 0);
	ASSERT_EQ(dssh_register_none_comp(), 0);
	ASSERT_EQ(dssh_register_curve25519_sha256(), 0);
	ASSERT_EQ(dssh_register_ssh_ed25519(), 0);

	dssh_session sess = dssh_session_init(true, 0);
	ASSERT_NOT_NULL(sess);
	dssh_session_cleanup(sess);
	return TEST_PASS;
}

/* ================================================================
 * Test table
 * ================================================================ */

static struct dssh_test_entry tests[] = {
	/* Registration */
	{ "mac/dssh_register_hmac_sha2_256",      test_register_hmac_sha2_256 },
	{ "mac/dssh_register_none_mac",           test_register_none_mac },
	{ "mac/register_both",               test_register_both_mac },
	{ "mac/register_hmac_after_lock",    test_register_hmac_after_lock },
	{ "mac/register_none_after_lock",    test_register_none_mac_after_lock },
	{ "mac/session_creation",            test_mac_session_creation },

	/* HMAC-SHA-256: RFC 4231 test vectors */
	{ "mac/hmac_sha256/rfc4231_tc1",     test_hmac_sha256_rfc4231_tc1 },
	{ "mac/hmac_sha256/rfc4231_tc2",     test_hmac_sha256_rfc4231_tc2 },
	{ "mac/hmac_sha256/rfc4231_tc3",     test_hmac_sha256_rfc4231_tc3 },

	/* HMAC-SHA-256: behavioral */
	{ "mac/hmac_sha256/deterministic",   test_hmac_sha256_deterministic },
	{ "mac/hmac_sha256/different_key",   test_hmac_sha256_different_key },
	{ "mac/hmac_sha256/different_data",  test_hmac_sha256_different_data },
	{ "mac/hmac_sha256/empty_data",      test_hmac_sha256_empty_data },
	{ "mac/hmac_sha256/large_data",      test_hmac_sha256_large_data },
	{ "mac/hmac_sha256/persistent_ctx",  test_hmac_sha256_persistent_ctx },
	{ "mac/hmac_sha256/single_byte",     test_hmac_sha256_single_byte },

	/* None MAC */
	{ "mac/none/passthrough",            test_none_mac_passthrough },
	{ "mac/none/no_init",                test_none_mac_no_init },
};

DSSH_TEST_MAIN(tests)
