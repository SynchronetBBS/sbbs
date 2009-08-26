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

#include <stdio.h>
#include <stdlib.h>
#ifndef __FreeBSD__
#include <malloc.h>
#if defined(_WIN32) || defined(DOS)
# include <conio.h> /* Defines getch */
#endif
#endif /* !__FreeBSD__ */
#ifdef __unix__
#include "unix_wrappers.h"
#include <unistd.h>
#endif
#include <time.h>
#include <string.h>
#include <ctype.h>
/*#include <OpenDoor.h>*/
#include "structs.h"
#include "myopen.h"

struct village Village;
struct game Game;
size_t entry_size = 0;

void InitVillage(void);
void UpdateVillage(void);
void CheckMem(void *Test);
void RejectTrade(struct TradeData *TradeData);
void GetAlliances(struct Alliance *Alliances[MAX_ALLIANCES]);
void UpdateAlliances(struct Alliance *Alliances[MAX_ALLIANCES]);
void RemoveFromIPScores(_INT16 ClanID[2]);
BOOL ClanIDInList(_INT16 ClanID[2]);
void RemoveFromUList(_INT16 ClanID[2]);
void FreeClan(struct clan *Clan);
BOOL GetClan(_INT16 ClanID[2], struct clan *TmpClan);
void DeleteClan(_INT16 ClanID[2]);
void UpdateClan(struct clan *Clan);
void InitGame(void);
#ifdef __unix__
char getch(void);
#endif


#define TRUE_ENTRY_SIZE(x) \
    (sizeof(struct clan) + ((x) * sizeof(struct pc)))

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
	_INT16 NumClans, CurClan;
	char cKey;
	struct clan Clan;

	system("stty raw");
	system("stty opost onlcr");
	printf(".PC (Player Clans) Editor for The Clans.  v0.10\n");
	printf("\nThis PC editor should only be used on local games and only when\n");
	printf("no one is currently playing.\n\n");
	printf("Continue? [Yes]: ");
	fflush(stdout);

	do {
		cKey = toupper(getch());
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
		exit(0);
	}

	// open Village Data
	InitVillage();

	// open Game Data
	InitGame();

	entry_size = TRUE_ENTRY_SIZE(Game.Data->MaxPermanentMembers);

	// otherwise, open file, get filesize
	fseek(fpPC, 0L, SEEK_END);
	FileSize = ftell(fpPC);
	NumClans = FileSize / entry_size;

	CurClan = 0;

	for (;;) {
		if (fseek(fpPC, (CurClan * entry_size), SEEK_SET)) {
			printf("fseek error\n");
			break;
		}

		// get curuser from file
		if (!EncryptRead(&Clan, sizeof(struct clan), fpPC, XOR_PC)) {
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
			cKey = getch();
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
				exit(0);
			}

			// otherwise, open file, get filesize
			fseek(fpPC, 0L, SEEK_END);
			FileSize = ftell(fpPC);
			NumClans = FileSize / entry_size;

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

	if (Village.Data)
		free(Village.Data);
	if (Game.Data)
		free(Game.Data);
	return 0;
}

