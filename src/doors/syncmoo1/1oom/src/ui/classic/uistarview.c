#include "config.h"

#include <ctype.h>
#include <stdio.h>

#include "uistarview.h"
#include "comp.h"
#include "game.h"
#include "game_str.h"
#include "gfxaux.h"
#include "hw.h"
#include "kbd.h"
#include "lbx.h"
#include "lbxfont.h"
#include "lbxgfx.h"
#include "lbxpal.h"
#include "lib.h"
#include "log.h"
#include "types.h"
#include "uidelay.h"
#include "uidefs.h"
#include "uidraw.h"
#include "uiobj.h"
#include "uipal.h"
#include "uisound.h"
#include "util.h"

/* -------------------------------------------------------------------------- */

struct starview_data_s {
    struct game_s *g;
    player_id_t api;
    uint8_t planet_i;
    uint8_t mode;
    int frame;
    bool flag_pal;
    uint8_t *gfx_planet;
    uint8_t *gfx_star;
    uint8_t *gfx_shield;
    uint8_t *gfx_pop;
    uint8_t *gfx_fact;
    uint8_t *gfx_waste;
    uint8_t *gfx_base;
};

static void starview_load_data(struct starview_data_s *d)
{
    const struct game_s *g = d->g;
    const planet_t *p = &(g->planet[d->planet_i]);
    d->gfx_planet = lbxfile_item_get(LBXFILE_STARVIEW, 7 + p->infogfx);
    d->gfx_shield = lbxfile_item_get(LBXFILE_STARVIEW, 0);
    {
        int i = p->star_type;
        if (i == 3) {
            i = 4;
        } else if (i == 4) {
            i = 3;
        }
        d->gfx_star = lbxfile_item_get(LBXFILE_STARVIEW, 1 + i);
    }
    d->gfx_pop = lbxfile_item_get(LBXFILE_STARVIEW, 0x2a);
    d->gfx_fact = lbxfile_item_get(LBXFILE_STARVIEW, 0x2b);
    d->gfx_waste = lbxfile_item_get(LBXFILE_STARVIEW, 0x2c);
    d->gfx_base = lbxfile_item_get(LBXFILE_STARVIEW, 0x2d);
}

static void starview_free_data(struct starview_data_s *d)
{
    lbxfile_item_release(LBXFILE_STARVIEW, d->gfx_planet);
    lbxfile_item_release(LBXFILE_STARVIEW, d->gfx_shield);
    lbxfile_item_release(LBXFILE_STARVIEW, d->gfx_star);
    lbxfile_item_release(LBXFILE_STARVIEW, d->gfx_pop);
    lbxfile_item_release(LBXFILE_STARVIEW, d->gfx_fact);
    lbxfile_item_release(LBXFILE_STARVIEW, d->gfx_waste);
    lbxfile_item_release(LBXFILE_STARVIEW, d->gfx_base);
}

