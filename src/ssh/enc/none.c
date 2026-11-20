#include <stdlib.h>
#include <string.h>

#include "deucessh-algorithms.h"
#include "deucessh-enc.h"
#include "deucessh.h"
#ifdef DSSH_TESTING
 #include "ssh-internal.h"
#endif

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
dssh_register_none_enc(void)
{
	static const char  name[] = "none";
	struct dssh_enc_s *enc    = malloc(sizeof(*enc) + sizeof(name));

	if (enc == NULL)
		return DSSH_ERROR_ALLOC;
	enc->next          = NULL;
	enc->init          = NULL;
	enc->encrypt       = encrypt;
	enc->decrypt       = decrypt;
	enc->cleanup       = cleanup;
	enc->bytes_per_key = UINT64_MAX; /* no encryption => no key wear */
	enc->flags         = 0;
	enc->blocksize     = 1;
	enc->key_size      = 0;
	memcpy(enc->name, name, sizeof(name));
	int ret = dssh_transport_register_enc(enc);
	if (ret < 0)
		free(enc);
	return ret;
}
