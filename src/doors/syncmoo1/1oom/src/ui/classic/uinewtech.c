#include "config.h"

#include <ctype.h>
#include <stdio.h>

#include "ui.h"
#include "comp.h"
#include "game.h"
#include "game_aux.h"
#include "game_diplo.h"
#include "game_planet.h"
#include "game_misc.h"
#include "game_num.h"
#include "game_str.h"
#include "game_tech.h"
#include "game_techtypes.h"
#include "hw.h"
#include "kbd.h"
#include "lbx.h"
#include "lbxfont.h"
#include "lbxgfx.h"
#include "lib.h"
#include "log.h"
#include "rnd.h"
#include "types.h"
#include "uidelay.h"
#include "uidefs.h"
#include "uidraw.h"
#include "uiobj.h"
#include "uipal.h"
#include "uisound.h"
#include "uiswitch.h"

/* -------------------------------------------------------------------------- */

struct newtech_data_s {
    struct game_s *g;
    player_id_t api;
    uint8_t *gfx_lab;
    uint8_t *gfx_tech;
    uint8_t *gfx_spies;
    uint8_t *gfx_framing;
    uint8_t *gfx_pulldown_u;
    uint8_t *gfx_pulldown_d;
    uint8_t *gfx_eco_chng2;
    uint8_t *gfx_eco_chng4;
    uint8_t *gfx_robo_but;
    newtech_t nt;
    uint8_t music_i;
    int cur_source;
    uint8_t anim;
    uint8_t dialog_type;
    bool flag_fadeout;
    bool flag_music;
    bool flag_is_current;
    bool flag_choose_next;
    uint8_t tech_next[TECH_NEXT_MAX];
    int num_next;
    int16_t selected;
    int16_t tbl_tech[TECH_NEXT_MAX + 1];
};

static const uint8_t newtech_music_tbl[6] = { 5, 6, 7, 5, 5, 5 };

static void newtech_load_data(struct newtech_data_s *d)
{
    d->gfx_framing = lbxfile_item_get(LBXFILE_BACKGRND, 9);
    d->gfx_pulldown_u = lbxfile_item_get(LBXFILE_BACKGRND, 0x1a);
    d->gfx_pulldown_d = lbxfile_item_get(LBXFILE_BACKGRND, 0x1b);
    d->gfx_eco_chng2 = lbxfile_item_get(LBXFILE_BACKGRND, 0x1f);
    d->gfx_eco_chng4 = lbxfile_item_get(LBXFILE_BACKGRND, 0x30);
    d->gfx_robo_but = lbxfile_item_get(LBXFILE_BACKGRND, 0x31);
    d->gfx_spies = 0;
}

static void newtech_free_data(struct newtech_data_s *d)
{
    lbxfile_item_release(LBXFILE_BACKGRND, d->gfx_framing);
    lbxfile_item_release(LBXFILE_BACKGRND, d->gfx_pulldown_u);
    lbxfile_item_release(LBXFILE_BACKGRND, d->gfx_pulldown_d);
    lbxfile_item_release(LBXFILE_BACKGRND, d->gfx_eco_chng2);
    lbxfile_item_release(LBXFILE_BACKGRND, d->gfx_eco_chng4);
    lbxfile_item_release(LBXFILE_BACKGRND, d->gfx_robo_but);
    if (d->gfx_spies) {
        lbxfile_item_release(LBXFILE_SPIES, d->gfx_spies);
    }
}

