#include <openssl/evp.h>
#include <openssl/pem.h>

#include <stdlib.h>
#include <string.h>

#include "deucessh-algorithms.h"
#include "deucessh-key-algo.h"
#include "deucessh.h"
#include "private-key-file.h"
#include "client-key.h"
#include "key-context-openssl.h"
#include "key-wire.h"
#include "key-file-openssl.h"
#include "key-text.h"
#ifdef DSSH_TESTING
 #include "ssh-internal.h"
#endif

#define ED25519_RAW_PUB_LEN 32
#define ED25519_RAW_SIG_LEN 64
#define ED25519_NAME        "ssh-ed25519"
#define ED25519_NAME_LEN    11


static struct dssh_openssl_key_context *ed25519_ctx;

static int
sign(struct dssh_bytes_result *out, const uint8_t *data, size_t data_len, dssh_key_algo_ctx *ctx)
{
	if (out == NULL || (data == NULL && data_len > 0)
	    || out->bytes.data != NULL || out->bytes.len != 0
	    || out->lease.release != NULL || out->lease.owner != NULL)
		return DSSH_ERROR_INVALID;
	struct dssh_openssl_key_context *cbd = (struct dssh_openssl_key_context *)ctx;
#ifdef DSSH_TESTING
	if (cbd == NULL)
#else
	if (cbd == NULL || cbd->pkey == NULL)
#endif
		return DSSH_ERROR_INIT;
	EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
	if (mdctx == NULL)
		return DSSH_ERROR_ALLOC;
	if (EVP_DigestSignInit(mdctx, NULL, NULL, NULL, cbd->pkey) != 1) {
		EVP_MD_CTX_free(mdctx);
		return DSSH_ERROR_INIT;
	}
	size_t siglen = ED25519_RAW_SIG_LEN;
	size_t prefix = 4 + ED25519_NAME_LEN + 4;
	if (siglen > UINT32_MAX || siglen > SIZE_MAX - prefix) {
		EVP_MD_CTX_free(mdctx);
		return DSSH_ERROR_INVALID;
	}
	size_t capacity = siglen;
	size_t needed = prefix + capacity;
	uint8_t *buf = NULL;
	int ret = dssh_signature_buffer_acquire(cbd->signatures, needed, out, &buf);
	if (ret < 0) {
		EVP_MD_CTX_free(mdctx);
		return ret;
	}
	/* The provider writes directly into retained producer storage. A partial
	 * result keeps its lease on error; the consumer releases it once. */
	int signed_ok = EVP_DigestSign(mdctx, &buf[prefix], &siglen, data, data_len);
	EVP_MD_CTX_free(mdctx);
	if (signed_ok != 1 || siglen == 0 || siglen > capacity || siglen != ED25519_RAW_SIG_LEN)
		return DSSH_ERROR_INIT;
	size_t pos = 0;
	ret = dssh_serialize_uint32(ED25519_NAME_LEN, buf, needed, &pos);
	if (ret < 0)
		return ret;
	memcpy(&buf[pos], ED25519_NAME, ED25519_NAME_LEN);
	pos += ED25519_NAME_LEN;
	uint32_t siglen_u32 = (uint32_t)siglen;
	ret = dssh_serialize_uint32(siglen_u32, buf, needed, &pos);
	if (ret < 0)
		return ret;
	out->bytes.len = prefix + siglen;
	return 0;
}

/*
 * Return a pointer to the cached SSH-format public key blob:
 *   string  "ssh-ed25519"
 *   string  <32-byte raw public key>
 *
 * The blob is computed once and cached in dssh_openssl_key_context.
 * Returns 0 on success, negative error code on failure.
 */
static int
pubkey(const uint8_t **out, size_t *outlen, dssh_key_algo_ctx *ctx)
{
	struct dssh_openssl_key_context *cbd = (struct dssh_openssl_key_context *)ctx;
	/* cbd->pkey always set before pubkey is callable. */
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

	uint8_t raw_pub[ED25519_RAW_PUB_LEN];
	size_t  raw_pub_len = sizeof(raw_pub);
	if (EVP_PKEY_get_raw_public_key(cbd->pkey, raw_pub, &raw_pub_len) != 1
	    || raw_pub_len != ED25519_RAW_PUB_LEN)
		return DSSH_ERROR_INIT;

	size_t   needed = 4 + ED25519_NAME_LEN + 4 + raw_pub_len;
	uint8_t *buf    = malloc(needed);
	if (buf == NULL)
		return DSSH_ERROR_ALLOC;

	size_t pos = 0;
	int    ret = dssh_serialize_uint32(ED25519_NAME_LEN, buf, needed, &pos);
	if (ret < 0) {
		free(buf);
		return ret;
	}
	memcpy(&buf[pos], ED25519_NAME, ED25519_NAME_LEN);
	pos += ED25519_NAME_LEN;
	ret = dssh_serialize_uint32((uint32_t)raw_pub_len, buf, needed, &pos);
	if (ret < 0) {
		free(buf);
		return ret;
	}
	memcpy(&buf[pos], raw_pub, raw_pub_len);
	pos += raw_pub_len;

	cbd->pub_blob     = buf;
	cbd->pub_blob_len = pos;
	*out              = buf;
	*outlen           = pos;
	return 0;
}

