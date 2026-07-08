#ifndef INC_1OOM_GAME_NUM_H
#define INC_1OOM_GAME_NUM_H

#include "game_types.h"
#include "types.h"

extern bool game_num_deterministic;
extern bool game_num_ai_trans_range_fix;
extern bool game_num_ai_4_colony_curse_fix;
extern bool game_num_ai_first_tech_cost_fix;
extern int game_num_bt_turn_max;
extern bool game_num_bt_wait_no_reload;
extern bool game_num_bt_precap_tohit;
extern bool game_num_bt_no_tohit_acc;
extern bool game_num_bt_oracle_fix;
extern int game_num_stargate_cost;
extern int game_num_stargate_maint;
extern int game_num_weapon_list_max;
extern int game_num_limit_ships;
extern int game_num_limit_ships_all;
extern int game_num_max_pop;
extern int game_num_max_factories;
extern int game_num_max_inbound;
extern int game_num_atmos_cost;
extern int game_num_soil_cost;
extern int game_num_adv_soil_cost;
extern int game_num_adv_scan_range;
extern int game_num_pop_hp;
extern int game_num_fact_hp;
extern int game_num_max_bomb_dmg;
extern int game_num_max_bio_dmg;
extern int game_num_max_trans_dmg;
extern int game_num_max_ship_maint;
extern int game_num_max_tribute_bc;
extern int game_num_event_roll;
extern bool game_num_accident_chk_factories;
extern int game_num_council_years;
extern bool game_num_news_orion;
extern bool game_num_aud_ask_break_nap;
extern bool game_num_aud_bounty_give;
extern bool game_num_aud_update_tech;
extern bool game_num_update_on_load;
extern bool game_num_newtech_adjust_fix;
extern bool game_num_monster_rest_att;
extern bool game_num_orbital_bio_fix;
extern bool game_num_orbital_weap_any;
extern bool game_num_orbital_weap_4;
extern bool game_num_orbital_torpedo;
extern bool game_num_orbital_comp_fix;
extern bool game_num_combat_trans_fix;
extern bool game_num_stargate_redir_fix;
extern bool game_num_trans_redir_fix;
extern bool game_num_retreat_redir_fix;
extern bool game_num_first_tech_rp_fix;
extern bool game_num_waste_calc_fix;
extern bool game_num_waste_adjust_fix;
extern bool game_num_slider_respects_locks;
extern bool game_num_pop_tenths_fix;
extern bool game_num_factory_cost_fix;
extern bool game_num_doom_stack_fix;
extern uint8_t game_num_eco_slider_slack;
extern bool game_num_reset_tform_to_max;
extern bool game_num_soil_rounding_fix;
extern bool game_num_leaving_trans_fix;
extern bool game_num_hidden_child_labor_fix;
extern bool game_num_extended_reloc_range;
extern bool game_num_colonized_factories_fix;
extern bool game_num_cond_switch_to_ind_fix;
extern bool game_num_slider_eco_done_fix;
extern bool game_num_ship_scanner_fix;
extern bool game_num_ai_fleet_cheating_fix;
extern uint8_t game_num_tbl_hull_w[4];
extern uint8_t game_num_tech_costmuld[DIFFICULTY_NUM];
extern uint8_t game_num_tech_costmula[DIFFICULTY_NUM];
extern int game_num_tech_costmuld2;
extern int game_num_tech_costmula2;
extern int game_num_tech_costmul;
extern uint8_t game_num_tech_costmulr[RACE_NUM][TECH_FIELD_NUM];
#define GAME_NG_TECH_NEVER  (1 << 0)
#define GAME_NG_TECH_ALWAYS (1 << 1)
extern uint8_t game_num_ng_tech[RACE_NUM][TECH_FIELD_NUM][50 + 1];
#define BASE_HP_TBL_NUM 7
extern uint16_t game_num_base_hp[BASE_HP_TBL_NUM];
#define TRAIT1_TBL_NUM 10
#define TRAIT2_TBL_NUM 12
extern uint8_t game_num_tbl_trait1[RACE_NUM][TRAIT1_TBL_NUM];
extern uint8_t game_num_tbl_trait2[RACE_NUM][TRAIT2_TBL_NUM];
#define PSHIELD_NUM 5
extern uint16_t game_num_pshield_cost[PSHIELD_NUM];
extern uint8_t game_num_tbl_tech_autoadj[4];

#endif
