#ifndef INC_1OOM_UIRACES_H
#define INC_1OOM_UIRACES_H

#include "game_types.h"
#include "types.h"

struct game_s;
struct input_token_s;

extern player_id_t ui_player_from_param(const struct game_s *g, const struct input_token_s *param);

extern int ui_cmd_audience(struct game_s *g, int api, struct input_token_s *param, int num_param, void *var);
extern int ui_cmd_races(struct game_s *g, int api, struct input_token_s *param, int num_param, void *var);
extern int ui_cmd_spy(struct game_s *g, int api, struct input_token_s *param, int num_param, void *var);

#endif
