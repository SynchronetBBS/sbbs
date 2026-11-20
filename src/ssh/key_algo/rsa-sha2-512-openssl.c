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
#include "private-key-file.h"
#include "client-key.h"
#include "key-context-openssl.h"
#include "key-wire.h"
#include "key-file-openssl.h"
#include "key-text.h"
#define DSSH_RSA_CACHE_OPENSSL
#include "key-rsa-cache.h"
#undef DSSH_RSA_CACHE_OPENSSL
#ifdef DSSH_TESTING
 #include "ssh-internal.h"
#endif

#define RSA_SHA2_512_NAME     "rsa-sha2-512"
#define RSA_SHA2_512_NAME_LEN 12

/* The SSH wire format uses "ssh-rsa" as the key type name,
 * even for rsa-sha2-512 signatures (RFC 8332 s3). */
#define RSA_KEY_TYPE_NAME     "ssh-rsa"
#define RSA_KEY_TYPE_NAME_LEN 7


static struct dssh_openssl_key_context *rsa_ctx;

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
	int result = DSSH_ERROR_INIT;
	int ret;
	struct dssh_rsa_wire_key key;
	struct dssh_bytes signature;
	BIGNUM *e_bn = NULL;
	BIGNUM *n_bn = NULL;
	OSSL_PARAM_BLD *bld = NULL;
	OSSL_PARAM *params = NULL;
	EVP_PKEY_CTX *pctx = NULL;
	EVP_PKEY *pkey = NULL;
	EVP_MD_CTX *mdctx = NULL;

	if (data == NULL && data_len != 0)
		return DSSH_ERROR_INVALID;
	ret = dssh_key_wire_rsa(key_blob, key_blob_len, &key);
	if (ret < 0)
		return ret;
	ret = dssh_key_wire_blob(sig_blob, sig_blob_len, RSA_SHA2_512_NAME,
	    RSA_SHA2_512_NAME_LEN, &signature);
	if (ret < 0)
		return ret;
	if (signature.len == 0 || key.exponent.len > INT_MAX || key.modulus.len > INT_MAX)
		return DSSH_ERROR_INVALID;
	int e_len = (int)key.exponent.len;
	int n_len = (int)key.modulus.len;
	e_bn = BN_bin2bn(key.exponent.data, e_len, NULL);
	if (e_bn == NULL)
		goto done;
	n_bn = BN_bin2bn(key.modulus.data, n_len, NULL);
	if (n_bn == NULL)
		goto done;
	const uint8_t *raw_sig = signature.data;
	size_t raw_sig_len = signature.len;

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
	EVP_PKEY_CTX *pctx = NULL;
	if (EVP_DigestSignInit(mdctx, &pctx, EVP_sha512(), NULL, cbd->pkey) != 1
	    || EVP_PKEY_CTX_set_rsa_padding(pctx, RSA_PKCS1_PADDING) != 1) {
		EVP_MD_CTX_free(mdctx);
		return DSSH_ERROR_INIT;
	}
	size_t siglen = 0;
	if (EVP_DigestSign(mdctx, NULL, &siglen, data, data_len) != 1 || siglen == 0) {
		EVP_MD_CTX_free(mdctx);
		return DSSH_ERROR_INIT;
	}
	size_t prefix = 4 + RSA_SHA2_512_NAME_LEN + 4;
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
	if (signed_ok != 1)
		return DSSH_ERROR_INIT;
	if (siglen == 0 || siglen > capacity)
		return DSSH_ERROR_INVALID;
	size_t pos = 0;
	ret = dssh_serialize_uint32(RSA_SHA2_512_NAME_LEN, buf, needed, &pos);
	if (ret < 0)
		return ret;
	memcpy(&buf[pos], RSA_SHA2_512_NAME, RSA_SHA2_512_NAME_LEN);
	pos += RSA_SHA2_512_NAME_LEN;
	uint32_t siglen_u32 = (uint32_t)siglen;
	ret = dssh_serialize_uint32(siglen_u32, buf, needed, &pos);
	if (ret < 0)
		return ret;
	out->bytes.len = prefix + siglen;
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
	struct dssh_openssl_key_context *cbd = (struct dssh_openssl_key_context *)ctx;
	if (cbd == NULL || cbd->pkey == NULL)
		return DSSH_ERROR_INIT;
	if (cbd->pub_blob != NULL) {
		*out = cbd->pub_blob;
		*outlen = cbd->pub_blob_len;
		return 0;
	}
	BIGNUM *e = NULL, *n = NULL;
	uint8_t *buf = NULL;
	int ret = DSSH_ERROR_INIT;
	if (EVP_PKEY_get_bn_param(cbd->pkey, OSSL_PKEY_PARAM_RSA_E, &e) != 1 || e == NULL
	    || EVP_PKEY_get_bn_param(cbd->pkey, OSSL_PKEY_PARAM_RSA_N, &n) != 1 || n == NULL)
		goto done;
	struct dssh_rsa_cache_number exponent, modulus;
	struct dssh_rsa_cache_layout layout;
	ret = dssh_rsa_cache_native_number(e, &exponent);
	if (ret < 0)
		goto done;
	ret = dssh_rsa_cache_native_number(n, &modulus);
	if (ret < 0)
		goto done;
	ret = dssh_rsa_cache_size_native(exponent, modulus, &layout);
	if (ret < 0)
		goto done;
	buf = dssh_rsa_cache_allocate(layout.length);
	if (buf == NULL) {
		ret = DSSH_ERROR_ALLOC;
		goto done;
	}
	uint8_t *slots[2];
	ret = dssh_rsa_cache_prefix(buf, &layout, &slots[0], &slots[1]);
	if (ret < 0)
		goto done;
	const BIGNUM *numbers[] = {e, n};
	const size_t sizes[] = {exponent.bytes, modulus.bytes};
	for (unsigned int i = 0; i < 2; i++) {
		int written = BN_bn2bin(numbers[i], slots[i]);
		ret = DSSH_ERROR_INIT;
		if (written < 0)
			goto done;
#if INT_MAX > SIZE_MAX
		if (written > SIZE_MAX)
			goto done;
#endif
		size_t count = (size_t)written;
		if (count != sizes[i])
			goto done;
	}
	cbd->pub_blob = buf;
	cbd->pub_blob_len = layout.length;
	*out = buf;
	*outlen = layout.length;
	buf = NULL;
	ret = 0;
