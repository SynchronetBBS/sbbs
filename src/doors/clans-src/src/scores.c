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
#ifndef __unix__
#include <share.h>
#endif
#include "unix_wrappers.h"

#include "door.h"
#include "game.h"
#include "ibbs.h"
#include "language.h"
#include "mstrings.h"
#include "myopen.h"
#include "packet.h"
#include "parsing.h"
#include "structs.h"
#include "system.h"
#include "user.h"
#include "village.h"

void GetColourString(char *szColourString, int16_t Colour, bool Clear)
{
	int16_t TempColour;
	char *szFgColours[8] = {
		"30m",
		"34m",
		"32m",
		"36m",
		"31m",
		"35m",
		"33m",
		"37m"
	},
	*szBgColours[8] = {
		"40m",
		"44m",
		"42m",
		"46m",
		"41m",
		"45m",
		"43m",
		"47m"
	};

	strcpy(szColourString, "\x1B[");

	if (Clear)
		strcat(szColourString, "0;");

	if (Colour >= 24) {
		// background flashing

		TempColour = Colour - 24;

		if (TempColour >= 8)
			TempColour = 0;

		strcat(szColourString, "5;");
		strcat(szColourString, szBgColours[ TempColour ]);
	}
	else if (Colour >= 16) {
		// background

		TempColour = Colour - 16;

		strcat(szColourString, szBgColours[ TempColour ]);
	}
	else if (Colour >= 8) {
		// foreground bright
		TempColour = Colour - 8;

		strcat(szColourString, "1;");
		strcat(szColourString, szFgColours[ TempColour ]);
	}
	else {
		// regular colour
		TempColour = Colour;

		strcat(szColourString, szFgColours[ TempColour ]);
	}
}

void PipeToAnsi(char *szOut, char *szIn)
{
	char *pcOut, *pcIn;
	char Colour, Bg, Fg, szDigits[3],
	szColourString[20];

	pcOut = szOut;
	pcIn  = szIn;

	*pcOut = 0;

	while (*pcIn) {
		if ((*pcIn) == '`' && iscodechar(*(pcIn + 1))
				&& iscodechar(*(pcIn +2))) {
			pcIn++;

			// get background
			if (isdigit(*pcIn))
				Bg = *pcIn - '0';
			else
				Bg = *pcIn - 'A' + 10;

			// must add this to use GetColourString properly
			Bg += 16;
			GetColourString(szColourString, Bg, true);
			strcat(pcOut, szColourString);
			pcOut = strchr(szOut, 0);

			pcIn++;

			// get foreground
			if (isdigit(*pcIn))
				Fg = *pcIn - '0';
			else
				Fg = *pcIn - 'A' + 10;
			GetColourString(szColourString, Fg, false);
			strcat(pcOut, szColourString);
			pcOut = strchr(szOut, 0);

			pcIn++;
		}
		else if (*pcIn == '|' && isdigit(*(pcIn+1)) && isdigit(*(pcIn+2))) {
			szDigits[0] = *(pcIn+1);
			szDigits[1] = *(pcIn+2);
			szDigits[2] = 0;

			Colour = atoi(szDigits);

			GetColourString(szColourString, Colour, true);

			strcat(pcOut, szColourString);

			pcOut = strchr(szOut, 0);

			pcIn += 3;
		}
		else {
			*pcOut = *pcIn;

			pcIn++;
			pcOut++;

			*pcOut = 0;
		}
	}
}




