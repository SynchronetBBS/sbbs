#ifndef INC_1OOM_UI_H
#define INC_1OOM_UI_H

/* API to ui/ */

#include "cfg.h"
#include "options.h"
#include "types.h"

extern const char *idstr_ui;

extern void ui_early_show_message_box(const char *msg);

extern int ui_early_init(void);
extern int ui_init(void);
extern int ui_late_init(void);
extern void ui_shutdown(void);

extern bool ui_use_audio;

extern const struct cmdline_options_s ui_cmdline_options[];
extern const struct cfg_items_s ui_cfg_items[];

#define UI_STRBUF_SIZE 1024
extern char *ui_get_strbuf(void);

extern void ui_play_intro(void);
extern void ui_play_ending_good(int race, const char *name);
extern void ui_play_ending_tyrant(int race, const char *name);
extern void ui_play_ending_funeral(int banner_live, int banner_dead);
extern void ui_play_ending_exile(const char *name);

typedef enum {
    MAIN_MENU_ACT_NEW_GAME,
    MAIN_MENU_ACT_CUSTOM_GAME,
    MAIN_MENU_ACT_CHALLENGE_GAME,
    MAIN_MENU_ACT_LOAD_GAME,
    MAIN_MENU_ACT_LOAD_GAME_MOO13,
    MAIN_MENU_ACT_CONTINUE_GAME,
    MAIN_MENU_ACT_QUIT_GAME,
    MAIN_MENU_ACT_TUTOR
} main_menu_action_t;

struct game_new_options_s;

extern main_menu_action_t ui_main_menu(struct game_new_options_s *newopts, struct game_new_options_s *customopts, struct game_new_options_s *challengeopts, int *load_game_i_ptr);

struct game_s;
struct game_end_s;

extern void ui_game_start(struct game_s *g);
extern void ui_game_end(struct game_s *g);

extern void ui_sound_play_sfx(int sfxi);

typedef enum {
    UI_TURN_ACT_NEXT_TURN,
    UI_TURN_ACT_LOAD_GAME,
    UI_TURN_ACT_QUIT_GAME
} ui_turn_action_t;

extern ui_turn_action_t ui_game_turn(struct game_s *g, int *load_game_i_ptr, int pi);

extern void *ui_gmap_basic_init(struct game_s *g, bool show_player_switch);
extern void ui_gmap_basic_start_player(void *ctx, int pi);
extern void ui_gmap_basic_start_frame(void *ctx, int pi);
extern void ui_gmap_basic_draw_frame(void *ctx, int pi);
extern void ui_gmap_basic_finish_frame(void *ctx, int pi);
extern void ui_gmap_basic_shutdown(void *ctx);

extern uint8_t *ui_gfx_get_ship(int look);
extern uint8_t *ui_gfx_get_planet(int look);
extern uint8_t *ui_gfx_get_rock(int look);

struct battle_s;

#define UI_BATTLE_ACT_CLICK(_x_, _y_)    BATTLE_XY_SET((_x_), (_y_))
#define UI_BATTLE_ACT_GET_X(_v_)    BATTLE_XY_GET_X(_v_)
#define UI_BATTLE_ACT_GET_Y(_v_)    BATTLE_XY_GET_Y(_v_)
#define UI_BATTLE_ACT_NONE      0x80
#define UI_BATTLE_ACT_WAIT      0x81
#define UI_BATTLE_ACT_DONE      0x82
#define UI_BATTLE_ACT_RETREAT   0x83
#define UI_BATTLE_ACT_AUTO      0x84
#define UI_BATTLE_ACT_MISSILE   0x85
#define UI_BATTLE_ACT_PLANET    0x86
#define UI_BATTLE_ACT_SCAN      0x87
#define UI_BATTLE_ACT_SPECIAL   0x88

typedef uint8_t ui_battle_action_t;

typedef enum {
    UI_BATTLE_AUTORESOLVE_OFF,
    UI_BATTLE_AUTORESOLVE_AUTO,
    UI_BATTLE_AUTORESOLVE_RETREAT
} ui_battle_autoresolve_t;

typedef enum {
    UI_BATTLE_BOMB_BOMB,
    UI_BATTLE_BOMB_BIO,
    UI_BATTLE_BOMB_WARPDIS
} ui_battle_bomb_t;

extern ui_battle_autoresolve_t ui_battle_init(struct battle_s *bt);
extern void ui_battle_shutdown(struct battle_s *bt, bool colony_destroyed, int winner);

