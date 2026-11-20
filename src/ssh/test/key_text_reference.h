#ifndef DSSH_TEST_KEY_TEXT_REFERENCE_H
#define DSSH_TEST_KEY_TEXT_REFERENCE_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

/* Independent bit-stream oracle, not the production three-byte encoder. */
static bool
key_text_reference(const uint8_t *input, size_t length, const char *prefix,
    char *out, size_t capacity, size_t *written)
{
	const char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	size_t pos = strlen(prefix);
	if (pos >= capacity)
		return false;
	memcpy(out, prefix, pos);
	out[pos++] = ' ';
	unsigned int bits = 0, count = 0;
	for (size_t i = 0; i < length; i++) {
		bits = (bits << 8) | input[i];
		count += 8;
		while (count >= 6) {
			count -= 6;
			if (pos >= capacity)
				return false;
			out[pos++] = alphabet[(bits >> count) & 63U];
		}
		bits &= (1U << count) - 1;
	}
	if (count != 0) {
		if (pos >= capacity)
			return false;
		out[pos++] = alphabet[(bits << (6 - count)) & 63U];
	}
	while ((pos - strlen(prefix) - 1) % 4 != 0) {
		if (pos >= capacity)
			return false;
		out[pos++] = '=';
	}
	if (pos >= capacity)
		return false;
	out[pos++] = 0;
	*written = pos;
	return true;
}

#endif
