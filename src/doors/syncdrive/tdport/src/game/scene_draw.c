/* scene_render: road buffer drawing — spans, scenery split, objects and the deferred draw list.
 * Port of TDEGA image 0x28C5..0x349C (port/spec/scene_render.md §4.6–4.7).
 * Spans write plane bytes of the current target (the road buffer) in mem[] like the asm. */
#include "scene.h"

/* ------------------------------------------------------------------------------------------------
 * Explicit replacement of the CPU stack used by 0x28C5 / 0x2F7A to defer object draws.
 * Words are pushed in exactly the asm order; popping mirrors the pop loops at 0x2DCA / 0x32E4. */
#define FRAME_STACK_WORDS 16384
typedef struct { u16 w[FRAME_STACK_WORDS]; int n; } FrameStack;

static void fs_push(FrameStack *s, u16 v)
{
    if (s->n < FRAME_STACK_WORDS) s->w[s->n++] = v;   /* PORT: bounded; the list never gets near this */
}
static u16 fs_pop(FrameStack *s) { return s->n > 0 ? s->w[--s->n] : 0xFFFF; }

/* push y, x, [or+2], [or], y, x, [and+2], [and], clip_x1, clip_y1 */
static void fs_push_frame(FrameStack *s, u16 x, u16 y, u16 or_addr, u16 and_addr, u16 clipx, u16 clipy)
{
    fs_push(s, y); fs_push(s, x); fs_push(s, DSW((u16)(or_addr + 2))); fs_push(s, DSW(or_addr));
    fs_push(s, y); fs_push(s, x); fs_push(s, DSW((u16)(and_addr + 2))); fs_push(s, DSW(and_addr));
    fs_push(s, clipx); fs_push(s, clipy);
}

/* call blitter with [sp]=off,[sp+2]=seg,[sp+4]=x,[sp+6]=y ; add sp,8 */
static void fs_blit(FrameStack *s, void (*blit)(FarPtr, s16, s16))
{
    FarPtr spr;
    spr.off = fs_pop(s);
    spr.seg = fs_pop(s);
    s16 x = (s16)fs_pop(s);
    s16 y = (s16)fs_pop(s);
    blit(spr, x, y);
}

/* ------------------------------------------------------------------------------------------------ */
typedef struct { u16 es, di, dx; } SpanPos;   /* ES plane segment, DI byte ptr, DX byte column */

/* DGROUP sprite-handle table entries and scratch used by the road drawing. */
#define H_POLES        EGA_CGA(0x0FA7, 0x0F7E)   /* XROADA pal0..3; pol0..3 at +0x10 */
#define H_SCENERY      EGA_CGA(0x0FC7, 0x0F9E)
#define H_CLIFF_AND    EGA_CGA(0x0F9F, 0x0F76)   /* clfa */
#define H_CLIFF_OR     EGA_CGA(0x0FA3, 0x0F7A)   /* clfo */
#define H_MCLIFF_AND   EGA_CGA(0x1027, 0x0FFE)   /* mcfa */
#define H_MCLIFF_OR    EGA_CGA(0x102B, 0x1002)   /* mcfo */
#define H_SIGNS        EGA_CGA(0x1043, 0x1016)
#define H_POSTS        EGA_CGA(0x1123, 0x10F6)
#define H_HAZARD       EGA_CGA(0x1133, 0x1106)
#define H_TRAFFIC_OR   EGA_CGA(0x1173, 0x1146)
#define H_TRAFFIC_AND  EGA_CGA(0x1177, 0x114A)
#define H_COP_MAIN     EGA_CGA(0x132B, 0x12FE)
#define H_COP_FAR      EGA_CGA(0x134B, 0x131E)
#define H_COPB         EGA_CGA(0x137B, 0x134E)
#define H_COPM         EGA_CGA(0x137F, 0x1352)
#define H_COP_MIRROR   EGA_CGA(0x1303, 0x12D6)
#define DS_COP_ROW_SMOOTH EGA_CGA(0x1447, 0x140E)
#define DS_TRAFFIC_L0  EGA_CGA(0x09AD, 0x0995)   /* r_traffic_slots: lane 0 list ... */
#define DS_TRAFFIC_L1  EGA_CGA(0x09D5, 0x09BD)   /* ... lane 1 list */
#define DS_ROW_SY_39   EGA_CGA(0x1BC1, 0x1B94)   /* row_sy[39] */

#if TD_CGA
/* TDCGA road buffer: 2 bits per pixel; spans are written as words (8 pixels) of a 4-colour pattern. */
#define DS_CGA_MASK_LEFT    0x1410   /* u16[8]: pixels b..7 of a word */
#define DS_CGA_MASK_RIGHT   0x1420   /* u16[8]: pixels 0..b of a word */
#define DS_CGA_PIXEL_MASK   0x1430   /* u8[4]: one pixel of a byte */
#define DS_CGA_SPAN_PATTERN 0x1434   /* u16: pattern of the current span */
#define CGA_PAT_SCENERY  0x5555
#define CGA_PAT_SHOULDER 0xAAAA
#define CGA_PAT_ROAD     0x0000
static inline u16 cga_pat_grass(u16 odd) { return odd ? 0x2222 : 0x8888; }   /* 0x8888 >> 2 on odd rows */

static u16 cga_stosw(u16 es, u16 di, u16 w, u16 n)
{
    while (n--) { wr16(es, di, w); di = (u16)(di + 2); }
    return di;
}

