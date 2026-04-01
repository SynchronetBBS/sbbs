/*
 * kex/curve25519-sha256-botan.c -- Registration for curve25519-sha256 (Botan3).
 */

#include <stdlib.h>
#include <string.h>

#include "deucessh.h"
#include "deucessh-kex.h"
#include "curve25519-sha256-ops.h"

/* Implemented in curve25519-sha256-botan.cpp */
extern int dssh_botan_curve25519_handler(struct dssh_kex_context *kctx);
extern void dssh_botan_curve25519_cleanup(void *kex_data);

DSSH_PUBLIC int
dssh_register_curve25519_sha256(void)
{
	struct dssh_kex_s *kex = malloc(sizeof(*kex) + C25519_KEX_NAME_LEN + 1);

	if (kex == NULL)
		return DSSH_ERROR_ALLOC;
	kex->next = NULL;
	kex->handler = dssh_botan_curve25519_handler;
	kex->cleanup = dssh_botan_curve25519_cleanup;
	kex->flags = DSSH_KEX_FLAG_NEEDS_SIGNATURE_CAPABLE;
	kex->hash_name = "SHA-256";
	kex->ctx = NULL;
	memcpy(kex->name, C25519_KEX_NAME, C25519_KEX_NAME_LEN + 1);
	return dssh_transport_register_kex(kex);
}
