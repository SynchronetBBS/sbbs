#include "config.h"

#include <stdio.h>

#include "game_misc.h"
#include "bits.h"
#include "boolvec.h"
#include "comp.h"
#include "game.h"
#include "game_aux.h"
#include "game_num.h"
#include "game_shiptech.h"
#include "game_str.h"
#include "game_tech.h"
#include "lib.h"
#include "log.h"
#include "rnd.h"
#include "types.h"
#include "util.h"
#include "util_math.h"

/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */

void game_update_have_reserve_fuel(struct game_s *g)
{
    for (player_id_t pi = PLAYER_0; pi < g->players; ++pi) {
        shipresearch_t *srd = &(g->srd[pi]);
        int num = g->eto[pi].shipdesigns_num;
        for (int si = 0; si < NUM_SHIPDESIGNS; ++si) {
            srd->have_reserve_fuel[si] = false;
        }
        for (int si = 0; si < num; ++si) {
            shipdesign_t *sd = &(srd->design[si]);
            if (0
              || (sd->special[0] == SHIP_SPECIAL_RESERVE_FUEL_TANKS)
              || (sd->special[1] == SHIP_SPECIAL_RESERVE_FUEL_TANKS)
              || (sd->special[2] == SHIP_SPECIAL_RESERVE_FUEL_TANKS)
            ) {
                srd->have_reserve_fuel[si] = true;
            }
        }
    }
}

void game_update_maint_costs(struct game_s *g)
{
    for (player_id_t pi = PLAYER_0; pi < g->players; ++pi) {
        shipsum_t tbl_ships[NUM_SHIPDESIGNS];
        uint32_t totalcost; /* FIXME maybe uint64_t? */
        uint16_t bases;
        shipresearch_t *srd = &(g->srd[pi]);
        empiretechorbit_t *e = &(g->eto[pi]);
        int numsd = e->shipdesigns_num;
        for (int si = 0; si < NUM_SHIPDESIGNS; ++si) {
            tbl_ships[si] = 0;
        }
        for (int si = 0; si < numsd; ++si) {
            for (int i = 0; i < g->galaxy_stars; ++i) {
                tbl_ships[si] += e->orbit[i].ships[si];
            }
        }
        for (int j = 0; j < g->enroute_num; ++j) {
            if (g->enroute[j].owner == pi) {
                for (int si = 0; si < numsd; ++si) {
                    tbl_ships[si] += g->enroute[j].ships[si];
                }
            }
        }
        totalcost = 0;
        for (int si = 0; si < NUM_SHIPDESIGNS; ++si) {
            const shipdesign_t *sd = &(srd->design[si]);
            srd->shipcount[si] = tbl_ships[si];
            totalcost += tbl_ships[si] * sd->cost;
        }
        totalcost = totalcost / 50;
        SETMIN(totalcost, game_num_max_ship_maint);
        bases = 0;
        for (int i = 0; i < g->galaxy_stars; ++i) {
            const planet_t *p = &(g->planet[i]);
            if (p->owner == pi) {
                if (p->have_stargate) {
                    totalcost += game_num_stargate_maint; /* WASBUG MOO1 sums to a int16_t var after limiting to 32000 */
                }
                bases += p->missile_bases;
            }
        }
        SETMIN(totalcost, game_num_max_ship_maint);
        e->ship_maint_bc = totalcost;
        e->bases_maint_bc = (bases * game_get_base_cost(g, pi)) / 50;
    }
}

