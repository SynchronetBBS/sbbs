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
 * Empire ADT
 *
 *
 */

#include <stdio.h>
#include <stdlib.h>
#ifndef __FreeBSD__
#include <malloc.h>
#endif
#include <string.h>
#include <string.h>
#ifdef __unix__
#include "unix_wrappers.h"
#endif

#include "structs.h"
#include "language.h"
#include "mstrings.h"
#include "help.h"
#include "input.h"
#include "door.h"
#include "news.h"
#include <OpenDoor.h>
#include "user.h"
#include "fight.h"
#include "alliance.h"
#include "mail.h"
#include "packet.h"
#include "ibbs.h"
#include "myopen.h"

// empire owner types
#define EO_VILLAGE    0
#define EO_ALLIANCE     1
#define EO_CLAN       2

#define MAX_VILLAGEEMPIREATTACKS    2
#define MAX_EMPIREATTACKS           5

#define MAX_SPIES       10
#define SPY_COST        1000L

// our goals
#define G_OUSTRULER     0
#define G_STEALLAND     1
#define G_STEALGOLD     2
#define G_DESTROY       3

#define SCOST_FOOTMAN   15
#define SCOST_AXEMEN    30
#define SCOST_KNIGHT    60
#define SCOST_CATAPULT  120

// stats for army
#define DAMAGE_FACTOR   60
#define SPD_FOOTMEN     5
#define SPD_AXEMEN      3
#define SPD_KNIGHTS     7
#define OFF_FOOTMEN     6
#define OFF_AXEMEN      8
#define OFF_KNIGHTS     9
#define DEF_FOOTMEN     2
#define DEF_AXEMEN      4
#define DEF_KNIGHTS     1
#define VIT_FOOTMEN     8
#define VIT_AXEMEN      7
#define VIT_KNIGHTS     6


extern struct config *Config;
extern struct game Game;
extern struct Language *Language;
extern struct clan *PClan;
extern struct village Village;


struct BuildingType BuildingType[NUM_BUILDINGTYPES] = {
	{"Barracks",            5, 15,  30,  2500 },
	{"Walls",              15, 5,    7,  1000 },
	{"Towers",             10, 8,   12,  1500 },
	{"Steel Mill",          8, 20,  28,  3000 },
	{"Stables",            10, 20,  40,  5000 },
	{"Intell. Agencies",    9, 10,  25,  1000 },
	{"Security Centers",    9, 10,  25,  1000 },
	{"Gymnasiums",         10, 5,   20,  4500 },
	{"Developer's Halls",   7, 10,  15,  3000 },
	{"Shops",               8, 20,  30,  5000 }
};

extern struct ibbs IBBS;

void DestroyBuildings(_INT16 NumBuildings[MAX_BUILDINGS],
					  _INT16 NumDestroyed[MAX_BUILDINGS], _INT16 Percent, _INT16 *LandGained);

void EmpireAttack(struct empire *AttackingEmpire, struct Army *AttackingArmy,
				  struct empire *DefendingEmpire, struct AttackResult *Result,
				  _INT16 Goal, _INT16 ExtentOfAttack);

void ProcessAttackResult(struct AttackResult *AttackResult);

// ------------------------------------------------------------------------- //
void SendResultPacket(struct AttackResult *Result, _INT16 DestID)
{
	Result->ResultIndex = IBBS.Data->Nodes[DestID-1].Attack.SendIndex+1;
	IBBS.Data->Nodes[DestID-1].Attack.SendIndex++;

	IBBS_SendPacket(PT_ATTACKRESULT, sizeof(struct AttackResult), Result,
					DestID);
}


void ProcessAttackPacket(struct AttackPacket *AttackPacket)
{
	struct AttackResult *Result;
	_INT16 LandGained, iTemp;

	// initialize result beforehand
	Result = malloc(sizeof(struct AttackResult));
	CheckMem(Result);
	Result->Success = FALSE;
	Result->NoTarget= FALSE;
	Result->InterBBS = TRUE;
	Result->AttackerType = AttackPacket->AttackingEmpire.OwnerType;
	Result->AttackerID[0] = AttackPacket->AttackOriginatorID[0];
	Result->AttackerID[1] = AttackPacket->AttackOriginatorID[1];
	Result->AllianceID = -1;
	strcpy(Result->szAttackerName, AttackPacket->AttackingEmpire.szName);
	Result->DefenderType = AttackPacket->TargetType;
	Result->AttackIndex = AttackPacket->AttackIndex;

	if (Result->DefenderType == EO_VILLAGE) {
		Result->DefenderID[0] = Village.Data->RulingClanId[0];
		Result->DefenderID[1] = Village.Data->RulingClanId[1];
		strcpy(Result->szDefenderName, Village.Data->szName);
	}
	else if (Result->DefenderType == EO_CLAN) {
		Result->DefenderID[0] = AttackPacket->ClanID[0];
		Result->DefenderID[1] = AttackPacket->ClanID[1];
		GetClanNameID(Result->szDefenderName, Result->DefenderID);
	}

	Result->BBSIDFrom = AttackPacket->BBSFromID;
	Result->BBSIDTo   = IBBS.Data->BBSID;

	Result->PercentDamage = 0;
	Result->Goal = AttackPacket->Goal;
	Result->ExtentOfAttack = AttackPacket->ExtentOfAttack;

	Result->AttackCasualties.Footmen = 0;
	Result->AttackCasualties.Axemen  = 0;
	Result->AttackCasualties.Knights = 0;
	Result->DefendCasualties.Footmen = 0;
	Result->DefendCasualties.Axemen  = 0;
	Result->DefendCasualties.Knights = 0;

	for (iTemp = 0; iTemp < MAX_BUILDINGS; iTemp++)
		Result->BuildingsDestroyed[iTemp] = 0;

	Result->GoldStolen = 0;
	Result->LandStolen = 0;

	Result->ReturningArmy.Footmen = AttackPacket->AttackingArmy.Footmen;
	Result->ReturningArmy.Axemen  = AttackPacket->AttackingArmy.Axemen;
	Result->ReturningArmy.Knights = AttackPacket->AttackingArmy.Knights;

	// if no ruler, skip this
	if ((Village.Data->RulingClanId[0] == -1 && AttackPacket->TargetType == EO_VILLAGE
			&& AttackPacket->Goal == G_OUSTRULER)
			|| (ClanExists(Result->DefenderID) == FALSE && AttackPacket->TargetType == EO_CLAN)) {
		Result->NoTarget = TRUE;

		// set everything to 0
		Result->Success = FALSE;
		Result->OrigAttackArmy = AttackPacket->AttackingArmy;
	}
	else {
		EmpireAttack(&AttackPacket->AttackingEmpire, &AttackPacket->AttackingArmy,
					 &Village.Data->Empire, Result,
					 AttackPacket->Goal, AttackPacket->ExtentOfAttack);

		// process result -- this writes messages, updates news
		ProcessAttackResult(Result);
	}

	// send back result to other BBS
	SendResultPacket(Result, Result->BBSIDFrom);

	// update defender's army
	Village.Data->Empire.Army.Footmen -= Result->DefendCasualties.Footmen;
	Village.Data->Empire.Army.Axemen  -= Result->DefendCasualties.Axemen;
	Village.Data->Empire.Army.Knights -= Result->DefendCasualties.Knights;

	// "give" the defender land from which his buildings came from
	//   this land is "gained" because of buildings destroyed
	LandGained = 0;
	for (iTemp = 0; iTemp < NUM_BUILDINGTYPES; iTemp++) {
		LandGained += (Result->BuildingsDestroyed[iTemp]*BuildingType[iTemp].LandUsed);
	}
	Village.Data->Empire.Land += LandGained;

	// update his losses
	Village.Data->Empire.VaultGold -= Result->GoldStolen;
	Village.Data->Empire.Land -= Result->LandStolen;

	for (iTemp = 0; iTemp < MAX_BUILDINGS; iTemp++) {
		Village.Data->Empire.Buildings[iTemp] -= Result->BuildingsDestroyed[iTemp];
	}

	free(Result);
}



// ------------------------------------------------------------------------- //
void ProcessResultPacket(struct AttackResult *Result)
{
	// if village, update village's stats

	// if clan, update his stats

	struct Packet Packet;
	struct AttackPacket *AttackPacket;
	struct clan *TmpClan;
	char szNews[255], szAttackerName[35], szDefenderName[40], *szMessage,
	szGoal[40], szOutcome[30], *szString, *cpBuffer;
	_INT16 WhichBBS, iTemp, Junk[2];
	BOOL ShowedOne;
	long AfterOffset, BeforeOffset;
	FILE *fpBackup;


	WhichBBS = Result->BBSIDTo-1;

	switch (Result->AttackerType) {
		case EO_VILLAGE :
			// update attack's army
			Village.Data->Empire.Army.Footmen += Result->ReturningArmy.Footmen;
			Village.Data->Empire.Army.Axemen  += Result->ReturningArmy.Axemen;
			Village.Data->Empire.Army.Knights += Result->ReturningArmy.Knights;

			// give attacker his land
			Village.Data->Empire.Land         += Result->LandStolen;
			Village.Data->Empire.VaultGold    += Result->GoldStolen;
			strcpy(szAttackerName, Village.Data->szName);
			break;
		case EO_CLAN :
			TmpClan = malloc(sizeof(struct clan));
			CheckMem(TmpClan);
			if (GetClan(Result->AttackerID, TmpClan)) {
				TmpClan->Empire.Army.Footmen += Result->ReturningArmy.Footmen;
				TmpClan->Empire.Army.Axemen  += Result->ReturningArmy.Axemen;
				TmpClan->Empire.Army.Knights += Result->ReturningArmy.Knights;

				// give attacker his land
				TmpClan->Empire.Land     += Result->LandStolen;
				TmpClan->Empire.VaultGold  += Result->GoldStolen;

				// give points for win, take away some for loss
				if (Result->Success)
					PClan->Points += 50;
				else
					PClan->Points -= 25;

				strcpy(szAttackerName, TmpClan->szName);

				Clan_Update(TmpClan);
			}
			FreeClan(TmpClan);
			break;
	}

	if (Result->DefenderType == EO_VILLAGE)
		sprintf(szDefenderName, "the village of %s", Result->szDefenderName);
	else
		sprintf(szDefenderName, "the clan of %s", Result->szDefenderName);

	// write to news
	if (Result->NoTarget && Result->Goal == G_OUSTRULER) {
		// sprintf(szNews, ">> %s's army returns after finding no ruler to oust in %s\n",
		sprintf(szNews, ST_WNEWS5,
				szAttackerName, IBBS.Data->Nodes[WhichBBS].Info.pszVillageName);
		News_AddNews(szNews);

		// strcpy(szOutcome, "but found no ruler to oust!\n");
		strcpy(szOutcome, ST_WNEWS6);
	}
	else if (Result->NoTarget) {
		// sprintf(szNews, ">> %s's army returns after being unable to find the clan empire.\n",
		sprintf(szNews, ST_WNEWS7, szAttackerName);
		News_AddNews(szNews);

		// strcpy(szOutcome, "but found no empire!\n");
		strcpy(szOutcome, ST_WNEWS8);
	}
	else if (Result->Success) {
		// strcpy(szOutcome, "and came out victorious!\n");
		strcpy(szOutcome, ST_WNEWS9);

		switch (Result->Goal) {
			case G_OUSTRULER :
				// sprintf(szNews, ">> %s's army returns after successfully ousting the ruler of %s\n\n",
				sprintf(szNews, ST_WNEWS10,
						szAttackerName, IBBS.Data->Nodes[WhichBBS].Info.pszVillageName);
				News_AddNews(szNews);
				break;
			case G_STEALLAND :
				// sprintf(szNews, ">> %s's army returns successfully from %s looting %d land from %s.\n\n",
				sprintf(szNews, ST_WNEWS11,
						szAttackerName,
						IBBS.Data->Nodes[WhichBBS].Info.pszVillageName,
						Result->LandStolen, szDefenderName);
				News_AddNews(szNews);
				break;
			case G_STEALGOLD :
				// sprintf(szNews, ">> %s's army returns successfully from %s looting %d gold from %s.\n\n",
				sprintf(szNews, ST_WNEWS12,
						szAttackerName,
						IBBS.Data->Nodes[WhichBBS].Info.pszVillageName,
						Result->GoldStolen, szDefenderName);
				News_AddNews(szNews);
				break;
			case G_DESTROY :
				// sprintf(szNews, ">> %s's army returns successfully from %s destroying %s's buildings.\n\n",
				sprintf(szNews, ST_WNEWS13,
						szAttackerName,
						IBBS.Data->Nodes[WhichBBS].Info.pszVillageName, szDefenderName);
				News_AddNews(szNews);
				break;
		}
	}
	else {
		// strcpy(szOutcome, "and came out defeated.\n");
		strcpy(szOutcome, ST_WNEWS14);
		// sprintf(szNews, ">> %s's army returns unsuccessfully from %s after attacking %s.\n\n",
		sprintf(szNews, ST_WNEWS15,
				szAttackerName,
				IBBS.Data->Nodes[WhichBBS].Info.pszVillageName, szDefenderName);
		News_AddNews(szNews);
	}

	szMessage = MakeStr(600);
	szString  = MakeStr(255);

	switch (Result->Goal) {
		case G_OUSTRULER :
			strcpy(szGoal, "to oust the ruler");
			break;
		case G_STEALLAND :
			strcpy(szGoal, "to steal land");
			break;
		case G_STEALGOLD :
			strcpy(szGoal, "to steal gold");
			break;
		case G_DESTROY :
			strcpy(szGoal, "to destroy");
			break;
	}

	// sprintf(szMessage, "Results of %s's attack on %s have returned.\n Your troops attempted %s %s.\n\nYou killed the following",
	sprintf(szMessage, ST_WNEWS16,
			Result->AttackerType == EO_VILLAGE ? "the town" : "your clan",
			szDefenderName, szGoal, szOutcome);

	// append what was lost and who you killed
	//    sprintf(szString, " %ld Footmen\n %ld Axemen\n %ld Knights \n",
	sprintf(szString, ST_WAR0,
			Result->DefendCasualties.Footmen,
			Result->DefendCasualties.Axemen,
			Result->DefendCasualties.Knights);
	strcat(szMessage, szString);
	//    sprintf(szString, "You lost the following:\n %ld Footmen\n %ld Axemen\n %ld Knights \n",
	sprintf(szString, ST_WAR1,
			Result->AttackCasualties.Footmen,
			Result->AttackCasualties.Axemen,
			Result->AttackCasualties.Knights);
	strcat(szMessage, szString);

	// sprintf(szString, "The following have returned:\n %ld Footmen\n %ld Axemen\n %ld Knights \n",
	sprintf(szString, ST_WNEWS17,
			Result->ReturningArmy.Footmen,
			Result->ReturningArmy.Axemen,
			Result->ReturningArmy.Knights);
	strcat(szMessage, szString);

	switch (Result->Goal) {
		case G_STEALLAND :
			if (Result->LandStolen) {
				// sprintf(szString, "You stole %ld land.\n", Result->LandStolen);
				sprintf(szString, ST_WNEWS18, Result->LandStolen);
				strcat(szMessage, szString);
			}
			else if (Result->Success)
				// strcat(szMessage, "You found no land to steal!\n");
				strcat(szMessage, ST_WNEWS19);
			break;
		case G_STEALGOLD :
			if (Result->GoldStolen) {
				// sprintf(szString, "You stole %ld gold.\n", Result->GoldStolen);
				sprintf(szString, ST_WNEWS20, Result->GoldStolen);
				strcat(szMessage, szString);
			}
			else if (Result->Success)
				// strcat(szMessage, "You found no gold to steal!\n");
				strcat(szMessage, ST_WNEWS21);
			break;
	}

	ShowedOne = FALSE;
	for (iTemp = 0; iTemp < NUM_BUILDINGTYPES; iTemp++) {
		if (Result->BuildingsDestroyed[iTemp] == 0)
			continue;

		if (!ShowedOne) {
			// strcat(szMessage, "The following buildings were destroyed:\n");
			strcat(szMessage, ST_PAR2);
			ShowedOne = TRUE;
		}
		sprintf(szString, ST_WRESULTS10, Result->BuildingsDestroyed[iTemp],
				BuildingType[iTemp].szName);
		strcat(szMessage, szString);
	}

	GenericMessage(szMessage, Result->AttackerID, Junk, "", FALSE);

	free(szMessage);
	free(szString);


	// now, wipe out that packet from backup.dat
	fpBackup = fopen("backup.dat", "r+b");
	if (!fpBackup) {
		return;
	}

	for (;;) {
		BeforeOffset = ftell(fpBackup);

		if (EncryptRead(&Packet, sizeof(struct Packet), fpBackup, XOR_PACKET) == 0)
			break;

		AfterOffset = ftell(fpBackup);

		if (Packet.Active) {
			if (Packet.PacketType == PT_ATTACK) {
				// get attackpacket
				AttackPacket = malloc(sizeof(struct AttackPacket));
				CheckMem(AttackPacket);
				EncryptRead(AttackPacket, sizeof(struct AttackPacket), fpBackup, XOR_PACKET);

				// if same AttackIndex, mark it off
				if (AttackPacket->AttackIndex == Result->AttackIndex) {
					Packet.Active = FALSE;

					fseek(fpBackup, BeforeOffset, SEEK_SET);

					/* write it to file */
					EncryptWrite(&Packet, sizeof(struct Packet), fpBackup, XOR_PACKET);
				}
				free(AttackPacket);
			}

			fseek(fpBackup, AfterOffset, SEEK_SET);
		}

		// move on
		if (Packet.PacketLength)
			fseek(fpBackup, Packet.PacketLength, SEEK_CUR);
	}

	fclose(fpBackup);

	(void)cpBuffer;

}


