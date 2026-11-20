#include "deucessh-algorithms.h"
#include "hmac-openssl.h"

#define HMAC_SHA2_256_DIGEST_SIZE 32
#define HMAC_SHA2_256_KEY_SIZE    32
#define HMAC_SHA2_256_NAME        "hmac-sha2-256"
#define HMAC_SHA2_256_NAME_LEN    13

static int
init(const uint8_t *key, dssh_mac_ctx **out)
{
	return dssh_openssl_hmac_init(key, HMAC_SHA2_256_KEY_SIZE, "SHA256", out);
}

static int
generate(const uint8_t *buf, size_t bufsz, uint8_t *outbuf, dssh_mac_ctx *ctx)
{
	return dssh_openssl_hmac_generate(buf, bufsz, outbuf, HMAC_SHA2_256_DIGEST_SIZE, ctx);
}

DSSH_PUBLIC int
dssh_register_hmac_sha2_256(void)
{
	static const char  name[] = HMAC_SHA2_256_NAME;
	struct dssh_mac_s *mac    = malloc(sizeof(*mac) + sizeof(name));

	if (mac == NULL)
		return DSSH_ERROR_ALLOC;
	mac->next        = NULL;
	mac->init        = init;
	mac->generate    = generate;
	mac->cleanup     = dssh_openssl_hmac_cleanup;
	mac->digest_size = HMAC_SHA2_256_DIGEST_SIZE;
	mac->key_size    = HMAC_SHA2_256_KEY_SIZE;
	memcpy(mac->name, name, sizeof(name));
	int ret = dssh_transport_register_mac(mac);
	if (ret < 0)
		free(mac);
	return ret;
}