static void newtech_draw_cb1(void *vptr)
{
    struct newtech_data_s *d = vptr;
    struct game_s *g = d->g;
    char *buf = ui_data.strbuf;
    hw_video_copy_back_from_page2();
    {
        int frame = lbxgfx_get_frame(d->gfx_spies);
        lbxgfx_set_frame_0(d->gfx_spies);
        for (int f = 0; f <= frame; ++f) {
            lbxgfx_draw_frame(0, 0, d->gfx_spies, UI_SCREEN_W, ui_scale);
        }
        if ((d->anim = (d->anim + 1) % 3) != 0) {
            lbxgfx_set_new_frame(d->gfx_spies, frame);
        }
    }
    game_tech_get_newtech_msg(g, d->api, &(d->nt), buf, UI_STRBUF_SIZE);
    lbxfont_select_set_12_4(5, 5, 0, 0);
    lbxfont_print_str_center(161, 7, buf, UI_SCREEN_W, ui_scale);
    if (d->nt.source != TECHSOURCE_CHOOSE) {
        int strh, y;
        char *p, c;
        game_tech_get_name(d->g->gaux, d->nt.field, d->nt.tech, buf, UI_STRBUF_SIZE);
        p = buf;
        while ((c = *p) != 0) {
            if (islower(c)) {
                *p = toupper(c);
            }
            ++p;
        }
        lbxfont_select_set_12_4(5, 8, 0, 0);
        lbxfont_print_str_center(160, 32, buf, UI_SCREEN_W, ui_scale);
        game_tech_get_descr(d->g->gaux, d->nt.field, d->nt.tech, buf, UI_STRBUF_SIZE);
        lbxfont_select_set_12_5(4, 0xf, 0, 0);
        strh = lbxfont_calc_split_str_h(305, buf);
        /* BUG?
           Some lowercase letters extend past the screen, for example 'p' in the Hyper-X msg.
           On DOS/v1.3 this only overwrites unused VRAM. y <= 148 would be OK.
           In this version of 1oom, the video buffer does not contain padding, which can lead
           to small visible artifacts.
        */
        y = (strh >= 36) ? 150 : 160;
        lbxfont_print_str_split(9, y, 305, buf, 3, UI_SCREEN_W, UI_SCREEN_H, ui_scale);
    }
    if (d->nt.frame) {
        /*ui_newtech_draw_frame:*/
        ui_draw_filled_rect(31, 62, 202, 125, 0xfb, ui_scale);
        ui_draw_filled_rect(37, 68, 196, 91, 0x04, ui_scale);
        lbxgfx_draw_frame(31, 62, d->gfx_framing, UI_SCREEN_W, ui_scale);
        ui_draw_filled_rect(50, 106, 110, 120, 0x00, ui_scale);
        ui_draw_filled_rect(51, 107, 109, 119, tbl_banner_color2[g->eto[d->nt.other1].banner], ui_scale);
        ui_draw_filled_rect(122, 106, 183, 120, 0x00, ui_scale);
        ui_draw_filled_rect(123, 107, 182, 119, tbl_banner_color2[g->eto[d->nt.other2].banner], ui_scale);
        lbxfont_select(5, 6, 0, 0);
        lbxfont_print_str_center(80, 110, game_str_tbl_races[g->eto[d->nt.other1].race], UI_SCREEN_W, ui_scale);
        lbxfont_print_str_center(152, 110, game_str_tbl_races[g->eto[d->nt.other2].race], UI_SCREEN_W, ui_scale);
        lbxfont_select(5, 0, 0, 0);
        lbxfont_set_gap_h(2);
        lbxfont_print_str_split(40, 70, 154, game_str_nt_frame, 3, UI_SCREEN_W, UI_SCREEN_H, ui_scale);
        lbxfont_select(0, 0, 0, 0);
        lbxfont_print_str_center(115, 96, game_str_nt_victim, UI_SCREEN_W, ui_scale);
    }
}

static void newtech_choose_next_draw_cb(void *vptr)
{
    struct newtech_data_s *d = vptr;
    char buf[RESEARCH_DESCR_LEN + 20];
    int x = 145, y = 30, yo, pos;
    uint8_t tech = d->tbl_tech[d->selected];
    uint32_t cost;
    d->nt.source = TECHSOURCE_CHOOSE;
    newtech_draw_cb1(d);
    yo = ((d->num_next > 10) ? 8 : 9) * d->num_next + 8;
    SETMAX(yo, 30);
    ui_draw_filled_rect(x, y, x + 165, y + yo + 12, 0xf9, ui_scale);
    lbxgfx_draw_frame_offs(x, y, d->gfx_pulldown_u, 0, y, UI_VGA_W - 1, y + yo - 1, UI_SCREEN_W, ui_scale);
    /* WASBUG
       MOO1 does not limit the bottom part which will go below screen with enough techs to choose from.
       On DOS/v1.3 this only overwrite unused VRAM. We must the the _offs version.
    */
    lbxgfx_draw_frame_offs(x, y + yo, d->gfx_pulldown_d, 0, 0, UI_VGA_W - 1, UI_VGA_H - 1, UI_SCREEN_W, ui_scale);
    lib_sprintf(buf, sizeof(buf), "%s %s", game_str_tbl_te_field[d->nt.field], game_str_te_techno);
    lbxfont_select(5, 0xe, 0, 0);
    lbxfont_print_str_center(x + 85, y + 5, buf, UI_SCREEN_W, ui_scale);
    lbxfont_select_set_12_1(0, 0, 0xe, 0);
    game_tech_get_descr(d->g->gaux, d->nt.field, tech, buf, sizeof(buf));
    pos = strlen(buf);
    cost = game_tech_get_next_rp(d->g, d->api, d->nt.field, tech);
    if (game_num_tech_costmuld2 != 100) {
        cost = (cost * game_num_tech_costmuld2) / 100;
    }
    if (game_num_tech_costmul != 100) {
        cost = (cost * game_num_tech_costmul) / 100;
    }
    lib_sprintf(&buf[pos], sizeof(buf) - pos, " \x02(%u %s)\x1", cost, game_str_te_rp);
    lbxfont_print_str_split(151, y + yo + 18, 156, buf, 0, UI_SCREEN_W, UI_SCREEN_H, ui_scale);
}

