#include "xp_cipher.h"

#include <botan/aead.h>
#include <botan/cipher_mode.h>
#include <botan/secmem.h>
#include <botan/stream_cipher.h>

#include <cstring>
#include <memory>
#include <new>
#include <string>
#include <vector>

struct xp_cipher_context {
	struct xp_cipher_config config{};
	Botan::secure_vector<uint8_t> key;
	Botan::secure_vector<uint8_t> iv;
	Botan::secure_vector<uint8_t> tag;
	std::vector<uint8_t> aad;
	Botan::secure_vector<uint8_t> input;
	bool finalized{false};
	std::string err;
};

static const char *
mode_name(const struct xp_cipher_config& config, std::string& storage)
{
	if (config.algorithm == XP_CIPHER_CHACHA20)
		return config.mode == XP_CIPHER_MODE_STREAM && config.key_len == 32
			&& config.iv_len == 16 ? "ChaCha(20)" : nullptr;
	if (config.algorithm != XP_CIPHER_AES
	    || (config.key_len != 16 && config.key_len != 24 && config.key_len != 32)
	    || config.iv_len != 16)
		return nullptr;
	storage = "AES-" + std::to_string(config.key_len * 8);
	if (config.mode == XP_CIPHER_MODE_CBC)
		storage += config.padding == XP_CIPHER_PADDING_PKCS7
			? "/CBC/PKCS7" : "/CBC/NoPadding";
	else if (config.mode == XP_CIPHER_MODE_CFB && config.padding == XP_CIPHER_PADDING_NONE)
		storage += "/CFB";
	else if (config.mode == XP_CIPHER_MODE_GCM && config.padding == XP_CIPHER_PADDING_NONE)
		storage += "/GCM";
	else
		return nullptr;
	return storage.c_str();
}

extern "C" bool
xp_cipher_supported(enum xp_cipher_algorithm algorithm, enum xp_cipher_mode mode)
{
	return (algorithm == XP_CIPHER_AES
	        && (mode == XP_CIPHER_MODE_CBC || mode == XP_CIPHER_MODE_CFB
	            || mode == XP_CIPHER_MODE_GCM))
		|| (algorithm == XP_CIPHER_CHACHA20 && mode == XP_CIPHER_MODE_STREAM);
}

extern "C" int
xp_cipher_create(xp_cipher_t *out, const struct xp_cipher_config *config)
{
	if (out == nullptr)
		return XP_CRYPTO_ERR_INVALID;
	*out = nullptr;
	if (config == nullptr || config->key == nullptr || config->iv == nullptr
	    || (config->direction != XP_CIPHER_ENCRYPT
	        && config->direction != XP_CIPHER_DECRYPT))
		return XP_CRYPTO_ERR_INVALID;
	std::string name;
	if (mode_name(*config, name) == nullptr)
		return XP_CRYPTO_ERR_POLICY;
	auto context = std::unique_ptr<xp_cipher_context>(
		new (std::nothrow) xp_cipher_context());
	if (!context)
		return XP_CRYPTO_ERR;
	context->config = *config;
	context->config.key = nullptr;
	context->config.iv = nullptr;
	context->key.assign(static_cast<const uint8_t *>(config->key),
		static_cast<const uint8_t *>(config->key) + config->key_len);
	context->iv.assign(static_cast<const uint8_t *>(config->iv),
		static_cast<const uint8_t *>(config->iv) + config->iv_len);
	size_t tag_len = config->mode == XP_CIPHER_MODE_GCM
		? (config->tag_len == 0 ? 16 : config->tag_len) : 0;
	if (tag_len != 0 && (tag_len < 12 || tag_len > 16))
		return XP_CRYPTO_ERR_POLICY;
	context->tag.resize(tag_len);
	*out = context.release();
	return XP_CRYPTO_OK;
}

extern "C" int
xp_cipher_aad(xp_cipher_t context, const void *data, size_t len)
{
	if (context == nullptr || context->finalized
	    || context->config.mode != XP_CIPHER_MODE_GCM || !context->input.empty()
	    || (data == nullptr && len != 0))
		return XP_CRYPTO_ERR_POLICY;
	const auto *bytes = static_cast<const uint8_t *>(data);
	context->aad.insert(context->aad.end(), bytes, bytes + len);
	return XP_CRYPTO_OK;
}

