#ifndef WREN_BIND_MENU_H
#define WREN_BIND_MENU_H

#include <stdbool.h>

#include "wren.h"

WrenForeignMethodFn wren_menu_bind_lookup(const char *module,
    const char *class_name, bool is_static, const char *signature);
WrenForeignClassMethods wren_menu_bind_lookup_class(const char *module,
    const char *class_name);

#endif
