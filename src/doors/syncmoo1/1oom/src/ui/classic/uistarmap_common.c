#include "config.h"

#include <stdio.h>

#include "uistarmap_common.h"
#include "comp.h"
#include "game.h"
#include "game_aux.h"
#include "game_fleet.h"
#include "game_misc.h"
#include "game_num.h"
#include "game_str.h"
#include "game_tech.h"
#include "kbd.h"
#include "mouse.h"
#include "lbxgfx.h"
#include "lbxfont.h"
#include "lib.h"
#include "log.h"
#include "rnd.h"
#include "types.h"
#include "ui.h"
#include "uicursor.h"
#include "uidraw.h"
#include "uidefs.h"
#include "uidelay.h"
#include "uiobj.h"
#include "uisound.h"
#include "uistarmap.h"

/* -------------------------------------------------------------------------- */

const uint8_t colortbl_textbox[5] = { 0x18, 0x17, 0x16, 0x15, 0x14 };
const uint8_t colortbl_line_red[5] = { 0x44, 0x43, 0x42, 0x41, 0x40 };
const uint8_t colortbl_line_reloc[5] = { 0x14, 0x15, 0x16, 0x17, 0x18 };
const uint8_t colortbl_line_green[5] = { 0xb0, 0xb1, 0xb2, 0xb3, 0xb4 };

static uint8_t ui_starmap_cursor_on_star(const struct starmap_data_s *d, int16_t oi)
{
    if (oi == 0) {
        return PLANET_NONE;
    }
    for (int i = 0; i < d->g->galaxy_stars; ++i) {
        if (oi == d->oi_tbl_stars[i]) {
            return i;
        }
    }
    return PLANET_NONE;
}

/* -------------------------------------------------------------------------- */

static void ui_starmap_draw_planetinfo_do(const struct game_s *g, player_id_t api, uint8_t planet_i, bool explored, bool show_plus, bool draw_name)
{
    const planet_t *p = &g->planet[planet_i];
    if (explored || (ui_extra_enabled && g->gaux->flag_cheat_stars)) {
        if (draw_name) {
            lbxfont_select_set_12_4(4, 0xf, 0, 0);
            lbxfont_print_str_center(269, 10, p->name, UI_SCREEN_W, ui_scale);
        }
        /* stars in nebulas get a purple instead of a red frame */
        if (ui_extra_enabled && p->battlebg==0) {
            ui_draw_box1(228, 25, 309, 52, 0xd3, 0xd3, ui_scale);
        }
        if (p->type == PLANET_TYPE_NOT_HABITABLE) {
            lbxfont_select(0, 0xe, 0, 0);
            lbxfont_print_str_center(269, 32, game_str_sm_nohabit, UI_SCREEN_W, ui_scale);
            lbxfont_print_str_center(269, 41, game_str_sm__planets, UI_SCREEN_W, ui_scale);
        } else {
            const char *str = NULL;
            lbxgfx_draw_frame(229, 27, ui_data.gfx.planets.planet[p->infogfx], UI_SCREEN_W, ui_scale);
            lbxfont_select(0, 0xd, 0, 0);
            lbxfont_print_str_right(305, 28, game_str_tbl_sm_pltype[p->type], UI_SCREEN_W, ui_scale);
            if (g->evn.have_plague && (g->evn.plague_planet_i == planet_i)) {
                str = game_str_sm_plague;
            } else if (g->evn.have_nova && (g->evn.nova_planet_i == planet_i)) {
                str = game_str_sm_nova;
            } else if (g->evn.have_comet && (g->evn.comet_planet_i == planet_i)) {
                str = game_str_sm_comet;
            } else if (g->evn.have_pirates && (g->evn.pirates_planet_i == planet_i)) {
                str = game_str_sm_pirates;
            } else if (p->unrest == PLANET_UNREST_REBELLION) {
                str = game_str_sm_rebellion;
            } else if (p->unrest == PLANET_UNREST_UNREST) {
                str = game_str_sm_unrest;
            } else if (g->evn.have_accident && (g->evn.accident_planet_i == planet_i)) {
                str = game_str_sm_accident;
            }
            if (str) {
                lbxfont_select(5, 5, 0, 0);
                lbxfont_print_str_right(305, 40, str, UI_SCREEN_W, ui_scale);
            } else {
                int x, xp, max_pop = p->max_pop3;
                lbxfont_select(0, 1, 0, 0);
                if (p->special == PLANET_SPECIAL_NORMAL) {
                    str = game_str_tbl_sm_pgrowth[p->growth];
                } else {
                    str = game_str_tbl_sm_pspecial[p->special];
                }
                lbxfont_print_str_right(305, 36, str, UI_SCREEN_W, ui_scale);
                lbxfont_select(2, 0xe, 0, 0);
                if (show_plus && ((p->owner == api) || (ui_extra_enabled && g->gaux->flag_cheat_stars)) && game_planet_can_terraform(g, p, p->owner, ui_extra_enabled)) {
                    lbxfont_print_str_normal(289, 44, "+", UI_SCREEN_W, ui_scale);
                    x = 287;
                    xp = x - 22;
                } else {
                    x = 291;
                    xp = x - 23;
                }
                lbxfont_print_str_normal(xp, 45, game_str_sm_pop, UI_SCREEN_W, ui_scale);
                lbxfont_print_str_normal(295, 45, game_str_sm_max, UI_SCREEN_W, ui_scale);
                if ((!show_plus) || (g->eto[api].race != RACE_SILICOID)) {
                    max_pop -= p->waste;
                }
                SETMAX(max_pop, 10);
                lbxfont_print_num_right(x, 45, max_pop, UI_SCREEN_W, ui_scale);
            }
        }
    } else {
        lbxfont_select(5, 0xe, 0, 0);
        lbxfont_print_str_center(269, show_plus ? 27 : 35, game_str_sm_unexplored, UI_SCREEN_W, ui_scale);
    }
}

static void ui_starmap_draw_range_parsec(struct starmap_data_s *d, int y)
{
    const struct game_s *g = d->g;
    int dist = game_get_min_dist(g, d->api, g->planet_focus_i[d->api]);
    char buf[64];
    lib_sprintf(buf, sizeof(buf), "%s %i %s", game_str_sm_range, dist, (dist == 1) ? game_str_sm_parsec : game_str_sm_parsecs);
    lbxfont_select_set_12_4(0, 4, 0, 0);
    lbxfont_print_str_center(269, y, buf, UI_SCREEN_W, ui_scale);
}

