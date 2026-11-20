/* Private key-file status, extent and password handling. */
#ifndef DSSH_KEY_FILE_COMMON_H
#define DSSH_KEY_FILE_COMMON_H

#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
#include <string>
#include <string_view>
#endif
#include "deucessh-algorithms.h"
#include "deucessh-crypto.h"
#include "private-key-file.h"

/* Notify tests only while the cleared storage is still live. */
static inline void
dssh_key_file_clear(void *ptr, size_t length)
{
	dssh_cleanse(ptr, length);
#ifdef DSSH_TESTING
	dssh_test_key_file_cleared(ptr, length);
#endif
}

/* The stream is consumed once even if fclose reports an error. */
static inline int
dssh_key_file_close(FILE *stream, int status)
{
	if (fclose(stream) != 0)
		return DSSH_ERROR_INIT;
	return status;
}

/* One caller-owned I/O block is reused throughout an operation. The stream
 * borrows it until fclose completes, including a reported close/flush error.
 * Never copy an initialized stream or let its storage expire before close. */
struct dssh_key_file_stream {
	FILE *file;
	char buffer[BUFSIZ];
};

static inline int
dssh_key_file_stream_close(struct dssh_key_file_stream *stream, int status)
{
	int ret = dssh_key_file_close(stream->file, status);
	stream->file = NULL;
	dssh_key_file_clear(stream->buffer, sizeof(stream->buffer));
	return ret;
}

/* fp is open and has performed no stdio I/O. Failure consumes it once. */
static inline int
dssh_key_file_stream_init(struct dssh_key_file_stream *stream, FILE *fp)
{
	stream->file = fp;
	memset(stream->buffer, 0, sizeof(stream->buffer));
	if (setvbuf(fp, stream->buffer, _IOFBF, sizeof(stream->buffer)) != 0)
		return dssh_key_file_stream_close(stream, DSSH_ERROR_INIT);
	return 0;
}

/* Output belongs to this caller and remains unchanged on failure. The
 * explicit bound also permits arithmetic-model checks on the native host. */
static inline int
dssh_key_file_length(long value, size_t maximum, size_t *out)
{
	if (value <= 0)
		return DSSH_ERROR_INIT;
#if LONG_MAX > SIZE_MAX
	if (value > SIZE_MAX)
		return DSSH_ERROR_INVALID;
#endif
	size_t length = (size_t)value;
	if (length > maximum)
		return DSSH_ERROR_INVALID;
	*out = length;
	return 0;
}

struct dssh_key_file_password_state {
	dssh_pem_password_cb callback;
	void *data;
	int status;
};

/* Always provide this bridge to OpenSSL, including when the application has
 * no callback. Native defaults must not prompt or interpret opaque cbdata as
 * a password. Returned lengths are checked before reaching the provider. */
static inline int
dssh_key_file_password(char *buf, int size, int rwflag, void *opaque)
{
	if (opaque == NULL)
		return -1;
	struct dssh_key_file_password_state *state = (struct dssh_key_file_password_state *)opaque;
	if (buf == NULL || size < 1 || (rwflag != 0 && rwflag != 1)) {
		state->status = DSSH_ERROR_INVALID;
		return -1;
	}
#if INT_MAX > SIZE_MAX
	if (size > SIZE_MAX) {
		state->status = DSSH_ERROR_INVALID;
		return -1;
	}
#endif
	size_t capacity = (size_t)size;
	if (state->callback == NULL) {
		dssh_key_file_clear(buf, capacity);
		state->status = DSSH_ERROR_INIT;
		return -1;
	}
	memset(buf, 0, capacity);
	int length = state->callback(buf, size, rwflag, state->data);
	if (length < 0 || length > size) {
		dssh_key_file_clear(buf, capacity);
		state->status = length < 0 ? DSSH_ERROR_INIT : DSSH_ERROR_INVALID;
		return -1;
	}
	return length;
}

#ifdef __cplusplus
/* Every guard is constructed around live library-owned storage and retired
 * before its storage owner. Guards allocate nothing and cannot be copied. */
class dssh_key_file_secret_region {
	void *data;
	size_t length;
public:
	dssh_key_file_secret_region(void *bytes, size_t count) noexcept : data(bytes), length(count) {}
	dssh_key_file_secret_region(const dssh_key_file_secret_region &) = delete;
	dssh_key_file_secret_region &operator=(const dssh_key_file_secret_region &) = delete;
	~dssh_key_file_secret_region() noexcept
	{
		if (length != 0)
			dssh_key_file_clear(data, length);
	}
};

class dssh_key_file_buffer {
	uint8_t *data;
	size_t length;
public:
	dssh_key_file_buffer(uint8_t *bytes, size_t count) noexcept : data(bytes), length(count) {}
	dssh_key_file_buffer(const dssh_key_file_buffer &) = delete;
	dssh_key_file_buffer &operator=(const dssh_key_file_buffer &) = delete;
	~dssh_key_file_buffer() noexcept
	{
		dssh_key_file_clear(data, length);
		free(data);
	}
};

class dssh_key_file_secret_string {
public:
	std::string value;
	dssh_key_file_secret_string() = default;
	dssh_key_file_secret_string(const dssh_key_file_secret_string &) = delete;
	dssh_key_file_secret_string &operator=(const dssh_key_file_secret_string &) = delete;
	~dssh_key_file_secret_string() noexcept
	{
		if (!value.empty())
			dssh_key_file_clear(value.data(), value.size());
	}
};

struct dssh_key_file_password_error {};
#endif

#endif
