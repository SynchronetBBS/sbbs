#include "config.h"

#include <stdio.h>
#include <stdlib.h> /* abs */
#include <string.h>

#include "game_turn.h"
#include "boolvec.h"
#include "comp.h"
#include "game.h"
#include "game_ai.h"
#include "game_audience.h"
#include "game_aux.h"
#include "game_battle.h"
#include "game_bomb.h"
#include "game_design.h"
#include "game_diplo.h"
#include "game_election.h"
#include "game_end.h"
#include "game_event.h"
#include "game_fleet.h"
#include "game_ground.h"
#include "game_misc.h"
#include "game_news.h"
#include "game_num.h"
#include "game_shiptech.h"
#include "game_spy.h"
#include "game_stat.h"
#include "game_str.h"
#include "game_tech.h"
#include "lib.h"
#include "log.h"
#include "rnd.h"
#include "types.h"
#include "ui.h"
#include "util.h"
#include "util_math.h"

/* -------------------------------------------------------------------------- */

int copyprot_status = 0;

static void game_turn_limit_ships(struct game_s *g)
{
    for (int ei = 0; ei < g->enroute_num; ++ei) {
        fleet_enroute_t *r = &(g->enroute[ei]);
        for (int si = 0; si < NUM_SHIPDESIGNS; ++si) {
            SETMIN(r->ships[si], game_num_limit_ships);
        }
        r->retreat = false;
    }
    for (int i = 0; i < g->galaxy_stars; ++i) {
        for (player_id_t j = PLAYER_0; j < g->players; ++j) {
            fleet_orbit_t *r = &(g->eto[j].orbit[i]);
            for (int si = 0; si < NUM_SHIPDESIGNS; ++si) {
                SETMIN(r->ships[si], game_num_limit_ships);
            }
        }
    }
    game_remove_empty_fleets(g);
}

static void game_turn_countdown_ceasefire(struct game_s *g)
{
    for (player_id_t j = PLAYER_0; j < g->players; ++j) {
        for (player_id_t i = PLAYER_0; i < g->players; ++i) {
            if (g->evn.ceasefire[j][i] > 0) {
                --g->evn.ceasefire[j][i];
            }
        }
    }
}

static void game_turn_update_mood_blunder(struct game_s *g)
{
    for (player_id_t i = PLAYER_0; i < g->players; ++i) {
        empiretechorbit_t *e = &(g->eto[i]);
        for (player_id_t j = PLAYER_1; j < g->players; ++j) {
            if (i != j) {
                int16_t v;
                v = e->diplo_type[j];
                if ((v >= 4) && (v <= 11)) {
                    e->blunder[j] = v;
                } else if ((v >= 42) && (v <= 49)) {
                    e->blunder[j] = v - 30;
                } else if ((v >= 50) && (v <= 57)) {
                    e->blunder[j] = v - 46;
                }
                if (e->treaty[j] != TREATY_FINAL_WAR) {
                    v = e->mood_treaty[j];
                    if (v < 0) {
                        v += rnd_1_n(5, &g->seed);
                    }
                    if (v < 50) {
                        v += rnd_1_n(5, &g->seed);
                    }
                    e->mood_treaty[j] = v;
                    v = e->mood_trade[j];
                    if (v < 0) {
                        v += rnd_1_n(5, &g->seed);
                    }
                    if (v < 50) {
                        v += rnd_1_n(5, &g->seed);
                    }
                    e->mood_trade[j] = v;
                    v = e->mood_tech[j];
                    if (v < 0) {
                        v += rnd_1_n(5, &g->seed);
                    }
                    if (v < 50) {
                        v += rnd_1_n(5, &g->seed);
                    }
                    e->mood_tech[j] = v;
                    v = e->mood_peace[j];
                    if (v < 0) {
                        v += rnd_1_n(5, &g->seed);
                    }
                    if (v < 50) {
                        v += rnd_1_n(5, &g->seed);
                    }
                    SETRANGE(v, -200, 200);
                    e->mood_peace[j] = v;
                }
            }
        }
    }
    for (player_id_t i = PLAYER_0; i < g->players; ++i) {
        empiretechorbit_t *e = &(g->eto[i]);
        for (player_id_t j = PLAYER_0; j < g->players; ++j) {
            e->diplo_val[j] = 0;
            e->diplo_type[j] = 0;
            if (e->bounty_collect[j] != PLAYER_NONE) {
                e->attack_bounty[j] = PLAYER_NONE;
                e->bounty_collect[j] = PLAYER_NONE;
            }
            e->au_tech_trade_num[j] = 0;
            e->offer_tech[j] = 0;
            e->au_want_tech[j] = 0;
            e->offer_bc[j] = 0;
        }
    }
}

static void game_turn_init_z_finished(struct game_s *g)
{
    memset(g->evn.spies_caught, 0, sizeof(g->evn.spies_caught));
    memset(g->evn.new_ships, 0, sizeof(g->evn.new_ships));
    for (int i = 0; i < g->galaxy_stars; ++i) {
        planet_t *p = &(g->planet[i]);
        BOOLVEC_CLEAR(p->finished, FINISHED_NUM);
        BOOLVEC_CLEAR(p->unrefuel, PLAYER_NUM);
    }
    for (int i = 0; i < g->players; ++i) {
        empiretechorbit_t *e = &(g->eto[i]);
        BOOLVEC_CLEAR(e->contact_broken, PLAYER_NUM);
    }
    memset(g->evn.build_finished_num, 0, sizeof(g->evn.build_finished_num));
}

static void game_turn_send_transport(struct game_s *g)
{
    for (int i = 0; i < g->galaxy_stars; ++i) {
        planet_t *p = &(g->planet[i]);
        if ((p->owner != PLAYER_NONE) && (p->trans_num > 0)) {
            game_send_transport(g, p);
        }
    }
}

static int game_turn_build_eco_sub1(int fact, int colonist_oper_fact, int pop1, int waste, int max_pop2)
{
    if (game_num_waste_calc_fix) {
        /* WASBUG
           MOO1 adds waste twice using two separate pieces of code.
           The first time (see above) is as described in OSG.
           The second time (see below, from game_turn_build_eco_sub1) has several issues:
           - p->pop * e->colonist_oper_factories * e->colonist_oper_factories / 10 makes no sense
           - since p->waste is not limited at this point, the value added can be negative, leading to Silicoid planets with 0 waste
           - the cost of eco restoration is already calculated; as the slider is typically set to Clean (ecorestore <= ecoprod) the point is mostly moot
           Speculation: Someone added the above working code and forgot to remove this broken code.
        */
        return 0;
    }
    int v;
    v = pop1 * colonist_oper_fact;
    SETMIN(fact, v);
    v = (fact * colonist_oper_fact) / 10;
    return ((100 - ((waste * 100) / max_pop2)) * v) / 100;
}

static inline void game_add_planet_to_eco_finished(struct game_s *g, uint8_t pli, player_id_t owner)
{
    BOOLVEC_SET1(g->planet[pli].finished, FINISHED_SOILATMOS);
}

