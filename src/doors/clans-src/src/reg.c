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

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <OpenDoor.h>
#include "platform.h"

#include "door.h"
#include "language.h"
#include "mstrings.h"
#include "readcfg.h"
#include "reg.h"
#include "structs.h"
#include "system.h"
#include "video.h"

#define PI        3.1415926
#define MAX_CHARS       50                      // max chars in a jumble
#define HEXSEED         0x7290F683              // used by "Jumble"

extern struct {
	int32_t VideoType;
	int32_t y_lookup[25];
	char FAR *VideoMem;
} Video;

#if !defined(__unix__) && !defined(_WIN32)
static void dputs(char *string);
static void AddRegToCfg(char *szString);
#endif

static void Jumble(char *szString, size_t n)
{
	char CurChar, MidChar, StrLength;
	char *pcCurrentPos, szJumbled[MAX_CHARS+1];

	pcCurrentPos = szJumbled;

	// abcdef ->  fdbace
	//
	// abcd -> dbac

	MidChar = (char)(strlen(szString)/2);

	// middle char is simply first char in szString
	*(pcCurrentPos + MidChar) = szString[0];

	StrLength = (char)(strlen(szString));

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

	strlcpy(szString, szJumbled, n);
}


int16_t IsRegged(char *szSysopName, char *szBBSName, char *szRegCode)
{
	uint32_t chksum = 0;
	uint32_t chksum2 = 0;
	char szRealCode[40];
	char szUserCode[40] = {0};
	char *pc, *pc2, szString[155];
	uint16_t c;
	uint16_t c2;

	// jumble user code once so a memory scan does nothing
	Jumble(szUserCode, sizeof(szUserCode));

	if (!strlen(szSysopName))
		return NFALSE;

	strlcpy(szString, szSysopName, sizeof(szString));
	strlcat(szString, szBBSName, sizeof(szString));

	/* further encrypt 'em */
	pc = szUserCode;
	pc2 = szRegCode;
	while (*pc2) {
		*pc = (char)(toupper(*pc2));
		*pc ^= (char)0xD5;
		pc++;
		pc2++;
	}
	*pc = 0;

	pc = szString;
	while (*pc) {
		c = (uint16_t)*pc;
		chksum += (uint32_t)c;
		pc++;
	}

	pc = szString;
	while (*pc) {
		c = (uint16_t)*pc;
		chksum += ((uint32_t)((double)c*(PI)));
		pc++;
	}

	pc = szString;
	while (*pc) {
		c = (uint16_t)*pc;
		chksum += ((uint32_t)c&HEXSEED);
		pc++;
	}

	chksum2 = 0;

	pc = szString;
	while (*pc) {
		*pc ^= (char)0xF7;
		pc++;
	}

	pc = szString;
	while (*pc) {
		c2 = (uint16_t)*pc;
		chksum2 += (uint32_t)c2;
		pc++;
	}

	pc = szString;
	while (*pc) {
		c2 = (uint16_t)*pc;
		chksum2 += ((uint32_t)((double)c2*(PI)));
		pc++;
	}

	pc = szString;
	while (*pc) {
		c2 = (uint16_t)*pc;
		chksum2 += ((uint32_t)c2&HEXSEED);
		pc++;
	}


	snprintf(szRealCode, sizeof(szRealCode), "%" PRIx32 "%" PRIx32, chksum, chksum2);

	// finally, jumble it all up 11 times ;)
	Jumble(szRealCode, sizeof(szRealCode));
	Jumble(szRealCode, sizeof(szRealCode));
	Jumble(szRealCode, sizeof(szRealCode));
	Jumble(szRealCode, sizeof(szRealCode));
	Jumble(szRealCode, sizeof(szRealCode));
	Jumble(szRealCode, sizeof(szRealCode));

	/* further encrypt 'em */
	pc = szRealCode;
	while (*pc) {
		*pc = (char)toupper(*pc);
		*pc ^= (char)0xD5;
		pc++;
	}

	if (strcasecmp(szUserCode, szRealCode) == 0)
		return NTRUE;
	else
		return NFALSE;
}

