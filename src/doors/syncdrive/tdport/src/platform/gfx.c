/* Planar EGA graphics — port of the TDEGA hand-written graphics assembly (port/spec/platform.md §4.1–4.4a,
 * game_flow.md §5/§5b).
 *
 * Structure: every routine is transcribed from the disassembly. Bytes are read and written through
 * vrd()/vwr(), which address mem[] for RAM segments and an EGA hardware model for segment 0xA000.
 * The EGA model implements the sequencer map mask, the graphics controller set/reset, enable
 * set/reset, function (replace/AND/OR/XOR), read map, write modes 0/1/2 and bit mask, so the
 * screen-path register tricks (and their quirks: XOR partial bytes, clear_clip map-mask inheritance and
 * overshoot, draw_line leaving the bit mask at 0, ...) fall out of the transcription instead of being
 * special-cased. Plane storage is the full 64 KB per plane of the real adapter, so every 16-bit offset
 * overrun the original performs lands in (invisible) VRAM exactly like on the hardware.
 *
 * All target/descriptor state stays in code-segment memory (CS:528E pool pointer, CS:5A60 current
 * descriptor pointer, CS:5A64 live copy, CS:5A7C screen descriptor, CS:5A94 screen row table); all of
 * it is statically initialised in the load image. */
#include "gfx.h"
#include "res.h"
#include "../host.h"
#include "../symbols.h"

#include <string.h>

/* ------------------------------------------------------------------------------------------------ */
/* EGA hardware model (host-side: VRAM and adapter registers are not part of mem[]).                */

#define EGA_PLANE_BYTES 0x10000u

static struct {
    u8 plane[4][EGA_PLANE_BYTES];
    u8 latch[4];      /* last CPU read of all planes (write mode 1 source) */
    u8 map_mask;      /* sequencer index 2 */
    u8 set_reset;     /* GC index 0 */
    u8 enable_sr;     /* GC index 1 */
    u8 func;          /* GC index 3 (bits 3-4: 0 replace, 1 AND, 2 OR, 3 XOR) */
    u8 read_map;      /* GC index 4 */
    u8 mode;          /* GC index 5 (write mode in bits 0-1) */
    u8 bit_mask;      /* GC index 8 */
    u8 palette[17];   /* attribute controller palette registers + overscan (INT 10h AX=1002h) */
    bool dirty;
} ega;

/* Current CS offsets of the live descriptor copy. */
#define CUR_PLANE(k) CSW(0x5A66 + 2 * (k))
#define CUR_ROWTAB   CSW(0x5A6E)
#define CUR_STRIDE   CSW(GFX_CUR_STRIDE)

static void ega_reset_registers(void)
{
    ega.map_mask = 0x0F;
    ega.set_reset = 0;
    ega.enable_sr = 0;
    ega.func = 0;
    ega.read_map = 0;
    ega.mode = 0;
    ega.bit_mask = 0xFF;
}

static void gc_out(u8 index, u8 value)          /* out 3CEh,index ; out 3CFh,value */
{
    switch (index) {
    case 0: ega.set_reset = value; break;
    case 1: ega.enable_sr = value; break;
    case 3: ega.func = value; break;
    case 4: ega.read_map = value; break;
    case 5: ega.mode = value; break;
    case 8: ega.bit_mask = value; break;
    default: break;
    }
}

static void seq_map_mask(u8 value) { ega.map_mask = value; }   /* out 3C4h,2 ; out 3C5h,value */

static u8 ega_read(u16 off)
{
    for (int k = 0; k < 4; k++) ega.latch[k] = ega.plane[k][off];
    return ega.plane[ega.read_map & 3][off];
}

static void ega_write(u16 off, u8 v)
{
    for (int k = 0; k < 4; k++) {
        if (!(ega.map_mask >> k & 1)) continue;
        u8 d = ega.plane[k][off];
        u8 x;
        switch (ega.mode & 3) {
        case 1:
            ega.plane[k][off] = ega.latch[k];   /* write mode 1 ignores function and bit mask */
            continue;
        case 2:
            x = (v >> k & 1) ? 0xFF : 0x00;
            break;
        default:
            x = (ega.enable_sr >> k & 1) ? ((ega.set_reset >> k & 1) ? 0xFF : 0x00) : v;
            break;
        }
        switch (ega.func >> 3 & 3) {
        case 1: x &= d; break;
        case 2: x |= d; break;
        case 3: x ^= d; break;
        default: break;
        }
        ega.plane[k][off] = (u8)((x & ega.bit_mask) | (d & (u8)~ega.bit_mask));
    }
    ega.dirty = true;
}

static inline u8 vrd(u16 seg, u16 off) { return seg == VRAM_SEG ? ega_read(off) : rd8(seg, off); }
static inline void vwr(u16 seg, u16 off, u8 v)
{
    if (seg == VRAM_SEG) ega_write(off, v);
    else wr8(seg, off, v);
}

/* ------------------------------------------------------------------------------------------------ */
/* Flag helpers for the asm's signed branches.                                                       */

static inline bool slt(u16 a, u16 b) { return (s16)a < (s16)b; }   /* cmp a,b ; jl  */
static inline bool sle(u16 a, u16 b) { return (s16)a <= (s16)b; }  /* cmp a,b ; jle */
static inline bool sgt(u16 a, u16 b) { return (s16)a > (s16)b; }   /* cmp a,b ; jg  */
/* dec r ; jg  — taken while the true value old-1 is > 0 */
static inline bool dec_jg(u16 *r) { s16 old = (s16)*r; *r = (u16)(*r - 1); return old > 1; }
/* dec r ; jle — taken while the true value old-1 is <= 0 */
static inline bool dec_jle(u16 *r) { s16 old = (s16)*r; *r = (u16)(*r - 1); return old <= 1; }
static inline u8 ror8(u8 v, unsigned n) { n &= 7; return n ? (u8)(v >> n | v << (8 - n)) : v; }

/* Row table lookup through the live copy: CS:[CS:5A6E + y*2]. */
static inline u16 row_at(u16 y) { return CSW((u16)(y * 2 + CUR_ROWTAB)); }

/* ------------------------------------------------------------------------------------------------ */
/* Port setup / presentation                                                                         */

void gfx_init(void)
{
    memset(ega.plane, 0, sizeof ega.plane);
    memset(ega.latch, 0, sizeof ega.latch);
    ega_reset_registers();
    /* Mode 0Dh default palette (identical to DS:637C); only relevant before gfx_init_ega runs. */
    memcpy(ega.palette, mp(DGROUP, 0x637C), sizeof ega.palette);
    ega.dirty = true;
    /* CS:528E = 5290, CS:5A60 = far CS:5A7C, CS:5A64 = screen copy, CS:5A7C screen descriptor and the
     * CS:5A94 row table are statically initialised in the image (5A62 is relocated); nothing to do. */
    host_set_frame_source(gfx_compose, 320, 200);
}

static u32 ega_rgb(u8 v)
{
    /* 200-line EGA monitor decoding: bit0 B, bit1 G, bit2 R, bit4 intensity; R+G without intensity = brown. */
    u32 r = (v & 4) ? 0xAA : 0, g = (v & 2) ? 0xAA : 0, b = (v & 1) ? 0xAA : 0;
    if ((v & 0x17) == 0x06) g = 0x55;
    if (v & 0x10) { r += 0x55; g += 0x55; b += 0x55; }
    return r << 16 | g << 8 | b;
}

