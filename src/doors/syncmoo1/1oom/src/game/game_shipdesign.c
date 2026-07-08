#include "config.h"

#include "game_shipdesign.h"
#include "game_shiptech.h"
#include "types.h"

/* -------------------------------------------------------------------------- */

int16_t startship_num = 5;

shipdesign_t tbl_startship[NUM_SHIPDESIGNS] = {
    { /* SCOUT */
      "", 10, 0, SHIP_HULL_SMALL, 0,
      { 0, 0, 0, 0 },
      { 0, 0, 0, 0 },
      0, 10,
      { SHIP_SPECIAL_RESERVE_FUEL_TANKS, 0, 0 },
      0, 0, 0, 0, 0, 3
    },
    { /* FIGHTER */
      "", 15, 0, SHIP_HULL_SMALL, 1,
      { WEAPON_LASER, 0, 0, 0 },
      { 1, 0, 0, 0 },
      0, 30,
      { 0, 0, 0 },
      0, 0, 0, 0, 0, 3
    },
    { /* DESTROYER */
      "", 66, 0, SHIP_HULL_MEDIUM, 6,
      { WEAPON_NUCLEAR_MISSILE_2, WEAPON_LASER, 0, 0 },
      { 1, 3, 0, 0 },
      0, 115,
      { 0, 0, 0 },
      0, 0, 0, 0, 0, 18
    },
    { /* BOMBER */
      "", 86, 0, SHIP_HULL_MEDIUM, 7,
      { WEAPON_NUCLEAR_BOMB, WEAPON_LASER, 0, 0 },
      { 2, 2, 0, 0 },
      0, 90,
      { 0, 0, 0 },
      0, 0, 0, 0, 0, 18
    },
    { /* COLONY SHIP */
      "", 591, 0, SHIP_HULL_LARGE, 12,
      { 0, 0, 0, 0 },
      { 0, 0, 0, 0 },
      0, 205,
      { SHIP_SPECIAL_STANDARD_COLONY_BASE, 0, 0 },
      0, 0, 0, 0, 0, 100
    },
    { /* (unused) */
      "", 10, 0, SHIP_HULL_SMALL, 0,
      { 0, 0, 0, 0 },
      { 0, 0, 0, 0 },
      0, 10,
      { 0, 0, 0 },
      0, 0, 0, 0, 0, 3
    }
};

shipcount_t startfleet_ships[NUM_SHIPDESIGNS] = { 2, 0, 0, 0, 1, 0 };
