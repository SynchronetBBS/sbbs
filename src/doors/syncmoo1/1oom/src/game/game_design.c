#include "config.h"

#include <string.h>

#include "game_design.h"
#include "boolvec.h"
#include "comp.h"
#include "game.h"
#include "game_fleet.h"
#include "game_misc.h"
#include "game_shipdesign.h"
#include "game_shiptech.h"
#include "game_str.h"
#include "game_tech.h"
#include "lib.h"
#include "log.h"
#include "rnd.h"
#include "types.h"
#include "util.h"

/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */

static void game_get_random_shipnames(struct game_s *g, player_id_t player, char shipnames[SHIP_HULL_NUM][SHIP_NAME_LEN + 1])
{
    BOOLVEC_DECLARE(name_unused, SHIP_NAME_NUM);
    const empiretechorbit_t *e = &(g->eto[player]);
    const shipresearch_t *srd = &(g->srd[player]);
    char const * const *names = &game_str_tbl_ship_names[e->race * SHIP_NAME_NUM];
    for (int n = 0; n < SHIP_NAME_NUM; ++n) {
        BOOLVEC_SET1(name_unused, n);
        for (int sdi = 0; sdi < e->shipdesigns_num; ++sdi) {
            if (strcmp(srd->design[sdi].name, names[n]) == 0) {
                BOOLVEC_SET0(name_unused, n);
                break;
            }
        }
    }
    /* game_get_random_unused_shipnames */
    for (int i = 0; i < SHIP_HULL_NUM; ++i) {
        shipnames[i][0] = '\0';
    }
    for (int i = SHIP_HULL_NUM - 1; i >= 0; --i) {
        int namei;
        bool flag_unused;
        flag_unused = false;
        namei = i * 3 + rnd_0_nm1(3, &g->seed);
        if (BOOLVEC_IS1(name_unused, namei)) {
            flag_unused = true;
        } else {
            while ((!flag_unused) && (namei < SHIP_NAME_NUM)) {
                if (BOOLVEC_IS1(name_unused, namei)) {
                    flag_unused = true;
                } else {
                    ++namei;
                }
            }
            if (!flag_unused) {
                --namei;
                while ((!flag_unused) && (namei >= 0)) {
                    if (BOOLVEC_IS1(name_unused, namei)) {
                        flag_unused = true;
                    } else {
                        --namei;
                    }
                }
            }
            if (!flag_unused) {
                namei = 0;
            }
        }
        if ((namei >= 0) && (namei < SHIP_NAME_NUM)) {
            BOOLVEC_SET0(name_unused, namei);
        }
        lib_strcpy(shipnames[i], names[namei], SHIP_NAME_LEN + 1); /* TODO weird that the strcpy is not inside the previous if */
    }
}

static void game_design_look_add(struct game_design_s *gd, int ld)
{
    shipdesign_t *sd = &(gd->sd);
    ship_hull_t hull = sd->hull;
    int numlook = 0, look = sd->look + ld;
    int lookbase = SHIP_LOOK_PER_HULL * hull + gd->lookbase;
    for (int i = 0; i < gd->sd_num; ++i) {
        int l;
        l = gd->tbl_shiplook[i];
        if ((l >= lookbase) && (l < (lookbase + SHIP_LOOK_PER_HULL))) {
            ++numlook;
        }
    }
    if (look < lookbase) {
        look = lookbase + SHIP_LOOK_PER_HULL - 1;
    } else if (look >= (lookbase + SHIP_LOOK_PER_HULL)) {
        look = lookbase;
    }
    if (numlook < SHIP_LOOK_PER_HULL) {
        for (int i = 0; i < gd->sd_num; ++i) {
            if (look == gd->tbl_shiplook[i]) {
                look += ld;
                if (look < lookbase) {
                    look = lookbase + SHIP_LOOK_PER_HULL - 1;
                } else if (look >= (lookbase + SHIP_LOOK_PER_HULL)) {
                    look = lookbase;
                }
                i = -1;
            }
        }
    }
    sd->look = look;
    gd->tbl_shiplook_hull[hull] = look;
}

