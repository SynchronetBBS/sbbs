/*
    The "Classic" AI mimics MOO1 v1.3 behaviour.
    Minor bug fixes are allowed.
    The "Classic+" AI fixes small bugs and does small improvements.
    Major improvements go to other game_ai_* implementations.
*/

#include "config.h"

#include <stdlib.h> /* abs */
#include <string.h>

#include "game_ai_classic.h"
#include "boolvec.h"
#include "comp.h"
#include "game.h"
#include "game_ai.h"
#include "game_audience.h"
#include "game_aux.h"
#include "game_battle.h"
#include "game_battle_human.h"
#include "game_design.h"
#include "game_diplo.h"
#include "game_election.h"
#include "game_fleet.h"
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
#include "util_math.h"

/* -------------------------------------------------------------------------- */

static void game_ai_classic_new_game_init(struct game_s *g, player_id_t player, uint8_t home)
{
    int n = 0;
    planet_t *p = &g->planet[home];
    empiretechorbit_t *e = &g->eto[player];
    for (int i = 0; i < g->galaxy_stars; ++i) {
        const planet_t *q;
        q = &g->planet[i];
        if (1
          && (q->type > (PLANET_TYPE_MINIMAL - 1))
          && (util_math_dist_fast(q->x, q->y, p->x, p->y) <= 30)
        ) {
            ++n;
        }
    }
    e->ai_p3_countdown = rnd_1_n(6, &g->seed) + 14;
    e->ai_p2_countdown = rnd_1_n(10, &g->seed);
    if (n > 1) {
        p->slider[PLANET_SLIDER_SHIP] = 10;
        p->slider[PLANET_SLIDER_DEF] = 0;
        p->slider[PLANET_SLIDER_IND] = 90;
        p->slider[PLANET_SLIDER_ECO] = 0;
        p->slider[PLANET_SLIDER_TECH] = 0;
        e->tech.slider[TECH_FIELD_COMPUTER] = 20;
        e->tech.slider[TECH_FIELD_CONSTRUCTION] = 20;
        e->tech.slider[TECH_FIELD_FORCE_FIELD] = 0;
        e->tech.slider[TECH_FIELD_PLANETOLOGY] = 40;
        e->tech.slider[TECH_FIELD_PROPULSION] = 20;
        e->tech.slider[TECH_FIELD_WEAPON] = 0;
    } else {
        e->ai_p3_countdown = rnd_1_n(6, &g->seed) + 24;
        p->slider[PLANET_SLIDER_SHIP] = 0;
        p->slider[PLANET_SLIDER_DEF] = 0;
        p->slider[PLANET_SLIDER_IND] = 60;
        p->slider[PLANET_SLIDER_ECO] = 10;
        p->slider[PLANET_SLIDER_TECH] = 40;
        e->tech.slider[TECH_FIELD_COMPUTER] = 0;
        e->tech.slider[TECH_FIELD_CONSTRUCTION] = 0;
        e->tech.slider[TECH_FIELD_FORCE_FIELD] = 0;
        e->tech.slider[TECH_FIELD_PLANETOLOGY] = 0;
        e->tech.slider[TECH_FIELD_PROPULSION] = 100;
        e->tech.slider[TECH_FIELD_WEAPON] = 0;
    }
    /* XXX these shipi values are wrong, but fixed by first next turn */
    e->shipi_colony = 1;
    e->shipi_bomber = 4;
}

static void game_ai_classic_new_game_tech(struct game_s *g)
{
    for (player_id_t pli = PLAYER_0; pli < g->players; ++pli) {
        if (IS_HUMAN(g, pli)) {
            continue;
        }
        for (tech_field_t field = TECH_FIELD_COMPUTER; field < TECH_FIELD_NUM; ++field) {
            uint8_t tech;
            uint8_t *rl;
            rl = g->srd[pli].researchlist[field][0];
            do {
                tech = rl[rnd_0_nm1(3, &g->seed)];
            } while (tech == 0);
            if (game_num_ai_first_tech_cost_fix) {
                game_tech_start_next(g, pli, field, tech);
            } else {
                empiretechorbit_t *e;
                e = &g->eto[pli];
                e->tech.project[field] = tech;
                e->tech.cost[field] = tech * tech * 50;
            }
        }
    }
}

/* -------------------------------------------------------------------------- */

struct ai_turn_p1_s {
    bool have_colonizable;
    bool need_conquer;
    uint32_t tbl_shipthreat[PLAYER_NUM + 1][NUM_SHIPDESIGNS];
    uint16_t tbl_xcenter[PLAYER_NUM];
    uint16_t tbl_ycenter[PLAYER_NUM];
    uint64_t tbl_force_own[PLANETS_MAX];
    int num_fronts;
    int tbl_front_relation[PLAYER_NUM];
    uint8_t tbl_front_planet[PLAYER_NUM];
    uint64_t force_own_sum;
    int planet_en_num;
    int planet_own_num;
    int tbl_planet_own_w[PLANETS_MAX];
    int tbl_planet_en_w[PLANETS_MAX];
    uint8_t tbl_planet_own_i[PLANETS_MAX];
    uint8_t tbl_planet_en_i[PLANETS_MAX];
};

struct ai_turn_p2_s {
    struct game_design_s gd;
    int shiptype;
    ship_hull_t hull;
    uint8_t shiplook;
    bool have_pulsar;
    bool have_repulwarp;
};

/* -------------------------------------------------------------------------- */

static int game_ai_best_speed(const struct game_s *g, player_id_t player_i)
{
    /* FIXME: The name of the function does not correctly reflect its meaning.
       Misunderstanding of the meaning of this function was the cause of errors.
    */
    return game_tech_player_best_tech(g, TECH_FIELD_PROPULSION, 0, 6, 50, player_i) + 3;
}

static void game_ai_classic_turn_p1_send_scout(struct game_s *g, struct ai_turn_p1_s *ait, player_id_t pi)
{
    empiretechorbit_t *e = &(g->eto[pi]);
    BOOLVEC_DECLARE(tbl_planet_ignore, PLANETS_MAX);
    uint8_t tbl_planet_scout[PLANETS_MAX];
    shipsum_t tbl_ownorbit[PLANETS_MAX];
    int num_to_scout = 0;

    BOOLVEC_CLEAR(tbl_planet_ignore, PLANETS_MAX);
    for (int i = 0; i < g->galaxy_stars; ++i) {
        const planet_t *p = &(g->planet[i]);
        shipsum_t ships;
        ships = 0;
        for (player_id_t pi2 = PLAYER_0; pi2 < g->players; ++pi2) {
            if ((pi != pi2) && (e->treaty[pi2] != TREATY_ALLIANCE)) {
                const shipcount_t *t = &(g->eto[pi2].orbit[i].ships[0]);
                int sd_num = g->eto[pi2].shipdesigns_num;
                for (int j = 0; j < sd_num; ++j) {
                    ships += t[j];
                }
            }
        }
        if (0
          || BOOLVEC_IS1(p->explored, pi)
          || (!p->within_frange[pi])
          || ((g->year < 150) && (g->evn.planet_orion_i == i)) /* XXX Orion is unconditionally ignored below */
          || (ships > 0)
        ) {
            BOOLVEC_SET1(tbl_planet_ignore, i);
        }
    }
    for (int j = 0; j < g->enroute_num; ++j) {
        const fleet_enroute_t *r = &(g->enroute[j]);
        if (r->owner == pi) {
            BOOLVEC_SET1(tbl_planet_ignore, r->dest);
        }
    }
    ait->have_colonizable = false;
    ait->need_conquer = !rnd_0_nm1(8 - g->difficulty, &g->seed);
    for (int i = 0; i < g->galaxy_stars; ++i) {
        const planet_t *p = &(g->planet[i]);
        if (p->owner == PLAYER_NONE) {
            if (BOOLVEC_IS0(tbl_planet_ignore, i) && (g->evn.planet_orion_i != i)) {
                tbl_planet_scout[num_to_scout++] = i;
            }
            if ((p->type >= e->have_colony_for) && ((g->evn.planet_orion_i != i) || (!g->evn.have_guardian))) {
                ait->have_colonizable = true;
                ait->need_conquer = false;
            }
        }
    }
    if (g->end != GAME_END_NONE) {
        ait->need_conquer = false;
    }
    for (int i = 0; i < g->galaxy_stars; ++i) {
        const shipcount_t *t = &(e->orbit[i].ships[0]);
        shipsum_t ships; /* MOO1 uses uint16_t */
        ships = 0;
        for (int j = 0; j < e->shipdesigns_num; ++j) {
            ships += t[j];
        }
        tbl_ownorbit[i] = ships;
    }
    for (int i = 0; i < num_to_scout; ++i) {
        uint8_t mini, pli;
        int mindist;
        const planet_t *p;
        mindist = 10000;
        mini = PLANET_NONE;
        pli = tbl_planet_scout[i];
        p = &(g->planet[pli]);
        for (int j = 0; j < g->galaxy_stars; ++j) {
            if (tbl_ownorbit[j] > 0) {
                const planet_t *p2 = &(g->planet[j]);
                int dist;
                dist = util_math_dist_fast(p->x, p->y, p2->x, p2->y);
                if (dist < mindist) {
                    mindist = dist;
                    mini = j;
                }
            }
        }
        if (mini != PLANET_NONE) {
            if (g->enroute_num >= FLEET_ENROUTE_AI_MAX) {
                log_warning("fleet enroute table (size %i/%i) too large for AI fleet (%i)!\n", g->enroute_num, FLEET_ENROUTE_MAX, FLEET_ENROUTE_AI_MAX);
            } else {
                shipcount_t *t = &(e->orbit[mini].ships[0]);
                const bool *hrf = &(g->srd[pi].have_reserve_fuel[0]);
                int shipi;
                shipi = -1;
                for (int j = 0; j < e->shipdesigns_num; ++j) {
                    if ((t[j] != 0) && ((shipi == -1) || (!hrf[shipi]))) {
                        shipi = j;
                    }
                }
                if (!((p->within_frange[pi] == 1) || ((p->within_frange[pi] == 2) && hrf[shipi]))) {
                    shipi = -1;
                }
                if (shipi != -1) {
                    fleet_enroute_t *r;
                    const planet_t *p2 = &(g->planet[mini]);
                    shipcount_t num_send;
                    r = &(g->enroute[g->enroute_num++]);
                    num_send = t[shipi] / 4 + 1;
                    r->dest = pli;
                    r->owner = pi;
                    r->x = p2->x;
                    r->y = p2->y;
                    r->speed = g->srd[pi].design[shipi].engine + 1;
                    r->retreat = false;
                    for (int k = 0; k < NUM_SHIPDESIGNS; ++k) {
                        r->ships[k] = 0;
                    }
                    r->ships[shipi] = num_send;
                    BOOLVEC_CLEAR(r->visible, PLAYER_NUM);
                    BOOLVEC_SET1(r->visible, pi);
                    t[shipi] -= num_send;
                    tbl_ownorbit[mini] -= num_send;
                }
            }
        }
    }
}

static uint8_t game_ai_classic_turn_p1_front_find_planet(struct game_s *g, struct ai_turn_p1_s *ait, player_id_t pi, int x, int y)
{
    empiretechorbit_t *e = &(g->eto[pi]);
    uint8_t mini = 0;
    int mindist = 10000;
    for (int i = 0; i < g->galaxy_stars; ++i) {
        const planet_t *p = &(g->planet[i]);
        if (p->owner == pi) {
            int dist;
            dist = util_math_dist_fast(p->x, p->y, x, y);
            if (dist < mindist) {
                mindist = dist;
                mini = i;
            }
        }
    }
    mindist = 10000;
    for (int i = 0; i < g->galaxy_stars; ++i) {
        const planet_t *p = &(g->planet[i]);
        if (p->owner == pi) {
            shipsum_t defense;
            defense = p->missile_bases ? 1 : 0;
            for (int j = 0; (j < e->shipdesigns_num) && (defense == 0); ++j) {
                defense += e->orbit[i].ships[j];
            }
            if (defense == 1) {
                int dist;
                dist = util_math_dist_fast(p->x, p->y, x, y);
                if (dist < mindist) {
                    mindist = dist;
                    mini = i;
                }
            }
        }
    }
    return mini;
}

static void game_ai_classic_turn_p1_front(struct game_s *g, struct ai_turn_p1_s *ait, player_id_t pi)
{
    empiretechorbit_t *e = &(g->eto[pi]);
    int bestspeed;
    int tbl_x[PLAYER_NUM], tbl_y[PLAYER_NUM];
    bestspeed = game_ai_best_speed(g, pi) * 10 + 10;
    ait->num_fronts = 0;
    /* unused
    BOOLVEC_DECLARE(tbl_own_transport_dest, PLANETS_MAX);
    BOOLVEC_CLEAR(tbl_own_transport_dest, PLANETS_MAX);
    for (int i = 0; i < g->transport_num; ++i) {
        const transport_t *r = &(g->transport[j]);
        if (r->owner == pi) {
            BOOLVEC_SET1(tbl_own_transport_dest, r->dest);
        }
    }
    */
    for (player_id_t pi2 = PLAYER_0; pi2 < g->players; ++pi2) {
        if (1
          && (pi != pi2)
          && BOOLVEC_IS1(e->contact, pi2)
          && ((g->end == GAME_END_NONE) || BOOLVEC_IS1(g->refuse, pi2))
        ) {
            int v8, vc;
            ait->tbl_front_relation[ait->num_fronts] = 0;
            tbl_x[ait->num_fronts] = (ait->tbl_xcenter[pi2] * 4 + ait->tbl_xcenter[pi] * 6) / 10;
            tbl_y[ait->num_fronts] = (ait->tbl_ycenter[pi2] * 4 + ait->tbl_ycenter[pi] * 6) / 10;
            vc = 0;
            for (int i = 0; i < ait->num_fronts; ++i) {
                if (util_math_dist_fast(tbl_x[i], tbl_y[i], tbl_x[ait->num_fronts], tbl_y[ait->num_fronts]) <= bestspeed) {
                    vc = i + 1;
                }
            }
            v8 = e->relation1[pi2];
            SETMIN(v8, 0);
            if (vc == 0) {
                ait->tbl_front_relation[ait->num_fronts++] = v8;
            /* unused before overwritten below
            } else {
                ait->tbl_force_own[vc] += v8;
            */
            }
        }
    }
    if (ait->num_fronts == 0) {
        tbl_x[ait->num_fronts] = ait->tbl_xcenter[pi];
        tbl_y[ait->num_fronts] = ait->tbl_ycenter[pi];
        /* WASBUG: tbs_front_relation could be used uninitialized when there was no real front.
           In case of no real fronts, initialize the dummy front with a peaceful relation
        */
        ait->tbl_front_relation[ait->num_fronts++] = 80;
    }
    for (int i = 0; i < ait->num_fronts; ++i) {
        ait->tbl_front_planet[i] = game_ai_classic_turn_p1_front_find_planet(g, ait, pi, tbl_x[i], tbl_y[i]);
    }
    for (int k = 0; k < (PLAYER_NUM - 1); ++k) {
        for (int i = 0; i < ait->num_fronts;) {
            int m;
            m = -1;
            for (int j = i + 1; j < ait->num_fronts; ++j) {
                if (ait->tbl_front_planet[i] == ait->tbl_front_planet[j]) {
                    m = j;
                }
            }
            if (m != -1) {
                ait->tbl_front_relation[i] += ait->tbl_front_relation[m];
                for (int j = m; j < (ait->num_fronts - 1); ++j) {
                    ait->tbl_front_relation[j] = ait->tbl_front_relation[j + 1];
                    ait->tbl_front_planet[j] = ait->tbl_front_planet[j + 1];
                }
                --ait->num_fronts;
            } else {
                ++i;
            }
        }
    }
    /*7ff9d*/
    /* unused
    foreach shipdesign { tbl_shipweight[i] = game_num_tbl_hull_w[sd[i].hull]; }
    */
    {
        uint64_t sum = 0;
        for (int i = 0; i < g->galaxy_stars; ++i) {
            const planet_t *p = &(g->planet[i]);
            ait->tbl_force_own[i] = 0;
            if ((p->owner == pi) || (p->owner == PLAYER_NONE)) {
                for (int j = 0; j < e->shipdesigns_num; ++j) {
                    shipcount_t n;
                    n = e->orbit[i].ships[j];
                    if (n) {
                        uint64_t v;
                        v = ait->tbl_shipthreat[pi][j] * n;
                        sum += v;
                        if (g->ai_id == GAME_AI_CLASSIC) {
                            v &= 0xffffffff;    /* WASBUG MOO1 uses 32 bit var */
                        }
                        ait->tbl_force_own[i] += v;
                    }
                }
            }
        }
        if (g->ai_id == GAME_AI_CLASSIC) {
            sum &= 0xffffffff;  /* WASBUG MOO1 uses 32 bit var */
        }
        if (sum != 0) {
            for (int i = 0; i < ait->num_fronts; ++i) {
                ait->tbl_front_relation[i] += (ait->tbl_force_own[ait->tbl_front_planet[i]] * 100) / sum;
            }
        }
        ait->force_own_sum = sum / 25;
    }
}

static bool game_ai_classic_turn_p1_have_colony_ship(struct game_s *g, struct ai_turn_p1_s *ait, player_id_t pi)
{
    empiretechorbit_t *e = &(g->eto[pi]);
    shipresearch_t *srd = &(g->srd[pi]);
    int shipi = e->shipi_colony;
    uint32_t prod;
    uint8_t planeti;
    shipcount_t shipn;
    if (0
      || (ait->num_fronts == 0) /* never true? */
      || (shipi == -1)
      || (e->total_production_bc == 0)
    ) {
        return false;
    }
    shipn = srd->shipcount[shipi];
    if (!game_num_ai_4_colony_curse_fix && (shipn > 3)) {
        return false;
    }
    planeti = ait->tbl_front_planet[rnd_0_nm1(ait->num_fronts, &g->seed)];
    if (planeti == PLANET_NONE) { /* never true? */
        return false;
    }
    if (shipn > 3) {
        return true;
    }
    prod = (e->total_production_bc * 2) / 5;
    SETRANGE(prod, 1, 500);
    if (g->difficulty < DIFFICULTY_AVERAGE) {
        if (rnd_0_nm1(6, &g->seed) > g->difficulty) {
            prod = 0;
        }
    }
    if ((!ait->have_colonizable) || (rnd_1_n(500, &g->seed) > prod)) {
        return (shipn > 0);
    }
    /* spawn a new colony ship by magic */
    srd->shipcount[shipi] = ++shipn;
    ++e->orbit[planeti].ships[shipi];
    return true;
}

static void game_turn_fleet_send(struct game_s *g, struct ai_turn_p1_s *ait, player_id_t pi, uint8_t from, uint8_t dest)
{
    empiretechorbit_t *e = &(g->eto[pi]);
    fleet_enroute_t *r;
    const planet_t *pf, *pt;
    fleet_orbit_t *o;
    uint8_t speed, num_shiptypes;
    if (from == dest) {
        return;
    }
    ait->tbl_force_own[from] = 0;
    if (g->enroute_num == FLEET_ENROUTE_MAX) {
        log_warning("fleet enroute table (size %i) full, could not leave orbit!\n", FLEET_ENROUTE_MAX);
        return;
    }
    pf = &(g->planet[from]);
    pt = &(g->planet[dest]);
    r = &(g->enroute[g->enroute_num]);
    o = &(e->orbit[from]);
    for (int i = 0; i < NUM_SHIPDESIGNS; ++i) {
        r->ships[i] = 0;
    }
    speed = 50;
    num_shiptypes = 0;
    for (int i = 0; i < e->shipdesigns_num; ++i) {
        int n;
        n = o->ships[i];
        if ((pt->within_frange[pi] == 0) || ((pt->within_frange[pi] == 2) && !g->srd[pi].have_reserve_fuel[i])) {
            n = 0;
        }
        if (n > 0) {
            uint8_t s;
            r->ships[i] = n;
            if (g->ai_id == GAME_AI_CLASSICPLUS) {
                o->ships[i] = 0;
            }
            ++num_shiptypes;
            s = g->srd[pi].design[i].engine;
            SETMIN(speed, s);
        }
    }
    if ((pt->owner == pf->owner) && pt->have_stargate && pf->have_stargate) {
        speed = 100;
    }
    if (num_shiptypes > 0) {
        r->speed = speed + 1;
        BOOLVEC_CLEAR(r->visible, PLAYER_NUM);
        BOOLVEC_SET1(r->visible, pi);
        r->dest = dest;
        r->owner = pi;
        r->x = pf->x;
        r->y = pf->y;
        ++g->enroute_num;
    }
    {
        bool all_sent = true;
        if (g->ai_id == GAME_AI_CLASSICPLUS) {
            for (int i = 0; i < NUM_SHIPDESIGNS; ++i) {
                if (o->ships[i]) {
                    all_sent = false;
                    break;
                }
            }
        } else {
            for (int i = 0; i < NUM_SHIPDESIGNS; ++i) {
                o->ships[i] = 0;    /* BUG ships removed even if they were not sent due to range == 2 && !reserve_fuel */
            }
        }
        if (all_sent) {
            BOOLVEC_CLEAR(o->visible, NUM_SHIPDESIGNS);
        }
    }
}

static void game_ai_classic_turn_p1_send_colony_ships(struct game_s *g, struct ai_turn_p1_s *ait, player_id_t pi)
{
    empiretechorbit_t *e = &(g->eto[pi]);
    planet_type_t can_colonize = PLANET_TYPE_MINIMAL;
    int range = e->fuel_range * 15, si = e->shipi_colony, num_planet_colonize = 0;
    uint8_t tbl_planet_colonize[PLANETS_MAX];
    shipcount_t tbl_orbit[PLANETS_MAX];
    BOOLVEC_DECLARE(tbl_planet_ignore, PLANETS_MAX);

    for (int i = 0; i < g->galaxy_stars; ++i) {
        const planet_t *p = &(g->planet[i]);
        BOOLVEC_SET(tbl_planet_ignore, i, (p->owner != PLAYER_NONE) || (p->within_frange[pi] == 0));
    }
    for (int i = 0; i < g->enroute_num; ++i) {
        const fleet_enroute_t *r = &(g->enroute[i]);
        if ((r->owner == pi) && (r->ships[si] > 0)) {
            BOOLVEC_SET1(tbl_planet_ignore, r->dest);
        }
    }
    {
        const shipdesign_t *sd = &(g->srd[pi].design[si]);
        for (int i = 0; i < SPECIAL_SLOT_NUM; ++i) {
            ship_special_t s;
            s = sd->special[i];
            if ((s >= SHIP_SPECIAL_STANDARD_COLONY_BASE) && (s <= SHIP_SPECIAL_RADIATED_COLONY_BASE)) {
                can_colonize = PLANET_TYPE_MINIMAL - (s - SHIP_SPECIAL_STANDARD_COLONY_BASE);
            }
        }
    }
    if (e->race == RACE_SILICOID) {
        can_colonize = PLANET_TYPE_RADIATED;
    }
    for (int i = 0; i < g->galaxy_stars; ++i) {
        const planet_t *p = &(g->planet[i]);
        if (1
          && (p->owner == PLAYER_NONE)
          && BOOLVEC_IS0(tbl_planet_ignore, i)
          && ((g->evn.planet_orion_i != i) || (!g->evn.have_guardian))
          && (p->type >= can_colonize)
        ) {
            tbl_planet_colonize[num_planet_colonize++] = i;
        }
    }
    for (int i = 0; i < g->galaxy_stars; ++i) {
        tbl_orbit[i] = e->orbit[i].ships[si];
    }
    for (int i = 0; i < num_planet_colonize; ++i) {
        const planet_t *p;
        uint8_t pli, mini;
        int mindist;
        pli = tbl_planet_colonize[i];
        p = &(g->planet[pli]);
        mini = PLANET_NONE;
        if ((p->within_frange[pi] == 1) || ((p->within_frange[pi] == 2) && g->srd[pi].have_reserve_fuel[si])) {
            mindist = 10000;
            for (int j = 0; j < g->galaxy_stars; ++j) {
                if (tbl_orbit[j] > 0) {
                    const planet_t *p2 = &(g->planet[j]);
                    int dist;
                    dist = util_math_dist_fast(p->x, p->y, p2->x, p2->y);
                    if (dist < mindist) {
                        mindist = dist;
                        mini = j;
                    }
                }
            }
        }
        if (mini != PLANET_NONE) {
            const planet_t *p2;
            p2 = &(g->planet[mini]);
            if ((g->ai_id == GAME_AI_CLASSICPLUS) || (util_math_dist_fast(p->x, p->y, p2->x, p2->y) <= range)) {
                if (g->enroute_num >= FLEET_ENROUTE_AI_MAX) {
                    log_warning("fleet enroute table (size %i/%i) too large for AI fleet (%i)!\n", g->enroute_num, FLEET_ENROUTE_MAX, FLEET_ENROUTE_AI_MAX);
                } else {
                    game_turn_fleet_send(g, ait, pi, mini, pli);
                    if (g->ai_id == GAME_AI_CLASSICPLUS) {
                        /* WASBUG MOO1 does not update the local copy and may try to send multiple
                           colony ship fleets from the same planet */
                        tbl_orbit[mini] = e->orbit[mini].ships[si];
                    }
                }
            }
        }
    }
}

