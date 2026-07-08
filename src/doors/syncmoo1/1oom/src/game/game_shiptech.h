#ifndef INC_1OOM_GAME_SHIPTECH_H
#define INC_1OOM_GAME_SHIPTECH_H

#include "game_types.h"
#include "types.h"

typedef enum {
    WEAPON_NONE = 0,
    WEAPON_NUCLEAR_BOMB, /*1*/
    WEAPON_LASER, /*2*/
    WEAPON_NUCLEAR_MISSILE_2, /*3*/
    WEAPON_NUCLEAR_MISSILE_5, /*4*/
    WEAPON_HEAVY_LASER, /*5*/
    WEAPON_HYPER_V_ROCKET_2, /*6*/
    WEAPON_HYPER_V_ROCKET_5, /*7*/
    WEAPON_GATLING_LASER, /*8*/
    WEAPON_NEUTRON_PELLET_GUN, /*9*/
    WEAPON_HYPER_X_ROCKET_2, /*10*/
    WEAPON_HYPER_X_ROCKET_5, /*11*/
    WEAPON_FUSION_BOMB, /*12*/
    WEAPON_ION_CANNON, /*13*/
    WEAPON_HEAVY_ION_CANNON, /*14*/
    WEAPON_SCATTER_PACK_V_2, /*15*/
    WEAPON_SCATTER_PACK_V_5, /*16*/
    WEAPON_DEATH_SPORES, /*17*/
    WEAPON_MASS_DRIVER, /*18*/
    WEAPON_MERCULITE_MISSILE_2, /*19*/
    WEAPON_MERCULITE_MISSILE_5, /*20*/
    WEAPON_NEUTRON_BLASTER, /*21*/
    WEAPON_HEAVY_BLAST_CANNON, /*22*/
    WEAPON_ANTI_MATTER_BOMB, /*23*/
    WEAPON_GRAVITON_BEAM, /*24*/
    WEAPON_STINGER_MISSILE_2, /*25*/
    WEAPON_STINGER_MISSILE_5, /*26*/
    WEAPON_HARD_BEAM, /*27*/
    WEAPON_FUSION_BEAM, /*28*/
    WEAPON_HEAVY_FUSION_BEAM, /*29*/
    WEAPON_OMEGA_V_BOMB, /*30*/
    WEAPON_ANTI_MATTER_TORP, /*31*/
    WEAPON_MEGABOLT_CANNON, /*32*/
    WEAPON_PHASOR, /*33*/
    WEAPON_HEAVY_PHASOR, /*34*/
    WEAPON_SCATTER_PACK_VII_2, /*35*/
    WEAPON_SCATTER_PACK_VII_5, /*36*/
    WEAPON_DOOM_VIRUS, /*37*/
    WEAPON_AUTO_BLASTER, /*38*/
    WEAPON_PULSON_MISSILE_2, /*39*/
    WEAPON_PULSON_MISSILE_5, /*40*/
    WEAPON_TACHYON_BEAM, /*41*/
    WEAPON_GAUSS_AUTOCANON, /*42*/
    WEAPON_PARTICLE_BEAM, /*43*/
    WEAPON_HERCULAR_MISSILE_2, /*44*/
    WEAPON_HERCULAR_MISSILE_5, /*45*/
    WEAPON_PLASMA_CANNON, /*46*/
    WEAPON_DISRUPTOR, /*47*/
    WEAPON_PULSE_PHASOR, /*48*/
    WEAPON_NEUTRONIUM_BOMB, /*49*/
    WEAPON_BIO_TERMINATOR, /*50*/
    WEAPON_HELLFIRE_TORPEDO, /*51*/
    WEAPON_ZEON_MISSILE, /*52*/
    WEAPON_ZEON_MISSILE_5, /*53*/
    WEAPON_PROTON_TORPEDO, /*54*/
    WEAPON_SCATTER_PACK_X_2, /*55*/
    WEAPON_SCATTER_PACK_X_5, /*56*/
    WEAPON_TRI_FOCUS_PLASMA, /*57*/
    WEAPON_STELLAR_CONVERTER, /*58*/
    WEAPON_MAULER_DEVICE, /*59*/
    WEAPON_PLASMA_TORPEDO, /*60*/
    WEAPON_CRYSTAL_RAY, /*61*/
    WEAPON_DEATH_RAY, /*62*/
    WEAPON_AMEOBA_STREAM, /*63*/
    WEAPON_NUM /*64*/
} weapon_t;

