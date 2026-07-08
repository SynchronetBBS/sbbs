#include "config.h"

#include <stdio.h>

#include "uistarmap.h"
#include "comp.h"
#include "game.h"
#include "game_misc.h"
#include "game_num.h"
#include "game_str.h"
#include "kbd.h"
#include "lbxgfx.h"
#include "lbxfont.h"
#include "lib.h"
#include "log.h"
#include "types.h"
#include "uidraw.h"
#include "uidefs.h"
#include "uidelay.h"
#include "uiobj.h"
#include "uisearch.h"
#include "uisound.h"
#include "uistarmap_common.h"

/* -------------------------------------------------------------------------- */

static void ui_starmap_reloc_draw_cb(void *vptr)
{
    struct starmap_data_s *d = vptr;
    const struct game_s *g = d->g;
    const planet_t *pf = &g->planet[d->from];
    const planet_t *pt = &g->planet[g->planet_focus_i[d->api]];
    char buf[0x40];
    int x0, y0;
    STARMAP_LIM_INIT();
    ui_starmap_draw_basic(d);
    x0 = (pf->x - ui_data.starmap.x) * 2 + 8;
    y0 = (pf->y - ui_data.starmap.y) * 2 + 8;
    if (g->planet_focus_i[d->api] != d->from) {
        int x1, y1;
        const uint8_t *ctbl;
        x1 = (pt->x - ui_data.starmap.x) * 2 + 14;
        y1 = (pt->y - ui_data.starmap.y) * 2 + 14;
        if (game_reloc_dest_ok(g, g->planet_focus_i[d->api], d->api)) {
            ctbl = colortbl_line_green;
        } else {
            ctbl = colortbl_line_red;
        }
        ui_draw_line_limit_ctbl(x0 + 6, y0 + 6, x1, y1, ctbl, 5, ui_data.starmap.line_anim_phase, starmap_scale);
    }
    lbxgfx_draw_frame_offs_delay(x0, y0, !d->anim_delay, ui_data.gfx.starmap.planbord, STARMAP_LIMITS, UI_SCREEN_W, starmap_scale);
    if (pt->owner == d->api) {
        lbxgfx_draw_frame(222, 80, ui_data.gfx.starmap.relocate, UI_SCREEN_W, ui_scale);
    } else {
        lbxgfx_draw_frame_offs(222, 80, ui_data.gfx.starmap.relocate, 0, 83, 310, 199, UI_SCREEN_W, ui_scale);
        if (BOOLVEC_IS0(pt->explored, d->api)) {
            ui_draw_filled_rect(227, 57, 310, 159, 0, ui_scale);
            lbxgfx_draw_frame_offs(224, 5, ui_data.gfx.starmap.unexplor, 227, 57, 310, 159, UI_SCREEN_W, ui_scale);
        } else {
            ui_draw_filled_rect(227, 73, 310, 159, 7, ui_scale);
            ui_draw_box1(227, 73, 310, 159, 4, 4, ui_scale);
        }
    }
    lbxfont_select_set_12_1(5, 5, 0, 0);
    lbxfont_print_str_center(269, 90, game_str_sm_sreloc, UI_SCREEN_W, ui_scale);
    lbxfont_select(0, 6, 0, 0);
    lbxfont_print_str_split(229, 105, 80, game_str_sm_sreloc2, 2, UI_SCREEN_W, UI_SCREEN_H, ui_scale);
    if (g->planet_focus_i[d->api] != d->from) {
        if (pf->have_stargate && pt->have_stargate && pf->owner == pt->owner) {
            lib_strcpy(buf, game_str_sm_stargate, sizeof(buf));
        } else {
            int eta;
            eta = game_calc_eta_ship(g, g->srd[d->api].design[pf->buildship].engine + 1, pf->x, pf->y, pt->x, pt->y);
            lib_sprintf(buf, sizeof(buf), "%s %i %s", game_str_sm_delay, eta, (eta == 1) ? game_str_sm_turn : game_str_sm_turns);
        }
        lbxfont_select(0, 0, 0, 0);
        lbxfont_print_str_center(268, 149, buf, UI_SCREEN_W, ui_scale);
    }
    lbxgfx_set_new_frame(ui_data.gfx.starmap.reloc_bu_accept, 1);
    lbxgfx_draw_frame(271, 163, ui_data.gfx.starmap.reloc_bu_accept, UI_SCREEN_W, ui_scale);
}

