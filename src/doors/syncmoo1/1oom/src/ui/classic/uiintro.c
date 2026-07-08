#include "config.h"

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
#include "uiobj.h"
#include "uipal.h"
#include "uisound.h"

/* -------------------------------------------------------------------------- */

#define SFX_ID_1    (NUM_SOUNDS + 0)
#define SFX_ID_3    (NUM_SOUNDS + 1)
#define SFX_ID_5    (NUM_SOUNDS + 2)

/* -------------------------------------------------------------------------- */

static bool check_intro_files(void)
{
    const lbxfile_e tbl[3] = { LBXFILE_INTRO, LBXFILE_INTRO2, LBXFILE_INTROSND };
    for (int i = 0; i < 3; ++i) {
        if (!lbxfile_exists(tbl[i])) {
            log_warning("skipping intro due to missing %s\n", lbxfile_name(tbl[i]));
            return false;
        }
    }
    return true;
}

/* -------------------------------------------------------------------------- */

void ui_play_intro(void)
{
    int16_t oi_skip;
    uint16_t frame = 0;
    bool flag_skip, flag_fadein = false;
    uint8_t *intro_sfx1;
    uint8_t *intro_sfx3;
    uint8_t *intro_sfx5;
    uint8_t *intro_gfx = 0, *old_gfx = 0;

    if (!check_intro_files()) {
        return;
    }

    ui_palette_fadeout_14_14_2();
    lbxpal_select(0, -1, 0);
    ui_draw_erase_buf();
    lbxfont_select(5, 1, 0, 0);
    lbxfont_print_str_normal(0, 0, game_str_in_loading, UI_SCREEN_W, ui_scale);
    ui_cursor_setup_area(1, &ui_cursor_area_all_i0);
    uiobj_table_clear();
    uiobj_finish_frame();
    ui_palette_fadein_60_3_1();

    ui_palette_fadeout_4_3_1();

    /* uisound.c is not used as these are only used once */
    {
        uint32_t len;
        hw_audio_sfx_batch_start(SFX_ID_5);
        intro_sfx5 = lbxfile_item_get_with_len(LBXFILE_INTROSND, 5, &len);
        hw_audio_sfx_init(SFX_ID_5, intro_sfx5, len);
        intro_sfx3 = lbxfile_item_get_with_len(LBXFILE_INTROSND, 3, &len);
        hw_audio_sfx_init(SFX_ID_3, intro_sfx3, len);
        intro_sfx1 = lbxfile_item_get_with_len(LBXFILE_INTROSND, 1, &len);
        hw_audio_sfx_init(SFX_ID_1, intro_sfx1, len);
        hw_audio_sfx_batch_end();
    }

    ui_sound_play_music(0);
    ui_draw_erase_buf();
    uiobj_finish_frame();
    ui_draw_erase_buf();

    lbxpal_select(1, -1, 0);
    uiobj_table_clear();
    uiobj_set_skip_delay(true);

    oi_skip = uiobj_add_mousearea_all(MOO_KEY_UNKNOWN);
    flag_skip = 0;
    uiobj_set_downcount(2);

    if (!flag_skip) {
        intro_gfx = lbxfile_item_get(LBXFILE_INTRO, 0);
        old_gfx = intro_gfx;
        ui_palette_fadeout_14_14_2();
    }

    while ((frame < 0x73) && (!flag_skip)) {
        int16_t oi;
        ui_delay_prepare();
        oi = uiobj_handle_input_cond();
        if ((oi == oi_skip) || (oi == UIOBJI_ESC)) {
            flag_skip = true;
            break;
        } else {
            if (frame == 0) {
                ui_draw_erase_buf();
            } else {
                lbxgfx_set_frame(intro_gfx, frame - 1);
                lbxgfx_draw_frame_pal(0, 0, intro_gfx, UI_SCREEN_W, ui_scale);
            }
            lbxgfx_draw_frame_pal(0, 0, intro_gfx, UI_SCREEN_W, ui_scale);
            if (!flag_fadein) {
                uiobj_finish_frame();
                ui_palette_fadein_50_14_2();
                flag_fadein = true;
            } else {
                ui_palette_set_n();
                uiobj_finish_frame();
            }
            ui_delay_ticks_or_click(3);
            ++frame;
        }
    }

    if (!flag_skip) {
        intro_gfx = lbxfile_item_get(LBXFILE_INTRO, 1);
        lbxfile_item_release(LBXFILE_INTRO, old_gfx);
        old_gfx = intro_gfx;
        ui_palette_fadeout_14_14_2();
        frame = 0;
        flag_fadein = false;
    }

    while ((frame < 0x1e) && (!flag_skip)) {
        int16_t oi;
        ui_delay_prepare();
        oi = uiobj_handle_input_cond();
        if ((oi == oi_skip) || (oi == UIOBJI_ESC)) {
            flag_skip = true;
            break;
        } else {
            if (frame == 0) {
                ui_draw_erase_buf();
            } else {
                if (frame == 1) {
                    ui_draw_erase_buf();
                }
                if (1/*sound_enabled && sfx_enabled*/) {
                    if (frame == 9) {
                        hw_audio_sfx_play(SFX_ID_5);
                    } else if (frame == 0xe) {
                        hw_audio_sfx_play(SFX_ID_5); /*seg2*/
                    } else if (frame == 0x13) {
                        hw_audio_sfx_play(SFX_ID_5); /*seg3*/
                    }
                }
                lbxgfx_set_frame(intro_gfx, frame - 1);
                lbxgfx_draw_frame_pal(0, 0, intro_gfx, UI_SCREEN_W, ui_scale);
            }
            lbxgfx_draw_frame_pal(0, 0, intro_gfx, UI_SCREEN_W, ui_scale);
            if (!flag_fadein) {
                uiobj_finish_frame();
                ui_palette_fadein_60_3_1();
                flag_fadein = true;
            } else {
                ui_palette_set_n();
                uiobj_finish_frame();
            }
            ++frame;
            ui_delay_ticks_or_click(((frame > 0xe) && (frame < 0x1a)) ? 1 : 3);
        }
    }

    if (!flag_skip) {
        intro_gfx = lbxfile_item_get(LBXFILE_INTRO, 2);
        lbxfile_item_release(LBXFILE_INTRO, old_gfx);
        old_gfx = intro_gfx;
        ui_palette_fadeout_14_14_2();
        frame = 0;
        flag_fadein = false;
    }

    while ((frame < 0x46) && (!flag_skip)) {
        int16_t oi;
        ui_delay_prepare();
        oi = uiobj_handle_input_cond();
        if ((oi == oi_skip) || (oi == UIOBJI_ESC)) {
            flag_skip = true;
            break;
        } else {
            if (frame == 0) {
                ui_draw_erase_buf();
            } else {
                if (frame == 1) {
                    ui_draw_erase_buf();
                }
                if (1/*sound_enabled && sfx_enabled*/) {
                    switch (frame) {
                        case 0x32:
                        case 0x37:
                        case 0x3c:
                        case 0x41:
                            hw_audio_sfx_play(SFX_ID_3);
                            break;
                        default:
                            break;
                    }
                }
                lbxgfx_set_frame(intro_gfx, frame - 1);
                lbxgfx_draw_frame_pal(0, 0, intro_gfx, UI_SCREEN_W, ui_scale);
            }
            lbxgfx_draw_frame_pal(0, 0, intro_gfx, UI_SCREEN_W, ui_scale);
            if (!flag_fadein) {
                uiobj_finish_frame();
                ui_palette_fadein_50_14_2();
                flag_fadein = true;
            } else {
                ui_palette_set_n();
                uiobj_finish_frame();
            }
            ++frame;
            ui_delay_ticks_or_click((frame < 0x32) ? 3 : 1);
        }
    }

    if (!flag_skip) {
        intro_gfx = lbxfile_item_get(LBXFILE_INTRO, 3);
        lbxfile_item_release(LBXFILE_INTRO, old_gfx);
        old_gfx = intro_gfx;
        ui_palette_fadeout_14_14_2();
        frame = 0;
        flag_fadein = false;
    }

    while ((frame < 0x45) && (!flag_skip)) {
        int16_t oi;
        ui_delay_prepare();
        oi = uiobj_handle_input_cond();
        if ((oi == oi_skip) || (oi == UIOBJI_ESC)) {
            flag_skip = true;
            break;
        } else {
            if (frame == 0) {
                ui_draw_erase_buf();
            } else {
                if (frame == 1) {
                    ui_draw_erase_buf();
                }
                if (1/*sound_enabled && sfx_enabled*/) {
                    if ((frame == 0x14) || (frame == 0x28)) {
                        hw_audio_sfx_play(SFX_ID_1);
                    }
                }
                lbxgfx_set_frame(intro_gfx, frame - 1);
                lbxgfx_draw_frame_pal(0, 0, intro_gfx, UI_SCREEN_W, ui_scale);
            }
            lbxgfx_draw_frame_pal(0, 0, intro_gfx, UI_SCREEN_W, ui_scale);
            if (!flag_fadein) {
                uiobj_finish_frame();
                ui_palette_fadein_50_14_2();
                flag_fadein = true;
            } else {
                ui_palette_set_n();
                uiobj_finish_frame();
            }
            ++frame;
            ui_delay_ticks_or_click(3);
        }
    }

    if (old_gfx) {
        lbxfile_item_release(LBXFILE_INTRO, old_gfx);
        old_gfx = 0;
    }

    if (!flag_skip) {
        intro_gfx = lbxfile_item_get(LBXFILE_INTRO2, 0);
        old_gfx = intro_gfx;
        ui_palette_fadeout_14_14_2();
        frame = 0;
        flag_fadein = false;
    }

    while ((frame < 0x28) && (!flag_skip)) {
        int16_t oi;
        ui_delay_prepare();
        oi = uiobj_handle_input_cond();
        if ((oi == oi_skip) || (oi == UIOBJI_ESC)) {
            flag_skip = true;
            break;
        } else {
            if (frame == 0) {
                ui_draw_erase_buf();
            } else {
                if (frame == 1) {
                    ui_draw_erase_buf();
                }
                if (1/*sound_enabled && sfx_enabled*/) {
                    if (frame == 0x14) {
                        hw_audio_sfx_play(SFX_ID_1);
                    }
                }
                lbxgfx_set_frame(intro_gfx, frame - 1);
                lbxgfx_draw_frame_pal(0, 0, intro_gfx, UI_SCREEN_W, ui_scale);
            }
            lbxgfx_draw_frame_pal(0, 0, intro_gfx, UI_SCREEN_W, ui_scale);
            if (!flag_fadein) {
                uiobj_finish_frame();
                ui_palette_fadein_50_14_2();
                flag_fadein = true;
            } else {
                ui_palette_set_n();
                uiobj_finish_frame();
            }
            ++frame;
            ui_delay_ticks_or_click(3);
        }
    }

    if (!flag_skip) {
        intro_gfx = lbxfile_item_get(LBXFILE_INTRO2, 1);
        lbxfile_item_release(LBXFILE_INTRO2, old_gfx);
        old_gfx = intro_gfx;
        ui_palette_fadeout_14_14_2();
        frame = 0;
        flag_fadein = false;
    }

    while ((frame < 0x1e) && (!flag_skip)) {
        int16_t oi;
        ui_delay_prepare();
        oi = uiobj_handle_input_cond();
        if ((oi == oi_skip) || (oi == UIOBJI_ESC)) {
            flag_skip = true;
            break;
        } else {
            if (frame == 0) {
                ui_draw_erase_buf();
            } else {
                if (frame == 1) {
                    ui_draw_erase_buf();
                }
                lbxgfx_set_frame(intro_gfx, frame - 1);
                lbxgfx_draw_frame_pal(0, 0, intro_gfx, UI_SCREEN_W, ui_scale);
            }
            lbxgfx_draw_frame_pal(0, 0, intro_gfx, UI_SCREEN_W, ui_scale);
            if (!flag_fadein) {
                uiobj_finish_frame();
                ui_palette_fadein_50_14_2();
                flag_fadein = true;
            } else {
                ui_palette_set_n();
                uiobj_finish_frame();
            }
            ++frame;
            ui_delay_ticks_or_click(3);
        }
    }

    if (!flag_skip) {
        intro_gfx = lbxfile_item_get(LBXFILE_INTRO2, 2);
        lbxfile_item_release(LBXFILE_INTRO2, old_gfx);
        old_gfx = intro_gfx;
        ui_palette_fadeout_14_14_2();
        frame = 0;
        flag_fadein = false;
    }

    while ((frame < 0xb4) && (!flag_skip)) {
        int16_t oi;
        ui_delay_prepare();
        oi = uiobj_handle_input_cond();
        if ((oi == oi_skip) || (oi == UIOBJI_ESC)) {
            flag_skip = true;
            break;
        } else {
            if (frame == 0) {
                ui_draw_erase_buf();
            } else {
                if (frame == 1) {
                    ui_draw_erase_buf();
                }
                if (frame == 0xb2) {
                    hw_audio_music_fadeout();
                }
                lbxgfx_set_frame(intro_gfx, frame - 1);
                lbxgfx_draw_frame_pal(0, 0, intro_gfx, UI_SCREEN_W, ui_scale);
            }
            lbxgfx_draw_frame_pal(0, 0, intro_gfx, UI_SCREEN_W, ui_scale);
            if (!flag_fadein) {
                uiobj_finish_frame();
                ui_palette_fadein_50_14_2();
                flag_fadein = true;
            } else {
                ui_palette_set_n();
                uiobj_finish_frame();
            }
            ++frame;
            ui_delay_ticks_or_click(4);
        }
    }

    if (old_gfx) {
        lbxfile_item_release(LBXFILE_INTRO2, old_gfx);
        old_gfx = 0;
    }

    ui_palette_fadeout_14_14_2();
    ui_draw_erase_buf();
    uiobj_finish_frame();
    ui_draw_erase_buf();
    ui_palette_fadeout_14_14_2();

    uiobj_set_skip_delay(false);

    ui_sound_stop_music();
    hw_audio_sfx_release(SFX_ID_1);
    hw_audio_sfx_release(SFX_ID_3);
    hw_audio_sfx_release(SFX_ID_5);
    lbxfile_item_release(LBXFILE_INTROSND, intro_sfx1);
    lbxfile_item_release(LBXFILE_INTROSND, intro_sfx3);
    lbxfile_item_release(LBXFILE_INTROSND, intro_sfx5);
}
