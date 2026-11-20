/* RFC 8268 fixed RFC 3526 groups, with the RFC 4253 DH wire exchange. */
#include <stdlib.h>
#include <string.h>
#include "deucessh-algorithms.h"
#include "deucessh-kex.h"
#include "deucessh.h"
#include "dh-gex-groups.h"
#include "dh-gex-sha256-ops.h"
#ifdef DSSH_TESTING
 #include "ssh-internal.h"
#endif

static const struct dssh_dh_fixed_group group14 = {rfc3526_grp14_p, 256, "SHA-256", 32};
static const struct dssh_dh_fixed_group group16 = {rfc3526_grp16_p, 512, "SHA-512", 64};

static int
group14_init(bool client, void *config, void **state)
{
	(void)config;
	return dssh_dh_fixed_init(client, state, &group14);
}

static int
group16_init(bool client, void *config, void **state)
{
	(void)config;
	return dssh_dh_fixed_init(client, state, &group16);
}

static int
fixed_handler(const struct dssh_kex_context *ctx, struct dssh_kex_result *out)
{
	return dhgex_handler_impl(ctx, out, &dssh_dhgex_backend);
}

static int
register_fixed(bool larger)
{
	/* Both registered names contain exactly29 bytes, excluding the NUL. */
	static const char name14[] = "diffie-hellman-group14-sha256";
	static const char name16[] = "diffie-hellman-group16-sha512";
	_Static_assert(sizeof(name14) == sizeof(name16), "fixed DH names have equal extent");
	struct dssh_kex_s *kex = malloc(sizeof(*kex) + sizeof(name14));
	if (kex == NULL)
		return DSSH_ERROR_ALLOC;
	kex->next = NULL;
	kex->init = larger ? group16_init : group14_init;
	kex->handler = fixed_handler;
	kex->cleanup = dhgex_cleanup;
	kex->flags = DSSH_KEX_FLAG_NEEDS_SIGNATURE_CAPABLE;
	kex->hash_name = larger ? group16.hash_name : group14.hash_name;
	kex->config = NULL;
	memcpy(kex->name, larger ? name16 : name14, sizeof(name14));
	int ret = dssh_transport_register_kex(kex);
	if (ret < 0)
		free(kex);
	return ret;
}

DSSH_PUBLIC int
dssh_register_dh_group14_sha256(void)
{
	return register_fixed(false);
}

DSSH_PUBLIC int
dssh_register_dh_group16_sha512(void)
{
	return register_fixed(true);
}
