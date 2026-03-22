#include <openssl/core_names.h>
#include <openssl/evp.h>
#include <openssl/params.h>
#include <stdlib.h>
#include <string.h>

#include "ssh-trans.h"

#define HMAC_SHA2_256_DIGEST_SIZE 32
#define HMAC_SHA2_256_KEY_SIZE    32
#define HMAC_SHA2_256_NAME        "hmac-sha2-256"
#define HMAC_SHA2_256_NAME_LEN    13

struct cbdata {
	EVP_MAC *mac;
	EVP_MAC_CTX *ctx;
};

static int
init(const uint8_t *key, dssh_mac_ctx **out)
{
	struct cbdata *cbd = calloc(1, sizeof(*cbd));
	if (cbd == NULL)
		return DSSH_ERROR_ALLOC;

	cbd->mac = EVP_MAC_fetch(NULL, "HMAC", NULL);
	if (cbd->mac == NULL) {
		free(cbd);
		return DSSH_ERROR_INIT;
	}

	cbd->ctx = EVP_MAC_CTX_new(cbd->mac);
	if (cbd->ctx == NULL) {
		EVP_MAC_free(cbd->mac);
		free(cbd);
		return DSSH_ERROR_ALLOC;
	}

	OSSL_PARAM params[] = {
		OSSL_PARAM_construct_utf8_string(
		    OSSL_MAC_PARAM_DIGEST, "SHA256", 0),
		OSSL_PARAM_construct_end(),
	};

	if (EVP_MAC_init(cbd->ctx, key, HMAC_SHA2_256_KEY_SIZE,
	    params) != 1) {
		EVP_MAC_CTX_free(cbd->ctx);
		EVP_MAC_free(cbd->mac);
		free(cbd);
		return DSSH_ERROR_INIT;
	}

	*out = (dssh_mac_ctx *)cbd;
	return 0;
}

static int
generate(const uint8_t *buf, size_t bufsz, uint8_t *outbuf,
    dssh_mac_ctx *ctx)
{
	struct cbdata *cbd = (struct cbdata *)ctx;
	if (cbd == NULL || cbd->ctx == NULL)
		return DSSH_ERROR_INIT;

	/* Re-initialize with same key — resets HMAC state */
	if (EVP_MAC_init(cbd->ctx, NULL, 0, NULL) != 1)
		return DSSH_ERROR_INIT;

	if (EVP_MAC_update(cbd->ctx, buf, bufsz) != 1)
		return DSSH_ERROR_INIT;

	size_t outlen = 0;
	if (EVP_MAC_final(cbd->ctx, outbuf, &outlen,
	    HMAC_SHA2_256_DIGEST_SIZE) != 1)
		return DSSH_ERROR_INIT;

	return 0;
}

static void
cleanup(dssh_mac_ctx *ctx)
{
	struct cbdata *cbd = (struct cbdata *)ctx;
	if (cbd != NULL) {
		EVP_MAC_CTX_free(cbd->ctx);
		EVP_MAC_free(cbd->mac);
		free(cbd);
	}
}

DSSH_PUBLIC int
register_hmac_sha2_256(void)
{
	static const char name[] = HMAC_SHA2_256_NAME;
	struct dssh_mac_s *mac = malloc(sizeof(*mac) + sizeof(name));
	if (mac == NULL)
		return DSSH_ERROR_ALLOC;
	mac->next = NULL;
	mac->init = init;
	mac->generate = generate;
	mac->cleanup = cleanup;
	mac->digest_size = HMAC_SHA2_256_DIGEST_SIZE;
	mac->key_size = HMAC_SHA2_256_KEY_SIZE;
	memcpy(mac->name, name, sizeof(name));
	return dssh_transport_register_mac(mac);
}