static void ui_starmap_draw_sliders_and_prod(struct starmap_data_s *d)
{
    const struct game_s *g = d->g;
    const planet_t *p = &g->planet[g->planet_focus_i[d->api]];
    int x = 311;
    char buf[64];

    for (planet_slider_i_t i = PLANET_SLIDER_SHIP; i < PLANET_SLIDER_NUM; ++i) {
        ui_draw_filled_rect(227, 81 + 11 * i, 244, 90 + 11 * i, p->slider_lock[i] ? 0x22 : 0, ui_scale);
    }

    lbxgfx_draw_frame(224, 5, ui_data.gfx.starmap.yourplnt, UI_SCREEN_W, ui_scale);
    lbxfont_select(2, 0xd, 0xe, 0);
    lib_sprintf(buf, sizeof(buf), "%i \x02(%i)\x01", p->prod_after_maint, p->total_prod);
    lbxfont_print_str_right(x, 72, buf, UI_SCREEN_W, ui_scale);
    lbxfont_select(2, 0xd, 0, 0);
    lbxfont_print_num_right(265, 61, p->pop, UI_SCREEN_W, ui_scale);
    lbxfont_print_num_right(x, 61, p->missile_bases, UI_SCREEN_W, ui_scale);

    for (planet_slider_i_t i = PLANET_SLIDER_SHIP; i < PLANET_SLIDER_NUM; ++i) {
        ui_draw_filled_rect(253, 84 + 11 * i, 278, 84 + 11 * i + 3, 0x2f, ui_scale);
        if (p->slider[i] != 0) {
            ui_draw_slider(253, 84 + 11 * i + 1, p->slider[i], 4, -1, p->slider_lock[i] ? 0x22 : 0x73, ui_scale);
        }
    }

    lbxfont_select(2, 0xa, 0, 0);
    if (p->buildship == BUILDSHIP_STARGATE) {
        ui_draw_filled_rect(229, 141, 274, 166, 0, ui_scale);
        lbxgfx_draw_frame(229, 141, ui_data.gfx.starmap.stargate, UI_SCREEN_W, ui_scale);
        lbxfont_print_str_center(251, 169, game_str_sm_stargate, UI_SCREEN_W, ui_scale);
    } else {
        const shipdesign_t *sd = &g->srd[p->owner].design[p->buildship];
        uint8_t *gfx = ui_data.gfx.ships[sd->look];
        lbxgfx_set_frame_0(gfx);
        lbxgfx_draw_frame(236, 142, gfx, UI_SCREEN_W, ui_scale);
        lbxfont_print_str_center(252, 169, sd->name, UI_SCREEN_W, ui_scale);
    }

    lbxfont_select(2, 6, 0, 0);
    {
        int v;
        v = game_planet_get_slider_text(g, p, PLANET_SLIDER_SHIP, buf, sizeof(buf));
        lbxfont_print_str_right(x, 83, buf, UI_SCREEN_W, ui_scale);
        lbxfont_select(0, 0xd, 0, 0);
        if (v >= 0) {
            lbxfont_print_num_right(271, 160, v, UI_SCREEN_W, ui_scale);
        }
    }
    lbxfont_select(2, 6, 0, 0);
    game_planet_get_slider_text(g, p, PLANET_SLIDER_DEF, buf, sizeof(buf));
    lbxfont_print_str_right(x, 94, buf, UI_SCREEN_W, ui_scale);
    game_planet_get_slider_text(g, p, PLANET_SLIDER_IND, buf, sizeof(buf));
    lbxfont_print_str_right(x, 105, buf, UI_SCREEN_W, ui_scale);
    {
        int v;
        v = game_planet_get_slider_text_eco(g, p, ui_extra_enabled, buf, sizeof(buf));
        lbxfont_print_str_right(x, 116, buf, UI_SCREEN_W, ui_scale);
        if (v >= 0) {
            if (ui_extra_enabled) {
                if (v < 100) {
                    lib_sprintf(buf, sizeof(buf), "%i.%i", v / 10, v % 10); /* "+0.X" does not fit the box */
                } else {
                    lib_sprintf(buf, sizeof(buf), "+%i", v / 10);
                }
            } else {
                lib_sprintf(buf, sizeof(buf), "+%i", v);
            }
            lbxfont_print_str_right(297, 116, buf, UI_SCREEN_W, ui_scale);
        }
    }
    {
        int v;
        v = game_planet_get_slider_text(g, p, PLANET_SLIDER_TECH, buf, sizeof(buf));
        if (v > 9999) {
            ui_draw_filled_rect(288, 127, 312, 132, 7, ui_scale);
        } else {
            x -= 9;
        }
        lbxfont_print_str_right(x, 127, buf, UI_SCREEN_W, ui_scale);
    }
}

static void ui_starmap_draw_textbox_finished(const struct game_s *g, player_id_t api, int pi)
{
    const planet_t *p = &g->planet[pi];
    char *buf = ui_data.strbuf;
    planet_finished_t i;
    for (i = 0; i < FINISHED_NUM; ++i) {
        if (BOOLVEC_IS1(p->finished, i)) {
            break;
        }
    }
    game_planet_get_finished_text(g, p, i, buf, UI_STRBUF_SIZE);
    int y = ui_extra_enabled ? 45 : 54;
    ui_draw_textbox_2str("", buf, y, ui_scale);
    ui_draw_textbox_2str("", game_str_sm_planratio, 110, ui_scale);
}

static void ui_starmap_add_oi_enroute(struct starmap_data_s *d, bool want_prio)
{
    const struct game_s *g = d->g;
    const int x = ui_data.starmap.x;
    const int y = ui_data.starmap.y;
    for (int i = 0; i < g->enroute_num; ++i) {
        const fleet_enroute_t *r = &(g->enroute[i]);
        if (BOOLVEC_IS1(r->visible, d->api) && (BOOLVEC_IS1(ui_data.starmap.select_prio_fleet, i) == want_prio)) {
            int x0 = (r->x - x) * 2 + 8;
            int y0 = (r->y - y) * 2 + 8;
            d->oi_tbl_enroute[i] = uiobj_add_mousearea_limited(x0, y0, x0 + 8, y0 + 4, starmap_scale, MOO_KEY_UNKNOWN);
        }
    }
    for (int i = 0; i < g->transport_num; ++i) {
        const transport_t *r = &(g->transport[i]);
        if (BOOLVEC_IS1(r->visible, d->api) && (BOOLVEC_IS1(ui_data.starmap.select_prio_trans, i) == want_prio)) {
            int x0 = (r->x - x) * 2 + 8;
            int y0 = (r->y - y) * 2 + 8;
            d->oi_tbl_transport[i] = uiobj_add_mousearea_limited(x0, y0, x0 + 8, y0 + 4, starmap_scale, MOO_KEY_UNKNOWN);
        }
    }
}

static int ui_starmap_scrollkey_accel(int zh)
{
    int v = zh;
    if (zh < 0) {
        v = -v;
    }
    v = 1 + (v / 4);
    if (v > 8) v = 8;
    v *= ui_sm_scroll_speed / 2 + 1;
    if (zh < 0) {
        v = -v;
    }
    return v;
}

/* -------------------------------------------------------------------------- */

