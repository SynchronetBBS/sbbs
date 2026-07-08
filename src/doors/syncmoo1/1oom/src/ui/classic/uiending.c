#include "config.h"

#include <string.h>

#include "ui.h"
#include "game_str.h"
#include "hw.h"
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

/* -------------------------------------------------------------------------- */

struct anim_winlose_1_s {
    int frame;
    uint8_t *gfx_stars;
    uint8_t *gfx_planets;
    uint8_t *gfx_ships;
    const char *name;
};

struct anim_winlose_2_s {
    int frame;
    uint8_t *gfx_winning2;
};

struct anim_winlose_3_s {
    int frame;
    uint8_t *gfx_winlast;
    uint8_t *gfx_winface;
    const char *str1;
    const char *str2;
    const char *str3;
    const char *str4;
    const char *name;
    bool flag_good;
};

struct anim_winlose_exile_s {
    int frame;
    uint8_t *gfx_ships;
    uint8_t *gfx_stars;
    const char *name;
};

struct anim_winlose_funeral_s {
    int frame;
    uint8_t *gfx_lose;
    uint8_t *gfx_flag;
    uint8_t *gfx_coffin;
    uint8_t *gfx_march;
};

/* -------------------------------------------------------------------------- */

static bool check_lbx_file(void)
{
    if (!lbxfile_exists(LBXFILE_WINLOSE)) {
        log_warning("skipping ending due to missing %s\n", lbxfile_name(LBXFILE_WINLOSE));
        uiobj_table_clear();
        uiobj_unset_callback();
        ui_draw_erase_buf();
        uiobj_finish_frame();
        ui_draw_erase_buf();
        return false;
    }
    return true;
}

static void ui_play_winlose_cb1(void *vptr)
{
    struct anim_winlose_1_s *p = vptr;
    int f = p->frame;
    ui_draw_erase_buf();
    lbxgfx_draw_frame_offs(0 - f, 0, p->gfx_stars, 0, 0, UI_VGA_W - 1, UI_VGA_H - 1, UI_SCREEN_W, ui_scale);
    lbxgfx_draw_frame_offs(UI_VGA_W - f, 0, p->gfx_stars, 0, 0, UI_VGA_W - 1, UI_VGA_H - 1, UI_SCREEN_W, ui_scale);
    if (f > 0x32) {
        lbxgfx_draw_frame(0, 0, p->gfx_ships, UI_SCREEN_W, ui_scale);
        lbxgfx_draw_frame_offs(UI_VGA_W - 1 - ((f - 0x32) * 3) / 2, 0, p->gfx_planets, 0, 0, UI_VGA_W - 1, UI_VGA_H - 1, UI_SCREEN_W, ui_scale);
    }
    lbxfont_select(4, 0, 0, 0);
    if ((f > 0xa) && (f < 0x14)) {
        lbxpal_set_update_range(2, 0x20);
        ui_palette_fade_n(0x64 - (f - 0xa) * 0xa);
    }
    if ((f > 0x3c) && (f < 0x46)) {
        lbxpal_set_update_range(2, 0x20);
        ui_palette_fade_n((f - 0x3c) * 0xa);
    }
    if (f == 0x14) {
        lbxpal_set_update_range(2, 0x20);
        ui_palette_fade_n(0);
    }
    if ((f > 0xa) && (f < 0x46)) {
        char buf[100];
        lib_strcpy(buf, game_str_wl_won_1, sizeof(buf));
        lib_strcat(buf, p->name, sizeof(buf));
        lbxfont_print_str_normal(5, 10, buf, UI_SCREEN_W, ui_scale);
        lbxfont_print_str_normal(5, 0x19, game_str_wl_won_2, UI_SCREEN_W, ui_scale);
    }
    if ((f > 0x50) && (f < 0x5a)) {
        lbxpal_set_update_range(2, 0x20);
        ui_palette_fade_n(0x64 - (f - 0x50) * 0xa);
    }
    if (f == 0x5a) {
        lbxpal_set_update_range(2, 0x20);
        ui_palette_fade_n(0);
    }
    if ((f > 0x82) && (f < 0x8c)) {
        lbxpal_set_update_range(2, 0x20);
        ui_palette_fade_n((f - 0x82) * 0xa);
    }
    if ((f > 0x50) && (f < 0x8c)) {
        lbxfont_print_str_normal(10, 10, game_str_wl_won_3, UI_SCREEN_W, ui_scale);
    }

    p->frame = ++f;
}