typedef enum {
    SHIP_COMP_NONE = 0,
    SHIP_COMP_MARK_I, /*1*/
    SHIP_COMP_MARK_II, /*2*/
    SHIP_COMP_MARK_III, /*3*/
    SHIP_COMP_MARK_IV, /*4*/
    SHIP_COMP_MARK_V, /*5*/
    SHIP_COMP_MARK_VI, /*6*/
    SHIP_COMP_MARK_VII, /*7*/
    SHIP_COMP_MARK_VIII, /*8*/
    SHIP_COMP_MARK_IX, /*9*/
    SHIP_COMP_MARK_X, /*10*/
    SHIP_COMP_MARK_XI, /*11*/
    SHIP_COMP_NUM /*12*/
} ship_comp_t;

typedef enum {
    SHIP_ENGINE_RETROS = 0,
    SHIP_ENGINE_NUCLEAR, /*1*/
    SHIP_ENGINE_SUB_LIGHT, /*2*/
    SHIP_ENGINE_FUSION, /*3*/
    SHIP_ENGINE_IMPULSE, /*4*/
    SHIP_ENGINE_ION, /*5*/
    SHIP_ENGINE_ANTI_MATTER, /*6*/
    SHIP_ENGINE_INTERPHASED, /*7*/
    SHIP_ENGINE_HYPERTHRUST, /*8*/
    SHIP_ENGINE_NUM /*9*/
} ship_engine_t;

typedef enum {
    SHIP_ARMOR_TITANIUM = 0,
    SHIP_ARMOR_TITANIUM_II, /*1*/
    SHIP_ARMOR_DURALLOY, /*2*/
    SHIP_ARMOR_DURALLOY_II, /*3*/
    SHIP_ARMOR_ZORTRIUM, /*4*/
    SHIP_ARMOR_ZORTRIUM_II, /*5*/
    SHIP_ARMOR_ANDRIUM, /*6*/
    SHIP_ARMOR_ANDRIUM_II, /*7*/
    SHIP_ARMOR_TRITANIUM, /*8*/
    SHIP_ARMOR_TRITANIUM_II, /*9*/
    SHIP_ARMOR_ADAMANTIUM, /*10*/
    SHIP_ARMOR_ADAMANTIUM_II, /*11*/
    SHIP_ARMOR_NEUTRONIUM, /*12*/
    SHIP_ARMOR_NEUTRONIUM_II, /*13*/
    SHIP_ARMOR_NUM /*14*/
} ship_armor_t;

typedef enum {
    SHIP_SHIELD_NONE = 0,
    SHIP_SHIELD_CLASS_I, /*1*/
    SHIP_SHIELD_CLASS_II, /*2*/
    SHIP_SHIELD_CLASS_III, /*3*/
    SHIP_SHIELD_CLASS_IV, /*4*/
    SHIP_SHIELD_CLASS_V, /*5*/
    SHIP_SHIELD_CLASS_VI, /*6*/
    SHIP_SHIELD_CLASS_VII, /*7*/
    SHIP_SHIELD_CLASS_IX, /*8*/
    SHIP_SHIELD_CLASS_XI, /*9*/
    SHIP_SHIELD_CLASS_XIII, /*10*/
    SHIP_SHIELD_CLASS_XV, /*11*/
    SHIP_SHIELD_NUM /*12*/
} ship_shield_t;

