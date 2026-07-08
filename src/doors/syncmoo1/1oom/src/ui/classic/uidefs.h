#ifndef INC_1OOM_UIDEFS_H
#define INC_1OOM_UIDEFS_H

#include "boolvec.h"
#include "game.h"
#include "game_planet.h"
#include "game_types.h"
#include "gfxaux.h"
#include "osdefs.h"
#include "ui.h"
#include "types.h"

#ifdef OS_UI_SCALE_MAX
#define UI_SCALE_MAX    OS_UI_SCALE_MAX
#else
#define UI_SCALE_MAX    6
#endif

#define UI_SCROLL_SPEED_MAX 10

#define UI_VGA_W    320
#define UI_VGA_H    200

#define UI_SCREEN_W ui_screen_w
#define UI_SCREEN_H ui_screen_h

#define NUM_SOUNDS  0x29
#define NUM_MUSICS  0x28

typedef enum {
    UI_MAIN_LOOP_STARMAP = 0,
    UI_MAIN_LOOP_GAMEOPTS,  /*1*/
    UI_MAIN_LOOP_DESIGN, /*2*/
    UI_MAIN_LOOP_FLEET, /*3*/
    UI_MAIN_LOOP_MAP, /*4*/
    UI_MAIN_LOOP_RACES, /*5*/
    UI_MAIN_LOOP_PLANETS, /*6*/
    UI_MAIN_LOOP_TECH, /*7*/
    UI_MAIN_LOOP_NEXT_TURN, /*8*/
    UI_MAIN_LOOP_RELOC, /*9*/
    UI_MAIN_LOOP_TRANS, /*a*/
    UI_MAIN_LOOP_STARVIEW, /*b*/
    UI_MAIN_LOOP_ORBIT_OWN_SEL, /*c*/
    UI_MAIN_LOOP_TRANSPORT_SEL, /*d*/
    UI_MAIN_LOOP_ENROUTE_SEL, /*e*/
    UI_MAIN_LOOP_SPECS, /*f*/
    UI_MAIN_LOOP_MUSTSCRAP, /*10*/
    UI_MAIN_LOOP_EMPIRESTATUS, /*12*/
    UI_MAIN_LOOP_EMPIREREPORT, /*13*/
    UI_MAIN_LOOP_AUDIENCE, /*14*/
    UI_MAIN_LOOP_ORBIT_EN_SEL, /*15*/
    UI_MAIN_LOOP_SCRAP_BASES, /*16*/
    UI_MAIN_LOOP_SPIES_CAUGHT, /*17*/
    UI_MAIN_LOOP_PLANET_SHIPS,
    UI_MAIN_LOOP_NUM
} ui_main_loop_action_t;

typedef enum {
    UI_SM_STAR_TEXT_NAME,
    UI_SM_STAR_TEXT_POPULATION,
    UI_SM_STAR_TEXT_DISTANCE,
    UI_SM_STAR_TEXT_ENVIRONMENT,
    UI_SM_STAR_TEXT_SPECIAL,
    UI_SM_STAR_TEXT_NUM,
} ui_starmap_star_text_t;

