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
#ifdef __unix__
# include <time.h>
#else
# include <dos.h>
# include <share.h>
# ifndef _WIN32
#  include <mem.h>
# else
#  include <windows.h>
# endif
#endif
#include "unix_wrappers.h"
#include "win_wrappers.h"

#include "alliance.h"
#include "myopen.h"
#include "parsing.h"
#include "readcfg.h"
#include "structs.h"
#include "video.h"

#define MAX_OPTION      14

/* reset types */
#define RESET_LOCAL     1   // normal reset -- local, NO ibbs junk
#define RESET_JOINIBBS  2   // joining an IBBS league game
#define RESET_LC        3   // leaguewide reset by LC

#define FIGHTSPERDAY    12
#define CLANCOMBATADAY  5


static void ResetMenu(bool InterBBSGame);

static bool YesNo(char *Query, bool Current);

static void EditOption(int16_t WhichOption);
static void DosHelp(char *Topic, char *File);

static void GoReset(struct ResetData *ResetData, int16_t Option);

static void News_AddNews(char *szString);
static void News_CreateTodayNews(void);

static void CreateVillageDat(struct ResetData *ResetData);

void System_Error(char *szErrorMsg);

static void UpdateOption(char Option);

static struct ResetData ResetData;

static char szTodaysDate[11];

//extern unsigned _stklen = 32U*(1024U);

int main(void)
{
	SYSTEMTIME system_time;

	Video_Init();
	Config_Init(0, NULL);

	// initialize date
	GetSystemTime(&system_time);
	snprintf(szTodaysDate, sizeof(szTodaysDate), "%02d/%02d/%4d", system_time.wMonth, system_time.wDay,
			system_time.wYear);

	// load up config

	ResetMenu(Config.InterBBS);
	Video_Close();
	return (0);
}