void ui_starmap_draw_basic(struct starmap_data_s *d)
{
    const struct game_s *g = d->g;
    const planet_t *p = &g->planet[g->planet_focus_i[d->api]];

    ui_starmap_draw_starmap(d);
    ui_starmap_draw_button_text(d, true);
    ui_draw_filled_rect(224, 5, 314, 178, 0, ui_scale);
    if (BOOLVEC_IS0(p->explored, d->api) && !(ui_extra_enabled && g->gaux->flag_cheat_stars)) {
        lbxgfx_draw_frame(224, 5, ui_data.gfx.starmap.unexplor, UI_SCREEN_W, ui_scale);
        lbxfont_select_set_12_4(5, 1, 0, 0);
        lbxfont_print_str_split(232, 74, 76, game_str_tbl_sm_stinfo[p->star_type], 2, UI_SCREEN_W, UI_SCREEN_H, ui_scale);
        ui_starmap_draw_range_parsec(d, 165);
    } else {
        player_id_t owner = p->owner;
        int pi = g->planet_focus_i[d->api];
        if (BOOLVEC_IS0(p->within_srange, d->api) && !(ui_extra_enabled && g->gaux->flag_cheat_stars) && ((owner == PLAYER_NONE) || BOOLVEC_IS0(g->eto[d->api].contact, owner))) {
            owner = g->seen[d->api][pi].owner;
        }
        if (owner == PLAYER_NONE) {
            lbxgfx_draw_frame(224, 5, ui_data.gfx.starmap.no_colny, UI_SCREEN_W, ui_scale);
            ui_data.gfx.colonies.current = ui_data.gfx.colonies.d[p->type * 2];
            lbxgfx_draw_frame(227, 73, ui_data.gfx.colonies.current, UI_SCREEN_W, ui_scale);
            ui_draw_box1(227, 73, 310, 174, 0, 0, ui_scale);
            ui_starmap_draw_range_parsec(d, 80);
        } else if (((owner != d->api) && !(ui_extra_enabled && g->gaux->flag_cheat_stars)) || (p->unrest == PLANET_UNREST_REBELLION)) {
            char buf[64];
            int pop, bases, range_y;
            lbxgfx_draw_frame(224, 5, ui_data.gfx.starmap.en_colny, UI_SCREEN_W, ui_scale);
            ui_data.gfx.colonies.current = ui_data.gfx.colonies.d[p->type * 2 + 1];
            lbxgfx_draw_frame(227, 73, ui_data.gfx.colonies.current, UI_SCREEN_W, ui_scale);
            ui_draw_box1(227, 73, 310, 174, 0, 0, ui_scale);
            lib_sprintf(buf, sizeof(buf), "%s %s", game_str_tbl_race[g->eto[owner].race], game_str_sm_colony);
            if (BOOLVEC_IS1(p->within_srange, d->api)) {
                lbxfont_select_set_12_4(5, tbl_banner_fontparam[g->eto[owner].banner], 0, 0);
                lbxfont_print_str_center(270, 84, buf, UI_SCREEN_W, ui_scale);
                pop = p->pop;
                bases = p->missile_bases;
                range_y = 95;
            } else {
                lbxfont_select_set_12_4(0, tbl_banner_fontparam[g->eto[owner].banner], 0, 0);
                lbxfont_print_str_center(269, 75, game_str_sm_lastrep, UI_SCREEN_W, ui_scale);
                lbxfont_print_str_center(268, 83, buf, UI_SCREEN_W, ui_scale);    /* TODO combine with above */
                pop = g->seen[d->api][pi].pop;
                bases = g->seen[d->api][pi].bases;
                range_y = 92;
            }
            lbxfont_select(0, 0xd, 0, 0);
            lbxfont_print_num_right(265, 61, pop, UI_SCREEN_W, ui_scale);
            lbxfont_print_num_right(310, 61, bases, UI_SCREEN_W, ui_scale);
            ui_starmap_draw_range_parsec(d, range_y);
        } else {
            ui_starmap_draw_sliders_and_prod(d);
            lbxgfx_set_frame_0(ui_data.gfx.starmap.col_butt_ship);
            lbxgfx_set_frame_0(ui_data.gfx.starmap.col_butt_reloc);
            if (p->buildship == BUILDSHIP_STARGATE) {
                lbxgfx_set_frame(ui_data.gfx.starmap.col_butt_reloc, 1);
            }
            if ((g->evn.have_plague == 0) || (g->evn.plague_planet_i != g->planet_focus_i[d->api])) {
                lbxgfx_set_frame_0(ui_data.gfx.starmap.col_butt_trans);
            } else {
                lbxgfx_set_frame(ui_data.gfx.starmap.col_butt_trans, 1);
            }
            lbxgfx_draw_frame(282, 140, ui_data.gfx.starmap.col_butt_ship, UI_SCREEN_W, ui_scale);
            lbxgfx_draw_frame(282, 152, ui_data.gfx.starmap.col_butt_reloc, UI_SCREEN_W, ui_scale);
            lbxgfx_draw_frame(282, 164, ui_data.gfx.starmap.col_butt_trans, UI_SCREEN_W, ui_scale);
        }
    }
    ui_starmap_draw_planetinfo(g, d->api, g->planet_focus_i[d->api], d->planet_draw_name);
    if (g->evn.build_finished_num[d->api]) {
        ui_starmap_draw_textbox_finished(g, d->api, g->planet_focus_i[d->api]);
    }
}

