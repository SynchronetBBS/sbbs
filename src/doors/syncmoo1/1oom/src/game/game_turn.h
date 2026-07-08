#ifndef INC_1OOM_GAME_TURN_H
#define INC_1OOM_GAME_TURN_H

#include "game_end.h"
#include "types.h"

struct game_s;

extern struct game_end_s game_turn_process(struct game_s *g);

extern int copyprot_status;

#endif
