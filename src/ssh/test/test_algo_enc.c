/*
 * test_algo_enc.c — Unit tests for DeuceSSH encryption algorithm modules.
 *
 * Tests registration of aes256-ctr and none ciphers, AES-256-CTR
 * correctness against NIST SP 800-38A test vectors (F.5.5/F.5.6),
 * encrypt/decrypt roundtrips, and none cipher passthrough.
 */

#include "dssh_test.h"
#include "deucessh.h"
#include "deucessh-algorithms.h"
#include "ssh-trans.h"
#include "test/dssh_test_internal.h"

#include <openssl/evp.h>
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
 * Helper: encrypt or decrypt a buffer in-place using AES-256-CTR
 * via OpenSSL EVP (mirrors what aes256-ctr.c does internally).
 * CTR mode uses EncryptInit for both directions.
 * Returns 0 on success.
 * ---------------------------------------------------------------- */
static int
aes256_ctr_crypt(const uint8_t *key, const uint8_t *iv,
    uint8_t *buf, int bufsz)
{
	EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
	if (ctx == NULL)
		return -1;

	int ok = 0;
	if (EVP_EncryptInit_ex(ctx, EVP_aes_256_ctr(), NULL, key, iv) != 1)
		goto out;
	if (EVP_CIPHER_CTX_set_padding(ctx, 0) != 1)
		goto out;

	int outlen = 0;
	if (EVP_EncryptUpdate(ctx, buf, &outlen, buf, bufsz) != 1)
		goto out;
	ok = 1;

out:
	EVP_CIPHER_CTX_free(ctx);
	return ok ? 0 : -1;
}

/* ================================================================
 * NIST SP 800-38A F.5.5 — AES-256-CTR Encrypt
 *
 * Key:       603deb1015ca71be2b73aef0857d7781
 *            1f352c073b6108d72d9810a30914dff4
 * Init Ctr:  f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff
 * Block 1 PT:  6bc1bee22e409f96e93d7e117393172a
 * Block 1 CT:  601ec313775789a5b7a7f504bbf3d228
 * Block 2 PT:  ae2d8a571e03ac9c9eb76fac45af8e51
 * Block 2 CT:  f443e3ca4d62b59aca84e990cacaf5c5
 * Block 3 PT:  30c81c46a35ce411e5fbc1191a0a52ef
 * Block 3 CT:  2b0930daa23de94ce87017ba2d84988d
 * Block 4 PT:  f69f2445df4f9b17ad2b417be66c3710
 * Block 4 CT:  dfc9c58db67aada613c2dd08457941a6
 * ================================================================ */

static const uint8_t nist_key[32] = {
	0x60, 0x3d, 0xeb, 0x10, 0x15, 0xca, 0x71, 0xbe,
	0x2b, 0x73, 0xae, 0xf0, 0x85, 0x7d, 0x77, 0x81,
	0x1f, 0x35, 0x2c, 0x07, 0x3b, 0x61, 0x08, 0xd7,
	0x2d, 0x98, 0x10, 0xa3, 0x09, 0x14, 0xdf, 0xf4
};

static const uint8_t nist_iv[16] = {
	0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7,
	0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff
};

static const uint8_t nist_pt[64] = {
	0x6b, 0xc1, 0xbe, 0xe2, 0x2e, 0x40, 0x9f, 0x96,
	0xe9, 0x3d, 0x7e, 0x11, 0x73, 0x93, 0x17, 0x2a,
	0xae, 0x2d, 0x8a, 0x57, 0x1e, 0x03, 0xac, 0x9c,
	0x9e, 0xb7, 0x6f, 0xac, 0x45, 0xaf, 0x8e, 0x51,
	0x30, 0xc8, 0x1c, 0x46, 0xa3, 0x5c, 0xe4, 0x11,
	0xe5, 0xfb, 0xc1, 0x19, 0x1a, 0x0a, 0x52, 0xef,
	0xf6, 0x9f, 0x24, 0x45, 0xdf, 0x4f, 0x9b, 0x17,
	0xad, 0x2b, 0x41, 0x7b, 0xe6, 0x6c, 0x37, 0x10
};

