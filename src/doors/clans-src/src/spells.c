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

#include "system.h"
#include "structs.h"
#include "door.h"
#include "myopen.h"
#include "language.h"
#include "mstrings.h"
#include "user.h"
#include "video.h"

BOOL SpellsInitialized = FALSE;
struct Spell *Spells[MAX_SPELLS];
extern struct IniFile IniFile;
extern struct clan *PClan;
extern struct Language *Language;
extern struct village Village;

char Spells_szCastDestination[25];
char Spells_szCastSource[25];
int Spells_CastValue;
extern BOOL Verbose;


// ------------------------------------------------------------------------- //

/* Simple and more protective function to handle string loading in
   Spells_Init()
*/
char * get_spell(char **dest, FILE *fp)
{
	_INT16 StringLength = 0;

	if (!fread(&StringLength, sizeof(_INT16), 1, fp))
		System_Error("fread failed in get_spell() [StringLength]");
	else if (StringLength) {
		*dest = (char *) malloc(StringLength);
		CheckMem(*dest);
		if (!fread(*dest, StringLength, 1, fp))
			System_Error("fread failed in get_spell() [String Read]");
	}
	return (*dest);
}

void Spells_Init(void)
/*
 * This function loads spells from file.
 */
{
	_INT16 iTemp, NumSpells;
	_INT16 CurFile, CurSpell = 0;
	struct FileHeader SpellFile;

	if (Verbose) {
		DisplayStr("> Spells_Init()\n");
		delay(500);
	}

	SpellsInitialized = TRUE;

	// for each file, read in the data
	for (CurFile = 0; CurFile < MAX_SPELLFILES; CurFile++) {
		if (IniFile.pszSpells[CurFile] == NULL)
			break;

		// open file if possible
		MyOpen(IniFile.pszSpells[CurFile], "rb", &SpellFile);

		if (SpellFile.fp == NULL) continue;

		// read in data

		/* get num spells */
		fread(&NumSpells, sizeof(_INT16), 1, SpellFile.fp);

		/* read them in */
		for (iTemp = 0; iTemp < NumSpells; iTemp++) {
			Spells[CurSpell] = malloc(sizeof(struct Spell));
			CheckMem(Spells[CurSpell]);

			fread(Spells[CurSpell], sizeof(struct Spell), 1, SpellFile.fp);

			Spells[CurSpell]->pszHealStr = NULL;
			Spells[CurSpell]->pszUndeadName = NULL;
			Spells[CurSpell]->pszDamageStr = NULL;
			Spells[CurSpell]->pszModifyStr = NULL;
			Spells[CurSpell]->pszWearoffStr = NULL;
			Spells[CurSpell]->pszStatusStr = NULL;
			Spells[CurSpell]->pszOtherStr = NULL;

			/* get strings */
			get_spell(&Spells[CurSpell]->pszDamageStr, SpellFile.fp);
			get_spell(&Spells[CurSpell]->pszHealStr, SpellFile.fp);
			get_spell(&Spells[CurSpell]->pszModifyStr, SpellFile.fp);
			get_spell(&Spells[CurSpell]->pszWearoffStr, SpellFile.fp);
			get_spell(&Spells[CurSpell]->pszStatusStr, SpellFile.fp);
			get_spell(&Spells[CurSpell]->pszOtherStr, SpellFile.fp);
			get_spell(&Spells[CurSpell]->pszUndeadName, SpellFile.fp);
			CurSpell++;
			if (CurSpell == MAX_SPELLS)   break;
		}
		fclose(SpellFile.fp);

		if (CurSpell == MAX_SPELLS)   break;
	}

	if (Verbose) {
		DisplayStr("> Spells_Init done()\n");
		delay(500);
	}

}

// ------------------------------------------------------------------------- //

