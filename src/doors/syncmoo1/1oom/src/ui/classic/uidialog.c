#include "config.h"

#include "game_str.h"
#include "lbx.h"
#include "lbxfont.h"
#include "lbxgfx.h"
#include "lib.h"
#include "types.h"
#include "uicursor.h"
#include "uidefs.h"
#include "uidelay.h"
#include "uidialog.h"
#include "uidraw.h"
#include "uiobj.h"
#include "uisound.h"

struct ui_dialog_data_s {
    struct game_s *g;
    player_id_t api;
    int x, y;
    int we;
    uint8_t *gfx_eco_chng2;
    uint8_t *gfx_eco_chng4;
    uint8_t *gfx_robo_but;
    uint8_t dialog_type;
    const char *str;
    int result;
};

static void ui_dialog_load_data(struct ui_dialog_data_s *d)
{
    d->gfx_eco_chng2 = lbxfile_item_get(LBXFILE_BACKGRND, 0x1f);
    d->gfx_eco_chng4 = lbxfile_item_get(LBXFILE_BACKGRND, 0x30);
    d->gfx_robo_but = lbxfile_item_get(LBXFILE_BACKGRND, 0x31);
}

static void ui_dialog_free_data(struct ui_dialog_data_s *d)
{
    lbxfile_item_release(LBXFILE_BACKGRND, d->gfx_eco_chng2);
    lbxfile_item_release(LBXFILE_BACKGRND, d->gfx_eco_chng4);
    lbxfile_item_release(LBXFILE_BACKGRND, d->gfx_robo_but);
}

static void ui_dialog_yesno_draw_cb(void *vptr)
{
    struct ui_dialog_data_s *d = vptr;
    char buf[0x96];
    int x = d->x, y = d->y;
    ui_draw_filled_rect(x, y, x + 135, y + 80, 0xf9, ui_scale);
    lbxgfx_draw_frame(x, y, d->gfx_eco_chng2, UI_SCREEN_W, ui_scale);
    lbxfont_select_set_12_1(0, 0, 0, 0);
    lib_strcpy(buf, game_str_nt_doyou, sizeof(buf));
    lib_strcat(buf, d->str, sizeof(buf));
    lbxfont_print_str_split(x + 15 - (d->we / 2), y + 11, 110 + d->we, buf, 3, UI_SCREEN_W, UI_SCREEN_H, ui_scale);
    lbxgfx_set_frame_0(ui_data.gfx.starmap.scrapbut_yes);
    lbxgfx_set_frame_0(ui_data.gfx.starmap.scrapbut_no);
    lbxgfx_draw_frame(x + 83, y + 60, ui_data.gfx.starmap.scrapbut_yes, UI_SCREEN_W, ui_scale);
    lbxgfx_draw_frame(x + 18, y + 60, ui_data.gfx.starmap.scrapbut_no, UI_SCREEN_W, ui_scale);
}

static void ui_dialog_choose_draw_cb(void *vptr)
{
    struct ui_dialog_data_s *d = vptr;
    char buf[0x96];
    int x = d->x, y = d->y;
    ui_draw_filled_rect(x, y, x + 135, y + 80, 0xf9, ui_scale);
    lbxgfx_draw_frame(x, y, d->gfx_eco_chng4, UI_SCREEN_W, ui_scale);
    lbxfont_select_set_12_1(0, 0, 0, 0);
    lib_strcpy(buf, game_str_nt_doyou, sizeof(buf));
    lib_strcat(buf, d->str, sizeof(buf));
    lbxfont_print_str_split(x + 15 - (d->we / 2), y + 16, 110 + d->we, buf, 3, UI_SCREEN_W, UI_SCREEN_H, ui_scale);
}

static void ui_dialog_do(struct ui_dialog_data_s *d)
{
    int16_t oi_tbl[3], oi_y, oi_n;
    int x = d->x, y = d->y;
    bool flag_done = false;
    ui_cursor_setup_area(2, &ui_cursor_area_tbl[0]);
    uiobj_set_xyoff(0, 0);
    ui_dialog_load_data(d);
    uiobj_set_callback_and_delay((d->dialog_type == 0) ? ui_dialog_yesno_draw_cb : ui_dialog_choose_draw_cb, d, 1);
    uiobj_table_clear();
    oi_y = UIOBJI_INVALID;
    oi_n = UIOBJI_INVALID;
    UIOBJI_SET_TBL_INVALID(oi_tbl);
    if (d->dialog_type == 0) {
        oi_y = uiobj_add_t0(x + 83, y + 60, "", ui_data.gfx.starmap.scrapbut_yes, MOO_KEY_y);
        oi_n = uiobj_add_t0(x + 18, y + 60, "", ui_data.gfx.starmap.scrapbut_no, MOO_KEY_n);
    } else {
        lbxfont_select(2, 6, 0, 0);
        oi_n = uiobj_add_t0(x + 10, y + 60, game_str_tbl_nt_adj[0], d->gfx_robo_but, MOO_KEY_n);
        oi_tbl[0] = uiobj_add_t0(x + 42, y + 60, game_str_tbl_nt_adj[1], d->gfx_robo_but, MOO_KEY_2);
        oi_tbl[1] = uiobj_add_t0(x + 74, y + 60, game_str_tbl_nt_adj[2], d->gfx_robo_but, MOO_KEY_5);
        oi_tbl[2] = uiobj_add_t0(x + 106, y + 60, game_str_tbl_nt_adj[3], d->gfx_robo_but, MOO_KEY_7);

    }
    while (!flag_done) {
        int16_t oi;
        ui_delay_prepare();
        oi = uiobj_handle_input_cond();
        if ((oi == UIOBJI_ESC) || (oi == oi_n)) {
            ui_sound_play_sfx_06();
            d->result = 0;
            flag_done = true;
        }
        if (oi == oi_y) {
            ui_sound_play_sfx_24();
            d->result = 1;
            flag_done = true;
        }
        for (int i = 0; i < 3; ++i) {
            if (oi == oi_tbl[i]) {
                ui_sound_play_sfx_24();
                d->result = i + 1;
                flag_done = true;
            }
        }
        if (!flag_done) {
            if (d->dialog_type == 0) {
                ui_dialog_yesno_draw_cb(d);
            } else {
                ui_dialog_choose_draw_cb(d);
            }
            ui_draw_finish();
        }
        ui_delay_ticks_or_click(1);
    }
    uiobj_table_clear();
    ui_dialog_free_data(d);
}

int ui_dialog_yesno(struct game_s *g, int pi, const char *str, int x, int y, int we)
{
    struct ui_dialog_data_s d;

    memset(&d, 0, sizeof(d));
    d.g = g;
    d.api = pi;
    d.str = str;
    d.dialog_type = 0;
    d.x = x;
    d.y = y;
    d.we = we;

    ui_dialog_do(&d);

    return d.result;
}

int ui_dialog_choose(struct game_s *g, int pi, const char *str, int x, int y, int we)
{
    struct ui_dialog_data_s d;

    memset(&d, 0, sizeof(d));
    d.g = g;
    d.api = pi;
    d.str = str;
    d.dialog_type = 1;
    d.x = x;
    d.y = y;
    d.we = we;

    ui_dialog_do(&d);

    return d.result;
}
