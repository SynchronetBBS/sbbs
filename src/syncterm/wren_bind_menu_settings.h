#ifndef WREN_BIND_MENU_SETTINGS_H
#define WREN_BIND_MENU_SETTINGS_H

#include "wren.h"

void wren_menu_settings_bind_init(void);
WrenForeignMethodFn wren_menu_settings_bind_lookup(const char *module,
    const char *class_name, bool is_static, const char *signature);
WrenForeignClassMethods wren_menu_settings_bind_lookup_class(
    const char *module, const char *class_name);

#endif
