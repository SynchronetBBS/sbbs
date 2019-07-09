/* Synchronet Unicode encode/decode/translate functions */

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright Rob Swindell - http://www.synchro.net/copyright.html			*
 *																			*
 * This library is free software; you can redistribute it and/or			*
 * modify it under the terms of the GNU Lesser General Public License		*
 * as published by the Free Software Foundation; either version 2			*
 * of the License, or (at your option) any later version.					*
 * See the GNU Lesser General Public License for more details: lgpl.txt or	*
 * http://www.fsf.org/copyleft/lesser.html									*
 *																			*
 * Anonymous FTP access to the most recent released source is available at	*
 * ftp://vert.synchro.net, ftp://cvs.synchro.net and ftp://ftp.synchro.net	*
 *																			*
 * Anonymous CVS access to the development source and modification history	*
 * is available at cvs.synchro.net:/cvsroot/sbbs, example:					*
 * cvs -d :pserver:anonymous@cvs.synchro.net:/cvsroot/sbbs login			*
 *     (just hit return, no password is necessary)							*
 * cvs -d :pserver:anonymous@cvs.synchro.net:/cvsroot/sbbs checkout src		*
 *																			*
 * For Synchronet coding style and modification guidelines, see				*
 * http://www.synchro.net/source.html										*
 *																			*
 * You are encouraged to submit any modifications (preferably in Unix diff	*
 * format) via e-mail to mods@synchro.net									*
 *																			*
 * Note: If this box doesn't appear square, then you need to fix your tabs.	*
 ****************************************************************************/

#include "unicode.h"
#include "cp437defs.h"
#include "unicode_defs.h"

// Want UNICDE encodings of terminal control characters?
#if defined USE_UNICODE_FOR_TERM_CTRL_CHARS
#	define UNICODE_TERM_CTRL_CHAR_CODE(x) x
#else
#	define UNICODE_TERM_CTRL_CHAR_CODE(x) 0
#endif