static void game_ai_classic_turn_p1_planet_w(struct game_s *g, struct ai_turn_p1_s *ait, player_id_t pi)
{
    ait->planet_en_num = 0;
    ait->planet_own_num = 0;
    for (int i = 0; i < g->galaxy_stars; ++i) {
        const planet_t *p = &(g->planet[i]);
        player_id_t owner;
        int v4;
        v4 = (p->special - 2) * 20 + p->special * 10 + 20;
        owner = p->owner;
        if (owner == PLAYER_NONE) {
            for (player_id_t pi2 = PLAYER_0; (pi2 < PLAYER_NUM) && (owner == PLAYER_NONE); ++pi2) {
                const empiretechorbit_t *e2 = &(g->eto[pi2]);
                if (pi2 != pi) {
                    for (int j = 0; (j < e2->shipdesigns_num) && (owner == PLAYER_NONE); ++j) {
                        if (e2->orbit[i].ships[j]) {
                            owner = pi2;
                        }
                    }
                }
            }
        }
        if (owner == pi) {
            ait->tbl_planet_own_i[ait->planet_own_num] = i;
            ait->tbl_planet_own_w[ait->planet_own_num] = p->pop - (p->missile_bases * 5) + v4;
            ++ait->planet_own_num;
        } else if (owner != PLAYER_NONE) {
            empiretechorbit_t *e = &(g->eto[pi]);
            if (1
              && ((e->treaty[owner] >= TREATY_WAR) || ait->need_conquer || (g->end != GAME_END_NONE))
              && (p->within_frange[pi] == 1) && (p->type >= e->have_colony_for)
              && ((g->year > 120) || (g->evn.planet_orion_i != i))
            ) {
                ait->tbl_planet_en_i[ait->planet_en_num] = i;
                ait->tbl_planet_en_w[ait->planet_en_num] = p->pop - (p->missile_bases * 10) + v4;
                ++ait->planet_en_num;
            }
        }
    }
    {
        bool work_left = true;
        for (int i = 0; (i < ait->planet_en_num) && work_left; ++i) {
            for (int j = 0; j < (ait->planet_en_num - 1); ++j) {
                work_left = false;
                if (ait->tbl_planet_en_w[j] < ait->tbl_planet_en_w[j + 1]) {
                    { int t; t = ait->tbl_planet_en_w[j]; ait->tbl_planet_en_w[j] = ait->tbl_planet_en_w[j + 1]; ait->tbl_planet_en_w[j + 1] = t; }
                    { uint8_t t; t = ait->tbl_planet_en_i[j]; ait->tbl_planet_en_i[j] = ait->tbl_planet_en_i[j + 1]; ait->tbl_planet_en_i[j + 1] = t; }
                    work_left = true;
                }
            }
        }
    }
}

static void game_ai_classic_turn_p1_send_attack(struct game_s *g, struct ai_turn_p1_s *ait, player_id_t pi)
{
    empiretechorbit_t *e = &(g->eto[pi]);
    int range = e->fuel_range * 15;
    for (int i = 0; i < ait->num_fronts; ++i) {
        uint8_t pfrom, pto, pto2;
        pto2 = PLANET_NONE;
        pfrom = ait->tbl_front_planet[i];
        for (int j = 0; (j < ait->planet_en_num) && (pto2 == PLANET_NONE); ++j) {
            const planet_t *pt;
            pto = ait->tbl_planet_en_i[j];
            pt = &(g->planet[pto]);
            if (1
              && (ait->tbl_planet_en_w[j] != -1000)
              && (util_math_dist_fast(pt->x, pt->y, g->planet[pfrom].x, g->planet[pfrom].y) < range)
              && ((rnd_1_n(100, &g->seed) < 40) || (ait->planet_en_num < 2))
            ) {
                empiretechorbit_t *e2;
                uint64_t weight;
                weight = 0;
                for (player_id_t pi2 = PLAYER_0; pi2 < g->players; ++pi2) {
                    if (pi2 != pi) {
                        e2 = &(g->eto[pi2]);
                        for (int l = 0; l < e2->shipdesigns_num; ++l) { /* WASBUG MOO1 uses e->shipdesigns_num */
                            shipcount_t n;
                            if ((n = e2->orbit[pto].ships[l]) != 0) {
                                weight += n * ait->tbl_shipthreat[pi2][l];
                            }
                        }
                    }
                }
                if (pt->owner != PLAYER_NONE) {
                    e2 = &(g->eto[pt->owner]);
                    weight += ((e2->tech.percent[TECH_FIELD_WEAPON] * 5) + (e2->have_planet_shield + 10) * 10) * pt->missile_bases;
                }
                if (g->ai_id == GAME_AI_CLASSIC) {
                    weight &= 0xffffffff;   /* WASBUG MOO1 uses 32 bit var */
                }
                if (ait->tbl_force_own[pfrom] > weight) {
                    pto2 = pto;
                    if (pt->owner == PLAYER_NONE) {
                        game_turn_fleet_send(g, ait, pi, pfrom, pto2);
                    } else if (e2->treaty[pi] == TREATY_ALLIANCE) {
                        if ((rnd_1_n(4, &g->seed) == 1) || ((rnd_1_n(2, &g->seed) == 1) && (e->trait2 == TRAIT2_EXPANSIONIST))) {
                            game_diplo_act(g, -10000, pt->owner, pi, 32, pto2, pto2); /* BUG? 2 * pto2?? */
                            game_diplo_break_treaty(g, pi, pt->owner);
                            if (e->relation1[pt->owner] > 30) {
                                e->relation1[pt->owner] = 30;
                                e2->relation1[pi] = 30;
                            }
                        }
                    } else if (e2->treaty[pi] == TREATY_NONAGGRESSION) {
                        if ((rnd_1_n(2, &g->seed) == 1) || (e->trait2 == TRAIT2_EXPANSIONIST)) {
                            game_diplo_act(g, -10000, pt->owner, pi, 32, pto2, pto2); /* BUG? 2 * pto2?? */
                            game_diplo_break_treaty(g, pi, pt->owner);
                            if (e->relation1[pt->owner] > 30) { /* BUG? should be 20? */
                                e->relation1[pt->owner] = 20;
                                e2->relation1[pi] = 20;
                            }
                        }
                    } else {
                        game_turn_fleet_send(g, ait, pi, pfrom, pto2);
                    }
                    ait->tbl_planet_en_w[j] = -1000;
                }
            }
        }
    }
    for (uint8_t pfrom = 0; pfrom < g->galaxy_stars; ++pfrom) {
        const planet_t *p = &(g->planet[pfrom]);
        if ((p->owner != PLAYER_NONE) && (p->owner != pi) && (ait->tbl_force_own[pfrom] != 0)) {
            for (int j = 0; j < ait->planet_en_num; ++j) {
                if (ait->tbl_planet_en_w[j] > -1000) {
                    const planet_t *pt;
                    uint8_t pto;
                    pto = ait->tbl_planet_en_i[j];
                    pt = &(g->planet[pto]);
                    if (1
                      && (util_math_dist_fast(pt->x, pt->y, g->planet[pfrom].x, g->planet[pfrom].y) < range)
                      && (rnd_1_n(100, &g->seed) < 60)
                    ) {
                        uint32_t weight;
                        weight = 0;
                        for (player_id_t pi2 = PLAYER_0; pi2 < g->players; ++pi2) {
                            if (pi2 != pi) {
                                const empiretechorbit_t *e2;
                                const shipdesign_t *sd;
                                e2 = &(g->eto[pi2]);
                                sd = &(g->srd[pi2].design[0]);
                                for (int k = 0; k < e2->shipdesigns_num; ++k) {
                                    weight += e2->orbit[pto].ships[k] * game_num_tbl_hull_w[sd[k].hull]; /* FIXME? why not tbl_shipthreat? */
                                }
                            }
                        }
                        if (((ait->tbl_force_own[pfrom] * 3) / 2) > weight) {
                            game_turn_fleet_send(g, ait, pi, pfrom, pto);
                            ait->tbl_planet_en_w[j] = -1000;
                            break;
                        }
                    }
                }
            }
        }
    }
}

static void game_ai_classic_turn_p1_send_defend(struct game_s *g, struct ai_turn_p1_s *ait, player_id_t pi)
{
    int bestspeed = game_ai_best_speed(g, pi) * 30 + 20;
    uint8_t tbl_shipw[PLAYER_NUM][NUM_SHIPDESIGNS];
    uint64_t tbl_planet_threat[PLANETS_MAX];
    uint8_t tbl_defend[PLANETS_MAX];
    int num_defend = 0;
    for (player_id_t pi2 = PLAYER_0; pi2 < g->players; ++pi2) {
        const empiretechorbit_t *e2 = &(g->eto[pi2]);
        const shipdesign_t *sd = &(g->srd[pi2].design[0]);
        for (int i = 0; i < e2->shipdesigns_num; ++i) {
            tbl_shipw[pi2][i] = game_num_tbl_hull_w[sd[i].hull];
        }
    }
    for (int i = 0; i < g->galaxy_stars; ++i) {
        tbl_planet_threat[i] = 0;
    }
    for (int i = 0; i < g->enroute_num; ++i) {
        const fleet_enroute_t *r = &(g->enroute[i]);
        if ((r->owner != pi) && (g->planet[r->dest].owner == pi)) {
            const empiretechorbit_t *e2 = &(g->eto[r->owner]);
            for (int j = 0; j < e2->shipdesigns_num; ++j) {
                shipcount_t n;
                n = r->ships[j];
                if (n != 0) {
                    tbl_planet_threat[r->dest] += n * tbl_shipw[r->owner][j];
                }
            }
        }
    }
    for (int i = 0; i < g->transport_num; ++i) {
        const transport_t *r = &(g->transport[i]);
        if ((r->owner != pi) && (g->planet[r->dest].owner == pi)) {
            tbl_planet_threat[r->dest] += r->pop * game_num_tbl_hull_w[SHIP_HULL_MEDIUM];
        }
    }
    for (int i = 0; i < g->galaxy_stars; ++i) {
        const planet_t *p = &(g->planet[i]);
        if ((p->missile_bases < 5) && (ait->tbl_force_own[i] == 0)) {
            tbl_planet_threat[i] = 50;
        }
    }
    if (g->ai_id == GAME_AI_CLASSIC) {
        for (int i = 0; i < g->galaxy_stars; ++i) {
            tbl_planet_threat[i] &= 0xffffffff; /* WASBUG MOO1 uses 32 bit var */
        }
    }
    for (int i = 0; i < g->galaxy_stars; ++i) {
        const planet_t *p = &(g->planet[i]);
        if ((p->owner == pi) && (((ait->tbl_force_own[i] * 4) / 3) < tbl_planet_threat[i])) {
            tbl_defend[num_defend++] = i;
        }
    }
    for (int i = 0; i < g->galaxy_stars; ++i) {
        if ((ait->tbl_force_own[i] > 0) && (tbl_planet_threat[i] == 0)) {
            const planet_t *pf;
            uint8_t pto;
            pto = PLANET_NONE;
            pf = &(g->planet[i]);
            for (int j = 0; (j < num_defend) && (pto == PLANET_NONE); ++j) {
                const planet_t *pt;
                uint8_t pli = tbl_defend[j];
                pt = &(g->planet[pli]);
                if (util_math_dist_fast(pf->x, pf->y, pt->x, pt->y) <= bestspeed) {
                    pto = pli;
                }
            }
            if ((pto != PLANET_NONE) && ((tbl_planet_threat[pto] * 5) / 3) < ait->tbl_force_own[i]) {
                game_turn_fleet_send(g, ait, pi, i, pto);
                tbl_planet_threat[pto] = 100000000;
            }
        }
    }
}

static void game_ai_classic_turn_p1_send_idle(struct game_s *g, struct ai_turn_p1_s *ait, player_id_t pi)
{
    int bestspeed = game_ai_best_speed(g, pi) * 20 + 20;
    for (int i = 0; i < ait->num_fronts; ++i) {
        ait->tbl_force_own[ait->tbl_front_planet[i]] = 0;
    }
    for (int i = 0; i < ait->planet_own_num; ++i) {
        if ((ait->tbl_force_own[ait->tbl_planet_own_i[i]] - ait->tbl_planet_own_w[i]) < 0) {
            ait->tbl_force_own[i] = 0;
        }
    }
    if (g->election_held) {
        ait->force_own_sum /= 2;
    }
    if (g->end != GAME_END_NONE) {
        ait->force_own_sum /= 2;
    }
    for (int i = 0; i < g->galaxy_stars; ++i) {
        if (ait->tbl_force_own[i] < ait->force_own_sum) {
            ait->tbl_force_own[i] = 0;
        }
    }
    for (int i = 0; i < g->galaxy_stars; ++i) {
        if (ait->tbl_force_own[i] > 0) {
            uint8_t pto;
            const planet_t *pf;
            int minv;
            pf = &(g->planet[i]);
            pto = PLANET_NONE;
            minv = 10000;
            for (int j = 0; j < ait->num_fronts; ++j) {
                const planet_t *pt;
                int v;
                uint8_t pli;
                pli = ait->tbl_front_planet[j];
                pt = &(g->planet[pli]);
                v = (util_math_dist_fast(pt->x, pt->y, pf->x, pf->y) * 10) / bestspeed + ait->tbl_front_relation[j];
                if (v < minv) {
                    minv = v;
                    pto = pli;
                }
            }
            if (pto != PLANET_NONE) {
                game_turn_fleet_send(g, ait, pi, i, pto);
            }
        }
    }
}

static void game_ai_classic_turn_p1_trans_en(struct game_s *g, struct ai_turn_p1_s *ait, player_id_t pi)
{
    int bestspeed = game_ai_best_speed(g, pi) * 30 + 30;
    empiretechorbit_t *e = &(g->eto[pi]);
    BOOLVEC_DECLARE(tbl_trans_from, PLANETS_MAX); /* NOTE overwrites ait->tbl_planet_own_i */
    BOOLVEC_DECLARE(tbl_trans_to, PLANETS_MAX);
    for (int i = 0; i < g->galaxy_stars; ++i) {
        const planet_t *p = &(g->planet[i]);
        bool have_orbit;
        have_orbit = false;
        if (1
          && (p->owner != PLAYER_NONE) && (p->owner != pi)
          && (IS_AI(g, p->owner) || (g->evn.ceasefire[p->owner][pi] <= 0))
          && (e->treaty[p->owner] != TREATY_ALLIANCE)
          && (e->have_colony_for <= p->type)
          && ((!game_num_ai_trans_range_fix) || (p->within_frange[pi] == 1))
        ) {
            const shipcount_t *s;
            s = &(e->orbit[i].ships[0]);
            for (int j = 0; j < e->shipdesigns_num; ++j) {
                if (s[j]) {
                    have_orbit = true;
                    break;
                }
            }
        }
        BOOLVEC_SET(tbl_trans_to, i, have_orbit);
        BOOLVEC_SET(tbl_trans_from, i, (p->owner == pi) && (p->pop >= (p->max_pop3 / 2)) && (p->pop > 20)
          && ((g->ai_id == GAME_AI_CLASSIC) || (p->unrest != PLANET_UNREST_REBELLION)));
    }
    for (int i = 0; i < g->transport_num; ++i) {
        const transport_t *r = &(g->transport[i]);
        if (r->owner == pi) {
            BOOLVEC_SET0(tbl_trans_to, r->dest);
        }
    }
    for (int i = 0; i < g->galaxy_stars; ++i) {
        if (BOOLVEC_IS1(tbl_trans_to, i)) {
            const planet_t *pt;
            planet_t *pf;
            uint8_t pfrom;
            int mindist, dist;
            mindist = 10000;
            pfrom = PLANET_NONE;
            pt = &(g->planet[i]);
            for (int j = 0; j < g->galaxy_stars; ++j) {
                if (BOOLVEC_IS1(tbl_trans_from, j)) {
                    pf = &(g->planet[j]);
                    if ((dist = util_math_dist_fast(pt->x, pt->y, pf->x, pf->y)) < mindist) {
                        mindist = dist;
                        pfrom = j;
                    }
                }
            }
            if ((pfrom != PLANET_NONE) && (mindist < bestspeed)) {
                pf = &(g->planet[pfrom]);
                pf->trans_num = pf->pop / 2;
                pf->trans_dest = i;
                BOOLVEC_SET0(tbl_trans_from, pfrom);
            }
        }
    }
}

static void game_ai_classic_turn_p1_trans_own(struct game_s *g, struct ai_turn_p1_s *ait, player_id_t pi)
{
    int bestspeed = game_ai_best_speed(g, pi) * 20 + 20;
    BOOLVEC_DECLARE(tbl_trans_from, PLANETS_MAX);
    BOOLVEC_DECLARE(tbl_trans_to, PLANETS_MAX);
    for (int i = 0; i < g->galaxy_stars; ++i) {
        const planet_t *p = &(g->planet[i]);
        BOOLVEC_SET0(tbl_trans_to, i);
        BOOLVEC_SET0(tbl_trans_from, i);
        if (p->owner == pi) {
            if ((p->pop < (p->max_pop3 / 3)) || (p->unrest == PLANET_UNREST_REBELLION)) {
                BOOLVEC_SET1(tbl_trans_to, i);
            }
            if (p->pop > ((p->max_pop3 * 3) / 4) && ((g->ai_id == GAME_AI_CLASSIC) || (p->unrest != PLANET_UNREST_REBELLION))) {
                BOOLVEC_SET1(tbl_trans_from, i);
            }
        }
    }
    for (int i = 0; i < g->transport_num; ++i) {
        const transport_t *r = &(g->transport[i]);
        if (r->owner == pi) {
            BOOLVEC_SET0(tbl_trans_to, r->dest);
        }
    }
    for (int i = 0; i < g->galaxy_stars; ++i) {
        if (BOOLVEC_IS1(tbl_trans_to, i)) {
            const planet_t *pt;
            planet_t *pf;
            uint8_t pfrom;
            pfrom = PLANET_NONE;
            pt = &(g->planet[i]);
            for (int j = 0; (j < g->galaxy_stars) && (pfrom == PLANET_NONE); ++j) {
                if (BOOLVEC_IS1(tbl_trans_from, j)) {
                    pf = &(g->planet[j]);
                    if (util_math_dist_fast(pt->x, pt->y, pf->x, pf->y) < bestspeed) {
                        pfrom = j;
                        pf->trans_num = pf->pop / 3;
                        pf->trans_dest = i;
                        BOOLVEC_SET0(tbl_trans_from, j);
                    }
                }
            }
        }
    }
}

static void game_ai_classic_turn_p1_build_defending_ships(struct game_s *g, player_id_t pi)
{
    empiretechorbit_t *e = &(g->eto[pi]);
    BOOLVEC_DECLARE(tbl_incoming, PLANETS_MAX);
    BOOLVEC_CLEAR(tbl_incoming, PLANETS_MAX);
    for (int i = 0; i < g->enroute_num; ++i) {
        const fleet_enroute_t *r = &(g->enroute[i]);
        if (1
          && (r->owner != pi)
          && ((g->ai_id == GAME_AI_CLASSIC) || (e->treaty[r->owner] != TREATY_ALLIANCE))
          && (g->planet[r->dest].owner == pi)
        ) {
            BOOLVEC_SET1(tbl_incoming, r->dest);
        }
    }
    for (int i = 0; i < g->galaxy_stars; ++i) {
        if (BOOLVEC_IS1(tbl_incoming, i)) {
            uint32_t v;
            planet_t *p;
            p = &(g->planet[i]);
            p->slider[PLANET_SLIDER_SHIP] += p->slider[PLANET_SLIDER_DEF] + p->slider[PLANET_SLIDER_IND] + p->slider[PLANET_SLIDER_TECH];
            p->slider[PLANET_SLIDER_DEF] = 0;
            p->slider[PLANET_SLIDER_IND] = 0;
            p->slider[PLANET_SLIDER_TECH] = 0;
            v = e->reserve_bc / 5;
            e->reserve_bc -= v;
            p->reserve += v;
        }
    }
}

static void game_ai_classic_turn_p1_fund_developing(struct game_s *g, player_id_t pi)
{
    empiretechorbit_t *e = &(g->eto[pi]);
    for (int i = 0; i < g->galaxy_stars; ++i) {
        planet_t *p = &(g->planet[i]);
        if ((p->owner == pi) && (p->pop < 20)) {
            uint32_t v;
            v = e->reserve_bc / 5;
            e->reserve_bc -= v;
            p->reserve += v;
        }
    }
}

static void game_ai_classic_turn_p1_tax(struct game_s *g, player_id_t pi)
{
    g->eto[pi].tax = (g->year >= 20) ? (rnd_1_n(10, &g->seed) + g->difficulty + 2) : 0;
}

