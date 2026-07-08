#include "config.h"

#include <string.h>

#include "ui.h"
#include "game.h"
#include "game_ai.h"
#include "game_new.h"
#include "game_str.h"
#include "gfxaux.h"
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
#include "uidelay.h"
#include "uidefs.h"
#include "uidraw.h"
#include "uinewgame.h"
#include "uiobj.h"
#include "uipal.h"
#include "uisound.h"
#include "util.h"

/* -------------------------------------------------------------------------- */

static const uint8_t tbl_cursor_color[] = { 0x91, 0x91, 0x8d, 0x8d, 0x89, 0x89, 0x84, 0x84, 0x7f, 0x7f };

/* -------------------------------------------------------------------------- */

struct new_game_data_s {
    struct game_new_options_s *newopts;
    int frame;
    bool fadein;
    int16_t selected;
    player_id_t pi;
    bool have_human;
    const char *str_title;
    uint8_t *gfx_newgame;
    uint8_t *gfx_optb_ng;
    uint8_t *gfx_optb_cancel;
    uint8_t *gfx_optb_ok;
    uint8_t *gfx_custom;
    uint8_t *gfx_flag[BANNER_NUM];
    uint8_t *gfx_portrait[RACE_NUM];
    char *str_tbl_2space_race[RACE_NUM + 2];
    char *str_tbl_2space_banner[BANNER_NUM + 2];
};

static void new_game_load_data(struct new_game_data_s *d)
{
    d->gfx_newgame = lbxfile_item_get(LBXFILE_VORTEX, 5);
    d->gfx_optb_ng = lbxfile_item_get(LBXFILE_VORTEX, 6);
    d->gfx_optb_cancel = lbxfile_item_get(LBXFILE_VORTEX, 0x1a);
    d->gfx_optb_ok = lbxfile_item_get(LBXFILE_VORTEX, 0x1b);
    d->gfx_custom = lbxfile_item_get(LBXFILE_VORTEX, 9);
    for (int i = 0; i < BANNER_NUM; ++i) {
        const banner_t t[BANNER_NUM] = { /* wrong order in lbx */
            BANNER_GREEN, BANNER_BLUE, BANNER_RED, BANNER_WHITE, BANNER_YELLOW, BANNER_PURPLE
        };
        d->gfx_flag[t[i]] = lbxfile_item_get(LBXFILE_VORTEX, 0xa + i);
    }
    for (int i = 0; i < RACE_NUM; ++i) {
        d->gfx_portrait[i] = lbxfile_item_get(LBXFILE_VORTEX, 0x10 + i);
    }
    for (int i = 0; i < RACE_NUM + 1; ++i) {
        d->str_tbl_2space_race[i] = util_concat("  ", game_str_tbl_race[i], NULL);
    }
    d->str_tbl_2space_race[RACE_NUM + 1] = 0;
    for (int i = 0; i < BANNER_NUM + 1; ++i) {
        d->str_tbl_2space_banner[i] = util_concat("  ", game_str_tbl_banner[i], NULL);
    }
    d->str_tbl_2space_banner[BANNER_NUM + 1] = 0;
}

static void new_game_free_data(struct new_game_data_s *d)
{
    lbxfile_item_release(LBXFILE_VORTEX, d->gfx_newgame);
    lbxfile_item_release(LBXFILE_VORTEX, d->gfx_optb_ng);
    lbxfile_item_release(LBXFILE_VORTEX, d->gfx_optb_cancel);
    lbxfile_item_release(LBXFILE_VORTEX, d->gfx_optb_ok);
    lbxfile_item_release(LBXFILE_VORTEX, d->gfx_custom);
    for (int i = 0; i < BANNER_NUM; ++i) {
        lbxfile_item_release(LBXFILE_VORTEX, d->gfx_flag[i]);
        lib_free(d->str_tbl_2space_banner[i]);
    }
    lib_free(d->str_tbl_2space_banner[BANNER_NUM + 1]);
    for (int i = 0; i < RACE_NUM; ++i) {
        lbxfile_item_release(LBXFILE_VORTEX, d->gfx_portrait[i]);
        lib_free(d->str_tbl_2space_race[i]);
    }
    lib_free(d->str_tbl_2space_race[RACE_NUM + 1]);
}

