// RFC 8332: rsa-sha2-512 Host Key Algorithm

#include <openssl/core_names.h>
#include <openssl/evp.h>
#include <openssl/param_build.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>

#include <stdlib.h>
#include <string.h>

#include "deucessh-algorithms.h"
#include "deucessh-crypto.h"
#include "deucessh-key-algo.h"
#include "deucessh.h"
#ifdef DSSH_TESTING
 #include "ssh-internal.h"
#endif

#define RSA_SHA2_512_NAME     "rsa-sha2-512"
#define RSA_SHA2_512_NAME_LEN 12

/* The SSH wire format uses "ssh-rsa" as the key type name,
 * even for rsa-sha2-512 signatures (RFC 8332 s3). */
#define RSA_KEY_TYPE_NAME     "ssh-rsa"
#define RSA_KEY_TYPE_NAME_LEN 7

struct cbdata {
	EVP_PKEY *pkey;
	uint8_t  *pub_blob;
	size_t    pub_blob_len;
};

static struct cbdata *rsa_ctx;

/*
 * Verify a signature over data using an RSA public host key blob.
 *
 * key_blob: SSH wire format:
 *   string  "ssh-rsa"
 *   string  e (public exponent, unsigned big-endian)
 *   string  n (modulus, unsigned big-endian)
 *
 * sig_blob: SSH wire format:
 *   string  "rsa-sha2-512"
 *   string  <RSASSA-PKCS1-v1_5 signature bytes>
 */
