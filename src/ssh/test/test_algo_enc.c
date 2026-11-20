/*
 * test_algo_enc.c -- Unit tests for DeuceSSH encryption algorithm modules.
 *
 * Tests registration of AES-256-CTR, AES-128-CBC and none ciphers, AES-256-CTR
 * correctness against NIST SP 800-38A test vectors (F.5.5/F.5.6),
 * AES-128-CBC answers (F.2.1/F.2.2), streaming and failure boundaries,
 * encrypt/decrypt roundtrips, and none cipher passthrough.
 */

#include "dssh_test.h"
#include "deucessh.h"
#include "deucessh-algorithms.h"
#include "ssh-trans.h"
#include "test/dssh_test_internal.h"
#include "dssh_test_alloc.h"

#ifdef DSSH_CRYPTO_OPENSSL
#include "dssh_test_ossl.h"
#include <openssl/evp.h>
#include <openssl/err.h>
#elif defined(DSSH_CRYPTO_BOTAN)
#include <botan/ffi.h>
#endif
#include <limits.h>
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
 * Helper: encrypt or decrypt a buffer in-place using AES-256-CTR
 * by calling the selected native provider directly.
 * CTR mode is symmetric (encrypt == decrypt).
 * Returns 0 on success.
 * ---------------------------------------------------------------- */
#ifdef DSSH_CRYPTO_OPENSSL
static int
aes256_ctr_crypt(const uint8_t *key, const uint8_t *iv,
    uint8_t *buf, size_t bufsz)
{
	if (key == NULL || iv == NULL || (buf == NULL && bufsz != 0) || bufsz > INT_MAX)
		return -1;
	int input_length = (int)bufsz;
	EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
	if (ctx == NULL)
		return -1;

	int ok = 0;
	if (EVP_EncryptInit_ex(ctx, EVP_aes_256_ctr(), NULL, key, iv) != 1)
		goto out;
	if (EVP_CIPHER_CTX_set_padding(ctx, 0) != 1)
		goto out;

	int outlen = 0;
	if (EVP_EncryptUpdate(ctx, buf, &outlen, buf, input_length) != 1
	    || outlen != input_length)
		goto out;
	ok = 1;

out:
	EVP_CIPHER_CTX_free(ctx);
	return ok ? 0 : -1;
}
#elif defined(DSSH_CRYPTO_BOTAN)
static int
aes256_ctr_crypt(const uint8_t *key, const uint8_t *iv,
    uint8_t *buf, size_t bufsz)
{
	if (key == NULL || iv == NULL || (buf == NULL && bufsz != 0))
		return -1;
	botan_cipher_t cipher = NULL;

	if (botan_cipher_init(&cipher, "AES-256/CTR-BE",
	    BOTAN_CIPHER_INIT_FLAG_ENCRYPT) != 0 || cipher == NULL)
		return -1;

	int ok = 0;
	if (botan_cipher_set_key(cipher, key, 32) != 0)
		goto out;
	if (botan_cipher_start(cipher, iv, 16) != 0)
		goto out;

	size_t outwritten = 0;
	size_t inconsumed = 0;
	if (botan_cipher_update(cipher, 0, buf, bufsz, &outwritten,
	    buf, bufsz, &inconsumed) != 0
	    || outwritten != bufsz || inconsumed != bufsz)
		goto out;
	ok = 1;

out:
	if (botan_cipher_destroy(cipher) != 0)
		ok = 0;
	return ok ? 0 : -1;
}
#endif

/* ================================================================
 * NIST SP 800-38A F.5.5 -- AES-256-CTR Encrypt
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


/* NIST SP 800-38A F.2.1/F.2.2. The plaintext is nist_pt above. */
static const uint8_t cbc_key[16] = {
	0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6,
	0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c
};

static const uint8_t cbc_iv[16] = {
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
	0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f
};