static void ui_play_winlose_cb2(void *vptr)
{
    struct anim_winlose_2_s *p = vptr;
    int f = p->frame;
    if (f == 0) {
        ui_draw_erase_buf();
        lbxgfx_draw_frame_pal(0, 0, p->gfx_winning2, UI_SCREEN_W, ui_scale);
        lbxpal_set_update_range(0, 255);
    } else {
        hw_video_copy_buf();
        lbxgfx_draw_frame_pal(0, 0, p->gfx_winning2, UI_SCREEN_W, ui_scale);
    }
    p->frame = ++f;
}

static void ui_play_winlose_cb3(void *vptr)
{
    struct anim_winlose_3_s *p = vptr;
    int fa = lbxgfx_get_frame(p->gfx_winlast);
    int fb = lbxgfx_get_frame(p->gfx_winface);
    int fc = p->frame;
    int ff = (fc >= 0xa) ? fb : 0;
    int f;
    if ((fa == 0) || (fa == 8)) {
        ui_draw_erase_buf();
    } else {
        hw_video_copy_buf();
    }
    lbxgfx_draw_frame_pal(0, 0, p->gfx_winlast, UI_SCREEN_W, ui_scale);
    lbxgfx_set_frame_0(p->gfx_winface);
    for (f = 0; f <= ff; ++f) {
        lbxgfx_draw_frame(0x82, 0x32, p->gfx_winface, UI_SCREEN_W, ui_scale);
    }
    f = ++fc;
    lbxfont_select(4, 0, 0, 0);
    if ((f > 0xa) && (f < 0x14)) {
        lbxpal_set_update_range(2, 0x20);
        ui_palette_fade_n(0x64 - (f - 0xa) * 0xa);
    }
    if ((f > 0x3c) && (f < 0x46)) {
        lbxpal_set_update_range(2, 0x20);
        ui_palette_fade_n((f - 0x3c) * 0xa);
    }
    if (f == 0x14) {
        lbxpal_set_update_range(2, 0x20);
        ui_palette_fade_n(0);
    }
    if ((f > 0x0a) && (f < 0x46)) {
        lbxfont_print_str_normal(10, 10, p->str1, UI_SCREEN_W, ui_scale);
        if (p->flag_good) {
            lbxfont_print_str_normal(10, 0x19, p->str2, UI_SCREEN_W, ui_scale);
        } else {
            char buf[0x48];
            lib_strcpy(buf, p->str2, sizeof(buf));
            lib_strcat(buf, p->name, sizeof(buf));
            lib_strcat(buf, p->str3, sizeof(buf));
            lbxfont_print_str_normal(10, 0x19, buf, UI_SCREEN_W, ui_scale);
            lbxfont_print_str_normal(10, 0x28, p->str4, UI_SCREEN_W, ui_scale);
        }
    }
    p->frame = f;
}

