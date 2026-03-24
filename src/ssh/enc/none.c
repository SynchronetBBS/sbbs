#include <stdlib.h>
#include <string.h>

#include "ssh-trans.h"
#include "ssh-internal.h"

static int
encrypt(uint8_t *buf, size_t bufsz, dssh_enc_ctx *ctx)
{
	return 0;
}

static int
decrypt(uint8_t *buf, size_t bufsz, dssh_enc_ctx *ctx)
{
	return 0;
}

static void
cleanup(dssh_enc_ctx *ctx)
{
}

DSSH_PUBLIC int
register_none_enc(void)
{
	static const char name[] = "none";
	struct dssh_enc_s *enc = malloc(sizeof(*enc) + sizeof(name));
	if (enc == NULL)
		return DSSH_ERROR_ALLOC;
	enc->next = NULL;
	enc->init = NULL;
	enc->encrypt = encrypt;
	enc->decrypt = decrypt;
	enc->cleanup = cleanup;
	enc->flags = 0;
	enc->blocksize = 1;
	enc->key_size = 0;
	memcpy(enc->name, name, sizeof(name));
	return dssh_transport_register_enc(enc);
}
