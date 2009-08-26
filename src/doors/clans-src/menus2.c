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
#include <ctype.h>
#ifdef __unix__
#include "unix_wrappers.h"
#endif

#include <OpenDoor.h>
#include "structs.h"
#include "language.h"
#include "mstrings.h"
#include "user.h"
#include "help.h"
#include "door.h"
#include "input.h"
#include "reg.h"

extern struct Language *Language;
extern struct village Village;
extern struct clan *PClan;
extern struct game Game;
extern struct config *Config;

void ResurrectDead(BOOL Unconscious)
{
	_INT16 NumDead = 0, iTemp, WhichOne, MaxRes;
	long Cost;
	char szString[128], cInput;

	// if reached max..
	if (Unconscious) {
		MaxRes = Village.Data->ChurchLevel + 1;
		if (PClan->ResUncToday == MaxRes) {
			rputs("|07This level of church cannot revive any more unconscious today.\n%P");
			return;
		}
	}
	else {
		MaxRes = Village.Data->ChurchLevel;
		if (PClan->ResDeadToday == MaxRes) {
			rputs("|07This level of church cannot raise any more dead today.\n%P");
			return;
		}
	}


	/* if nobody dead in party, tell him */
	for (iTemp = 0; iTemp < MAX_MEMBERS; iTemp++) {
		if (PClan->Member[iTemp]) {
			if (Unconscious == FALSE &&
					PClan->Member[iTemp]->Status == Dead)
				NumDead++;
			else if (Unconscious == TRUE &&
					 PClan->Member[iTemp]->Status == Unconscious)
				NumDead++;
		}
	}
	if (NumDead == 0) {
		if (Unconscious == FALSE)
			rputs("\n|07No members of your party need resurrection from the dead.\n%P");
		else
			rputs("\n|07No members of your party are unconscious.\n%P");
		return;
	}

	/* if nobody dead in party, tell him */
	rputs(ST_LONGLINE);
	for (iTemp = 0; iTemp < MAX_MEMBERS; iTemp++) {
		if (PClan->Member[iTemp] &&
				PClan->Member[iTemp]->Status == Dead && Unconscious == FALSE) {
			sprintf(szString, " |0A(|0B%c|0A) |0C%s\n", iTemp + 'A', PClan->Member[iTemp]->szName);
			rputs(szString);
		}
		else if (PClan->Member[iTemp] &&
				 PClan->Member[iTemp]->Status == Unconscious && Unconscious == TRUE) {
			sprintf(szString, " |0A(|0B%c|0A) |0C%s\n", iTemp + 'A', PClan->Member[iTemp]->szName);
			rputs(szString);
		}
	}
	rputs(" |0A(|0BQ|0A) |0CAbort\n");

	rputs(ST_LONGLINE);
	rputs(" |0AWhich to resurrect? [|0BEnter=Abort|0A] : |15");
	cInput = toupper(od_get_key(TRUE));

	if (cInput == 'Q' || cInput == '\r' || cInput == '\n') {
		rputs(ST_ABORTED);
		return;
	}

	WhichOne = cInput - 'A';

	sprintf(szString, "%c\n", cInput);
	rputs(szString);

	/* if that dude is alive, tell them */
	if (PClan->Member[ WhichOne ] == NULL ||
			(cInput-'A') > MAX_MEMBERS || (cInput-'A') < 0) {
		rputs("|12Member not found.\n%P");
		return;
	}

	if (!Unconscious && PClan->Member[ WhichOne ]->Status != Dead) {
		rputs("|12That member isn't dead!\n%P");
		return;
	}
	if (Unconscious && PClan->Member[ WhichOne ]->Status != Unconscious) {
		rputs("|12That member isn't unconscious!\n%P");
		return;
	}

	/* else tell them how much it costs */
	if (Unconscious == FALSE)
		Cost = ((long)PClan->Member[ WhichOne ]->Level + 1L) * 750L;
	else
		Cost = ((long)PClan->Member[ WhichOne ]->Level + 1L) * 300L;

	sprintf(szString, "\n|0CIt will cost you %ld gold.\n", Cost);
	rputs(szString);

	/* if not enough dough, say go away */
	if (PClan->Empire.VaultGold < Cost) {
		rputs("|12You cannot afford it!\n%P");
		return;
	}

	/* if enough, ask NoYes if wish to revive */
	sprintf(szString, "|0CAre you sure you wish to resurrect |0B%s|0C?",
			PClan->Member[WhichOne]->szName);

	if (NoYes(szString) == YES) {
		/* if so, revive stats et al */

		PClan->Member[ WhichOne ]->HP = PClan->Member[ WhichOne ]->MaxHP;
		PClan->Member[ WhichOne ]->SP = PClan->Member[ WhichOne ]->MaxSP;
		PClan->Member[ WhichOne ]->Status = Here;

		PClan->Empire.VaultGold -= Cost;

		if (Unconscious == FALSE)
			sprintf(szString, "%s has been resurrected from the dead!\n%%P",
					PClan->Member[ WhichOne ]->szName);
		else
			sprintf(szString, "%s has been revived!\n%%P",
					PClan->Member[ WhichOne ]->szName);
		rputs(szString);

		if (Unconscious)
			PClan->ResUncToday++;
		else
			PClan->ResDeadToday++;
	}
}


