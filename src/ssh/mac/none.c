#include <stdlib.h>
#include <string.h>

#include "deucessh.h"

static int
generate(const uint8_t *buf, size_t bufsz, uint8_t *outbuf,
    deuce_ssh_mac_ctx *ctx)
{
	return 0;
}

static void
cleanup(deuce_ssh_mac_ctx *ctx)
{
}

DEUCE_SSH_PUBLIC int
register_none_mac(void)
{
	static const char name[] = "none";
	struct deuce_ssh_mac_s *mac = malloc(sizeof(*mac) + sizeof(name));
	if (mac == NULL)
		return DEUCE_SSH_ERROR_ALLOC;
	mac->next = NULL;
	mac->init = NULL;
	mac->generate = generate;
	mac->cleanup = cleanup;
	mac->digest_size = 0;
	mac->key_size = 0;
	memcpy(mac->name, name, sizeof(name));
	return deuce_ssh_transport_register_mac(mac);
}