void game_update_production(struct game_s *g)
{
    game_update_maint_costs(g);
    for (player_id_t pi = PLAYER_0; pi < g->players; ++pi) {
        g->eto[pi].total_production_bc = 0;
    }
    for (player_id_t pi = PLAYER_0; pi < g->players; ++pi) {
        empiretechorbit_t *e = &(g->eto[pi]);
        for (player_id_t pi2 = PLAYER_0; pi2 < g->players; ++pi2) {
            if ((pi == pi2) || BOOLVEC_IS0(e->contact, pi2)) {
                e->spying[pi2] = 0;
            }
        }
    }
    for (int i = 0; i < g->galaxy_stars; ++i) {
        planet_t *p = &(g->planet[i]);
        player_id_t owner = p->owner;
        if (owner != PLAYER_NONE) {
            empiretechorbit_t *e = &(g->eto[owner]);
            int pop = p->pop;
            int v;
            if (game_num_leaving_trans_fix) {
                int rebels = p->rebels;
                if (p->trans_num) {           /* see game_send_transport() */
                    pop -= p->trans_num;
                    SUBSAT0(rebels, p->trans_num / 2 + 1);
                    SETMAX(pop, 1);
                }
                v = (pop - rebels) * game_planet_get_pop_oper_fact(g, p);
                SETMAX(v, 0);
            } else {
                int popx = p->pop - p->rebels;
                SETMAX(popx, 0);
                v = popx * game_planet_get_pop_oper_fact(g, p);
            }
            {
                uint16_t factories = p->factories;
                int extra;
                SETMIN(factories, v);
                extra = (pop * (e->tech.percent[TECH_FIELD_PLANETOLOGY] * 3 + 50)) / 100;
                if (e->race == RACE_KLACKON) {
                    extra <<= 1;
                }
                v = factories + extra;
            }
            if (BOOLVEC_IS1(g->is_ai, owner)) {
                switch (g->difficulty) {    /* TODO BUG? simple has larger v than easy */
                    case DIFFICULTY_EASY:
                        v = (v << 2) / 5;
                        break;
                    case DIFFICULTY_AVERAGE:
                        v += v / 4;
                        break;
                    case DIFFICULTY_HARD:
                        v += v / 2;
                        break;
                    case DIFFICULTY_IMPOSSIBLE:
                        v += v;
                        break;
                    default:
                        break;
                }
            }
            {
                uint32_t reserve;
                reserve = p->reserve;
                SETMIN(reserve, v);
                v += reserve;
            }
            if (p->unrest == PLANET_UNREST_REBELLION) {
                v = 0;
            }
            SETMAX(v, 0);
            p->prod_after_maint = v;
            p->total_prod = v;
            e->total_production_bc += v;
        }
    }
    for (player_id_t pi = PLAYER_0; pi < g->players; ++pi) {
        empiretechorbit_t *e = &(g->eto[pi]);
        uint16_t spysum;
        spysum = 0;
        for (player_id_t pi2 = PLAYER_0; pi2 < g->players; ++pi2) {
            spysum += e->spying[pi2];
        }
        {
            uint16_t spymaint;
            spymaint = 0;
            for (player_id_t pi2 = PLAYER_0; pi2 < g->players; ++pi2) {
                spymaint += (e->total_production_bc * e->spying[pi2]) / 1000;
            }
            e->spying_maint_bc = spymaint;
        }
        {
            uint8_t bonus = (e->race == RACE_HUMAN) ? 25 : 0;
            int32_t trade = 0;
            for (player_id_t pi2 = PLAYER_0; pi2 < g->players; ++pi2) {
                trade += (e->trade_bc[pi2] * (e->trade_percent[pi2] + bonus)) / 100;
            }
            e->total_trade_bc = trade;
        }
        {
            int32_t actual_prod;
            actual_prod = (e->total_production_bc * (1000 - e->security - spysum)) / 1000
                        + e->total_trade_bc - e->ship_maint_bc - e->bases_maint_bc
                        - (e->total_production_bc * e->tax) / 1000;
            if (actual_prod < 1) {
                actual_prod = 1;
            }
            e->total_maint_bc = e->total_production_bc - actual_prod;
            e->percent_prod_total_to_actual = e->total_production_bc ? ((actual_prod * 100) / e->total_production_bc) : 0;
        }
        for (int i = 0; i < g->galaxy_stars; ++i) {
            planet_t *p = &(g->planet[i]);
            if (p->owner == pi) {
                int v;
                v = (p->prod_after_maint * e->percent_prod_total_to_actual) / 100 - p->trans_num;
                /* BUG: deducting transport cost takes no effect for actual production,
                 * which happens after departure, but leads to wrong display for player. */
                p->prod_after_maint = (v > 0) ? v : 0;
            }
        }
    }
}

void game_update_total_research(struct game_s *g)
{
    for (player_id_t pi = PLAYER_0; pi < g->players; ++pi) {
        g->eto[pi].total_research_bc = 0;
    }
    for (int i = 0; i < g->galaxy_stars; ++i) {
        planet_t *p = &(g->planet[i]);
        if (1
          && (p->owner != PLAYER_NONE)
          && (!(g->evn.have_plague && (g->evn.plague_planet_i == i)))
          && (!(g->evn.have_nova && (g->evn.nova_planet_i == i)))
        ) {
            empiretechorbit_t *e;
            e = &(g->eto[p->owner]);
            e->total_research_bc += game_get_tech_prod(p->prod_after_maint, p->slider[PLANET_SLIDER_TECH], e->race, p->special);
        }
    }
}

