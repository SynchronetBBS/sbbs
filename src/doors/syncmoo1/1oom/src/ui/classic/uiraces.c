#include "config.h"

#include <stdio.h>

#include "uiraces.h"
#include "comp.h"
#include "game.h"
#include "game_aux.h"
#include "game_diplo.h"
#include "game_misc.h"
#include "game_str.h"
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

struct races_data_s {
    struct game_s *g;
    uint8_t *gfx;
    int num;
    player_id_t api;
    uint8_t tbl_ei[PLAYER_NUM - 1];
    bool tbl_gone[PLAYER_NUM - 1];
    uint8_t cursor_mode; /* 0, 4, 5 */
    int16_t tbl_spymode[PLAYER_NUM - 1];
};

static void races_data_load(struct races_data_s *d)
{
    d->gfx = lbxfile_item_get(LBXFILE_BACKGRND, 0x34);
}

static void races_data_free(struct races_data_s *d)
{
    lbxfile_item_release(LBXFILE_BACKGRND, d->gfx);
}

static void races_draw_cb(void *vptr)
{
    struct races_data_s *d = vptr;
    struct game_s *g = d->g;
    empiretechorbit_t *e = &(g->eto[d->api]);
    char buf[0x80];

    for (int i = 0; i < d->num; ++i) {
        player_id_t pi;
        pi = d->tbl_ei[i];
        e->spymode[pi] = (spymode_t)d->tbl_spymode[i];
    }

    ui_draw_color_buf(0x5b);
    lbxgfx_draw_frame(0, 0, d->gfx, UI_SCREEN_W, ui_scale);

    lbxfont_select_set_12_1(5, 0xa, 0, 0);
    for (int i = d->num; i < (PLAYER_NUM - 1); ++i) {
        int x, y;
        x = (i / 3) * 157;
        y = (i % 3) * 64;
        ui_draw_filled_rect(x + 58, y + 9, x + 156, y + 21, 0x5b, ui_scale);
        ui_draw_filled_rect(x + 56, y + 25, x + 158, y + 37, 0x5b, ui_scale);
        ui_draw_filled_rect(x + 58, y + 41, x + 156, y + 51, 0x5b, ui_scale);
        ui_draw_filled_rect(x + 56, y + 55, x + 158, y + 65, 0x5b, ui_scale);
        lbxfont_print_str_center(x + 109, y + 27, (i < g->players - 1) ? game_str_ra_nocont : game_str_ra_notpres, UI_SCREEN_W, ui_scale);
    }

    if (d->num) {
        int v;
        ui_draw_filled_rect(238, 141, 283, 144, 0, ui_scale);
        if (e->security) {
            ui_draw_slider(238, 142, e->security * 9, 40, -1, 0x73, ui_scale);
        }
        v = (e->security / 5);
        if (e->race == RACE_DARLOK) {
            v += 20;
        }
        lib_sprintf(buf, sizeof(buf), "+%i%%", v);
        lbxfont_select(2, 0xb, 0, 0);
        lbxfont_set_color_c_n(0x26, 5);
        lbxfont_print_str_right(309, 141, buf, UI_SCREEN_W, ui_scale);
    } else {
        ui_draw_filled_rect(238, 141, 283, 144, 0x5a, ui_scale);
    }
    lbxfont_select(2, 6, 0, 0);
    lbxfont_print_str_normal(167, 150, game_str_ra_secline1, UI_SCREEN_W, ui_scale);
    lbxfont_print_str_normal(167, 157, game_str_ra_secline2, UI_SCREEN_W, ui_scale);
    {
        int sumalloc = e->security;
        struct strbuild_s strbuild = strbuild_init(buf, sizeof(buf));
        for (int i = 0; i < d->num; ++i) {
            player_id_t pi;
            pi = d->tbl_ei[i];
            sumalloc += e->spying[pi];
        }
        strbuild_catf(&strbuild, "%s: ", game_str_ra_alloc);
        if (sumalloc == 0) {
            strbuild_append_char(&strbuild, '0');
        } else {
            strbuild_catf(&strbuild, "%i.%i", sumalloc / 10, sumalloc % 10);
        }
        strbuild_catf(&strbuild, "%% %s", game_str_ra_planres);
        lbxfont_select(2, 0, 0, 0);
        lbxfont_print_str_normal(167, 170, buf, UI_SCREEN_W, ui_scale);
    }
    for (int i = 0; i < d->num; ++i) {
        const uint8_t colortbl_races_banner[BANNER_NUM] = { 0xeb, 0x6d, 0xd4, 0x43, 0x0a, 0x52 };
        int x, y;
        player_id_t pi;
        pi = d->tbl_ei[i];
        x = (i / 3) * 157;
        y = (i % 3) * 64;
        lbxgfx_draw_frame(x + 9, y + 10, ui_data.gfx.planets.race[g->eto[pi].race], UI_SCREEN_W, ui_scale);
        if (d->tbl_gone[i]) {
            lbxfont_select_set_12_4(0, 0, 0, 0);
            lbxfont_print_str_center(x + 29, y + 23, game_str_ra_diplo, UI_SCREEN_W, ui_scale);
            lbxfont_print_str_center(x + 29, y + 31, game_str_ra_gone, UI_SCREEN_W, ui_scale);
        }
        ui_draw_filled_rect(x + 9, y + 48, x + 48, y + 61, colortbl_races_banner[g->eto[pi].banner], ui_scale);
        lbxfont_select(5, 6, 0, 0);
        lbxfont_print_str_center(x + 29, y + 51, game_str_tbl_races[g->eto[pi].race], UI_SCREEN_W, ui_scale);
        {
            int spying, spyprod, spyspend, spies, spycost;
            spying = e->spying[pi];
            ui_draw_filled_rect(x + 103, y + 44, x + 128, y + 47, 0, ui_scale);
            lbxfont_select(2, 0, 0, 0);
            lbxfont_set_color_c_n(0x26, 5);
            spyprod = (e->total_production_bc * spying) / 1000;
            spyspend = spyprod + e->spyfund[pi];
            spies = e->spies[pi];
            spycost = e->tech.percent[TECH_FIELD_COMPUTER] * 2 + 25;
            if (e->race == RACE_DARLOK) {
               spycost /= 2;
            }
            if (spying != 0) {
                ui_draw_slider(x + 103, y + 45, spying, 4, -1, 0x73, ui_scale);
            }
            if (spies == 0) {
                lib_strcpy(buf, game_str_ra_nospies, sizeof(buf));
            } else {
                lib_sprintf(buf, sizeof(buf), "%i %s", spies, (spies == 1) ? game_str_ra_spy : game_str_ra_spies);
            }
            lbxfont_select(2, 6, 0, 0);
            lbxfont_print_str_right(x + 91, y + 44, buf, UI_SCREEN_W, ui_scale);
            if (spycost <= spyspend) {
                spies = 0;
                while (spycost <= spyspend) {
                    ++spies;
                    spyspend -= spycost;
                    spycost *= 2;
                }
                lib_sprintf(buf, sizeof(buf), "%i%c%s", spies, (spies == 1) ? ' ' : '/', game_str_y);
            } else {
                if (spyprod == 0) {
                    lib_strcpy(buf, game_str_st_none, sizeof(buf));
                } else {
                    int left, years;
                    left = spycost - spyspend;
                    years = left / spyprod;
                    if (left % spyprod) {
                        ++years;
                    }
                    lib_sprintf(buf, sizeof(buf), "%i %s", years, game_str_y);
                }
            }
            lbxfont_print_str_right(x + 153, y + 44, buf, UI_SCREEN_W, ui_scale);
        }
        {
            const char *str;
            str = game_str_tbl_ra_treaty[e->treaty[pi]];
            ui_draw_filled_rect(x + 57, y + 8, x + 157, y + 22, 0x5b, ui_scale);
            lbxfont_select(0, 6, 0, 0);
            lbxfont_print_str_normal(x + 61, y + 10, str, UI_SCREEN_W, ui_scale);
            lbxfont_select(0, 0, 0, 0);
            lbxfont_set_color_c_n(0xe6, 5);
            lbxfont_print_str_normal(x + 60, y + 9, str, UI_SCREEN_W, ui_scale);
        }
        if (e->trade_bc[pi] != 0) {
            lib_sprintf(buf, sizeof(buf), "%s: %i %s%s", game_str_ra_trade, e->trade_bc[pi], game_str_bc, game_str_year0);
        } else {
            lib_strcpy(buf, game_str_ra_notrade, sizeof(buf));
        }
        lbxfont_select(0, 6, 0, 0);
        lbxfont_print_str_normal(x + 61, y + 17, buf, UI_SCREEN_W, ui_scale);
        lbxfont_select(0, 0, 0, 0);
        lbxfont_set_color_c_n(0xe6, 5);
        lbxfont_print_str_normal(x + 60, y + 16, buf, UI_SCREEN_W, ui_scale);
        {
            int rel;
            rel = e->relation1[pi] / 2;
            SETRANGE(rel, -50, 50);
            lbxfont_select_set_12_4(0, 6, 0, 0);
            lbxfont_select(2, 0xd, 0, 0);
            lbxgfx_draw_frame(x + 107 + rel, y + 34, ui_data.gfx.screens.race_pnt, UI_SCREEN_W, ui_scale);
            rel = e->relation1[pi] / 12 + 8;
            SETRANGE(rel, 0, 16);
            lbxfont_print_str_center(x + 107, y + 27, game_str_tbl_ra_relat[rel], UI_SCREEN_W, ui_scale);
        }
    }
    lbxgfx_set_new_frame(ui_data.gfx.screens.races_bu.status, 1);
    lbxgfx_draw_frame(165, 180, ui_data.gfx.screens.races_bu.status, UI_SCREEN_W, ui_scale);
    lbxgfx_set_new_frame(ui_data.gfx.screens.races_bu.report, 1);
    lbxgfx_draw_frame(205, 180, ui_data.gfx.screens.races_bu.report, UI_SCREEN_W, ui_scale);
    lbxgfx_set_new_frame(ui_data.gfx.screens.races_bu.audience, 1);
    lbxgfx_draw_frame(245, 180, ui_data.gfx.screens.races_bu.audience, UI_SCREEN_W, ui_scale);
    lbxgfx_set_new_frame(ui_data.gfx.screens.races_bu.ok, 1);
    lbxgfx_draw_frame(290, 180, ui_data.gfx.screens.races_bu.ok, UI_SCREEN_W, ui_scale);
    for (int i = 0; i < d->num; ++i) {
        int x, y;
        x = (i / 3) * 157;
        y = (i % 3) * 64;
        lbxgfx_set_new_frame(ui_data.gfx.screens.races_bu.hiding, (d->tbl_spymode[i] == SPYMODE_HIDE) ? 1 : 0);
        lbxgfx_draw_frame(x + 59, y + 55, ui_data.gfx.screens.races_bu.hiding, UI_SCREEN_W, ui_scale);
        lbxgfx_set_new_frame(ui_data.gfx.screens.races_bu.sabotage, (d->tbl_spymode[i] == SPYMODE_SABOTAGE) ? 1 : 0);
        lbxgfx_draw_frame(x + 77, y + 55, ui_data.gfx.screens.races_bu.sabotage, UI_SCREEN_W, ui_scale);
        lbxgfx_set_new_frame(ui_data.gfx.screens.races_bu.espionage, (d->tbl_spymode[i] == SPYMODE_ESPIONAGE) ? 1 : 0);
        lbxgfx_draw_frame(x + 114, y + 55, ui_data.gfx.screens.races_bu.espionage, UI_SCREEN_W, ui_scale);
    }
}

