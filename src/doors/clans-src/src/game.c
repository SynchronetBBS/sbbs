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
#include <stdlib.h>
#ifndef __unix__
#include <share.h>
#endif
#include "unix_wrappers.h"

#include "door.h"
#include "game.h"
#include "language.h"
#include "misc.h"
#include "mstrings.h"
#include "myopen.h"
#include "news.h"
#include "structs.h"
#include "system.h"
#include "video.h"

struct game Game;

// ------------------------------------------------------------------------- //

static bool Game_Read(void)
/*
 * Reads in the GAME.DAT file.  If not found, returns false.
 */
{
	FILE *fp;

	fp = _fsopen("game.dat", "rb", _SH_DENYWR);
	if (!fp)  return false;

	EncryptRead_s(game_data, &Game.Data, fp, XOR_GAME);
	fclose(fp);
	return true;
}

void Game_Write(void)
/*
 * Writes the game.dat file.
 */
{
	FILE *fp;

	fp = _fsopen("game.dat", "wb", _SH_DENYRW);
	if (fp) {
		s_game_data_s(&Game.Data, serBuf, BUF_SIZE_game_data);
		EncryptWrite(serBuf, BUF_SIZE_game_data, fp, XOR_GAME);
		fclose(fp);
	}
}

static void Game_Destroy(void)
{
	Game.Initialized = false;
}

// ------------------------------------------------------------------------- //

void Game_Settings(void)
{
	char szString[128];

	snprintf(szString, sizeof(szString), ST_GSETTINGS0, Game.Data.szDateGameStart);
	rputs(szString);
	/* REP:
	snprintf(szString, sizeof(szString), ST_GSETTINGS1, Game.Data.EliminationMode ? "On" : "Off");
	rputs(szString);
	*/
	snprintf(szString, sizeof(szString), ST_GSETTINGS2, (int)Game.Data.MaxPermanentMembers);
	rputs(szString);

	if (Game.Data.InterBBS) {
		snprintf(szString, sizeof(szString), ST_GSETTINGS3, Game.Data.ClanTravel ? "allowed" : "disabled");
		rputs(szString);
	}

	snprintf(szString, sizeof(szString), ST_GSETTINGS4, Game.Data.ClanEmpires ? "allowed" : "disabled");
	rputs(szString);

	snprintf(szString, sizeof(szString), ST_GSETTINGS5, (int)Game.Data.MineFights);
	rputs(szString);
	snprintf(szString, sizeof(szString), ST_GSETTINGS6, (int)Game.Data.ClanFights);
	rputs(szString);
	snprintf(szString, sizeof(szString), ST_GSETTINGS7, (int)Game.Data.DaysOfProtection);
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
	Game.Data.GameState = 0;

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
		LogDisplayStr("> Game_Init()\n");
		delay(500);
	}

	Game.Initialized = true;

	if (!Game_Read()) {
		Game_Destroy();
		System_Error("GAME.DAT not found!  Please run reset.exe\n");
	}

	// ensure CRC is correct
	if (!Game.Data.CRC) {
		Game_Destroy();
		System_Error("Game data corrupt!\n");
	}

	// If game has not yet begun and is waiting for day to start
	if (Game.Data.GameState == 1) {
		// if today is game's start date
		if (DaysBetween(Game.Data.szDateGameStart, System.szTodaysDate) >= 0) {
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
	if (Game.Initialized == false) return;

	Game_Write();
	Game_Destroy();
}
