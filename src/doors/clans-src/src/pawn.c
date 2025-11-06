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
 * Pawn Shop ADT
 */

#include <stdio.h>
#include <stdlib.h>
#include "platform.h"

#include "door.h"
#include "help.h"
#include "input.h"
#include "items.h"
#include "language.h"
#include "misc.h"
#include "mstrings.h"
#include "myopen.h"
#include "pawn.h"
#include "structs.h"
#include "system.h"
#include "user.h"
#include "video.h"
#include "village.h"

#define MAX_PSITEMS     100
#define MAX_PSITEMAGE   2

static int16_t PS_GetOpenItemSlot(struct clan *Clan)
{
	// return -1 if no more open slots

	int16_t iTemp;

	// see if he has room to carry it
	for (iTemp = 0; iTemp < MAX_ITEMS_HELD; iTemp++) {
		if (Clan->Items[iTemp].Available == false)
			break;
	}
	if (iTemp == MAX_ITEMS_HELD) {
		/* no more room in inventory */
		return -1;
	}
	else
		return iTemp;
}

static void PS_Init(struct item_data **PS_Items)
{
	int16_t iTemp, CurItem;
	FILE *fp;

	for (iTemp = 0; iTemp < MAX_PSITEMS; iTemp++) {
		PS_Items[iTemp] = NULL;
	}

	fp = _fsopen("pawn.dat", "rb", _SH_DENYWR);
	if (!fp) {
		return;
	}

	for (CurItem = 0; CurItem < MAX_PSITEMS; CurItem++) {
		PS_Items[CurItem] = malloc(sizeof(struct item_data));
		CheckMem(PS_Items[CurItem]);

		notEncryptRead_s(item_data, PS_Items[CurItem], fp, XOR_ITEMS) {
			free(PS_Items[CurItem]);
			PS_Items[CurItem] = NULL;
			break;
		}
	}

	fclose(fp);
}

static void PS_Close(struct item_data **PS_Items)
{
	FILE *fp;
	int16_t CurItem;

	// write to PAWN.DAT only those items which exist
	fp = _fsopen("pawn.dat", "wb", _SH_DENYWR);

	if (fp) {
		for (CurItem = 0; CurItem < MAX_PSITEMS; CurItem++) {
			if (PS_Items[CurItem])
				EncryptWrite_s(item_data, PS_Items[CurItem], fp, XOR_ITEMS);
		}
		fclose(fp);
	}

	for (CurItem = 0; CurItem < MAX_PSITEMS; CurItem++) {
		if (PS_Items[CurItem]) {
			free(PS_Items[CurItem]);
			PS_Items[CurItem] = NULL;
		}
	}
}

static void PS_List(struct item_data *PS_Items[MAX_PSITEMS])
{
	// list items of a specified type -- in future use type

	int16_t CurItem, ItemsShown = 0;
	char szString[128];

	for (CurItem = 0; CurItem < MAX_PSITEMS; CurItem++) {
		if (PS_Items[CurItem]) {
			snprintf(szString, sizeof(szString), ST_PAWN6,
					(int)(CurItem + 1), PS_Items[CurItem]->szName);
			rputs(szString);

			if (ItemsShown == 60)
				rputs("\n%P");

			ItemsShown++;
		}
		else
			continue;

		if (ItemsShown%3 == 0)
			rputs("\n");
	}

	if (ItemsShown == 0)
		rputs(ST_PAWN13);
	else if (ItemsShown%3 != 0)
		rputs("\n");
}