static void game_ai_classic_turn_p1(struct game_s *g)
{
    struct ai_turn_p1_s ait[1];
    for (player_id_t pi = PLAYER_0; pi < PLAYER_NUM; ++pi) {
        const empiretechorbit_t *e = &(g->eto[pi]);
        const shipdesign_t *sd = &(g->srd[pi].design[0]);
        for (int i = 0; i < e->shipdesigns_num; ++i) {
            uint32_t v;
            v = 0;
            for (int j = 0; j < WEAPON_SLOT_NUM; ++j) {
                weapon_t wt;
                wt = sd[i].wpnt[j];
                if (!tbl_shiptech_weap[wt].is_bomb) {
                    v += tbl_shiptech_weap[wt].tech_i * sd[i].wpnn[j];
                }
            }
            if (v != 0) {
                v += ((sd[i].shield + 10) * sd[i].hp) / 50;
            }
            for (int j = 0; j < SPECIAL_SLOT_NUM; ++j) {
                ship_special_t st;
                st = sd[i].special[j];
                if (st >= SHIP_SPECIAL_BATTLE_SCANNER) {
                    v += tbl_shiptech_special[st].tech_i * 2;
                }
            }
            if (IS_AI(g, pi)) {
                if (g->election_held) {
                    v *= 2;
                }
                if (g->end != GAME_END_NONE) {
                    v *= 2;
                }
            }
            if (g->ai_id == GAME_AI_CLASSIC) {
                v &= 0xffff;    /* WASBUG the table variables are 16 bit in MOO1 */
            }
            ait->tbl_shipthreat[pi][i] = v;
        }
        for (int i = e->shipdesigns_num; i < NUM_SHIPDESIGNS; ++i) {
            ait->tbl_shipthreat[pi][i] = 0;
        }
    }
    for (int i = 0; i < NUM_SHIPDESIGNS; ++i) {
        ait->tbl_shipthreat[PLAYER_NUM][i] = 1800;
    }
    game_update_maint_costs(g);
    for (player_id_t pi = PLAYER_0; pi < g->players; ++pi) {
        if (IS_ALIVE(g, pi)) {
            int xsum, ysum, num_planets;
            /* BUG moved below to next loop as only the last player affected num_enroute
            int num_enroute;
            num_enroute = 0;
            for (int i = 0; i < g->enroute_num; ++i) {
                if (g->enroute[i].owner == pi) {
                    ++num_enroute;
                }
            }
            */
            xsum = 0;
            ysum = 0;
            num_planets = 0;
            for (int i = 0; i < g->galaxy_stars; ++i) {
                planet_t *p = &(g->planet[i]);
                if (p->owner == pi) {
                    xsum += p->x;
                    ysum += p->y;
                    ++num_planets;
                }
            }
            if (num_planets) {
                ait->tbl_xcenter[pi] = xsum / num_planets;
                ait->tbl_ycenter[pi] = ysum / num_planets;
            } else {
                ait->tbl_xcenter[pi] = 0;
                ait->tbl_ycenter[pi] = 0;
            }
        }
    }
    for (player_id_t pi = PLAYER_0; pi < g->players; ++pi) {
        bool flag_send_colony;
        if (IS_HUMAN(g, pi)) {
            continue;
        }
        flag_send_colony = true;
        if (g->eto[pi].trait2 != TRAIT2_EXPANSIONIST) {
            int num_planets, num_developing_planets;
            num_planets = 0;
            num_developing_planets = 0;
            for (int i = 0; i < g->galaxy_stars; ++i) {
                const planet_t *p = &(g->planet[i]);
                if (p->owner == pi) {
                    ++num_planets;
                    if ((p->missile_bases < (p->max_pop3 / 20)) && (p->pop < ((p->max_pop3 * 2) / 3))) {
                        ++num_developing_planets;
                    }
                }
            }
            if ((num_planets / 2) < num_developing_planets) {
                for (player_id_t pi2 = PLAYER_0; pi2 < g->players; ++pi2) {
                    if (IS_HUMAN(g, pi2) && BOOLVEC_IS1(g->eto[pi2].contact, pi)) {
                        flag_send_colony = false;
                        break;
                    }
                }
            }
        }
        game_ai_classic_turn_p1_send_scout(g, ait, pi);
        game_ai_classic_turn_p1_front(g, ait, pi);
        if (game_ai_classic_turn_p1_have_colony_ship(g, ait, pi)) {
            if (flag_send_colony) {
                game_ai_classic_turn_p1_send_colony_ships(g, ait, pi);
            }
        }
        game_ai_classic_turn_p1_front(g, ait, pi);
        game_ai_classic_turn_p1_planet_w(g, ait, pi);
        if (ait->planet_en_num != 0) {
            game_ai_classic_turn_p1_send_attack(g, ait, pi);
        }
        game_ai_classic_turn_p1_send_defend(g, ait, pi);
        /* WASBUG moved above to next loop as only the last player affected num_enroute */
        {
            int num_enroute;
            num_enroute = 0;
            for (int i = 0; i < g->enroute_num; ++i) {
                if (g->enroute[i].owner == pi) {
                    ++num_enroute;
                }
            }
            if (num_enroute < 8) {
                game_ai_classic_turn_p1_send_idle(g, ait, pi);
            }
        }
        game_ai_classic_turn_p1_trans_en(g, ait, pi);
        game_ai_classic_turn_p1_trans_own(g, ait, pi);
        game_ai_classic_turn_p1_build_defending_ships(g, pi);
        game_ai_classic_turn_p1_fund_developing(g, pi);
        game_ai_classic_turn_p1_tax(g, pi);
    }
}

/* -------------------------------------------------------------------------- */

static void game_ai_classic_design_scrap(struct game_s *g, player_id_t pi, int shipi)
{
    empiretechorbit_t *e = &(g->eto[pi]);
    int si;
    game_design_scrap(g, pi, shipi, false);
    si = e->shipi_colony;
    if (si == shipi) {
        si = -1;
    } else if (si > shipi) {
        --si;
    }
    e->shipi_colony = si;
    si = e->shipi_bomber;
    if (si == shipi) {
        si = -1;
    } else if (si > shipi) {
        --si;
    }
    e->shipi_bomber = si;
}

static uint8_t game_ai_classic_design_ship_get_look(struct game_s *g, player_id_t pi, ship_hull_t hull)
{
    empiretechorbit_t *e = &(g->eto[pi]);
    shipresearch_t *srd = &(g->srd[pi]);
    BOOLVEC_DECLARE(tbl_in_use, SHIP_LOOK_PER_HULL);
    BOOLVEC_CLEAR(tbl_in_use, SHIP_LOOK_PER_HULL);
    uint8_t look, lookmax, lookbase = SHIP_LOOK_PER_HULL * hull + e->banner * SHIP_LOOK_PER_BANNER;
    lookmax = lookbase + (SHIP_LOOK_PER_HULL - 1);
    for (int i = 0; i < e->shipdesigns_num; ++i) {
        look = srd->design[i].look;
        if ((look >= lookbase) && (look <= lookmax)) {
            BOOLVEC_SET1(tbl_in_use, look - lookbase);
        }
    }
    look = 0;
    while (BOOLVEC_IS1(tbl_in_use, look)) {
        ++look;
    }
    if (look > (SHIP_LOOK_PER_HULL - 1)) {
        look = 0;
    }
    return look + lookbase;
}

static int count_havebuf_items(const int8_t *tbl, int last)
{
    int num = 0;
    for (int i = 0; i < last; ++i) {
        if (tbl[i] > 0) {
            ++num;
        }
    }
    return num;
}

static int find_havebuf_item(const int8_t *tbl, int num)
{
    int i;
    for (i = 0; (num > 0); ++i) {
        if (tbl[i] > 0) {
            --num;
        }
    }
    return i ? (i - 1) : 0;
}

static int game_ai_classic_design_ship_get_item(struct game_s *g, int num, int chance)
{
    while ((rnd_1_n(100, &g->seed) > chance) && (num > 1)) {
        chance *= 2;
        --num;
    }
    return num;
}

static int game_ai_classic_design_update_engines_space(struct game_design_s *gd)
{
    shipdesign_t *sd = &(gd->sd);
    game_design_update_engines(sd);
    sd->space = game_design_calc_space(gd);
    return sd->space;
}

static void game_ai_classic_design_ship_base(struct game_s *g, struct ai_turn_p2_s *ait, player_id_t pi)
{
    int8_t tbl_have[SHIP_SPECIAL_NUM];  /* largest of the used */
    shipdesign_t *sd = &(ait->gd.sd);
    ship_hull_t hull = sd->hull;
    int space;

    const uint8_t tbl_chance_special[SHIP_SPECIAL_NUM][SHIP_HULL_NUM] = {
        { 0, 0, 0, 0 },
        { 0, 0, 0, 0 },
        { 0, 0, 0, 0 },
        { 0, 0, 0, 0 },
        { 0, 0, 0, 0 },
        { 0, 0, 0, 0 },
        { 0, 0, 0, 0 },
        { 0, 0, 0, 0 },
        { 0, 0, 0, 0 },
        { 0, 0, 30, 80 },
        { 0, 10, 50, 80 },
        { 0, 0, 25, 80 },
        { 0, 0, 20, 70 },
        { 0, 5, 20, 60 },
        { 20, 30, 50, 80 },
        { 0, 10, 40, 70 },
        { 0, 0, 25, 50 },
        { 0, 0, 20, 40 },
        { 20, 30, 40, 70 },
        { 0, 0, 20, 30 },
        { 10, 20, 25, 35 },
        { 0, 0, 20, 30 },
        { 0, 0, 20, 40 },
        { 10, 20, 30, 40 },
        { 0, 10, 30, 50 },
        { 0, 0, 30, 60 },
        { 0, 0, 0, 50 },
        { 0, 10, 20, 30 },
        { 10, 20, 40, 60 },
        { 10, 20, 40, 60 },
        { 10, 20, 40, 60 }
    };

    {
        int v;
        v = game_design_build_tbl_fit_engine(g, &ait->gd, tbl_have);
        v = count_havebuf_items(tbl_have, v);
        if (g->ai_id == GAME_AI_CLASSIC || v < 2) {
            v = game_ai_classic_design_ship_get_item(g, v, 60);
        } else {
            if (v > 2) {
                int best = find_havebuf_item(tbl_have, v);
                int next = find_havebuf_item(tbl_have, v - 1);
                if (next > 1 && best - next < 3)
                    v = game_ai_classic_design_ship_get_item(g, v, 45 + (best - next)*15);
            } /* else v = v; */
        }
        sd->engine = find_havebuf_item(tbl_have, v);
    }
    space = game_ai_classic_design_update_engines_space(&ait->gd) / 3;
    if (ait->shiptype == 0/*colony*/) {
        int v;
        v = game_design_build_tbl_fit_special(g, &ait->gd, tbl_have, 0);
        tbl_have[SHIP_SPECIAL_RESERVE_FUEL_TANKS] = 0;
        for (int i = SHIP_SPECIAL_BATTLE_SCANNER; i <= v; ++i) {
            tbl_have[i] = 0;
        }
        v = count_havebuf_items(tbl_have, v);
        if (v >= 2) {
            v = find_havebuf_item(tbl_have, v);
            if (v < SHIP_SPECIAL_STANDARD_COLONY_BASE) {
                LOG_DEBUG((1, "%s: BUG: invalid colony special %i\n", __func__, v));
                v = SHIP_SPECIAL_STANDARD_COLONY_BASE;
            }
        } else {
            v = SHIP_SPECIAL_STANDARD_COLONY_BASE;
        }
        sd->special[0] = v;
        if (ait->gd.percent[TECH_FIELD_CONSTRUCTION] > 5) {
            sd->special[1] = SHIP_SPECIAL_RESERVE_FUEL_TANKS;
        }
        /* BUG? no update_engines */
    }
    {
        const int tbl_chance[SHIP_HULL_NUM] = { 10, 20, 35, 50 };
        int v;
        v = game_design_build_tbl_fit_comp(g, &ait->gd, tbl_have);
        v = count_havebuf_items(tbl_have, v);
        v = game_ai_classic_design_ship_get_item(g, v, tbl_chance[hull]);
        sd->comp = find_havebuf_item(tbl_have, v);
    }
    if (game_ai_classic_design_update_engines_space(&ait->gd) < space) {
        return;
    }
    {
        const int tbl_chance[SHIP_HULL_NUM] = { 5, 15, 40, 70 };
        int v;
        v = game_design_build_tbl_fit_shield(g, &ait->gd, tbl_have);
        v = count_havebuf_items(tbl_have, v);
        v = game_ai_classic_design_ship_get_item(g, v, tbl_chance[hull]);
        sd->shield = find_havebuf_item(tbl_have, v);
    }
    if (game_ai_classic_design_update_engines_space(&ait->gd) < space) {
        return;
    }
    {
        const int tbl_chance[SHIP_HULL_NUM] = { 1, 2, 4, 8 };
        int v;
        v = game_design_build_tbl_fit_armor(g, &ait->gd, tbl_have);
        if ((rnd_1_n(100, &g->seed) <= tbl_chance[hull]) && ((v & 1) == 1)) {
            /* NOP */
        } else {
            v &= ~1;
        }
        sd->armor = v;
    }
    if (game_ai_classic_design_update_engines_space(&ait->gd) < space) {
        return;
    }
    if (ait->shiptype == 0/*colony*/) {
        return;
    }
    for (int si = 0; si <= 1; ++si) {
        int v;
        ship_special_t st;
        st = SHIP_SPECIAL_NONE;
        v = game_design_build_tbl_fit_special(g, &ait->gd, tbl_have, si);
        for (int i = SHIP_SPECIAL_RESERVE_FUEL_TANKS; i < SHIP_SPECIAL_BATTLE_SCANNER; ++i) {
            tbl_have[i] = 0;
        }
        for (int i = 0; i <= v; ++i) {
            if ((rnd_1_n(100, &g->seed) <= tbl_chance_special[i][hull]) && (tbl_have[i] > 0)) {
                st = i;
            }
        }
        sd->special[si] = st;
        if (game_ai_classic_design_update_engines_space(&ait->gd) < space) {
            return;
        }
    }
    {
        const int tbl_chance[SHIP_HULL_NUM] = { 30, 20, 20, 15 };
        int v;
        v = game_design_build_tbl_fit_man(g, &ait->gd, tbl_have);
        v = count_havebuf_items(tbl_have, v);
        v = game_ai_classic_design_ship_get_item(g, v, tbl_chance[hull]);
        sd->man = find_havebuf_item(tbl_have, v);
    }
    if (game_ai_classic_design_update_engines_space(&ait->gd) < space) {
        return;
    }
    {
        const int tbl_chance[SHIP_HULL_NUM] = { 2, 8, 20, 30 };
        int v;
        v = game_design_build_tbl_fit_jammer(g, &ait->gd, tbl_have);
        v = count_havebuf_items(tbl_have, v);
        v = game_ai_classic_design_ship_get_item(g, v, tbl_chance[hull]);
        sd->jammer = find_havebuf_item(tbl_have, v);
    }
    {
        int v;
        ship_special_t st;
        st = SHIP_SPECIAL_NONE;
        v = game_design_build_tbl_fit_special(g, &ait->gd, tbl_have, 2);
        for (int i = SHIP_SPECIAL_RESERVE_FUEL_TANKS; i < SHIP_SPECIAL_BATTLE_SCANNER; ++i) {
            tbl_have[i] = 0;
        }
        for (int i = 0; i <= v; ++i) {
            if ((rnd_1_n(100, &g->seed) <= tbl_chance_special[i][hull]) && (tbl_have[i] > 0)) {
                st = i;
            }
        }
        sd->special[2] = st;
    }
}

static void game_ai_classic_design_ship_sub2(struct game_s *g, struct ai_turn_p2_s *ait, player_id_t pi)
{
    int8_t tbl_have[SHIP_SPECIAL_NUM];
    shipdesign_t *sd = &(ait->gd.sd);
    ship_hull_t hull = sd->hull;
    if ((sd->special[2] != SHIP_SPECIAL_NONE) || (hull < SHIP_HULL_LARGE)) {
        return;
    }
    game_design_build_tbl_fit_special(g, &ait->gd, tbl_have, 2);
    if (tbl_have[SHIP_SPECIAL_BATTLE_SCANNER] > 0) {
        sd->special[2] = SHIP_SPECIAL_BATTLE_SCANNER;
    }
    if ((ait->gd.percent[TECH_FIELD_PROPULSION] < 30) || (sd->special[1] != SHIP_SPECIAL_NONE)) {
        return;
    }
    game_design_build_tbl_fit_special(g, &ait->gd, tbl_have, 2); /* BUG should be slot 1 */
    if (tbl_have[SHIP_SPECIAL_INERTIAL_STABILIZER] > 0) {
        sd->special[1] = SHIP_SPECIAL_INERTIAL_STABILIZER;
    }
}

static void game_ai_classic_design_ship_weapon(struct game_s *g, struct ai_turn_p2_s *ait, player_id_t pi, int sloti, int numshots_ignore, int c1, int c2)
{
    int8_t tbl_have[WEAPON_NUM];
    shipdesign_t *sd = &(ait->gd.sd);
    int v;
    v = game_design_build_tbl_fit_weapon(g, &ait->gd, tbl_have, sloti, WEAPON_GROUP_ALL);
    if ((sloti == 0) && tbl_shiptech_weap[v].is_bomb && (ait->shiptype != 2/*bomber*/)) {
        --v;
    }
    SETMAX(v, 1);
    for (int i = 0; i < v; ++i) { /* FIXME <= v ? */
        if (tbl_have[i] > 0) {
            if (0
              || (tbl_shiptech_weap[i].numshots == numshots_ignore)
              || (ait->have_repulwarp && (tbl_shiptech_weap[i].range == 1))
            ) {
                tbl_have[i] = 0;
            }
        }
    }
    for (int i = 0; i < sloti; ++i) {
        tbl_have[sd->wpnt[i]] = 0;
    }
    v = count_havebuf_items(tbl_have, v);
    if (v > 1) {
        int r;
        v = find_havebuf_item(tbl_have, v);
        sd->wpnt[sloti] = v;
        r = ((tbl_shiptech_weap[v].numfire > 0) || (tbl_shiptech_weap[v].nummiss > 0)) ? 41 : 11;
        sd->wpnn[sloti] = (tbl_have[v] * (rnd_1_n(c2 - c1 + r, &g->seed) + c1)) / 100;
    }
}

static void game_ai_classic_design_ship_weapons(struct game_s *g, struct ai_turn_p2_s *ait, player_id_t pi)
{
    int numshots_ignore, weapnum = 1;
    numshots_ignore = rnd_0_nm1(2, &g->seed) ? 2 : 5;
    {
        const shipdesign_t *sd = &(ait->gd.sd);
        ship_hull_t hull = sd->hull;
        const int tbl_chance_2[SHIP_HULL_NUM] = { 0, 50, 75, 100 };
        if (rnd_1_n(100, &g->seed) <= tbl_chance_2[hull]) {
            const int tbl_chance_3[SHIP_HULL_NUM] = { 0, 0, 50, 100 };
            weapnum = 2;
            if (rnd_1_n(100, &g->seed) <= tbl_chance_3[hull]) {
                const int tbl_chance_4[SHIP_HULL_NUM] = { 0, 0, 25, 50 };
                weapnum = 3;
                if (rnd_1_n(100, &g->seed) <= tbl_chance_4[hull]) {
                    weapnum = 4;
                }
            }
        }
    }
    for (int i = 0; i < weapnum; ++i) {
        const uint8_t tbl_c1[WEAPON_SLOT_NUM][WEAPON_SLOT_NUM] = {
            { 100, 100, 100, 100 },
            { 60, 100, 100, 100 },
            { 40, 40, 100, 100 },
            { 40, 40, 60, 100 }
        };
        const uint8_t tbl_c2[WEAPON_SLOT_NUM][WEAPON_SLOT_NUM] = {
            { 100, 100, 100, 100 },
            { 80, 100, 100, 100 },
            { 70, 70, 100, 100 },
            { 60, 60, 80, 100 }
        };
        game_ai_classic_design_ship_weapon(g, ait, pi, i, numshots_ignore, tbl_c1[weapnum - 1][i], tbl_c2[weapnum - 1][i]);
    }
}

static void game_ai_classic_design_ship(struct game_s *g, struct ai_turn_p2_s *ait, player_id_t pi)
{
    shipdesign_t *sd = &(ait->gd.sd);
    empiretechorbit_t *e = &(g->eto[pi]);
    bool temp_repulwarp = ait->have_repulwarp;
    int loops = 0;
again:
    if (++loops >= 100000) {
        log_fatal_and_die("BUG: %s looped %i times\n", __func__, loops);
    }
    if (ait->shiptype != 0/*colony*/) {
        const int8_t tbl_hulldiff[RACE_NUM] = { 0, 0, 3, 0, 0, -3, -3, 3, 3, 0 };
        const ship_hull_t tbl_hull[12] = { 0, 1, 1, 1, 2, 2, 2, 2, 2, 3, 3, 3 };
        int v;
        v = rnd_0_nm1(12, &g->seed) + tbl_hulldiff[e->race];
        SETMAX(v, 0);
        if (ait->have_pulsar) {
            ++v;
        }
        SETMIN(v, 11);
        ait->hull = tbl_hull[v];
    } else {
        ait->hull = SHIP_HULL_LARGE;
    }
    ait->shiplook = game_ai_classic_design_ship_get_look(g, pi, ait->hull);
    sd->wpnn[0] = 0;
    for (int loops2 = 0; (sd->wpnn[0] == 0) && (loops2 < 100000); ++loops2) {
        game_design_prepare_ai(g, &ait->gd, pi, ait->hull, ait->shiplook);
        game_ai_classic_design_ship_base(g, ait, pi);
        game_ai_classic_design_ship_sub2(g, ait, pi);
        game_ai_classic_design_ship_weapons(g, ait, pi);
        game_design_set_hp(sd);
        game_design_compact_slots(sd);
        if (ait->shiptype == 0/*colony*/) {
            lib_strcpy(sd->name, game_str_ai_colonyship, SHIP_NAME_LEN);
            if (sd->wpnn[0] == 0) {
                sd->wpnt[0] = WEAPON_LASER;
                sd->wpnn[0] = 1;
            }
        }
        sd->cost = game_design_calc_cost(&ait->gd);
    }
    if (sd->wpnn[0] == 0) {
        if (ait->have_repulwarp) {
            log_warning("%s: failure to design against repulsor/warpdis (hull %i)\n", __func__, ait->hull);
            if (ait->hull == SHIP_HULL_SMALL) {
                log_warning("%s: trying again\n", __func__);
            } else {
                ait->have_repulwarp = false;
                log_warning("%s: relaxing 1 rng limit\n", __func__);
            }
            goto again;
        } else {
            log_fatal_and_die("BUG: %s loops2\n", __func__);
        }
    }
    {
        bool flag_again, is_missile;
        weapon_t wt;
        flag_again = false;
        wt = sd->wpnt[0];
        is_missile = tbl_shiptech_weap[wt].damagemin == tbl_shiptech_weap[wt].damagemax;
        if (is_missile || tbl_shiptech_weap[wt].is_bomb) {
            for (int i = 0; i < SPECIAL_SLOT_NUM; ++i) {
                if (sd->special[i] == SHIP_SPECIAL_HIGH_ENERGY_FOCUS) {
                    flag_again = true;
                }
            }
        }
        if (is_missile) {
            for (int i = 0; i < SPECIAL_SLOT_NUM; ++i) {
                if (sd->special[i] == SHIP_SPECIAL_ORACLE_INTERFACE) {
                    flag_again = true;
                }
            }
        }
        if (flag_again) {
            goto again;
        }
    }
    game_design_add(g, pi, sd, false);
    ait->have_repulwarp = temp_repulwarp;
}

