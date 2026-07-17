#ifndef WREN_BIND_MENU_FONTS_H
#define WREN_BIND_MENU_FONTS_H

#include "wren.h"

void wren_menu_fonts_bind_init(void);
void wren_menu_fonts_bind_shutdown(void);
WrenForeignMethodFn wren_menu_fonts_bind_lookup(const char *module,
    const char *class_name, bool is_static, const char *signature);
WrenForeignClassMethods wren_menu_fonts_bind_lookup_class(
    const char *module, const char *class_name);

#endif
