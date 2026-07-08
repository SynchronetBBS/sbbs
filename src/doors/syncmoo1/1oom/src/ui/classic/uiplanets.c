#include "config.h"

#include <stdio.h>
#include <stdlib.h>

#include "uiplanets.h"
#include "comp.h"
#include "game.h"
#include "game_cheat.h"
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

#define PLANETS_ON_SCREEN   12

struct planets_data_s {
    player_id_t api;
    int pos;
    int num;
    int16_t amount_trans;
    uint8_t focus_i;
    int planet_i;
    int order_i;
    struct game_s *g;
    uint8_t *gfx_report;
    uint8_t *gfx_but_trans;
    uint8_t *gfx_but_ok;
    uint8_t *gfx_transfer;
};

static void load_pl_data(struct planets_data_s *d)
{
    d->gfx_report = lbxfile_item_get(LBXFILE_BACKGRND, 1);
    d->gfx_but_trans = lbxfile_item_get(LBXFILE_BACKGRND, 0x1c);
    d->gfx_but_ok = lbxfile_item_get(LBXFILE_BACKGRND, 0x1d);
    d->gfx_transfer = lbxfile_item_get(LBXFILE_BACKGRND, 0x1e);
}

static void free_pl_data(struct planets_data_s *d)
{
    lbxfile_item_release(LBXFILE_BACKGRND, d->gfx_report);
    lbxfile_item_release(LBXFILE_BACKGRND, d->gfx_but_trans);
    lbxfile_item_release(LBXFILE_BACKGRND, d->gfx_but_ok);
    lbxfile_item_release(LBXFILE_BACKGRND, d->gfx_transfer);
}

static const char *planets_get_notes_str(const struct game_s *g, uint8_t pli, bool *flag_normal_ptr, char *buf, size_t bufsize)
{
    const planet_t *p = &(g->planet[pli]);
    const char *str = NULL;
    bool flag_normal = false;
    if (g->evn.have_plague && (g->evn.plague_planet_i == pli)) {
        str = game_str_pl_plague;
    } else if (g->evn.have_nova && (g->evn.nova_planet_i == pli)) {
        str = game_str_pl_nova;
    } else if (g->evn.have_comet && (g->evn.comet_planet_i == pli)) {
        str = game_str_pl_comet;
    } else if (g->evn.have_pirates && (g->evn.pirates_planet_i == pli)) {
        str = game_str_pl_pirates;
    } else if (p->unrest == PLANET_UNREST_REBELLION) {
        str = game_str_pl_rebellion;
    } else if (p->unrest == PLANET_UNREST_UNREST) {
        str = game_str_pl_unrest;
    } else if (g->evn.have_accident && (g->evn.accident_planet_i == pli)) {
        str = game_str_pl_accident;
    } else {
        flag_normal = true;
        if (p->special != PLANET_SPECIAL_NORMAL) {
            str = game_str_tbl_sm_pspecial[p->special];
        } else if (p->growth != PLANET_GROWTH_NORMAL) {
            str = game_str_tbl_sm_pgrowth[p->growth];
        }
        if (p->have_stargate) {
            if (str) {
                lib_strcpy(buf, str, bufsize);
                lib_strcat(buf, " *", bufsize);
                str = buf;
             } else {
                 str = game_str_sm_stargate;
             }
        }
    }
    if (flag_normal_ptr) {
        *flag_normal_ptr = flag_normal;
    }
    return str;
}

static const char *planets_get_dock_str(const struct game_s *g, const struct planet_s *p)
{
    const char *str;
    if (p->buildship == BUILDSHIP_STARGATE) {
        str = game_str_sm_stargate;
    } else {
        str = g->srd[p->owner].design[p->buildship].name;
    }
    return str;
}

