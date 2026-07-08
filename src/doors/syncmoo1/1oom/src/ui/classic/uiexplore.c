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

struct explore_data_s {
    struct game_s *g;
    player_id_t api;
    uint8_t planet;
    bool by_scanner;
    bool colony_ship;
    uint8_t *gfx_explobac;
    uint8_t *gfx_contbutt;
    uint8_t *gfx_yn_back;
    uint8_t *gfx_colony;
    void *gmap;
};

static void explore_load_data(struct explore_data_s *d)
{
    const planet_t *p = &(d->g->planet[d->planet]);
    d->gfx_explobac = lbxfile_item_get(LBXFILE_BACKGRND, 0xf);
    d->gfx_contbutt = lbxfile_item_get(LBXFILE_BACKGRND, 0xc);
    d->gfx_yn_back = lbxfile_item_get(LBXFILE_BACKGRND, 0x18);
    d->gfx_colony = lbxfile_item_get(LBXFILE_COLONIES, p->type * 2 + ((p->owner != PLAYER_NONE) ? 1 : 0));
}

static void explore_free_data(struct explore_data_s *d)
{
    lbxfile_item_release(LBXFILE_BACKGRND, d->gfx_explobac);
    lbxfile_item_release(LBXFILE_BACKGRND, d->gfx_contbutt);
    lbxfile_item_release(LBXFILE_BACKGRND, d->gfx_yn_back);
    lbxfile_item_release(LBXFILE_COLONIES, d->gfx_colony);
}

static void explore_draw_planetinfo(const struct game_s *g, uint8_t planet)
{
    const planet_t *p = &(g->planet[planet]);
    int y = 0;
    if (p->type != PLANET_TYPE_NOT_HABITABLE) {
        if (p->growth != PLANET_GROWTH_NORMAL) {
            int i = p->growth;
            if (i > PLANET_GROWTH_NORMAL) {
                --i;
            }
            y = 33;
            lbxfont_select_set_12_4(3, 0, 0, 0);
            lbxfont_print_str_center(267, 92, game_str_ex_pg1[i], UI_SCREEN_W, ui_scale);
            lbxfont_print_str_center(267, 101, game_str_ex_pg2[i], UI_SCREEN_W, ui_scale);
            lbxfont_select_set_12_4(2, 0xa, 0, 0);
            lbxfont_print_str_center(267, 111, game_str_ex_popgr, UI_SCREEN_W, ui_scale);
            lbxfont_print_str_center(267, 118, game_str_ex_pg3[i], UI_SCREEN_W, ui_scale);
        }
        if (p->special != PLANET_SPECIAL_NORMAL) {
            const char *s1, *s2, *s3;
            int i = p->special;
            if (i > PLANET_SPECIAL_NORMAL) {
                if (i == PLANET_SPECIAL_4XTECH) {
                    i = PLANET_SPECIAL_ARTIFACTS;
                }
                --i;
            }
            lbxfont_select_set_12_4(3, 0, 0, 0);
            lbxfont_print_str_center(267, 96 + y, game_str_ex_ps1[i], UI_SCREEN_W, ui_scale);
            lbxfont_select_set_12_4(2, 0xa, 0, 0);
            if ((p->special != PLANET_SPECIAL_ARTIFACTS) && (p->special != PLANET_SPECIAL_4XTECH)) {
                i = p->special;
                if (i > PLANET_SPECIAL_NORMAL) {
                    i -= 2;
                }
                s3 = game_str_ex_ps2[i];
                s1 = game_str_ex_resopnt;
                s2 = game_str_ex_fromind;
            } else {
                s1 = game_str_ex_techpnt;
                s2 = game_str_ex_fromres;
                s3 = (p->special == PLANET_SPECIAL_ARTIFACTS) ? game_str_ex_aredbl : game_str_ex_arequad;
            }
            lbxfont_print_str_center(267, 106 + y, s1, UI_SCREEN_W, ui_scale);
            lbxfont_print_str_center(267, 113 + y, s2, UI_SCREEN_W, ui_scale);
            lbxfont_print_str_center(267, 120 + y, s3, UI_SCREEN_W, ui_scale);
        }
    }
}

