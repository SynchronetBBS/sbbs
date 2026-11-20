/* Private, allocation-free SSH key/signature framing for builtin modules.
 * Native providers receive only fully bounded, validated wire values.
 * Output/cursor pointers below are library-owned non-NULL locals. */
#ifndef DSSH_KEY_WIRE_H
#define DSSH_KEY_WIRE_H

#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "deucessh.h"
#include "deucessh-result.h"

struct dssh_rsa_wire_key {
	struct dssh_bytes exponent;
	struct dssh_bytes modulus;
};

static inline int
dssh_key_wire_field(struct dssh_bytes *remaining, struct dssh_bytes *out)
{
	if (remaining->len < 4)
		return DSSH_ERROR_PARSE;
	const uint8_t *p = remaining->data;
	uint32_t wire = ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16)
	    | ((uint32_t)p[2] << 8) | p[3];
	if (wire > remaining->len - 4)
		return DSSH_ERROR_PARSE;
	size_t length = (size_t)wire;
	struct dssh_bytes value = {p + 4, length};
	struct dssh_bytes tail = {p + 4 + length, remaining->len - 4 - length};
	*out = value;
	*remaining = tail;
	return 0;
}

static inline int
dssh_key_wire_name(struct dssh_bytes *remaining, const char *expected, size_t length)
{
	struct dssh_bytes name;
	int ret = dssh_key_wire_field(remaining, &name);
	if (ret < 0)
		return ret;
	if (name.len != length || memcmp(name.data, expected, length) != 0)
		return DSSH_ERROR_INVALID;
	return 0;
}

/* A complete named binary capsule. Contents may need algorithm-specific checks. */
static inline int
dssh_key_wire_blob(const uint8_t *data, size_t length, const char *name,
    size_t name_length, struct dssh_bytes *out)
{
	if (data == NULL)
		return DSSH_ERROR_INVALID;
	struct dssh_bytes remaining = {data, length};
	struct dssh_bytes value;
	int ret = dssh_key_wire_name(&remaining, name, name_length);
	if (ret < 0)
		return ret;
	ret = dssh_key_wire_field(&remaining, &value);
	if (ret < 0)
		return ret;
	if (remaining.len != 0)
		return DSSH_ERROR_PARSE;
	*out = value;
	return 0;
}

static inline int
dssh_key_wire_ed25519(const uint8_t *data, size_t length, struct dssh_bytes *out)
{
	struct dssh_bytes value;
	int ret = dssh_key_wire_blob(data, length, "ssh-ed25519", 11, &value);
	if (ret < 0)
		return ret;
	if (value.len != 32)
		return DSSH_ERROR_INVALID;
	*out = value;
	return 0;
}

/* RSA integers must be nonzero, positive, minimally encoded SSH mpints.
 * Borrow the magnitude after removing only a necessary sign-padding byte. */
static inline int
dssh_key_wire_positive(struct dssh_bytes encoded, struct dssh_bytes *out)
{
	if (encoded.len == 0 || encoded.data[0] >= 128)
		return DSSH_ERROR_INVALID;
	if (encoded.data[0] == 0) {
		if (encoded.len == 1 || encoded.data[1] < 128)
			return DSSH_ERROR_INVALID;
		encoded.data++;
		encoded.len--;
	}
	*out = encoded;
	return 0;
}

static inline int
dssh_key_wire_rsa(const uint8_t *data, size_t length, struct dssh_rsa_wire_key *out)
{
	if (data == NULL)
		return DSSH_ERROR_INVALID;
	struct dssh_bytes remaining = {data, length};
	struct dssh_bytes e, n;
	int ret = dssh_key_wire_name(&remaining, "ssh-rsa", 7);
	if (ret < 0)
		return ret;
	ret = dssh_key_wire_field(&remaining, &e);
	if (ret < 0)
		return ret;
	ret = dssh_key_wire_field(&remaining, &n);
	if (ret < 0)
		return ret;
	if (remaining.len != 0)
		return DSSH_ERROR_PARSE;
	ret = dssh_key_wire_positive(e, &e);
	if (ret < 0)
		return ret;
	ret = dssh_key_wire_positive(n, &n);
	if (ret < 0)
		return ret;
	struct dssh_rsa_wire_key key = {e, n};
	*out = key;
	return 0;
}

/* Magnitude is canonical and positive. A result not representable within the
 * caller's bound is unknown (zero), never a wrapped or truncated bit count. */
static inline unsigned int
dssh_key_wire_bits(struct dssh_bytes magnitude, unsigned int maximum)
{
	if (magnitude.data == NULL || magnitude.len == 0 || magnitude.data[0] == 0)
		return 0;
	uint8_t first = magnitude.data[0];
	unsigned int high = 0;
	while (first != 0) {
		high++;
		first >>= 1;
	}
	if (high > maximum || magnitude.len - 1 > (maximum - high) / 8)
		return 0;
	unsigned int lower_bytes = (unsigned int)(magnitude.len - 1);
	return lower_bytes * 8 + high;
}

#endif