void ui_starmap_draw_starmap(struct starmap_data_s *d)
{
    const struct game_s *g = d->g;
    int x, y, tx, ty;
    char str[16];
    STARMAP_LIM_INIT();
    {
        int v, step;
        step = STARMAP_SCROLLSTEP;
        v = ui_data.starmap.x2;
        x = ui_data.starmap.x;
        if (v != x) {
            if (step == 0) {
                x = v;
            } else {
                if (v < x) {
                    x -= STARMAP_SCROLLSTEP;
                    SETMAX(x, v);
                } else {
                    x += STARMAP_SCROLLSTEP;
                    SETMIN(x, v);
                }
            }
            ui_data.starmap.x = x;
        }
        v = ui_data.starmap.y2;
        y = ui_data.starmap.y;
        if (v != y) {
            if (step == 0) {
                y = v;
            } else {
                if (v < y) {
                    y -= STARMAP_SCROLLSTEP;
                    SETMAX(y, v);
                } else {
                    y += STARMAP_SCROLLSTEP;
                    SETMIN(y, v);
                }
            }
            ui_data.starmap.y = y;
        }
    }
    if (++d->anim_delay >= STARMAP_ANIM_DELAY) {
        d->anim_delay = 0;
    }
    ui_draw_filled_rect(6, 6, 222 - 1, 178 - 1, 0, ui_scale);
    lbxgfx_draw_frame(0, 0, ui_data.gfx.starmap.mainview, UI_SCREEN_W, ui_scale);
    uiobj_set_limits(STARMAP_LIMITS);
    {
        uint8_t *gfx1, *gfx2;
        int x0, y0, x1, y1, lx, ly;
        x0 = (-x / 4) + 6;
        y0 = (-y / 4) + 6;
        x1 = ((-x + 1) / 2) + 6;
        y1 = ((-y + 1) / 2) + 6;
        gfx1 = ui_fixbugs_enabled ? ui_data.gfx.starmap.starbak2 : ui_data.gfx.starmap.starback;
        gfx2 = ui_fixbugs_enabled ? ui_data.gfx.starmap.starback : ui_data.gfx.starmap.starbak2;
        lx = 222 * ui_scale - 1;
        ly = 178 * ui_scale - 1;
        for (int yb = y0; yb < ly; yb += 200) {
            for (int xb = x0; xb < lx; xb += 320) {
                lbxgfx_draw_frame_offs(xb, yb, gfx1, STARMAP_LIMITS, UI_SCREEN_W, starmap_scale);
            }
        }
        for (int yb = y1; yb < ly; yb += 200) {
            for (int xb = x1; xb < lx; xb += 320) {
                lbxgfx_draw_frame_offs(xb, yb, gfx2, STARMAP_LIMITS, UI_SCREEN_W, starmap_scale);
            }
        }
    }
    for (int i = 0; i < g->nebula_num; ++i) {
        int tx, ty;
        tx = (g->nebula_x[i] - x) * 2 + 7;
        ty = (g->nebula_y[i] - y) * 2 + 7;
        lbxgfx_draw_frame_offs(tx, ty, ui_data.gfx.starmap.nebula[i], STARMAP_LIMITS, UI_SCREEN_W, starmap_scale);
    }
    if (ui_data.starmap.flag_show_grid) {
        int x0, y0, x1, y1;
        for (y0 = 10; y0 < g->galaxy_maxy; y0 += 50) {
            int ty;
            x0 = (-x) * 2 + 6;
            x1 = (g->galaxy_maxx - x) * 2 + 6;
            ty = (y0 - y) * 2 + 6;
            ui_draw_line_limit(x0, ty, x1, ty, 4, starmap_scale);
        }
        for (x0 = 10; x0 < g->galaxy_maxx; x0 += 50) {
            int tx;
            y0 = (-y) * 2 + 6;
            y1 = (g->galaxy_maxy - y) * 2 + 6;
            tx = (x0 - x) * 2 + 6;
            ui_draw_line_limit(tx, y0, tx, y1, 4, starmap_scale);
        }
    }
    for (int pi = 0; pi < g->galaxy_stars; ++pi) {
        const planet_t *p = &g->planet[pi];
        if ((p->owner == d->api) && (p->reloc != pi)) {
            const planet_t *p2 = &g->planet[p->reloc];
            int x0, y0, x1, y1;
            x0 = (p->x - x) * 2 + 14;
            x1 = (p2->x - x) * 2 + 14;
            y0 = (p->y - y) * 2 + 14;
            y1 = (p2->y - y) * 2 + 14;
            ui_draw_line_limit_ctbl(x0, y0, x1, y1, colortbl_line_reloc, 5, ui_data.starmap.line_anim_phase, starmap_scale);
        }
    }
    for (int pi = 0; pi < g->galaxy_stars; ++pi) {
        const planet_t *p = &g->planet[pi];
        uint8_t *gfx = ui_data.gfx.starmap.stars[p->star_type + p->look];
        uint8_t anim_frame = ui_data.star_frame[pi];
        bool explored = BOOLVEC_IS1(p->explored, d->api);
        bool visible = BOOLVEC_IS1(p->within_srange, d->api);
        bool done = false;
        lbxgfx_set_new_frame(gfx, (anim_frame < 4) ? anim_frame : 0);
        gfx_aux_draw_frame_to(gfx, &ui_data.starmap.star_aux);
        if (p->look > 0) {
            tx = (p->x - x) * 2 + 8;
            ty = (p->y - y) * 2 + 9;
        } else {
            tx = (p->x - x) * 2 + 11;
            ty = (p->y - y) * 2 + 11;
        }
        gfx_aux_draw_frame_from_limit(tx, ty, &ui_data.starmap.star_aux, STARMAP_LIMITS, UI_SCREEN_W, starmap_scale);
        if (d->anim_delay == 0) {
            if (anim_frame == 4) {
                anim_frame = rnd_0_nm1(50, &ui_data.seed);
            } else {
                anim_frame = (anim_frame + 1) % 50;
            }
            ui_data.star_frame[pi] = anim_frame;
        }
        tx = (p->x - x) * 2 + 14;
        ty = (p->y - y) * 2 + 22;
        if (p->owner == PLAYER_NONE) {
            lbxfont_select(2, 7, 0, 0);
        } else if (visible || (pi == g->evn.planet_orion_i)) {
            lbxfont_select(2, tbl_banner_fontparam[g->eto[p->owner].banner], 0, 0);
        } else if (BOOLVEC_IS1(g->eto[d->api].contact, p->owner) || (p->within_frange[d->api] == 1)) {
            lbxfont_select(2, 0, 0, 0);
            lbxfont_set_color0(tbl_banner_color[g->eto[p->owner].banner]);
        } else {
            lbxfont_select(2, 7, 0, 0);
        }
        if (ui_extra_enabled && explored && ui_data.starmap.star_text_type != UI_SM_STAR_TEXT_NAME) {
            if (p->type == PLANET_TYPE_NOT_HABITABLE) {
                lbxfont_print_str_center_limit(tx, ty, "0/0", STARMAP_TEXT_LIMITS, UI_SCREEN_W, starmap_scale);
                done = true;
            } else if (visible && ui_data.starmap.star_text_type == UI_SM_STAR_TEXT_POPULATION) {
                int max_pop = p->max_pop3;
                if (g->eto[d->api].race != RACE_SILICOID) {
                    max_pop -= p->waste;
                }
                if (game_planet_can_terraform(g, p, d->api, true)) {
                    lib_sprintf(str, sizeof(str), "%i/%i+", p->pop, max_pop);
                } else {
                    lib_sprintf(str, sizeof(str), "%i/%i", p->pop, max_pop);
                }
                lbxfont_print_str_center_limit(tx, ty, str, STARMAP_TEXT_LIMITS, UI_SCREEN_W, starmap_scale);
                done = true;
            } else if (ui_data.starmap.star_text_type == UI_SM_STAR_TEXT_ENVIRONMENT) {
                lib_sprintf(str, sizeof(str), "%s", game_str_tbl_sm_pltype[p->type]);
                lbxfont_print_str_center_limit(tx, ty, str, STARMAP_TEXT_LIMITS, UI_SCREEN_W, starmap_scale);
                done = true;
            } else if (ui_data.starmap.star_text_type == UI_SM_STAR_TEXT_SPECIAL) {
                if (p->special == PLANET_SPECIAL_NORMAL) {
                    lib_sprintf(str, sizeof(str), "%s", "Normal");
                } else {
                    lib_sprintf(str, sizeof(str), "%s", game_str_tbl_sm_pspecial[p->special]);
                }
                lbxfont_print_str_center_limit(tx, ty, str, STARMAP_TEXT_LIMITS, UI_SCREEN_W, starmap_scale);
                done = true;
            }
        }
        if (ui_data.starmap.star_text_type == UI_SM_STAR_TEXT_DISTANCE) {
            if (p->owner != d->api) {
                lib_sprintf(str, sizeof(str), "%d parsecs", game_get_min_dist(g, d->api, pi));
                lbxfont_print_str_center_limit(tx, ty, str, STARMAP_TEXT_LIMITS, UI_SCREEN_W, starmap_scale);
                done = true;
            }
        }
        if (!done) {
            bool do_print = visible
                        || (pi == g->evn.planet_orion_i)
                        || (BOOLVEC_IS1(g->eto[d->api].contact, p->owner))
                        || (p->within_frange[d->api] == 1);
            if (p->owner != PLAYER_NONE && do_print) {
                lbxfont_print_str_center_limit(tx, ty, p->name, STARMAP_TEXT_LIMITS, UI_SCREEN_W, starmap_scale);
            }
        }
    }
    if (1
      && (ui_data.ui_main_loop_action != UI_MAIN_LOOP_RELOC)
      && (ui_data.ui_main_loop_action != UI_MAIN_LOOP_TRANS)
      && (ui_data.ui_main_loop_action != UI_MAIN_LOOP_ORBIT_OWN_SEL)
      && (ui_data.ui_main_loop_action != UI_MAIN_LOOP_TRANSPORT_SEL)
      && (ui_data.ui_main_loop_action != UI_MAIN_LOOP_ENROUTE_SEL)
      && (ui_data.ui_main_loop_action != UI_MAIN_LOOP_ORBIT_EN_SEL)
    ) {
        const planet_t *p = &g->planet[g->planet_focus_i[d->api]];
        tx = (p->x - x) * 2 + 8;
        ty = (p->y - y) * 2 + 8;
        lbxgfx_draw_frame_offs_delay(tx, ty, !d->anim_delay, ui_data.gfx.starmap.planbord, STARMAP_LIMITS, UI_SCREEN_W, starmap_scale);
    }
    if (d->anim_delay == 0) {
        if (--ui_data.starmap.line_anim_phase < 0) {
            ui_data.starmap.line_anim_phase = 4;
        }
    }
    for (int i = 0; i < g->enroute_num; ++i) {
        const fleet_enroute_t *r = &g->enroute[i];
        if (BOOLVEC_IS1(r->visible, d->api)) {
            uint8_t *gfx = ui_data.gfx.starmap.smalship[g->eto[r->owner].banner];
            const planet_t *p = &g->planet[r->dest];
            tx = (r->x - x) * 2 + 8;
            ty = (r->y - y) * 2 + 8;
            if (p->x < r->x) {
                lbxgfx_set_new_frame(gfx, 1);
            } else {
                lbxgfx_set_frame_0(gfx);
            }
            p = &g->planet[g->planet_focus_i[d->api]];
            if (g->eto[d->api].have_ia_scanner && (p->owner == d->api) && (r->owner != d->api) && (r->dest == g->planet_focus_i[d->api])) {
                ui_draw_line_limit_ctbl(tx + 5, ty + 2, (p->x - x) * 2 + 14, (p->y - y) * 2 + 14, colortbl_line_red, 5, ui_data.starmap.line_anim_phase, starmap_scale);
            }
            if (ui_extra_enabled && ui_data.starmap.flag_show_own_routes && d->show_planet_focus) {
                if ((r->owner == d->api) && (r->dest == g->planet_focus_i[d->api])) {
                    ui_draw_line_limit_ctbl(tx + 5, ty + 2, (p->x - x) * 2 + 14, (p->y - y) * 2 + 14, colortbl_line_green, 5, ui_data.starmap.line_anim_phase, starmap_scale);
                }
            }
            lbxgfx_draw_frame_offs(tx, ty, gfx, STARMAP_LIMITS, UI_SCREEN_W, starmap_scale);
        }
    }
    for (int i = 0; i < g->transport_num; ++i) {
        const transport_t *r = &g->transport[i];
        if (BOOLVEC_IS1(r->visible, d->api)) {
            uint8_t *gfx = ui_data.gfx.starmap.smaltran[g->eto[r->owner].banner];
            const planet_t *p = &g->planet[r->dest];
            tx = (r->x - x) * 2 + 8;
            ty = (r->y - y) * 2 + 8;
            if (p->x < r->x) {
                lbxgfx_set_new_frame(gfx, 1);
            } else {
                lbxgfx_set_frame_0(gfx);
            }
            p = &g->planet[g->planet_focus_i[d->api]];
            if (g->eto[d->api].have_ia_scanner && (p->owner == d->api) && (r->owner != d->api) && (r->dest == g->planet_focus_i[d->api])) {
                ui_draw_line_limit_ctbl(tx + 5, ty + 2, (p->x - x) * 2 + 14, (p->y - y) * 2 + 14, colortbl_line_red, 5, ui_data.starmap.line_anim_phase, starmap_scale);
            }
            if (ui_extra_enabled && ui_data.starmap.flag_show_own_routes && d->show_planet_focus) {
                if ((r->owner == d->api) && (r->dest == g->planet_focus_i[d->api])) {
                    ui_draw_line_limit_ctbl(tx + 5, ty + 2, (p->x - x) * 2 + 14, (p->y - y) * 2 + 14, colortbl_line_green, 5, ui_data.starmap.line_anim_phase, starmap_scale);
                }
            }
            lbxgfx_draw_frame_offs(tx, ty, gfx, STARMAP_LIMITS, UI_SCREEN_W, starmap_scale);
        }
    }
    if (g->evn.crystal.exists && (g->evn.crystal.killer == PLAYER_NONE)) {
        tx = (g->evn.crystal.x - x) * 2 + 8;
        ty = (g->evn.crystal.y - y) * 2 + 8;
        lbxgfx_draw_frame_offs(tx, ty, ui_data.gfx.planets.smonster, STARMAP_LIMITS, UI_SCREEN_W, starmap_scale);
        lbxfont_select(2, 8, 0, 0);
        lbxfont_print_str_center_limit(tx + 2, ty + 5, game_str_sm_crystal, STARMAP_TEXT_LIMITS, UI_SCREEN_W, starmap_scale);
    }
    if ((g->evn.amoeba.exists != 0) && (g->evn.amoeba.killer == PLAYER_NONE)) {
        tx = (g->evn.amoeba.x - x) * 2 + 8;
        ty = (g->evn.amoeba.y - y) * 2 + 8;
        lbxgfx_draw_frame_offs(tx, ty, ui_data.gfx.planets.smonster, STARMAP_LIMITS, UI_SCREEN_W, starmap_scale);
        lbxfont_select(2, 8, 0, 0);
        lbxfont_print_str_center_limit(tx + 2, ty + 5, game_str_sm_amoeba, STARMAP_TEXT_LIMITS, UI_SCREEN_W, starmap_scale);
    }
    for (int pi = 0; pi < g->galaxy_stars; ++pi) {
        const planet_t *p = &g->planet[pi];
        if (BOOLVEC_IS1(p->within_srange, d->api) || BOOLVEC_IS1(g->eto[d->api].orbit[pi].visible, d->api)) {
            player_id_t tblorbit[PLAYER_NUM];
            player_id_t num;
            num = 0;
            for (player_id_t i = PLAYER_0; i < g->players; ++i) {
                const empiretechorbit_t *e = &g->eto[i];
                if (BOOLVEC_IS0(p->within_srange, d->api) && (i != d->api)) {
                    continue;
                }
                for (int j = 0; j < e->shipdesigns_num; ++j) {
                    if (e->orbit[pi].ships[j]) {
                        tblorbit[num++] = i;
                        break;
                    }
                }
            }
            tx = (p->x - x) * 2 + 25;
            if (p->have_stargate) {
                lbxgfx_draw_frame_offs(tx, (p->y - y) * 2 + 7, ui_data.gfx.starmap.stargate2, STARMAP_LIMITS, UI_SCREEN_W, starmap_scale);
            }
            ++tx;
            for (player_id_t i = PLAYER_0; i < num; ++i) {
                player_id_t i2 = tblorbit[i];
                uint8_t *gfx = ui_data.gfx.starmap.smalship[g->eto[i2].banner];
                lbxgfx_set_frame_0(gfx);
                ty = (p->y - y) * 2 + i * 6 + 8;
                lbxgfx_draw_frame_offs(tx, ty, gfx, STARMAP_LIMITS, UI_SCREEN_W, starmap_scale);
            }
        }
    }
}

