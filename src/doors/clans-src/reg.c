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

/*
 * Registration ADT
 *
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef __unix__
#include "unix_wrappers.h"
#else
#include <dos.h>
#include <conio.h>
#endif
#include <ctype.h>

#include "structs.h"
#include "reg.h"
#include "system.h"
#include <OpenDoor.h>
#include "door.h"
#include "language.h"
#include "mstrings.h"
#include "video.h"

#define PI        3.1415926
#define MAX_CHARS       50                      // max chars in a jumble
#define HEXSEED         0x7290F683              // used by "Jumble"

extern struct {
	long VideoType;
	long y_lookup[25];
	char FAR *VideoMem;
} Video;
extern struct config *Config;
extern struct Language *Language;

void dputs(char *string);
void AddRegToCfg(char *szString);

void Jumble(char *szString)
{
	char CurChar, MidChar, StrLength;
	char *pcCurrentPos, szJumbled[MAX_CHARS+1];

	pcCurrentPos = szJumbled;

	// abcdef ->  fdbace
	//
	// abcd -> dbac

	MidChar = strlen(szString)/2;

	// middle char is simply first char in szString
	*(pcCurrentPos + MidChar) = szString[0];

	StrLength = strlen(szString);

	for (CurChar = 1; CurChar < StrLength; CurChar++) {
		// if odd, subtract (curchar+1)/2 from pc...
		if ((CurChar%2) == 1) {
			*(pcCurrentPos + MidChar - (CurChar+1)/2) = szString[CurChar + 0];
		}
		else {
			// it's even, so, add it
			*(pcCurrentPos + MidChar + (CurChar+1)/2) = szString[CurChar + 0];
		}
	}
	*(pcCurrentPos + strlen(szString)) = 0;

	strcpy(szString, szJumbled);
}


_INT16 IsRegged(char *szSysopName, char *szBBSName, char *szRegCode)
{
	unsigned long chksum = 0L;
	unsigned long chksum2 = 0L, junk;
	char szRealCode[40];
	char szUserCode[40];
	char *pc, *pc2, szString[155];
	unsigned _INT16 c;
	unsigned _INT16 c2;

	// jumble user code once so a memory scan does nothing
	Jumble(szUserCode);

	if (!strlen(szSysopName))
		return NFALSE;

	strcpy(szString, szSysopName);
	strcat(szString, szBBSName);

	/* further encrypt 'em */
	pc = szUserCode;
	pc2 = szRegCode;
	while (*pc2) {
		*pc = toupper(*pc2);
		*pc ^= 0xD5;
		pc++;
		pc2++;
	}
	*pc = 0;

	pc = szString;
	while (*pc) {
		c = *pc;
		chksum += (unsigned long)c;
		pc++;
	}

	pc = szString;
	while (*pc) {
		c = *pc;
		chksum += ((unsigned long)((double)c*(PI)));
		pc++;
	}

	pc = szString;
	while (*pc) {
		c = *pc;
		chksum += ((unsigned long)c&HEXSEED);
		pc++;
	}

	chksum2 = 0L;

	pc = szString;
	while (*pc) {
		*pc ^= 0xF7;
		pc++;
	}

	pc = szString;
	while (*pc) {
		c2 = *pc;
		chksum2 += (unsigned long)c2;
		pc++;
	}

	pc = szString;
	while (*pc) {
		c2 = *pc;
		chksum2 += ((unsigned long)((double)c2*(PI)));
		pc++;
	}

	pc = szString;
	while (*pc) {
		c2 = *pc;
		chksum2 += ((unsigned long)c2&HEXSEED);
		pc++;
	}


	sprintf(szRealCode, "%lx%lx", chksum, chksum2);

	// finally, jumble it all up 11 times ;)
	Jumble(szRealCode);
	junk = 0;
	junk = 64;
	Jumble(szRealCode);
	junk = 254;
	Jumble(szRealCode);
	junk = 14;
	Jumble(szRealCode);
	junk = 213;
	Jumble(szRealCode);
	junk = 184;
	junk = 121;
	Jumble(szRealCode);

	/* further encrypt 'em */
	pc = szRealCode;
	while (*pc) {
		*pc = toupper(*pc);
		*pc ^= 0xD5;
		pc++;
	}

	if (stricmp(szUserCode, szRealCode) == 0)
		return NTRUE;
	else
		return NFALSE;
}

