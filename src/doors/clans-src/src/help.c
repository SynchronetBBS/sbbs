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
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#ifdef __unix__
#include "unix_wrappers.h"
#include <sys/mman.h>
#include <termios.h>
#endif


#include <OpenDoor.h>
#include "structs.h"
#include "language.h"
#include "mstrings.h"
#include "input.h"
#include "myopen.h"
#include "door.h"

extern struct Language *Language;

// ------------------------------------------------------------------------- //


void Help(char *Topic, char *File)
/*
 * Given the Topic and File, the help is shown.
 */
{
	char *Lines[22], *string;
	_INT16 cTemp, Found = FALSE, CurLine, NumLines;
	long MaxBytes;
	BOOL EndOfTopic = FALSE, Pause = FALSE;
	struct FileHeader FileHeader;


	// fp = fopen(File, "r");
	MyOpen(File, "rt", &FileHeader);
	if (!FileHeader.fp) {
		rputs(ST_NOHELP);
		return;
	}

	for (cTemp = 0; cTemp < 22; cTemp++) {
		Lines[cTemp] = malloc(255);
		CheckMem(Lines[cTemp]);
	}

	string = MakeStr(255);

	/* search for topic */
	while (!Found) {
		MaxBytes = FileHeader.lEnd - ftell(FileHeader.fp) + EXTRABYTES;
		if (MaxBytes > 254)
			MaxBytes = 254;

		if (fgets(string, (_INT16) MaxBytes, FileHeader.fp) == NULL) {
			fclose(FileHeader.fp);
			rputs(ST_NOHELP);
			rputs("\n");

			for (cTemp = 0; cTemp < 22; cTemp++)
				free(Lines[cTemp]);
			free(string);
			return;
		}

		if (string[0] == '^') {
			// get rid of \n
			if (string[ strlen(string) - 1] == '\n')
				string[ strlen(string) - 1] = 0;
			if (string[ strlen(string) - 1] == '\r')
				string[ strlen(string) - 1] = 0;

			/* see if topic is correct */
			// if (strspn(&string[1], Topic) == strlen(Topic))
			if (stricmp(&string[1], Topic) == 0) {
				Found = TRUE;
			}
		}
	}

	while (EndOfTopic == FALSE) {
		/* read in up to 22 lines */
		for (CurLine = 0; CurLine < 22; CurLine++) {
			MaxBytes = FileHeader.lEnd - ftell(FileHeader.fp) + EXTRABYTES;

			if (MaxBytes > 254)
				MaxBytes = 254;

			if (MaxBytes == 1)
				break;

			fgets(Lines[CurLine], (_INT16)MaxBytes, FileHeader.fp);

			// Lines[CurLine][ MaxBytes - EXTRABYTES + 1] = 0;

			if (Lines[CurLine][0] == '^') {
				// get rid of \n
				if (Lines[CurLine][ strlen(Lines[CurLine]) - 1] == '\n')
					Lines[CurLine][ strlen(Lines[CurLine]) - 1] = 0;
				if (Lines[CurLine][ strlen(Lines[CurLine]) - 1] == '\r')
					Lines[CurLine][ strlen(Lines[CurLine]) - 1] = 0;

				if (stricmp(&Lines[CurLine][1], ST_HELPEND) == 0) {
					EndOfTopic = TRUE;
					break;
				}
				else if (stricmp(&Lines[CurLine][1], ST_HELPPAUSE) == 0) {
					Pause = TRUE;
					break;
				}
			}
		}
		NumLines = CurLine;

		/* display it */
		for (CurLine = 0; CurLine < NumLines; CurLine++) {
			// break on input
			if (od_get_key(FALSE)) {
				rputs(ST_ABORTED);
				break;
			}

			rputs(Lines[CurLine]);
		}

		if (Pause) {
			door_pause();
			Pause = FALSE;
		}
		/* else pause normally if 22 lines */
		else if (CurLine == 22 && Door_AllowScreenPause()) {
			rputs(ST_MORE);
			od_sleep(0);
			if (toupper(od_get_key(TRUE)) == 'N') {
				rputs("\r                       \r");
				break;
			}
			rputs("\r                       \r");

			CurLine = 0;
		}
	}

	fclose(FileHeader.fp);

	for (cTemp = 0; cTemp < 22; cTemp++)
		free(Lines[cTemp]);
	free(string);
}