static int game_design_calc_cost_item_do(struct game_design_s *gd, design_slot_t slot, int i)
{
    const shipdesign_t *sd = &(gd->sd);
    int tm, cost = 0;
    switch (slot) {
        case DESIGN_SLOT_WEAPON1:
        case DESIGN_SLOT_WEAPON2:
        case DESIGN_SLOT_WEAPON3:
        case DESIGN_SLOT_WEAPON4:
            tm = tbl_shiptech_weap[i].is_bio ? gd->percent[TECH_FIELD_PLANETOLOGY] : gd->percent[TECH_FIELD_WEAPON];
            tm -= tbl_shiptech_weap[i].tech_i;
            SETRANGE(tm, 0, 50);
            cost = (tbl_shiptech_weap[i].cost * tech_reduce_50percent_per_10pts[tm]) / 100;
            break;
        case DESIGN_SLOT_ENGINE:
            tm = gd->percent[TECH_FIELD_PROPULSION] - tbl_shiptech_engine[i].tech_i;
            SETRANGE(tm, 0, 50);
            cost = (tbl_shiptech_engine[i].cost * tech_reduce_50percent_per_10pts[tm]) / 100;
            break;
        case DESIGN_SLOT_COMP:
            tm = gd->percent[TECH_FIELD_COMPUTER] - tbl_shiptech_comp[i].tech_i;
            SETRANGE(tm, 0, 50);
            cost = (tbl_shiptech_comp[i].cost[sd->hull] * tech_reduce_50percent_per_10pts[tm]) / 100;
            break;
        case DESIGN_SLOT_JAMMER:
            tm = gd->percent[TECH_FIELD_COMPUTER] - tbl_shiptech_jammer[i].tech_i;
            SETRANGE(tm, 0, 50);
            cost = (tbl_shiptech_jammer[i].cost[sd->hull] * tech_reduce_50percent_per_10pts[tm]) / 100;
            break;
        case DESIGN_SLOT_SHIELD:
            tm = gd->percent[TECH_FIELD_FORCE_FIELD] - tbl_shiptech_shield[i].tech_i;
            SETRANGE(tm, 0, 50);
            cost = (tbl_shiptech_shield[i].cost[sd->hull] * tech_reduce_50percent_per_10pts[tm]) / 100;
            break;
        case DESIGN_SLOT_ARMOR:
            tm = gd->percent[TECH_FIELD_CONSTRUCTION] - tbl_shiptech_armor[i].tech_i;
            SETRANGE(tm, 0, 50);
            cost = (tbl_shiptech_armor[i].cost[sd->hull] * tech_reduce_50percent_per_10pts[tm]) / 100;
            break;
        case DESIGN_SLOT_SPECIAL1:
        case DESIGN_SLOT_SPECIAL2:
        case DESIGN_SLOT_SPECIAL3:
            {
                tech_field_t fi;
                fi = tbl_shiptech_special[i].field;
                tm = gd->percent[fi] - tbl_shiptech_special[i].tech_i;
                SETRANGE(tm, 0, 50);
                cost = (tbl_shiptech_special[i].cost[sd->hull] * tech_reduce_50percent_per_10pts[tm]) / 100;
            }
            break;
        default:
            break;
    }
    return cost;
}

static void game_design_prepare_do(struct game_s *g, struct game_design_s *gd, player_id_t player, shipdesign_t *sd)
{
    const empiretechorbit_t *e = &(g->eto[player]);
    gd->player_i = player;
    gd->sd_num = e->shipdesigns_num;
    game_get_random_shipnames(g, gd->player_i, gd->names);
    if (sd) {
        gd->sd = *sd;
    } else {
        memset(&gd->sd, 0, sizeof(shipdesign_t));
    }
    memcpy(gd->percent, e->tech.percent, sizeof(gd->percent));
}

static weapon_group_t game_design_get_weapon_group(const struct game_s *g, const struct shiptech_weap_s *w)
{
    tech_field_t field;
    tech_group_t group;
    if (w->tech_i > 50) {
        return WEAPON_GROUP_SPECIAL;
    }
    field = w->is_bio ? TECH_FIELD_PLANETOLOGY : TECH_FIELD_WEAPON;
    group = game_tech_get_group(g->gaux, field, w->tech_i);
    if (group == TECH_GROUP_SINGULAR) {
        if (w->is_bio) {
            return WEAPON_GROUP_BIOLOGICAL;
        }
        if (w->is_bomb) {
            return WEAPON_GROUP_BOMBS;
        }
        if (w->halveshield) {
            return WEAPON_GROUP_KINETIC;
        }
        if (w->damagemin != w->damagemax) {
            return WEAPON_GROUP_BEAMS;
        }
        return WEAPON_GROUP_MISSILES;
    }
    switch (group) {
        case TECH_GROUP_BEAMS:
            return WEAPON_GROUP_BEAMS;
        case TECH_GROUP_PENETRATING_BEAMS:
            return WEAPON_GROUP_KINETIC;
        case TECH_GROUP_BIO_WEAPONS:
            return WEAPON_GROUP_BIOLOGICAL;
        case TECH_GROUP_BOMBS:
            return WEAPON_GROUP_BOMBS;
        case TECH_GROUP_ROCKETS:
            return WEAPON_GROUP_MISSILES;
        default:
            break;
    }
    return WEAPON_GROUP_NUM;
}

/* -------------------------------------------------------------------------- */

void game_design_prepare(struct game_s *g, struct game_design_s *gd, player_id_t player, shipdesign_t *sd)
{
    const empiretechorbit_t *e = &(g->eto[player]);
    BOOLVEC_DECLARE(lookused, SHIP_LOOK_PER_BANNER);
    BOOLVEC_CLEAR(lookused, SHIP_LOOK_PER_BANNER);
    game_design_prepare_do(g, gd, player, sd);
    gd->lookbase = e->banner * SHIP_LOOK_PER_BANNER;
    for (int i = 0; i < gd->sd_num; ++i) {
        uint8_t look;
        look = g->srd[player].design[i].look;
        gd->tbl_shiplook[i] = look;
        BOOLVEC_SET1(lookused, look - gd->lookbase);
    }
    /* from ui_design */
    for (ship_hull_t hull = SHIP_HULL_SMALL; hull < SHIP_HULL_NUM; ++hull) {
        uint8_t look, lookbase;
        look = lookbase = SHIP_LOOK_PER_HULL * hull;
        for (int i = 0; i < SHIP_LOOK_PER_HULL; ++i) {
            if (BOOLVEC_IS0(lookused, look + i)) {
                look += i;
                break;
            }
        }
        gd->tbl_shiplook_hull[hull] = look + gd->lookbase;
    }
}