void DisplayScores(bool MakeFile)
{
	FILE *fpPCFile, *fpScoreFile[2] = {NULL, NULL};
	char szFileName[50], szString[255], szPadding[21];
	struct clan *TmpClan;
	struct SortData {
		char szName[25];
		int32_t Points;
		bool IsRuler;
		char Symbol[21], PlainSymbol[21];
		int16_t WorldStatus;
		bool UsedInList;
		bool Eliminated;
		int16_t VillageID;
		bool Living;
	} *SortData[128];
	char AnsiSymbol[190];
	int16_t SortList[128];
	int16_t CurClan = 0, iTemp, NumClans, CurMember;
	long Offset;
	int32_t MostPoints;
	int16_t CurHigh, Padding;
	bool NoPlayers = true;

	/* initialize sortdata */
	for (iTemp = 0; iTemp < 128; iTemp++)
		SortData[iTemp] = NULL;

	/* read in all clans from data file  and place them in sortdata
	   structure */

	/* sort them into SortList */
	/* SortList[0] points to top clan
	   SortList[1] points to 2nd top clan
	   etc.
	 */
	strcpy(szFileName, ST_CLANSPCFILE);

	if (MakeFile) {
		fpScoreFile[0] = fopen(Config->szScoreFile[0], "w");
		if (!fpScoreFile[0])
			return;

		fpScoreFile[1] = fopen(Config->szScoreFile[1], "w");
		if (!fpScoreFile[1]) {
			fclose(fpScoreFile[0]);
			return;
		}

		// write headers
		fputs(ST_SCORE0ASCII, fpScoreFile[0]);
		fputs(ST_SCORE0ANSI, fpScoreFile[1]);
	}

	fpPCFile = _fsopen(ST_CLANSPCFILE, "rb", SH_DENYWR);
	if (!fpPCFile) {
		NoPlayers = true;
	}
	else {
		TmpClan = malloc(sizeof(struct clan));
		CheckMem(TmpClan);
		/* read 'em in */
		for (CurClan = 0;; CurClan++) {
			Offset = (long)CurClan * (BUF_SIZE_clan + 6L * BUF_SIZE_pc);
			if (fseek(fpPCFile, Offset, SEEK_SET)) {
				break;  /* couldn't fseek, so exit */
			}

			notEncryptRead_s(clan, TmpClan, fpPCFile, XOR_USER) {
				break;  /* stop reading if no more players found */
			}

			for (CurMember = 0; CurMember < MAX_MEMBERS; CurMember++)
				TmpClan->Member[CurMember] = NULL;

			for (CurMember = 0; CurMember < 6; CurMember++) {
				TmpClan->Member[CurMember] = malloc(sizeof(struct pc));
				CheckMem(TmpClan->Member[CurMember]);
				EncryptRead_s(pc, TmpClan->Member[CurMember], fpPCFile, XOR_PC);
			}

			// since we could read in a player, means at least one exists
			NoPlayers = false;

			/* allocate mem for this clan in the list */
			SortData[CurClan] = malloc(sizeof(struct SortData));
			CheckMem(SortData[CurClan]);

			/* this sets it so it hasn't been used yet in the sort list */
			SortData[CurClan]->UsedInList = false;

			strcpy(SortData[CurClan]->szName, TmpClan->szName);
			SortData[CurClan]->Points = TmpClan->Points;
			SortData[CurClan]->VillageID = TmpClan->ClanID[0];


			SortData[CurClan]->IsRuler = false;

			if (TmpClan->ClanID[0] == Village.Data->RulingClanId[0] &&
					TmpClan->ClanID[1] == Village.Data->RulingClanId[1])
				SortData[CurClan]->IsRuler = true;
			else
				SortData[CurClan]->IsRuler = false;

			if (TmpClan->Eliminated)
				SortData[CurClan]->Eliminated = true;
			else
				SortData[CurClan]->Eliminated = false;

			strcpy(SortData[CurClan]->Symbol, TmpClan->Symbol);
			RemovePipes(TmpClan->Symbol, SortData[CurClan]->PlainSymbol);

			SortData[CurClan]->WorldStatus = TmpClan->WorldStatus;

			if (NumMembers(TmpClan, true) == 0)
				SortData[CurClan]->Living = false;
			else
				SortData[CurClan]->Living = true;

			for (CurMember = 0; CurMember < 6; CurMember++) {
				free(TmpClan->Member[CurMember]);
				TmpClan->Member[CurMember] = NULL;
			}
		}

		free(TmpClan);
		TmpClan = NULL;
		NumClans = CurClan;
		fclose(fpPCFile);
	}

	if (NoPlayers) {
		if (MakeFile) {
			fputs(ST_SCORE1ASCII, fpScoreFile[0]);
			fputs(ST_SCORE1ANSI, fpScoreFile[1]);
			fclose(fpScoreFile[0]);
			fclose(fpScoreFile[1]);
		}
		else
			rputs(" |07No one has played the game.\n");
		return;
	}
	/* sort them into SortList */
	/* SortList[0] points to top clan
	   SortList[1] points to 2nd top clan
	   etc.
	 */


	for (iTemp = 0; iTemp < NumClans; iTemp++) {
		MostPoints = -32000;
		CurHigh = 0;

		/* go through them finding highest one */
		for (CurClan = 0; CurClan < NumClans; CurClan++) {
			/* skip if used already */
			if (SortData[CurClan]->UsedInList)
				continue;

			/* is this highest? */
			if (SortData[CurClan]->Points > MostPoints) {
				/* yes, set stuff up */
				CurHigh = CurClan;
				MostPoints = SortData[CurClan]->Points;
			}
		}

		/* found highest out of list */
		SortData[CurHigh]->UsedInList = true;
		SortList[iTemp] = CurHigh;
	}

	/* show them */
	if (!MakeFile) {
		rputs(ST_SCOREHEADER);
		rputs(ST_SCORELINE);
	}

	/* go through list */
	for (CurClan = 0; CurClan < NumClans; CurClan++) {
		if (MakeFile) {
			strcpy(szString, SortData[ SortList[CurClan] ]->Symbol);
			RemovePipes(SortData[ SortList[CurClan] ]->Symbol, szString);
			Padding = 20 - strlen(szString);
			szPadding[0] = 0;
			for (iTemp = 0; iTemp < Padding; iTemp++)
				strcat(szPadding, " ");

			sprintf(szString, ST_SCORE2ASCII,
					SortData[ SortList[CurClan] ]->szName,
					szPadding, SortData[ SortList[CurClan] ]->PlainSymbol,
					SortData[ SortList[CurClan] ]->Points);

			if (SortData[ SortList[CurClan] ]->Eliminated)
				strcat(szString, ST_SCORE6ASCII);
			else if (SortData[ SortList[CurClan] ]->WorldStatus == WS_GONE) {
				strcat(szString, ST_SCORE3ASCII);
			}
			else {
				if (SortData[ SortList[CurClan] ]->Living == false)
					strcat(szString, "Dead");
				else if (SortData[ SortList[CurClan] ]->VillageID != Config->BBSID
						 && Game.Data->InterBBS)
					strcat(szString, "Visiting");
				else
					strcat(szString, ST_SCORE4ASCII);
			}

			if (SortData[ SortList[CurClan] ]->IsRuler)
				strcat(szString, ST_SCORE5ASCII);
			else
				strcat(szString, "\n");

			// for ASCII
			fputs(szString, fpScoreFile[0]);

			// for ANSI

			PipeToAnsi(AnsiSymbol, SortData[ SortList[CurClan] ]->Symbol);

			sprintf(szString, ST_SCORE2ANSI,
					SortData[ SortList[CurClan] ]->szName,
					szPadding, AnsiSymbol,
					SortData[ SortList[CurClan] ]->Points);

			if (SortData[ SortList[CurClan] ]->Eliminated)
				strcat(szString, ST_SCORE6ANSI);
			else if (SortData[ SortList[CurClan] ]->WorldStatus == WS_GONE) {
				strcat(szString, ST_SCORE3ANSI);
			}
			else {
				if (SortData[ SortList[CurClan] ]->Living == false)
					strcat(szString, "\x1B[0;31mDead");
				else if (SortData[ SortList[CurClan] ]->VillageID != Config->BBSID
						 && Game.Data->InterBBS)
					strcat(szString, "Visiting");
				else
					strcat(szString, ST_SCORE4ANSI);
			}

			if (SortData[ SortList[CurClan] ]->IsRuler)
				strcat(szString, ST_SCORE5ANSI);
			else
				strcat(szString, "\n");

			fputs(szString, fpScoreFile[1]);
		}
		else {
			// sprintf(szString, " |%02d%3s |02%-30s |15%6ld  ", SortData[ SortList[CurClan] ]->Color, SortData[ SortList[CurClan] ]->Symbol, SortData[ SortList[CurClan] ]->szName, SortData[ SortList[CurClan] ]->Points);
			// figure out clanid length without pipes
			RemovePipes(SortData[ SortList[CurClan] ]->Symbol, szString);
			Padding = 20 - strlen(szString);
			szPadding[0] = 0;
			for (iTemp = 0; iTemp < Padding; iTemp++)
				strcat(szPadding, " ");

			sprintf(szString, " |0C%-30s %s%s`07  |15%-6" PRId32 "  ",
					SortData[ SortList[CurClan] ]->szName,
					szPadding, SortData[ SortList[CurClan] ]->Symbol,
					SortData[ SortList[CurClan] ]->Points);

			if (SortData[ SortList[CurClan] ]->Eliminated)
				strcat(szString, "|04Eliminated");
			else if (SortData[ SortList[CurClan] ]->WorldStatus == WS_GONE)
				strcat(szString, "|0BAway");
			else {
				if (SortData[ SortList[CurClan] ]->Living == false)
					strcat(szString, "|04Dead");
				else if (SortData[ SortList[CurClan] ]->VillageID != Config->BBSID
						 && Game.Data->InterBBS)
					strcat(szString, "|0BVisiting");
				else
					strcat(szString, "|0BHere");
			}

			if (SortData[ SortList[CurClan] ]->IsRuler)
				strcat(szString, "  |0A(Ruler)\n");
			else
				strcat(szString, "\n");

			if (CurClan%20 == 0 && CurClan)
				door_pause();

			rputs(szString);
		}
	}

	if (MakeFile) {
		fclose(fpScoreFile[0]);
		fclose(fpScoreFile[1]);
	}
	else
		rputs(ST_SCORELINE);

	/* free mem used by sortdata */
	for (iTemp = 0; iTemp < 128; iTemp++)
		if (SortData[iTemp]) {
			free(SortData[iTemp]);
			SortData[iTemp] = NULL;
		}

	if (!MakeFile)
		door_pause();
}



