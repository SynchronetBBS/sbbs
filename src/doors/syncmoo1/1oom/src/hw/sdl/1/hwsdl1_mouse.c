#include "config.h"

#include "SDL.h"

#include "hwsdl_mouse.c"

void hw_mouse_set_xy(int mx, int my)
{
    mouse_set_xy_from_hw(mx, my);
}