static void ui_play_ending_good_or_tyrant(int race, const char *name, bool flag_good)
{
    int16_t oi_skip;
    bool flag_skip = false;
    struct anim_winlose_1_s wld1;
    struct anim_winlose_2_s wld2;
    struct anim_winlose_3_s wld3;

    if (ui_draw_finish_mode == 0) {
        ui_palette_fadeout_4_3_1();
    }

    if (!check_lbx_file()) {
        return;
    }

    if (flag_good) {
        wld3.str1 = game_str_wl_3_good_1;
        wld3.str2 = game_str_wl_3_good_2;
        wld3.str3 = 0;
        wld3.str4 = 0;
    } else {
        wld3.str1 = game_str_wl_3_tyrant_1;
        wld3.str2 = game_str_wl_3_tyrant_2;
        wld3.str3 = game_str_wl_3_tyrant_3;
        wld3.str4 = game_str_wl_3_tyrant_4;
    }
    wld3.flag_good = flag_good;
    wld3.name = name;

    lbxpal_select(8, -1, 0);
    wld1.name = name;
    wld1.gfx_planets = lbxfile_item_get(LBXFILE_WINLOSE, 0x13);
    wld1.gfx_stars = lbxfile_item_get(LBXFILE_WINLOSE, 0x14);
    wld3.gfx_winface = lbxfile_item_get(LBXFILE_WINLOSE, 0x15 + race);
    wld1.gfx_ships = lbxfile_item_get(LBXFILE_WINLOSE, 0x20);

    ui_sound_play_music(2);

    uiobj_table_clear();
    oi_skip = uiobj_add_mousearea_all(MOO_KEY_SPACE);
    uiobj_set_downcount(3);
    uiobj_set_callback_and_delay(ui_play_winlose_cb1, &wld1, 3);

    wld1.frame = 0;

    while ((wld1.frame < 150) && (!flag_skip)) {
        int16_t oi;
        ui_delay_prepare();
        oi = uiobj_handle_input_cond();
        if ((oi == oi_skip) || (oi == UIOBJI_ESC)) {
            flag_skip = true;
            break;
        } else {
            if (wld1.frame == 0) {
                ui_draw_erase_buf();
            }
            ui_play_winlose_cb1(&wld1);
            uiobj_finish_frame();
            if (wld1.frame == 1) {
                ui_palette_fadein_60_3_1();
            }
            ui_delay_ticks_or_click(3);
        }
    }

    ui_palette_fadeout_5_5_1();

    lbxfile_item_release(LBXFILE_WINLOSE, wld1.gfx_planets);
    lbxfile_item_release(LBXFILE_WINLOSE, wld1.gfx_ships);
    lbxfile_item_release(LBXFILE_WINLOSE, wld1.gfx_stars);

    if (!flag_skip) {
        wld2.gfx_winning2 = lbxfile_item_get(LBXFILE_WINLOSE, 0x1f);
        wld2.frame = 0;

        uiobj_set_callback_and_delay(ui_play_winlose_cb2, &wld2, 3);

        while ((wld2.frame < 0x14) && (!flag_skip)) {
            int16_t oi;
            ui_delay_prepare();
            oi = uiobj_handle_input_cond();
            if ((oi == oi_skip) || (oi == UIOBJI_ESC)) {
                flag_skip = true;
                break;
            } else {
                ui_play_winlose_cb2(&wld2);
                uiobj_finish_frame();
                if (wld2.frame == 1) {
                    ui_palette_fadein_5f_5_1();
                }
                ui_delay_ticks_or_click(3);
            }
        }
        ui_palette_fadeout_5_5_1();
        lbxfile_item_release(LBXFILE_WINLOSE, wld2.gfx_winning2);
    }

    if (!flag_skip) {
        wld3.gfx_winlast = lbxfile_item_get(LBXFILE_WINLOSE, 0x21);
        wld3.frame = 0;

        uiobj_set_callback_and_delay(ui_play_winlose_cb3, &wld3, 3);
        lbxpal_select(8, -1, 0);
        lbxpal_set_update_range(0, 255);

        while ((wld3.frame < 0x50) && (!flag_skip)) {
            int16_t oi;
            ui_delay_prepare();
            oi = uiobj_handle_input_cond();
            if ((oi == oi_skip) || (oi == UIOBJI_ESC)) {
                flag_skip = true;
                break;
            } else {
                ui_play_winlose_cb3(&wld3);
                uiobj_finish_frame();
                if (wld3.frame == 1) {
                    ui_palette_fadein_4b_19_1();
                }
                ui_delay_ticks_or_click(3);
            }
        }
        ui_palette_fadeout_5_5_1();
        lbxfile_item_release(LBXFILE_WINLOSE, wld3.gfx_winlast);
    }

    lbxfile_item_release(LBXFILE_WINLOSE, wld3.gfx_winface);

    ui_sound_stop_music();

    if (!flag_skip) {
        ui_news_won(flag_good);
        ui_palette_fadeout_4_3_1();
    }
    hw_audio_music_fadeout();
    ui_delay_1e();
    ui_sound_stop_music();
    ui_draw_erase_buf();
    uiobj_finish_frame();
    ui_draw_erase_buf();
}

