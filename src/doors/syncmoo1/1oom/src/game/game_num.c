#include "config.h"

#include "game_num.h"
#include "game_types.h"
#include "types.h"

/* -------------------------------------------------------------------------- */

bool game_num_deterministic = false;
bool game_num_ai_trans_range_fix = false;
bool game_num_ai_4_colony_curse_fix = false;
bool game_num_ai_first_tech_cost_fix = false;
int game_num_bt_turn_max = 50;
bool game_num_bt_wait_no_reload = false;
bool game_num_bt_precap_tohit = false;
bool game_num_bt_no_tohit_acc = false;
bool game_num_bt_oracle_fix = false;
int game_num_stargate_cost = 3000;
int game_num_stargate_maint = 100;
int game_num_weapon_list_max = 30;
int game_num_limit_ships = 32000;
int game_num_limit_ships_all = 32000;
int game_num_max_pop = 300;
int game_num_max_factories = 2500;
int game_num_max_inbound = 300;
int game_num_atmos_cost = 200;
int game_num_soil_cost = 150;
int game_num_adv_soil_cost = 300;
int game_num_adv_scan_range = 110;
int game_num_pop_hp = 200;
int game_num_fact_hp = 50;
int game_num_max_bomb_dmg = 100000;
int game_num_max_bio_dmg = 10000;
int game_num_max_trans_dmg = 32000;
int game_num_max_ship_maint = 32000;
int game_num_max_tribute_bc = 32000;
int game_num_event_roll = 512;
bool game_num_accident_chk_factories = false;
int game_num_council_years = 25;
bool game_num_news_orion = false;
bool game_num_aud_ask_break_nap = false;
bool game_num_aud_bounty_give = false;
bool game_num_aud_update_tech = false;
bool game_num_update_on_load = true;
bool game_num_newtech_adjust_fix = false;
bool game_num_monster_rest_att = false;
bool game_num_orbital_bio_fix = false;
bool game_num_orbital_weap_any = false;
bool game_num_orbital_weap_4 = false;
bool game_num_orbital_torpedo = false;
bool game_num_orbital_comp_fix = false;
bool game_num_combat_trans_fix = false;
bool game_num_stargate_redir_fix = false;
bool game_num_trans_redir_fix = false;
bool game_num_retreat_redir_fix = false;
bool game_num_first_tech_rp_fix = false;
bool game_num_waste_calc_fix = false;
bool game_num_waste_adjust_fix = false;
bool game_num_slider_respects_locks = false;
bool game_num_pop_tenths_fix = false;
bool game_num_factory_cost_fix = false;
bool game_num_doom_stack_fix = false;
uint8_t game_num_eco_slider_slack = 7;
bool game_num_reset_tform_to_max = true;
bool game_num_soil_rounding_fix = false;
bool game_num_leaving_trans_fix = false;
bool game_num_hidden_child_labor_fix = false;
bool game_num_extended_reloc_range = false;
bool game_num_colonized_factories_fix = false;
bool game_num_cond_switch_to_ind_fix = false;
bool game_num_slider_eco_done_fix = false;
bool game_num_ship_scanner_fix = false;
bool game_num_ai_fleet_cheating_fix = false;

uint8_t game_num_tbl_hull_w[4] = { 1, 5, 25, 125 };

uint16_t game_num_base_hp[BASE_HP_TBL_NUM] = { 50, 75, 100, 125, 150, 175, 200 };
uint16_t game_num_pshield_cost[PSHIELD_NUM] = { 0, 500, 1000, 1500, 2000 };

uint8_t game_num_tech_costmuld[DIFFICULTY_NUM] = { 20, 25, 30, 35, 40 };
uint8_t game_num_tech_costmula[DIFFICULTY_NUM] = { 20, 20, 20, 20, 20 };
int game_num_tech_costmuld2 = 100;
int game_num_tech_costmula2 = 100;
int game_num_tech_costmul = 100;

uint8_t game_num_tech_costmulr[RACE_NUM][TECH_FIELD_NUM] = {
    { 100, 100, 60, 80, 80, 100 },
    { 100, 125, 100, 100, 100, 60 },
    { 80, 125, 125, 125, 125, 125 },
    { 100, 100, 100, 60, 100, 100 },
    { 80, 80, 80, 80, 80, 80 },
    { 100, 100, 125, 100, 60, 100 },
    { 100, 60, 100, 100, 125, 100 },
    { 125, 80, 100, 100, 100, 80 },
    { 60, 100, 100, 125, 100, 100 },
    { 80, 100, 100, 100, 100, 100 }
};

                     /*   1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31 32 33 34 35 36 37 38 39 40 41 42 43 44 45 46 47 48 49 50 */
