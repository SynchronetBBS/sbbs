#include "config.h"

#include <stdio.h>

#include "uistarmap.h"
#include "comp.h"
#include "game.h"
#include "game_fleet.h"
#include "game_misc.h"
#include "game_num.h"
#include "game_save.h"
#include "game_str.h"
#include "kbd.h"
#include "lbxgfx.h"
#include "lbxfont.h"
#include "lib.h"
#include "log.h"
#include "types.h"
#include "uicursor.h"
#include "uidraw.h"
#include "uidefs.h"
#include "uidelay.h"
#include "uiobj.h"
#include "uisearch.h"
#include "uisound.h"
#include "uistarmap_common.h"

/* -------------------------------------------------------------------------- */

static void ui_starmap_orbit_en_draw_cb(void *vptr)
{
    struct starmap_data_s *d = vptr;
    const struct game_s *g = d->g;
    const planet_t *p = &g->planet[d->from];
    const empiretechorbit_t *e = &(g->eto[d->oe.player]);
    char buf[0x80];
    STARMAP_LIM_INIT();

    ui_starmap_draw_basic(d);
    {
        int x, y;
        x = (p->x - ui_data.starmap.x) * 2 + 23;
        y = (p->y - ui_data.starmap.y) * 2 + 5 + d->oe.yoff;
        lbxgfx_draw_frame_offs_delay(x, y, !d->anim_delay, ui_data.gfx.starmap.shipbord, STARMAP_LIMITS, UI_SCREEN_W, starmap_scale);
    }
    ui_draw_filled_rect(225, 8, 314, 180, 7, ui_scale);
    lbxgfx_draw_frame(224, 4, ui_data.gfx.starmap.movextr2, UI_SCREEN_W, ui_scale);
    ui_draw_filled_rect(227, 8, 310, 39, 0, ui_scale);
    lbxgfx_set_frame_0(ui_data.gfx.starmap.scanner);
    for (int f = 0; f <= ui_data.starmap.frame_scanner; ++f) {
        lbxgfx_draw_frame(227, 8, ui_data.gfx.starmap.scanner, UI_SCREEN_W, ui_scale);
    }
    lib_sprintf(buf, sizeof(buf), "%s %s", game_str_tbl_race[e->race], game_str_sm_fleet);
    lbxfont_select_set_12_4(5, tbl_banner_fontparam[e->banner], 0, 0);
    lbxfont_print_str_center(267, 10, buf, UI_SCREEN_W, ui_scale);
    lbxfont_select_set_12_4(0, 0, 0, 0);
    lbxfont_print_str_center(268, 33, game_str_sm_inorbit, UI_SCREEN_W, ui_scale);
    for (int i = 0; i < d->oe.sn0.num; ++i) {
        const shipdesign_t *sd = &(g->srd[d->oe.player].design[0]);
        uint8_t *gfx;
        int st, x, y;
        x = 228 + (i % 2) * 43;
        y = 44 + (i / 2) * 40;
        ui_draw_filled_rect(x, y, x + 38, y + 24, 0, ui_scale);
        ui_draw_filled_rect(x, y + 28, x + 38, y + 34, 0x1c, ui_scale);
        ui_data.starmap.stars_xoff1 = 0;
        ui_data.starmap.stars_xoff2 = 0;
        ui_draw_stars(x, y, 0, 38, ui_scale);
        st = d->oe.sn0.type[i];
        gfx = ui_data.gfx.ships[sd[st].look];
        lbxgfx_set_frame_0(gfx);
        lbxgfx_draw_frame(x, y, gfx, UI_SCREEN_W, ui_scale);
        lbxfont_select(0, 0xd, 0, 0);
        lbxfont_print_num_right(x + 35, y + 19, d->oe.ships[st], UI_SCREEN_W, ui_scale);
        lbxfont_select(2, 0xa, 0, 0);
        lbxfont_print_str_center(x + 19, y + 29, sd[st].name, UI_SCREEN_W, ui_scale);
    }
    if (!d->anim_delay) {
        if (ui_data.starmap.scanner_delay == 0) {
            ui_data.starmap.frame_scanner = (ui_data.starmap.frame_scanner + 1) % 20;
            ++ui_data.starmap.scanner_delay;
        } else {
            ui_data.starmap.scanner_delay = 0;
        }
    }
}

/* -------------------------------------------------------------------------- */

