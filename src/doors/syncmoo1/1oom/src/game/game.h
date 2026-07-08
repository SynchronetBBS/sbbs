#ifndef INC_1OOM_GAME_H
#define INC_1OOM_GAME_H

#include "boolvec.h"
#include "game_planet.h"
#include "game_shipdesign.h"
#include "game_shiptech.h"
#include "game_types.h"
#include "types.h"

typedef struct fleet_enroute_s {
    player_id_t owner;
    uint16_t x;
    uint16_t y;
    uint8_t dest;   /* planet index */
    uint8_t speed;
    bool retreat;
    BOOLVEC_DECLARE(visible, PLAYER_NUM);
    shipcount_t ships[NUM_SHIPDESIGNS];
} fleet_enroute_t;

typedef struct transport_s {
    player_id_t owner;
    uint16_t x;
    uint16_t y;
    uint8_t dest;   /* planet index */
    uint8_t speed;
    BOOLVEC_DECLARE(visible, PLAYER_NUM);
    uint16_t pop;
} transport_t;

typedef struct fleet_orbit_s {
    BOOLVEC_DECLARE(visible, PLAYER_NUM);
    shipcount_t ships[NUM_SHIPDESIGNS];
} fleet_orbit_t;

typedef struct techdata_s {
    uint8_t percent[TECH_FIELD_NUM];   /* tech level % */
    int16_t slider[TECH_FIELD_NUM]; /* % */
    uint16_t slider_lock[TECH_FIELD_NUM]; /* FIXME should be boolvec but uiobj uses uint16_t */
    uint32_t investment[TECH_FIELD_NUM];
    uint8_t project[TECH_FIELD_NUM];
    uint32_t cost[TECH_FIELD_NUM];
    uint16_t completed[TECH_FIELD_NUM]; /* number of completed projects (len of srd[i].researchcompleted) */
} techdata_t;

#define TECH_TIER_NUM   10
#define TECH_PER_FIELD  60
#define TECH_MAX_LEVEL  100

typedef struct shipresearch_s {
    shipdesign_t design[NUM_SHIPDESIGNS];
    uint8_t researchlist[TECH_FIELD_NUM][TECH_TIER_NUM][3];
    uint8_t researchcompleted[TECH_FIELD_NUM][TECH_PER_FIELD];
    bool have_reserve_fuel[NUM_SHIPDESIGNS];
    uint16_t year[NUM_SHIPDESIGNS];
    shipsum_t shipcount[NUM_SHIPDESIGNS];
} shipresearch_t;

