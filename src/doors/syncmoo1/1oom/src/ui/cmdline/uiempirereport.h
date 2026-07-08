#ifndef INC_1OOM_UIEMPIREREPORT_H
#define INC_1OOM_UIEMPIREREPORT_H

#include "types.h"

struct game_s;
struct input_token_s;

extern int ui_cmd_empirereport(struct game_s *g, int api, struct input_token_s *param, int num_param, void *var);

#endif
