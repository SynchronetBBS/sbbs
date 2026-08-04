#include "xp_cipher.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <openssl/crypto.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/provider.h>

struct xp_cipher_context {
	struct xp_cipher_config config;
	EVP_CIPHER *cipher;
	EVP_CIPHER_CTX *native;
	unsigned char key[32];
	unsigned char iv[16];
	unsigned char tag[16];
	size_t tag_len;
	unsigned char *input;
	size_t input_len;
	bool text_started;
	bool tag_set;
	bool finalized;
	char err[256];
};

static CRYPTO_ONCE legacy_once = CRYPTO_ONCE_STATIC_INIT;
static OSSL_PROVIDER *legacy_provider;

static void
load_legacy_provider(void)
{
	legacy_provider = OSSL_PROVIDER_load(NULL, "legacy");
}

static const char *
cipher_name(enum xp_cipher_algorithm algorithm, enum xp_cipher_mode mode,
	size_t key_len)
{
	switch (algorithm) {
		case XP_CIPHER_AES:
			if (key_len != 16 && key_len != 24 && key_len != 32)
				return NULL;
			if (mode == XP_CIPHER_MODE_CBC)
				return key_len == 16 ? "AES-128-CBC"
					: key_len == 24 ? "AES-192-CBC" : "AES-256-CBC";
			if (mode == XP_CIPHER_MODE_CFB)
				return key_len == 16 ? "AES-128-CFB"
					: key_len == 24 ? "AES-192-CFB" : "AES-256-CFB";
			if (mode == XP_CIPHER_MODE_GCM)
				return key_len == 16 ? "AES-128-GCM"
					: key_len == 24 ? "AES-192-GCM" : "AES-256-GCM";
			return NULL;
		case XP_CIPHER_CHACHA20:
			return mode == XP_CIPHER_MODE_STREAM && key_len == 32
				? "ChaCha20" : NULL;
		case XP_CIPHER_3DES:
			return mode == XP_CIPHER_MODE_CBC && key_len == 24
				? "DES-EDE3-CBC" : NULL;
		case XP_CIPHER_CAST128:
			return mode == XP_CIPHER_MODE_CBC && key_len >= 5 && key_len <= 16
				? "CAST5-CBC" : NULL;
		case XP_CIPHER_RC4:
			return mode == XP_CIPHER_MODE_STREAM && key_len > 0 && key_len <= 32
				? "RC4" : NULL;
	}
	return NULL;
}

static EVP_CIPHER *
fetch_cipher(enum xp_cipher_algorithm algorithm, enum xp_cipher_mode mode,
	size_t key_len)
{
	const char *name = cipher_name(algorithm, mode, key_len);
	if (name == NULL)
		return NULL;
	EVP_CIPHER *cipher = EVP_CIPHER_fetch(NULL, name, NULL);
	if (cipher == NULL && (algorithm == XP_CIPHER_CAST128
	    || algorithm == XP_CIPHER_RC4)) {
		CRYPTO_THREAD_run_once(&legacy_once, load_legacy_provider);
		cipher = EVP_CIPHER_fetch(NULL, name, NULL);
	}
	return cipher;
}

static bool
legacy_algorithm(enum xp_cipher_algorithm algorithm)
{
	return algorithm == XP_CIPHER_3DES || algorithm == XP_CIPHER_CAST128
	    || algorithm == XP_CIPHER_RC4;
}

static size_t
representative_key_length(enum xp_cipher_algorithm algorithm)
{
	switch (algorithm) {
		case XP_CIPHER_AES: return 32;
		case XP_CIPHER_CHACHA20: return 32;
		case XP_CIPHER_3DES: return 24;
		case XP_CIPHER_CAST128: return 16;
		case XP_CIPHER_RC4: return 16;
	}
	return 0;
}

bool
xp_cipher_supported(enum xp_cipher_algorithm algorithm,
	enum xp_cipher_mode mode, enum xp_cipher_direction direction)
{
	if (direction != XP_CIPHER_ENCRYPT && direction != XP_CIPHER_DECRYPT)
		return false;
	if (direction == XP_CIPHER_ENCRYPT && legacy_algorithm(algorithm))
		return false;
	EVP_CIPHER *cipher = fetch_cipher(algorithm, mode,
	    representative_key_length(algorithm));
	bool supported = cipher != NULL;
	EVP_CIPHER_free(cipher);
	return supported;
}

