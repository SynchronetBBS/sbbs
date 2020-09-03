/* $Id: petdefs.h,v 1.4 2019/07/09 23:54:43 rswindell Exp $ */

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
#ifndef _PETDEFS_H_
#define _PETDEFS_H_

enum petscii_char {
	/* Colors */
	PETSCII_BLACK		= 144,
	PETSCII_WHITE		= 5,
	PETSCII_RED			= 28,
	PETSCII_GREEN		= 30,
	PETSCII_BLUE		= 31,
	PETSCII_ORANGE		= 129,		// Dark purple in 80 column mode
	PETSCII_BROWN		= 149,		// Dark yellow in 80 column mode
	PETSCII_YELLOW		= 158,
	PETSCII_CYAN		= 159,		// Light cyan in 80 column mode
	PETSCII_LIGHTRED 	= 150,
	PETSCII_DARKGRAY	= 151,		// Dark cyan in 80 column mode
	PETSCII_MEDIUMGRAY	= 152,
	PETSCII_LIGHTGREEN	= 153,
	PETSCII_LIGHTBLUE	= 154,
	PETSCII_LIGHTGRAY	= 155,
	PETSCII_PURPLE		= 156,
	/* Char-set */
	PETSCII_UPPERLOWER	= 14,
	PETSCII_UPPERGRFX	= 142,
	/* Attributes */
	PETSCII_FLASH_ON	= 15,
	PETSCII_FLASH_OFF	= 143,
	PETSCII_REVERSE_ON	= 18,
	PETSCII_REVERSE_OFF	= 146,
	/* Cursor movement */
	PETSCII_UP			= 145,
	PETSCII_DOWN		= 17,
	PETSCII_LEFT		= 157,
	PETSCII_RIGHT		= 29,
	PETSCII_HOME		= 19,
	PETSCII_CLEAR		= 147,
	PETSCII_INSERT		= 148,
	PETSCII_DELETE		= 20,
	PETSCII_CRLF		= 141,
	/* Symbols (which don't align with ASCII) */
	PETSCII_BRITPOUND	= 92,
	/* Graphic chars */
	PETSCII_UPARROW		= 0x5E,
	PETSCII_LEFTARROW	= 0x5f,
	PETSCII_LIGHTHASH	= 0xA6,
	PETSCII_MEDIUMHASH	= 0xDE,
	PETSCII_HEAVYHASH	= 0xA9,
	PETSCII_SOLID		= 0xA0,	// Actually inversed solid (empty)
	PETSCII_LEFTHALF	= 0xA1,
	PETSCII_RIGHTHALF	= 0xB6,	// Not quite a full half
	PETSCII_TOPHALF		= 0xB8,	// Not quite a full half
	PETSCII_BOTTOMHALF	= 0xA2,
	PETSCII_CHECKMARK	= 0xBA,
	PETSCII_CROSS		= 0xDB,
	PETSCII_HORZLINE	= 0xC0,
	PETSCII_VERTLINE	= 0xDD,
	PETSCII_LWRRHTBOX	= 0xAC,
	PETSCII_LWRLFTBOX	= 0xBB,
	PETSCII_UPRRHTBOX	= 0xBC,
	PETSCII_UPRLFTBOX	= 0xBE,
	PETSCII_CHECKERBRD	= 0xBF,
	/* Replacement chars (missing ASCII chars) */
	PETSCII_BACKSLASH	= '/',	// the 109 graphics char is an 'M' in shifted/text mode :-(
	PETSCII_BACKTICK	= 0xAD,	// a graphics char actually
	PETSCII_TILDE		= 0xA8,	// a graphics char actually
	PETSCII_UNDERSCORE	= 0xA4,	// a graphics char actually
};

#endif /* Don't add anything after this line */
