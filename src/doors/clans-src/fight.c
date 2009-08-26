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
#include <ctype.h>

#ifdef __unix__
#include "unix_wrappers.h"
#else
# ifdef _MSC_VER
#  include <malloc.h>
# else
#  include <alloc.h>
# endif
#endif

#include <string.h>

#include "structs.h"
#include "mstrings.h"
#include "door.h"
#include "user.h"
#include "help.h"
#include "language.h"
#include "input.h"
#include "spells.h"
#include <OpenDoor.h>
#include "myopen.h"
#include "items.h"
#include "news.h"
#include "mail.h"
#include "video.h"


#define ATTACKER            0
#define DEFENDER            1
#define MAX_ROUNDS          50
#define MAX_MONSTERS        255

#define PLAYERTEAM          0


extern struct Language *Language;
extern struct Spell *Spells[MAX_SPELLS];
extern struct clan *PClan;
extern struct village Village;

struct move {
	_INT16 Action, Target, SpellNum, ScrollNum;
};

struct order {
	_INT16 TeamNum, MemberNum;
};

// ------------------------------------------------------------------------- //

void Fight_Heal(struct clan *Clan)
{
	_INT16 CurMember;

	for (CurMember = 0; CurMember < MAX_MEMBERS; CurMember++) {
		if (Clan->Member[CurMember] && Clan->Member[CurMember]->Status == Here)
			Clan->Member[CurMember]->HP = Clan->Member[CurMember]->MaxHP;
	}
}

void Fight_GetBattleOrder(struct order *BattleOrder, struct clan *Team[2])
{
	struct {
		_INT16 WhichTeam;
		_INT16 WhichPlayer;
		_INT16 WhichSortData;
		_INT16 AgiValue;
	} CurHighest;
	struct {
		_INT16 WhichTeam;
		_INT16 WhichPlayer;
		_INT16 AgiValue;                   // his "agility+energy" ranking
		_INT16 InList;                     // set if already in sort list
	} SortData[MAX_MEMBERS * 2];
	_INT16 CurSortMember, CurMember, CurTeam, CurOrder, TotalMembers;
	struct pc *TempPC;


	CurSortMember = 0;
	for (CurTeam = 0; CurTeam < 2; CurTeam++)
		for (CurMember = 0; CurMember < MAX_MEMBERS; CurMember++) {
			if (Team[CurTeam]->Member[CurMember] &&
					Team[CurTeam]->Member[CurMember]->Status == Here) {
				TempPC = Team[CurTeam]->Member[CurMember];

				SortData[CurSortMember].AgiValue =
					GetStat(TempPC, stAgility) + (TempPC->HP*10)/TempPC->MaxHP + RANDOM(2);
				SortData[CurSortMember].WhichTeam = CurTeam;
				SortData[CurSortMember].WhichPlayer = CurMember;
				SortData[CurSortMember].InList = FALSE;

				CurSortMember++;
			}
		}

	// now sort them
	TotalMembers = NumMembers(Team[0], TRUE) + NumMembers(Team[1], TRUE);

	if (TotalMembers != CurSortMember)
		DisplayStr(">>> TotalMembers != CurSortMember!\n%P");

	for (CurOrder = 0; CurOrder < TotalMembers; CurOrder++) {
		// initialize sort data for this iteration (loop)
		CurHighest.WhichTeam = -1;
		CurHighest.WhichPlayer = -1;
		CurHighest.WhichSortData = -1;
		CurHighest.AgiValue = -1;

		for (CurSortMember = 0; CurSortMember < TotalMembers; CurSortMember++) {
			// see who is the highest in value
			if (SortData[CurSortMember].AgiValue > CurHighest.AgiValue &&
					SortData[CurSortMember].InList == FALSE) {
				// Found a higher one
				CurHighest.WhichTeam =      SortData[CurSortMember].WhichTeam;
				CurHighest.WhichPlayer =    SortData[CurSortMember].WhichPlayer;
				CurHighest.AgiValue =       SortData[CurSortMember].AgiValue;
				CurHighest.WhichSortData =  CurSortMember;
			}
		}

		if (CurHighest.WhichSortData == -1) {
			break;
		}
		else {
			// record that turn
			BattleOrder[CurOrder].TeamNum = CurHighest.WhichTeam;
			BattleOrder[CurOrder].MemberNum = CurHighest.WhichPlayer;
			SortData[ CurHighest.WhichSortData ].InList = TRUE;

			// od_printf("Using %s\n\r", Team[ Turn[CurOrder].WhichTeam ]->Member[ Turn[CurOrder].WhichPlayer ]->szName);
		}
	}

}


void Fight_ManaRegenerate(struct pc *PC)
{
	/* if at max, return */
	if (PC->SP == PC->MaxSP)
		return;

	/* regenerate according to Wisdom attribute */
	PC->SP += ((GetStat(PC, ATTR_WISDOM)*(RANDOM(15)+5))/150 + 1);

	if (PC->SP >= PC->MaxSP)
		PC->SP = PC->MaxSP;
}

BOOL Fight_IsIncapacitated(struct pc *PC)
{
	/* return TRUE if dude is incapacitated */

	_INT16 iTemp;
	BOOL Result = FALSE;

	/* scan spellsineffect */

	for (iTemp = 0; iTemp < 10; iTemp++) {
		if (PC->SpellsInEffect[iTemp].SpellNum == -1)
			continue;

		/* see if this is incapacitate spell, if so return TRUE and tell dudes */
		if (Spells[ PC->SpellsInEffect[iTemp].SpellNum ]->TypeFlag & SF_INCAPACITATE) {
			Result = TRUE;

			/* tell dudes */
			rputs(Spells[ PC->SpellsInEffect[iTemp].SpellNum ]->pszStatusStr);
			rputs("\n");

			break;
		}
	}

	return Result;
}

_INT16 Fight_ChooseVictim(struct clan *EnemyClan)
{
	_INT16 LowestEnergy, TempEnergyPercentage, EnemyTarget = 0;
	_INT16 NumPCs, CurPC;
	_INT16 iTemp, Target;
	BOOL FoundTarget = FALSE;

	if (RANDOM(100) < 60) {
		/* attack guy with lowest HP percentage */

		//od_printf("Choosing guy with lowest HP\n\r");

		LowestEnergy = 110; /* set lowest energy percentage to 110% */

		for (iTemp = 0; iTemp < MAX_MEMBERS; iTemp++) {
			if (EnemyClan->Member[iTemp] && EnemyClan->Member[iTemp]->Status == Here) {
				TempEnergyPercentage =
					(EnemyClan->Member[iTemp]->HP * 100)/EnemyClan->Member[iTemp]->MaxHP;

				if (TempEnergyPercentage < LowestEnergy) {
					EnemyTarget = iTemp;
					LowestEnergy = TempEnergyPercentage;
					FoundTarget = TRUE;
				}
			}
		}
		/* found who to attack */
		Target = EnemyTarget;

		if (FoundTarget == FALSE) {
			rputs("Bug #1A\n%P");
		}
	}
	else {
		/* attack anyone at random */
		//od_printf("Choosing guy at random\n\r");

		/* find how many PCs in enemy clan */
		NumPCs = NumMembers(EnemyClan, TRUE);

		/* choose one at random */
		EnemyTarget = RANDOM(NumPCs);

		/* since the player listing may skip some spaces, we must
		   "seek" to that player in the member listing and find
		   out what number he is */
		CurPC = -1;
		for (iTemp = 0; iTemp < MAX_MEMBERS; iTemp++) {
			if (EnemyClan->Member[iTemp] && EnemyClan->Member[iTemp]->Status == Here)
				CurPC++;

			if (CurPC == EnemyTarget) {
				FoundTarget = TRUE;
				break;
			}
		}
		Target = iTemp;

		if (FoundTarget == FALSE) {
			rputs("Bug #1B\n%P");
		}
	}


	return Target;
}

_INT16 NumUndeadMembers(struct clan *Clan)
{
	_INT16 iTemp;
	_INT16 NumUndead = 0;

	for (iTemp = 0; iTemp < MAX_MEMBERS; iTemp++) {
		if (Clan->Member[iTemp] && Clan->Member[iTemp]->Undead &&
				Clan->Member[iTemp]->Status == Here)
			NumUndead++;
	}

	return NumUndead;
}


