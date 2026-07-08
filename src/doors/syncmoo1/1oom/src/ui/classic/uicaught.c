#include "config.h"

#include <stdio.h>

#include "uicaught.h"
#include "comp.h"
#include "game.h"
#include "game_str.h"
#include "kbd.h"
#include "lbx.h"
#include "lbxfont.h"
#include "lbxgfx.h"
#include "lbxpal.h"
#include "lib.h"
#include "types.h"
#include "uicursor.h"
#include "uidelay.h"
#include "uidefs.h"
#include "uidraw.h"
#include "uiobj.h"
#include "uisound.h"

/* -------------------------------------------------------------------------- */

struct caught_data_s {
    struct game_s *g;
    uint8_t *gfx;
    player_id_t api;
};

static void caught_data_load(struct caught_data_s *d)
{
    d->gfx = lbxfile_item_get(LBXFILE_BACKGRND, 0x1e);
}

static void caught_data_free(struct caught_data_s *d)
{
    lbxfile_item_release(LBXFILE_BACKGRND, d->gfx);
}

static void caught_draw_cb(void *vptr)
{
    struct caught_data_s *d = vptr;
    struct game_s *g = d->g;
    empiretechorbit_t *e = &(g->eto[d->api]);
    char buf[0x40];
    int x = 56, y = 50, n = 0;

    lbxgfx_draw_frame(x, y, d->gfx, UI_SCREEN_W, ui_scale);
    ui_draw_filled_rect(x + 5, y + 5, x + 110, y + 63, 0xfb, ui_scale);

    lbxfont_select(2, 0, 0, 0);
    lbxfont_print_str_normal(x + 10, y + 8, game_str_sc_caught, UI_SCREEN_W, ui_scale); /* FIXME split str to 3 and place using x */
    lbxfont_select(2, 6, 0, 0);

    y += 20;
    for (int i = 0; i < g->players; ++i) {
        if ((i != d->api) && BOOLVEC_IS1(e->contact, i)) {
            int v;
            lib_sprintf(buf, sizeof(buf), "%s:", game_str_tbl_race[g->eto[i].race]);
            lbxfont_print_str_normal(x + 11, y, buf, UI_SCREEN_W, ui_scale);
            v = g->evn.spies_caught[i][d->api];
            lbxfont_print_num_normal(x + 68, y, v, UI_SCREEN_W, ui_scale);
            v = g->evn.spies_caught[d->api][i];
            lbxfont_print_num_normal(x + 96, y, v, UI_SCREEN_W, ui_scale);
            ++n;
            y += 8;
        }
    }
}

/* -------------------------------------------------------------------------- */

void ui_caught(struct game_s *g, player_id_t active_player)
{
    struct caught_data_s d;
    bool flag_done = false;
    int16_t oi_ma;

    caught_data_load(&d);
    d.g = g;
    d.api = active_player;

    ui_draw_copy_buf();
    uiobj_finish_frame();
    ui_cursor_setup_area(1, &ui_cursor_area_tbl[0]);
    uiobj_table_clear();
    oi_ma = uiobj_add_mousearea_all(MOO_KEY_SPACE);
    uiobj_set_callback_and_delay(caught_draw_cb, &d, 1);

    while (!flag_done) {
        int16_t oi;
        oi = uiobj_handle_input_cond();
        ui_delay_prepare();
        if ((oi == oi_ma) || (oi == UIOBJI_ESC)) {
            ui_sound_play_sfx_06();
            flag_done = true;
        }
        if (!flag_done) {
            caught_draw_cb(&d);
            ui_draw_finish();
            ui_delay_ticks_or_click(1);
        }
    }

    uiobj_unset_callback();
    caught_data_free(&d);
}
