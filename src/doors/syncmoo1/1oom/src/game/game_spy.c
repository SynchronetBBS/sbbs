#include "config.h"

#include "game_spy.h"
#include "comp.h"
#include "game.h"
#include "game_aux.h"
#include "game_diplo.h"
#include "game_misc.h"
#include "game_tech.h"
#include "game_techtypes.h"
#include "log.h"
#include "rnd.h"
#include "types.h"
#include "ui.h"

/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */

static uint8_t game_spy_esp_get_group_tier(struct game_s *g, tech_group_t group, tech_field_t field, uint16_t len, const uint8_t *research_completed)
{
    int v = 0;
    for (int i = 0; i < len; ++i) {
        uint8_t techi;
        tech_group_t group2;
        techi = research_completed[i];
        group2 = game_tech_get_group(g->gaux, field, techi);
        if (group == group2) {
            v = game_tech_get_tier(g->gaux, field, techi);
        }
    }
    return v;
}

static void game_spy_esp_sift_useful_techs_do(struct game_s *g, struct spy_esp_s *s, tech_field_t field, int target_len, const uint8_t *target_research_completed, int spy_len, const uint8_t *spy_research_completed)
{
    for (int i = 0; i < target_len; ++i) {
        uint8_t techi, tier;
        tech_group_t group;
        bool have_tech;
        techi = target_research_completed[i];
        group = game_tech_get_group(g->gaux, field, techi);
        tier = game_tech_get_tier(g->gaux, field, techi);
        have_tech = false;
        for (int j = 0; j < spy_len; ++j) {
            if (spy_research_completed[j] == techi) {
                have_tech = true;
                break;
            }
        }
        if (0
          || (group == TECH_GROUP_IMPROVED_ROBOTIC_CONTROLS)
          || (group == TECH_GROUP_SPACE_SCANNER)
          || (group == TECH_GROUP_REDUCED_INDUSTRIAL_WASTE)
          || (group == TECH_GROUP_IMPROVED_INDUSTRIAL_TECH)
          || (group == TECH_GROUP_PLANETARY_SHIELD)
          || (group == TECH_GROUP_IMPROVED_TERRAFORMING)
          || (group == TECH_GROUP_CONTROLLED_ENVIRONMENT)
          || (group == TECH_GROUP_ECO_RESTORATION)
          || (group == TECH_GROUP_PERSONAL_ARMOR)
          || (group == TECH_GROUP_PERSONAL_SHIELD)
          || (group == TECH_GROUP_FUEL_CELLS)
          || (group == TECH_GROUP_PERSONAL_WEAPONS)
        ) {
            if (game_spy_esp_get_group_tier(g, group, field, spy_len, spy_research_completed) >= tier) {
                have_tech = true;
            }
        }
        /*632ff*/
        if (g->eto[s->spy].race == RACE_SILICOID) {
            if (0
              || (group == TECH_GROUP_REDUCED_INDUSTRIAL_WASTE)
              || (group == TECH_GROUP_CONTROLLED_ENVIRONMENT)
              || (group == TECH_GROUP_ECO_RESTORATION)
            ) {
                have_tech = true;
            }
        }
        if (!have_tech) {
            s->tbl_techi[field][s->tbl_num[field]++] = techi;
        }
    }
}

