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
#include "help.h"
#include "input.h"
#include "language.h"
#include "mstrings.h"
#include "myopen.h"
#include "structs.h"

// ------------------------------------------------------------------------- //


void Help(const char *Topic, char *File)
/*
 * Given the Topic and File, the help is shown.
 */
{
	char Lines[22][256], string[256];
	int16_t Found = false, CurLine, NumLines;
	int32_t MaxBytes;
	bool EndOfTopic = false, Pause = false;
	struct FileHeader FileHeader;


	MyOpen(File, "rt", &FileHeader);
	if (!FileHeader.fp) {
		rputs(ST_NOHELP);
		return;
	}

	/* search for topic */
	while (!Found) {
		MaxBytes = (int32_t)(FileHeader.lEnd - ftell(FileHeader.fp) + EXTRABYTES);
		if (MaxBytes > 254)
			MaxBytes = 254;

		if (fgets(string, MaxBytes, FileHeader.fp) == NULL) {
			fclose(FileHeader.fp);
			rputs(ST_NOHELP);
			rputs("\n");

			return;
		}

		if (string[0] == '^') {
			// get rid of \n
			if (string[ strlen(string) - 1] == '\n')
				string[ strlen(string) - 1] = 0;
			if (string[ strlen(string) - 1] == '\r')
				string[ strlen(string) - 1] = 0;

			/* see if topic is correct */
			if (plat_stricmp(&string[1], Topic) == 0) {
				Found = true;
			}
		}
	}

	while (EndOfTopic == false) {
		/* read in up to 22 lines */
		for (CurLine = 0; CurLine < 22; CurLine++) {
			MaxBytes = (int32_t)(FileHeader.lEnd - ftell(FileHeader.fp) + EXTRABYTES);

			if (MaxBytes > 254)
				MaxBytes = 254;

			if (MaxBytes == 1)
				break;

			fgets(Lines[CurLine], MaxBytes, FileHeader.fp);


			if (Lines[CurLine][0] == '^') {
				// get rid of \n
				if (Lines[CurLine][ strlen(Lines[CurLine]) - 1] == '\n')
					Lines[CurLine][ strlen(Lines[CurLine]) - 1] = 0;
				if (Lines[CurLine][ strlen(Lines[CurLine]) - 1] == '\r')
					Lines[CurLine][ strlen(Lines[CurLine]) - 1] = 0;

				if (plat_stricmp(&Lines[CurLine][1], ST_HELPEND) == 0) {
					EndOfTopic = true;
					break;
				}
				else if (plat_stricmp(&Lines[CurLine][1], ST_HELPPAUSE) == 0) {
					Pause = true;
					break;
				}
			}
		}
		NumLines = CurLine;

		/* display it */
		for (CurLine = 0; CurLine < NumLines; CurLine++) {
			// break on input
			if (GetKeyNoWait()) {
				rputs(ST_ABORTED);
				break;
			}

			rputs(Lines[CurLine]);
		}

		if (Pause) {
			door_pause();
			Pause = false;
		}
		/* else pause normally if 22 lines */
		else if (CurLine == 22 && Door_AllowScreenPause()) {
			rputs(ST_MORE);
			InputCallback();
			if (toupper(GetKey()) == 'N') {
				rputs("\r                       \r");
				break;
			}
			rputs("\r                       \r");
		}
	}

	fclose(FileHeader.fp);
}


// ------------------------------------------------------------------------- //