static void ui_play_winlose_exile_cb(void *vptr)
{
    struct anim_winlose_exile_s *p = vptr;
    int f = p->frame;
    ui_draw_erase_buf();
    lbxgfx_draw_frame_offs(0 - f, 0, p->gfx_stars, 0, 0, UI_VGA_W - 1, UI_VGA_H - 1, UI_SCREEN_W, ui_scale);
    lbxgfx_draw_frame_offs(UI_VGA_W - f, 0, p->gfx_stars, 0, 0, UI_VGA_W - 1, UI_VGA_H - 1, UI_SCREEN_W, ui_scale);
    lbxgfx_draw_frame_offs(UI_VGA_W * 2- f, 0, p->gfx_stars, 0, 0, UI_VGA_W - 1, UI_VGA_H - 1, UI_SCREEN_W, ui_scale);
    lbxgfx_draw_frame_offs(f * 3 - 0xf0, 0, p->gfx_ships, 0, 0, UI_VGA_W - 1, UI_VGA_H - 1, UI_SCREEN_W, ui_scale);

    lbxfont_select(4, 0, 0, 0);

    if ((f > 0x14) && (f < 0x28)) {
        lbxpal_set_update_range(2, 0x20);
        ui_palette_fade_n(0x64 - (f - 0x14) * 5);
    }
    if ((f > 0x78) && (f < 0x8c)) {
        lbxpal_set_update_range(2, 0x20);
        ui_palette_fade_n((f - 0x78) * 5);
    }
    if (f == 0x28) {
        lbxpal_set_update_range(2, 0x20);
        ui_palette_fade_n(0);
    }
    if ((f > 0x14) && (f < 0x8c)) {
        char buf[100];
        lib_strcpy(buf, game_str_wl_exile_1, sizeof(buf));
        lib_strcat(buf, p->name, sizeof(buf));
        lbxfont_print_str_normal(0, 0xa, buf, UI_SCREEN_W, ui_scale);
        lbxfont_print_str_normal(0, 0x19, game_str_wl_exile_2, UI_SCREEN_W, ui_scale);
    }

    lbxfont_select(4, 0, 0, 0);

    if ((f > 0xa0) && (f < 0xb4)) {
        lbxpal_set_update_range(2, 0x20);
        ui_palette_fade_n(0x64 - (f - 0xa0) * 5);
    }
    if ((f > 0x104) && (f < 0x118)) {
        lbxpal_set_update_range(2, 0x20);
        ui_palette_fade_n((f - 0x104) * 5);
    }
    if (f == 0xb4) {
        lbxpal_set_update_range(2, 0x20);
        ui_palette_fade_n(0);
    }

    if ((f > 0xa0) && (f < 0x118)) {
        lbxfont_print_str_normal(0xa, 0xa, game_str_wl_exile_3, UI_SCREEN_W, ui_scale);
        lbxfont_print_str_normal(0xa, 0x19, game_str_wl_exile_4, UI_SCREEN_W, ui_scale);
    }

    p->frame = ++f;
}

static void ui_play_winlose_funeral_cb(void *vptr)
{
    struct anim_winlose_funeral_s *p = vptr;
    int fa, f = p->frame;
    ui_draw_erase_buf();
    fa = lbxgfx_get_frame(p->gfx_lose);
    lbxgfx_set_frame_0(p->gfx_lose);
    for (int i = 0; i <= fa; ++i) {
        lbxgfx_draw_frame(0, 0, p->gfx_lose, UI_SCREEN_W, ui_scale);
    }
    if ((f > 0x14) && (f < 0xa6)) {
        int x, y;
        x = 0xf9 - ((f - 0x14) * 0x17b) / 0x92;
        y = ((f - 0x14) * 0xe3) / 0x92 - 0x4a;
        lbxgfx_draw_frame_offs(x, y, p->gfx_coffin, 0, 0, UI_VGA_W - 1, UI_VGA_H - 1, UI_SCREEN_W, ui_scale);
        lbxgfx_draw_frame_offs(x, y, p->gfx_march, 0, 0, UI_VGA_W - 1, UI_VGA_H - 1, UI_SCREEN_W, ui_scale);
    }
    lbxgfx_draw_frame(0, 0, p->gfx_flag, UI_SCREEN_W, ui_scale);
    p->frame = ++f;
}

/* -------------------------------------------------------------------------- */

void ui_play_ending_good(int race, const char *name)
{
    ui_play_ending_good_or_tyrant(race, name, true);
}

void ui_play_ending_tyrant(int race, const char *name)
{
    ui_play_ending_good_or_tyrant(race, name, false);
}

