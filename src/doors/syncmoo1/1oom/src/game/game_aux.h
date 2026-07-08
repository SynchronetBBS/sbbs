#ifndef INC_1OOM_GAME_AUX_H
#define INC_1OOM_GAME_AUX_H

#include "game.h"
#include "game_planet.h"
#include "types.h"

#define NUM_SHIPLOOKS   0x93

#define DIPLOMAT_D0_NUM 0x51
#define DIPLOMAT_MSG_NUM    1215
#define DIPLOMAT_MSG_LEN    0xc8

#define DIPLOMAT_MSG_PTR(ga_, r_, t_)   (&((ga_)->diplomat.msg[((t_) * 15 + (r_)) * DIPLOMAT_MSG_LEN]))
#define DIPLOMAT_MSG_GFX(p_)   ((p_)[0xc4])
#define DIPLOMAT_MSG_MUS(p_)   ((p_)[0xc6])

#define RESEARCH_DESCR_LEN  0xc3

#define EVENTMSG_TYPE_NUM   22
#define EVENTMSG_SUB_NUM    7
#define EVENTMSG_NUM  (EVENTMSG_TYPE_NUM * EVENTMSG_SUB_NUM)
#define EVENTMSG_LEN  0xc8

#define EVENTMSG_PTR(ga_, t_, s_)   (&((ga_)->eventmsg[((t_ - 1) * 7 + (s_)) * EVENTMSG_LEN]))

/* Storage for repeated movement in local multiplayer */
struct game_move_aux_s {
    fleet_enroute_t enroute[FLEET_ENROUTE_MAX];
    transport_t transport[TRANSPORT_MAX];
    monster_t crystal;
    monster_t amoeba;
};

struct firing_s {
    uint8_t d0[12]; /* uint16_t in lbx */
    uint8_t target_x;
    uint8_t target_y;
};

/* Aux game data, not stored in saves. */
struct game_aux_s {
    struct {
        const uint8_t *d0; /*[TECH_FIELD_NUM * 50 * 6]*/
        const char *names; /* tech names, "foo\0bar\0" etc */
        const char *descr; /*[TECH_FIELD_NUM * 50 * RESEARCH_DESCR_LEN] tech descriptions */
    } research;
    struct {
        uint8_t d0[DIPLOMAT_D0_NUM];    /* uint16_t in lbx */
        const char *msg;
    } diplomat;
    struct firing_s firing[NUM_SHIPLOOKS];
    const char *eventmsg;
    uint8_t star_dist[PLANETS_MAX][PLANETS_MAX];
    struct game_move_aux_s *move_temp;
    player_id_t human_killer;   /* used for funeral ending */
    int local_players;
    bool flag_cheat_galaxy;
    bool flag_cheat_elections;
    bool flag_cheat_events;
    bool flag_cheat_spy_hint;
    bool flag_cheat_stars;
    bool flag_cheat_tech_hint;
    bool flag_cheat_news;
    bool initialized;
    int savenamebuflen;
    int savebuflen;
    char *savenamebuf;
    uint8_t *savebuf;
};

extern int game_aux_init(struct game_aux_s *gaux, struct game_s *g);
extern void game_aux_shutdown(struct game_aux_s *gaux);

extern uint8_t game_aux_get_firing_param_x(const struct game_aux_s *gaux, uint8_t look, uint8_t a2, bool dir);
extern uint8_t game_aux_get_firing_param_y(const struct game_aux_s *gaux, uint8_t look, uint8_t a2, bool dir);

struct game_s;
extern void game_aux_start(struct game_aux_s *gaux, struct game_s *g);

#endif
