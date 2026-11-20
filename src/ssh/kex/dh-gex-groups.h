/* kex/dh-gex-groups.h -- RFC 3526 DH group tables for DH-GEX. */

#ifndef DH_GEX_GROUPS_H
#define DH_GEX_GROUPS_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#include "deucessh-portable.h"

#ifdef __cplusplus
extern "C" {
#endif

struct dssh_dh_gex_provider;
struct dssh_dh_gex_group_result;

extern const uint8_t rfc3526_grp14_p[]; /* 2048-bit, 256 bytes */
extern const uint8_t rfc3526_grp15_p[]; /* 3072-bit, 384 bytes */
extern const uint8_t rfc3526_grp16_p[]; /* 4096-bit, 512 bytes */
extern const uint8_t rfc3526_grp17_p[]; /* 6144-bit, 768 bytes */
extern const uint8_t rfc3526_grp18_p[]; /* 8192-bit, 1024 bytes */

struct rfc3526_group {
	uint32_t       bits;
	const uint8_t *p;
	size_t         p_len;
};

extern const struct rfc3526_group rfc3526_groups[];
#define RFC3526_NGROUPS 5

int default_select_group(uint32_t min, uint32_t preferred, uint32_t max,
    struct dssh_dh_gex_group_result *out, void *cbdata);

extern struct dssh_dh_gex_provider default_provider;

DSSH_PRIVATE bool dssh_dhgex_group_known(const uint8_t *p, size_t p_len,
    const uint8_t *g, size_t g_len);

#ifdef __cplusplus
}
#endif

#endif /* DH_GEX_GROUPS_H */
