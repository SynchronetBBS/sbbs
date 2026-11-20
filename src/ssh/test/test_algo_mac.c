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
#include "test/dssh_test_alloc.h"
#include "test_mac.h"

#ifdef DSSH_CRYPTO_OPENSSL
#include "dssh_test_ossl.h"
#include <openssl/core_names.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/params.h>
#elif defined(DSSH_CRYPTO_BOTAN)
#include <botan/ffi.h>
#endif
#include <string.h>

/*
 * Provide extern definitions for the DSSH_TESTABLE inline functions
 * from ssh-trans.c.  When compiled with -DDSSH_TESTING these become
 * plain (non-static) inline functions, and C requires an extern
 * definition in exactly one translation unit to satisfy the linker.
 */
extern bool has_nulls(uint8_t *buf, size_t buflen);
extern bool missing_crlf(uint8_t *buf, size_t buflen);
extern bool is_version_line(uint8_t *buf, size_t buflen);
extern bool has_non_ascii(uint8_t *buf, size_t buflen);
extern bool is_20(uint8_t *buf, size_t buflen);

/* ----------------------------------------------------------------
 * Helper: compute HMAC-SHA-256 by calling the selected native provider directly.
 * Returns 0 on success, fills outbuf with 32-byte digest.
 * ---------------------------------------------------------------- */
#ifdef DSSH_CRYPTO_OPENSSL
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
	if (EVP_MAC_final(ctx, outbuf, &outlen, 32) != 1 || outlen != 32)
		goto out_ctx;

	ret = 0;

out_ctx:
	EVP_MAC_CTX_free(ctx);
out_mac:
	EVP_MAC_free(mac);
	return ret;
}
#elif defined(DSSH_CRYPTO_BOTAN)
static int
hmac_sha256(const uint8_t *key, size_t key_len,
    const uint8_t *data, size_t data_len,
    uint8_t *outbuf)
{
	botan_mac_t mac = NULL;

	if (botan_mac_init(&mac, "HMAC(SHA-256)", 0) != 0 || mac == NULL)
		return -1;

	int ret = -1;
	size_t output_length = 0;
	if (botan_mac_output_length(mac, &output_length) != 0 || output_length != 32)
		goto out;
	if (botan_mac_set_key(mac, key, key_len) != 0)
		goto out;
	if (botan_mac_update(mac, data, data_len) != 0)
		goto out;
	if (botan_mac_final(mac, outbuf) != 0)
		goto out;
	ret = 0;

out:
	if (botan_mac_destroy(mac) != 0)
		ret = -1;
	return ret;
}
#endif

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
	dssh_session_cleanup(sess);
	ASSERT_EQ(ret, DSSH_ERROR_TOOLATE);
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
	dssh_session_cleanup(sess);
	ASSERT_EQ(ret, DSSH_ERROR_TOOLATE);
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

	const uint8_t empty[] = {0};
	ASSERT_EQ(hmac_sha256(key, 32, empty, 0, digest), 0);

	static const uint8_t expected[32] = {
		0x2e, 0x72, 0xeb, 0x66, 0x26, 0xe1, 0xd4, 0xf7,
		0xfc, 0x6d, 0xff, 0x11, 0x15, 0x85, 0x89, 0x8f,
		0x2e, 0x37, 0xc4, 0x71, 0xa6, 0x88, 0x62, 0xf0,
		0x7f, 0x10, 0xaa, 0x28, 0x24, 0xd5, 0x7c, 0x75,
	};
	ASSERT_MEM_EQ(digest, expected, sizeof(expected));
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

static const uint8_t tc2_under_tc1[32] = {
	0x6a, 0x05, 0x5a, 0xfb, 0x12, 0x95, 0xef, 0x9d,
	0xe3, 0x56, 0x05, 0x91, 0x9c, 0xbb, 0x8f, 0x86,
	0xf5, 0x1e, 0xe1, 0x83, 0x90, 0x1f, 0x00, 0x1e,
	0x6d, 0xc5, 0x3e, 0xc3, 0xd2, 0x48, 0x0b, 0xa9,
};

/* ================================================================
 * HMAC-SHA-256: Persistent native context with fixed answers.
 * OpenSSL reuses one context with an explicit key; Botan final retains the key.
 * Registered-module key/reset behavior is tested separately below.
 * ================================================================ */