typedef enum {
    SHIP_JAMMER_NONE = 0,
    SHIP_JAMMER_JAMMER_I, /*1*/
    SHIP_JAMMER_JAMMER_II, /*2*/
    SHIP_JAMMER_JAMMER_III, /*3*/
    SHIP_JAMMER_JAMMER_IV, /*4*/
    SHIP_JAMMER_JAMMER_V, /*5*/
    SHIP_JAMMER_JAMMER_VI, /*6*/
    SHIP_JAMMER_JAMMER_VII, /*7*/
    SHIP_JAMMER_JAMMER_VIII, /*8*/
    SHIP_JAMMER_JAMMER_IX, /*9*/
    SHIP_JAMMER_JAMMER_X, /*10*/
    SHIP_JAMMER_NUM /*11*/
} ship_jammer_t;

typedef enum {
    SHIP_SPECIAL_NONE = 0,
    SHIP_SPECIAL_RESERVE_FUEL_TANKS, /*1*/
    SHIP_SPECIAL_STANDARD_COLONY_BASE, /*2*/
    SHIP_SPECIAL_BARREN_COLONY_BASE, /*3*/
    SHIP_SPECIAL_TUNDRA_COLONY_BASE, /*4*/
    SHIP_SPECIAL_DEAD_COLONY_BASE, /*5*/
    SHIP_SPECIAL_INFERNO_COLONY_BASE, /*6*/
    SHIP_SPECIAL_TOXIC_COLONY_BASE, /*7*/
    SHIP_SPECIAL_RADIATED_COLONY_BASE, /*8*/
    SHIP_SPECIAL_BATTLE_SCANNER, /*9*/
    SHIP_SPECIAL_ANTI_MISSILE_ROCKETS, /*10*/
    SHIP_SPECIAL_REPULSOR_BEAM, /*11*/
    SHIP_SPECIAL_WARP_DISSIPATOR, /*12*/
    SHIP_SPECIAL_ENERGY_PULSAR, /*13*/
    SHIP_SPECIAL_INERTIAL_STABILIZER, /*14*/
    SHIP_SPECIAL_ZYRO_SHIELD, /*15*/
    SHIP_SPECIAL_AUTOMATED_REPAIR, /*16*/
    SHIP_SPECIAL_STASIS_FIELD, /*17*/
    SHIP_SPECIAL_CLOAKING_DEVICE, /*18*/
    SHIP_SPECIAL_ION_STREAM_PROJECTOR, /*19*/
    SHIP_SPECIAL_HIGH_ENERGY_FOCUS, /*20*/
    SHIP_SPECIAL_IONIC_PULSAR, /*21*/
    SHIP_SPECIAL_BLACK_HOLE_GENERATOR, /*22*/
    SHIP_SPECIAL_SUB_SPACE_TELEPORTER, /*23*/
    SHIP_SPECIAL_LIGHTNING_SHIELD, /*24*/
    SHIP_SPECIAL_NEUTRON_STREAM_PROJECTOR, /*25*/
    SHIP_SPECIAL_ADV_DAMAGE_CONTROL, /*26*/
    SHIP_SPECIAL_TECHNOLOGY_NULLIFIER, /*27*/
    SHIP_SPECIAL_INERTIAL_NULLIFIER, /*28*/
    SHIP_SPECIAL_ORACLE_INTERFACE, /*29*/
    SHIP_SPECIAL_DISPLACMENT_DEVICE, /*30*/
    SHIP_SPECIAL_NUM /*31*/
} ship_special_t;

typedef enum {
    SHIP_HULL_SMALL = 0,
    SHIP_HULL_MEDIUM, /*1*/
    SHIP_HULL_LARGE, /*2*/
    SHIP_HULL_HUGE, /*3*/
    SHIP_HULL_NUM /*4*/
} ship_hull_t;

struct shiptech_weap_s {
    char const * const * const nameptr;
    char const * const * const extratextptr;
    uint16_t damagemin; /* damagemax != damagemin means beam or bomb weapon */
    uint16_t damagemax;
    uint8_t range;
    uint8_t extraacc;
    bool halveshield;
    bool is_bomb;
    bool damagefade;
    uint8_t misstype;
    uint8_t damagemul;
    uint8_t numfire;
    int8_t numshots;
    uint16_t cost;
    uint16_t space;
    uint16_t power;
    bool is_bio;
    uint8_t tech_i;
    uint8_t v24;    /* beam: ? ; missile: fuel */
    uint8_t dtbl[7]; /* beam: color table ; missile: 0=speed */
    uint8_t sound;
    uint8_t nummiss; /* beam: streaming */
};

