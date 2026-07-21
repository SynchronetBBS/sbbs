#ifndef MENU_SETTINGS_H
#define MENU_SETTINGS_H

#include <stdbool.h>
#include <stddef.h>

#include "syncterm.h"

void menu_settings_snapshot(struct syncterm_settings *snapshot);
bool menu_settings_apply(const struct syncterm_settings *snapshot);
bool menu_settings_save(const struct syncterm_settings *snapshot);
bool menu_settings_select_theme(const char *filename, char *error,
    size_t error_size);
int menu_settings_scaling_mode(const struct syncterm_settings *snapshot);
void menu_settings_set_scaling_mode(struct syncterm_settings *snapshot,
    int mode);

#endif
