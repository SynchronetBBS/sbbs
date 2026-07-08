#include "config.h"

#include <string.h>

#include "game_battle.h"
#include "boolvec.h"
#include "comp.h"
#include "game.h"
#include "game_ai.h"
#include "game_aux.h"
#include "game_battle_human.h"
#include "game_diplo.h"
#include "game_fleet.h"
#include "game_num.h"
#include "game_parsed.h"
#include "game_str.h"
#include "lib.h"
#include "log.h"
#include "types.h"
#include "ui.h"
#include "util_math.h"

/* -------------------------------------------------------------------------- */

#define PARTY_NUM   ((int)PLAYER_NUM + (int)MONSTER_NUM)
#define COPY_PROP(bi_, sp_, xyz_) bi_->xyz_ = sp->xyz_
#define COPY_BOOL_TO_INT(b_, i_, f_) b_->i_ = (b_->sbmask & (1 << SHIP_SPECIAL_BOOL_##f_)) ? 1 : 0

/* -------------------------------------------------------------------------- */

static void game_battle_item_from_parsed(struct battle_item_s *b, const shipparsed_t *sp)
{
    memset(b, 0, sizeof(*b));
    COPY_PROP(b, sp, look);
    lib_strcpy(b->name, sp->name, SHIP_NAME_LEN);
    COPY_PROP(b, sp, hull);
    for (int i = 0; i < SPECIAL_SLOT_NUM; ++i) {
        b->special[i] = sp->special[i];
    }
    COPY_PROP(b, sp, repair);
    COPY_PROP(b, sp, misshield);
    COPY_PROP(b, sp, look);
    COPY_PROP(b, sp, pulsar);
    COPY_PROP(b, sp, stream);
    COPY_PROP(b, sp, pshield);
    COPY_PROP(b, sp, sbmask);
    COPY_PROP(b, sp, extrarange);
    COPY_PROP(b, sp, num);
    COPY_PROP(b, sp, complevel);
    COPY_PROP(b, sp, defense);
    COPY_PROP(b, sp, misdefense);
    b->hp1 = sp->hp;
    b->hp2 = sp->hp;
    COPY_PROP(b, sp, absorb);
    COPY_PROP(b, sp, man);
    for (int i = 0; i < WEAPON_SLOT_NUM; ++i) {
        b->wpn[i].t = sp->wpnt[i];
        b->wpn[i].n = sp->wpnn[i];
    }
    COPY_BOOL_TO_INT(b, stasis, STASIS);
    COPY_BOOL_TO_INT(b, subspace, SUBSPACE);
    COPY_BOOL_TO_INT(b, blackhole, BLACKHOLE);
    COPY_BOOL_TO_INT(b, warpdis, WARPDIS);
    COPY_BOOL_TO_INT(b, technull, TECHNULL);
    COPY_BOOL_TO_INT(b, repulsor, REPULSOR);
    COPY_BOOL_TO_INT(b, cloak, CLOAK);
}

static void game_battle_item_add(struct battle_s *bt, const shipparsed_t *sp, battle_side_i_t side)
{
    struct battle_item_s *b;
    int itemi, shiptbli;
    switch (side) {
        default:
        case SIDE_NONE/*planet*/:
            itemi = shiptbli = 0;
            b = &(bt->item[itemi]);
            game_battle_item_from_parsed(b, sp);
            b->absorb += b->pshield;
            b->wpn[0].numshots = -1;
            b->wpn[1].numshots = -1;
            if (bt->bases == 0) {
                b->wpn[0].t = 0;
                b->wpn[0].n = 0;
            }
            b->gfx = ui_gfx_get_planet(b->look);
            break;
        case SIDE_L:
            itemi = ++bt->s[SIDE_L].items;
            b = &(bt->item[itemi]);
            shiptbli = itemi - 1;
            break;
        case SIDE_R:
            itemi = ++bt->s[SIDE_R].items + bt->s[SIDE_L].items;
            b = &(bt->item[itemi]);
            shiptbli = bt->s[SIDE_R].items - 1;
            break;
    }
    if (side != SIDE_NONE/*planet*/) {
        game_battle_item_from_parsed(b, sp);
        if (b->sbmask & (1 << SHIP_SPECIAL_BOOL_SCANNER)) {
            bt->s[side].flag_have_scan = true;
        }
        b->side = side;
        b->gfx = ui_gfx_get_ship(b->look);
        b->shiptbli = shiptbli;
    }
}