void Spells_Close(void)
/*
 * This function frees any mem initialized by Spells_Init.
 */
{
	_INT16 iTemp;

	if (SpellsInitialized == FALSE) return;

	for (iTemp = 0; iTemp < MAX_SPELLS; iTemp++) {
		if (Spells[iTemp]) {
			/* free strings first */
			if (Spells[iTemp]->pszHealStr)
				free(Spells[iTemp]->pszHealStr);
			if (Spells[iTemp]->pszUndeadName)
				free(Spells[iTemp]->pszUndeadName);
			if (Spells[iTemp]->pszDamageStr)
				free(Spells[iTemp]->pszDamageStr);
			if (Spells[iTemp]->pszModifyStr)
				free(Spells[iTemp]->pszModifyStr);
			if (Spells[iTemp]->pszWearoffStr)
				free(Spells[iTemp]->pszWearoffStr);
			if (Spells[iTemp]->pszStatusStr)
				free(Spells[iTemp]->pszStatusStr);
			if (Spells[iTemp]->pszOtherStr)
				free(Spells[iTemp]->pszOtherStr);

			free(Spells[iTemp]);
			Spells[iTemp] = NULL;
		}
	}

	SpellsInitialized = FALSE;
}


// ------------------------------------------------------------------------- //
void Spells_UpdatePCSpells(struct pc *PC)
{
	_INT16 iTemp;

	/* set up %SD so it shows this dude */
	strcpy(Spells_szCastDestination, PC->szName);

	/* reduce each spell in effect accordingly */
	for (iTemp = 0; iTemp < 10; iTemp++) {
		/* skip if no spell in that slot */
		if (PC->SpellsInEffect[iTemp].SpellNum == -1)
			continue;

		/* reduce by 10 this round */
		PC->SpellsInEffect[iTemp].Energy -= 10;

		/* if strength can reduce this, reduce it */
		if (Spells[ PC->SpellsInEffect[iTemp].SpellNum ]->Friendly == FALSE &&
				Spells[ PC->SpellsInEffect[iTemp].SpellNum ]->StrengthCanReduce) {
			PC->SpellsInEffect[iTemp].Energy -= (GetStat(PC, ATTR_STRENGTH)/2);
		}
		/* if wisdom can reduce this, reduce it */
		if (Spells[ PC->SpellsInEffect[iTemp].SpellNum ]->Friendly == FALSE &&
				Spells[ PC->SpellsInEffect[iTemp].SpellNum ]->WisdomCanReduce) {
			PC->SpellsInEffect[iTemp].Energy -= (GetStat(PC, ATTR_WISDOM)/2);
		}

		/* check wisdom variables + Level var
		   -- this increases chance of spell wearing off */

		/* see if spell is dead */
		if (PC->SpellsInEffect[iTemp].Energy <= 0) {
			/* if so, display ending string */
			rputs(Spells[ PC->SpellsInEffect[iTemp].SpellNum ]->pszWearoffStr);

			/* kill spell */
			PC->SpellsInEffect[iTemp].Energy = 0;
			PC->SpellsInEffect[iTemp].SpellNum = -1;
		}
	}
}

void Spells_ClearSpells(struct clan *Clan)
{
	_INT16 CurMember, iTemp;

	for (CurMember = 0; CurMember < MAX_MEMBERS; CurMember++) {
		if (Clan->Member[CurMember])
			for (iTemp = 0; iTemp < 10; iTemp++)
				Clan->Member[CurMember]->SpellsInEffect[iTemp].SpellNum = -1;
	}
}


