/* scene_render: stage start, per-life reset, snapshot, buffer selection/present, overlays and the
 * end sequences. Port of TDEGA image 0x1F4E..0x2053, 0x349D..0x3519, 0x38EB..0x39EA, 0x3AC2..0x3B17
 * (port/spec/scene_render.md §4.1, §4.8, §4.9). Projection: scene_project.c, road drawing:
 * scene_draw.c, cockpit: scene_cockpit.c. */
#include <string.h>
#include "scene.h"
#include "../host.h"
#include "../platform/input.h"

/* 0x1F4E stage_init_road_ptr — §4.1 */
void stage_init_road_ptr(void)
{
    DSW(DS_road_pos) = (u16)(DSW((u16)(EGA_CGA(0x6361, 0x6331) + (DSW(DS_g_stage) << 1))) + 0x2D);   /* stage_ptr[] */
    DSW(DS_sub_unit) = 0;
    DSW(DS_g_stageTime) = 0;                  /* DS:80A4 tick count */
    DSW(DS_speed_limit_idx) = 0;
    DSB(DS_wheel_state) = 1;
    DSB(DS_marker_saved) = 0;
}

/* 0x1F7B wait_fire_button — §2: returns at once in demo mode or without an input device */
void wait_fire_button(void)
{
    for (;;) {
        if (DSW(DS_demo_mode) != 0) return;
        host_pump();                          /* PORT: busy-wait loop pumps the host */
        u16 ax = input_poll_drive();
        if ((u8)ax == 0xFF) return;
        if ((u8)(ax >> 8) == 0xFF) return;
        if (ax & 0x10) return;
    }
}

/* 0x1F93 reset_car_state — §4.1 */
void reset_car_state(void)
{
    DSW(DS_g_carSpeedFixed) = 0;
    DSW(DS_car_x) = 0xFF8B;
    DSW(DS_rpm) = 800;
    DSW(DS_rpm_smooth) = 800;
    DSW(DS_steer_angle) = 0;
    DSW(DS_yaw_lateral) = 0;
    DSW(DS_view_heading) = 0;
    DSW(DS_unused_090e) = 0;
    DSW(DS_row_limit_main) = 0;
    DSW(DS_row_limit_mirror) = 0;
    DSB(DS_g_stageEvent) = 0;
    DSB(DS_gear) = 0;
    DSB(DS_skidding) = 0;
    DSB(DS_clock_running) = 0;
    DSB(DS_ending_shown) = 0;
    DSB(DS_cop_state) = 0;
    DSB(DS_oncoming_count) = 0;
    DSB(DS_samedir_count) = 0;
    DSB(DS_grind_timer) = 0;
    for (u16 k = 0; k < 13; k++) DSW((u16)(DS_oncoming_list + 8 * k)) = 0;
    DSW(DS_note_div_engine) = DSW(EGA_CGA(0x208F, 0x2062));
    DSW(DS_note_div_beep) = 900;
    DSW(DS_note_div_skid) = 0xFFFF;
    DSW(DS_knob_x) = DSW(DS_car_knob_xy);
    DSW(DS_knob_y) = DSW(DS_car_knob_xy + 2);
    DSB(DS_dash_timer) = 13;
    DSB(DS_gearbox_dirty) = 1;
    DSB(DS_gauges_dirty) = 1;
    DSB(DS_gate_node) = 1;
}

/* 0x2013 snapshot_sim_state — §4.1 (also leaves ES = DS for the projection code) */
void snapshot_sim_state(void)
{
    memmove(mp(DGROUP, DS_r_traffic_slots), mp(DGROUP, DS_oncoming_list), 0x68);
    DSW(DS_walk_ptr) = DSW(DS_road_pos);
    DSW(DS_r_road_ptr) = DSW(DS_road_pos);
    DSW(DS_r_subpos) = DSW(DS_sub_unit);
    DSW(DS_r_cop_pos) = DSW(DS_cop_pos);
    DSW(DS_r_cop_sub) = DSW(DS_cop_sub);
    DSW(DS_r_cop_lane) = DSW(DS_cop_lateral);
    DSB(DS_r_cop_state) = DSB(DS_cop_state);
    DSB(DS_r_traffic_count_l) = DSB(DS_oncoming_count);
    DSB(DS_r_traffic_count_r) = DSB(DS_samedir_count);
}

/* 0x3AC2 select_screen */
void select_screen(void)
{
    gfx_select_target(gfx_screen_desc());
}

/* 0x3ACE select_road_buffer: descriptor CS:[0890], clip x 0..40 bytes, y 0x13..0x6F */
void select_road_buffer(void)
{
    gfx_select_target(far_make(CODE_SEG, DSW(DS_road_buf_desc)));
    CSW(GFX_CUR_CLIP_Y1) = 0x6F;
    CSW(GFX_CUR_CLIP_Y0) = 0x13;
    CSW(GFX_CUR_CLIP_X0) = 0;
    CSW(GFX_CUR_CLIP_X1) = EGA_CGA(0x28, 0x50);
}

