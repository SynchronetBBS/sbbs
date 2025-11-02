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

// Player Editor for The Clans -- no promises that this works! 01/09/2002 au

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#if defined(_WIN32) || defined(DOS)
# include <conio.h> /* Defines _getch */
#endif
#ifdef __unix__
# include <unistd.h>
#endif
#include <string.h>
#include "unix_wrappers.h"
#include "win_wrappers.h"

#include "alliance.h"
#include "console.h"
#include "structs.h"
#include "myopen.h"

static struct village Village;
static struct game Game;
static size_t entry_size = 0;

static void InitVillage(void);
static void UpdateVillage(void);
static void RejectTrade(struct TradeData *TradeData);
static void RemoveFromIPScores(const int16_t ClanID[2]);
static bool ClanIDInList(const int16_t ClanID[2]);
static void RemoveFromUList(const int16_t ClanID[2]);
static void FreeClan(struct clan *Clan);
static bool GetClan(int16_t ID[2], struct clan *TmpClan);
static void DeleteClan(int16_t ID[2]);
static void UpdateClan(struct clan *Clan);
static void InitGame(void);
#ifdef __unix__
static char cio_getch(void);
#endif


#define true_ENTRY_SIZE(x) \
    (BUF_SIZE_clan + ((x) * BUF_SIZE_pc))

#define GAME_DATAFILE "game.dat"
#define VILLAGE_DATAFILE "village.dat"
#define PLAYER_DATAFILE "clans.pc"
#define NEW_PLAYER_DATAFILE "new.pc"
#define ALLIANCE_DATAFILE "ally.dat"
#define TEMP_FILENAME "tmp.$$$"
#define TRADES_DATAFILE "trades.dat"
#define MESSAGE_DATAFILE "clans.msj"
#define USERLIST_DATAFILE "userlist.dat"
#define IPSCORES_DATAFILE "ipscores.dat"


int main(void)
{
	FILE *fpPC;
	long FileSize;
	int16_t NumClans, CurClan;
	int cKey;
	struct clan Clan;

	system("stty raw");
	system("stty opost onlcr");
	printf(".PC (Player Clans) Editor for The Clans.  v0.10\n");
	printf("\nThis PC editor should only be used on local games and only when\n");
	printf("no one is currently playing.\n\n");
	printf("Continue? [Yes]: ");
	fflush(stdout);

	do {
		cKey = toupper(cio_getch()) & 0x7f;
	}
	while (!strchr("YN\n\r", cKey));

	if (cKey == 'N') {
		printf("No\n\nProgram Aborted\n");
		exit(0);
	}
	else
		printf("Yes\n\n");

	// WARNING:  Do not use while player is online!!

	// if no PC file, boot to DOS

	fpPC = fopen(PLAYER_DATAFILE,"rb");

	if (!fpPC) {
		printf("No " PLAYER_DATAFILE " file!\n");
		exit(EXIT_FAILURE);
	}

	// open Village Data
	InitVillage();

	// open Game Data
	InitGame();

	entry_size = true_ENTRY_SIZE((unsigned)Game.Data.MaxPermanentMembers);

	// otherwise, open file, get filesize
	fseek(fpPC, 0L, SEEK_END);
	FileSize = ftell(fpPC);
	if (FileSize / (long)entry_size > INT16_MAX) {
		puts(PLAYER_DATAFILE " too large");
		fclose(fpPC);
		return(EXIT_FAILURE);
	}
	NumClans = (int16_t)(FileSize / (long)entry_size);

	CurClan = 0;

	for (;;) {
		if (fseek(fpPC, CurClan * (long)entry_size, SEEK_SET)) {
			printf("fseek error\n");
			break;
		}

		// get curuser from file
		notEncryptRead_s(clan, &Clan, fpPC, XOR_USER) {
			printf("Couldn't read any players\n");
			break;
		}

		// display user stats
		printf("ClanID  : %02d|%02d\n", Clan.ClanID[0], Clan.ClanID[1]);
		printf("Clan    : %s\n", Clan.szName);
		printf("User    : %s\n", Clan.szUserName);
		printf("LastPlay: %s\n", Clan.szDateOfLastGame);

		printf("\n\n ] next\n [ previous\n ! delete clan\n Q quit\n---------------------\n# ");
		fflush(stdout);

		// get key
		do {
			cKey = cio_getch();
		}
		while (!strchr("[]q!", cKey));

		if (cKey == 'q') {
			printf("Quit\n\n");
			break;
		}
		else if (cKey == '[') {
			printf("Previous Clan\n\n");

			if (CurClan > 0)
				CurClan--;
			else {
				CurClan = NumClans-1;
			}
		}
		else if (cKey == ']') {
			printf("Next Clan\n\n");

			if (CurClan < NumClans-1)
				CurClan++;
			else
				CurClan = 0;
		}
		else if (cKey == '!') {
			printf("Delete Clan\n\n");

			fclose(fpPC);

			printf("Deleting %s\n", Clan.szName);

			DeleteClan(Clan.ClanID);

			fpPC = fopen("clans.pc","r+b");

			if (!fpPC) {
				printf("No CLANS.PC file!\n");
				exit(EXIT_FAILURE);
			}

			// otherwise, open file, get filesize
			fseek(fpPC, 0L, SEEK_END);
			FileSize = ftell(fpPC);
			NumClans = (int16_t)(FileSize / (long)entry_size);

			if (NumClans == 0) {
				printf("All players deleted\n");
				break;
			}

			CurClan = 0;

			UpdateVillage();
		}

		// [ = back one user
		// if at user #1, figure out last user using FileSize/sizeof()
		// choose that user as one to edit
		// else if > #1, CurUser--

		// ] = forward a user
		// if already at end, CurUser = 0
		// else

		// ! = delete user
		// close .PC file
		// run DeleteClan
		// reload file and get stats once more
		// update village info now in case crash occurs later

	}

	fclose(fpPC);

	// Update Village Data
	UpdateVillage();

	return 0;
}

