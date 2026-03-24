#include <openssl/evp.h>
#include <openssl/pem.h>
#include <stdlib.h>
#include <string.h>

#include "deucessh.h"
#include "ssh-trans.h"
#include "ssh-internal.h"

#define ED25519_RAW_PUB_LEN  32
#define ED25519_RAW_SIG_LEN  64
#define ED25519_NAME         "ssh-ed25519"
#define ED25519_NAME_LEN     11

struct cbdata {
	EVP_PKEY *pkey;
};

static int
sign(uint8_t *buf, size_t bufsz, size_t *outlen,
    const uint8_t *data, size_t data_len, dssh_key_algo_ctx *ctx)
{
	struct cbdata *cbd = (struct cbdata *)ctx;
	/* cbd->pkey is always set by generate_key/load_key_file
	 * before sign is callable via the KEX/auth path. */
#ifdef DSSH_TESTING
	if (cbd == NULL)
#else
	if (cbd == NULL || cbd->pkey == NULL)
#endif
		return DSSH_ERROR_INIT;

	size_t needed = 4 + ED25519_NAME_LEN + 4 + ED25519_RAW_SIG_LEN;
	if (bufsz < needed)
		return DSSH_ERROR_TOOLONG;

	EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
	if (mdctx == NULL)
		return DSSH_ERROR_ALLOC;

	if (EVP_DigestSignInit(mdctx, NULL, NULL, NULL, cbd->pkey) != 1) {
		EVP_MD_CTX_free(mdctx);
		return DSSH_ERROR_INIT;
	}

	uint8_t raw_sig[ED25519_RAW_SIG_LEN];
	size_t siglen = sizeof(raw_sig);
	if (EVP_DigestSign(mdctx, raw_sig, &siglen, data, data_len) != 1) {
		EVP_MD_CTX_free(mdctx);
		return DSSH_ERROR_INIT;
	}
	EVP_MD_CTX_free(mdctx);

	/* Serialize SSH signature blob */
	size_t pos = 0;
	dssh_serialize_uint32(ED25519_NAME_LEN, buf, bufsz, &pos);
	memcpy(&buf[pos], ED25519_NAME, ED25519_NAME_LEN);
	pos += ED25519_NAME_LEN;
	dssh_serialize_uint32((uint32_t)siglen, buf, bufsz, &pos);
	memcpy(&buf[pos], raw_sig, siglen);
	pos += siglen;

	*outlen = pos;
	return 0;
}

/*
 * Write the SSH-format public key blob to buf:
 *   string  "ssh-ed25519"
 *   string  <32-byte raw public key>
 *
 * Returns 0 on success, negative error code on failure.
 */
static int
pubkey(uint8_t *buf, size_t bufsz, size_t *outlen, dssh_key_algo_ctx *ctx)
{
	struct cbdata *cbd = (struct cbdata *)ctx;
	/* cbd->pkey always set before pubkey is callable. */
#ifdef DSSH_TESTING
	if (cbd == NULL)
#else
	if (cbd == NULL || cbd->pkey == NULL)
#endif
		return DSSH_ERROR_INIT;

	uint8_t raw_pub[ED25519_RAW_PUB_LEN];
	size_t raw_pub_len = sizeof(raw_pub);
	if (EVP_PKEY_get_raw_public_key(cbd->pkey, raw_pub, &raw_pub_len) != 1)
		return DSSH_ERROR_INIT;

	size_t needed = 4 + ED25519_NAME_LEN + 4 + raw_pub_len;
	if (bufsz < needed)
		return DSSH_ERROR_TOOLONG;

	size_t pos = 0;
	dssh_serialize_uint32(ED25519_NAME_LEN, buf, bufsz, &pos);
	memcpy(&buf[pos], ED25519_NAME, ED25519_NAME_LEN);
	pos += ED25519_NAME_LEN;
	dssh_serialize_uint32((uint32_t)raw_pub_len, buf, bufsz, &pos);
	memcpy(&buf[pos], raw_pub, raw_pub_len);
	pos += raw_pub_len;

	*outlen = pos;
	return 0;
}

/*
 * Verify a signature over data using a public host key blob.
 * key_blob: SSH wire format (string "ssh-ed25519" + string <32-byte key>)
 * sig_blob: SSH wire format (string "ssh-ed25519" + string <64-byte sig>)
 * data/data_len: the data that was signed (exchange hash)
 */