static void
set_error(xp_cipher_t context, const char *fallback)
{
	unsigned long error = ERR_get_error();
	if (error != 0)
		ERR_error_string_n(error, context->err, sizeof(context->err));
	else
		snprintf(context->err, sizeof(context->err), "%s", fallback);
}

static int
append_input(xp_cipher_t context, const void *data, size_t len)
{
	if (len == 0)
		return XP_CRYPTO_OK;
	if (data == NULL || context->input_len > SIZE_MAX - len)
		return XP_CRYPTO_ERR_INVALID;
	unsigned char *replacement = realloc(context->input,
	    context->input_len + len);
	if (replacement == NULL)
		return XP_CRYPTO_ERR;
	memcpy(replacement + context->input_len, data, len);
	context->input = replacement;
	context->input_len += len;
	return XP_CRYPTO_OK;
}

int
xp_cipher_create(xp_cipher_t *out, const struct xp_cipher_config *config)
{
	if (out == NULL)
		return XP_CRYPTO_ERR_INVALID;
	*out = NULL;
	if (config == NULL || config->key == NULL
	    || (config->iv == NULL && config->iv_len != 0)
	    || config->key_len > 32 || config->iv_len > 16
	    || (config->direction != XP_CIPHER_ENCRYPT
	        && config->direction != XP_CIPHER_DECRYPT)
	    || (config->padding != XP_CIPHER_PADDING_NONE
	        && config->padding != XP_CIPHER_PADDING_PKCS7)
	    || (config->mode != XP_CIPHER_MODE_CBC
	        && config->padding != XP_CIPHER_PADDING_NONE)
	    || (legacy_algorithm(config->algorithm)
	        && config->direction != XP_CIPHER_DECRYPT))
		return XP_CRYPTO_ERR_POLICY;
	if ((config->algorithm == XP_CIPHER_CHACHA20 && config->iv_len != 12)
	    || (config->algorithm == XP_CIPHER_RC4 && config->iv_len != 0)
	    || ((config->algorithm == XP_CIPHER_AES
	         && config->mode != XP_CIPHER_MODE_GCM) && config->iv_len != 16)
	    || ((config->algorithm == XP_CIPHER_3DES
	         || config->algorithm == XP_CIPHER_CAST128) && config->iv_len != 8))
		return XP_CRYPTO_ERR_POLICY;
	EVP_CIPHER *cipher = fetch_cipher(config->algorithm, config->mode,
	    config->key_len);
	if (cipher == NULL)
		return cipher_name(config->algorithm, config->mode, config->key_len) == NULL
		    ? XP_CRYPTO_ERR_POLICY : XP_CRYPTO_ERR_UNSUPPORTED;
	xp_cipher_t context = calloc(1, sizeof(*context));
	if (context == NULL) {
		EVP_CIPHER_free(cipher);
		return XP_CRYPTO_ERR;
	}
	context->config = *config;
	context->config.key = context->key;
	context->config.iv = context->iv;
	context->cipher = cipher;
	context->native = EVP_CIPHER_CTX_new();
	memcpy(context->key, config->key, config->key_len);
	if (config->iv_len != 0)
		memcpy(context->iv, config->iv, config->iv_len);
	context->tag_len = config->mode == XP_CIPHER_MODE_GCM
	    ? (config->tag_len == 0 ? 16 : config->tag_len) : 0;
	if ((context->tag_len != 0
	     && (context->tag_len < 12 || context->tag_len > 16))
	    || context->native == NULL) {
		xp_cipher_free(context);
		return XP_CRYPTO_ERR_POLICY;
	}
	unsigned char native_iv[16];
	const unsigned char *iv = context->iv;
	if (config->algorithm == XP_CIPHER_CHACHA20) {
		native_iv[0] = (unsigned char)config->initial_counter;
		native_iv[1] = (unsigned char)(config->initial_counter >> 8);
		native_iv[2] = (unsigned char)(config->initial_counter >> 16);
		native_iv[3] = (unsigned char)(config->initial_counter >> 24);
		memcpy(native_iv + 4, context->iv, 12);
		iv = native_iv;
	}
	int encrypt = config->direction == XP_CIPHER_ENCRYPT;
	int ok = EVP_CipherInit_ex(context->native, context->cipher, NULL,
	    NULL, NULL, encrypt);
	if (ok == 1 && config->mode == XP_CIPHER_MODE_GCM)
		ok = EVP_CIPHER_CTX_ctrl(context->native, EVP_CTRL_AEAD_SET_IVLEN,
		    (int)config->iv_len, NULL);
	if (ok == 1 && (config->algorithm == XP_CIPHER_CAST128
	    || config->algorithm == XP_CIPHER_RC4))
		ok = EVP_CIPHER_CTX_set_key_length(context->native,
		    (int)config->key_len);
	if (ok == 1)
		ok = EVP_CipherInit_ex(context->native, NULL, NULL, context->key,
		    config->iv_len == 0 ? NULL : iv, encrypt);
	if (ok == 1)
		ok = EVP_CIPHER_CTX_set_padding(context->native,
		    config->padding == XP_CIPHER_PADDING_PKCS7);
	OPENSSL_cleanse(native_iv, sizeof(native_iv));
	if (ok != 1) {
		set_error(context, "cipher initialization failed");
		xp_cipher_free(context);
		return XP_CRYPTO_ERR;
	}
	*out = context;
	return XP_CRYPTO_OK;
}

