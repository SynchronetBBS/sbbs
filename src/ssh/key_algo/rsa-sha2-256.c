// RFC 8332: rsa-sha2-256 Host Key Algorithm

#include <openssl/core_names.h>
#include <openssl/evp.h>
#include <openssl/param_build.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <stdlib.h>
#include <string.h>

#include "deucessh.h"

#define RSA_SHA2_256_NAME     "rsa-sha2-256"
#define RSA_SHA2_256_NAME_LEN 12

/* The SSH wire format uses "ssh-rsa" as the key type name,
 * even for rsa-sha2-256 signatures (RFC 8332 s3). */
#define RSA_KEY_TYPE_NAME     "ssh-rsa"
#define RSA_KEY_TYPE_NAME_LEN 7

struct cbdata {
	EVP_PKEY *pkey;
};

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

/*
 * Sign data with RSASSA-PKCS1-v1_5 + SHA-256.
 * Output is SSH wire format:
 *   string  "rsa-sha2-256"
 *   string  <raw signature bytes>
 */
static int
sign(uint8_t *buf, size_t bufsz, size_t *outlen,
    const uint8_t *data, size_t data_len, deuce_ssh_key_algo_ctx *ctx)
{
	struct cbdata *cbd = (struct cbdata *)ctx;
	if (cbd == NULL || cbd->pkey == NULL)
		return DEUCE_SSH_ERROR_INIT;

	EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
	if (mdctx == NULL)
		return DEUCE_SSH_ERROR_ALLOC;

	EVP_PKEY_CTX *pctx = NULL;
	if (EVP_DigestSignInit(mdctx, &pctx, EVP_sha256(), NULL, cbd->pkey) != 1) {
		EVP_MD_CTX_free(mdctx);
		return DEUCE_SSH_ERROR_INIT;
	}
	if (EVP_PKEY_CTX_set_rsa_padding(pctx, RSA_PKCS1_PADDING) != 1) {
		EVP_MD_CTX_free(mdctx);
		return DEUCE_SSH_ERROR_INIT;
	}

	/* Determine signature size */
	size_t siglen = 0;
	if (EVP_DigestSign(mdctx, NULL, &siglen, data, data_len) != 1) {
		EVP_MD_CTX_free(mdctx);
		return DEUCE_SSH_ERROR_INIT;
	}

	size_t needed = 4 + RSA_SHA2_256_NAME_LEN + 4 + siglen;
	if (bufsz < needed) {
		EVP_MD_CTX_free(mdctx);
		return DEUCE_SSH_ERROR_TOOLONG;
	}

	uint8_t *raw_sig = malloc(siglen);
	if (raw_sig == NULL) {
		EVP_MD_CTX_free(mdctx);
		return DEUCE_SSH_ERROR_ALLOC;
	}

	if (EVP_DigestSign(mdctx, raw_sig, &siglen, data, data_len) != 1) {
		free(raw_sig);
		EVP_MD_CTX_free(mdctx);
		return DEUCE_SSH_ERROR_INIT;
	}
	EVP_MD_CTX_free(mdctx);

	/* Serialize SSH signature blob */
	size_t pos = 0;
	deuce_ssh_serialize_uint32(RSA_SHA2_256_NAME_LEN, buf, bufsz, &pos);
	memcpy(&buf[pos], RSA_SHA2_256_NAME, RSA_SHA2_256_NAME_LEN);
	pos += RSA_SHA2_256_NAME_LEN;
	deuce_ssh_serialize_uint32((uint32_t)siglen, buf, bufsz, &pos);
	memcpy(&buf[pos], raw_sig, siglen);
	pos += siglen;

	free(raw_sig);
	*outlen = pos;
	return 0;
}

/*
 * Write the SSH-format public key blob:
 *   string  "ssh-rsa"
 *   string  e (public exponent)
 *   string  n (modulus)
 */
