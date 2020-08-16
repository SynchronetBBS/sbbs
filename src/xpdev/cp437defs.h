/* Synchronet IBM Code Page 437 (CP437) definitions */

/* $Id: cp437defs.h,v 1.5 2019/08/30 10:47:41 rswindell Exp $ */

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

#ifndef CP437DEFS_H_
#define CP437DEFS_H_

enum cp437_char {
	CP437_WHITE_SMILING_FACE = '\x01',
	CP437_BLACK_SMILING_FACE = '\x02',
	CP437_BLACK_HEART_SUIT = '\x03',
	CP437_BLACK_DIAMOND_SUIT = '\x04',
	CP437_BLACK_CLUB_SUIT = '\x05',
	CP437_BLACK_SPADE_SUIT = '\x06',
	CP437_BULLET = '\x07',
	CP437_INVERSE_BULLET = '\x08',
	CP437_WHITE_CIRCLE = '\x09',
	CP437_INVERSE_WHITE_CIRCLE = '\x0A',
	CP437_MALE_SIGN = '\x0B',
	CP437_FEMALE_SIGN = '\x0C',
	CP437_EIGHTH_NOTE = '\x0D',
	CP437_BEAMED_EIGHTH_NOTES = '\x0E',
	CP437_WHITE_SUN_WITH_RAYS = '\x0F',
	CP437_BLACK_RIGHT_POINTING_POINTER = '\x10',
	CP437_BLACK_LEFT_POINTING_POINTER = '\x11',
	CP437_UP_DOWN_ARROW = '\x12',
	CP437_DOUBLE_EXCLAMATION_MARK = '\x13',
	CP437_PILCROW_SIGN = '\x14',	// Paragraph mark
	CP437_SECTION_SIGN = '\x15',
	CP437_BLACK_RECTANGLE = '\x16',
	CP437_UP_DOWN_ARROW_WITH_BASE = '\x17',
	CP437_UPWARDS_ARROW = '\x18',
	CP437_DOWNWARDS_ARROW = '\x19',
	CP437_RIGHTWARDS_ARROW = '\x1A',
	CP437_LEFTWARDS_ARROW = '\x1B',
	CP437_RIGHT_ANGLE = '\x1C',
	CP437_LEFT_RIGHT_ARROW = '\x1D',
	CP437_BLACK_UP_POINTING_TRIANGLE = '\x1E',
	CP437_BLACK_DOWN_POINTING_TRIANGLE = '\x1F',

	// 0x20 - 0x7E are ASCII characters