void ReleaseMember(void)
{
	/* list members */
	/* see which one he wants */
	/* confirm release of member */

	_INT16 iTemp, WhichOne, Choice;
	long Cost;
	char szString[128], cInput;

	rputs(ST_LONGLINE);
	for (iTemp = 0; iTemp < MAX_MEMBERS; iTemp++) {
		if (PClan->Member[iTemp]) {
			sprintf(szString, " |0A(|0B%c|0A) |0C%-15s ",
					iTemp + 'A', PClan->Member[iTemp]->szName);

			if (PClan->Member[iTemp]->Status == Here)
				strcat(szString, "|14(alive)\n");
			else if (PClan->Member[iTemp]->Status == Unconscious)
				strcat(szString, "|12(unconscious)\n");
			else if (PClan->Member[iTemp]->Status == Dead)
				strcat(szString, "|04(dead)\n");

			rputs(szString);
		}
	}
	rputs(" |0A(|0BQ|0A) |0CAbort\n");

	rputs(ST_LONGLINE);
	rputs(" |0AWhich to release? [|0BEnter=abort|0A] : |0F");

	for (;;) {
		cInput = toupper(od_get_key(TRUE));

		if (cInput == 'Q' || cInput == '\r' || cInput == '\n') {
			rputs(ST_ABORTED);
			return;
		}

		WhichOne = cInput - 'A';

		if (WhichOne >= 0 && WhichOne < MAX_MEMBERS &&
				PClan->Member[WhichOne])
			break;
	}
	sprintf(szString, "%s\n", PClan->Member[WhichOne]->szName);
	rputs(szString);

	// if he is not a perm. member, tell user
	if (WhichOne >= Game.Data->MaxPermanentMembers) {
		rputs("|04That is not a permanent member.  He will be released at the end of the day.\n%P");
		return;
	}

	/* confirm it */
	//sprintf(szString, "|0SAre you sure you wish to remove %s from the clan?",
	sprintf(szString, ST_REMOVEMEM, PClan->Member[ WhichOne ]->szName);
	if (NoYes(szString) == YES) {
		// %s removed from clan
		sprintf(szString, ST_REMOVED,
				PClan->Member[ WhichOne ]->szName);
		rputs(szString);

		/* release member */
		/* release data from his links */
		for (iTemp = 0; iTemp < MAX_ITEMS_HELD; iTemp++) {
			/* if held by deleted char, remove link */
			if (PClan->Items[iTemp].Available &&
					PClan->Items[iTemp].UsedBy == WhichOne+1) {
				PClan->Items[iTemp].UsedBy = 0;
			}
		}


		free(PClan->Member[ WhichOne ]);
		PClan->Member[ WhichOne ] = NULL;
	}
	(void)Choice;
	(void)Cost;
}

