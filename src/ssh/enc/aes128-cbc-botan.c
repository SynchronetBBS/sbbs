/*
 * enc/aes128-cbc-botan.c -- Registration wrapper for AES-128-CBC (Botan3).
 *
 * Thin C file that wires the C++ crypto functions into the dssh_enc_s
 * struct and registers the algorithm.
 */

#include <stdlib.h>
#include <string.h>

#include "deucessh-enc.h"
#include "deucessh.h"

#define AES128_CBC_BLOCK_SIZE 16
#define AES128_CBC_KEY_SIZE   16
#define AES128_CBC_NAME       "aes128-cbc"

/* Implemented in aes128-cbc-botan.cpp */
extern int  dssh_botan_aes128cbc_init(const uint8_t *key, const uint8_t *iv, bool encrypt_dir, dssh_enc_ctx **ctx);
extern int  dssh_botan_aes128cbc_crypt(uint8_t *buf, size_t bufsz, dssh_enc_ctx *ctx);
extern void dssh_botan_aes128cbc_cleanup(dssh_enc_ctx *ctx);

DSSH_PUBLIC int
dssh_register_aes128_cbc(void)
{
	static const char  name[] = AES128_CBC_NAME;
	struct dssh_enc_s *enc    = malloc(sizeof(*enc) + sizeof(name));

	if (enc == NULL)
		return DSSH_ERROR_ALLOC;
	enc->next      = NULL;
	enc->init      = dssh_botan_aes128cbc_init;
	enc->encrypt   = dssh_botan_aes128cbc_crypt;
	enc->decrypt   = dssh_botan_aes128cbc_crypt;
	enc->cleanup   = dssh_botan_aes128cbc_cleanup;
	enc->flags     = 0;
	enc->blocksize = AES128_CBC_BLOCK_SIZE;
	enc->key_size  = AES128_CBC_KEY_SIZE;
	memcpy(enc->name, name, sizeof(name));
	return dssh_transport_register_enc(enc);
}
