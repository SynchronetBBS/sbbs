#include "config.h"

#include <stdio.h>

#include "uigmap.h"
#include "comp.h"
#include "game.h"
#include "game_misc.h"
#include "game_str.h"
#include "hw.h"
#include "kbd.h"
#include "mouse.h"
#include "lbx.h"
#include "lbxfont.h"
#include "lbxgfx.h"
#include "lbxpal.h"
#include "lib.h"
#include "log.h"
#include "rnd.h"
#include "types.h"
#include "ui.h"
#include "uidelay.h"
#include "uidefs.h"
#include "uidraw.h"
#include "uiobj.h"
#include "uipal.h"
#include "uisound.h"
#include "uistarmap.h"
#include "uistarmap_common.h"
#include "uiswitch.h"

/* -------------------------------------------------------------------------- */

#define GMAP_LIMITS  7, 7, 230, 191
#define GMAP_SCALED_LIMITS  7 * ui_scale, 7 * ui_scale, 230 * ui_scale, 191 * ui_scale

struct gmap_blink_data_s {
    struct game_s *g;
    uint8_t planet_i;
    int countdown;
};

struct gmap_data_s {
    const struct game_s *g;
    struct gmap_blink_data_s b;
    player_id_t api;
    int16_t mode;
    uint8_t *gfx_mapview;
    uint8_t *gfx_but_col;
    uint8_t *gfx_but_env;
    uint8_t *gfx_but_min;
    uint8_t *gfx_but_ok;
};

struct gmap_basic_data_s {
    struct game_s *g;
    struct gmap_blink_data_s b;
    bool show_switch;
};

static void gmap_load_data(struct gmap_data_s *d)
{
    d->gfx_mapview = lbxfile_item_get(LBXFILE_STARMAP, 0x30);
    d->gfx_but_col = lbxfile_item_get(LBXFILE_STARMAP, 0x31);
    d->gfx_but_env = lbxfile_item_get(LBXFILE_STARMAP, 0x32);
    d->gfx_but_min = lbxfile_item_get(LBXFILE_STARMAP, 0x33);
    d->gfx_but_ok = lbxfile_item_get(LBXFILE_STARMAP, 0x34);
}

static void gmap_free_data(struct gmap_data_s *d)
{
    lbxfile_item_release(LBXFILE_STARMAP, d->gfx_mapview);
    lbxfile_item_release(LBXFILE_STARMAP, d->gfx_but_col);
    lbxfile_item_release(LBXFILE_STARMAP, d->gfx_but_env);
    lbxfile_item_release(LBXFILE_STARMAP, d->gfx_but_min);
    lbxfile_item_release(LBXFILE_STARMAP, d->gfx_but_ok);
}

static void gmap_blink_step(struct gmap_blink_data_s *b)
{
    if (b->countdown < 0) {
        struct game_s *g = b->g;
        b->planet_i = rnd_0_nm1(g->galaxy_stars, &ui_data.seed);
        b->countdown = 3;
    } else {
        --b->countdown;
    }
}

