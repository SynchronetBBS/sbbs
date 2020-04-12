#include <inttypes.h>
#include <stdlib.h>
#include "utf8_codepages.h"

// Sorted by unicode codepoint...
struct cp437map cp437_table[160] = {
	{0x0000, 0},   {0x00A0, 255}, {0x00A1, 173}, {0x00A2, 155},
	{0x00A3, 156}, {0x00A5, 157}, {0x00A7, 21},  {0x00AA, 166},
	{0x00AB, 174}, {0x00AC, 170}, {0x00B0, 248}, {0x00B1, 241},
	{0x00B2, 253}, {0x00B5, 230}, {0x00B6, 20},  {0x00B7, 250},
	{0x00BA, 167}, {0x00BB, 175}, {0x00BC, 172}, {0x00BD, 171},
	{0x00BF, 168}, {0x00C4, 142}, {0x00C5, 143}, {0x00C6, 146},
	{0x00C7, 128}, {0x00C9, 144}, {0x00D1, 165}, {0x00D6, 153},
	{0x00DC, 154}, {0x00DF, 225}, {0x00E0, 133}, {0x00E1, 160},
	{0x00E2, 131}, {0x00E4, 132}, {0x00E5, 134}, {0x00E6, 145},
	{0x00E7, 135}, {0x00E8, 138}, {0x00E9, 130}, {0x00EA, 136},
	{0x00EB, 137}, {0x00EC, 141}, {0x00ED, 161}, {0x00EE, 140}, 
	{0x00EF, 139}, {0x00F1, 164}, {0x00F2, 149}, {0x00F3, 162},
	{0x00F4, 147}, {0x00F6, 148}, {0x00F7, 246}, {0x00F9, 151},
	{0x00FA, 163}, {0x00FB, 150}, {0x00FC, 129}, {0x00FF, 152},
	{0x0192, 159}, {0x0393, 226}, {0x0398, 233}, {0x03A3, 228},
	{0x03A6, 232}, {0x03A9, 234}, {0x03B1, 224}, {0x03B4, 235},
	{0x03B5, 238}, {0x03C0, 227}, {0x03C3, 229}, {0x03C4, 231},
	{0x03C6, 237}, {0x2022, 7},   {0x203C, 19},  {0x207F, 252},
	{0x20A7, 158}, {0x2190, 27},  {0x2191, 24},  {0x2192, 26},
	{0x2193, 25},  {0x2194, 29},  {0x2195, 18},  {0x21A8, 23},
	{0x2219, 249}, {0x221A, 251}, {0x221E, 236}, {0x221F, 28},
	{0x2229, 239}, {0x2248, 247}, {0x2261, 240}, {0x2264, 243},
	{0x2265, 242}, {0x2310, 169}, {0x2320, 244}, {0x2321, 245},
	{0x2500, 196}, {0x2502, 179}, {0x250C, 218}, {0x2510, 191},
	{0x2514, 192}, {0x2518, 217}, {0x251C, 195}, {0x2524, 180},
	{0x252C, 194}, {0x2534, 193}, {0x253C, 197}, {0x2550, 205},
	{0x2551, 186}, {0x2552, 213}, {0x2553, 214}, {0x2554, 201},
	{0x2555, 184}, {0x2556, 183}, {0x2557, 187}, {0x2558, 212},
	{0x2559, 211}, {0x255A, 200}, {0x255B, 190}, {0x255C, 189},
	{0x255D, 188}, {0x255E, 198}, {0x255F, 199}, {0x2560, 204},
	{0x2561, 181}, {0x2562, 182}, {0x2563, 185}, {0x2564, 209},
	{0x2565, 210}, {0x2566, 203}, {0x2567, 207}, {0x2568, 208},
	{0x2569, 202}, {0x256A, 216}, {0x256B, 215}, {0x256C, 206},
	{0x2580, 223}, {0x2584, 220}, {0x2588, 219}, {0x258C, 221},
	{0x2590, 222}, {0x2591, 176}, {0x2592, 177}, {0x2593, 178},
	{0x25A0, 254}, {0x25AC, 22},  {0x25B2, 30},  {0x25BA, 16},
	{0x25BC, 31},  {0x25C4, 17},  {0x25CB, 9},   {0x25D8, 8},
	{0x25D9, 10},  {0x263A, 1},   {0x263B, 2},   {0x263C, 15},
	{0x2640, 12},  {0x2642, 11},  {0x2660, 6},   {0x2663, 5}, 
	{0x2665, 3},   {0x2666, 4},   {0x266A, 13},  {0x266B, 14},
};