void game_design_prepare_ai(struct game_s *g, struct game_design_s *gd, player_id_t player, ship_hull_t hull, uint8_t look)
{
    shipdesign_t *sd = &(gd->sd);
    game_design_prepare_do(g, gd, player, 0);
    sd->hull = hull;
    sd->look = look;
    lib_strcpy(sd->name, gd->names[hull], SHIP_NAME_LEN);
    game_design_update_engines(sd);
    sd->space = game_design_calc_space(gd);
    sd->cost = game_design_calc_cost(gd);
}

void game_design_update_engines(shipdesign_t *sd)
{
    uint32_t engines, power = 0;
    for (int i = 0; i < WEAPON_SLOT_NUM; ++i) {
        if (sd->wpnt[i]) {
            power += sd->wpnn[i] * tbl_shiptech_weap[sd->wpnt[i]].power;
        }
    }
    power += tbl_shiptech_hull[sd->hull].power * (sd->man + 1);
    for (int i = 0; i < SPECIAL_SLOT_NUM; ++i) {
        power += tbl_shiptech_special[sd->special[i]].power[sd->hull];
    }
    power += tbl_shiptech_comp[sd->comp].power[sd->hull];
    power += tbl_shiptech_jammer[sd->jammer].power[sd->hull];
    power += tbl_shiptech_shield[sd->shield].power[sd->hull];
    engines = power / (tbl_shiptech_engine[sd->engine].power / 10);
    SETMAX(engines, 1);
    sd->engines = engines;
}

int game_design_get_hull_space(const struct game_design_s *gd)
{
    /* MOO1 also has some additional variable here, but it is always 0 */
    int space = tbl_shiptech_hull[gd->sd.hull].space;
    return space + (space * gd->percent[TECH_FIELD_CONSTRUCTION]) / 50;
}

int game_design_calc_space_item(struct game_design_s *gd, design_slot_t slot, int i)
{
    const shipdesign_t *sd = &(gd->sd);
    int tm, space = 0;
    switch (slot) {
        case DESIGN_SLOT_WEAPON1:
        case DESIGN_SLOT_WEAPON2:
        case DESIGN_SLOT_WEAPON3:
        case DESIGN_SLOT_WEAPON4:
            tm = tbl_shiptech_weap[i].is_bio ? gd->percent[TECH_FIELD_PLANETOLOGY] : gd->percent[TECH_FIELD_WEAPON];
            tm -= tbl_shiptech_weap[i].tech_i;
            SETRANGE(tm, 0, 50);
            space = (tbl_shiptech_weap[i].space * tech_reduce_50percent_per_10pts[tm]) / 100;
            break;
        case DESIGN_SLOT_ENGINE:
            tm = gd->percent[TECH_FIELD_PROPULSION] - tbl_shiptech_engine[i].tech_i;
            SETRANGE(tm, 0, 50);
            space = (tbl_shiptech_engine[i].space * tech_reduce_25percent_per_10pts[tm]) / 100;
            break;
        case DESIGN_SLOT_COMP:
            tm = gd->percent[TECH_FIELD_COMPUTER] - tbl_shiptech_comp[i].tech_i;
            SETRANGE(tm, 0, 50);
            space = (tbl_shiptech_comp[i].space[sd->hull] * tech_reduce_25percent_per_10pts[tm]) / 100;
            break;
        case DESIGN_SLOT_JAMMER:
            tm = gd->percent[TECH_FIELD_COMPUTER] - tbl_shiptech_jammer[i].tech_i;
            SETRANGE(tm, 0, 50);
            space = (tbl_shiptech_jammer[i].space[sd->hull] * tech_reduce_25percent_per_10pts[tm]) / 100;
            break;
        case DESIGN_SLOT_SHIELD:
            tm = gd->percent[TECH_FIELD_FORCE_FIELD] - tbl_shiptech_shield[i].tech_i;
            SETRANGE(tm, 0, 50);
            space = (tbl_shiptech_shield[i].space[sd->hull] * tech_reduce_25percent_per_10pts[tm]) / 100;
            break;
        case DESIGN_SLOT_ARMOR:
            tm = gd->percent[TECH_FIELD_CONSTRUCTION] - tbl_shiptech_armor[i].tech_i;
            SETRANGE(tm, 0, 50);
            space = (tbl_shiptech_armor[i].space[sd->hull] * tech_reduce_25percent_per_10pts[tm]) / 100;
            break;
        case DESIGN_SLOT_SPECIAL1:
        case DESIGN_SLOT_SPECIAL2:
        case DESIGN_SLOT_SPECIAL3:
            {
                tech_field_t fi;
                fi = tbl_shiptech_special[i].field;
                tm = gd->percent[fi] - tbl_shiptech_special[i].tech_i;
                SETRANGE(tm, 0, 50);
                space = (tbl_shiptech_special[i].space[sd->hull] * tech_reduce_25percent_per_10pts[tm]) / 100;
            }
            break;
        default:
            break;
    }
    if (i) {
        SETMAX(space, 1);
    }
    return space;
}