void game_update_eco_on_waste(struct game_s *g, int player_i, bool force_adjust)
{
    empiretechorbit_t *e = &(g->eto[player_i]);
    if (e->race == RACE_SILICOID) {
        return;
    }
    for (int i = 0; i < g->galaxy_stars; ++i) {
        planet_t *p = &(g->planet[i]);
        if (p->owner == player_i) {
            uint16_t v;
            int16_t left;
            bool check_locks = !force_adjust && game_num_slider_respects_locks && IS_HUMAN(g, p->owner);
            v = game_planet_get_waste_percent(NULL, g, p, false);
            if (check_locks && p->slider_lock[PLANET_SLIDER_ECO]) {
            } else if (check_locks && p->slider_lock[PLANET_SLIDER_SHIP] && p->slider_lock[PLANET_SLIDER_DEF] && p->slider_lock[PLANET_SLIDER_IND] && p->slider_lock[PLANET_SLIDER_TECH]) {
            } else if ((p->slider[PLANET_SLIDER_ECO] < v) || force_adjust) {
                int16_t eco_diff = v - p->slider[PLANET_SLIDER_ECO];
                int16_t sum = 100;
                if (game_num_waste_adjust_fix) {
                    sum = p->slider[PLANET_SLIDER_SHIP] + p->slider[PLANET_SLIDER_DEF] + p->slider[PLANET_SLIDER_IND] + p->slider[PLANET_SLIDER_TECH];
                }
                if (check_locks) {
                    int16_t sum_locks = 0;
                    for (int si = 0; si < PLANET_SLIDER_NUM; ++si){
                        if (p->slider_lock[si]) {
                            sum_locks += p->slider[si];
                        }
                    }
                    sum -= sum_locks;
                    SETMIN(v, 100 - sum_locks);
                }
                p->slider[PLANET_SLIDER_ECO] = v;
                if (sum > 0) {
                    if (!check_locks || !p->slider_lock[PLANET_SLIDER_SHIP]) {
                        p->slider[PLANET_SLIDER_SHIP] -= (p->slider[PLANET_SLIDER_SHIP] * eco_diff) / sum;
                    }
                    if (!check_locks || !p->slider_lock[PLANET_SLIDER_DEF]) {
                        p->slider[PLANET_SLIDER_DEF] -= (p->slider[PLANET_SLIDER_DEF] * eco_diff) / sum;
                    }
                    if (!check_locks || !p->slider_lock[PLANET_SLIDER_IND]) {
                        p->slider[PLANET_SLIDER_IND] -= (p->slider[PLANET_SLIDER_IND] * eco_diff) / sum;
                    }
                }
            }
            SETMAX(p->slider[PLANET_SLIDER_SHIP], 0);
            SETMAX(p->slider[PLANET_SLIDER_DEF], 0);
            SETMAX(p->slider[PLANET_SLIDER_IND], 0);
            SETRANGE(p->slider[PLANET_SLIDER_ECO], 0, 100);
            left = 100;
            left -= p->slider[PLANET_SLIDER_SHIP];
            left -= p->slider[PLANET_SLIDER_DEF];
            left -= p->slider[PLANET_SLIDER_IND];
            left -= p->slider[PLANET_SLIDER_ECO];
            SETMAX(left, 0);
            if (!check_locks || !p->slider_lock[PLANET_SLIDER_TECH]) {
                p->slider[PLANET_SLIDER_TECH] = left;
            }
        }
    }
}

void game_update_seen_by_orbit(struct game_s *g, player_id_t pi)
{
    empiretechorbit_t *e = &(g->eto[pi]);
    for (int i = 0; i < g->galaxy_stars; ++i) {
        planet_t *p = &(g->planet[i]);
        bool in_orbit;
        in_orbit = false;
        for (int j = 0; j < e->shipdesigns_num; ++j) {
            if (e->orbit[i].ships[j] != 0) {
                in_orbit = true;
                break;
            }
        }
        if ((p->owner == pi) || in_orbit) {
            g->seen[pi][i].owner = p->owner;
            g->seen[pi][i].pop = p->pop;
            g->seen[pi][i].bases = p->missile_bases;
            g->seen[pi][i].factories = p->factories;
        }
    }
}