static const uint8_t cbc_ct[64] = {
	0x76, 0x49, 0xab, 0xac, 0x81, 0x19, 0xb2, 0x46,
	0xce, 0xe9, 0x8e, 0x9b, 0x12, 0xe9, 0x19, 0x7d,
	0x50, 0x86, 0xcb, 0x9b, 0x50, 0x72, 0x19, 0xee,
	0x95, 0xdb, 0x11, 0x3a, 0x91, 0x76, 0x78, 0xb2,
	0x73, 0xbe, 0xd6, 0xb8, 0xe3, 0xc1, 0x74, 0x3b,
	0x71, 0x16, 0xe6, 0x9e, 0x22, 0x22, 0x95, 0x16,
	0x3f, 0xf1, 0xca, 0xa1, 0x68, 0x1f, 0xac, 0x09,
	0x12, 0x0e, 0xca, 0x30, 0x75, 0x86, 0xe1, 0xa7
};

/* ================================================================
 * Registration tests
 * ================================================================ */

static int
test_register_aes256_ctr(void)
{
	dssh_test_reset_global_config();
	int ret = dssh_register_aes256_ctr();
	ASSERT_EQ(ret, 0);
	return TEST_PASS;
}

static int
test_register_none_enc(void)
{
	dssh_test_reset_global_config();
	int ret = dssh_register_none_enc();
	ASSERT_EQ(ret, 0);
	return TEST_PASS;
}

static int
test_register_aes256_ctr_after_lock(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_aes256_ctr(), 0);
	ASSERT_EQ(dssh_register_none_enc(), 0);
	ASSERT_EQ(dssh_register_none_comp(), 0);
	ASSERT_EQ(dssh_register_none_mac(), 0);
	ASSERT_EQ(dssh_register_curve25519_sha256(), 0);
	ASSERT_EQ(dssh_register_ssh_ed25519(), 0);

	/* Creating a session locks the algorithm registry */
	dssh_session sess = dssh_session_init(true, 0);
	ASSERT_NOT_NULL(sess);

	/* Registration must fail with DSSH_ERROR_TOOLATE */
	int ret = dssh_register_aes256_ctr();
	dssh_session_cleanup(sess);
	ASSERT_EQ(ret, DSSH_ERROR_TOOLATE);
	return TEST_PASS;
}

static int
test_register_none_enc_after_lock(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_aes256_ctr(), 0);
	ASSERT_EQ(dssh_register_none_enc(), 0);
	ASSERT_EQ(dssh_register_none_comp(), 0);
	ASSERT_EQ(dssh_register_none_mac(), 0);
	ASSERT_EQ(dssh_register_curve25519_sha256(), 0);
	ASSERT_EQ(dssh_register_ssh_ed25519(), 0);

	dssh_session sess = dssh_session_init(true, 0);
	ASSERT_NOT_NULL(sess);

	int ret = dssh_register_none_enc();
	dssh_session_cleanup(sess);
	ASSERT_EQ(ret, DSSH_ERROR_TOOLATE);
	return TEST_PASS;
}

static int
test_register_both_enc(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_aes256_ctr(), 0);
	ASSERT_EQ(dssh_register_none_enc(), 0);
	return TEST_PASS;
}