int game_design_calc_space(struct game_design_s *gd)
{
    const shipdesign_t *sd = &(gd->sd);
    int s, space;
    space = game_design_get_hull_space(gd);
    for (int i = 0; i < WEAPON_SLOT_NUM; ++i) {
        weapon_t wi;
        wi = sd->wpnt[i];
        if ((wi != WEAPON_NONE) && sd->wpnn[i]) {
            s = game_design_calc_space_item(gd, DESIGN_SLOT_WEAPON1, wi);
            s *= sd->wpnn[i];
            space -= s;
        }
    }
    if (sd->engines) {
        s = game_design_calc_space_item(gd, DESIGN_SLOT_ENGINE, sd->engine);
        s = (s * sd->engines) / 10;
        space -= s;
    }
    if (sd->comp) {
        s = game_design_calc_space_item(gd, DESIGN_SLOT_COMP, sd->comp);
        space -= s;
    }
    if (sd->jammer) {
        s = game_design_calc_space_item(gd, DESIGN_SLOT_JAMMER, sd->jammer);
        space -= s;
    }
    if (sd->shield) {
        s = game_design_calc_space_item(gd, DESIGN_SLOT_SHIELD, sd->shield);
        space -= s;
    }
    if (sd->armor) {
        s = game_design_calc_space_item(gd, DESIGN_SLOT_ARMOR, sd->armor);
        space -= s;
    }
    for (int i = 0; i < SPECIAL_SLOT_NUM; ++i) {
        ship_special_t si = sd->special[i];
        if (si) {
            s = game_design_calc_space_item(gd, DESIGN_SLOT_SPECIAL1, si);
            space -= s;
        }
    }
    return space;
}

int game_design_calc_cost_item(struct game_design_s *gd, design_slot_t slot, int i)
{
    int cost = game_design_calc_cost_item_do(gd, slot, i);
    if ((cost < 5) && (i != 0)) {
        cost = 5;
    }
    cost = (cost + 5) / 10;
    if ((cost < 1) && (i != 0)) {
        cost = 1;
    }
    return cost;
}

int game_design_calc_cost(struct game_design_s *gd)
{
    const shipdesign_t *sd = &(gd->sd);
    uint32_t s, cost = tbl_shiptech_hull[sd->hull].cost;
    for (int i = 0; i < WEAPON_SLOT_NUM; ++i) {
        weapon_t wi;
        wi = sd->wpnt[i];
        if ((wi != WEAPON_NONE) && sd->wpnn[i]) {
            s = game_design_calc_cost_item_do(gd, DESIGN_SLOT_WEAPON1, wi);
            SETMAX(s, 5);
            s *= sd->wpnn[i];
            cost += s;
        }
    }
    if (sd->engines) {
        s = game_design_calc_cost_item_do(gd, DESIGN_SLOT_ENGINE, sd->engine);
        SETMAX(s, 5);
        cost += (s * sd->engines) / 10;
    }
    if (sd->comp) {
        s = game_design_calc_cost_item_do(gd, DESIGN_SLOT_COMP, sd->comp);
        SETMAX(s, 1);
        cost += s;
    }
    if (sd->jammer) {
        s = game_design_calc_cost_item_do(gd, DESIGN_SLOT_JAMMER, sd->jammer);
        SETMAX(s, 1);
        cost += s;
    }
    if (sd->shield) {
        s = game_design_calc_cost_item_do(gd, DESIGN_SLOT_SHIELD, sd->shield);
        SETMAX(s, 5);
        cost += s;
    }
    if (sd->armor) {
        s = game_design_calc_cost_item_do(gd, DESIGN_SLOT_ARMOR, sd->armor);
        SETMAX(s, 5);
        cost += s;
    }
    for (int i = 0; i < SPECIAL_SLOT_NUM; ++i) {
        ship_special_t si = sd->special[i];
        if (si) {
            s = game_design_calc_cost_item_do(gd, DESIGN_SLOT_SPECIAL1, si);
            SETMAX(s, 5);
            cost += s;
        }
    }
    cost = (cost + 5) / 10;
    SETMAX(cost, 1);
    return cost;
}

void game_design_clear(struct game_design_s *gd)
{
    shipdesign_t *sd = &(gd->sd);
    char name[SHIP_NAME_LEN + 1];
    uint8_t look = sd->look;
    ship_hull_t hull = sd->hull;
    lib_strcpy(name, sd->name, sizeof(name));
    memset(sd, 0, sizeof(shipdesign_t));
    sd->look = look;
    sd->hull = hull;
    lib_strcpy(sd->name, name, SHIP_NAME_LEN);
    game_design_update_engines(sd);
    sd->space = game_design_calc_space(gd);
    sd->cost = game_design_calc_cost(gd);
}

void game_design_look_next(struct game_design_s *gd)
{
    game_design_look_add(gd, 1);
}

void game_design_look_prev(struct game_design_s *gd)
{
    game_design_look_add(gd, -1);
}

