/* NOTE!
   If the save format changes, increase GAME_SAVE_VERSION and implement a converter in 1oom_saveconv.
   The format is not to be changed without a very good reason.
*/
#include "config.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "game_nebula.h"
#include "game_save.h"
#include "bits.h"
#include "game.h"
#include "game_aux.h"
#include "game_endecode.h"
#include "lib.h"
#include "log.h"
#include "os.h"
#include "types.h"
#include "util.h"

/* -------------------------------------------------------------------------- */

#define GAME_SAVE_HDR_SIZE  64
#define GAME_SAVE_MAGIC "1oomSAVE"
#define GAME_SAVE_END   0x646e450a/*dnE\n*/
#define GAME_SAVE_OFFS_VERSION  8
#define GAME_SAVE_OFFS_NAME 16

#define GAME_SAVE_VERSION   0

/* -------------------------------------------------------------------------- */

bool game_save_tbl_have_save[NUM_ALL_SAVES];
char game_save_tbl_name[NUM_ALL_SAVES][SAVE_NAME_LEN];

/* -------------------------------------------------------------------------- */

/*
 * HACK: Nebula types are not stored in the save file format, but can be inferred
 * from the bounding boxes.
 */
static bool game_save_nebula_match(uint16_t x, uint16_t y, const uint16_t *x0, const uint16_t *y0, const uint16_t *x1, const uint16_t *y1, const uint8_t nebula_data[4][4]) {
    for (int i = 0; i < 4; ++i) {
        if (x0[i] != x + nebula_data[0][i]) {
            return false;
        }
        if (y0[i] != y + nebula_data[1][i]) {
            return false;
        }
        if (x1[i] != x + nebula_data[2][i]) {
            return false;
        }
        if (y1[i] != y + nebula_data[3][i]) {
            return false;
        }
    }
    return true;
}

static int game_save_infer_nebula_type(const struct game_s *g, uint32_t index)
{
    uint16_t x  = g->nebula_x[index];
    uint16_t y  = g->nebula_y[index];
    const uint16_t *x0 = g->nebula_x0[index];
    const uint16_t *y0 = g->nebula_y0[index];
    const uint16_t *x1 = g->nebula_x1[index];
    const uint16_t *y1 = g->nebula_y1[index];
    for (int t = 0; t < NEBULA_TYPE_NUM; ++t) {
        if (game_save_nebula_match(x, y, x0, y0, x1, y1, tbl_nebula_data[t])) {
            return t;
        }
    }
    return -1;
}

static int game_save_encode_planet(uint8_t *buf, int pos, const planet_t *p, int pnum, uint32_t version)
{
    SG_1OOM_EN_TBL_U8(p->name, PLANET_NAME_LEN);
    SG_1OOM_EN_U16(p->x);
    SG_1OOM_EN_U16(p->y);
    SG_1OOM_EN_U8(p->star_type);
    SG_1OOM_EN_U8(p->look);
    SG_1OOM_EN_U8(p->frame);
    SG_1OOM_EN_U8(p->rocks);
    SG_1OOM_EN_U16(p->max_pop1);
    SG_1OOM_EN_U16(p->max_pop2);
    SG_1OOM_EN_U16(p->max_pop3);
    SG_1OOM_EN_U8(p->type);
    SG_1OOM_EN_U8(p->battlebg);
    SG_1OOM_EN_U8(p->infogfx);
    SG_1OOM_EN_U8(p->growth);
    SG_1OOM_EN_U8(p->special);
    SG_1OOM_EN_U16(p->bc_to_ecoproj);
    SG_1OOM_EN_U16(p->bc_to_ship);
    SG_1OOM_EN_U16(p->bc_to_factory);
    SG_1OOM_EN_U32(p->reserve);
    SG_1OOM_EN_U16(p->waste);
    SG_1OOM_EN_U8(p->owner);
    SG_1OOM_EN_U8(p->prev_owner);
    SG_1OOM_EN_U8(p->claim);
    SG_1OOM_EN_U16(p->pop);
    SG_1OOM_EN_U16(p->pop_prev);
    SG_1OOM_EN_U16(p->factories);
    SG_1OOM_EN_TBL_U16(p->slider, PLANET_SLIDER_NUM);
    SG_1OOM_EN_TBL_U8(p->slider_lock, PLANET_SLIDER_NUM);
    SG_1OOM_EN_U8(p->buildship);
    SG_1OOM_EN_U8(p->reloc);
    SG_1OOM_EN_U16(p->missile_bases);
    SG_1OOM_EN_U16(p->bc_to_base);
    SG_1OOM_EN_U16(p->bc_upgrade_base);
    SG_1OOM_EN_U8(p->have_stargate);
    SG_1OOM_EN_U8(p->shield);
    SG_1OOM_EN_U16(p->bc_to_shield);
    SG_1OOM_EN_U16(p->trans_num);
    SG_1OOM_EN_U8(p->trans_dest);
    SG_1OOM_EN_U8(p->pop_tenths);
    SG_1OOM_EN_BV(p->explored, PLAYER_NUM);
    SG_1OOM_EN_BV(p->unrefuel, PLAYER_NUM);
    SG_1OOM_EN_DUMMY(4);
    SG_1OOM_EN_U8(p->pop_oper_fact);
    SG_1OOM_EN_U16(p->bc_to_refit);
    SG_1OOM_EN_U16(p->rebels);
    SG_1OOM_EN_U8(p->unrest);
    SG_1OOM_EN_U8(p->unrest_reported);
    SG_1OOM_EN_BV(p->finished, FINISHED_NUM);
    SG_1OOM_EN_DUMMY(5);
    return pos;
}