void GeneralHelp(char *pszFileName)
/*
 * Given the File, all help topics are shown and user can freely choose
 * and read them.
 */
{
	const char *Topics[50];
	char Line[256];
	int16_t NumTopics, cTemp, WhichTopic, iTemp;
	int32_t MaxBytes;
	struct FileHeader HelpFile;
	bool ShowInitially;

	/* read the topics in */
	MyOpen(pszFileName, "rt", &HelpFile);
	if (!HelpFile.fp) {
		rputs(ST_NOHELPFILE);
		return;
	}

	for (iTemp = 0; iTemp < 50; iTemp++)
		Topics[iTemp] = NULL;

	NumTopics = 0;
	for (;;) {
		// see if end of file
		if (ftell(HelpFile.fp) >= HelpFile.lEnd) {
			break;
		}


		// get how many more bytes to read as max
		MaxBytes = (int32_t)(HelpFile.lEnd - ftell(HelpFile.fp) + EXTRABYTES);
		if (MaxBytes > 254)
			MaxBytes = 254;

		if (MaxBytes == 1) {
			break;
		}

		if (fgets(Line, (int16_t) MaxBytes, HelpFile.fp) == NULL) {
			break;
		}


		if (Line[0] == '^') {
			/* get rid of \n */
			if (Line[ strlen(Line) - 1] == '\n')
				Line[ strlen(Line) - 1] = 0;
			if (Line[ strlen(Line) - 1] == '\r')
				Line[ strlen(Line) - 1] = 0;

			if (plat_stricmp(&Line[1], ST_HELPPAUSE) == 0)
				continue; /* skip it, it's a pause */
			else if (plat_stricmp(&Line[1], ST_HELPEND) != 0) {
				Topics[NumTopics] = DupeStr(&Line[1]);
				NumTopics++;
			}
		}
	}

	fclose(HelpFile.fp);

	ShowInitially = true;
	for (;;) {
		// choose which topic
		GetStringChoice(Topics, NumTopics, ST_HELPHINT, &WhichTopic, ShowInitially, DT_WIDE, true);
		ShowInitially = false;

		if (WhichTopic == -1)
			break;

		Help(Topics[WhichTopic], pszFileName);
		rputs("\n");
	}

	/* free memory */
	for (cTemp = 0; cTemp < NumTopics; cTemp++) {
		if (Topics[cTemp])
			free((void*)Topics[cTemp]);
	}
}

// ------------------------------------------------------------------------- //

void MainHelp(void)
/*
 * The main help database (from PreGame menu).
 *
 */
{
	char *szAvailableTopics[17] = {
		"Attributes",
		"Combat",
		"Empire",
		"General Help",
		"Items",
		"Ruler",
		"Newbie Help",
		"Spells",
		"Races and Classes",
		"Strategies",
		"Citizen",
		"Village Wars",
		"World Travel",
		"Army",
		"Wizard Shop",
		"Quit",
		"Redisplay Menu"
	};

	/* choose main topic */
	for (;;)
		switch (GetChoice("Help Menu", "|0GChoose Help Topic|0E> |0F", szAvailableTopics, "ACEGILNPRSTVWYZQ?", 'Q', true)) {
			case 'A' : GeneralHelp(ST_STATSHLP);    break;
			case 'C' : GeneralHelp(ST_COMBATHLP);   break;
			case 'E' : GeneralHelp(ST_EMPIREHLP);   break;
			case 'G' : GeneralHelp(ST_CLANSHLP);    break;
			case 'I' : GeneralHelp(ST_ITEMHLP);     break;
			case 'L' : GeneralHelp(ST_RULERHLP);    break;
			case 'N' : GeneralHelp(ST_NEWBIEHLP);   break;
			case 'P' : GeneralHelp(ST_SPELLHLP);    break;
			case 'R' : GeneralHelp(ST_RACESHLP);    break;
			case 'S' : GeneralHelp(ST_STRATHLP);    break;
			case 'T' : GeneralHelp(ST_CITIZENHLP);  break;
			case 'V' : GeneralHelp(ST_WARHLP);      break;
			case 'W' : GeneralHelp(ST_VILLHLP);     break;
			case 'Y' : GeneralHelp(ST_ARMYHLP);     break;
			case 'Z' : GeneralHelp(ST_WIZHLP);      break;
			case 'Q' : return;
			case '?' : break;
		}
}
