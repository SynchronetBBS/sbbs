#ifndef INC_1OOM_MENU_H
#define INC_1OOM_MENU_H

#include "kbd.h"

#define MENU_MAX_ITEMS_PER_PAGE 32

typedef enum {
    MENU_ITEM_TYPE_NONE,
    MENU_ITEM_TYPE_RETURN,
    MENU_ITEM_TYPE_PAGE,
    MENU_ITEM_TYPE_PAGE_BACK,
    MENU_ITEM_TYPE_FUNCTION,
    MENU_ITEM_TYPE_BOOL,
    MENU_ITEM_TYPE_INT,
    MENU_ITEM_TYPE_ENUM,
    MENU_ITEM_TYPE_STR,
} menu_item_type_t;

struct menu_item_data_s {
    menu_item_type_t type;
    const char *text;
    mookey_t key;

    void *value_ptr;
    const char *(*get_text_value) (uint32_t);
    const char *(*get_text_value2)(void);
    bool (*func) (void);
    bool (*is_active) (void);

    uint32_t action_i;
    uint32_t value_min;
    uint32_t value_max;
    bool need_restart;
};

static inline struct menu_item_data_s *menu_item_force_restart(struct menu_item_data_s *d)
{
    d->need_restart = true;
    return d;
}

static inline void menu_make_page(struct menu_item_data_s *d, const char *text, uint32_t page, mookey_t key)
{
    d->type = MENU_ITEM_TYPE_PAGE;
    d->text = text;
    d->key = key;

    d->action_i = page;
}

static inline void menu_make_page_conditional(struct menu_item_data_s *d, const char *text, uint32_t page, bool (*is_active)(void), mookey_t key)
{
    menu_make_page(d, text, page, key);

    d->is_active = is_active;
}

static inline void menu_make_action(struct menu_item_data_s *d, const char *text, uint32_t action, mookey_t key)
{
    d->type = MENU_ITEM_TYPE_RETURN;
    d->text = text;
    d->key = key;

    d->action_i = action;
}

static inline void menu_make_action_conditional(struct menu_item_data_s *d, const char *text, uint32_t action, bool (*is_active)(void), mookey_t key)
{
    menu_make_action(d, text, action, key);

    d->is_active = is_active;
}

static inline void menu_make_func(struct menu_item_data_s *d, const char *text, bool (*func)(void), mookey_t key)
{
    d->type = MENU_ITEM_TYPE_FUNCTION;
    d->text = text;
    d->key = key;

    d->func = func;
}

static inline void menu_make_bool(struct menu_item_data_s *d, const char *text, bool *value_ptr, mookey_t key)
{
    d->type = MENU_ITEM_TYPE_BOOL;
    d->text = text;
    d->key = key;

    d->value_ptr = value_ptr;
}

static inline void menu_make_bool_func(struct menu_item_data_s *d, const char *text, bool *value_ptr, bool (*func)(void), mookey_t key)
{
    menu_make_bool(d, text, value_ptr, key);

    d->func = func;
}

static inline void menu_make_int(struct menu_item_data_s *d, const char *text, uint32_t *value_ptr, uint32_t value_min, uint32_t value_max, mookey_t key)
{
    d->type = MENU_ITEM_TYPE_INT;
    d->text = text;
    d->key = key;

    d->value_ptr = value_ptr;

    d->value_min = value_min;
    d->value_max = value_max;
}

static inline void menu_make_int_func(struct menu_item_data_s *d, const char *text, uint32_t *value_ptr, uint32_t value_min, uint32_t value_max, bool (*func)(void), mookey_t key)
{
    menu_make_int(d, text, value_ptr, value_min, value_max, key);

    d->func = func;
}

static inline void menu_make_enum(struct menu_item_data_s *d, const char *text, const char *(*get_text_value) (uint32_t), uint32_t *value_ptr, uint32_t value_min, uint32_t value_max, mookey_t key)
{
    d->type = MENU_ITEM_TYPE_ENUM;
    d->text = text;
    d->key = key;

    d->value_ptr = value_ptr;
    d->get_text_value = get_text_value;

    d->value_min = value_min;
    d->value_max = value_max;
}

static inline void menu_make_str_func(struct menu_item_data_s *d, const char *text, const char *(*value_ptr)(void), bool (*func)(void), mookey_t key)
{
    d->type = MENU_ITEM_TYPE_STR;
    d->text = text;
    d->key = key;

    d->get_text_value2 = value_ptr;
    d->func = func;
}

static inline void menu_make_back(struct menu_item_data_s *d)
{
    d->type = MENU_ITEM_TYPE_PAGE_BACK;
    d->text = "Back";
    d->key = MOO_KEY_b;
}

extern void menu_clear(void);
extern struct menu_item_data_s *menu_allocate_item(void);
extern const struct menu_item_data_s *menu_get_item(uint32_t i);
extern uint32_t menu_get_item_count(void);

#endif
