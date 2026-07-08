#include "config.h"

#include <stdio.h>
#include <string.h>

#include "game_ground.h"
#include "bits.h"
#include "boolvec.h"
#include "comp.h"
#include "game.h"
#include "game_ai.h"
#include "game_aux.h"
#include "game_endecode.h"
#include "game_misc.h"
#include "game_num.h"
#include "game_shiptech.h"
#include "game_spy.h"
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

static void game_ground_resolve_init(struct game_s *g, struct ground_s *gr)
{
    gr->seed = g->seed;
    {
        const planet_t *p = &(g->planet[gr->planet_i]);
        gr->fact = p->factories;
    }
    for (int i = 0; i < 2; ++i) {
        const empiretechorbit_t *e = &(g->eto[gr->s[i].player]);
        const shipresearch_t *srd = &(g->srd[gr->s[i].player]);
        const uint8_t *rc;
        uint8_t bestarmor, bestsuit, bestshield, bestweap, besti;
        gr->s[i].human = IS_HUMAN(g, gr->s[i].player);
        gr->s[i].force = 0;
        bestarmor = 0;
        bestsuit = besti = 0;
        rc = &(srd->researchcompleted[TECH_FIELD_CONSTRUCTION][0]);
        for (int j = 0; j < e->tech.completed[TECH_FIELD_CONSTRUCTION]; ++j) {
            uint8_t tier;
            tech_group_t group;
            group = game_tech_get_group(g->gaux, TECH_FIELD_CONSTRUCTION, rc[j]);
            tier = game_tech_get_tier(g->gaux, TECH_FIELD_CONSTRUCTION, rc[j]);
            if (group == TECH_GROUP_ARMOR) {
                bestarmor = tier;
            } else if (group == TECH_GROUP_PERSONAL_ARMOR) {
                bestsuit = tier;
                besti = rc[j];
            }
        }
        gr->s[i].force += bestarmor * 5 + bestsuit * 10;
        gr->s[i].armori = bestarmor;
        gr->s[i].suiti = besti;
        bestshield = besti = 0;
        rc = &(srd->researchcompleted[TECH_FIELD_FORCE_FIELD][0]);
        for (int j = 0; j < e->tech.completed[TECH_FIELD_FORCE_FIELD]; ++j) {
            uint8_t tier;
            tech_group_t group;
            group = game_tech_get_group(g->gaux, TECH_FIELD_FORCE_FIELD, rc[j]);
            tier = game_tech_get_tier(g->gaux, TECH_FIELD_FORCE_FIELD, rc[j]);
            if (group == TECH_GROUP_PERSONAL_SHIELD) {
                bestshield = tier;
                besti = rc[j];
            }
        }
        gr->s[i].shieldi = besti;
        if (bestshield != 0) {
            gr->s[i].force += bestshield * 10;
        }
        bestweap = besti = 0;
        rc = &(srd->researchcompleted[TECH_FIELD_WEAPON][0]);
        for (int j = 0; j < e->tech.completed[TECH_FIELD_WEAPON]; ++j) {
            uint8_t tier;
            tech_group_t group;
            group = game_tech_get_group(g->gaux, TECH_FIELD_WEAPON, rc[j]);
            tier = game_tech_get_tier(g->gaux, TECH_FIELD_WEAPON, rc[j]);
            if (group == TECH_GROUP_PERSONAL_WEAPONS) {
                bestweap = tier;
                besti = rc[j];
            }
        }
        gr->s[i].weapi = besti;
        if (bestweap != 0) {
            if (bestweap > 2) {
                ++bestweap;
            }
            gr->s[i].force += bestweap * 5;
        }
        if (e->race == RACE_BULRATHI) {
            gr->s[i].force += 25;
        }
    }
    gr->s[gr->flag_swap ? 0 : 1].force += 5;
    for (int i = 0; i < TECH_SPY_MAX; ++i) {
        gr->got[i].tech = 0;
        gr->got[i].field = 0;
    }
}

static void game_ground_show_init(struct game_s *g, struct ground_s *gr)
{
    for (int i = 0; i < 2; ++i) {
        char strbuf[GAME_GROUND_STRBUF_SIZE];
        uint8_t besti;
        gr->s[i].human = IS_HUMAN(g, gr->s[i].player);
        gr->s[i].pop2 = gr->s[i].pop1;
        gr->s[i].strnum = 1;
        lib_strcpy(strbuf, *tbl_shiptech_armor[gr->s[i].armori * 2].nameptr, sizeof(strbuf));
        util_str_tolower(&strbuf[1], sizeof(strbuf) - 1);
        lib_sprintf(gr->s[i].str[0], GAME_GROUND_STRBUF_SIZE, "%s ", strbuf);
        besti = gr->s[i].suiti;
        if (besti == 0) {
            lib_strcat(gr->s[i].str[0], game_str_gr_carmor, GAME_GROUND_STRBUF_SIZE);
        } else {
            game_tech_get_name(g->gaux, TECH_FIELD_CONSTRUCTION, besti, strbuf, GAME_GROUND_STRBUF_SIZE);
            lib_strcat(gr->s[i].str[0], strbuf, GAME_GROUND_STRBUF_SIZE);
        }
        besti = gr->s[i].shieldi;
        if (besti != 0) {
            game_tech_get_name(g->gaux, TECH_FIELD_FORCE_FIELD, besti, gr->s[i].str[1], GAME_GROUND_STRBUF_SIZE);
            gr->s[i].strnum = 2;
        }
        besti = gr->s[i].weapi;
        if (besti != 0) {
            game_tech_get_name(g->gaux, TECH_FIELD_WEAPON, besti, gr->s[i].str[gr->s[i].strnum++], GAME_GROUND_STRBUF_SIZE);
        }
    }
}