static void ui_newtech_choose_next(struct newtech_data_s *d)
{
    char tname[TECH_NEXT_MAX][35];
    const char *nptr[TECH_NEXT_MAX + 1];
    bool cond[TECH_NEXT_MAX + 1];
    int di = (d->num_next > 10) ? 8 : 9;
    for (int i = 0; i < (TECH_NEXT_MAX + 1); ++i) {
        cond[i] = true;
    }
    for (int i = 0; i < d->num_next; ++i) {
        uint8_t tech;
        tech = d->tech_next[i];
        d->tbl_tech[i] = tech; /* WASBUG overwrites flag_copybuf, flag_dialog and d->tech_next */
        game_tech_get_name(d->g->gaux, d->nt.field, tech, tname[i], 35);
        nptr[i] = tname[i];
    }
    nptr[d->num_next] = 0;
    uiobj_set_callback_and_delay(newtech_choose_next_draw_cb, d, 1);
    uiobj_table_clear();
    d->selected = 0;
    newtech_choose_next_draw_cb(d);
    lbxfont_select(0, 0, 0, 0);
    lbxfont_set_gap_h(di - 6);
    ui_draw_filled_rect(155, 49, 304, 56, 0x60, ui_scale);
    for (int i = 0; i < d->num_next; ++i) {
        lbxfont_print_str_normal(156, i * di + di + 41, tname[i], UI_SCREEN_W, ui_scale);
    }
    lbxfont_select_set_12_1(0, 0, 0, 0);
    ui_draw_finish();
    lbxfont_select(0, 0, 0, 0);
    lbxfont_set_gap_h(di - 6);
    /*sel = */uiobj_select_from_list1(156, 41, 148, "", nptr, &d->selected, cond, 1, 0x60, true);
    ui_sound_play_sfx_24();
    ui_delay_prepare();
    game_tech_start_next(d->g, d->api, d->nt.field, d->tbl_tech[d->selected]);
    ui_delay_ticks_or_click(2);
    uiobj_unset_callback();
}

static void newtech_adjust_draw_typestr(char *buf, size_t bufsize, const char *str1, const char *str2, int x, int y, int we)
{
    lib_strcat(buf, game_str_nt_inc, bufsize);
    lib_strcat(buf, str1, bufsize);
    if (str2) {
        lib_strcat(buf, str2, bufsize);
    }
    lbxfont_print_str_split(x + 15, y + 16, 110 + we, buf, 3, UI_SCREEN_W, UI_SCREEN_H, ui_scale);
}

