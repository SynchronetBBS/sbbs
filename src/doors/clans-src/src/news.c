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

#include <stdio.h>
#ifdef __unix__
#include "unix_wrappers.h"
#else
#include <share.h>
#endif
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include <OpenDoor.h>
#include "structs.h"
#include "door.h"
#include "language.h"
#include "mstrings.h"

extern struct Language *Language;
extern struct system System;

void News_AddNews(char *szString)
{
	FILE *fpNewsFile;

	/* open news file */

	/* add to it */

	fpNewsFile = _fsopen("today.asc", "at", SH_DENYWR);

	fputs(szString, fpNewsFile);

	fclose(fpNewsFile);
}

void News_ReadNews(BOOL Today)
{
	_INT16 CurLine = 0, NumLines, cTemp;
	FILE *fp;
	char *Lines[30];

	/* now display it according to the type of file it is */

	if (Today)
		fp = _fsopen("today.asc", "r", SH_DENYWR);
	else
		fp = _fsopen("yest.asc", "r", SH_DENYWR);
	if (!fp) {
		rputs("No news to report.\n\n%P");
		return;
	}

	/* init mem */
	for (cTemp = 0; cTemp < 30; cTemp++) {
		Lines[cTemp] = malloc(255);
		CheckMem(Lines[cTemp]);
	}

	for (;;) {
		/* get SCREEN_LENGTH-4 lines if possible */
		for (cTemp = 0; cTemp < (od_control.user_screen_length-4); cTemp++) {
			fgets(Lines[cTemp], 255, fp);
			if (feof(fp))
				break;
		}
		NumLines = cTemp;

		/* display them all */
		for (CurLine = 0; CurLine < NumLines; CurLine++)
			rputs(Lines[CurLine]);

		/* pause if SCREEN_LENGTH-4 lines */
		if (CurLine == (od_control.user_screen_length-4) && Door_AllowScreenPause()) {
			rputs(ST_MORE);
			od_sleep(0);
			if (toupper(od_get_key(TRUE)) == 'N') {
				rputs("\r                       \r");
				break;
			}
			rputs("\r                       \r");

			CurLine = 0;
		}
		else if (Door_AllowScreenPause() == FALSE)
			CurLine = 0;

		/* see if end of file, if so, exit */
		if (feof(fp))
			break;

		/* see if key hit */
		if (od_get_key(FALSE)) break;
	}

	/* de-init mem */
	for (cTemp = 0; cTemp < 30; cTemp++) {
		free(Lines[cTemp]);
		Lines[cTemp] = NULL;
	}

	fclose(fp);

	door_pause();

}

void News_CreateTodayNews(void)
{
	/* this initializes the TODAY.ASC file */

	char szString[128];

	sprintf(szString, ST_NEWSFOR, System.szTodaysDate);
	News_AddNews(szString);

	/* give other info like increase in cost of etc. */
}

