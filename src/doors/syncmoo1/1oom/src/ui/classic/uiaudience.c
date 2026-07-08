#include "config.h"

#include <stdio.h>

#include "ui.h"
#include "comp.h"
#include "game_audience.h"
#include "game_aux.h"
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

struct audience_data_s {
    struct audience_s *au;
    uint8_t *gfx_border;
    uint8_t *gfx_race;
    uint8_t *gfx_emperor;
    int8_t music_0;
    int8_t music_1;
    int8_t music_i;
    uint8_t gfxi;
    int delay;
};

static const int8_t tbl_audience_music[RACE_NUM] = {
    0x16, 0x14, 0x22, 0x20, 0x1c, 0x12, 0x18, 0x24, 0x1a, 0x1e
};

/* -------------------------------------------------------------------------- */

static void audience_load_data(struct audience_data_s *d)
{
    struct game_s *g = d->au->g;
    race_t ra = g->eto[d->au->pa].race;
    d->gfx_border = lbxfile_item_get(LBXFILE_EMBASSY, 0);
    {
        uint8_t *gfx;
        gfx = lbxfile_item_get(LBXFILE_EMBASSY, 1);
        ui_draw_erase_buf();
        lbxgfx_draw_frame(0, 0, gfx, UI_SCREEN_W, ui_scale);
        uiobj_table_clear();
        uiobj_finish_frame();
        ui_draw_erase_buf();
        lbxgfx_draw_frame(0, 0, gfx, UI_SCREEN_W, ui_scale);
        lbxfile_item_release(LBXFILE_EMBASSY, gfx);
    }
    d->gfx_race = lbxfile_item_get(LBXFILE_EMBASSY, 0x2 + ra);
    d->gfx_emperor = lbxfile_item_get(LBXFILE_EMBASSY, 0xc + g->eto[d->au->ph].banner);
    d->music_0 = tbl_audience_music[ra];
    d->music_1 = tbl_audience_music[ra] + 1;
}

static void audience_free_data(struct audience_data_s *d)
{
    lbxfile_item_release(LBXFILE_EMBASSY, d->gfx_border);
    lbxfile_item_release(LBXFILE_EMBASSY, d->gfx_race);
    lbxfile_item_release(LBXFILE_EMBASSY, d->gfx_emperor);
}

static void ui_audience_draw_race(struct audience_data_s *d)
{
    struct audience_s *au = d->au;
    uint8_t *gfx = d->gfx_race;
    int frame = -1, x = 96;
    if (au->gfxi != d->gfxi) {
        switch (au->gfxi) {
            case 0:
            case 1:
            case 2:
                frame = 2;
                break;
            case 3:
                frame = 0;
                break;
            case 4:
            case 5:
                frame = 1;
                break;
            default:
                break;
        }
        if (frame >= 0) {
            lbxgfx_set_new_frame(gfx, frame);
        }
    }
    frame = lbxgfx_get_frame(gfx);
    switch (au->gfxi) {
        case 0:
        draw_anim:
            SETMAX(frame, 2);
            lbxgfx_set_new_frame(gfx, 2);
            for (int f = 0; f <= (frame - 2); ++f) {
                lbxgfx_draw_frame(x, 0, gfx, UI_SCREEN_W, ui_scale);
            }
            d->delay = (d->delay + 1) % 4;
            if (d->delay != 0) {
                lbxgfx_set_new_frame(gfx, frame);
            }
            break;
        case 1:
            if (frame < 2) {
                lbxgfx_set_frame_0(gfx);
                lbxgfx_draw_frame(x, 0, gfx, UI_SCREEN_W, ui_scale);
                d->delay = (d->delay + 1) % 4;
            } else {
                goto draw_anim;
            }
            break;
        case 2:
            if (frame < 2) {
                lbxgfx_set_frame_0(gfx);
                lbxgfx_draw_frame(x, 0, gfx, UI_SCREEN_W, ui_scale);
                lbxgfx_draw_frame(x, 0, gfx, UI_SCREEN_W, ui_scale);
                d->delay = (d->delay + 1) % 4;
            } else {
                goto draw_anim;
            }
            break;
        case 3:
            lbxgfx_set_frame_0(gfx);
            lbxgfx_draw_frame(x, 0, gfx, UI_SCREEN_W, ui_scale);
            break;
        case 4:
            lbxgfx_set_frame_0(gfx);
            lbxgfx_draw_frame(x, 0, gfx, UI_SCREEN_W, ui_scale);
            lbxgfx_draw_frame(x, 0, gfx, UI_SCREEN_W, ui_scale);
            break;
        case 5:
        default:
            break;
    }
    d->gfxi = au->gfxi;
}

