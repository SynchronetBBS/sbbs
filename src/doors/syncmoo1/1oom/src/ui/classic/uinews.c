#include "config.h"

#include <stdio.h>

#include "ui.h"
#include "game_news.h"
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
#include "uinews.h"
#include "uiobj.h"
#include "uipal.h"
#include "uisound.h"
#include "uiswitch.h"

/* -------------------------------------------------------------------------- */

struct news_data_s {
    int frame;
    struct news_s *ns;
    const char *str;
};

/* -------------------------------------------------------------------------- */

static void news_free_data(void)
{
    if (ui_data.gfx.news.icon != 0) {
        lbxfile_item_release(LBXFILE_NEWSCAST, ui_data.gfx.news.icon);
        ui_data.gfx.news.icon = 0;
    }
}

static void news_load_data(news_type_t type)
{
    if (!ui_data.news.flag_also) {
        news_free_data();
        lbxgfx_set_frame_0(ui_data.gfx.news.nc);
        lbxgfx_set_frame_0(ui_data.gfx.news.world);
        lbxgfx_set_frame_0(ui_data.gfx.news.gnn);
    }
    if (type != GAME_NEWS_NONE) {
        ui_data.gfx.news.icon = lbxfile_item_get(LBXFILE_NEWSCAST, 3 + (int)type);
    } else {
        news_free_data();
    }
}

static void ui_news_cb1(void *vptr)
{
    struct news_data_s *d = vptr;
    ui_draw_filled_rect(32, 142, 287, 182, 0, ui_scale);
    ui_draw_filled_rect(34, 184, 285, 191, 0, ui_scale);
    ui_draw_line1(33, 182, 286, 182, 0, ui_scale);
    ui_draw_line1(32, 183, 287, 183, 0, ui_scale);
    {
        uint8_t *gfx = ui_data.gfx.news.nc;
        int fn = lbxgfx_get_frame(gfx);
        lbxgfx_set_frame_0(gfx);
        for (int f = 0; f <= fn; ++f) {
            lbxgfx_draw_frame(14, 14, gfx, UI_SCREEN_W, ui_scale);
        }
    }
    if (ui_data.gfx.news.icon != 0) {
        lbxgfx_draw_frame(208, 38, ui_data.gfx.news.icon, UI_SCREEN_W, ui_scale);
    }
    {
        uint8_t *gfx = ui_data.gfx.news.world;
        int fn = lbxgfx_get_frame(gfx);
        lbxgfx_set_frame_0(gfx);
        for (int f = 0; f <= fn; ++f) {
            lbxgfx_draw_frame(76, 36, gfx, UI_SCREEN_W, ui_scale);
        }
    }
    lbxfont_select(3, 1, 0, 0);
    ui_draw_filled_rect(38, 145, 284, 190, 0, ui_scale);
    lbxfont_set_space_w(2);
    lbxfont_print_str_split(38, 145, 245, d->str, 3, UI_SCREEN_W, UI_SCREEN_H, ui_scale);
    if (d->ns->type == GAME_NEWS_STATS) {
        lbxfont_select(3, 1, 0, 0);
        for (int i = 0; i < d->ns->statsnum; ++i) {
            char buf[5];
            int x, y;
            x = 48 + (i / 3) * 122;
            y = 157 + (i % 3) * 10;
            lib_sprintf(buf, sizeof(buf), "%i.", i + 1);
            lbxfont_print_str_right(x, y, buf, UI_SCREEN_W, ui_scale);
            lbxfont_print_str_normal(x + 7, y, d->ns->stats[i], UI_SCREEN_W, ui_scale);
        }
    }
    ++d->frame;
}

static void ui_news_draw_start_anim(void)
{
    int frame;
    ui_delay_1();
    ui_sound_stop_music();
    uiobj_table_clear();
    ui_draw_erase_buf();
    lbxgfx_draw_frame(0, 0, ui_data.gfx.news.tv, UI_SCREEN_W, ui_scale);
    uiobj_finish_frame();
    ui_draw_erase_buf();
    lbxgfx_draw_frame(0, 0, ui_data.gfx.news.tv, UI_SCREEN_W, ui_scale);
    ui_sound_play_music(9);
    frame = 0;
    while (frame < 25) {
        ui_delay_prepare();
        if (frame > 0) {
            uint16_t f;
            f = lbxgfx_get_frame(ui_data.gfx.news.gnn) - 1;
            lbxgfx_set_new_frame(ui_data.gfx.news.gnn, f);
            lbxgfx_draw_frame(14, 14, ui_data.gfx.news.gnn, UI_SCREEN_W, ui_scale);
        }
        lbxgfx_draw_frame(14, 14, ui_data.gfx.news.gnn, UI_SCREEN_W, ui_scale);
        ui_draw_filled_rect(32, 142, 287, 182, 0xc1, ui_scale);
        ui_draw_filled_rect(34, 184, 285, 191, 0xc1, ui_scale);
        ui_draw_line1(33, 182, 286, 182, 0xc1, ui_scale);
        ui_draw_line1(32, 183, 287, 183, 0xc1, ui_scale);
        ui_draw_finish();
        ui_delay_ticks_or_click(1);
        ++frame;
    }
}