static int
pubkey(uint8_t *buf, size_t bufsz, size_t *outlen, deuce_ssh_key_algo_ctx *ctx)
{
	struct cbdata *cbd = (struct cbdata *)ctx;
	if (cbd == NULL || cbd->pkey == NULL)
		return DEUCE_SSH_ERROR_INIT;

	BIGNUM *e_bn = NULL, *n_bn = NULL;
	if (EVP_PKEY_get_bn_param(cbd->pkey, OSSL_PKEY_PARAM_RSA_E, &e_bn) != 1 ||
	    EVP_PKEY_get_bn_param(cbd->pkey, OSSL_PKEY_PARAM_RSA_N, &n_bn) != 1) {
		BN_free(e_bn);
		BN_free(n_bn);
		return DEUCE_SSH_ERROR_INIT;
	}

	int e_bytes = BN_num_bytes(e_bn);
	int n_bytes = BN_num_bytes(n_bn);
	uint8_t *e_buf = malloc(e_bytes);
	uint8_t *n_buf = malloc(n_bytes);
	if (!e_buf || !n_buf) {
		free(e_buf); free(n_buf);
		BN_free(e_bn); BN_free(n_bn);
		return DEUCE_SSH_ERROR_ALLOC;
	}
	BN_bn2bin(e_bn, e_buf);
	BN_bn2bin(n_bn, n_buf);

	/* mpint encoding: add leading zero if MSB is set */
	bool e_pad = (e_bytes > 0 && (e_buf[0] & 0x80));
	bool n_pad = (n_bytes > 0 && (n_buf[0] & 0x80));
	uint32_t e_wire = e_bytes + (e_pad ? 1 : 0);
	uint32_t n_wire = n_bytes + (n_pad ? 1 : 0);

	size_t needed = 4 + RSA_KEY_TYPE_NAME_LEN + 4 + e_wire + 4 + n_wire;
	if (bufsz < needed) {
		free(e_buf); free(n_buf);
		BN_free(e_bn); BN_free(n_bn);
		return DEUCE_SSH_ERROR_TOOLONG;
	}

	size_t pos = 0;
	deuce_ssh_serialize_uint32(RSA_KEY_TYPE_NAME_LEN, buf, bufsz, &pos);
	memcpy(&buf[pos], RSA_KEY_TYPE_NAME, RSA_KEY_TYPE_NAME_LEN);
	pos += RSA_KEY_TYPE_NAME_LEN;

	deuce_ssh_serialize_uint32(e_wire, buf, bufsz, &pos);
	if (e_pad)
		buf[pos++] = 0;
	memcpy(&buf[pos], e_buf, e_bytes);
	pos += e_bytes;

	deuce_ssh_serialize_uint32(n_wire, buf, bufsz, &pos);
	if (n_pad)
		buf[pos++] = 0;
	memcpy(&buf[pos], n_buf, n_bytes);
	pos += n_bytes;

	free(e_buf); free(n_buf);
	BN_free(e_bn); BN_free(n_bn);

	*outlen = pos;
	return 0;
}

static int
haskey(deuce_ssh_key_algo_ctx *ctx)
{
	struct cbdata *cbd = (struct cbdata *)ctx;
	return (cbd != NULL && cbd->pkey != NULL);
}

static void
cleanup(deuce_ssh_key_algo_ctx *ctx)
{
	struct cbdata *cbd = (struct cbdata *)ctx;
	if (cbd != NULL) {
		EVP_PKEY_free(cbd->pkey);
		free(cbd);
	}
}

DEUCE_SSH_PUBLIC int
rsa_sha2_256_load_key_file(deuce_ssh_session sess, const char *path)
{
	FILE *fp = fopen(path, "r");
	if (fp == NULL)
		return DEUCE_SSH_ERROR_INIT;

	EVP_PKEY *pkey = PEM_read_PrivateKey(fp, NULL, NULL, NULL);
	fclose(fp);
	if (pkey == NULL)
		return DEUCE_SSH_ERROR_INIT;

	if (EVP_PKEY_id(pkey) != EVP_PKEY_RSA) {
		EVP_PKEY_free(pkey);
		return DEUCE_SSH_ERROR_INVALID;
	}

	struct cbdata *cbd = (struct cbdata *)sess->trans.key_algo_ctx;
	if (cbd == NULL) {
		cbd = calloc(1, sizeof(*cbd));
		if (cbd == NULL) {
			EVP_PKEY_free(pkey);
			return DEUCE_SSH_ERROR_ALLOC;
		}
		sess->trans.key_algo_ctx = (deuce_ssh_key_algo_ctx *)cbd;
	}
	else {
		EVP_PKEY_free(cbd->pkey);
	}

	cbd->pkey = pkey;
	return 0;
}

DEUCE_SSH_PUBLIC int
rsa_sha2_256_generate_key(deuce_ssh_session sess, unsigned int bits)
{
	EVP_PKEY *pkey = NULL;
	EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new_from_name(NULL, "RSA", NULL);
	if (ctx == NULL)
		return DEUCE_SSH_ERROR_ALLOC;

	OSSL_PARAM params[] = {
		OSSL_PARAM_construct_uint("bits", &bits),
		OSSL_PARAM_construct_end(),
	};

	if (EVP_PKEY_keygen_init(ctx) != 1 ||
	    EVP_PKEY_CTX_set_params(ctx, params) != 1 ||
	    EVP_PKEY_keygen(ctx, &pkey) != 1) {
		EVP_PKEY_CTX_free(ctx);
		return DEUCE_SSH_ERROR_INIT;
	}
	EVP_PKEY_CTX_free(ctx);

	struct cbdata *cbd = (struct cbdata *)sess->trans.key_algo_ctx;
	if (cbd == NULL) {
		cbd = calloc(1, sizeof(*cbd));
		if (cbd == NULL) {
			EVP_PKEY_free(pkey);
			return DEUCE_SSH_ERROR_ALLOC;
		}
		sess->trans.key_algo_ctx = (deuce_ssh_key_algo_ctx *)cbd;
	}
	else {
		EVP_PKEY_free(cbd->pkey);
	}

	cbd->pkey = pkey;
	return 0;
}

DEUCE_SSH_PUBLIC int
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