#ifdef DSSH_CRYPTO_OPENSSL
static int
test_hmac_sha256_persistent_ctx(void)
{
	EVP_MAC *mac = EVP_MAC_fetch(NULL, "HMAC", NULL);
	EVP_MAC_CTX *ctx = mac == NULL ? NULL : EVP_MAC_CTX_new(mac);
	OSSL_PARAM params[] = {
		OSSL_PARAM_construct_utf8_string(OSSL_MAC_PARAM_DIGEST, "SHA256", 0),
		OSSL_PARAM_construct_end(),
	};
	uint8_t output[3][32];
	size_t length = 0;
	bool valid = ctx != NULL
	    && EVP_MAC_init(ctx, rfc4231_tc1_key, sizeof(rfc4231_tc1_key), params) == 1;
	for (unsigned round = 0; round < 3 && valid; round++) {
		const uint8_t *data = round == 0 ? rfc4231_tc1_data : rfc4231_tc2_data;
		size_t count = round == 0 ? rfc4231_tc1_data_len : rfc4231_tc2_data_len;
		const uint8_t *expected = round == 0 ? rfc4231_tc1_hmac : tc2_under_tc1;
		length = 0;
		valid = EVP_MAC_init(ctx, rfc4231_tc1_key, sizeof(rfc4231_tc1_key), NULL) == 1
		    && EVP_MAC_update(ctx, data, count) == 1
		    && EVP_MAC_final(ctx, output[round], &length, sizeof(output[round])) == 1
		    && length == sizeof(output[round])
		    && memcmp(output[round], expected, sizeof(output[round])) == 0;
	}
	EVP_MAC_CTX_free(ctx);
	EVP_MAC_free(mac);
	ASSERT_TRUE(valid);
	return TEST_PASS;
}
#elif defined(DSSH_CRYPTO_BOTAN)
static int
test_hmac_sha256_persistent_ctx(void)
{
	botan_mac_t mac = NULL;
	int ret = botan_mac_init(&mac, "HMAC(SHA-256)", 0);
	bool created = ret == 0 && mac != NULL;
	size_t length = 0;
	bool valid = created && botan_mac_output_length(mac, &length) == 0 && length == 32
	    && botan_mac_set_key(mac, rfc4231_tc1_key, sizeof(rfc4231_tc1_key)) == 0;
	uint8_t output[3][32];
	for (unsigned round = 0; round < 3 && valid; round++) {
		const uint8_t *data = round == 0 ? rfc4231_tc1_data : rfc4231_tc2_data;
		size_t count = round == 0 ? rfc4231_tc1_data_len : rfc4231_tc2_data_len;
		const uint8_t *expected = round == 0 ? rfc4231_tc1_hmac : tc2_under_tc1;
		valid = botan_mac_update(mac, data, count) == 0
		    && botan_mac_final(mac, output[round]) == 0
		    && memcmp(output[round], expected, sizeof(output[round])) == 0;
	}
	if (created) {
		int released = botan_mac_destroy(mac);
		valid = released == 0 && valid;
	}
	ASSERT_TRUE(valid);
	return TEST_PASS;
}
#endif

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
 * HMAC-SHA-512: Registration
 * ================================================================ */

static int
test_register_hmac_sha2_512(void)
{
	dssh_test_reset_global_config();
	int ret = dssh_register_hmac_sha2_512();
	ASSERT_EQ(ret, 0);
	return TEST_PASS;
}

static int
test_register_hmac_after_lock_512(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_hmac_sha2_512(), 0);
	ASSERT_EQ(dssh_register_hmac_sha2_256(), 0);
	ASSERT_EQ(dssh_register_none_mac(), 0);
	ASSERT_EQ(dssh_register_aes256_ctr(), 0);
	ASSERT_EQ(dssh_register_none_comp(), 0);
	ASSERT_EQ(dssh_register_curve25519_sha256(), 0);
	ASSERT_EQ(dssh_register_ssh_ed25519(), 0);

	dssh_session sess = dssh_session_init(true, 0);
	ASSERT_NOT_NULL(sess);

	int ret = dssh_register_hmac_sha2_512();
	dssh_session_cleanup(sess);
	ASSERT_EQ(ret, DSSH_ERROR_TOOLATE);
	return TEST_PASS;
}

/* ================================================================
 * HMAC-SHA-512: RFC 4231 Test Case 1 (SHA-512 truncated to 256 not
 * used here -- we verify full 64-byte HMAC-SHA-512 output)
 *
 * Key  = 20 bytes of 0x0b
 * Data = "Hi There"
 * HMAC-SHA-512 = 87aa7cdea5ef619d4ff0b4241a1d6cb0
 *                2379f4e2ce4ec2787ad0b30545e17cde
 *                daa833b7d6b8a702038b274eaea3f4e4
 *                be9d914eeb61f1702e696c203a126854
 * ================================================================ */