int
xp_cipher_aad(xp_cipher_t context, const void *data, size_t len)
{
	if (context == NULL || context->finalized || context->text_started
	    || context->config.mode != XP_CIPHER_MODE_GCM
	    || (data == NULL && len != 0) || len > INT_MAX)
		return XP_CRYPTO_ERR_POLICY;
	int produced = 0;
	if (len != 0 && EVP_CipherUpdate(context->native, NULL, &produced,
	    data, (int)len) != 1) {
		set_error(context, "cipher AAD failed");
		return XP_CRYPTO_ERR;
	}
	return XP_CRYPTO_OK;
}

int
xp_cipher_update(xp_cipher_t context, const void *input, size_t input_len,
	void *out, size_t *out_len)
{
	if (context == NULL || out_len == NULL || context->finalized
	    || (input == NULL && input_len != 0) || input_len > INT_MAX)
		return XP_CRYPTO_ERR_INVALID;
	if (context->config.mode == XP_CIPHER_MODE_GCM
	    && context->config.direction == XP_CIPHER_DECRYPT) {
		int result = append_input(context, input, input_len);
		if (result == XP_CRYPTO_OK && input_len != 0)
			context->text_started = true;
		*out_len = 0;
		return result;
	}
	size_t block = (size_t)EVP_CIPHER_CTX_get_block_size(context->native);
	size_t required = input_len;
	if (block > 1) {
		if (input_len > SIZE_MAX - block)
			return XP_CRYPTO_ERR_INVALID;
		required += block;
	}
	if (out == NULL || *out_len < required) {
		*out_len = required;
		return XP_CRYPTO_ERR_BUFFER_TOO_SMALL;
	}
	int produced = 0;
	if (input_len != 0 && EVP_CipherUpdate(context->native, out, &produced,
	    input, (int)input_len) != 1) {
		set_error(context, "cipher update failed");
		return XP_CRYPTO_ERR;
	}
	if (input_len != 0)
		context->text_started = true;
	*out_len = (size_t)produced;
	return XP_CRYPTO_OK;
}

int
xp_cipher_set_tag(xp_cipher_t context, const void *tag, size_t len)
{
	if (context == NULL || context->finalized || tag == NULL
	    || context->config.mode != XP_CIPHER_MODE_GCM
	    || context->config.direction != XP_CIPHER_DECRYPT
	    || len != context->tag_len)
		return XP_CRYPTO_ERR_POLICY;
	memcpy(context->tag, tag, len);
	context->tag_set = true;
	return XP_CRYPTO_OK;
}

