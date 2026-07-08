#include "config.h"

#include <ctype.h>
#include <stdio.h>

#include "ui.h"
#include "comp.h"
#include "game.h"
#include "game_aux.h"
#include "game_str.h"
#include "game_tech.h"
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
#include "uiobj.h"
#include "uisound.h"
#include "uiswitch.h"
#include "uiempirereport.h"

/* -------------------------------------------------------------------------- */

struct steal_data_s {
    struct game_s *g;
    player_id_t spy;
    player_id_t target;
    uint8_t flags_field;
    uint8_t *gfx_espionag;
    void *gmap;
};

static void steal_load_data(struct steal_data_s *d)
{
    d->gfx_espionag = lbxfile_item_get(LBXFILE_BACKGRND, 0xd);
}

static void steal_free_data(struct steal_data_s *d)
{
    lbxfile_item_release(LBXFILE_BACKGRND, d->gfx_espionag);
}

static void steal_draw_cb(void *vptr)
{
    struct steal_data_s *d = vptr;
    struct game_s *g = d->g;
    empiretechorbit_t *e = &(g->eto[d->target]);
    char buf[0xe0];

    hw_video_copy_back_from_page3();
    ui_gmap_basic_draw_frame(d->gmap, d->spy);
    lbxgfx_draw_frame(6, 24, d->gfx_espionag, UI_SCREEN_W, ui_scale);
    {
        char rbuf[0x20], *p, c;
        bool usean = false;
        lib_strcpy(rbuf, game_str_tbl_race[e->race], sizeof(rbuf));
        p = rbuf;
        while ((c = *p) != '\0') {
            if (islower(c)) {
                c = toupper(c);
                *p = c;
            }
            if ((c == 'A') || (c == 'E') || (c == 'I') || (c == 'O') || (c == 'U')) {
                usean = true;
            }
            ++p;
        }
        lib_sprintf(buf, sizeof(buf), "%s%s %s %s", game_str_es_youresp1, usean ? "N" : "", rbuf, game_str_es_youresp2);
    }
    lbxfont_select_set_12_1(0, 8, 0, 0);
    lbxfont_print_str_center(118, 30, buf, UI_SCREEN_W, ui_scale);
    lbxfont_print_str_normal(23, 83, game_str_es_youresp3, UI_SCREEN_W, ui_scale);
    lbxgfx_draw_frame(102, 43, ui_data.gfx.planets.race[e->race], UI_SCREEN_W, ui_scale);

    for (int i = 0; i < TECH_FIELD_NUM; ++i) {
        if (d->flags_field & (1 << i)) {
            int x, y;
            x = (i / 3) * 102 + 20;
            y = (i % 3) * 22 + 97;
            ui_draw_filled_rect(x, y, x + 86, y + 11, 4, ui_scale);
            lbxfont_select(5, 0xe, 0, 0);
            lbxfont_print_str_center(x + 43, y + 3, game_str_tbl_te_field[i], UI_SCREEN_W, ui_scale);
        }
    }
}

/* -------------------------------------------------------------------------- */

struct stolen_data_s {
    struct game_s *g;
    player_id_t api;
    player_id_t spy;
    tech_field_t field;
    uint8_t tech;
    uint8_t *gfx;
    void *gmap;
};

static void stolen_load_data(struct stolen_data_s *d)
{
    d->gfx = lbxfile_item_get(LBXFILE_BACKGRND, 0xe);
}

static void stolen_free_data(struct stolen_data_s *d)
{
    lbxfile_item_release(LBXFILE_BACKGRND, d->gfx);
}