/* ================================================================
 * AES-256-CTR: NIST SP 800-38A F.5.5 -- Encrypt single block
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
 * AES-256-CTR: NIST SP 800-38A F.5.5 -- Encrypt all 4 blocks
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
 * AES-256-CTR: NIST SP 800-38A F.5.6 -- Decrypt single block
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
 * AES-256-CTR: NIST SP 800-38A F.5.6 -- Decrypt all 4 blocks
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

#ifdef DSSH_CRYPTO_OPENSSL
static int
test_aes256_ctr_streaming(void)
{
	uint8_t buf[sizeof(nist_pt)];
	memcpy(buf, nist_pt, sizeof(buf));
	EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
	int outlen = 0;
	bool valid = ctx != NULL
	    && EVP_EncryptInit_ex(ctx, EVP_aes_256_ctr(), NULL, nist_key, nist_iv) == 1
	    && EVP_CIPHER_CTX_set_padding(ctx, 0) == 1
	    && EVP_EncryptUpdate(ctx, buf, &outlen, buf, 32) == 1 && outlen == 32
	    && EVP_EncryptUpdate(ctx, buf + 32, &outlen, buf + 32, 32) == 1 && outlen == 32
	    && memcmp(buf, nist_ct, sizeof(buf)) == 0;
	EVP_CIPHER_CTX_free(ctx);
	ASSERT_TRUE(valid);
	return TEST_PASS;
}
#elif defined(DSSH_CRYPTO_BOTAN)
static int
test_aes256_ctr_streaming(void)
{
	uint8_t buf[sizeof(nist_pt)];
	memcpy(buf, nist_pt, sizeof(buf));
	botan_cipher_t cipher = NULL;
	int ret = botan_cipher_init(&cipher, "AES-256/CTR-BE", BOTAN_CIPHER_INIT_FLAG_ENCRYPT);
	bool created = ret == 0 && cipher != NULL;
	size_t outwritten = 0, inconsumed = 0;
	bool valid = created && botan_cipher_set_key(cipher, nist_key, sizeof(nist_key)) == 0
	    && botan_cipher_start(cipher, nist_iv, sizeof(nist_iv)) == 0
	    && botan_cipher_update(cipher, 0, buf, 32, &outwritten, buf, 32, &inconsumed) == 0
	    && outwritten == 32 && inconsumed == 32
	    && botan_cipher_update(cipher, 0, buf + 32, 32, &outwritten, buf + 32, 32, &inconsumed) == 0
	    && outwritten == 32 && inconsumed == 32
	    && memcmp(buf, nist_ct, sizeof(buf)) == 0;
	if (created) {
		int released = botan_cipher_destroy(cipher);
		valid = released == 0 && valid;
	}
	ASSERT_TRUE(valid);
	return TEST_PASS;
}
#endif

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

/* Use the registered module callbacks as well as the independent backend
 * helpers above. Separate directional contexts are advanced in interleaved
 * calls, including the first-block/remainder split used by SSH receive. */
static int
test_aes256_ctr_module_vectors(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_aes256_ctr(), 0);
	dssh_enc enc = gconf.enc_head;
	ASSERT_NOT_NULL(enc);
	ASSERT_NOT_NULL(enc->init);
	ASSERT_NOT_NULL(enc->encrypt);
	ASSERT_NOT_NULL(enc->decrypt);
	ASSERT_NOT_NULL(enc->cleanup);
	ASSERT_STR_EQ(enc->name, "aes256-ctr");
	ASSERT_EQ_U(enc->blocksize, 16);
	ASSERT_EQ_U(enc->key_size, 32);
	ASSERT_EQ_U(enc->bytes_per_key, UINT64_C(1) << 36);
	const size_t cuts[] = { 16, 32, 48, 64 };
	bool valid = true;
	for (size_t i = 0; i < sizeof(cuts) / sizeof(cuts[0]); i++) {
		dssh_enc_ctx *tx = NULL;
		dssh_enc_ctx *rx = NULL;
		uint8_t encrypted[sizeof(nist_pt)];
		uint8_t decrypted[sizeof(nist_ct)];
		memcpy(encrypted, nist_pt, sizeof(encrypted));
		memcpy(decrypted, nist_ct, sizeof(decrypted));
		int ret = enc->init(nist_key, nist_iv, true, &tx);
		if (ret == 0)
			ret = enc->init(nist_key, nist_iv, false, &rx);
		if (ret == 0 && (tx == NULL || rx == NULL || tx == rx))
			ret = DSSH_ERROR_INIT;
		if (ret == 0)
			ret = enc->encrypt(encrypted, cuts[i], tx);
		if (ret == 0)
			ret = enc->decrypt(decrypted, cuts[i], rx);
		if (ret == 0 && cuts[i] < sizeof(encrypted))
			ret = enc->encrypt(&encrypted[cuts[i]], sizeof(encrypted) - cuts[i], tx);
		if (ret == 0 && cuts[i] < sizeof(decrypted))
			ret = enc->decrypt(&decrypted[cuts[i]], sizeof(decrypted) - cuts[i], rx);
		valid = valid && ret == 0
		    && memcmp(encrypted, nist_ct, sizeof(encrypted)) == 0
		    && memcmp(decrypted, nist_pt, sizeof(decrypted)) == 0;
		if (rx != NULL && rx != tx)
			enc->cleanup(rx);
		if (tx != NULL)
			enc->cleanup(tx);
	}
	dssh_test_reset_global_config();
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

