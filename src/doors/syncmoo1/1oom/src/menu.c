#include <string.h>

#include "menu.h"

static struct menu_item_data_s menu_items[MENU_MAX_ITEMS_PER_PAGE];
static uint32_t menu_item_count;

void menu_clear(void)
{
    memset(menu_items, 0, sizeof(menu_items));
    menu_item_count = 0;
}

struct menu_item_data_s *menu_allocate_item(void)
{
    struct menu_item_data_s *ret = &menu_items[menu_item_count];
    ++menu_item_count;
    return ret;
}

const struct menu_item_data_s *menu_get_item(uint32_t i)
{
    struct menu_item_data_s *ret = NULL;
    if (i < menu_item_count) {
        ret = &menu_items[i];
    }
    return ret;
}

uint32_t menu_get_item_count(void)
{
    return menu_item_count;
}