static void newtech_adjust_draw_cb(void *vptr)
{
    struct newtech_data_s *d = vptr;
    char buf[0x96];
    int x = 150, y = 30;
    newtech_draw_cb1(d);
    ui_draw_filled_rect(x, y, x + 135, y + 80, 0xf9, ui_scale);
    lbxgfx_draw_frame(x, y, (d->dialog_type == 0) ? d->gfx_eco_chng2 : d->gfx_eco_chng4, UI_SCREEN_W, ui_scale);
    lbxfont_select_set_12_1(0, 0, 0, 0);
    lib_strcpy(buf, game_str_nt_doyou, sizeof(buf));
    switch (d->dialog_type) {
        case 0:
            lib_strcat(buf, game_str_nt_redueco, sizeof(buf));
            lbxfont_print_str_split(x + 15, y + 11, 110, buf, 3, UI_SCREEN_W, UI_SCREEN_H, ui_scale);
            lbxgfx_set_frame_0(ui_data.gfx.starmap.scrapbut_yes);
            lbxgfx_set_frame_0(ui_data.gfx.starmap.scrapbut_no);
            lbxgfx_draw_frame(x + 83, y + 60, ui_data.gfx.starmap.scrapbut_yes, UI_SCREEN_W, ui_scale);
            lbxgfx_draw_frame(x + 18, y + 60, ui_data.gfx.starmap.scrapbut_no, UI_SCREEN_W, ui_scale);
            break;
        case 1:
            newtech_adjust_draw_typestr(buf, sizeof(buf), game_str_nt_ind, 0, x, y, 0);
            break;
        case 2:
            newtech_adjust_draw_typestr(buf, sizeof(buf), game_str_nt_ecoall, game_str_nt_terra, x, y, 0);
            break;
        case 3:
            newtech_adjust_draw_typestr(buf, sizeof(buf), game_str_nt_def, 0, x, y - 5, 0);
            break;
        case 4:
            newtech_adjust_draw_typestr(buf, sizeof(buf), game_str_nt_ecostd, game_str_nt_terra, x - 1, y - 5, 2);
            break;
        case 5:
            newtech_adjust_draw_typestr(buf, sizeof(buf), game_str_nt_ecohost, game_str_nt_terra, x - 1, y - 5, 2);
            break;
        default:
            break;
    }
}

static void ui_newtech_adjust(struct newtech_data_s *d)
{
    int16_t oi_tbl[3], oi_y, oi_n;
    int x = 150, y = 30;
    bool flag_done = false;
    uiobj_set_callback_and_delay(newtech_adjust_draw_cb, d, 1);
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
            flag_done = true;
        }
        if (oi == oi_y) {
            ui_sound_play_sfx_24();
            game_update_tech_util(d->g);
            game_update_eco_on_waste(d->g, d->api, true);
            flag_done = true;
        }
        for (int i = 0; i < 3; ++i) {
            if (oi == oi_tbl[i]) {
                const planet_slider_i_t tbl_si[5] = { PLANET_SLIDER_IND, PLANET_SLIDER_ECO, PLANET_SLIDER_DEF, PLANET_SLIDER_ECO, PLANET_SLIDER_ECO };
                const int tbl_gr[5] = { 0, 0, 0, 1, 2 };
                ui_sound_play_sfx_24();
                game_update_tech_util(d->g);
                if (!game_num_newtech_adjust_fix) {
                    game_update_eco_on_waste(d->g, d->api, true);
                }
                game_planet_adjust_percent(d->g, d->api, tbl_si[d->dialog_type - 1], game_num_tbl_tech_autoadj[i + 1], tbl_gr[d->dialog_type - 1]);
                flag_done = true;
            }
        }
        if (!flag_done) {
            newtech_adjust_draw_cb(d);
            ui_draw_finish();
        }
        ui_delay_ticks_or_click(1);
    }
    uiobj_table_clear();
}