bool gfx_compose(u32 *xrgb)
{
    if (!ega.dirty) return false;
    ega.dirty = false;
    u32 pal[16];
    for (int i = 0; i < 16; i++) pal[i] = ega_rgb(ega.palette[i]);
    for (int y = 0; y < 200; y++) {
        for (int bx = 0; bx < 40; bx++) {
            u32 off = (u32)(y * 40 + bx);
            u8 p0 = ega.plane[0][off], p1 = ega.plane[1][off], p2 = ega.plane[2][off], p3 = ega.plane[3][off];
            for (int b = 0; b < 8; b++) {
                int s = 7 - b;
                int idx = (p0 >> s & 1) | (p1 >> s & 1) << 1 | (p2 >> s & 1) << 2 | (p3 >> s & 1) << 3;
                xrgb[y * 320 + bx * 8 + b] = pal[idx];
            }
        }
    }
    return true;
}

/* ------------------------------------------------------------------------------------------------ */
/* 0x4941 gfx_fill_rect — platform.md §4.3                                                          */

void gfx_fill_rect(s16 x, s16 y, s16 w, s16 h, u8 colour)
{
    u16 col = (u16)(x >> 3);                                   /* bp-0C */
    u8 dh = DSB(0x636C + (x & 7));                             /* left mask */
    u16 cx = (u16)(x + w - 1);
    u8 dl = DSB(0x6374 + (cx & 7));                            /* right mask */
    cx = (u16)(((s16)cx >> 3) - col);                          /* span = last byte - first byte */
    u16 di0 = (u16)(row_at((u16)y) + col);                     /* bp-06 */
    u16 skip = (u16)(CUR_STRIDE - cx);                         /* bp-08 */
    u16 n = (u16)(cx - 1);                                     /* bp-0A (dec cx) */

    if (CUR_PLANE(0) != VRAM_SEG) {
        if ((s16)cx > 1) {                                     /* jg: whole middle bytes */
            u8 c = colour;
            for (int k = 0; k < 4; k++, c >>= 1) {
                u16 seg = CUR_PLANE(k);
                if (!seg) continue;
                u16 di = di0, rows = (u16)h;
                u8 l = dh, r = dl, mid = 0xFF;
                bool set = c & 1;
                if (!set) { l = (u8)~dh; r = (u8)~dl; mid = 0; }
                do {
                    vwr(seg, di, set ? (u8)(vrd(seg, di) | l) : (u8)(vrd(seg, di) & l));
                    di++;
                    for (u16 m = n; m; m--) vwr(seg, di++, mid);
                    vwr(seg, di, set ? (u8)(vrd(seg, di) | r) : (u8)(vrd(seg, di) & r));
                    di = (u16)(di + skip);
                } while (dec_jg(&rows));
            }
        } else if (cx == 1) {                                  /* je: two partial bytes */
            u8 c = colour;
            for (int k = 0; k < 4; k++, c >>= 1) {
                u16 seg = CUR_PLANE(k);
                if (!seg) continue;
                u16 di = di0, cnt = (u16)h;
                bool set = c & 1;
                do {
                    if (set) {
                        vwr(seg, di, (u8)(vrd(seg, di) | dh)); di++;
                        vwr(seg, di, (u8)(vrd(seg, di) | dl));
                    } else {
                        vwr(seg, di, (u8)(vrd(seg, di) & (u8)~dh)); di++;
                        vwr(seg, di, (u8)(vrd(seg, di) & (u8)~dl));
                    }
                    di = (u16)(di + skip);
                } while (--cnt);
            }
        } else {                                               /* single byte */
            u8 m = dh & dl, im = (u8)~m, c = colour;
            for (int k = 0; k < 4; k++, c >>= 1) {
                u16 seg = CUR_PLANE(k);
                if (!seg) continue;
                u16 di = di0, cnt = (u16)h;
                do {
                    vwr(seg, di, (c & 1) ? (u8)(vrd(seg, di) | m) : (u8)(vrd(seg, di) & im));
                    di = (u16)(di + skip);
                } while (--cnt);
            }
        }
        return;
    }

    /* EGA: write mode 2, map mask FFh, bit mask per partial column (edges step by 40). */
    u8 bh = dh, bl = dl;
    gc_out(5, 2);
    seq_map_mask(0xFF);
    if ((s16)cx > 1) {
        gc_out(8, bh);
        u16 di = di0, cnt = (u16)h;
        do { ega_write(di, colour); di = (u16)(di + 0x28); } while (--cnt);
        gc_out(8, bl);
        di = (u16)(di0 + n + 1); cnt = (u16)h;
        do { ega_write(di, colour); di = (u16)(di + 0x28); } while (--cnt);
        gc_out(8, 0xFF);
        di = (u16)(di0 + 1);
        u16 rows = (u16)h, step = (u16)(skip + 1);
        do {
            u16 m = n;
            do { ega_write(di, colour); di++; } while (--m);
            di = (u16)(di + step);
        } while (dec_jg(&rows));
    } else if (cx == 1) {
        gc_out(8, bh);
        u16 di = di0, cnt = (u16)h;
        do { ega_write(di, colour); di = (u16)(di + 0x28); } while (--cnt);
        gc_out(8, bl);
        di = (u16)(di0 + 1); cnt = (u16)h;
        do { ega_write(di, colour); di = (u16)(di + 0x28); } while (--cnt);
    } else {
        gc_out(8, bh & bl);
        u16 di = di0, cnt = (u16)h;
        do { ega_write(di, colour); di = (u16)(di + 0x28); } while (--cnt);
    }
    gc_out(8, 0xFF);
    gc_out(5, 0);
}

/* 0x4B98 gfx_clear_screen — always A000h, 8000 bytes, write mode 2, map mask 0Fh */
void gfx_clear_screen(u8 colour)
{
    gc_out(5, 2);
    seq_map_mask(0x0F);
    for (u16 di = 0; di < 0x1F40; di++) ega_write(di, colour);
    gc_out(5, 0);
}

/* ------------------------------------------------------------------------------------------------ */
/* 0x4BD0 / 0x4BE7 text — platform.md §4.3, game_flow.md §5                                          */

static void text_render(const u8 *s)
{
    u8 eq = 0;
    if (CUR_PLANE(0) == VRAM_SEG) {
        u8 fg = DSB(DS_text_fg), bg = DSB(DS_text_bg);
        eq = (u8)~(fg ^ bg);                                   /* bp-10: planes where fg == bg */
        gc_out(1, eq);
        gc_out(0, fg & bg);
    }
    u16 stride = CUR_STRIDE;
    if (DSW(DS_text_enabled) == 1) {
        for (;;) {
            u8 ch = *s;
            if (ch == 0) break;
            s++;
            u16 glyph = DSW((u16)(ch * 2 + DSW(DS_text_font)));   /* near ptr in DGROUP */
            if (glyph == 0) {
                if (ch == 0x0D || ch == 0x0A) {
                    DSW(DS_text_x) = DSW(DS_text_margin_x);
                    DSW(DS_text_y) = (u16)(DSW(DS_text_y) + DSW(DS_text_adv_y));
                }
                continue;
            }
            u16 pos = (u16)((DSW(DS_text_x) >> 3) + row_at(DSW(DS_text_y)));
            if (CUR_PLANE(0) != VRAM_SEG) {
                u16 dx = (u16)(DSB(DS_text_fg) | (u8)(DSB(DS_text_bg) ^ DSB(DS_text_fg)) << 8);
                for (int k = 0; k < 4; k++, dx >>= 1) {
                    u16 seg = CUR_PLANE(k);
                    if (!seg) continue;
                    u16 cnt = DSW(DS_text_glyph_h), di = pos, si = glyph;
                    if (dx & 0x100) {
                        if (dx & 1) do { vwr(seg, di, rd8(DGROUP, si++)); di = (u16)(di + stride); } while (--cnt);
                        else do { vwr(seg, di, (u8)~rd8(DGROUP, si++)); di = (u16)(di + stride); } while (--cnt);
                    } else {
                        u8 v = (dx & 1) ? 0xFF : 0x00;
                        do { vwr(seg, di, v); di = (u16)(di + stride); } while (--cnt);
                    }
                }
            } else {
                seq_map_mask((u8)(DSB(DS_text_fg) | eq));
                u16 di = pos, cnt = DSW(DS_text_glyph_h), si = glyph;
                do { ega_write(di, rd8(DGROUP, si++)); di = (u16)(di + stride); } while (--cnt);
                u8 m = (u8)((DSB(DS_text_fg) ^ DSB(DS_text_bg)) & DSB(DS_text_bg));
                if (m) {
                    seq_map_mask((u8)(DSB(DS_text_bg) | eq));
                    di = pos; cnt = DSW(DS_text_glyph_h); si = glyph;
                    do { ega_write(di, (u8)~rd8(DGROUP, si++)); di = (u16)(di + stride); } while (--cnt);
                }
            }
            DSW(DS_text_x) = (u16)(DSW(DS_text_x) + DSW(DS_text_adv_x));
        }
    }
    gc_out(1, 0);                                              /* 0x4C34 */
}

