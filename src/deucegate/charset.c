#include "deucegate.h"

#include <string.h>

#include "utf8_codepages.h"

static size_t
utf8_emit(uint32_t cp, uint8_t *out, size_t cap)
{
	if (cp <= 0x7f) {
		if (cap < 1) return 0;
		out[0] = (uint8_t)cp;
		return 1;
	}
	if (cp <= 0x7ff) {
		if (cap < 2) return 0;
		out[0] = 0xc0 | (uint8_t)(cp >> 6);
		out[1] = 0x80 | (uint8_t)(cp & 0x3f);
		return 2;
	}
	if (cp <= 0xffff) {
		if (cap < 3) return 0;
		out[0] = 0xe0 | (uint8_t)(cp >> 12);
		out[1] = 0x80 | (uint8_t)((cp >> 6) & 0x3f);
		out[2] = 0x80 | (uint8_t)(cp & 0x3f);
		return 3;
	}
	if (cap < 4) return 0;
	out[0] = 0xf0 | (uint8_t)(cp >> 18);
	out[1] = 0x80 | (uint8_t)((cp >> 12) & 0x3f);
	out[2] = 0x80 | (uint8_t)((cp >> 6) & 0x3f);
	out[3] = 0x80 | (uint8_t)(cp & 0x3f);
	return 4;
}

static int
utf8_one(const uint8_t *s, size_t len, uint32_t *cp)
{
	unsigned need;
	uint32_t val, min;
	if (len == 0) return 0;
	if (s[0] < 0x80) { *cp = s[0]; return 1; }
	if ((s[0] & 0xe0) == 0xc0) { need = 2; val = s[0] & 0x1f; min = 0x80; }
	else if ((s[0] & 0xf0) == 0xe0) { need = 3; val = s[0] & 0x0f; min = 0x800; }
	else if ((s[0] & 0xf8) == 0xf0) { need = 4; val = s[0] & 7; min = 0x10000; }
	else return -1;
	if (len < need) return 0;
	for (unsigned i = 1; i < need; i++) {
		if ((s[i] & 0xc0) != 0x80) return -1;
		val = (val << 6) | (s[i] & 0x3f);
	}
	if (val < min || val > 0x10ffff || (val >= 0xd800 && val <= 0xdfff)) return -1;
	*cp = val;
	return (int)need;
}

void
dg_decoder_init(dg_decoder_t *decoder, dg_encoding_t encoding)
{
	memset(decoder, 0, sizeof(*decoder));
	decoder->encoding = encoding;
}

size_t
dg_decode(dg_decoder_t *decoder, const uint8_t *in, size_t inlen,
    uint8_t *out, size_t outsz, bool flush)
{
	size_t used = 0, pos = 0;
	if (decoder->encoding == DG_CP437) {
		for (; pos < inlen; pos++) {
			size_t n = utf8_emit(cpoint_from_cpchar(CIOLIB_CP437, in[pos]), out + used, outsz - used);
			if (n == 0) break;
			used += n;
		}
		return used;
	}
	while (pos < inlen || decoder->pending_len > 0) {
		uint8_t seq[4];
		size_t have = decoder->pending_len;
		uint32_t cp;
		int n;
		memcpy(seq, decoder->pending, have);
		while (have < sizeof(seq) && pos < inlen)
			seq[have++] = in[pos++];
		n = utf8_one(seq, have, &cp);
		if (n == 0 && pos < inlen)
			continue;
		if (n == 0 && !flush) {
			memcpy(decoder->pending, seq, have);
			decoder->pending_len = have;
			break;
		}
		if (n < 0 || n == 0) {
			if (used == outsz) break;
			out[used++] = '?';
			decoder->invalid_count++;
			n = 1;
		}
		else {
			if (used + (size_t)n > outsz) break;
			memcpy(out + used, seq, (size_t)n);
			used += (size_t)n;
		}
		if ((size_t)n < have) {
			memmove(decoder->pending, seq + n, have - (size_t)n);
			decoder->pending_len = have - (size_t)n;
		}
		else
			decoder->pending_len = 0;
	}
	return used;
}
size_t
dg_encode(dg_encoding_t encoding, const uint8_t *utf8, size_t len,
    uint8_t *out, size_t outsz)
{
	size_t pos = 0, used = 0;
	if (encoding == DG_UTF8) {
		if (len > outsz) len = outsz;
		memcpy(out, utf8, len);
		return len;
	}
	while (pos < len && used < outsz) {
		uint32_t cp;
		int n = utf8_one(utf8 + pos, len - pos, &cp);
		if (n <= 0) {
			out[used++] = '?';
			pos++;
		}
		else {
			out[used++] = cpchar_from_unicode_cpoint(CIOLIB_CP437, cp, '?');
			pos += (size_t)n;
		}
	}
	return used;
}