static void game_battle_post(struct game_s *g, int loser, int winner, uint8_t from)
{
    if (loser >= PLAYER_NUM) {
        monster_id_t mi;
        mi = loser - PLAYER_NUM;
        switch (mi) {
            case MONSTER_CRYSTAL:
                g->evn.crystal.killer = winner;
                break;
            case MONSTER_AMOEBA:
                g->evn.amoeba.killer = winner;
                break;
            case MONSTER_GUARDIAN:
                g->evn.have_guardian = false;
                g->guardian_killer = winner;
                break;
            default:
                break;
        }
    } else {
        empiretechorbit_t *e = &(g->eto[loser]);
        fleet_orbit_t *o = &(e->orbit[from]);
        const planet_t *pf = &g->planet[from];
        uint8_t dest = PLANET_NONE;
        int mindist = 10000;
        shipcount_t ships[NUM_SHIPDESIGNS];
        uint8_t shiptypes[NUM_SHIPDESIGNS];
        uint8_t numtypes = e->shipdesigns_num;
        for (int i = 0; i < numtypes; ++i) {
            shiptypes[i] = i;
            ships[i] = o->ships[i];
        }
        for (int i = 0; i < g->galaxy_stars; ++i) {
            const planet_t *pt = &g->planet[i];
            if ((i != from) && (pt->owner == loser)) {
                int dist;
                dist = util_math_dist_fast(pf->x, pf->y, pt->x, pt->y);
                if (dist < mindist) {
                    mindist = dist;
                    dest = i;
                }
            }
        }
        if ((dest != PLANET_NONE) /*&& (numtypes > 0)*/) {
            game_send_fleet_retreat(g, loser, from, dest, ships, shiptypes, numtypes);
        }
        for (int i = 0; i < NUM_SHIPDESIGNS; ++i) {
            o->ships[i] = 0;
        }
    }
}

static void game_battle_prepare_p1(struct battle_s *bt, battle_side_i_t side, uint8_t planet_i)
{
    const struct game_s *g = bt->g;
    const empiretechorbit_t *e = &(g->eto[bt->s[side].party]);
    const shipdesign_t *sd = &(g->srd[bt->s[side].party].design[0]);
    bt->s[side].apparent_force = 0;
    for (int i = 0; i < e->shipdesigns_num; ++i) {
        bt->s[side].apparent_force += (sd[i].hull + 1) * e->orbit[planet_i].ships[i];
    }
    bt->s[side].race = e->race;
}

static void game_battle_prepare_add_ships(struct battle_s *bt, battle_side_i_t side, uint8_t planet_i)
{
    const struct game_s *g = bt->g;
    const empiretechorbit_t *e = &(g->eto[bt->s[side].party]);
    const shipdesign_t *sd = &(g->srd[bt->s[side].party].design[0]);
    bool flag_shield_disable = (g->planet[planet_i].battlebg == 0);
    shipparsed_t sp[1];
    int num_types = 0;
    for (int i = 0; i < e->shipdesigns_num; ++i) {
        shipcount_t s;
        s = e->orbit[planet_i].ships[i];
        if (s > 0) {
            bt->s[side].tbl_ships[num_types] = s;
            bt->s[side].tbl_shiptype[num_types] = i;
            game_parsed_from_design(sp, &sd[i], s);
            if (bt->s[side].race == RACE_MRRSHAN) {
                sp->complevel += 4;
            }
            if (bt->s[side].race == RACE_ALKARI) {
                sp->defense += 3;
                sp->misdefense += 3;
            }
            if (flag_shield_disable) {
                sp->pshield = 0;
                sp->absorb = 0;
                sp->shield = 0;
            }
            game_battle_item_add(bt, sp, side);
            ++num_types;
        }
    }
    bt->s[side].num_types = num_types;
    bt->s[side].flag_human = IS_HUMAN(g, bt->s[side].party);
    bt->s[side].flag_auto = !bt->s[side].flag_human;
}

