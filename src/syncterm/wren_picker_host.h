#ifndef WREN_PICKER_HOST_H
#define WREN_PICKER_HOST_H

#include <stdbool.h>

/* Persistent trusted file-picker VM.  It is isolated from both the menu
 * and connected VMs and remains parked except while a picker is active. */
bool wren_picker_host_init(void);
void wren_picker_host_shutdown(void);
bool wren_picker_host_active(void);

#endif
