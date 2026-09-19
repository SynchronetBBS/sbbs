/* scene_render: road projection (main view and rear-view mirror).
 * Port of TDEGA image 0x2054..0x28C4 and 0x39EB..0x3AC1 (port/spec/scene_render.md §4.2–4.5).
 * All state is in DGROUP; register-argument asm helpers are static functions named after them. */
#include "scene.h"

/* 0x39EB sin_deg — §4.2. AX = sign-extended byte (degrees); returns 15*sin from DS:22F2. */
static u16 sin_deg(u16 ax)
{
    u8 bh = 0;
    if ((s16)ax < 0) ax = (u16)(ax + 0x168);
    if (ax > 0xB4) { ax = (u16)(ax - 0xB4); bh = 1; }
    if ((u8)ax > 0x5A) ax = (u16)((ax & 0xFF00) | (u8)(0xB4 - (u8)ax));   /* sub cl,al ; mov al,cl */
    ax = DSW((u16)(DS_sine_x15 + (u16)(ax << 1)));
    if (bh) ax = (u16)-ax;
    return ax;
}

/* 0x3A18 atan_lookup — §4.2. AX = ratio*256 -> degrees 45..90 (0xFFFF -> 68, signed start test). */
static u16 atan_lookup(u16 ax)
{
    u16 bx = ((s16)ax > 0x27A) ? 0x5A : 0x2E;
    while (ax < DSW((u16)(bx + DS_tan256))) bx = (u16)(bx - 2);
    return (u16)(((s16)bx >> 1) + 0x2D);
}

/* 0x23F2 proj_x_main / 0x282C proj_x_mirror — §4.3. mult*|X|/Z clamped, sign restored, + centre. */
static u16 proj_x_common(u16 bx, u16 dx, u16 mult, u16 clampv, u16 centre)
{
    u8 cl = 0;
    u16 ax;
    if ((s16)bx < 0) { bx = (u16)-bx; cl = 1; }
    if ((s16)dx <= 0) {
        ax = clampv;
    } else {
        ax = div32_16((u32)mult * bx, dx, NULL);   /* xchg dx,bx ; mul dx ; div bx */
        if (ax > clampv) ax = clampv;
    }
    if (cl) ax = (u16)-ax;
    return (u16)(ax + DSW(centre));
}
static u16 proj_x_main(u16 bx_x, u16 dx_z)   { return proj_x_common(bx_x, dx_z, 0x258, 0xC80, DS_proj_cx_main); }
static u16 proj_x_mirror(u16 bx_x, u16 dx_z) { return proj_x_common(bx_x, dx_z, 0x96, 0x320, DS_proj_cx_mirror); }

/* Shared body of 0x2421 / 0x285B: returns AX after `mov al,ah ; xor ah,ah` (0..0x45), DI = Y<0. */
static u16 proj_y_core(u16 bx, u16 dx, u16 cx, u16 *di)
{
    u16 ax;
    *di = 0;
    if ((s16)bx < 0) bx = (u16)-bx;
    if ((s16)cx < 0) { cx = (u16)-cx; *di = 1; }
    if ((s16)dx <= 0) {
        ax = 0x45FF;
    } else {
        if ((s16)bx > (s16)dx) { u16 t = dx; dx = bx; bx = t; }
        u16 hi = dx;                                        /* push dx */
        ax = div32_16((u32)hi << 8, bx, NULL);              /* DX:AX = hi<<8 ; div bx */
        u16 deg = atan_lookup(ax);
        u32 prod = (u32)DSW((u16)(DS_persp_T + (u16)(deg << 1))) * cx;
        ax = div32_16(prod, hi, NULL);                      /* pop cx ; div cx */
        if (ax >= 0x4600) ax = 0x45FF;
    }
    return (u16)(ax >> 8);
}

