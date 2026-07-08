#include "config.h"

#include <stdio.h>

#include "uiempirestatus.h"
#include "game.h"
#include "game_stat.h"
#include "game_str.h"
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

struct empirestatus_data_s {
    struct game_s *g;
    uint8_t *gfx;
    struct game_stats_s st;
};

static void empirestatus_data_load(struct empirestatus_data_s *d)
{
    d->gfx = lbxfile_item_get(LBXFILE_BACKGRND, 2);
}

static void empirestatus_data_free(struct empirestatus_data_s *d)
{
    lbxfile_item_release(LBXFILE_BACKGRND, d->gfx);
}

static void empirestatus_draw_cb(void *vptr)
{
    struct empirestatus_data_s *d = vptr;
    const struct game_s *g = d->g;
    const struct game_stats_s *st = &(d->st);

    ui_draw_color_buf(0x3a);
    lbxgfx_draw_frame(0, 0, d->gfx, UI_SCREEN_W, ui_scale);

    lbxfont_select_set_12_4(4, 0xf, 0, 0);
    lbxfont_print_str_center(160, 9, game_str_ra_stats, UI_SCREEN_W, ui_scale);
    lbxfont_select_set_12_4(5, 5, 0, 0);
    lbxfont_print_str_normal(15, 11, game_str_year, UI_SCREEN_W, ui_scale);
    lbxfont_print_num_normal(42, 11, g->year + YEAR_BASE, UI_SCREEN_W, ui_scale);

    lbxfont_select(2, 6, 0, 0);
    for (int s = 0; s < 6; ++s) {
        for (int i = 0; i < st->num; ++i) {
            player_id_t pi;
            const empiretechorbit_t *e;
            int x, y;
            uint8_t v;
            x = (s / 3) * 156 + 11;
            y = (s % 3) * 57 + i * 7 + 38;
            pi = st->p[i];
            e = (&g->eto[pi]);
            lbxfont_print_str_normal(x, y, game_str_tbl_race[e->race], UI_SCREEN_W, ui_scale);
            v = st->v[s][i];
            if (v) {
                ui_draw_filled_rect(x + 35, y + 1, x + 34 + v, y + 2, tbl_banner_color2[e->banner], ui_scale);
                if (v > 1) {
                    ui_draw_line1(x + 35, y + 3, x + 33 + v, y + 3, 0, ui_scale);
                }
            }
        }
    }
}

/* -------------------------------------------------------------------------- */

void ui_empirestatus(struct game_s *g, player_id_t api)
{
    struct empirestatus_data_s d;
    bool flag_done = false;

    empirestatus_data_load(&d);
    d.g = g;

    game_stats_all(g, api, &(d.st));

    uiobj_table_clear();
    uiobj_set_help_id(15);
    uiobj_set_callback_and_delay(empirestatus_draw_cb, &d, 1);

    while (!flag_done) {
        int16_t oi;
        oi = uiobj_handle_input_cond();
        ui_delay_prepare();
        if (oi != UIOBJI_NONE) {
            flag_done = true;
        }
        if (!flag_done) {
            empirestatus_draw_cb(&d);
            uiobj_table_clear();
            uiobj_add_mousearea_all(MOO_KEY_SPACE);
            ui_draw_finish();
            ui_delay_ticks_or_click(1);
        }
    }

    uiobj_unset_callback();
    uiobj_set_help_id(-1);
    empirestatus_data_free(&d);
}