/* -------------------------------------------------------------------------- */

void game_battle_prepare(struct battle_s *bt, int party_r, int party_l, uint8_t planet_i)
{
    struct game_s *g = bt->g;
    const planet_t *p = &(g->planet[planet_i]);
    shipparsed_t sp[1];
    {
        bool t = bt->flag_human_att;
        memset(bt, 0, sizeof(*bt));
        bt->flag_human_att = t;
    }
    bt->g = g;
    bt->s[SIDE_R].party = party_r;
    bt->s[SIDE_L].party = party_l;
    bt->planet_i = planet_i;
    bt->pop = p->pop;
    bt->fact = p->factories;
    game_battle_prepare_p1(bt, SIDE_L, planet_i);
    if (party_r < PLAYER_NUM) {
        game_battle_prepare_p1(bt, SIDE_R, planet_i);
    } else {
        bt->s[SIDE_R].apparent_force = 1;
        bt->s[SIDE_R].race = RACE_NUM/*monster*/;
        bt->s[SIDE_R].flag_human = false;
        bt->s[SIDE_R].flag_auto = 1;
    }
    {
        player_id_t owner;
        owner = p->owner;
        if ((owner != PLAYER_NONE) && ((owner == party_l) || (owner == party_r))) {
            bt->planet_side = (owner == party_l) ? SIDE_L : SIDE_R;
            bt->bases = p->missile_bases;
            game_parsed_from_planet(sp, g, p);
            game_battle_item_add(bt, sp, SIDE_NONE/*planet*/);
        } else {
            bt->planet_side = SIDE_NONE;
        }
    }
    game_battle_prepare_add_ships(bt, SIDE_L, planet_i);
    if (party_r >= PLAYER_NUM) {
        monster_id_t mi;
        mi = party_r - PLAYER_NUM;
        memcpy(sp, &tbl_monster[mi][g->difficulty], sizeof(*sp));
        strncpy(sp->name, game_str_tbl_monsh_names[mi], SHIP_NAME_LEN);
        sp->name[SHIP_NAME_LEN - 1] = 0;
        game_battle_item_add(bt, sp, SIDE_R);
        bt->s[SIDE_R].num_types = 1;
        /* BUG? these were uninitialized */
        bt->s[SIDE_R].tbl_ships[0] = 1;
        bt->s[SIDE_R].tbl_shiptype[0] = 0;
    } else {
        game_battle_prepare_add_ships(bt, SIDE_R, planet_i);
    }
}