void game_update_within_range(struct game_s *g)
{
    for (player_id_t pi = PLAYER_0; pi < g->players; ++pi) {
        uint8_t tblplanet[PLANETS_MAX];
        uint8_t tblplanet_num;
        empiretechorbit_t *e = &(g->eto[pi]);
        bool tbl_alliance[PLAYER_NUM];
        uint8_t frange, frangep3, srange, srange2;
        for (int i = 0; i < PLAYER_NUM; ++i) {
            tbl_alliance[i] = (e->treaty[i] == TREATY_ALLIANCE);
        }
        tbl_alliance[pi] = true;
        frange = e->fuel_range;
        frangep3 = frange + 3;
        srange = e->scanner_range;
        switch (srange) {
            case 3: srange2 = 0; break;
            case 5: srange2 = 10; break;
            case 7: srange2 = 20; break;
            case 9: srange2 = 30; break;
            default: break;
        }
        tblplanet_num = 0;
        for (int i = 0; i < g->galaxy_stars; ++i) {
            planet_t *p = &(g->planet[i]);
            if ((p->owner != PLAYER_NONE) && tbl_alliance[p->owner]) {
                tblplanet[tblplanet_num++] = i;
            }
        }
        for (int i = 0; i < g->galaxy_stars; ++i) {
            planet_t *p = &(g->planet[i]);
            if (p->owner == pi) {
                p->within_frange[pi] = 1;
                BOOLVEC_SET1(p->within_srange, pi);
            } else {
                uint16_t dist, mindist1, mindist2;
                mindist1 = 0x2710;
                mindist2 = 0x2710;
                for (int j = 0; (j < tblplanet_num) && ((mindist1 > frange) || (mindist2 > srange)); ++j) {
                    uint8_t planet_i2;
                    planet_i2 = tblplanet[j];
                    dist = g->gaux->star_dist[i][planet_i2];
                    SETMIN(mindist1, dist);
                    if ((dist < mindist2) && (g->planet[planet_i2].owner == pi)) {
                        mindist2 = dist;
                    }
                }
                if (mindist1 <= frange) {
                    p->within_frange[pi] = 1;
                } else if (mindist1 <= frangep3) {
                    p->within_frange[pi] = 2;
                } else {
                    p->within_frange[pi] = 0;
                }
                BOOLVEC_SET(p->within_srange, pi, (mindist2 <= srange));
                if (BOOLVEC_IS0(p->within_srange, pi) && ((srange2 > 0) || game_num_ship_scanner_fix)) {
                    mindist1 = 10000;
                    for (int j = 0; (j < g->enroute_num) && (mindist1 > srange2); ++j) {
                        if (g->enroute[j].owner == pi) {
                            dist = util_math_dist_fast(g->enroute[j].x, g->enroute[j].y, p->x, p->y);
                            if (game_num_ship_scanner_fix) {
                                if (dist < mindist1) {
                                    mindist1 = dist;
                                }
                            } else {
                                dist = (dist + 9) / 10;
                                if (dist < mindist1) {
                                    dist = mindist1;
                                }
                            }
                        }
                    }
                    if (mindist1 <= srange2) {
                        BOOLVEC_SET1(p->within_srange, pi);
                    }
                }
            }
        }
    }
    for (int i = 0; i < g->galaxy_stars; ++i) {
        planet_t *p = &(g->planet[i]);
        for (player_id_t pi = PLAYER_0; pi < g->players; ++pi) {
            empiretechorbit_t *e = &(g->eto[pi]);
            uint16_t snum;
            snum = 0;
            for (int j = 0; (j < e->shipdesigns_num) && !snum; ++j) {
                snum += e->orbit[i].ships[j];
            }
            if (!snum) {
                for (player_id_t pi2 = PLAYER_0; pi2 < PLAYER_NUM; ++pi2) {
                    BOOLVEC_CLEAR(g->eto[pi2].orbit[i].visible, PLAYER_NUM); /* WASBUG? only cleared bit 0 */
                }
            } else {
                BOOLVEC_SET1(p->within_srange, pi);
            }
        }
    }
}

void game_update_reloc_dest(struct game_s *g)
{
    for (int i = 0; i < g->galaxy_stars; ++i) {
        struct planet_s *pf = &(g->planet[i]);
        if (!game_reloc_dest_ok(g, pf->reloc, pf->owner)) {
            pf->reloc = i;
        }
    }
}