static int game_spy_esp_get_value(struct game_s *g, tech_field_t field, uint8_t techi, player_id_t player_i)
{
    const empiretechorbit_t *es = &(g->eto[player_i]);
    const shipresearch_t *srds = &(g->srd[player_i]);
    uint8_t maxtier = 0, maxti = 0;
    tech_group_t group = game_tech_get_group(g->gaux, field, techi);
    int v;
    if (group & 0x80) {
        return 0;
    }
    for (int i = 0; i < es->tech.completed[field]; ++i) {
        uint8_t tier2, ti;
        tech_group_t group2;
        ti = srds->researchcompleted[field][i];
        group2 = game_tech_get_group(g->gaux, field, ti);
        tier2 = game_tech_get_tier(g->gaux, field, ti);
        if ((group2 == group) && (tier2 > maxtier)) {
             maxtier = tier2;
             maxti = ti;
        }
    }
#if 0
    /* BUG v is uninitialized and overwritten anyway */
    /*62e63*/
    switch (field) {
        case TECH_FIELD_COMPUTER:
            if (es->trait2 == TRAIT2_INDUSTRIALIST) {
                v += v / 6;
            }
            break;
        case TECH_FIELD_CONSTRUCTION:
            if (es->trait2 == TRAIT2_INDUSTRIALIST) {
                v += v / 4;
            }
            break;
        case TECH_FIELD_FORCE_FIELD:
            if (es->trait2 == TRAIT2_MILITARIST) {
                v += v / 4;
            }
            break;
        case TECH_FIELD_PLANETOLOGY:
            if (es->trait2 == TRAIT2_ECOLOGIST) {
                v += v / 4;
            }
            break;
        case TECH_FIELD_PROPULSION:
            if (es->trait2 == TRAIT2_EXPANSIONIST) {
                v += v / 4;
            }
            break;
        case TECH_FIELD_WEAPON:
            if (es->trait2 == TRAIT2_MILITARIST) {
                v += v / 4;
            }
            break;
        default:
            break;
    }
#endif
    /*62f24*/
    v = techi * techi;
    if (group == TECH_GROUP_SINGULAR) {
        v *= 10;
    }
    if (group == TECH_GROUP_IMPROVED_ROBOTIC_CONTROLS) {
        v *= 6;
    }
    if (group == TECH_GROUP_REDUCED_INDUSTRIAL_WASTE) {
        v *= 3;
    }
    if (group == TECH_GROUP_ECO_RESTORATION) {
        v *= 3;
    }
    if (maxti > techi) {
        v /= 4;
    }
    if (maxti == techi) {
        v = 0;
    }
    if (es->race == RACE_SILICOID) {
        if (0
          || (group == TECH_GROUP_REDUCED_INDUSTRIAL_WASTE)
          || (group == TECH_GROUP_CONTROLLED_ENVIRONMENT)
          || (group == TECH_GROUP_ECO_RESTORATION)
        ) {
            v = 0;
        }
    }
    if ((field == TECH_FIELD_WEAPON) && (techi == TECH_WEAP_DEATH_RAY)) {
        v = 30000;
    }
    return v;
}

static void game_spy_esp_sub5(struct spy_esp_s *s, int r)
{
    for (int i = 0; i < s->tnum; ++i) {
        if (s->tbl_tech2[i] > r) {
            --s->tnum;
            for (int j = 0; j < s->tnum; ++j) {
                s->tbl_field[j] = s->tbl_field[j + 1];
                s->tbl_tech2[j] = s->tbl_tech2[j + 1];
            }
            --i;
        }
    }
}

static player_id_t game_spy_frame_random(struct game_s *g, player_id_t spy, player_id_t target)
{
    const empiretechorbit_t *et = &(g->eto[target]);
    player_id_t tbl_scapegoat[PLAYER_NUM];
    int n = 0;
    for (player_id_t i = 0; i < PLAYER_NUM; ++i) {
        if ((i != spy) && (i != target) && BOOLVEC_IS1(et->contact, i)) {
            tbl_scapegoat[n++] = i;
        }
    }
    return (n == 0) ? PLAYER_NONE : tbl_scapegoat[rnd_0_nm1(n, &g->seed)];
}

