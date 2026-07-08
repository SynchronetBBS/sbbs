#ifndef INC_1OOM_GAME_PLANET_H
#define INC_1OOM_GAME_PLANET_H

#include "boolvec.h"
#include "game_types.h"
#include "types.h"

typedef enum planet_type_e {
    PLANET_TYPE_NOT_HABITABLE = 0,
    PLANET_TYPE_RADIATED, /*1*/
    PLANET_TYPE_TOXIC, /*2*/
    PLANET_TYPE_INFERNO, /*3*/
    PLANET_TYPE_DEAD, /*4*/
    PLANET_TYPE_TUNDRA, /*5*/
    PLANET_TYPE_BARREN, /*6*/
    PLANET_TYPE_MINIMAL, /*7*/
    PLANET_TYPE_DESERT, /*8*/
    PLANET_TYPE_STEPPE, /*9*/
    PLANET_TYPE_ARID, /*a*/
    PLANET_TYPE_OCEAN, /*b*/
    PLANET_TYPE_JUNGLE, /*c*/
    PLANET_TYPE_TERRAN, /*d*/
    PLANET_TYPE_NUM
} planet_type_t;

typedef enum planet_growth_e {
    PLANET_GROWTH_HOSTILE = 0,
    PLANET_GROWTH_NORMAL, /*1*/
    PLANET_GROWTH_FERTILE, /*2*/
    PLANET_GROWTH_GAIA, /*3*/
    PLANET_GROWTH_NUM
} planet_growth_t;

typedef enum planet_special_e {
    PLANET_SPECIAL_ULTRA_POOR = 0,
    PLANET_SPECIAL_POOR, /*1*/
    PLANET_SPECIAL_NORMAL, /*2*/
    PLANET_SPECIAL_ARTIFACTS, /*3*/
    PLANET_SPECIAL_RICH, /*4*/
    PLANET_SPECIAL_ULTRA_RICH, /*5*/
    PLANET_SPECIAL_4XTECH, /*6*/
    PLANET_SPECIAL_NUM
} planet_special_t;

typedef enum {
    PLANET_SLIDER_SHIP = 0,
    PLANET_SLIDER_DEF, /*1*/
    PLANET_SLIDER_IND, /*2*/
    PLANET_SLIDER_ECO, /*3*/
    PLANET_SLIDER_TECH, /*4*/
    PLANET_SLIDER_NUM
} planet_slider_i_t;

typedef enum {
    PLANET_UNREST_NORMAL = 0,
    PLANET_UNREST_UNREST, /*1*/
    PLANET_UNREST_HMM2, /*2*/
    PLANET_UNREST_REBELLION, /*3*/
    PLANET_UNREST_RESOLVED /*4*/
} planet_unrest_t;

typedef enum {
    STAR_TYPE_YELLOW = 0,
    STAR_TYPE_RED,  /*1*/
    STAR_TYPE_GREEN,    /*2*/
    STAR_TYPE_WHITE,    /*3*/
    STAR_TYPE_BLUE, /*4*/
    STAR_TYPE_NEUTRON,  /*5*/
    STAR_TYPE_NUM   /*6*/
} star_type_t;

typedef enum {
    PLANET_ROCKS_NONE = 0,
    PLANET_ROCKS_SOME, /*1*/
    PLANET_ROCKS_MANY /*2*/
} planet_rocks_t;

typedef enum {
    FINISHED_FACT, /*0*/
    FINISHED_POPMAX, /*1*/
    FINISHED_SOILATMOS, /*2*/
    FINISHED_STARGATE, /*3*/
    FINISHED_SHIELD, /*4*/
    FINISHED_SHIP, /*5*/
    FINISHED_NUM
} planet_finished_t;

#define PLANET_NAME_LEN 12