void Fight_GetNPCAction(struct pc *NPC, struct clan *EnemyClan, struct move *Move)
{
	_INT16 iTemp, NumSpellsTried, TotalSpells;
	_INT16 LowestEnergy, TempEnergyPercentage;
	BOOL SomeoneNeedsHeal, FoundHealSpell, FoundSpell;
	_INT16 WhoNeedsHeal = 0, HealSpellNum = 0, WhichSpell = 0;


	/* see if anyone needs healing in the clan, skip undead! */
	SomeoneNeedsHeal = FALSE;
	LowestEnergy = 60;
	for (iTemp = 0; iTemp < MAX_MEMBERS; iTemp++) {
		if (NPC->MyClan->Member[iTemp] == NULL)
			continue;

		if (!NPC->MyClan->Member[iTemp]->Undead && NPC->MyClan->Member[iTemp]->Status == Here) {
			TempEnergyPercentage = (NPC->MyClan->Member[iTemp]->HP * 100) /
								   NPC->MyClan->Member[iTemp]->MaxHP;

			if (TempEnergyPercentage < 50) {
				/* this guy needs healing */
				SomeoneNeedsHeal = TRUE;

				if (TempEnergyPercentage < LowestEnergy) {
					WhoNeedsHeal = iTemp;
					LowestEnergy = TempEnergyPercentage;
				}
			}
		}
	}


	/* this is used to see if he should run or not */
	// EnergyPercentage = (NPC->HP * 100)/NPC->MaxHP;

	/* see if any spells available */
	if (NPC->SpellsKnown[0] != 0) {
		/* Yes, knows at least one spell */

		/* see if any heal spells and can cast 'em */
		FoundHealSpell = FALSE;
		for (iTemp = 0; iTemp < MAX_SPELLS; iTemp++) {
			if (NPC->SpellsKnown[iTemp] == 0)
				break;  /* no more spells to check */

			if (Spells[ NPC->SpellsKnown[iTemp] - 1 ]->TypeFlag & SF_HEAL) {
				/* see if can cast it */
				if (NPC->SP >= Spells[ NPC->SpellsKnown[iTemp] - 1]->SP) {
					FoundHealSpell = TRUE;
					HealSpellNum = NPC->SpellsKnown[iTemp] - 1;
					break;
				}
			}
		}

		/* if FoundHealSpell is TRUE, that means a heal spell was found
		   and it CAN be cast.  Decide now whether to cast or not using
		   randomness */
		if (FoundHealSpell && SomeoneNeedsHeal && RANDOM(100) < 55) {
			/* 55% of the time, he'll heal him if he can */
			Move->Action = acCast;
			Move->Target = WhoNeedsHeal;
			Move->SpellNum = HealSpellNum;

			//rputs("|15Enemy casts HEAL!\n");

			return;
		}
		else {
			/* see what other spells are available, cast 'em if
			   enough SP and randomness allows it */

			/* keep going through spells at random till gone through all
			   of 'em */

			/* see how many spells that guy has first */
			for (iTemp = 0; iTemp < MAX_SPELLS; iTemp++) {
				if (NPC->SpellsKnown[iTemp] == 0)
					break;
			}
			TotalSpells = iTemp;

			NumSpellsTried = 0;
			FoundSpell = FALSE;
			while (NumSpellsTried < TotalSpells) {
				/* choose spell at random*/
				WhichSpell = RANDOM(TotalSpells);

				/* see if this spell can be cast  and is NOT a heal spell */
				if (NPC->SP >= Spells[ NPC->SpellsKnown[WhichSpell] - 1]->SP &&
						!(Spells[ NPC->SpellsKnown[WhichSpell] - 1]->TypeFlag & SF_HEAL)) {
					/* if so, set FoundSpell to true and break from loop */
					FoundSpell = TRUE;
					break;
				}

				/* else increment numspellstried and continue */
				NumSpellsTried++;
			}

			/* if spell found, see if we can cast this, see
			   what type of spell to choose whether or not to cast */
			if (FoundSpell) {
				/* according to spell type, see if we can using randomness */
				if ((Spells[ NPC->SpellsKnown[WhichSpell] - 1]->TypeFlag &
						SF_RAISEUNDEAD) && (RANDOM(10) == 0)) {
					// od_printf("Gonna cast %s\n\r", Spells[NPC->SpellsKnown[WhichSpell] - 1]->szName);

					/* 1 in 10 chance of casting this spell */

					/* raise undead spell cast */
					Move->Action = acCast;
					Move->Target = 0;        /* target means nothing with this spell */
					Move->SpellNum = NPC->SpellsKnown[WhichSpell] - 1;
					return;
				}
				if ((Spells[ NPC->SpellsKnown[WhichSpell] - 1]->TypeFlag &
						SF_BANISHUNDEAD) && RANDOM(5) == 0 &&
						NumUndeadMembers(EnemyClan) != 0) {
					// od_printf("Gonna cast %s\n\r", Spells[NPC->SpellsKnown[WhichSpell] - 1]->szName);

					/* ALSO check to see if other clan even has undead */
					/* 1 in 5 chance of casting this spell */

					/* banish undead spell cast */
					Move->Action = acCast;
					Move->Target = 0;        /* target means nothing with this spell */
					Move->SpellNum = NPC->SpellsKnown[WhichSpell] - 1;
					return;
				}
				if ((Spells[ NPC->SpellsKnown[WhichSpell] - 1]->TypeFlag &
						SF_DAMAGE) && RANDOM(3) == 0) {
					// od_printf("Gonna cast %s\n\r", Spells[NPC->SpellsKnown[WhichSpell] - 1]->szName);

					/* 1 in 3 chance of casting this spell */

					/* damage spell */
					Move->Action = acCast;
					Move->Target = Fight_ChooseVictim(EnemyClan);
					Move->SpellNum = NPC->SpellsKnown[WhichSpell] - 1;
					return;
				}
				if (((Spells[ NPC->SpellsKnown[WhichSpell] - 1]->TypeFlag &
						SF_MODIFY) || (Spells[ NPC->SpellsKnown[WhichSpell] - 1]->TypeFlag &
									   SF_INCAPACITATE)) && RANDOM(5) == 0) {
					// od_printf("Gonna cast %s\n\r", Spells[NPC->SpellsKnown[WhichSpell] - 1]->szName);

					/* of course, in future, we'd see who is the strongest
					   dude on other team before casting */
					/* 1 in 13 chance of casting this spell */

					Move->Action = acCast;

					if (Spells[ NPC->SpellsKnown[WhichSpell] - 1]->Friendly) {
						/* modify/incapacitate cast */
						Move->Target = Fight_ChooseVictim(NPC->MyClan);
						// FIXME:
//                        od_printf("Gonna cast %s on %s\n\r", Spells[NPC->SpellsKnown[WhichSpell] - 1]->szName,
//                            NPC->MyClan->Member[*Target]->szName);
					}
					else {
						Move->Target = Fight_ChooseVictim(EnemyClan);
//                        od_printf("Gonna cast %s on %s\n\r", Spells[NPC->SpellsKnown[WhichSpell] - 1]->szName,
//                            EnemyClan->Member[*Target]->szName);
					}


					Move->SpellNum = NPC->SpellsKnown[WhichSpell] - 1;
					return;
				}

				/* if all this failed, fall through and attack enemy instead */
			}
		}
	}


	Move->Action = acAttack;

	/* choose who to attack */
	Move->Target = Fight_ChooseVictim(EnemyClan);

	/* otherwise attack */
	// od_printf("Gonna attack %s\n\r", EnemyClan->Member[*Target]->szName);
}