/* 0x2421 proj_y_main (BX, DX preserved) */
static u16 proj_y_main(u16 bx_x, u16 dx_z, u16 cx_y)
{
    u16 di;
    u16 ax = proj_y_core(bx_x, dx_z, cx_y, &di);
    if (di == 0) ax = (u16)-ax;
    return (u16)(ax + DSW(DS_proj_horizon_main));
}

/* 0x285B proj_y_mirror */
static u16 proj_y_mirror(u16 bx_x, u16 dx_z, u16 cx_y)
{
    u16 di;
    u16 ax = proj_y_core(bx_x, dx_z, cx_y, &di);
    if (di == 0) ax = (u16)-ax;
    ax = (u16)((s16)ax >> 2);
    if ((s16)ax < 0) ax = 0;
    if ((s16)ax > 0x11) ax = 0x11;
    return (u16)(ax + DSW(DS_proj_y_mirror_off));
}

/* 0x3A37 interp_edge_span — §4.5, literal. AX=a, BX=b, CX (CL = Bresenham span, CH = 0 from the
 * caller), DL = number of words written at DS:DI. CL and DL differ on the first span after the
 * cached crest (CL = sy[prev]-sy, DL = crest-sy). bh/ch/bl are 8-bit, `jle` on them is signed. */
static void interp_edge_span(u16 ax, u16 bx, u16 cx, u16 dx, u16 di)
{
    u8 cl = (u8)cx, bh, ch, bl;
    u16 bp;
    dx &= 0x00FF;                                           /* xor dh,dh */
    bool ge = (s32)(s16)bx - (s32)(s16)ax >= 0;             /* sub bx,ax ; jge */
    bx = (u16)(bx - ax);
    if (!ge) {
        bp = DSW(DS_span_clamp_min);
        bx = (u16)-bx;
        if ((s16)bx > (s16)cx) {                            /* steep */
            bh = 0; bl = (u8)bx; ch = (u8)(bl >> 1);
            for (;;) {
                ax--;
                if ((s16)ax < (s16)bp) goto fill;
                bh = (u8)(bh + cl);
                if ((s8)bh <= (s8)ch) continue;
                ax++;
                wr16(DGROUP, di, ax); di = (u16)(di + 2);
                if (--dx == 0) return;
                bh = (u8)(bh - bl);
                ax--;
            }
        } else {                                            /* shallow */
            bh = 0; bl = (u8)bx; ch = (u8)(cl >> 1);
            for (;;) {
                wr16(DGROUP, di, ax); di = (u16)(di + 2);
                if (--dx == 0) return;
                bh = (u8)(bh + bl);
                if ((s8)bh <= (s8)ch) continue;
                bh = (u8)(bh - cl);
                ax--;
                if ((s16)ax >= (s16)bp) continue;
                goto fill;
            }
        }
    } else {
        bp = DSW(DS_span_clamp_max);
        if ((s16)bx > (s16)cx) {
            bh = 0; bl = (u8)bx; ch = (u8)(bl >> 1);
            for (;;) {
                ax++;
                if ((s16)ax > (s16)bp) goto fill;
                bh = (u8)(bh + cl);
                if ((s8)bh <= (s8)ch) continue;
                ax--;
                wr16(DGROUP, di, ax); di = (u16)(di + 2);
                if (--dx == 0) return;
                bh = (u8)(bh - bl);
                ax++;
            }
        } else {
            bh = 0; bl = (u8)bx; ch = (u8)(cl >> 1);
            for (;;) {
                wr16(DGROUP, di, ax); di = (u16)(di + 2);
                if (--dx == 0) return;
                bh = (u8)(bh + bl);
                if ((s8)bh <= (s8)ch) continue;
                bh = (u8)(bh - cl);
                ax++;
                if ((s16)ax < (s16)bp) continue;
                goto fill;
            }
        }
    }
fill:                                                       /* 0x3A7C: rep stosw with CX = DX */
    while (dx--) { wr16(DGROUP, di, bp); di = (u16)(di + 2); }
}