static void new_game_draw_cb1(void *vptr)
{
    struct new_game_data_s *d = vptr;
    ui_draw_erase_buf();
    lbxgfx_draw_frame(0, 0, d->gfx_newgame, UI_SCREEN_W, ui_scale);
}

static void new_game_draw_race_cb(void *vptr)
{
    struct new_game_data_s *d = vptr;
    hw_video_copy_back_from_page2();
    if (d->selected < RACE_NUM) {
        lbxgfx_draw_frame(91, 11, d->gfx_portrait[d->selected], UI_SCREEN_W, ui_scale);
        lbxfont_select(0, 4, 0, 0);
        for (int i = 0; i < 3; ++i) {
            if (*game_str_tbl_traits[d->selected * 3 + i]) {
                lbxfont_print_str_center(0x6f, 0x35 + i * 8, game_str_tbl_traits[d->selected * 3 + i], UI_SCREEN_W, ui_scale);
            }
        }
    }
}

static int16_t ui_new_game_choose_race(struct game_new_options_s *newopts, struct new_game_data_s *d)
{
    int16_t race;
    d->selected = 0;
    new_game_draw_race_cb(d);
    uiobj_finish_frame();
    if (d->fadein) {
        d->fadein = false;
        ui_palette_fadein_4b_19_1();
    }
    uiobj_set_callback_and_delay(new_game_draw_race_cb, d, 2);
    uiobj_table_clear();
    lbxfont_select(5, 0xf, 0, 0);
    race = uiobj_select_from_list1(0xa, 0xa, 0x32, game_str_ng_choose_race, (char const * const *)d->str_tbl_2space_race, &d->selected, 0, 0xf, 2, true);
    if (race == -1) {
        ui_sound_play_sfx_06();
        return -1;
    }
    ui_sound_play_sfx_24();
    newopts->pdata[d->pi].race = race;
    return race;
}

static void new_game_draw_banner_cb(void *vptr)
{
    struct new_game_data_s *d = vptr;
    race_t race = d->newopts->pdata[d->pi].race;
    hw_video_copy_back_from_page2();
    if (d->str_title) {
        lbxfont_select(5, 0, 0, 0);
        lbxfont_print_str_normal(0xa, 0xa, d->str_title, UI_SCREEN_W, ui_scale);
        lbxgfx_apply_colortable(0xa, 0x14, 0x52, 0x1f, 2, UI_SCREEN_W, ui_scale);
    }
    if (race < RACE_NUM) {
        lbxgfx_draw_frame(91, 11, d->gfx_portrait[race], UI_SCREEN_W, ui_scale);
    }
    ui_draw_filled_rect(0x5a, 0x35, 0x83, 0x5a, 0, ui_scale);
    ui_draw_box1(0x5a, 0x35, 0x83, 0x5a, 0x9b, 0x9b, ui_scale);
    if (d->selected < BANNER_NUM) {
        lbxgfx_set_new_frame(d->gfx_flag[d->selected], d->frame);
        gfx_aux_draw_frame_to(d->gfx_flag[d->selected], &ui_data.aux.screen);
        gfx_aux_draw_frame_from(0x5b, 0x38, &ui_data.aux.screen, UI_SCREEN_W, ui_scale);
    }
    if (++d->frame == 0xa) {
        d->frame = 0;
    }
}

