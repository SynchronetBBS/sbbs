/*
 * test_enc.c -- Test encryption module for DeuceSSH.
 *
 * A minimal block cipher that XORs with a repeating key.  Provides
 * controllable failure injection: init, encrypt, and decrypt can
 * each be set to fail on the Nth call.
 *
 * Registered as "aes256-ctr" so it participates in normal algorithm
 * negotiation.  Only linked into test builds.
 */

#include <stdlib.h>
#include <string.h>

#include "deucessh.h"
#include "ssh-trans.h"
#include "test_enc.h"

#define TEST_ENC_BLOCK_SIZE 16
#define TEST_ENC_KEY_SIZE   32

/* Global failure injection counters */
static int init_fail_at = -1;
static int init_count = 0;
static int encrypt_fail_at = -1;
static int encrypt_count = 0;
static int decrypt_fail_at = -1;
static int decrypt_count = 0;

void
test_enc_reset(void)
{
	init_fail_at = -1;
	init_count = 0;
	encrypt_fail_at = -1;
	encrypt_count = 0;
	decrypt_fail_at = -1;
	decrypt_count = 0;
}

void test_enc_fail_init_at(int n)    { init_fail_at = n; init_count = 0; }
void test_enc_fail_encrypt_at(int n) { encrypt_fail_at = n; encrypt_count = 0; }
void test_enc_fail_decrypt_at(int n) { decrypt_fail_at = n; decrypt_count = 0; }

struct dssh_enc_ctx {
	uint8_t key[TEST_ENC_KEY_SIZE];
};

static int
test_enc_init(const uint8_t *key, const uint8_t *iv,
    bool encrypt_dir, dssh_enc_ctx **ctx)
{
	(void)iv;
	(void)encrypt_dir;

	if (init_fail_at >= 0 && init_count++ == init_fail_at)
		return DSSH_ERROR_INIT;

	struct dssh_enc_ctx *cbd = malloc(sizeof(*cbd));
	if (cbd == NULL)
		return DSSH_ERROR_ALLOC;
	memcpy(cbd->key, key, TEST_ENC_KEY_SIZE);
	*ctx = cbd;
	return 0;
}

static int
test_enc_encrypt(uint8_t *buf, size_t bufsz, dssh_enc_ctx *ctx)
{
	if (encrypt_fail_at >= 0 && encrypt_count++ == encrypt_fail_at)
		return DSSH_ERROR_INIT;

	struct dssh_enc_ctx *cbd = ctx;
	for (size_t i = 0; i < bufsz; i++)
		buf[i] ^= cbd->key[i % TEST_ENC_KEY_SIZE];
	return 0;
}

static int
test_enc_decrypt(uint8_t *buf, size_t bufsz, dssh_enc_ctx *ctx)
{
	if (decrypt_fail_at >= 0 && decrypt_count++ == decrypt_fail_at)
		return DSSH_ERROR_INIT;

	/* XOR cipher: decrypt == encrypt */
	struct dssh_enc_ctx *cbd = ctx;
	for (size_t i = 0; i < bufsz; i++)
		buf[i] ^= cbd->key[i % TEST_ENC_KEY_SIZE];
	return 0;
}

static void
test_enc_cleanup(dssh_enc_ctx *ctx)
{
	free(ctx);
}

int
register_test_enc(void)
{
	static const char name[] = "aes256-ctr";
	struct dssh_enc_s *enc = malloc(sizeof(*enc) + sizeof(name));
	if (enc == NULL)
		return DSSH_ERROR_ALLOC;
	enc->next = NULL;
	enc->init = test_enc_init;
	enc->encrypt = test_enc_encrypt;
	enc->decrypt = test_enc_decrypt;
	enc->cleanup = test_enc_cleanup;
	enc->flags = 0;
	enc->blocksize = TEST_ENC_BLOCK_SIZE;
	enc->key_size = TEST_ENC_KEY_SIZE;
	memcpy(enc->name, name, sizeof(name));
	return dssh_transport_register_enc(enc);
}
