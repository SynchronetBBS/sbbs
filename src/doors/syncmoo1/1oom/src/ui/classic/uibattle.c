#include "config.h"

#include <stdio.h>
#include <stdlib.h>

#include "ui.h"
#include "comp.h"
#include "game.h"
#include "game_aux.h"
#include "game_battle.h"
#include "game_battle_human.h"
#include "game_misc.h"
#include "game_shiptech.h"
#include "game_str.h"
#include "gfxaux.h"
#include "hw.h"
#include "lbx.h"
#include "lbxfont.h"
#include "lbxgfx.h"
#include "lbxpal.h"
#include "lib.h"
#include "log.h"
#include "rnd.h"
#include "types.h"
#include "uibattlepre.h"
#include "uicursor.h"
#include "uidefs.h"
#include "uidelay.h"
#include "uidraw.h"
#include "uiobj.h"
#include "uipal.h"
#include "uisound.h"
#include "uistarmap_common.h"
#include "uiswitch.h"
#include "util_math.h"

/* -------------------------------------------------------------------------- */

#define MAX_VISIBLE_MISSILE 10

struct ui_battle_data_s {
    struct battle_s *bt;
    uint8_t *gfx_bg;
    bool show_switch;
    uint8_t frame_ship;
    uint8_t frame_missile;
    bool flag_scanning;
    battle_side_i_t scan_side;
    int16_t oi_ai;
    int16_t oi_missile;
    int16_t oi_special;
    int16_t oi_wait;
    int16_t oi_retreat;
    int16_t oi_planet;
    int16_t oi_scan;
    int16_t oi_auto;
    int16_t oi_done;
    int16_t oi_grid;
    int16_t oi_area[BATTLE_AREA_H][BATTLE_AREA_W];
    ui_cursor_area_t cursor[1 + BATTLE_AREA_W * BATTLE_AREA_H];
};

/* -------------------------------------------------------------------------- */

static void ui_battle_draw_scan_cb(void *vptr)
{
    const struct battle_s *bt = vptr;
    struct ui_battle_data_s *d = bt->uictx;
    int itembase, itemnum;
    itembase = (d->scan_side == SIDE_L) ? 1 : (bt->s[SIDE_L].items + 1);
    itemnum = bt->s[d->scan_side].items;
    ui_draw_color_buf(0x3a);
    lbxgfx_draw_frame(0, 0, ui_data.gfx.starmap.viewship, UI_SCREEN_W, ui_scale);
    for (int i = 0; i < itemnum; ++i) {
        const struct battle_item_s *b = &(bt->item[itembase + i]);
        int y;
        y = i * 32 + 5;
        lbxgfx_draw_frame(46, y - 1, ui_data.gfx.space.vs2, UI_SCREEN_W, ui_scale);
        ui_draw_filled_rect(6, y, 38, y + 29, 0, ui_scale);
        ui_draw_stars(6, y + 1, 0, 32, ui_scale);
        ui_battle_draw_item(bt, itembase + i, 6, y + 1);
        lbxfont_select(2, 0xd, 0, 0);
        lbxfont_print_str_normal(52, y + 3, b->name, UI_SCREEN_W, ui_scale);
        lbxfont_select(2, 0xb, 0, 0);
        lbxfont_print_num_right(84, y + 13, b->defense, UI_SCREEN_W, ui_scale);
        lbxfont_print_num_right(84, y + 23, b->misdefense, UI_SCREEN_W, ui_scale);
        lbxfont_print_num_right(124, y + 13, b->complevel, UI_SCREEN_W, ui_scale);
        if (b->hp1 < b->hp2) {
            lbxfont_select(2, 5, 0, 0);
        }
        lbxfont_print_num_right(124, y + 23, b->hp1, UI_SCREEN_W, ui_scale);
        lbxfont_select(2, 0xb, 0, 0);
        lbxfont_print_num_right(161, y + 3, b->absorb, UI_SCREEN_W, ui_scale);
        lbxfont_print_num_right(161, y + 13, b->hploss, UI_SCREEN_W, ui_scale);
        lbxfont_print_num_right(161, y + 23, b->man - b->unman, UI_SCREEN_W, ui_scale);
        lbxfont_select(2, 0xa, 0, 0);
        for (int wi = 0; wi < WEAPON_SLOT_NUM; ++wi) {
            if (b->wpn[wi].n != 0) {
                const struct shiptech_weap_s *w = &(tbl_shiptech_weap[b->wpn[wi].t]);
                lbxfont_print_num_right(176, y + 3 + wi * 7, b->wpn[wi].n, UI_SCREEN_W, ui_scale);
                lbxfont_print_str_normal(181, y + 3 + wi * 7, *w->nameptr, UI_SCREEN_W, ui_scale);
                if (b->wpn[wi].numshots >= 0) {
                    lbxfont_print_num_right(250, y + 3 + wi * 7, b->wpn[wi].numshots, UI_SCREEN_W, ui_scale);
                    lbxfont_print_str_normal(252, y + 3 + wi * 7, "&", UI_SCREEN_W, ui_scale);
                }
                if (w->misstype == 1) {
                    uint8_t c1, c2;
                    if (b->wpn[wi].numfire > 0) {
                        c1 = 0x6c;
                        c2 = 0x6f;
                    } else {
                        c1 = 0x28;
                        c2 = 0x41;
                    }
                    ui_draw_filled_rect(248, y + 3 + wi * 7, 253, y + 7 + wi * 7, c1, ui_scale);
                    ui_draw_filled_rect(249, y + 4 + wi * 7, 252, y + 6 + wi * 7, c2, ui_scale);
                }
            }
        }
        if (b->special[0] == 0) {
            lbxfont_select(2, 0xa, 0, 0);
        } else {
            lbxfont_set_color_c_n(0xb5, 5);
        }
        lbxfont_print_str_center(286, y + 2, game_str_tbl_st_specsh[b->special[0]], UI_SCREEN_W, ui_scale);
        if (b->special[1] != 0) {
            lbxfont_print_str_center(286, y + 11, game_str_tbl_st_specsh[b->special[1]], UI_SCREEN_W, ui_scale);
        }
        if (b->special[2] != 0) {
            lbxfont_print_str_center(286, y + 20, game_str_tbl_st_specsh[b->special[2]], UI_SCREEN_W, ui_scale);
        }
    }
    uiobj_finish_frame();
    ui_draw_copy_buf();
    ui_draw_set_stars_xoffs(d->scan_side == SIDE_R);
    d->frame_ship = (d->frame_ship + 1) % 5;
}

static void ui_battle_draw_focusinfo(const struct battle_s *bt)
{
    const struct battle_item_s *b;
    int16_t v = uiobj_at_cursor() - 1;
    if ((v < 0) || (v >= (BATTLE_AREA_W * BATTLE_AREA_H))) {
        v = 100;
    } else {
        int sx, sy;
        sx = v % BATTLE_AREA_W;
        sy = v / BATTLE_AREA_W;
        v = bt->area[sy][sx];
    }
    if ((v == 1) || (v == 0)) {
        v = 100;
    }
    if (v < 0) {
        if (v == -30) {
            v = 0;
        } else {
            v = -v;
        }
    }
    if ((v >= 10) && (v < 30)) {
        v -= 10;
    }
    if (v >= 30) {
        v -= 30;
    }
    ui_draw_filled_rect(0, 193, 94, UI_VGA_H - 1, 0xe9, ui_scale);
    b = (v == 70) ? &(bt->item[bt->cur_item]) : &(bt->item[v]);
    lbxfont_select(2, 0xa, 5, 0);
    lbxfont_print_str_normal(2, 194, b->name, UI_SCREEN_W, ui_scale);
    if (v != 70) {
        if (b->num > 0) {
            char buf[32];
            int h1 = b->hp1, h2 = b->hp2, ha = h1 - b->hploss, pos;
            pos = lib_sprintf(buf, sizeof(buf), "%i/", ha);
            if (h1 < h2) {
                buf[pos++] = 2;
            }
            lib_sprintf(&buf[pos], sizeof(buf) - pos, "%i", h1);
            lbxfont_print_str_right(92, 194, buf, UI_SCREEN_W, ui_scale);
        }
    }
}

static void ui_battle_clear_ois(struct ui_battle_data_s *d)
{
    d->oi_missile = UIOBJI_INVALID;
    d->oi_special = UIOBJI_INVALID;
    d->oi_wait = UIOBJI_INVALID;
    d->oi_retreat = UIOBJI_INVALID;
    d->oi_planet = UIOBJI_INVALID;
    d->oi_scan = UIOBJI_INVALID;
    d->oi_auto = UIOBJI_INVALID;
    d->oi_done = UIOBJI_INVALID;
    d->oi_grid = UIOBJI_INVALID;
    for (int y = 0; y < BATTLE_AREA_H; ++y) {
        for (int x = 0; x < BATTLE_AREA_W; ++x) {
            d->oi_area[y][x] = UIOBJI_INVALID; /* BUG? not done in MOO1 */
        }
    }
}

static void ui_battle_draw_bottom_no_ois(const struct battle_s *bt)
{
    struct ui_battle_data_s *d = bt->uictx;
    const struct battle_item_s *b;
    uint8_t *gfx;
    ui_draw_filled_rect(0, 193, 319, 199, 0xe9, ui_scale);
    ui_draw_line1(120, 192, 120, 199, 0xeb, ui_scale);
    ui_draw_line1(96, 192, 96, 199, 0xeb, ui_scale);
    ui_draw_line1(0, 192, 319, 192, 0xeb, ui_scale);
    lbxfont_select(2, 0xa, 5, 0);
    b = &(bt->item[bt->cur_item]);
    if (bt->s[b->side].flag_auto) {
        lbxfont_print_str_normal(2, 194, game_str_bt_auto_move, UI_SCREEN_W, ui_scale);
    } else {
        ui_battle_draw_focusinfo(bt);
    }
    if (bt->item[0].side != SIDE_NONE) {
        gfx = ui_data.gfx.space.planet;
        lbxgfx_set_frame_0(gfx);
    } else {
        gfx = ui_data.gfx.space.planet_off;
    }
    lbxgfx_draw_frame(123, 193, gfx, UI_SCREEN_W, ui_scale);
    if (1
      && ((bt->cur_item != 0) || (b->num > 0) || (!bt->s[b->side].flag_auto))
      && (((b->man - b->unman) != 0) || (bt->cur_item == 0) || (!bt->s[b->side].flag_human))
    ) {
        gfx = ui_data.gfx.space.retreat;
        lbxgfx_set_frame_0(gfx);
    } else {
        gfx = ui_data.gfx.space.retr_off;
    }
    lbxgfx_draw_frame(241, 193, gfx, UI_SCREEN_W, ui_scale);
    if (bt->s[b->side].flag_auto) {
        ui_battle_clear_ois(d);
    }
    if ((b->missile == -1) || bt->s[b->side].flag_auto) {
        gfx = ui_data.gfx.space.misl_off;
    } else if (bt->cur_item == 0) {
        gfx = ui_data.gfx.space.base_btn;
        if (tbl_shiptech_weap[b->wpn[0].t].nummiss == 1) {
            lbxgfx_set_frame_0(gfx);
        } else {
            lbxgfx_set_new_frame(gfx, 1);
        }
    } else {
        gfx = ui_data.gfx.space.misbutt;
        if (b->missile == 0) {
            lbxgfx_set_frame_0(gfx);
        } else {
            lbxgfx_set_new_frame(gfx, 1);
        }
    }
    lbxgfx_draw_frame(175, 193, gfx, UI_SCREEN_W, ui_scale);
    if ((bt->special_button == -1) || bt->s[b->side].flag_auto) {
        gfx = ui_data.gfx.space.spec_off;
    } else {
        gfx = ui_data.gfx.space.special;
        if (bt->special_button == 0) {
            lbxgfx_set_frame_0(gfx);
        } else {
            lbxgfx_set_new_frame(gfx, 1);
        }
    }
    lbxgfx_draw_frame(210, 193, gfx, UI_SCREEN_W, ui_scale);
    if (bt->s[bt->s[b->side].flag_human ? b->side : SIDE_L].flag_have_scan) {
        gfx = ui_data.gfx.space.scan;
        lbxgfx_set_frame_0(gfx);
    } else {
        gfx = ui_data.gfx.space.scan_off;
    }
    lbxgfx_draw_frame(153, 193, gfx, UI_SCREEN_W, ui_scale);
    gfx = ui_data.gfx.space.done;
    lbxgfx_set_frame_0(gfx);
    lbxgfx_draw_frame(297, 193, gfx, UI_SCREEN_W, ui_scale);
    gfx = ui_data.gfx.space.wait;
    lbxgfx_set_frame_0(gfx);
    lbxgfx_draw_frame(274, 193, gfx, UI_SCREEN_W, ui_scale);
    gfx = ui_data.gfx.space.autob;
    if (bt->s[b->side].flag_auto) {
        lbxgfx_set_new_frame(gfx, 1);
    } else {
        lbxgfx_set_frame_0(gfx);
    }
    lbxgfx_draw_frame(99, 193, gfx, UI_SCREEN_W, ui_scale);
}