static void game_spy_espionage(struct game_s *g, player_id_t spy, player_id_t target, bool flag_frame, int spies, bool flag_any_caught, struct spy_turn_s *st)
{
    struct spy_esp_s s[1];
    empiretechorbit_t *et = &(g->eto[target]);
    int spied = spies, rmax = 0, tmax = 0;
    for (int i = 0; i < spies; ++i) {
        int r;
        r = rnd_1_n(100, &g->seed);
        SETMAX(rmax, r);
    }
    for (int i = 0; i < TECH_FIELD_NUM; ++i) {
        SETMAX(tmax, et->tech.percent[i]);
    }
    rmax = (rmax * tmax) / 100;
    if (spies > 0) {
        s->target = target;
        s->spy = spy;
        game_spy_esp_sub1(g, s, 0, 0);
        /*81f0a*/
        game_spy_esp_sub5(s, rmax);
        SETMIN(spied, s->tnum);
        if (s->tnum > 0) {
            g->evn.stolen_spy[target][spy] = flag_any_caught ? spy : PLAYER_NONE;
            if (IS_HUMAN(g, spy)) {
                /*81fd3*/
                int v = 0;
                st->tbl_rmax[target][spy] = rmax;
                g->evn.spied_num[target][spy] = spied;
                g->evn.spied_spy[target][spy] = flag_frame ? -1 : spy;
                for (int i = 0; i < 5; ++i) {
                    v += rnd_1_n(12, &g->seed);
                }
                if ((!flag_frame) && flag_any_caught) {
                    g->evn.spied_spy[target][spy] = v;
                }
            } else if (IS_HUMAN(g, target)) {
                /*81f35*/
                g->evn.stolen_field[target][spy] = s->tbl_field[0];
                g->evn.stolen_tech[target][spy] = s->tbl_tech2[0];
                if (flag_frame) {
                    g->evn.stolen_spy[target][spy] = game_spy_frame_random(g, spy, target);
                }
                /*81fa7*/
                game_tech_get_new(g, spy, s->tbl_field[0], s->tbl_tech2[0], TECHSOURCE_AI_SPY, 0, PLAYER_NONE, false);
            } else {
                /*8207f*/
                game_tech_get_new(g, spy, s->tbl_field[0], s->tbl_tech2[0], TECHSOURCE_AI_SPY, 0, PLAYER_NONE, false);
                if (flag_frame && (rnd_0_nm1(2, &g->seed) == 0)) {
                    player_id_t scapegoat[PLAYER_NUM];
                    player_id_t pi;
                    int n = 0;
                    for (pi = PLAYER_0; pi < g->players; ++pi) {
                        if ((pi != target) && IS_HUMAN(g, pi) && IS_ALIVE(g, pi) && BOOLVEC_IS1(et->contact, pi)) {
                            scapegoat[n++] = pi;
                        }
                    }
                    if (n > 0) {
                        pi = scapegoat[(n > 1) ? rnd_0_nm1(n, &g->seed) : 0];
                        game_diplo_act(g, -(rnd_1_n(20, &g->seed) + 20), pi, target, 5, 0, s->tbl_field[0]);
                    }
                }
            }
            /*820e6*/
        }
    }
}

static void game_spy_sabotage(struct game_s *g, player_id_t spy, player_id_t target, bool flag_frame, int spies, bool flag_any_caught)
{
    int rcaught, v8 = 0, pl;
    bool flag_bases;
    rcaught = flag_any_caught ? (rnd_1_n(20, &g->seed) + 20) : 0;
    {
        int num;
        num = ((g->eto[spy].tech.percent[TECH_FIELD_WEAPON] + 9) * spies) / 10;
        for (int i = 0; i < num; ++i) {
            v8 += rnd_1_n(5, &g->seed);
        }
    }
    flag_bases = (rnd_0_nm1(2, &g->seed) != 0);
    pl = game_planet_get_random(g, target); /* WASBUG? used a function that returned 0 on no planets */
    if ((v8 > 0) && (pl != PLANET_NONE)) {
        planet_t *p = &(g->planet[pl]);
        g->evn.sabotage_spy[target][spy] = rcaught ? spy : PLAYER_NONE;
        if (IS_HUMAN(g, spy)) {
            /*82431*/
            g->evn.sabotage_num[target][spy] = v8;
            g->evn.sabotage_spy[target][spy] = flag_frame ? -1 : rcaught;
        } else if (IS_HUMAN(g, target)) {
            if (rnd_0_nm1(4, &g->seed) == 0) {
                v8 = (p->pop * (v8 / 2)) / 100;
                SETMAX(v8, 1);
                SETMIN(v8, p->pop);
                p->rebels += v8;
                SETMIN(p->rebels, p->pop);
                if (p->rebels >= (p->pop / 2)) {
                    p->unrest = PLANET_UNREST_REBELLION;
                    p->unrest_reported = false;
                    p->rebels = p->pop;
                }
            } else {
                /*822f6*/
                if (!flag_bases) {
                    SETMIN(v8, p->factories);
                } else {
                    /*82328*/
                    v8 /= 5;
                    SETMIN(v8, p->missile_bases);
                }
                /*8235e*/
                if (v8 > 0) {
                    g->evn.sabotage_is_bases[target][spy] = flag_bases;
                    g->evn.sabotage_planet[target][spy] = pl;
                    g->evn.sabotage_num[target][spy] = v8;
                    if (flag_frame) {
                        g->evn.sabotage_spy[target][spy] = game_spy_frame_random(g, spy, target);
                    }
                    if (flag_bases) {
                        p->missile_bases -= v8;
                    } else {
                        p->factories -= v8;
                    }
                }
            }
        } else {
            /*8247a*/
            if (!flag_bases) {
                SUBSAT0(p->factories, v8);
            } else {
                /*82328*/
                v8 /= 5;
                SUBSAT0(p->missile_bases, v8);
            }
            if (!flag_frame) {
                game_diplo_act(g, -rcaught, spy, target, 6, pl, flag_bases);
            } else {
                player_id_t p2 = game_spy_frame_random(g, spy, target);
                if (p2 != PLAYER_NONE) {
                    int r = -(rnd_1_n(16, &g->seed) + rnd_1_n(16, &g->seed));
                    game_diplo_act(g, -r, p2, target, 7, pl, flag_bases);
                }
            }
        }
    }
}