static int
verify(const uint8_t *key_blob, size_t key_blob_len,
    const uint8_t *sig_blob, size_t sig_blob_len,
    const uint8_t *data, size_t data_len)
{
	/* Parse public key from key_blob:
	 * string algo_name, string raw_public_key */
	size_t kp = 0;
	uint32_t algo_len;
	if (dssh_parse_uint32(&key_blob[kp], key_blob_len, &algo_len) < 4 ||
	    kp + 4 + algo_len > key_blob_len)
		return DSSH_ERROR_PARSE;
	kp += 4;
	if (algo_len != ED25519_NAME_LEN ||
	    memcmp(&key_blob[kp], ED25519_NAME, ED25519_NAME_LEN) != 0)
		return DSSH_ERROR_INVALID;
	kp += algo_len;
	uint32_t raw_pub_len;
	if (dssh_parse_uint32(&key_blob[kp], key_blob_len - kp, &raw_pub_len) < 4 ||
	    kp + 4 + raw_pub_len > key_blob_len)
		return DSSH_ERROR_PARSE;
	kp += 4;
	const uint8_t *raw_pub = &key_blob[kp];
	kp += raw_pub_len;

	if (raw_pub_len != ED25519_RAW_PUB_LEN)
		return DSSH_ERROR_INVALID;
	if (kp != key_blob_len)
		return DSSH_ERROR_PARSE;

	/* Parse signature from sig_blob:
	 * string algo_name, string raw_signature */
	size_t sp = 0;
	uint32_t sig_algo_len;
	if (dssh_parse_uint32(&sig_blob[sp], sig_blob_len, &sig_algo_len) < 4 ||
	    sp + 4 + sig_algo_len > sig_blob_len)
		return DSSH_ERROR_PARSE;
	sp += 4;
	if (sig_algo_len != ED25519_NAME_LEN ||
	    memcmp(&sig_blob[sp], ED25519_NAME, ED25519_NAME_LEN) != 0)
		return DSSH_ERROR_INVALID;
	sp += sig_algo_len;
	uint32_t raw_sig_len;
	if (dssh_parse_uint32(&sig_blob[sp], sig_blob_len - sp, &raw_sig_len) < 4 ||
	    sp + 4 + raw_sig_len > sig_blob_len)
		return DSSH_ERROR_PARSE;
	sp += 4;
	const uint8_t *raw_sig = &sig_blob[sp];
	sp += raw_sig_len;

	if (raw_sig_len != ED25519_RAW_SIG_LEN)
		return DSSH_ERROR_INVALID;
	if (sp != sig_blob_len)
		return DSSH_ERROR_PARSE;

	EVP_PKEY *pkey = EVP_PKEY_new_raw_public_key(
	    EVP_PKEY_ED25519, NULL, raw_pub, raw_pub_len);
	if (pkey == NULL)
		return DSSH_ERROR_INIT;

	EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
	if (mdctx == NULL) {
		EVP_PKEY_free(pkey);
		return DSSH_ERROR_ALLOC;
	}

	int result;
	if (EVP_DigestVerifyInit(mdctx, NULL, NULL, NULL, pkey) != 1 ||
	    EVP_DigestVerify(mdctx, raw_sig, raw_sig_len, data, data_len) != 1)
		result = DSSH_ERROR_INVALID;
	else
		result = 0;

	EVP_MD_CTX_free(mdctx);
	EVP_PKEY_free(pkey);
	return result;
}

static int
haskey(dssh_key_algo_ctx *ctx)
{
	struct cbdata *cbd = (struct cbdata *)ctx;
	/* Only Ed25519 keys are stored in this module's ctx;
	 * EVP_PKEY_id always matches. */
	return (cbd != NULL && cbd->pkey != NULL
	    && EVP_PKEY_id(cbd->pkey) == EVP_PKEY_ED25519);
}

static void
cleanup(dssh_key_algo_ctx *ctx)
{
	struct cbdata *cbd = (struct cbdata *)ctx;
	if (cbd != NULL) {
		EVP_PKEY_free(cbd->pkey);
		free(cbd);
	}
}

DSSH_PUBLIC int
register_ssh_ed25519(void)
{
	struct dssh_key_algo_s *ka = malloc(sizeof(*ka) + ED25519_NAME_LEN + 1);
	if (ka == NULL)
		return DSSH_ERROR_ALLOC;
	ka->next = NULL;
	ka->sign = sign;
	ka->verify = verify;
	ka->pubkey = pubkey;
	ka->haskey = haskey;
	ka->cleanup = cleanup;
	ka->ctx = NULL;
	ka->flags = DSSH_KEY_ALGO_FLAG_SIGNATURE_CAPABLE;
	memcpy(ka->name, ED25519_NAME, ED25519_NAME_LEN + 1);
	return dssh_transport_register_key_algo(ka);
}

