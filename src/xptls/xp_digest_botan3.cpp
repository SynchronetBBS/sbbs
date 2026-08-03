#include "xp_digest.h"

#include <memory>

#include <botan/hash.h>

struct xp_digest_context {
	std::unique_ptr<Botan::HashFunction> native;
};

static const char *
algorithm_name(enum xp_digest_algorithm algorithm)
{
	switch (algorithm) {
		case XP_DIGEST_SHA256: return "SHA-256";
		case XP_DIGEST_SHA384: return "SHA-384";
		case XP_DIGEST_SHA512: return "SHA-512";
		default: return nullptr;
	}
}

extern "C" size_t
xp_digest_size(enum xp_digest_algorithm algorithm)
{
	try {
		const char *name = algorithm_name(algorithm);
		auto hash = name == nullptr ? nullptr : Botan::HashFunction::create(name);
		return hash == nullptr ? 0 : hash->output_length();
	} catch (...) {
		return 0;
	}
}

extern "C" xp_digest_t
xp_digest_create(enum xp_digest_algorithm algorithm)
{
	try {
		const char *name = algorithm_name(algorithm);
		auto hash = name == nullptr ? nullptr : Botan::HashFunction::create(name);
		if (hash == nullptr)
			return nullptr;
		auto result = std::make_unique<xp_digest_context>();
		result->native = std::move(hash);
		return result.release();
	} catch (...) {
		return nullptr;
	}
}

extern "C" int
xp_digest_update(xp_digest_t context, const void *data, size_t len)
{
	try {
		if (context == nullptr || (data == nullptr && len != 0))
			return -1;
		context->native->update(static_cast<const uint8_t *>(data), len);
		return 0;
	} catch (...) {
		return -1;
	}
}

extern "C" int
xp_digest_final(xp_digest_t context, void *out, size_t *len)
{
	try {
		if (context == nullptr || len == nullptr)
			return -1;
		size_t required = context->native->output_length();
		if (out == nullptr) {
			*len = required;
			return 0;
		}
		if (*len < required) {
			*len = required;
			return -1;
		}
		context->native->final(static_cast<uint8_t *>(out));
		*len = required;
		return 0;
	} catch (...) {
		return -1;
	}
}

extern "C" void
xp_digest_free(xp_digest_t context)
{
	delete context;
}

extern "C" int
xp_digest(enum xp_digest_algorithm algorithm, const void *data, size_t len,
	      void *out, size_t *out_len)
{
	xp_digest_t context = xp_digest_create(algorithm);
	if (context == nullptr)
		return -1;
	int status = xp_digest_update(context, data, len);
	if (status == 0)
		status = xp_digest_final(context, out, out_len);
	xp_digest_free(context);
	return status;
}