/// ------------------------------------------------------------------------- //

void SendScoreData(struct UserScore **UserScores)
{
	int16_t iTemp, NumScores;
	struct Packet Packet;
	FILE *fp;

	// figure out how many scores
	NumScores = 0;
	for (iTemp = 0; iTemp < MAX_USERS; iTemp++)
		if (UserScores[iTemp])
			NumScores++;

	/* create packet header */
	Packet.Active = true;
	Packet.BBSIDTo = 1;         // Main BBS
	Packet.BBSIDFrom = IBBS.Data->BBSID;
	Packet.PacketType = PT_SCOREDATA;
	strcpy(Packet.szDate, System.szTodaysDate);
	Packet.PacketLength = NumScores * BUF_SIZE_UserScore + sizeof(int16_t);
	strcpy(Packet.GameID, Game.Data->GameID);

	fp = _fsopen("tmp.$$$", "wb", SH_DENYRW);
	if (!fp)  return;

	// write packet header
	EncryptWrite_s(Packet, &Packet, fp, XOR_PACKET);

	// write how many scores
	EncryptWrite16(&NumScores, fp, XOR_PACKET);

	// write scores
	for (iTemp = 0; iTemp < MAX_USERS; iTemp++)
		if (UserScores[iTemp])
			EncryptWrite_s(UserScore, UserScores[iTemp], fp, XOR_PACKET);

	fclose(fp);

	IBBS_SendPacketFile(Packet.BBSIDTo, "tmp.$$$");
	unlink("tmp.$$$");
}

