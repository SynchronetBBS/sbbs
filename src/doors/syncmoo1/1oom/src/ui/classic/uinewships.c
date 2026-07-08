#include "config.h"

#include <ctype.h>
#include <stdio.h>

#include "ui.h"
#include "comp.h"
#include "game.h"
#include "game_str.h"
#include "hw.h"
#include "kbd.h"
#include "lbx.h"
#include "lbxfont.h"
#include "lbxgfx.h"
#include "lib.h"
#include "log.h"
#include "types.h"
#include "uidelay.h"
#include "uidefs.h"
#include "uidraw.h"
#include "uigmap.h"
#include "uilanding.h"
#include "uiobj.h"
#include "uisound.h"
#include "uistarmap_common.h"
#include "uiswitch.h"

/* -------------------------------------------------------------------------- */

struct newships_data_s {
    struct game_s *g;
    player_id_t api;
    uint8_t *gfx_newship;
    struct starmap_data_s sm;
};

static void newships_load_data(struct newships_data_s *d)
{
    d->gfx_newship = lbxfile_item_get(LBXFILE_BACKGRND, 0x14);
}

static void newships_free_data(struct newships_data_s *d)
{
    lbxfile_item_release(LBXFILE_BACKGRND, d->gfx_newship);
}

static void newships_draw_cb(void *vptr)
{
    struct newships_data_s *d = vptr;
    const struct game_s *g = d->g;
    int x = 38, y = 27;
    char buf[0x20];
    ui_starmap_draw_basic(&d->sm);
    ui_draw_filled_rect(x, y, x + 151, y + 128, 0x2b, ui_scale);
    lbxgfx_draw_frame(x, y, d->gfx_newship, UI_SCREEN_W, ui_scale);
    lbxfont_select(5, 6, 0, 0);
    lbxfont_set_color_c_n(0x49, 5);
    lib_sprintf(buf, sizeof(buf), "%s%i", game_str_year, g->year + YEAR_BASE);
    lbxfont_print_str_center(x + 76, y + 9, buf, UI_SCREEN_W, ui_scale);
    for (int i = 0; i < NUM_SHIPDESIGNS; ++i) {
        shipsum_t n;
        n = g->evn.new_ships[d->api][i];
        if (n != 0) {
            const shipdesign_t *sd = &(g->srd[d->api].design[i]);
            uint8_t *gfx;
            int x0, y0;
            x0 = x + 8 + (i % 3) * 48;
            y0 = y + 35 + (i / 3) * 47;
            ui_draw_filled_rect(x0, y0, x0 + 39, y0 + 39, 0, ui_scale);
            ui_draw_filled_rect(x0, y0 + 31, x0 + 39, y0 + 39, 0xe9, ui_scale);
            ui_draw_line1(x0, y0 + 30, x0 + 39, y0 + 30, 0x5c, ui_scale);
            lbxfont_select(2, 0, 0, 0);
            lbxfont_print_str_center(x0 + 20, y0 + 33, sd->name, UI_SCREEN_W, ui_scale);
            ui_draw_stars(x0, y0 + 2, i * 10, 40, ui_scale);
            gfx = ui_data.gfx.ships[sd->look];
            lbxgfx_set_frame_0(gfx);
            lbxgfx_draw_frame(x0 + 4, y0 + 3, gfx, UI_SCREEN_W, ui_scale);
            lbxfont_select(0, 0xd, 0, 0);
            lbxfont_print_num_right(x0 + 36, y0 + 23, n, UI_SCREEN_W, ui_scale);
        }
    }
    ui_draw_set_stars_xoffs(false);
}

/* -------------------------------------------------------------------------- */

void ui_newships(struct game_s *g, int pi)
{
    struct newships_data_s d;
    bool flag_done = false;
    int tempnum;
    tempnum = g->evn.build_finished_num[pi];
    g->evn.build_finished_num[pi] = 0;
    ui_switch_1(g, pi);
    d.g = g;
    d.api = pi;
    ui_starmap_common_init(g, &d.sm, pi);
    d.sm.bottom_highlight = -1;
    newships_load_data(&d);
    uiobj_set_callback_and_delay(newships_draw_cb, &d, 4);
    uiobj_table_clear();
    uiobj_add_mousearea_all(MOO_KEY_SPACE);
    while (!flag_done) {
        int16_t oi;
        ui_delay_prepare();
        oi = uiobj_handle_input_cond();
        if (oi != 0) {
            flag_done = true;
        }
        if (!flag_done) {
            newships_draw_cb(&d);
            ui_draw_finish();
            ui_delay_ticks_or_click(4);
        }
    }
    ui_sound_play_sfx_24();
    ui_delay_1();
    uiobj_unset_callback();
    uiobj_table_clear();
    newships_free_data(&d);
    g->evn.build_finished_num[pi] = tempnum;
}
