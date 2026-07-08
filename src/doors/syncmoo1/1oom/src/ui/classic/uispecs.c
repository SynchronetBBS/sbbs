#include "config.h"

#include <stdio.h>
#include <string.h>

#include "uispecs.h"
#include "comp.h"
#include "game.h"
#include "game_aux.h"
#include "game_misc.h"
#include "game_parsed.h"
#include "game_str.h"
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
#include "util.h"

/* -------------------------------------------------------------------------- */

struct specs_data_s {
    struct game_s *g;
    player_id_t api;
    int scrapi;
    int renamei;
};

static void specs_print_weap(weapon_t wi, uint8_t wn, char *buf1, size_t buf1_size, char *buf2, size_t buf2_size)
{
    lib_strcpy(buf1, *tbl_shiptech_weap[wi].nameptr, buf1_size);
    /* FIXME make plural form somehow modifiable */
    if ((wn != 1) && (wi != WEAPON_DEATH_SPORES)) {
        lib_strcat(buf1, "s", buf1_size);
    }
    if (tbl_shiptech_weap[wi].numshots > 0) {
        lib_sprintf(buf2, buf2_size, "%i", tbl_shiptech_weap[wi].numshots);
    } else {
        buf2[0] = '\0';
    }
}

static void specs_draw_cb1(void *vptr)
{
    struct specs_data_s *d = vptr;
    struct game_s *g = d->g;
    empiretechorbit_t *e = &(g->eto[d->api]);
    shipresearch_t *srd = &(g->srd[d->api]);

    ui_draw_color_buf(0x3a);
    lbxgfx_draw_frame(0, 0, ui_data.gfx.starmap.viewship, UI_SCREEN_W, ui_scale);

    for (int si = 0; si < e->shipdesigns_num; ++si) {
        shipparsed_t sp;
        const shipdesign_t *sd;
        int y;
        sd = &(srd->design[si]);
        game_parsed_from_design(&sp, sd, 1);
        y = (si << 5) + 5;
        lbxgfx_draw_frame(44, y - 1, ui_data.gfx.starmap.viewshp2, UI_SCREEN_W, ui_scale);
        ui_draw_filled_rect(6, y, 37, y + 29, 0, ui_scale);
        ui_draw_stars(6, y + 1, si * 5, 32, ui_scale);
        lbxgfx_set_frame_0(ui_data.gfx.ships[sp.look]);
        for (int f = 0; f <= ui_data.starmap.frame_ship; ++f) {
            lbxgfx_draw_frame(6, y + 3, ui_data.gfx.ships[sp.look], UI_SCREEN_W, ui_scale);
        }
        lbxgfx_set_new_frame(ui_data.gfx.starmap.viewshbt, 1);
        lbxgfx_draw_frame(106, y + 1, ui_data.gfx.starmap.viewshbt, UI_SCREEN_W, ui_scale);
        lbxfont_select(0, 0xd, 0, 0);
        lbxfont_print_num_right(35, y + 22, srd->shipcount[si], UI_SCREEN_W, ui_scale);
        if (d->renamei != si) {
            lbxfont_print_str_normal(49, y + 2, sp.name, UI_SCREEN_W, ui_scale);
        }
        lbxfont_select(2, 0xb, 0, 0);
        lbxfont_print_num_right(86, y + 13, sp.defense, UI_SCREEN_W, ui_scale);
        lbxfont_print_num_right(86, y + 23, sp.misdefense, UI_SCREEN_W, ui_scale);
        lbxfont_print_num_right(126, y + 13, sp.complevel, UI_SCREEN_W, ui_scale);
        lbxfont_print_num_right(126, y + 23, sp.hp, UI_SCREEN_W, ui_scale);
        lbxfont_print_num_right(163, y + 3, sp.absorb, UI_SCREEN_W, ui_scale);
        lbxfont_print_num_right(163, y + 13, sp.engine + 1, UI_SCREEN_W, ui_scale);
        lbxfont_print_num_right(163, y + 23, sp.man, UI_SCREEN_W, ui_scale);
        lbxfont_select(2, 0xa, 0, 0);
        for (int wi = 0; wi < WEAPON_SLOT_NUM; ++wi) {
            if (sp.wpnn[wi] != 0) {
                char buf1[64];
                char buf2[12];
                lbxfont_print_num_right(176, y + 3 + wi * 7, sp.wpnn[wi], UI_SCREEN_W, ui_scale);
                specs_print_weap(sp.wpnt[wi], sp.wpnn[wi], buf1, sizeof(buf1), buf2, sizeof(buf2));
                lbxfont_print_str_normal(180, y + 3 + wi * 7, buf1, UI_SCREEN_W, ui_scale);
                if (buf2[0] != '\0') {
                    lbxfont_print_str_right(250, y + 3 + wi * 7, buf2, UI_SCREEN_W, ui_scale);
                    lbxfont_print_str_normal(252, y + 3 + wi * 7, "&", UI_SCREEN_W, ui_scale);
                }
            }
        }
        if (sp.special[0] == 0) {
            lbxfont_select(2, 0xa, 0, 0);
        } else {
            lbxfont_set_color_c_n(0xb5, 5);
        }
        lbxfont_print_str_center(285, y + 2, game_str_tbl_st_specsh[sp.special[0]], UI_SCREEN_W, ui_scale);
        if (sp.special[1] != 0) {
            lbxfont_print_str_center(285, y + 9, game_str_tbl_st_specsh[sp.special[1]], UI_SCREEN_W, ui_scale);
        }
        if (sp.special[2] != 0) {
            lbxfont_print_str_center(285, y + 16, game_str_tbl_st_specsh[sp.special[2]], UI_SCREEN_W, ui_scale);
        }
        lbxfont_select(2, 0xa, 0, 0);
        lbxfont_print_str_normal(262, y + 23, game_str_sp_cost, UI_SCREEN_W, ui_scale);
        lbxfont_print_str_normal(305, y + 23, game_str_bc, UI_SCREEN_W, ui_scale);
        lbxfont_print_num_right(295, y + 23, sd->cost, UI_SCREEN_W, ui_scale);
    }

    ui_draw_set_stars_xoffs(false);
    ui_data.starmap.frame_ship = (ui_data.starmap.frame_ship + 1) % 5;
}

