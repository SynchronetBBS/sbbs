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

/* The Clans v0.95 -- recode
 *
 *
 * Main module
 */

#include <stdio.h>
#ifndef __unix__
#include <conio.h>
#include <dos.h>
#endif
#include "unix_wrappers.h"

#include <OpenDoor.h>

#include "clans.h"                // functions for this module
#ifdef _WIN32
#include "cmdline.h" /* Win32 only */
#endif
#include "door.h"
#include "game.h"
#include "help.h"
#include "ibbs.h"
#include "input.h"
#include "language.h"
#include "mail.h"
#include "maint.h"
#include "menus.h"
#include "misc.h"
#include "mstrings.h"
#include "news.h"
#include "reg.h"
#include "scores.h"
#include "structs.h"
#include "system.h"
#include "trades.h"
#include "user.h"

/* ----------------------------------------------------------------------- */

static void MainGame(void)
{
	bool Quit = false, EnteredGame = false;
	char *szTheOptions[12], DefaultAction, szString[128];

	LoadStrings(950, 12, szTheOptions);

	while (!Quit) {
		// tell user if game in progress, etc.
		switch (Game.Data.GameState) {
			case 0 :  // Game in progress
				// rputs("The game is currently in progress\n");
				break;
			case 1 :  // Game is waiting for day to start
				rputs("\n|0CThe game will begin on ");
				rputs(Game.Data.szDateGameStart);
				rputs("\n");
				break;
			case 2 :  // Game is waiting for LC's reset
				rputs("\n|0CThe game has not yet begun.  (Waiting for LC Reset)\n");
				break;
		}

		if (DaysBetween(Game.Data.szLastJoinDate, System.szTodaysDate) > 0)
			rputs("|12No new players may join at the moment.\n");


		if (EnteredGame)
			DefaultAction = 'Q';
		else
			DefaultAction = 'E';

		switch (GetChoice("Clan Title", ST_ENTEROPTION, szTheOptions, "ESTYHIQ?BVGL", DefaultAction, true)) {
			case 'E' :  // Enter game
				EnteredGame = true;

				if (Disbanded()) {
					Help("Disbanded", ST_MENUSHLP);
					break;
				}

				// if user disbanded today, don't let him play

				if (Game.Data.GameState == 1 || Game.Data.GameState == 2) {
					// Game is waiting for day to start
					rputs("The game has not yet begun.\n");
					break;
				}

				/* see if past end of game, if so, disallow user */
				if (DaysBetween(Game.Data.szLastJoinDate, System.szTodaysDate) > 0) {
					// rputs("|07Sorry, this game is currently taking in no new players.\n");
					rputs(ST_MAIN0);
					break;
				}

				// otherwise, let user play
				if (!User_Init())
					break;

				/* if not on this BBS, tell him */
				if (PClan->WorldStatus == WS_GONE) {
					// tell him
					// snprintf(szString, sizeof(szString), "|02Your clan was last seen headed for |14%s |02(%s)!\nPlease call that BBS instead.\n\n",
					snprintf(szString, sizeof(szString), ST_MAIN1,
							IBBS.Data->Nodes[ PClan->DestinationBBS - 1].Info.pszVillageName,
							IBBS.Data->Nodes[ PClan->DestinationBBS - 1].Info.pszBBSName);
					rputs(szString);

					rputs("\nIf you had trouble connecting to that BBS, you can call back your clan.\n");
					if (NoYes("|0SCall back your clan to this BBS?") == YES) {
						IBBS_SendComeBack(PClan->DestinationBBS, PClan);
						rputs("\n|0SYour clan will return tomorrow to this village.\n%P");
					}
					break;
				}

				if (stricmp(PClan->szDateOfLastGame, System.szTodaysDate) != 0)
					User_FirstTimeToday();

				News_ReadNews(true);

				ClanStats(PClan, true);

				Mail_Read();
				Trades_CheckTrades();

				GameLoop();

				User_Close();

				break;

			case 'B' :  // Bulletin
				GeneralHelp(ST_BULLHLP);
				break;
			case 'V' :  /* village stats */
				ShowVillageStats();
				break;
			case 'I' :  // Instruction
				Help("Instructions", ST_CLANSHLP);
				door_pause();
				break;
			case 'T' :  /* Today's News */
				News_ReadNews(true);
				break;
			case 'Y' :  /* Yesterday's News */
				News_ReadNews(false);
				break;
			case 'S' :  // See scores
				DisplayScores(false);
				break;
			case 'Q' :  /* Quit */
				Quit = true;
				break;
			case 'H' :  // Help
				MainHelp();
				break;
			case 'G' :  /* game settings */
				// show game settings
				Game_Settings();

				if (Game.Data.InterBBS)
					IBBS_LeagueInfo();
				break;
			case 'L' :  // league scores
				if (Game.Data.InterBBS == false)
					rputs("|07Not in a league\n%P");
				else
					LeagueScores();
				break;
		}
	}
}

int _argc;
char **_argv;
#ifdef _WIN32
char *_lpCmdLine;
#endif

/* ----------------------------------------------------------------------- */
#ifdef ODPLAT_WIN32
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrevInst, LPSTR lpCmdLine, int nShowCmd)
#else
int main(int argc, char *argv[])
#endif
{
#ifdef ODPLAT_WIN32
	_lpCmdLine = lpCmdLine;
#else
	_argc=argc;
	_argv=argv;
#endif

	System_Init();
	// run maintenance if need be
	if (DaysBetween(Game.Data.szTodaysDate, System.szTodaysDate) > 0)
		Maintenance();

	UnregMessage();

	Door_ShowTitle();

	Display("news.txt");

	if (Game.Data.InterBBS)
		IBBS_ShowLeagueAscii();

	MainGame();

	System_Close();

#ifdef ODPLAT_WIN32
	/* Unused Variables */
	(void)hInst;
	(void)hPrevInst;
	(void)nShowCmd;
#endif

	return 0;
}
