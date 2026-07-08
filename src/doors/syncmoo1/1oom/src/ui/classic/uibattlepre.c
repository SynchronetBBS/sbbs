#include "config.h"

#include <stdio.h>

#include "ui.h"
#include "game.h"
#include "game_aux.h"
#include "game_battle.h"
#include "game_str.h"
#include "hw.h"
#include "lbx.h"
#include "lbxfont.h"
#include "lbxgfx.h"
#include "lbxpal.h"
#include "lib.h"
#include "log.h"
#include "rnd.h"
#include "types.h"
#include "uidefs.h"
#include "uidelay.h"
#include "uidraw.h"
#include "uigmap.h"
#include "uiobj.h"
#include "uisound.h"
#include "uistarmap_common.h"
#include "uiswitch.h"

/* -------------------------------------------------------------------------- */

struct ui_battle_pre_data_s {
    struct game_s *g;
    int party_u;
    int party_d;
    int party_winner;
    uint8_t planet_i;
    bool flag_human_att;
    bool hide_other;
    shipsum_t force[2][SHIP_HULL_NUM];
    shipsum_t bases;
    uint8_t *gfx_contbutt;
    uint8_t *gfx_fleet;
    uint8_t *gfx_dfleet;
    uint8_t *gfx_ufleet;
    void *gmapctx;
};

/* -------------------------------------------------------------------------- */

static void battle_pre_load_data(struct ui_battle_pre_data_s *d)
{
    int iu, id;
    struct game_s *g = d->g;
    d->gfx_contbutt = lbxfile_item_get(LBXFILE_BACKGRND, 0x0c);
    d->gfx_fleet = lbxfile_item_get(LBXFILE_BACKGRND, 0x15);
    id = (d->party_d < PLAYER_NUM) ? g->eto[d->party_d].banner : 6;
    iu = (d->party_u < PLAYER_NUM) ? g->eto[d->party_u].banner : 6;
    if (!d->flag_human_att) {
        uint8_t t = id; id = iu; iu = t;
    }
    d->gfx_dfleet = lbxfile_item_get(LBXFILE_BACKGRND, 0x28 + id);
    d->gfx_ufleet = lbxfile_item_get(LBXFILE_BACKGRND, 0x21 + iu);
    d->gmapctx = ui_gmap_basic_init(g, true);
}

static void battle_pre_free_data(struct ui_battle_pre_data_s *d)
{
    lbxfile_item_release(LBXFILE_BACKGRND, d->gfx_contbutt);
    lbxfile_item_release(LBXFILE_BACKGRND, d->gfx_fleet);
    lbxfile_item_release(LBXFILE_BACKGRND, d->gfx_dfleet);
    lbxfile_item_release(LBXFILE_BACKGRND, d->gfx_ufleet);
    ui_gmap_basic_shutdown(d->gmapctx);
}

static void ui_battle_pre_draw_cb(void *vptr)
{
    struct ui_battle_pre_data_s *d = vptr;
    struct game_s *g = d->g;
    const planet_t *p = &(g->planet[d->planet_i]);
    char buf[32];
    hw_video_copy_back_from_page2();
    ui_draw_filled_rect(222, 4, 314, 179, 0, ui_scale);
    lbxgfx_draw_frame(227, 57, d->gfx_ufleet, UI_SCREEN_W, ui_scale);
    lbxgfx_draw_frame(227, 102, d->gfx_dfleet, UI_SCREEN_W, ui_scale);
    lbxgfx_draw_frame(222, 4, d->gfx_fleet, UI_SCREEN_W, ui_scale);
    if (d->hide_other) {
        ui_gmap_basic_draw_only(d->gmapctx, d->planet_i);
    } else {
        ui_gmap_basic_draw_frame(d->gmapctx, d->party_u);
    }
    ui_gmap_draw_planet_border(g, d->planet_i);
    ui_starmap_draw_planetinfo_2(g, d->party_u, d->party_d, d->planet_i);
    if (p->owner != PLAYER_NONE) {
        uint8_t *gfx;
        int x, y;
        x = (p->x * 215) / g->galaxy_maxx + 5;
        y = (p->y * 171) / g->galaxy_maxy + 5;
        gfx = ui_data.gfx.starmap.smalflag[g->eto[p->owner].banner];
        lbxgfx_draw_frame(x + 3, y - 2, gfx, UI_SCREEN_W, ui_scale);
    }
    lbxfont_select_set_12_4(3, 0, 0, 0);
    lbxfont_print_str_center(267, 64, game_str_bp_scombat, UI_SCREEN_W, ui_scale);
    if (d->party_d >= PLAYER_NUM) {
        lib_strcpy(buf, game_str_tbl_mon_names[d->party_d - PLAYER_NUM], sizeof(buf));
    } else {
        race_t race = g->eto[d->flag_human_att ? d->party_u : d->party_d].race;
        lib_strcpy(buf, game_str_tbl_races[race], sizeof(buf));
    }
    if (ui_space_combat_autoresolve) {
        if (d->flag_human_att) {
            lbxfont_print_str_normal(230, 80, buf, UI_SCREEN_W, ui_scale);
        } else {
            lbxfont_print_str_right(308, 80, buf, UI_SCREEN_W, ui_scale);
        }
    } else {
        lbxfont_print_str_center(267, 100, buf, UI_SCREEN_W, ui_scale);
    }
    lbxfont_print_str_center(267, ui_space_combat_autoresolve ? 90 : 115, (d->party_d >= PLAYER_NUM) ? game_str_bp_attacks : game_str_bp_attack, UI_SCREEN_W, ui_scale);
    {
        race_t race = g->eto[d->flag_human_att ? d->party_d : d->party_u].race;
        lib_strcpy(buf, game_str_tbl_races[race], sizeof(buf));
    }
    if (ui_space_combat_autoresolve) {
        if (d->flag_human_att) {
            lbxfont_print_str_right(308, 100, buf, UI_SCREEN_W, ui_scale);
        } else {
            lbxfont_print_str_normal(230, 100, buf, UI_SCREEN_W, ui_scale);
        }
    } else {
        lbxfont_print_str_center(267, 130, buf, UI_SCREEN_W, ui_scale);
    }
    if (ui_space_combat_autoresolve) {
        int y = 112;
        lbxfont_set_temp_color(0x0);
        lbxfont_select_set_12_4(0, 0x2, 0, 0);
        for (int i = 0; i < SHIP_HULL_NUM; ++i, y += 8) {
            lbxfont_print_num_normal(230, y, d->force[SIDE_L][i], UI_SCREEN_W, ui_scale);
            lbxfont_print_num_right(308, y, d->force[SIDE_R][i], UI_SCREEN_W, ui_scale);
            lbxfont_print_str_center(269, y, game_str_tbl_st_hull[i], UI_SCREEN_W, ui_scale);
        }
        if (d->bases) {
            if (d->flag_human_att) {
                lbxfont_print_num_right(308, y, d->bases, UI_SCREEN_W, ui_scale);
                lbxfont_print_str_normal(230, y, game_str_bt_bases, UI_SCREEN_W, ui_scale);
            } else {
                lbxfont_print_num_normal(230, y, d->bases, UI_SCREEN_W, ui_scale);
                lbxfont_print_str_right(308, y, game_str_bt_bases, UI_SCREEN_W, ui_scale);
            }
        }
        if (d->party_winner >= 0) {
            const char *str;
            if (d->party_winner >= PLAYER_NUM) {
                str = game_str_tbl_mon_names[d->party_winner - PLAYER_NUM];
            } else {
                race_t race = g->eto[d->party_winner].race;
                str = game_str_tbl_races[race];
                lbxfont_set_color0(tbl_banner_color[g->eto[d->party_winner].banner]);
                lbxfont_select_set_12_4(0, tbl_banner_fontparam[g->eto[d->party_winner].banner], 0, 0);
            }
            lib_sprintf(buf, sizeof(buf), "%s %s", str, game_str_bp_won);
            y += 8;
            lbxfont_print_str_center(267, y, buf, UI_SCREEN_W, ui_scale);
        }
    }
}

