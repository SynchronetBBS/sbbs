#include "config.h"

#include "game_shiptech.h"
#include "game_str.h"
#include "game_techtypes.h"
#include "types.h"

static const char *strspace = " ";
static const char *strempty = "";

struct shiptech_weap_s tbl_shiptech_weap[WEAPON_NUM] = {
    { /* NONE */
        &game_str_st_none, &strspace,
        0, 0, 0,
        0, false, false, false,
        0, 1, 0, -1,
        0, 0, 0,
        0, 0,
        0, { 0, 0, 0, 0, 0, 0, 0 },
        0, 0
    },
    { /* NUCLEAR BOMB */
        &game_str_tbl_st_weap[0], &game_str_tbl_st_weapx[0],
        3, 12, 1,
        0, false, true, false,
        0, 1, 1, 10,
        30, 40, 10,
        false, TECH_WEAP_LASERS,
        0, { 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 },
        0x25, 0
    },
    { /* LASER */
        &game_str_tbl_st_weap[1], &game_str_tbl_st_weapx[1],
        1, 4, 1,
        0, false, false, false,
        0, 1, 1, -1,
        30, 10, 25,
        false, TECH_WEAP_LASERS,
        0, { 0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46 },
        0x7, 0
    },
    { /* NUCLEAR MISSILE */
        &game_str_tbl_st_weap[2], &game_str_tbl_st_weapx[2],
        4, 4, 6,
        0, false, false, false,
        0, 1, 1, 2,
        70, 50, 20,
        false, TECH_WEAP_LASERS,
        2, { 0x60, 0x0, 0x0, 0x6, 0x50, 0x0, 0x0 },
        0x8, 1
    },
    { /* NUCLEAR MISSILE */
        &game_str_tbl_st_weap[3], &game_str_tbl_st_weapx[3],
        4, 4, 4,
        0, false, false, false,
        0, 1, 1, 5,
        105, 75, 30,
        false, TECH_WEAP_LASERS,
        2, { 0x40, 0x0, 0x0, 0x6, 0x50, 0x0, 0x0 },
        0x8, 1
    },
    { /* HEAVY LASER */
        &game_str_tbl_st_weap[4], &game_str_tbl_st_weapx[4],
        1, 7, 2,
        0, false, false, false,
        0, 1, 1, -1,
        90, 30, 75,
        false, TECH_WEAP_LASERS,
        0, { 0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46 },
        0xa, 0
    },
    { /* HYPER-V ROCKET */
        &game_str_tbl_st_weap[5], &game_str_tbl_st_weapx[5],
        6, 6, 7,
        0, false, false, false,
        0, 1, 1, 2,
        90, 70, 20,
        false, TECH_WEAP_HYPER_V_ROCKETS,
        2, { 0x70, 0x0, 0x0, 0x5, 0x64, 0x0, 0x0 },
        0x8, 1
    },
    { /* HYPER-V ROCKET */
        &game_str_tbl_st_weap[6], &game_str_tbl_st_weapx[6],
        6, 6, 5,
        0, false, false, false,
        0, 1, 1, 5,
        135, 105, 30,
        false, TECH_WEAP_HYPER_V_ROCKETS,
        2, { 0x50, 0x0, 0x0, 0x5, 0x64, 0x0, 0x0 },
        0x8, 1
    },
    { /* GATLING LASER */
        &game_str_tbl_st_weap[7], &game_str_tbl_st_weapx[7],
        1, 4, 1,
        0, false, false, false,
        0, 1, 4, -1,
        90, 20, 70,
        false, TECH_WEAP_GATLING_LASER,
        0, { 0x43, 0x41, 0x3f, 0x25, 0x3f, 0x41, 0x43 },
        0x1, 0
    },
    { /* NEUTRON PELLET GUN */
        &game_str_tbl_st_weap[8], &game_str_tbl_st_weapx[8],
        2, 5, 1,
        0, true, false, false,
        0, 1, 1, -1,
        30, 15, 25,
        false, TECH_WEAP_NEUTRON_PELLET_GUN,
        0, { 0x0, 0xae, 0x0, 0x0, 0x0, 0xae, 0x0 },
        0x1, 0
    },
    { /* HYPER-X ROCKET */
        &game_str_tbl_st_weap[9], &game_str_tbl_st_weapx[9],
        8, 8, 7,
        1, false, false, false,
        0, 1, 1, 2,
        120, 100, 20,
        false, TECH_WEAP_HYPER_X_ROCKETS,
        2, { 0x60, 0x0, 0x0, 0x6, 0x50, 0x0, 0x0 },
        0x8, 1
    },
    { /* HYPER-X ROCKET */
        &game_str_tbl_st_weap[10], &game_str_tbl_st_weapx[10],
        8, 8, 5,
        1, false, false, false,
        0, 1, 1, 5,
        180, 150, 30,
        false, TECH_WEAP_HYPER_X_ROCKETS,
        2, { 0x50, 0x0, 0x0, 0x6, 0x50, 0x0, 0x0 },
        0x8, 1
    },
    { /* FUSION BOMB */
        &game_str_tbl_st_weap[11], &game_str_tbl_st_weapx[11],
        5, 20, 1,
        0, false, true, false,
        0, 1, 1, 10,
        30, 50, 10,
        false, TECH_WEAP_FUSION_BOMB,
        0, { 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 },
        0x25, 0
    },
    { /* ION CANNON */
        &game_str_tbl_st_weap[12], &game_str_tbl_st_weapx[12],
        3, 8, 1,
        0, false, false, false,
        0, 1, 1, -1,
        40, 15, 35,
        false, TECH_WEAP_ION_CANNON,
        1, { 0xe4, 0xe5, 0xe6, 0xe7, 0xe6, 0xe5, 0xe4 },
        0x10, 0
    },
    { /* HEAVY ION CANNON */
        &game_str_tbl_st_weap[13], &game_str_tbl_st_weapx[13],
        3, 15, 2,
        0, false, false, false,
        0, 1, 1, -1,
        110, 45, 105,
        false, TECH_WEAP_ION_CANNON,
        1, { 0xe4, 0xe5, 0xe6, 0xe7, 0xe6, 0xe5, 0xe4 },
        0xf, 0
    },
    { /* SCATTER PACK V */
        &game_str_tbl_st_weap[14], &game_str_tbl_st_weapx[14],
        6, 6, 7,
        1, false, false, false,
        0, 1, 1, 2,
        180, 115, 50,
        false, TECH_WEAP_SCATTER_PACK_V_ROCKETS,
        2, { 0x60, 0x0, 0x0, 0x5, 0x64, 0x0, 0x0 },
        0x3, 5
    },
    { /* SCATTER PACK V */
        &game_str_tbl_st_weap[15], &game_str_tbl_st_weapx[15],
        6, 6, 5,
        1, false, false, false,
        0, 1, 1, 5,
        270, 170, 80,
        false, TECH_WEAP_SCATTER_PACK_V_ROCKETS,
        2, { 0x50, 0x0, 0x0, 0x5, 0x64, 0x0, 0x0 },
        0x3, 5
    },
    { /* DEATH SPORES */
        &game_str_tbl_st_weap[16], &game_str_tbl_st_weapx[16],
        1, 1, 1,
        0, false, true, false,
        0, 1, 1, 5,
        100, 100, 10,
        true, TECH_PLAN_DEATH_SPORES,
        0, { 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 },
        0x25, 0
    },
    { /* MASS DRIVER */
        &game_str_tbl_st_weap[17], &game_str_tbl_st_weapx[17],
        5, 8, 1,
        0, true, false, false,
        0, 1, 1, -1,
        90, 55, 50,
        false, TECH_WEAP_MASS_DRIVER,
        0, { 0x0, 0xc, 0x0, 0x0, 0x0, 0xc, 0x0 },
        0x17, 0
    },
    { /* MERCULITE MISSILE */
        &game_str_tbl_st_weap[18], &game_str_tbl_st_weapx[18],
        10, 10, 8,
        2, false, false, false,
        0, 1, 1, 2,
        130, 105, 20,
        false, TECH_WEAP_MERCULITE_MISSILES,
        2, { 0x80, 0x0, 0x0, 0x4, 0x78, 0x0, 0x0 },
        0x8, 1
    },
    { /* MERCULITE MISSILE */
        &game_str_tbl_st_weap[19], &game_str_tbl_st_weapx[19],
        10, 10, 6,
        2, false, false, false,
        0, 1, 1, 5,
        195, 155, 30,
        false, TECH_WEAP_MERCULITE_MISSILES,
        2, { 0x60, 0x0, 0x0, 0x4, 0x78, 0x0, 0x0 },
        0x8, 1
    },
    { /* NEUTRON BLASTER */
        &game_str_tbl_st_weap[20], &game_str_tbl_st_weapx[20],
        3, 12, 1,
        0, false, false, false,
        0, 1, 1, -1,
        60, 20, 60,
        false, TECH_WEAP_NEUTRON_BLASTER,
        0, { 0xcf, 0xce, 0xcd, 0xcc, 0xcb, 0xca, 0xc9 },
        0xa, 0
    },
    { /* HEAVY BLAST CANNON */
        &game_str_tbl_st_weap[21], &game_str_tbl_st_weapx[21],
        3, 24, 2,
        0, false, false, false,
        0, 1, 1, -1,
        180, 60, 180,
        false, TECH_WEAP_NEUTRON_BLASTER,
        0, { 0xcf, 0xce, 0xcd, 0xcc, 0xcb, 0xca, 0xc9 },
        0x12, 0
    },
    { /* ANTI-MATTER BOMB */
        &game_str_tbl_st_weap[22], &game_str_tbl_st_weapx[22],
        10, 40, 1,
        0, false, true, false,
        0, 1, 1, 10,
        50, 75, 10,
        false, TECH_WEAP_ANTI_MATTER_BOMB,
        0, { 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 },
        0x25, 0
    },
    { /* GRAVITON BEAM */
        &game_str_tbl_st_weap[23], &game_str_tbl_st_weapx[23],
        1, 15, 1,
        0, false, false, false,
        0, 1, 1, -1,
        60, 20, 50,
        false, TECH_WEAP_GRAVITON_BEAM,
        2, { 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7 },
        0x0, 1
    },
    { /* STINGER MISSILE */
        &game_str_tbl_st_weap[24], &game_str_tbl_st_weapx[24],
        15, 15, 9,
        3, false, false, false,
        0, 1, 1, 2,
        190, 155, 30,
        false, TECH_WEAP_STINGER_MISSILES,
        2, { 0x90, 0x0, 0x0, 0x5, 0x64, 0x0, 0x0 },
        0x8, 1
    },
    { /* STINGER MISSILE */
        &game_str_tbl_st_weap[25], &game_str_tbl_st_weapx[25],
        15, 15, 7,
        3, false, false, false,
        0, 1, 1, 5,
        270, 230, 45,
        false, TECH_WEAP_STINGER_MISSILES,
        2, { 0x70, 0x0, 0x0, 0x5, 0x64, 0x0, 0x0 },
        0x8, 1
    },
    { /* HARD BEAM */
        &game_str_tbl_st_weap[26], &game_str_tbl_st_weapx[26],
        8, 12, 1,
        0, true, false, false,
        0, 1, 1, -1,
        120, 50, 100,
        false, TECH_WEAP_HARD_BEAM,
        0, { 0xa1, 0x96, 0xa1, 0x96, 0xa1, 0x96, 0xa1 },
        0x1, 0
    },
    { /* FUSION BEAM */
        &game_str_tbl_st_weap[27], &game_str_tbl_st_weapx[27],
        4, 16, 1,
        0, false, false, false,
        0, 1, 1, -1,
        70, 20, 75,
        false, TECH_WEAP_FUSION_BEAM,
        0, { 0xb7, 0xb6, 0xb5, 0xb4, 0xb3, 0xb2, 0xb1 },
        0xb, 0
    },
    { /* HEAVY FUSION BEAM */
        &game_str_tbl_st_weap[28], &game_str_tbl_st_weapx[28],
        4, 30, 2,
        0, false, false, false,
        0, 1, 1, -1,
        210, 60, 225,
        false, TECH_WEAP_FUSION_BEAM,
        0, { 0xb7, 0xb6, 0xb5, 0xb4, 0xb3, 0xb2, 0xb1 },
        0x19, 0
    },
    { /* OMEGA-V BOMB */
        &game_str_tbl_st_weap[29], &game_str_tbl_st_weapx[29],
        20, 50, 1,
        0, false, true, false,
        0, 1, 1, 10,
        80, 140, 10,
        false, TECH_WEAP_OMEGA_V_BOMB,
        0, { 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 },
        0x25, 0
    },
    { /* ANTI-MATTER TORP */
        &game_str_tbl_st_weap[30], &game_str_tbl_st_weapx[30],
        30, 30, 8,
        4, false, false, false,
        1, 1, 1, -1,
        300, 75, 300,
        false, TECH_WEAP_ANTI_MATTER_TORPEDOES,
        2, { 0x80, 0x0, 0x0, 0x3, 0xa0, 0x0, 0x0 },
        0x11, 1
    },
    { /* MEGABOLT CANNON */
        &game_str_tbl_st_weap[31], &game_str_tbl_st_weapx[31],
        2, 20, 1,
        3, false, false, false,
        0, 1, 1, -1,
        80, 30, 65,
        false, TECH_WEAP_MEGABOLT_CANNON,
        3, { 0xaf, 0xd7, 0xae, 0xd7, 0xaf, 0xd7, 0xae },
        0x12, 0
    },
    { /* PHASOR */
        &game_str_tbl_st_weap[32], &game_str_tbl_st_weapx[32],
        5, 20, 1,
        0, false, false, false,
        0, 1, 1, -1,
        90, 20, 90,
        false, TECH_WEAP_PHASOR,
        1, { 0xdb, 0xdc, 0xdd, 0xde, 0xdd, 0xdc, 0xdb },
        0x1c, 0
    },
    { /* HEAVY PHASOR */
        &game_str_tbl_st_weap[33], &game_str_tbl_st_weapx[33],
        5, 40, 2,
        0, false, false, false,
        0, 1, 1, -1,
        260, 60, 270,
        false, TECH_WEAP_PHASOR,
        1, { 0xdb, 0xdc, 0xdd, 0xde, 0xdd, 0xdc, 0xdb },
        0x1a, 0
    },
    { /* SCATTER PACK VII */
        &game_str_tbl_st_weap[34], &game_str_tbl_st_weapx[34],
        10, 10, 8,
        2, false, false, false,
        0, 1, 1, 2,
        280, 170, 50,
        false, TECH_WEAP_SCATTER_PACK_VII_MISSILES,
        2, { 0x80, 0x0, 0x0, 0x4, 0x78, 0x0, 0x0 },
        0x3, 7
    },
    { /* SCATTER PACK VII */
        &game_str_tbl_st_weap[35], &game_str_tbl_st_weapx[35],
        10, 10, 6,
        2, false, false, false,
        0, 1, 1, 5,
        420, 230, 80,
        false, TECH_WEAP_SCATTER_PACK_VII_MISSILES,
        2, { 0x60, 0x0, 0x0, 0x4, 0x78, 0x0, 0x0 },
        0x3, 7
    },
    { /* DOOM VIRUS */
        &game_str_tbl_st_weap[36], &game_str_tbl_st_weapx[36],
        2, 2, 1,
        0, false, true, false,
        0, 1, 1, 5,
        150, 200, 10,
        true, TECH_PLAN_DOOM_VIRUS,
        0, { 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 },
        0x25, 0
    },
    { /* AUTO BLASTER */
        &game_str_tbl_st_weap[37], &game_str_tbl_st_weapx[37],
        4, 16, 1,
        0, false, false, false,
        0, 1, 3, -1,
        140, 30, 90,
        false, TECH_WEAP_AUTO_BLASTER,
        1, { 0xbe, 0xbd, 0xbc, 0xbb, 0xba, 0xb9, 0xb8 },
        0x1, 0
    },
    { /* PULSON MISSILE */
        &game_str_tbl_st_weap[38], &game_str_tbl_st_weapx[38],
        20, 20, 10,
        4, false, false, false,
        0, 1, 1, 2,
        200, 160, 40,
        false, TECH_WEAP_PULSON_MISSILES,
        2, { 0xa0, 0x0, 0x0, 0x4, 0xa0, 0x0, 0x0 },
        0x8, 1
    },
    { /* PULSON MISSILE */
        &game_str_tbl_st_weap[39], &game_str_tbl_st_weapx[39],
        20, 20, 8,
        4, false, false, false,
        0, 1, 1, 5,
        300, 240, 60,
        false, TECH_WEAP_PULSON_MISSILES,
        2, { 0x80, 0x0, 0x0, 0x4, 0xa0, 0x0, 0x0 },
        0x8, 1
    },
    { /* TACHYON BEAM */
        &game_str_tbl_st_weap[40], &game_str_tbl_st_weapx[40],
        1, 25, 1,
        0, false, false, false,
        0, 1, 1, -1,
        90, 30, 70,
        false, TECH_WEAP_TACHYON_BEAM,
        2, { 0x8e, 0x8c, 0x8b, 0x8a, 0x89, 0x88, 0x87 },
        0x1f, 1
    },
    { /* GAUSS AUTOCANON */
        &game_str_tbl_st_weap[41], &game_str_tbl_st_weapx[41],
        7, 10, 1,
        0, true, false, false,
        0, 1, 4, -1,
        280, 105, 105,
        false, TECH_WEAP_GAUSS_AUTOCANNON,
        0, { 0x0, 0xf, 0x0, 0x12, 0x0, 0x15, 0x0 },
        0x1, 0
    },
    { /* PARTICLE BEAM */
        &game_str_tbl_st_weap[42], &game_str_tbl_st_weapx[42],
        10, 20, 1,
        0, true, false, false,
        0, 1, 1, -1,
        150, 90, 75,
        false, TECH_WEAP_PARTICLE_BEAM,
        0, { 0x0, 0xf, 0x0, 0x12, 0x0, 0x15, 0x0 },
        0x21, 0
    },
    { /* HERCULAR MISSILE */
        &game_str_tbl_st_weap[43], &game_str_tbl_st_weapx[43],
        25, 25, 10,
        5, false, false, false,
        0, 1, 1, 2,
        260, 220, 40,
        false, TECH_WEAP_HERCULAR_MISSILES,
        2, { 0xb0, 0x0, 0x0, 0x6, 0x64, 0x0, 0x0 },
        0x8, 1
    },
    { /* HERCULAR MISSILE */
        &game_str_tbl_st_weap[44], &game_str_tbl_st_weapx[44],
        25, 25, 9,
        5, false, false, false,
        0, 1, 1, 5,
        390, 330, 60,
        false, TECH_WEAP_HERCULAR_MISSILES,
        2, { 0x90, 0x0, 0x0, 0x6, 0x64, 0x0, 0x0 },
        0x8, 1
    },
    { /* PLASMA CANNON */
        &game_str_tbl_st_weap[45], &game_str_tbl_st_weapx[45],
        6, 30, 1,
        0, false, false, false,
        0, 1, 1, -1,
        120, 30, 100,
        false, TECH_WEAP_PLASMA_CANNON,
        2, { 0x46, 0x45, 0x44, 0x43, 0x44, 0x45, 0x46 },
        0x1e, 0
    },
    { /* DISRUPTOR */
        &game_str_tbl_st_weap[46], &game_str_tbl_st_weapx[46],
        10, 40, 2,
        0, false, false, false,
        0, 1, 1, -1,
        210, 70, 160,
        false, TECH_WEAP_DISRUPTOR,
        0, { 0xf7, 0xf6, 0xf5, 0xf4, 0xf3, 0xf2, 0xf1 },
        0x1c, 0
    },
    { /* PULSE PHASOR */
        &game_str_tbl_st_weap[47], &game_str_tbl_st_weapx[47],
        5, 20, 1,
        0, false, false, false,
        0, 1, 3, -1,
        250, 40, 120,
        false, TECH_WEAP_PULSE_PHASOR,
        1, { 0xdb, 0xdc, 0xdd, 0xde, 0xdd, 0xdc, 0xdb },
        0x1, 0
    },
    { /* NEUTRONIUM BOMB */
        &game_str_tbl_st_weap[48], &game_str_tbl_st_weapx[48],
        40, 70, 1,
        0, false, true, false,
        0, 1, 1, 10,
        90, 200, 10,
        false, TECH_WEAP_NEUTRONIUM_BOMB,
        0, { 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 },
        0x14, 0
    },
    { /* BIO TERMINATOR */
        &game_str_tbl_st_weap[49], &game_str_tbl_st_weapx[49],
        3, 3, 1,
        0, false, true, false,
        0, 1, 1, 5,
        200, 300, 10,
        true, TECH_PLAN_BIO_TERMINATOR,
        0, { 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 },
        0x14, 0
    },
    { /* HELLFIRE TORPEDO */
        &game_str_tbl_st_weap[50], &game_str_tbl_st_weapx[50],
        25, 25, 10,
        6, false, false, false,
        2, 4, 1, -1,
        500, 150, 350,
        false, TECH_WEAP_HELLFIRE_TORPEDOES,
        2, { 0xa0, 0x0, 0x0, 0x4, 0x8c, 0x0, 0x0 },
        0x11, 1
    },
    { /* ZEON MISSILE */
        &game_str_tbl_st_weap[51], &game_str_tbl_st_weapx[51],
        30, 30, 9,
        6, false, false, false,
        0, 1, 1, 2,
        300, 250, 50,
        false, TECH_WEAP_ZEON_MISSILES,
        2, { 0xc0, 0x0, 0x0, 0x5, 0x64, 0x0, 0x0 },
        0x8, 1
    },
    { /* ZEON MISSILE */
        &game_str_tbl_st_weap[52], &game_str_tbl_st_weapx[52],
        30, 30, 7,
        6, false, false, false,
        0, 1, 1, 5,
        450, 375, 75,
        false, TECH_WEAP_ZEON_MISSILES,
        2, { 0xa0, 0x0, 0x0, 0x5, 0x64, 0x0, 0x0 },
        0x8, 1
    },
    { /* PROTON TORPEDO */
        &game_str_tbl_st_weap[53], &game_str_tbl_st_weapx[53],
        75, 75, 10,
        6, false, false, false,
        3, 1, 1, -1,
        500, 100, 400,
        false, TECH_WEAP_PROTON_TORPEDOES,
        2, { 0xff, 0x0, 0x0, 0x3, 0xc8, 0x0, 0x0 },
        0x11, 1
    },
    { /* SCATTER PACK X */
        &game_str_tbl_st_weap[54], &game_str_tbl_st_weapx[54],
        15, 15, 10,
        3, false, false, false,
        0, 1, 1, 2,
        300, 250, 50,
        false, TECH_WEAP_SCATTER_PACK_X_MISSILES,
        2, { 0x90, 0x0, 0x0, 0x5, 0x64, 0x0, 0x0 },
        0x3, 10
    },
    { /* SCATTER PACK X */
        &game_str_tbl_st_weap[55], &game_str_tbl_st_weapx[55],
        15, 15, 10,
        3, false, false, false,
        0, 1, 1, 5,
        450, 420, 80,
        false, TECH_WEAP_SCATTER_PACK_X_MISSILES,
        2, { 0x70, 0x0, 0x0, 0x5, 0x64, 0x0, 0x0 },
        0x3, 10
    },
    { /* TRI-FOCUS PLASMA */
        &game_str_tbl_st_weap[56], &game_str_tbl_st_weapx[56],
        20, 50, 1,
        0, false, false, false,
        0, 1, 1, -1,
        250, 70, 180,
        false, TECH_WEAP_TRI_FOCUS_PLASMA_CANNON,
        1, { 0x46, 0x45, 0x44, 0x43, 0x44, 0x45, 0x46 },
        0x1b, 0
    },
    { /* STELLAR CONVERTER */
        &game_str_tbl_st_weap[57], &game_str_tbl_st_weapx[57],
        10, 35, 3,
        0, false, false, false,
        0, 4, 1, -1,
        500, 200, 300,
        false, TECH_WEAP_STELLAR_CONVERTER,
        3, { 0x49, 0x56, 0x71, 0x46, 0x49, 0x56, 0x71 },
        0x15, 0
    },
    { /* MAULER DEVICE */
        &game_str_tbl_st_weap[58], &game_str_tbl_st_weapx[58],
        20, 100, 1,
        0, false, false, false,
        0, 1, 1, -1,
        550, 150, 300,
        false, TECH_WEAP_MAULER_DEVICE,
        4, { 0xbb, 0xbc, 0xbd, 0xbd, 0xbd, 0xbc, 0xbb },
        0x23, 0
    },
    { /* PLASMA TORPEDO */
        &game_str_tbl_st_weap[59], &game_str_tbl_st_weapx[59],
        150, 150, 10,
        7, false, false, true,
        4, 1, 1, -1,
        600, 150, 450,
        false, TECH_WEAP_PLASMA_TORPEDOES,
        2, { 0xc0, 0x0, 0x0, 0x3, 0xa0, 0x0, 0x0 },
        0x11, 1
    },
    { /* CRYSTAL RAY */
        &game_str_tbl_st_weap[60], &game_str_tbl_st_weapx[60],
        100, 300, 3,
        0, false, false, false,
        0, 4, 1, -1,
        600, 200, 400,
        false, 101,
        3, { 0xe, 0xc7, 0xe, 0xe, 0xef, 0xe, 0xc7 },
        0x18, 0
    },
    { /* DEATH RAY */
        &game_str_tbl_st_weap[61], &game_str_tbl_st_weapx[61],
        200, 1000, 1,
        0, false, false, false,
        0, 1, 1, -1,
        1000, 2000, 2000,
        false, TECH_WEAP_DEATH_RAY,
        4, { 0xcb, 0xc5, 0xc4, 0x46, 0xc4, 0xc5, 0xcb },
        0x1d, 0
    },
    { /* AMEOBA STREAM */
        &game_str_tbl_st_weap[62], &game_str_tbl_st_weapx[62],
        250, 1000, 3,
        0, false, false, false,
        0, 1, 1, -1,
        600, 200, 400,
        false, 101,
        2, { 0x46, 0x46, 0xdc, 0xdc, 0xdc, 0xd3, 0xd3 },
        0xc, 1
    }
};