static int16_t ui_new_game_choose_banner(struct game_new_options_s *newopts, struct new_game_data_s *d)
{
    int16_t banner;
    d->selected = 0;
    d->frame = 0;
    d->str_title = 0;
    uiobj_table_clear();
    uiobj_set_callback_and_delay(new_game_draw_banner_cb, d, 2);
    lbxfont_select(5, 0xf, 0, 0);
    banner = uiobj_select_from_list1(0xa, 0xa, 0x32, game_str_ng_choose_banner, (char const * const *)d->str_tbl_2space_banner, &d->selected, 0, 0xf, 2, true);
    if (banner == -1) {
        ui_sound_play_sfx_06();
        return -1;
    }
    ui_sound_play_sfx_24();
    newopts->pdata[d->pi].banner = banner;
    return banner;
}

static bool ui_new_game_pname(struct game_new_options_s *newopts, struct new_game_data_s *d, bool flag_generate)
{
    char buf[32];
    bool flag_ok;

    if (!flag_generate) {
        lib_strcpy(buf, newopts->pdata[d->pi].playername, sizeof(buf));
        flag_generate = (buf[0] == '\0');
    }
    uiobj_set_callback_and_delay(new_game_draw_banner_cb, d, 2);
    d->str_title = game_str_ng_your_name;
    uiobj_table_clear();
    flag_ok = false;
    while (!flag_ok) {
        lbxfont_select(5, 0xf, 0xf, 0xf);
        if (flag_generate) {
            game_new_generate_emperor_name(d->newopts->pdata[d->pi].race, buf, sizeof(buf));
        }
        if (!uiobj_read_str(0xf, 0x16, 0x41, buf, 0xb/*len*/, 0, 0, tbl_cursor_color)) {
            return false;
        }
        util_trim_whitespace(buf, sizeof(buf));
        flag_ok = buf[0] != '\0';
        flag_generate = true;
    }
    lib_strcpy(newopts->pdata[d->pi].playername, buf, EMPEROR_NAME_LEN);
    ui_sound_play_sfx_24();
    return true;
}

static bool ui_new_game_hname(struct game_new_options_s *newopts, struct new_game_data_s *d, bool flag_generate)
{
    char buf[32];
    bool flag_ok;

    if (!flag_generate) {
        lib_strcpy(buf, newopts->pdata[d->pi].homename, sizeof(buf));
        flag_generate = (buf[0] == '\0');
    }
    uiobj_set_callback_and_delay(new_game_draw_banner_cb, d, 2);
    d->str_title = game_str_ng_home_name;
    uiobj_table_clear();
    flag_ok = false;
    while (!flag_ok) {
        lbxfont_select(5, 0xf, 0xf, 0xf);
        if (flag_generate) {
            game_new_generate_home_name(d->newopts->pdata[d->pi].race, buf, sizeof(buf));
        }
        if (!uiobj_read_str(0xf, 0x16, 0x32, buf, PLANET_NAME_LEN, 0, 0, tbl_cursor_color)) {
            return false;
        }
        util_trim_whitespace(buf, sizeof(buf));
        flag_ok = buf[0] != '\0';
        flag_generate = true;
    }
    lib_strcpy(newopts->pdata[d->pi].homename, buf, PLANET_NAME_LEN);
    ui_sound_play_sfx_24();

    lbxfont_select(5, 1, 0xf, 0xf);
    return true;
}

static bool ui_new_game_racebannernames(struct game_new_options_s *newopts, struct new_game_data_s *d)
{
    d->pi = PLAYER_0;
    /* orion.exe inits game->offs02 and game_new:researchflag here for whatever reason */
    uiobj_table_clear();
    ui_palette_fadeout_19_19_1();
    d->fadein = true;
    lbxpal_select(4, -1, 0);
    lbxpal_build_colortables();
    ui_draw_erase_buf();
    lbxgfx_draw_frame(0, 0, d->gfx_custom, UI_SCREEN_W, ui_scale);
    ui_draw_box1(0x5a, 0xa, 0x83, 0x2d, 0x9b, 0x9b, ui_scale);
    hw_video_copy_back_to_page2();
    if (0
      || (ui_new_game_choose_race(newopts, d) < 0)
      || (ui_new_game_choose_banner(newopts, d) < 0)
      || (!ui_new_game_pname(newopts, d, true))
      || (!ui_new_game_hname(newopts, d, true))
    ) {
        return false;
    }
    return true;
}