void game_design_look_fix(const struct game_s *g, player_id_t pi, shipdesign_t *sd)
{
    const empiretechorbit_t *e = &(g->eto[pi]);
    bool need_fix = false;
    int look = sd->look;
    int lookbase = SHIP_LOOK_PER_HULL * sd->hull + e->banner * SHIP_LOOK_PER_BANNER;
    BOOLVEC_DECLARE(lookused, SHIP_LOOK_PER_HULL);
    BOOLVEC_CLEAR(lookused, SHIP_LOOK_PER_HULL);
    for (int i = 0; i < e->shipdesigns_num; ++i) {
        int l;
        l = g->srd[pi].design[i].look;
        if ((l >= lookbase) && (l < (lookbase + SHIP_LOOK_PER_HULL))) {
            BOOLVEC_SET1(lookused, l - lookbase);
            if (l == look) {
                need_fix = true;
            }
        }
    }
    if (need_fix) {
        for (int i = 0; i < SHIP_LOOK_PER_HULL; ++i) {
            if (BOOLVEC_IS0(lookused, i)) {
                look = lookbase + i;
                break;
            }
        }
        sd->look = look;
    }
}

int game_design_build_tbl_fit_comp(struct game_s *g, struct game_design_s *gd, int8_t *buf)
{
    shipdesign_t *sd = &(gd->sd);
    ship_comp_t actcomp = sd->comp;
    int last = 0;
    buf[0] = 1/*HAVE*/;
    for (int i = 1; i < SHIP_COMP_NUM; ++i) {
        if (game_tech_player_has_tech(g, TECH_FIELD_COMPUTER, tbl_shiptech_comp[i].tech_i, gd->player_i)) {
            sd->comp = i;
            game_design_update_engines(sd);
            buf[i] = (game_design_calc_space(gd) >= 0) ? 1/*HAVE*/ : 0/*NOPE*/;
            last = i;
        } else {
            buf[i] = -1/*NO_TECH*/;
        }
    }
    sd->comp = actcomp;
    game_design_update_engines(sd);
    return last;
}

int game_design_build_tbl_fit_shield(struct game_s *g, struct game_design_s *gd, int8_t *buf)
{
    shipdesign_t *sd = &(gd->sd);
    ship_shield_t actshield = sd->shield;
    int last = 0;
    buf[0] = 1/*HAVE*/;
    for (int i = 1; i < SHIP_SHIELD_NUM; ++i) {
        if (game_tech_player_has_tech(g, TECH_FIELD_FORCE_FIELD, tbl_shiptech_shield[i].tech_i, gd->player_i)) {
            sd->shield = i;
            game_design_update_engines(sd);
            buf[i] = (game_design_calc_space(gd) >= 0) ? 1/*HAVE*/ : 0/*NOPE*/;
            last = i;
        } else {
            buf[i] = -1/*NO_TECH*/;
        }
    }
    sd->shield = actshield;
    game_design_update_engines(sd);
    return last;
}

int game_design_build_tbl_fit_jammer(struct game_s *g, struct game_design_s *gd, int8_t *buf)
{
    shipdesign_t *sd = &(gd->sd);
    ship_jammer_t actjammer = sd->jammer;
    int last = 0;
    buf[0] = 1/*HAVE*/;
    for (int i = 1; i < SHIP_JAMMER_NUM; ++i) {
        if (game_tech_player_has_tech(g, TECH_FIELD_COMPUTER, tbl_shiptech_jammer[i].tech_i, gd->player_i)) {
            sd->jammer = i;
            game_design_update_engines(sd);
            buf[i] = (game_design_calc_space(gd) >= 0) ? 1/*HAVE*/ : 0/*NOPE*/;
            last = i;
        } else {
            buf[i] = -1/*NO_TECH*/;
        }
    }
    sd->jammer = actjammer;
    game_design_update_engines(sd);
    return last;
}

int game_design_build_tbl_fit_armor(struct game_s *g, struct game_design_s *gd, int8_t *buf)
{
    shipdesign_t *sd = &(gd->sd);
    ship_armor_t actarmor = sd->armor;
    int last = 0;
    buf[0] = 1/*HAVE*/;
    for (int i = 1; i < SHIP_ARMOR_NUM; ++i) {
        if (game_tech_player_has_tech(g, TECH_FIELD_CONSTRUCTION, tbl_shiptech_armor[i].tech_i, gd->player_i)) {
            sd->armor = i;
            game_design_update_engines(sd);
            buf[i] = (game_design_calc_space(gd) >= 0) ? 1/*HAVE*/ : 0/*NOPE*/;
            last = i;
        } else {
            buf[i] = -1/*NO_TECH*/;
        }
    }
    sd->armor = actarmor;
    game_design_update_engines(sd);
    return last;
}

int game_design_build_tbl_fit_engine(struct game_s *g, struct game_design_s *gd, int8_t *buf)
{
    shipdesign_t *sd = &(gd->sd);
    ship_engine_t actengine = sd->engine;
    int last = 0;
    buf[0] = 1/*HAVE*/;
    for (int i = 0; i < SHIP_ENGINE_NUM; ++i) {
        if (game_tech_player_has_tech(g, TECH_FIELD_PROPULSION, tbl_shiptech_engine[i].tech_i, gd->player_i)) {
            sd->engine = i;
            game_design_update_engines(sd);
            buf[i] = (game_design_calc_space(gd) >= 0) ? 1/*HAVE*/ : 0/*NOPE*/;
            last = i;
        } else {
            buf[i] = -1/*NO_TECH*/;
        }
    }
    sd->engine = actengine;
    game_design_update_engines(sd);
    return last;
}