/*
 * Verify a signature over data using a public host key blob.
 * key_blob: SSH wire format (string "ssh-ed25519" + string <32-byte key>)
 * sig_blob: SSH wire format (string "ssh-ed25519" + string <64-byte sig>)
 * data/data_len: the data that was signed (exchange hash)
 */
static int
verify(const uint8_t *key_blob, size_t key_blob_len, const uint8_t *sig_blob, size_t sig_blob_len,
    const uint8_t *data, size_t data_len)
{
	struct dssh_bytes key, signature;
	int ret;
	if (data == NULL && data_len != 0)
		return DSSH_ERROR_INVALID;
	ret = dssh_key_wire_ed25519(key_blob, key_blob_len, &key);
	if (ret < 0)
		return ret;
	ret = dssh_key_wire_blob(sig_blob, sig_blob_len, ED25519_NAME, ED25519_NAME_LEN, &signature);
	if (ret < 0)
		return ret;
	if (signature.len != ED25519_RAW_SIG_LEN)
		return DSSH_ERROR_INVALID;
	const uint8_t *raw_pub = key.data;
	const uint8_t *raw_sig = signature.data;
	size_t raw_sig_len = signature.len;
	size_t raw_pub_len = key.len;

	EVP_PKEY *pkey = EVP_PKEY_new_raw_public_key_ex(NULL, "ED25519", NULL, raw_pub, raw_pub_len);
	if (pkey == NULL)
		return DSSH_ERROR_INIT;

	EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
	if (mdctx == NULL) {
		EVP_PKEY_free(pkey);
		return DSSH_ERROR_ALLOC;
	}

	int result;
	if (EVP_DigestVerifyInit(mdctx, NULL, NULL, NULL, pkey) != 1
	    || EVP_DigestVerify(mdctx, raw_sig, raw_sig_len, data, data_len) != 1)
		result = DSSH_ERROR_INVALID;
	else
		result = 0;

	EVP_MD_CTX_free(mdctx);
	EVP_PKEY_free(pkey);
	return result;
}

static unsigned int
key_bits(const uint8_t *key_blob, size_t key_blob_len)
{
	struct dssh_bytes key;
	if (dssh_key_wire_ed25519(key_blob, key_blob_len, &key) < 0)
		return 0;
	return 256;
}

static int
haskey(dssh_key_algo_ctx *ctx)
{
	struct dssh_openssl_key_context *cbd = (struct dssh_openssl_key_context *)ctx;
	/* Only Ed25519 keys are stored in this module's ctx;
	 * EVP_PKEY_is_a always matches. */
	return (cbd != NULL && cbd->pkey != NULL && EVP_PKEY_is_a(cbd->pkey, "ED25519"));
}

static void
cleanup(dssh_key_algo_ctx *ctx)
{
	struct dssh_openssl_key_context *cbd = (struct dssh_openssl_key_context *)ctx;

	if (cbd == ed25519_ctx)
		ed25519_ctx = NULL;
	if (cbd != NULL) {
		EVP_PKEY_free(cbd->pkey);
		free(cbd->pub_blob);
		dssh_signature_pool_destroy(cbd->signatures);
		free(cbd);
	}
}