void ui_starmap_draw_button_text(struct starmap_data_s *d, bool highlight)
{
    lbxfont_select_set_12_4(5, (highlight && (d->bottom_highlight == 0)) ? 0 : 2, 0, 0);
    lbxfont_print_str_normal(10, 184, game_str_sm_game, UI_SCREEN_W, ui_scale);
    lbxfont_select_set_12_4(5, (highlight && (d->bottom_highlight == 1)) ? 0 : 2, 0, 0);
    lbxfont_print_str_normal(44, 184, game_str_sm_design, UI_SCREEN_W, ui_scale);
    lbxfont_select_set_12_4(5, (highlight && (d->bottom_highlight == 2)) ? 0 : 2, 0, 0);
    lbxfont_print_str_normal(83, 184, game_str_sm_fleet, UI_SCREEN_W, ui_scale);
    lbxfont_select_set_12_4(5, (highlight && (d->bottom_highlight == 3)) ? 0 : 2, 0, 0);
    lbxfont_print_str_normal(119, 184, game_str_sm_map, UI_SCREEN_W, ui_scale);
    lbxfont_select_set_12_4(5, (highlight && (d->bottom_highlight == 4)) ? 0 : 2, 0, 0);
    lbxfont_print_str_normal(147, 184, game_str_sm_races, UI_SCREEN_W, ui_scale);
    lbxfont_select_set_12_4(5, (highlight && (d->bottom_highlight == 5)) ? 0 : 2, 0, 0);
    lbxfont_print_str_normal(184, 184, game_str_sm_planets, UI_SCREEN_W, ui_scale);
    lbxfont_select_set_12_4(5, (highlight && (d->bottom_highlight == 6)) ? 0 : 2, 0, 0);
    lbxfont_print_str_normal(230, 184, game_str_sm_tech, UI_SCREEN_W, ui_scale);
    lbxfont_select_set_12_4(5, (highlight && (d->bottom_highlight == 7)) ? 0 : 2, 0, 0);
    lbxfont_print_str_normal(263, 184, game_str_sm_next_turn, UI_SCREEN_W, ui_scale);
}

static void ui_starmap_clamp_xy(const struct game_s *g, int *x, int *y)
{
    if (!ui_sm_expanded_scroll) {
        SETRANGE(*x, 0, g->galaxy_maxx - ((108 * ui_scale) / starmap_scale));
        SETRANGE(*y, 0, g->galaxy_maxy - ((86 * ui_scale) / starmap_scale));
    } else {
        SETRANGE(*x, -((54 * ui_scale) / starmap_scale), g->galaxy_maxx - ((54 * ui_scale) / starmap_scale));
        SETRANGE(*y, -((43 * ui_scale) / starmap_scale), g->galaxy_maxy - ((43 * ui_scale) / starmap_scale));
    }
}