static void ui_audience_play_music(struct audience_data_s *d, uint8_t musi)
{
    int mi = -1;
    if ((musi == 0) || (musi == 1)) {
        mi = d->music_0;
    } else if (musi == 2) {
        mi = d->music_1;
    }
    if ((mi >= 0) && (d->music_i != mi)) {
        d->music_i = mi;
        ui_sound_play_music(mi);
    }
}

static void ui_audience_draw_cb1(void *vptr)
{
    struct audience_data_s *d = vptr;
    struct audience_s *au = d->au;
    if (au->mode != 1) {
        ui_audience_draw_race(d);
    }
    lbxgfx_draw_frame(0, -2, d->gfx_emperor, UI_SCREEN_W, ui_scale);
    lbxgfx_draw_frame(0, 0, d->gfx_border, UI_SCREEN_W, ui_scale);
    if ((au->mode >= 0) && (au->mode <= 2)) {
        lbxfont_select(3, 2, 0, 0);
        lbxfont_set_gap_h(1);
        lbxfont_print_str_split(38, 140, 245, au->buf, 0, UI_SCREEN_W, UI_SCREEN_H, ui_scale);
    }
    /*d->delay2 = (d->delay2 + 1) % 6;*/
}

static void ui_audience_draw_cb2(void *vptr)
{
    struct audience_data_s *d = vptr;
    struct audience_s *au = d->au;
    int strh;
    ui_audience_draw_race(d);
    lbxgfx_draw_frame(0, -2, d->gfx_emperor, UI_SCREEN_W, ui_scale);
    lbxgfx_draw_frame(0, 0, d->gfx_border, UI_SCREEN_W, ui_scale);
    lbxfont_select(3, 1, 0, 0);
    lbxfont_set_gap_h(1);
    strh = lbxfont_calc_split_str_h(245, au->buf);
    if (strh > 39) {
        lbxfont_set_gap_h(0);
    }
    lbxfont_print_str_split(38, 140, 245, au->buf, 0, UI_SCREEN_W, UI_SCREEN_H, ui_scale);
    /*d->delay2 = (d->delay2 + 1) % 6;*/
}

static void ui_audience_draw_cb3(void *vptr)
{
    struct audience_data_s *d = vptr;
    struct audience_s *au = d->au;
    ui_audience_draw_race(d);
    lbxgfx_draw_frame(0, -2, d->gfx_emperor, UI_SCREEN_W, ui_scale);
    lbxgfx_draw_frame(0, 0, d->gfx_border, UI_SCREEN_W, ui_scale);
    lbxfont_select(3, 2, 1, 0);
    lbxfont_set_gap_h(1);
    lbxfont_print_str_split(38, 140, 245, au->buf, 0, UI_SCREEN_W, UI_SCREEN_H, ui_scale);
    /*d->delay2 = (d->delay2 + 1) % 6;*/
}