#ifdef DSSH_CRYPTO_OPENSSL
static int
test_cipher_module_extents(int (*register_cipher)(void), const uint8_t *key,
    const uint8_t *iv, const uint8_t *ciphertext)
{
	const int reported[] = { INT_MIN, -1, 0, 15, 16, 17, INT_MAX,
#if INT_MAX > UINT16_MAX
	    UINT16_MAX + 17,
#endif
	};
	dssh_test_reset_global_config();
	ASSERT_EQ(register_cipher(), 0);
	dssh_enc enc = gconf.enc_head;
	bool valid = enc != NULL && enc->init != NULL && enc->encrypt != NULL
	    && enc->decrypt != NULL && enc->cleanup != NULL;
	for (unsigned direction = 0; direction < 2 && valid; direction++) {
		for (size_t i = 0; i < sizeof(reported) / sizeof(reported[0]) && valid; i++) {
			dssh_test_ossl_reset();
			dssh_enc_ctx *ctx = NULL;
			int ret = enc->init(key, iv, direction == 0, &ctx);
			if (ret != 0 || ctx == NULL) {
				valid = false;
				if (ctx != NULL)
					enc->cleanup(ctx);
				break;
			}
			uint8_t guarded[18];
			memset(guarded, 0xA5, sizeof(guarded));
			memcpy(&guarded[1], direction == 0 ? nist_pt : ciphertext, 16);
			dssh_test_ossl_encrypt_length(reported[i]);
			ret = direction == 0 ? enc->encrypt(&guarded[1], 16, ctx)
			    : enc->decrypt(&guarded[1], 16, ctx);
			int native_length = 0;
			bool observed = dssh_test_ossl_encrypt_observed(&native_length);
			dssh_test_ossl_reset();
			valid = observed && native_length == 16
			    && ret == (reported[i] == 16 ? 0 : DSSH_ERROR_INIT)
			    && guarded[0] == 0xA5 && guarded[17] == 0xA5
			    && memcmp(&guarded[1], direction == 0 ? ciphertext : nist_pt, 16) == 0;
			enc->cleanup(ctx);
		}
	}
	dssh_test_ossl_reset();
	dssh_test_reset_global_config();
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

static int
test_aes256_ctr_module_extents(void)
{
	return test_cipher_module_extents(dssh_register_aes256_ctr, nist_key, nist_iv, nist_ct);
}

static int
test_aes128_cbc_module_extents(void)
{
	return test_cipher_module_extents(dssh_register_aes128_cbc, cbc_key, cbc_iv, cbc_ct);
}
#endif

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
 * return 0 without modifying the buffer. Exercise actual registered callbacks
 * with guarded nonempty input and empty operations.
 * ================================================================ */

static int
test_none_enc_passthrough(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_none_enc(), 0);
	dssh_enc enc = gconf.enc_head;
	ASSERT_NOT_NULL(enc);
	ASSERT_NOT_NULL(enc->encrypt);
	ASSERT_NOT_NULL(enc->decrypt);
	ASSERT_NOT_NULL(enc->cleanup);
	ASSERT_STR_EQ(enc->name, "none");
	ASSERT_EQ_U(enc->bytes_per_key, UINT64_MAX);
	ASSERT_EQ(enc->flags, 0);
	ASSERT_NULL(enc->init);
	uint8_t frame[sizeof(nist_pt) + 2];
	memset(frame, 0xa5, sizeof(frame));
	memcpy(frame + 1, nist_pt, sizeof(nist_pt));
	bool valid = enc->encrypt(frame + 1, sizeof(nist_pt), NULL) == 0
	    && enc->decrypt(frame + 1, sizeof(nist_pt), NULL) == 0
	    && enc->encrypt(NULL, 0, NULL) == 0 && enc->decrypt(NULL, 0, NULL) == 0
	    && memcmp(frame + 1, nist_pt, sizeof(nist_pt)) == 0
	    && frame[0] == 0xa5 && frame[sizeof(frame) - 1] == 0xa5;
	enc->cleanup(NULL);
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

/* ================================================================
 * None cipher: no init function (init is NULL)
 * ================================================================ */

static int
test_none_enc_no_init(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_none_enc(), 0);
	dssh_enc enc = gconf.enc_head;
	ASSERT_NOT_NULL(enc);
	ASSERT_NOT_NULL(enc->encrypt);
	ASSERT_NOT_NULL(enc->decrypt);
	ASSERT_NOT_NULL(enc->cleanup);
	ASSERT_NULL(enc->init);
	ASSERT_EQ(enc->blocksize, 1);
	ASSERT_EQ(enc->key_size, 0);
	return TEST_PASS;
}

