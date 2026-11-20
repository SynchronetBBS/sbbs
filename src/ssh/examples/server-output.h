#ifndef DSSH_EXAMPLE_SERVER_OUTPUT_H
#define DSSH_EXAMPLE_SERVER_OUTPUT_H

/* Private application display/formatting helpers, never installed with the SDK.
 * Wire data remains unchanged; caller-owned display buffers may elide a suffix. */
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "deucessh.h"

#if defined(__GNUC__) || defined(__clang__)
#define DSSH_EXAMPLE_PRINTF_LIKE(format_index, arguments_index) __attribute__((format(printf, format_index, arguments_index)))
#else
#define DSSH_EXAMPLE_PRINTF_LIKE(format_index, arguments_index)
#endif

static inline size_t
example_encode_byte(uint8_t byte, char output[4])
{
	if (byte >= 32 && byte <= 126 && byte != '\\' && byte != '\'' && byte != '"') {
		char plain = (char)byte;
		output[0] = plain;
		return 1;
	}
	static const char hex[] = "0123456789abcdef";
	output[0] = '\\';
	output[1] = 'x';
	output[2] = hex[byte >> 4];
	output[3] = hex[byte & 15];
	return 4;
}

/* A bounded display is intentional, rather than a truncated protocol value.
 * Keep escapes whole and reserve the visible elision marker while more input
 * remains. Source/destination are distinct application/packet storage. */
static inline int
example_display_text(char *output, size_t capacity, const uint8_t *input, size_t length)
{
	if (output == NULL || capacity < 4 || (length > 0 && input == NULL))
		return DSSH_ERROR_INVALID;
	size_t measured = 0;
	bool elide = false;
	for (size_t i = 0; i < length; i++) {
		char encoded[4];
		size_t count = example_encode_byte(input[i], encoded);
		if (count > capacity - 1 - measured) {
			elide = true;
			break;
		}
		measured += count;
	}
	size_t budget = capacity - 1 - (elide ? 3 : 0);
	size_t used = 0;
	for (size_t i = 0; i < length; i++) {
		char encoded[4];
		size_t count = example_encode_byte(input[i], encoded);
		if (count > budget - used) {
			memcpy(&output[used], "...", 3);
			used += 3;
			break;
		}
		memcpy(&output[used], encoded, count);
		used += count;
	}
	output[used] = 0;
	return 0;
}

static inline int
example_write_text(FILE *stream, const uint8_t *input, size_t length)
{
	if (stream == NULL || (length > 0 && input == NULL))
		return DSSH_ERROR_INVALID;
	for (size_t i = 0; i < length; i++) {
		char encoded[4];
		size_t count = example_encode_byte(input[i], encoded);
		if (fwrite(encoded, 1, count, stream) != count)
			return DSSH_ERROR_INIT;
	}
	return 0;
}

static inline int example_format(char *, size_t, const char *, ...) DSSH_EXAMPLE_PRINTF_LIKE(3, 4);

static inline int
example_format(char *output, size_t capacity, const char *format, ...)
{
	if (output == NULL || capacity == 0 || format == NULL)
		return DSSH_ERROR_INVALID;
	va_list arguments;
	va_start(arguments, format);
	int result = vsnprintf(output, capacity, format, arguments);
	va_end(arguments);
	if (result < 0)
		return DSSH_ERROR_INIT;
#if SIZE_MAX < INT_MAX
	if (result > SIZE_MAX)
		return DSSH_ERROR_TOOLONG;
#endif
	size_t written = (size_t)result;
	return written < capacity ? 0 : DSSH_ERROR_TOOLONG;
}

#endif
