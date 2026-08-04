#include "xp_cipher.h"

#include <botan/aead.h>
#include <botan/cipher_mode.h>
#include <botan/exceptn.h>
#include <botan/secmem.h>
#include <botan/stream_cipher.h>

#include <algorithm>
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
	Botan::secure_vector<uint8_t> pending;
	std::vector<uint8_t> aad;
	std::unique_ptr<Botan::Cipher_Mode> mode;
	std::unique_ptr<Botan::StreamCipher> stream;
	bool started{false};
	bool text_started{false};
	bool tag_set{false};
	bool finalized{false};
	std::string err;
};

static bool
legacy_algorithm(enum xp_cipher_algorithm algorithm)
{
	return algorithm == XP_CIPHER_3DES || algorithm == XP_CIPHER_CAST128
	    || algorithm == XP_CIPHER_RC4;
}

static std::string
cipher_name(const struct xp_cipher_config& config)
{
	if (config.algorithm == XP_CIPHER_CHACHA20)
		return config.mode == XP_CIPHER_MODE_STREAM && config.key_len == 32
		    ? "ChaCha(20)" : "";
	if (config.algorithm == XP_CIPHER_RC4)
		return config.mode == XP_CIPHER_MODE_STREAM && config.key_len > 0
		    && config.key_len <= 32 ? "RC4" : "";
	std::string base;
	if (config.algorithm == XP_CIPHER_AES) {
		if (config.key_len != 16 && config.key_len != 24 && config.key_len != 32)
			return "";
		base = "AES-" + std::to_string(config.key_len * 8);
	}
	else if (config.algorithm == XP_CIPHER_3DES && config.key_len == 24)
		base = "TripleDES";
	else if (config.algorithm == XP_CIPHER_CAST128
	    && config.key_len >= 5 && config.key_len <= 16)
		base = "CAST-128";
	else
		return "";
	if (config.mode == XP_CIPHER_MODE_CBC) {
		if (config.padding == XP_CIPHER_PADDING_PKCS7)
			return base + "/CBC/PKCS7";
		if (config.padding == XP_CIPHER_PADDING_NONE)
			return base + "/CBC/NoPadding";
	}
	else if (config.algorithm == XP_CIPHER_AES
	    && config.mode == XP_CIPHER_MODE_CFB
	    && config.padding == XP_CIPHER_PADDING_NONE)
		return base + "/CFB";
	else if (config.algorithm == XP_CIPHER_AES
	    && config.mode == XP_CIPHER_MODE_GCM
	    && config.padding == XP_CIPHER_PADDING_NONE) {
		size_t tag_len = config.tag_len == 0 ? 16 : config.tag_len;
		return base + "/GCM(" + std::to_string(tag_len) + ")";
	}
	return "";
}

static bool
can_create(enum xp_cipher_algorithm algorithm, enum xp_cipher_mode mode,
	enum xp_cipher_direction direction)
{
	if (direction != XP_CIPHER_ENCRYPT && direction != XP_CIPHER_DECRYPT)
		return false;
	if (direction == XP_CIPHER_ENCRYPT && legacy_algorithm(algorithm))
		return false;
	struct xp_cipher_config config{};
	config.algorithm = algorithm;
	config.mode = mode;
	config.direction = direction;
	config.padding = XP_CIPHER_PADDING_NONE;
	config.key_len = algorithm == XP_CIPHER_3DES ? 24
	    : algorithm == XP_CIPHER_CAST128 || algorithm == XP_CIPHER_RC4 ? 16 : 32;
	config.tag_len = 16;
	std::string name = cipher_name(config);
	if (name.empty())
		return false;
	if (algorithm == XP_CIPHER_CHACHA20 || algorithm == XP_CIPHER_RC4)
		return Botan::StreamCipher::create(name) != nullptr;
	return Botan::Cipher_Mode::create(name,
	    direction == XP_CIPHER_ENCRYPT ? Botan::Cipher_Dir::Encryption
	                                   : Botan::Cipher_Dir::Decryption) != nullptr;
}

extern "C" bool
xp_cipher_supported(enum xp_cipher_algorithm algorithm,
	enum xp_cipher_mode mode, enum xp_cipher_direction direction)
{
	try {
		return can_create(algorithm, mode, direction);
	}
	catch (...) {
		return false;
	}
}

