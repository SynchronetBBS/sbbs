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
#ifndef __FreeBSD__
#include <malloc.h>
#endif

#ifdef __unix__
#include "unix_wrappers.h"
#else
#include <share.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <OpenDoor.h>
#include "structs.h"
#include "language.h"
#include "mstrings.h"
#include "door.h"
#include "parsing.h"
#include "help.h"
#include "input.h"
#include "game.h"
#include "crc.h"
#include "myopen.h"
#include "items.h"
#include "mail.h"
#include "empire.h"
#include "system.h"
#include "quests.h"
#include "alliance.h"
#include "misc.h"
#include "video.h"
#include "news.h"
#include "ibbs.h"
#include "scores.h"
#include "trades.h"


struct user {
	BOOL Initialized;     // TRUE if user clan malloc'd
	BOOL UpdateUser;      // TRUE if user data should be written to file
} User = { FALSE, FALSE };

struct clan *PClan=NULL;
extern struct Language *Language;
extern struct config *Config;
extern struct game Game;
extern struct system System;
extern struct Spell *Spells[MAX_SPELLS];
extern struct PClass *PClasses[MAX_PCLASSES], *Races[MAX_PCLASSES];
extern struct village Village;
extern struct ibbs IBBS;


// ------------------------------------------------------------------------- //
BOOL Disbanded(void)
{
	// returns true if the user is in DISBAND.DAT
	char szUserName[36];
	FILE *fp;
	BOOL Found = FALSE;

	fp = _fsopen("disband.dat", "rb", SH_DENYWR);

	if (!fp)  return FALSE;

	for (;;) {
		if (!EncryptRead(szUserName, 36, fp, XOR_DISBAND)) break;

		if (stricmp(szUserName, od_control.user_name) == 0) {
			Found = TRUE;
			break;
		}
	}

	fclose(fp);

	return Found;
}


// ------------------------------------------------------------------------- //
void AddToDisband(void)
{
	// append user name to disband.dat
	FILE *fp;

	fp = _fsopen("disband.dat", "ab", SH_DENYRW);
	if (fp) {
		EncryptWrite(od_control.user_name, 36, fp, XOR_DISBAND);
		fclose(fp);
	}
}


// ------------------------------------------------------------------------- //
void DeleteClan(_INT16 ClanID[2], char *szClanName, BOOL Eliminate)
{
	FILE *fpOldPC, *fpNewPC, *OldMessage, *NewMessage;
	FILE *fpTradeFile;
	long OldOffset;
	char /*szFileName[40],*/ szString[128];
	_INT16 CurTradeData, iTemp, CurAlliance, CurMember;
	struct TradeData TradeData;
	struct clan *TmpClan;
	struct Message Message;
	BOOL FoundInPCFile = FALSE;   // set to true if he was ever on this board
	BOOL FoundNewCreator;
	struct Alliance *Alliances[MAX_ALLIANCES];


	AddToDisband();

	if (Game.Data->InterBBS) {
		// remove from list of clan names, remove from list of user names
		RemoveFromUList(ClanID);

		// remove from high scores list
		RemoveFromIPScores(ClanID);
	}

	// if this is the ruler of town, remove him
	if (ClanID[0] == Village.Data->RulingClanId[0] && ClanID[1] == Village.Data->RulingClanId[1]) {
		Village.Data->RulingClanId[0] = -1;
		Village.Data->RulingClanId[1] = -1;
		Village.Data->RulingDays = 0;
		Village.Data->GovtSystem = GS_DEMOCRACY;

		Village.Data->szRulingClan[0] = 0;
	}

	// go through PC file
	fpOldPC = _fsopen(ST_CLANSPCFILE, "rb", SH_DENYWR);
	if (fpOldPC) {
		fpNewPC = _fsopen(ST_NEWPCFILE, "w+b", SH_DENYWR);
		if (!fpNewPC) {
			System_Error("Can't write to new.pc\n");
		}

		/* allocate memory */
		TmpClan = (struct clan *) malloc(sizeof(struct clan));
		CheckMem(TmpClan);

		for (;;) {
			/* go through each clan and write his info to new file and
			   skip the clan in question if he is found */

			if (EncryptRead(TmpClan, sizeof(struct clan), fpOldPC, XOR_USER) == 0)
				break;

			/* if this is him */
			if (TmpClan->ClanID[0] == ClanID[0] && TmpClan->ClanID[1] == ClanID[1]) {
				FoundInPCFile = TRUE;

				if (Eliminate == FALSE) {
					// skip the guy since we're deleting him
					// skip 6 members
					fseek(fpOldPC, 6L*sizeof(struct pc), SEEK_CUR);
					continue;
				}
				else {
					//==== eliminated, might as well get rid of his allies
					// to clean things up
					// see if this clan is allied with him, if so, remove from allies list
					for (iTemp = 0; iTemp < MAX_ALLIES; iTemp++)
						TmpClan->Alliances[iTemp] = -1;
				}
			}

			// read in 6 members
			for (iTemp = 0; iTemp < 6; iTemp++) {
				TmpClan->Member[iTemp] = malloc(sizeof(struct pc));
				CheckMem(TmpClan->Member[iTemp]);
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
			EncryptWrite(TmpClan, sizeof(struct clan), fpNewPC, XOR_USER);

			for (iTemp = 0; iTemp < 6; iTemp++) {
				EncryptWrite(TmpClan->Member[iTemp], sizeof(struct pc), fpNewPC, XOR_PC);
				free(TmpClan->Member[iTemp]);
			}
		}

		/* deallocate memory for PCs */
		free(TmpClan);

		fclose(fpOldPC);
		fclose(fpNewPC);

		/* delete old file, rename new one */
		unlink(ST_CLANSPCFILE);
		rename(ST_NEWPCFILE, ST_CLANSPCFILE);
	}

	// go through msg file, set all his mail (to/from him) as deleted

	OldMessage = _fsopen("clans.msj", "rb", SH_DENYRW);
	if (OldMessage) {   // MSJ file exists, so go on
		NewMessage = _fsopen("tmp.$$$", "wb", SH_DENYRW);
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
		unlink("clans.msj");
		rename("tmp.$$$", "clans.msj");
	}

	// go through trades.dat, for each trade struct, see if trade is headed
	// for him, if so, set trade as aborted, return goods to user sending

	fpTradeFile = _fsopen("trades.dat", "r+b", SH_DENYRW);

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

			if (TradeData.ToClanID[0] == ClanID[0] && TradeData.ToClanID[1] == ClanID[1]) {
				rputs(">");
				// it's for this deleted player, so, rejecttrade
				RejectTrade(&TradeData);
				rputs("<");

				// write it to file
				fseek(fpTradeFile, OldOffset, SEEK_SET);
				EncryptWrite(&TradeData, sizeof(struct TradeData), fpTradeFile, XOR_TRADE);
			}
			else if (TradeData.FromClanID[0] == ClanID[0] &&
					 TradeData.FromClanID[1] == ClanID[1]) {
				rputs(".");
				// trade is coming from this player, remove it
				TradeData.Active = FALSE;
				fseek(fpTradeFile, OldOffset, SEEK_SET);
				EncryptWrite(&TradeData, sizeof(struct TradeData), fpTradeFile, XOR_TRADE);
			}
		}

		fclose(fpTradeFile);
	}


	// add message to news
	if (*szClanName && FoundInPCFile && Eliminate == FALSE &&
			PClan->Eliminated == FALSE) {
		sprintf(szString, "|0A ¯¯¯ |0CThe clan of |0D%s |0Chas disbanded!\n\n",
				szClanName);
		News_AddNews(szString);
	}
	else if (*szClanName && FoundInPCFile && Eliminate) {
		sprintf(szString, "|0A ¯¯¯ |0CThe clan of |0D%s |0Chas been eliminated!\n\n",
				szClanName);
		News_AddNews(szString);
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
		for (CurMember = 0; CurMember < MAX_ALLIANCEMEMBERS; CurMember++) {
			if (Alliances[CurAlliance] && Alliances[CurAlliance]->Member[CurMember][0] == ClanID[0] &&
					Alliances[CurAlliance]->Member[CurMember][1] == ClanID[1]) {
				Alliances[CurAlliance]->Member[CurMember][0] = -1;
				Alliances[CurAlliance]->Member[CurMember][1] = -1;
			}
		}
	}

	// deinit alliances and update to file
	UpdateAlliances(Alliances);

	// free up mem used by alliances
	for (CurAlliance = 0; CurAlliance < MAX_ALLIANCES; CurAlliance++)
		if (Alliances[CurAlliance])
			free(Alliances[CurAlliance]);
}


// ------------------------------------------------------------------------- //

BOOL ClanExists(_INT16 ClanID[2])
{
	FILE *fpPlayerFile;
	_INT16 CurClan/*, iTemp*/;
	long /*OldOffset,*/ Offset;
	struct clan *TmpClan;
	BOOL FoundClan = FALSE;

	TmpClan = malloc(sizeof(struct clan));
	CheckMem(TmpClan);

	fpPlayerFile = fopen(ST_CLANSPCFILE, "rb");
	if (!fpPlayerFile) {
		free(TmpClan);
		return FALSE;  /* failed to find clan */
	}

	for (CurClan = 0;; CurClan++) {
		/* go through file till you find clan he wants */

		Offset = (long)CurClan * ((long)sizeof(struct clan) + 6L*sizeof(struct pc));
		if (fseek(fpPlayerFile, Offset, SEEK_SET))
			break;  /* couldn't fseek, so exit */

		if (EncryptRead(TmpClan, sizeof(struct clan), fpPlayerFile, XOR_USER) == 0)
			break;  /* stop reading if no more players found */

		/* skip if deleted clan */
		if (TmpClan->ClanID[0] == -1)
			continue;

		/* if same Ids, found clan! */
		if (TmpClan->ClanID[0] == ClanID[0] && TmpClan->ClanID[1] == ClanID[1]) {
			FoundClan = TRUE;
			break;
		}
	}
	fclose(fpPlayerFile);

	free(TmpClan);

	return FoundClan;
}



// ------------------------------------------------------------------------- //

char GetStat(struct pc *PC, char Stat)
/*
 * Returns value of attribute specfied and takes into account weapons
 * and spells.
 */
{
	char StatValue;
	_INT16 iTemp;

	/* soon to contain other modifiers! */

	StatValue = PC->Attributes[(Stat + 0)];

	/* if other stats to add on to it, add it on here */
	if (PC->Weapon)
		StatValue += PC->MyClan->Items[ PC->Weapon - 1 ].Attributes[Stat + 0];
	if (PC->Shield)
		StatValue += PC->MyClan->Items[ PC->Shield - 1 ].Attributes[Stat + 0];
	if (PC->Armor)
		StatValue += PC->MyClan->Items[ PC->Armor - 1 ].Attributes[Stat + 0];

	/* add on spell modifiers here */
	for (iTemp = 0; iTemp < 10; iTemp++) {
		/* skip if no spell in that slot */
		if (PC->SpellsInEffect[iTemp].SpellNum == -1)
			continue;

		/* spell found, add it to modifiers */
		StatValue += Spells[ PC->SpellsInEffect[iTemp].SpellNum ]->Attributes[ Stat + 0 ];
	}

	return StatValue;
}

