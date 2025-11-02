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
 * Alliances
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef __unix__
#include <dos.h>
#include <share.h>
#endif
#include "unix_wrappers.h"
#include "win_wrappers.h"

#include "alliance.h"
#include "door.h"
#include "empire.h"
#include "fight.h"
#include "game.h"
#include "help.h"
#include "input.h"
#include "items.h"
#include "mail.h"
#include "menus.h"
#include "parsing.h"
#include "language.h"
#include "mstrings.h"
#include "myopen.h"
#include "system.h"
#include "user.h"

void GetAlliances(struct Alliance *Alliances[MAX_ALLIANCES])
{
	FILE    *fp;
	int     iTemp;

	// init alliances as NULLs
	for (iTemp = 0; iTemp < MAX_ALLIANCES; iTemp++)
		Alliances[iTemp] = NULL;

	fp = fopen("ally.dat", "rb");
	if (fp) {
		for (iTemp = 0; iTemp < MAX_ALLIANCES; iTemp++) {
			Alliances[iTemp] = malloc(sizeof(struct Alliance));
			CheckMem(Alliances[iTemp]);

			notEncryptRead_s(Alliance, Alliances[iTemp], fp, XOR_ALLIES) {
				// no more alliances to read in
				free(Alliances[iTemp]);
				Alliances[iTemp] = NULL;
				break;
			}
		}
		fclose(fp);
	}
}

void DeleteAlliance(int Index, struct Alliance *Alliances[MAX_ALLIANCES])
{
	free(Alliances[Index]);
	Alliances[Index] = NULL;
}

void FreeAlliances(struct Alliance *Alliances[MAX_ALLIANCES])
{
	int iTemp;

	for (iTemp = 0; iTemp < MAX_ALLIANCES; iTemp++) {
		DeleteAlliance(iTemp, Alliances);
	}
}

void UpdateAlliances(struct Alliance *Alliances[MAX_ALLIANCES])
{
	FILE *fp;
	int iTemp;

	fp = fopen("ally.dat", "wb");
	if (fp) {
		for (iTemp = 0; iTemp < MAX_ALLIANCES; iTemp++) {
			if (Alliances[iTemp] == NULL)
				continue;

			EncryptWrite_s(Alliance, Alliances[iTemp], fp, XOR_ALLIES);
		}
		fclose(fp);
	}
}

void KillAlliances(void)
{
	struct Alliance *Alliances[MAX_ALLIANCES];
	char szFileName[16];
	int16_t iTemp;

	GetAlliances(Alliances);

	// delete files
	for (iTemp = 0; iTemp < MAX_ALLIANCES; iTemp++) {
		if (Alliances[iTemp]) {
			snprintf(szFileName, sizeof(szFileName), "hall%02d.txt", Alliances[iTemp]->ID);
			unlink(szFileName);
		}
	}

	// free up mem used by alliances
	FreeAlliances(Alliances);

	// called to destroy ALLY.DAT and remove those pesky HALLxxyy.TXT files
	unlink("ally.dat");
}