static void game_ai_classic_turn_p2_do(struct game_s *g, player_id_t pi)
{
    struct ai_turn_p2_s ait[1];
    empiretechorbit_t *e = &(g->eto[pi]);
    shipresearch_t *srd = &(g->srd[pi]);
    shipdesign_t *sd = &(srd->design[0]);
    int num_non0 = 0;
    ait->have_pulsar = false;
    ait->have_repulwarp = false;
    for (player_id_t p2 = PLAYER_0; p2 < g->players; ++p2) {
        const shipdesign_t *sd2;
        int sn;
        if (IS_AI(g, p2) || (!IS_ALIVE(g, p2))) {
            continue;
        }
        sn = g->eto[p2].shipdesigns_num;
        sd2 = &(g->srd[p2].design[0]);
        for (int i = 0; i < sn; ++i) {
            const ship_special_t *ss = &(sd2[i].special[0]);
            for (int j = 0; j < SPECIAL_SLOT_NUM; ++j) {
                ship_special_t s;
                s = ss[j];
                if ((s == SHIP_SPECIAL_ENERGY_PULSAR) || (s == SHIP_SPECIAL_IONIC_PULSAR)) {
                    ait->have_pulsar = true;
                }
                if ((s == SHIP_SPECIAL_REPULSOR_BEAM) || (s == SHIP_SPECIAL_WARP_DISSIPATOR)) {
                    ait->have_repulwarp = true;
                }
            }
        }
    }
    e->ai_p2_countdown = rnd_1_n(12, &g->seed) + 8;
    game_update_maint_costs(g);
    e->shipi_colony = -1;
    e->shipi_bomber = -1;
    for (int i = 0; i < e->shipdesigns_num; ++i) {
        ship_special_t *ss = &(sd[i].special[0]);
        if (srd->shipcount[i] != 0) {
            ++num_non0;
        }
        for (int j = 0; j < SPECIAL_SLOT_NUM; ++j) {
            ship_special_t s;
            s = ss[j];
            if ((s >= SHIP_SPECIAL_STANDARD_COLONY_BASE) && (s <= SHIP_SPECIAL_RADIATED_COLONY_BASE)) {
                e->shipi_colony = i;
            }
        }
        if (tbl_shiptech_weap[sd[i].wpnt[0]].is_bomb) {
            e->shipi_bomber = i;
        }
    }
    for (int i = 0; (i < e->shipdesigns_num) && (num_non0 > 1); ++i) {
        if (0
          || ((g->year < 100) && ((g->year - srd->year[i]) > 50))
          || ((g->year >= 100) && ((g->year - srd->year[i]) > 100))
        ) {
            game_ai_classic_design_scrap(g, pi, i);
            --num_non0; /* BUG this could be a non-non0 ship design */
        }
    }
    num_non0 = 0;
    for (int i = 0; i < e->shipdesigns_num; ++i) {
        if (srd->shipcount[i] != 0) {
            ++num_non0;
        }
    }
    {
        int shipi_remove = -1;
        if (num_non0 > 4) {
            int year_oldest = 0xffff;
            for (int i = 0; i < e->shipdesigns_num; ++i) {
                int sy;
                sy = srd->year[i];
                if (sy < year_oldest) {
                    year_oldest = sy;
                    shipi_remove = i;
                }
            }
        } else if (e->shipdesigns_num > 1) {
            for (int i = 0; i < e->shipdesigns_num; ++i) {
                if (srd->shipcount[i] == 0) {
                    shipi_remove = i;
                    break;
                }
            }
        }
        if (shipi_remove != -1) {
            game_ai_classic_design_scrap(g, pi, shipi_remove);
        }
    }
    for (int loops = 0; (e->shipdesigns_num < NUM_SHIPDESIGNS) && (loops < 10000); ++loops) {
        int si;
        si = e->shipdesigns_num;
        if (e->shipi_colony == -1) {
            e->shipi_colony = si;
            ait->shiptype = 0/*colony*/;
        } else if (e->shipi_bomber == -1) {
            e->shipi_bomber = si;
            ait->shiptype = 2/*bomber*/;
        } else {
            ait->shiptype = 1/*fighter*/;
        }
        game_ai_classic_design_ship(g, ait, pi);
    }
    if (e->shipdesigns_num < NUM_SHIPDESIGNS) {
        log_fatal_and_die("BUG: %s hang\n", __func__);
    }
    for (int i = 0; i < e->shipdesigns_num; ++i) {
        ship_special_t *ss = &(sd[i].special[0]);
        for (int j = 0; j < SPECIAL_SLOT_NUM; ++j) {
            ship_special_t s;
            s = ss[j];
            if ((s >= SHIP_SPECIAL_STANDARD_COLONY_BASE) && (s <= SHIP_SPECIAL_RADIATED_COLONY_BASE)) {
                e->shipi_colony = i;
            }
        }
        if (tbl_shiptech_weap[sd[i].wpnt[0]].is_bomb) {
            e->shipi_bomber = i;
        }
    }
    {
        uint8_t tbl_i[NUM_SHIPDESIGNS];
        uint16_t tbl_year[NUM_SHIPDESIGNS];
        for (int i = 0; i < NUM_SHIPDESIGNS; ++i) {
            tbl_i[i] = i;
            tbl_year[i] = srd->year[i];
            if (i == e->shipi_colony) {
                tbl_year[i] = 0;
            }
        }
        for (int j = 0; j < NUM_SHIPDESIGNS - 1; ++j) {
            for (int i = 0; i < NUM_SHIPDESIGNS - 1; ++i) {
                uint16_t y1, y;
                y1 = tbl_year[i + 1];
                y = tbl_year[i];
                if (y1 > y) {
                    uint8_t t;
                    tbl_year[i + 1] = y;
                    tbl_year[i] = y1;
                    t = tbl_i[i];
                    tbl_i[i] = tbl_i[i + 1];
                    tbl_i[i + 1] = t;
                }
            }
        }
        for (int i = 0; i < g->galaxy_stars; ++i) {
            planet_t *p = &(g->planet[i]);
            if (p->owner == pi) {
                uint8_t n;
                if ((!p->have_stargate) && (e->have_stargates) && (rnd_0_nm1(2, &g->seed))) {
                    n = BUILDSHIP_STARGATE;
                } else {
                    n = tbl_i[rnd_0_nm1(3, &g->seed)];
                }
                p->buildship = n;
            }
        }
    }
}

static void game_ai_classic_turn_p2(struct game_s *g)
{
    for (player_id_t pi = PLAYER_0; pi < g->players; ++pi) {
        if (IS_AI(g, pi) && IS_ALIVE(g, pi)) {
            if (--g->eto[pi].ai_p2_countdown <= 0) {
                game_ai_classic_turn_p2_do(g, pi);
            }
        }
    }
}

/* -------------------------------------------------------------------------- */

static void game_ai_classic_turn_p3_sub1(struct game_s *g, player_id_t pi)
{
    empiretechorbit_t *e = &(g->eto[pi]);
    for (player_id_t pi2 = PLAYER_0; pi2 < g->players; ++pi2) {
        e->spymode_next[pi2] = SPYMODE_HIDE;
        if (IS_HUMAN(g, pi2) && (g->evn.ceasefire[pi2][pi2] > 0)) { /* FIXME BUG should be [pi2][pi] */
            e->spymode_next[pi2] = SPYMODE_HIDE; /* FIXME redundant */
        } else if ((pi != pi2) && BOOLVEC_IS1(e->contact, pi2)) {
            if (e->treaty[pi2] >= TREATY_WAR) {
                e->spymode_next[pi2] = SPYMODE_SABOTAGE;
            } else if (e->spymode_next[pi2] == SPYMODE_HIDE) { /* FIXME BUG always true */
                if ((e->race == RACE_DARLOK) || rnd_0_nm1(2, &g->seed)) {
                    if (rnd_1_n(200, &g->seed) > (e->relation1[pi2] * 2 + 200)) {
                        e->spymode_next[pi2] = SPYMODE_ESPIONAGE;
                    }
                } else {
                    e->spymode_next[pi2] = SPYMODE_HIDE; /* FIXME redundant */
                }
            } else {
                if (rnd_1_n(100, &g->seed) > (e->relation1[pi2] + 100)) {
                    e->spymode_next[pi2] = SPYMODE_SABOTAGE;
                }
            }
        }
    }
}

static void game_ai_classic_turn_p3(struct game_s *g)
{
    /* AI p3 slider weights
        [0..3] ship/def/ind/eco
        [4..8] computer/construction/force field/planetology/propulsion
    */
    static const int8_t ai_p3_tbl_w[TRAIT2_NUM + 1/*war*/][9] = {
        { 1, 2, 4, 1,  2, 2, 4, 2, 3 },
        { 4, 1, 2, 1,  3, 1, 4, 2, 1 },
        { 2, 4, 1, 1,  1, 1, 3, 5, 2 },
        { 1, 2, 3, 1,  4, 2, 3, 2, 2 },
        { 1, 2, 4, 1,  1, 5, 4, 1, 2 },
        { 1, 1, 2, 4,  1, 2, 4, 1, 5 },
        { 4, 1, 1, 1,  2, 2, 4, 2, 1 }  /* War */
    };

    for (player_id_t pi = PLAYER_0; pi < g->players; ++pi) {
        empiretechorbit_t *e = &(g->eto[pi]);
        trait2_t ti;
        race_t race;
        if (IS_HUMAN(g, pi)) {
            continue;
        }
        race = e->race;
        /* BUG this was inside the if (--countdown) and ti is used later outside it */
        ti = e->trait2;
        for (player_id_t pi2 = PLAYER_0; pi2 < g->players; ++pi2) {
            if (e->treaty[pi2] >= TREATY_WAR) {
                ti = TRAIT2_NUM/*war*/;
           }
        }
        if (--e->ai_p3_countdown <= 0) {
            int w_ship, w_def, w_ind, w_eco;
            e->ai_p3_countdown = rnd_1_n(5, &g->seed) + 1;
            game_update_eco_on_waste(g, pi, true);
            w_ship = ai_p3_tbl_w[ti][0];
            w_def = w_ship + ai_p3_tbl_w[ti][1];
            w_ind = w_def + ai_p3_tbl_w[ti][2];
            w_eco = w_ind + ai_p3_tbl_w[ti][2];
            for (int i = 0; i < g->galaxy_stars; ++i) {
                planet_t *p = &(g->planet[i]);
                if (p->owner == pi) {
                    int left;
                    int16_t *sl;
                    sl = &(p->slider[0]);
                    sl[PLANET_SLIDER_SHIP] = 0;
                    sl[PLANET_SLIDER_DEF] = 0;
                    sl[PLANET_SLIDER_IND] = 0;
                    sl[PLANET_SLIDER_TECH] = 0;
                    if (sl[PLANET_SLIDER_ECO] < 96) {
                        sl[PLANET_SLIDER_SHIP] = 5;
                    }
                    if (sl[PLANET_SLIDER_ECO] < 91) {
                        sl[PLANET_SLIDER_DEF] = 5;
                    }
                    if (sl[PLANET_SLIDER_ECO] < 81) {
                        sl[PLANET_SLIDER_IND] = 10;
                    }
                    if (sl[PLANET_SLIDER_ECO] < 71) {
                        sl[PLANET_SLIDER_TECH] = 25;
                    }
                    if (p->pop < ((p->max_pop3 * 3) / 4)) {
                        sl[PLANET_SLIDER_IND] = 50;
                    }
                    if (((race == RACE_BULRATHI) || (race == RACE_SAKKRA)) && (p->pop < p->max_pop2)) {
                        sl[PLANET_SLIDER_ECO] += 10;
                    }
                    if ((race == RACE_KLACKON) && (p->pop < p->max_pop2)) {
                        sl[PLANET_SLIDER_ECO] += 15;
                    }
                    if (race == RACE_MEKLAR) {
                        sl[PLANET_SLIDER_IND] += 20;
                    }
                    if (race == RACE_PSILON) {
                        sl[PLANET_SLIDER_TECH] += 20;
                    }
                    if (race == RACE_MRRSHAN) {
                        sl[PLANET_SLIDER_SHIP] += 15;
                    }
                    if ((p->special == PLANET_SPECIAL_ARTIFACTS) || (p->special == PLANET_SPECIAL_4XTECH)) {
                        sl[PLANET_SLIDER_TECH] += 20;
                    }
                    if (p->special >= PLANET_SPECIAL_RICH) {
                        sl[PLANET_SLIDER_IND] += 20;
                        sl[PLANET_SLIDER_SHIP] += 10;
                    }
                    left = 100 - sl[PLANET_SLIDER_SHIP] - sl[PLANET_SLIDER_DEF] - sl[PLANET_SLIDER_IND] - sl[PLANET_SLIDER_ECO] - sl[PLANET_SLIDER_TECH];
                    while (left > 0) {
                        int r1, r2;
                        r1 = rnd_1_n(13, &g->seed);
                        r2 = rnd_1_n(8, &g->seed) + 2;
                        SETMIN(r2, left);
                        left -= r2;
                        if (r1 <= w_ship) {
                            sl[PLANET_SLIDER_SHIP] += r2;
                        } else if (r1 <= w_def) {
                            sl[PLANET_SLIDER_DEF] += r2;
                        } else if (r1 <= w_ind) {
                            sl[PLANET_SLIDER_IND] += r2;
                        } else if (r1 <= w_eco) {
                            sl[PLANET_SLIDER_ECO] += r2;
                        } else {
                            sl[PLANET_SLIDER_TECH] += r2;
                        }
                    }
                }
            }
        }
        for (int i = 0; i < g->galaxy_stars; ++i) {
            planet_t *p = &(g->planet[i]);
            if (p->owner == pi) {
                int16_t *sl;
                sl = &(p->slider[0]);
                if (p->factories >= (p->pop * e->colonist_oper_factories)) {
                    uint8_t si;
                    si = sl[PLANET_SLIDER_IND];
                    sl[PLANET_SLIDER_DEF] += si / 2;
                    sl[PLANET_SLIDER_TECH] += (si / 2) + (si % 2);
                    sl[PLANET_SLIDER_IND] = 0;
                }
                if (1
                  && (p->missile_bases > 4)
                  && ((p->missile_bases * 3) >= (p->pop / (5 - g->difficulty)))
                  && (p->shield >= e->have_planet_shield)
                ) {
                    uint16_t v;
                    if (game_num_ai_fleet_cheating_fix) {
                        v = sl[PLANET_SLIDER_DEF];
                    } else {
                        v = sl[PLANET_SLIDER_SHIP];   /* BUG creates very large fleets */
                    }
                    sl[PLANET_SLIDER_SHIP] += v;
                    sl[PLANET_SLIDER_DEF] = 0;
                }
            }
        }
        if ((g->year % 10) == pi) {
            int16_t *sl;
            sl = &(e->tech.slider[0]);
            sl[TECH_FIELD_COMPUTER] = 5;
            sl[TECH_FIELD_CONSTRUCTION] = 5;
            sl[TECH_FIELD_FORCE_FIELD] = 5;
            sl[TECH_FIELD_PLANETOLOGY] = 5;
            sl[TECH_FIELD_PROPULSION] = 5;
            sl[TECH_FIELD_WEAPON] = 5;
            if ((g->year < 30) && rnd_0_nm1(2, &g->seed)) {
                sl[TECH_FIELD_PROPULSION] = 75;
            } else {
                bool flag_focus_own;
                flag_focus_own = !rnd_0_nm1(4, &g->seed);
                if (((race == RACE_BULRATHI) || (race == RACE_MRRSHAN)) && flag_focus_own) {
                    sl[TECH_FIELD_WEAPON] = 75;
                } else if ((race == RACE_DARLOK) && flag_focus_own) {
                    sl[TECH_FIELD_COMPUTER] = 75;
                } else if ((race == RACE_MEKLAR) && flag_focus_own) {
                    sl[rnd_0_nm1(2, &g->seed) ? TECH_FIELD_CONSTRUCTION : TECH_FIELD_PLANETOLOGY] = 75;
                } else if ((race == RACE_ALKARI) && flag_focus_own) {
                    sl[TECH_FIELD_PROPULSION] = 75;
                } else if ((race == RACE_SAKKRA) && flag_focus_own) {
                    sl[TECH_FIELD_PLANETOLOGY] = 75;
                } else {
                    int r1, w_comp, w_cons, w_ff, w_plan, w_prop;
                    r1 = rnd_1_n(16, &g->seed);
                    w_comp = ai_p3_tbl_w[ti][4];
                    w_cons = w_comp + ai_p3_tbl_w[ti][5];
                    w_ff = w_cons + ai_p3_tbl_w[ti][6];
                    w_prop = w_ff + ai_p3_tbl_w[ti][8];
                    w_plan = w_prop + ai_p3_tbl_w[ti][7];
                    if (r1 <= w_comp) {
                        sl[TECH_FIELD_COMPUTER] = 75;
                    } else if (r1 <= w_cons) {
                        sl[TECH_FIELD_CONSTRUCTION] = 75;
                    } else if (r1 <= w_ff) {
                        sl[TECH_FIELD_FORCE_FIELD] = 75;
                    } else if (r1 <= w_prop) {
                        sl[TECH_FIELD_PROPULSION] = 75;
                    } else if (r1 <= w_plan) {
                        sl[TECH_FIELD_PLANETOLOGY] = 75;
                    } else {
                        sl[TECH_FIELD_WEAPON] = 75;
                    }
                }
            }
        }
        if (g->year > 30) {
            uint32_t totalspies;
            int16_t sec;
            totalspies = 0;
            for (player_id_t pi2 = PLAYER_0; pi2 < g->players; ++pi2) {
                if (BOOLVEC_IS1(e->contact, pi2)) {
                    totalspies += g->eto[pi2].spies[pi];
                }
            }
            if (totalspies == 0) {
                sec = 0;
            } else {
                sec = rnd_0_nm1(2, &g->seed) * totalspies;
            }
            sec += g->difficulty * 8;
            SETMIN(sec, 100);
            e->security = sec;
            game_ai_classic_turn_p3_sub1(g, pi);
            for (player_id_t pi2 = PLAYER_0; pi2 < g->players; ++pi2) {
                e->spying[pi2] = 0;
                e->spymode[pi2] = SPYMODE_HIDE;
                if (1
                  && (pi != pi2) && BOOLVEC_IS1(e->contact, pi2) && (e->spymode_next[pi2] != SPYMODE_HIDE)
                  && (BOOLVEC_IS0(g->is_ai, pi2) || !rnd_0_nm1(3, &g->seed))
                ) {
                    e->spymode[pi2] = e->spymode_next[pi2];
                    e->spying[pi2] = rnd_1_n(5, &g->seed) + 5;
                }
            }
        }
    }
}

/* -------------------------------------------------------------------------- */

static void game_ai_classic_battle_ai_ai_get_weights(const struct game_s *g, player_id_t pi, int *tbl)
{
    const empiretechorbit_t *e = &(g->eto[pi]);
    for (int i = 0; i < e->shipdesigns_num; ++i) {
        const shipdesign_t *sd = &(g->srd[pi].design[i]);
        tbl[i] += sd->shield * 5;
        tbl[i] += sd->comp * 5;
        tbl[i] += sd->wpnt[0];
        tbl[i] += sd->armor * 10;
        tbl[i] += sd->special[0] * 2;
        tbl[i] += sd->special[1] * 2;
        tbl[i] += sd->special[2] * 2;
        tbl[i] += e->tech.percent[TECH_FIELD_CONSTRUCTION] / 2;
        if ((e->race == RACE_MRRSHAN) || (e->race == RACE_ALKARI)) {
            tbl[i] += ((g->ai_id == GAME_AI_CLASSIC) ? 15 : 20);
        }
        tbl[i] *= game_num_tbl_hull_w[sd->hull];
    }
}

static bool game_ai_classic_battle_ai_ai_resolve_do(struct battle_s *bt)
{
    struct game_s *g = bt->g;
    bool r_won;
    int tbl_weight_l[NUM_SHIPDESIGNS], tbl_weight_r[NUM_SHIPDESIGNS];
    int base_l = 0, base_r = 0, wl = 0, wr = 0, wl2, wr2;
    bt->biodamage = 0;
    for (int i = 0; i < NUM_SHIPDESIGNS; ++i) {
        tbl_weight_l[i] = 0;
        tbl_weight_r[i] = 0;
    }
    if (bt->s[SIDE_L].party < PLAYER_NUM) {
        game_ai_classic_battle_ai_ai_get_weights(g, bt->s[SIDE_L].party, tbl_weight_l);
    }
    if (bt->s[SIDE_R].party < PLAYER_NUM) {
        game_ai_classic_battle_ai_ai_get_weights(g, bt->s[SIDE_R].party, tbl_weight_r);
    }
    switch (bt->planet_side) {
        case SIDE_NONE:
            break;
        case SIDE_L:
            {
                empiretechorbit_t *e = &(g->eto[bt->s[SIDE_L].party]);
                base_l = e->tech.percent[TECH_FIELD_WEAPON] + e->tech.percent[TECH_FIELD_FORCE_FIELD];
            }
            break;
        case SIDE_R:
            {
                empiretechorbit_t *e = &(g->eto[bt->s[SIDE_R].party]);
                base_r = e->tech.percent[TECH_FIELD_WEAPON] + e->tech.percent[TECH_FIELD_FORCE_FIELD];
            }
            break;
    }
    for (int i = 0; i < bt->s[SIDE_L].num_types; ++i) {
        wl += bt->s[SIDE_L].tbl_ships[i] * tbl_weight_l[bt->s[SIDE_L].tbl_shiptype[i]];
    }
    for (int i = 0; i < bt->s[SIDE_R].num_types; ++i) {
        wr += bt->s[SIDE_R].tbl_ships[i] * tbl_weight_r[bt->s[SIDE_R].tbl_shiptype[i]];
    }
    wl += base_l * 75 * bt->bases;
    wr += base_r * 75 * bt->bases;
    if (bt->s[SIDE_L].party >= PLAYER_NUM) {
        wl = ((bt->s[SIDE_L].party - PLAYER_NUM) == MONSTER_GUARDIAN) ? 100000 : 30000;
    }
    if (bt->s[SIDE_R].party >= PLAYER_NUM) {
        wr = ((bt->s[SIDE_R].party - PLAYER_NUM) == MONSTER_GUARDIAN) ? 100000 : 30000;
    }
    wl2 = wl;
    wr2 = wr;
    while ((wl2 > 0) && (wr2 > 0)) {
        int v;
        v = (rnd_1_n(10, &g->seed) * wl) / 100;
        SETMAX(v, 1);
        wr2 -= v;
        v = (rnd_1_n(10, &g->seed) * wr) / 100;
        SETMAX(v, 1);
        wl2 -= v;
    }
    SETMAX(wl2, 0);
    SETMAX(wr2, 0);
    r_won = wr2 > 0;
    for (battle_side_i_t s = SIDE_L; s <= SIDE_R; ++s) {
        if (bt->s[s].party < PLAYER_NUM) {
            int ws2, ws;
            ws2 = (s == SIDE_L) ? wl2 : wr2;
            ws = (s == SIDE_L) ? wl : wr;
            if (ws == 0) {
                for (int i = 0; i < NUM_SHIPDESIGNS; ++i) {
                    bt->s[s].tbl_ships[i] = 0;
                }
            } else {
                for (int i = 0; i < NUM_SHIPDESIGNS; ++i) {
                    if (game_num_doom_stack_fix) {
                        bt->s[s].tbl_ships[i] = (((uint64_t)bt->s[s].tbl_ships[i]) * ws2) / ws;
                    } else {
                        /* WASBUG MOO1 uses 32bit math which results in negative overflows with large ws2,
                           giving negative ship counts which get limited to 32000 later.
                        */
                        bt->s[s].tbl_ships[i] = (bt->s[s].tbl_ships[i] * ws2) / ws;
                    }
                }
            }
        }
    }
    if (g->ai_id == GAME_AI_CLASSIC) {
        /* WASBUG MOO1 uses ui_main_loop_action for the switch variable below;
           since it is UI_MAIN_LOOP_NEXT_TURN (== 8) at this point, bt->bases is unaffected.
        */
        return r_won;
    }
    switch (bt->planet_side) {
        case SIDE_NONE:
            bt->bases = 0;
            break;
        case SIDE_L:
            bt->bases = wl ? ((bt->bases * (uint64_t)wl2) / wl) : 0;
            break;
        case SIDE_R:
            bt->bases = wr ? ((bt->bases * (uint64_t)wr2) / wr) : 0;
            break;
    }
    /* BUG? MOO1 passes pointers to pop1 and factories but does not touch them */
    return r_won;
}

