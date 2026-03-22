#include <openssl/evp.h>
#include <stdlib.h>
#include <string.h>

#include "deucessh.h"

#define AES256_CTR_BLOCK_SIZE 16
#define AES256_CTR_KEY_SIZE   32
#define AES256_CTR_NAME       "aes256-ctr"
#define AES256_CTR_NAME_LEN   10

struct deuce_ssh_enc_ctx {
	EVP_CIPHER_CTX *ctx;
};

static int
init_ctx(const uint8_t *key, const uint8_t *iv, bool encrypt_dir, deuce_ssh_enc_ctx **ctx)
{
	struct deuce_ssh_enc_ctx *cbd = malloc(sizeof(*cbd));
	if (cbd == NULL)
		return DEUCE_SSH_ERROR_ALLOC;

	cbd->ctx = EVP_CIPHER_CTX_new();
	if (cbd->ctx == NULL) {
		free(cbd);
		return DEUCE_SSH_ERROR_ALLOC;
	}

	(void)encrypt_dir;
	if (EVP_EncryptInit_ex(cbd->ctx, EVP_aes_256_ctr(), NULL, key, iv) != 1 ||
	    EVP_CIPHER_CTX_set_padding(cbd->ctx, 0) != 1) {
		EVP_CIPHER_CTX_free(cbd->ctx);
		free(cbd);
		return DEUCE_SSH_ERROR_INIT;
	}

	*ctx = cbd;
	return 0;
}

static int
do_crypt(uint8_t *buf, size_t bufsz, deuce_ssh_enc_ctx *ctx)
{
	struct deuce_ssh_enc_ctx *cbd = ctx;
	if (cbd == NULL || cbd->ctx == NULL)
		return DEUCE_SSH_ERROR_INIT;

	int outlen = 0;
	if (EVP_EncryptUpdate(cbd->ctx, buf, &outlen, buf, (int)bufsz) != 1)
		return DEUCE_SSH_ERROR_INIT;

	return 0;
}

static void
cleanup(deuce_ssh_enc_ctx *ctx)
{
	struct deuce_ssh_enc_ctx *cbd = ctx;
	if (cbd != NULL) {
		EVP_CIPHER_CTX_free(cbd->ctx);
		free(cbd);
	}
}

int
register_aes256_ctr(void)
{
	static const char name[] = AES256_CTR_NAME;
	struct deuce_ssh_enc_s *enc = malloc(sizeof(*enc) + sizeof(name));
	if (enc == NULL)
		return DEUCE_SSH_ERROR_ALLOC;
	enc->next = NULL;
	enc->init = init_ctx;
	enc->encrypt = do_crypt;
	enc->decrypt = do_crypt;
	enc->cleanup = cleanup;
	enc->flags = 0;
	enc->blocksize = AES256_CTR_BLOCK_SIZE;
	enc->key_size = AES256_CTR_KEY_SIZE;
	memcpy(enc->name, name, sizeof(name));
	return deuce_ssh_transport_register_enc(enc);
}