static void ui_battle_draw_bottom_add_ois(const struct battle_s *bt)
{
    struct ui_battle_data_s *d = bt->uictx;
    const struct battle_item_s *b;
    uint8_t *gfx;
    uiobj_table_clear();
    for (int sy = 0; sy < BATTLE_AREA_H; ++sy) {
        int y0, y1;
        y0 = sy * 24;
        y1 = y0 + 23;
        for (int sx = 0; sx < BATTLE_AREA_W; ++sx) {
            ui_cursor_area_t *cr = &(d->cursor[1 + sy * BATTLE_AREA_W + sx]);
            int x0, x1;
            x0 = sx * 32;
            x1 = x0 + 31;
            cr->x0 = x0 * ui_scale;
            cr->y0 = y0 * ui_scale;
            cr->x1 = x1 * ui_scale + ui_scale - 1;
            cr->y1 = y1 * ui_scale + ui_scale - 1;
            d->oi_area[sy][sx] = uiobj_add_mousearea(x0, y0, x1, y1, MOO_KEY_UNKNOWN);
        }
    }
    ui_draw_filled_rect(0, 193, 319, 199, 0xe9, ui_scale);
    ui_draw_line1(120, 192, 120, 199, 0xeb, ui_scale);
    ui_draw_line1(96, 192, 96, 199, 0xeb, ui_scale);
    ui_draw_line1(0, 192, 319, 192, 0xeb, ui_scale);
    if (bt->item[0].side != SIDE_NONE) {
        d->oi_planet = uiobj_add_t0(123, 193, "", ui_data.gfx.space.planet, MOO_KEY_p);
    } else {
        lbxgfx_draw_frame(123, 193, ui_data.gfx.space.planet_off, UI_SCREEN_W, ui_scale);
        d->oi_planet = UIOBJI_INVALID;
    }
    b = &(bt->item[bt->cur_item]);
    if (0
      || ((b->side != SIDE_NONE) && (bt->s[b->side].items <= 0))
      || ((bt->cur_item != 0) && (b->num <= 0))
      || (((b->man - b->unman) != 0) && (bt->cur_item == 0))
    ) {
        lbxgfx_draw_frame(241, 193, ui_data.gfx.space.retr_off, UI_SCREEN_W, ui_scale);
        d->oi_retreat = UIOBJI_INVALID;
    } else {
        d->oi_retreat = uiobj_add_t0(241, 193, "", ui_data.gfx.space.retreat, MOO_KEY_r);
    }
    d->oi_done = uiobj_add_t0(297, 193, "", ui_data.gfx.space.done, MOO_KEY_d);
    d->oi_grid = uiobj_add_inputkey(MOO_KEY_g);
    d->oi_wait = uiobj_add_t0(274, 193, "", ui_data.gfx.space.wait, MOO_KEY_w);
#if 0
    d->oi_auto = uiobj_add_t0(99, 193, "", ui_data.gfx.space.autob, MOO_KEY_a);
#else
    /* HACK MOO1 does this. Breaks const *bt and requires flag to be int16_t, but works while it is not the player's turn. */
    d->oi_auto = uiobj_add_t1(99, 193, "", ui_data.gfx.space.autob, &(((struct battle_s *)bt)->s[b->side].flag_auto), MOO_KEY_a);
#endif
    if ((b->missile == 0) || (b->missile == 1)) {
        if (bt->cur_item == 0) {
            if (b->wpn[0].t != b->wpn[1].t) {
                d->oi_missile = uiobj_add_mousearea(175, 193, 208, 199, MOO_KEY_m);
                gfx = ui_data.gfx.space.base_btn;
                if (bt->s[b->side].flag_base_missile) {
                    lbxgfx_set_frame_0(gfx);
                } else {
                    lbxgfx_set_new_frame(gfx, 1);
                }
            } else {
                gfx = ui_data.gfx.space.misl_off;
                d->oi_missile = UIOBJI_INVALID;
            }
        } else {
            d->oi_missile = uiobj_add_mousearea(175, 193, 208, 199, MOO_KEY_m);
            gfx = ui_data.gfx.space.misbutt;
            if (b->missile == 0) {
                lbxgfx_set_frame_0(gfx);
            } else {
                lbxgfx_set_new_frame(gfx, 1);
            }
        }
    } else {
        gfx = ui_data.gfx.space.misl_off;
        d->oi_missile = UIOBJI_INVALID;
    }
    lbxgfx_draw_frame(175, 193, gfx, UI_SCREEN_W, ui_scale);
    if (bt->s[b->side].flag_have_scan) {
        d->oi_scan = uiobj_add_t0(153, 193, "", ui_data.gfx.space.scan, MOO_KEY_s);
    } else {
        lbxgfx_draw_frame(153, 193, ui_data.gfx.space.scan_off, UI_SCREEN_W, ui_scale);
        d->oi_scan = UIOBJI_INVALID;
    }
    if ((bt->special_button == 0) || ((bt->special_button == 1) && (bt->cur_item != 0))) {
        gfx = ui_data.gfx.space.special;
        if ((b->pulsar == 3) || (b->stasis == 3) || (b->pulsar == 4)) {
            lbxgfx_set_frame_0(gfx);
            d->oi_special = UIOBJI_INVALID;
        } else {
            d->oi_special = uiobj_add_mousearea(210, 193, 239, 199, MOO_KEY_p);
            if (bt->special_button == 0) {
                lbxgfx_set_frame_0(gfx);
            } else {
                lbxgfx_set_new_frame(gfx, 1);
            }
        }
    } else {
        gfx = ui_data.gfx.space.spec_off;
        d->oi_special = UIOBJI_INVALID;
    }
    lbxgfx_draw_frame(210, 193, gfx, UI_SCREEN_W, ui_scale);
}

static void ui_battle_draw_cb(void *vptr)
{
    const struct battle_s *bt = vptr;
    ui_battle_draw_arena(bt, 0, 0);
    ui_battle_draw_bottom_no_ois(bt);
}

static void ui_battle_transition_to(int px, int py, int steps)
{
    uiobj_table_clear();
    hw_video_copy_buf_out(ui_data.aux.screen.data);
    ui_draw_copy_buf();
    memcpy(ui_data.gfx.vgafileh, ui_data.aux.screen.data, UI_SCREEN_W * UI_SCREEN_H);
    for (int i = 1; i <= steps; ++i) {
        int x, y, percent;
        ui_delay_prepare();
        ui_data.aux.screen.w = UI_SCREEN_W;
        ui_data.aux.screen.h = UI_SCREEN_H;
        memcpy(ui_data.aux.screen.data, ui_data.gfx.vgafileh, UI_SCREEN_W * UI_SCREEN_H);
        x = px - ((i * px) / steps);
        y = py - ((i * py) / steps);
        percent = (i * 100) / steps;
        if ((x + (percent * UI_SCREEN_W) / 100) > UI_SCREEN_W) {
            x = UI_SCREEN_W - (percent * UI_SCREEN_W) / 100;
        }
        if ((y + (percent * UI_SCREEN_H) / 100) > UI_SCREEN_H) {
            y = UI_SCREEN_H - (percent * UI_SCREEN_H) / 100;
        }
        gfx_aux_scale(&ui_data.aux.screen, percent, percent);
        gfx_aux_color_replace(&ui_data.aux.screen, 0, 1);
        gfx_aux_draw_frame_from(x, y, &ui_data.aux.screen, UI_SCREEN_W, 1);
        uiobj_finish_frame();
        ui_delay_us_or_click(MOO_TICKS_TO_US(1) / 3);
    }
#if 0
    /* test for scale up */
    ui_data.aux.screen.w = UI_SCREEN_W;
    ui_data.aux.screen.h = UI_SCREEN_H;
    memcpy(ui_data.aux.screen.data, ui_data.gfx.vgafileh, UI_SCREEN_W * UI_SCREEN_H);
    struct gfx_auxbuf_s aux2;
    memset(&aux2, 0, sizeof(aux2));
    for (int i = 1; i <= steps; ++i) {
        int x, y, percent;
        ui_delay_prepare();
        lbxgfx_setup_auxbuf_wh(&aux2, 320, 200);
        aux2.w = 32;
        aux2.h = 24;
        gfx_aux_draw_frame_to(ui_data.gfx.ships[0x92], &aux2);
        percent = (i * 700) / steps;
        x = 320/2 - (percent * (200/2)) / 700;
        y = 200/2 - (percent * (200/2)) / 700;
        gfx_aux_scale(&aux2, percent, percent);
        memcpy(ui_data.aux.screen.data, ui_data.gfx.vgafileh, UI_SCREEN_W * UI_SCREEN_H);
        gfx_aux_color_replace(&ui_data.aux.screen, 0, 1);
        gfx_aux_overlay(x, y, &ui_data.aux.screen, &aux2);
        gfx_aux_draw_frame_from(0, 0, &ui_data.aux.screen, UI_SCREEN_W);
        uiobj_finish_frame();
        ui_delay_us_or_click(MOO_TICKS_TO_US(1) / 3);
    }
    ui_delay_us_or_click(1000000);
#endif
}

static void ui_battle_draw_beam_line(int fx, int fy, int tx, int ty, int d0, int d1, uint8_t v24, const uint8_t *ctbl)
{
    int dist, x0, y0, x1, y1;
    dist = util_math_dist_fast(fx, fy, tx, ty);
    x0 = fx;
    y0 = fy;
    util_math_go_line_dist(&x0, &y0, tx, ty, (d0 * dist) / 100);
    x1 = x0;
    y1 = y0;
    util_math_go_line_dist(&x1, &y1, tx, ty, (d1 * dist) / 100);
    if ((v24 == 1) || (v24 == 3) || (v24 == 5)) {
        ui_draw_line1(x0, y0, x1, y1, ctbl[0], ui_scale);
    } else {
        const uint8_t *c;
        uint8_t revctbl[7];
        if (x0 <= x1) {
            for (int i = 0; i < 7; ++i) {
                revctbl[6 - i] = ctbl[i];
            }
            c = revctbl;
        } else {
            c = ctbl;
        }
        ui_draw_line_ctbl(x0, y0, x1, y1, c, 7, 1/*ctblpos*/, ui_scale);
    }
}

static void ui_battle_draw_beam_attack_do1(const struct battle_s *bt, int *fx, int *fy, int tx, int ty, weapon_t wpnt, int attacker_i, int v16, uint8_t btype)
{
    const struct battle_item_s *b = &(bt->item[attacker_i]);
    const struct shiptech_weap_s *w = &(tbl_shiptech_weap[wpnt]);
    int frames = 2, xo = 0, yo = 0;
    for (int f = 0; f < frames; ++f) {
        ui_delay_prepare();
        ui_battle_draw_arena(bt, attacker_i, 1);
        ui_battle_draw_bottom(bt);
        if (v16 > 1) {
            ui_battle_draw_beam_line(fx[0], fy[0], tx, ty, 0, 100 / frames + (100 / frames) * f, w->v24, w->dtbl);
            ui_battle_draw_beam_line(fx[1], fy[1], tx, ty, 0, 100 / frames + (100 / frames) * f, w->v24, w->dtbl);
        }
        /*53827*/
        if (v16 != 2) {
            ui_battle_draw_beam_line(fx[2], fy[2], tx, ty, 0, 100 / frames + (100 / frames) * f, w->v24, w->dtbl);
        }
        /*53874*/
        ui_battle_draw_item(bt, attacker_i, b->sx * 32, b->sy * 24);
        uiobj_finish_frame();
        ui_delay_ticks_or_click(1);
    }
    /*538c4*/
    if (((btype == 1) || (w->numfire > 1)) && !(bt->s[SIDE_L].flag_auto && bt->s[SIDE_R].flag_auto)) {
        for (int f = 0; f < 7; ++f) {
            ui_delay_prepare();
            if ((f & 1) == 1) {
                ui_battle_draw_arena(bt, attacker_i, 1);
            }
            ui_battle_draw_bottom(bt);
            if (w->numfire > 1) {
                xo = rnd_1_n(3, &ui_data.seed) - 2;
                yo = rnd_1_n(3, &ui_data.seed) - 2;
            } else {
                xo = 0;
                yo = 0;
            }
            if (btype == 1) {
                if (v16 > 1) {
                    ui_draw_line1(fx[0], fy[0], tx + xo, ty + yo, w->dtbl[f], ui_scale);
                    ui_draw_line1(fx[1], fy[1], tx + xo, ty + yo, w->dtbl[f], ui_scale);
                }
                if (v16 != 2) {
                    ui_draw_line1(fx[2], fy[2], tx + xo, ty + yo, w->dtbl[f], ui_scale);
                }
            }
            if (btype == 0) {
                if (v16 > 1) {
                    ui_draw_line_ctbl(fx[0], fy[0], tx + xo, ty + yo, w->dtbl, 7, 0, ui_scale);
                    ui_draw_line_ctbl(fx[1], fy[1], tx + xo, ty + yo, w->dtbl, 7, 0, ui_scale);
                }
                if (v16 != 2) {
                    ui_draw_line_ctbl(fx[2], fy[2], tx + xo, ty + yo, w->dtbl, 7, 0, ui_scale);
                }
            }
            ui_battle_draw_item(bt, attacker_i, b->sx * 32, b->sy * 24);
            uiobj_finish_frame();
            ui_delay_ticks_or_click(1);
        }
    }
    if (bt->s[SIDE_L].flag_auto && bt->s[SIDE_R].flag_auto) {
        return;
    }
    frames = 4;
    for (int f = 0; f < frames; ++f) {
        ui_delay_prepare();
        ui_battle_draw_arena(bt, attacker_i, 0);
        ui_battle_draw_bottom(bt);
        if (v16 > 1) {
            ui_battle_draw_beam_line(fx[0], fy[0], tx + xo, ty + yo, f * 25 + 25, 100, w->v24, w->dtbl);
            ui_battle_draw_beam_line(fx[1], fy[1], tx + xo, ty + yo, f * 25 + 25, 100, w->v24, w->dtbl);
        }
        if (v16 != 2) {
            ui_battle_draw_beam_line(fx[2], fy[2], tx + xo, ty + yo, f * 25 + 25, 100, w->v24, w->dtbl);
        }
        ui_battle_draw_item(bt, attacker_i, b->sx * 32, b->sy * 24);
        uiobj_finish_frame();
        ui_delay_ticks_or_click(1);
    }
}

