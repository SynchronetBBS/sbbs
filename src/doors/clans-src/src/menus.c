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

#include <stdio.h>
#include <stdlib.h>
#ifndef __FreeBSD__
#include <malloc.h>
#endif
#include <string.h>
#ifdef __unix__
#include "unix_wrappers.h"
#endif

#include <OpenDoor.h>
#include "structs.h"
#include "language.h"
#include "mstrings.h"
#include "user.h"
#include "door.h"
#include "input.h"
#include "help.h"
#include "fight.h"
#include "mail.h"
#include "scores.h"
#include "items.h"
#include "pawn.h"
#include "quests.h"
#include "alliance.h"
#include "npc.h"
#include "village.h"
#include "trades.h"
#include "menus2.h"
#include "fight.h"
#include "village.h"
#include "empire.h"
#include "mail.h"
#include "ibbs.h"
#include "reg.h"
#include "system.h"
#include "user.h"


#define MT_PUBLIC       0
#define MT_PRIVATE      1
#define MT_ALLIANCE     2

extern struct Language *Language;
extern struct village Village;
extern struct clan *PClan;
extern struct game Game;
BOOL FirstTimeInMain = FALSE;
extern struct config *Config;

// ------------------------------------------------------------------------- //
_INT16 WorldMenu(void)
{
	char *szTheOptions[7];
	_INT16 iTemp;

	LoadStrings(990, 7, szTheOptions);


	if (!PClan->TravelHelp) {
		PClan->TravelHelp = TRUE;
		Help("World Travel", ST_NEWBIEHLP);
		rputs("\n%P");
	}

	/* get a choice */
	for (;;) {
		rputs("\n\n");

		switch (GetChoice("Travel Menu", ST_ENTEROPTION, szTheOptions, "OQ?VTHS", 'Q', TRUE)) {
			case 'O' :    /* other villages */
				IBBS_SeeVillages(FALSE);
				break;
			case 'T' :    /* travel to other town */
				// FIXME: This may cause problems in the future:  what if a
				// user calls and plays around 11:50pm and then calls back
				// at 12:05, he'll be able to travel!?
				if (Game.Data->ClanTravel == FALSE) {
					rputs("\n|07Clan travel has been disabled in this league.\n%P");
					return 0;
				}

				if (PClan->FirstDay)
					rputs("|07You may not travel to another BBS until tomorrow\n%P");
				else
					IBBS_SeeVillages(TRUE);
				break;
			case 'Q' :    /* return to previous menu */
				return 0;
			case '?' :    /* redisplay options */
				break;
			case 'V' :    /* stats */
				ClanStats(PClan, TRUE);
				break;
			case 'H' :  /* help */
				GeneralHelp(ST_VILLHLP);
				break;
			case 'S' :  /* current trip info */
				IBBS_CurrentTravelInfo();
				break;
		}
	}
	(void)iTemp;
}

// ------------------------------------------------------------------------- //

void AddChatFile(char *szString, char *pszFileName)
{
	FILE *fpChatFile;
	_INT16 LinesRead, iTemp;
	char *szOldLines[42];
	char *szNewLines[42];

	fpChatFile = fopen(pszFileName, "r");

	if (!fpChatFile) {
		/* no file, make it */
		fpChatFile = fopen(pszFileName, "w");
		/* write string */
		fputs(szString, fpChatFile);
		fclose(fpChatFile);
		return;
	}

	for (iTemp = 0; iTemp < 42; iTemp++) {
		szOldLines[iTemp] = malloc(90);
		CheckMem(szOldLines[iTemp]);
		szNewLines[iTemp] = malloc(90);
		CheckMem(szNewLines[iTemp]);
	}

	/* found file, read in lines */

	/* read 42 lines */

	LinesRead = 0;
	for (iTemp = 0; iTemp < 42; iTemp++) {
		if (fgets(szOldLines[LinesRead], 89, fpChatFile) == 0)
			break;

		++LinesRead;
	}
	fclose(fpChatFile);

	/* if there were 42 lines, delete first line */
	if (LinesRead == 42) {
		for (iTemp = 0; iTemp < 42; iTemp++)
			strcpy(szNewLines[iTemp], szOldLines[iTemp + 1]);

		/* first line deleted, now write them to file */
		fpChatFile = fopen(pszFileName, "w");
		if (fpChatFile) {
			for (iTemp = 0; iTemp < 42; iTemp++) {
				fputs(szNewLines[iTemp], fpChatFile);
			}

			/* write OUR new string */
			fputs(szString, fpChatFile);
			fclose(fpChatFile);
		}
	}
	else {
		/* just write them to file and add our own */
		fpChatFile = fopen(pszFileName, "w");
		if (fpChatFile) {
			for (iTemp = 0; iTemp < LinesRead; iTemp++)
				fputs(szOldLines[iTemp], fpChatFile);

			/* write OUR new string */
			fputs(szString, fpChatFile);
			fclose(fpChatFile);
		}
	}

	for (iTemp = 0; iTemp < 42; iTemp++) {
		free(szOldLines[iTemp]);
		free(szNewLines[iTemp]);
	}
}