void ProcessScoreData(struct UserScore **UserScores)
{
	struct UserScore **NewList;
	struct UserScore **OldList;
	struct UserScore *HighestFound;
	FILE *fpOld, *fpNew;
	bool FromOld = false;
	int16_t iTemp, CurUser, CurScore, WhichOne = 0;
	int32_t HighestPoints;

	// initialize lists
	NewList = malloc(sizeof(struct UserScore *) * MAX_USERS);
	OldList = malloc(sizeof(struct UserScore *) * MAX_USERS);
	CheckMem(NewList);
	CheckMem(OldList);
	for (iTemp = 0; iTemp < MAX_USERS; iTemp++)
		NewList[iTemp] = OldList[iTemp] = NULL;

	// load up old list
	fpOld = _fsopen("ipscores.dat", "rb", SH_DENYRW);
	if (fpOld) {
		// skip date
		fseek(fpOld, 11 * sizeof(char), SEEK_SET);
		for (iTemp = 0; iTemp < MAX_USERS; iTemp++) {
			OldList[iTemp] = malloc(sizeof(struct UserScore));
			CheckMem(OldList[iTemp]);
			notEncryptRead_s(UserScore, OldList[iTemp], fpOld, XOR_IPS) {
				free(OldList[iTemp]);
				OldList[iTemp] = NULL;
				break;
			}
		}
		fclose(fpOld);
	}

	// merge lists by removing common elements in old list
	for (iTemp = 0; iTemp < MAX_USERS; iTemp++) {
		if (UserScores[iTemp]) {
			// search through old list for this user, if there, delete him
			// from the old list
			for (CurUser = 0; CurUser < MAX_USERS; CurUser++) {
				if (OldList[CurUser] &&
						OldList[CurUser]->ClanID[0] == UserScores[iTemp]->ClanID[0] &&
						OldList[CurUser]->ClanID[1] == UserScores[iTemp]->ClanID[1]) {
					// in old list, remove him
					free(OldList[CurUser]);
					OldList[CurUser] = NULL;
				}
			}
		}
	}

	// sort lists
	for (CurScore = 0; CurScore < MAX_TOPUSERS; CurScore++) {
		HighestPoints = 0;
		HighestFound = NULL;

		// go through old list first, look for score higher than current one
		for (iTemp = 0; iTemp < MAX_USERS; iTemp++) {
			if (OldList[iTemp] &&
					OldList[iTemp]->Points >= HighestPoints) {
				HighestFound = OldList[iTemp];
				HighestPoints = HighestFound->Points;
				FromOld = true;
				WhichOne = iTemp;
			}
		}

		// go through userscores list next
		for (iTemp = 0; iTemp < MAX_USERS; iTemp++) {
			if (UserScores[iTemp] &&
					UserScores[iTemp]->Points >= HighestPoints) {
				HighestFound = UserScores[iTemp];
				HighestPoints = HighestFound->Points;
				FromOld = false;

				WhichOne = iTemp;
			}
		}

		// if none higher found, stop now
		if (HighestFound == NULL)
			break;

		// else, add to list, remove highest found from the list
		NewList[CurScore] = malloc(sizeof(struct UserScore));
		CheckMem(NewList[CurScore]);
		*NewList[CurScore] = *HighestFound;

		if (FromOld) {
			free(OldList[WhichOne]);
			OldList[WhichOne] = NULL;
		}
		else {
			free(UserScores[WhichOne]);
			UserScores[WhichOne] = NULL;
		}
	}

	// write new list to file
	fpNew = _fsopen("ipscores.dat", "wb", SH_DENYRW);
	if (fpNew) {
		// write date
		EncryptWrite(System.szTodaysDate, 11, fpNew, XOR_IPS);

		for (iTemp = 0; iTemp < MAX_USERS; iTemp++)
			if (NewList[iTemp])
				EncryptWrite_s(UserScore, NewList[iTemp], fpNew, XOR_IPS);

		fclose(fpNew);
	}

	// free up lists we used
	for (iTemp = 0; iTemp < MAX_USERS; iTemp++) {
		if (NewList[iTemp])
			free(NewList[iTemp]);
		if (OldList[iTemp])
			free(OldList[iTemp]);
	}
	free(NewList);
	free(OldList);
}

