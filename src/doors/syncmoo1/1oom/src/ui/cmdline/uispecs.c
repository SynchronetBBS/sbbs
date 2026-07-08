#include "config.h"

#include <stdio.h>
#include <string.h>

#include "uifleet.h"
#include "boolvec.h"
#include "comp.h"
#include "game.h"
#include "game_parsed.h"
#include "game_shiptech.h"
#include "game_str.h"
#include "uidefs.h"
#include "util.h"

/* -------------------------------------------------------------------------- */

static void ui_specs_print(const shipparsed_t *sp, uint32_t shipcount, int cost)
{
    printf("- %s, %i, %s", sp->name, shipcount, game_str_tbl_st_hull[sp->hull]);
    if (cost >= 0) {
        printf(", %s %i %s", game_str_sp_cost, cost, game_str_bc);
    }
    putchar('\n');
    printf("  bdef %i, mdef %i, att %i, hits %i, shield %i, warp %i, speed %i\n", sp->defense, sp->misdefense, sp->complevel, sp->hp, sp->absorb, sp->engine + 1, sp->man);
    for (int wi = 0; wi < WEAPON_SLOT_NUM; ++wi) {
        int wn;
        wn = sp->wpnn[wi];
        if (wn != 0) {
            weapon_t wt;
            int ns;
            wt = sp->wpnt[wi];
            printf("  %2i %s", wn, *tbl_shiptech_weap[wt].nameptr);
            /* FIXME make plural form somehow modifiable */
            if ((wn != 1) && (wt != WEAPON_DEATH_SPORES)) {
                putchar('S');
            }
            ns = tbl_shiptech_weap[wt].numshots;
            if (ns > 0) {
                printf(" x%i", ns);
            }
            putchar('\n');
        }
    }
    for (int i = 0; i < SPECIAL_SLOT_NUM; ++i) {
        if (sp->special[i] != 0) {
            printf(" %s\n", game_str_tbl_st_specsh[sp->special[i]]);
        }
    }
}

/* -------------------------------------------------------------------------- */

int ui_cmd_fleet_specs(struct game_s *g, int api, struct input_token_s *param, int num_param, void *var)
{
    const empiretechorbit_t *e = &(g->eto[api]);
    const shipresearch_t *srd = &(g->srd[api]);
    for (int si = 0; si < e->shipdesigns_num; ++si) {
        shipparsed_t sp;
        const shipdesign_t *sd;
        sd = &(srd->design[si]);
        game_parsed_from_design(&sp, sd, 1);
        ui_specs_print(&sp, srd->shipcount[si], sd->cost);
    }
    return 0;
}