static const uint8_t rfc4231_tc1_hmac512[64] = {
	0x87, 0xaa, 0x7c, 0xde, 0xa5, 0xef, 0x61, 0x9d,
	0x4f, 0xf0, 0xb4, 0x24, 0x1a, 0x1d, 0x6c, 0xb0,
	0x23, 0x79, 0xf4, 0xe2, 0xce, 0x4e, 0xc2, 0x78,
	0x7a, 0xd0, 0xb3, 0x05, 0x45, 0xe1, 0x7c, 0xde,
	0xda, 0xa8, 0x33, 0xb7, 0xd6, 0xb8, 0xa7, 0x02,
	0x03, 0x8b, 0x27, 0x4e, 0xae, 0xa3, 0xf4, 0xe4,
	0xbe, 0x9d, 0x91, 0x4e, 0xeb, 0x61, 0xf1, 0x70,
	0x2e, 0x69, 0x6c, 0x20, 0x3a, 0x12, 0x68, 0x54
};

/* ================================================================
 * HMAC-SHA-512: RFC 4231 Test Case 2
 *
 * Key  = "Jefe"
 * Data = "what do ya want for nothing?"
 * HMAC-SHA-512 = 164b7a7bfcf819e2e395fbe73b56e0a3
 *                87bd64222e831fd610270cd7ea250554
 *                9758bf75c05a994a6d034f65f8f0e6fd
 *                caeab1a34d4a6b4b636e070a38bce737
 * ================================================================ */

static const uint8_t rfc4231_tc2_hmac512[64] = {
	0x16, 0x4b, 0x7a, 0x7b, 0xfc, 0xf8, 0x19, 0xe2,
	0xe3, 0x95, 0xfb, 0xe7, 0x3b, 0x56, 0xe0, 0xa3,
	0x87, 0xbd, 0x64, 0x22, 0x2e, 0x83, 0x1f, 0xd6,
	0x10, 0x27, 0x0c, 0xd7, 0xea, 0x25, 0x05, 0x54,
	0x97, 0x58, 0xbf, 0x75, 0xc0, 0x5a, 0x99, 0x4a,
	0x6d, 0x03, 0x4f, 0x65, 0xf8, 0xf0, 0xe6, 0xfd,
	0xca, 0xea, 0xb1, 0xa3, 0x4d, 0x4a, 0x6b, 0x4b,
	0x63, 0x6e, 0x07, 0x0a, 0x38, 0xbc, 0xe7, 0x37
};

/* ================================================================
 * HMAC-SHA-512: RFC 4231 Test Case 3
 *
 * Key  = 20 bytes of 0xaa
 * Data = 50 bytes of 0xdd
 * HMAC-SHA-512 = fa73b0089d56a284efb0f0756c890be9
 *                b1b5dbdd8ee81a3655f83e33b2279d39
 *                bf3e848279a722c806b485a47e67c807
 *                b946a337bee8942674278859e13292fb
 * ================================================================ */

static const uint8_t rfc4231_tc3_hmac512[64] = {
	0xfa, 0x73, 0xb0, 0x08, 0x9d, 0x56, 0xa2, 0x84,
	0xef, 0xb0, 0xf0, 0x75, 0x6c, 0x89, 0x0b, 0xe9,
	0xb1, 0xb5, 0xdb, 0xdd, 0x8e, 0xe8, 0x1a, 0x36,
	0x55, 0xf8, 0x3e, 0x33, 0xb2, 0x27, 0x9d, 0x39,
	0xbf, 0x3e, 0x84, 0x82, 0x79, 0xa7, 0x22, 0xc8,
	0x06, 0xb4, 0x85, 0xa4, 0x7e, 0x67, 0xc8, 0x07,
	0xb9, 0x46, 0xa3, 0x37, 0xbe, 0xe8, 0x94, 0x26,
	0x74, 0x27, 0x88, 0x59, 0xe1, 0x32, 0x92, 0xfb
};

#ifdef DSSH_CRYPTO_OPENSSL
static int
hmac_sha512(const uint8_t *key, size_t key_len,
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
		    OSSL_MAC_PARAM_DIGEST, "SHA512", 0),
		OSSL_PARAM_construct_end(),
	};

	if (EVP_MAC_init(ctx, key, key_len, params) != 1)
		goto out_ctx;
	if (EVP_MAC_update(ctx, data, data_len) != 1)
		goto out_ctx;

	size_t outlen = 0;
	if (EVP_MAC_final(ctx, outbuf, &outlen, 64) != 1 || outlen != 64)
		goto out_ctx;

	ret = 0;

out_ctx:
	EVP_MAC_CTX_free(ctx);