void LeagueScores(void)
{
	struct UserScore **ScoreList;
	int16_t iTemp, UsersFound, /*CurID,*/ iTemp2, Padding;
	char ScoreDate[11], szString[128], szPadding[21];
	FILE *fp;

	fp = _fsopen("ipscores.dat", "rb", SH_DENYRW);

	if (!fp) {
		rputs(ST_LSCORES0);
		return;
	}

	// initialize the score data
	ScoreList = malloc(sizeof(struct UserScore *)*MAX_USERS);
	CheckMem(ScoreList);

	for (iTemp = 0; iTemp < MAX_USERS; iTemp++)
		ScoreList[iTemp] = NULL;

	EncryptRead(ScoreDate, 11, fp, XOR_IPS);

	for (iTemp = 0; iTemp < MAX_USERS; iTemp++) {
		ScoreList[iTemp] = malloc(sizeof(struct UserScore));
		CheckMem(ScoreList[iTemp]);
		notEncryptRead_s(UserScore, ScoreList[iTemp], fp, XOR_IPS) {
			free(ScoreList[iTemp]);
			ScoreList[iTemp] = NULL;
			break;
		}
	}
	UsersFound = iTemp;

	sprintf(szString, ST_LSCORES2, ScoreDate);
	rputs(szString);

	rputs(ST_LSCORES1);
	rputs(ST_LONGLINE);

	// show scores
	for (iTemp = 0; iTemp < UsersFound; iTemp++) {
		if (ScoreList[iTemp] == NULL)
			break;

		RemovePipes(ScoreList[iTemp]->Symbol, szString);
		Padding = 20 - strlen(szString);
		szPadding[0] = 0;
		for (iTemp2 = 0; iTemp2 < Padding; iTemp2++)
			strcat(szPadding, " ");

		sprintf(szString, ST_LSCORES3,
				ScoreList[iTemp]->szName,
				szPadding,
				ScoreList[iTemp]->Symbol,
				ScoreList[iTemp]->Points,
				IBBS.Data->Nodes[ ScoreList[iTemp]->ClanID[0]-1 ].Info.pszVillageName);
		rputs(szString);
	}

	if (UsersFound == 0)
		rputs(ST_LSCORES0);

	rputs(ST_LONGLINE);
	door_pause();

	// free 'em
	for (iTemp = 0; iTemp < MAX_USERS; iTemp++)
		if (ScoreList[iTemp])
			free(ScoreList[iTemp]);

	free(ScoreList);

	fclose(fp);
}

