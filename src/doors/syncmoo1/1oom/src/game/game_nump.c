#include "config.h"

#include <string.h>

#include "game_nump.h"
#include "gameapi.h"
#include "game_aux.h"
#include "game_num.h"
#include "game_parsed.h"
#include "game_shipdesign.h"
#include "game_shiptech.h"
#include "lib.h"
#include "log.h"

/* -------------------------------------------------------------------------- */

#define DEFNUMITEMLSTBL(id, name, field, type, vmin, vmax) { #id, &(name->field), type, sizeof(*name), sizeof(name) / sizeof(*name), vmin, vmax }
#define DEFNUMITEMLTBL(id, name, type, vmin, vmax)  { #id, &name, type, sizeof(*name), sizeof(name) / sizeof(*name), vmin, vmax }
#define DEFNUMITEMLTBLL(name, type, vmin, vmax)     { #name, game_num_##name, type, sizeof(*game_num_##name), sizeof(game_num_##name) / sizeof(*game_num_##name), vmin, vmax }
#define DEFNUMITEM(id, name, type, vmin, vmax)      { #id, &name, type, sizeof(name), 1, vmin, vmax }
#define DEFNUMITEML(name, type, vmin, vmax)         DEFNUMITEM(name, game_num_##name, type, vmin, vmax)
#define DEFNUMEND           { NULL, NULL, NUMTYPE_S, 0, 0, 0, 0 }

#define NUMTYPE_GUESS(z)    ((sizeof(z) == 4) ? NUMTYPE_U32 : ((sizeof(z) == 2) ? NUMTYPE_U16 : NUMTYPE_U8))
#define NUMTYPE_IS_S(t)     (!(t & 1))