static void planets_draw_cb(void *vptr)
{
    struct planets_data_s *d = vptr;
    const struct game_s *g = d->g;
    const empiretechorbit_t *e = &(g->eto[d->api]);
    char buf[64];
    int v;

    ui_draw_color_buf(0x5d);
    for (int i = 0; i < PLANETS_ON_SCREEN; ++i) {
        int pi;
        pi = d->pos + i;
        if ((pi < d->num) && (ui_data.sorted.value[ui_data.sorted.index[pi]] == d->focus_i)) {
            int y0, y1;
            y0 = 21 + i * 11;
            y1 = y0 + 6;
            ui_draw_filled_rect(8, y0, 309, y1, 0xeb, ui_scale);
        }
    }
    lbxgfx_draw_frame(0, 0, d->gfx_report, UI_SCREEN_W, ui_scale);
    lbxgfx_set_new_frame(d->gfx_but_trans, 1);
    lbxgfx_draw_frame(209, 181, d->gfx_but_trans, UI_SCREEN_W, ui_scale);
    ui_draw_filled_rect(213, 159, 245, 162, 0x2f, ui_scale);

    v = g->eto[d->api].tax;
    if (v > 0) {
        ui_draw_slider(213, 160, v * 4, 25, -1, 0x73, ui_scale);
    }

    lbxfont_select(2, 8, 0, 0);
    lbxfont_print_str_normal(210, 170, game_str_pl_reserve, UI_SCREEN_W, ui_scale);
    lbxfont_select(2, 6, 0, 0);
    v = (e->tax * e->total_production_bc) / 2000;
    lib_sprintf(buf, sizeof(buf), "+%i", v);
    lbxfont_print_str_right(272, 159, buf, UI_SCREEN_W, ui_scale);
    lbxfont_print_str_normal(276, 159, game_str_bc, UI_SCREEN_W, ui_scale);
    lbxfont_print_str_normal(276, 170, game_str_bc, UI_SCREEN_W, ui_scale);
    lbxfont_print_num_right(272, 170, e->reserve_bc, UI_SCREEN_W, ui_scale);

    for (int i = 0; i < PLANETS_ON_SCREEN; ++i) {
        int pi;
        pi = d->pos + i;
        if (pi < d->num) {
            int y0;
            const planet_t *p;
            const char *str;
            uint8_t pli;
            pli = ui_data.sorted.value[ui_data.sorted.index[pi]];
            y0 = 21 + i * 11 + 1;   /* di + 1 */
            p = &(g->planet[pli]);
            lbxfont_select(2, 0xb, 0, 0);
            lbxfont_print_num_right(17, y0, ui_data.sorted.index[pi] + 1, UI_SCREEN_W, ui_scale);
            lbxfont_select(2, 0xd, 0, 0);
            lbxfont_print_str_normal(25, y0, p->name, UI_SCREEN_W, ui_scale);
            lbxfont_select(2, 6, 0, 0);
            if (ui_extra_enabled) {
                lbxfont_print_num_right(99, y0, p->pop, UI_SCREEN_W, ui_scale);
                lbxfont_select(2, 1, 0, 0);
                lbxfont_print_num_normal(102, y0, p->max_pop3, UI_SCREEN_W, ui_scale);
            } else {
                lbxfont_print_num_right(83, y0, p->pop, UI_SCREEN_W, ui_scale);
            }
            if (p->pop != p->pop_prev) {
                int v;
                char c;
                uint8_t *gfx = NULL;
                if (p->pop > p->pop_prev) {
                    if (ui_extra_enabled) {
                        lbxfont_select(2, 0xe, 0, 0);
                    } else {
                        gfx = ui_data.gfx.starmap.gr_arrow_u;
                    }
                    v = p->pop - p->pop_prev;
                    c = '+';
                } else {
                    if (ui_extra_enabled) {
                        lbxfont_select(2, 0xb, 0, 0);
                    } else {
                        gfx = ui_data.gfx.starmap.gr_arrow_d;
                    }
                    v = p->pop_prev - p->pop;
                    c = '-';
                }
                lib_sprintf(buf, sizeof(buf), "%c%i", c, v);
                if (ui_extra_enabled) {
                    lbxfont_print_str_right(83, y0, buf, UI_SCREEN_W, ui_scale);
                } else {
                    lbxgfx_draw_frame(92, y0 - 1, gfx, UI_SCREEN_W, ui_scale);
                    lbxfont_print_str_right(111, y0, buf, UI_SCREEN_W, ui_scale);
                }
            }
            lbxfont_select(2, 6, 0, 0);
            if (ui_extra_enabled && p->battlebg == 0) {
                lbxfont_select(2, 1, 0, 0);
                lbxfont_print_str_right(149, y0, "neb", UI_SCREEN_W, ui_scale);
            } else {
                lbxfont_print_str_right(149, y0, game_str_tbl_roman[p->shield], UI_SCREEN_W, ui_scale);
            }
            lbxfont_select(2, 6, 0, 0);
            lbxfont_print_num_right(132, y0, p->factories, UI_SCREEN_W, ui_scale);
            lbxfont_print_num_right(170, y0, p->missile_bases, UI_SCREEN_W, ui_scale);
            if (p->waste) {
                lbxfont_print_num_right(189, y0, p->waste, UI_SCREEN_W, ui_scale);
            }
            lbxfont_select(0, 0xe, 0, 0);
            if (p->unrest == PLANET_UNREST_REBELLION) {
                v = 0;
            } else {
                v = p->total_prod;
            }
            lbxfont_print_num_right(214, y0, v, UI_SCREEN_W, ui_scale);
            lbxfont_select(2, 0xb, 0, 0);
            {
                bool flag_normal;
                str = planets_get_notes_str(g, pli, &flag_normal, buf, sizeof(buf));
                if (flag_normal) {
                    lbxfont_select(2, 1, 0, 0);
                }
                if (str) {
                    lbxfont_print_str_normal(268, y0, str, UI_SCREEN_W, ui_scale);
                }
            }
            if (ui_extra_enabled) {
                lbxfont_select(2, 6, 0, 0);
            }
            if (p->slider[PLANET_SLIDER_SHIP] > 0) {
                str = planets_get_dock_str(g, p);
                lbxfont_print_str_normal(221, y0, str, UI_SCREEN_W, ui_scale);
            }
        }
    }

    lbxfont_select(2, 6, 0, 0);
    game_print_prod_of_total(g, d->api, e->ship_maint_bc, buf, sizeof(buf));
    lbxfont_print_str_right(59, 174, buf, UI_SCREEN_W, ui_scale);
    game_print_prod_of_total(g, d->api, e->bases_maint_bc, buf, sizeof(buf));
    lbxfont_print_str_right(59, 185, buf, UI_SCREEN_W, ui_scale);

    v = 0;
    for (player_id_t i = PLAYER_0; i < PLAYER_NUM; ++i) {
        if (i != d->api) {
            v += e->spying[i];
        }
    }
    lib_sprintf(buf, sizeof(buf), "%i.%i%%", v / 10, v % 10);
    lbxfont_print_str_right(116, 174, buf, UI_SCREEN_W, ui_scale);
    v = e->security;
    lib_sprintf(buf, sizeof(buf), "%i.%i%%", v / 10, v % 10);
    lbxfont_print_str_right(116, 185, buf, UI_SCREEN_W, ui_scale);
    lib_sprintf(buf, sizeof(buf), "%i %s", e->total_trade_bc, game_str_bc);
    lbxfont_print_str_right(195, 174, buf, UI_SCREEN_W, ui_scale);
    lib_sprintf(buf, sizeof(buf), "%i %s", e->total_production_bc, game_str_bc);
    lbxfont_print_str_right(195, 185, buf, UI_SCREEN_W, ui_scale);

    lbxfont_select_set_12_1(5, 8, 0, 0);
    lbxfont_print_str_center(66, 161, game_str_pl_spending, UI_SCREEN_W, ui_scale);
    lbxfont_print_str_center(163, 161, game_str_pl_tincome, UI_SCREEN_W, ui_scale);
}


