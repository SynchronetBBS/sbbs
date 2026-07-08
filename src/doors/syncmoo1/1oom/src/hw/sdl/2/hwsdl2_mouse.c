#include "config.h"
#include "types.h"

#include "SDL.h"

#include "hwsdl2_video.h"

#include "hwsdl_mouse.c"

void hw_mouse_set_xy(int mx, int my)
{
    if (hw_opt_relmouse) {
        mouse_set_xy_from_hw(mx, my);
    } else {
        hw_video_mouse_warp(mx, my);
    }
}