struct shiptech_comp_s tbl_shiptech_comp[SHIP_COMP_NUM] = {
    { &game_str_st_none, { 0, 0, 0, 0 }, { 0, 0, 0, 0 }, { 0, 0, 0, 0 }, 0, 0 },
    { &game_str_tbl_st_comp[0], { 5, 10, 20, 100 }, { 5, 10, 20, 100 }, { 40, 200, 1000, 5000 }, 1, 1 },
    { &game_str_tbl_st_comp[1], { 7, 15, 30, 150 }, { 7, 15, 30, 150 }, { 50, 240, 1200, 6000 }, 5, 2 },
    { &game_str_tbl_st_comp[2], { 10, 20, 40, 200 }, { 10, 20, 40, 200 }, { 60, 280, 1400, 7000 }, 10, 3 },
    { &game_str_tbl_st_comp[3], { 12, 25, 50, 250 }, { 12, 25, 50, 250 }, { 70, 320, 1600, 8000 }, 15, 4 },
    { &game_str_tbl_st_comp[4], { 15, 30, 60, 300 }, { 15, 30, 60, 300 }, { 80, 360, 1800, 9000 }, 20, 5 },
    { &game_str_tbl_st_comp[5], { 17, 35, 70, 350 }, { 17, 35, 70, 350 }, { 90, 400, 2000, 10000 }, 25, 6 },
    { &game_str_tbl_st_comp[6], { 20, 40, 80, 400 }, { 20, 40, 80, 400 }, { 100, 440, 2200, 11000 }, 30, 7 },
    { &game_str_tbl_st_comp[7], { 22, 45, 90, 450 }, { 22, 45, 90, 450 }, { 110, 480, 2400, 12000 }, 35, 8 },
    { &game_str_tbl_st_comp[8], { 25, 50, 100, 500 }, { 25, 50, 100, 500 }, { 120, 520, 2600, 13000 }, 40, 9 },
    { &game_str_tbl_st_comp[9], { 27, 55, 110, 550 }, { 27, 55, 110, 550 }, { 130, 560, 2800, 14000 }, 45, 10 },
    { &game_str_tbl_st_comp[10], { 30, 60, 120, 600 }, { 30, 60, 120, 600 }, { 140, 600, 3000, 15000 }, 50, 11 }
};