static void ui_audience_draw_cb4(void *vptr)    /* FIXME combine with cb3 and/or cb2 */
{
    struct audience_data_s *d = vptr;
    struct audience_s *au = d->au;
    int strh;
    ui_audience_draw_race(d);
    lbxgfx_draw_frame(0, -2, d->gfx_emperor, UI_SCREEN_W, ui_scale);
    lbxgfx_draw_frame(0, 0, d->gfx_border, UI_SCREEN_W, ui_scale);
    lbxfont_select(3, 1, 0, 0);
    lbxfont_set_gap_h(1);
    strh = lbxfont_calc_split_str_h(245, au->buf);
    if (strh > 39) {
        lbxfont_set_gap_h(0);
    }
    lbxfont_print_str_split(38, 136, 245, au->buf, 0, UI_SCREEN_W, UI_SCREEN_H, ui_scale);
    /*d->delay2 = (d->delay2 + 1) % 6;*/
}

static int16_t ui_audience_ask_do(struct audience_s *au, int y, void (*draw_cb)(void *vptr))
{
    int16_t selected = 0;
    ui_audience_play_music(au->uictx, au->musi); /* side effect of game_audience_get_diplo_msg() */
    uiobj_set_callback_and_delay(draw_cb, au->uictx, 1);
    uiobj_set_downcount(1);
    lbxfont_select(0, 2, 3, 0);
    selected = uiobj_select_from_list1(38, y, 245, "", au->strtbl, &selected, au->condtbl, 0xf, 0xb, true);
    return selected;
}

/* -------------------------------------------------------------------------- */

void ui_audience_start(struct audience_s *au)
{
    static struct audience_data_s d;    /* HACK */
    ui_switch_2(au->g, au->ph, au->pa);
    d.au = au;
    au->uictx = &d;
    d.delay = 0;
    d.music_i = -1;
    d.gfxi = 0xff;
    if (ui_draw_finish_mode == 0) {
        ui_palette_fadeout_a_f_1();
    }
    ui_draw_finish_mode = 2;
    ui_sound_stop_music();
    ui_delay_1();
    audience_load_data(&d);
    lbxpal_select(7, -1, 0);
    lbxpal_set_update_range(0, 255);
    lbxpal_build_colortables();
    uiobj_table_clear();
    uiobj_set_callback_and_delay(ui_audience_draw_cb1, &d, 3);
    uiobj_set_downcount(1);
}

void ui_audience_show1(struct audience_s *au)
{
    struct audience_data_s *d = au->uictx;
    bool flag_done = false;
    int16_t oi_ma = UIOBJI_INVALID;
    ui_audience_play_music(au->uictx, au->musi); /* side effect of game_audience_get_diplo_msg() */
    uiobj_set_callback_and_delay(ui_audience_draw_cb1, d, 1);
    uiobj_set_downcount(1);
    uiobj_table_clear();
    while (!flag_done) {
        int16_t oi;
        ui_delay_prepare();
        oi = uiobj_handle_input_cond();
        if ((oi == oi_ma) || (oi == UIOBJI_ESC)) {
            flag_done = true;
        }
        uiobj_table_clear();
        oi_ma = UIOBJI_INVALID;
        if ((au->mode >= 0) && (au->mode <= 2)) {
            oi_ma = uiobj_add_mousearea_all(MOO_KEY_UNKNOWN);
        }
        if (!flag_done) {
            ui_audience_draw_cb1(d);
            ui_delay_ticks_or_click(1);
            ui_draw_finish();
        }
    }
    uiobj_table_clear();
}

void ui_audience_show2(struct audience_s *au)
{
    struct audience_data_s *d = au->uictx;
    int16_t oi = 0;
    ui_audience_play_music(d, au->musi); /* side effect of game_audience_get_diplo_msg() */
    uiobj_set_callback_and_delay(ui_audience_draw_cb2, d, 1);
    uiobj_set_downcount(1);
    uiobj_table_clear();
    uiobj_add_mousearea_all(MOO_KEY_UNKNOWN);
    while (oi == 0) {
        ui_delay_prepare();
        oi = uiobj_handle_input_cond();
        if (oi == 0) {
            ui_audience_draw_cb2(d);
            ui_draw_finish();
            ui_delay_ticks_or_click(1);
        }
    }
    uiobj_table_clear();
}

