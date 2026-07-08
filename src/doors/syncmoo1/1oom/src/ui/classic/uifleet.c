#include "config.h"

#include <stdio.h>
#include <stdlib.h>

#include "uifleet.h"
#include "boolvec.h"
#include "comp.h"
#include "game.h"
#include "game_aux.h"
#include "game_misc.h"
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

/* -------------------------------------------------------------------------- */

#define FLEET_LINES 5

#define FLEET_VALUE(_i_)    ui_data.sorted.value[ui_data.sorted.index[(_i_)]]
#define FLEET_IS_ENROUTE(_i_)   ((FLEET_VALUE(_i_) & 0x100) != 0)
#define FLEET_GET_ENROUTE(_i_)  (FLEET_VALUE(_i_) >> 9)
#define FLEET_GET_PLANET(_i_)   (FLEET_VALUE(_i_) & 0xff)
#define FLEETS_IS_ENROUTE(_i_)  ((ui_data.sorted.value[_i_] & 0x100) != 0)
#define FLEETS_GET_ENROUTE(_i_) (ui_data.sorted.value[_i_] >> 9)
#define FLEETS_GET_PLANET(_i_)  (ui_data.sorted.value[_i_] & 0xff)

struct fleet_data_s {
    struct game_s *g;
    player_id_t api;
    int selected;
    int pos;
    int num;
    int lines;
    int order_i;
    uint8_t *gfx_fleetbrb;
};

static void load_fl_data(struct fleet_data_s *d)
{
    d->gfx_fleetbrb = lbxfile_item_get(LBXFILE_BACKGRND, 0);
}

static void free_fl_data(struct fleet_data_s *d)
{
    lbxfile_item_release(LBXFILE_BACKGRND, d->gfx_fleetbrb);
}

