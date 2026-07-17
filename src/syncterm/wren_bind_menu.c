#include "wren_bind_menu.h"

#include <stddef.h>

WrenForeignMethodFn
wren_menu_bind_lookup(const char *module, const char *class_name,
    bool is_static, const char *signature)
{
	(void)module;
	(void)class_name;
	(void)is_static;
	(void)signature;
	return NULL;
}

WrenForeignClassMethods
wren_menu_bind_lookup_class(const char *module, const char *class_name)
{
	WrenForeignClassMethods none = { NULL, NULL };
	(void)module;
	(void)class_name;
	return none;
}