typedef struct planet_s {
    char name[PLANET_NAME_LEN];
    uint16_t x;
    uint16_t y;
    star_type_t star_type;
    uint8_t look;   /* 0, 6 */
    uint8_t frame;  /* 0..49 */
    planet_rocks_t rocks;
    int16_t max_pop1;  /* Base size */
    int16_t max_pop2;  /* Size adjusted by soil enrichment tech */
    int16_t max_pop3;  /* Maximum population */
    planet_type_t type;
    uint8_t battlebg;   /* 0..4 ; 0 implies planet is in nebula */
    uint8_t infogfx;    /* index to planets.lbx */
    planet_growth_t growth;
    planet_special_t special;
    int16_t bc_to_ecoproj;
    uint16_t bc_to_ship;
    uint8_t bc_to_factory;
    uint32_t reserve;
    int16_t waste;
    player_id_t owner;
    player_id_t prev_owner;
    player_id_t claim;
    int16_t pop;
    int16_t pop_prev;
    int16_t factories;
    uint16_t prod_after_maint;
    uint16_t total_prod;
    int16_t slider[PLANET_SLIDER_NUM];          /* FIXME? could be int8_t but uiobj uses uint16_t */
    uint16_t slider_lock[PLANET_SLIDER_NUM];    /* FIXME should be boolvec but uiobj uses uint16_t */
    uint8_t buildship; /* 0..NUM_SHIPDESIGNS-1 or BUILDSHIP_STARGATE */
    uint8_t reloc; /* planet i to relocate produced ships (== planet's own index if no relocation) */
    uint16_t missile_bases;
    uint16_t bc_to_base;
    uint16_t bc_upgrade_base;
    bool have_stargate;
    uint8_t shield; /* 0, 10, 15, 20 */
    uint16_t bc_to_shield;
    uint16_t trans_num;
    uint8_t trans_dest;
    int8_t pop_tenths;
    BOOLVEC_DECLARE(explored, PLAYER_NUM);
    BOOLVEC_DECLARE(within_srange, PLAYER_NUM); /* scanner range covers planet */
    uint8_t within_frange[PLAYER_NUM]; /* fuel range reaches planet: 0=no, 1=yes, 2=with reserve fuel */
    uint8_t pop_oper_fact;
    uint16_t bc_to_refit;
    int16_t rebels;
    planet_unrest_t unrest;
    bool unrest_reported;
    BOOLVEC_DECLARE(unrefuel, PLAYER_NUM);
    BOOLVEC_DECLARE(finished, FINISHED_NUM);
    /* remaining variables used only during game_turn_process */
    uint16_t inbound[PLAYER_NUM];
    uint16_t total_inbound[PLAYER_NUM];
    player_id_t artifact_looter;
} planet_t;

#define PLANET_NONE 255

struct game_s;

extern void game_planet_destroy(struct game_s *g, uint8_t planet_i, player_id_t attacker);
extern uint8_t game_planet_get_random(struct game_s *g, player_id_t owner);
extern void game_planet_adjust_percent2(struct game_s *g, uint8_t planet_i, planet_slider_i_t si, uint8_t percent, bool respect_locks);
extern void game_planet_adjust_percent(struct game_s *g, player_id_t owner, planet_slider_i_t si, uint8_t percent, int growth);
extern int game_planet_get_waste_percent(int *r_waste, const struct game_s *g, const planet_t *p, bool subtract_transports);
extern bool game_planet_can_terraform(const struct game_s *g, const planet_t *p, player_id_t active_player, bool soilatmos);
extern void game_planet_update_home(struct game_s *g);
extern const char *game_planet_get_finished_text(const struct game_s *g, const planet_t *p, planet_finished_t type, char *buf, size_t bufsize);
extern int game_planet_get_slider_text(const struct game_s *g, const planet_t *p, planet_slider_i_t si, char *buf, size_t bufsize);
extern int game_planet_get_slider_text_eco(const struct game_s *g, const planet_t *p, bool flag_tenths, char *buf, size_t bufsize);

#endif