/* -------------------------------------------------------------------------- */

ui_battle_autoresolve_t ui_battle_pre(struct game_s *g, const struct battle_s *bt, bool hide_other, int winner)
{
    struct ui_battle_pre_data_s d[1];
    int16_t oi_cont = UIOBJI_INVALID, oi_cont2 = UIOBJI_INVALID, oi_auto = UIOBJI_INVALID, oi_retreat = UIOBJI_INVALID;
    int16_t oi_esc = UIOBJI_INVALID;
    bool flag_done = false;
    ui_battle_autoresolve_t ret;
    int party_u = bt->s[SIDE_L].party, party_d = bt->s[SIDE_R].party;
    memset(d, 0, sizeof(*d));
    d->g = g;
    d->party_u = party_u;
    d->party_d = party_d;
    d->party_winner = (winner != SIDE_NONE) ? bt->s[winner].party : -1;
    d->planet_i = bt->planet_i;
    d->flag_human_att = bt->flag_human_att;
    d->hide_other = hide_other;
    battle_pre_load_data(d);
    if (IS_HUMAN(g, party_u)) {
        g->planet_focus_i[party_u] = bt->planet_i;
    }
    if (IS_HUMAN(g, party_d)) {
        g->planet_focus_i[party_d] = bt->planet_i;
    }
    if (ui_space_combat_autoresolve) {
        d->bases = bt->bases;
        game_battle_count_hulls(bt, d->force);
    }
    uiobj_table_clear();
    oi_cont = uiobj_add_t0(227, 163, "", d->gfx_contbutt, MOO_KEY_c);
    oi_cont2 = uiobj_add_inputkey(MOO_KEY_SPACE);
    if (ui_space_combat_autoresolve && (winner == SIDE_NONE)) {
        oi_auto = uiobj_add_t0(250, 152, "", ui_data.gfx.space.autob, MOO_KEY_a);
        oi_retreat = uiobj_add_t0(270, 152, "", ui_data.gfx.space.retreat, MOO_KEY_r);
    }
    if (ui_space_combat_autoresolve) {
        oi_esc = UIOBJI_ESC;
    }
    uiobj_set_focus(oi_cont);
    uiobj_set_callback_and_delay(ui_battle_pre_draw_cb, &d, 4);
    while (!flag_done) {
        int16_t oi;
        ui_delay_prepare();
        oi = uiobj_handle_input_cond();
        if ((oi == oi_cont) || (oi == oi_cont2)) {
            ui_sound_play_sfx_24();
            ret = UI_BATTLE_AUTORESOLVE_OFF;
            flag_done = true;
        } else if (oi == oi_auto || oi == oi_esc) {
            ui_sound_play_sfx_24();
            ret = UI_BATTLE_AUTORESOLVE_AUTO;
            flag_done = true;
        } else if (oi == oi_retreat) {
            ui_sound_play_sfx_24();
            ret = UI_BATTLE_AUTORESOLVE_RETREAT;
            flag_done = true;
        }
        if (!flag_done) {
            ui_battle_pre_draw_cb(d);
            ui_draw_finish();
        }
        ui_delay_ticks_or_click(4);
    }
    uiobj_table_clear();
    uiobj_unset_callback();
    battle_pre_free_data(d);
    return ret;
}
