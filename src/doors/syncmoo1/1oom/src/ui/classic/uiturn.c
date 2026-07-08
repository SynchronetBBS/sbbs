#include "config.h"

#include <ctype.h>
#include <stdio.h>

#include "ui.h"
#include "comp.h"
#include "game.h"
#include "game_aux.h"
#include "game_str.h"
#include "hw.h"
#include "kbd.h"
#include "lbx.h"
#include "lbxfont.h"
#include "lbxgfx.h"
#include "lib.h"
#include "log.h"
#include "types.h"
#include "uidelay.h"
#include "uidefs.h"
#include "uidraw.h"
#include "uigmap.h"
#include "uiobj.h"
#include "uisound.h"
#include "uistarmap_common.h"
#include "uiswitch.h"

/* -------------------------------------------------------------------------- */

struct turnmsg_data_s {
    struct game_s *g;
    player_id_t api;
    const char *str;
    struct starmap_data_s sm;
};

static void ui_turn_msg_draw_cb(void *vptr)
{
    struct turnmsg_data_s *d = vptr;
    ui_starmap_draw_basic(&d->sm);
    ui_draw_textbox_2str("", d->str, 74, ui_scale);
}

/* -------------------------------------------------------------------------- */

void ui_turn_pre(const struct game_s *g)
{
    struct starmap_data_s d;
    ui_starmap_draw_button_text(&d, false);
    hw_video_copy_back_to_page2();
}

void ui_turn_msg(struct game_s *g, int pi, const char *str)
{
    struct turnmsg_data_s d;
    bool flag_done = false;
    int tempnum;
    ui_switch_1(g, pi);
    tempnum = g->evn.build_finished_num[pi];
    g->evn.build_finished_num[pi] = 0;
    d.g = g;
    d.api = pi;
    d.str = str;
    ui_starmap_common_init(g, &d.sm, pi);
    d.sm.bottom_highlight = -1;
    uiobj_set_callback_and_delay(ui_turn_msg_draw_cb, &d, 3);
    uiobj_table_clear();
    uiobj_add_mousearea_all(MOO_KEY_SPACE);
    while (!flag_done) {
        int16_t oi;
        ui_delay_prepare();
        oi = uiobj_handle_input_cond();
        if (oi != 0) {
            flag_done = true;
        }
        ui_turn_msg_draw_cb(&d);
        ui_draw_finish();
        ui_delay_ticks_or_click(3);
    }
    uiobj_unset_callback();
    uiobj_table_clear();
    ui_sound_play_sfx_24();
    ui_delay_1();
    g->evn.build_finished_num[pi] = tempnum;
}
