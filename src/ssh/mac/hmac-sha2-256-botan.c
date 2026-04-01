/*
 * mac/hmac-sha2-256-botan.c -- Registration wrapper for HMAC-SHA-256 (Botan3).
 */

#include <stdlib.h>
#include <string.h>

#include "deucessh.h"
#include "deucessh-mac.h"

#define HMAC_SHA2_256_DIGEST_SIZE 32
#define HMAC_SHA2_256_KEY_SIZE 32
#define HMAC_SHA2_256_NAME "hmac-sha2-256"
#define HMAC_SHA2_256_NAME_LEN 13

extern int dssh_botan_hmac256_init(const uint8_t *key, dssh_mac_ctx **out);
extern int dssh_botan_hmac256_generate(const uint8_t *buf, size_t bufsz,
    uint8_t *outbuf, dssh_mac_ctx *ctx);
extern void dssh_botan_hmac256_cleanup(dssh_mac_ctx *ctx);

DSSH_PUBLIC int
dssh_register_hmac_sha2_256(void)
{
	static const char  name[] = HMAC_SHA2_256_NAME;
	struct dssh_mac_s *mac = malloc(sizeof(*mac) + sizeof(name));

	if (mac == NULL)
		return DSSH_ERROR_ALLOC;
	mac->next = NULL;
	mac->init = dssh_botan_hmac256_init;
	mac->generate = dssh_botan_hmac256_generate;
	mac->cleanup = dssh_botan_hmac256_cleanup;
	mac->digest_size = HMAC_SHA2_256_DIGEST_SIZE;
	mac->key_size = HMAC_SHA2_256_KEY_SIZE;
	memcpy(mac->name, name, sizeof(name));
	return dssh_transport_register_mac(mac);
}