uint32_t cp437_ext_table[32] = {
	0x0000, 0x263A, 0x263B, 0x2665, 0x2666, 0x2663, 0x2660, 0x2022,
	0x25D8, 0x25CB, 0x25D9, 0x2642, 0x2640, 0x266A, 0x266B, 0x263C,
	0x25BA, 0x25C4, 0x2195, 0x203C, 0x00B6, 0x00A7, 0x25AC, 0x21A8,
	0x2191, 0x2193, 0x2192, 0x2190, 0x221F, 0x2194, 0x25B2, 0x25BC
};

uint32_t cp437_unicode_table[128] = {
	0x00C7, 0x00FC, 0x00E9, 0x00E2, 0x00E4, 0x00E0, 0x00E5, 0x00E7, 
	0x00EA, 0x00EB, 0x00E8, 0x00EF, 0x00EE, 0x00EC, 0x00C4, 0x00C5, 
	0x00C9, 0x00E6, 0x00C6, 0x00F4, 0x00F6, 0x00F2, 0x00FB, 0x00F9, 
	0x00FF, 0x00D6, 0x00DC, 0x00A2, 0x00A3, 0x00A5, 0x20A7, 0x0192, 
	0x00E1, 0x00ED, 0x00F3, 0x00FA, 0x00F1, 0x00D1, 0x00AA, 0x00BA, 
	0x00BF, 0x2310, 0x00AC, 0x00BD, 0x00BC, 0x00A1, 0x00AB, 0x00BB, 
	0x2591, 0x2592, 0x2593, 0x2502, 0x2524, 0x2561, 0x2562, 0x2556, 
	0x2555, 0x2563, 0x2551, 0x2557, 0x255D, 0x255C, 0x255B, 0x2510, 
	0x2514, 0x2534, 0x252C, 0x251C, 0x2500, 0x253C, 0x255E, 0x255F, 
	0x255A, 0x2554, 0x2569, 0x2566, 0x2560, 0x2550, 0x256C, 0x2567, 
	0x2568, 0x2564, 0x2565, 0x2559, 0x2558, 0x2552, 0x2553, 0x256B, 
	0x256A, 0x2518, 0x250C, 0x2588, 0x2584, 0x258C, 0x2590, 0x2580, 
	0x03B1, 0x00DF, 0x0393, 0x03C0, 0x03A3, 0x03C3, 0x00B5, 0x03C4, 
	0x03A6, 0x0398, 0x03A9, 0x03B4, 0x221E, 0x03C6, 0x03B5, 0x2229, 
	0x2261, 0x00B1, 0x2265, 0x2264, 0x2320, 0x2321, 0x00F7, 0x2248, 
	0x00B0, 0x2219, 0x00B7, 0x221A, 0x207F, 0x00B2, 0x25A0, 0x00A0
};

static int
cmptab(const void *key, const void *entry)
{
	const uint32_t *pkey = key;
	const struct cp437map *pentry = entry;

	if (*pkey == pentry->unicode)
		return 0;
	if (*pkey < pentry->unicode)
		return -1;
	return 1;
}

uint8_t
cp437_from_unicode_cp(uint32_t cp, char unmapped)
{
	struct cp437map *mapped;

	if (cp < 128)
		return cp;
	mapped = bsearch(&cp, cp437_table, 
	    sizeof(cp437_table) / sizeof(cp437_table[0]),
	    sizeof(cp437_table[0]), cmptab);
	if (mapped == NULL)
		return unmapped;
	return mapped->cp437;
}

uint8_t
cp437_from_unicode_cp_ext(uint32_t cp, char unmapped)
{
	if (cp < 32) {
		return cp437_ext_table[cp];
	}
	return cp437_from_unicode_cp(cp, unmapped);
}

static int
write_cp(uint8_t *str, uint32_t cp)
{
	if (cp < 128) {
		*str = cp;
		return 1;
	}
	if (cp < 0x800) {
		*(str++) = 0xc0 | (cp >> 6);
		*(str++) = 0x80 | (cp & 0x3f);
		return 2;
	}
	if (cp < 0x10000) {
		*(str++) = 0xe0 | (cp >> 12);
		*(str++) = 0x80 | ((cp >> 6) & 0x3f);
		*(str++) = 0x80 | (cp & 0x3f);
		return 3;
	}
	if (cp < 0x110000) {
		*(str++) = 0xf0 | (cp >> 18);
		*(str++) = 0x80 | ((cp >> 12) & 0x3f);
		*(str++) = 0x80 | ((cp >> 6) & 0x3f);
		*(str++) = 0x80 | (cp & 0x3f);
		return 4;
	}
	return -1;
}

