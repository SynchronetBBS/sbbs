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
 * Alliances
 *
 */


#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#ifdef __unix__
#include "unix_wrappers.h"
#else
#include <dos.h>
#include <share.h>
#endif
#include <errno.h>
#include <string.h>

#include <OpenDoor.h>
#include "structs.h"
#include "door.h"
#include "mstrings.h"
#include "language.h"
#include "user.h"
#include "empire.h"
#include "menus.h"
#include "mail.h"
#include "input.h"
#include "items.h"
#include "help.h"
#include "myopen.h"
#include "parsing.h"
#include "fight.h"

extern struct Language *Language;
extern struct game Game;
extern struct clan *PClan;

void RemoveFromAlliance(struct Alliance *Alliance)
{
	int     iTemp, WhichMember = 0;
	_INT16  ClanID[2];
	char szName[40], /*cKey,*/ szString[128];
	struct clan *TmpClan;

	if (GetClanID(ClanID, FALSE, FALSE, Alliance->ID, TRUE) == FALSE) {
		return;
	}

	GetClanNameID(szName, ClanID);

	// if it's the creator, tell him he can't
	if (ClanID[0] == Alliance->CreatorID[0] &&
			ClanID[1] == Alliance->CreatorID[1]) {
		rputs("You can't remove the creator\n");
		return;
	}

	// ask user if he's sure he wants to remove this guy
	if (NoYes("Remove from alliance?") == NO)
		return;

	// list all members of this alliance
	for (iTemp = 0; iTemp < MAX_ALLIANCEMEMBERS; iTemp++) {
		if (Alliance->Member[iTemp][0] == ClanID[0] &&
				Alliance->Member[iTemp][1] == ClanID[1]) {
			WhichMember = iTemp;
			break;
		}
	}

	// remove from alliance[]
	Alliance->Member[ WhichMember ][0] = -1;
	Alliance->Member[ WhichMember ][1] = -1;

	// remove from .PC file
	TmpClan = malloc(sizeof(struct clan));
	CheckMem(TmpClan);

	GetClan(ClanID, TmpClan);

	for (iTemp = 0; iTemp < MAX_ALLIES; iTemp++)
		if (TmpClan->Alliances[iTemp] == Alliance->ID)
			break;

	if (iTemp == MAX_ALLIES) {
		// couldn't find link to alliance
	}
	else {
		// found link!
		TmpClan->Alliances[ iTemp ] = -1;

		Clan_Update(TmpClan);
	}

	FreeClan(TmpClan);

	// send message to that player
	sprintf(szString, "%s ousted you from %s!", PClan->szName,
			Alliance->szName);
	GenericMessage(szString, ClanID, PClan->ClanID, PClan->szName, FALSE);
}




void SeeMemberStats(struct Alliance *Alliance)
{
	_INT16      ClanID[2];
	int         iTemp;
	struct clan *TmpClan;

	// ask for member name
	if (GetClanID(ClanID, FALSE, FALSE, Alliance->ID, TRUE) == FALSE) {
		return;
	}

	// get TmpClan
	TmpClan = malloc(sizeof(struct clan));
	CheckMem(TmpClan);

	GetClan(ClanID, TmpClan);

	// see if in this alliance
	for (iTemp = 0; iTemp < MAX_ALLIES; iTemp++)
		if (TmpClan->Alliances[iTemp] == Alliance->ID)
			break;

	// if so, show stats, else boot him
	if (iTemp == MAX_ALLIES) {
		rputs("Clan is not in this alliance!  Can't view stats.\n");
		FreeClan(TmpClan);
		return;
	}

	ClanStats(TmpClan, FALSE);

	FreeClan(TmpClan);
}

