#include "config.h"

#include "game_ai.h"
#include "game_ai_classic.h"
#include "game.h"
#include "game_aux.h"

/* -------------------------------------------------------------------------- */

const struct game_ai_s *game_ai = &game_ai_classic;

const struct game_ai_s *const game_ais[GAME_AI_NUM] = {
    &game_ai_classic,
    &game_ai_classicplus
};