/* Differences between 0x2054 and 0x2478 (table in §4.4). */
typedef struct {
    bool mirror;
    u16 X, Y, sy, cx, ol, l, r, orr, obj;           /* row arrays (element i at base + 2i) */
    u16 span_ol, span_l, span_r, span_or;           /* scanline arrays */
    u16 row_limit, top_sy, top_row, cut_x, cut_sy, cut_row, cut_top_sy, min_ol, min_ol_sy;
    u16 cached_top_row;
} ProjCfg;

static const ProjCfg PROJ_MAIN = {
    false, DS_row_X, DS_row_Y, DS_row_sy, DS_row_cx, DS_row_ol, DS_row_l, DS_row_r, DS_row_or, DS_row_obj,
    DS_span_ol, DS_span_l, DS_span_r, DS_span_or,
    DS_row_limit_main, DS_top_sy, DS_top_row, DS_cut_x, DS_cut_sy, DS_cut_row, DS_cut_top_sy,
    DS_min_ol, DS_min_ol_sy, DS_cached_top_row,
};
static const ProjCfg PROJ_MIRROR = {
    true, DS_row_X_mirror, DS_row_Y_mirror, DS_row_sy_mirror, DS_row_cx_mirror, DS_row_ol_mirror,
    DS_row_l_mirror, DS_row_r_mirror, DS_row_or_mirror, DS_row_obj_mirror,
    DS_span_ol_mirror, DS_span_l_mirror, DS_span_r_mirror, DS_span_or_mirror,
    DS_row_limit_mirror, DS_top_sy_mirror, DS_top_row_mirror, DS_cut_x_mirror, DS_cut_sy_mirror,
    DS_cut_row_mirror, DS_cut_top_sy_mirror, DS_min_ol_mirror, DS_min_ol_sy_mirror, DS_cached_top_row_mirror,
};

