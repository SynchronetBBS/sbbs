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
 * Classes/Races ADT
 */

#ifdef __unix__
#include "unix_wrappers.h"
#endif

#include <stdio.h>
#ifdef __FreeBSD__
#include <stdlib.h>
#else
#include <malloc.h>
#endif

#include "structs.h"
#include "myopen.h"
#include "language.h"
#include "video.h"

__BOOL ClassesInitialized = FALSE;

extern struct IniFile IniFile;
struct PClass *PClasses[MAX_PCLASSES], *Races[MAX_PCLASSES];
extern __BOOL Verbose;


// ------------------------------------------------------------------------- //

void Load_PClasses(struct PClass *PClass[MAX_PCLASSES], __BOOL GetPClasses)
/*
 * This function will load classes from file into PClass[].
 *
 * PRE: GetPClasses = TRUE if we're get classes, FALSE if getting races.
 */
{
	int iTemp, CurFile, CurClass = 0, MaxFiles;
	_INT16 NumClasses;
	struct FileHeader ClassFile;

	if (GetPClasses)
		MaxFiles = MAX_CLASSFILES;
	else
		MaxFiles = MAX_RACEFILES;

	// for each file, read in the data
	for (CurFile = 0; CurFile < MaxFiles; CurFile++) {
		if (GetPClasses && IniFile.pszClasses[CurFile] == NULL)
			break;
		else if (!GetPClasses && IniFile.pszRaces[CurFile] == NULL)
			break;

		// open file if possible
		if (GetPClasses)
			MyOpen(IniFile.pszClasses[CurFile], "rb", &ClassFile);
		else
			MyOpen(IniFile.pszRaces[CurFile], "rb", &ClassFile);

		if (ClassFile.fp == NULL) continue;

		// read in data

		/* get num classes */
		fread(&NumClasses, sizeof(NumClasses), 1, ClassFile.fp);

		/* read them in */
		for (iTemp = 0; iTemp < NumClasses; iTemp++) {
			PClass[CurClass] = malloc(sizeof(struct PClass));
			CheckMem(PClass[CurClass]);

			fread(PClass[CurClass], sizeof(struct PClass), 1, ClassFile.fp);

			//printf("%s\n\r", PClass[CurClass]->szName);

			CurClass++;
			if (CurClass == MAX_PCLASSES) break;
		}
		fclose(ClassFile.fp);

		if (CurClass == MAX_PCLASSES) break;
	}
}

void Free_PClasses(struct PClass *PClass[MAX_PCLASSES])
/*
 * This function will free the classes loaded by Load_PClasses
 */
{
	int iTemp;

	for (iTemp = 0; iTemp < MAX_PCLASSES; iTemp++) {
		if (PClass[iTemp]) {
			free(PClass[iTemp]);
			PClass[iTemp] = NULL;
		}
	}
}


// ------------------------------------------------------------------------- //

void PClass_Init(void)
/*
 * Initialize classes and races.
 *
 */
{
	if (Verbose) {
		DisplayStr("> PClass_Init()\n");
		delay(500);
	}

	Load_PClasses(Races, FALSE);
	Load_PClasses(PClasses, TRUE);

	ClassesInitialized = TRUE;
}

void PClass_Close(void)
/*
 * DeInitializes classes and races.
 *
 */
{
	if (ClassesInitialized == FALSE) return;

	Free_PClasses(PClasses);
	Free_PClasses(Races);

	ClassesInitialized = FALSE;
}