static void specs_before_draw_cb(void *vptr)
{
    struct specs_data_s *d = vptr;
    specs_draw_cb1(d);
    lbxgfx_apply_colortable(0, 0, UI_VGA_W - 1, UI_VGA_H - 1, 0, UI_SCREEN_W, ui_scale);
    lbxgfx_draw_frame(67, 73, ui_data.gfx.starmap.dismiss, UI_SCREEN_W, ui_scale);
    lbxfont_select_set_12_1(3, 0, 0, 0);
    lbxfont_print_str_split(74, 83, 174, game_str_sp_before, 2, UI_SCREEN_W, UI_SCREEN_H, ui_scale);
}

static void specs_mustscrap_draw_cb(void *vptr)
{
    struct specs_data_s *d = vptr;
    struct game_s *g = d->g;
    shipresearch_t *srd = &(g->srd[d->api]);
    uint8_t *gfx = ui_data.gfx.ships[srd->design[d->scrapi].look];
    hw_video_copy_back_from_page2();
    lbxgfx_draw_frame(107, 50, ui_data.gfx.starmap.scrap, UI_SCREEN_W, ui_scale);
    lbxfont_select(2, 6, 0, 0);
    lbxfont_print_str_split(117, 58, 90, game_str_sp_only6, 2, UI_SCREEN_W, UI_SCREEN_H, ui_scale);
    lbxgfx_set_frame_0(gfx);
    lbxgfx_draw_frame(114, 102, gfx, UI_SCREEN_W, ui_scale);
    lbxfont_select(0, 0xd, 0, 0);
    lbxfont_print_num_right(143, 119, srd->shipcount[d->scrapi], UI_SCREEN_W, ui_scale);
    lbxfont_select(0, 0, 0, 0);
    lbxfont_print_str_split(150, 107, 60, game_str_sp_wantscrap, 0, UI_SCREEN_W, UI_SCREEN_H, ui_scale);
}

