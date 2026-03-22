#include <stdlib.h>
#include <string.h>

#include "deucessh.h"

static int
encrypt(uint8_t *buf, size_t bufsz, deuce_ssh_enc_ctx *ctx)
{
	return 0;
}

static int
decrypt(uint8_t *buf, size_t bufsz, deuce_ssh_enc_ctx *ctx)
{
	return 0;
}

static void
cleanup(deuce_ssh_enc_ctx *ctx)
{
}

DEUCE_SSH_PUBLIC int
register_none_enc(void)
{
	static const char name[] = "none";
	struct deuce_ssh_enc_s *enc = malloc(sizeof(*enc) + sizeof(name));
	if (enc == NULL)
		return DEUCE_SSH_ERROR_ALLOC;
	enc->next = NULL;
	enc->init = NULL;
	enc->encrypt = encrypt;
	enc->decrypt = decrypt;
	enc->cleanup = cleanup;
	enc->flags = 0;
	enc->blocksize = 1;
	enc->key_size = 0;
	memcpy(enc->name, name, sizeof(name));
	return deuce_ssh_transport_register_enc(enc);
}
