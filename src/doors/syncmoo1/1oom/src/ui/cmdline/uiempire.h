#ifndef INC_1OOM_UIEMPIRE_H
#define INC_1OOM_UIEMPIRE_H

#include "types.h"

struct game_s;
struct input_token_s;

extern int ui_cmd_empire_look(struct game_s *g, int api, struct input_token_s *param, int num_param, void *var);
extern int ui_cmd_empire_tax(struct game_s *g, int api, struct input_token_s *param, int num_param, void *var);
extern int ui_cmd_empire_security(struct game_s *g, int api, struct input_token_s *param, int num_param, void *var);

#endif