static void ui_newtech_do(struct newtech_data_s *d)
{
    struct game_s *g = d->g;
    empiretechorbit_t *e = &(g->eto[d->api]);
    uint8_t tech = d->nt.tech;
    bool flag_dialog;
    if (d->cur_source != d->nt.source) {
        if ((d->nt.source <= TECHSOURCE_FOUND) || (d->cur_source != 0)) {
            int m;
            m = ((d->nt.source == TECHSOURCE_RESEARCH) || (d->nt.source > TECHSOURCE_CHOOSE)) ? d->music_i : newtech_music_tbl[d->nt.source];
            ui_sound_play_music(m);
        }
        d->flag_music = true;
        d->cur_source = d->nt.source;
    }
    flag_dialog = false;
    if (tech < 51) {
        flag_dialog = false;
        if ((d->nt.field == TECH_FIELD_CONSTRUCTION) && (((tech - 5) % 10) == 0) && (g->evn.best_wastereduce[d->api] < tech)) {
            flag_dialog = true;
            d->dialog_type = 0;
            g->evn.best_wastereduce[d->api] = tech;
        }
        if ((d->nt.field == TECH_FIELD_PLANETOLOGY) && (g->evn.best_ecorestore[d->api] < tech)) {
            if (0
              || (tech == TECH_PLAN_IMPROVED_ECO_RESTORATION)
              || (tech == TECH_PLAN_ENHANCED_ECO_RESTORATION)
              || (tech == TECH_PLAN_ADVANCED_ECO_RESTORATION)
              || (tech == TECH_PLAN_COMPLETE_ECO_RESTORATION)
            ) {
                flag_dialog = true;
                d->dialog_type = 0;
                g->evn.best_ecorestore[d->api] = tech;
            }
        }
        if (e->race == RACE_SILICOID) {
            flag_dialog = false;
        }
        if ((d->nt.field == TECH_FIELD_COMPUTER) && (((tech - 8) % 10) == 0) && (g->evn.best_roboctrl[d->api] < tech)) {
            flag_dialog = true;
            d->dialog_type = 1;
            g->evn.best_roboctrl[d->api] = tech;
        }
        if (d->nt.field == TECH_FIELD_PLANETOLOGY) {
            if ((((tech - 2) % 6) == 0) && (g->evn.best_terraform[d->api] < tech)) {
                flag_dialog = true;
                d->dialog_type = 2;
                g->evn.best_terraform[d->api] = tech;
            }
            if ((tech == TECH_PLAN_SOIL_ENRICHMENT) || (tech == TECH_PLAN_ADVANCED_SOIL_ENRICHMENT)) {
                flag_dialog = true;
                d->dialog_type = 4;
            }
            if (tech == TECH_PLAN_ATMOSPHERIC_TERRAFORMING) {
                flag_dialog = true;
                d->dialog_type = 5;
            }
        }
        if ((d->nt.field == TECH_FIELD_FORCE_FIELD) && (((tech - 12) % 10) == 0)) {
            flag_dialog = true;
            d->dialog_type = 3;
        }
    }
    if ((ui_draw_finish_mode == 0) && d->flag_fadeout) {
        ui_palette_fadeout_a_f_1();
        ui_draw_finish_mode = 2;
    }
    d->flag_fadeout = false;
again:
    if (d->flag_choose_next) {
        nexttech_t *xt = &(g->evn.newtech[d->api].next[d->nt.field]);
        d->num_next = xt->num;
        if (d->num_next == 0) {
            return;
        }
        memcpy(d->tech_next, xt->tech, d->num_next);
        d->nt.frame = false;
        d->nt.source = TECHSOURCE_CHOOSE;
        ui_newtech_choose_next(d);
    } else {
        bool flag_done;
        int16_t oi_ok, oi_o1, oi_o2;
        oi_ok = UIOBJI_INVALID;
        oi_o1 = UIOBJI_INVALID;
        oi_o2 = UIOBJI_INVALID;
        flag_done = false;
        uiobj_set_callback_and_delay(newtech_draw_cb1, d, 1);
        uiobj_table_clear();
        if (!d->nt.frame) {
            oi_ok = uiobj_add_mousearea_all(MOO_KEY_SPACE);
        } else {
            oi_o1 = uiobj_add_mousearea(50, 106, 110, 120, MOO_KEY_UNKNOWN);
            oi_o2 = uiobj_add_mousearea(122, 106, 183, 120, MOO_KEY_UNKNOWN);
        }
        while (!flag_done) {
            int16_t oi;
            ui_delay_prepare();
            oi = uiobj_handle_input_cond();
            if ((oi == UIOBJI_ESC) || (oi == oi_ok)) {
                flag_done = true;
            }
            if ((oi == oi_o1) || (oi == oi_o2)) {
                player_id_t framed = (oi == oi_o1) ? d->nt.other1 : d->nt.other2;
                flag_done = true;
                ui_sound_play_sfx_24();
                g->evn.stolen_spy[d->nt.stolen_from][d->api] = framed;
                game_diplo_esp_frame(g, framed, d->nt.stolen_from);
            }
            newtech_draw_cb1(d);
            ui_draw_finish();
            ui_delay_ticks_or_click(3);
        }
        uiobj_table_clear();
        if (flag_dialog) {
            ui_newtech_adjust(d);
        }
        if (d->flag_is_current) {
            d->flag_is_current = false;
            d->flag_choose_next = true;
            d->nt.frame = false;
            goto again;
        }
    }
    uiobj_unset_callback();
}