static void DeleteClan(int16_t ClanID[2])
{
	FILE *fpOldPC, *fpNewPC, *OldMessage, *NewMessage;
	FILE *fpTradeFile;
	long OldOffset;
	char szFileName[PATH_SIZE];
	int16_t CurTradeData, iTemp, CurAlliance, CurMember;
	struct TradeData TradeData;
	struct clan *TmpClan;
	struct Message Message;
	bool FoundNewCreator;
	struct Alliance *Alliances[MAX_ALLIANCES];

	// if this is the ruler of town, remove him
	if (ClanID[0] == Village.Data.RulingClanId[0] &&
			ClanID[1] == Village.Data.RulingClanId[1]) {
		Village.Data.RulingClanId[0] = -1;
		Village.Data.RulingClanId[1] = -1;
		Village.Data.RulingDays = 0;
		Village.Data.GovtSystem = GS_DEMOCRACY;

		Village.Data.szRulingClan[0] = 0;

		UpdateVillage();
	}

	// go through PC file
	fpOldPC = fopen(PLAYER_DATAFILE, "rb");
	if (fpOldPC) {
		fpNewPC = fopen(NEW_PLAYER_DATAFILE, "w+b");
		if (!fpNewPC) {
			printf("Can't write to " NEW_PLAYER_DATAFILE "!\n");
			exit(EXIT_FAILURE);
		}

		/* allocate memory */
		TmpClan = (struct clan *) malloc(sizeof(struct clan));
		CheckMem(TmpClan);

		for (;;) {
			/* go through each clan and write his info to new file and
			   skip the clan in question if he is found */

			notEncryptRead_s(clan, TmpClan, fpOldPC, XOR_USER)
				break;

			/* if this is him */
			if (TmpClan->ClanID[0] == ClanID[0] &&
					TmpClan->ClanID[1] == ClanID[1]) {
				fseek(fpOldPC, (long)((unsigned)Game.Data.MaxPermanentMembers * BUF_SIZE_pc), SEEK_CUR);
				continue;
			}

			// read in 6 members
			for (iTemp = 0; iTemp < Game.Data.MaxPermanentMembers; iTemp++) {
				TmpClan->Member[iTemp] = malloc(sizeof(struct pc));
				EncryptRead_s(pc, TmpClan->Member[iTemp], fpOldPC, XOR_PC);
			}

			//=== modifications go here
			// make it so that he is NOT voted for
			if (TmpClan->ClanRulerVote[0] == ClanID[0] &&
					TmpClan->ClanRulerVote[1] == ClanID[1]) {
				TmpClan->ClanRulerVote[0] = TmpClan->ClanRulerVote[1] = -1;
			}


			// IN FUTURE: if ruler of an alliance, set new ruler if another
			// member was found.  if none, remove alliance from list

			//===

			/* write new stuff to new file */
			EncryptWrite_s(clan, TmpClan, fpNewPC, XOR_USER);

			for (iTemp = 0; iTemp < Game.Data.MaxPermanentMembers; iTemp++) {
				EncryptWrite_s(pc, TmpClan->Member[iTemp], fpNewPC, XOR_PC);
				free(TmpClan->Member[iTemp]);
			}
		}

		/* deallocate memory for PCs */
		free(TmpClan);

		fclose(fpOldPC);
		fclose(fpNewPC);

		/* delete old file, rename new one */
		unlink(PLAYER_DATAFILE);
		rename(NEW_PLAYER_DATAFILE, PLAYER_DATAFILE);
	}

	// go through msg file, set all his mail (to/from him) as deleted

	strlcpy(szFileName, MESSAGE_DATAFILE, sizeof(szFileName));

	OldMessage = fopen(szFileName, "rb");
	if (OldMessage) {       // MSJ file exists, so go on
		NewMessage = fopen(TEMP_FILENAME, "wb");
		if (!NewMessage) {
			fclose(OldMessage);
			return;
		}

		for (;;) {
			notEncryptRead_s(Message, &Message, OldMessage, XOR_MSG)
				break;

			if (Message.Data.Length < 0) {
				puts("Negative Message Length\n");
				fclose(OldMessage);
				fclose(NewMessage);
				unlink(TEMP_FILENAME);
				exit(EXIT_FAILURE);
			}
			if ((Message.FromClanID[0] == ClanID[0] &&
					Message.FromClanID[1] == ClanID[1]) ||
					(Message.ToClanID[0] == ClanID[0] &&
					 Message.ToClanID[1] == ClanID[1])) {
				// delete this message by skipping over it
				fseek(OldMessage, Message.Data.Length, SEEK_CUR);
			}
			else {
				Message.Data.MsgTxt = malloc((size_t)Message.Data.Length);
				CheckMem(Message.Data.MsgTxt);

				// write it to new file
				EncryptRead(Message.Data.MsgTxt, (size_t)Message.Data.Length, OldMessage, XOR_MSG);

				EncryptWrite_s(Message, &Message, NewMessage, XOR_MSG);
				CheckedEncryptWrite(Message.Data.MsgTxt, (size_t)Message.Data.Length, NewMessage, XOR_MSG);

				free(Message.Data.MsgTxt);
			}
		}

		fclose(NewMessage);
		fclose(OldMessage);

		// delete old, and rename new
		unlink(szFileName);
		rename(TEMP_FILENAME, szFileName);
	}

	// go through trades.dat, for each trade struct, see if trade is headed
	// for him, if so, set trade as aborted, return goods to user sending

	fpTradeFile = fopen(TRADES_DATAFILE, "r+b");

	if (fpTradeFile) {
		for (CurTradeData = 0;; CurTradeData++) {
			if (fseek(fpTradeFile, (long)CurTradeData * BUF_SIZE_TradeData, SEEK_SET))
				break;

			OldOffset = ftell(fpTradeFile);

			notEncryptRead_s(TradeData, &TradeData, fpTradeFile, XOR_TRADE)
				break;

			/* see if active */
			if (TradeData.Active == false)
				continue;

			if (TradeData.ToClanID[0] == ClanID[0] &&
					TradeData.ToClanID[1] == ClanID[1]) {
				// it's for this deleted player, so, rejecttrade
				RejectTrade(&TradeData);

				// write it to file
				fseek(fpTradeFile, OldOffset, SEEK_SET);
				EncryptWrite_s(TradeData, &TradeData, fpTradeFile, XOR_TRADE);
			}
			else if (TradeData.FromClanID[0] == ClanID[0] &&
					 TradeData.FromClanID[1] == ClanID[1]) {
				// trade is coming from this player, remove it
				TradeData.Active = false;
				fseek(fpTradeFile, OldOffset, SEEK_SET);
				EncryptWrite_s(TradeData, &TradeData, fpTradeFile, XOR_TRADE);
			}
		}

		fclose(fpTradeFile);
	}


	// remove from ALLY.DAT
	GetAlliances(Alliances);

	// see if this clan is the creator of an alliance
	for (CurAlliance = 0; CurAlliance < MAX_ALLIANCES; CurAlliance++) {
		if (Alliances[CurAlliance] &&
				Alliances[CurAlliance]->CreatorID[0] == ClanID[0] &&
				Alliances[CurAlliance]->CreatorID[1] == ClanID[1]) {
			// find a new "leader"
			FoundNewCreator = false;
			for (CurMember = 0; CurMember < MAX_ALLIANCEMEMBERS; CurMember++) {
				// if this is an actual member AND it's not the deleted clan,
				// see if
				if ((Alliances[CurAlliance]->Member[CurMember][0] != -1 &&
						Alliances[CurAlliance]->Member[CurMember][1] != -1) &&
						!(Alliances[CurAlliance]->Member[CurMember][0] == ClanID[0] &&
						  Alliances[CurAlliance]->Member[CurMember][1] == ClanID[1])) {
					FoundNewCreator = true;
					Alliances[CurAlliance]->CreatorID[0] = Alliances[CurAlliance]->Member[CurMember][0];
					Alliances[CurAlliance]->CreatorID[1] = Alliances[CurAlliance]->Member[CurMember][1];

					// FIXME: write him a message in future?
				}
			}

			if (FoundNewCreator == false) {
				// delete this alliance since no new ruler
				DeleteAlliance(CurAlliance, Alliances);
			}
		}
	}

	// remove from any and all alliances he was in
	for (CurAlliance = 0; CurAlliance < MAX_ALLIANCES; CurAlliance++) {
		if (Alliances[CurAlliance]) {
			for (CurMember = 0; CurMember < MAX_ALLIANCEMEMBERS; CurMember++) {
				if (Alliances[CurAlliance]->Member[CurMember][0] == ClanID[0] &&
						Alliances[CurAlliance]->Member[CurMember][1] == ClanID[1]) {
					Alliances[CurAlliance]->Member[CurMember][0] = -1;
					Alliances[CurAlliance]->Member[CurMember][1] = -1;
				}
			}
		}
	}

	// deinit alliances and update to file
	UpdateAlliances(Alliances);

	// free up mem used by alliances
	FreeAlliances(Alliances);

	// remove from list of clan names, remove from list of user names
	RemoveFromUList(ClanID);

	// remove from high scores list
	RemoveFromIPScores(ClanID);
}