out_mac:
	EVP_MAC_free(mac);
	return ret;
}
#elif defined(DSSH_CRYPTO_BOTAN)
static int
hmac_sha512(const uint8_t *key, size_t key_len,
    const uint8_t *data, size_t data_len,
    uint8_t *outbuf)
{
	botan_mac_t mac = NULL;

	if (botan_mac_init(&mac, "HMAC(SHA-512)", 0) != 0 || mac == NULL)
		return -1;

	int ret = -1;
	size_t output_length = 0;
	if (botan_mac_output_length(mac, &output_length) != 0 || output_length != 64)
		goto out;
	if (botan_mac_set_key(mac, key, key_len) != 0)
		goto out;
	if (botan_mac_update(mac, data, data_len) != 0)
		goto out;
	if (botan_mac_final(mac, outbuf) != 0)
		goto out;
	ret = 0;

out:
	if (botan_mac_destroy(mac) != 0)
		ret = -1;
	return ret;
}
#endif

static int
test_hmac_sha512_rfc4231_tc1(void)
{
	uint8_t digest[64];
	ASSERT_EQ(hmac_sha512(rfc4231_tc1_key, sizeof(rfc4231_tc1_key),
	    rfc4231_tc1_data, rfc4231_tc1_data_len, digest), 0);
	ASSERT_MEM_EQ(digest, rfc4231_tc1_hmac512, 64);
	return TEST_PASS;
}

static int
test_hmac_sha512_rfc4231_tc2(void)
{
	uint8_t digest[64];
	ASSERT_EQ(hmac_sha512(rfc4231_tc2_key, rfc4231_tc2_key_len,
	    rfc4231_tc2_data, rfc4231_tc2_data_len, digest), 0);
	ASSERT_MEM_EQ(digest, rfc4231_tc2_hmac512, 64);
	return TEST_PASS;
}

static int
test_hmac_sha512_rfc4231_tc3(void)
{
	uint8_t key[20];
	memset(key, 0xaa, 20);
	uint8_t data[50];
	memset(data, 0xdd, 50);

	uint8_t digest[64];
	ASSERT_EQ(hmac_sha512(key, 20, data, 50, digest), 0);
	ASSERT_MEM_EQ(digest, rfc4231_tc3_hmac512, 64);
	return TEST_PASS;
}

static int
test_hmac_sha512_deterministic(void)
{
	uint8_t d1[64], d2[64];
	ASSERT_EQ(hmac_sha512(rfc4231_tc1_key, sizeof(rfc4231_tc1_key),
	    rfc4231_tc1_data, rfc4231_tc1_data_len, d1), 0);
	ASSERT_EQ(hmac_sha512(rfc4231_tc1_key, sizeof(rfc4231_tc1_key),
	    rfc4231_tc1_data, rfc4231_tc1_data_len, d2), 0);
	ASSERT_MEM_EQ(d1, d2, 64);
	return TEST_PASS;
}

static int
test_hmac_sha512_different_key(void)
{
	uint8_t key2[20];
	memcpy(key2, rfc4231_tc1_key, 20);
	key2[0] ^= 0x01;

	uint8_t d1[64], d2[64];
	ASSERT_EQ(hmac_sha512(rfc4231_tc1_key, 20,
	    rfc4231_tc1_data, rfc4231_tc1_data_len, d1), 0);
	ASSERT_EQ(hmac_sha512(key2, 20,
	    rfc4231_tc1_data, rfc4231_tc1_data_len, d2), 0);
	ASSERT_TRUE(memcmp(d1, d2, 64) != 0);
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
	dssh_mac mac = gconf.mac_head;
	ASSERT_NOT_NULL(mac);
	ASSERT_EQ(mac->digest_size, 0);
	ASSERT_EQ(mac->key_size, 0);
	ASSERT_NULL(mac->init);
	ASSERT_NOT_NULL(mac->generate);
	ASSERT_NOT_NULL(mac->cleanup);
	uint8_t output[3] = {0x5a, 0x5a, 0x5a};
	ASSERT_EQ(mac->generate(NULL, 0, &output[1], NULL), 0);
	ASSERT_EQ(output[0], 0x5a);
	ASSERT_EQ(output[1], 0x5a);
	ASSERT_EQ(output[2], 0x5a);
	mac->cleanup(NULL);
	return TEST_PASS;
}

static int
mac_contract_generate(const uint8_t *buf, size_t bufsz, uint8_t *out, dssh_mac_ctx *ctx)
{
	(void)buf;
	(void)bufsz;
	(void)ctx;
	memset(out, 0, 32);
	return 0;
}