void ShowBaseStats(struct PClass *PClass)
/*
 * Shows base stats for a given class.  Used for creating new PCs.
 */
{
	char szFullString[128], szString[128];
	char *szAttributeNames[NUM_ATTRIBUTES];
	_INT16 SpellStrLength = 0;
	/* = {
	  "Agility",
	  "Dexterity",
	  "Strength",
	  "Wisdom",
	  "Armor Strength",
	  "Charisma"};
	*/
	_INT16 iTemp, NumSpellsKnown;
	BOOL AtLeastOneSpell = FALSE;

	// Load attribute names
	LoadStrings(10, 6, szAttributeNames);

	rputs("|07Base Stats\n\n");

	for (iTemp = 0; iTemp < NUM_ATTRIBUTES; iTemp++) {
		/* go through each one, listing it */
		sprintf(szFullString, "|0C%-20s ", szAttributeNames[iTemp]);

		if (PClass->Attributes[iTemp] < 0)
			strcat(szFullString, "|04");
		else if (PClass->Attributes[iTemp] == 0)
			strcat(szFullString, "|0C");
		else
			strcat(szFullString, "|15");

		sprintf(szString, "%-2d      ", PClass->Attributes[iTemp]);
		strcat(szFullString, szString);
		rputs(szFullString);

		if ((iTemp+1)%2 == 0)
			rputs("\n");
	}

	strcpy(szFullString, "|0CHitpoints            ");
	if (PClass->MaxHP < 0)
		strcat(szFullString, "|04");
	else if (PClass->MaxHP == 0)
		strcat(szFullString, "|0C");
	else
		strcat(szFullString, "|15");
	sprintf(szString, "%-3d     ", PClass->MaxHP);
	strcat(szFullString, szString);
	rputs(szFullString);

	strcpy(szFullString, "|0CSkill Points         ");
	if (PClass->MaxSP < 0)
		strcat(szFullString, "|04");
	else if (PClass->MaxSP == 0)
		strcat(szFullString, "|0C");
	else
		strcat(szFullString, "|15");
	sprintf(szString, "%-3d\n", PClass->MaxSP);
	strcat(szFullString, szString);
	rputs(szFullString);

	strcpy(szFullString, "|0CGold                 ");
	if (PClass->Gold < 0)
		strcat(szFullString, "|04");
	else if (PClass->Gold == 0)
		strcat(szFullString, "|0C");
	else
		strcat(szFullString, "|15");
	sprintf(szString, "%-3d\n", PClass->Gold);
	strcat(szFullString, szString);
	rputs(szFullString);

	rputs("\n");

	/* show spells known */
	rputs(ST_SPELLSKNOWN);

	NumSpellsKnown = 0;
	for (iTemp = 0; iTemp < MAX_SPELLS; iTemp++) {
		if (PClass->SpellsKnown[iTemp]) {
			if (AtLeastOneSpell) {
				rputs(", ");
				SpellStrLength += 2;
			}

			sprintf(szString, "%s", Spells[ PClass->SpellsKnown[iTemp] - 1]->szName);
			rputs(szString);
			SpellStrLength += strlen(szString);
			NumSpellsKnown++;

			AtLeastOneSpell = TRUE;

			if (SpellStrLength > 60) {
				rputs("\n");
				SpellStrLength = 0;
				AtLeastOneSpell = FALSE;
			}
		}
	}

	if (NumSpellsKnown == 0)
		rputs("|07None.");

	rputs("\n\n\n");
}

_INT16 GetClass(struct PClass *PClass[MAX_PCLASSES], char *szHelp)
/*
 * Returns a class as chosen by the user.  Used for creating new PCs.
 */
{
	_INT16 ClassChosen, iTemp;
	char szKeys[MAX_PCLASSES + 2], szString[128], Choice;

	/* so we have szKeys[] = "?ABCDEFGHI..." */
	szKeys[0] = '?';
	for (iTemp = 0; iTemp < MAX_PCLASSES; iTemp++)
		szKeys[iTemp + 1] = 'A' + iTemp;
	szKeys[iTemp + 1] = 0;

	rputs("\n");

	for (;;) {
		/* show options */
		for (iTemp = 0; iTemp < MAX_PCLASSES; iTemp++) {
			if (PClass[iTemp] == NULL) {
				szKeys[iTemp + 1] = 0;
				break;
			}

			sprintf(szString, ST_GETCLASS0, szKeys[iTemp + 1], PClass[iTemp]->szName);
			rputs(szString);
		}
		/* help [?] option */
		rputs(ST_GETCLASS1);

		/* get input -- this asks for Enter Choice */
		rputs(ST_GETCLASS2);

		Choice = od_get_answer(szKeys);

		if (Choice == '?') {
			rputs("Help\n");
			Help(szHelp, ST_RACESHLP);
			door_pause();
		}
		else {
			ClassChosen = Choice - 'A';
			rputs(PClass[ClassChosen]->szName);
			rputs("\n");

			/* show info */
			Help(PClass[ClassChosen]->szName, ST_RACESHLP);

			/* show base stats */
			ShowBaseStats(PClass[ClassChosen]);

			if (YesNo(ST_CHOOSECLASS) == YES)
				break;
		}
	}

	return ClassChosen;
}

void ChooseDefaultAction(struct pc *PC)
/*
 * This allows the user to choose a default action for the PC specified.
 */
{
	char *pszOptions[MAX_SPELLS + 1];       // add 1 for "Attack" command
	_INT16 iTemp, NumOptions, WhichOption;

	if (!PClan->DefActionHelp) {
		PClan->DefActionHelp = TRUE;
		Help("Default Action", ST_NEWBIEHLP);
	}

	for (iTemp = 0; iTemp < MAX_SPELLS+1; iTemp++)
		pszOptions[iTemp] = NULL;

	pszOptions[0] = "Attack";

	NumOptions = 1;

	for (iTemp = 0; iTemp < MAX_SPELLS; iTemp++) {
		if (PC->SpellsKnown[iTemp]) {
			pszOptions[iTemp + 1] = Spells[ PC->SpellsKnown[iTemp] - 1]->szName;
			NumOptions++;
		}
	}

	// choose one
	GetStringChoice(pszOptions, NumOptions, ST_DEFACTION2, &WhichOption,
					TRUE, DT_LONG, FALSE);

	if (WhichOption == 0)
		PC->DefaultAction = 0;
	else
		PC->DefaultAction = WhichOption + 9;
}


void ShowPlayerStats(struct pc *PC, BOOL AllowModify)
/*
 * Shows statistics for the given PC.
 */
{
	_INT16 iTemp, NumSpellsKnown, SpellNum;
	BOOL AtLeastOneSpell, NewLine, Done;
	_INT16 SpellStrLength = 0, CurAttr, CurSpell;
	char szString[255], cInput;
	long XPRequired[MAX_LEVELS];
	_INT16 Level;
	char *szAttrNames[NUM_ATTRIBUTES];

	// load attribute names
	LoadStrings(10, 6, szAttrNames);

	/* figure XP required per level */
	for (Level = 1; Level < MAX_LEVELS; Level++) {
		XPRequired[Level] = 50;

		for (iTemp = 1; iTemp <= Level; iTemp++)
			XPRequired[Level] += ((iTemp-1)*75);
	}

	if (AllowModify == FALSE)
		Done = TRUE;
	else
		Done = FALSE;

	do {
		SpellStrLength = 0;
		NewLine = FALSE;
		AtLeastOneSpell = FALSE;
		od_clr_scr();

		sprintf(szString, ST_P2STATS0, PC->szName);

		switch (PC->Status) {
			case Dead :
				strcat(szString, "|04(Dead)");
				break;
			case Unconscious :
				strcat(szString, "|04(Unconscious)");
				break;
			case Here :
				strcat(szString, "|0M(Alive)");
				break;
		}

		PadString(szString, 43);
		strcat(szString, ST_P2STATS1);
		rputs(szString);

		sprintf(szString, ST_P2STATS2, PC->Level);
		PadString(szString, 43);
		rputs(szString);

		if (PC->Weapon)
			sprintf(szString, ST_P2STATS3, PC->MyClan->Items[ PC->Weapon-1 ].szName);
		else
			sprintf(szString, ST_P2STATS3, "Nothing");
		rputs(szString);


		sprintf(szString, ST_P2STATS4, PC->Experience, XPRequired[ PC->Level + 1 ]-PC->Experience,
				PC->TrainingPoints);
		PadString(szString, 43);
		rputs(szString);

		if (PC->Armor)
			sprintf(szString, ST_P2STATS5, PC->MyClan->Items[ PC->Armor-1 ].szName);
		else
			sprintf(szString, ST_P2STATS5, "Nothing");
		rputs(szString);


		if (PC->WhichRace != -1) {
			sprintf(szString, ST_P2STATS6, Races[ PC->WhichRace ]->szName,
					PClasses[ PC->WhichClass ]->szName);
		}
		else {
			sprintf(szString, ST_P2STATS6, "Unknown", "Unknown");
		}
		PadString(szString, 43);
		rputs(szString);

		// shield
		if (PC->Shield)
			sprintf(szString, ST_P2STATS7, PC->MyClan->Items[ PC->Shield-1 ].szName);
		else
			sprintf(szString, ST_P2STATS7, "Nothing");
		rputs(szString);

		sprintf(szString, ST_P2STATS8, PC->HP, PC->MaxHP);
		rputs(szString);
		sprintf(szString, ST_P2STATS9, PC->SP, PC->MaxSP);
		rputs(szString);
		sprintf(szString, ST_P2STATS14, PC->TrainingPoints);
		rputs(szString);

		rputs(ST_P2STATS10);

		NumSpellsKnown = 0;
		for (iTemp = 0; iTemp < MAX_SPELLS; iTemp++) {
			if (PC->SpellsKnown[iTemp])
				NumSpellsKnown++;
		}

		for (CurAttr = 0, CurSpell = 0; CurAttr < NUM_ATTRIBUTES; CurAttr++) {
			sprintf(szString, "|0L %-19s |0M(%-2d) |0N%-2d",
					szAttrNames[CurAttr], PC->Attributes[CurAttr], GetStat(PC, (char)CurAttr));

			PadString(szString, 43);
			rputs(szString);

			// update spells shown
			if (CurAttr == 0 && NumSpellsKnown == 0)
				rputs("None\n");

			else {
				AtLeastOneSpell = FALSE;
				SpellStrLength = 0;

				while (CurSpell < MAX_SPELLS) {
					if (PC->SpellsKnown[CurSpell]) {
						if ((SpellStrLength + strlen(Spells[ PC->SpellsKnown[CurSpell] - 1]->szName)) > 27) {
							// don't display the current spell now, but wait
							// till next iteration
							break;
						}

						if (AtLeastOneSpell) {
							rputs(", ");
							SpellStrLength += 2;
						}
						AtLeastOneSpell = TRUE;


						sprintf(szString, "%s", Spells[ PC->SpellsKnown[CurSpell] - 1]->szName);
						rputs(szString);
						SpellStrLength += strlen(szString);

						if (SpellStrLength > 30) {
							// done!
							CurSpell++;
							break;
						}
					}

					CurSpell++;
				}
				rputs("\n");
			}
		}

		rputs(ST_P2STATS11);
		if (PC->DefaultAction == 0)
			rputs("Attack\n");
		else if (PC->DefaultAction >= 1) {
			SpellNum = PC->SpellsKnown[PC->DefaultAction - 10] - 1;

			sprintf(szString, "%s\n", Spells[SpellNum]->szName);
			rputs(szString);
		}

		rputs(ST_P2STATS12);

		if (AllowModify) {
			rputs(ST_P2STATS13);

			cInput = toupper(od_get_key(TRUE));

			if (cInput == 'C') {
				rputs("Change Default Acton\n\n");
				ChooseDefaultAction(PC);
			}
			else
				break;
		}

	}
	while (!Done);

}