/* -------------------------------------------------------------------------- */

int game_spy_esp_sub1(struct game_s *g, struct spy_esp_s *s, int a4, int a6)
{
    s->tnum = 0;
    game_spy_esp_sub2(g, s, a6);
    for (int loops = 0; (loops < 500) && (s->tnum < TECH_SPY_MAX); ++loops) {
        tech_field_t field;
        field = rnd_0_nm1(TECH_FIELD_NUM, &g->seed);
        if (s->tbl_num[field] > 0) {
            int value;
            bool have_tech;
            uint8_t techi;
            techi = s->tbl_techi[field][rnd_0_nm1(s->tbl_num[field], &g->seed)];
            have_tech = false;
            for (int i = 0; i < s->tnum; ++i) {
                /*63495*/
                if ((s->tbl_field[i] == field) && (s->tbl_tech2[i] == techi)) {
                    have_tech = true;
                }
            }
            value = game_spy_esp_get_value(g, field, techi, s->spy);
            if ((value == 0) || (value < a4)) {
                have_tech = true;
            }
            if (!have_tech) {
                int i;
                i = s->tnum;
                s->tbl_field[i] = field;
                s->tbl_tech2[i] = techi;
                s->tbl_value[i] = value;
                s->tnum = i + 1;
            }
        }
    }
    return s->tnum;
}

int game_spy_esp_sub2(struct game_s *g, struct spy_esp_s *s, int a4)
{
    const empiretechorbit_t *es = &(g->eto[s->spy]);
    const empiretechorbit_t *et = &(g->eto[s->target]);
    const shipresearch_t *srds = &(g->srd[s->spy]);
    const shipresearch_t *srdt = &(g->srd[s->target]);
    int sum = 0;
    for (tech_field_t f = 0; f < TECH_FIELD_NUM; ++f) {
        s->tbl_num[f] = 0;
    }
    for (tech_field_t f = 0; f < TECH_FIELD_NUM; ++f) {
        game_spy_esp_sift_useful_techs_do(g, s, f, et->tech.completed[f] - a4, srdt->researchcompleted[f], es->tech.completed[f], srds->researchcompleted[f]);
    }
    for (tech_field_t f = 0; f < TECH_FIELD_NUM; ++f) {
        sum += s->tbl_num[f];
    }
    return sum;
}

void game_spy_build(struct game_s *g)
{
    for (player_id_t i = PLAYER_0; i < g->players; ++i) {
        empiretechorbit_t *e = &(g->eto[i]);
        int spycost_base;
        spycost_base = e->tech.percent[TECH_FIELD_COMPUTER] * 2 + 25;
        if (e->race == RACE_DARLOK) {
            spycost_base /= 2;
        }
        for (player_id_t j = PLAYER_0; j < g->players; ++j) {
            if ((i != j) && (e->spying[j] != 0)) {
                int spyfund, spycost;
                spyfund = (e->total_production_bc * e->spying[j]) / 1000 + e->spyfund[j];
                spycost = spycost_base; /* WASBUG MOO1 does not reset spycost between target players */
                while (spyfund >= spycost) {
                    ++e->spies[j];
                    spyfund -= spycost;
                    spycost *= 2;
                }
                e->spyfund[j] = spyfund;
            }
        }
    }
}