void Fight_Stats(struct clan *PlayerClan, struct clan *MobClan, struct pc *WhichPC)
{
	_INT16 CurMember, MembersShown;
	char szString[128];

	rputs("\n");
	MembersShown = 0;
	for (CurMember = 0; CurMember < MAX_MEMBERS; CurMember++)
		if (PlayerClan->Member[CurMember] && PlayerClan->Member[CurMember]->Status != RanAway) {
			if (PlayerClan->Member[CurMember]->HP <= 0)
				continue;   // skip him

			if ((MembersShown%4) == 0 && MembersShown)
				rputs("\n");

			if (PlayerClan->Member[CurMember] == WhichPC)
				rputs("|17");

			if ((PlayerClan->Member[CurMember]->HP*100)/PlayerClan->Member[CurMember]->MaxHP
					<= 50)
				rputs("|06");
			else if ((PlayerClan->Member[CurMember]->HP*100)/PlayerClan->Member[CurMember]->MaxHP
					 <= 75)
				rputs("|12");
			else
				rputs("|07");

			sprintf(szString, " %s %d/%d ", PlayerClan->Member[CurMember]->szName,
					PlayerClan->Member[CurMember]->HP,
					PlayerClan->Member[CurMember]->MaxHP);
			rputs(szString);

			if (PlayerClan->Member[CurMember] == WhichPC)
				rputs("|16");

			MembersShown++;
		}

	rputs("\n\n");
	MembersShown = 0;
	for (CurMember = 0; CurMember < MAX_MEMBERS; CurMember++)
		if (MobClan->Member[CurMember] && MobClan->Member[CurMember]->Status != RanAway)
			//MobClan->Member[CurMember]->HP > 0)
		{
			if (MobClan->Member[CurMember]->HP <= 0)
				continue;   // skip him

			if ((MembersShown%4) == 0 && MembersShown)
				rputs("\n");

			if ((MobClan->Member[CurMember]->HP*100)/MobClan->Member[CurMember]->MaxHP
					<= 50)
				rputs("|06");
			else if ((MobClan->Member[CurMember]->HP*100)/MobClan->Member[CurMember]->MaxHP
					 <= 75)
				rputs("|12");
			else
				rputs("|13");

			sprintf(szString, " %s ", MobClan->Member[CurMember]->szName);
			rputs(szString);
			MembersShown++;
		}
	rputs("\n\n");
}

_INT16 FirstAvailable(struct clan *Clan)
{
	_INT16 CurMember;

	/* see if that guy is available to attack, if not, attack somebody else */
	for (CurMember = 0; CurMember < MAX_MEMBERS; CurMember++)
		if (Clan->Member[CurMember] && Clan->Member[CurMember]->Status == Here)
			break;

	if (CurMember == MAX_MEMBERS) { /* nobody there in that clan! */
		return (-1);
	}
	else
		return CurMember;
}


_INT16 Fight_GetTarget(struct clan *MobClan, _INT16 Default)
{
	_INT16 CurMember, MemberNumber, CurIndex;
	_INT16 Index[MAX_MEMBERS];             // indexes who's around to fight
	_INT16 TotalMembers;
	char cInput, szString[128];

	// get list of those who are alive
	CurIndex = 0;
	for (CurMember = 0; CurMember < MAX_MEMBERS; CurMember++) {
		if (MobClan->Member[CurMember] == NULL ||
				MobClan->Member[CurMember]->HP <= 0)
			continue;

		Index[CurIndex] = CurMember;

		CurIndex++;
	}
	TotalMembers = CurIndex;        // total members who are alive

	rputs(ST_FIGHTTARGET1);

	if (Default && Default-1 < TotalMembers)
		MemberNumber = Index[Default-1];
	else
		for (;;) {
			cInput = toupper(od_get_key(TRUE));

			if (cInput == '?') {
				rputs("List\n");

				// list who's here
				for (CurIndex = 0; CurIndex < TotalMembers; CurIndex++) {
					sprintf(szString, "(%c) %s\n",
							CurIndex+'A', MobClan->Member[ Index[CurIndex] ]->szName);
					rputs(szString);
				}
				rputs(ST_FIGHTTARGET1);
			}
			else if (cInput == '\r' || cInput == '\n') {
				MemberNumber = FirstAvailable(MobClan);
				break;
			}
			else {
				if (isdigit(cInput))
					CurIndex = cInput - '1';
				else
					CurIndex = cInput - 'A';

				if (CurIndex >= TotalMembers || CurIndex < 0)
					continue;

				MemberNumber = Index[CurIndex];
				break;
			}
		}

	sprintf(szString, "%s\n\n", MobClan->Member[ MemberNumber ]->szName);
	rputs(szString);
	return MemberNumber;
}



// ------------------------------------------------------------------------- //

BOOL CanRun(struct clan *RunningClan, struct clan *StayingClan)
{
	/* returns TRUE if RunningClan can run away */
	_INT16 RunningVariable, StayingVariable;
	_INT16 AllAgilities, AllDexterities, AvgWisdom, iTemp, TotalWisdom;
	_INT16 TotalMembers;

	TotalMembers = NumMembers(RunningClan, TRUE);

	AllAgilities = 0;
	for (iTemp = 0; iTemp < MAX_MEMBERS; iTemp++) {
		if (RunningClan->Member[iTemp] != NULL &&
				RunningClan->Member[iTemp]->Undead == FALSE)
			AllAgilities += GetStat(RunningClan->Member[iTemp], ATTR_AGILITY);
	}
	AllDexterities = 0;
	for (iTemp = 0; iTemp < MAX_MEMBERS; iTemp++) {
		if (RunningClan->Member[iTemp] != NULL &&
				RunningClan->Member[iTemp]->Undead == FALSE)
			AllDexterities += GetStat(RunningClan->Member[iTemp], ATTR_DEXTERITY);
	}
	TotalWisdom = 0;
	for (iTemp = 0; iTemp < MAX_MEMBERS; iTemp++) {
		if (RunningClan->Member[iTemp] != NULL &&
				RunningClan->Member[iTemp]->Undead == FALSE)
			TotalWisdom += GetStat(RunningClan->Member[iTemp], ATTR_WISDOM);
	}
	AvgWisdom = TotalWisdom / TotalMembers;

	RunningVariable = AllAgilities + AllDexterities + AvgWisdom + TotalMembers*3
					  + RANDOM(15);


	TotalMembers = NumMembers(StayingClan, TRUE);

	AllAgilities = 0;
	for (iTemp = 0; iTemp < MAX_MEMBERS; iTemp++) {
		if (StayingClan->Member[iTemp] != NULL &&
				StayingClan->Member[iTemp]->Undead == FALSE)
			AllAgilities += GetStat(StayingClan->Member[iTemp], ATTR_AGILITY);
	}
	AllDexterities = 0;
	for (iTemp = 0; iTemp < MAX_MEMBERS; iTemp++) {
		if (StayingClan->Member[iTemp] != NULL &&
				StayingClan->Member[iTemp]->Undead == FALSE)
			AllDexterities += GetStat(StayingClan->Member[iTemp], ATTR_DEXTERITY);
	}
	TotalWisdom = 0;
	for (iTemp = 0; iTemp < MAX_MEMBERS; iTemp++) {
		if (StayingClan->Member[iTemp] != NULL &&
				StayingClan->Member[iTemp]->Undead == FALSE)
			TotalWisdom += GetStat(StayingClan->Member[iTemp], ATTR_WISDOM);
	}
	AvgWisdom = TotalWisdom / TotalMembers;

	StayingVariable = AllAgilities + AllDexterities + AvgWisdom + TotalMembers*3
					  + RANDOM(15);

	if (RunningVariable > StayingVariable)
		return TRUE;
	else
		return FALSE;
}


BOOL Fight_Dead(struct clan *Clan)
{
	BOOL FoundLiving = FALSE;
	_INT16 CurMember;

	// See if player clan dead or has no living members
	for (CurMember = 0; CurMember < MAX_MEMBERS; CurMember++)
		if (Clan->Member[CurMember] &&
				Clan->Member[CurMember]->Undead == FALSE &&
				Clan->Member[CurMember]->Status == Here) {
			FoundLiving = TRUE;
			break;
		}

	if (NumMembers(Clan, TRUE) == 0 || FoundLiving == FALSE) {
		rputs(ST_FIGHTDEFEAT);
		return TRUE;
	}
	else {
		return FALSE;
	}
}

