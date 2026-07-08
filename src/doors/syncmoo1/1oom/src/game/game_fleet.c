#include "config.h"

#include <stdio.h>

#include "game_fleet.h"
#include "boolvec.h"
#include "comp.h"
#include "game.h"
#include "game_num.h"
#include "game_str.h"
#include "log.h"
#include "types.h"
#include "ui.h"
#include "util.h"

/* -------------------------------------------------------------------------- */

static bool game_send_fleet_do(struct game_s *g, player_id_t owner, uint8_t from, uint8_t dest, const shipcount_t ships[NUM_SHIPDESIGNS], const uint8_t shiptypes[NUM_SHIPDESIGNS], uint8_t numtypes, bool retreat)
{
    fleet_enroute_t *r;
    const planet_t *pf, *pt;
    uint8_t speed;
    {
        bool found = false;
        for (int i = 0; i < numtypes; ++i) {
            if (ships[i] != 0) {
                found = true;
                break;
            }
        }
        if (!found) {
            return false;
        }
    }
    pf = (&g->planet[from]);
    pt = (&g->planet[dest]);
    if ((pt->owner == owner) && (pf->owner == owner) && pt->have_stargate && pf->have_stargate) {
        speed = FLEET_SPEED_STARGATE;
    } else {
        const shipdesign_t *sd = (&g->srd[owner].design[0]);
        speed = 100;
        for (int i = 0; i < numtypes; ++i) {
            if (ships[i] > 0) {
                uint8_t s, st;
                st = shiptypes[i];
                s = sd[st].engine + 1;
                SETMIN(speed, s);
            }
        }
    }
    for (int i = 0; i < g->enroute_num; ++i) {
        r = &(g->enroute[i]);
        if ((r->owner == owner) && (r->x == pf->x) && (r->y == pf->y) && (r->dest == dest) && (r->speed == speed)) {
            for (int j = 0; j < numtypes; ++j) {
                if (ships[j] > 0) {
                    uint8_t st;
                    st = shiptypes[j];
                    ADDSATT(r->ships[st], ships[j], game_num_limit_ships);
                }
            }
            return true;
        }
    }
    if (g->enroute_num == FLEET_ENROUTE_MAX) {
        log_warning("fleet enroute table (size %i) full!\n", FLEET_ENROUTE_MAX);
        return false;
    }
    r = (&g->enroute[g->enroute_num]);
    r->owner = owner;
    r->x = pf->x;
    r->y = pf->y;
    r->dest = dest;
    r->speed = speed;
    r->retreat = retreat;
    for (int i = 0; i < NUM_SHIPDESIGNS; ++i) {
        r->ships[i] = 0;
    }
    for (int i = 0; i < numtypes; ++i) {
        if (ships[i] > 0) {
            uint8_t st;
            st = shiptypes[i];
            r->ships[st] = ships[i];
        }
    }
    BOOLVEC_CLEAR(r->visible, PLAYER_NUM);
    BOOLVEC_SET1(r->visible, owner);
    ++g->enroute_num;
    return true;
}

static bool game_send_fleet_from_orbit_do(struct game_s *g, player_id_t owner, uint8_t from, uint8_t dest, const shipcount_t ships[NUM_SHIPDESIGNS], const uint8_t shiptypes[NUM_SHIPDESIGNS], uint8_t numtypes, bool retreat)
{
    if (!game_send_fleet_do(g, owner, from, dest, ships, shiptypes, numtypes, retreat)) {
        return false;
    }
    {
        fleet_orbit_t *o = &(g->eto[owner].orbit[from]);
        bool found = false;
        for (int i = 0; i < numtypes; ++i) {
            o->ships[shiptypes[i]] -= ships[i];
        }
        for (int i = 0; i < NUM_SHIPDESIGNS; ++i) {
            if (o->ships[i] > 0) {
                found = true;
                break;
            }
        }
        if (!found) {
            BOOLVEC_CLEAR(o->visible, PLAYER_NUM);
        }
    }
    return true;
}

/* -------------------------------------------------------------------------- */

bool game_send_fleet_from_orbit(struct game_s *g, player_id_t owner, uint8_t from, uint8_t dest, const shipcount_t ships[NUM_SHIPDESIGNS], const uint8_t shiptypes[NUM_SHIPDESIGNS], uint8_t numtypes)
{
    if (dest == from) {
        return false;
    }
    return game_send_fleet_from_orbit_do(g, owner, from, dest, ships, shiptypes, numtypes, false);
}

bool game_send_fleet_retreat(struct game_s *g, player_id_t owner, uint8_t from, uint8_t dest, const shipcount_t ships[NUM_SHIPDESIGNS], const uint8_t shiptypes[NUM_SHIPDESIGNS], uint8_t numtypes)
{
    return game_send_fleet_from_orbit_do(g, owner, from, dest, ships, shiptypes, numtypes, true);
}

bool game_send_fleet_reloc(struct game_s *g, player_id_t owner, uint8_t from, uint8_t dest, uint8_t si, shipcount_t shipnum)
{
    shipcount_t ships[NUM_SHIPDESIGNS];
    uint8_t shiptypes[NUM_SHIPDESIGNS];
    ships[0] = shipnum;
    shiptypes[0] = si;
    return game_send_fleet_do(g, owner, from, dest, ships, shiptypes, 1, false);
}