void gfx_draw_text(const char *s, s16 x, s16 y)
{
    DSW(DS_text_x) = (u16)x;
    DSW(DS_text_y) = (u16)y;
    text_render((const u8 *)s);
}

void gfx_draw_text_at_cursor(const char *s) { text_render((const u8 *)s); }

/* 0x74E3 */
void gfx_set_text_cursor(s16 x, s16 y)
{
    DSW(DS_text_x) = (u16)x;
    DSW(DS_text_y) = (u16)y;
}

/* 0x6C85 */
void gfx_set_text_colours(u16 fg, u16 bg)
{
    DSW(DS_text_bg) = bg;
    DSW(DS_text_fg) = fg;
}

/* 0x9507 draw_text_centered: x = 0xA0 - strlen*4 */
void draw_text_centered(const char *s, s16 y)
{
    u16 ax = (u16)(strlen(s) << 2);
    gfx_draw_text(s, (s16)(u16)(0xA0 - ax), y);
}

/* 0x9530 draw_rect_outline */
void draw_rect_outline(s16 x0, s16 y0, s16 x1, s16 y1, u8 colour)
{
    s16 w = (s16)(x1 - x0 + 1), h = (s16)(y1 - y0 + 1);
    if (w <= 0 || h <= 0) return;
    gfx_fill_rect(x0, y0, w, 1, colour);
    gfx_fill_rect(x0, y1, w, 1, colour);
    gfx_fill_rect(x0, y0, 1, h, colour);
    gfx_fill_rect(x1, y0, 1, h, colour);
}

/* ------------------------------------------------------------------------------------------------ */
/* Palette / mode                                                                                   */

/* 0x4D84 INT 10h AX=1002h ES:DX = DS:table */
void gfx_set_palette(u16 ds_table)
{
    memcpy(ega.palette, mp(DGROUP, ds_table), sizeof ega.palette);
    ega.dirty = true;
}

static void bios_equipment_colour(void)
{
    wr16(0x40, 0x10, (u16)((rd16(0x40, 0x10) & 0xFFCF) | 0x10));
}

/* 0x4D96 gfx_init_ega */
void gfx_init_ega(void)
{
    gc_out(5, 0);
    gc_out(1, 0);
    gc_out(8, 0xFF);
    gc_out(3, 0);
    gfx_clear_screen(0);
    bios_equipment_colour();
    /* INT 10h AX=000Dh: the BIOS mode set clears video memory and loads the default register state. */
    memset(ega.plane, 0, sizeof ega.plane);
    ega_reset_registers();
    ega.dirty = true;
    gfx_set_palette(0x637C);
}

/* 0x50FE gfx_shutdown */
void gfx_shutdown(void)
{
    gfx_clear_screen(0);
    bios_equipment_colour();
    /* PORT: INT 10h mode 3 and AH=0Bh have no host equivalent; model the mode set as clear + reset. */
    memset(ega.plane, 0, sizeof ega.plane);
    ega_reset_registers();
    ega.dirty = true;
}

/* ------------------------------------------------------------------------------------------------ */
/* Targets                                                                                          */

/* 0x50A1 gfx_set_clip */
void gfx_set_clip(FarPtr desc, s16 x0, s16 x1, s16 y0, s16 y1)
{
    bool cur = CUR_PLANE(0) == rd16(desc.seg, (u16)(desc.off + 2));
    wr16(desc.seg, (u16)(desc.off + 0x0C), (u16)x0);
    wr16(desc.seg, (u16)(desc.off + 0x0E), (u16)x1);
    wr16(desc.seg, (u16)(desc.off + 0x10), (u16)y0);
    wr16(desc.seg, (u16)(desc.off + 0x12), (u16)y1);
    if (cur) {
        CSW(GFX_CUR_CLIP_X0) = (u16)x0;
        CSW(GFX_CUR_CLIP_X1) = (u16)x1;
        CSW(GFX_CUR_CLIP_Y0) = (u16)y0;
        CSW(GFX_CUR_CLIP_Y1) = (u16)y1;
    }
}

/* 0x5128 gfx_select_target */
void gfx_select_target(FarPtr desc)
{
    CSW(0x5A62) = desc.seg;
    CSW(0x5A60) = desc.off;
    for (u16 i = 0; i < 12; i++)
        CSW((u16)(GFX_CUR_OFF + 2 * i)) = rd16(desc.seg, (u16)(desc.off + 2 * i));
}

/* 0xA3B7 / 0xA3CB: 24 words CS:5A64.. <-> DS:6AC4 */
void gfx_target_save(void)
{
    for (u16 i = 0; i < 0x18; i++) DSW(DS_gfx_target_save_area + 2 * i) = CSW((u16)(GFX_CUR_OFF + 2 * i));
}

void gfx_target_restore(void)
{
    for (u16 i = 0; i < 0x18; i++) CSW((u16)(GFX_CUR_OFF + 2 * i)) = DSW(DS_gfx_target_save_area + 2 * i);
}

/* 0x5150 gfx_create_buffer */
FarPtr gfx_create_buffer(u16 w_bytes, u16 h, u16 plane_mask)
{
    u16 size = (u16)((s16)w_bytes * (s16)h);                   /* imul, low word */
    u16 pad = (u16)(-(size & 0x0F)) & 0x0F;
    size = (u16)(size + pad);
    u16 total = 0;
    for (int k = 0; k < 4; k++)
        if (plane_mask >> k & 1) total = (u16)(total + size);
    u16 paras = (u16)(((u16)(total + 0x10) >> 4) + 1);
    u16 seg = buf_alloc(paras);

    wr16(seg, 0, w_bytes);
    wr16(seg, 2, h);
    wr16(seg, 4, 0); wr16(seg, 6, 0); wr16(seg, 8, 0); wr16(seg, 0x0A, 0);
    for (u16 i = 0; i < 4; i++) wr8(seg, (u16)(0x0C + i), 0);
    u16 di = 0x0C;
    for (int k = 0; k < 4; k++) {
        u8 bit = (u8)(plane_mask & (1u << k));
        if (bit) wr8(seg, di++, bit);
    }
    wr8(seg, 0x0F, (u8)(rd8(seg, 0x0F) | (u8)(pad << 4)));

    u16 top = CSW(0x528E);
    u16 next = (u16)(((u16)(h + 0x0C) << 1) + top);
    if (next >= 0x5A60) fatal("%s", (const char *)mp(DGROUP, 0x638E));   /* "OUT OF ROW TABLE SPACE" */
    CSW(0x528E) = next;
    CSW(top) = 0;
    u16 ax = seg, step = (u16)(size >> 4), m = plane_mask;
    for (u16 k = 0; k < 4; k++, m >>= 1) {
        CSW((u16)(top + 2 + 2 * k)) = 0;
        if (m & 1) {
            CSW((u16)(top + 2 + 2 * k)) = ax;
            ax = (u16)(ax + step);
        }
    }
    CSW((u16)(top + 0x0A)) = (u16)(top + 0x18);
    CSW((u16)(top + 0x0C)) = 0;
    CSW((u16)(top + 0x0E)) = w_bytes;
    CSW((u16)(top + 0x14)) = w_bytes;
    CSW((u16)(top + 0x10)) = 0;
    CSW((u16)(top + 0x12)) = h;
    CSW((u16)(top + 0x16)) = pad;
    u16 cnt = h, off = 0x10, b = top;
    do {
        CSW((u16)(b + 0x18)) = off;
        b = (u16)(b + 2);
        off = (u16)(off + w_bytes);
    } while (--cnt);
    return far_make(CODE_SEG, top);
}

