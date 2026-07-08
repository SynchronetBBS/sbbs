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

static inline bool ui_starmap_enroute_in_frange(struct starmap_data_s *d)
{
    uint8_t pi = d->g->planet_focus_i[d->api];
    const planet_t *p = &d->g->planet[pi];
    return ((p->within_frange[d->api] == 1) || ((p->within_frange[d->api] == 2) && d->en.sn0.have_reserve_fuel));
}

static inline bool ui_starmap_enroute_locked_by_retreat(struct starmap_data_s *d, uint8_t planet_i)
{
    const fleet_enroute_t *r = &(d->g->enroute[ui_data.starmap.fleet_selected]);
    return (game_num_retreat_redir_fix && r->retreat && !d->g->eto[d->api].have_hyperspace_comm && (planet_i == d->en.pon));
}

static void ui_starmap_enroute_draw_cb(void *vptr)
{
    struct starmap_data_s *d = vptr;
    const struct game_s *g = d->g;
    const fleet_enroute_t *r = &(g->enroute[ui_data.starmap.fleet_selected]);
    const empiretechorbit_t *e = &(g->eto[r->owner]);
    uint8_t pto = g->planet_focus_i[d->api];
    const planet_t *pt = &g->planet[pto];
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
    lbxgfx_draw_frame(224, 4, ui_data.gfx.starmap.movextr2, UI_SCREEN_W, ui_scale);
    if (d->controllable) {
        lbxgfx_draw_frame(224, 160, ui_data.gfx.starmap.movextr3, UI_SCREEN_W, ui_scale);
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
        x1 = (pt->x - ui_data.starmap.x) * 2 + 8;
        y1 = (pt->y - ui_data.starmap.y) * 2 + 8;
        lbxgfx_draw_frame_offs_delay(x1, y1, !d->anim_delay, ui_data.gfx.starmap.planbord, STARMAP_LIMITS, UI_SCREEN_W, starmap_scale);
        x0 = (r->x - ui_data.starmap.x) * 2 + 8;
        y0 = (r->y - ui_data.starmap.y) * 2 + 8;
        {
            const uint8_t *ctbl;
            ctbl = (d->controllable && !ui_starmap_enroute_in_frange(d)) ? colortbl_line_red : colortbl_line_green;
            ui_draw_line_limit_ctbl(x0 + 4, y0 + 1, x1 + 6, y1 + 6, ctbl, 5, ui_data.starmap.line_anim_phase, starmap_scale);
        }
        if (ui_extra_enabled && ui_data.starmap.flag_show_own_routes && (r->owner == d->api)) {
            int x2, y2;
            x2 = (pd->x - ui_data.starmap.x) * 2 + 8;
            y2 = (pd->y - ui_data.starmap.y) * 2 + 8;
            ui_draw_line_limit_ctbl(x0 + 4, y0 + 1, x2 + 6, y2 + 6, colortbl_line_green, 5, ui_data.starmap.line_anim_phase, starmap_scale);
        }
        gfx = ui_data.gfx.starmap.smalship[e->banner];
        if (pd->x < r->x) {
            lbxgfx_set_new_frame(gfx, 1);
        } else {
            lbxgfx_set_frame_0(gfx);
        }
        lbxgfx_draw_frame_offs(x0, y0, gfx, STARMAP_LIMITS, UI_SCREEN_W, starmap_scale);
        dist = game_get_min_dist(g, r->owner, g->planet_focus_i[d->api]);
        if (d->controllable && !ui_starmap_enroute_in_frange(d)) {
            /* FIXME use proper positioning for varying str length */
            lib_sprintf(buf, sizeof(buf), "  %s   %i %s.", game_str_sm_outsr, dist - e->fuel_range, game_str_sm_parsecs2);
            lbxfont_select_set_12_4(2, 0, 0, 0);
            lbxfont_set_gap_h(2);
            lbxfont_print_str_split(230, 26, 80, buf, 2, UI_SCREEN_W, UI_SCREEN_H, ui_scale);
        } else {
            int eta = game_calc_eta_ship(g, game_fleet_get_speed(g, r, d->en.pon, pto), pt->x, pt->y, r->x, r->y);
            lib_sprintf(buf, sizeof(buf), "%s %i %s", game_str_sm_eta, eta, (eta == 1) ? game_str_sm_turn : game_str_sm_turns);
            lbxfont_select_set_12_4(0, 0, 0, 0);
            lbxfont_print_str_center(268, 32, buf, UI_SCREEN_W, ui_scale);
        }
    }
    for (int i = 0; i < d->en.sn0.num; ++i) {
        const shipdesign_t *sd = &(g->srd[r->owner].design[0]);
        uint8_t *gfx;
        int st, x, y;
        x = (i & 1) * 43 + 228;
        y = (i / 2) * 40 + 44;
        ui_draw_filled_rect(x, y, x + 38, y + 24, 0, ui_scale);
        ui_draw_filled_rect(x, y + 28, x + 38, y + 34, 0x1c, ui_scale);
        ui_draw_stars(x, y, 0, 38, ui_scale);
        st = d->en.sn0.type[i];
        gfx = ui_data.gfx.ships[sd[st].look];
        lbxgfx_set_frame_0(gfx);
        for (int f = 0; f <= ui_data.starmap.frame_ship; ++f) {
            lbxgfx_draw_frame(x, y, gfx, UI_SCREEN_W, ui_scale);
        }
        lbxfont_select(0, 0xd, 0, 0);
        lbxfont_print_num_right(x + 35, y + 19, d->en.sn0.ships[i], UI_SCREEN_W, ui_scale);
        lbxfont_select(2, 0xa, 0, 0);
        lbxfont_print_str_center(x + 19, y + 29, sd[st].name, UI_SCREEN_W, ui_scale);
    }
    if (1
      && d->controllable
      && (!ui_starmap_enroute_in_frange(d) || ui_starmap_enroute_locked_by_retreat(d, pto))
    ) {
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

static void ui_starmap_enroute_set_pos_focus(const struct game_s *g, player_id_t player_i)
{
    const fleet_enroute_t *r = &g->enroute[ui_data.starmap.fleet_selected];
    ui_starmap_set_pos(g, r->x, r->y);
}

static int ui_starmap_enroute_next(const struct game_s *g, player_id_t pi, int i)
{
    const fleet_enroute_t *r = &g->enroute[i];
    player_id_t owner = r->owner;
    int start = i;
    do {
        i = (i + 1) % g->enroute_num;
        r = &g->enroute[i];
        if (r->owner == owner && BOOLVEC_IS1(r->visible, pi)) {
            return i;
        }
    } while (i != start);
    return g->enroute_num;
}

static int ui_starmap_enroute_prev(const struct game_s *g, player_id_t pi, int i)
{
    const fleet_enroute_t *r = &g->enroute[i];
    player_id_t owner = r->owner;
    int start = i;
    do {
        if (--i < 0) { i = g->enroute_num - 1; }
        r = &g->enroute[i];
        if (r->owner == owner && BOOLVEC_IS1(r->visible, pi)) {
            return i;
        }
    } while (i != start);
    return g->enroute_num;
}

void ui_starmap_enroute(struct game_s *g, player_id_t active_player)
{
    bool flag_done = false;
    int16_t oi_cancel, oi_accept, oi_search;
    int16_t oi_f4, oi_f5;
    struct starmap_data_s d;
    fleet_enroute_t *r;

    ui_starmap_common_init(g, &d, active_player);
    d.set_pos_focus = ui_starmap_enroute_set_pos_focus;

    r = &(g->enroute[ui_data.starmap.fleet_selected]);
    d.show_planet_focus = ((r->owner == d.api) || (g->eto[d.api].have_ia_scanner));
    d.en.pon = PLANET_NONE;
    for (int i = 0; i < g->galaxy_stars; ++i) {
        const planet_t *p;
        p = &g->planet[i];
        if ((p->x == r->x) && (p->y == r->y)) {
            d.en.pon = i;
            break;
        }
    }
    d.controllable = (g->eto[active_player].have_hyperspace_comm || d.en.pon != PLANET_NONE) && (r->owner == active_player);
    d.from = g->planet_focus_i[active_player];
    ui_data.starmap.frame_scanner = 0;
    ui_data.starmap.scanner_delay = 0;
    ui_data.starmap.frame_ship = 0;
    g->planet_focus_i[active_player] = r->dest;
    ui_starmap_sn0_setup(&d.en.sn0, g->eto[r->owner].shipdesigns_num, r->ships);
    ui_starmap_update_reserve_fuel(g, &d.en.sn0, r->ships, active_player);

    uiobj_table_clear();

#define UIOBJ_CLEAR_LOCAL() \
    do { \
        STARMAP_UIOBJ_CLEAR_COMMON(); \
        oi_accept = UIOBJI_INVALID; \
        oi_cancel = UIOBJI_INVALID; \
        oi_f4 = UIOBJI_INVALID;\
        oi_f5 = UIOBJI_INVALID;\
    } while (0)

    UIOBJ_CLEAR_LOCAL();

    uiobj_set_help_id(3);
    uiobj_set_callback_and_delay(ui_starmap_enroute_draw_cb, &d, STARMAP_DELAY);

    while (!flag_done) {
        planet_t *p;
        int16_t oi1, oi2;
        p = &g->planet[g->planet_focus_i[active_player]];
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
            int i = ui_starmap_enroute_next(g, active_player, ui_data.starmap.fleet_selected);
            if (i != g->enroute_num) {
                ui_data.starmap.fleet_selected = i;
                flag_done = true;
                ui_sound_play_sfx_24();
            }
        } else if (oi1 == oi_f5) {
            int i = ui_starmap_enroute_prev(g, active_player, ui_data.starmap.fleet_selected);
            if (i != g->enroute_num) {
                ui_data.starmap.fleet_selected = i;
                flag_done = true;
                ui_sound_play_sfx_24();
            }
        } else if (oi1 == oi_accept) {
do_accept:
            ui_sound_play_sfx_24();
            //BUG: Always true, but otherwise leads to undefined behavior
            if (p->within_frange[active_player] != 0) {
                game_fleet_redirect(g, r, d.en.pon, g->planet_focus_i[active_player]);
                flag_done = true;
            }
            ui_data.ui_main_loop_action = UI_MAIN_LOOP_STARMAP;
        }
        for (int i = 0; i < g->enroute_num; ++i) {
            if (oi1 == d.oi_tbl_enroute[i]) {
                ui_data.starmap.fleet_selected = i;
                ui_sound_play_sfx_24();
                flag_done = true;
                break;
            }
        }
        if (!d.controllable || ui_sm_explicit_cursor_context) {
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
            ui_starmap_enroute_draw_cb(&d);
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
                if (ui_starmap_enroute_in_frange(&d) && !ui_starmap_enroute_locked_by_retreat(&d, g->planet_focus_i[active_player])) {
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