/* -------------------------------------------------------------------------- */

void ui_newtech(struct game_s *g, int pi)
{
    struct newtech_data_s d;
    empiretechorbit_t *e = &(g->eto[pi]);
    bool flag_copybuf = false;

    ui_sound_stop_music();
    uiobj_set_xyoff(0, 0);

    memset(&d, 0, sizeof(d));
    d.g = g;
    d.api = pi;
    d.flag_fadeout = true;
    d.cur_source = -1;
    d.flag_music = false;
    d.anim = 0;

    newtech_load_data(&d);
    for (int i = 0; i < g->evn.newtech[pi].num; ++i) {
        d.nt = g->evn.newtech[pi].d[i];
        if (!flag_copybuf) {
            flag_copybuf = true;
            ui_switch_1(g, pi);
            hw_video_copy_back_from_page2();
            hw_video_copy_back_to_page3();
        }
        d.flag_is_current = false;
        d.flag_choose_next = false;
        if (g->eto[pi].tech.project[d.nt.field] == d.nt.tech) {
            d.flag_is_current = true;
        }
        ui_draw_erase_buf();
        d.gfx_lab = lbxfile_item_get(LBXFILE_TECHNO, (d.nt.source != TECHSOURCE_TRADE) ? d.nt.source : 0);
        lbxgfx_draw_frame(0, 0, d.gfx_lab, UI_SCREEN_W, ui_scale);
        {
            int v;
            if (d.nt.tech <= 50) {
                v = game_tech_get_gfx_i(g->gaux, d.nt.field, d.nt.tech);
            } else {
                const int tbl[TECH_FIELD_NUM] = { 9, 22, 20, 25, 15, 29 };
                v = tbl[d.nt.field];
            }
            d.gfx_tech = lbxfile_item_get(LBXFILE_TECHNO, v);
        }
        lbxgfx_draw_frame(145, 54, d.gfx_tech, UI_SCREEN_W, ui_scale);
        hw_video_copy_back_to_page2();
        {
            int v;
            if (d.nt.source == TECHSOURCE_TRADE) {
                v = g->eto[d.nt.v06].race;
            } else {
                v = d.nt.source * 10 + e->race;
            }
            if (d.gfx_spies) {
                lbxfile_item_release(LBXFILE_SPIES, d.gfx_spies);
            }
            d.gfx_spies = lbxfile_item_get(LBXFILE_SPIES, v);
        }
        d.music_i = newtech_music_tbl[d.nt.source];
        ui_newtech_do(&d);
        lbxfile_item_release(LBXFILE_TECHNO, d.gfx_lab);
        lbxfile_item_release(LBXFILE_TECHNO, d.gfx_tech);
    }
    for (tech_field_t field = 0; field < TECH_FIELD_NUM; ++field) {
        if (game_tech_can_choose(g, pi, field)) {
            if (!flag_copybuf) {
                flag_copybuf = true;
                ui_switch_1(g, pi);
                hw_video_copy_back_from_page2();
                hw_video_copy_back_to_page3();
            }
            if (d.cur_source == -1) {
                ui_draw_erase_buf();
                d.gfx_lab = lbxfile_item_get(LBXFILE_TECHNO, 0);
                lbxgfx_draw_frame(0, 0, d.gfx_lab, UI_SCREEN_W, ui_scale);
                hw_video_copy_back_to_page2();
                lbxfile_item_release(LBXFILE_TECHNO, d.gfx_lab);
                if (d.gfx_spies) {
                    lbxfile_item_release(LBXFILE_SPIES, d.gfx_spies);
                }
                d.gfx_spies = lbxfile_item_get(LBXFILE_SPIES, e->race);
                d.music_i = newtech_music_tbl[0];
            }
            d.nt.field = field;
            d.nt.tech = 0;
            d.nt.source = TECHSOURCE_CHOOSE;
            d.flag_choose_next = true;
            d.nt.frame = false;
            ui_newtech_do(&d);
        }
    }
    if (d.flag_music) {
        ui_sound_stop_music();
    }
    if (flag_copybuf) {
        hw_video_copy_back_from_page3();
        hw_video_copy_back_to_page2();
        ui_palette_fadeout_a_f_1();
        ui_draw_finish_mode = 2;
    }
    g->evn.newtech[pi].num = 0;
    newtech_free_data(&d);
}
