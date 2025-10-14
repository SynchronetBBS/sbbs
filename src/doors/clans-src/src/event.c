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
 * Event ADT
 *
 *
 */


#include <stdio.h>
#include <stdlib.h>

#include <OpenDoor.h>
#include "structs.h"
#include "language.h"
#include "mstrings.h"
#include "door.h"
#include "help.h"
#include "quests.h"


extern struct clan *PClan;
extern struct Language *Language;
extern struct Quest Quests[MAX_QUESTS];

void GoQuest(void)
{
	_INT16 iTemp, TotalQuests = 0, WhichQuest, NumQuestsDone, QuestIndex[64];
	BOOL QuestDone, QuestKnown;
	char KnownBitSet, DoneBitSet;
	char szString[50], cInput;

	// tell him how many quests he's done right here for now since i'm too lazy
	for (iTemp = 0, NumQuestsDone = 0; iTemp < MAX_QUESTS; iTemp++) {
		if (PClan->QuestsDone[ iTemp/8 ] & (1 << (iTemp%8)))
			NumQuestsDone++;
	}

	for (;;) {
		sprintf(szString, " |0CQuests Completed:  |0B%d\n", NumQuestsDone);
		rputs(szString);
		rputs(ST_LONGLINE);

		/* List quests known and not completed */
		/* <pause> where needed */
		TotalQuests = 0;
		for (iTemp = 0; iTemp < MAX_QUESTS; iTemp++) {
			KnownBitSet =
				(PClan->QuestsKnown[ iTemp/8 ] & (1 << (iTemp%8)) ||
				 (Quests[iTemp].Known && Quests[iTemp].Active));
			DoneBitSet  = PClan->QuestsDone[ iTemp/8 ] & (1 << (iTemp%8));

			// quest known? AND not complete?
			if (KnownBitSet && !DoneBitSet && Quests[iTemp].pszQuestName) {
				// show quest then
				sprintf(szString, " |0A(|0B%c|0A) |0C%s\n", TotalQuests + 'A',
						Quests[iTemp].pszQuestName);
				rputs(szString);
				QuestIndex[TotalQuests] = iTemp;
				TotalQuests++;
			}
		}

		if (TotalQuests == 0) {
			rputs("\n|12You do not know of any quests.\n%P");
			return;
		}

		rputs(ST_LONGLINE);
		rputs(" |0GWhich quest? (Enter=abort)|0E> |0F");
		/* get choice from user on which quest to complete */
		for (;;) {
			cInput = toupper(od_get_key(TRUE));

			if (cInput == '\r' || cInput == '\n') {
				rputs(ST_ABORTED);
				return;
			}

			/* run QUEST.EVT block here */
			WhichQuest = cInput - 'A';
			if (WhichQuest >= 0 && WhichQuest < TotalQuests) {
				break;
			}
		}

		if (Quests[ QuestIndex[WhichQuest] ].pszQuestIndex == NULL) {
			rputs("\n|12Quest not found.\n%P");
			return;
		}

		QuestKnown = PClan->QuestsKnown[ QuestIndex[WhichQuest]/8 ] & (char)(1 << (QuestIndex[WhichQuest]%8)) ||
					 (Quests[QuestIndex[WhichQuest]].Known && Quests[QuestIndex[WhichQuest]].Active);
		QuestDone  = PClan->QuestsDone[ QuestIndex[WhichQuest]/8 ] & (char)(1 << (QuestIndex[WhichQuest]%8));

		//od_printf("Comparing with %d\n", (1 << (QuestIndex[WhichQuest]%8)));

		if (QuestKnown == FALSE || QuestDone || !Quests[QuestIndex[WhichQuest]].pszQuestName) {
			rputs("\n|12Quest not found.\n%P");
			return;
		}

		// display name of quest
		rputs(Quests[QuestIndex[WhichQuest]].pszQuestName);
		rputs("\n\n");

		// show help
		Help(Quests[ QuestIndex[WhichQuest] ].pszQuestName, "quests.hlp");
		if (YesNo("\n|0SGo on this quest?") == YES)
			break;

		rputs("\n");
	}

	PClan->QuestToday = TRUE;

	/* if successful (returns TRUE), set that quest bit to done */
	if (RunEvent(FALSE,
				 Quests[QuestIndex[WhichQuest]].pszQuestFile, Quests[QuestIndex[WhichQuest]].pszQuestIndex,
				 NULL, NULL)) {
		// set bit since quest completed
		PClan->QuestsDone[ QuestIndex[WhichQuest]/8 ] |= (1<<(QuestIndex[WhichQuest]%8));
	}
	door_pause();
}