void AddMember(void)
{
	struct pc *TmpPC;
	_INT16 EmptySlot, iTemp, NumMembers;

	NumMembers = 0;
	for (iTemp = 0; iTemp < MAX_MEMBERS; iTemp++) {
		if (PClan->Member[iTemp] && iTemp < Game.Data->MaxPermanentMembers &&
				PClan->Member[iTemp]->Undead == FALSE)
			NumMembers++;
	}

	/* see if too many members already */
	if (NumMembers >= Game.Data->MaxPermanentMembers) {
		// rputs("|07Your clan has the maximum number of permanent members already.\n%P");
		rputs(ST_TOOMANYMEMBERS);
		return;
	}

	/* train dude */

	/* search for empty slot in player list */
	for (iTemp = 0; iTemp < Game.Data->MaxPermanentMembers; iTemp++)
		if (PClan->Member[iTemp] == NULL)
			break;

	EmptySlot = iTemp;

	TmpPC = malloc(sizeof(struct pc));
	CheckMem(TmpPC);

	if (iTemp == 0)
		PC_Create(TmpPC, TRUE);
	else
		PC_Create(TmpPC, FALSE);

	PClan->Member[EmptySlot] = malloc(sizeof(struct pc));
	CheckMem(PClan->Member[EmptySlot]);
	// CopyPC(PClan->Member[EmptySlot], TmpPC);
	*PClan->Member[EmptySlot] = *TmpPC;
	free(TmpPC);
}