static int game_save_decode_planet(const uint8_t *buf, int pos, planet_t *p, int pnum, uint32_t version)
{
    SG_1OOM_DE_TBL_U8(p->name, PLANET_NAME_LEN);
    SG_1OOM_DE_U16(p->x);
    SG_1OOM_DE_U16(p->y);
    SG_1OOM_DE_U8(p->star_type);
    SG_1OOM_DE_U8(p->look);
    SG_1OOM_DE_U8(p->frame);
    SG_1OOM_DE_U8(p->rocks);
    SG_1OOM_DE_U16(p->max_pop1);
    SG_1OOM_DE_U16(p->max_pop2);
    SG_1OOM_DE_U16(p->max_pop3);
    SG_1OOM_DE_U8(p->type);
    SG_1OOM_DE_U8(p->battlebg);
    SG_1OOM_DE_U8(p->infogfx);
    SG_1OOM_DE_U8(p->growth);
    SG_1OOM_DE_U8(p->special);
    SG_1OOM_DE_U16(p->bc_to_ecoproj);
    SG_1OOM_DE_U16(p->bc_to_ship);
    SG_1OOM_DE_U16(p->bc_to_factory);
    SG_1OOM_DE_U32(p->reserve);
    SG_1OOM_DE_U16(p->waste);
    SG_1OOM_DE_U8(p->owner);
    SG_1OOM_DE_U8(p->prev_owner);
    SG_1OOM_DE_U8(p->claim);
    SG_1OOM_DE_U16(p->pop);
    SG_1OOM_DE_U16(p->pop_prev);
    SG_1OOM_DE_U16(p->factories);
    SG_1OOM_DE_TBL_U16(p->slider, PLANET_SLIDER_NUM);
    SG_1OOM_DE_TBL_U8(p->slider_lock, PLANET_SLIDER_NUM);
    SG_1OOM_DE_U8(p->buildship);
    SG_1OOM_DE_U8(p->reloc);
    SG_1OOM_DE_U16(p->missile_bases);
    SG_1OOM_DE_U16(p->bc_to_base);
    SG_1OOM_DE_U16(p->bc_upgrade_base);
    SG_1OOM_DE_U8(p->have_stargate);
    SG_1OOM_DE_U8(p->shield);
    SG_1OOM_DE_U16(p->bc_to_shield);
    SG_1OOM_DE_U16(p->trans_num);
    SG_1OOM_DE_U8(p->trans_dest);
    SG_1OOM_DE_U8(p->pop_tenths);
    SG_1OOM_DE_BV(p->explored, PLAYER_NUM);
    SG_1OOM_DE_BV(p->unrefuel, PLAYER_NUM);
    SG_1OOM_DE_DUMMY(4);
    SG_1OOM_DE_U8(p->pop_oper_fact);
    SG_1OOM_DE_U16(p->bc_to_refit);
    SG_1OOM_DE_U16(p->rebels);
    SG_1OOM_DE_U8(p->unrest);
    SG_1OOM_DE_U8(p->unrest_reported);
    SG_1OOM_DE_BV(p->finished, FINISHED_NUM);
    SG_1OOM_DE_DUMMY(5);
    return pos;
}

static int game_save_encode_enroute(uint8_t *buf, int pos, const fleet_enroute_t *r)
{
    SG_1OOM_EN_U8(r->owner);
    SG_1OOM_EN_U16(r->x);
    SG_1OOM_EN_U16(r->y);
    SG_1OOM_EN_U8(r->dest);
    {
        uint8_t v = r->speed;
        if (r->retreat) {
            v |= 0x80;
        }
        SG_1OOM_EN_U8(v);
    }
    SG_1OOM_EN_TBL_U16(r->ships, NUM_SHIPDESIGNS);
    return pos;
}

static int game_save_decode_enroute(const uint8_t *buf, int pos, fleet_enroute_t *r)
{
    SG_1OOM_DE_U8(r->owner);
    SG_1OOM_DE_U16(r->x);
    SG_1OOM_DE_U16(r->y);
    SG_1OOM_DE_U8(r->dest);
    {
        uint8_t v;
        SG_1OOM_DE_U8(v);
        r->speed = v & 0x7f;
        r->retreat = ((v & 0x80) != 0);
    }
    SG_1OOM_DE_TBL_U16(r->ships, NUM_SHIPDESIGNS);
    return pos;
}

static int game_save_encode_transport(uint8_t *buf, int pos, const transport_t *r)
{
    SG_1OOM_EN_U8(r->owner);
    SG_1OOM_EN_U16(r->x);
    SG_1OOM_EN_U16(r->y);
    SG_1OOM_EN_U8(r->dest);
    SG_1OOM_EN_U8(r->speed);
    SG_1OOM_EN_U16(r->pop);
    return pos;
}

static int game_save_decode_transport(const uint8_t *buf, int pos, transport_t *r)
{
    SG_1OOM_DE_U8(r->owner);
    SG_1OOM_DE_U16(r->x);
    SG_1OOM_DE_U16(r->y);
    SG_1OOM_DE_U8(r->dest);
    SG_1OOM_DE_U8(r->speed);
    SG_1OOM_DE_U16(r->pop);
    return pos;
}

static int game_save_encode_orbits(uint8_t *buf, int pos, const fleet_orbit_t *o, int planets)
{
    for (uint8_t i = 0; i < planets; ++i) {
        bool any_ships;
        any_ships = false;
        for (int j = 0; j < NUM_SHIPDESIGNS; ++j) {
            if (o[i].ships[j] != 0) {
                any_ships = true;
                break;
            }
        }
        if (any_ships) {
            SG_1OOM_EN_U8(i);
            SG_1OOM_EN_TBL_U16(o[i].ships, NUM_SHIPDESIGNS);
        }
    }
    SG_1OOM_EN_U8(PLANET_NONE);
    return pos;
}

static int game_save_decode_orbits(const uint8_t *buf, int pos, fleet_orbit_t *o, int planets)
{
    for (int loops = 0; loops <= planets; ++loops) {
        uint8_t i;
        SG_1OOM_DE_U8(i);
        if (i == PLANET_NONE) {
            return pos;
        } else if (i >= planets) {
            log_error("Save: decode orbit planet %i >= %i max\n", i, planets);
            return -1;
        }
        SG_1OOM_DE_TBL_U16(o[i].ships, NUM_SHIPDESIGNS);
    }
    log_error("Save: decode orbit terminator missing\n");
    return -1;
}