struct shiptech_comp_s {
    char const * const * const nameptr;
    uint16_t power[SHIP_HULL_NUM];
    uint16_t space[SHIP_HULL_NUM];
    uint16_t cost[SHIP_HULL_NUM];
    uint8_t tech_i;
    uint8_t level;
};

struct shiptech_jammer_s {
    char const * const * const nameptr;
    uint16_t power[SHIP_HULL_NUM];
    uint16_t space[SHIP_HULL_NUM];
    uint16_t cost[SHIP_HULL_NUM];
    uint8_t tech_i;
    uint8_t level;
};

struct shiptech_engine_s {
    char const * const * const nameptr;
    uint16_t power;
    uint16_t space;
    uint16_t cost;
    uint8_t warp;
    uint8_t tech_i;
};

struct shiptech_armor_s {
    char const * const * const nameptr;
    uint16_t cost[SHIP_HULL_NUM];
    uint16_t space[SHIP_HULL_NUM];
    uint16_t armor;
    uint8_t tech_i;
};

struct shiptech_shield_s {
    char const * const * const nameptr;
    uint16_t cost[SHIP_HULL_NUM];
    uint16_t space[SHIP_HULL_NUM];
    uint16_t power[SHIP_HULL_NUM];
    uint8_t absorb;
    uint8_t tech_i;
};

typedef enum {
    SHIP_SPECIAL_BOOL_SCANNER = 0,
    SHIP_SPECIAL_BOOL_REPULSOR, /*1*/
    SHIP_SPECIAL_BOOL_WARPDIS, /*2*/
    SHIP_SPECIAL_BOOL_STASIS, /*3*/
    SHIP_SPECIAL_BOOL_CLOAK, /*4*/
    SHIP_SPECIAL_BOOL_BLACKHOLE, /*5*/
    SHIP_SPECIAL_BOOL_SUBSPACE, /*6*/
    SHIP_SPECIAL_BOOL_TECHNULL, /*7*/
    SHIP_SPECIAL_BOOL_ORACLE, /*8*/
    SHIP_SPECIAL_BOOL_DISP /*9*/
} ship_special_bool_i_t;

struct shiptech_special_s {
    char const * const * const nameptr;
    char const * const * const extratextptr;
    uint16_t cost[SHIP_HULL_NUM];
    uint16_t space[SHIP_HULL_NUM];
    uint16_t power[SHIP_HULL_NUM];
    uint8_t tech_i;
    tech_field_t field;
    uint8_t type;
    uint8_t repair;
    uint8_t extraman;
    uint8_t misshield;
    uint8_t extrarange;
    uint8_t pulsar;
    uint8_t stream;
    uint16_t boolmask;
};

struct shiptech_hull_s {
    char const * const * const nameptr;
    uint16_t cost; /* BC*10*/
    uint16_t space;
    uint16_t hits;
    uint16_t power;
    int16_t defense;
};

extern struct shiptech_weap_s tbl_shiptech_weap[WEAPON_NUM];
extern struct shiptech_comp_s tbl_shiptech_comp[SHIP_COMP_NUM];
extern struct shiptech_engine_s tbl_shiptech_engine[SHIP_ENGINE_NUM];
extern struct shiptech_armor_s tbl_shiptech_armor[SHIP_ARMOR_NUM];
extern struct shiptech_shield_s tbl_shiptech_shield[SHIP_SHIELD_NUM];
extern struct shiptech_jammer_s tbl_shiptech_jammer[SHIP_JAMMER_NUM];
extern struct shiptech_special_s tbl_shiptech_special[SHIP_SPECIAL_NUM];
extern struct shiptech_hull_s tbl_shiptech_hull[SHIP_HULL_NUM];

#endif
