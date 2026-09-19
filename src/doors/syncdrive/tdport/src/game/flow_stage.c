/* game_flow: stage loop, scoring, gas station results, per-frame stage runner —
 * port/spec/game_flow.md §4, scene_render.md §4.9, simulation.md §1. */
#include <stdio.h>
#include <string.h>

#include "flow.h"
#include "../host.h"
#include "../platform/gfx.h"
#include "../platform/input.h"
#include "../platform/res.h"
#include "../platform/timer.h"

#define DS_STAGE_START EGA_CGA(0x6361, 0x6331)   /* u16[5] road stream start per stage (read by 0x1F4E and 0x1DC0) */

/* 0x1030 run_game — game_flow.md §4 (verified against disassembly 0x1030-0x125C) */
int run_game(void)
{
    char tmp[30];
    int r;
    s32 score;

    DSS(DS_g_lives) = 5;
    DSL(DS_g_totalScore) = 0;
    DSW(DS_g_tooSlow) = 0;
    DSW(DS_g_stage) = DSW(DS_demo_mode) ? 4 : 0;
    for (;;) {
        u16 si = (u16)(DSW(DS_g_stage) * 2);
        DSW(DS_g_parTime) = DSW(DS_STAGE_PAR_TIME + si);
        DSW(DS_g_stageConstB) = DSW(DS_STAGE_CONST_B + si);
#if TD_CGA
        /* TDCGA 0x1036: one traffic archive, xroadb.cmp, kept in g_xroadC (xroada.cmp is loaded by main) */
        far_wr(DGROUP, DS_g_xroadC, load_archive(DSTR(0x27A), 0x0908));
#else
        far_wr(DGROUP, DS_g_xroadA, load_archive(DSTR(0x26E), 0x1E84));
        far_wr(DGROUP, DS_g_xroadB, load_archive(DSTR(0x279), 0x17AE));
        far_wr(DGROUP, DS_g_xroadC, load_archive(DSTR(0x284), 0x0FA0));
#endif
        snprintf(tmp, sizeof tmp, EGA_CGA("%s.pes", "%s.cmp"), flow_car_name(DSS(DS_g_selectedCar)));
        far_wr(DGROUP, DS_g_carArchive, load_archive(tmp, EGA_CGA(0x7D0, 0x3E8)));

        /* car .BIN loader at 0x10E2: open(O_BINARY); read(fd, car_data_ptr(), 0x4D6); close (no checks) */
        snprintf(tmp, sizeof tmp, "%s.bin", flow_car_name(DSS(DS_g_selectedCar)));
        FILE *fb = flow_fopen_game(tmp, false, "rb");
        u16 bin = car_data_ptr();
        if (fb) {
            size_t got = fread(mp(DGROUP, bin), 1, 0x4D6, fb);
            (void)got;
            fclose(fb);
        }

        delay_ticks(400);
        gfx_free_buffer(flow_page_desc());
        r = run_stage();
        far_wr(DGROUP, DS_page_buf_desc, gfx_create_buffer(EGA_CGA(0x28, 0x50), 200, 0x0F));
        if (DSW(DS_g_stageTime) == 0) DSW(DS_g_stageTime) = 1;
        s16 t = DSS(DS_g_stageTime);
        DSS(DS_g_avgSpeed) = (s16)(((s32)DSS(DS_g_avgSpeed) * 0x5A) / (s32)t);   /* _aNlmul, _aNldiv */
        DSS(DS_g_stageTime) = (s16)(t / 12);                                    /* cwd; idiv */
        timer_install_menu();                             /* run_stage restored the timer */
        if (r != 1) break;
        score = stage_score();
        if (DSW(DS_demo_mode) != 0) score = 0;
        DSL(DS_g_totalScore) = DSL(DS_g_totalScore) + (u32)score;
        if (DSS(DS_g_stage) >= 4) return 0;
        stage_results(score);
        if (DSW(DS_g_tooSlow) != 0) return 0;
        DSW(DS_g_stage)++;
    }
    if (r != 0) {                                         /* -1: Esc or demo timeout */
        DSL(DS_g_totalScore) = 0;
        return 1;
    }
    /* game over: wipe to black, unpaced */
    gfx_select_target(flow_page_desc());
    gfx_clear_clip(0);
    gfx_select_target(gfx_screen_desc());
    for (int i = 0; i < 8; i++) {
        set_deadline(1);
        gfx_dissolve(flow_page_sprite(), (u8)i);
        host_present_now();
    }
    return 0;
}