void Fight_BattleAttack(struct pc *Attacker, struct clan *VictimClan, _INT16 Who,
						BOOL SkipOutput)
{
	_INT16 CurMember, Damage, PercentGold;
	char szString[128];
	long XPGained, GoldGained, TaxedGold;

	if (Attacker->MyClan == PClan)
		rputs(ST_FIGHTYOURCOLOR);
	else
		rputs(ST_FIGHTENEMYCOLOR);

	/* see if that guy is available to attack, if not, attack somebody else */
	if (VictimClan->Member[Who] == NULL ||
			VictimClan->Member[Who]->Status != Here) {
		/* find somebody else since this guy ain't here! */
		CurMember = FirstAvailable(VictimClan);

		if (CurMember == -1)
			return;
		else
			Who = CurMember;
	}

	/* Figure out if attack lands on other victim */
	if ((GetStat(Attacker, stDexterity) + RANDOM(4) + RANDOM(4) + 2) <
			(GetStat(VictimClan->Member[Who], stAgility) + RANDOM(4))) {
		if (SkipOutput == FALSE) {
			sprintf(szString, ST_FIGHTMISS, Attacker->szName);
			rputs(szString);
		}
	}
	else {
		Damage = ((GetStat(Attacker, stStrength)/2)*(RANDOM(40)+80))/100   -
				 GetStat(VictimClan->Member[Who], stArmorStr);

		if (Damage <= 0)
			Damage = 1;

		if (SkipOutput == FALSE) {
			sprintf(szString, ST_FIGHTATTACK, Attacker->szName, VictimClan->Member[Who]->szName, Damage);
			rputs(szString);
		}

		/* experience goes here */
		XPGained = Damage/3 + 1;

		if (SkipOutput == FALSE) {
			sprintf(szString, ST_FIGHTXP, XPGained);
			rputs(szString);
		}

		Attacker->Experience += XPGained;

		if (!SkipOutput)
			rputs("\n\n");

		VictimClan->Member[Who]->HP -= Damage;
	}

	if (VictimClan->Member[Who]->HP <= 0) {
		//od_printf("in battleattack\n\r");

		/* according to how bad the hit was, figure out status */
		if (VictimClan->szName[0] == 0) {
			VictimClan->Member[Who]->Status = Dead;
			sprintf(szString, ST_FIGHTKILLED, VictimClan->Member[Who]->szName,
					VictimClan->Member[Who]->Difficulty);

			/* give xp because of death */
			Attacker->Experience += (VictimClan->Member[Who]->Difficulty);
		}
		else if (VictimClan->Member[Who]->HP < -15) {
			VictimClan->Member[Who]->Status = Dead;
			sprintf(szString, ST_FIGHTKILLED, VictimClan->Member[Who]->szName,
					VictimClan->Member[Who]->Level*2);

			/* loses percentage of MaxHP */
			VictimClan->Member[Who]->MaxHP = (VictimClan->Member[Who]->MaxHP * (RANDOM(10)+90))/100;

			/* give xp because of death */
			Attacker->Experience += (VictimClan->Member[Who]->Level*2);
		}
		else if (VictimClan->Member[Who]->HP < -5) {
			VictimClan->Member[Who]->Status = Unconscious;
			sprintf(szString, ST_FIGHTMORTALWOUND, VictimClan->Member[Who]->szName,
					VictimClan->Member[Who]->Level);

			/* loses percentage of MaxHP */
			VictimClan->Member[Who]->MaxHP = (VictimClan->Member[Who]->MaxHP * (RANDOM(10)+90))/100;

			Attacker->Experience += (VictimClan->Member[Who]->Level);
		}
		else {
			VictimClan->Member[Who]->Status = Unconscious;
			sprintf(szString, ST_FIGHTKNOCKEDOUT, VictimClan->Member[Who]->szName,
					VictimClan->Member[Who]->Level);

			Attacker->Experience += VictimClan->Member[Who]->Level;
		}
		rputs(szString);

		/* give gold to clan */
		if (Attacker->MyClan == PClan) {
			if (VictimClan->Member[Who]->Difficulty != -1) {
				GoldGained = VictimClan->Member[Who]->Difficulty*((long)RANDOM(10) + 20L) + 50L + (long)RANDOM(20);
				sprintf(szString, ST_FIGHTGETGOLD, GoldGained);
				rputs(szString);

				/* take some away due to taxes */
				TaxedGold = (long)(GoldGained * Village.Data->TaxRate)/100L;
				if (TaxedGold) {
					sprintf(szString, ST_FIGHTTAXEDGOLD, TaxedGold);
					rputs(szString);
				}

				if ((GoldGained-TaxedGold) > 0) {
					PClan->Empire.VaultGold += (GoldGained-TaxedGold);
					Village.Data->Empire.VaultGold += TaxedGold;
				}
			}
		}

		// if that character was an undead dude, free him up
		if (VictimClan->Member[Who]->Undead) {
			// BUGFIX:
			free(VictimClan->Member[Who]);
			VictimClan->Member[Who] = NULL;
		}

	}
	(void)PercentGold;
}


BOOL Fight_DoMove(struct pc *AttackerPC, struct move Move, struct clan *Defender,
				  BOOL FightToDeath, _INT16 CurRound)
/*
 * returns TRUE if user ran away
 *
 */
{
	// do action
	if (Move.Action == acCast) {
		/* cast spell */
		Spells_CastSpell(AttackerPC, Defender, Move.Target, Move.SpellNum);
	}
	else if (Move.Action == acSkip) {
		// do nothing
	}
	else if (Move.Action == acRead) {
		/* read scroll */
		Items_ReadScroll(AttackerPC, Defender, Move.Target, Move.ScrollNum);
	}
	else if (Move.Action == acAttack) {
		Fight_BattleAttack(AttackerPC, Defender, Move.Target, FightToDeath);
	}
	else if (Move.Action == acRun) {
		// if FightToDeath and they chose to run, then allow
		// them to regardless of whether or not they can
		if (CanRun(AttackerPC->MyClan, Defender) ||
				(FightToDeath && CurRound >= MAX_ROUNDS)) {
			rputs(ST_FIGHTRUNAWAY);
			return TRUE;
		}
		else {
			// cannot run away
			rputs(ST_FIGHTNORUN);
		}
	}

	return FALSE;
}

_INT16 GetTarget2(struct clan *Clan, _INT16 Default)
{
	_INT16 CurMember, MemberNumber, CurIndex;
	_INT16 Index[MAX_MEMBERS];             // indexes who's around to fight
	_INT16 TotalMembers;
	char cInput, szString[50];

	// get list of those who are alive
	CurIndex = 0;
	for (CurMember = 0; CurMember < MAX_MEMBERS; CurMember++) {
		if (Clan->Member[CurMember] == NULL || Clan->Member[CurMember]->HP <= 0)
			continue;

		Index[CurIndex] = CurMember;

		CurIndex++;
	}
	TotalMembers = CurIndex;    // total members who are alive


	// list who's here
	for (CurIndex = 0; CurIndex < TotalMembers; CurIndex++) {
		if (Clan == PClan)
			sprintf(szString, ST_FIGHTTARGETLIST1, CurIndex+'A', Clan->Member[Index[CurIndex]]->szName,
					Clan->Member[Index[CurIndex]]->HP, Clan->Member[Index[CurIndex]]->MaxHP);
		else
			sprintf(szString, ST_FIGHTTARGETLIST2, CurIndex+'A', Clan->Member[Index[CurIndex]]->szName);

		rputs(szString);
	}

	rputs("|0SUse on whom? (|0B?=List|0S): |0F");

	for (;;) {
		cInput = toupper(od_get_key(TRUE));

		if (cInput == '?') {
			rputs("List\n");

			// list who's here
			for (CurIndex = 0; CurIndex < TotalMembers; CurIndex++) {
				if (Clan == PClan)
					sprintf(szString, ST_FIGHTTARGETLIST1, CurIndex+'A', Clan->Member[Index[CurIndex]]->szName,
							Clan->Member[Index[CurIndex]]->HP, Clan->Member[Index[CurIndex]]->MaxHP);
				else
					sprintf(szString, ST_FIGHTTARGETLIST2, CurIndex+'A', Clan->Member[Index[CurIndex]]->szName);

				rputs(szString);
			}
			rputs("|0SUse on whom? (|0B?=List|0S): |0F");

		}
		else if (cInput == '\r' || cInput == 'Q' || cInput == '\n') {
			MemberNumber = FirstAvailable(Clan);
			return -1;
		}
		else {
			if (isdigit(cInput))
				CurIndex = cInput - '1';
			else
				CurIndex = cInput - 'A';

			if (CurIndex >= TotalMembers || CurIndex < 0)
				continue;

			MemberNumber = Index[CurIndex];
			break;
		}
	}

	sprintf(szString, "%s\n\n", Clan->Member[ MemberNumber ]->szName);
	rputs(szString);
	return MemberNumber;
}