void game_spy_report(struct game_s *g)
{
    for (player_id_t i = PLAYER_0; i < g->players; ++i) {
        empiretechorbit_t *e = &(g->eto[i]);
        for (player_id_t j = PLAYER_0; j < g->players; ++j) {
            if ((i != j) && (e->spies[j] > 0)) {
                uint16_t *ntbl = &(g->eto[j].tech.completed[0]);
                shipresearch_t *srd = &(g->srd[j]);
                e->spyreportyear[j] = g->year;
                for (tech_field_t f = 0; f < TECH_FIELD_NUM; ++f) {
                    e->spyreportfield[j][f] = srd->researchcompleted[f][ntbl[f] - 1];
                }
            }
        }
    }
}

void game_spy_turn(struct game_s *g, struct spy_turn_s *st)
{
    bool have_planet[PLAYER_NUM];
    int comptech1[PLAYER_NUM], comptech2[PLAYER_NUM];
    memset(st, 0, sizeof(*st));
    for (player_id_t i = PLAYER_0; i < g->players; ++i) {
        empiretechorbit_t *e = &(g->eto[i]);
        have_planet[i] = false;
        for (int j = 0; j < g->galaxy_stars; ++j) {
            if (g->planet[j].owner == i) {
                have_planet[i] = true;
                break;
            }
        }
        comptech1[i] = e->tech.percent[TECH_FIELD_COMPUTER];
        comptech2[i] = e->tech.percent[TECH_FIELD_COMPUTER];
        if (e->race == RACE_DARLOK) {
            comptech1[i] += 20;
            comptech2[i] += 30;
        }
        for (player_id_t j = PLAYER_0; j < g->players; ++j) {
            g->evn.spies_caught[j][i] = 0;
            g->evn.spies_caught[i][j] = 0;
            g->evn.stolen_field[j][i] = 0;
            g->evn.stolen_field[i][j] = 0;
            g->evn.stolen_tech[j][i] = 0;
            g->evn.stolen_tech[i][j] = 0;
            g->evn.stolen_spy[j][i] = 0;
            g->evn.stolen_spy[i][j] = 0;
            g->evn.spied_num[j][i] = 0;
            g->evn.spied_num[i][j] = 0;
            g->evn.spied_spy[j][i] = 0;
            g->evn.spied_spy[i][j] = 0;
            g->evn.sabotage_is_bases[j][i] = 0;
            g->evn.sabotage_is_bases[i][j] = 0;
            g->evn.sabotage_num[j][i] = 0;
            g->evn.sabotage_num[i][j] = 0;
            g->evn.sabotage_spy[j][i] = 0;
            g->evn.sabotage_spy[i][j] = 0;
        }
    }
    /*8af1*/
    for (player_id_t spy = PLAYER_0; spy < g->players; ++spy) {
        empiretechorbit_t *es = &(g->eto[spy]);
        for (player_id_t target = PLAYER_0; target < g->players; ++target) {
            empiretechorbit_t *et = &(g->eto[target]);
            int spies, dt1, dt2, numcaught, numsuccess, numfail;
            bool flag_frame, flag_any_caught;
            if ((spy == target) || (!have_planet[spy]) || (!have_planet[target])) {
                continue;
            }
            spies = es->spies[target];
            dt1 = comptech2[spy] - comptech1[target];
            dt2 = comptech1[target] - comptech2[spy];
            SETMAX(dt1, 0);
            SETMAX(dt2, 0);
            dt2 += et->security / 5;
            if (es->spymode[target] == SPYMODE_HIDE) {
                dt1 += 30;
            }
            numcaught = numsuccess = numfail = 0;
            flag_frame = flag_any_caught = false;
            if (es->spymode[target] == SPYMODE_HIDE) {
                for (int i = 0; i < spies; ++i) {
                    if ((rnd_1_n(100, &g->seed) + dt2) > 85) {
                        ++numcaught;
                    }
                }
                spies = 0;
            } else {
                /*8c30*/
                for (int i = 0; i < spies; ++i) {
                    int r;
                    r = rnd_1_n(100, &g->seed) + dt2;
                    if (r > 99) {
                        numcaught = spies;
                        numfail = spies;
                        flag_any_caught = true;
                        break;
                    } else if (r > 70) {
                        ++numcaught;
                        ++numfail;
                        flag_any_caught = true;
                    } else if (r > 50) {
                        ++numfail;
                        flag_any_caught = true;
                    } else if (r > 50) { /* BUG never true */
                        flag_any_caught = true;
                    }
                }
            }
            /*8c9e*/
            SUBSAT0(spies, numfail);
            SETMIN(numcaught, es->spies[target]);
            es->spies[target] -= numcaught;
            {
                int r = rnd_1_n(100, &g->seed) + dt1;
                if (r > 84) {
                    numsuccess = spies;
                }
                if (r > 100) {
                    flag_frame = true;
                }
            }
            g->evn.spies_caught[target][spy] = numcaught;
            if (es->spymode[target] == SPYMODE_ESPIONAGE) {
                game_spy_espionage(g, spy, target, flag_frame, numsuccess, flag_any_caught, st);
            } else if (es->spymode[target] == SPYMODE_SABOTAGE) {
                game_spy_sabotage(g, spy, target, flag_frame, numsuccess, flag_any_caught);
            }
        }
    }
}

