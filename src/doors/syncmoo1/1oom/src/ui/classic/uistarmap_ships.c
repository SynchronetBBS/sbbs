#include "config.h"

#include <stdio.h>

#include "uistarmap.h"
#include "comp.h"
#include "game.h"
#include "game_misc.h"
#include "game_str.h"
#include "game_tech.h"
#include "kbd.h"
#include "lbx.h"
#include "lbxfont.h"
#include "lbxgfx.h"
#include "lbxpal.h"
#include "lib.h"
#include "log.h"
#include "types.h"
#include "uicursor.h"
#include "uidelay.h"
#include "uidefs.h"
#include "uidraw.h"
#include "uiobj.h"
#include "uipal.h"
#include "uisearch.h"
#include "uisound.h"
#include "uistarmap_common.h"

/* -------------------------------------------------------------------------- */

static inline int ui_starmap_ships_get_x(int i)
{
    return 228 + 44 * (i / 3);
}

static inline int ui_starmap_ships_get_y(int i)
{
    return 83 + 26 * (i % 3);
}

static void ui_starmap_ships_draw_cb1(void *vptr)
{
    struct starmap_data_s *d = vptr;
    const struct game_s *g = d->g;
    const planet_t *p = &(g->planet[g->planet_focus_i[d->api]]);
    char buf[0x80];
    ui_starmap_draw_basic(d);
    lbxgfx_draw_frame(222, 80, ui_data.gfx.starmap.relocate, UI_SCREEN_W, ui_scale);
    ui_draw_filled_rect(225, 81, 312, 160, 0, ui_scale);

    const uint8_t n = g->eto[d->api].shipdesigns_num;
    ui_data.starmap.stars_xoff1 = 0;
    ui_data.starmap.stars_xoff2 = 0;
    for (int i = 0; i < NUM_SHIPDESIGNS; ++i) {
        int x = ui_starmap_ships_get_x(i);
        int y = ui_starmap_ships_get_y(i);
        ui_draw_stars(x, y, i * 5, 32, ui_scale);
    }
    for (int i = 0; i < n; ++i) {
        const shipdesign_t *sd = &g->srd[d->api].design[i];
        uint8_t *gfx = ui_data.gfx.ships[sd->look];
        int x = ui_starmap_ships_get_x(i);
        int y = ui_starmap_ships_get_y(i);
        lbxgfx_set_frame_0(gfx);
        lbxgfx_draw_frame(x, y, gfx, UI_SCREEN_W, ui_scale);
        lbxfont_select(2, 0xd, 0, 0);
        lbxfont_select_set_12_1(2, 0xa, 0, 0);
        lbxfont_print_str_center(x + 19, y + 18, sd->name, UI_SCREEN_W, ui_scale);
    }
    int i = p->buildship;
    if (i < n) {
        int x = ui_starmap_ships_get_x(i);
        int y = ui_starmap_ships_get_y(i);
        ui_draw_box1(x - 3, y - 1, x + 40, y + 24, 0x56, 0x56, ui_scale);
    }
    if (kbd_is_modifier(MOO_MOD_CTRL)) {
        lbxfont_select_set_12_1(2, 0xd, 0, 0);
        lbxfont_print_str_center(272, 84, game_str_sm_ship_everywhere, UI_SCREEN_W, ui_scale);
    } else if (kbd_is_modifier(MOO_MOD_ALT)) {
        lbxfont_select_set_12_1(2, 0xd, 0, 0);
        if (p->buildship != BUILDSHIP_STARGATE) {
            const shipdesign_t *sd = &g->srd[d->api].design[p->buildship];
            lib_sprintf(buf, sizeof(buf), "%s %s", game_str_sm_ship_replace, sd->name);
        } else {
            lib_sprintf(buf, sizeof(buf), "%s %s", game_str_sm_ship_replace, game_str_sm_stargate);
        }
        lbxfont_print_str_center(272, 84, buf, UI_SCREEN_W, ui_scale);
    }
}

/* -------------------------------------------------------------------------- */