static void project_road(const ProjCfg *c)
{
    const u16 FC = DS_first_changed_row;
    u16 bp, ax, dx, si;

    if (!c->mirror) DSB(DS_prev_row_limit) = DSB(c->row_limit);    /* byte copy (0x2054) */
    else            DSW(DS_prev_row_limit) = DSW(c->row_limit);    /* word copy (0x2478) */
    DSW(DS_slope_acc) = 0;
    if (!c->mirror) DSW(DS_span_clamp_min) = 0;
    DSB(c->row_limit) = 0;
    DSB(FC) = 0;
    DSB(c->cut_row) = 0;
    DSW(c->X) = (u16)(DSS(DS_car_x) >> 3);
    DSW(c->Y) = 0xFFF4;
    if (c->mirror) DSW(DS_walk_ptr) = (u16)(DSW(DS_road_pos) - 2);
    DSW(DS_heading_acc) = c->mirror ? (u16)-DSW(DS_view_heading) : DSW(DS_view_heading);
    DSW(c->cut_x) = 0x141;
    DSW(c->min_ol) = 0x141;
    DSW(DS_span_clamp_max) = 0x141;
    if (c->mirror) DSW(DS_span_clamp_min) = 0xF0;
    {
        u8 t = c->mirror ? 0x11 : 0x6F;
        DSB(c->top_sy) = t; DSB(DS_cached_top_sy) = t; DSB(c->cut_sy) = t;
    }
    DSB(c->min_ol_sy) = c->mirror ? 0x1E : 0x8C;
    bp = 2;
    DSB(c->cached_top_row) = 2;

    /* ---- phase 1: walk the stream, integrate, project, track crest and cut */
    for (;;) {
        u16 wp = DSW(DS_walk_ptr);
        u8 b = DSB(wp);
        if (DSB(DS_g_stageEvent) != 0 && !((s16)wp < (s16)DSW(DS_stage_end_pos))) b = 0;
        u16 rec = (u16)(EGA_CGA(0x2B70, 0x2B40) + ((u16)b << 2)); /* road record table DS:2B70 */
        DSB(DS_cur_curve) = DSB(rec + 1);
        u8 obj = DSB(rec + 3);
        DSW(DS_walk_ptr) = c->mirror ? (u16)(wp - 1) : (u16)(wp + 1);
        DSB(c->obj + bp) = obj;

        ax = (u16)((u16)(s16)(s8)DSB(rec + 2) << 2);
        ax = (u16)(ax + DSW(DS_slope_acc));
        DSW(DS_slope_acc) = ax;
        ax = sin_deg((u16)(s16)(s8)(ax >> 8));
        ax = (u16)(ax + DSW(c->Y + bp - 2));
        if (DSB(FC) == 0 && ax != DSW(c->Y + bp)) DSW(FC) = bp;
        DSW(c->Y + bp) = ax;

        ax = (u16)((s16)(u16)((u16)DSB(DS_cur_curve) << 8) >> 2);
        ax = (u16)(ax + DSW(DS_heading_acc));
        if ((s16)ax < (s16)0xB500) ax = 0xB500;
        else if ((s16)ax > 0x4B00) ax = 0x4B00;
        DSW(DS_heading_acc) = ax;
        ax = (u16)(sin_deg((u16)(s16)(s8)(ax >> 8)) << 1);
        ax = (u16)(ax + DSW(c->X + bp - 2));
        if (DSB(FC) == 0 && ax != DSW(c->X + bp)) DSW(FC) = bp;
        DSW(c->X + bp) = ax;

        if (DSW(FC) == 0 && (s16)bp < (s16)DSW(DS_prev_row_limit)) {
            /* unchanged row: reuse cached edges */
            ax = DSW(c->ol + bp);
            dx = DSW(c->orr + bp);
            u8 cl = DSB(c->sy + bp);
            if (cl < DSB(c->top_sy)) {
                DSB(c->top_sy) = cl; DSW(c->top_row) = bp;
                DSB(DS_cached_top_sy) = cl; DSW(c->cached_top_row) = bp;
            }
            si = cl;
        } else {
            if (DSW(FC) == 0) DSW(FC) = bp;
            u16 X = DSW(c->X + bp), Z = DSW(DS_row_depth_Z + bp), Y = DSW(c->Y + bp);
            ax = c->mirror ? proj_y_mirror(X, Z, Y) : proj_y_main(X, Z, Y);
            DSB(c->sy + bp) = (u8)ax;
            si = ax;
            if ((u8)ax < DSB(c->top_sy)) { DSB(c->top_sy) = (u8)ax; DSW(c->top_row) = bp; }
            if ((u8)ax == DSB(c->sy + bp - 2)) {
                /* same scanline as the nearer row: copy it, skip tracking */
                DSW(c->cx + bp)  = DSW(c->cx + bp - 2);
                DSW(c->ol + bp)  = DSW(c->ol + bp - 2);
                DSW(c->l + bp)   = DSW(c->l + bp - 2);
                DSW(c->r + bp)   = DSW(c->r + bp - 2);
                DSW(c->orr + bp) = DSW(c->orr + bp - 2);
                goto next;
            }
            ax = c->mirror ? proj_x_mirror(X, Z) : proj_x_main(X, Z);
            dx = ax;
            DSW(c->cx + bp) = (u16)((s16)ax >> 2);
            u16 bx = DSW(DS_road_halfwidth_q4 + bp);
            if (c->mirror) bx >>= 1;
            u16 cx = (u16)((bx >> 2) + bx + ((bx >> 1) & 1));       /* shr,shr,adc */
            DSW(c->l + bp) = (u16)((s16)(u16)(dx - bx) >> 2);
            u16 di = (u16)((s16)(u16)(dx - cx) >> 2);
            DSW(c->ol + bp) = di;
            DSW(c->r + bp) = (u16)((s16)(u16)(dx + bx) >> 2);
            dx = (u16)((s16)(u16)(dx + cx) >> 2);
            DSW(c->orr + bp) = dx;
            ax = di;
        }

        /* tracking (0x2254 / 0x2689) */
        if (bp != 2 && (s16)ax < (s16)DSW(c->ol + bp - 2) && (s16)ax < (s16)DSW(c->min_ol)
            && si < DSW(c->min_ol_sy)) {
            DSW(c->min_ol) = ax;
            DSW(c->min_ol_sy) = si;
        }
        if ((s16)ax > (s16)DSW(c->cut_x)) DSW(c->row_limit) = bp;
        if ((s16)dx < (s16)DSW(c->cut_x)) {
            if (si <= DSW(c->min_ol_sy)) {
                DSW(c->cut_x) = dx;
                DSW(c->cut_sy) = si;
                DSW(c->cut_row) = bp;
                DSB(c->cut_top_sy) = DSB(c->top_sy);
            }
            if ((s16)dx <= (s16)(c->mirror ? 0xF0 : 0)) {
                if (si <= DSW(c->min_ol_sy)) DSW(c->cut_x) = c->mirror ? 0xE6 : 0xFFDF;
                DSW(c->row_limit) = bp;
            }
        }
    next:
        bp = (u16)(bp + 2);
        if (!c->mirror) {
            if (DSW(c->row_limit) != 0 && (s16)bp > 8) break;
            if (bp == 0x50) break;
        } else {
            if (DSW(c->row_limit) != 0 && (s16)bp > 8) break;
            DSW(DS_mirror_rows_done) = bp;
            if (bp == 0x32) break;
        }
    }
    DSW(c->row_limit) = bp;

    /* ---- phase 2: rebuild scanline edge arrays from the first changed row */
    bp = DSW(FC);
    if (bp == 0) return;
    if (DSB(DS_cached_top_sy) == DSB(c->top_sy)) return;
    DSB(DS_crest_sy) = DSB(DS_cached_top_sy);
    si = DSW(c->cached_top_row);
    for (;;) {
        u8 cl = DSB(c->sy + bp);
        u8 crest = DSB(DS_crest_sy);
        u8 dl = (u8)(crest - cl);
        if (crest > cl && cl != DSB(c->sy + si)) {
            if ((s16)bp >= (s16)DSW(c->cut_row)) DSW(DS_span_clamp_max) = (u16)(DSW(c->cut_x) + 6);
            DSB(DS_crest_sy) = cl;
            u16 di = (u16)((u16)cl << 1);
            if (dl == 1) {
                u16 mn = DSW(DS_span_clamp_min), mx = DSW(DS_span_clamp_max);
                const u16 src[4] = { c->ol, c->l, c->r, c->orr };
                const u16 dst[4] = { c->span_ol, c->span_l, c->span_r, c->span_or };
                for (int k = 0; k < 4; k++) {
                    u16 v = DSW(src[k] + bp);
                    if ((s16)v < (s16)mn) v = mn;
                    else if ((s16)v > (s16)mx) v = mx;
                    DSW(dst[k] + di) = v;
                }
            } else {
                u16 cxr = (u8)(DSB(c->sy + si) - cl);                /* sub cl,[si+sy] ; neg cl */
                interp_edge_span(DSW(c->ol + bp),  DSW(c->ol + si),  cxr, dl, (u16)(c->span_ol + di));
                interp_edge_span(DSW(c->l + bp),   DSW(c->l + si),   cxr, dl, (u16)(c->span_l + di));
                interp_edge_span(DSW(c->r + bp),   DSW(c->r + si),   cxr, dl, (u16)(c->span_r + di));
                interp_edge_span(DSW(c->orr + bp), DSW(c->orr + si), cxr, dl, (u16)(c->span_or + di));
            }
            si = bp;
            if (DSB(DS_crest_sy) == DSB(c->top_sy)) return;
        }
        bp = (u16)(bp + 2);
        if (bp == DSW(c->row_limit)) return;
    }
}

/* 0x2054 project_road_main — §4.4 */
void project_road_main(void) { project_road(&PROJ_MAIN); }

/* 0x2478 project_road_mirror — §4.4 (mirror table) */
void project_road_mirror(void) { project_road(&PROJ_MIRROR); }