static void fleet_draw_cb(void *vptr)
{
    struct fleet_data_s *d = vptr;
    const struct game_s *g = d->g;
    const empiretechorbit_t *e = &(g->eto[d->api]);
    const shipdesign_t *sd = &(g->srd[d->api].design[0]);
    int num;

    ui_draw_color_buf(0x3a);
    ui_draw_filled_rect(5, 15, 40, 190, 0, ui_scale);
    lbxgfx_draw_frame(0, 0, d->gfx_fleetbrb, UI_SCREEN_W, ui_scale);
    lbxgfx_set_new_frame(ui_data.gfx.starmap.fleetbut_scrap, 1);
    lbxgfx_draw_frame(224, 181, ui_data.gfx.starmap.fleetbut_scrap, UI_SCREEN_W, ui_scale);
    lbxgfx_set_new_frame(ui_data.gfx.starmap.fleetbut_up, 1);
    lbxgfx_set_new_frame(ui_data.gfx.starmap.fleetbut_down, 1);
    lbxgfx_draw_frame(307, 24, ui_data.gfx.starmap.fleetbut_up, UI_SCREEN_W, ui_scale);
    lbxgfx_draw_frame(307, 156, ui_data.gfx.starmap.fleetbut_down, UI_SCREEN_W, ui_scale);

    lbxfont_select_set_12_4(0, 0, 0, 0);
    lbxfont_print_str_normal(7, 6, game_str_fl_station, UI_SCREEN_W, ui_scale);
    num = d->num - d->pos;
    SETMIN(num, 5);

    for (int i = 0; i < e->shipdesigns_num; ++i) {
        int x0;
        x0 = 44 * i + 48;
        lbxfont_select(2, 0xd, 0, 0);
        lbxfont_print_str_center(x0 + 19, 6, sd[i].name, UI_SCREEN_W, ui_scale);
    }
    for (int i = 0; i < num; ++i) {
        const planet_t *p;
        const shipcount_t *s;
        int x0 = 5, y0, pi, fi;
        y0 = 33 * i + 17;
        fi = i + d->pos;
        pi = FLEET_GET_PLANET(fi);
        p = &(g->planet[pi]);
        if (BOOLVEC_IS1(p->explored, d->api)) {
            player_id_t seenowner;
            uint8_t a2;
            lbxgfx_draw_frame(x0 + 1, y0, ui_data.gfx.planets.planet[p->infogfx], UI_SCREEN_W, ui_scale);
            seenowner = g->seen[d->api][pi].owner;
            if (seenowner != PLAYER_NONE) {
                a2 = tbl_banner_fontparam[g->eto[seenowner].banner];
            } else {
                a2 = 4;
            }
            lbxfont_select(2, a2, 0, 0);
            lbxfont_print_str_center(x0 + 19, y0 + 12, FLEET_IS_ENROUTE(fi) ? game_str_fl_moving : game_str_fl_inorbit, UI_SCREEN_W, ui_scale);
            lbxfont_print_str_center(x0 + 19, y0 + 19, p->name, UI_SCREEN_W, ui_scale);
        } else {
            lbxfont_select_set_12_4(2, 0xe, 0, 0);
            lbxfont_print_str_center(x0 + 19, y0 + 7, game_str_fl_unknown, UI_SCREEN_W, ui_scale);
            lbxfont_print_str_center(x0 + 19, y0 + 14, game_str_fl_system, UI_SCREEN_W, ui_scale);
        }
        if (FLEET_IS_ENROUTE(fi)) {
            s = g->enroute[FLEET_GET_ENROUTE(fi)].ships;
        } else {
            s = e->orbit[pi].ships;
        }
        for (int j = 0; j < e->shipdesigns_num; ++j) {
            int ships;
            ships = s[j];
            if (ships) {
                uint8_t *gfx_ship;
                x0 = 44 * j + 48;
                ui_draw_filled_rect(x0, y0, x0 + 36, y0 + 25, 0, ui_scale);
                if (!FLEET_IS_ENROUTE(fi)) {
                    int tmp_xoff1 = ui_data.starmap.stars_xoff1;
                    int tmp_xoff2 = ui_data.starmap.stars_xoff2;
                    ui_data.starmap.stars_xoff1 = 0;
                    ui_data.starmap.stars_xoff2 = 0;
                    ui_draw_stars(x0, y0 + 1, j * 10, 37, ui_scale);
                    ui_data.starmap.stars_xoff1 = tmp_xoff1;
                    ui_data.starmap.stars_xoff2 = tmp_xoff2;
                } else {
                    ui_draw_stars(x0, y0 + 1, j * 5, 37, ui_scale);
                }
                gfx_ship = ui_data.gfx.ships[sd[j].look];
                lbxgfx_set_frame_0(gfx_ship);
                if (!FLEET_IS_ENROUTE(fi)) {
                    lbxgfx_draw_frame(x0, y0 + 1, gfx_ship, UI_SCREEN_W, ui_scale);
                } else {
                    for (int f = 0; f <= ui_data.starmap.frame_ship; ++f) {
                        lbxgfx_draw_frame(x0, y0 + 1, gfx_ship, UI_SCREEN_W, ui_scale);
                    }
                }
                lbxfont_select(0, 0xd, 0, 0);
                lbxfont_print_num_right(x0 + 31, y0 + 19, ships, UI_SCREEN_W, ui_scale);
            }
        }
    }
    for (int i = num; i < FLEET_LINES; ++i) {
        ui_draw_filled_rect(7, i * 33 + 17, 40, i * 33 + 42, 0x3a, ui_scale);
    }
    ui_data.starmap.frame_ship = (ui_data.starmap.frame_ship + 1) % 5;
    ui_draw_set_stars_xoffs(false);
    lbxfont_select(2, 6, 0, 0);
    lbxfont_print_num_right(137, 185, e->ship_maint_bc, UI_SCREEN_W, ui_scale);
}

static void ui_fleet_sub(struct fleet_data_s *d)
{
    const struct game_s *g = d->g;
    const empiretechorbit_t *e = &(g->eto[d->api]);
    int num = 0, sd_num = e->shipdesigns_num;
    for (int i = 0; i < g->galaxy_stars; ++i) {
        const fleet_orbit_t *r = &(e->orbit[i]);
        for (int j = 0; j < sd_num; ++j) {
            if (r->ships[j] != 0) {
                ui_data.sorted.index[num] = num;
                ui_data.sorted.value[num] = i;
                ++num;
                break;
            }
        }
    }
    for (int i = 0; i < g->enroute_num; ++i) {
        const fleet_enroute_t *r = &(g->enroute[i]);
        if (r->owner == d->api) {
            for (int j = 0; j < sd_num; ++j) {
                if (r->ships[j] != 0) {
                    ui_data.sorted.index[num] = num;
                    ui_data.sorted.value[num] = (i << 9) | 0x100 | r->dest;
                    ++num;
                    break;
                }
            }
        }
    }
    d->num = num;
}

/* -------------------------------------------------------------------------- */

enum {
    UI_SORT_INDEX = 0,
    UI_SORT_STATION,
    UI_SORT_SHIP0,
    UI_SORT_SHIP1,
    UI_SORT_SHIP2,
    UI_SORT_SHIP3,
    UI_SORT_SHIP4,
    UI_SORT_SHIP5,
    UI_SORT_NUM
};

