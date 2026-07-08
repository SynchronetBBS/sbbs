#include "config.h"

#include <stdio.h>
#include <string.h>

#include "game_planet.h"
#include "comp.h"
#include "game.h"
#include "game_aux.h"
#include "game_misc.h"
#include "game_num.h"
#include "game_str.h"
#include "game_tech.h"
#include "lib.h"
#include "rnd.h"
#include "types.h"

/* -------------------------------------------------------------------------- */

static void tenths_2str(struct strbuild_s *str, int num)
{
    strbuild_catf(str, "%i", num / 10);
    if ((num < 100) && ((num % 10) != 0)) {
        strbuild_catf(str, ".%i", num % 10);
    }
}

/* -------------------------------------------------------------------------- */

void game_planet_destroy(struct game_s *g, uint8_t planet_i, player_id_t attacker)
{
    planet_t *p = &(g->planet[planet_i]);
    player_id_t owner = p->owner;
    if (IS_PLAYER(g, owner)) {
        /* g->seen is not used by classical AI,
           so the original skips this for AI. */
        g->seen[owner][planet_i].owner = PLAYER_NONE;
        g->seen[owner][planet_i].pop = 0;
        g->seen[owner][planet_i].bases = 0;
        g->seen[owner][planet_i].factories = 0;
    }
    if (IS_HUMAN(g, owner)) {
        /* WASBUG there was an unreliable mess here */
        g->gaux->human_killer = attacker;
    }
    p->rebels = 0;
    p->unrest = 0;
    p->reserve = 0;
    p->prev_owner = owner;
    p->owner = PLAYER_NONE;
    p->bc_to_ecoproj = 0;
    p->pop = 0;
    p->pop_prev = 0;
    p->prod_after_maint = 0;
    p->bc_to_ship = 0;
    for (int i = 0; i < PLAYER_NUM; ++i) {
        p->inbound[i] = 0;
    }
    p->buildship = 0;
    for (int i = 0; i < PLANET_SLIDER_NUM; ++i) {
        p->slider[i] = 0;
        p->slider_lock[i] = 0;
    }
    p->slider[PLANET_SLIDER_IND] = 100;
    p->reloc = planet_i;
    p->missile_bases = 0;
    p->bc_upgrade_base = 0;
    p->bc_to_base = 0;
    p->trans_num = 0;
    p->bc_to_ship = 0;
    p->bc_to_factory = 0;
    p->have_stargate = false;
    p->shield = 0;
    p->bc_to_shield = 0;
    BOOLVEC_CLEAR(p->finished, FINISHED_NUM);
    if (game_num_extended_reloc_range) {
        return;
    }
    for (int i = 0; i < g->galaxy_stars; ++i) {
        p = &(g->planet[i]);
        if (p->reloc == planet_i) {
            p->reloc = i;
        }
    }
}

uint8_t game_planet_get_random(struct game_s *g, player_id_t owner)
{
    uint8_t tbl[PLANETS_MAX];
    int num = 0;
    for (int i = 0; i < g->galaxy_stars; ++i) {
        if (g->planet[i].owner == owner) {
            tbl[num++] = i;
        }
    }
    if (num == 0) {
        return PLANET_NONE;
    } else {
        return tbl[rnd_0_nm1(num, &g->seed)];
    }
}

void game_planet_adjust_percent2(struct game_s *g, uint8_t planet_i, planet_slider_i_t si, uint8_t percent, bool respect_locks)
{
    planet_t *p = &(g->planet[planet_i]);
    if (respect_locks && p->slider_lock[si]) return;
    for (planet_slider_i_t j = 0; j < PLANET_SLIDER_NUM; ++j) {
        if (respect_locks && p->slider_lock[j]) continue;
        if (si == j) continue;
        int diff = 0;
        if (j == PLANET_SLIDER_ECO) {
            int w = game_planet_get_waste_percent(NULL, g, p, true);
            if (w < p->slider[j]) {
                diff = (p->slider[j] - w) * percent / 100;
            }
        } else {
            diff = (p->slider[j] * percent) / 100;
        }
        p->slider[si] += diff;
        p->slider[j] -= diff;
    }
}