static void ui_battle_draw_beam_attack_do2(const struct battle_s *bt, int *fx, int *fy, int tx, int ty, weapon_t wpnt, int attacker_i, int target_i, int v16)
{
    /*btype = 5*/
    const struct battle_item_s *b = &(bt->item[attacker_i]);
    const struct shiptech_weap_s *w = &(tbl_shiptech_weap[wpnt]);
    int y_ns, x_ns, frames = 4;
    y_ns = game_battle_get_xy_notsame(bt, attacker_i, target_i, &x_ns);
    for (int f = 0; f < frames; ++f) {
        ui_delay_prepare();
        ui_battle_draw_arena(bt, attacker_i, 1);
        ui_battle_draw_bottom(bt);
        if (v16 > 1) {
            ui_battle_draw_beam_line(fx[0], fy[0], tx, ty, 0, f * 25 + 25, w->v24, w->dtbl);
            ui_battle_draw_beam_line(fx[0], fy[0], tx - y_ns, ty - x_ns, 0, f * 25 + 25, w->v24, w->dtbl);
            ui_battle_draw_beam_line(fx[0], fy[0], tx + y_ns, ty + x_ns, 0, f * 25 + 25, w->v24, w->dtbl);
            ui_battle_draw_beam_line(fx[1], fy[1], tx, ty, 0, f * 25 + 25, w->v24, w->dtbl);
            ui_battle_draw_beam_line(fx[1], fy[1], tx - y_ns, ty - x_ns, 0, f * 25 + 25, w->v24, w->dtbl);
            ui_battle_draw_beam_line(fx[1], fy[1], tx + y_ns, ty + x_ns, 0, f * 25 + 25, w->v24, w->dtbl);
            /*if (btype == 5) always true*/
            ui_battle_draw_beam_line(fx[0], fy[0], tx - y_ns * 2, ty - x_ns * 2, 0, f * 25 + 25, w->v24, w->dtbl);
            ui_battle_draw_beam_line(fx[0], fy[0], tx + y_ns * 2, ty + x_ns * 2, 0, f * 25 + 25, w->v24, w->dtbl);
            ui_battle_draw_beam_line(fx[1], fy[1], tx - y_ns * 2, ty - x_ns * 2, 0, f * 25 + 25, w->v24, w->dtbl);
            ui_battle_draw_beam_line(fx[1], fy[1], tx + y_ns * 2, ty + x_ns * 2, 0, f * 25 + 25, w->v24, w->dtbl);
            if ((y_ns == 1) && (x_ns == 1)) {
                ui_battle_draw_beam_line(fx[0], fy[0], tx, ty - x_ns, 0, f * 25 + 25, w->v24, w->dtbl);
                ui_battle_draw_beam_line(fx[0], fy[0], tx - x_ns, ty, 0, f * 25 + 25, w->v24, w->dtbl);
                ui_battle_draw_beam_line(fx[1], fy[1], tx, ty - x_ns, 0, f * 25 + 25, w->v24, w->dtbl);
                ui_battle_draw_beam_line(fx[1], fy[1], tx - x_ns, ty, 0, f * 25 + 25, w->v24, w->dtbl);
            }
        }
        /*5435b*/
        if (v16 != 2) {
            ui_battle_draw_beam_line(fx[2], fy[2], tx, ty, 0, f * 25 + 25, w->v24, w->dtbl);
            ui_battle_draw_beam_line(fx[2], fy[2], tx - y_ns, ty - x_ns, 0, f * 25 + 25, w->v24, w->dtbl);
            ui_battle_draw_beam_line(fx[2], fy[2], tx + y_ns, ty + x_ns, 0, f * 25 + 25, w->v24, w->dtbl);
            /*if (btype == 5) always true*/
            ui_battle_draw_beam_line(fx[2], fy[2], tx - y_ns * 2, ty - x_ns * 2, 0, f * 25 + 25, w->v24, w->dtbl);
            ui_battle_draw_beam_line(fx[2], fy[2], tx + y_ns * 2, ty + x_ns * 2, 0, f * 25 + 25, w->v24, w->dtbl);
            if ((y_ns == 1) && (x_ns == 1)) {
                ui_battle_draw_beam_line(fx[2], fy[2], tx, ty - x_ns, 0, f * 25 + 25, w->v24, w->dtbl);
                ui_battle_draw_beam_line(fx[2], fy[2], tx - x_ns, ty, 0, f * 25 + 25, w->v24, w->dtbl);
            }
        }
        /*5450f*/
        ui_battle_draw_item(bt, attacker_i, b->sx * 32, b->sy * 24);
        uiobj_finish_frame();
        ui_delay_ticks_or_click(1);
    }
    /*5455f*/
    for (int f = 0; f < 7; ++f) {
        ui_delay_prepare();
        if ((w->v24 == 2) || (w->v24 == 4)) {
            if (v16 > 1) {
                ui_draw_line_ctbl(fx[0], fy[0], tx, ty, w->dtbl, 7, 6 - f, ui_scale);
                ui_draw_line_ctbl(fx[0], fy[0], tx - y_ns, ty - x_ns, w->dtbl, 7, 6 - f, ui_scale);
                ui_draw_line_ctbl(fx[0], fy[0], tx + y_ns, ty + x_ns, w->dtbl, 7, 6 - f, ui_scale);
                ui_draw_line_ctbl(fx[1], fy[1], tx, ty, w->dtbl, 7, 6 - f, ui_scale);
                ui_draw_line_ctbl(fx[1], fy[1], tx - y_ns, ty - x_ns, w->dtbl, 7, 6 - f, ui_scale);
                ui_draw_line_ctbl(fx[1], fy[1], tx + y_ns, ty + x_ns, w->dtbl, 7, 6 - f, ui_scale);
                /*if (btype == 5) always true*/
                ui_draw_line_ctbl(fx[0], fy[0], tx - y_ns * 2, ty - x_ns * 2, w->dtbl, 7, 6 - f, ui_scale);
                ui_draw_line_ctbl(fx[0], fy[0], tx + y_ns * 2, ty + x_ns * 2, w->dtbl, 7, 6 - f, ui_scale);
                ui_draw_line_ctbl(fx[1], fy[1], tx - y_ns * 2, ty - x_ns * 2, w->dtbl, 7, 6 - f, ui_scale);
                ui_draw_line_ctbl(fx[1], fy[1], tx + y_ns * 2, ty + x_ns * 2, w->dtbl, 7, 6 - f, ui_scale);
                if ((y_ns == 1) && (x_ns == 1)) {
                    ui_draw_line_ctbl(fx[0], fy[0], tx, ty - x_ns, w->dtbl, 7, 6 - f, ui_scale);
                    ui_draw_line_ctbl(fx[0], fy[0], tx - x_ns, ty, w->dtbl, 7, 6 - f, ui_scale);
                    ui_draw_line_ctbl(fx[1], fy[1], tx, ty - x_ns, w->dtbl, 7, 6 - f, ui_scale);
                    ui_draw_line_ctbl(fx[1], fy[1], tx - x_ns, ty, w->dtbl, 7, 6 - f, ui_scale);
                }
            }
            /*54879*/
            if (v16 != 2) {
                ui_draw_line_ctbl(fx[2], fy[2], tx, ty, w->dtbl, 7, 6 - f, ui_scale);
                ui_draw_line_ctbl(fx[2], fy[2], tx - y_ns, ty - x_ns, w->dtbl, 7, 6 - f, ui_scale);
                ui_draw_line_ctbl(fx[2], fy[2], tx + y_ns, ty + x_ns, w->dtbl, 7, 6 - f, ui_scale);
                /*if (btype == 5) always true*/
                ui_draw_line_ctbl(fx[2], fy[2], tx - y_ns * 2, ty - x_ns * 2, w->dtbl, 7, 6 - f, ui_scale);
                ui_draw_line_ctbl(fx[2], fy[2], tx + y_ns * 2, ty + x_ns * 2, w->dtbl, 7, 6 - f, ui_scale);
                if ((y_ns == 1) && (x_ns == 1)) {
                    ui_draw_line_ctbl(fx[2], fy[2], tx, ty - x_ns, w->dtbl, 7, 6 - f, ui_scale);
                    ui_draw_line_ctbl(fx[2], fy[2], tx - x_ns, ty, w->dtbl, 7, 6 - f, ui_scale);
                }
            }
        } else {
            /*549fc*/
            uint8_t c = w->dtbl[0];
            if (v16 > 1) {
                ui_draw_line1(fx[0], fy[0], tx, ty, c, ui_scale);
                ui_draw_line1(fx[0], fy[0], tx - y_ns, ty - x_ns, c, ui_scale);
                ui_draw_line1(fx[0], fy[0], tx + y_ns, ty + x_ns, c, ui_scale);
                ui_draw_line1(fx[1], fy[1], tx, ty, c, ui_scale);
                ui_draw_line1(fx[1], fy[1], tx - y_ns, ty - x_ns, c, ui_scale);
                ui_draw_line1(fx[1], fy[1], tx + y_ns, ty + x_ns, c, ui_scale);
                /*if (btype == 5) always true*/
                ui_draw_line1(fx[0], fy[0], tx - y_ns * 2, ty - x_ns * 2, c, ui_scale);
                ui_draw_line1(fx[0], fy[0], tx + y_ns * 2, ty + x_ns * 2, c, ui_scale);
                ui_draw_line1(fx[1], fy[1], tx - y_ns * 2, ty - x_ns * 2, c, ui_scale);
                ui_draw_line1(fx[1], fy[1], tx + y_ns * 2, ty + x_ns * 2, c, ui_scale);
                if ((y_ns == 1) && (x_ns == 1)) {
                    ui_draw_line1(fx[0], fy[0], tx, ty - x_ns, c, ui_scale);
                    ui_draw_line1(fx[0], fy[0], tx - x_ns, ty, c, ui_scale);
                    ui_draw_line1(fx[1], fy[1], tx, ty - x_ns, c, ui_scale);
                    ui_draw_line1(fx[1], fy[1], tx - x_ns, ty, c, ui_scale);
                }
            }
            /*54cb2*/
            if (v16 != 2) {
                ui_draw_line1(fx[2], fy[2], tx, ty, c, ui_scale);
                ui_draw_line1(fx[2], fy[2], tx - y_ns, ty - x_ns, c, ui_scale);
                ui_draw_line1(fx[2], fy[2], tx + y_ns, ty + x_ns, c, ui_scale);
                /*if (btype == 5) always true*/
                ui_draw_line1(fx[2], fy[2], tx - y_ns * 2, ty - x_ns * 2, c, ui_scale);
                ui_draw_line1(fx[2], fy[2], tx + y_ns * 2, ty + x_ns * 2, c, ui_scale);
                if ((y_ns == 1) && (x_ns == 1)) {
                    ui_draw_line1(fx[2], fy[2], tx, ty - x_ns, c, ui_scale);
                    ui_draw_line1(fx[2], fy[2], tx - x_ns, ty, c, ui_scale);
                }
            }
        }
        /*54e1d*/
        ui_battle_draw_item(bt, attacker_i, b->sx * 32, b->sy * 24);
        uiobj_finish_frame();
        ui_delay_ticks_or_click(1);
    }
    /*54e6b*/
    if (bt->s[SIDE_L].flag_auto && bt->s[SIDE_R].flag_auto) {
        return;
    }
    for (int f = 0; f < frames; ++f) {
        ui_delay_prepare();
        ui_battle_draw_arena(bt, attacker_i, 0);
        ui_battle_draw_bottom(bt);
        if (v16 > 1) {
            ui_battle_draw_beam_line(fx[0], fy[0], tx, ty, f * 25 + 25, 100, w->v24, w->dtbl);
            ui_battle_draw_beam_line(fx[0], fy[0], tx - y_ns, ty - x_ns, f * 25 + 25, 100, w->v24, w->dtbl);
            ui_battle_draw_beam_line(fx[0], fy[0], tx + y_ns, ty + x_ns, f * 25 + 25, 100, w->v24, w->dtbl);
            ui_battle_draw_beam_line(fx[1], fy[1], tx, ty, f * 25 + 25, 100, w->v24, w->dtbl);
            ui_battle_draw_beam_line(fx[1], fy[1], tx - y_ns, ty - x_ns, f * 25 + 25, 100, w->v24, w->dtbl);
            ui_battle_draw_beam_line(fx[1], fy[1], tx + y_ns, ty + x_ns, f * 25 + 25, 100, w->v24, w->dtbl);
            /*if (btype == 5) always true*/
            ui_battle_draw_beam_line(fx[0], fy[0], tx - y_ns * 2, ty - x_ns * 2, f * 25 + 25, 100, w->v24, w->dtbl);
            ui_battle_draw_beam_line(fx[0], fy[0], tx + y_ns * 2, ty + x_ns * 2, f * 25 + 25, 100, w->v24, w->dtbl);
            ui_battle_draw_beam_line(fx[1], fy[1], tx - y_ns * 2, ty - x_ns * 2, f * 25 + 25, 100, w->v24, w->dtbl);
            ui_battle_draw_beam_line(fx[1], fy[1], tx + y_ns * 2, ty + x_ns * 2, f * 25 + 25, 100, w->v24, w->dtbl);
            if ((y_ns == 1) && (x_ns == 1)) {
                ui_battle_draw_beam_line(fx[0], fy[0], tx, ty - x_ns, f * 25 + 25, 100, w->v24, w->dtbl);
                ui_battle_draw_beam_line(fx[0], fy[0], tx - x_ns, ty, f * 25 + 25, 100, w->v24, w->dtbl);
                ui_battle_draw_beam_line(fx[1], fy[1], tx, ty - x_ns, f * 25 + 25, 100, w->v24, w->dtbl);
                ui_battle_draw_beam_line(fx[1], fy[1], tx - x_ns, ty, f * 25 + 25, 100, w->v24, w->dtbl);
            }
        }
        if (v16 != 2) {
            ui_battle_draw_beam_line(fx[2], fy[2], tx, ty, f * 25 + 25, 100, w->v24, w->dtbl);
            ui_battle_draw_beam_line(fx[2], fy[2], tx - y_ns, ty - x_ns, f * 25 + 25, 100, w->v24, w->dtbl);
            ui_battle_draw_beam_line(fx[2], fy[2], tx + y_ns, ty + x_ns, f * 25 + 25, 100, w->v24, w->dtbl);
            /*if (btype == 5) always true*/
            ui_battle_draw_beam_line(fx[2], fy[2], tx - y_ns * 2, ty - x_ns * 2, f * 25 + 25, 100, w->v24, w->dtbl);
            ui_battle_draw_beam_line(fx[2], fy[2], tx + y_ns * 2, ty + x_ns * 2, f * 25 + 25, 100, w->v24, w->dtbl);
            if ((y_ns == 1) && (x_ns == 1)) {
                ui_battle_draw_beam_line(fx[2], fy[2], tx, ty - x_ns, f * 25 + 25, 100, w->v24, w->dtbl);
                ui_battle_draw_beam_line(fx[2], fy[2], tx - x_ns, ty, f * 25 + 25, 100, w->v24, w->dtbl);
            }
        }
        uiobj_finish_frame();
        ui_delay_ticks_or_click(1);
    }
}

