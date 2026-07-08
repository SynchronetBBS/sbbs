#include "config.h"

#include <stdio.h>

#include "uistarmap.h"
#include "comp.h"
#include "game.h"
#include "game_fleet.h"
#include "game_misc.h"
#include "game_num.h"
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
#include "util.h"

/* -------------------------------------------------------------------------- */

static void ui_starmap_transport_draw_cb(void *vptr)
{
    struct starmap_data_s *d = vptr;
    const struct game_s *g = d->g;
    const transport_t *r = &(g->transport[ui_data.starmap.fleet_selected]);
    const empiretechorbit_t *e = &(g->eto[r->owner]);
    const planet_t *pt = &g->planet[g->planet_focus_i[d->api]];
    char buf[0x80];
    STARMAP_LIM_INIT();

    ui_starmap_draw_basic(d);
    {
        int x, y;
        x = (r->x - ui_data.starmap.x) * 2 + 5;
        y = (r->y - ui_data.starmap.y) * 2 + 5;
        lbxgfx_draw_frame_offs_delay(x, y, !d->anim_delay, ui_data.gfx.starmap.shipbord, STARMAP_LIMITS, UI_SCREEN_W, starmap_scale);
    }
    ui_draw_filled_rect(225, 8, 314, 180, 7, ui_scale);
    lbxgfx_draw_frame(224, 4, ui_data.gfx.starmap.tranbord, UI_SCREEN_W, ui_scale);
    if (d->controllable) {
        lbxgfx_draw_frame(224, 159, ui_data.gfx.starmap.tranxtra, UI_SCREEN_W, ui_scale);
    }
    ui_draw_filled_rect(227, 8, 310, 39, 0, ui_scale);
    lbxgfx_set_frame_0(ui_data.gfx.starmap.scanner);
    for (int f = 0; f <= ui_data.starmap.frame_scanner; ++f) {
        lbxgfx_draw_frame(227, 8, ui_data.gfx.starmap.scanner, UI_SCREEN_W, ui_scale);
    }
    lib_sprintf(buf, sizeof(buf), "%s %s", game_str_tbl_race[e->race], game_str_sm_fleet);
    lbxfont_select_set_12_4(5, tbl_banner_fontparam[e->banner], 0, 0);
    lbxfont_print_str_center(267, 10, buf, UI_SCREEN_W, ui_scale);
    if (d->show_planet_focus) {
        const planet_t *pd = &(g->planet[r->dest]);
        uint8_t *gfx;
        int x0, y0, x1, y1, dist;
        bool dest_ok = true;
        x1 = (pt->x - ui_data.starmap.x) * 2 + 8;
        y1 = (pt->y - ui_data.starmap.y) * 2 + 8;
        lbxgfx_draw_frame_offs_delay(x1, y1, !d->anim_delay, ui_data.gfx.starmap.planbord, STARMAP_LIMITS, UI_SCREEN_W, starmap_scale);
        x0 = (r->x - ui_data.starmap.x) * 2 + 8;
        y0 = (r->y - ui_data.starmap.y) * 2 + 8;
        if (d->controllable) {
            dest_ok = game_transport_dest_ok(g, pt, d->api);
        }
        {
            const uint8_t *ctbl;
            ctbl = dest_ok ? colortbl_line_green : colortbl_line_red;
            ui_draw_line_limit_ctbl(x0 + 4, y0 + 1, x1 + 6, y1 + 6, ctbl, 5, ui_data.starmap.line_anim_phase, starmap_scale);
        }
        if (ui_extra_enabled && ui_data.starmap.flag_show_own_routes && (r->owner == d->api)) {
            int x2, y2;
            x2 = (pd->x - ui_data.starmap.x) * 2 + 8;
            y2 = (pd->y - ui_data.starmap.y) * 2 + 8;
            ui_draw_line_limit_ctbl(x0 + 4, y0 + 1, x2 + 6, y2 + 6, colortbl_line_green, 5, ui_data.starmap.line_anim_phase, starmap_scale);
        }
        gfx = ui_data.gfx.starmap.smaltran[e->banner];
        if (pd->x < r->x) {
            lbxgfx_set_new_frame(gfx, 1);
        } else {
            lbxgfx_set_frame_0(gfx);
        }
        lbxgfx_draw_frame_offs(x0, y0, gfx, STARMAP_LIMITS, UI_SCREEN_W, starmap_scale);
        dist = game_get_min_dist(g, r->owner, g->planet_focus_i[d->api]);
        if (d->controllable && (pt->within_frange[d->api] == 0)) {
            /* FIXME use proper positioning for varying str length */
            d->ts.in_frange = false;
            lib_sprintf(buf, sizeof(buf), "  %s   %i %s.", game_str_sm_outsr, dist - e->fuel_range, game_str_sm_parsecs2);
            lbxfont_select_set_12_4(2, 0, 0, 0);
            lbxfont_set_gap_h(2);
            lbxfont_print_str_split(230, 26, 80, buf, 2, UI_SCREEN_W, UI_SCREEN_H, ui_scale);
        } else {
            int eta = game_calc_eta_trans(g, r->speed, pt->x, pt->y, r->x, r->y);
            d->ts.in_frange = true;
            lib_sprintf(buf, sizeof(buf), "%s %i %s", game_str_sm_eta, eta, (eta == 1) ? game_str_sm_turn : game_str_sm_turns);
            lbxfont_select_set_12_4(0, 0, 0, 0);
            lbxfont_print_str_center(268, 32, buf, UI_SCREEN_W, ui_scale);
        }
        if (!dest_ok) {
            d->ts.in_frange = false;
        }
    } else {
        /*6a51c*/
        d->ts.in_frange = false;
    }
    {
        int x = 228, y = 73;
        ui_draw_filled_rect(x, y, x + 81, y + 25, 0, ui_scale);
        ui_draw_stars(x, y, 0, 80, ui_scale);
        lbxgfx_set_frame_0(ui_data.gfx.starmap.tranship);
        for (int f = 0; f <= ui_data.starmap.frame_ship; ++f) {
            lbxgfx_draw_frame(x + 7, y + 3, ui_data.gfx.starmap.tranship, UI_SCREEN_W, ui_scale);
        }
    }
    lbxfont_select_set_12_4(0, 5, 0, 0);
    lib_sprintf(buf, sizeof(buf), "%i %s", r->pop, game_str_sm_colony);
    lbxfont_print_str_center(267, 48, buf, UI_SCREEN_W, ui_scale);
    lbxfont_print_str_center(267, 57, (r->pop == 1) ? game_str_sm_trans1 : game_str_sm_transs, UI_SCREEN_W, ui_scale);
    if (d->show_planet_focus) {
        const planet_t *pd = &(g->planet[r->dest]);
        lbxfont_select_set_12_1(5, 0, 0, 0);
        lbxfont_print_str_center(267, 110, game_str_sm_tdest, UI_SCREEN_W, ui_scale);
        lbxfont_print_str_center(267, 120, pd->name, UI_SCREEN_W, ui_scale);
    }
    if (d->controllable && (!d->ts.in_frange)) {
        lbxgfx_set_new_frame(ui_data.gfx.starmap.reloc_bu_accept, 1);
        lbxgfx_draw_frame(271, 163, ui_data.gfx.starmap.reloc_bu_accept, UI_SCREEN_W, ui_scale);
    }
    if (!d->anim_delay) {
        if (ui_data.starmap.scanner_delay == 0) {
            ui_data.starmap.frame_scanner = (ui_data.starmap.frame_scanner + 1) % 20;
            ++ui_data.starmap.scanner_delay;
        } else {
            ui_data.starmap.scanner_delay = 0;
        }
        ui_data.starmap.frame_ship = (ui_data.starmap.frame_ship + 1) % 5;
        ui_draw_set_stars_xoffs(false);
    }
}