/* TDCGA 0x2DF2 fill_span_to / 0x32EF fill_span_to_mirror: AX = x_end, BX = pattern. Returns ZF (DX == 40). */
static bool fill_span_to_cga(SpanPos *p, u16 ax, u16 pattern, bool mirror)
{
    u16 bx;
    if (!mirror) {
        if ((s32)(s16)ax - 1 < 0) return false;          /* dec ax ; jl */
        ax--;
    } else {
        ax--;
        if ((s16)ax < 0xF0) return false;                /* dec ax ; cmp ax,0F0h ; jl */
    }
    DSW(DS_CGA_SPAN_PATTERN) = pattern;
    if ((s16)ax >= 0x140) { ax = 0x27; bx = 7; }
    else { bx = (u16)(ax & 7); ax = (u16)((s16)ax >> 3); }
    if ((s32)(s16)ax - (s32)(s16)p->dx < 0) return false;
    s16 cx = (s16)(u16)(ax - p->dx);
    u16 w = (u16)((DSW((u16)(DS_CGA_MASK_LEFT + (u16)(DSW(DS_span_bit) << 1))) & DSW(DS_CGA_SPAN_PATTERN))
                  | rd16(p->es, p->di));
    cx--;
    if (cx >= 0) {
        wr16(p->es, p->di, w); p->di = (u16)(p->di + 2); p->dx++;
        w = DSW(DS_CGA_SPAN_PATTERN);
        while (cx != 0) { wr16(p->es, p->di, w); p->di = (u16)(p->di + 2); p->dx++; cx--; }
        wr16(p->es, p->di, 0);
    }
    w &= DSW((u16)(DS_CGA_MASK_RIGHT + (bx << 1)));
    if (!mirror) wr16(p->es, p->di, (u16)(rd16(p->es, p->di) | w));   /* main: ORed (0x2E3E) */
    else         wr16(p->es, p->di, w);                                /* mirror: assigned (0x3347) */
    bx++;
    if (bx == 8) {
        p->di = (u16)(p->di + 2); p->dx++; bx = 0;
        if (p->dx != 0x28) wr16(p->es, p->di, 0);
    }
    DSW(DS_span_bit) = bx;
    return p->dx == 0x28;
}

/* TDCGA 0x2D58 fill_road_scanlines_main / 0x3259 fill_road_scanlines_mirror */
static void fill_road_scanlines_cga(bool mirror)
{
    const u16 min_ol_sy = mirror ? DS_min_ol_sy_mirror : DS_min_ol_sy;
    const u16 min_ol    = mirror ? DS_min_ol_mirror : DS_min_ol;
    const u16 s_ol = mirror ? DS_span_ol_mirror : DS_span_ol;
    const u16 s_l  = mirror ? DS_span_l_mirror  : DS_span_l;
    const u16 s_r  = mirror ? DS_span_r_mirror  : DS_span_r;
    const u16 s_or = mirror ? DS_span_or_mirror : DS_span_or;
    SpanPos p;
    p.es = scene_road_seg();

    DSB(min_ol_sy) = (u8)(DSB(min_ol_sy) << 1);            /* byte doubling in place (quirk) */
    u16 si = (u16)(DSW(mirror ? DS_top_sy_mirror : DS_top_sy) << 1);
    do {
        DSW(DS_span_bit) = 0;
        if (!mirror) {
            p.di = scene_rowtab(si);
            p.dx = 0;
        } else {
            p.di = (u16)(scene_rowtab((u16)(si + 0x36)) + 0x3C);
            p.dx = 0x1E;
        }
        wr16(p.es, p.di, 0);
        u16 pat = CGA_PAT_SCENERY;
        if ((s16)si > (s16)DSW(min_ol_sy) && (s16)DSW(min_ol) < (s16)DSW((u16)(s_ol + si))) {
            if (fill_span_to_cga(&p, DSW(min_ol), pat, mirror)) goto next;
            pat = cga_pat_grass(si & 2);
        }
        if (fill_span_to_cga(&p, DSW((u16)(s_ol + si)), pat, mirror)) goto next;
        if (fill_span_to_cga(&p, DSW((u16)(s_l + si)), CGA_PAT_SHOULDER, mirror)) goto next;
        if (fill_span_to_cga(&p, DSW((u16)(s_r + si)), CGA_PAT_ROAD, mirror)) goto next;
        if (fill_span_to_cga(&p, DSW((u16)(s_or + si)), CGA_PAT_SHOULDER, mirror)) goto next;
        fill_span_to_cga(&p, 0x140, cga_pat_grass(si & 2), mirror);
    next:
        si = (u16)(si + 2);
    } while (si != (mirror ? 0x22 : 0xDE));
}
#endif

#if !TD_CGA
/* 0x2E8B fill_span_to / 0x339E fill_span_to_mirror — §4.6. AX = x_end. Returns ZF (DX == 40). */
static bool fill_span_to(SpanPos *p, u16 ax, bool mirror)
{
    u16 bx;
    if (!mirror) {
        if ((s32)(s16)ax - 1 < 0) return false;          /* dec ax ; jl */
        ax--;
    } else {
        ax--;
        if ((s16)ax < 0xF0) return false;                /* dec ax ; cmp ax,0F0h ; jl */
    }
    if ((s16)ax >= 0x140) { ax = 0x27; bx = 7; }
    else { bx = (u16)(ax & 7); ax = (u16)((s16)ax >> 3); }
    if ((s32)(s16)ax - (s32)(s16)p->dx < 0) return false;
    s16 cx = (s16)(u16)(ax - p->dx);
    u8 al = (u8)(DSB((u16)(DS_mask_left_from_bit + DSW(DS_span_bit))) | rd8(p->es, p->di));
    cx--;
    if (cx >= 0) {
        wr8(p->es, p->di, al); p->di++; p->dx++;
        al = 0xFF;
        while (cx != 0) { wr8(p->es, p->di, al); p->di++; p->dx++; cx--; }
    }
    al &= DSB((u16)(DS_mask_right_to_bit + bx));
    wr8(p->es, p->di, al);                               /* last byte is assigned, not ORed */
    bx++;
    if (bx == 8) { p->di++; p->dx++; bx = 0; }
    DSW(DS_span_bit) = bx;
    return p->dx == 0x28;
}

