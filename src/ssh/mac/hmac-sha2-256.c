#include <openssl/core_names.h>
#include <openssl/evp.h>
#include <openssl/params.h>
#include <stdlib.h>
#include <string.h>

#include "deucessh.h"

#define HMAC_SHA2_256_DIGEST_SIZE 32
#define HMAC_SHA2_256_KEY_SIZE    32
#define HMAC_SHA2_256_NAME        "hmac-sha2-256"
#define HMAC_SHA2_256_NAME_LEN    13

static int
generate(const uint8_t *key, const uint8_t *buf, size_t bufsz, uint8_t *outbuf)
{
	EVP_MAC *mac = EVP_MAC_fetch(NULL, "HMAC", NULL);
	if (mac == NULL)
		return DEUCE_SSH_ERROR_INIT;

	EVP_MAC_CTX *ctx = EVP_MAC_CTX_new(mac);
	if (ctx == NULL) {
		EVP_MAC_free(mac);
		return DEUCE_SSH_ERROR_ALLOC;
	}

	OSSL_PARAM params[] = {
		OSSL_PARAM_construct_utf8_string(
		    OSSL_MAC_PARAM_DIGEST, "SHA256", 0),
		OSSL_PARAM_construct_end(),
	};

	if (EVP_MAC_init(ctx, key, HMAC_SHA2_256_KEY_SIZE, params) != 1) {
		EVP_MAC_CTX_free(ctx);
		EVP_MAC_free(mac);
		return DEUCE_SSH_ERROR_INIT;
	}

	if (EVP_MAC_update(ctx, buf, bufsz) != 1) {
		EVP_MAC_CTX_free(ctx);
		EVP_MAC_free(mac);
		return DEUCE_SSH_ERROR_INIT;
	}

	size_t outlen = 0;
	if (EVP_MAC_final(ctx, outbuf, &outlen, HMAC_SHA2_256_DIGEST_SIZE) != 1) {
		EVP_MAC_CTX_free(ctx);
		EVP_MAC_free(mac);
		return DEUCE_SSH_ERROR_INIT;
	}

	EVP_MAC_CTX_free(ctx);
	EVP_MAC_free(mac);
	return 0;
}

static void
cleanup(deuce_ssh_mac_ctx *ctx)
{
	free(ctx);
}

int
register_hmac_sha2_256(void)
{
	static const char name[] = HMAC_SHA2_256_NAME;
	struct deuce_ssh_mac_s *mac = malloc(sizeof(*mac) + sizeof(name));
	if (mac == NULL)
		return DEUCE_SSH_ERROR_ALLOC;
	mac->next = NULL;
	mac->generate = generate;
	mac->cleanup = cleanup;
	mac->digest_size = HMAC_SHA2_256_DIGEST_SIZE;
	mac->key_size = HMAC_SHA2_256_KEY_SIZE;
	memcpy(mac->name, name, sizeof(name));
	return deuce_ssh_transport_register_mac(mac);
}