int
xp_cipher_final(xp_cipher_t context, void *out, size_t *out_len)
{
	if (context == NULL || out_len == NULL || context->finalized)
		return XP_CRYPTO_ERR_INVALID;
	bool gcm_decrypt = context->config.mode == XP_CIPHER_MODE_GCM
	    && context->config.direction == XP_CIPHER_DECRYPT;
	if (gcm_decrypt && !context->tag_set)
		return XP_CRYPTO_ERR_POLICY;
	if (gcm_decrypt && context->input_len > INT_MAX)
		return XP_CRYPTO_ERR_INVALID;
	size_t block = (size_t)EVP_CIPHER_CTX_get_block_size(context->native);
	size_t required = gcm_decrypt ? context->input_len : block;
	if (out == NULL || *out_len < required) {
		*out_len = required;
		return XP_CRYPTO_ERR_BUFFER_TOO_SMALL;
	}
	unsigned char *temporary = gcm_decrypt
	    ? malloc(required == 0 ? 1 : required) : out;
	if (gcm_decrypt && temporary == NULL)
		return XP_CRYPTO_ERR;
	int produced = 0;
	int tail = 0;
	int ok = 1;
	if (gcm_decrypt && context->input_len != 0)
		ok = EVP_CipherUpdate(context->native, temporary, &produced,
		    context->input, (int)context->input_len);
	if (ok == 1 && gcm_decrypt)
		ok = EVP_CIPHER_CTX_ctrl(context->native, EVP_CTRL_AEAD_SET_TAG,
		    (int)context->tag_len, context->tag);
	if (ok == 1)
		ok = EVP_CipherFinal_ex(context->native, temporary + produced, &tail);
	if (ok == 1 && context->config.mode == XP_CIPHER_MODE_GCM
	    && context->config.direction == XP_CIPHER_ENCRYPT)
		ok = EVP_CIPHER_CTX_ctrl(context->native, EVP_CTRL_AEAD_GET_TAG,
		    (int)context->tag_len, context->tag);
	if (ok != 1) {
		if (gcm_decrypt) {
			OPENSSL_cleanse(temporary, required);
			free(temporary);
		}
		set_error(context, gcm_decrypt ? "cipher authentication failed"
		    : "cipher finalization failed");
		return gcm_decrypt ? XP_CRYPTO_ERR_VERIFY : XP_CRYPTO_ERR;
	}
	if (gcm_decrypt) {
		memcpy(out, temporary, (size_t)(produced + tail));
		OPENSSL_cleanse(temporary, required);
		free(temporary);
	}
	*out_len = (size_t)(produced + tail);
	context->finalized = true;
	return XP_CRYPTO_OK;
}

int
xp_cipher_get_tag(xp_cipher_t context, void *tag, size_t *len)
{
	if (context == NULL || len == NULL
	    || context->config.mode != XP_CIPHER_MODE_GCM
	    || context->config.direction != XP_CIPHER_ENCRYPT
	    || !context->finalized)
		return XP_CRYPTO_ERR_POLICY;
	if (tag == NULL) {
		*len = context->tag_len;
		return XP_CRYPTO_OK;
	}
	if (*len < context->tag_len) {
		*len = context->tag_len;
		return XP_CRYPTO_ERR_BUFFER_TOO_SMALL;
	}
	memcpy(tag, context->tag, context->tag_len);
	*len = context->tag_len;
	return XP_CRYPTO_OK;
}

const char *
xp_cipher_errstr(xp_cipher_t context)
{
	return context == NULL ? "(null xp_cipher_t)"
	    : context->err[0] == '\0' ? "no error" : context->err;
}

void
xp_cipher_free(xp_cipher_t context)
{
	if (context != NULL) {
		EVP_CIPHER_CTX_free(context->native);
		EVP_CIPHER_free(context->cipher);
		OPENSSL_cleanse(context->key, sizeof(context->key));
		OPENSSL_cleanse(context->tag, sizeof(context->tag));
		if (context->input != NULL)
			OPENSSL_clear_free(context->input, context->input_len);
		free(context);
	}
}
