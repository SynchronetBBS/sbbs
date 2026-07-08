#ifndef INC_1OOM_UICMDS_H
#define INC_1OOM_UICMDS_H

#include "uidefs.h"

struct game_s;
struct input_token_s;

extern int ui_cmd_dummy_ret(struct game_s *g, int api, struct input_token_s *param, int num_param, void *var);
extern int ui_cmd_quit(struct game_s *g, int api, struct input_token_s *param, int num_param, void *var);

extern const struct input_cmd_s ui_cmds_opts[];

#endif