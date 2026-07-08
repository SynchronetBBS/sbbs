#ifndef INC_1OOM_UIBATTLEPRE_H
#define INC_1OOM_UIBATTLEPRE_H

#include "ui.h"
#include "types.h"

struct game_s;
struct battle_s;

extern ui_battle_autoresolve_t ui_battle_pre(struct game_s *g, const struct battle_s *bt, bool hide_other, int winner);

#endif
