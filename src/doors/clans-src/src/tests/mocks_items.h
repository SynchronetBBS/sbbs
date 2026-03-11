/*
 * mocks_items.h
 *
 * Shared mocks for items.h functionality.
 */

#ifndef MOCKS_ITEMS_H
#define MOCKS_ITEMS_H

void Items_Close(void)
{
}

bool ItemPenalty(struct pc *PC, struct item_data *Item)
{
	(void)PC; (void)Item;
	return false;
}

void Items_GiveItem(char *szItemName)
{
	(void)szItemName;
}

void Items_ReadScroll(struct pc *PC, struct clan *TargetClan,
	int16_t Target, int16_t ScrollNum)
{
	(void)PC; (void)TargetClan; (void)Target; (void)ScrollNum;
}

void Item_BuyItem(signed char ItemType)
{
	(void)ItemType;
}

void ShowItemStats(struct item_data *Item, struct clan *Clan)
{
	(void)Item; (void)Clan;
}

void Items_FindTreasureChest(void)
{
}

void ReadBook(void)
{
}

int16_t GetOpenItemSlot(struct clan *Clan)
{
	(void)Clan;
	return -1;
}

int16_t ChooseItem(const char *prompt, struct clan *Clan,
	int16_t type, int filter)
{
	(void)prompt; (void)Clan; (void)type; (void)filter;
	return -1;
}

void UnequipItemsFromPC(int16_t PC)
{
	(void)PC;
}

#endif /* MOCKS_ITEMS_H */