struct shiptech_engine_s tbl_shiptech_engine[SHIP_ENGINE_NUM] = {
    { &game_str_tbl_st_engine[0], 10, 10, 20, 1, 1 },
    { &game_str_tbl_st_engine[1], 20, 18, 40, 2, 6 },
    { &game_str_tbl_st_engine[2], 30, 26, 60, 3, 12 },
    { &game_str_tbl_st_engine[3], 40, 33, 80, 4, 18 },
    { &game_str_tbl_st_engine[4], 50, 36, 100, 5, 24 },
    { &game_str_tbl_st_engine[5], 60, 40, 120, 6, 30 },
    { &game_str_tbl_st_engine[6], 70, 44, 140, 7, 36 },
    { &game_str_tbl_st_engine[7], 80, 47, 160, 8, 42 },
    { &game_str_tbl_st_engine[8], 90, 50, 180, 9, 48 }
};

struct shiptech_armor_s tbl_shiptech_armor[SHIP_ARMOR_NUM] = {
    { &game_str_tbl_st_armor[0], { 0, 0, 0, 0 }, { 0, 0, 0, 0 }, 100, 1 },
    { &game_str_tbl_st_armor[1], { 20, 100, 500, 2500 }, { 20, 80, 400, 2000 }, 150, 1 },
    { &game_str_tbl_st_armor[2], { 20, 100, 600, 3000 }, { 2, 10, 60, 300 }, 150, 10 },
    { &game_str_tbl_st_armor[3], { 30, 150, 900, 4500 }, { 25, 85, 425, 2100 }, 225, 10 },
    { &game_str_tbl_st_armor[4], { 40, 200, 1000, 5000 }, { 4, 20, 100, 500 }, 200, 17 },
    { &game_str_tbl_st_armor[5], { 60, 300, 1500, 7500 }, { 30, 100, 500, 2500 }, 300, 17 },
    { &game_str_tbl_st_armor[6], { 60, 300, 1500, 7500 }, { 6, 30, 150, 750 }, 250, 26 },
    { &game_str_tbl_st_armor[7], { 90, 450, 2250, 11250 }, { 35, 115, 575, 2875 }, 375, 26 },
    { &game_str_tbl_st_armor[8], { 80, 400, 2000, 10000 }, { 8, 40, 200, 1000 }, 300, 34 },
    { &game_str_tbl_st_armor[9], { 120, 600, 3000, 15000 }, { 40, 130, 650, 3250 }, 450, 34 },
    { &game_str_tbl_st_armor[10], { 100, 500, 2500, 12500 }, { 10, 50, 250, 1250 }, 350, 42 },
    { &game_str_tbl_st_armor[11], { 150, 750, 3750, 18750 }, { 45, 150, 750, 3750 }, 525, 42 },
    { &game_str_tbl_st_armor[12], { 120, 600, 3000, 15000 }, { 12, 60, 300, 1500 }, 400, 50 },
    { &game_str_tbl_st_armor[13], { 180, 900, 4500, 25000 }, { 50, 175, 875, 4375 }, 600, 50 }
};