static void game_turn_build_eco(struct game_s *g)
{
    for (int i = 0; i < g->galaxy_stars; ++i) {
        planet_t *p = &(g->planet[i]);
        player_id_t owner;
        owner = p->owner;
        p->pop_prev = (owner != PLAYER_NONE) ? p->pop : 0;
        if ((owner != PLAYER_NONE) && (p->type != PLANET_TYPE_NOT_HABITABLE) && (p->pop != 0)) {
            const empiretechorbit_t *e;
            int ecorestore, ecoprod;
            e = &(g->eto[owner]);
            {
                int v, fact, waste;
                fact = p->factories;
                v = p->pop * game_planet_get_pop_oper_fact(g, p);
                SETMIN(fact, v);
                waste = (fact * e->ind_waste_scale) / 10;
                p->waste += ((100 - ((p->waste * 100) / p->max_pop2)) * waste) / 100;
            }
            if (p->unrest == PLANET_UNREST_REBELLION) {
                p->waste = 0;
            }
            ecorestore = (e->race != RACE_SILICOID) ? (p->waste / e->have_eco_restoration_n) : 0;
            p->waste += game_turn_build_eco_sub1(p->factories, game_planet_get_pop_oper_fact(g, p), p->pop, p->waste, p->max_pop2);
            ecoprod = (p->slider[PLANET_SLIDER_ECO] * p->prod_after_maint) / 100;
            if (e->race != RACE_SILICOID) {
                if (ecorestore > ecoprod) {
                    p->waste -= e->have_eco_restoration_n * ecoprod;
                    ecoprod = 0;
                } else {
                    ecoprod -= ecorestore;
                    p->waste = 0;
                }
            }
            SETMAX(p->waste, 0);
            if ((p->max_pop2 - p->waste) < 10) {
                p->waste = p->max_pop2 - 10;
            }
            SETMAX(p->waste, 0);
            if ((ecoprod > 0) && e->have_atmos_terra && (p->growth == PLANET_GROWTH_HOSTILE)) {
                p->bc_to_ecoproj += ecoprod;
                if (p->bc_to_ecoproj >= game_num_atmos_cost) {
                    game_turn_atmos_tform(p);
                    ecoprod = p->bc_to_ecoproj - game_num_atmos_cost;
                    p->bc_to_ecoproj = 0;
                    /* WASBUG MOO1 did p->bc_to_ecoproj -= game_num_atmos_cost
                     * instead which allowed double-spending of credits if
                     * p->bc_to_ecoproj > game_num_atmos_cost. */
                    game_add_planet_to_eco_finished(g, i, owner);
                } else {
                    ecoprod = 0;
                }
            }
            if ((ecoprod > 0) && e->have_soil_enrich && (p->growth == PLANET_GROWTH_NORMAL)) {
                p->bc_to_ecoproj += ecoprod;
                if (p->bc_to_ecoproj > game_num_soil_cost) {
                    game_turn_soil_enrich(p, (int)e->have_terraform_n, false);
                    ecoprod = p->bc_to_ecoproj - game_num_soil_cost;
                    /* p->bc_to_ecoproj carries over as partial progress
                     * towards advanced soil enrichment. */
                    SETMIN(p->bc_to_ecoproj, game_num_soil_cost);
                    /* WASBUG MOO1 did not do the SETMIN, allowing
                     * double-spending again. */
                    game_add_planet_to_eco_finished(g, i, owner);
                } else {
                    ecoprod = 0;
                }
            }
            if ((ecoprod > 0) && e->have_adv_soil_enrich && (p->growth < PLANET_GROWTH_GAIA) && (p->growth > PLANET_GROWTH_HOSTILE)) {
                p->bc_to_ecoproj += ecoprod;
                if (p->bc_to_ecoproj > game_num_adv_soil_cost) {
                    game_turn_soil_enrich(p, (int)e->have_terraform_n, true);
                    ecoprod = p->bc_to_ecoproj - game_num_adv_soil_cost;
                    p->bc_to_ecoproj = 0;
                    /* WASBUG MOO1 did not reset p->bc_to_ecoproj to 0, but
                     * the planet may become hostile due to a random event and
                     * then this variable becomes relevant again. */
                    game_add_planet_to_eco_finished(g, i, owner);
                } else {
                    ecoprod = 0;
                }
            }
            if ((ecoprod > 0) && ((p->max_pop3 - p->max_pop2) < e->have_terraform_n) && (p->max_pop3 < game_num_max_pop)) {
                int v, tmax;
                v = ecoprod / e->terraform_cost_per_inc;
                tmax = e->have_terraform_n + p->max_pop2;
                if (tmax < (p->max_pop3 + v)) {
                    ecoprod -= (tmax - p->max_pop3) * e->terraform_cost_per_inc;
                    p->max_pop3 = tmax;
                } else {
                    p->max_pop3 += v;
                    ecoprod = 0;
                }
            }
            SETMIN(p->max_pop3, game_num_max_pop);
            SETMAX(ecoprod, 0);
            {
                int v;
                v = game_get_pop_growth_max(g, p, p->max_pop3) + game_get_pop_growth_for_eco(g, p, ecoprod) + p->pop_tenths;
                p->pop += v / 10;
                p->pop_tenths = v % 10;
            }
            SETRANGE(p->pop, 0, p->max_pop3);
        }
    }
}

static void game_turn_update_trade(struct game_s *g)
{
    for (player_id_t i = PLAYER_0; i < g->players; ++i) {
        empiretechorbit_t *e = &(g->eto[i]);
        for (player_id_t j = i; j < g->players; ++j) {
            empiretechorbit_t *e2 = &(g->eto[j]);
            uint16_t bc;
            bc = e->trade_bc[j];
            if (bc != 0) {
                if (BOOLVEC_IS0(e->contact, j)) {
                    e->trade_bc[j] = 0;
                    e->trade_percent[j] = 0;
                    e->trade_established_bc[j] = 0;
                } else {
                    uint16_t estbc;
                    int16_t v;
                    estbc = e->trade_established_bc[j];
                    if (estbc < bc) {   /* FIXME BUG? never true ; both are set to bc */
                        estbc += bc / 10;
                        SETMIN(estbc, bc);
                        e->trade_established_bc[j] = estbc;
                        e2->trade_established_bc[i] = estbc;
                    }
                    v = (rnd_1_n(200, &g->seed) + e->relation1[j] + 25) / 60;
                    SETMAX(v, 0);
                    ADDSATT(e->trade_percent[j], v, 100);
                    ADDSATT(e2->trade_percent[i], v, 100);
                }
            }
        }
    }
}

static void game_turn_diplo_adjust(struct game_s *g)
{
    for (player_id_t i = PLAYER_0; i < g->players; ++i) {
        empiretechorbit_t *e = &(g->eto[i]);
        for (player_id_t j = i + 1; j < g->players; ++j) {
            if (!rnd_0_nm1(2, &g->seed)) {
                if (e->treaty[j] == TREATY_NONAGGRESSION) {
                    game_diplo_act(g, rnd_1_n(3, &g->seed), i, j, 0, 0, 0);
                }
                if (e->treaty[j] == TREATY_ALLIANCE) {
                    game_diplo_act(g, rnd_1_n(6, &g->seed), i, j, 0, 0, 0);
                }
                if (e->trade_bc[j] != 0) {
                    game_diplo_act(g, rnd_1_n(3, &g->seed), i, j, 2, 0, 0);
                }
            }
        }
    }
    for (player_id_t i = PLAYER_0; i < g->players; ++i) {
        int s_ai;
        if (IS_HUMAN(g, i)) {
            continue;
        }
        s_ai = game_stat_fleet(g, i);
        for (player_id_t j = PLAYER_0; j < g->players; ++j) {
            int s_h;
            if (IS_AI(g, j)) {
                continue;
            }
            s_h = game_stat_fleet(g, j);
            if (1
              && (s_ai < s_h) && (s_ai > 0)
              && (rnd_1_n(100, &g->seed) >= ((s_h * 50) / s_ai))
              && (!rnd_0_nm1(20, &g->seed))
            ) {
                game_diplo_act(g, -10, j, i, 8, 0, 0);
            }
        }
    }
    if ((g->year & 1) == 0) {
        uint8_t tbl_num_pp[PLAYER_NUM];
        int gscale = ((g->galaxy_size + 1) * 3);
        int gdiv = g->galaxy_size + 6 - g->difficulty;
        for (player_id_t i = PLAYER_0; i < g->players; ++i) {
            tbl_num_pp[i] = 0;
        }
        for (int j = 0; j < g->galaxy_stars; ++j) {
            planet_t *p = &(g->planet[j]);
            if (p->prod_after_maint >= 100) {
                ++tbl_num_pp[p->owner];
            }
        }
        for (player_id_t i = PLAYER_0; i < g->players; ++i) {
            empiretechorbit_t *e = &(g->eto[i]);
            if ((gscale < tbl_num_pp[i]) && !rnd_0_nm1(4, &g->seed)) {
                int v;
                v = rnd_1_n(4, &g->seed) * ((-(tbl_num_pp[i] - gscale)) / gdiv);
                v = (v / 3) * 4;
                if (IS_HUMAN(g, i)) {
                    v /= 2;
                }
                for (player_id_t j = PLAYER_0; j < g->players; ++j) {
                    if (IS_HUMAN(g, j)) {
                        continue;
                    }
                    if (BOOLVEC_IS1(e->contact, j) && (e->treaty[j] == TREATY_ALLIANCE)) {
                        game_diplo_act(g, v, i, j, 12, 0, 0);
                    }
                }
            }
        }
    }
    for (player_id_t i = PLAYER_0; i < g->players; ++i) {
        empiretechorbit_t *e = &(g->eto[i]);
        if (IS_HUMAN(g, i)) {
            continue;
        }
        for (player_id_t j = PLAYER_0; j < g->players; ++j) {
            if ((i != j) && (e->diplo_val[j] == 0)) {
                int16_t r1, r2;
                uint8_t v;
                r1 = e->relation1[j];
                r2 = e->relation2[j];
                v = (abs(r1) > rnd_1_n(105, &g->seed)) ? rnd_0_nm1(2, &g->seed) : 0;
                if (r1 < r2) {
                    r1 += v;
                    if (r1 > r2) {
                        r1 = r2;
                    }
                } else {
                    r1 -= v;
                    if (r1 < r2) {
                        r1 = r2;
                    }
                }
                SETRANGE(r1, -100, 100);
                e->relation1[j] = r1;
            }
        }
    }
    for (player_id_t i = PLAYER_0; i < g->players; ++i) {
        empiretechorbit_t *e = &(g->eto[i]);
        for (player_id_t j = PLAYER_0; j < g->players; ++j) {
            if (i == j) {
                e->relation1[j] = 0;
            } else {
                g->eto[j].relation1[i] = e->relation1[j];
            }
        }
    }
}

static void game_add_planet_to_shield_finished(struct game_s *g, uint8_t pli, player_id_t owner)
{
    BOOLVEC_SET1(g->planet[pli].finished, FINISHED_SHIELD);
}