void game_spy_esp_human(struct game_s *g, struct spy_turn_s *st)
{
    /* FIXME refactor for multiplayer */
    for (player_id_t spy = PLAYER_0; spy < g->players; ++spy) {
        struct spy_esp_s s[1];
        g->evn.newtech[spy].num = 0;
        if (IS_AI(g, spy)) {
            continue;
        }
        s->spy = spy;
        for (player_id_t target = PLAYER_0; target < g->players; ++target) {
            if ((spy != target) && (g->evn.spied_num[target][spy] > 0)) {
                uint8_t tbl_tech[TECH_FIELD_NUM];
                uint8_t flags_field;
                s->target = target;
                flags_field = 0;
                for (int i = 0; i < TECH_FIELD_NUM; ++i) {
                    tbl_tech[i] = 0;
                }
                for (int loops = 0; loops < 5; ++loops) {
                    int num;
                    num = game_spy_esp_sub1(g, s, 0, 0);
                    game_spy_esp_sub5(s, st->tbl_rmax[target][spy]);
                    for (int i = 0; i < num; ++i) {
                        int field;
                        field = s->tbl_field[i];
                        if (tbl_tech[field] == 0) {
                            tbl_tech[field] = s->tbl_tech2[i];
                            flags_field |= (1 << field);
                        }
                    }
                }
                if (flags_field != 0) {
                    int field;
                    field = ui_spy_steal(g, spy, target, flags_field);
                    if ((field >= 0) && (field < TECH_FIELD_NUM)) {
                        bool framed;
                        uint8_t planet;
                        planet = game_planet_get_random(g, target);
                        framed = (g->evn.spied_spy[target][spy] == -1);
                        g->evn.stolen_field[target][spy] = field;
                        g->evn.stolen_tech[target][spy] = tbl_tech[field];
                        game_tech_get_new(g, spy, field, tbl_tech[field], TECHSOURCE_SPY, planet, target, framed);
                        if (!framed) {
                            game_diplo_act(g, -g->evn.spied_spy[target][spy], spy, target, 4, 0, field);
                        }
                        game_tech_finish_new(g, spy);
                        ui_newtech(g, spy);
                        g->evn.newtech[spy].num = 0;
                    }
                }
            }
        }
    }
    for (player_id_t player = PLAYER_0; player < g->players; ++player) {
        uint16_t tbl[PLAYER_NUM];
        int n;
        if (IS_AI(g, player)) {
            continue;
        }
        n = 0;
        for (player_id_t spy = PLAYER_0; spy < g->players; ++spy) {
            uint8_t tech;
            tech = g->evn.stolen_tech[player][spy];
            if ((spy != player) && (tech != 0)) {
                tbl[n++] = ((((uint16_t)g->evn.stolen_spy[player][spy])) << 12) | ((((uint16_t)g->evn.stolen_field[player][spy])) << 8) | tech;
            }
        }
        for (int loops = 0; loops < n; ++loops) {
            for (int i = 0; i < n - 1; ++i) {
                uint16_t v0, v1;
                v0 = tbl[i];
                v1 = tbl[i + 1];
                if (v0 > v1) {
                    tbl[i + 1] = v0;
                    tbl[i] = v1;
                }
            }
        }
        for (int i = 0; i < n; ++i) {
            uint16_t v;
            v = tbl[i];
            ui_spy_stolen(g, player, (v >> 12) & 7, (v >> 8) & 7, v & 0xff);
        }
    }
}