void ui_starmap_set_pos_focus(const struct game_s *g, player_id_t active_player)
{
    const planet_t *p = &g->planet[g->planet_focus_i[active_player]];
    ui_starmap_set_pos(g, p->x, p->y);
}

void ui_starmap_set_pos(const struct game_s *g, int x, int y)
{
    x -= (54 * ui_scale) / starmap_scale;
    y -= (43 * ui_scale) / starmap_scale;
    ui_starmap_clamp_xy(g, &x, &y);
    ui_data.starmap.x = x;
    ui_data.starmap.x2 = x;
    ui_data.starmap.y = y;
    ui_data.starmap.y2 = y;
}

static void ui_starmap_select_target(struct starmap_data_s *d, uint8_t planet_i)
{
    if (!d->controllable || (planet_i == PLANET_NONE)) {
        return;
    }
    d->g->planet_focus_i[d->api] = planet_i;
    if (ui_data.ui_main_loop_action == UI_MAIN_LOOP_TRANS) {
        d->tr.other = true;
    }
}

void ui_starmap_handle_oi_ctrl(struct starmap_data_s *d, int16_t oi)
{
#define XSTEP   0x1b
#define YSTEP   0x15
    const struct game_s *g = d->g;
    bool changed = false;
    int x, y;
    if (g->evn.build_finished_num[d->api]) {
        return;
    }
    x = ui_data.starmap.x;
    y = ui_data.starmap.y;
    if (oi == d->oi_scroll) {
        if (d->scrollx >= 0) {
            x += d->scrollx - 54;
            y += d->scrolly - 43;
            changed = true;
        } else {
            starmap_scale = d->scrollz;
            d->set_pos_focus(g, g->active_player);
        }
    } else if (oi == d->oi_ctrl_ul) {
        x -= XSTEP;
        y -= YSTEP;
        changed = true;
    } else if ((oi == d->oi_ctrl_up) || (oi == d->oi_ctrl_u2)) {
        y -= YSTEP;
        changed = true;
    } else if (oi == d->oi_ctrl_ur) {
        x += XSTEP;
        y -= YSTEP;
        changed = true;
    } else if ((oi == d->oi_ctrl_left) || (oi == d->oi_ctrl_l2)) {
        x -= XSTEP;
        changed = true;
    } else if ((oi == d->oi_ctrl_right) || (oi == d->oi_ctrl_r2)) {
        x += XSTEP;
        changed = true;
    } else if (oi == d->oi_ctrl_dl) {
        x -= XSTEP;
        y += YSTEP;
        changed = true;
    } else if ((oi == d->oi_ctrl_down) || (oi == d->oi_ctrl_d2)) {
        y += YSTEP;
        changed = true;
    } else if (oi == d->oi_ctrl_dr) {
        x += XSTEP;
        y += YSTEP;
        changed = true;
    } else if (oi == d->oi_pgdown) {
        --d->scrollz;
        SETMAX(d->scrollz, 1);
        starmap_scale = d->scrollz;
        ui_starmap_set_pos_focus(g, g->active_player);
    } else if (oi == d->oi_pgup) {
        ++d->scrollz;
        SETMIN(d->scrollz, ui_scale);
        starmap_scale = d->scrollz;
        ui_starmap_set_pos_focus(g, g->active_player);
    }
    if (changed) {
        ui_starmap_clamp_xy(g, &x, &y);
        ui_data.starmap.x2 = x;
        ui_data.starmap.y2 = y;
    }
#undef XSTEP
#undef YSTEP
}

void ui_starmap_handle_scrollkeys(struct starmap_data_s *d, int16_t oi)
{
    const struct game_s *g = d->g;
    int x, y, xh, yh;
    if (oi != 0) {
        ui_data.starmap.xhold = 0;
        ui_data.starmap.yhold = 0;
        return;
    }
    if (g->evn.build_finished_num[d->api]) {
        return;
    }
    x = ui_data.starmap.x;
    y = ui_data.starmap.y;
    xh = ui_data.starmap.xhold;
    yh = ui_data.starmap.yhold;
    if (0
      || (ui_sm_uhjk_scroll && kbd_is_pressed(MOO_KEY_u, 0, MOO_MOD_SHIFT | MOO_MOD_ALT | MOO_MOD_CTRL))
      || (ui_sm_mouse_scroll && (moouse_y <= 0))) {
        if (yh > 0) {
            yh = 0;
        }
        --yh;
    } else if (0
      || (ui_sm_uhjk_scroll && kbd_is_pressed(MOO_KEY_j, 0, MOO_MOD_SHIFT | MOO_MOD_ALT | MOO_MOD_CTRL))
      || (ui_sm_mouse_scroll && (moouse_y >= UI_SCREEN_H - 1))) {
        if (yh < 0) {
            yh = 0;
        }
        ++yh;
    } else {
        yh = 0;
    }
    if (yh) {
        y += ui_starmap_scrollkey_accel(yh);
    }
    if (0
      || (ui_sm_uhjk_scroll && kbd_is_pressed(MOO_KEY_h, 0, MOO_MOD_SHIFT | MOO_MOD_ALT | MOO_MOD_CTRL))
      || (ui_sm_mouse_scroll && (moouse_x <= 0))) {
        if (xh > 0) {
            xh = 0;
        }
        --xh;
    } else if (0
      || (ui_sm_uhjk_scroll && kbd_is_pressed(MOO_KEY_k, 0, MOO_MOD_SHIFT | MOO_MOD_ALT | MOO_MOD_CTRL))
      || (ui_sm_mouse_scroll && (moouse_x >= UI_SCREEN_W - 1))) {
        if (xh < 0) {
            xh = 0;
        }
        ++xh;
    } else {
        xh = 0;
    }
    if (xh) {
        x += ui_starmap_scrollkey_accel(xh);
    }
    if (xh || yh) {
        ui_starmap_clamp_xy(g, &x, &y);
        ui_data.starmap.x2 = x;
        ui_data.starmap.y2 = y;
        ui_data.starmap.x = x;
        ui_data.starmap.y = y;
    }
    ui_data.starmap.xhold = xh;
    ui_data.starmap.yhold = yh;
}

void ui_starmap_add_oi_bottom_buttons(struct starmap_data_s *d)
{
    d->oi_gameopts = uiobj_add_mousearea(5, 181, 36, 194, MOO_KEY_g);
    d->oi_design = uiobj_add_mousearea(40, 181, 75, 194, MOO_KEY_d);
    d->oi_fleet = uiobj_add_mousearea(79, 181, 111, 194, MOO_KEY_f);
    d->oi_map = uiobj_add_mousearea(115, 181, 139, 194, MOO_KEY_m);
    d->oi_races = uiobj_add_mousearea(143, 181, 176, 194, (ui_illogical_hotkey_fix ? MOO_KEY_a : MOO_KEY_r));
    d->oi_planets = uiobj_add_mousearea(180, 181, 221, 194, MOO_KEY_p);
    d->oi_tech = uiobj_add_mousearea(225, 181, 254, 194, MOO_KEY_t);
    d->oi_next_turn = uiobj_add_mousearea(258, 181, 314, 194, MOO_KEY_n);
}