/* -------------------------------------------------------------------------- */

int ui_races(struct game_s *g, player_id_t api)
{
    struct races_data_s d;
    bool flag_done = false;
    int16_t oi_ok, oi_ok2, oi_audience, oi_report, oi_status, oi_sec_minus, oi_sec_plus,
            oi_tbl_oppon[PLAYER_NUM - 1],
            oi_tbl_spy_minus[PLAYER_NUM - 1],
            oi_tbl_spy_plus[PLAYER_NUM - 1]
            ;
    empiretechorbit_t *e = &(g->eto[api]);
    int ret = -1;

    d.g = g;
    d.api = api;
    races_data_load(&d);
    lbxgfx_apply_palette(d.gfx);

    game_update_production(g);
    game_update_empire_contact(g);

    d.num = 0;
    for (player_id_t pi = PLAYER_0; pi < g->players; ++pi) {
        if ((pi != api) && BOOLVEC_IS1(e->contact, pi) && IS_ALIVE(g, pi)) {
            d.tbl_ei[d.num++] = pi;
        } else {
            e->spying[pi] = 0;
        }
    }

    for (int i = 0; i < d.num; ++i) {
        d.tbl_gone[i] = game_diplo_is_gone(g, api, d.tbl_ei[i]);
    }

    d.cursor_mode = 0;

#define UIOBJ_CLEAR_LOCAL() \
    do { \
        oi_ok = UIOBJI_INVALID; \
        oi_ok2 = UIOBJI_INVALID; \
        oi_audience = UIOBJI_INVALID; \
        oi_report = UIOBJI_INVALID; \
        oi_status = UIOBJI_INVALID; \
        oi_sec_minus = UIOBJI_INVALID; \
        oi_sec_plus = UIOBJI_INVALID; \
        UIOBJI_SET_TBL3_INVALID(oi_tbl_oppon, oi_tbl_spy_minus, oi_tbl_spy_plus); \
    } while (0)

    UIOBJ_CLEAR_LOCAL();

    uiobj_set_help_id(d.num ? 10 : 11);
    uiobj_set_callback_and_delay(races_draw_cb, &d, 1);

    uiobj_table_clear();

    while (!flag_done) {
        int16_t oi;
        ui_cursor_setup_area(1, &ui_cursor_area_tbl[d.cursor_mode ? 8 : 0]);
        oi = uiobj_handle_input_cond();
        ui_delay_prepare();
        if ((oi == oi_ok) || (oi == oi_ok2) || ((oi == UIOBJI_ESC) && !d.cursor_mode)) {
            ui_sound_play_sfx_24();
            flag_done = true;
            ui_data.ui_main_loop_action = UI_MAIN_LOOP_STARMAP;
        } else if ((oi == UIOBJI_ESC) && d.cursor_mode) {
            ui_sound_play_sfx_06();
            d.cursor_mode = 0;
        } else if (oi == oi_status) {
            ui_sound_play_sfx_24();
            flag_done = true;
            ui_data.ui_main_loop_action = UI_MAIN_LOOP_EMPIRESTATUS;
        } else if (oi == oi_report) {
            ui_sound_play_sfx_24();
            d.cursor_mode = 4;
        } else if (oi == oi_audience) {
            ui_sound_play_sfx_24();
            d.cursor_mode = 5;
        } else if (oi == oi_sec_minus) {
            if (kbd_is_modifier(MOO_MOD_ALT)) {
                e->security = 0;
            } else {
                int v;
                ui_sound_play_sfx_24();
                v = e->security;
                v -= 10;
                SETMAX(v, 0);
                e->security = v;
            }
        } else if (oi == oi_sec_plus) {
            if (kbd_is_modifier(MOO_MOD_ALT)) {
                e->security = 200;
            } else {
                int v;
                ui_sound_play_sfx_24();
                v = e->security;
                v += 10;
                SETMIN(v, 200);
                e->security = v;
            }
        }
        for (int i = 0; i < (PLAYER_NUM - 1); ++i) {
            if (oi == oi_tbl_spy_minus[i]) {
                if (kbd_is_modifier(MOO_MOD_ALT)) {
                    e->spying[d.tbl_ei[i]] = 0;
                } else {
                    int v;
                    ui_sound_play_sfx_24();
                    v = e->spying[d.tbl_ei[i]];
                    v -= 4;
                    SETMAX(v, 0);
                    e->spying[d.tbl_ei[i]] = v;
                }
            } else if (oi == oi_tbl_spy_plus[i]) {
                if (kbd_is_modifier(MOO_MOD_ALT)) {
                    e->spying[d.tbl_ei[i]] = 100;
                } else {
                    int v;
                    ui_sound_play_sfx_24();
                    v = e->spying[d.tbl_ei[i]];
                    v += 4;
                    SETMIN(v, 100);
                    e->spying[d.tbl_ei[i]] = v;
                }
            } else if (oi == oi_tbl_oppon[i]) {
                ui_sound_play_sfx_24();
                if ((d.cursor_mode == 4) || (ui_extra_enabled && (d.cursor_mode == 0))) {
                    flag_done = true;
                    ui_data.ui_main_loop_action = UI_MAIN_LOOP_EMPIREREPORT;
                    ret = d.tbl_ei[i];
                    d.cursor_mode = 0;
                } else if (d.cursor_mode == 5) {
                    if (!d.tbl_gone[i]) {
                        flag_done = true;
                        ui_data.ui_main_loop_action = UI_MAIN_LOOP_AUDIENCE;
                        ret = d.tbl_ei[i];
                        d.cursor_mode = 0;
                    }
                } else {
                    d.cursor_mode = 0;
                }
            }
        }
        UIOBJ_CLEAR_LOCAL();
        for (int i = 0; i < d.num; ++i) {
            player_id_t pi;
            pi = d.tbl_ei[i];
            d.tbl_spymode[i] = (int16_t)e->spymode[pi];
        }
        if (!flag_done) {
            /* FIXME one of these copyings is redundant? */
            for (int i = 0; i < d.num; ++i) {
                player_id_t pi;
                pi = d.tbl_ei[i];
                e->spymode[pi] = (spymode_t)d.tbl_spymode[i];
            }
            races_draw_cb(&d);
            uiobj_table_clear();
            if (d.num > 0) {
                if (d.cursor_mode == 0) {
                    oi_report = uiobj_add_t0(205, 180, "", ui_data.gfx.screens.races_bu.report, MOO_KEY_r);
                    if (g->end == GAME_END_NONE) {
                        oi_audience = uiobj_add_t0(245, 180, "", ui_data.gfx.screens.races_bu.audience, MOO_KEY_a);
                    }
                }
                for (int i = 0; i < d.num; ++i) {
                    int x, y, x1;
                    x = (i / 3) * 157;
                    y = (i % 3) * 64;
                    if (d.cursor_mode == 0) {
                        uiobj_add_t3(x + 59, y + 55, "", ui_data.gfx.screens.races_bu.hiding, &(d.tbl_spymode[i]), SPYMODE_HIDE, MOO_KEY_UNKNOWN);
                        uiobj_add_t3(x + 77, y + 55, "", ui_data.gfx.screens.races_bu.sabotage, &(d.tbl_spymode[i]), SPYMODE_SABOTAGE, MOO_KEY_UNKNOWN);
                        uiobj_add_t3(x + 114, y + 55, "", ui_data.gfx.screens.races_bu.espionage, &(d.tbl_spymode[i]), SPYMODE_ESPIONAGE, MOO_KEY_UNKNOWN);
                        x1 = x + 54;
                    } else {
                        x1 = x + 157;
                    }
                    oi_tbl_oppon[i] = uiobj_add_mousearea(x + 4, y + 5, x1, y + 66, MOO_KEY_1 + i);
                }
            }
            if (d.cursor_mode == 0) {
                oi_status = uiobj_add_t0(165, 180, "", ui_data.gfx.screens.races_bu.status, MOO_KEY_s);
                oi_ok = uiobj_add_t0(290, 180, "", ui_data.gfx.screens.races_bu.ok, MOO_KEY_o);
                oi_ok2 = uiobj_add_inputkey(MOO_KEY_SPACE);
                if (d.num > 0) {
                    uiobj_add_slider_int(238, 141, 0, 200, 45, 9, &e->security);
                    oi_sec_minus = uiobj_add_mousearea(233, 139, 236, 147, MOO_KEY_UNKNOWN);
                    oi_sec_plus = uiobj_add_mousearea(285, 139, 289, 147, MOO_KEY_UNKNOWN);
                } else {
                    e->security = 0;
                }
                for (int i = 0; i < d.num; ++i) {
                    int x, y;
                    player_id_t pi;
                    x = (i / 3) * 157;
                    y = (i % 3) * 64;
                    pi = d.tbl_ei[i];
                    uiobj_add_slider_int(x + 103, y + 44, 0, 100, 25, 9, &e->spying[pi]);
                    oi_tbl_spy_minus[i] = uiobj_add_mousearea(x + 97, y + 42, x + 100, y + 50, MOO_KEY_UNKNOWN);
                    oi_tbl_spy_plus[i] = uiobj_add_mousearea(x + 130, y + 42, x + 134, y + 50, MOO_KEY_UNKNOWN);
                }
            }
            ui_draw_finish();
            ui_delay_ticks_or_click(1);
        }
    }
    for (int i = 0; i < d.num; ++i) {
        player_id_t pi;
        pi = d.tbl_ei[i];
        e->spymode[pi] = (spymode_t)d.tbl_spymode[i];
    }
    uiobj_unset_callback();
    uiobj_table_clear();
    uiobj_set_help_id(-1);
    lbxpal_select(0, 104, 113);
    races_data_free(&d);
    return ret;
}