void game_spy_sab_human(struct game_s *g)
{
    /* FIXME refactor for multiplayer */
    for (player_id_t player = PLAYER_0; player < g->players; ++player) {
        if (IS_AI(g, player)) {
            continue;
        }
        for (player_id_t target = PLAYER_0; target < g->players; ++target) {
            int snum;
            snum = g->evn.sabotage_num[target][player];
            if ((player != target) && (snum > 0)) {
                ui_sabotage_t act;
                uint8_t planet;
                planet_t *p;
                player_id_t other1, other2;
                act = ui_spy_sabotage_ask(g, player, target, &planet);
                g->evn.sabotage_planet[target][player] = planet;
                p = &(g->planet[planet]);
                /*ui_spy_sabotage_ask:*/
                switch (act) {
                    case UI_SABOTAGE_FACT: /*0*/
                        SETMIN(snum, p->factories);
                        p->factories -= snum;
                        break;
                    case UI_SABOTAGE_BASES: /*1*/
                        {
                            int n;
                            n = 0;
                            for (int i = 0; i < snum; ++i) {
                                if (!rnd_0_nm1(4, &g->seed)) {
                                    ++n;
                                }
                            }
                            snum = n;
                        }
                        SETMIN(snum, p->missile_bases);
                        p->missile_bases -= snum;
                        break;
                    case UI_SABOTAGE_REVOLT: /*2*/
                        snum = (p->pop * snum) / 200;
                        if (p->pop < snum) {
                            snum = p->pop - p->rebels;
                        }
                        SETMAX(snum, 1);
                        p->rebels += snum;
                        if (p->rebels >= (p->pop / 2)) {
                            p->unrest = PLANET_UNREST_REBELLION;
                            p->unrest_reported = false;
                            p->rebels = p->pop / 2;    /* FIXME BUG? AI cheat? */
                        }
                        break;
                    case UI_SABOTAGE_NONE: /*-1*/
                    default:
                        break;
                }
                other1 = PLAYER_NONE;
                other2 = PLAYER_NONE;
                if ((act != UI_SABOTAGE_NONE) && (act != UI_SABOTAGE_REVOLT)) {
                    if (g->evn.spied_spy[target][player] != -1) {
                        game_diplo_act(g, -g->evn.spied_spy[target][player], player, target, 6, planet, act);
                    } else if ((snum != 0) && (act > UI_SABOTAGE_FACT)) {
                        const empiretechorbit_t *et;
                        et = &(g->eto[target]);
                        for (int i = 0; (i < g->players) && (other2 == PLAYER_NONE); ++i) {
                            if ((i != player) && BOOLVEC_IS1(et->contact, i)) {
                                if (other1 == PLAYER_NONE) {
                                    other1 = i;
                                } else {
                                    other2 = i;
                                }
                            }
                        }
                        if (other2 == PLAYER_NONE) {
                            other1 = PLAYER_NONE;
                        }
                    }
                }
                if (act != UI_SABOTAGE_NONE) {
                    int other;
                    BOOLVEC_SET1(p->explored, player);
                    g->seen[player][planet].owner = p->owner;
                    g->seen[player][planet].pop = p->pop;
                    g->seen[player][planet].bases = p->missile_bases;
                    g->seen[player][planet].factories = p->factories;
                    other = ui_spy_sabotage_done(g, player, player, target, act, other1, other2, planet, snum);
                    if ((other2 != PLAYER_NONE) && (other != PLAYER_NONE)) {
                        int v;
                        v = -(rnd_1_n(12, &g->seed) + rnd_1_n(12, &g->seed));
                        game_diplo_act(g, v, other, target, 7, planet, act);
                    }
                }
            }
        }
    }
    for (player_id_t player = PLAYER_0; player < g->players; ++player) {
        if (IS_AI(g, player)) {
            continue;
        }
        for (player_id_t spy = PLAYER_0; spy < g->players; ++spy) {
            int snum;
            snum = g->evn.sabotage_num[player][spy];
            if ((player != spy) && (snum > 0)) {
                ui_sabotage_t act;
                uint8_t planet;
                int spy2;
                planet = g->evn.sabotage_planet[player][spy];
                spy2 = g->evn.sabotage_spy[player][spy];
                if (spy2 == -1) {
                    spy2 = PLAYER_NONE;
                }
                act = g->evn.sabotage_is_bases[player][spy] ? UI_SABOTAGE_BASES : UI_SABOTAGE_FACT;
                ui_spy_sabotage_done(g, player, spy2, player, act, PLAYER_NONE, PLAYER_NONE, planet, snum);
            }
        }
    }
}