static int
start_mode(xp_cipher_t context)
{
	if (context->started)
		return XP_CRYPTO_OK;
	try {
		if (context->config.mode == XP_CIPHER_MODE_GCM) {
			auto *aead = dynamic_cast<Botan::AEAD_Mode *>(context->mode.get());
			if (aead == nullptr)
				return XP_CRYPTO_ERR;
			aead->set_associated_data(context->aad);
		}
		context->mode->start(context->iv);
		context->started = true;
		return XP_CRYPTO_OK;
	}
	catch (const std::exception& e) {
		context->err = e.what();
		return XP_CRYPTO_ERR;
	}
}

extern "C" int
xp_cipher_create(xp_cipher_t *out, const struct xp_cipher_config *config)
{
	if (out == nullptr)
		return XP_CRYPTO_ERR_INVALID;
	*out = nullptr;
	if (config == nullptr || config->key == nullptr
	    || (config->iv == nullptr && config->iv_len != 0)
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
	    || (config->algorithm == XP_CIPHER_AES
	        && config->mode != XP_CIPHER_MODE_GCM && config->iv_len != 16)
	    || ((config->algorithm == XP_CIPHER_3DES
	         || config->algorithm == XP_CIPHER_CAST128) && config->iv_len != 8))
		return XP_CRYPTO_ERR_POLICY;
	size_t tag_len = config->mode == XP_CIPHER_MODE_GCM
	    ? (config->tag_len == 0 ? 16 : config->tag_len) : 0;
	if (tag_len != 0 && (tag_len < 12 || tag_len > 16))
		return XP_CRYPTO_ERR_POLICY;
	std::string name = cipher_name(*config);
	if (name.empty())
		return XP_CRYPTO_ERR_POLICY;
	try {
		auto context = std::unique_ptr<xp_cipher_context>(
		    new (std::nothrow) xp_cipher_context());
		if (!context)
			return XP_CRYPTO_ERR;
		context->config = *config;
		context->config.key = nullptr;
		context->config.iv = nullptr;
		const auto *key = static_cast<const uint8_t *>(config->key);
		context->key.assign(key, key + config->key_len);
		if (config->iv_len != 0) {
			const auto *iv = static_cast<const uint8_t *>(config->iv);
			context->iv.assign(iv, iv + config->iv_len);
		}
		context->tag.resize(tag_len);
		if (config->algorithm == XP_CIPHER_CHACHA20
		    || config->algorithm == XP_CIPHER_RC4) {
			context->stream = Botan::StreamCipher::create(name);
			if (!context->stream)
				return XP_CRYPTO_ERR_UNSUPPORTED;
			context->stream->set_key(context->key);
			context->stream->set_iv(context->iv);
			if (config->algorithm == XP_CIPHER_CHACHA20)
				context->stream->seek(static_cast<uint64_t>(
				    config->initial_counter) * 64);
		}
		else {
			context->mode = Botan::Cipher_Mode::create(name,
			    config->direction == XP_CIPHER_ENCRYPT
			        ? Botan::Cipher_Dir::Encryption : Botan::Cipher_Dir::Decryption);
			if (!context->mode)
				return XP_CRYPTO_ERR_UNSUPPORTED;
			context->mode->set_key(context->key);
			if (!context->mode->valid_nonce_length(config->iv_len))
				return XP_CRYPTO_ERR_POLICY;
		}
		*out = context.release();
		return XP_CRYPTO_OK;
	}
	catch (...) {
		return XP_CRYPTO_ERR;
	}
}

extern "C" int
xp_cipher_aad(xp_cipher_t context, const void *data, size_t len)
{
	if (context == nullptr || context->finalized || context->text_started
	    || context->config.mode != XP_CIPHER_MODE_GCM
	    || (data == nullptr && len != 0))
		return XP_CRYPTO_ERR_POLICY;
	const auto *bytes = static_cast<const uint8_t *>(data);
	if (len != 0)
		context->aad.insert(context->aad.end(), bytes, bytes + len);
	return XP_CRYPTO_OK;
}

static size_t
processable(xp_cipher_t context, size_t total)
{
	size_t granularity = context->mode->update_granularity();
	size_t reserve = 0;
	if (context->config.padding == XP_CIPHER_PADDING_PKCS7
	    && context->config.direction == XP_CIPHER_DECRYPT)
		reserve = context->mode->minimum_final_size();
	if (total <= reserve)
		return 0;
	return ((total - reserve) / granularity) * granularity;
}

