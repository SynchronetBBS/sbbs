#include <stdlib.h>
#include <string.h>

#include "deucessh.h"

static int
compress(uint8_t *buf, size_t *bufsz, deuce_ssh_comp_ctx *ctx)
{
	return 0;
}

static int
uncompress(uint8_t *buf, size_t *bufsz, deuce_ssh_comp_ctx *ctx)
{
	return 0;
}

static void
cleanup(deuce_ssh_comp_ctx *ctx)
{
}

DEUCE_SSH_PUBLIC int
register_none_comp(void)
{
	static const char name[] = "none";
	struct deuce_ssh_comp_s *comp = malloc(sizeof(*comp) + sizeof(name));
	if (comp == NULL)
		return DEUCE_SSH_ERROR_ALLOC;
	comp->next = NULL;
	comp->compress = compress;
	comp->uncompress = uncompress;
	comp->cleanup = cleanup;
	memcpy(comp->name, name, sizeof(name));
	return deuce_ssh_transport_register_comp(comp);
}