void Menus_ChatRoom(char *pszFileName)
{
	char szLine[128];
	char szString[128];
	_INT16 LinesInput;
	BOOL FirstTime;

	/* display file */
	od_clr_scr();
	//    GameInfo.NoPause = TRUE;
	Display(pszFileName);
	//    GameInfo.NoPause = FALSE;

	/* ask if user wants to add some words */
	if (NoYes(ST_CHATADDQ) == NO) {
		/* no, leave now! */
		rputs("\n");
		return;
	}

	rputs(ST_CHATENTERCOMMENT);
	// loop until blank line or 3 lines input
	FirstTime = TRUE;
	for (LinesInput = 0; LinesInput < 3; LinesInput++) {
		// get his input
		szString[0] = 0;
		rputs("|0A> |0F");
		GetStr(szString, 77, TRUE);

		if (strlen(szString) < 2) {
			// rputs(ST_ABORTED);
			return;
		}

		if (FirstTime) {
			/* first write clan name on one line */
			strcpy(szLine, "|0B");
			strcat(szLine, PClan->szName);
			strcat(szLine, "\n");
			AddChatFile(szLine, pszFileName);

			FirstTime = FALSE;
		}

		// write quote to file
		strcpy(szLine, "|0B> |0C");
		strcat(szLine, szString);
		strcat(szLine, "\n");

		/* add it on */
		AddChatFile(szLine, pszFileName);
	}

	rputs("\n");
}