BOOL Fight_ReadyScroll(struct pc *PC, struct clan *TargetClan, struct move *Move)
{
	_INT16 ItemIndex, SpellChosen;

	// list items
	ListItems(PC->MyClan);

	// choose one
	ItemIndex = (_INT16)GetLong("|0SWhich scroll to read?", 0, MAX_ITEMS_HELD);

	// if not chosen properly, return 0
	if (ItemIndex == 0) {
		rputs(ST_ABORTED);
		return 0;
	}

	ItemIndex--;

	if (PC->MyClan->Items[ItemIndex].Available == FALSE ||
			PC->MyClan->Items[ItemIndex].cType != I_SCROLL) {
		rputs(ST_INVALIDITEM);
		return 0;
	}

	// see if can even use that scroll
	if (ItemPenalty(PC, &PC->MyClan->Items[ItemIndex])) {
		rputs("That character cannot use that item!\n%P");
		return 0;
	}

	// choose target now as if this was a regular spell

	SpellChosen = PC->MyClan->Items[ItemIndex].SpellNum;
	Move->ScrollNum = ItemIndex;

	/* get target */
	if (Spells[SpellChosen]->Target) {
		if (Spells[SpellChosen]->Friendly) {
			/* choose from your players */
			Move->Target = GetTarget2(PC->MyClan, 0);
		}
		else {
			/* choose from enemies */
			Move->Target = GetTarget2(TargetClan, 0);
		}
		if (Move->Target == -1) {
			rputs(ST_ABORTED);
			rputs("\n");
			return FALSE;
		}
	}
	else
		Move->Target = 0;

	return 1;   // sucess
}

BOOL Fight_ChooseSpell(struct pc *PC, struct clan *VictimClan, struct move *Move)
{
	_INT16 iTemp, SpellChosen;
	char szKeys[MAX_SPELLS + 5], szString[80], Choice;
	_INT16 NumSpellsKnown;

	/* so we have szKeys[] = "?QABCDEFGHI..." */
	szKeys[0] = '?';
	szKeys[1] = 'Q';
	szKeys[2] = '\r';
	szKeys[3] = '\n';
	for (iTemp = 0; iTemp < MAX_SPELLS; iTemp++)
		szKeys[iTemp + 4] = 'A' + iTemp;
	szKeys[iTemp+3] = 0;

	rputs("\n");


	/* if no spells, tell user to go away */

	/* ---- assume all spells known for now */

	NumSpellsKnown = 0;
	for (iTemp = 0; iTemp < MAX_SPELLS; iTemp++) {
		if (PC->SpellsKnown[iTemp])
			NumSpellsKnown++;
	}

	if (NumSpellsKnown == 0) {
		rputs(ST_CSPELL0);
		return FALSE;
	}


	for (;;) {
		/* list spells known */
		for (iTemp = 0; iTemp < MAX_SPELLS; iTemp++) {
			if (PC->SpellsKnown[iTemp] == 0) {
				szKeys[iTemp + 4] = 0;
				break;
			}

			sprintf(szString, ST_CSPELL1, iTemp+'A',
					PC->SP >= Spells[ PC->SpellsKnown[iTemp]-1 ]->SP ? "|0C" : "|08",
					Spells[ PC->SpellsKnown[iTemp]-1 ]->szName,
					Spells[ PC->SpellsKnown[iTemp]-1 ]->SP);
			rputs(szString);
		}

		// show more options
		rputs(ST_CSPELL2);

		/* get input */
		rputs(ST_CSPELL3);

		// if default spell chosen, do that instead
		if (Move->SpellNum == -1)
			Choice = od_get_answer(szKeys);
		else
			Choice = Move->SpellNum + 'A';

		if (Choice == '?') {
			rputs("Help\n\n");
			Help("Spells", ST_SPELLHLP);
			door_pause();
			rputs("\n");
		}
		else if (Choice == 'Q' || Choice == '\r' || Choice == '\n') {
			rputs(ST_ABORTED);
			rputs("\n");
			return FALSE;
		}
		else {
			SpellChosen = PC->SpellsKnown[ Choice - 'A' ] - 1;

			rputs(Spells[SpellChosen]->szName);
			rputs("\n\n");

			/* show info */
			Help(Spells[SpellChosen]->szName, ST_SPELLHLP);

			/* see if can even use that spell */
			if (PC->SP < Spells[SpellChosen]->SP) {
				// can't cast spell, not enough SP
				rputs(ST_CSPELL4);
				return FALSE;
			}

			// ask if they really want to cast it for sure
			if (YesNo(ST_CSPELL5) == YES)
				break;
			else    // aborted it
				Move->SpellNum = -1;
		}
	}

	Move->SpellNum = SpellChosen;

	/* get target */
	if (Spells[SpellChosen]->Target) {
		if (Spells[SpellChosen]->Friendly) {
			/* choose from your players */
			Move->Target = GetTarget2(PC->MyClan, 0);
		}
		else {
			/* choose from enemies */
			Move->Target = GetTarget2(VictimClan, 0);
		}
		if (Move->Target == -1) {
			rputs(ST_ABORTED);
			rputs("\n");
			return FALSE;
		}
	}
	else
		Move->Target = 0;

	/* use up SP */
	PC->SP -= Spells[SpellChosen]->SP;

	/* if successful spell choice */
	return TRUE;
}


_INT16 Fight_Fight(struct clan *Attacker, struct clan *Defender,
				   BOOL HumanEnemy, BOOL CanRun, BOOL AutoFight)