void game_update_empire_contact(struct game_s *g)
{
    uint8_t tbl_pnum[PLAYER_NUM];
    uint8_t tbl_planet[PLAYER_NUM][PLANETS_MAX];
    for (player_id_t pi = PLAYER_0; pi < g->players; ++pi) {
        tbl_pnum[pi] = 0;
    }
    for (int i = 0; i < g->galaxy_stars; ++i) {
        planet_t *p = &(g->planet[i]);
        player_id_t owner;
        owner = p->owner;
        if (owner != PLAYER_NONE) {
            tbl_planet[owner][tbl_pnum[owner]++] = i;
        }
    }
    for (player_id_t pi1 = PLAYER_0; pi1 < g->players; ++pi1) {
        empiretechorbit_t *e1 = &(g->eto[pi1]);
        for (player_id_t pi2 = pi1 + 1; pi2 < g->players; ++pi2) {
            empiretechorbit_t *e2 = &(g->eto[pi2]);
            uint8_t frange, dist, mindist;
            frange = MAX(e1->fuel_range, e2->fuel_range);
            mindist = 0xff;
            for (int i1 = 0; i1 < tbl_pnum[pi1]; ++i1) {
                for (int i2 = 0; i2 < tbl_pnum[pi2]; ++i2) {
                    dist = g->gaux->star_dist[tbl_planet[pi1][i1]][tbl_planet[pi2][i2]];
                    SETMIN(mindist, dist);
                }
            }
            if (mindist <= frange) {
                BOOLVEC_SET1(e1->contact, pi2);
                BOOLVEC_SET1(e2->contact, pi1);
            } else {
                BOOLVEC_SET0(e1->contact, pi2);
                BOOLVEC_SET0(e2->contact, pi1);
            }
        }
    }
}

bool game_check_coord_is_visible(const struct game_s *g, player_id_t pi, int range, int x, int y)
{
    const empiretechorbit_t *e = &(g->eto[pi]);
    range *= 10;    /* 30, 50, 70, 90 */
    for (int i = 0; i < g->galaxy_stars; ++i) {
        const planet_t *p = &(g->planet[i]);
        if ((p->owner == pi) && (util_math_dist_fast(x, y, p->x, p->y) <= range)) {
            return true;
        }
    }
    range = (range - 30) / 2;  /* 0, 10, 20, 30 */
    for (int i = 0; i < g->galaxy_stars; ++i) {
        const planet_t *p = &(g->planet[i]);
        uint32_t snum;
        snum = 0;
        for (int j = 0; (j < e->shipdesigns_num) && !snum; ++j) {
            snum += e->orbit[i].ships[j];
        }
        if (snum && (util_math_dist_fast(x, y, p->x, p->y) <= range)) {
            return true;
        }
    }
    for (int ei = 0; ei < g->enroute_num; ++ei) {
        const fleet_enroute_t *r = &(g->enroute[ei]);
        if ((r->owner == pi) && (util_math_dist_fast(x, y, r->x, r->y) <= range)) {
            return true;
        }
    }
    return false;
}

void game_update_visibility(struct game_s *g)
{
    for (int ei = 0; ei < g->enroute_num; ++ei) {
        fleet_enroute_t *r = &(g->enroute[ei]);
        for (player_id_t pi = PLAYER_0; pi < g->players; ++pi) {
            if (r->owner != pi) {
                BOOLVEC_SET(r->visible, pi, game_check_coord_is_visible(g, pi, g->eto[pi].scanner_range, r->x, r->y));
            } else {
                BOOLVEC_SET1(r->visible, pi);
            }
        }
    }
    for (int i = 0; i < g->transport_num; ++i) {
        transport_t *t = &(g->transport[i]);
        for (player_id_t pi = PLAYER_0; pi < g->players; ++pi) {
            if (t->owner != pi) {
                BOOLVEC_SET(t->visible, pi, game_check_coord_is_visible(g, pi, g->eto[pi].scanner_range, t->x, t->y));
            } else {
                BOOLVEC_SET1(t->visible, pi);
            }
        }
    }
    for (int i = 0; i < g->galaxy_stars; ++i) {
        const planet_t *p = &(g->planet[i]);
        for (player_id_t pi = PLAYER_0; pi < g->players; ++pi) {
            bool visible = BOOLVEC_IS1(p->within_srange, pi);
            for (player_id_t pi2 = PLAYER_0; pi2 < g->players; ++pi2) {
                BOOLVEC_SET(g->eto[pi2].orbit[i].visible, pi, visible);
            }
            BOOLVEC_SET1(g->eto[pi].orbit[i].visible, pi);
        }
    }
    for (int i = 0; i < g->galaxy_stars; ++i) {
        for (player_id_t pi = PLAYER_0; pi < g->players; ++pi) {
            fleet_orbit_t *o = &(g->eto[pi].orbit[i]);
            bool any_ships;
            any_ships = false;
            for (int j = 0; j < NUM_SHIPDESIGNS; ++j) {
                if (o->ships[j] != 0) {
                    any_ships = true;
                    break;
                }
            }
            if (!any_ships) {
                BOOLVEC_CLEAR(o->visible, PLAYER_NUM);
            }
        }
    }
}

