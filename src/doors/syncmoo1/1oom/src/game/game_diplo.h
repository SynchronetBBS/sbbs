#ifndef INC_1OOM_GAME_DIPLO_H
#define INC_1OOM_GAME_DIPLO_H

#include "game_types.h"
#include "types.h"

struct game_s;

extern const int16_t game_diplo_tbl_reldiff[6];

extern void game_diplo_act(struct game_s *g, int dv, player_id_t pi, player_id_t pi2, int dtype, uint8_t pli1, int16_t dp2);
extern void game_diplo_break_treaty(struct game_s *g, player_id_t breaker, player_id_t victim);
extern void game_diplo_start_war(struct game_s *g, player_id_t pi1, player_id_t pi2);
extern void game_diplo_start_war_swap(struct game_s *g, player_id_t pi1, player_id_t pi2);
extern void game_diplo_break_trade(struct game_s *g, player_id_t pi/*breaker*/, player_id_t pi2);
extern void game_diplo_annoy(struct game_s *g, player_id_t pi1, player_id_t pi2, int n);
extern void game_diplo_battle_finish(struct game_s *g, int def, int att, int popdiff, uint32_t app_def, uint16_t biodamage, uint32_t app_att, uint8_t planet_i);
extern void game_diplo_set_treaty(struct game_s *g, player_id_t pi1, player_id_t pi2, treaty_t treaty);
extern void game_diplo_set_trade(struct game_s *g, player_id_t pi1, player_id_t pi2, int bc);
extern void game_diplo_stop_war(struct game_s *g, player_id_t pi1, player_id_t pi2);
extern void game_diplo_esp_frame(struct game_s *g, player_id_t framed, player_id_t victim);
extern void game_diplo_limit_mood_treaty(struct game_s *g);
extern void game_diplo_mood_relax(struct game_s *g);
extern int16_t game_diplo_get_mood(struct game_s *g, player_id_t p1, player_id_t p2);
extern bool game_diplo_is_gone(struct game_s *g, player_id_t api, player_id_t pi);

#endif
