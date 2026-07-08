#include "config.h"

#include "uisave.h"
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
#include "uiobj.h"
#include "uipal.h"
#include "uisound.h"

/* -------------------------------------------------------------------------- */

struct save_game_data_s {
    int selected;
    int tbl_savei[NUM_SAVES];
    char tbl_savename[NUM_SAVES][SAVE_NAME_LEN + 10];
    uint8_t *gfx_savegame;
    uint8_t *gfx_lg_gray;
    uint8_t *gfx_lg_green;
};

static void load_sg_data(struct save_game_data_s *d)
{
    d->gfx_savegame = lbxfile_item_get(LBXFILE_VORTEX, 4);
    d->gfx_lg_gray = lbxfile_item_get(LBXFILE_VORTEX, 7);
    d->gfx_lg_green = lbxfile_item_get(LBXFILE_VORTEX, 8);
}

static void free_sg_data(struct save_game_data_s *d)
{
    lbxfile_item_release(LBXFILE_VORTEX, d->gfx_savegame);
    lbxfile_item_release(LBXFILE_VORTEX, d->gfx_lg_gray);
    lbxfile_item_release(LBXFILE_VORTEX, d->gfx_lg_green);
}

static void save_game_draw_cb(void *vptr)
{
    struct save_game_data_s *d = vptr;
    const int xoff = 0x76;
    const int yoff = 0xa;
    hw_video_copy_back_from_page2();
    lbxgfx_draw_frame(0, 0, d->gfx_savegame, UI_SCREEN_W, ui_scale);
    for (int i = 0; i < NUM_SAVES; ++i) {
        lbxgfx_draw_frame(16 + xoff, 23 + yoff + 18 * i, (d->selected == i) ? d->gfx_lg_green : d->gfx_lg_gray, UI_SCREEN_W, ui_scale);
    }
}

/* -------------------------------------------------------------------------- */

int ui_save_game(struct game_s *g)
{
    struct save_game_data_s d;
    bool flag_done = false;
    const int xoff = 118;
    const int yoff = 10;
    int16_t oi_esc, oi_cancel, oi_ok, oi_save[NUM_SAVES];
    const uint8_t ctbl[] = { 5, 6, 7, 8, 9, 10, 0, 0, 0, 0 };
    load_sg_data(&d);

    for (int i = 0; i < NUM_SAVES; ++i) {
        lib_strcpy(d.tbl_savename[i], game_save_tbl_name[i], SAVE_NAME_LEN + 10);
    }

    ui_draw_erase_buf();
    hw_video_copy_back_to_page2();
    uiobj_set_callback_and_delay(save_game_draw_cb, &d, 2);
    uiobj_table_clear();

    for (int i = 0; i < NUM_SAVES; ++i) {
        oi_save[i] = UIOBJI_INVALID;
    }

    d.selected = -1;
    oi_ok = UIOBJI_INVALID;
    oi_cancel = UIOBJI_INVALID;
    oi_esc = UIOBJI_INVALID;

    while (!flag_done) {
        int16_t oi;
        oi = uiobj_handle_input_cond();
        ui_delay_prepare();
        for (int i = 0; i < NUM_SAVES; ++i) {
            if (oi == oi_save[i]) {
                d.selected = i;
                ui_sound_play_sfx_24();
            }
        }
        if ((oi == oi_cancel) || (oi == oi_esc)) {
            ui_sound_play_sfx_06();
            d.selected = -1;
            flag_done = true;
        }
        if ((oi == oi_ok) && (d.selected != -1)) {
            ui_sound_play_sfx_24();
            game_save_do_save_i(d.selected, d.tbl_savename[d.selected], g);
            flag_done = true;
        }
        uiobj_table_clear();
        for (int i = 0; i < NUM_SAVES; ++i) {
            lbxfont_select(0, (i == d.selected) ? 2 : 1, 0, 0);
            oi_save[i] = uiobj_add_textinput(149, 35 + i * 18, 106, &(d.tbl_savename[i][0]), SAVE_NAME_LEN - 1, 1, false, true, ctbl, MOO_KEY_UNKNOWN);
            uiobj_dec_y1(oi_save[i]);
        }
        oi_esc = uiobj_add_inputkey(MOO_KEY_ESCAPE);
        oi_cancel = uiobj_add_mousearea(20 + xoff, 135 + yoff, 74 + xoff, 152 + yoff, MOO_KEY_ESCAPE);
        oi_ok = uiobj_add_mousearea(84 + xoff, 135 + yoff, 138 + xoff, 152 + yoff, MOO_KEY_SPACE);
        if (!flag_done) {
            save_game_draw_cb(&d);
            ui_draw_finish();
            ui_delay_ticks_or_click(2);
        }
    }

    uiobj_unset_callback();
    free_sg_data(&d);
    return d.selected;
}