void ui_starmap_ships(struct game_s *g, player_id_t active_player)
{
    bool flag_done = false;
    int16_t oi_ship_design[NUM_SHIPDESIGNS];
    int16_t oi_cancel, oi_accept, oi_search, oi_finished, oi_s;
    struct starmap_data_s d;
    ui_starmap_common_init(g, &d, active_player);

    planet_t *p = &(g->planet[g->planet_focus_i[active_player]]);

    uiobj_table_clear();

#define UIOBJ_CLEAR_LOCAL() \
    do { \
        STARMAP_UIOBJ_CLEAR_COMMON(); \
        UIOBJI_SET_TBL_INVALID(oi_ship_design); \
        oi_cancel = UIOBJI_INVALID; \
        oi_accept = UIOBJI_INVALID; \
        oi_finished = UIOBJI_INVALID; \
        oi_s = UIOBJI_INVALID; \
    } while (0)

    UIOBJ_CLEAR_LOCAL();

    uiobj_set_callback_and_delay(ui_starmap_ships_draw_cb1, &d, STARMAP_DELAY);

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
        } else if (oi1 == oi_s) {
            p->buildship = (p->buildship + 1) % g->eto[active_player].shipdesigns_num;
        } else if (oi1 == oi_finished) {
            if (ui_starmap_remove_build_finished(g, active_player, p)) {
                if (ui_extra_enabled) {
                    g->planet_focus_i[active_player] = ui_data.start_planet_focus_i;
                    ui_starmap_set_pos_focus(g, active_player);
                }
            }
            ui_sound_play_sfx_24();
            flag_done = true;
            ui_delay_1();
            oi1 = 0;
        }
        for (int i = 0; i < g->eto[active_player].shipdesigns_num; ++i) {
            if (oi1 == oi_ship_design[i]) {
                ui_sound_play_sfx_24();
                if (kbd_is_modifier(MOO_MOD_CTRL)) {
                    game_ship_build_everywhere(g, p->owner, i);
                } else if (kbd_is_modifier(MOO_MOD_ALT)) {
                    game_ship_replace_everywhere(g, p->owner, p->buildship, i);
                } else {
                    p->buildship = i;
                }
            }
        }
        if ((oi1 == oi_cancel) || (oi1 == oi_accept) || (oi1 == UIOBJI_ESC)) {
            ui_sound_play_sfx_24();
            flag_done = true;
            ui_data.ui_main_loop_action = UI_MAIN_LOOP_STARMAP;
        }
        ui_starmap_handle_oi_ctrl(&d, oi1);
        for (int i = 0; i < g->galaxy_stars; ++i) {
            if ((oi1 == d.oi_tbl_stars[i]) && !g->evn.build_finished_num[active_player]) {
                g->planet_focus_i[active_player] = i;
                flag_done = true;
                ui_sound_play_sfx_24();
                break;
            }
        }
        if (!flag_done) {
            ui_starmap_common_update_mouse_hover(&d, oi2);
            ui_starmap_ships_draw_cb1(&d);
            uiobj_table_clear();
            UIOBJ_CLEAR_LOCAL();
            if (g->evn.build_finished_num[active_player]) {
                oi_finished = uiobj_add_mousearea(6, 6, 225, 180, MOO_KEY_SPACE);
            }
            ui_starmap_fill_oi_tbls(&d);
            ui_starmap_fill_oi_tbl_stars_own(&d, active_player);
            oi_cancel = uiobj_add_t0(227, 163, "", ui_data.gfx.starmap.reloc_bu_cancel, MOO_KEY_ESCAPE);
            oi_accept = uiobj_add_t0(271, 163, "", ui_data.gfx.starmap.reloc_bu_accept, MOO_KEY_SPACE);
            oi_s = uiobj_add_inputkey(MOO_KEY_s);
            for (int i = 0; i < g->eto[active_player].shipdesigns_num; ++i) {
                int x = ui_starmap_ships_get_x(i);
                int y = ui_starmap_ships_get_y(i);
                oi_ship_design[i] = uiobj_add_mousearea(x - 3, y - 1, x + 40, y + 24, MOO_KEY_1 + i);
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
}
