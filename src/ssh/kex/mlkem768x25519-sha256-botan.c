/*
 * kex/mlkem768x25519-sha256-botan.c -- Registration (Botan3).
 */

#include <stdlib.h>
#include <string.h>

#include "deucessh-kex.h"
#include "deucessh.h"

#define KEX_NAME     "mlkem768x25519-sha256"
#define KEX_NAME_LEN 21

extern int  dssh_botan_mlkem768x25519_handler(struct dssh_kex_context *kctx);
extern void dssh_botan_mlkem768x25519_cleanup(void *kex_data);

DSSH_PUBLIC int
dssh_register_mlkem768x25519_sha256(void)
{
	struct dssh_kex_s *kex = malloc(sizeof(*kex) + KEX_NAME_LEN + 1);

	if (kex == NULL)
		return DSSH_ERROR_ALLOC;
	kex->next      = NULL;
	kex->handler   = dssh_botan_mlkem768x25519_handler;
	kex->cleanup   = dssh_botan_mlkem768x25519_cleanup;
	kex->flags     = DSSH_KEX_FLAG_NEEDS_SIGNATURE_CAPABLE | DSSH_KEX_FLAG_K_ENCODING_STRING;
	kex->hash_name = "SHA-256";
	kex->ctx       = NULL;
	memcpy(kex->name, KEX_NAME, KEX_NAME_LEN + 1);
	return dssh_transport_register_kex(kex);
}