static void game_turn_build_def(struct game_s *g)
{
    int16_t cost_new[PLAYER_NUM], cost_diff[PLAYER_NUM];
    for (player_id_t i = PLAYER_0; i < g->players; ++i) {
        empiretechorbit_t *e = &(g->eto[i]);
        int16_t cost_old;
        cost_old = game_get_base_cost(g, i);
        e->base_weapon = game_get_base_weapon(g, i, e->tech.percent[TECH_FIELD_WEAPON]);
        e->base_shield = game_get_best_shield(g, i, e->tech.percent[TECH_FIELD_FORCE_FIELD]);
        e->base_comp = game_get_best_comp(g, i, e->tech.percent[TECH_FIELD_COMPUTER]);
        cost_new[i] = game_get_base_cost(g, i);
        cost_diff[i] = cost_new[i] - cost_old;
        SETMAX(cost_diff[i], 0);
    }
    for (int i = 0; i < g->galaxy_stars; ++i) {
        planet_t *p = &(g->planet[i]);
        player_id_t owner;
        owner = p->owner;
        if (owner != PLAYER_NONE) {
            empiretechorbit_t *e = &(g->eto[owner]);
            int prod, toup;
            prod = game_adjust_prod_by_special((p->slider[PLANET_SLIDER_DEF] * p->prod_after_maint) / 100, p->special);
            prod += p->bc_to_base;
            toup = cost_diff[owner] * p->missile_bases + p->bc_upgrade_base;
            if (toup >= prod) {
                toup -= prod;
                p->bc_upgrade_base = toup;
                p->bc_to_base = 0;
            } else {
                int tosh;
                prod -= toup;
                p->bc_upgrade_base = 0;
                tosh = e->planet_shield_cost - p->bc_to_shield;
                SETMAX(tosh, 0);
                if ((p->battlebg == 0) || game_xy_is_in_nebula(g, p->x, p->y)) { /* FIXME later check is redundant */
                    tosh = 0;
                    p->bc_to_shield = 0;
                }
                if (prod <= tosh) { /* FIXME just "<" ? */
                    p->bc_to_shield += prod;
                    prod = 0;
                } else {
                    prod -= tosh;
                    p->bc_to_shield = e->planet_shield_cost;
                    while (prod >= cost_new[owner]) {
                        prod -= cost_new[owner];
                        ++p->missile_bases;
                    }
                }
                p->bc_to_base = prod;
            }
            {
                uint8_t newshield;
                int curcost;
                curcost = MIN(e->planet_shield_cost, p->bc_to_shield);
                newshield = 0;
                for (int j = 1; (j < PSHIELD_NUM) && (curcost >= game_num_pshield_cost[j]); ++j) {
                    newshield += 5;
                }
                if (newshield != p->shield) {
                    p->shield = newshield;
                    if (newshield == g->eto[owner].have_planet_shield) {
                        game_add_planet_to_shield_finished(g, i, owner);
                    }
                }
            }
        }
    }
}

static inline void game_add_planet_to_stargate_finished(struct game_s *g, uint8_t pli, player_id_t owner)
{
    BOOLVEC_SET1(g->planet[pli].finished, FINISHED_STARGATE);
}

static void game_turn_build_ship(struct game_s *g)
{
    game_update_maint_costs(g);
    for (player_id_t i = PLAYER_0; i < g->players; ++i) {
        empiretechorbit_t *e = &(g->eto[i]);
        shipresearch_t *srd = &(g->srd[i]);
        struct game_design_s gd;
        gd.sd_num = e->shipdesigns_num;
        gd.player_i = i;
        memcpy(gd.percent, e->tech.percent, sizeof(gd.percent));
        for (int j = 0; j < gd.sd_num; ++j) {
            shipdesign_t *sd = &(srd->design[j]);
            gd.sd = *sd;
            sd->cost = game_design_calc_cost(&gd);
        }
    }
    for (int i = 0; i < g->galaxy_stars; ++i) {
        planet_t *p = &(g->planet[i]);
        player_id_t owner;
        owner = p->owner;
        if (owner != PLAYER_NONE) {
            empiretechorbit_t *e = &(g->eto[owner]);
            shipresearch_t *srd = &(g->srd[owner]);
            int prod;
            uint8_t si;
            prod = game_adjust_prod_by_special((p->slider[PLANET_SLIDER_SHIP] * p->prod_after_maint) / 100, p->special);
            prod += p->bc_to_ship;
            /* BUG: A 1 BC bonus for having the slider > 0.
               This bonus is not taken into account by UI */
            if (p->slider[PLANET_SLIDER_SHIP] > 0) {
                ++prod;
            }
            si = p->buildship;
            if (si == BUILDSHIP_STARGATE) {
                if (prod >= game_num_stargate_cost) {
                    p->have_stargate = true;
                    p->buildship = 0;
                    p->bc_to_ship = prod - game_num_stargate_cost;
                    game_add_planet_to_stargate_finished(g, i, owner);
                } else {
                    p->bc_to_ship = prod;
                }
            } else {
                shipdesign_t *sd = &(srd->design[si]);
                int cost, shipnum;
                uint8_t dest;
                cost = sd->cost;
                shipnum = 0;
                while (prod >= cost) {
                    ++shipnum;
                    prod -= cost;
                }
                if ((shipnum + srd->shipcount[si]) > game_num_limit_ships_all) {
                    shipnum = game_num_limit_ships_all - srd->shipcount[si];
                }
                SETMAX(shipnum, 0);
                srd->shipcount[si] += shipnum;
                dest = p->reloc;
                if (dest == i) {
                    e->orbit[i].ships[si] += shipnum;
                    if (shipnum > 0) {
                        BOOLVEC_SET1(p->finished, FINISHED_SHIP);
                    }
                } else {
                    if (shipnum > 0) {
                        game_send_fleet_reloc(g, owner, i, dest, si, shipnum);
                    }
                }
                g->evn.new_ships[owner][si] += shipnum;
                p->bc_to_ship = prod;
            }
        }
    }
    game_update_visibility(g);
}

static void game_turn_reserve(struct game_s *g)
{
    for (int i = 0; i < g->galaxy_stars; ++i) {
        planet_t *p = &(g->planet[i]);
        player_id_t owner;
        owner = p->owner;
        if (owner != PLAYER_NONE) {
            uint32_t v, r;
            v = p->prod_after_maint / 2;
            r = p->reserve;
            if (v < r) {
                r -= v;
            } else {
                r = 0;
            }
            p->reserve = r;
        }
    }
    for (player_id_t i = PLAYER_0; i < g->players; ++i) {
        empiretechorbit_t *e = &(g->eto[i]);
        e->reserve_bc += (e->total_production_bc * e->tax) / 2000;
    }
}

static inline void game_add_planet_to_build_finished(struct game_s *g, uint8_t pli, player_id_t owner, uint8_t type)
{
    if (game_opt_message_filter[type]) {
        return;
    }
    planet_t *p = &(g->planet[pli]);
    BOOLVEC_SET1(p->finished, type);
    ++g->evn.build_finished_num[owner];
}

static bool game_turn_finished_switch_slider(struct game_s *g, struct planet_s *p, planet_slider_i_t from, planet_slider_i_t to)
{
    if (game_num_slider_respects_locks && (p->slider_lock[from] || p->slider_lock[to])) {
        return false;
    }
    if (from == to) {
        return false;
    }
    int v = p->slider[from];
    if (from == PLANET_SLIDER_ECO) {
        v -= game_planet_get_waste_percent(NULL, g, p, false);
        SETMAX(v, 0);
    }
    p->slider[from] -= v;
    p->slider[to] += v;
    return true;
}

static void game_turn_finished_disable_slider(struct game_s *g, struct planet_s *p, planet_slider_i_t from, bool build_ind)
{
    if (game_num_slider_respects_locks && p->slider_lock[from]) {
        return;
    }
    planet_slider_i_t first = build_ind ? PLANET_SLIDER_IND : PLANET_SLIDER_TECH;
    game_turn_finished_switch_slider(g, p, from, first)
    || game_turn_finished_switch_slider(g, p, from, PLANET_SLIDER_TECH)
    || game_turn_finished_switch_slider(g, p, from, PLANET_SLIDER_SHIP)
    || game_turn_finished_switch_slider(g, p, from, PLANET_SLIDER_DEF)
    || game_turn_finished_switch_slider(g, p, from, PLANET_SLIDER_IND)
    || game_turn_finished_switch_slider(g, p, from, PLANET_SLIDER_ECO);
}

