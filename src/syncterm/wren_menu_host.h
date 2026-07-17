#ifndef WREN_MENU_HOST_H
#define WREN_MENU_HOST_H

#include <stdbool.h>

struct bbslist;

/* Persistent trusted menu VM.  It is initialized once after conio and the
 * settings model are ready, parked while a connection is active, and freed
 * during application shutdown. */
bool wren_menu_host_init(void);
void wren_menu_host_shutdown(void);
bool wren_menu_host_active(void);

/* Run the persistent menu controller.  In selection mode, a successful BBS
 * choice is returned as a host-owned copy with the same lifetime contract as
 * show_bbslist(); connected mode is edit-only and always returns NULL. */
struct bbslist *wren_menu_host_run(const char *current, bool connected);

#endif
