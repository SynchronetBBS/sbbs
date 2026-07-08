#include "config.h"

#include <ctype.h>
#include <stdio.h>

#include "ui.h"
#include "comp.h"
#include "game.h"
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

struct sabotage_data_s {
    struct game_s *g;
    player_id_t api;
    player_id_t spy;
    player_id_t target;
    player_id_t other1;
    player_id_t other2;
    ui_sabotage_t act;
    int snum;
    uint8_t planet;
    uint8_t *gfx_saboback;
    uint8_t *gfx_sabobac2;
    uint8_t *gfx_butt_revolt;
    uint8_t *gfx_butt_bases;
    uint8_t *gfx_butt_ind;
    uint8_t *gfx_basexplo;
    uint8_t *gfx_ind_expl;
    uint8_t *gfx_framing;
    uint8_t *gfx_contbutt;
    uint8_t *gfx_contback;
    uint8_t *gfx_colony;
    void *gmap;
};

static void sabotage_load_data(struct sabotage_data_s *d)
{
    d->gfx_saboback = lbxfile_item_get(LBXFILE_BACKGRND, 0x5);
    d->gfx_sabobac2 = lbxfile_item_get(LBXFILE_BACKGRND, 0x32);
    d->gfx_butt_revolt = lbxfile_item_get(LBXFILE_BACKGRND, 0x33);
    d->gfx_butt_bases = lbxfile_item_get(LBXFILE_BACKGRND, 0x6);
    d->gfx_butt_ind = lbxfile_item_get(LBXFILE_BACKGRND, 0xa);
    d->gfx_basexplo = lbxfile_item_get(LBXFILE_BACKGRND, 0x7);
    d->gfx_ind_expl = lbxfile_item_get(LBXFILE_BACKGRND, 0x8);
    d->gfx_framing = lbxfile_item_get(LBXFILE_BACKGRND, 0x9);
    d->gfx_contbutt = lbxfile_item_get(LBXFILE_BACKGRND, 0xc);
    d->gfx_contback = lbxfile_item_get(LBXFILE_BACKGRND, 0xb);
    d->gfx_colony = lbxfile_item_get(LBXFILE_COLONIES, d->g->planet[d->planet].type * 2 + 1);
}

static void sabotage_free_data(struct sabotage_data_s *d)
{
    lbxfile_item_release(LBXFILE_BACKGRND, d->gfx_saboback);
    lbxfile_item_release(LBXFILE_BACKGRND, d->gfx_sabobac2);
    lbxfile_item_release(LBXFILE_BACKGRND, d->gfx_butt_revolt);
    lbxfile_item_release(LBXFILE_BACKGRND, d->gfx_butt_bases);
    lbxfile_item_release(LBXFILE_BACKGRND, d->gfx_butt_ind);
    lbxfile_item_release(LBXFILE_BACKGRND, d->gfx_basexplo);
    lbxfile_item_release(LBXFILE_BACKGRND, d->gfx_ind_expl);
    lbxfile_item_release(LBXFILE_BACKGRND, d->gfx_framing);
    lbxfile_item_release(LBXFILE_BACKGRND, d->gfx_contbutt);
    lbxfile_item_release(LBXFILE_BACKGRND, d->gfx_contback);
    lbxfile_item_release(LBXFILE_COLONIES, d->gfx_colony);
}

