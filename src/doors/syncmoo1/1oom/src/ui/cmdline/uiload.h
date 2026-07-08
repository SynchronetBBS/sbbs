#ifndef INC_1OOM_UILOAD_H
#define INC_1OOM_UILOAD_H

struct input_token_s;

/* returns -1 on cancel or 0..7 on load game */
extern int ui_load_game(void);
extern int ui_cmd_load(struct game_s *g, int api, struct input_token_s *param, int num_param, void *var);

#endif