void game_adjust_slider_group(int16_t *slidertbl, int slideri, int16_t value, int num, const uint16_t *locktbl)
{
    bool have_unlocked = false;
    int first_unlocked_i = 0;
    int last_unlocked_i = 0;
    int left = 100;
    for (int i = 0; i < num; ++i) {
        if (locktbl[i]) {
            left -= slidertbl[i];
        } else {
            last_unlocked_i = i;
            if (!have_unlocked) {
                have_unlocked = true;
                first_unlocked_i = i;
            }
        }
    }
    SETMIN(value, left);
    slidertbl[slideri] = value;
    left -= value;
    for (int i = 0; i < num; ++i) {
        if ((i != slideri) && (!locktbl[i])) {
            if (slidertbl[i] <= left) {
                left -= slidertbl[i];
            } else {
                slidertbl[i] = left;
                left = 0;
            }
        }
    }
    if ((left > 0) && (have_unlocked)) {
        int j;
        if (slideri != last_unlocked_i) {
            j = last_unlocked_i;
        } else {
            j = first_unlocked_i;
        }
        slidertbl[j] += left;
    }
}

void game_equalize_slider_group(int16_t *slidertbl, int num, const uint16_t *locktbl)
{
    int total = 0;
    int num_unlocked = 0;
    for (int i = 0; i < num; ++i) {
        if (!locktbl[i]) {
            total += slidertbl[i];
            ++num_unlocked;
        }
    }
    if (num_unlocked > 0) {
        int n = total / num_unlocked;
        int o = total % num_unlocked;
        for (int i = 0; i < num; ++i) {
            if (!locktbl[i]) {
                int v;
                v = n;
                if (o) {
                    ++v;
                    --o;
                }
                slidertbl[i] = v;
            }
        }
    }
}

int game_get_min_dist(const struct game_s *g, int player_i, int planet_i)
{
    int dist, mindist = 255;
    for (int i = 0; i < g->galaxy_stars; ++i) {
        if ((i != planet_i) && (g->planet[i].owner == player_i)) {
            dist = g->gaux->star_dist[planet_i][i];
            SETMIN(mindist, dist);
        }
    }
    return mindist;
}

int game_adjust_prod_by_special(int prod, planet_special_t special)
{
    switch (special) {
        case PLANET_SPECIAL_ULTRA_POOR:
            prod /= 3;
            break;
        case PLANET_SPECIAL_POOR:
            prod /= 2;
            break;
        case PLANET_SPECIAL_RICH:
            prod *= 2;
            break;
        case PLANET_SPECIAL_ULTRA_RICH:
            prod *= 3;
            break;
        default:
            break;
    }
    return prod;
}

int game_get_pop_growth_max(const struct game_s *g, const planet_t *p, int max_pop3)
{
    const empiretechorbit_t *e = &(g->eto[p->owner]);
    int v, retval;
    if (e->race == RACE_SILICOID) {
        v = (100 - (p->pop * 100) / max_pop3) / 2;
    } else {
        v = 100 - ((p->pop + p->waste) * 100) / max_pop3;
        switch (p->growth) {
            case PLANET_GROWTH_HOSTILE:
                if (v >= 0) {
                    v /= 2;
                } else {
                    v *= 2;
                }
                break;
            case PLANET_GROWTH_NORMAL:
            default:
                break;
            case PLANET_GROWTH_FERTILE:
                if (v >= 0) {
                    v += v / 2;
                } else {
                    v = (v * 2) / 3;
                }
                break;
            case PLANET_GROWTH_GAIA:
                if (v >= 0) {
                    v *= 2;
                } else {
                    v /= 2;
                }
                break;
        }
        if (e->race == RACE_SAKKRA) {
            v *= 2;
        }
    }
    retval = (v * p->pop + 5) / 100;
    if (!game_num_pop_tenths_fix) {
        retval += p->pop_tenths;
    }
    if ((v > 0) && (retval < 1)) {
        retval = 1;
    }
    if ((p->pop + retval / 10) < 1) {
        retval = 1;
    }
    return retval;
}