static void InitVillage(void)
{
	FILE *fpVillage;
	/*    char ColorScheme[23] = {
	            6,14, 7, 5, 13,7, 5, 8, 3, 8,     5, 14,6, 4, 12,   4,12,3,
	            6,0,0,  1, 9};
	    char FlagScheme[3] = { 12, 15, 9 }; */
	/*  int16_t iTemp;*/

	/* try opening it for share */
	fpVillage = fopen(VILLAGE_DATAFILE, "rb");
	if (!fpVillage) {
		printf("Error opening " VILLAGE_DATAFILE "!\n");
		exit(EXIT_FAILURE);
	}
	else
		EncryptRead_s(village_data, &Village.Data, fpVillage, XOR_VILLAGE);

	fclose(fpVillage);
}

static void UpdateVillage(void)
{
	FILE *fpVillage;

	/* NOTE: Village data held in memory (global) is NOT accurate if this game
	   is played in multinode since the other node could write new info to
	   the village.dat.  Only use Village struct for reference and update it
	   if you want to display info */

	/* try opening it for share */
	fpVillage = fopen(VILLAGE_DATAFILE, "wb");
	if (!fpVillage) {
		printf("Error opening " VILLAGE_DATAFILE "!\n");
		exit(EXIT_FAILURE);
	}

	EncryptWrite_s(village_data, &Village.Data, fpVillage, XOR_VILLAGE);

	fclose(fpVillage);
}

