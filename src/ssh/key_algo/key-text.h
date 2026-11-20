/* Private checked public-key text formatting and bounded file streaming. */
#ifndef DSSH_KEY_TEXT_H
#define DSSH_KEY_TEXT_H

#include <limits.h>
#include <stdint.h>
#include <string.h>
#include "deucessh-crypto.h"
#include "key-file-common.h"
#if defined(DSSH_CRYPTO_BOTAN) && defined(__cplusplus)
#include <botan/base64.h>
#endif

/* The explicit bounds permit independent arithmetic models. No buffer is
 * touched here, and the caller's output remains unchanged on rejection. */
static inline int
dssh_key_text_extent(size_t bytes, size_t name_length, size_t total_limit,
    size_t encoded_limit, size_t *out)
{
	if (name_length > total_limit || total_limit - name_length < 2)
		return DSSH_ERROR_TOOLONG;
	size_t groups = bytes / 3;
	/* bytes / 3 + 1 is representable for every size_t value. */
	if (bytes % 3 != 0)
		groups++;
	size_t available = total_limit - name_length - 2;
	if (groups > available / 4 || groups > encoded_limit / 4)
		return DSSH_ERROR_TOOLONG;
	size_t encoded = groups * 4;
	*out = name_length + 2 + encoded;
	return 0;
}

static inline int
dssh_key_text_size(size_t bytes, size_t name_length, size_t *out)
{
	size_t total_limit = SIZE_MAX;
	size_t encoded_limit = SIZE_MAX;
#if SIZE_MAX > INT64_MAX
	total_limit = INT64_MAX;
#endif
#if SIZE_MAX > INT_MAX
	encoded_limit = INT_MAX;
#endif
	return dssh_key_text_extent(bytes, name_length, total_limit, encoded_limit, out);
}

/* These module-owned input and caller output spans are disjoint. Keep the
 * general public crypto API's separate alias/lifetime policy unchanged. */
static inline int
dssh_key_text_encode(const uint8_t *input, size_t length, char *out, size_t capacity)
{
#if defined(DSSH_CRYPTO_BOTAN) && defined(__cplusplus)
	size_t total;
	int ret = dssh_key_text_size(length, 0, &total);
	if (ret < 0)
		return ret;
	size_t expected = total - 2;
	if (capacity <= expected)
		return DSSH_ERROR_TOOLONG;
	try {
		size_t consumed = 0;
		size_t produced = Botan::base64_encode(out, input, length, consumed, true);
		if (consumed != length || produced != expected)
			return DSSH_ERROR_INIT;
		out[produced] = 0;
		int result = (int)produced;
		return result;
	}
	catch (...) {
		return DSSH_ERROR_INIT;
	}
#else
	return dssh_base64_encode(input, length, out, capacity);
#endif
}

/* name and blob are borrowed from the module's owned immutable key cache.
 * A successful count includes the NUL, as required by the existing API. */
static inline int64_t
dssh_key_text_format(const uint8_t *blob, size_t bytes, const char *name,
    size_t name_length, char *buf, size_t capacity)
{
	if (blob == NULL || name == NULL)
		return DSSH_ERROR_INVALID;
	size_t needed;
	int ret = dssh_key_text_size(bytes, name_length, &needed);
	if (ret < 0)
		return ret;
	int64_t result = (int64_t)needed;
	if (buf == NULL || capacity == 0)
		return result;
	if (capacity < needed)
		return DSSH_ERROR_TOOLONG;
	ret = dssh_key_text_encode(blob, bytes, buf + name_length + 1,
	    needed - name_length - 1);
	if (ret < 0)
		return ret;
#if INT_MAX > SIZE_MAX
	if (ret > SIZE_MAX)
		return DSSH_ERROR_INIT;
#endif
	size_t encoded = (size_t)ret;
	if (encoded != needed - name_length - 2)
		return DSSH_ERROR_INIT;
	memcpy(buf, name, name_length);
	buf[name_length] = ' ';
	return result;
}

/* A chunk is a multiple of three input bytes, so concatenation adds padding
 * only at the end. This is bounded streaming scratch, never a key-size limit.
 * No allocation or complete temporary public string is needed. */
static inline int
dssh_key_text_write(const char *path, const uint8_t *blob, size_t bytes,
    const char *name, size_t name_length)
{
	if (blob == NULL || name == NULL)
		return DSSH_ERROR_INVALID;
	size_t needed;
	int ret = dssh_key_text_size(bytes, name_length, &needed);
	if (ret < 0)
		return ret;
	FILE *fp = dssh_key_file_open_public(path);
	if (fp == NULL)
		return DSSH_ERROR_INIT;
	if (fwrite(name, 1, name_length, fp) != name_length || fputc(' ', fp) == EOF)
		return dssh_key_file_close(fp, DSSH_ERROR_INIT);
	char encoded[1025];
	size_t remaining = bytes;
	const uint8_t *input = blob;
	while (remaining != 0) {
		size_t count = remaining < 768 ? remaining : 768;
		ret = dssh_key_text_encode(input, count, encoded, sizeof(encoded));
		if (ret < 0)
			return dssh_key_file_close(fp, ret);
		/* Every native report is checked before a file write. */
		size_t expected = 4 * ((count + 2) / 3);
#if INT_MAX > SIZE_MAX
		if (ret > SIZE_MAX)
			return dssh_key_file_close(fp, DSSH_ERROR_INIT);
#endif
		size_t produced = (size_t)ret;
		if (produced != expected)
			return dssh_key_file_close(fp, DSSH_ERROR_INIT);
		if (fwrite(encoded, 1, produced, fp) != produced)
			return dssh_key_file_close(fp, DSSH_ERROR_INIT);
		input += count;
		remaining -= count;
	}
	ret = fputc('\n', fp) == EOF ? DSSH_ERROR_INIT : 0;
	return dssh_key_file_close(fp, ret);
}

#endif