/* -------------------------------------------------------------------------- */

static uint8_t ui_news_fade_tbl_xoff[] = {
    2, 2, 2, 1, 1, 3, 0, 3, 3, 2, 0, 1, 3, 3, 1, 3,
    3, 0, 3, 1, 2, 3, 0, 2, 1, 1, 2, 2, 1, 1, 3, 3,
    2, 3, 3, 0, 2, 3, 0, 2, 0, 3, 2, 0, 2, 0, 0, 3,
    1, 0, 1, 1, 1, 3, 3, 1, 1, 0, 0, 2, 1, 1, 2, 1,
    0, 0, 0, 3, 3, 1, 1, 2, 2, 1, 2, 2, 0, 2, 3, 1,
    2, 2, 2, 0, 0, 0, 1, 1, 0, 2, 0, 3, 2, 2, 1, 0,
    3, 1, 2, 3, 0, 0, 1, 0, 2, 2, 3, 2, 1, 1, 2, 2,
    0, 3, 0, 0, 2, 0, 0, 3, 3, 1, 2, 1, 0, 0, 1, 3,
    1, 1, 1, 0, 0, 2, 3, 1, 0, 3, 3, 0, 1, 0, 0, 0,
    1, 3, 1, 3, 3, 2, 2, 3, 2, 0, 1, 1, 0, 3, 0, 2,
    0, 2, 1, 1, 3, 1, 2, 1, 3, 1, 0, 1, 3, 3, 1, 1,
    3, 2, 3, 2, 3, 1, 1, 2, 0, 2, 3, 3, 2, 2, 0, 2,
    3, 3, 3, 2, 2, 0, 2, 0, 1, 0, 1, 3, 2, 1, 2, 2,
    0, 1, 0, 2, 1, 1, 3, 0, 3, 3, 3, 0, 3, 0, 2, 1,
    1, 0, 0, 2, 1, 2, 3, 3, 1, 0, 1, 3, 0, 2, 3, 0,
    2, 1, 2, 3, 0, 2, 2, 0, 2, 3, 1, 0, 3, 3, 3, 0
};

static uint8_t ui_news_fade_tbl_line[] = {
    0, 3, 30, 18, 28, 22, 29, 12, 34, 47, 31, 32, 7, 49, 46, 14,
    38, 43, 35, 40, 11, 9, 36, 4, 33, 26, 39, 15, 5, 21, 2, 16,
    25, 10, 48, 13, 24, 37, 17, 19, 44, 6, 8, 20, 1, 23, 41, 27,
    45, 42
};

static uint8_t ui_news_fade_tbl_col[] = {
    2, 9, 30, 38, 13, 27, 31, 1, 19, 8, 24, 36, 37, 26, 0, 11,
    23, 32, 22, 12, 16, 20, 29, 18, 28, 7, 10, 35, 14, 25, 5, 33,
    15, 21, 4, 17, 6, 3, 39, 34
};

static inline void ui_news_fade_plot(uint8_t *pb, uint8_t *pf, int si, uint8_t ah)
{
    if (ui_scale == 1) {
        pf[si * 4 + ah] = pb[si * 4 + ah];
    } else {
        pf += (si * 4 + ah) * ui_scale;
        pb += (si * 4 + ah) * ui_scale;
        for (int y = 0; y < ui_scale; ++y) {
            for (int x = 0; x < ui_scale; ++x) {
                pf[x] = pb[x];
            }
            pf += UI_SCREEN_W;
            pb += UI_SCREEN_W;
        }
    }
}

#define UI_NEWS_FADE_PIXELS_PER_FRAME  12000