static void sabotage_draw_cb(void *vptr)
{
    struct sabotage_data_s *d = vptr;
    const struct game_s *g = d->g;
    const empiretechorbit_t *e = &(g->eto[d->target]);
    const planet_t *p = &(g->planet[d->planet]);
    int pop, bases, fact;
    hw_video_copy_back_from_page2();
    ui_draw_filled_rect(222, 4, 314, 179, 0, ui_scale);
    lbxgfx_draw_frame(222, 4, d->gfx_sabobac2, UI_SCREEN_W, ui_scale);
    ui_starmap_draw_planetinfo_2(g, d->api, d->target, d->planet);
    /*set_limits(228, 110, 309, 143);*/
    lbxgfx_draw_frame_offs(228, 70, d->gfx_colony, 228, 110, 309, 143, UI_SCREEN_W, ui_scale);
    ui_draw_filled_rect(228, 58, 309, 68, tbl_banner_color2[e->banner], ui_scale);
    lbxfont_select(5, 6, 0, 0);
    lbxfont_print_str_center(269, 60, game_str_tbl_races[e->race], UI_SCREEN_W, ui_scale);
    ui_draw_filled_rect(227, 73, 310, 105, 0, ui_scale);
    ui_draw_filled_rect(228, 74, 309, 104, 7, ui_scale);
    lbxfont_select_set_12_1(5, 0, 0, 0);
    lbxfont_print_str_split(228, 79, 80, game_str_sb_choose, 2, UI_SCREEN_W, UI_SCREEN_H, ui_scale);
    /*game_update_visibility();*/
    ui_gmap_basic_draw_frame(d->gmap, d->api);
    lbxfont_select_set_12_4(0, 0xa, 0, 0);
    if (BOOLVEC_IS1(p->within_srange, d->api)) {
        pop = p->pop;
        bases = p->missile_bases;
        fact = p->factories;
    } else {
        pop = g->seen[d->api][d->planet].pop;
        bases = g->seen[d->api][d->planet].bases;
        fact = g->seen[d->api][d->planet].factories;
        lbxfont_print_str_normal(230, 111, game_str_sb_lastrep, UI_SCREEN_W, ui_scale);
    }
    /*6d2b8*/
    if (pop > 0) {
        lbxfont_print_str_normal(230, 120, game_str_sb_pop, UI_SCREEN_W, ui_scale);
        lbxfont_print_num_right(305, 120, pop, UI_SCREEN_W, ui_scale);
    }
    if (fact > 0) {
        lbxfont_print_str_normal(230, 128, game_str_sb_fact, UI_SCREEN_W, ui_scale);
        lbxfont_print_num_right(305, 128, fact, UI_SCREEN_W, ui_scale);
    }
    if (bases > 0) {
        lbxfont_print_str_normal(230, 136, game_str_sb_bases, UI_SCREEN_W, ui_scale);
        lbxfont_print_num_right(305, 136, bases, UI_SCREEN_W, ui_scale);
    }
    ui_gmap_draw_planet_border(g, d->planet);
    for (int i = 0; i < g->galaxy_stars; ++i) {
        const planet_t *p = &(g->planet[i]);
        if (p->owner == d->target) {
            int x, y;
            x = (p->x * 215) / g->galaxy_maxx + 5 + 3;
            y = (p->y * 171) / g->galaxy_maxy + 5 - 2;
            lbxgfx_draw_frame(x, y, ui_data.gfx.starmap.smalflag[e->banner], UI_SCREEN_W, ui_scale);
        }
    }
}

static void sabotage_draw_anim(uint8_t *gfx, bool anim, int soundframe)
{
    int frame = anim ? lbxgfx_get_frame(gfx) : 0;
    lbxgfx_set_frame_0(gfx);
    for (int f = 0; f <= frame; ++f) {
        lbxgfx_draw_frame(227, 73, gfx, UI_SCREEN_W, ui_scale);
    }
    if (frame == soundframe) {
        ui_sound_play_sfx(0x11);
    }
}