/* 0x79FF gfx_free_buffer: header read through (hdr_off, plane_seg[0]); pops the pool (LIFO). */
void gfx_free_buffer(FarPtr desc)
{
    u16 seg = rd16(desc.seg, (u16)(desc.off + 2));
    u16 hoff = rd16(desc.seg, desc.off);
    u16 h = rd16(seg, (u16)(hoff + 2));
    CSW(0x528E) = (u16)(CSW(0x528E) - (u16)((u16)(h + 0x0C) << 1));
    buf_free(seg);
}

/* 0x74F4 gfx_clear_clip */
void gfx_clear_clip(u8 colour)
{
    u16 rows = (u16)(CSW(GFX_CUR_CLIP_Y1) - CSW(GFX_CUR_CLIP_Y0));   /* bp-0E */
    u16 width = (u16)(CSW(GFX_CUR_CLIP_X1) - CSW(GFX_CUR_CLIP_X0));  /* bp-08 */
    u16 di0 = (u16)(row_at(CSW(GFX_CUR_CLIP_Y0)) + CSW(GFX_CUR_CLIP_X0));
    u16 skip = (u16)(CUR_STRIDE - width);                              /* bp-0C */

    if (CUR_PLANE(0) != VRAM_SEG) {
        if (width == CUR_STRIDE) {
            u16 count = (u16)((s16)(CUR_STRIDE >> 1) * (s16)rows);       /* imul dx, low word */
            u8 c = colour;
            for (int k = 0; k < 4; k++, c >>= 1) {
                u16 seg = CUR_PLANE(k);
                if (!seg) continue;
                u8 v = (c & 1) ? 0xFF : 0x00;
                u16 di = di0;
                for (u16 n = count; n; n--) { vwr(seg, di++, v); vwr(seg, di++, v); }   /* rep stosw */
            }
        } else {
            u8 c = colour;
            for (int k = 0; k < 4; k++, c >>= 1) {
                u16 seg = CUR_PLANE(k);
                if (!seg) continue;
                u8 v = (c & 1) ? 0xFF : 0x00;
                u16 di = di0, r = rows;
                do {
                    for (u16 n = width; n; n--) vwr(seg, di++, v);
                    di = (u16)(di + skip);
                } while (dec_jg(&r));
            }
        }
        return;
    }

    /* EGA: write mode 2, map mask NOT written (inherited from the previous primitive). */
    gc_out(5, 2);
    if (width == 0x28) {
        u16 count = (u16)((s16)(s8)(u8)width * (s16)(s8)(u8)rows);    /* imul dl (8-bit) */
        u16 di = di0;
        do { ega_write(di, colour); di++; } while (--count);
        gc_out(5, 0);
    } else {
        /* dec dx ; jle — fills the first row only when rows > 1 (and wraps all of VRAM when rows == 1);
         * returns without restoring write mode 0. */
        u16 di = di0, r = rows;
        do {
            u16 n = width;
            do { ega_write(di, colour); di++; } while (--n);
            di = (u16)(di + skip);
        } while (dec_jle(&r));
    }
}

/* ------------------------------------------------------------------------------------------------ */
/* Sprite blitters — platform.md §4.2; bodies 0x5E0E (clipped) and 0x6D48 (unclipped)                */

enum { OP_REPLACE, OP_OR, OP_AND, OP_XOR };
static const u8 op_gcfunc[4] = { 0x00, 0x10, 0x08, 0x18 };
static const u8 op_flags[4] = { 3, 2, 1, 4 };
enum { RECT_CLEAR, RECT_SET, RECT_XOR };

/* 0x97D4 / 0x981E / 0x9868: ES:DI dst, DX width, SI rows, CX row skip, BX shift (RAM planes) */
static void rect_ram(int op, u16 seg, u16 di, u16 dx, u16 si, u16 cx, u16 bx)
{
    if (bx == 0) {
        if (op == RECT_XOR) {
            do {
                u16 n = dx;
                do { vwr(seg, di, (u8)(vrd(seg, di) ^ 0xFF)); di++; } while (--n);
                di = (u16)(di + cx);
            } while (dec_jg(&si));
        } else {
            u8 v = op == RECT_SET ? 0xFF : 0x00;
            do {
                for (u16 n = dx; n; n--) vwr(seg, di++, v);
                di = (u16)(di + cx);
            } while (dec_jg(&si));
        }
        return;
    }
    u8 ah = CSB(0x97CC + bx), dh = (u8)~ah, dl = (u8)dx;
    if (dl == 1) {
        u16 n = si;
        do {
            switch (op) {
            case RECT_CLEAR: vwr(seg, di, (u8)(vrd(seg, di) & ah)); di++; vwr(seg, di, (u8)(vrd(seg, di) & dh)); break;
            case RECT_SET:   vwr(seg, di, (u8)(vrd(seg, di) | dh)); di++; vwr(seg, di, (u8)(vrd(seg, di) | ah)); break;
            default:         vwr(seg, di, (u8)(vrd(seg, di) ^ dh)); di++; vwr(seg, di, (u8)(vrd(seg, di) ^ ah)); break;
            }
            di = (u16)(di + cx);
        } while (--n);
        return;
    }
    dl--;
    do {
        switch (op) {
        case RECT_CLEAR:
            vwr(seg, di, (u8)(vrd(seg, di) & ah)); di++;
            for (u16 n = dl; n; n--) vwr(seg, di++, 0x00);
            vwr(seg, di, (u8)(vrd(seg, di) & dh));
            break;
        case RECT_SET:
            vwr(seg, di, (u8)(vrd(seg, di) | dh)); di++;
            for (u16 n = dl; n; n--) vwr(seg, di++, 0xFF);
            vwr(seg, di, (u8)(vrd(seg, di) | ah));
            break;
        default: {
            vwr(seg, di, (u8)(vrd(seg, di) ^ dh)); di++;
            u16 n = dl;
            do { vwr(seg, di, (u8)(vrd(seg, di) ^ 0xFF)); di++; } while (--n);
            vwr(seg, di, (u8)(vrd(seg, di) ^ ah));
            break;
        }
        }
        di = (u16)(di + cx);
    } while (dec_jg(&si));
}

/* 0x98C2 / 0x992B / 0x9994: EGA variants (read map / map mask set by the caller, GC function = blit op).
 * Transcribed as written: the shift != 0, width >= 2 paths write the middle bytes back with their own
 * value (mov ah,es:[di] ; mov es:[di],ah — XOR variant writes ah^FF) and thereby clobber AH, which the
 * next row's first byte (clear) or the last byte (set/xor) then uses as its mask. */
