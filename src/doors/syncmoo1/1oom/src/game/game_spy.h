#ifndef INC_1OOM_GAME_SPY_H
#define INC_1OOM_GAME_SPY_H

#include "game_types.h"

struct spy_turn_s {
    int tbl_rmax[PLAYER_NUM][PLAYER_NUM];   /* [target][spy] */
};

struct spy_esp_s {
    player_id_t target;
    player_id_t spy;
    int tnum;
    int tbl_num[TECH_FIELD_NUM];
    uint8_t tbl_techi[TECH_FIELD_NUM][50];
    tech_field_t tbl_field[TECH_SPY_MAX];
    uint8_t tbl_tech2[TECH_SPY_MAX];
    int tbl_value[TECH_SPY_MAX];
};

struct game_s;

extern int game_spy_esp_sub1(struct game_s *g, struct spy_esp_s *s, int a4, int a6);
extern int game_spy_esp_sub2(struct game_s *g, struct spy_esp_s *s, int a4);
extern void game_spy_build(struct game_s *g);
extern void game_spy_report(struct game_s *g);
extern void game_spy_turn(struct game_s *g, struct spy_turn_s *st);
extern void game_spy_esp_human(struct game_s *g, struct spy_turn_s *st);
extern void game_spy_sab_human(struct game_s *g);

#endif