static void ui_battle_draw_stasis_sub1(const struct battle_s *bt, int target_i, int frame)
{
    const struct battle_item_s *b = &(bt->item[target_i]);
    int x, y;
    x = b->sx * 32;
    y = b->sy * 24;
    lbxgfx_set_frame_0(b->gfx);
    gfx_aux_draw_frame_to(b->gfx, &ui_data.aux.ship_p1);
    if (b->side == SIDE_R) {
        gfx_aux_flipx(&ui_data.aux.ship_p1);
    }
    gfx_aux_setup_wh(&ui_data.aux.btemp, 34, 26);
    gfx_aux_setup_wh(&ui_data.aux.ship_overlay, 34, 26);
    gfx_aux_overlay(1, 1, &ui_data.aux.ship_overlay, &ui_data.aux.ship_p1);
    gfx_aux_color_non0(&ui_data.aux.ship_p1, 0xc4);
    gfx_aux_overlay(0, 0, &ui_data.aux.btemp, &ui_data.aux.ship_p1);
    gfx_aux_overlay(0, 1, &ui_data.aux.btemp, &ui_data.aux.ship_p1);
    gfx_aux_overlay(0, 2, &ui_data.aux.btemp, &ui_data.aux.ship_p1);
    gfx_aux_overlay(1, 0, &ui_data.aux.btemp, &ui_data.aux.ship_p1);
    gfx_aux_overlay(1, 2, &ui_data.aux.btemp, &ui_data.aux.ship_p1);
    /*gfx_aux_overlay(1, 0, &ui_data.aux.btemp, &ui_data.aux.ship_p1); FIXME should be 2, 0 ? */
    gfx_aux_overlay(2, 1, &ui_data.aux.btemp, &ui_data.aux.ship_p1);
    gfx_aux_overlay(2, 2, &ui_data.aux.btemp, &ui_data.aux.ship_p1);
    lbxgfx_set_frame_0(b->gfx);
    gfx_aux_draw_frame_to(b->gfx, &ui_data.aux.ship_p1);
    if (b->side == SIDE_R) {
        gfx_aux_flipx(&ui_data.aux.ship_p1);
    }
    /*4c5fa*/
    {
        const uint8_t ctbl[5] = { 0x18, 0x16, 0x14, 0x13, 0x12 };
        gfx_aux_recolor_ctbl(&ui_data.aux.ship_p1, ctbl, 5);
    }
    gfx_aux_overlay(1, 1, &ui_data.aux.btemp, &ui_data.aux.ship_p1);
    lbxgfx_set_new_frame(ui_data.gfx.space.circle, frame - 5);
    gfx_aux_draw_frame_to(ui_data.gfx.space.circle, &ui_data.aux.ship_p1);
    gfx_aux_overlay_clear_unused(1, 1, &ui_data.aux.btemp, &ui_data.aux.ship_p1);
    gfx_aux_overlay(0, 0, &ui_data.aux.ship_overlay, &ui_data.aux.btemp);
    gfx_aux_draw_frame_from_limit(x, y, &ui_data.aux.ship_overlay, 0, 0, UI_VGA_W - 1, UI_VGA_H - 1, UI_SCREEN_W, ui_scale);
    if (b->num > 0) {
        lbxfont_set_temp_color(0);
        lbxfont_select_set_12_4(2, 0xd, 0, 0);
        if (b->side == SIDE_L) {
            lbxfont_print_num_right(x + 29, y + 18, b->num, UI_SCREEN_W, ui_scale);
        } else {
            lbxfont_print_num_normal(x + 3, y + 18, b->num, UI_SCREEN_W, ui_scale);
        }
    }
}

static void ui_battle_draw_colony_destroyed_cb(void *vptr)
{
    ui_draw_copy_buf();
    ui_draw_textbox_2str("", game_str_bt_coldest, 74, ui_scale);
}

/* -------------------------------------------------------------------------- */

ui_battle_autoresolve_t ui_battle_init(struct battle_s *bt)
{
    static struct ui_battle_data_s ctx; /* HACK */
    memset(&ctx, 0, sizeof(ctx));
    bt->uictx = &ctx;
    ctx.bt = bt;
    ctx.show_switch = (bt->g->gaux->local_players > 1);
    if (ctx.show_switch) {
        ui_switch_2(bt->g, bt->s[SIDE_L].party, bt->s[SIDE_R].party);
    }
    ui_sound_play_music(8);
    {
        ui_battle_autoresolve_t ar = ui_battle_pre(bt->g, bt, ctx.show_switch, SIDE_NONE);
        if (ar != UI_BATTLE_AUTORESOLVE_OFF) {
            return ar;
        }
    }
    /* ui_battle_do_sub1: */
    uiobj_set_callback_and_delay(ui_battle_draw_cb, bt, 2);
    ctx.cursor[0].cursor_i = 1;
    ctx.cursor[0].x1 = UI_SCREEN_W - 1;
    ctx.cursor[0].y1 = UI_SCREEN_H - 1;
    ui_battle_clear_ois(&ctx);
    ctx.oi_ai = UIOBJI_INVALID;

    /* from ui_battle_do: */
    ctx.gfx_bg = ui_data.gfx.space.bg[bt->g->planet[bt->planet_i].battlebg];
    ui_battle_draw_arena(bt, 0, 0);
    ui_battle_draw_bottom_no_ois(bt);
    {
        const planet_t *p = &(bt->g->planet[bt->planet_i]);
        ui_battle_transition_to(p->x, p->y, 10 * 3); /* FIXME BUG? x,y not in screen coords */
    }
    return UI_BATTLE_AUTORESOLVE_OFF;
}

void ui_battle_shutdown(struct battle_s *bt, bool colony_destroyed, int winner)
{
    struct ui_battle_data_s *d = bt->uictx;
    ui_cursor_setup_area(1, &ui_cursor_area_tbl[0]);
    uiobj_set_help_id(-1);
    if (colony_destroyed && (!bt->autoresolve)) {
        bool flag_done = false;
        uiobj_set_callback_and_delay(ui_battle_draw_colony_destroyed_cb, 0, 1);
        ui_draw_copy_buf();
        uiobj_finish_frame();
        uiobj_table_clear();
        uiobj_add_mousearea_all(MOO_KEY_UNKNOWN);
        while (!flag_done) {
            ui_delay_prepare();
            if (uiobj_handle_input_cond() != 0) {
                flag_done = true;
            }
            ui_battle_draw_colony_destroyed_cb(0);
            ui_draw_finish();
            ui_delay_ticks_or_click(3);
        }
    }
    /*128c7*/
    uiobj_unset_callback();
    uiobj_table_clear();
    if (bt->autoresolve) {
        ui_battle_pre(bt->g, bt, d->show_switch, winner);
    }
    ui_sound_stop_music();
    if (!bt->autoresolve) {
        ui_palette_fadeout_a_f_1();
        ui_draw_finish_mode = 2;
    }
}

void ui_battle_draw_planetinfo(const struct battle_s *bt, bool side_r)
{
    const struct battle_item_s *b = &(bt->item[0/*planet*/]);
    int x = 100, y = 50;
    uiobj_unset_callback();
    uiobj_table_clear();
    ui_battle_draw_arena(bt, 0, 0);
    ui_battle_draw_bottom_no_ois(bt);
    ui_draw_filled_rect(x, y, x + 131, y + 37, 0x3a, ui_scale);
    ui_draw_filled_rect(x + 85, y + 4, x + 131, y + 37, 0, ui_scale);
    lbxgfx_draw_frame(x, y, ui_data.gfx.space.vp2_top, UI_SCREEN_W, ui_scale);
    lbxgfx_draw_frame(x + 90, y + 9, b->gfx, UI_SCREEN_W, ui_scale);
    lbxfont_select(1, 0xa, 0, 0);
    lbxfont_print_str_center(x + 44, y + 7, b->name, UI_SCREEN_W, ui_scale);
    lbxfont_select(0, 0xa, 0, 0);
    lbxfont_print_str_normal(x + 7, y + 21, game_str_bt_pop, UI_SCREEN_W, ui_scale);
    lbxfont_print_str_normal(x + 7, y + 30, game_str_bt_ind, UI_SCREEN_W, ui_scale);
    lbxfont_print_str_normal(x + 60, y + 21, ":", UI_SCREEN_W, ui_scale);
    lbxfont_print_num_right(x + 80, y + 21, bt->pop, UI_SCREEN_W, ui_scale);
    lbxfont_print_str_normal(x + 60, y + 30, ":", UI_SCREEN_W, ui_scale);
    lbxfont_print_num_right(x + 80, y + 30, bt->fact, UI_SCREEN_W, ui_scale);
    if (bt->s[bt->item[bt->cur_item].side].flag_have_scan || (side_r == (b->side == SIDE_R))) {
        int y1;
        ui_draw_filled_rect(x, y + 39, x + 131, y + 79, 0x3a, ui_scale);
        lbxgfx_draw_frame(x, y + 38, ui_data.gfx.space.vp2_data, UI_SCREEN_W, ui_scale);
        lbxfont_print_str_normal(x + 68, y + 42, game_str_bt_bases, UI_SCREEN_W, ui_scale);
        if (bt->have_subspace_int) {
            ui_draw_filled_rect(x, y + 79, x + 131, y + 89, 0x3a, ui_scale);
            lbxgfx_draw_frame(x, y + 77, ui_data.gfx.space.vp2_line, UI_SCREEN_W, ui_scale);
            lbxfont_select(2, 6, 0, 0);
            lbxfont_print_str_normal(x + 10, y + 80, game_str_bt_subint, UI_SCREEN_W, ui_scale);
            y1 = y + 86;
        } else {
            y1 = y + 77;
        }
        lbxgfx_draw_frame(x, y1, ui_data.gfx.space.vp2_bottom, UI_SCREEN_W, ui_scale);
        lbxfont_select(2, 6, 0, 0);
        {
            char buf[80];
            const struct shiptech_weap_s *w = &(tbl_shiptech_weap[b->wpn[0].t]);
            if ((!bt->s[b->side].flag_base_missile) && (w->nummiss == 1)) {
                w = &(tbl_shiptech_weap[b->wpn[1].t]);
            }
            lib_sprintf(buf, sizeof(buf), "3 %s %s", *w->nameptr, game_str_bt_launch);
            lbxfont_print_str_normal(x + 10, y + 71, buf, UI_SCREEN_W, ui_scale);
        }
        lbxfont_select(2, 0xb, 0, 0);
        lbxfont_print_num_right(x + 46, y + 51, b->defense, UI_SCREEN_W, ui_scale);
        lbxfont_print_num_right(x + 46, y + 61, b->misdefense, UI_SCREEN_W, ui_scale);
        lbxfont_print_num_right(x + 86, y + 51, b->complevel, UI_SCREEN_W, ui_scale);
        lbxfont_print_num_right(x + 86, y + 61, b->hp1, UI_SCREEN_W, ui_scale);
        lbxfont_print_num_right(x + 123, y + 51, b->hploss, UI_SCREEN_W, ui_scale);
        lbxfont_print_num_right(x + 124, y + 61, b->absorb, UI_SCREEN_W, ui_scale);
    } else {
        lbxgfx_draw_frame(x, y + 37, ui_data.gfx.space.vp2_bottom, UI_SCREEN_W, ui_scale);
    }
    uiobj_finish_frame();
    ui_draw_copy_buf();
    uiobj_input_wait();
    uiobj_set_callback_and_delay(ui_battle_draw_cb, (void *)bt, 2);
}