static void PS_Buy(struct item_data *PS_Items[MAX_PSITEMS])
{
	bool Done;
	int16_t ItemIndex;
	int32_t lCost;
	char *szTheOptions[5], szString[50];
	int16_t EmptySlot;

	LoadStrings(1190, 5, szTheOptions);

	Done = false;
	while (!Done) {
		switch (GetChoice("", ST_PAWN0, szTheOptions, "LQBX?", 'Q', false)) {
			case 'Q' :
				Done = true;
				break;

			case 'L' :  // list items
				PS_List(PS_Items);
				break;

			case 'X' :  // examine item
				ItemIndex = (int16_t)GetLong(ST_PAWN1, 0, MAX_PSITEMS);

				if (ItemIndex == 0) break;

				ItemIndex--;

				if (PS_Items[ItemIndex] == NULL) {
					rputs(ST_INVALIDITEM);
					break;
				}

				// show item
				ShowItemStats(PS_Items[ItemIndex], NULL);
				break;
			case 'B' :  // buy an item
				// find open slot
				EmptySlot = PS_GetOpenItemSlot(PClan);

				if (EmptySlot == -1) {
					rputs(ST_PAWN2);
					break;
				}

				ItemIndex = (int16_t)GetLong(ST_PAWN3, 0, MAX_PSITEMS);

				if (ItemIndex == 0) break;

				ItemIndex--;

				if (PS_Items[ItemIndex] == NULL) {
					rputs(ST_INVALIDITEM);
					break;
				}

				// show item
				ShowItemStats(PS_Items[ItemIndex], NULL);

				lCost = PS_Items[ItemIndex]->lCost -
						(PS_Items[ItemIndex]->lCost * 10)/100 +
						(PS_Items[ItemIndex]->lCost * my_random(10)) / 100;

				snprintf(szString, sizeof(szString), ST_PAWN4, (long)lCost);

				if (YesNo(szString) == NO)
					break;

				// see if enough gold
				if (PClan->Empire.VaultGold < lCost) {
					rputs(ST_PAWN5);
					break;
				}

				// else add to items, remove cash
				PClan->Empire.VaultGold -= lCost;

				PClan->Items[EmptySlot] = *PS_Items[ItemIndex];

				free(PS_Items[ItemIndex]);
				PS_Items[ItemIndex] = NULL;
				break;
		}
	}

	// use B key to buy an item
	// if no more room in inventory, tell user
	// prompt user for item #
	// make sure it's legal
	// show stats of item
	// ask if user wants to buy for sure
	// if so, add item to user's inventory
	//   remove from PS_Items[] by freeing up that memory
}

static void PS_Sell(struct item_data *PS_Items[MAX_PSITEMS])
{
	// allow user to sell an item he owns

	// allow user to list his own items, examine them, etc.

	// use S key to buy an item
	// if no more room in inventory, tell user they aren't buying
	// prompt user for item #
	// make sure it's legal -- if it's in use, tell him he to unequip it
	// show stats of item, show how much he'll buy it for
	// ask if user wants to sell it for sure
	// if so, add item to PS_Items[] (find NULL and get mem for it)
	//   remove from user's item list, make Item->Date = Today;


	bool Done;
	int16_t ItemIndex, ItemSlot, CurItem;
	int32_t lCost;
	char *szTheOptions[6], szString[128];

	LoadStrings(1195, 5, szTheOptions);
	szTheOptions[5] = "Sell All";

	snprintf(szString, sizeof(szString), "|0CAt its current level, this shop can take in up to |0B%d |0Citems.\n\n",
			20*Village.Data.PawnLevel);
	rputs(szString);

	Done = false;
	while (!Done) {
		switch (GetChoice("", ST_PAWN7, szTheOptions, "LQSX?A", 'Q', false)) {
			case 'Q' :
				Done = true;
				break;

			case 'L' :  // list items
				ListItems(PClan);
				break;

			case 'X' :  // examine item
				ItemIndex = (int16_t)GetLong(ST_PAWN8, 0, 30);

				if (ItemIndex == 0) break;

				ItemIndex--;

				if (PClan->Items[ItemIndex].Available == false) {
					rputs(ST_INVALIDITEM);
					break;
				}

				// show item
				ShowItemStats(&PClan->Items[ItemIndex], PClan);
				break;

			case 'S' :  // sell item
				// find index of next item slot open
				for (CurItem = 0; CurItem < 20*Village.Data.PawnLevel; CurItem++) {
					if (PS_Items[CurItem] == NULL)  break;
				}

				if (CurItem == 20*Village.Data.PawnLevel) {
					//snprintf(szString, sizeof(szString), "I'm sorry, we cannot accept any more items.\n\r");
					rputs(ST_PAWN9);
					break;
				}
				ItemSlot = CurItem;

				ItemIndex = (int16_t)GetLong(ST_PAWN10, 0, 30);

				if (ItemIndex == 0) break;

				ItemIndex--;

				if (PClan->Items[ItemIndex].Available == false) {
					rputs(ST_INVALIDITEM);
					break;
				}

				// show item
				ShowItemStats(&PClan->Items[ItemIndex], PClan);

				// if in use, tell him
				if (PClan->Items[ItemIndex].UsedBy != 0) {
					rputs(ST_PAWN11);
					break;
				}

				lCost = (PClan->Items[ItemIndex].lCost * 3) / 4 +
						(PClan->Items[ItemIndex].lCost * (int32_t)my_random(15)) / 100;

				snprintf(szString, sizeof(szString), ST_PAWN12, (long)lCost);

				if (YesNo(szString) == NO) break;

				// give him gold
				PClan->Empire.VaultGold += lCost;

				// get mem for it
				PS_Items[ItemSlot] = malloc(sizeof(struct item_data));
				CheckMem(PS_Items[ItemSlot]);

				*PS_Items[ItemSlot] = PClan->Items[ItemIndex];

				PS_Items[ItemSlot]->ItemDate = DaysSince1970(System.szTodaysDate);

				// remove from user's item list
				PClan->Items[ItemIndex].Available = false;
				break;
			case 'A' :  // sell all
				for (ItemIndex = 0; ItemIndex < MAX_ITEMS_HELD; ItemIndex++) {
					// skip this item if not available or in use
					if (PClan->Items[ItemIndex].Available == false ||
							PClan->Items[ItemIndex].UsedBy) {
						continue;
					}

					// find index of next item slot open
					for (CurItem = 0; CurItem < 20*Village.Data.PawnLevel; CurItem++) {
						if (PS_Items[CurItem] == NULL)  break;
					}

					if (CurItem == 20*Village.Data.PawnLevel) {
						// od_printf("I'm sorry, we cannot accept any more items.\n\r");
						rputs(ST_PAWN9);
						break;
					}
					ItemSlot = CurItem;

					// show item
					ShowItemStats(&PClan->Items[ItemIndex], PClan);

					lCost = (PClan->Items[ItemIndex].lCost * 3) / 4 +
							(PClan->Items[ItemIndex].lCost * (int32_t)my_random(15)) / 100;

					snprintf(szString, sizeof(szString), ST_PAWN12, (long)lCost);

					if (YesNo(szString) == NO) continue;

					// give him gold
					PClan->Empire.VaultGold += lCost;

					// get mem for it
					PS_Items[ItemSlot] = malloc(sizeof(struct item_data));
					CheckMem(PS_Items[ItemSlot]);

					*PS_Items[ItemSlot] = PClan->Items[ItemIndex];

					PS_Items[ItemSlot]->ItemDate = DaysSince1970(System.szTodaysDate);

					// remove from user's item list
					PClan->Items[ItemIndex].Available = false;
				}
				break;
		}
	}
}

