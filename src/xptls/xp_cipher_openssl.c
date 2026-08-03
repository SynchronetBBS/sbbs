#include "xp_cipher.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <openssl/evp.h>

struct xp_cipher_context {
	struct xp_cipher_config config;
	unsigned char key[32];
	unsigned char iv[16];
	unsigned char tag[16];
	size_t tag_len;
	unsigned char *aad;
	size_t aad_len;
	unsigned char *input;
	size_t input_len;
	bool finalized;
	char err[256];
};

static int
append(unsigned char **buffer, size_t *length, const void *data, size_t len)
{
	if (len == 0)
		return XP_CRYPTO_OK;
	if (data == NULL || *length > SIZE_MAX - len)
		return XP_CRYPTO_ERR_INVALID;
	unsigned char *replacement = realloc(*buffer, *length + len);
	if (replacement == NULL)
		return XP_CRYPTO_ERR;
	memcpy(replacement + *length, data, len);
	*buffer = replacement;
	*length += len;
	return XP_CRYPTO_OK;
}

static const EVP_CIPHER *
cipher_for(const struct xp_cipher_config *config)
{
	if (config->algorithm == XP_CIPHER_CHACHA20)
		return config->mode == XP_CIPHER_MODE_STREAM && config->key_len == 32
			&& config->iv_len == 16 ? EVP_chacha20() : NULL;
	if (config->algorithm != XP_CIPHER_AES
	    || (config->key_len != 16 && config->key_len != 24 && config->key_len != 32)
	    || config->iv_len != 16)
		return NULL;
	if (config->mode == XP_CIPHER_MODE_CBC)
		return config->key_len == 16 ? EVP_aes_128_cbc()
			: config->key_len == 24 ? EVP_aes_192_cbc() : EVP_aes_256_cbc();
	if (config->mode == XP_CIPHER_MODE_CFB)
		return config->key_len == 16 ? EVP_aes_128_cfb128()
			: config->key_len == 24 ? EVP_aes_192_cfb128() : EVP_aes_256_cfb128();
	if (config->mode == XP_CIPHER_MODE_GCM)
		return config->key_len == 16 ? EVP_aes_128_gcm()
			: config->key_len == 24 ? EVP_aes_192_gcm() : EVP_aes_256_gcm();
	return NULL;
}

bool
xp_cipher_supported(enum xp_cipher_algorithm algorithm, enum xp_cipher_mode mode)
{
	return (algorithm == XP_CIPHER_AES
	        && (mode == XP_CIPHER_MODE_CBC || mode == XP_CIPHER_MODE_CFB
	            || mode == XP_CIPHER_MODE_GCM))
		|| (algorithm == XP_CIPHER_CHACHA20 && mode == XP_CIPHER_MODE_STREAM);
}

int
xp_cipher_create(xp_cipher_t *out, const struct xp_cipher_config *config)
{
	if (out == NULL)
		return XP_CRYPTO_ERR_INVALID;
	*out = NULL;
	if (config == NULL || config->key == NULL || config->iv == NULL
	    || config->key_len > 32 || config->iv_len > 16
	    || (config->direction != XP_CIPHER_ENCRYPT
	        && config->direction != XP_CIPHER_DECRYPT)
	    || cipher_for(config) == NULL
	    || (config->mode != XP_CIPHER_MODE_CBC
	        && config->padding != XP_CIPHER_PADDING_NONE)
	    || (config->mode == XP_CIPHER_MODE_GCM
	        && config->padding != XP_CIPHER_PADDING_NONE))
		return XP_CRYPTO_ERR_POLICY;
	xp_cipher_t context = calloc(1, sizeof(*context));
	if (context == NULL)
		return XP_CRYPTO_ERR;
	context->config = *config;
	context->config.key = context->key;
	context->config.iv = context->iv;
	memcpy(context->key, config->key, config->key_len);
	memcpy(context->iv, config->iv, config->iv_len);
	context->tag_len = config->mode == XP_CIPHER_MODE_GCM
		? (config->tag_len == 0 ? 16 : config->tag_len) : 0;
	if (context->tag_len != 0 && (context->tag_len < 12 || context->tag_len > 16)) {
		xp_cipher_free(context);
		return XP_CRYPTO_ERR_POLICY;
	}
	*out = context;
	return XP_CRYPTO_OK;
}

int
xp_cipher_aad(xp_cipher_t context, const void *data, size_t len)
{
	if (context == NULL || context->finalized || context->config.mode != XP_CIPHER_MODE_GCM
	    || context->input_len != 0)
		return XP_CRYPTO_ERR_POLICY;
	return append(&context->aad, &context->aad_len, data, len);
}

int
xp_cipher_update(xp_cipher_t context, const void *input, size_t input_len,
	void *out, size_t *out_len)
{
	(void)out;
	if (context == NULL || out_len == NULL || context->finalized)
		return XP_CRYPTO_ERR_INVALID;
	*out_len = 0;
	return append(&context->input, &context->input_len, input, input_len);
}

