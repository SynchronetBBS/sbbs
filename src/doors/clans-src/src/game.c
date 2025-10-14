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
 * Game ADT (stores game instance data)
 *
 */


#include <stdio.h>

#ifdef __unix__
#include "unix_wrappers.h"
#else
#include <share.h>
#endif
#ifdef __FreeBSD__
#include <stdlib.h>
#else
#include <malloc.h>
#endif

#include "system.h"
#include "structs.h"
#include "language.h"
#include "mstrings.h"
#include "misc.h"
#include "crc.h"
#include "myopen.h"
#include "news.h"
#include "door.h"
#include "video.h"

struct game Game = { FALSE, NULL };
extern struct system System;
extern struct Language *Language;
extern __BOOL Verbose;

// ------------------------------------------------------------------------- //

__BOOL Game_Read(void)
/*
 * Reads in the GAME.DAT file.  If not found, returns FALSE.
 */
{
	FILE *fp;

	fp = _fsopen("game.dat", "rb", SH_DENYWR);
	if (!fp)  return FALSE;

	EncryptRead(Game.Data, sizeof(struct game_data), fp, XOR_GAME);
	fclose(fp);
	return TRUE;
}

void Game_Write(void)
/*
 * Writes the game.dat file.
 */
{
	FILE *fp;

	Game.Data->CRC = CRCValue(Game.Data, sizeof(struct game_data) - sizeof(long));

	fp = _fsopen("game.dat", "wb", SH_DENYRW);
	if (fp) {
		EncryptWrite(Game.Data, sizeof(struct game_data), fp, XOR_GAME);
		fclose(fp);
	}
}

void Game_Destroy(void)
/*
 * Frees mem held by Game
 */
{
	free(Game.Data);
	Game.Initialized = FALSE;
}

// ------------------------------------------------------------------------- //

void Game_Settings(void)
{
	char szString[128];

	sprintf(szString, ST_GSETTINGS0, Game.Data->szDateGameStart);
	rputs(szString);
	/* REP:
	sprintf(szString, ST_GSETTINGS1, Game.Data->EliminationMode ? "On" : "Off");
	rputs(szString);
	*/
	sprintf(szString, ST_GSETTINGS2, Game.Data->MaxPermanentMembers);
	rputs(szString);

	if (Game.Data->InterBBS) {
		sprintf(szString, ST_GSETTINGS3, Game.Data->ClanTravel ? "allowed" : "disabled");
		rputs(szString);
	}

	sprintf(szString, ST_GSETTINGS4, Game.Data->ClanEmpires ? "allowed" : "disabled");
	rputs(szString);

	sprintf(szString, ST_GSETTINGS5, Game.Data->MineFights);
	rputs(szString);
	sprintf(szString, ST_GSETTINGS6, Game.Data->ClanFights);
	rputs(szString);
	sprintf(szString, ST_GSETTINGS7, Game.Data->DaysOfProtection);
	rputs(szString);
	door_pause();
}


// ------------------------------------------------------------------------- //

void Game_Start(void)
/*
 * Function is called when the game starts up for FIRST time (i.e. when
 * the StartGameDate is reached.
 */
{
	// this sets up all the necessary stuff to start the game
	News_AddNews(ST_NEWSLOCALRESET);
	Game.Data->GameState = 0;

	Game_Write();
}


// ------------------------------------------------------------------------- //

void Game_Init(void)
/*
 * Initializes Game by reading in the GAME.DAT file.  If file not found,
 * outputs error message and returns to DOS.
 */
{
	if (Verbose) {
		DisplayStr("> Game_Init()\n");
		delay(500);
	}

	Game.Data = malloc(sizeof(struct game_data));
	CheckMem(Game.Data);
	Game.Initialized = TRUE;

	if (!Game_Read()) {
		Game_Destroy();
		System_Error("GAME.DAT not found!  Please run reset.exe\n");
	}

	// ensure CRC is correct
	if (CheckCRC(Game.Data, sizeof(struct game_data) - sizeof(long), Game.Data->CRC) == FALSE) {
		Game_Destroy();
		System_Error("Game data corrupt!\n");
	}

	// If game has not yet begun and is waiting for day to start
	if (Game.Data->GameState == 1) {
		// if today is game's start date
		if (DaysBetween(Game.Data->szDateGameStart, System.szTodaysDate) >= 0) {
			// today is the start of new game
			Game_Start();
		}
	}
}

void Game_Close(void)
/*
 * Deinitializes Game.
 */
{
	if (Game.Initialized == FALSE) return;

	Game_Write();
	Game_Destroy();
}