// ------------------------------------------------------------------------- //
_INT16 MainMenu(void)
{
	char *szTheOptions[20], DefaultAction, szMainMenu[20],
	*szSecret = "/e/Secret";
	struct UserInfo User;
	_INT16 iTemp, BannerShown;

	BannerShown = RANDOM(5) + 1;

	LoadStrings(970, 20, szTheOptions);
	sprintf(szMainMenu, "Main Menu%d", BannerShown);

	/* get a choice */
	for (;;) {
		rputs("\n\n");

		if (!FirstTimeInMain) {
			Help("Main Title", ST_MENUSHLP);
			FirstTimeInMain = TRUE;
		}

		if (PClan->FightsLeft == 0 || NumMembers(PClan, TRUE) == 0)
			DefaultAction = 'Q';
		else
			DefaultAction = 'E';

		switch (GetChoice(szMainMenu, ST_ENTEROPTION, szTheOptions, "EQ?VMWCTPUH/N!1A2345", DefaultAction, TRUE)) {
			case '!' :    /* delete clan */
				if (NoYes("|0SAre you sure you wish to delete your clan?!") == YES) {
					DeleteClan(PClan->ClanID, PClan->szName, FALSE);

					// if interbbs, send packet to main BBS saying this guy
					// was deleted and that he should be removed from the userlist
					rputs("|14You have been successfully deleted from the game.\n|07Please play again.\n%P");

					if (Game.Data->InterBBS) {
						// remove user from league
						User.ClanID[0] = PClan->ClanID[0];
						User.ClanID[1] = PClan->ClanID[1];

						User.Deleted = FALSE; // don't care

						strcpy(User.szMasterName, PClan->szUserName);
						strcpy(User.szName, PClan->szName);

						LeagueKillUser(&User);
					}

					User_Destroy();
					System_Close();
				}
				break;
			case 'E' :    /* the mines */
				return 1;
			case 'Q' :    /* Quit */
				return -1;
			case '?' :    /* redisplay options */
				break;
			case 'V' :    /* stats */
				ClanStats(PClan, TRUE);
				break;
			case 'M' :    /* market */
				return 2;
			case 'C' :    /* comm. menu */
				return 3;
			case 'B' :    /* clan relations menu */
				return 4;
			case 'W' :  /* world travel */
				return 5;
			case 'T' :  /* town hall */
				return 6;
			case 'P' :  /* empire menu */
				return 7;
			case 'U' :  /* church menu */
				return 9;
			case 'H' :  /* training hall */
				return 10;
			case 'A' :  /* alliance menu */
				return 11;
			case '/' :  //chat villagers
				ChatVillagers(WN_STREET);
				break;
			case 'N' :  // newbie help
				GeneralHelp(ST_NEWBIEHLP);
				break;
			case '1' :
				// od_printf("mem = %ld, stack = %u\n\r", farcoreleft(), stackavail());
				// give gold

#ifdef PRELAB

				Village.Data->MarketQuality = ((Village.Data->MarketQuality+1)%4);

				rputs("Cheat ON!\n");
				PClan->Empire.VaultGold += 320000;
				PClan->Empire.Land += 10;
				PClan->FightsLeft = 20;
				PClan->QuestToday = FALSE;
				for (iTemp = 0; iTemp < MAX_MEMBERS; iTemp++) {
					if (PClan->Member[iTemp]) {
						PClan->Member[iTemp]->Status = Here;
					}
				}
				Fight_Heal(PClan);
#else
				rputs("Debugging flag is off.\n");
#endif

				/*
				PClan->Empire.Land += 10;
				PClan->Empire.VaultGold += 1000;
				PClan->Empire.Army.Followers += 200;
				Village.Empire.Army.Followers += 200;
				Village.Empire.VaultGold += 2000;
				Village.Empire.Land += 20;
				*/
				door_pause();
				break;
			case '2' :  // secret #2
				if (BannerShown != 2)
					rputs(ST_SECRET2);
				else
					RunEvent(FALSE, szSecret, "2", NULL, NULL);
				break;
			case '3' :  // secret #3
				RunEvent(FALSE, szSecret, "3", NULL, NULL);
				break;
			case '4' :  // secret #4
				RunEvent(FALSE, szSecret, "4", NULL, NULL);
				break;
			case '5' :  // secret #5
				if (BannerShown != 5)
					rputs(ST_SECRET5);
				else
					RunEvent(FALSE, szSecret, "5", NULL, NULL);
				break;
		}
	}
	(void)iTemp;
}


// ------------------------------------------------------------------------- //