/* 0x2DEE fill_road_scanlines_main / 0x3306 fill_road_scanlines_mirror — §4.6 */
static void fill_road_scanlines(bool mirror)
{
    const u16 min_ol_sy = mirror ? DS_min_ol_sy_mirror : DS_min_ol_sy;
    const u16 min_ol    = mirror ? DS_min_ol_mirror : DS_min_ol;
    const u16 s_ol = mirror ? DS_span_ol_mirror : DS_span_ol;
    const u16 s_l  = mirror ? DS_span_l_mirror  : DS_span_l;
    const u16 s_r  = mirror ? DS_span_r_mirror  : DS_span_r;
    const u16 s_or = mirror ? DS_span_or_mirror : DS_span_or;

    DSB(min_ol_sy) = (u8)(DSB(min_ol_sy) << 1);            /* byte doubling in place (quirk) */
    u16 si = (u16)(DSW(mirror ? DS_top_sy_mirror : DS_top_sy) << 1);
    do {
        SpanPos p;
        if (!mirror) {
            p.di = scene_rowtab(si);
            p.dx = 0;
            DSW(DS_span_bit) = 0;
        } else {
            p.di = scene_rowtab((u16)(si + 0x36));
            DSW(DS_span_bit) = 0;
            p.di = (u16)(p.di + 0x1E);
            p.dx = 0x1E;
        }
        p.es = scene_cur_plane(2);
        if ((s16)si > (s16)DSW(min_ol_sy) && (s16)DSW(min_ol) < (s16)DSW((u16)(s_ol + si))) {
            if (fill_span_to(&p, DSW(min_ol), mirror)) goto next;
            p.es = scene_cur_plane(1);
        }
        if (fill_span_to(&p, DSW((u16)(s_ol + si)), mirror)) goto next;
        p.es = scene_cur_plane(0);
        if (fill_span_to(&p, DSW((u16)(s_l + si)), mirror)) goto next;
        {
            u16 ax = DSW((u16)(s_r + si));
            if ((s16)ax >= 0x140) goto next;
            u16 bx = (u16)(ax & 7);
            ax = (u16)((s16)ax >> 3);
            p.di = (u16)(p.di + ax - p.dx);
            p.dx = ax;
            DSW(DS_span_bit) = bx;
        }
        p.es = scene_cur_plane(0);
        if (fill_span_to(&p, DSW((u16)(s_or + si)), mirror)) goto next;
        p.es = scene_cur_plane(1);
        fill_span_to(&p, 0x140, mirror);
    next:
        si = (u16)(si + 2);
    } while (si != (mirror ? 0x22 : 0xDE));   /* TODO(verify): top_sy == 0x6F / 0x11 would wrap si
                                                  through 0xFFFF like the asm (writes stay inside mem[]) */
}
#endif

/* 0x2EDC fill_scenery_above_road — §4.6. Rows 0x13..top_sy-1, 16-px words, mirror rows 15 words.
 * TODO(verify): like the asm, top_sy <= 0x13 makes the `si != top_sy` loop wrap through 64K rows. */
void fill_scenery_above_road(void)
{
#if TD_CGA
    /* TDCGA 0x2E5C: 40 words per row, no hole for the mirror (draw_mirror_background overwrites it) */
    u16 es = scene_road_seg();
    u16 si = 0x13;
    u16 bp = (u16)((si << 1) + DSW(DS_road_buf_rowtab));
    u16 ax = (u16)((s16)(u16)(DSW(DS_cut_x) + 0x20) >> 3);
    u16 dx = 0x28;
    if ((s16)ax <= 0 || (s16)ax >= (s16)dx) {
        bool grass = (s16)ax <= 0;
        do {
            u16 di = CSW(bp);
            bp = (u16)(bp + 2);
            cga_stosw(es, di, grass ? cga_pat_grass(si & 1) : CGA_PAT_SCENERY, dx);
            si++;
        } while (si != DSW(DS_top_sy));
        return;
    }
    dx = (u16)(dx - ax);
    u16 bx = ax;
    do {
        u16 di = CSW(bp);
        bp = (u16)(bp + 2);
        di = cga_stosw(es, di, CGA_PAT_SCENERY, bx);
        cga_stosw(es, di, cga_pat_grass(si & 1), dx);
        si++;
    } while (si != DSW(DS_top_sy));
#else
    u16 si = 0x13;
    u16 bp = (u16)((si << 1) + DSW(DS_road_buf_rowtab));
    u16 ax = (u16)((s16)(u16)(DSW(DS_cut_x) + 0x20) >> 4);
    u16 dx = 0x14;

    if ((s16)ax <= 0 || (s16)ax >= (s16)dx) {
        u16 es = (s16)ax <= 0 ? scene_cur_plane(1) : scene_cur_plane(2);
        do {
            u16 di = CSW(bp);
            bp = (u16)(bp + 2);
            u16 cx = dx;
            if (!((s16)si > 0x2C) && !((s16)si < 0x1B)) cx = (u16)(cx - 5);
            scene_stos_ff(es, di, (u16)(cx * 2));
            si++;
        } while (si != DSW(DS_top_sy));
        return;
    }
    dx = (u16)(dx - ax);
    u16 bx = ax;
    do {
        u16 di = CSW(bp);
        bp = (u16)(bp + 2);
        u16 cx = bx;
        if (!((s16)si > 0x2C) && !((s16)si < 0x1B)) {
            if ((s16)cx >= 0xF) {
                scene_stos_ff(scene_cur_plane(2), di, 0xF * 2);
            } else {
                di = scene_stos_ff(scene_cur_plane(2), di, (u16)(cx * 2));
                cx = (u16)(dx - 5);
                scene_stos_ff(scene_cur_plane(1), di, (u16)(cx * 2));
            }
        } else {
            di = scene_stos_ff(scene_cur_plane(2), di, (u16)(cx * 2));
            scene_stos_ff(scene_cur_plane(1), di, (u16)(dx * 2));
        }
        si++;
    } while (si != DSW(DS_top_sy));
#endif
}

