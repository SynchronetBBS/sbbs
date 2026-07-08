#include "config.h"

#include <stdio.h>

#include "uiempirereport.h"
#include "comp.h"
#include "game.h"
#include "game_aux.h"
#include "game_misc.h"
#include "game_stat.h"
#include "game_str.h"
#include "game_tech.h"
#include "kbd.h"
#include "lbx.h"
#include "lbxfont.h"
#include "lbxgfx.h"
#include "lbxpal.h"
#include "lib.h"
#include "types.h"
#include "uidelay.h"
#include "uidefs.h"
#include "uidraw.h"
#include "uiobj.h"

/* -------------------------------------------------------------------------- */

struct empirereport_data_s {
    struct game_s *g;
    uint8_t *gfx;
    player_id_t api;
    player_id_t pi;
};

static void empirereport_data_load(struct empirereport_data_s *d)
{
    d->gfx = lbxfile_item_get(LBXFILE_BACKGRND, 3);
}

static void empirereport_data_free(struct empirereport_data_s *d)
{
    lbxfile_item_release(LBXFILE_BACKGRND, d->gfx);
}

static void empirereport_draw_cb(void *vptr)
{
    struct empirereport_data_s *d = vptr;
    const struct game_s *g = d->g;
    const empiretechorbit_t *e = &(g->eto[d->pi]);
    const shipresearch_t *srd = &(g->srd[d->pi]);
    char buf[0x40];

    ui_draw_color_buf(0x3e);
    lbxgfx_draw_frame(0, 0, d->gfx, UI_SCREEN_W, ui_scale);
    ui_draw_filled_rect(21, 15, 60, 48, 0, ui_scale);
    lbxgfx_draw_frame(21, 15, ui_data.gfx.planets.race[e->race], UI_SCREEN_W, ui_scale);
    ui_draw_filled_rect(17, 58, 64, 68, tbl_banner_color2[e->banner], ui_scale);
    lbxfont_select(5, 6, 0, 0);
    {
        const char *str = ui_extra_enabled ? g->emperor_names[d->pi] : game_str_tbl_races[e->race];
        lbxfont_print_str_center(40, 60, str, UI_SCREEN_W, ui_scale);
    }
    lbxfont_select(0, 6, 0, 0);
    lbxfont_print_str_center(40, 74, game_str_tbl_trait1[e->trait1], UI_SCREEN_W, ui_scale);
    lbxfont_print_str_center(40, 81, game_str_tbl_trait2[e->trait2], UI_SCREEN_W, ui_scale);
    lbxfont_print_str_center(40, 101, game_str_re_reportis, UI_SCREEN_W, ui_scale);
    {
        int reportage = g->year - g->eto[d->api].spyreportyear[d->pi] - 1;
        if (reportage < 2) {
            lib_sprintf(buf, sizeof(buf), "%s", game_str_re_current);
        } else {
            lib_sprintf(buf, sizeof(buf), "%i %s", reportage, game_str_re_yearsold);
        }
        lbxfont_print_str_center(40, 108, buf, UI_SCREEN_W, ui_scale);
    }
    lbxfont_select(3, 0, 0, 0);
    lbxfont_set_color_c_n(0x26, 6);
    lbxfont_print_str_center(41, 128, game_str_re_alliance, UI_SCREEN_W, ui_scale);
    ui_draw_line1(9, 136, 72, 136, 0x26, ui_scale);
    lbxfont_select(0, 6, 0, 0);
    {
        int n = 0;
        for (int i = 0; (i < g->players) && (n < 3); ++i) {
            if ((i != d->pi) && (e->treaty[i] == TREATY_ALLIANCE) && IS_ALIVE(g, i)) {
                ui_draw_pixel(9, 140 + 6 * n, 0, ui_scale);
                ui_draw_pixel(9, 141 + 6 * n, 0, ui_scale);
                ui_draw_pixel(10, 140 + 6 * n, 0, ui_scale);
                ui_draw_pixel(10, 141 + 6 * n, 0, ui_scale);
                lbxfont_print_str_normal(13, 139 + 6 * n, game_str_tbl_races[g->eto[i].race], UI_SCREEN_W, ui_scale);
                ++n;
            }
        }
    }
    lbxfont_select(3, 0, 0, 0);
    lbxfont_set_color_c_n(0x26, 6);
    lbxfont_print_str_center(41, 165, game_str_re_wars, UI_SCREEN_W, ui_scale);
    ui_draw_line1(9, 173, 72, 173, 0x26, ui_scale);
    lbxfont_select(0, 6, 0, 0);
    {
        int n = 0;
        for (int i = 0; (i < g->players) && (n < 3); ++i) {
            if ((i != d->pi) && (e->treaty[i] >= TREATY_WAR) && IS_ALIVE(g, i)) {
                ui_draw_pixel(9, 177 + 6 * n, 0, ui_scale);
                ui_draw_pixel(9, 178 + 6 * n, 0, ui_scale);
                ui_draw_pixel(10, 177 + 6 * n, 0, ui_scale);
                ui_draw_pixel(10, 178 + 6 * n, 0, ui_scale);
                lbxfont_print_str_normal(13, 176 + 6 * n, game_str_tbl_races[g->eto[i].race], UI_SCREEN_W, ui_scale);
                ++n;
            }
        }
    }
    for (int f = 0; f < TECH_FIELD_NUM; ++f) {
        const uint8_t *rct = &(srd->researchcompleted[f][0]);
        uint16_t tc;
        uint8_t first, num, rf;
        tc = e->tech.completed[f];
        rf = g->eto[d->api].spyreportfield[d->pi][f];
        num = 0;
        for (int i = 0; i < tc; ++i) {
            uint8_t rc;
            rc = rct[i];
            if (rc <= rf) {
                num = i + 1;
            }
        }
        if (num > 8) {
            first = num - 8;
            num = 8;
        } else {
            first = 0;
        }
        for (int i = 0; i < num; ++i) {
            uint8_t rc;
            rc = rct[first + i];
            game_tech_get_name(g->gaux, f, rc, buf, sizeof(buf));
            lbxfont_select(2, game_tech_player_has_tech(g, f, rc, d->api) ? 0xa : 0, 0, 0);
            if ((rc <= 50) && (game_tech_get_group(g->gaux, f, rc) == TECH_GROUP_CONTROLLED_ENVIRONMENT)) {
                int j, pos_space;
                j = 0;
                pos_space = 0;
                while (buf[j] != 0) {
                    if (buf[j] == ' ') {
                        pos_space = j;
                    }
                    ++j;
                }
                lib_sprintf(&buf[pos_space], sizeof(buf) - pos_space, " %s", game_str_re_environ);
            }
            lbxfont_print_str_normal(85 + (f & 1) * 120, 16 + (f / 2) * 65 + i * 6, buf, UI_SCREEN_W, ui_scale);
        }
    }
}

/* -------------------------------------------------------------------------- */

void ui_empirereport(struct game_s *g, player_id_t active_player, player_id_t pi)
{
    struct empirereport_data_s d;
    bool flag_done = false;

    empirereport_data_load(&d);
    d.g = g;
    d.api = active_player;
    d.pi = pi;

    game_update_production(g);
    game_update_empire_contact(g);
    game_update_maint_costs(g);

    uiobj_table_clear();
    uiobj_set_help_id(39);
    uiobj_set_callback_and_delay(empirereport_draw_cb, &d, 1);

    while (!flag_done) {
        int16_t oi;
        oi = uiobj_handle_input_cond();
        ui_delay_prepare();
        if (oi != UIOBJI_NONE) {
            flag_done = true;
        }
        if (!flag_done) {
            empirereport_draw_cb(&d);
            uiobj_table_clear();
            uiobj_add_mousearea_all(MOO_KEY_SPACE);
            ui_draw_finish();
            ui_delay_ticks_or_click(1);
        }
    }

    uiobj_unset_callback();
    uiobj_set_help_id(-1);
    empirereport_data_free(&d);
}
