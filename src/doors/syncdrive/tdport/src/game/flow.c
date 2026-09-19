/* game_flow: main (0x0010), CARS.TXT, play-again menu, intro screens — port/spec/game_flow.md §4. */
#include <stdlib.h>
#include <string.h>

#include "flow.h"
#include "../host.h"
#include "../platform/gfx.h"
#include "../platform/input.h"
#include "../platform/res.h"
#include "../platform/timer.h"

/* ================================================================================================
 * Host-side stdio helpers (the original uses MSC text-mode stdio; see game_flow.md §6).
 * ============================================================================================== */

FILE *flow_fopen_game(const char *name, bool create, const char *mode)
{
    char *path = host_game_path(name, create);
    if (!path) return NULL;
    FILE *f = fopen(path, mode);
    host_free(path);
    return f;
}

bool flow_text_open(FlowText *t, const char *name)
{
    t->data = NULL; t->len = t->pos = 0;
    FILE *f = flow_fopen_game(name, false, "rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size < 0) size = 0;
    unsigned char *raw = malloc((size_t)size + 1);
    t->data = malloc((size_t)size + 1);
    if (!raw || !t->data) { free(raw); free(t->data); t->data = NULL; fclose(f); return false; }
    size_t n = fread(raw, 1, (size_t)size, f);
    fclose(f);
    /* MSC text mode: CR LF -> LF, ^Z ends the file. */
    for (size_t i = 0; i < n; i++) {
        if (raw[i] == 0x1A) break;
        if (raw[i] == '\r' && i + 1 < n && raw[i + 1] == '\n') continue;
        t->data[t->len++] = raw[i];
    }
    free(raw);
    return true;
}

char *flow_text_gets(char *s, int n, FlowText *t)
{
    int i = 0;
    if (n <= 1) return NULL;
    while (i < n - 1 && t->pos < t->len) {
        char c = (char)t->data[t->pos++];
        s[i++] = c;
        if (c == '\n') break;
    }
    if (i == 0) return NULL;
    s[i] = 0;
    return s;
}

static bool text_is_space(int c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f'; }

static void text_skip_ws(FlowText *t)
{
    while (t->pos < t->len && text_is_space(t->data[t->pos])) t->pos++;
}

static bool text_scan_int16(FlowText *t, int *out)
{
    text_skip_ws(t);
    bool neg = false;
    size_t p = t->pos;
    if (p < t->len && (t->data[p] == '-' || t->data[p] == '+')) { neg = t->data[p] == '-'; p++; }
    if (p >= t->len || t->data[p] < '0' || t->data[p] > '9') return false;
    u16 v = 0;
    while (p < t->len && t->data[p] >= '0' && t->data[p] <= '9') v = (u16)(v * 10u + (t->data[p++] - '0'));
    t->pos = p;
    *out = (s16)(neg ? (u16)-v : v);   /* MSC int is 16 bits */
    return true;
}

int flow_text_scan_2d(FlowText *t, int *a, int *b)
{
    if (!text_scan_int16(t, a)) return 0;
    if (!text_scan_int16(t, b)) return 1;   /* the ' ' directive skips whitespace (done inside) */
    text_skip_ws(t);                        /* the '\n' directive skips all following whitespace */
    return 2;
}

void flow_text_close(FlowText *t)
{
    free(t->data);
    t->data = NULL; t->len = t->pos = 0;
}

int flow_find_list(FarPtr arc, char *names, FarPtr *out, int max)
{
    /* 0x94C7: for (; *names; names += 4) out[i++] = res_find(arc, names); */
    int i = 0;
    for (; *names; names += 4) {
        if (i >= max) break;   /* PORT: the original overruns its stack array */
        out[i++] = res_find(arc, names);
    }
    return i;
}

/* ================================================================================================
 * 0x0010 main — game_flow.md §4 main (verified against disassembly 0x0010-0x0242)
 * ============================================================================================== */
int game_main(void)
{
    int r;
    u16 need;

    gfx_init();                 /* PORT: port setup */
    timer_init();

    /* PORT: copy_protection_check() (0x8DC7) dropped, treated as 0.
     * PORT: the TD.EXE launcher password (DS:00DE) check dropped (TDCGA has none).
     * PORT: argv[1] "herc" is the build variant: TD_HERC builds act as "tdcga herc", others as no argument. */
#if TD_HERC
    gfx_herc_init();                                    /* TDCGA 0x0036 */
    DSW(DS_herc_mode) = 1;
#endif
    mem_init();
    gfx_init_ega();
#if !TD_CGA
    gfx_set_palette(DS_pal_game);                       /* 0x00CC (TDCGA: no palette) */
#endif
    input_set_mode(4);
    timer_install_menu();
    FarPtr snd = load_raw_archive(DSTR(0x45), EGA_CGA(0x26D1, 0x153D));  /* "tdsnd.snd" */
    far_wr(DGROUP, DS_g_songGas,     res_find(snd, DSTR(0x4F)));   /* sng1 */
    far_wr(DGROUP, DS_g_songIntro,   res_find(snd, DSTR(0x54)));   /* sng2 */
    far_wr(DGROUP, DS_g_songCarSel,  res_find(snd, DSTR(0x59)));   /* sng4 */
    far_wr(DGROUP, DS_g_songHiScore, res_find(snd, DSTR(0x5E)));   /* sng3 */
#if TD_CGA
    {   /* TDCGA 0x00C0: xroada.cmp is loaded once here, into the DGROUP buffer DS:73AA */
        u16 len;
        far_wr(DGROUP, DS_g_xroadA, load_packed_near(DSTR(0x63), 0x73AA, &len));
    }
#endif
    far_wr(DGROUP, DS_page_buf_desc, gfx_create_buffer(EGA_CGA(0x28, 0x50), 200, 0x0F));
    DSL(DS_g_totalScore) = 0;
    DSW(DS_g_selectedCar) = 0xFFFF;
    DSW(DS_demo_mode) = 0;
    load_cars_txt();
    kbd_flush();

intro:
    DSW(DS_demo_mode) = 0;
    r = run_intro();
    if (r == 1) goto dos;
    if (r != -1) goto decide;
    DSW(DS_demo_mode) = 1;
scores:
    r = high_scores((s32)DSL(DS_g_totalScore));
    DSL(DS_g_totalScore) = 0;
    if (r == 1) goto dos;
    DSW(DS_demo_mode) = (r == -1) ? 1 : 0;
decide:
    need = (u16)((DSS(DS_g_selectedCar) == -1 ? 1 : 0) | DSW(DS_demo_mode) | DSW(DS_g_carAutoSelected));
    if (need == 0) {
        need = (u16)play_again_menu();
        if (need == 0xFFFF) goto dos;
    }
    if (need != 0) {
        load_cars_txt();                                 /* called with a dummy argument 1 */
        r = car_select();
        if (r == 1) goto dos;
        DSW(DS_demo_mode) = (r == -1) ? 1 : 0;
    }
    DSL(DS_g_totalScore) = 0;
    r = run_game();
    if (r == 1 || r == -1) goto intro;
    if (r == 0) goto scores;
dos:
    /* PORT: copy_protection_check() here (non-zero would quit) dropped. */
    gfx_clear_screen(0);
    gfx_select_target(gfx_screen_desc());
    gfx_set_text_colours(EGA_CGA(0x0F, 2), 1);
    draw_text_centered(DSTR(EGA_CGA(0x63, 0x6E)), 100);  /* " BACK TO DOS (Y or other key) ? " */
    if (toupper_c(getkey_wait()) != 'Y') goto intro;

#if TD_CGA
    if (DSW(DS_herc_mode)) gfx_herc_shutdown();        /* TDCGA 0x0225: set only in TD_HERC builds */
    else
#endif
    /* PORT: in the EGA build DS:008A (Hercules) is never set, so always the EGA shutdown. */
    gfx_shutdown();
    timer_restore();
    return 0;
}

/* 0x0243 load_cars_txt — game_flow.md §4 (verified) */
void load_cars_txt(void)
{
    FlowText f;
    if (!flow_text_open(&f, DSTR(EGA_CGA(0xF5, 0x101)))) /* "CARS.TXT", mode "r" */
        fatal("CARS.TXT open error");                    /* PORT: the original does not check fopen */
    DSW(DS_g_numCars) = 0;
    u16 off = 0;
    do {
        if (!flow_text_gets(DSTR(DS_g_carNameBuf + off), 30, &f)) break;
        u16 n = DSW(DS_g_numCars);
        DSW(DS_g_numCars) = (u16)(n + 1);
        DSW(DS_g_carNames + n * 2) = (u16)(DS_g_carNameBuf + off);
        off = (u16)(off + strlen(DSTR(DS_g_carNameBuf + off)));
        DSB(DS_g_carNameBuf + off - 1) = 0;              /* kill '\n' (or the last char) */
    } while (DSS(DS_g_numCars) < 10);
    flow_text_close(&f);
}

/* 0x02BF play_again_menu — game_flow.md §4 (verified; y = 0x5A / 100, strings DS:008C / DS:00AC) */
int play_again_menu(void)
{
    gfx_clear_screen(0);
    for (;;) {
        int sel = 0;
        for (;;) {
            u16 a, b;
            if (sel == 0) { a = 0x00; b = EGA_CGA(0x0F, 3); }
            else          { a = EGA_CGA(0x0F, 3); b = 0x00; }
            gfx_set_text_colours(a, b);
            draw_text_centered(DSTR(EGA_CGA(0x8C, 0x98)), 0x5A);
            gfx_set_text_colours(b, a);
            draw_text_centered(DSTR(EGA_CGA(0xAC, 0xB8)), 100);
            set_deadline(12000);
            int k = menu_key();
            if (k == 0x0D) { DSW(DS_demo_mode) = 0; return sel; }
            if (k == -1)   { DSW(DS_demo_mode) = 1; return sel; }
            if (k == 0x4800) break;                      /* Up -> sel = 0 */
            if (k == 0x5000) sel = 1;
            else if (k == 1) return -1;
        }
    }
}

/* 0x037A run_intro — game_flow.md §4 (verified) */
int run_intro(void)
{
    gfx_clear_screen(0);
    snd_play_oneshot(far_rd(DGROUP, DS_g_songIntro));
    far_wr(DGROUP, DS_g_introArchive, load_archive(DSTR(EGA_CGA(0xFE, 0x10A)), EGA_CGA(2000, 1000)));   /* ACCOLADE.PES (.CMP) */
    int r = intro_accolade();
    if (r == -1) {
        far_wr(DGROUP, DS_g_introArchive, load_archive(DSTR(EGA_CGA(0x10B, 0x117)), EGA_CGA(2000, 1000))); /* TESTDRV.PES (.CMP) */
        r = intro_testdrive_car();
    }
    if (r == -1) r = intro_testdrive_logo();
    snd_stop_oneshot();
    /* PORT: if (copy_protection_check()) r = 1; dropped. */
    return r;
}

/* 0x03F4 intro_accolade — game_flow.md §4 (verified) */
int intro_accolade(void)
{
    FarPtr a = far_rd(DGROUP, DS_g_introArchive);
    int k;
    gfx_select_target(flow_page_desc());
    gfx_clear_clip(0);
    blit_copy_own(res_find(a, DSTR(EGA_CGA(0x1CD, 0x1D9))));             /* "acc " */
    blit_copy_own(res_find(a, DSTR(EGA_CGA(0x1D2, 0x1DE))));             /* "pres" */
    blit_copy_own(res_find(a, DSTR(EGA_CGA(0x1D7, 0x1E3))));             /* "copy" */
    gfx_select_target(gfx_screen_desc());
    for (int i = 0; i < 8; i++) {
        set_deadline(1);
        gfx_dissolve(flow_page_sprite(), (u8)i);
        k = menu_key();
        if (k != -1) return k;
    }
    FarPtr bull = res_find(a, DSTR(EGA_CGA(0x1DC, 0x1E8)));              /* "bull" */
    s16 y    = (s16)rd16(bull.seg, (u16)(bull.off + 0x0A));
    s16 xend = (s16)rd16(bull.seg, (u16)(bull.off + 0x08));
    for (s16 x = 0; x < xend; x = (s16)(x + 2)) {
        set_deadline(1);
        blit_copy_clip_raw(bull, x, y);                  /* not erased: leaves a trail */
        k = menu_key();
        if (k != -1) return k;
    }
    set_deadline(100);
    return menu_key();
}

/* 0x051B intro_testdrive_car — game_flow.md §4 (verified) */
int intro_testdrive_car(void)
{
    FarPtr a = far_rd(DGROUP, DS_g_introArchive);
    FarPtr frm[5], rrm[5], wnd[33];
    int k;
    flow_find_list(a, DSTR(EGA_CGA(0x11C, 0x128)), frm, 5);
    flow_find_list(a, DSTR(EGA_CGA(0x132, 0x13E)), rrm, 5);
    flow_find_list(a, DSTR(EGA_CGA(0x148, 0x154)), wnd, 33);
    gfx_select_target(flow_page_desc());
    gfx_clear_clip(0);
    blit_copy_own(res_find(a, DSTR(EGA_CGA(0x1E1, 0x1ED))));             /* "car " */
    gfx_select_target(gfx_screen_desc());
    for (int i = 0; i < 8; i++) {
        set_deadline(10);
        gfx_dissolve(flow_page_sprite(), (u8)i);
        k = menu_key();
        if (k != -1) return k;
    }

    s16 done = 0, moving = 0, dist = 0, vel = 0, wf = 0;
    FarPtr pg = flow_page_sprite();
    do {
        if (done) return -1;
        gfx_select_target(flow_page_desc());
        set_deadline(4);
        if (moving == 1) {
            s16 fi = (s16)(((dist + 4) / 5) % 5);
            blit_copy_own(rrm[dist % 5]);
            blit_copy_own(frm[fi]);
            dist = (s16)(dist + (vel >> 4));
            vel = (s16)(vel + 10);
            done = dist > 0x13F;
            set_deadline(4);
        }
        if (moving == 0 || wf < 0x21) {
            blit_copy_own(wnd[wf]);
            wf++;
            if (wf < 0x1A) set_deadline(20);
            else moving = 1;
        }
        gfx_select_target(gfx_screen_desc());
        gfx_set_clip(gfx_screen_desc(), 0, EGA_CGA(0x28, 0x50), 100, 200);
        s16 w = dist;
        if (dist > 0x20) w = 0x20;
        if (w != 0) gfx_fill_rect((s16)(0x140 - dist), 100, w, 0x50, 0);
        blit_copy_clip_raw(pg, (s16)(0 - dist), 0);      /* whole page shifted left */
        k = menu_key();
    } while (k == -1);
    return k;
}

/* 0x0792 intro_testdrive_logo — game_flow.md §4 (verified against disassembly 0x0792-0x098B) */
int intro_testdrive_logo(void)
{
    FarPtr a = far_rd(DGROUP, DS_g_introArchive);
    FarPtr logo = res_find(a, DSTR(EGA_CGA(0x1E6, 0x1F2)));              /* "tdrv" */
    s16 lx = (s16)rd16(logo.seg, (u16)(logo.off + 8));
    s16 ly = (s16)rd16(logo.seg, (u16)(logo.off + 0x0A));
    gfx_select_target(gfx_screen_desc());
    gfx_set_clip(gfx_screen_desc(), 0, 0x50, 0, 200);    /* x1 = 0x50 as in the original */

    s16 phase = 0, settle = 0, accel = 0x30C, y = (s16)0xFF3D, damp = 7, finished = 0;
    s32 vel = 0;
    for (;;) {
        if (phase == 0) {
            y = (s16)(y + (s16)(vel >> 12));             /* 0xC954 long sar, low word */
            vel += (s32)accel;
            if (y > 0) {
                accel = (s16)(u16)((u16)accel * 0xFFF9u);  /* imul -7, low word */
                phase = 1;
                vel = (s32)(0u - (u32)vel);
            }
        } else if (phase == 1) {
            s16 prev = y;
            y = (s16)(y + (s16)(vel >> 12));
            vel += (s32)accel;
            if ((y >= 0 && prev < 0) || (y < 0 && prev >= 0)) {
                vel = vel / (s32)damp;                   /* 0xC95F signed long divide-assign */
                damp++;
                accel = (s16)(0 - accel);
            }
            if (y == 0 && vel < 0xDAC && vel > -0xDAC) phase = 2;
        } else {
            y = 0;
            settle++;
            finished = settle > 5;
        }
        set_deadline(10);
        blit_copy_clip_raw(logo, lx, (s16)(ly + y));
        int k = menu_key();
        if (k != -1) return k;
        if (finished) {
            blit_copy_own(res_find(a, DSTR(EGA_CGA(0x1EB, 0x1F7))));     /* "fob " */
            blit_copy_own(res_find(a, DSTR(EGA_CGA(0x1F0, 0x1FC))));     /* "tmar" */
            gfx_set_text_colours(EGA_CGA(0x0F, 3), 0);
            draw_text_centered(DSTR(EGA_CGA(0x1F5, 0x201)), 0xB4);       /* CTRL - (J)OYSTICK OR CTRL - (K)EYBOARD */
            set_deadline(1000);
            return menu_key();
        }
    }
}