/* 0x125D stage_score — game_flow.md §4 (verified against disassembly 0x125D-0x12DA) */
s32 stage_score(void)
{
    s32 bonus = 0;
    u16 diff = (u16)(DSW(DS_g_stageTime) - DSW(DS_g_parTime));
    s16 d = (s16)(u16)(diff * 12u);                       /* 16-bit multiply (shl/add), then cwd */
    s32 t = d;
    if (t < 100) {
        bonus = 100 - t;
        t = 100;
    }
    s32 factor = DSS(DS_STAGE_SCORE_FACTOR + DSW(DS_g_stage) * 2);
    return (factor * 1000000) / t + bonus;                /* _aNlmul(factor, 0xF4240), _aNldiv */
}

/* 0x1993 stage_results — game_flow.md §4 (verified) */
void stage_results(s32 score)
{
    FarPtr scr = gfx_screen_desc();
    DSW(DS_g_scrollDelay) = 15;
    snd_play_oneshot(far_rd(DGROUP, DS_g_songGas));
    DSS(DS_g_lives) = (s16)(DSS(DS_g_lives) + 2);
    FarPtr sb  = load_archive(DSTR(DS_g_sbArchiveName), EGA_CGA(0x109A, 0x908));  /* "<car>sb.pes" (cached) */
    FarPtr gas = load_archive(DSTR(EGA_CGA(0x768, 0x75E)), EGA_CGA(2000, 1000));  /* "gas.pes" (.cmp) */
    FarPtr gcar = res_find(sb, DSTR(EGA_CGA(0x770, 0x766)));              /* "gcar" */
    FarPtr bg   = res_find(gas, DSTR(EGA_CGA(0x775, 0x76B)));             /* "gas " */
    FarPtr sign = res_find(gas, DSTR(DSW(DS_GAS_SIGN_NAMES + DSW(DS_g_stage) * 2)));
    gfx_select_target(flow_page_desc());
    gfx_clear_clip(0);
    gfx_set_clip(scr, 0, EGA_CGA(0x28, 0x50), 0, 200);
    gfx_select_target(scr);
    for (int i = 0; i < 8; i++) {                         /* wipe to black: deadline set, never waited */
        set_deadline(10);
        gfx_dissolve(flow_page_sprite(), (u8)i);
        host_present_now();
    }
    gfx_select_target(flow_page_desc());
    blit_copy_own(bg);
    blit_copy_own(sign);
    blit_copy_own(gcar);
    gfx_select_target(scr);
    for (int i = 0; i < 8; i++) {                         /* wipe in, unpaced */
        set_deadline(10);
        gfx_dissolve(flow_page_sprite(), (u8)i);
        host_present_now();
    }
    results_text(score);
    getkey_wait();
    gfx_set_clip(scr, 0, EGA_CGA(0x28, 0x50), 0, 200);
    gfx_clear_screen(0);
    snd_stop_oneshot();
}

