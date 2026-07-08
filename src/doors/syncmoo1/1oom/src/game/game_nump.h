#ifndef INC_1OOM_GAME_NUMP_H
#define INC_1OOM_GAME_NUMP_H

#include "types.h"

extern bool game_opt_fix_bugs;
extern bool game_opt_fix_starting_ships;

extern void game_num_fix_bugs(void);
extern void game_num_fix_bugs_1_3a(void);
extern void game_num_fix_starting_ships(void);
extern void game_num_dump(void);

#endif