static bool game_ai_classic_battle_ai_ai_resolve(struct battle_s *bt)
{
    bool r_won;
    r_won = game_ai_classic_battle_ai_ai_resolve_do(bt);
    game_battle_finish(bt);
    return r_won;
}

/* -------------------------------------------------------------------------- */

static int game_ai_battle_missile_dmg(const struct battle_s *bt, int missile_i)
{
    const struct battle_missile_s *m = &(bt->missile[missile_i]);
    /*di*/const struct battle_item_s *b = &(bt->item[m->target]);
    const struct battle_item_s *bs = &(bt->item[m->source]);
    const struct shiptech_weap_s *w = &(tbl_shiptech_weap[m->wpnt]);
    int damagepotential, damagediv = 1, /*si*/miss_chance, damage;
    miss_chance = 50 - (bs->complevel - b->misdefense) * 10;
    if (b->cloak == 1) {
        miss_chance += 50;
    }
    miss_chance -= w->extraacc * 10;
    SETRANGE(miss_chance, 0, 95);
    if ((m->target == 0/*planet*/) && (!w->is_bomb) && (w->misstype > 0)) {
        damagediv = 2;
    }
    damage = w->damagemax / damagediv;
    if (bt->g->ai_id == GAME_AI_CLASSIC) {
        int absorbdiv = game_battle_get_absorbdiv(b, w, true); /* WASBUG checks target's Oracle */
        /* WASBUG should be damage -= b->absorb / absorbdiv; if Oracle worked for missiles */
        damage -= b->absorb;
        damage /= absorbdiv;
    } else {
        damage -= b->absorb;    /* missiles are not affected by Oracle */
    }
    damage *= w->damagemul;
    SETMIN(damage, b->hp1);
    damage *= m->damagemul2;
    damage *= m->nummissiles;
    if (damage > 0) {
        damagepotential = ((100 - miss_chance) * damage) / 100;
    } else {
        damagepotential = 0;
    }
    SETMAX(damagepotential, 0);
    return damagepotential;
}

static int game_ai_battle_incoming_missiles_dmg(const struct battle_s *bt, int itemi)
{
    int v = 0, hp;
    if (itemi == 0/*plamet*/) {
        return 0;
    }
    hp = bt->item[itemi].hp1;
    for (int i = 0; i < bt->num_missile; ++i) {
        const struct battle_missile_s *m = &(bt->missile[i]);
        if (m->target == itemi) {
            v += game_ai_battle_missile_dmg(bt, i) / hp;
        }
    }
    return v;
}

static int game_ai_battle_dmgmax(struct battle_s *bt, int itemi)
{
    const struct battle_item_s *b = &(bt->item[itemi]);
    int v = 0, num_weap = (itemi == 0/*planet*/) ? 1 : 4;
    bool flag_planet_opponent = false;
#if 1
    if ((b->side + bt->item[0/*planet*/].side) == 1) {
        flag_planet_opponent = true;
    }
#else
    /* FIXME MOO1 does this, but v8 is unused */
    int v8 = 0;
    if (b->side == SIDE_L) {
        if (bt->item[0/*planet*/].side == SIDE_R) {
            flag_planet_opponent = true;
            v8 = bt->item[0/*planet*/].absorb;
        }
        for (int i = bt->s[SIDE_L].items + 1; i <= bt->items_num; ++i) {
            v8 += bt->item[i].absorb;
        }
        if ((bt->s[SIDE_R].items > 0) || flag_planet_opponent) {
            v8 /= bt->s[SIDE_R].items + (flag_planet_opponent ? 1 : 0);
        }
    } else {
        if (bt->item[0/*planet*/].side == SIDE_L) {
            flag_planet_opponent = true;
            v8 = bt->item[0/*planet*/].absorb;
        }
        for (int i = 1; i <= bt->s[SIDE_L].items; ++i) {
            v8 += bt->item[i].absorb;
        }
        if ((bt->s[SIDE_R].items > 0) || flag_planet_opponent) {
            v8 /= bt->s[SIDE_R].items + (flag_planet_opponent ? 1 : 0);
        }
    }
#endif
    for (int i = 0; i < num_weap; ++i) {
        const struct shiptech_weap_s *w = &(tbl_shiptech_weap[b->wpn[i].t]);
        if (b->wpn[i].numshots != 0) {
            if (w->is_bomb) {
                if (flag_planet_opponent) {
                    int dmg;
                    dmg = w->damagemax - bt->item[0/*planet*/].absorb;
                    if (dmg > 0) {
                        v += dmg * b->wpn[i].n * w->numfire;
                    }
                    if (w->is_bio) {
                        dmg = w->damagemax - bt->antidote;
                        if (dmg > 0) {
                            v += dmg * b->wpn[i].n * w->numfire * 50;
                        }
                    }
                }
            } else {
                v += w->damagemax * b->wpn[i].n * w->numfire;
            }
        }
    }
    if (b->blackhole == 1) {
        v += 1000;
    }
    return v;
}

static int game_ai_battle_dmggive(struct battle_s *bt, int itemi1, int itemi2, int a2)
{
    struct battle_item_s *b = &(bt->item[itemi1]);
    /*si*/struct battle_item_s *bd = &(bt->item[itemi2]);
    int v = 0, num_weap = (itemi1 == 0/*planet*/) ? 1 : 4, miss_chance_beam, miss_chance_missile, dist, damagediv = 1, dmg;
    if ((bd->stasisby > 0) && ((a2 == 0) || (a2 == 2))) {
        return 0;
    }
    miss_chance_beam = 50 - (b->complevel - bd->defense) * 10;
    miss_chance_missile = 50 - (b->complevel - bd->misdefense) * 10;
    if (bd->cloak == 1) {
        miss_chance_beam += 50;
        miss_chance_missile += 50;
    }
    SETRANGE(miss_chance_beam, 0, 95);
    if (bt->g->ai_id == GAME_AI_CLASSICPLUS) {
        SETMIN(miss_chance_missile, 95);
    }
    dist = util_math_dist_maxabs(b->sx, b->sy, bd->sx, bd->sy);
    for (int i = 0; i < num_weap; ++i) {
        const struct shiptech_weap_s *w = &(tbl_shiptech_weap[b->wpn[i].t]);
        int absorbdiv, range;
        if (bt->g->ai_id == GAME_AI_CLASSIC) {
            absorbdiv = game_battle_get_absorbdiv(bd, w, true); /* WASBUG checks target's oracle */
        } else {
            absorbdiv = game_battle_get_absorbdiv(b, w, false);
        }
        if (itemi2 == 0/*planet*/) {
            if ((!w->is_bomb) && ((w->misstype > 0) || (w->damagemin != w->damagemax))) {
                damagediv = 2;
            } else {
                damagediv = 1;
            }
        }
        range = 0;
        if ((w->damagemin != w->damagemax) && (!w->is_bomb)) {
            range = b->extrarange;
        } else if (b->maxrange < 0) {
            range = b->maxrange;
        }
        if (itemi1 == 0/*planet*/) {
            range += 40;
        }
        if (1
          && ((!w->is_bomb) || (itemi2 == 0/*planet*/))
          && (b->wpn[i].numshots != 0)
          && (((w->range + range) >= dist) || (a2 > 0))
          && ((b->wpn[i].numfire > 0) || (a2 > 0))
        ) {
            if (w->damagemin == w->damagemax) {
                /*584de*/
                int miss_chance;
                miss_chance = miss_chance_missile - w->extraacc * 10;
                SETRANGE(miss_chance, 0, 95);
                if (bt->g->ai_id == GAME_AI_CLASSIC) {
                    miss_chance_missile = miss_chance; /* WASBUG MOO1 substracts for every missile weapon */
                }
                dmg = ((100 - miss_chance) / 5) * (w->damagemax - bd->absorb / absorbdiv);
                dmg /= damagediv;
                dmg *= w->damagemul;
                dmg *= w->nummiss;
            } else {
                /*585ae*/
                int miss_chance;
                miss_chance = miss_chance_beam;
                if (bd->absorb > w->damagemin) {
                    miss_chance += ((100 - miss_chance) * (bd->absorb + 1 - w->damagemin)) / (w->damagemax + 1 - w->damagemin);
                    if (bt->g->ai_id == GAME_AI_CLASSICPLUS) {
                        SETMIN(miss_chance, 95);
                    }
                }
                if (bt->g->ai_id == GAME_AI_CLASSIC) {
                    miss_chance_beam = miss_chance; /* WASBUG MOO1 adds for every beam weapon */
                }
                /*5861e*/
                if ((w->damagemax / damagediv) > (bd->absorb / absorbdiv)) {
                    int dmgmin, dmgmax;
                    dmgmax = (w->damagemax / damagediv) - (bd->absorb / absorbdiv);
                    if (bt->g->ai_id == GAME_AI_CLASSIC) {
                        dmgmin = (w->damagemin > bd->absorb) ? (w->damagemin - bd->absorb) : 1; /* WASBUG {damage,absorb}div? */
                    } else {
                        dmgmin = (w->damagemin / damagediv) - (bd->absorb / absorbdiv);
                        SETMAX(dmgmin, 1);
                    }
                    dmg = (dmgmax + dmgmin) / 2;
                    dmg = ((100 - miss_chance) * dmg) / 5;
                    dmg *= w->damagemul;
                } else {
                    dmg = 0;
                }
            }
            /*58729*/
            if (w->is_bio && (b->wpn[i].numshots != 0)) {
                dmg = 500 * w->damagemax;
            }
            /*58785*/
            if (dmg <= 0) {
                dmg = ((w->damagemax / damagediv) > (bd->absorb / absorbdiv));
            }
            if (a2 > 0) {
                v += b->wpn[i].n * dmg * w->numfire;
            } else {
                /*5882d*/
                v += b->wpn[i].n * dmg * b->wpn[i].numfire;
            }
            /*58873*/
        }
    }
    if ((b->warpdis == 1) && ((dist <= 3) || (a2 > 0)) && (itemi2 != 0/*planet*/) && (bd->unman < bd->man)) {
        v += 100;
    }
    if ((b->blackhole == 1) && ((dist <= 1) || (a2 > 0))) {
        v += 1000;
    }
    if ((b->technull == 1) && ((dist <= 4) || (a2 > 0))) {
        v += 500;
    }
    if ((b->pulsar == 1) && (bt->special_button == 1) && ((dist <= 1) || (a2 > 0))) {
        dmg = (5 - bd->absorb) * bd->num; /* FIXME BUG? should be b->num ? */
        if (dmg > 0) {
            v += dmg;
        }
    }
    if ((b->pulsar == 2) && (bt->special_button == 1) && ((dist <= 1) || (a2 > 0))) {
        dmg = (14 - bd->absorb) * bd->num; /* FIXME BUG? should be b->num ? */
        if (dmg > 0) {
            v += dmg;
        }
    }
    if ((b->stream == 1) && ((dist <= 2) || (a2 > 0))) {
        dmg = ((bd->hp1 * ((b->num / 2) + 20)) / 100) * bd->num;
        if (dmg > 0) {
            v += dmg;
        }
    }
    if ((b->stream == 2) && ((dist <= 2) || (a2 > 0))) {
        dmg = ((bd->hp1 * (b->num + 40)) / 100) * bd->num;
        if (dmg > 0) {
            v += dmg;
        }
    }
    return v;
}

static int game_ai_battle_rival(struct battle_s *bt, int itemi, int a2)
{
    /*di*/struct battle_item_s *b = &(bt->item[itemi]);
    int rival = -1, maxw = 0;
    for (int i = 0; i <= bt->items_num; ++i) {
        struct battle_item_s *b2 = &(bt->item[i]);
        if (((b->side + b2->side) == 1) && (b->num > 0)) {
            int dmgmissile, dmgmax, w, dmgmany, dmggive, repair;
            dmgmissile = game_ai_battle_incoming_missiles_dmg(bt, i);
            dmgmax = game_ai_battle_dmgmax(bt, i);
            dmggive = game_ai_battle_dmggive(bt, itemi, i, a2);
            repair = (b2->repair * b2->hp2) / 100;
            if (itemi == 0/*planet*/) {
                int dmgother;
                weapon_t t;
                t = b->wpn[0].t; b->wpn[0].t = b->wpn[1].t; b->wpn[1].t = t;
                dmgother = game_ai_battle_dmggive(bt, itemi, i, a2);
                if (dmgother > dmggive) {
                    dmggive = dmgother;
                } else {
                    t = b->wpn[0].t; b->wpn[0].t = b->wpn[1].t; b->wpn[1].t = t;
                }
            }
            /*58fc7*/
            dmgmany = (b->num * dmggive - repair) / (b2->hp1 * 20);
            if (dmgmany > 0) {
                int vt;
                vt = b2->num - dmgmissile;  /* FIMXE BUG? num - dmg? */
                if (vt < dmgmany) {
                    dmgmany = vt;
                    SETMAX(dmgmany, 0);
                }
                /*59099*/
                w = dmgmany * dmgmax * 20;
            } else {
                /*590c1*/
                if ((dmgmissile > 0) && (dmggive > 0)) {
                    w = 3;
                } else {
                    /*590e1*/
                    w = ((dmggive - repair) * dmgmax) / b2->hp1;
                }
                /*59124*/
                SETMAX(w, 0);
                if (dmggive > 0) {
                    SETMAX(w, 3);
                }
                if ((w == 1) && (i != 0/*planet*/)) {
                    w = 2;
                }
            }
            /*5917c*/
            if ((dmgmax <= 0) && (dmggive > 0)) {
                w = (i == 0/*planet*/) ? 1 : 2;
            }
            if ((itemi == 0/*planet*/) && (w > 0)) {
                int dist;
                dist = util_math_dist_maxabs(b->sx, b->sy, b2->sx, b2->sy);
                w += (10 - dist) * 750;
            }
            /*59221*/
            if ((i == 0/*planet*/) && (w == 1)) {
                for (int j = 0; j < WEAPON_SLOT_NUM; ++j) {
                    weapon_t wpnt = b->wpn[j].t;
                    if ((tbl_shiptech_weap[wpnt].is_bio) && (b->wpn[j].numshots != 0)) {
                        dmggive = tbl_shiptech_weap[wpnt].damagemax - bt->antidote;
                        if (dmggive > 0) {
                            w += dmggive * b->wpn[j].n * 100;
                        }
                    }
                }
            }
            /*59306*/
            if (w > maxw) {
                rival = i;
                maxw = w;
                bt->bases_using_mirv = (tbl_shiptech_weap[bt->item[0/*planet*/].wpn[0].t].nummiss > 1);
            }
        }
    }
    /*59354*/
    if (itemi == 0/*planet*/) {
        weapon_t t = b->wpn[0].t;
        bool is_mirv = (tbl_shiptech_weap[t].nummiss > 1);
        if (bt->bases_using_mirv != is_mirv) {
            b->wpn[0].t = b->wpn[1].t; b->wpn[1].t = t;
        }
    }
    return rival;
}

static bool game_battle_missile_none_fired_by(const struct battle_s *bt, int itemi)
{
    for (int i = 0; i < bt->num_missile; ++i) {
        const struct battle_missile_s *m = &(bt->missile[i]);
        if (m->source == itemi) {
            return false;
        }
    }
    return true;
}

static int game_battle_stasis_target(struct battle_s *bt)
{
    int itemi = bt->cur_item;
    struct battle_item_s *b = &(bt->item[itemi]);
    int target_i = -1;
    bool flag_have_unstasis = false;
    {
        int n = 0, itembase = (b->side == SIDE_R) ? 1 : (bt->s[SIDE_L].items + 1), itemnum = bt->s[b->side ^ 1].items;
        for (int i = 0; i < itemnum; ++i) {
            if (bt->item[itembase + i].stasisby > 0) {
                ++n;
            }
        }
        if ((itemnum - 2) >= n) {
            flag_have_unstasis = true;
        }
    }
    if (flag_have_unstasis) {
        int vmax = 0;
        b->stasis = 1;
        for (int i = 1; i <= bt->items_num; ++i) {
            struct battle_item_s *bd = &(bt->item[i]);
            if ((b->side != bd->side) && (bd->stasisby == 0) && (bd->cloak != 1)) {
                int v;
                v = game_ai_battle_dmgmax(bt, i) * bd->num;
                if (v > vmax) {
                    vmax = v;
                    target_i = i;
                }
            }
        }
    } else {
        b->stasis = 2;
    }
    return target_i;
}

static int game_battle_ai_missile_evade(const struct battle_s *bt)
{
    int itemi = bt->cur_item;
    const struct battle_item_s *b = &(bt->item[itemi]);
    int evade = 0;
    if (b->unman == b->man) {
        return 0;
    }
    for (int i = 0; (i < bt->num_missile) && (evade <= 1); ++i) {
        const struct battle_missile_s *m = &(bt->missile[i]);
        if (m->target == itemi) {
            int roomx, dangerdist, movex, dist, sx;
            sx = m->x / 32;
            if (b->subspace == 1) {
                roomx = 9 - sx;
                SETMAX(roomx, sx);
            } else {
                sx -= b->sx;
                if (sx < 0) {
                    roomx = 9 - b->sx;
                } else /*if (sx > 0)*/ { /* WASBUG this was if > 0 and the == 0 case was unhandled */
                    roomx = b->sx;
                }
            }
            movex = (b->man - b->unman) * m->fuel;
            if ((movex > roomx) || (b->subspace == 1)) {
                movex = roomx;
            }
            dist = util_math_dist_fast(b->sx * 32 + 16, b->sy * 24 + 12, m->x, m->y);
            dangerdist = tbl_shiptech_weap[m->wpnt].dtbl[0] * m->fuel + 13;
            if (dist > dangerdist) {
                evade = 1;
            }
            dist += movex * 32 - 18;
            if ((rnd_1_n(3, &bt->g->seed) < bt->g->difficulty) && (dist > dangerdist)) {
                evade = 2;
            }
        }
    }
    return evade;
}

/*
 * Tries to approximate by how much enemy ship can increase distance in 2 turns.
 */
static void get_possible_distance_increase(struct battle_s *bt, int target_i)
{
    struct battle_item_s *b = &(bt->item[bt->cur_item]);
    const struct battle_item_s *bd = &(bt->item[target_i]);
    int si, t;
    if (b->sx < bd->sx) {
        si = BATTLE_AREA_W - bd->sx - 1;
    } else {
        si = bd->sx;
    }
    t = (bd->man - bd->unman) * 2;
    b->maxrange = -(MIN(si, t));
}

static int game_battle_ai_best_range(struct battle_s *bt, int target_i)
{
    int itemi = bt->cur_item;
    const struct battle_item_s *b = &(bt->item[itemi]);
    const struct battle_item_s *bd = &(bt->item[target_i]);
    int bestrange = 1, damagediv = 1, weight1 = 0;
    if (target_i == 0/*planet*/) {
        return 1;
    }
    for (int i = 1; i < 10; ++i) {
        int weight;
        weight = 0;
        if ((b->blackhole > 0) && (i <= 1)) {
            weight += 2000;
        }
        if ((b->stasis == 1) && (i <= 1)) {
            weight += 2000;
        }
        if ((b->pulsar == 1) && (i <= 1)) {
            int v;
            v = (6 - bd->absorb) * bd->num;
            if (v > 0) {
                weight += v;
            }
        }
        if ((b->pulsar == 2) && (i <= 1)) {
            int v;
            v = (16 - bd->absorb) * bd->num;
            if (v > 0) {
                weight += v;
            }
        }
        if ((b->stream == 1) && (i <= 2)) {
            int v;
            v = ((bd->hp1 * (b->num + 20) + 99) / 100) * bd->num;
            if (v > 0) {
                weight += v;
            }
        }
        if ((b->stream == 2) && (i <= 2)) {
            int v;
            v = ((bd->hp1 * (b->num + 40) + 99) / 100) * bd->num;
            if (v > 0) {
                weight += v;
            }
        }
        for (int j = 0; j < WEAPON_SLOT_NUM; ++j) {
            const struct shiptech_weap_s *w = &(tbl_shiptech_weap[b->wpn[j].t]);
            int absorbdiv, range;
            if (bt->g->ai_id == GAME_AI_CLASSIC) {
                absorbdiv = game_battle_get_absorbdiv(bd, w, true); /* WASBUG checks target's oracle */
            } else {
                if (w->damagemin != w->damagemax) {
                    absorbdiv = game_battle_get_absorbdiv(b, w, false);
                } else {
                    absorbdiv = 1;
                }
            }
            if (target_i == 0/*planet*/) {
                if ((!w->is_bomb) && ((w->misstype > 0) || (w->damagemin != w->damagemax))) {
                    damagediv = 2;
                } else {
                    damagediv = 1;  /* WASBUG there was no else, so the 2 stuck */
                }
            }
            range = (itemi == 0/*planet*/) ? 12 : 0;
            if ((w->damagemax != w->damagemin) && (!w->is_bomb)) {
                range = b->extrarange;
            } else if (!w->is_bomb) {
                /* if missile (or no weapon but it doesn't matter) */
                get_possible_distance_increase(bt, target_i);
                range = b->maxrange;
            }
            if (((w->range + range) >= i) && (b->wpn[j].numshots != 0) && ((!w->damagefade) || (i == 1))) {
                int dmg;
                dmg = (w->damagemax / damagediv - bd->absorb / absorbdiv) * w->damagemul * b->wpn[j].n * w->numfire;
                if (w->is_bio) {
                    dmg = w->damagemax * 100;
                }
                SETMAX(dmg, 0);
                weight += dmg;
            }
        }
        if (i == 1) {
            weight1 = weight;
        }
        if (weight == 0) {
            break;
        }
        if (b->cloak == 1) {
            if (weight1 > weight) {
                break;
            } else {
                bestrange = i;
            }
        } else {
            if (((weight1 * 2) / (weight * 3)) <= 1) {
                bestrange = i;
            } else {
                break;
            }
        }
    }
    return bestrange;
}

/*
 * Evaluate position (sx, sy) in case ship has pulsar.
 * If position is next to friendly ship that can't absorb pulsar damage devaluate it.
 * If position is next to enemy ship that can't absorb pulsar damage add value to it.
 */
