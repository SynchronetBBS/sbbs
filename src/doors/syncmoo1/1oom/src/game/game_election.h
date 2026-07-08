#ifndef INC_1OOM_GAME_ELECTION_H
#define INC_1OOM_GAME_ELECTION_H

#include "game_types.h"

struct game_s;

struct election_s {
    struct game_s *g;
    void *uictx;
    char *buf;
    const char *str;
    int num;
    player_id_t tbl_ei[PLAYER_NUM];
    uint8_t tbl_votes[PLAYER_NUM];
    player_id_t candidate[2];
    uint16_t total_votes;
    uint16_t got_votes[2];
    player_id_t first_human;
    player_id_t last_human;
    int cur_i;
    bool flag_show_votes;
    int ui_delay;
};

extern const char *game_election_print_votes(uint16_t n, char *buf, size_t bufsize);
extern void game_election(struct game_s *g);

#endif