int game_design_build_tbl_fit_man(struct game_s *g, struct game_design_s *gd, int8_t *buf)
{
    shipdesign_t *sd = &(gd->sd);
    ship_engine_t actman = sd->man;
    for (int i = 0; i <= sd->engine; ++i) {
        sd->man = i;
        game_design_update_engines(sd);
        buf[i] = (game_design_calc_space(gd) >= 0) ? 1/*HAVE*/ : 0/*NOPE*/;
    }
    sd->man = actman;
    game_design_update_engines(sd);
    return sd->engine;
}

int game_design_build_tbl_fit_weapon(struct game_s *g, struct game_design_s *gd, int8_t *buf, int wslot, weapon_group_t wgroup)
{
    shipdesign_t *sd = &(gd->sd);
    weapon_t actwpnt = sd->wpnt[wslot];
    uint8_t actwpnn = sd->wpnn[wslot];
    int last = 0;
    buf[0] = 1/*HAVE*/;
    for (int i = 1; i < WEAPON_NUM; ++i) {
        tech_field_t fi;
        weapon_group_t group;
        bool match;
        fi = tbl_shiptech_weap[i].is_bio ? TECH_FIELD_PLANETOLOGY : TECH_FIELD_WEAPON;
        group = game_design_get_weapon_group(g, &(tbl_shiptech_weap[i]));
        match = (wgroup == WEAPON_GROUP_ALL) || (wgroup == group);
        if (game_tech_player_has_tech(g, fi, tbl_shiptech_weap[i].tech_i, gd->player_i) && match) {
            uint8_t n;
            sd->wpnt[wslot] = i;
            /* TODO use binary search */
            for (n = 1; n < 100; ++n) {
                sd->wpnn[wslot] = n;
                game_design_update_engines(sd);
                if (game_design_calc_space(gd) < 0) {
                    break;
                }
            }
            buf[i] = n - 1;
            last = i;
        } else {
            buf[i] = -1/*NO_TECH*/;
        }
    }
    sd->wpnt[wslot] = actwpnt;
    sd->wpnn[wslot] = actwpnn;
    game_design_update_engines(sd);
    return last;
}

int game_design_build_tbl_fit_special(struct game_s *g, struct game_design_s *gd, int8_t *buf, int sslot)
{
    shipdesign_t *sd = &(gd->sd);
    ship_special_t actspec = sd->special[sslot];
    int last = 0;
    uint8_t othertype[SPECIAL_SLOT_NUM - 1];
    {
        int j, n;
        for (j = 0, n = 0; j < SPECIAL_SLOT_NUM; ++j) {
            if (j != sslot) {
                othertype[n++] = tbl_shiptech_special[sd->special[j]].type;
            }
        }
    }
    buf[0] = 1/*HAVE*/;
    for (int i = 1; i < SHIP_SPECIAL_NUM; ++i) {
        if (game_tech_player_has_tech(g, tbl_shiptech_special[i].field, tbl_shiptech_special[i].tech_i, gd->player_i)) {
            bool flag_check;
            uint8_t thistype;
            flag_check = true;
            thistype = tbl_shiptech_special[i].type;
            for (int j = 0; j < SPECIAL_SLOT_NUM - 1; ++j) {
                if (thistype == othertype[j]) {
                    flag_check = false;
                    buf[i] = 0/*NOPE*/;
                    break;
                }
            }
            if (flag_check) {
                sd->special[sslot] = i;
                game_design_update_engines(sd);
                buf[i] = (game_design_calc_space(gd) >= 0) ? 1/*HAVE*/ : 0/*NOPE*/;
            }
            last = i;
        } else {
            buf[i] = -1/*NO_TECH*/;
        }
    }
    sd->special[sslot] = actspec;
    game_design_update_engines(sd);
    return last;
}

void game_design_compact_slots(shipdesign_t *sd)
{
    for (int loops = 0; loops < SPECIAL_SLOT_NUM; ++loops) {
        for (int i = 0; i < SPECIAL_SLOT_NUM - 1; ++i) {
            if (sd->special[i] == 0) {
                sd->special[i] = sd->special[i + 1];
                sd->special[i + 1] = 0;
            }
        }
    }
    for (int i = 0; i < WEAPON_SLOT_NUM; ++i) {
        if ((sd->wpnt[i] == 0) || (sd->wpnn[i] == 0)) {
            sd->wpnt[i] = 0;
            sd->wpnn[i] = 0;
        }
    }
    for (int loops = 0; loops < WEAPON_SLOT_NUM; ++loops) {
        for (int i = 0; i < WEAPON_SLOT_NUM - 1; ++i) {
            if (sd->wpnt[i] == 0) {
                sd->wpnt[i] = sd->wpnt[i + 1];
                sd->wpnn[i] = sd->wpnn[i + 1];
                sd->wpnt[i + 1] = 0;
                sd->wpnn[i + 1] = 0;
            }
        }
    }
}