static void RejectTrade(struct TradeData *TradeData)
{
	struct clan *TmpClan;

	TradeData->Active = false;

	TmpClan = malloc(sizeof(struct clan));
	CheckMem(TmpClan);

	if (GetClan(TradeData->FromClanID, TmpClan)) {
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

		UpdateClan(TmpClan);
	}
	else {
		puts("Failed to reject trade, couldn't load the clan");
	}
	FreeClan(TmpClan);
}

static void RemoveFromUList(const int16_t ClanID[2])
{
	// open file for r+b
	// scan file for ClanID
	// set user as deleted
	// close file

	FILE *fpOldUList, *fpNewUList;
	struct UserInfo User;

	// if this guy is NOT in our list, then we don't need to do this
	if (ClanIDInList(ClanID) == false)
		return;

	// open user file
	fpOldUList = fopen(USERLIST_DATAFILE, "rb");
	if (!fpOldUList)    // no user list at all
		return;

	fpNewUList = fopen(TEMP_FILENAME, "wb");
	if (!fpNewUList) {
		printf("Failed to create %s\n", TEMP_FILENAME);
		fclose(fpOldUList);
		exit(EXIT_FAILURE);
	}

	for (;;) {
		notEncryptRead_s(UserInfo, &User, fpOldUList, XOR_USER)
			break;

//        printf("Read in %s\n", User.szName);

		// for each user in file, see if same as ClanID
		if (User.ClanID[0] == ClanID[0] && User.ClanID[1] == ClanID[1]) {
			//printf("skipping over %s\n", User.szName);
			// same, skip over him
			continue;
		}

		// otherwise, don't skip him, write him to new file
		EncryptWrite_s(UserInfo, &User, fpNewUList, XOR_USER);
	}

	// close file
	fclose(fpOldUList);
	fclose(fpNewUList);

	// rename file
	unlink(USERLIST_DATAFILE);
	rename(TEMP_FILENAME, USERLIST_DATAFILE);
}

