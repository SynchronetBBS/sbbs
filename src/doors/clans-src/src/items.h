
void Items_Init(void);
/*
 * Loads item data into memory.
 */

void Items_Close(void);
/*
 * Unloads items.
 */


__BOOL ItemPenalty(struct pc *PC, struct item_data *Item);
/*
 * This function will check to see if PC has the required attributes to
 * use the item.  If he does, FALSE is returned, otherwise, TRUE.
 */

void Items_GiveItem(char *szItemName);
/*
 * This function will give the item with the name specified to
 * PClan.
 */

void Items_ReadScroll(struct pc *PC, struct clan *TargetClan, _INT16 Target, _INT16 ScrollNum);

void Item_BuyItem(_INT16 ItemType);

void ShowItemStats(struct item_data *Item, struct clan *Clan);

void Items_FindTreasureChest(void);

void ReadBook(void);
