#ifndef INC_1OOM_GAME_EVENT_H
#define INC_1OOM_GAME_EVENT_H

#include "types.h"

struct game_s;
struct game_end_s;

extern void game_event_new(struct game_s *g);
extern bool game_event_run(struct game_s *g, struct game_end_s *ge);

extern bool game_opt_skip_random_news;

#endif
