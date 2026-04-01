/*
 * kex/sntrup761x25519-sha512-botan.c -- Registration (Botan3).
 */

#include <stdlib.h>
#include <string.h>

#include "deucessh.h"
#include "deucessh-kex.h"

#define KEX_NAME     "sntrup761x25519-sha512"
#define KEX_NAME_LEN 22

extern int dssh_botan_sntrup761x25519_handler(struct dssh_kex_context *kctx);
extern void dssh_botan_sntrup761x25519_cleanup(void *kex_data);

DSSH_PUBLIC int
dssh_register_sntrup761x25519_sha512(void)
{
	struct dssh_kex_s *kex = malloc(sizeof(*kex) + KEX_NAME_LEN + 1);

	if (kex == NULL)
		return DSSH_ERROR_ALLOC;
	kex->next = NULL;
	kex->handler = dssh_botan_sntrup761x25519_handler;
	kex->cleanup = dssh_botan_sntrup761x25519_cleanup;
	kex->flags = DSSH_KEX_FLAG_NEEDS_SIGNATURE_CAPABLE
	    | DSSH_KEX_FLAG_K_ENCODING_STRING;
	kex->hash_name = "SHA-512";
	kex->ctx = NULL;
	memcpy(kex->name, KEX_NAME, KEX_NAME_LEN + 1);
	return dssh_transport_register_kex(kex);
}