// CP437 character to/from UNICODE code point conversion
// The CP437 character value is the index into the table.
// If the value at that index is 0, no translation is needed (1:1 mapping).
uint32_t cp437_unicode_tbl[] =
{
	/* 0x00 */ 0,
	/* 0x01 */ 0x263A,
	/* 0x02 */ 0x263B,
	/* 0x03 */ 0x2665,
	/* 0x04 */ 0x2666,
	/* 0x05 */ 0x2663,
	/* 0x06 */ 0x2660,
	/* 0x07 '\a' */	UNICODE_TERM_CTRL_CHAR_CODE(0x2022),
	/* 0x08 '\b' */	UNICODE_TERM_CTRL_CHAR_CODE(0x25D8),
	/* 0x09 '\t' */	UNICODE_TERM_CTRL_CHAR_CODE(0x25CB),
	/* 0x0A '\n' */	UNICODE_TERM_CTRL_CHAR_CODE(0x25D9),
	/* 0x0B */ 0x2642,
	/* 0x0C '\f' */	UNICODE_TERM_CTRL_CHAR_CODE(0x2640),
	/* 0x0D '\r' */	UNICODE_TERM_CTRL_CHAR_CODE(0x266A),
	/* 0x0E */ 0x266B,
	/* 0x0F */ 0x263C,
	/* 0x10 */ 0x25BA,
	/* 0x11 */ 0x25C4,
	/* 0x12 */ 0x2195,
	/* 0x13 */ 0x203C,
	/* 0x14 */ 0x00B6,
	/* 0x15 */ 0x00A7,
	/* 0x16 */ 0x25AC,
	/* 0x17 */ 0x21A8,
	/* 0x18 */ 0x2191,
	/* 0x19 */ 0x2193,
	/* 0x1A */ 0x2192,
	/* 0x1B '\e' */ UNICODE_TERM_CTRL_CHAR_CODE(0x2190),
	/* 0x1C */ 0x221F,
	/* 0x1D */ 0x2194,
	/* 0x1E */ 0x25B2,
	/* 0x1F */ 0x25BC,
	/* 0x20-0x7E	(1:1 with US-ASCII and CP437) */
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	/* 0x7F (DEL) */ UNICODE_TERM_CTRL_CHAR_CODE(0x2302),
	/* 0x80 */ 0x00C7,
	/* 0x81 */ 0x00FC,
	/* 0x82 */ 0x00E9,
	/* 0x83 */ 0x00E2,
	/* 0x84 */ 0x00E4,
	/* 0x85 */ 0x00E0,
	/* 0x86 */ 0x00E5,
	/* 0x87 */ 0x00E7,
	/* 0x88 */ 0x00EA,
	/* 0x89 */ 0x00EB,
	/* 0x8A */ 0x00E8,
	/* 0x8B */ 0x00EF,
	/* 0x8C */ 0x00EE,
	/* 0x8D */ 0x00EC,
	/* 0x8E */ 0x00C4,
	/* 0x8F */ 0x00C5,
	/* 0x90 */ 0x00C9,
	/* 0x91 */ 0x00E6,
	/* 0x92 */ 0x00C6,
	/* 0x93 */ 0x00F4,
	/* 0x94 */ 0x00F6,
	/* 0x95 */ 0x00F2,
	/* 0x96 */ 0x00F8,
	/* 0x97 */ 0x00F9,
	/* 0x98 */ 0x00FF,
	/* 0x99 */ 0x00D6,
	/* 0x9A */ 0x00DC,
	/* 0x9B */ 0x00A2,
	/* 0x9C */ 0x00A3,
	/* 0x9D */ 0x00A5,
	/* 0x9E */ 0x20A7,
	/* 0x9F */ 0x0192,
	/* 0xA0 */ 0x00E1,
	/* 0xA1 */ 0x00ED,
	/* 0xA2 */ 0x00F3,
	/* 0xA3 */ 0x00FA,
	/* 0xA4 */ 0x00F1,
	/* 0xA5 */ 0x00D1,
	/* 0xA6 */ 0x00AA,
	/* 0xA7 */ 0x00BA,
	/* 0xA8 */ 0x00BF,
	/* 0xA9 */ 0x2310,
	/* 0xAA */ 0x00AC,
	/* 0xAB */ 0x00BD,
	/* 0xAC */ 0x00BC,
	/* 0xAD */ 0x00A1,
	/* 0xAE */ 0x00AB,
	/* 0xAF */ 0x00BB,
	/* 0xB0 */ 0x2591,
	/* 0xB1 */ 0x2592,
	/* 0xB2 */ 0x2593,
	/* 0xB3 */ 0x2502,
	/* 0xB4 */ 0x2524,
	/* 0xB5 */ 0x2561,
	/* 0xB6 */ 0x2562,
	/* 0xB7 */ 0x2556,
	/* 0xB8 */ 0x2555,
	/* 0xB9 */ 0x2563,
	/* 0xBA */ 0x2551,
	/* 0xBB */ 0x2557,
	/* 0xBC */ 0x255D,
	/* 0xBD */ 0x255C,
	/* 0xBE */ 0x255B,
	/* 0xBF */ 0x2510,
	/* 0xC0 */ 0x2514,
	/* 0xC1 */ 0x2534,
	/* 0xC2 */ 0x252C,
	/* 0xC3 */ 0x251C,
	/* 0xC4 */ 0x2500,
	/* 0xC5 */ 0x253C,
	/* 0xC6 */ 0x255E,
	/* 0xC7 */ 0x255F,
	/* 0xC8 */ 0x255A,
	/* 0xC9 */ 0x2554,
	/* 0xCA */ 0x2569,
	/* 0xCB */ 0x2566,
	/* 0xCC */ 0x2560,
	/* 0xCD */ 0x2550,
	/* 0xCE */ 0x256C,
	/* 0xCF */ 0x2567,
	/* 0xD0 */ 0x2568,
	/* 0xD1 */ 0x2564,
	/* 0xD2 */ 0x2565,
	/* 0xD3 */ 0x2559,
	/* 0xD4 */ 0x2558,
	/* 0xD5 */ 0x2552,
	/* 0xD6 */ 0x2553,
	/* 0xD7 */ 0x256B,
	/* 0xD8 */ 0x256A,
	/* 0xD9 */ 0x2518,
	/* 0xDA */ 0x250C,
	/* 0xDB */ 0x2588,
	/* 0xDC */ 0x2584,
	/* 0xDD */ 0x258C,
	/* 0xDE */ 0x2590,
	/* 0xDF */ 0x2580,
	/* 0xE0 */ 0x03B1,
	/* 0xE1 */ 0x00DF,
	/* 0xE2 */ 0x0393,
	/* 0xE3 */ 0x03C0,
	/* 0xE4 */ 0x03A3,
	/* 0xE5 */ 0x03C3,
	/* 0xE6 */ 0x00B5,
	/* 0xE7 */ 0x03C4,
	/* 0xE8 */ 0x03A6,
	/* 0xE9 */ 0x0398,
	/* 0xEA */ 0x03A9,
	/* 0xEB */ 0x03B4,
	/* 0xEC */ 0x221E,
	/* 0xED */ 0x03C6,
	/* 0xEE */ 0x03B5,
	/* 0xEF */ 0x2229,
	/* 0xF0 */ 0x2261,
	/* 0xF1 */ 0x00B1,
	/* 0xF2 */ 0x2265,
	/* 0xF3 */ 0x2264,
	/* 0xF4 */ 0x2320,
	/* 0xF5 */ 0x2321,
	/* 0xF6 */ 0x00F7,
	/* 0xF7 */ 0x2248,
	/* 0xF8 */ 0x00B0,
	/* 0xF9 */ 0x2219,
	/* 0xFA */ 0x00B7,
	/* 0xFB */ 0x221A,
	/* 0xFC */ 0x207F,
	/* 0xFD */ 0x00B2,
	/* 0xFE */ 0xFFED, //0x25A0,
	/* 0xFF */ 0x00A0
};