void TrainMember(void)
{
	/* ask for which one */
	/* see if that member has any training points */
	/* if not, boot him */
	/* else, give listing of attributes which can be "trained" */

	_INT16 iTemp, WhichOne = 0, Choice;
	long Cost;
	char szString[128], cInput;
	BOOL Done = FALSE, DoneTraining = FALSE;
	char *szTheOptions[11];

	/* cost to upgrade stat */
	_INT16 TCost[8] = { 10, 10, 20, 10, 30, 40, 10, 10 };
	_INT16 IncreaseAmount;

	for (iTemp = 0; iTemp < 8; iTemp++) {
		TCost[iTemp] -= Village.Data->TrainingHallLevel;
	}

	szTheOptions[0] = ST_ATTRNAME0;
	szTheOptions[1] = ST_ATTRNAME1;
	szTheOptions[2] = ST_ATTRNAME2;
	szTheOptions[3] = ST_ATTRNAME3;
	szTheOptions[4] = ST_ATTRNAME4;
	szTheOptions[5] = ST_ATTRNAME5;
	szTheOptions[6] = "HP";
	szTheOptions[7] = "SP";
	szTheOptions[8] = "View Stats";
	szTheOptions[9] = "Help";
	szTheOptions[10]= "Quit";

	while (!DoneTraining) {
		rputs(ST_LONGLINE);
		for (iTemp = 0; iTemp < Game.Data->MaxPermanentMembers; iTemp++) {
			if (PClan->Member[iTemp] &&
					PClan->Member[iTemp]->Status == Here) {
				sprintf(szString, " |0A(|0B%c|0A) |0C%-15s |03(%d tpoints)\n", iTemp + 'A', PClan->Member[iTemp]->szName,
						PClan->Member[iTemp]->TrainingPoints);
				rputs(szString);
			}
		}
		rputs(" |0A(|0BQ|0A) |0CQuit\n");

		rputs(ST_LONGLINE);
		rputs(" |0SWho to train? [|0BEnter=Return|0A] : |0F");

		for (;;) {
			cInput = toupper(od_get_key(TRUE));

			if (cInput == 'Q' || cInput == '\r' || cInput == '\n') {
				DoneTraining = TRUE;
				break;
			}

			WhichOne = cInput - 'A';
			if (WhichOne >= 0 && WhichOne < Game.Data->MaxPermanentMembers &&
					PClan->Member[WhichOne])
				break;
		}

		if (DoneTraining)
			break;

		sprintf(szString, "%s\n\r", PClan->Member[WhichOne]->szName);
		rputs(szString);

		/* if that dude is alive, tell them */
		if (PClan->Member[ WhichOne ]->Status != Here) {
			rputs("|04Member is not conscious and cannot train.\n%P");
			continue;
		}

		if (PClan->Member[WhichOne]->TrainingPoints == 0) {
			rputs("|07That member has no training points!\n%P");
			continue;
		}

		/* if that dude is alive, tell them */
		/* NO MORE REG
		      if (PClan->Member[ WhichOne ]->Level > 5 &&
		        IsRegged(Config->szSysopName, Config->szBBSName, Config->szRegcode) == NFALSE)
		      {
		        rputs("\n|12Members cannot be trained beyond level 5 in the unregistered version.\n%P");
		        continue;
		      }
		*/

		Done = FALSE;
		while (!Done) {
			sprintf(szString, "\n\n |0BTraining %s\n",
					PClan->Member[WhichOne]->szName);
			rputs(szString);
			rputs(ST_LONGLINE);
			/* list attributes */
			for (iTemp = 0; iTemp < 8; iTemp++) {
				sprintf(szString, " |0A(|0B%d|0A) |0C%-20s |06(%2d tpoints)\n",
						iTemp+1, szTheOptions[iTemp], TCost[iTemp]);
				rputs(szString);
			}
			rputs(" |0A(|0BV|0A) |0CView Stats\n");
			rputs(" |0A(|0B?|0A) |0CHelp on Stats\n");
			rputs(" |0A(|0BQ|0A) |0CAbort\n\n");
			rputs(" |0CPlease choose an attribute to upgrade.\n");
			sprintf(szString, " |0B%s |0Chas |0B%d |0Ctraining point(s) to use.\n",
					PClan->Member[WhichOne]->szName,
					PClan->Member[WhichOne]->TrainingPoints);
			rputs(szString);
			rputs(ST_LONGLINE);

			switch (Choice = GetChoice("", ST_ENTEROPTION, szTheOptions, "12345678V?Q", 'Q', TRUE)) {
				case '1' :  /* agility */
				case '2' :  /* dexterity */
				case '3' :  /* strength */
				case '4' :  /* wisdom */
				case '5' :  /* armor str */
				case '6' :  /* charisma */
				case '7' :  /* hp */
				case '8' :  /* mp */
					/* see if enough TPoints */
					if (PClan->Member[WhichOne]->TrainingPoints <
							TCost[Choice - '1']) {
						rputs("|04Not enough training points!\n%P");
						break;
					}

					PClan->Member[WhichOne]->TrainingPoints -=
						TCost[Choice - '1'];

					/* upgrade stat */
					if (Choice >= '1' && Choice <= '6') {
						PClan->Member[WhichOne]->Attributes[Choice-'1']++;

						sprintf(szString, "|03%s's %s increases by |141|03.\n",
								PClan->Member[WhichOne]->szName,
								szTheOptions[Choice -'1']);
						rputs(szString);
					}
					else if (Choice == '7') {
						/* HP increase */
						IncreaseAmount = (3 + RANDOM(5) + RANDOM(5));
						PClan->Member[WhichOne]->MaxHP += IncreaseAmount;

						sprintf(szString, "|03%s's max HP increases by |14%d|03.\n",
								PClan->Member[WhichOne]->szName, IncreaseAmount);
						rputs(szString);
					}
					else if (Choice == '8') {
						/* SP increase */
						IncreaseAmount = (2 + RANDOM(3) + RANDOM(3));
						PClan->Member[WhichOne]->MaxSP += IncreaseAmount;

						sprintf(szString, "|03%s's max SP increases by |14%d|03.\n",
								PClan->Member[WhichOne]->szName, IncreaseAmount);
						rputs(szString);
					}
					door_pause();
					break;
				case 'Q' :  /* quit */
					Done = TRUE;
					break;
				case 'V' :  /* view stats */
					ShowPlayerStats(PClan->Member[WhichOne], FALSE);
					door_pause();
					break;
				case '?' :  /* help file */
					GeneralHelp(ST_STATSHLP);
					break;
			}
		}
	}

	// return
	(void)Cost;
}