/*
 * Returns TRUE if Attacker won.
 *
 */
{
	struct clan *Team[2];
	char *Options[20], szString[255];
	struct order BattleOrder[MAX_MEMBERS*2];
	struct move Move;
	_INT16 TotalMembers, CurMember, Choice, CurRound = 0;
	BOOL FightToDeath = FALSE, DoneMove;

	Team[0] = Attacker;
	Team[1] = Defender;

	if (AutoFight)
		FightToDeath = TRUE;

	LoadStrings(1000, 20, Options);


	// replenish both sides' energy
	Fight_Heal(Defender);
	Fight_Heal(Attacker);

	// header stuff goes here
	if (HumanEnemy) {
		sprintf(szString, ST_FIGHTVSHEADER, Attacker->szName, Defender->szName);
		rputs(szString);
	}

	// main loop
	for (;; CurRound++) {
		TotalMembers = NumMembers(Attacker, TRUE) + NumMembers(Defender, TRUE);

		// figure out order of battle
		Fight_GetBattleOrder(BattleOrder, Team);

		for (CurMember = 0; CurMember < TotalMembers; CurMember++) {
			// if that player is non-existant, skip him
			if (Team[ BattleOrder[CurMember].TeamNum ]->Member[ BattleOrder[CurMember].MemberNum ] == NULL ||
					Team[ BattleOrder[CurMember].TeamNum ]->Member[ BattleOrder[CurMember].MemberNum ]->Status != Here)
				continue;

			// otherwise:
			Spells_UpdatePCSpells(Team[ BattleOrder[CurMember].TeamNum ]->Member[ BattleOrder[CurMember].MemberNum ]);
			Fight_ManaRegenerate(Team[ BattleOrder[CurMember].TeamNum ]->Member[ BattleOrder[CurMember].MemberNum ]);

			if (Fight_IsIncapacitated(Team[ BattleOrder[CurMember].TeamNum ]->Member[ BattleOrder[CurMember].MemberNum ]))
				continue;

			if (BattleOrder[CurMember].TeamNum == DEFENDER ||
					(BattleOrder[CurMember].TeamNum == ATTACKER && FightToDeath) ||
					Team[ BattleOrder[CurMember].TeamNum ]->Member[ BattleOrder[CurMember].MemberNum ]->Undead) {
				if (CurRound >= MAX_ROUNDS && BattleOrder[CurMember].TeamNum == PLAYERTEAM) {
					// run away
					Move.Action = acRun;
					rputs("\n\n|0CSeeing as the enemy will not yield, you choose to retreat.\n\n");
				}
				else
					Fight_GetNPCAction(Team[ BattleOrder[CurMember].TeamNum ]->Member[ BattleOrder[CurMember].MemberNum ],
									   Team[ !BattleOrder[CurMember].TeamNum ], &Move);
			}
			else {
				// it's a human and fight to death off

				DoneMove = FALSE;

				while (!DoneMove) {
					// show stats
					Fight_Stats(Attacker, Defender, Attacker->Member[ BattleOrder[CurMember].MemberNum ]);

					rputs(ST_FIGHTOPTIONS);

					sprintf(szString, ST_FIGHTPSTATS, Team[ BattleOrder[CurMember].TeamNum ]->Member[ BattleOrder[CurMember].MemberNum ]->szName,
							Team[ BattleOrder[CurMember].TeamNum ]->Member[ BattleOrder[CurMember].MemberNum ]->HP, Team[ BattleOrder[CurMember].TeamNum ]->Member[ BattleOrder[CurMember].MemberNum ]->MaxHP,
							Team[ BattleOrder[CurMember].TeamNum ]->Member[ BattleOrder[CurMember].MemberNum ]->SP, Team[ BattleOrder[CurMember].TeamNum ]->Member[ BattleOrder[CurMember].MemberNum ]->MaxSP);
					rputs(szString);

					// show options
					switch (Choice = GetChoice("", "|0E> |0F", Options, "SAR123456789IPK?F#ED", 'D', FALSE)) {
						case 'S' :  // specific move
						case '1' :
						case '2' :
						case '3' :
						case '4' :
						case '5' :
						case '6' :
						case '7' :
						case '8' :
						case '9' :
							if (Choice == 'S')
								Choice = 0;
							else
								Choice -= '0';

							/* ask who to attack */
							if ((Move.Target = Fight_GetTarget(Team[ !BattleOrder[CurMember].TeamNum ], Choice)) != -1) {
								DoneMove = TRUE;
								Move.Action = acAttack;
							}
							break;
						case 'A' :  // attack
							/* ask who to attack */
							Move.Target = 0;
							Move.Action = acAttack;
							DoneMove = TRUE;
							break;
						case 'R' :  // run away
							if (HumanEnemy) {
								rputs(ST_FIGHTNORUN2);
								break;
							}
							else if (CanRun == FALSE) {
								rputs("|12** |14You cannot run in this battle.\n");
								break;
							}
							else {
								Move.Action = acRun;
								DoneMove = TRUE;
							}
							break;
						case 'I' :  // parry blow
							Move.Action = acSkip;
							DoneMove = TRUE;
							break;
						case 'P' :
							ShowPlayerStats(Team[ BattleOrder[CurMember].TeamNum ]->Member[ BattleOrder[CurMember].MemberNum ], FALSE);
							door_pause();
							break;
						case 'K' :
							Move.SpellNum = -1;
							if (Fight_ChooseSpell(Team[ BattleOrder[CurMember].TeamNum ]->Member[ BattleOrder[CurMember].MemberNum ],
												  Team[ !BattleOrder[CurMember].TeamNum ], &Move)) {
								Move.Action = acCast;
								DoneMove = TRUE;
							}
							break;
						case '?' :
							GeneralHelp(ST_COMBATHLP);
							break;
						case 'F' :
							Move.Action = acAttack;
							Move.Target = 0;
							FightToDeath = TRUE;
							DoneMove = TRUE;
							break;
						case '#' :
							rputs("\n|07Please enter the number of the enemy to attack.\n%P\n");
							break;
						case 'E' :
							if (Fight_ReadyScroll(Team[ BattleOrder[CurMember].TeamNum ]->Member[ BattleOrder[CurMember].MemberNum ], Team[ !BattleOrder[CurMember].TeamNum ], &Move)) {
								Move.Action = acRead;
								DoneMove = TRUE;
							}
							break;
						case 'D' :  // default action
							// if spell, see if can cast
							if (Team[ BattleOrder[CurMember].TeamNum ]->Member[ BattleOrder[CurMember].MemberNum ]->DefaultAction >= 10) {
								Move.SpellNum = Team[ BattleOrder[CurMember].TeamNum ]->Member[ BattleOrder[CurMember].MemberNum ]->DefaultAction - 10;

								if (Team[ BattleOrder[CurMember].TeamNum ]->Member[ BattleOrder[CurMember].MemberNum ]->SP >=
										Spells[Team[ BattleOrder[CurMember].TeamNum ]->Member[ BattleOrder[CurMember].MemberNum ]->SpellsKnown[Move.SpellNum] - 1]->SP) {
									// spell does exist, do it
									if (Fight_ChooseSpell(Team[ BattleOrder[CurMember].TeamNum ]->Member[ BattleOrder[CurMember].MemberNum ],
														  Team[ !BattleOrder[CurMember].TeamNum ], &Move)) {
										Move.Action = acCast;
										DoneMove = TRUE;
									}
								}
								else {  // do regular attack
									Move.Target = 0;
									Move.Action = acAttack;
									DoneMove = TRUE;
								}
							}
							else {  // do regular attack
								Move.Target = 0;
								Move.Action = acAttack;
								DoneMove = TRUE;
							}
							break;
					}
				}
			}

			if (Fight_DoMove(Team[BattleOrder[CurMember].TeamNum]->Member[ BattleOrder[CurMember].MemberNum ], Move,
							 Team[ !BattleOrder[CurMember].TeamNum ], FightToDeath, CurRound))
				return FT_RAN; // ran away

			// attacker is considered dead if no more living members EXCLUDING undead
			if (Fight_Dead(Attacker))
				return FT_LOST;
			else if (NumMembers(Defender, TRUE) == 0)
				return FT_WON;

			// otherwise, keep fighting
		}
	}

}

void Fight_CheckLevelUp(void)
{
	/* sees if any clansmen need to raise a level.  if so, grant them
	   a training point each */
	char szString[128];
	_INT16 CurMember;
	long XPRequired[MAX_LEVELS];
	_INT16 iTemp, Level, Points;
	BOOL LevelUpFound = FALSE, FirstTime = TRUE;

	/* figure XP required per level */
	for (Level = 1; Level < MAX_LEVELS; Level++) {
		XPRequired[Level] = 50L;

		for (iTemp = 1; iTemp <= Level; iTemp++)
			XPRequired[Level] += ((long)(iTemp-1)*75L);
	}

	/* for each member, see if xp is higher than the xp required for the
	   next level */
	for (CurMember = 0; CurMember < MAX_MEMBERS; CurMember++) {
		/* if so, tell user "blah has raised a level".  raise his level,
		   graint him RANDOM(1) + 1 training points */


		if (PClan->Member[CurMember]) {
			/* see if enough XP */

			if (PClan->Member[CurMember]->Experience >=
					XPRequired[PClan->Member[CurMember]->Level+1]) {
				if (FirstTime)
					rputs("\n");

				FirstTime = FALSE;

				/* Enough XP, raise level and give training point */
				Points = RANDOM(4) + 10;

				PClan->Member[CurMember]->Level++;
				PClan->Member[CurMember]->TrainingPoints += Points;

				// sprintf(szString, "|10>> |15%s |02raises to level |14%d |02and gains %d training points!\n",
				sprintf(szString, ST_LEVELUP, PClan->Member[CurMember]->szName,
						PClan->Member[CurMember]->Level, Points);
				rputs(szString);

				LevelUpFound = TRUE;
			}
		}
	}
	if (LevelUpFound && !PClan->TrainHelp) {
		rputs("\n\n");
		PClan->TrainHelp = TRUE;
		Help("Level Raise", ST_NEWBIEHLP);
	}

	/* tell him (in helpform) that he can go train at a training center */

}


// ------------------------------------------------------------------------- //

_INT16 GetDifficulty(_INT16 Level)
{
	_INT16 Difficulty = 0;

	if (Level <= 2)
		Difficulty = 5 + RANDOM(3);
	else if (Level <= 4)
		Difficulty = 7 + RANDOM(4);
	else if (Level <= 6)
		Difficulty = 20 + RANDOM(10);
	else if (Level <= 10)
		Difficulty = 30 + RANDOM(10);
	else if (Level <= 15)
		Difficulty = 45 + RANDOM(15);
	else if (Level <= 20)
		Difficulty = 90 + RANDOM(30);

	return Difficulty;
}

struct clan *MallocClan(void) {
	struct clan *Clan;
	_INT16 iTemp;

	Clan = malloc(sizeof(struct clan));
	CheckMem(Clan);

	/* NULL-pointer each member of clan for now */
	for (iTemp = 0; iTemp < MAX_MEMBERS; iTemp++)
		Clan->Member[iTemp] = NULL;

	for (iTemp = 0; iTemp < MAX_ITEMS_HELD; iTemp++)
		Clan->Items[iTemp].Available = FALSE;

	Clan->szName[0] = 0;

	return Clan;
}

