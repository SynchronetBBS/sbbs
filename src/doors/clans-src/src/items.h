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

#endif
