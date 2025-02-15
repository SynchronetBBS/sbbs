/* Synchronet ANSI terminal functions */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright Rob Swindell - http://www.synchro.net/copyright.html			*
 *																			*
 * This program is free software; you can redistribute it and/or			*
 * modify it under the terms of the GNU General Public License				*
 * as published by the Free Software Foundation; either version 2			*
 * of the License, or (at your option) any later version.					*
 * See the GNU General Public License for more details: gpl.txt or			*
 * http://www.fsf.org/copyleft/gpl.html										*
 *																			*
 * For Synchronet coding style and modification guidelines, see				*
 * http://www.synchro.net/source.html										*
 *																			*
 * Note: If this box doesn't appear square, then you need to fix your tabs.	*
 ****************************************************************************/

#include "sbbs.h"

#define TIMEOUT_ANSI_GETXY  5   // Seconds

/* insure str is at least 14 bytes in size! */
extern "C" char* ansi_attr(int atr, int curatr, char* str, bool color)
{
	if (!color) {  /* eliminate colors if terminal doesn't support them */
		if (atr & LIGHTGRAY)       /* if any foreground bits set, set all */
			atr |= LIGHTGRAY;
		if (atr & BG_LIGHTGRAY)  /* if any background bits set, set all */
			atr |= BG_LIGHTGRAY;
		if ((atr & LIGHTGRAY) && (atr & BG_LIGHTGRAY))
			atr &= ~LIGHTGRAY;  /* if background is solid, foreground is black */
		if (!atr)
			atr |= LIGHTGRAY;   /* don't allow black on black */
	}
	if (curatr == atr) { /* text hasn't changed. no sequence needed */
		*str = 0;
		return str;
	}

	strcpy(str, "\033[");
	if ((!(atr & HIGH) && curatr & HIGH) || (!(atr & BLINK) && curatr & BLINK)
	    || atr == LIGHTGRAY) {
		strcat(str, "0;");
		curatr = LIGHTGRAY;
	}
	if (atr & BLINK) {                     /* special attributes */
		if (!(curatr & BLINK))
			strcat(str, "5;");
	}
	if (atr & HIGH) {
		if (!(curatr & HIGH))
			strcat(str, "1;");
	}
	if ((atr & 0x07) != (curatr & 0x07)) {
		switch (atr & 0x07) {
			case BLACK:
				strcat(str, "30;");
				break;
			case RED:
				strcat(str, "31;");
				break;
			case GREEN:
				strcat(str, "32;");
				break;
			case BROWN:
				strcat(str, "33;");
				break;
			case BLUE:
				strcat(str, "34;");
				break;
			case MAGENTA:
				strcat(str, "35;");
				break;
			case CYAN:
				strcat(str, "36;");
				break;
			case LIGHTGRAY:
				strcat(str, "37;");
				break;
		}
	}
	if ((atr & 0x70) != (curatr & 0x70)) {
		switch (atr & 0x70) {
			/* The BG_BLACK macro is 0x200, so isn't in the mask */
			case 0 /* BG_BLACK */:
				strcat(str, "40;");
				break;
			case BG_RED:
				strcat(str, "41;");
				break;
			case BG_GREEN:
				strcat(str, "42;");
				break;
			case BG_BROWN:
				strcat(str, "43;");
				break;
			case BG_BLUE:
				strcat(str, "44;");
				break;
			case BG_MAGENTA:
				strcat(str, "45;");
				break;
			case BG_CYAN:
				strcat(str, "46;");
				break;
			case BG_LIGHTGRAY:
				strcat(str, "47;");
				break;
		}
	}
	if (strlen(str) == 2)  /* Convert <ESC>[ to blank */
		*str = 0;
	else
		str[strlen(str) - 1] = 'm';
	return str;
}