static int game_ground_encode_start(const struct ground_s *gr, uint8_t *buf, int pos)
{
    SG_1OOM_EN_U8(gr->planet_i);
    SG_1OOM_EN_U8(gr->s[0].player);
    SG_1OOM_EN_U8(gr->s[1].player);
    SG_1OOM_EN_U8(gr->flag_swap | (gr->flag_rebel ? 2 : 0));
    SG_1OOM_EN_U32(gr->seed);
    SG_1OOM_EN_U16(gr->inbound);
    SG_1OOM_EN_U16(gr->total_inbound);
    SG_1OOM_EN_U16(gr->fact);
    for (int i = 0; i < 2; ++i) {
        SG_1OOM_EN_U16(gr->s[i].force);
        SG_1OOM_EN_U16(gr->s[i].pop1);
        SG_1OOM_EN_U8(gr->s[i].armori);
        SG_1OOM_EN_U8(gr->s[i].suiti);
        SG_1OOM_EN_U8(gr->s[i].shieldi);
        SG_1OOM_EN_U8(gr->s[i].weapi);
    }
    return pos;
}

static int game_ground_encode_end(const struct ground_s *gr, uint8_t *buf, int pos)
{
    for (int i = 0; i < TECH_SPY_MAX; ++i) {
        SG_1OOM_EN_U8(gr->got[i].tech);
        SG_1OOM_EN_U8(gr->got[i].field);
    }
    return pos;
}

static int game_ground_decode(struct ground_s *gr, const uint8_t *buf, int pos)
{
    {
        uint8_t v;
        SG_1OOM_DE_U8(v);
        if (v == 0xff) {
            return 0;
        }
        gr->planet_i = v;
    }
    SG_1OOM_DE_U8(gr->s[0].player);
    SG_1OOM_DE_U8(gr->s[1].player);
    {
        uint8_t v;
        SG_1OOM_DE_U8(v);
        gr->flag_swap = (v & 1) != 0;
        gr->flag_rebel = (v & 2) != 0;
    }
    SG_1OOM_DE_U32(gr->seed);
    SG_1OOM_DE_U16(gr->inbound);
    SG_1OOM_DE_U16(gr->total_inbound);
    SG_1OOM_DE_U16(gr->fact);
    for (int i = 0; i < 2; ++i) {
        SG_1OOM_DE_U16(gr->s[i].force);
        SG_1OOM_DE_U16(gr->s[i].pop1);
        SG_1OOM_DE_U8(gr->s[i].armori);
        SG_1OOM_DE_U8(gr->s[i].suiti);
        SG_1OOM_DE_U8(gr->s[i].shieldi);
        SG_1OOM_DE_U8(gr->s[i].weapi);
    }
    {
        int num = 0;
        for (int i = 0; i < TECH_SPY_MAX; ++i) {
            uint8_t t;
            SG_1OOM_DE_U8(t);
            gr->got[i].tech = t;
            if (t != 0) {
                ++num;
            }
            SG_1OOM_DE_U8(gr->got[i].field);
        }
        gr->techchance = num;
    }
    return pos;
}

static inline uint8_t *game_ground_get_buf(struct game_s *g)
{
    return g->gaux->savebuf;    /* unused during turn processing, large enough for ground data */
}