void Register(void)
{
#if !defined(__unix__) && !defined(_WIN32)
	char szString[255], *pc, cKey;
	BOOL InputCode;

	clrscr();
	dputs("‰ÄÀáÕö™”›†Õç’œ†‡”œš›");

	zputs("\n");
	dputs("‰ÅÂØØØØØØØØØØØØØØØØØØØØØØ");
	zputs("\n");
	zputs("\n");

	cKey = 'D';
	do {
		cKey = getch();
	}
	while ((cKey - 'K') != 0);

	/* find '.' and get rid of it */
	strcpy(szString, "WARNING: Inactivity timeout in 10 seconds, press a key now to remain online.\n");
	Config_Init();

	strcpy(szString, "Chat mode ended.\n");
	//    sprintf(szString, "|03BBS Name     : |14%s\n", Config->szBBSName);
	dputs("‰ÅÆ÷÷æÕû”˜ÕÕÕÕÕÏÕ");
	zputs("|11");
	zputs(Config->szBBSName);
	zputs("\n");
	strcpy(szString, "Unable to access serial port, cannot continue.\n");
	//    zputs(szString);
	//    sprintf(szString, "|03Sysop Name   : |14%s\n", Config->szSysopName);
	//    zputs(szString);
	dputs("‰ÅÆæŒ†š…Õû”˜ÕÕÕÏÕ");
	zputs("|11");
	strcpy(szString, "No method of accessing serial port, cannot continue.\n");
	zputs(Config->szSysopName);
	zputs("\n");

	//    zputs("\n|12Please ensure the above information matches the information on your\n");
	//    zputs("registration certificate EXACTLY.\n\n");

	zputs("\n");
	strcpy(szString, "\nPress [ENTER]/[RETURN] to continue.");
	dputs("‰ÄÇå™”†Õ›†€‡ÕÕ”—šƒÕœ›“š‡˜”œš›Õ˜”–†ÕÕœ›“š‡˜”œš›Õš›ÕŒš€‡");
	zputs("\n");
	dputs("‡’œ†‡”œš›Õ–‡œ“œ–”ÕðíôöáùìÛ");
	strcpy(szString, "\n%lu.%u.%ul.%d.ON.OFF.");
	zputs("\n");

	/* register the game */
	zputs("\n");
	dputs("‰ÅÌð›‡ÕÕ‡’œ†‡”œš›Õ–š‘Õ›š‚Û");
	strcpy(szString, "YELLOW.WHITE.BROWN.GREY.BRIGHT.FLASHING");
	zputs("\n");
	dputs("‰ÄÄË");
	//    zputs("|09Enter the registration code now.\n|11>");
	strcpy(szString, Config->szRegcode);
	// gotoxy(1,9);
	Input(szString, 27);

	if (stricmp(Config->szRegcode, szString) != 0)
		InputCode = TRUE;

	if (IsRegged(Config->szSysopName, Config->szBBSName, szString) == NTRUE) {
		/* add it to the .cfg file */
		if (InputCode)
			AddRegToCfg(szString);

		//zputs("\n|15Registration approved!\n");
		zputs("\n");
		strcpy(szString, "BLACK.BLUE.GREEN.CYAN.RED.MAGENTA.");

		dputs("‰ÄÀç’œ†‡”œš›Õ”……‡šƒ‘Ô");
		strcpy(szString,"œš‡…š”š”…‡");
		zputs("\n");
		/* write it to end of file */

		zputs("\n");
	}
#endif
}