void ListItems(struct clan *Clan)
/*
 * Lists Items for the given clan.
 */
{
	_INT16 iTemp, iTemp2, Length, LastItem = 0, FoundItem = FALSE;
	_INT16 CurItem;
	char szString[100], szOwner[30];

	// rputs("|0LNumber |0MItemName       |0N(Used by)\n\n");

	rputs("|0B # Name|07                 |0CUsed By|07      ");
	rputs("|0B # Name|07                 |0CUsed By|07\n");
	rputs("|0AÍÍÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÍÍ|07   ");
	rputs("|0AÍÍÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÍÍ|07\n");

	/* show all the items here */
	for (iTemp = 0, CurItem = 0; iTemp < MAX_ITEMS_HELD; iTemp++) {
		if (Clan->Items[iTemp].Available) {
			FoundItem = TRUE;
			/* see if owned */
			if (Clan->Items[iTemp].UsedBy) {
				strcpy(szOwner, Clan->Member[ Clan->Items[iTemp].UsedBy - 1]->szName);
				szOwner[10] = 0;

				sprintf(szString, "|0L%2d |0M%-20s |0N%s", iTemp+1, Clan->Items[iTemp].szName,
						szOwner);
			}
			else
				sprintf(szString, "|0L%2d |0M%-20s |0N", iTemp+1, Clan->Items[iTemp].szName);

#define LEN (37+9)    // add 9 for the | codes!

			/* add or remove spaces to filler it */
			if (strlen(szString) > LEN) {
				szString[LEN] = 0;
			}
			else {
				/* add spaces */
				szString[LEN] = 0;
				Length = strlen(szString);
				for (iTemp2 = LEN-1; iTemp2 >= Length; iTemp2--)
					szString[iTemp2] = ' ';
			}
			rputs(szString);
			if (CurItem%2 != 0)
				rputs("\n");
			CurItem++;

			LastItem = iTemp;
		}
	}
	if (FoundItem == FALSE)
		rputs(" |04No items in inventory.");

	if (LastItem%2 == 0)
		rputs("\n");

	rputs("\n");
}


void ItemEquipResults(struct pc *PC, struct item_data *Item, BOOL Equipping)
/*
 * This functions outputs results of equipping an item.
 */
{
	char *szVerb;
	char *szString, Gain,
	*szAttrNames[NUM_ATTRIBUTES];
	_INT16 CurStat;

	szString = MakeStr(128);
	LoadStrings(10, 6, szAttrNames);

	// go through each stat, say if gained or lost
	for (CurStat = 0; CurStat < NUM_ATTRIBUTES; CurStat++) {
		Gain = Item->Attributes[CurStat];

		if (Equipping == FALSE)
			Gain = -Gain;

		if (Gain < 0)
			szVerb = "|04decreases|03";
		else
			szVerb = "|11increases|03";

		if (Gain == 0)  // no change
			continue;

		// make it positive again to display it
		if (Gain < 0)
			Gain = -Gain;

		sprintf(szString, "|03%s %s by %d\n", szAttrNames[ CurStat], szVerb, Gain);
		rputs(szString);
	}

	free(szString);
}

void ItemStats(void)
/*
 * Shows item statistics for PClan.
 */
{
	/* modify item stats, assume it's the player */
	_INT16 ItemIndex, OneItemFound;
	_INT16 DefaultItemIndex, iTemp, WhoEquip;
	char szKeys[11], szString[100], /*szTemp[60],*/ szItemName[25];
	BOOL DoneEquipping, EquippedAlready;

	for (;;) {
		rputs(ST_ISTATS0);

		switch (od_get_answer("?XEDSLQ\r\n R")) {
			case 'R' :  // read book
				rputs("Read book\n");
				ReadBook();
				break;
			case '?' :
				rputs("Help\n\n");
				Help("Item Help", ST_MENUSHLP);
				break;
			case ' ' :
			case '\r' :
			case '\n' :
			case 'Q' :
				return;
			case 'X' :  /* examine item */
				rputs(ST_ISTATS1);

				/* see if anything to examine */
				for (iTemp = 0; iTemp < MAX_ITEMS_HELD; iTemp++) {
					if (PClan->Items[iTemp].Available)
						break;
				}
				if (iTemp == MAX_ITEMS_HELD) {
					rputs(ST_ISTATS2);
					break;
				}

				/* find first item in inventory */
				for (iTemp = 0; iTemp < MAX_ITEMS_HELD; iTemp++) {
					if (PClan->Items[iTemp].Available)
						break;
				}
				if (iTemp == MAX_ITEMS_HELD) {
					DefaultItemIndex = 1;
				}
				else
					DefaultItemIndex = iTemp+1;

				ItemIndex = (_INT16) GetLong(ST_ISTATS3, DefaultItemIndex, MAX_ITEMS_HELD);
				if (ItemIndex == 0)
					break;
				ItemIndex--;

				/* if that item is non-existant, tell him */
				if (PClan->Items[ItemIndex].Available == FALSE) {
					rputs(ST_ISTATS4);
					break;
				}
				ShowItemStats(&PClan->Items[ItemIndex], PClan);
				break;
			case 'L' :
				rputs(ST_ISTATS5);
				ListItems(PClan);
				break;
			case 'D' :  /* drop item */
				rputs(ST_ISTATS6);

				/* see if anything to drop */
				for (iTemp = 0; iTemp < MAX_ITEMS_HELD; iTemp++) {
					if (PClan->Items[iTemp].Available)
						break;
				}
				if (iTemp == MAX_ITEMS_HELD) {
					rputs(ST_ISTATS2);
					break;
				}

				/* find first item which is not equipped */
				for (iTemp = 0; iTemp < MAX_ITEMS_HELD; iTemp++) {
					if (PClan->Items[iTemp].Available &&
							PClan->Items[iTemp].UsedBy == 0)
						break;
				}
				if (iTemp == MAX_ITEMS_HELD) {
					DefaultItemIndex = 1;
				}
				else
					DefaultItemIndex = iTemp+1;

				ItemIndex = (_INT16) GetLong(ST_ISTATS7, DefaultItemIndex, MAX_ITEMS_HELD);
				if (ItemIndex == 0)
					break;
				ItemIndex--;

				/* if that item is non-existant, tell him */
				if (PClan->Items[ItemIndex].Available == FALSE) {
					rputs(ST_ISTATS4);
					break;
				}
				/* if that item is in use, tell him */
				if (PClan->Items[ItemIndex].UsedBy != 0) {
					rputs(ST_ISTATS8);
					break;
				}

				/* still wanna drop it? */
				sprintf(szString, ST_ISTATS9, PClan->Items[ItemIndex].szName);

				if (NoYes(szString) == YES) {
					/* drop it */
					sprintf(szString, ST_ISTATS10, PClan->Items[ItemIndex].szName);
					rputs(szString);

					PClan->Items[ItemIndex].szName[0] = 0;
					PClan->Items[ItemIndex].Available = FALSE;
				}
				break;
			case 'S' :
				rputs(ST_ISTATS11);

				/* find first item which is equipped */
				OneItemFound = FALSE;
				for (iTemp = 0; iTemp < MAX_ITEMS_HELD; iTemp++) {
					if (PClan->Items[iTemp].Available &&
							PClan->Items[iTemp].cType != I_SCROLL &&
							PClan->Items[iTemp].cType != I_BOOK)
						OneItemFound = TRUE;

					if (PClan->Items[iTemp].Available &&
							PClan->Items[iTemp].UsedBy != 0 &&
							PClan->Items[iTemp].cType != I_SCROLL &&
							PClan->Items[iTemp].cType != I_BOOK)
						break;
				}
				if (iTemp == MAX_ITEMS_HELD && OneItemFound == TRUE) {
					rputs(ST_ISTATS12);
					break;
				}
				else if (OneItemFound == FALSE) {
					rputs(ST_ISTATS2);
					break;
				}

				DefaultItemIndex = iTemp+1;

				ItemIndex = (_INT16) GetLong(ST_ISTATS13, DefaultItemIndex, MAX_ITEMS_HELD);
				if (ItemIndex == 0)
					break;
				ItemIndex--;

				/* if that item is non-existant, tell him */
				if (PClan->Items[ItemIndex].Available == FALSE) {
					rputs(ST_ISTATS4);
					break;
				}

				/* see if not equipped */
				if (PClan->Items[ItemIndex].UsedBy == 0) {
					rputs(ST_ISTATS14);
					break;
				}

				/* if equipped, take it away from whoever using it */
				switch (PClan->Items[ ItemIndex ].cType) {
					case I_WEAPON :
						sprintf(szString, ST_ISTATS15,
								PClan->Member[PClan->Items[ItemIndex].UsedBy-1]->szName, PClan->Items[ItemIndex].szName);
						rputs(szString);

						ItemEquipResults(PClan->Member[PClan->Items[ItemIndex].UsedBy-1], &PClan->Items[ItemIndex], FALSE);

						PClan->Member[PClan->Items[ItemIndex].UsedBy-1]->Weapon = 0;
						PClan->Items[ItemIndex].UsedBy = 0;
						break;
					case I_ARMOR :
						sprintf(szString, ST_ISTATS16,
								PClan->Member[PClan->Items[ItemIndex].UsedBy-1]->szName, PClan->Items[ItemIndex].szName);
						rputs(szString);

						ItemEquipResults(PClan->Member[PClan->Items[ItemIndex].UsedBy-1], &PClan->Items[ItemIndex], FALSE);

						PClan->Member[PClan->Items[ItemIndex].UsedBy-1]->Armor = 0;
						PClan->Items[ItemIndex].UsedBy = 0;
						break;
					case I_SHIELD :
						sprintf(szString, ST_ISTATS17,
								PClan->Member[PClan->Items[ItemIndex].UsedBy-1]->szName, PClan->Items[ItemIndex].szName);
						rputs(szString);

						ItemEquipResults(PClan->Member[PClan->Items[ItemIndex].UsedBy-1], &PClan->Items[ItemIndex], FALSE);

						PClan->Member[PClan->Items[ItemIndex].UsedBy-1]->Shield = 0;
						PClan->Items[ItemIndex].UsedBy = 0;
						break;
				}
				break;
			case 'E' :
				rputs(ST_ISTATS18);
				/* find first item which is not equipped yet */
				OneItemFound = FALSE;
				for (iTemp = 0; iTemp < MAX_ITEMS_HELD; iTemp++) {
					if (PClan->Items[iTemp].Available &&
							PClan->Items[iTemp].cType != I_SCROLL &&
							PClan->Items[iTemp].cType != I_BOOK)
						OneItemFound = TRUE;

					if (PClan->Items[iTemp].Available &&
							PClan->Items[iTemp].UsedBy == 0 &&
							PClan->Items[iTemp].cType != I_SCROLL &&
							PClan->Items[iTemp].cType != I_BOOK)
						break;
				}
				if (iTemp == MAX_ITEMS_HELD && OneItemFound == TRUE) {
					rputs(ST_ISTATS19);
					break;
				}
				else if (OneItemFound == FALSE) {
					rputs(ST_ISTATS2);
					break;
				}
				DefaultItemIndex = iTemp+1;

				ItemIndex = (_INT16) GetLong(ST_ISTATS20, DefaultItemIndex, MAX_ITEMS_HELD);
				if (ItemIndex == 0)
					break;
				ItemIndex--;

				/* if that item is non-existant, tell him */
				if (PClan->Items[ItemIndex].Available == FALSE) {
					rputs(ST_ISTATS4);
					break;
				}

				/* see if already equipped */
				if (PClan->Items[ItemIndex].UsedBy != 0) {
					rputs(ST_ISTATS21);
					break;
				}

				if (PClan->Items[ ItemIndex ].cType == I_SCROLL ||
						PClan->Items[ ItemIndex ].cType == I_BOOK ||
						PClan->Items[ ItemIndex ].cType == I_OTHER) {
					rputs("|04Can't equip that!\n");
					break;
				}

				DoneEquipping = FALSE;
				while (!DoneEquipping) {
					/* if not, ask who to equip it on */
					szKeys[0] = '\r';
					szKeys[1] = '\n';
					szKeys[2] = 0;
					rputs("\n");
					for (iTemp = 0; iTemp < 6; iTemp++) {
						EquippedAlready = FALSE;
						if (PClan->Member[iTemp]) {
							strcpy(szItemName, "Nothing");

							// see if equipped already
							switch (PClan->Items[ ItemIndex ].cType) {
								case I_WEAPON :
									if (PClan->Member[iTemp]->Weapon) {
										EquippedAlready = TRUE;
										strcpy(szItemName,
											   PClan->Items[PClan->Member[iTemp]->Weapon-1].szName);
									}
									break;
								case I_ARMOR :
									if (PClan->Member[iTemp]->Armor) {
										EquippedAlready = TRUE;
										strcpy(szItemName,
											   PClan->Items[PClan->Member[iTemp]->Armor-1].szName);
									}
									break;
								case I_SHIELD :
									if (PClan->Member[iTemp]->Shield) {
										EquippedAlready = TRUE;
										strcpy(szItemName,
											   PClan->Items[PClan->Member[iTemp]->Shield-1].szName);
									}
									break;
							}

							sprintf(szString, ST_ISTATS22,
									iTemp+'A',
									ItemPenalty(PClan->Member[iTemp], &PClan->Items[ItemIndex]) ? "|08" : "|0C",
									PClan->Member[iTemp]->szName,
									szItemName);

							// append race/class
							/* show race/class */
							/*
							              if (PClan->Member[iTemp]->WhichRace != -1)
							                sprintf(szTemp, "%s/%s\n",
							                  Races[PClan->Member[iTemp]->WhichRace]->szName,
							                  PClasses[PClan->Member[iTemp]->WhichClass]->szName);
							              else
							                strcpy(szTemp, "[unknown]\n");
							              strcat(szString, szTemp);
							*/

							rputs(szString);
							szKeys[ strlen(szKeys) +1 ] = 0;
							szKeys[ strlen(szKeys)] = iTemp + 'A';
						}
					}
					strcat(szKeys, "Q");
					rputs(ST_ISTATS23);


					sprintf(szString, ST_ISTATS24, PClan->Items[ItemIndex].szName);
					rputs(szString);

					WhoEquip = od_get_answer(szKeys);

					if (WhoEquip == '\r' || WhoEquip == 'Q' || WhoEquip == '\n') {
						rputs(ST_ABORTED);
						break;
					}
					else
						WhoEquip -= 'A';

					/* if that guy is not a permanent member, in future, say you
					   can't equip him */

					/* see if he has access to use that weapon, probably does :) */
					sprintf(szString, "|15%s\n\n", PClan->Member[WhoEquip]->szName);
					rputs(szString);

					if (ItemPenalty(PClan->Member[WhoEquip], &PClan->Items[ ItemIndex ])) {
						rputs("\n|04That player cannot use that item\n");
						continue;
					}


					/* see if that guy has something equipped, if so, tell user */
					switch (PClan->Items[ ItemIndex ].cType) {
						case I_WEAPON :
							if (PClan->Member[WhoEquip]->Weapon) {
								sprintf(szString, ST_ISTATS25,
										PClan->Items[ PClan->Member[WhoEquip]->Weapon-1].szName);
								if (YesNo(szString) == YES) {
									iTemp = PClan->Member[WhoEquip]->Weapon-1;
									sprintf(szString, ST_ISTATS15,
											PClan->Member[WhoEquip]->szName, PClan->Items[iTemp].szName);
									rputs(szString);

									ItemEquipResults(PClan->Member[WhoEquip], &PClan->Items[iTemp], FALSE);

									PClan->Member[WhoEquip]->Weapon = 0;
									PClan->Items[iTemp].UsedBy = 0;
									DoneEquipping = TRUE;
								}
								else {
									rputs(ST_ABORTED);
									break;
								}
							}

							sprintf(szString, ST_ISTATS26, PClan->Member[WhoEquip]->szName, PClan->Items[ItemIndex].szName);
							rputs(szString);

							PClan->Member[WhoEquip]->Weapon = ItemIndex + 1;
							PClan->Items[ItemIndex].UsedBy = WhoEquip + 1;
							// tell them what happened to his stats
							ItemEquipResults(PClan->Member[WhoEquip], &PClan->Items[ItemIndex], TRUE);
							DoneEquipping = TRUE;
							break;
						case I_ARMOR :
							if (PClan->Member[WhoEquip]->Armor) {
								sprintf(szString, ST_ISTATS27,
										PClan->Items[ PClan->Member[WhoEquip]->Armor-1].szName);
								if (YesNo(szString) == YES) {
									iTemp = PClan->Member[WhoEquip]->Armor-1;
									sprintf(szString, ST_ISTATS15,
											PClan->Member[WhoEquip]->szName, PClan->Items[iTemp].szName);
									rputs(szString);

									ItemEquipResults(PClan->Member[WhoEquip], &PClan->Items[iTemp], FALSE);

									PClan->Member[WhoEquip]->Armor = 0;
									PClan->Items[iTemp].UsedBy = 0;
									DoneEquipping = TRUE;
								}
								else {
									rputs(ST_ABORTED);
									break;
								}
							}
							sprintf(szString, ST_ISTATS28, PClan->Member[WhoEquip]->szName, PClan->Items[ItemIndex].szName);
							rputs(szString);

							PClan->Member[WhoEquip]->Armor = ItemIndex + 1;
							PClan->Items[ItemIndex].UsedBy = WhoEquip + 1;
							// tell them what happened to his stats
							ItemEquipResults(PClan->Member[WhoEquip], &PClan->Items[ItemIndex], TRUE);
							DoneEquipping = TRUE;
							break;
						case I_SHIELD :
							if (PClan->Member[WhoEquip]->Shield) {
								sprintf(szString, ST_ISTATS29,
										PClan->Items[ PClan->Member[WhoEquip]->Shield-1].szName);
								if (YesNo(szString) == YES) {
									iTemp = PClan->Member[WhoEquip]->Shield-1;
									sprintf(szString, ST_ISTATS15,
											PClan->Member[WhoEquip]->szName, PClan->Items[iTemp].szName);
									rputs(szString);

									ItemEquipResults(PClan->Member[WhoEquip], &PClan->Items[iTemp], FALSE);

									PClan->Member[WhoEquip]->Shield = 0;
									PClan->Items[iTemp].UsedBy = 0;
									DoneEquipping = TRUE;
								}
								else {
									rputs(ST_ABORTED);
									break;
								}
							}
							sprintf(szString, ST_ISTATS30, PClan->Member[WhoEquip]->szName, PClan->Items[ItemIndex].szName);
							rputs(szString);

							PClan->Member[WhoEquip]->Shield = ItemIndex + 1;
							PClan->Items[ItemIndex].UsedBy = WhoEquip + 1;
							// tell them what happened to his stats
							ItemEquipResults(PClan->Member[WhoEquip], &PClan->Items[ItemIndex], TRUE);
							DoneEquipping = TRUE;
							break;
					}
				}
				break;
		}
	}
}

