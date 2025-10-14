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
 * CLANS.INI stuff
 */

#include <stdio.h>
#ifndef __FreeBSD__
#include <malloc.h>
#endif
#ifdef __unix__
#include "unix_wrappers.h"
#else
#include <share.h>
#endif
#include <string.h>

#include "structs.h"
#include "language.h"
#include "system.h"
#include "parsing.h"
#include "k_clansi.h"

struct IniFile IniFile = { FALSE };
extern __BOOL Verbose;

void DisplayStr(char *);

void ClansIni_Init(void)
{
	/* read in quests file and allocate memory for quest names */
	int iTemp;
	FILE *fp;
	char szLine[128], *pcCurrentPos;
	char szToken[MAX_TOKEN_CHARS + 1];
	int iKeyWord;
	int CurNPCFile, CurItemFile, CurRaceFile, CurClassFile, CurSpellFile;

	if (Verbose) {
		DisplayStr("> ClansIni_Init()\n");
		delay(500);
	}


	IniFile.Initialized = FALSE;

	for (iTemp = 0; iTemp < MAX_NPCFILES; iTemp++)
		IniFile.pszNPCFileName[iTemp] = NULL;
	for (iTemp = 0; iTemp < MAX_SPELLFILES; iTemp++)
		IniFile.pszSpells[iTemp] = NULL;
	for (iTemp = 0; iTemp < MAX_ITEMFILES; iTemp++)
		IniFile.pszItems[iTemp] = NULL;
	for (iTemp = 0; iTemp < MAX_RACEFILES; iTemp++)
		IniFile.pszRaces[iTemp] = NULL;
	for (iTemp = 0; iTemp < MAX_CLASSFILES; iTemp++)
		IniFile.pszClasses[iTemp] = NULL;
	for (iTemp = 0; iTemp < MAX_VILLFILES; iTemp++)
		IniFile.pszVillages[iTemp] = NULL;

	IniFile.pszLanguage = NULL;

	fp = _fsopen("clans.ini", "r", SH_DENYWR);
	if (!fp) {
		System_Error("No clans.ini\n");
	}

	/* read in all lines and get event names */
	CurNPCFile = -1;
	CurItemFile = -1;
	CurRaceFile = -1;
	CurClassFile = -1;
	CurSpellFile = -1;
	for (;;) {
		/* read in a line */
		if (fgets(szLine, 128, fp) == NULL) break;

		/* Ignore all of line after comments or CR/LF char */
		pcCurrentPos=(char *)szLine;

		ParseLine(pcCurrentPos);

		/* If no token was found, proceed to process the next line */
		if (!*pcCurrentPos) continue;

		GetToken(pcCurrentPos, szToken);

		if (szToken[0] == '$')
			break;

		/* Loop through list of keywords */
		for (iKeyWord = 0; iKeyWord < MAX_INI_WORDS; ++iKeyWord) {
			/* If keyword matches */
			if (stricmp(szToken, papszIniKeyWords[iKeyWord]) == 0) {
				/* Process config token */
				switch (iKeyWord) {
					case 0 :  /* npcfile */
						if (CurNPCFile == MAX_NPCFILES)
							break;

						CurNPCFile++;

						IniFile.pszNPCFileName[ CurNPCFile ] =
							MakeStr(strlen(pcCurrentPos) + 1);
						strcpy(IniFile.pszNPCFileName[ CurNPCFile ], pcCurrentPos);
						break;
					case 1 :  /* Language */
						if (IniFile.pszLanguage) {
							free(IniFile.pszLanguage);
							IniFile.pszLanguage = NULL;
						}
						IniFile.pszLanguage =
							MakeStr(strlen(pcCurrentPos) + 1);
						strcpy(IniFile.pszLanguage, pcCurrentPos);
						break;
					case 2 :  // item files
						if (CurItemFile == MAX_ITEMFILES)
							break;

						CurItemFile++;

						IniFile.pszItems[ CurItemFile ] =
							MakeStr(strlen(pcCurrentPos) + 1);
						strcpy(IniFile.pszItems[ CurItemFile ], pcCurrentPos);

						// printf("itemfile = %d. %s\n", CurItemFile,
						//    IniFile.pszItems[ CurItemFile ]);
						break;
					case 3 :  // races files
						if (CurRaceFile == MAX_RACEFILES)
							break;

						CurRaceFile++;

						IniFile.pszRaces[ CurRaceFile ] =
							MakeStr(strlen(pcCurrentPos) + 1);
						strcpy(IniFile.pszRaces[ CurRaceFile ], pcCurrentPos);

						// printf("race = %d. %s\n", CurRaceFile,
						//     IniFile.pszRaces[ CurRaceFile ]);
						break;
					case 4 :  // class files
						if (CurClassFile == MAX_CLASSFILES)
							break;

						CurClassFile++;

						IniFile.pszClasses[ CurClassFile ] =
							MakeStr(strlen(pcCurrentPos) + 1);
						strcpy(IniFile.pszClasses[ CurClassFile ], pcCurrentPos);

						// printf("class = %d. %s\n", CurClassFile,
						//     IniFile.pszClasses[ CurClassFile ]);
						break;
					case 5 :  // spell files
						if (CurSpellFile == MAX_SPELLFILES)
							break;

						CurSpellFile++;

						IniFile.pszSpells[ CurSpellFile ] =
							MakeStr(strlen(pcCurrentPos) + 1);
						strcpy(IniFile.pszSpells[ CurSpellFile ], pcCurrentPos);

						// printf("spellfile = %d. %s\n", CurSpellFile,
						//     IniFile.pszSpells[ CurSpellFile ]);
						break;
				}
			}
		}
	}

	fclose(fp);
}

void ClansIni_Close(void)
{
	int iTemp;

	if (IniFile.Initialized == FALSE) return;

	if (IniFile.pszLanguage)
		free(IniFile.pszLanguage);

	// NPC filenames
	for (iTemp = 0; iTemp < MAX_NPCFILES; iTemp++)
		if (IniFile.pszNPCFileName[iTemp])
			free(IniFile.pszNPCFileName[iTemp]);

	// spell filenames
	for (iTemp = 0; iTemp < MAX_SPELLFILES; iTemp++)
		if (IniFile.pszSpells[iTemp])
			free(IniFile.pszSpells[iTemp]);

	for (iTemp = 0; iTemp < MAX_ITEMFILES; iTemp++)
		if (IniFile.pszItems[iTemp])
			free(IniFile.pszItems[iTemp]);

	for (iTemp = 0; iTemp < MAX_RACEFILES; iTemp++)
		if (IniFile.pszRaces[iTemp])
			free(IniFile.pszRaces[iTemp]);

	for (iTemp = 0; iTemp < MAX_CLASSFILES; iTemp++)
		if (IniFile.pszClasses[iTemp])
			free(IniFile.pszClasses[iTemp]);

	for (iTemp = 0; iTemp < MAX_VILLFILES; iTemp++)
		if (IniFile.pszVillages[iTemp])
			free(IniFile.pszVillages[iTemp]);

	IniFile.Initialized = FALSE;
}
