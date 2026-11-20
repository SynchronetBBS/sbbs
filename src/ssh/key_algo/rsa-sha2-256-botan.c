/*
 * key_algo/rsa-sha2-256-botan.c -- Registration for rsa-sha2-256 (Botan3).
 */

#include <stdlib.h>
#include <string.h>

#include "deucessh-key-algo.h"
#include "deucessh.h"
#include "key-wire.h"
#include "client-key.h"
#ifdef DSSH_TESTING
 #include "ssh-internal.h"
#endif

#define RSA_SHA2_256_NAME     "rsa-sha2-256"
#define RSA_SHA2_256_NAME_LEN 12

/* Implemented in rsa-sha2-256-botan.cpp */
extern int  dssh_botan_rsa256_sign(struct dssh_bytes_result *out, const uint8_t *data, size_t data_len,
     dssh_key_algo_ctx *ctx);
extern int  dssh_botan_rsa256_verify(const uint8_t *key_blob, size_t key_blob_len, const uint8_t *sig_blob,
     size_t sig_blob_len, const uint8_t *data, size_t data_len);
extern int  dssh_botan_rsa256_pubkey(const uint8_t **out, size_t *outlen, dssh_key_algo_ctx *ctx);
extern int  dssh_botan_rsa256_haskey(dssh_key_algo_ctx *ctx);
extern void dssh_botan_rsa256_cleanup(dssh_key_algo_ctx *ctx);

/*
 * Compute RSA key bits from the wire-format public key blob.
 * Parses past the algo name and exponent to reach the modulus,
 * then counts bits from the raw big-endian byte array.
 */
static unsigned int
key_bits(const uint8_t *key_blob, size_t key_blob_len)
{
	struct dssh_rsa_wire_key key;
	if (dssh_key_wire_rsa(key_blob, key_blob_len, &key) < 0)
		return 0;
	return dssh_key_wire_bits(key.modulus, UINT_MAX);
}

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
	ka->cleanup  = dssh_botan_rsa256_cleanup;
	ka->key_bits = key_bits;
	ka->ctx      = NULL;
	ka->flags    = DSSH_KEY_ALGO_FLAG_SIGNATURE_CAPABLE;
	memcpy(ka->name, RSA_SHA2_256_NAME, RSA_SHA2_256_NAME_LEN + 1);
	int ret = dssh_transport_register_key_algo(ka);
	if (ret < 0)
		free(ka);
	return ret;
}

extern const struct dssh_client_key_ops *dssh_botan_rsa256_client_ops(void);

DSSH_PRIVATE const struct dssh_client_key_ops *
dssh_rsa256_client_ops(struct dssh_key_algo_s *module)
{
	if (module->sign != dssh_botan_rsa256_sign
	    || module->pubkey != dssh_botan_rsa256_pubkey
	    || module->haskey != dssh_botan_rsa256_haskey)
		return NULL;
	return dssh_botan_rsa256_client_ops();
}