static void game_turn_build_ind(struct game_s *g)
{
    for (int i = 0; i < g->galaxy_stars; ++i) {
        planet_t *p = &(g->planet[i]);
        player_id_t owner;
        owner = p->owner;
        if (owner != PLAYER_NONE) {
            empiretechorbit_t *e;
            uint16_t fact, factold, num;
            uint8_t cost, bonus;
            int prod, v;
            e = &(g->eto[owner]);
            factold = fact = p->factories;
            cost = game_planet_get_fact_adj_cost(g, p);
            prod = game_adjust_prod_by_special((p->slider[PLANET_SLIDER_IND] * p->prod_after_maint) / 100, p->special);
            prod += p->bc_to_factory;
            bonus = (e->race == RACE_MEKLAR) ? 2 : 0;
            v = e->colonist_oper_factories - p->pop_oper_fact - bonus;
            if (v > 0) {
                v = v * (e->factory_cost / 2);
                if (game_num_factory_cost_fix) {
                    if (e->race != RACE_MEKLAR) {
                        v = (e->factory_cost * p->factories) / 2;
                    } else {
                        v = 0;
                    }
                }
            } else {
                v = 0;
            }
            if ((prod / cost + fact) > (p->pop * p->pop_oper_fact)) {
                if (game_num_factory_cost_fix && (fact < (p->pop * p->pop_oper_fact))) {
                    num = p->pop * p->pop_oper_fact - fact;
                    fact = p->pop * p->pop_oper_fact;
                    prod -= num * cost;
                }
                p->bc_to_refit += prod;
                if (p->bc_to_refit >= v) {
                    if (game_num_factory_cost_fix && (e->race != RACE_MEKLAR)) {
                        ++p->pop_oper_fact;
                        SETMIN(p->pop_oper_fact, e->colonist_oper_factories);
                    } else {
                        p->pop_oper_fact = e->colonist_oper_factories;
                    }
                    prod = p->bc_to_refit - v;
                    p->bc_to_refit = 0;
                } else {
                    prod = 0;
                }
            }
            num = prod / cost;
            p->bc_to_factory = prod % cost;
            v = p->max_pop3 * game_planet_get_pop_oper_fact(g, p);
            if (v < (fact + num)) {
                v = (fact + num) - v;
                SETMAX(v, 0);
                SETMIN(v, num);
                num -= v;
            } else {
                v = 0;
            }
            if (game_num_factory_cost_fix && (p->pop_oper_fact < e->colonist_oper_factories)) {
                p->bc_to_refit += (v * cost) / 2;
            } else {
                e->reserve_bc += (v * cost) / 2;
            }
            fact += num;
            SETMIN(fact, game_num_max_factories);
            p->factories = fact;
            if ((fact != factold) && (fact >= (p->max_pop3 * e->colonist_oper_factories))) {
                if (IS_HUMAN(g, owner) && (p->slider[PLANET_SLIDER_IND] > 0)) {
                    game_turn_finished_disable_slider(g, p, PLANET_SLIDER_IND, false);
                }
                game_add_planet_to_build_finished(g, i, owner, FINISHED_FACT);
            }
        }
    }
}

static void game_turn_move_ships(struct game_s *g)
{
    void *ctx;
    bool local_multiplayer = g->gaux->local_players > 1, move_back = false;
    ctx = ui_gmap_basic_init(g, local_multiplayer);
    if (local_multiplayer) {
        memcpy(g->gaux->move_temp->enroute, g->enroute, g->enroute_num * sizeof(fleet_enroute_t));
        memcpy(g->gaux->move_temp->transport, g->transport, g->transport_num * sizeof(transport_t));
        g->gaux->move_temp->crystal = g->evn.crystal;
        g->gaux->move_temp->amoeba = g->evn.amoeba;
    }
    for (g->active_player = 0; g->active_player < g->players; ++g->active_player) {
        bool flag_more;
        if (BOOLVEC_IS1(g->is_ai, g->active_player)) {
            continue;
        }
        if (move_back) {
            memcpy(g->enroute, g->gaux->move_temp->enroute, g->enroute_num * sizeof(fleet_enroute_t));
            memcpy(g->transport, g->gaux->move_temp->transport, g->transport_num * sizeof(transport_t));
            g->evn.crystal = g->gaux->move_temp->crystal;
            g->evn.amoeba = g->gaux->move_temp->amoeba;
        }
        game_update_visibility(g);
        flag_more = true;
        ui_gmap_basic_start_player(ctx, g->active_player);
        for (int frame = 0; (frame < 20) && flag_more; ++frame) {
            bool odd_frame;
            game_update_visibility(g);
            odd_frame = frame & 1;
            ui_gmap_basic_start_frame(ctx, g->active_player);
            flag_more = false;
            for (int i = 0; i < g->enroute_num; ++i) {
                fleet_enroute_t *r = &(g->enroute[i]);
                if ((r->speed * 2) > frame) {
                    bool in_nebula;
                    int x, y;
                    x = r->x;
                    y = r->y;
                    in_nebula = (odd_frame || (frame == 0)) ? false : game_xy_is_in_nebula(g, x, y);
                    if (odd_frame || (frame == 0) || (!in_nebula)) {
                        int x1, y1;
                        const planet_t *p;
                        p = &(g->planet[r->dest]);
                        x1 = p->x;
                        y1 = p->y;
                        if (r->speed == FLEET_SPEED_STARGATE) {
                            x = x1;
                            y = y1;
                        } else {
                            flag_more = true;
                            util_math_go_line_dist(&x, &y, x1, y1, odd_frame ? 6 : 5);
                        }
                        r->x = x;
                        r->y = y;
                    } else if (in_nebula) {
                        flag_more = true;   /* WASBUG MOO1 stopped nebula movement early if no transports or faster ships enroute */
                    }
                }
            }
            for (int i = 0; i < g->transport_num; ++i) {
                transport_t *r = &(g->transport[i]);
                if ((r->speed * 2) > frame) {
                    bool in_nebula;
                    int x, y;
                    x = r->x;
                    y = r->y;
                    in_nebula = (!odd_frame) ? false : game_xy_is_in_nebula(g, x, y);
                    if ((!odd_frame) || (!in_nebula)) {
                        int x1, y1;
                        const planet_t *p;
                        p = &(g->planet[r->dest]);
                        x1 = p->x;
                        y1 = p->y;
                        if (r->speed == FLEET_SPEED_STARGATE) {
                            x = x1;
                            y = y1;
                        } else {
                            flag_more = true;
                            util_math_go_line_dist(&x, &y, x1, y1, odd_frame ? 6 : 5);
                        }
                        r->x = x;
                        r->y = y;
                    } else if (in_nebula) {
                        flag_more = true;   /* WASBUG MOO1 stopped nebula movement early if no fleets or faster ships enroute */
                    }
                }
            }
            for (int i = 0; i < 2; ++i) {
                monster_t *m;
                m = (i == 0) ? &(g->evn.crystal) : &(g->evn.amoeba);
                if (m->exists && (m->counter <= 0)) {
                    int x, y, x1, y1;
                    const planet_t *p;
                    p = &(g->planet[m->dest]);
                    x1 = p->x;
                    y1 = p->y;
                    x = m->x;
                    y = m->y;
                    if (((x != x1) || (y != y1)) && (frame < 2)) {
                        util_math_go_line_dist(&x, &y, x1, y1, odd_frame ? 6 : 5);
                        m->x = x;
                        m->y = y;
                    }
                }
            }
            ui_gmap_basic_draw_frame(ctx, g->active_player);
            ui_gmap_basic_finish_frame(ctx, g->active_player);
        }
        move_back = local_multiplayer;
    }
    ui_gmap_basic_shutdown(ctx);
    for (int i = 0; i < g->enroute_num; ++i) {
        fleet_enroute_t *r = &(g->enroute[i]);
        const planet_t *p;
        p = &(g->planet[r->dest]);
        if ((r->x == p->x) && (r->y == p->y)) {
            fleet_orbit_t *o;
            o = &(g->eto[r->owner].orbit[r->dest]);
            for (int j = 0; j < NUM_SHIPDESIGNS; ++j) {
                uint32_t s;
                s = o->ships[j] + r->ships[j];
                SETMIN(s, game_num_limit_ships);
                o->ships[j] = s;
            }
            util_table_remove_item_any_order(i, g->enroute, sizeof(fleet_enroute_t), g->enroute_num);
            --g->enroute_num;
            --i;
        }
    }
    game_update_visibility(g);
}

#if 0
static void game_turn_unrest_hmm1(struct game_s *g)
{
    /* this does nothing except churn the rng! */
    int tbl1[PLAYER_NUM], tbl2[PLAYER_NUM];
    for (player_id_t i = PLAYER_0; i < g->players; ++i) {
        tbl1[i] = 0;
        tbl2[i] = 0;
    }
    for (player_id_t i = PLAYER_0; i < g->players; ++i) {
        const planet_t *p;
        int pl, v;
        v = tbl2[i] * 2;   /* always 0 */
        pl = game_planet_get_random(g, i);
        if (pl != PLANET_NONE) {
            p = &(g->planet[pi]);
            /* WASBUG unrest != RESOLVED test done even when pl < 0 */
            if ((p->unrest != PLANET_UNREST_REBELLION) || (p->unrest != PLANET_UNREST_RESOLVED)) {
                if (rnd_1_n(100, &g->seed) <= v) {  /* never true */
                    p->unrest = PLANET_UNREST_REBELLION;
                    p->unrest_reported = false;
                    p->rebels = p->pop / 2;
                }
            }
        }
    }
}
#endif