/* Build the immutable public-key cache before publishing the key. */
static int
install_key(EVP_PKEY *pkey, struct dssh_openssl_key_context **local)
{
	struct dssh_openssl_key_context *replacement = malloc(sizeof(*replacement));

	if (replacement == NULL) {
		EVP_PKEY_free(pkey);
		return DSSH_ERROR_ALLOC;
	}
	*replacement = (struct dssh_openssl_key_context){.pkey = pkey};
	dssh_key_algo_ctx *opaque = (dssh_key_algo_ctx *)replacement;
	const uint8_t *blob;
	size_t blob_len;
	int ret = pubkey(&blob, &blob_len, opaque);

	if (ret == 0)
		ret = dssh_signature_pool_create(&replacement->signatures);
	if (ret == 0 && local == NULL)
		ret = dssh_key_algo_set_ctx(ED25519_NAME, replacement);
	if (ret < 0) {
		cleanup(opaque);
		return ret;
	}
	if (local != NULL) {
		*local = replacement;
		return 0;
	}
	struct dssh_openssl_key_context *old = ed25519_ctx;
	dssh_key_algo_ctx *old_opaque = (dssh_key_algo_ctx *)old;

	ed25519_ctx = replacement;
	cleanup(old_opaque);
	return 0;
}

DSSH_PUBLIC int
dssh_register_ssh_ed25519(void)
{
	struct dssh_key_algo_s *ka = malloc(sizeof(*ka) + ED25519_NAME_LEN + 1);
	if (ka == NULL)
		return DSSH_ERROR_ALLOC;
	ka->next    = NULL;
	ka->sign    = sign;
	ka->verify  = verify;
	ka->pubkey  = pubkey;
	ka->haskey  = haskey;
	ka->cleanup  = cleanup;
	ka->key_bits = key_bits;
	ka->ctx      = NULL;
	ka->flags    = DSSH_KEY_ALGO_FLAG_SIGNATURE_CAPABLE;
	memcpy(ka->name, ED25519_NAME, ED25519_NAME_LEN + 1);
	int ret = dssh_transport_register_key_algo(ka);
	if (ret < 0)
		free(ka);
	return ret;
}

static int
load_key_file_inner(const char *path, dssh_pem_password_cb pw_cb, void *pw_cbdata, struct dssh_openssl_key_context **local)
{
	if (path == NULL || path[0] == 0)
		return DSSH_ERROR_INIT;
	if (local == NULL) {
		int config_ret = dssh_key_algo_setup_check(ED25519_NAME);
		if (config_ret < 0)
			return config_ret;
	}
	FILE *fp = dssh_key_file_open_read(path);
	if (fp == NULL)
		return DSSH_ERROR_INIT;
	struct dssh_key_file_stream stream;
	int stream_ret = dssh_key_file_stream_init(&stream, fp);
	if (stream_ret < 0)
		return stream_ret;
	struct dssh_key_file_password_state password = {pw_cb, pw_cbdata, 0};
	EVP_PKEY *pkey = dssh_key_file_read_private(fp, &password);
	int closed = dssh_key_file_stream_close(&stream, 0);
	if (closed < 0 || pkey == NULL || password.status < 0) {
		EVP_PKEY_free(pkey);
		return password.status < 0 ? password.status : DSSH_ERROR_INIT;
	}
	if (!EVP_PKEY_is_a(pkey, "ED25519")) {
		EVP_PKEY_free(pkey);
		return DSSH_ERROR_INVALID;
	}
	EVP_PKEY_CTX *validation = EVP_PKEY_CTX_new(pkey, NULL);
	if (validation == NULL) {
		EVP_PKEY_free(pkey);
		return DSSH_ERROR_ALLOC;
	}
	int checked = EVP_PKEY_pairwise_check(validation);
	EVP_PKEY_CTX_free(validation);
	if (checked != 1) {
		EVP_PKEY_free(pkey);
		return checked == 0 ? DSSH_ERROR_INVALID : DSSH_ERROR_INIT;
	}
	return install_key(pkey, local);
}

static int
save_key_file_inner(const char *path, dssh_pem_password_cb pw_cb, void *pw_cbdata, struct dssh_openssl_key_context *selected)
{
	if (path == NULL || path[0] == 0)
		return DSSH_ERROR_INIT;
#ifdef DSSH_TESTING
	if (selected == NULL)
#else
	if (selected == NULL || selected->pkey == NULL)
#endif
		return DSSH_ERROR_INIT;

	EVP_PKEY *pkey = selected->pkey;
	if (EVP_PKEY_up_ref(pkey) != 1)
		return DSSH_ERROR_INIT;
	FILE *fp = dssh_private_key_open(path);
	if (fp == NULL) {
		EVP_PKEY_free(pkey);
		return DSSH_ERROR_INIT;
	}
	struct dssh_key_file_stream stream;
	int stream_ret = dssh_key_file_stream_init(&stream, fp);
	if (stream_ret < 0) {
		EVP_PKEY_free(pkey);
		return stream_ret;
	}

	EVP_CIPHER *cipher = NULL;

	if (pw_cb != NULL) {
		cipher = EVP_CIPHER_fetch(NULL, "AES-256-CBC", NULL);
		if (cipher == NULL) {
			EVP_PKEY_free(pkey);
			return dssh_key_file_stream_close(&stream, DSSH_ERROR_INIT);
		}
	}
	struct dssh_key_file_password_state password = {pw_cb, pw_cbdata, 0};
	int ok = dssh_key_file_write_private(fp, pkey, cipher, &password);

	EVP_CIPHER_free(cipher);
	EVP_PKEY_free(pkey);
	int status = ok == 1 ? 0 : DSSH_ERROR_INIT;
	if (password.status < 0)
		status = password.status;
	return dssh_key_file_stream_close(&stream, status);
}