static void starview_draw_cb(void *vptr)
{
    struct starview_data_s *d = vptr;
    const struct game_s *g = d->g;
    const planet_t *p = &(g->planet[d->planet_i]);
    const seen_t *s = &(g->seen[d->api][d->planet_i]);
    player_id_t sowner = s->owner;
    char buf[0x80];
    ui_draw_erase_buf();
    if (p->type != PLANET_TYPE_NOT_HABITABLE) {
        lbxgfx_draw_frame(0, 0, d->gfx_planet, UI_SCREEN_W, ui_scale);
        if (!d->flag_pal) {
            d->flag_pal = true;
            lbxgfx_apply_palette(d->gfx_planet);
            lbxgfx_apply_palette(d->gfx_star);
        }
    }
    if ((p->shield != 0) && (sowner != PLAYER_NONE)) {
        lbxgfx_draw_frame(0, 0, d->gfx_shield, UI_SCREEN_W, ui_scale);
    }
    lbxgfx_draw_frame(212, 0, d->gfx_star, UI_SCREEN_W, ui_scale);
    lbxfont_select(4, 0, 0, 0);
    lbxfont_print_str_center(170, 2, p->name, UI_SCREEN_W, ui_scale);
    lbxfont_select(3, 0xb, 0, 0);
    lib_strcpy(buf, game_str_tbl_sm_pltype[p->type], sizeof(buf));
    util_str_tolower(buf, sizeof(buf));
    if (p->type != PLANET_TYPE_NOT_HABITABLE) {
        int l;
        l = strlen(buf);
        buf[l++] = ' ';
        buf[l] = '\0';
        lib_strcat(&buf[l], game_str_sv_envir, sizeof(buf));
    }
    lbxfont_print_str_center(170, 17, buf, UI_SCREEN_W, ui_scale);
    if (p->have_stargate) {
        lbxfont_select(5, 0xb, 0, 0);
        lbxfont_print_str_center(269, 134, game_str_sv_stargt, UI_SCREEN_W, ui_scale);
    }
    if ((sowner != PLAYER_NONE) && d->mode) {
        const empiretechorbit_t *e = &(g->eto[sowner]);
        const shipdesign_t *sd = &(g->srd[sowner].design[0]);
        int n = 0;
        lbxfont_select(0, 0x8, 0, 0);
        for (int i = 0; i < e->shipdesigns_num; ++i) {
            int ships;
            ships = e->orbit[d->planet_i].ships[i];
            if (ships > 0) {
                const int sx[NUM_SHIPDESIGNS] = { 225, 175, 225, 175, 275, 125 };
                const int sy[NUM_SHIPDESIGNS] = { 66, 33, 100, 66, 100, 33 };
                int x, y;
                uint8_t *gfx;
                x = sx[n];
                y = sy[n];
                gfx = ui_data.gfx.ships[sd[i].look];
                lbxgfx_set_new_frame(gfx, (d->frame + i) % 5);
                gfx_aux_draw_frame_to(gfx, &ui_data.aux.ship_p1);
                gfx_aux_draw_frame_from(x + 8, y + 8, &ui_data.aux.ship_p1, UI_SCREEN_W, ui_scale);
                lbxfont_print_num_right(x + 39, y + 25, ships, UI_SCREEN_W, ui_scale);
                ++n;
            }
        }
    }
    if ((sowner != PLAYER_NONE) && d->mode) {
        if (p->shield != 0) {
            lib_sprintf(buf, sizeof(buf), "%s %s %s", game_str_sv_shild1, game_str_tbl_roman[p->shield], game_str_sv_shild2);
            lbxfont_select(0, 0xd, 0, 0);
            lbxfont_print_str_normal(8, 10, buf, UI_SCREEN_W, ui_scale);
        }
        ui_draw_filled_rect(225, 144, 319, 199, 0xf0, ui_scale);
        ui_draw_box2(225, 144, 319, 199, 0xb3, 0xb3, 0xb3, 0xb3, ui_scale);
        lbxfont_select(0, 0xe, 0, 0);
        lbxfont_print_str_right(284, 149, game_str_sv_psize, UI_SCREEN_W, ui_scale);
        lbxfont_print_str_right(284, 159, game_str_sv_fact, UI_SCREEN_W, ui_scale);
        lbxfont_print_str_right(284, 169, game_str_sv_waste, UI_SCREEN_W, ui_scale);
        lbxfont_print_str_right(284, 179, game_str_sv_pop, UI_SCREEN_W, ui_scale);
        lbxfont_print_str_right(284, 189, game_str_sv_growth, UI_SCREEN_W, ui_scale);
        lbxfont_select(0, 0xd, 0, 0);
        lbxfont_print_num_right(308, 149, p->max_pop3, UI_SCREEN_W, ui_scale);
        lbxfont_print_num_right(308, 159, s->factories, UI_SCREEN_W, ui_scale);
        lbxfont_print_num_right(308, 169, p->waste, UI_SCREEN_W, ui_scale);
        lbxfont_print_num_right(308, 179, s->pop, UI_SCREEN_W, ui_scale);
        if ((sowner == d->api) && (p->pop != p->pop_prev)) {
            lib_sprintf(buf, sizeof(buf), "%+i", p->pop - p->pop_prev);
            lbxfont_print_str_right(308, 189, buf, UI_SCREEN_W, ui_scale);
        }
    } else if ((p->type != PLANET_TYPE_NOT_HABITABLE)
       && ((p->special != PLANET_SPECIAL_NORMAL) || (p->growth != PLANET_GROWTH_NORMAL))
    ) {
        int y0 = (p->growth != PLANET_GROWTH_NORMAL) ? 122 : 162;
        if (p->special != PLANET_SPECIAL_NORMAL) {
            const char *str0, *str1, *str2;
            switch (p->special) {
                case PLANET_SPECIAL_ULTRA_POOR:
                    str0 = game_str_ex_ps1[0];
                    str2 = game_str_sv_1_3x;
                    str1 = game_str_sv_resp;
                    break;
                case PLANET_SPECIAL_POOR:
                    str0 = game_str_ex_ps1[1];
                    str2 = game_str_sv_1_2x;
                    str1 = game_str_sv_resp;
                    break;
                case PLANET_SPECIAL_RICH:
                    str0 = game_str_ex_ps1[3];
                    str2 = game_str_sv_2x;
                    str1 = game_str_sv_resp;
                    break;
                case PLANET_SPECIAL_ULTRA_RICH:
                    str0 = game_str_ex_ps1[4];
                    str2 = game_str_sv_3x;
                    str1 = game_str_sv_resp;
                    break;
                case PLANET_SPECIAL_ARTIFACTS:
                    str0 = game_str_ex_ps1[2];
                    str2 = game_str_sv_2x;
                    str1 = game_str_sv_techp;
                    break;
                case PLANET_SPECIAL_4XTECH:
                    str0 = game_str_ex_ps1[2];
                    str2 = game_str_sv_4x;
                    str1 = game_str_sv_techp;
                    break;
                default:
                    str0 = 0;
                    str2 = 0;
                    str1 = 0;
                    break;
            }
            lib_sprintf(buf, sizeof(buf), "%s:", str0);
            lbxfont_select_set_12_4(5, 0xd, 0, 0);
            lbxfont_print_str_normal(8, y0, buf, UI_SCREEN_W, ui_scale);
            lib_sprintf(buf, sizeof(buf), "%s %s", str1, str2);
            lbxfont_select_set_12_4(5, 0xa, 0, 0);
            lbxfont_print_str_split(80, y0, 110, buf, 0, UI_SCREEN_W, UI_SCREEN_H, ui_scale);
        }
        if (p->growth != PLANET_GROWTH_NORMAL) {
            int i = p->growth;
            if (i > PLANET_GROWTH_NORMAL) {
                --i;
            }
            y0 = 162;
            lib_sprintf(buf, sizeof(buf), "%s:", game_str_sv_pg1[i]);
            lbxfont_select_set_12_4(5, 0xd, 0, 0);
            lbxfont_print_str_normal(8, y0, buf, UI_SCREEN_W, ui_scale);
            lib_sprintf(buf, sizeof(buf), "%s %s", game_str_sv_popgr, game_str_sv_pg2[i]);
            lbxfont_select_set_12_4(5, 0xa, 0, 0);
            lbxfont_print_str_split(80, y0, 110, buf, 0, UI_SCREEN_W, UI_SCREEN_H, ui_scale);
        }
    }
    if ((sowner != PLAYER_NONE) && d->mode) {
        int v10, v12, v14, y0 = 184;
        v12 = (s->pop + 9) / 10;
        v10 = (v12 + 9) / 10;
        for (int i = 0; i < v10; ++i) {
            if (i == (v10 - 1)) {
                v14 = v12 - (v10 - 1) * 10;
            } else {
                v14 = 10;
            }
            for (int j = 0; j < v14; ++j) {
                lbxgfx_draw_frame(j * 18 + (i & 1) * 9 + 5, y0 - (v10 - 1 - i) * 5, d->gfx_pop, UI_SCREEN_W, ui_scale);
            }
        }
        y0 -= v10 * 5 + 10;
        v12 = (MIN(s->factories, 1200) + 9) / 10;
        v10 = (v12 + 9) / 10;
        for (int i = 0; i < v10; ++i) {
            if (i == (v10 - 1)) {
                v14 = v12 - (v10 - 1) * 10;
            } else {
                v14 = 10;
            }
            for (int j = 0; j < v14; ++j) {
                lbxgfx_draw_frame(j * 18 + (i & 1) * 9 + 5, y0 - (v10 - 1 - i) * 5, d->gfx_fact, UI_SCREEN_W, ui_scale);
            }
        }
        y0 -= v10 * 5 + 10;
        v12 = (MIN(s->bases, 300) + 4) / 5;
        v10 = (v12 + 9) / 10;
        for (int i = 0; i < v10; ++i) {
            if (i == (v10 - 1)) {
                v14 = v12 - (v10 - 1) * 10;
            } else {
                v14 = 10;
            }
            for (int j = 0; j < v14; ++j) {
                lbxgfx_draw_frame(j * 14 + (i & 1) * 7 + 5, y0 - (v10 - 1 - i) * 5, d->gfx_base, UI_SCREEN_W, ui_scale);
            }
        }
        y0 -= v10 * 5 + 10;
        v12 = (MIN(p->waste, 200) + 9) / 10;
        v10 = (v12 + 9) / 10;
        for (int i = 0; i < v10; ++i) {
            if (i == (v10 - 1)) {
                v14 = v12 - (v10 - 1) * 10;
            } else {
                v14 = 10;
            }
            for (int j = 0; j < v14; ++j) {
                lbxgfx_draw_frame(j * 18 + (i & 1) * 9 + 5, y0 - (v10 - 1 - i) * 5, d->gfx_waste, UI_SCREEN_W, ui_scale);
            }
        }
    }
    d->frame = (d->frame + 1) % 5;
}

