#ifndef INC_1OOM_UIFLEET_H
#define INC_1OOM_UIFLEET_H

#include "types.h"

struct game_s;
struct fleet_enroute_s;
struct transport_s;
struct input_token_s;

extern void ui_fleet_print_fleets_orbit(const struct game_s *g, int api, uint8_t planet_i, bool show_my, bool show_opp);
extern void ui_fleet_print_fleet_enroute(const struct game_s *g, int api, const struct fleet_enroute_s *r, uint8_t pon);
extern void ui_fleet_print_transport_enroute(const struct game_s *g, int api, const struct transport_s *r);

extern int ui_cmd_fleet_list(struct game_s *g, int api, struct input_token_s *param, int num_param, void *var);
extern int ui_cmd_fleet_redir(struct game_s *g, int api, struct input_token_s *param, int num_param, void *var);
extern int ui_cmd_fleet_send(struct game_s *g, int api, struct input_token_s *param, int num_param, void *var);
extern int ui_cmd_fleet_scrap(struct game_s *g, int api, struct input_token_s *param, int num_param, void *var);

#endif