	CP437_HOUSE = '\x7F',
	CP437_LATIN_CAPTIAL_LETTER_C_WITH_CEDILLA = '\x80',
	CP437_LATIN_SMALL_LETTER_U_WITH_DIAERESIS = '\x81',
	CP437_LATIN_SMALL_LETTER_E_WITH_ACUTE = '\x82',
	CP437_LATIN_SMALL_LETTER_A_WITH_CIRCUMFLEX = '\x83',
	CP437_LATIN_SMALL_LETTER_A_WITH_DIAERESIS = '\x84',
	CP437_LATIN_SMALL_LETTER_A_WITH_GRAVE = '\x85',
	CP437_LATIN_SMALL_LETTER_A_WITH_RING_ABOVE = '\x86',
	CP437_LATIN_SMALL_LETTER_C_WITH_CEDILLA = '\x87',
	CP437_LATIN_SMALL_LETTER_E_WITH_CIRCUMFLEX = '\x88',
	CP437_LATIN_SMALL_LETTER_E_WITH_DIAERESIS = '\x89',
	CP437_LATIN_SMALL_LETTER_E_WITH_GRAVE = '\x8a',
	CP437_LATIN_SMALL_LETTER_I_WITH_DIAERESIS = '\x8b',
	CP437_LATIN_SMALL_LETTER_I_WITH_CIRCUMFLEX = '\x8c',
	CP437_LATIN_SMALL_LETTER_I_WITH_GRAVE = '\x8d',
	CP437_LATIN_CAPITAL_LETTER_A_WITH_DIAERESIS = '\x8e',
	CP437_LATIN_CAPITAL_LETTER_A_WITH_RING_ABOVE = '\x8f',
	CP437_LATIN_CAPITAL_LETTER_E_WITH_ACUTE = '\x90',
	CP437_LATIN_SMALL_LETTER_AE = '\x91',
	CP437_LATIN_CAPITAL_LETTER_AE = '\x92',
	CP437_LATIN_SMALL_LETTER_O_WITH_CIRCUMFLEX = '\x93',
	CP437_LATIN_SMALL_LETTER_O_WITH_DIAERESIS = '\x94',
	CP437_LATIN_SMALL_LETTER_O_WITH_GRAVE = '\x95',
	CP437_LATIN_SMALL_LETTER_U_WITH_CIRCUMFLEX = '\x96',
	CP437_LATIN_SMALL_LETTER_U_WITH_GRAVE = '\x97',
	CP437_LATIN_SMALL_LETTER_Y_WITH_DIAERESIS = '\x98',
	CP437_LATIN_CAPITAL_LETTER_O_WITH_DIAERESIS = '\x99',
	CP437_LATIN_CAPITAL_LETTER_U_WITH_DIAERESIS = '\x9A',
	CP437_CENT_SIGN = '\x9B',
	CP437_POUND_SIGN = '\x9C',
	CP437_YEN_SIGN = '\x9E',
	CP437_PESETA_SIGN = '\x9E',
	CP437_LATIN_SMALL_LETTER_F_WITH_HOOK = '\x9F',
	CP437_LATIN_SMALL_LETTER_A_WITH_ACUTE = '\xA0',
	CP437_LATIN_SMALL_LETTER_I_WITH_ACUTE = '\xA1',
	CP437_LATIN_SMALL_LETTER_O_WITH_ACUTE = '\xA2',
	CP437_LATIN_SMALL_LETTER_U_WITH_ACUTE = '\xA3',
	CP437_LATIN_SMALL_LETTER_N_WITH_TILDE = '\xA4',
	CP437_LATIN_CAPITAL_LETTER_N_WITH_TILDE = '\xA5',
	CP437_FEMININE_ORDINAL_INDICATOR = '\xA6',
	CP437_MASCULINE_ORDINAL_INDICATOR = '\xA7',
	CP437_INVERTED_QUESTION_MARK = '\xA8',
	CP437_REVERSED_NOT_SIGN = '\xA9',
	CP437_NOT_SIGN = '\xAA',
	CP437_VULGAR_FRACTION_ONE_HALF = '\xAB',
	CP437_VULGAR_FRACTION_ONE_QUARTER = '\xAC',
	CP437_INVERTED_EXCLAMATION_MARK = '\xAD',
	CP437_LEFT_POINTING_DOUBLE_ANGLE_QUOTATION_MARK = '\xAE',
	CP437_RIGHT_POINTING_DOUBLE_ANGLE_QUOTATION_MARK = '\xAF',
	CP437_LIGHT_SHADE = '\xB0',
	CP437_MEDIUM_SHADE = '\xB1',
	CP437_DARK_SHADE = '\xB2',
	CP437_BOX_DRAWINGS_LIGHT_VERTICAL = '\xB3',
	CP437_BOX_DRAWINGS_LIGHT_VERTICAL_AND_LEFT = '\xB4',
	CP437_BOX_DRAWINGS_VERTICAL_SINGLE_AND_LEFT_DOUBLE = '\xB5',
	CP437_BOX_DRAWINGS_VERTICAL_DOUBLE_AND_LEFT_SINGLE = '\xB6',
	CP437_BOX_DRAWINGS_DOUBLE_VERTICAL = '\xBA',
	CP437_BOX_DRAWINGS_VERTICAL_AND_HORIZONTAL = '\xC5',
	// TODO...
	CP437_FULL_BLOCK = '\xDB',
	CP437_LOWER_HALF_BLOCK = '\xDC',
	CP437_LEFT_HALF_BLOCK = '\xDD',
	CP437_RIGHT_HALF_BLOCK = '\xDE',
	CP437_UPPER_HALF_BLOCK = '\xDF',
	// TODO...
	CP437_GREEK_SMALL_LETTER_ALPHA = '\xE0',
	CP437_GREEK_SMALL_LETTER_BETA = '\xE1',
	CP437_LATIN_SMALL_LETTER_SHARP_S = '\xE1',		// Duplicate
	CP437_GREEK_CAPITAL_LETTER_GAMMA = '\xE2',
	CP437_GREEK_SMALL_LETTER_PI = '\xE3',
	CP437_GREEK_CAPITAL_LETTER_SIGMA = '\xE4',
	CP437_GREEK_SMALL_LETTER_SIGMA = '\xE5',
	CP437_GREEK_SMALL_LETTER_MU = '\xE6',
	CP437_MICRO_SIGN = '\xE6',	// Duplicate
	CP437_GREEK_SMALL_LETTER_TAU = '\xE7',
	CP437_GREEK_CAPITAL_LETTER_PHI = '\xE8',
	CP437_GREEK_SMALL_LETTER_THETA = '\xE9',
	CP437_GREEK_CAPITAL_LETTER_OMEGA = '\xEA',
	CP437_GREEK_SMALL_LETTER_DELTA = '\xEB',
	CP437_INFINITY = '\xEC',
	CP437_GREEK_SMALL_LETTER_PHI = '\xED',
	CP437_GREEK_SMALL_LETTER_EPSILION = '\xEE',
	CP437_INTERSECTION = '\xEF',
	CP437_IDENTICAL_TO = '\xF0',
	CP437_TRIPLE_BAR = '\xF0',	// Duplicate
	CP437_PLUS_MINUS_SIGN = '\xF1',
	CP437_GREATER_THAN_OR_EQUAL_TO = '\xF2',
	CP437_LESS_THAN_OR_EQUAL_TO = '\xF3',
	CP437_TOP_HALF_INTEGRAL = '\xF4',
	CP437_BOTTOM_HALF_INTEGRAL = '\xF5',
	CP437_DIVISION_SIGN = '\xF6',
	CP437_ALMOST_EQUAL_TO = '\xF7',
	CP437_DEGREE_SIGN = '\xF8',
	CP437_BULLET_OPERATOR = '\xF9',
	CP437_MIDDLE_DOT = '\xFA',
	CP437_SQUARE_ROOT = '\xFB',
	CP437_CHECK_MARK = '\xFB',	// Duplicate
	CP437_SUPERSCRIPT_LATIN_SMALL_LETTER_N = '\xFC',
	CP437_SUPERSCRIPT_TWO = '\xFD',
	CP437_BLACK_SQUARE = '\xFE',
	CP437_HALFWIDTH_BLACK_SQUARE = '\xFE',	// Duplicate
	CP437_NO_BREAK_SPACE = '\xFF'
};

#endif // Don't add anything after this line
