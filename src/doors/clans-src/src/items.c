
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
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "unix_wrappers.h"
#include "win_wrappers.h"

#include <OpenDoor.h>

#include "clansini.h"
#include "door.h"
#include "game.h"
#include "help.h"
#include "input.h"
#include "items.h"
#include "language.h"
#include "mstrings.h"
#include "myopen.h"
#include "random.h"
#include "spells.h"
#include "structs.h"
#include "user.h"
#include "village.h"

#define MT_POLYMETRAL   1
#define MT_LACONIA      2


static struct {
	bool Initialized;
	int16_t NumItems;

	struct item_data *Data[MAX_ITEMS];
} Items = { false, 0 };
static void Items_Init(void);

// ------------------------------------------------------------------------- //
void Items_FindTreasureChest(void)
{
	char *RandomTable, szString[128], szItemName[25];
	signed char CurItem;
	int16_t CurLevel, RandIndex, ChosenItem;

	Items_Init();

	RandomTable = malloc((size_t)Items.NumItems * 10);
	CheckMem(RandomTable);

	// generate random table
	for (CurItem = 0, RandIndex = 0; CurItem < Items.NumItems; CurItem++) {
		for (CurLevel = 0; CurLevel < Items.Data[CurItem]->RandLevel; CurLevel++) {
			RandomTable[ RandIndex++ ] = CurItem;
		}
	}

	// choose one of those items
	ChosenItem = RandomTable[ my_random(RandIndex)];

	// give him that item
	// snprintf(szString, sizeof(szString), "|07You find |15%s|07. |0STake it?", Items[ ChosenItem ]->szName);
	snprintf(szString, sizeof(szString), ST_TREASURE0, Items.Data[ ChosenItem ]->szName);

	strlcpy(szItemName, Items.Data[ ChosenItem ]->szName, sizeof(szItemName));
	free(RandomTable);
	Items_Close();

	if (YesNo(szString) == YES) {
		// give item reloads items!
		Items_GiveItem(szItemName);
	}
}

