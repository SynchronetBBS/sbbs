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

/*
 * Compute RSA key bits from the wire-format public key blob.
 * Parses past the algo name and exponent to reach the modulus,
 * then counts bits from the raw big-endian byte array.
 */
static unsigned int
key_bits(const uint8_t *key_blob, size_t key_blob_len)
{
	size_t   kp = 0;
	uint32_t slen;

	/* Skip algorithm name */
	if (dssh_parse_uint32(&key_blob[kp], key_blob_len, &slen) < 4 || kp + 4 + slen > key_blob_len)
		return 0;
	kp += 4 + slen;

	/* Skip e (public exponent) */
	if (dssh_parse_uint32(&key_blob[kp], key_blob_len - kp, &slen) < 4 || kp + 4 + slen > key_blob_len)
		return 0;
	kp += 4 + slen;

	/* Parse n (modulus) length */
	if (dssh_parse_uint32(&key_blob[kp], key_blob_len - kp, &slen) < 4 || kp + 4 + slen > key_blob_len)
		return 0;
	kp += 4;

	/* Skip leading zero bytes (mpint sign padding) */
	const uint8_t *n = &key_blob[kp];
	uint32_t       n_len = slen;

	while (n_len > 0 && n[0] == 0) {
		n++;
		n_len--;
	}
	if (n_len == 0)
		return 0;

	/* Count bits: 8 * (n_len - 1) + high-bit position of first byte */
	unsigned int bits = (n_len - 1) * 8;
	uint8_t      msb  = n[0];

	while (msb) {
		bits++;
		msb >>= 1;
	}
	return bits;
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
	return dssh_transport_register_key_algo(ka);
}
