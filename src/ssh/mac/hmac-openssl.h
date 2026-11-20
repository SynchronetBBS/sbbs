#ifndef DSSH_HMAC_OPENSSL_H
#define DSSH_HMAC_OPENSSL_H

#include <openssl/core_names.h>
#include <openssl/evp.h>
#include <openssl/params.h>

#include <stdlib.h>
#include <string.h>

#include "deucessh-mac.h"
#include "deucessh.h"
#ifdef DSSH_TESTING
 #include "ssh-internal.h"
#endif

struct dssh_openssl_hmac {
	EVP_MAC *mac;
	EVP_MAC_CTX *ctx;
	size_t reset_key_size;
	uint8_t reset_key[];
};

static inline void
dssh_openssl_hmac_cleanup(dssh_mac_ctx *ctx)
{
	struct dssh_openssl_hmac *hmac = (struct dssh_openssl_hmac *)ctx;
	if (hmac == NULL)
		return;
	EVP_MAC_CTX_free(hmac->ctx);
	EVP_MAC_free(hmac->mac);
	/* The context allocation was checked with this exact retained size. */
	dssh_secure_zero(hmac, sizeof(*hmac) + hmac->reset_key_size);
	free(hmac);
}

static inline int
dssh_openssl_hmac_init(const uint8_t *key, size_t key_size, char *digest, dssh_mac_ctx **out)
{
	/* OpenSSL 3.0.0 through 3.0.2 skip HMAC reinitialization when
	 * EVP_MAC_init receives a null key. Keep an explicit reset key only
	 * for those provider releases. Later releases reset the keyed context. */
	bool explicit_reset = OPENSSL_version_major() == 3 && OPENSSL_version_minor() == 0
	    && OPENSSL_version_patch() < 3;
	size_t retained = explicit_reset ? key_size : 0;
	if (retained > SIZE_MAX - sizeof(struct dssh_openssl_hmac))
		return DSSH_ERROR_INVALID;
	struct dssh_openssl_hmac *hmac = calloc(1, sizeof(*hmac) + retained);
	if (hmac == NULL)
		return DSSH_ERROR_ALLOC;
	*hmac = (struct dssh_openssl_hmac){.reset_key_size = retained};
	dssh_mac_ctx *result = (dssh_mac_ctx *)hmac;

	hmac->mac = EVP_MAC_fetch(NULL, "HMAC", NULL);
	if (hmac->mac == NULL) {
		dssh_openssl_hmac_cleanup(result);
		return DSSH_ERROR_INIT;
	}
	hmac->ctx = EVP_MAC_CTX_new(hmac->mac);
	if (hmac->ctx == NULL) {
		dssh_openssl_hmac_cleanup(result);
		return DSSH_ERROR_ALLOC;
	}
	OSSL_PARAM params[] = {
	    OSSL_PARAM_construct_utf8_string(OSSL_MAC_PARAM_DIGEST, digest, 0),
	    OSSL_PARAM_construct_end(),
	};
	if (EVP_MAC_init(hmac->ctx, key, key_size, params) != 1) {
		dssh_openssl_hmac_cleanup(result);
		return DSSH_ERROR_INIT;
	}
	if (retained > 0)
		memcpy(hmac->reset_key, key, retained);
	*out = result;
	return 0;
}

static inline int
dssh_openssl_hmac_generate(const uint8_t *buf, size_t bufsz, uint8_t *outbuf,
    size_t digest_size, dssh_mac_ctx *ctx)
{
	struct dssh_openssl_hmac *hmac = (struct dssh_openssl_hmac *)ctx;
	if (hmac == NULL || hmac->ctx == NULL)
		return DSSH_ERROR_INIT;
	const uint8_t *key = hmac->reset_key_size > 0 ? hmac->reset_key : NULL;
	if (EVP_MAC_init(hmac->ctx, key, hmac->reset_key_size, NULL) != 1
	    || EVP_MAC_update(hmac->ctx, buf, bufsz) != 1)
		return DSSH_ERROR_INIT;
	size_t outlen = 0;
	if (EVP_MAC_final(hmac->ctx, outbuf, &outlen, digest_size) != 1 || outlen != digest_size)
		return DSSH_ERROR_INIT;
	return 0;
}

#endif