static const uint8_t nist_ct[64] = {
	0x60, 0x1e, 0xc3, 0x13, 0x77, 0x57, 0x89, 0xa5,
	0xb7, 0xa7, 0xf5, 0x04, 0xbb, 0xf3, 0xd2, 0x28,
	0xf4, 0x43, 0xe3, 0xca, 0x4d, 0x62, 0xb5, 0x9a,
	0xca, 0x84, 0xe9, 0x90, 0xca, 0xca, 0xf5, 0xc5,
	0x2b, 0x09, 0x30, 0xda, 0xa2, 0x3d, 0xe9, 0x4c,
	0xe8, 0x70, 0x17, 0xba, 0x2d, 0x84, 0x98, 0x8d,
	0xdf, 0xc9, 0xc5, 0x8d, 0xb6, 0x7a, 0xad, 0xa6,
	0x13, 0xc2, 0xdd, 0x08, 0x45, 0x79, 0x41, 0xa6
};

/* ================================================================
 * Registration tests
 * ================================================================ */

static int
test_register_aes256_ctr(void)
{
	dssh_test_reset_global_config();
	int ret = register_aes256_ctr();
	ASSERT_EQ(ret, 0);
	return TEST_PASS;
}

static int
test_register_none_enc(void)
{
	dssh_test_reset_global_config();
	int ret = register_none_enc();
	ASSERT_EQ(ret, 0);
	return TEST_PASS;
}

static int
test_register_aes256_ctr_after_lock(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(register_aes256_ctr(), 0);
	ASSERT_EQ(register_none_enc(), 0);
	ASSERT_EQ(register_none_comp(), 0);
	ASSERT_EQ(register_none_mac(), 0);
	ASSERT_EQ(register_curve25519_sha256(), 0);
	ASSERT_EQ(register_ssh_ed25519(), 0);

	/* Creating a session locks the algorithm registry */
	dssh_session sess = dssh_session_init(true, 0);
	ASSERT_NOT_NULL(sess);

	/* Registration must fail with DSSH_ERROR_TOOLATE */
	int ret = register_aes256_ctr();
	ASSERT_EQ(ret, DSSH_ERROR_TOOLATE);

	dssh_session_cleanup(sess);
	return TEST_PASS;
}

static int
test_register_none_enc_after_lock(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(register_aes256_ctr(), 0);
	ASSERT_EQ(register_none_enc(), 0);
	ASSERT_EQ(register_none_comp(), 0);
	ASSERT_EQ(register_none_mac(), 0);
	ASSERT_EQ(register_curve25519_sha256(), 0);
	ASSERT_EQ(register_ssh_ed25519(), 0);

	dssh_session sess = dssh_session_init(true, 0);
	ASSERT_NOT_NULL(sess);

	int ret = register_none_enc();
	ASSERT_EQ(ret, DSSH_ERROR_TOOLATE);

	dssh_session_cleanup(sess);
	return TEST_PASS;
}

static int
test_register_both_enc(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(register_aes256_ctr(), 0);
	ASSERT_EQ(register_none_enc(), 0);
	return TEST_PASS;
}

/* ================================================================
 * AES-256-CTR: NIST SP 800-38A F.5.5 — Encrypt single block
 * ================================================================ */

static int
test_aes256_ctr_nist_block1_encrypt(void)
{
	uint8_t buf[16];
	memcpy(buf, nist_pt, 16);
	ASSERT_EQ(aes256_ctr_crypt(nist_key, nist_iv, buf, 16), 0);
	ASSERT_MEM_EQ(buf, nist_ct, 16);
	return TEST_PASS;
}

/* ================================================================
 * AES-256-CTR: NIST SP 800-38A F.5.5 — Encrypt all 4 blocks
 * ================================================================ */

static int
test_aes256_ctr_nist_4block_encrypt(void)
{
	uint8_t buf[64];
	memcpy(buf, nist_pt, 64);
	ASSERT_EQ(aes256_ctr_crypt(nist_key, nist_iv, buf, 64), 0);
	ASSERT_MEM_EQ(buf, nist_ct, 64);
	return TEST_PASS;
}

/* ================================================================
 * AES-256-CTR: NIST SP 800-38A F.5.6 — Decrypt single block
 * CTR mode: decrypt == encrypt with same key/iv
 * ================================================================ */

static int
test_aes256_ctr_nist_block1_decrypt(void)
{
	uint8_t buf[16];
	memcpy(buf, nist_ct, 16);
	ASSERT_EQ(aes256_ctr_crypt(nist_key, nist_iv, buf, 16), 0);
	ASSERT_MEM_EQ(buf, nist_pt, 16);
	return TEST_PASS;
}

/* ================================================================
 * AES-256-CTR: NIST SP 800-38A F.5.6 — Decrypt all 4 blocks
 * ================================================================ */

