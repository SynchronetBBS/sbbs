#include <openssl/evp.h>
#include <openssl/pem.h>
#include <stdlib.h>
#include <string.h>

#include "deucessh.h"

#define ED25519_RAW_PUB_LEN  32
#define ED25519_RAW_SIG_LEN  64
#define ED25519_NAME         "ssh-ed25519"
#define ED25519_NAME_LEN     11

struct cbdata {
	EVP_PKEY *pkey;
};

static int
sign(uint8_t *buf, size_t bufsz, size_t *outlen,
    const uint8_t *data, size_t data_len, deuce_ssh_key_algo_ctx *ctx)
{
	struct cbdata *cbd = (struct cbdata *)ctx;
	if (cbd == NULL || cbd->pkey == NULL)
		return DEUCE_SSH_ERROR_INIT;

	size_t needed = 4 + ED25519_NAME_LEN + 4 + ED25519_RAW_SIG_LEN;
	if (bufsz < needed)
		return DEUCE_SSH_ERROR_TOOLONG;

	EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
	if (mdctx == NULL)
		return DEUCE_SSH_ERROR_ALLOC;

	if (EVP_DigestSignInit(mdctx, NULL, NULL, NULL, cbd->pkey) != 1) {
		EVP_MD_CTX_free(mdctx);
		return DEUCE_SSH_ERROR_INIT;
	}

	uint8_t raw_sig[ED25519_RAW_SIG_LEN];
	size_t siglen = sizeof(raw_sig);
	if (EVP_DigestSign(mdctx, raw_sig, &siglen, data, data_len) != 1) {
		EVP_MD_CTX_free(mdctx);
		return DEUCE_SSH_ERROR_INIT;
	}
	EVP_MD_CTX_free(mdctx);

	/* Serialize SSH signature blob */
	size_t pos = 0;
	deuce_ssh_serialize_uint32(ED25519_NAME_LEN, buf, bufsz, &pos);
	memcpy(&buf[pos], ED25519_NAME, ED25519_NAME_LEN);
	pos += ED25519_NAME_LEN;
	deuce_ssh_serialize_uint32((uint32_t)siglen, buf, bufsz, &pos);
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
pubkey(uint8_t *buf, size_t bufsz, size_t *outlen, deuce_ssh_key_algo_ctx *ctx)
{
	struct cbdata *cbd = (struct cbdata *)ctx;
	if (cbd == NULL || cbd->pkey == NULL)
		return DEUCE_SSH_ERROR_INIT;

	uint8_t raw_pub[ED25519_RAW_PUB_LEN];
	size_t raw_pub_len = sizeof(raw_pub);
	if (EVP_PKEY_get_raw_public_key(cbd->pkey, raw_pub, &raw_pub_len) != 1)
		return DEUCE_SSH_ERROR_INIT;

	/* 4 + 11 + 4 + 32 = 51 bytes */
	size_t needed = 4 + ED25519_NAME_LEN + 4 + raw_pub_len;
	if (bufsz < needed)
		return DEUCE_SSH_ERROR_TOOLONG;

	size_t pos = 0;
	deuce_ssh_serialize_uint32(ED25519_NAME_LEN, buf, bufsz, &pos);
	memcpy(&buf[pos], ED25519_NAME, ED25519_NAME_LEN);
	pos += ED25519_NAME_LEN;
	deuce_ssh_serialize_uint32((uint32_t)raw_pub_len, buf, bufsz, &pos);
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
	if (deuce_ssh_parse_uint32(&key_blob[kp], key_blob_len, &algo_len) < 4 ||
	    kp + 4 + algo_len > key_blob_len)
		return DEUCE_SSH_ERROR_PARSE;
	kp += 4 + algo_len;
	uint32_t raw_pub_len;
	if (deuce_ssh_parse_uint32(&key_blob[kp], key_blob_len - kp, &raw_pub_len) < 4 ||
	    kp + 4 + raw_pub_len > key_blob_len)
		return DEUCE_SSH_ERROR_PARSE;
	kp += 4;
	const uint8_t *raw_pub = &key_blob[kp];

	if (raw_pub_len != ED25519_RAW_PUB_LEN)
		return DEUCE_SSH_ERROR_INVALID;

	/* Parse signature from sig_blob:
	 * string algo_name, string raw_signature */
	size_t sp = 0;
	uint32_t sig_algo_len;
	if (deuce_ssh_parse_uint32(&sig_blob[sp], sig_blob_len, &sig_algo_len) < 4 ||
	    sp + 4 + sig_algo_len > sig_blob_len)
		return DEUCE_SSH_ERROR_PARSE;
	sp += 4 + sig_algo_len;
	uint32_t raw_sig_len;
	if (deuce_ssh_parse_uint32(&sig_blob[sp], sig_blob_len - sp, &raw_sig_len) < 4 ||
	    sp + 4 + raw_sig_len > sig_blob_len)
		return DEUCE_SSH_ERROR_PARSE;
	sp += 4;
	const uint8_t *raw_sig = &sig_blob[sp];

	if (raw_sig_len != ED25519_RAW_SIG_LEN)
		return DEUCE_SSH_ERROR_INVALID;

	EVP_PKEY *pkey = EVP_PKEY_new_raw_public_key(
	    EVP_PKEY_ED25519, NULL, raw_pub, raw_pub_len);
	if (pkey == NULL)
		return DEUCE_SSH_ERROR_INIT;

	EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
	if (mdctx == NULL) {
		EVP_PKEY_free(pkey);
		return DEUCE_SSH_ERROR_ALLOC;
	}

	int result;
	if (EVP_DigestVerifyInit(mdctx, NULL, NULL, NULL, pkey) != 1 ||
	    EVP_DigestVerify(mdctx, raw_sig, raw_sig_len, data, data_len) != 1)
		result = DEUCE_SSH_ERROR_INVALID;
	else
		result = 0;

	EVP_MD_CTX_free(mdctx);
	EVP_PKEY_free(pkey);
	return result;
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

int
register_ssh_ed25519(void)
{
	struct deuce_ssh_key_algo_s *ka = malloc(sizeof(*ka) + ED25519_NAME_LEN + 1);
	if (ka == NULL)
		return DEUCE_SSH_ERROR_ALLOC;
	ka->next = NULL;
	ka->sign = sign;
	ka->verify = verify;
	ka->pubkey = pubkey;
	ka->haskey = haskey;
	ka->cleanup = cleanup;
	ka->flags = DEUCE_SSH_KEY_ALGO_FLAG_SIGNATURE_CAPABLE;
	memcpy(ka->name, ED25519_NAME, ED25519_NAME_LEN + 1);
	return deuce_ssh_transport_register_key_algo(ka);
}

int
ssh_ed25519_load_key_file(deuce_ssh_session sess, const char *path)
{
	FILE *fp = fopen(path, "r");
	if (fp == NULL)
		return DEUCE_SSH_ERROR_INIT;

	EVP_PKEY *pkey = PEM_read_PrivateKey(fp, NULL, NULL, NULL);
	fclose(fp);
	if (pkey == NULL)
		return DEUCE_SSH_ERROR_INIT;

	if (EVP_PKEY_id(pkey) != EVP_PKEY_ED25519) {
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

int
ssh_ed25519_generate_key(deuce_ssh_session sess)
{
	EVP_PKEY *pkey = NULL;
	EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_ED25519, NULL);
	if (ctx == NULL)
		return DEUCE_SSH_ERROR_ALLOC;

	if (EVP_PKEY_keygen_init(ctx) != 1 ||
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