static void planets_transfer_draw_cb(void *vptr)
{
    struct planets_data_s *d = vptr;
    struct game_s *g = d->g;
    planet_t *p = &(g->planet[d->planet_i]);
    empiretechorbit_t *e = &(g->eto[d->api]);
    const int x = 100, y = 50;
    char buf[64];

    hw_video_copy_back_from_page2();
    lbxgfx_draw_frame(x, y, d->gfx_transfer, UI_SCREEN_W, ui_scale);
    ui_draw_filled_rect(x + 14, y + 35, x + 64, y + 38, 0x2f, ui_scale);
    if (d->amount_trans > 0) {
        ui_draw_slider(x + 14, y + 36, d->amount_trans, 2, -1, 0x74, ui_scale);
    }
    lib_sprintf(buf, sizeof(buf), "%s %s", game_str_pl_resto, p->name);
    lbxfont_select(0, 0xd, 0, 0);
    lbxfont_print_str_center(x + 57, y + 11, game_str_pl_transof, UI_SCREEN_W, ui_scale);
    lbxfont_print_str_center(x + 57, y + 20, buf, UI_SCREEN_W, ui_scale);
    lbxfont_select(2, 6, 0, 0);
    lbxfont_print_str_right(x + 104, y + 35, game_str_bc, UI_SCREEN_W, ui_scale);
    {
        int v;
        v = (d->amount_trans * e->reserve_bc) / 100;
        lbxfont_print_num_right(x + 95, y + 35, v, UI_SCREEN_W, ui_scale);
    }
}