bool game_send_transport(struct game_s *g, struct planet_s *pf)
{
    transport_t *r;
    const planet_t *pt;
    player_id_t owner;
    if (g->transport_num == TRANSPORT_MAX) {
        log_warning("transport table (size %i) full, could not send!\n", TRANSPORT_MAX);
        return false;
    }
    pt = &(g->planet[pf->trans_dest]);
    owner = pf->owner;
    r = (&g->transport[g->transport_num]);
    r->owner = owner;
    r->x = pf->x;
    r->y = pf->y;
    r->dest = pf->trans_dest;
    {
        uint8_t speed = g->eto[owner].have_engine;
        if ((pt->owner == owner) && pt->have_stargate && pf->have_stargate) {
            speed = FLEET_SPEED_STARGATE;
        }
        r->speed = speed;
    }
    {
        uint16_t n = pf->trans_num;
        r->pop = n;
        pf->pop -= n;
        SETMAX(pf->pop, 1);
        SUBSAT0(pf->rebels, n / 2 + 1);
    }
    pf->trans_num = 0;
    ++g->transport_num;
    return true;
}

void game_remove_empty_fleets(struct game_s *g)
{
    for (int i = 0; i < g->enroute_num; ++i) {
        fleet_enroute_t *r = &(g->enroute[i]);
        bool is_empty;
        is_empty = true;
        for (int j = 0; j < NUM_SHIPDESIGNS; ++j) {
            if (r->ships[j] != 0) {
                is_empty = false;
                break;
            }
        }
        if (is_empty) {
            util_table_remove_item_any_order(i, g->enroute, sizeof(fleet_enroute_t), g->enroute_num);
            --g->enroute_num;
            --i;
        }
    }
    for (int i = 0; i < g->transport_num; ++i) {
        transport_t *r = &(g->transport[i]);
        if (r->pop == 0) {
            util_table_remove_item_any_order(i, g->transport, sizeof(transport_t), g->transport_num);
            --g->transport_num;
            --i;
        }
    }
}

void game_remove_player_fleets(struct game_s *g, player_id_t owner)
{
    for (int i = 0; i < g->enroute_num; ++i) {
        fleet_enroute_t *r = &(g->enroute[i]);
        if (r->owner == owner) {
            util_table_remove_item_any_order(i, g->enroute, sizeof(fleet_enroute_t), g->enroute_num);
            --g->enroute_num;
            --i;
        }
    }
    for (int i = 0; i < g->transport_num; ++i) {
        transport_t *r = &(g->transport[i]);
        if (r->owner == owner) {
            util_table_remove_item_any_order(i, g->transport, sizeof(transport_t), g->transport_num);
            --g->transport_num;
            --i;
        }
    }
    memset(g->eto[owner].orbit, 0, sizeof(g->eto[owner].orbit));
}

bool game_fleet_any_dest_player(const struct game_s *g, player_id_t owner, player_id_t target)
{
    for (int i = 0; i < g->enroute_num; ++i) {
        const fleet_enroute_t *r = &(g->enroute[i]);
        if ((r->owner == owner) && (g->planet[r->dest].owner == target)) {
            return true;
        }
    }
    return false;
}

void game_fleet_unrefuel(struct game_s *g)
{
    for (player_id_t pi = PLAYER_0; pi < g->players; ++pi) {
        const shipresearch_t *srd = &(g->srd[pi]);
        empiretechorbit_t *e = &(g->eto[pi]);
        for (uint8_t pli = 0; pli < g->galaxy_stars; ++pli) {
            planet_t *p = &(g->planet[pli]);
            uint8_t wr;
            wr = p->within_frange[pi];
            if (wr != 1) {
                fleet_orbit_t *r = &(e->orbit[pli]);
                bool unfueled;
                unfueled = false;
                for (int si = 0; si < e->shipdesigns_num; ++si) {
                    if (1
                      && (r->ships[si] != 0)
                      && ((wr == 0) || ((wr == 2) && (!srd->have_reserve_fuel[si])))
                    ) {
                        unfueled = true;
                        r->ships[si] = 0;
                    }
                }
                if (unfueled) {
                    BOOLVEC_SET1(p->unrefuel, pi);
                }
            }
        }
    }
}

uint8_t game_fleet_get_speed(const struct game_s *g, const struct fleet_enroute_s *r, uint8_t pfrom, uint8_t pto)
{
    if (game_num_stargate_redir_fix && ((r->speed == FLEET_SPEED_STARGATE) || (pfrom != PLANET_NONE))) {
        player_id_t owner = r->owner;
        const planet_t *pt = (&g->planet[pto]);
        const planet_t *pf = (pfrom != PLANET_NONE) ? (&g->planet[pfrom]) : 0;
        if (1
          && ((pt->owner == owner) && pt->have_stargate)
          && ((r->speed == FLEET_SPEED_STARGATE) || (pf && (pf->owner == owner) && pf->have_stargate))
        ) {
            return FLEET_SPEED_STARGATE;
        } else {
            const shipdesign_t *sd = (&g->srd[owner].design[0]);
            uint8_t speed = 100;
            for (int i = 0; i < NUM_SHIPDESIGNS; ++i) {
                if (r->ships[i] > 0) {
                    uint8_t s;
                    s = sd[i].engine + 1;
                    SETMIN(speed, s);
                }
            }
            return speed;
        }
    } else {
        return r->speed;
    }
}

void game_fleet_redirect(struct game_s *g, struct fleet_enroute_s *r, uint8_t pfrom, uint8_t pto)
{
    if ((pfrom != PLANET_NONE) && (pfrom == pto)) {
        shipcount_t *os;
        os = &(g->eto[r->owner].orbit[pto].ships[0]);
        for (int i = 0; i < NUM_SHIPDESIGNS; ++i) {
            os[i] += r->ships[i];
        }
        util_table_remove_item_any_order(r - g->enroute, g->enroute, sizeof(fleet_enroute_t), g->enroute_num);
        --g->enroute_num;
    } else {
        r->dest = pto;
        r->speed = game_fleet_get_speed(g, r, pfrom, pto);
    }
}