int
xp_cipher_set_tag(xp_cipher_t context, const void *tag, size_t len)
{
	if (context == NULL || context->finalized || tag == NULL
	    || context->config.mode != XP_CIPHER_MODE_GCM
	    || context->config.direction != XP_CIPHER_DECRYPT || len != context->tag_len)
		return XP_CRYPTO_ERR_POLICY;
	memcpy(context->tag, tag, len);
	return XP_CRYPTO_OK;
}

int
xp_cipher_final(xp_cipher_t context, void *out, size_t *out_len)
{
	if (context == NULL || out_len == NULL || context->finalized)
		return XP_CRYPTO_ERR_INVALID;
	size_t required = context->input_len
		+ (context->config.padding == XP_CIPHER_PADDING_PKCS7 ? 16 : 0);
	if (out == NULL || *out_len < required) {
		*out_len = required;
		return XP_CRYPTO_ERR_BUFFER_TOO_SMALL;
	}
	if (context->input_len > INT_MAX || context->aad_len > INT_MAX)
		return XP_CRYPTO_ERR_INVALID;
	EVP_CIPHER_CTX *native = EVP_CIPHER_CTX_new();
	const EVP_CIPHER *cipher = cipher_for(&context->config);
	int encrypt = context->config.direction == XP_CIPHER_ENCRYPT;
	int produced = 0, tail = 0;
	unsigned char *temporary = malloc(required == 0 ? 1 : required);
	int ok = native != NULL
		&& EVP_CipherInit_ex(native, cipher, NULL, NULL, NULL, encrypt) == 1;
	if (ok && context->config.mode == XP_CIPHER_MODE_GCM)
		ok = EVP_CIPHER_CTX_ctrl(native, EVP_CTRL_AEAD_SET_IVLEN,
		                         (int)context->config.iv_len, NULL) == 1;
	if (ok)
		ok = EVP_CipherInit_ex(native, NULL, NULL, context->key, context->iv, encrypt) == 1;
	if (ok)
		EVP_CIPHER_CTX_set_padding(native,
			context->config.padding == XP_CIPHER_PADDING_PKCS7);
	if (ok && context->aad_len != 0)
		ok = EVP_CipherUpdate(native, NULL, &tail, context->aad,
		                      (int)context->aad_len) == 1;
	if (ok && context->input_len != 0)
		ok = EVP_CipherUpdate(native, temporary, &produced, context->input,
		                      (int)context->input_len) == 1;
	if (ok && context->config.mode == XP_CIPHER_MODE_GCM && !encrypt)
		ok = EVP_CIPHER_CTX_ctrl(native, EVP_CTRL_AEAD_SET_TAG,
		                         (int)context->tag_len, context->tag) == 1;
	if (ok)
		ok = EVP_CipherFinal_ex(native, temporary + produced, &tail) == 1;
	if (ok && context->config.mode == XP_CIPHER_MODE_GCM && encrypt)
		ok = EVP_CIPHER_CTX_ctrl(native, EVP_CTRL_AEAD_GET_TAG,
		                         (int)context->tag_len, context->tag) == 1;
	EVP_CIPHER_CTX_free(native);
	if (!ok) {
		memset(temporary, 0, required);
		free(temporary);
		snprintf(context->err, sizeof(context->err), "%s",
		         context->config.mode == XP_CIPHER_MODE_GCM && !encrypt
		         ? "cipher authentication failed" : "cipher finalization failed");
		return context->config.mode == XP_CIPHER_MODE_GCM && !encrypt
			? XP_CRYPTO_ERR_VERIFY : XP_CRYPTO_ERR;
	}
	memcpy(out, temporary, (size_t)(produced + tail));
	memset(temporary, 0, required);
	free(temporary);
	*out_len = (size_t)(produced + tail);
	context->finalized = true;
	return XP_CRYPTO_OK;
}

int
xp_cipher_get_tag(xp_cipher_t context, void *tag, size_t *len)
{
	if (context == NULL || len == NULL || context->config.mode != XP_CIPHER_MODE_GCM
	    || context->config.direction != XP_CIPHER_ENCRYPT || !context->finalized)
		return XP_CRYPTO_ERR_POLICY;
	if (tag == NULL || *len < context->tag_len) {
		*len = context->tag_len;
		return tag == NULL ? XP_CRYPTO_OK : XP_CRYPTO_ERR_BUFFER_TOO_SMALL;
	}
	memcpy(tag, context->tag, context->tag_len);
	*len = context->tag_len;
	return XP_CRYPTO_OK;
}

const char *xp_cipher_errstr(xp_cipher_t context)
{
	return context == NULL ? "(null xp_cipher_t)"
		: context->err[0] == '\0' ? "no error" : context->err;
}

void
xp_cipher_free(xp_cipher_t context)
{
	if (context != NULL) {
		memset(context->key, 0, sizeof(context->key));
		memset(context->tag, 0, sizeof(context->tag));
		if (context->input != NULL)
			memset(context->input, 0, context->input_len);
		free(context->input);
		free(context->aad);
		free(context);
	}
}