static int eval_pos_for_pulsar_use(struct battle_s *bt, int sx, int sy)
{
    struct battle_item_s *b = &(bt->item[bt->cur_item]);
    int ndiv, rbase, weight = 0, dmg;
    if (b->pulsar == 1) {
        ndiv = 2;
        rbase = 5;
    } else {
        ndiv = 1;
        rbase = 10;
    }
    dmg = rbase + b->num / ndiv;
    for (int i = 0; i <= bt->items_num; ++i) {
        struct battle_item_s *bd = &(bt->item[i]);
        if (util_math_dist_maxabs(b->sx, b->sy, bd->sx, bd->sy) == 1) {
            if (b->side == bd->side) {
                if (bd->absorb < dmg) {
                    weight -= 4;
                }
            } else {
                if (bd->absorb < dmg) {
                    weight += 3;
                }
            }
        }
    }
    return weight;
}

static int game_battle_ai_target1_sub3(struct battle_s *bt, int sx, int sy, int target_i, int bestrange)
{
    int itemi = bt->cur_item;
    const struct battle_item_s *b = &(bt->item[itemi]);
    const struct battle_item_s *bd = &(bt->item[target_i]);
    int si = 0, dist, len, tblx[20], tbly[20];
    dist = util_math_dist_maxabs(sx, sy, bd->sx, bd->sy);
    len = util_math_line_plot(sx, sy, bd->sx, bd->sy, tblx, tbly);
    if (dist == bestrange) {
        si = 10;
    } else {
        si -= dist * 4;
    }
    if ((b->pulsar == 1) || (b->pulsar == 2)) {
        si += eval_pos_for_pulsar_use(bt, sx, sy);
    }
    if (game_battle_area_check_line_ok(bt, tblx, tbly, len) < 1) {
        si -= 2;
    }
    if (bt->area[sy][sx] == (itemi + 10)) {
        si += (dist == bestrange) ? 1 : -1;
    }
    return si;
}

static void game_battle_ai_target1_sub4(struct battle_s *bt)
{
    int itemi = bt->cur_item;
    struct battle_item_s *b = &(bt->item[itemi]);
    int dist = 0, mindist, n = 0, oppdist;
    uint8_t tblxy[10];
    if (b->unman == b->man) {
        return;
    }
    if (b->subspace == 1) {
        oppdist = 0;
        for (int sy = 0; sy < BATTLE_AREA_H; ++sy) {
            for (int sx = 0; sx < BATTLE_AREA_W; ++sx) {
                if (bt->area[sy][sx] == 1) {
                    for (int i = 0; i <= bt->items_num; ++i) {
                        const struct battle_item_s *bd = &(bt->item[i]);
                        mindist = 10;   /* FIXME BUG this results in always setting to last enemy dist */
                        if ((b->side + bd->side) == 1) {
                            dist = util_math_dist_maxabs(sx, sy, bd->sx, bd->sy);
                            SETMIN(mindist, dist);
                        }
                    }
                    if (oppdist <= mindist) {
                        if (oppdist < dist) {
                            n = 0;
                        }
                        oppdist = dist;
                        tblxy[n++] = BATTLE_XY_SET(sx, sy);
                        SETMIN(n, TBLLEN(tblxy) - 1); /* WASBUG? not limited in MOO1 */
                    }
                }
            }
        }
        /*59d70*/
        game_battle_item_move(bt, itemi, BATTLE_XY_GET_X(tblxy[0]), BATTLE_XY_GET_Y(tblxy[0]));
    } else {
        /*59d85*/
        oppdist = 10;
        for (int i = 0; i <= bt->items_num; ++i) {
            const struct battle_item_s *bd = &(bt->item[i]);
            if ((b->side + bd->side) == 1) {
                dist = util_math_dist_maxabs(b->sx, b->sy, bd->sx, bd->sy);
                SETMIN(oppdist, dist);
            }
        }
        while (b->actman > 0) {
            n = 0;
            for (int sx = b->sx - 1; sx <= (b->sx + 1); ++sx) {
                if ((sx < 0) || (sx >= BATTLE_AREA_W)) {
                    continue;
                }
                for (int sy = b->sy - 1; sy < (b->sy + 1); ++sy) {
                    if ((sy < 0) || (sy >= BATTLE_AREA_H)) {
                        continue;
                    }
                    if (bt->area[sy][sx] == 1) {
                        mindist = 10;
                        for (int i = 0; i <= bt->items_num; ++i) {
                            const struct battle_item_s *bd = &(bt->item[i]);
                            if ((b->side + bd->side) == 1) {
                                dist = util_math_dist_maxabs(sx, sy, bd->sx, bd->sy);
                                SETMIN(mindist, dist);
                            }
                        }
                        if (mindist > oppdist) {
                            oppdist = mindist;
                            n = 1;
                            tblxy[0] = BATTLE_XY_SET(sx, sy);
                        }
                    }
                }
            }
            if (n > 0) {
                game_battle_item_move(bt, itemi, BATTLE_XY_GET_X(tblxy[0]), BATTLE_XY_GET_Y(tblxy[0]));
            } else {
                b->actman = 0;
            }
        }
    }
}

static void game_battle_ai_target1_sub5(struct battle_s *bt)
{
    int itemi = bt->cur_item;
    struct battle_item_s *b = &(bt->item[itemi]);
    int dist = 0, mindist = 10, missdist = 0;
    uint8_t xy = BATTLE_XY_INVALID;
    if (b->unman == b->man) {
        return;
    }
    if (b->subspace == 1) {
        for (int sy = 0; sy < BATTLE_AREA_H; ++sy) {
            for (int sx = 0; sx < BATTLE_AREA_W; ++sx) {
                if (bt->area[sy][sx] == 1) {
                    for (int i = 0; i < bt->num_missile; ++i) {
                        const struct battle_missile_s *m = &(bt->missile[i]);
                        if ((m->target == itemi) && (m->fuel < 8)) {
                            mindist = 10;   /* FIXME BUG this results in always setting to last missile dist */
                            dist = util_math_dist_maxabs(sx, sy, m->x / 32, m->y / 24);
                            SETMIN(mindist, dist);
                        }
                    }
                    if (missdist < mindist) {
                        xy = BATTLE_XY_SET(sx, sy);
                        missdist = dist; /* FIXME BUG? should be mindist ? */
                    }
                }
            }
        }
        if (xy == BATTLE_XY_INVALID) {
            log_warning("BUG: subspace destination not found\n");
            xy = BATTLE_XY_SET(b->sx, b->sy);
        }
        game_battle_item_move(bt, itemi, BATTLE_XY_GET_X(xy), BATTLE_XY_GET_Y(xy));
    } else {
        /*5a0d8*/
        while (b->actman > 0) {
            missdist = 10;
            for (int i = 0; i < bt->num_missile; ++i) {
                const struct battle_missile_s *m = &(bt->missile[i]);
                if ((m->target == itemi) && (m->fuel < 8)) {
                    dist = util_math_dist_maxabs(b->sx, b->sy, m->x / 32, m->y / 24);
                    SETMIN(missdist, dist);
                }
            }
            xy = BATTLE_XY_INVALID;
            for (int sx = b->sx - 1; sx <= (b->sx + 1); ++sx) {
                if ((sx < 0) || (sx >= BATTLE_AREA_W)) {
                    continue;
                }
                for (int sy = b->sy - 1; sy < (b->sy + 1); ++sy) {
                    if ((sy < 0) || (sy >= BATTLE_AREA_H)) {
                        continue;
                    }
                    if ((bt->area[sy][sx] == 1) && ((b->sx != sx) || (b->sy != sy))) {
                        mindist = 10;
                        for (int i = 0; i < bt->num_missile; ++i) {
                            const struct battle_missile_s *m = &(bt->missile[i]);
                            if ((m->target == itemi) && (m->fuel < 8)) {
                                dist = util_math_dist_maxabs(sx, sy, m->x / 32, m->y / 24);
                                SETMIN(mindist, dist);
                            }
                        }
                        if (mindist > missdist) {
                            missdist = mindist;
                            xy = BATTLE_XY_SET(sx, sy);
                        }
                    }
                }
            }
            if (xy != BATTLE_XY_INVALID) {
                game_battle_item_move(bt, itemi, BATTLE_XY_GET_X(xy), BATTLE_XY_GET_Y(xy));
            } else {
                b->actman = 0;
            }
        }
    }
}

static int game_battle_ai_target1(struct battle_s *bt, int target_i)
{
    int itemi = bt->cur_item;
    struct battle_item_s *b = &(bt->item[itemi]);
    int bestrange;
    bestrange = game_battle_ai_best_range(bt, target_i);
    if ((b->actman > 0) || (b->subspace == 1)) {
        if ((target_i != -1) && (game_battle_ai_missile_evade(bt) == 0)) {
            int vmax = -999, n = 1, i;
            uint8_t tblxy[20];
            for (int sy = 0; sy < BATTLE_AREA_H; ++sy) {
                for (int sx = 0; sx < BATTLE_AREA_W; ++sx) {
                    if ((bt->area[sy][sx] == 1) || ((b->sx == sx) && (b->sy == sy))) {
                        int v;
                        /*5a3dc*/
                        v = game_battle_ai_target1_sub3(bt, sx, sy, target_i, bestrange);
                        if (v > vmax) {
                            n = 1;
                            tblxy[0] = BATTLE_XY_SET(sx, sy);
                            vmax = v;
                        } else if ((v == vmax) && (n < TBLLEN(tblxy))) {
                            tblxy[n++] = BATTLE_XY_SET(sx, sy);
                        }
                    }
                }
            }
            /*5a449*/
            i = (n < 2) ? 0 : rnd_0_nm1(n, &bt->g->seed);
            if (itemi != 0/*planet*/) {
                game_battle_item_move(bt, itemi, BATTLE_XY_GET_X(tblxy[i]), BATTLE_XY_GET_Y(tblxy[i]));
            }
        } else if (target_i == -1) {
            /*5a494*/
            game_battle_ai_target1_sub4(bt);
        } else {
            /*5a4a0*/
            game_battle_ai_target1_sub5(bt);
        }
    }
    return bestrange;
}

static void game_ai_classic_battle_ai_turn(struct battle_s *bt)
{
    int itemi = bt->cur_item;
    struct battle_item_s *b = &(bt->item[itemi]);
    /*5a581*/
    int /*si*/target_i = 0;  /* FIXME WASBUG used uninitialized later ; proper value ? */
    int v4;
    game_battle_area_setup(bt);    /* FIXME already done by caller */
    if (1
       && (b->side == SIDE_R)
       && (game_ai_battle_rival(bt, itemi, 1) == -1)
       && game_battle_missile_none_fired_by(bt, itemi)
    ) {
        ++b->retreat;
    }
    /*5a5cb*/
    if ((b->man - b->unman) == 0) {
        b->retreat = 0;
    }
    /*5a604*/
    if (b->stasis > 0) {
        target_i = game_battle_stasis_target(bt);
        if (target_i != -1) {
            game_battle_ai_target1(bt, target_i);
            game_battle_area_setup(bt);
            if (!bt->flag_cur_item_destroyed) {
                const struct battle_item_s *bd = &(bt->item[target_i]);
                if (util_math_dist_maxabs(b->sx, b->sy, bd->sx, bd->sy) <= 1) {
                    get_possible_distance_increase(bt, target_i);
                    game_battle_attack(bt, itemi, target_i, false);
                }
            }
        }
    }
    /*5a69e*/
    game_battle_area_setup(bt);
    if (1
      && (game_ai_battle_incoming_missiles_dmg(bt, itemi) > 0)
      && (target_i != -1) /* BUG used uninitialized if b->stasis == 0 */
      && ((target_i = game_ai_battle_rival(bt, itemi, 0)) > -1)
    ) {
        if ((b->pulsar == 1) || (b->pulsar == 2)) {
            if (eval_pos_for_pulsar_use(bt, b->sx, b->sy) < 0) {
                bt->special_button = 0;
            }
        }
        /*5a72a*/
        get_possible_distance_increase(bt, target_i);
        game_battle_attack(bt, itemi, target_i, false);
    }
    /*5a740*/
    if (bt->special_button == -1) {
        bt->special_button = 1;
    }
    game_battle_area_setup(bt);
    if (b->retreat > 0) {
        target_i = -1;
    } else {
        target_i = game_ai_battle_rival(bt, itemi, 2);
    }
    if (!bt->flag_cur_item_destroyed) {
        v4 = game_battle_ai_target1(bt, target_i);
    }
    if (!bt->flag_cur_item_destroyed) {
        int loops;
        if ((b->pulsar == 1) || (b->pulsar == 2)) {
            if (eval_pos_for_pulsar_use(bt, b->sx, b->sy) < 0) {
                bt->special_button = 0;
            }
        }
        target_i = game_ai_battle_rival(bt, itemi, 0);
        if ((target_i == -1) && (itemi == 0/*planet*/) && (b->num > 0)) {
            int ii = (b->side == SIDE_R) ? 1 : (bt->s[SIDE_L].items + 1);
            if (bt->item[ii].side != b->side) {
                game_battle_attack(bt, itemi, ii, false);
            }
        }
        /*5a866*/
        loops = 0;
        while (target_i != -1) {
            target_i = game_ai_battle_rival(bt, itemi, 0);
            ++loops;
            if ((b->cloak == 1) && (target_i != -1)) { /* WASBUG no test for -1 */
                const struct battle_item_s *bd;
                bd = &(bt->item[target_i]);
                if (util_math_dist_maxabs(b->sx, b->sy, bd->sx, bd->sy) != v4) {    /* WASBUG bd at index -1 */
                    target_i = -1;
                }
            }
            if (b->retreat > 0) {
                target_i = -1;
            }
            /*5a8e1*/
            if (target_i > -1) {
                get_possible_distance_increase(bt, target_i);
                game_battle_attack(bt, itemi, target_i, false);
            }
            if (loops > 10) {  /* MOO1 does not need this but it does have the loop counter */
                LOG_DEBUG((3, "%s: BUG: break from loop, item:%i target_i:%i\n", __func__, itemi, target_i));
                break;
            }
        }
        /*5a91b*/
        b->selected = 0;
        bt->turn_done = true;
    }
    /*5a934*/
}

/* -------------------------------------------------------------------------- */

static bool game_ai_classic_battle_ai_retreat(struct battle_s *bt)
{
    int missile[BATTLE_ITEM_MAX];
    uint32_t repair[BATTLE_ITEM_MAX];
    int64_t dmg[2] = { 0, 0 };
    uint32_t hp[2] = { 0, 0 };
    for (int i = 0; i < BATTLE_ITEM_MAX; ++i) {
        const struct battle_item_s *b = &(bt->item[i]);
        missile[i] = 0;
        repair[i] = (b->hp1 * b->repair) / 5;
    }
    for (int i = 0; i < bt->num_missile; ++i) {
        const struct battle_missile_s *m = &(bt->missile[i]);
        if (m->target != MISSILE_TARGET_NONE) {
            int s;
            s = bt->item[m->target].side;
            missile[m->source] += 2;
            dmg[s] += game_ai_battle_missile_dmg(bt, i) / (missile[m->source] + 1);
            dmg[s] -= repair[m->target];
            repair[m->target] = 0;
        }
    }
    for (int i = 1; i <= bt->items_num; ++i) {
        int j, s;
        s = (i <= bt->s[SIDE_L].items) ? SIDE_R : SIDE_L;
        j = game_ai_battle_rival(bt, i, 1);
        if (j >= 0) {
            const struct battle_item_s *b;
            b = &(bt->item[i]);
            hp[s ^ 1] += b->hp1 * b->num;
            dmg[s] += game_ai_battle_dmggive(bt, i, j, 1) * b->num;
            dmg[s] -= repair[j];
            repair[j] = 0;
        } else {
            ++hp[s ^ 1];
        }
    }
    if (bt->item[0/*planet*/].num > 0) {
        const struct battle_item_s *b = &(bt->item[0/*planet*/]);
        int j, s = (b->side == SIDE_L) ? SIDE_R : SIDE_L;
        hp[s ^ 1] += (b->hp1 * b->num * 7) / 10;
        j = game_ai_battle_rival(bt, 0/*planet*/, 1);
        if (j > 0) {
            dmg[s] += (game_ai_battle_dmggive(bt, 0/*planet*/, j, 1) * b->num * 7) / 10;
            dmg[s] -= (repair[j] * 7) / 10;
            repair[j] = 0;
        }
    }
    if (hp[SIDE_L] == 0) {
        return false;
    }
    if (hp[SIDE_R] == 0) {
        return false;
    }
    if (dmg[SIDE_L] == 0) {
        return true;
    }
    {
        int v;
        int64_t w[2];
        v = (5 - bt->g->difficulty) * 5 + 5;
        switch (bt->item[0/*planet*/].side) {
            case SIDE_L:
                v += 10;
                break;
            case SIDE_R:
                v += 20;
                break;
            default:
                break;
        }
        v += rnd_1_n(15, &bt->g->seed);
        if (bt->g->ai_id == GAME_AI_CLASSICPLUS) {
            w[SIDE_R] = (dmg[SIDE_R] * 100) / hp[SIDE_R];
            w[SIDE_L] = (dmg[SIDE_L] * 100 * v) / hp[SIDE_L];
        } else { /* WASBUG MOO1 uses 32b vars which can overflow on extreme cases */
            w[SIDE_R] = (int32_t)((((int32_t)dmg[SIDE_R]) * 100) / hp[SIDE_R]);
            w[SIDE_L] = (int32_t)((((int32_t)dmg[SIDE_L]) * 100 * v) / hp[SIDE_L]);
        }
        if (w[SIDE_R] > w[SIDE_L]) {
            return true;
        }
    }
    return false;
}

/* -------------------------------------------------------------------------- */

static uint8_t game_ai_classic_tech_next(struct game_s *g, player_id_t player, tech_field_t field, uint8_t *tbl, int num)
{
    uint8_t tech;
    if (rnd_1_n(6, &g->seed) < g->difficulty) {
        tech = tbl[num - 1];
    } else {
        tech = tbl[rnd_0_nm1(num, &g->seed)];
    }
    return tech;
}

/* -------------------------------------------------------------------------- */

static bool game_ai_classic_bomb(struct game_s *g, player_id_t player, uint8_t planet, int pop_inbound)
{
    bool flag_do_bomb;
    const planet_t *p = &(g->planet[planet]);
    if (g->eto[player].race == RACE_BULRATHI) {
        if (g->ai_id == GAME_AI_CLASSICPLUS) {
            /* OSG states 2.15:1 average kill:death ratio for +20 (+Bulrathi -Defense) advantage */
            pop_inbound = (pop_inbound * 215) / 100;
        } else {
            pop_inbound += pop_inbound / 5;
        }
    }
    flag_do_bomb = (p->pop > pop_inbound);
    if ((IS_HUMAN(g, p->owner)) && (g->evn.ceasefire[p->owner][player] > 0)) {
        flag_do_bomb = false;
    }
    return flag_do_bomb;
}

/* -------------------------------------------------------------------------- */

static void game_ai_classic_ground(struct game_s *g, player_id_t def, player_id_t att, uint8_t planet, int pop_killed, bool owner_changed)
{
    if (IS_HUMAN(g, def)) {
        return;
    }
    if (IS_AI(g, att)) {
        /*e996*/
        pop_killed = 1; /* FIXME BUG? AI-AI ground diplo always sees 1 pop killed */
        /*e9c4*/
        game_diplo_act(g, -((rnd_1_n(4, &g->seed) + 4) * pop_killed), att, def, 0xa, planet, 0);
    } else if (owner_changed && IS_HUMAN(g, att)) {
        if (g->eto[att].treaty[def] < TREATY_WAR) {
            game_diplo_act(g, -50 - rnd_1_n(50, &g->seed), att, def, 0xd, planet, 0);
            game_diplo_start_war_swap(g, def, att);
        } else {
            /*e93f*/
            game_diplo_act(g, -50 - rnd_1_n(50, &g->seed), att, def, 0xa, planet, 0);
        }
    } else {
        /*e969*/
        game_diplo_act(g, -((rnd_1_n(5, &g->seed) + 5) * pop_killed), att, def, 0xa, planet, 0);
    }
}

/* -------------------------------------------------------------------------- */

static void game_ai_classic_add_reserve(struct game_s *g, planet_t *p)
{
    empiretechorbit_t *e = &(g->eto[p->owner]);
    uint32_t v = e->reserve_bc / 5;
    e->reserve_bc -= v;
    p->reserve += v;
}

static void game_ai_classic_crank_tech(struct game_s *g, uint8_t planet)
{
    planet_t *p = &(g->planet[planet]);
    int v;
    if ((p->owner == PLAYER_NONE) || IS_HUMAN(g, p->owner)) {
        return;
    }
    v = p->slider[PLANET_SLIDER_SHIP];
    v += p->slider[PLANET_SLIDER_IND];
    v += p->slider[PLANET_SLIDER_DEF];
    p->slider[PLANET_SLIDER_TECH] += v;
    p->slider[PLANET_SLIDER_SHIP] = 0;
    p->slider[PLANET_SLIDER_IND] = 0;
    p->slider[PLANET_SLIDER_DEF] = 0;
    game_ai_classic_add_reserve(g, p);
}

static void game_ai_classic_crank_ship(struct game_s *g, uint8_t planet)
{
    planet_t *p = &(g->planet[planet]);
    int v;
    if ((p->owner == PLAYER_NONE) || IS_HUMAN(g, p->owner)) {
        return;
    }
    v = p->slider[PLANET_SLIDER_TECH];
    v += p->slider[PLANET_SLIDER_IND];
    v += p->slider[PLANET_SLIDER_DEF];
    p->slider[PLANET_SLIDER_SHIP] += v;
    p->slider[PLANET_SLIDER_TECH] = 0;
    p->slider[PLANET_SLIDER_IND] = 0;
    p->slider[PLANET_SLIDER_DEF] = 0;
    game_ai_classic_add_reserve(g, p);
}

/* -------------------------------------------------------------------------- */

static int game_ai_classic_vote_w(struct game_s *g, player_id_t player, player_id_t cand)
{
    int v = 0;
    if (g->planet[g->evn.planet_orion_i].owner == cand) {
        v += 20;
    }
    if (g->eto[cand].race == RACE_HUMAN) {
        v += 20;
    }
    if (g->evn.voted[player] == cand) {
        v += 40;
    }
    return v;
}

static bool game_ai_classic_vote_like(struct game_s *g, player_id_t player, player_id_t cand)
{
    int v;
    v = g->eto[player].relation1[cand] / 4;
    v += game_ai_classic_vote_w(g, player, cand);
    return (rnd_1_n(100, &g->seed) < v);
}

static bool game_ai_classic_vote_hate(struct game_s *g, player_id_t player, player_id_t c1, player_id_t c2)
{
    int v;
    v = -g->eto[player].relation1[c1] / 4;
    v += game_ai_classic_vote_w(g, player, c2);
    return (rnd_1_n(100, &g->seed) < v);
}

static int game_ai_classic_vote(struct election_s *el, player_id_t player)
{
    struct game_s *g = el->g;
    empiretechorbit_t *e = &(g->eto[player]);
    player_id_t c1 = el->candidate[0];
    player_id_t c2 = el->candidate[1];
    if (player == c1) {
        return 1;
    }
    if (player == c2) {
        return 2;
    }
    if (e->treaty[c1] == TREATY_ALLIANCE) {
        return 1;
    }
    if (e->treaty[c2] == TREATY_ALLIANCE) {
        return 2;
    }
    if (e->treaty[c1] == TREATY_WAR) {
        return 2;
    }
    if (e->treaty[c2] == TREATY_WAR) {
        return 1;
    }
    if (e->relation1[c1] >= 0) {
        if (game_ai_classic_vote_like(g, player, c1)) {
            return 1;
        }
    } else {
        if (game_ai_classic_vote_hate(g, player, c1, c2)) {
            return 2;
        }
    }
    if (e->relation1[c2] >= 0) {
        if (game_ai_classic_vote_like(g, player, c2)) {
            return 2;
        }
    } else {
        int v;
        v = -g->eto[player].relation1[c2] / 5;  /* FIXME BUG? the other case uses / 4 */
        v += game_ai_classic_vote_w(g, player, c1);
        if (rnd_1_n(100, &g->seed) < v) {
            return 1;
        }
    }
    return 0;
}

