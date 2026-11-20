/*
 * kex/sntrup761x25519-sha512-botan.c -- Registration (Botan3).
 */

#include <stdlib.h>
#include <string.h>

#include "deucessh-algorithms.h"
#include "deucessh-kex.h"
#include "hybrid-pq-kex-ops.h"
#include "deucessh.h"
#ifdef DSSH_TESTING
 #include "ssh-internal.h"
#endif

#define KEX_NAME     "sntrup761x25519-sha512"
#define KEX_NAME_LEN 22

extern int  dssh_botan_sntrup761x25519_handler(const struct dssh_kex_context *kctx, struct dssh_kex_result *out);

DSSH_PUBLIC int
dssh_register_sntrup761x25519_sha512(void)
{
	struct dssh_kex_s *kex = malloc(sizeof(*kex) + KEX_NAME_LEN + 1);

	if (kex == NULL)
		return DSSH_ERROR_ALLOC;
	kex->next      = NULL;
	kex->init      = sntrup761x25519_init;
	kex->handler   = dssh_botan_sntrup761x25519_handler;
	kex->cleanup   = hybrid_pq_cleanup;
	kex->flags     = DSSH_KEX_FLAG_NEEDS_SIGNATURE_CAPABLE | DSSH_KEX_FLAG_K_ENCODING_STRING;
	kex->hash_name = "SHA-512";
	kex->config       = NULL;
	memcpy(kex->name, KEX_NAME, KEX_NAME_LEN + 1);
	int ret = dssh_transport_register_kex(kex);
	if (ret < 0)
		free(kex);
	return ret;
}