typedef struct empiretechorbit_s {
    race_t race;
    banner_t banner;
    trait1_t trait1;
    trait2_t trait2;
    int8_t ai_p3_countdown;
    int8_t ai_p2_countdown;
    BOOLVEC_DECLARE(contact, PLAYER_NUM);
    BOOLVEC_DECLARE(contact_broken, PLAYER_NUM);
    int16_t relation1[PLAYER_NUM];
    int16_t relation2[PLAYER_NUM];
    uint8_t diplo_type[PLAYER_NUM];
    int16_t diplo_val[PLAYER_NUM];
    uint16_t diplo_p1[PLAYER_NUM];
    int16_t diplo_p2[PLAYER_NUM];
    int16_t trust[PLAYER_NUM];
    treaty_t broken_treaty[PLAYER_NUM];
    int16_t blunder[PLAYER_NUM];
    tech_field_t tribute_field[PLAYER_NUM];
    uint8_t tribute_tech[PLAYER_NUM];
    int16_t mood_treaty[PLAYER_NUM];
    int16_t mood_trade[PLAYER_NUM];
    int16_t mood_tech[PLAYER_NUM];
    int16_t mood_peace[PLAYER_NUM];
    treaty_t treaty[PLAYER_NUM];
    uint16_t trade_bc[PLAYER_NUM];
    int16_t trade_percent[PLAYER_NUM];
    spymode_t spymode_next[PLAYER_NUM];
    uint16_t au_want_trade[PLAYER_NUM];
    tech_field_t au_want_field[PLAYER_NUM];
    uint8_t au_want_tech[PLAYER_NUM];
    uint8_t au_tech_trade_num[PLAYER_NUM];
    tech_field_t au_tech_trade_field[PLAYER_NUM][TECH_SPY_MAX];
    uint8_t au_tech_trade_tech[PLAYER_NUM][TECH_SPY_MAX];
    tech_field_t offer_field[PLAYER_NUM];
    uint8_t offer_tech[PLAYER_NUM]; /* tech_i */
    uint16_t offer_bc[PLAYER_NUM];
    player_id_t au_ally_attacker[PLAYER_NUM];
    player_id_t au_ask_break_treaty[PLAYER_NUM];
    player_id_t attack_bounty[PLAYER_NUM];
    player_id_t bounty_collect[PLAYER_NUM];
    tech_field_t attack_gift_field[PLAYER_NUM];
    uint8_t attack_gift_tech[PLAYER_NUM];
    int16_t attack_gift_bc[PLAYER_NUM];
    int16_t hatred[PLAYER_NUM];
    uint16_t have_met[PLAYER_NUM]; /* 0, 1, 2 */
    uint16_t trade_established_bc[PLAYER_NUM];
    uint8_t have_planet_shield; /* 0, 5, 10, 15, 20 */
    uint16_t planet_shield_cost;
    int16_t spying[PLAYER_NUM]; /* tenths */
    uint16_t spyfund[PLAYER_NUM];
    spymode_t spymode[PLAYER_NUM];
    int16_t security; /* tenths */
    uint16_t spies[PLAYER_NUM];
    int32_t total_trade_bc;
    uint32_t ship_maint_bc;
    uint32_t bases_maint_bc;
    uint16_t spying_maint_bc;
    uint16_t percent_prod_total_to_actual;
    int32_t total_maint_bc;
    uint32_t total_research_bc;
    uint32_t total_production_bc;
    uint32_t reserve_bc;
    int16_t tax;
    uint8_t base_shield;
    uint8_t base_comp;
    uint8_t base_weapon;
    bool have_sub_space_int;
    uint8_t antidote;
    planet_type_t have_colony_for;
    uint8_t have_eco_restoration_n; /* 2, 3, 5, 10, 20 */
    uint8_t have_terraform_n;   /* 0, 10, ... 120 */
    uint8_t terraform_cost_per_inc; /* 5..2 */
    bool have_adv_soil_enrich;
    bool have_atmos_terra;
    bool have_soil_enrich;
    uint8_t inc_pop_cost;  /* cost of adding 1 population */
    uint8_t scanner_range; /* 3, 5, 7, 9 */
    bool have_ia_scanner;
    bool have_adv_scanner;
    bool have_hyperspace_comm;
    bool have_stargates;
    uint8_t colonist_oper_factories; /* 2.. */
    uint8_t factory_cost;   /* 10..2 */
    uint8_t factory_adj_cost;   /* meklar ? factory_cost : factory_cost*colonist_oper_factories/2 */
    uint8_t ind_waste_scale;    /* 0, 2, ..10 */
    uint8_t fuel_range;     /* 3..10, 30 */
    bool have_combat_transporter;
    techdata_t tech;
    uint8_t have_engine;    /* 1.. */
    uint8_t shipdesigns_num;
    fleet_orbit_t orbit[PLANETS_MAX];
    uint8_t spyreportfield[PLAYER_NUM][TECH_FIELD_NUM];
    uint16_t spyreportyear[PLAYER_NUM];
    int8_t shipi_colony;
    int8_t shipi_bomber;
} empiretechorbit_t;

#define NEWTECH_MAX 15

typedef struct monster_s {
    uint8_t exists; /* 0..3 */
    int16_t x;
    int16_t y;
    player_id_t killer; /* MOO1: 0 or id+1 */
    uint8_t dest;
    int8_t counter;
    int8_t nuked;   /* planets destroyed */
} monster_t;