extern "C" int
xp_cipher_update(xp_cipher_t context, const void *input, size_t input_len,
	void *out, size_t *out_len)
{
	(void)out;
	if (context == nullptr || out_len == nullptr || context->finalized
	    || (input == nullptr && input_len != 0))
		return XP_CRYPTO_ERR_INVALID;
	const auto *bytes = static_cast<const uint8_t *>(input);
	context->input.insert(context->input.end(), bytes, bytes + input_len);
	*out_len = 0;
	return XP_CRYPTO_OK;
}

extern "C" int
xp_cipher_set_tag(xp_cipher_t context, const void *tag, size_t len)
{
	if (context == nullptr || context->finalized || tag == nullptr
	    || context->config.mode != XP_CIPHER_MODE_GCM
	    || context->config.direction != XP_CIPHER_DECRYPT
	    || len != context->tag.size())
		return XP_CRYPTO_ERR_POLICY;
	std::memcpy(context->tag.data(), tag, len);
	return XP_CRYPTO_OK;
}

extern "C" int
xp_cipher_final(xp_cipher_t context, void *out, size_t *out_len)
{
	if (context == nullptr || out_len == nullptr || context->finalized)
		return XP_CRYPTO_ERR_INVALID;
	size_t maximum = context->input.size()
		+ (context->config.padding == XP_CIPHER_PADDING_PKCS7 ? 16 : 0);
	if (out == nullptr || *out_len < maximum) {
		*out_len = maximum;
		return XP_CRYPTO_ERR_BUFFER_TOO_SMALL;
	}
	try {
		if (context->config.algorithm == XP_CIPHER_CHACHA20) {
			auto cipher = Botan::StreamCipher::create_or_throw("ChaCha(20)");
			cipher->set_key(context->key);
			cipher->set_iv(context->iv.data(), context->iv.size());
			cipher->cipher1(context->input.data(), context->input.size());
		} else {
			std::string name;
			auto cipher = Botan::Cipher_Mode::create_or_throw(
				mode_name(context->config, name),
				context->config.direction == XP_CIPHER_ENCRYPT
				? Botan::Cipher_Dir::Encryption : Botan::Cipher_Dir::Decryption);
			cipher->set_key(context->key);
			if (!context->aad.empty()) {
				auto *aead = dynamic_cast<Botan::AEAD_Mode *>(cipher.get());
				if (aead == nullptr)
					throw std::runtime_error("AAD supplied to a non-AEAD cipher");
				aead->set_associated_data(context->aad);
			}
			cipher->start(context->iv);
			if (context->config.mode == XP_CIPHER_MODE_GCM
			    && context->config.direction == XP_CIPHER_DECRYPT)
				context->input.insert(context->input.end(),
					context->tag.begin(), context->tag.end());
			cipher->finish(context->input);
			if (context->config.mode == XP_CIPHER_MODE_GCM
			    && context->config.direction == XP_CIPHER_ENCRYPT) {
				if (context->input.size() < context->tag.size())
					throw std::runtime_error("invalid GCM output");
				std::copy(context->input.end() - context->tag.size(),
					context->input.end(), context->tag.begin());
				context->input.resize(context->input.size() - context->tag.size());
			}
		}
		std::memcpy(out, context->input.data(), context->input.size());
		*out_len = context->input.size();
		context->finalized = true;
		return XP_CRYPTO_OK;
	} catch (const std::exception& e) {
		context->err = e.what();
		return context->config.mode == XP_CIPHER_MODE_GCM
		    && context->config.direction == XP_CIPHER_DECRYPT
			? XP_CRYPTO_ERR_VERIFY : XP_CRYPTO_ERR;
	}
}

extern "C" int
xp_cipher_get_tag(xp_cipher_t context, void *tag, size_t *len)
{
	if (context == nullptr || len == nullptr
	    || context->config.mode != XP_CIPHER_MODE_GCM
	    || context->config.direction != XP_CIPHER_ENCRYPT || !context->finalized)
		return XP_CRYPTO_ERR_POLICY;
	if (tag == nullptr) {
		*len = context->tag.size();
		return XP_CRYPTO_OK;
	}
	if (*len < context->tag.size()) {
		*len = context->tag.size();
		return XP_CRYPTO_ERR_BUFFER_TOO_SMALL;
	}
	std::memcpy(tag, context->tag.data(), context->tag.size());
	*len = context->tag.size();
	return XP_CRYPTO_OK;
}

extern "C" const char *
xp_cipher_errstr(xp_cipher_t context)
{
	return context == nullptr ? "(null xp_cipher_t)"
		: context->err.empty() ? "no error" : context->err.c_str();
}

extern "C" void xp_cipher_free(xp_cipher_t context) { delete context; }