static int
test_mac_registration_contract(void)
{
	bool valid = true;
	for (unsigned variant = 0; variant < 4; variant++) {
		dssh_test_reset_global_config();
		if (dssh_register_hmac_sha2_256() != 0) {
			dssh_test_reset_global_config();
			return TEST_FAIL;
		}
		dssh_mac head = gconf.mac_head;
		ASSERT_NOT_NULL(head);
		if (sizeof(struct dssh_mac_s) > SIZE_MAX - sizeof("contract-mac")) {
			dssh_test_reset_global_config();
			return TEST_FAIL;
		}
		dssh_mac mac = calloc(1, sizeof(*mac) + sizeof("contract-mac"));
		if (mac == NULL) {
			dssh_test_reset_global_config();
			return TEST_FAIL;
		}
		memcpy(mac->name, "contract-mac", sizeof("contract-mac"));
		mac->digest_size = variant < 2 ? 32 : 0;
		mac->generate = variant % 2 == 0 ? NULL : mac_contract_generate;
		int ret = dssh_transport_register_mac(mac);
		int expected = variant == 0 ? DSSH_ERROR_INVALID : 0;
		valid = valid && ret == expected && gconf.mac_head == head;
		if (expected != 0)
			valid = valid && head->next == NULL && gconf.mac_tail == head
			    && gconf.mac_entries == 1 && mac->next == NULL;
		else
			valid = valid && head->next == mac && gconf.mac_tail == mac
			    && gconf.mac_entries == 2;
		bool published = head->next == mac;
		if (!published)
			free(mac);
		dssh_test_reset_global_config();
	}
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

static void
test_mac_observe_free(const void *ptr, void *data)
{
	(void)ptr;
	unsigned *count = data;
	if (*count == UINT_MAX)
		abort();
	(*count)++;
}

static int
test_mac_test_module_ownership(void)
{
	bool valid = true;
	for (unsigned oversized = 0; oversized < 2; oversized++) {
		dssh_test_reset_global_config();
		test_mac_reset();
		int ret = oversized != 0 ? register_test_mac_oversized() : register_test_mac();
		if (ret != 0) {
			dssh_test_reset_global_config();
			return TEST_FAIL;
		}
		dssh_mac mac = gconf.mac_head;
		ASSERT_NOT_NULL(mac);
		ASSERT_NOT_NULL(mac->init);
		ASSERT_NOT_NULL(mac->generate);
		ASSERT_NOT_NULL(mac->cleanup);
		ASSERT_EQ_U(mac->digest_size, oversized == 0 ? 32 : 128);
		ASSERT_EQ_U(mac->key_size, 32);
		unsigned duplicate_frees = 0;
		dssh_test_alloc_observe_frees(test_mac_observe_free, &duplicate_frees);
		ret = oversized != 0 ? register_test_mac() : register_test_mac_oversized();
		dssh_test_alloc_observe_frees(NULL, NULL);
		valid = valid && ret == DSSH_ERROR_INVALID && gconf.mac_head == mac
		    && mac->next == NULL && duplicate_frees == 1;
		dssh_session sess = dssh_session_init(true, 0);
		if (sess == NULL) {
			dssh_test_reset_global_config();
			return TEST_FAIL;
		}
		unsigned late_frees = 0;
		dssh_test_alloc_observe_frees(test_mac_observe_free, &late_frees);
		ret = oversized != 0 ? register_test_mac() : register_test_mac_oversized();
		dssh_test_alloc_observe_frees(NULL, NULL);
		valid = valid && ret == DSSH_ERROR_TOOLATE && late_frees == 1;
		uint8_t key[32] = {0};
		const uint8_t data[] = {0};
		uint8_t output[130];
		memset(output, 0x5a, sizeof(output));
		dssh_mac_ctx *ctx = NULL;
		ret = mac->init(key, &ctx);
		if (ret == 0 && ctx == NULL)
			ret = DSSH_ERROR_INIT;
		if (ret == 0)
			ret = mac->generate(data, sizeof(data), &output[1], ctx);
		valid = valid && ret == 0 && output[0] == 0x5a;
		for (size_t i = 1; i < sizeof(output); i++)
			valid = valid && output[i] == (i <= mac->digest_size ? 0 : 0x5a);
		/* The fixture's key is the first 32-byte member of its owned context. */
		dssh_test_alloc_watch_free(ctx, sizeof(key));
		mac->cleanup(ctx);
		bool cleared = false;
		bool released = dssh_test_alloc_watched_free(&cleared);
		dssh_test_alloc_watch_free(NULL, 0);
		valid = valid && released && cleared;
		dssh_session_cleanup(sess);
		dssh_test_reset_global_config();
	}
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

static int
test_mac_test_module_failures(void)
{
	bool valid = true;
	for (unsigned oversized = 0; oversized < 2; oversized++) {
		dssh_test_reset_global_config();
		test_mac_reset();
		dssh_test_alloc_fail_after(0);
		int ret = oversized != 0 ? register_test_mac_oversized() : register_test_mac();
		int calls = dssh_test_alloc_count();
		dssh_test_alloc_reset();
		valid = valid && ret == DSSH_ERROR_ALLOC && calls == 1 && gconf.mac_head == NULL;
		ret = oversized != 0 ? register_test_mac_oversized() : register_test_mac();
		if (ret != 0) {
			dssh_test_reset_global_config();
			return TEST_FAIL;
		}
		dssh_mac mac = gconf.mac_head;
		ASSERT_NOT_NULL(mac);
		ASSERT_NOT_NULL(mac->init);
		ASSERT_NOT_NULL(mac->generate);
		ASSERT_NOT_NULL(mac->cleanup);
		ASSERT_EQ_U(mac->digest_size, oversized == 0 ? 32 : 128);
		ASSERT_EQ_U(mac->key_size, 32);
		uint8_t key[32] = {0};
		dssh_mac_ctx *ctx = NULL;
		dssh_test_alloc_fail_after(0);
		ret = mac->init(key, &ctx);
		calls = dssh_test_alloc_count();
		dssh_test_alloc_reset();
		valid = valid && ret == DSSH_ERROR_ALLOC && calls == 1 && ctx == NULL;
		mac->cleanup(ctx);
		ctx = NULL;
		test_mac_fail_init_at(0);
		dssh_test_alloc_fail_after(0);
		ret = mac->init(key, &ctx);
		calls = dssh_test_alloc_count();
		dssh_test_alloc_reset();
		valid = valid && ret == DSSH_ERROR_INIT && calls == 0 && ctx == NULL
		    && test_mac_init_failure_seen();
		mac->cleanup(ctx);
		ctx = NULL;
		test_mac_reset();
		ret = mac->init(key, &ctx);
		uint8_t output[130];
		memset(output, 0x5a, sizeof(output));
		valid = ret == 0 && ctx != NULL && valid;
		if (ret == 0 && ctx != NULL) {
			const uint8_t data[] = {0};
			test_mac_fail_generate_at(0);
			ret = mac->generate(data, sizeof(data), &output[1], ctx);
			valid = valid && ret == DSSH_ERROR_INIT;
			for (size_t i = 0; i < sizeof(output); i++)
				valid = valid && output[i] == 0x5a;
			test_mac_reset();
			ret = mac->generate(data, sizeof(data), &output[1], ctx);
		}
		valid = valid && ret == 0 && output[0] == 0x5a
		    && output[mac->digest_size + 1U] == 0x5a;
		mac->cleanup(ctx);
		dssh_test_reset_global_config();
		test_mac_reset();
	}
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

static int
test_none_mac_no_init(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_none_mac(), 0);
	dssh_mac mac = gconf.mac_head;
	ASSERT_NOT_NULL(mac);
	ASSERT_NULL(mac->init);
	ASSERT_EQ(mac->key_size, 0);
	ASSERT_EQ(mac->digest_size, 0);
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

#ifdef DSSH_CRYPTO_OPENSSL
struct mac_context_allocation {
	size_t size;
	unsigned calls;
};

static void
observe_mac_context(size_t count, size_t size, void *data)
{
	struct mac_context_allocation *allocation = data;
	if (count != 1 || allocation->calls == UINT_MAX)
		abort();
	allocation->calls++;
	allocation->size = size;
}
#endif

/* Exercise the registered module callbacks, rather than only provider helpers.
 * RFC 4231's short keys can be zero-padded to SSH's fixed key lengths without
 * changing HMAC: both lengths are below the hash's key-padding block size. */
static int
test_mac_module_rfc4231_vectors(void)
{
	int (*registrations[])(void) = {
		dssh_register_hmac_sha2_256,
		dssh_register_hmac_sha2_512,
	};
	for (size_t algorithm = 0; algorithm < 2; algorithm++) {
		dssh_test_reset_global_config();
		ASSERT_EQ(registrations[algorithm](), 0);
		dssh_mac mac = gconf.mac_head;
		ASSERT_NOT_NULL(mac);
		ASSERT_NOT_NULL(mac->init);
		ASSERT_NOT_NULL(mac->generate);
		ASSERT_NOT_NULL(mac->cleanup);
		size_t digest_len = 32;
		if (algorithm == 1)
			digest_len = 64;
		ASSERT_EQ_U(mac->key_size, digest_len);
		ASSERT_EQ_U(mac->digest_size, digest_len);
		ASSERT_STR_EQ(mac->name, algorithm == 0 ? "hmac-sha2-256" : "hmac-sha2-512");
		const uint8_t *expected[] = {
			algorithm == 0 ? rfc4231_tc1_hmac : rfc4231_tc1_hmac512,
			algorithm == 0 ? rfc4231_tc2_hmac : rfc4231_tc2_hmac512,
			algorithm == 0 ? rfc4231_tc3_hmac : rfc4231_tc3_hmac512,
		};
		uint8_t data3[50];
		memset(data3, 0xdd, sizeof(data3));
		const uint8_t *data[] = { rfc4231_tc1_data, rfc4231_tc2_data, data3 };
		const size_t data_len[] = { rfc4231_tc1_data_len, rfc4231_tc2_data_len, sizeof(data3) };
		for (size_t vector = 0; vector < 3; vector++) {
			uint8_t key[64] = { 0 };
			if (vector == 0)
				memcpy(key, rfc4231_tc1_key, sizeof(rfc4231_tc1_key));
			else if (vector == 1)
				memcpy(key, rfc4231_tc2_key, rfc4231_tc2_key_len);
			else
				memset(key, 0xaa, 20);
			dssh_mac_ctx *ctx = NULL;
#ifdef DSSH_CRYPTO_OPENSSL
			struct mac_context_allocation allocation = {0};
			dssh_test_alloc_observe_allocations(observe_mac_context, &allocation);
			int init_result = mac->init(key, &ctx);
			dssh_test_alloc_observe_allocations(NULL, NULL);
#else
			int init_result = mac->init(key, &ctx);
#endif
			bool initialized = init_result == 0 && ctx != NULL;
			uint8_t output[2][66];
			memset(output, 0x5a, sizeof(output));
#ifdef DSSH_CRYPTO_OPENSSL
			dssh_test_alloc_fail_after(0);
#endif
			int first = initialized ? mac->generate(data[vector], data_len[vector], &output[0][1], ctx)
			    : DSSH_ERROR_INIT;
			int second = first == 0
			    ? mac->generate(data[vector], data_len[vector], &output[1][1], ctx) : first;
#ifdef DSSH_CRYPTO_OPENSSL
			int generation_allocations = dssh_test_alloc_count();
			dssh_test_alloc_reset();
			if (allocation.calls == 1 && ctx != NULL)
				dssh_test_alloc_watch_free(ctx, allocation.size);
#endif
			mac->cleanup(ctx);
#ifdef DSSH_CRYPTO_OPENSSL
			bool cleared = false;
			bool released = dssh_test_alloc_watched_free(&cleared);
			dssh_test_alloc_watch_free(NULL, 0);
			ASSERT_EQ(allocation.calls, 1);
			ASSERT_EQ(generation_allocations, 0);
			ASSERT_TRUE(released && cleared);
#endif
			ASSERT_EQ(init_result, 0);
			ASSERT_TRUE(initialized);
			ASSERT_EQ(first, 0);
			ASSERT_EQ(second, 0);
			for (size_t round = 0; round < 2; round++) {
				ASSERT_EQ(output[round][0], 0x5a);
				ASSERT_EQ(output[round][digest_len + 1], 0x5a);
				ASSERT_MEM_EQ(&output[round][1], expected[vector], digest_len);
			}
		}
	}
	dssh_test_reset_global_config();
	return TEST_PASS;
}

#ifdef DSSH_CRYPTO_OPENSSL
static int
test_mac_native_failures(void)
{
	int (*registrations[])(void) = {
		dssh_register_hmac_sha2_256, dssh_register_hmac_sha2_512,
	};
	bool valid = true;
	for (size_t algorithm = 0; algorithm < 2; algorithm++) {
		dssh_test_reset_global_config();
		ASSERT_EQ(registrations[algorithm](), 0);
		dssh_mac mac = gconf.mac_head;
		ASSERT_NOT_NULL(mac);
		ASSERT_NOT_NULL(mac->init);
		ASSERT_NOT_NULL(mac->generate);
		ASSERT_NOT_NULL(mac->cleanup);
		ASSERT_EQ_U(mac->digest_size, algorithm == 0 ? 32 : 64);
		ASSERT_EQ_U(mac->key_size, algorithm == 0 ? 32 : 64);
		uint8_t key[64] = {0};
		memcpy(key, rfc4231_tc1_key, sizeof(rfc4231_tc1_key));
		const uint8_t *expected = algorithm == 0 ? rfc4231_tc1_hmac : rfc4231_tc1_hmac512;
		for (int fail = 0; fail < 4; fail++) {
			dssh_mac_ctx *ctx = NULL;
			if (fail == 0)
				dssh_test_alloc_fail_after(0);
			else
				dssh_test_ossl_fail_after(fail - 1);
			int ret = mac->init(key, &ctx);
			int calls = dssh_test_ossl_count();
			dssh_test_alloc_reset();
			dssh_test_ossl_reset();
			int error = fail == 0 || fail == 2 ? DSSH_ERROR_ALLOC : DSSH_ERROR_INIT;
			valid = valid && ret == error && ctx == NULL && (fail == 0 || calls == fail);
			mac->cleanup(ctx);
		}
		for (int fail = 0; fail < 3; fail++) {
			dssh_mac_ctx *ctx = NULL;
			int ret = mac->init(key, &ctx);
			uint8_t output[66];
			memset(output, 0x5a, sizeof(output));
			valid = ret == 0 && ctx != NULL && valid;
			if (ret == 0 && ctx != NULL) {
				dssh_test_ossl_fail_after(fail);
				ret = mac->generate(rfc4231_tc1_data, rfc4231_tc1_data_len, &output[1], ctx);
				valid = valid && dssh_test_ossl_count() == fail + 1;
			}
			dssh_test_ossl_reset();
			valid = valid && ret == DSSH_ERROR_INIT;
			for (size_t i = 0; i < sizeof(output); i++)
				valid = valid && output[i] == 0x5a;
			mac->cleanup(ctx);
		}
		const size_t lengths[] = {0, mac->digest_size - 1U, mac->digest_size + 1U, SIZE_MAX};
		for (size_t i = 0; i < sizeof(lengths) / sizeof(lengths[0]); i++) {
			dssh_mac_ctx *ctx = NULL;
			int ret = mac->init(key, &ctx);
			uint8_t output[66];
			memset(output, 0x5a, sizeof(output));
			size_t actual = 0;
			valid = ret == 0 && ctx != NULL && valid;
			if (ret == 0 && ctx != NULL) {
				dssh_test_ossl_mac_length(lengths[i]);
				ret = mac->generate(rfc4231_tc1_data, rfc4231_tc1_data_len, &output[1], ctx);
				valid = valid && dssh_test_ossl_mac_observed(&actual) && actual == mac->digest_size;
			}
			dssh_test_ossl_reset();
			valid = valid && ret == DSSH_ERROR_INIT && output[0] == 0x5a
			    && output[mac->digest_size + 1U] == 0x5a
			    && memcmp(&output[1], expected, mac->digest_size) == 0;
			mac->cleanup(ctx);
		}
		int configured = EVP_set_default_properties(NULL, "provider=deucessh_missing_provider");
		dssh_mac_ctx *ctx = NULL;
		int ret = configured == 1 ? mac->init(key, &ctx) : 0;
		int restored = EVP_set_default_properties(NULL, "");
		ERR_clear_error();
		valid = valid && configured == 1 && restored == 1 && ret == DSSH_ERROR_INIT && ctx == NULL;
		mac->cleanup(ctx);
	}
	dssh_test_reset_global_config();
	ASSERT_TRUE(valid);
	return TEST_PASS;
}
#endif

/* ================================================================
 * Test table
 * ================================================================ */

static struct dssh_test_entry tests[] = {
	{ "mac/registration_contract",       test_mac_registration_contract },
	{ "mac/test_module_ownership",       test_mac_test_module_ownership },
	{ "mac/test_module_failures",        test_mac_test_module_failures },
#ifdef DSSH_CRYPTO_OPENSSL
	{ "mac/native_failures",             test_mac_native_failures },
#endif
	{ "mac/module_rfc4231_vectors",       test_mac_module_rfc4231_vectors },
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

	/* HMAC-SHA-512: registration */
	{ "mac/dssh_register_hmac_sha2_512",      test_register_hmac_sha2_512 },
	{ "mac/register_hmac_after_lock_512",    test_register_hmac_after_lock_512 },

	/* HMAC-SHA-512: RFC 4231 test vectors */
	{ "mac/hmac_sha512/rfc4231_tc1",     test_hmac_sha512_rfc4231_tc1 },
	{ "mac/hmac_sha512/rfc4231_tc2",     test_hmac_sha512_rfc4231_tc2 },
	{ "mac/hmac_sha512/rfc4231_tc3",     test_hmac_sha512_rfc4231_tc3 },

	/* HMAC-SHA-512: behavioral */
	{ "mac/hmac_sha512/deterministic",   test_hmac_sha512_deterministic },
	{ "mac/hmac_sha512/different_key",   test_hmac_sha512_different_key },

	/* None MAC */
	{ "mac/none/passthrough",            test_none_mac_passthrough },
	{ "mac/none/no_init",                test_none_mac_no_init },
};

/* Native handles, sessions and module contexts are retired before assertions. */
void
dssh_test_failure_cleanup(void)
{
	dssh_test_alloc_reset();
#ifdef DSSH_CRYPTO_OPENSSL
	dssh_test_ossl_reset();
#endif
	test_mac_reset();
	dssh_test_reset_global_config();
}
DSSH_TEST_MAIN(tests)