void Spells_CastSpell(struct pc *PC, struct clan *EnemyClan, _INT16 Target, _INT16 SpellNum)
{
	BOOL Test = FALSE;
	_INT16 Damage, Value, HPIncrease, OldHP, Level;
	char szString[128];
	struct pc *TargetPC;
	long XPGained, GoldGained, TaxedGold;
	_INT16 iTemp, NumUndead, NumUndeadToRemove, CurSlot, iTemp2/*, PercentGold*/;
	_INT16 NumUndeadRemoved;

	/* see if spell cast was successful, if not, return */

	/* do stuff according to type of spell it is */
	// od_printf("Spell chosen was %d\n\r", SpellNum);

	/* set up global vars */
	strcpy(Spells_szCastSource, PC->szName);

	if (Spells[SpellNum]->Friendly == FALSE) {
		if (Spells[SpellNum]->Target)
			strcpy(Spells_szCastDestination, EnemyClan->Member[Target]->szName);
		else
			strcpy(Spells_szCastDestination, EnemyClan->szName);

		TargetPC = EnemyClan->Member[Target];
	}
	else {
		if (Spells[SpellNum]->Target)
			strcpy(Spells_szCastDestination, PC->MyClan->Member[Target]->szName);
		else
			strcpy(Spells_szCastDestination, PC->MyClan->szName);

		TargetPC = PC->MyClan->Member[Target];
	}

	if (Spells[SpellNum]->TypeFlag & SF_BANISHUNDEAD) {
		if (Test)
			printf("\aError!\n");
		else
			Test = TRUE;
		/* see if spell is successful */
		if ((PC->Level + 3 + Spells[SpellNum]->TypeFlag + GetStat(PC, ATTR_WISDOM) +
				RANDOM(5)) < RANDOM(15)) {
			sprintf(szString, ST_SPELLFAIL, PC->szName, Spells[SpellNum]->szName);
			rputs(szString);
			return;
		}

		/* banish undead warriors from this realm */

		/* see how many undead the enemy has */
		NumUndead = 0;
		for (iTemp = 0; iTemp < MAX_MEMBERS; iTemp++) {
			if (EnemyClan->Member[iTemp] &&
					EnemyClan->Member[iTemp]->Undead &&
					EnemyClan->Member[iTemp]->Status == Here)
				NumUndead++;
		}

		if (NumUndead == 0) {
			// no undead to banish
			rputs(ST_SPELLNOUNDEAD);
			return;
		}

		/* see how many undead to banish */
		NumUndeadToRemove = RANDOM(4) + 1;

		/* if too many to remove, truncate size */
		if (NumUndeadToRemove > NumUndead)
			NumUndeadToRemove = NumUndead;

		/* output of spell */
		rputs(Spells[SpellNum]->pszOtherStr);

		/* figure out XP gained */
		XPGained = NumUndeadToRemove * 4;
		sprintf(szString, ST_FIGHTXP, XPGained);
		rputs(szString);
		PC->Experience += XPGained;
		rputs("\n\n");

		/* now remove 'em */
		NumUndeadRemoved = 0;
		while (NumUndeadRemoved < NumUndeadToRemove) {
			/* go through player file till found an undead character */
			/* remove him */

			/* find open slot to use */
			for (CurSlot = 0; CurSlot < MAX_MEMBERS; CurSlot++) {
				if (EnemyClan->Member[CurSlot] &&
						EnemyClan->Member[CurSlot]->Undead &&
						EnemyClan->Member[CurSlot]->Status == Here)
					break;
			}

			if (CurSlot == MAX_MEMBERS)
				break;

			sprintf(szString, "|04*** |12%s is banished!\n\n",
					EnemyClan->Member[CurSlot]->szName);
			rputs(szString);

			/* found undead, remove him */
			EnemyClan->Member[CurSlot]->Status = Dead;
			EnemyClan->Member[CurSlot]->HP = 0;
			NumUndeadRemoved++;
		}
	}

	if (Spells[SpellNum]->TypeFlag & SF_RAISEUNDEAD) {
		if (Test)
			printf("\aError!\n");
		else
			Test = TRUE;
		/* see if spell is successful */
		if ((PC->Level + Spells[SpellNum]->Level + GetStat(PC, ATTR_WISDOM) + RANDOM(6)) <
				RANDOM(15)) {
			sprintf(szString, ST_SPELLFAIL, PC->szName, Spells[SpellNum]->szName);
			rputs(szString);
			return;
		}

		/* raise undead */

		/* see how many undead to raise */
		if (Spells[SpellNum]->Value == 0)
			NumUndead = RANDOM(2 + PC->Level/4) + 1;
		else
			NumUndead = Spells[SpellNum]->Value;

		//od_printf("Tried raising %d undead\n\r", NumUndead);

		if (NumUndead > 4)
			NumUndead = 4;

		/* now make 'em */
		for (iTemp = 0; iTemp < NumUndead; iTemp++) {
			/* find open slot to use */
			for (CurSlot = 0; CurSlot < MAX_MEMBERS; CurSlot++) {
				if (PC->MyClan->Member[CurSlot] == NULL)
					break;
			}

			if (CurSlot == MAX_MEMBERS) {
				sprintf(szString, ST_SPELLFAIL, PC->szName, Spells[SpellNum]->szName);
				rputs(szString);
				return;
			}

			//od_printf("Using %d as slot\n\r", CurSlot);

			/* found slot, fill it up */
			PC->MyClan->Member[CurSlot] = (struct pc *) malloc(sizeof(struct pc));
			CheckMem(PC->MyClan->Member[CurSlot]);

			strcpy(PC->MyClan->Member[CurSlot]->szName, Spells[SpellNum]->pszUndeadName);
			PC->MyClan->Member[CurSlot]->Undead = TRUE;
			PC->MyClan->Member[CurSlot]->Status = Here;

			PC->MyClan->Member[CurSlot]->MaxHP =
				PC->MyClan->Member[CurSlot]->HP = RANDOM(10) + 5 + PC->Level;

			PC->MyClan->Member[CurSlot]->MaxSP =
				PC->MyClan->Member[CurSlot]->SP = 0;

			for (iTemp2 = 0; iTemp2 < NUM_ATTRIBUTES; iTemp2++) {
				/* if attributes set, use them, otherwise, use caster's */
				if (Spells[SpellNum]->Attributes[iTemp2])
					PC->MyClan->Member[CurSlot]->Attributes[iTemp2] =
						Spells[SpellNum]->Attributes[iTemp2] + RANDOM(1);
				else {
					PC->MyClan->Member[CurSlot]->Attributes[iTemp2] =
						PC->Attributes[iTemp2]  - 2 + RANDOM(4);
				}
			}

			PC->MyClan->Member[CurSlot]->Weapon = 0;
			PC->MyClan->Member[CurSlot]->Armor = 0;
			PC->MyClan->Member[CurSlot]->Shield = 0;

			PC->MyClan->Member[CurSlot]->WhichRace = -1;
			PC->MyClan->Member[CurSlot]->WhichClass = 0;
			PC->MyClan->Member[CurSlot]->Experience = 0;
			PC->MyClan->Member[CurSlot]->Level = 0;

			PC->MyClan->Member[CurSlot]->MyClan = PC->MyClan;

			for (iTemp2 = 0; iTemp2 < MAX_SPELLS; iTemp2++)
				PC->MyClan->Member[CurSlot]->SpellsKnown[iTemp2] = 0;

			for (iTemp2 = 0; iTemp2 < 10; iTemp2++)
				PC->MyClan->Member[CurSlot]->SpellsInEffect[iTemp2].SpellNum = -1;

			PC->MyClan->Member[CurSlot]->Difficulty = 1;
		}

		/* output of spell */
		rputs(Spells[SpellNum]->pszOtherStr);

		/* figure out XP gained */
		XPGained = NumUndead * 2;
		sprintf(szString, ST_FIGHTXP, XPGained);
		rputs(szString);
		PC->Experience += XPGained;
		rputs("\n\n");
	}

	if (Spells[SpellNum]->TypeFlag & SF_HEAL) {
		if (Test)
			printf("\aError!\n");
		else
			Test = TRUE;
		/* heal him */

		OldHP = PC->MyClan->Member[Target]->HP;

		if (PC->Level > 10)
			Level = 10;
		else
			Level = PC->Level;

		Value = (((Spells[SpellNum]->Value + Level/2)*(RANDOM(50)+50))/100);

		PC->MyClan->Member[Target]->HP += Value;
		if (PC->MyClan->Member[Target]->HP > PC->MyClan->Member[Target]->MaxHP)
			PC->MyClan->Member[Target]->HP = PC->MyClan->Member[Target]->MaxHP;

		HPIncrease = PC->MyClan->Member[Target]->HP - OldHP;
		Spells_CastValue = HPIncrease;

		/* output of spell */
		rputs(Spells[SpellNum]->pszHealStr);

		/* figure out XP gained */
		XPGained = HPIncrease/3;
		sprintf(szString, ST_FIGHTXP, XPGained);
		rputs(szString);
		PC->Experience += XPGained;

		rputs("\n\n");
	}

	if (((Spells[SpellNum]->TypeFlag & SF_MODIFY) ||
			(Spells[SpellNum]->TypeFlag & SF_INCAPACITATE))) {
		if (Test)
			printf("\aError!\n");
		else
			Test = TRUE;
		/* see if spell is successful */
		if (Spells[SpellNum]->Friendly) {
			if ((PC->Level + Spells[SpellNum]->Level + GetStat(PC, ATTR_WISDOM) +
					RANDOM(12)) < (RANDOM(12))) {
				sprintf(szString, ST_SPELLFAIL, PC->szName, Spells[SpellNum]->szName);
				rputs(szString);
				return;
			}
		}
		else {
			if ((PC->Level + GetStat(PC, ATTR_WISDOM) + RANDOM(8) + Spells[SpellNum]->Level) <
					(TargetPC->Level + GetStat(TargetPC, ATTR_WISDOM) + RANDOM(4))) {
				sprintf(szString, ST_SPELLFAIL, PC->szName,
						Spells[SpellNum]->szName);
				rputs(szString);
				return;
			}
		}

		/* see if spell is successfully cast */

		/* put in SpellsInEffect */

		/* see if spell in effect already, if so, just add onto energy */
		for (iTemp = 0; iTemp < 10; iTemp++) {
			if (TargetPC->SpellsInEffect[iTemp].SpellNum == SpellNum) {
				/* found one which is same as this */
				break;
			}
		}

		if (iTemp != 10) {
			/* means it broke; early so just add onto the Energy */
			TargetPC->SpellsInEffect[iTemp].Energy += Spells[SpellNum]->Energy;
		}
		else {
			/* find empty slot, if none found just use #9*/
			for (iTemp = 0; iTemp < 10; iTemp++) {
				if (TargetPC->SpellsInEffect[iTemp].SpellNum == -1)
					break;
			}

			if (iTemp == 10)
				iTemp = 9;

			/* now set spell */
			TargetPC->SpellsInEffect[iTemp].SpellNum = SpellNum;
			TargetPC->SpellsInEffect[iTemp].Energy = Spells[SpellNum]->Energy + (GetStat(PC, ATTR_WISDOM)*4) + PC->Level*2;
		}

		/* output of spell */
		rputs(Spells[SpellNum]->pszModifyStr);

		/* figure out XP gained */
		XPGained = 3;
		sprintf(szString, ST_FIGHTXP, XPGained);
		rputs(szString);
		PC->Experience += XPGained;
		rputs("\n\n");
	}

	if (Spells[SpellNum]->TypeFlag & SF_DAMAGE) {
		if (Test)
			printf("\aError!\n");
		else
			Test = TRUE;
		// if is multiaffect spell, go through each guy

		// if multiaffect, start from first enemy
		if (Spells[SpellNum]->MultiAffect)
			Target = 0;

		// the following for loop incorporates both single damage spells
		// and multi damage, it can be a bit tricky!

		for (; Target < MAX_MEMBERS; Target++) {
			if (Spells[SpellNum]->MultiAffect) {
				// skip this dude if not alive or here
				if (EnemyClan->Member[Target] == NULL || EnemyClan->Member[Target]->Status != Here) {
					continue;
				}

				TargetPC = EnemyClan->Member[Target];

				strcpy(Spells_szCastDestination, EnemyClan->Member[Target]->szName);
			}
			else {
				// not multiaffect, so end spell now since target no longer exists
				if (EnemyClan->Member[Target] == NULL || EnemyClan->Member[Target]->Status != Here) {
					break;
				}
			}

			if ((PC->Level + GetStat(PC, ATTR_WISDOM) + RANDOM(10) + 8 + Spells[SpellNum]->Level) <
					(TargetPC->Level + GetStat(TargetPC, ATTR_WISDOM) + RANDOM(6))) {
				// failed attack

				sprintf(szString, ST_SPELLFAIL, PC->szName, Spells[SpellNum]->szName);
				rputs(szString);

				// if this ain't a multihit spell, break from for loop now
				if (Spells[SpellNum]->MultiAffect == FALSE)
					break;
				else
					continue;
			}

			/* damage him */
			Level = PC->Level;
			if (Level > 10)
				Level = 10;

			Damage = ((Spells[SpellNum]->Value+(Level+GetStat(PC, ATTR_WISDOM))/3)*(RANDOM(60) + 50))/100
					 - GetStat(TargetPC, ATTR_ARMORSTR);
			Spells_CastValue = Damage;

			if (Damage <= 0) {
				//rputs(Spells[SpellNum]->pszDamageStr);
				//rputs("\n");

				sprintf(szString, ST_SPELLFAIL, PC->szName,
						Spells[SpellNum]->szName);
				rputs(szString);
			}
			else {
				rputs(Spells[SpellNum]->pszDamageStr);

				/* experience goes here */
				XPGained = Damage/3 + 1;
				sprintf(szString, ST_FIGHTXP, XPGained);
				rputs(szString);
				PC->Experience += XPGained;

				EnemyClan->Member[Target]->HP -= Damage;

				rputs("\n\n");
			}

			if (EnemyClan->Member[Target]->HP <= 0) {
				//od_printf("in cspells\n\r");

				/* according to how bad the hit was, figure out status */
				if (EnemyClan->szName[0] == 0) {
					EnemyClan->Member[Target]->Status = Dead;
					sprintf(szString, ST_FIGHTKILLED, EnemyClan->Member[Target]->szName,
							EnemyClan->Member[Target]->Difficulty);

					/* give xp because of death */
					PC->Experience += (EnemyClan->Member[Target]->Difficulty);
				}
				else if (EnemyClan->Member[Target]->HP < -15) {
					EnemyClan->Member[Target]->Status = Dead;
					sprintf(szString, ST_FIGHTKILLED, EnemyClan->Member[Target]->szName,
							EnemyClan->Member[Target]->Level*2);

					/* loses percentage of MaxHP */
					EnemyClan->Member[Target]->MaxHP = (EnemyClan->Member[Target]->MaxHP * (RANDOM(10)+90))/100;

					/* give xp because of death */
					PC->Experience += (EnemyClan->Member[Target]->Level*2);
				}
				else if (EnemyClan->Member[Target]->HP < -5) {
					EnemyClan->Member[Target]->Status = Unconscious;
					sprintf(szString, ST_FIGHTMORTALWOUND, EnemyClan->Member[Target]->szName,
							EnemyClan->Member[Target]->Level);

					/* loses percentage of MaxHP */
					EnemyClan->Member[Target]->MaxHP = (EnemyClan->Member[Target]->MaxHP * (RANDOM(10)+90))/100;

					PC->Experience += (EnemyClan->Member[Target]->Level);
				}
				else {
					EnemyClan->Member[Target]->Status = Unconscious;
					sprintf(szString, ST_FIGHTKNOCKEDOUT, EnemyClan->Member[Target]->szName,
							EnemyClan->Member[Target]->Level);

					PC->Experience += (EnemyClan->Member[Target]->Level);
				}
				rputs(szString);

				/* give gold to clan */
				if (PC->MyClan == PClan) {
					if (EnemyClan->Member[Target]->Difficulty != -1) {
						GoldGained = EnemyClan->Member[Target]->Difficulty*((long)RANDOM(10) + 20L) + 50L + (long)RANDOM(20);
						sprintf(szString, ST_FIGHTGETGOLD, GoldGained);
						rputs(szString);

						/* take some away due to taxes */
						if (GoldGained > 0) {
							TaxedGold = (long)(GoldGained * Village.Data->TaxRate)/100L;
							if (TaxedGold) {
								sprintf(szString, ST_FIGHTTAXEDGOLD, TaxedGold);
								rputs(szString);
							}

							PClan->Empire.VaultGold += (GoldGained-TaxedGold);
							Village.Data->Empire.VaultGold += TaxedGold;
						}
					}
				}

				// if that character was an undead dude, free him up
				if (EnemyClan->Member[Target]->Undead) {
					// BUGFIX:
					free(EnemyClan->Member[Target]);
					EnemyClan->Member[Target] = NULL;
				}
			}

			// if this ain't a multihit spell, break from for loop now
			if (Spells[SpellNum]->MultiAffect == FALSE)
				break;
		}   // end of for()

	}
}