void ui_play_ending_funeral(int banner_live, int banner_dead)
{
    int16_t oi_skip;
    struct anim_winlose_funeral_s wld;

    ui_draw_finish_mode = 2;

    ui_palette_fadeout_4_3_1();

    if (!check_lbx_file()) {
        return;
    }

    wld.gfx_lose = lbxfile_item_get(LBXFILE_WINLOSE, 0);
    wld.gfx_flag = lbxfile_item_get(LBXFILE_WINLOSE, 1 + banner_live);
    wld.gfx_coffin = lbxfile_item_get(LBXFILE_WINLOSE, 7 + banner_dead);
    wld.gfx_march = lbxfile_item_get(LBXFILE_WINLOSE, 0xd + banner_live);

    ui_sound_play_music(3);

    lbxgfx_apply_palette(wld.gfx_lose); /* FIXME should not be needed */
    ui_cursor_setup_area(1, &ui_cursor_area_all_i0);

    uiobj_table_clear();
    oi_skip = uiobj_add_mousearea_all(MOO_KEY_SPACE);
    uiobj_set_downcount(3);
    uiobj_set_callback_and_delay(ui_play_winlose_funeral_cb, &wld, 4);

    wld.frame = 0;
    while (wld.frame < 0xba) {
        int16_t oi;
        ui_delay_prepare();
        oi = uiobj_handle_input_cond();
        ui_play_winlose_funeral_cb(&wld);
        if ((oi == oi_skip) || (oi == UIOBJI_ESC)) {
            wld.frame = 0x2710;
        }
        ++wld.frame;
        ui_delay_ticks_or_click(4);
        ui_draw_finish();
        ui_palette_set_n();
    }

    hw_audio_music_fadeout();
    ui_palette_fadeout_14_14_2();

    lbxfile_item_release(LBXFILE_WINLOSE, wld.gfx_lose);
    lbxfile_item_release(LBXFILE_WINLOSE, wld.gfx_flag);
    lbxfile_item_release(LBXFILE_WINLOSE, wld.gfx_coffin);
    lbxfile_item_release(LBXFILE_WINLOSE, wld.gfx_march);

    ui_draw_erase_buf();
    ui_draw_finish_mode = 1;
    uiobj_finish_frame();
    ui_draw_erase_buf();
    ui_sound_stop_music();
}

void ui_play_ending_exile(const char *name)
{
    int16_t oi_skip;
    bool flag_skip = false;
    struct anim_winlose_exile_s wld;

    if (ui_draw_finish_mode == 0) {
        ui_palette_fadeout_4_3_1();
    }

    if (!check_lbx_file()) {
        return;
    }

    lbxpal_select(8, -1, 0);
    wld.name = name;
    wld.gfx_ships = lbxfile_item_get(LBXFILE_WINLOSE, 0x22);
    wld.gfx_stars = lbxfile_item_get(LBXFILE_WINLOSE, 0x23);

    ui_sound_play_music(3);

    uiobj_table_clear();
    oi_skip = uiobj_add_mousearea_all(MOO_KEY_SPACE);
    uiobj_set_downcount(3);
    uiobj_set_callback_and_delay(ui_play_winlose_exile_cb, &wld, 3);
    wld.frame = 0;

    while ((wld.frame < 0x17c) && (!flag_skip)) {
        int16_t oi;
        ui_delay_prepare();
        oi = uiobj_handle_input_cond();
        if ((oi == oi_skip) || (oi == UIOBJI_ESC)) {
            flag_skip = true;
            break;
        } else {
            ui_play_winlose_exile_cb(&wld);
            uiobj_finish_frame();
            if (wld.frame == 1) {
                ui_palette_fadein_60_3_1();
            }
            ui_delay_ticks_or_click(2);
        }
    }

    hw_audio_music_fadeout();
    ui_palette_fadeout_14_14_2();

    lbxfile_item_release(LBXFILE_WINLOSE, wld.gfx_ships);
    lbxfile_item_release(LBXFILE_WINLOSE, wld.gfx_stars);

    ui_draw_erase_buf();
    ui_draw_finish_mode = 1;
    uiobj_finish_frame();
    ui_draw_erase_buf();
    ui_sound_stop_music();
}