static const struct numtbl_s {
    const char *numid;
    void *ptr;
    enum {
        NUMTYPE_S = 0,
        NUMTYPE_U,
        NUMTYPE_S8,
        NUMTYPE_U8,
        NUMTYPE_S16,
        NUMTYPE_U16,
        NUMTYPE_S32,
        NUMTYPE_U32,
        NUMTYPE_BOOL
    } numtype;
    int tstep;
    int size;
    int vmin;
    uint32_t vmax;
} game_num_id_tbl[] = {
    DEFNUMITEMLSTBL(st_weap_dmgmin, tbl_shiptech_weap, damagemin, NUMTYPE_U16, 0, 0xffff),
    DEFNUMITEMLSTBL(st_weap_dmgmax, tbl_shiptech_weap, damagemax, NUMTYPE_U16, 0, 0xffff),
    DEFNUMITEMLSTBL(st_weap_range, tbl_shiptech_weap, range, NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLSTBL(st_weap_extraacc, tbl_shiptech_weap, extraacc, NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLSTBL(st_weap_halveshield, tbl_shiptech_weap, halveshield, NUMTYPE_BOOL, 0, 1),
    DEFNUMITEMLSTBL(st_weap_is_bomb, tbl_shiptech_weap, is_bomb, NUMTYPE_BOOL, 0, 1),
    DEFNUMITEMLSTBL(st_weap_dmgfade, tbl_shiptech_weap, damagefade, NUMTYPE_BOOL, 0, 1),
    DEFNUMITEMLSTBL(st_weap_misstype, tbl_shiptech_weap, misstype, NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLSTBL(st_weap_dmgmul, tbl_shiptech_weap, damagemul, NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLSTBL(st_weap_numfire, tbl_shiptech_weap, numfire, NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLSTBL(st_weap_numshots, tbl_shiptech_weap, numshots, NUMTYPE_S8, -1, 0x7f),
    DEFNUMITEMLSTBL(st_weap_cost, tbl_shiptech_weap, cost, NUMTYPE_U16, 0, 0xffff),
    DEFNUMITEMLSTBL(st_weap_space, tbl_shiptech_weap, space, NUMTYPE_U16, 0, 0xffff),
    DEFNUMITEMLSTBL(st_weap_power, tbl_shiptech_weap, power, NUMTYPE_U16, 0, 0xffff),
    DEFNUMITEMLSTBL(st_weap_is_bio, tbl_shiptech_weap, is_bio, NUMTYPE_BOOL, 0, 1),
    DEFNUMITEMLSTBL(st_comp_tech, tbl_shiptech_weap, tech_i, NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLSTBL(st_comp_v24, tbl_shiptech_weap, v24, NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLSTBL(st_comp_dtbl0, tbl_shiptech_weap, dtbl[0], NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLSTBL(st_comp_dtbl1, tbl_shiptech_weap, dtbl[1], NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLSTBL(st_comp_dtbl2, tbl_shiptech_weap, dtbl[2], NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLSTBL(st_comp_dtbl3, tbl_shiptech_weap, dtbl[3], NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLSTBL(st_comp_dtbl4, tbl_shiptech_weap, dtbl[4], NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLSTBL(st_comp_dtbl4, tbl_shiptech_weap, dtbl[5], NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLSTBL(st_comp_dtbl6, tbl_shiptech_weap, dtbl[6], NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLSTBL(st_comp_sound, tbl_shiptech_weap, v24, NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLSTBL(st_comp_nummiss, tbl_shiptech_weap, nummiss, NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLSTBL(st_comp_cost0, tbl_shiptech_comp, cost[0], NUMTYPE_U16, 0, 0xffff),
    DEFNUMITEMLSTBL(st_comp_cost1, tbl_shiptech_comp, cost[1], NUMTYPE_U16, 0, 0xffff),
    DEFNUMITEMLSTBL(st_comp_cost2, tbl_shiptech_comp, cost[2], NUMTYPE_U16, 0, 0xffff),
    DEFNUMITEMLSTBL(st_comp_cost3, tbl_shiptech_comp, cost[3], NUMTYPE_U16, 0, 0xffff),
    DEFNUMITEMLSTBL(st_comp_space0, tbl_shiptech_comp, space[0], NUMTYPE_U16, 0, 0xffff),
    DEFNUMITEMLSTBL(st_comp_space1, tbl_shiptech_comp, space[1], NUMTYPE_U16, 0, 0xffff),
    DEFNUMITEMLSTBL(st_comp_space2, tbl_shiptech_comp, space[2], NUMTYPE_U16, 0, 0xffff),
    DEFNUMITEMLSTBL(st_comp_space3, tbl_shiptech_comp, space[3], NUMTYPE_U16, 0, 0xffff),
    DEFNUMITEMLSTBL(st_comp_power0, tbl_shiptech_comp, power[0], NUMTYPE_U16, 0, 0xffff),
    DEFNUMITEMLSTBL(st_comp_power1, tbl_shiptech_comp, power[1], NUMTYPE_U16, 0, 0xffff),
    DEFNUMITEMLSTBL(st_comp_power2, tbl_shiptech_comp, power[2], NUMTYPE_U16, 0, 0xffff),
    DEFNUMITEMLSTBL(st_comp_power3, tbl_shiptech_comp, power[3], NUMTYPE_U16, 0, 0xffff),
    DEFNUMITEMLSTBL(st_comp_tech, tbl_shiptech_comp, tech_i, NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLSTBL(st_comp_level, tbl_shiptech_comp, level, NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLSTBL(st_jamm_cost0, tbl_shiptech_jammer, cost[0], NUMTYPE_U16, 0, 0xffff),
    DEFNUMITEMLSTBL(st_jamm_cost1, tbl_shiptech_jammer, cost[1], NUMTYPE_U16, 0, 0xffff),
    DEFNUMITEMLSTBL(st_jamm_cost2, tbl_shiptech_jammer, cost[2], NUMTYPE_U16, 0, 0xffff),
    DEFNUMITEMLSTBL(st_jamm_cost3, tbl_shiptech_jammer, cost[3], NUMTYPE_U16, 0, 0xffff),
    DEFNUMITEMLSTBL(st_jamm_space0, tbl_shiptech_jammer, space[0], NUMTYPE_U16, 0, 0xffff),
    DEFNUMITEMLSTBL(st_jamm_space1, tbl_shiptech_jammer, space[1], NUMTYPE_U16, 0, 0xffff),
    DEFNUMITEMLSTBL(st_jamm_space2, tbl_shiptech_jammer, space[2], NUMTYPE_U16, 0, 0xffff),
    DEFNUMITEMLSTBL(st_jamm_space3, tbl_shiptech_jammer, space[3], NUMTYPE_U16, 0, 0xffff),
    DEFNUMITEMLSTBL(st_jamm_power0, tbl_shiptech_jammer, power[0], NUMTYPE_U16, 0, 0xffff),
    DEFNUMITEMLSTBL(st_jamm_power1, tbl_shiptech_jammer, power[1], NUMTYPE_U16, 0, 0xffff),
    DEFNUMITEMLSTBL(st_jamm_power2, tbl_shiptech_jammer, power[2], NUMTYPE_U16, 0, 0xffff),
    DEFNUMITEMLSTBL(st_jamm_power3, tbl_shiptech_jammer, power[3], NUMTYPE_U16, 0, 0xffff),
    DEFNUMITEMLSTBL(st_jamm_tech, tbl_shiptech_jammer, tech_i, NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLSTBL(st_jamm_level, tbl_shiptech_jammer, level, NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLSTBL(st_engn_cost, tbl_shiptech_engine, cost, NUMTYPE_U16, 0, 0xffff),
    DEFNUMITEMLSTBL(st_engn_space, tbl_shiptech_engine, space, NUMTYPE_U16, 0, 0xffff),
    DEFNUMITEMLSTBL(st_engn_power, tbl_shiptech_engine, power, NUMTYPE_U16, 0, 0xffff),
    DEFNUMITEMLSTBL(st_engn_warp, tbl_shiptech_engine, warp, NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLSTBL(st_engn_tech, tbl_shiptech_engine, tech_i, NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLSTBL(st_armr_cost0, tbl_shiptech_armor, cost[0], NUMTYPE_U16, 0, 0xffff),
    DEFNUMITEMLSTBL(st_armr_cost1, tbl_shiptech_armor, cost[1], NUMTYPE_U16, 0, 0xffff),
    DEFNUMITEMLSTBL(st_armr_cost2, tbl_shiptech_armor, cost[2], NUMTYPE_U16, 0, 0xffff),
    DEFNUMITEMLSTBL(st_armr_cost3, tbl_shiptech_armor, cost[3], NUMTYPE_U16, 0, 0xffff),
    DEFNUMITEMLSTBL(st_armr_space0, tbl_shiptech_armor, space[0], NUMTYPE_U16, 0, 0xffff),
    DEFNUMITEMLSTBL(st_armr_space1, tbl_shiptech_armor, space[1], NUMTYPE_U16, 0, 0xffff),
    DEFNUMITEMLSTBL(st_armr_space2, tbl_shiptech_armor, space[2], NUMTYPE_U16, 0, 0xffff),
    DEFNUMITEMLSTBL(st_armr_space3, tbl_shiptech_armor, space[3], NUMTYPE_U16, 0, 0xffff),
    DEFNUMITEMLSTBL(st_armr_tech, tbl_shiptech_armor, tech_i, NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLSTBL(st_armr_armor, tbl_shiptech_armor, armor, NUMTYPE_U16, 0, 0xffff),
    DEFNUMITEMLSTBL(st_shld_cost0, tbl_shiptech_shield, cost[0], NUMTYPE_U16, 0, 0xffff),
    DEFNUMITEMLSTBL(st_shld_cost1, tbl_shiptech_shield, cost[1], NUMTYPE_U16, 0, 0xffff),
    DEFNUMITEMLSTBL(st_shld_cost2, tbl_shiptech_shield, cost[2], NUMTYPE_U16, 0, 0xffff),
    DEFNUMITEMLSTBL(st_shld_cost3, tbl_shiptech_shield, cost[3], NUMTYPE_U16, 0, 0xffff),
    DEFNUMITEMLSTBL(st_shld_space0, tbl_shiptech_shield, space[0], NUMTYPE_U16, 0, 0xffff),
    DEFNUMITEMLSTBL(st_shld_space1, tbl_shiptech_shield, space[1], NUMTYPE_U16, 0, 0xffff),
    DEFNUMITEMLSTBL(st_shld_space2, tbl_shiptech_shield, space[2], NUMTYPE_U16, 0, 0xffff),
    DEFNUMITEMLSTBL(st_shld_space3, tbl_shiptech_shield, space[3], NUMTYPE_U16, 0, 0xffff),
    DEFNUMITEMLSTBL(st_shld_power0, tbl_shiptech_shield, power[0], NUMTYPE_U16, 0, 0xffff),
    DEFNUMITEMLSTBL(st_shld_power1, tbl_shiptech_shield, power[1], NUMTYPE_U16, 0, 0xffff),
    DEFNUMITEMLSTBL(st_shld_power2, tbl_shiptech_shield, power[2], NUMTYPE_U16, 0, 0xffff),
    DEFNUMITEMLSTBL(st_shld_power3, tbl_shiptech_shield, power[3], NUMTYPE_U16, 0, 0xffff),
    DEFNUMITEMLSTBL(st_shld_absorb, tbl_shiptech_shield, absorb, NUMTYPE_U16, 0, 0xffff),
    DEFNUMITEMLSTBL(st_shld_tech, tbl_shiptech_shield, tech_i, NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLSTBL(st_spec_cost0, tbl_shiptech_special, cost[0], NUMTYPE_U16, 0, 0xffff),
    DEFNUMITEMLSTBL(st_spec_cost1, tbl_shiptech_special, cost[1], NUMTYPE_U16, 0, 0xffff),
    DEFNUMITEMLSTBL(st_spec_cost2, tbl_shiptech_special, cost[2], NUMTYPE_U16, 0, 0xffff),
    DEFNUMITEMLSTBL(st_spec_cost3, tbl_shiptech_special, cost[3], NUMTYPE_U16, 0, 0xffff),
    DEFNUMITEMLSTBL(st_spec_space0, tbl_shiptech_special, space[0], NUMTYPE_U16, 0, 0xffff),
    DEFNUMITEMLSTBL(st_spec_space1, tbl_shiptech_special, space[1], NUMTYPE_U16, 0, 0xffff),
    DEFNUMITEMLSTBL(st_spec_space2, tbl_shiptech_special, space[2], NUMTYPE_U16, 0, 0xffff),
    DEFNUMITEMLSTBL(st_spec_space3, tbl_shiptech_special, space[3], NUMTYPE_U16, 0, 0xffff),
    DEFNUMITEMLSTBL(st_spec_power0, tbl_shiptech_special, power[0], NUMTYPE_U16, 0, 0xffff),
    DEFNUMITEMLSTBL(st_spec_power1, tbl_shiptech_special, power[1], NUMTYPE_U16, 0, 0xffff),
    DEFNUMITEMLSTBL(st_spec_power2, tbl_shiptech_special, power[2], NUMTYPE_U16, 0, 0xffff),
    DEFNUMITEMLSTBL(st_spec_power3, tbl_shiptech_special, power[3], NUMTYPE_U16, 0, 0xffff),
    DEFNUMITEMLSTBL(st_spec_tech, tbl_shiptech_special, tech_i, NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLSTBL(st_spec_field, tbl_shiptech_special, field, NUMTYPE_GUESS(tbl_shiptech_special[0].field), 0, 5),
    DEFNUMITEMLSTBL(st_spec_type, tbl_shiptech_special, type, NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLSTBL(st_spec_repair, tbl_shiptech_special, repair, NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLSTBL(st_spec_extraman, tbl_shiptech_special, extraman, NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLSTBL(st_spec_misshield, tbl_shiptech_special, misshield, NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLSTBL(st_spec_extrarange, tbl_shiptech_special, extrarange, NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLSTBL(st_spec_pulsar, tbl_shiptech_special, pulsar, NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLSTBL(st_spec_stream, tbl_shiptech_special, stream, NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLSTBL(st_spec_boolmask, tbl_shiptech_special, boolmask, NUMTYPE_U16, 0, 0xffff),
    DEFNUMITEMLSTBL(st_hull_cost, tbl_shiptech_hull, cost, NUMTYPE_U16, 0, 0xffff),
    DEFNUMITEMLSTBL(st_hull_space, tbl_shiptech_hull, space, NUMTYPE_U16, 0, 0xffff),
    DEFNUMITEMLSTBL(st_hull_hits, tbl_shiptech_hull, hits, NUMTYPE_U16, 0, 0xffff),
    DEFNUMITEMLSTBL(st_hull_power, tbl_shiptech_hull, power, NUMTYPE_U16, 0, 0xffff),
    DEFNUMITEMLSTBL(st_hull_defense, tbl_shiptech_hull, defense, NUMTYPE_S16, -0x8000, 0x7fff),
    DEFNUMITEML(deterministic, NUMTYPE_BOOL, 0, 1),
    DEFNUMITEML(ai_trans_range_fix, NUMTYPE_BOOL, 0, 1),
    DEFNUMITEML(ai_4_colony_curse_fix, NUMTYPE_BOOL, 0, 1),
    DEFNUMITEML(ai_first_tech_cost_fix, NUMTYPE_BOOL, 0, 1),
    DEFNUMITEML(bt_turn_max, NUMTYPE_S, 1, 0xffff),
    DEFNUMITEML(bt_wait_no_reload, NUMTYPE_BOOL, 0, 1),
    DEFNUMITEML(bt_precap_tohit, NUMTYPE_BOOL, 0, 1),
    DEFNUMITEML(bt_no_tohit_acc, NUMTYPE_BOOL, 0, 1),
    DEFNUMITEML(bt_oracle_fix, NUMTYPE_BOOL, 0, 1),
    DEFNUMITEML(stargate_cost, NUMTYPE_S, 1, 0xffff),
    DEFNUMITEML(stargate_maint, NUMTYPE_S, 1, 0xffff),
    DEFNUMITEML(weapon_list_max, NUMTYPE_S, 0, WEAPON_NUM),
    DEFNUMITEML(limit_ships, NUMTYPE_S, 1, 0xffff),
    DEFNUMITEML(limit_ships_all, NUMTYPE_S, 1, 0xffff),
    DEFNUMITEML(max_pop, NUMTYPE_S, 1, 0x7fff),
    DEFNUMITEML(max_factories, NUMTYPE_S, 1, 0x7fff),
    DEFNUMITEML(max_inbound, NUMTYPE_S, 1, 0x7fff),
    DEFNUMITEML(atmos_cost, NUMTYPE_S, 1, 0x7fff),
    DEFNUMITEML(soil_cost, NUMTYPE_S, 1, 0x7fff),
    DEFNUMITEML(adv_soil_cost, NUMTYPE_S, 1, 0x7fff),
    DEFNUMITEML(adv_scan_range, NUMTYPE_S, 1, 0x7fff),
    DEFNUMITEML(pop_hp, NUMTYPE_S, 1, 0x7fff),
    DEFNUMITEML(fact_hp, NUMTYPE_S, 1, 0x7fff),
    DEFNUMITEML(max_bomb_dmg, NUMTYPE_S, 1, 0x7fffffff),
    DEFNUMITEML(max_bio_dmg, NUMTYPE_S, 1, 0x7fffffff),
    DEFNUMITEML(max_trans_dmg, NUMTYPE_S, 1, 0x7fffffff),
    DEFNUMITEML(max_ship_maint, NUMTYPE_S, 1, 0x7fffffff),
    DEFNUMITEML(max_tribute_bc, NUMTYPE_S, 1, 0x7fffffff),
    DEFNUMITEML(event_roll, NUMTYPE_S, 0, 0x7fffffff),
    DEFNUMITEML(accident_chk_factories, NUMTYPE_BOOL, 0, 1),
    DEFNUMITEML(council_years, NUMTYPE_S, 0, 0x7fff),
    DEFNUMITEML(news_orion, NUMTYPE_BOOL, 0, 1),
    DEFNUMITEML(aud_ask_break_nap, NUMTYPE_BOOL, 0, 1),
    DEFNUMITEML(aud_bounty_give, NUMTYPE_BOOL, 0, 1),
    DEFNUMITEML(aud_update_tech, NUMTYPE_BOOL, 0, 1),
    DEFNUMITEML(update_on_load, NUMTYPE_BOOL, 0, 1),
    DEFNUMITEML(newtech_adjust_fix, NUMTYPE_BOOL, 0, 1),
    DEFNUMITEML(monster_rest_att, NUMTYPE_BOOL, 0, 1),
    DEFNUMITEML(orbital_bio_fix, NUMTYPE_BOOL, 0, 1),
    DEFNUMITEML(orbital_weap_any, NUMTYPE_BOOL, 0, 1),
    DEFNUMITEML(orbital_weap_4, NUMTYPE_BOOL, 0, 1),
    DEFNUMITEML(orbital_torpedo, NUMTYPE_BOOL, 0, 1),
    DEFNUMITEML(orbital_comp_fix, NUMTYPE_BOOL, 0, 1),
    DEFNUMITEML(combat_trans_fix, NUMTYPE_BOOL, 0, 1),
    DEFNUMITEML(stargate_redir_fix, NUMTYPE_BOOL, 0, 1),
    DEFNUMITEML(trans_redir_fix, NUMTYPE_BOOL, 0, 1),
    DEFNUMITEML(retreat_redir_fix, NUMTYPE_BOOL, 0, 1),
    DEFNUMITEML(first_tech_rp_fix, NUMTYPE_BOOL, 0, 1),
    DEFNUMITEML(waste_calc_fix, NUMTYPE_BOOL, 0, 1),
    DEFNUMITEML(waste_adjust_fix, NUMTYPE_BOOL, 0, 1),
    DEFNUMITEML(slider_respects_locks, NUMTYPE_BOOL, 0, 1),
    DEFNUMITEML(pop_tenths_fix, NUMTYPE_BOOL, 0, 1),
    DEFNUMITEML(factory_cost_fix, NUMTYPE_BOOL, 0, 1),
    DEFNUMITEML(doom_stack_fix, NUMTYPE_BOOL, 0, 1),
    DEFNUMITEML(eco_slider_slack, NUMTYPE_U8, 0, 0xff),
    DEFNUMITEML(reset_tform_to_max, NUMTYPE_BOOL, 0, 1),
    DEFNUMITEML(soil_rounding_fix, NUMTYPE_BOOL, 0, 1),
    DEFNUMITEML(leaving_trans_fix, NUMTYPE_BOOL, 0, 1),
    DEFNUMITEML(hidden_child_labor_fix, NUMTYPE_BOOL, 0, 1),
    DEFNUMITEML(extended_reloc_range, NUMTYPE_BOOL, 0, 1),
    DEFNUMITEML(colonized_factories_fix, NUMTYPE_BOOL, 0, 1),
    DEFNUMITEML(cond_switch_to_ind_fix, NUMTYPE_BOOL, 0, 1),
    DEFNUMITEML(slider_eco_done_fix, NUMTYPE_BOOL, 0, 1),
    DEFNUMITEML(ship_scanner_fix, NUMTYPE_BOOL, 0, 1),
    DEFNUMITEML(ai_fleet_cheating_fix, NUMTYPE_BOOL, 0, 1),
    DEFNUMITEMLTBLL(tbl_hull_w, NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLTBLL(tech_costmuld, NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLTBLL(tech_costmula, NUMTYPE_U8, 0, 0xff),
    DEFNUMITEML(tech_costmuld2, NUMTYPE_S, 1, 0x7fffffff),
    DEFNUMITEML(tech_costmula2, NUMTYPE_S, 1, 0x7fffffff),
    DEFNUMITEML(tech_costmul, NUMTYPE_S, 1, 0x7fffffff),
    DEFNUMITEMLTBL(tcostm_human, game_num_tech_costmulr[RACE_HUMAN], NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLTBL(tcostm_mrrshan, game_num_tech_costmulr[RACE_MRRSHAN], NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLTBL(tcostm_silicoid, game_num_tech_costmulr[RACE_SILICOID], NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLTBL(tcostm_sakkra, game_num_tech_costmulr[RACE_SAKKRA], NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLTBL(tcostm_psilon, game_num_tech_costmulr[RACE_PSILON], NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLTBL(tcostm_alkari, game_num_tech_costmulr[RACE_ALKARI], NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLTBL(tcostm_klackon, game_num_tech_costmulr[RACE_KLACKON], NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLTBL(tcostm_bulrathi, game_num_tech_costmulr[RACE_BULRATHI], NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLTBL(tcostm_meklar, game_num_tech_costmulr[RACE_MEKLAR], NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLTBL(tcostm_darlok, game_num_tech_costmulr[RACE_DARLOK], NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLTBLL(base_hp, NUMTYPE_U16, 0, 0xffff),
    DEFNUMITEMLTBLL(pshield_cost, NUMTYPE_U16, 0, 0xffff),
    DEFNUMITEMLTBL(trait1_human, game_num_tbl_trait1[RACE_HUMAN], NUMTYPE_U8, 0, 5),
    DEFNUMITEMLTBL(trait1_mrrshan, game_num_tbl_trait1[RACE_MRRSHAN], NUMTYPE_U8, 0, 5),
    DEFNUMITEMLTBL(trait1_silicoid, game_num_tbl_trait1[RACE_SILICOID], NUMTYPE_U8, 0, 5),
    DEFNUMITEMLTBL(trait1_sakkra, game_num_tbl_trait1[RACE_SAKKRA], NUMTYPE_U8, 0, 5),
    DEFNUMITEMLTBL(trait1_psilon, game_num_tbl_trait1[RACE_PSILON], NUMTYPE_U8, 0, 5),
    DEFNUMITEMLTBL(trait1_alkari, game_num_tbl_trait1[RACE_ALKARI], NUMTYPE_U8, 0, 5),
    DEFNUMITEMLTBL(trait1_klackon, game_num_tbl_trait1[RACE_KLACKON], NUMTYPE_U8, 0, 5),
    DEFNUMITEMLTBL(trait1_bulrathi, game_num_tbl_trait1[RACE_BULRATHI], NUMTYPE_U8, 0, 5),
    DEFNUMITEMLTBL(trait1_meklar, game_num_tbl_trait1[RACE_MEKLAR], NUMTYPE_U8, 0, 5),
    DEFNUMITEMLTBL(trait1_darlok, game_num_tbl_trait1[RACE_DARLOK], NUMTYPE_U8, 0, 5),
    DEFNUMITEMLTBL(trait2_human, game_num_tbl_trait2[RACE_HUMAN], NUMTYPE_U8, 0, 5),
    DEFNUMITEMLTBL(trait2_mrrshan, game_num_tbl_trait2[RACE_MRRSHAN], NUMTYPE_U8, 0, 5),
    DEFNUMITEMLTBL(trait2_silicoid, game_num_tbl_trait2[RACE_SILICOID], NUMTYPE_U8, 0, 5),
    DEFNUMITEMLTBL(trait2_sakkra, game_num_tbl_trait2[RACE_SAKKRA], NUMTYPE_U8, 0, 5),
    DEFNUMITEMLTBL(trait2_psilon, game_num_tbl_trait2[RACE_PSILON], NUMTYPE_U8, 0, 5),
    DEFNUMITEMLTBL(trait2_alkari, game_num_tbl_trait2[RACE_ALKARI], NUMTYPE_U8, 0, 5),
    DEFNUMITEMLTBL(trait2_klackon, game_num_tbl_trait2[RACE_KLACKON], NUMTYPE_U8, 0, 5),
    DEFNUMITEMLTBL(trait2_bulrathi, game_num_tbl_trait2[RACE_BULRATHI], NUMTYPE_U8, 0, 5),
    DEFNUMITEMLTBL(trait2_meklar, game_num_tbl_trait2[RACE_MEKLAR], NUMTYPE_U8, 0, 5),
    DEFNUMITEMLTBL(trait2_darlok, game_num_tbl_trait2[RACE_DARLOK], NUMTYPE_U8, 0, 5),
    DEFNUMITEMLTBLL(tbl_tech_autoadj, NUMTYPE_U8, 0, 100),
    DEFNUMITEMLSTBL(startship_cost, tbl_startship, cost, NUMTYPE_U16, 0, 0xffff),
    DEFNUMITEMLSTBL(startship_hull, tbl_startship, hull, NUMTYPE_GUESS(ship_hull_t), 0, SHIP_HULL_NUM - 1),
    DEFNUMITEMLSTBL(startship_look, tbl_startship, look, NUMTYPE_U8, 0, NUM_SHIPLOOKS - 1),
    DEFNUMITEMLSTBL(startship_wpnt1, tbl_startship, wpnt[0], NUMTYPE_GUESS(weapon_t), 0, WEAPON_NUM - 1),
    DEFNUMITEMLSTBL(startship_wpnt2, tbl_startship, wpnt[1], NUMTYPE_GUESS(weapon_t), 0, WEAPON_NUM - 1),
    DEFNUMITEMLSTBL(startship_wpnt3, tbl_startship, wpnt[2], NUMTYPE_GUESS(weapon_t), 0, WEAPON_NUM - 1),
    DEFNUMITEMLSTBL(startship_wpnt4, tbl_startship, wpnt[3], NUMTYPE_GUESS(weapon_t), 0, WEAPON_NUM - 1),
    DEFNUMITEMLSTBL(startship_wpnn1, tbl_startship, wpnn[0], NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLSTBL(startship_wpnn2, tbl_startship, wpnn[1], NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLSTBL(startship_wpnn3, tbl_startship, wpnn[2], NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLSTBL(startship_wpnn4, tbl_startship, wpnn[3], NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLSTBL(startship_engine, tbl_startship, engine, NUMTYPE_GUESS(ship_engine_t), 0, SHIP_ENGINE_NUM - 1),
    DEFNUMITEMLSTBL(startship_engines, tbl_startship, engines, NUMTYPE_U32, 0, 0xffffffff),
    DEFNUMITEMLSTBL(startship_special1, tbl_startship, special[0], NUMTYPE_GUESS(ship_special_t), 0, SHIP_SPECIAL_NUM - 1),
    DEFNUMITEMLSTBL(startship_special2, tbl_startship, special[1], NUMTYPE_GUESS(ship_special_t), 0, SHIP_SPECIAL_NUM - 1),
    DEFNUMITEMLSTBL(startship_special3, tbl_startship, special[2], NUMTYPE_GUESS(ship_special_t), 0, SHIP_SPECIAL_NUM - 1),
    DEFNUMITEMLSTBL(startship_shield, tbl_startship, shield, NUMTYPE_GUESS(ship_shield_t), 0, SHIP_SHIELD_NUM - 1),
    DEFNUMITEMLSTBL(startship_jammer, tbl_startship, jammer, NUMTYPE_GUESS(ship_jammer_t), 0, SHIP_JAMMER_NUM - 1),
    DEFNUMITEMLSTBL(startship_comp, tbl_startship, comp, NUMTYPE_GUESS(ship_comp_t), 0, SHIP_COMP_NUM - 1),
    DEFNUMITEMLSTBL(startship_armor, tbl_startship, armor, NUMTYPE_GUESS(ship_armor_t), 0, SHIP_ARMOR_NUM - 1),
    DEFNUMITEMLSTBL(startship_man, tbl_startship, man, NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLSTBL(startship_hp, tbl_startship, hp, NUMTYPE_U16, 0, 0xffff),
    DEFNUMITEMLTBL(startship_num, startfleet_ships, NUMTYPE_U16, 0, 0xffff),
    DEFNUMITEMLSTBL(crystal_hull, tbl_monster[MONSTER_CRYSTAL], hull, NUMTYPE_GUESS(ship_hull_t), 0, SHIP_HULL_NUM - 1),
    DEFNUMITEMLSTBL(crystal_comp, tbl_monster[MONSTER_CRYSTAL], comp, NUMTYPE_GUESS(ship_comp_t), 0, SHIP_COMP_NUM - 1),
    DEFNUMITEMLSTBL(crystal_jammer, tbl_monster[MONSTER_CRYSTAL], jammer, NUMTYPE_GUESS(ship_jammer_t), 0, SHIP_JAMMER_NUM - 1),
    DEFNUMITEMLSTBL(crystal_shield, tbl_monster[MONSTER_CRYSTAL], shield, NUMTYPE_GUESS(ship_shield_t), 0, SHIP_SHIELD_NUM - 1),
    DEFNUMITEMLSTBL(crystal_armor, tbl_monster[MONSTER_CRYSTAL], armor, NUMTYPE_GUESS(ship_armor_t), 0, SHIP_ARMOR_NUM - 1),
    DEFNUMITEMLSTBL(crystal_engine, tbl_monster[MONSTER_CRYSTAL], engine, NUMTYPE_GUESS(ship_engine_t), 0, SHIP_ENGINE_NUM - 1),
    DEFNUMITEMLSTBL(crystal_special1, tbl_monster[MONSTER_CRYSTAL], special[0], NUMTYPE_GUESS(ship_special_t), 0, SHIP_SPECIAL_NUM - 1),
    DEFNUMITEMLSTBL(crystal_special2, tbl_monster[MONSTER_CRYSTAL], special[1], NUMTYPE_GUESS(ship_special_t), 0, SHIP_SPECIAL_NUM - 1),
    DEFNUMITEMLSTBL(crystal_special3, tbl_monster[MONSTER_CRYSTAL], special[2], NUMTYPE_GUESS(ship_special_t), 0, SHIP_SPECIAL_NUM - 1),
    DEFNUMITEMLSTBL(crystal_wpnt1, tbl_monster[MONSTER_CRYSTAL], wpnt[0], NUMTYPE_GUESS(weapon_t), 0, WEAPON_NUM - 1),
    DEFNUMITEMLSTBL(crystal_wpnt2, tbl_monster[MONSTER_CRYSTAL], wpnt[1], NUMTYPE_GUESS(weapon_t), 0, WEAPON_NUM - 1),
    DEFNUMITEMLSTBL(crystal_wpnt3, tbl_monster[MONSTER_CRYSTAL], wpnt[2], NUMTYPE_GUESS(weapon_t), 0, WEAPON_NUM - 1),
    DEFNUMITEMLSTBL(crystal_wpnt4, tbl_monster[MONSTER_CRYSTAL], wpnt[3], NUMTYPE_GUESS(weapon_t), 0, WEAPON_NUM - 1),
    DEFNUMITEMLSTBL(crystal_wpnn1, tbl_monster[MONSTER_CRYSTAL], wpnn[0], NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLSTBL(crystal_wpnn2, tbl_monster[MONSTER_CRYSTAL], wpnn[1], NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLSTBL(crystal_wpnn3, tbl_monster[MONSTER_CRYSTAL], wpnn[2], NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLSTBL(crystal_wpnn4, tbl_monster[MONSTER_CRYSTAL], wpnn[3], NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLSTBL(crystal_hp, tbl_monster[MONSTER_CRYSTAL], hp, NUMTYPE_U16, 0, 0xffff),
    DEFNUMITEMLSTBL(crystal_man, tbl_monster[MONSTER_CRYSTAL], man, NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLSTBL(crystal_complevel, tbl_monster[MONSTER_CRYSTAL], complevel, NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLSTBL(crystal_defense, tbl_monster[MONSTER_CRYSTAL], defense, NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLSTBL(crystal_misdefense, tbl_monster[MONSTER_CRYSTAL], misdefense, NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLSTBL(crystal_absorb, tbl_monster[MONSTER_CRYSTAL], absorb, NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLSTBL(crystal_repair, tbl_monster[MONSTER_CRYSTAL], repair, NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLSTBL(crystal_misshield, tbl_monster[MONSTER_CRYSTAL], misshield, NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLSTBL(crystal_extrarange, tbl_monster[MONSTER_CRYSTAL], extrarange, NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLSTBL(crystal_num, tbl_monster[MONSTER_CRYSTAL], num, NUMTYPE_U16, 0, 0xffff),
    DEFNUMITEMLSTBL(crystal_look, tbl_monster[MONSTER_CRYSTAL], look, NUMTYPE_U8, 0, NUM_SHIPLOOKS - 1),
    DEFNUMITEMLSTBL(crystal_pulsar, tbl_monster[MONSTER_CRYSTAL], pulsar, NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLSTBL(crystal_stream, tbl_monster[MONSTER_CRYSTAL], stream, NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLSTBL(crystal_sbmask, tbl_monster[MONSTER_CRYSTAL], sbmask, NUMTYPE_U16, 0, 0xffff),
    DEFNUMITEMLSTBL(amoeba_hull, tbl_monster[MONSTER_AMOEBA], hull, NUMTYPE_GUESS(ship_hull_t), 0, SHIP_HULL_NUM - 1),
    DEFNUMITEMLSTBL(amoeba_comp, tbl_monster[MONSTER_AMOEBA], comp, NUMTYPE_GUESS(ship_comp_t), 0, SHIP_COMP_NUM - 1),
    DEFNUMITEMLSTBL(amoeba_jammer, tbl_monster[MONSTER_AMOEBA], jammer, NUMTYPE_GUESS(ship_jammer_t), 0, SHIP_JAMMER_NUM - 1),
    DEFNUMITEMLSTBL(amoeba_shield, tbl_monster[MONSTER_AMOEBA], shield, NUMTYPE_GUESS(ship_shield_t), 0, SHIP_SHIELD_NUM - 1),
    DEFNUMITEMLSTBL(amoeba_armor, tbl_monster[MONSTER_AMOEBA], armor, NUMTYPE_GUESS(ship_armor_t), 0, SHIP_ARMOR_NUM - 1),
    DEFNUMITEMLSTBL(amoeba_engine, tbl_monster[MONSTER_AMOEBA], engine, NUMTYPE_GUESS(ship_engine_t), 0, SHIP_ENGINE_NUM - 1),
    DEFNUMITEMLSTBL(amoeba_special1, tbl_monster[MONSTER_AMOEBA], special[0], NUMTYPE_GUESS(ship_special_t), 0, SHIP_SPECIAL_NUM - 1),
    DEFNUMITEMLSTBL(amoeba_special2, tbl_monster[MONSTER_AMOEBA], special[1], NUMTYPE_GUESS(ship_special_t), 0, SHIP_SPECIAL_NUM - 1),
    DEFNUMITEMLSTBL(amoeba_special3, tbl_monster[MONSTER_AMOEBA], special[2], NUMTYPE_GUESS(ship_special_t), 0, SHIP_SPECIAL_NUM - 1),
    DEFNUMITEMLSTBL(amoeba_wpnt1, tbl_monster[MONSTER_AMOEBA], wpnt[0], NUMTYPE_GUESS(weapon_t), 0, WEAPON_NUM - 1),
    DEFNUMITEMLSTBL(amoeba_wpnt2, tbl_monster[MONSTER_AMOEBA], wpnt[1], NUMTYPE_GUESS(weapon_t), 0, WEAPON_NUM - 1),
    DEFNUMITEMLSTBL(amoeba_wpnt3, tbl_monster[MONSTER_AMOEBA], wpnt[2], NUMTYPE_GUESS(weapon_t), 0, WEAPON_NUM - 1),
    DEFNUMITEMLSTBL(amoeba_wpnt4, tbl_monster[MONSTER_AMOEBA], wpnt[3], NUMTYPE_GUESS(weapon_t), 0, WEAPON_NUM - 1),
    DEFNUMITEMLSTBL(amoeba_wpnn1, tbl_monster[MONSTER_AMOEBA], wpnn[0], NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLSTBL(amoeba_wpnn2, tbl_monster[MONSTER_AMOEBA], wpnn[1], NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLSTBL(amoeba_wpnn3, tbl_monster[MONSTER_AMOEBA], wpnn[2], NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLSTBL(amoeba_wpnn4, tbl_monster[MONSTER_AMOEBA], wpnn[3], NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLSTBL(amoeba_hp, tbl_monster[MONSTER_AMOEBA], hp, NUMTYPE_U16, 0, 0xffff),
    DEFNUMITEMLSTBL(amoeba_man, tbl_monster[MONSTER_AMOEBA], man, NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLSTBL(amoeba_complevel, tbl_monster[MONSTER_AMOEBA], complevel, NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLSTBL(amoeba_defense, tbl_monster[MONSTER_AMOEBA], defense, NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLSTBL(amoeba_misdefense, tbl_monster[MONSTER_AMOEBA], misdefense, NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLSTBL(amoeba_absorb, tbl_monster[MONSTER_AMOEBA], absorb, NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLSTBL(amoeba_repair, tbl_monster[MONSTER_AMOEBA], repair, NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLSTBL(amoeba_misshield, tbl_monster[MONSTER_AMOEBA], misshield, NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLSTBL(amoeba_extrarange, tbl_monster[MONSTER_AMOEBA], extrarange, NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLSTBL(amoeba_num, tbl_monster[MONSTER_AMOEBA], num, NUMTYPE_U16, 0, 0xffff),
    DEFNUMITEMLSTBL(amoeba_look, tbl_monster[MONSTER_AMOEBA], look, NUMTYPE_U8, 0, NUM_SHIPLOOKS - 1),
    DEFNUMITEMLSTBL(amoeba_pulsar, tbl_monster[MONSTER_AMOEBA], pulsar, NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLSTBL(amoeba_stream, tbl_monster[MONSTER_AMOEBA], stream, NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLSTBL(amoeba_sbmask, tbl_monster[MONSTER_AMOEBA], sbmask, NUMTYPE_U16, 0, 0xffff),
    DEFNUMITEMLSTBL(guardian_hull, tbl_monster[MONSTER_GUARDIAN], hull, NUMTYPE_GUESS(ship_hull_t), 0, SHIP_HULL_NUM - 1),
    DEFNUMITEMLSTBL(guardian_comp, tbl_monster[MONSTER_GUARDIAN], comp, NUMTYPE_GUESS(ship_comp_t), 0, SHIP_COMP_NUM - 1),
    DEFNUMITEMLSTBL(guardian_jammer, tbl_monster[MONSTER_GUARDIAN], jammer, NUMTYPE_GUESS(ship_jammer_t), 0, SHIP_JAMMER_NUM - 1),
    DEFNUMITEMLSTBL(guardian_shield, tbl_monster[MONSTER_GUARDIAN], shield, NUMTYPE_GUESS(ship_shield_t), 0, SHIP_SHIELD_NUM - 1),
    DEFNUMITEMLSTBL(guardian_armor, tbl_monster[MONSTER_GUARDIAN], armor, NUMTYPE_GUESS(ship_armor_t), 0, SHIP_ARMOR_NUM - 1),
    DEFNUMITEMLSTBL(guardian_engine, tbl_monster[MONSTER_GUARDIAN], engine, NUMTYPE_GUESS(ship_engine_t), 0, SHIP_ENGINE_NUM - 1),
    DEFNUMITEMLSTBL(guardian_special1, tbl_monster[MONSTER_GUARDIAN], special[0], NUMTYPE_GUESS(ship_special_t), 0, SHIP_SPECIAL_NUM - 1),
    DEFNUMITEMLSTBL(guardian_special2, tbl_monster[MONSTER_GUARDIAN], special[1], NUMTYPE_GUESS(ship_special_t), 0, SHIP_SPECIAL_NUM - 1),
    DEFNUMITEMLSTBL(guardian_special3, tbl_monster[MONSTER_GUARDIAN], special[2], NUMTYPE_GUESS(ship_special_t), 0, SHIP_SPECIAL_NUM - 1),
    DEFNUMITEMLSTBL(guardian_wpnt1, tbl_monster[MONSTER_GUARDIAN], wpnt[0], NUMTYPE_GUESS(weapon_t), 0, WEAPON_NUM - 1),
    DEFNUMITEMLSTBL(guardian_wpnt2, tbl_monster[MONSTER_GUARDIAN], wpnt[1], NUMTYPE_GUESS(weapon_t), 0, WEAPON_NUM - 1),
    DEFNUMITEMLSTBL(guardian_wpnt3, tbl_monster[MONSTER_GUARDIAN], wpnt[2], NUMTYPE_GUESS(weapon_t), 0, WEAPON_NUM - 1),
    DEFNUMITEMLSTBL(guardian_wpnt4, tbl_monster[MONSTER_GUARDIAN], wpnt[3], NUMTYPE_GUESS(weapon_t), 0, WEAPON_NUM - 1),
    DEFNUMITEMLSTBL(guardian_wpnn1, tbl_monster[MONSTER_GUARDIAN], wpnn[0], NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLSTBL(guardian_wpnn2, tbl_monster[MONSTER_GUARDIAN], wpnn[1], NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLSTBL(guardian_wpnn3, tbl_monster[MONSTER_GUARDIAN], wpnn[2], NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLSTBL(guardian_wpnn4, tbl_monster[MONSTER_GUARDIAN], wpnn[3], NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLSTBL(guardian_hp, tbl_monster[MONSTER_GUARDIAN], hp, NUMTYPE_U16, 0, 0xffff),
    DEFNUMITEMLSTBL(guardian_man, tbl_monster[MONSTER_GUARDIAN], man, NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLSTBL(guardian_complevel, tbl_monster[MONSTER_GUARDIAN], complevel, NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLSTBL(guardian_defense, tbl_monster[MONSTER_GUARDIAN], defense, NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLSTBL(guardian_misdefense, tbl_monster[MONSTER_GUARDIAN], misdefense, NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLSTBL(guardian_absorb, tbl_monster[MONSTER_GUARDIAN], absorb, NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLSTBL(guardian_repair, tbl_monster[MONSTER_GUARDIAN], repair, NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLSTBL(guardian_misshield, tbl_monster[MONSTER_GUARDIAN], misshield, NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLSTBL(guardian_extrarange, tbl_monster[MONSTER_GUARDIAN], extrarange, NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLSTBL(guardian_num, tbl_monster[MONSTER_GUARDIAN], num, NUMTYPE_U16, 0, 0xffff),
    DEFNUMITEMLSTBL(guardian_look, tbl_monster[MONSTER_GUARDIAN], look, NUMTYPE_U8, 0, NUM_SHIPLOOKS - 1),
    DEFNUMITEMLSTBL(guardian_pulsar, tbl_monster[MONSTER_GUARDIAN], pulsar, NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLSTBL(guardian_stream, tbl_monster[MONSTER_GUARDIAN], stream, NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLSTBL(guardian_sbmask, tbl_monster[MONSTER_GUARDIAN], sbmask, NUMTYPE_U16, 0, 0xffff),
    DEFNUMITEMLTBL(ngt_cm_human, game_num_ng_tech[RACE_HUMAN][TECH_FIELD_COMPUTER], NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLTBL(ngt_cm_mrrshan, game_num_ng_tech[RACE_MRRSHAN][TECH_FIELD_COMPUTER], NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLTBL(ngt_cm_silicoid, game_num_ng_tech[RACE_SILICOID][TECH_FIELD_COMPUTER], NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLTBL(ngt_cm_sakkra, game_num_ng_tech[RACE_SAKKRA][TECH_FIELD_COMPUTER], NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLTBL(ngt_cm_psilon, game_num_ng_tech[RACE_PSILON][TECH_FIELD_COMPUTER], NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLTBL(ngt_cm_alkari, game_num_ng_tech[RACE_ALKARI][TECH_FIELD_COMPUTER], NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLTBL(ngt_cm_klackon, game_num_ng_tech[RACE_KLACKON][TECH_FIELD_COMPUTER], NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLTBL(ngt_cm_bulrathi, game_num_ng_tech[RACE_BULRATHI][TECH_FIELD_COMPUTER], NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLTBL(ngt_cm_meklar, game_num_ng_tech[RACE_MEKLAR][TECH_FIELD_COMPUTER], NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLTBL(ngt_cm_darlok, game_num_ng_tech[RACE_DARLOK][TECH_FIELD_COMPUTER], NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLTBL(ngt_cn_human, game_num_ng_tech[RACE_HUMAN][TECH_FIELD_CONSTRUCTION], NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLTBL(ngt_cn_mrrshan, game_num_ng_tech[RACE_MRRSHAN][TECH_FIELD_CONSTRUCTION], NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLTBL(ngt_cn_silicoid, game_num_ng_tech[RACE_SILICOID][TECH_FIELD_CONSTRUCTION], NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLTBL(ngt_cn_sakkra, game_num_ng_tech[RACE_SAKKRA][TECH_FIELD_CONSTRUCTION], NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLTBL(ngt_cn_psilon, game_num_ng_tech[RACE_PSILON][TECH_FIELD_CONSTRUCTION], NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLTBL(ngt_cn_alkari, game_num_ng_tech[RACE_ALKARI][TECH_FIELD_CONSTRUCTION], NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLTBL(ngt_cn_klackon, game_num_ng_tech[RACE_KLACKON][TECH_FIELD_CONSTRUCTION], NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLTBL(ngt_cn_bulrathi, game_num_ng_tech[RACE_BULRATHI][TECH_FIELD_CONSTRUCTION], NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLTBL(ngt_cn_meklar, game_num_ng_tech[RACE_MEKLAR][TECH_FIELD_CONSTRUCTION], NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLTBL(ngt_cn_darlok, game_num_ng_tech[RACE_DARLOK][TECH_FIELD_CONSTRUCTION], NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLTBL(ngt_ff_human, game_num_ng_tech[RACE_HUMAN][TECH_FIELD_FORCE_FIELD], NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLTBL(ngt_ff_mrrshan, game_num_ng_tech[RACE_MRRSHAN][TECH_FIELD_FORCE_FIELD], NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLTBL(ngt_ff_silicoid, game_num_ng_tech[RACE_SILICOID][TECH_FIELD_FORCE_FIELD], NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLTBL(ngt_ff_sakkra, game_num_ng_tech[RACE_SAKKRA][TECH_FIELD_FORCE_FIELD], NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLTBL(ngt_ff_psilon, game_num_ng_tech[RACE_PSILON][TECH_FIELD_FORCE_FIELD], NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLTBL(ngt_ff_alkari, game_num_ng_tech[RACE_ALKARI][TECH_FIELD_FORCE_FIELD], NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLTBL(ngt_ff_klackon, game_num_ng_tech[RACE_KLACKON][TECH_FIELD_FORCE_FIELD], NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLTBL(ngt_ff_bulrathi, game_num_ng_tech[RACE_BULRATHI][TECH_FIELD_FORCE_FIELD], NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLTBL(ngt_ff_meklar, game_num_ng_tech[RACE_MEKLAR][TECH_FIELD_FORCE_FIELD], NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLTBL(ngt_ff_darlok, game_num_ng_tech[RACE_DARLOK][TECH_FIELD_FORCE_FIELD], NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLTBL(ngt_pl_human, game_num_ng_tech[RACE_HUMAN][TECH_FIELD_PLANETOLOGY], NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLTBL(ngt_pl_mrrshan, game_num_ng_tech[RACE_MRRSHAN][TECH_FIELD_PLANETOLOGY], NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLTBL(ngt_pl_silicoid, game_num_ng_tech[RACE_SILICOID][TECH_FIELD_PLANETOLOGY], NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLTBL(ngt_pl_sakkra, game_num_ng_tech[RACE_SAKKRA][TECH_FIELD_PLANETOLOGY], NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLTBL(ngt_pl_psilon, game_num_ng_tech[RACE_PSILON][TECH_FIELD_PLANETOLOGY], NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLTBL(ngt_pl_alkari, game_num_ng_tech[RACE_ALKARI][TECH_FIELD_PLANETOLOGY], NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLTBL(ngt_pl_klackon, game_num_ng_tech[RACE_KLACKON][TECH_FIELD_PLANETOLOGY], NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLTBL(ngt_pl_bulrathi, game_num_ng_tech[RACE_BULRATHI][TECH_FIELD_PLANETOLOGY], NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLTBL(ngt_pl_meklar, game_num_ng_tech[RACE_MEKLAR][TECH_FIELD_PLANETOLOGY], NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLTBL(ngt_pl_darlok, game_num_ng_tech[RACE_DARLOK][TECH_FIELD_PLANETOLOGY], NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLTBL(ngt_pr_human, game_num_ng_tech[RACE_HUMAN][TECH_FIELD_PROPULSION], NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLTBL(ngt_pr_mrrshan, game_num_ng_tech[RACE_MRRSHAN][TECH_FIELD_PROPULSION], NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLTBL(ngt_pr_silicoid, game_num_ng_tech[RACE_SILICOID][TECH_FIELD_PROPULSION], NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLTBL(ngt_pr_sakkra, game_num_ng_tech[RACE_SAKKRA][TECH_FIELD_PROPULSION], NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLTBL(ngt_pr_psilon, game_num_ng_tech[RACE_PSILON][TECH_FIELD_PROPULSION], NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLTBL(ngt_pr_alkari, game_num_ng_tech[RACE_ALKARI][TECH_FIELD_PROPULSION], NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLTBL(ngt_pr_klackon, game_num_ng_tech[RACE_KLACKON][TECH_FIELD_PROPULSION], NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLTBL(ngt_pr_bulrathi, game_num_ng_tech[RACE_BULRATHI][TECH_FIELD_PROPULSION], NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLTBL(ngt_pr_meklar, game_num_ng_tech[RACE_MEKLAR][TECH_FIELD_PROPULSION], NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLTBL(ngt_pr_darlok, game_num_ng_tech[RACE_DARLOK][TECH_FIELD_PROPULSION], NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLTBL(ngt_we_human, game_num_ng_tech[RACE_HUMAN][TECH_FIELD_WEAPON], NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLTBL(ngt_we_mrrshan, game_num_ng_tech[RACE_MRRSHAN][TECH_FIELD_WEAPON], NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLTBL(ngt_we_silicoid, game_num_ng_tech[RACE_SILICOID][TECH_FIELD_WEAPON], NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLTBL(ngt_we_sakkra, game_num_ng_tech[RACE_SAKKRA][TECH_FIELD_WEAPON], NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLTBL(ngt_we_psilon, game_num_ng_tech[RACE_PSILON][TECH_FIELD_WEAPON], NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLTBL(ngt_we_alkari, game_num_ng_tech[RACE_ALKARI][TECH_FIELD_WEAPON], NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLTBL(ngt_we_klackon, game_num_ng_tech[RACE_KLACKON][TECH_FIELD_WEAPON], NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLTBL(ngt_we_bulrathi, game_num_ng_tech[RACE_BULRATHI][TECH_FIELD_WEAPON], NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLTBL(ngt_we_meklar, game_num_ng_tech[RACE_MEKLAR][TECH_FIELD_WEAPON], NUMTYPE_U8, 0, 0xff),
    DEFNUMITEMLTBL(ngt_we_darlok, game_num_ng_tech[RACE_DARLOK][TECH_FIELD_WEAPON], NUMTYPE_U8, 0, 0xff),
    DEFNUMEND
};

/* -------------------------------------------------------------------------- */

static const struct numtbl_s *find_match(const char *numid, int i, int num)
{
    const struct numtbl_s *s = &game_num_id_tbl[0];
    while (s->numid) {
        if (strcmp(s->numid, numid) == 0) {
            if ((num > 0) && ((i + num) <= s->size)) {
                return s;
            } else {
                log_error("NUM: numid '%s' index %i+%i=%i > size %i\n", numid, i, num, i + num, s->size);
                return NULL;
            }
        }
        ++s;
    }
    log_error("NUM: unknown numid '%s'\n", numid);
    return NULL;
}

/* -------------------------------------------------------------------------- */

bool game_opt_fix_bugs = false;
bool game_opt_fix_starting_ships = false;

void game_num_fix_bugs(void)
{
    game_num_ng_tech[RACE_SILICOID][TECH_FIELD_PLANETOLOGY][24] = 1;
    game_num_ng_tech[RACE_SILICOID][TECH_FIELD_PLANETOLOGY][30] = 1;
    game_num_accident_chk_factories = true;
    game_num_weapon_list_max = 64;
    game_num_aud_bounty_give = true;
    game_num_aud_update_tech = true;
    game_num_update_on_load = false;
    game_num_combat_trans_fix = true;
    game_num_eco_slider_slack = 0;
    game_num_reset_tform_to_max = false;
}

void game_num_fix_bugs_1_3a(void)
{
    game_num_ai_4_colony_curse_fix = true;
    game_num_max_factories = 2700;
    game_num_bt_oracle_fix = true;
    game_num_orbital_weap_any = true;
    game_num_orbital_torpedo = true;
    game_num_ship_scanner_fix = true;

    tbl_monster[MONSTER_GUARDIAN][3].special[1] = 16;
    tbl_monster[MONSTER_GUARDIAN][4].special[1] = 26;
    tbl_monster[MONSTER_GUARDIAN][4].repair = 30;
}

void game_num_fix_starting_ships(void)
{
    tbl_startship[0].engines = 2;
    tbl_startship[1].engines = 27;
    tbl_startship[2].engines = 110;
    tbl_startship[3].engines = 85;
    tbl_startship[4].engines = 100;
    tbl_startship[0].cost = 8;
    tbl_startship[1].cost = 14;
    tbl_startship[2].cost = 74;
    tbl_startship[3].cost = 65;
    tbl_startship[4].cost = 570;
}

bool game_num_patch(const char *numid, const int32_t *patchnums, int first, int num)
{
    const struct numtbl_s *s = find_match(numid, first, num);
    if (s) {
        void *p;
        bool is_signed;
        p = ((uint8_t *)s->ptr) + first * s->tstep;
        is_signed = NUMTYPE_IS_S(s->numtype);
        for (int i = 0; i < num; ++i) {
            int32_t v;
            v = patchnums[i];
            if (0
              || ((is_signed) && ((v < s->vmin) || (v > s->vmax)))
              || ((!is_signed) && ((((uint32_t)v) < s->vmin) || (((uint32_t)v) > s->vmax)))
            ) {
                log_error("NUM: numid '%s' %ssigned value %i (%u) outside range %i..%u\n", numid, is_signed ? "" : "un", v, (uint32_t)v, s->vmin, s->vmax);
                return false;
            }
            switch (s->numtype) {
                default:
                case NUMTYPE_S: *((int *)p) = v; break;
                case NUMTYPE_U: *((unsigned int *)p) = (unsigned int)v; break;
                case NUMTYPE_S8: *((int8_t *)p) = (int8_t)v; break;
                case NUMTYPE_U8: *((uint8_t *)p) = (uint8_t)v; break;
                case NUMTYPE_S16: *((int16_t *)p) = (int16_t)v; break;
                case NUMTYPE_U16: *((uint16_t *)p) = (uint16_t)v; break;
                case NUMTYPE_S32: *((int32_t *)p) = v; break;
                case NUMTYPE_U32: *((uint32_t *)p) = (uint32_t)v; break;
                case NUMTYPE_BOOL: *((bool *)p) = (v != 0); break;
            }
            p = ((uint8_t *)p) + s->tstep;
        }
        return true;
    } else {
        return false;
    }
}

void game_num_dump(void)
{
    const struct numtbl_s *s = &game_num_id_tbl[0];
    int prevmin = 1, prevmax = 0;
    log_message("# dump of all patchable game numbers\n");
    while (s->numid) {
        void *p;
        p = s->ptr;
        if ((s->vmin != prevmin) || (s->vmax != prevmax)) {
            prevmin = s->vmin;
            prevmax = s->vmax;
            log_message("# %i..%u\n", prevmin, prevmax);
        }
        log_message("4,%s,0", s->numid);
        for (int i = 0; i < s->size; ++i) {
            int vs = 0;
            unsigned int vu = 0;
            switch (s->numtype) {
                default:
                case NUMTYPE_S: vs = *((int *)p); break;
                case NUMTYPE_U: vu = *((unsigned int *)p); break;
                case NUMTYPE_S8: vs = *((int8_t *)p); break;
                case NUMTYPE_U8: vu = *((uint8_t *)p); break;
                case NUMTYPE_S16: vs = *((int16_t *)p); break;
                case NUMTYPE_U16: vu = *((uint16_t *)p); break;
                case NUMTYPE_S32: vs = *((int32_t *)p); break;
                case NUMTYPE_U32: vu = *((uint32_t *)p); break;
                case NUMTYPE_BOOL: vs = *((bool *)p); break;
            }
            if (NUMTYPE_IS_S(s->numtype)) {
                log_message(",%i", vs);
            } else {
                log_message(",%u", vu);
            }
            p = ((uint8_t *)p) + s->tstep;
        }
        log_message("\n");
        ++s;
    }
}