_INT16 MineMenu(void)
{
	char *szTheOptions[9];
	_INT16 iTemp;
	char DefaultAction;

	LoadStrings(930, 9, szTheOptions);

	if (!PClan->MineHelp) {
		PClan->MineHelp = TRUE;
		Help("Mine Help", ST_NEWBIEHLP);
		rputs("\n%P");
	}

	Help("Mines", ST_MENUSHLP);

	/* get a choice */
	for (;;) {
		if (PClan->FightsLeft == 0 || NumMembers(PClan, TRUE) == 0)
			DefaultAction = 'Q';
		else
			DefaultAction = 'L';

		switch (GetChoice("Mine Menu", ST_ENTEROPTION, szTheOptions, "LFQ?VCG/W", DefaultAction, TRUE)) {
			case 'W' :  // who's here?
				DisplayScores(FALSE);
				break;
			case 'G' :  /* Quest */
				/* if all guys dead, tell guy can't fight */
				if (NumMembers(PClan, TRUE) == 0) {
					rputs(ST_FIGHT0);
					break;
				}
				// if (PClan->QuestToday && Debug == FALSE)
				if (PClan->QuestToday) {
					rputs("\n|15You have already gone on a quest today.  Please try again tomorrow!\n%P");
					break;
				}

				Quests_GoQuest();
				break;
			case 'L' :    /* the mines */
				if (PClan->FightsLeft == 0)
					rputs("|07You have no more fights left for today.\n%P");
				else {
					/* if all guys dead, tell guy can't fight */
					if (NumMembers(PClan, TRUE) == 0) {
						/* tell him all members are dead, come again tomorrow */
						rputs(ST_FIGHT0);
						break;
					}

					// sometimes find treasure
					if (RANDOM(15) == 0) {
						Items_FindTreasureChest();
					}
					// sometimes run a random event
					else if (RANDOM(7) == 0) {
						// run random event using file corresponding to
						// level groupings
						if (PClan->MineLevel == 0)
							RunEvent(FALSE, "/e/Eva", "", NULL, NULL);
						else if (PClan->MineLevel == 1)
							RunEvent(FALSE, "/e/Eva", "", NULL, NULL);
						else if (PClan->MineLevel == 2)
							RunEvent(FALSE, "/e/Eva", "", NULL, NULL);
						else if (PClan->MineLevel == 3)
							RunEvent(FALSE, "/e/Eva", "", NULL, NULL);
						else if (PClan->MineLevel == 4)
							RunEvent(FALSE, "/e/Eva", "", NULL, NULL);
						else if (PClan->MineLevel <= 10)
							RunEvent(FALSE, "/e/Eva", "", NULL, NULL);
						else
							RunEvent(FALSE, "/e/Eva", "", NULL, NULL);
						door_pause();
					}
					else {
						Fight_Monster(PClan->MineLevel, "/m/Output");
					}
					User_Write();
				}
				break;
			case 'F' :  /* fight another clan */
				if (PClan->ClanFights == 0)
					rputs("|07You have no more clan battles.\n%P");
				else {
					Fight_Clan();
					User_Write();
				}
				break;
			case 'C' :    /* change level */
				if (!PClan->MineLevelHelp) {
					PClan->MineLevelHelp = TRUE;
					Help("Mine Level", ST_NEWBIEHLP);
				}

				iTemp = (_INT16) GetLong("|0SEnter level of mine to change to.", PClan->MineLevel, 20);
				/* NO MORE REG!
				if (iTemp >= 5 &&
				  IsRegged(Config->szSysopName, Config->szBBSName, Config->szRegcode) == NFALSE)
				{
				  rputs("\n|12Sorry, you may only play up to the 4th level of the mines in the unregistered\nversion.  Please encourage your sysop to register.\n%P");
				  break;
				}
				*/
				if (iTemp)
					PClan->MineLevel = iTemp;
				break;
			case 'Q' :    /* return to previous menu */
				return 0;
			case '?' :    /* redisplay options */
				break;
			case 'V' :    /* stats */
				ClanStats(PClan, TRUE);
				break;
			case '/' :  //chat villagers
				ChatVillagers(WN_MINE);
				break;
		}
	}
}



// ------------------------------------------------------------------------- //

_INT16 CommunicationsMenu(void)
{
	char *szTheOptions[8];
	char szString[128];
	_INT16 iTemp, ClanId[2];

	LoadStrings(205, 8, szTheOptions);

	if (!PClan->CommHelp) {
		PClan->CommHelp = TRUE;
		Help("Communications", ST_NEWBIEHLP);
		rputs("\n%P");
	}

	for (;;) {
		rputs("\n\n");

		switch (GetChoice("Mail Menu", ST_ENTEROPTION, szTheOptions, "CWPUQV?G", 'Q', TRUE)) {
			case 'C' :      /* check mail */
				if (Mail_Read() == FALSE)
					rputs("|15Sorry, no mail found.\n%P");
				break;
			case 'W' :      /* write mail */
				Mail_Write(MT_PRIVATE);
				break;
			case 'P' :      /* public mail */
				Mail_Write(MT_PUBLIC);
				break;
			case 'U' :      /* update lastread pointer */
				sprintf(szString, "|03The last message you read was #%d.\nSet to which message? ",
						PClan->PublicMsgIndex);
				PClan->PublicMsgIndex = GetLong(szString, PClan->PublicMsgIndex, Village.Data->PublicMsgIndex-1);
				break;
			case 'Q' :      /* return */
				return 0;
			case 'V' :      /* stats */
				ClanStats(PClan, TRUE);
				break;
			case '?' :      /* redisplay options */
				break;
			case 'G' :
				if (Game.Data->InterBBS == FALSE)
					rputs("Option available only in IBBS!\n");
				else
					GlobalMsgPost();
				break;
		}
	}
	(void)ClanId;
	(void)iTemp;
}

// ------------------------------------------------------------------------- //