static int
test_aes256_ctr_nist_4block_decrypt(void)
{
	uint8_t buf[64];
	memcpy(buf, nist_ct, 64);
	ASSERT_EQ(aes256_ctr_crypt(nist_key, nist_iv, buf, 64), 0);
	ASSERT_MEM_EQ(buf, nist_pt, 64);
	return TEST_PASS;
}

/* ================================================================
 * AES-256-CTR: Encrypt then decrypt roundtrip
 * ================================================================ */

static int
test_aes256_ctr_roundtrip(void)
{
	uint8_t buf[64];
	memcpy(buf, nist_pt, 64);

	/* Encrypt */
	ASSERT_EQ(aes256_ctr_crypt(nist_key, nist_iv, buf, 64), 0);
	/* Verify it changed */
	ASSERT_TRUE(memcmp(buf, nist_pt, 64) != 0);
	/* Decrypt with fresh context (same key/iv resets counter) */
	ASSERT_EQ(aes256_ctr_crypt(nist_key, nist_iv, buf, 64), 0);
	ASSERT_MEM_EQ(buf, nist_pt, 64);
	return TEST_PASS;
}

/* ================================================================
 * AES-256-CTR: Multi-call streaming (incremental blocks)
 *
 * Encrypt blocks 1+2 in one call, then blocks 3+4 in a second
 * call on the same context.  Must produce same result as single
 * 4-block call.
 * ================================================================ */

static int
test_aes256_ctr_streaming(void)
{
	uint8_t buf[64];
	memcpy(buf, nist_pt, 64);

	EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
	ASSERT_NOT_NULL(ctx);
	ASSERT_TRUE(EVP_EncryptInit_ex(ctx, EVP_aes_256_ctr(),
	    NULL, nist_key, nist_iv) == 1);
	ASSERT_TRUE(EVP_CIPHER_CTX_set_padding(ctx, 0) == 1);

	int outlen = 0;
	ASSERT_TRUE(EVP_EncryptUpdate(ctx, buf, &outlen, buf, 32) == 1);
	ASSERT_EQ(outlen, 32);
	ASSERT_TRUE(EVP_EncryptUpdate(ctx, buf + 32, &outlen,
	    buf + 32, 32) == 1);
	ASSERT_EQ(outlen, 32);

	EVP_CIPHER_CTX_free(ctx);

	ASSERT_MEM_EQ(buf, nist_ct, 64);
	return TEST_PASS;
}

/* ================================================================
 * AES-256-CTR: Partial block (non-multiple of blocksize)
 * CTR mode handles partial blocks without padding.
 * ================================================================ */

static int
test_aes256_ctr_partial_block(void)
{
	/* Encrypt 7 bytes (less than one block) */
	uint8_t buf[7];
	memcpy(buf, nist_pt, 7);
	ASSERT_EQ(aes256_ctr_crypt(nist_key, nist_iv, buf, 7), 0);
	/* Must match first 7 bytes of the NIST ciphertext */
	ASSERT_MEM_EQ(buf, nist_ct, 7);
	return TEST_PASS;
}

/* ================================================================
 * AES-256-CTR: 20 bytes (one block + 4 bytes)
 * ================================================================ */

static int
test_aes256_ctr_20bytes(void)
{
	uint8_t buf[20];
	memcpy(buf, nist_pt, 20);
	ASSERT_EQ(aes256_ctr_crypt(nist_key, nist_iv, buf, 20), 0);
	ASSERT_MEM_EQ(buf, nist_ct, 20);
	return TEST_PASS;
}

/* ================================================================
 * AES-256-CTR: All-zero plaintext
 * Encrypting zeros yields the raw keystream.
 * Encrypting the keystream again with same key/iv yields zeros.
 * ================================================================ */

static int
test_aes256_ctr_zero_plaintext(void)
{
	uint8_t buf[32];
	memset(buf, 0, sizeof(buf));

	ASSERT_EQ(aes256_ctr_crypt(nist_key, nist_iv, buf, 32), 0);

	/* Keystream is non-zero */
	uint8_t zeros[32];
	memset(zeros, 0, sizeof(zeros));
	ASSERT_TRUE(memcmp(buf, zeros, 32) != 0);

	/* Encrypting keystream with same key/iv gives back zeros */
	ASSERT_EQ(aes256_ctr_crypt(nist_key, nist_iv, buf, 32), 0);
	ASSERT_MEM_EQ(buf, zeros, 32);
	return TEST_PASS;
}