static void game_turn_explore(struct game_s *g)
{
    for (int pli = 0; pli < g->galaxy_stars; ++pli) {
        planet_t *p = &(g->planet[pli]);
        p->artifact_looter = PLAYER_NONE;
        for (player_id_t i = PLAYER_0; i < g->players; ++i) {
            if (BOOLVEC_IS0(p->explored, i) || (p->owner == PLAYER_NONE)) {
                empiretechorbit_t *e = &(g->eto[i]);
                bool flag_visible, by_scanner;
                flag_visible = false;
                for (int j = 0; j < e->shipdesigns_num; ++j) {
                    if (e->orbit[pli].ships[j] > 0) {
                        flag_visible = true;
                        break;
                    }
                }
                by_scanner = false;
                if ((!flag_visible) && e->have_adv_scanner) {
                    for (int pli2 = 0; pli2 < g->galaxy_stars; ++pli2) {
                        const planet_t *p2 = &(g->planet[pli2]);
                        if ((p2->owner == i) && (util_math_dist_fast(p->x, p->y, p2->x, p2->y) <= game_num_adv_scan_range)) {
                            flag_visible = true;
                            break;
                        }
                    }
                    by_scanner = true;
                }
                /*c4ca*/
                if (flag_visible) {
                    bool first, flag_colony_ship, flag_do_colonize, was_explored;
                    int best_colonize, best_colonyship = 0;
                    /* FIXME artifacts disappearing due to scanning a planet is weird */
                    first = BOOLVEC_IS_CLEAR(p->explored, PLAYER_NUM);
                    if ((p->special == PLANET_SPECIAL_ARTIFACTS) && (!by_scanner) && first) {
                        /* FIXME? AIs (last player ID) win on simultaneous explore */
                        p->artifact_looter = i;
                    }
                    flag_colony_ship = false;
                    flag_do_colonize = false;
                    best_colonize = 200;
                    for (int j = 0; j < e->shipdesigns_num; ++j) {
                        const shipdesign_t *sd = &(g->srd[i].design[j]);
                        int can_colonize;
                        can_colonize = 200;
                        for (int k = 0; k < SPECIAL_SLOT_NUM; ++k) {
                            ship_special_t s;
                            s = sd->special[k];
                            if ((s >= SHIP_SPECIAL_STANDARD_COLONY_BASE) && (s <= SHIP_SPECIAL_RADIATED_COLONY_BASE)) {
                                can_colonize = PLANET_TYPE_MINIMAL - (s - SHIP_SPECIAL_STANDARD_COLONY_BASE);
                            }
                        }
                        if ((can_colonize < 200) && (e->orbit[pli].ships[j] > 0)) {
                            flag_colony_ship = true;
                            if (can_colonize < best_colonize) {
                                best_colonize = can_colonize;
                                best_colonyship = j;
                            }
                            if (e->race == RACE_SILICOID) {
                                best_colonize = PLANET_TYPE_RADIATED;
                            }
                        }
                    }
                    if (0
                      || (best_colonize == 200)
                      || (p->owner != PLAYER_NONE)
                      || (p->type == PLANET_TYPE_NOT_HABITABLE)
                      || (p->type < best_colonize)
                    ) {
                        best_colonize = 0;
                    }
                    was_explored = BOOLVEC_IS1(p->explored, i);
                    BOOLVEC_SET1(p->explored, i);
                    if (IS_HUMAN(g, i)) {
                        if ((best_colonize != 0) || (!was_explored)) {
                            game_update_visibility(g);  /* from explore_draw_cb */
                            if ((p->type < best_colonize) || (best_colonize == 0)) {
                                flag_colony_ship = false;
                            } else {
                                flag_colony_ship = true;
                            }
                            if (ui_explore(g, i, pli, by_scanner, flag_colony_ship) && flag_colony_ship) {
                                flag_do_colonize = true;
                            }
                        }
                    } else {
                        if (best_colonize != 0) {
                            flag_do_colonize = true;
                        }
                    }
                    if (flag_do_colonize) {
                        p->owner = i;
                        p->pop = 2;
                        if (1
                          && game_num_colonized_factories_fix
                          && (p->prev_owner != PLAYER_NONE)
                          && (p->prev_owner != i)) {
                            if (p->factories != 0) {
                                p->pop_oper_fact = 1;
                            } else {
                                p->pop_oper_fact = 2;
                            }
                            p->bc_to_refit = 0;
                        }
                        --e->orbit[pli].ships[best_colonyship];
                        if ((pli == g->evn.planet_orion_i) && game_num_news_orion) {
                            g->evn.have_orion_conquer = i + 1;
                        }
                    }
                }
            }
        }
    }
}

static int game_turn_transport_shoot(struct game_s *g, uint8_t planet_i, player_id_t rowner, uint16_t num_transports, uint8_t speed, player_id_t attacker, int bases, weapon_t basewpnt)
{
    const planet_t *p = &(g->planet[planet_i]);
    const empiretechorbit_t *ea = &(g->eto[attacker]);
    const empiretechorbit_t *ed = &(g->eto[rowner]);
    int totaldmg = 0, transport_hp, total_hp, complevel, killed;
    uint8_t bestcomp = 0, bestarmor = 0;
    const uint8_t *rc = &(g->srd[rowner].researchcompleted[TECH_FIELD_CONSTRUCTION][0]);

    /* Transports use the best armor that the owner has researched. */
    for (int i = 0; i < ed->tech.completed[TECH_FIELD_CONSTRUCTION]; ++i) {
        uint8_t tier;
        tech_group_t group;
        group = game_tech_get_group(g->gaux, TECH_FIELD_CONSTRUCTION, rc[i]);
        tier = game_tech_get_tier(g->gaux, TECH_FIELD_CONSTRUCTION, rc[i]);
        if (group == TECH_GROUP_ARMOR) {
            bestarmor = tier;
        }
    }
    transport_hp = ((bestarmor + 1) * 15);
    total_hp = transport_hp * num_transports;

    uint32_t tbl[WEAPON_NUM];
    memset(tbl, 0, sizeof(tbl));
    tbl[basewpnt] = bases * 24;
    for (int i = 0; i < ea->shipdesigns_num; ++i) {
        const shipdesign_t *sd = &(g->srd[attacker].design[i]);
        uint8_t comp;
        comp = sd->comp;
        if (comp > speed) { /* FIXME BUG ? */
            bestcomp = comp;
        }
        for (int j = 0; j < WEAPON_SLOT_NUM; ++j) {
            weapon_t wpnt = sd->wpnt[j];
            uint32_t v;
            v = ea->orbit[planet_i].ships[i] * sd->wpnn[j];
            if (v != 0) {
                const struct shiptech_weap_s *w;
                int ns;
                w = &(tbl_shiptech_weap[wpnt]);
                ns = w->numshots;
                if (ns == -1) {
                    ns = 4;
                }
                v *= ns;
                if (w->nummiss != 0) {
                    v *= w->nummiss;
                }
                SETMIN(v, game_num_max_trans_dmg);
                if (w->is_bomb || w->is_bio) {
                    v = 0;
                }
                if (w->misstype != 0) {
                    v /= 2;
                }
                tbl[wpnt] += (v * 2) / speed;
            }
        }
    }
    if (bases > 0) {
        bestcomp = game_get_best_comp(g, rowner, ed->tech.percent[TECH_FIELD_COMPUTER]);  /* FIXME should be ea ? */
    }
    complevel = (bestcomp - speed) * 2 + 12;
    SETRANGE(complevel, 1, 20);
    for (int i = 0; i < (game_num_orbital_weap_any ? WEAPON_NUM : WEAPON_CRYSTAL_RAY); ++i) {  /* WASBUG? excludes death ray and amoeba stream too */
        uint32_t vcur = tbl[i];
        if (vcur != 0) {
            const struct shiptech_weap_s *w = &(tbl_shiptech_weap[i]);
            int dmgmin, dmgmax;
            dmgmin = w->damagemin;
            dmgmax = w->damagemax;
            if (dmgmin == dmgmax) {
                for (uint32_t n = 0; n < vcur; ++n) {
                    if (rnd_1_n(20, &g->seed) <= complevel) {
                        totaldmg += dmgmax;
                    }
                }
            } else {
                /*7c47d*/
                int dmgrange;
                dmgrange = dmgmax - dmgmin + 1;
                --dmgmin;
                vcur = (complevel * vcur) / 20;
                for (uint32_t n = 0; n < vcur; ++n) {
                    int dmg;
                    dmg = rnd_1_n(dmgrange, &g->seed) + dmgmin;
                    if (dmg > 0) {
                        totaldmg += dmg;
                    }
                }
            }
            if (totaldmg >= total_hp) {
                break;
            }
        }
    }
    killed = totaldmg / transport_hp;
    SETMIN(killed, 1000);
    for (monster_id_t i = MONSTER_CRYSTAL; i <= MONSTER_AMOEBA; ++i) {
        monster_t *m;
        m = (i == MONSTER_CRYSTAL) ? &(g->evn.crystal) : &(g->evn.amoeba);
        if (m->exists && (m->killer == PLAYER_NONE) && (m->x == p->x) && (m->y == p->y)){
            killed = 1000;
        }
    }
    return killed;
}