void WizardShop(void)
{
	char *szTheOptions[6], szString[128];
	_INT16 iTemp, ItemIndex;
	long ExamineCost;

	LoadStrings(1320, 6, szTheOptions);

	if (!PClan->WizardHelp) {
		PClan->WizardHelp = TRUE;
		Help("Wizard", ST_NEWBIEHLP);
		rputs("\n%P");
	}

	// if no wizard shop, tell him
	if (Village.Data->WizardLevel == 0) {
		rputs(ST_WIZ0);
		return;
	}

	/* get a choice */
	for (;;) {
		rputs("\n\n");
		switch (GetChoice("Wizard Menu", ST_ENTEROPTION, szTheOptions, "SBXQ?V", 'Q', TRUE)) {
			case 'Q' :  // quit
				return;
			case 'S' :  /* scrolls */
				Item_BuyItem(I_SCROLL);
				break;
			case 'B' :  /* books */
				Item_BuyItem(I_BOOK);
				break;
			case 'X' :  // examine item
				// list items
				// choose one
				// if not a book or scroll, tell him "I only examine magical items!"
				// tell him cost (30% of item cost?)
				// examine it? yes/no
				// take gold
				// show help


				ListItems(PClan);

				// which item to examine?
				ItemIndex = GetLong(ST_WIZ1, 0, 30);

				if (ItemIndex == 0) break;

				ItemIndex--;

				if (PClan->Items[ItemIndex].Available == FALSE) {
					rputs(ST_INVALIDITEM);
					break;
				}

				// show item
				ShowItemStats(&PClan->Items[ItemIndex], PClan);

				if (PClan->Items[ItemIndex].cType != I_SCROLL &&
						PClan->Items[ItemIndex].cType != I_BOOK) {
					rputs(ST_WIZ2);
					break;
				}

				ExamineCost = PClan->Items[ItemIndex].lCost/4;

				sprintf(szString, ST_WIZ3, ExamineCost, PClan->Empire.VaultGold);

				rputs(szString);
				if (PClan->Empire.VaultGold < ExamineCost) {
					rputs(ST_FMENUNOAFFORD);
					break;
				}
				if (YesNo(ST_WIZ4) == YES) {
					Help(PClan->Items[ItemIndex].szName, ST_WIZHLP);
					PClan->Empire.VaultGold -= ExamineCost;
					door_pause();
				}
				break;
			case 'V' :  // clan stats
				ClanStats(PClan, TRUE);
				break;
		}
	}
	(void)iTemp;
}

_INT16 MarketMenu(void)
{
	char *szTheOptions[10];
	_INT16 iTemp;

	LoadStrings(231, 9, szTheOptions);
	szTheOptions[9] = "Wizard's Shop";

	if (!PClan->MarketHelp) {
		PClan->MarketHelp = TRUE;
		Help("Market", ST_NEWBIEHLP);
		rputs("\n%P");
	}

	/* get a choice */
	for (;;) {
		rputs("\n\n");
		switch (GetChoice("Market Menu", ST_ENTEROPTION, szTheOptions, "WQ?VAST/PZ", 'Q', TRUE)) {
			case 'W' :    /* buy weapon */
				Item_BuyItem(I_WEAPON);
				break;
			case 'A' :    /* buy armor */
				Item_BuyItem(I_ARMOR);
				break;
			case 'S' :    /* buy shield */
				Item_BuyItem(I_SHIELD);
				break;
			case 'Z' :  // wizard's shop
				WizardShop();
				break;
			case 'T' :  /* trading */
				if (PClan->FirstDay)
					rputs("|07You cannot trade until tomorrow.\n%P");
				else
					Trades_MakeTrade();
				break;
			case 'P' :  // pawn shop
				PawnShop();
				break;
			case 'Q' :    /* return to previous menu */
				return 0;
			case '?' :    /* redisplay options */
				break;
			case 'V' :    /* stats */
				ClanStats(PClan, TRUE);
				break;
			case '/' :  //chat villagers
				ChatVillagers(WN_MARKET);
				break;
		}
	}
	(void)iTemp;
}


// ------------------------------------------------------------------------- //

