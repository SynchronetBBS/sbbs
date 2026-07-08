#ifndef INC_1OOM_GAME_SHIPDESIGN_H
#define INC_1OOM_GAME_SHIPDESIGN_H

#include "game_shiptech.h"
#include "game_types.h"
#include "types.h"

#define SHIP_NAME_LEN   12
#define SHIP_LOOK_PER_HULL  6
#define SHIP_LOOK_PER_BANNER    24
#define WEAPON_SLOT_NUM 4
#define SPECIAL_SLOT_NUM 3

typedef struct shipdesign_s {
    char name[SHIP_NAME_LEN];
    uint16_t cost;
    uint16_t space;
    ship_hull_t hull;
    uint8_t look;
    weapon_t wpnt[WEAPON_SLOT_NUM];   /* weapon type */
    uint8_t wpnn[WEAPON_SLOT_NUM];   /* weapon num */
    ship_engine_t engine;
    uint32_t engines;
    ship_special_t special[SPECIAL_SLOT_NUM];
    ship_shield_t shield;
    ship_jammer_t jammer;
    ship_comp_t comp;
    ship_armor_t armor;
    uint8_t man;    /* maneuverability */
    uint16_t hp;     /* hit points */
} shipdesign_t;

extern int16_t startship_num;
extern shipdesign_t tbl_startship[NUM_SHIPDESIGNS];
extern shipcount_t startfleet_ships[NUM_SHIPDESIGNS];

#endif