void ui_audience_show3(struct audience_s *au)
{
    struct audience_data_s *d = au->uictx;
    bool flag_done = false;
    ui_audience_play_music(au->uictx, au->musi); /* side effect of game_audience_get_diplo_msg() */
    uiobj_set_callback_and_delay(ui_audience_draw_cb3, d, 1);
    uiobj_set_downcount(1);
    uiobj_table_clear();
    uiobj_add_mousearea_all(MOO_KEY_UNKNOWN);
    while (!flag_done) {
        int16_t oi;
        ui_delay_prepare();
        oi = uiobj_handle_input_cond();
        if (oi != 0) {
            flag_done = true;
        }
        if (!flag_done) {
            ui_audience_draw_cb3(d);
            ui_delay_ticks_or_click(1);
            ui_draw_finish();
        }
    }
    uiobj_table_clear();
}

int16_t ui_audience_ask2a(struct audience_s *au)
{
    return ui_audience_ask_do(au, 145, ui_audience_draw_cb2);
}

int16_t ui_audience_ask2b(struct audience_s *au)
{
    int strh;
    lbxfont_select(3, 1, 0, 0);
    lbxfont_set_gap_h(1);
    strh = lbxfont_calc_split_str_h(245, au->buf);
    if (strh > 39) {
        lbxfont_set_gap_h(0);
        strh = lbxfont_calc_split_str_h(245, au->buf);
    }
    return ui_audience_ask_do(au, 133 + strh, ui_audience_draw_cb2);
}

int16_t ui_audience_ask3(struct audience_s *au)
{
    int strh;
    lbxfont_select(3, 2, 1, 0);
    lbxfont_set_gap_h(1);
    strh = lbxfont_calc_split_str_h(245, au->buf);
    return ui_audience_ask_do(au, 139 + strh, ui_audience_draw_cb3);
}

int16_t ui_audience_ask4(struct audience_s *au)
{
    return ui_audience_ask_do(au, 139, ui_audience_draw_cb4);
}

void ui_audience_newtech(struct audience_s *au, int pi)
{
    bool flag_switched;
    ui_palette_fadeout_a_f_1();
    ui_draw_finish_mode = 2;
    lbxpal_select(0, -1, 0);
    lbxpal_set_update_range(0, 255);
    lbxpal_build_colortables();
    if (pi == PLAYER_NONE) {
        ui_newtech(au->g, au->ph);
        ui_newtech(au->g, au->pa);
    } else {
        ui_newtech(au->g, pi);
    }
    flag_switched = ui_switch_2(au->g, au->ph, au->pa);
    if (flag_switched) {
        ui_palette_fadeout_a_f_1();
    }
    lbxpal_select(7, -1, 0);
    lbxpal_set_update_range(0, 255);
    if ((!flag_switched) && (ui_draw_finish_mode == 0)) {
        ui_palette_fadeout_a_f_1();
    }
    ui_draw_erase_buf();
    uiobj_finish_frame();
    ui_draw_erase_buf();
    audience_free_data(au->uictx);
    audience_load_data(au->uictx);
    if (flag_switched) {
        ui_draw_finish_mode = 2;
    } else {
        ui_draw_finish_mode = 0;
    }
    lbxpal_build_colortables();
    ui_sound_play_music(((struct audience_data_s *)(au->uictx))->music_0);
}

void ui_audience_end(struct audience_s *au)
{
    struct audience_data_s *d = au->uictx;
    uiobj_unset_callback();
    uiobj_table_clear();
    ui_sound_stop_music();
    ui_palette_fadeout_a_f_1();
    ui_draw_finish_mode = 2;
    lbxpal_select(0, -1, 0);
    lbxpal_set_update_range(0, 255);
    lbxpal_build_colortables();
    ui_switch_1(au->g, au->ph);
    audience_free_data(d);
}