/* -------------------------------------------------------------------------- */

void ui_starview(struct game_s *g, player_id_t pi)
{
    struct starview_data_s d;
    bool flag_done = false;
    int16_t oi_mode;
    d.g = g;
    d.api = pi;
    d.planet_i = g->planet_focus_i[pi];
    d.mode = (g->planet[d.planet_i].owner == PLAYER_NONE) ? 0 : 1;
    d.frame = 0;
    d.flag_pal = false;
    ui_palette_fadeout_a_f_1();
    ui_draw_finish_mode = 2;
    lbxpal_select(5, -1, 0);
    starview_load_data(&d);
    uiobj_set_callback_and_delay(starview_draw_cb, &d, 4);
    uiobj_table_clear();
    uiobj_add_mousearea_all(MOO_KEY_UNKNOWN);
    oi_mode = ui_extra_enabled ? uiobj_add_inputkey(MOO_KEY_i) : UIOBJI_INVALID;
    uiobj_set_help_id(34);
    uiobj_set_downcount(1);
    while (!flag_done) {
        int16_t oi;
        ui_delay_prepare();
        oi = uiobj_handle_input_cond();
        if (oi == oi_mode) {
            d.mode = (d.mode + 1) % 2;
        } else if (oi != 0) {
            flag_done = true;
        }
        if (!flag_done) {
            starview_draw_cb(&d);
            ui_draw_finish();
            ui_delay_ticks_or_click(3);
        }
    }
    ui_sound_play_sfx_24();
    ui_delay_1();
    uiobj_unset_callback();
    uiobj_table_clear();
    ui_palette_fadeout_a_f_1();
    ui_draw_finish_mode = 2;
    lbxpal_select(0, -1, 0);
    uiobj_set_help_id(-1);
    starview_free_data(&d);
}
