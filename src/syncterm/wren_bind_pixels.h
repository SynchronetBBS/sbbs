#ifndef WREN_BIND_PIXELS_H
#define WREN_BIND_PIXELS_H

#include <stdbool.h>

#include "wren.h"

WrenForeignMethodFn wren_bind_pixels_lookup(const char *class_name,
    bool is_static, const char *signature);
WrenForeignClassMethods wren_bind_pixels_lookup_class(
    const char *class_name);

#endif