static void gmap_draw_cb(void *vptr)
{
    struct gmap_data_s *d = vptr;
    const struct game_s *g = d->g;
    player_id_t tbl_races[PLAYER_NUM];
    int racesnum = 1;

    tbl_races[0] = d->api;
    for (player_id_t i = PLAYER_0; i < g->players; ++i) {
        if ((i != d->api) && IS_ALIVE(g, i)) {
            tbl_races[racesnum++] = i;
        }
    }

    uiobj_set_limits(GMAP_LIMITS);
    ui_draw_erase_buf();
    lbxgfx_draw_frame(0, 0, ui_data.gfx.starmap.sky, UI_SCREEN_W, ui_scale);
    lbxgfx_draw_frame(0, 0, d->gfx_mapview, UI_SCREEN_W, ui_scale);

    for (int i = 0; i < g->nebula_num; ++i) {
        int x, y;
        x = (g->nebula_x[i] * 224) / g->galaxy_maxx + 7;
        y = (g->nebula_y[i] * 185) / g->galaxy_maxy + 7;
        lbxgfx_draw_frame_offs(x, y, ui_data.gfx.starmap.smnebula[i], GMAP_LIMITS, UI_SCREEN_W, ui_scale);
    }

    for (int i = 0; i < g->enroute_num; ++i) {
        const fleet_enroute_t *r = &(g->enroute[i]);
        if (BOOLVEC_IS1(r->visible, d->api)) {
            uint8_t *gfx;
            int x, y;
            x = (r->x * 224) / g->galaxy_maxx + 7;
            y = (r->y * 185) / g->galaxy_maxy + 7;
            gfx = ui_data.gfx.starmap.tinyship[g->eto[r->owner].banner];
            lbxgfx_draw_frame_offs(x, y, gfx, GMAP_LIMITS, UI_SCREEN_W, ui_scale);
        }
    }

    for (int i = 0; i < g->transport_num; ++i) {
        const transport_t *r = &(g->transport[i]);
        if (BOOLVEC_IS1(r->visible, d->api)) {
            uint8_t *gfx;
            int x, y;
            x = (r->x * 224) / g->galaxy_maxx + 7;
            y = (r->y * 185) / g->galaxy_maxy + 7;
            gfx = ui_data.gfx.starmap.tinytran[g->eto[r->owner].banner];
            lbxgfx_draw_frame_offs(x, y, gfx, GMAP_LIMITS, UI_SCREEN_W, ui_scale);
        }
    }

    if (ui_extra_enabled) {
        char buf[32];
        lib_sprintf(buf, sizeof(buf), "Year %i", g->year+2299);
        lbxfont_select(0, 6, 0, 0);
        lbxfont_set_color_c_n(0, 5);
        lbxfont_print_str_normal(9, 9, buf, UI_SCREEN_W, ui_scale);
        lbxfont_set_color_c_n(194, 5);
        lbxfont_print_str_normal(8, 8, buf, UI_SCREEN_W, ui_scale);
    }

    for (int i = 0; i < g->galaxy_stars; ++i) {
        const planet_t *p = &(g->planet[i]);
        if (BOOLVEC_IS1(p->within_srange, d->api)) {
            player_id_t tbl_have_orbit_owner[PLAYER_NUM];
            int have_orbit_num;

            have_orbit_num = 0;
            for (player_id_t j = PLAYER_0; j < g->players; ++j) {
                const fleet_orbit_t *r = &(g->eto[j].orbit[i]);
                for (int k = 0; k < g->eto[j].shipdesigns_num; ++k) {
                    if (r->ships[k] != 0) {
                        tbl_have_orbit_owner[have_orbit_num++] = j;
                        break;
                    }
                }
            }

            for (int j = 0; j < have_orbit_num; ++j) {
                uint8_t *gfx;
                int x, y;
                /* FIXME all drawn to same position? */
                x = (p->x * 224) / g->galaxy_maxx + 14;
                y = (p->y * 185) / g->galaxy_maxy + 8;
                gfx = ui_data.gfx.starmap.tinyship[g->eto[tbl_have_orbit_owner[j]].banner];
                lbxgfx_draw_frame_offs(x, y, gfx, GMAP_LIMITS, UI_SCREEN_W, ui_scale);
            }
        }
    }

    for (int i = 0; i < g->galaxy_stars; ++i) {
        const planet_t *p = &(g->planet[i]);
        uint8_t *gfx;
        int x, y;
        x = (p->x * 224) / g->galaxy_maxx + 7;
        y = (p->y * 185) / g->galaxy_maxy + 7;
        if (BOOLVEC_IS1(p->explored, d->api) || (d->mode == 0)) {
            gfx = ui_data.gfx.starmap.smstars[p->star_type];
            if ((d->b.planet_i == i) && (d->b.countdown > 0)) {
                lbxgfx_set_new_frame(gfx, d->b.countdown);
            } else {
                lbxgfx_set_frame_0(gfx);
            }
            lbxgfx_draw_frame(x, y, gfx, UI_SCREEN_W, ui_scale);
        } else {
            gfx = ui_data.gfx.starmap.smallstr;
            lbxgfx_set_frame_0(gfx);
            lbxgfx_draw_frame(x + 1, y + 1, gfx, UI_SCREEN_W, ui_scale);
        }
    }

    for (int i = 0; i < g->galaxy_stars; ++i) {
        const planet_t *p = &(g->planet[i]);
        uint8_t *gfx;
        player_id_t owner;
        int x, y;
        x = (p->x * 224) / g->galaxy_maxx + 7;
        y = (p->y * 185) / g->galaxy_maxy + 7;
        owner = (BOOLVEC_IS1(p->within_srange, d->api)) ? p->owner : g->seen[d->api][i].owner;
        if (owner != PLAYER_NONE) {
            gfx = ui_data.gfx.starmap.smalflag[g->eto[owner].banner];
            lbxgfx_draw_frame(x + 2, y - 3, gfx, UI_SCREEN_W, ui_scale);
        }
        switch (d->mode) {
            case 1:
                if (BOOLVEC_IS1(p->explored, d->api)) {
                    char buf[2] = { 0, 0 };
                    int pt;
                    lbxfont_select_set_12_1(2, (p->type < g->eto[PLAYER_0].have_colony_for) ? 5 : 0xe, 0, 0);
                    pt = (PLANET_TYPE_TERRAN - p->type);
                    SETMAX(pt, 0);
                    buf[0] = game_str_gm_tchar[pt];
                    lbxfont_print_str_normal_limit(x + 7, y, buf, GMAP_SCALED_LIMITS, UI_SCREEN_W, ui_scale);
                }
                break;
            case 2:
                if (BOOLVEC_IS1(p->explored, d->api) && (p->special != PLANET_SPECIAL_NORMAL)) {
                    if (p->special > PLANET_SPECIAL_ARTIFACTS) {
                        lbxfont_select(2, 1, 0, 0);
                    } else {
                        lbxfont_select_set_12_1(2, (p->special != PLANET_SPECIAL_ARTIFACTS) ? 5 : 0xe, 0, 0);
                    }
                    if (p->special == PLANET_SPECIAL_4XTECH) {
                        lbxfont_select(2, 0xe, 0, 0);
                    }
                    lbxfont_print_str_center_limit(x + 2, y + 7, game_str_tbl_gm_spec[p->special], GMAP_SCALED_LIMITS, UI_SCREEN_W, ui_scale);
                }
                break;
            default:
                break;
        }
    }
    switch (d->mode) {
        case 0:
            lbxfont_select(0, 6, 0, 0);
            for (int i = 0; i < racesnum; ++i) {
                const empiretechorbit_t *e;
                e = &(g->eto[tbl_races[i]]);
                lbxgfx_draw_frame(245, 105 + 10 * i, ui_data.gfx.starmap.smalflag[e->banner], UI_SCREEN_W, ui_scale);
                lbxfont_print_str_normal(260, 105 + 10 * i, game_str_tbl_race[e->race], UI_SCREEN_W, ui_scale);
            }
            break;
        case 1:
            for (int i = 0; i < 7; ++i) {
                char buf[2] = { 0, 0 };
                buf[0] = game_str_gm_tchar[i];
                lbxfont_select(2, ((PLANET_TYPE_TERRAN - i) < g->eto[d->api].have_colony_for) ? 5 : 0xe, 0, 0);
                lbxfont_print_str_normal(241, 105 + 7 * i, buf, UI_SCREEN_W, ui_scale);
                lbxfont_select(2, 6, 0, 0);
                lbxfont_print_str_normal(247, 105 + 7 * i, game_str_tbl_sm_pltype[13 - i], UI_SCREEN_W, ui_scale);
            }
            for (int i = 7; i < 14; ++i) {
                char buf[2] = { 0, 0 };
                const char *str;
                buf[0] = game_str_gm_tchar[i];
                lbxfont_select(2, ((PLANET_TYPE_TERRAN - i) < g->eto[d->api].have_colony_for) ? 5 : 0xe, 0, 0);
                lbxfont_print_str_normal(276, 105 + 7 * (i - 7), buf, UI_SCREEN_W, ui_scale);
                lbxfont_select(2, 6, 0, 0);
                if (i == 13) {
                    str = game_str_st_none2;
                } else {
                    str = game_str_tbl_sm_pltype[13 - i];
                }
                lbxfont_print_str_normal(282, 105 + 7 * (i - 7), str, UI_SCREEN_W, ui_scale);
            }
            lbxfont_print_str_normal(247, 160, game_str_gm_unable, UI_SCREEN_W, ui_scale);
            ui_draw_pixel(241, 161, 0x44, ui_scale);
            ui_draw_pixel(241, 162, 0x44, ui_scale);
            ui_draw_pixel(241, 163, 0x44, ui_scale);
            ui_draw_pixel(242, 161, 0x44, ui_scale);
            ui_draw_pixel(242, 162, 0x44, ui_scale);
            ui_draw_pixel(242, 163, 0x44, ui_scale);
            ui_draw_pixel(243, 161, 0x44, ui_scale);
            ui_draw_pixel(243, 162, 0x44, ui_scale);
            ui_draw_pixel(243, 163, 0x44, ui_scale);
            break;
        case 2:
            lbxfont_select_set_12_1(2, 5, 0, 0);
            lbxfont_print_str_normal(240, 103, game_str_tbl_sm_pspecial[0], UI_SCREEN_W, ui_scale);
            lbxfont_print_str_normal(240, 111, game_str_tbl_sm_pspecial[1], UI_SCREEN_W, ui_scale);
            lbxfont_select_set_12_1(2, 1, 0, 0);
            lbxfont_print_str_normal(240, 119, game_str_tbl_sm_pspecial[4], UI_SCREEN_W, ui_scale);
            lbxfont_print_str_normal(240, 127, game_str_tbl_sm_pspecial[5], UI_SCREEN_W, ui_scale);
            lbxfont_select_set_12_1(2, 0xe, 0, 0);
            lbxfont_print_str_normal(240, 135, game_str_tbl_sm_pspecial[3], UI_SCREEN_W, ui_scale);
            lbxfont_print_str_normal(240, 143, game_str_tbl_gm_spec[6], UI_SCREEN_W, ui_scale);
            lbxfont_select(2, 6, 0, 0);
            lbxfont_print_str_normal(295, 103, game_str_gm_prod, UI_SCREEN_W, ui_scale);
            lbxfont_print_str_normal(295, 111, game_str_gm_prod, UI_SCREEN_W, ui_scale);
            lbxfont_print_str_normal(295, 119, game_str_gm_prod, UI_SCREEN_W, ui_scale);
            lbxfont_print_str_normal(295, 127, game_str_gm_prod, UI_SCREEN_W, ui_scale);
            lbxfont_print_str_normal(295, 135, game_str_gm_tech, UI_SCREEN_W, ui_scale);
            lbxfont_print_str_normal(295, 143, game_str_gm_tech, UI_SCREEN_W, ui_scale);
            lbxfont_print_str_right(291, 103, game_str_gm_1_3, UI_SCREEN_W, ui_scale);
            lbxfont_print_str_right(291, 111, game_str_gm_1_2, UI_SCREEN_W, ui_scale);
            lbxfont_print_str_right(291, 119, game_str_gm_2x, UI_SCREEN_W, ui_scale);
            lbxfont_print_str_right(291, 127, game_str_gm_3x, UI_SCREEN_W, ui_scale);
            lbxfont_print_str_right(291, 135, game_str_gm_2x, UI_SCREEN_W, ui_scale);
            lbxfont_print_str_right(291, 143, game_str_gm_4x, UI_SCREEN_W, ui_scale);
            lbxfont_print_str_center(275, 152, game_str_gm_prodb1, UI_SCREEN_W, ui_scale);
            lbxfont_print_str_center(275, 159, game_str_gm_prodb2, UI_SCREEN_W, ui_scale);
            lbxfont_print_str_center(275, 166, game_str_gm_prodb3, UI_SCREEN_W, ui_scale);
            break;
        default:
            break;
    }

    lbxfont_set_temp_color(0x2b);
    lbxfont_select_set_12_4(4, 0xf, 0, 0);
    lbxfont_print_str_normal(242, 8, game_str_gm_gmap, UI_SCREEN_W, ui_scale);
    lbxfont_print_str_normal(250, 88, game_str_gm_mapkey, UI_SCREEN_W, ui_scale);
    lbxfont_set_temp_color(0x00);
    gmap_blink_step(&(d->b));
    if (ui_scale > 1) {
        const uint8_t ctblf[5] = { 0xbd, 0xbc, 0xbb, 0xba, 0xb9 };
        const uint8_t ctblb[5] = { 0xba, 0xbb, 0xbc, 0xbd, 0xb9 };
        int x0, y0, x1, y1, pos;
        x0 = (ui_data.starmap.x * 224) / g->galaxy_maxx + 7;
        SETMAX(x0, 7);
        y0 = (ui_data.starmap.y * 185) / g->galaxy_maxy + 7;
        SETMAX(y0, 7);
        x1 = ((ui_data.starmap.x + ((108 * ui_scale) / starmap_scale)) * 224) / g->galaxy_maxx + 7;
        SETMIN(x1, 230);
        y1 = ((ui_data.starmap.y + ((86 * ui_scale) / starmap_scale)) * 185) / g->galaxy_maxy + 7;
        SETMIN(y1, 192);
        pos = d->b.countdown + 1;
        ui_draw_line_ctbl(x0, y0, x0 + 4, y0, ctblf, 5, pos, ui_scale);
        ui_draw_line_ctbl(x0, y0, x0, y0 + 4, ctblf, 5, pos, ui_scale);
        ui_draw_line_ctbl(x1 - 1, y0, x1 - 4, y0, ctblb, 5, 4 - pos, ui_scale);
        ui_draw_line_ctbl(x1, y0, x1, y0 + 4, ctblf, 5, pos, ui_scale);
        ui_draw_line_ctbl(x0, y1, x0 + 4, y1, ctblf, 5, pos, ui_scale);
        ui_draw_line_ctbl(x0, y1, x0, y1 - 4, ctblf, 5, pos, ui_scale);
        ui_draw_line_ctbl(x1 - 1, y1, x1 - 4, y1, ctblb, 5, 4 - pos, ui_scale);
        ui_draw_line_ctbl(x1, y1, x1, y1 - 4, ctblf, 5, pos, ui_scale);
    } else {
        int x, y;
        x = (ui_data.starmap.x * 224) / g->galaxy_maxx + 7;
        y = (ui_data.starmap.y * 185) / g->galaxy_maxy + 7;
        if (ui_sm_expanded_scroll) {
            lbxgfx_draw_frame_offs(x, y, ui_data.gfx.starmap.bmap, GMAP_LIMITS, UI_SCREEN_W, ui_scale);
        } else {
            lbxgfx_draw_frame(x, y, ui_data.gfx.starmap.bmap, UI_SCREEN_W, ui_scale);
        }
    }
}