_INT16 AlliancesMenu(void)
{
	struct Alliance *Alliances[MAX_ALLIANCES];
	_INT16 iTemp, NumAlliances, WhichAlliance, NumUserAlliances;
	char cKey, szChoices[MAX_ALLIANCES + 5], szFileName[13];
	char *szString;

	if (!PClan->AllyHelp) {
		PClan->AllyHelp = TRUE;
		Help("Alliances", ST_NEWBIEHLP);
		rputs("\n%P");
	}

	GetAlliances(Alliances);

	szString = MakeStr(128);

	// display them, if any
	rputs(ST_AMENU0);    // show title
	rputs(ST_LONGLINE);
	for (iTemp = 0; iTemp < MAX_ALLIANCES; iTemp++) {
		if (Alliances[iTemp] == NULL) break;

		// od_printf("(%c) %s\n\r", iTemp+'A', Alliances[iTemp]->szName);
		sprintf(szString, ST_AMENU1, iTemp+'A', Alliances[iTemp]->szName);
		rputs(szString);

		szChoices[iTemp] = iTemp + 'A';
	}
	NumAlliances = iTemp;

	// see how many alliances the user has
	NumUserAlliances = 0;
	for (iTemp = 0; iTemp < MAX_ALLIES; iTemp++)
		if (PClan->Alliances[iTemp] != -1)
			NumUserAlliances++;

	// make options ... "ABC....Q\r\n"
	szChoices[ NumAlliances ] = 'Q';
	szChoices[ NumAlliances + 1 ] = 'Z';
	szChoices[ NumAlliances + 2 ] = '\r';
	szChoices[ NumAlliances + 3 ] = '\n';
	szChoices[ NumAlliances + 4 ] = 0;

	rputs(ST_AMENU2);

	rputs(ST_LONGLINE);
	rputs(" ");
	rputs(ST_ENTEROPTION);

	cKey = od_get_answer(szChoices);

	if (cKey == 'Z') {
		rputs(ST_CREATEALLIANCE);
		if (PClan->MadeAlliance)
			rputs("\n|07You have already created an alliance.\n");
		else {
			// ask user if he wants to build one
			// if (NoYes("Create alliance?") == YES)
			if (NoYes(ST_MAKEALLIANCEQ) == YES) {
				if (NumAlliances == MAX_ALLIANCES) {
					// rputs("|09You cannot create a new alliance, there are already too many\n");
					rputs(ST_CANTBUILD);
				}
				else if (NumUserAlliances == MAX_ALLIES) {
					// see if already too many alliances for this guy
					rputs("|09You're already in the max. alliances. can't join anymore.\n");
				}
				else {
					Alliances[NumAlliances] = malloc(sizeof(struct Alliance));
					CheckMem(Alliances[NumAlliances]);

					CreateAlliance(Alliances[ NumAlliances ], Alliances);
					UpdateAlliances(Alliances);
					PClan->MadeAlliance = TRUE;
					if (EnterAlliance(Alliances[ NumAlliances ])) {
						// remove chatfile first
						sprintf(szFileName, "hall%02d.txt", Alliances[NumAlliances]->ID);
						unlink(szFileName);

						// remove from user's alliance list
						for (iTemp = 0; iTemp < MAX_ALLIES; iTemp++)
							if (PClan->Alliances[iTemp] == Alliances[NumAlliances]->ID)
								PClan->Alliances[iTemp] = -1;

						// only if he is the ORIGINAL creator will he have flag set
						if (Alliances[NumAlliances]->OriginalCreatorID[0] == PClan->ClanID[0] &&
								Alliances[NumAlliances]->OriginalCreatorID[1] == PClan->ClanID[1]) {
							PClan->MadeAlliance = FALSE;
						}

						KillAlliance(Alliances[NumAlliances]->ID);

						// delete alliance since user chose to
						free(Alliances[NumAlliances]);

						Alliances[NumAlliances] = NULL;
					}
				}
			}
		}
	}
	else if (cKey != 'Q' && cKey != '\r' && cKey != '\n') {
		WhichAlliance = cKey - 'A';

		sprintf(szString, "%s\n", Alliances[ WhichAlliance ]->szName);
		rputs(szString);

		// see if in that alliance
		for (iTemp = 0; iTemp < MAX_ALLIES; iTemp++) {
			if (PClan->Alliances[ iTemp ] == Alliances[ WhichAlliance ]->ID)
				break;
		}

		if (iTemp != MAX_ALLIES) {
			// in that alliance, enter it

			if (EnterAlliance(Alliances[ WhichAlliance ])) {
				// remove chatfile first
				sprintf(szFileName, "hall%02d.txt", Alliances[WhichAlliance]->ID);
				unlink(szFileName);

				// only if he is the ORIGINAL creator will he have flag set
				if (Alliances[WhichAlliance]->OriginalCreatorID[0] == PClan->ClanID[0] &&
						Alliances[WhichAlliance]->OriginalCreatorID[1] == PClan->ClanID[1]) {
					PClan->MadeAlliance = FALSE;
				}

				// remove from user's alliance list
				for (iTemp = 0; iTemp < MAX_ALLIES; iTemp++)
					if (PClan->Alliances[iTemp] == Alliances[WhichAlliance]->ID)
						PClan->Alliances[iTemp] = -1;

				KillAlliance(Alliances[WhichAlliance]->ID);

				// delete hall since user chose to
				free(Alliances[WhichAlliance]);

				Alliances[WhichAlliance] = NULL;
			}
		}
		else {
			// not in alliance
			rputs("\n |0CYou are not in that alliance.\n");
			if (YesNo(" |0SWrite a message to the alliance's creator?") == YES) {
				MyWriteMessage2(Alliances[WhichAlliance]->CreatorID, FALSE, FALSE, -1, "", FALSE, -1);
			}
		}
	}
	else  // quit
		rputs(ST_QUIT);

	// deinit alliances and update to file
	UpdateAlliances(Alliances);

	// free up mem used by alliances
	for (iTemp = 0; iTemp < MAX_ALLIANCES; iTemp++)
		if (Alliances[iTemp]) {
			free(Alliances[iTemp]);
			Alliances[iTemp] = NULL;
		}

	free(szString);

	return 0;
}