static int game_save_encode_eto(uint8_t *buf, int pos, const empiretechorbit_t *e, int pnum, int planets, uint32_t version)
{
    SG_1OOM_EN_U8(e->race);
    SG_1OOM_EN_U8(e->banner);
    SG_1OOM_EN_U8(e->trait1);
    SG_1OOM_EN_U8(e->trait2);
    SG_1OOM_EN_U8(e->ai_p3_countdown);
    SG_1OOM_EN_U8(e->ai_p2_countdown);
    SG_1OOM_EN_BV(e->contact, PLAYER_NUM);
    SG_1OOM_EN_BV(e->contact_broken, PLAYER_NUM);
    SG_1OOM_EN_DUMMY(4);
    SG_1OOM_EN_TBL_U16(e->relation1, pnum);
    SG_1OOM_EN_TBL_U16(e->relation2, pnum);
    SG_1OOM_EN_TBL_U8(e->diplo_type, pnum);
    SG_1OOM_EN_TBL_U16(e->diplo_val, pnum);
    SG_1OOM_EN_TBL_U16(e->diplo_p1, pnum);
    SG_1OOM_EN_TBL_U16(e->diplo_p2, pnum);
    SG_1OOM_EN_TBL_U16(e->trust, pnum);
    SG_1OOM_EN_TBL_U8(e->broken_treaty, pnum);
    SG_1OOM_EN_TBL_U16(e->blunder, pnum);
    SG_1OOM_EN_TBL_U8(e->tribute_field, pnum);
    SG_1OOM_EN_TBL_U8(e->tribute_tech, pnum);
    SG_1OOM_EN_TBL_U16(e->mood_treaty, pnum);
    SG_1OOM_EN_TBL_U16(e->mood_trade, pnum);
    SG_1OOM_EN_TBL_U16(e->mood_tech, pnum);
    SG_1OOM_EN_TBL_U16(e->mood_peace, pnum);
    SG_1OOM_EN_TBL_U8(e->treaty, pnum);
    SG_1OOM_EN_TBL_U16(e->trade_bc, pnum);
    SG_1OOM_EN_TBL_U16(e->trade_percent, pnum);
    SG_1OOM_EN_TBL_U8(e->spymode_next, pnum);
    SG_1OOM_EN_TBL_U8(e->offer_field, pnum);
    SG_1OOM_EN_TBL_U8(e->offer_tech, pnum);
    SG_1OOM_EN_TBL_U16(e->offer_bc, pnum);
    {
        /* HACK Fit 5 tables into 2 while not breaking old versions. */
        uint16_t tbl[PLAYER_NUM];
        for (player_id_t i = pnum; i < PLAYER_NUM; ++i) {
            tbl[i] = 0; /* keep compiler happy */
        }
        /* Old attack_bounty. The attack_gift_* are irrelevant if attack_bounty is PLAYER_NONE. */
        for (player_id_t i = 0; i < pnum; ++i) {
            uint16_t v;
            if (e->attack_gift_bc[i] != 0) {
                v = e->attack_gift_bc[i]; /* multiple of 50, always even */
            } else if (e->attack_gift_tech[i] != 0) {
                v = (((uint16_t)e->attack_gift_tech[i]) << 8) | (e->attack_gift_field[i] << 1) | 1;
            } else {
                v = 0;
            }
            tbl[i] = v;
        }
        SG_1OOM_EN_TBL_U16(tbl, pnum);
        /* Old bounty_collect.
           attack_bounty[0] is never 0, and thus tbl[0] is never PLAYER_NONE (but typically (PLAYER_NONE << 8) | PLAYER_NONE).
           bounty_collect[0] is always PLAYER_NONE.
           Old code reading this to bounty_collect will get != PLAYER_NONE and reset attack_bounty on next turn.
        */
        for (player_id_t i = 0; i < pnum; ++i) {
            tbl[i] = (((uint16_t)e->attack_bounty[i]) << 8) | e->bounty_collect[i];
        }
        SG_1OOM_EN_TBL_U16(tbl, pnum);
    }
    SG_1OOM_EN_TBL_U16(e->hatred, pnum);
    SG_1OOM_EN_TBL_U16(e->have_met, pnum);
    SG_1OOM_EN_TBL_U16(e->trade_established_bc, pnum);
    SG_1OOM_EN_TBL_U16(e->spying, pnum);
    SG_1OOM_EN_TBL_U16(e->spyfund, pnum);
    SG_1OOM_EN_TBL_U8(e->spymode, pnum);
    SG_1OOM_EN_U16(e->security);
    SG_1OOM_EN_TBL_U16(e->spies, pnum);
    SG_1OOM_EN_U32(e->reserve_bc);
    SG_1OOM_EN_U16(e->tax);
    SG_1OOM_EN_U8(e->base_shield);
    SG_1OOM_EN_U8(e->base_comp);
    SG_1OOM_EN_U8(e->base_weapon);
    SG_1OOM_EN_U8(e->colonist_oper_factories);
    SG_1OOM_EN_TBL_U8(e->tech.percent, TECH_FIELD_NUM);
    SG_1OOM_EN_TBL_U16(e->tech.slider, TECH_FIELD_NUM);
    SG_1OOM_EN_TBL_U8(e->tech.slider_lock, TECH_FIELD_NUM);
    SG_1OOM_EN_TBL_U32(e->tech.investment, TECH_FIELD_NUM);
    SG_1OOM_EN_TBL_U8(e->tech.project, TECH_FIELD_NUM);
    SG_1OOM_EN_TBL_U32(e->tech.cost, TECH_FIELD_NUM);
    SG_1OOM_EN_TBL_U16(e->tech.completed, TECH_FIELD_NUM);
    SG_1OOM_EN_U8(e->shipdesigns_num);
    pos = game_save_encode_orbits(buf, pos, e->orbit, planets);
    SG_1OOM_EN_TBL_TBL_U8(e->spyreportfield, pnum, TECH_FIELD_NUM);
    SG_1OOM_EN_TBL_U16(e->spyreportyear, pnum);
    SG_1OOM_EN_U8(e->shipi_colony);
    SG_1OOM_EN_U8(e->shipi_bomber);
    return pos;
}