static void ui_gmap_basic_draw_galaxy(struct gmap_basic_data_s *d)
{
    const struct game_s *g = d->g;
    ui_draw_filled_rect(6, 6, 221, 177, 0, ui_scale);
    /*uiobj_set_limits(6, 6, 221, 177);*/
    lbxgfx_draw_frame_offs(0, 0, ui_data.gfx.starmap.sky, 6, 6, 221, 177, UI_SCREEN_W, ui_scale);
    for (int i = 0; i < g->nebula_num; ++i) {
        int x, y;
        x = (g->nebula_x[i] * 215) / g->galaxy_maxx + 6;
        y = (g->nebula_y[i] * 171) / g->galaxy_maxy + 6;
        lbxgfx_draw_frame_offs(x, y, ui_data.gfx.starmap.smnebula[i], 6, 6, 221, 177, UI_SCREEN_W, ui_scale);
    }
    for (int i = 0; i < g->galaxy_stars; ++i) {
        const planet_t *p = &(g->planet[i]);
        uint8_t *gfx;
        int x, y;
        x = (p->x * 215) / g->galaxy_maxx + 6;
        y = (p->y * 171) / g->galaxy_maxy + 6;
        gfx = ui_data.gfx.starmap.smstars[p->star_type];
        if ((d->b.planet_i == i) && (d->b.countdown > 0)) {
            lbxgfx_set_new_frame(gfx, d->b.countdown);
        } else {
            lbxgfx_set_frame_0(gfx);
        }
        lbxgfx_draw_frame(x, y, gfx, UI_SCREEN_W, ui_scale);
    }
}

