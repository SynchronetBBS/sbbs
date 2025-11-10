#ifndef THE_CLANS__ITEMS___H
#define THE_CLANS__ITEMS___H

#include "structs.h"

void Items_Close(void);
/*
 * Unloads items.
 */


bool ItemPenalty(struct pc *PC, struct item_data *Item);
/*
 * This function will check to see if PC has the required attributes to
 * use the item.  If he does, false is returned, otherwise, true.
 */

void Items_GiveItem(char *szItemName);
/*
 * This function will give the item with the name specified to
 * PClan.
 */

void Items_ReadScroll(struct pc *PC, struct clan *TargetClan, int16_t Target, int16_t ScrollNum);

void Item_BuyItem(signed char ItemType);

void ShowItemStats(struct item_data *Item, struct clan *Clan);

void Items_FindTreasureChest(void);

void ReadBook(void);

// Results
#define ITEM_NO_MATCH	        -1
#define ITEM_NONE_EQUIPPED      -2
#define ITEM_SELECTED_ABORT     -3
#define ITEM_SELECTED_INVALID   -4
#define ITEM_NOTHING_EQUIPPABLE -5

int16_t GetOpenItemSlot(struct clan *Clan);

// Item filters
#define IF_ANY      1
#define IF_UNUSED   2
#define IF_USED     3
#define IF_CANEQUIP 4
int16_t ChooseItem(const char *prompt, struct clan *Clan, int16_t type, int filter);

void UnequipItemsFromPC(int16_t PC);

#endif