static void explore_draw_cb(void *vptr)
{
    struct explore_data_s *d = vptr;
    const struct game_s *g = d->g;
    hw_video_copy_back_from_page2();
    ui_draw_filled_rect(222, 4, 314, 179, 0, ui_scale);
    lbxgfx_draw_frame(222, 4, d->gfx_explobac, UI_SCREEN_W, ui_scale);
    ui_starmap_draw_planetinfo_2(g, d->api, PLAYER_NUM, d->planet);
    lbxgfx_draw_frame(227, 58, d->gfx_colony, UI_SCREEN_W, ui_scale);
    ui_draw_line1(227, 57, 227, 160, 0, ui_scale);
    ui_draw_line1(227, 57, 310, 57, 0, ui_scale);
    ui_draw_line1(310, 57, 310, 160, 0, ui_scale);
    /*game_update_visibility();*/
    ui_gmap_basic_draw_frame(d->gmap, d->api);
    ui_gmap_draw_planet_border(g, d->planet);
    lbxfont_select_set_12_1(3, 0xa, 0, 0);
    if (!d->colony_ship) {
        if (d->by_scanner) {
            lbxfont_print_str_center(267, 58, game_str_ex_planeta, UI_SCREEN_W, ui_scale);
            lbxfont_print_str_center(267, 66, game_str_ex_scanner, UI_SCREEN_W, ui_scale);
            lbxfont_print_str_center(267, 74, game_str_ex_explore, UI_SCREEN_W, ui_scale);
            lbxfont_print_str_center(267, 82, game_str_ex_starsys, UI_SCREEN_W, ui_scale);
        } else {
            lbxfont_print_str_center(267, 60, game_str_ex_scout, UI_SCREEN_W, ui_scale);
            lbxfont_print_str_center(267, 69, game_str_ex_explore, UI_SCREEN_W, ui_scale);
            lbxfont_print_str_center(267, 78, game_str_ex_starsys, UI_SCREEN_W, ui_scale);
        }
    } else {
        lbxfont_print_str_center(267, 60, game_str_ex_build, UI_SCREEN_W, ui_scale);
        lbxfont_print_str_center(267, 69, game_str_ex_colony, UI_SCREEN_W, ui_scale);
        lbxgfx_draw_frame(224, 159, d->gfx_yn_back, UI_SCREEN_W, ui_scale);
    }
    explore_draw_planetinfo(g, d->planet);
}

/* -------------------------------------------------------------------------- */

bool ui_explore(struct game_s *g, int pi, uint8_t planet_i, bool by_scanner, bool flag_colony_ship)
{
    struct explore_data_s d;
    int16_t oi_cont, oi_y, oi_n;
    bool flag_done = false;
    ui_switch_1(g, pi);
    d.g = g;
    d.api = pi;
    d.planet = planet_i;
    d.by_scanner = by_scanner;
    d.colony_ship = flag_colony_ship;
    d.gmap = ui_gmap_basic_init(g, true);
    explore_load_data(&d);
    uiobj_set_callback_and_delay(explore_draw_cb, &d, 4);
    uiobj_table_clear();
    if (!flag_colony_ship) {
        oi_cont = uiobj_add_t0(227, 164, "", d.gfx_contbutt, MOO_KEY_c);
        oi_n = uiobj_add_inputkey(MOO_KEY_SPACE);
        oi_y = UIOBJI_INVALID;
    } else {
        oi_cont = UIOBJI_INVALID;
        oi_n = uiobj_add_t0(227, 163, "", ui_data.gfx.starmap.scrapbut_no, MOO_KEY_n);
        oi_y = uiobj_add_t0(271, 163, "", ui_data.gfx.starmap.scrapbut_yes, MOO_KEY_y);
    }
    while (!flag_done) {
        int16_t oi;
        ui_delay_prepare();
        oi = uiobj_handle_input_cond();
        if ((oi == UIOBJI_ESC) || (oi == oi_cont) || (oi == oi_n)) {
            ui_sound_play_sfx_24();
            flag_done = true;
            flag_colony_ship = false;
        }
        if (oi == oi_y) {
            ui_sound_play_sfx_24();
            flag_done = true;
            ui_landing(g, pi, planet_i);
        }
        if (!flag_done) {
            explore_draw_cb(&d);
            ui_draw_finish();
            ui_delay_ticks_or_click(4);
        }
    }
    uiobj_unset_callback();
    uiobj_table_clear();
    explore_free_data(&d);
    return flag_colony_ship;
}