void game_planet_adjust_percent(struct game_s *g, player_id_t owner, planet_slider_i_t si, uint8_t percent, int growth)
{
    for (int i = 0; i < g->galaxy_stars; ++i) {
        planet_t *p = &(g->planet[i]);
        if (p->owner == owner) {
            if (0
              || (growth == 0)
              || ((growth == 1) && (p->growth > PLANET_GROWTH_HOSTILE))
              || ((growth == 2) && (p->growth == PLANET_GROWTH_HOSTILE))
            ) {
                int sum, slider_increase;

                /* Summing up all sliders except for the one that is going to be increased
                 * and ECO, because ECO is set to the minimum for waste cleanup before this
                 * function is called. SHIP and TECH are never increased in this function. */
                sum = p->slider[PLANET_SLIDER_SHIP] + p->slider[PLANET_SLIDER_TECH];
                if (si != PLANET_SLIDER_DEF) {
                    sum += p->slider[PLANET_SLIDER_DEF];
                }
                if (si != PLANET_SLIDER_IND) {
                    sum += p->slider[PLANET_SLIDER_IND];
                }
                if (game_num_newtech_adjust_fix && (si != PLANET_SLIDER_ECO)) {
                    int eco = p->slider[PLANET_SLIDER_ECO] - game_planet_get_waste_percent(NULL, g, p, false);
                    SETMAX(eco, 0);
                    sum += eco;
                    p->slider[PLANET_SLIDER_ECO] -= (eco * percent) / 100;
                }

                slider_increase = (sum * percent) / 100;
                p->slider[PLANET_SLIDER_SHIP] = (p->slider[PLANET_SLIDER_SHIP] * (100 - percent)) / 100;
                if (si == PLANET_SLIDER_DEF) {
                    p->slider[PLANET_SLIDER_DEF] += slider_increase;
                } else {
                    p->slider[PLANET_SLIDER_DEF] = (p->slider[PLANET_SLIDER_DEF] * (100 - percent)) / 100;
                }
                if (si == PLANET_SLIDER_IND) {
                    p->slider[PLANET_SLIDER_IND] += slider_increase;
                } else {
                    p->slider[PLANET_SLIDER_IND] = (p->slider[PLANET_SLIDER_IND] * (100 - percent)) / 100;
                }
                if (si == PLANET_SLIDER_ECO) {
                    p->slider[PLANET_SLIDER_ECO] += slider_increase;
                }
                p->slider[PLANET_SLIDER_TECH] = 100 - p->slider[PLANET_SLIDER_SHIP] - p->slider[PLANET_SLIDER_ECO] - p->slider[PLANET_SLIDER_DEF] - p->slider[PLANET_SLIDER_IND];
                SETMAX(p->slider[PLANET_SLIDER_TECH], 0);
            }
        }
    }
}

int game_planet_get_waste_percent(int *r_waste, const struct game_s *g, const planet_t *p, bool subtract_transports)
{
    const empiretechorbit_t *e;
    int w, fact, waste, prod, pop;
    if (p->owner == PLAYER_NONE) {
        return 0;
    }
    e = &(g->eto[p->owner]);
    fact = p->factories;
    pop = p->pop;
    if (subtract_transports) {
        pop -= p->trans_num;
    }
    SETMIN(fact, pop * game_planet_get_pop_oper_fact(g, p));
    waste = (e->race == RACE_SILICOID) ? 0 : (((fact * e->ind_waste_scale) / 10 + p->waste) / e->have_eco_restoration_n);
    prod = p->prod_after_maint;
    if (prod == 0) {
        prod = 1000;
    }
    w = ((waste * 100) + prod - 1) / prod;
    SETRANGE(w, 0, 100);
    if (r_waste != NULL) {
        *r_waste = waste;
    }
    return w;
}

bool game_planet_can_terraform(const struct game_s *g, const planet_t *p, player_id_t active_player, bool soilatmos)
{
    const empiretechorbit_t *e = &(g->eto[active_player]);
    if (soilatmos) {
        if (e->have_atmos_terra && (p->growth == PLANET_GROWTH_HOSTILE)) {
            return true;
        }
        if (e->have_soil_enrich && (p->growth == PLANET_GROWTH_NORMAL)) {
            return true;
        }
        if (e->have_adv_soil_enrich && ((p->growth == PLANET_GROWTH_NORMAL) || (p->growth == PLANET_GROWTH_FERTILE))) {
            return true;
        }
    }
    return ((p->max_pop2 + e->have_terraform_n) > p->max_pop3) && (p->max_pop3 < game_num_max_pop);
}