void game_battle_handle_all(struct game_s *g)
{
    struct battle_s bt[1];
    uint8_t monster_planet[MONSTER_NUM];
    bt->g = g;
    for (monster_id_t i = 0; i < MONSTER_NUM; ++i) {
        monster_planet[i] = PLANET_NONE;
    }
    for (monster_id_t i = MONSTER_CRYSTAL; i <= MONSTER_AMOEBA; ++i) {
        const monster_t *m;
        m = (i == MONSTER_CRYSTAL) ? &(g->evn.crystal) : &(g->evn.amoeba);
        if (m->exists) {
            const planet_t *p;
            p = &(g->planet[m->dest]);
            if ((m->x == p->x) && (m->y == p->y)) {
                monster_planet[i] = m->dest;
            }
        }
    }
    if (g->evn.have_guardian) {
        monster_planet[MONSTER_GUARDIAN] = g->evn.planet_orion_i;
    }
    /* FIXME refactor this so that human/AI conflicts can be resolved in parallel in (non-local) multiplayer */
    for (int pli = 0; pli < g->galaxy_stars; ++pli) {
        planet_t *p = &(g->planet[pli]);
        player_id_t owner;
        int sum_forces;
        BOOLVEC_DECLARE(tbl_have_force, PARTY_NUM);
        BOOLVEC_CLEAR(tbl_have_force, PARTY_NUM);
        owner = p->owner;
        for (player_id_t i = PLAYER_0; i < g->players; ++i) {
            empiretechorbit_t *e = &(g->eto[i]);
            if ((owner == i) && (p->missile_bases > 0)) {
                BOOLVEC_SET1(tbl_have_force, i);
            } else {
                for (int j = 0; j < e->shipdesigns_num; ++j) {
                    if (e->orbit[pli].ships[j] > 0) {
                        BOOLVEC_SET1(tbl_have_force, i);
                        break;
                    }
                }
            }
        }
        for (monster_id_t i = MONSTER_CRYSTAL; i < MONSTER_NUM; ++i) {
            if (pli == monster_planet[i]) {
                BOOLVEC_SET1(tbl_have_force, (int)PLAYER_NUM + i);
            } else if (game_num_monster_rest_att && (i != MONSTER_GUARDIAN)) {
                /* 1oom option: allow fighting resting monsters */
                const monster_t *m;
                m = (i == MONSTER_CRYSTAL) ? &(g->evn.crystal) : &(g->evn.amoeba);
                if ((m->counter >= 0) && (m->x == p->x) && (m->y == p->y)) {
                    BOOLVEC_SET1(tbl_have_force, (int)PLAYER_NUM + i);
                }
            }
        }
        sum_forces = 0;
        for (int i = 0; i < PARTY_NUM; ++i) {
            if (BOOLVEC_IS1(tbl_have_force, i)) {
                ++sum_forces;
            }
        }
        while (sum_forces > 1) {
            int tbl_party[PARTY_NUM];
            int num_party, party_def, party_att;
            empiretechorbit_t *e;
            num_party = 0;
            tbl_party[0] = -1;
            if ((owner != PLAYER_NONE) && BOOLVEC_IS1(tbl_have_force, owner)) {
                tbl_party[0] = owner;
                num_party = 1;
            }
            for (int i = 0; i < PARTY_NUM; ++i) {
                if ((i != tbl_party[0]) && BOOLVEC_IS1(tbl_have_force, i)) {
                    tbl_party[num_party++] = i;
                }
            }
            party_def = tbl_party[0];
            if ((num_party < 2) || (party_def >= PLAYER_NUM)) {
                break;
            }
            e = &(g->eto[party_def]);
            party_att = tbl_party[1];
            if ((party_att < PLAYER_NUM)
              && ((e->treaty[party_att] == TREATY_ALLIANCE) || ((e->treaty[party_att] == TREATY_NONAGGRESSION) && (party_def != owner)))
            ) {
                /* TODO BUG? the scenario of owner dying to attacker and an alliance partner not doing anything
                   afterwards on the same turn could be fixed */
                BOOLVEC_SET0(tbl_have_force, party_att);
            } else {
                if (!IS_HUMAN(g, party_def) && !IS_HUMAN(g, party_att)) {
                    game_battle_prepare(bt, party_att, party_def, pli);
                    if (game_ai->battle_ai_ai_resolve(bt)) {
                        /* HACK _att won, swap variables */
                        int t = party_def; party_def = party_att; party_att = t;
                    }
                    /* _def won */
                    BOOLVEC_SET0(tbl_have_force, party_att);
                    game_battle_post(g, party_att, party_def, pli);
                } else {
                    /*11926*/
                    /* BUG? first check not in MOO1, reads past table if monster */
                    if ((party_att < PLAYER_NUM) && IS_AI(g, party_att) && (g->evn.ceasefire[party_def][party_att] > 0)) {
                        BOOLVEC_SET0(tbl_have_force, party_att);
                        game_battle_post(g, party_att, party_def, pli);
                    } else {
                        /*1195f*/
                        int party_l, party_r;
                        if (IS_HUMAN(g, party_def)) {
                            party_l = party_def;
                            party_r = party_att;
                            bt->flag_human_att = false;
                        } else {
                            party_l = party_att;
                            party_r = party_def;
                            bt->flag_human_att = true;
                        }
                        game_battle_prepare(bt, party_r, party_l, pli);
                        if (game_battle_with_human(bt)) {
                            /* HACK _r won, swap variables */
                            int t = party_r; party_r = party_l; party_l = t;
                        }
                        /* _l won */
                        BOOLVEC_SET0(tbl_have_force, party_r);
                        game_battle_post(g, party_r, party_l, pli);
                        /*v14 = 1;*/
                    }
                }
            }
            /*119eb*/
            sum_forces = 0;
            for (int i = 0; i < PARTY_NUM; ++i) {
                if (BOOLVEC_IS1(tbl_have_force, i)) {
                    ++sum_forces;
                }
            }
        }
    }
}