static void game_ground_finish(struct game_s *g, struct ground_s *gr)
{
    planet_t *p = &(g->planet[gr->planet_i]);
    struct spy_esp_s s[1];
    g->seed = gr->seed;
    gr->techchance = 0;
    if (gr->s[0].pop1 > 0) {
        if (gr->flag_rebel) {
            p->unrest = PLANET_UNREST_RESOLVED;
            p->pop = p->pop - p->rebels + gr->s[0].pop1;
            p->rebels = 0;
        } else {
            int fact, chance, num;
            fact = gr->fact;
            chance = 0;
            for (int i = 0; i < fact; ++i) {
                if (!rnd_0_nm1(50, &g->seed)) {
                    ++chance;
                }
            }
            gr->techchance = chance;
            s->target = gr->s[1].player;
            s->spy = gr->s[0].player;
            num = game_spy_esp_sub1(g, s, 0, 0);
            SETMIN(num, chance);
            s->tnum = num;
            for (int i = 0; i < num; ++i) {
                gr->got[i].tech = s->tbl_tech2[i];
                gr->got[i].field = s->tbl_field[i];
                game_tech_get_new(g, gr->s[0].player, s->tbl_field[i], s->tbl_tech2[i], TECHSOURCE_FOUND, gr->planet_i, gr->s[1].player, false);
            }
            gr->techchance = num;
            game_planet_destroy(g, gr->planet_i, gr->s[0].player);
            p->owner = gr->s[0].player;
            p->pop = gr->s[0].pop1;
            p->bc_to_refit = 0;
            p->pop_oper_fact = 1;
        }
    } else {
        if (gr->flag_rebel) {
            p->rebels = gr->s[1].pop1;
            if (p->rebels == 0) {
                p->unrest = PLANET_UNREST_RESOLVED;
            }
        } else {
            p->pop = gr->s[1].pop1;
        }
    }
    SETMIN(p->pop, p->max_pop3);
}

/* -------------------------------------------------------------------------- */

void game_ground_kill(struct ground_s *gr)
{
    int v1, v2, death;
    v1 = rnd_1_n(100, &gr->seed) + gr->s[0].force;
    v2 = rnd_1_n(100, &gr->seed) + gr->s[1].force;
    if (v1 <= v2) {
        death = 0;
    } else {
        death = 1;
    }
    gr->death = death;
    --gr->s[death].pop1;
}

const uint8_t *game_turn_ground_resolve_all(struct game_s *g)
{
    struct ground_s gr[1];
    uint8_t *buf = game_ground_get_buf(g);
    int pos = 0;
    gr->seed = g->seed;
    for (int pli = 0; pli < g->galaxy_stars; ++pli) {
        planet_t *p = &(g->planet[pli]);
        player_id_t powner;
        powner = p->owner;
        for (player_id_t i = 0; i < g->players; ++i) {
            if ((i != powner) || (p->unrest == PLANET_UNREST_REBELLION)) {
                if ((powner != PLAYER_NONE) && (p->inbound[i] > 0)) {
                    int pop_planet;
                    gr->flag_rebel = (p->unrest == PLANET_UNREST_REBELLION) && IS_HUMAN(g, i) && (p->owner == i);
                    gr->inbound = p->inbound[i];
                    gr->total_inbound = p->total_inbound[i];
                    gr->s[0].pop2 = gr->s[0].pop1 = p->inbound[i];
                    pop_planet = gr->s[1].pop2 = gr->s[1].pop1 = gr->flag_rebel ? p->rebels : p->pop;
                    gr->s[0].player = i;
                    gr->s[1].player = powner;
                    gr->planet_i = pli;
                    gr->flag_swap = false;
                    if (IS_HUMAN(g, i) || IS_HUMAN(g, powner)) {
                        int t;
                        gr->flag_swap = true;
                        t = gr->s[0].pop2; gr->s[0].pop2 = gr->s[1].pop2; gr->s[1].pop2 = t;
                        t = gr->s[0].pop1; gr->s[0].pop1 = gr->s[1].pop1; gr->s[1].pop1 = t;
                        t = gr->s[0].player; gr->s[0].player = gr->s[1].player; gr->s[1].player = t;
                    }
                    game_ground_resolve_init(g, gr);
                    if ((gr->s[0].pop1 != 0) && (gr->s[1].pop1 != 0)) {
                        if (gr->s[0].human || gr->s[1].human) {
                            pos = game_ground_encode_start(gr, buf, pos);
                        }
                        while ((gr->s[0].pop1 != 0) && (gr->s[1].pop1 != 0)) {
                            game_ground_kill(gr);
                        }
                        if (gr->flag_swap) {
                            int t;
                            t = gr->s[0].pop1; gr->s[0].pop1 = gr->s[1].pop1; gr->s[1].pop1 = t;
                            t = gr->s[0].player; gr->s[0].player = gr->s[1].player; gr->s[1].player = t;
                        }
                        game_ground_finish(g, gr);
                        if (gr->s[0].human || gr->s[1].human) {
                            pos = game_ground_encode_end(gr, buf, pos);
                        }
                        pop_planet -= gr->s[1].pop1;
                        SETMAX(pop_planet, 1);
                        game_ai->ground(g, gr->s[1].player, gr->s[0].player, pli, pop_planet, (p->owner != powner));
                    }
                }
                powner = p->owner;
            }
        }
    }
    SG_1OOM_EN_U8(0xff);
    return buf;
}

int game_turn_ground_show_all(struct game_s *g, const uint8_t *buf)
{
    struct ground_s gr[1];
    int pos = 0;
    /* TODO reorder for minimum player switching */
    while ((pos = game_ground_decode(gr, buf, pos)) > 0) {
        game_ground_show_init(g, gr);
        ui_ground(g, gr);
    }
    return (pos == 0) ? 0 : -1;
}