static void game_turn_transport(struct game_s *g)
{
    for (int pli = 0; pli < g->galaxy_stars; ++pli) {
        planet_t *p = &(g->planet[pli]);
        for (player_id_t i = PLAYER_0; i < g->players; ++i) {
            p->inbound[i] = 0;
            p->total_inbound[i] = 0;
        }
    }
    for (int i = 0; i < g->transport_num; ++i) {
        transport_t *r = &(g->transport[i]);
        planet_t *p;
        uint8_t dest;
        dest = r->dest;
        p = &(g->planet[dest]);
        if ((r->x == p->x) && (r->y == p->y)) {
            int pop2, pop3;
            player_id_t owner;
            owner = r->owner;
            pop2 = pop3 = r->pop;
            for (int j = 0; j < g->players; ++j) {
                treaty_t t;
                if (j == owner) {
                    continue;
                }
                t = g->eto[owner].treaty[j];
                if ((j == p->owner) || (t == TREATY_NONE) || (t >= TREATY_WAR)) {
                    empiretechorbit_t *e;
                    e = &(g->eto[j]);
                    if (j == p->owner) {
                        pop3 -= game_turn_transport_shoot(g, dest, owner, pop3, r->speed, j, p->missile_bases, e->base_weapon);
                    } else {
                        /*e102*/
                        bool any_ships;
                        any_ships = false;
                        for (int k = 0; k < e->shipdesigns_num; ++k) {
                            if (e->orbit[dest].ships[k] > 0) {
                                any_ships = true;
                                break;
                            }
                        }
                        if (any_ships) {
                            pop3 -= game_turn_transport_shoot(g, dest, owner, pop3, r->speed, j, 0, WEAPON_NONE);
                        }
                    }
                }
            }
            if (g->evn.have_guardian && (dest == g->evn.planet_orion_i)) {
                pop3 = 0;
            }
            for (monster_id_t j = MONSTER_CRYSTAL; j <= MONSTER_AMOEBA; ++j) {
                monster_t *m;
                m = (j == MONSTER_CRYSTAL) ? &(g->evn.crystal) : &(g->evn.amoeba);
                if (m->exists && /*(m->killer == PLAYER_NONE) &&*/ (m->x == p->x) && (m->y == p->y)) { /* FIXME dead monster kills transports ? */
                    pop3 = 0;
                }
            }
            SETMAX(pop3, 0);
            if (g->eto[owner].have_combat_transporter) {
                int n;
                n = pop2 - pop3;
                if (game_num_combat_trans_fix) {    /* do as OSG says: 50% chance, 25% if interdictor */
                    int c;
                    c = g->eto[p->owner].have_sub_space_int ? 4 : 2;
                    for (int j = 0; j < n; ++j) {
                        if (!rnd_0_nm1(c, &g->seed)) {
                            ++pop3;
                        }
                    }
                } else if (g->eto[p->owner].have_sub_space_int) {   /* WASBUG transporters only work when planet owner has interdictor */
                    for (int j = 0; j < n; ++j) {
                        if (!rnd_0_nm1(4, &g->seed)) {
                            ++pop3;
                        } else if (!rnd_0_nm1(2, &g->seed)) {
                            ++pop3;
                        }
                    }
                }
            }
            /*e2a4*/
            if (pop3 <= 0) {
                if (IS_HUMAN(g, owner) || IS_HUMAN(g, p->owner)) {
                    char *buf = ui_get_strbuf();
                    const char *s;
                    if ((g->gaux->local_players == 1) && IS_HUMAN(g, owner)) {
                        s = game_str_sb_your;
                    } else {
                        s = game_str_tbl_race[g->eto[owner].race];
                    }
                    lib_sprintf(buf, UI_STRBUF_SIZE, "%s %s %s %s", s, game_str_sm_traad1, g->planet[dest].name, game_str_sm_traad2);
                    ui_turn_msg(g, IS_HUMAN(g, owner) ? owner : p->owner, buf);
                }
            } else if (p->owner == PLAYER_NONE) {
                /*e36d*/
                if (IS_HUMAN(g, owner)) {
                    char *buf = ui_get_strbuf();
                    lib_sprintf(buf, UI_STRBUF_SIZE, "%s %s %s", game_str_sm_trbdb1, g->planet[dest].name, game_str_sm_trbdb2);
                    ui_turn_msg(g, owner, buf);
                }
            } else {
                /*e3fe*/
                if ((p->owner == PLAYER_NONE) || (p->pop == 0)) {
                    if (IS_HUMAN(g, owner)) {
                        ui_landing(g, owner, dest);
                    }
                    p->pop = pop3;
                    p->bc_to_refit = 0;
                    p->pop_oper_fact = 1;
                    if (dest == g->evn.planet_orion_i) {
                        g->evn.have_orion_conquer = owner + 1;
                    }
                    p->owner = owner;
                } else if (p->owner == owner) {
                    if (p->unrest == PLANET_UNREST_REBELLION) {
                        ADDSATT(p->inbound[owner], pop3, game_num_max_inbound);
                        p->total_inbound[owner] += pop3;
                    } else {
                        ADDSATT(p->pop, pop3, p->max_pop3);
                    }
                } else {
                    /*e5a6*/
                    if (g->eto[owner].treaty[p->owner] != TREATY_ALLIANCE) {
                        ADDSATT(p->inbound[owner], pop3, game_num_max_inbound);
                        p->total_inbound[owner] += pop2;
                    }
                }
            }
            /*e639*/
            util_table_remove_item_any_order(i, g->transport, sizeof(transport_t), g->transport_num);
            --g->transport_num;
            --i;
        }
    }
}

static void game_turn_coup(struct game_s *g)
{
    uint8_t tbl_planets[PLAYER_NUM];
    uint8_t tbl_rebelplanets[PLAYER_NUM];
    memset(tbl_planets, 0, sizeof(tbl_planets));
    memset(tbl_rebelplanets, 0, sizeof(tbl_rebelplanets));
    BOOLVEC_CLEAR(g->evn.coup, PLAYER_NUM);
    for (int pli = 0; pli < g->galaxy_stars; ++pli) {
        planet_t *p = &(g->planet[pli]);
        if (p->owner != PLAYER_NONE) {
            ++tbl_planets[p->owner];
            if (p->unrest == PLANET_UNREST_REBELLION) {
                ++tbl_rebelplanets[p->owner];
            }
        }
    }
    for (player_id_t i = PLAYER_0; i < g->players; ++i) {
        if ((tbl_rebelplanets[i] >= (tbl_planets[i] + 1) / 2) && IS_ALIVE(g, i)) {
            empiretechorbit_t *e = &(g->eto[i]);
            e->trait2 = rnd_0_nm1(TRAIT2_NUM, &g->seed);
            e->trait1 = rnd_0_nm1(TRAIT1_NUM, &g->seed);
            /* WASBUG MOO1 reads an unitialized variable at BP-2 and check for it to be 0 or 1.
               It is the return address to the caller which is neither of the values, leading to the code below. */
            for (player_id_t j = PLAYER_0; j < g->players; ++j) {
                int16_t rel;
                rel = e->relation2[j];
                e->relation1[j] = rel;
                if (i != j) {
                    g->eto[j].relation1[i] = rel;
                    if (IS_HUMAN(g, j)) {
                        game_diplo_break_treaty(g, i, j);
                        game_diplo_break_trade(g, i, j);
                    }
                }
            }
            BOOLVEC_SET1(g->evn.coup, i);
            for (int pli = 0; pli < g->galaxy_stars; ++pli) {
                planet_t *p = &(g->planet[pli]);
                if (p->owner == i) {
                    p->unrest = PLANET_UNREST_NORMAL;
                    p->rebels = 0;
                }
            }
        }
    }
}

