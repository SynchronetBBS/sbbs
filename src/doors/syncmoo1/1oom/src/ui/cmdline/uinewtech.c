#include "config.h"

#include <stdio.h>

#include "ui.h"
#include "game.h"
#include "game_diplo.h"
#include "game_misc.h"
#include "game_num.h"
#include "game_str.h"
#include "game_tech.h"
#include "game_techtypes.h"
#include "lib.h"
#include "rnd.h"
#include "uidefs.h"
#include "uiinput.h"
#include "uiswitch.h"

/* -------------------------------------------------------------------------- */

struct newtech_data_s {
    struct game_s *g;
    player_id_t api;
    newtech_t nt;
    uint8_t dialog_type;
    bool flag_is_current;
    bool flag_choose_next;
    uint8_t tech_next[TECH_NEXT_MAX];
    int num_next;
};

/* -------------------------------------------------------------------------- */

static void ui_newtech_print1(struct newtech_data_s *d)
{
    const struct game_s *g = d->g;
    puts(game_tech_get_newtech_msg(g, d->api, &(d->nt), ui_data.strbuf, UI_STRBUF_SIZE));
    if (d->nt.source != TECHSOURCE_CHOOSE) {
        puts(game_tech_get_name(g->gaux, d->nt.field, d->nt.tech, ui_data.strbuf, UI_STRBUF_SIZE));
        puts(game_tech_get_descr(g->gaux, d->nt.field, d->nt.tech, ui_data.strbuf, UI_STRBUF_SIZE));
    }
}

static void ui_newtech_choose_next(struct newtech_data_s *d)
{
    struct input_list_s rl_in[] = {
        { -1, "1", NULL, NULL },
        { -1, "2", NULL, NULL },
        { -1, "3", NULL, NULL },
        { -1, "4", NULL, NULL },
        { -1, "5", NULL, NULL },
        { -1, "6", NULL, NULL },
        { -1, "7", NULL, NULL },
        { -1, "8", NULL, NULL },
        { -1, "9", NULL, NULL },
        { -1, "A", NULL, NULL },
        { -1, "B", NULL, NULL },
        { -1, "C", NULL, NULL },
        { 0, NULL, NULL, NULL }
    };
    char tname[TECH_NEXT_MAX][35];
    int i;
    for (i = 0; i < d->num_next; ++i) {
        uint8_t tech;
        tech = d->tech_next[i];
        game_tech_get_name(d->g->gaux, d->nt.field, tech, tname[i], 35);
        rl_in[i].value = tech;
        rl_in[i].display = tname[i];
    }
    rl_in[i].value = 0;
    rl_in[i].key = NULL;
    rl_in[i].str = NULL;
    rl_in[i].display = NULL;
    i = ui_input_list(game_str_nt_choose, "> ", rl_in);
    game_tech_start_next(d->g, d->api, d->nt.field, i);
}

static void newtech_adjust_draw_typestr(char *buf, size_t bufsize, const char *str1, const char *str2)
{
    lib_strcat(buf, game_str_nt_inc, bufsize);
    lib_strcat(buf, str1, bufsize);
    if (str2) {
        lib_strcat(buf, str2, bufsize);
    }
}