static void ResetMenu(bool InterBBSGame)
{
	char CurOption = 0;
	int  cInput;
	char OldOption = -1;
	bool Quit = false, DidReset = false, InterBBS = false;
//  int16_t iTemp;

	/* clear all */
	textattr(7);
	clrscr();
	ShowTextCursor(false);

	/* show options */
	qputs(" |03Reset for The Clans " VERSION, 0,0);
	qputs("|09- |01---------------------------------------------------------------------------",0,1);
	xputs(" Game start date", 0, 2);
	xputs(" Last date to join game", 0, 3);
	xputs(" Village Name", 0, 4);
	xputs(" Elimination mode", 0, 5);

	xputs(" Allow interBBS clan travel", 0, 6);
	xputs(" Days before lost troops/clans return", 0, 7);
	xputs(" Allow clan empires", 0, 8);
	xputs(" Mine fights per day", 0, 9);
	xputs(" Clan fights per day", 0, 10);

	xputs(" Max. Permanent Clan Members", 0, 11);
	xputs(" Days of Protection for new users", 0, 12);

	xputs(" Reset local game with these settings", 0, 13);
	xputs(" Join a league", 0, 14);
	xputs(" Leaguewide reset with these settings", 0, 15);
	xputs(" Abort reset", 0, 16);
	qputs("|01--------------------------------------------------------------------------- |09-",0,17);

	qputs("|09 Press the up and down keys to navigate.", 0, 18);

	/* init defaults */
	strlcpy(ResetData.szDateGameStart, szTodaysDate, sizeof(ResetData.szDateGameStart));
	strlcpy(ResetData.szLastJoinDate, "12/16/2199", sizeof(ResetData.szLastJoinDate));
	strlcpy(ResetData.szVillageName, "The Village", sizeof(ResetData.szVillageName));
	ResetData.InterBBSGame = Config.InterBBS;
	ResetData.LeagueWide = false;
	ResetData.InProgress = false;
	ResetData.EliminationMode = false;
	ResetData.ClanTravel = true;
	ResetData.LostDays = 3;
	ResetData.ClanEmpires = true;
	ResetData.MineFights = FIGHTSPERDAY;
	ResetData.ClanFights = CLANCOMBATADAY;
	ResetData.DaysOfProtection = 4;
	ResetData.MaxPermanentMembers = 4;

	ColorArea(0,2+MAX_OPTION, 39, 2+MAX_OPTION, 7);
	gotoxy(2, CurOption+3);

	/* dehilight all options */
	ColorArea(39,2, 76, 16, 15);    // choices on the right side

	ColorArea(0, 2, 39, MAX_OPTION+2, 7);

	/* dehilight options which can't be activated */
	if (!Config.InterBBS) {
		/*          ColorArea(0,2, 39, 13, 7); */
		ColorArea(0,14, 76, 15,  8);
	}
	else if (Config.BBSID != 1) {
		// not LC, can't use leaguewide reset option
		/*          ColorArea(0,2, 39, 16, 7); */
		ColorArea(0,15, 76, 15, 8);
		ColorArea(0,13, 39, 13, 8);
	}
	else {
		// LC's BBS AND IBBS set
		/*                  ColorArea(0,2, 39, 16, 7); */
		ColorArea(0,13, 39, 14, 8);
	}

	textattr(15);
	/* show data */
	UpdateOption(0);
	UpdateOption(1);
	UpdateOption(2);
	UpdateOption(3);
	UpdateOption(4);
	UpdateOption(5);
	UpdateOption(6);
	UpdateOption(7);
	UpdateOption(8);
	UpdateOption(9);
	UpdateOption(10);

	while (!Quit) {
		if (OldOption != CurOption) {
			if (OldOption != -1)
				ColorArea(0, (int16_t)(OldOption+2), 39, (int16_t)(OldOption+2), 7);
			OldOption = CurOption;
			/* show which is highlighted */
			ColorArea(0, (int16_t)(CurOption+2), 39, (int16_t)(CurOption+2), HILIGHT);
			/* Unhighlight old option */
		}

		cInput = getch();
		if (cInput == 0 || cInput == (char)0xE0) {
			cInput = getch();
			switch (cInput) {
				case K_UP   :
				case K_LEFT :
					if (CurOption == 0)
						CurOption = MAX_OPTION;
					else
						CurOption--;
					if ((CurOption == (MAX_OPTION-1)) && (Config.BBSID != 1))
						CurOption--;
					if ((CurOption == (MAX_OPTION-2)) && ((Config.InterBBS == false) || (Config.BBSID == 1)))
						CurOption--;
					if ((CurOption == (MAX_OPTION-3)) && Config.InterBBS)
						CurOption--;
					break;
				case K_DOWN :
				case K_RIGHT:
					if (CurOption == MAX_OPTION)
						CurOption = 0;
					else
						CurOption++;
					if ((CurOption == (MAX_OPTION-3)) && Config.InterBBS)
						CurOption++;
					if ((CurOption == (MAX_OPTION-2)) && ((Config.InterBBS == false) || (Config.BBSID == 1)))
						CurOption++;
					if ((CurOption == (MAX_OPTION-1)) && (Config.BBSID != 1))
						CurOption++;
					break;
				case K_HOME :
				case K_PGUP :
					CurOption = 0;
					break;
				case K_END :
				case K_PGDN :
					CurOption = MAX_OPTION;
					break;
			}
		}
		else if (cInput == 0x1B)
			Quit = true;
		else if (cInput == 13) {
			if (Config.InterBBS == false && CurOption >= (MAX_OPTION-2) &&
					CurOption < MAX_OPTION)
				continue;
			else if (Config.BBSID == 1 && Config.InterBBS && CurOption == (MAX_OPTION-2))
				continue;
			else if (Config.BBSID != 1 && CurOption == (MAX_OPTION-1)) {
				continue;
			}

			if (CurOption == MAX_OPTION) {
				Quit = true;
				DidReset = false;
			}
			else if (CurOption == (MAX_OPTION-3)) {
				void *screen_state;

				// can't reset locally here
				if (Config.InterBBS)
					continue;

				/* reset local now */
				// ask one last time, are you sure?
				screen_state = save_screen();
				textattr(7);
				clrscr();
				if (!YesNo("|07Are you sure you wish to reset?", false)) {
					restore_screen(screen_state);
					break;
				}

				GoReset(&ResetData, RESET_LOCAL);
				Quit = true;
				DidReset = true;
				restore_screen(screen_state);
				break;
			}
			else if (CurOption == (MAX_OPTION-2)) {
				void *screen_state;
				screen_state = save_screen();

				// ask one last time, are you sure?
				textattr(7);
				clrscr();
				if (!YesNo("|07Are you sure you are joining a league?", false)) {
					restore_screen(screen_state);
					break;
				}
				/* join league NOT in progress */
				ResetData.LeagueWide = true;
				ResetData.InProgress = false;
				GoReset(&ResetData, RESET_JOINIBBS);
				Quit = true;
				DidReset = true;
				InterBBS = true;
				restore_screen(screen_state);
			}
			else if (CurOption == (MAX_OPTION-1)) {
				void *screen_state;
				screen_state = save_screen();
				// ask one last time, are you sure?
				textattr(7);
				clrscr();
				if (!YesNo("|07Are you sure you wish to reset the league?", false)) {
					restore_screen(screen_state);
					break;
				}
				/* leaguewide reset now */
				ResetData.LeagueWide = true;
				GoReset(&ResetData, RESET_LC);
				Quit = true;
				DidReset = true;
				InterBBS = true;
				restore_screen(screen_state);
			}
			else {
				EditOption(CurOption);
				textattr(15);
				UpdateOption(CurOption);
			}
		}
	}

	textattr(7);
	clrscr();

	if (DidReset) {
		qputs("|09> |07Your game has been reset.", 0,0);
		if (InterBBS)
			qputs("|09> |03You must now type |11CLANS /I |03to complete the IBBS reset.", 0,1);
	}
	else
		qputs("|09> |07The game was not reset.  No changes were made.", 0,0);

	textattr(7);
	gotoxy(0,3);
	ShowTextCursor(true);
}


