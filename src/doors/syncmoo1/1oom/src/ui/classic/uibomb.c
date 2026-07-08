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

struct bomb_data_s {
    struct game_s *g;
    player_id_t api;
    player_id_t owner;
    uint8_t planet;
    bool hide_other;
    int pop_inbound;
    int popdmg;
    int factdmg;
    uint8_t *gfx_bombback;
    uint8_t *gfx_bombbutt;
    uint8_t *gfx_bombbutc;
    uint8_t *gfx_explobac;
    uint8_t *gfx_contbutt;
    uint8_t *gfx_bombard;
    void *gmap;
};

static void bomb_load_data(struct bomb_data_s *d)
{
    d->gfx_bombback = lbxfile_item_get(LBXFILE_BACKGRND, 0x11);
    d->gfx_bombbutt = lbxfile_item_get(LBXFILE_BACKGRND, 0x13);
    d->gfx_bombbutc = lbxfile_item_get(LBXFILE_BACKGRND, 0x12);
    d->gfx_explobac = lbxfile_item_get(LBXFILE_BACKGRND, 0xf);
    d->gfx_contbutt = lbxfile_item_get(LBXFILE_BACKGRND, 0xc);
    d->gfx_bombard = lbxfile_item_get(LBXFILE_BACKGRND, 0x10);
}

static void bomb_free_data(struct bomb_data_s *d)
{
    lbxfile_item_release(LBXFILE_BACKGRND, d->gfx_bombback);
    lbxfile_item_release(LBXFILE_BACKGRND, d->gfx_bombbutt);
    lbxfile_item_release(LBXFILE_BACKGRND, d->gfx_bombbutc);
    lbxfile_item_release(LBXFILE_BACKGRND, d->gfx_explobac);
    lbxfile_item_release(LBXFILE_BACKGRND, d->gfx_contbutt);
    lbxfile_item_release(LBXFILE_BACKGRND, d->gfx_bombard);
}

static void bomb_draw_anim(uint8_t *gfx, bool anim)
{
    int frame = anim ? lbxgfx_get_frame(gfx) : 0;
    lbxgfx_set_frame_0(gfx);
    for (int f = 0; f <= frame; ++f) {
        lbxgfx_draw_frame(227, 58, gfx, UI_SCREEN_W, ui_scale);
    }
    switch (frame) {
        case 3:
            ui_sound_play_sfx(0x25);
            break;
        case 6:
            ui_sound_play_sfx(0x2);
            break;
        case 0xf:
            ui_sound_play_sfx(0x11);
            break;
        default:
            break;
    }
}

static void bomb_ask_draw_cb(void *vptr)
{
    struct bomb_data_s *d = vptr;
    const struct game_s *g = d->g;
    const planet_t *p = &(g->planet[d->planet]);
    const empiretechorbit_t *e = &(g->eto[p->owner]);
    char buf[0x80];
    hw_video_copy_back_from_page2();
    ui_draw_filled_rect(222, 4, 314, 179, 0, ui_scale);
    lbxgfx_draw_frame(222, 4, d->gfx_bombback, UI_SCREEN_W, ui_scale);
    ui_starmap_draw_planetinfo_2(g, d->api, d->api, d->planet);
    bomb_draw_anim(d->gfx_bombard, false);
    lbxfont_select_set_12_4(5, tbl_banner_fontparam[e->banner], 0, 0);
    lib_sprintf(buf, sizeof(buf), "%s %s", game_str_tbl_race[e->race], game_str_sm_colony);
    lbxfont_print_str_center(270, 84, buf, UI_SCREEN_W, ui_scale);
    lbxfont_select_set_12_4(3, 0xa, 0, 0);
    lbxfont_print_str_center(268, 60, game_str_sm_bomb1, UI_SCREEN_W, ui_scale);
    lbxfont_print_str_center(268, 69, game_str_sm_bomb2, UI_SCREEN_W, ui_scale);
    /*game_update_visibility(g);*/
    ui_gmap_basic_draw_frame(d->gmap, d->api);
    ui_gmap_draw_planet_border(g, d->planet);
    lbxfont_select_set_12_4(0, 0xa, 0, 0);
    lbxfont_print_str_normal(230, 115, game_str_sb_pop, UI_SCREEN_W, ui_scale);
    lbxfont_print_num_right(305, 115, p->pop, UI_SCREEN_W, ui_scale);
    lbxfont_print_str_normal(230, 125, game_str_sb_fact, UI_SCREEN_W, ui_scale);
    lbxfont_print_num_right(305, 125, p->factories, UI_SCREEN_W, ui_scale);
    if (d->pop_inbound) {
        lbxfont_select_set_12_4(0, 0x0, 0, 0);
        lib_sprintf(buf, sizeof(buf), "%i %s", d->pop_inbound, (d->pop_inbound == 1) ? game_str_sm_trinb1 : game_str_sm_trinb1s);
        lbxfont_print_str_center(268, 140, buf, UI_SCREEN_W, ui_scale);
        lbxfont_print_str_center(268, 148, game_str_sm_trinb2, UI_SCREEN_W, ui_scale);
    }
}