static bool ClanIDInList(const int16_t ClanID[2])
{
	FILE *fpUList;
	bool Found = false;
	struct UserInfo User;

	fpUList = fopen(USERLIST_DATAFILE, "rb");
	if (!fpUList) {
		// no user list found, assume not in list then
		return false;
	}

	// scan through file until end
	for (;;) {
		notEncryptRead_s(UserInfo, &User, fpUList, XOR_USER)
			break;

		// see if this user's name is same as one we're looking for
		if (User.ClanID[0] == ClanID[0] &&
				User.ClanID[1] == ClanID[1]) {
			Found = true;
			break;
		}
	}

	fclose(fpUList);

	return Found;
}

static void RemoveFromIPScores(const int16_t ClanID[2])
{
	struct UserScore **ScoreList;
	int16_t iTemp;
	char ScoreDate[11];
	FILE *fp;

	fp = fopen(IPSCORES_DATAFILE, "rb");

	if (!fp) {
		return;
	}

	// initialize the score data
	ScoreList = malloc(sizeof(struct UserScore *)*MAX_USERS);
	CheckMem(ScoreList);

	for (iTemp = 0; iTemp < MAX_USERS; iTemp++)
		ScoreList[iTemp] = NULL;

	// read date
	EncryptRead(ScoreDate, 11, fp, XOR_IPS);

	for (iTemp = 0; iTemp < MAX_USERS; iTemp++) {
		ScoreList[iTemp] = malloc(sizeof(struct UserScore));
		CheckMem(ScoreList[iTemp]);
		notEncryptRead_s(UserScore, ScoreList[iTemp], fp, XOR_IPS) {
			free(ScoreList[iTemp]);
			ScoreList[iTemp] = NULL;
			break;
		}

		// if this is the user we wanted, remove him from the list
		if (ScoreList[iTemp]->ClanID[0] == ClanID[0] &&
				ScoreList[iTemp]->ClanID[1] == ClanID[1]) {
			free(ScoreList[iTemp]);
			ScoreList[iTemp] = NULL;
		}
	}
	fclose(fp);

	fp = fopen(IPSCORES_DATAFILE, "wb");

	// write date
	CheckedEncryptWrite(ScoreDate, 11, fp, XOR_IPS);

	// write them to file now and free them at the same time
	for (iTemp = 0; iTemp < MAX_USERS; iTemp++)
		if (ScoreList[iTemp]) {
			EncryptWrite_s(UserScore, ScoreList[iTemp], fp, XOR_IPS);
			free(ScoreList[iTemp]);
		}

	free(ScoreList);

	fclose(fp);
}

static void FreeClan(struct clan *Clan)
{
	int16_t CurMember;

	// free members first
	for (CurMember = 0; CurMember < MAX_MEMBERS; CurMember++) {
		if (Clan->Member[CurMember]) {
			free(Clan->Member[CurMember]);
			Clan->Member[CurMember] = NULL;
		}
	}

	free(Clan);
	Clan = NULL;
}