void DeleteClan(_INT16 ClanID[2])
{
	FILE *fpOldPC, *fpNewPC, *OldMessage, *NewMessage;
	FILE *fpTradeFile;
	long OldOffset;
	char szFileName[40]/*, szString[128]*/;
	_INT16 CurTradeData, iTemp, CurAlliance, CurMember;
	struct TradeData TradeData;
	struct clan *TmpClan;
	struct Message Message;
	BOOL FoundInPCFile = FALSE;     // set to true if he was ever on this board
	BOOL FoundNewCreator;
	struct Alliance *Alliances[MAX_ALLIANCES];

	// if this is the ruler of town, remove him
	if (ClanID[0] == Village.Data->RulingClanId[0] &&
			ClanID[1] == Village.Data->RulingClanId[1]) {
		Village.Data->RulingClanId[0] = -1;
		Village.Data->RulingClanId[1] = -1;
		Village.Data->RulingDays = 0;
		Village.Data->GovtSystem = GS_DEMOCRACY;

		Village.Data->szRulingClan[0] = 0;

		UpdateVillage();
	}

	// go through PC file
	fpOldPC = fopen(PLAYER_DATAFILE, "rb");
	if (fpOldPC) {
		fpNewPC = fopen(NEW_PLAYER_DATAFILE, "w+b");
		if (!fpNewPC) {
			printf("Can't write to " NEW_PLAYER_DATAFILE "!\n");
			exit(0);
		}

		/* allocate memory */
		TmpClan = (struct clan *) malloc(sizeof(struct clan));
		CheckMem(TmpClan);

		for (;;) {
			/* go through each clan and write his info to new file and
			   skip the clan in question if he is found */

			if (EncryptRead(TmpClan, sizeof(struct clan), fpOldPC, XOR_PC) == 0)
				break;

			/* if this is him */
			if (TmpClan->ClanID[0] == ClanID[0] &&
					TmpClan->ClanID[1] == ClanID[1]) {
				FoundInPCFile = TRUE;

				fseek(fpOldPC, Game.Data->MaxPermanentMembers*sizeof(struct pc), SEEK_CUR);
				continue;
			}

			// read in 6 members
			for (iTemp = 0; iTemp < Game.Data->MaxPermanentMembers; iTemp++) {
				TmpClan->Member[iTemp] = malloc(sizeof(struct pc));
				EncryptRead(TmpClan->Member[iTemp], sizeof(struct pc), fpOldPC, XOR_PC);
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
			EncryptWrite(TmpClan, sizeof(struct clan), fpNewPC, XOR_PC);

			for (iTemp = 0; iTemp < Game.Data->MaxPermanentMembers; iTemp++) {
				EncryptWrite(TmpClan->Member[iTemp], sizeof(struct pc), fpNewPC, XOR_PC);
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

	strcpy(szFileName, MESSAGE_DATAFILE);

	OldMessage = fopen(szFileName, "rb");
	if (OldMessage) {       // MSJ file exists, so go on
		NewMessage = fopen(TEMP_FILENAME, "wb");
		if (!NewMessage) {
			return;
		}

		for (;;) {
			if (!EncryptRead(&Message, sizeof(struct Message), OldMessage, XOR_MSG))
				break;

			if ((Message.FromClanID[0] == ClanID[0] &&
					Message.FromClanID[1] == ClanID[1]) ||
					(Message.ToClanID[0] == ClanID[0] &&
					 Message.ToClanID[1] == ClanID[1])) {
				// delete this message by skipping over it
				fseek(OldMessage, Message.Data.Length, SEEK_CUR);
			}
			else {
				Message.Data.MsgTxt = malloc(Message.Data.Length);
				CheckMem(Message.Data.MsgTxt);

				// write it to new file
				EncryptRead(Message.Data.MsgTxt, Message.Data.Length, OldMessage, XOR_MSG);

				EncryptWrite(&Message, sizeof(Message), NewMessage, XOR_MSG);
				EncryptWrite(Message.Data.MsgTxt, Message.Data.Length, NewMessage, XOR_MSG);

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
			if (fseek(fpTradeFile, (long)(CurTradeData * sizeof(struct TradeData)), SEEK_SET))
				break;

			OldOffset = ftell(fpTradeFile);

			if (EncryptRead(&TradeData, sizeof(struct TradeData), fpTradeFile, XOR_TRADE) == 0)
				break;

			/* see if active */
			if (TradeData.Active == FALSE)
				continue;

			if (TradeData.ToClanID[0] == ClanID[0] &&
					TradeData.ToClanID[1] == ClanID[1]) {
				// it's for this deleted player, so, rejecttrade
				RejectTrade(&TradeData);

				// write it to file
				fseek(fpTradeFile, OldOffset, SEEK_SET);
				EncryptWrite(&TradeData, sizeof(struct TradeData), fpTradeFile, XOR_TRADE);
			}
			else if (TradeData.FromClanID[0] == ClanID[0] &&
					 TradeData.FromClanID[1] == ClanID[1]) {
				// trade is coming from this player, remove it
				TradeData.Active = FALSE;
				fseek(fpTradeFile, OldOffset, SEEK_SET);
				EncryptWrite(&TradeData, sizeof(struct TradeData), fpTradeFile, XOR_TRADE);
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
			FoundNewCreator = FALSE;
			for (CurMember = 0; CurMember < MAX_ALLIANCEMEMBERS; CurMember++) {
				// if this is an actual member AND it's not the deleted clan,
				// see if
				if ((Alliances[CurAlliance]->Member[CurMember][0] != -1 &&
						Alliances[CurAlliance]->Member[CurMember][1] != -1) &&
						!(Alliances[CurAlliance]->Member[CurMember][0] == ClanID[0] &&
						  Alliances[CurAlliance]->Member[CurMember][1] == ClanID[1])) {
					FoundNewCreator = TRUE;
					Alliances[CurAlliance]->CreatorID[0] = Alliances[CurAlliance]->Member[CurMember][0];
					Alliances[CurAlliance]->CreatorID[1] = Alliances[CurAlliance]->Member[CurMember][1];

					// FIXME: write him a message in future?
				}
			}

			if (FoundNewCreator == FALSE) {
				// delete this alliance since no new ruler
				free(Alliances[CurAlliance]);
				Alliances[CurAlliance] = NULL;
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
	for (CurAlliance = 0; CurAlliance < MAX_ALLIANCES; CurAlliance++)
		if (Alliances[CurAlliance])
			free(Alliances[CurAlliance]);

	// remove from list of clan names, remove from list of user names
	RemoveFromUList(ClanID);

	// remove from high scores list
	RemoveFromIPScores(ClanID);
}

void InitVillage(void)
{
	FILE *fpVillage;
	/*    char ColorScheme[23] = {
	            6,14, 7, 5, 13,7, 5, 8, 3, 8,     5, 14,6, 4, 12,   4,12,3,
	            6,0,0,  1, 9};
	    char FlagScheme[3] = { 12, 15, 9 }; */
	/*  _INT16 iTemp;*/

	Village.Data = (struct village_data *) malloc(sizeof(struct village_data));
	CheckMem(Village.Data);

	/* try opening it for share */
	fpVillage = fopen(VILLAGE_DATAFILE, "rb");
	if (!fpVillage) {
		printf("Error opening " VILLAGE_DATAFILE "!\n");
		exit(0);
	}
	else
		EncryptRead(Village.Data, (long)sizeof(struct village_data), fpVillage, XOR_VILLAGE);

	fclose(fpVillage);
}

void UpdateVillage(void)
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
		exit(0);
	}

	EncryptWrite(Village.Data, (long)sizeof(struct village_data), fpVillage, XOR_VILLAGE);

	fclose(fpVillage);
}

void CheckMem(void *Test)
{
	if (Test == NULL) {
		printf("\aCheckmem Failed!\n");
		exit(-1);
	}
}

void RejectTrade(struct TradeData *TradeData)
{
	struct clan *TmpClan;

	TradeData->Active = FALSE;

	TmpClan = malloc(sizeof(struct clan));
	CheckMem(TmpClan);

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

	UpdateClan(TmpClan);

	FreeClan(TmpClan);
}

void GetAlliances(struct Alliance *Alliances[MAX_ALLIANCES])
{
	FILE *fp;
	_INT16 iTemp;

	// init alliances as NULLs
	for (iTemp = 0; iTemp < MAX_ALLIANCES; iTemp++)
		Alliances[iTemp] = NULL;

	fp = fopen(ALLIANCE_DATAFILE, "rb");
	if (fp) {
		for (iTemp = 0; iTemp < MAX_ALLIANCES; iTemp++) {
			Alliances[iTemp] = malloc(sizeof(struct Alliance));
			CheckMem(Alliances[iTemp]);

			if (EncryptRead(Alliances[iTemp], sizeof(struct Alliance), fp, XOR_ALLIES) == 0) {
				// no more alliances to read in
				free(Alliances[iTemp]);
				Alliances[iTemp] = NULL;
				break;
			}
		}
		fclose(fp);
	}
}

void UpdateAlliances(struct Alliance *Alliances[MAX_ALLIANCES])
{
	FILE *fp;
	_INT16 iTemp;

	fp = fopen(ALLIANCE_DATAFILE, "wb");
	if (fp) {
		for (iTemp = 0; iTemp < MAX_ALLIANCES; iTemp++) {
			if (Alliances[iTemp] == NULL)
				continue;

			EncryptWrite(Alliances[iTemp], sizeof(struct Alliance), fp, XOR_ALLIES);
		}
		fclose(fp);
	}
}

void RemoveFromUList(_INT16 ClanID[2])
{
	// open file for r+b
	// scan file for ClanID
	// set user as deleted
	// close file

	FILE *fpOldUList, *fpNewUList;
	struct UserInfo User;

	// if this guy is NOT in our list, then we don't need to do this
	if (ClanIDInList(ClanID) == FALSE)
		return;

	// open user file
	fpOldUList = fopen(USERLIST_DATAFILE, "rb");
	if (!fpOldUList)    // no user list at all
		return;

	fpNewUList = fopen(TEMP_FILENAME, "wb");
	// FIXME: assume file is opened

	for (;;) {
		if (EncryptRead(&User, sizeof(struct UserInfo), fpOldUList, XOR_USER) == 0)
			break;

//        printf("Read in %s\n", User.szName);

		// for each user in file, see if same as ClanID
		if (User.ClanID[0] == ClanID[0] && User.ClanID[1] == ClanID[1]) {
			//printf("skipping over %s\n", User.szName);
			// same, skip over him
			continue;
		}

		// otherwise, don't skip him, write him to new file
		EncryptWrite(&User, sizeof(struct UserInfo), fpNewUList, XOR_USER);
	}

	// close file
	fclose(fpOldUList);
	fclose(fpNewUList);

	// rename file
	unlink(USERLIST_DATAFILE);
	rename(TEMP_FILENAME, USERLIST_DATAFILE);
}

BOOL ClanIDInList(_INT16 ClanID[2])
{
	FILE *fpUList;
	BOOL Found = FALSE;
	struct UserInfo User;

	fpUList = fopen(USERLIST_DATAFILE, "rb");
	if (!fpUList) {
		// no user list found, assume not in list then
		return FALSE;
	}

	// scan through file until end
	for (;;) {
		if (EncryptRead(&User, sizeof(struct UserInfo), fpUList, XOR_USER) == 0)
			break;

		// see if this user's name is same as one we're looking for
		if (User.ClanID[0] == ClanID[0] &&
				User.ClanID[1] == ClanID[1]) {
			Found = TRUE;
			break;
		}
	}

	fclose(fpUList);

	return Found;
}

void RemoveFromIPScores(_INT16 ClanID[2])
{
	struct UserScore **ScoreList;
	_INT16 iTemp, UsersFound = 0;
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
		if (!EncryptRead(ScoreList[iTemp], sizeof(struct UserScore), fp, XOR_IPS)) {
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
	UsersFound = iTemp;
	fclose(fp);

	fp = fopen(IPSCORES_DATAFILE, "wb");

	// write date
	EncryptWrite(ScoreDate, 11, fp, XOR_IPS);

	// write them to file now and free them at the same time
	for (iTemp = 0; iTemp < MAX_USERS; iTemp++)
		if (ScoreList[iTemp]) {
			EncryptWrite(ScoreList[iTemp], sizeof(struct UserScore), fp, XOR_IPS);
			free(ScoreList[iTemp]);
		}

	free(ScoreList);

	fclose(fp);
}

void FreeClan(struct clan *Clan)
{
	_INT16 CurMember;

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

BOOL GetClan(_INT16 ClanID[2], struct clan *TmpClan)
{
	FILE *fpPlayerFile;
	_INT16 ClanNum, iTemp;
	char szFileName[50];

	// make them all NULLs for safety
	for (iTemp = 0; iTemp < MAX_MEMBERS; iTemp++)
		TmpClan->Member[iTemp] = NULL;

	strcpy(szFileName, PLAYER_DATAFILE);

	/* find guy in file */
	fpPlayerFile = fopen(szFileName, "rb");
	if (!fpPlayerFile) {
		return FALSE;     /* means failed to find clan */
	}

	for (ClanNum = 0;; ClanNum++) {
		if (fseek(fpPlayerFile, (long)ClanNum *(sizeof(struct clan) + 6L*sizeof(struct pc)), SEEK_SET)) {
			// couldn't find clan in file
			fclose(fpPlayerFile);
			return FALSE;
		}
		if (!EncryptRead(TmpClan, sizeof(struct clan), fpPlayerFile, XOR_PC)) {
			fclose(fpPlayerFile);
			return FALSE;
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
				EncryptRead(TmpClan->Member[iTemp], sizeof(struct pc), fpPlayerFile, XOR_PC);

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

	return TRUE;
}

void UpdateClan(struct clan *Clan)
{
	FILE *fpPlayerFile;
	/*    char szFileName[50];*/
	_INT16 CurClan, iTemp;
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

		Offset = (long)CurClan * (sizeof(struct clan) + Game.Data->MaxPermanentMembers*sizeof(struct pc));
		if (fseek(fpPlayerFile, Offset, SEEK_SET)) {
			break;  /* couldn't fseek, so exit */
		}

		OldOffset = ftell(fpPlayerFile);

		if (EncryptRead(TmpClan, sizeof(struct clan), fpPlayerFile, XOR_PC) == 0)
			break;  /* stop reading if no more players found */

		/* skip if deleted clan */
		if (TmpClan->ClanID[0] == -1)
			continue;

		/* if same Ids, seek back and write to file */
		if (TmpClan->ClanID[0] == Clan->ClanID[0] &&
				TmpClan->ClanID[1] == Clan->ClanID[1]) {
			fseek(fpPlayerFile, OldOffset, SEEK_SET);
			EncryptWrite(Clan, sizeof(struct clan), fpPlayerFile, XOR_PC);

			// fwrite players
			TmpPC->szName[0] = 0;
			for (iTemp = 0; iTemp < 6; iTemp++) {
				if (Clan->Member[iTemp] && Clan->Member[iTemp]->Undead == FALSE)
					EncryptWrite(Clan->Member[iTemp], sizeof(struct pc), fpPlayerFile, XOR_PC);
				else
					EncryptWrite(TmpPC, sizeof(struct pc), fpPlayerFile, XOR_PC);
			}
		}
	}
	fclose(fpPlayerFile);

	free(TmpPC);
	free(TmpClan);
}

void System_Error(char *szErrorMsg)
{
	fprintf(stderr, "System Error: %s\n", szErrorMsg);
	fflush(stderr);
	exit(1);
}

void InitGame(void)
{
	FILE *fpGame;

	/* Open Game Datafile */
	fpGame = fopen(GAME_DATAFILE, "rb");
	if (!fpGame) {
		System_Error("Failure to open " GAME_DATAFILE);
	}

	Game.Data = (struct game_data *) malloc(sizeof(struct game_data));
	CheckMem(Game.Data);

	if (!EncryptRead(Game.Data, (long)sizeof(struct game_data), fpGame, XOR_GAME)) {
		System_Error("Unable to read the " GAME_DATAFILE " information!");
	}

	fclose(fpGame);
}

#ifdef __unix__
char
getch()
{
	fd_set fds;
	FD_ZERO(&fds);
	FD_SET(fileno(stdin),&fds);

	select(fileno(stdin)+1, &fds, NULL, NULL, NULL);
	return (char)getchar();
}
#endif
