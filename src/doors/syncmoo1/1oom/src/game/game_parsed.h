#ifndef INC_1OOM_GAME_PARSED_H
#define INC_1OOM_GAME_PARSED_H

#include "game_shipdesign.h"
#include "game_shiptech.h"
#include "game_types.h"
#include "types.h"

typedef struct shipparsed_s {
    char name[SHIP_NAME_LEN];
    ship_hull_t hull;
    ship_comp_t comp;
    ship_jammer_t jammer;
    ship_shield_t shield;
    ship_armor_t armor;
    ship_engine_t engine;
    ship_special_t special[SPECIAL_SLOT_NUM];
    weapon_t wpnt[WEAPON_SLOT_NUM];   /* weapon type */
    uint8_t wpnn[WEAPON_SLOT_NUM];   /* weapon num */
    uint16_t hp;     /* hit points */
    uint8_t man;    /* maneuverability */
    uint8_t complevel;
    uint8_t defense;
    uint8_t misdefense;
    uint8_t absorb;
    uint8_t repair;
    uint8_t misshield;
    uint8_t pshield;
    uint8_t extrarange;
    shipcount_t num;
    uint8_t look;
    uint8_t pulsar; /* also antidote for planets */
    uint8_t stream;
    uint16_t sbmask;
} shipparsed_t;

struct game_s;
struct planet_s;

extern void game_parsed_from_design(shipparsed_t *sp, const shipdesign_t *sd, int num);
extern void game_parsed_from_planet(shipparsed_t *sp, const struct game_s *g, const struct planet_s *p);

extern shipparsed_t tbl_monster[MONSTER_NUM][DIFFICULTY_NUM];

#endif