static void rect_ega(int op, u16 di, u16 dx, u16 si, u16 cx, u16 bx)
{
    if (bx == 0) {
        u8 al = op == RECT_CLEAR ? 0x00 : 0xFF;
        do {
            u16 n = dx;
            do { ega_write(di, al); di++; } while (--n);
            di = (u16)(di + cx);
        } while (dec_jg(&si));
        return;
    }
    u8 ah = CSB(0x98BA + bx), dh = (u8)~ah, dl = (u8)dx;
    if (dl == 1) {
        u16 n = si;
        do {
            u8 v = ega_read(di);
            v = op == RECT_CLEAR ? (u8)(v & ah) : op == RECT_SET ? (u8)(v | dh) : (u8)(v ^ dh);
            ega_write(di, v); di++;
            v = ega_read(di);
            v = op == RECT_CLEAR ? (u8)(v & dh) : op == RECT_SET ? (u8)(v | ah) : (u8)(v ^ ah);
            ega_write(di, v);
            di = (u16)(di + cx);
        } while (--n);
        return;
    }
    dl--;
    do {
        u8 v = ega_read(di);
        v = op == RECT_CLEAR ? (u8)(v & ah) : op == RECT_SET ? (u8)(v | dh) : (u8)(v ^ dh);
        ega_write(di, v); di++;
        u16 n = dl;
        do {
            ah = ega_read(di);
            if (op == RECT_XOR) ah ^= 0xFF;
            ega_write(di, ah); di++;
        } while (--n);
        v = ega_read(di);
        v = op == RECT_CLEAR ? (u8)(v & dh) : op == RECT_SET ? (u8)(v | ah) : (u8)(v ^ ah);
        ega_write(di, v);
        di = (u16)(di + cx);
    } while (dec_jg(&si));
}

static void blit_body(FarPtr spr, u16 px, u16 py, int op, bool clip)
{
    u16 ds = spr.seg, so = spr.off;
    u8 flags = op_flags[op];
    u16 w = rd16(ds, so), h = rd16(ds, (u16)(so + 2));
    u16 src = (u16)(so + 0x10);                                /* bp-28 */
    u16 rows, vis_w, row_skip = 0, col;                        /* bp-0E, bp-0C, bp-36, bp-08 */
    u8 edge = 1;                                               /* bp-3E */
    u16 shift = px & 7;                                        /* bp-34 */
    /* Lookup tables (clipped CS:5D5C/5D6C/5D7C/5D80, unclipped CS:6C96/6CA6/6CB6/6CBA) */
    u16 t_idx = clip ? 0x5D5C : 0x6C96, t_mask = clip ? 0x5D6C : 0x6CA6;
    u16 t_clr = clip ? 0x5D7C : 0x6CB6, t_bit = clip ? 0x5D80 : 0x6CBA;

    if (clip) {
        u16 bx = py, cx = h;
        u16 y0 = CSW(GFX_CUR_CLIP_Y0), y1 = CSW(GFX_CUR_CLIP_Y1);
        if (!slt(bx, y0)) {
            u16 ax = (u16)(bx + cx);
            if (sgt(ax, y1)) {
                u16 over = (u16)(ax - y1);
                if (!sgt(cx, over)) return;
                cx = (u16)(cx - over);
            }
        } else {
            bx = y0;
            u16 ax = (u16)(py + cx);
            if (sle(ax, bx)) return;
            u16 nr = (u16)(ax - bx);
            ax = (u16)(h - nr);
            src = (u16)(src + (u8)ax * rd8(ds, so));           /* mul byte ptr [si] */
            ax = (u16)(bx + nr);
            if (sgt(ax, y1)) {
                u16 over = (u16)(ax - y1);
                if (sle(nr, over)) return;
                nr = (u16)(nr - over);
            }
            cx = nr;
        }
        rows = cx;
        py = bx;

        cx = w;
        u16 dx = 0;
        bx = (u16)((s16)px >> 3);
        u16 x0 = CSW(GFX_CUR_CLIP_X0), x1 = CSW(GFX_CUR_CLIP_X1);
        if (!slt(bx, x0)) {
            u16 ax = (u16)(bx + cx);
            if (!slt(ax, x1)) {
                edge = 0;
                u16 over = (u16)(ax - x1);
                if (sle(cx, over)) return;
                cx = (u16)(cx - over);
                dx = over;
            }
        } else {
            u16 ax = (u16)(bx + cx);
            bx = x0;
            if (sle(ax, bx)) return;
            ax = (u16)(ax - bx);
            dx = (u16)(w - ax);
            cx = ax;
            src = (u16)(src + dx);
            edge |= 2;
        }
        vis_w = cx;
        row_skip = dx;
        col = bx;
    } else {
        rows = h;
        vis_w = w;
        col = (u16)((s16)px >> 3);
    }

    u16 dst0 = (u16)(col + row_at(py));                        /* bp-2A */
    u8 pm3 = rd8(ds, (u16)(so + 0x0F));
    u16 blocksize = (u16)((u8)h * rd8(ds, so));                /* bp-38 */
    if (pm3 & 0xF0) blocksize = (u16)(blocksize + (pm3 >> 4));
    u16 dstep = (u16)(CUR_STRIDE - vis_w);                     /* bp-2C */
    u8 pm0 = rd8(ds, (u16)(so + 0x0C)), pm1 = rd8(ds, (u16)(so + 0x0D));
    unsigned n_inv = 8 - shift;
    u8 keep_right = (u8)(0xFF >> shift), keep_left = (u8)~keep_right;

    if (CUR_PLANE(0) != VRAM_SEG) {
        /* destination list (seg, source) — processed last to first */
        u16 eseg[8], esrc[8];
        int n = 0;
        u16 p = (u16)(so + 0x0C), dxp = src;
        for (;;) {
            u16 bx = rd8(ds, p) & 0x0F;
            if (!bx) break;
            while (bx) {
                u16 c = bx;
                u8 idx = CSB(t_idx + bx);
                c &= CSB(t_clr + idx);
                u16 seg = CUR_PLANE(idx);
                bx = c;
                if (!seg) continue;
                if (n < 8) { eseg[n] = seg; esrc[n] = dxp; }
                n++;
            }
            if (n >= 4) break;                                 /* cmp di,8 ; jge */
            p++;
            dxp = (u16)(dxp + blocksize);
        }
        /* TODO(verify): n > 4 overflows the original's stack slots (never in shipped data); capped here. */
        if (n > 8) n = 8;

        if ((pm0 & 0xF0) && (flags & 1))
            for (int b = 0; b < 4; b++)
                if (pm0 & (0x10 << b) && CUR_PLANE(b))
                    rect_ram(RECT_CLEAR, CUR_PLANE(b), dst0, vis_w, rows, dstep, shift);
        if ((pm1 & 0xF0) && (flags & 6))
            for (int b = 0; b < 4; b++)
                if (pm1 & (0x10 << b) && CUR_PLANE(b))
                    rect_ram((flags & 4) ? RECT_XOR : RECT_SET, CUR_PLANE(b), dst0, vis_w, rows, dstep, shift);

        /* TODO(verify): with an empty destination list the original runs one pass with uninitialised
         * stack slots (seg [bp-18h], source [bp-20h]) — e.g. REPLACE of `blnk`. Not reproducible; skipped. */
        for (int e = n - 1; e >= 0; e--) {
            u16 seg = eseg[e], di = dst0, si = esrc[e], r = rows;
            do {
                u16 cnt = vis_w;
                if (shift == 0) {
                    if (op == OP_REPLACE) {
                        for (; cnt; cnt--) vwr(seg, di++, rd8(ds, si++));      /* rep movsb */
                    } else {
                        do {
                            u8 s = rd8(ds, si++), d = vrd(seg, di);
                            vwr(seg, di, op == OP_OR ? (u8)(d | s) : op == OP_AND ? (u8)(d & s) : (u8)(d ^ s));
                            di++;
                        } while (--cnt);
                    }
                } else {
                    u8 dh;
                    switch (op) {
                    case OP_REPLACE:
                        dh = (clip && (edge & 2)) ? (u8)(rd8(ds, (u16)(si - 1)) << n_inv)
                                                  : (u8)(vrd(seg, di) & keep_left);
                        break;
                    case OP_AND:
                        dh = keep_left;
                        if (clip && (edge & 2)) {
                            u8 s = rd8(ds, (u16)(si - 1));
                            dh = shift <= 3 ? (u8)(s >> shift) : (u8)(s << n_inv);   /* 0x8344 n=1..3 bug */
                        }
                        break;
                    default:
                        dh = 0;
                        if (clip && (edge & 2)) {
                            u8 s = rd8(ds, (u16)(si - 1));
                            dh = shift <= 3 ? (u8)(s >> shift) : (u8)(s << n_inv);   /* 0x4E2D/0x8094 bug */
                        }
                        break;
                    }
                    do {
                        u8 s = rd8(ds, si++);
                        u8 al = (u8)(s >> shift | dh);
                        dh = (u8)(s << n_inv);
                        switch (op) {
                        case OP_REPLACE: vwr(seg, di, al); break;
                        case OP_OR:  vwr(seg, di, (u8)(vrd(seg, di) | al)); break;
                        case OP_AND: vwr(seg, di, (u8)(vrd(seg, di) & al)); break;
                        default:     vwr(seg, di, (u8)(vrd(seg, di) ^ al)); break;
                        }
                        di++;
                    } while (--cnt);
                    if (!clip || (edge & 1)) {
                        u8 d = vrd(seg, di);
                        switch (op) {
                        case OP_REPLACE: vwr(seg, di, (u8)((d & keep_right) | dh)); break;
                        case OP_OR:  vwr(seg, di, (u8)(d | dh)); break;
                        case OP_AND: vwr(seg, di, (u8)(d & (dh | keep_right))); break;
                        default:     vwr(seg, di, (u8)(d ^ dh)); break;
                        }
                    }
                }
                di = (u16)(di + dstep);
                if (clip) si = (u16)(si + row_skip);
            } while (dec_jg(&r));
        }
        return;
    }

    /* EGA path (0x5FF7 / 0x6EAB): GC function = op, per-row entry list (map mask, read map, advance). */
    gc_out(3, op_gcfunc[op]);
    u16 adv_last = (u16)(blocksize - vis_w);                   /* bp-30 */
    u8 emask[16], eidx[16];
    u16 eadv[16];
    int n = 0;
    u16 back = 0;                                              /* bp-32 */
    u16 p = (u16)(so + 0x0C);
    for (int k = 0; k < 4; k++, p++) {
        u16 bx = rd8(ds, p) & 0x0F;
        if (!bx) break;
        u16 dx = (u16)-vis_w;
        do {
            u8 ah = CSB(t_idx + bx), al = CSB(t_mask + bx);
            emask[n] = al; eidx[n] = ah; eadv[n] = dx;
            n++;
            bx &= (u16)~al;
        } while (bx);
        eadv[n - 1] = adv_last;
        back = (u16)(back + blocksize);
    }
    back = (u16)(back - w);

    if ((pm0 & 0xF0) && (flags & 1))
        for (int b = 0; b < 4; b++)
            if (pm0 & (0x10 << b)) {
                gc_out(4, (u8)b);
                seq_map_mask(CSB(t_bit + b));
                rect_ega(RECT_CLEAR, dst0, vis_w, rows, dstep, shift);
            }
    if ((pm1 & 0xF0) && (flags & 6))
        for (int b = 0; b < 4; b++)
            if (pm1 & (0x10 << b)) {
                gc_out(4, (u8)b);
                seq_map_mask(CSB(t_bit + b));
                rect_ega((flags & 4) ? RECT_XOR : RECT_SET, dst0, vis_w, rows, dstep, shift);
            }

    /* TODO(verify): empty entry list -> the original runs one entry from uninitialised stack; skipped. */
    if (n > 0) {
        u16 si = src, di = dst0, r = rows;
        do {
            for (int e = 0; e < n; e++) {
                seq_map_mask(emask[e]);
                u16 cnt = vis_w;
                if (shift == 0) {
                    do { u8 al = rd8(ds, si++); ega_write(di, al); di++; } while (--cnt);
                } else {
                    gc_out(4, eidx[e]);
                    u8 dh = ega_read(di);
                    if (clip && (edge & 2)) dh = (u8)(rd8(ds, (u16)(si - 1)) << n_inv);
                    dh &= keep_left;
                    do {
                        u8 s = rd8(ds, si++);
                        u8 al = (u8)(s >> shift | dh);
                        dh = (u8)(s << n_inv);
                        ega_write(di, al);
                        di++;
                    } while (--cnt);
                    if (!clip || (edge & 1)) ega_write(di, (u8)((ega_read(di) & keep_right) | dh));
                }
                di = (u16)(di - vis_w);
                si = (u16)(si + eadv[e]);
            }
            di = (u16)(di + 0x28);
            si = (u16)(si - back);
        } while (dec_jg(&r));
    }
    gc_out(3, 0);
}