/* -------------------------------------------------------------------------- */

static void new_game_draw_extra_cb(void *vptr)
{
    struct new_game_data_s *d = vptr;
    struct game_new_options_s *newopts = d->newopts;
    hw_video_copy_back_from_page3();
    lbxfont_select(5, 0, 0, 0);
    for (player_id_t i = 0; i < d->newopts->players; ++i) {
        int x0 = 4 + (i / 3) * 160;
        int y0 = 20 + (i % 3) * 50;
        if (newopts->pdata[i].race < RACE_NUM) {
            lbxgfx_draw_frame(x0 + 1, y0 + 1, d->gfx_portrait[newopts->pdata[i].race], UI_SCREEN_W, ui_scale);
        }
        if (newopts->pdata[i].banner < BANNER_NUM) {
            lbxgfx_set_new_frame(d->gfx_flag[newopts->pdata[i].banner], d->frame);
            gfx_aux_draw_frame_to(d->gfx_flag[newopts->pdata[i].banner], &ui_data.aux.screen);
            gfx_aux_draw_frame_from(x0 + 43 + 1, y0 + 1, &ui_data.aux.screen, UI_SCREEN_W, ui_scale);
        }
        lbxfont_print_str_normal(x0 + 43 + 41 + 2, y0 + 2 , d->newopts->pdata[i].playername, UI_SCREEN_W, ui_scale);
        lbxfont_print_str_normal(x0 + 43 + 41 + 2, y0 + 2 + 11, d->newopts->pdata[i].homename, UI_SCREEN_W, ui_scale);
        lbxfont_print_str_normal(x0 + 43 + 41 + 2, y0 + 2 + 22, d->newopts->pdata[i].is_ai ? game_str_ng_computer : game_str_ng_player, UI_SCREEN_W, ui_scale);
    }
    if (!d->have_human) {
        lbxfont_print_str_center(160, 2, game_str_ng_allai, UI_SCREEN_W, ui_scale);
    }
    if (++d->frame >= 10) {
        d->frame = 0;
    }
}

