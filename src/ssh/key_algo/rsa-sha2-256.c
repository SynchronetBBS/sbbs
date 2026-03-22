// RFC 8332: rsa-sha2-256 Host Key Algorithm

#include <openssl/core_names.h>
#include <openssl/evp.h>
#include <openssl/param_build.h>
#include <openssl/rsa.h>
#include <stdlib.h>
#include <string.h>

#include "deucessh.h"

#define RSA_SHA2_256_NAME     "rsa-sha2-256"
#define RSA_SHA2_256_NAME_LEN 12

/*
 * Verify a signature over data using an RSA public host key blob.
 *
 * key_blob: SSH wire format:
 *   string  "ssh-rsa"
 *   string  e (public exponent, unsigned big-endian)
 *   string  n (modulus, unsigned big-endian)
 *
 * sig_blob: SSH wire format:
 *   string  "rsa-sha2-256"
 *   string  <RSASSA-PKCS1-v1_5 signature bytes>
 */
static int
verify(const uint8_t *key_blob, size_t key_blob_len,
    const uint8_t *sig_blob, size_t sig_blob_len,
    const uint8_t *data, size_t data_len)
{
	int result = DEUCE_SSH_ERROR_INIT;
	BIGNUM *e_bn = NULL;
	BIGNUM *n_bn = NULL;
	OSSL_PARAM_BLD *bld = NULL;
	OSSL_PARAM *params = NULL;
	EVP_PKEY_CTX *pctx = NULL;
	EVP_PKEY *pkey = NULL;
	EVP_MD_CTX *mdctx = NULL;

	/* Parse public key from key_blob */
	size_t kp = 0;
	uint32_t slen;

	/* Skip algorithm name ("ssh-rsa") */
	if (deuce_ssh_parse_uint32(&key_blob[kp], key_blob_len - kp, &slen) < 4 ||
	    kp + 4 + slen > key_blob_len)
		return DEUCE_SSH_ERROR_PARSE;
	kp += 4 + slen;

	/* e (public exponent) */
	if (deuce_ssh_parse_uint32(&key_blob[kp], key_blob_len - kp, &slen) < 4 ||
	    kp + 4 + slen > key_blob_len)
		return DEUCE_SSH_ERROR_PARSE;
	kp += 4;
	e_bn = BN_bin2bn(&key_blob[kp], slen, NULL);
	kp += slen;

	/* n (modulus) */
	if (deuce_ssh_parse_uint32(&key_blob[kp], key_blob_len - kp, &slen) < 4 ||
	    kp + 4 + slen > key_blob_len)
		goto done;
	kp += 4;
	n_bn = BN_bin2bn(&key_blob[kp], slen, NULL);

	if (e_bn == NULL || n_bn == NULL)
		goto done;

	/* Build RSA public key via EVP */
	bld = OSSL_PARAM_BLD_new();
	if (bld == NULL)
		goto done;
	if (OSSL_PARAM_BLD_push_BN(bld, OSSL_PKEY_PARAM_RSA_N, n_bn) != 1 ||
	    OSSL_PARAM_BLD_push_BN(bld, OSSL_PKEY_PARAM_RSA_E, e_bn) != 1)
		goto done;
	params = OSSL_PARAM_BLD_to_param(bld);
	if (params == NULL)
		goto done;

	pctx = EVP_PKEY_CTX_new_from_name(NULL, "RSA", NULL);
	if (pctx == NULL)
		goto done;
	if (EVP_PKEY_fromdata_init(pctx) != 1 ||
	    EVP_PKEY_fromdata(pctx, &pkey, EVP_PKEY_PUBLIC_KEY, params) != 1)
		goto done;

	/* Parse signature from sig_blob */
	size_t sp = 0;

	/* Skip algorithm name ("rsa-sha2-256") */
	if (deuce_ssh_parse_uint32(&sig_blob[sp], sig_blob_len - sp, &slen) < 4 ||
	    sp + 4 + slen > sig_blob_len) {
		result = DEUCE_SSH_ERROR_PARSE;
		goto done;
	}
	sp += 4 + slen;

	/* Raw signature bytes */
	uint32_t raw_sig_len;
	if (deuce_ssh_parse_uint32(&sig_blob[sp], sig_blob_len - sp, &raw_sig_len) < 4 ||
	    sp + 4 + raw_sig_len > sig_blob_len) {
		result = DEUCE_SSH_ERROR_PARSE;
		goto done;
	}
	sp += 4;
	const uint8_t *raw_sig = &sig_blob[sp];

	/* Verify RSASSA-PKCS1-v1_5 with SHA-256 */
	mdctx = EVP_MD_CTX_new();
	if (mdctx == NULL)
		goto done;

	EVP_PKEY_CTX *vctx = NULL;
	if (EVP_DigestVerifyInit(mdctx, &vctx, EVP_sha256(), NULL, pkey) != 1)
		goto done;
	if (EVP_PKEY_CTX_set_rsa_padding(vctx, RSA_PKCS1_PADDING) != 1)
		goto done;

	if (EVP_DigestVerify(mdctx, raw_sig, raw_sig_len,
	    data, data_len) == 1)
		result = 0;
	else
		result = DEUCE_SSH_ERROR_INVALID;

done:
	EVP_MD_CTX_free(mdctx);
	EVP_PKEY_free(pkey);
	EVP_PKEY_CTX_free(pctx);
	OSSL_PARAM_free(params);
	OSSL_PARAM_BLD_free(bld);
	BN_free(e_bn);
	BN_free(n_bn);
	return result;
}

static int
sign(uint8_t *buf, size_t bufsz, size_t *outlen,
    const uint8_t *data, size_t data_len, deuce_ssh_key_algo_ctx *ctx)
{
	/* Not yet implemented — server-side only */
	return DEUCE_SSH_ERROR_INIT;
}

static int
pubkey(uint8_t *buf, size_t bufsz, size_t *outlen, deuce_ssh_key_algo_ctx *ctx)
{
	return DEUCE_SSH_ERROR_INIT;
}

static int
haskey(deuce_ssh_key_algo_ctx *ctx)
{
	return 0;
}

static void
cleanup(deuce_ssh_key_algo_ctx *ctx)
{
}

int
register_rsa_sha2_256(void)
{
	struct deuce_ssh_key_algo_s *ka = malloc(sizeof(*ka) + RSA_SHA2_256_NAME_LEN + 1);
	if (ka == NULL)
		return DEUCE_SSH_ERROR_ALLOC;
	ka->next = NULL;
	ka->sign = sign;
	ka->verify = verify;
	ka->pubkey = pubkey;
	ka->haskey = haskey;
	ka->cleanup = cleanup;
	ka->flags = DEUCE_SSH_KEY_ALGO_FLAG_SIGNATURE_CAPABLE;
	memcpy(ka->name, RSA_SHA2_256_NAME, RSA_SHA2_256_NAME_LEN + 1);
	return deuce_ssh_transport_register_key_algo(ka);
}
