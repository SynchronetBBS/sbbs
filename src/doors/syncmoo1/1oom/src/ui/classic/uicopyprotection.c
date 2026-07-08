#include "config.h"

#include <ctype.h>
#include <stdio.h>

#include "comp.h"
#include "game.h"
#include "game_news.h"
#include "game_str.h"
#include "game_turn.h"
#include "game_types.h"
#include "lbx.h"
#include "lbxfont.h"
#include "lbxgfx.h"
#include "lbxpal.h"
#include "lib.h"
#include "rnd.h"
#include "uidefs.h"
#include "uidelay.h"
#include "uidraw.h"
#include "uiobj.h"
#include "uipal.h"
#include "uisound.h"

/* -------------------------------------------------------------------------- */

struct copyprotection_data_s {
    struct game_s *g;
    uint8_t *gfx;
    uint8_t *gfx2;
    uint16_t seed;
    uint16_t page_min;
    uint16_t page_max;
    uint16_t ship_name_id[8];
    uint16_t correct_answer_id;
};

static void ui_copyprotection_draw_cb(void *vptr)
{
    uint8_t look_i[] = {
        0x12,0x68,0x87,0x2A,0x39,0x6D,0x5A,0x83,0x17,0x42,
        0x0D,0x4E,0x8B,0x27,0x7,0x73,0x54,0x23,0x45,0x41,
        0x15,0x67,0x89,0x2E,0x38,0x6F,0x5D,0x80,0x2F,0x46,
        0x11,0x51,0x8F,0x29,0x0B,0x77,0x56,0x1F,0x5C,0x40
    };
    char buf[64];
    struct copyprotection_data_s *d = vptr;
    lbxgfx_draw_frame(0, 0, d->gfx, UI_SCREEN_W, ui_scale);
    lbxgfx_set_frame_0(ui_data.gfx.ships[look_i[d->seed]]);
    lbxgfx_draw_frame(208, 23, ui_data.gfx.ships[look_i[d->seed]], UI_SCREEN_W, ui_scale);
    lbxfont_select(5, 0, 0, 0);
    lbxfont_print_str_normal(197, 48, "Pages", UI_SCREEN_W, ui_scale);
    lib_sprintf(buf, 64, "%d-%d", d->page_min, d->page_max);
    lbxfont_print_str_normal(224, 48, buf, UI_SCREEN_W, ui_scale);
}

/* -------------------------------------------------------------------------- */