static void ui_planets_transfer(struct planets_data_s *d)
{
    struct game_s *g = d->g;
    planet_t *p = &(g->planet[d->planet_i]);
    int16_t oi_cancel, oi_accept, oi_minus, oi_plus, oi_equals, oi_hash;
    int prod, allreserve, v;
    const int x = 100, y = 50;
    bool flag_done = false;

    d->amount_trans = 0;
    ui_draw_copy_buf();
    ui_cursor_setup_area(1, &ui_cursor_area_tbl[0]);
    lbxgfx_set_new_frame(d->gfx_but_ok, 1);
    lbxgfx_set_new_frame(d->gfx_but_trans, 1);
    lbxgfx_set_new_frame(ui_data.gfx.starmap.reprtbut_up, 1);
    lbxgfx_set_new_frame(ui_data.gfx.starmap.reprtbut_down, 1);
    lbxgfx_draw_frame(292, 159, ui_data.gfx.starmap.reprtbut_up, UI_SCREEN_W, ui_scale);
    lbxgfx_draw_frame(292, 176, ui_data.gfx.starmap.reprtbut_down, UI_SCREEN_W, ui_scale);
    lbxgfx_draw_frame(256, 181, d->gfx_but_ok, UI_SCREEN_W, ui_scale);
    lbxgfx_draw_frame(209, 181, d->gfx_but_trans, UI_SCREEN_W, ui_scale);
    hw_video_copy_back_to_page2();

    prod = p->prod_after_maint - p->reserve;
    SETMAX(prod, 0);
    allreserve = g->eto[d->api].reserve_bc;
    v = allreserve;
    if (v != 0) {
        v = (prod * 100) / v;
    }

    uiobj_table_clear();
    oi_cancel = uiobj_add_t0(x + 10, y + 47, "", ui_data.gfx.starmap.reloc_bu_cancel, MOO_KEY_LEFT); /* FIXME key == "\x01xb" ?? */
    oi_accept = uiobj_add_t0(x + 66, y + 47, "", ui_data.gfx.starmap.reloc_bu_accept, MOO_KEY_SPACE);
    uiobj_add_slider_int(x + 14, y + 35, 0, 100, 50, 9, &d->amount_trans);
    oi_minus = uiobj_add_mousearea(x + 10, y + 33, x + 12, y + 41, MOO_KEY_UNKNOWN);
    oi_plus = uiobj_add_mousearea(x + 66, y + 33, x + 70, y + 41, MOO_KEY_UNKNOWN);
    oi_equals = uiobj_add_inputkey(MOO_KEY_EQUALS);
    oi_hash = uiobj_add_inputkey(MOO_KEY_HASH);

    uiobj_set_callback_and_delay(planets_transfer_draw_cb, d, 1);

    while (!flag_done) {
        int16_t oi;
        oi = uiobj_handle_input_cond();
        ui_delay_prepare();
        if ((oi == oi_cancel) || (oi == UIOBJI_ESC)) {
            ui_sound_play_sfx_06();
            flag_done = true;
        } else if (oi == oi_accept) {
            ui_sound_play_sfx_24();
            v = (d->amount_trans * allreserve) / 100;
            p->reserve += v;
            allreserve -= v;
            SETMAX(allreserve, 0);
            g->eto[d->api].reserve_bc = allreserve;
            flag_done = true;
        } else if (oi == oi_minus) {
            ui_sound_play_sfx_24();
            d->amount_trans -= 2;
            SETMAX(d->amount_trans, 0);
        } else if (oi == oi_plus) {
            ui_sound_play_sfx_24();
            d->amount_trans += 2;
            SETMIN(d->amount_trans, 100);
        } else if (oi == oi_equals || oi == oi_hash) {
            ui_sound_play_sfx_24();
            d->amount_trans = v;
            SETRANGE(d->amount_trans, 0, 100);
        }
        if (!flag_done) {
            planets_transfer_draw_cb(d);
            ui_draw_finish();
            ui_delay_ticks_or_click(1);
        }
    }
}