static inline u16 hdr_hot_x(FarPtr s) { return rd16(s.seg, (u16)(s.off + 4)); }
static inline u16 hdr_hot_y(FarPtr s) { return rd16(s.seg, (u16)(s.off + 6)); }
static inline u16 hdr_own_x(FarPtr s) { return (u16)(rd16(s.seg, (u16)(s.off + 8)) & 0xFFFC); }
static inline u16 hdr_own_y(FarPtr s) { return rd16(s.seg, (u16)(s.off + 0x0A)); }

#define BLIT_FAMILY(name_hot, name_raw, name_own, op, clip)                                              \
    void name_hot(FarPtr spr, s16 x, s16 y)                                                              \
    { blit_body(spr, (u16)((u16)x - hdr_hot_x(spr)), (u16)((u16)y - hdr_hot_y(spr)), op, clip); }       \
    void name_raw(FarPtr spr, s16 x, s16 y) { blit_body(spr, (u16)x, (u16)y, op, clip); }               \
    void name_own(FarPtr spr) { blit_body(spr, hdr_own_x(spr), hdr_own_y(spr), op, clip); }

/* 0x5D84/0x5DB6/0x5DE2 */ BLIT_FAMILY(blit_copy_clip_hot, blit_copy_clip_raw, blit_copy_clip_own, OP_REPLACE, true)
/* 0x4DF1/0x4E05/0x4E19 */ BLIT_FAMILY(blit_or_clip_hot, blit_or_clip_raw, blit_or_clip_own, OP_OR, true)
/* 0x8308/0x831C/0x8330 */ BLIT_FAMILY(blit_and_clip_hot, blit_and_clip_raw, blit_and_clip_own, OP_AND, true)
/* 0x8058/0x806C/0x8080 */ BLIT_FAMILY(blit_xor_clip_hot, blit_xor_clip_raw, blit_xor_clip_own, OP_XOR, true)
/* 0x6CBE/0x6CF0/0x6D1C */ BLIT_FAMILY(blit_copy_hot, blit_copy_raw, blit_copy_own, OP_REPLACE, false)
/* 0x7A37/0x7A4B/0x7A5F */ BLIT_FAMILY(blit_or_hot, blit_or_raw, blit_or_own, OP_OR, false)
/* 0x7C3B/0x7C4F/0x7C63 */ BLIT_FAMILY(blit_and_hot, blit_and_raw, blit_and_own, OP_AND, false)
/* 0x7E54/0x7E68/0x7E7C */ BLIT_FAMILY(blit_xor_hot, blit_xor_raw, blit_xor_own, OP_XOR, false)

/* 0x9A69 draw_glyph: XOR hotspot blit of the cursor sprite CS:[CS:9A19 + idx*2] */
void draw_glyph(s16 x, s16 y, s16 idx)
{
    u16 off = CSW((u16)(0x9A19 + (u16)idx * 2));
    blit_xor_hot(far_make(CODE_SEG, off), x, y);
}