void RemoveFromIPScores(const int16_t ClanID[2])
{
	struct UserScore **ScoreList;
	int16_t iTemp;
	char ScoreDate[11];
	FILE *fp;

	fp = _fsopen("ipscores.dat", "rb", SH_DENYRW);

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

	fp = _fsopen("ipscores.dat", "wb", SH_DENYRW);

	// write date
	EncryptWrite(ScoreDate, 11, fp, XOR_IPS);

	// write them to file now and free them at the same time
	for (iTemp = 0; iTemp < MAX_USERS; iTemp++)
		if (ScoreList[iTemp]) {
			EncryptWrite_s(UserScore, ScoreList[iTemp], fp, XOR_IPS);
			free(ScoreList[iTemp]);
		}

	free(ScoreList);

	fclose(fp);
}

void SendScoreList(void)
{
	struct UserScore **ScoreList;
	struct Packet Packet;
	int16_t iTemp, NumScores, CurBBS;
	char ScoreDate[11];
	FILE *fp;

	fp = _fsopen("ipscores.dat", "rb", SH_DENYWR);

	if (!fp) {
		// no scorefile yet
		return;
	}

	// initialize the score data
	ScoreList = malloc(sizeof(struct UserScore *)*MAX_USERS);
	CheckMem(ScoreList);

	for (iTemp = 0; iTemp < MAX_USERS; iTemp++)
		ScoreList[iTemp] = NULL;

	EncryptRead(ScoreDate, 11, fp, XOR_IPS);

	for (iTemp = 0; iTemp < MAX_USERS; iTemp++) {
		ScoreList[iTemp] = malloc(sizeof(struct UserScore));
		CheckMem(ScoreList[iTemp]);
		notEncryptRead_s(UserScore, ScoreList[iTemp], fp, XOR_IPS) {
			free(ScoreList[iTemp]);
			ScoreList[iTemp] = NULL;
			break;
		}
	}
	NumScores = iTemp;

	if (NumScores == 0) {
		// No scores have been generated yet.
		free(ScoreList);
		fclose(fp);
		return;
	}

	fclose(fp);

	/* create packet header */
	Packet.Active = true;
	Packet.BBSIDFrom = IBBS.Data->BBSID;
	Packet.PacketType = PT_SCORELIST;
	strcpy(Packet.szDate, System.szTodaysDate);
	Packet.PacketLength = NumScores * BUF_SIZE_UserScore + sizeof(int16_t) +
						  sizeof(char)*11;
	strcpy(Packet.GameID, Game.Data->GameID);

	// send it to all bbses except this one in the league
	for (CurBBS = 0; CurBBS < MAX_IBBSNODES; CurBBS++) {
		if (IBBS.Data->Nodes[CurBBS].Active == false || CurBBS+1 == IBBS.Data->BBSID)
			continue;

		Packet.BBSIDTo = CurBBS+1;

		fp = _fsopen("tmp.$$$", "wb", SH_DENYRW);
		if (!fp)    return;

		// write packet header
		EncryptWrite_s(Packet, &Packet, fp, XOR_PACKET);

		// write how many scores
		EncryptWrite16(&NumScores, fp, XOR_PACKET);

		// write date
		EncryptWrite(ScoreDate, 11, fp, XOR_PACKET);

		// write scores
		for (iTemp = 0; iTemp < MAX_USERS; iTemp++)
			if (ScoreList[iTemp])
				EncryptWrite_s(UserScore, ScoreList[iTemp], fp, XOR_PACKET);

		fclose(fp);

		// send packet to BBS
		IBBS_SendPacketFile(Packet.BBSIDTo, "tmp.$$$");
		unlink("tmp.$$$");
	}

	// free 'em
	for (iTemp = 0; iTemp < MAX_USERS; iTemp++)
		if (ScoreList[iTemp])
			free(ScoreList[iTemp]);

	free(ScoreList);
}

