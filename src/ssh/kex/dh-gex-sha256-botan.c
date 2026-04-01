/*
 * kex/dh-gex-sha256-botan.c -- Registration for DH-GEX SHA-256 (Botan3).
 */

#include <stdlib.h>
#include <string.h>

#include "deucessh.h"
#include "deucessh-kex.h"
#include "dh-gex-sha256-ops.h"
#include "kex/dh-gex-sha256.h"
#include "kex/dh-gex-groups.h"

extern int dssh_botan_dhgex_handler(struct dssh_kex_context *kctx);
extern void dssh_botan_dhgex_cleanup(void *kex_data);

DSSH_PUBLIC int
dssh_register_dh_gex_sha256(void)
{
	struct dssh_kex_s *kex = malloc(sizeof(*kex) + DHGEX_KEX_NAME_LEN + 1);

	if (kex == NULL)
		return DSSH_ERROR_ALLOC;
	kex->next = NULL;
	kex->handler = dssh_botan_dhgex_handler;
	kex->cleanup = dssh_botan_dhgex_cleanup;
	kex->flags = DSSH_KEX_FLAG_NEEDS_SIGNATURE_CAPABLE;
	kex->hash_name = "SHA-256";
	kex->ctx = &default_provider;
	memcpy(kex->name, DHGEX_KEX_NAME, DHGEX_KEX_NAME_LEN + 1);
	return dssh_transport_register_kex(kex);
}