void game_design_scrap(struct game_s *g, player_id_t player, int shipi, bool flag_for_new)
{
    empiretechorbit_t *e = &(g->eto[player]);
    shipresearch_t *srd = &(g->srd[player]);
    game_update_maint_costs(g);
    if ((e->shipdesigns_num <= 1) || (shipi >= e->shipdesigns_num)) {
        return;
    }
    for (int i = shipi; i < (NUM_SHIPDESIGNS - 1); ++i) {
        srd->year[i] = srd->year[i + 1];
    }
    for (int i = 0; i < g->galaxy_stars; ++i) {
        fleet_orbit_t *r = &(e->orbit[i]);
        for (int j = shipi; j < (NUM_SHIPDESIGNS - 1); ++j) {
            r->ships[j] = r->ships[j + 1];
        }
        r->ships[NUM_SHIPDESIGNS - 1] = 0;
    }
    for (int i = 0; i < g->enroute_num; ++i) {
        fleet_enroute_t *r = &(g->enroute[i]);
        if (r->owner == player) {
            for (int j = shipi; j < (NUM_SHIPDESIGNS - 1); ++j) {
                r->ships[j] = r->ships[j + 1];
            }
            r->ships[NUM_SHIPDESIGNS - 1] = 0;
        }
    }
    e->reserve_bc += (srd->shipcount[shipi] * srd->design[shipi].cost) / 4;
    for (int i = 0; i < g->galaxy_stars; ++i) {
        planet_t *p = &(g->planet[i]);
        int bs;
        bs = p->buildship;
        if ((p->owner == player) && (bs != BUILDSHIP_STARGATE)) {
            if (bs == shipi) {
                if (!IS_HUMAN(g, player) || (!flag_for_new)) {
                    p->buildship = 0;
                } else {
                    p->buildship = e->shipdesigns_num - 1;
                }
            } else if (bs > shipi) {
                p->buildship = --bs;
            }
        }
    }
    util_table_remove_item_keep_order(shipi, srd->design, sizeof(shipdesign_t), NUM_SHIPDESIGNS);
    game_remove_empty_fleets(g);
    game_update_visibility(g);
    --e->shipdesigns_num;
    game_update_maint_costs(g);
    game_update_have_reserve_fuel(g);
}

bool game_design_add(struct game_s *g, player_id_t player, const shipdesign_t *sd, bool update_reserve_fuel)
{
    empiretechorbit_t *e = &(g->eto[player]);
    int num = e->shipdesigns_num;
    if (num < NUM_SHIPDESIGNS) {
        shipresearch_t *srd = &(g->srd[player]);
        srd->design[num] = *sd;
        srd->year[num] = g->year;
        e->shipdesigns_num = ++num;
        if (update_reserve_fuel) {
            game_update_have_reserve_fuel(g);
        }
        return true;
    } else {
        return false;
    }
}

void game_design_set_hp(shipdesign_t *sd)
{
    sd->hp = (tbl_shiptech_hull[sd->hull].hits * tbl_shiptech_armor[sd->armor].armor) / 100;
}