/* 0x1B36 results_text — game_flow.md §4 (thresholds and sprintf arguments verified 0x1B36-0x1CF9) */
void results_text(s32 score)
{
    char buf[80];
    u16 l1, l2;
    gfx_set_text_colours(EGA_CGA(0x0F, 3), 0);
    s16 secs = DSS(DS_g_stageTime);
    gfx_select_target(flow_page_desc());
    gfx_clear_clip(0);
    DSW(DS_g_resultTextY) = 0;
    DSW(DS_g_resultLineCount) = 0;
    u16 k = (u16)(s16)(crt_rand() % 3);                   /* cwd; idiv 3 */
    s16 avg = DSS(DS_g_avgSpeed);
    if (avg < 50) {
        l1 = DSW(DS_MSG_TABLES + 0x18 + k * 2); l2 = DSW(DS_MSG_TABLES + 0x36 + k * 2);   /* DS:0792, DS:07B0 */
        DSW(DS_g_tooSlow) = 1;
    } else if (avg > 105) {
        l1 = DSW(DS_MSG_TABLES + 0x00 + k * 2); l2 = DSW(DS_MSG_TABLES + 0x1E + k * 2);   /* DS:077A, DS:0798 */
    } else if (avg > 95) {
        l1 = DSW(DS_MSG_TABLES + 0x06 + k * 2); l2 = DSW(DS_MSG_TABLES + 0x24 + k * 2);   /* DS:0780, DS:079E */
    } else if (avg > 65) {
        l1 = DSW(DS_MSG_TABLES + 0x0C + k * 2); l2 = DSW(DS_MSG_TABLES + 0x2A + k * 2);   /* DS:0786, DS:07A4 */
    } else {
        l1 = DSW(DS_MSG_TABLES + 0x12 + k * 2); l2 = DSW(DS_MSG_TABLES + 0x30 + k * 2);   /* DS:078C, DS:07AA */
    }
    delay_ticks(50);
    results_add_line(DSTR(l1));
    results_add_line(DSTR(l2));
    snprintf(buf, sizeof buf, "Your average speed was %d and it took", (int)avg);           /* DS:07C4 */
    results_add_line(buf);
    if (secs >= 120)
        snprintf(buf, sizeof buf, "you %2d minutes and %02d seconds.", secs / 60, secs % 60); /* DS:07EA */
    else if (secs >= 60)
        snprintf(buf, sizeof buf, "you %2d minute and %02d seconds.", secs / 60, secs % 60);  /* DS:080C */
    else
        snprintf(buf, sizeof buf, "you %2d seconds.", (int)secs);                             /* DS:082D */
    results_add_line(buf);
    snprintf(buf, sizeof buf, "That kind of time is worth");                                  /* DS:083E */
    results_add_line(buf);
    snprintf(buf, sizeof buf, "%ld points.", (long)score);                                    /* DS:0859 */
    results_add_line(buf);
    if (DSW(DS_g_tooSlow) == 0) {
        u16 tip = (u16)(s16)(crt_rand() % 3);
        results_add_line(DSTR(DSW(DS_MSG_TABLES + 0x3C + tip * 2)));                  /* DS:07B6 */
    }
    results_add_line(DSTR(EGA_CGA(0x865, 0x85B)));                        /* "Press key or joystick button to continue" */
    results_scroll();
}

/* 0x1CFA results_add_line — game_flow.md §4 (verified) */
void results_add_line(const char *s)
{
    if (*s != 0) {
        gfx_draw_text(s, 0, DSS(DS_g_resultTextY));
        DSW(DS_g_resultTextY) = (u16)(DSW(DS_g_resultTextY) + 8);
        DSW(DS_g_resultLineCount)++;
    }
}

/* 0x1D20 results_scroll — game_flow.md §4 (verified) */
void results_scroll(void)
{
    s16 y = 199;
    gfx_select_target(gfx_screen_desc());
    gfx_set_clip(gfx_screen_desc(), 0, EGA_CGA(0x28, 0x50), 0xB0, 200);
    for (s16 line = 0; line < DSS(DS_g_resultLineCount); line++) {
        for (s16 j = 0; j < 8; j++) {
            set_deadline(DSW(DS_g_scrollDelay));
            blit_copy_clip_hot(flow_page_sprite(), 0, y);    /* page top at y */
            if (menu_key() != -1) DSW(DS_g_scrollDelay) = 1; /* any key speeds up, sticky */
            y--;
        }
        delay_ticks(100);                                    /* 1 s per line, not skippable */
    }
}