#define UI_SORT_SETUP() \
    const struct game_s *g = ui_data.sorted.g; \
    uint16_t i0 = *((uint16_t const *)ptr0); \
    uint16_t i1 = *((uint16_t const *)ptr1); \
    uint8_t pli0 = ui_data.sorted.value[i0]; \
    uint8_t pli1 = ui_data.sorted.value[i1]; \
    const planet_t *p0 = &(g->planet[pli0]); \
    const planet_t *p1 = &(g->planet[pli1])

#define UI_SORT_CMP_VALUE(_v0_, _v1_) (((_v0_) != (_v1_)) ? ((_v0_) - (_v1_)) : (i0 - i1))
#define UI_SORT_CMP_VARIABLE(_var_) UI_SORT_CMP_VALUE(p0->_var_, p1->_var_)

static int fleet_sort_inc_index(const void *ptr0, const void *ptr1)
{
    uint16_t i0 = *((uint16_t const *)ptr0);
    uint16_t i1 = *((uint16_t const *)ptr1);
    return i0 - i1;
}

static int fleet_sort_dec_index(const void *ptr0, const void *ptr1)
{
    return fleet_sort_inc_index(ptr1, ptr0);
}

static int fleet_sort_inc_station(const void *ptr0, const void *ptr1)
{
    const struct game_s *g = ui_data.sorted.g;
    uint16_t i0 = *((uint16_t const *)ptr0);
    uint16_t i1 = *((uint16_t const *)ptr1);
    uint8_t pli0 = FLEETS_GET_PLANET(i0);
    uint8_t pli1 = FLEETS_GET_PLANET(i1);
    const planet_t *p0 = &(g->planet[pli0]);
    const planet_t *p1 = &(g->planet[pli1]);
    int d;
    if (BOOLVEC_IS0(p0->explored, g->active_player) && BOOLVEC_IS0(p1->explored, g->active_player)) {
        d = i0 - i1;
    } else if (BOOLVEC_IS0(p0->explored, g->active_player)) {
        d = -1;
    } else if (BOOLVEC_IS0(p1->explored, g->active_player)) {
        d = 1;
    } else {
        d = strcmp(p0->name, p1->name);
    }
    return d;
}

static int fleet_sort_dec_station(const void *ptr0, const void *ptr1)
{
    return fleet_sort_inc_station(ptr1, ptr0);
}

static int fleet_sort_ship(const void *ptr0, const void *ptr1, int si)
{
    const struct game_s *g = ui_data.sorted.g;
    const empiretechorbit_t *e = &(g->eto[g->active_player]);
    uint16_t i0 = *((uint16_t const *)ptr0);
    uint16_t i1 = *((uint16_t const *)ptr1);
    uint8_t pli0 = FLEETS_GET_PLANET(i0);
    uint8_t pli1 = FLEETS_GET_PLANET(i1);
    const shipcount_t *s0 = FLEETS_IS_ENROUTE(i0) ? g->enroute[FLEETS_GET_ENROUTE(i0)].ships : e->orbit[pli0].ships;
    const shipcount_t *s1 = FLEETS_IS_ENROUTE(i1) ? g->enroute[FLEETS_GET_ENROUTE(i1)].ships : e->orbit[pli1].ships;
    int d = 0;
    if (s0[si] != s1[si]) {
        d = s0[si] - s1[si];
    } else {
        for (int j = 1; j < e->shipdesigns_num; ++j) {
            int i;
            i = (si + j) % e->shipdesigns_num;
            if (s0[i] != s1[i]) {
                d = s0[i] - s1[i];
                break;
            }
        }
    }
    if (d == 0) {
        d = pli0 - pli1;
    }
    if (d == 0) {
        d = i0 - i1;
    }
    return d;
}

#define UI_SORT_DEFINE_SHIP(_n_) \
static int fleet_sort_inc_ship##_n_(const void *ptr0, const void *ptr1) \
{ \
    return fleet_sort_ship(ptr0, ptr1, _n_); \
} \
static int fleet_sort_dec_ship##_n_(const void *ptr0, const void *ptr1) \
{ \
    return fleet_sort_ship(ptr1, ptr0, _n_); \
}
UI_SORT_DEFINE_SHIP(0)
UI_SORT_DEFINE_SHIP(1)
UI_SORT_DEFINE_SHIP(2)
UI_SORT_DEFINE_SHIP(3)
UI_SORT_DEFINE_SHIP(4)
UI_SORT_DEFINE_SHIP(5)