// ------------------------------------------------------------------------- //

_INT16 ChurchMenu(void)
{
	char *szTheOptions[9];
	char szString[80];
	_INT16 iTemp, Event;

	// ST_CHURCHOP0
	LoadStrings(890, 9, szTheOptions);

	if (!PClan->ChurchHelp) {
		PClan->ChurchHelp = TRUE;
		Help("Church Menu", ST_NEWBIEHLP);
		rputs("\n%P");
	}

	/* if no church yet, tell user */
	if (Village.Data->ChurchLevel == 0) {
		rputs("\n|07There is currently no church in the village.\n%P");
		return 0;
	}

	/* get a choice */
	for (;;) {
		/* church info */
		rputs("\n\n");
		rputs(ST_LONGLINE);

		sprintf(szString, " |0CLevel of Church: |0M%d\n", Village.Data->ChurchLevel);
		rputs(szString);

		switch (GetChoice("Church Menu", ST_ENTEROPTION, szTheOptions, "MBPDQ?VU/", 'Q', TRUE)) {
			case 'M' :      /* attend mass */
				if (PClan->AttendedMass) {
					rputs("|07You've already attended today's mass.\n%P");
					break;
				}
				PClan->AttendedMass = TRUE;

				RunEvent(FALSE, "/e/Church", "", NULL, NULL);
				door_pause();
				break;
			case 'P' :      /* pray */
				if (PClan->Prayed) {
					rputs("|07You've already prayed today.\n%P");
					break;
				}
				PClan->Prayed = TRUE;

				RunEvent(FALSE, "/e/Pray", "", NULL, NULL);
				door_pause();
				break;
			case 'B' :      /* ask for blessing */
				// FIXME: make better?
				if (Village.Data->ChurchLevel < 2) {
					rputs("|07This church isn't of a high enough level.  You aren't able to find a priest.\n%P");
					break;
				}

				if (PClan->GotBlessing) {
					rputs("|07You've already gotten a blessing today.\n%P");
					break;
				}
				PClan->GotBlessing = TRUE;

				rputs("\n|06A priest blesses your clan.\n\n");

				/* give XP */
				for (iTemp = 0; iTemp < MAX_MEMBERS; iTemp++) {
					if (PClan->Member[iTemp] && PClan->Member[iTemp]->Status == Here) {
						PClan->Member[iTemp]->SP = PClan->Member[iTemp]->MaxSP;
					}
				}
				rputs("|14Your clan regenerates all its lost skill points!\n%P");
				break;
			case 'D' :      /* resurrect somebody */
				if (Village.Data->ChurchLevel < 3) {
					rputs("|07This church isn't capable of that at its current level.\n%P");
					break;
				}
				ResurrectDead(FALSE);
				break;
			case 'U' :      /* revive unconscious */
				if (Village.Data->ChurchLevel < 2) {
					rputs("|07This church isn't capable of that at its current level.\n%P");
					break;
				}
				ResurrectDead(TRUE);
				break;
			case 'Q' :      /* return to previous menu */
				return 0;
			case '?' :      /* redisplay options */
				break;
			case 'V' :      /* stats */
				ClanStats(PClan, TRUE);
				break;
			case '/' :  //chat villagers
				ChatVillagers(WN_CHURCH);
				break;
		}
	}
	(void)Event;
}


// ------------------------------------------------------------------------- //