void game_design_update_haveflags(struct design_data_s *d)
{
    shipdesign_t *sd = &(d->gd->sd);
    SETRANGE(sd->man, 0, sd->engine);
    game_design_update_engines(sd);
    sd->cost = game_design_calc_cost(d->gd);
    sd->space = game_design_calc_space(d->gd);
    game_design_set_hp(sd);
    d->flag_disable_cspeed = true;
    if (sd->man < sd->engine) {
        ++sd->man;
        game_design_update_engines(sd);
        if (game_design_calc_space(d->gd) >= 0) {
            d->flag_disable_cspeed = false;
        }
        --sd->man;
    }
    d->flag_disable_comp = true;
    {
        ship_comp_t actcomp = sd->comp;
        ship_comp_t comp = actcomp;
        while (++comp < SHIP_COMP_NUM) {
            if (game_tech_player_has_tech(d->g, TECH_FIELD_COMPUTER, tbl_shiptech_comp[comp].tech_i, d->gd->player_i)) {
                sd->comp = comp;
                game_design_update_engines(sd);
                if (game_design_calc_space(d->gd) >= 0) {
                    d->flag_disable_comp = false;
                    break;
                }
            }
        }
        sd->comp = actcomp;
    }
    d->flag_disable_jammer = true;
    {
        ship_jammer_t actjammer = sd->jammer;
        ship_jammer_t jammer = actjammer;
        while (++jammer < SHIP_JAMMER_NUM) {
            if (game_tech_player_has_tech(d->g, TECH_FIELD_COMPUTER, tbl_shiptech_jammer[jammer].tech_i, d->gd->player_i)) {
                sd->jammer = jammer;
                game_design_update_engines(sd);
                if (game_design_calc_space(d->gd) >= 0) {
                    d->flag_disable_jammer = false;
                    break;
                }
            }
        }
        sd->jammer = actjammer;
    }
    d->flag_disable_shield = true;
    {
        ship_shield_t actshield = sd->shield;
        ship_shield_t shield = actshield;
        while (++shield < SHIP_SHIELD_NUM) {
            if (game_tech_player_has_tech(d->g, TECH_FIELD_FORCE_FIELD, tbl_shiptech_shield[shield].tech_i, d->gd->player_i)) {
                sd->shield = shield;
                game_design_update_engines(sd);
                if (game_design_calc_space(d->gd) >= 0) {
                    d->flag_disable_shield = false;
                    break;
                }
            }
        }
        sd->shield = actshield;
    }
    d->flag_disable_armor = true;
    {
        ship_armor_t actarmor = sd->armor;
        ship_armor_t armor = actarmor;
        while (++armor < SHIP_ARMOR_NUM) {
            if (game_tech_player_has_tech(d->g, TECH_FIELD_CONSTRUCTION, tbl_shiptech_armor[armor].tech_i, d->gd->player_i)) {
                sd->armor = armor;
                game_design_update_engines(sd);
                if (game_design_calc_space(d->gd) >= 0) {
                    d->flag_disable_armor = false;
                    break;
                }
            }
        }
        sd->armor = actarmor;
    }
    d->flag_disable_engine = true;
    {
        ship_engine_t actengine = sd->engine;
        ship_engine_t engine = actengine;
        while (++engine < SHIP_ENGINE_NUM) {
            if (game_tech_player_has_tech(d->g, TECH_FIELD_PROPULSION, tbl_shiptech_engine[engine].tech_i, d->gd->player_i)) {
                sd->engine = engine;
                game_design_update_engines(sd);
                if (game_design_calc_space(d->gd) >= 0) {
                    d->flag_disable_engine = false;
                    break;
                }
            }
        }
        sd->engine = actengine;
    }
    for (int i = 0; i < WEAPON_SLOT_NUM; ++i) {
        weapon_t wi, actwi;
        int wn;
        wi = actwi = sd->wpnt[i];
        wn = sd->wpnn[i];
        d->flag_tbl_weap_dn[i] = ((wn == 0) || (wi == WEAPON_NONE)) ? 1 : 0;
        d->flag_tbl_weap_up[i] = 1;
        if ((wi != WEAPON_NONE) && (wn < 99)) {
            sd->wpnn[i] = wn + 1;
            game_design_update_engines(sd);
            if (game_design_calc_space(d->gd) >= 0) {
                d->flag_tbl_weap_up[i] = 0;
            }
            sd->wpnn[i] = wn;
        }
        d->flag_tbl_weapon[i] = true;
        while (++wi <= d->last_avail_tech_weap) {
            tech_field_t fi;
            fi = tbl_shiptech_weap[wi].is_bio ? TECH_FIELD_PLANETOLOGY : TECH_FIELD_WEAPON;
            if (game_tech_player_has_tech(d->g, fi, tbl_shiptech_weap[wi].tech_i, d->gd->player_i)) {
                sd->wpnt[i] = wi;
                sd->wpnn[i] = 1;
                game_design_update_engines(sd);
                if (game_design_calc_space(d->gd) >= 0) {
                    d->flag_tbl_weapon[i] = false;
                    break;
                }
            }
        }
        sd->wpnt[i] = actwi;
        sd->wpnn[i] = wn;
    }
    for (int i = 0; i < SPECIAL_SLOT_NUM; ++i) {
        ship_special_t otherspecial[SPECIAL_SLOT_NUM - 1];
        ship_special_t actsi, si;
        si = actsi = sd->special[i];
        {
            int j, n;
            for (j = 0, n = 0; j < SPECIAL_SLOT_NUM; ++j) {
                if (i != j) {
                    otherspecial[n++] = sd->special[j];
                }
            }
        }
        d->flag_tbl_special[i] = true;
        while (++si <= d->last_avail_tech_special) {
            bool flag_check;
            flag_check = true;
            for (int j = 0; j < SPECIAL_SLOT_NUM - 1; ++j) {
                ship_special_t osi;
                osi = otherspecial[j];
                if ((si == osi) || (tbl_shiptech_special[si].type == tbl_shiptech_special[osi].type)) {
                    flag_check = false;
                    break;
                }
            }
            if (flag_check && game_tech_player_has_tech(d->g, tbl_shiptech_special[si].field, tbl_shiptech_special[si].tech_i, d->gd->player_i)) {
                sd->special[i] = si;
                game_design_update_engines(sd);
                if (game_design_calc_space(d->gd) >= 0) {
                    d->flag_tbl_special[i] = false;
                    break;
                }
            }
        }
        sd->special[i] = actsi;
    }
    {
        ship_hull_t acthull;
        acthull = sd->hull;
        for (ship_hull_t i = SHIP_HULL_SMALL; i < SHIP_HULL_NUM; ++i) {
            sd->hull = i;
            game_design_update_engines(sd);
            d->flag_tbl_hull[i] = (game_design_calc_space(d->gd) < 0);
        }
        sd->hull = acthull;
    }
    game_design_update_engines(sd);
}

void game_design_init_maxtech_haveflags(struct design_data_s *d)
{
    {
        weapon_t j = 0;
        for (weapon_t wi = WEAPON_NUCLEAR_BOMB; wi < WEAPON_NUM; ++wi) {
            tech_field_t fi;
            fi = tbl_shiptech_weap[wi].is_bio ? TECH_FIELD_PLANETOLOGY : TECH_FIELD_WEAPON;
            if (game_tech_player_has_tech(d->g, fi, tbl_shiptech_weap[wi].tech_i, d->gd->player_i)) {
                j = wi;
            }
        }
        d->last_avail_tech_weap = j;
    }
    {
        ship_special_t j = 0;
        for (ship_special_t si = SHIP_SPECIAL_RESERVE_FUEL_TANKS; si < SHIP_SPECIAL_NUM; ++si) {
            if (game_tech_player_has_tech(d->g, tbl_shiptech_special[si].field, tbl_shiptech_special[si].tech_i, d->gd->player_i)) {
                j = si;
            }
        }
        d->last_avail_tech_special = j;
    }
    game_design_update_haveflags(d);
}