static bool YesNo(char *Query, bool Current)
{
	char Answer;
	/* show query */
	zputs(Query);

	/* show Yes/no */
	if (Current)
		zputs(" |01(|15Yes|07/no|01) |11");
	else
		zputs(" |01(|07yes|15/No|01) |11");

	/* get user input */
	Answer = get_answer("YN\r\n");
	if (Answer == 'N') {
		/* user says NO */
		zputs("No\n");
		return false;
	}
	else if (Answer == 'Y') {  /* user says YES */
		zputs("Yes\n");
		return true;
	}
	zputs(Current ? "Yes\n" : "No\n");
	return Current;
}

static void EditOption(int16_t WhichOption)
{
	char *aszHelp[11] = {
		"Date Game Starts",
		"Last Join Date",
		"Village Name",
		"Elimination Mode",
		"Clan Travel",
		"Lost Troops",
		"Clan Empires",
		"Mine Fights",
		"Clan Combat",
		"Max Permanent Members",
		"Days Of Protection"
	};
	int16_t Month, Day, Year;

	/* save screen */
	void *screen_state;
	screen_state = save_screen();

	textattr(7);
	clrscr();

	/* show help */
	DosHelp(aszHelp[WhichOption], "reset.hlp");


	switch (WhichOption) {
		case 0 :    /* dategame started */
			gotoxy(0, 15);
			Month = (int16_t)DosGetLong("|07Enter Month", atoi(ResetData.szDateGameStart), 12);
			Day = (int16_t)DosGetLong("|07Enter Day", atoi(&ResetData.szDateGameStart[3]), 31);
			Year = (int16_t)DosGetLong("|07Enter Year", atoi(&ResetData.szDateGameStart[6]), 2500);

			snprintf(ResetData.szDateGameStart, sizeof(ResetData.szDateGameStart), "%02d/%02d/%4d", Month, Day, Year);
			break;
		case 1 :    /* last join date */
			gotoxy(0, 15);
			Month = (int16_t)DosGetLong("|07Enter Month", atoi(ResetData.szLastJoinDate), 12);
			Day = (int16_t)DosGetLong("|07Enter Day", atoi(&ResetData.szLastJoinDate[3]), 31);
			Year = (int16_t)DosGetLong("|07Enter Year", atoi(&ResetData.szLastJoinDate[6]), 2500);

			snprintf(ResetData.szLastJoinDate, sizeof(ResetData.szLastJoinDate), "%02d/%02d/%4d", Month, Day, Year);
			break;
		case 2 :    /* village name */
			gotoxy(0, 15);
			zputs("|07Please enter the village name|08> |15");
			DosGetStr(ResetData.szVillageName, 29);
			break;
		case 3 :    /* elimination mode? */
			gotoxy(0, 15);
			ResetData.EliminationMode = YesNo("|07Setup elimination mode?", ResetData.EliminationMode);
			break;
		case 4 :    /* clan travel? */
			gotoxy(0, 15);
			ResetData.ClanTravel = YesNo("|07Allow clans to travel?", ResetData.ClanTravel);
			break;
		case 5 :    /* days before lost troops return */
			gotoxy(0, 15);
			ResetData.LostDays = (int16_t)DosGetLong("|07Days before lost troops/clans returned?", ResetData.LostDays, 14);
			break;
		case 6 :    /* clan empires? */
			gotoxy(0, 15);
			ResetData.ClanEmpires = YesNo("|07Allow clans to create empires?", ResetData.ClanEmpires);
			break;
		case 7 :    /* mine fights */
			gotoxy(0, 15);
			ResetData.MineFights = (int16_t)DosGetLong("|07How many mine fights per day?", ResetData.MineFights, 50);
			break;
		case 8 :    /* clan combat */
			gotoxy(0, 15);
			ResetData.ClanFights = (int16_t)DosGetLong("|07How many clan fights per day?", ResetData.ClanFights, MAX_CLANCOMBAT);
			break;
		case 9 :    /* max permanent clan members */
			gotoxy(0, 15);
			ResetData.MaxPermanentMembers = (int16_t)DosGetLong("|07How many members?", ResetData.MaxPermanentMembers, 6);
			if (ResetData.MaxPermanentMembers == 0)
				ResetData.MaxPermanentMembers = 1;
			break;
		case 10:    /* days of protection */
			gotoxy(0, 15);
			ResetData.DaysOfProtection = (int16_t)DosGetLong("|07How many days?", ResetData.DaysOfProtection, 16);
			break;
	}

	/* restore screen */
	restore_screen(screen_state);
}

