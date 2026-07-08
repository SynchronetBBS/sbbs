#include "config.h"

#include "ui.h"
#include "game_save.h"
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
#include "uiload.h"
#include "uiobj.h"
#include "uipal.h"
#include "uisound.h"

/* -------------------------------------------------------------------------- */

struct load_game_data_s {
    int selected;
    int savenum;
    int tbl_savei[NUM_ALL_SAVES];
    uint8_t *gfx_loadgame;
    uint8_t *gfx_lg_gray;
    uint8_t *gfx_lg_green;
    bool flag_moo13;
};

static void load_lg_data(struct load_game_data_s *d)
{
    d->gfx_loadgame = lbxfile_item_get(LBXFILE_VORTEX, 3);
    d->gfx_lg_gray = lbxfile_item_get(LBXFILE_VORTEX, 7);
    d->gfx_lg_green = lbxfile_item_get(LBXFILE_VORTEX, 8);
}

static void free_lg_data(struct load_game_data_s *d)
{
    lbxfile_item_release(LBXFILE_VORTEX, d->gfx_loadgame);
    lbxfile_item_release(LBXFILE_VORTEX, d->gfx_lg_gray);
    lbxfile_item_release(LBXFILE_VORTEX, d->gfx_lg_green);
}

static void load_game_draw_cb(void *vptr)
{
    struct load_game_data_s *d = vptr;
    hw_video_copy_back_from_page2();
    lbxgfx_draw_frame(0, 0, d->gfx_loadgame, UI_SCREEN_W, ui_scale);
    for (int i = 0; i < d->savenum; ++i) {
        int si, y;
        si = d->tbl_savei[i];
        y = (si < NUM_SAVES) ? (33 + 18 * si) : (110 + 10 * si);
        lbxgfx_draw_frame(134, y, (d->selected == i) ? d->gfx_lg_green : d->gfx_lg_gray, UI_SCREEN_W, ui_scale);
        lbxfont_select(0, (d->selected == i) ? 2 : 1, 0, 0);
        if (d->flag_moo13) {
            char fname[12] = "SAVEX.GAM";
            fname[4] = si + '0' + 1;
            lbxfont_print_str_normal(149, y + 2, fname, UI_SCREEN_W, ui_scale);
        } else {
            lbxfont_print_str_normal(149, y + 2, game_save_tbl_name[si], UI_SCREEN_W, ui_scale);
        }
    }
}

/* -------------------------------------------------------------------------- */

static int ui_load_game_do(bool flag_moo13)
{
    struct load_game_data_s d;
    bool flag_done = false, flag_fadein = false;
    int16_t oi_cancel, oi_ok, oi_save[NUM_ALL_SAVES];

    d.flag_moo13 = flag_moo13;
    d.savenum = 0;
    for (int i = 0; i < (ui_load_opts_extra ? NUM_ALL_SAVES : NUM_SAVES); ++i) {
        if (game_save_tbl_have_save[i]) {
            d.tbl_savei[d.savenum++] = i;
        }
    }
    if (d.flag_moo13) { /* FIXME */
        d.savenum = 7;
        for (int i = 0; i < d.savenum; ++i) {
            d.tbl_savei[i] = i;
        }
    }

    if (d.savenum == 0) {
        return -1;
    }

    load_lg_data(&d);

    ui_palette_fadeout_19_19_1();
    lbxpal_select(2, -1, 0);

    uiobj_table_clear();
    oi_cancel = uiobj_add_mousearea(138, 145, 192, 161, MOO_KEY_ESCAPE);
    oi_ok = uiobj_add_mousearea(202, 145, 256, 161, MOO_KEY_SPACE);
    uiobj_set_focus(oi_ok);

    for (int i = 0; i < d.savenum; ++i) {
        int si, x0, x1, y0, y1;
        si = d.tbl_savei[i];
        y0 = (si < NUM_SAVES) ? (31 + 18 * si) : (110 + 10 * si);
        y1 = y0 + ((si < NUM_SAVES) ? 14 : 8);
        x0 = 130;
        x1 = x0 + ((si < NUM_SAVES) ? 134 : 70);
        oi_save[i] = uiobj_add_mousearea(x0, y0, x1, y1, MOO_KEY_1 + si);
    }

    d.selected = 0;

    ui_draw_erase_buf();
    hw_video_copy_back_to_page2();
    uiobj_set_callback_and_delay(load_game_draw_cb, &d, 2);

    while (!flag_done) {
        int16_t oi;
        oi = uiobj_handle_input_cond();
        ui_delay_prepare();
        for (int i = 0; i < d.savenum; ++i) {
            if (oi == oi_save[i]) {
                d.selected = i;
                ui_sound_play_sfx_24();
            }
        }
        if ((oi == oi_cancel) || (oi == UIOBJI_ESC)) {
            ui_sound_play_sfx_06();
            d.selected = -1;
            flag_done = true;
        }
        if (oi == oi_ok) {
            ui_sound_play_sfx_24();
            flag_done = true;
        }
        load_game_draw_cb(&d);
        uiobj_finish_frame();
        if (!flag_fadein) {
            ui_palette_fadein_4b_19_1();
            flag_fadein = true;
        }
        ui_delay_ticks_or_click(2);
    }

    uiobj_unset_callback();
    free_lg_data(&d);
    return (d.selected >= 0) ? d.tbl_savei[d.selected] : -1;
}

int ui_load_game(void) {
    return ui_load_game_do(false);
}

int ui_load_game_moo13(void) {
    return ui_load_game_do(true);
}