void CreateScoreData(bool LocalOnly)
{
	struct UserScore **UserScores;
	struct clan *TmpClan;
	FILE *fpPlayerFile;
	int16_t ClanNum, iTemp, NumClans;

	UserScores = malloc(sizeof(struct UserScore *) * MAX_USERS);
	CheckMem(UserScores);

	// initialize scores to NULL pointers
	for (iTemp = 0; iTemp < MAX_USERS; iTemp++)
		UserScores[iTemp] = NULL;

	TmpClan = malloc(sizeof(struct clan));
	CheckMem(TmpClan);

	/* find guy in file */
	fpPlayerFile = _fsopen(ST_CLANSPCFILE, "rb", SH_DENYRW);
	if (!fpPlayerFile) {
		free(UserScores);
		free(TmpClan);
		return;
	}

	// set all members to NULLs for now
	for (iTemp = 0; iTemp < MAX_MEMBERS; iTemp++)
		TmpClan->Member[iTemp] = NULL;

	NumClans = 0;
	for (ClanNum = 0;; ClanNum++) {
		if (fseek(fpPlayerFile, (long)ClanNum * (BUF_SIZE_clan + 6L * BUF_SIZE_pc), SEEK_SET))
			break;

		notEncryptRead_s(clan, TmpClan, fpPlayerFile, XOR_USER)
			break;

		// skip if deleted
		if (TmpClan->ClanID[0] == -1)
			continue;

		// else, add to list
		UserScores[NumClans] = malloc(sizeof(struct UserScore));
		CheckMem(UserScores[NumClans]);
		strcpy(UserScores[NumClans]->szName, TmpClan->szName);
		UserScores[NumClans]->Points = TmpClan->Points;
		UserScores[NumClans]->BBSID = IBBS.Data->BBSID;
		UserScores[NumClans]->ClanID[0] = TmpClan->ClanID[0];
		UserScores[NumClans]->ClanID[1] = TmpClan->ClanID[1];
		strcpy(UserScores[NumClans]->Symbol, TmpClan->Symbol);
		NumClans++;
	}
	fclose(fpPlayerFile);

	//printf("%d clans found.\n", NumClans);

	// if not main BBS, write this data to file and send the packet
	if (IBBS.Data->BBSID != 1 && LocalOnly == false) {
		SendScoreData(UserScores);
	}
	else {
		ProcessScoreData(UserScores);
	}

	// free up mem used by score data
	for (iTemp = 0; iTemp < MAX_USERS; iTemp++)
		if (UserScores[iTemp]) {
			free(UserScores[iTemp]);
			UserScores[iTemp] = NULL;
		}

	free(TmpClan);
	free(UserScores);
}