static void DosHelp(char *Topic, char *File)
{
	char *Lines[22], string[155];
	int16_t cTemp, Found = false, CurLine, NumLines;
	FILE *fp;
	bool EndOfTopic = false;

	qputs("|15", 0, 0);
	qputs(Topic, 1, 0);
	qputs("|09- |01---------------------------------------------------------------------------",0,1);

	qputs("|01--------------------------------------------------------------------------- |09-",0,12);

	fp = fopen(File, "r");
	if (!fp) {
		qputs("|12Game help not found!\n", 0, 2);
		return;
	}

	for (cTemp = 0; cTemp < 22; cTemp++)
		Lines[cTemp] = malloc(255);

	/* search for topic */
	while (!Found) {
		if (fgets(string, 155, fp) == NULL) {
			fclose(fp);
			qputs("|12Game help not found!\n", 0, 2);
			for (cTemp = 0; cTemp < 22; cTemp++)
				free(Lines[cTemp]);
			return;
		}

		if (string[0] == '^') {
			/* see if topic is correct */
			if (strspn(&string[1], Topic) == strlen(Topic)) {
				Found = true;
			}
		}
	}

	while (EndOfTopic == false) {
		/* read in up to 22 lines */
		for (CurLine = 0; CurLine < 22; CurLine++) {
			fgets(Lines[CurLine], 355, fp);

			if (Lines[CurLine][0] == '^') {
				if (strspn(&Lines[CurLine][1], "END") == 3) {
					EndOfTopic = true;
					break;
				}
			}
		}
		NumLines = CurLine;

		/* display it */
		for (CurLine = 0; CurLine < NumLines; CurLine++) {
			qputs(Lines[CurLine], 0, (int16_t)(CurLine+2));
		}
	}

	fclose(fp);

	for (cTemp = 0; cTemp < 22; cTemp++)
		free(Lines[cTemp]);
}

