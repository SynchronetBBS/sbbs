#ifndef INC_1OOM_HWSDL_MOUSE_H
#define INC_1OOM_HWSDL_MOUSE_H

#include "types.h"

extern bool hw_mouse_enabled;

extern void hw_mouse_grab(void);
extern void hw_mouse_ungrab(void);
extern void hw_mouse_toggle_grab(void);

extern void hw_mouse_set_limits(int w, int h);
extern void hw_mouse_set_scale(int w, int h);

extern void hw_mouse_move(int dx, int dy);
extern void hw_mouse_button(int i, int pressed);
extern void hw_mouse_scroll(int scroll);

#endif