struct shiptech_shield_s tbl_shiptech_shield[SHIP_SHIELD_NUM] = {
    { &game_str_st_none, { 0, 0, 0, 0 }, { 0, 0, 0, 0 }, { 0, 0, 0, 0 }, 0, 0 },
    { &game_str_tbl_st_shield[0], { 30, 190, 1200, 7500 }, { 5, 20, 60, 250 }, { 5, 20, 60, 250 }, 1, 1 },
    { &game_str_tbl_st_shield[1], { 35, 220, 1400, 8750 }, { 10, 35, 90, 375 }, { 10, 35, 90, 375 }, 2, 4 },
    { &game_str_tbl_st_shield[2], { 40, 250, 1600, 10000 }, { 15, 50, 120, 500 }, { 15, 50, 120, 500 }, 3, 10 },
    { &game_str_tbl_st_shield[3], { 45, 280, 1800, 11250 }, { 20, 65, 150, 675 }, { 20, 65, 150, 675 }, 4, 14 },
    { &game_str_tbl_st_shield[4], { 50, 310, 2000, 12500 }, { 25, 80, 180, 750 }, { 25, 80, 180, 750 }, 5, 20 },
    { &game_str_tbl_st_shield[5], { 55, 340, 2200, 13750 }, { 30, 95, 210, 875 }, { 30, 95, 210, 875 }, 6, 24 },
    { &game_str_tbl_st_shield[6], { 60, 370, 2400, 15000 }, { 35, 110, 240, 1000 }, { 35, 110, 240, 1000 }, 7, 30 },
    { &game_str_tbl_st_shield[7], { 65, 400, 2600, 16250 }, { 40, 125, 270, 1125 }, { 40, 125, 270, 1125 }, 9, 34 },
    { &game_str_tbl_st_shield[8], { 70, 430, 2800, 17500 }, { 45, 140, 300, 1250 }, { 45, 140, 300, 1250 }, 11, 40 },
    { &game_str_tbl_st_shield[9], { 80, 460, 3000, 18750 }, { 50, 155, 330, 1375 }, { 50, 155, 330, 1375 }, 13, 44 },
    { &game_str_tbl_st_shield[10], { 90, 490, 3200, 20000 }, { 55, 160, 360, 1500 }, { 55, 160, 360, 1500 }, 15, 50 }
/*
    { NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }
*/
};

