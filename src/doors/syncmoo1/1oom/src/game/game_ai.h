#ifndef INC_1OOM_GAME_AI_H
#define INC_1OOM_GAME_AI_H

#include "game_types.h"
#include "types.h"

struct game_s;
struct battle_s;
struct election_s;
struct audience_s;

typedef enum {
    GAME_AI_CLASSIC = 0,
    GAME_AI_CLASSICPLUS, /*1*/
    GAME_AI_NUM
} game_ai_id_t;

struct game_ai_s {
    game_ai_id_t id;
    char const * const name;
    void (*new_game_init)(struct game_s *g, player_id_t player, uint8_t home);
    void (*new_game_tech)(struct game_s *g);
    void (*turn_p1)(struct game_s *g);
    void (*turn_p2)(struct game_s *g);
    void (*turn_p3)(struct game_s *g);
    bool (*battle_ai_ai_resolve)(struct battle_s *bt); /* true if right side won */
    void (*battle_ai_turn)(struct battle_s *bt);
    bool (*battle_ai_retreat)(struct battle_s *bt); /* true if retreat all */
    uint8_t (*tech_next)(struct game_s *g, player_id_t player, tech_field_t field, uint8_t *tbl, int num);
    bool (*bomb)(struct game_s *g, player_id_t player, uint8_t planet, int pop_inbound);
    void (*ground)(struct game_s *g, player_id_t def, player_id_t att, uint8_t planet, int pop_killed, bool owner_changed);
    void (*plague)(struct game_s *g, uint8_t planet);
    void (*nova)(struct game_s *g, uint8_t planet);
    void (*comet)(struct game_s *g, uint8_t planet);
    void (*pirates)(struct game_s *g, uint8_t planet);
    int (*vote)(struct election_s *el, player_id_t player); /* 0 = abstain, N = candidate N */
    void (*turn_diplo_p1)(struct game_s *g);
    void (*turn_diplo_p2)(struct game_s *g);
    void (*aud_start_human)(struct audience_s *au);
    int (*aud_treaty_nap)(struct audience_s *au);
    int (*aud_treaty_alliance)(struct audience_s *au);
    int (*aud_treaty_peace)(struct audience_s *au);
    int (*aud_treaty_declare_war)(struct audience_s *au);
    int (*aud_treaty_break_alliance)(struct audience_s *au);
    int (*aud_trade)(struct audience_s *au);
    bool (*aud_sweeten)(struct audience_s *au, int *bcptr, tech_field_t *fieldptr, uint8_t *techptr);
    uint8_t (*aud_threaten)(struct audience_s *au);
    void (*aud_tribute_bc)(struct audience_s *au, int selected, int bc);
    void (*aud_tribute_tech)(struct audience_s *au, int selected, tech_field_t field, uint8_t tech);
    int (*aud_tech_scale)(struct audience_s *au);
    uint8_t (*aud_get_dtype)(struct audience_s *au, uint8_t dtype, int a2);
    bool (*aud_later)(struct audience_s *au);
};

extern const struct game_ai_s *game_ai;
extern const struct game_ai_s *const game_ais[GAME_AI_NUM];

#endif