/* -------------------------------------------------------------------------- */

void ui_specs_before(struct game_s *g, player_id_t active_player)
{
    struct specs_data_s d;
    bool flag_done = false;
    int16_t oi_ma;

    game_update_maint_costs(g);

    d.g = g;
    d.api = active_player;
    d.renamei = -1;
    ui_data.starmap.frame_ship = 0;
    ui_data.starmap.stars_xoff1 = 0;
    ui_data.starmap.stars_xoff2 = 0;

    oi_ma = UIOBJI_INVALID;
    uiobj_set_callback_and_delay(specs_before_draw_cb, &d, 2);
    uiobj_table_clear();

    while (!flag_done) {
        int16_t oi;
        oi = uiobj_handle_input_cond();
        ui_delay_prepare();
        if ((oi == oi_ma) || (oi == UIOBJI_ESC)) {
            flag_done = true;
        }
        specs_before_draw_cb(&d);
        uiobj_table_clear();
        oi_ma = uiobj_add_mousearea_all(MOO_KEY_UNKNOWN);
        ui_draw_finish();
        ui_delay_ticks_or_click(3);
    }

    uiobj_unset_callback();
    uiobj_table_clear();
}

void ui_specs_mustscrap(struct game_s *g, player_id_t active_player, int scrapi)
{
    struct specs_data_s d;
    bool flag_done = false;
    int16_t oi_no = UIOBJI_INVALID, oi_yes = UIOBJI_INVALID;

    d.g = g;
    d.api = active_player;
    ui_data.starmap.frame_ship = 0;
    d.scrapi = scrapi;
    d.renamei = -1;

    uiobj_set_callback_and_delay(specs_mustscrap_draw_cb, &d, 2);
    uiobj_table_clear();

    ui_draw_copy_buf();
    lbxgfx_apply_colortable(0, 0, UI_VGA_W - 1, UI_VGA_H - 1, 0, UI_SCREEN_W, ui_scale);
    hw_video_copy_back_to_page2();

    while (!flag_done) {
        int16_t oi;
        oi = uiobj_handle_input_cond();
        ui_delay_prepare();
        if ((oi == oi_no) || (oi == UIOBJI_ESC)) {
            ui_sound_play_sfx_06();
            ui_data.ui_main_loop_action = ui_data.ui_main_loop_action_next;
            flag_done = true;
        } else if (oi == oi_yes) {
            ui_sound_play_sfx_24();
            game_design_scrap(g, active_player, scrapi, ui_data.flag_scrap_for_new_design);
            ui_data.ui_main_loop_action = ui_data.ui_main_loop_action_prev;
            flag_done = true;
        }
        if (!flag_done) {
            specs_mustscrap_draw_cb(&d);
            uiobj_table_clear();
            oi_no = uiobj_add_t0(116, 132, "", ui_data.gfx.starmap.scrapbut_no, MOO_KEY_n);
            oi_yes = uiobj_add_t0(165, 132, "", ui_data.gfx.starmap.scrapbut_yes, MOO_KEY_y);
            ui_draw_finish();
            ui_delay_ticks_or_click(2);
        }
    }

    uiobj_unset_callback();
    uiobj_table_clear();
}

