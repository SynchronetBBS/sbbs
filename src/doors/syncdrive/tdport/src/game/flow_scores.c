/* game_flow: high-score table (SCORES) — port/spec/game_flow.md §4, FORMATS.md "SCORES". */
#include <stdio.h>
#include <string.h>

#include "flow.h"
#include "../platform/gfx.h"
#include "../platform/input.h"
#include "../platform/res.h"
#include "../platform/timer.h"

/* 0x12DB high_scores — game_flow.md §4 (verified) */
int high_scores(s32 new_score)
{
    /* original: char names[160], cars[160]; long scores[8] (uninitialised stack). One spare row here. */
    char names[9 * 20], cars[9 * 20];
    s32 scores[9];
    memset(names, 0, sizeof names);
    memset(cars, 0, sizeof cars);
    memset(scores, 0, sizeof scores);

    scores_load(names, scores, cars);
    if (new_score > scores[7]) {                         /* signed 32-bit, strictly greater */
        scores_enter_name(new_score, DSS(DS_g_selectedCar), names, scores, cars);
        scores_save(names, scores, cars);
    }
    int r = scores_show(names, scores, cars);
    if (r == -1) r = credits_show();
    return r;
}

/* 0x1369 scores_load — game_flow.md §4 (verified against disassembly 0x1369-0x1481) */
void scores_load(char *names, s32 *scores, char *cars)
{
    char line1[80], line2[80];
    u16 crc_read = 0xFFFF;   /* TODO(verify): uninitialised stack word in the original if the first "%x" fails */
    int n = 0;
    FlowText f;
    memset(line1, 0, sizeof line1);
    memset(line2, 0, sizeof line2);

    if (!flow_text_open(&f, DSTR(EGA_CGA(0x2BE, 0x2B4)))) {              /* "SCORES", "r" */
        /* PORT: the original calls fatal("SCORE file open error"); the port starts from the padded
         * default table below (the file is created by scores_save when a score qualifies). */
    } else {
        do {
            if (!flow_text_gets(line1, 0x3C, &f)) break;
            flow_text_gets(line2, 0x3C, &f);
            unsigned v;
            if (sscanf(line2, "%x", &v) == 1) crc_read = (u16)v;
            u16 crc = crc8_b8((const u8 *)line1, (int)strlen(line1), (u8)n);   /* seed = valid rows so far */
            if (crc == crc_read) {
                long sc = scores[n];
                sscanf(line1, "%20c%20c%ld", names + n * 20, cars + n * 20, &sc);  /* %20c: no NUL */
                scores[n] = (s32)sc;
                n++;
            }
        } while (n < 8);
        flow_text_close(&f);
    }
    for (; n < 8; n++) {
        names[n * 20] = ' '; names[n * 20 + 1] = 0;
        cars[n * 20]  = ' '; cars[n * 20 + 1]  = 0;
        scores[n] = 0;
    }
}

/* 0x1482 scores_enter_name — game_flow.md §4 (verified against disassembly 0x1482-0x162D) */
void scores_enter_name(s32 score, int car, char *names, s32 *scores, char *cars)
{
    char name[20];
    memset(name, 0, sizeof name);
    gfx_clear_screen(0);
    snd_play_oneshot(far_rd(DGROUP, DS_g_songHiScore));
    gfx_select_target(gfx_screen_desc());
    FarPtr ll = load_archive(DSTR(EGA_CGA(0x2EA, 0x2E0)), EGA_CGA(2000, 1000)); /* "llogo.pes" (.cmp) */
    blit_copy_own(res_find(ll, flow_car_name(car)));      /* 4-char match "coun", "lotu", ... */
    gfx_set_text_colours(3, 0);
    draw_text_centered(DSTR(EGA_CGA(0x2F4, 0x2EA)), 0x96);                /* "You have qualified as one" */
    draw_text_centered(DSTR(EGA_CGA(0x30E, 0x304)), 0xA0);                /* "of Test Drive's best drivers." */
    gfx_draw_text(DSTR(EGA_CGA(0x32C, 0x322)), 0x14, 0xB4);               /* "Enter your name:" */
    draw_rect_outline(0xAC, 0xAF, 0x13C, 0xBE, 0xFF);     /* colour 0xFFFF */
    text_input_line(name, 15, 0xB8, 0xB4, 3000);
    if (name[0] != 0) {
        int i, j;
        for (i = 0; i < 8; i++)
            if (scores[i] < score) break;                 /* hi signed, lo unsigned = signed long < */
        for (j = 6; j >= i; j--) {
            strncpy(names + (j + 1) * 20, names + j * 20, 20);
            strncpy(cars + (j + 1) * 20, cars + j * 20, 20);
            scores[j + 1] = scores[j];
        }
        strncpy(names + i * 20, name, 20);
        strncpy(cars + i * 20, flow_car_name(car), 20);
        scores[i] = score;
    }
    snd_stop_oneshot();
}