// ------------------------------------------------------------------------- //
void ReadBook(void)
{
	int16_t DefaultItemIndex, iTemp, ItemIndex, WhichMember;
	char cKey, szString[128];

	// which book?

	// see if anything to read
	for (iTemp = 0; iTemp < MAX_ITEMS_HELD; iTemp++) {
		if (PClan->Items[iTemp].Available)
			break;
	}
	if (iTemp == MAX_ITEMS_HELD) {
		rputs(ST_ISTATS2);
		return;
	}

	/* find first item in inventory which is a book */
	for (iTemp = 0; iTemp < MAX_ITEMS_HELD; iTemp++) {
		if (PClan->Items[iTemp].Available &&
				PClan->Items[iTemp].cType == I_BOOK)
			break;
	}
	if (iTemp == MAX_ITEMS_HELD) {
		rputs("|04No books found.\n");
		return;
	}
	else
		DefaultItemIndex = iTemp+1;

	ItemIndex = (int16_t) GetLong(ST_ISTATS3, DefaultItemIndex, MAX_ITEMS_HELD);
	if (ItemIndex == 0)
		return;
	ItemIndex--;

	/* if that item is non-existant, tell him */
	if (PClan->Items[ItemIndex].Available == false ||
			PClan->Items[ItemIndex].cType != I_BOOK) {
		rputs(ST_INVALIDITEM);
		return;
	}
	ShowItemStats(&PClan->Items[ItemIndex], PClan);

	// choose who will read it
	rputs("\n");
	for (iTemp = 0; iTemp < MAX_MEMBERS; iTemp++) {
		if (PClan->Member[iTemp] && iTemp < Game.Data.MaxPermanentMembers) {
			snprintf(szString, sizeof(szString), "|0A(|0B%c|0A) |0C%s\n",
					iTemp + 'A', PClan->Member[iTemp]->szName);
			rputs(szString);
		}
	}
	rputs("|0SWho will read it? |04[Enter=abort] |0F");

	for (;;) {
		cKey = toupper(od_get_key(true) & 0x7f) & 0x7f;

		if (cKey == '\r' || cKey == '\n') {
			rputs(ST_ABORTED);
			return;
		}

		WhichMember = cKey - 'A';

		if (WhichMember >= 0 && WhichMember < Game.Data.MaxPermanentMembers) {
			if (PClan->Member[ WhichMember ]) {
				rputs(PClan->Member[ WhichMember ]->szName);
				rputs("\n");
				break;
			}
		}
	}

	// see what can read
	if (ItemPenalty(PClan->Member[ WhichMember], &PClan->Items[ItemIndex])) {
		snprintf(szString, sizeof(szString), "%s cannot read that book!\n",
				PClan->Member[ WhichMember]->szName);
		rputs(szString);
		return;
	}

	// find next spell index open AND see if spell already known
	for (iTemp = 0; iTemp < MAX_SPELLS; iTemp++) {
		if (PClan->Member[ WhichMember]->SpellsKnown[iTemp]-1 ==
				PClan->Items[ItemIndex].SpellNum &&
				PClan->Items[ItemIndex].SpellNum != -1) {
			// already know this spell
			rputs("|0CThat character already knows that spell.\n");
			return;
		}

		if (PClan->Member[ WhichMember]->SpellsKnown[iTemp] == 0)
			break;
	}

	if (iTemp == MAX_SPELLS) {
		rputs("|07That characters knows already too many spells.\n");
		return;
	}

	// list who can read it

	// make him read it NOW
	snprintf(szString, sizeof(szString), "|0B%s |0Creads the book...\n",
			PClan->Member[ WhichMember ]->szName);
	rputs(szString);

	if (PClan->Items[ ItemIndex ].SpellNum != -1 && PClan->Items[ ItemIndex ].SpellNum < MAX_SPELLS) {
		snprintf(szString, sizeof(szString), "|0C%s learns the |0B%s |0Cspell.\n",
				PClan->Member[ WhichMember ]->szName,
				Spells[ PClan->Items[ ItemIndex ].SpellNum ]->szName);
		rputs(szString);

		// learn it
		PClan->Member[ WhichMember ]->SpellsKnown[iTemp] =
			(char)(PClan->Items[ ItemIndex ].SpellNum + 1);
	}

	// if HPadd
	if (PClan->Items[ ItemIndex ].HPAdd) {
		snprintf(szString, sizeof(szString), "|0C%s gains |0B%d HP\n",
				PClan->Member[ WhichMember ]->szName,
				PClan->Items[ ItemIndex ].HPAdd);
		rputs(szString);

		PClan->Member[ WhichMember ]->MaxHP +=
			PClan->Items[ ItemIndex ].HPAdd;

		PClan->Member[ WhichMember ]->HP =
			PClan->Member[ WhichMember ]->MaxHP;
	}

	// if SPadd
	if (PClan->Items[ ItemIndex ].SPAdd) {
		snprintf(szString, sizeof(szString), "|0C%s gains |0B%d SP\n",
				PClan->Member[ WhichMember ]->szName,
				PClan->Items[ ItemIndex ].SPAdd);
		rputs(szString);

		PClan->Member[ WhichMember ]->MaxSP +=
			PClan->Items[ ItemIndex ].SPAdd;

		PClan->Member[ WhichMember ]->SP =
			PClan->Member[ WhichMember ]->MaxSP;
	}

	// get rid of item
	PClan->Items[ ItemIndex ].Available = false;

}


// ------------------------------------------------------------------------- //
bool ItemPenalty(struct pc *PC, struct item_data *Item)
/*
 * This function will check to see if PC has the required attributes to
 * use the item.  If he does, false is returned, otherwise, true.
 */
{
	int16_t iTemp;
	bool Penalty = false;

	for (iTemp = 0; iTemp < NUM_ATTRIBUTES; iTemp++)
		if (PC->Attributes[iTemp] < Item->ReqAttributes[iTemp]
				&& Item->ReqAttributes[iTemp])
			Penalty = true;

	return Penalty;
}