_INT16 NumClansInVillage(void)
{
	FILE *fp;
	struct clan *TmpClan;
	_INT16 NumClans;

	fp = fopen(ST_CLANSPCFILE, "rb");
	if (!fp)
		return 0;

	TmpClan = malloc(sizeof(struct clan));
	CheckMem(TmpClan);

	for (NumClans = 0;;) {
		if (EncryptRead(TmpClan, sizeof(struct clan), fp, XOR_USER) == 0)
			break;

		if (TmpClan->ClanID[0] != -1)  /* means is active */
			NumClans++;

		/* skip his 6 PCs */
		fseek(fp, 6*sizeof(struct pc), SEEK_CUR);
	}


	fclose(fp);

	free(TmpClan);
	return (NumClans);
}

void ShowVillageStats(void)
{
	char szString[255], szRuler[25];

	od_clr_scr();
	if (Game.Data->InterBBS)
		sprintf(szString, ST_VSTATHEADER, IBBS.Data->Nodes[IBBS.Data->BBSID-1].Info.pszVillageName);
	else
		sprintf(szString, ST_VSTATHEADER, Village.Data->szName);
	rputs(szString);

	rputs(ST_LONGDIVIDER);

	if (Village.Data->szRulingClan[0] == 0)
		strcpy(szRuler, "None");
	else
		strcpy(szRuler, Village.Data->szRulingClan);

	sprintf(szString, ST_VSTATS1, NumClansInVillage(), szRuler);
	rputs(szString);

	sprintf(szString, ST_VSTATS2, Village.Data->TaxRate);
	rputs(szString);

	sprintf(szString, ST_VSTATS4, Village.Data->GST);
	rputs(szString);

	sprintf(szString, ST_VSTATS7, Village.Data->Empire.VaultGold);
	rputs(szString);

	sprintf(szString, ST_TMENUSTAT8, Village.Data->ConscriptionRate);
	rputs(szString);
	/*    sprintf(szString, ST_TMENUSTAT9, Village.Data->GovtSystem == GS_DEMOCRACY ?
	        "Democracy" : "Dictatorship");
	      rputs(szString);

	      sprintf(szString, ST_TMENUSTAT10, Village.Data->ShowEmpireStats ?
	        "Available" : "Unavailable");
	      rputs(szString);
	*/

	// == 0 means game in progress
	if (Game.Data->GameState == 0) {
		sprintf(szString, ST_VSTATS8, DaysBetween(Game.Data->szDateGameStart, System.szTodaysDate),
				Game.Data->szDateGameStart, "Disabled");
		rputs(szString);
	}
	else if (Game.Data->GameState == 1) {
		if (Game.Data->szDateGameStart[0] != 0) {
			sprintf(szString, ST_VSTATS9, Game.Data->szDateGameStart);
			rputs(szString);
		}
		else
			rputs(ST_VSTATS10);
	}

	rputs(ST_LONGDIVIDER);
	door_pause();
}