void ui_battle_draw_scan(const struct battle_s *bt, bool side_r)
{
    struct ui_battle_data_s *d = bt->uictx;
    d->flag_scanning = true;
    d->scan_side = side_r ? SIDE_R : SIDE_L;
    uiobj_unset_callback();
    uiobj_table_clear();
    ui_cursor_setup_area(1, &(d->cursor[0]));
    uiobj_set_help_id(0x23);
    uiobj_set_callback_and_delay(ui_battle_draw_scan_cb, (void *)bt, 2);
    uiobj_input_wait();
    uiobj_unset_callback();
    uiobj_set_help_id(7);
    uiobj_set_callback_and_delay(ui_battle_draw_cb, (void *)bt, 2);
    ui_cursor_setup_area(1 + BATTLE_AREA_W * BATTLE_AREA_H, &(d->cursor[0]));
    d->flag_scanning = false;
}

void ui_battle_draw_item(const struct battle_s *bt, int itemi, int x, int y)
{
    const struct ui_battle_data_s *d = bt->uictx;
    const struct battle_item_s *b;
    b = &(bt->item[itemi]);
    if (b->selected && (!bt->s[b->side].flag_auto)) {
        if (b->selected != 2/*moving*/) {
            uint8_t *gfx;
            if ((b->sx != (BATTLE_AREA_W - 1)) && (!d->flag_scanning)) {
                gfx = (b->sy == (BATTLE_AREA_H - 1)) ? ui_data.gfx.space.box_y : ui_data.gfx.space.box;
            } else {
                gfx = (b->sy == (BATTLE_AREA_H - 1)) ? ui_data.gfx.space.box_xy : ui_data.gfx.space.box_x;
            }
            lbxgfx_set_frame_0(gfx);
            for (int i = 0; i <= d->frame_ship; ++i) {
                lbxgfx_draw_frame(x, y, gfx, UI_SCREEN_W, ui_scale);
            }
        }
        if (b->side != SIDE_NONE) {
            lbxgfx_set_frame_0(b->gfx);
            for (int i = 0; i <= d->frame_ship; ++i) {
                gfx_aux_draw_frame_to(b->gfx, &ui_data.aux.ship_p1);
            }
        }
    } else {
        /* FIXME no SIDE_NONE test? */
        lbxgfx_set_frame_0(b->gfx);
        gfx_aux_draw_frame_to(b->gfx, &ui_data.aux.ship_p1);
    }
    if (b->side == SIDE_R) {
        gfx_aux_flipx(&ui_data.aux.ship_p1);
    }
    gfx_aux_setup_wh(&ui_data.aux.btemp, 34, 26);
    if (b->cloak == 1) {
        gfx_aux_draw_cloak(&ui_data.aux.ship_p1, 30, rnd_1_n(1000, &ui_data.seed));
    }
    if (b->unman > 0) {
        int v = (b->unman * 100) / b->man;
        uint8_t *gfx;
        if (v > 75) {
            gfx = ui_data.gfx.space.warp4;
        } else if (v > 50) {
            gfx = ui_data.gfx.space.warp3;
        } else if (v > 25) {
            gfx = ui_data.gfx.space.warp2;
        } else {
            gfx = ui_data.gfx.space.warp1;
        }
        gfx_aux_draw_frame_to(gfx, &ui_data.aux.ship_overlay);
        gfx_aux_overlay_clear_unused(0, 0, &ui_data.aux.ship_overlay, &ui_data.aux.ship_p1);
        gfx_aux_overlay(0, 0, &ui_data.aux.ship_p1, &ui_data.aux.ship_overlay);
    }
    /*4fcf1*/
    if (b->stasisby > 0) {
        const uint8_t ctbl[5] = { 0x18, 0x16, 0x14, 0x13, 0x12 };
        gfx_aux_copy(&ui_data.aux.ship_overlay, &ui_data.aux.ship_p1);
        gfx_aux_recolor_ctbl(&ui_data.aux.ship_p1, ctbl, 5);
        gfx_aux_color_non0(&ui_data.aux.ship_overlay, 0xc4);
        gfx_aux_overlay(0, 0, &ui_data.aux.btemp, &ui_data.aux.ship_overlay);
        gfx_aux_overlay(0, 1, &ui_data.aux.btemp, &ui_data.aux.ship_overlay);
        gfx_aux_overlay(0, 2, &ui_data.aux.btemp, &ui_data.aux.ship_overlay);
        gfx_aux_overlay(1, 0, &ui_data.aux.btemp, &ui_data.aux.ship_overlay);
        gfx_aux_overlay(1, 2, &ui_data.aux.btemp, &ui_data.aux.ship_overlay);
        /* BUG? 1,0 twice, no 2, 0 */
        gfx_aux_overlay(2, 1, &ui_data.aux.btemp, &ui_data.aux.ship_overlay);
        gfx_aux_overlay(2, 2, &ui_data.aux.btemp, &ui_data.aux.ship_overlay);
    }
    /*4fdf3*/
    gfx_aux_overlay(1, 1, &ui_data.aux.btemp, &ui_data.aux.ship_p1);
    {
        int xa = x, ya = y;
        if (b->sbmask & (1 << SHIP_SPECIAL_BOOL_DISP)) {
            int xoff, yoff;
            xoff = rnd_0_nm1(5, &ui_data.seed) - 2;
            yoff = rnd_0_nm1(5, &ui_data.seed) - 2;
            xa += xoff;
            ya += yoff;
        }
        gfx_aux_draw_frame_from_limit(xa, ya, &ui_data.aux.btemp, 0, 0, UI_VGA_W - 1, UI_VGA_H - 1, UI_SCREEN_W, ui_scale);
    }
    if (b->num > 0) {
        lbxfont_set_temp_color(0);
        lbxfont_select_set_12_4(2, 0xd, 0, 0);
        if (b->side == SIDE_L) {
            lbxfont_print_num_right(x + 29, y + 18, b->num, UI_SCREEN_W, ui_scale);
        } else {
            lbxfont_print_num_normal(x + 3, y + 18, b->num, UI_SCREEN_W, ui_scale);
        }
    }
}

void ui_battle_draw_arena(const struct battle_s *bt, int itemi, int dmode)
{
    struct ui_battle_data_s *d = bt->uictx;
    d->frame_ship = (d->frame_ship + 1) % 5;
    d->frame_missile = (d->frame_missile + 1) % 4;
    ui_draw_erase_buf();
    lbxgfx_draw_frame(0, 0, d->gfx_bg, UI_SCREEN_W, ui_scale);
    lbxfont_set_temp_color(0);
    lbxfont_select_set_12_4(2, 0xd, 0, 0);
    if (ui_extra_enabled && ui_data.battle.show_grid) {
        for (int x = 0; x < 10; ++x) {
            ui_draw_line1(x * 32, 0, x * 32, 24 * 8 - 1, 4, ui_scale);
        }
        for (int y = 0; y < 8; ++y) {
            ui_draw_line1(0, y * 24, 32 * 10 - 1, y * 24, 4, ui_scale);
        }
    }
    for (int i = 0; i <= bt->items_num; ++i) {
        if ((i != itemi) || (dmode == 0/*normal*/)) {
            const struct battle_item_s *b;
            b = &(bt->item[i]);
            if (b->side != SIDE_NONE) {
                ui_battle_draw_item(bt, i, b->sx * 32, b->sy * 24);
            }
        }
    }
    for (int i = 0; i < bt->num_rocks; ++i) {
        const struct battle_rock_s *r;
        r = &(bt->rock[i]);
        lbxgfx_draw_frame(r->sx * 32, r->sy * 24, r->gfx, UI_SCREEN_W, ui_scale);
    }
    for (int i = 0; i < bt->num_missile; ++i) {
        const struct battle_missile_s *m = &(bt->missile[i]);
        int8_t target;
        target = m->target;
        if ((target != MISSILE_TARGET_NONE) && ((target != itemi) || (dmode != 2/*hide target missile*/))) {
            const struct battle_item_s *b;
            b = &(bt->item[target]);
            ui_battle_draw_missile(bt, i, m->x, m->y, b->sx * 32 + 16, b->sy * 24 + 12);
        }
    }
}

void ui_battle_draw_misshield(const struct battle_s *bt, int target_i, int target_x, int target_y, int missile_i)
{
    const struct battle_missile_s *m = &(bt->missile[missile_i]);
    int mx = m->x, my = m->y, target_x_hit, target_y_hit;
    const struct battle_item_s *b = &(bt->item[target_i]);
    {
        const struct firing_s *fr = &(bt->g->gaux->firing[b->look]);
        if (b->side == SIDE_R) {
            target_x_hit = target_x + 32 - fr->target_x;
        } else {
            target_x_hit = target_x + fr->target_x;
        }
        target_y_hit = target_y + fr->target_y;
    }
    if (b->misshield < 45) {
        const uint8_t colortbl[8] = { 0x1d, 0x42, 0x41, 0, 0, 0, 0x46, 0 };
        for (int f = 0; f < 5; ++f) {
            ui_delay_prepare();
            if (f == 3) {
                ui_sound_play_sfx(2);
            }
            ui_battle_draw_arena(bt, target_i, 1);
            ui_battle_draw_bottom(bt);
            ui_battle_draw_item(bt, target_i, target_x, target_y);
            ui_draw_line_ctbl(target_x_hit, target_y_hit, mx, my, colortbl, 8, 0, ui_scale);
            if (f > 1) {
                gfx_aux_draw_frame_to(ui_data.gfx.space.explos[f - 2], &ui_data.aux.btemp);
                gfx_aux_scale(&ui_data.aux.btemp, 30, 30);
                gfx_aux_draw_frame_from(mx - 4, my - 4, &ui_data.aux.btemp, UI_SCREEN_W, ui_scale);
            }
            uiobj_finish_frame();
            ui_delay_ticks_or_click(2);
        }
    } else {
        uint8_t color = (b->misshield >= 90) ? 0xd6 : 0x46;
        for (int f = 0; f < 3; ++f) {
            ui_delay_prepare();
            ui_battle_draw_arena(bt, target_i, 1);
            ui_battle_draw_bottom(bt);
            gfx_aux_draw_frame_to(b->gfx, &ui_data.aux.ship_overlay);
            if (b->side == SIDE_R) {
                gfx_aux_flipx(&ui_data.aux.ship_overlay);
            }
            gfx_aux_setup_wh(&ui_data.aux.btemp, 38, 30);
            gfx_aux_copy(&ui_data.aux.ship_p1, &ui_data.aux.ship_overlay);
            gfx_aux_color_non0(&ui_data.aux.ship_p1, color);
            gfx_aux_overlay(3 - (f + 1), 3, &ui_data.aux.btemp, &ui_data.aux.ship_p1);
            gfx_aux_overlay(3, f + 4, &ui_data.aux.btemp, &ui_data.aux.ship_p1);
            gfx_aux_overlay(3, 3 - (f + 1), &ui_data.aux.btemp, &ui_data.aux.ship_p1);
            gfx_aux_overlay(f + 4, f + 4, &ui_data.aux.btemp, &ui_data.aux.ship_p1);
            gfx_aux_overlay(f + 4, 3 - (f + 1), &ui_data.aux.btemp, &ui_data.aux.ship_p1);
            gfx_aux_overlay(3 - (f + 1), f + 4, &ui_data.aux.btemp, &ui_data.aux.ship_p1);
            gfx_aux_overlay(3 - (f + 1), 3 - (f + 1), &ui_data.aux.btemp, &ui_data.aux.ship_p1);
            if (f > 0) {
                gfx_aux_copy(&ui_data.aux.ship_p1, &ui_data.aux.ship_overlay);
                gfx_aux_color_non0(&ui_data.aux.ship_p1, color - 4);
                gfx_aux_overlay(3 - f, 3, &ui_data.aux.btemp, &ui_data.aux.ship_p1);
                gfx_aux_overlay(3 + f, 3, &ui_data.aux.btemp, &ui_data.aux.ship_p1);
                gfx_aux_overlay(3, 3 + f, &ui_data.aux.btemp, &ui_data.aux.ship_p1);
                gfx_aux_overlay(3, 3 - f, &ui_data.aux.btemp, &ui_data.aux.ship_p1);
                gfx_aux_overlay(3 + f, 3 + f, &ui_data.aux.btemp, &ui_data.aux.ship_p1);
                gfx_aux_overlay(3 + f, 3 - f, &ui_data.aux.btemp, &ui_data.aux.ship_p1);
                gfx_aux_overlay(3 - f, 3 + f, &ui_data.aux.btemp, &ui_data.aux.ship_p1);
                gfx_aux_overlay(3 - f, 3 - f, &ui_data.aux.btemp, &ui_data.aux.ship_p1);
            }
            if (f == 2) {
                gfx_aux_copy(&ui_data.aux.ship_p1, &ui_data.aux.ship_overlay);
                gfx_aux_color_non0(&ui_data.aux.ship_p1, 1);
                gfx_aux_overlay(2, 3, &ui_data.aux.btemp, &ui_data.aux.ship_p1);
                gfx_aux_overlay(4, 3, &ui_data.aux.btemp, &ui_data.aux.ship_p1);
                gfx_aux_overlay(3, 4, &ui_data.aux.btemp, &ui_data.aux.ship_p1);
                gfx_aux_overlay(3, 2, &ui_data.aux.btemp, &ui_data.aux.ship_p1);
                gfx_aux_overlay(4, 4, &ui_data.aux.btemp, &ui_data.aux.ship_p1);
                gfx_aux_overlay(4, 2, &ui_data.aux.btemp, &ui_data.aux.ship_p1);
                gfx_aux_overlay(2, 4, &ui_data.aux.btemp, &ui_data.aux.ship_p1);
                gfx_aux_overlay(2, 2, &ui_data.aux.btemp, &ui_data.aux.ship_p1);
            }
            gfx_aux_overlay(3, 3, &ui_data.aux.btemp, &ui_data.aux.ship_overlay);
            gfx_aux_draw_frame_from_limit(target_x - 3, target_y - 3, &ui_data.aux.btemp, 0, 0, UI_VGA_W - 1, UI_VGA_H - 1, UI_SCREEN_W, ui_scale);
            uiobj_finish_frame();
            ui_delay_ticks_or_click(5);
        }
    }
    ui_sound_stop_sfx();
}

