#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "base64.h"

static const char *const base64alphabet =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=";

// TODO: No padding... we're using fixed buffers and don't need it

bool
b64_decode(uint8_t *target, size_t tlen, const char *source)
{
	unsigned working = 0;
	int bits = 0;

	size_t slen = strlen(source);
	uint8_t *outp = target;
	const char *inp = source;
	uint8_t *outend = target + tlen;
	const char *inend = source + slen;
	for (; outp < outend && inp < inend; inp++) {
		working <<= 6;
		char *i = strchr(base64alphabet, (char)*inp);
		if (i == NULL)
			return false;
		if (*i == '=')  { /* pad char */
			if ((working & 0xFF) != 0)
				return false;
			break;
		}
		bits += 6;
		working |= (i - base64alphabet);
		if (bits >= 8) {
			*(outp++) = (uint8_t)((working & (0xFFU << (bits - 8))) >> (bits - 8));
			bits -= 8;
		}
	}
	if (outp != outend)
		return false;
	return true;
}

static bool
add_char(char *pos, uint8_t ch, bool done, char *end)
{
	if (pos >= end)
		return false;
	if (done)
		*pos = base64alphabet[64];
	else
		*pos = base64alphabet[(int)ch];
	return true;
}

char *
b64_encode(const uint8_t *source, size_t slen)
{
	const uint8_t *inp = source;
	size_t tlen = (((4ULL * slen / 3ULL) + 3ULL) & ~3ULL) + 1;
	char *target = malloc(tlen);
	char *outp = target;
	char *outend = outp + tlen;
	const uint8_t *inend = inp + slen;
	unsigned buf;
	for (bool done = false; (inp < inend) && !done;)  {
		uint8_t enc = *(inp++);
		buf = (enc & 0x03U) << 4;
		enc = (enc & 0xFCU) >> 2;
		add_char(outp++, enc, done, outend);
		if (inp >= inend)
			enc = (uint8_t)buf;
		else
			enc = (uint8_t)(buf | ((*inp & 0xF0U) >> 4));
		add_char(outp++, enc, done, outend);
		if (inp == inend)
			done = true;
		if (!done) {
			buf = (((unsigned)*(inp++) << 2) & 0x3CU);
			if (inp == inend)
				enc = (uint8_t)buf;
			else
				enc = (uint8_t)(buf | ((*inp & 0xC0U) >> 6));
		}
		add_char(outp++, enc, done, outend);
		if (inp == inend)
			done = true;
		if (!done)
			enc = *(inp++) & 0x3FU;
		add_char(outp++, enc, done, outend);
		if (inp == inend)
			done = true;
	}
	if (outp == outend) {
		free(target);
		return NULL;
	}
	*(outp++) = 0;
	if (outp != outend) {
		free(target);
		return NULL;
	}
	return target;
}