void ClanStats(struct clan *Clan, BOOL AllowModify)
/*
 * Shows stats for given Clan with option to modify values.
 */
{
	_INT16 iTemp, TotalItems, ItemsShown, Length, iTemp2;
	char szString[160], szStats[160];
	BOOL DoneLooking = FALSE;
	char szShortName[25], cKey;

	/* show stats for clan */

	while (DoneLooking == FALSE) {
		od_clr_scr();

		// sprintf(szString, ST_CSTATS0, Clan->szName, Clan->ClanID[0], Clan->ClanID[1]);
		// using color sprintf(szString, ST_CSTATS0, Clan->szName, Clan->Color, Clan->Symbol);
		sprintf(szString, ST_CSTATS0, Clan->szName, Clan->Symbol);
		rputs(szString);

		rputs(ST_LONGDIVIDER);
		sprintf(szString, ST_CSTATS1, Clan->szDateOfLastGame);
		rputs(szString);

		sprintf(szString, ST_CSTATS2, Clan->Empire.VaultGold);
		rputs(szString);

		sprintf(szString, ST_CSTATS3, Clan->MineLevel);
		rputs(szString);

		/* members */
		for (iTemp = 0; iTemp < MAX_MEMBERS; iTemp++) {
			if (Clan->Member[iTemp]) {
				strcpy(szShortName, Clan->Member[iTemp]->szName);
				szShortName[8] = 0;
				sprintf(szString, " |0P(|0Q%d|0P) |0L%-8s |0M%3d /%3d HP   ", iTemp+1, szShortName, Clan->Member[iTemp]->HP, Clan->Member[iTemp]->MaxHP);
				rputs(szString);

				/* show race/class */
				if (Clan->Member[iTemp]->WhichRace != -1)
					sprintf(szString, "|0N%s/%s  ", Races[Clan->Member[iTemp]->WhichRace]->szName, PClasses[Clan->Member[iTemp]->WhichClass]->szName);
				else
					strcpy(szString, "|0N[unknown]  ");

				/* add or remove spaces to filler it */
				if (strlen(szString) > 20) {
					szString[20] = 0;
				}
				else {
					/* add spaces */
					szString[20] = 0;
					Length = strlen(szString);
					for (iTemp2 = 19; iTemp2 >= Length; iTemp2--)
						szString[iTemp2] = ' ';
				}
				rputs(szString);

				/* show items that guys has, if any */
				TotalItems = 0;
				if (Clan->Member[iTemp]->Weapon)
					TotalItems++;
				if (Clan->Member[iTemp]->Armor)
					TotalItems++;
				if (Clan->Member[iTemp]->Shield)
					TotalItems++;

				if (TotalItems > 0) {
					strcpy(szStats, "|0O(");  /* begin stats */

					ItemsShown = 0;

					/* show first item */
					if (Clan->Member[iTemp]->Weapon) {
						sprintf(szString, "%s", Clan->Items[ Clan->Member[iTemp]->Weapon - 1].szName);
						strcat(szStats, szString);

						ItemsShown++;

						if (TotalItems > ItemsShown)
							strcat(szStats, ", ");
					}

					if (Clan->Member[iTemp]->Armor) {
						sprintf(szString, "%s", Clan->Items[ Clan->Member[iTemp]->Armor - 1].szName);
						strcat(szStats, szString);

						ItemsShown++;

						if (TotalItems > ItemsShown)
							strcat(szStats, ", ");
					}

					if (Clan->Member[iTemp]->Shield) {
						sprintf(szString, "%s", Clan->Items[ Clan->Member[iTemp]->Shield - 1].szName);
						strcat(szStats, szString);

						ItemsShown++;
					}
					/* truncate it */
					szStats[36] = 0;
					strcat(szStats, ")\n");

					rputs(szStats);
				}
				else
					rputs("|0O(Nothing equipped)\n");
			}
		}

		rputs("\n");

		/* show army stats */
		sprintf(szString, ST_CSTATS5, Clan->Empire.Army.Followers);
		rputs(szString);

		if (Clan->Protection) {
			sprintf(szString, "|0L This clan will remain in protection for %d more day(s)\n", Clan->Protection);
			rputs(szString);
		}

		rputs(ST_LONGDIVIDER);
		rputs(ST_CSTATS4);

		switch (cKey = od_get_answer("123456I\r\nQASV E")) {
			case '1' :  // first member
			case '2' :  // first member
			case '3' :  // first member
			case '4' :  // first member
			case '5' :  // first member
			case '6' :  // first member
				if (Clan->Member[ cKey - '1' ] == NULL)
					break;
				ShowPlayerStats(Clan->Member[ cKey - '1'], AllowModify);
				if (AllowModify == FALSE)
					door_pause();
				break;
			case 'E' :  // empire stats
				rputs("Empire\n\n");
				if (Game.Data->ClanEmpires == TRUE) {
					if (AllowModify == FALSE)
						rputs("You are unable to find any info on that.\n%P");
					else
						Empire_Stats(&Clan->Empire);
				}
				else
					rputs("Clan Empires disabled.\n%P");
				break;
			case ' ' :
			case 'Q' :
			case '\n' :
			case '\r' :
				DoneLooking = TRUE;
				break;
			case 'I' :
				rputs("Inventory\n\n");
				ListItems(Clan);
				if (AllowModify)
					ItemStats();
				else
					door_pause();
				break;
			case 'A' :  /* alliances */
				rputs("Allies\n\n");

				/* show alliances */
				ShowAlliances(Clan);
				break;
			case 'S' :  /* change symbol */
				rputs("Symbol\n\n");

				if (AllowModify == FALSE) {
					sprintf(szString, "|0GClan's current symbol is %s\n\n", Clan->Symbol);
					rputs(szString);
					door_pause();
					break;
				}

				if (Clan->Symbol[0] == 0) {
					rputs("|0SYour clan has not yet chosen a symbol\n\n");
					Help("Clan Symbol", ST_CLANSHLP);
					rputs("\n%P");
				}
				else {
					rputs("Clan Symbol\n\n");
					// sprintf(szString, "|0SYour current symbol is |%02d%s\n\n", Clan->Color, Clan->Symbol);
					sprintf(szString, "|0SYour current symbol is %s\n\n", Clan->Symbol);
					rputs(szString);
				}

				rputs("|0SPlease type your new symbol or press enter to keep it the same\n|0E> |0F");
				GetStr(Clan->Symbol, 20, TRUE);

				sprintf(szString, "|0SYour clan symbol is now %s\n\n", Clan->Symbol);
				rputs(szString);
				door_pause();
				break;
			case 'V' :  /* Village stats */
				ShowVillageStats();
				break;
		}
	}
	rputs("%C");
}


void PC_Create(struct pc *PC, BOOL ClanLeader)
/*
 * Used to create a new PC.
 */
{
	char szString[128];
	_INT16 iTemp, LastSpellSlot;

	for (;;) {
		// rputs("|11Please choose a race for this character.\n");
		rputs(ST_STUFF6);
		PC->WhichRace = GetClass(Races, "Races");
		// rputs("|11Please choose a class for this character.\n");
		rputs(ST_STUFF7);
		PC->WhichClass = GetClass(PClasses, "Classes");

		/* init stats */
		for (iTemp = 0; iTemp < NUM_ATTRIBUTES; iTemp++)
			PC->Attributes[iTemp] = Races[ PC->WhichRace ]->Attributes[iTemp];

		PC->MaxHP = Races[ PC->WhichRace ]->MaxHP;
		if (ClanLeader)
			PC->MaxHP += RANDOM(10);

		PC->MaxSP = Races[ PC->WhichRace ]->MaxSP;

		for (iTemp = 0; iTemp < NUM_ATTRIBUTES; iTemp++)
			PC->Attributes[iTemp] += PClasses[ PC->WhichClass ]->Attributes[iTemp];

		PC->MaxHP += PClasses[ PC->WhichClass ]->MaxHP;
		PC->MaxSP += PClasses[ PC->WhichClass ]->MaxSP;
		PC->Difficulty = -1;  // no difficulty

		/* randomize stats a bit */
		for (iTemp = 0; iTemp < NUM_ATTRIBUTES; iTemp++)
			PC->Attributes[iTemp] += RANDOM(1);

		// if clan leader, add on some random stats
		if (ClanLeader)
			for (iTemp = 0; iTemp < NUM_ATTRIBUTES; iTemp++)
				PC->Attributes[iTemp] += RANDOM(3);

		PC->MaxHP += RANDOM(5);
		PC->MaxSP += RANDOM(5);
		PC->HP = PC->MaxHP;
		PC->SP = PC->MaxSP;

		PC->Weapon = 0;
		PC->Armor = 0;
		PC->Shield = 0;
		PC->Status = Here;
		PC->Experience = 0;
		PC->Level = 0;
		PC->TrainingPoints = 0;

		/* no spells known */
		for (iTemp = 0; iTemp < MAX_SPELLS; iTemp++)
			PC->SpellsKnown[iTemp] = 0;

		/* "give" him spells from his race */
		for (iTemp = 0; iTemp < MAX_SPELLS; iTemp++) {
			if (Races[ PC->WhichRace ]->SpellsKnown[iTemp] == 0)
				break;

			PC->SpellsKnown[iTemp] = Races[ PC->WhichRace ]->SpellsKnown[iTemp];
		}
		LastSpellSlot = iTemp;
		for (iTemp = 0; iTemp < MAX_SPELLS; iTemp++) {
			if (PClasses[ PC->WhichClass ]->SpellsKnown[iTemp] == 0)
				break;

			PC->SpellsKnown[LastSpellSlot + iTemp] = PClasses[ PC->WhichClass ]->SpellsKnown[iTemp];
		}
		PC->DefaultAction = 0;

		/* show stats */
		// rputs("|13Stats generated for this player\n");
		rputs(ST_STUFF8);

		strcpy(PC->szName, "New Player");

		/* set all spells to 0 */
		for (iTemp = 0; iTemp < 10; iTemp++)
			PC->SpellsInEffect[iTemp].SpellNum = -1;

		ShowPlayerStats(PC, FALSE);

		/* are these ok? */
		if (YesNo("|03Use these stats?") == YES)
			break;
	}

	/* ask for name */
	for (;;) {
		// rputs("\n|02Enter the name of this member\n|10> |07");
		rputs(ST_STUFF9);

		szString[0] = 0;
		GetStr(szString, 12, FALSE);

		RemovePipes(szString, PC->szName);
		Strip(PC->szName);

		if (PC->szName[0] == 0)
			continue;
		else
			break;
	}

	PC->MyClan = PClan;
	PC->Undead = FALSE;
	PC->DefaultAction = 0;

	// sprintf(szString, |10%s joins the clan.\n%%P, PC->szName);
	sprintf(szString, ST_STUFF10, PC->szName);
	rputs(szString);
}


BOOL NameInUse(char *szName)
/*
 * Returns true if the given Clan name is already in use.
 */
{
	FILE *fpPCFile;
	struct clan *TmpClan;
	_INT16 CurClan = 0;
	long Offset;

	fpPCFile = fopen(ST_CLANSPCFILE, "rb");
	if (fpPCFile) {
		TmpClan = malloc(sizeof(struct clan));
		CheckMem(TmpClan);

		/* go through list */
		for (CurClan = 0;; CurClan++) {
			Offset = (long) CurClan * ((long)sizeof(struct clan) + 6L*sizeof(struct pc));
			if (fseek(fpPCFile, Offset, SEEK_SET))
				break;  /* couldn't fseek, so exit */

			if (EncryptRead(TmpClan, sizeof(struct clan), fpPCFile, XOR_USER) == 0)
				break;  /* stop reading if no more players found */

			/* see if this player has same name as user online */
			/* strip names */
			if (stricmp(szName, TmpClan->szName) == 0) {
				/* are the same, found another player with name, close file */
				fclose(fpPCFile);
				free(TmpClan);
				return TRUE;
			}
		}

		/* no players found with same name */
		fclose(fpPCFile);

		free(TmpClan);
	}

	// look through names list to see if that name is in use
	if (Game.Data->InterBBS && IBBS_InList(szName, TRUE)) {
		// found this clanname in the list of clannames of the league
		// don't allow him to use it
		return TRUE;
	}

	return FALSE;
}

BOOL ChooseClanName(char *szName)
/*
 * Allows user to choose a clan name.
 */
{
	char szString[30];

	for (;;) {
		// rputs("\n|02What will you call your clan?\n|10> |07");
		rputs(ST_STUFF3);
		szString[0] = 0;
		GetStr(szString, 24, FALSE);

		RemovePipes(szString, szName);
		Strip(szName);

		if (szName[0] == 0) {
			rputs(ST_ABORTED);
			return FALSE;
		}

		if (szName[0] == '?') {
			rputs("|12Invalid name!\n");
		}

		/* see if that name is in use */
		else if (NameInUse(szName) == FALSE) {
			rputs("|0F");
			rputs(szName);
			if (YesNo("|16|0E : |0SUse this name?") == YES) {
				return TRUE;
			}
			rputs("\n");
		}
		else {
			rputs("|12That name is in use, please choose again.\n\n");
		}
	}
}


void User_ResetHelp(void)
/*
 * Resets help database for clan
 */
{
	PClan->DefActionHelp = FALSE;
	PClan->CommHelp      = FALSE;
	PClan->MineHelp      = FALSE;
	PClan->MineLevelHelp = FALSE;
	PClan->CombatHelp    = FALSE;
	PClan->TrainHelp     = FALSE;
	PClan->MarketHelp    = FALSE;
	PClan->PawnHelp      = FALSE;
	PClan->WizardHelp    = FALSE;
	PClan->EmpireHelp    = FALSE;
	PClan->DevelopHelp   = FALSE;
}

