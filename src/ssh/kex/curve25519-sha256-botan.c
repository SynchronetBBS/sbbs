/*
 * kex/curve25519-sha256-botan.c -- Registration for curve25519-sha256 (Botan3).
 */

#include <stdlib.h>
#include <string.h>

#include "deucessh-algorithms.h"
#include "curve25519-sha256-ops.h"
#include "deucessh-kex.h"
#include "deucessh.h"
#ifdef DSSH_TESTING
 #include "ssh-internal.h"
#endif

/* Implemented in curve25519-sha256-botan.cpp */
extern int  dssh_botan_curve25519_handler(const struct dssh_kex_context *kctx, struct dssh_kex_result *out);
extern int dssh_botan_curve25519_init(bool client, void *config, void **state);

DSSH_PRIVATE int
curve25519_init(bool client, void *config, void **state)
{
	return dssh_botan_curve25519_init(client, config, state);
}

DSSH_PUBLIC int
dssh_register_curve25519_sha256(void)
{
	struct dssh_kex_s *kex = malloc(sizeof(*kex) + C25519_KEX_NAME_LEN + 1);

	if (kex == NULL)
		return DSSH_ERROR_ALLOC;
	kex->next      = NULL;
	kex->init      = curve25519_init;
	kex->handler   = dssh_botan_curve25519_handler;
	kex->cleanup   = curve25519_cleanup;
	kex->flags     = DSSH_KEX_FLAG_NEEDS_SIGNATURE_CAPABLE;
	kex->hash_name = "SHA-256";
	kex->config       = NULL;
	memcpy(kex->name, C25519_KEX_NAME, C25519_KEX_NAME_LEN + 1);
	int ret = dssh_transport_register_kex(kex);
	if (ret < 0)
		free(kex);
	return ret;
}
