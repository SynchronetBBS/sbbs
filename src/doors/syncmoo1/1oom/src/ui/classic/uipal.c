#include "config.h"

#include <string.h>

#include "uipal.h"
#include "hw.h"
#include "os.h"
#include "lbxpal.h"
#include "types.h"
#include "uidelay.h"

/* -------------------------------------------------------------------------- */

static void fadeout_do(int start, int step, int delay)
{
    for (uint16_t v = start; v < 0x65; v += step) {
        ui_delay_prepare();
        lbxpal_set_update_range(0, 255);
        ui_palette_fade_n(v);
        ui_delay_ticks_or_click(delay);
    }
}

static void fadein_do(int start, int step, int delay)
{
    for (int v = start; v >= 0; v -= step) {
        ui_delay_prepare();
        lbxpal_set_update_range(0, 255);
        ui_palette_fade_n(v);
        ui_delay_ticks_or_click(delay);
    }
}

static void ui_palette_update(void)
{
    memset(lbxpal_update_flag, 0, sizeof(lbxpal_update_flag));
    hw_video_refresh_palette();
}

/* -------------------------------------------------------------------------- */

void ui_palette_set_n(void)
{
    int i, j;
    bool got_update = false;

    /* wait_retrace */
    for (i = j = 0; j < 256; ++j, i += 3) {
        if (lbxpal_update_flag[j] != 0) {
            hw_video_set_palette_color(j, lbxpal_palette[i], lbxpal_palette[i + 1], lbxpal_palette[i + 2]);
            got_update = true;
        }
    }

    if (got_update) {
        ui_palette_update();
    }
}

void ui_palette_fade_n(uint16_t fadepercent)
{
    int v, i, j;

    v = 100 - fadepercent;
    if (v <= 0) {
        /* wait_retrace */
        for (j = 0; j < 256; ++j) {
            if (lbxpal_update_flag[j] != 0) {
                hw_video_set_palette_color(j, 0, 0, 0);
            }
        }
    } else if (v >= 100) {
        ui_palette_set_n();
        return;
    } else {
        v = ((v * 0x100) / 100) & 0xff;
        /* wait_retrace */
        for (i = j = 0; j < 256; ++j, i += 3) {
            if (lbxpal_update_flag[j] != 0) {
                hw_video_set_palette_color(j, (((uint16_t)lbxpal_palette[i]) * v) >> 8,
                                              (((uint16_t)lbxpal_palette[i + 1]) * v) >> 8,
                                              (((uint16_t)lbxpal_palette[i + 2]) * v) >> 8);
            }
        }
    }

    ui_palette_update();
}

void ui_palette_fadeout_19_19_1(void)
{
    fadeout_do(0x19, 0x19, 1);
}

void ui_palette_fadeout_14_14_2(void)
{
    fadeout_do(0x14, 0x14, 2);
}

void ui_palette_fadeout_a_f_1(void)
{
    fadeout_do(0xa, 0xf, 1);
}

void ui_palette_fadeout_4_3_1(void)
{
    fadeout_do(4, 3, 1);
}

void ui_palette_fadeout_5_5_1(void)
{
    fadeout_do(5, 5, 1);
}

void ui_palette_fadein_60_3_1(void)
{
    fadein_do(0x60, 3, 1);
}

void ui_palette_fadein_5f_5_1(void)
{
    fadein_do(0x5f, 5, 1);
}

void ui_palette_fadein_5a_f_1(void)
{
    fadein_do(0x5f, 0xf, 1);
}

void ui_palette_fadein_50_14_2(void)
{
    fadein_do(0x50, 0x14, 2);
}

void ui_palette_fadein_4b_19_1(void)
{
    fadein_do(0x4b, 0x19, 1);
}