BOOL User_Create(void)
/*
 * Creates a new clan for the player.
 */
{
	FILE *fpPlayerFile;
	struct pc *TmpPC;
	struct UserInfo User;
	_INT16 iTemp;
	char szString[128];

	strcpy(PClan->szUserName, od_control.user_name);

	Help("Welcome", ST_CLANSHLP);

	// (YesNo("|0SDo you wish to join this game of clans?") == NO)
	if (YesNo(ST_STUFF1) == NO) {
		rputs(ST_ABORTED);
		return FALSE;
	}

	if (!ChooseClanName(PClan->szName))
		return FALSE;


	TmpPC = malloc(sizeof(struct pc));
	CheckMem(TmpPC);


	// initialize clan data
	PClan->Symbol[0] = 0;

	for (iTemp = 0; iTemp < 8; iTemp++) {
		PClan->QuestsDone[iTemp] = 0;
		PClan->QuestsKnown[iTemp] = 0;
	}

	PClan->WorldStatus = WS_STAYING;
	PClan->DestinationBBS = -1;

	PClan->Points = 0;
	strcpy(PClan->szDateOfLastGame, System.szTodaysDate);

	// Reset help database for him
	User_ResetHelp();

	PClan->QuestToday = FALSE;
	PClan->VaultWithdrawals = 0;
	PClan->AttendedMass = FALSE;
	PClan->GotBlessing = FALSE;
	PClan->Prayed = FALSE;
	PClan->FightsLeft = Game.Data->MineFights;
	PClan->ClanFights = Game.Data->ClanFights;
	PClan->WasRulerToday = FALSE;
	PClan->ClanWars = 0;
	PClan->ChatsToday = 0;
	ClearFlags(PClan->PFlags);
	ClearFlags(PClan->DFlags);
	PClan->ResUncToday = 0;
	PClan->ResDeadToday = 0;
	PClan->MineLevel = 1;
	PClan->PublicMsgIndex = 0;
	PClan->MadeAlliance = FALSE;
	PClan->Eliminated = FALSE;
	PClan->WasRulerToday = FALSE;
	PClan->FirstDay = TRUE;
	PClan->Protection = Game.Data->DaysOfProtection;

	PClan->Empire.VaultGold = 0;
	PClan->TradesToday = 0;
	PClan->ClanRulerVote[0] = -1;
	PClan->ClanRulerVote[1] = -1;

	// initialize empire

	// set up no alliances
	for (iTemp = 0; iTemp < MAX_ALLIES; iTemp++)
		PClan->Alliances[iTemp] = -1;

	/* NULL-pointer each member of clan for now */
	for (iTemp = 0; iTemp < MAX_MEMBERS; iTemp++)
		PClan->Member[iTemp] = NULL;

	for (iTemp = 0; iTemp < MAX_ITEMS_HELD; iTemp++)
		PClan->Items[iTemp].Available = FALSE;


	// Figure out ID
	PClan->ClanID[0] = Config->BBSID;
	PClan->ClanID[1] = Game.Data->NextClanID;
	Game.Data->NextClanID++;

	// init empire
	Empire_Create(&PClan->Empire, TRUE);
	strcpy(PClan->Empire.szName, PClan->szName);
	PClan->Empire.OwnerType = EO_CLAN;
	PClan->Empire.AllianceID = -1;

	// Create the players
	for (iTemp = 0; iTemp < Game.Data->MaxPermanentMembers; iTemp++) {
		od_clr_scr();

		// creating clansmen x of
		sprintf(szString, ST_STUFF5, iTemp + 1, Game.Data->MaxPermanentMembers);
		rputs(szString);

		if (iTemp == 0) {
			/* first clansman is the "leader" of the four */
			Help("Clan Leader", ST_NEWBIEHLP);
			rputs("\n%P");
		}

		if (iTemp == 0)
			PC_Create(TmpPC, TRUE);
		else
			PC_Create(TmpPC, FALSE);


		PClan->Member[iTemp] = malloc(sizeof(struct pc));
		CheckMem(PClan->Member[iTemp]);
		*PClan->Member[iTemp] = *TmpPC;

		// give 'em gold dude
		PClan->Empire.VaultGold  += PClasses[ TmpPC->WhichClass ]->Gold;
		PClan->Empire.VaultGold += Races[ TmpPC->WhichRace ]->Gold;
	}

	/* open player file for append */
	fpPlayerFile = _fsopen(ST_CLANSPCFILE, "r+b", SH_DENYRW);
	if (!fpPlayerFile) {
		fpPlayerFile = _fsopen(ST_CLANSPCFILE, "wb", SH_DENYRW);
		if (!fpPlayerFile) {
			// !!!
			// DisplayStr("User_Create: creating new file\n");
			/* !!! */
			rputs("!! Chkpt 1\n");
			rputs(ST_ERRORPC);
			free(TmpPC);
			return FALSE;
		}
	}
	else {
		// !!!
		// DisplayStr("User_Create: appending to file\n");
		fseek(fpPlayerFile, 0L, SEEK_END);
	}

	// figure out CRC first
	PClan->CRC = CRCValue(PClan, sizeof(struct clan) - sizeof(long));

	/* write it to file */
	EncryptWrite(PClan, sizeof(struct clan), fpPlayerFile, XOR_USER);
	for (iTemp = 0; iTemp < Game.Data->MaxPermanentMembers; iTemp++) {
		PClan->Member[iTemp]->CRC = CRCValue(PClan->Member[iTemp], sizeof(struct pc) - sizeof(long));

		EncryptWrite(PClan->Member[iTemp], sizeof(struct pc), fpPlayerFile, XOR_PC);
	}

	/* write null players to complete it */
	TmpPC->szName[0] = 0;
	TmpPC->Status = Dead;
	for (iTemp = Game.Data->MaxPermanentMembers; iTemp < 6; iTemp++) {
		TmpPC->CRC = CRCValue(TmpPC, sizeof(struct pc) - sizeof(long));

		EncryptWrite(TmpPC, sizeof(struct pc), fpPlayerFile, XOR_PC);
	}

	fclose(fpPlayerFile);

	// !!!
	// DisplayStr("User_Create: done writing to file\n%P");

	free(TmpPC);

	/* write news */
	sprintf(szString, ST_NEWSNEWCLAN, PClan->szName);
	News_AddNews(szString);


	// add to league
	if (Game.Data->InterBBS) {
		User.ClanID[0] = PClan->ClanID[0];
		User.ClanID[1] = PClan->ClanID[1];
		strcpy(User.szMasterName, PClan->szUserName);
		strcpy(User.szName, PClan->szName);
		User.Deleted = FALSE;
		IBBS_LeagueNewUser(&User);
	}



	return TRUE;
}


// ------------------------------------------------------------------------- //

void User_Destroy(void)
/*
 * Destroys the clan in memory.
 */
{
	_INT16 iTemp;

	for (iTemp = 0; iTemp < MAX_MEMBERS; iTemp++)
		if (PClan->Member[iTemp])
			free(PClan->Member[iTemp]);

	free(PClan);
	User.Initialized = FALSE;
}

// ------------------------------------------------------------------------- //

void CopyPC(struct pc *PCDest, struct pc *PCSrc)
{
	_INT16 iTemp;

	strcpy(PCDest->szName, PCSrc->szName);
	PCDest->HP = PCSrc->HP;
	PCDest->SP = PCSrc->SP;
	PCDest->MaxHP = PCSrc->MaxHP;
	PCDest->MaxSP = PCSrc->MaxSP;

	/* copy all attributes */
	for (iTemp = 0; iTemp < NUM_ATTRIBUTES; iTemp++)
		PCDest->Attributes[iTemp] = PCSrc->Attributes[iTemp];

	PCDest->Status = PCSrc->Status;
	PCDest->Weapon = PCSrc->Weapon;
	PCDest->Shield = PCSrc->Shield;
	PCDest->Armor = PCSrc->Armor;
	PCDest->MyClan = PCSrc->MyClan;
	PCDest->WhichRace = PCSrc->WhichRace;
	PCDest->WhichClass = PCSrc->WhichClass;
	PCDest->Experience = PCSrc->Experience;
	PCDest->Level = PCSrc->Level;
	PCDest->TrainingPoints = PCSrc->TrainingPoints;
	PCDest->Difficulty = PCSrc->Difficulty;
	PCDest->Undead = PCSrc->Undead;
	PCDest->DefaultAction = PCSrc->DefaultAction;

	PCDest->CRC = PCSrc->CRC;

	for (iTemp = 0; iTemp < MAX_SPELLS; iTemp++)
		PCDest->SpellsKnown[iTemp] = PCSrc->SpellsKnown[iTemp];

	for (iTemp = 0; iTemp < 10; iTemp++)
		PCDest->SpellsInEffect[iTemp] = PCSrc->SpellsInEffect[iTemp];
}

BOOL User_Read(void)
/*
 * Reads the PClan from file which corresponds to the user online.
 * Returns FALSE if clan was not found (i.e. not in game yet)
 */
{
	// search for user which matches current user logged in

	FILE *fpPlayerFile;
	struct clan *TmpClan;
	struct pc *TmpPC;
	_INT16 CurClan, CurMember, iTemp;
	long Offset;

	fpPlayerFile = _fsopen(ST_CLANSPCFILE, "rb", SH_DENYRW);
	if (!fpPlayerFile) {
		/* file not found, so clan not found! */
		return FALSE;
	}

	TmpClan = malloc(sizeof(struct clan));
	CheckMem(TmpClan);
	TmpPC = malloc(sizeof(struct pc));
	CheckMem(TmpPC);

	for (CurClan = 0;; CurClan++) {
		/* seek to the current player */
		Offset = (long) CurClan * ((long)sizeof(struct clan) + 6L*sizeof(struct pc));

		if (fseek(fpPlayerFile, Offset, SEEK_SET))
			break;  /* couldn't fseek, so exit */

		if (EncryptRead(TmpClan, sizeof(struct clan), fpPlayerFile, XOR_USER) == 0)
			break;  /* stop reading if no more players found */

		// !!!
		// DisplayStr("Read in "); DisplayStr(TmpClan->szUserName); DisplayStr("\n");
		// getch();

		/* see if this player has same name as user online */
		if (stricmp(od_control.user_name, TmpClan->szUserName) == 0) {

			/* are the same, found player, copy it, return TRUE */
			*PClan = *TmpClan;

			/* read in the other guys, members, etc. */
			for (CurMember = 0; CurMember < MAX_MEMBERS; CurMember++)
				PClan->Member[CurMember] = NULL;

			for (CurMember = 0; CurMember < 6; CurMember++) {
				/* read 'em in */
				EncryptRead(TmpPC, sizeof(struct pc), fpPlayerFile, XOR_PC);

				// [0] != 0 if member exists
				if (TmpPC->szName[0]) {
					PClan->Member[CurMember] = malloc(sizeof(struct pc));
					CheckMem(PClan->Member[CurMember]);

					// CopyPC(PClan->Member[CurMember], TmpPC);
					*PClan->Member[CurMember] = *TmpPC;
					PClan->Member[CurMember]->MyClan = PClan;

					/* set all spells to 0 */
					for (iTemp = 0; iTemp < 10; iTemp++)
						PClan->Member[CurMember]->SpellsInEffect[iTemp].SpellNum = -1;

				}
			}

			fclose(fpPlayerFile);
			free(TmpClan);
			free(TmpPC);
			return TRUE;
		}

	}

	/* was not found, so return FALSE) */
	fclose(fpPlayerFile);

	free(TmpClan);
	free(TmpPC);

	return FALSE;
}

// ------------------------------------------------------------------------- //