/* -------------------------------------------------------------------------- */

static void ui_starmap_transport_set_pos_focus(const struct game_s *g, player_id_t player_i)
{
    const transport_t *r = &g->transport[ui_data.starmap.fleet_selected];
    ui_starmap_set_pos(g, r->x, r->y);
}

static int ui_starmap_transport_next(const struct game_s *g, player_id_t pi, int i)
{
    const transport_t *r = &g->transport[i];
    player_id_t owner = r->owner;
    int start = i;
    do {
        i = (i + 1) % g->transport_num;
        r = &g->transport[i];
        if (r->owner == owner && BOOLVEC_IS1(r->visible, pi)) {
            return i;
        }
    } while (i != start);
    return g->transport_num;
}

static int ui_starmap_transport_prev(const struct game_s *g, player_id_t pi, int i)
{
    const transport_t *r = &g->transport[i];
    player_id_t owner = r->owner;
    int start = i;
    do {
        if (--i < 0) { i = g->transport_num - 1; }
        r = &g->transport[i];
        if (r->owner == owner && BOOLVEC_IS1(r->visible, pi)) {
            return i;
        }
    } while (i != start);
    return g->transport_num;
}

void ui_starmap_transport(struct game_s *g, player_id_t active_player)
{
    bool flag_done = false;
    int16_t oi_cancel, oi_accept, oi_search;
    int16_t oi_f4, oi_f5;
    struct starmap_data_s d;
    transport_t *r = &(g->transport[ui_data.starmap.fleet_selected]);

    ui_starmap_common_init(g, &d, active_player);
    d.show_planet_focus = ((r->owner == d.api) || (g->eto[d.api].have_ia_scanner));
    d.set_pos_focus = ui_starmap_transport_set_pos_focus;
    d.ts.in_frange = false;
    ui_data.starmap.frame_ship = 0;
    ui_data.starmap.frame_scanner = 0;
    ui_data.starmap.scanner_delay = 0;
    d.from = g->planet_focus_i[active_player];
    g->planet_focus_i[active_player] = r->dest;
    d.controllable = g->eto[active_player].have_hyperspace_comm && (r->owner == active_player);

    uiobj_table_clear();

#define UIOBJ_CLEAR_LOCAL() \
    do { \
        STARMAP_UIOBJ_CLEAR_COMMON(); \
        oi_accept = UIOBJI_INVALID; \
        oi_cancel = UIOBJI_INVALID; \
        oi_f4 = UIOBJI_INVALID; \
        oi_f5 = UIOBJI_INVALID; \
    } while (0)

    UIOBJ_CLEAR_LOCAL();

    uiobj_set_help_id(3);
    uiobj_set_callback_and_delay(ui_starmap_transport_draw_cb, &d, STARMAP_DELAY);

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
            if (ui_search_set_pos(g, active_player)) {
                if (!d.controllable) {
                    d.from = g->planet_focus_i[active_player];
                    flag_done = true;
                    ui_data.ui_main_loop_action = UI_MAIN_LOOP_STARMAP;
                }
            }
        } else if ((oi1 == oi_cancel) || (oi1 == UIOBJI_ESC)) {
            ui_sound_play_sfx_06();
            flag_done = true;
            ui_data.ui_main_loop_action = UI_MAIN_LOOP_STARMAP;
        } else if (oi1 == oi_f4) {
            int i = ui_starmap_transport_next(g, active_player, ui_data.starmap.fleet_selected);
            if (i != g->transport_num) {
                ui_data.starmap.fleet_selected = i;
                flag_done = true;
                ui_sound_play_sfx_24();
            }
        } else if (oi1 == oi_f5) {
            int i = ui_starmap_transport_prev(g, active_player, ui_data.starmap.fleet_selected);
            if (i != g->transport_num) {
                ui_data.starmap.fleet_selected = i;
                flag_done = true;
                ui_sound_play_sfx_24();
            }
        } else if (oi1 == oi_accept) {
do_accept:
            ui_sound_play_sfx_24();
            if (g->planet[g->planet_focus_i[active_player]].within_frange[active_player] != 0) { /* FIXME allows redirecting no nonexplored planets */
                r->dest = g->planet_focus_i[active_player];
                flag_done = true;
            }
            ui_data.ui_main_loop_action = UI_MAIN_LOOP_STARMAP;
        }
        if (!d.controllable || ui_sm_explicit_cursor_context) {
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
        }
        ui_starmap_handle_oi_ctrl(&d, oi1);
        for (int i = 0; i < g->galaxy_stars; ++i) {
            if (oi1 == d.oi_tbl_stars[i]) {
                if (ui_extra_enabled && (oi_accept != UIOBJI_INVALID) && (g->planet_focus_i[active_player] == i)) {
                    oi1 = oi_accept;
                    goto do_accept;
                }
                g->planet_focus_i[active_player] = i;
                if (!d.controllable) {
                    d.from = i;
                    flag_done = true;
                    ui_data.ui_main_loop_action = UI_MAIN_LOOP_STARMAP;
                }
                break;
            }
        }
        if (!flag_done) {
            ui_starmap_common_update_mouse_hover(&d, oi2);
            ui_starmap_transport_draw_cb(&d);
            uiobj_table_clear();
            UIOBJ_CLEAR_LOCAL();
            oi_f4 = uiobj_add_inputkey(MOO_KEY_F4);
            oi_f5 = uiobj_add_inputkey(MOO_KEY_F5);
            if (!ui_sm_explicit_cursor_context || kbd_is_modifier(MOO_MOD_ALT) || !d.controllable) {
                ui_starmap_fill_oi_tbls(&d);
                ui_cursor_setup_area(2, &ui_cursor_area_tbl[3]);
            } else {
                ui_cursor_setup_area(2, &ui_cursor_area_tbl[1]);
            }
            ui_starmap_fill_oi_tbl_stars(&d);
            if (d.controllable) {
                oi_cancel = uiobj_add_t0(227, 163, "", ui_data.gfx.starmap.reloc_bu_cancel, MOO_KEY_ESCAPE);
                if (game_transport_dest_ok(g, &(g->planet[g->planet_focus_i[active_player]]), active_player)) {
                    oi_accept = uiobj_add_t0(271, 163, "", ui_data.gfx.starmap.reloc_bu_accept, MOO_KEY_SPACE);
                }
            }
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
    uiobj_set_help_id(-1);
    g->planet_focus_i[active_player] = d.from;
}
