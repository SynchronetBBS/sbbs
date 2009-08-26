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

#ifdef __unix__
#include <unistd.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#ifndef __FreeBSD__
#include <malloc.h>
#endif
#include <string.h>


#include "structs.h"
#include "language.h"
#include "mstrings.h"
#include "myopen.h"
#include "user.h"
#include "door.h"
#include "input.h"
#include "mail.h"
#include "fight.h"

extern struct clan *PClan;
extern struct Language *Language;


void GetTradeList(struct TradeList *TradeList, BOOL GivingList, char *szTitle)
{
	/* enter choices */
	/* keep looping till chooses "Done" */

	_INT16 /*iTemp, WhichOne,*/ Choice;
	long MaxInput;
	char szString[128];
	BOOL Done = FALSE;
	char *szTheOptions[7] = {"Gold",
							 "Followers",
							 "Footmen",
							 "Axemen",
							 "Knights",
							 "View Stats",
							 "Quit"
							};

	while (!Done) {
		rputs(szTitle);
		rputs(ST_LONGLINE);

		/* list 'em */
		sprintf(szString, " |0A(|0B1|0A) |0CGold         |15%ld\n", TradeList->Gold);
		rputs(szString);
		sprintf(szString, " |0A(|0B2|0A) |0CFollowers    |15%ld\n", TradeList->Followers);
		rputs(szString);
		sprintf(szString, " |0A(|0B3|0A) |0CFootmen      |15%ld\n", TradeList->Footmen);
		rputs(szString);
		sprintf(szString, " |0A(|0B4|0A) |0CAxemen       |15%ld\n", TradeList->Axemen);
		rputs(szString);
		sprintf(szString, " |0A(|0B5|0A) |0CKnights      |15%ld\n", TradeList->Knights);
		rputs(szString);

		rputs(" |0A(|0BV|0A) |0CView Stats\n");
		rputs(" |0A(|0B0|0A) |0CDone\n");
		rputs(ST_LONGLINE);

		switch (Choice = GetChoice("", ST_ENTEROPTION, szTheOptions, "12345V0", '0', TRUE)) {
			case '1' :  /* gold */
				if (GivingList)
					MaxInput = PClan->Empire.VaultGold;
				else
					MaxInput = 1000000000L;

				TradeList->Gold = GetLong("|0SHow much gold?", TradeList->Gold, MaxInput);
				break;
			case '2' :  /* followers */
				if (GivingList)
					MaxInput = PClan->Empire.Army.Followers;
				else
					MaxInput = 1000000L;

				TradeList->Followers = GetLong("|0SHow many followers?", TradeList->Followers, MaxInput);
				break;
			case '3' :  /* footmen */
				if (GivingList)
					MaxInput = PClan->Empire.Army.Footmen;
				else
					MaxInput = 1000000L;

				TradeList->Footmen = GetLong("|0SHow many footmen?", TradeList->Footmen, MaxInput);
				break;
			case '4' :  /* axemen */
				if (GivingList)
					MaxInput = PClan->Empire.Army.Axemen;
				else
					MaxInput = 1000000L;

				TradeList->Axemen = GetLong("|0SHow many axemen?", TradeList->Axemen, MaxInput);
				break;
			case '5' :  /* knights */
				if (GivingList)
					MaxInput = PClan->Empire.Army.Knights;
				else
					MaxInput = 1000000L;

				TradeList->Knights = GetLong("|0SHow many knights?", TradeList->Knights, MaxInput);
				break;
			case '0' :  /* done */
				Done = TRUE;
				break;
			case 'V' :  /* view stats */
				ClanStats(PClan, TRUE);
				break;
		}
	}
}

