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
 * Language Initialization and Closure.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "platform.h"

#include "door.h"
#include "language.h"
#include "myopen.h"
#include "structs.h"
#include "system.h"
#include "video.h"

struct Language *Language;

// ------------------------------------------------------------------------- //

void LoadStrings(int16_t StartIndex, int16_t NumStrings, char *szStrings[])
/*
 * This function loads NumStrings of strings from the language file.
 * starting with string StartIndex.
 *
 * PRE: StartIndex is the first string's index.
 *      NumStrings is the number of strings to load.
 *
 */
{
	int16_t CurString;

	for (CurString = 0; CurString < NumStrings; CurString++) {
		szStrings[CurString] = &Language->BigString[Language->StrOffsets[StartIndex + CurString]];
	}
}

// ------------------------------------------------------------------------- //

void Language_Init(char *szLangFile)
/*
 * The language file is loaded.  If it isn't found, the system is shut
 * down.
 */
{
	int16_t iTemp;
	char szString[50];
	struct FileHeader FileHeader;

	if (Verbose) {
		LogDisplayStr("> Language_Init()\n");
		plat_Delay(500);
	}

	/* Read in Language File */
	Language = malloc(sizeof(struct Language));
	CheckMem(Language);
	Language->BigString = NULL;

	for (iTemp = 0; iTemp < MAX_LANG_STRINGS; iTemp++)
		Language->StrOffsets[iTemp] = 0;

	MyOpen(szLangFile, "rb", &FileHeader);

	if (!FileHeader.fp) {
		snprintf(szString, sizeof(szString), "File not found: %s\n", szLangFile);
		System_Error(szString);
	}
	fread(serBuf, BUF_SIZE_Language, 1, FileHeader.fp);
	s_Language_d(serBuf, sizeof(serBuf), Language);

	Language->BigString = malloc(Language->NumBytes);
	if (!Language->BigString) {
		System_Error("|12Couldn't allocate enough memory to run!\n");
	}

	fread(Language->BigString, Language->NumBytes, 1, FileHeader.fp);

	fclose(FileHeader.fp);
}

// ------------------------------------------------------------------------- //

void Language_Close(void)
/*
 * The language file is freed from memory.
 */
{
	if (Language)
		free(Language->BigString);
	free(Language);
}
