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

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <OpenDoor.h>
#include "platform.h"

#include "door.h"
#include "language.h"
#include "mstrings.h"
#include "news.h"
#include "structs.h"
#include "system.h"

void News_AddNews(char *szString)
{
	FILE *fpNewsFile;

	/* open news file */
	fpNewsFile = plat_fsopen("today.asc", "at", PLAT_SH_DENYRW);
	if (fpNewsFile) {
		/* add to it */
		fputs(szString, fpNewsFile);
		fclose(fpNewsFile);
	}
}

void News_ReadNews(bool Today)
{
	int16_t CurLine = 0, NumLines, cTemp;
	FILE *fp;
	char Lines[30][256];

	/* now display it according to the type of file it is */

	if (Today)
		fp = plat_fsopen("today.asc", "r", PLAT_SH_DENYWR);
	else
		fp = plat_fsopen("yest.asc", "r", PLAT_SH_DENYWR);
	if (!fp) {
		rputs("No news to report.\n\n%P");
		return;
	}

	for (;;) {
		/* get SCREEN_LENGTH-4 lines if possible */
		for (cTemp = 0; cTemp < (od_control.user_screen_length-4); cTemp++) {
			fgets(Lines[cTemp], 256, fp);
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
			InputCallback();
			if (toupper(GetKey()) == 'N') {
				rputs("\r                       \r");
				break;
			}
			rputs("\r                       \r");
		}

		/* see if end of file, if so, exit */
		if (feof(fp))
			break;

		/* see if key hit */
		if (GetKeyNoWait())
			break;
	}

	fclose(fp);

	door_pause();

}

void News_CreateTodayNews(void)
{
	/* this initializes the TODAY.ASC file */

	char szString[128];

	snprintf(szString, sizeof(szString), ST_NEWSFOR, System.szTodaysDate);
	News_AddNews(szString);

	/* give other info like increase in cost of etc. */
}