/* ================================================================
 * Verify session creation with enc algorithms registered
 * ================================================================ */

static int
test_enc_session_creation(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_aes256_ctr(), 0);
	ASSERT_EQ(dssh_register_none_enc(), 0);
	ASSERT_EQ(dssh_register_none_comp(), 0);
	ASSERT_EQ(dssh_register_none_mac(), 0);
	ASSERT_EQ(dssh_register_curve25519_sha256(), 0);
	ASSERT_EQ(dssh_register_ssh_ed25519(), 0);

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

	/* "Encrypt" the ciphertext -- in CTR mode this decrypts it */
	ASSERT_EQ(aes256_ctr_crypt(nist_key, nist_iv, ct, 16), 0);
	ASSERT_MEM_EQ(ct, nist_pt, 16);
	return TEST_PASS;
}

/* NIST SP 800-38A F.2.1/F.2.2, through the registered callbacks. */
static int
test_aes128_cbc_module_vectors(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_aes128_cbc(), 0);
	dssh_enc enc = gconf.enc_head;
	ASSERT_NOT_NULL(enc);
	ASSERT_NOT_NULL(enc->init);
	ASSERT_NOT_NULL(enc->encrypt);
	ASSERT_NOT_NULL(enc->decrypt);
	ASSERT_NOT_NULL(enc->cleanup);
	ASSERT_STR_EQ(enc->name, "aes128-cbc");
	ASSERT_EQ_U(enc->blocksize, 16);
	ASSERT_EQ_U(enc->key_size, 16);
	ASSERT_EQ_U(enc->bytes_per_key, UINT64_C(1) << 36);
	bool valid = true;
	/* Each bit selects a cut after one of the first three blocks. */
	for (unsigned cuts = 0; cuts < 8; cuts++) {
		dssh_enc_ctx *tx = NULL;
		dssh_enc_ctx *rx = NULL;
		uint8_t encrypted[sizeof(nist_pt)];
		uint8_t decrypted[sizeof(cbc_ct)];
		memcpy(encrypted, nist_pt, sizeof(encrypted));
		memcpy(decrypted, cbc_ct, sizeof(decrypted));
		int ret = enc->init(cbc_key, cbc_iv, true, &tx);
		if (ret == 0)
			ret = enc->init(cbc_key, cbc_iv, false, &rx);
		if (ret == 0 && (tx == NULL || rx == NULL || tx == rx))
			ret = DSSH_ERROR_INIT;
		size_t start = 0;
		for (unsigned block = 0; block < 4 && ret == 0; block++) {
			if (block != 3 && (cuts & (1U << block)) == 0)
				continue;
			size_t end = (block + 1) * 16;
			ret = enc->encrypt(&encrypted[start], end - start, tx);
			if (ret == 0)
				ret = enc->decrypt(&decrypted[start], end - start, rx);
			start = end;
		}
		valid = valid && ret == 0
		    && memcmp(encrypted, cbc_ct, sizeof(encrypted)) == 0
		    && memcmp(decrypted, nist_pt, sizeof(decrypted)) == 0;
		if (rx != NULL && rx != tx)
			enc->cleanup(rx);
		if (tx != NULL)
			enc->cleanup(tx);
	}
	dssh_test_reset_global_config();
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