void ui_battle_draw_damage(const struct battle_s *bt, int target_i, int target_x, int target_y, uint32_t damage)
{
    const struct battle_item_s *b = &(bt->item[target_i]);
    int target_x_hit, target_y_hit, si, v4, ax, ay, scale;
    bool flag_quick;
    si = (b->hp1 > 0) ? b->hp1 * 3 : 1;
    if ((b->num <= 0) || (si < damage)) {
        v4 = si;
    } else {
        v4 = damage; /* & 0xffff */
    }
    if ((si / v4) >= 3) {
        v4 = si / 3 + 1;
    }
    scale = (v4 * 100) / si;
    ui_sound_play_sfx(((si / v4) < 3) ? 2 : 17);
    {
        const struct firing_s *fr = &(bt->g->gaux->firing[b->look]);
        if (b->side == SIDE_R) {
            target_x_hit = target_x + 32 - fr->target_x;
        } else {
            target_x_hit = target_x + fr->target_x;
        }
        target_y_hit = target_y + fr->target_y;
    }
    ax = target_x_hit - (v4 * 16) / si;
    ay = target_y_hit - (v4 * 15) / si;
    flag_quick = (bt->s[0].flag_auto && bt->s[1].flag_auto);
    for (int i = 0; i < (flag_quick ? 2 : 10); ++i) {
        int f;
        ui_delay_prepare();
        ui_battle_draw_arena(bt, target_i, 1);
        ui_battle_draw_bottom_no_ois(bt);
        ui_battle_draw_item(bt, target_i, target_x, target_y);
        f = flag_quick ? (i * 2 + 4) : i;
        gfx_aux_draw_frame_to(ui_data.gfx.space.explos[f], &ui_data.aux.btemp);
        gfx_aux_scale(&ui_data.aux.btemp, scale, scale);
        gfx_aux_draw_frame_from_limit(ax, ay, &ui_data.aux.btemp, 0, 0, UI_VGA_W - 1, UI_VGA_H - 1, UI_SCREEN_W, ui_scale);
        lbxfont_select(2, 0xd, 0, 0);
        lbxfont_print_num_center(target_x_hit, target_y_hit - 2, damage, UI_SCREEN_W, ui_scale);
        uiobj_finish_frame();
        ui_delay_ticks_or_click(1);
    }
}

void ui_battle_draw_explos_small(const struct battle_s *bt, int x, int y)
{
    gfx_aux_draw_frame_to(ui_data.gfx.space.explos[5], &ui_data.aux.btemp);
    gfx_aux_scale(&ui_data.aux.btemp, 30, 30);
    gfx_aux_draw_frame_from(x, y, &ui_data.aux.btemp, UI_SCREEN_W, ui_scale);
}

void ui_battle_draw_basic(const struct battle_s *bt)
{
    ui_battle_draw_arena(bt, 0, 0);
    ui_battle_draw_bottom(bt);
    uiobj_finish_frame();
}

void ui_battle_draw_basic_copy(const struct battle_s *bt)
{
    ui_battle_draw_basic(bt);
    ui_draw_copy_buf();
}

void ui_battle_draw_finish(const struct battle_s *bt)
{
    uiobj_finish_frame();
}

void ui_battle_draw_missile(const struct battle_s *bt, int missilei, int x, int y, int tx, int ty)
{
    const int8_t tbl_missile_off[MAX_VISIBLE_MISSILE][MAX_VISIBLE_MISSILE * 2/*x, y*/] = {
        { 0, 0 },
        { -2, 0, 2, 0 },
        { 0, -2, -3, 4, 3, 4 },
        { -2, -2, 2, -2, 5, 2, -5, 2 },
        { 0, -4, -3, 0, -6, 4, 6, 4, 3, 0 },
        { 0, -4, -3, 0, -6, 4, 3, 0, 6, 4, 0, 4 },
        { -2, -4, 2, -4, -5, 0, 5, 0, -8, 4, 0, 4, 8, 4 },
        { 0, 5, -9, 0, 6, 2, 9, 0, 0, -6, 3, -2, -6, 2, -3, -2 },
        { -2, -6, 2, -6, -5, -2, 5, -2, -8, 2, 0, 2, 8, 2, -4, 6, 4, 6 },
        { 0, -7, -3, -3, -6, 1, 3, -3, 6, 1, 0, 1, -3, 5, -9, 5, 3, 5, 9, 5 }
    };
    struct ui_battle_data_s *d = bt->uictx;
    const struct battle_missile_s *m = &(bt->missile[missilei]);
    const struct shiptech_weap_s *w = &(tbl_shiptech_weap[m->wpnt]);
    int extradiv = 0, angle, dir, nummissiles = m->nummissiles, axoff = 0, ayoff = 0;
    uint8_t *gfx;
    angle = util_math_calc_angle(tx - x, ty - y);
    if ((angle >= 337) || (angle < 22)) {
        dir = 2;
        angle = 0;
    } else if ((angle >= 22) && (angle < 67)) {
        dir = 3;
        angle = 45;
    } else if ((angle >= 67) && (angle < 112)) {
        dir = 4;
        angle = 90;
    } else if ((angle >= 112) && (angle < 157)) {
        dir = 5;
        angle = 135;
    } else if ((angle >= 157) && (angle < 202)) {
        dir = 6;
        angle = 180;
    } else if ((angle >= 202) && (angle < 247)) {
        dir = 7;
        angle = 225;
    } else if ((angle >= 247) && (angle < 292)) {
        dir = 0;
        angle = 270;
    } else /*if ((angle >= 292) && (angle < 337))*/ {
        dir = 1;
        angle = 315;
    }
    switch (w->misstype) {
        default:
        case 0:
            gfx = ui_data.gfx.missile.missiles[dir];
            break;
        case 1:
            gfx = ui_data.gfx.missile.antimatr[dir];
            break;
        case 2:
            gfx = ui_data.gfx.missile.hellfire[dir];
            extradiv = 2;
            nummissiles /= 4;
            break;
        case 3:
            gfx = ui_data.gfx.missile.proton[dir];
            extradiv = 2;
            nummissiles /= 4;
            break;
        case 4:
            gfx = ui_data.gfx.missile.plasmaqt[dir];
            lbxgfx_set_new_frame(gfx, d->frame_missile);
            gfx_aux_draw_frame_to(gfx, &ui_data.aux.btemp);
            gfx = 0;
            {
                int v;
                v = w->damagemax - (((w->v24 - m->fuel) * w->dtbl[0] + (w->dtbl[0] - m->speed))) / 2; /* FIXME check this calc */
                if (v < 0) {
                    v = 0;
                    /*m->target = -1; XXX moved to game */
                }
                v = (v * 100) / w->damagemax;
                gfx_aux_scale(&ui_data.aux.btemp, v, v);
            }
            extradiv = 10000;
            axoff = -1;
            ayoff = -1;
            break;
    }
    if (gfx) {
        lbxgfx_set_new_frame(gfx, d->frame_missile);
        gfx_aux_draw_frame_to(gfx, &ui_data.aux.btemp);
    }
    nummissiles /= 10;
    SETMIN(nummissiles, MAX_VISIBLE_MISSILE);
    if (extradiv) {
        nummissiles /= extradiv;
    }
    SETMAX(nummissiles, 1);
    angle = (angle + 90) % 360; /* FIXME set the correct angle in the if chain, it is not used between */
    for (int i = 0; i < nummissiles; ++i) {
        int xoff, yoff;
        if ((extradiv > 0) && (extradiv != 10000) && ((i & 1) != 0)) {
            continue;
        }
        xoff = util_math_angle_dist_cos(angle, tbl_missile_off[nummissiles - 1][i * 2 + 0])
             - util_math_angle_dist_sin(angle, tbl_missile_off[nummissiles - 1][i * 2 + 1])
             + axoff - 3;
        yoff = util_math_angle_dist_cos(angle, tbl_missile_off[nummissiles - 1][i * 2 + 1])
             + util_math_angle_dist_sin(angle, tbl_missile_off[nummissiles - 1][i * 2 + 0])
             + ayoff - 3;
        gfx_aux_draw_frame_from_limit(x + xoff, y + yoff, &ui_data.aux.btemp, 0, 0, UI_VGA_W - 1, UI_VGA_H - 1, UI_SCREEN_W, ui_scale);
    }
}

void ui_battle_draw_bomb_attack(const struct battle_s *bt, int attacker_i, int target_i, ui_battle_bomb_t bombtype)
{
    uint8_t *gfx;
    int x, y, x0, y0, x1, y1;
    switch (bombtype) {
        default:
        case UI_BATTLE_BOMB_BOMB:
            gfx = ui_data.gfx.space.bombs;
            break;
        case UI_BATTLE_BOMB_BIO:
            gfx = ui_data.gfx.space.biologic;
            break;
        case UI_BATTLE_BOMB_WARPDIS:
            gfx = ui_data.gfx.space.dis_bem2;
            break;
    }
    lbxgfx_set_frame_0(gfx);
    {
        const struct battle_item_s *b;
        const struct firing_s *fr = &(bt->g->gaux->firing[0]);
        b = &(bt->item[attacker_i]);
        x = b->sx * 32;
        y = b->sy * 24;
        x0 = x + fr[b->look].target_x;
        y0 = y + fr[b->look].target_y;
        b = &(bt->item[target_i]);
        x1 = b->sx * 32 + fr[b->look].target_x;
        y1 = b->sy * 24 + fr[b->look].target_y;
    }
    for (int f = 0; f < 10; ++f) {
        ui_delay_prepare();
        ui_battle_draw_arena(bt, attacker_i, 1);
        ui_battle_draw_bottom(bt);
        gfx_aux_draw_frame_to(gfx, &ui_data.aux.btemp);
        gfx_aux_draw_frame_from_rotate_limit(x0, y0, x1, y1, &ui_data.aux.btemp, 0, 0, UI_VGA_W - 1, UI_VGA_H - 1, UI_SCREEN_W, ui_scale);
        ui_battle_draw_item(bt, attacker_i, x, y);
        uiobj_finish_frame();
        ui_delay_ticks_or_click(2);
    }
}

void ui_battle_draw_beam_attack(const struct battle_s *bt, int attacker_i, int target_i, int wpni)
{
    const struct battle_item_s *b = &(bt->item[attacker_i]);
    const struct battle_item_s *b2 = &(bt->item[target_i]);
    const struct firing_s *fr = &(bt->g->gaux->firing[0]);
    weapon_t wpnt = b->wpn[wpni].t;
    uint8_t btype = tbl_shiptech_weap[wpnt].v24;
    int v16, fx[3], fy[3], tx, ty;
    bool dir = false;
    v16 = (b->wpn[wpni].n + 1) / 2;
    SETMIN(v16, 3);
    if (b->side == SIDE_R) {
        if (b->sx < b2->sx) {
            dir = true;
        }
        for (int i = 0; i < 3; ++i) {
            fx[i] = b->sx * 32 + 31 - game_aux_get_firing_param_x(bt->g->gaux, b->look, i + 1, dir);
        }
        tx = b2->sx * 32 + fr[b2->look].target_x;
    } else {
        /*53d58*/
        if (b->sx > b2->sx) {
            dir = true;
        }
        for (int i = 0; i < 3; ++i) {
            fx[i] = b->sx * 32 + game_aux_get_firing_param_x(bt->g->gaux, b->look, i + 1, dir);
        }
        tx = b2->sx * 32 + 31 - fr[b2->look].target_x;
    }
    /*53e5c*/
    for (int i = 0; i < 3; ++i) {
        fy[i] = b->sy * 24 + game_aux_get_firing_param_y(bt->g->gaux, b->look, i + 1, dir);
    }
    ty = b2->sy * 24 + fr[b2->look].target_y;
    switch (btype) {
        default: /* 0..3 */
            if (btype == 2) {
                btype = 3;
            }
            ui_battle_draw_beam_attack_do1(bt, fx, fy, tx, ty, wpnt, attacker_i, v16, btype);
            break;
        case 4:
            ui_battle_draw_beam_attack_do2(bt, fx, fy, tx, ty, wpnt, attacker_i, target_i, v16);
            break;
    }
}

void ui_battle_draw_stasis(const struct battle_s *bt, int attacker_i, int target_i)
{
    uint8_t *gfx = ui_data.gfx.space.stasis2;
    int x, y, x0, y0, x1, y1;
    lbxgfx_set_frame_0(gfx);
    {
        const struct battle_item_s *b;
        const struct firing_s *fr = &(bt->g->gaux->firing[0]);
        b = &(bt->item[attacker_i]);
        x0 = b->sx * 32 + fr[b->look].target_x;
        y0 = b->sy * 24 + fr[b->look].target_y;
        b = &(bt->item[target_i]);
        x = b->sx * 32;
        y = b->sy * 24;
        x1 = x + fr[b->look].target_x;
        y1 = y + fr[b->look].target_y;
    }
    for (int f = 0; f < 10; ++f) {
        ui_delay_prepare();
        ui_battle_draw_arena(bt, target_i, 1);
        ui_battle_draw_bottom(bt);
        gfx_aux_draw_frame_to(gfx, &ui_data.aux.btemp);
        gfx_aux_draw_frame_from_rotate_limit(x0, y0, x1, y1, &ui_data.aux.btemp, 0, 0, UI_VGA_W - 1, UI_VGA_H - 1, UI_SCREEN_W, ui_scale);
        if (f > 4) {
            ui_battle_draw_stasis_sub1(bt, target_i, f);
        } else {
            ui_battle_draw_item(bt, target_i, x, y);
        }
        uiobj_finish_frame();
        ui_delay_ticks_or_click(2);
    }
}

