/* Private checked RSA public-cache layout and native magnitude information. */
#ifndef DSSH_KEY_RSA_CACHE_H
#define DSSH_KEY_RSA_CACHE_H

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "deucessh.h"
#if (defined(DSSH_RSA_CACHE_OPENSSL) || defined(DSSH_CRYPTO_OPENSSL))
#include <openssl/bn.h>
#elif (defined(DSSH_RSA_CACHE_BOTAN) || defined(DSSH_CRYPTO_BOTAN)) && defined(__cplusplus)
#include <botan/bigint.h>
#endif

#ifdef DSSH_TESTING
#ifdef __cplusplus
extern "C" {
#endif
void *dssh_test_malloc(size_t size);
void dssh_test_free(void *ptr);
#ifdef __cplusplus
}
#endif
#endif

struct dssh_rsa_cache_number {
	size_t bytes;
	bool pad;
};

struct dssh_rsa_cache_layout {
	struct dssh_rsa_cache_number exponent;
	struct dssh_rsa_cache_number modulus;
	uint32_t exponent_wire;
	uint32_t modulus_wire;
	size_t length;
};

/* Explicit output bounds permit numeric models without fabricated objects. */
static inline int
dssh_rsa_cache_number_size(uint64_t bits, size_t limit, struct dssh_rsa_cache_number *out)
{
	if (bits == 0)
		return DSSH_ERROR_INVALID;
	uint64_t bytes = bits / 8 + (bits % 8 != 0 ? 1U : 0U);
	if (bytes > limit)
		return DSSH_ERROR_INVALID;
	size_t length = (size_t)bytes;
	struct dssh_rsa_cache_number result = {length, bits % 8 == 0};
	*out = result;
	return 0;
}

#if (defined(DSSH_RSA_CACHE_OPENSSL) || defined(DSSH_CRYPTO_OPENSSL))
static inline int
dssh_rsa_cache_native_number(const BIGNUM *number, struct dssh_rsa_cache_number *out)
{
	if (number == NULL || BN_is_negative(number))
		return DSSH_ERROR_INVALID;
	int bits = BN_num_bits(number);
	if (bits <= 0)
		return DSSH_ERROR_INVALID;
#if INT_MAX > UINT64_MAX
	if (bits > UINT64_MAX)
		return DSSH_ERROR_INVALID;
#endif
	uint64_t count = (uint64_t)bits;
	return dssh_rsa_cache_number_size(count, SIZE_MAX, out);
}
#elif (defined(DSSH_RSA_CACHE_BOTAN) || defined(DSSH_CRYPTO_BOTAN)) && defined(__cplusplus)
static inline int
dssh_rsa_cache_native_number(const Botan::BigInt &number, struct dssh_rsa_cache_number *out)
{
	if (number.signum() <= 0)
		return DSSH_ERROR_INVALID;
	size_t bits = number.bits();
#if SIZE_MAX > UINT64_MAX
	if (bits > UINT64_MAX)
		return DSSH_ERROR_INVALID;
#endif
	uint64_t count = (uint64_t)bits;
	return dssh_rsa_cache_number_size(count, SIZE_MAX, out);
}
#endif

static inline int
dssh_rsa_cache_size(struct dssh_rsa_cache_number exponent,
    struct dssh_rsa_cache_number modulus, size_t buffer_limit,
    uint32_t field_limit, struct dssh_rsa_cache_layout *out)
{
	if (exponent.bytes == 0 || modulus.bytes == 0 || buffer_limit < 19)
		return DSSH_ERROR_INVALID;
	size_t epad = exponent.pad ? 1U : 0U;
	size_t npad = modulus.pad ? 1U : 0U;
	if (exponent.bytes > SIZE_MAX - epad || modulus.bytes > SIZE_MAX - npad)
		return DSSH_ERROR_INVALID;
	size_t ewire = (size_t)(exponent.bytes + epad);
	size_t nwire = (size_t)(modulus.bytes + npad);
	if (ewire > field_limit || nwire > field_limit
	    || ewire > buffer_limit - 19 || nwire > buffer_limit - 19 - ewire)
		return DSSH_ERROR_INVALID;
	uint32_t efield = (uint32_t)ewire;
	uint32_t nfield = (uint32_t)nwire;
	size_t length = (size_t)(19 + ewire + nwire);
	struct dssh_rsa_cache_layout layout = {exponent, modulus, efield, nfield, length};
	*out = layout;
	return 0;
}

/* The complete key is itself carried in an SSH string. */
static inline int
dssh_rsa_cache_size_native(struct dssh_rsa_cache_number exponent,
    struct dssh_rsa_cache_number modulus, struct dssh_rsa_cache_layout *out)
{
	size_t limit = SIZE_MAX;
#if SIZE_MAX > UINT32_MAX
	limit = UINT32_MAX;
#endif
	return dssh_rsa_cache_size(exponent, modulus, limit, UINT32_MAX, out);
}

/* buf is owned storage of layout.length bytes; layout was checked above.
 * The returned payload slots are never published until native export succeeds. */
static inline int
dssh_rsa_cache_prefix(uint8_t *buf, const struct dssh_rsa_cache_layout *layout,
    uint8_t **exponent, uint8_t **modulus)
{
	size_t pos = 0;
	int ret = dssh_serialize_uint32(7, buf, layout->length, &pos);
	if (ret < 0)
		return ret;
	memcpy(buf + pos, "ssh-rsa", 7);
	pos += 7;
	ret = dssh_serialize_uint32(layout->exponent_wire, buf, layout->length, &pos);
	if (ret < 0)
		return ret;
	if (layout->exponent.pad)
		buf[pos++] = 0;
	uint8_t *epos = buf + pos;
	pos += layout->exponent.bytes;
	ret = dssh_serialize_uint32(layout->modulus_wire, buf, layout->length, &pos);
	if (ret < 0)
		return ret;
	if (layout->modulus.pad)
		buf[pos++] = 0;
	*exponent = epos;
	*modulus = buf + pos;
	return 0;
}

static inline uint8_t *
dssh_rsa_cache_allocate(size_t length)
{
#ifdef DSSH_TESTING
	uint8_t *buf = (uint8_t *)dssh_test_malloc(length);
#else
	uint8_t *buf = (uint8_t *)malloc(length);
#endif
	return buf;
}

static inline void
dssh_rsa_cache_release(void *buf)
{
	if (buf == NULL)
		return;
#ifdef DSSH_TESTING
	dssh_test_free(buf);
#else
	free(buf);
#endif
}

#ifdef __cplusplus
struct dssh_rsa_cache_deleter {
	void operator()(uint8_t *buf) const noexcept
	{
		dssh_rsa_cache_release(buf);
	}
};
#endif
#endif
