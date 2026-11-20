/*
 * crypto/botan.cpp -- Botan3 C++ implementation of deucessh-crypto.h.
 *
 * Uses Botan's native C++ API directly instead of the FFI layer.
 * All exported functions use C linkage (via the extern "C" in the headers).
 */

#include <botan/base64.h>
#include <botan/hash.h>
#include <botan/mem_ops.h>
#include <botan/system_rng.h>

#include <climits>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <new>
#include <string>

#include "deucessh-crypto.h"
#include "deucessh.h"
#ifdef DSSH_TESTING
 #include "test/dssh_test_botan_base64.h"
 #include "test/dssh_test_botan_hash.h"
#endif

struct dssh_hash_ctx {
	std::unique_ptr<Botan::HashFunction> hash;
	size_t                               digest_len;
};

static std::unique_ptr<Botan::HashFunction>
create_hash(const char *name)
{
#ifdef DSSH_TESTING
	return dssh_test_botan_hash_create(name);
#else
	return Botan::HashFunction::create_or_throw(name);
#endif
}

DSSH_PUBLIC int
dssh_hash_init(dssh_hash_ctx **ctx, const char *name, size_t *out_digest_len)
{
	if (ctx == NULL || name == NULL || out_digest_len == NULL)
		return DSSH_ERROR_INVALID;

	try {
		auto h = create_hash(name);
		size_t digest_len = h->output_length();
		if (digest_len == 0)
			return DSSH_ERROR_INIT;
		std::unique_ptr<dssh_hash_ctx> hctx(new (std::nothrow) dssh_hash_ctx);
		if (hctx == nullptr)
			return DSSH_ERROR_ALLOC;
		hctx->digest_len = digest_len;
		hctx->hash = std::move(h);
		*ctx = hctx.release();
		*out_digest_len = digest_len;
		return 0;
	}
	catch (const std::bad_alloc&) {
		return DSSH_ERROR_ALLOC;
	}
	catch (...) {
		return DSSH_ERROR_INIT;
	}
}

DSSH_PUBLIC int
dssh_hash_update(dssh_hash_ctx *ctx, const uint8_t *data, size_t len)
{
	if (ctx == NULL || ctx->hash == nullptr)
		return DSSH_ERROR_INVALID;
	if (data == NULL && len > 0)
		return DSSH_ERROR_INVALID;
	if (len == 0)
		return 0;
	try {
		ctx->hash->update(data, len);
	}
	catch (const std::bad_alloc&) {
		ctx->hash.reset();
		return DSSH_ERROR_ALLOC;
	}
	catch (...) {
		ctx->hash.reset();
		return DSSH_ERROR_INIT;
	}
	return 0;
}

DSSH_PUBLIC int
dssh_hash_final(dssh_hash_ctx *ctx, uint8_t *out, size_t outlen)
{
	if (ctx == NULL || ctx->hash == nullptr || out == NULL)
		return DSSH_ERROR_INVALID;
	if (outlen < ctx->digest_len)
		return DSSH_ERROR_INVALID;

	try {
		/* final() resets the context for reuse. */
		ctx->hash->final(out);
	}
	catch (const std::bad_alloc&) {
		ctx->hash.reset();
		return DSSH_ERROR_ALLOC;
	}
	catch (...) {
		ctx->hash.reset();
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
dssh_hash_oneshot(const char *name, const uint8_t *data, size_t len, uint8_t *out, size_t outlen)
{
	if (name == NULL || out == NULL)
		return DSSH_ERROR_INVALID;
	if (data == NULL && len > 0)
		return DSSH_ERROR_INVALID;

	try {
		auto h = create_hash(name);
		size_t digest_len = h->output_length();
		if (digest_len == 0)
			return DSSH_ERROR_INIT;
		if (outlen < digest_len)
			return DSSH_ERROR_INVALID;
		if (data != NULL && len > 0)
			h->update(data, len);
		h->final(out);
	}
	catch (const std::bad_alloc&) {
		return DSSH_ERROR_ALLOC;
	}
	catch (...) {
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
	}
	catch (...) {
		return DSSH_ERROR_INIT;
	}
	return 0;
}

DSSH_PUBLIC int
dssh_crypto_memcmp(const void *a, const void *b, size_t len)
{
	if (len == 0)
		return 0;
	if (a == NULL || b == NULL)
		return 1;
	return Botan::constant_time_compare(static_cast<const uint8_t *>(a), static_cast<const uint8_t *>(b), len)
	           ? 0
	           : 1;
}

DSSH_PUBLIC int
dssh_base64_encode(const uint8_t *in, size_t len, char *out, size_t outlen)
{
	if (in == NULL || out == NULL)
		return DSSH_ERROR_INVALID;

	/* Check the arithmetic and the API's int result before encoding. */
	if (len > SIZE_MAX - 2)
		return DSSH_ERROR_TOOLONG;
	size_t groups = (len + 2) / 3;
	if (groups > (SIZE_MAX - 1) / 4)
		return DSSH_ERROR_TOOLONG;
	size_t needed = 4 * groups;
	if (needed > INT_MAX)
		return DSSH_ERROR_TOOLONG;
	if (outlen < needed + 1)
		return DSSH_ERROR_TOOLONG;

	/* Preserve overlap without allocating: place the input in the output,
	 * then expand complete Base64 quanta backwards. Each write starts at
	 * or after the remaining input prefix. The native encoder sees only
	 * disjoint buffers and retains its native byte conversion. */
	std::memmove(out, in, len);
	uint8_t block[3 * 64];
	size_t remaining = len;
	size_t position = needed;
	int status = 0;
	try {
		while (remaining > 0) {
			size_t chunk = remaining % sizeof(block);
			if (chunk == 0)
				chunk = sizeof(block);
			remaining -= chunk;
			std::memcpy(block, out + remaining, chunk);
			/* All later chunks are multiples of three. Only this
			 * first (last-in-input) chunk can require padding. */
			size_t extent = 4 * ((chunk + 2) / 3);
			position -= extent;
			size_t consumed = 0;
			size_t count =
#ifdef DSSH_TESTING
			    dssh_test_botan_base64_encode(out + position, block,
#else
			    Botan::base64_encode(out + position, block,
#endif
			    chunk, consumed, true);
			if (count != extent || consumed != chunk) {
				status = DSSH_ERROR_INIT;
				break;
			}
		}
	}
	catch (...) {
		status = DSSH_ERROR_INIT;
	}
	Botan::secure_scrub_memory(block, sizeof(block));
	if (status < 0)
		return status;

	out[needed] = '\0';
	int result = static_cast<int>(needed);
	return result;
}