/* -------------------------------------------------------------------------- */

bool ui_gmap(struct game_s *g, player_id_t active_player)
{
    struct gmap_data_s d;
    bool flag_done = false, flag_do_focus = false;
    int16_t /*oi_col, oi_env, oi_min,*/ oi_ok, oi_tbl_planet[PLANETS_MAX];
    int16_t oi_map = UIOBJI_INVALID;

    gmap_load_data(&d);
    d.g = g;
    d.api = active_player;
    d.mode = 0;
    d.b.g = g;
    d.b.countdown = -1;
    d.b.planet_i = 0;

    uiobj_table_clear();

    oi_ok = uiobj_add_t0(246, 181, "", d.gfx_but_ok, MOO_KEY_SPACE);
    /*oi_col =*/ uiobj_add_t3(246, 27, "", d.gfx_but_col, &(d.mode), 0, MOO_KEY_c);
    /*oi_env =*/ uiobj_add_t3(246, 47, "", d.gfx_but_env, &(d.mode), 1, MOO_KEY_e);
    /*oi_min =*/ uiobj_add_t3(246, 67, "", d.gfx_but_min, &(d.mode), 2, MOO_KEY_m);
    for (int i = 0; i < g->galaxy_stars; ++i) {
        const planet_t *p = &(g->planet[i]);
        int x, y;
        x = (p->x * 224) / g->galaxy_maxx + 7;
        y = (p->y * 185) / g->galaxy_maxy + 7;
        /* FIXME limits not set! ... but no need to limit anyway */
        oi_tbl_planet[i] = uiobj_add_mousearea/*_limited*/(x, y, x + 4, y + 4, MOO_KEY_UNKNOWN);
    }
    if (ui_extra_enabled) {
        oi_map = uiobj_add_mousearea(GMAP_LIMITS, MOO_KEY_UNKNOWN);
    }

    uiobj_set_help_id(9);
    uiobj_set_callback_and_delay(gmap_draw_cb, &d, 4);
    uiobj_set_downcount(1);

    while (!flag_done) {
        int16_t oi;
        oi = uiobj_handle_input_cond();
        ui_delay_prepare();
        if ((oi == oi_ok) || (oi == UIOBJI_ESC)) {
            flag_done = true;
        }
        if (oi != 0) {
            ui_sound_play_sfx_24();
        }
        for (int i = 0; i < g->galaxy_stars; ++i) {
            if (oi == oi_tbl_planet[i]) {
                g->planet_focus_i[active_player] = i;
                flag_do_focus = true;
                flag_done = true;
                break;
            }
        }
        if (ui_extra_enabled && (oi == oi_map)) {
            int x, y;
            x = ((moouse_x / ui_scale - 7) * g->galaxy_maxx) / 224;
            y = ((moouse_y / ui_scale - 7) * g->galaxy_maxy) / 185;
            ui_starmap_set_pos(g, x, y);
            flag_done = true;
        }
        if (!flag_done) {
            gmap_draw_cb(&d);
            ui_draw_finish();
            ui_delay_ticks_or_click(4);
        }
    }

    uiobj_unset_callback();
    uiobj_set_help_id(-1);
    gmap_free_data(&d);
    return flag_do_focus;
}

