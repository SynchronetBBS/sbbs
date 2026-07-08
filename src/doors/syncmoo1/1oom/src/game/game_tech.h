#ifndef INC_1OOM_GAME_TECH_H
#define INC_1OOM_GAME_TECH_H

#include "game_types.h"
#include "types.h"

struct game_s;
struct game_aux_s;
struct empiretechorbit_s;
struct newtech_s;

extern const uint8_t tech_reduce_50percent_per_10pts[51];
extern const uint8_t tech_reduce_25percent_per_10pts[51];

extern uint8_t game_tech_player_has_tech(const struct game_s *g, int field_i, int tech_i, int player_i);
extern uint8_t game_tech_player_best_tech(const struct game_s *g, int field_i, int tech_i_base, int tech_i_step, int tech_i_max, int player_i);
extern uint16_t game_get_base_cost(const struct game_s *g, int player_i);
extern uint8_t game_get_base_weapon(const struct game_s *g, player_id_t player_i, int tech_i);
extern uint8_t game_get_base_weapon_2(const struct game_s *g, player_id_t player_i, int tech_i, uint8_t weap1);
extern uint8_t game_get_best_shield(struct game_s *g, player_id_t player_i, int tech_i);
extern uint8_t game_get_best_comp(struct game_s *g, player_id_t player_i, int tech_i);
extern uint8_t game_get_best_jammer(const struct game_s *g, player_id_t player_i, int tech_i);
extern void game_update_tech_util(struct game_s *g);
extern uint8_t game_tech_get_group(const struct game_aux_s *gaux, tech_field_t field, int tech);
extern uint8_t game_tech_get_tier(const struct game_aux_s *gaux, tech_field_t field, int tech);
extern uint8_t game_tech_get_gfx_i(const struct game_aux_s *gaux, tech_field_t field, int tech);
extern const char *game_tech_get_name(const struct game_aux_s *gaux, tech_field_t field, int tech, char *buf, size_t bufsize);
extern const char *game_tech_get_descr(const struct game_aux_s *gaux, tech_field_t field, int tech, char *buf, size_t bufsize);
extern const char *game_tech_get_newtech_msg(const struct game_s *g, player_id_t pi, struct newtech_s *nt, char *buf, size_t bufsize);
extern int game_tech_current_research_percent1(const struct game_s *g, player_id_t player, tech_field_t field);
extern int game_tech_current_research_percent2(const struct game_s *g, player_id_t player, tech_field_t field);
extern bool game_tech_current_research_has_max_bonus(const struct game_s *g, player_id_t player, tech_field_t field);
extern void game_tech_set_to_max_bonus(struct game_s *g, player_id_t player, tech_field_t field);
extern void game_tech_get_new(struct game_s *g, player_id_t player, tech_field_t field, uint8_t tech, techsource_t source, int a8, player_id_t stolen_from, bool flag_frame);
extern void game_tech_finish_new(struct game_s *g, player_id_t player);
extern bool game_tech_can_choose(const struct game_s *g, player_id_t player, tech_field_t field);
extern uint32_t game_tech_get_next_rp(const struct game_s *g, player_id_t player, tech_field_t field, uint8_t tech);
extern void game_tech_start_next(struct game_s *g, player_id_t player, tech_field_t field, uint8_t tech);
extern int game_tech_get_field_percent(const struct game_s *g, player_id_t player, tech_field_t field);
extern int game_tech_get_field_cost(const struct game_s *g, player_id_t player, tech_field_t field);
extern void game_tech_research(struct game_s *g);
extern void game_tech_get_orion_loot(struct game_s *g, player_id_t player);
extern void game_tech_get_artifact_loot(struct game_s *g, uint8_t planet, player_id_t player);
extern void game_tech_final_war_share(struct game_s *g);

#endif