void game_battle_finish(struct battle_s *bt)
{
    struct game_s *g = bt->g;
    planet_t *p = &(g->planet[bt->planet_i]);
    if (bt->planet_side != SIDE_NONE) {
        p->max_pop3 -= bt->biodamage;
        SETMAX(p->max_pop3, 10);
        p->missile_bases = bt->bases;
        if ((p->pop == 0) && (bt->s[SIDE_R].party < PLAYER_NUM)) {
            game_planet_destroy(g, bt->planet_i, bt->s[SIDE_R].party);
        }
    }
    for (battle_side_i_t side = SIDE_L; side <= SIDE_R; ++side) {
        if (bt->s[side].party < PLAYER_NUM) {
            empiretechorbit_t *e = &(g->eto[bt->s[side].party]);
            const shipdesign_t *sd = &(g->srd[bt->s[side].party].design[0]);
            shipcount_t *os = &(e->orbit[bt->planet_i].ships[0]);
            for (int i = 0; i < bt->s[side].num_types; ++i) {
                uint8_t st;
                shipcount_t n;
                st = bt->s[side].tbl_shiptype[i];
                n = bt->s[side].tbl_ships[i];
                os[st] = n;
                bt->s[side].apparent_force -= (sd[st].hull + 1) * n;
            }
        }
    }
    game_diplo_battle_finish(g, bt->s[SIDE_L].party, bt->s[SIDE_R].party, bt->pop - p->pop, bt->s[SIDE_L].apparent_force, bt->biodamage, bt->s[SIDE_R].apparent_force, bt->planet_i);
}

void game_battle_count_hulls(const struct battle_s *bt, shipsum_t force[2][SHIP_HULL_NUM])
{
    const struct game_s *g = bt->g;
    for (int s = 0; s < 2; ++s) {
        int party = bt->s[s].party;
        for (ship_hull_t h = 0; h < SHIP_HULL_NUM; ++h) {
            force[s][h] = 0;
        }
        if (party < PLAYER_NUM) {
            const shipcount_t *ships;
            const shipdesign_t *sd;
            sd = &(g->srd[party].design[0]);
            ships = &(g->eto[party].orbit[bt->planet_i].ships[0]);
            for (int i = 0; i < NUM_SHIPDESIGNS; ++i) {
                shipcount_t n;
                n = ships[i];
                if (n) {
                    force[s][sd[i].hull] += n;
                }
            }
        } else {
            const struct battle_item_s *b = &(bt->item[bt->s[SIDE_L].items + 1]);
            force[s][b->hull] += b->num;
        }
    }
}
