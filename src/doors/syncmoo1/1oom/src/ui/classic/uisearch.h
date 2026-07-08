#ifndef INC_1OOM_UISEARCH_H
#define INC_1OOM_UISEARCH_H

#include "game_types.h"

struct game_s;

extern int ui_search(struct game_s *g, player_id_t pi);
extern bool ui_search_set_pos(struct game_s *g, player_id_t pi);

#endif