void RejectTrade(struct TradeData *TradeData)
{
	struct clan *TmpClan;
	char *szString;

	TradeData->Active = FALSE;

	TmpClan = malloc(sizeof(struct clan));
	CheckMem(TmpClan);
	szString = MakeStr(255);

	GetClan(TradeData->FromClanID, TmpClan);

	TmpClan->Empire.VaultGold += TradeData->Giving.Gold;
	TmpClan->Empire.Army.Followers += TradeData->Giving.Followers;
	TmpClan->Empire.Army.Footmen += TradeData->Giving.Footmen;
	TmpClan->Empire.Army.Axemen += TradeData->Giving.Axemen;
	TmpClan->Empire.Army.Knights += TradeData->Giving.Knights;

	/* this ensures the file is zeroed for cheaters -- improve later */
	TradeData->Giving.Gold = 0L;
	TradeData->Giving.Followers = 0L;
	TradeData->Giving.Footmen = 0L;
	TradeData->Giving.Axemen = 0L;
	TradeData->Giving.Knights = 0L;
	TradeData->Giving.Catapults = 0L;

	sprintf(szString, "%s rejected your trade offer.", PClan->szName);
	GenericMessage(szString, TradeData->FromClanID, PClan->ClanID, PClan->szName, FALSE);

	Clan_Update(TmpClan);

	FreeClan(TmpClan);
	free(szString);
}


