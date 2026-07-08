#ifndef INC_1OOM_UICHEAT_H
#define INC_1OOM_UICHEAT_H

#include "types.h"

struct game_s;
struct input_token_s;

extern int ui_cmd_cheat_galaxy(struct game_s *g, int api, struct input_token_s *param, int num_param, void *var);
extern int ui_cmd_cheat_events(struct game_s *g, int api, struct input_token_s *param, int num_param, void *var);
extern int ui_cmd_cheat_moola(struct game_s *g, int api, struct input_token_s *param, int num_param, void *var);

#endif