static int game_save_decode_eto(const uint8_t *buf, int pos, empiretechorbit_t *e, int pnum, int planets, uint32_t version)
{
    SG_1OOM_DE_U8(e->race);
    SG_1OOM_DE_U8(e->banner);
    SG_1OOM_DE_U8(e->trait1);
    SG_1OOM_DE_U8(e->trait2);
    SG_1OOM_DE_U8(e->ai_p3_countdown);
    SG_1OOM_DE_U8(e->ai_p2_countdown);
    SG_1OOM_DE_BV(e->contact, PLAYER_NUM);
    SG_1OOM_DE_BV(e->contact_broken, PLAYER_NUM);
    SG_1OOM_DE_DUMMY(4);
    SG_1OOM_DE_TBL_U16(e->relation1, pnum);
    SG_1OOM_DE_TBL_U16(e->relation2, pnum);
    SG_1OOM_DE_TBL_U8(e->diplo_type, pnum);
    SG_1OOM_DE_TBL_U16(e->diplo_val, pnum);
    SG_1OOM_DE_TBL_U16(e->diplo_p1, pnum);
    SG_1OOM_DE_TBL_U16(e->diplo_p2, pnum);
    SG_1OOM_DE_TBL_U16(e->trust, pnum);
    SG_1OOM_DE_TBL_U8(e->broken_treaty, pnum);
    SG_1OOM_DE_TBL_U16(e->blunder, pnum);
    SG_1OOM_DE_TBL_U8(e->tribute_field, pnum);
    SG_1OOM_DE_TBL_U8(e->tribute_tech, pnum);
    SG_1OOM_DE_TBL_U16(e->mood_treaty, pnum);
    SG_1OOM_DE_TBL_U16(e->mood_trade, pnum);
    SG_1OOM_DE_TBL_U16(e->mood_tech, pnum);
    SG_1OOM_DE_TBL_U16(e->mood_peace, pnum);
    SG_1OOM_DE_TBL_U8(e->treaty, pnum);
    SG_1OOM_DE_TBL_U16(e->trade_bc, pnum);
    SG_1OOM_DE_TBL_U16(e->trade_percent, pnum);
    SG_1OOM_DE_TBL_U8(e->spymode_next, pnum);
    SG_1OOM_DE_TBL_U8(e->offer_field, pnum);
    SG_1OOM_DE_TBL_U8(e->offer_tech, pnum);
    SG_1OOM_DE_TBL_U16(e->offer_bc, pnum);
    {
        uint16_t ab[PLAYER_NUM];
        uint16_t bc[PLAYER_NUM];
        for (player_id_t i = pnum; i < PLAYER_NUM; ++i) {   /* keep compiler happy */
            ab[i] = 0;
            bc[i] = 0;
        }
        SG_1OOM_DE_TBL_U16(ab, pnum);
        SG_1OOM_DE_TBL_U16(bc, pnum);
        if (bc[0] != PLAYER_NONE) { /* New save, demangle the data. */
            for (player_id_t i = 0; i < pnum; ++i) {
                uint16_t v;
                v = bc[i];
                e->attack_bounty[i] = (v >> 8) & 0xf;
                e->bounty_collect[i] = v & 0xf;
                v = ab[i];
                if (v & 1) {
                    e->attack_gift_tech[i] = (v >> 8) & 0xff;
                    e->attack_gift_field[i] = (v >> 1) & 0xf;
                } else {
                    e->attack_gift_bc[i] = v;
                }
            }
        } else { /* Old save, simply copy the tables. */
            for (player_id_t i = 0; i < pnum; ++i) {
                e->attack_bounty[i] = ab[i];
                e->bounty_collect[i] = bc[i];
            }
        }
    }
    SG_1OOM_DE_TBL_U16(e->hatred, pnum);
    SG_1OOM_DE_TBL_U16(e->have_met, pnum);
    SG_1OOM_DE_TBL_U16(e->trade_established_bc, pnum);
    SG_1OOM_DE_TBL_U16(e->spying, pnum);
    SG_1OOM_DE_TBL_U16(e->spyfund, pnum);
    SG_1OOM_DE_TBL_U8(e->spymode, pnum);
    SG_1OOM_DE_U16(e->security);
    SG_1OOM_DE_TBL_U16(e->spies, pnum);
    SG_1OOM_DE_U32(e->reserve_bc);
    SG_1OOM_DE_U16(e->tax);
    SG_1OOM_DE_U8(e->base_shield);
    SG_1OOM_DE_U8(e->base_comp);
    SG_1OOM_DE_U8(e->base_weapon);
    SG_1OOM_DE_U8(e->colonist_oper_factories);
    SG_1OOM_DE_TBL_U8(e->tech.percent, TECH_FIELD_NUM);
    SG_1OOM_DE_TBL_U16(e->tech.slider, TECH_FIELD_NUM);
    SG_1OOM_DE_TBL_U8(e->tech.slider_lock, TECH_FIELD_NUM);
    SG_1OOM_DE_TBL_U32(e->tech.investment, TECH_FIELD_NUM);
    SG_1OOM_DE_TBL_U8(e->tech.project, TECH_FIELD_NUM);
    SG_1OOM_DE_TBL_U32(e->tech.cost, TECH_FIELD_NUM);
    SG_1OOM_DE_TBL_U16(e->tech.completed, TECH_FIELD_NUM);
    SG_1OOM_DE_U8(e->shipdesigns_num);
    if ((e->shipdesigns_num > NUM_SHIPDESIGNS)) {
        log_error("Save: decode invalid number of ship designs %i\n", e->shipdesigns_num);
        return -1;
    }
    pos = game_save_decode_orbits(buf, pos, e->orbit, planets);
    if (pos < 0) {
        return -1;
    }
    SG_1OOM_DE_TBL_TBL_U8(e->spyreportfield, pnum, TECH_FIELD_NUM);
    SG_1OOM_DE_TBL_U16(e->spyreportyear, pnum);
    SG_1OOM_DE_U8(e->shipi_colony);
    SG_1OOM_DE_U8(e->shipi_bomber);
    return pos;
}

static int game_save_encode_sd(uint8_t *buf, int pos, const shipdesign_t *sd)
{
    SG_1OOM_EN_TBL_U8(sd->name, SHIP_NAME_LEN);
    SG_1OOM_EN_U16(sd->cost);
    SG_1OOM_EN_U16(sd->space);
    SG_1OOM_EN_U8(sd->hull);
    SG_1OOM_EN_U8(sd->look);
    SG_1OOM_EN_TBL_U8(sd->wpnt, WEAPON_SLOT_NUM);
    SG_1OOM_EN_TBL_U8(sd->wpnn, WEAPON_SLOT_NUM);
    SG_1OOM_EN_U8(sd->engine);
    SG_1OOM_EN_U32(sd->engines);
    SG_1OOM_EN_TBL_U8(sd->special, SPECIAL_SLOT_NUM);
    SG_1OOM_EN_U8(sd->shield);
    SG_1OOM_EN_U8(sd->jammer);
    SG_1OOM_EN_U8(sd->comp);
    SG_1OOM_EN_U8(sd->armor);
    SG_1OOM_EN_U8(sd->man);
    SG_1OOM_EN_U16(sd->hp);
    return pos;
}

static int game_save_decode_sd(const uint8_t *buf, int pos, shipdesign_t *sd)
{
    SG_1OOM_DE_TBL_U8(sd->name, SHIP_NAME_LEN);
    SG_1OOM_DE_U16(sd->cost);
    SG_1OOM_DE_U16(sd->space);
    SG_1OOM_DE_U8(sd->hull);
    SG_1OOM_DE_U8(sd->look);
    SG_1OOM_DE_TBL_U8(sd->wpnt, WEAPON_SLOT_NUM);
    SG_1OOM_DE_TBL_U8(sd->wpnn, WEAPON_SLOT_NUM);
    SG_1OOM_DE_U8(sd->engine);
    SG_1OOM_DE_U32(sd->engines);
    SG_1OOM_DE_TBL_U8(sd->special, SPECIAL_SLOT_NUM);
    SG_1OOM_DE_U8(sd->shield);
    SG_1OOM_DE_U8(sd->jammer);
    SG_1OOM_DE_U8(sd->comp);
    SG_1OOM_DE_U8(sd->armor);
    SG_1OOM_DE_U8(sd->man);
    SG_1OOM_DE_U16(sd->hp);
    return pos;
}

static int game_save_encode_srd(uint8_t *buf, int pos, const shipresearch_t *srd, int sdnum, uint32_t version)
{
    for (int i = 0; i < sdnum; ++i) {
        pos = game_save_encode_sd(buf, pos, &(srd->design[i]));
    }
    for (int f = 0; f < TECH_FIELD_NUM; ++f) {
        SG_1OOM_EN_TBL_TBL_U8(srd->researchlist[f], TECH_TIER_NUM, 3);
    }
    SG_1OOM_EN_TBL_TBL_U8(srd->researchcompleted, TECH_FIELD_NUM, TECH_PER_FIELD);
    SG_1OOM_EN_DUMMY(NUM_SHIPDESIGNS);
    SG_1OOM_EN_TBL_U16(srd->year, NUM_SHIPDESIGNS);
    SG_1OOM_EN_DUMMY(NUM_SHIPDESIGNS * 4);
    return pos;
}