/* 0x33FB draw_mirror_background — §4.6 */
void draw_mirror_background(void)
{
#if TD_CGA
    /* TDCGA 0x3365: no target select, clip or clear; rows are written from byte 0x3C */
    u16 es = scene_road_seg();
    u16 si = 0x1B;
    u16 bp = (u16)((si << 1) + DSW(DS_road_buf_rowtab));
    DSW(DS_crest_sy) = (u16)(DSW(DS_top_sy_mirror) + si);
    u16 ax = (u16)((s16)(u16)(DSW(DS_cut_x_mirror) + 0x10) >> 3);
    u16 dx = 0x0A;
    bool gt = (s32)(s16)ax - 0x1E > 0;                /* sub ax,1Eh ; jg */
    ax = (u16)(ax - 0x1E);
    if (!gt || (s16)ax >= (s16)dx) {
        do {
            u16 di = (u16)(CSW(bp) + 0x3C);
            bp = (u16)(bp + 2);
            cga_stosw(es, di, !gt ? cga_pat_grass(si & 1) : CGA_PAT_SCENERY, dx);
            si++;
        } while (si != DSW(DS_crest_sy));
        return;
    }
    dx = (u16)(dx - ax);
    u16 bx = ax;
    do {
        u16 di = (u16)(CSW(bp) + 0x3C);
        bp = (u16)(bp + 2);
        di = cga_stosw(es, di, CGA_PAT_SCENERY, bx);
        cga_stosw(es, di, cga_pat_grass(si & 1), dx);
        si++;
    } while (si != DSW(DS_crest_sy));
#else
    select_road_buffer();
    CSW(GFX_CUR_CLIP_X0) = 0x1E;
    CSW(GFX_CUR_CLIP_X1) = 0x28;
    CSW(GFX_CUR_CLIP_Y0) = 0x1B;
    CSW(GFX_CUR_CLIP_Y1) = 0x2C;
    gfx_clear_clip(8);

    u16 si = 0x1B;
    u16 bp = (u16)((si << 1) + DSW(DS_road_buf_rowtab));
    DSW(DS_crest_sy) = (u16)(DSW(DS_top_sy_mirror) + si);
    u16 ax = (u16)((s16)(u16)(DSW(DS_cut_x_mirror) + 0x10) >> 3);
    u16 dx = 0x0A;
    bool gt = (s32)(s16)ax - 0x1E > 0;                    /* sub ax,1Eh ; jg */
    ax = (u16)(ax - 0x1E);

    if (!gt || (s16)ax >= (s16)dx) {
        u16 es = !gt ? scene_cur_plane(1) : scene_cur_plane(2);
        do {
            u16 di = (u16)(CSW(bp) + 0x1E);
            bp = (u16)(bp + 2);
            scene_stos_ff(es, di, dx);
            si++;
        } while (si != DSW(DS_crest_sy));
        return;
    }
    dx = (u16)(dx - ax);
    u16 bx = ax;
    do {
        u16 di = CSW(bp);
        bp = (u16)(bp + 2);
        di = (u16)(di + 0x1E);
        di = scene_stos_ff(scene_cur_plane(2), di, bx);
        scene_stos_ff(scene_cur_plane(1), di, dx);
        si++;
    } while (si != DSW(DS_crest_sy));
#endif
}

/* Centre-line dash pixel: OR mask_single_pixel into planes 2,1,0 at rowtab[row] + x>>3. */
static void plot_dash(u16 row2, u16 x)
{
#if TD_CGA
    /* TDCGA 0x28ED: one 2bpp pixel of colour 3, ES = road buffer */
    u16 di = (u16)(scene_rowtab(row2) + (u16)((s16)x >> 2));
    u16 es = scene_road_seg();
    wr8(es, di, (u8)(rd8(es, di) | DSB((u16)(DS_CGA_PIXEL_MASK + (x & 3)))));
#else
    u16 di = (u16)(scene_rowtab(row2) + (u16)((s16)x >> 3));
    u8 m = DSB((u16)(DS_mask_single_pixel + (x & 7)));
    u8 *p;
    p = mp(scene_cur_plane(2), di); *p |= m;
    p = mp(scene_cur_plane(1), di); *p |= m;
    p = mp(scene_cur_plane(0), di); *p |= m;
#endif
}

/* Police-car row smoothing (DS:1447) shared by main and mirror; returns the row (bp units). */
static u16 cop_row(u16 bp)
{
    if (DSB(DS_r_cop_state) != 3) {
        u16 d = (u16)(DSW(DS_COP_ROW_SMOOTH) - bp);   /* cop_row_smooth */
        if ((s16)d <= 2 && (s16)d >= -2) return DSW(DS_COP_ROW_SMOOTH);
    }
    DSW(DS_COP_ROW_SMOOTH) = bp;
    return bp;
}