static bool ui_new_game_extra(struct game_new_options_s *newopts, struct new_game_data_s *d)
{
    bool flag_done = false, flag_ok = false;
    int16_t oi_cancel, oi_ok, oi_race[PLAYER_NUM], oi_banner[PLAYER_NUM], oi_pname[PLAYER_NUM], oi_hname[PLAYER_NUM], oi_ai[PLAYER_NUM];
    d->pi = PLAYER_0;
    d->str_title = 0;
    d->frame = 0;

    d->have_human = false;
    for (int i = 0; i < newopts->players; ++i) {
        if (!newopts->pdata[i].is_ai) {
            d->have_human = true;
            break;
        }
    }
    for (int i = 0; i < newopts->players; ++i) {
        if (newopts->pdata[i].race != RACE_RANDOM) {
            if (!strlen(newopts->pdata[i].playername)) {
                game_new_generate_emperor_name(newopts->pdata[i].race, newopts->pdata[i].playername, EMPEROR_NAME_LEN);
            }
            if (!strlen(newopts->pdata[i].homename)) {
                game_new_generate_home_name(newopts->pdata[i].race, newopts->pdata[i].homename, PLANET_NAME_LEN);
            }
        }
    }
    uiobj_table_clear();
    ui_palette_fadeout_19_19_1();
    d->fadein = true;
    lbxpal_select(4, -1, 0);
    lbxpal_build_colortables();
    ui_draw_erase_buf();
    lbxgfx_draw_frame(0, 0, d->gfx_custom, UI_SCREEN_W, ui_scale);
    hw_video_copy_back_to_page3();
    ui_draw_box1(0x5a, 0xa, 0x83, 0x2d, 0x9b, 0x9b, ui_scale);
    hw_video_copy_back_to_page2();
    hw_video_copy_back_from_page3();
    for (int i = 0; i < newopts->players; ++i) {
        int x0 = 4 + (i / 3) * 160;
        int y0 = 20 + (i % 3) * 50;
        ui_draw_box1(x0, y0, x0 + 41, y0 + 35, 0x9b, 0x9b, ui_scale);
        ui_draw_filled_rect(x0 + 43, y0, x0 + 43 + 41, y0 + 35, 0, ui_scale);
        ui_draw_box1(x0 + 43, y0, x0 + 43 + 41, y0 + 35, 0x9b, 0x9b, ui_scale);
    }
    lbxfont_select(5, 0, 0, 0);
    lbxfont_print_str_center(40, 180, game_str_ng_cancel, UI_SCREEN_W, ui_scale);
    lbxfont_print_str_center(260, 180, game_str_ng_ok, UI_SCREEN_W, ui_scale);

    hw_video_copy_back_to_page3();

#define MAKE_UIOBJS() \
    do { \
        uiobj_table_clear(); \
        oi_cancel = uiobj_add_mousearea(0, 170, 80, 199, MOO_KEY_ESCAPE); \
        oi_ok = uiobj_add_mousearea(220, 170, 300, 199, MOO_KEY_SPACE); \
        for (int i = 0; i < newopts->players; ++i) { \
            int x0 = 4 + (i / 3) * 160; \
            int y0 = 20 + (i % 3) * 50; \
            oi_race[i] = uiobj_add_mousearea(x0, y0, x0 + 41, y0 + 35, MOO_KEY_UNKNOWN); \
            oi_banner[i] = uiobj_add_mousearea(x0 + 43, y0, x0 + 43 + 41, y0 + 35, MOO_KEY_UNKNOWN); \
            oi_pname[i] = uiobj_add_mousearea(x0 + 43 + 41 + 2, y0 + 2, x0 + 43 + 41 + 2 + 60, y0 + 2 + 10, MOO_KEY_UNKNOWN); \
            oi_hname[i] = uiobj_add_mousearea(x0 + 43 + 41 + 2, y0 + 2 + 11, x0 + 43 + 41 + 2 + 60, y0 + 2 + 11 + 10, MOO_KEY_UNKNOWN); \
            oi_ai[i] = uiobj_add_mousearea(x0 + 43 + 41 + 2, y0 + 2 + 22, x0 + 43 + 41 + 2 + 60, y0 + 2 + 22 + 10, MOO_KEY_UNKNOWN); \
        } \
        for (int i = newopts->players; i < PLAYER_NUM; ++i) { \
            oi_race[i] = UIOBJI_INVALID; \
            oi_banner[i] = UIOBJI_INVALID; \
            oi_pname[i] = UIOBJI_INVALID; \
            oi_hname[i] = UIOBJI_INVALID; \
            oi_ai[i] = UIOBJI_INVALID; \
        } \
    } while (0)

    MAKE_UIOBJS();

    uiobj_set_callback_and_delay(new_game_draw_extra_cb, d, 2);
    uiobj_set_xyoff(1, 1);

    while (!flag_done) {
        int16_t oi;
        oi = uiobj_handle_input_cond();
        ui_delay_prepare();
        if ((oi != UIOBJI_ESC) && (oi != UIOBJI_NONE) && (oi != oi_cancel)) {
            ui_sound_play_sfx_24();
        }
        if ((oi == UIOBJI_ESC) || (oi == oi_cancel)) {
            ui_sound_play_sfx_06();
            flag_ok = false;
            flag_done = true;
        } else if (oi == oi_ok) {
            if (d->have_human) {
                flag_ok = true;
                flag_done = true;
            }
        }
        for (int i = 0; i < newopts->players; ++i) {
            if (oi == oi_race[i]) {
                race_t old_race, new_race;
                old_race = newopts->pdata[i].race;
                d->pi = i;
                ui_new_game_choose_race(newopts, d);
                new_race = newopts->pdata[i].race;
                if (new_race != old_race) {
                    if (new_race < RACE_NUM) {
                        game_new_generate_emperor_name(new_race, newopts->pdata[i].playername, EMPEROR_NAME_LEN);
                        game_new_generate_home_name(new_race, newopts->pdata[i].homename, PLANET_NAME_LEN);
                    } else {
                        newopts->pdata[i].playername[0] = '\0';
                        newopts->pdata[i].homename[0] = '\0';
                    }
                }
                uiobj_set_callback_and_delay(new_game_draw_extra_cb, d, 2);
            } else if (oi == oi_banner[i]) {
                d->pi = i;
                d->selected = newopts->pdata[i].banner;
                ui_new_game_choose_banner(newopts, d);
                uiobj_set_callback_and_delay(new_game_draw_extra_cb, d, 2);
            } else if (oi == oi_pname[i]) {
                d->pi = i;
                d->selected = newopts->pdata[i].banner;
                ui_new_game_pname(newopts, d, false);
                uiobj_set_callback_and_delay(new_game_draw_extra_cb, d, 2);
            } else if (oi == oi_hname[i]) {
                d->pi = i;
                d->selected = newopts->pdata[i].banner;
                ui_new_game_hname(newopts, d, false);
                uiobj_set_callback_and_delay(new_game_draw_extra_cb, d, 2);
            } else if (oi == oi_ai[i]) {
                newopts->pdata[i].is_ai = !newopts->pdata[i].is_ai;
                d->have_human = false;
                for (int j = 0; j < newopts->players; ++j) {
                    if (!newopts->pdata[j].is_ai) {
                        d->have_human = true;
                        break;
                    }
                }
            }
        }

        new_game_draw_extra_cb(d);

        MAKE_UIOBJS();

        uiobj_finish_frame();
        if (d->fadein) {
            d->fadein = false;
            ui_palette_fadein_4b_19_1();
        }
        ui_delay_ticks_or_click(2);
    }

#undef MAKE_UIOBJS

    return flag_ok;
}

