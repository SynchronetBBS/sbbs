#ifndef WREN_BIND_MENU_H
#define WREN_BIND_MENU_H

#include "wren.h"

#include <stdbool.h>

struct bbslist;

void wren_menu_bind_init(void);
void wren_menu_bind_shutdown(void);

WrenForeignMethodFn wren_menu_bind_lookup(const char *module,
    const char *class_name, bool is_static, const char *signature);
WrenForeignClassMethods wren_menu_bind_lookup_class(const char *module,
    const char *class_name);

/* Validate a menu BBS foreign value while its VM is still selected and copy
 * the C-owned record.  No Wren object or model pointer crosses the VM
 * boundary. */
bool wren_menu_bind_copy_bbs(WrenVM *vm, int slot, struct bbslist *dest);

/* Copy a host record into menu-owned transient storage and place a menu BBS
 * foreign value in slot.  The menu model must already be loaded. */
bool wren_menu_bind_push_transient_bbs(WrenVM *vm, int slot,
                                      const struct bbslist *source);

#endif
