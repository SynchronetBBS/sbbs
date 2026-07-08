#ifndef INC_1OOM_UIDESIGN_H
#define INC_1OOM_UIDESIGN_H

#include "game_types.h"
#include "types.h"

struct game_s;
struct game_design_s;

extern bool ui_design(struct game_s *g, struct game_design_s *gd, player_id_t pi);

#endif