void game_planet_update_home(struct game_s *g)
{
    for (int i = 0; i < g->galaxy_stars; ++i) {
        const planet_t *p = &(g->planet[i]);
        player_id_t pi;
        pi = p->owner;
        if ((pi != PLAYER_NONE) && (g->evn.home[pi] == PLANET_NONE)) {
            g->evn.home[pi] = i;   /* WASBUG? MOO1 sets to 0 which affects rebellion event check */
        }
    }
}

const char *game_planet_get_finished_text(const struct game_s *g, const planet_t *p, planet_finished_t type, char *buf, size_t bufsize)
{
    int num;
    switch (type) {
        case FINISHED_FACT:
            num = p->max_pop3 * g->eto[p->owner].colonist_oper_factories;
            lib_sprintf(buf, bufsize, "%s %s %s %i %s. %s", p->name, game_str_sm_hasreached, game_str_sm_indmaxof, num, game_str_sm_factories, game_str_sm_extrares);
            break;
        case FINISHED_POPMAX:
            num = p->max_pop3;
            lib_sprintf(buf, bufsize, "%s %s %s %i %s. %s", p->name, game_str_sm_hasreached, game_str_sm_popmaxof, num, game_str_sm_colonists, game_str_sm_extrares);
            break;
        case FINISHED_SOILATMOS:
            lib_sprintf(buf, bufsize, "%s %s %s %s %s%s.", p->name, game_str_sm_hasterraf, game_str_tbl_sm_terraf[p->growth - 1], game_str_sm_envwith, game_str_tbl_sm_envmore[p->growth - 1], game_str_sm_stdgrow);
            break;
        case FINISHED_STARGATE:
            lib_sprintf(buf, bufsize, "%s %s.", p->name, game_str_sm_hasfsgate);
            break;
        case FINISHED_SHIELD:
            lib_sprintf(buf, bufsize, "%s %s %s %s.", p->name, game_str_sm_hasfshield, game_str_tbl_roman[p->shield], game_str_sm_planshield);
            break;
        default:
            lib_sprintf(buf, bufsize, "BUG: invalid finished tyoe %i at '%s'", type, p->name);
            break;
    }
    return buf;
}