/* `shr bx,1 ; mov bl,[bx+table]` — keeps BH */
static u16 scale_entry(u16 row, u16 table)
{
    u16 bx = (u16)(row >> 1);
    return (u16)((bx & 0xFF00) | DSB((u16)(bx + table)));
}

/* 0x28C5 draw_road_main — §4.7 */
void draw_road_main(void)
{
    static FrameStack st;      /* host-side work area (the original's CPU stack); empty between calls */
    st.n = 0;

#if TD_CGA
    fill_road_scanlines_cga(false);
#else
    fill_road_scanlines(false);
#endif
    u16 bp = 2;
    DSB(DS_crest_sy) = 0x6F;
    CSW(GFX_CUR_CLIP_Y1) = 0x6F;
    CSW(GFX_CUR_CLIP_Y0) = 0x13;
    CSW(GFX_CUR_CLIP_X1) = EGA_CGA(0x28, 0x50);
    CSW(GFX_CUR_CLIP_X0) = 0;
    DSB(DS_dash_counter) = DSB(DS_road_anim_counter);
    DSW(DS_xclip_px) = 0x140;
    DSW(DS_xclip_bytes) = EGA_CGA(0x28, 0x50);
    fs_push(&st, 0xFFFF);

    do {
        u16 dx = (u16)(DSW((u16)(DS_road_halfwidth_q4 + bp)) >> 4);
        if ((s16)dx >= 0x1F) dx = 0x1F;
        DSW(DS_obj_scale) = dx;

        if (bp == DSW(DS_cut_row)) {
#if TD_CGA
            dx = (u16)((s16)(u16)(DSW(DS_cut_x) + 0x20) >> 2);   /* TDCGA 0x28A6: 4 px per byte */
            if (dx > 0x50) dx = ((s16)dx > 0x50) ? 0x50 : 0;
            DSW(DS_xclip_bytes) = dx;
            DSW(DS_xclip_px) = (u16)(dx << 2);
#else
            dx = (u16)((s16)(u16)(DSW(DS_cut_x) + 0x20) >> 3);
            if (dx > 0x28) dx = ((s16)dx > 0x28) ? 0x28 : 0;
            DSW(DS_xclip_bytes) = dx;
            DSW(DS_xclip_px) = (u16)(dx << 3);
#endif
        }

        u8 bl = DSB((u16)(DS_row_sy + bp));
        if (bl < DSB(DS_crest_sy)) {
            DSB(DS_crest_sy) = bl;
            CSW(GFX_CUR_CLIP_Y1) = bl;
            if (!(DSB(DS_dash_counter) & 4)) {
                u16 ax = DSW((u16)(DS_row_cx + bp));
                if (ax < DSW(DS_xclip_px)) plot_dash((u16)((u16)bl << 1), ax);
            }
        }

        /* poles, every 16 units on the left shoulder */
        if (!(DSB(DS_dash_counter) & 0x0F)) {
            u8 cl = DSB((u16)(DS_row_sy + bp));
            u16 s = DSW(DS_obj_scale);
            u16 bx = DSW((u16)(DS_row_ol + bp));
            if (bx < DSW(DS_xclip_px)) {
                u16 cx = (u8)(cl - (u8)s);
                u16 e = (u16)(((s >> 1) & 0xFC) + H_POLES);         /* XROADA pal*, pol* at +0x10 */
                fs_push_frame(&st, bx, cx, (u16)(e + 0x10), e, DSW(DS_xclip_bytes), DSW(DS_crest_sy));
            }
        }

        /* right-side scenery, immediate XOR blit */
        if ((s16)bp < 0x20 && (s16)bp <= (s16)DSW(DS_cut_row)) {
            u16 bx = (u16)((u8)(DSB(DS_dash_counter) << 1) & 0x1E);
            u16 ax = DSW((u16)(DS_roadside_pattern + bx));
            if ((u8)ax < 6) {
                u16 di = DSW((u16)(DS_row_or + bp));
                if (di < 0x140) {
                    u8 cl = (u8)(ax >> 8);
                    ax = (u16)(((ax & 0xFF) << 4) + H_SCENERY);
                    u16 d = (u16)(DSW(DS_obj_scale) >> 1);
                    u8 ch = (u8)d;
                    d = (u16)((d & 0xFF00) | ((u8)d & 0xFC));
                    d = (u16)(d + ax);
                    u16 y = DSB((u16)(DS_row_sy + bp));
                    if (cl != 0) y = (u16)(y - (u16)ch * cl);
#if TD_CGA
                    blit_or_clip_hot(scene_ds_far(d), (s16)di, (s16)y);   /* TDCGA 0x29AD: OR, not XOR */
#else
                    blit_xor_clip_hot(scene_ds_far(d), (s16)di, (s16)y);
#endif
                }
            }
        }

        /* signs 2..8 with post */
        {
            u8 al = DSB((u16)(DS_row_obj + bp));
            u8 side = (u8)(al & 0x80);
            al = (u8)((al & 0x3F) - 2);
            if (al <= 6) {
                u16 ax = (u16)((u16)al << 5);
                u8 cl = DSB((u16)(DS_row_sy + bp));
                u16 s = DSW(DS_obj_scale);
                u16 bx = side ? DSW((u16)(DS_row_l + bp)) : DSW((u16)(DS_row_r + bp));
                if (bx < DSW(DS_xclip_px)) {
                    u16 di = (u16)((s & 0xF8) + ax + H_SIGNS);
                    u16 cx = (u16)((u16)cl - s);
                    u16 post = (u16)(((s >> 1) & 0xFC) + H_POSTS);
                    fs_push(&st, cx); fs_push(&st, bx);
                    fs_push(&st, DSW((u16)(post + 2))); fs_push(&st, DSW(post));
                    fs_push(&st, 0xFFFE);
                    fs_push_frame(&st, bx, cx, di, (u16)(di + 4), DSW(DS_xclip_bytes), DSW(DS_crest_sy));
                }
            }
        }

        /* traffic slots 0..9 */
        {
            u16 si = DS_r_traffic_slots, cx = 0;
            for (;;) {
                u16 bx = DSW(si);
                if (bx != 0) {
                    bx = (u16)((u16)(bx - DSW(DS_r_road_ptr)) << 1);
                    bool draw;
                    if (bx == bp) draw = !((s16)DSW(DS_r_subpos) > (s16)DSW((u16)(si + 2)));
                    else draw = (u16)(bx + 2) == bp && (s16)DSW(DS_r_subpos) > (s16)DSW((u16)(si + 2));
                    if (draw) {
                        u16 e = (u16)(scale_entry(bp, DS_traffic_scale_main) + DSW((u16)(si + 6)));
                        u16 ax = DSB((u16)(DS_row_sy + bp));
                        u16 d = (u16)(DSW((u16)(DS_row_cx + bp))
                                      + ((s16)cx < 5 ? DSW((u16)(DS_row_l + bp)) : DSW((u16)(DS_row_r + bp))));
                        d = (u16)((s16)d >> 1);
                        if (d < DSW(DS_xclip_px)) {
                            u16 di = DSW(DS_crest_sy);
                            if (bp == 2) { ax = 0x76; di = 0x6F; }
                            fs_push_frame(&st, d, ax, (u16)(e + H_TRAFFIC_OR), (u16)(e + H_TRAFFIC_AND), DSW(DS_xclip_bytes), di);
                        }
                    }
                }
                si = (u16)(si + 8);
                cx++;
                if ((u8)cx == 10) break;
            }
        }

        /* police car ahead */
        if (DSB(DS_r_cop_state) != 0 && DSB(DS_r_cop_state) != 7) {
            u16 ax = (u16)((u16)(DSW(DS_r_cop_pos) - DSW(DS_r_road_ptr)) << 1);
            bool draw;
            if (ax == bp) draw = !((s16)DSW(DS_r_subpos) < (s16)DSW(DS_r_cop_sub));
            else draw = (u16)(ax + 2) == bp && (s16)DSW(DS_r_subpos) < (s16)DSW(DS_r_cop_sub);
            if (draw) {
                u16 a = DSW((u16)(DS_row_cx + bp));
                u16 b = DSW((u16)(DS_row_r + bp));
                u16 c = (u16)(b - a);
                b = (u16)((u16)(b + a) >> 1);
                u16 m = (u16)((u16)((u32)DSW(DS_r_cop_lane) * c) >> 4);
                b = (u16)(b - m);
                if (!((s16)b > (s16)DSW(DS_xclip_px))) {
                    u16 x = b;
                    u16 e = (u16)(scale_entry(cop_row(bp), DS_traffic_scale_main) + H_COP_MAIN);
                    u16 y = DSW((u16)(DS_row_sy + bp));
                    u16 cy = DSW(DS_crest_sy);
                    if (bp == 2) { y = 0x76; cy = 0x6F; }
                    if (e == H_COP_FAR)
                        fs_push_frame(&st, x, y, H_COPB, H_COPM, DSW(DS_xclip_bytes), cy);   /* copb / copm */
                    if (!(DSB(DS_g_stageTime) & 8))                                          /* DS:80A4 ticks */
                        fs_push_frame(&st, x, y, (u16)(e + 0x28), (u16)(e + 0x2C), DSW(DS_xclip_bytes), cy);
                    fs_push_frame(&st, x, y, e, (u16)(e + 4), DSW(DS_xclip_bytes), cy);
                }
            }
        }

        /* hazard slot 12 (DS:0A0D), immediate OR */
        {
            u16 bx = (u16)((u16)(DSW(DS_r_hazard_slot) - DSW(DS_r_road_ptr)) << 1);
            if (bx == bp) {
                u16 cx = DSW((u16)(DS_r_hazard_slot + 6));
                u16 c0 = DSW((u16)(DS_row_cx + bp));
                u16 d = (u16)((u16)(DSW((u16)(DS_row_r + bp)) - c0) >> 2);
                u16 ax = (u16)(d >> 1);
                u8 ch = (u8)(cx >> 8);
                while (ch != 0) { ax = (u16)(ax + d); ch--; }
                cx &= 0x00FF;
                ax = (u16)(ax + c0);
                if (ax < DSW(DS_xclip_px)) {
                    u16 e = (u16)(DSW(DS_obj_scale) >> 1);
                    e = (u16)((e & 0xFF00) | ((u8)e & 0xFC));
                    e = (u16)(e + H_HAZARD + cx);
                    blit_or_clip_hot(scene_ds_far(e), (s16)ax, (s16)DSW((u16)(DS_row_sy + bp)));
                }
            }
        }

        /* horizon cliff where the road bends away */
        if (DSW(DS_cut_x) < 0x140 && bp == DSW(DS_cut_row)) {
            u16 ax = DSW(DS_cut_x);
            u16 d = DSW(DS_cut_sy);
            if (!((s16)d < 0x75)) d = 0x75;
#if TD_CGA
            d |= 1; ax |= 1;                                                      /* TDCGA 0x2C31 */
#endif
            fs_push_frame(&st, ax, d, H_CLIFF_OR, H_CLIFF_AND, DSW(DS_xclip_bytes), DSW(DS_crest_sy));  /* clfo / clfa */
        }

        DSB(DS_dash_counter)++;
        bp = (u16)(bp + 2);
    } while ((s16)bp < (s16)DSW(DS_row_limit_main));

    if (bp == 0x50) {
        /* whole draw distance visible: nearest far car per lane at the top row */
        u16 si = DSW(DS_r_road_ptr);
        u16 tr = DSW(DS_top_row);
        for (int lane = 0; lane < 2; lane++) {
            u16 cx = DSB(lane == 0 ? DS_r_traffic_count_l : DS_r_traffic_count_r);
            if ((u8)cx == 0) continue;
            u16 bx = (u16)(((cx - 1) << 3) + (lane == 0 ? DS_TRAFFIC_L0 : DS_TRAFFIC_L1));
            for (;;) {
                if (!((s16)(u16)(DSW(bx) - si) < 0x27)) {
                    u16 e = DSW((u16)(bx + 6));
                    u16 y = DSB(DS_ROW_SY_39);                              /* sy[39] */
                    u16 d = (u16)(DSW((u16)(DS_row_cx + tr))
                                  + (lane == 0 ? DSW((u16)(DS_row_l + tr)) : DSW((u16)(DS_row_r + tr))));
                    d = (u16)((s16)d >> 1);
                    if (d < DSW(DS_xclip_px))
                        fs_push_frame(&st, d, y, (u16)(e + H_TRAFFIC_OR), (u16)(e + H_TRAFFIC_AND),
                                      DSW(DS_xclip_bytes), DSW(DS_crest_sy));
                    break;
                }
                bx = (u16)(bx - 8);
                if (--cx == 0) break;
            }
        }
    }

    /* pop loop (0x2DCA): far -> near */
    for (;;) {
        u16 ax = fs_pop(&st);
        if (ax != 0xFFFE) {
            if (ax == 0xFFFF) return;
            u16 bx = fs_pop(&st);
            CSW(GFX_CUR_CLIP_X1) = bx;
            CSW(GFX_CUR_CLIP_Y1) = ax;
            fs_blit(&st, blit_and_clip_hot);
        }
        fs_blit(&st, blit_or_clip_hot);
    }
}

