#include <stdlib.h>
#include <string.h>

#include "deucessh.h"
#include "deucessh-mac.h"

static int
generate(const uint8_t *buf, size_t bufsz, uint8_t *outbuf,
    dssh_mac_ctx *ctx)
{
	return 0;
}

static void
cleanup(dssh_mac_ctx *ctx)
{
}

DSSH_PUBLIC int
dssh_register_none_mac(void)
{
	static const char  name[] = "none";
	struct dssh_mac_s *mac = malloc(sizeof(*mac) + sizeof(name));

	if (mac == NULL)
		return DSSH_ERROR_ALLOC;
	mac->next = NULL;
	mac->init = NULL;
	mac->generate = generate;
	mac->cleanup = cleanup;
	mac->digest_size = 0;
	mac->key_size = 0;
	memcpy(mac->name, name, sizeof(name));
	return dssh_transport_register_mac(mac);
}