done:
	dssh_rsa_cache_release(buf);
	BN_free(e);
	BN_free(n);
	return ret;
}

static unsigned int
key_bits(const uint8_t *key_blob, size_t key_blob_len)
{
	struct dssh_rsa_wire_key key;
	if (dssh_key_wire_rsa(key_blob, key_blob_len, &key) < 0)
		return 0;
	return dssh_key_wire_bits(key.modulus, UINT_MAX);
}

static int
haskey(dssh_key_algo_ctx *ctx)
{
	/* Only RSA keys stored in this module's ctx. */
	struct dssh_openssl_key_context *cbd = (struct dssh_openssl_key_context *)ctx;
	return (cbd != NULL && cbd->pkey != NULL && EVP_PKEY_is_a(cbd->pkey, "RSA"));
}

static void
cleanup(dssh_key_algo_ctx *ctx)
{
	struct dssh_openssl_key_context *cbd = (struct dssh_openssl_key_context *)ctx;

	if (cbd == rsa_ctx)
		rsa_ctx = NULL;
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
		ret = dssh_key_algo_set_ctx(RSA_SHA2_512_NAME, replacement);
	if (ret < 0) {
		cleanup(opaque);
		return ret;
	}
	if (local != NULL) {
		*local = replacement;
		return 0;
	}
	struct dssh_openssl_key_context *old = rsa_ctx;
	dssh_key_algo_ctx *old_opaque = (dssh_key_algo_ctx *)old;

	rsa_ctx = replacement;
	cleanup(old_opaque);
	return 0;
}

static int
load_key_file_inner(const char *path, dssh_pem_password_cb pw_cb, void *pw_cbdata, struct dssh_openssl_key_context **local)
{
	if (path == NULL || path[0] == 0)
		return DSSH_ERROR_INIT;
	if (local == NULL) {
		int config_ret = dssh_key_algo_setup_check(RSA_SHA2_512_NAME);
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
	if (!EVP_PKEY_is_a(pkey, "RSA")) {
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
dssh_rsa_sha2_512_get_pub_str(char *buf, size_t bufsz)
{
	const uint8_t *blob;
	size_t length;
	dssh_key_algo_ctx *context = (dssh_key_algo_ctx *)rsa_ctx;
	int ret = pubkey(&blob, &length, context);
	if (ret < 0)
		return ret;
	return dssh_key_text_format(blob, length, RSA_KEY_TYPE_NAME, RSA_KEY_TYPE_NAME_LEN, buf, bufsz);
}

DSSH_PUBLIC int
dssh_rsa_sha2_512_save_pub_file(const char *path)
{
	if (path == NULL || path[0] == 0)
		return DSSH_ERROR_INIT;
	const uint8_t *blob;
	size_t length;
	dssh_key_algo_ctx *context = (dssh_key_algo_ctx *)rsa_ctx;
	int ret = pubkey(&blob, &length, context);
	if (ret < 0)
		return ret;
	return dssh_key_text_write(path, blob, length, RSA_KEY_TYPE_NAME, RSA_KEY_TYPE_NAME_LEN);
}

static int
generate_key_inner(unsigned int bits, struct dssh_openssl_key_context **local)
{
	if (local == NULL) {
		int config_ret = dssh_key_algo_setup_check(RSA_SHA2_512_NAME);
		if (config_ret < 0)
			return config_ret;
	}
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

	return install_key(pkey, local);
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
	ka->cleanup  = cleanup;
	ka->key_bits = key_bits;
	ka->ctx      = NULL;
	ka->flags    = DSSH_KEY_ALGO_FLAG_SIGNATURE_CAPABLE;
	memcpy(ka->name, RSA_SHA2_512_NAME, RSA_SHA2_512_NAME_LEN + 1);
	int ret = dssh_transport_register_key_algo(ka);
	if (ret < 0)
		free(ka);
	return ret;
}

DSSH_PUBLIC int
dssh_rsa_sha2_512_load_key_file(const char *path, dssh_pem_password_cb cb, void *cbdata)
{
	return load_key_file_inner(path, cb, cbdata, NULL);
}

DSSH_PUBLIC int
dssh_rsa_sha2_512_generate_key(unsigned int bits)
{
	return generate_key_inner(bits, NULL);
}

DSSH_PUBLIC int
dssh_rsa_sha2_512_save_key_file(const char *path, dssh_pem_password_cb cb, void *cbdata)
{
	return save_key_file_inner(path, cb, cbdata, rsa_ctx);
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
	struct dssh_openssl_key_context *local = NULL;
	int ret = generate_key_inner(bits, &local);
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
dssh_rsa512_client_ops(struct dssh_key_algo_s *module)
{
	static const struct dssh_client_key_ops ops = {
		client_load, client_generate, client_save, client_release
	};
	if (module->sign != sign || module->pubkey != pubkey || module->haskey != haskey)
		return NULL;
	return &ops;
}
