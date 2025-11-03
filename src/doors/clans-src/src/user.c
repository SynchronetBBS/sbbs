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
# include <share.h>
#endif
#include "unix_wrappers.h"
#include "win_wrappers.h"

#include <OpenDoor.h>

#include "alliance.h"
#include "alliancem.h"
#include "class.h"
#include "door.h"
#include "empire.h"
#include "game.h"
#include "help.h"
#include "ibbs.h"
#include "items.h"
#include "input.h"
#include "language.h"
#include "mail.h"
#include "misc.h"
#include "mstrings.h"
#include "myopen.h"
#include "news.h"
#include "parsing.h"
#include "quests.h"
#include "readcfg.h"
#include "scores.h"
#include "spells.h"
#include "structs.h"
#include "system.h"
#include "trades.h"
#include "user.h"
#include "video.h"
#include "village.h"

static struct user {
	bool Initialized;     // true if user clan malloc'd
	bool UpdateUser;      // true if user data should be written to file
} User = { false, false };

struct clan *PClan=NULL;

// ------------------------------------------------------------------------- //
bool Disbanded(void)
{
	// returns true if the user is in DISBAND.DAT
	char szUserName[36];
	FILE *fp;
	bool Found = false;

	fp = _fsopen("disband.dat", "rb", _SH_DENYWR);

	if (!fp)  return false;

	for (;;) {
		if (!EncryptRead(szUserName, 36, fp, XOR_DISBAND)) break;
		if (memchr(szUserName, 0, 36) == NULL)
			System_Error("Unterminated username in disband.dat");

		if (strcasecmp(szUserName, od_control.user_name) == 0) {
			Found = true;
			break;
		}
	}

	fclose(fp);

	return Found;
}


// ------------------------------------------------------------------------- //
static void AddToDisband(void)
{
	// append user name to disband.dat
	FILE *fp;

	fp = _fsopen("disband.dat", "ab", _SH_DENYRW);
	if (fp) {
		CheckedEncryptWrite(od_control.user_name, 36, fp, XOR_DISBAND);
		fclose(fp);
	}
}