int game_get_pop_growth_for_eco(const struct game_s *g, const planet_t *p, int eco)
{
    const empiretechorbit_t *e = &(g->eto[p->owner]);
    int v, vmax;
    v = (eco * 10) / e->inc_pop_cost;
    if (e->race == RACE_SAKKRA) {
        v *= 2;
    } else if (e->race == RACE_SILICOID) {
        v /= 2;
    }
    vmax = (p->pop * 10) / 4;
    return MIN(v, vmax);
}

int game_get_tech_prod(int prod, int slider, race_t race, planet_special_t special)
{
    int v = (prod * slider) / 100;
    if (race == RACE_PSILON) {
        v += v / 2;
    }
    switch (special) {
        case PLANET_SPECIAL_ARTIFACTS:
            v *= 2;
            break;
        case PLANET_SPECIAL_4XTECH:
            v *= 4;
            break;
        default:
            break;
    }
    return MIN(v, 0x7fff);
}

void game_print_prod_of_total(const struct game_s *g, player_id_t pi, int prod, char *buf, size_t bufsize)
{
    int v = g->eto[pi].total_production_bc;
    if (v == 0) {
        lib_strcpy(buf, "0%%", bufsize);
    } else {
        v = (prod * 1000) / v;
        lib_sprintf(buf, bufsize, "%i.%i%%", v / 10, v % 10);
    }
}

bool game_xy_is_in_nebula(const struct game_s *g, int x, int y)
{
    for (int n = 0; n < g->nebula_num; ++n) {
        for (int i = 0; i < 4; ++i) {
            if (1
              && (x >= g->nebula_x0[n][i])
              && (x <= g->nebula_x1[n][i])
              && (y >= g->nebula_y0[n][i])
              && (y <= g->nebula_y1[n][i])
            ) {
                return true;
            }
        }
    }
    return false;
}

int game_calc_eta_ship(const struct game_s *g, int speed, int x0, int y0, int x1, int y1)
{
    int x, y, num = 0;
    x = x0;
    y = y0;
    while ((x != x1) || (y != y1)) {
        int v;
        bool first;
        v = speed;
        first = true;
        do {
            /* Inside nebulas, ships move on frame 0 and then on every odd frame. */
            if (first || (!game_xy_is_in_nebula(g, x, y))) {
                util_math_go_line_dist(&x, &y, x1, y1, 5);
            }
            util_math_go_line_dist(&x, &y, x1, y1, 6);
            first = false;
            --v;
        } while (v > 0);
        ++num;
    }
    return num;
}

int game_calc_eta_trans(const struct game_s *g, int speed, int x0, int y0, int x1, int y1)
{
    int x = x0, y = y0, num = 0;
    while ((x != x1) || (y != y1)) {
        int v = speed;
        do {
            /* Inside nebulas, transports move on every even frame. */
            util_math_go_line_dist(&x, &y, x1, y1, 5);
            if (!game_xy_is_in_nebula(g, x, y)) {
                util_math_go_line_dist(&x, &y, x1, y1, 6);
            }
            --v;
        } while (v > 0);
        ++num;
    }
    return num;
}

bool game_transport_dest_ok(const struct game_s *g, const planet_t *p, player_id_t pi)
{
    if (!game_num_trans_redir_fix) {
        return (p->within_frange[pi] != 0); /* WASBUG MOO1 allows redirection almost anywhere */
    } else {
        return (p->within_frange[pi] == 1)
            && BOOLVEC_IS1(p->explored, pi)
            && (p->type >= g->eto[pi].have_colony_for)
            && (p->owner != PLAYER_NONE)
            ;
    }
}

bool game_reloc_dest_ok(const struct game_s *g, uint8_t planet_i, player_id_t pi)
{
    return game_num_extended_reloc_range ? (g->planet[planet_i].within_frange[pi] == 1) : (g->planet[planet_i].owner == pi);
}

void game_rng_step(struct game_s *g)
{
    /* TODO disable for multiplayer */
    if (!game_num_deterministic) {
        rnd_0_nm1(2, &g->seed);
    }
}