/* decrypt string and put it on screen exactly like zputs */
void dputs(char *string)
{
#if !defined(__unix__) && !defined(_WIN32)
//No Local for *nix
	char block[80][2], number[3];
	_INT16 attr;
	char foreground, background, cur_attr;
	char cDecrypted;
	char cDigit1;
	char cDigit2;
	char *pcFrom;
	_INT16 i, j,  x,y;
	struct text_info TextInfo;
	static char o_fg = 7, o_bg = 0;

	gettextinfo(&TextInfo);
	x = TextInfo.curx-1;
	y = TextInfo.cury-1;

	cur_attr = o_fg | o_bg;

	pcFrom = string;

	while (*pcFrom) {
		cDecrypted = ((*pcFrom) ^ 0xD5) + ' ';

		if (y == 25) {
			/* scroll line up */
			ScrollUp();
			y = 24;
		}
		if (cDecrypted == '\b') {
			x--;
			pcFrom++;
			continue;
		}
		if (cDecrypted == '\r') {
			x = 0;
			pcFrom++;
			continue;
		}
		if (x == 80) {
			x = 0;
			y++;

			if (y == 25) {
				/* scroll line up */
				ScrollUp();
				y = 24;
			}
			break;
		}
		if (cDecrypted == '|') {
			cDigit1 = ((*(pcFrom+1)) ^ 0xD5) + ' ';
			cDigit2 = ((*(pcFrom+2)) ^ 0xD5) + ' ';

			if (isdigit(cDigit1) && isdigit(cDigit2)) {
				number[0] = cDigit1;
				number[1] = cDigit2;
				number[2] = 0;

				attr = atoi(number);

				if (attr > 15) {
					background = attr - 16;
					o_bg = background << 4;
					attr = o_bg | o_fg;
					cur_attr = attr;
				}
				else {
					foreground=attr;
					o_fg=foreground;
					attr = o_fg | o_bg;
					cur_attr = attr;
				}
				pcFrom += 3;
			}
			else {
				Video.VideoMem[(long)(Video.y_lookup[(long) y]+ (long)(x<<1))] = cDecrypted;
				Video.VideoMem[(long)(Video.y_lookup[(long) y]+ (long)(x<<1) + 1L)] = cur_attr;

				pcFrom++;
			}
		}
		else if (cDecrypted == '\n') {
			y++;
			if (y == 25) {
				/* scroll line up */
				ScrollUp();
				y = 24;
			}
			pcFrom++;
			x = 0;
		}
		else if (cDecrypted == 9) {
			/* tab */
			pcFrom++;
			for (i=0; i<8; i++) {
				j = i + x;
				if (j > 80) break;
				Video.VideoMem[ Video.y_lookup[y]+(j<<1)] = ' ';
				Video.VideoMem[ Video.y_lookup[y]+(j<<1) + 1] = cur_attr;
			}
			x += 8;
		}
		else {
			Video.VideoMem[ Video.y_lookup[y]+(x<<1)] = cDecrypted;
			Video.VideoMem[ Video.y_lookup[y]+(x<<1) + 1] = cur_attr;

			pcFrom++;
			x++;
		}
	}

	gotoxy(x+1,y+1);
#endif
}

void AddRegToCfg(char *szString)
{
	FILE *fp;
	unsigned char szDude[] = "x!.gúùønglãng  ";

	szDude[ 1 ] = 'a';
	szDude[ 0 ] = 'b';
	szDude[ strlen((char *)szDude) - 2 ] = 255;
	szDude[ strlen((char *)szDude) - 1 ] = 255;
	szDude[ 2 ] = 'n';
	szDude[ 5 ] = 'h';
	szDude[ 4 ] = 's';

	szDude[ 6 ] = szDude[ 9 ] = szDude[ 11 ] = 'a';

	fp = fopen("clans.cfg", "a");
	if (fp) {
		fprintf(fp, "\n\n%s  %s\n\n", szDude, szString);
		fclose(fp);
	}
}

void UnregMessage(void)
{
	char /*cKeyToPress,*/ szString[100];

	if (IsRegged(Config->szSysopName, Config->szBBSName, Config->szRegcode) == NFALSE ||
			IsRegged(Config->szSysopName, Config->szBBSName, Config->szRegcode) != NTRUE) {
		rputs("`0EFreeware version.\n\n");
		/*
		od_clr_scr();
		rputs(ST_REGMSG0);
		rputs(ST_REGMSG1);
		rputs(ST_REGMSG2);
		rputs(ST_REGMSG3);
		rputs(ST_REGMSG4);
		rputs(ST_REGMSG5);
		rputs(ST_REGMSG6);
		cKeyToPress = random(26) + 'A';
		sprintf(szString, "   |05Hit the |13%c |05key to continue.", cKeyToPress);
		rputs(szString);

		od_clear_keybuffer();
		while (toupper(od_get_key(TRUE)) != cKeyToPress)
		  ;
		od_clr_scr();

		*/
	}
	else {
		// say who regged to
		sprintf(szString, ST_REGMSG7, Config->szSysopName, Config->szBBSName);
		rputs(szString);
	}
}