void ui_battle_draw_pulsar(const struct battle_s *bt, int attacker_i, int ptype, const uint32_t *dmgtbl)
{
    const uint8_t ctbl[5] = { 0x25, 0x40, 0x42, 0x44, 0x46 };
    uint8_t *gfx = ui_data.gfx.space.sphere2;
    const struct firing_s *fr = &(bt->g->gaux->firing[0]);
    int x, y;
    lbxgfx_set_frame_0(gfx);
    {
        const struct battle_item_s *b;
        b = &(bt->item[attacker_i]);
        x = b->sx * 32;
        y = b->sy * 24;
    }
    for (int f = 0; f < 13; ++f) {
        ui_delay_prepare();
        ui_battle_draw_arena(bt, attacker_i, 0);
        ui_battle_draw_bottom(bt);
        if (f < 6) {
            gfx_aux_draw_frame_to(gfx, &ui_data.aux.btemp);
            if (ptype == 1) {
                gfx_aux_recolor_ctbl(&ui_data.aux.btemp, ctbl, 5);
            }
            gfx_aux_scale(&ui_data.aux.btemp, f * 10 + 50, f * 10 + 50);
            gfx_aux_draw_frame_from_limit(x - (f + 1) * 5, y - (f + 1) * 4, &ui_data.aux.btemp, 0, 0, UI_VGA_W - 1, UI_VGA_H - 1, UI_SCREEN_W, ui_scale);
        }
        /*4cd27*/
        if (f > 2) {
            gfx_aux_draw_frame_to(ui_data.gfx.space.explos[f - 3], &ui_data.aux.btemp);
            gfx_aux_scale(&ui_data.aux.btemp, 80, 80);
            for (int i = 0; i <= bt->items_num; ++i) {
                if (dmgtbl[i] > 0) {
                    const struct battle_item_s *b = &(bt->item[i]);
                    int x1, y1;
                    if (b->side == SIDE_R) {
                        x1 = b->sx * 32 + 32 - fr[b->look].target_x;    /* FIXME usually + 31 - */
                    } else {
                        x1 = b->sx * 32 + fr[b->look].target_x;
                    }
                    y1 = b->sy * 24 + fr[b->look].target_y;
                    gfx_aux_draw_frame_from_limit(x1 - 13, y1 - 12, &ui_data.aux.btemp, 0, 0, UI_VGA_W - 1, UI_VGA_H - 1, UI_SCREEN_W, ui_scale);
                    lbxfont_select(2, 0xd, 0, 0);
                    lbxfont_print_num_center(x1, y1 - 2, dmgtbl[i], UI_SCREEN_W, ui_scale);
                }
            }
        }
        uiobj_finish_frame();
        ui_delay_ticks_or_click(1);
    }
}

void ui_battle_draw_stream1(const struct battle_s *bt, int attacker_i, int target_i)
{
    const uint8_t ctbl[5] = { 0x25, 0x40, 0x42, 0x44, 0x46 };
    uint8_t *gfx = ui_data.gfx.space.dis_bem2;
    int x, y, x0, y0, x1, y1;
    lbxgfx_set_frame_0(gfx);
    {
        const struct battle_item_s *b;
        const struct firing_s *fr = &(bt->g->gaux->firing[0]);
        b = &(bt->item[attacker_i]);
        x = b->sx * 32;
        y = b->sy * 24;
        x0 = x + fr[b->look].target_x;
        y0 = y + fr[b->look].target_y;
        b = &(bt->item[target_i]);
        x1 = b->sx * 32 + fr[b->look].target_x;
        y1 = b->sy * 24 + fr[b->look].target_y;
    }
    for (int f = 0; f < 10; ++f) {
        ui_delay_prepare();
        ui_battle_draw_arena(bt, attacker_i, 1);
        ui_battle_draw_bottom(bt);
        gfx_aux_draw_frame_to(gfx, &ui_data.aux.btemp);
        gfx_aux_recolor_ctbl(&ui_data.aux.btemp, ctbl, 5);
        gfx_aux_draw_frame_from_rotate_limit(x0, y0, x1, y1, &ui_data.aux.btemp, 0, 0, UI_VGA_W - 1, UI_VGA_H - 1, UI_SCREEN_W, ui_scale);
        ui_battle_draw_item(bt, attacker_i, x, y);
        uiobj_finish_frame();
        ui_delay_ticks_or_click(2);
    }
}

void ui_battle_draw_stream2(const struct battle_s *bt, int attacker_i, int target_i)
{
    const struct battle_item_s *b;
    uint8_t *gfx = ui_data.gfx.space.enviro;
    int x, y, x0, y0, x1, y1;
    lbxgfx_set_frame_0(gfx);
    {
        const struct firing_s *fr = &(bt->g->gaux->firing[0]);
        b = &(bt->item[attacker_i]);
        x = b->sx * 32;
        y = b->sy * 24;
        x0 = x + fr[b->look].target_x;
        y0 = y + fr[b->look].target_y;
        b = &(bt->item[target_i]);
        x1 = b->sx * 32 + fr[b->look].target_x;
        y1 = b->sy * 24 + fr[b->look].target_y;
    }
    for (int f = 0; f < 15; ++f) {
        ui_delay_prepare();
        if (f < 10) {
            ui_battle_draw_arena(bt, attacker_i, 1);
            ui_battle_draw_bottom(bt);
            gfx_aux_draw_frame_to(gfx, &ui_data.aux.btemp);
            gfx_aux_draw_frame_from_rotate_limit(x0, y0, x1, y1, &ui_data.aux.btemp, 0, 0, UI_VGA_W - 1, UI_VGA_H - 1, UI_SCREEN_W, ui_scale);
            ui_battle_draw_item(bt, attacker_i, x, y);
        }
        /*4c06a*/
        if (f >= 10) {
            ui_battle_draw_arena(bt, attacker_i, 0);
            ui_battle_draw_bottom(bt);
        }
        if (f > 7) {
            int s, x2, y2;
            gfx_aux_draw_frame_to(ui_data.gfx.space.envterm, &ui_data.aux.btemp);
            if (f < 11) {
                s = (100 * (f - 7)) / 4;
                x2 = b->sx * 32 + 12 - (f - 8) * 4;
                y2 = b->sy * 24 + 9 - (f - 8) * 3;
            } else {
                /*4c11d*/
                s = (100 * (16 - f)) / 4;
                x2 = b->sx * 32 + (f - 12) * 4;
                y2 = b->sy * 24 + (f - 12) * 3;
            }
            gfx_aux_scale(&ui_data.aux.btemp, s, s);
            gfx_aux_draw_frame_from_limit(x2, y2, &ui_data.aux.btemp, 0, 0, UI_VGA_W - 1, UI_VGA_H - 1, UI_SCREEN_W, ui_scale);
        }
        /*4c19d*/
        uiobj_finish_frame();
        ui_delay_ticks_or_click(2);
    }
}

void ui_battle_draw_retreat(const struct battle_s *bt)
{
    int itemi = bt->cur_item;
    const struct battle_item_s *b = &(bt->item[itemi]);
    uint8_t *gfx = ui_data.gfx.space.warpout;
    int x = b->sx * 32, y = b->sy * 24, /*di*/xf, yf;
    {
        const struct firing_s *fr = &(bt->g->gaux->firing[b->look]);
        if (b->side == SIDE_L) {
            xf = 32 - fr->target_x;
        } else {
            xf = fr->target_x;
        }
        yf = fr->target_y;
    }
    ui_sound_play_sfx(0x13);
    for (int f = 0; f < 4; ++f) {
        int s, x1;
        ui_delay_prepare();
        ui_battle_draw_arena(bt, itemi, 1);
        ui_battle_draw_bottom(bt);
        gfx_aux_draw_frame_to(gfx, &ui_data.aux.btemp);
        s = (f + 1) * (100 / 5);
        gfx_aux_scale(&ui_data.aux.btemp, s, s);
        if (b->side == SIDE_R) {
            gfx_aux_flipx(&ui_data.aux.btemp);
            x1 = x + 16 - f * 4;
        } else {
            /*4da3c*/
            x1 = x + (4 - f) * 3;
        }
        gfx_aux_draw_frame_from(x1, y + (4 - f) * 3, &ui_data.aux.btemp, UI_SCREEN_W, ui_scale);
        gfx_aux_draw_frame_to(b->gfx, &ui_data.aux.btemp);
        if (b->side == SIDE_L) {
            gfx_aux_flipx(&ui_data.aux.btemp);
        }
        gfx_aux_draw_frame_from_limit(x, y, &ui_data.aux.btemp, 0, 0, UI_VGA_W - 1, UI_VGA_H - 1, UI_SCREEN_W, ui_scale);
        uiobj_finish_frame();
        ui_delay_ticks_or_click(1);
    }
    for (int f = 0; f < 10; ++f) {
        int s, x1, y1;
        ui_delay_prepare();
        ui_battle_draw_arena(bt, itemi, 1);
        ui_battle_draw_bottom(bt);
        gfx_aux_draw_frame_to(gfx, &ui_data.aux.btemp);
        if (b->side == SIDE_R) {
            gfx_aux_flipx(&ui_data.aux.btemp);
        }
        s = 100 - (f * 5);
        gfx_aux_scale(&ui_data.aux.btemp, s, s);
        gfx_aux_draw_frame_from(x + f / 2, y + (f * 3) / 5, &ui_data.aux.btemp, UI_SCREEN_W, ui_scale);
        lbxgfx_set_frame_0(b->gfx);
        gfx_aux_draw_frame_to(b->gfx, &ui_data.aux.btemp);
        s = 100 - (f * 10);
        gfx_aux_scale(&ui_data.aux.btemp, s, s);
        if (b->side == SIDE_L) {
            gfx_aux_flipx(&ui_data.aux.btemp);
            x1 = x;
        } else {
            x1 = x + (xf * 2 * f * 10) / 100;
        }
        y1 = y + (f * yf * 10) / 100;
        gfx_aux_draw_frame_from_limit(x1, y1, &ui_data.aux.btemp, 0, 0, UI_VGA_W - 1, UI_VGA_H - 1, UI_SCREEN_W, ui_scale);
        uiobj_finish_frame();
        ui_delay_ticks_or_click(2);
    }
}

void ui_battle_draw_blackhole(const struct battle_s *bt, int attacker_i, int target_i)
{
    const uint8_t ctbl[5] = { 0xd8, 0xd4, 0xd3, 0xd2, 0xd1 };
    const struct battle_item_s *b = &(bt->item[attacker_i]);
    const struct battle_item_s *bd = &(bt->item[target_i]);
    uint8_t *gfx = ui_data.gfx.space.blk_hole;
    int x, y, x2, y2, x0, y0, x1, y1, x_ns, y_ns;
    lbxgfx_set_frame_0(gfx);
    {
        const struct firing_s *fr = &(bt->g->gaux->firing[0]);
        x = b->sx * 32;
        y = b->sy * 24;
        x2 = bd->sx * 32;
        y2 = bd->sy * 24;
        /* FIXME both have the same + 31 - flip ?? */
        if (b->side == SIDE_R) {
            x0 = x + 31 - fr[b->look].target_x;
            x1 = x2 + 32 - fr[bd->look].target_x; /* FIXME usually + 31 - */
        } else {
            x0 = x + fr[b->look].target_x;
            x1 = x2 + fr[bd->look].target_x;
        }
        y0 = y + fr[b->look].target_y;
        y1 = y2 + fr[bd->look].target_y;
    }
    y_ns = game_battle_get_xy_notsame(bt, attacker_i, target_i, &x_ns);
    for (int f = 0; f < 16; ++f) {
        ui_delay_prepare();
        ui_battle_draw_arena(bt, attacker_i, 0);
        ui_battle_draw_bottom(bt);
        if (f < 5) {
            ui_draw_line_ctbl(x0, y0, x1, y1, ctbl, 5, f, ui_scale);
            ui_draw_line_ctbl(x0, y0, x1 - y_ns, y1 - x_ns, ctbl, 5, (f + 1) % 5, ui_scale);
            ui_draw_line_ctbl(x0, y0, x1 + y_ns, y1 + x_ns, ctbl, 5, (f + 2) % 5, ui_scale);
        }
        if (f > 3) {
            gfx_aux_draw_frame_to(gfx, &ui_data.aux.btemp);
            gfx_aux_draw_frame_from(x2 + 5, y2 + 3, &ui_data.aux.btemp, UI_SCREEN_W, ui_scale);
        }
        uiobj_finish_frame();
        ui_delay_ticks_or_click(2);
    }
}

