#include "config.h"

#include "uicheat.h"
#include "game.h"
#include "game_cheat.h"

/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */

int ui_cmd_cheat_galaxy(struct game_s *g, int api, struct input_token_s *param, int num_param, void *var)
{
    return game_cheat_galaxy(g, api) ? 0 : -1;
}

int ui_cmd_cheat_events(struct game_s *g, int api, struct input_token_s *param, int num_param, void *var)
{
    return game_cheat_events(g, api) ? 0 : -1;
}

int ui_cmd_cheat_moola(struct game_s *g, int api, struct input_token_s *param, int num_param, void *var)
{
    return game_cheat_moola(g, api) ? 0 : -1;
}