_INT16 Fight_GetMonster(struct pc *Monster, _INT16 MinDifficulty, _INT16 MaxDifficulty,
						char *szFileName)
{
	_INT16 *MonIndex;
	_INT16 iTemp, ValidMonsters;
	_INT16 *ValidMonIndex, MonsterChosen;
	struct FileHeader FileHeader;
	char szString[128];

	MyOpen(szFileName, "rb", &FileHeader);

	if (!FileHeader.fp) {
		sprintf(szString, "Error:  couldn't open %s!\n", szFileName);
		rputs(szString);
		od_exit(0, FALSE);
	}

	MonIndex =      malloc(sizeof(_INT16) * MAX_MONSTERS);
	ValidMonIndex = malloc(sizeof(_INT16) * MAX_MONSTERS);
	CheckMem(MonIndex);
	CheckMem(ValidMonIndex);

	/* read index */
	fread(MonIndex, sizeof(_INT16) * MAX_MONSTERS, 1, FileHeader.fp);

	/* calculate how many guys there are that are within range */
	ValidMonsters = 0;
	for (iTemp = 0; iTemp < MAX_MONSTERS; iTemp++) {
		if (MonIndex[iTemp] == 0)   /* done looking */
			break;

		if (MonIndex[iTemp] >= MinDifficulty && MonIndex[iTemp] <= MaxDifficulty) {
			ValidMonIndex[ValidMonsters] = iTemp;
			ValidMonsters++;
		}
	}

	if (ValidMonsters == 0) {
		rputs("Error:  No valid monsters!!\n%P");
		free(MonIndex);
		free(ValidMonIndex);
		fclose(FileHeader.fp);
		return -1;
	}

	//od_printf("%d valid monsters found.\n\r", ValidMonsters);

	iTemp = RANDOM(ValidMonsters);
	MonsterChosen = ValidMonIndex[ iTemp ];

	//od_printf("Using #%d in the file.\n\r", MonsterChosen);

	/* fseek there and read it in */
	//   fseek(FileHeader.fp, (long)MonsterChosen * sizeof(struct pc), SEEK_CUR);

	fseek(FileHeader.fp,
		  ((long)sizeof(_INT16) * MAX_MONSTERS + ((long)MonsterChosen * sizeof(struct pc)) + FileHeader.lStart),
		  SEEK_SET);
	fread(Monster, sizeof(struct pc), 1, FileHeader.fp);

	fclose(FileHeader.fp);

	/* randomize stats */
	for (iTemp = 0; iTemp < NUM_ATTRIBUTES; iTemp++) {
		Monster->Attributes[iTemp] = (Monster->Attributes[iTemp] * (RANDOM(60)+80))/100;
	}
	Monster->HP = (Monster->HP* (RANDOM(30)+80))/100;
	Monster->MaxHP = Monster->HP;

	free(MonIndex);
	free(ValidMonIndex);

	return 0;
}



void Fight_LoadMonsters(struct clan *Clan, _INT16 Level, char *szFileName)
{
	_INT16 MinDifficulty, MaxDifficulty, Difficulty, CurDiff, iTemp,
	CurMonster;

	// figure out difficulty value
	Difficulty = GetDifficulty(Level);

	MinDifficulty = Level - 2 - RANDOM(3);
	if (MinDifficulty < 1)
		MinDifficulty = 1;
	MaxDifficulty = Level + 1 + RANDOM(3);
	if (MaxDifficulty > 20)
		MaxDifficulty = 20;

	// keep getting monsters of level(s) we want until totalsum reached
	CurMonster = 0;
	CurDiff = 0;
	while (CurDiff < Difficulty) {
		Clan->Member[CurMonster] = malloc(sizeof(struct pc));
		CheckMem(Clan->Member[CurMonster]);

		if (Fight_GetMonster(Clan->Member[CurMonster], MinDifficulty, MaxDifficulty, szFileName) == -1) {
			free(Clan->Member[CurMonster]);
			Clan->Member[CurMonster] = NULL;
			continue;
		}

		/* set its spells to -1 */
		for (iTemp = 0; iTemp < 10; iTemp++)
			Clan->Member[CurMonster]->SpellsInEffect[iTemp].SpellNum = -1;

		Clan->Member[CurMonster]->MyClan = Clan;

		CurDiff += Clan->Member[CurMonster]->Difficulty;

		CurMonster++;
	}
}