void ShowAllianceItems(struct Alliance *Alliance)
{
	int     iTemp, iTemp2, Length, LastItem = 0, FoundItem = FALSE;
	int     CurItem;
	char    szString[100];

	rputs("|0B # Name                      ");
	rputs("|0B # Name                 \n");
	rputs("|0AÍÍÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÍÍ|07  ");
	rputs("|0AÍÍÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÍÍ|07\n");

	/* show all the items here */
	for (iTemp = 0, CurItem = 0; iTemp < MAX_ALLIANCEITEMS; iTemp++) {
		if (Alliance->Items[iTemp].Available) {
			FoundItem = TRUE;

			sprintf(szString, "|0L%2d |0M%-20s", iTemp+1, Alliance->Items[iTemp].szName);

#define LEN (26+9)      // add 9 for the | codes!

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
		rputs(" |04No items.");

	if (LastItem%2 == 0)
		rputs("\n");

	rputs("\n");
}



void DonationRoom(struct Alliance *Alliance)
{
	/* modify item stats, assume it's the player */
	int     ItemIndex, RoomItemIndex, EmptySlot;
	int     DefaultItemIndex, iTemp;
	char    szString[100];

	/* options available

	[L]ist items
	e[X]amine item
	[D]onate item
	[T]ake item
	[Q]uit
	[V]iew Stats

	*/

	for (;;) {
		rputs(ST_ITEMROOM0);

		switch (od_get_answer("?XDTLQ\r\n I*V")) {
			case 'V' :  // clan stats
				rputs("View Stats\n");
				ClanStats(PClan, TRUE);
				break;
			case 'I' :  // show room items
				rputs("List room items\n\n");
				ShowAllianceItems(Alliance);
				break;
			case '?' :
				rputs("Help\n\n");
				Help("Item Help", ST_ITEMHLP);
				break;
			case ' ' :
			case '\r':
			case '\n':
			case 'Q' :
				return;
			case 'X' :  /* examine item */
				rputs(ST_ISTATS1);

				/* see if anything to examine */
				for (iTemp = 0; iTemp < MAX_ALLIANCEITEMS; iTemp++) {
					if (Alliance->Items[iTemp].Available)
						break;
				}
				if (iTemp == MAX_ALLIANCEITEMS) {
					rputs(ST_ISTATS2);
					break;
				}

				/* find first item in inventory */
				for (iTemp = 0; iTemp < MAX_ALLIANCEITEMS; iTemp++) {
					if (Alliance->Items[iTemp].Available)
						break;
				}
				if (iTemp == MAX_ALLIANCEITEMS) {
					DefaultItemIndex = 1;
				}
				else
					DefaultItemIndex = iTemp+1;

				ItemIndex = GetLong(ST_ISTATS3, DefaultItemIndex, MAX_ALLIANCEITEMS);
				if (ItemIndex == 0)
					break;
				ItemIndex--;

				/* if that item is non-existant, tell him */
				if (Alliance->Items[ItemIndex].Available == FALSE) {
					rputs(ST_ISTATS4);
					break;
				}
				ShowItemStats(&Alliance->Items[ItemIndex], NULL);
				break;
			case 'L' :
				rputs(ST_ISTATS5);
				ListItems(PClan);
				break;
			case 'D' :  /* drop item */
				// see if room in room to drop item
				for (iTemp = 0; iTemp < MAX_ALLIANCEITEMS; iTemp++) {
					if (Alliance->Items[iTemp].Available == FALSE)
						break;
				}
				if (iTemp == MAX_ALLIANCEITEMS) {
					rputs("\n|07The room is currently full.  You cannot drop any more.\n");
					break;
				}
				else RoomItemIndex = iTemp;

				rputs(ST_ISTATS6);

				/* see if anything to drop */
				for (iTemp = 0; iTemp < MAX_ITEMS_HELD; iTemp++) {
					if (PClan->Items[iTemp].Available &&
							PClan->Items[iTemp].UsedBy == 0)
						break;
				}
				if (iTemp == MAX_ITEMS_HELD) {
					rputs("No items to be dropped.\n");
					break;
				}

				DefaultItemIndex = iTemp+1;

				ItemIndex = GetLong(ST_ISTATS7, DefaultItemIndex, MAX_ITEMS_HELD);
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

					// add it to room
					Alliance->Items[RoomItemIndex] = PClan->Items[ItemIndex];
					Alliance->Items[RoomItemIndex] = PClan->Items[ItemIndex];

					// remove from user's stats
					PClan->Items[ItemIndex].szName[0] = 0;
					PClan->Items[ItemIndex].Available = FALSE;
				}
				break;
			case '*' :  /* destroy item */
				rputs("Destroy Item\n");

				/* see if anything to destroy */
				for (iTemp = 0; iTemp < MAX_ALLIANCEITEMS; iTemp++) {
					if (Alliance->Items[iTemp].Available)
						break;
				}

				if (iTemp == MAX_ALLIANCEITEMS) {
					// nothing to destroy
					rputs("Nothing to destroy!\n");
					break;
				}
				else
					DefaultItemIndex = iTemp+1;

				ItemIndex = GetLong("Which item to destroy? (0=abort)",
									DefaultItemIndex, MAX_ALLIANCEITEMS);
				if (ItemIndex == 0)
					break;
				ItemIndex--;

				/* if that item is non-existant, tell him */
				if (Alliance->Items[ItemIndex].Available == FALSE) {
					rputs(ST_ISTATS4);
					break;
				}

				/* still wanna drop it? */
				sprintf(szString, "|0SAre you sure you wish to destroy %s?",
						Alliance->Items[ItemIndex].szName);

				if (NoYes(szString) == YES) {
					/* destroy it */
					sprintf(szString, "%s destroyed!\n\n",
							Alliance->Items[ItemIndex].szName);
					rputs(szString);

					Alliance->Items[ItemIndex].szName[0] = 0;
					Alliance->Items[ItemIndex].Available = FALSE;

				}
				break;
			case 'T' :  /* take item */
				rputs("Take item\n");

				// see if inventory full
				for (iTemp = 0; iTemp < MAX_ITEMS_HELD; iTemp++) {
					if (PClan->Items[iTemp].Available == FALSE)
						break;
				}
				if (iTemp == MAX_ITEMS_HELD) {
					/* no more room in inventory */
					rputs(ST_ITEMNOMOREROOM);
					break;
				}
				else
					EmptySlot = iTemp;

				/* see if anything to take */
				for (iTemp = 0; iTemp < MAX_ALLIANCEITEMS; iTemp++) {
					if (Alliance->Items[iTemp].Available)
						break;
				}

				if (iTemp == MAX_ALLIANCEITEMS) {
					// nothing to destroy
					rputs("Nothing to take!\n");
					break;
				}
				else
					DefaultItemIndex = iTemp+1;

				ItemIndex = GetLong("|0STake which item from the room? (0=abort)",
									DefaultItemIndex, MAX_ALLIANCEITEMS);
				if (ItemIndex == 0)
					break;
				ItemIndex--;

				/* if that item is non-existant, tell him */
				if (Alliance->Items[ItemIndex].Available == FALSE) {
					rputs(ST_ISTATS4);
					break;
				}

				/* take it */
				sprintf(szString, "%s taken!\n\n", Alliance->Items[ItemIndex].szName);
				rputs(szString);

				PClan->Items[EmptySlot] = Alliance->Items[ItemIndex];

				Alliance->Items[ItemIndex].szName[0] = 0;
				Alliance->Items[ItemIndex].Available = FALSE;
				break;
		}
	}
}



void GetAlliances(struct Alliance *Alliances[MAX_ALLIANCES])
{
	FILE    *fp;
	int     iTemp;

	// init alliances as NULLs
	for (iTemp = 0; iTemp < MAX_ALLIANCES; iTemp++)
		Alliances[iTemp] = NULL;

	fp = fopen("ally.dat", "rb");
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
	int iTemp;

	fp = fopen("ally.dat", "wb");
	if (fp) {
		for (iTemp = 0; iTemp < MAX_ALLIANCES; iTemp++) {
			if (Alliances[iTemp] == NULL)
				continue;

			EncryptWrite(Alliances[iTemp], sizeof(struct Alliance), fp, XOR_ALLIES);
		}
		fclose(fp);
	}
}

void CreateAlliance(struct Alliance *Alliance, struct Alliance *Alliances[MAX_ALLIANCES])
{
	int iTemp;
	char szName[30], szString[128];
	BOOL AllianceNameInUse;

	// get NextID
	Alliance->ID = Game.Data->NextAllianceID++;

	// get name of alliance
	szName[0] = 0;
	Alliance->szName[0] = 0;
	// rputs("Enter a name for this alliance.\n");
	while (! szName[0]) {
		rputs(ST_AMENU4);
		GetStr(szName, 29, TRUE);
		RemovePipes(szName, szString);
		strcpy(szName, szString);

		if (szName[0] == 0)
			continue;

		if (szName[0] == '?')
			szName[0] = '!';

		// see if in use, if so, nullify name
		AllianceNameInUse = FALSE;
		for (iTemp = 0; iTemp < MAX_ALLIANCES; iTemp++) {
			if (Alliances[iTemp] == NULL)
				continue;

			if (stricmp(szName, Alliances[iTemp]->szName) == 0) {
				AllianceNameInUse = TRUE;
				break;
			}
		}

		if (AllianceNameInUse) {
			rputs("|07That name is already in use.\n\n");
			szName[0] = 0;
		}
	}

	strcpy(Alliance->szName, szName);

	Alliance->CreatorID[0] = PClan->ClanID[0];
	Alliance->CreatorID[1] = PClan->ClanID[1];
	Alliance->OriginalCreatorID[0] = PClan->ClanID[0];
	Alliance->OriginalCreatorID[1] = PClan->ClanID[1];

	for (iTemp = 0; iTemp < MAX_ALLIANCEMEMBERS; iTemp++) {
		Alliance->Member[ iTemp ][0] = -1;
		Alliance->Member[ iTemp ][1] = -1;
	}

	// set first member to be this creator
	Alliance->Member[0][0] = PClan->ClanID[0];
	Alliance->Member[0][1] = PClan->ClanID[1];

	Empire_Create(&Alliance->Empire, FALSE);
	Alliance->Empire.VaultGold = 0;
	strcpy(Alliance->Empire.szName, Alliance->szName);
	Alliance->Empire.OwnerType = EO_ALLIANCE;
	Alliance->Empire.AllianceID = Alliance->ID;

	// initialize items
	for (iTemp = 0; iTemp < MAX_ALLIANCEITEMS; iTemp++)
		Alliance->Items[iTemp].Available = FALSE;

	// make user a part of alliance
	for (iTemp = 0; iTemp < MAX_ALLIES; iTemp++)
		if (PClan->Alliances[iTemp] == -1)
			break;

	PClan->Alliances[ iTemp ] = Alliance->ID;
}


void ShowAlliances(struct clan *Clan)
{
	int iTemp, NumAlliances = 0, CurAlliance;
	char /*szName[25],*/ szString[50];
	struct Alliance *Alliances[MAX_ALLIANCES];

	rputs(ST_ALLIANCES);

	/* go through each of alliance variables */
	for (iTemp = 0; iTemp < MAX_ALLIES; iTemp++)
		if (Clan->Alliances[iTemp] != -1)
			NumAlliances++;

	/* if no alliances, tell user */
	if (NumAlliances == 0) {
		rputs(ST_NOALLIANCES);
		return;
	}

	GetAlliances(Alliances);

	/* if alliances found, display names */

	for (iTemp = 0; iTemp < MAX_ALLIES; iTemp++) {
		if (Clan->Alliances[iTemp] != -1) {
			for (CurAlliance = 0; CurAlliance < MAX_ALLIANCES; CurAlliance++)
				if (Alliances[CurAlliance] != NULL && Alliances[CurAlliance]->ID == Clan->Alliances[iTemp]) {
					sprintf(szString, "|06* |14%s\n", Alliances[ CurAlliance ]->szName);
					rputs(szString);
				}
		}
	}
	door_pause();

	// free up mem used by alliances
	for (iTemp = 0; iTemp < MAX_ALLIANCES; iTemp++)
		if (Alliances[iTemp] != NULL)
			free(Alliances[iTemp]);
}

BOOL EnterAlliance(struct Alliance *Alliance)
{
	char *szTheOptions[13], *szString, szName[25];
	char szFileName[13];
	int iTemp;

	// show stats

	// show help if user needs it

	szString = MakeStr(128);

	LoadStrings(1290, 13, szTheOptions);

	/* get a choice */
	for (;;) {
		sprintf(szString, "\n |0BAlliance Menu:  |0C%s\n", Alliance->szName);
		rputs(szString);

		switch (GetChoice("Alliance", ST_ENTEROPTION, szTheOptions, "LIRSCDP*VQ?W!", 'Q', TRUE)) {
			case 'S' :  // see member's stats
				SeeMemberStats(Alliance);
				break;
			case '!' :  // remove self from alliance
				// if creator, tell him he can't or destroy alliance
				if (PClan->ClanID[0] == Alliance->CreatorID[0] &&
						PClan->ClanID[1] == Alliance->CreatorID[1]) {
					rputs("You are the creator, can't remove self...\n");
					break;
				}

				// are you sure?
				if (NoYes("Remove self?") == NO)
					break;

				// remove from member[] list
				for (iTemp = 0; iTemp < MAX_ALLIANCEMEMBERS; iTemp++)
					if (Alliance->Member[iTemp][0] == PClan->ClanID[0] &&
							Alliance->Member[iTemp][1] == PClan->ClanID[1]) {
						Alliance->Member[iTemp][0] = -1;
						Alliance->Member[iTemp][1] = -1;
						break;
					}

				// remove from globalplayerclan->alliances[]
				for (iTemp = 0; iTemp < MAX_ALLIES; iTemp++)
					if (PClan->Alliances[iTemp] == Alliance->ID) {
						PClan->Alliances[iTemp] = -1;
						break;
					}

				// write message to creator
				sprintf(szString, "%s left the alliance of %s",
						PClan->szName, Alliance->szName);

				GenericMessage(szString, Alliance->CreatorID,
							   PClan->ClanID, PClan->szName, FALSE);

				// done
				free(szString);
				return FALSE;
			case 'W' :  // write to all allies
				Mail_WriteToAllies(Alliance);
				break;
			case 'L' :  // list members
				for (iTemp = 0; iTemp < MAX_ALLIANCEMEMBERS; iTemp++) {
					if (Alliance->Member[iTemp][0] != -1) {
						GetClanNameID(szName, Alliance->Member[iTemp]);
						sprintf(szString, "|0A* |0B%s\n", szName);
						rputs(szString);
					}
				}
				door_pause();
				break;
			case 'I' :  // invite clan
				Mail_RequestAlliance(Alliance);
				break;
			case 'R' :  // remove from alliance
				RemoveFromAlliance(Alliance);
				break;
			case 'V' :  // clan stats
				ClanStats(PClan, TRUE);
				break;
			case 'D' :    /* donation room */
				DonationRoom(Alliance);
				break;
			case 'C' :    /* chat room */
				sprintf(szFileName, "HALL%02d.TXT", Alliance->ID);
				Menus_ChatRoom(szFileName);
				break;
			case 'P' :    /* manage empire */
				Empire_Manage(&Alliance->Empire);
				break;
			case '*' :  // destroy alliance
				if (Alliance->CreatorID[0] != PClan->ClanID[0] ||
						Alliance->CreatorID[1] != PClan->ClanID[1]) {
					rputs("|0SOnly the creator may change destroy this alliance.\n");
					if (ClanExists(Alliance->CreatorID) == FALSE) {
						rputs("|0SAlliance creator was not found, however.  You will be the new creator.\n");
						Alliance->CreatorID[0] = PClan->ClanID[0];
						Alliance->CreatorID[1] = PClan->ClanID[1];
					}
					else {
						door_pause();
						break;
					}
				}
				if (NoYes("|0SDestroy this alliance? |08(All will be lost!) |0S:") == YES) {
					free(szString);
					return TRUE;
				}
				break;
			case 'Q' :
				free(szString);
				return FALSE;
		}
	}
}

void KillAlliance(int AllianceID)
{
	// go through player file
	// skip user online
	// if Alliance is same as one given, make it -1
	// write to file

	struct clan *TmpClan;
	FILE *fpPlayerFile;
	int CurClan, CurAlliance;
	long Offset;

	TmpClan = malloc(sizeof(struct clan));
	CheckMem(TmpClan);

	fpPlayerFile = _fsopen(ST_CLANSPCFILE, "r+b", SH_DENYRW);
	if (!fpPlayerFile) {
		rputs(ST_ERRORPC);
		free(TmpClan);
		return;
	}

	// get list of all clan names from file, write to
	for (CurClan = 0;; CurClan++) {
		Offset = (long)CurClan * (sizeof(struct clan) + 6L*sizeof(struct pc));
		if (fseek(fpPlayerFile, Offset, SEEK_SET)) {
			fclose(fpPlayerFile);
			break;  /* couldn't fseek, so exit */
		}

		if (EncryptRead(TmpClan, sizeof(struct clan), fpPlayerFile, XOR_USER) == 0) {
			fclose(fpPlayerFile);
			break;  /* stop reading if no more players found */
		}

		// skip if user is online
		if (TmpClan->ClanID[0] == PClan->ClanID[0] &&
				TmpClan->ClanID[1] == PClan->ClanID[1])
			continue;

		// see if in alliance
		for (CurAlliance = 0; CurAlliance < MAX_ALLIES; CurAlliance++)
			if (TmpClan->Alliances[CurAlliance] == AllianceID)
				break;

		// not in alliance, skip
		if (CurAlliance == MAX_ALLIES)
			continue;

		// in alliance, update his info and write to file
		TmpClan->Alliances[CurAlliance] = -1;

		Offset = (long)CurClan * (sizeof(struct clan) + 6L*sizeof(struct pc));
		fseek(fpPlayerFile, Offset, SEEK_SET);
		EncryptWrite(TmpClan, sizeof(struct clan), fpPlayerFile, XOR_USER);
	}

	fclose(fpPlayerFile);
	free(TmpClan);
}

void Alliance_Maint(void)
{
	struct Alliance *Alliances[MAX_ALLIANCES];
	int iTemp;

	GetAlliances(Alliances);

	// maintain alliances...
	for (iTemp = 0; iTemp  < MAX_ALLIANCES; iTemp++) {
		// update empire
		if (Alliances[iTemp] != NULL)  {
			Empire_Maint(&Alliances[iTemp]->Empire);
			if (Alliances[iTemp]->szName[0] == '?')
				Alliances[iTemp]->szName[0] = '!';
		}
	}

	// deinit alliances and update to file
	UpdateAlliances(Alliances);

	// free up mem used by alliances
	for (iTemp = 0; iTemp < MAX_ALLIANCES; iTemp++)
		if (Alliances[iTemp] != NULL)
			free(Alliances[iTemp]);
}


void FormAlliance(int AllyID)
{
	int iTemp, UserAllianceSlot, WhichAlliance;
	struct Alliance *Alliances[MAX_ALLIANCES];

	// see if this guy can ally any more
	for (iTemp = 0; iTemp < MAX_ALLIES; iTemp++) {
		// if in this alliance, tell user and exit!
		if (PClan->Alliances[iTemp] == AllyID) {
			rputs("Already in this alliance!\n");
			return;
		}

		// find open slot
		if (PClan->Alliances[iTemp] == -1)
			break;
	}

	if (iTemp == MAX_ALLIES) {
		// FIXME: make look better
		rputs("Too many alliances!\n");
		return;
	}

	// else, found spot to place alliance
	UserAllianceSlot = iTemp;

	// load up alliances
	GetAlliances(Alliances);

	// find that alliance
	for (iTemp = 0; iTemp < MAX_ALLIANCES; iTemp++) {
		if (Alliances[iTemp] != NULL && Alliances[iTemp]->ID == AllyID)
			break;
	}

	if (iTemp == MAX_ALLIANCES) {
		rputs("Alliance no longer exists!\n");

		// free up mem used by alliances
		for (iTemp = 0; iTemp < MAX_ALLIANCES; iTemp++)
			if (Alliances[iTemp] != NULL)
				free(Alliances[iTemp]);
		return;
	}

	// else found it
	WhichAlliance = iTemp;

	// find spot to place in that particular alliance to place clan
	for (iTemp = 0; iTemp < MAX_ALLIANCEMEMBERS; iTemp++)
		if (Alliances[WhichAlliance] != NULL && Alliances[WhichAlliance]->Member[ iTemp ][0] == -1)
			break;

	if (iTemp == MAX_ALLIANCEMEMBERS) {
		rputs("Too many members in that alliance, you cannot join.\n");

		// free up mem used by alliances
		for (iTemp = 0; iTemp < MAX_ALLIANCES; iTemp++)
			if (Alliances[iTemp] != NULL)
				free(Alliances[iTemp]);
		return;
	}

	// else, found spot, make him part of it now
	Alliances[WhichAlliance]->Member[ iTemp ][0] = PClan->ClanID[0];
	Alliances[WhichAlliance]->Member[ iTemp ][1] = PClan->ClanID[1];
	PClan->Alliances[ UserAllianceSlot ] = AllyID;


	// deinit alliances and update to file
	UpdateAlliances(Alliances);

	// free up mem used by alliances
	for (iTemp = 0; iTemp < MAX_ALLIANCES; iTemp++)
		if (Alliances[iTemp] != NULL)
			free(Alliances[iTemp]);
}