/* Max. population values: Each planet has three max_pop variables which are
 * affected by the ECO projects in different ways.
 *
 * The base size, max_pop1, is the size of the planet at the start of the game.
 * It is used to determine the increase in max. population from (advanced) soil
 * enrichment; max_pop1 itself is increased only when applying atmospheric
 * terraforming to some types of hostile planets.
 *
 * max_pop2 is max_pop1 modified by (advanced) soil enrichment. Note that for
 * planets that are Fertile or Gaia at the start of the game,
 * max_pop1 == max_pop2. They are only different when the planets were made
 * Fertile or Gaia by a player (human or AI).
 *
 * max_pop3 is the actual population maximum. It equals max_pop2 modified by
 * terraforming and/or by bioweapon damage.
 */
void game_turn_atmos_tform(struct planet_s *p) {
    int max_pop_increase;
    if (p->type < PLANET_TYPE_DEAD) {
        max_pop_increase = 20;
    } else if (p->type < PLANET_TYPE_BARREN) {
        max_pop_increase = 10;
    } else {
        max_pop_increase = 0;
    }
    /* WASBUG max_pop += moved from outside if (bc >= cost) */
    p->type = PLANET_TYPE_MINIMAL;
    p->max_pop1 += max_pop_increase;
    p->max_pop2 += max_pop_increase;
    p->max_pop3 += max_pop_increase;
    p->growth = PLANET_GROWTH_NORMAL;
}

void game_turn_soil_enrich(struct planet_s *p, int best_tform, bool advanced) {
    int max_pop_increase = 0;
    int16_t old_max_pop2 = 0;
    p->growth = advanced ? PLANET_GROWTH_GAIA : PLANET_GROWTH_FERTILE;
    if (advanced) {
        max_pop_increase = (p->max_pop1 / 10) * 5;
        /* BUG? If we want to calculate 50% of the base size, rounded up
        * to the next multiple of 5, we'd check if p->max_pop1 % 10 != 0
        * below. Instead, we add an additional +5 unless the base size is
        * in the range 50-59 or 100-109. Penalizing these specific sizes
        * seems weird and arbitrary. */
        if ((p->max_pop1 / 10) % 5) {
            max_pop_increase += 5;
        }
        if (game_num_soil_rounding_fix) {
            max_pop_increase = ((p->max_pop1 + 9) / 10) * 5;
        }
    } else {
        max_pop_increase = (p->max_pop1 / 20) * 5;
        /* BUG? See above. */
        if ((p->max_pop1 / 20) % 5) {
            max_pop_increase += 5;
        }
        if (game_num_soil_rounding_fix) {
            max_pop_increase = ((p->max_pop1 + 19) / 20) * 5;
        }
    }
    SETMAX(max_pop_increase, 5);
    old_max_pop2 = p->max_pop2;
    p->max_pop2 = p->max_pop1 + max_pop_increase;
    p->max_pop3 += p->max_pop2 - old_max_pop2;
    if (game_num_reset_tform_to_max) {
        /* BUG? Having p->max_pop3 > (best_tform + p->max_pop2) seems
         * only possible when conquering a planet from a race with better
         * terraforming tech. Is it intended that the player loses the
         * more advanced terraforming here, or is this a sanity check
         * with unintended consequences? */
        SETMIN(p->max_pop3, (best_tform + p->max_pop2));
    }
    SETMIN(p->max_pop3, game_num_max_pop);
}

void game_ship_build_everywhere(struct game_s *g, player_id_t owner, uint8_t ship_i) {
    for (uint8_t i = 0; i < g->galaxy_stars; ++i) {
        planet_t *p = &(g->planet[i]);
        if (p->owner == owner) {
            if (ship_i == BUILDSHIP_STARGATE) {
                if (!p->have_stargate) {
                    p->buildship = BUILDSHIP_STARGATE;
                }
            } else {
                p->buildship = ship_i;
            }
        }
    }
}

void game_ship_replace_everywhere(struct game_s *g, player_id_t owner, uint8_t replace_i, uint8_t ship_i) {
    for (uint8_t i = 0; i < g->galaxy_stars; ++i) {
        planet_t *p = &(g->planet[i]);
        if (p->owner == owner && p->buildship == replace_i) {
            if (ship_i == BUILDSHIP_STARGATE) {
                if (!p->have_stargate) {
                    p->buildship = BUILDSHIP_STARGATE;
                }
            } else {
                p->buildship = ship_i;
            }
        }
    }
}