/* 0x2F7A draw_road_mirror — §4.7 mirror table */
void draw_road_mirror(void)
{
    static FrameStack st;
    st.n = 0;

#if TD_CGA
    fill_road_scanlines_cga(true);
#else
    fill_road_scanlines(true);
#endif
    u16 bp = 2;
    DSB(DS_crest_sy) = 0x11;
    CSW(GFX_CUR_CLIP_X0) = EGA_CGA(0x1E, 0x3C);
    CSW(GFX_CUR_CLIP_X1) = EGA_CGA(0x28, 0x50);
    CSW(GFX_CUR_CLIP_Y0) = 0x1B;
    CSW(GFX_CUR_CLIP_Y1) = 0x11 + 0x1B;
    DSB(DS_dash_counter) = (u8)(DSB(DS_road_anim_counter) - 2);
    DSW(DS_xclip_px) = 0x140;
    DSW(DS_xclip_bytes) = EGA_CGA(0x28, 0x50);
    fs_push(&st, 0xFFFF);

    do {
        u16 dx = (u16)(DSW((u16)(DS_road_halfwidth_q4 + 4 + bp)) >> 5);   /* [bp+21A9] */
        if ((s16)dx >= 0x1F) dx = 0x1F;
        DSW(DS_obj_scale) = dx;

        if (bp == DSW(DS_cut_row_mirror)) {
#if TD_CGA
            dx = (u16)((s16)(u16)(DSW(DS_cut_x_mirror) + 0x10) >> 2);   /* TDCGA 0x2F4D */
            if (dx > 0x50) dx = ((s16)dx > 0x50) ? 0x50 : 0;
            DSW(DS_xclip_bytes) = dx;
            DSW(DS_xclip_px) = (u16)(dx << 2);
#else
            dx = (u16)((s16)(u16)(DSW(DS_cut_x_mirror) + 0x10) >> 3);
            if (dx > 0x28) dx = ((s16)dx > 0x28) ? 0x28 : 0;
            DSW(DS_xclip_bytes) = dx;
            DSW(DS_xclip_px) = (u16)(dx << 3);
#endif
        }

        u8 bl = DSB((u16)(DS_row_sy_mirror + bp));
        if (bl < DSB(DS_crest_sy)) {
            DSB(DS_crest_sy) = bl;
            CSW(GFX_CUR_CLIP_Y1) = (u8)(bl + 0x1B);
            if (!(DSB(DS_dash_counter) & 4)) {
                u16 ax = DSW((u16)(DS_row_cx_mirror + bp));
                if (!((s16)ax < 0xF0) && !((s16)ax > (s16)DSW(DS_xclip_px)))
                    plot_dash((u16)(((u16)bl << 1) + 0x36), ax);
            }
        }

        if (!(DSB(DS_dash_counter) & 0x0F)) {
            u8 cl = DSB((u16)(DS_row_sy_mirror + bp));
            u16 s = DSW(DS_obj_scale);
            u16 bx = DSW((u16)(DS_row_ol_mirror + bp));
            if ((s16)bx < (s16)DSW(DS_xclip_px) && !((s16)bx < 0xF0)) {
                u16 cx = (u8)((u8)(cl - (u8)s) + 0x1B);
                u16 e = (u16)(((s >> 1) & 0xFC) + H_POLES);
                fs_push_frame(&st, bx, cx, (u16)(e + 0x10), e, DSW(DS_xclip_bytes), DSW(DS_crest_sy));
            }
        }

        /* signs: only the type-0 mask at this scale, then the post (faithful quirk) */
        {
            u8 al = DSB((u16)(DS_row_obj_mirror + bp));
            u8 side = (u8)(al & 0x80);
            al = (u8)((al & 0x3F) - 2);
            if (al <= 6) {
                u8 cl = DSB((u16)(DS_row_sy_mirror + bp));
                u16 s = DSW(DS_obj_scale);
                u16 bx = side ? DSW((u16)(DS_row_l_mirror + bp)) : DSW((u16)(DS_row_r_mirror + bp));
                if ((s16)bx < (s16)DSW(DS_xclip_px) && !((s16)bx < 0xF0)) {
                    u16 di = (u16)((s & 0xF8) + H_SIGNS);
                    u16 cx = (u16)((u16)cl - s + 0x1B);
                    u16 post = (u16)(((s >> 1) & 0xFC) + H_POSTS);
                    fs_push_frame(&st, bx, cx, post, (u16)(di + 4), DSW(DS_xclip_bytes), DSW(DS_crest_sy));
                }
            }
        }

        /* traffic */
        {
            u16 si = DS_r_traffic_slots, cx = 0;
            for (;;) {
                u16 bx = DSW(si);
                if (bx != 0) {
                    bx = (u16)-(u16)((u16)(bx - DSW(DS_r_road_ptr)) << 1);
                    bool draw;
                    if (bx == bp) draw = !((s16)DSW(DS_r_subpos) < (s16)DSW((u16)(si + 2)));
                    else draw = (u16)(bx + 2) == bp && (s16)DSW(DS_r_subpos) < (s16)DSW((u16)(si + 2));
                    if (draw) {
                        u16 e = (u16)(DSW((u16)(si + 6)) + DSB((u16)(DS_traffic_scale_mirror + (bp >> 1))));
                        u16 ax = (u16)(DSB((u16)(DS_row_sy_mirror + bp)) + 0x1B);
                        u16 d = DSW((u16)(DS_row_cx_mirror + bp));
                        if ((s16)cx < 5) { d = (u16)(d + DSW((u16)(DS_row_l_mirror + bp))); e = (u16)(e + 0xC8); }
                        else             { d = (u16)(d + DSW((u16)(DS_row_r_mirror + bp))); e = (u16)(e - 0xC8); }
                        d = (u16)((s16)d >> 1);
                        if (d < DSW(DS_xclip_px))
                            fs_push_frame(&st, d, ax, (u16)(e + H_TRAFFIC_OR), (u16)(e + H_TRAFFIC_AND),
                                          DSW(DS_xclip_bytes), DSW(DS_crest_sy));
                    }
                }
                si = (u16)(si + 8);
                cx++;
                if ((u8)cx == 10) break;
            }
        }

        /* police car behind */
        if (DSB(DS_r_cop_state) != 0 && DSB(DS_r_cop_state) != 7) {
            u16 ax = (u16)((u16)(DSW(DS_r_road_ptr) - DSW(DS_r_cop_pos)) << 1);
            bool draw;
            if (ax == bp) draw = !((s16)DSW(DS_r_subpos) > (s16)DSW(DS_r_cop_sub));
            else draw = (u16)(ax + 2) == bp && (s16)DSW(DS_r_subpos) > (s16)DSW(DS_r_cop_sub);
            if (draw) {
                u16 a = DSW((u16)(DS_row_cx_mirror + bp));
                u16 b = DSW((u16)(DS_row_r_mirror + bp));
                u16 c = (u16)(b - a);
                b = (u16)((u16)(b + a) >> 1);
                u16 m = (u16)((u16)((u32)DSW(DS_r_cop_lane) * c) >> 4);
                b = (u16)(b - m);
                if (!((s16)b > (s16)DSW(DS_xclip_px))) {
                    u16 x = b;
                    u16 e = (u16)(scale_entry(cop_row(bp), DS_traffic_scale_mirror) + H_COP_MIRROR);
                    u16 y = (u16)(DSW((u16)(DS_row_sy_mirror + bp)) + 0x1B);
                    if (!(DSB(DS_g_stageTime) & 8))
                        fs_push_frame(&st, x, y, (u16)(e + 0x50), (u16)(e + 0x54), DSW(DS_xclip_bytes), DSW(DS_crest_sy));
                    fs_push_frame(&st, x, y, e, (u16)(e + 4), DSW(DS_xclip_bytes), DSW(DS_crest_sy));
                }
            }
        }

        /* mirror horizon cliff */
        {
            u16 ax = DSW(DS_cut_x_mirror);
            if (!((s16)ax < 0xF0) && (s16)ax < 0x140 && bp == DSW(DS_cut_row_mirror)) {
                u16 d = DSW(DS_cut_sy_mirror);
                if (!((s16)d < 0xF)) d = 0xF;
                d = (u16)(d + 0x1B);
#if TD_CGA
                d |= 1; ax |= 1;                                                  /* TDCGA 0x3206 */
#endif
                fs_push_frame(&st, ax, d, H_MCLIFF_OR, H_MCLIFF_AND, DSW(DS_xclip_bytes), DSW(DS_crest_sy)); /* mcfo / mcfa */
            }
        }

        DSB(DS_dash_counter)--;
        bp = (u16)(bp + 2);
    } while ((s16)bp < (s16)DSW(DS_row_limit_mirror));

    /* pop loop (0x32E4) */
    for (;;) {
        u16 ax = fs_pop(&st);
        if (ax == 0xFFFF) return;
        u16 bx = fs_pop(&st);
        CSW(GFX_CUR_CLIP_X1) = bx;
        CSW(GFX_CUR_CLIP_Y1) = (u16)(ax + 0x1B);
        fs_blit(&st, blit_and_clip_hot);
        fs_blit(&st, blit_or_clip_hot);
    }
}