void ui_starmap_orbit_en(struct game_s *g, player_id_t active_player)
{
    bool flag_done = false;
    int16_t oi_search;
    int16_t oi_f4, oi_f5;
    struct starmap_data_s d;
    shipcount_t *os;

    ui_starmap_common_init(g, &d, active_player);
    ui_data.starmap.frame_scanner = 0;
    ui_data.starmap.scanner_delay = 0;
    d.from = g->planet_focus_i[active_player];
    d.oe.player = ui_data.starmap.orbit_player;
    os = &(g->eto[d.oe.player].orbit[d.from].ships[0]);
    for (int i = 0; i < NUM_SHIPDESIGNS; ++i) {
        d.oe.ships[i] = os[i];
    }
    ui_starmap_sn0_setup(&d.oe.sn0, NUM_SHIPDESIGNS, d.oe.ships);
    {
        int n = 0;
        for (player_id_t i = PLAYER_0; i < d.oe.player; ++i) {
            os = &(g->eto[i].orbit[d.from].ships[0]);
            for (int j = 0; j < g->eto[i].shipdesigns_num; ++j) {
                if (os[j] != 0) {
                    ++n;
                    break;
                }
            }
        }
        d.oe.yoff = n * 6;
    }

    uiobj_table_clear();

#define UIOBJ_CLEAR_LOCAL() \
    do { \
        STARMAP_UIOBJ_CLEAR_COMMON(); \
        oi_f4 = UIOBJI_INVALID; \
        oi_f5 = UIOBJI_INVALID; \
    } while (0)

    UIOBJ_CLEAR_LOCAL();

    uiobj_set_callback_and_delay(ui_starmap_orbit_en_draw_cb, &d, STARMAP_DELAY);

    while (!flag_done) {
        int16_t oi1, oi2;
        oi1 = uiobj_handle_input_cond();
        oi2 = uiobj_at_cursor();
        ui_delay_prepare();
        ui_starmap_handle_scrollkeys(&d, oi1);
        if (ui_starmap_handle_oi_bottom_buttons(&d, oi1)) {
            flag_done = true;
            ui_sound_play_sfx_24();
        } else if (ui_starmap_handle_oi_misc(&d, oi1)) {
            ui_sound_play_sfx_24();
        } else if (oi1 == oi_search) {
            ui_sound_play_sfx_24();
            ui_search_set_pos(g, active_player);
        }
        for (int i = 0; i < g->enroute_num; ++i) {
            if (oi1 == d.oi_tbl_enroute[i]) {
                ui_data.starmap.fleet_selected = i;
                ui_data.ui_main_loop_action = UI_MAIN_LOOP_ENROUTE_SEL;
                ui_sound_play_sfx_24();
                flag_done = true;
                break;
            }
        }
        for (int i = 0; i < g->transport_num; ++i) {
            if (oi1 == d.oi_tbl_transport[i]) {
                ui_data.starmap.fleet_selected = i;
                ui_data.ui_main_loop_action = UI_MAIN_LOOP_TRANSPORT_SEL;
                ui_sound_play_sfx_24();
                flag_done = true;
                break;
            }
        }
        for (int i = 0; i < g->galaxy_stars; ++i) {
            for (player_id_t j = PLAYER_0; j < g->players; ++j) {
                if (oi1 == d.oi_tbl_pl_stars[j][i]) {
                    g->planet_focus_i[active_player] = i;
                    d.from = i;
                    ui_data.starmap.orbit_player = j;
                    ui_data.ui_main_loop_action = (j == active_player) ? UI_MAIN_LOOP_ORBIT_OWN_SEL : UI_MAIN_LOOP_ORBIT_EN_SEL;
                    ui_sound_play_sfx_24();
                    flag_done = true;
                    j = g->players; i = g->galaxy_stars;
                }
            }
        }
        if (oi1 == UIOBJI_ESC) {
            ui_sound_play_sfx_24();
            flag_done = true;
            ui_data.ui_main_loop_action = UI_MAIN_LOOP_STARMAP;
        } else if (oi1 == oi_f4) {
            bool found;
            int i, pi;
            i = pi = d.from;
            found = false;
            do {
                i = (i + 1) % g->galaxy_stars;
                for (int j = 0; j < g->eto[ui_data.starmap.orbit_player].shipdesigns_num; ++j) {
                    if (g->eto[ui_data.starmap.orbit_player].orbit[i].ships[j]
                     && BOOLVEC_IS1(g->planet[i].within_srange, d.api)) {
                        found = true;
                        break;
                    }
                }
            } while ((!found) && (i != pi));
            if (found) {
                g->planet_focus_i[active_player] = i;
                ui_starmap_set_pos_focus(g, active_player);
                d.from = i;
                ui_sound_play_sfx_24();
                flag_done = true;
            }
        } else if (oi1 == oi_f5) {
            bool found;
            int i, pi;
            i = pi = d.from;
            found = false;
            do {
                if (--i < 0) { i = g->galaxy_stars - 1; }
                for (int j = 0; j < g->eto[ui_data.starmap.orbit_player].shipdesigns_num; ++j) {
                    if (g->eto[ui_data.starmap.orbit_player].orbit[i].ships[j]
                     && BOOLVEC_IS1(g->planet[i].within_srange, d.api)) {
                        found = true;
                        break;
                    }
                }
            } while ((!found) && (i != pi));
            if (found) {
                g->planet_focus_i[active_player] = i;
                ui_starmap_set_pos_focus(g, active_player);
                d.from = i;
                ui_sound_play_sfx_24();
                flag_done = true;
            }
        }
        ui_starmap_handle_oi_ctrl(&d, oi1);
        for (int i = 0; i < g->galaxy_stars; ++i) {
            if ((oi1 == d.oi_tbl_stars[i]) && !g->evn.build_finished_num[active_player]) {
                g->planet_focus_i[active_player] = i;
                ui_sound_play_sfx_24();
                ui_data.ui_main_loop_action = UI_MAIN_LOOP_STARMAP;
                flag_done = true;
                break;
            }
        }
        if (!flag_done) {
            ui_starmap_common_update_mouse_hover(&d, oi2);
            ui_starmap_orbit_en_draw_cb(&d);
            uiobj_table_clear();
            UIOBJ_CLEAR_LOCAL();
            oi_f4 = uiobj_add_inputkey(MOO_KEY_F4);
            oi_f5 = uiobj_add_inputkey(MOO_KEY_F5);
            ui_starmap_fill_oi_tbls(&d);
            ui_starmap_fill_oi_tbl_stars(&d);
            oi_search = uiobj_add_inputkey(MOO_KEY_SLASH);
            ui_starmap_fill_oi_ctrl(&d);
            ui_starmap_add_oi_bottom_buttons(&d);
            ui_starmap_add_oi_misc(&d);
            ui_draw_finish();
            ui_delay_ticks_or_click(STARMAP_DELAY);
        }
    }
    uiobj_unset_callback();
    uiobj_table_clear();
}
