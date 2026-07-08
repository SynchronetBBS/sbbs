#include "config.h"

#include "uigameopts.h"
#include "hw.h"
#include "kbd.h"
#include "lbx.h"
#include "lbxfont.h"
#include "lbxgfx.h"
#include "lbxpal.h"
#include "lib.h"
#include "log.h"
#include "options.h"
#include "types.h"
#include "uicursor.h"
#include "uidelay.h"
#include "uidefs.h"
#include "uidraw.h"
#include "uiload.h"
#include "uiobj.h"
#include "uipal.h"
#include "uisave.h"
#include "uisound.h"

/* -------------------------------------------------------------------------- */

struct gameopts_data_s {
    int selected;
    uint8_t *gfx_game;
    uint8_t *gfx_save;
    uint8_t *gfx_load;
    uint8_t *gfx_quit;
    uint8_t *gfx_silent;
    uint8_t *gfx_fx;
    uint8_t *gfx_music;
};

static void load_go_data(struct gameopts_data_s *d)
{
    d->gfx_game = lbxfile_item_get(LBXFILE_VORTEX, 0x1c);
    d->gfx_save = lbxfile_item_get(LBXFILE_VORTEX, 0x1d);
    d->gfx_load = lbxfile_item_get(LBXFILE_VORTEX, 0x1e);
    d->gfx_quit = lbxfile_item_get(LBXFILE_VORTEX, 0x1f);
    d->gfx_silent = lbxfile_item_get(LBXFILE_VORTEX, 0x20);
    d->gfx_fx = lbxfile_item_get(LBXFILE_VORTEX, 0x21);
    d->gfx_music = lbxfile_item_get(LBXFILE_VORTEX, 0x22);
}

static void free_go_data(struct gameopts_data_s *d)
{
    lbxfile_item_release(LBXFILE_VORTEX, d->gfx_game);
    lbxfile_item_release(LBXFILE_VORTEX, d->gfx_save);
    lbxfile_item_release(LBXFILE_VORTEX, d->gfx_load);
    lbxfile_item_release(LBXFILE_VORTEX, d->gfx_quit);
    lbxfile_item_release(LBXFILE_VORTEX, d->gfx_silent);
    lbxfile_item_release(LBXFILE_VORTEX, d->gfx_fx);
    lbxfile_item_release(LBXFILE_VORTEX, d->gfx_music);
}

static void gameopts_draw_cb(void *vptr)
{
    struct gameopts_data_s *d = vptr;
    ui_draw_erase_buf();
    lbxgfx_draw_frame(0, 0, d->gfx_game, UI_SCREEN_W, ui_scale);
}

/* -------------------------------------------------------------------------- */

gameopts_act_t ui_gameopts(struct game_s *g, int *load_game_i_ptr)
{
    struct gameopts_data_s d;
    bool flag_done = false;
    gameopts_act_t ret = GAMEOPTS_DONE;
    int16_t oi_quit, oi_done, oi_load, oi_save, oi_silent, oi_fx, oi_music;
    int16_t fxmusic = opt_music_enabled ? 2 : (opt_sfx_enabled ? 1 : 0);

    load_go_data(&d);

    ui_palette_fadeout_19_19_1();
    lbxpal_select(2, -1, 0);
    ui_draw_finish_mode = 2;

    uiobj_table_clear();
    oi_load = uiobj_add_t0(115, 81, "", d.gfx_load, MOO_KEY_l);
    oi_save = uiobj_add_t0(115, 56, "", d.gfx_save, MOO_KEY_s);
    oi_quit = uiobj_add_t0(115, 106, "", d.gfx_quit, MOO_KEY_q);
    oi_silent = uiobj_add_t3(210, 56, "", d.gfx_silent, &fxmusic, 0, MOO_KEY_i);
    oi_fx = uiobj_add_t3(210, 81, "", d.gfx_fx, &fxmusic, 1, MOO_KEY_f);
    oi_music = uiobj_add_t3(210, 106, "", d.gfx_music, &fxmusic, 2, MOO_KEY_m);
    oi_done = uiobj_add_mousearea(173, 134, 226, 150, MOO_KEY_SPACE);
    uiobj_set_downcount(1);
    uiobj_set_callback_and_delay(gameopts_draw_cb, &d, 2);

    while (!flag_done) {
        int16_t oi;
        oi = uiobj_handle_input_cond();
        ui_delay_prepare();
        if ((oi == UIOBJI_ESC) || (oi == oi_done)) {
            ui_sound_play_sfx_24();
            flag_done = true;
        } else if ((oi == oi_silent) || (oi == oi_fx) || (oi == oi_music)) {
            opt_music_enabled = (fxmusic == 2);
            opt_sfx_enabled = (fxmusic >= 1);
            ui_sound_play_sfx_24();
        } else if (oi == oi_load) {
            int loadi;
            ui_sound_play_sfx_24();
            loadi = ui_load_game();
            if (loadi >= 0) {
                *load_game_i_ptr = loadi;
                ret = GAMEOPTS_LOAD;
            }
            flag_done = true;
        } else if (oi == oi_save) {
            ui_sound_play_sfx_24();
            ui_save_game(g);
            flag_done = true;
        } else if (oi == oi_quit) {
            ret = GAMEOPTS_QUIT;
            ui_sound_play_sfx_24();
            flag_done = true;
        }
        if (!flag_done) {
            gameopts_draw_cb(&d);
            ui_draw_finish();
            ui_delay_ticks_or_click(2);
        }
    }

    if (ret != GAMEOPTS_QUIT) {
        ui_palette_fadeout_a_f_1();
        lbxpal_select(0, -1, 0);
    }
    ui_draw_finish_mode = 2;

    uiobj_unset_callback();
    free_go_data(&d);
    return ret;
}
