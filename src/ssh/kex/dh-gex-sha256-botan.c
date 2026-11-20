/*
 * kex/dh-gex-sha256-botan.c -- Registration for DH-GEX SHA-256 (Botan3).
 */

#include <stdlib.h>
#include <string.h>

#include "deucessh-algorithms.h"
#include "deucessh-kex.h"
#include "deucessh.h"
#include "dh-gex-sha256-ops.h"
#include "kex/dh-gex-groups.h"
#include "kex/dh-gex-sha256.h"
#ifdef DSSH_TESTING
 #include "ssh-internal.h"
#endif

extern int  dssh_botan_dhgex_handler(const struct dssh_kex_context *kctx, struct dssh_kex_result *out);

DSSH_PUBLIC int
dssh_register_dh_gex_sha256(void)
{
	struct dssh_kex_s *kex = malloc(sizeof(*kex) + DHGEX_KEX_NAME_LEN + 1);

	if (kex == NULL)
		return DSSH_ERROR_ALLOC;
	kex->next      = NULL;
	kex->init      = dhgex_init;
	kex->handler   = dssh_botan_dhgex_handler;
	kex->cleanup   = dhgex_cleanup;
	kex->flags     = DSSH_KEX_FLAG_NEEDS_SIGNATURE_CAPABLE;
	kex->hash_name = "SHA-256";
	kex->config       = &default_provider;
	memcpy(kex->name, DHGEX_KEX_NAME, DHGEX_KEX_NAME_LEN + 1);
	int ret = dssh_transport_register_kex(kex);
	if (ret < 0)
		free(kex);
	return ret;
}
