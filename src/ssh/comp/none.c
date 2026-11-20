#include <stdlib.h>
#include <string.h>

#include "deucessh-algorithms.h"
#include "deucessh-comp.h"
#include "deucessh.h"
#ifdef DSSH_TESTING
 #include "ssh-internal.h"
#endif

static int
init(bool compress, dssh_comp_ctx **ctx)
{
	(void)compress;
	if (ctx == NULL || *ctx != NULL)
		return DSSH_ERROR_INVALID;
	*ctx = NULL;
	return 0;
}

static int
bound(size_t input_len, size_t *output_bound, dssh_comp_ctx *ctx)
{
	(void)ctx;
	if (output_bound == NULL)
		return DSSH_ERROR_INVALID;
	*output_bound = input_len;
	return 0;
}

static int
transform(const uint8_t *input, size_t input_len, uint8_t *output, size_t output_capacity,
    size_t *output_len, dssh_comp_ctx *ctx)
{
	(void)ctx;
	if (output_len == NULL || (input_len > 0 && input == NULL)
	    || (output_capacity > 0 && output == NULL))
		return DSSH_ERROR_INVALID;
	*output_len = 0;
	if (input_len > output_capacity)
		return DSSH_ERROR_TOOLONG;
	if (input_len > 0)
		memcpy(output, input, input_len);
	*output_len = input_len;
	return 0;
}

static void
cleanup(dssh_comp_ctx *ctx)
{
	(void)ctx;
}

DSSH_PUBLIC int
dssh_register_none_comp(void)
{
	static const char   name[] = "none";
	struct dssh_comp_s *comp   = malloc(sizeof(*comp) + sizeof(name));

	if (comp == NULL)
		return DSSH_ERROR_ALLOC;
	comp->next       = NULL;
	comp->init       = init;
	comp->bound      = bound;
	comp->compress   = transform;
	comp->uncompress = transform;
	comp->cleanup    = cleanup;
	memcpy(comp->name, name, sizeof(name));
	int ret = dssh_transport_register_comp(comp);
	if (ret < 0)
		free(comp);
	return ret;
}