void Trades_CheckTrades(void)
{
	FILE *fpTradeFile;
	long OldOffset;
	struct clan *TmpClan;
	_INT16 CurTradeData;
	struct TradeData TradeData;
	char szString[255];

	fpTradeFile = fopen("trades.dat", "r+b");

	if (fpTradeFile == NULL)
		return;

	for (CurTradeData = 0;; CurTradeData++) {
		if (fseek(fpTradeFile, (long)(CurTradeData * sizeof(struct TradeData)), SEEK_SET))
			break;

		OldOffset = ftell(fpTradeFile);

		if (EncryptRead(&TradeData, sizeof(struct TradeData), fpTradeFile, XOR_TRADE) == 0)
			break;

		/* see if active */
		if (TradeData.Active == FALSE)
			continue;

		/* if so, see if for this dude */
		if (TradeData.ToClanID[0] != PClan->ClanID[0] ||
				TradeData.ToClanID[1] != PClan->ClanID[1]) {
			/* not for him, skip it */
			continue;
		}

		/* list stuff */
		sprintf(szString, "\n|0B%s wishes to trade with you.  They are offering the following:\n|0C",
				TradeData.szFromClan);
		rputs(szString);

		if (TradeData.Giving.Gold) {
			sprintf(szString, "  %ld gold\n", TradeData.Giving.Gold);
			rputs(szString);
		}
		if (TradeData.Giving.Followers) {
			sprintf(szString, "  %ld followers\n", TradeData.Giving.Followers);
			rputs(szString);
		}
		if (TradeData.Giving.Footmen) {
			sprintf(szString, "  %ld footmen\n", TradeData.Giving.Footmen);
			rputs(szString);
		}
		if (TradeData.Giving.Axemen) {
			sprintf(szString, "  %ld axemen\n", TradeData.Giving.Axemen);
			rputs(szString);
		}
		if (TradeData.Giving.Knights) {
			sprintf(szString, "  %ld knights\n", TradeData.Giving.Knights);
			rputs(szString);
		}
		if (TradeData.Giving.Catapults) {
			sprintf(szString, "  %ld catapults\n", TradeData.Giving.Catapults);
			rputs(szString);
		}
		if (TradeData.Giving.Gold == 0 &&
				TradeData.Giving.Followers == 0 &&
				TradeData.Giving.Footmen == 0 &&
				TradeData.Giving.Axemen == 0 &&
				TradeData.Giving.Knights == 0)
			rputs("  |12Nothing\n");


		/* what they want for it */
		rputs("\n|0BAnd they would like the following in return:\n|0C");

		if (TradeData.Asking.Gold) {
			sprintf(szString, "  %ld gold\n", TradeData.Asking.Gold);
			rputs(szString);
		}
		if (TradeData.Asking.Followers) {
			sprintf(szString, "  %ld followers\n", TradeData.Asking.Followers);
			rputs(szString);
		}
		if (TradeData.Asking.Footmen) {
			sprintf(szString, "  %ld footmen\n", TradeData.Asking.Footmen);
			rputs(szString);
		}
		if (TradeData.Asking.Axemen) {
			sprintf(szString, "  %ld archers\n", TradeData.Asking.Axemen);
			rputs(szString);
		}
		if (TradeData.Asking.Knights) {
			sprintf(szString, "  %ld knights\n", TradeData.Asking.Knights);
			rputs(szString);
		}
		if (TradeData.Asking.Catapults) {
			sprintf(szString, "  %ld catapults\n", TradeData.Asking.Catapults);
			rputs(szString);
		}
		if (TradeData.Asking.Gold == 0 &&
				TradeData.Asking.Followers == 0 &&
				TradeData.Asking.Footmen == 0 &&
				TradeData.Asking.Axemen == 0 &&
				TradeData.Asking.Knights == 0)
			rputs("  |12Nothing\n");

		/* see if user CAN trade */
		if ((PClan->Empire.VaultGold < TradeData.Asking.Gold) ||
				(PClan->Empire.Army.Followers < TradeData.Asking.Followers) ||
				(PClan->Empire.Army.Footmen < TradeData.Asking.Footmen) ||
				(PClan->Empire.Army.Axemen < TradeData.Asking.Axemen) ||
				(PClan->Empire.Army.Knights < TradeData.Asking.Knights)) {
			if (YesNo("\n|12You cannot currently accept the trade's requirements.\n\n|0SDo you wish to ignore this for now?")
					== YES)
				continue;

			/* doesn't want to ignore it, delete it! */
			RejectTrade(&TradeData);

			fseek(fpTradeFile, OldOffset, SEEK_SET);
			EncryptWrite(&TradeData, sizeof(struct TradeData), fpTradeFile, XOR_TRADE);
		}
		else if (((PClan->Empire.Army.Footmen+TradeData.Giving.Footmen) >
				  PClan->Empire.Buildings[B_BARRACKS]*20) ||
				 ((PClan->Empire.Army.Axemen+TradeData.Giving.Axemen) >
				  PClan->Empire.Buildings[B_BARRACKS]*10) ||
				 ((PClan->Empire.Army.Knights+TradeData.Giving.Knights) >
				  PClan->Empire.Buildings[B_BARRACKS]*5)) {
			if (YesNo("\n|12You cannot currently accept the trade's requirements.  You do\nnot have enough room in your barracks to hold the troops.\n\n|0SDo you wish to ignore this for now?")
					== YES)
				continue;

			/* doesn't want to ignore it, delete it! */
			RejectTrade(&TradeData);

			fseek(fpTradeFile, OldOffset, SEEK_SET);
			EncryptWrite(&TradeData, sizeof(struct TradeData), fpTradeFile, XOR_TRADE);
		}
		else {
			/* CAN trade */
			if (NoYes("\n|0SDo you wish to make the trade?") == YES) {
				/* update both dudes' stats */

				TmpClan = malloc(sizeof(struct clan));
				CheckMem(TmpClan);

				GetClan(TradeData.FromClanID, TmpClan);

				TmpClan->Empire.VaultGold += TradeData.Asking.Gold;
				TmpClan->Empire.Army.Followers += TradeData.Asking.Followers;
				TmpClan->Empire.Army.Footmen += TradeData.Asking.Footmen;
				TmpClan->Empire.Army.Axemen += TradeData.Asking.Axemen;
				TmpClan->Empire.Army.Knights += TradeData.Asking.Knights;

				sprintf(szString, "%s accepted your trade.  You received the following:\n\n\
 %ld Gold\n %ld Followers\n %ld Footmen\n %ld Axemen\n %ld Knights\n %ld Catapults\n",
						PClan->szName, TradeData.Asking.Gold,
						TradeData.Asking.Followers, TradeData.Asking.Footmen,
						TradeData.Asking.Axemen, TradeData.Asking.Knights,
						TradeData.Asking.Catapults);

				GenericMessage(szString, TradeData.FromClanID,
							   PClan->ClanID, PClan->szName, FALSE);

				/* update this data */
				Clan_Update(TmpClan);

				FreeClan(TmpClan);

				/* update this clan's data */
				PClan->Empire.VaultGold += TradeData.Giving.Gold;
				PClan->Empire.Army.Followers += TradeData.Giving.Followers;
				PClan->Empire.Army.Footmen += TradeData.Giving.Footmen;
				PClan->Empire.Army.Axemen += TradeData.Giving.Axemen;
				PClan->Empire.Army.Knights += TradeData.Giving.Knights;

				/* update this clan's data */
				PClan->Empire.VaultGold -= TradeData.Asking.Gold;
				PClan->Empire.Army.Followers -= TradeData.Asking.Followers;
				PClan->Empire.Army.Footmen -= TradeData.Asking.Footmen;
				PClan->Empire.Army.Axemen -= TradeData.Asking.Axemen;
				PClan->Empire.Army.Knights -= TradeData.Asking.Knights;

				/* now delete it */
				TradeData.Active = FALSE;

				fseek(fpTradeFile, OldOffset, SEEK_SET);
				EncryptWrite(&TradeData, sizeof(struct TradeData), fpTradeFile, XOR_TRADE);
			}
			else if (YesNo("\n|0SDo you wish to ignore this trade for now and decide on it later?") == YES) {
				/* skip it */
				continue;
			}
			else {
				/* no, just delete it then */
				RejectTrade(&TradeData);

				fseek(fpTradeFile, OldOffset, SEEK_SET);
				EncryptWrite(&TradeData, sizeof(struct TradeData), fpTradeFile, XOR_TRADE);
			}
		}
	}

	fclose(fpTradeFile);
}