void Register(void)
{
#if !defined(__unix__) && !defined(_WIN32)
	char szString[255], *pc, cKey;
	bool InputCode;

	clrscr();
	dputs("\x89\xc4\xc0\xe1\x9d\x90\xd5\xf6\x99\x94\x9b\x86\xd5\xe7\x90\x92\x9c\x86\x81\x87\x94\x81\x9c\x9a\x9b");

	zputs("\n");
	dputs("\x89\xc5\xc2\xd8\xd8\xd8\xd8\xd8\xd8\xd8\xd8\xd8\xd8\xd8\xd8\xd8\xd8\xd8\xd8\xd8\xd8\xd8\xd8\xd8\xd8");
	zputs("\n");
	zputs("\n");

	cKey = 'D';
	do {
		cKey = getch();
	}
	while ((cKey - 'K') != 0);

	/* find '.' and get rid of it */
	strlcpy(szString, "WARNING: Inactivity timeout in 10 seconds, press a key now to remain online.\n", sizeof(szString));
	Config_Init();

	strlcpy(szString, "Chat mode ended.\n", sizeof(szString));
	dputs("\x89\xc5\xc6\xf7\xf7\xe6\xd5\xfb\x94\x98\x90\xd5\xd5\xd5\xd5\xd5\xcf\xd5");
	zputs("|11");
	zputs(Config.szBBSName);
	zputs("\n");
	strlcpy(szString, "Unable to access serial port, cannot continue.\n", sizeof(szString));
	dputs("\x89\xc5\xc6\xe6\x8c\x86\x9a\x85\xd5\xfb\x94\x98\x90\xd5\xd5\xd5\xcf\xd5");
	zputs("|11");
	strlcpy(szString, "No method of accessing serial port, cannot continue.\n", sizeof(szString));
	zputs(Config.szSysopName);
	zputs("\n");

	//    zputs("\n|12Please ensure the above information matches the information on your\n");
	//    zputs("registration certificate EXACTLY.\n\n");

	zputs("\n");
	strlcpy(szString, "\nPress [ENTER]/[RETURN] to continue.", sizeof(szString));
	dputs("\x89\xc4\xc7\xe5\x99\x90\x94\x86\x90\xd5\x90\x9b\x86\x80\x87\x90\xd5\x81\x9d\x90\xd5\x94\x97\x9a\x83\x90\xd5\x9c\x9b\x93\x9a\x87\x98\x94\x81\x9c\x9a\x9b\xd5\x98\x94\x81\x96\x9d\x90\x86\xd5\x81\x9d\x90\xd5\x9c\x9b\x93\x9a\x87\x98\x94\x81\x9c\x9a\x9b\xd5\x9a\x9b\xd5\x8c\x9a\x80\x87");
	zputs("\n");
	dputs("\x87\x90\x92\x9c\x86\x81\x87\x94\x81\x9c\x9a\x9b\xd5\x96\x90\x87\x81\x9c\x93\x9c\x96\x94\x81\x90\xd5\xf0\xed\xf4\xf6\xe1\xf9\xec\xdb");
	strlcpy(szString, "\n%lu.%u.%ul.%d.ON.OFF.", sizeof(szString));
	zputs("\n");

	/* register the game */
	zputs("\n");
	dputs("\x89\xc5\xcc\xf0\x9b\x81\x90\x87\xd5\x81\x9d\x90\xd5\x87\x90\x92\x9c\x86\x81\x87\x94\x81\x9c\x9a\x9b\xd5\x96\x9a\x91\x90\xd5\x9b\x9a\x82\xdb");
	strlcpy(szString, "YELLOW.WHITE.BROWN.GREY.BRIGHT.FLASHING", sizeof(szString));
	zputs("\n");
	dputs("\x89\xc4\xc4\xcb");
	//    zputs("|09Enter the registration code now.\n|11>");
	strlcpy(szString, Config.szRegcode, sizeof(szString));
	// gotoxy(1,9);
	Input(szString, 27);

	if (strcasecmp(Config.szRegcode, szString) != 0)
		InputCode = true;

	if (IsRegged(Config.szSysopName, Config.szBBSName, szString) == NTRUE) {
		/* add it to the .cfg file */
		if (InputCode)
			AddRegToCfg(szString);

		//zputs("\n|15Registration approved!\n");
		zputs("\n");
		strlcpy(szString, "BLACK.BLUE.GREEN.CYAN.RED.MAGENTA.", sizeof(szString));

		dputs("\x89\xc4\xc0\xe7\x90\x92\x9c\x86\x81\x87\x94\x81\x9c\x9a\x9b\xd5\x94\x85\x85\x87\x9a\x83\x90\x91\xd4");
		strlcpy(szString, "\x81\x9c\x9a\x87\x85\x9a\x94\x9a\x94\x85\x81\x87", sizeof(szString));
		zputs("\n");
		/* write it to end of file */

		zputs("\n");
	}
#endif
}

#if !defined(__unix__) && !defined(_WIN32)
/* decrypt string and put it on screen exactly like zputs */
static void dputs(char *string)
{
//No Local for *nix
	char block[80][2], number[3];
	int16_t attr;
	char foreground, background, cur_attr;
	char cDecrypted;
	char cDigit1;
	char cDigit2;
	char *pcFrom;
	int16_t i, j,  x,y;
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
				Video.VideoMem[(int32_t)(Video.y_lookup[(int32_t) y]+ (int32_t)(x<<1))] = cDecrypted;
				Video.VideoMem[(int32_t)(Video.y_lookup[(int32_t) y]+ (int32_t)(x<<1) + 1)] = cur_attr;

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
}

static void AddRegToCfg(char *szString)
{
	FILE *fp;
	unsigned char szDude[] = "x!.g\xfa\xf9\xf8ng\x7fl\xe3ng  ";

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
#endif

void UnregMessage(void)
{
	char /*cKeyToPress,*/ szString[100];

	if (IsRegged(Config.szSysopName, Config.szBBSName, Config.szRegcode) == NFALSE ||
			IsRegged(Config.szSysopName, Config.szBBSName, Config.szRegcode) != NTRUE) {
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
		cKeyToPress = my_random(26) + 'A';
		snprintf(szString, sizeof(szString), "   |05Hit the |13%c |05key to continue.", cKeyToPress);
		rputs(szString);

		od_clear_keybuffer();
		while (toupper(od_get_key(true)) != cKeyToPress)
		  ;
		od_clr_scr();

		*/
	}
	else {
		// say who regged to
		snprintf(szString, sizeof(szString), ST_REGMSG7, Config.szSysopName, Config.szBBSName);
		rputs(szString);
	}
}