static int
verify(const uint8_t *key_blob, size_t key_blob_len, const uint8_t *sig_blob, size_t sig_blob_len,
    const uint8_t *data, size_t data_len)
{
	int             result = DSSH_ERROR_INIT;
	BIGNUM         *e_bn   = NULL;
	BIGNUM         *n_bn   = NULL;
	OSSL_PARAM_BLD *bld    = NULL;
	OSSL_PARAM     *params = NULL;
	EVP_PKEY_CTX   *pctx   = NULL;
	EVP_PKEY       *pkey   = NULL;
	EVP_MD_CTX     *mdctx  = NULL;

	/* Parse public key from key_blob */
	size_t   kp = 0;
	uint32_t slen;

	/* Validate algorithm name ("ssh-rsa") */
	if (dssh_parse_uint32(&key_blob[kp], key_blob_len - kp, &slen) < 4 || kp + 4 + slen > key_blob_len)
		return DSSH_ERROR_PARSE;
	kp += 4;
	if (slen != RSA_KEY_TYPE_NAME_LEN || memcmp(&key_blob[kp], RSA_KEY_TYPE_NAME, RSA_KEY_TYPE_NAME_LEN) != 0)
		return DSSH_ERROR_INVALID;
	kp += slen;

	/* e (public exponent) */
	if (dssh_parse_uint32(&key_blob[kp], key_blob_len - kp, &slen) < 4 || kp + 4 + slen > key_blob_len)
		return DSSH_ERROR_PARSE;
	kp += 4;
	if (slen > INT_MAX)
		return DSSH_ERROR_INVALID;
	int e_len = (int)slen;
	e_bn      = BN_bin2bn(&key_blob[kp], e_len, NULL);
	kp += slen;

	/* n (modulus) */
	if (dssh_parse_uint32(&key_blob[kp], key_blob_len - kp, &slen) < 4 || kp + 4 + slen > key_blob_len) {
		result = DSSH_ERROR_PARSE;
		goto done;
	}
	kp += 4;
	if (slen > INT_MAX) {
		result = DSSH_ERROR_INVALID;
		goto done;
	}
	int n_len = (int)slen;
	n_bn      = BN_bin2bn(&key_blob[kp], n_len, NULL);
	kp += slen;

	if (e_bn == NULL || n_bn == NULL)
		goto done;
	if (kp != key_blob_len) {
		result = DSSH_ERROR_PARSE;
		goto done;
	}

	/* Build RSA public key via EVP */
	bld = OSSL_PARAM_BLD_new();
	if (bld == NULL)
		goto done;
	if (OSSL_PARAM_BLD_push_BN(bld, OSSL_PKEY_PARAM_RSA_N, n_bn) != 1
	    || OSSL_PARAM_BLD_push_BN(bld, OSSL_PKEY_PARAM_RSA_E, e_bn) != 1)
		goto done;
	params = OSSL_PARAM_BLD_to_param(bld);
	if (params == NULL)
		goto done;

	pctx = EVP_PKEY_CTX_new_from_name(NULL, "RSA", NULL);
	if (pctx == NULL)
		goto done;
	if (EVP_PKEY_fromdata_init(pctx) != 1 || EVP_PKEY_fromdata(pctx, &pkey, EVP_PKEY_PUBLIC_KEY, params) != 1)
		goto done;

	/* Parse signature from sig_blob */
	size_t sp = 0;

	/* Validate algorithm name ("rsa-sha2-512") */
	if (dssh_parse_uint32(&sig_blob[sp], sig_blob_len - sp, &slen) < 4 || sp + 4 + slen > sig_blob_len) {
		result = DSSH_ERROR_PARSE;
		goto done;
	}
	sp += 4;
	if (slen != RSA_SHA2_512_NAME_LEN
	    || memcmp(&sig_blob[sp], RSA_SHA2_512_NAME, RSA_SHA2_512_NAME_LEN) != 0) {
		result = DSSH_ERROR_INVALID;
		goto done;
	}
	sp += slen;

	/* Raw signature bytes */
	uint32_t raw_sig_len;
	if (dssh_parse_uint32(&sig_blob[sp], sig_blob_len - sp, &raw_sig_len) < 4
	    || sp + 4 + raw_sig_len > sig_blob_len) {
		result = DSSH_ERROR_PARSE;
		goto done;
	}
	sp += 4;
	const uint8_t *raw_sig = &sig_blob[sp];
	sp += raw_sig_len;
	if (sp != sig_blob_len) {
		result = DSSH_ERROR_PARSE;
		goto done;
	}

	/* Verify RSASSA-PKCS1-v1_5 with SHA-512 */
	mdctx = EVP_MD_CTX_new();
	if (mdctx == NULL)
		goto done;

	EVP_PKEY_CTX *vctx = NULL;
	if (EVP_DigestVerifyInit(mdctx, &vctx, EVP_sha512(), NULL, pkey) != 1)
		goto done;
	if (EVP_PKEY_CTX_set_rsa_padding(vctx, RSA_PKCS1_PADDING) != 1)
		goto done;

	if (EVP_DigestVerify(mdctx, raw_sig, raw_sig_len, data, data_len) == 1)
		result = 0;
	else
		result = DSSH_ERROR_INVALID;

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
 * Sign data with RSASSA-PKCS1-v1_5 + SHA-512.
 * Output is SSH wire format:
 *   string  "rsa-sha2-512"
 *   string  <raw signature bytes>
 */
static int
sign(uint8_t **out, size_t *outlen, const uint8_t *data, size_t data_len, dssh_key_algo_ctx *ctx)
{
	/* cbd->pkey always set by keygen/load before sign is callable. */
	struct cbdata *cbd = (struct cbdata *)ctx;
#ifdef DSSH_TESTING
	if (cbd == NULL)
#else
	if (cbd == NULL || cbd->pkey == NULL)
#endif
		return DSSH_ERROR_INIT;

	EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
	if (mdctx == NULL)
		return DSSH_ERROR_ALLOC;

	EVP_PKEY_CTX *pctx = NULL;
	if (EVP_DigestSignInit(mdctx, &pctx, EVP_sha512(), NULL, cbd->pkey) != 1) {
		EVP_MD_CTX_free(mdctx);
		return DSSH_ERROR_INIT;
	}
	if (EVP_PKEY_CTX_set_rsa_padding(pctx, RSA_PKCS1_PADDING) != 1) {
		EVP_MD_CTX_free(mdctx);
		return DSSH_ERROR_INIT;
	}

	/* Determine signature size */
	size_t siglen = 0;
	if (EVP_DigestSign(mdctx, NULL, &siglen, data, data_len) != 1) {
		EVP_MD_CTX_free(mdctx);
		return DSSH_ERROR_INIT;
	}

	if (siglen > SIZE_MAX - 20) { /* 4 + 12 + 4 */
		EVP_MD_CTX_free(mdctx);
		return DSSH_ERROR_INVALID;
	}
	size_t needed = 20 + siglen;

	uint8_t *raw_sig = malloc(siglen);
	if (raw_sig == NULL) {
		EVP_MD_CTX_free(mdctx);
		return DSSH_ERROR_ALLOC;
	}

	if (EVP_DigestSign(mdctx, raw_sig, &siglen, data, data_len) != 1) {
		free(raw_sig);
		EVP_MD_CTX_free(mdctx);
		return DSSH_ERROR_INIT;
	}
	EVP_MD_CTX_free(mdctx);
	if (siglen > UINT32_MAX) {
		free(raw_sig);
		return DSSH_ERROR_INVALID;
	}

	/* Serialize SSH signature blob */
	uint8_t *buf = malloc(needed);
	if (buf == NULL) {
		free(raw_sig);
		return DSSH_ERROR_ALLOC;
	}

	size_t pos  = 0;
	int    sret = dssh_serialize_uint32(RSA_SHA2_512_NAME_LEN, buf, needed, &pos);
	if (sret < 0) {
		free(buf);
		free(raw_sig);
		return sret;
	}
	memcpy(&buf[pos], RSA_SHA2_512_NAME, RSA_SHA2_512_NAME_LEN);
	pos += RSA_SHA2_512_NAME_LEN;
	sret = dssh_serialize_uint32((uint32_t)siglen, buf, needed, &pos);
	if (sret < 0) {
		free(buf);
		free(raw_sig);
		return sret;
	}
	memcpy(&buf[pos], raw_sig, siglen);
	pos += siglen;

	free(raw_sig);
	*out    = buf;
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
pubkey(const uint8_t **out, size_t *outlen, dssh_key_algo_ctx *ctx)
{
	/* cbd->pkey always set before pubkey is callable. */
	struct cbdata *cbd = (struct cbdata *)ctx;
#ifdef DSSH_TESTING
	if (cbd == NULL)
#else
	if (cbd == NULL || cbd->pkey == NULL)
#endif
		return DSSH_ERROR_INIT;

	/* Return cached blob if already computed */
	if (cbd->pub_blob != NULL) {
		*out    = cbd->pub_blob;
		*outlen = cbd->pub_blob_len;
		return 0;
	}

	BIGNUM *e_bn = NULL, *n_bn = NULL;
	if (EVP_PKEY_get_bn_param(cbd->pkey, OSSL_PKEY_PARAM_RSA_E, &e_bn) != 1
	    || EVP_PKEY_get_bn_param(cbd->pkey, OSSL_PKEY_PARAM_RSA_N, &n_bn) != 1) {
		BN_free(e_bn);
		BN_free(n_bn);
		return DSSH_ERROR_INIT;
	}

	int e_bytes = BN_num_bytes(e_bn);
	int n_bytes = BN_num_bytes(n_bn);

	if (e_bytes < 0 || n_bytes < 0) {
		BN_free(e_bn);
		BN_free(n_bn);
		return DSSH_ERROR_PARSE;
	}
#if INT_MAX > SIZE_MAX
	if (e_bytes > SIZE_MAX || n_bytes > SIZE_MAX) {
		BN_free(e_bn);
		BN_free(n_bn);
		return DSSH_ERROR_INVALID;
	}
#endif
	size_t e_sz = (size_t)e_bytes;
	size_t n_sz = (size_t)n_bytes;

	uint8_t *e_buf = malloc(e_sz);
	if (!e_buf) {
		BN_free(e_bn);
		BN_free(n_bn);
		return DSSH_ERROR_ALLOC;
	}
	uint8_t *n_buf = malloc(n_sz);
	if (!n_buf) {
		free(e_buf);
		BN_free(e_bn);
		BN_free(n_bn);
		return DSSH_ERROR_ALLOC;
	}
	BN_bn2bin(e_bn, e_buf);
	BN_bn2bin(n_bn, n_buf);

	/* mpint encoding: add leading zero if MSB is set.
	 * RSA public exponent is typically 65537 (0x010001), so e_pad
	 * is never true in practice. */
#ifdef DSSH_TESTING
	bool e_pad = (e_buf[0] & DSSH_MPINT_SIGN_BIT);
#else
	bool e_pad = (e_bytes > 0 && (e_buf[0] & DSSH_MPINT_SIGN_BIT));
#endif
	bool n_pad = (n_bytes > 0 && (n_buf[0] & DSSH_MPINT_SIGN_BIT));

	if (e_sz > UINT32_MAX - 1 || n_sz > UINT32_MAX - 1) {
		free(e_buf);
		free(n_buf);
		BN_free(e_bn);
		BN_free(n_bn);
		return DSSH_ERROR_INVALID;
	}
	uint32_t e_u32  = (uint32_t)e_sz;
	uint32_t n_u32  = (uint32_t)n_sz;
	uint32_t e_wire = e_u32 + (e_pad ? 1 : 0);
	uint32_t n_wire = n_u32 + (n_pad ? 1 : 0);

	if ((size_t)e_wire + n_wire > SIZE_MAX - 19) {
		free(e_buf);
		free(n_buf);
		BN_free(e_bn);
		BN_free(n_bn);
		return DSSH_ERROR_INVALID;
	}
	size_t needed = 19 + e_wire + n_wire;

	uint8_t *buf = malloc(needed);
	if (buf == NULL) {
		free(e_buf);
		free(n_buf);
		BN_free(e_bn);
		BN_free(n_bn);
		return DSSH_ERROR_ALLOC;
	}

	size_t pos = 0;
	int    sret;

#define PK_SER(v)                                                     \
	do {                                                          \
		sret = dssh_serialize_uint32((v), buf, needed, &pos); \
		if (sret < 0) {                                       \
			free(buf);                                    \
			free(e_buf);                                  \
			free(n_buf);                                  \
			BN_free(e_bn);                                \
			BN_free(n_bn);                                \
			return sret;                                  \
		}                                                     \
	} while (0)

	PK_SER(RSA_KEY_TYPE_NAME_LEN);
	memcpy(&buf[pos], RSA_KEY_TYPE_NAME, RSA_KEY_TYPE_NAME_LEN);
	pos += RSA_KEY_TYPE_NAME_LEN;

	PK_SER(e_wire);
	if (e_pad)
		buf[pos++] = 0;
	memcpy(&buf[pos], e_buf, e_sz);
	pos += e_sz;

	PK_SER(n_wire);
	if (n_pad)
		buf[pos++] = 0;
	memcpy(&buf[pos], n_buf, n_sz);
	pos += n_sz;

#undef PK_SER

	free(e_buf);
	free(n_buf);
	BN_free(e_bn);
	BN_free(n_bn);

	cbd->pub_blob     = buf;
	cbd->pub_blob_len = pos;
	*out              = buf;
	*outlen           = pos;
	return 0;
}

static int
haskey(dssh_key_algo_ctx *ctx)
{
	/* Only RSA keys stored in this module's ctx. */
	struct cbdata *cbd = (struct cbdata *)ctx;
	return (cbd != NULL && cbd->pkey != NULL && EVP_PKEY_is_a(cbd->pkey, "RSA"));
}

static void
cleanup(dssh_key_algo_ctx *ctx)
{
	struct cbdata *cbd = (struct cbdata *)ctx;

	if (cbd != NULL) {
		EVP_PKEY_free(cbd->pkey);
		free(cbd->pub_blob);
		free(cbd);
	}
	if (cbd == rsa_ctx)
		rsa_ctx = NULL;
}

DSSH_PUBLIC int
dssh_rsa_sha2_512_load_key_file(const char *path, dssh_pem_password_cb pw_cb, void *pw_cbdata)
{
	if (path == NULL)
		return DSSH_ERROR_INIT;
	FILE *fp = fopen(path, "r");

	if (fp == NULL)
		return DSSH_ERROR_INIT;

	EVP_PKEY *pkey = PEM_read_PrivateKey(fp, NULL, pw_cb, pw_cbdata);

	fclose(fp);
	if (pkey == NULL)
		return DSSH_ERROR_INIT;

	if (!EVP_PKEY_is_a(pkey, "RSA")) {
		EVP_PKEY_free(pkey);
		return DSSH_ERROR_INVALID;
	}

	if (rsa_ctx == NULL) {
		rsa_ctx = calloc(1, sizeof(*rsa_ctx));
		if (rsa_ctx == NULL) {
			EVP_PKEY_free(pkey);
			return DSSH_ERROR_ALLOC;
		}
		int res = dssh_key_algo_set_ctx(RSA_SHA2_512_NAME, rsa_ctx);

		if (res < 0) {
			free(rsa_ctx);
			rsa_ctx = NULL;
			EVP_PKEY_free(pkey);
			return res;
		}
	}
	else {
		EVP_PKEY_free(rsa_ctx->pkey);
		free(rsa_ctx->pub_blob);
		rsa_ctx->pub_blob     = NULL;
		rsa_ctx->pub_blob_len = 0;
	}

	rsa_ctx->pkey = pkey;
	return 0;
}

DSSH_PUBLIC int
dssh_rsa_sha2_512_save_key_file(const char *path, dssh_pem_password_cb pw_cb, void *pw_cbdata)
{
	if (path == NULL)
		return DSSH_ERROR_INIT;
#ifdef DSSH_TESTING
	if (rsa_ctx == NULL)
#else
	if (rsa_ctx == NULL || rsa_ctx->pkey == NULL)
#endif
		return DSSH_ERROR_INIT;

	FILE *fp = fopen(path, "w");

	if (fp == NULL)
		return DSSH_ERROR_INIT;

	EVP_CIPHER *cipher = NULL;

	if (pw_cb != NULL) {
		cipher = EVP_CIPHER_fetch(NULL, "AES-256-CBC", NULL);
		if (cipher == NULL) {
			fclose(fp);
			return DSSH_ERROR_INIT;
		}
	}
	int ok = PEM_write_PrivateKey(fp, rsa_ctx->pkey, cipher, NULL, 0, pw_cb, pw_cbdata);

	EVP_CIPHER_free(cipher);
	if (fclose(fp) != 0)
		ok = 0;
	return ok == 1 ? 0 : DSSH_ERROR_INIT;
}

DSSH_PUBLIC int64_t
dssh_rsa_sha2_512_get_pub_str(char *buf, size_t bufsz)
{
	const uint8_t *blob;
	size_t         blob_len;
	int            res = pubkey(&blob, &blob_len, (dssh_key_algo_ctx *)rsa_ctx);
	if (res < 0)
		return res;

	size_t b64_len = 4 * ((blob_len + 2) / 3);
	/* "ssh-rsa " + base64 + NUL */
	size_t needed = RSA_KEY_TYPE_NAME_LEN + 1 + b64_len + 1;

	if (buf == NULL || bufsz == 0)
		return (int64_t)needed;
	if (bufsz < needed)
		return DSSH_ERROR_TOOLONG;

	memcpy(buf, RSA_KEY_TYPE_NAME, RSA_KEY_TYPE_NAME_LEN);
	buf[RSA_KEY_TYPE_NAME_LEN] = ' ';
	EVP_EncodeBlock((unsigned char *)&buf[RSA_KEY_TYPE_NAME_LEN + 1], blob, (int)blob_len);
	return (int64_t)needed;
}

DSSH_PUBLIC int
dssh_rsa_sha2_512_save_pub_file(const char *path)
{
	if (path == NULL)
		return DSSH_ERROR_INIT;
	char   *str = NULL;
	int64_t len = dssh_rsa_sha2_512_get_pub_str(NULL, 0);
	if (len < 0)
		return (int)len;
	str = malloc((size_t)len);
	if (str == NULL)
		return DSSH_ERROR_ALLOC;
	int64_t res = dssh_rsa_sha2_512_get_pub_str(str, (size_t)len);
	if (res < 0) {
		free(str);
		return (int)res;
	}

	FILE *fp = fopen(path, "w");
	if (fp == NULL) {
		free(str);
		return DSSH_ERROR_INIT;
	}
	int wok = fprintf(fp, "%s\n", str) >= 0;
	if (fclose(fp) != 0)
		wok = 0;
	free(str);
	return wok ? 0 : DSSH_ERROR_INIT;
}

DSSH_PUBLIC int
dssh_rsa_sha2_512_generate_key(unsigned int bits)
{
	EVP_PKEY     *pkey = NULL;
	EVP_PKEY_CTX *pctx = EVP_PKEY_CTX_new_from_name(NULL, "RSA", NULL);

	if (pctx == NULL)
		return DSSH_ERROR_ALLOC;

	OSSL_PARAM params[] = {
	    OSSL_PARAM_construct_uint("bits", &bits),
	    OSSL_PARAM_construct_end(),
	};

	if (EVP_PKEY_keygen_init(pctx) != 1 || EVP_PKEY_CTX_set_params(pctx, params) != 1
	    || EVP_PKEY_keygen(pctx, &pkey) != 1) {
		EVP_PKEY_CTX_free(pctx);
		return DSSH_ERROR_INIT;
	}
	EVP_PKEY_CTX_free(pctx);

	if (rsa_ctx == NULL) {
		rsa_ctx = calloc(1, sizeof(*rsa_ctx));
		if (rsa_ctx == NULL) {
			EVP_PKEY_free(pkey);
			return DSSH_ERROR_ALLOC;
		}
		int res = dssh_key_algo_set_ctx(RSA_SHA2_512_NAME, rsa_ctx);

		if (res < 0) {
			free(rsa_ctx);
			rsa_ctx = NULL;
			EVP_PKEY_free(pkey);
			return res;
		}
	}
	else {
		EVP_PKEY_free(rsa_ctx->pkey);
		free(rsa_ctx->pub_blob);
		rsa_ctx->pub_blob     = NULL;
		rsa_ctx->pub_blob_len = 0;
	}

	rsa_ctx->pkey = pkey;
	return 0;
}

DSSH_PUBLIC int
dssh_register_rsa_sha2_512(void)
{
	struct dssh_key_algo_s *ka = malloc(sizeof(*ka) + RSA_SHA2_512_NAME_LEN + 1);
	if (ka == NULL)
		return DSSH_ERROR_ALLOC;
	ka->next    = NULL;
	ka->sign    = sign;
	ka->verify  = verify;
	ka->pubkey  = pubkey;
	ka->haskey  = haskey;
	ka->cleanup = cleanup;
	ka->ctx     = NULL;
	ka->flags   = DSSH_KEY_ALGO_FLAG_SIGNATURE_CAPABLE;
	memcpy(ka->name, RSA_SHA2_512_NAME, RSA_SHA2_512_NAME_LEN + 1);
	return dssh_transport_register_key_algo(ka);
}