typedef int sort_cb_t(const void *, const void *);

static sort_cb_t * const sort_cb_tbl[UI_SORT_NUM * 2] = {
    fleet_sort_inc_index,
    fleet_sort_dec_index,
    fleet_sort_inc_station,
    fleet_sort_dec_station,
    fleet_sort_dec_ship0,
    fleet_sort_inc_ship0,
    fleet_sort_dec_ship1,
    fleet_sort_inc_ship1,
    fleet_sort_dec_ship2,
    fleet_sort_inc_ship2,
    fleet_sort_dec_ship3,
    fleet_sort_inc_ship3,
    fleet_sort_dec_ship4,
    fleet_sort_inc_ship4,
    fleet_sort_dec_ship5,
    fleet_sort_inc_ship5
};

static void ui_fleet_sort(struct fleet_data_s *d)
{
    qsort(ui_data.sorted.index, d->num, sizeof(ui_data.sorted.index[0]), sort_cb_tbl[d->order_i]);
    ui_data.sorted.fleet_order[d->api] = d->order_i;
}

/* -------------------------------------------------------------------------- */

int ui_fleet(struct game_s *g, player_id_t active_player)
{
    struct fleet_data_s d;
    bool flag_done = false, flag_scrap = false;
    int16_t oi_up, oi_down, oi_wheel, oi_ok, oi_scrap, oi_view, oi_tbl_ship[NUM_SHIPDESIGNS], oi_tbl_line[FLEET_LINES];
    int16_t oi_sort[UI_SORT_NUM];
    int ret = -1;
    int16_t scroll = 0;

    load_fl_data(&d);

    d.g = g;
    ui_data.sorted.g = g;
    d.api = active_player;
    d.pos = 0;
    d.num = 0;
    d.order_i = 0;
    game_update_maint_costs(g);
    ui_fleet_sub(&d);
    ui_data.starmap.frame_ship = 0;
    d.lines = 0;
    if (ui_extra_enabled) {
        d.order_i = ui_data.sorted.fleet_order[active_player];
        ui_fleet_sort(&d);
    }

    uiobj_table_clear();

#define UIOBJ_CLEAR_LOCAL() \
    do { \
        oi_up = UIOBJI_INVALID; \
        oi_down = UIOBJI_INVALID; \
        oi_wheel = UIOBJI_INVALID; \
        oi_ok = UIOBJI_INVALID; \
        oi_view = UIOBJI_INVALID; \
        oi_scrap = UIOBJI_INVALID; \
        UIOBJI_SET_TBL_INVALID(oi_tbl_line); \
        UIOBJI_SET_TBL_INVALID(oi_tbl_ship); \
        UIOBJI_SET_TBL_INVALID(oi_sort); \
    } while (0)

    UIOBJ_CLEAR_LOCAL();

    uiobj_set_help_id(8);
    uiobj_set_callback_and_delay(fleet_draw_cb, &d, 3);

    while (!flag_done) {
        int16_t oi;
        ui_cursor_setup_area(1, &ui_cursor_area_tbl[flag_scrap ? 0xa : 0]);
        oi = uiobj_handle_input_cond();
        ui_delay_prepare();
        if ((oi == oi_ok) || (oi == UIOBJI_ESC)) {
            ui_sound_play_sfx_24();
            flag_done = true;
            ui_data.ui_main_loop_action = UI_MAIN_LOOP_STARMAP;
        } else if (oi == oi_view) {
            ui_sound_play_sfx_24();
            flag_done = true;
            ui_data.ui_main_loop_action = UI_MAIN_LOOP_SPECS;
        } else if (oi == oi_scrap) {
            ui_sound_play_sfx_24();
            flag_scrap = true;
        }
        for (int i = 0; i < g->eto[active_player].shipdesigns_num; ++i) {
            if (oi == oi_tbl_ship[i]) {
                ui_sound_play_sfx_24();
                ret = i;
                ui_data.ui_main_loop_action = UI_MAIN_LOOP_MUSTSCRAP;
                ui_data.ui_main_loop_action_prev = UI_MAIN_LOOP_FLEET;
                ui_data.ui_main_loop_action_next = UI_MAIN_LOOP_FLEET;
                flag_done = true;
                break;
            }
        }
        if (oi == oi_up) {
            ui_sound_play_sfx_24();
            d.pos -= FLEET_LINES;
            SETMAX(d.pos, 0);
        } else if (oi == oi_down) {
            ui_sound_play_sfx_24();
            d.pos += FLEET_LINES;
            if ((d.pos + FLEET_LINES) > d.num) {
                d.pos = d.num - FLEET_LINES;
            }
            SETMAX(d.pos, 0);
        } else if (oi == oi_wheel) {
            d.pos += scroll;
            scroll = 0;
            SETRANGE(d.pos, 0, d.num - FLEET_LINES);
        }
        for (int i = 0; i < d.lines; ++i) {
            if (oi == oi_tbl_line[i]) {
                ui_sound_play_sfx_24();
                if (!FLEET_IS_ENROUTE(d.pos + i)) {
                    ui_data.ui_main_loop_action = UI_MAIN_LOOP_ORBIT_OWN_SEL;
                    g->planet_focus_i[active_player] = FLEET_GET_PLANET(d.pos + i);
                } else {
                    ui_data.ui_main_loop_action = UI_MAIN_LOOP_ENROUTE_SEL;
                    ui_data.starmap.fleet_selected = FLEET_GET_ENROUTE(d.pos + i);
                }
                flag_done = true;
                break;
            }
        }
        for (int i = 0; i < UI_SORT_NUM; ++i) {
            if (oi == oi_sort[i]) {
                int v;
                v = i * 2;
                if (v == d.order_i) {
                    ++v;
                }
                d.order_i = v;
                ui_fleet_sort(&d);
            }
        }
        if (!flag_done) {
            if ((d.pos + FLEET_LINES) > d.num) {
                d.pos = d.num - FLEET_LINES;
            }
            SETMAX(d.pos, 0);
            d.lines = d.num - d.pos;
            SETMIN(d.lines, FLEET_LINES);

            fleet_draw_cb(&d);
            uiobj_table_clear();
            UIOBJ_CLEAR_LOCAL();

            oi_ok = uiobj_add_t0(268, 181, "", ui_data.gfx.starmap.fleetbut_ok, MOO_KEY_SPACE);
            oi_view = uiobj_add_t0(180, 181, "", ui_data.gfx.starmap.fleetbut_view, (ui_illogical_hotkey_fix ? MOO_KEY_s : MOO_KEY_v));
            if (d.pos > 0) {
                oi_up = uiobj_add_t0(307, 24, "", ui_data.gfx.starmap.fleetbut_up, MOO_KEY_COMMA);
            }
            if ((d.num - 5) > d.pos) {
                oi_down = uiobj_add_t0(307, 156, "", ui_data.gfx.starmap.fleetbut_down, MOO_KEY_PERIOD);
            }
            if ((g->eto[active_player].shipdesigns_num > 1) && !flag_scrap) {
                oi_scrap = uiobj_add_t0(224, 181, "", ui_data.gfx.starmap.fleetbut_scrap, (ui_illogical_hotkey_fix ? MOO_KEY_c : MOO_KEY_s));
            } else {
                lbxgfx_set_new_frame(ui_data.gfx.starmap.fleetbut_scrap, 1);
                lbxgfx_draw_frame(224, 181, ui_data.gfx.starmap.fleetbut_scrap, UI_SCREEN_W, ui_scale);
            }
            if (flag_scrap) {
                for (int i = 0; i < g->eto[active_player].shipdesigns_num; ++i) {
                    int x0;
                    x0 = 44 * i + 48;
                    oi_tbl_ship[i] = uiobj_add_mousearea(x0, 0, x0 + 38, 180, MOO_KEY_UNKNOWN);
                }
            } else if (ui_extra_enabled) {
                const int x0[UI_SORT_NUM] = { 0, 10, 47,  91, 134, 178, 222, 267 };
                const int x1[UI_SORT_NUM] = { 9, 41, 87, 131, 174, 218, 262, 307 };
                const int y0 = 5, y1 = 12;
                int n;
                n = UI_SORT_SHIP0 + g->eto[active_player].shipdesigns_num;
                for (int i = 0; i < n; ++i) {
                    oi_sort[i] = uiobj_add_mousearea(x0[i], y0, x1[i], y1, MOO_KEY_UNKNOWN);
                }
            }
            for (int i = 0; i < d.lines; ++i) {
                oi_tbl_line[i] = uiobj_add_mousearea(5, 15 + i * 33, 288, 44 + i * 33, MOO_KEY_UNKNOWN);
            }
            if (d.num >= FLEET_LINES) {
                oi_wheel = uiobj_add_mousewheel(0, 0, 319, 199, &scroll);
            }
            ui_draw_finish();
            ui_delay_ticks_or_click(3);
        }
    }

    uiobj_unset_callback();
    uiobj_set_help_id(-1);
    free_fl_data(&d);
    return ret;
}