struct ui_data_s {
    uint32_t seed;
    uint8_t *sfx[NUM_SOUNDS];
    uint8_t *mus;
    int music_i;
    struct {
        struct {
            uint8_t *nebula[NEBULA_MAX];    /* varies based on game data */
            uint8_t *smnebula[NEBULA_MAX];  /* varies based on game data */
            uint8_t *bmap;      /* varies based on game data */
            /* every other gfx.* is the same for every game */
            uint8_t *mainview;
            uint8_t *starback;
            uint8_t *starbak2;
            uint8_t *stars[12];
            uint8_t *planbord;
            uint8_t *yourplnt;
            uint8_t *unexplor;
            uint8_t *en_colny;
            uint8_t *no_colny;
            uint8_t *col_butt_ship;
            uint8_t *col_butt_reloc;
            uint8_t *col_butt_trans;
            uint8_t *sky;
            uint8_t *smstars[6];
            uint8_t *smalflag[BANNER_NUM];
            uint8_t *stargate; /* starmap.lbx */
            uint8_t *smallstr;
            uint8_t *smneb[4 * 10]; /* 10.. from nebula.lbx */
            uint8_t *relocate;
            uint8_t *reloc_bu_cancel;
            uint8_t *reloc_bu_accept;
            uint8_t *tran_bar;
            uint8_t *smalship[BANNER_NUM];
            uint8_t *smaltran[BANNER_NUM];
            uint8_t *tinyship[BANNER_NUM];
            uint8_t *tinytran[BANNER_NUM];
            uint8_t *move_shi;
            uint8_t *move_but_p;
            uint8_t *move_but_m;
            uint8_t *move_but_a;
            uint8_t *move_but_n;
            uint8_t *shipbord;
            uint8_t *movextra;
            uint8_t *movextr2;
            uint8_t *movextr3;
            uint8_t *scanner;
            uint8_t *tranship;
            uint8_t *tranbord;
            uint8_t *tranxtra;
            uint8_t *dismiss;
            uint8_t *fleetbut_view;
            uint8_t *fleetbut_scrap;
            uint8_t *fleetbut_ok;
            uint8_t *fleetbut_down;
            uint8_t *fleetbut_up;
            uint8_t *viewship;
            uint8_t *viewshp2;
            uint8_t *viewshbt;
            uint8_t *scrap;
            uint8_t *scrapbut_no;
            uint8_t *scrapbut_yes;
            uint8_t *reprtbut_ok;
            uint8_t *reprtbut_up;
            uint8_t *reprtbut_down;
            uint8_t *gr_arrow_u;
            uint8_t *gr_arrow_d;
            uint8_t *slanbord;
            uint8_t *stargate2; /* v11.lbx */
        } starmap;
        struct {
            uint8_t *planet[0x23];
            uint8_t *race[10];
            uint8_t *smonster;
            uint8_t *tmonster;
        } planets;
        uint8_t *ships[0x48 * 2 + 3];   /* 0..0x47 from ships2, 0x48.. from ships */
        struct {
            uint8_t *tech_but_up;
            uint8_t *tech_but_down;
            uint8_t *tech_but_ok;
            uint8_t *litebulb_off;
            uint8_t *litebulb_on;
            uint8_t *techback;
            uint8_t *race_pnt;
            struct {
                uint8_t *sabotage;
                uint8_t *espionage;
                uint8_t *hiding;
                uint8_t *status;
                uint8_t *report;
                uint8_t *audience;
                uint8_t *ok;
            } races_bu;
        } screens;
        struct {
            uint8_t *bg;
            uint8_t *blank;
            uint8_t *icon_dn;
            uint8_t *icon_up;
            uint8_t *count_up;
            uint8_t *count_dn;
            uint8_t *pop1_ul;
            uint8_t *pop1_ur;
            uint8_t *pop1_dl;
            uint8_t *pop1_dr;   /* planets.lbx 0x31 */
            uint8_t *titlebox;
            uint8_t *popscrol_u;
            uint8_t *popscrol_d;
        } design;
        struct {
            uint8_t *bg[5];
            uint8_t *box;
            uint8_t *box_x;
            uint8_t *box_y;
            uint8_t *box_xy;
            uint8_t *done;
            uint8_t *retreat;
            uint8_t *retr_off;
            uint8_t *wait;
            uint8_t *autob;
            uint8_t *special;
            uint8_t *spec_off;
            uint8_t *scan;
            uint8_t *scan_off;
            uint8_t *planet;
            uint8_t *planet_off;
            uint8_t *explos[10];
            uint8_t *warp1;
            uint8_t *warp2;
            uint8_t *warp3;
            uint8_t *warp4;
            uint8_t *technull;
            uint8_t *misbutt;
            uint8_t *misl_off;
            uint8_t *warpout;
            uint8_t *envterm;
            uint8_t *enviro;
            uint8_t *base_btn;
            uint8_t *dis_bem2;
            uint8_t *stasis2;
            uint8_t *vs2;
            uint8_t *vp2_top;
            uint8_t *vp2_data;
            uint8_t *vp2_line;
            uint8_t *vp2_bottom;
            uint8_t *blk_hole;
            uint8_t *bombs;
            uint8_t *biologic;
            uint8_t *circle;
            uint8_t *sphere2;
            uint8_t *asteroid[4];
        } space;
        struct {
            /* directions: u, ur, r, dr, d, dl, l, ul */
            uint8_t *missiles[8];
            uint8_t *antimatr[8];
            uint8_t *hellfire[8];
            uint8_t *proton[8];
            uint8_t *plasmaqt[8];
        } missile;
        struct {
            uint8_t *d[0x1c];
            uint8_t *current;
        } colonies;
        struct {
            uint8_t *tv;
            uint8_t *gnn;
            uint8_t *nc;
            uint8_t *world;
            uint8_t *icon;
            bool flag_also;
        } news;
        uint8_t *vgafileh;
        bool initialized;
    } gfx;
    struct {
        int x;
        int y;
        int x2;
        int y2;
        int xhold;
        int yhold;
        bool flag_show_grid;
        bool flag_show_own_routes;
        ui_starmap_star_text_t star_text_type;
        int line_anim_phase;
        int frame_ship;
        int frame_scanner;
        int scanner_delay;
        int stars_xoff1;
        int stars_xoff2;
        struct gfx_aux_s star_aux;
        int fleet_selected;
        int orbit_player;
        BOOLVEC_DECLARE(select_prio_fleet, FLEET_ENROUTE_MAX);
        BOOLVEC_DECLARE(select_prio_trans, TRANSPORT_MAX);
    } starmap;
    struct {
        struct gfx_aux_s screen;
        struct gfx_aux_s ship_p1;
        struct gfx_aux_s ship_overlay;
        struct gfx_aux_s btemp;
    } aux;
    struct {
        bool flag_also;
    } news;
    struct {
        const struct game_s *g;
        uint16_t index[FLEET_ENROUTE_MAX + PLANETS_MAX];
        uint32_t value[FLEET_ENROUTE_MAX + PLANETS_MAX];
        int fleet_order[PLAYER_NUM];
        int planets_order[PLAYER_NUM];
        int weapon_type[PLAYER_NUM];
    } sorted;   /* global for qsort */
    struct {
        bool show_grid;
    } battle;
    uint8_t star_frame[PLANETS_MAX];
    ui_main_loop_action_t ui_main_loop_action;
    ui_main_loop_action_t ui_main_loop_action_prev;
    ui_main_loop_action_t ui_main_loop_action_next;
    uint8_t start_planet_focus_i;
    bool flag_scrap_for_new_design;
    bool have_help;
    BOOLVEC_DECLARE(players_viewing, PLAYER_NUM);
    char strbuf[UI_STRBUF_SIZE];
};

extern struct ui_data_s ui_data;
extern int ui_scale;
extern int ui_scale_hint;
extern int starmap_scale;
extern int ui_screen_w;
extern int ui_screen_h;
extern bool ui_extra_enabled;
extern bool ui_fixbugs_enabled;
extern bool ui_illogical_hotkey_fix;
extern bool ui_load_opts_extra;
extern bool ui_space_combat_autoresolve;
extern bool ui_sm_ships_enabled;
extern bool ui_sm_expanded_scroll;
extern bool ui_sm_no_question_mark_cursor;
extern bool ui_sm_explicit_cursor_context;
extern bool ui_mouse_lmb_fix;
extern bool ui_mouse_warp_disabled;
extern bool ui_copyprotection_disabled;
extern bool ui_sm_mouseover_focus;
extern bool ui_sm_smoother_scrolling;
extern bool ui_sm_mouse_scroll;
extern bool ui_sm_uhjk_scroll;
extern bool ui_kbd_cursor_keys_fix;
extern bool ui_mwi_slider;
extern bool ui_mwi_counter;
extern int ui_sm_scroll_speed;
extern bool ui_kbd_repeat;

extern void ui_extra_toggle_preset(bool enabled);

#endif