/* 0x162E scores_save — game_flow.md §4 (verified) */
void scores_save(char *names, s32 *scores, char *cars)
{
    FILE *f = flow_fopen_game(DSTR(EGA_CGA(0x33F, 0x335)), true, "wb");    /* "SCORES", "w" */
    if (!f) fatal("%s", DSTR(EGA_CGA(0x362, 0x358)));                     /* "SCORE file save error" */
    for (int i = 0; i < 8; i++) {
        char line[80];
        int len = snprintf(line, sizeof line, "%-20.20s%-20.20s%ld\n",
                           names + i * 20, cars + i * 20, (long)scores[i]);
        if (len < 0) len = 0;
        if (len > (int)sizeof line - 1) len = (int)sizeof line - 1;
        u8 c = crc8_b8((const u8 *)line, len, (u8)i);     /* CRC over the line with '\n' only */
        /* PORT: text-mode fprintf writes "\n" as CR LF; written explicitly in binary mode. */
        if (len > 0 && line[len - 1] == '\n') line[len - 1] = 0;
        fprintf(f, "%s\r\n", line);
        fprintf(f, "%x\r\n", (unsigned)c);
    }
    fclose(f);
}

/* 0x16D7 scores_show — game_flow.md §4 (verified against disassembly 0x16D7-0x186D) */
int scores_show(char *names, s32 *scores, char *cars)
{
    char buf[80];
    gfx_clear_screen(0);
    gfx_set_text_colours(EGA_CGA(0x0C, 1), 0);
    draw_text_centered(DSTR(EGA_CGA(0x378, 0x36E)), 0);                   /* "TEST DRIVE'S BEST" */
    gfx_set_text_colours(EGA_CGA(0x0F, 3), 0);
    FarPtr sl = load_archive(DSTR(EGA_CGA(0x38A, 0x380)), EGA_CGA(2000, 1000)); /* "slogo.pes" (.cmp) */
    int i;
    for (i = 0; i < 4; i++) {
        char *car = cars + i * 20;
        if (*car != ' ') {
            res_find(sl, car);                            /* result discarded */
            blit_copy_hot(res_find(sl, car), 0x22, (s16)(i * 0x23 + 0x14));
        }
        snprintf(buf, sizeof buf, "%7ld  %-20.20s", (long)scores[i], names + i * 20);
        gfx_draw_text(buf, 0x5A, (s16)(i * 0x23 + 0x0F));
    }
    for (; i < 8; i++) {
        snprintf(buf, sizeof buf, "%7ld  %-20.20s", (long)scores[i], names + i * 20);
        gfx_draw_text(buf, 0x5A, (s16)(i * 10 + 0x69));
    }
    if (DSW(DS_demo_mode) != 0) {
        draw_text_centered(DSTR(EGA_CGA(0x3B2, 0x3A8)), 0xBE);            /* CTRL - (J)OYSTICK OR CTRL - (K)EYBOARD */
    } else if ((s32)DSL(DS_g_totalScore) > 0) {
        snprintf(buf, sizeof buf, "Your Score : %ld", (long)(s32)DSL(DS_g_totalScore));
        draw_text_centered(buf, 0xBE);
    }
    kbd_flush();
    set_deadline(2000);
    return menu_key();
}
