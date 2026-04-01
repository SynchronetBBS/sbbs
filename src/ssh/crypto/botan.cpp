/*
 * crypto/botan.cpp -- Botan3 C++ implementation of deucessh-crypto.h.
 *
 * Uses Botan's native C++ API directly instead of the FFI layer.
 * All exported functions use C linkage (via the extern "C" in the headers).
 */

#include <climits>
#include <cstdlib>
#include <memory>
#include <string>

#include <botan/base64.h>
#include <botan/hash.h>
#include <botan/mem_ops.h>
#include <botan/system_rng.h>

#include "deucessh.h"
#include "deucessh-crypto.h"

struct dssh_hash_ctx {
	std::unique_ptr<Botan::HashFunction> hash;
	size_t                               digest_len;
};

DSSH_PUBLIC int
dssh_hash_init(dssh_hash_ctx **ctx, const char *name,
    size_t *out_digest_len)
{
	if (ctx == NULL || name == NULL || out_digest_len == NULL)
		return DSSH_ERROR_INVALID;

	std::unique_ptr<Botan::HashFunction> h;

	try {
		h = Botan::HashFunction::create_or_throw(name);
	} catch (...) {
		return DSSH_ERROR_INIT;
	}

	size_t digest_len = h->output_length();

	dssh_hash_ctx *hctx = new (std::nothrow) dssh_hash_ctx;

	if (hctx == NULL)
		return DSSH_ERROR_ALLOC;

	hctx->digest_len = digest_len;
	hctx->hash = std::move(h);

	*ctx = hctx;
	*out_digest_len = digest_len;
	return 0;
}

DSSH_PUBLIC int
dssh_hash_update(dssh_hash_ctx *ctx, const uint8_t *data, size_t len)
{
	if (ctx == NULL)
		return DSSH_ERROR_INVALID;
	if (data == NULL && len > 0)
		return DSSH_ERROR_INVALID;
	try {
		ctx->hash->update(data, len);
	} catch (...) {
		return DSSH_ERROR_INIT;
	}
	return 0;
}

DSSH_PUBLIC int
dssh_hash_final(dssh_hash_ctx *ctx, uint8_t *out, size_t outlen)
{
	if (ctx == NULL || out == NULL)
		return DSSH_ERROR_INVALID;
	if (outlen < ctx->digest_len)
		return DSSH_ERROR_INVALID;

	try {
		/* final() resets the context for reuse. */
		ctx->hash->final(out);
	} catch (...) {
		return DSSH_ERROR_INIT;
	}

	return 0;
}

DSSH_PUBLIC void
dssh_hash_free(dssh_hash_ctx *ctx)
{
	delete ctx;
}

DSSH_PUBLIC int
dssh_hash_oneshot(const char *name, const uint8_t *data, size_t len,
    uint8_t *out, size_t outlen)
{
	if (name == NULL || out == NULL)
		return DSSH_ERROR_INVALID;
	if (data == NULL && len > 0)
		return DSSH_ERROR_INVALID;

	std::unique_ptr<Botan::HashFunction> h;

	try {
		h = Botan::HashFunction::create_or_throw(name);
	} catch (...) {
		return DSSH_ERROR_INIT;
	}

	if (outlen < h->output_length())
		return DSSH_ERROR_INVALID;

	try {
		if (data != NULL && len > 0)
			h->update(data, len);
		h->final(out);
	} catch (...) {
		return DSSH_ERROR_INIT;
	}
	return 0;
}

DSSH_PUBLIC void
dssh_cleanse(void *buf, size_t len)
{
	if (buf != NULL && len > 0)
		Botan::secure_scrub_memory(buf, len);
}

DSSH_PUBLIC int
dssh_random(uint8_t *buf, size_t len)
{
	if (buf == NULL)
		return DSSH_ERROR_INVALID;
	if (len == 0)
		return 0;
	try {
		Botan::system_rng().randomize(buf, len);
	} catch (...) {
		return DSSH_ERROR_INIT;
	}
	return 0;
}

DSSH_PUBLIC int
dssh_crypto_memcmp(const void *a, const void *b, size_t len)
{
	if (len > 0 && (a == NULL || b == NULL))
		return 1;
	return Botan::constant_time_compare(
	    static_cast<const uint8_t *>(a),
	    static_cast<const uint8_t *>(b), len) ? 0 : 1;
}

DSSH_PUBLIC int
dssh_base64_encode(const uint8_t *in, size_t len, char *out, size_t outlen)
{
	if (in == NULL || out == NULL)
		return DSSH_ERROR_INVALID;

	/* Guard against size_t overflow in base64 length calculation.
	 * On 32-bit, len near SIZE_MAX makes 4*ceil(len/3) wrap. */
	if (len > (SIZE_MAX - 3) / 4)
		return DSSH_ERROR_TOOLONG;

	size_t needed = Botan::base64_encode_max_output(len);

	if (needed > SIZE_MAX - 1)
		return DSSH_ERROR_TOOLONG;
	if (outlen < needed + 1)
		return DSSH_ERROR_TOOLONG;

	std::string encoded;

	try {
		encoded = Botan::base64_encode(in, len);
	} catch (...) {
		return DSSH_ERROR_INIT;
	}

	size_t count = encoded.size();

	if (count > INT_MAX)
		return DSSH_ERROR_TOOLONG;
	if (count > SIZE_MAX - 1)
		return DSSH_ERROR_TOOLONG;
	if (count + 1 > outlen)
		return DSSH_ERROR_TOOLONG;

	memcpy(out, encoded.c_str(), count + 1);
	return (int)count;
}
