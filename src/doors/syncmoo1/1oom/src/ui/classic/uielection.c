#include "config.h"

#include <stdio.h>

#include "ui.h"
#include "comp.h"
#include "game_election.h"
#include "game_str.h"
#include "hw.h"
#include "kbd.h"
#include "lbx.h"
#include "lbxfont.h"
#include "lbxgfx.h"
#include "lbxpal.h"
#include "lib.h"
#include "log.h"
#include "types.h"
#include "uicursor.h"
#include "uidefs.h"
#include "uidelay.h"
#include "uidraw.h"
#include "uiobj.h"
#include "uipal.h"
#include "uisound.h"
#include "uiswitch.h"

/* -------------------------------------------------------------------------- */

struct election_data_s {
    struct election_s *el;
    uint8_t *gfx_cylinder;
    uint8_t *gfx_racem[4];
    uint8_t *gfx_race[PLAYER_NUM];
    bool flag_countdown;
    int count;
};

/* -------------------------------------------------------------------------- */

static void election_load_data(struct election_data_s *d)
{
    struct game_s *g = d->el->g;
    ui_draw_erase_buf();
    for (int i = 0; i < 3; ++i) {
        const int item[3] = { 0x17, 0, 0x16 };
        uint8_t *gfx;
        gfx = lbxfile_item_get(LBXFILE_COUNCIL, item[i]);
        lbxgfx_draw_frame(0, 0, gfx, UI_SCREEN_W, ui_scale);
        lbxfile_item_release(LBXFILE_COUNCIL, gfx);
    }
    hw_video_copy_back_to_page2();
    d->gfx_cylinder = lbxfile_item_get(LBXFILE_COUNCIL, 1);
    {
        int num;
        num = d->el->num;
        SETRANGE(num, 1, 4);
        for (int i = 0; i < num; ++i) {
            d->gfx_racem[i] = lbxfile_item_get(LBXFILE_COUNCIL, 0xc + g->eto[d->el->tbl_ei[i]].race);
        }
        for (int i = num; i < 4; ++i) {
            d->gfx_racem[i] = 0;
        }
        num = d->el->num + 1;
        for (int i = 0; i < num; ++i) {
            d->gfx_race[i] = lbxfile_item_get(LBXFILE_COUNCIL, 0x2 + g->eto[d->el->tbl_ei[i]].race);
        }
    }
}

static void election_free_data(struct election_data_s *d)
{
    lbxfile_item_release_file(LBXFILE_COUNCIL);
}

static void ui_election_draw_cb(void *vptr)
{
    struct election_data_s *d = vptr;
    struct election_s *el = d->el;
    struct game_s *g = el->g;
    hw_video_copy_back_from_page2();
    if ((el->cur_i != PLAYER_NONE) && (d->count == 0)) {
        uint8_t *gfx = d->gfx_race[el->cur_i];
        int fn = lbxgfx_get_frame(gfx);
        lbxgfx_set_frame_0(gfx);
        for (int f = 0; f <= fn; ++f) {
            lbxgfx_draw_frame(125, 0, gfx, UI_SCREEN_W, ui_scale);
        }
    }
    lbxgfx_draw_frame(0, 0, d->gfx_cylinder, UI_SCREEN_W, ui_scale);
    for (int i = 0; i < MIN(el->num, 4); ++i) {
        const int lx0[4] = { 50, 200, 0, 275 };
        const int lx1[4] = { 125, 275, 50, 319 };
        lbxgfx_draw_frame_offs(0, 0, d->gfx_racem[i], lx0[i], 0, lx1[i], UI_VGA_H - 1, UI_SCREEN_W, ui_scale);
    }
    if (el->str) {
        lbxfont_select_set_12_1(3, 0, 0, 0);
        lbxfont_set_gap_h(1);
        lbxfont_print_str_split(10, 169, 305, el->str, 0, UI_SCREEN_W, UI_SCREEN_H, ui_scale);
    }
    if (d->flag_countdown) {
        if (--d->count <= 0) {
            d->flag_countdown = false;
            d->count = 0;
        }
    }
#if 0 /* unused, flag is always 0 or 1 */
    if (d->flag_countdown == 2) {
        if (++d->count >= 10) {
            d->flag_countdown = false;
            d->count = 0;
        }
    }
#endif
    if (el->flag_show_votes) {
        char buf[0x40];
        char vbuf[0x20];
        uint16_t n;
        lbxfont_select(3, 1, 0, 0);
        n = el->got_votes[0];
        lib_sprintf(buf, sizeof(buf), "%s %s", game_election_print_votes(n, vbuf, sizeof(vbuf)), g->emperor_names[el->candidate[0]]);
        lbxfont_print_str_normal(10, 10, buf, UI_SCREEN_W, ui_scale);
        n = el->got_votes[1];
        lib_sprintf(buf, sizeof(buf), "%s %s", game_election_print_votes(n, vbuf, sizeof(vbuf)), g->emperor_names[el->candidate[1]]);
        lbxfont_print_str_right(310, 10, buf, UI_SCREEN_W, ui_scale);
        n = el->total_votes;
        lib_sprintf(buf, sizeof(buf), "%s %s", game_election_print_votes(n, vbuf, sizeof(vbuf)), game_str_el_total);
        lbxfont_print_str_center(160, 10, buf, UI_SCREEN_W, ui_scale);
    }
}

/* -------------------------------------------------------------------------- */