struct shiptech_jammer_s tbl_shiptech_jammer[SHIP_JAMMER_NUM] = {
    { &game_str_st_none, { 0, 0, 0, 0 }, { 0, 0, 0, 0 }, { 0, 0, 0, 0 }, 0, 0 },
    { &game_str_tbl_st_jammer[0], { 10, 20, 40, 170 }, { 10, 20, 40, 170 }, { 25, 150, 1000, 6250 }, 2, 1 },
    { &game_str_tbl_st_jammer[1], { 15, 30, 60, 250 }, { 15, 30, 60, 250 }, { 27, 165, 1100, 6875 }, 7, 2 },
    { &game_str_tbl_st_jammer[2], { 20, 40, 80, 330 }, { 20, 40, 80, 330 }, { 30, 180, 1200, 7500 }, 12, 3 },
    { &game_str_tbl_st_jammer[3], { 25, 50, 100, 410 }, { 25, 50, 100, 410 }, { 32, 195, 1300, 8125 }, 17, 4 },
    { &game_str_tbl_st_jammer[4], { 30, 60, 120, 490 }, { 30, 60, 120, 490 }, { 35, 210, 1400, 8750 }, 22, 5 },
    { &game_str_tbl_st_jammer[5], { 35, 70, 140, 570 }, { 35, 70, 140, 570 }, { 37, 225, 1500, 9375 }, 27, 6 },
    { &game_str_tbl_st_jammer[6], { 40, 80, 160, 650 }, { 40, 80, 160, 650 }, { 40, 240, 1600, 10000 }, 32, 7 },
    { &game_str_tbl_st_jammer[7], { 45, 90, 180, 730 }, { 45, 90, 180, 730 }, { 42, 255, 1700, 10625 }, 37, 8 },
    { &game_str_tbl_st_jammer[8], { 50, 100, 200, 810 }, { 50, 100, 200, 810 }, { 45, 270, 1800, 11250 }, 42, 9 },
    { &game_str_tbl_st_jammer[9], { 55, 110, 220, 900 }, { 55, 110, 220, 900 }, { 50, 285, 1900, 11875 }, 47, 10 }
};

