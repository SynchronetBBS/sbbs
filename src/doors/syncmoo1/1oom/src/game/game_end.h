#ifndef INC_1OOM_GAME_END_H
#define INC_1OOM_GAME_END_H

#include "game_types.h"

struct game_end_s {
    game_end_type_t type;
    int race; /* or banner_live */
    int banner_dead;
    const char *name;
    int varnum;
};

#endif
