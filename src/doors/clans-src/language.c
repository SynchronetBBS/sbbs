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

#include "structs.h"
#include "myopen.h"
#include "system.h"
#include "language.h"
#include "video.h"

#ifdef __unix__
#include "unix_wrappers.h"
#endif

struct Language *Language;
extern __BOOL Verbose;

// ------------------------------------------------------------------------- //

void CheckMem(void *Test)
/*
 * Gives system error if the pointer is NULL.
 */
{
	if (Test == NULL) {
		System_Error("Checkmem Failed.  Please send a copy of this screen to\nthe author to help debug the game.\n");
	}
}

// ------------------------------------------------------------------------- //

void LoadStrings(_INT16 StartIndex, _INT16 NumStrings, char *szStrings[])
/*
 * This function loads NumStrings of strings from the language file.
 * starting with string StartIndex.
 *
 * PRE: StartIndex is the first string's index.
 *      NumStrings is the number of strings to load.
 *
 */
{
	_INT16 CurString;

	for (CurString = 0; CurString < NumStrings; CurString++) {
		szStrings[CurString] = &Language->BigString[Language->StrOffsets[StartIndex + CurString]];
	}
}

// ------------------------------------------------------------------------- //

char *MakeStr(_INT16 length)
/*
 * This returns a pointer to a malloc'd string of length length.
 */
{
	char *pc;

	pc = malloc(sizeof(char)*length);
	CheckMem(pc);

	return pc;
}

// ------------------------------------------------------------------------- //

void Language_Init(char *szLangFile)
/*
 * The language file is loaded.  If it isn't found, the system is shut
 * down.
 */
{
	_INT16 iTemp;
	char szString[50];
	struct FileHeader FileHeader;

	if (Verbose) {
		DisplayStr("> Language_Init()\n");
		delay(500);
	}

	/* Read in Language File */
	Language = malloc(sizeof(struct Language));
	CheckMem(Language);
	Language->BigString = NULL;

	for (iTemp = 0; iTemp < 2000; iTemp++)
		Language->StrOffsets[iTemp] = 0;

	MyOpen(szLangFile, "rb", &FileHeader);

	if (!FileHeader.fp) {
		sprintf(szString, "File not found: %s\n", szLangFile);
		zputs(szString);
		System_Close();
	}
	fread(Language, sizeof(struct Language), 1, FileHeader.fp);

	Language->BigString = malloc(Language->NumBytes);
	if (!Language->BigString) {
		DisplayStr("|12Couldn't allocate enough memory to run!");
		fclose(FileHeader.fp);
		System_Close();
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
	free(Language->BigString);
	free(Language);
}
