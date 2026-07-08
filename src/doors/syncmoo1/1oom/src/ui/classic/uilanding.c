#include "config.h"

#include <ctype.h>
#include <stdio.h>

#include "uilanding.h"
#include "comp.h"
#include "game.h"
#include "game_str.h"
#include "gfxaux.h"
#include "hw.h"
#include "kbd.h"
#include "lbx.h"
#include "lbxfont.h"
#include "lbxgfx.h"
#include "lbxpal.h"
#include "lib.h"
#include "log.h"
#include "types.h"
#include "uidelay.h"
#include "uidefs.h"
#include "uidraw.h"
#include "uiobj.h"
#include "uipal.h"
#include "uisound.h"
#include "uiswitch.h"
#include "util.h"

/* -------------------------------------------------------------------------- */

static void landing_draw_cb1(void *vptr)
{
    struct landing_data_s *d = vptr;
    const struct game_s *g = d->g;
    int y = 100;
    bool do_walk = false;
    char buf[0x80];
    hw_video_copy_back_from_page3();
    if (d->frame < 0x32) {
        y = d->frame * 4 - 100;
    } else {
        lbxgfx_set_new_frame(d->gfx_transprt, 0x27);
        if (d->frame >= 0x37) {
            do_walk = true;
        }
    }
    gfx_aux_draw_frame_to(d->gfx_transprt, &ui_data.aux.screen);
    gfx_aux_draw_frame_from_limit(0, y, &ui_data.aux.screen, 0, 0, UI_VGA_W - 1, UI_VGA_H - 1, UI_SCREEN_W, ui_scale);
    if (do_walk) {
        lbxgfx_draw_frame(0, 0, d->gfx_walk, UI_SCREEN_W, ui_scale);
        ui_draw_filled_rect(115, 81, 204, 109, 0xa, ui_scale);
        ui_draw_box1(115, 81, 204, 109, 0x34, 0x34, ui_scale);
        ui_draw_box1(121, 95, 195, 106, 0x34, 0x34, ui_scale);
        lbxfont_select(5, 0xf, 0, 0);
        lbxfont_print_str_normal(124, 84, game_str_la_colony, UI_SCREEN_W, ui_scale);
        /*uiobj_handle_ta_sub1(120, 80, 180, 90, 0); does nothing */
    }
    lib_sprintf(buf, sizeof(buf), "%s %i %s %s%s", game_str_la_inyear, g->year + YEAR_BASE, game_str_la_the, game_str_tbl_race[g->eto[d->api].race], game_str_la_formnew);
    lbxfont_select_set_12_4(4, 0x5, 0, 0);
    lbxfont_print_str_center(160, 5, buf, UI_SCREEN_W, ui_scale);
    if (++d->frame == (0x41 + 100)) {
        d->frame = 0x41;
    }
}

/* -------------------------------------------------------------------------- */

void ui_landing_prepare(struct landing_data_s *d)
{
    const struct game_s *g = d->g;
    const planet_t *p = &(g->planet[d->planet]);
    ui_delay_prepare();
    ui_draw_erase_buf();
    ui_delay_ticks_or_click(1);
    ui_sound_stop_music();
    {
        uint8_t *gfx;
        int i = p->type;
        i += d->colonize ? 6 : 0x24;
        gfx = lbxfile_item_get(LBXFILE_LANDING, i);
        lbxgfx_draw_frame(0, 0, gfx, UI_SCREEN_W, ui_scale);
        lbxgfx_apply_palette(gfx);
        lbxfile_item_release(LBXFILE_LANDING, gfx);
    }
    hw_video_copy_back_to_page3();
    d->gfx_transprt = lbxfile_item_get(LBXFILE_LANDING, 0x0);
    d->gfx_walk = lbxfile_item_get(LBXFILE_LANDING, 0x15 + g->eto[d->api].banner);
    lbxgfx_set_frame_0(d->gfx_walk);
    d->music_i = d->colonize ? 0xa : 8;
}

void ui_landing_free_data(struct landing_data_s *d)
{
    lbxfile_item_release(LBXFILE_LANDING, d->gfx_transprt);
    lbxfile_item_release(LBXFILE_LANDING, d->gfx_walk);
}

void ui_landing(struct game_s *g, int pi, uint8_t planet_i)
{
    struct landing_data_s d;
    bool flag_done = false;
    ui_switch_1(g, pi);
    memset(&d, 0, sizeof(d));
    d.g = g;
    d.api = pi;
    d.planet = planet_i;
    d.frame = 0;
    d.colonize = (g->planet[planet_i].owner == PLAYER_NONE);
    if (ui_draw_finish_mode != 2) {
        ui_palette_fadeout_a_f_1();
    }
    lbxpal_select(6, -1, 0);
    ui_landing_prepare(&d);
    ui_sound_play_music(d.music_i);
    lbxpal_build_colortables();
    ui_draw_finish_mode = 2;
    uiobj_set_callback_and_delay(landing_draw_cb1, &d, 2);
    uiobj_table_clear();
    uiobj_add_mousearea_all(MOO_KEY_SPACE);
    uiobj_set_downcount(3);
    while (d.frame < 0x41) {
        int16_t oi;
        ui_delay_prepare();
        oi = uiobj_handle_input_cond();
        if (oi != 0) {
            d.frame = 0x41;
            flag_done = true;
        }
        if (!flag_done) {
            landing_draw_cb1(&d);
            ui_draw_finish();
            ui_delay_ticks_or_click(2);
        }
    }
    if (d.planet != g->evn.planet_orion_i) {
        const uint8_t ctbl[8] = { 0x34, 0x34, 0x34, 0x34, 0x34, 0x34, 0x34, 0x34 };
        char buf[PLANET_NAME_LEN];
        lib_strcpy(buf, g->planet[planet_i].name, sizeof(buf));
        lbxfont_select(5, 0xf, 0xf, 0);
        if (uiobj_read_str(125, 97, 65, buf, PLANET_NAME_LEN - 1, 0, false, ctbl)) {
            util_trim_whitespace(buf, sizeof(buf));
            if (buf[0] != 0) {
                lib_strcpy(g->planet[planet_i].name, buf, PLANET_NAME_LEN);
            }
        }
    }
    ui_sound_stop_music();
    ui_palette_fadeout_a_f_1();
    lbxpal_select(0, -1, 0);
    lbxpal_build_colortables();
    ui_draw_finish_mode = 2;
    uiobj_unset_callback();
    uiobj_table_clear();
    ui_landing_free_data(&d);
}