struct shiptech_special_s tbl_shiptech_special[SHIP_SPECIAL_NUM] = {
    { /* NONE */
        &game_str_st_none, &strempty,
        { 0, 0, 0, 0 },
        { 0, 0, 0, 0 },
        { 0, 0, 0, 0 },
        0, 0, 0, 0, 0, 0,
        0
    },
    { /* RESERVE FUEL TANKS */
        &game_str_tbl_st_special[0], &game_str_tbl_st_specialx[0],
        { 20, 100, 500, 2500 },
        { 20, 100, 500, 2500 },
        { 0, 0, 0, 0 },
        TECH_CONS_RESERVE_FUEL_TANKS, TECH_FIELD_CONSTRUCTION, 1,
        0, 0, 0, 0, 0, 0,
        0
    },
    { /* STANDARD COLONY BASE */
        &game_str_tbl_st_special[1], &game_str_tbl_st_specialx[1],
        { 3500, 3500, 3500, 3500 },
        { 700, 700, 700, 700 },
        { 0, 0, 0, 0 },
        TECH_PLAN_ECOLOGICAL_RESTORATION, TECH_FIELD_PLANETOLOGY, 2,
        0, 0, 0, 0, 0, 0,
        0
    },
    { /* BARREN COLONY BASE */
        &game_str_tbl_st_special[2], &game_str_tbl_st_specialx[2],
        { 3750, 3750, 3750, 3750 },
        { 700, 700, 700, 700 },
        { 0, 0, 0, 0 },
        TECH_PLAN_CONTROLLED_BARREN_ENVIRONMENT, TECH_FIELD_PLANETOLOGY, 2,
        0, 0, 0, 0, 0, 0,
        0
    },
    { /* TUNDRA COLONY BASE */
        &game_str_tbl_st_special[3], &game_str_tbl_st_specialx[3],
        { 4000, 4000, 4000, 4000 },
        { 700, 700, 700, 700 },
        { 0, 0, 0, 0 },
        TECH_PLAN_CONTROLLED_TUNDRA_ENVIRONMENT, TECH_FIELD_PLANETOLOGY, 2,
        0, 0, 0, 0, 0, 0,
        0
    },
    { /* DEAD COLONY BASE */
        &game_str_tbl_st_special[4], &game_str_tbl_st_specialx[4],
        { 4250, 4250, 4250, 4250 },
        { 700, 700, 700, 700 },
        { 0, 0, 0, 0 },
        TECH_PLAN_CONTROLLED_DEAD_ENVIRONMENT, TECH_FIELD_PLANETOLOGY, 2,
        0, 0, 0, 0, 0, 0,
        0
    },
    { /* INFERNO COLONY BASE */
        &game_str_tbl_st_special[5], &game_str_tbl_st_specialx[5],
        { 4500, 4500, 4500, 4500 },
        { 700, 700, 700, 700 },
        { 0, 0, 0, 0 },
        TECH_PLAN_CONTROLLED_INFERNO_ENVIRONMENT, TECH_FIELD_PLANETOLOGY, 2,
        0, 0, 0, 0, 0, 0,
        0
    },
    { /* TOXIC COLONY BASE */
        &game_str_tbl_st_special[6], &game_str_tbl_st_specialx[6],
        { 4750, 4750, 4750, 4750 },
        { 700, 700, 700, 700 },
        { 0, 0, 0, 0 },
        TECH_PLAN_CONTROLLED_TOXIC_ENVIRONMENT, TECH_FIELD_PLANETOLOGY, 2,
        0, 0, 0, 0, 0, 0,
        0
    },
    { /* RADIATED COLONY BASE */
        &game_str_tbl_st_special[7], &game_str_tbl_st_specialx[7],
        { 5000, 5000, 5000, 5000 },
        { 700, 700, 700, 700 },
        { 0, 0, 0, 0 },
        TECH_PLAN_CONTROLLED_RADIATED_ENVIRONMENT, TECH_FIELD_PLANETOLOGY, 2,
        0, 0, 0, 0, 0, 0,
        0
    },
    { /* BATTLE SCANNER */
        &game_str_tbl_st_special[8], &game_str_tbl_st_specialx[8],
        { 300, 300, 300, 300 },
        { 50, 50, 50, 50 },
        { 50, 50, 50, 50 },
        TECH_COMP_BATTLE_SCANNER, TECH_FIELD_COMPUTER, 3,
        0, 0, 0, 0, 0, 0,
        (1 << SHIP_SPECIAL_BOOL_SCANNER)
    },
    { /* ANTI-MISSILE ROCKETS */
        &game_str_tbl_st_special[9], &game_str_tbl_st_specialx[9],
        { 100, 100, 100, 100 },
        { 2, 10, 50, 250 },
        { 8, 40, 200, 1000 },
        TECH_WEAP_ANTI_MISSILE_ROCKETS, TECH_FIELD_WEAPON, 4,
        0, 0, 40, 0, 0, 0,
        0
    },
    { /* REPULSOR BEAM */
        &game_str_tbl_st_special[10], &game_str_tbl_st_specialx[10],
        { 550, 550, 550, 550 },
        { 100, 100, 100, 100 },
        { 200, 200, 200, 200 },
        TECH_FFLD_REPULSOR_BEAM, TECH_FIELD_FORCE_FIELD, 5,
        0, 0, 0, 0, 0, 0,
        (1 << SHIP_SPECIAL_BOOL_REPULSOR)
    },
    { /* WARP DISSIPATOR */
        &game_str_tbl_st_special[11], &game_str_tbl_st_specialx[11],
        { 650, 650, 650, 650 },
        { 100, 100, 100, 100 },
        { 300, 300, 300, 300 },
        TECH_PROP_WARP_DISSIPATOR, TECH_FIELD_PROPULSION, 6,
        0, 0, 0, 0, 0, 0,
        (1 << SHIP_SPECIAL_BOOL_WARPDIS)
    },
    { /* ENERGY PULSAR */
        &game_str_tbl_st_special[12], &game_str_tbl_st_specialx[12],
        { 750, 750, 750, 750 },
        { 150, 150, 150, 150 },
        { 250, 250, 250, 250 },
        TECH_PROP_ENERGY_PULSAR, TECH_FIELD_PROPULSION, 7,
        0, 0, 0, 0, 1, 0,
        0
    },
    { /* INERTIAL STABILIZER */
        &game_str_tbl_st_special[13], &game_str_tbl_st_specialx[13],
        { 20, 75, 500, 2700 },
        { 4, 20, 100, 500 },
        { 8, 40, 200, 1000 },
        TECH_PROP_INERTIAL_STABILIZER, TECH_FIELD_PROPULSION, 8,
        0, 2, 0, 0, 0, 0,
        0
    },
    { /* ZYRO SHIELD */
        &game_str_tbl_st_special[14], &game_str_tbl_st_specialx[14],
        { 50, 100, 200, 300 },
        { 4, 20, 100, 500 },
        { 12, 60, 300, 1500 },
        TECH_FFLD_ZYRO_SHIELD, TECH_FIELD_FORCE_FIELD, 4,
        0, 0, 75, 0, 0, 0,
        0
    },
    { /* AUTOMATED REPAIR */
        &game_str_tbl_st_special[15], &game_str_tbl_st_specialx[15],
        { 2, 8, 50, 300 },
        { 3, 15, 100, 600 },
        { 3, 10, 50, 300 },
        TECH_CONS_AUTOMATED_REPAIR_SYSTEM, TECH_FIELD_CONSTRUCTION, 9,
        15, 0, 0, 0, 0, 0,
        0
    },
    { /* STASIS FIELD */
        &game_str_tbl_st_special[16], &game_str_tbl_st_specialx[16],
        { 2500, 2500, 2500, 2500 },
        { 200, 200, 200, 200 },
        { 275, 275, 275, 275 },
        TECH_FFLD_STASIS_FIELD, TECH_FIELD_FORCE_FIELD, 10,
        0, 0, 0, 0, 0, 0,
        (1 << SHIP_SPECIAL_BOOL_STASIS)
    },
    { /* CLOAKING DEVICE */
        &game_str_tbl_st_special[17], &game_str_tbl_st_specialx[17],
        { 30, 150, 750, 3750 },
        { 5, 25, 120, 600 },
        { 10, 50, 250, 1250 },
        TECH_FFLD_CLOAKING_DEVICE, TECH_FIELD_FORCE_FIELD, 11,
        0, 0, 0, 0, 0, 0,
        (1 << SHIP_SPECIAL_BOOL_CLOAK)
    },
    { /* ION STREAM PROJECTOR */
        &game_str_tbl_st_special[18], &game_str_tbl_st_specialx[18],
        { 1000, 1000, 1000, 1000 },
        { 250, 250, 250, 250 },
        { 500, 500, 500, 500 },
        TECH_WEAP_ION_STREAM_PROJECTOR, TECH_FIELD_WEAPON, 12,
        0, 0, 0, 0, 0, 1,
        0
    },
    { /* HIGH ENERGY FOCUS */
        &game_str_tbl_st_special[19], &game_str_tbl_st_specialx[19],
        { 30, 135, 625, 3500 },
        { 35, 100, 150, 500 },
        { 65, 200, 350, 1000 },
        TECH_PROP_HIGH_ENERGY_FOCUS, TECH_FIELD_PROPULSION, 13,
        0, 0, 0, 3, 0, 0,
        0
    },
    { /* IONIC PULSAR */
        &game_str_tbl_st_special[20], &game_str_tbl_st_specialx[20],
        { 1500, 1500, 1500, 1500 },
        { 400, 400, 400, 400 },
        { 750, 750, 750, 750 },
        TECH_PROP_IONIC_PULSAR, TECH_FIELD_PROPULSION, 7,
        0, 0, 0, 0, 2, 0,
        0
    },
    { /* BLACK HOLE GENERATOR */
        &game_str_tbl_st_special[21], &game_str_tbl_st_specialx[21],
        { 2750, 2750, 2750, 2750 },
        { 750, 750, 750, 750 },
        { 750, 750, 750, 750 },
        TECH_FFLD_BLACK_HOLE_GENERATOR, TECH_FIELD_FORCE_FIELD, 14,
        0, 0, 0, 0, 0, 0,
        (1 << SHIP_SPECIAL_BOOL_BLACKHOLE)
    },
    { /* SUB SPACE TELEPORTER */
        &game_str_tbl_st_special[22], &game_str_tbl_st_specialx[22],
        { 25, 100, 450, 2250 },
        { 4, 20, 100, 500 },
        { 16, 80, 400, 2000 },
        TECH_PROP_SUB_SPACE_TELEPORTER, TECH_FIELD_PROPULSION, 15,
        0, 0, 0, 0, 0, 0,
        (1 << SHIP_SPECIAL_BOOL_SUBSPACE)
    },
    { /* LIGHTNING SHIELD */
        &game_str_tbl_st_special[23], &game_str_tbl_st_specialx[23],
        { 200, 300, 400, 500 },
        { 6, 30, 150, 750 },
        { 15, 70, 350, 1750 },
        TECH_FFLD_LIGHTNING_SHIELD, TECH_FIELD_FORCE_FIELD, 4,
        0, 0, 100, 0, 0, 0,
        0
    },
    { /* NEUTRON STREAM PROJECTOR */
        &game_str_tbl_st_special[24], &game_str_tbl_st_specialx[24],
        { 2000, 2000, 2000, 2000 },
        { 500, 500, 500, 500 },
        { 1250, 1250, 1250, 1250 },
        TECH_WEAP_NEUTRON_STREAM_PROJECTOR, TECH_FIELD_WEAPON, 16,
        0, 0, 0, 0, 0, 2,
        0
    },
    { /* ADV DAMAGE CONTROL */
        &game_str_tbl_st_special[25], &game_str_tbl_st_specialx[25],
        { 40, 200, 1000, 5000 },
        { 9, 45, 300, 1800 },
        { 9, 30, 150, 450 },
        TECH_CONS_ADVANCED_DAMAGE_CONTROL, TECH_FIELD_CONSTRUCTION, 9,
        30, 0, 0, 0, 0, 0,
        0
    },
    { /* TECHNOLOGY NULLIFIER */
        &game_str_tbl_st_special[26], &game_str_tbl_st_specialx[26],
        { 3000, 3000, 3000, 3000 },
        { 750, 750, 750, 750 },
        { 1000, 1000, 1000, 1000 },
        TECH_COMP_TECHNOLOGY_NULLIFIER, TECH_FIELD_COMPUTER, 17,
        0, 0, 0, 0, 0, 0,
        (1 << SHIP_SPECIAL_BOOL_TECHNULL)
    },
    { /* INERTIAL NULLIFIER */
        &game_str_tbl_st_special[27], &game_str_tbl_st_specialx[27],
        { 60, 200, 1500, 5000 },
        { 6, 30, 150, 750 },
        { 12, 60, 300, 1500 },
        TECH_PROP_INERTIAL_NULLIFIER, TECH_FIELD_PROPULSION, 8,
        0, 4, 0, 0, 0, 0,
        0
    },
    { /* ORACLE INTERFACE */
        &game_str_tbl_st_special[28], &game_str_tbl_st_specialx[28],
        { 30, 150, 600, 2750 },
        { 8, 40, 200, 1000 },
        { 12, 60, 300, 1500 },
        TECH_COMP_ORACLE_INTERFACE, TECH_FIELD_COMPUTER, 20,
        0, 0, 0, 0, 0, 0,
        (1 << SHIP_SPECIAL_BOOL_ORACLE)
    },
    { /* DISPLACMENT DEVICE */
        &game_str_tbl_st_special[29], &game_str_tbl_st_specialx[29],
        { 30, 150, 300, 2750 },
        { 10, 50, 225, 1250 },
        { 10, 50, 225, 1250 },
        TECH_PROP_DISPLACEMENT_DEVICE, TECH_FIELD_PROPULSION, 21,
        0, 0, 0, 0, 0, 0,
        (1 << SHIP_SPECIAL_BOOL_DISP)
    }
};

struct shiptech_hull_s tbl_shiptech_hull[SHIP_HULL_NUM] = {
    { &game_str_tbl_st_hull[0], 60, 40, 3, 2, 2 },
    { &game_str_tbl_st_hull[1], 360, 200, 18, 15, 1 },
    { &game_str_tbl_st_hull[2], 2000, 1000, 100, 100, 0 },
    { &game_str_tbl_st_hull[3], 12000, 5000, 600, 700, -1 }
};