DSSH_PUBLIC int64_t
dssh_ed25519_get_pub_str(char *buf, size_t bufsz)
{
	const uint8_t *blob;
	size_t length;
	dssh_key_algo_ctx *context = (dssh_key_algo_ctx *)ed25519_ctx;
	int ret = pubkey(&blob, &length, context);
	if (ret < 0)
		return ret;
	return dssh_key_text_format(blob, length, ED25519_NAME, ED25519_NAME_LEN, buf, bufsz);
}

DSSH_PUBLIC int
dssh_ed25519_save_pub_file(const char *path)
{
	if (path == NULL || path[0] == 0)
		return DSSH_ERROR_INIT;
	const uint8_t *blob;
	size_t length;
	dssh_key_algo_ctx *context = (dssh_key_algo_ctx *)ed25519_ctx;
	int ret = pubkey(&blob, &length, context);
	if (ret < 0)
		return ret;
	return dssh_key_text_write(path, blob, length, ED25519_NAME, ED25519_NAME_LEN);
}

static int
generate_key_inner(struct dssh_openssl_key_context **local)
{
	if (local == NULL) {
		int config_ret = dssh_key_algo_setup_check(ED25519_NAME);
		if (config_ret < 0)
			return config_ret;
	}
	EVP_PKEY     *pkey = NULL;
	EVP_PKEY_CTX *pctx = EVP_PKEY_CTX_new_from_name(NULL, "ED25519", NULL);

	if (pctx == NULL)
		return DSSH_ERROR_ALLOC;

	if (EVP_PKEY_keygen_init(pctx) != 1 || EVP_PKEY_keygen(pctx, &pkey) != 1) {
		EVP_PKEY_CTX_free(pctx);
		return DSSH_ERROR_INIT;
	}
	EVP_PKEY_CTX_free(pctx);

	return install_key(pkey, local);
}

DSSH_PUBLIC int
dssh_ed25519_load_key_file(const char *path, dssh_pem_password_cb cb, void *cbdata)
{
	return load_key_file_inner(path, cb, cbdata, NULL);
}

DSSH_PUBLIC int
dssh_ed25519_generate_key(void)
{
	return generate_key_inner(NULL);
}

DSSH_PUBLIC int
dssh_ed25519_save_key_file(const char *path, dssh_pem_password_cb cb, void *cbdata)
{
	return save_key_file_inner(path, cb, cbdata, ed25519_ctx);
}

static int
client_load(void **out, const char *path, dssh_pem_password_cb cb, void *cbdata)
{
	struct dssh_openssl_key_context *local = NULL;
	int ret = load_key_file_inner(path, cb, cbdata, &local);
	if (ret == 0)
		*out = local;
	return ret;
}

static int
client_generate(void **out, unsigned int bits)
{
	if (bits != 256)
		return DSSH_ERROR_INVALID;
	struct dssh_openssl_key_context *local = NULL;
	int ret = generate_key_inner(&local);
	if (ret == 0)
		*out = local;
	return ret;
}

static int
client_save(void *ctx, const char *path, dssh_pem_password_cb cb, void *cbdata)
{
	struct dssh_openssl_key_context *selected = (struct dssh_openssl_key_context *)ctx;
	return save_key_file_inner(path, cb, cbdata, selected);
}

static void
client_release(void *ctx)
{
	dssh_key_algo_ctx *opaque = (dssh_key_algo_ctx *)ctx;
	cleanup(opaque);
}

DSSH_PRIVATE const struct dssh_client_key_ops *
dssh_ed25519_client_ops(struct dssh_key_algo_s *module)
{
	static const struct dssh_client_key_ops ops = {
		client_load, client_generate, client_save, client_release
	};
	if (module->sign != sign || module->pubkey != pubkey || module->haskey != haskey)
		return NULL;
	return &ops;
}
