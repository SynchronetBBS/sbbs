/*
 * enc/aes256-ctr-botan.c -- Registration wrapper for AES-256-CTR (Botan3).
 *
 * Thin C file that wires the C++ crypto functions into the dssh_enc_s
 * struct and registers the algorithm.
 */

#include <stdlib.h>
#include <string.h>

#include "deucessh-enc.h"
#include "deucessh.h"

#define AES256_CTR_BLOCK_SIZE 16
#define AES256_CTR_KEY_SIZE   32
#define AES256_CTR_NAME       "aes256-ctr"
#define AES256_CTR_NAME_LEN   10

/* Implemented in aes256-ctr-botan.cpp */
extern int  dssh_botan_aes256ctr_init(const uint8_t *key, const uint8_t *iv, bool encrypt_dir, dssh_enc_ctx **ctx);
extern int  dssh_botan_aes256ctr_crypt(uint8_t *buf, size_t bufsz, dssh_enc_ctx *ctx);
extern void dssh_botan_aes256ctr_cleanup(dssh_enc_ctx *ctx);

DSSH_PUBLIC int
dssh_register_aes256_ctr(void)
{
	static const char  name[] = AES256_CTR_NAME;
	struct dssh_enc_s *enc    = malloc(sizeof(*enc) + sizeof(name));

	if (enc == NULL)
		return DSSH_ERROR_ALLOC;
	enc->next      = NULL;
	enc->init      = dssh_botan_aes256ctr_init;
	enc->encrypt   = dssh_botan_aes256ctr_crypt;
	enc->decrypt   = dssh_botan_aes256ctr_crypt;
	enc->cleanup   = dssh_botan_aes256ctr_cleanup;
	enc->flags     = 0;
	enc->blocksize = AES256_CTR_BLOCK_SIZE;
	enc->key_size  = AES256_CTR_KEY_SIZE;
	memcpy(enc->name, name, sizeof(name));
	return dssh_transport_register_enc(enc);
}