static bool game_turn_check_end(struct game_s *g, struct game_end_s *ge)
{
    player_id_t pih = PLAYER_NONE;
    for (player_id_t i = PLAYER_0; i < g->players; ++i) {
        if (IS_HUMAN(g, i) && IS_ALIVE(g, i)) {
            pih = i;
            break;
        }
    }
    if (g->end == GAME_END_LOST_EXILE) {
        ge->type = GAME_END_LOST_EXILE;
        if (pih == g->winner) {
            for (player_id_t i = pih + 1; i < g->players; ++i) {
                if (IS_HUMAN(g, i) && IS_ALIVE(g, i)) {
                    pih = i;
                    break;
                }
            }
        }
        ge->name = g->emperor_names[pih];
        return true;
    }
    if (g->end == GAME_END_WON_GOOD) {
        ge->type = GAME_END_WON_GOOD;
        ge->name = g->emperor_names[g->winner];
        ge->race = g->eto[g->winner].race;
        return true;
    }
    {
        player_id_t pi1 = PLAYER_NONE, pi2 = PLAYER_NONE;
        bool human_alive = false;
        uint8_t num_planets[PLAYER_NUM];
        uint8_t good_planets[PLAYER_NUM];
        memset(num_planets, 0, sizeof(num_planets));
        memset(good_planets, 0, sizeof(num_planets));
        for (int pli = 0; pli < g->galaxy_stars; ++pli) {
            const planet_t *p = &(g->planet[pli]);
            if (p->owner != PLAYER_NONE) {
                ++num_planets[p->owner];
                if (p->unrest != PLANET_UNREST_REBELLION) {
                    ++good_planets[p->owner];
                }
            }
        }
        for (player_id_t i = PLAYER_0; i < g->players; ++i) {
            if (num_planets[i] > 0) {
                if (pi1 == PLAYER_NONE) {
                    pi1 = i;
                } else if (pi2 == PLAYER_NONE) {
                    pi2 = i;
                }
                if (IS_HUMAN(g, i) && (good_planets[i] > 0) && IS_ALIVE(g, i)) {
                    human_alive = true;
                }
            }
        }
        if (pi2 == PLAYER_NONE) {
            ge->type = GAME_END_WON_TYRANT;
            ge->name = g->emperor_names[pi1];
            ge->race = g->eto[pi1].race;
            return true;
        }
        if (!human_alive) {
            struct news_s ns;
            int killer = g->gaux->human_killer;
            if (killer >= g->players) {
                killer = (pi1 != PLAYER_NONE) ? pi1 : 1;
            }
            ns.type = GAME_NEWS_GENOCIDE;
            ns.race = g->eto[pih].race;
            ns.subtype = 3;
            ns.num1 = pih;
            ui_news_start();
            ui_news(g, &ns);
            ge->type = GAME_END_LOST_FUNERAL;
            ge->race = g->eto[killer].banner;
            ge->banner_dead = pih;
            return true;
        }
    }
    return false;
}

static void game_turn_update_have_met(struct game_s *g)
{
    game_update_empire_contact(g);
    for (player_id_t i = PLAYER_0; i < g->players; ++i) {
        empiretechorbit_t *e = &(g->eto[i]);
        if (!IS_HUMAN(g, i)) {
            continue;
        }
        for (player_id_t j = PLAYER_0; j < g->players; ++j) {
            if ((i != j) && (!e->have_met[j]) && BOOLVEC_IS1(e->contact, j)) {
                e->have_met[j] = 1;
                e->treaty[j] = TREATY_NONE;
                g->eto[j].treaty[i] = TREATY_NONE;
            }
        }
    }
}

static void game_turn_audiences(struct game_s *g)
{
    for (player_id_t ph = PLAYER_0; ph < PLAYER_NUM; ++ph) {
        empiretechorbit_t *e = &(g->eto[ph]);
        if (!IS_HUMAN(g, ph)) {
            continue;
        }
        g->evn.newtech[ph].num = 0;
        for (player_id_t pa = PLAYER_0; pa < PLAYER_NUM; ++pa) {
            if (IS_AI(g, pa) && (e->diplo_type[pa] != 0)) {
                game_audience(g, ph, pa);
                e->diplo_type[pa] = 0;
            }
        }
        game_tech_finish_new(g, ph);
        ui_newtech(g, ph);
    }
}

static void game_turn_contact_broken(struct game_s *g, player_id_t pi, const BOOLVEC_PTRPARAMI(bv))
{
    empiretechorbit_t *e = &(g->eto[pi]);
    if (!IS_ALIVE(g, pi)) {
        return;
    }
    for (player_id_t i = PLAYER_0; i < g->players; ++i) {
        if ((i != pi) && IS_ALIVE(g, i) && BOOLVEC_IS0(e->contact, i) && BOOLVEC_IS1(bv, i)) {
            empiretechorbit_t *e2 = &(g->eto[i]);
            e->mood_trade[i] = 0;
            e->trade_bc[i] = 0;
            e->trade_percent[i] = 0;
            e->spymode[i] = SPYMODE_HIDE;
            BOOLVEC_SET1(e->contact_broken, i);
            e2->mood_trade[pi] = 0;
            e2->trade_bc[pi] = 0;
            e2->trade_percent[pi] = 0;
            e2->spymode[pi] = SPYMODE_HIDE;
            BOOLVEC_SET1(e2->contact_broken, pi);
        }
    }
}

static void game_turn_update_seen(struct game_s *g)
{
    game_update_visibility(g);
    for (uint8_t i = 0; i < g->galaxy_stars; ++i) {
        const planet_t *p = &(g->planet[i]);
        for (player_id_t pi = PLAYER_0; pi < g->players; ++pi) {
            bool can_see;
            can_see = (BOOLVEC_IS1(p->within_srange, pi) || (p->owner == pi));
            if (!can_see) { /* WASBUG handled only on game load (game_update_seen_by_orbit) */
                const shipcount_t *os = &(g->eto[pi].orbit[i].ships[0]);
                for (int j = 0; j < NUM_SHIPDESIGNS; ++j) {
                    if (os[j] != 0) {
                        can_see = true;
                        break;
                    }
                }
            }
            if (can_see) {
                g->seen[pi][i].owner = p->owner;
                g->seen[pi][i].pop = p->pop;
                g->seen[pi][i].bases = p->missile_bases;
                g->seen[pi][i].factories = p->factories;
            } else if ((p->owner != PLAYER_NONE) && BOOLVEC_IS1(g->eto[pi].contact, p->owner)) {
                g->seen[pi][i].owner = p->owner;
            }
        }
    }
}

static void game_turn_finished_slider(struct game_s *g)
{
    for (uint8_t pli = 0; pli < g->galaxy_stars; ++pli) {
        planet_t *p = &(g->planet[pli]);
        player_id_t pi;
        empiretechorbit_t *e;
        bool cond_build_ind;
        pi = p->owner;
        if ((pi == PLAYER_NONE) || IS_AI(g, pi)) {
            BOOLVEC_CLEAR(p->finished, FINISHED_NUM);
            continue;
        }
        e = &(g->eto[pi]);
        cond_build_ind = (p->factories < (p->pop * e->colonist_oper_factories));
        if (game_num_cond_switch_to_ind_fix) {
            cond_build_ind = (p->factories < (p->max_pop3 * e->colonist_oper_factories));
        }
        if (game_num_factory_cost_fix) {
            cond_build_ind |= (p->pop_oper_fact < e->colonist_oper_factories);
        }
        if (BOOLVEC_IS1(p->finished, FINISHED_SOILATMOS)
          && !(game_num_slider_eco_done_fix && game_planet_can_terraform(g, p, p->owner, true))) {
            game_turn_finished_disable_slider(g, p, PLANET_SLIDER_ECO, cond_build_ind);
            game_add_planet_to_build_finished(g, pli, pi, FINISHED_SOILATMOS);
        }
        if (1
          && (p->pop >= p->max_pop3)
          && (p->unrest == PLANET_UNREST_NORMAL)
        ) {
            bool flag_pending_ecoproj;
            int v;
            v = (p->prod_after_maint * p->slider[PLANET_SLIDER_ECO]) / 100;
            flag_pending_ecoproj = false;
            if (v > 0) {
                if (0
                  || (e->have_atmos_terra && (p->growth == PLANET_GROWTH_HOSTILE))
                  || (e->have_soil_enrich && (p->growth == PLANET_GROWTH_NORMAL))
                  || (e->have_adv_soil_enrich && ((p->growth == PLANET_GROWTH_NORMAL) || (p->growth == PLANET_GROWTH_FERTILE)))
                  || ((p->max_pop3 - p->max_pop2) < e->have_terraform_n)
                ) {
                    flag_pending_ecoproj = true;
                }
            }
            if (!flag_pending_ecoproj) {
                int w, waste = 0;
                w = game_planet_get_waste_percent(&waste, g, p, false);
                SUBSAT0(v, waste);
                if (0
                  || ((game_get_pop_growth_for_eco(g, p, v) / 10) > 0)
                  || ((p->slider[PLANET_SLIDER_ECO] != 0) && (e->race == RACE_SILICOID))
                ) {
                    if ((p->pop > p->pop_prev) || (p->slider[PLANET_SLIDER_ECO] > (w + game_num_eco_slider_slack))) {
                        game_turn_finished_disable_slider(g, p, PLANET_SLIDER_ECO, cond_build_ind);
                        if (p->pop > p->pop_prev) {
                            game_add_planet_to_build_finished(g, pli, pi, FINISHED_POPMAX);
                        }
                    }
                }
                if (1
                  && (p->slider[PLANET_SLIDER_ECO] > 0)
                  && ((e->race == RACE_SILICOID) || ((e->ind_waste_scale == 0) && (p->waste == 0)))
                ) {
                    game_turn_finished_disable_slider(g, p, PLANET_SLIDER_ECO, cond_build_ind);
                }
            }
        }
        if (BOOLVEC_IS1(p->finished, FINISHED_SHIELD)) {
            game_turn_finished_disable_slider(g, p, PLANET_SLIDER_DEF, false);
            game_add_planet_to_build_finished(g, pli, pi, FINISHED_SHIELD);
        }
        if (BOOLVEC_IS1(p->finished, FINISHED_STARGATE)) {
            /* WASBUG MOO1 has DEF, not SHIP */
            /* p->slider[PLANET_SLIDER_TECH] += p->slider[PLANET_SLIDER_SHIP];
            p->slider[PLANET_SLIDER_SHIP] = 0; */
            game_turn_finished_disable_slider(g, p, PLANET_SLIDER_SHIP, false);
            game_add_planet_to_build_finished(g, pli, pi, FINISHED_STARGATE);
        }
    }
}

