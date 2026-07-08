#ifndef INC_1OOM_UIPLANET_H
#define INC_1OOM_UIPLANET_H

#include "types.h"

struct game_s;
struct input_token_s;

extern const char *ui_planet_str(const struct game_s *g, int api, uint8_t planet_i, char *buf, size_t bufsize);
extern uint8_t ui_planet_from_param(struct game_s *g, int api, struct input_token_s *param);
extern void ui_planet_look(const struct game_s *g, int api, uint8_t planet_i, bool show_full);

extern int ui_cmd_planet_look(struct game_s *g, int api, struct input_token_s *param, int num_param, void *var);
extern int ui_cmd_planet_go(struct game_s *g, int api, struct input_token_s *param, int num_param, void *var);
extern int ui_cmd_planet_slider(struct game_s *g, int api, struct input_token_s *param, int num_param, void *var);
extern int ui_cmd_planet_slider_lock(struct game_s *g, int api, struct input_token_s *param, int num_param, void *var);
extern int ui_cmd_planet_build(struct game_s *g, int api, struct input_token_s *param, int num_param, void *var);
extern int ui_cmd_planet_reloc(struct game_s *g, int api, struct input_token_s *param, int num_param, void *var);
extern int ui_cmd_planet_trans(struct game_s *g, int api, struct input_token_s *param, int num_param, void *var);
extern int ui_cmd_planet_reserve(struct game_s *g, int api, struct input_token_s *param, int num_param, void *var);
extern int ui_cmd_planet_scrap_bases(struct game_s *g, int api, struct input_token_s *param, int num_param, void *var);

#endif