static void sabotage_done_draw_cb(void *vptr)
{
    struct sabotage_data_s *d = vptr;
    const struct game_s *g = d->g;
    const empiretechorbit_t *e = &(g->eto[d->target]);
    const planet_t *p = &(g->planet[d->planet]);
    char *buf = ui_data.strbuf;
    struct strbuild_s strbuild = strbuild_init(buf, UI_STRBUF_SIZE);
    hw_video_copy_back_from_page2();
    ui_draw_filled_rect(222, 4, 314, 179, 0, ui_scale);
    lbxgfx_draw_frame(222, 4, d->gfx_saboback, UI_SCREEN_W, ui_scale);
    lbxgfx_draw_frame(222, 159, d->gfx_contback, UI_SCREEN_W, ui_scale);
    /*set_limits(228, 110, 309, 158);*/
    lbxgfx_draw_frame_offs(228, 70, d->gfx_colony, 228, 110, 309, 158, UI_SCREEN_W, ui_scale);
    switch (d->act) {
        case UI_SABOTAGE_FACT: /*0*/
            sabotage_draw_anim(d->gfx_ind_expl, (d->snum > 0), 11);
            break;
        case UI_SABOTAGE_BASES: /*1*/
            sabotage_draw_anim(d->gfx_basexplo, (d->snum > 0), 5);
            break;
        case UI_SABOTAGE_REVOLT: /*2*/
            sabotage_draw_anim(d->gfx_ind_expl, false, 100);
            break;
        default:
            break;
    }
    ui_starmap_draw_planetinfo_2(g, d->api, d->target, d->planet);
    ui_draw_filled_rect(228, 58, 309, 68, tbl_banner_color2[e->banner], ui_scale);
    lbxfont_select(5, 6, 0, 0);
    lbxfont_print_str_center(269, 60, game_str_tbl_races[e->race], UI_SCREEN_W, ui_scale);
    if (d->spy == PLAYER_NONE) {
        strbuild_catf(&strbuild, "%s ", game_str_sb_unkn);
    } else {
        strbuild_catf(&strbuild, "%s %s ", (d->spy == d->api) ? game_str_sb_your : game_str_tbl_race[g->eto[d->spy].race], game_str_sb_spies);
    }
    if (d->snum > 0) {
        switch (d->act) {
            default:
            case UI_SABOTAGE_FACT: /*0*/
                strbuild_catf(&strbuild, "%s %i %s", game_str_sb_destr, d->snum, (d->snum == 1) ? game_str_sb_fact2 : game_str_sb_facts);
                break;
            case UI_SABOTAGE_BASES: /*1*/
                strbuild_catf(&strbuild, "%s %i %s", game_str_sb_destr, d->snum, (d->snum == 1) ? game_str_sb_mbase : game_str_sb_mbases);
                break;
            case UI_SABOTAGE_REVOLT: /*2*/
                if (p->unrest == PLANET_UNREST_REBELLION) {
                    strbuild_catf(&strbuild, "%s", game_str_sb_increv);
                } else {
                    int v = (p->pop == 0) ? 0 : ((p->rebels * 100) / p->pop);
                    strbuild_catf(&strbuild, "%s %i %s %i%%.", game_str_sb_inc1, d->snum, game_str_sb_inc2, v);
                }
                break;
        }
    } else {
        /*6da9e*/
        strbuild_catf(&strbuild, "%s", game_str_sb_failed);
        switch (d->act) {
            default:
            case UI_SABOTAGE_FACT: /*0*/
                if (p->factories == 0) {
                    lib_sprintf(buf, UI_STRBUF_SIZE, "%s", game_str_sb_nofact);
                }
                break;
            case UI_SABOTAGE_BASES: /*1*/
                if (p->missile_bases == 0) {
                    lib_sprintf(buf, UI_STRBUF_SIZE, "%s", game_str_sb_nobases);
                }
                break;
            case UI_SABOTAGE_REVOLT: /*2*/
                lib_strcat(buf, game_str_sb_noinc, UI_STRBUF_SIZE); /* FIXME never happens? */
                break;
        }
    }
    /*6db14*/
    lbxfont_select_set_12_4(5, 5, 0, 0);
    lbxfont_print_str_split(228, 118, 84, buf, 2, UI_SCREEN_W, UI_SCREEN_H, ui_scale);
    ui_gmap_basic_draw_frame(d->gmap, d->api);
    ui_gmap_draw_planet_border(g, d->planet);
    lbxgfx_set_new_frame(d->gfx_contbutt, 1);
    lbxgfx_draw_frame(227, 163, d->gfx_contbutt, UI_SCREEN_W, ui_scale);
    if (d->other2 != PLAYER_NONE) {
        ui_draw_filled_rect(31, 62, 202, 125, 0xbb, ui_scale);
        ui_draw_filled_rect(37, 68, 196, 91, 0xba, ui_scale);
        lbxgfx_draw_frame(31, 62, d->gfx_framing, UI_SCREEN_W, ui_scale);
        ui_draw_filled_rect(50, 106, 110, 120, 0x00, ui_scale);
        ui_draw_filled_rect(51, 107, 109, 119, tbl_banner_color2[g->eto[d->other1].banner], ui_scale);
        ui_draw_filled_rect(122, 106, 183, 120, 0x00, ui_scale);
        ui_draw_filled_rect(123, 107, 182, 119, tbl_banner_color2[g->eto[d->other2].banner], ui_scale);
        lbxfont_select(5, 6, 0, 0);
        lbxfont_print_str_center(80, 110, game_str_tbl_races[g->eto[d->other1].race], UI_SCREEN_W, ui_scale);
        lbxfont_print_str_center(152, 110, game_str_tbl_races[g->eto[d->other2].race], UI_SCREEN_W, ui_scale);
        lbxfont_select(5, 6, 0, 0);
        lbxfont_set_gap_h(2);
        lbxfont_print_str_split(40, 70, 154, game_str_sb_frame, 3, UI_SCREEN_W, UI_SCREEN_H, ui_scale);
        lbxfont_select(0, 0, 0, 0);
        lbxfont_print_str_center(115, 96, game_str_nt_victim, UI_SCREEN_W, ui_scale);
    }
}