bool ui_starmap_handle_oi_bottom_buttons(struct starmap_data_s *d, int16_t oi)
{
    ui_main_loop_action_t action = UI_MAIN_LOOP_NUM;
    if (oi == d->oi_gameopts) {
        action = UI_MAIN_LOOP_GAMEOPTS;
    } else if (oi == d->oi_design) {
        action = UI_MAIN_LOOP_DESIGN;
    } else if (oi == d->oi_fleet) {
        action = UI_MAIN_LOOP_FLEET;
    } else if (oi == d->oi_map) {
        action = UI_MAIN_LOOP_MAP;
    } else if (oi == d->oi_races) {
        action = UI_MAIN_LOOP_RACES;
    } else if (oi == d->oi_planets) {
        action = UI_MAIN_LOOP_PLANETS;
    } else if (oi == d->oi_tech) {
        action = UI_MAIN_LOOP_TECH;
    } else if (oi == d->oi_next_turn) {
        action = UI_MAIN_LOOP_NEXT_TURN;
    }
    if (action != UI_MAIN_LOOP_NUM) {
        ui_data.ui_main_loop_action = action;
        return true;
    }
    return false;
}

void ui_starmap_add_oi_misc(struct starmap_data_s *d)
{
    d->oi_alt_c = uiobj_add_inputkey(MOO_KEY_c | MOO_MOD_ALT);
    d->oi_alt_m = uiobj_add_inputkey(MOO_KEY_m | MOO_MOD_ALT);
    if (d->show_planet_focus) {
        d->oi_alt_r = uiobj_add_inputkey(MOO_KEY_r | MOO_MOD_ALT);
        d->oi_ctrl_r = uiobj_add_inputkey(MOO_KEY_r | MOO_MOD_CTRL);
    }
    if (ui_extra_enabled) {
        d->oi_alt_f = uiobj_add_inputkey(MOO_KEY_f | MOO_MOD_ALT);
        d->oi_alt_o = uiobj_add_inputkey(MOO_KEY_o | MOO_MOD_ALT);
    }
}

bool ui_starmap_handle_oi_misc(struct starmap_data_s *d, int16_t oi)
{
    bool match = false;
    struct game_s *g = d->g;
    uint8_t planet_focus_i = g->planet_focus_i[d->api];
    if (oi == d->oi_alt_m) {
        ui_data.starmap.flag_show_grid = !ui_data.starmap.flag_show_grid;
        match = true;
    } else if (oi == d->oi_alt_c) {
        d->set_pos_focus(d->g, d->api);
        match = true;
    } else if (oi == d->oi_alt_f) {
        ui_data.starmap.flag_show_own_routes = !ui_data.starmap.flag_show_own_routes;
        match = true;
    } else if (oi == d->oi_alt_o) {
        ui_data.starmap.star_text_type = (ui_data.starmap.star_text_type + 1) % UI_SM_STAR_TEXT_NUM;
        match = true;
    } else if ((oi == d->oi_alt_r) && game_reloc_dest_ok(g, planet_focus_i, d->api)) {
        for (int i = 0; i < g->galaxy_stars; ++i) {
            planet_t *p = &(g->planet[i]);
            if ((p->owner == d->api) && (p->reloc != i)) {
                p->reloc = planet_focus_i;
            }
        }
        match = true;
    } else if ((oi == d->oi_ctrl_r) && game_reloc_dest_ok(g, planet_focus_i, d->api)) {
        int count = 0;
        for (int i = 0; i < g->galaxy_stars; ++i) {
            planet_t *p = &(g->planet[i]);
            if ((p->owner == d->api) && (p->reloc != planet_focus_i)) {
                p->reloc = planet_focus_i;
                ++count;
            }
        }
        if (count == 0) {
            for (int i = 0; i < g->galaxy_stars; ++i) {
                planet_t *p = &(g->planet[i]);
                if (p->owner == d->api) {
                    p->reloc = i;
                }
            }
        }
        match = true;
    }
    return match;
}

void ui_starmap_fill_oi_tbls(struct starmap_data_s *d)
{
    const struct game_s *g = d->g;
    const int x = ui_data.starmap.x;
    const int y = ui_data.starmap.y;
    STARMAP_LIM_INIT();
    uiobj_set_limits(STARMAP_LIMITS);
    UIOBJI_SET_TBL_INVALID(d->oi_tbl_enroute);
    UIOBJI_SET_TBL_INVALID(d->oi_tbl_transport);
    ui_starmap_add_oi_enroute(d, false);
    for (int i = 0; i < g->galaxy_stars; ++i) {
        for (int j = 0; j < g->players; ++j) {
            d->oi_tbl_pl_stars[j][i] = UIOBJI_INVALID;
        }
    }
    for (int i = 0; i < g->galaxy_stars; ++i) {
        const planet_t *p = &(g->planet[i]);
        if (BOOLVEC_IS1(p->within_srange, d->api) || BOOLVEC_IS1(g->eto[d->api].orbit[i].visible, d->api)) {
            int numorbits, x0, y0;
            player_id_t tblpl[PLAYER_NUM];
            numorbits = 0;
            for (int j = 0; j < g->players; ++j) {
                const fleet_orbit_t *r = &(g->eto[j].orbit[i]);
                if (BOOLVEC_IS0(p->within_srange, d->api) && (j != d->api)) {
                    continue;
                }
                for (int k = 0; k < g->eto[j].shipdesigns_num; ++k) {
                    if (r->ships[k]) {
                        tblpl[numorbits++] = j;
                        break;
                    }
                }
            }
            x0 = (p->x - x) * 2 + 26;
            y0 = (p->y - y) * 2 + 8;
            for (int j = 0; j < numorbits; ++j, y0 += 6) {
                d->oi_tbl_pl_stars[tblpl[j]][i] = uiobj_add_mousearea_limited(x0, y0, x0 + 8, y0 + 4, starmap_scale, MOO_KEY_UNKNOWN);
            }
        }
    }
    ui_starmap_add_oi_enroute(d, true);
}

void ui_starmap_fill_oi_tbl_stars(struct starmap_data_s *d)
{
    const struct game_s *g = d->g;
    const int x = ui_data.starmap.x;
    const int y = ui_data.starmap.y;
    STARMAP_LIM_INIT();
    uiobj_set_limits(STARMAP_LIMITS);
    for (int i = 0; i < g->galaxy_stars; ++i) {
        const planet_t *p = &(g->planet[i]);
        int x0, y0;
        x0 = (p->x - x) * 2 + 8;
        y0 = (p->y - y) * 2 + 8;
        d->oi_tbl_stars[i] = uiobj_add_mousearea_limited(x0, y0, x0 + 13, y0 + 13, starmap_scale, MOO_KEY_UNKNOWN);
    }
}

void ui_starmap_fill_oi_tbl_stars_own(struct starmap_data_s *d, player_id_t owner)
{
    const struct game_s *g = d->g;
    const int x = ui_data.starmap.x;
    const int y = ui_data.starmap.y;
    STARMAP_LIM_INIT();
    uiobj_set_limits(STARMAP_LIMITS);
    for (int i = 0; i < g->galaxy_stars; ++i) {
        const planet_t *p = &(g->planet[i]);
        if (p->owner == owner) {
            int x0, y0;
            x0 = (p->x - x) * 2 + 8;
            y0 = (p->y - y) * 2 + 8;
            d->oi_tbl_stars[i] = uiobj_add_mousearea_limited(x0, y0, x0 + 13, y0 + 13, starmap_scale, MOO_KEY_UNKNOWN);
        }
    }
}

void ui_starmap_clear_oi_ctrl(struct starmap_data_s *d)
{
    d->oi_scroll = UIOBJI_INVALID;
    d->oi_ctrl_left = UIOBJI_INVALID;
    d->oi_ctrl_l2 = UIOBJI_INVALID;
    d->oi_ctrl_right = UIOBJI_INVALID;
    d->oi_ctrl_r2 = UIOBJI_INVALID;
    d->oi_ctrl_ul = UIOBJI_INVALID;
    d->oi_ctrl_ur = UIOBJI_INVALID;
    d->oi_ctrl_up = UIOBJI_INVALID;
    d->oi_ctrl_u2 = UIOBJI_INVALID;
    d->oi_ctrl_dl = UIOBJI_INVALID;
    d->oi_ctrl_down = UIOBJI_INVALID;
    d->oi_ctrl_d2 = UIOBJI_INVALID;
    d->oi_ctrl_dr = UIOBJI_INVALID;
    d->oi_pgup = UIOBJI_INVALID;
    d->oi_pgdown = UIOBJI_INVALID;
}