void *ui_gmap_basic_init(struct game_s *g, bool show_player_switch)
{
    static struct gmap_basic_data_s ctx; /* HACK */
    ctx.g = g;
    ctx.b.g = g;
    ctx.b.countdown = -1;
    ctx.b.planet_i = 0;
    ctx.show_switch = show_player_switch;
    if (!show_player_switch) {
        ui_draw_copy_buf();
        ui_starmap_draw_button_text(0/*unused*/, false);
        hw_video_copy_back_to_page2();
    }
    return &ctx;
}

void ui_gmap_basic_shutdown(void *ctx)
{
}

void ui_gmap_basic_start_player(void *ctx, int pi)
{
    struct gmap_basic_data_s *d = ctx;
    if (d->show_switch) {
        ui_switch_1(d->g, pi);
        ui_draw_copy_buf();
        hw_video_copy_back_to_page2();
    }
}

void ui_gmap_basic_start_frame(void *ctx, int pi)
{
    /*struct gmap_basic_data_s *d = ctx;*/
    ui_delay_prepare();
    hw_video_copy_back_from_page2();
}

void ui_gmap_basic_draw_frame(void *ctx, int pi/*player_i*/)
{
    struct gmap_basic_data_s *d = ctx;
    const struct game_s *g = d->g;
    ui_gmap_basic_draw_galaxy(d);
    if (pi >= 0) {
        for (int i = 0; i < g->enroute_num; ++i) {
            const fleet_enroute_t *r = &(g->enroute[i]);
            if (BOOLVEC_IS1(r->visible, pi)) {
                uint8_t *gfx;
                int x, y;
                x = (r->x * 215) / g->galaxy_maxx + 6;
                y = (r->y * 171) / g->galaxy_maxy + 6;
                gfx = ui_data.gfx.starmap.tinyship[g->eto[r->owner].banner];
                lbxgfx_draw_frame_offs(x, y, gfx, 6, 6, 221, 177, UI_SCREEN_W, ui_scale);
            }
        }
        for (int i = 0; i < g->transport_num; ++i) {
            const transport_t *r = &(g->transport[i]);
            if (BOOLVEC_IS1(r->visible, pi)) {
                uint8_t *gfx;
                int x, y;
                x = (r->x * 215) / g->galaxy_maxx + 6;
                y = (r->y * 171) / g->galaxy_maxy + 6;
                gfx = ui_data.gfx.starmap.tinytran[g->eto[r->owner].banner];
                lbxgfx_draw_frame_offs(x, y, gfx, 6, 6, 221, 177, UI_SCREEN_W, ui_scale);
            }
        }
        for (int i = 0; i < g->galaxy_stars; ++i) {
            const planet_t *p = &(g->planet[i]);
            if (BOOLVEC_IS1(p->within_srange, pi)) {
                player_id_t tbl_have_orbit_owner[PLAYER_NUM];
                int have_orbit_num;
                have_orbit_num = 0;
                for (player_id_t j = PLAYER_0; j < g->players; ++j) {
                    const fleet_orbit_t *r = &(g->eto[j].orbit[i]);
                    for (int k = 0; k < g->eto[j].shipdesigns_num; ++k) {
                        if (r->ships[k] != 0) {
                            tbl_have_orbit_owner[have_orbit_num++] = j;
                            break;
                        }
                    }
                }
                for (int j = 0; j < have_orbit_num; ++j) {
                    uint8_t *gfx;
                    int x, y;
                    /* FIXME all drawn to same position? */
                    x = (p->x * 215) / g->galaxy_maxx + 13;
                    y = (p->y * 171) / g->galaxy_maxy + 7;
                    gfx = ui_data.gfx.starmap.tinyship[g->eto[tbl_have_orbit_owner[j]].banner];
                    lbxgfx_draw_frame_offs(x, y, gfx, 6, 6, 221, 177, UI_SCREEN_W, ui_scale);
                }
            }
        }
        for (int i = 0; i < 2; ++i) {
            const monster_t *r;
            r = (i == 0) ? &(g->evn.crystal) : &(g->evn.amoeba);
            if (r->exists && (r->killer == PLAYER_NONE)) {
                uint8_t *gfx;
                int x, y;
                x = (r->x * 215) / g->galaxy_maxx + 6;
                y = (r->y * 171) / g->galaxy_maxy + 6;
                gfx = ui_data.gfx.planets.tmonster;
                lbxgfx_draw_frame_offs(x, y, gfx, 6, 6, 221, 177, UI_SCREEN_W, ui_scale);
            }
        }
    }
    gmap_blink_step(&(d->b));
}