bool unicode_is_zerowidth(uint32_t u)
{
	switch(u) {
		case UNICODE_ZERO_WIDTH_SPACE:
		case UNICODE_ZERO_WIDTH_NON_JOINER:
		case UNICODE_ZERO_WIDTH_JOINER:
		case UNICODE_VARIATION_SELECTOR_1:
		case UNICODE_VARIATION_SELECTOR_2:
		case UNICODE_VARIATION_SELECTOR_3:
		case UNICODE_VARIATION_SELECTOR_4:
		case UNICODE_VARIATION_SELECTOR_5:
		case UNICODE_VARIATION_SELECTOR_6:
		case UNICODE_VARIATION_SELECTOR_7:
		case UNICODE_VARIATION_SELECTOR_8:
		case UNICODE_VARIATION_SELECTOR_9:
		case UNICODE_VARIATION_SELECTOR_10:
		case UNICODE_VARIATION_SELECTOR_11:
		case UNICODE_VARIATION_SELECTOR_12:
		case UNICODE_VARIATION_SELECTOR_13:
		case UNICODE_VARIATION_SELECTOR_14:
		case UNICODE_VARIATION_SELECTOR_15:
		case UNICODE_VARIATION_SELECTOR_16:
		case UNICODE_ZERO_WIDTH_NO_BREAK_SPACE:
			return true;
	}
	return false;
}