// ------------------------------------------------------------------------- //

static void ItemUseableBy(struct item_data *Item)
/*
 * This function outputs who in PClan may use the item.
 *
 */
{
	int16_t CurMember/*, iTemp*/;
	char szString[128];
	struct pc *PC;
	bool ShownOne;

	rputs("\n|0CThe following members may use this item:\n|0B");

	// go through each member
	ShownOne = false;    // haven't shown at least one member yet
	for (CurMember = 0; CurMember < MAX_MEMBERS; CurMember++) {
		if (PClan->Member[CurMember] == NULL)
			continue;

		// if he meets the requirements, display him
		PC = PClan->Member[CurMember];

		if (ItemPenalty(PC, Item) == false) {
			if (ShownOne)
				rputs(", ");

			snprintf(szString, sizeof(szString), "%s", PC->szName);
			rputs(szString);

			ShownOne = true;
		}
	}
	if (ShownOne)
		rputs("\n");
	else
		rputs("No one\n");
}


void ShowItemStats(struct item_data *Item, struct clan *Clan)
/*
 * This function displays the stats for the item AND who is using it
 * within Clan AND who can use it within PClan.
 */
{
	char *szAttributeNames[NUM_ATTRIBUTES];
	int16_t iTemp/*, AttributesShown*/;
	char szString[128]/*, szFullString[128]*/;
	int16_t CurBonus, CurReq, CurLine;
	char *szAttr[NUM_ATTRIBUTES] = {
		"AGI",
		"DEX",
		"STR",
		"WIS",
		"ARM",
		"CHR"
	};
	char szLine[128];
	char *szIcon[6];

	switch (Item->cType) {
		case I_WEAPON :
			LoadStrings(1210, 6, szIcon);
			break;
		case I_ARMOR :
			LoadStrings(1216, 6, szIcon);
			break;
		case I_SHIELD :
			LoadStrings(1222, 6, szIcon);
			break;
		case I_SCROLL :
			LoadStrings(1228, 6, szIcon);
			break;
		case I_BOOK :
			LoadStrings(1234, 6, szIcon);
			break;
		case I_OTHER :
			LoadStrings(1240, 6, szIcon);
			break;
	}

	CurBonus = 0;
	CurReq = 0;

	LoadStrings(10, 6, szAttributeNames);

	// show first lines here
	rputs("\n");
	snprintf(szString, sizeof(szString), "|0B %s\n", Item->szName);
	rputs(szIcon[0]);
	rputs(szString);
	rputs(szIcon[1]);
	rputs("|0A\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\xc4\n");
	rputs(szIcon[2]);

	if (Item->cType != I_SCROLL && Item->cType != I_BOOK &&
			Item->cType != I_OTHER) {
		rputs("|15 Modifiers        |12Requirements\n");

		for (CurLine = 0; CurLine < 3; CurLine++) {
			strlcpy(szLine, " ", sizeof(szLine));

			for (iTemp = 0; iTemp < 2; iTemp++) {
				// find stat modified
				for (; CurBonus < NUM_ATTRIBUTES; CurBonus++) {
					if (Item->Attributes[CurBonus])
						break;  // found one
				}

				if (CurBonus != NUM_ATTRIBUTES) {
					if (Item->Attributes[CurBonus] < 0)
						snprintf(szString, sizeof(szString), "|04%2d %s ",
								Item->Attributes[CurBonus], szAttr[CurBonus]);
					else if (Item->Attributes[CurBonus] > 0)
						snprintf(szString, sizeof(szString), "|0C%2d %s ",
								Item->Attributes[CurBonus], szAttr[CurBonus]);

					CurBonus++;
				}
				else strlcpy(szString, "       ", sizeof(szString));

				strlcat(szLine, szString, sizeof(szLine));
			}

			strlcat(szLine, "   ", sizeof(szLine));

			for (iTemp = 0; iTemp < 2; iTemp++) {
				// find next requirement
				for (; CurReq < NUM_ATTRIBUTES; CurReq++) {
					if (Item->ReqAttributes[CurReq])
						break;  // found one
				}

				if (CurReq != NUM_ATTRIBUTES) {
					snprintf(szString, sizeof(szString), "|0C%2d %s ", Item->ReqAttributes[CurReq], szAttr[CurReq]);
					CurReq++;
				}
				else szString[0] = 0;

				strlcat(szLine, szString, sizeof(szLine));
			}

			// display line now
			rputs(szIcon[CurLine+3]); // icon first
			rputs(szLine);
			rputs("\n");
		}
	}
	else if (Item->cType == I_SCROLL) {
		snprintf(szString, sizeof(szString), " |0B%d Use(s) Left\n", Item->Energy);
		rputs(szString);

		for (CurLine = 3; CurLine < 6; CurLine++) {
			rputs(szIcon[CurLine]);
			rputs("\n");
		}
	}
	else if (Item->cType == I_BOOK || Item->cType == I_OTHER) {
		if (Item->cType == I_BOOK)
			rputs(" |0CHave a wizard examine this.\n");
		else
			rputs("\n");

		for (CurLine = 3; CurLine < 6; CurLine++) {
			// display icon only
			rputs(szIcon[CurLine]);
			rputs("\n");
		}
	}

	// show who's using it right now
	if (Item->UsedBy && Clan) {
		snprintf(szString, sizeof(szString), "\n|0CUsed by:  |0B%s\n", Clan->Member[Item->UsedBy - 1]->szName);
		rputs(szString);
	}

	// show who can use this without penalty
	ItemUseableBy(Item);
}


