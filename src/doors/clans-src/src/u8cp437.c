/*

The Clans BBS Door Game
Copyright (C) 1997-2002 Allen Ussher

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

// UTF-8 to CP437 conversion for devkit compilers.
// See devkit/utf8.txt for the full mapping reference.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "platform.h"
#include "u8cp437.h"

// --- Reverse lookup table (Unicode codepoint -> CP437 byte) ---
// Sorted by codepoint for binary search.

struct u8_map {
	uint32_t codepoint;
	uint8_t  cp437;
};

// 168 canonical + 8 alternatives = 176 entries.
// MUST be sorted by codepoint.
static const struct u8_map reverse_table[] = {
	// Control-character glyphs (CP437 0x01-0x1F, 0x7F)
	{ 0x00A0, 0xFF },  // NO-BREAK SPACE
	{ 0x00A1, 0xAD },  // INVERTED EXCLAMATION MARK
	{ 0x00A2, 0x9B },  // CENT SIGN
	{ 0x00A3, 0x9C },  // POUND SIGN
	{ 0x00A5, 0x9D },  // YEN SIGN
	{ 0x00A7, 0x15 },  // SECTION SIGN
	{ 0x00AA, 0xA6 },  // FEMININE ORDINAL INDICATOR
	{ 0x00AB, 0xAE },  // LEFT-POINTING DOUBLE ANGLE QUOTATION MARK
	{ 0x00AC, 0xAA },  // NOT SIGN
	{ 0x00B0, 0xF8 },  // DEGREE SIGN
	{ 0x00B1, 0xF1 },  // PLUS-MINUS SIGN
	{ 0x00B2, 0xFD },  // SUPERSCRIPT TWO
	{ 0x00B5, 0xE6 },  // MICRO SIGN
	{ 0x00B6, 0x14 },  // PILCROW SIGN
	{ 0x00B7, 0xFA },  // MIDDLE DOT
	{ 0x00BA, 0xA7 },  // MASCULINE ORDINAL INDICATOR
	{ 0x00BB, 0xAF },  // RIGHT-POINTING DOUBLE ANGLE QUOTATION MARK
	{ 0x00BC, 0xAC },  // VULGAR FRACTION ONE QUARTER
	{ 0x00BD, 0xAB },  // VULGAR FRACTION ONE HALF
	{ 0x00BF, 0xA8 },  // INVERTED QUESTION MARK
	{ 0x00C4, 0x8E },  // LATIN CAPITAL LETTER A WITH DIAERESIS
	{ 0x00C5, 0x8F },  // LATIN CAPITAL LETTER A WITH RING ABOVE
	{ 0x00C6, 0x92 },  // LATIN CAPITAL LETTER AE
	{ 0x00C7, 0x80 },  // LATIN CAPITAL LETTER C WITH CEDILLA
	{ 0x00C9, 0x90 },  // LATIN CAPITAL LETTER E WITH ACUTE
	{ 0x00D1, 0xA5 },  // LATIN CAPITAL LETTER N WITH TILDE
	{ 0x00D6, 0x99 },  // LATIN CAPITAL LETTER O WITH DIAERESIS
	{ 0x00D8, 0xED },  // LATIN CAPITAL LETTER O WITH STROKE (alt -> phi)
	{ 0x00DC, 0x9A },  // LATIN CAPITAL LETTER U WITH DIAERESIS
	{ 0x00DF, 0xE1 },  // LATIN SMALL LETTER SHARP S
	{ 0x00E0, 0x85 },  // LATIN SMALL LETTER A WITH GRAVE
	{ 0x00E1, 0xA0 },  // LATIN SMALL LETTER A WITH ACUTE
	{ 0x00E2, 0x83 },  // LATIN SMALL LETTER A WITH CIRCUMFLEX
	{ 0x00E4, 0x84 },  // LATIN SMALL LETTER A WITH DIAERESIS
	{ 0x00E5, 0x86 },  // LATIN SMALL LETTER A WITH RING ABOVE
	{ 0x00E6, 0x91 },  // LATIN SMALL LETTER AE
	{ 0x00E7, 0x87 },  // LATIN SMALL LETTER C WITH CEDILLA
	{ 0x00E8, 0x8A },  // LATIN SMALL LETTER E WITH GRAVE
	{ 0x00E9, 0x82 },  // LATIN SMALL LETTER E WITH ACUTE
	{ 0x00EA, 0x88 },  // LATIN SMALL LETTER E WITH CIRCUMFLEX
	{ 0x00EB, 0x89 },  // LATIN SMALL LETTER E WITH DIAERESIS
	{ 0x00EC, 0x8D },  // LATIN SMALL LETTER I WITH GRAVE
	{ 0x00ED, 0xA1 },  // LATIN SMALL LETTER I WITH ACUTE
	{ 0x00EE, 0x8C },  // LATIN SMALL LETTER I WITH CIRCUMFLEX
	{ 0x00EF, 0x8B },  // LATIN SMALL LETTER I WITH DIAERESIS
	{ 0x00F1, 0xA4 },  // LATIN SMALL LETTER N WITH TILDE
	{ 0x00F2, 0x95 },  // LATIN SMALL LETTER O WITH GRAVE
	{ 0x00F3, 0xA2 },  // LATIN SMALL LETTER O WITH ACUTE
	{ 0x00F4, 0x93 },  // LATIN SMALL LETTER O WITH CIRCUMFLEX
	{ 0x00F6, 0x94 },  // LATIN SMALL LETTER O WITH DIAERESIS
	{ 0x00F7, 0xF6 },  // DIVISION SIGN
	{ 0x00F9, 0x97 },  // LATIN SMALL LETTER U WITH GRAVE
	{ 0x00FA, 0xA3 },  // LATIN SMALL LETTER U WITH ACUTE
	{ 0x00FB, 0x96 },  // LATIN SMALL LETTER U WITH CIRCUMFLEX
	{ 0x00FC, 0x81 },  // LATIN SMALL LETTER U WITH DIAERESIS
	{ 0x00FF, 0x98 },  // LATIN SMALL LETTER Y WITH DIAERESIS
	{ 0x0192, 0x9F },  // LATIN SMALL LETTER F WITH HOOK
	{ 0x0393, 0xE2 },  // GREEK CAPITAL LETTER GAMMA
	{ 0x0398, 0xE9 },  // GREEK CAPITAL LETTER THETA
	{ 0x03A3, 0xE4 },  // GREEK CAPITAL LETTER SIGMA
	{ 0x03A6, 0xE8 },  // GREEK CAPITAL LETTER PHI
	{ 0x03A9, 0xEA },  // GREEK CAPITAL LETTER OMEGA
	{ 0x03B1, 0xE0 },  // GREEK SMALL LETTER ALPHA
	{ 0x03B2, 0xE1 },  // GREEK SMALL LETTER BETA (alt -> sharp-s)
	{ 0x03B4, 0xEB },  // GREEK SMALL LETTER DELTA
	{ 0x03B5, 0xEE },  // GREEK SMALL LETTER EPSILON
	{ 0x03BC, 0xE6 },  // GREEK SMALL LETTER MU (alt -> micro sign)
	{ 0x03C0, 0xE3 },  // GREEK SMALL LETTER PI
	{ 0x03C3, 0xE5 },  // GREEK SMALL LETTER SIGMA
	{ 0x03C4, 0xE7 },  // GREEK SMALL LETTER TAU
	{ 0x03C6, 0xED },  // GREEK SMALL LETTER PHI
	{ 0x03D5, 0xED },  // GREEK PHI SYMBOL (alt -> phi)
	{ 0x2022, 0x07 },  // BULLET
	{ 0x203C, 0x13 },  // DOUBLE EXCLAMATION MARK
	{ 0x207F, 0xFC },  // SUPERSCRIPT LATIN SMALL LETTER N
	{ 0x20A7, 0x9E },  // PESETA SIGN
	{ 0x2126, 0xEA },  // OHM SIGN (alt -> omega)
	{ 0x2190, 0x1B },  // LEFTWARDS ARROW
	{ 0x2191, 0x18 },  // UPWARDS ARROW
	{ 0x2192, 0x1A },  // RIGHTWARDS ARROW
	{ 0x2193, 0x19 },  // DOWNWARDS ARROW
	{ 0x2194, 0x1D },  // LEFT RIGHT ARROW
	{ 0x2195, 0x12 },  // UP DOWN ARROW
	{ 0x21A8, 0x17 },  // UP DOWN ARROW WITH BASE
	{ 0x2205, 0xED },  // EMPTY SET (alt -> phi)
	{ 0x2208, 0xEE },  // ELEMENT OF (alt -> epsilon)
	{ 0x2219, 0xF9 },  // BULLET OPERATOR
	{ 0x221A, 0xFB },  // SQUARE ROOT
	{ 0x221E, 0xEC },  // INFINITY
	{ 0x221F, 0x1C },  // RIGHT ANGLE
	{ 0x2229, 0xEF },  // INTERSECTION
	{ 0x2248, 0xF7 },  // ALMOST EQUAL TO
	{ 0x2261, 0xF0 },  // IDENTICAL TO
	{ 0x2264, 0xF3 },  // LESS-THAN OR EQUAL TO
	{ 0x2265, 0xF2 },  // GREATER-THAN OR EQUAL TO
	{ 0x2300, 0xED },  // DIAMETER SIGN (alt -> phi)
	{ 0x2302, 0x7F },  // HOUSE
	{ 0x2310, 0xA9 },  // REVERSED NOT SIGN
	{ 0x2320, 0xF4 },  // TOP HALF INTEGRAL
	{ 0x2321, 0xF5 },  // BOTTOM HALF INTEGRAL
	{ 0x2500, 0xC4 },  // BOX DRAWINGS LIGHT HORIZONTAL
	{ 0x2502, 0xB3 },  // BOX DRAWINGS LIGHT VERTICAL
	{ 0x250C, 0xDA },  // BOX DRAWINGS LIGHT DOWN AND RIGHT
	{ 0x2510, 0xBF },  // BOX DRAWINGS LIGHT DOWN AND LEFT
	{ 0x2514, 0xC0 },  // BOX DRAWINGS LIGHT UP AND RIGHT
	{ 0x2518, 0xD9 },  // BOX DRAWINGS LIGHT UP AND LEFT
	{ 0x251C, 0xC3 },  // BOX DRAWINGS LIGHT VERTICAL AND RIGHT
	{ 0x2524, 0xB4 },  // BOX DRAWINGS LIGHT VERTICAL AND LEFT
	{ 0x252C, 0xC2 },  // BOX DRAWINGS LIGHT DOWN AND HORIZONTAL
	{ 0x2534, 0xC1 },  // BOX DRAWINGS LIGHT UP AND HORIZONTAL
	{ 0x253C, 0xC5 },  // BOX DRAWINGS LIGHT VERT AND HORIZONTAL
	{ 0x2550, 0xCD },  // BOX DRAWINGS DOUBLE HORIZONTAL
	{ 0x2551, 0xBA },  // BOX DRAWINGS DOUBLE VERTICAL
	{ 0x2552, 0xD5 },  // BOX DRAWINGS DOWN SINGLE AND RIGHT DOUBLE
	{ 0x2553, 0xD6 },  // BOX DRAWINGS DOWN DOUBLE AND RIGHT SINGLE
	{ 0x2554, 0xC9 },  // BOX DRAWINGS DOUBLE DOWN AND RIGHT
	{ 0x2555, 0xB8 },  // BOX DRAWINGS DOWN SINGLE AND LEFT DOUBLE
	{ 0x2556, 0xB7 },  // BOX DRAWINGS DOWN DOUBLE AND LEFT SINGLE
	{ 0x2557, 0xBB },  // BOX DRAWINGS DOUBLE DOWN AND LEFT
	{ 0x2558, 0xD4 },  // BOX DRAWINGS UP SINGLE AND RIGHT DOUBLE
	{ 0x2559, 0xD3 },  // BOX DRAWINGS UP DOUBLE AND RIGHT SINGLE
	{ 0x255A, 0xC8 },  // BOX DRAWINGS DOUBLE UP AND RIGHT
	{ 0x255B, 0xBE },  // BOX DRAWINGS UP SINGLE AND LEFT DOUBLE
	{ 0x255C, 0xBD },  // BOX DRAWINGS UP DOUBLE AND LEFT SINGLE
	{ 0x255D, 0xBC },  // BOX DRAWINGS DOUBLE UP AND LEFT
	{ 0x255E, 0xC6 },  // BOX DRAWINGS VERT SINGLE AND RIGHT DOUBLE
	{ 0x255F, 0xC7 },  // BOX DRAWINGS VERT DOUBLE AND RIGHT SINGLE
	{ 0x2560, 0xCC },  // BOX DRAWINGS DOUBLE VERTICAL AND RIGHT
	{ 0x2561, 0xB5 },  // BOX DRAWINGS VERT SINGLE AND LEFT DOUBLE
	{ 0x2562, 0xB6 },  // BOX DRAWINGS VERT DOUBLE AND LEFT SINGLE
	{ 0x2563, 0xB9 },  // BOX DRAWINGS DOUBLE VERTICAL AND LEFT
	{ 0x2564, 0xD1 },  // BOX DRAWINGS DOWN SINGLE AND HORIZ DOUBLE
	{ 0x2565, 0xD2 },  // BOX DRAWINGS DOWN DOUBLE AND HORIZ SINGLE
	{ 0x2566, 0xCB },  // BOX DRAWINGS DOUBLE DOWN AND HORIZONTAL
	{ 0x2567, 0xCF },  // BOX DRAWINGS UP SINGLE AND HORIZ DOUBLE
	{ 0x2568, 0xD0 },  // BOX DRAWINGS UP DOUBLE AND HORIZ SINGLE
	{ 0x2569, 0xCA },  // BOX DRAWINGS DOUBLE UP AND HORIZONTAL
	{ 0x256A, 0xD8 },  // BOX DRAWINGS VERT SINGLE AND HORIZ DOUBLE
	{ 0x256B, 0xD7 },  // BOX DRAWINGS VERT DOUBLE AND HORIZ SINGLE
	{ 0x256C, 0xCE },  // BOX DRAWINGS DOUBLE VERT AND HORIZONTAL
	{ 0x2580, 0xDF },  // UPPER HALF BLOCK
	{ 0x2584, 0xDC },  // LOWER HALF BLOCK
	{ 0x2588, 0xDB },  // FULL BLOCK
	{ 0x258C, 0xDD },  // LEFT HALF BLOCK
	{ 0x2590, 0xDE },  // RIGHT HALF BLOCK
	{ 0x2591, 0xB0 },  // LIGHT SHADE
	{ 0x2592, 0xB1 },  // MEDIUM SHADE
	{ 0x2593, 0xB2 },  // DARK SHADE
	{ 0x25A0, 0xFE },  // BLACK SQUARE
	{ 0x25AC, 0x16 },  // BLACK RECTANGLE
	{ 0x25B2, 0x1E },  // BLACK UP-POINTING TRIANGLE
	{ 0x25BA, 0x10 },  // BLACK RIGHT-POINTING POINTER
	{ 0x25BC, 0x1F },  // BLACK DOWN-POINTING TRIANGLE
	{ 0x25C4, 0x11 },  // BLACK LEFT-POINTING POINTER
	{ 0x25CB, 0x09 },  // WHITE CIRCLE
	{ 0x25D8, 0x08 },  // INVERSE BULLET
	{ 0x25D9, 0x0A },  // INVERSE WHITE CIRCLE
	{ 0x263A, 0x01 },  // WHITE SMILING FACE
	{ 0x263B, 0x02 },  // BLACK SMILING FACE
	{ 0x263C, 0x0F },  // WHITE SUN WITH RAYS
	{ 0x2640, 0x0C },  // FEMALE SIGN
	{ 0x2642, 0x0B },  // MALE SIGN
	{ 0x2660, 0x06 },  // BLACK SPADE SUIT
	{ 0x2663, 0x05 },  // BLACK CLUB SUIT
	{ 0x2665, 0x03 },  // BLACK HEART SUIT
	{ 0x2666, 0x04 },  // BLACK DIAMOND SUIT
	{ 0x266A, 0x0D },  // EIGHTH NOTE
	{ 0x266B, 0x0E },  // BEAMED EIGHTH NOTES
};

#define REVERSE_TABLE_SIZE \
	(sizeof(reverse_table) / sizeof(reverse_table[0]))

// --- Blocked codepoints ---

struct u8_blocked {
	uint32_t    codepoint;
	const char *suggestion;
};

static const struct u8_blocked blocked_table[] = {
	{ 0x2013, "use - (hyphen) instead" },
	{ 0x2014, "use -- (double hyphen) instead" },
	{ 0x2018, "use ASCII apostrophe ' instead" },
	{ 0x2019, "use ASCII apostrophe ' instead" },
	{ 0x201C, "use ASCII quotation mark \" instead" },
	{ 0x201D, "use ASCII quotation mark \" instead" },
};

#define BLOCKED_TABLE_SIZE \
	(sizeof(blocked_table) / sizeof(blocked_table[0]))

// --- Binary search for reverse lookup ---

static int cmp_map(const void *key, const void *elem)
{
	uint32_t cp = *(const uint32_t *)key;
	const struct u8_map *m = (const struct u8_map *)elem;
	if (cp < m->codepoint)
		return -1;
	if (cp > m->codepoint)
		return 1;
	return 0;
}

// --- UTF-8 decoding ---

// Decode one UTF-8 codepoint from buf[0..len-1].
// Sets *codepoint and returns number of bytes consumed (1-4).
// Returns -1 on error (invalid/truncated sequence).
static int decode_utf8(const unsigned char *buf, size_t len,
                       uint32_t *codepoint)
{
	if (len == 0)
		return -1;

	unsigned char b0 = buf[0];

	if (b0 < 0x80) {
		*codepoint = b0;
		return 1;
	}

	int seqlen;
	uint32_t cp;

	if ((b0 & 0xE0) == 0xC0) {
		seqlen = 2;
		cp = b0 & 0x1F;
	} else if ((b0 & 0xF0) == 0xE0) {
		seqlen = 3;
		cp = b0 & 0x0F;
	} else if ((b0 & 0xF8) == 0xF0) {
		seqlen = 4;
		cp = b0 & 0x07;
	} else {
		return -1;  // invalid leading byte
	}

	if ((size_t)seqlen > len)
		return -1;  // truncated

	for (int i = 1; i < seqlen; i++) {
		if ((buf[i] & 0xC0) != 0x80)
			return -1;  // bad continuation byte
		cp = (cp << 6) | (buf[i] & 0x3F);
	}

	// Reject overlong encodings
	if (seqlen == 2 && cp < 0x80)
		return -1;
	if (seqlen == 3 && cp < 0x800)
		return -1;
	if (seqlen == 4 && cp < 0x10000)
		return -1;

	// Reject surrogates and out-of-range
	if (cp >= 0xD800 && cp <= 0xDFFF)
		return -1;
	if (cp > 0x10FFFF)
		return -1;

	*codepoint = cp;
	return seqlen;
}

// --- Public API ---

int u8_to_cp437(unsigned char *buf, size_t len,
                const char *filename, int lineno)
{
	const unsigned char *src = buf;
	const unsigned char * const end = buf + len;
	unsigned char *dst = buf;
	size_t coff = 0;

	while (src < end) {
		// ASCII bytes pass through unchanged
		if (*src < 0x80) {
			*dst++ = *src++;
			coff++;
			continue;
		}

		// Multi-byte UTF-8 sequence
		uint32_t cp;
		int seqlen = decode_utf8(src, (size_t)(end - src), &cp);
		if (seqlen < 0) {
			fprintf(stderr, "%s:%d: invalid UTF-8 at codepoint offset %zu (\\x%02x)\n",
			        filename, lineno, coff, *src);
			return -1;
		}

		// Look up in reverse table (happy path)
		const struct u8_map *found = bsearch(
			&cp, reverse_table, REVERSE_TABLE_SIZE,
			sizeof(reverse_table[0]), cmp_map);

		if (found) {
			*dst++ = found->cp437;
			src += seqlen;
			coff++;
			continue;
		}

		// Not in reverse table -- check for blocked codepoints
		// to give a more helpful error message
		for (size_t i = 0; i < BLOCKED_TABLE_SIZE; i++) {
			if (cp == blocked_table[i].codepoint) {
				fprintf(stderr,
				        "%s:%d: U+%04X is not allowed; %s\n",
				        filename, lineno, cp,
				        blocked_table[i].suggestion);
				return -1;
			}
		}

		// Generic failure
		fprintf(stderr,
		        "%s:%d: U+%04X has no CP437 equivalent\n",
		        filename, lineno, cp);
		return -1;
	}

	// Null-terminate (there's always room since output <= input)
	*dst = '\0';

	return 0;
}

char *u8_fgets(char *buf, int size, FILE *fp, int is_utf8,
               const char *filename, int *lineno)
{
	char *result = fgets(buf, size, fp);
	if (!result)
		return NULL;

	if (lineno)
		(*lineno)++;

	if (!is_utf8)
		return result;

	size_t len = strlen(buf);
	int ln = lineno ? *lineno : 0;
	if (u8_to_cp437((unsigned char *)buf, len, filename, ln) != 0)
		return NULL;

	return result;
}

FILE *u8_fopen(const char *filename, const char *mode, int *is_utf8)
{
	*is_utf8 = 0;

	// Find the last '.' in the basename (FileName() handles / and \ and drive letters)
	const char *base = FileName(filename);
	const char *dot = strrchr(base, '.');

	// Check for .u8. immediately before the final extension
	if (dot && (dot - base) >= 3 && memcmp(dot - 3, ".u8", 3) == 0)
		*is_utf8 = 1;

	FILE *fp = fopen(filename, mode);
	if (fp && *is_utf8)
		printf("Reading UTF-8 file: %s\n", filename);
	return fp;
}
