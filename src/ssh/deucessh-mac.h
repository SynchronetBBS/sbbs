/*
 * deucessh-mac.h -- MAC algorithm module interface.
 */

#ifndef DSSH_MAC_H
#define DSSH_MAC_H

#include <stddef.h>
#include <stdint.h>

#include "deucessh-portable.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct dssh_mac_ctx dssh_mac_ctx;

typedef int (*dssh_mac_init)(const uint8_t *key, dssh_mac_ctx **ctx);
typedef int (*dssh_mac_generate)(const uint8_t *buf, size_t bufsz,
    uint8_t *outbuf, dssh_mac_ctx *ctx);
typedef void (*dssh_mac_cleanup)(dssh_mac_ctx *ctx);

typedef struct dssh_mac_s {
	struct dssh_mac_s *next;       /* must be first -- generic traversal assumes offsetof(next) == 0 */
	dssh_mac_init      init;
	dssh_mac_generate  generate;
	dssh_mac_cleanup   cleanup;
	uint16_t           digest_size;
	uint16_t           key_size;
	char               name[];
} *dssh_mac;
static_assert(!offsetof(struct dssh_mac_s, next),
    "next must be at offset 0 for generic list traversal");

DSSH_PUBLIC int dssh_transport_register_mac(dssh_mac mac);

#ifdef __cplusplus
}
#endif

#endif /* DSSH_MAC_H */
