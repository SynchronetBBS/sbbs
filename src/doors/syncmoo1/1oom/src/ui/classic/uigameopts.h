#ifndef INC_1OOM_UIGAMEOPTS_H
#define INC_1OOM_UIGAMEOPTS_H

struct game_s;

typedef enum {
    GAMEOPTS_DONE,
    GAMEOPTS_QUIT,
    GAMEOPTS_LOAD
} gameopts_act_t;

extern gameopts_act_t ui_gameopts(struct game_s *g, int *load_game_i_ptr);

#endif