void ui_election_start(struct election_s *el)
{
    static struct election_data_s d;    /* HACK */
    d.el = el;
    el->uictx = &d;
    ui_switch_all(el->g);
    hw_video_copy_back_from_page2();
    hw_video_copy_back_to_page3();
    if (ui_draw_finish_mode == 0) {
        ui_palette_fadeout_a_f_1();
    }
    ui_draw_finish_mode = 2;
    ui_sound_stop_music();
    ui_delay_1();
    lbxpal_select(10, -1, 0);
    lbxpal_set_update_range(0, 255);
    lbxpal_build_colortables();
    election_load_data(&d);
    ui_sound_play_music(0x27);
    uiobj_table_clear();
    uiobj_set_callback_and_delay(ui_election_draw_cb, &d, 3);
}

void ui_election_show(struct election_s *el)
{
    struct election_data_s *d = el->uictx;
    bool flag_done = false;
    d->count = 0;
    uiobj_table_clear();
    uiobj_add_mousearea_all(MOO_KEY_SPACE);
    uiobj_set_downcount(1);
    while (!flag_done) {
        int16_t oi;
        ui_delay_prepare();
        oi = uiobj_handle_input_cond();
        if (oi != 0) {
            flag_done = true;
            break;
        }
        ui_election_draw_cb(d);
        ui_delay_ticks_or_click(el->ui_delay);
        ui_draw_finish();
    }
    uiobj_table_clear();
}

void ui_election_delay(struct election_s *el, int delay)
{
    struct election_data_s *d = el->uictx;
    d->flag_countdown = true;
    d->count = delay;
    while (d->flag_countdown) {
        ui_delay_prepare();
        ui_election_draw_cb(d);
        ui_delay_ticks_or_click(el->ui_delay);
        ui_draw_finish();
    }
}

int ui_election_vote(struct election_s *el, int player_i)
{
    struct election_data_s *d = el->uictx;
    struct game_s *g = el->g;
    char cnamebuf[3][0x20];
    int16_t oi, oi_c1, oi_c2, choice = -1;
    bool flag_done = false;
    for (int i = 0; i < 2; ++i) {
        player_id_t pi;
        pi = el->candidate[i];
        lib_sprintf(cnamebuf[i], 0x20, "%s %s", game_str_el_bull, (pi == player_i) ? game_str_el_self : g->emperor_names[pi]);
    }
    lib_sprintf(cnamebuf[2], 0x20, "%s %s", game_str_el_bull, game_str_el_abs);
    uiobj_table_clear();
    lbxfont_select(3, 1, 0, 0);
    oi_c1 = uiobj_add_mousearea(150, 169, 190, 177, MOO_KEY_1);
    oi_c2 = uiobj_add_mousearea(150, 179, 190, 187, MOO_KEY_2);
    /*oi_ca =*/ uiobj_add_mousearea(150, 189, 190, 197, MOO_KEY_0);
    uiobj_add_ta(150, 169, 40, cnamebuf[0], false, &choice, 1, 0, 0, MOO_KEY_UNKNOWN);
    uiobj_add_ta(150, 179, 40, cnamebuf[1], false, &choice, 2, 0, 0, MOO_KEY_UNKNOWN);
    uiobj_add_ta(150, 189, 40, cnamebuf[2], false, &choice, 0, 0, 0, MOO_KEY_UNKNOWN);
    while (!flag_done) {
        ui_delay_prepare();
        oi = uiobj_handle_input_cond();
        if (oi != 0) {
            flag_done = true;
            break;
        }
        ui_election_draw_cb(d);
        ui_delay_ticks_or_click(3);
        ui_draw_finish();
    }
    uiobj_table_clear();
    if (oi == oi_c1) {
        return 1;
    }
    if (oi == oi_c2) {
        return 2;
    }
    return 0;
}

bool ui_election_accept(struct election_s *el, int player_i)
{
    struct election_data_s *d = el->uictx;
    char buf[2][0x20];
    int16_t oi_y, oi_n, choice = -1, y = 169;
    bool flag_done = false, flag_accept = false;
    lib_sprintf(buf[0], 0x20, "%s %s", game_str_el_bull, game_str_el_yes);
    lib_sprintf(buf[1], 0x20, "%s %s", game_str_el_bull, game_str_el_no2);
    uiobj_table_clear();
    lbxfont_select_set_12_1(3, 0, 0, 0);
    if (lbxfont_calc_str_width(el->str) >= (160 - 10)) {
        y += 10;
    }
    lbxfont_select(3, 1, 0, 0);
    oi_y = uiobj_add_mousearea(160, y, 200, y + 8, MOO_KEY_y);
    oi_n = uiobj_add_mousearea(160, y + 10, 200, y + 18, MOO_KEY_n);
    uiobj_add_ta(160, y, 40, buf[0], false, &choice, 1, 0, 0, MOO_KEY_UNKNOWN);
    uiobj_add_ta(160, y + 10, 40, buf[1], false, &choice, 0, 0, 0, MOO_KEY_UNKNOWN);
    while (!flag_done) {
        int16_t oi;
        ui_delay_prepare();
        oi = uiobj_handle_input_cond();
        if ((oi == oi_y) || (oi == UIOBJI_ESC)) {
            flag_accept = true;
        }
        if (oi == oi_n) {
            flag_accept = false;
        }
        if (oi != 0) {
            ui_sound_play_sfx_24();
            flag_done = true;
        }
        ui_election_draw_cb(d);
        ui_delay_ticks_or_click(3);
        ui_draw_finish();
    }
    uiobj_table_clear();
    return flag_accept;
}

void ui_election_end(struct election_s *el)
{
    struct election_data_s *d = el->uictx;
    ui_sound_stop_music();
    ui_palette_fadeout_a_f_1();
    ui_draw_finish_mode = 2;
    hw_video_copy_back_from_page3();
    hw_video_copy_back_to_page2();
    lbxpal_select(0, -1, 0);
    lbxpal_set_update_range(0, 255);
    lbxpal_build_colortables();
    uiobj_unset_callback();
    uiobj_table_clear();
    election_free_data(d);
}