static int game_save_decode_srd(const uint8_t *buf, int pos, shipresearch_t *srd, int sdnum, uint32_t version)
{
    for (int i = 0; i < sdnum; ++i) {
        pos = game_save_decode_sd(buf, pos, &(srd->design[i]));
    }
    for (int f = 0; f < TECH_FIELD_NUM; ++f) {
        SG_1OOM_DE_TBL_TBL_U8(srd->researchlist[f], TECH_TIER_NUM, 3);
    }
    SG_1OOM_DE_TBL_TBL_U8(srd->researchcompleted, TECH_FIELD_NUM, TECH_PER_FIELD);
    SG_1OOM_DE_DUMMY(NUM_SHIPDESIGNS);
    SG_1OOM_DE_TBL_U16(srd->year, NUM_SHIPDESIGNS);
    SG_1OOM_DE_DUMMY(NUM_SHIPDESIGNS * 4);
    return pos;
}

static int game_save_encode_monster(uint8_t *buf, int pos, const monster_t *m)
{
    SG_1OOM_EN_U8(m->exists);
    SG_1OOM_EN_U16(m->x);
    SG_1OOM_EN_U16(m->y);
    SG_1OOM_EN_U8(m->killer);
    SG_1OOM_EN_U8(m->dest);
    SG_1OOM_EN_U8(m->counter);
    SG_1OOM_EN_U8(m->nuked);
    return pos;
}

static int game_save_decode_monster(const uint8_t *buf, int pos, monster_t *m)
{
    SG_1OOM_DE_U8(m->exists);
    SG_1OOM_DE_U16(m->x);
    SG_1OOM_DE_U16(m->y);
    SG_1OOM_DE_U8(m->killer);
    SG_1OOM_DE_U8(m->dest);
    SG_1OOM_DE_U8(m->counter);
    SG_1OOM_DE_U8(m->nuked);
    return pos;
}

static int game_save_encode_evn(uint8_t *buf, int pos, const gameevents_t *ev, int pnum, uint32_t version)
{
    SG_1OOM_EN_U16(ev->year);
    SG_1OOM_EN_BV(ev->done, GAME_EVENT_TBL_NUM);
    SG_1OOM_EN_U8(ev->diplo_msg_subtype);
    SG_1OOM_EN_DUMMY(16);
    SG_1OOM_EN_U8(ev->have_plague);
    SG_1OOM_EN_U8(ev->plague_player);
    SG_1OOM_EN_U8(ev->plague_planet_i);
    SG_1OOM_EN_U32(ev->plague_val);
    SG_1OOM_EN_U8(ev->have_nova);
    SG_1OOM_EN_U8(ev->nova_player);
    SG_1OOM_EN_U8(ev->nova_planet_i);
    SG_1OOM_EN_U8(ev->nova_years);
    SG_1OOM_EN_U32(ev->nova_val);
    SG_1OOM_EN_U8(ev->have_accident);
    SG_1OOM_EN_U8(ev->accident_planet_i);
    SG_1OOM_EN_U8(ev->have_comet);
    SG_1OOM_EN_U8(ev->comet_player);
    SG_1OOM_EN_U8(ev->comet_planet_i);
    SG_1OOM_EN_U8(ev->comet_years);
    SG_1OOM_EN_U16(ev->comet_hp);
    SG_1OOM_EN_U16(ev->comet_dmg);
    SG_1OOM_EN_U8(ev->have_pirates);
    SG_1OOM_EN_U8(ev->pirates_planet_i);
    SG_1OOM_EN_U16(ev->pirates_hp);
    pos = game_save_encode_monster(buf, pos, &(ev->crystal));
    pos = game_save_encode_monster(buf, pos, &(ev->amoeba));
    SG_1OOM_EN_U8(ev->planet_orion_i);
    SG_1OOM_EN_U8(ev->have_guardian);
    SG_1OOM_EN_TBL_U8(ev->home, pnum);
    SG_1OOM_EN_U8(ev->report_stars);
    SG_1OOM_EN_TBL_TBL_U32(ev->new_ships, pnum, NUM_SHIPDESIGNS);
    SG_1OOM_EN_TBL_TBL_U16(ev->spies_caught, pnum, pnum);
    SG_1OOM_EN_TBL_TBL_U16(ev->ceasefire, pnum, pnum);
    for (int i = 0; i < pnum; ++i) {
        SG_1OOM_EN_BV(ev->help_shown[i], HELP_SHOWN_NUM);
        SG_1OOM_EN_DUMMY(14);
    }
    SG_1OOM_EN_TBL_U16(ev->build_finished_num, pnum);
    SG_1OOM_EN_TBL_U8(ev->voted, pnum);
    SG_1OOM_EN_TBL_U8(ev->best_ecorestore, pnum);
    SG_1OOM_EN_TBL_U8(ev->best_wastereduce, pnum);
    SG_1OOM_EN_TBL_U8(ev->best_roboctrl, pnum);
    SG_1OOM_EN_TBL_U8(ev->best_terraform, pnum);
    return pos;
}

static int game_save_decode_evn(const uint8_t *buf, int pos, gameevents_t *ev, int pnum, uint32_t version)
{
    SG_1OOM_DE_U16(ev->year);
    SG_1OOM_DE_BV(ev->done, GAME_EVENT_TBL_NUM);
    SG_1OOM_DE_U8(ev->diplo_msg_subtype);
    SG_1OOM_DE_DUMMY(16);
    SG_1OOM_DE_U8(ev->have_plague);
    SG_1OOM_DE_U8(ev->plague_player);
    SG_1OOM_DE_U8(ev->plague_planet_i);
    SG_1OOM_DE_U32(ev->plague_val);
    SG_1OOM_DE_U8(ev->have_nova);
    SG_1OOM_DE_U8(ev->nova_player);
    SG_1OOM_DE_U8(ev->nova_planet_i);
    SG_1OOM_DE_U8(ev->nova_years);
    SG_1OOM_DE_U32(ev->nova_val);
    SG_1OOM_DE_U8(ev->have_accident);
    SG_1OOM_DE_U8(ev->accident_planet_i);
    SG_1OOM_DE_U8(ev->have_comet);
    SG_1OOM_DE_U8(ev->comet_player);
    SG_1OOM_DE_U8(ev->comet_planet_i);
    SG_1OOM_DE_U8(ev->comet_years);
    SG_1OOM_DE_U16(ev->comet_hp);
    SG_1OOM_DE_U16(ev->comet_dmg);
    SG_1OOM_DE_U8(ev->have_pirates);
    SG_1OOM_DE_U8(ev->pirates_planet_i);
    SG_1OOM_DE_U16(ev->pirates_hp);
    pos = game_save_decode_monster(buf, pos, &(ev->crystal));
    pos = game_save_decode_monster(buf, pos, &(ev->amoeba));
    SG_1OOM_DE_U8(ev->planet_orion_i);
    SG_1OOM_DE_U8(ev->have_guardian);
    SG_1OOM_DE_TBL_U8(ev->home, pnum);
    SG_1OOM_DE_U8(ev->report_stars);
    SG_1OOM_DE_TBL_TBL_U32(ev->new_ships, pnum, NUM_SHIPDESIGNS);
    SG_1OOM_DE_TBL_TBL_U16(ev->spies_caught, pnum, pnum);
    SG_1OOM_DE_TBL_TBL_U16(ev->ceasefire, pnum, pnum);
    for (int i = 0; i < pnum; ++i) {
        SG_1OOM_DE_BV(ev->help_shown[i], HELP_SHOWN_NUM);
        SG_1OOM_DE_DUMMY(14);
    }
    SG_1OOM_DE_TBL_U16(ev->build_finished_num, pnum);
    SG_1OOM_DE_TBL_U8(ev->voted, pnum);
    SG_1OOM_DE_TBL_U8(ev->best_ecorestore, pnum);
    SG_1OOM_DE_TBL_U8(ev->best_wastereduce, pnum);
    SG_1OOM_DE_TBL_U8(ev->best_roboctrl, pnum);
    SG_1OOM_DE_TBL_U8(ev->best_terraform, pnum);
    return pos;
}