static void game_turn_claim(struct game_s *g)
{
    for (uint8_t pli = 0; pli < g->galaxy_stars; ++pli) {
        planet_t *p = &(g->planet[pli]);
        player_id_t pi;
        pi = p->owner;
        p->claim = pi;
        if (pi == PLAYER_NONE) {
            for (pi = PLAYER_0; pi < g->players; ++pi) {
                empiretechorbit_t *e = &(g->eto[pi]);
                for (int i = 0; i < e->shipdesigns_num; ++i) {
                    if (e->orbit[pli].ships[i] != 0) {
                        p->claim = pi;
                        break;
                    }
                }
            }
        }
    }
}

static void game_turn_update_final_war(struct game_s *g)
{
    if (g->end != GAME_END_FINAL_WAR) {
        return;
    }
    game_tech_final_war_share(g);
    for (player_id_t i = 0; i < g->players; ++i) {
        if (IS_HUMAN(g, i)) {
            game_tech_finish_new(g, i);
            ui_newtech(g, i);
        }
    }
    for (player_id_t pi1 = PLAYER_0; pi1 < g->players; ++pi1) {
        empiretechorbit_t *e1 = &(g->eto[pi1]);
        for (player_id_t pi2 = pi1 + 1; pi2 < g->players; ++pi2) {
            empiretechorbit_t *e2 = &(g->eto[pi2]);
            if (BOOLVEC_IS1(g->refuse, pi1) == BOOLVEC_IS1(g->refuse, pi2)) {
                e1->treaty[pi2] = TREATY_ALLIANCE;
                e2->treaty[pi1] = TREATY_ALLIANCE;
                e1->relation1[pi2] = 100;
                e2->relation1[pi1] = 100;
            } else {
                e1->treaty[pi2] = TREATY_FINAL_WAR;
                e2->treaty[pi1] = TREATY_FINAL_WAR;
                e1->relation1[pi2] = -100;
                e2->relation1[pi1] = -100;
            }
        }
    }
}

/* -------------------------------------------------------------------------- */

struct game_end_s game_turn_process(struct game_s *g)
{
    struct game_end_s game_end;
    BOOLVEC_TBL_DECLARE(old_contact, PLAYER_NUM, PLAYER_NUM);
    uint8_t old_focus[PLAYER_NUM];
    int num_alive = 0, num_colony = 0;
    game_end.type = GAME_END_NONE;
    game_turn_limit_ships(g);
    for (int i = 0; i < g->players; ++i) {
        BOOLVEC_TBL_COPY1(old_contact, g->eto[i].contact, i, PLAYER_NUM);
        old_focus[i] = g->planet_focus_i[i];
    }
    if ((g->year > 40) && (!rnd_0_nm1(30, &g->seed)) && (copyprot_status == 0)) {
        copyprot_status = 1;
    }
    ui_turn_pre(g);
    game_turn_countdown_ceasefire(g);
    game_turn_update_mood_blunder(g);
    game_update_have_reserve_fuel(g);
    game_ai->turn_p1(g);
    game_ai->turn_p2(g);
    game_update_have_reserve_fuel(g);
    game_ai->turn_p3(g);
    game_turn_init_z_finished(g);
    game_turn_send_transport(g);
    game_update_production(g);
    game_turn_build_eco(g);
    game_update_total_research(g);
    game_update_have_reserve_fuel(g);
    game_turn_update_trade(g);
    game_spy_build(g);
    if (!game_num_hidden_child_labor_fix) {
        game_update_production(g);
    }
    game_turn_diplo_adjust(g);
    game_turn_build_def(g);
    game_turn_build_ship(g);
    game_turn_reserve(g);
    game_turn_build_ind(g);
    game_turn_move_ships(g);
    game_turn_limit_ships(g);
    game_remove_empty_fleets(g);
    game_spy_report(g);
    game_battle_handle_all(g);
    /*game_turn_unrest_hmm1(g);*/
    for (int i = 0; i < g->players; ++i) {
        g->evn.newtech[i].num = 0;
    }
    {
        struct spy_turn_s st[1];
        game_spy_turn(g, st);
        game_spy_esp_human(g, st);
        game_spy_sab_human(g);
    }
    /*ui_newtech(); FIXME redundant;*/
    for (int i = 0; i < g->players; ++i) {
        g->evn.newtech[i].num = 0;
    }
    /*935f*/
    game_tech_research(g);
    for (int i = 0; i < g->players; ++i) {
        if (IS_HUMAN(g, i)) {
            game_tech_finish_new(g, i);
            ui_newtech(g, i);
        }
    }
    /* FIXME useless? game_battle should have taken care of this */
    if (g->evn.have_guardian) {
        uint8_t o = g->evn.planet_orion_i;
        for (int i = 0; i < g->players; ++i) {
            shipcount_t *s = &(g->eto[i].orbit[o].ships[0]);
            memset(s, 0, sizeof(*s));
        }
    }
    game_turn_explore(g);
    game_turn_bomb(g);
    game_turn_transport(g);
    {
        const uint8_t *grdata;
        grdata = game_turn_ground_resolve_all(g);
        game_turn_ground_show_all(g, grdata);
    }
    game_turn_coup(g);
    if (game_turn_check_end(g, &game_end)) {
        return game_end;
    }
    game_event_new(g);
    if (game_event_run(g, &game_end)) {
        return game_end;
    }
    if (game_turn_check_end(g, &game_end)) {
        return game_end;
    }
    for (int pli = 0; pli < g->galaxy_stars; ++pli) {
        player_id_t looter;
        looter = g->planet[pli].artifact_looter;
        if (looter != PLAYER_NONE) {
            game_tech_get_artifact_loot(g, pli, looter);
        }
    }
    for (int i = 0; i < g->players; ++i) {
        if (IS_HUMAN(g, i)) {
            game_tech_finish_new(g, i);
            ui_newtech(g, i);
        }
        g->evn.newtech[i].num = 0;
    }
    game_turn_update_have_met(g);
    game_ai->turn_diplo_p1(g);
    for (int i = 0; i < g->galaxy_stars; ++i) {
        if (g->planet[i].owner != PLAYER_NONE) {
            ++num_colony;
        }
    }
    for (int i = 0; i < g->players; ++i) {
        if (IS_ALIVE(g, i)) {
            ++num_alive;
        }
    }
    if (1
      && (!g->gaux->flag_cheat_elections)
      && (game_num_council_years != 0)
      && (((g->year % game_num_council_years) == 0) || (!g->election_held))
      && (num_alive > 2)
      && (((g->galaxy_stars * 2) / 3) <= num_colony)
      && (g->end == GAME_END_NONE)
    ) {
        game_election(g);
        g->election_held = true;
    }
    if (game_turn_check_end(g, &game_end)) {
        return game_end;
    }
    if (g->gaux->local_players > 1) {
        game_turn_update_final_war(g); /* MOO1 does this much later */
    }
    game_ai->turn_diplo_p2(g);
    game_turn_audiences(g);
    if (game_turn_check_end(g, &game_end)) {
        return game_end;
    }
    for (player_id_t i = PLAYER_0; i < g->players; ++i) {
        game_turn_contact_broken(g, i, BOOLVEC_TBL_PTRPARAMM(old_contact, i));
    }
    game_fleet_unrefuel(g);
    game_update_production(g);
    if (copyprot_status == 1) {
        ui_copyprotection_check(g);
    }
    for (player_id_t i = PLAYER_0; i < g->players; ++i) {
        game_update_eco_on_waste(g, i, false);
    }
    game_turn_update_seen(g);
    game_diplo_mood_relax(g);
    for (int i = 0; i < g->players; ++i) {
        g->planet_focus_i[i] = old_focus[i]; /* FIXME should not be needed anymore */
    }
    ++g->year;
    if (copyprot_status == 1) {
        ui_copyprotection_lose(g, &game_end);
        return game_end;
    }
#if 0
    game_turn_show_newships(g); /* handled in game_turn_start_messages */
#endif
    /* MOO1 autosaves here every 5 turns */
    game_update_tech_util(g);
    for (player_id_t i = PLAYER_0; i < g->players; ++i) {
        empiretechorbit_t *e = &(g->eto[i]);
        for (tech_field_t f = 0; f < TECH_FIELD_NUM; ++f) {
            e->tech.percent[f] = game_tech_get_field_percent(g, i, f);
        }
    }
    game_turn_finished_slider(g);
    game_turn_claim(g);
    game_update_within_range(g);
    if (game_num_extended_reloc_range) {
        game_update_reloc_dest(g);
    }
    game_update_visibility(g);
    if (g->gaux->local_players == 1) {
        game_turn_update_final_war(g);  /* MOO1 does this here, after update_tech_util etc */
    }
    game_remove_empty_fleets(g);
    game_planet_update_home(g);
    return game_end;
}