static int
test_aes128_cbc_inputs(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_aes128_cbc(), 0);
	dssh_enc enc = gconf.enc_head;
	ASSERT_NOT_NULL(enc);
	ASSERT_NOT_NULL(enc->init);
	ASSERT_NOT_NULL(enc->encrypt);
	ASSERT_NOT_NULL(enc->decrypt);
	ASSERT_NOT_NULL(enc->cleanup);
	const size_t invalid[] = {1, 15, 17, 63,
#if SIZE_MAX > INT_MAX && defined(DSSH_CRYPTO_OPENSSL)
	    (size_t)INT_MAX + 1,
#endif
	};
	bool valid = enc->encrypt(NULL, 0, NULL) == DSSH_ERROR_INIT
	    && enc->decrypt(NULL, 0, NULL) == DSSH_ERROR_INIT;
	for (unsigned direction = 0; direction < 2; direction++) {
		dssh_enc_ctx *ctx = NULL;
		int ret = enc->init(cbc_key, cbc_iv, direction == 0, &ctx);
		uint8_t buffer[sizeof(nist_pt) + 2];
		memset(buffer, 0xa5, sizeof(buffer));
		const uint8_t *input = direction == 0 ? nist_pt : cbc_ct;
		const uint8_t *expected = direction == 0 ? cbc_ct : nist_pt;
		memcpy(&buffer[1], input, sizeof(nist_pt));
		if (ret == 0 && ctx == NULL)
			ret = DSSH_ERROR_INIT;
		if (ret == 0) {
			for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++) {
				ret = direction == 0 ? enc->encrypt(&buffer[1], invalid[i], ctx)
				    : enc->decrypt(&buffer[1], invalid[i], ctx);
				valid = valid && ret == DSSH_ERROR_INVALID
				    && memcmp(&buffer[1], input, sizeof(nist_pt)) == 0;
			}
			ret = direction == 0 ? enc->encrypt(&buffer[1], 0, ctx)
			    : enc->decrypt(&buffer[1], 0, ctx);
			valid = valid && ret == 0
			    && memcmp(&buffer[1], input, sizeof(nist_pt)) == 0;
			ret = direction == 0 ? enc->encrypt(&buffer[1], sizeof(nist_pt), ctx)
			    : enc->decrypt(&buffer[1], sizeof(nist_pt), ctx);
		}
		valid = valid && ret == 0 && buffer[0] == 0xa5
		    && buffer[sizeof(buffer) - 1] == 0xa5
		    && memcmp(&buffer[1], expected, sizeof(nist_pt)) == 0;
		enc->cleanup(ctx);
	}
	dssh_test_reset_global_config();
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

static int
test_aes128_cbc_registration(void)
{
	dssh_test_reset_global_config();
	dssh_test_alloc_fail_after(0);
	int ret = dssh_register_aes128_cbc();
	dssh_test_alloc_reset();
	ASSERT_EQ(ret, DSSH_ERROR_ALLOC);
	ASSERT_NULL(gconf.enc_head);
	ASSERT_EQ(dssh_register_aes128_cbc(), 0);
	ASSERT_EQ(dssh_register_none_comp(), 0);
	ASSERT_EQ(dssh_register_none_mac(), 0);
	ASSERT_EQ(dssh_register_curve25519_sha256(), 0);
	ASSERT_EQ(dssh_register_ssh_ed25519(), 0);
	dssh_session sess = dssh_session_init(true, 0);
	ASSERT_NOT_NULL(sess);
	ret = dssh_register_aes128_cbc();
	dssh_session_cleanup(sess);
	bool single = gconf.enc_head != NULL && gconf.enc_head->next == NULL;
	dssh_test_reset_global_config();
	ASSERT_EQ(ret, DSSH_ERROR_TOOLATE);
	ASSERT_TRUE(single);
	return TEST_PASS;
}