// ------------------------------------------------------------------------- //
void DeleteClan(int16_t ClanID[2], char *szClanName, bool Eliminate)
{
	FILE *fpOldPC, *fpNewPC, *OldMessage, *NewMessage;
	FILE *fpTradeFile;
	long OldOffset;
	char szString[128];
	int16_t CurTradeData, iTemp, CurAlliance, CurMember;
	struct TradeData TradeData;
	struct clan TmpClan = {0};
	struct Message Message;
	bool FoundInPCFile = false;   // set to true if he was ever on this board
	bool FoundNewCreator;
	struct Alliance *Alliances[MAX_ALLIANCES];


	AddToDisband();

	if (Game.Data.InterBBS) {
		// remove from list of clan names, remove from list of user names
		RemoveFromUList(ClanID);

		// remove from high scores list
		RemoveFromIPScores(ClanID);
	}

	// if this is the ruler of town, remove him
	if (ClanID[0] == Village.Data.RulingClanId[0] && ClanID[1] == Village.Data.RulingClanId[1]) {
		Village.Data.RulingClanId[0] = -1;
		Village.Data.RulingClanId[1] = -1;
		Village.Data.RulingDays = 0;
		Village.Data.GovtSystem = GS_DEMOCRACY;

		Village.Data.szRulingClan[0] = 0;
	}

	// go through PC file
	fpOldPC = _fsopen(ST_CLANSPCFILE, "rb", _SH_DENYWR);
	if (fpOldPC) {
		fpNewPC = _fsopen(ST_NEWPCFILE, "w+b", _SH_DENYWR);
		if (!fpNewPC) {
			System_Error("Can't write to new.pc\n");
			return;
		}

		for (;;) {
			/* go through each clan and write his info to new file and
			   skip the clan in question if he is found */

			notEncryptRead_s(clan, &TmpClan, fpOldPC, XOR_USER)
				break;

			/* if this is him */
			if (TmpClan.ClanID[0] == ClanID[0] && TmpClan.ClanID[1] == ClanID[1]) {
				FoundInPCFile = true;

				if (Eliminate == false) {
					// skip the guy since we're deleting him
					// skip 6 members
					fseek(fpOldPC, 6L * BUF_SIZE_pc, SEEK_CUR);
					continue;
				}
				else {
					//==== eliminated, might as well get rid of his allies
					// to clean things up
					// see if this clan is allied with him, if so, remove from allies list
					for (iTemp = 0; iTemp < MAX_ALLIES; iTemp++)
						TmpClan.Alliances[iTemp] = -1;
				}
			}

			// read in 6 members
			for (iTemp = 0; iTemp < 6; iTemp++) {
				TmpClan.Member[iTemp] = malloc(sizeof(struct pc));
				CheckMem(TmpClan.Member[iTemp]);
				EncryptRead_s(pc, TmpClan.Member[iTemp], fpOldPC, XOR_PC);
			}

			//=== modifications go here
			// make it so that he is NOT voted for
			if (TmpClan.ClanRulerVote[0] == ClanID[0] &&
					TmpClan.ClanRulerVote[1] == ClanID[1]) {
				TmpClan.ClanRulerVote[0] = TmpClan.ClanRulerVote[1] = -1;
			}


			// IN FUTURE: if ruler of an alliance, set new ruler if another
			// member was found.  if none, remove alliance from list

			//===

			/* write new stuff to new file */
			EncryptWrite_s(clan, &TmpClan, fpNewPC, XOR_USER);

			for (iTemp = 0; iTemp < 6; iTemp++) {
				EncryptWrite_s(pc, TmpClan.Member[iTemp], fpNewPC, XOR_PC);
				free(TmpClan.Member[iTemp]);
				TmpClan.Member[iTemp] = NULL;
			}
		}

		fclose(fpOldPC);
		fclose(fpNewPC);

		/* delete old file, rename new one */
		unlink(ST_CLANSPCFILE);
		rename(ST_NEWPCFILE, ST_CLANSPCFILE);
	}

	// go through msg file, set all his mail (to/from him) as deleted

	OldMessage = _fsopen("clans.msj", "rb", _SH_DENYRW);
	if (OldMessage) {   // MSJ file exists, so go on
		NewMessage = _fsopen("clansmsj.new", "wb", _SH_DENYRW);
		if (!NewMessage) {
			return;
		}

		for (;;) {
			notEncryptRead_s(Message, &Message, OldMessage, XOR_MSG)
				break;

			if (Message.Data.Length < 0) {
				fclose(NewMessage);
				unlink("clansmsj.new");
				System_Error("Message with negative length in DeleteClan()");
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
		unlink("clans.msj");
		rename("clansmsj.new", "clans.msj");
	}

	// go through trades.dat, for each trade struct, see if trade is headed
	// for him, if so, set trade as aborted, return goods to user sending

	fpTradeFile = _fsopen("trades.dat", "r+b", _SH_DENYRW);

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

			if (TradeData.ToClanID[0] == ClanID[0] && TradeData.ToClanID[1] == ClanID[1]) {
				rputs(">");
				// it's for this deleted player, so, rejecttrade
				RejectTrade(&TradeData);
				rputs("<");

				// write it to file
				fseek(fpTradeFile, OldOffset, SEEK_SET);
				EncryptWrite_s(TradeData, &TradeData, fpTradeFile, XOR_TRADE);
			}
			else if (TradeData.FromClanID[0] == ClanID[0] &&
					 TradeData.FromClanID[1] == ClanID[1]) {
				rputs(".");
				// trade is coming from this player, remove it
				TradeData.Active = false;
				fseek(fpTradeFile, OldOffset, SEEK_SET);
				EncryptWrite_s(TradeData, &TradeData, fpTradeFile, XOR_TRADE);
			}
		}

		fclose(fpTradeFile);
	}


	// add message to news
	if (*szClanName && FoundInPCFile && Eliminate == false &&
			PClan->Eliminated == false) {
		snprintf(szString, sizeof(szString), "|0A \xaf\xaf\xaf |0CThe clan of |0D%s |0Chas disbanded!\n\n",
				szClanName);
		News_AddNews(szString);
	}
	else if (*szClanName && FoundInPCFile && Eliminate) {
		snprintf(szString, sizeof(szString), "|0A \xaf\xaf\xaf |0CThe clan of |0D%s |0Chas been eliminated!\n\n",
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
	FreeAlliances(Alliances);
}


// ------------------------------------------------------------------------- //

bool ClanExists(int16_t ClanID[2])
{
	FILE *fpPlayerFile;
	int16_t CurClan/*, iTemp*/;
	long Offset;
	struct clan TmpClan = {0};
	bool FoundClan = false;

	fpPlayerFile = fopen(ST_CLANSPCFILE, "rb");
	if (!fpPlayerFile)
		return false;  /* failed to find clan */

	for (CurClan = 0;; CurClan++) {
		/* go through file till you find clan he wants */

		Offset = (long)CurClan * (BUF_SIZE_clan + 6L * BUF_SIZE_pc);
		if (fseek(fpPlayerFile, Offset, SEEK_SET))
			break;  /* couldn't fseek, so exit */

		notEncryptRead_s(clan, &TmpClan, fpPlayerFile, XOR_USER)
			break;  /* stop reading if no more players found */

		/* skip if deleted clan */
		if (TmpClan.ClanID[0] == -1)
			continue;

		/* if same Ids, found clan! */
		if (TmpClan.ClanID[0] == ClanID[0] && TmpClan.ClanID[1] == ClanID[1]) {
			FoundClan = true;
			break;
		}
	}
	fclose(fpPlayerFile);

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
	int16_t iTemp;

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

static void ShowBaseStats(struct PClass *PClass)
/*
 * Shows base stats for a given class.  Used for creating new PCs.
 */
{
	char szFullString[128], szString[128];
	char *szAttributeNames[NUM_ATTRIBUTES];
	size_t SpellStrLength = 0;
	/* = {
	  "Agility",
	  "Dexterity",
	  "Strength",
	  "Wisdom",
	  "Armor Strength",
	  "Charisma"};
	*/
	int16_t iTemp, NumSpellsKnown;
	bool AtLeastOneSpell = false;

	// Load attribute names
	LoadStrings(10, 6, szAttributeNames);

	rputs("|07Base Stats\n\n");

	for (iTemp = 0; iTemp < NUM_ATTRIBUTES; iTemp++) {
		/* go through each one, listing it */
		snprintf(szFullString, sizeof(szFullString), "|0C%-20s ", szAttributeNames[iTemp]);

		if (PClass->Attributes[iTemp] < 0)
			strlcat(szFullString, "|04", sizeof(szFullString));
		else if (PClass->Attributes[iTemp] == 0)
			strlcat(szFullString, "|0C", sizeof(szFullString));
		else
			strlcat(szFullString, "|15", sizeof(szFullString));

		snprintf(szString, sizeof(szString), "%-2d      ", PClass->Attributes[iTemp]);
		strlcat(szFullString, szString, sizeof(szFullString));
		rputs(szFullString);

		if ((iTemp+1)%2 == 0)
			rputs("\n");
	}

	strlcpy(szFullString, "|0CHitpoints            ", sizeof(szFullString));
	if (PClass->MaxHP < 0)
		strlcat(szFullString, "|04", sizeof(szFullString));
	else if (PClass->MaxHP == 0)
		strlcat(szFullString, "|0C", sizeof(szFullString));
	else
		strlcat(szFullString, "|15", sizeof(szFullString));
	snprintf(szString, sizeof(szString), "%-3d     ", PClass->MaxHP);
	strlcat(szFullString, szString, sizeof(szFullString));
	rputs(szFullString);

	strlcpy(szFullString, "|0CSkill Points         ", sizeof(szFullString));
	if (PClass->MaxSP < 0)
		strlcat(szFullString, "|04", sizeof(szFullString));
	else if (PClass->MaxSP == 0)
		strlcat(szFullString, "|0C", sizeof(szFullString));
	else
		strlcat(szFullString, "|15", sizeof(szFullString));
	snprintf(szString, sizeof(szString), "%-3d\n", PClass->MaxSP);
	strlcat(szFullString, szString, sizeof(szFullString));
	rputs(szFullString);

	strlcpy(szFullString, "|0CGold                 ", sizeof(szFullString));
	if (PClass->Gold < 0)
		strlcat(szFullString, "|04", sizeof(szFullString));
	else if (PClass->Gold == 0)
		strlcat(szFullString, "|0C", sizeof(szFullString));
	else
		strlcat(szFullString, "|15", sizeof(szFullString));
	snprintf(szString, sizeof(szString), "%-3d\n", PClass->Gold);
	strlcat(szFullString, szString, sizeof(szFullString));
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

			snprintf(szString, sizeof(szString), "%s", Spells[ PClass->SpellsKnown[iTemp] - 1]->szName);
			rputs(szString);
			SpellStrLength += strlen(szString);
			NumSpellsKnown++;

			AtLeastOneSpell = true;

			if (SpellStrLength > 60) {
				rputs("\n");
				SpellStrLength = 0;
				AtLeastOneSpell = false;
			}
		}
	}

	if (NumSpellsKnown == 0)
		rputs("|07None.");

	rputs("\n\n\n");
}

static int16_t GetClass(struct PClass *PClass[MAX_PCLASSES], char *szHelp)
/*
 * Returns a class as chosen by the user.  Used for creating new PCs.
 */
{
	int16_t ClassChosen, iTemp;
	signed char cTemp;
	char szKeys[MAX_PCLASSES + 2], szString[128], Choice;

	/* so we have szKeys[] = "?ABCDEFGHI..." */
	szKeys[0] = '?';
	for (cTemp = 0; cTemp < MAX_PCLASSES; cTemp++)
		szKeys[cTemp + 1] = 'A' + cTemp;
	szKeys[cTemp + 1] = 0;

	rputs("\n");

	for (;;) {
		/* show options */
		for (iTemp = 0; iTemp < MAX_PCLASSES; iTemp++) {
			if (PClass[iTemp] == NULL) {
				szKeys[iTemp + 1] = 0;
				break;
			}

			snprintf(szString, sizeof(szString), ST_GETCLASS0, szKeys[iTemp + 1], PClass[iTemp]->szName);
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

static void ChooseDefaultAction(struct pc *PC)
/*
 * This allows the user to choose a default action for the PC specified.
 */
{
	const char *pszOptions[MAX_SPELLS + 1];       // add 1 for "Attack" command
	int16_t iTemp, NumOptions, WhichOption;

	if (!PClan->DefActionHelp) {
		PClan->DefActionHelp = true;
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
					true, DT_LONG, false);

	if (WhichOption == 0)
		PC->DefaultAction = 0;
	else
		PC->DefaultAction = (unsigned)((WhichOption + 9) & 0x7F);
}


void ShowPlayerStats(struct pc *PC, bool AllowModify)
/*
 * Shows statistics for the given PC.
 */
{
	int16_t iTemp, NumSpellsKnown, SpellNum;
	bool AtLeastOneSpell, Done;
	size_t SpellStrLength = 0;
	int16_t CurAttr, CurSpell;
	char szString[255], cInput;
	int32_t XPRequired[MAX_LEVELS];
	int16_t Level;
	char *szAttrNames[NUM_ATTRIBUTES];

	// load attribute names
	LoadStrings(10, 6, szAttrNames);

	/* figure XP required per level */
	for (Level = 1; Level < MAX_LEVELS; Level++) {
		XPRequired[Level] = 50;

		for (iTemp = 1; iTemp <= Level; iTemp++)
			XPRequired[Level] += ((iTemp-1)*75);
	}

	if (AllowModify == false)
		Done = true;
	else
		Done = false;

	do {
		od_clr_scr();

		snprintf(szString, sizeof(szString), ST_P2STATS0, PC->szName);

		switch (PC->Status) {
			case Dead :
				strlcat(szString, "|04(Dead)", sizeof(szString));
				break;
			case Unconscious :
				strlcat(szString, "|04(Unconscious)", sizeof(szString));
				break;
			case Here :
				strlcat(szString, "|0M(Alive)", sizeof(szString));
				break;
		}

		PadString(szString, 43);
		strlcat(szString, ST_P2STATS1, sizeof(szString));
		rputs(szString);

		snprintf(szString, sizeof(szString), ST_P2STATS2, (int)PC->Level);
		PadString(szString, 43);
		rputs(szString);

		if (PC->Weapon)
			snprintf(szString, sizeof(szString), ST_P2STATS3, PC->MyClan->Items[ PC->Weapon-1 ].szName);
		else
			snprintf(szString, sizeof(szString), ST_P2STATS3, "Nothing");
		rputs(szString);


		snprintf(szString, sizeof(szString), ST_P2STATS4, (long)PC->Experience, (long)(XPRequired[ PC->Level + 1 ] - PC->Experience),
				PC->TrainingPoints);
		PadString(szString, 43);
		rputs(szString);

		if (PC->Armor)
			snprintf(szString, sizeof(szString), ST_P2STATS5, PC->MyClan->Items[ PC->Armor-1 ].szName);
		else
			snprintf(szString, sizeof(szString), ST_P2STATS5, "Nothing");
		rputs(szString);


		if (PC->WhichRace != -1) {
			snprintf(szString, sizeof(szString), ST_P2STATS6, Races[ PC->WhichRace ]->szName,
					PClasses[ PC->WhichClass ]->szName);
		}
		else {
			snprintf(szString, sizeof(szString), ST_P2STATS6, "Unknown", "Unknown");
		}
		PadString(szString, 43);
		rputs(szString);

		// shield
		if (PC->Shield)
			snprintf(szString, sizeof(szString), ST_P2STATS7, PC->MyClan->Items[ PC->Shield-1 ].szName);
		else
			snprintf(szString, sizeof(szString), ST_P2STATS7, "Nothing");
		rputs(szString);

		snprintf(szString, sizeof(szString), ST_P2STATS8, (int)PC->HP, (int)PC->MaxHP);
		rputs(szString);
		snprintf(szString, sizeof(szString), ST_P2STATS9, (int)PC->SP, (int)PC->MaxSP);
		rputs(szString);
		snprintf(szString, sizeof(szString), ST_P2STATS14, (int)PC->TrainingPoints);
		rputs(szString);

		rputs(ST_P2STATS10);

		NumSpellsKnown = 0;
		for (iTemp = 0; iTemp < MAX_SPELLS; iTemp++) {
			if (PC->SpellsKnown[iTemp])
				NumSpellsKnown++;
		}

		for (CurAttr = 0, CurSpell = 0; CurAttr < NUM_ATTRIBUTES; CurAttr++) {
			snprintf(szString, sizeof(szString), "|0L %-19s |0M(%-2d) |0N%-2d",
					szAttrNames[CurAttr], PC->Attributes[CurAttr], GetStat(PC, (char)CurAttr));

			PadString(szString, 43);
			rputs(szString);

			// update spells shown
			if (CurAttr == 0 && NumSpellsKnown == 0)
				rputs("None\n");

			else {
				AtLeastOneSpell = false;
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
						AtLeastOneSpell = true;


						snprintf(szString, sizeof(szString), "%s", Spells[ PC->SpellsKnown[CurSpell] - 1]->szName);
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

			snprintf(szString, sizeof(szString), "%s\n", Spells[SpellNum]->szName);
			rputs(szString);
		}

		rputs(ST_P2STATS12);

		if (AllowModify) {
			rputs(ST_P2STATS13);

			cInput = toupper(od_get_key(true) & 0x7f) & 0x7f;

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
	size_t iTemp2, Length;
	int16_t iTemp, LastItem = 0, FoundItem = false;
	int16_t CurItem;
	char szString[100], szOwner[30];

	rputs("|0B # Name|07                 |0CUsed By|07      ");
	rputs("|0B # Name|07                 |0CUsed By|07\n");
	rputs("|0A\xcd\xcd\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xcd\xcd|07   ");
	rputs("|0A\xcd\xcd\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xcd\xcd|07\n");

	/* show all the items here */
	for (iTemp = 0, CurItem = 0; iTemp < MAX_ITEMS_HELD; iTemp++) {
		if (Clan->Items[iTemp].Available) {
			FoundItem = true;
			/* see if owned */
			if (Clan->Items[iTemp].UsedBy) {
				strlcpy(szOwner, Clan->Member[ Clan->Items[iTemp].UsedBy - 1]->szName, sizeof(szOwner));
				szOwner[10] = 0;

				snprintf(szString, sizeof(szString), "|0L%2d |0M%-20s |0N%s", iTemp+1, Clan->Items[iTemp].szName,
						szOwner);
			}
			else
				snprintf(szString, sizeof(szString), "|0L%2d |0M%-20s |0N", iTemp+1, Clan->Items[iTemp].szName);

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
	if (FoundItem == false)
		rputs(" |04No items in inventory.");

	if (LastItem%2 == 0)
		rputs("\n");

	rputs("\n");
}


static void ItemEquipResults(struct pc *PC, struct item_data *Item, bool Equipping)
/*
 * This functions outputs results of equipping an item.
 */
{
	char *szVerb;
	char szString[128], Gain,
	*szAttrNames[NUM_ATTRIBUTES];
	int16_t CurStat;

	LoadStrings(10, 6, szAttrNames);

	// go through each stat, say if gained or lost
	for (CurStat = 0; CurStat < NUM_ATTRIBUTES; CurStat++) {
		Gain = Item->Attributes[CurStat];

		if (Equipping == false)
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

		snprintf(szString, sizeof(szString), "|03%s %s by %d\n", szAttrNames[ CurStat], szVerb, Gain);
		rputs(szString);
	}
}

void ItemStats(void)
/*
 * Shows item statistics for PClan.
 */
{
	/* modify item stats, assume it's the player */
	int16_t ItemIndex, OneItemFound;
	int16_t DefaultItemIndex, iTemp, WhoEquip;
	char szKeys[11], szString[100], /*szTemp[60],*/ szItemName[25];
	bool DoneEquipping;
	signed char cTemp;

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

				ItemIndex = (int16_t) GetLong(ST_ISTATS3, DefaultItemIndex, MAX_ITEMS_HELD);
				if (ItemIndex == 0)
					break;
				ItemIndex--;

				/* if that item is non-existant, tell him */
				if (PClan->Items[ItemIndex].Available == false) {
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

				ItemIndex = (int16_t) GetLong(ST_ISTATS7, DefaultItemIndex, MAX_ITEMS_HELD);
				if (ItemIndex == 0)
					break;
				ItemIndex--;

				/* if that item is non-existant, tell him */
				if (PClan->Items[ItemIndex].Available == false) {
					rputs(ST_ISTATS4);
					break;
				}
				/* if that item is in use, tell him */
				if (PClan->Items[ItemIndex].UsedBy != 0) {
					rputs(ST_ISTATS8);
					break;
				}

				/* still wanna drop it? */
				snprintf(szString, sizeof(szString), ST_ISTATS9, PClan->Items[ItemIndex].szName);

				if (NoYes(szString) == YES) {
					/* drop it */
					snprintf(szString, sizeof(szString), ST_ISTATS10, PClan->Items[ItemIndex].szName);
					rputs(szString);

					PClan->Items[ItemIndex].szName[0] = 0;
					PClan->Items[ItemIndex].Available = false;
				}
				break;
			case 'S' :
				rputs(ST_ISTATS11);

				/* find first item which is equipped */
				OneItemFound = false;
				for (iTemp = 0; iTemp < MAX_ITEMS_HELD; iTemp++) {
					if (PClan->Items[iTemp].Available &&
							PClan->Items[iTemp].cType != I_SCROLL &&
							PClan->Items[iTemp].cType != I_BOOK)
						OneItemFound = true;

					if (PClan->Items[iTemp].Available &&
							PClan->Items[iTemp].UsedBy != 0 &&
							PClan->Items[iTemp].cType != I_SCROLL &&
							PClan->Items[iTemp].cType != I_BOOK)
						break;
				}
				if (iTemp == MAX_ITEMS_HELD && OneItemFound == true) {
					rputs(ST_ISTATS12);
					break;
				}
				else if (OneItemFound == false) {
					rputs(ST_ISTATS2);
					break;
				}

				DefaultItemIndex = iTemp+1;

				ItemIndex = (int16_t) GetLong(ST_ISTATS13, DefaultItemIndex, MAX_ITEMS_HELD);
				if (ItemIndex == 0)
					break;
				ItemIndex--;

				/* if that item is non-existant, tell him */
				if (PClan->Items[ItemIndex].Available == false) {
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
						snprintf(szString, sizeof(szString), ST_ISTATS15,
								PClan->Member[PClan->Items[ItemIndex].UsedBy-1]->szName, PClan->Items[ItemIndex].szName);
						rputs(szString);

						ItemEquipResults(PClan->Member[PClan->Items[ItemIndex].UsedBy-1], &PClan->Items[ItemIndex], false);

						PClan->Member[PClan->Items[ItemIndex].UsedBy-1]->Weapon = 0;
						PClan->Items[ItemIndex].UsedBy = 0;
						break;
					case I_ARMOR :
						snprintf(szString, sizeof(szString), ST_ISTATS16,
								PClan->Member[PClan->Items[ItemIndex].UsedBy-1]->szName, PClan->Items[ItemIndex].szName);
						rputs(szString);

						ItemEquipResults(PClan->Member[PClan->Items[ItemIndex].UsedBy-1], &PClan->Items[ItemIndex], false);

						PClan->Member[PClan->Items[ItemIndex].UsedBy-1]->Armor = 0;
						PClan->Items[ItemIndex].UsedBy = 0;
						break;
					case I_SHIELD :
						snprintf(szString, sizeof(szString), ST_ISTATS17,
								PClan->Member[PClan->Items[ItemIndex].UsedBy-1]->szName, PClan->Items[ItemIndex].szName);
						rputs(szString);

						ItemEquipResults(PClan->Member[PClan->Items[ItemIndex].UsedBy-1], &PClan->Items[ItemIndex], false);

						PClan->Member[PClan->Items[ItemIndex].UsedBy-1]->Shield = 0;
						PClan->Items[ItemIndex].UsedBy = 0;
						break;
				}
				break;
			case 'E' :
				rputs(ST_ISTATS18);
				/* find first item which is not equipped yet */
				OneItemFound = false;
				for (iTemp = 0; iTemp < MAX_ITEMS_HELD; iTemp++) {
					if (PClan->Items[iTemp].Available &&
							PClan->Items[iTemp].cType != I_SCROLL &&
							PClan->Items[iTemp].cType != I_BOOK)
						OneItemFound = true;

					if (PClan->Items[iTemp].Available &&
							PClan->Items[iTemp].UsedBy == 0 &&
							PClan->Items[iTemp].cType != I_SCROLL &&
							PClan->Items[iTemp].cType != I_BOOK)
						break;
				}
				if (iTemp == MAX_ITEMS_HELD && OneItemFound == true) {
					rputs(ST_ISTATS19);
					break;
				}
				else if (OneItemFound == false) {
					rputs(ST_ISTATS2);
					break;
				}
				DefaultItemIndex = iTemp+1;

				ItemIndex = (int16_t) GetLong(ST_ISTATS20, DefaultItemIndex, MAX_ITEMS_HELD);
				if (ItemIndex == 0)
					break;
				ItemIndex--;

				/* if that item is non-existant, tell him */
				if (PClan->Items[ItemIndex].Available == false) {
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

				DoneEquipping = false;
				while (!DoneEquipping) {
					/* if not, ask who to equip it on */
					szKeys[0] = '\r';
					szKeys[1] = '\n';
					szKeys[2] = 0;
					rputs("\n");
					for (cTemp = 0; cTemp < 6; cTemp++) {
						if (PClan->Member[cTemp]) {
							strlcpy(szItemName, "Nothing", sizeof(szItemName));

							// see if equipped already
							switch (PClan->Items[ ItemIndex ].cType) {
								case I_WEAPON :
									if (PClan->Member[cTemp]->Weapon) {
										strlcpy(szItemName, PClan->Items[PClan->Member[cTemp]->Weapon-1].szName, sizeof(szItemName));
									}
									break;
								case I_ARMOR :
									if (PClan->Member[cTemp]->Armor) {
										strlcpy(szItemName, PClan->Items[PClan->Member[cTemp]->Armor-1].szName, sizeof(szItemName));
									}
									break;
								case I_SHIELD :
									if (PClan->Member[cTemp]->Shield) {
										strlcpy(szItemName, PClan->Items[PClan->Member[cTemp]->Shield-1].szName, sizeof(szItemName));
									}
									break;
							}

							snprintf(szString, sizeof(szString), ST_ISTATS22,
									(char)(cTemp + 'A'),
									ItemPenalty(PClan->Member[cTemp], &PClan->Items[ItemIndex]) ? "|08" : "|0C",
									PClan->Member[cTemp]->szName,
									szItemName);

							// append race/class
							/* show race/class */
							/*
							              if (PClan->Member[cTemp]->WhichRace != -1)
							                snprintf(szTemp, sizeof(szTemp), "%s/%s\n",
							                  Races[PClan->Member[cTemp]->WhichRace]->szName,
							                  PClasses[PClan->Member[cTemp]->WhichClass]->szName);
							              else
							                strlcpy(szTemp, "[unknown]\n", sizeof(szTemp));
							              strlcat(szString, szTemp, sizeof(szString));
							*/

							rputs(szString);
							szKeys[ strlen(szKeys) +1 ] = 0;
							szKeys[ strlen(szKeys)] = cTemp + 'A';
						}
					}
					strlcat(szKeys, "Q", sizeof(szKeys));
					rputs(ST_ISTATS23);


					snprintf(szString, sizeof(szString), ST_ISTATS24, PClan->Items[ItemIndex].szName);
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
					snprintf(szString, sizeof(szString), "|15%s\n\n", PClan->Member[WhoEquip]->szName);
					rputs(szString);

					if (ItemPenalty(PClan->Member[WhoEquip], &PClan->Items[ ItemIndex ])) {
						rputs("\n|04That player cannot use that item\n");
						continue;
					}


					/* see if that guy has something equipped, if so, tell user */
					switch (PClan->Items[ ItemIndex ].cType) {
						case I_WEAPON :
							if (PClan->Member[WhoEquip]->Weapon) {
								snprintf(szString, sizeof(szString), ST_ISTATS25,
										PClan->Items[ PClan->Member[WhoEquip]->Weapon-1].szName);
								if (YesNo(szString) == YES) {
									iTemp = PClan->Member[WhoEquip]->Weapon-1;
									snprintf(szString, sizeof(szString), ST_ISTATS15,
											PClan->Member[WhoEquip]->szName, PClan->Items[iTemp].szName);
									rputs(szString);

									ItemEquipResults(PClan->Member[WhoEquip], &PClan->Items[iTemp], false);

									PClan->Member[WhoEquip]->Weapon = 0;
									PClan->Items[iTemp].UsedBy = 0;
								}
								else {
									rputs(ST_ABORTED);
									break;
								}
							}

							snprintf(szString, sizeof(szString), ST_ISTATS26, PClan->Member[WhoEquip]->szName, PClan->Items[ItemIndex].szName);
							rputs(szString);

							PClan->Member[WhoEquip]->Weapon = ItemIndex + 1;
							PClan->Items[ItemIndex].UsedBy = WhoEquip + 1;
							// tell them what happened to his stats
							ItemEquipResults(PClan->Member[WhoEquip], &PClan->Items[ItemIndex], true);
							DoneEquipping = true;
							break;
						case I_ARMOR :
							if (PClan->Member[WhoEquip]->Armor) {
								snprintf(szString, sizeof(szString), ST_ISTATS27,
										PClan->Items[ PClan->Member[WhoEquip]->Armor-1].szName);
								if (YesNo(szString) == YES) {
									iTemp = PClan->Member[WhoEquip]->Armor-1;
									snprintf(szString, sizeof(szString), ST_ISTATS15,
											PClan->Member[WhoEquip]->szName, PClan->Items[iTemp].szName);
									rputs(szString);

									ItemEquipResults(PClan->Member[WhoEquip], &PClan->Items[iTemp], false);

									PClan->Member[WhoEquip]->Armor = 0;
									PClan->Items[iTemp].UsedBy = 0;
								}
								else {
									rputs(ST_ABORTED);
									break;
								}
							}
							snprintf(szString, sizeof(szString), ST_ISTATS28, PClan->Member[WhoEquip]->szName, PClan->Items[ItemIndex].szName);
							rputs(szString);

							PClan->Member[WhoEquip]->Armor = ItemIndex + 1;
							PClan->Items[ItemIndex].UsedBy = WhoEquip + 1;
							// tell them what happened to his stats
							ItemEquipResults(PClan->Member[WhoEquip], &PClan->Items[ItemIndex], true);
							DoneEquipping = true;
							break;
						case I_SHIELD :
							if (PClan->Member[WhoEquip]->Shield) {
								snprintf(szString, sizeof(szString), ST_ISTATS29,
										PClan->Items[ PClan->Member[WhoEquip]->Shield-1].szName);
								if (YesNo(szString) == YES) {
									iTemp = PClan->Member[WhoEquip]->Shield-1;
									snprintf(szString, sizeof(szString), ST_ISTATS15,
											PClan->Member[WhoEquip]->szName, PClan->Items[iTemp].szName);
									rputs(szString);

									ItemEquipResults(PClan->Member[WhoEquip], &PClan->Items[iTemp], false);

									PClan->Member[WhoEquip]->Shield = 0;
									PClan->Items[iTemp].UsedBy = 0;
								}
								else {
									rputs(ST_ABORTED);
									break;
								}
							}
							snprintf(szString, sizeof(szString), ST_ISTATS30, PClan->Member[WhoEquip]->szName, PClan->Items[ItemIndex].szName);
							rputs(szString);

							PClan->Member[WhoEquip]->Shield = ItemIndex + 1;
							PClan->Items[ItemIndex].UsedBy = WhoEquip + 1;
							// tell them what happened to his stats
							ItemEquipResults(PClan->Member[WhoEquip], &PClan->Items[ItemIndex], true);
							DoneEquipping = true;
							break;
					}
				}
				break;
		}
	}
}

static int16_t NumClansInVillage(void)
{
	FILE *fp;
	struct clan TmpClan = {0};
	int16_t NumClans;

	fp = fopen(ST_CLANSPCFILE, "rb");
	if (!fp)
		return 0;

	for (NumClans = 0;;) {
		notEncryptRead_s(clan, &TmpClan, fp, XOR_USER)
			break;

		if (TmpClan.ClanID[0] != -1)  /* means is active */
			NumClans++;

		/* skip his 6 PCs */
		fseek(fp, 6 * BUF_SIZE_pc, SEEK_CUR);
	}


	fclose(fp);

	return (NumClans);
}

void ShowVillageStats(void)
{
	char szString[255], szRuler[25];

	od_clr_scr();
	if (Game.Data.InterBBS)
		snprintf(szString, sizeof(szString), ST_VSTATHEADER, VillageName(IBBS.Data.BBSID));
	else
		snprintf(szString, sizeof(szString), ST_VSTATHEADER, Village.Data.szName);
	rputs(szString);

	rputs(ST_LONGDIVIDER);

	if (Village.Data.szRulingClan[0] == 0)
		strlcpy(szRuler, "None", sizeof(szRuler));
	else
		strlcpy(szRuler, Village.Data.szRulingClan, sizeof(szRuler));

	snprintf(szString, sizeof(szString), ST_VSTATS1, (int)NumClansInVillage(), szRuler);
	rputs(szString);

	snprintf(szString, sizeof(szString), ST_VSTATS2, (int)Village.Data.TaxRate);
	rputs(szString);

	snprintf(szString, sizeof(szString), ST_VSTATS4, (int)Village.Data.GST);
	rputs(szString);

	snprintf(szString, sizeof(szString), ST_VSTATS7, (long)Village.Data.Empire.VaultGold);
	rputs(szString);

	snprintf(szString, sizeof(szString), ST_TMENUSTAT8, (int)Village.Data.ConscriptionRate);
	rputs(szString);

	// == 0 means game in progress
	if (Game.Data.GameState == 0) {
		snprintf(szString, sizeof(szString), ST_VSTATS8, (long)DaysBetween(Game.Data.szDateGameStart, System.szTodaysDate),
				Game.Data.szDateGameStart, "Disabled");
		rputs(szString);
	}
	else if (Game.Data.GameState == 1) {
		if (Game.Data.szDateGameStart[0] != 0) {
			snprintf(szString, sizeof(szString), ST_VSTATS9, Game.Data.szDateGameStart);
			rputs(szString);
		}
		else
			rputs(ST_VSTATS10);
	}

	rputs(ST_LONGDIVIDER);
	door_pause();
}


void ClanStats(struct clan *Clan, bool AllowModify)
/*
 * Shows stats for given Clan with option to modify values.
 */
{
	size_t Length;
	int16_t iTemp, TotalItems, ItemsShown;
	char szString[160], szStats[160];
	bool DoneLooking = false;
	char szShortName[25], cKey;

	/* show stats for clan */

	while (DoneLooking == false) {
		od_clr_scr();

		snprintf(szString, sizeof(szString), ST_CSTATS0, Clan->szName, Clan->Symbol);
		rputs(szString);

		rputs(ST_LONGDIVIDER);
		snprintf(szString, sizeof(szString), ST_CSTATS1, Clan->szDateOfLastGame);
		rputs(szString);

		snprintf(szString, sizeof(szString), ST_CSTATS2, (long)Clan->Empire.VaultGold);
		rputs(szString);

		snprintf(szString, sizeof(szString), ST_CSTATS3, (int)Clan->MineLevel);
		rputs(szString);

		/* members */
		for (iTemp = 0; iTemp < MAX_MEMBERS; iTemp++) {
			if (Clan->Member[iTemp]) {
				strlcpy(szShortName, Clan->Member[iTemp]->szName, sizeof(szShortName));
				szShortName[8] = 0;
				snprintf(szString, sizeof(szString), " |0P(|0Q%d|0P) |0L%-8s |0M%3d /%3d HP   ", iTemp+1, szShortName, Clan->Member[iTemp]->HP, Clan->Member[iTemp]->MaxHP);
				rputs(szString);

				/* show race/class */
				if (Clan->Member[iTemp]->WhichRace != -1)
					snprintf(szString, sizeof(szString), "|0N%s/%s  ", Races[Clan->Member[iTemp]->WhichRace]->szName, PClasses[Clan->Member[iTemp]->WhichClass]->szName);
				else
					strlcpy(szString, "|0N[unknown]  ", sizeof(szString));

				/* add or remove spaces to filler it */
				if (strlen(szString) > 20) {
					szString[20] = 0;
				}
				else {
					/* add spaces */
					szString[20] = 0;
					Length = strlen(szString);
					for (size_t stTemp = 19; stTemp >= Length; stTemp--)
						szString[stTemp] = ' ';
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
					strlcpy(szStats, "|0O(", sizeof(szStats));  /* begin stats */

					ItemsShown = 0;

					/* show first item */
					if (Clan->Member[iTemp]->Weapon) {
						snprintf(szString, sizeof(szString), "%s", Clan->Items[ Clan->Member[iTemp]->Weapon - 1].szName);
						strlcat(szStats, szString, sizeof(szStats));

						ItemsShown++;

						if (TotalItems > ItemsShown)
							strlcat(szStats, ", ", sizeof(szStats));
					}

					if (Clan->Member[iTemp]->Armor) {
						snprintf(szString, sizeof(szString), "%s", Clan->Items[ Clan->Member[iTemp]->Armor - 1].szName);
						strlcat(szStats, szString, sizeof(szStats));

						ItemsShown++;

						if (TotalItems > ItemsShown)
							strlcat(szStats, ", ", sizeof(szStats));
					}

					if (Clan->Member[iTemp]->Shield) {
						snprintf(szString, sizeof(szString), "%s", Clan->Items[ Clan->Member[iTemp]->Shield - 1].szName);
						strlcat(szStats, szString, sizeof(szStats));

						ItemsShown++;
					}
					/* truncate it */
					szStats[36] = 0;
					strlcat(szStats, ")\n", sizeof(szStats));

					rputs(szStats);
				}
				else
					rputs("|0O(Nothing equipped)\n");
			}
		}

		rputs("\n");

		/* show army stats */
		snprintf(szString, sizeof(szString), ST_CSTATS5, (long)Clan->Empire.Army.Followers);
		rputs(szString);

		if (Clan->Protection) {
			snprintf(szString, sizeof(szString), "|0L This clan will remain in protection for %d more day(s)\n", Clan->Protection);
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
				if (AllowModify == false)
					door_pause();
				break;
			case 'E' :  // empire stats
				rputs("Empire\n\n");
				if (Game.Data.ClanEmpires == true) {
					if (AllowModify == false)
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
				DoneLooking = true;
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

				if (AllowModify == false) {
					snprintf(szString, sizeof(szString), "|0GClan's current symbol is %s\n\n", Clan->Symbol);
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
					// snprintf(szString, sizeof(szString), "|0SYour current symbol is |%02d%s\n\n", Clan->Color, Clan->Symbol);
					snprintf(szString, sizeof(szString), "|0SYour current symbol is %s\n\n", Clan->Symbol);
					rputs(szString);
				}

				rputs("|0SPlease type your new symbol or press enter to keep it the same\n|0E> |0F");
				GetStr(Clan->Symbol, 20, true);

				snprintf(szString, sizeof(szString), "|0SYour clan symbol is now %s\n\n", Clan->Symbol);
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


void PC_Create(struct pc *PC, bool ClanLeader)
/*
 * Used to create a new PC.
 */
{
	char szString[128];
	int16_t iTemp, LastSpellSlot;

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
			PC->MaxHP += (int16_t)my_random(10);

		PC->MaxSP = Races[ PC->WhichRace ]->MaxSP;

		for (iTemp = 0; iTemp < NUM_ATTRIBUTES; iTemp++)
			PC->Attributes[iTemp] += PClasses[ PC->WhichClass ]->Attributes[iTemp];

		PC->MaxHP += PClasses[ PC->WhichClass ]->MaxHP;
		PC->MaxSP += PClasses[ PC->WhichClass ]->MaxSP;
		PC->Difficulty = -1;  // no difficulty

		/* randomize stats a bit */
		// my_random(1) always returns 0.
		//for (iTemp = 0; iTemp < NUM_ATTRIBUTES; iTemp++)
		//	PC->Attributes[iTemp] += my_random(1);

		// if clan leader, add on some random stats
		if (ClanLeader)
			for (iTemp = 0; iTemp < NUM_ATTRIBUTES; iTemp++)
				PC->Attributes[iTemp] += (char)my_random(3);

		PC->MaxHP += (int16_t)my_random(5);
		PC->MaxSP += (int16_t)my_random(5);
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

		strlcpy(PC->szName, "New Player", sizeof(PC->szName));

		/* set all spells to 0 */
		for (iTemp = 0; iTemp < 10; iTemp++)
			PC->SpellsInEffect[iTemp].SpellNum = -1;

		ShowPlayerStats(PC, false);

		/* are these ok? */
		if (YesNo("|03Use these stats?") == YES)
			break;
	}

	/* ask for name */
	for (;;) {
		// rputs("\n|02Enter the name of this member\n|10> |07");
		rputs(ST_STUFF9);

		szString[0] = 0;
		GetStr(szString, 12, false);

		RemovePipes(szString, PC->szName);
		Strip(PC->szName);

		if (PC->szName[0] == 0)
			continue;
		else
			break;
	}

	PC->MyClan = PClan;
	PC->Undead = false;
	PC->DefaultAction = 0;

	// snprintf(szString, sizeof(szString), |10%s joins the clan.\n%%P, PC->szName);
	snprintf(szString, sizeof(szString), ST_STUFF10, PC->szName);
	rputs(szString);
}


bool NameInUse(char *szName)
/*
 * Returns true if the given Clan name is already in use.
 */
{
	FILE *fpPCFile;
	struct clan TmpClan = {0};
	int16_t CurClan = 0;
	long Offset;

	fpPCFile = fopen(ST_CLANSPCFILE, "rb");
	if (fpPCFile) {
		/* go through list */
		for (CurClan = 0;; CurClan++) {
			Offset = (long)CurClan * (BUF_SIZE_clan + 6L * BUF_SIZE_pc);
			if (fseek(fpPCFile, Offset, SEEK_SET))
				break;  /* couldn't fseek, so exit */

			notEncryptRead_s(clan, &TmpClan, fpPCFile, XOR_USER)
				break;  /* stop reading if no more players found */

			/* see if this player has same name as user online */
			/* strip names */
			if (strcasecmp(szName, TmpClan.szName) == 0) {
				/* are the same, found another player with name, close file */
				fclose(fpPCFile);
				return true;
			}
		}

		/* no players found with same name */
		fclose(fpPCFile);
	}

	// look through names list to see if that name is in use
	if (Game.Data.InterBBS && IBBS_InList(szName, true)) {
		// found this clanname in the list of clannames of the league
		// don't allow him to use it
		return true;
	}

	return false;
}

static bool ChooseClanName(char *szName)
/*
 * Allows user to choose a clan name.
 */
{
	char szString[30];

	for (;;) {
		// rputs("\n|02What will you call your clan?\n|10> |07");
		rputs(ST_STUFF3);
		szString[0] = 0;
		GetStr(szString, 24, false);

		RemovePipes(szString, szName);
		Strip(szName);

		if (szName[0] == 0) {
			rputs(ST_ABORTED);
			return false;
		}

		if (szName[0] == '?') {
			rputs("|12Invalid name!\n");
		}

		/* see if that name is in use */
		else if (NameInUse(szName) == false) {
			rputs("|0F");
			rputs(szName);
			if (YesNo("|16|0E : |0SUse this name?") == YES) {
				return true;
			}
			rputs("\n");
		}
		else {
			rputs("|12That name is in use, please choose again.\n\n");
		}
	}
}


static void User_ResetHelp(void)
/*
 * Resets help database for clan
 */
{
	PClan->DefActionHelp = false;
	PClan->CommHelp      = false;
	PClan->MineHelp      = false;
	PClan->MineLevelHelp = false;
	PClan->CombatHelp    = false;
	PClan->TrainHelp     = false;
	PClan->MarketHelp    = false;
	PClan->PawnHelp      = false;
	PClan->WizardHelp    = false;
	PClan->EmpireHelp    = false;
	PClan->DevelopHelp   = false;
}

static bool User_Create(void)
/*
 * Creates a new clan for the player.
 */
{
	FILE *fpPlayerFile;
	struct pc TmpPC = {0};
	struct UserInfo User;
	int16_t iTemp;
	char szString[128];

	strlcpy(PClan->szUserName, od_control.user_name, sizeof(PClan->szUserName));

	Help("Welcome", ST_CLANSHLP);

	// (YesNo("|0SDo you wish to join this game of clans?") == NO)
	if (YesNo(ST_STUFF1) == NO) {
		rputs(ST_ABORTED);
		return false;
	}

	if (!ChooseClanName(PClan->szName))
		return false;


	// initialize clan data
	PClan->Symbol[0] = 0;

	for (iTemp = 0; iTemp < 8; iTemp++) {
		PClan->QuestsDone[iTemp] = 0;
		PClan->QuestsKnown[iTemp] = 0;
	}

	PClan->WorldStatus = WS_STAYING;
	PClan->DestinationBBS = -1;

	PClan->Points = 0;
	strlcpy(PClan->szDateOfLastGame, System.szTodaysDate, sizeof(PClan->szDateOfLastGame));

	// Reset help database for him
	User_ResetHelp();

	PClan->QuestToday = false;
	PClan->VaultWithdrawals = 0;
	PClan->AttendedMass = false;
	PClan->GotBlessing = false;
	PClan->Prayed = false;
	PClan->FightsLeft = Game.Data.MineFights;
	PClan->ClanFights = Game.Data.ClanFights;
	PClan->WasRulerToday = false;
	PClan->ClanWars = 0;
	PClan->ChatsToday = 0;
	ClearFlags(PClan->PFlags);
	ClearFlags(PClan->DFlags);
	PClan->ResUncToday = 0;
	PClan->ResDeadToday = 0;
	PClan->MineLevel = 1;
	PClan->PublicMsgIndex = 0;
	PClan->MadeAlliance = false;
	PClan->Eliminated = false;
	PClan->WasRulerToday = false;
	PClan->FirstDay = true;
	PClan->Protection = (unsigned)(Game.Data.DaysOfProtection & 0x0F);

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
		PClan->Items[iTemp].Available = false;


	// Figure out ID
	PClan->ClanID[0] = Config.BBSID;
	PClan->ClanID[1] = Game.Data.NextClanID;
	Game.Data.NextClanID++;

	// init empire
	Empire_Create(&PClan->Empire, true);
	strlcpy(PClan->Empire.szName, PClan->szName, sizeof(PClan->Empire.szName));
	PClan->Empire.OwnerType = EO_CLAN;
	PClan->Empire.AllianceID = -1;

	// Create the players
	for (iTemp = 0; iTemp < Game.Data.MaxPermanentMembers; iTemp++) {
		od_clr_scr();

		// creating clansmen x of
		snprintf(szString, sizeof(szString), ST_STUFF5, (int)(iTemp + 1), (int)(Game.Data.MaxPermanentMembers));
		rputs(szString);

		if (iTemp == 0) {
			/* first clansman is the "leader" of the four */
			Help("Clan Leader", ST_NEWBIEHLP);
			rputs("\n%P");
		}

		if (iTemp == 0)
			PC_Create(&TmpPC, true);
		else
			PC_Create(&TmpPC, false);


		PClan->Member[iTemp] = malloc(sizeof(struct pc));
		CheckMem(PClan->Member[iTemp]);
		*PClan->Member[iTemp] = TmpPC;

		// give 'em gold dude
		PClan->Empire.VaultGold  += PClasses[ TmpPC.WhichClass ]->Gold;
		PClan->Empire.VaultGold += Races[ TmpPC.WhichRace ]->Gold;
	}

	/* open player file for append */
	fpPlayerFile = _fsopen(ST_CLANSPCFILE, "r+b", _SH_DENYRW);
	if (!fpPlayerFile) {
		fpPlayerFile = _fsopen(ST_CLANSPCFILE, "wb", _SH_DENYRW);
		if (!fpPlayerFile) {
			rputs("!! Chkpt 1\n");
			rputs(ST_ERRORPC);
			return false;
		}
	}
	else {
		fseek(fpPlayerFile, 0, SEEK_END);
	}

	/* write it to file */
	EncryptWrite_s(clan, PClan, fpPlayerFile, XOR_USER);
	for (iTemp = 0; iTemp < Game.Data.MaxPermanentMembers; iTemp++) {
		EncryptWrite_s(pc, PClan->Member[iTemp], fpPlayerFile, XOR_PC);
	}

	/* write null players to complete it */
	TmpPC.szName[0] = 0;
	TmpPC.Status = Dead;
	for (iTemp = Game.Data.MaxPermanentMembers; iTemp < 6; iTemp++) {
		EncryptWrite_s(pc, &TmpPC, fpPlayerFile, XOR_PC);
	}

	fclose(fpPlayerFile);

	/* write news */
	snprintf(szString, sizeof(szString), ST_NEWSNEWCLAN, PClan->szName);
	News_AddNews(szString);


	// add to league
	if (Game.Data.InterBBS) {
		User.ClanID[0] = PClan->ClanID[0];
		User.ClanID[1] = PClan->ClanID[1];
		strlcpy(User.szMasterName, PClan->szUserName, sizeof(User.szMasterName));
		strlcpy(User.szName, PClan->szName, sizeof(User.szName));
		User.Deleted = false;
		IBBS_LeagueNewUser(&User);
	}



	return true;
}


// ------------------------------------------------------------------------- //

void User_Destroy(void)
/*
 * Destroys the clan in memory.
 */
{
	int16_t iTemp;

	for (iTemp = 0; iTemp < MAX_MEMBERS; iTemp++)
		if (PClan->Member[iTemp])
			free(PClan->Member[iTemp]);

	free(PClan);
	User.Initialized = false;
}

// ------------------------------------------------------------------------- //

void CopyPC(struct pc *PCDest, struct pc *PCSrc)
{
	int16_t iTemp;

	strlcpy(PCDest->szName, PCSrc->szName, sizeof(PCDest->szName));
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

static bool User_Read(void)
/*
 * Reads the PClan from file which corresponds to the user online.
 * Returns false if clan was not found (i.e. not in game yet)
 */
{
	// search for user which matches current user logged in

	FILE *fpPlayerFile;
	struct clan TmpClan = {0};
	struct pc TmpPC = {0};
	int16_t CurClan, CurMember, iTemp;
	long Offset;

	fpPlayerFile = _fsopen(ST_CLANSPCFILE, "rb", _SH_DENYRW);
	if (!fpPlayerFile) {
		/* file not found, so clan not found! */
		return false;
	}

	for (CurClan = 0;; CurClan++) {
		/* seek to the current player */
		Offset = (long)CurClan * (BUF_SIZE_clan + 6L * BUF_SIZE_pc);

		if (fseek(fpPlayerFile, Offset, SEEK_SET))
			break;  /* couldn't fseek, so exit */

		notEncryptRead_s(clan, &TmpClan, fpPlayerFile, XOR_USER)
			break;  /* stop reading if no more players found */

		/* see if this player has same name as user online */
		if (strcasecmp(od_control.user_name, TmpClan.szUserName) == 0) {

			/* are the same, found player, copy it, return true */
			*PClan = TmpClan;

			/* read in the other guys, members, etc. */
			for (CurMember = 0; CurMember < MAX_MEMBERS; CurMember++)
				PClan->Member[CurMember] = NULL;

			for (CurMember = 0; CurMember < 6; CurMember++) {
				/* read 'em in */
				EncryptRead_s(pc, &TmpPC, fpPlayerFile, XOR_PC);

				// [0] != 0 if member exists
				if (TmpPC.szName[0]) {
					PClan->Member[CurMember] = malloc(sizeof(struct pc));
					CheckMem(PClan->Member[CurMember]);

					*PClan->Member[CurMember] = TmpPC;
					PClan->Member[CurMember]->MyClan = PClan;

					/* set all spells to 0 */
					for (iTemp = 0; iTemp < 10; iTemp++)
						PClan->Member[CurMember]->SpellsInEffect[iTemp].SpellNum = -1;

				}
			}

			fclose(fpPlayerFile);
			return true;
		}

	}

	/* was not found, so return false) */
	fclose(fpPlayerFile);

	return false;
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
	int16_t CurClan, iTemp;
	long OldOffset, Offset;
	struct clan TmpClan = {0};
	struct pc TmpPC = {0};

	fpPlayerFile = fopen(ST_CLANSPCFILE, "r+b");
	if (!fpPlayerFile) {
		/* !! */
		rputs("!! Chkpt 2\n");
		rputs(ST_ERRORPC);
		return;  /* failed to find clan */
	}

	for (CurClan = 0;; CurClan++) {
		/* go through file till you find clan he wants */

		Offset = (long)CurClan * (BUF_SIZE_clan + 6L * BUF_SIZE_pc);
		if (fseek(fpPlayerFile, Offset, SEEK_SET))
			break;  /* couldn't fseek, so exit */

		OldOffset = ftell(fpPlayerFile);

		notEncryptRead_s(clan, &TmpClan, fpPlayerFile, XOR_USER)
			break;  /* stop reading if no more players found */

		/* skip if deleted clan */
		if (TmpClan.ClanID[0] == -1)
			continue;

		/* if same Ids, seek back and write to file */
		if (TmpClan.ClanID[0] == Clan->ClanID[0] &&
				TmpClan.ClanID[1] == Clan->ClanID[1]) {
			fseek(fpPlayerFile, OldOffset, SEEK_SET);

			s_clan_s(Clan, serBuf, BUF_SIZE_clan);
			if (!EncryptWrite(serBuf, BUF_SIZE_clan, fpPlayerFile, XOR_USER))
				break;

			// fwrite players
			TmpPC.szName[0] = 0;
			TmpPC.Status = Dead;
			for (iTemp = 0; iTemp < 6; iTemp++) {
				if (Clan->Member[iTemp] && Clan->Member[iTemp]->Undead == false) {
					s_pc_s(Clan->Member[iTemp], serBuf, BUF_SIZE_pc);
					if (!EncryptWrite(serBuf, BUF_SIZE_pc, fpPlayerFile, XOR_PC))
						break;
				}
				else {
					s_pc_s(&TmpPC, serBuf, BUF_SIZE_pc);
					if (!EncryptWrite(serBuf, BUF_SIZE_pc, fpPlayerFile, XOR_PC))
						break;
				}
			}
			break;
		}
	}
	fclose(fpPlayerFile);
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
	int16_t iTemp;

	PClan->Points += 25;

	for (iTemp = 0; iTemp < MAX_MEMBERS; iTemp++) {
		if (PClan->Member[iTemp] && PClan->Member[iTemp]->Status != Dead) {
			PClan->Member[iTemp]->HP = PClan->Member[iTemp]->MaxHP;
			PClan->Member[iTemp]->SP = PClan->Member[iTemp]->MaxSP;

			if (PClan->Member[iTemp]->Status == Unconscious)
				PClan->Member[iTemp]->Status = Here;
		}
	}

	strlcpy(PClan->szDateOfLastGame, System.szTodaysDate, sizeof(PClan->szDateOfLastGame));
}

// ------------------------------------------------------------------------- //

int16_t NumMembers(struct clan *Clan, bool OnlyOnesHere)
/*
 * Returns the number of members in the clan.
 *
 * PRE: OnlyOnesHere is true if only the alive members are to be checked.
 */
{
	int16_t iTemp;
	int16_t Members = 0;

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

bool GetClanID(int16_t ID[2], bool OnlyLiving, bool IncludeSelf,
			   int16_t WhichAlliance, bool InAllianceOnly)
{
	FILE *fpPlayerFile;
	const char *apszClanNames[50];
	int16_t CurClan, ClanIDs[50][2], WhichClan, NumClans, iTemp;
	bool AllianceFound;
	struct clan TmpClan = {0};
	struct pc TmpPC = {0};
	bool FoundClan = false, AtLeastOneLiving = false;

	for (CurClan = 0; CurClan < 50; CurClan++)
		apszClanNames[CurClan] = NULL;

	// get list of all clan names from file, write to
	NumClans = 0;
	for (CurClan = 0; CurClan < 50; CurClan++) {
		fpPlayerFile = _fsopen(ST_CLANSPCFILE, "rb", _SH_DENYRW);
		if (!fpPlayerFile) {
			/* !! */
			rputs("!! Chkpt 3\n");
			rputs(ST_ERRORPC);
			break;
		}
		if (fseek(fpPlayerFile, (long)CurClan * (BUF_SIZE_clan + 6L * BUF_SIZE_pc), SEEK_SET)) {
			fclose(fpPlayerFile);
			break;  /* couldn't fseek, so exit */
		}

		notEncryptRead_s(clan, &TmpClan, fpPlayerFile, XOR_USER) {
			fclose(fpPlayerFile);
			break;  /* stop reading if no more players found */
		}

		// see if any of the members are alive
		if (OnlyLiving) {
			AtLeastOneLiving = false;
			for (iTemp = 0; iTemp < 6; iTemp++) {
				EncryptRead_s(pc, &TmpPC, fpPlayerFile, XOR_PC);

				if (TmpPC.szName[0] && TmpPC.Status == Here) {
					AtLeastOneLiving = true;
					break;
				}
			}
		}

		fclose(fpPlayerFile);

		/* skip if deleted clan or eliminated */
		if (TmpClan.ClanID[0] == -1)
			continue;

		/* skip if your clan */
		if (IncludeSelf == false &&
				TmpClan.ClanID[0] == PClan->ClanID[0] &&
				TmpClan.ClanID[1] == PClan->ClanID[1])
			continue;

		/* skip if not in alliance and alliance used */
		if (WhichAlliance != -1) {
			AllianceFound = false;
			for (iTemp = 0; iTemp < MAX_ALLIES; iTemp++) {
				if (TmpClan.Alliances[iTemp] == WhichAlliance) {
					AllianceFound = true;
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
		if (AtLeastOneLiving == false && OnlyLiving)
			continue;

		apszClanNames[NumClans] = DupeStr(TmpClan.szName);

		// add to our list
		ClanIDs[NumClans][0] = TmpClan.ClanID[0];
		ClanIDs[NumClans][1] = TmpClan.ClanID[1];

		NumClans++;
	}

	if (NumClans != 0)
		GetStringChoice(apszClanNames, NumClans,
						"|0SEnter the name of the clan\n|0E> |0F",
						&WhichClan, true, DT_WIDE, true);
	else {
		rputs("No clans found!\n");
		WhichClan = -1;
	}

	if (WhichClan == -1) {
		FoundClan = false;
		rputs(ST_ABORTED);
	}
	else {
		FoundClan = true;
		ID[0] = ClanIDs[ WhichClan ][0];
		ID[1] = ClanIDs[ WhichClan ][1];
	}

	for (CurClan = 0; CurClan < 50; CurClan++) {
		// free up clan names
		if (apszClanNames[CurClan])
			free((void*)apszClanNames[CurClan]);
	}

	return FoundClan;
}

// ------------------------------------------------------------------------- //

bool GetClanNameID(char *szName, int16_t ID[2])
{
	FILE *fpPlayerFile;
	/*    char szString[255];*/
	int16_t CurClan = 0;
	bool FoundClan = false;
	struct clan TmpClan = {0};

	szName[0] = 0;

	for (;;) {
		/* go through file till you find clan he wants */

		fpPlayerFile = _fsopen(ST_CLANSPCFILE, "rb", _SH_DENYRW);
		if (!fpPlayerFile) {
			return false;  /* means failed to find clan */
		}
		if (fseek(fpPlayerFile, (long)CurClan * (BUF_SIZE_clan + 6L * BUF_SIZE_pc), SEEK_SET)) {
			fclose(fpPlayerFile);
			break;  /* couldn't fseek, so exit */
		}

		notEncryptRead_s(clan, &TmpClan, fpPlayerFile, XOR_USER) {
			fclose(fpPlayerFile);
			break;  /* stop reading if no more players found */
		}

		fclose(fpPlayerFile);

		/* see if name is the one */
		if (TmpClan.ClanID[0] == ID[0] && TmpClan.ClanID[1] == ID[1]) {
			/* found it! */
			strlcpy(szName, TmpClan.szName, sizeof(szName));
			FoundClan = true;
			break;
		}

		CurClan++;
	}

	return FoundClan;
}



// ------------------------------------------------------------------------- //
bool GetClan(int16_t ClanID[2], struct clan *TmpClan)
{
	FILE *fpPlayerFile;
	int16_t ClanNum, iTemp;
	char szFileName[PATH_SIZE];
	bool FoundClan = false;

	// make them all NULLs for safety
	for (iTemp = 0; iTemp < MAX_MEMBERS; iTemp++)
		TmpClan->Member[iTemp] = NULL;

	strlcpy(szFileName, ST_CLANSPCFILE, sizeof(szFileName));

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
			for (iTemp = 0; iTemp < MAX_MEMBERS; iTemp++)
				TmpClan->Member[iTemp] = NULL;
			return false;
		}

		// make them all NULLs for safety
		for (iTemp = 0; iTemp < MAX_MEMBERS; iTemp++)
			TmpClan->Member[iTemp] = NULL;

		if (TmpClan->ClanID[0] == ClanID[0] && TmpClan->ClanID[1] == ClanID[1]) {
			/* found it */
			FoundClan = true;

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

	if (FoundClan)
		return true;
	else
		return false;
}

// ------------------------------------------------------------------------- //

void User_List(void)
{
	/* /USER parameter -- displays user list */

	FILE *fpPlayerFile, *fpUserList;
	struct clan TmpClan = {0};
	int16_t CurClan/*, CurMember, iTemp*/;
	struct UserInfo User;
	long Offset;

	fputs(ST_USERLISTH, stdout);

	if (Game.Data.InterBBS == false) {
		fpPlayerFile = _fsopen(ST_CLANSPCFILE, "rb", _SH_DENYWR);
		if (!fpPlayerFile) {
			/* file not found, so clan not found! */
			return;
		}

		/* else, file opened properly, so look for player */

		// list local players
		for (CurClan = 0;; CurClan++) {
			/* seek to the current player */
			Offset = (long)CurClan * (BUF_SIZE_clan + 6L * BUF_SIZE_pc);
			if (fseek(fpPlayerFile, Offset, SEEK_SET))
				break;  /* couldn't fseek, so exit */

			notEncryptRead_s(clan, &TmpClan, fpPlayerFile, XOR_USER)
				break;  /* stop reading if no more players found */

			// print out name etc.
			printf("%2d|%2d %-20s  %-20s\n", TmpClan.ClanID[0], TmpClan.ClanID[1],
				   TmpClan.szUserName, TmpClan.szName);
		}

		/* was not found, so return false) */
		fclose(fpPlayerFile);
	}
	else {
		// list interbbs users using USERLIST.DAT
		fpUserList = _fsopen("userlist.dat", "rb", _SH_DENYWR);
		if (fpUserList) {
			for (;;) {
				notEncryptRead_s(UserInfo, &User, fpUserList, XOR_ULIST)
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
	struct clan TmpClan = {0};
	int16_t iTemp, iTemp2, CurItem, Level;
	int32_t XPRequired[MAX_LEVELS];

	LogDisplayStr("* User_Maint()\n");

	/* figure XP required per level */
	for (Level = 1; Level < MAX_LEVELS; Level++) {
		XPRequired[Level] = 50;

		for (iTemp = 1; iTemp <= Level; iTemp++)
			XPRequired[Level] += (((int32_t)iTemp - 1) * 75);
	}

	fpOldPC = _fsopen(ST_CLANSPCFILE, "rb", _SH_DENYRW);
	if (fpOldPC) {
		fpNewPC = _fsopen(ST_NEWPCFILE, "w+b", _SH_DENYRW);
		if (!fpNewPC) {
			System_Error("New.PC unopenable");
		}

		for (;;) {
			/* go through each clan and write his updated info to new file */

			notEncryptRead_s(clan, &TmpClan, fpOldPC, XOR_USER)
				break;

			/* skip if deleted */
			if (TmpClan.ClanID[0] == -1)
				continue;

			/* FIXME: if over 14 days old, delete */
			// add to "list" of users who are going to be deleted

			/* update stats needed */
			TmpClan.FirstDay = false;

			// fix bugs
			if (TmpClan.Empire.VaultGold < 0)  TmpClan.Empire.VaultGold = 0;

			TmpClan.QuestToday = false;
			TmpClan.VaultWithdrawals = 0;
			TmpClan.AttendedMass = false;
			TmpClan.GotBlessing = false;
			TmpClan.Prayed = false;
			TmpClan.FightsLeft = Game.Data.MineFights;
			TmpClan.ClanFights = Game.Data.ClanFights;
			TmpClan.WasRulerToday = false;
			TmpClan.ClanWars = 0;
			TmpClan.ChatsToday = 0;
			ClearFlags(TmpClan.DFlags);
			TmpClan.ResUncToday = 0;
			TmpClan.ResDeadToday = 0;

			// reduce days of protection
			if (TmpClan.Protection > 0)
				TmpClan.Protection--;
			else
				TmpClan.Protection = 0;
			TmpClan.TradesToday = 0;

			Empire_Maint(&TmpClan.Empire);

			for (iTemp = 0; iTemp < MAX_CLANCOMBAT; iTemp++) {
				TmpClan.ClanCombatToday[iTemp][0] = -1;
				TmpClan.ClanCombatToday[iTemp][1] = -1;
			}

			if (TmpClan.Empire.VaultGold < 0)
				TmpClan.Empire.VaultGold = 0;

			/* WAS this the ruler and WAS he ousted? */
			/* REP:
			if (OustedRuler && OldRulerId[0] == TmpClan.ClanID[0] &&
			  OldRulerId[1] == TmpClan.ClanID[1])
			{
			  TmpClan.WasRulerToday = true;
			  TmpClan.Points -= 100;
			}
			*/

			/* is this the current ruler?  If so, give daily points */
			if (TmpClan.ClanID[0] == Village.Data.RulingClanId[0] &&
					TmpClan.ClanID[1] == Village.Data.RulingClanId[1]) {
				TmpClan.Points += 25;
			}

			// read in 6 members
			for (iTemp = 0; iTemp < 6; iTemp++) {
				TmpClan.Member[iTemp] = malloc(sizeof(struct pc));
				CheckMem(TmpClan.Member[iTemp]);
				EncryptRead_s(pc, TmpClan.Member[iTemp], fpOldPC, XOR_PC);
			}

			/* set HP to max */
			for (iTemp = 0; iTemp < Game.Data.MaxPermanentMembers; iTemp++) {
				/* if player is unconscious, revive him */
				if (TmpClan.Member[iTemp]->Status == Unconscious ||
						TmpClan.Member[iTemp]->Status == Here) {
					TmpClan.Member[iTemp]->Status = Here;
					TmpClan.Member[iTemp]->HP = TmpClan.Member[iTemp]->MaxHP;
					TmpClan.Member[iTemp]->SP = TmpClan.Member[iTemp]->MaxSP;
				}
				// if XP < 0, set it to what it should be
				if (TmpClan.Member[iTemp]->Experience <= 0) {
					TmpClan.Member[iTemp]->Experience =
						XPRequired[TmpClan.Member[iTemp]->Level];
				}
				if (TmpClan.Member[iTemp]->Experience >
						XPRequired[TmpClan.Member[iTemp]->Level+1]) {
					TmpClan.Member[iTemp]->Experience =
						XPRequired[TmpClan.Member[iTemp]->Level+1];
				}
			}

			// "release" NPC members
			for (iTemp = Game.Data.MaxPermanentMembers; iTemp < 6; iTemp++)
				if (TmpClan.Member[iTemp]->szName[0]) {
					// is a npc-player
					//printf("releasing %s\n", TmpClan.Member[iTemp]->szName);

					for (iTemp2 = 0; iTemp2 < MAX_ITEMS_HELD; iTemp2++) {
						/* if held by deleted char, remove link */
						if (TmpClan.Items[iTemp2].Available &&
								TmpClan.Items[iTemp2].UsedBy == iTemp+1) {
							TmpClan.Items[iTemp2].UsedBy = 0;
						}
					}

					// delete him now
					TmpClan.Member[iTemp]->szName[0] = 0;
				}

			// fix up screwed up items, make them owned by no one
			for (CurItem = 0; CurItem < MAX_ITEMS_HELD; CurItem++) {
				/* if held by deleted char, remove link */
				if (TmpClan.Items[CurItem].UsedBy < 0 ||
						TmpClan.Items[CurItem].UsedBy > Game.Data.MaxPermanentMembers) {
					TmpClan.Items[CurItem].UsedBy = 0;
				}
			}

			/* make it so they stop using invalid items */
			for (iTemp = 0; iTemp < Game.Data.MaxPermanentMembers; iTemp++) {
				if (TmpClan.Member[iTemp]->szName[0]) {
					if (TmpClan.Member[iTemp]->Weapon &&
							TmpClan.Items[ TmpClan.Member[iTemp]->Weapon-1 ].Available == false) {
						TmpClan.Member[iTemp]->Weapon = 0;
					}
					if (TmpClan.Member[iTemp]->Armor &&
							TmpClan.Items[ TmpClan.Member[iTemp]->Armor-1 ].Available == false) {
						TmpClan.Member[iTemp]->Armor = 0;
					}
					if (TmpClan.Member[iTemp]->Shield &&
							TmpClan.Items[ TmpClan.Member[iTemp]->Shield-1 ].Available == false) {
						TmpClan.Member[iTemp]->Shield = 0;
					}
				}
			}

			/* write new stuff to new file */
			EncryptWrite_s(clan, &TmpClan, fpNewPC, XOR_USER);

			for (iTemp = 0; iTemp < 6; iTemp++) {
				EncryptWrite_s(pc, TmpClan.Member[iTemp], fpNewPC, XOR_USER);
				free(TmpClan.Member[iTemp]);
				TmpClan.Member[iTemp] = NULL;
			}
		}

		fclose(fpOldPC);
		fclose(fpNewPC);

		/* delete old file, rename new one */
		unlink(ST_CLANSPCFILE);
		rename(ST_NEWPCFILE, ST_CLANSPCFILE);
	}
}


// ------------------------------------------------------------------------- //

bool User_Init(void)
/*
 * The current user online is initialized.  His stats are read in from
 * the .PC file.  If not found, a new clan is created.
 *
 * Returns false if user is new and didn't want to join the game.
 */
{
	// Returns true if user was created/loaded successfully, otherwise,
	// returns false
	int16_t iTemp;

	User.Initialized = true;
	PClan = malloc(sizeof(struct clan));
	CheckMem(PClan);

	for (iTemp = 0; iTemp < MAX_MEMBERS; iTemp++)
		PClan->Member[iTemp] = NULL;

	if (!User_Read()) {
		// see if in league already but he's just not on this
		// BBS
		if (IBBS_InList(od_control.user_name, false)) {
			// YES, he is IN the list already but he's not on
			// this BBS, so tell him that

			rputs("You're already logged as a user on another BBS.\n%P");
			User_Destroy();
			return false;
		}

		if (!User_Create()) {
			User_Destroy();
			return false;
		}
	}

	/*
	  // ensure CRC is correct
	  if (!PClan->CRC)
	  {
	    User_Destroy();
	    System_Error("CLANS.PC data corrupt! [u]\n");
	  }

	  // ensure CRC is correct
	  for (iTemp = 0; iTemp < MAX_MEMBERS; iTemp++)
	    if (!PClan->Member[iTemp]->CRC)
	    {
	      User_Destroy();
	      System_Error("CLANS.PC data corrupt [m]!\n");
	    }
	*/


	User.UpdateUser = true;
	return true;
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
	if (User.Initialized == false) return;

	if (User.UpdateUser)
		User_Write();

	User_Destroy();
}