void Trades_MakeTrade(void)
{
	struct TradeData TradeData;
	FILE *fpTradeFile;
	_INT16 ClanID[2];
	char szString[255];

	/* ask for who to trade with */
	if (GetClanID(ClanID, FALSE, FALSE, -1, -1) == FALSE) {
		return;
	}

	/* ask for what you want to trade */
	TradeData.Giving.Gold = 0;
	TradeData.Giving.Followers = 0;
	TradeData.Giving.Footmen = 0;
	TradeData.Giving.Axemen = 0;
	TradeData.Giving.Knights = 0;
	TradeData.Giving.Catapults = 0;
	GetTradeList(&TradeData.Giving, TRUE, "\n |0BPlease enter the equipment/troops that you will offer.\n");

	/* see what user wants in return */
	TradeData.Asking.Gold = 0;
	TradeData.Asking.Followers = 0;
	TradeData.Asking.Footmen = 0;
	TradeData.Asking.Axemen = 0;
	TradeData.Asking.Knights = 0;
	TradeData.Asking.Catapults = 0;
	GetTradeList(&TradeData.Asking, FALSE, "\n |0BPlease enter the equipment/troops that you would like in return.\n");

	/* if all zeros, abort */
	if ((TradeData.Asking.Gold == 0 &&
			TradeData.Asking.Followers == 0 &&
			TradeData.Asking.Footmen == 0 &&
			TradeData.Asking.Axemen == 0 &&
			TradeData.Asking.Knights == 0 &&
			TradeData.Asking.Catapults == 0) &&
			(TradeData.Giving.Gold == 0 &&
			 TradeData.Giving.Followers == 0 &&
			 TradeData.Giving.Footmen == 0 &&
			 TradeData.Giving.Axemen == 0 &&
			 TradeData.Giving.Knights == 0 &&
			 TradeData.Giving.Catapults == 0)) {
		rputs(ST_ABORTED);
		return;
	}

	/* show tradelist */
	rputs("|0BYou are offering the following:\n|0C");

	if (TradeData.Giving.Gold) {
		sprintf(szString, "  %ld gold\n", TradeData.Giving.Gold);
		rputs(szString);
	}
	if (TradeData.Giving.Followers) {
		sprintf(szString, "  %ld followers\n", TradeData.Giving.Followers);
		rputs(szString);
	}
	if (TradeData.Giving.Footmen) {
		sprintf(szString, "  %ld footmen\n", TradeData.Giving.Footmen);
		rputs(szString);
	}
	if (TradeData.Giving.Axemen) {
		sprintf(szString, "  %ld archers\n", TradeData.Giving.Axemen);
		rputs(szString);
	}
	if (TradeData.Giving.Knights) {
		sprintf(szString, "  %ld knights\n", TradeData.Giving.Knights);
		rputs(szString);
	}
	if (TradeData.Giving.Catapults) {
		sprintf(szString, "  %ld catapults\n", TradeData.Giving.Catapults);
		rputs(szString);
	}
	if (TradeData.Giving.Gold == 0 &&
			TradeData.Giving.Followers == 0 &&
			TradeData.Giving.Footmen == 0 &&
			TradeData.Giving.Axemen == 0 &&
			TradeData.Giving.Knights == 0 &&
			TradeData.Giving.Catapults == 0)
		rputs("  |12Nothing\n");

	rputs("\n|0BIn return for the following:\n|0C");

	if (TradeData.Asking.Gold) {
		sprintf(szString, "  %ld gold\n", TradeData.Asking.Gold);
		rputs(szString);
	}
	if (TradeData.Asking.Followers) {
		sprintf(szString, "  %ld followers\n", TradeData.Asking.Followers);
		rputs(szString);
	}
	if (TradeData.Asking.Footmen) {
		sprintf(szString, "  %ld footmen\n", TradeData.Asking.Footmen);
		rputs(szString);
	}
	if (TradeData.Asking.Axemen) {
		sprintf(szString, "  %ld archers\n", TradeData.Asking.Axemen);
		rputs(szString);
	}
	if (TradeData.Asking.Knights) {
		sprintf(szString, "  %ld knights\n", TradeData.Asking.Knights);
		rputs(szString);
	}
	if (TradeData.Asking.Catapults) {
		sprintf(szString, "  %ld catapults\n", TradeData.Asking.Catapults);
		rputs(szString);
	}
	if (TradeData.Asking.Gold == 0 &&
			TradeData.Asking.Followers == 0 &&
			TradeData.Asking.Footmen == 0 &&
			TradeData.Asking.Axemen == 0 &&
			TradeData.Asking.Knights == 0 &&
			TradeData.Asking.Catapults == 0)
		rputs("  |12Nothing\n");

	rputs("\n");

	/* ask if he really wants to do this */
	if (YesNo("|0SAre you sure you wish to trade the above?") == NO) {
		rputs(ST_ABORTED);
		return;
	}

	/* if so, reduce his stats accordingly */
	PClan->Empire.VaultGold       -= TradeData.Giving.Gold;
	PClan->Empire.Army.Followers  -= TradeData.Giving.Followers;
	PClan->Empire.Army.Footmen -= TradeData.Giving.Footmen;
	PClan->Empire.Army.Axemen -= TradeData.Giving.Axemen;
	PClan->Empire.Army.Knights -= TradeData.Giving.Knights;

	TradeData.Active = TRUE;
	TradeData.ToClanID[0] = ClanID[0];
	TradeData.ToClanID[1] = ClanID[1];
	TradeData.FromClanID[0] = PClan->ClanID[0];
	TradeData.FromClanID[1] = PClan->ClanID[1];
	strcpy(TradeData.szFromClan, PClan->szName);

	fpTradeFile = fopen("trades.dat", "ab");

	/* append it */
	EncryptWrite(&TradeData, sizeof(struct TradeData), fpTradeFile, XOR_TRADE);
	fclose(fpTradeFile);

	rputs("\n|13Trade has been requested.\n\n");
}

void Trades_Maint(void)
{
	FILE *fpTradeFile, *fpNewTradeFile;
	struct TradeData TradeData;

	fpTradeFile = fopen("trades.dat", "rb");
	if (fpTradeFile) {
		fpNewTradeFile = fopen("tmp.$$$", "wb");
		if (!fpNewTradeFile)
			return;

		for (;;) {
			if (EncryptRead(&TradeData, sizeof(struct TradeData), fpTradeFile, XOR_TRADE) == 0)
				break;

			/* see if active */
			if (TradeData.Active == FALSE)
				continue;

			// it's active, write it to new file
			EncryptWrite(&TradeData, sizeof(struct TradeData), fpNewTradeFile, XOR_TRADE);
		}

		fclose(fpTradeFile);
		fclose(fpNewTradeFile);

		unlink("trades.dat");
		rename("tmp.$$$", "trades.dat");
	}

}
