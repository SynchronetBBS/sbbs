/* Private DH-GEX operations. Native objects never cross the public boundary. */
#ifndef DSSH_DHGEX_OPS_H
#define DSSH_DHGEX_OPS_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "deucessh-portable.h"
#ifdef __cplusplus
extern "C" {
#endif
#define SHA256_DIGEST_LEN 32
#define DHGEX_KEX_NAME "diffie-hellman-group-exchange-sha256"
#define DHGEX_KEX_NAME_LEN 36
#define SSH_MSG_KEX_DH_GEX_REQUEST UINT8_C(34)
#define SSH_MSG_KEX_DH_GEX_GROUP UINT8_C(31)
#define SSH_MSG_KEX_DH_GEX_INIT UINT8_C(32)
#define SSH_MSG_KEX_DH_GEX_REPLY UINT8_C(33)
#define GEX_MIN 2048
#define GEX_N 4096
#define GEX_MAX 8192
#define DHGEX_RAW_MAX (GEX_MAX / 8)
#define DHGEX_MPINT_MAX (DHGEX_RAW_MAX + 5)
struct dssh_kex_context;
struct dssh_kex_result;
/* init publishes NULL or cleanup-valid state, including on failure; NULL success
 * is permitted. reset clears every private exponent/derived value and destroys
 * any per-exchange native key, retaining only safe reusable containers/capacity.
 * cleanup/reset accept NULL. keygen imports/validates the supplied raw unsigned
 * group and ALWAYS generates a fresh exponent; it writes raw unsigned public
 * bytes into caller storage. derive validates the raw peer and writes raw K into
 * caller storage. Both output lengths are bounded by capacity. No ownership
 * transfer or allocating result adapter is part of this private interface. */
struct dhgex_ops {
	int (*init)(void **state);
	void (*reset)(void *state);
	void (*cleanup)(void *state);
	int (*keygen)(void *state, const uint8_t *p, size_t plen, const uint8_t *g, size_t glen,
	    uint8_t *public_key, size_t capacity, size_t *length);
	int (*derive)(void *state, const uint8_t *peer, size_t peerlen,
	    uint8_t *secret, size_t capacity, size_t *length);
};
/* Fixed RFC 3526 group and RFC 8268 hash selected by its constructor.
 * These immutable module records are never application configuration. */
struct dssh_dh_fixed_group {
	const uint8_t *p;
	size_t p_len;
	const char *hash_name;
	size_t hash_len;
};
extern DSSH_PRIVATE const struct dhgex_ops dssh_dhgex_backend;
DSSH_PRIVATE int dhgex_init(bool client, void *config, void **state);
DSSH_PRIVATE int dhgex_init_impl(bool client, void *config, void **state, const struct dhgex_ops *ops);
DSSH_PRIVATE int dssh_dh_fixed_init(bool client, void **state, const struct dssh_dh_fixed_group *group);
DSSH_PRIVATE void dhgex_cleanup(void *state);
DSSH_PRIVATE int64_t dssh_dhgex_parse_mpint(const uint8_t *buf, size_t bufsz,
    const uint8_t **body, size_t *body_len);
DSSH_PRIVATE int dhgex_handler_impl(const struct dssh_kex_context *ctx,
    struct dssh_kex_result *out, const struct dhgex_ops *ops);
#ifdef __cplusplus
}
#endif
#endif
