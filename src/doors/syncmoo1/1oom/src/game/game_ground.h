#ifndef INC_1OOM_GAME_GROUND_H
#define INC_1OOM_GAME_GROUND_H

#include "game_types.h"
#include "types.h"

#define GAME_GROUND_STRBUF_SIZE 64

struct game_s;

struct ground_side_s {
    player_id_t player;
    bool human;
    int force;
    int pop1;
    int pop2;
    uint8_t armori, suiti, shieldi, weapi;
    int strnum;
    char str[3][GAME_GROUND_STRBUF_SIZE];
};

struct ground_s {
    uint32_t seed;
    uint8_t planet_i;
    bool flag_swap;
    bool flag_rebel;
    int inbound;
    int total_inbound;
    int death;  /* 0, 1, -1 */
    int fact;
    int techchance;
    struct {
        uint8_t tech;
        tech_field_t field;
    } got[TECH_SPY_MAX];
    struct ground_side_s s[2];
};

extern void game_ground_kill(struct ground_s *gr);
extern const uint8_t *game_turn_ground_resolve_all(struct game_s *g);
extern int game_turn_ground_show_all(struct game_s *g, const uint8_t *buf);

#endif