extern "C" int
xp_cipher_update(xp_cipher_t context, const void *input, size_t input_len,
	void *out, size_t *out_len)
{
	if (context == nullptr || out_len == nullptr || context->finalized
	    || (input == nullptr && input_len != 0))
		return XP_CRYPTO_ERR_INVALID;
	if (context->config.mode == XP_CIPHER_MODE_GCM) {
		try {
			const auto *bytes = static_cast<const uint8_t *>(input);
			if (input_len != 0) {
				context->pending.insert(context->pending.end(), bytes,
				    bytes + input_len);
				context->text_started = true;
			}
			*out_len = 0;
			return XP_CRYPTO_OK;
		}
		catch (const std::exception& e) {
			context->err = e.what();
			return XP_CRYPTO_ERR;
		}
	}
	if (context->stream) {
		if (out == nullptr || *out_len < input_len) {
			*out_len = input_len;
			return XP_CRYPTO_ERR_BUFFER_TOO_SMALL;
		}
		try {
			context->stream->cipher(static_cast<const uint8_t *>(input),
			    static_cast<uint8_t *>(out), input_len);
			if (input_len != 0)
				context->text_started = true;
			*out_len = input_len;
			return XP_CRYPTO_OK;
		}
		catch (const std::exception& e) {
			context->err = e.what();
			return XP_CRYPTO_ERR;
		}
	}
	if (context->pending.size() > SIZE_MAX - input_len)
		return XP_CRYPTO_ERR_INVALID;
	size_t count = processable(context, context->pending.size() + input_len);
	if (out == nullptr || *out_len < count) {
		*out_len = count;
		return XP_CRYPTO_ERR_BUFFER_TOO_SMALL;
	}
	try {
		int result = start_mode(context);
		if (result != XP_CRYPTO_OK)
			return result;
		const auto *bytes = static_cast<const uint8_t *>(input);
		if (input_len != 0)
			context->pending.insert(context->pending.end(), bytes, bytes + input_len);
		if (input_len != 0)
			context->text_started = true;
		if (count == 0) {
			*out_len = 0;
			return XP_CRYPTO_OK;
		}
		Botan::secure_vector<uint8_t> chunk(context->pending.begin(),
		    context->pending.begin() + count);
		context->mode->update(chunk);
		std::memcpy(out, chunk.data(), chunk.size());
		context->pending.erase(context->pending.begin(),
		    context->pending.begin() + count);
		*out_len = chunk.size();
		return XP_CRYPTO_OK;
	}
	catch (const std::exception& e) {
		context->err = e.what();
		return XP_CRYPTO_ERR;
	}
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
	context->tag_set = true;
	return XP_CRYPTO_OK;
}

extern "C" int
xp_cipher_final(xp_cipher_t context, void *out, size_t *out_len)
{
	if (context == nullptr || out_len == nullptr || context->finalized)
		return XP_CRYPTO_ERR_INVALID;
	if (context->stream) {
		context->finalized = true;
		*out_len = 0;
		return XP_CRYPTO_OK;
	}
	bool gcm = context->config.mode == XP_CIPHER_MODE_GCM;
	bool decrypt = context->config.direction == XP_CIPHER_DECRYPT;
	if (gcm && decrypt && !context->tag_set)
		return XP_CRYPTO_ERR_POLICY;
	if (!gcm && context->config.padding == XP_CIPHER_PADDING_NONE
	    && context->pending.empty()) {
		context->finalized = true;
		*out_len = 0;
		return XP_CRYPTO_OK;
	}
	size_t input_size = context->pending.size()
	    + (gcm && decrypt ? context->tag.size() : 0);
	size_t maximum;
	try {
		maximum = context->mode->output_length(input_size);
	}
	catch (...) {
		return XP_CRYPTO_ERR;
	}
	if (out == nullptr || *out_len < maximum) {
		*out_len = maximum;
		return XP_CRYPTO_ERR_BUFFER_TOO_SMALL;
	}
	try {
		int result = start_mode(context);
		if (result != XP_CRYPTO_OK)
			return result;
		Botan::secure_vector<uint8_t> final = context->pending;
		if (gcm && decrypt)
			final.insert(final.end(), context->tag.begin(), context->tag.end());
		context->mode->finish(final);
		if (gcm && !decrypt) {
			if (final.size() < context->tag.size())
				return XP_CRYPTO_ERR;
			std::copy(final.end() - context->tag.size(), final.end(),
			    context->tag.begin());
			final.resize(final.size() - context->tag.size());
		}
		std::memcpy(out, final.data(), final.size());
		*out_len = final.size();
		context->pending.clear();
		context->finalized = true;
		return XP_CRYPTO_OK;
	}
	catch (const Botan::Invalid_Authentication_Tag& e) {
		context->err = e.what();
		return XP_CRYPTO_ERR_VERIFY;
	}
	catch (const std::exception& e) {
		context->err = e.what();
		return XP_CRYPTO_ERR;
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

extern "C" void
xp_cipher_free(xp_cipher_t context)
{
	delete context;
}
