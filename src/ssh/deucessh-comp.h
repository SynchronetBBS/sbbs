/*
 * deucessh-comp.h -- Compression algorithm module interface.
 */

#ifndef DSSH_COMP_H
#define DSSH_COMP_H

#include <stddef.h>
#include <stdint.h>

#include "deucessh-portable.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct dssh_comp_ctx dssh_comp_ctx;

typedef int  (*dssh_comp_compress)(uint8_t *buf, size_t *bufsz, dssh_comp_ctx *ctx);
typedef int  (*dssh_comp_uncompress)(uint8_t *buf, size_t *bufsz, dssh_comp_ctx *ctx);
typedef void (*dssh_comp_cleanup)(dssh_comp_ctx *ctx);

typedef struct dssh_comp_s {
	struct dssh_comp_s  *next; /* must be first -- generic traversal assumes offsetof(next) == 0 */
	dssh_comp_compress   compress;
	dssh_comp_uncompress uncompress;
	dssh_comp_cleanup    cleanup;
	char                 name[];
} *dssh_comp;

static_assert(!offsetof(struct dssh_comp_s, next), "next must be at offset 0 for generic list traversal");
DSSH_PUBLIC int dssh_transport_register_comp(dssh_comp comp);

#ifdef __cplusplus
}
#endif

#endif /* DSSH_COMP_H */