DSSH_PUBLIC int
ssh_ed25519_load_key_file(const char *path, pem_password_cb *pw_cb,
    void *pw_cbdata)
{
	dssh_key_algo ka =
	    dssh_transport_find_key_algo(ED25519_NAME);
	if (ka == NULL)
		return DSSH_ERROR_INIT;

	FILE *fp = fopen(path, "r");
	if (fp == NULL)
		return DSSH_ERROR_INIT;

	EVP_PKEY *pkey = PEM_read_PrivateKey(fp, NULL, pw_cb, pw_cbdata);
	fclose(fp);
	if (pkey == NULL)
		return DSSH_ERROR_INIT;

	if (EVP_PKEY_id(pkey) != EVP_PKEY_ED25519) {
		EVP_PKEY_free(pkey);
		return DSSH_ERROR_INVALID;
	}

	struct cbdata *cbd = (struct cbdata *)ka->ctx;
	if (cbd == NULL) {
		cbd = calloc(1, sizeof(*cbd));
		if (cbd == NULL) {
			EVP_PKEY_free(pkey);
			return DSSH_ERROR_ALLOC;
		}
		ka->ctx = (dssh_key_algo_ctx *)cbd;
	}
	else {
		EVP_PKEY_free(cbd->pkey);
	}

	cbd->pkey = pkey;
	return 0;
}

DSSH_PUBLIC int
ssh_ed25519_save_key_file(const char *path, pem_password_cb *pw_cb,
    void *pw_cbdata)
{
	dssh_key_algo ka =
	    dssh_transport_find_key_algo(ED25519_NAME);
	if (ka == NULL)
		return DSSH_ERROR_INIT;

	/* If cbd exists, pkey is always set by keygen/load. */
	struct cbdata *cbd = (struct cbdata *)ka->ctx;
#ifdef DSSH_TESTING
	if (cbd == NULL)
#else
	if (cbd == NULL || cbd->pkey == NULL)
#endif
		return DSSH_ERROR_INIT;

	FILE *fp = fopen(path, "w");
	if (fp == NULL)
		return DSSH_ERROR_INIT;

	const EVP_CIPHER *cipher = pw_cb != NULL ?
	    EVP_aes_256_cbc() : NULL;
	int ok = PEM_write_PrivateKey(fp, cbd->pkey,
	    cipher, NULL, 0, pw_cb, pw_cbdata);
	if (fclose(fp) != 0)
		ok = 0;
	return ok == 1 ? 0 : DSSH_ERROR_INIT;
}

DSSH_PUBLIC int64_t
ssh_ed25519_get_pub_str(char *buf, size_t bufsz)
{
	dssh_key_algo ka =
	    dssh_transport_find_key_algo(ED25519_NAME);
	if (ka == NULL)
		return DSSH_ERROR_INIT;

	uint8_t blob[256];
	size_t blob_len;
	int res = pubkey(blob, sizeof(blob), &blob_len, ka->ctx);
	if (res < 0)
		return res;

	size_t b64_len = 4 * ((blob_len + 2) / 3);
	/* "ssh-ed25519 " + base64 + NUL */
	size_t needed = ED25519_NAME_LEN + 1 + b64_len + 1;

	if (buf == NULL || bufsz == 0)
		return (int64_t)needed;
	if (bufsz < needed)
		return DSSH_ERROR_TOOLONG;

	memcpy(buf, ED25519_NAME, ED25519_NAME_LEN);
	buf[ED25519_NAME_LEN] = ' ';
	EVP_EncodeBlock((unsigned char *)&buf[ED25519_NAME_LEN + 1],
	    blob, (int)blob_len);
	return (int64_t)needed;
}

DSSH_PUBLIC int
ssh_ed25519_save_pub_file(const char *path)
{
	char str[256];
	int64_t len = ssh_ed25519_get_pub_str(str, sizeof(str));
	if (len < 0)
		return (int)len;

	FILE *fp = fopen(path, "w");
	if (fp == NULL)
		return DSSH_ERROR_INIT;
	int wok = fprintf(fp, "%s\n", str) >= 0;
	if (fclose(fp) != 0)
		wok = 0;
	return wok ? 0 : DSSH_ERROR_INIT;
}

DSSH_PUBLIC int
ssh_ed25519_generate_key(void)
{
	dssh_key_algo ka =
	    dssh_transport_find_key_algo(ED25519_NAME);
	if (ka == NULL)
		return DSSH_ERROR_INIT;

	EVP_PKEY *pkey = NULL;
	EVP_PKEY_CTX *pctx = EVP_PKEY_CTX_new_id(EVP_PKEY_ED25519, NULL);
	if (pctx == NULL)
		return DSSH_ERROR_ALLOC;

	if (EVP_PKEY_keygen_init(pctx) != 1 ||
	    EVP_PKEY_keygen(pctx, &pkey) != 1) {
		EVP_PKEY_CTX_free(pctx);
		return DSSH_ERROR_INIT;
	}
	EVP_PKEY_CTX_free(pctx);

	struct cbdata *cbd = (struct cbdata *)ka->ctx;
	if (cbd == NULL) {
		cbd = calloc(1, sizeof(*cbd));
		if (cbd == NULL) {
			EVP_PKEY_free(pkey);
			return DSSH_ERROR_ALLOC;
		}
		ka->ctx = (dssh_key_algo_ctx *)cbd;
	}
	else {
		EVP_PKEY_free(cbd->pkey);
	}

	cbd->pkey = pkey;
	return 0;
}
