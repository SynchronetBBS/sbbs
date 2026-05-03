#include <openssl/evp.h>

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "deucessh-enc.h"
#include "deucessh.h"
#ifdef DSSH_TESTING
 #include "ssh-internal.h"
#endif

#define AES128_CBC_BLOCK_SIZE 16
#define AES128_CBC_KEY_SIZE   16
#define AES128_CBC_NAME       "aes128-cbc"

struct dssh_enc_ctx {
	EVP_CIPHER_CTX *ctx;
};

static int
init_ctx(const uint8_t *key, const uint8_t *iv, bool encrypt_dir, dssh_enc_ctx **ctx)
{
	struct dssh_enc_ctx *cbd = malloc(sizeof(*cbd));

	if (cbd == NULL)
		return DSSH_ERROR_ALLOC;

	cbd->ctx = EVP_CIPHER_CTX_new();
	if (cbd->ctx == NULL) {
		free(cbd);
		return DSSH_ERROR_ALLOC;
	}

	EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, "AES-128-CBC", NULL);

	if (cipher == NULL) {
		EVP_CIPHER_CTX_free(cbd->ctx);
		free(cbd);
		return DSSH_ERROR_INIT;
	}
	int enc_int = encrypt_dir ? 1 : 0;
	int ok      = (EVP_CipherInit_ex(cbd->ctx, cipher, NULL, key, iv, enc_int) == 1)
	         && (EVP_CIPHER_CTX_set_padding(cbd->ctx, 0) == 1);

	EVP_CIPHER_free(cipher);
	if (!ok) {
		EVP_CIPHER_CTX_free(cbd->ctx);
		free(cbd);
		return DSSH_ERROR_INIT;
	}

	*ctx = cbd;
	return 0;
}

static int
do_crypt(uint8_t *buf, size_t bufsz, dssh_enc_ctx *ctx)
{
	struct dssh_enc_ctx *cbd = ctx;

	if ((cbd == NULL) || (cbd->ctx == NULL))
		return DSSH_ERROR_INIT;
	if (bufsz % AES128_CBC_BLOCK_SIZE != 0)
		return DSSH_ERROR_INVALID;
	if (bufsz > INT_MAX)
		return DSSH_ERROR_INVALID;

	int    inl    = (int)bufsz;
	int    outlen = 0;

	if (EVP_CipherUpdate(cbd->ctx, buf, &outlen, buf, inl) != 1)
		return DSSH_ERROR_INIT;
	if (outlen != inl)
		return DSSH_ERROR_INIT;

	return 0;
}

static void
cleanup(dssh_enc_ctx *ctx)
{
	struct dssh_enc_ctx *cbd = ctx;

	if (cbd != NULL) {
		EVP_CIPHER_CTX_free(cbd->ctx);
		free(cbd);
	}
}

DSSH_PUBLIC int
dssh_register_aes128_cbc(void)
{
	static const char  name[] = AES128_CBC_NAME;
	struct dssh_enc_s *enc    = malloc(sizeof(*enc) + sizeof(name));

	if (enc == NULL)
		return DSSH_ERROR_ALLOC;
	enc->next      = NULL;
	enc->init      = init_ctx;
	enc->encrypt   = do_crypt;
	enc->decrypt   = do_crypt;
	enc->cleanup   = cleanup;
	enc->flags     = 0;
	enc->blocksize = AES128_CBC_BLOCK_SIZE;
	enc->key_size  = AES128_CBC_KEY_SIZE;
	memcpy(enc->name, name, sizeof(name));
	return dssh_transport_register_enc(enc);
}