#ifdef DSSH_CRYPTO_OPENSSL
static int
test_aes128_cbc_provider_failures(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_register_aes128_cbc(), 0);
	dssh_enc enc = gconf.enc_head;
	ASSERT_NOT_NULL(enc);
	ASSERT_NOT_NULL(enc->init);
	ASSERT_NOT_NULL(enc->encrypt);
	ASSERT_NOT_NULL(enc->decrypt);
	ASSERT_NOT_NULL(enc->cleanup);
	bool valid = true;
	for (unsigned direction = 0; direction < 2; direction++) {
		for (int fail = 0; fail < 4; fail++) {
			dssh_enc_ctx *ctx = NULL;
			if (fail == 0)
				dssh_test_alloc_fail_after(0);
			else
				dssh_test_ossl_fail_after(fail - 1);
			int ret = enc->init(cbc_key, cbc_iv, direction == 0, &ctx);
			int calls = dssh_test_ossl_count();
			dssh_test_alloc_reset();
			dssh_test_ossl_reset();
			valid = valid && ctx == NULL
			    && ret == (fail < 2 ? DSSH_ERROR_ALLOC : DSSH_ERROR_INIT)
			    && (fail == 0 || calls == fail);
			enc->cleanup(ctx);
		}
		dssh_enc_ctx *ctx = NULL;
		int ret = enc->init(cbc_key, cbc_iv, direction == 0, &ctx);
		uint8_t buffer[16];
		memcpy(buffer, direction == 0 ? nist_pt : cbc_ct, sizeof(buffer));
		valid = ret == 0 && ctx != NULL && valid;
		if (ret == 0 && ctx != NULL) {
			dssh_test_ossl_fail_after(0);
			ret = direction == 0 ? enc->encrypt(buffer, sizeof(buffer), ctx)
			    : enc->decrypt(buffer, sizeof(buffer), ctx);
			valid = valid && dssh_test_ossl_count() == 1;
		}
		dssh_test_ossl_reset();
		valid = valid && ret == DSSH_ERROR_INIT
		    && memcmp(buffer, direction == 0 ? nist_pt : cbc_ct, sizeof(buffer)) == 0;
		enc->cleanup(ctx);
	}
	/* Exercise a real cipher-fetch refusal after the native context exists. */
	int configured = EVP_set_default_properties(NULL, "provider=deucessh_missing_provider");
	if (configured == 1) {
		for (unsigned direction = 0; direction < 2; direction++) {
			dssh_enc_ctx *ctx = NULL;
			int ret = enc->init(cbc_key, cbc_iv, direction == 0, &ctx);
			valid = valid && ret == DSSH_ERROR_INIT && ctx == NULL;
			enc->cleanup(ctx);
		}
	}
	int restored = EVP_set_default_properties(NULL, "");
	ERR_clear_error();
	dssh_test_reset_global_config();
	ASSERT_EQ(configured, 1);
	ASSERT_EQ(restored, 1);
	ASSERT_TRUE(valid);
	return TEST_PASS;
}
#endif

/* ================================================================
 * Test table
 * ================================================================ */

static struct dssh_test_entry tests[] = {
	/* Registration */
	{ "enc/dssh_register_aes256_ctr",        test_register_aes256_ctr },
	{ "enc/dssh_register_none_enc",          test_register_none_enc },
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
	{ "enc/aes256_ctr/module_vectors",  test_aes256_ctr_module_vectors },
#ifdef DSSH_CRYPTO_OPENSSL
	{ "enc/aes256_ctr/module_extents",  test_aes256_ctr_module_extents },
#endif

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

	/* Registered AES-128-CBC callbacks and their failure boundaries. */
	{ "enc/aes128_cbc/module_vectors", test_aes128_cbc_module_vectors },
	{ "enc/aes128_cbc/inputs", test_aes128_cbc_inputs },
	{ "enc/aes128_cbc/registration", test_aes128_cbc_registration },
#ifdef DSSH_CRYPTO_OPENSSL
	{ "enc/aes128_cbc/provider_failures", test_aes128_cbc_provider_failures },
	{ "enc/aes128_cbc/module_extents", test_aes128_cbc_module_extents },
#endif

	/* None cipher */
	{ "enc/none/passthrough",           test_none_enc_passthrough },
	{ "enc/none/no_init",               test_none_enc_no_init },
};

/* All native/session/module owners are retired before assertions. The remaining
 * shared registry and inactive injection state can be reset by the live hook. */
void
dssh_test_failure_cleanup(void)
{
	dssh_test_alloc_reset();
#ifdef DSSH_CRYPTO_OPENSSL
	dssh_test_ossl_reset();
#endif
	dssh_test_reset_global_config();
}
DSSH_TEST_MAIN(tests)