/* -------------------------------------------------------------------------- */

static int game_ai_classic_diplo_wage_war_fleet_w(struct game_s *g, player_id_t p1, player_id_t p2)
{
    int ratio, fleetw[2];
    for (int i = 0; i < 2; ++i) {
        player_id_t player;
        empiretechorbit_t *e;
        shipresearch_t *srd;
        player = (i == 0) ? p1 : p2;
        e = &(g->eto[player]);
        srd = &(g->srd[player]);
        fleetw[i] = 0;
        for (int j = 0; j < e->shipdesigns_num; ++j) {
            uint32_t v;
            v = srd->shipcount[j] * game_num_tbl_hull_w[srd->design[j].hull];
            ADDSATT(fleetw[i], v, 3250000);
        }
        SETMAX(fleetw[i], 1);
    }
    ratio = ((fleetw[0] - fleetw[1]) * 100) / fleetw[1];
    SETRANGE(ratio, -300, 300);
    if (ratio == 0) {
        return 0;
    } else if (ratio < 0) {
        return -rnd_1_n(-ratio, &g->seed);
    } else {
        return rnd_1_n(ratio, &g->seed);
    }
}

static void game_ai_classic_diplo_wage_war_do(struct game_s *g, player_id_t p1, player_id_t p2)
{
    empiretechorbit_t *e1 = &(g->eto[p1]);
    empiretechorbit_t *e2 = &(g->eto[p2]);
    if ((e1->treaty[p2] == TREATY_ALLIANCE) || (e1->treaty[p2] == TREATY_NONAGGRESSION)) {
        if (g->difficulty < DIFFICULTY_AVERAGE) {
            return;
        }
        if (0
          || ((e1->treaty[p2] == TREATY_ALLIANCE) && ((!rnd_0_nm1(4, &g->seed)) || ((!rnd_0_nm1(2, &g->seed)) && (e2->trait2 == TRAIT2_EXPANSIONIST))))
          || ((e1->treaty[p2] == TREATY_NONAGGRESSION) && ((!rnd_0_nm1(2, &g->seed)) || (e2->trait2 == TRAIT2_EXPANSIONIST)))
        ) {
            game_diplo_act(g, -10000, p1, p2, 32, 0, 0);
            game_diplo_break_treaty(g, p2, p1);
            if (e1->relation1[p2] > 30) {
                e1->relation1[p2] = 30;
                e2->relation1[p1] = 30;
            }
        }
    } else if (g->evn.ceasefire[p1][p2] <= 0) {
        game_diplo_act(g, -10000, p1, p2, (e1->relation1[p2] < 0) ? 13 : 60, 0, 0);
        game_diplo_start_war(g, p2, p1);
    }
}

static int game_ai_classic_diplo_wage_war_prod_w(struct game_s *g, player_id_t p1, player_id_t p2)
{
    int ratio, prod[2];
    prod[0] = g->eto[p1].total_production_bc;
    prod[1] = g->eto[p2].total_production_bc;
    SETRANGE(prod[1], 1, 3250000);  /* FIXME BUG? only p2 prod limited */
    ratio = ((prod[0] - prod[1]) * 100) / prod[1];
    SETRANGE(ratio, -300, 300);
    if (ratio == 0) {
        return 0;
    } else if (ratio < 0) {
        return -rnd_1_n(-ratio, &g->seed);
    } else {
        return rnd_1_n(ratio, &g->seed);
    }
}

static void game_ai_classic_diplo_wage_war(struct game_s *g, player_id_t p1, player_id_t p2)
{
    if (g->end != GAME_END_NONE) {
        for (p1 = PLAYER_0; p1 < g->players; ++p1) {
            empiretechorbit_t *e1 = &(g->eto[p1]);
            for (p2 = PLAYER_0; p2 < g->players; ++p2) {
                if ((p1 != p2) && (BOOLVEC_IS1(g->refuse, p1) == BOOLVEC_IS1(g->refuse, p2))) {
                    e1->treaty[p2] = TREATY_ALLIANCE;
                }
            }
        }
    } else {
        empiretechorbit_t *e1 = &(g->eto[p1]);
        empiretechorbit_t *e2 = &(g->eto[p2]);
        if ((e1->treaty[p2] >= TREATY_WAR) || BOOLVEC_IS0(e1->contact, p2) || (!IS_ALIVE(g, p1))) {
            return;
        }
        if (1
          && (e2->trait1 == TRAIT1_ERRATIC)
          && (rnd_1_n(300, &g->seed) <= g->difficulty)
          && (IS_AI(g, p1) || (g->evn.ceasefire[p1][p2] < 1))
        ) {
            e1->diplo_type[p2] = 61;
            e1->diplo_val[p2] = 2000;
            game_diplo_start_war(g, p2, p1);
        } else {
            if (1
              && (!rnd_0_nm1(20, &g->seed))
              && ((e2->trait2 == TRAIT2_MILITARIST) || (e2->trait2 == TRAIT2_EXPANSIONIST))
              && (e2->trait1 != TRAIT1_HONORABLE)
              && (IS_AI(g, p1) || (g->evn.ceasefire[p1][p2] < 1))
            ) {
                int v;
                v = game_ai_classic_diplo_wage_war_fleet_w(g, p1, p2);
                v = e1->relation1[p2] - v + game_diplo_tbl_reldiff[e2->trait1] + e1->trust[p2];
                if (v < -150) {
                    game_ai_classic_diplo_wage_war_do(g, p1, p2);
                }
            }
            /*1679f*/
            if (1
              && (!rnd_0_nm1(20, &g->seed))
              && (IS_AI(g, p1) || (g->evn.ceasefire[p1][p2] < 1))
            ) {
                int v;
                v = game_ai_classic_diplo_wage_war_prod_w(g, p1, p2);
                v = e1->relation1[p2] - v + game_diplo_tbl_reldiff[e2->trait1] + e1->trust[p2];
                if (v < -150) {
                    game_ai_classic_diplo_wage_war_do(g, p1, p2);
                }
            }
        }
        /*16829*/
        for (player_id_t p3 = PLAYER_0; p3 < g->players; ++p3) {
            if (IS_HUMAN(g, p3)) {
                continue;
            }
            for (player_id_t p4 = PLAYER_0; p4 < g->players; ++p4) {
                empiretechorbit_t *e4 = &(g->eto[p4]);
                if (IS_AI(g, p4)) {
                    continue;
                }
                if (1
                  && (e4->treaty[p3] == TREATY_WAR)
                  && (e2->treaty[p3] == TREATY_ALLIANCE)
                  && (g->evn.ceasefire[p4][p2] <= 0)
                  && (!rnd_0_nm1(10, &g->seed))
                  && (e2->treaty[p1] == TREATY_ALLIANCE)
                ) {
                    game_ai_classic_diplo_wage_war_do(g, p1, p2);
                }
            }
        }
        if (IS_HUMAN(g, p1) && (g->difficulty >= DIFFICULTY_AVERAGE)) {
            int num = 0;
            for (player_id_t p3 = PLAYER_0; p3 < g->players; ++p3) {
                if (e1->treaty[p3] >= TREATY_WAR) {
                    ++num;
                }
            }
            if (num < g->difficulty) {
                /* MOO1 does unused buggy count of planets ; overwrites local variable at tbl[-1] (which is also unused) */
                int v = e1->relation1[p2];
                if (v < -30) {
                    v = (-v) / 10;
                    if (rnd_1_n(10, &g->seed) <= v) {
                        game_ai_classic_diplo_wage_war_do(g, p1, p2);
                    }
                }
            }
        }
    }
}

static void game_ai_classic_turn_diplo_p1_sub1(struct game_s *g)
{
    for (player_id_t p1 = PLAYER_0; p1 < g->players; ++p1) {
        empiretechorbit_t *e1 = &(g->eto[p1]);
        if (IS_HUMAN(g, p1)) {
            continue;
        }
        for (player_id_t p2 = p1 + 1; p2 < g->players; ++p2) {
            empiretechorbit_t *e2 = &(g->eto[p2]);
            if (IS_HUMAN(g, p2)) {
                continue;
            }
            for (player_id_t p3 = p1 + 1; p2 < g->players; ++p2) {  /* FIXME BUG? p2 + 1 ; BUG p3 < ; BUG ++p3 */
                if (IS_HUMAN(g, p3)) {
                    continue;
                }
                if ((e2->treaty[p3] == TREATY_WAR) && (e1->treaty[p2] == TREATY_ALLIANCE) && (e1->treaty[p3] == TREATY_ALLIANCE)) {
                    game_diplo_break_treaty(g, p1, p2);
                    game_diplo_break_treaty(g, p1, p3);
                }
            }
        }
    }
}

static int game_ai_classic_turn_diplo_p1_get_ai_trade_tech(struct game_s *g, player_id_t p1, player_id_t p2, tech_field_t *fieldptr, uint8_t *techptr)
{
    struct spy_esp_s s;
    tech_field_t field = 0;
    uint8_t tech = 0;
    int num, r;
    s.target = p1;
    s.spy = p2;
    r = rnd_1_n(3, &g->seed);
    num = game_spy_esp_sub2(g, &s, r);
    /* WASBUG
       MOO1 does field = s.tbl_field[0] and tech = s.tbl_tech2[0] but neither is set by game_spy_esp_sub2.
       MOO1 uses global variables for the tables; the values are from some previous spy call (possibly ground combat tech steal).
    */
    if (num > 0) {
        int i;
        i = (g->year ^ r) % num; /* "random" value without advancing the rng */
        for (tech_field_t f = 0; f < TECH_FIELD_NUM; ++f) {
            int n;
            n = s.tbl_num[f];
            if (i < n) {
                field = f;
                tech = s.tbl_techi[f][i];
                break;
            }
            i -= n;
        }
    }
    *fieldptr = field;
    *techptr = tech;
    return num;
}

static void game_ai_classic_turn_diplo_p1(struct game_s *g)
{
    for (player_id_t p1 = PLAYER_0; p1 < g->players; ++p1) {
        empiretechorbit_t *e1 = &(g->eto[p1]);
        if (IS_HUMAN(g, p1)) {
            continue;
        }
        for (player_id_t p2 = PLAYER_0; p2 < g->players; ++p2) {
            empiretechorbit_t *e2 = &(g->eto[p2]);
            if ((p1 == p2) || IS_HUMAN(g, p2)) {
                continue;
            }
            if ((!(rnd_0_nm1(15 - g->difficulty * 2, &g->seed))) && BOOLVEC_IS1(e1->contact, p2)) {
                int v;
                v = e1->trust[p2] + e1->relation1[p2] + game_diplo_tbl_reldiff[e2->trait1] + rnd_1_n(100, &g->seed);
                if (e1->treaty[p2] == TREATY_NONAGGRESSION) {
                    v += 10;
                }
                if (e1->treaty[p2] == TREATY_ALLIANCE) {
                    v += 20;
                }
                if (e1->trade_bc[p2] != 0) {
                    v += 10;
                }
                if (((v + e1->mood_treaty[p2]) > 150) && (e1->treaty[p2] != TREATY_ALLIANCE)) {
                    game_diplo_set_treaty(g, p1, p2, TREATY_ALLIANCE);
                } else if (((v + e1->mood_treaty[p2]) > 150) && (e1->treaty[p2] != TREATY_NONAGGRESSION)) { /* BUG ignores Alliance, results in NAP/Alliance oscillation */
                    game_diplo_set_treaty(g, p1, p2, TREATY_NONAGGRESSION);
                } else {
                    if ((e1->mood_tech[p2] + 80) < v) {
                        tech_field_t field[2];
                        int num[2];
                        uint8_t tech[2];
                        num[0] = game_ai_classic_turn_diplo_p1_get_ai_trade_tech(g, p1, p2, &field[0], &tech[0]);
                        num[1] = game_ai_classic_turn_diplo_p1_get_ai_trade_tech(g, p2, p1, &field[1], &tech[1]);
                        if ((num[0] > 0) && (num[1] > 0)) {
                            game_tech_get_new(g, p2, field[0], tech[0], TECHSOURCE_TRADE, 0, PLAYER_NONE, false);
                            game_tech_get_new(g, p1, field[1], tech[1], TECHSOURCE_TRADE, 0, PLAYER_NONE, false);
                        }
                    } else if (v > 70) {
                        int v1, v2;
                        v1 = e1->total_production_bc / 4;
                        v2 = e2->total_production_bc / 4;
                        SETMIN(v1, v2);
                        SETMIN(v1, 32000);
                        game_diplo_set_trade(g, p1, p2, v1);
                    }
                }
                if (!(rnd_0_nm1(20, &g->seed))) {
                    game_diplo_act(g, rnd_1_n(5, &g->seed), p1, p2, 1, 0, 0);
                }
                /* WASBUG
                   MOO1 uses the last audience message variant index (diplo_msg_subtype, -1..13) in place of v.
                   This is weird, nondeterministic (in MOO1) and unworkable for multiplayer.
                   Whether v is what was intended is unknown, but it makes a whole lot more sense.
                */
                if (g->ai_id == GAME_AI_CLASSIC) {
                    v = g->evn.diplo_msg_subtype;
                }
                if ((e1->treaty[p2] == TREATY_WAR) && ((v + e1->mood_peace[p2]) > 100)) {
                    game_diplo_stop_war(g, p1, p2);
                }
                game_diplo_annoy(g, p1, p2, 3);
                game_ai_classic_turn_diplo_p1_sub1(g);
                game_ai_classic_diplo_wage_war(g, p1, p2);
            }
        }
    }
    for (player_id_t p1 = PLAYER_0; p1 < g->players; ++p1) {
        empiretechorbit_t *e1 = &(g->eto[p1]);
        if (IS_HUMAN(g, p1)) {
            continue;
        }
        for (player_id_t p2 = PLAYER_0; p2 < g->players; ++p2) {
            if ((p1 == p2) || IS_HUMAN(g, p2)) {
                continue;
            }
            if (e1->treaty[p2] == TREATY_ALLIANCE) {
                for (player_id_t p3 = PLAYER_0; p3 < g->players; ++p3) {
                    empiretechorbit_t *e3 = &(g->eto[p3]);
                    if ((p1 != p3) && (p2 != p3) && IS_AI(g, p3) && (e3->treaty[p1] == TREATY_WAR) && (e3->treaty[p2] != TREATY_WAR)) {
                        game_diplo_start_war(g, p2, p3);
                    }
                }
            }
        }
    }
}

/* -------------------------------------------------------------------------- */

static void game_ai_classic_turn_diplo_p2_sub1(struct game_s *g, player_id_t p1, player_id_t p2)
{
    empiretechorbit_t *e1 = &(g->eto[p1]);
    empiretechorbit_t *e2 = &(g->eto[p2]);
    int v, v4;
    if (BOOLVEC_IS0(e1->contact, p2) || (e1->treaty[p2] >= TREATY_WAR)) { /* WASBUG? MOO1 also tests for "|| (e1->diplo_val == 0)" ; note the missing [p2] */
        e1->diplo_type[p2] = 0;
        return;
    }
    if (game_fleet_any_dest_player(g, p2, p1)) {
        e1->diplo_type[p2] = 0;
        return;
    }
    if ((e1->diplo_type[p2] != 0) && (!rnd_0_nm1(2, &g->seed))) {
        return;
    }
    v4 = 0;
    v = e1->trust[p2] + e1->relation1[p2] + ((e1->race == RACE_HUMAN) ? 50 : 0) + game_diplo_tbl_reldiff[e2->trait1];
    if ((e1->treaty[p2] == TREATY_NONE) && (e1->relation1[p2] > 15)) {
        int v8;
        v8 = v + rnd_1_n(100, &g->seed) + e1->mood_treaty[p2];
        if (v8 > 50) {
            v4 = 1;
            e1->diplo_type[p2] = 24;
        }
    }
    /*41684*/
    if (((e1->treaty[p2] == TREATY_NONAGGRESSION) || (v4 == 1)) && (e1->relation1[p2] > 50)) {
        int v8;
        v8 = v + rnd_1_n(100, &g->seed) + e1->mood_treaty[p2];
        if (v8 > 100) {
            v4 = 2;
            e1->diplo_type[p2] = 25;
        }
    }
    /*416e8*/
    if (v4 == 0) {
        int v8;
        v8 = v + rnd_1_n(150, &g->seed) + e1->mood_trade[p2];
        if (v8 > 50) {
            int prod, want_trade;
            prod = MIN(e1->total_production_bc, e2->total_production_bc) / 4;
            SETMIN(prod, 32000);
            want_trade = prod / 25;
            if (want_trade != 0) {
                want_trade = rnd_1_n(want_trade, &g->seed) * 25;
            }
            if (e1->trade_bc[p2] < want_trade) {
                v4 = 3;
                e1->diplo_type[p2] = 26;
                e1->au_want_trade[p2] = want_trade;
            }
        }
    }
    /*417f2*/
    if (v4 == 0) {
        player_id_t pa = PLAYER_NONE;
        for (player_id_t p3 = PLAYER_0; p3 < g->players; ++p3) {
            if ((p3 != p1) && (p3 != p2) && (e2->treaty[p3] >= TREATY_WAR) && BOOLVEC_IS1(e1->contact, p3)) {
                pa = p3;
            }
        }
        if ((pa != PLAYER_NONE) && (!rnd_0_nm1(10, &g->seed)) && (e1->attack_bounty[p2] == PLAYER_NONE)) {
            struct spy_esp_s s[1];
            int num, bc;
            s->spy = p1;
            s->target = p2;
            num = game_spy_esp_sub1(g, s, 0, 2);
            /*game_spy_esp_get_random(g, s, &field, &tech); unused */
            bc = 0;
            if (num == 0) {
                bc = (((rnd_1_n(3, &g->seed) + 1) * g->year) / 50) * 50;
                s->tbl_tech2[0] = 0;
            }
            if ((bc != 0) || (num != 0)) {
                v4 = 4;
                e1->diplo_type[p2] = 28;
                e1->attack_bounty[p2] = pa;
                e1->attack_gift_field[p2] = s->tbl_field[0];
                e1->attack_gift_tech[p2] = s->tbl_tech2[0];
                e1->attack_gift_bc[p2] = bc;
                e1->bounty_collect[p2] = PLAYER_NONE;
            }
        }
    }
    /*418d2*/
    if (v4 == 0) {
        int v8;
        v8 = v + rnd_1_n(100, &g->seed) + e1->mood_trade[p2];
        if (v8 > 25) {
            struct spy_esp_s s[1];
            int v1c, v14, num;
            v1c = e1->mood_tech[p2];
            if (v1c > 0) {
                v1c /= 5;
            }
            SETMIN(v1c, 30);
            if (e1->treaty[p2] == TREATY_ALLIANCE) {
                v1c += 25;
            }
            v = e1->trust[p2] + e1->relation1[p2] / 2 + ((e2->race == RACE_HUMAN) ? 50 : 0) + game_diplo_tbl_reldiff[e2->trait1] + v1c - 125;
            v8 = v + rnd_1_n(100, &g->seed) + v1c - 125;
            if (v8 < 0) {
                v14 = abs(v8) + 100;
            } else {
                v14 = 20000 / (v8 + 200);
            }
            SETMIN(v14, 50);
            s->spy = p2;
            s->target = p1;
            num = game_spy_esp_sub1(g, s, 0, 0);
            if (num > 0) {
                bool found;
                tech_field_t zfield;
                uint8_t ztech;
                int zvalue;
                zfield = s->tbl_field[0];
                ztech = s->tbl_tech2[0];
                zvalue = (s->tbl_value[0] * 100) / v14;
                s->spy = p1;
                s->target = p2;
                num = game_spy_esp_sub1(g, s, zvalue, 1);
                found = false;
                for (int i = 0; i < num; ++i) {
                    if (s->tbl_value[i] <= zvalue) {
                        found = true;
                        break;
                    }
                }
                if (found) {
                    int n = 0;
                    v4 = 5;
                    e1->diplo_type[p2] = 29;
                    e1->au_want_field[p2] = zfield;
                    e1->au_want_tech[p2] = ztech;
                    for (int i = 0; i < num; ++i) {
                        if (s->tbl_value[i] <= zvalue) {
                            e1->au_tech_trade_field[p2][n] = s->tbl_field[i];
                            e1->au_tech_trade_tech[p2][n] = s->tbl_tech2[i];
                            ++n;
                        }
                    }
                    e1->au_tech_trade_num[p2] = n;
                    e1->mood_tech[p2] = 0;
                    e2->mood_tech[p1] = 0;
                }
            }
        }
    }
    /*41aa2*/
    if (v4 == 0) {
        if ((e1->relation1[p2] < 0) && (e1->total_production_bc > e2->total_production_bc) && (e1->treaty[p2] == TREATY_NONE)) {
            v4 = 1;
            e1->diplo_type[p2] = 24;
        }
    }
    /*41ae2*/
    /*41c23*/
    if (1
      && (e2->total_production_bc != 0)
      && ((v4 == 1) || (v4 == 2))
      && (e2->trait1 > TRAIT1_RUTHLESS)
      && (rnd_1_n(100, &g->seed) <= ((e1->total_production_bc * 30) / e2->total_production_bc))
      && (!rnd_0_nm1(8, &g->seed))
    ) {
        if (rnd_0_nm1(4, &g->seed)) {
            e1->offer_bc[p2] = ((e2->total_production_bc / (rnd_1_n(3, &g->seed) + 1) + g->year) / 25) * 25;
        } else {
            struct spy_esp_s s[1];
            s->spy = p1;
            s->target = p2;
            if (game_spy_esp_sub1(g, s, 0, 2) > 0) {
                e1->offer_field[p2] = s->tbl_field[0];
                e1->offer_tech[p2] = s->tbl_tech2[0];
            }
        }
    }
}

static void game_ai_classic_turn_diplo_p2_sub2(struct game_s *g, player_id_t p1, player_id_t p2)
{
    empiretechorbit_t *e1 = &(g->eto[p1]);
    if ((e1->treaty[p2] == TREATY_ALLIANCE) && (!rnd_0_nm1(10, &g->seed))) {
        player_id_t pa = PLAYER_NONE;
        for (player_id_t p3 = PLAYER_0; p3 < g->players; ++p3) {
            empiretechorbit_t *e3 = &(g->eto[p3]);
            if (1
              && (p3 != p1) && (p3 != p2) && (e3->treaty[p2] == TREATY_WAR)
              && BOOLVEC_IS1(e1->contact, p3)
              && (e1->treaty[p3] != TREATY_WAR)
            ) {
                pa = p3;
            }
        }
        e1->au_ally_attacker[p2] = pa;
        e1->diplo_type[p2] = (pa == PLAYER_NONE) ? 0 : 76;
    } else {
        e1->diplo_type[p2] = 0;
    }
}

