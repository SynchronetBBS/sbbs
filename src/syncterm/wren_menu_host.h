#ifndef WREN_MENU_HOST_H
#define WREN_MENU_HOST_H

#include <stdbool.h>

/* Persistent trusted menu VM.  It is initialized once after conio and the
 * settings model are ready, parked while a connection is active, and freed
 * during application shutdown. */
bool wren_menu_host_init(void);
void wren_menu_host_shutdown(void);
bool wren_menu_host_active(void);

#endif