/* -------------------------------------------------------------------------- */

ui_sabotage_t ui_spy_sabotage_ask(struct game_s *g, int spy, int target, uint8_t *planetptr)
{
    struct sabotage_data_s d;
    int16_t oi_bases, oi_ind, oi_revolt, oi_planet[PLANETS_MAX];
    bool flag_done = false;
    ui_sabotage_t action = UI_SABOTAGE_NONE;
    ui_switch_1(g, spy);
    d.g = g;
    d.api = spy;
    d.spy = spy;
    d.target = target;
    d.planet = PLANET_NONE;
    d.gmap = ui_gmap_basic_init(g, true);
    for (int i = 0; i < g->galaxy_stars; ++i) {
        if (g->planet[i].owner == target) {
            d.planet = i;
            break;
        }
    }
    if (d.planet == PLANET_NONE) {
        *planetptr = 0;
        return UI_SABOTAGE_NONE;
    }
    sabotage_load_data(&d);
    ui_sound_play_music(0x10);
    uiobj_table_clear();
    oi_bases = UIOBJI_INVALID;
    oi_ind = UIOBJI_INVALID;
    oi_revolt = UIOBJI_INVALID;
    UIOBJI_SET_TBL_INVALID(oi_planet);
    uiobj_set_callback_and_delay(sabotage_draw_cb, &d, 4);
    while (!flag_done) {
        int16_t oi;
        ui_delay_prepare();
        oi = uiobj_handle_input_cond();
        if (oi != 0) {
            ui_sound_play_sfx_24();
        }
        if (oi == UIOBJI_ESC) {
            flag_done = true;
            action = UI_SABOTAGE_NONE;
        }
        if (oi == oi_bases) {
            flag_done = true;
            action = UI_SABOTAGE_BASES;
        }
        if (oi == oi_ind) {
            flag_done = true;
            action = UI_SABOTAGE_FACT;
        }
        if (oi == oi_revolt) {
            flag_done = true;
            action = UI_SABOTAGE_REVOLT;
        }
        for (int i = 0; i < g->galaxy_stars; ++i) {
            if (oi == oi_planet[i]) {
                d.planet = i;
                lbxfile_item_release(LBXFILE_COLONIES, d.gfx_colony);
                d.gfx_colony = lbxfile_item_get(LBXFILE_COLONIES, g->planet[i].type * 2 + 1);
                break;
            }
        }
        uiobj_table_clear();
        oi_bases = uiobj_add_t0(227, 147, "", d.gfx_butt_bases, MOO_KEY_b);
        oi_ind = uiobj_add_t0(267, 147, "", d.gfx_butt_ind, MOO_KEY_f);
        if (g->planet[d.planet].unrest == PLANET_UNREST_NORMAL) {
            oi_revolt = uiobj_add_t0(227, 163, "", d.gfx_butt_revolt, MOO_KEY_i);
        } else {
            oi_revolt = UIOBJI_INVALID;
        }
        for (int i = 0; i < g->galaxy_stars; ++i) {
            const planet_t *p = &(g->planet[i]);
            if (p->owner == target) {
                int x, y;
                x = (p->x * 215) / g->galaxy_maxx + 5;
                y = (p->y * 171) / g->galaxy_maxy + 5;
                oi_planet[i] = uiobj_add_mousearea(x - 1, y - 1, x + 7, y + 7, MOO_KEY_UNKNOWN);
            } else {
                oi_planet[i] = UIOBJI_INVALID;
            }
        }
        sabotage_draw_cb(&d);
        ui_draw_finish();
        ui_delay_ticks_or_click(4);
    }
    uiobj_unset_callback();
    uiobj_table_clear();
    ui_sound_stop_music();
    sabotage_free_data(&d);
    *planetptr = d.planet;
    return action;
}