static void game_ai_classic_turn_diplo_p2_sub3(struct game_s *g, player_id_t p1, player_id_t p2)
{
    empiretechorbit_t *e1 = &(g->eto[p1]);
    empiretechorbit_t *e2 = &(g->eto[p2]);
    int v;
    if (BOOLVEC_IS0(e1->contact, p2)) { /* WASBUG? MOO1 also tests for "|| (e1->diplo_val == 0)" ; note the missing [p2] */
        e1->diplo_type[p2] = 0;
        return;
    }
    v = e1->trust[p2] + e1->relation1[p2] + ((e1->race == RACE_HUMAN) ? 50 : 0) + game_diplo_tbl_reldiff[e2->trait1];
    if (e1->treaty[p2] < TREATY_WAR) {
        if (e1->relation1[p2] <= -95) {
            game_diplo_start_war(g, p2, p1);
            e1->diplo_type[p2] = 13;
        } else if ((v - rnd_1_n(100, &g->seed) + e1->mood_treaty[p2]) <= -100) {
            if (e1->hatred[p2] > 0) {
                ++e1->hatred[p2];
                if ((e1->hatred[p2] > 2) || (e1->treaty[p2] == TREATY_NONE)) {
                    if (rnd_1_n(100, &g->seed) < -(game_diplo_tbl_reldiff[e2->trait1] / 2 + e1->diplo_val[p2])) {
                        game_diplo_start_war(g, p2, p1);
                        e1->diplo_type[p2] = 13;
                        e1->hatred[p2] = 1;
                    }
                } else {
                    e1->diplo_type[p2] += 46;
                    game_diplo_break_treaty(g, p2, p1);
                }
            } else {
                e1->hatred[p2] = 1;
                if ((e1->treaty[p2] == TREATY_NONAGGRESSION) || (e1->treaty[p2] == TREATY_ALLIANCE) || (e1->trade_bc[p2] != 0)) {
                    e1->diplo_type[p2] += 38;
                }
            }
        }
    } else if (((v + rnd_1_n(100, &g->seed) + e1->mood_peace[p2]) >= 50) && (!rnd_0_nm1(8, &g->seed))) {
        e1->diplo_type[p2] = 30;
        if (rnd_1_n(100, &g->seed) <= ((e2->total_production_bc * 30) / e1->total_production_bc)) {
            if (rnd_0_nm1(4, &g->seed)) {
                e1->offer_bc[p2] = ((e2->total_production_bc / (rnd_1_n(3, &g->seed) + 1) + g->year) / 25) * 25;
            } else {
                struct spy_esp_s s[1];
                s->spy = p1;
                s->target = p2;
                if (game_spy_esp_sub1(g, s, 0, 2) > 0) {
                    e1->offer_field[p2] = s->tbl_field[0];
                    e1->offer_tech[p2] = s->tbl_tech2[0];
                }
            }
        }
    }
}

static void game_ai_classic_turn_diplo_p2(struct game_s *g)
{
    game_diplo_limit_mood_treaty(g);
    for (player_id_t p1 = PLAYER_0; p1 < g->players; ++p1) {
        empiretechorbit_t *e1 = &(g->eto[p1]);
        if (IS_AI(g, p1)) {
            continue;
        }
        if (!IS_ALIVE(g, p1)) {
            memset(e1->diplo_type, 0, sizeof(e1->diplo_type));
            continue;
        }
        for (player_id_t p2 = PLAYER_0; p2 < g->players; ++p2) {
            empiretechorbit_t *e2 = &(g->eto[p2]);
            if (IS_HUMAN(g, p2)) {
                continue;
            }
            if (!IS_ALIVE(g, p2)) {
                e1->diplo_type[p2] = 0;
                continue;
            }
            if ((e1->treaty[p2] == TREATY_FINAL_WAR) || (e1->diplo_type[p2] == 59)) {
                if (e1->diplo_type[p2] != 59) {
                    e1->diplo_type[p2] = 0;
                }
            } else if (e1->have_met[p2] == 1) {
                e1->have_met[p2] = 2;
                e1->diplo_type[p2] = e2->trait1 + 14;
            } else if (e1->bounty_collect[p2] != PLAYER_NONE) {
                e1->diplo_type[p2] = 58;
            } else if ((e1->diplo_type[p2] == 13) || (e1->diplo_type[p2] == 32) || (e1->diplo_type[p2] == 61) || (e1->diplo_type[p2] == 60)) {
                /*v8 = 0; unused */
            } else {
                /*16441*/
                int16_t v, v2, dv2;
                v = game_diplo_get_mood(g, p1, p2);
                v2 = v + e1->trust[p2] + game_diplo_tbl_reldiff[e2->trait1];
                dv2 = e1->diplo_val[p2] * 2;
                if ((v2 <= -100) || (v <= -100)) {
                    e1->diplo_type[p2] = 0;
                } else if (e1->diplo_val[p2] < 0) {
                    if ((e1->hatred[p2] > 0) || (rnd_1_n(100, &g->seed) < abs(dv2))) {
                        game_ai_classic_turn_diplo_p2_sub3(g, p1, p2);
                    } else {
                        e1->diplo_type[p2] = 0;
                    }
                } else if ((e1->treaty[p2] == TREATY_WAR) && ((rnd_1_n(100, &g->seed) + 30) < e1->mood_peace[p2])) {
                    game_ai_classic_turn_diplo_p2_sub3(g, p1, p2);
                } else if ((rnd_1_n(100, &g->seed) < (dv2 + ((e1->hatred[p2] == 0) ? 3 : 0))) && (!rnd_0_nm1(4, &g->seed))) {
                    if (e1->hatred[p2] > 0) {
                        e1->hatred[p2] = 0;
                    } else {
                        game_ai_classic_turn_diplo_p2_sub1(g, p1, p2);
                    }
                } else {
                    /*1656f*/
                    game_ai_classic_turn_diplo_p2_sub2(g, p1, p2);
                }
            }
            /*16576*/
            if (e1->diplo_type[p2] == 0) {
                game_ai_classic_diplo_wage_war(g, p1, p2);
            }
            if ((e1->diplo_type[p2] == 2) && rnd_0_nm1(10, &g->seed)) {
                e1->diplo_type[p2] = 0;
            }
        }
    }
}

/* -------------------------------------------------------------------------- */

static int game_ai_classic_aud_check_mood(struct audience_s *au, int a0, int a2)
{
    struct game_s *g = au->g;
    player_id_t ph = au->ph, pa = au->pa;
    empiretechorbit_t *eh = &(g->eto[ph]);
    empiretechorbit_t *ea = &(g->eto[pa]);
    int v;
    switch (a2) {
        default:
        case 0:
            v = eh->mood_treaty[pa];
            break;
        case 1:
            v = eh->mood_trade[pa];
            break;
        case 2:
            v = eh->mood_peace[pa];
            break;
        case 3:
            v = eh->mood_tech[pa];
            break;
    }
    v += eh->trust[pa] + eh->relation1[pa] + ((eh->race == RACE_HUMAN) ? 50 : 0) + game_diplo_tbl_reldiff[ea->trait1];
    v += rnd_1_n(100, &g->seed);
    v -= a0;
    if (eh->treaty[pa] == TREATY_ALLIANCE) {
        v += 40;
    }
    game_diplo_annoy(g, ph, pa, 1);
    switch (a2) {
        default:
        case 0:
            eh->mood_treaty[pa] -= rnd_1_n(30, &g->seed) + 20;
            break;
        case 1:
            eh->mood_trade[pa] -= rnd_1_n(30, &g->seed) + 20;
            break;
        case 2:
            eh->mood_peace[pa] -= rnd_1_n(50, &g->seed) + 50;
            break;
        case 3:
            eh->mood_tech[pa] -= rnd_1_n(50, &g->seed) + 20;
            break;
    }
    if (v < -75) {
        return 0;
    } else if (v < -50) {
        return 1;
    } else if (v < 0) {
        return 2;
    } else {
        return 3;
    }
}

static void game_ai_classic_aud_start_human(struct audience_s *au)
{
    struct game_s *g = au->g;
    player_id_t ph = au->ph, pa = au->pa;
    empiretechorbit_t *eh = &(g->eto[ph]);
    empiretechorbit_t *ea = &(g->eto[pa]);
    int v;
    v = eh->trust[pa] + game_diplo_get_mood(g, ph, pa) + game_diplo_tbl_reldiff[ea->trait1];
    if (v < -100) {
        au->dtype = (eh->treaty[pa] >= TREATY_WAR) ? 20 : 21;
        au->mode = 1;
    } else {
        v += eh->relation1[pa];
        au->dtype = (v > -50) ? 22 : 23;
        au->mode = 0;
    }
}

static int game_ai_classic_aud_treaty_nap(struct audience_s *au)
{
    struct game_s *g = au->g;
    player_id_t ph = au->ph, pa = au->pa;
    empiretechorbit_t *eh = &(g->eto[ph]);
    int si;
    if (eh->relation1[pa] > 10) {
        si = game_ai_classic_aud_check_mood(au, 50, 0);
    } else {
        si = 0;
    }
    return si;
}

static int game_ai_classic_aud_treaty_alliance(struct audience_s *au)
{
    struct game_s *g = au->g;
    player_id_t ph = au->ph, pa = au->pa;
    empiretechorbit_t *eh = &(g->eto[ph]);
    int si;
    if (eh->relation1[pa] > 50) {
        si = game_ai_classic_aud_check_mood(au, 125, 0);
    } else {
        si = 0;
    }
    return si;
}

static int game_ai_classic_aud_treaty_peace(struct audience_s *au)
{
    int si;
    int mi = (au->g->ai_id == GAME_AI_CLASSICPLUS) ? 2/*peace*/ : 0/*treaty*/;  /* WASBUG? mood_peace makes sense */
    si = game_ai_classic_aud_check_mood(au, 60, mi);
    return si;
}

static int game_ai_classic_aud_treaty_declare_war(struct audience_s *au)
{
    struct game_s *g = au->g;
    player_id_t ph = au->ph, pa = au->pa;
    empiretechorbit_t *eh = &(g->eto[ph]);
    empiretechorbit_t *ea = &(g->eto[pa]);
    int si;
    if ((eh->treaty[pa] == TREATY_ALLIANCE) && (eh->treaty[au->pstartwar] == TREATY_WAR)) {
        si = (!rnd_0_nm1(8, &g->seed)) ? 2 : 3;
    } else {
        int v = ea->relation1[au->pstartwar] + 150;
        if (g->ai_id != GAME_AI_CLASSIC) {
            /* WASBUG?
               MOO1 contains the treaty * 30 code but the result is unused.
               OSG states that either NAP or alliance changes that 150 to 250 but there is no such code.
            */
            v += ea->treaty[au->pstartwar] * 30;
        }
        si = game_ai_classic_aud_check_mood(au, v, 0);
    }
    return si;
}

static int game_ai_classic_aud_treaty_break_alliance(struct audience_s *au)
{
    struct game_s *g = au->g;
    player_id_t ph = au->ph, pa = au->pa;
    empiretechorbit_t *eh = &(g->eto[ph]);
    empiretechorbit_t *ea = &(g->eto[pa]);
    int si;
    if (eh->relation1[pa] > 24) {
        si = game_ai_classic_aud_check_mood(au, ea->relation1[au->pwar] + 175, 0);
    } else {
        si = 0;
    }
    return si;
}

static int game_ai_classic_aud_trade(struct audience_s *au)
{
    int si;
    si = game_ai_classic_aud_check_mood(au, 50, 1);
    return si;
}

static bool game_ai_classic_aud_sweeten(struct audience_s *au, int *bcptr, tech_field_t *fieldptr, uint8_t *techptr)
{
    struct game_s *g = au->g;
    player_id_t ph = au->ph, pa = au->pa;
    empiretechorbit_t *eh = &(g->eto[ph]);
    int bc;
    tech_field_t field = 0;
    uint8_t tech;
    bc = (((rnd_1_n(8, &g->seed) + rnd_1_n(8, &g->seed)) * g->year) / 25) * 25;
    if (bc > eh->reserve_bc) {
        bc = 0;
    }
    {
        struct spy_esp_s s[1];
        s->spy = pa;
        s->target = ph;
        if (game_spy_esp_sub1(g, s, g->year, 1) > 0) {
            field = s->tbl_field[0];
            tech = s->tbl_tech2[0];
        } else {
            tech = 0;
        }
    }
    *bcptr = bc;
    *fieldptr = field;
    *techptr = tech;
    if ((bc == 0) && (tech == 0)) {
        return false;
    }
    return true;
}

static uint8_t game_ai_classic_aud_threaten(struct audience_s *au)
{
    struct game_s *g = au->g;
    player_id_t ph = au->ph, pa = au->pa;
    empiretechorbit_t *eh = &(g->eto[ph]);
    empiretechorbit_t *ea = &(g->eto[pa]);
    int v;
    uint8_t dtype;
    v = rnd_1_n(200, &g->seed) + eh->mood_treaty[pa] / 2;
    v += game_diplo_tbl_reldiff[ea->trait1] * 2;
    if (ea->total_production_bc > 0) {
        v += (eh->total_production_bc * 100) / ea->total_production_bc;
    } else {
        v += 100;
    }
    SUBSATT(eh->relation1[pa], rnd_1_n(15, &g->seed), -100);
    ea->relation1[ph] = eh->relation1[pa];
    eh->mood_treaty[pa] = -120;
    if (v < 170) {
        if ((rnd_1_n(15, &g->seed) - game_diplo_tbl_reldiff[ea->trait1]) > rnd_1_n(100, &g->seed)) {
            game_diplo_start_war(g, ph, pa);
            dtype = 13;
        } else {
            dtype = 69;
        }
    } else {
        if (g->ai_id == GAME_AI_CLASSICPLUS) {
            ea->spymode_next[ph] = SPYMODE_HIDE;
            ea->spymode[ph] = SPYMODE_HIDE;
        } else { /* WASBUG MOO1 hides your spies */
            eh->spymode_next[pa] = SPYMODE_HIDE;
            eh->spymode[pa] = SPYMODE_HIDE;
        }
        g->evn.ceasefire[ph][pa] = rnd_1_n(15, &g->seed) + 5;
        dtype = 70;
        if (v >= 275) {
            struct spy_esp_s s[1];
            s->spy = ph;
            s->target = pa;
            if (game_spy_esp_sub1(g, s, 0, 1) > 0) {
                au->tribute_field = s->tbl_field[0];
                au->tribute_tech = s->tbl_tech2[0];
                game_tech_get_new(g, ph, au->tribute_field, au->tribute_tech, TECHSOURCE_TRADE, pa, PLAYER_NONE, false); /* WASBUG? pa was 0 */
            }
        } else if (v >= 200) {
            int bc;
            bc = (((rnd_1_n(8, &g->seed) + 2) * g->year) / 25) * 25;
            if (bc != 0) {
                eh->reserve_bc += bc;
                au->tribute_bc = bc;
                dtype = 71;
            }
        }
    }
    return dtype;
}

static void game_ai_classic_aud_tribute_bc(struct audience_s *au, int selected, int bc)
{
    struct game_s *g = au->g;
    player_id_t ph = au->ph, pa = au->pa;
    empiretechorbit_t *eh = &(g->eto[ph]);
    empiretechorbit_t *ea = &(g->eto[pa]);
    int v;
    if (ea->total_production_bc != 0) {
        /* WASBUG MOO1 uses the value
              (reserve * 12) / (0x786e + 0xdd4 * pa + 0x326b0000)
           == (reserve * 12) / ea->relation1
           == 0
           This makes money tributes quite useless.
           The formula below is according to OSG.
        */
        if (g->ai_id == GAME_AI_CLASSICPLUS) {
            v = (bc * 12) / ea->total_production_bc;
        } else {
            v = 0;
        }
    } else {
        v = selected + 1;
    }
    if (eh->race == RACE_HUMAN) {
        v *= 2;
    }
    ADDSATT(eh->relation1[pa], v, 100);
    ea->relation1[ph] = eh->relation1[pa];
    if (eh->treaty[pa] >= TREATY_WAR) {
        SETMIN(ea->relation1[ph], -25);
    }
    if (eh->treaty[pa] != TREATY_ALLIANCE) {
        SETMIN(ea->relation1[ph], 65);
    }
    /* BUG? eh->relation1[pa] = ea->relation1[ph]; is missing */
    if (g->ai_id == GAME_AI_CLASSICPLUS) {
        eh->relation1[pa] = ea->relation1[ph];
    }
}

static void game_ai_classic_aud_tribute_tech(struct audience_s *au, int selected, tech_field_t field, uint8_t tech)
{
    struct game_s *g = au->g;
    player_id_t ph = au->ph, pa = au->pa;
    empiretechorbit_t *eh = &(g->eto[ph]);
    empiretechorbit_t *ea = &(g->eto[pa]);
    int v;
    if (eh->relation1[pa] < 0) {
        v = 20;
    } else {
        v = (100 - eh->relation1[pa]) / 10;
    }
    v = ((rnd_1_n(8, &g->seed) + rnd_1_n(8, &g->seed) + (tech / 4)) * v) / 10;
    if (eh->race == RACE_HUMAN) {
        v *= 2;
    }
    ADDSATT(eh->relation1[pa], v, 100);
    ea->relation1[ph] = eh->relation1[pa];
    ADDSATT(eh->mood_peace[pa], v, 200);
    ADDSATT(eh->trust[pa], rnd_1_n(8, &g->seed) + 2, 30);
    if (eh->treaty[pa] >= TREATY_WAR) {
        SETMIN(ea->relation1[ph], -25);
    }
    if (eh->treaty[pa] != TREATY_ALLIANCE) {
        SETMIN(ea->relation1[ph], 70);
    }
    /* BUG? eh->relation1[pa] = ea->relation1[ph]; is missing */
    if (g->ai_id == GAME_AI_CLASSICPLUS) {
        eh->relation1[pa] = ea->relation1[ph];
    }
}

static int game_ai_classic_aud_tech_scale(struct audience_s *au)
{
    struct game_s *g = au->g;
    player_id_t ph = au->ph, pa = au->pa;
    empiretechorbit_t *eh = &(g->eto[ph]);
    empiretechorbit_t *ea = &(g->eto[pa]);
    int v, di;
    di = eh->mood_tech[pa];
    if (di > 0) {
        di /= 5;
    }
    SETMIN(di, 30);
    if (eh->treaty[pa] == TREATY_ALLIANCE) {
        di += 25;
    }
    v = eh->trust[pa] + eh->relation1[pa] / 2 + ((eh->race == RACE_HUMAN) ? 50 : 0);
    v += game_diplo_tbl_reldiff[ea->trait1] * 2 + di + rnd_1_n(100, &g->seed) - 125;
    if (v < 0) {
        v = abs(v) + 100;
    } else {
        v = 20000 / (v + 200);
    }
    v /= 4;
    SETMAX(v, 75);
    return v;
}

static uint8_t game_ai_classic_aud_get_dtype(struct audience_s *au, uint8_t dtype, int a2)
{
    struct game_s *g = au->g;
    player_id_t ph = au->ph, pa = au->pa;
    empiretechorbit_t *eh = &(g->eto[ph]);
    if ((dtype == 31) || ((dtype >= 33) && (dtype <= 41)) || ((dtype >= 62) && (dtype <= 68))) {
        switch (a2) {
            case 0:
            case 1:
            case 2:
                if ((eh->blunder[pa] != 0) && (!rnd_0_nm1(2, &g->seed))) {
                    dtype = eh->blunder[pa] + 30;
                    eh->blunder[pa] = 0;
                } else if ((eh->broken_treaty[pa] != TREATY_NONE) && (!rnd_0_nm1(4, &g->seed))) {
                    dtype = 33;
                    eh->broken_treaty[pa] = TREATY_NONE;
                } else {
                    dtype = 31;
                }
                break;
            case 3:
                if ((!rnd_0_nm1(4, &g->seed)) && (eh->tribute_tech[pa] != 0)) {
                    au->tribute_field = eh->tribute_field[pa];
                    au->tribute_tech = eh->tribute_tech[pa];
                    eh->tribute_tech[pa] = 0;
                    dtype = 66;
                }
                break;
            default:
                break;
        }
    }
    return dtype;
}

static bool game_ai_classic_aud_later(struct audience_s *au)
{
    struct game_s *g = au->g;
    player_id_t ph = au->ph, pa = au->pa;
    return (game_diplo_get_mood(g, ph, pa) < -100);
}

/* -------------------------------------------------------------------------- */

const struct game_ai_s game_ai_classic = {
    GAME_AI_CLASSIC,
    "Classic",
    game_ai_classic_new_game_init,
    game_ai_classic_new_game_tech,
    game_ai_classic_turn_p1,
    game_ai_classic_turn_p2,
    game_ai_classic_turn_p3,
    game_ai_classic_battle_ai_ai_resolve,
    game_ai_classic_battle_ai_turn,
    game_ai_classic_battle_ai_retreat,
    game_ai_classic_tech_next,
    game_ai_classic_bomb,
    game_ai_classic_ground,
    game_ai_classic_crank_tech, /* plague */
    game_ai_classic_crank_tech, /* nova */
    game_ai_classic_crank_ship, /* comet */
    game_ai_classic_crank_ship, /* pirates */
    game_ai_classic_vote,
    game_ai_classic_turn_diplo_p1,
    game_ai_classic_turn_diplo_p2,
    game_ai_classic_aud_start_human,
    game_ai_classic_aud_treaty_nap,
    game_ai_classic_aud_treaty_alliance,
    game_ai_classic_aud_treaty_peace,
    game_ai_classic_aud_treaty_declare_war,
    game_ai_classic_aud_treaty_break_alliance,
    game_ai_classic_aud_trade,
    game_ai_classic_aud_sweeten,
    game_ai_classic_aud_threaten,
    game_ai_classic_aud_tribute_bc,
    game_ai_classic_aud_tribute_tech,
    game_ai_classic_aud_tech_scale,
    game_ai_classic_aud_get_dtype,
    game_ai_classic_aud_later
};

const struct game_ai_s game_ai_classicplus = {
    GAME_AI_CLASSICPLUS,
    "Classic+",
    game_ai_classic_new_game_init,
    game_ai_classic_new_game_tech,
    game_ai_classic_turn_p1,
    game_ai_classic_turn_p2,
    game_ai_classic_turn_p3,
    game_ai_classic_battle_ai_ai_resolve,
    game_ai_classic_battle_ai_turn,
    game_ai_classic_battle_ai_retreat,
    game_ai_classic_tech_next,
    game_ai_classic_bomb,
    game_ai_classic_ground,
    game_ai_classic_crank_tech, /* plague */
    game_ai_classic_crank_tech, /* nova */
    game_ai_classic_crank_ship, /* comet */
    game_ai_classic_crank_ship, /* pirates */
    game_ai_classic_vote,
    game_ai_classic_turn_diplo_p1,
    game_ai_classic_turn_diplo_p2,
    game_ai_classic_aud_start_human,
    game_ai_classic_aud_treaty_nap,
    game_ai_classic_aud_treaty_alliance,
    game_ai_classic_aud_treaty_peace,
    game_ai_classic_aud_treaty_declare_war,
    game_ai_classic_aud_treaty_break_alliance,
    game_ai_classic_aud_trade,
    game_ai_classic_aud_sweeten,
    game_ai_classic_aud_threaten,
    game_ai_classic_aud_tribute_bc,
    game_ai_classic_aud_tribute_tech,
    game_ai_classic_aud_tech_scale,
    game_ai_classic_aud_get_dtype,
    game_ai_classic_aud_later
};