/* 0x3AF7 present_road_buffer: replace blit of the 3-plane buffer (planes 0-2 only) to VRAM */
void present_road_buffer(void)
{
    gfx_select_target(gfx_screen_desc());
#if !TD_CGA
    CSW(GFX_CUR_CLIP_Y1) = 0x6F;                                         /* TDCGA 0x3A41: not set */
#endif
    blit_copy_clip_own(scene_ds_far(DS_road_buf_sprite));
}

/* 0x349D draw_buffer_overlays — §4.8 */
void draw_buffer_overlays(void)
{
    select_road_buffer();
    blit_and_own(scene_ds_far(DS_spr_mirr));                            /* CAR mirr (DS:13BB) */
    if (DSB(DS_cop_state) == 6 && DSB(DS_cop_timer) == 0)
        blit_copy_own(scene_ds_far(DS_spr_tick));                       /* XROADA tick (DS:103F) */
    if (DSB(DS_g_stageEvent) != 2) return;
    if (DSW(DS_g_stage) != 4) {
        gfx_set_text_colours(3, 0);
        draw_text_centered((const char *)mp(DGROUP, EGA_CGA(0x1FF7, 0x1FCA)), 0x50);    /* " Pulling into the gas station... " */
    } else if (DSB(DS_ending_shown) != 1) {
        gfx_set_text_colours(3, 0);
        draw_text_centered((const char *)mp(DGROUP, EGA_CGA(0x2019, 0x1FEC)), 0x50);    /* " Pulling into the dealership... " */
    }
}

/* 0x38EB dealership_ending — §4.9 */
void dealership_ending(void)
{
    DSB(DS_ending_shown) = 1;
#if TD_CGA
    /* TDCGA 0x383B: drawn into the road buffer, with the overlays, then presented */
    select_road_buffer();
    blit_copy_clip_own(scene_ds_far(DS_spr_deal));
    draw_buffer_overlays();
    present_road_buffer();
    wait_fire_button();
    select_road_buffer();
    blit_copy_own(scene_ds_far(DS_spr_note));
    present_road_buffer();
#else
    select_screen();
    blit_copy_clip_own(scene_ds_far(DS_spr_deal));
    blit_and_own(scene_ds_far(DS_spr_mirr));
    wait_fire_button();
    blit_copy_own(scene_ds_far(DS_spr_note));
#endif
    for (;;) {                                                           /* wait for fire release */
        if (!(input_poll_drive() & 0x10)) break;
        host_pump();                                                     /* PORT: busy-wait pumps the host */
    }
    wait_fire_button();
}

/* 0x392C crash_windscreen_sequence — §4.9. Reuses the last projection; cracks from DS:2532/2539. */
void crash_windscreen_sequence(void)
{
    host_frame_begin();                      /* PORT: each of the 7 redraws takes one emulated frame */
    select_road_buffer();
#if !TD_CGA
    gfx_clear_clip(8);                       /* TDCGA 0x3878: none */
#endif
    fill_scenery_above_road();
    draw_road_main();                        /* doubles min_ol_sy a second time (quirk) */
    select_road_buffer();

    {   /* set 0: star from point 0 to points 1..cnt */
        u16 bx = DSW(DS_crack_ptrs);
        u16 dx = DSB(DS_crack_counts);
        u16 si = 2;
        do {
            gfx_draw_line((s16)((u16)DSB(bx) << 1), (s16)DSB((u16)(bx + 1)),
                          (s16)((u16)DSB((u16)(bx + si)) << 1), (s16)DSB((u16)(bx + si + 1)), 0xFF);
            si = (u16)(si + 2);
        } while (--dx != 0);
    }
    draw_mirror_background();
    draw_road_mirror();
    draw_buffer_overlays();
    present_road_buffer();

    for (u16 cx = 1; cx != 7; cx++) {        /* sets 1..6: segments, accumulating in the buffer */
        host_frame_begin();
        select_road_buffer();
        u16 dx = DSB((u16)(DS_crack_counts + cx));
        u16 bx = DSW((u16)(DS_crack_ptrs + (cx << 1)));
        u16 si = 0;
        do {
            gfx_draw_line((s16)((u16)DSB((u16)(bx + si)) << 1), (s16)DSB((u16)(bx + si + 1)),
                          (s16)((u16)DSB((u16)(bx + si + 2)) << 1), (s16)DSB((u16)(bx + si + 3)), 0xFF);
            si = (u16)(si + 4);
        } while (--dx != 0);
        draw_mirror_background();
        draw_road_mirror();
        draw_buffer_overlays();
        present_road_buffer();
    }
    if (DSW(DS_g_lives) != 1) wait_fire_button();
}