static void ui_news_fade(void)
{
    int pixelcount = UI_NEWS_FADE_PIXELS_PER_FRAME;
    uint8_t *pb, *pf;
    pb = hw_video_get_buf();
    pf = hw_video_get_buf_front();
    for (int loops = 4; loops > 0; --loops) {
        int we0;
        we0 = (loops - 1) << 6;
        for (int wde = 39; wde >= 0; --wde) {
            for (int we2 = 49; we2 >= 0; --we2) {
                int dx, v, si;
                uint8_t bl, ah;
                v = dx = ui_news_fade_tbl_line[we2];
                si = v * UI_SCREEN_W / 4;
                v += wde;
                if (v >= 40) {
                    v -= 40;
                    if (v >= 40) {
                        v -= 40;
                    }
                }
                v = ui_news_fade_tbl_col[v];
                bl = v;
                si += v * 2;
                bl += dx + we0;
                ah = ui_news_fade_tbl_xoff[bl++];
                ui_news_fade_plot(pb, pf, si, ah);
                si += 8000;
                ui_news_fade_plot(pb, pf, si, ah);
                si -= 4000;
                ui_news_fade_plot(pb, pf, si, ah);
                si += 8000;
                ui_news_fade_plot(pb, pf, si, ah);
                si -= 12000;
                ah = ui_news_fade_tbl_xoff[bl++];
                ui_news_fade_plot(pb, pf, si, ah);
                si += 8000;
                ui_news_fade_plot(pb, pf, si, ah);
                si -= 4000;
                ui_news_fade_plot(pb, pf, si, ah);
                si += 8000;
                ui_news_fade_plot(pb, pf, si, ah);
                pixelcount -= 8;
                if (pixelcount <= 0) {
                    pixelcount = UI_NEWS_FADE_PIXELS_PER_FRAME;
                    hw_video_redraw_front();
                    ui_delay_1();
                }
            }
        }
    }
    memcpy(pf, pb, UI_SCREEN_W * UI_SCREEN_H);
    hw_video_redraw_front();
    ui_delay_1();
}

/* -------------------------------------------------------------------------- */

void ui_news_won(bool flag_good)
{
    bool flag_skip = false, flag_fade;
    struct news_data_s d;
    struct news_s ns;

    lbxpal_select(0, -1, 0);
    lbxpal_set_update_range(0, 255);

    ui_draw_finish_mode = 2;

    d.str = flag_good ? game_str_gnn_end_good : game_str_gnn_end_tyrant;
    d.ns = &ns;
    ns.type = GAME_NEWS_NONE;
    news_load_data(GAME_NEWS_NONE);

    ui_news_draw_start_anim();

    uiobj_table_clear();
    uiobj_add_mousearea_all(MOO_KEY_SPACE);
    uiobj_set_downcount(1);
    uiobj_set_callback_and_delay(ui_news_cb1, &d, 3);

    flag_fade = true;
    d.frame = 0;
    for (int i = 0; (i < 0x46) && !flag_skip; ++i) {
        int16_t oi;
        ui_delay_prepare();
        oi = uiobj_handle_input_cond();
        if (oi != 0) {
            flag_skip = true;
            break;
        }
        ui_news_cb1(&d);
        ui_delay_ticks_or_click(3);
        if (flag_fade) {
            ui_news_fade();
        } else {
            ui_draw_finish();
        }
        flag_fade = false;
    }

    hw_audio_music_fadeout();
    uiobj_unset_callback();
    ui_data.news.flag_also = false;
}

void ui_news(struct game_s *g, struct news_s *ns)
{
    bool flag_skip = false, flag_fade;
    struct news_data_s d;
    ui_switch_all(g);
    d.ns = ns;
    if (!ui_data.news.flag_also) {
        if (ui_draw_finish_mode == 0) {
            ui_palette_fadeout_a_f_1();
        }
        ui_draw_finish_mode = 2;
        ui_news_draw_start_anim();
        flag_fade = true;
    } else {
        d.str = game_str_gnn_also;
        for (int i = 0; (i < 5) && !flag_skip; ++i) {
            int16_t oi;
            ui_delay_prepare();
            oi = uiobj_handle_input_cond();
            if (oi != 0) {
                flag_skip = true;
            }
            if (!flag_skip) {
                ui_news_cb1(&d);
                ui_delay_ticks_or_click(3);
                ui_draw_finish();
            }
        }
        flag_fade = false;
    }
    game_news_get_msg(g, ns, ui_data.strbuf, UI_STRBUF_SIZE);
    d.str = ui_data.strbuf;

    news_load_data(ns->type);
    uiobj_table_clear();
    uiobj_add_mousearea_all(MOO_KEY_SPACE);
    uiobj_set_downcount(1);
    uiobj_set_callback_and_delay(ui_news_cb1, &d, 3);

    flag_skip = false;
    while (!flag_skip) {
        int16_t oi;
        ui_delay_prepare();
        oi = uiobj_handle_input_cond();
        if (oi != 0) {
            flag_skip = true;
            break;
        }
        ui_news_cb1(&d);
        ui_delay_ticks_or_click(3);
        if (flag_fade) {
            ui_news_fade();
        } else {
            ui_draw_finish();
        }
        flag_fade = false;
    }
    ui_data.news.flag_also = true;
    ui_sound_stop_music();
    uiobj_unset_callback();
    uiobj_table_clear();
}

void ui_news_start(void)
{
    ui_data.news.flag_also = false;
}

void ui_news_end(void)
{
    if (ui_data.news.flag_also) {
        ui_data.news.flag_also = false;
        ui_palette_fadeout_a_f_1();
        ui_draw_finish_mode = 2;
    }
}