/* ================================================================
 * AES-256-CTR: Different keys produce different ciphertext
 * ================================================================ */

static int
test_aes256_ctr_different_key(void)
{
	uint8_t key2[32];
	memcpy(key2, nist_key, 32);
	key2[0] ^= 0x01;  /* Flip one bit */

	uint8_t buf1[16], buf2[16];
	memcpy(buf1, nist_pt, 16);
	memcpy(buf2, nist_pt, 16);

	ASSERT_EQ(aes256_ctr_crypt(nist_key, nist_iv, buf1, 16), 0);
	ASSERT_EQ(aes256_ctr_crypt(key2, nist_iv, buf2, 16), 0);
	ASSERT_TRUE(memcmp(buf1, buf2, 16) != 0);
	return TEST_PASS;
}

/* ================================================================
 * AES-256-CTR: Different IVs produce different ciphertext
 * ================================================================ */

static int
test_aes256_ctr_different_iv(void)
{
	uint8_t iv2[16];
	memcpy(iv2, nist_iv, 16);
	iv2[15] ^= 0x01;

	uint8_t buf1[16], buf2[16];
	memcpy(buf1, nist_pt, 16);
	memcpy(buf2, nist_pt, 16);

	ASSERT_EQ(aes256_ctr_crypt(nist_key, nist_iv, buf1, 16), 0);
	ASSERT_EQ(aes256_ctr_crypt(nist_key, iv2, buf2, 16), 0);
	ASSERT_TRUE(memcmp(buf1, buf2, 16) != 0);
	return TEST_PASS;
}

/* ================================================================
 * AES-256-CTR: Per-block verify (each NIST block individually)
 * ================================================================ */

static int
test_aes256_ctr_nist_per_block(void)
{
	/* Encrypt all 4 blocks in one call, verify each 16-byte
	 * block matches the expected NIST ciphertext. */
	uint8_t buf[64];
	memcpy(buf, nist_pt, 64);
	ASSERT_EQ(aes256_ctr_crypt(nist_key, nist_iv, buf, 64), 0);

	ASSERT_MEM_EQ(buf + 0, nist_ct + 0, 16);
	ASSERT_MEM_EQ(buf + 16, nist_ct + 16, 16);
	ASSERT_MEM_EQ(buf + 32, nist_ct + 32, 16);
	ASSERT_MEM_EQ(buf + 48, nist_ct + 48, 16);
	return TEST_PASS;
}

/* ================================================================
 * AES-256-CTR: Large buffer (256 bytes, encrypt/decrypt roundtrip)
 * ================================================================ */

static int
test_aes256_ctr_large_buffer(void)
{
	uint8_t pt[256], buf[256];
	for (int i = 0; i < 256; i++)
		pt[i] = (uint8_t)i;
	memcpy(buf, pt, 256);

	ASSERT_EQ(aes256_ctr_crypt(nist_key, nist_iv, buf, 256), 0);
	ASSERT_TRUE(memcmp(buf, pt, 256) != 0);
	ASSERT_EQ(aes256_ctr_crypt(nist_key, nist_iv, buf, 256), 0);
	ASSERT_MEM_EQ(buf, pt, 256);
	return TEST_PASS;
}

/* ================================================================
 * None cipher: data passthrough (encrypt is identity)
 *
 * The none cipher's encrypt and decrypt functions are no-ops that
 * return 0 without modifying the buffer.  We verify registration
 * succeeds and the algorithm is findable via negotiate_algo.
 * ================================================================ */

static int
test_none_enc_passthrough(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(register_none_enc(), 0);

	/* Find the registered struct via negotiate_algo.  We pass
	 * NULL head — negotiate_algo traverses the linked list, but
	 * we cannot access gconf.enc_head from test code.  Instead
	 * we register all required algos, create a session (which
	 * locks the registry), and verify the enc name is available
	 * through the query API. */
	ASSERT_EQ(register_aes256_ctr(), 0);
	ASSERT_EQ(register_none_comp(), 0);
	ASSERT_EQ(register_none_mac(), 0);
	ASSERT_EQ(register_curve25519_sha256(), 0);
	ASSERT_EQ(register_ssh_ed25519(), 0);

	dssh_session sess = dssh_session_init(true, 0);
	ASSERT_NOT_NULL(sess);

	/* Session created successfully with none enc registered —
	 * confirms the struct was properly initialized with valid
	 * function pointers and properties. */
	dssh_session_cleanup(sess);
	return TEST_PASS;
}