extern void ui_battle_draw_misshield(const struct battle_s *bt, int target_i, int target_x, int target_y, int missile_i);
extern void ui_battle_draw_damage(const struct battle_s *bt, int target_i, int target_x, int target_y, uint32_t damage);
extern void ui_battle_draw_explos_small(const struct battle_s *bt, int x, int y);
extern void ui_battle_draw_basic(const struct battle_s *bt);
extern void ui_battle_draw_basic_copy(const struct battle_s *bt);
extern void ui_battle_draw_missile(const struct battle_s *bt, int missilei, int x, int y, int tx, int ty);
extern void ui_battle_draw_cloaking(const struct battle_s *bt, int from, int to, int sx, int sy);
extern void ui_battle_draw_arena(const struct battle_s *bt, int itemi, int dmode);
extern void ui_battle_draw_item(const struct battle_s *bt, int itemi, int x, int y);
extern void ui_battle_draw_bomb_attack(const struct battle_s *bt, int attacker_i, int target_i, ui_battle_bomb_t bombtype);
extern void ui_battle_draw_beam_attack(const struct battle_s *bt, int attacker_i, int target_i, int wpni);
extern void ui_battle_draw_stasis(const struct battle_s *bt, int attacker_i, int target_i);
extern void ui_battle_draw_pulsar(const struct battle_s *bt, int attacker_i, int ptype, const uint32_t *dmgtbl);
extern void ui_battle_draw_stream1(const struct battle_s *bt, int attacker_i, int target_i);
extern void ui_battle_draw_stream2(const struct battle_s *bt, int attacker_i, int target_i);
extern void ui_battle_draw_blackhole(const struct battle_s *bt, int attacker_i, int target_i);
extern void ui_battle_draw_technull(const struct battle_s *bt, int attacker_i, int target_i);
extern void ui_battle_draw_repulse(const struct battle_s *bt, int attacker_i, int target_i, int sx, int sy);
extern void ui_battle_draw_retreat(const struct battle_s *bt);
extern void ui_battle_draw_bottom(const struct battle_s *bt);
extern void ui_battle_draw_planetinfo(const struct battle_s *bt, bool side_r);
extern void ui_battle_draw_scan(const struct battle_s *bt, bool side_r);
extern void ui_battle_draw_finish(const struct battle_s *bt);

extern void ui_battle_area_setup(const struct battle_s *bt);
extern void ui_battle_turn_pre(const struct battle_s *bt);
extern void ui_battle_turn_post(const struct battle_s *bt);
extern ui_battle_action_t ui_battle_turn(const struct battle_s *bt);
extern void ui_battle_ai_pre(const struct battle_s *bt);
extern bool ui_battle_ai_post(const struct battle_s *bt);

extern int ui_spy_steal(struct game_s *g, int spy, int target, uint8_t flags_field);
extern void ui_spy_stolen(struct game_s *g, int pi, int spy, int field, uint8_t tech);

typedef enum {
    UI_SABOTAGE_FACT, /*0*/
    UI_SABOTAGE_BASES, /*1*/
    UI_SABOTAGE_REVOLT, /*2*/
    UI_SABOTAGE_NONE /*-1*/
} ui_sabotage_t;

extern ui_sabotage_t ui_spy_sabotage_ask(struct game_s *g, int spy, int target, uint8_t *planetptr);
extern int ui_spy_sabotage_done(struct game_s *g, int pi, int spy, int target, ui_sabotage_t act, int other1, int other2, uint8_t planet, int snum);

extern void ui_newtech(struct game_s *g, int pi);

extern bool ui_explore(struct game_s *g, int pi, uint8_t planet_i, bool by_scanner, bool flag_colony_ship);
extern void ui_landing(struct game_s *g, int pi, uint8_t planet_i);

extern bool ui_bomb_ask(struct game_s *g, int pi, uint8_t planet_i, int pop_inbound);
extern void ui_bomb_show(struct game_s *g, int pi, int attacker_i, int owner_i, uint8_t planet_i, int popdmg, int factdmg, bool play_music, bool hide_other);

extern void ui_turn_pre(const struct game_s *g);
extern void ui_turn_msg(struct game_s *g, int pi, const char *str);

struct ground_s;
extern void ui_ground(struct game_s *g, struct ground_s *gr);

struct news_s;
extern void ui_news_start(void);
extern void ui_news(struct game_s *g, struct news_s *ns);
extern void ui_news_end(void);

struct election_s;
extern void ui_election_start(struct election_s *el);
extern void ui_election_show(struct election_s *el);
extern void ui_election_delay(struct election_s *el, int delay);
extern int ui_election_vote(struct election_s *el, int player_i);
extern bool ui_election_accept(struct election_s *el, int player_i);
extern void ui_election_end(struct election_s *el);

struct audience_s;
extern void ui_audience_start(struct audience_s *au);
extern void ui_audience_show1(struct audience_s *au);
extern void ui_audience_show2(struct audience_s *au);
extern void ui_audience_show3(struct audience_s *au);
extern int16_t ui_audience_ask2a(struct audience_s *au);
extern int16_t ui_audience_ask2b(struct audience_s *au);
extern int16_t ui_audience_ask3(struct audience_s *au);
extern int16_t ui_audience_ask4(struct audience_s *au);
extern void ui_audience_newtech(struct audience_s *au, int pi);
extern void ui_audience_end(struct audience_s *au);

extern void ui_newships(struct game_s *g, int pi);

extern void ui_copyprotection_check(struct game_s *g);
extern void ui_copyprotection_lose(struct game_s *g, struct game_end_s *ge);

#endif
