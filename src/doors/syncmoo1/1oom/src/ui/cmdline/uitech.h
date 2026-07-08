#ifndef INC_1OOM_UITECH_H
#define INC_1OOM_UITECH_H

#include "types.h"

struct game_s;
struct input_token_s;

extern int ui_cmd_tech_look(struct game_s *g, int api, struct input_token_s *param, int num_param, void *var);
extern int ui_cmd_tech_slider(struct game_s *g, int api, struct input_token_s *param, int num_param, void *var);
extern int ui_cmd_tech_slider_lock(struct game_s *g, int api, struct input_token_s *param, int num_param, void *var);
extern int ui_cmd_tech_equals(struct game_s *g, int api, struct input_token_s *param, int num_param, void *var);

#endif