int game_planet_get_slider_text(const struct game_s *g, const planet_t *p, planet_slider_i_t si, char *buf, size_t bufsize)
{
    player_id_t player = p->owner;
    const empiretechorbit_t *e = &(g->eto[player]);
    int retval = -1;
    switch (si) {
        case PLANET_SLIDER_SHIP:
            {
                int vthis, vtotal, cost;
                vthis = game_adjust_prod_by_special((p->prod_after_maint * p->slider[PLANET_SLIDER_SHIP]) / 100, p->special);
                vtotal = vthis + p->bc_to_ship;
                if (p->buildship == BUILDSHIP_STARGATE) {
                    cost = game_num_stargate_cost;
                } else {
                    cost = g->srd[player].design[p->buildship].cost;
                }
                if ((vtotal < cost) || (p->buildship == BUILDSHIP_STARGATE)) {
                    if (vthis < 1) {
                        lib_strcpy(buf, game_str_sm_prodnone, bufsize);
                        retval = 0;
                    } else {
                        int num = 0, over;
                        over = cost - p->bc_to_ship;
                        while (over > 0) {
                            over -= vthis;
                            ++num;
                        }
                        SETMAX(num, 1);
                        lib_sprintf(buf, bufsize, "%i %s", num, game_str_sm_prod_y);
                        if (p->buildship != BUILDSHIP_STARGATE) {
                            retval = 1;
                        }
                    }
                } else {
                    int num;
                    /* TODO this adjusted sd->cost directly! */
                    SETMAX(cost, 1);
                    num = vtotal / cost;
                    lib_sprintf(buf, bufsize, "1 %s", game_str_sm_prod_y);
                    retval = num;
                }
            }
            break;
        case PLANET_SLIDER_DEF:
            {
                int vthis, vtotal, cost, v8, va;
                cost = game_get_base_cost(g, player);
                vthis = game_adjust_prod_by_special((p->prod_after_maint * p->slider[PLANET_SLIDER_DEF]) / 100, p->special);
                vtotal = vthis + p->bc_to_base;
                v8 = p->bc_upgrade_base;
                if (vthis == 0) {
                    lib_strcpy(buf, game_str_sm_prodnone, bufsize);
                } else if (vtotal <= v8) {
                    lib_strcpy(buf, game_str_sm_defupg, bufsize);
                } else {
                    vtotal -= v8;
                    SETMAX(vtotal, 0);
                    va = e->planet_shield_cost - p->bc_to_shield;
                    if (p->battlebg == 0) {
                        va = 0;
                    }
                    SETMAX(va, 0);
                    if (vtotal <= va) {
                        lib_strcpy(buf, game_str_sm_defshld, bufsize);
                    } else {
                        int num, over;
                        vtotal -= va;
                        SETMAX(vtotal, 0);
                        if ((cost * 2) > vtotal) {
                            num = 1;
                            over = cost - vtotal;
                            while (over > 0) {
                                over -= vthis;
                                ++num;
                            }
                            lib_sprintf(buf, bufsize, "%i %s", num, game_str_sm_prod_y);
                        } else {
                            num = vtotal / cost;
                            lib_sprintf(buf, bufsize, "%i/%s", num, game_str_sm_prod_y);
                        }
                    }
                }
            }
            break;
        case PLANET_SLIDER_IND:
            {
                const char *str = NULL;
                int vthis, cost;
                cost = game_planet_get_fact_adj_cost(g, p);
                vthis = game_adjust_prod_by_special((p->prod_after_maint * p->slider[PLANET_SLIDER_IND]) / 100, p->special);
                if (vthis != 0) {
                    int v20;
                    v20 = (vthis * 10) / cost;
                    if ((v20 / 10 + p->factories) >= ((p->pop - p->trans_num) * p->pop_oper_fact)) {
                        if (p->pop_oper_fact < e->colonist_oper_factories) {
                            if (e->race != RACE_MEKLAR) {
                                str = game_str_sm_refit;
                                if (game_num_factory_cost_fix) {
                                    /* FIXME: Doesn't always match the result */
                                    str = game_str_sm_irc[p->pop_oper_fact - 1];
                                }
                            } else {
                                struct strbuild_s strbuild = strbuild_init(buf, bufsize);
                                tenths_2str(&strbuild, v20);
                                strbuild_catf(&strbuild, "/%s", game_str_sm_prod_y);
                            }
                        } else {
                            str = game_str_sm_indres;
                            if (p->factories < (p->max_pop3 * e->colonist_oper_factories)) {
                                str = game_str_sm_indmax;
                            }
                        }
                    } else {
                        struct strbuild_s strbuild = strbuild_init(buf, bufsize);
                        tenths_2str(&strbuild, v20);
                        strbuild_catf(&strbuild, "/%s", game_str_sm_prod_y);
                    }
                } else {
                    str = game_str_sm_prodnone;
                }
                if (str) {
                    lib_strcpy(buf, str, bufsize);
                }
            }
            break;
        case PLANET_SLIDER_ECO:
            retval = game_planet_get_slider_text_eco(g, p, false, buf, bufsize);
            break;
        case PLANET_SLIDER_TECH:
            {
                int v = game_get_tech_prod(p->prod_after_maint, p->slider[PLANET_SLIDER_TECH], e->race, p->special);
                lib_sprintf(buf, bufsize, "%i", v);
                retval = v;
            }
            break;
        default:
            *buf = '\0';
            break;
    }
    return retval;
}

