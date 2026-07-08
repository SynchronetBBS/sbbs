#ifndef INC_1OOM_UILANDING_H
#define INC_1OOM_UILANDING_H

#include "game_types.h"
#include "types.h"

struct game_s;

struct landing_data_s {
    struct game_s *g;
    player_id_t api;
    uint8_t planet;
    int frame;
    int music_i;
    bool colonize;
    uint8_t *gfx_transprt;
    uint8_t *gfx_walk;
};

extern void ui_landing_prepare(struct landing_data_s *d);
extern void ui_landing_free_data(struct landing_data_s *d);

#endif