static bool GetClan(int16_t ClanID[2], struct clan *TmpClan)
{
	FILE *fpPlayerFile;
	int16_t ClanNum, iTemp;
	char szFileName[PATH_SIZE];

	// make them all NULLs for safety
	for (iTemp = 0; iTemp < MAX_MEMBERS; iTemp++)
		TmpClan->Member[iTemp] = NULL;

	strlcpy(szFileName, PLAYER_DATAFILE, sizeof(szFileName));

	/* find guy in file */
	fpPlayerFile = fopen(szFileName, "rb");
	if (!fpPlayerFile) {
		return false;     /* means failed to find clan */
	}

	for (ClanNum = 0;; ClanNum++) {
		if (fseek(fpPlayerFile, (long)ClanNum * (BUF_SIZE_clan + 6L * BUF_SIZE_pc), SEEK_SET)) {
			// couldn't find clan in file
			fclose(fpPlayerFile);
			return false;
		}
		notEncryptRead_s(clan, TmpClan, fpPlayerFile, XOR_USER) {
			fclose(fpPlayerFile);
			return false;
		}

		// make them all NULLs for safety
		for (iTemp = 0; iTemp < MAX_MEMBERS; iTemp++)
			TmpClan->Member[iTemp] = NULL;

		if (TmpClan->ClanID[0] == ClanID[0] &&
				TmpClan->ClanID[1] == ClanID[1]) {
			/* found it */
			/* read in PCs */
			for (iTemp = 0; iTemp < 6; iTemp++) {
				TmpClan->Member[iTemp] = malloc(sizeof(struct pc));
				CheckMem(TmpClan->Member[iTemp]);
				EncryptRead_s(pc, TmpClan->Member[iTemp], fpPlayerFile, XOR_PC);

				/* skip those members which are non-existant */
				if (TmpClan->Member[iTemp]->szName[0] == 0) {
					free(TmpClan->Member[iTemp]);
					TmpClan->Member[iTemp] = NULL;
				}
				else
					TmpClan->Member[iTemp]->MyClan = TmpClan;
			}
			break;
		}
	}
	fclose(fpPlayerFile);

	return true;
}

static void UpdateClan(struct clan *Clan)
{
	FILE *fpPlayerFile;
	/*    char szFileName[PATH_SIZE];*/
	int16_t CurClan, iTemp;
	long OldOffset, Offset;
	struct clan *TmpClan;
	struct pc *TmpPC;

	TmpClan = malloc(sizeof(struct clan));
	CheckMem(TmpClan);
	TmpPC = malloc(sizeof(struct pc));
	CheckMem(TmpPC);

	fpPlayerFile = fopen(PLAYER_DATAFILE, "r+b");
	if (!fpPlayerFile) {
		printf("Couldn't open " PLAYER_DATAFILE "\n");
		free(TmpClan);
		free(TmpPC);
		return;  /* failed to find clan */
	}

	for (CurClan = 0;; CurClan++) {
		/* go through file till you find clan he wants */

		Offset = (long)CurClan * (BUF_SIZE_clan + Game.Data.MaxPermanentMembers * BUF_SIZE_pc);
		if (fseek(fpPlayerFile, Offset, SEEK_SET)) {
			break;  /* couldn't fseek, so exit */
		}

		OldOffset = ftell(fpPlayerFile);

		notEncryptRead_s(clan, TmpClan, fpPlayerFile, XOR_USER)
			break;  /* stop reading if no more players found */

		/* skip if deleted clan */
		if (TmpClan->ClanID[0] == -1)
			continue;

		/* if same Ids, seek back and write to file */
		if (TmpClan->ClanID[0] == Clan->ClanID[0] &&
				TmpClan->ClanID[1] == Clan->ClanID[1]) {
			fseek(fpPlayerFile, OldOffset, SEEK_SET);
			EncryptWrite_s(clan, Clan, fpPlayerFile, XOR_USER);

			// fwrite players
			TmpPC->szName[0] = 0;
			for (iTemp = 0; iTemp < 6; iTemp++) {
				if (Clan->Member[iTemp] && Clan->Member[iTemp]->Undead == false)
					EncryptWrite_s(pc, Clan->Member[iTemp], fpPlayerFile, XOR_PC);
				else
					EncryptWrite_s(pc, TmpPC, fpPlayerFile, XOR_PC);
			}
		}
	}
	fclose(fpPlayerFile);

	free(TmpPC);
	free(TmpClan);
}

static void InitGame(void)
{
	FILE *fpGame;

	/* Open Game Datafile */
	fpGame = fopen(GAME_DATAFILE, "rb");
	if (!fpGame) {
		System_Error("Failure to open " GAME_DATAFILE);
	}

	notEncryptRead_s(game_data, &Game.Data, fpGame, XOR_GAME) {
		System_Error("Unable to read the " GAME_DATAFILE " information!");
	}

	fclose(fpGame);
}

#ifdef __unix__
static char
cio_getch(void)
{
	fd_set fds;
	FD_ZERO(&fds);
	FD_SET(fileno(stdin),&fds);

	select(fileno(stdin)+1, &fds, NULL, NULL, NULL);
	return (char)getchar();
}
#endif