void PS_Maint(void)
{
	struct item_data *PS_Items[MAX_PSITEMS];
	int16_t CurItem;

	LogDisplayStr("* PS_Maint()\n");

	PS_Init(PS_Items);

	// remove those items which are older than X days
	for (CurItem = 0; CurItem < MAX_PSITEMS; CurItem++) {
		if (PS_Items[CurItem]) {
			if ((DaysSince1970(System.szTodaysDate) - PS_Items[CurItem]->ItemDate) >= MAX_PSITEMAGE) {
				free(PS_Items[CurItem]);
				PS_Items[CurItem] = NULL;
			}
		}
	}

	PS_Close(PS_Items);
}


// Pawn Shop Stuff goes here
void PawnShop(void)
{
	struct item_data *PS_Items[MAX_PSITEMS];
	char *szTheOptions[5];

	LoadStrings(1180, 5, szTheOptions);

	if (!PClan->PawnHelp) {
		PClan->PawnHelp = true;
		Help("Pawn Shop", ST_NEWBIEHLP);
		rputs("\n%P");
	}

	// if no pawn shop, tell him
	if (Village.Data.PawnLevel == 0) {
		rputs("\n|07There is currently no pawn shop in the village.\n%P");
		return;
	}

	// load pawn shop data -- can't use
	PS_Init(PS_Items);

	/* get a choice */
	for (;;) {
		rputs("\n\n");
		switch (GetChoice("Pawn Menu", ST_ENTEROPTION, szTheOptions, "BSVQ?", 'Q', true)) {
			case 'B' :    /* buy */
				PS_Buy(PS_Items);
				break;
			case 'S' :    /* sell items */
				PS_Sell(PS_Items);
				break;
			case 'Q' :    /* return to previous menu */
				PS_Close(PS_Items);
				return;
			case '?' :    /* redisplay options */
				break;
			case 'V' :    /* stats */
				ClanStats(PClan, true);
				break;
		}
	}
}