/* -------------------------------------------------------------------------- */

bool ui_new_game(struct game_new_options_s *newopts)
{
    struct new_game_data_s d;
    bool flag_done = false, flag_fadein = false, flag_ok = false;
    uint16_t oppon = 0, gsize = 0, diffic = 0;
    int16_t oi_gsize, oi_diffic, oi_oppon, oi_cancel, oi_ok;
    int16_t oi_esc, oi_d, oi_g, oi_o, oi_space;

    d.newopts = newopts;

    gsize = newopts->galaxy_size;
    diffic = newopts->difficulty;
    oppon = newopts->players - 1/*0-based*/ - 1/*player*/;

    ui_palette_fadeout_19_19_1();
    lbxpal_select(3, -1, 0);
    lbxpal_build_colortables();

    new_game_load_data(&d);

#define MAKE_UIOBJS() \
    do { \
        lbxfont_select(0, 0, 0, 0); \
        uiobj_table_clear(); \
        oi_esc = uiobj_add_inputkey(MOO_KEY_ESCAPE); \
        oi_gsize = uiobj_add_t0(0xaf, 0x1d, game_str_tbl_gsize[gsize], d.gfx_optb_ng, MOO_KEY_UNKNOWN); \
        oi_diffic = uiobj_add_t0(0xaf, 0x44, game_str_tbl_diffic[diffic], d.gfx_optb_ng, MOO_KEY_UNKNOWN); \
        oi_oppon = uiobj_add_t0(0xaf, 0x6b, game_str_tbl_oppon[oppon], d.gfx_optb_ng, MOO_KEY_UNKNOWN); \
        oi_cancel = uiobj_add_t0(0x5a, 0x93, "", d.gfx_optb_cancel, MOO_KEY_UNKNOWN); \
        oi_ok = uiobj_add_t0(0xa1, 0x93, "", d.gfx_optb_ok, MOO_KEY_UNKNOWN); \
        oi_d = uiobj_add_inputkey(MOO_KEY_d); \
        oi_g = uiobj_add_inputkey(MOO_KEY_g); \
        oi_o = uiobj_add_inputkey(MOO_KEY_o); \
        oi_space = uiobj_add_inputkey(MOO_KEY_SPACE); \
    } while (0)

    MAKE_UIOBJS();

    uiobj_set_callback_and_delay(new_game_draw_cb1, &d, 2);
    uiobj_set_xyoff(1, 1);

    while (!flag_done) {
        int16_t oi;
        oi = uiobj_handle_input_cond();
        ui_delay_prepare();
        if ((oi != UIOBJI_ESC) && (oi != UIOBJI_NONE)) {
            ui_sound_play_sfx_24();
        }
        if ((oi == UIOBJI_ESC) || (oi == oi_cancel)) {
            ui_sound_play_sfx_06();
        }
        if ((oi == oi_diffic) || (oi == oi_d)) {
            if (++diffic >= 5) { diffic = 0; }
        }
        if ((oi == oi_gsize) || (oi == oi_g)) {
            if (++gsize >= 4) { gsize = 0; }
        }
        if ((oi == oi_oppon) || (oi == oi_o)) {
            if (++oppon >= 5) { oppon = 0; }
        }
        if ((oi == UIOBJI_ESC) || (oi == oi_cancel) || (oi == oi_esc)) {
            flag_ok = false;
            flag_done = true;
        }
        if ((oi == oi_ok) || (oi == oi_space)) {
            flag_ok = true;
            flag_done = true;
        }
        new_game_draw_cb1(&d);

        MAKE_UIOBJS();

        uiobj_finish_frame();

        if (!flag_fadein) {
            ui_palette_fadein_4b_19_1();
            flag_fadein = true;
        }
        ui_delay_ticks_or_click(2);
    }

#undef MAKE_UIOBJS

    newopts->galaxy_size = gsize;
    newopts->difficulty = diffic;
    newopts->players = oppon + 1/*0-based*/ + 1/*player*/;

    uiobj_unset_callback();

    if (flag_ok) {
        flag_ok = ui_new_game_racebannernames(newopts, &d);
    }

    uiobj_unset_callback();
    new_game_free_data(&d);

    return flag_ok;
}