void Clan_Update(struct clan *Clan)
/*
 * Writes Clan to file in the appropriate place.
 *
 * PRE: Clan MUST already reside in the .PC file!
 */
{
	FILE *fpPlayerFile;
	_INT16 CurClan, iTemp;
	long OldOffset, Offset;
	struct clan *TmpClan;
	struct pc *TmpPC;

	TmpClan = malloc(sizeof(struct clan));
	CheckMem(TmpClan);
	TmpPC = malloc(sizeof(struct pc));
	CheckMem(TmpPC);

	fpPlayerFile = fopen(ST_CLANSPCFILE, "r+b");
	if (!fpPlayerFile) {
		/* !! */
		rputs("!! Chkpt 2\n");
		rputs(ST_ERRORPC);
		free(TmpClan);
		free(TmpPC);
		return;  /* failed to find clan */
	}

	for (CurClan = 0;; CurClan++) {
		/* go through file till you find clan he wants */

		Offset = (long)CurClan * (sizeof(struct clan) + 6L*sizeof(struct pc));
		if (fseek(fpPlayerFile, Offset, SEEK_SET))
			break;  /* couldn't fseek, so exit */

		OldOffset = ftell(fpPlayerFile);

		if (EncryptRead(TmpClan, sizeof(struct clan), fpPlayerFile, XOR_USER) == 0)
			break;  /* stop reading if no more players found */

		/* skip if deleted clan */
		if (TmpClan->ClanID[0] == -1)
			continue;

		/* if same Ids, seek back and write to file */
		if (TmpClan->ClanID[0] == Clan->ClanID[0] &&
				TmpClan->ClanID[1] == Clan->ClanID[1]) {
			fseek(fpPlayerFile, OldOffset, SEEK_SET);

			Clan->CRC = CRCValue(Clan, sizeof(struct clan) - sizeof(long));
			EncryptWrite(Clan, sizeof(struct clan), fpPlayerFile, XOR_USER);

			// fwrite(Clan, sizeof(struct clan), 1, fpPlayerFile);

			// fwrite players
			TmpPC->szName[0] = 0;
			TmpPC->Status = Dead;
			TmpPC->CRC = CRCValue(TmpPC, sizeof(struct pc) - sizeof(long));
			for (iTemp = 0; iTemp < 6; iTemp++) {
				if (Clan->Member[iTemp] && Clan->Member[iTemp]->Undead == FALSE) {
					Clan->Member[iTemp]->CRC = CRCValue(Clan->Member[iTemp], sizeof(struct pc) - sizeof(long));
					EncryptWrite(Clan->Member[iTemp], sizeof(struct pc), fpPlayerFile, XOR_PC);

					// fwrite(Clan->Member[iTemp], sizeof(struct pc), 1, fpPlayerFile);
				}
				else
					EncryptWrite(TmpPC, sizeof(struct pc), fpPlayerFile, XOR_PC);
				// fwrite(TmpPC, sizeof(struct pc), 1, fpPlayerFile);
			}
			break;
		}
	}
	fclose(fpPlayerFile);

	free(TmpPC);
	free(TmpClan);

}


void User_Write(void)
/*
 * The current user online is written to file.
 *
 * PRE: User has been written to file already once.
 */
{

	// updates the current user information to file
	if (User.Initialized) {
		Clan_Update(PClan);
	}
}

// ------------------------------------------------------------------------- //

void User_FirstTimeToday(void)
/*
 * Run whenever user plays first time that day.
 *
 */
{
	_INT16 iTemp;

	PClan->Points += 25;

	for (iTemp = 0; iTemp < MAX_MEMBERS; iTemp++) {
		if (PClan->Member[iTemp] && PClan->Member[iTemp]->Status != Dead) {
			PClan->Member[iTemp]->HP = PClan->Member[iTemp]->MaxHP;
			PClan->Member[iTemp]->SP = PClan->Member[iTemp]->MaxSP;

			if (PClan->Member[iTemp]->Status == Unconscious)
				PClan->Member[iTemp]->Status = Here;
		}
	}

	strcpy(PClan->szDateOfLastGame, System.szTodaysDate);
}

// ------------------------------------------------------------------------- //

_INT16 NumMembers(struct clan *Clan, BOOL OnlyOnesHere)
/*
 * Returns the number of members in the clan.
 *
 * PRE: OnlyOnesHere is TRUE if only the alive members are to be checked.
 */
{
	_INT16 iTemp;
	_INT16 Members = 0;

	for (iTemp = 0; iTemp < MAX_MEMBERS; iTemp++) {
		if (Clan->Member[iTemp]) {
			if (OnlyOnesHere && Clan->Member[iTemp]->Status != Here)
				continue;

			Members++;
		}
	}

	return Members;
}

// ------------------------------------------------------------------------- //

BOOL GetClanID(_INT16 ID[2], BOOL OnlyLiving, BOOL IncludeSelf,
			   _INT16 WhichAlliance, BOOL InAllianceOnly)
{
	FILE *fpPlayerFile;
	/*char szFragName[30], szString[128];*/
	char *apszClanNames[50];
	_INT16 CurClan, ClanIDs[50][2], WhichClan, NumClans, *ClanIndex, iTemp;
	BOOL AllianceFound;
	struct clan *TmpClan;
	struct pc *TmpPC;
	BOOL FoundClan = FALSE, AtLeastOneLiving = FALSE;

	TmpClan = malloc(sizeof(struct clan));
	CheckMem(TmpClan);

	for (CurClan = 0; CurClan < 50; CurClan++)
		apszClanNames[CurClan] = NULL;

	ClanIndex = malloc(sizeof(_INT16)*50);
	CheckMem(ClanIndex);

	// get list of all clan names from file, write to
	NumClans = 0;
	for (CurClan = 0; CurClan < 50; CurClan++) {
		fpPlayerFile = _fsopen(ST_CLANSPCFILE, "rb", SH_DENYRW);
		if (!fpPlayerFile) {
			/* !! */
			rputs("!! Chkpt 3\n");
			rputs(ST_ERRORPC);
			break;
		}
		if (fseek(fpPlayerFile, (long)CurClan *(sizeof(struct clan) + 6L*sizeof(struct pc)), SEEK_SET)) {
			fclose(fpPlayerFile);
			break;  /* couldn't fseek, so exit */
		}

		if (EncryptRead(TmpClan, sizeof(struct clan), fpPlayerFile, XOR_USER) == 0) {
			fclose(fpPlayerFile);
			break;  /* stop reading if no more players found */
		}

		// see if any of the members are alive
		if (OnlyLiving) {
			TmpPC = malloc(sizeof(struct pc));
			CheckMem(TmpPC);

			AtLeastOneLiving = FALSE;
			for (iTemp = 0; iTemp < 6; iTemp++) {
				EncryptRead(TmpPC, sizeof(struct pc), fpPlayerFile, XOR_PC);

				if (TmpPC->szName[0] && TmpPC->Status == Here) {
					AtLeastOneLiving = TRUE;
					break;
				}
			}
			free(TmpPC);
		}

		fclose(fpPlayerFile);

		/* skip if deleted clan or eliminated */
		if (TmpClan->ClanID[0] == -1)
			continue;

		/* skip if your clan */
		if (IncludeSelf == FALSE &&
				TmpClan->ClanID[0] == PClan->ClanID[0] &&
				TmpClan->ClanID[1] == PClan->ClanID[1])
			continue;

		/* skip if not in alliance and alliance used */
		if (WhichAlliance != -1) {
			AllianceFound = FALSE;
			for (iTemp = 0; iTemp < MAX_ALLIES; iTemp++) {
				if (TmpClan->Alliances[iTemp] == WhichAlliance) {
					AllianceFound = TRUE;
					break;
				}
			}
			// not in alliance so skip AND we want in alliance only
			if (!AllianceFound && InAllianceOnly)
				continue;

			// skip also if we don't want those in the alliance to be listed
			else if (AllianceFound && !InAllianceOnly)
				continue;
		}

		// skip if no one alive
		if (AtLeastOneLiving == FALSE && OnlyLiving)
			continue;

		apszClanNames[NumClans] = malloc(strlen(TmpClan->szName) + 1);
		CheckMem(apszClanNames[NumClans]);
		strcpy(apszClanNames[NumClans], TmpClan->szName);

		// add to our list
		ClanIDs[NumClans][0] = TmpClan->ClanID[0];
		ClanIDs[NumClans][1] = TmpClan->ClanID[1];

		NumClans++;
	}

	if (NumClans != 0)
		GetStringChoice(apszClanNames, NumClans,
						"|0SEnter the name of the clan\n|0E> |0F",
						&WhichClan, TRUE, DT_WIDE, TRUE);
	else {
		rputs("No clans found!\n");
		WhichClan = -1;
	}

	if (WhichClan == -1) {
		FoundClan = FALSE;
		rputs(ST_ABORTED);
	}
	else {
		FoundClan = TRUE;
		ID[0] = ClanIDs[ WhichClan ][0];
		ID[1] = ClanIDs[ WhichClan ][1];
	}

	for (CurClan = 0; CurClan < 50; CurClan++) {
		// free up clan names
		if (apszClanNames[CurClan])
			free(apszClanNames[CurClan]);
	}

	free(TmpClan);
	free(ClanIndex);

	return FoundClan;
}

// ------------------------------------------------------------------------- //

BOOL GetClanNameID(char *szName, _INT16 ID[2])
{
	FILE *fpPlayerFile;
	/*    char szString[255];*/
	_INT16 CurClan = 0;
	BOOL FoundClan = FALSE;
	struct clan *TmpClan;

	szName[0] = 0;

	TmpClan = malloc(sizeof(struct clan));
	CheckMem(TmpClan);

	for (;;) {
		/* go through file till you find clan he wants */

		fpPlayerFile = _fsopen(ST_CLANSPCFILE, "rb", SH_DENYRW);
		if (!fpPlayerFile) {
			free(TmpClan);
			return FALSE;  /* means failed to find clan */
		}
		if (fseek(fpPlayerFile, (long)CurClan *(sizeof(struct clan) + 6L*sizeof(struct pc)), SEEK_SET)) {
			fclose(fpPlayerFile);
			break;  /* couldn't fseek, so exit */
		}

		if (EncryptRead(TmpClan, sizeof(struct clan), fpPlayerFile, XOR_USER) == 0) {
			fclose(fpPlayerFile);
			break;  /* stop reading if no more players found */
		}

		fclose(fpPlayerFile);

		/* see if name is the one */
		if (TmpClan->ClanID[0] == ID[0] && TmpClan->ClanID[1] == ID[1]) {
			/* found it! */
			strcpy(szName, TmpClan->szName);
			FoundClan = TRUE;
			break;
		}

		CurClan++;
	}

	free(TmpClan);
	return FoundClan;
}