static int game_save_encode(uint8_t *buf, int buflen, const struct game_s *g, uint32_t version)
{
    int pos = 0;
    if (buflen < sizeof(*g)) {
        log_error("Save: BUG: encode expected len > %i, got %i\n", sizeof(*g), buflen);
        return -1;
    }
    SG_1OOM_EN_U8(g->players);
    SG_1OOM_EN_BV(g->is_ai, PLAYER_NUM);
    SG_1OOM_EN_BV(g->refuse, PLAYER_NUM);
    SG_1OOM_EN_U8(g->ai_id);
    SG_1OOM_EN_DUMMY(3);
    SG_1OOM_EN_U8(g->active_player);
    SG_1OOM_EN_U8(g->difficulty);
    SG_1OOM_EN_U8(g->galaxy_size);
    SG_1OOM_EN_U8(g->galaxy_w);
    SG_1OOM_EN_U8(g->galaxy_h);
    SG_1OOM_EN_U8(g->galaxy_stars);
    SG_1OOM_EN_U16(g->galaxy_maxx);
    SG_1OOM_EN_U16(g->galaxy_maxy);
    SG_1OOM_EN_U32(g->galaxy_seed);
    SG_1OOM_EN_U32(g->seed);
    SG_1OOM_EN_U16(g->year);
    SG_1OOM_EN_U16(g->enroute_num);
    SG_1OOM_EN_U16(g->transport_num);
    SG_1OOM_EN_U8(g->end);
    SG_1OOM_EN_U8(g->winner);
    SG_1OOM_EN_U8(g->election_held);
    SG_1OOM_EN_U8(g->nebula_num);
    SG_1OOM_EN_TBL_U16(g->nebula_x, g->nebula_num);
    SG_1OOM_EN_TBL_U16(g->nebula_y, g->nebula_num);
    SG_1OOM_EN_TBL_TBL_U16(g->nebula_x0, g->nebula_num, 4);
    SG_1OOM_EN_TBL_TBL_U16(g->nebula_x1, g->nebula_num, 4);
    SG_1OOM_EN_TBL_TBL_U16(g->nebula_y0, g->nebula_num, 4);
    SG_1OOM_EN_TBL_TBL_U16(g->nebula_y1, g->nebula_num, 4);
    SG_1OOM_EN_TBL_TBL_U8(g->emperor_names, g->players, EMPEROR_NAME_LEN);
    SG_1OOM_EN_TBL_U8(g->planet_focus_i, g->players);
    for (int i = 0; i < g->galaxy_stars; ++i) {
        pos = game_save_encode_planet(buf, pos, &(g->planet[i]), g->players, version);
    }
    for (int j = 0; j < g->players; ++j) {
        for (int i = 0; i < g->galaxy_stars; ++i) {
            const seen_t *s = &(g->seen[j][i]);
            SG_1OOM_EN_U8(s->owner);
            SG_1OOM_EN_U16(s->pop);
            SG_1OOM_EN_U16(s->bases);
            SG_1OOM_EN_U16(s->factories);
        }
    }
    for (int i = 0; i < g->enroute_num; ++i) {
        pos = game_save_encode_enroute(buf, pos, &(g->enroute[i]));
    }
    for (int i = 0; i < g->transport_num; ++i) {
        pos = game_save_encode_transport(buf, pos, &(g->transport[i]));
    }
    for (int i = 0; i < g->players; ++i) {
        pos = game_save_encode_eto(buf, pos, &(g->eto[i]), g->players, g->galaxy_stars, version);
    }
    for (int i = 0; i < g->players; ++i) {
        pos = game_save_encode_srd(buf, pos, &(g->srd[i]), g->eto[i].shipdesigns_num, version);
    }
    for (int i = 0; i < g->players; ++i) {
        pos = game_save_encode_sd(buf, pos, &(g->current_design[i]));
    }
    pos = game_save_encode_evn(buf, pos, &(g->evn), g->players, version);
    SG_1OOM_EN_U32(GAME_SAVE_END);
    return pos;
}