typedef struct newtech_s {
    tech_field_t field;
    uint8_t tech;
    techsource_t source;
    int8_t v06;    /* 4: race_t giver  2: NEWTECH_V06_ORION or planet_i ruins or -(planet_i+1) artifact */
    player_id_t stolen_from;
    bool frame;
    player_id_t other1;
    player_id_t other2;
} newtech_t;

typedef struct nexttech_s {
    uint8_t num;
    uint8_t tech[TECH_NEXT_MAX];
} nexttech_t;

#define NEWTECH_V06_ORION   PLANETS_MAX

typedef struct newtechs_s {
    uint8_t num;
    newtech_t d[NEWTECH_MAX];
    nexttech_t next[TECH_FIELD_NUM];
} newtechs_t;

#define GAME_EVENT_TBL_NUM  20
#define HELP_SHOWN_NUM  16

typedef struct gameevents_s {
    uint16_t year;
    BOOLVEC_DECLARE(done, GAME_EVENT_TBL_NUM);
    int8_t diplo_msg_subtype; /* -1..13 */
    uint8_t have_plague;    /* 0..3 */
    player_id_t plague_player;
    uint8_t plague_planet_i;
    int plague_val;
    bool have_quake;
    player_id_t quake_player;
    uint8_t quake_planet_i;
    uint8_t have_nova;  /* 0..3 */
    player_id_t nova_player;
    uint8_t nova_planet_i;
    int8_t nova_years;
    int nova_val;
    uint8_t have_accident;  /* 0..2 */
    player_id_t accident_player;
    uint8_t accident_planet_i;
    bool have_assassin;
    player_id_t assassin_player;
    player_id_t assassin_player2;
    bool have_virus;
    player_id_t virus_player;
    tech_field_t virus_field;
    uint8_t have_comet; /* 0..3 */
    player_id_t comet_player;
    uint8_t comet_planet_i;
    uint8_t comet_years;
    uint16_t comet_hp;
    uint16_t comet_dmg;
    uint8_t have_pirates;   /* 0..3 */
    uint8_t pirates_planet_i;
    uint16_t pirates_hp;
    bool have_derelict;
    player_id_t derelict_player;
    monster_t crystal;
    monster_t amoeba;
    bool have_enviro;
    uint8_t enviro_planet_i;
    bool have_rich;
    uint8_t rich_planet_i;
    bool have_support;
    player_id_t support_player;
    bool have_poor;
    uint8_t poor_planet_i;
    uint8_t have_orion_conquer; /* 0, pi+1 */
    uint8_t planet_orion_i;
    bool have_guardian;
    uint8_t home[PLAYER_NUM];   /* home planet index or PLANET_NONE if dead */
    uint8_t report_stars;
    BOOLVEC_DECLARE(coup, PLAYER_NUM);
    newtechs_t newtech[PLAYER_NUM];
    shipsum_t new_ships[PLAYER_NUM][NUM_SHIPDESIGNS];
    uint16_t spies_caught[PLAYER_NUM][PLAYER_NUM]; /* [catcher][spy] */
    uint16_t spied_num[PLAYER_NUM][PLAYER_NUM]; /* [victim][spy] */
    int16_t spied_spy[PLAYER_NUM][PLAYER_NUM]; /* [victim][spy] */
    tech_field_t stolen_field[PLAYER_NUM][PLAYER_NUM]; /* [victim][spy] */
    uint8_t stolen_tech[PLAYER_NUM][PLAYER_NUM]; /* [victim][spy] */
    player_id_t stolen_spy[PLAYER_NUM][PLAYER_NUM]; /* [victim][spy] */
    bool sabotage_is_bases[PLAYER_NUM][PLAYER_NUM]; /* [victim][spy] */
    uint8_t sabotage_planet[PLAYER_NUM][PLAYER_NUM]; /* [victim][spy] */
    uint16_t sabotage_num[PLAYER_NUM][PLAYER_NUM]; /* [victim][spy] */
    player_id_t sabotage_spy[PLAYER_NUM][PLAYER_NUM]; /* [victim][spy] */
    int16_t ceasefire[PLAYER_NUM][PLAYER_NUM]; /* [human][ai] */
    BOOLVEC_TBL_DECLARE(help_shown, PLAYER_NUM, HELP_SHOWN_NUM);
    uint16_t build_finished_num[PLAYER_NUM];
    player_id_t voted[PLAYER_NUM];
    uint8_t best_ecorestore[PLAYER_NUM];
    uint8_t best_wastereduce[PLAYER_NUM];
    uint8_t best_roboctrl[PLAYER_NUM];
    uint8_t best_terraform[PLAYER_NUM];
} gameevents_t;