// ------------------------------------------------------------------------- //
BOOL GetClan(_INT16 ClanID[2], struct clan *TmpClan)
{
	FILE *fpPlayerFile;
	_INT16 ClanNum, iTemp;
	char szFileName[50];
	BOOL FoundClan = FALSE;

	// make them all NULLs for safety
	for (iTemp = 0; iTemp < MAX_MEMBERS; iTemp++)
		TmpClan->Member[iTemp] = NULL;

	strcpy(szFileName, ST_CLANSPCFILE);

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

		if (!EncryptRead(TmpClan, sizeof(struct clan), fpPlayerFile, XOR_USER)) {
			fclose(fpPlayerFile);
			for (iTemp = 0; iTemp < MAX_MEMBERS; iTemp++)
				TmpClan->Member[iTemp] = NULL;
			return FALSE;
		}

		// if CRCs don't match, warn for now
//    if (TmpClan->CRC != CRCValue(TmpClan, sizeof(TmpClan)) )
//      DisplayStr("|12-> CRCs don't match!!\n");

		// make them all NULLs for safety
		for (iTemp = 0; iTemp < MAX_MEMBERS; iTemp++)
			TmpClan->Member[iTemp] = NULL;

		if (TmpClan->ClanID[0] == ClanID[0] && TmpClan->ClanID[1] == ClanID[1]) {
			/* found it */
			FoundClan = TRUE;

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

	if (FoundClan)
		return TRUE;
	else {
		// DisplayStr("Bug #2\n%P");
		return FALSE;
	}
}

// ------------------------------------------------------------------------- //

void User_List(void)
{
	/* /USER parameter -- displays user list */

	FILE *fpPlayerFile, *fpUserList;
	struct clan *TmpClan;
	_INT16 CurClan/*, CurMember, iTemp*/;
	struct UserInfo User;
	long Offset;

	printf(ST_USERLISTH);

	if (Game.Data->InterBBS == FALSE) {
		fpPlayerFile = _fsopen(ST_CLANSPCFILE, "rb", SH_DENYWR);
		if (!fpPlayerFile) {
			/* file not found, so clan not found! */
			return;
		}

		TmpClan = malloc(sizeof(struct clan));
		CheckMem(TmpClan);

		/* else, file opened properly, so look for player */

		// list local players
		for (CurClan = 0;; CurClan++) {
			/* seek to the current player */
			Offset = (long)CurClan * (sizeof(struct clan) + 6L*sizeof(struct pc));
			if (fseek(fpPlayerFile, Offset, SEEK_SET))
				break;  /* couldn't fseek, so exit */

			if (EncryptRead(TmpClan, sizeof(struct clan), fpPlayerFile, XOR_USER) == 0)
				break;  /* stop reading if no more players found */

			// print out name etc.
			printf("%2d|%2d %-20s  %-20s\n", TmpClan->ClanID[0], TmpClan->ClanID[1],
				   TmpClan->szUserName, TmpClan->szName);
		}

		/* was not found, so return FALSE) */
		fclose(fpPlayerFile);

		free(TmpClan);
		TmpClan = NULL;
	}
	else {
		// list interbbs users using USERLIST.DAT
		fpUserList = _fsopen("userlist.dat", "rb", SH_DENYWR);
		if (fpUserList) {
			for (;;) {
				if (EncryptRead(&User, sizeof(struct UserInfo), fpUserList, XOR_ULIST) == 0)
					break;

				printf("%2d|%2d %-20s  %-20s\n", User.ClanID[0], User.ClanID[1],
					   User.szMasterName, User.szName);
			}
			fclose(fpUserList);
		}
	}
}

// ------------------------------------------------------------------------- //

void User_Maint(void)
/*
 * Runs maintenance on all users.
 *
 *
 *
 */
{
	FILE *fpOldPC, *fpNewPC;
	struct clan *TmpClan;
	_INT16 iTemp, iTemp2, CurItem, Level;
	long XPRequired[MAX_LEVELS];

	DisplayStr("* User_Maint()\n");

	/* figure XP required per level */
	for (Level = 1; Level < MAX_LEVELS; Level++) {
		XPRequired[Level] = 50L;

		for (iTemp = 1; iTemp <= Level; iTemp++)
			XPRequired[Level] += ((long)(iTemp-1)*75L);
	}


	fpOldPC = _fsopen(ST_CLANSPCFILE, "rb", SH_DENYRW);
	if (fpOldPC) {
		fpNewPC = _fsopen(ST_NEWPCFILE, "w+b", SH_DENYRW);
		if (!fpNewPC) {
			System_Error("New.PC unopenable");
		}

		/* allocate memory */
		TmpClan = (struct clan *) malloc(sizeof(struct clan));
		CheckMem(TmpClan);

		for (;;) {
			/* go through each clan and write his updated info to new file */

			if (EncryptRead(TmpClan, sizeof(struct clan), fpOldPC, XOR_USER) == 0)
				break;

			/* skip if deleted */
			if (TmpClan->ClanID[0] == -1)
				continue;

			/* FIXME: if over 14 days old, delete */
			// add to "list" of users who are going to be deleted

			/* update stats needed */
			TmpClan->FirstDay = FALSE;

			// fix bugs
			if (TmpClan->Empire.VaultGold < 0)  TmpClan->Empire.VaultGold = 0;

			TmpClan->QuestToday = FALSE;
			TmpClan->VaultWithdrawals = 0;
			TmpClan->AttendedMass = FALSE;
			TmpClan->GotBlessing = FALSE;
			TmpClan->Prayed = FALSE;
			TmpClan->FightsLeft = Game.Data->MineFights;
			TmpClan->ClanFights = Game.Data->ClanFights;
			TmpClan->WasRulerToday = FALSE;
			TmpClan->ClanWars = 0;
			TmpClan->ChatsToday = 0;
			ClearFlags(TmpClan->DFlags);
			TmpClan->ResUncToday = 0;
			TmpClan->ResDeadToday = 0;

			// reduce days of protection
			if (TmpClan->Protection > 0)
				TmpClan->Protection--;
			else
				TmpClan->Protection = 0;
			TmpClan->TradesToday = 0;

			Empire_Maint(&TmpClan->Empire);

			for (iTemp = 0; iTemp < MAX_CLANCOMBAT; iTemp++) {
				TmpClan->ClanCombatToday[iTemp][0] = -1;
				TmpClan->ClanCombatToday[iTemp][1] = -1;
			}

			if (TmpClan->Empire.VaultGold < 0)
				TmpClan->Empire.VaultGold = 0;

			/* WAS this the ruler and WAS he ousted? */
			/* REP:
			if (OustedRuler && OldRulerId[0] == TmpClan->ClanID[0] &&
			  OldRulerId[1] == TmpClan->ClanID[1])
			{
			  TmpClan->WasRulerToday = TRUE;
			  TmpClan->Points -= 100;
			}
			*/

			/* is this the current ruler?  If so, give daily points */
			if (TmpClan->ClanID[0] == Village.Data->RulingClanId[0] &&
					TmpClan->ClanID[1] == Village.Data->RulingClanId[1]) {
				TmpClan->Points += 25;
			}

			// read in 6 members
			for (iTemp = 0; iTemp < 6; iTemp++) {
				TmpClan->Member[iTemp] = malloc(sizeof(struct pc));
				CheckMem(TmpClan->Member[iTemp]);
				EncryptRead(TmpClan->Member[iTemp], sizeof(struct pc), fpOldPC, XOR_PC);
			}

			/* set HP to max */
			for (iTemp = 0; iTemp < Game.Data->MaxPermanentMembers; iTemp++) {
				/* if player is unconscious, revive him */
				if (TmpClan->Member[iTemp]->Status == Unconscious ||
						TmpClan->Member[iTemp]->Status == Here) {
					TmpClan->Member[iTemp]->Status = Here;
					TmpClan->Member[iTemp]->HP = TmpClan->Member[iTemp]->MaxHP;
					TmpClan->Member[iTemp]->SP = TmpClan->Member[iTemp]->MaxSP;
				}
				// if XP < 0, set it to what it should be
				if (TmpClan->Member[iTemp]->Experience <= 0) {
					TmpClan->Member[iTemp]->Experience =
						XPRequired[TmpClan->Member[iTemp]->Level];
				}
				if (TmpClan->Member[iTemp]->Experience >
						XPRequired[TmpClan->Member[iTemp]->Level+1]) {
					TmpClan->Member[iTemp]->Experience =
						XPRequired[TmpClan->Member[iTemp]->Level+1];
				}
			}

			// "release" NPC members
			for (iTemp = Game.Data->MaxPermanentMembers; iTemp < 6; iTemp++)
				if (TmpClan->Member[iTemp]->szName[0]) {
					// is a npc-player
					//printf("releasing %s\n", TmpClan->Member[iTemp]->szName);

					for (iTemp2 = 0; iTemp2 < MAX_ITEMS_HELD; iTemp2++) {
						/* if held by deleted char, remove link */
						if (TmpClan->Items[iTemp2].Available &&
								TmpClan->Items[iTemp2].UsedBy == iTemp+1) {
							TmpClan->Items[iTemp2].UsedBy = 0;
						}
					}

					// delete him now
					TmpClan->Member[iTemp]->szName[0] = 0;
				}

			// fix up screwed up items, make them owned by no one
			for (CurItem = 0; CurItem < MAX_ITEMS_HELD; CurItem++) {
				/* if held by deleted char, remove link */
				if (TmpClan->Items[CurItem].UsedBy < 0 ||
						TmpClan->Items[CurItem].UsedBy > Game.Data->MaxPermanentMembers) {
					TmpClan->Items[CurItem].UsedBy = 0;
				}
			}

			/* make it so they stop using invalid items */
			for (iTemp = 0; iTemp < Game.Data->MaxPermanentMembers; iTemp++) {
				if (TmpClan->Member[iTemp]->szName[0]) {
					if (TmpClan->Member[iTemp]->Weapon &&
							TmpClan->Items[ TmpClan->Member[iTemp]->Weapon-1 ].Available == FALSE) {
						TmpClan->Member[iTemp]->Weapon = 0;
					}
					if (TmpClan->Member[iTemp]->Armor &&
							TmpClan->Items[ TmpClan->Member[iTemp]->Armor-1 ].Available == FALSE) {
						TmpClan->Member[iTemp]->Armor = 0;
					}
					if (TmpClan->Member[iTemp]->Shield &&
							TmpClan->Items[ TmpClan->Member[iTemp]->Shield-1 ].Available == FALSE) {
						TmpClan->Member[iTemp]->Shield = 0;
					}
				}
			}

			/* write new stuff to new file */
			EncryptWrite(TmpClan, sizeof(struct clan), fpNewPC, XOR_USER);

			for (iTemp = 0; iTemp < 6; iTemp++) {
				EncryptWrite(TmpClan->Member[iTemp], sizeof(struct pc), fpNewPC, XOR_USER);
				free(TmpClan->Member[iTemp]);
				TmpClan->Member[iTemp] = NULL;
			}
		}

		/* deallocate memory for PCs */
		free(TmpClan);

		fclose(fpOldPC);
		fclose(fpNewPC);

		/* delete old file, rename new one */
		unlink(ST_CLANSPCFILE);
		rename(ST_NEWPCFILE, ST_CLANSPCFILE);
	}
}


// ------------------------------------------------------------------------- //

BOOL User_Init(void)
/*
 * The current user online is initialized.  His stats are read in from
 * the .PC file.  If not found, a new clan is created.
 *
 * Returns FALSE if user is new and didn't want to join the game.
 */
{
	// Returns TRUE if user was created/loaded successfully, otherwise,
	// returns FALSE
	_INT16 iTemp;

	User.Initialized = TRUE;
	PClan = malloc(sizeof(struct clan));
	CheckMem(PClan);

	for (iTemp = 0; iTemp < MAX_MEMBERS; iTemp++)
		PClan->Member[iTemp] = NULL;

	if (!User_Read()) {
		// see if in league already but he's just not on this
		// BBS
		if (IBBS_InList(od_control.user_name, FALSE)) {
			// YES, he is IN the list already but he's not on
			// this BBS, so tell him that

			rputs("You're already logged as a user on another BBS.\n%P");
			User_Destroy();
			return FALSE;
		}

		if (!User_Create()) {
			User_Destroy();
			return FALSE;
		}
	}

	/*
	  // ensure CRC is correct
	  if (CheckCRC(PClan, sizeof(struct clan) - sizeof(long), PClan->CRC) == FALSE)
	  {
	    User_Destroy();
	    System_Error("CLANS.PC data corrupt! [u]\n");
	  }

	  // ensure CRC is correct
	  for (iTemp = 0; iTemp < MAX_MEMBERS; iTemp++)
	    if (PClan->Member[iTemp] && CheckCRC(PClan->Member[iTemp], sizeof(struct pc) - sizeof(long), PClan->Member[iTemp]->CRC) == FALSE)
	    {
	      User_Destroy();
	      System_Error("CLANS.PC data corrupt [m]!\n");
	    }
	*/


	User.UpdateUser = TRUE;
	return TRUE;
}

// ------------------------------------------------------------------------- //

void User_Close(void)
/*
 * Must be called to close down the user data.  User data is written to
 * file if UpdateUser set (so that deleted users are not written to file
 * if they chose to delete themselves).
 */
{
	// closes down current user logged in
	if (User.Initialized == FALSE) return;

	if (User.UpdateUser)
		User_Write();

	User_Destroy();
}