/* -------------------------------------------------------------------------- */

void ui_starmap_reloc(struct game_s *g, player_id_t active_player)
{
    bool flag_done = false;
    int16_t oi_cancel, oi_accept, oi_search;
    struct starmap_data_s d;
    ui_starmap_common_init(g, &d, active_player);
    d.controllable = true;
    {
        uint8_t oldreloc;
        uint8_t pi = g->planet_focus_i[active_player];
        d.from = pi;
        oldreloc = g->planet[pi].reloc;
        g->planet_focus_i[active_player] = oldreloc;
        if (!game_reloc_dest_ok(g, oldreloc, active_player)) {
            g->planet_focus_i[active_player] = pi;
        }
    }

    uiobj_table_clear();

#define UIOBJ_CLEAR_LOCAL() \
    do { \
        STARMAP_UIOBJ_CLEAR_COMMON(); \
        oi_accept = UIOBJI_INVALID; \
        oi_cancel = UIOBJI_INVALID; \
    } while (0)

    UIOBJ_CLEAR_LOCAL();

    uiobj_set_help_id(2);
    uiobj_set_callback_and_delay(ui_starmap_reloc_draw_cb, &d, STARMAP_DELAY);

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
            int i;
            i = ui_search(g, active_player);
            if (i >= 0) {
                if (game_reloc_dest_ok(g, i, active_player)) {
                    ui_sound_play_sfx_24();
                    g->planet_focus_i[active_player] = i;
                    ui_starmap_set_pos_focus(g, active_player);
                } else {
                    ui_sound_play_sfx_06();
                    flag_done = true;
                    ui_data.ui_main_loop_action = UI_MAIN_LOOP_STARMAP;
                }
            }
        }
        if ((oi1 == oi_cancel) || (oi1 == UIOBJI_ESC)) {
            ui_sound_play_sfx_06();
            flag_done = true;
            ui_data.ui_main_loop_action = UI_MAIN_LOOP_STARMAP;
        } else if (oi1 == oi_accept) {
do_accept:
            ui_sound_play_sfx_24();
            flag_done = true;
            g->planet[d.from].reloc = g->planet_focus_i[active_player];
            ui_data.ui_main_loop_action = UI_MAIN_LOOP_STARMAP;
        }
        ui_starmap_handle_oi_ctrl(&d, oi1);
        for (int i = 0; i < g->galaxy_stars; ++i) {
            if (oi1 == d.oi_tbl_stars[i]) {
                if (ui_extra_enabled && (oi_accept != UIOBJI_INVALID) && (g->planet_focus_i[active_player] == i)) {
                    oi1 = oi_accept;
                    goto do_accept;
                }
                g->planet_focus_i[active_player] = i;
                ui_sound_play_sfx_24();
                break;
            }
        }
        if (!flag_done) {
            ui_starmap_common_update_mouse_hover(&d, oi2);
            ui_starmap_reloc_draw_cb(&d);
            uiobj_table_clear();
            UIOBJ_CLEAR_LOCAL();
            if (game_num_extended_reloc_range) {
                ui_starmap_fill_oi_tbl_stars(&d);
            } else {
                ui_starmap_fill_oi_tbl_stars_own(&d, active_player);
            }
            oi_cancel = uiobj_add_t0(227, 163, "", ui_data.gfx.starmap.reloc_bu_cancel, MOO_KEY_ESCAPE);
            if (g->planet[d.from].buildship != BUILDSHIP_STARGATE
             && game_reloc_dest_ok(g, g->planet_focus_i[active_player], active_player)) {
                oi_accept = uiobj_add_t0(271, 163, "", ui_data.gfx.starmap.reloc_bu_accept, MOO_KEY_SPACE);
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
    uiobj_set_help_id(-1);
    g->planet_focus_i[active_player] = d.from;
}
