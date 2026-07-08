#include "config.h"

#include "mouse.h"

/* ------------------------------------------------------------------------- */

static bool mouse_have_click_hw = false;
static bool mouse_have_click_sw = false;

/* ------------------------------------------------------------------------- */

int moouse_x = 0;
int moouse_y = 0;
int mouse_buttons = 0;
int mouse_scroll = 0;
int mouse_stored_x = 0;
int mouse_stored_y = 0;
int mouse_click_x = 0;
int mouse_click_y = 0;
int mouse_click_buttons = 0;
int mouse_cursor_scale = 1;

/* ------------------------------------------------------------------------- */

void mouse_set_xy_from_hw(int mx, int my)
{
    moouse_x = mx;
    moouse_y = my;
}

void mouse_set_buttons_from_hw(int buttons)
{
    buttons &= (MOUSE_BUTTON_MASK_LEFT | MOUSE_BUTTON_MASK_RIGHT);
    mouse_buttons = buttons;
    if (buttons) {
        mouse_click_buttons = buttons;
        mouse_click_x = moouse_x;
        mouse_click_y = moouse_y;
        mouse_have_click_hw = true;
        mouse_have_click_sw = true;
    }
}

void mouse_set_scroll_from_hw(int scroll)
{
    mouse_scroll += scroll;
}

void mouse_set_click_xy(int mx, int my)
{
    mouse_have_click_sw = true;
    mouse_click_x = mx;
    mouse_click_y = my;
}

bool mouse_getclear_click_hw(void)
{
    bool r = mouse_have_click_hw;
    mouse_have_click_hw = false;
    return r;
}

bool mouse_getclear_click_sw(void)
{
    bool r = mouse_have_click_sw;
    mouse_have_click_sw = false;
    return r;
}