// ------------------------------------------------------------------------- //

static void Items_Read(void)
{
	int16_t iTemp, NumItems, CurFile, CurItem;
	struct FileHeader ItemFile;
	uint8_t sBuf[BUF_SIZE_item_data];

	for (CurItem = 0; CurItem < MAX_ITEMS; CurItem++)
		Items.Data[CurItem] = NULL;


	// for each file, read in the data
	for (CurItem = 0, CurFile = 0; CurFile < MAX_ITEMFILES; CurFile++) {
		if (IniFile.pszItems[CurFile] == NULL)
			break;

		MyOpen(IniFile.pszItems[CurFile], "rb", &ItemFile);
		if (ItemFile.fp == NULL) continue;

		// get num items in file
		fread(&NumItems, sizeof(int16_t), 1, ItemFile.fp);
		NumItems = SWAP16S(NumItems);

		// read each item in file
		for (iTemp = 0; iTemp < NumItems; iTemp++) {
			Items.Data[CurItem] = malloc(sizeof(struct item_data));
			CheckMem(Items.Data[CurItem]);

			fread(sBuf, sizeof(sBuf), 1, ItemFile.fp);
			s_item_data_d(sBuf, sizeof(sBuf), Items.Data[CurItem]);

			CurItem++;
			if (CurItem == MAX_ITEMS) break;
		}
		fclose(ItemFile.fp);

		if (CurItem == MAX_ITEMS) break;
	}

	Items.NumItems = CurItem;
}

// ------------------------------------------------------------------------- //

static void Items_Destroy(void)
{
	int16_t CurItem;

	for (CurItem = 0; CurItem < MAX_ITEMS; CurItem++)
		if (Items.Data[CurItem]) {
			free(Items.Data[CurItem]);
			Items.Data[CurItem] = NULL;
		}

	Items.NumItems = 0;
}