#define GAME_NUMT_CM { 0, 0, 0, 0, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 0, 0 }
#define GAME_NUMT_CN { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }
#define GAME_NUMT_FF { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }
#define GAME_NUMT_PL { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0 }
#define GAME_NUMT_PR { 0, 0, 0, 4, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }
#define GAME_NUMT_WE { 0, 0, 0, 0, 4, 0, 0, 0, 4, 0, 0, 4, 0, 0, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 4, 0, 0, 4, 0, 0, 0, 0, 0, 0 }

uint8_t game_num_ng_tech[RACE_NUM][TECH_FIELD_NUM][50 + 1] = {
    { GAME_NUMT_CM, GAME_NUMT_CN, GAME_NUMT_FF, GAME_NUMT_PL, GAME_NUMT_PR, GAME_NUMT_WE },
    { GAME_NUMT_CM, GAME_NUMT_CN, GAME_NUMT_FF, GAME_NUMT_PL, GAME_NUMT_PR, GAME_NUMT_WE },
    {   /* silicoid */
        GAME_NUMT_CM,
        /*   1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31 32 33 34 35 36 37 38 39 40 41 42 43 44 45 46 47 48 49 50 */
        { 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0 },
        GAME_NUMT_FF,
        { 0, 0, 0, 1, 0, 1, 1, 0, 0, 1, 0, 0, 1, 1, 0, 1, 1, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 0, 1, 0, 0, 0, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0 },
        GAME_NUMT_PR, GAME_NUMT_WE
    },
    { GAME_NUMT_CM, GAME_NUMT_CN, GAME_NUMT_FF, GAME_NUMT_PL, GAME_NUMT_PR, GAME_NUMT_WE },
    { GAME_NUMT_CM, GAME_NUMT_CN, GAME_NUMT_FF, GAME_NUMT_PL, GAME_NUMT_PR, GAME_NUMT_WE },
    { GAME_NUMT_CM, GAME_NUMT_CN, GAME_NUMT_FF, GAME_NUMT_PL, GAME_NUMT_PR, GAME_NUMT_WE },
    { GAME_NUMT_CM, GAME_NUMT_CN, GAME_NUMT_FF, GAME_NUMT_PL, GAME_NUMT_PR, GAME_NUMT_WE },
    { GAME_NUMT_CM, GAME_NUMT_CN, GAME_NUMT_FF, GAME_NUMT_PL, GAME_NUMT_PR, GAME_NUMT_WE },
    { GAME_NUMT_CM, GAME_NUMT_CN, GAME_NUMT_FF, GAME_NUMT_PL, GAME_NUMT_PR, GAME_NUMT_WE },
    { GAME_NUMT_CM, GAME_NUMT_CN, GAME_NUMT_FF, GAME_NUMT_PL, GAME_NUMT_PR, GAME_NUMT_WE }
};

uint8_t game_num_tbl_trait1[RACE_NUM][TRAIT1_TBL_NUM] = {
    { 4, 4, 4, 4, 4, 4, 4, 5, 5, 3 },
    { 1, 1, 1, 1, 1, 1, 1, 2, 2, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 2, 2, 3 },
    { 2, 2, 2, 2, 2, 2, 2, 3, 3, 1 },
    { 5, 5, 5, 5, 5, 5, 5, 4, 3, 3 },
    { 4, 4, 4, 4, 4, 4, 4, 5, 5, 3 },
    { 0, 0, 0, 0, 0, 0, 0, 2, 2, 1 },
    { 2, 2, 2, 2, 2, 2, 2, 3, 3, 1 },
    { 3, 3, 3, 3, 3, 3, 3, 2, 2, 0 },
    { 2, 2, 2, 2, 2, 2, 2, 1, 1, 0 }
};

uint8_t game_num_tbl_trait2[RACE_NUM][TRAIT2_TBL_NUM] = {
    { 0, 0, 0, 0, 1, 2, 3, 4, 4, 0, 3, 5 },
    { 0, 1, 1, 1, 1, 3, 2, 2, 3, 3, 4, 5 },
    { 0, 1, 2, 2, 2, 2, 3, 3, 3, 4, 4, 5 },
    { 0, 3, 1, 5, 5, 3, 4, 2, 2, 2, 2, 2 },
    { 0, 0, 3, 1, 2, 3, 3, 3, 3, 4, 4, 5 },
    { 0, 1, 1, 1, 2, 2, 2, 3, 4, 2, 3, 5 },
    { 0, 1, 1, 2, 2, 3, 3, 4, 5, 5, 5, 5 },
    { 0, 5, 5, 5, 5, 2, 2, 3, 3, 3, 4, 5 },
    { 0, 1, 2, 2, 3, 3, 3, 4, 4, 4, 4, 5 },
    { 0, 0, 3, 3, 1, 2, 0, 3, 3, 4, 4, 5 }
};

uint8_t game_num_tbl_tech_autoadj[4] = { 0, 25, 50, 75 };