/* 0x1DC0 run_stage — game_flow.md §4, scene_render.md §4.9 (verified against disassembly 0x1DC0-0x1F4D) */
int run_stage(void)
{
    int r;
    stage_init_road_ptr();
    reset_car_state();
    stage_enter_install_isr();
    select_screen();
#if TD_CGA
    /* TDCGA 0x1D7B: dash and roof are drawn before the clip is set, and there is no clear_clip(8) */
    blit_copy_own(far_rd(DGROUP, DS_car_handles));       /* dash */
    blit_copy_own(far_rd(DGROUP, DS_spr_roof));
    gfx_set_clip(gfx_screen_desc(), 0, 0x50, 0x13, 200);
#else
    gfx_set_clip(gfx_screen_desc(), 0, 0x28, 0x13, 200);
    gfx_clear_clip(8);
    blit_copy_own(far_rd(DGROUP, DS_car_handles));       /* dash */
    blit_copy_own(far_rd(DGROUP, DS_spr_roof));
#endif
    DSW(DS_g_stageTime) = 0;
    for (;;) {
        host_frame_begin();                               /* PORT: ticks (and the sim ISR) advance here; paced
                                                             to the emulated original frame rate */
        snapshot_sim_state();
        project_road_main();
        project_road_mirror();
        select_road_buffer();
#if !TD_CGA
        gfx_clear_clip(8);                                /* TDCGA 0x1DC1: none */
#endif
        fill_scenery_above_road();
        draw_road_main();
        draw_mirror_background();
        draw_road_mirror();
        draw_buffer_overlays();
        present_road_buffer();
        draw_dashboard_dynamic();
        draw_gear_box();
        rand8();
        if (DSW(DS_demo_mode) != 0 && DSS(DS_g_stageTime) > 0x2D0) { r = -1; break; }   /* signed jg */
        s8 ev = DSC(DS_g_stageEvent);
        if (ev == 0) continue;
        if (ev == 1) { r = -1; break; }                   /* Esc / any key in demo */
        if (ev <= 2) {                                    /* signed byte jg: >= 0x80 also lands here */
            if (DSW(DS_g_carSpeedFixed) != 0) continue;   /* wait until the car has stopped */
            snd_stop_all();
            DSW(DS_g_avgSpeed) = (u16)(DSW(DS_road_pos) - DSW(DS_STAGE_START + DSW(DS_g_stage) * 2) - 0x2D);
            if (DSW(DS_g_stage) == 4) dealership_ending();
            r = 1;
            break;
        }
        /* ev >= 3: crash */
        snd_stop_all();
        DSB(DS_clock_running) = 0;
        DSW(DS_g_stageTime) = (u16)(DSW(DS_g_stageTime) + 0xF0);
        snapshot_sim_state();
        crash_windscreen_sequence();
        DSW(DS_g_lives) = (u16)(DSW(DS_g_lives) - 1);
        if (DSW(DS_g_lives) == 0) {
#if !TD_CGA
            blit_and_own(far_rd(DGROUP, DS_spr_gvrm));    /* GAME OVER mask (TDCGA 0x1E66: none) */
#endif
            blit_or_own(far_rd(DGROUP, DS_spr_govr));
            wait_fire_button();
            r = 0;
            break;
        }
        reset_car_state();
        snd_set_loop(far_make(DGROUP, DS_SONG_INGAME));
    }
    snd_stop_all();
    /* the three descriptors are offsets into the code-segment pool (push cs; push [DS:08B2]) — LIFO */
    gfx_free_buffer(far_make(CODE_SEG, DSW(DS_buf_desc_c)));
    gfx_free_buffer(far_make(CODE_SEG, DSW(DS_buf_desc_b)));
    gfx_free_buffer(far_make(CODE_SEG, DSW(DS_road_buf_desc)));
    /* PORT: int 21h/25h restores of int 0 (DS:0934) and int 8 (CS:3B18) replaced by removing the ISR. */
    timer_set_driving_isr(NULL);
    timer_restore();                                      /* 0x694A: back to the BIOS timer */
    return r;
}