// ------------------------------------------------------------------------- //
void Items_GiveItem(char *szItemName)
/*
 * This function will give the item with the name specified to
 * PClan.
 */
{
	int16_t iTemp, EmptySlot/*, ItemIndex*/;

	// see if he has room to carry it
	for (iTemp = 0; iTemp < MAX_ITEMS_HELD; iTemp++) {
		if (PClan->Items[iTemp].Available == false)
			break;
	}
	if (iTemp == MAX_ITEMS_HELD) {
		/* no more room in inventory */
		rputs(ST_ITEMNOMOREROOM);
		return;
	}
	else
		EmptySlot = iTemp;

	// load items
	Items_Init();

	// see which item we're referring to
	for (iTemp = 0; iTemp < MAX_ITEMS; iTemp++)
		if (strcasecmp(Items.Data[iTemp]->szName, szItemName) == 0)
			break;

	if (iTemp == MAX_ITEMS) {
		rputs("Item not found in file!\n");
		Items_Close();
		return;
	}

	PClan->Items[EmptySlot] = *Items.Data[iTemp];
	PClan->Items[EmptySlot].UsedBy = 0;
	PClan->Items[EmptySlot].Available = true;

	Items_Close();
}



// ------------------------------------------------------------------------- //

static void Items_Init(void)
/*
 * Loads item data into memory.
 */
{
	// items already loaded
	if (Items.Initialized) return;

	Items.Initialized = true;

	Items_Read();
}

// ------------------------------------------------------------------------- //

void Items_Close(void)
/*
 * Unloads items.
 */
{
	if (!Items.Initialized) return;

	Items_Destroy();
	Items.Initialized = false;
}


/* read scroll */
void Items_ReadScroll(struct pc *PC, struct clan *TargetClan, int16_t Target, int16_t ScrollNum)
{
	char szString[128];

	// use up scroll
	snprintf(szString, sizeof(szString), "\xaf\xaf\xaf |14%s |07reads |12%s\n",
			PC->szName, PC->MyClan->Items[ ScrollNum ].szName);
	rputs(szString);

	// reduce energy
	PC->MyClan->Items[ ScrollNum ].Energy--;

	// if depleted
	if (PC->MyClan->Items[ ScrollNum ].Energy == 0) {
		snprintf(szString, sizeof(szString), "\xaf\xaf\xaf |07%s vanishes\n", PC->MyClan->Items[ ScrollNum ].szName);
		rputs(szString);
		PC->MyClan->Items[ ScrollNum ].Available = false;
	}

	Spells_CastSpell(PC, TargetClan, Target, PC->MyClan->Items[ScrollNum].SpellNum);
}