static void ui_newtech_adjust(struct newtech_data_s *d)
{
    char *buf = ui_data.strbuf;
    lib_strcpy(buf, game_str_nt_doyou, UI_STRBUF_SIZE);
    switch (d->dialog_type) {
        case 0:
            lib_strcat(buf, game_str_nt_redueco, UI_STRBUF_SIZE);
            break;
        case 1:
            newtech_adjust_draw_typestr(buf, UI_STRBUF_SIZE, game_str_nt_ind, 0);
            break;
        case 2:
            newtech_adjust_draw_typestr(buf, UI_STRBUF_SIZE, game_str_nt_ecoall, game_str_nt_terra);
            break;
        case 3:
            newtech_adjust_draw_typestr(buf, UI_STRBUF_SIZE, game_str_nt_def, 0);
            break;
        case 4:
            newtech_adjust_draw_typestr(buf, UI_STRBUF_SIZE, game_str_nt_ecostd, game_str_nt_terra);
            break;
        case 5:
            newtech_adjust_draw_typestr(buf, UI_STRBUF_SIZE, game_str_nt_ecohost, game_str_nt_terra);
            break;
        default:
            break;
    }
    if (d->dialog_type == 0) {
        int v = ui_input_list(buf, "> ", il_yes_no);
        if (v) {
            game_update_tech_util(d->g);
            game_update_eco_on_waste(d->g, d->api, true);
        }
    } else {
        struct input_list_s rl_in[] = {
            { 0, "1", NULL, NULL },
            { 1, "2", NULL, NULL },
            { 2, "3", NULL, NULL },
            { 3, "4", NULL, NULL },
            { 0, NULL, NULL, NULL }
        };
        const int tbl_a0[5] = { 0, 0, 0, 1, 2 };
        const int tbl_a2[5] = { 0, 3, 2, 3, 3 };
        int v;
        for (int i = 0; i < 4; ++i) {
            rl_in[i].display = game_str_tbl_nt_adj[i];
        }
        v = ui_input_list(buf, "> ", rl_in);
        game_update_tech_util(d->g);
        if (!game_num_newtech_adjust_fix) {
            game_update_eco_on_waste(d->g, d->api, true);
        }
        game_planet_adjust_percent(d->g, d->api, tbl_a0[d->dialog_type - 1], game_num_tbl_tech_autoadj[v], tbl_a2[d->dialog_type - 1]);
    }
}

static void ui_newtech_do(struct newtech_data_s *d)
{
    struct game_s *g = d->g;
    empiretechorbit_t *e = &(g->eto[d->api]);
    uint8_t tech = d->nt.tech;
    bool flag_dialog = false;
    if (tech < 51) {
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
    ui_newtech_print1(d);
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
        if (d->nt.frame) {
            struct input_list_s rl_in[] = {
                { -1, "1", NULL, NULL },
                { -1, "2", NULL, NULL },
                { -1, "0", NULL, "(none)" },
                { 0, NULL, NULL, NULL }
            };
            int frame;
            rl_in[0].value = d->nt.other1;
            rl_in[0].display = game_str_tbl_races[g->eto[d->nt.other1].race];
            rl_in[1].value = d->nt.other2;
            rl_in[1].display = game_str_tbl_races[g->eto[d->nt.other2].race];
            puts(game_str_nt_frame);
            frame = ui_input_list(game_str_nt_victim, "> ", rl_in);
            if (frame >= 0) {
                g->evn.stolen_spy[d->nt.stolen_from][d->api] = frame;
                game_diplo_esp_frame(g, frame, d->nt.stolen_from);
            }
        }
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
}

/* -------------------------------------------------------------------------- */

void ui_newtech(struct game_s *g, int pi)
{
    struct newtech_data_s d;
    bool flag_switch = false;

    d.g = g;
    d.api = pi;

    for (int i = 0; i < g->evn.newtech[pi].num; ++i) {
        d.nt = g->evn.newtech[pi].d[i];
        if (!flag_switch) {
            flag_switch = true;
            ui_switch_1(g, pi);
        }
        d.flag_is_current = false;
        d.flag_choose_next = false;
        if (g->eto[pi].tech.project[d.nt.field] == d.nt.tech) {
            d.flag_is_current = true;
        }
        ui_newtech_do(&d);
    }

    for (tech_field_t field = 0; field < TECH_FIELD_NUM; ++field) {
        if (game_tech_can_choose(g, pi, field)) {
            if (!flag_switch) {
                flag_switch = true;
                ui_switch_1(g, pi);
            }
            d.nt.field = field;
            d.nt.tech = 0;
            d.nt.source = TECHSOURCE_CHOOSE;
            d.flag_choose_next = true;
            d.nt.frame = false;
            ui_newtech_do(&d);
        }
    }
    if (flag_switch) {
        ui_switch_wait(g);
    }
    g->evn.newtech[pi].num = 0;
}
