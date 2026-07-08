#include "config.h"

#include <stdio.h>
#include <string.h>

#include "uiempire.h"
#include "comp.h"
#include "game.h"
#include "game_misc.h"
#include "game_str.h"
#include "uidefs.h"

/* -------------------------------------------------------------------------- */

static int ui_empire_slider(struct input_token_s *param, int base)
{
    int v;
    if (param[0].type == INPUT_TOKEN_NUMBER) {
        v = param[0].data.num;
    } else if (param[0].type == INPUT_TOKEN_RELNUMBER) {
        v = base + param[0].data.num;
    } else {
        return -1;
    }
    SETRANGE(v, 0, 200);
    return v;
}

/* -------------------------------------------------------------------------- */

int ui_cmd_empire_look(struct game_s *g, int api, struct input_token_s *param, int num_param, void *var)
{
    const empiretechorbit_t *e = &(g->eto[api]);
    char buf[64];
    printf("%s, %s\n", game_str_tbl_races[e->race], g->emperor_names[api]);
    printf("- %s: ", game_str_pl_spending);
    game_print_prod_of_total(g, api, e->ship_maint_bc, buf, sizeof(buf));
    printf("Ships %s", buf);
    game_print_prod_of_total(g, api, e->bases_maint_bc, buf, sizeof(buf));
    printf(", Bases %s", buf);
    {
        int v = 0;
        for (player_id_t i = PLAYER_0; i < PLAYER_NUM; ++i) {
            if (i != api) {
                v += e->spying[i];
            }
        }
        printf(", Spying %i.%i%%", v / 10, v % 10);
    }
    {
        int v = e->security;
        printf(", Security %i.%i%%\n", v / 10, v % 10);
    }
    printf("- %s: Trade %i %s, Planets %i %s\n", game_str_pl_tincome, e->total_trade_bc, game_str_bc, e->total_production_bc, game_str_bc);
    printf("- %s: %i %s\n", game_str_pl_reserve, e->reserve_bc, game_str_bc);
    {
        int tax, bc;
        tax = e->tax;
        bc = (tax * e->total_production_bc) / 2000;
        printf("- Tax: %i.%i%% +%i %s\n", tax / 10, tax % 10, bc, game_str_bc);
    }
    {
        int v = (e->security / 5);
        if (e->race == RACE_DARLOK) {
            v += 20;
        }
        printf("- Security: %i%%\n", v);
    }
    return 0;
}

int ui_cmd_empire_tax(struct game_s *g, int api, struct input_token_s *param, int num_param, void *var)
{
    empiretechorbit_t *e = &(g->eto[api]);
    int v = ui_empire_slider(param, e->tax);
    if (v < 0) {
        return -1;
    }
    e->tax = v;
    return 0;
}

int ui_cmd_empire_security(struct game_s *g, int api, struct input_token_s *param, int num_param, void *var)
{
    empiretechorbit_t *e = &(g->eto[api]);
    int v = ui_empire_slider(param, e->security);
    if (v < 0) {
        return -1;
    }
    e->security = v;
    return 0;
}