int game_planet_get_slider_text_eco(const struct game_s *g, const planet_t *p, bool flag_tenths, char *buf, size_t bufsize)
{
    player_id_t player = p->owner;
    const empiretechorbit_t *e = &(g->eto[player]);
    int retval = -1;
    const char *str = NULL;
    int vthis, adjwaste = 0, tform_cost;
    bool flag_ecoproj = false;
    vthis = (p->prod_after_maint * p->slider[PLANET_SLIDER_ECO]) / 100;
    game_planet_get_waste_percent(&adjwaste, g, p, true);
    if ((vthis < adjwaste) || (vthis == 0)) {
        str = (vthis < adjwaste) ? game_str_sm_ecowaste : game_str_sm_prodnone;
    } else {
        SUBSAT0(vthis, adjwaste);
        if ((vthis > 0) && e->have_atmos_terra && (p->growth == PLANET_GROWTH_HOSTILE)) {
            vthis -= game_num_atmos_cost - p->bc_to_ecoproj;
            if (vthis < 0) {
                flag_ecoproj = true;
                str = game_str_sm_ecoatmos;
            }
        }
        if ((vthis > 0) && e->have_soil_enrich && (p->growth == PLANET_GROWTH_NORMAL)) {
            vthis -= game_num_soil_cost - p->bc_to_ecoproj;
            if (vthis < 0) {
                flag_ecoproj = true;
                str = game_str_sm_ecosoil;
            }
        }
        if ((vthis > 0) && e->have_adv_soil_enrich && ((p->growth == PLANET_GROWTH_NORMAL) || (p->growth == PLANET_GROWTH_FERTILE))) {
           vthis -= game_num_adv_soil_cost - p->bc_to_ecoproj;
           if (vthis < 0) {
                flag_ecoproj = true;
                str = game_str_sm_ecogaia;
            }
        }
        if (vthis > 0) {
            if (p->max_pop3 < game_num_max_pop) {
                /* WASBUG In MOO 1.3, if you conquered a planet from a race
                 * with more advanced terraforming than you, a negative
                 * terraforming cost would be calculated here. This affected
                 * the displayed pop growth estimate but not actual growth. */
                tform_cost = (p->max_pop2 + (e->have_terraform_n - p->max_pop3)) * e->terraform_cost_per_inc;
                SETMAX(tform_cost, 0);
            } else {
                tform_cost = 0;
            }
            if (vthis < tform_cost) {
                str = game_str_sm_ecotform;
            } else {
                int growth, growth2, max_pop;
                bool max = false;
                bool flag_tform = false;
                max_pop = p->max_pop3;
                if (tform_cost > 0) {
                    if (vthis < tform_cost) {
                        flag_tform = true;
                    } else if (flag_tenths) {   /* keep same +N as MOO1 for developing planets on -nouiextra */
                        max_pop = p->max_pop2 + e->have_terraform_n;
                    }
                }
                vthis -= tform_cost;
                growth = game_get_pop_growth_max(g, p, max_pop) + p->pop_tenths;
                if (!flag_tenths) {
                    if (((p->pop - p->trans_num) + (growth / 10)) > max_pop) {
                        growth = (max_pop - (p->pop - p->trans_num)) * 10;
                    }
                    growth2 = game_get_pop_growth_for_eco(g, p, vthis) + growth;
                    if (((p->pop - p->trans_num) + (growth2 / 10)) > max_pop) {
                        growth2 = (max_pop - (p->pop - p->trans_num)) * 10;
                    }
                    growth = growth2 / 10 - growth / 10;
                } else {
                    if (((p->pop - p->trans_num) * 10 + growth) > (max_pop * 10)) {
                        growth = (max_pop - (p->pop - p->trans_num)) * 10;
                        if (!game_num_pop_tenths_fix) {
                            growth += p->pop_tenths;
                        }
                    }
                    growth2 = game_get_pop_growth_for_eco(g, p, vthis) + growth;
                    if (((p->pop - p->trans_num) * 10 + growth2) >= (max_pop * 10)) {
                        growth2 = (max_pop - (p->pop - p->trans_num)) * 10;
                        if (!game_num_pop_tenths_fix) {
                            growth2 += p->pop_tenths;
                        }
                        max = (growth != growth2);
                    }
                    growth = growth2 - growth;
                }
                if (growth <= 0) {
                    str = flag_tform ? game_str_sm_ecotform : game_str_sm_ecoclean;
                } else if (max) {
                    str = game_str_sm_max;
                } else {
                    retval = growth;
                    str = game_str_sm_ecopop;
                }
            }
        } else {
            if ((flag_ecoproj == false) && (e->race != RACE_SILICOID)) {
                str = game_str_sm_ecoclean;
            }
        }
    }
    if (str) {
        lib_strcpy(buf, str, bufsize);
    }
    return retval;
}