static int game_save_decode(const uint8_t *buf, int buflen, struct game_s *g, uint32_t version)
{
    int pos = 0;
    if (buflen < 512) {
        log_error("Save: decode expected len > %i, got %i\n", 512, buflen);
        return -1;
    }
    {
        struct game_aux_s *ga;
        ga = g->gaux;
        memset(g, 0, sizeof(*g));
        g->gaux = ga;
    }
    SG_1OOM_DE_U8(g->players);
    if ((g->players < 2) || (g->players > 6)) {
        log_error("Save: decode invalid number of players %i\n", g->players);
        return -1;
    }
    SG_1OOM_DE_BV(g->is_ai, PLAYER_NUM);
    SG_1OOM_DE_BV(g->refuse, PLAYER_NUM);
    SG_1OOM_DE_U8(g->ai_id);
    SG_1OOM_DE_DUMMY(3);
    SG_1OOM_DE_U8(g->active_player);
    SG_1OOM_DE_U8(g->difficulty);
    if ((g->difficulty < 0) || (g->difficulty >= DIFFICULTY_NUM)) {
        log_error("Save: decode invalid difficulty value %i\n", g->difficulty);
        return -1;
    }
    SG_1OOM_DE_U8(g->galaxy_size);
    SG_1OOM_DE_U8(g->galaxy_w);
    SG_1OOM_DE_U8(g->galaxy_h);
    SG_1OOM_DE_U8(g->galaxy_stars);
    if ((g->galaxy_stars > PLANETS_MAX)) {
        log_error("Save: decode invalid number of stars %i\n", g->galaxy_stars);
        return -1;
    }
    SG_1OOM_DE_U16(g->galaxy_maxx);
    SG_1OOM_DE_U16(g->galaxy_maxy);
    SG_1OOM_DE_U32(g->galaxy_seed);
    SG_1OOM_DE_U32(g->seed);
    SG_1OOM_DE_U16(g->year);
    SG_1OOM_DE_U16(g->enroute_num);
    if ((g->enroute_num > FLEET_ENROUTE_MAX)) {
        log_error("Save: decode invalid number of fleets %i\n", g->enroute_num);
        return -1;
    }
    SG_1OOM_DE_U16(g->transport_num);
    if ((g->transport_num > TRANSPORT_MAX)) {
        log_error("Save: decode invalid number of transports %i\n", g->transport_num);
        return -1;
    }
    SG_1OOM_DE_U8(g->end);
    SG_1OOM_DE_U8(g->winner);
    SG_1OOM_DE_U8(g->election_held);
    SG_1OOM_DE_U8(g->nebula_num);
    if ((g->nebula_num > NEBULA_MAX)) {
        log_error("Save: decode invalid number of nebula %i\n", g->nebula_num);
        return -1;
    }
    SG_1OOM_DE_TBL_U16(g->nebula_x, g->nebula_num);
    SG_1OOM_DE_TBL_U16(g->nebula_y, g->nebula_num);
    SG_1OOM_DE_TBL_TBL_U16(g->nebula_x0, g->nebula_num, 4);
    SG_1OOM_DE_TBL_TBL_U16(g->nebula_x1, g->nebula_num, 4);
    SG_1OOM_DE_TBL_TBL_U16(g->nebula_y0, g->nebula_num, 4);
    SG_1OOM_DE_TBL_TBL_U16(g->nebula_y1, g->nebula_num, 4);
    for (int i = 0; i < g->nebula_num; ++i) {
        g->nebula_type[i] = game_save_infer_nebula_type(g, i);
        if (g->nebula_type[i] < 0) {
            log_error("Save: unknown nebula type for nebula %i\n", i);
            return -1;
        }
    }

    SG_1OOM_DE_TBL_TBL_U8(g->emperor_names, g->players, EMPEROR_NAME_LEN);
    SG_1OOM_DE_TBL_U8(g->planet_focus_i, g->players);
    for (int i = 0; i < g->galaxy_stars; ++i) {
        pos = game_save_decode_planet(buf, pos, &(g->planet[i]), g->players, version);
    }
    for (int j = 0; j < g->players; ++j) {
        for (int i = 0; i < g->galaxy_stars; ++i) {
            seen_t *s = &(g->seen[j][i]);
            SG_1OOM_DE_U8(s->owner);
            SG_1OOM_DE_U16(s->pop);
            SG_1OOM_DE_U16(s->bases);
            SG_1OOM_DE_U16(s->factories);
        }
    }
    for (int i = 0; i < g->enroute_num; ++i) {
        pos = game_save_decode_enroute(buf, pos, &(g->enroute[i]));
    }
    for (int i = 0; i < g->transport_num; ++i) {
        pos = game_save_decode_transport(buf, pos, &(g->transport[i]));
    }
    for (int i = 0; i < g->players; ++i) {
        pos = game_save_decode_eto(buf, pos, &(g->eto[i]), g->players, g->galaxy_stars, version);
        if (pos < 0) {
            return -1;
        }
    }
    for (int i = 0; i < g->players; ++i) {
        pos = game_save_decode_srd(buf, pos, &(g->srd[i]), g->eto[i].shipdesigns_num, version);
    }
    for (int i = 0; i < g->players; ++i) {
        pos = game_save_decode_sd(buf, pos, &(g->current_design[i]));
    }
    pos = game_save_decode_evn(buf, pos, &(g->evn), g->players, version);
    {
        uint32_t v;
        SG_1OOM_DE_U32(v);
        if (pos > buflen) {
            log_error("Save: decode read len %i > got %i\n", pos, buflen);
            return -1;
        }
        if (v != GAME_SAVE_END) {
            log_error("Save: invalid end mark 0x%08x != 0x%08x at %i!\n", v, GAME_SAVE_END, pos);
            return -1;
        }
    }
    if (pos < buflen) {
        log_warning("Save: decode read len %i < got %i (left %i)\n", pos, buflen, buflen - pos);
    }
    /* generate g->refuse if needed  */
    if ((g->end == GAME_END_FINAL_WAR) && BOOLVEC_IS_CLEAR(g->refuse, PLAYER_NUM)) {
        for (player_id_t i = PLAYER_0; i < g->players; ++i) {
            if (IS_HUMAN(g, i) && IS_ALIVE(g, i)) {
                BOOLVEC_SET1(g->refuse, i);
            }
        }
    }
    g->guardian_killer = PLAYER_NONE;
    return 0;
}

/* -------------------------------------------------------------------------- */

static void game_save_make_header(uint8_t *buf, const char *savename, uint32_t version)
{
    memset(buf, 0, GAME_SAVE_HDR_SIZE);
    memcpy(buf, (const uint8_t *)GAME_SAVE_MAGIC, 8);
    SET_LE_32(&buf[GAME_SAVE_OFFS_VERSION], version);
    strncpy((char *)&buf[GAME_SAVE_OFFS_NAME], savename, SAVE_NAME_LEN);
}

static int game_save_do_save_do(const char *filename, const char *savename, const struct game_s *g, int savei, uint32_t version)
{
    FILE *fd;
    uint8_t hdr[GAME_SAVE_HDR_SIZE];
    int res = -1, len;
    if ((len = game_save_encode(g->gaux->savebuf, g->gaux->savebuflen, g, version)) <= 0) {
        return -1;
    }
    if (os_make_path_for(filename)) {
        log_error("Save: failed to create path for '%s'\n", filename);
    }
    game_save_make_header(hdr, savename, version);
    fd = game_save_open_check_header(filename, -1, false, 0, 0);
    if (fd) {
        /* file exists */
        fclose(fd);
        fd = NULL;
    }
    if ((savei >= 0) && (savei < (NUM_SAVES + 1))) {
        game_save_tbl_have_save[savei] = false;
        game_save_tbl_name[savei][0] = '\0';
    }
    fd = fopen(filename, "wb+");
    if (0
      || (!fd)
      || (fwrite(hdr, GAME_SAVE_HDR_SIZE, 1, fd) != 1)
      || (fwrite(g->gaux->savebuf, len, 1, fd) != 1)
    ) {
        log_error("Save: failed to save '%s'\n", filename);
        unlink(filename);
        goto done;
    }
    if ((savei >= 0) && (savei < (NUM_SAVES + 1))) {
        game_save_tbl_have_save[savei] = true;
        memcpy(game_save_tbl_name[savei], &hdr[GAME_SAVE_OFFS_NAME], SAVE_NAME_LEN);
    }
    log_message("Save: save '%s' '%s'\n", filename, savename);
    res = 0;
done:
    if (fd) {
        fclose(fd);
        fd = NULL;
    }
    return res;
}