/* ------------------------------------------------------------------------------------------------ */
/* 0x85D5 gfx_draw_line — platform.md §4.3                                                          */

static void line_plot_ram(u16 y, u16 x)
{
    u16 di = (u16)(row_at(y) + (x >> 3));
    u8 al = CSB(0x85CD + (x & 7));
    for (int k = 0; k < 4; k++) {
        u16 seg = CUR_PLANE(k);
        if (seg) vwr(seg, di, (u8)(vrd(seg, di) | al));        /* colour ignored */
    }
}

static void line_plot_ega(u16 y, u16 x, u8 colour)
{
    u16 di = (u16)(row_at(y) + (x >> 3));
    gc_out(8, CSB(0x85CD + (x & 7)));
    ega_write(di, colour);
}

static inline void add32(u16 *frac, u16 *ip, u16 sfrac, u16 sint)   /* add frac ; adc int */
{
    u32 f = (u32)*frac + sfrac;
    *frac = (u16)f;
    *ip = (u16)(*ip + sint + (f >> 16));
}

void gfx_draw_line(s16 x0, s16 y0, s16 x1, s16 y1, u8 colour)
{
    u16 cx0 = (u16)(CSW(GFX_CUR_CLIP_X0) << 3), cx1 = (u16)(CSW(GFX_CUR_CLIP_X1) << 3);   /* bp-14, bp-16 */
    u16 cy0 = CSW(GFX_CUR_CLIP_Y0), cy1 = CSW(GFX_CUR_CLIP_Y1);
    u16 X0 = (u16)x0, Y0 = (u16)y0, X1 = (u16)x1, Y1 = (u16)y1;

    if (Y0 == Y1) {
        if (slt(Y0, cy0) || !slt(Y0, cy1)) return;
        u16 cx = (u16)(X1 - X0), bx;
        if (cx & 0x8000) { cx = (u16)-cx; bx = X1; } else bx = X0;
        cx++;
        if (slt(bx, cx0)) {
            u16 t = (u16)(cx + bx);
            if (sle(t, cx0)) return;
            cx = (u16)(t - cx0);
            bx = cx0;
        }
        if (!slt(bx, cx1)) return;
        u16 ax = (u16)(cx + bx);
        if (sgt(ax, cx1)) cx = (u16)(cx - (u16)(ax - cx1));
        gfx_fill_rect((s16)bx, (s16)Y0, (s16)cx, 1, colour);
        return;
    }
    if (X0 == X1) {
        if (slt(X0, cx0) || !slt(X0, cx1)) return;
        u16 cx, si;
        if (slt(Y1, Y0)) { cx = (u16)(Y0 - Y1); si = Y1; } else { cx = (u16)(Y1 - Y0); si = Y0; }
        cx++;
        if (slt(si, cy0)) {
            u16 t = (u16)(cx + si);
            if (sle(t, cy0)) return;
            cx = (u16)(t - cy0);
            si = cy0;
        }
        if (!slt(si, cy1)) return;
        u16 ax = (u16)(cx + si);
        if (sgt(ax, cy1)) cx = (u16)(cx - (u16)(ax - cy1));
        gfx_fill_rect((s16)X0, (s16)si, 1, (s16)cx, colour);
        return;
    }

    u8 al = 0, ah = 0;
    u16 fx = 0, fy = 0;                                        /* bp-0A, bp-0E */
    u16 bx = (u16)(X1 - X0);
    if (bx & 0x8000) { bx = (u16)-bx; al = 1; }
    bx++;
    u16 cx = (u16)(Y1 - Y0);
    if (cx & 0x8000) { ah = 1; cx = (u16)-cx; }
    cx++;
    bool screen = CUR_PLANE(0) == VRAM_SEG;

    if (!slt(bx, cx)) {                                        /* x-major */
        u16 xi, yi;                                            /* bp-08 (dx), bp-0C */
        if (al) { xi = X1; yi = Y1; ah ^= 1; } else { xi = X0; yi = Y0; }
        u16 sfrac, sint;
        if (bx == cx) {
            sfrac = 0;
            sint = ah ? 0xFFFF : 1;
        } else {
            u16 q = div32_16((u32)cx << 16, bx, NULL);
            cx = bx;
            sfrac = q; sint = 0;
            if (ah) { sint = 0xFFFF; sfrac = (u16)-sfrac; }
        }
        u16 x = xi;
        for (;;) {                                             /* phase 1: skip while outside */
            if (!slt(yi, cy0) && slt(yi, cy1) && !slt(x, cx0)) {
                if (!slt(x, cx1)) return;
                break;
            }
            x++;
            add32(&fy, &yi, sfrac, sint);
            if (--cx == 0) return;
        }
        if (!screen) {
            do {
                if (slt(yi, cy0) || !slt(yi, cy1) || !slt(x, cx1)) return;
                line_plot_ram(yi, x);
                x++;
                add32(&fy, &yi, sfrac, sint);
            } while (--cx);
        } else {
            gc_out(5, 2);
            gc_out(3, 0x10);
            seq_map_mask(0x0F);
            do {
                if (slt(yi, cy0) || !slt(yi, cy1) || !slt(x, cx1)) break;
                line_plot_ega(yi, x, colour);
                x++;
                add32(&fy, &yi, sfrac, sint);
            } while (--cx);
            gc_out(5, 0);
            gc_out(3, 0);
            gc_out(8, 0);                                      /* leaves the bit mask at 0 (spec §4.4a) */
        }
        return;
    }

    /* y-major */
    u16 xi, si;                                                /* bp-08, si */
    if (ah) { xi = X1; si = Y1; al ^= 1; } else { xi = X0; si = Y0; }
    u16 q = div32_16((u32)bx << 16, cx, NULL);
    u16 sfrac = q, sint = 0;
    if (al) { sint = 0xFFFF; sfrac = (u16)-sfrac; }
    for (;;) {
        if (!slt(xi, cx0) && slt(xi, cx1) && !slt(si, cy0)) {
            if (!slt(si, cy1)) return;
            break;
        }
        si++;
        add32(&fx, &xi, 0xFFEE, 0xFFF0);                       /* assembler typo: constant, not the step */
        if (--cx == 0) return;
    }
    if (!screen) {
        do {
            if (!slt(si, cy1)) return;
            if (slt(xi, cx0) || !slt(xi, cx1)) return;
            line_plot_ram(si, xi);
            si++;
            add32(&fx, &xi, sfrac, sint);
        } while (--cx);
    } else {
        gc_out(5, 2);
        gc_out(3, 0x10);
        seq_map_mask(0x0F);
        do {
            if (!slt(si, cy1)) break;
            if (slt(xi, cx0) || !slt(xi, cx1)) break;
            line_plot_ega(si, xi, colour);
            si++;
            add32(&fx, &xi, sfrac, sint);
        } while (--cx);
        gc_out(5, 0);
        gc_out(8, 0);
        gc_out(3, 0);
    }
}

/* ------------------------------------------------------------------------------------------------ */
/* 0x768B gfx_dissolve — platform.md §4.3, game_flow.md §5b                                          */

#define DIS_COPY_LAST 0x77E0
#define DIS_COPY      0x7849
#define DIS_CLEAR     0x781D
#define DIS_SET       0x7832