static void
GenerateGameID(char *ptr, size_t sz)
{
	SYSTEMTIME st;

	GetSystemTime(&st);
	snprintf(ptr, sz, "%04d%02d%02d%02d%02d%02d", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
}

static void GoReset(struct ResetData *ResetData, int16_t Option)
{
	FILE *fp;
	struct game_data Game_Data;

	// Delete unwanted files here
	unlink("clans.msj");
	unlink("clans.pc");
	unlink("disband.dat");
	unlink("trades.dat");
	unlink("public.txt");
	unlink("pawn.dat");
	unlink("clans.npx");

	KillAlliances();

	unlink("ally.dat");

	// delete IBBS files
	unlink("ibbs.dat");
	unlink("ipscores.dat");
	unlink("userlist.dat");
	unlink("backup.dat");
	unlink("leaving.dat");

	if (Option == RESET_LOCAL) {
		/*
		 * reset local game, no IBBS
		 */

		// create new game.dat file
		Game_Data.GameState = 1;
		Game_Data.InterBBS = false;

		strlcpy(Game_Data.szTodaysDate, szTodaysDate, sizeof(Game_Data.szTodaysDate));
		strlcpy(Game_Data.szDateGameStart, ResetData->szDateGameStart, sizeof(Game_Data.szDateGameStart));
		strlcpy(Game_Data.szLastJoinDate, ResetData->szLastJoinDate, sizeof(Game_Data.szLastJoinDate));

		Game_Data.NextClanID = 0;
		Game_Data.NextAllianceID = 0;

		Game_Data.MaxPermanentMembers = ResetData->MaxPermanentMembers;
		Game_Data.ClanEmpires = ResetData->ClanEmpires;
		Game_Data.MineFights = ResetData->MineFights;
		Game_Data.ClanFights = ResetData->ClanFights;
		Game_Data.DaysOfProtection = ResetData->DaysOfProtection;

		fp = fopen("game.dat", "wb");
		EncryptWrite_s(game_data, &Game_Data, fp, XOR_GAME);
		fclose(fp);

		// update news
		unlink("yest.asc");
		rename("today.asc", "yest.asc");
		News_CreateTodayNews();
		News_AddNews("|0A \xaf\xaf\xaf |0CThe game has been reset!\n\n");
	}
	else if (Option == RESET_JOINIBBS) {
		// create new game.dat file
		Game_Data.GameState = 2;
		Game_Data.InterBBS = true;

		GenerateGameID(Game_Data.GameID, sizeof(Game_Data.GameID));

		strlcpy(Game_Data.szTodaysDate, szTodaysDate, sizeof(Game_Data.szTodaysDate));
		strlcpy(Game_Data.szDateGameStart, ResetData->szDateGameStart, sizeof(Game_Data.szDateGameStart));
		strlcpy(Game_Data.szLastJoinDate, ResetData->szLastJoinDate, sizeof(Game_Data.szLastJoinDate));

		Game_Data.NextClanID = 0;
		Game_Data.NextAllianceID = 0;

		Game_Data.MaxPermanentMembers = ResetData->MaxPermanentMembers;
		Game_Data.ClanEmpires = ResetData->ClanEmpires;
		Game_Data.MineFights = ResetData->MineFights;
		Game_Data.ClanFights = ResetData->ClanFights;
		Game_Data.DaysOfProtection = ResetData->DaysOfProtection;

		fp = fopen("game.dat", "wb");
		EncryptWrite_s(game_data, &Game_Data, fp, XOR_GAME);
		fclose(fp);

		// update news
		unlink("yest.asc");
		rename("today.asc", "yest.asc");
		News_CreateTodayNews();
		News_AddNews("|0A \xaf\xaf\xaf |0CThe game is waiting for LC OK.\n\n");

	}
	else if (Option == RESET_LC) {
		// leaguewide reset

		// REP: make sure only LC can do this somehow...

		// create game.dat file
		Game_Data.GameState = 1;
		Game_Data.InterBBS = true;

		GenerateGameID(Game_Data.GameID, sizeof(Game_Data.GameID));

		strlcpy(Game_Data.szTodaysDate, szTodaysDate, sizeof(Game_Data.szTodaysDate));
		strlcpy(Game_Data.szDateGameStart, ResetData->szDateGameStart, sizeof(Game_Data.szDateGameStart));
		strlcpy(Game_Data.szLastJoinDate, ResetData->szLastJoinDate, sizeof(Game_Data.szLastJoinDate));

		Game_Data.NextClanID = 0;
		Game_Data.NextAllianceID = 0;

		Game_Data.MaxPermanentMembers = ResetData->MaxPermanentMembers;
		Game_Data.ClanEmpires = ResetData->ClanEmpires;
		Game_Data.MineFights = ResetData->MineFights;
		Game_Data.ClanFights = ResetData->ClanFights;
		Game_Data.DaysOfProtection = ResetData->DaysOfProtection;

		fp = fopen("game.dat", "wb");
		EncryptWrite_s(game_data, &Game_Data, fp, XOR_GAME);
		fclose(fp);

		// update news
		unlink("yest.asc");
		rename("today.asc", "yest.asc");
		News_CreateTodayNews();
		News_AddNews("|0A \xaf\xaf\xaf |0CThe game has been reset!\n\n");
	}

	// create new village data
	CreateVillageDat(ResetData);
}


static void News_CreateTodayNews(void)
{
	/* this initializes the TODAY.ASC file */

	char szString[128];

	snprintf(szString, sizeof(szString), "|0CNews for |0B%s\n\n", szTodaysDate);
	News_AddNews(szString);

	/* give other info like increase in cost of etc. */
}

static void News_AddNews(char *szString)
{
	FILE *fpNewsFile;

	/* open news file */

	/* add to it */

	fpNewsFile = _fsopen("today.asc", "at", SH_DENYWR);

	fputs(szString, fpNewsFile);

	fclose(fpNewsFile);
}

void ClearFlags(char *Flags)
{
	int16_t iTemp;

	for (iTemp = 0; iTemp < 8; iTemp++)
		Flags[iTemp] = 0;
}

static void InitEmpire(struct empire *Empire, int16_t UserEmpire)
{
	int16_t iTemp;

	if (UserEmpire)
		Empire->Land = 100;
	else
		Empire->Land = 0;

	Empire->Points = 0;

	for (iTemp = 0; iTemp < MAX_BUILDINGS; iTemp++)
		Empire->Buildings[iTemp] = 0;

	Empire->WorkerEnergy = 100;

	// deal with army
	Empire->Army.Followers = 0;
	Empire->Army.Footmen = 0;
	Empire->Army.Axemen = 0;
	Empire->Army.Knights = 0;
	Empire->Army.Rating = 100;
	Empire->Army.Level = 0;
	Empire->SpiesToday = 0;
	Empire->AttacksToday = 0;
	Empire->LandDevelopedToday = 0;
	Empire->Army.Strategy.AttackLength = 10;
	Empire->Army.Strategy.DefendLength = 10;
	Empire->Army.Strategy.LootLevel    = 0;
	Empire->Army.Strategy.AttackIntensity = 50;
	Empire->Army.Strategy.DefendIntensity = 50;
}



static void CreateVillageDat(struct ResetData *ResetData)
{
	FILE *fp;
	struct village_data Village_Data = {
		{ 1, 9, 3, 1, 11, 7, 3, 2, 10, 2, 5, 3, 9, 5, 13, 1, 9, 15, 7, 0, 0, 8, 4,
			1, 8, 1 },
		{ -1 }
	};

	Village_Data.PublicMsgIndex = 1;
	strlcpy(Village_Data.szName, ResetData->szVillageName, sizeof(Village_Data.szName));

	Village_Data.TownType = -1;      /* unknown for now */
	Village_Data.TaxRate = 0;      /* no taxes */
	Village_Data.InterestRate = 0;   /* 0% interest rate */
	Village_Data.GST = 0;        /* no gst */
	Village_Data.Empire.VaultGold = 45000L; /* some money to begin with */
	Village_Data.ConscriptionRate = 10;

	Village_Data.RulingClanId[0] = -1; /* no ruling clan yet */
	Village_Data.RulingClanId[1] = -1;

	Village_Data.szRulingClan[0] = 0;
	Village_Data.RulingDays = 0;
	Village_Data.GovtSystem = GS_DEMOCRACY;


	Village_Data.MarketLevel = 0;      /* weaponshop level */
	Village_Data.TrainingHallLevel = 0;
	Village_Data.ChurchLevel = 0;
	Village_Data.PawnLevel = 0;
	Village_Data.WizardLevel = 0;

	Village_Data.SetTaxToday = false;
	Village_Data.SetInterestToday = false;
	Village_Data.SetGSTToday = false;
	Village_Data.SetConToday = false;

	Village_Data.UpMarketToday = false;
	Village_Data.UpTHallToday = false;
	Village_Data.UpChurchToday = false;
	Village_Data.UpPawnToday = false;
	Village_Data.UpWizToday = false;
	Village_Data.ShowEmpireStats = false;

	ClearFlags(Village_Data.HFlags);
	ClearFlags(Village_Data.GFlags);

	InitEmpire(&Village_Data.Empire, false);
	strlcpy(Village_Data.Empire.szName, Village_Data.szName, sizeof(Village_Data.Empire.szName));
	Village_Data.Empire.Land = 500;      // village starts off with this
	Village_Data.Empire.Buildings[B_BARRACKS] = 10;
	Village_Data.Empire.Buildings[B_WALL] = 10;
	Village_Data.Empire.Buildings[B_TOWER] = 5;
	Village_Data.Empire.Buildings[B_STEELMILL] = 5;
	Village_Data.Empire.Buildings[B_BUSINESS] = 10;
	Village_Data.Empire.Army.Footmen = 100;
	Village_Data.Empire.Army.Axemen = 25;

	Village_Data.CostFluctuation = 5 - RANDOM(11);
	Village_Data.MarketQuality = MQ_AVERAGE;

	fp = fopen("village.dat", "wb");
	EncryptWrite_s(village_data, &Village_Data, fp, XOR_VILLAGE);
	fclose(fp);
}

static void UpdateOption(char Option)
{
	char szString[50];
	switch (Option) {
		case 0:
			xputs(ResetData.szDateGameStart, 40, 2);
			break;
		case 1:
			xputs(ResetData.szLastJoinDate, 40, 3);
			break;
		case 2:
			xputs("                                        ", 40, 4);
			xputs(ResetData.szVillageName, 40, 4);
			break;
		case 3:
			if (ResetData.EliminationMode)
				xputs("Yes", 40, 5);
			else
				xputs("No ", 40, 5);
			break;
		case 4:
			if (ResetData.ClanTravel)
				xputs("Yes", 40, 6);
			else
				xputs("No ", 40, 6);
			break;
		case 5:
			snprintf(szString, sizeof(szString), "%-2d ", ResetData.LostDays);
			xputs(szString, 40, 7);
			break;
		case 6:
			if (ResetData.ClanEmpires)
				xputs("Yes", 40, 8);
			else
				xputs("No ", 40, 8);
			break;
		case 7:
			snprintf(szString, sizeof(szString), "%-2d ", ResetData.MineFights);
			xputs(szString, 40, 9);
			break;
		case 8:
			snprintf(szString, sizeof(szString), "%-2d ", ResetData.ClanFights);
			xputs(szString, 40, 10);
			break;
		case 9:
			snprintf(szString, sizeof(szString), "%-2d ", ResetData.MaxPermanentMembers);
			xputs(szString, 40, 11);
			break;
		case 10:
			snprintf(szString, sizeof(szString), "%-2d ", ResetData.DaysOfProtection);
			xputs(szString, 40, 12);
			break;
	}
}
