#ifndef WREN_PICKER_HOST_H
#define WREN_PICKER_HOST_H

#include <stdbool.h>

struct wren_picker_call;

/* Persistent trusted file-picker VM.  It is isolated from both the menu
 * and connected VMs and remains parked except while a picker is active. */
bool wren_picker_host_init(void);
void wren_picker_host_shutdown(void);

/* Run one request synchronously in the persistent picker VM.  The caller
 * owns the completed paths and releases them with
 * wren_picker_call_dispose(). */
bool wren_picker_host_run(struct wren_picker_call *call);

#endif