// ------------------------------------------------------------------------- //

void GeneralHelp(char *pszFileName)
/*
 * Given the File, all help topics are shown and user can freely choose
 * and read them.
 */
{
	char *Topics[50], *Line;
	_INT16 NumTopics, cTemp, WhichTopic, iTemp;
	long MaxBytes;
	struct FileHeader HelpFile;
	__BOOL ShowInitially;

	/* read the topics in */
	// fpHelpFile = fopen(pszFileName, "rt");
	MyOpen(pszFileName, "rt", &HelpFile);
	if (!HelpFile.fp) {
		rputs(ST_NOHELPFILE);
		return;
	}

	Line = MakeStr(255);

	for (iTemp = 0; iTemp < 50; iTemp++)
		Topics[iTemp] = NULL;

	NumTopics = 0;
	for (;;) {
		// see if end of file
		if (ftell(HelpFile.fp) >= HelpFile.lEnd) {
			break;
		}

		// od_printf("%ld  %ld\n", ftell(HelpFile.fp), HelpFile.lEnd);

		// get how many more bytes to read as max
		MaxBytes = HelpFile.lEnd - ftell(HelpFile.fp) + EXTRABYTES;
		if (MaxBytes > 254)
			MaxBytes = 254;

		if (MaxBytes == 1) {
			break;
		}

		if (fgets(Line, (_INT16) MaxBytes, HelpFile.fp) == NULL) {
			break;
		}

		// Line[ MaxBytes - EXTRABYTES + 1] = 0;

		if (Line[0] == '^') {
			/* get rid of \n */
			if (Line[ strlen(Line) - 1] == '\n')
				Line[ strlen(Line) - 1] = 0;
			if (Line[ strlen(Line) - 1] == '\r')
				Line[ strlen(Line) - 1] = 0;

			if (stricmp(&Line[1], ST_HELPPAUSE) == 0)
				continue; /* skip it, it's a pause */
			else if (stricmp(&Line[1], ST_HELPEND) != 0) {
				Topics[NumTopics] = MakeStr(strlen(&Line[1]) + 1);
				strcpy(Topics[NumTopics], &Line[1]);
				NumTopics++;
			}
		}
	}

	fclose(HelpFile.fp);

	ShowInitially = TRUE;
	for (;;) {
		// choose which topic
		GetStringChoice(Topics, NumTopics, ST_HELPHINT, &WhichTopic, ShowInitially, DT_WIDE, TRUE);
		ShowInitially = FALSE;

		if (WhichTopic == -1)
			break;

		Help(Topics[WhichTopic], pszFileName);
		rputs("\n");
	}

	/* free memory */
	for (cTemp = 0; cTemp < NumTopics; cTemp++) {
		if (Topics[cTemp])
			free(Topics[cTemp]);
	}
	free(Line);
}

// ------------------------------------------------------------------------- //

void MainHelp(void)
/*
 * The main help database (from PreGame menu).
 *
 */
{
	char *szAvailableTopics[9] = {
		"Items",
		"Races and Classes",
		"General Help",
		"Combat",
		"Strategy",
		"Newbie Help",
		"Stats Help",
		"Quit",
		"Redisplay Menu"
	};
	char *szFiles[7];
	_INT16 iTemp;

	szFiles[0] = ST_ITEMHLP;
	szFiles[1] = ST_RACESHLP;
	szFiles[2] = ST_CLANSHLP;
	szFiles[3] = ST_COMBATHLP;
	szFiles[4] = ST_STRATHLP;
	szFiles[5] = ST_NEWBIEHLP;
	szFiles[6] = ST_STATSHLP;

	/* choose main topic */
	for (;;)
		switch (iTemp = GetChoice("Help Menu", "|0GChoose Help Topic|0E> |0F", szAvailableTopics, "1234567Q?", 'Q', TRUE)) {
			case '1' :
			case '2' :
			case '3' :
			case '4' :
			case '5' :
			case '6' :
			case '7' :
				GeneralHelp(szFiles[ iTemp - '1' ]);
				break;
			case 'Q' :
				return;
			case '?' :
				break;
		}
}