typedef struct seen_s {
    player_id_t owner;
    uint16_t pop;
    uint16_t bases;
    uint16_t factories;
} seen_t;

#define EMPEROR_NAME_LEN    15
#define NEBULA_MAX  4
#define YEAR_BASE   2299

struct game_aux_s;

struct game_s {
    uint16_t enroute_num;
    uint16_t transport_num;
    uint16_t year;  /* init to 1 */
    uint8_t players;
    uint8_t ai_id;
    BOOLVEC_DECLARE(is_ai, PLAYER_NUM);
    player_id_t active_player;
    difficulty_t difficulty;
    galaxy_size_t galaxy_size;
    uint8_t galaxy_w;  /* 6 8 a c */
    uint8_t galaxy_h;  /* 4 6 7 9 */
    uint8_t galaxy_stars;  /* w*h */
    uint16_t galaxy_maxx;
    uint16_t galaxy_maxy;
    uint32_t seed;   /* current random seed */
    uint32_t galaxy_seed; /* seed of generated galaxy */
    game_end_type_t end;
    player_id_t winner;
    player_id_t guardian_killer;
    bool election_held;
    BOOLVEC_DECLARE(refuse, PLAYER_NUM);
    uint8_t planet_focus_i[PLAYER_NUM];
    uint8_t nebula_num;        /* 0..4 */
    uint8_t nebula_type[NEBULA_MAX];    /* 0..9 */
    uint16_t nebula_x[NEBULA_MAX];
    uint16_t nebula_y[NEBULA_MAX];
    uint16_t nebula_x0[NEBULA_MAX][4];
    uint16_t nebula_x1[NEBULA_MAX][4];
    uint16_t nebula_y0[NEBULA_MAX][4];
    uint16_t nebula_y1[NEBULA_MAX][4];
    planet_t planet[PLANETS_MAX];
    fleet_enroute_t enroute[FLEET_ENROUTE_MAX];
    transport_t transport[TRANSPORT_MAX];
    empiretechorbit_t eto[PLAYER_NUM];
    shipresearch_t srd[PLAYER_NUM];
    gameevents_t evn;
    char emperor_names[PLAYER_NUM][EMPEROR_NAME_LEN];
    seen_t seen[PLAYER_NUM][PLANETS_MAX];
    shipdesign_t current_design[PLAYER_NUM];
    struct game_aux_s *gaux;
};

static inline bool IS_PLAYER(const struct game_s *g, player_id_t i)
{
    return (i >= 0) && (i < g->players);
}

static inline bool IS_HUMAN(const struct game_s *g, player_id_t i)
{
    /*
        In MOO1, instead of IS_HUMAN, the index is checked for equality to zero,
        which in turn implicitly guarantees that the index is less than g->players.
        The IS_PLAYER check eliminates the potential for memory corruption.
    */
    return IS_PLAYER(g, i) && BOOLVEC_IS0(g->is_ai, i);
}

static inline bool IS_ALIVE(const struct game_s *g, player_id_t i)
{
    return (g->evn.home[i] != PLANET_NONE);
}

#define IS_AI(_g_, _i_) BOOLVEC_IS1((_g_)->is_ai, (_i_))

extern bool game_opt_skip_intro_always;
extern bool game_opt_message_filter[FINISHED_NUM];
extern struct game_new_options_s game_opt_custom;

#endif