void ui_copyprotection_check(struct game_s *g)
{
    struct copyprotection_data_s d;
    bool flag_done = false;
    int16_t oi_ship_name[8];
    int16_t oi;
    d.g = g;
    if (ui_copyprotection_disabled) {
        copyprot_status = -99;
        return;
    }
    uiobj_set_xyoff(1, 1);
    if (copyprot_status == 0) {
        return;
    }
    ui_delay_1();
    ui_sound_stop_music();
    d.gfx = lbxfile_item_get(LBXFILE_BACKGRND, 0x20);
    d.gfx2 = lbxfile_item_get(LBXFILE_BACKGRND, 0x2f);
    uiobj_table_clear();
    if (ui_draw_finish_mode == 0) {
        ui_palette_fadeout_a_f_1();
        ui_draw_finish_mode = 2;
    }
    lbxpal_select(9, -1, 0);
    lbxpal_set_update_range(0, 255);

    ui_draw_erase_buf();
    ui_draw_finish();
    ui_draw_erase_buf();
    ui_palette_set_n();
    for (int attempts_left = 3; attempts_left > 0; --attempts_left) {
        d.seed = rnd_0_nm1(40, &g->seed);
        do {
            d.page_min = d.seed + 0x1a - rnd_0_nm1(4, &g->seed);
            d.page_max = d.seed + rnd_1_n(4, &g->seed) + 0x19;
        } while (d.page_min == d.page_max);
        SETMAX(d.page_min, 0x1a);
        SETMIN(d.page_max, 0x41);
        for (int si = 0; si < 8;) {
            bool flag_again = false;
            d.ship_name_id[si] = rnd_0_nm1(40, &g->seed);
            for (int di = 0; di < si; ++di) {
                if (d.ship_name_id[si] == d.seed) {
                    flag_again = true;
                }
                if (d.ship_name_id[si] == d.ship_name_id[di]) {
                    flag_again = true;
                }
            }
            if (!flag_again) {
                ++si;
            }
        }
        d.correct_answer_id = rnd_0_nm1(8, &g->seed);
        d.ship_name_id[d.correct_answer_id] = d.seed;
        uiobj_set_callback_and_delay(ui_copyprotection_draw_cb, &d, 1);
        uiobj_table_clear();
        for (int si = 0; si < 8; ++si) {
            uint16_t x, y;
            x = (si / 4) * 0x48 + 0x75;
            y = (si % 4) * 0x13 + 0x3e;
            lbxfont_select(0, 0, 0, 0);
            oi_ship_name[si] = uiobj_add_t0(x, y, game_str_tbl_copyprotection_ship_names[d.ship_name_id[si]], d.gfx2, MOO_KEY_UNKNOWN);
        }
        flag_done = false;
        while (!flag_done) {
            oi = uiobj_handle_input_cond();
            if (oi != 0) {
                flag_done = true;
            }
            ui_copyprotection_draw_cb(&d);
            ui_draw_finish();
            ui_delay_ticks_or_click(1);
        }
        int pick = -1;
        for (int i = 0; i < 8; ++i) {
            if (oi_ship_name[i] == oi) {
                pick = i;
                break;
            }
        }
        if (d.ship_name_id[pick] == d.seed) {
            copyprot_status = -99;
            ui_sound_play_sfx_24();
        } else {
            ui_sound_play_sfx_06();
        }
        if (copyprot_status != 1) {
            break;
        }
    }
    uiobj_table_clear();
    uiobj_unset_callback();
    ui_palette_fadeout_a_f_1();
    ui_draw_finish_mode = 2;
    ui_draw_erase_buf();
    uiobj_finish_frame();
    ui_draw_erase_buf();
    lbxpal_select(0, -1, 0);
    lbxpal_set_update_range(0, 255);
    lbxfile_item_release(LBXFILE_BACKGRND, d.gfx);
    lbxfile_item_release(LBXFILE_BACKGRND, d.gfx2);
}

void ui_copyprotection_lose(struct game_s *g, struct game_end_s *ge)
{
    struct news_s ns;
    int best_ai = 0;
    int best_planet_num = 0;
    int planet_num[6] = {0, 0, 0, 0, 0, 0};
    player_id_t api = 0;
    for (player_id_t i = PLAYER_0; i < g->players; ++i) {
        api = i;
        if (IS_HUMAN(g, i)) {
            break;
        }
    }
    for (int si = 0; si < g->galaxy_stars; ++si) {
        if (g->planet[si].owner == PLAYER_NONE) {
            continue;
        }
        if (IS_HUMAN(g, g->planet[si].owner)) {
            continue;
        }
        /*  ++planet_num[si];  WASBUG: wrong index   */
        ++planet_num[g->planet[si].owner];
    }
    for (player_id_t si = PLAYER_0; si < g->players; ++si) {
        if (si == api) {
            continue;
        }
        if (planet_num[si] <= best_planet_num) {
            continue;
        }
        best_planet_num = planet_num[si];
        best_ai = si;
    }
    /* MOO deletes save_7 here */
    ns.type = GAME_NEWS_GENOCIDE;
    ns.subtype = 3;
    ns.num1 = 0;
    ns.race = g->eto[api].race;
    ui_data.news.flag_also = false;
    ui_news(g, &ns);
    ge->banner_dead = g->eto[api].banner;
    ge->race = g->eto[best_ai].banner;
    ge->type = GAME_END_LOST_FUNERAL;
}