int ui_spy_sabotage_done(struct game_s *g, int pi, int spy, int target, ui_sabotage_t act, int other1, int other2, uint8_t planet, int snum)
{
    struct sabotage_data_s d;
    int16_t oi_cont, oi_cont2, oi_other1, oi_other2;
    bool flag_done = false;
    int other = PLAYER_NONE;
    ui_switch_1(g, pi);
    d.g = g;
    d.api = pi;
    d.spy = spy;
    d.target = target;
    d.other1 = other1;
    d.other2 = other2;
    d.planet = planet;
    d.act = act;
    d.snum = snum;
    d.gmap = ui_gmap_basic_init(g, true);
    sabotage_load_data(&d);
    if (snum > 0) {
        ui_sound_play_music(0x11);
    }
    lbxgfx_set_frame_0(d.gfx_basexplo);
    lbxgfx_set_frame_0(d.gfx_ind_expl);
    uiobj_table_clear();
    oi_cont = UIOBJI_INVALID;
    oi_cont2 = UIOBJI_INVALID;
    oi_other1 = UIOBJI_INVALID;
    oi_other2 = UIOBJI_INVALID;
    if (other2 == PLAYER_NONE) {
        oi_cont = uiobj_add_t0(227, 163, "", d.gfx_contbutt, MOO_KEY_c);
        oi_cont2 = uiobj_add_inputkey(MOO_KEY_SPACE);
    } else {
        oi_other1 = uiobj_add_mousearea(50, 106, 110, 120, MOO_KEY_UNKNOWN);
        oi_other2 = uiobj_add_mousearea(122, 106, 183, 120, MOO_KEY_UNKNOWN);
    }
    uiobj_set_callback_and_delay(sabotage_done_draw_cb, &d, 4);
    while (!flag_done) {
        int16_t oi;
        ui_delay_prepare();
        oi = uiobj_handle_input_cond();
        if ((oi == UIOBJI_ESC) || (oi == oi_cont) || (oi == oi_cont2)) {
            ui_sound_play_sfx_24();
            flag_done = true;
        }
        if (oi == oi_other1) {
            ui_sound_play_sfx_24();
            flag_done = true;
            other = other1;
        }
        if (oi == oi_other2) {
            ui_sound_play_sfx_24();
            flag_done = true;
            other = other2;
        }
        sabotage_done_draw_cb(&d);
        ui_draw_finish();
        ui_delay_ticks_or_click(4);
    }
    uiobj_unset_callback();
    uiobj_table_clear();
    ui_sound_stop_music();
    sabotage_free_data(&d);
    return other;
}