void ui_gmap_basic_draw_only(void *ctx, int pi/*planet_i*/)
{
    struct gmap_basic_data_s *d = ctx;
    const struct game_s *g = d->g;
    ui_gmap_basic_draw_galaxy(d);
    {
        const planet_t *p = &(g->planet[pi]);
        player_id_t tbl_have_orbit_owner[PLAYER_NUM];
        int have_orbit_num;
        have_orbit_num = 0;
        for (player_id_t j = PLAYER_0; j < g->players; ++j) {
            const fleet_orbit_t *r = &(g->eto[j].orbit[pi]);
            for (int k = 0; k < g->eto[j].shipdesigns_num; ++k) {
                if (r->ships[k] != 0) {
                    tbl_have_orbit_owner[have_orbit_num++] = j;
                    break;
                }
            }
        }
        for (int j = 0; j < have_orbit_num; ++j) {
            uint8_t *gfx;
            int x, y;
            /* FIXME all drawn to same position? */
            x = (p->x * 215) / g->galaxy_maxx + 13;
            y = (p->y * 171) / g->galaxy_maxy + 7;
            gfx = ui_data.gfx.starmap.tinyship[g->eto[tbl_have_orbit_owner[j]].banner];
            lbxgfx_draw_frame_offs(x, y, gfx, 6, 6, 221, 177, UI_SCREEN_W, ui_scale);
        }
        for (int i = 0; i < 2; ++i) {
            const monster_t *r;
            r = (i == 0) ? &(g->evn.crystal) : &(g->evn.amoeba);
            if (r->exists && (r->killer == PLAYER_NONE) && (r->x == p->x) && (r->y == p->y)) {
                uint8_t *gfx;
                int x, y;
                x = (r->x * 215) / g->galaxy_maxx + 6;
                y = (r->y * 171) / g->galaxy_maxy + 6;
                gfx = ui_data.gfx.planets.tmonster;
                lbxgfx_draw_frame_offs(x, y, gfx, 6, 6, 221, 177, UI_SCREEN_W, ui_scale);
            }
        }
    }
    gmap_blink_step(&(d->b));
}

void ui_gmap_basic_finish_frame(void *ctx, int pi)
{
    /*struct gmap_basic_data_s *d = ctx;*/
    ui_draw_finish();
    ui_delay_ticks_or_click(2);
}

void ui_gmap_draw_planet_border(const struct game_s *g, uint8_t planet_i)
{
    const planet_t *p = &(g->planet[planet_i]);
    int x, y;
    x = (p->x * 215) / g->galaxy_maxx + 5;
    y = (p->y * 171) / g->galaxy_maxy + 5;
    lbxgfx_draw_frame_offs(x, y, ui_data.gfx.starmap.slanbord, 6, 6, 221, 177, UI_SCREEN_W, ui_scale);
}