/* -------------------------------------------------------------------------- */

enum {
    UI_SORT_INDEX = 0,
    UI_SORT_NAME,
    UI_SORT_GROWTH,
    UI_SORT_POP,
    UI_SORT_MAXPOP,
    UI_SORT_FACT,
    UI_SORT_SHIELD,
    UI_SORT_BASE,
    UI_SORT_WASTE,
    UI_SORT_PROD,
    UI_SORT_DOCK,
    UI_SORT_NOTES,
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

#define UI_SORT_CMP_VALUE(_v0_, _v1_) (((_v0_) != (_v1_)) ? ((_v0_) - (_v1_)) : (i1 - i0))
#define UI_SORT_CMP_VARIABLE(_var_) UI_SORT_CMP_VALUE(p0->_var_, p1->_var_)

static int planets_sort_inc_index(const void *ptr0, const void *ptr1)
{
    uint16_t i0 = *((uint16_t const *)ptr0);
    uint16_t i1 = *((uint16_t const *)ptr1);
    return i0 - i1;
}

static int planets_sort_dec_index(const void *ptr0, const void *ptr1)
{
    return planets_sort_inc_index(ptr1, ptr0);
}

static int planets_sort_inc_name(const void *ptr0, const void *ptr1)
{
    UI_SORT_SETUP();
    return strcmp(p0->name, p1->name);
}

static int planets_sort_dec_name(const void *ptr0, const void *ptr1)
{
    return planets_sort_inc_name(ptr1, ptr0);
}

static int planets_sort_inc_pop(const void *ptr0, const void *ptr1)
{
    UI_SORT_SETUP();
    return UI_SORT_CMP_VARIABLE(pop);
}

static int planets_sort_dec_pop(const void *ptr0, const void *ptr1)
{
    return planets_sort_inc_pop(ptr1, ptr0);
}

static int planets_sort_inc_max_pop(const void *ptr0, const void *ptr1)
{
    UI_SORT_SETUP();
    return UI_SORT_CMP_VARIABLE(max_pop3);
}

static int planets_sort_dec_max_pop(const void *ptr0, const void *ptr1)
{
    return planets_sort_inc_max_pop(ptr1, ptr0);
}

static int planets_sort_inc_growth(const void *ptr0, const void *ptr1)
{
    UI_SORT_SETUP();
    int v0 = p0->pop - p0->pop_prev;
    int v1 = p1->pop - p1->pop_prev;
    return UI_SORT_CMP_VALUE(v0, v1);
}

static int planets_sort_dec_growth(const void *ptr0, const void *ptr1)
{
    return planets_sort_inc_growth(ptr1, ptr0);
}

static int planets_sort_inc_fact(const void *ptr0, const void *ptr1)
{
    UI_SORT_SETUP();
    return UI_SORT_CMP_VARIABLE(factories);
}

static int planets_sort_dec_fact(const void *ptr0, const void *ptr1)
{
    return planets_sort_inc_fact(ptr1, ptr0);
}

static int planets_sort_inc_shield(const void *ptr0, const void *ptr1)
{
    UI_SORT_SETUP();
    int v0 = p0->battlebg ? p0->shield : -1;
    int v1 = p1->battlebg ? p1->shield : -1;
    return UI_SORT_CMP_VALUE(v0, v1);
}

static int planets_sort_dec_shield(const void *ptr0, const void *ptr1)
{
    return planets_sort_inc_shield(ptr1, ptr0);
}

static int planets_sort_inc_base(const void *ptr0, const void *ptr1)
{
    UI_SORT_SETUP();
    return UI_SORT_CMP_VARIABLE(missile_bases);
}

static int planets_sort_dec_base(const void *ptr0, const void *ptr1)
{
    return planets_sort_inc_base(ptr1, ptr0);
}

static int planets_sort_inc_waste(const void *ptr0, const void *ptr1)
{
    UI_SORT_SETUP();
    return UI_SORT_CMP_VARIABLE(waste);
}

static int planets_sort_dec_waste(const void *ptr0, const void *ptr1)
{
    return planets_sort_inc_waste(ptr1, ptr0);
}

static int planets_sort_inc_prod(const void *ptr0, const void *ptr1)
{
    UI_SORT_SETUP();
    int v0 = (p0->unrest == PLANET_UNREST_REBELLION) ? 0 : p0->total_prod;
    int v1 = (p1->unrest == PLANET_UNREST_REBELLION) ? 0 : p1->total_prod;
    return UI_SORT_CMP_VALUE(v0, v1);
}

static int planets_sort_dec_prod(const void *ptr0, const void *ptr1)
{
    return planets_sort_inc_prod(ptr1, ptr0);
}

static int planets_sort_inc_dock(const void *ptr0, const void *ptr1)
{
    UI_SORT_SETUP();
    const char *s0 = (p0->slider[PLANET_SLIDER_SHIP] > 0) ? planets_get_dock_str(g, p0) : "\xff";
    const char *s1 = (p1->slider[PLANET_SLIDER_SHIP] > 0) ? planets_get_dock_str(g, p1) : "\xff";
    return UI_SORT_CMP_VALUE(strcmp(s1, s0), 0);
}

static int planets_sort_dec_dock(const void *ptr0, const void *ptr1)
{
    return planets_sort_inc_dock(ptr1, ptr0);
}

static int planets_sort_inc_notes(const void *ptr0, const void *ptr1)
{
    UI_SORT_SETUP();
    char buf0[64];
    char buf1[64];
    bool normal0, normal1;
    const char *s0 = planets_get_notes_str(g, pli0, &normal0, buf0, sizeof(buf0));
    const char *s1 = planets_get_notes_str(g, pli1, &normal1, buf1, sizeof(buf1));
    int d;
    if (normal1 && !normal0) {
        d = 1;
    } else if (normal0 && !normal1) {
        d = -1;
    } else if (!normal0 && !normal1) {
        d = UI_SORT_CMP_VALUE(strcmp(s1, s0), 0);
    } else {
        if (p0->special > p1->special) {
            d = 1;
        } else if (p0->special < p1->special) {
            d = -1;
        } else {
            d = UI_SORT_CMP_VARIABLE(growth);
        }
    }
    return d;
}

static int planets_sort_dec_notes(const void *ptr0, const void *ptr1)
{
    return planets_sort_inc_notes(ptr1, ptr0);
}

typedef int sort_cb_t(const void *, const void *);

static sort_cb_t * const sort_cb_tbl[UI_SORT_NUM * 2] = {
    planets_sort_inc_index,
    planets_sort_dec_index,
    planets_sort_inc_name,
    planets_sort_dec_name,
    planets_sort_dec_growth,
    planets_sort_inc_growth,
    planets_sort_dec_pop,
    planets_sort_inc_pop,
    planets_sort_dec_max_pop,
    planets_sort_inc_max_pop,
    planets_sort_dec_fact,
    planets_sort_inc_fact,
    planets_sort_dec_shield,
    planets_sort_inc_shield,
    planets_sort_dec_base,
    planets_sort_inc_base,
    planets_sort_dec_waste,
    planets_sort_inc_waste,
    planets_sort_dec_prod,
    planets_sort_inc_prod,
    planets_sort_dec_dock,
    planets_sort_inc_dock,
    planets_sort_dec_notes,
    planets_sort_inc_notes
};

static void ui_planets_sort(struct planets_data_s *d)
{
    qsort(ui_data.sorted.index, d->num, sizeof(ui_data.sorted.index[0]), sort_cb_tbl[d->order_i]);
    ui_data.sorted.planets_order[d->api] = d->order_i;
    for (int i = 0; i < d->num; ++i) {
        int index = ui_data.sorted.index[i];
        if (ui_data.sorted.value[index] == d->focus_i) {
            d->pos = i - 5;
            break;
        }
    }
    if ((d->pos + PLANETS_ON_SCREEN) >= d->num) {
        d->pos = d->num - PLANETS_ON_SCREEN;
    }
    SETMAX(d->pos, 0);
}

/* -------------------------------------------------------------------------- */

void ui_planets(struct game_s *g, player_id_t active_player)
{
    struct planets_data_s d;
    bool flag_done = false, flag_trans;
    int16_t oi_alt_moola, oi_up, oi_down, oi_wheel, oi_ok, oi_trans, oi_minus, oi_plus, oi_tbl_planets[PLANETS_ON_SCREEN];
    int16_t oi_sort[UI_SORT_NUM];
    uint8_t tbl_onscreen_planets[PLANETS_ON_SCREEN];
    int16_t scroll = 0;

    load_pl_data(&d);
    uiobj_set_help_id(37);

    d.g = g;
    d.api = active_player;
    d.focus_i = g->planet_focus_i[active_player];
    d.order_i = 0;
    d.pos = 0;
    d.num = 0;

    ui_data.sorted.g = g;
    for (int i = 0; i < g->galaxy_stars; ++i) {
        if (g->planet[i].owner == active_player) {
            if (i == d.focus_i) {
                d.pos = d.num - 5;  /* WASBUG MOO1 uses i - 5 */
            }
            ui_data.sorted.index[d.num] = d.num;
            ui_data.sorted.value[d.num++] = i;
        }
    }

    if (ui_extra_enabled) {
        d.order_i = ui_data.sorted.planets_order[active_player];
        ui_planets_sort(&d);
    }

again:
    flag_trans = false;
    game_update_production(g); /* this is needed for correct shown production after transfers */

    oi_up = UIOBJI_INVALID;
    oi_down = UIOBJI_INVALID;
    oi_wheel = UIOBJI_INVALID;
    oi_ok = UIOBJI_INVALID;
    oi_trans = UIOBJI_INVALID;
    oi_minus = UIOBJI_INVALID;
    oi_plus = UIOBJI_INVALID;
    for (int i = 0; i < PLANETS_ON_SCREEN; ++i) {
        tbl_onscreen_planets[i] = 0;
        oi_tbl_planets[i] = UIOBJI_INVALID;
    }
    UIOBJI_SET_TBL_INVALID(oi_sort);

    uiobj_set_callback_and_delay(planets_draw_cb, &d, 1);

    uiobj_table_clear();
    oi_alt_moola = uiobj_add_alt_str("moola");

    while (!flag_done) {
        int16_t oi;
        oi = uiobj_handle_input_cond();
        ui_delay_prepare();
        if ((oi == oi_ok) || (oi == UIOBJI_ESC)) {
            ui_sound_play_sfx_06();
            flag_done = true;
        } else if (oi == oi_up) {
            ui_sound_play_sfx_24();
            d.pos -= PLANETS_ON_SCREEN;
        } else if (oi == oi_down) {
            ui_sound_play_sfx_24();
            d.pos += PLANETS_ON_SCREEN;
        } else if (oi == oi_wheel) {
            d.pos += scroll;
            scroll = 0;
        }
        for (int i = 0; i < PLANETS_ON_SCREEN; ++i) {
            if (oi == oi_tbl_planets[i]) {
                ui_sound_play_sfx_24();
                if (!flag_trans) {
                    g->planet_focus_i[active_player] = tbl_onscreen_planets[i];
                    flag_done = true;
                } else {
                    d.planet_i = tbl_onscreen_planets[i];
                    ui_planets_transfer(&d);
                    ui_cursor_setup_area(1, &ui_cursor_area_tbl[0]);
                    goto again;
                }
            }
        }
        if (oi == oi_alt_moola) {
            if (game_cheat_moola(g, active_player)) {
                ui_sound_play_sfx_24();
            }
        }
        if ((d.pos + PLANETS_ON_SCREEN) >= d.num) {
            d.pos = d.num - PLANETS_ON_SCREEN;
        }
        SETMAX(d.pos, 0);
        if (oi == oi_minus) {
            ui_sound_play_sfx_24();
            g->eto[active_player].tax -= 5;
            SETMAX(g->eto[active_player].tax, 0);
        } else if (oi == oi_plus) {
            ui_sound_play_sfx_24();
            g->eto[active_player].tax += 5;
            SETMIN(g->eto[active_player].tax, 200);
        } else if (oi == oi_trans) {
            ui_sound_play_sfx_24();
            flag_trans = true;
        }
        for (int i = 0; i < UI_SORT_NUM; ++i) {
            if (oi == oi_sort[i]) {
                int v;
                v = i * 2;
                if (v == d.order_i) {
                    ++v;
                }
                d.order_i = v;
                ui_planets_sort(&d);
            }
        }
        ui_cursor_setup_area(1, &ui_cursor_area_tbl[flag_trans ? 9 : 0]);
        if (!flag_done) {
            uiobj_table_set_last(oi_alt_moola);
            oi_up = uiobj_add_t0(292, 159, "", ui_data.gfx.starmap.reprtbut_up, MOO_KEY_COMMA);
            oi_down = uiobj_add_t0(292, 176, "", ui_data.gfx.starmap.reprtbut_down, MOO_KEY_PERIOD);
            oi_ok = uiobj_add_t0(256, 181, "", d.gfx_but_ok, MOO_KEY_o);
            for (int i = 0; i < PLANETS_ON_SCREEN; ++i) {
                int pi, y0, y1;
                pi = i + d.pos;
                if (pi < d.num) {
                    tbl_onscreen_planets[i] = ui_data.sorted.value[ui_data.sorted.index[pi]];
                    y0 = 21 + i * 11;
                    y1 = y0 + 8;
                    oi_tbl_planets[i] = uiobj_add_mousearea(7, y0, 248, y1, MOO_KEY_UNKNOWN);
                }
            }
            if (ui_extra_enabled) {
                const int x0[UI_SORT_NUM] = {  8, 23, 71,  87, 101, 117, 138, 155, 176, 194, 220, 267 };
                const int x1[UI_SORT_NUM] = { 20, 60, 86,  99, 113, 134, 151, 172, 192, 216, 262, 310 };
                const int y0 = 8, y1 = 16;
                for (int i = 0; i < UI_SORT_NUM; ++i) {
                    oi_sort[i] = uiobj_add_mousearea(x0[i], y0, x1[i], y1, MOO_KEY_UNKNOWN);
                }
            }
            oi_wheel = uiobj_add_mousewheel(0, 0, 319, 159, &scroll);
            oi_trans = UIOBJI_INVALID;
            if (!flag_trans) {
                if (g->eto[active_player].reserve_bc != 0) {
                    oi_trans = uiobj_add_t0(209, 181, "", d.gfx_but_trans, MOO_KEY_t);
                }
                uiobj_add_slider_int(213, 160, 0, 200, 32, 9, &g->eto[active_player].tax);
                oi_minus = uiobj_add_mousearea(208, 157, 211, 165, MOO_KEY_UNKNOWN);
                oi_plus = uiobj_add_mousearea(247, 157, 251, 165, MOO_KEY_UNKNOWN);
            } else {
                oi_minus = UIOBJI_INVALID;
                oi_plus = UIOBJI_INVALID;
            }
            planets_draw_cb(&d); /* FIXME not needed in original */
            ui_draw_finish();
            ui_delay_ticks_or_click(1);
        }
    }

    uiobj_unset_callback();
    uiobj_set_help_id(-1);
    free_pl_data(&d);
}
