#include "config.h"

#include <stdio.h>

#include "ui.h"
#include "game.h"
#include "game_str.h"
#include "uidefs.h"
#include "uihelp.h"
#include "uiinput.h"
#include "uiplanet.h"
#include "uiswitch.h"

/* -------------------------------------------------------------------------- */

struct ui_sab_data_s {
    player_id_t target;
    ui_sabotage_t act;
};

#define UI_SPY_SAB_MAKE(_act_, _planet_)    (((_act_) << 8) | (_planet_))
#define UI_SPY_SAB_ACT(_v_) ((((uint32_t)(_v_)) >> 8) & 0xf)
#define UI_SPY_SAB_PLANET(_v_)  ((_v_) & 0xff)

static int cmd_look_sab(struct game_s *g, int api, struct input_token_s *param, int num_param, void *var)
{
    struct ui_sab_data_s *sabdata = var;
    for (uint8_t i = 0; i < g->galaxy_stars; ++i) {
        const planet_t *p = &(g->planet[i]);
        if (p->owner == sabdata->target) {
            ui_planet_look(g, api, i, true);
        }
    }
    return 0;
}

static int cmd_sab(struct game_s *g, int api, struct input_token_s *param, int num_param, void *var)
{
    struct ui_sab_data_s *sabdata = var;
    uint8_t planet = 0;
    ui_sabotage_t act = sabdata->act;
    player_id_t target = sabdata->target;
    if (act != UI_SABOTAGE_NONE) {
        const planet_t *p;
        planet = ui_planet_from_param(g, api, param);
        if (planet == PLANET_NONE) {
            return -1;
        }
        p = &(g->planet[planet]);
        if (p->owner != target) {
            printf("The planet does not belong to the %s\n", game_str_tbl_race[g->eto[target].race]);
            return -1;
        }
    }
    return UI_SPY_SAB_MAKE(act, planet);
}

static const struct input_cmd_s * const cmdsptr_sab[2];

static const struct input_cmd_s cmds_sab[] = {
    { "?", NULL, "Help", 0, 0, 0, ui_cmd_help, (void *)cmdsptr_sab },
    { "l", NULL, "List target planets", 0, 0, 0, cmd_look_sab, 0 },
    { "b", "PLANET", "Sabotage missile bases", 1, 1, 0, cmd_sab, (void *)UI_SABOTAGE_BASES },
    { "f", "PLANET", "Sabotage factories", 1, 1, 0, cmd_sab, (void *)UI_SABOTAGE_FACT },
    { "r", "PLANET", "Incite revolt", 1, 1, 0, cmd_sab, (void *)UI_SABOTAGE_REVOLT },
    { "n", NULL, "Do nothing", 0, 0, 0, cmd_sab, (void *)UI_SABOTAGE_NONE },
    { NULL, NULL, NULL, 0, 0, 0, NULL, 0 }
};

static const struct input_cmd_s * const cmdsptr_sab[2] = {
    cmds_sab,
    0
};

/* -------------------------------------------------------------------------- */

ui_sabotage_t ui_spy_sabotage_ask(struct game_s *g, int spy, int target, uint8_t *planetptr)
{
    struct ui_sab_data_s sabdata;
    sabdata.target = target;
    ui_switch_1(g, spy);
    printf("Sabotage | %s | %s\n", game_str_tbl_race[g->eto[target].race], game_str_sb_choose);
    while (1) {
        char *input;
        input = ui_input_line("> ");
        if ((ui_input_tokenize(input, cmdsptr_sab) == 0) && (ui_data.input.num > 0)) {
            if (ui_data.input.tok[0].type == INPUT_TOKEN_COMMAND) {
                const struct input_cmd_s *cmd;
                void *var;
                int v;
                cmd = ui_data.input.tok[0].data.cmd;
                if (cmd->handle == ui_cmd_help) {
                    var = cmd->var;
                } else {
                    sabdata.act = (ui_sabotage_t)cmd->var;
                    var = &sabdata;
                }
                v = cmd->handle(g, spy, &ui_data.input.tok[1], ui_data.input.num - 1, var);
                if ((v >= 0) && (cmd->handle == cmd_sab)) {
                    uint8_t planet = UI_SPY_SAB_PLANET(v);
                    ui_sabotage_t act = UI_SPY_SAB_ACT(v);
                    *planetptr = planet;
                    return act;
                }
            }
        }
    }
    return UI_SABOTAGE_NONE;
}

int ui_spy_sabotage_done(struct game_s *g, int pi, int spy, int target, ui_sabotage_t act, int other1, int other2, uint8_t planet, int snum)
{
    const planet_t *p = &(g->planet[planet]);
    ui_switch_1(g, pi);
    printf("Sabotage | %s | ", p->name);
    if (spy == PLAYER_NONE) {
        printf("%s ", game_str_sb_unkn);
    } else {
        printf("%s %s ", (spy == pi) ? game_str_sb_your : game_str_tbl_race[g->eto[spy].race], game_str_sb_spies);
    }
    if (snum > 0) {
        switch (act) {
            default:
            case UI_SABOTAGE_FACT: /*0*/
                printf("%s %i %s", game_str_sb_destr, snum, (snum == 1) ? game_str_sb_fact2 : game_str_sb_facts);
                break;
            case UI_SABOTAGE_BASES: /*1*/
                printf("%s %i %s", game_str_sb_destr, snum, (snum == 1) ? game_str_sb_mbase : game_str_sb_mbases);
                break;
            case UI_SABOTAGE_REVOLT: /*2*/
                if (p->unrest == PLANET_UNREST_REBELLION) {
                    printf("%s", game_str_sb_increv);
                } else {
                    int v = (p->pop == 0) ? 0 : ((p->rebels * 100) / p->pop);
                    printf("%s %i %s %i%%.", game_str_sb_inc1, snum, game_str_sb_inc2, v);
                }
                break;
        }
    } else {
        printf("%s ", game_str_sb_failed);
        switch (act) {
            default:
            case UI_SABOTAGE_FACT: /*0*/
                if (p->factories == 0) {
                    fputs(game_str_sb_nofact, stdout);
                }
                break;
            case UI_SABOTAGE_BASES: /*1*/
                if (p->missile_bases == 0) {
                    fputs(game_str_sb_nobases, stdout);
                }
                break;
            case UI_SABOTAGE_REVOLT: /*2*/
                fputs(game_str_sb_noinc, stdout); /* FIXME never happens? */
                break;
        }
    }
    putchar('\n');
    if (other2 == PLAYER_NONE) {
        ui_switch_wait(g);
        return PLAYER_NONE;
    } else {
        struct input_list_s rl_in[] = {
            { 0, "1", NULL, 0 },
            { 1, "2", NULL, 0 },
            { PLAYER_NONE, "Q", NULL, game_str_mm_quit },
            { 0, NULL, NULL, NULL },
        };
        rl_in[0].value = other1;
        rl_in[0].display = game_str_tbl_races[g->eto[other1].race];
        rl_in[0].value = other2;
        rl_in[0].display = game_str_tbl_races[g->eto[other2].race];
        printf("Sabotage | %s | %s\n", p->name, game_str_sb_frame);
        return ui_input_list(game_str_nt_victim, "> ", rl_in);
    }
}