void gfx_dissolve(FarPtr spr, u8 phase)
{
    u16 ds = spr.seg, so = spr.off;
    u16 es = CUR_PLANE(0);
    u16 hx = rd16(ds, (u16)(so + 8));                          /* bp-14 */
    u16 w = rd16(ds, so);                                      /* bp-16 */
    u16 step12 = (u16)((u8)w * 12);                            /* bp-0A (mul bl) */
    u16 psize = (u16)(rd8(ds, (u16)(so + 2)) * rd8(ds, so));   /* bp-32 */
    u16 adv = (u16)(psize - w);                                /* bp-1A */

    u16 emm[16], ert[16];
    int n = 0;
    u16 cx = 0;
    u16 p = (u16)(so + 0x0C);
    for (;;) {
        u16 bx = rd8(ds, p) & 0x0F;
        if (!bx) break;
        cx = (u16)(cx + psize);
        while (bx) {
            u16 ax = CSW((u16)(0x764B + bx * 2));
            u16 rt = CSW((u16)(0x766B + bx * 2));
            if (n < 16) { emm[n] = ax; ert[n] = rt; }
            n++;
            bx &= (u16)~ax;
        }
        if (n >= 4) break;
        p++;
    }
    u16 rewind = (u16)(cx - step12);                           /* bp-1C */
    for (int pmk = 0; pmk < 2; pmk++) {                        /* pm[0] high: clear, pm[1] high: set */
        u16 bx = (rd8(ds, (u16)(so + 0x0C + pmk)) >> 4) & 0x0F;
        while (bx) {
            u16 ax = CSW((u16)(0x764B + bx * 2));
            if (n < 16) { emm[n] = ax; ert[n] = pmk ? DIS_SET : DIS_CLEAR; }
            n++;
            bx &= (u16)~ax;
        }
    }
    if (n > 16) n = 16;

    u16 rt = (u16)(rd16(ds, (u16)(so + 0x0A)) * 2 + CUR_ROWTAB);   /* bp-0E */
    u16 h = rd16(ds, (u16)(so + 2));
    u16 rend = (u16)(rt + h + h);                              /* bp-10 */
    u16 data = (u16)(so + 0x10);                               /* bp-12 */

    /* TODO(verify): n == 0 runs one entry from uninitialised stack in the original; skipped. */
    for (s16 pass = 11; pass >= 0; pass--) {
        u8 r0 = CSB(0x7627 + pass);
        u16 si = (u16)(data + (u16)((u8)w * r0));
        u16 rp = (u16)(r0 * 2 + rt);
        while (rp < rend) {
            u16 d0 = (u16)(CSW(rp) + hx);                      /* bp-1E */
            u8 ah = CSB(0x7633 + (phase & 7));
            for (int e = 0; e < n; e++) {
                seq_map_mask((u8)emm[e]);
                gc_out(4, (u8)(emm[e] >> 8));
                u16 c = w, di = d0;
                u16 r = ert[e];
                bool body = true, rewind_w = true;
                if (r == DIS_CLEAR) {
                    ah = (u8)~ah;
                    u8 dl = (u8)(vrd(es, di) & ah);
                    vwr(es, di, dl); di++;
                    ah = ror8(ah, 1);
                    if (--c == 0) body = rewind_w = false;
                } else if (r == DIS_SET) {
                    u8 al = ah;
                    ah = (u8)~ah;
                    u8 dl = (u8)(vrd(es, di) & ah);
                    vwr(es, di, (u8)(al | dl)); di++;
                    ah = (u8)~ah;
                    ah = ror8(ah, 1);
                    if (--c == 0) body = rewind_w = false;
                }
                if (body) {
                    do {
                        u8 al = (u8)(rd8(ds, si++) & ah);
                        ah = (u8)~ah;
                        u8 dl = (u8)(vrd(es, di) & ah);
                        vwr(es, di, (u8)(al | dl)); di++;
                        ah = (u8)~ah;
                        ah = ror8(ah, 1);
                    } while (--c);
                }
                if (r == DIS_COPY_LAST) si = (u16)(si + adv);
                else if (rewind_w) si = (u16)(si - w);
            }
            phase++;
            rp = (u16)(rp + 0x18);
            si = (u16)(si - rewind);
        }
    }
}

/* ------------------------------------------------------------------------------------------------ */
/* 0x91CF gfx_scroll_window — write mode 1 row copy + one sprite row                                */

void gfx_scroll_window(s16 x, s16 y, s16 w, s16 h, s16 row_step, FarPtr spr, s16 srow)
{
    gc_out(5, 1);
    seq_map_mask(0x0F);
    u16 di = (u16)(row_at((u16)y) + ((u16)x >> 3));
    u16 si = (u16)(di + row_step);
    u16 bx = (u16)(row_step - w);
    u16 dx = (u16)h;
    do {
        for (u16 c = (u16)w; c; c--) {                        /* rep movsb within A000h */
            (void)ega_read(si++);
            ega_write(di++, 0);
        }
        si = (u16)(si + bx);
        di = (u16)(di + bx);
    } while (dec_jg(&dx));
    gc_out(5, 0);
    if (spr.seg == 0) return;

    u16 ds = spr.seg, so = spr.off;
    u16 bsz = (u16)((u8)rd16(ds, (u16)(so + 2)) * (u8)rd16(ds, so) + (rd8(ds, (u16)(so + 0x0F)) >> 4));
    u16 adv = (u16)(bsz - (u16)w);                             /* bp-0A */
    si = (u16)(so + 0x10 + (u16)((u8)srow * (u8)w));
    u16 pm = (u16)(so + 0x0C), d0 = di;
    for (int k = 0; k < 4; k++, pm++) {
        u8 cl = rd8(ds, pm) & 0x0F;
        if (!cl) break;
        seq_map_mask(cl);
        di = d0;
        u16 c = (u16)w;
        do { ega_write(di++, rd8(ds, si++)); } while (--c);
        si = (u16)(si + adv);
    }
}

/* ------------------------------------------------------------------------------------------------ */
/* Screen grabs                                                                                     */

/* 0x8B08 gfx_grab_screen: screen (read map, screen row table CS:5A86, stride 40) -> current target */
void gfx_grab_screen(s16 sx, s16 sy, s16 dx, s16 dy, s16 w_bytes, s16 h)
{
    u16 sxb = (u16)(sx >> 3), dxb = (u16)(dx >> 3);
    u16 src0 = (u16)(CSW((u16)(sy * 2 + CSW(0x5A86))) + sxb);   /* bp-08 */
    u16 sskip = (u16)(0x28 - w_bytes);                          /* bp-0C */
    u16 dst0 = (u16)(row_at((u16)dy) + dxb);                    /* bp-0A */
    u16 dskip = (u16)(CUR_STRIDE - w_bytes);                    /* bp-0E */
    for (u16 bx = 0; bx < 8; bx += 2) {
        gc_out(4, (u8)(bx >> 1));
        u16 seg = CSW((u16)(0x5A66 + bx));
        if (!seg) continue;
        u16 si = src0, di = dst0, rows = (u16)h;
        do {
            for (u16 c = (u16)w_bytes; c; c--) vwr(seg, di++, ega_read(si++));
            si = (u16)(si + sskip);
            di = (u16)(di + dskip);
        } while (dec_jg(&rows));
    }
}

/* 0x8C00 grab_into_sprite_raw: screen -> the sprite's stored planes at (x, y), planes stored contiguously
 * (no padding skip) */
void grab_into_sprite_raw(FarPtr spr, s16 x, s16 y)
{
    u16 ds = spr.seg, so = spr.off;
    u16 w = rd16(ds, so), h = rd16(ds, (u16)(so + 2));
    u16 src0 = (u16)(CSW((u16)(y * 2 + CSW(0x5A86))) + (u16)(x >> 3));   /* bp-10 */
    u16 sskip = (u16)(0x28 - w);                                          /* bp-12 */
    u16 pm = (u16)(so + 0x0C), di = (u16)(so + 0x10);
    for (int k = 0; k < 4; k++, pm++) {
        u16 bx = rd16(ds, pm) & 0x0F;
        if (!bx) continue;
        gc_out(4, CSB(0x8BCC + bx));
        u16 si = src0, rows = h;
        do {
            for (u16 c = w; c; c--) vwr(ds, di++, ega_read(si++));
            si = (u16)(si + sskip);
        } while (dec_jg(&rows));
    }
}