_INT16 THallMenu(void)
{
	char *szTheOptions[7];
	char szString[80];
	_INT16 iTemp;

	LoadStrings(655, 7, szTheOptions);

	if (!PClan->THallHelp) {
		PClan->THallHelp = TRUE;
		Help("Training Hall Menu", ST_NEWBIEHLP);
		rputs("\n%P");
	}

	/* if no training hall yet, tell user */
	if (Village.Data->TrainingHallLevel == 0) {
		rputs("\n|07There is currently no training hall in the village.\n%P");
		return 0;
	}

	/* get a choice */
	for (;;) {
		/* training info */
		rputs("\n\n");
		rputs(ST_LONGLINE);

		sprintf(szString, " |0CLevel of Hall  : |0M%d\n", Village.Data->TrainingHallLevel);
		rputs(szString);

		switch (GetChoice("Training Hall", ST_ENTEROPTION, szTheOptions, "TALQ?V/", 'Q', TRUE)) {
			case 'T' :      /* train member */
				TrainMember();
				break;
			case 'A' :      /* add a member */
				/* if too many members, tell user */
				AddMember();
				break;
			case 'L' :      /* release member */
				ReleaseMember();
				/* list members */
				/* see which one he wants */
				/* confirm release of member */
				break;
			case 'Q' :      /* return to previous menu */
				return 0;
			case '?' :      /* redisplay options */
				break;
			case 'V' :      /* stats */
				ClanStats(PClan, TRUE);
				break;
			case '/' :  //chat villagers
				ChatVillagers(WN_THALL);
				break;
		}
	}
	(void)iTemp;
}




// ------------------------------------------------------------------------- //

void GameLoop(void)
{
	_INT16 MenuNum = 0;
	BOOL Quit = FALSE;
	char szString[80];

	while (!Quit) {
		switch (MenuNum) {
			case 0 :  /* Main menu */
				rputs(ST_4RETURNS);
				MenuNum = MainMenu();
				break;
			case 1 :  /* mines */
				rputs(ST_4RETURNS);
				MenuNum = MineMenu();
				break;
			case 2 :  /* MarketMenu */
				rputs(ST_4RETURNS);
				MenuNum = MarketMenu();
				break;
			case 3 :  /* CommMenu */
				rputs(ST_4RETURNS);
				MenuNum = CommunicationsMenu();
				break;
			case 5 :  /* World Travel Menu */
				if (Game.Data->InterBBS) {
					rputs(ST_4RETURNS);
					MenuNum = WorldMenu();
				}
				else {
					// rputs("|07World Travel is only permitted in InterBBS games.\n%P");
					rputs(ST_MAIN5);
					rputs("\n\n");
					MenuNum = MainMenu();
				}
				break;
			case 6 :  /* Town Hall Menu */
				rputs(ST_4RETURNS);

				if (!PClan->TownHallHelp) {
					PClan->TownHallHelp = TRUE;
					Help("Town Hall", ST_NEWBIEHLP);
					rputs("\n%P");
				}

				/* see if ruler */
				if (Village.Data->RulingClanId[0] == -1) {
					/* no ruler yet, ask if he wants to rule */
					// currently no ruler
					sprintf(szString, ST_MAIN6, Village.Data->szName);
					rputs(szString);

					if (YesNo("|0SDoes your clan wish to rule the village?") == YES) {
						Village_NewRuler();
					}
				}
				//MenuNum = 0;
				//break;

				if (Village.Data->RulingClanId[0] != PClan->ClanID[0] ||
						Village.Data->RulingClanId[1] != PClan->ClanID[1]) {
					MenuNum = OutsiderTownHallMenu();
				}
				else
					MenuNum = TownHallMenu();
				break;
			case 7 :  /* empire Menu */
				rputs(ST_4RETURNS);
				if (Game.Data->ClanEmpires)
					Empire_Manage(&PClan->Empire);
				else
					rputs("Clan empires are disabled.\n%P");
				MenuNum = 0;
				break;
			case 9 :  /* church */
				rputs(ST_4RETURNS);
				MenuNum = ChurchMenu();
				break;
			case 10 :  /* training hall */
				rputs(ST_4RETURNS);
				MenuNum = THallMenu();
				break;
			case 11 :  /* Alliances */
				rputs(ST_4RETURNS);
				MenuNum = AlliancesMenu();
				break;
			case -1 : /* quit */
				Quit = TRUE;
				break;
		}
	}
}

