#ifndef INC_1OOM_MOUSE_H
#define INC_1OOM_MOUSE_H

#include "types.h"

#define MOUSE_BUTTON_MASK_LEFT  1
#define MOUSE_BUTTON_MASK_RIGHT 2

/* Allegro uses mouse_x and mouse_y */
extern int moouse_x;
extern int moouse_y;
extern int mouse_buttons;
extern int mouse_scroll;
extern int mouse_stored_x;
extern int mouse_stored_y;
extern int mouse_click_x;
extern int mouse_click_y;
extern int mouse_click_buttons;
extern int mouse_cursor_scale;

extern void mouse_set_xy_from_hw(int mx, int my);
extern void mouse_set_buttons_from_hw(int buttons);
extern void mouse_set_scroll_from_hw(int scroll);
extern void mouse_set_click_xy(int mx, int my);

extern bool mouse_getclear_click_hw(void);
extern bool mouse_getclear_click_sw(void);

#endif