int ui_specs(struct game_s *g, player_id_t active_player, bool flag_cheat)
{
    struct specs_data_s d;
    bool flag_done = false;
    int16_t oi_ma, oi_tbl_scrap[NUM_SHIPDESIGNS], oi_tbl_rename[NUM_SHIPDESIGNS];
    int scrapi = -1;
    game_update_maint_costs(g);

    d.g = g;
    d.api = active_player;
    d.renamei = -1;
    ui_data.starmap.frame_ship = 0;
    ui_data.starmap.stars_xoff1 = 0;
    ui_data.starmap.stars_xoff2 = 0;

    oi_ma = UIOBJI_INVALID;
    for (int i = 0; i < NUM_SHIPDESIGNS; ++i) {
        oi_tbl_scrap[i] = UIOBJI_INVALID;
        oi_tbl_rename[i] = UIOBJI_INVALID;
    }

    uiobj_set_help_id(35);
    uiobj_set_callback_and_delay(specs_draw_cb1, &d, 2);
    uiobj_table_clear();

    while (!flag_done) {
        int16_t oi;
        oi = uiobj_handle_input_cond();
        ui_delay_prepare();
        if ((oi == oi_ma) || (oi == UIOBJI_ESC)) {
            if (!flag_cheat) {
                ui_data.ui_main_loop_action = UI_MAIN_LOOP_FLEET;
                ui_data.flag_scrap_for_new_design = false;
            }
            flag_done = true;
        }
        for (int i = 0; i < NUM_SHIPDESIGNS; ++i) {
            if (oi == oi_tbl_scrap[i]) {
                ui_sound_play_sfx_24();
                scrapi = i;
                ui_data.ui_main_loop_action = UI_MAIN_LOOP_MUSTSCRAP;
                if (!ui_data.flag_scrap_for_new_design) {
                    ui_data.ui_main_loop_action_prev = UI_MAIN_LOOP_SPECS;
                    ui_data.ui_main_loop_action_next = UI_MAIN_LOOP_SPECS;
                } else {
                    ui_specs_mustscrap(g, active_player, scrapi);
                }
                flag_done = true;
            }
            if (oi == oi_tbl_rename[i]) {
                const uint8_t ctbl[8] = { 0x34, 0x34, 0x34, 0x34, 0x34, 0x34, 0x34, 0x34 };
                shipresearch_t *srd = &(g->srd[active_player]);
                char buf[SHIP_NAME_LEN];
                lib_strcpy(buf, srd->design[i].name, sizeof(buf));
                lbxfont_select(0, 0xd, 0, 0);
                d.renamei = i;
                if (uiobj_read_str(49, (i << 5) + 7, 55, buf, SHIP_NAME_LEN - 1, 0, false, ctbl)) {
                    util_trim_whitespace(buf, sizeof(buf));
                    if (buf[0] != 0) {
                        lib_strcpy(srd->design[i].name, buf, SHIP_NAME_LEN);
                    }
                }
                d.renamei = -1;
                flag_done = true;
            }
        }
        if (!flag_done) {
            int sd_num;
            sd_num = g->eto[active_player].shipdesigns_num;
            specs_draw_cb1(&d);
            for (int i = 0; i < NUM_SHIPDESIGNS; ++i) {
                oi_tbl_scrap[i] = UIOBJI_INVALID;
                oi_tbl_rename[i] = UIOBJI_INVALID;
            }
            uiobj_table_clear();
            if ((sd_num > 1) && !flag_cheat) {
                for (int i = 0; i < sd_num; ++i) {
                    oi_tbl_scrap[i] = uiobj_add_t0(106, (i << 5) + 6, "", ui_data.gfx.starmap.viewshbt, MOO_KEY_1 + i);
                }
            }
            if (!flag_cheat) {
                for (int i = 0; i < sd_num; ++i) {
                    oi_tbl_rename[i] = uiobj_add_mousearea(46, (i << 5) + 6, 103, (i << 5) + 13, MOO_KEY_UNKNOWN);
                }
            }
            oi_ma = uiobj_add_mousearea_all(MOO_KEY_o);
            ui_draw_finish();
            ui_delay_ticks_or_click(3);
        }
    }

    uiobj_unset_callback();
    uiobj_set_help_id(-1);
    uiobj_table_clear();
    return scrapi;
}