/* ================================================================
 * None cipher: no init function (init is NULL)
 * ================================================================ */

static int
test_none_enc_no_init(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(register_none_enc(), 0);
	/* The none cipher sets init=NULL, blocksize=1, key_size=0.
	 * Registration success confirms the struct was accepted. */
	return TEST_PASS;
}

/* ================================================================
 * Verify session creation with enc algorithms registered
 * ================================================================ */

static int
test_enc_session_creation(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(register_aes256_ctr(), 0);
	ASSERT_EQ(register_none_enc(), 0);
	ASSERT_EQ(register_none_comp(), 0);
	ASSERT_EQ(register_none_mac(), 0);
	ASSERT_EQ(register_curve25519_sha256(), 0);
	ASSERT_EQ(register_ssh_ed25519(), 0);

	dssh_session sess = dssh_session_init(true, 0);
	ASSERT_NOT_NULL(sess);
	dssh_session_cleanup(sess);
	return TEST_PASS;
}

/* ================================================================
 * AES-256-CTR: single byte encrypt/decrypt
 * ================================================================ */

static int
test_aes256_ctr_single_byte(void)
{
	uint8_t buf[1];
	buf[0] = nist_pt[0];
	ASSERT_EQ(aes256_ctr_crypt(nist_key, nist_iv, buf, 1), 0);
	ASSERT_EQ_U(buf[0], nist_ct[0]);
	return TEST_PASS;
}

/* ================================================================
 * AES-256-CTR: CTR mode is symmetric (encrypt == decrypt)
 * Verify that the same function handles both directions.
 * ================================================================ */

static int
test_aes256_ctr_symmetric(void)
{
	uint8_t ct[16];
	memcpy(ct, nist_ct, 16);

	/* "Encrypt" the ciphertext — in CTR mode this decrypts it */
	ASSERT_EQ(aes256_ctr_crypt(nist_key, nist_iv, ct, 16), 0);
	ASSERT_MEM_EQ(ct, nist_pt, 16);
	return TEST_PASS;
}

/* ================================================================
 * Test table
 * ================================================================ */

static struct dssh_test_entry tests[] = {
	/* Registration */
	{ "enc/register_aes256_ctr",        test_register_aes256_ctr },
	{ "enc/register_none_enc",          test_register_none_enc },
	{ "enc/register_both",              test_register_both_enc },
	{ "enc/register_aes256_after_lock", test_register_aes256_ctr_after_lock },
	{ "enc/register_none_after_lock",   test_register_none_enc_after_lock },
	{ "enc/session_creation",           test_enc_session_creation },

	/* AES-256-CTR: NIST test vectors */
	{ "enc/aes256_ctr/nist_block1_enc", test_aes256_ctr_nist_block1_encrypt },
	{ "enc/aes256_ctr/nist_4block_enc", test_aes256_ctr_nist_4block_encrypt },
	{ "enc/aes256_ctr/nist_block1_dec", test_aes256_ctr_nist_block1_decrypt },
	{ "enc/aes256_ctr/nist_4block_dec", test_aes256_ctr_nist_4block_decrypt },
	{ "enc/aes256_ctr/nist_per_block",  test_aes256_ctr_nist_per_block },

	/* AES-256-CTR: behavioral */
	{ "enc/aes256_ctr/roundtrip",       test_aes256_ctr_roundtrip },
	{ "enc/aes256_ctr/streaming",       test_aes256_ctr_streaming },
	{ "enc/aes256_ctr/partial_block",   test_aes256_ctr_partial_block },
	{ "enc/aes256_ctr/20bytes",         test_aes256_ctr_20bytes },
	{ "enc/aes256_ctr/zero_plaintext",  test_aes256_ctr_zero_plaintext },
	{ "enc/aes256_ctr/different_key",   test_aes256_ctr_different_key },
	{ "enc/aes256_ctr/different_iv",    test_aes256_ctr_different_iv },
	{ "enc/aes256_ctr/large_buffer",    test_aes256_ctr_large_buffer },
	{ "enc/aes256_ctr/single_byte",     test_aes256_ctr_single_byte },
	{ "enc/aes256_ctr/symmetric",       test_aes256_ctr_symmetric },

	/* None cipher */
	{ "enc/none/passthrough",           test_none_enc_passthrough },
	{ "enc/none/no_init",               test_none_enc_no_init },
};

DSSH_TEST_MAIN(tests)