void RemoveUndead(struct clan *Clan)
{
	_INT16 CurMember;

	for (CurMember = 0; CurMember < MAX_MEMBERS; CurMember++) {
		if (Clan->Member[CurMember] && Clan->Member[CurMember]->Undead) {
			free(Clan->Member[CurMember]);
			Clan->Member[CurMember] = NULL;
		}
	}
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

long MineFollowersGained(_INT16 Level)
{
	long NumFollowers = 0;

	if (Level <= 5)
		NumFollowers = 4 + RANDOM(3);
	else if (Level > 5 && Level <= 10)
		NumFollowers = 7 + RANDOM(3);
	else if (Level > 10 && Level <= 15)
		NumFollowers = 11 + RANDOM(3);
	else if (Level > 15 && Level <= 20)
		NumFollowers = 14 + RANDOM(3);

	return NumFollowers;
}


void Fight_GiveFollowers(_INT16 Level)
{
	long NumConscripted, NumFollowers;
	char szString[128];

	NumFollowers = MineFollowersGained(Level);
	NumConscripted = (NumFollowers*Village.Data->ConscriptionRate)/100L;

	if (NumFollowers || NumConscripted) {
		sprintf(szString, ST_FIGHTOVER1, NumFollowers, NumConscripted);
		rputs(szString);
	}

	NumFollowers -= NumConscripted;
	if (NumFollowers < 0)
		NumFollowers = 0;

	Village.Data->Empire.Army.Followers += NumConscripted;
	PClan->Empire.Army.Followers += NumFollowers;

}

_INT16 GetOpenItemSlot(struct clan *Clan)
{
	// return -1 if no more open slots

	_INT16 iTemp;

	// see if he has room to carry it
	for (iTemp = 0; iTemp < MAX_ITEMS_HELD; iTemp++) {
		if (Clan->Items[iTemp].Available == FALSE)
			break;
	}
	if (iTemp == MAX_ITEMS_HELD) {
		/* no more room in inventory */
		return -1;
	}
	else
		return iTemp;
}

void TakeItemsFromClan(struct clan *Clan, char *szMsg)
{
	_INT16 ItemIndex, OneItemFound, EmptySlot;
	_INT16 DefaultItemIndex, iTemp, WhoEquip, ItemsTaken = 0;
	char szString[100];
	BOOL SomethingTaken = FALSE;

	sprintf(szString, "You may take %d items from that clan.\n\r", MAX_ITEMSTAKEN);
	rputs(szString);

	for (;;) {
		rputs(ST_ISTATS0);

		switch (od_get_answer("?XLQT\r\n ")) {
			case '?' :
				rputs("Help\n\n");
				Help("Take Item Help", ST_MENUSHLP);
				break;
			case ' ' :
			case '\r':
			case '\n':
			case 'Q' :
				rputs("Quit\n");
				return;
			case 'T' :
				// take an item
				rputs("Take item\n");

				EmptySlot = GetOpenItemSlot(PClan);
				if (EmptySlot == -1) {
					rputs(ST_ITEMNOMOREROOM);
					break;
				}

				if (ItemsTaken == MAX_ITEMSTAKEN) {
					rputs("You have already taken the maximum number of items.\n");
					break;
				}

				ItemIndex = (_INT16) GetLong("|0STake which item?", 0, MAX_ITEMS_HELD);
				if (ItemIndex == 0)
					break;
				ItemIndex--;

				if (Clan->Items[ItemIndex].Available == FALSE) {
					rputs(ST_INVALIDITEM);
					break;
				}

				// take that item now
				// if it is equipped, stop equipping it
				if (Clan->Items[ItemIndex].UsedBy) {
					switch (Clan->Items[ItemIndex].cType) {
						case I_WEAPON :
							Clan->Member[  Clan->Items[ItemIndex].UsedBy - 1 ]->Weapon = 0;
							break;
						case I_ARMOR :
							Clan->Member[  Clan->Items[ItemIndex].UsedBy - 1 ]->Armor = 0;
							break;
						case I_SHIELD :
							Clan->Member[  Clan->Items[ItemIndex].UsedBy - 1 ]->Shield = 0;
							break;
					}
					Clan->Items[ItemIndex].UsedBy = 0;
				}
				// add it to our items
				PClan->Items[EmptySlot] = Clan->Items[ItemIndex];

				// get rid of it in theirs
				Clan->Items[ItemIndex].Available = FALSE;

				// increment counter
				ItemsTaken++;

				sprintf(szString, "%s taken!\n\r", Clan->Items[ItemIndex].szName);
				rputs(szString);

				// add onto szMsg
				if (SomethingTaken == FALSE) {
					// nothing was taken before this item, means there is no
					// header yet for szMsg
					strcat(szMsg, "\n\n The following items were taken:\n\n");
				}

				SomethingTaken = TRUE;
				sprintf(szString, " %s\n", Clan->Items[ItemIndex].szName);
				strcat(szMsg, szString);
				break;
			case 'X' :  /* examine item */
				rputs(ST_ISTATS1);

				/* see if anything to examine */
				for (iTemp = 0; iTemp < MAX_ITEMS_HELD; iTemp++) {
					if (Clan->Items[iTemp].Available)
						break;
				}
				if (iTemp == MAX_ITEMS_HELD) {
					rputs(ST_ISTATS2);
					break;
				}

				/* find first item in inventory */
				for (iTemp = 0; iTemp < MAX_ITEMS_HELD; iTemp++) {
					if (Clan->Items[iTemp].Available)
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
				if (Clan->Items[ItemIndex].Available == FALSE) {
					rputs(ST_ISTATS4);
					break;
				}
				ShowItemStats(&Clan->Items[ItemIndex], Clan);
				break;
			case 'L' :
				rputs(ST_ISTATS5);
				ListItems(Clan);
				break;
		}
	}
	(void)WhoEquip;
	(void)OneItemFound;
}



void Fight_Clan(void)
{
	struct clan *EnemyClan;
	_INT16 ClanID[2], FightResult;
	_INT16 iTemp, TotalDifficulty, CurMember;
	char szString[128], *szMessage;

	/* if all guys dead, tell guy can't fight */
	if (NumMembers(PClan, TRUE) == 0) {
		rputs(ST_FIGHT0);
		return;
	}

	rputs("|07Clans with Living Members:\n");
	if (GetClanID(ClanID, TRUE, FALSE, -1, FALSE) == FALSE) {
		return;
	}

	/* see if already fought him today */
	for (iTemp = 0; iTemp < MAX_CLANCOMBAT; iTemp++) {
		if (PClan->ClanCombatToday[iTemp][0] == ClanID[0] &&
				PClan->ClanCombatToday[iTemp][1] == ClanID[1]) {
			rputs(ST_FIGHTCLANALREADY);
			return;
		}
	}

	EnemyClan = MallocClan();

	GetClan(ClanID, EnemyClan);

	// if enemy clan is "away", don't fight
	if (EnemyClan->WorldStatus == WS_GONE) {
		rputs("|07That clan is currently not in town.\n%P");
		FreeClan(EnemyClan);
		return;
	}

	// if enemy clan is all "dead", don't fight
	if (NumMembers(EnemyClan, TRUE) == 0) {
		rputs("|07That clan currently has no living members.  You cannot fight them.\n%P");
		FreeClan(EnemyClan);
		return;
	}

	/* record fight so he can't do it again */
	for (iTemp = 0; iTemp < MAX_CLANCOMBAT; iTemp++) {
		if (PClan->ClanCombatToday[iTemp][0] == -1) {
			break;
		}
	}
	PClan->ClanCombatToday[iTemp][0] = ClanID[0];
	PClan->ClanCombatToday[iTemp][1] = ClanID[1];

	PClan->ClanFights--;

	/* total difficulty for later on */
	TotalDifficulty = 0;
	for (iTemp = 0; iTemp < MAX_MEMBERS; iTemp++) {
		if (EnemyClan->Member[iTemp])
			TotalDifficulty += EnemyClan->Member[iTemp]->Level;
	}

	/* set all spells to 0 */
	Spells_ClearSpells(PClan);
	Spells_ClearSpells(EnemyClan);

	/* fight 'em here using the function for fighting */
	FightResult = Fight_Fight(PClan, EnemyClan, TRUE, FALSE, FALSE);

	/* set all spells to 0 */
	Spells_ClearSpells(PClan);
	Spells_ClearSpells(EnemyClan);

	// reset their HPs if they are alive
	Fight_Heal(PClan);
	Fight_Heal(EnemyClan);

	RemoveUndead(PClan);
	RemoveUndead(EnemyClan);

	/* do stuff because of win/loss */

	/* figure out number of followers from Difficulty of battle */
	if (FightResult == FT_WON) {
		/* put in news you won */
		sprintf(szString, ST_NEWSFIGHT1, EnemyClan->szName, PClan->szName);
		News_AddNews(szString);

		/* tell that guy about loss */
		szMessage = MakeStr(500);
		sprintf(szMessage, ST_NEWSFIGHT2, PClan->szName);

		// allow user to take an item
		TakeItemsFromClan(EnemyClan, szMessage);

		GenericMessage(szMessage, EnemyClan->ClanID, PClan->ClanID, PClan->szName, FALSE);
		free(szMessage);

		/* give points for win in battle */
		PClan->Points += 20;
	}
	else if (FightResult == FT_LOST) {
		/* put in news you lost */
		sprintf(szString, ST_NEWSFIGHT3, PClan->szName, EnemyClan->szName);
		News_AddNews(szString);

		/* tell that guy about loss */
		szMessage = MakeStr(500);
		sprintf(szMessage, ST_NEWSFIGHT4, PClan->szName);
		GenericMessage(szMessage, EnemyClan->ClanID, PClan->ClanID, PClan->szName, FALSE);
		free(szMessage);

		PClan->Points -= 15;
	}
	else {
		// ran away
		/* put in news you lost */
		sprintf(szString, ST_NEWSFIGHT5, PClan->szName, EnemyClan->szName);
		News_AddNews(szString);

		/* tell that guy about loss */
		sprintf(szString, ST_NEWSFIGHT6, PClan->szName);
		GenericMessage(szString, EnemyClan->ClanID, PClan->ClanID, PClan->szName, FALSE);

		PClan->Points -= 20;
	}

	Clan_Update(EnemyClan);

	/* get result of fight and do stuff */
	FreeClan(EnemyClan);

	/* fix it so they didn't run away */
	for (CurMember = 0; CurMember < MAX_MEMBERS; CurMember++) {
		if (PClan->Member[CurMember] && PClan->Member[CurMember]->Status == RanAway)
			PClan->Member[CurMember]->Status = Here;
	}

	Fight_CheckLevelUp();
	door_pause();
}

void Fight_Monster(_INT16 Level, char *szFileName)
/*
 * Fights monster in the mines
 *
 */
{
	unsigned long MemBefore;
	_INT16 CurMember, FightResult;
	struct clan *EnemyClan;


#ifdef PRELAB
	od_printf("@@Mem: %lu\n\r", MemBefore = farcoreleft());

	rputs("Cheat ON\n");
	for (CurMember = 0; CurMember < MAX_MEMBERS; CurMember++)
		if (PClan->Member[CurMember]) {
			PClan->Member[CurMember]->HP = PClan->Member[CurMember]->MaxHP;
			PClan->Member[CurMember]->Status = Here;
		}

#endif

	if (!PClan->CombatHelp) {
		PClan->CombatHelp = TRUE;
		Help("Combat", ST_NEWBIEHLP);
	}

	// reduce # of fights left
	PClan->FightsLeft--;

	// load monsters into enemy clan

	EnemyClan = MallocClan();
	Fight_LoadMonsters(EnemyClan, Level, szFileName);


	od_clr_scr();
	FightResult = Fight_Fight(PClan, EnemyClan, FALSE, TRUE, FALSE);

	Fight_Heal(PClan);
	Spells_ClearSpells(PClan);

	RemoveUndead(PClan);

	FreeClan(EnemyClan);

	if (FightResult == FT_WON) {
		Fight_GiveFollowers(Level);

		PClan->Points += 10;
	}
	else {
		// lose points -- same for running AND losing
		PClan->Points -= 10;
	}

	// fix up status so players aren't set as running away
	for (CurMember = 0; CurMember < MAX_MEMBERS; CurMember++) {
		if (PClan->Member[CurMember] &&
				PClan->Member[CurMember]->Status == RanAway) {
			PClan->Member[CurMember]->Status = Here;
		}
	}

	Fight_CheckLevelUp();

#ifdef PRELAB
	od_printf("@@Mem: %lu, %lu\n\r", MemBefore, farcoreleft());
#endif
	(void)MemBefore;
}