bool ui_custom_game(struct game_new_options_s *newopts) {
    struct new_game_data_s d;
    bool flag_ok = false;

    d.newopts = newopts;

    new_game_load_data(&d);

    flag_ok = ui_new_game_extra(newopts, &d);

    new_game_free_data(&d);
    return flag_ok;
}

bool ui_challenge_game(struct game_new_options_s *newopts) {
    static const struct game_new_options_s challenge = GAME_NEW_OPTS_CHALLENGE_118835000;
    struct new_game_data_s d;
    bool flag_ok = false;

    d.newopts = newopts;
    *d.newopts = challenge;

    new_game_load_data(&d);

    flag_ok = ui_new_game_racebannernames(newopts, &d);

    uiobj_unset_callback();
    new_game_free_data(&d);

    if (d.newopts->pdata[0].race == RACE_MRRSHAN) {
        d.newopts->pdata[1].race = RACE_ALKARI;
    }

    if (d.newopts->pdata[0].race == RACE_SAKKRA) {
        d.newopts->pdata[2].race = RACE_MEKLAR;
    }

    if (d.newopts->pdata[0].race == RACE_PSILON) {
        d.newopts->pdata[3].race = RACE_HUMAN;
    }

    if (d.newopts->pdata[0].race == RACE_KLACKON) {
        d.newopts->pdata[4].race = RACE_MEKLAR;
    }

    if (d.newopts->pdata[0].race == RACE_SILICOID) {
        d.newopts->pdata[5].race = RACE_HUMAN;
    }

    return flag_ok;
}
