/*
 * key_algo/rsa-sha2-256-botan.c -- Registration for rsa-sha2-256 (Botan3).
 */

#include <stdlib.h>
#include <string.h>

#include "deucessh-key-algo.h"
#include "deucessh.h"

#define RSA_SHA2_256_NAME     "rsa-sha2-256"
#define RSA_SHA2_256_NAME_LEN 12

/* Implemented in rsa-sha2-256-botan.cpp */
extern int  dssh_botan_rsa256_sign(uint8_t **out, size_t *outlen, const uint8_t *data, size_t data_len,
     dssh_key_algo_ctx *ctx);
extern int  dssh_botan_rsa256_verify(const uint8_t *key_blob, size_t key_blob_len, const uint8_t *sig_blob,
     size_t sig_blob_len, const uint8_t *data, size_t data_len);
extern int  dssh_botan_rsa256_pubkey(const uint8_t **out, size_t *outlen, dssh_key_algo_ctx *ctx);
extern int  dssh_botan_rsa256_haskey(dssh_key_algo_ctx *ctx);
extern void dssh_botan_rsa256_cleanup(dssh_key_algo_ctx *ctx);

DSSH_PUBLIC int
dssh_register_rsa_sha2_256(void)
{
	struct dssh_key_algo_s *ka = malloc(sizeof(*ka) + RSA_SHA2_256_NAME_LEN + 1);
	if (ka == NULL)
		return DSSH_ERROR_ALLOC;
	ka->next    = NULL;
	ka->sign    = dssh_botan_rsa256_sign;
	ka->verify  = dssh_botan_rsa256_verify;
	ka->pubkey  = dssh_botan_rsa256_pubkey;
	ka->haskey  = dssh_botan_rsa256_haskey;
	ka->cleanup = dssh_botan_rsa256_cleanup;
	ka->ctx     = NULL;
	ka->flags   = DSSH_KEY_ALGO_FLAG_SIGNATURE_CAPABLE;
	memcpy(ka->name, RSA_SHA2_256_NAME, RSA_SHA2_256_NAME_LEN + 1);
	return dssh_transport_register_key_algo(ka);
}