void Item_BuyItem(signed char ItemType)
{
	int16_t ItemIndex[MAX_ITEMS];
	int16_t iTemp, TotalItems;
	signed char cTemp;
	char szString[255];
	char szKeys[MAX_ITEMS + 6], Choice;
	int32_t ItemCosts[MAX_ITEMS], /*NewCost,*/ ItemGst[MAX_ITEMS];
	char MaterialChoice = 0;
	int16_t EmptySlot;
	struct item_data Item;

	Items_Init();

	/* so we have szKeys[] = "?Q\r ABCDEFGHI..." */
	szKeys[0] = '?';
	szKeys[1] = 'Q';
	szKeys[2] = '\r';
	szKeys[3] = '\n';
	szKeys[4] = ' ';
	for (cTemp = 0; cTemp < MAX_ITEMS; cTemp++)
		szKeys[cTemp + 5] = 'A' + cTemp;
	szKeys[cTemp+5] = 0;

	/* generate index of items which are the item type */
	for (iTemp = 0, TotalItems = 0; iTemp < MAX_ITEMS; iTemp++) {
		// make sure item is even allowed to be bought from here
		// FIXME: use WizardLevel for books+scrolls?
		if (Items.Data[iTemp] &&
				Items.Data[iTemp]->cType == ItemType && Items.Data[iTemp]->Special == false
				&&
				((Items.Data[iTemp]->MarketLevel <= Village.Data.MarketLevel &&
				  (ItemType == I_WEAPON || ItemType == I_ARMOR || ItemType == I_SHIELD))
				 ||
				 (Items.Data[iTemp]->MarketLevel <= Village.Data.WizardLevel &&
				  (ItemType == I_SCROLL || ItemType == I_BOOK)))

				&& (Items.Data[iTemp]->VillageType == Village.Data.VillageType ||
					Items.Data[iTemp]->VillageType == V_ALL)) {
			/* found item type */
			ItemIndex[TotalItems] = iTemp;

			// cost fluctuation... FIXME: in future, have different ones for
			// spells+books??
			ItemCosts[TotalItems] = Items.Data[  ItemIndex[TotalItems] ]->lCost
									+ ((int32_t)Village.Data.CostFluctuation*Items.Data[ItemIndex[TotalItems] ]->lCost)/100L;

			ItemGst[TotalItems] = ((ItemCosts[TotalItems] * Village.Data.GST)/100L);

			/* add gst */
			ItemCosts[TotalItems] += ItemGst[TotalItems];
			TotalItems++;
		}
	}
	szKeys[TotalItems+5] = 0;


	/* get input */

	for (;;) {
		/* header stuff */
		if (ItemType == I_WEAPON)
			rputs(ST_WEAPONSAVAILABLE);
		else if (ItemType == I_ARMOR)
			rputs(ST_ARMORAVAILABLE);
		else if (ItemType == I_SHIELD)
			rputs(ST_SHIELDSAVAILABLE);

		rputs(ST_LONGLINE);
		rputs(ST_ITEMHEADER);
		/* list which ones to buy */
		for (iTemp = 0; iTemp < TotalItems; iTemp++) {
			snprintf(szString, sizeof(szString), ST_ITEMLINE, 'A' + iTemp,
					PClan->Empire.VaultGold >= Items.Data[ ItemIndex[iTemp] ]->lCost ? "|0C" : "|08",
					Items.Data[ ItemIndex[iTemp] ]->szName, ItemCosts[iTemp]);
			rputs(szString);
		}
		snprintf(szString, sizeof(szString), ST_ITEMOPTIONS, PClan->Empire.VaultGold);
		rputs(szString);
		rputs(ST_LONGLINE);
		rputs(ST_ITEMPROMPT);

		Choice = od_get_answer(szKeys);

		rputs("\b\b\b\b\b     \b\b\b\b\b|15");

		if (Choice == '?') {
			rputs(ST_ITEMHELPTITLE);
			if (ItemType == I_SCROLL || ItemType == I_BOOK)
				rputs("|07 The old wizard keeps to himself and tells you nothing about the items.\n%P\n\n");
			else
				GeneralHelp(ST_ITEMHLP);
		}
		else if (Choice == 'Q' || Choice == '\r' || Choice == ' ' || Choice == '\n') {
			rputs(ST_ITEMABORTED);
			Items_Close();
			return;
		}
		else {
			rputs(Items.Data[ ItemIndex[Choice - 'A'] ]->szName);
			rputs("\n\n");

			/* give info on item */
			if (Items.Data[ ItemIndex[Choice - 'A'] ]->cType != I_BOOK &&
					Items.Data[ ItemIndex[Choice - 'A'] ]->cType != I_SCROLL &&
					Items.Data[ ItemIndex[Choice - 'A'] ]->cType != I_OTHER)
				Help(Items.Data[ ItemIndex[Choice - 'A'] ]->szName, ST_ITEMHLP);

			snprintf(szString, sizeof(szString), ST_ITEMCOSTIS, ItemCosts[ Choice - 'A'], PClan->Empire.VaultGold);
			rputs(szString);

			/* ask if he still wants to buy it */
			if (YesNo(ST_ITEMBUYTHIS) == YES) {
				rputs("\n");

				/* bought.  see if user has room for item and can afford */
				/* if can't, kick him out */
				for (iTemp = 0; iTemp < MAX_ITEMS_HELD; iTemp++) {
					if (PClan->Items[iTemp].Available == false)
						break;
				}
				if (iTemp == MAX_ITEMS_HELD) {
					/* no more room in inventory */
					rputs(ST_ITEMNOMOREROOM);
					Items_Close();
					return;
				}
				else
					EmptySlot = iTemp;

				/* see if can afford it */
				if (PClan->Empire.VaultGold < ItemCosts[Choice - 'A']) {
					rputs(ST_ITEMNOAFFORD);
					continue;
				}

				/* choose type of material */
				if (Items.Data[ ItemIndex[Choice - 'A'] ]->DiffMaterials) {
					for (;;) {
						/* show two types */
						rputs(ST_ITEMMATERIALS);
						/* prompt for one */
						rputs(ST_ITEMCHOOSEONE);

						/* if chooses more expensive type, tell him that it costs x% more */
						MaterialChoice = od_get_answer("AB\r ");

						if (MaterialChoice == 'A' || MaterialChoice == '\r' || MaterialChoice == ' ') {
							rputs("\b\b\b\b\b\b\b\b\b\b|15Polymetral\n");
							Help("Polymetral", ST_ITEMHLP);

							/* ask if he wants to use this */
							if (YesNo("\n|0SUse this material?") == YES) {
								/* see if he can afford it */
								//NewCost = (ItemCosts[Choice - 'A'] * (my_random(Village.Data.MarketLevel*10) + 90 + my_random(10)))/100L;

								if (PClan->Empire.VaultGold < ItemCosts[Choice - 'A']) {
									rputs("|12You cannot afford this!\n");
									if (NoYes("|0SUse Laconia instead?") == NO) {
										Items_Close();
										return;
									}

									/* so use other material */
									MaterialChoice = MT_LACONIA;
									break;
								}
								else {
									/* can afford it, use it! */
									MaterialChoice = MT_POLYMETRAL;
									break;
								}
							}
						}
						else {
							rputs("\b\b\b\b\b\b\b\b\b\b|15Laconia   \n");
							Help("Laconia", ST_ITEMHLP);

							/* ask if he wants to use this */
							if (YesNo("\n|0SUse this material?") == YES) {
								/* see if he can afford it under this type of */
								//NewCost = ItemCosts[Choice - 'A'];

								if (PClan->Empire.VaultGold < ItemCosts[Choice - 'A']) {
									rputs("|12You cannot afford this!\n");
									if (NoYes("|0SUse PolyMetral instead?") == NO) {
										Items_Close();
										return;
									}

									/* so use other material */
									MaterialChoice = MT_POLYMETRAL;
									break;
								}
								else {
									/* can afford it, use it! */
									MaterialChoice = MT_LACONIA;
									break;
								}
							}
						}
					}
				}

				/* take user's money */
				PClan->Empire.VaultGold -= ItemCosts[Choice - 'A'];

				/* give GST to vault */
				Village.Data.Empire.VaultGold += ItemGst[Choice - 'A'];

				/* make stats here -------- */
				strlcpy(Item.szName, Items.Data[ ItemIndex[Choice - 'A'] ]->szName, sizeof(Item.szName));
				Item = *Items.Data[ ItemIndex[Choice - 'A'] ];

				Item.UsedBy = 0;
				Item.Available = true;
				Item.cType = ItemType;

				/* set stats */
				for (iTemp = 0; iTemp < NUM_ATTRIBUTES; iTemp++) {
					if (ItemType == I_WEAPON || ItemType == I_ARMOR || ItemType == I_SHIELD) {
						if (Item.Attributes[iTemp])
							Item.Attributes[iTemp] =
								(char)(Items.Data[ ItemIndex[Choice - 'A'] ]->Attributes[iTemp] + my_random(Village.Data.MarketLevel / 3 + 1) + Village.Data.MarketLevel / 2);
					}
					else { // scroll or book
						if (Item.Attributes[iTemp])
							Item.Attributes[iTemp] =
								(char)(Items.Data[ ItemIndex[Choice - 'A'] ]->Attributes[iTemp] + my_random(Village.Data.WizardLevel / 3 + 1) + Village.Data.WizardLevel / 2);
					}

					Item.ReqAttributes[iTemp] =
						Items.Data[ ItemIndex[Choice - 'A'] ]->ReqAttributes[iTemp];
				}

				/* if using a material, change stats accordingly */
				if (MaterialChoice == MT_POLYMETRAL) {
					/* make it lighter by decreasing req attributes */
					if (Item.ReqAttributes[ATTR_STRENGTH])
						Item.ReqAttributes[ATTR_STRENGTH] -= (char)(my_random(3) + 1);
					if (Item.Attributes[ATTR_STRENGTH])
						Item.Attributes[ATTR_AGILITY] += (char)my_random(2);
					if (Item.Attributes[ATTR_DEXTERITY])
						Item.Attributes[ATTR_DEXTERITY] += (char)my_random(2);

					/* make it have less energy -- it'll have between 0 and 20% less energy */
					Item.Energy = (int16_t)((Item.Energy * (my_random(20) + 80)) / 100);
				}
				else if (MaterialChoice == MT_LACONIA) {
					/* make it stronger by giving higher strength */
					if (Item.ReqAttributes[ATTR_STRENGTH])
						Item.ReqAttributes[ATTR_STRENGTH] += (char)(my_random(3) + 1);

					/* and increase strength attribute */
					if (Item.Attributes[ATTR_STRENGTH])
						Item.Attributes[ATTR_STRENGTH] += (char)(my_random(2) + 1);

					/* make it have longer lifespan but higher strength reqattrib */
					Item.Energy = (int16_t)((Item.Energy * (my_random(40) + 90)) / 100);
				}

				// make it super item (+)
				if (my_random(50) == 0 && ItemType != I_SCROLL && ItemType != I_BOOK) {
					// all stats go up by 25%
					for (iTemp = 0; iTemp < NUM_ATTRIBUTES; iTemp++) {
						Item.Attributes[iTemp] += (Item.Attributes[iTemp]/4);
					}

					// cost doubles
					Item.lCost = 2*ItemCosts[Choice - 'A'];

					rputs("Blacksmith creates a super item!!\n");
					strlcat(Item.szName, "+", sizeof(Item.szName));
				}
				else    // regular cost
					Item.lCost = ItemCosts[Choice - 'A'];


				// add onto it due to Quality Level
				if (Village.Data.MarketQuality)
					for (iTemp = 0; iTemp < NUM_ATTRIBUTES; iTemp++) {
						Item.Attributes[iTemp] += (char)((Item.Attributes[iTemp] * (my_random(20) + (Village.Data.MarketQuality * 50) / 4)) / 100);

						if (Item.ReqAttributes[iTemp])
							Item.ReqAttributes[iTemp] -= (char)((Item.ReqAttributes[iTemp]*(my_random(10) + (Village.Data.MarketQuality * 30) / 4)) / 100);
					}

				/* ------------------------ */

				/* show stats */
				ShowItemStats(&Item, NULL);

				/* ask if user wants to buy still.  if so, give it to user, take rest of money */
				if (YesNo("|0SBuy it?") == YES) {
					/* add to items list */
					rputs("\n|0BYou take the item.\n");

					PClan->Items[EmptySlot] = Item;
				}
				else {
					// rputs("\n|0BYou are refunded half the cost.\n");
					PClan->Empire.VaultGold += ((Item.lCost*75)/100);

					/* get GST back from gov't */
					Village.Data.Empire.VaultGold -= (ItemGst[Choice - 'A']/2);
				}
				continue;
			}
			rputs("\n");
		}
	}
}