static void bomb_show_draw_cb(void *vptr)
{
    struct bomb_data_s *d = vptr;
    const struct game_s *g = d->g;
    const planet_t *p = &(g->planet[d->planet]);
    const empiretechorbit_t *e = &(g->eto[d->owner]);
    char buf[0x80];
    hw_video_copy_back_from_page2();
    ui_draw_filled_rect(222, 4, 314, 179, 0, ui_scale);
    lbxgfx_draw_frame(222, 4, d->gfx_explobac, UI_SCREEN_W, ui_scale);
    ui_starmap_draw_planetinfo_2(g, d->api, d->api, d->planet);
    bomb_draw_anim(d->gfx_bombard, true);
    /*d615*/
    ui_draw_line1(227, 57, 227, 160, 0, ui_scale);
    ui_draw_line1(227, 57, 310, 57, 0, ui_scale);
    ui_draw_line1(310, 57, 310, 160, 0, ui_scale);
    if (d->hide_other) {
        ui_gmap_basic_draw_only(d->gmap, d->planet);
    } else {
        ui_gmap_basic_draw_frame(d->gmap, d->api);
    }
    ui_gmap_draw_planet_border(g, d->planet);
    lbxfont_select_set_12_4(3, 0xa, 0, 0);
    lbxfont_print_str_center(267, 60, game_str_sm_obomb1, UI_SCREEN_W, ui_scale);
    lbxfont_print_str_center(267, 69, game_str_sm_obomb2, UI_SCREEN_W, ui_scale);
    lbxfont_select_set_12_4(5, tbl_banner_fontparam[e->banner], 0, 0);
    {
        const char *s;
        if ((g->gaux->local_players == 1) && IS_HUMAN(g, d->owner)) {
            s = game_str_sb_your;
        } else {
            s = game_str_tbl_race[e->race];
        }
        lib_sprintf(buf, sizeof(buf), "%s %s", s, game_str_sm_colony);
    }
    lbxfont_print_str_center(270, 84, buf, UI_SCREEN_W, ui_scale);
    if (p->owner == PLAYER_NONE) {
        lbxfont_select_set_12_4(3, 0x0, 0, 0);
        lbxfont_print_str_center(268, 135, game_str_sm_cdest1, UI_SCREEN_W, ui_scale);
        lbxfont_print_str_center(268, 145, game_str_sm_cdest2, UI_SCREEN_W, ui_scale);
    } else if ((d->popdmg == 0) && (d->factdmg == 0)) {
        lbxfont_select_set_12_4(3, 0x0, 0, 0);
        lbxfont_print_str_center(268, 135, game_str_sm_ineff1, UI_SCREEN_W, ui_scale);
        lbxfont_print_str_center(268, 145, game_str_sm_ineff2, UI_SCREEN_W, ui_scale);
    } else {
        lbxfont_select_set_12_4(0, 0x0, 0, 0);
        if (d->popdmg) {
            lib_sprintf(buf, sizeof(buf), "%i %s", d->popdmg, game_str_sm_bkill1);
            lbxfont_print_str_center(267, 120, buf, UI_SCREEN_W, ui_scale);
            lbxfont_print_str_center(267, 127, game_str_sm_bkill2, UI_SCREEN_W, ui_scale);
        }
        if (d->factdmg) {
            lib_sprintf(buf, sizeof(buf), "%i %s", d->factdmg, (d->factdmg == 1) ? game_str_sm_bfact1 : game_str_sm_bfact1s);
            lbxfont_print_str_center(267, 140, buf, UI_SCREEN_W, ui_scale);
            lbxfont_print_str_center(267, 147, game_str_sm_bfact2, UI_SCREEN_W, ui_scale);
        }
    }
}