void ui_battle_draw_technull(const struct battle_s *bt, int attacker_i, int target_i)
{
    const struct battle_item_s *b = &(bt->item[attacker_i]);
    const struct battle_item_s *bd = &(bt->item[target_i]);
    uint8_t *gfx = ui_data.gfx.space.technull;
    int x, y, x2, y2, x0, y0, x1, y1, dist;
    {
        const struct firing_s *fr = &(bt->g->gaux->firing[0]);
        x = b->sx * 32;
        y = b->sy * 24;
        x2 = bd->sx * 32;
        y2 = bd->sy * 24;
        x0 = x + fr[b->look].target_x;
        y0 = y + fr[b->look].target_y;
        x1 = x2 + fr[bd->look].target_x;
        y1 = y2 + fr[bd->look].target_y;
    }
    dist = util_math_dist_fast(x0, y0, x1, y1);
    dist = (dist + 3) / 4;
    for (int f = 0; f < dist; ++f) {
        ui_delay_prepare();
        ui_battle_draw_arena(bt, target_i, 0);
        ui_battle_draw_bottom(bt);
        if ((f & 1) == 0) {
            lbxgfx_set_frame_0(gfx);
        }
        gfx_aux_draw_frame_to(gfx, &ui_data.aux.btemp);
        util_math_go_line_dist(&x0, &y0, x1, y1, 4);
        gfx_aux_draw_frame_from_limit(x0 - 16, y0 - 12, &ui_data.aux.btemp, 0, 0, UI_VGA_W - 1, UI_VGA_H - 1, UI_SCREEN_W, ui_scale);
        uiobj_finish_frame();
        ui_delay_ticks_or_click(1);
    }
    lbxgfx_set_frame_0(gfx);
    for (int f = 0; f < 10; ++f) {
        ui_delay_prepare();
        ui_battle_draw_arena(bt, target_i, 0);
        ui_battle_draw_bottom(bt);
        gfx_aux_draw_frame_to(gfx, &ui_data.aux.btemp);
        gfx_aux_draw_frame_from_limit(x2, y2, &ui_data.aux.btemp, 0, 0, UI_VGA_W - 1, UI_VGA_H - 1, UI_SCREEN_W, ui_scale);
        uiobj_finish_frame();
        ui_delay_ticks_or_click(2);
    }
}

void ui_battle_draw_repulse(const struct battle_s *bt, int attacker_i, int target_i, int sx, int sy)
{
    const uint8_t ctbl[5] = { 0x71, 0x70, 0x6f, 0x6e, 0x6d };
    const struct battle_item_s *b = &(bt->item[attacker_i]);
    const struct battle_item_s *bd = &(bt->item[target_i]);
    int numsteps = (bt->s[SIDE_L].flag_auto && bt->s[SIDE_R].flag_auto) ? 2 : 8;
    int tx = bd->sx * 32, ty = bd->sy * 24, x0, y0, fx, fy, tdx, tdy, xstep, ystep, xo = 1, yo = 0;
    {
        const struct firing_s *fa = &(bt->g->gaux->firing[b->look]);
        const struct firing_s *fd = &(bt->g->gaux->firing[bd->look]);
        if (b->side == SIDE_R) {
            fx = fd->target_x;
            x0 = b->sx * 32 + 31 - fa->target_x;
        } else {
            fx = 31 - fd->target_x;
            x0 = b->sx * 32 + fa->target_x;
        }
        fy = fd->target_y;
        y0 = b->sy * 24 + fa->target_y;
    }
    tdx = sx - bd->sx;
    tdy = sy - bd->sy;
    if (tdx == 0) {
        xstep = 0;
        xo = 1;
    } else {
        xstep = (tdx * (32 / numsteps)) / abs(tdx);
    }
    if (tdy == 0) {
        ystep = 0;
        yo = 0;
    } else {
        ystep = (tdy * (24 / numsteps)) / abs(tdy);
    }
    if ((tdx - tdy) == 0) {
        xo = -1;
    }
    ui_sound_play_sfx(0x13);
    for (int i = 0; i < numsteps; ++i) {
        ui_delay_prepare();
        ui_battle_draw_arena(bt, attacker_i, 0);
        ui_battle_draw_bottom(bt);
        ui_draw_line_ctbl(x0, y0, tx + fx, ty + fy, ctbl, 5, i % 5, ui_scale);
        ui_draw_line_ctbl(x0, y0, tx + fx - yo, ty + fy - xo, ctbl, 5, (i + 1) % 5, ui_scale);
        ui_draw_line_ctbl(x0, y0, tx + fx + yo, ty + fy + xo, ctbl, 5, (i + 2) % 5, ui_scale);
        ui_draw_line_ctbl(x0, y0, tx + fx - yo * 2, ty + fy - xo * 2, ctbl, 5, (i + 3) % 5, ui_scale);
        ui_draw_line_ctbl(x0, y0, tx + fx + yo * 2, ty + fy + xo * 2, ctbl, 5, (i + 4) % 5, ui_scale);
        uiobj_finish_frame();
        ui_delay_ticks_or_click(2);
    }
    for (int i = 0; i < numsteps; ++i) {
        ui_delay_prepare();
        tx += xstep;
        ty += ystep;
        ui_battle_draw_arena(bt, target_i, 1);
        ui_battle_draw_bottom(bt);
        ui_battle_draw_item(bt, target_i, tx, ty);
        ui_draw_line_ctbl(x0, y0, tx + fx, ty + fy, ctbl, 5, i % 5, ui_scale);
        ui_draw_line_ctbl(x0, y0, tx + fx - yo, ty + fy - xo, ctbl, 5, (i + 1) % 5, ui_scale);
        ui_draw_line_ctbl(x0, y0, tx + fx + yo, ty + fy + xo, ctbl, 5, (i + 2) % 5, ui_scale);
        ui_draw_line_ctbl(x0, y0, tx + fx - yo * 2, ty + fy - xo * 2, ctbl, 5, (i + 3) % 5, ui_scale);
        ui_draw_line_ctbl(x0, y0, tx + fx + yo * 2, ty + fy + xo * 2, ctbl, 5, (i + 4) % 5, ui_scale);
        uiobj_finish_frame();
        ui_delay_ticks_or_click(2);
    }
    /* MOO1 sets bd->sx,sy here, we avoid modifying bt in ui/ */
    for (int i = 0; i < numsteps; ++i) {
        ui_delay_prepare();
        ui_battle_draw_arena(bt, target_i, 1);
        ui_battle_draw_bottom(bt);
        ui_battle_draw_item(bt, target_i, sx * 32, sy * 24);
        ui_draw_line_ctbl(x0, y0, tx + fx, ty + fy, ctbl, 5, i % 5, ui_scale);
        ui_draw_line_ctbl(x0, y0, tx + fx - yo, ty + fy - xo, ctbl, 5, (i + 1) % 5, ui_scale);
        ui_draw_line_ctbl(x0, y0, tx + fx + yo, ty + fy + xo, ctbl, 5, (i + 2) % 5, ui_scale);
        ui_draw_line_ctbl(x0, y0, tx + fx - yo * 2, ty + fy - xo * 2, ctbl, 5, (i + 3) % 5, ui_scale);
        ui_draw_line_ctbl(x0, y0, tx + fx + yo * 2, ty + fy + xo * 2, ctbl, 5, (i + 4) % 5, ui_scale);
        uiobj_finish_frame();
        ui_delay_ticks_or_click(2);
    }
}

void ui_battle_draw_cloaking(const struct battle_s *bt, int from, int to, int sx, int sy)
{
    int step = (from <= to) ? 10 : -10;
    const struct battle_item_s *b = &(bt->item[bt->cur_item]);
    for (int i = from; i != to; i += step) {
        ui_delay_prepare();
        ui_battle_draw_arena(bt, bt->cur_item, 1);
        ui_battle_draw_bottom(bt);
        lbxgfx_set_frame_0(b->gfx);
        gfx_aux_draw_frame_to(b->gfx, &ui_data.aux.btemp);
        if (b->side != SIDE_L) {
            gfx_aux_flipx(&ui_data.aux.btemp);
        }
        gfx_aux_draw_cloak(&ui_data.aux.btemp, i, rnd_1_n(1000, &ui_data.seed));
        gfx_aux_draw_frame_from_limit(b->sx * 32 + 1, b->sy * 24 + 1, &ui_data.aux.btemp, 0, 0, UI_VGA_W - 1, UI_VGA_H - 1, UI_SCREEN_W, ui_scale);
        if (sx >= 0) {
            lbxgfx_set_frame_0(b->gfx);
            gfx_aux_draw_frame_to(b->gfx, &ui_data.aux.btemp);
            if (b->side != SIDE_L) {
                gfx_aux_flipx(&ui_data.aux.btemp);
            }
            gfx_aux_draw_cloak(&ui_data.aux.btemp, from - i, rnd_1_n(1000, &ui_data.seed));
            gfx_aux_draw_frame_from_limit(sx * 32 + 1, sy * 24 + 1, &ui_data.aux.btemp, 0, 0, UI_VGA_W - 1, UI_VGA_H - 1, UI_SCREEN_W, ui_scale);
        }
        uiobj_finish_frame();
        ui_delay_ticks_or_click(2);
    }
}

void ui_battle_draw_bottom(const struct battle_s *bt)
{
    if (bt->s[bt->item[bt->cur_item].side].flag_auto) {
        ui_battle_draw_bottom_no_ois(bt);
    } else {
        ui_battle_draw_bottom_add_ois(bt);
        ui_battle_draw_focusinfo(bt);
    }
}

void ui_battle_area_setup(const struct battle_s *bt)
{
    struct ui_battle_data_s *d = bt->uictx;
    const struct battle_item_s *b = &(bt->item[bt->cur_item]);
    bool is_auto = bt->s[b->side].flag_auto;
    if (is_auto) {
        d->cursor[0].cursor_i = 1;
    }
    for (int sy = 0; sy < BATTLE_AREA_H; ++sy) {
        for (int sx = 0; sx < BATTLE_AREA_W; ++sx) {
            ui_cursor_area_t *cr = &(d->cursor[1 + sy * BATTLE_AREA_W + sx]);
            if (is_auto) {
                cr->cursor_i = 1;
            } else {
                int8_t v;
                v = bt->area[sy][sx];
                if (v <= 0) {
                    cr->cursor_i = 2;
                    cr->mouseoff = 0/*battle_cursor_2_mouseoff*/;
                } else if (v == 1) {
                    cr->cursor_i = 3;
                    cr->mouseoff = 0/*battle_cursor_3_mouseoff*/;
                } else if (v >= 30) {
                    cr->cursor_i = 4;
                    cr->mouseoff = 0/*battle_cursor_4_mouseoff*/;
                } else {
                    cr->cursor_i = 5;
                    cr->mouseoff = 0/*battle_cursor_5_mouseoff*/;
                }
            }
        }
    }
    ui_cursor_setup_area(1 + BATTLE_AREA_W * BATTLE_AREA_H, &(d->cursor[0]));
}

void ui_battle_turn_pre(const struct battle_s *bt)
{
    ui_delay_prepare();
}

void ui_battle_turn_post(const struct battle_s *bt)
{
    ui_delay_ticks_or_click(2);
}

ui_battle_action_t ui_battle_turn(const struct battle_s *bt)
{
    const struct ui_battle_data_s *d = bt->uictx;
    int itemi = bt->cur_item;
    const struct battle_item_s *b = &(bt->item[itemi]);
    /*4ece2*/
    int16_t oi;
    oi = uiobj_handle_input_cond();
    if ((b->stasisby > 0) || ((itemi == 0) && (b->num <= 0))) {
        oi = d->oi_done;
    }
    if (0
      || (oi == d->oi_done) || (bt->turn_done) || (oi == UIOBJI_ESC)
      || ((b->missile != 0) && (b->maxrange == 0) && bt->has_attacked)
    ) {
        if (oi == d->oi_done) {
            ui_sound_play_sfx_24();
        }
        return UI_BATTLE_ACT_DONE;
    }
    if (oi == d->oi_grid) {
        ui_data.battle.show_grid = !ui_data.battle.show_grid;
    }
    /*4eddd*/
    if (oi == d->oi_wait) {
        ui_sound_play_sfx_24();
        return UI_BATTLE_ACT_WAIT;
    }
    /*4ee70*/
    if (oi == d->oi_auto) {
        ui_sound_play_sfx_24();
        /*bt->s[b->side].flag_auto = 1;*/
        if (b->missile == 0) {
            /*b->missile = 1;*/
            ui_battle_draw_bottom_no_ois(bt);
            uiobj_finish_frame();
        }
        return UI_BATTLE_ACT_AUTO;
    }
    /*4eeb8*/
    if (oi == d->oi_missile) {
        ui_sound_play_sfx_24();
        return UI_BATTLE_ACT_MISSILE;
    }
    /*4ef23*/
    if (oi == d->oi_planet) {
        ui_sound_play_sfx_24();
        return UI_BATTLE_ACT_PLANET;
    }
    /*4ef45*/
    if (oi == d->oi_scan) {
        ui_sound_play_sfx_24();
        return UI_BATTLE_ACT_SCAN;
    }
    /*4ef92*/
    if (oi == d->oi_special) {
        ui_sound_play_sfx_24();
        return UI_BATTLE_ACT_SPECIAL;
    }
    if (oi == d->oi_retreat) {
        ui_sound_play_sfx_24();
        return UI_BATTLE_ACT_RETREAT;
    }
    for (int sy = 0; sy < BATTLE_AREA_H; ++sy) {
        for (int sx = 0; sx < BATTLE_AREA_W; ++sx) {
            if (oi == d->oi_area[sy][sx]) {
                return UI_BATTLE_ACT_CLICK(sx, sy);
            }
        }
    }
    game_rng_step(bt->g);
    return UI_BATTLE_ACT_NONE;
}

void ui_battle_ai_pre(const struct battle_s *bt)
{
    struct ui_battle_data_s *d = bt->uictx;
    uiobj_table_clear();
    d->oi_ai = uiobj_add_mousearea_all(MOO_KEY_UNKNOWN);
}

bool ui_battle_ai_post(const struct battle_s *bt)
{
    struct ui_battle_data_s *d = bt->uictx;
    bool got_input;
    int16_t oi;
    oi = uiobj_handle_input_cond();
    got_input = ((oi == d->oi_ai) || (oi == UIOBJI_ESC));
    uiobj_table_clear();
    d->oi_ai = UIOBJI_INVALID;
    return got_input;
}