static void stolen_draw_cb(void *vptr)
{
    struct stolen_data_s *d = vptr;
    const struct game_s *g = d->g;
    const char *s;
    uint8_t fontparam;
    char buf[0x80];
    if (d->spy != PLAYER_NONE) {
        const empiretechorbit_t *e = &(g->eto[d->spy]);
        s = game_str_tbl_race[e->race];
        fontparam = tbl_banner_fontparam[e->banner];
    } else {
        s = game_str_es_unkn;
        fontparam = 2;
    }
    hw_video_copy_back_from_page2();
    ui_gmap_basic_draw_frame(d->gmap, d->api);
    ui_draw_filled_rect(31, 62, 202, 103, 0x36, ui_scale);
    lbxgfx_draw_frame(31, 62, d->gfx, UI_SCREEN_W, ui_scale);
    lib_sprintf(buf, sizeof(buf), "%s %s", s, game_str_es_thesp1);
    lbxfont_select_set_12_1(5, fontparam, 0, 0);
    lbxfont_print_str_center(116, 70, buf, UI_SCREEN_W, ui_scale);
    lbxfont_select(0, 0, 0, 0);
    lib_sprintf(buf, sizeof(buf), "%s %s ", s, game_str_es_thesp2);
    lbxfont_print_str_center(118, 84, buf, UI_SCREEN_W, ui_scale);
    game_tech_get_name(g->gaux, d->field, d->tech, buf, sizeof(buf));
    lbxfont_print_str_center(118, 94, buf, UI_SCREEN_W, ui_scale);
}

/* -------------------------------------------------------------------------- */

int ui_spy_steal(struct game_s *g, int spy, int target, uint8_t flags_field)
{
    struct steal_data_s d;
    bool flag_done = false;
    int16_t oi_tbl_field[TECH_FIELD_NUM];
    int16_t oi_report = UIOBJI_INVALID;
    int selected = -1;

    ui_switch_1(g, spy);
    ui_sound_play_music(0xf);

    hw_video_copy_back_from_page2();
    hw_video_copy_back_to_page3();

    d.g = g;
    d.spy = spy;
    d.target = target;
    d.flags_field = flags_field;
    d.gmap = ui_gmap_basic_init(g, true);
    steal_load_data(&d);
restart:
    uiobj_table_clear();

    for (int i = 0; i < TECH_FIELD_NUM; ++i) {
        if (flags_field & (1 << i)) {
            int x, y;
            x = (i / 3) * 102 + 20;
            y = (i % 3) * 22 + 97;
            oi_tbl_field[i] = uiobj_add_mousearea(x - 2, y - 2, x + 88, y + 13, MOO_KEY_UNKNOWN);
        } else {
            oi_tbl_field[i] = UIOBJI_INVALID;
        }
    }
    if (g->gaux->flag_cheat_spy_hint) {
        oi_report = uiobj_add_mousearea(18, 43, 210, 76, MOO_KEY_UNKNOWN);
    }

    uiobj_set_callback_and_delay(steal_draw_cb, &d, 4);

    while (!flag_done) {
        int16_t oi;
        ui_delay_prepare();
        oi = uiobj_handle_input_cond();
        if (oi == UIOBJI_ESC) {
            ui_sound_play_sfx_24();
            selected = -1;
            flag_done = true;
        } else if (oi == oi_report) {
            ui_empirereport(g, spy, target);
            goto restart;
        }
        for (int i = 0; i < TECH_FIELD_NUM; ++i) {
            if (oi == oi_tbl_field[i]) {
                ui_sound_play_sfx_24();
                selected = i;
                flag_done = true;
            }
        }
        if (!flag_done) {
            steal_draw_cb(&d);
            ui_draw_finish();
            ui_delay_ticks_or_click(4);
        }
    }
    ui_sound_stop_music();
    uiobj_unset_callback();
    uiobj_table_clear();
    steal_free_data(&d);
    hw_video_copy_back_from_page3();
    hw_video_copy_back_to_page2();
    return selected;
}

void ui_spy_stolen(struct game_s *g, int pi, int spy, int field, uint8_t tech)
{
    struct stolen_data_s d;
    bool flag_done = false;
    ui_switch_1(g, pi);
    d.g = g;
    d.api = pi;
    d.spy = spy;
    d.field = field;
    d.tech = tech;
    d.gmap = ui_gmap_basic_init(g, true);
    stolen_load_data(&d);
    uiobj_table_clear();
    uiobj_add_mousearea_all(MOO_KEY_SPACE);
    while (!flag_done) {
        int16_t oi;
        ui_delay_prepare();
        oi = uiobj_handle_input_cond();
        if (oi != 0) {
            flag_done = true;
        }
        stolen_draw_cb(&d);
        ui_draw_finish();
        ui_delay_ticks_or_click(3);
    }
    uiobj_table_clear();
    stolen_free_data(&d);
}
