#pragma once
/* Cross-subsystem prototypes for the game code (image 0x0000–0x4940). Each module implements its own
 * functions (see PORTING.md for ownership); internal helpers stay static or in the module's own header.
 * Names follow port/symbols.csv. All game state is in mem[] (symbols.h offsets). */
#include "../mem.h"
#include "../symbols.h"

/* ---- game_flow (game/flow*.c) ------------------------------------------------------------------ */
int  game_main(void);                   /* 0x0010 main (launcher password / protection dropped) */
int  run_game(void);                    /* 0x1030 */
int  run_stage(void);                   /* 0x1DC0 per-frame stage loop: -1 abort/demo end, 0 game over, 1 stage done */

/* ---- scene_render (game/scene*.c) -------------------------------------------------------------- */
void stage_init_road_ptr(void);         /* 0x1F4E */
void wait_fire_button(void);            /* 0x1F7B */
void reset_car_state(void);             /* 0x1F93 */
void snapshot_sim_state(void);          /* 0x2013 */
void project_road_main(void);           /* 0x2054 */
void project_road_mirror(void);         /* 0x2478 */
void draw_road_main(void);              /* 0x28C5 */
void fill_scenery_above_road(void);     /* 0x2EDC */
void draw_road_mirror(void);            /* 0x2F7A */
void draw_mirror_background(void);     /* 0x33FB */
void draw_buffer_overlays(void);        /* 0x349D */
void draw_gear_box(void);               /* 0x351A */
void draw_dashboard_dynamic(void);      /* 0x35C6 */
void dealership_ending(void);           /* 0x38EB */
void crash_windscreen_sequence(void);   /* 0x392C */
void select_screen(void);               /* 0x3AC2 */
void select_road_buffer(void);          /* 0x3ACE */
void present_road_buffer(void);         /* 0x3AF7 */

/* ---- simulation (game/sim*.c) ------------------------------------------------------------------ */
void stage_enter_install_isr(void);     /* 0x4792 stage setup; installs sim_timer_isr via timer_set_driving_isr */
void sim_timer_isr(void);               /* 0x3B1F driving ISR body (calls timer_isr() first) */
u16  car_data_ptr(void);                /* 0x493D returns 0x268F */