char unicode_to_cp437(uint32_t codepoint)
{
	switch(codepoint) {
		case 0:													return '\0';

		case UNICODE_ACUTE_ACCENT:								return '\'';

		case UNICODE_BROKEN_BAR:								return '|';

		// Perform Upper -> Lower case mapping where an upper case equivalent doesn't exist in CP437:
		case UNICODE_LATIN_CAPITAL_LETTER_A_WITH_GRAVE:			return CP437_CHAR_LATIN_SMALL_LETTER_A_WITH_GRAVE;
		case UNICODE_LATIN_CAPITAL_LETTER_A_WITH_ACUTE:			return CP437_CHAR_LATIN_SMALL_LETTER_A_WITH_ACUTE;
		case UNICODE_LATIN_CAPITAL_LETTER_A_WITH_CIRCUMFLEX:	return CP437_CHAR_LATIN_SMALL_LETTER_A_WITH_CIRCUMFLEX;

		case UNICODE_LATIN_CAPITAL_LETTER_E_WITH_GRAVE:			return CP437_CHAR_LATIN_SMALL_LETTER_E_WITH_GRAVE;
		case UNICODE_LATIN_CAPITAL_LETTER_E_WITH_CIRCUMFLEX:	return CP437_CHAR_LATIN_SMALL_LETTER_E_WITH_CIRCUMFLEX;
		case UNICODE_LATIN_CAPITAL_LETTER_E_WITH_DIAERESIS:		return CP437_CHAR_LATIN_SMALL_LETTER_E_WITH_DIAERESIS;

		case UNICODE_LATIN_CAPITAL_LETTER_I_WITH_GRAVE:			return CP437_CHAR_LATIN_SMALL_LETTER_I_WITH_GRAVE;
		case UNICODE_LATIN_CAPITAL_LETTER_I_WITH_ACUTE:			return CP437_CHAR_LATIN_SMALL_LETTER_I_WITH_ACUTE;
		case UNICODE_LATIN_CAPITAL_LETTER_I_WITH_CIRCUMFLEX:	return CP437_CHAR_LATIN_SMALL_LETTER_I_WITH_CIRCUMFLEX;
		case UNICODE_LATIN_CAPITAL_LETTER_I_WITH_DIAERESIS:		return CP437_CHAR_LATIN_SMALL_LETTER_I_WITH_DIAERESIS;

		case UNICODE_LATIN_CAPITAL_LETTER_O_WITH_GRAVE:			return CP437_CHAR_LATIN_SMALL_LETTER_O_WITH_GRAVE;
		case UNICODE_LATIN_CAPITAL_LETTER_O_WITH_ACUTE:			return CP437_CHAR_LATIN_SMALL_LETTER_O_WITH_ACUTE;
		case UNICODE_LATIN_CAPITAL_LETTER_O_WITH_CIRCUMFLEX:	return CP437_CHAR_LATIN_SMALL_LETTER_O_WITH_CIRCUMFLEX;

		case UNICODE_LATIN_CAPITAL_LETTER_U_WITH_GRAVE:			return CP437_CHAR_LATIN_SMALL_LETTER_U_WITH_GRAVE;
		case UNICODE_LATIN_CAPITAL_LETTER_U_WITH_ACUTE:			return CP437_CHAR_LATIN_SMALL_LETTER_U_WITH_ACUTE;
		case UNICODE_LATIN_CAPITAL_LETTER_U_WITH_CIRCUMFLEX:	return CP437_CHAR_LATIN_SMALL_LETTER_U_WITH_CIRCUMFLEX;

		case UNICODE_LATIN_CAPITAL_LETTER_Y_WITH_DIAERESIS:		return CP437_CHAR_LATIN_SMALL_LETTER_Y_WITH_DIAERESIS;

		// Greek letters
		case UNICODE_GREEK_CAPITAL_LETTER_ALPHA:
		case UNICODE_GREEK_CAPITAL_LETTER_ALPHA_WITH_TONOS:		return 'A';
		case UNICODE_GREEK_CAPITAL_LETTER_BETA:					return 'B';
		case UNICODE_GREEK_CAPITAL_LETTER_GAMMA:
		case UNICODE_GREEK_SMALL_LETTER_GAMMA:					return CP437_CHAR_GREEK_CAPITAL_LETTER_GAMMA;
		case UNICODE_GREEK_CAPITAL_LETTER_DELTA:
		case UNICODE_GREEK_SMALL_LETTER_DELTA:					return CP437_CHAR_GREEK_SMALL_LETTER_DELTA;
		case UNICODE_GREEK_CAPITAL_LETTER_EPSILON:
		case UNICODE_GREEK_CAPITAL_LETTER_EPSILON_WITH_TONOS:
		case UNICODE_GREEK_SMALL_LETTER_EPSILON:
		case UNICODE_GREEK_SMALL_LETTER_EPSILON_WITH_TONOS:		return CP437_CHAR_GREEK_SMALL_LETTER_EPSILION;
		case UNICODE_GREEK_CAPITAL_LETTER_ZETA:					return 'Z';
		case UNICODE_GREEK_CAPITAL_LETTER_ETA:					return 'H';
		case UNICODE_GREEK_CAPITAL_LETTER_THETA:
		case UNICODE_GREEK_SMALL_LETTER_THETA:
		case UNICODE_GREEK_THETA_SYMBOL:						return CP437_CHAR_GREEK_SMALL_LETTER_THETA;
		case UNICODE_GREEK_CAPITAL_LETTER_IOTA:					
		case UNICODE_GREEK_CAPITAL_LETTER_IOTA_WITH_DIALYTIKA:	return 'I';
		case UNICODE_GREEK_CAPITAL_LETTER_KAPPA:				return 'K';
		case UNICODE_GREEK_CAPITAL_LETTER_MU:					return 'M';
		case UNICODE_GREEK_CAPITAL_LETTER_NU:					return 'N';
		case UNICODE_GREEK_CAPITAL_LETTER_OMICRON:				return 'O';
		case UNICODE_GREEK_CAPITAL_LETTER_PI:
		case UNICODE_GREEK_SMALL_LETTER_PI:
		case UNICODE_GREEK_PI_SYMBOL:							return CP437_CHAR_GREEK_SMALL_LETTER_PI;
		case UNICODE_GREEK_CAPITAL_LETTER_RHO:					return 'P';
		case UNICODE_GREEK_CAPITAL_LETTER_SIGMA:				return CP437_CHAR_GREEK_CAPITAL_LETTER_SIGMA;			
		case UNICODE_GREEK_CAPITAL_LETTER_TAU:
		case UNICODE_GREEK_SMALL_LETTER_TAU:
																return 'T';
		case UNICODE_GREEK_CAPITAL_LETTER_UPSILON:				return 'Y';
		case UNICODE_GREEK_CAPITAL_LETTER_PHI:					return CP437_CHAR_GREEK_CAPITAL_LETTER_PHI;
		case UNICODE_GREEK_CAPITAL_LETTER_CHI:					return 'X';
		case UNICODE_GREEK_CAPITAL_LETTER_OMEGA:
		case UNICODE_GREEK_CAPITAL_LETTER_OMEGA_WITH_TONOS:
		case UNICODE_GREEK_SMALL_LETTER_OMEGA:
		case UNICODE_GREEK_SMALL_LETTER_OMEGA_WITH_TONOS:		return CP437_CHAR_GREEK_CAPITAL_LETTER_OMEGA;
		case UNICODE_GREEK_CAPITAL_LETTER_UPSILON_WITH_DIALYTIKA:
																return CP437_CHAR_LATIN_SMALL_LETTER_Y_WITH_DIAERESIS;
		case UNICODE_GREEK_SMALL_LETTER_ALPHA:
		case UNICODE_GREEK_SMALL_LETTER_ALPHA_WITH_TONOS:		return CP437_CHAR_GREEK_SMALL_LETTER_ALPHA;
		case UNICODE_GREEK_SMALL_LETTER_BETA:					return CP437_CHAR_GREEK_SMALL_LETTER_BETA;
		case UNICODE_GREEK_SMALL_LETTER_MU:						return CP437_CHAR_GREEK_SMALL_LETTER_MU;
		case UNICODE_GREEK_SMALL_LETTER_NU:						return 'v';
		case UNICODE_GREEK_SMALL_LETTER_OMICRON:				return 'o';
		case UNICODE_GREEK_SMALL_LETTER_OMICRON_WITH_TONOS:		return CP437_CHAR_LATIN_SMALL_LETTER_O_WITH_ACUTE;
		case UNICODE_GREEK_SMALL_LETTER_UPSILON:				return 'u';
		case UNICODE_GREEK_SMALL_LETTER_UPSILON_WITH_TONOS:		return CP437_CHAR_LATIN_SMALL_LETTER_U_WITH_ACUTE;
		case UNICODE_GREEK_SMALL_LETTER_UPSILON_WITH_DIALYTIKA:
		case UNICODE_GREEK_SMALL_LETTER_UPSILON_WITH_DIALYTIKA_AND_TONOS:
																return CP437_CHAR_LATIN_SMALL_LETTER_U_WITH_DIAERESIS;
		case UNICODE_GREEK_SMALL_LETTER_IOTA:					
		case UNICODE_GREEK_SMALL_LETTER_IOTA_WITH_TONOS:		return 'i';
		case UNICODE_GREEK_SMALL_LETTER_KAPPA:					return 'k';
		case UNICODE_GREEK_SMALL_LETTER_CHI:					return 'x';
		case UNICODE_GREEK_SMALL_LETTER_SIGMA:
		case UNICODE_GREEK_SMALL_LETTER_FINAL_SIGMA:			return CP437_CHAR_GREEK_SMALL_LETTER_SIGMA;
		case UNICODE_GREEK_SMALL_LETTER_RHO:					return 'p';	
		case UNICODE_GREEK_SMALL_LETTER_ZETA:					return 'z';
		case UNICODE_GREEK_SMALL_LETTER_ETA:					return 'n';

		case UNICODE_EM_DASH:
			return '\xC4';

		case UNICODE_BULLET: // BULLET
			return '\xF9';

		case UNICODE_NO_BREAK_SPACE:
		case UNICODE_EN_QUAD:
		case UNICODE_EM_QUAD:
		case UNICODE_EN_SPACE:
		case UNICODE_EM_SPACE:
			return ' ';

		case UNICODE_OVERLINE:
		case 0x2500: // Box Drawings Light Horizontal
		case 0x2501: // Box Drawings Heavy Horizontal
		case 0x2504: // Box Drawings Light Triple Dash Horizontal
		case 0x2505: // Box Drawings Heavy Triple Dash Horizontal
		case 0x2508: // Box Drawings Light Quadruple Dash Horizontal
		case 0x2509: // Box Drawings Heavy Quadruple Dash Horizontal
		case 0x254C: // Box Drawings Light Double Dash Horizontal
		case 0x254D: // Box Drawings Heavy Double Dash Horizontal
		case 0x2574: // Box Drawings Light Left
		case 0x2576: // Box Drawings Light Right
		case 0x2578: // Box Drawings Heavy Left
		case 0x257A: // Box Drawings Heavy Right
		case 0x257C: // Box Drawings Light Left and Heavy Right
		case 0x257E: // Box Drawings Heavy Left and Light Right
			return '\xC4';

		case 0x2502: // Box Drawings Light Vertical
		case 0x2503: // Box Drawings Heavy Vertical
		case 0x2506: // Box Drawings Light Triple Dash Vertical
		case 0x2507: // Box Drawings Heavy Triple Dash Vertical
		case 0x250A: // Box Drawings Light Quadruple Dash Vertical
		case 0x250B: // Box Drawings Heavy Quadruple Dash Vertical
			return '\xB3';

		case 0x250C: // BOX DRAWINGS LIGHT DOWN AND RIGHT
		case 0x250D:
		case 0x250E:
		case 0x250F: // BOX DRAWINGS HEAVY DOWN AND RIGHT
			return '\xDA';

		case 0x2510: // BOX DRAWINGS LIGHT DOWN AND LEFT
		case 0x2511:
		case 0x2512:
		case 0x2513: // BOX DRAWINGS HEAVY DOWN AND LEFT
			return '\xBF';

		case 0x2514: // BOX DRAWINGS LIGHT UP AND RIGHT
		case 0x2515:
		case 0x2516:
		case 0x2517: // BOX DRAWINGS HEAVY UP AND RIGHT
			return '\xC0';

		case 0x2518: // BOX DRAWINGS LIGHT UP AND LEFT
		case 0x2519:
		case 0x251A:
		case 0x251B: // BOX DRAWINGS HEAVY UP AND LEFT
			return '\xD9';

		case 0x251C: // BOX DRAWINGS LIGHT VERTICAL AND RIGHT
		case 0x251D:
		case 0x251E:
		case 0x251F:
		case 0x2520:
		case 0x2521:
		case 0x2522:
		case 0x2523: // BOX DRAWINGS HEAVY VERTICAL AND RIGHT
			return '\xC3';

		case 0x2524: // BOX DRAWINGS LIGHT VERTICAL AND LEFT
		case 0x2525:
		case 0x2526:
		case 0x2527:
		case 0x2528:
		case 0x2529:
		case 0x252A:
		case 0x252B:
			return '\xB4';

		case 0x252C: // BOX DRAWINGS LIGHT DOWN AND HORIZONTAL
		case 0x252D:
		case 0x252E:
		case 0x252F:
		case 0x2530:
		case 0x2531:
		case 0x2532: // BOX DRAWINGS LEFT LIGHT AND RIGHT DOWN HEAVY
		case 0x2533: // BOX DRAWINGS HEAVY DOWN AND HORIZONTAL
			return '\xC2';

		case 0x2534: // BOX DRAWINGS LIGHT UP AND HORIZONTAL
		case 0x2535: // BOX DRAWINGS LEFT HEAVY AND RIGHT UP LIGHT
		case 0x2536: // BOX DRAWINGS RIGHT HEAVY AND LEFT UP LIGHT
		case 0x2537: // BOX DRAWINGS UP LIGHT AND HORIZONTAL HEAVY
		case 0x2538: // BOX DRAWINGS UP HEAVY AND HORIZONTAL LIGHT
		case 0x2539: // BOX DRAWINGS RIGHT LIGHT AND LEFT UP HEAVY
		case 0x253A: // BOX DRAWINGS LEFT LIGHT AND RIGHT UP HEAVY
		case 0x253B: // BOX DRAWINGS HEAVY UP AND HORIZONTAL
			return '\xC1';

		case 0x253C: // BOX DRAWINGS LIGHT VERTICAL AND HORIZONTAL
		case 0x253D: // BOX DRAWINGS LEFT HEAVY AND RIGHT VERTICAL LIGHT
		case 0x253E: // BOX DRAWINGS RIGHT HEAVY AND LEFT VERTICAL LIGHT
		case 0x253F: // BOX DRAWINGS VERTICAL LIGHT AND HORIZONTAL HEAVY
		case 0x2540: // BOX DRAWINGS UP HEAVY AND DOWN HORIZONTAL LIGHT
		case 0x2541: // BOX DRAWINGS DOWN HEAVY AND UP HORIZONTAL LIGHT
		case 0x2542: // BOX DRAWINGS VERTICAL HEAVY AND HORIZONTAL LIGHT
		case 0x2543: // BOX DRAWINGS LEFT UP HEAVY AND RIGHT DOWN LIGHT
		case 0x2544: // BOX DRAWINGS RIGHT UP HEAVY AND LEFT DOWN LIGHT
		case 0x2545: // BOX DRAWINGS LEFT DOWN HEAVY AND RIGHT UP LIGHT
		case 0x2546: // BOX DRAWINGS RIGHT DOWN HEAVY AND LEFT UP LIGHT
		case 0x2547: // BOX DRAWINGS DOWN LIGHT AND UP HORIZONTAL HEAVY
		case 0x2548: // BOX DRAWINGS UP LIGHT AND DOWN HORIZONTAL HEAVY
		case 0x2549: // BOX DRAWINGS RIGHT LIGHT AND LEFT VERTICAL HEAVY
		case 0x254A: // BOX DRAWINGS LEFT LIGHT AND RIGHT VERTICAL HEAVY
		case 0x254B: // BOX DRAWINGS HEAVY VERTICAL AND HORIZONTAL
			return '\xC5';

		case 0x254E: // BOX DRAWINGS LIGHT DOUBLE DASH VERTICAL
		case 0x254F: // BOX DRAWINGS HEAVY DOUBLE DASH VERTICAL
			return '|';

		case 0x256D: // BOX DRAWINGS LIGHT ARC DOWN AND RIGHT
			return '\xDA';

		case 0x256E: // BOX DRAWINGS LIGHT ARC DOWN AND LEFT
			return '\xBF';

		case 0x256F: // BOX DRAWINGS LIGHT ARC UP AND LEFT
			return '\xD9';

		case 0x2570: // BOX DRAWINGS LIGHT ARC UP AND RIGHT
			return '\xC0';

		case 0x2571: // BOX DRAWINGS LIGHT DIAGONAL UPPER RIGHT TO LOWER LEFT
			return '/';

		case 0x2572: // BOX DRAWINGS LIGHT DIAGONAL UPPER LEFT TO LOWER RIGHT
			return '\\';

		case 0x2573: // BOX DRAWINGS LIGHT DIAGONAL CROSS
			return 'X';

		case 0x2575: // Box Drawings Light Up
		case 0x2577: // Box Drawings Light Down
		case 0x2579: // Box Drawings Heavy Up
		case 0x257B: // Box Drawings Heavy Down
		case 0x257D: // Box Drawings Light Up and Heavy Down
		case 0x257F: // Box Drawings Heavy Up and Light Down
			return '\xB3';

		case 0x2581: // Lower One Eighth Block
			return '_';

		case 0x2582: // Lower One Quarter Block
		case 0x2583: // Lower Three Eighths Block
			return '\x16';

		case 0x2585: // Lower Five Eighths Block
		case 0x2586: // Lower Three Quarters Block
		case 0x2587: // Lower Seven Eighths Block
			return '\xDC';

		case 0x2588: // Full Block
		case 0x2589: // Left Seven Eighths Block
			return '\xDB';

		case 0x258A: // Left Three Quarters Block
		case 0x258B: // Left Five Eighths Block
		case 0x258C: // Left Half Block
		case 0x258D: // Left Three Eighths Block
		case 0x258E: // Left One Quarter Block
		case 0x258F: // Left One Eighth Block
			return '\xDD';

		case 0x2590: // Right Half Block
		case 0x2595: // Right One Eighth Block
			return '\xDE';

		case 0x2594: // Upper One Eighth Block
			return '\xDF';
	}

	// Look for a 1:1 match in the CP437 -> Unicode table
	for(int i = 1; i < 0x100; i++) {
		if(cp437_unicode_tbl[i] == codepoint)
			return i;
	}

	return '\0'; // Not-mapped
}