// ------------------------------------------------------------------------- //

void Empire_Create(struct empire *Empire, BOOL UserEmpire)
{
	_INT16 iTemp;

	if (UserEmpire && Game.Data->ClanEmpires) {
		Empire->Land = 100;
	}
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

// ------------------------------------------------------------------------- //

void Empire_Maint(struct empire *Empire)
{
	// run "maintenance" on empire
	char szNews[128];
	long GoldMade;
	_INT16 iTemp, Rating;

	Empire->WorkerEnergy = 100;
	Empire->LandDevelopedToday = 0;
	Empire->SpiesToday = 0;
	Empire->AttacksToday = 0;

	Rating = Empire->Army.Rating;
	Rating += (RANDOM(10) + 2*Empire->Buildings[B_GYM]);
	if (Rating > 100)
		Rating = 100;
	if (Rating < 0)
		Rating = 0;
	Empire->Army.Rating = Rating;

	// make money for empire here
	if (Empire->OwnerType == EO_VILLAGE) {
		GoldMade = 0;

		// each business makes about 1000 gold
		for (iTemp = 0; iTemp < Empire->Buildings[B_BUSINESS]; iTemp++) {
			GoldMade += (1000 + RANDOM(500) + RANDOM(500));
		}

		// sprintf(szNews, ">> Businesses brought in %ld gold today!\n\n",
		sprintf(szNews, ST_NEWS3, GoldMade);
		News_AddNews(szNews);
		Empire->VaultGold += GoldMade;
	}

}

// ------------------------------------------------------------------------- //

_INT16 ArmySpeed(struct Army *Army)
{
	_INT16 Speed, NumTroops;

	NumTroops = Army->Footmen + Army->Axemen + Army->Knights;

	if (NumTroops)
		Speed = (Army->Footmen*SPD_FOOTMEN + Army->Axemen*SPD_AXEMEN
				 + Army->Knights*SPD_KNIGHTS) / NumTroops;
	else
		Speed = 0;

	return Speed;
}


long ArmyOffense(struct Army *Army)
{
	long Offense, NumTroops;

	Offense = Army->Footmen*OFF_FOOTMEN + Army->Axemen*OFF_AXEMEN
			  + Army->Knights*OFF_KNIGHTS;

	(void)NumTroops;
	return Offense;
}


long ArmyDefense(struct Army *Army)
{
	long Defense, NumTroops;

	Defense = Army->Footmen*DEF_FOOTMEN + Army->Axemen*DEF_AXEMEN
			  + Army->Knights*DEF_KNIGHTS;

	(void)NumTroops;
	return Defense;
}

long ArmyVitality(struct Army *Army)
{
	long Vitality, NumTroops;

	Vitality = (Army->Footmen*VIT_FOOTMEN + Army->Axemen*VIT_AXEMEN
				+ Army->Knights*VIT_KNIGHTS) * 2;

	(void)NumTroops;
	return Vitality;
}


void Empire_Stats(struct empire *Empire)
{
	char szString[128];
	_INT16 iTemp;
	BOOL ShowedOne;

	od_clr_scr();

	sprintf(szString, ST_ESTATS0, Empire->szName);
	rputs(szString);

	rputs(ST_LONGDIVIDER);

	sprintf(szString, ST_ESTATS1, Empire->VaultGold);
	rputs(szString);
	sprintf(szString, ST_ESTATS2, Empire->Land);
	rputs(szString);

	sprintf(szString, ST_ESTATS3, Empire->WorkerEnergy);
	rputs(szString);

	rputs(ST_ESTATS4);
	sprintf(szString, ST_ESTATS5, Empire->Army.Footmen);
	rputs(szString);
	sprintf(szString, ST_ESTATS6, Empire->Army.Axemen);
	rputs(szString);
	sprintf(szString, ST_ESTATS7, Empire->Army.Knights);
	rputs(szString);
	sprintf(szString, ST_ESTATS8, Empire->Army.Rating);
	rputs(szString);

	// show stats
	sprintf(szString, ST_ESTATS9, ArmySpeed(&Empire->Army));
	rputs(szString);
	sprintf(szString, ST_ESTATS10, ArmyVitality(&Empire->Army));
	rputs(szString);
	sprintf(szString, ST_ESTATS11, ArmyOffense(&Empire->Army));
	rputs(szString);
	sprintf(szString, ST_ESTATS12, ArmyDefense(&Empire->Army));
	rputs(szString);

	rputs(ST_ESTATS13);
	ShowedOne = FALSE;
	for (iTemp = 0; iTemp < NUM_BUILDINGTYPES; iTemp++) {
		if (Empire->Buildings[iTemp]) {
			ShowedOne = TRUE;
			sprintf(szString, "  %d %s\n", Empire->Buildings[iTemp],
					BuildingType[iTemp].szName);
			rputs(szString);
		}
	}
	if (!ShowedOne)
		rputs(ST_ESTATS14);

	rputs(ST_LONGDIVIDER);
	door_pause();
}

// ------------------------------------------------------------------------- //

void DevelopLand(struct empire *Empire)
{
	long LimitingVariable, CostToDevelop;
	_INT16 LandToDevelop;
	char szString[128];

	// show help for developing land
	if (!PClan->DevelopHelp) {
		PClan->DevelopHelp = TRUE;
		Help("Development Help", ST_NEWBIEHLP);
		rputs("\n%P");
	}

	// each unit of land costs this much to develop
	CostToDevelop = (long)(100L - Empire->Buildings[B_DEVELOPERS]*5L);
	if (CostToDevelop < 20)
		CostToDevelop = 20;
	// FIXME: in future have X developments as max per day?!

	LimitingVariable = Empire->VaultGold/CostToDevelop;

	if (LimitingVariable + Empire->Land >= 3000)
		LimitingVariable = 3000 - Empire->Land;

	if (LimitingVariable < 0)
		LimitingVariable = 0;


	sprintf(szString, ST_DEVLAND0, CostToDevelop);
	rputs(szString);

	LandToDevelop = GetLong(ST_DEVLAND1, 0, LimitingVariable);

	if (LandToDevelop) {
		Empire->Land += LandToDevelop;
		Empire->VaultGold -= (CostToDevelop*LandToDevelop);

		sprintf(szString, ST_DEVLAND2, LandToDevelop, (CostToDevelop*LandToDevelop));
		rputs(szString);

		Empire->LandDevelopedToday += LandToDevelop;
	}
}

void DonateToEmpire(struct empire *Empire)
{
	char *szTheOptions[15];
	char szString[128], szFileName[30];
	long LimitingVariable, NumToDonate;
	BOOL IsRuler;

	LoadStrings(1450, 15, szTheOptions);

	if (Empire->OwnerType == EO_VILLAGE)
		strcpy(szFileName, "Donate To Empire1");
	else
		strcpy(szFileName, "Donate To Empire2");

	/* get a choice */
	for (;;) {
		rputs("\n");
		rputs(" |0BDonation\n");
		rputs(ST_LONGLINE);

		IsRuler = PClan->ClanID[0] == Village.Data->RulingClanId[0] &&
				  PClan->ClanID[1] == Village.Data->RulingClanId[1];

		// show empire stats
		/*
		    if (Empire->OwnerType != EO_VILLAGE || (!IsRuler &&
		      Village.Data->ShowEmpireStats && Empire->OwnerType == EO_VILLAGE) ||
		      (Empire->OwnerType == EO_VILLAGE && IsRuler) )
		*/
		{
			sprintf(szString, ST_DEMPIRE0, Empire->Army.Followers);
			rputs(szString);
			sprintf(szString, ST_DEMPIRE1, Empire->Army.Footmen);
			rputs(szString);
			sprintf(szString, ST_DEMPIRE2, Empire->Army.Axemen);
			rputs(szString);
			sprintf(szString, ST_DEMPIRE3, Empire->Army.Knights);
			rputs(szString);
			sprintf(szString, ST_DEMPIRE4, Empire->VaultGold);
			rputs(szString);
			sprintf(szString, ST_DEMPIRE5, Empire->Land);
			rputs(szString);
		}
		/*
		    else if (Empire->OwnerType == EO_VILLAGE && IsRuler == FALSE &&
		      Village.Data->ShowEmpireStats == FALSE)
		    {
		      rputs(" |0CEmpire stats made unavailable by ruler.\n");
		    }
		*/

		switch (GetChoice(szFileName, ST_ENTEROPTION, szTheOptions, "12346789Q?V50AB", 'Q', TRUE)) {
			case '1' :      /* followers */
				LimitingVariable = PClan->Empire.Army.Followers;

				NumToDonate = GetLong(ST_DEMPIRE6, 0, LimitingVariable);

				if (NumToDonate) {
					Empire->Army.Followers += NumToDonate;
					PClan->Empire.Army.Followers -= NumToDonate;
				}
				break;
			case '2' :      /* footmen */
				// first limit is number of barracks they can own
				LimitingVariable = Empire->Buildings[B_BARRACKS]*20;

				// next, subtract from this the number of troops they already own
				LimitingVariable -= Empire->Army.Footmen;

				// this leaves how many they can take in

				// if you have less troops to donate than this,
				// you can only train that many
				if (PClan->Empire.Army.Footmen < LimitingVariable)
					LimitingVariable = PClan->Empire.Army.Footmen;

				// if less than 0, make it 0
				if (LimitingVariable < 0)
					LimitingVariable = 0;

				NumToDonate = GetLong(ST_DEMPIRE7, 0, LimitingVariable);

				if (NumToDonate) {
					Empire->Army.Footmen   += NumToDonate;
					PClan->Empire.Army.Footmen -= NumToDonate;
				}
				break;
			case '3' :      /* axemen */
				if (Empire->Buildings[B_STEELMILL] == 0) {
					rputs(ST_DEMPIRE18);
					break;
				}
				// first limit is number of barracks they can own
				LimitingVariable = Empire->Buildings[B_BARRACKS]*10;

				// next, subtract from this the number of troops they already own
				LimitingVariable -= Empire->Army.Axemen;

				// this leaves how many they can take in

				// if you have less troops to donate than this,
				// you can only train that many
				if (PClan->Empire.Army.Axemen < LimitingVariable)
					LimitingVariable = PClan->Empire.Army.Axemen;

				// if less than 0, make it 0
				if (LimitingVariable < 0)
					LimitingVariable = 0;

				NumToDonate = GetLong(ST_DEMPIRE8, 0, LimitingVariable);

				if (NumToDonate) {
					Empire->Army.Axemen   += NumToDonate;
					PClan->Empire.Army.Axemen -= NumToDonate;
				}
				break;
			case '4' :      /* Knights */
				if (Empire->Buildings[B_STEELMILL] == 0 ||
						Empire->Buildings[B_STABLES] == 0) {
					rputs(ST_DEMPIRE19);
					break;
				}
				// first limit is number of barracks they can own
				LimitingVariable = Empire->Buildings[B_BARRACKS]*5;

				// next, subtract from this the number of troops they already own
				LimitingVariable -= Empire->Army.Knights;

				// this leaves how many they can take in

				// if you have less troops to donate than this,
				// you can only train that many
				if (PClan->Empire.Army.Knights < LimitingVariable)
					LimitingVariable = PClan->Empire.Army.Knights;

				// if less than 0, make it 0
				if (LimitingVariable < 0)
					LimitingVariable = 0;

				NumToDonate = GetLong(ST_DEMPIRE9, 0, LimitingVariable);

				if (NumToDonate) {
					Empire->Army.Knights   += NumToDonate;
					PClan->Empire.Army.Knights -= NumToDonate;
				}
				break;
			case '7' :      /* withdraw footmen */
				if (Empire->OwnerType == EO_VILLAGE) {
					rputs(ST_DEMPIRE20);
					break;
				}

				// first limit is number of barracks you can own
				LimitingVariable = PClan->Empire.Buildings[B_BARRACKS]*20;

				// next, subtract from this the number of troops you already own
				LimitingVariable -= PClan->Empire.Army.Footmen;

				// this leaves how many you can take in

				// if they have less troops to give than this,
				// you can only take that many
				if (Empire->Army.Footmen < LimitingVariable)
					LimitingVariable = Empire->Army.Footmen;

				// if less than 0, make it 0
				if (LimitingVariable < 0)
					LimitingVariable = 0;

				NumToDonate = GetLong(ST_DEMPIRE11, 0, LimitingVariable);

				if (NumToDonate) {
					PClan->Empire.Army.Footmen += NumToDonate;
					Empire->Army.Footmen -= NumToDonate;
				}
				break;
			case '8' :      /* withdraw axemen */
				if (Empire->OwnerType == EO_VILLAGE) {
					rputs(ST_DEMPIRE20);
					break;
				}
				if (PClan->Empire.Buildings[B_STEELMILL] == 0) {
					rputs(ST_DEMPIRE21);
					break;
				}
				// first limit is number of barracks you can own
				LimitingVariable = PClan->Empire.Buildings[B_BARRACKS]*10;

				// next, subtract from this the number of troops you already own
				LimitingVariable -= PClan->Empire.Army.Axemen;

				// this leaves how many you can take in

				// if you have less troops to donate than this,
				// you can only train that many
				if (Empire->Army.Axemen < LimitingVariable)
					LimitingVariable = Empire->Army.Axemen;

				// if less than 0, make it 0
				if (LimitingVariable < 0)
					LimitingVariable = 0;

				NumToDonate = GetLong(ST_DEMPIRE12, 0, LimitingVariable);

				if (NumToDonate) {
					PClan->Empire.Army.Axemen   += NumToDonate;
					Empire->Army.Axemen -= NumToDonate;
				}
				break;
			case '9' :      /* withdraw Knights */
				if (Empire->OwnerType == EO_VILLAGE) {
					rputs(ST_DEMPIRE20);
					break;
				}
				if (PClan->Empire.Buildings[B_STEELMILL] == 0 ||
						PClan->Empire.Buildings[B_STABLES] == 0) {
					rputs(ST_DEMPIRE22);
					break;
				}
				// first limit is number of barracks you can own
				LimitingVariable = PClan->Empire.Buildings[B_BARRACKS]*5;

				// next, subtract from this the number of troops you already own
				LimitingVariable -= PClan->Empire.Army.Knights;

				// this leaves how many you can take in

				// if you have less troops to donate than this,
				// you can only train that many
				if (Empire->Army.Knights < LimitingVariable)
					LimitingVariable = Empire->Army.Knights;

				// if less than 0, make it 0
				if (LimitingVariable < 0)
					LimitingVariable = 0;

				NumToDonate = GetLong(ST_DEMPIRE13, 0, LimitingVariable);

				if (NumToDonate) {
					PClan->Empire.Army.Knights += NumToDonate;
					Empire->Army.Knights -= NumToDonate;
				}
				break;
			case '6' :      /* followers */
				if (Empire->OwnerType == EO_VILLAGE) {
					rputs(ST_DEMPIRE20);
					break;
				}
				LimitingVariable = Empire->Army.Followers;

				NumToDonate = GetLong(ST_DEMPIRE10, 0, LimitingVariable);

				if (NumToDonate) {
					PClan->Empire.Army.Followers += NumToDonate;
					Empire->Army.Followers -= NumToDonate;
				}
				break;
			case '5' :      /* land */
				LimitingVariable = PClan->Empire.Land;

				NumToDonate = GetLong(ST_DEMPIRE14, 0, LimitingVariable);

				if (NumToDonate) {
					PClan->Empire.Land -= NumToDonate;
					Empire->Land += NumToDonate;
				}
				break;
			case '0' :      /* take land */
				if (Empire->OwnerType == EO_VILLAGE) {
					rputs(ST_DEMPIRE20);
					break;
				}

				LimitingVariable = Empire->Land;

				NumToDonate = GetLong(ST_DEMPIRE15, 0, LimitingVariable);

				if (NumToDonate) {
					PClan->Empire.Land += NumToDonate;
					Empire->Land -= NumToDonate;
				}
				break;
			case 'A' :      /* give gold */
				if (Empire->OwnerType == EO_VILLAGE) {
					rputs(ST_DEMPIRE23);
					break;
				}
				LimitingVariable = PClan->Empire.VaultGold;

				NumToDonate = GetLong(ST_DEMPIRE16, 0, LimitingVariable);

				if (NumToDonate) {
					PClan->Empire.VaultGold -= NumToDonate;
					Empire->VaultGold += NumToDonate;
				}
				break;
			case 'B' :      /* take gold */
				if (Empire->OwnerType == EO_VILLAGE) {
					rputs(ST_DEMPIRE23);
					break;
				}

				LimitingVariable = Empire->VaultGold;

				NumToDonate = GetLong(ST_DEMPIRE17, 0, LimitingVariable);

				if (NumToDonate) {
					PClan->Empire.VaultGold += NumToDonate;
					Empire->VaultGold -= NumToDonate;
				}
				break;
			case 'Q' :      /* return to previous menu */
				return;
			case '?' :      /* redisplay options */
				break;
			case 'V' :      /* stats */
				ClanStats(PClan, TRUE);
				break;
		}
	}
}

void Destroy_Menu(struct empire *Empire)
{
	// allow user to build new structures

	_INT16 iTemp, WhichBuilding;
	char *szTheOptions[13], cKey, szString[128];

	LoadStrings(1270, 12, szTheOptions);
	szTheOptions[11] = "Buildings";
	szTheOptions[12] = "View Stats";

	for (;;) {
		rputs("\n\n");
		rputs(" |0BDestroy Menu\n");
		rputs(ST_LONGLINE);

		/* show stats */
		sprintf(szString, ST_MEMPIRE11, Empire->Land);
		rputs(szString);

		// show all buildings erected
		for (iTemp = 0; iTemp < NUM_BUILDINGTYPES; iTemp++) {
			// sprintf(szString, " |0A(|0B%c|0A) |0C%-20s    |0F%d\n\r", iTemp + '0', BuildingType[iTemp].szName,
			sprintf(szString, ST_MEMPIRE13, iTemp + '0', BuildingType[iTemp].szName,
					Empire->Buildings[iTemp]);
			rputs(szString);
		}
		// rputs(" |0A(|0BQ|0A) |0CQuit\n |0A(|0BV|0A) |0CView Stats\n");
		rputs(ST_MEMPIRE27);
		rputs(ST_LONGLINE);

		switch (cKey = GetChoice("", ST_ENTEROPTION, szTheOptions, "Q?0123456789V", 'Q', TRUE)) {
				// user chooses type to build here using 123456789
			case '0' :
			case '1' :
			case '2' :
			case '3' :
			case '4' :
			case '5' :
			case '6' :
			case '7' :
			case '8' :
			case '9' :
				WhichBuilding = cKey - '0';

				// show building stats
				sprintf(szString, ST_MEMPIRE15, BuildingType[ WhichBuilding ].szName);
				rputs(szString);
				sprintf(szString, ST_MEMPIRE17, BuildingType[ WhichBuilding ].LandUsed);
				rputs(szString);
				sprintf(szString, ST_MEMPIRE18, BuildingType[ WhichBuilding ].Cost);
				rputs(szString);

				Help(BuildingType[ WhichBuilding ].szName, ST_WARHLP);

				if (Empire->Buildings[WhichBuilding] <= 0) {
					Empire->Buildings[WhichBuilding] = 0;
					// rputs("|07You don't have any of that building to destroy.\n%P");
					rputs(ST_MEMPIRE29);
					break;
				}

				// if (NoYes("Destroy this?") == NO) break;
				if (NoYes(ST_MEMPIRE30) == NO) break;

				// "destroy" it now
				Empire->Land += BuildingType[ WhichBuilding ].LandUsed;
				Empire->VaultGold += ((BuildingType[ WhichBuilding ].Cost*1)/2);
				Empire->Buildings[WhichBuilding]--;

				// sprintf(szString, "%s destroyed.  %d land and %d gold gained.\n",
				sprintf(szString, ST_MEMPIRE31,
						BuildingType[ WhichBuilding ].szName,
						BuildingType[ WhichBuilding ].LandUsed,
						((BuildingType[ WhichBuilding ].Cost*1)/2));
				rputs(szString);
				break;
			case 'V' :      // clan stats
				ClanStats(PClan, TRUE);
				break;
			case 'Q' :      /* return to previous menu */
				return;
			case '?' :      /* redisplay options */
				break;
		}
	}
}

void StructureMenu(struct empire *Empire)
{
	// allow user to build new structures

	_INT16 iTemp, WhichBuilding;
	char *szTheOptions[14], cKey, szString[128];

	LoadStrings(1270, 14, szTheOptions);

	for (;;) {
		rputs("\n\n");
		rputs(" |0BBuild Menu\n");
		rputs(ST_LONGLINE);

		/* show stats */
		sprintf(szString, ST_MEMPIRE11, Empire->Land);
		rputs(szString);
		sprintf(szString, ST_MEMPIRE12, Empire->WorkerEnergy);
		rputs(szString);

		// show all buildings erected
		for (iTemp = 0; iTemp < NUM_BUILDINGTYPES; iTemp++) {
			if (iTemp == B_BUSINESS && Empire->OwnerType != EO_VILLAGE)
				continue;

			// sprintf(szString, " |0A(|0B%c|0A) |0C%-20s    |0F%d\n\r", iTemp + '0', BuildingType[iTemp].szName,
			sprintf(szString, ST_MEMPIRE13, iTemp + '0', BuildingType[iTemp].szName,
					Empire->Buildings[iTemp]);
			rputs(szString);
		}
		// rputs(" |0A(|0BQ|0A) |0CQuit\n |0A(|0BV|0A) |0CView Stats\n");
		rputs(ST_MEMPIRE14);
		rputs(ST_LONGLINE);

		switch (cKey = GetChoice("", ST_ENTEROPTION, szTheOptions, "Q?012345678*9V", 'Q', TRUE)) {
				// user chooses type to build here using 123456789
			case '0' :
			case '1' :
			case '2' :
			case '3' :
			case '4' :
			case '5' :
			case '6' :
			case '7' :
			case '8' :
			case '9' :
				WhichBuilding = cKey - '0';

				if (WhichBuilding == B_BUSINESS && Empire->OwnerType != EO_VILLAGE) {
					// rputs("|07Businesses can only be built by the village.\n");
					rputs(ST_MEMPIRE28);
					break;
				}

				// show building stats
				sprintf(szString, ST_MEMPIRE15, BuildingType[ WhichBuilding ].szName);
				rputs(szString);
				sprintf(szString, ST_MEMPIRE16, BuildingType[ WhichBuilding ].EnergyUsed);
				rputs(szString);
				sprintf(szString, ST_MEMPIRE17, BuildingType[ WhichBuilding ].LandUsed);
				rputs(szString);
				sprintf(szString, ST_MEMPIRE18, BuildingType[ WhichBuilding ].Cost);
				rputs(szString);

				Help(BuildingType[ WhichBuilding ].szName, ST_WARHLP);

				// see if can't build this
				if (BuildingType[ WhichBuilding ].LandUsed > Empire->Land) {
					// od_printf("You need more worker land.\n\r");
					rputs(ST_MEMPIRE19);
					break;
				}
				// see if can't build this
				else if (BuildingType[ WhichBuilding ].Cost > Empire->VaultGold) {
					// od_printf("You need more gold.\n\r");
					rputs(ST_MEMPIRE34);
					break;
				}
				else if (BuildingType[ WhichBuilding ].EnergyUsed > Empire->WorkerEnergy) {
					// od_printf("You need more worker energy.\n\r");
					rputs(ST_MEMPIRE20);
					break;
				}

				// show stats
				sprintf(szString, "|0CYou have |0B%ld |0Cgold, |0B%d |0Cland, |0B%d%% |0Cworker Energy\n",
						Empire->VaultGold, Empire->Land, Empire->WorkerEnergy);
				rputs(szString);

				// if (YesNo("Build this?") == NO) break;
				if (YesNo(ST_MEMPIRE21) == NO) break;

				// "build" it now
				Empire->Land -= BuildingType[ WhichBuilding ].LandUsed;
				Empire->VaultGold -= BuildingType[ WhichBuilding ].Cost;
				Empire->WorkerEnergy -= BuildingType[ WhichBuilding ].EnergyUsed;
				Empire->Buildings[WhichBuilding]++;

				// sprintf(szString, "%s built!\n\r", BuildingType[ WhichBuilding ].szName);
				sprintf(szString, ST_MEMPIRE22, BuildingType[ WhichBuilding ].szName);
				rputs(szString);
				break;
			case '*' :  // destroy buildings
				Destroy_Menu(Empire);
				break;
			case 'Q' :      /* return to previous menu */
				return;
			case 'V' :      // clan stats
				ClanStats(PClan, TRUE);
				break;
			case '?' :      /* redisplay options */
				break;
		}
	}
}


void ManageArmy(struct empire *Empire)
{
	char *szTheOptions[6];
	char szString[128];
	long LimitingVariable, NumToTrain;

	LoadStrings(360, 6, szTheOptions);

	/* get a choice */
	for (;;) {
		rputs("\n");
		rputs(ST_MEMPIRE23);
		rputs(ST_LONGLINE);

		// show army stats
		sprintf(szString, ST_ESTATS15, ArmySpeed(&Empire->Army));
		rputs(szString);
		sprintf(szString, ST_ESTATS16, ArmyOffense(&Empire->Army));
		rputs(szString);
		sprintf(szString, ST_ESTATS17, ArmyDefense(&Empire->Army));
		rputs(szString);
		sprintf(szString, ST_ESTATS18, ArmyVitality(&Empire->Army));
		rputs(szString);

		rputs(ST_MTROOPHEADER);

		sprintf(szString, ST_MTROOPCOST1, Empire->Army.Footmen);
		rputs(szString);

		if (Empire->Buildings[B_STEELMILL]) {
			sprintf(szString, ST_MTROOPCOST2, Empire->Army.Axemen);
			rputs(szString);
		}

		if (Empire->Buildings[B_STABLES] && Empire->Buildings[B_STEELMILL]) {
			sprintf(szString, ST_MTROOPCOST3, Empire->Army.Knights);
			rputs(szString);
		}

		rputs(ST_MMENURETURN);

		/* give status of troops in one line */
		sprintf(szString, ST_MMENUTROOPSTATUS,
				Empire->Army.Followers, Empire->VaultGold);
		rputs(szString);

		rputs(ST_LONGLINE);

		switch (GetChoice("", ST_ENTEROPTION, szTheOptions, "123?QH", 'Q', TRUE)) {
			case 'H' :  // general help
				GeneralHelp(ST_ARMYHLP);
				break;
			case '1' :      /* footmen */
				Help("Footmen", ST_ARMYHLP);

				// first limit is number of barracks you own
				LimitingVariable = Empire->Buildings[B_BARRACKS]*20;

				// next, subtract from this the number of troops you own
				LimitingVariable -= Empire->Army.Footmen;

				// this leaves how many you can train

				// if you have less gold than this, you can only train that
				// many
				if (Empire->VaultGold / SCOST_FOOTMAN < LimitingVariable)
					LimitingVariable = Empire->VaultGold / SCOST_FOOTMAN;

				if (Empire->Army.Followers < LimitingVariable)
					LimitingVariable = Empire->Army.Followers;

				// if less than 0, make it 0
				if (LimitingVariable < 0)
					LimitingVariable = 0;

				NumToTrain = GetLong(ST_MMENUTRAIN1, 0, LimitingVariable);

				if (NumToTrain) {
					/* tell him he trained em */
					sprintf(szString, ST_MMENUTRAIN2, NumToTrain, NumToTrain*SCOST_FOOTMAN);
					rputs(szString);

					Empire->VaultGold -= (NumToTrain*SCOST_FOOTMAN);

					Empire->Army.Footmen += NumToTrain;
					Empire->Army.Followers -= NumToTrain;
				}
				break;
			case '2' :      /* axemen */
				if (Empire->Buildings[B_STEELMILL] == 0) {
					rputs(ST_MEMPIRE25);
					break;
				}
				Help("Axemen", ST_ARMYHLP);

				// first limit is number of barracks you own
				LimitingVariable = Empire->Buildings[B_BARRACKS]*10;

				// next, subtract from this the number of troops you own
				LimitingVariable -= Empire->Army.Axemen;

				// this leaves how many you can train

				// if you have less gold than this, you can only train that
				// many
				if (Empire->VaultGold / SCOST_AXEMEN < LimitingVariable)
					LimitingVariable = Empire->VaultGold / SCOST_AXEMEN;

				if (Empire->Army.Followers < LimitingVariable)
					LimitingVariable = Empire->Army.Followers;

				// if less than 0, make it 0
				if (LimitingVariable < 0)
					LimitingVariable = 0;

				NumToTrain = GetLong(ST_MMENUTRAIN3, 0, LimitingVariable);

				if (NumToTrain) {
					sprintf(szString, ST_MMENUTRAIN4, NumToTrain, NumToTrain*SCOST_AXEMEN);
					rputs(szString);

					Empire->VaultGold -= (NumToTrain*SCOST_AXEMEN);

					Empire->Army.Axemen += NumToTrain;
					Empire->Army.Followers -= NumToTrain;
				}
				break;
			case '3' :      /* Knights */
				if (Empire->Buildings[B_STEELMILL] == 0 ||
						Empire->Buildings[B_STABLES] == 0) {
					rputs(ST_MEMPIRE26);
					break;
				}
				Help("Knights", ST_ARMYHLP);

				// first limit is number of barracks you own
				LimitingVariable = Empire->Buildings[B_BARRACKS]*5;

				// next, subtract from this the number of troops you own
				LimitingVariable -= Empire->Army.Knights;

				// this leaves how many you can train

				// if you have less gold than this, you can only train that
				// many
				if (Empire->VaultGold / SCOST_KNIGHT < LimitingVariable)
					LimitingVariable = Empire->VaultGold / SCOST_KNIGHT;

				if (Empire->Army.Followers < LimitingVariable)
					LimitingVariable = Empire->Army.Followers;

				// if less than 0, make it 0
				if (LimitingVariable < 0)
					LimitingVariable = 0;

				NumToTrain = GetLong(ST_MMENUTRAIN5, 0, LimitingVariable);

				if (NumToTrain) {
					sprintf(szString, ST_MMENUTRAIN6, NumToTrain, NumToTrain*SCOST_KNIGHT);
					rputs(szString);

					Empire->VaultGold -= (NumToTrain*SCOST_KNIGHT);

					Empire->Army.Knights += NumToTrain;
					Empire->Army.Followers -= NumToTrain;
				}
				break;
			case 'Q' :      /* return to previous menu */
				return;
			case '?' :      /* redisplay options */
				break;
		}
	}
}

// ------------------------------------------------------------------------- //
void ArmyAttack(struct Army *Attacker, struct Army *Defender,
				struct AttackResult *Result)
{
	_INT16 NumRounds, CurRound;
	struct Army OrigAttacker, OrigDefender;
	_INT16 AttackerSpeed, DefenderSpeed;
	long AttackerOffense, DefenderOffense;
	long AttackerDefense, DefenderDefense;
	long AttackerVitality, DefenderVitality;
	long Damage, Percent;
	_INT16 Intensity, AttackerLoss, DefenderLoss;

	// Initialize result now
	Result->Success = FALSE;
	Result->NoTarget = FALSE;
	Result->PercentDamage = 0;

	NumRounds = (Attacker->Strategy.AttackLength +
				 Defender->Strategy.DefendLength) / 2;

	Intensity = (Attacker->Strategy.AttackIntensity + Defender->Strategy.DefendIntensity)/2;
	//    od_printf("Intensity of battle is %d\n", Intensity);

	// save original values
	OrigAttacker = *Attacker;
	OrigDefender = *Defender;

	// figure out speeds
	AttackerSpeed = ArmySpeed(Attacker);
	DefenderSpeed = ArmySpeed(Defender);

	// figure out offense
	AttackerOffense = (ArmyOffense(Attacker)*AttackerSpeed*Attacker->Rating)/100;
	DefenderOffense = (ArmyOffense(Defender)*DefenderSpeed*Defender->Rating)/100;

	// figure out defense
	AttackerDefense = (ArmyDefense(Attacker)*AttackerSpeed*Attacker->Rating)/100;
	DefenderDefense = (ArmyDefense(Defender)*DefenderSpeed*Defender->Rating)/100;

	// figure out vitality
	AttackerVitality = ArmyVitality(Attacker);
	DefenderVitality = ArmyVitality(Defender);

	/*
	od_printf("Attacker:\n %ld Offense\n %ld Defense\n %d speed \n %ld Vitality\n",
	  AttackerOffense, AttackerDefense, AttackerSpeed, AttackerVitality);
	od_printf("Defender:\n %ld Offense\n %ld Defense\n %d speed \n %ld Vitality\n",
	  DefenderOffense, DefenderDefense, DefenderSpeed, DefenderVitality);
	*/

	// if attacker has no energy to begin with, defender wins battle
	if (AttackerVitality == 0) {
		//DisplayStr("You have no energy\n");
		Result->PercentDamage = 0;
		return;
	}


	// if no energy to begin with, attacker wins battle
	if (DefenderVitality == 0) {
		//DisplayStr("Enemy has no energy\n");
		Result->PercentDamage = 100;
		Result->Success = TRUE;
		return;
	}

	// go through rounds
	for (CurRound = 0; CurRound < NumRounds; CurRound++) {
		// figure out damage to defender
		Damage = ((AttackerOffense - DefenderDefense)*Intensity) / (DAMAGE_FACTOR*100);
		if (Damage < 0)
			Damage = 0;

		DefenderVitality -= Damage;
		if (DefenderVitality < 0)
			DefenderVitality = 0;

		// figure out damage to attacker
		Damage = ((DefenderOffense - AttackerDefense)*Intensity) / (DAMAGE_FACTOR*100);
		if (Damage < 0)
			Damage = 0;

		AttackerVitality -= Damage;
		if (AttackerVitality < 0)
			AttackerVitality = 0;


		// figure out # of troops left after attack
		Percent = (AttackerVitality*100)/ArmyVitality(&OrigAttacker);
		Attacker->Footmen = (Percent*OrigAttacker.Footmen)/100;
		Attacker->Axemen = (Percent*OrigAttacker.Axemen)/100;
		Attacker->Knights = (Percent*OrigAttacker.Knights)/100;

		Percent = (DefenderVitality*100)/ArmyVitality(&OrigDefender);
		Defender->Footmen = (Percent*OrigDefender.Footmen)/100;
		Defender->Axemen = (Percent*OrigDefender.Axemen)/100;
		Defender->Knights = (Percent*OrigDefender.Knights)/100;

		// *** Calculate stats for next round

		// figure out speeds
		AttackerSpeed = ArmySpeed(Attacker);
		DefenderSpeed = ArmySpeed(Defender);

		// figure out offense
		AttackerOffense = (ArmyOffense(Attacker)*AttackerSpeed*Attacker->Rating)/100;
		DefenderOffense = (ArmyOffense(Defender)*DefenderSpeed*Defender->Rating)/100;

		// figure out defense
		AttackerDefense = (ArmyDefense(Attacker)*AttackerSpeed*Attacker->Rating)/100;
		DefenderDefense = (ArmyDefense(Defender)*DefenderSpeed*Defender->Rating)/100;

		// figure out vitality
		AttackerVitality = ArmyVitality(Attacker);
		DefenderVitality = ArmyVitality(Defender);

		// if either side has no energy, break
		if (DefenderVitality == 0 || AttackerVitality == 0) {
			break;
		}
	}

	// figure out percent damage done to each side
	AttackerLoss = 100 - (ArmyVitality(Attacker)*100)/ArmyVitality(&OrigAttacker);
	DefenderLoss = 100 - (ArmyVitality(Defender)*100)/ArmyVitality(&OrigDefender);

	//    od_printf("Attacker's loss = %d\n\rDefender's Loss = %d\n\r", AttackerLoss, DefenderLoss);

	Result->PercentDamage = DefenderLoss;

	Result->AttackCasualties.Footmen += (OrigAttacker.Footmen - Attacker->Footmen);
	Result->AttackCasualties.Axemen += (OrigAttacker.Axemen - Attacker->Axemen);
	Result->AttackCasualties.Knights += (OrigAttacker.Knights - Attacker->Knights);

	Result->DefendCasualties.Footmen += (OrigDefender.Footmen - Defender->Footmen);
	Result->DefendCasualties.Axemen += (OrigDefender.Axemen - Defender->Axemen);
	Result->DefendCasualties.Knights += (OrigDefender.Knights - Defender->Knights);

	// if attacker's loss < defender's loss and defender's loss >= 50%,
	// attacker won!
	if (AttackerLoss < DefenderLoss && DefenderLoss >= 30)
		Result->Success = TRUE;
	else
		Result->Success = FALSE;
}

// ------------------------------------------------------------------------- //

void EmpireAttack(struct empire *AttackingEmpire, struct Army *AttackingArmy,
				  struct empire *DefendingEmpire, struct AttackResult *Result,
				  _INT16 Goal, _INT16 ExtentOfAttack)
{
	struct Army DefendingArmy, ArmyKilledByTowers, ArmyStoppedByWalls;
	_INT16 iTemp;

	// ExtentOfAttack is used for stealing land, gold, and destroying
	// it is the percentage of the attack (10-30% usually)

	// intialize result
	Result->Success = FALSE;
	Result->NoTarget= FALSE;

	Result->ReturningArmy = *AttackingArmy;
	Result->AttackerType = AttackingEmpire->OwnerType;
	Result->AttackCasualties.Footmen = 0;
	Result->AttackCasualties.Axemen  = 0;
	Result->AttackCasualties.Knights = 0;
	Result->DefendCasualties.Footmen = 0;
	Result->DefendCasualties.Axemen  = 0;
	Result->DefendCasualties.Knights = 0;
	strcpy(Result->szAttackerName, AttackingEmpire->szName);

	for (iTemp = 0; iTemp < MAX_BUILDINGS; iTemp++)
		Result->BuildingsDestroyed[iTemp] = 0;

	Result->Goal = Goal;
	Result->ExtentOfAttack = ExtentOfAttack;
	Result->GoldStolen = 0;
	Result->LandStolen = 0;

	DefendingArmy.Strategy = DefendingEmpire->Army.Strategy;
	DefendingArmy.Rating = DefendingEmpire->Army.Rating;

	// amass correct number of troops
	switch (Goal) {
		case G_OUSTRULER :    // oust the ruler, 60% of troops help out
			DefendingArmy.Footmen = (DefendingEmpire->Army.Footmen*3)/5;
			DefendingArmy.Axemen = (DefendingEmpire->Army.Axemen*3)/5;
			DefendingArmy.Knights = (DefendingEmpire->Army.Knights*3)/5;
			break;
		case G_STEALLAND :    // steal land
			// use 40% of troops + ExtentOfAttack, so 50% if getting 10% of land
			DefendingArmy.Footmen =
				(DefendingEmpire->Army.Footmen*(70+ExtentOfAttack))/100;
			DefendingArmy.Axemen =
				(DefendingEmpire->Army.Axemen*(70+ExtentOfAttack))/100;
			DefendingArmy.Knights =
				(DefendingEmpire->Army.Knights*(70+ExtentOfAttack))/100;
			break;
		case G_STEALGOLD :  // steal gold

			// use 20% of troops + ExtentOfAttack (5-20% of treasury)
			DefendingArmy.Footmen =
				(DefendingEmpire->Army.Footmen*(80+ExtentOfAttack))/100;
			DefendingArmy.Axemen =
				(DefendingEmpire->Army.Axemen*(80+ExtentOfAttack))/100;
			DefendingArmy.Knights =
				(DefendingEmpire->Army.Knights*(80+ExtentOfAttack))/100;
			break;
		case G_DESTROY :  // destroy, get 80% + extent of damage (5-15%)
			DefendingArmy.Footmen =
				(DefendingEmpire->Army.Footmen*(80+ExtentOfAttack))/100;
			DefendingArmy.Axemen =
				(DefendingEmpire->Army.Axemen*(80+ExtentOfAttack))/100;
			DefendingArmy.Knights =
				(DefendingEmpire->Army.Knights*(80+ExtentOfAttack))/100;
			break;
	}

	// add on a few more troops so that we can wipe 'em all out if need be
	DefendingArmy.Footmen += 25;
	DefendingArmy.Axemen  += 15;
	DefendingArmy.Knights += 10;

	if (DefendingArmy.Footmen > DefendingEmpire->Army.Footmen)
		DefendingArmy.Footmen = DefendingEmpire->Army.Footmen;
	if (DefendingArmy.Axemen > DefendingEmpire->Army.Axemen)
		DefendingArmy.Axemen = DefendingEmpire->Army.Axemen;
	if (DefendingArmy.Knights > DefendingEmpire->Army.Knights)
		DefendingArmy.Knights = DefendingEmpire->Army.Knights;


	// use walls here in future to prevent some of the troops from entering
	ArmyStoppedByWalls.Footmen = 0;
	ArmyStoppedByWalls.Axemen  = 0;
	ArmyStoppedByWalls.Knights = 0;
	for (iTemp = 0; iTemp < DefendingEmpire->Buildings[B_WALL]; iTemp++) {
		ArmyStoppedByWalls.Footmen += RANDOM(2);
		ArmyStoppedByWalls.Axemen  += RANDOM(2);
	}
	if (ArmyStoppedByWalls.Footmen > AttackingArmy->Footmen)
		ArmyStoppedByWalls.Footmen = AttackingArmy->Footmen;
	if (ArmyStoppedByWalls.Axemen > AttackingArmy->Axemen)
		ArmyStoppedByWalls.Axemen = AttackingArmy->Axemen;
	if (ArmyStoppedByWalls.Knights > AttackingArmy->Knights)
		ArmyStoppedByWalls.Knights = AttackingArmy->Knights;

	Result->ReturningArmy.Footmen = ArmyStoppedByWalls.Footmen;
	Result->ReturningArmy.Axemen  = ArmyStoppedByWalls.Axemen;
	Result->ReturningArmy.Knights = ArmyStoppedByWalls.Knights;
	AttackingArmy->Footmen -= ArmyStoppedByWalls.Footmen;
	AttackingArmy->Axemen  -= ArmyStoppedByWalls.Axemen;
	AttackingArmy->Knights -= ArmyStoppedByWalls.Knights;

	// use towers here in future to attack some of the troops
	// use ArmyKilledByTowers, then add this to the Result->AttackCasualties
	// later
	// make sure you reduce AttackingArmy with ArmyKilled...
	ArmyKilledByTowers.Footmen = 0;
	ArmyKilledByTowers.Axemen  = 0;
	ArmyKilledByTowers.Knights = 0;
	for (iTemp = 0; iTemp < DefendingEmpire->Buildings[B_TOWER]; iTemp++) {
		ArmyKilledByTowers.Footmen += RANDOM(3);
		ArmyKilledByTowers.Axemen  += RANDOM(3);
		ArmyKilledByTowers.Knights += RANDOM(2);
	}
	if (ArmyKilledByTowers.Footmen > AttackingArmy->Footmen)
		ArmyKilledByTowers.Footmen = AttackingArmy->Footmen;
	if (ArmyKilledByTowers.Axemen > AttackingArmy->Axemen)
		ArmyKilledByTowers.Axemen = AttackingArmy->Axemen;
	if (ArmyKilledByTowers.Knights > AttackingArmy->Knights)
		ArmyKilledByTowers.Knights = AttackingArmy->Knights;

	Result->AttackCasualties.Footmen += ArmyKilledByTowers.Footmen;
	Result->AttackCasualties.Axemen  += ArmyKilledByTowers.Axemen;
	Result->AttackCasualties.Knights += ArmyKilledByTowers.Knights;
	AttackingArmy->Footmen -= ArmyKilledByTowers.Footmen;
	AttackingArmy->Axemen  -= ArmyKilledByTowers.Axemen;
	AttackingArmy->Knights -= ArmyKilledByTowers.Knights;

	// attack now
	ArmyAttack(AttackingArmy, &DefendingArmy, Result);

	// returning army
	Result->ReturningArmy.Footmen += AttackingArmy->Footmen;
	Result->ReturningArmy.Axemen  += AttackingArmy->Axemen;
	Result->ReturningArmy.Knights += AttackingArmy->Knights;

	/*    // don't print results
	  od_printf("Your losses: %ld footmen, %ld axemen, %ld knights\n\r",
	    Result.AttackCasualties.Footmen, Result.AttackCasualties.Axemen, Result.AttackCasualties.Knights);
	  od_printf("His losses: %ld footmen, %ld axemen, %ld knights\n\r",
	    Result.DefendCasualties.Footmen, Result.DefendCasualties.Axemen, Result.DefendCasualties.Knights);
	*/
}




// according to result, do something
void ProcessAttackResult(struct AttackResult *AttackResult)
{
	char szNews[128], *szMessage, szString[255],
	szDefenderType[40], szAttacker[40], szDefender[40];
	_INT16 WhichBBS = 0, iTemp, Junk[2];  // <<-- Junk[] is used as dummy
	_INT16 Percent, WhichAlliance, LandGained;
	struct clan *TmpClan;
	struct Alliance *Alliances[MAX_ALLIANCES];
	BOOL ShowedOne;

	// function will update defender's empire but NOT the attacker's


	if (Game.Data->InterBBS)
		for (iTemp = 0; iTemp < MAX_IBBSNODES; iTemp++) {
			if (IBBS.Data->Nodes[iTemp].Active == FALSE) continue;

			if (iTemp+1 == AttackResult->BBSIDFrom) {
				WhichBBS = iTemp+1;
				break;
			}
		}
	else
		WhichBBS = 1;

	if (AttackResult->DefenderType == EO_VILLAGE)
		strcpy(szDefenderType, "village");
	else if (AttackResult->DefenderType == EO_ALLIANCE)
		strcpy(szDefenderType, "alliance");
	else if (AttackResult->DefenderType == EO_CLAN)
		strcpy(szDefenderType, "clan");

	if (AttackResult->InterBBS) {
		if (AttackResult->AttackerType == EO_VILLAGE)
			sprintf(szAttacker, "The village of %s", AttackResult->szAttackerName);
		else if (AttackResult->AttackerType == EO_CLAN)
			sprintf(szAttacker, "The clan of %s from %s", AttackResult->szAttackerName,
					IBBS.Data->Nodes[WhichBBS-1].Info.pszVillageName);
	}
	else {
		if (AttackResult->AttackerType == EO_VILLAGE)
			strcpy(szAttacker, "The village");
		else if (AttackResult->AttackerType == EO_CLAN)
			sprintf(szAttacker, "%s", AttackResult->szAttackerName);
		else if (AttackResult->AttackerType == EO_ALLIANCE)
			sprintf(szAttacker, "The alliance of %s", AttackResult->szAttackerName);
	}
	switch (AttackResult->DefenderType) {
		case EO_VILLAGE :
			strcpy(szDefender, "our village");
			break;
		case EO_CLAN :
			GetClanNameID(szDefender, AttackResult->DefenderID);
			break;
		case EO_ALLIANCE :
			GetAlliances(Alliances);
			// figure out which one is the defender
			for (iTemp = 0; iTemp < MAX_ALLIANCES; iTemp++) {
				if (Alliances[iTemp] == NULL) break;
				if (Alliances[iTemp]->ID == AttackResult->AllianceID)
					break;
			}
			WhichAlliance = iTemp;
			sprintf(szDefender, "the alliance of %s", Alliances[iTemp]->szName);

			// free up mem used by alliances
			for (iTemp = 0; iTemp < MAX_ALLIANCES; iTemp++)
				if (Alliances[iTemp])
					free(Alliances[iTemp]);
			break;
	}

	// start off news reel
	// sprintf(szNews, ">> %s attacked %s and %s!\n   ", szAttacker, szDefender,
	sprintf(szNews, ST_WNEWS0, szAttacker, szDefender,
			AttackResult->Success ? "won" : "lost");

	// add on the type of attack
	switch (AttackResult->Goal) {
		case G_OUSTRULER :
			if (AttackResult->Success)
				// strcat(szNews, "They ousted our rulers!\n  ");
				strcat(szNews, ST_WNEWS1);
			break;
		case G_STEALLAND :
			if (AttackResult->Success && AttackResult->LandStolen)
				// strcat(szNews, "They looted land.\n  ");
				strcat(szNews, ST_WNEWS2);
			break;
		case G_STEALGOLD :
			if (AttackResult->Success && AttackResult->GoldStolen)
				// strcat(szNews, "They looted some gold.\n  ");
				strcat(szNews, ST_WNEWS3);
			break;
		case G_DESTROY :
			if (AttackResult->Success)
				// strcat(szNews, "They caused much damage.\n  ");
				strcat(szNews, ST_WNEWS4);
			break;
	}

	szMessage = MakeStr(600);

	// start message off
	// sprintf(szMessage, "%s attacked your %s's empire!\nYour army defeated the following:\n",
	sprintf(szMessage, ST_PAR0, szAttacker, szDefender);

	// append what was lost and who you killed
	//    sprintf(szString, " %ld Footmen\n %ld Axemen\n %ld Knights \n",
	sprintf(szString, ST_WAR0,
			AttackResult->AttackCasualties.Footmen,
			AttackResult->AttackCasualties.Axemen,
			AttackResult->AttackCasualties.Knights);
	strcat(szMessage, szString);
	//    sprintf(szString, "You lost the following:\n %ld Footmen\n %ld Axemen\n %ld Knights \n",
	sprintf(szString, ST_WAR1,
			AttackResult->DefendCasualties.Footmen,
			AttackResult->DefendCasualties.Axemen,
			AttackResult->DefendCasualties.Knights);
	strcat(szMessage, szString);

	// finally find out what was lost in terms of stuff
	if (AttackResult->Success) {
		switch (AttackResult->Goal) {
			case G_OUSTRULER :  // ruler ousted!
				// strcat(szMessage, " You were ousted from rule!\n");
				strcat(szMessage, ST_PAR1);

				// oust the ruler
				Village.Data->RulingClanId[0] = -1;
				Village.Data->RulingClanId[1] = -1;
				Village.Data->szRulingClan[0] = 0;
				Village.Data->GovtSystem = GS_DEMOCRACY;
				Village.Data->RulingDays = 0;
				break;
			case G_STEALLAND :  // steal land, figure out how much land stolen
				// tell him in message how much was lost
				// half as much damage as regular attack

				switch (AttackResult->DefenderType) {
					case EO_VILLAGE :
						Percent = (AttackResult->PercentDamage*AttackResult->ExtentOfAttack)/200L;
						DestroyBuildings(Village.Data->Empire.Buildings,
										 AttackResult->BuildingsDestroyed, Percent, &LandGained);
						// after destroying everything, Land is gained

						AttackResult->LandStolen = ((Village.Data->Empire.Land + LandGained)*Percent)/100L;
						break;
					case EO_CLAN :
						TmpClan = malloc(sizeof(struct clan));
						CheckMem(TmpClan);
						GetClan(AttackResult->DefenderID, TmpClan);
						Percent = (AttackResult->PercentDamage*AttackResult->ExtentOfAttack)/200L;
						DestroyBuildings(TmpClan->Empire.Buildings,
										 AttackResult->BuildingsDestroyed, Percent, &LandGained);

						AttackResult->LandStolen = ((TmpClan->Empire.Land + LandGained)*Percent)/100L;
						FreeClan(TmpClan);
						break;
					case EO_ALLIANCE :
						GetAlliances(Alliances);
						// figure out which one is the defender
						for (iTemp = 0; iTemp < MAX_ALLIANCES; iTemp++) {
							if (Alliances[iTemp] == NULL) break;
							if (Alliances[iTemp]->ID == AttackResult->AllianceID)
								break;
						}
						WhichAlliance = iTemp;

						Percent = (AttackResult->PercentDamage*AttackResult->ExtentOfAttack)/200L;
						DestroyBuildings(Alliances[WhichAlliance]->Empire.Buildings,
										 AttackResult->BuildingsDestroyed, Percent, &LandGained);

						AttackResult->LandStolen = ((Alliances[WhichAlliance]->Empire.Land + LandGained)*Percent)/100L;

						// free up mem used by alliances
						for (iTemp = 0; iTemp < MAX_ALLIANCES; iTemp++)
							if (Alliances[iTemp])
								free(Alliances[iTemp]);
						break;
				}
				ShowedOne = FALSE;
				for (iTemp = 0; iTemp < NUM_BUILDINGTYPES; iTemp++) {
					if (AttackResult->BuildingsDestroyed[iTemp] == 0)
						continue;

					if (!ShowedOne) {
						// strcat(szMessage, "The following buildings were destroyed:\n");
						strcat(szMessage, ST_PAR2);
						ShowedOne = TRUE;
					}
					sprintf(szString, ST_WRESULTS10, AttackResult->BuildingsDestroyed[iTemp],
							BuildingType[iTemp].szName);
					strcat(szMessage, szString);
				}
				// sprintf(szString, " They stole %d land!\n", AttackResult->LandStolen);
				if (AttackResult->LandStolen) {
					sprintf(szString, ST_PAR3, AttackResult->LandStolen);
					strcat(szMessage, szString);
				}
				break;
			case G_STEALGOLD :  // steal gold, do some stuff here later, for now, get 3%
				switch (AttackResult->DefenderType) {
					case EO_VILLAGE :
						AttackResult->GoldStolen =
							((Village.Data->Empire.VaultGold/100L) * ((long)AttackResult->PercentDamage * (long)AttackResult->ExtentOfAttack)/100L);
						break;
					case EO_CLAN :
						TmpClan = malloc(sizeof(struct clan));
						CheckMem(TmpClan);
						GetClan(AttackResult->DefenderID, TmpClan);
						AttackResult->GoldStolen =
							((TmpClan->Empire.VaultGold/100L) * ((long)AttackResult->PercentDamage * (long)AttackResult->ExtentOfAttack)/100L);
						FreeClan(TmpClan);
						break;
					case EO_ALLIANCE :
						GetAlliances(Alliances);
						// figure out which one is the defender
						for (iTemp = 0; iTemp < MAX_ALLIANCES; iTemp++) {
							if (Alliances[iTemp] == NULL) break;
							if (Alliances[iTemp]->ID == AttackResult->AllianceID)
								break;
						}
						WhichAlliance = iTemp;

						AttackResult->GoldStolen =
							((Alliances[WhichAlliance]->Empire.VaultGold/100L) * ((long)AttackResult->PercentDamage * (long)AttackResult->ExtentOfAttack)/100L);

						// free up mem used by alliances
						for (iTemp = 0; iTemp < MAX_ALLIANCES; iTemp++)
							if (Alliances[iTemp])
								free(Alliances[iTemp]);
						break;
				}
				// sprintf(szString, " They stole %ld gold!\n", AttackResult->GoldStolen);
				if (AttackResult->GoldStolen) {
					sprintf(szString, ST_PAR4, AttackResult->GoldStolen);
					strcat(szMessage, szString);
				}
				break;
			case G_DESTROY :  // destroy, figure out how much of his stuff was destroyed
				switch (AttackResult->DefenderType) {
					case EO_VILLAGE :
						Percent = (AttackResult->PercentDamage*AttackResult->ExtentOfAttack)/100L;
						DestroyBuildings(Village.Data->Empire.Buildings,
										 AttackResult->BuildingsDestroyed, Percent, &LandGained);
						break;
					case EO_CLAN :
						TmpClan = malloc(sizeof(struct clan));
						CheckMem(TmpClan);
						GetClan(AttackResult->DefenderID, TmpClan);
						Percent = (AttackResult->PercentDamage*AttackResult->ExtentOfAttack)/100L;
						DestroyBuildings(TmpClan->Empire.Buildings,
										 AttackResult->BuildingsDestroyed, Percent, &LandGained);
						FreeClan(TmpClan);
						break;
					case EO_ALLIANCE :
						GetAlliances(Alliances);
						// figure out which one is the defender
						for (iTemp = 0; iTemp < MAX_ALLIANCES; iTemp++) {
							if (Alliances[iTemp] == NULL) break;
							if (Alliances[iTemp]->ID == AttackResult->AllianceID)
								break;
						}
						WhichAlliance = iTemp;

						Percent = (AttackResult->PercentDamage*AttackResult->ExtentOfAttack)/100L;
						DestroyBuildings(Alliances[WhichAlliance]->Empire.Buildings,
										 AttackResult->BuildingsDestroyed, Percent, &LandGained);

						// free up mem used by alliances
						for (iTemp = 0; iTemp < MAX_ALLIANCES; iTemp++)
							if (Alliances[iTemp])
								free(Alliances[iTemp]);
						break;
				}
				// tell what they lost
				// what destroyed
				ShowedOne = FALSE;
				for (iTemp = 0; iTemp < NUM_BUILDINGTYPES; iTemp++) {
					if (AttackResult->BuildingsDestroyed[iTemp] == 0)
						continue;

					if (!ShowedOne) {
						//              strcat(szMessage, "The following buildings were destroyed:\n");
						strcat(szMessage, ST_PAR2);
						ShowedOne = TRUE;
					}
					sprintf(szString, ST_WRESULTS10, AttackResult->BuildingsDestroyed[iTemp],
							BuildingType[iTemp].szName);
					strcat(szMessage, szString);
				}
				break;
		}
	}

	GenericMessage(szMessage, AttackResult->DefenderID, Junk, AttackResult->szAttackerName, FALSE);
	News_AddNews(szNews);
	News_AddNews("\n");

	free(szMessage);
}

void DestroyBuildings(_INT16 NumBuildings[MAX_BUILDINGS],
					  _INT16 NumDestroyed[MAX_BUILDINGS], _INT16 Percent, _INT16 *LandGained)
{
	_INT16 LandUsed[MAX_BUILDINGS];
	_INT16 NumRemaining[MAX_BUILDINGS];
	char *WarZone;
	_INT16 CurChar, CurType, CurBuilding, Start, End, DidHit, TotalEnergy,
	CurHit, WhichToHit, NumFound, TypeToHit, iTemp;
	long NumHits;

	//    od_printf("\r\nBefore:  %d towers\r\n %d barracks\r\n %d walls\r\n",
	//      NumBuildings[0], NumBuildings[1], NumBuildings[2]);

	// initialize how many destroyed
	for (iTemp = 0; iTemp < MAX_BUILDINGS; iTemp++) {
		NumDestroyed[iTemp] = 0;
		NumRemaining[iTemp] = 0;
	}

	// figure out how much mem this takes
	CurChar = 0;
	for (CurType = 0; CurType < NUM_BUILDINGTYPES; CurType++)
		for (CurBuilding = 0; CurBuilding < NumBuildings[CurType]; CurBuilding++) {
			// fill up with junk
			CurChar += BuildingType[CurType].HitZones;
		}

	if (CurChar == 0) {
		// no buildings found!
		*LandGained = 0;
		return;
	}

	WarZone = malloc(CurChar);
	CheckMem(WarZone);

	TotalEnergy = CurChar;
	//    od_printf("Total energy = %d\n\r", TotalEnergy);

	// put buildings in there
	CurChar = 0;
	for (CurType = 0; CurType < NUM_BUILDINGTYPES; CurType++)
		for (CurBuilding = 0; CurBuilding < NumBuildings[CurType]; CurBuilding++) {
			// fill up with junk
			Start = CurChar;
			End = CurChar + BuildingType[CurType].HitZones;
			for (CurChar = Start; CurChar < End; CurChar++)
				WarZone[CurChar] = CurType;
		}

	// damage them using percent

	// figure out units of land each ones wastes :)
	NumHits = 0;
	for (CurType = 0; CurType < NUM_BUILDINGTYPES; CurType++) {
		LandUsed[CurType] = BuildingType[CurType].LandUsed*NumBuildings[CurType];
		NumHits += LandUsed[CurType];
	}
	NumHits = (Percent*NumHits)/100;
	//    od_printf("Num hits = %d\n\r", NumHits);

	for (CurHit = 0; CurHit < NumHits;) {
		WhichToHit = RANDOM(TotalEnergy);
		TypeToHit = WarZone[WhichToHit];

		// do hit
		if (LandUsed[TypeToHit] == 0)
			continue;
		else {
			//        od_printf("hit %d\n\r", TypeToHit);
			LandUsed[TypeToHit]--;
			CurHit++;
		}
	}

	// figure out what we have left
	for (CurType = 0; CurType < NUM_BUILDINGTYPES; CurType++) {
		//      od_printf("%d's energy: %d of %d\n\r",
		//        CurType, LandUsed[CurType],
		//        BuildingType[CurType].LandUsed*NumBuildings[CurType]);

		NumRemaining[CurType] = LandUsed[CurType] /
								BuildingType[CurType].LandUsed;

		// round up here
		iTemp = LandUsed[CurType] -
				NumRemaining[CurType]*BuildingType[CurType].LandUsed;

		if ((iTemp*100)/BuildingType[CurType].LandUsed >= 50) {
			// round up
			NumRemaining[CurType]++;
		}

		NumDestroyed[CurType] = NumBuildings[CurType] - NumRemaining[CurType];
	}

	//    od_printf("After:  %d towers\r\n %d barracks\r\n %d walls\r\n",
	//      NumRemaining[0], NumRemaining[1], NumRemaining[2]);
	//    door_pause();

	*LandGained = 0;
	for (CurType = 0; CurType < NUM_BUILDINGTYPES; CurType++)
		*LandGained += (BuildingType[CurType].LandUsed*NumDestroyed[CurType]);

	free(WarZone);
	(void)DidHit;
	(void)NumFound;
}

void ShowResults(struct AttackResult *Result)
{
	_INT16 iTemp;
	BOOL ShowedOne;
	char szString[128];

	// reduce both armies
	// sprintf(szString, "You lost %ld footmen, %ld axemen, and %ld knights\n\r",
	sprintf(szString, ST_WRESULTS0,
			Result->AttackCasualties.Footmen,
			Result->AttackCasualties.Axemen,
			Result->AttackCasualties.Knights);
	rputs(szString);
	// sprintf(szString, "You killed %ld footmen, %ld axemen, and %ld knights\n\r",
	sprintf(szString, ST_WRESULTS1,
			Result->DefendCasualties.Footmen,
			Result->DefendCasualties.Axemen,
			Result->DefendCasualties.Knights);
	rputs(szString);

	// if no one killed and no one lost, means unable to get through
	if (Result->DefendCasualties.Footmen == 0 &&
			Result->DefendCasualties.Axemen  == 0 &&
			Result->DefendCasualties.Knights == 0 &&
			Result->AttackCasualties.Footmen == 0 &&
			Result->AttackCasualties.Axemen  == 0 &&
			Result->AttackCasualties.Knights == 0 &&
			(Result->DefendCasualties.Footmen ||
			 Result->DefendCasualties.Axemen ||
			 Result->DefendCasualties.Knights)) {
		// rputs("You were unable to penetrate their walls!\n");
		rputs(ST_WRESULTS2);
	}

	// display results of battle to user
	if (Result->Success) {
		// rputs("|07You came out |11victorious!\n");
		rputs(ST_WRESULTS3);

		switch (Result->Goal) {
			case G_OUSTRULER :
				// rputs("You successfully ousted the ruler!\n\r");
				rputs(ST_WRESULTS4);
				break;
			case G_STEALGOLD :
				if (Result->GoldStolen)
					// od_printf("You stole %ld gold\n\r", Result->GoldStolen);
					sprintf(szString, ST_WRESULTS5, Result->GoldStolen);
				else
					// od_printf("You couldn't find any gold!\n\r");
					sprintf(szString, ST_WRESULTS6);
				rputs(szString);
				break;
			case G_STEALLAND :
				if (Result->LandStolen)
					// od_printf("You stole %d land\n\r", Result->LandStolen);
					sprintf(szString, ST_WRESULTS7, Result->LandStolen);
				else
					// od_printf("You couldn't steal any land!\n\r");
					sprintf(szString, ST_WRESULTS8);
				rputs(szString);

				// what destroyed
				ShowedOne = FALSE;
				for (iTemp = 0; iTemp < NUM_BUILDINGTYPES; iTemp++) {
					if (Result->BuildingsDestroyed[iTemp] == 0)
						continue;

					if (!ShowedOne) {
						// rputs("You destroyed the following buildings:\n");
						rputs(ST_WRESULTS9);
						ShowedOne = TRUE;
					}
					// sprintf(szString, "%2d %s\n\r", Result->BuildingsDestroyed[iTemp],
					sprintf(szString, ST_WRESULTS10, Result->BuildingsDestroyed[iTemp],
							BuildingType[iTemp].szName);
					rputs(szString);
				}
				if (!ShowedOne)
					rputs(" No buildings were destroyed\n");
				break;
			case G_DESTROY :
				// what destroyed
				ShowedOne = FALSE;
				for (iTemp = 0; iTemp < NUM_BUILDINGTYPES; iTemp++) {
					if (Result->BuildingsDestroyed[iTemp] == 0)
						continue;

					if (!ShowedOne) {
						// rputs("You destroyed the following buildings:\n");
						rputs(ST_WRESULTS11);
						ShowedOne = TRUE;
					}
					// sprintf(szString, "%2d %s\n\r", Result->BuildingsDestroyed[iTemp],
					sprintf(szString, ST_WRESULTS12, Result->BuildingsDestroyed[iTemp],
							BuildingType[iTemp].szName);
					rputs(szString);
				}
				if (!ShowedOne)
					rputs(" No buildings were destroyed.\n");
				break;
		}
	}
	else {
		// rputs("|12You were defeated!\n");
		rputs(ST_WRESULTS13);
	}

}


// ------------------------------------------------------------------------- //

void GetNumTroops(struct Army *OriginalArmy, struct Army *AttackingArmy)
{
	char szString[128], cInput;

	// initialize it
	*AttackingArmy = *OriginalArmy;
	AttackingArmy->Footmen = 0;
	AttackingArmy->Axemen = 0;
	AttackingArmy->Knights = 0;

	for (;;) {
		rputs(" |0BTroops To Ready for Battle\n");
		rputs(ST_LONGLINE);

		// show army stats
		sprintf(szString, ST_ESTATS15, ArmySpeed(AttackingArmy));
		rputs(szString);
		sprintf(szString, ST_ESTATS16, ArmyOffense(AttackingArmy));
		rputs(szString);
		sprintf(szString, ST_ESTATS17, ArmyDefense(AttackingArmy));
		rputs(szString);
		sprintf(szString, ST_ESTATS18, ArmyVitality(AttackingArmy));
		rputs(szString);

		rputs("     |0CSoldiers    Owned  Readied\n");

		// show troops currently going
		sprintf(szString, ST_GTROOPS0,
				OriginalArmy->Footmen, AttackingArmy->Footmen);
		rputs(szString);
		sprintf(szString, ST_GTROOPS1,
				OriginalArmy->Axemen, AttackingArmy->Axemen);
		rputs(szString);
		sprintf(szString, ST_GTROOPS2,
				OriginalArmy->Knights, AttackingArmy->Knights);
		rputs(szString);
		rputs(ST_GTROOPS3);
		rputs(ST_LONGLINE);
		rputs(" ");
		rputs(ST_ENTEROPTION);
		rputs("Done");

		cInput = od_get_answer("ABC0\r\n][");

		rputs("\b\b\b\b    \b\b\b\b|15");

		if (cInput == '0' || cInput == '\r' || cInput == '\n')
			break;

		switch (cInput) {
			case ']' :
				rputs("Send All\n\n");
				AttackingArmy->Footmen = OriginalArmy->Footmen;
				AttackingArmy->Axemen  = OriginalArmy->Axemen;
				AttackingArmy->Knights = OriginalArmy->Knights;
				break;
			case '[' :
				rputs("Send None\n\n");
				AttackingArmy->Footmen = 0;
				AttackingArmy->Axemen  = 0;
				AttackingArmy->Knights = 0;
				break;
			case 'A' :
				rputs("Footmen\n\n");
				AttackingArmy->Footmen = GetLong(ST_GTROOPS4,
												 AttackingArmy->Footmen, OriginalArmy->Footmen);
				break;
			case 'B' :
				rputs("Axemen\n\n");
				AttackingArmy->Axemen= GetLong(ST_GTROOPS5,
											   AttackingArmy->Axemen, OriginalArmy->Axemen);
				break;
			case 'C' :
				rputs("Knights\n\n");
				AttackingArmy->Knights = GetLong(ST_GTROOPS6,
												 AttackingArmy->Knights, OriginalArmy->Knights);
				break;
		}
		rputs("\n");
	}
	rputs("Done\n\n");

	// later have him choose length of attack
	AttackingArmy->Strategy = OriginalArmy->Strategy;
}

void StartEmpireWar(struct empire *Empire)
{
	struct Army AttackingArmy, DefendingArmy;
	struct Alliance *Alliances[MAX_ALLIANCES];
	struct AttackResult Result;
	struct clan *TmpClan;
	char *pszVillage = "1 A Village",
					   *pszAlliance = "2 An Alliance",
									  *pszClan = "3 A Clan",
												 *pszOustRuler = "0 Oust the Ruler of the Village",
																 *pszStealLand = "1 Capture Land",
																				 *pszStealGold = "2 Capture Gold",
																								 *pszDestroy =   "3 Destroy Buildings";
	char *pszWhoToAttack[3], *aszVillageNames[MAX_IBBSNODES],
	*aszAllianceNames[MAX_ALLIANCES], *apszGoals[4], szNews[128];
	_INT16 TypeOfDefender, NumOfTypes, iTemp, NumBBSes, BBSIndex[MAX_IBBSNODES];
	_INT16 WhichVillage, NumAlliances, WhichAlliance, NumGoals, Goal, ExtentOfAttack = 0;
	_INT16 ClanID[2], LandGained, Decrease;

	if (!PClan->WarHelp) {
		PClan->WarHelp = TRUE;
		Help("War Help", ST_WARHLP);
		rputs("\n%P");
	}

	// choose who to attack:

	if (Game.Data->ClanEmpires == FALSE) {
		pszWhoToAttack[0] = pszVillage;
		pszWhoToAttack[1] = pszAlliance;
		NumOfTypes = 2;
	}
	else {
		pszWhoToAttack[0] = pszVillage;
		pszWhoToAttack[1] = pszAlliance;
		pszWhoToAttack[2] = pszClan;
		NumOfTypes = 3;
	}

	GetStringChoice(pszWhoToAttack, NumOfTypes, ST_WEMPIRE0,
					&TypeOfDefender, TRUE, DT_LONG, TRUE);

	if (TypeOfDefender == -1) {
		rputs(ST_ABORTED);
		return;
	}

	// if a village, choose which
	if (TypeOfDefender == EO_VILLAGE) {
		// choose which village to attack
		for (iTemp = 0; iTemp < MAX_IBBSNODES; iTemp++) {
			aszVillageNames[iTemp] = NULL;
		}

		// put our village in the list first IF the empire is not a village
		if (Empire->OwnerType != EO_VILLAGE) {
			aszVillageNames[0] = Village.Data->szName;
			// get rest of the other villages and skip ours
			NumBBSes = 1;

			if (Game.Data->InterBBS)
				for (iTemp = 0; iTemp < MAX_IBBSNODES; iTemp++) {
					if (IBBS.Data->Nodes[iTemp].Active == FALSE)
						continue;

					if (iTemp+1 == IBBS.Data->BBSID)
						break;
				}
			else
				iTemp = 0;

			BBSIndex[0] = iTemp+1;
		}
		else
			NumBBSes = 0;

		if (Game.Data->InterBBS)
			for (iTemp = 0; iTemp < MAX_IBBSNODES; iTemp++) {
				if (IBBS.Data->Nodes[iTemp].Active == FALSE)
					continue;

				if (iTemp+1 == IBBS.Data->BBSID)
					continue;

				aszVillageNames[NumBBSes] = IBBS.Data->Nodes[iTemp].Info.pszVillageName;
				BBSIndex[NumBBSes] = iTemp+1;
				NumBBSes++;
			}

		// choose a village

		GetStringChoice(aszVillageNames, NumBBSes, ST_WEMPIRE1,
						&WhichVillage, TRUE, DT_WIDE, TRUE);

		if (WhichVillage == -1) {
			rputs(ST_ABORTED);
			return;
		}

		// choose number of troops
		GetNumTroops(&Empire->Army, &AttackingArmy);

		if (AttackingArmy.Footmen == 0 &&
				AttackingArmy.Axemen  == 0 &&
				AttackingArmy.Knights == 0) {
			rputs(ST_ABORTED);
			return;
		}

		// choose type of attack (what is goal?)
		// attacking village, so up to 4 goals
		NumGoals = 4;
		apszGoals[0] = pszOustRuler;
		apszGoals[1] = pszStealLand;
		apszGoals[2] = pszStealGold;
		apszGoals[3] = pszDestroy;

		// choose one, ask user if sure
		for (;;) {
			GetStringChoice(apszGoals, NumGoals, ST_WEMPIRE2,
							&Goal, TRUE, DT_LONG, TRUE);

			if (Goal == -1) {
				// choose against it, so quit
				rputs(ST_ABORTED);
				return;
			}

			// ask user and show help
			Help(apszGoals[Goal], ST_WARHLP);
			if (YesNo(ST_WEMPIRE3) == YES)
				break;
		}

		// in future, choose length of attack, etc.
		switch (Goal) {
			case G_OUSTRULER :
				ExtentOfAttack = 50;
				break;
			case G_STEALLAND :  // find out how much to steal
				// ExtentOfAttack = GetLong(ST_WAR2, 5, 10);
				ExtentOfAttack = 10;
				if (!ExtentOfAttack)
					return;
				break;
			case G_STEALGOLD :
				// ExtentOfAttack = GetLong(ST_WAR3, 8, 15);
				ExtentOfAttack = 15;
				if (!ExtentOfAttack)
					return;
				break;
			case G_DESTROY :
				// ExtentOfAttack = GetLong(ST_WAR4, 5, 15);
				ExtentOfAttack = 15;
				if (!ExtentOfAttack)
					return;
				break;
		}

		// if OUR village, proceed normally in attack
		if ((Game.Data->InterBBS && BBSIndex[WhichVillage] == IBBS.Data->BBSID)
				|| (Game.Data->InterBBS == FALSE && WhichVillage == 0)) {
			// amass village's troops corresponding to type of attack
			// do attack now, tell user result, write changes

			// rputs("Our village!\n");

			// if no ruler, tell him
			if (Village.Data->RulingClanId[0] == -1 && Goal == G_OUSTRULER) {
				// rputs("There is no ruler to oust.  The attack is aborted.\n");
				rputs(ST_WEMPIRE4);
				return;
			}

			// initialize result beforehand
			Result.InterBBS = FALSE;
			Result.DefenderType = EO_VILLAGE;
			Result.BBSIDFrom = IBBS.Data->BBSID;
			Result.BBSIDTo = IBBS.Data->BBSID;
			Result.AttackerID[0] = PClan->ClanID[0];
			Result.AttackerID[1] = PClan->ClanID[1];
			Result.DefenderID[0] = Village.Data->RulingClanId[0];
			Result.DefenderID[1] = Village.Data->RulingClanId[1];
			Result.Goal = Goal;
			Result.AllianceID = -1;

			EmpireAttack(Empire, &AttackingArmy, &Village.Data->Empire, &Result, Goal, ExtentOfAttack);

			// process result -- this writes messages, updates news
			ProcessAttackResult(&Result);
			ShowResults(&Result);

			// update attacker's army
			Empire->Army.Footmen -= Result.AttackCasualties.Footmen;
			Empire->Army.Axemen  -= Result.AttackCasualties.Axemen;
			Empire->Army.Knights -= Result.AttackCasualties.Knights;

			// update defender's army
			Village.Data->Empire.Army.Footmen -= Result.DefendCasualties.Footmen;
			Village.Data->Empire.Army.Axemen  -= Result.DefendCasualties.Axemen;
			Village.Data->Empire.Army.Knights -= Result.DefendCasualties.Knights;

			// "give" the defender land from which his buildings came from
			LandGained = 0;
			for (iTemp = 0; iTemp < NUM_BUILDINGTYPES; iTemp++) {
				LandGained += (Result.BuildingsDestroyed[iTemp]*BuildingType[iTemp].LandUsed);
			}
			Village.Data->Empire.Land += LandGained;

			// update his losses
			Village.Data->Empire.VaultGold -= Result.GoldStolen;
			Village.Data->Empire.Land -= Result.LandStolen;

			for (iTemp = 0; iTemp < MAX_BUILDINGS; iTemp++) {
				Village.Data->Empire.Buildings[iTemp]
				-= Result.BuildingsDestroyed[iTemp];
			}

			// give attacker his land
			Empire->Land += Result.LandStolen;
			Empire->VaultGold += Result.GoldStolen;
		}
		else {
			// create a packet and send it to that village
			ClanID[0] = -1; // both unused
			ClanID[1] = -1;

			IBBS_SendAttackPacket(Empire, &AttackingArmy, Goal,
								  ExtentOfAttack, EO_VILLAGE, ClanID, BBSIndex[WhichVillage]);

			// reduce his army now
			Empire->Army.Footmen -= AttackingArmy.Footmen;
			Empire->Army.Axemen  -= AttackingArmy.Axemen;
			Empire->Army.Knights -= AttackingArmy.Knights;

			if (Empire->OwnerType == EO_VILLAGE)
				Empire->AttacksToday++;
			return;
		}
	}
	else if (TypeOfDefender == EO_ALLIANCE) {
		// get alliances and choose one

		GetAlliances(Alliances);

		for (iTemp = 0; iTemp < MAX_ALLIANCES; iTemp++) {
			if (Alliances[iTemp] == NULL)
				break;
			else
				aszAllianceNames[iTemp] = Alliances[iTemp]->szName;
		}
		NumAlliances = iTemp;

		if (NumAlliances == 0) {
			// rputs("No alliances found!\n");
			rputs(ST_WEMPIRE5);

			// free up mem used by alliances
			for (iTemp = 0; iTemp < MAX_ALLIANCES; iTemp++)
				if (Alliances[iTemp])
					free(Alliances[iTemp]);

			return;
		}

		GetStringChoice(aszAllianceNames, NumAlliances, ST_WEMPIRE6,
						&WhichAlliance, TRUE, DT_WIDE, TRUE);

		if (WhichAlliance == -1) {
			rputs(ST_ABORTED);

			// free up mem used by alliances
			for (iTemp = 0; iTemp < MAX_ALLIANCES; iTemp++)
				if (Alliances[iTemp])
					free(Alliances[iTemp]);

			return;
		}

		if (stricmp(Empire->szName, Alliances[WhichAlliance]->szName) == 0) {
			rputs("You cannot attack your own alliance!\n%P");

			// free up mem used by alliances
			for (iTemp = 0; iTemp < MAX_ALLIANCES; iTemp++)
				if (Alliances[iTemp])
					free(Alliances[iTemp]);

			return;
		}

		// choose number of troops
		GetNumTroops(&Empire->Army, &AttackingArmy);

		if (AttackingArmy.Footmen == 0 &&
				AttackingArmy.Axemen  == 0 &&
				AttackingArmy.Knights == 0) {
			rputs(ST_ABORTED);

			// free up mem used by alliances
			for (iTemp = 0; iTemp < MAX_ALLIANCES; iTemp++)
				if (Alliances[iTemp])
					free(Alliances[iTemp]);

			return;
		}

		// choose type of attack (what is goal?)
		NumGoals = 3;
		apszGoals[0] = pszStealLand;
		apszGoals[1] = pszStealGold;
		apszGoals[2] = pszDestroy;

		// choose one, ask user if sure
		for (;;) {
			GetStringChoice(apszGoals, NumGoals, ST_WEMPIRE2,
							&Goal, TRUE, DT_LONG, TRUE);

			if (Goal == -1) {
				// choose against it, so quit
				rputs(ST_ABORTED);

				// free up mem used by alliances
				for (iTemp = 0; iTemp < MAX_ALLIANCES; iTemp++)
					if (Alliances[iTemp])
						free(Alliances[iTemp]);

				return;
			}

			// ask user and show help
			Help(apszGoals[Goal], ST_WARHLP);
			if (YesNo(ST_WEMPIRE3) == YES)
				break;
		}
		Goal++;     // must add 1 since 0 is not used

		// in future, choose length of attack, etc.
		switch (Goal) {
			case G_STEALLAND :  // find out how much to steal
				ExtentOfAttack = 10;
				// ExtentOfAttack = GetLong(ST_WAR2, 5, 10);
				if (!ExtentOfAttack) {
					// free up mem used by alliances
					for (iTemp = 0; iTemp < MAX_ALLIANCES; iTemp++)
						if (Alliances[iTemp])
							free(Alliances[iTemp]);
					return;
				}
				break;
			case G_STEALGOLD :
				// ExtentOfAttack = GetLong(ST_WAR3, 8, 15);
				ExtentOfAttack = 15;
				if (!ExtentOfAttack) {
					// free up mem used by alliances
					for (iTemp = 0; iTemp < MAX_ALLIANCES; iTemp++)
						if (Alliances[iTemp])
							free(Alliances[iTemp]);
					return;
				}
				break;
			case G_DESTROY :
				// ExtentOfAttack = GetLong(ST_WAR4, 5, 15);
				ExtentOfAttack = 15;
				if (!ExtentOfAttack) {
					// free up mem used by alliances
					for (iTemp = 0; iTemp < MAX_ALLIANCES; iTemp++)
						if (Alliances[iTemp])
							free(Alliances[iTemp]);
					return;
				}
				break;
		}

		// initialize result beforehand
		Result.InterBBS = FALSE;
		Result.DefenderType = EO_ALLIANCE;
		Result.BBSIDFrom = Config->BBSID;
		Result.BBSIDTo = Config->BBSID;
		Result.AttackerID[0] = PClan->ClanID[0];
		Result.AttackerID[1] = PClan->ClanID[1];
		Result.DefenderID[0] = Alliances[WhichAlliance]->CreatorID[0];
		Result.DefenderID[1] = Alliances[WhichAlliance]->CreatorID[1];
		Result.Goal = Goal;
		Result.AllianceID = Alliances[WhichAlliance]->ID;

		// defenderID unused for villages
		EmpireAttack(Empire, &AttackingArmy, &Alliances[WhichAlliance]->Empire,
					 &Result, Goal, ExtentOfAttack);

		// process result (calculates total loss of land, etc.)
		ProcessAttackResult(&Result);

		// display results of battle to user
		ShowResults(&Result);

		// "give" the defender land from which his buildings came from
		LandGained = 0;
		for (iTemp = 0; iTemp < NUM_BUILDINGTYPES; iTemp++) {
			LandGained += (Result.BuildingsDestroyed[iTemp]*BuildingType[iTemp].LandUsed);
		}
		Alliances[WhichAlliance]->Empire.Land += LandGained;

		// update attacker's army
		Empire->Army.Footmen -= Result.AttackCasualties.Footmen;
		Empire->Army.Axemen  -= Result.AttackCasualties.Axemen;
		Empire->Army.Knights -= Result.AttackCasualties.Knights;

		// update defender's attack
		Alliances[WhichAlliance]->Empire.Army.Footmen -= Result.DefendCasualties.Footmen;
		Alliances[WhichAlliance]->Empire.Army.Axemen  -= Result.DefendCasualties.Axemen;
		Alliances[WhichAlliance]->Empire.Army.Knights -= Result.DefendCasualties.Knights;

		// update his losses
		Alliances[WhichAlliance]->Empire.VaultGold -= Result.GoldStolen;
		Alliances[WhichAlliance]->Empire.Land -= Result.LandStolen;

		for (iTemp = 0; iTemp < MAX_BUILDINGS; iTemp++) {
			Alliances[WhichAlliance]->Empire.Buildings[iTemp]
			-= Result.BuildingsDestroyed[iTemp];
		}

		// give attacker his land
		Empire->Land += Result.LandStolen;
		Empire->VaultGold += Result.GoldStolen;

		// update info to file
		UpdateAlliances(Alliances);

		// free up mem used by alliances
		for (iTemp = 0; iTemp < MAX_ALLIANCES; iTemp++)
			if (Alliances[iTemp])
				free(Alliances[iTemp]);

	}
	else if (TypeOfDefender == EO_CLAN) {
		// choose clan using GetClanID()

		// attack...

		if (GetClanID(ClanID, FALSE, FALSE, -1, -1) == FALSE) {
			return;
		}

		TmpClan = malloc(sizeof(struct clan));
		CheckMem(TmpClan);
		GetClan(ClanID, TmpClan);

		// if still in protection, tell user

		if (TmpClan->Protection) {
			rputs("|07That clan empire is still in protection and cannot be attacked.\n%P");
			FreeClan(TmpClan);
			return;
		}

		// choose number of troops
		GetNumTroops(&Empire->Army, &AttackingArmy);

		if (AttackingArmy.Footmen == 0 &&
				AttackingArmy.Axemen  == 0 &&
				AttackingArmy.Knights == 0) {
			rputs(ST_ABORTED);
			FreeClan(TmpClan);
			return;
		}

		// choose type of attack (what is goal?)
		NumGoals = 3;
		apszGoals[0] = pszStealLand;
		apszGoals[1] = pszStealGold;
		apszGoals[2] = pszDestroy;

		// choose one, ask user if sure
		for (;;) {
			GetStringChoice(apszGoals, NumGoals, ST_WEMPIRE2,
							&Goal, TRUE, DT_LONG, TRUE);

			if (Goal == -1) {
				// choose against it, so quit
				rputs(ST_ABORTED);
				FreeClan(TmpClan);
				return;
			}

			// ask user and show help
			Help(apszGoals[Goal], ST_WARHLP);
			if (YesNo(ST_WEMPIRE3) == YES)
				break;
		}
		Goal++;     // must add 1 since 0 is not used

		// choose length of attack, etc.
		switch (Goal) {
			case G_STEALLAND :  // find out how much to steal
				// ExtentOfAttack = GetLong(ST_WAR2, 5, 10);
				ExtentOfAttack = 10;
				if (!ExtentOfAttack) {
					FreeClan(TmpClan);
					return;
				}
				break;
			case G_STEALGOLD :
				// ExtentOfAttack = GetLong(ST_WAR3, 8, 15);
				ExtentOfAttack = 15;
				if (!ExtentOfAttack) {
					FreeClan(TmpClan);
					return;
				}
				break;
			case G_DESTROY :
				// ExtentOfAttack = GetLong(ST_WAR4, 5, 15);
				ExtentOfAttack = 15;
				if (!ExtentOfAttack) {
					FreeClan(TmpClan);
					return;
				}
				break;
		}

		// initialize result beforehand
		Result.InterBBS = FALSE;
		Result.DefenderType = EO_CLAN;
		Result.BBSIDFrom = Config->BBSID;
		Result.BBSIDTo = Config->BBSID;
		Result.AttackerID[0] = PClan->ClanID[0];
		Result.AttackerID[1] = PClan->ClanID[1];
		Result.DefenderID[0] = ClanID[0];
		Result.DefenderID[1] = ClanID[1];
		Result.AllianceID = Empire->AllianceID;
		Result.Goal = Goal;
		Result.AllianceID = -1;

		// defenderID unused for villages
		EmpireAttack(Empire, &AttackingArmy, &TmpClan->Empire,
					 &Result, Goal, ExtentOfAttack);

		// process result (calculates total loss of land, etc.)
		ProcessAttackResult(&Result);

		ShowResults(&Result);

		// "give" the defender land from which his buildings came from
		LandGained = 0;
		for (iTemp = 0; iTemp < NUM_BUILDINGTYPES; iTemp++) {
			LandGained += (Result.BuildingsDestroyed[iTemp]*BuildingType[iTemp].LandUsed);
		}
		TmpClan->Empire.Land += LandGained;

		// update attacker's army
		Empire->Army.Footmen -= Result.AttackCasualties.Footmen;
		Empire->Army.Axemen  -= Result.AttackCasualties.Axemen;
		Empire->Army.Knights -= Result.AttackCasualties.Knights;

		// update defender's attack
		TmpClan->Empire.Army.Footmen -= Result.DefendCasualties.Footmen;
		TmpClan->Empire.Army.Axemen  -= Result.DefendCasualties.Axemen;
		TmpClan->Empire.Army.Knights -= Result.DefendCasualties.Knights;

		// update his losses
		TmpClan->Empire.VaultGold -= Result.GoldStolen;
		TmpClan->Empire.Land -= Result.LandStolen;

		for (iTemp = 0; iTemp < MAX_BUILDINGS; iTemp++) {
			TmpClan->Empire.Buildings[iTemp] -= Result.BuildingsDestroyed[iTemp];
		}

		// give attacker his land
		Empire->Land += Result.LandStolen;
		Empire->VaultGold += Result.GoldStolen;

		// update info to file
		Clan_Update(TmpClan);
		FreeClan(TmpClan);

	}

	// reduce attacks
	if (Empire->OwnerType == EO_VILLAGE)
		Empire->AttacksToday++;
	else
		PClan->Empire.AttacksToday++;

	// update attacker's army rating
	Decrease = ExtentOfAttack/5 + 2;
	Empire->Army.Rating -= Decrease;
	if (Empire->Army.Rating < 0)
		Empire->Army.Rating = 0;

	// ** choose length of attack, etc.

	// -> if interbbs attack, generate packet destined for bbs
	// update this empire's stats (reduce army, etc.)
	// that bbs will run the empireattack function and send back the
	// results
	// when results return, update the empire...


	// give points for win, take away some for loss
	if (Empire->OwnerType == EO_CLAN) {
		if (Result.Success)
			PClan->Points += 50;
		else
			PClan->Points -= 25;
	}

	door_pause();
	(void)DefendingArmy;
	(void)szNews;
}


// ------------------------------------------------------------------------- //
void SpyMenu(struct empire *Empire)
{
	struct Alliance *Alliances[MAX_ALLIANCES];
	struct clan *TmpClan;
	char *pszVillage = "1 A Village",
					   *pszAlliance = "2 An Alliance",
									  *pszClan = "3 A Clan",
												 *aszVillageNames[MAX_IBBSNODES],
												 *pszWhoToSpy[3],
												 *aszAllianceNames[MAX_ALLIANCES], szSpierName[40], szMessage[128];
	_INT16 NumOfTypes, iTemp, NumBBSes, BBSIndex[MAX_IBBSNODES];
	_INT16 WhichVillage, NumAlliances, WhichAlliance, TypeToSpyOn;
	char szString[255];
	_INT16 ClanID[2], Junk[2];

	if (!PClan->SpyHelp) {
		PClan->SpyHelp = TRUE;
		Help("Spy Help", ST_EMPIREHLP);
		rputs("\n%P");
	}

	if (Empire->SpiesToday == MAX_SPIES) {
		// rputs("You have spied too much already today.\n%P");
		rputs(ST_SPY0);
		return;
	}

	if (Empire->OwnerType == EO_VILLAGE)
		strcpy(szSpierName, "the village");
	else if (Empire->OwnerType == EO_CLAN)
		sprintf(szSpierName, "%s", Empire->szName);
	else if (Empire->OwnerType == EO_ALLIANCE)
		sprintf(szSpierName, "the alliance of %s", Empire->szName);

	// choose who to spy on:
	if (Game.Data->ClanEmpires == FALSE) {
		pszWhoToSpy[0] = pszVillage;
		pszWhoToSpy[1] = pszAlliance;
		NumOfTypes = 2;
	}
	else {
		pszWhoToSpy[0] = pszVillage;
		pszWhoToSpy[1] = pszAlliance;
		pszWhoToSpy[2] = pszClan;
		NumOfTypes = 3;
	}

	GetStringChoice(pszWhoToSpy, NumOfTypes, ST_WEMPIRE0,
					&TypeToSpyOn, TRUE, DT_LONG, TRUE);

	if (TypeToSpyOn == -1) {
		rputs(ST_ABORTED);
		return;
	}

	// if a village, choose which
	if (TypeToSpyOn == EO_VILLAGE) {
		// choose which village to attack
		for (iTemp = 0; iTemp < MAX_IBBSNODES; iTemp++) {
			aszVillageNames[iTemp] = NULL;
		}

		// put our village in the list first IF the empire is not a village
		if (Empire->OwnerType != EO_VILLAGE) {
			aszVillageNames[0] = Village.Data->szName;
			// get rest of the other villages and skip ours
			NumBBSes = 1;

			if (Game.Data->InterBBS)
				for (iTemp = 0; iTemp < MAX_IBBSNODES; iTemp++) {
					if (IBBS.Data->Nodes[iTemp].Active == FALSE)
						continue;

					if (iTemp+1 == IBBS.Data->BBSID)
						break;
				}
			else
				iTemp = 0;

			BBSIndex[0] = iTemp+1;
		}
		else
			NumBBSes = 0;

		if (Game.Data->InterBBS)
			for (iTemp = 0; iTemp < MAX_IBBSNODES; iTemp++) {
				if (IBBS.Data->Nodes[iTemp].Active == FALSE)
					continue;

				if (iTemp+1 == IBBS.Data->BBSID)
					continue;

				aszVillageNames[NumBBSes] = IBBS.Data->Nodes[iTemp].Info.pszVillageName;
				BBSIndex[NumBBSes] = iTemp+1;
				NumBBSes++;
			}

		// choose a village

		GetStringChoice(aszVillageNames, NumBBSes, ST_WEMPIRE1,
						&WhichVillage, TRUE, DT_WIDE, TRUE);

		if (WhichVillage == -1) {
			rputs(ST_ABORTED);
			return;
		}
		// sprintf(szString, "It will cost you %ld gold to spy.  The empire has %ld gold.\nContinue?",
		sprintf(szString, ST_SPY1, SPY_COST, Empire->VaultGold);
		if (YesNo(szString) == NO) {
			return;
		}
		if (Empire->VaultGold < SPY_COST) {
			rputs(ST_FMENUNOAFFORD);
			return;
		}
		else
			Empire->VaultGold -= SPY_COST;

		if ((Game.Data->InterBBS && BBSIndex[WhichVillage] == IBBS.Data->BBSID)
				|| (Game.Data->InterBBS == FALSE && WhichVillage == 0)) {
			// see if we can spy, if so, spy on 'em now using EmpireStats
			// increment spies per day in future
			if ((Empire->Buildings[B_AGENCY]+RANDOM(5)) >
					(Village.Data->Empire.Buildings[B_SECURITY]+RANDOM(3))) {
				// success!
				// rputs("Your spy is successful!\n");
				rputs(ST_SPY2);
				Empire_Stats(&Village.Data->Empire);
			}
			else {
				// rputs("Your spy failed and was captured!\n");
				rputs(ST_SPY3);

				if (Village.Data->RulingClanId[0] != -1) {
					// sprintf(szMessage, " You caught a spy attempting to gain info on the village's empire.\n The spy was from %s.\n",
					sprintf(szMessage, ST_SPY4,
							szSpierName);
					GenericMessage(szMessage, Village.Data->RulingClanId, Junk, "", FALSE);
				}
			}
		}
		else {
			// send an IBBS spy packet
			IBBS_SendSpy(Empire, BBSIndex[WhichVillage]);
		}
	}
	else if (TypeToSpyOn == EO_ALLIANCE) {
		// get alliances and choose one

		GetAlliances(Alliances);

		for (iTemp = 0; iTemp < MAX_ALLIANCES; iTemp++) {
			if (Alliances[iTemp] == NULL)
				break;
			else
				aszAllianceNames[iTemp] = Alliances[iTemp]->szName;
		}
		NumAlliances = iTemp;

		if (NumAlliances == 0) {
			rputs("No alliances found!\n");

			// free up mem used by alliances
			for (iTemp = 0; iTemp < MAX_ALLIANCES; iTemp++)
				if (Alliances[iTemp])
					free(Alliances[iTemp]);

			return;
		}

		GetStringChoice(aszAllianceNames, NumAlliances, ST_WEMPIRE6,
						&WhichAlliance, TRUE, DT_WIDE, TRUE);

		if (WhichAlliance == -1) {
			rputs(ST_ABORTED);

			// free up mem used by alliances
			for (iTemp = 0; iTemp < MAX_ALLIANCES; iTemp++)
				if (Alliances[iTemp])
					free(Alliances[iTemp]);

			return;
		}

		// sprintf(szString, "It will cost you %ld gold to spy.  The empire has %ld gold.\nContinue?",
		sprintf(szString, ST_SPY1, SPY_COST, Empire->VaultGold);
		if (YesNo(szString) == NO) {
			// free up mem used by alliances
			for (iTemp = 0; iTemp < MAX_ALLIANCES; iTemp++)
				if (Alliances[iTemp])
					free(Alliances[iTemp]);
			return;
		}
		if (Empire->VaultGold < SPY_COST) {
			// free up mem used by alliances
			for (iTemp = 0; iTemp < MAX_ALLIANCES; iTemp++)
				if (Alliances[iTemp])
					free(Alliances[iTemp]);
			rputs(ST_FMENUNOAFFORD);
			return;
		}
		else
			Empire->VaultGold -= SPY_COST;

		// spy on them here if possible
		// see if we can spy, if so, spy on 'em now using EmpireStats
		// increment spies per day in future
		if ((Empire->Buildings[B_AGENCY]+RANDOM(5)) >
				(Alliances[WhichAlliance]->Empire.Buildings[B_SECURITY]+RANDOM(3))) {
			// success!
			// rputs("Your spy is successful!\n");
			rputs(ST_SPY2);
			Empire_Stats(&Alliances[WhichAlliance]->Empire);
		}
		else {
			// rputs("Your spy failed and was captured!\n");
			rputs(ST_SPY3);

			sprintf(szMessage, ST_SPY5,
					Alliances[WhichAlliance]->szName, szSpierName);
			GenericMessage(szMessage, Alliances[WhichAlliance]->CreatorID, Junk, "", FALSE);
		}

		// free up mem used by alliances
		for (iTemp = 0; iTemp < MAX_ALLIANCES; iTemp++)
			if (Alliances[iTemp])
				free(Alliances[iTemp]);
	}
	else if (TypeToSpyOn == EO_CLAN) {
		// choose clan using GetClanID()

		if (GetClanID(ClanID, FALSE, FALSE, -1, -1) == FALSE) {
			return;
		}

		// sprintf(szString, "It will cost you %ld gold to spy.  The empire has %ld gold.\nContinue?",
		sprintf(szString, ST_SPY1, SPY_COST, Empire->VaultGold);
		if (YesNo(szString) == NO) {
			return;
		}
		if (Empire->VaultGold < SPY_COST) {
			rputs(ST_FMENUNOAFFORD);
			return;
		}
		else
			Empire->VaultGold -= SPY_COST;

		TmpClan = malloc(sizeof(struct clan));
		CheckMem(TmpClan);
		GetClan(ClanID, TmpClan);

		// spy on them here if possible
		// see if we can spy, if so, spy on 'em now using EmpireStats
		// increment spies per day in future
		if ((Empire->Buildings[B_AGENCY]+RANDOM(5)) >
				(TmpClan->Empire.Buildings[B_SECURITY]+RANDOM(3))) {
			// success!
			rputs(ST_SPY2);
			Empire_Stats(&TmpClan->Empire);
		}
		else {
			rputs(ST_SPY3);

			sprintf(szMessage, ST_SPY6, szSpierName);
			GenericMessage(szMessage, TmpClan->ClanID, Junk, "", FALSE);
		}

		FreeClan(TmpClan);
	}
	Empire->SpiesToday++;
}

// ------------------------------------------------------------------------- //
void Empire_Manage(struct empire *Empire)
{
	char *szTheOptions[9], szString[128];

	LoadStrings(1260, 9, szTheOptions);

	if (!PClan->EmpireHelp) {
		PClan->EmpireHelp = TRUE;
		Help("Empire Help", ST_NEWBIEHLP);
		rputs("\n%P");
	}

	/* get a choice */
	for (;;) {
		rputs("\n\n");

		/* show 'menu' */
		rputs(ST_MEMPIRE0);
		rputs(ST_LONGLINE);
		sprintf(szString, ST_MEMPIRE1, Empire->VaultGold);
		rputs(szString);
		sprintf(szString, ST_MEMPIRE2, Empire->Army.Rating);
		rputs(szString);
		sprintf(szString, ST_MEMPIRE3, Empire->Land);
		rputs(szString);
		sprintf(szString, ST_MEMPIRE4, Empire->WorkerEnergy);
		rputs(szString);
		sprintf(szString, ST_MEMPIRE5, Empire->Buildings[B_SECURITY]);
		rputs(szString);
		sprintf(szString, ST_MEMPIRE6, Empire->Buildings[B_AGENCY]);
		rputs(szString);
		sprintf(szString, ST_MEMPIRE10,Empire->Buildings[B_DEVELOPERS]);
		rputs(szString);

		switch (GetChoice("Empire Menu", ST_ENTEROPTION, szTheOptions, "BMAQ?SDLH", 'Q', TRUE)) {
			case 'H' :  // help
				GeneralHelp(ST_EMPIREHLP);
				break;
			case 'L' :  // buy land
				DevelopLand(Empire);
				break;
			case 'D' :  // donate to empire
				if (Empire->OwnerType == EO_CLAN)
					// you can't donate to your own clan
					rputs(ST_MEMPIRE7);
				else
					DonateToEmpire(Empire);
				break;
			case 'B' :  // build structure
				StructureMenu(Empire);
				break;
			case 'M' :  // manage army
				if (Empire->Buildings[B_BARRACKS] == 0)
					// rputs("You need a barracks first!\n");
					rputs(ST_MEMPIRE8);
				else
					ManageArmy(Empire);
				break;
			case 'A' :  // attack empire
				if (PClan->Protection && Empire->OwnerType == EO_CLAN) {
					rputs("|07You cannot attack another empire while in protection.\n%P");
					break;
				}

				if (Empire->Buildings[B_BARRACKS] == 0)
					rputs(ST_MEMPIRE8);
				else {
					// if this is the user, tell him "You can only command X
					// attacks per day"
					if (Empire->OwnerType == EO_VILLAGE) {
						if (Empire->AttacksToday == MAX_VILLAGEEMPIREATTACKS) {
							// rputs("The village army can only handle 5 attacks per day.\n");
							rputs(ST_MEMPIRE32);
							break;
						}
						StartEmpireWar(Empire);
					}
					else {
						if (PClan->Empire.AttacksToday == MAX_EMPIREATTACKS) {
							// rputs("You can only command 5 attacks per day.\n");
							rputs(ST_MEMPIRE33);
							break;
						}
						StartEmpireWar(Empire);
					}
				}
				break;
			case 'S' :  // spy on empire
				if (Empire->Buildings[B_AGENCY] == 0)
					// rputs("You need an intelligence agency first!\n");
					rputs(ST_MEMPIRE9);
				else
					SpyMenu(Empire);
				break;
			case 'Q' :      /* return to previous menu */
				return;
			case '?' :      /* redisplay options */
				break;
		}
	}
}