static int
read_cp(const uint8_t *str, uint32_t *cp)
{
	int incode;
	const uint8_t *p = str;

	if (cp == NULL)
		goto error;

	if (str == NULL)
		goto error;

	if (*p & 0x80) {
		if ((*p & 0xe0) == 0xc0) {
			incode = 1;
			*cp = *p & 0x1f;
		}
		else if ((*p & 0xf0) == 0xe0) {
			incode = 2;
			*cp = *p & 0x0f;
		}
		else if ((*p & 0xf8) == 0xf0) {
			incode = 3;
			*cp = *p & 0x07;
		}
		else
			goto error;

		while (incode) {
			p++;
			incode--;
			if ((*p & 0xc0) != 0x80)
				goto error;
			*cp <<= 6;
			*cp |= (*p & 0x3f);
		}
	}
	else {
		*cp = *p;
	}
	return p - str + 1;

error:
	if (cp)
		*cp = 0xffff;
	return -1;
}

static int
utf8_bytes(uint32_t cp)
{
	if (cp < 0x80)
		return 1;
	if (cp < 0x800)
		return 2;
	if (cp < 0x10000)
		return 3;
	if (cp < 0x11000)
		return 4;
	return -1;
}

uint8_t *
cp437_to_utf8(const char *cp437str, size_t buflen, size_t *outlen)
{
	size_t needed = 0;
	int cplen;
	uint8_t *ret = NULL;
	uint8_t *rp;
	size_t idx;
	uint8_t ch;

	// Calculate the number of bytes needed
	for (idx = 0; idx < buflen; idx++) {
		ch = cp437str[idx];
		if (ch == 0)
			cplen += 4;
		else if (ch < 128)
			cplen++;
		else
			cplen = utf8_bytes(ch - 128);
		if (cplen == -1)
			goto error;
		needed += cplen;
	}

	ret = malloc(needed + 1);
	if (ret == NULL)
		goto error;

	rp = ret;
	for (idx = 0; idx < buflen; idx++) {
		ch = cp437str[idx];
		if (ch == 0) {
			*(rp++) = 0xef;
			*(rp++) = 0xbf;
			*(rp++) = 0xbe;
			cplen = 0;
		}
		else if (ch < 128) {
			*rp = ch;
			cplen = 1;
		}
		else {
			cplen = write_cp(rp, cp437_unicode_table[ch - 128]);
			if (cplen < 1)
				goto error;
		}
		rp += cplen;
	}
	*rp = 0;
	return ret;

error:
	free(ret);
	return NULL;
}

/*
 * Converts UTF-8 to CP437, replacing unmapped characters with unmapped
 * if unmapped is zero, unmapped characters are stripped.
 * 
 * Returns NULL if there are invalid sequences or codepoints.
 * Does not normalize the unicode, just a simple mapping
 * (TODO: Normalize into combined chars etc)
 */
char *
utf8_to_cp437(const uint8_t *utf8str, char unmapped)
{
	const uint8_t *p;
	char *rp;
	size_t outlen = 0;
	int incode = 0;
	uint32_t codepoint;
	char *ret = NULL;

	// TODO: Normalize UTF-8...

	// Calculate the number of code points and validate.
	for (p = utf8str; *p; p++) {
		if (incode) {
			switch (*p & 0xc0) {
				case 0x80:
					incode--;
					if (incode == 0)
						outlen++;
					break;
				default:
					goto error;
			}
		}
		else {
			if (*p & 0x80) {
				if ((*p & 0xe0) == 0xc0)
					incode = 1;
				else if ((*p & 0xf0) == 0xe0)
					incode = 2;
				else if ((*p & 0xf8) == 0xf0)
					incode = 3;
				else
					goto error;
			}
			else
				outlen++;
		}
	}
	ret = malloc(outlen + 1);
	if (ret == NULL)
		goto error;
	rp = ret;

	// Fill the string...
	while (*utf8str) {
		utf8str += read_cp(utf8str, &codepoint);
		if (codepoint == 0xffff || codepoint == 0xfffe)
			goto error;
		if (codepoint < 128)
			*(rp++) = codepoint;
		// Multiple representations...
		else if (codepoint == 0xa6) 
			*(rp++) = '|';
		else {
			*(rp++) = cp437_from_unicode_cp(codepoint, unmapped);
		}
	}
	*rp = 0;

	return ret;
error:
	free(ret);
	return NULL;
}