static int game_save_do_load_do(const char *filename, struct game_s *g, int savei, char *savename)
{
    FILE *fd = NULL;
    int res = -1, len = 0;
    uint32_t version;

    fd = game_save_open_check_header(filename, savei, true, savename, &version);
    if ((!fd) || ((len = fread(g->gaux->savebuf, 1, g->gaux->savebuflen, fd)) == 0) || (!feof(fd))) {
        log_error("Save: failed to load '%s'\n", filename);
    } else if (game_save_decode(g->gaux->savebuf, len, g, version) != 0) {
        log_error("Save: invalid data on load '%s'\n", filename);
    } else {
        log_message("Save: load '%s'\n", filename);
        res = 0;
    }
    if (fd) {
        fclose(fd);
        fd = NULL;
    }
    return res;
}

/* -------------------------------------------------------------------------- */

void *game_save_open_check_header(const char *filename, int i, bool update_table, char *savename, uint32_t *versionptr)
{
    uint8_t hdr[GAME_SAVE_HDR_SIZE];
    FILE *fd;
    if ((i < 0) || (i >= NUM_ALL_SAVES)) {
        update_table = false;
    }
    if (update_table) {
        game_save_tbl_have_save[i] = false;
        game_save_tbl_name[i][0] = '\0';
    }
    if (savename) {
        savename[0] = '\0';
    }
    fd = fopen(filename, "rb");
    if (0
      || (!fd) || (fread(hdr, GAME_SAVE_HDR_SIZE, 1, fd) != 1)
      || (memcmp(hdr, (const uint8_t *)GAME_SAVE_MAGIC, 8) != 0)
    ) {
        goto fail;
    } else {
        uint32_t version = GET_LE_32(&hdr[GAME_SAVE_OFFS_VERSION]);
        if (versionptr) {
            *versionptr = version;
        }
        if (version > GAME_SAVE_VERSION) {
            goto fail;
        }
        if (update_table) {
            game_save_tbl_have_save[i] = true;
            memcpy(game_save_tbl_name[i], &hdr[GAME_SAVE_OFFS_NAME], SAVE_NAME_LEN);
            game_save_tbl_name[i][SAVE_NAME_LEN - 1] = '\0';
        }
        if (savename) {
            memcpy(savename, &hdr[GAME_SAVE_OFFS_NAME], SAVE_NAME_LEN);
            savename[SAVE_NAME_LEN - 1] = '\0';
        }
    }
    return fd;
fail:
    if (fd) {
        fclose(fd);
        fd = NULL;
    }
    return NULL;
}

int game_save_get_slot_fname(char *buf, int buflen, int i)
{
    const char *path = os_get_path_user();
    char namebuf[16];
    int res;
    if (!os_get_fname_save_slot(namebuf, sizeof(namebuf), i + 1)) {
        lib_sprintf(namebuf, sizeof(namebuf), "1oom_save%i.bin", i + 1);
    }
    res = util_concat_buf(buf, buflen, path, FSDEV_DIR_SEP_STR, namebuf, NULL);
    if (res < 0) {
        log_error("Save: BUG: save name buffer too small by %i bytes\n", -res);
        return -1;
    }
    return 0;
}

int game_save_get_year_fname(char *buf, int buflen, int year)
{
    const char *path = os_get_path_user();
    char namebuf[32];
    int res;
    if (!os_get_fname_save_year(namebuf, sizeof(namebuf), year)) {
        lib_sprintf(namebuf, sizeof(namebuf), "1oom_save_%i.bin", year);
    }
    res = util_concat_buf(buf, buflen, path, FSDEV_DIR_SEP_STR, namebuf, NULL);
    if (res < 0) {
        log_error("Save: BUG: save name buffer too small by %i bytes\n", -res);
        return -1;
    }
    return 0;
}

int game_save_check_saves(char *fnamebuf, int buflen)
{
    FILE *fd;

    for (int i = 0; i < NUM_ALL_SAVES; ++i) {
        game_save_get_slot_fname(fnamebuf, buflen, i);
        fd = game_save_open_check_header(fnamebuf, i, true, 0, 0);
        if (fd) {
            fclose(fd);
        }
    }
    return 0;
}

int game_save_do_load_fname(const char *filename, char *savename, struct game_s *g)
{
    return game_save_do_load_do(filename, g, -1, savename);
}

int game_save_do_save_fname(const char *filename, const char *savename, const struct game_s *g, uint32_t version)
{
    if (version > GAME_SAVE_VERSION) {
        log_error("Save: BUG: invalid version %i > %i\n", version, GAME_SAVE_VERSION);
        return -1;
    }
    return game_save_do_save_do(filename, savename, g, -1, version);
}

int game_save_do_load_i(int savei, struct game_s *g)
{
    int res;
    char *filename = g->gaux->savenamebuf;
    game_save_get_slot_fname(filename, g->gaux->savenamebuflen, savei);
    res = game_save_do_load_do(filename, g, savei, 0);
    return res;
}

int game_save_do_save_i(int savei, const char *savename, const struct game_s *g)
{
    int res;
    char *filename = g->gaux->savenamebuf;
    if (os_make_path_user()) {
        log_error("Save: failed to create user path '%s'\n", os_get_path_user());
    }
    game_save_get_slot_fname(filename, g->gaux->savenamebuflen, savei);
    res = game_save_do_save_do(filename, savename, g, savei, GAME_SAVE_VERSION);
    return res;
}

int game_save_do_load_year(int year, char *savename, struct game_s *g)
{
    int res;
    char *filename = g->gaux->savenamebuf;
    game_save_get_year_fname(filename, g->gaux->savenamebuflen, year);
    res = game_save_do_load_do(filename, g, -1, savename);
    return res;
}

int game_save_do_save_year(const char *savename, const struct game_s *g)
{
    int res;
    char *filename = g->gaux->savenamebuf;
    char buf[SAVE_NAME_LEN];
    if (os_make_path_user()) {
        log_error("Save: failed to create user path '%s'\n", os_get_path_user());
    }
    game_save_get_year_fname(filename, g->gaux->savenamebuflen, g->year + YEAR_BASE);
    if (!savename) {
        lib_sprintf(buf, sizeof(buf), "Year %i", g->year + YEAR_BASE);
        savename = buf;
    }
    res = game_save_do_save_do(filename, savename, g, -1, GAME_SAVE_VERSION);
    return res;
}