void ui_starmap_fill_oi_ctrl(struct starmap_data_s *d)
{
    d->oi_scroll = uiobj_add_tb(6, 6, 2, 2, 108, 86, &d->scrollx, &d->scrolly, &d->scrollz, ui_scale);
    d->oi_ctrl_left = uiobj_add_inputkey(MOO_KEY_LEFT | MOO_MOD_CTRL);
    if (MOO_KEY_LEFT != MOO_KEY_KP4) {
        d->oi_ctrl_l2 = uiobj_add_inputkey(MOO_KEY_KP4 | MOO_MOD_CTRL);
    }
    d->oi_ctrl_right = uiobj_add_inputkey(MOO_KEY_RIGHT | MOO_MOD_CTRL);
    if (MOO_KEY_RIGHT != MOO_KEY_KP6) {
        d->oi_ctrl_r2 = uiobj_add_inputkey(MOO_KEY_KP6 | MOO_MOD_CTRL);
    }
    d->oi_ctrl_ul = uiobj_add_inputkey(MOO_KEY_KP7 | MOO_MOD_CTRL);
    d->oi_ctrl_ur = uiobj_add_inputkey(MOO_KEY_KP9 | MOO_MOD_CTRL);
    d->oi_ctrl_up = uiobj_add_inputkey(MOO_KEY_UP | MOO_MOD_CTRL);
    if (MOO_KEY_UP != MOO_KEY_KP8) {
        d->oi_ctrl_u2 = uiobj_add_inputkey(MOO_KEY_KP8 | MOO_MOD_CTRL);
    }
    d->oi_ctrl_dl = uiobj_add_inputkey(MOO_KEY_KP1 | MOO_MOD_CTRL);
    d->oi_ctrl_down = uiobj_add_inputkey(MOO_KEY_DOWN | MOO_MOD_CTRL);
    if (MOO_KEY_DOWN != MOO_KEY_KP2) {
        d->oi_ctrl_d2 = uiobj_add_inputkey(MOO_KEY_KP2 | MOO_MOD_CTRL);
    }
    d->oi_ctrl_dr = uiobj_add_inputkey(MOO_KEY_KP3 | MOO_MOD_CTRL);
    d->oi_pgup = uiobj_add_inputkey(MOO_KEY_PAGEUP);
    d->oi_pgdown = uiobj_add_inputkey(MOO_KEY_PAGEDOWN);
}

void ui_starmap_sn0_setup(struct shipnon0_s *sn0, int sd_num, const shipcount_t *ships)
{
    int num = 0;
    for (int i = 0; i < sd_num; ++i) {
        shipcount_t n;
        n = ships[i];
        sn0->ships[num] = n;
        if (n) {
            sn0->type[num++] = i;
        }
    }
    sn0->num = num;
}

void ui_starmap_update_reserve_fuel(struct game_s *g, struct shipnon0_s *sn0, const shipcount_t *ships, player_id_t pi)
{
    const bool *hrf = &(g->srd[pi].have_reserve_fuel[0]);
    for (int i = 0; i < sn0->num; ++i) {
        int st;
        st = sn0->type[i];
        if ((!hrf[st]) && (ships[st] != 0)) {
            sn0->have_reserve_fuel = false;
            return;
        }
    }
    sn0->have_reserve_fuel = true;
}

void ui_starmap_draw_planetinfo(const struct game_s *g, player_id_t api, int planet_i, bool draw_name)
{
    const planet_t *p = &(g->planet[planet_i]);
    ui_starmap_draw_planetinfo_do(g, api, planet_i, BOOLVEC_IS1(p->explored, api), true, draw_name);
}

void ui_starmap_draw_planetinfo_2(const struct game_s *g, int p1, int p2, int planet_i)
{
    const planet_t *p = &(g->planet[planet_i]);
    player_id_t api = (p1 < PLAYER_NUM) ? p1 : p2;
    bool explored = true;
    if (0
      || (IS_HUMAN(g, p1) && BOOLVEC_IS0(p->explored, p1))
      || (IS_HUMAN(g, p2) && BOOLVEC_IS0(p->explored, p2))
    ) {
        explored = false;
    }
    ui_starmap_draw_planetinfo_do(g, api, planet_i, explored, false, true);
}

int ui_starmap_newship_next(const struct game_s *g, player_id_t pi, int i)
{
    int t = i;
    const planet_t *p;
    do {
        i = (i + 1) % g->galaxy_stars;
        p = &(g->planet[i]);
    } while ((!((p->owner == pi) && BOOLVEC_IS1(p->finished, FINISHED_SHIP))) && (i != t));
    return i;
}

int ui_starmap_newship_prev(const struct game_s *g, player_id_t pi, int i)
{
    int t = i;
    const planet_t *p;
    do {
        if (--i < 0) { i = g->galaxy_stars - 1; }
        p = &(g->planet[i]);
    } while ((!((p->owner == pi) && BOOLVEC_IS1(p->finished, FINISHED_SHIP))) && (i != t));
    return i;
}

int ui_starmap_enemy_incoming(const struct game_s *g, player_id_t pi, int i, bool next)
{
    int t = i;
    do {
        if (next) {
            i = (i + 1) % g->galaxy_stars;
        } else {
            if (--i < 0) { i = g->galaxy_stars - 1; }
        }
        if (g->planet[i].owner == pi) {
           for (int j = 0; j < g->enroute_num; ++j) {
               const fleet_enroute_t *r = &(g->enroute[j]);
               if (BOOLVEC_IS1(r->visible, pi) && (r->owner != pi) && (r->dest == i)) {
                    return i;
                }
            }
            for (int j = 0; j < g->transport_num; ++j) {
                const transport_t *r = &(g->transport[j]);
                if (BOOLVEC_IS1(r->visible, pi) && (r->owner != pi) && (r->dest == i)) {
                    return i;
                }
            }
        }
    } while (i != t);
    return i;
}

void ui_starmap_common_init(struct game_s *g, struct starmap_data_s *d, player_id_t active_player)
{
    d->set_pos_focus = ui_starmap_set_pos_focus;
    d->g = g;
    d->api = active_player;
    d->controllable = false;
    d->show_planet_focus = true;
    d->anim_delay = 0;
    d->planet_draw_name = true;
    d->scrollx = 0;
    d->scrolly = 0;
    d->scrollz = starmap_scale;
}

void ui_starmap_common_update_mouse_hover(struct starmap_data_s *d, int16_t oi)
{
    d->bottom_highlight = -1;
    if (oi == d->oi_gameopts) {
        d->bottom_highlight = 0;
    } else if (oi == d->oi_design) {
        d->bottom_highlight = 1;
    } else if (oi == d->oi_fleet) {
        d->bottom_highlight = 2;
    } else if (oi == d->oi_map) {
        d->bottom_highlight = 3;
    } else if (oi == d->oi_races) {
        d->bottom_highlight = 4;
    } else if (oi == d->oi_planets) {
        d->bottom_highlight = 5;
    } else if (oi == d->oi_tech) {
        d->bottom_highlight = 6;
    } else if (oi == d->oi_next_turn) {
        d->bottom_highlight = 7;
    }
    if (ui_sm_mouseover_focus && d->controllable) {
        ui_starmap_select_target(d, ui_starmap_cursor_on_star(d, oi));
    }
}