/* -------------------------------------------------------------------------- */

bool ui_bomb_ask(struct game_s *g, int pi, uint8_t planet_i, int pop_inbound)
{
    struct bomb_data_s d;
    int16_t oi_y, oi_n;
    bool flag_done = false, flag_do_bomb = false;
    ui_switch_1(g, pi);
    d.g = g;
    d.api = pi;
    d.planet = planet_i;
    d.pop_inbound = pop_inbound;
    d.gmap = ui_gmap_basic_init(g, true);
    bomb_load_data(&d);
    uiobj_set_callback_and_delay(bomb_ask_draw_cb, &d, 4);
    uiobj_table_clear();
    oi_n = uiobj_add_t0(227, 164, "", d.gfx_bombbutc, MOO_KEY_ESCAPE);
    oi_y = uiobj_add_t0(271, 164, "", d.gfx_bombbutt, MOO_KEY_b);
    ui_sound_play_music(0xd);
    while (!flag_done) {
        int16_t oi;
        ui_delay_prepare();
        oi = uiobj_handle_input_cond();
        if ((oi == UIOBJI_ESC) || (oi == oi_n)) {
            ui_sound_play_sfx_06();
            flag_done = true;
        }
        if (oi == oi_y) {
            ui_sound_play_sfx_24();
            flag_done = true;
            flag_do_bomb = true;
        }
        bomb_ask_draw_cb(&d);
        ui_draw_finish();
        ui_delay_ticks_or_click(4);
    }
    uiobj_unset_callback();
    uiobj_table_clear();
    ui_sound_stop_music();
    bomb_free_data(&d);
    return flag_do_bomb;
}

void ui_bomb_show(struct game_s *g, int pi, int attacker_i, int owner_i, uint8_t planet_i, int popdmg, int factdmg, bool play_music, bool hide_other)
{
    struct bomb_data_s d;
    bool flag_done = false;
    ui_switch_2(g, attacker_i, owner_i);
    d.g = g;
    d.api = pi;
    d.hide_other = hide_other;
    d.owner = owner_i;
    d.planet = planet_i;
    d.popdmg = popdmg;
    d.factdmg = factdmg;
    d.gmap = ui_gmap_basic_init(g, true);
    bomb_load_data(&d);
    uiobj_set_callback_and_delay(bomb_show_draw_cb, &d, 4);
    uiobj_table_clear();
    uiobj_add_t0(227, 164, "", d.gfx_contbutt, MOO_KEY_c);
    uiobj_add_inputkey(MOO_KEY_SPACE);
    if (g->planet[planet_i].owner == PLAYER_NONE) {
        ui_sound_play_music(0xe);
    } else if (play_music) {
        ui_sound_play_music(0xd);
    }
    while (!flag_done) {
        int16_t oi;
        ui_delay_prepare();
        oi = uiobj_handle_input_cond();
        if (oi != 0) {
            flag_done = true;
        }
        bomb_show_draw_cb(&d);
        ui_draw_finish();
        ui_delay_ticks_or_click(4);
    }
    uiobj_unset_callback();
    uiobj_table_clear();
    ui_sound_stop_music();
    bomb_free_data(&d);
}
