/* Commodore/PET definitions */

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
 * For Synchronet coding style and modification guidelines, see				*
 * http://www.synchro.net/source.html										*
 *																			*
 * Note: If this box doesn't appear square, then you need to fix your tabs.	*
 ****************************************************************************/
#ifndef _MODE7DFS_H_
#define _MODE7DEFS_H_

enum mode7_char {
	/* Colors */
	MODE7_ALPHA_RED			= 129,
	MODE7_ALPHA_GREEN		= 130,
	MODE7_ALPHA_YELLOW		= 131,
	MODE7_ALPHA_BLUE		= 132,
	MODE7_ALPHA_MAGENTA		= 133,
	MODE7_ALPHA_CYAN		= 134,
	MODE7_ALPHA_WHITE		= 135,
	MODE7_FLASH				= 136,
	MODE7_STEADY			= 137,
	MODE7_NORMAL_HEIGHT		= 140,
	MODE7_DOUBLE_HEIGHT 	= 141,
	MODE7_GRAPHIC_RED		= 145,
	MODE7_GRAPHIC_GREEN		= 146,
	MODE7_GRAPHIC_YELLOW 	= 147,
	MODE7_GRAPHIC_BLUE		= 148,
	MODE7_GRAPHIC_MAGENTA 	= 149,
	MODE7_GRAPHIC_CYAN		= 150,
	MODE7_GRAPHIC_WHITE		= 151,
	MODE7_CONCEAL			= 152,
	MODE7_CONTIGUOUS_GFX 	= 153,
	MODE7_SEPARATED_GFX		= 154,
	MODE7_BLACK_BG			= 156,
	MODE7_NEW_BG			= 157,
	MODE7_HOLD_GFX			= 158,
	MODE7_RELEASE_GFX		= 159,
	
	/* Cursor movement */
	MODE7_LEFT        = 8,
	MODE7_RIGHT       = 9,
	MODE7_DOWN        = 10,
	MODE7_UP          = 11,
	MODE7_HOME        = 30,
	MODE7_CLEAR       = 12,
	/* Symbols (which don't align with ASCII) */
	MODE7_BRITPOUND		= 96,
	MODE7_QUARTER		= 123,
	MODE7_HALF			= 92,
	MODE7_THREE_QUARTER	= 195,
	MODE7_DIVIDE		= 196
	/* Replacement chars (missing ASCII chars) */
//	PETSCII_BACKSLASH   = '/',  // the 109 graphics char is an 'M' in shifted/text mode :-(
//	PETSCII_BACKTICK    = 0xAD, // a graphics char actually
//	PETSCII_TILDE       = 0xA8, // a graphics char actually
//	PETSCII_UNDERSCORE  = 0xA4, // a graphics char actually
};

#endif /* Don't add anything after this line */
