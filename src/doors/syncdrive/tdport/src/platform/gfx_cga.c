/* CGA graphics — port of the TDCGA.EXE hand-written graphics assembly (spec: port/cga/graphics.md).
 *
 * TDCGA keeps the TDEGA target model (12-word descriptors in code-segment memory, row tables, clip in
 * byte columns) but every routine only uses plane_seg[0]: a target is one 2-bits-per-pixel bitmap,
 * 4 pixels per byte, MSB first. There is no separate screen path: the screen is just a target whose
 * segment is B800h and whose row table interlaces the two CGA banks (row 2k -> B800:0000 + 80k,
 * row 2k+1 -> B800:2000 + 80k). Bytes are read and written through vrd()/vwr(), which address mem[]
 * for RAM segments and the host-side video RAM for segment B800h.
 *
 * Video hardware model (host side, not in mem[]):
 *   CGA       16 KB of RAM decoded twice in B800:0000-7FFF (B800:4000 mirrors B800:0000), colour
 *             select register 3D9h, BIOS mode (4 = 320x200 graphics).
 *   Hercules  (TD_HERC) 32 KB page 1 at B800:0000-7FFF, mapped once 3BFh bit 1 is set; mode control
 *             3B8h, the 12 CRTC registers herc_init programs.
 * Offsets B800:8000-FFFF are not video RAM on either card (C000:0000..): writes are dropped, reads
 * give FFh (open bus).
 *
 * All target/descriptor state stays in code-segment memory: CS:4F61 pool pointer (pool CS:4F63..),
 * CS:5734/5736 current descriptor seg/off, CS:5738 live copy, CS:5750 screen descriptor, CS:5768 screen
 * row table. It is all statically initialised in the load image (CS:5734 is relocated). */
#include "gfx.h"
#include "res.h"
#include "../host.h"
#include "../symbols.h"

#include <string.h>

#if !TD_CGA
#error "gfx_cga.c is the TDCGA graphics layer; build it with TD_CGA=1 (gfx.c is the EGA one)"
#endif

/* ------------------------------------------------------------------------------------------------ */
/* Code-segment and DGROUP addresses used here that symbols.h does not name for TDCGA               */

#define CUR_SEG          CSW(0x573A)     /* live copy +02: plane_seg[0], the only plane TDCGA uses */
#define CUR_ROWTAB       CSW(0x5742)     /* live copy +0A */
#define POOL_TOP         0x4F61          /* CS: descriptor pool bump pointer (EGA CS:528E) */
#define POOL_LIMIT       0x5733          /* create_buffer: fatal when the new top is >= this */
#define CUR_PTR_SEG      0x5734          /* CS: far pointer to the selected descriptor, segment first */
#define CUR_PTR_OFF      0x5736
#define SCREEN_ROWTAB    0x575A          /* CS: screen descriptor +0A (row table pointer, = CS:5768) */
#define GLYPH_SPRITES    0x7FA6          /* CS: draw_glyph sprite offset table (EGA CS:9A19) */
#define DIS_ROW_ORDER    0x6338          /* CS: dissolve row order 0B 05 08 02 0A 04 07 01 09 03 06 00 */
#define DIS_BIT_MASK     0x6344          /* CS: dissolve first-bit masks 01 08 40 02 10 80 04 20 */
#define LINE_LEFT_MASK   0x6D87          /* CS: FF 3F 0F 03 */
#define LINE_FIRST_LEN   0x6D8B          /* CS: 04 03 02 01 (pixels in the first byte) */
#define LINE_RIGHT_MASK  0x6D8F          /* CS: 00 C0 F0 FC */
#define LINE_PIXEL_MASK  0x6D93          /* CS: C0 30 0C 03 */

#define DS_FILL_LEFT     0x633C          /* 00 3F 0F 03 (EGA DS:636C) */
#define DS_FILL_RIGHT    0x6340          /* 00 C0 F0 FC (EGA DS:6374) */
#define DS_TEXT_PATTERN  0x6344          /* colour index -> pattern word: 0000 5555 AAAA FFFF */
#define DS_STR_ROWTAB    0x634C          /* "OUT OF ROW TABLE SPACE" (EGA DS:638E) */
#define DS_HERC_CRTC     0x661C          /* 38 28 2D 0A 7F 06 64 70 02 02 06 07 (EGA DS:6642) */
#define DS_MDA_CRTC      0x6628          /* 61 50 52 0F 19 06 19 19 02 0D 0B 0C (EGA DS:664E) */
#define DS_FONT_EXPAND   0x66E8          /* 256 words: font byte with every bit doubled (8 -> 16 bits) */
#define DS_TEXT_GLYPH_H  0x68F4          /* EGA DS:691A (TDCGA text globals = EGA - 26h) */
#define DS_TEXT_ADV_X    0x68FA          /* EGA DS:6920 */
#define DS_TEXT_ENABLED  0x68FE          /* EGA DS:6924 */

/* BIOS data area (0040:xxxx) fields touched by the mode routines. */
#define BDA_EQUIPMENT    0x10
#define BDA_CRT_MODE     0x49
#define BDA_CRT_PALETTE  0x66

/* ------------------------------------------------------------------------------------------------ */
/* Video hardware model                                                                              */

#define VRAM_BYTES 0x8000u

static struct {
    u8 ram[VRAM_BYTES];  /* B800:0000-7FFF as the CPU sees it (CGA uses 0000-3FFF, see vram_at) */
    u8 cga_mode;         /* last INT 10h mode set on the colour adapter (4 = 320x200 4-colour) */
    u8 color_select;     /* CGA 3D9h */
    u8 herc_config;      /* Hercules 3BFh (bit 0 allow graphics, bit 1 map page 1 at B800h) */
    u8 herc_mode;        /* Hercules 3B8h (bit 1 graphics, bit 3 video on, bit 7 display page 1) */
    u8 herc_crtc[12];    /* Hercules 6845 R0..R11 */
    bool dirty;
} vid;

#if TD_HERC
static u32 herc_phosphor = 0x33FF33;   /* port option (host-side), gfx_set_monitor() */
#endif

static u8 *vram_at(u16 off)
{
#if TD_HERC
    if (!(vid.herc_config & 2) || off >= VRAM_BYTES) return NULL;   /* page 1 not mapped / not RAM */
    return &vid.ram[off];
#else
    if (off >= VRAM_BYTES) return NULL;
    return &vid.ram[off & 0x3FFF];      /* 16 KB decoded twice in B800:0000-7FFF */
#endif
}

static inline u8 vrd(u16 seg, u16 off)
{
    if (seg != VRAM_SEG) return rd8(seg, off);
    u8 *p = vram_at(off);
    return p ? *p : 0xFF;
}

static inline void vwr(u16 seg, u16 off, u8 v)
{
    if (seg != VRAM_SEG) { wr8(seg, off, v); return; }
    u8 *p = vram_at(off);
    if (p) { *p = v; vid.dirty = true; }
}

static void vram_clear(void)            /* rep stosw 4000h words of 0 at B800:0000 */
{
    for (u32 off = 0; off < VRAM_BYTES; off++) vwr(VRAM_SEG, (u16)off, 0);
}

/* INT 10h AH=00h. Only the effects visible to the port are modelled. On a CGA (IBM BIOS) the mode set
 * programs the adapter, loads 3D9h with 30h (3Fh in mode 6) and clears the video buffer. In Hercules
 * builds the colour adapter is absent: the BIOS still clears B800h (reaching Hercules page 1 once
 * herc_init has mapped it) and for mode 7 programs the monochrome adapter (3B8h = 29h, text). */
static void bios_set_mode(u8 mode)
{
    wr8(0x40, BDA_CRT_MODE, mode);
    if (mode == 7) {
#if TD_HERC
        memcpy(vid.herc_crtc, mp(DGROUP, DS_MDA_CRTC), sizeof vid.herc_crtc);
        vid.herc_mode = 0x29;
#endif
    } else {
        u8 pal = mode == 6 ? 0x3F : 0x30;
        wr8(0x40, BDA_CRT_PALETTE, pal);
        vid.cga_mode = mode;
        vid.color_select = pal;
        vram_clear();
    }
    vid.dirty = true;
}

/* INT 10h AH=0Bh (IBM BIOS SET_COLOR): BH=0 sets the low 5 bits of the colour select register from BL
 * (background colour and, in graphics modes, the intensity bit 4 of colours 1-3); BH=1 sets bit 5
 * (palette) from BL bit 0. The register copy lives in 0040:0066. */
static void bios_set_color(u8 bh, u8 bl)
{
    u8 pal = rd8(0x40, BDA_CRT_PALETTE);
    if (bh == 0) pal = (u8)((pal & 0xE0) | (bl & 0x1F));
    else pal = (u8)((pal & 0xDF) | ((bl & 1) ? 0x20 : 0x00));
    wr8(0x40, BDA_CRT_PALETTE, pal);
    vid.color_select = pal;     /* out 3D9h */
    vid.dirty = true;
}

static void bios_equipment_video(u16 bits)          /* 0040:0010 = (x & FFCFh) | bits */
{
    wr16(0x40, BDA_EQUIPMENT, (u16)((rd16(0x40, BDA_EQUIPMENT) & 0xFFCF) | bits));
}

/* ------------------------------------------------------------------------------------------------ */
/* Flag helpers for the asm's signed branches                                                       */

static inline bool slt(u16 a, u16 b) { return (s16)a < (s16)b; }   /* cmp a,b ; jl  */
static inline bool sle(u16 a, u16 b) { return (s16)a <= (s16)b; }  /* cmp a,b ; jle */
static inline bool sgt(u16 a, u16 b) { return (s16)a > (s16)b; }   /* cmp a,b ; jg  */
/* dec r ; jg  — taken while the true value old-1 is > 0 */
static inline bool dec_jg(u16 *r) { s16 old = (s16)*r; *r = (u16)(*r - 1); return old > 1; }
static inline u16 swap16(u16 v) { return (u16)(v << 8 | v >> 8); }   /* xchg al,ah */
static inline u8 ror8(u8 v) { return (u8)(v >> 1 | v << 7); }

/* Row table lookup through the live copy: CS:[CS:5742 + y*2]. */
static inline u16 row_at(u16 y) { return CSW((u16)(y * 2 + CUR_ROWTAB)); }

/* ------------------------------------------------------------------------------------------------ */
/* Port setup / presentation                                                                         */

void gfx_init(void)
{
    memset(&vid, 0, sizeof vid);
    vid.cga_mode = 3;                   /* DOS text mode until gfx_init_ega */
    vid.color_select = 0x30;
    vid.herc_mode = 0x29;               /* monochrome text mode until herc_init */
    vid.dirty = true;
    /* CS:4F61 = 4F63, CS:5734/5736 = far CS:5750, CS:5738 = screen copy, CS:5750 screen descriptor and
     * the CS:5768 row table are statically initialised in the image (5734 is relocated); nothing to do. */
#if TD_HERC
    host_set_frame_source(gfx_compose, 640, 300);
#else
    host_set_frame_source(gfx_compose, 320, 200);
#endif
}

#if TD_HERC
bool gfx_set_monitor(const char *name)
{
    static const struct { const char *name; u32 rgb; } mon[] = {
        { "green", 0x33FF33 }, { "amber", 0xFFB000 }, { "white", 0xFFFFFF },
    };
    for (size_t i = 0; i < sizeof mon / sizeof mon[0]; i++) {
        const char *a = name, *b = mon[i].name;
        while (*a && (*a | 0x20) == *b) { a++; b++; }
        if (!*a && !*b) {
            herc_phosphor = mon[i].rgb;
            vid.dirty = true;
            return true;
        }
    }
    return false;
}

/* 640x300 from Hercules page 1. The CRTC table DS:661C gives R1 = 28h characters of 16 pixels (80 bytes,
 * 640 pixels), R6 = 64h rows and R9 = 2 (3 scan lines per row). In graphics mode the card addresses
 * RAM as (RA << 13) + MA*2 + half-character, so scan line s of row r shows the byte row
 * B800:(s * 2000h + r * 80) with one bit per pixel: s = 0 is CGA line 2r, s = 1 is CGA line 2r+1 and
 * s = 2 is bank 2 (B800:4000..), which herc_init clears and nothing draws to. Every 2-bit CGA pixel
 * therefore appears as its two bits side by side. Text mode, video off or page 0 display as black. */
bool gfx_compose(u32 *xrgb)
{
    if (!vid.dirty) return false;
    vid.dirty = false;
    memset(xrgb, 0, 640u * 300u * sizeof *xrgb);
    if ((vid.herc_mode & 0x8A) != 0x8A || (vid.herc_config & 3) != 3) return true;
    unsigned chars = vid.herc_crtc[1], rows = vid.herc_crtc[6], lines = (unsigned)(vid.herc_crtc[9] & 0x1F) + 1;
    unsigned w = chars * 16 < 640 ? chars * 16 : 640;
    for (unsigned r = 0; r < rows; r++) {
        for (unsigned s = 0; s < lines; s++) {
            unsigned y = r * lines + s;
            if (y >= 300) return true;
            u32 base = (u32)((s & 3) << 13) + r * chars * 2;
            for (unsigned x = 0; x < w; x++) {
                u32 off = base + (x >> 3);
                u8 b = off < VRAM_BYTES ? vid.ram[off] : 0;
                if (b >> (7 - (x & 7)) & 1) xrgb[y * 640 + x] = herc_phosphor;
            }
        }
    }
    return true;
}
#else
static const u32 cga_rgbi[16] = {
    0x000000, 0x0000AA, 0x00AA00, 0x00AAAA, 0xAA0000, 0xAA00AA, 0xAA5500, 0xAAAAAA,
    0x555555, 0x5555FF, 0x55FF55, 0x55FFFF, 0xFF5555, 0xFF55FF, 0xFFFF55, 0xFFFFFF,
};

/* 320x200 from the CGA banks (even lines B800:0000, odd lines B800:2000, 80 bytes per line).
 * Mode 4 colours come from the colour select register: colour 0 = low nibble (background), colours 1-3
 * = palette 0 (green/red/brown) or palette 1 (cyan/magenta/grey) by bit 5, intensified by bit 4.
 * gfx_init_ega leaves 3D9h = 20h: palette 1, low intensity, black background -> black, cyan #00AAAA,
 * magenta #AA00AA, light grey #AAAAAA (the mode set loads 30h, the final AH=0Bh BH=0 BL=0 clears
 * bit 4; see port/cga/graphics.md). Text modes display as black. */
bool gfx_compose(u32 *xrgb)
{
    if (!vid.dirty) return false;
    vid.dirty = false;
    if (vid.cga_mode < 4 || vid.cga_mode > 6) {
        memset(xrgb, 0, 320u * 200u * sizeof *xrgb);
        return true;
    }
    u8 cs = vid.color_select;
    u8 hi = (cs & 0x10) ? 8 : 0;
    u32 pal[4] = {
        cga_rgbi[cs & 0x0F],
        cga_rgbi[((cs & 0x20) ? 3 : 2) | hi],
        cga_rgbi[((cs & 0x20) ? 5 : 4) | hi],
        cga_rgbi[((cs & 0x20) ? 7 : 6) | hi],
    };
    for (int y = 0; y < 200; y++) {
        u32 base = (u32)((y & 1) ? 0x2000 : 0) + (u32)(y >> 1) * 80;
        for (int bx = 0; bx < 80; bx++) {
            u8 b = vid.ram[base + bx];
            for (int p = 0; p < 4; p++)
                xrgb[y * 320 + bx * 4 + p] = pal[b >> (6 - 2 * p) & 3];
        }
    }
    return true;
}
#endif

/* ------------------------------------------------------------------------------------------------ */
/* 0x4857 gfx_fill_rect — pattern fill, no clipping                                                  */

/* Pixels [x, x+w) x [y, y+h) of the current target (byte column = x >> 2) get the pattern: even rows
 * use the high byte, odd rows the low byte. Partial edge bytes keep the destination pixels outside the
 * rectangle (masks DS:633C / DS:6340). No clipping; loop counts behave like the asm for w/h <= 0. */
void gfx_fill_rect_pat(s16 x, s16 y, s16 w, s16 h, u16 pattern)
{
    enum { ONE, LEFT_MID, LEFT_MID_RIGHT, LEFT_RIGHT, MID_RIGHT, MID } path;
    u16 es = CUR_SEG;
    u16 col = (u16)(x >> 2);                                   /* bp-0E */
    u8 keep_l = 0, keep_r = 0;                                 /* bp-05, bp-06 */
    u16 pat_l = 0, pat_r = 0;                                  /* bp-08, bp-0A */
    u16 mid = 0;                                               /* bp-0C */
    u16 rows = (u16)h;                                         /* [bp+0A] */
    u16 end = (u16)(x + w);

    if (x & 3) {
        u8 cl = DSB(DS_FILL_LEFT + (x & 3));
        pat_l = (u16)(pattern & (cl * 0x0101u));
        keep_l = (u8)~cl;
        if ((end & 3) == 0) {                                  /* 0x4936 */
            u16 span = (u16)(((s16)end >> 2) - col);
            if ((s16)span - 1 <= 0) path = ONE;                /* dec dx ; jle */
            else { mid = (u16)(span - 1); path = LEFT_MID; }
        } else {
            cl = DSB(DS_FILL_RIGHT + (end & 3));
            pat_r = (u16)(pattern & (cl * 0x0101u));
            keep_r = (u8)~cl;
            u16 span = (u16)(((s16)end >> 2) - col);
            s32 r = (s32)(s16)span - 1;                        /* dec dx ; jg / je */
            if (r > 0) { mid = (u16)(span - 1); path = LEFT_MID_RIGHT; }
            else if (r == 0) path = LEFT_RIGHT;
            else {                                             /* both edges in one byte */
                keep_l |= keep_r;
                pat_l &= (u16)(cl * 0x0101u);
                path = ONE;
            }
        }
    } else {
        if ((end & 3) == 0) {                                  /* 0x4AA1 */
            mid = (u16)(((s16)end >> 2) - col);
            path = MID;
        } else {
            u8 cl = DSB(DS_FILL_RIGHT + (end & 3));
            pat_r = (u16)(pattern & (cl * 0x0101u));
            keep_r = (u8)~cl;
            s16 last = (s16)((s16)end >> 2);
            u16 span = (u16)(last - (s16)col);
            if ((s32)last - (s32)(s16)col > 0) { mid = span; path = MID_RIGHT; }   /* sub ; jg */
            else { keep_l = keep_r; pat_l = pat_r; path = ONE; }
        }
    }

    bool even = !(y & 1);
    u16 rp = (u16)(y * 2 + CUR_ROWTAB);
    u16 ax = pattern;
    switch (path) {
    case ONE: {                                                /* 0x4902 */
        u16 dx = even ? swap16(pat_l) : pat_l;
        u16 cx = rows;
        do {                                                   /* loop */
            u16 di = (u16)(CSW(rp) + col);
            vwr(es, di, (u8)((vrd(es, di) & keep_l) | (u8)dx));
            dx = swap16(dx);
            rp = (u16)(rp + 2);
        } while (--cx);
        break;
    }
    case LEFT_MID:                                             /* 0x4940 */
    case LEFT_MID_RIGHT:                                       /* 0x498D */
        if (even) {
            ax = swap16(ax);
            pat_l = swap16(pat_l);
            if (path == LEFT_MID_RIGHT) pat_r = swap16(pat_r);
        }
        do {
            u16 di = (u16)(CSW(rp) + col);
            vwr(es, di, (u8)((vrd(es, di) & keep_l) | (u8)pat_l));
            pat_l = swap16(pat_l);
            di++;
            for (u16 cx = mid; cx; cx--) vwr(es, di++, (u8)ax);   /* rep stosb */
            ax = swap16(ax);
            if (path == LEFT_MID_RIGHT) {
                vwr(es, di, (u8)((vrd(es, di) & keep_r) | (u8)pat_r));
                pat_r = swap16(pat_r);
            }
            rp = (u16)(rp + 2);
        } while (dec_jg(&rows));
        break;
    case LEFT_RIGHT: {                                         /* 0x49FB */
        if (even) { pat_l = swap16(pat_l); pat_r = swap16(pat_r); }
        u16 cx = rows;
        do {                                                   /* loop */
            u16 di = (u16)(CSW(rp) + col);
            vwr(es, di, (u8)((vrd(es, di) & keep_l) | (u8)pat_l));
            pat_l = swap16(pat_l);
            di++;
            vwr(es, di, (u8)((vrd(es, di) & keep_r) | (u8)pat_r));
            pat_r = swap16(pat_r);
            rp = (u16)(rp + 2);
        } while (--cx);
        break;
    }
    case MID_RIGHT:                                            /* 0x4A55 */
        if (even) { ax = swap16(ax); pat_r = swap16(pat_r); }
        do {
            u16 di = (u16)(CSW(rp) + col);
            for (u16 cx = mid; cx; cx--) vwr(es, di++, (u8)ax);
            ax = swap16(ax);
            vwr(es, di, (u8)((vrd(es, di) & keep_r) | (u8)pat_r));
            pat_r = swap16(pat_r);
            rp = (u16)(rp + 2);
        } while (dec_jg(&rows));
        break;
    case MID:                                                  /* 0x4AA1 */
        if (even) ax = swap16(ax);
        do {
            u16 di = (u16)(CSW(rp) + col);
            for (u16 cx = mid; cx; cx--) vwr(es, di++, (u8)ax);
            ax = swap16(ax);
            rp = (u16)(rp + 2);
        } while (dec_jg(&rows));
        break;
    }
}

/* TDCGA callers pass uniform patterns (0000h, AAAAh, FFFFh); the EGA-shaped API carries one byte. */
void gfx_fill_rect(s16 x, s16 y, s16 w, s16 h, u8 colour)
{
    gfx_fill_rect_pat(x, y, w, h, (u16)(colour * 0x0101u));
}

/* 0x4AD8 gfx_clear_screen: always B800h, rep stosw FA0h words at 0000h and at 2000h (both banks). */
void gfx_clear_screen_pat(u16 pattern)
{
    for (u16 i = 0; i < 0x1F40; i++) vwr(VRAM_SEG, i, (i & 1) ? (u8)(pattern >> 8) : (u8)pattern);
    for (u16 i = 0; i < 0x1F40; i++) vwr(VRAM_SEG, (u16)(0x2000 + i), (i & 1) ? (u8)(pattern >> 8) : (u8)pattern);
}

void gfx_clear_screen(u8 colour) { gfx_clear_screen_pat((u16)(colour * 0x0101u)); }

/* ------------------------------------------------------------------------------------------------ */
/* 0x4AF7 / 0x4B0E text                                                                             */

/* Opaque 8x8 font text. Each font byte is widened to 16 bits (DS:66E8, every bit doubled) and becomes
 * two target bytes = 8 CGA pixels at byte column x >> 2: pattern(fg) where the bit is set, pattern(bg)
 * elsewhere (DS:6344 maps colour index 0-3 to 0000/5555/AAAA/FFFF). Rows come from the current row
 * table; no clipping; nothing unless DS:68FE == 1. */
static void text_render(const u8 *s)
{
    u16 es = CUR_SEG;
    if (DSW(DS_TEXT_ENABLED) != 1) return;
    u16 fgp = DSW((u16)(DSW(DS_text_fg) * 2 + DS_TEXT_PATTERN));   /* bp-08 */
    u16 bgp = DSW((u16)(DSW(DS_text_bg) * 2 + DS_TEXT_PATTERN));   /* bp-06 */
    for (;;) {
        u8 ch = *s;
        if (ch == 0) return;
        s++;
        u16 glyph = DSW((u16)(ch * 2 + DSW(DS_text_font)));   /* near ptr in DGROUP */
        if (glyph == 0) {
            if (ch == 0x0D || ch == 0x0A) {
                DSW(DS_text_x) = DSW(DS_text_margin_x);
                DSW(DS_text_y) = (u16)(DSW(DS_text_y) + DSW(DS_text_adv_y));
            }
            continue;
        }
        u16 col = (u16)(DSW(DS_text_x) >> 2);                  /* shr: unsigned */
        u16 cx = DSW(DS_TEXT_GLYPH_H);
        u16 rp = (u16)(DSW(DS_text_y) * 2 + CUR_ROWTAB);
        do {                                                   /* loop */
            u16 ax = DSW((u16)(rd8(DGROUP, glyph) * 2 + DS_FONT_EXPAND));
            u16 dx = (u16)(~ax & bgp);
            ax = (u16)((ax & fgp) | dx);
            u16 bx = (u16)(CSW(rp) + col);
            vwr(es, bx, (u8)(ax >> 8));                        /* xchg ah,al ; mov es:[bx],ax */
            vwr(es, (u16)(bx + 1), (u8)ax);
            glyph++;
            rp = (u16)(rp + 2);
        } while (--cx);
        DSW(DS_text_x) = (u16)(DSW(DS_text_x) + DSW(DS_TEXT_ADV_X));
    }
}

void gfx_draw_text(const char *s, s16 x, s16 y)
{
    DSW(DS_text_x) = (u16)x;
    DSW(DS_text_y) = (u16)y;
    text_render((const u8 *)s);
}

/* 0x4B0E (unreferenced in TDCGA) */
void gfx_draw_text_at_cursor(const char *s) { text_render((const u8 *)s); }

/* 0x62DC (unreferenced in TDCGA) */
void gfx_set_text_cursor(s16 x, s16 y)
{
    DSW(DS_text_x) = (u16)x;
    DSW(DS_text_y) = (u16)y;
}

/* 0x6178: colour indices 0-3 */
void gfx_set_text_colours(u16 fg, u16 bg)
{
    DSW(DS_text_bg) = bg;
    DSW(DS_text_fg) = fg;
}

/* 0x7CC9 draw_text_centered: x = 0xA0 - strlen*4 (8-pixel characters) */
void draw_text_centered(const char *s, s16 y)
{
    u16 ax = (u16)(strlen(s) << 2);
    gfx_draw_text(s, (s16)(u16)(0xA0 - ax), y);
}

/* 0x7CF2 draw_rect_outline: four fill_rect calls with the caller's pattern word (TDCGA passes FFFFh) */
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
/* Mode set                                                                                          */

/* 0x4BDA (TDEGA's gfx_init_ega): CGA mode 4, palette 1, black background. */
void gfx_init_ega(void)
{
    /* PORT: 0x4BDA does PUSH BP without MOV BP,SP, so 0x4AD8 fills the screen with the caller's BP value
     * as pattern; the BIOS mode set below clears it again, so 0 is used. */
    gfx_clear_screen(0);
    bios_equipment_video(0x10);
    bios_set_mode(4);
    bios_set_color(1, 1);                 /* AH=0Bh BH=1 BL=1: palette 1 */
    bios_set_color(0, 0);                 /* AH=0Bh BH=0 BL=0: background black, intensity bit cleared */
}

/* 0x4E53 gfx_shutdown */
void gfx_shutdown(void)
{
    gfx_clear_screen(0);                  /* PORT: pattern is the caller's BP, see gfx_init_ega */
    bios_equipment_video(0x10);
    /* PORT: INT 10h mode 3 is modelled as mode state + buffer clear (the BIOS text fill is not visible:
     * text modes compose as black). */
    bios_set_mode(3);
    bios_set_color(0, 0x20);              /* AH=0Bh BX=0020h */
}

/* 0x7587 herc_init */
void gfx_herc_init(void)
{
    bios_equipment_video(0x20);
    bios_set_mode(4);                     /* no colour adapter in a Hercules system: only the buffer clear */
    vid.herc_config = 3;                  /* out 3BFh,3 */
    vid.herc_mode = 2;                    /* out 3B8h,2: graphics, video off */
    memcpy(vid.herc_crtc, mp(DGROUP, DS_HERC_CRTC), sizeof vid.herc_crtc);   /* out 3B4h/3B5h x12 */
    vram_clear();
    vid.herc_mode = 0x8A;                 /* out 3B8h,8Ah: graphics, video on, display page 1 */
    vid.dirty = true;
}

/* 0x75D7 herc_shutdown */
void gfx_herc_shutdown(void)
{
    bios_equipment_video(0x20);
    vid.herc_config = 3;                  /* out 3BFh,3 (page 1 stays mapped) */
    vid.herc_mode = 0x20;                 /* out 3B8h,20h: text, video off */
    memcpy(vid.herc_crtc, mp(DGROUP, DS_MDA_CRTC), sizeof vid.herc_crtc);
    vram_clear();
    vid.herc_mode = 0x28;                 /* out 3B8h,28h: text, video on, blink */
    bios_set_mode(7);
    vid.dirty = true;
}

/* ------------------------------------------------------------------------------------------------ */
/* Targets                                                                                          */

/* 0x4DF6 gfx_set_clip: descriptor clip (byte columns of 4 pixels / rows, x1 and y1 exclusive); also the
 * live copy when the descriptor's plane segment is the current one. */
void gfx_set_clip(FarPtr desc, s16 x0, s16 x1, s16 y0, s16 y1)
{
    bool cur = CUR_SEG == rd16(desc.seg, (u16)(desc.off + 2));
    wr16(desc.seg, (u16)(desc.off + 0x0C), (u16)x0);
    if (cur) CSW(GFX_CUR_CLIP_X0) = (u16)x0;
    wr16(desc.seg, (u16)(desc.off + 0x0E), (u16)x1);
    if (cur) CSW(GFX_CUR_CLIP_X1) = (u16)x1;
    wr16(desc.seg, (u16)(desc.off + 0x10), (u16)y0);
    if (cur) CSW(GFX_CUR_CLIP_Y0) = (u16)y0;
    wr16(desc.seg, (u16)(desc.off + 0x12), (u16)y1);
    if (cur) CSW(GFX_CUR_CLIP_Y1) = (u16)y1;
}

/* 0x4E7D gfx_select_target: CS:5734 = seg, CS:5736 = off, 12 words to CS:5738 */
void gfx_select_target(FarPtr desc)
{
    CSW(CUR_PTR_SEG) = desc.seg;
    CSW(CUR_PTR_OFF) = desc.off;
    for (u16 i = 0; i < 12; i++)
        CSW((u16)(GFX_CUR_OFF + 2 * i)) = rd16(desc.seg, (u16)(desc.off + 2 * i));
}

/* 0x82AB / 0x82BF: 24 words CS:5738.. <-> DS:6986 */
void gfx_target_save(void)
{
    for (u16 i = 0; i < 0x18; i++) DSW(DS_gfx_target_save_area + 2 * i) = CSW((u16)(GFX_CUR_OFF + 2 * i));
}

void gfx_target_restore(void)
{
    for (u16 i = 0; i < 0x18; i++) CSW((u16)(GFX_CUR_OFF + 2 * i)) = DSW(DS_gfx_target_save_area + 2 * i);
}

/* 0x4EA5 gfx_create_buffer(w_bytes, h): one 2bpp bitmap = sprite (16-byte header + w*h bytes).
 * TDCGA reads only two arguments: plane_mask is ignored (the caller still pushes it). Unlike TDEGA the
 * header planemap bytes, descriptor plane_seg[1..3] (+04..+08) and pad (+16) are not written. */
FarPtr gfx_create_buffer(u16 w_bytes, u16 h, u16 plane_mask)
{
    (void)plane_mask;
    u16 size = (u16)((s16)w_bytes * (s16)h);                   /* imul, low word */
    u16 paras = (u16)(((u16)(size + 0x10) >> 4) + 1);
    u16 seg = buf_alloc(paras);

    wr16(seg, 0, w_bytes);
    wr16(seg, 2, h);
    wr16(seg, 8, 0);
    wr16(seg, 0x0A, 0);
    wr16(seg, 4, 0);
    wr16(seg, 6, 0);

    u16 top = CSW(POOL_TOP);
    u16 next = (u16)(((u16)(h + 0x0C) << 1) + top);
    if (next >= POOL_LIMIT) fatal("%s", (const char *)mp(DGROUP, DS_STR_ROWTAB));   /* "OUT OF ROW TABLE SPACE" */
    CSW(POOL_TOP) = next;
    CSW(top) = 0;
    CSW((u16)(top + 2)) = seg;
    CSW((u16)(top + 0x0A)) = (u16)(top + 0x18);
    CSW((u16)(top + 0x0C)) = 0;
    CSW((u16)(top + 0x0E)) = w_bytes;
    CSW((u16)(top + 0x14)) = w_bytes;
    CSW((u16)(top + 0x10)) = 0;
    CSW((u16)(top + 0x12)) = h;
    u16 cnt = h, off = 0x10, b = top;
    do {                                                       /* loop */
        CSW((u16)(b + 0x18)) = off;
        b = (u16)(b + 2);
        off = (u16)(off + w_bytes);
    } while (--cnt);
    return far_make(CODE_SEG, top);
}

/* 0x6599 gfx_free_buffer: header read through (hdr_off, plane_seg[0]); pops the pool (LIFO).
 * The original returns with SI and DI exchanged (push ds,si,di / pop si,di,ds). No shipped caller uses
 * them afterwards (0x0FFA and 0x7DB6 only pop SI, the stage exit 0x1D60 pops both), so nothing to model. */
void gfx_free_buffer(FarPtr desc)
{
    u16 seg = rd16(desc.seg, (u16)(desc.off + 2));
    u16 hoff = rd16(desc.seg, desc.off);
    u16 h = rd16(seg, (u16)(hoff + 2));
    CSW(POOL_TOP) = (u16)(CSW(POOL_TOP) - (u16)((u16)(h + 0x0C) << 1));
    buf_free(seg);
}

/* 0x62ED gfx_clear_clip(pattern): fills the clip rectangle (byte columns x rows). Row parity is the
 * opposite of fill_rect: the low pattern byte goes to even rows, the high byte to odd rows. */
void gfx_clear_clip_pat(u16 pattern)
{
    u16 es = CUR_SEG;
    u16 rows = (u16)(CSW(GFX_CUR_CLIP_Y1) - CSW(GFX_CUR_CLIP_Y0));
    u16 bx = (u16)(CSW(GFX_CUR_CLIP_Y0) << 1);
    u16 ax = pattern;
    u16 width = (u16)(CSW(GFX_CUR_CLIP_X1) - CSW(GFX_CUR_CLIP_X0));
    u16 x0 = CSW(GFX_CUR_CLIP_X0);
    if (bx & 2) ax = swap16(ax);
    bx = (u16)(bx + CUR_ROWTAB);
    do {
        u16 di = (u16)(CSW(bx) + x0);
        for (u16 cx = width; cx; cx--) vwr(es, di++, (u8)ax);  /* rep stosb */
        ax = swap16(ax);
        bx = (u16)(bx + 2);
    } while (dec_jg(&rows));
}

void gfx_clear_clip(u8 colour) { gfx_clear_clip_pat((u16)(colour * 0x0101u)); }

/* ------------------------------------------------------------------------------------------------ */
/* 0x634C gfx_dissolve                                                                              */

/* Draws a sprite into the current target (any target, not only the screen) at byte column hdr.x (not
 * shifted) and row hdr.y in 12 interlaced passes; in each byte only one BIT changes (half a CGA pixel),
 * the mask rotating right by one per byte and starting at CS:6344[phase & 7], phase + 1 per row. */
void gfx_dissolve(FarPtr spr, u8 phase)
{
    u16 ds = spr.seg, so = spr.off;
    u16 es = CUR_SEG;
    u16 hx = rd16(ds, (u16)(so + 8));                          /* bp-14 */
    u16 w = rd16(ds, so);                                      /* bp-16 */
    u16 skip = (u16)((u8)w * 11);                              /* bp-0A (mul bl) */
    u16 rt = (u16)(rd16(ds, (u16)(so + 0x0A)) * 2 + CUR_ROWTAB);   /* bp-0E */
    u16 h = rd16(ds, (u16)(so + 2));
    u16 rend = (u16)(rt + h + h);                              /* bp-10 */
    u16 data = (u16)(so + 0x10);                               /* bp-12 */
    u16 bx = phase;                                            /* [bp+8] word argument */

    for (s16 pass = 11; pass >= 0; pass--) {                   /* dec [bp-18h] ; jge */
        u8 r0 = CSB((u16)(DIS_ROW_ORDER + pass));
        u16 si = (u16)(data + (u16)((u8)w * r0));              /* mul dl */
        u16 rp = (u16)(r0 * 2 + rt);                           /* bp-0C */
        while (rp < rend) {                                    /* cmp ; jae */
            u16 di = (u16)(CSW(rp) + hx);
            u16 cx = w;
            bx &= 7;
            u8 ah = CSB((u16)(DIS_BIT_MASK + bx));
            do {                                               /* loop */
                u8 al = (u8)(rd8(ds, si++) & ah);
                ah = (u8)~ah;
                al |= (u8)(vrd(es, di) & ah);
                vwr(es, di, al);                               /* mov es:[di],al ; stosb */
                di++;
                ah = ror8((u8)~ah);
            } while (--cx);
            bx++;
            rp = (u16)(rp + 0x18);
            si = (u16)(si + skip);
        }
    }
}

/* ------------------------------------------------------------------------------------------------ */
/* Sprite blitters                                                                                   */

/* Sprite: 16-byte header (w bytes, h, hot_x, hot_y, x, y, 4 bytes TDCGA never reads), then h rows of
 * w bytes, 2 bits per pixel. shift = px & 3 pixels = 2*shift bits. Every family has its own copy of
 * the row loops (one routine per shift); their quirks are selected below by op. */
enum { OP_COPY, OP_OR, OP_AND, OP_XOR };

static inline u8 apply(int op, u8 d, u8 v)
{
    switch (op) {
    case OP_OR:  return (u8)(d | v);
    case OP_AND: return (u8)(d & v);
    case OP_XOR: return (u8)(d ^ v);
    default:     return v;
    }
}

/* One shifted row: vis source bytes into vis+1 destination bytes (the last only if write_last). */
static void blit_row_shifted(int op, u16 es, u16 di, u16 ds, u16 *si, u16 vis, unsigned n,
                             bool clip, u8 edge)
{
    unsigned bits = 2 * n;
    u8 keep = (u8)~(0xFF >> bits);                             /* destination pixels left of the sprite */
    u8 dh;
    switch (op) {
    case OP_COPY:                                              /* clipped: from the clipped column */
        dh = (clip && (edge & 2)) ? (u8)(rd8(ds, (u16)(*si - 1)) << (8 - bits))
                                  : (u8)(vrd(es, di) & keep);
        break;
    case OP_AND: dh = keep; break;                             /* also when left-clipped */
    default:     dh = 0; break;                                /* OR/XOR: spill pixels dropped */
    }
    u8 al = 0;
    u16 cx = vis;
    do {                                                       /* loop */
        u8 s = rd8(ds, (*si)++);
        al = (u8)(s >> bits | dh);
        dh = (u8)(s << (8 - bits));
        vwr(es, di, apply(op, vrd(es, di), al));
        di++;
    } while (--cx);
    /* Final carry byte: unclipped always; clipped when edge bit 0 (right edge not clipped), but the
     * shift-3 routines test edge != 0, so a left-clipped sprite also gets it. */
    bool last = !clip || (n == 3 ? edge != 0 : (edge & 1) != 0);
    if (!last) return;
    u8 d = vrd(es, di);
    switch (op) {
    case OP_COPY: vwr(es, di, (u8)((d & (u8)~keep) | dh)); break;
    case OP_OR:   vwr(es, di, (u8)(d | dh)); break;
    case OP_AND:  vwr(es, di, (u8)(d & (dh | (u8)~keep))); break;
    default:
        /* 0x6B84: the clipped XOR shift-3 routine XORs AL (the last written byte) instead of DH. */
        vwr(es, di, (u8)(d ^ ((clip && n == 3) ? al : dh)));
        break;
    }
}

/* Clipped body: 0x4C69 (OR), 0x5A8D (COPY), 0x6A07 (XOR), 0x6BF1 (AND). */
static void blit_clip(FarPtr spr, u16 px, u16 py, int op)
{
    u16 ds = spr.seg, so = spr.off;
    u16 bx = py, cx = rd16(ds, (u16)(so + 2));
    u16 src = (u16)(so + 0x10);                                /* dx -> bp-12 */
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
        ax = (u16)(cx - nr);
        src = (u16)(src + (u16)((u8)ax * rd8(ds, so)));       /* mul byte ptr [si] */
        ax = (u16)(bx + nr);
        if (sgt(ax, y1)) {
            u16 over = (u16)(ax - y1);
            if (sle(nr, over)) return;
            nr = (u16)(nr - over);
        }
        cx = nr;
    }
    u16 rows = cx;                                             /* bp-10 */
    u16 row = bx;                                              /* bp-0A */
    u8 edge = 1;                                               /* bp-14 */

    cx = rd16(ds, so);
    u16 dx = 0;
    u16 shift = px & 3;                                        /* bp-16 */
    bx = (u16)((s16)px >> 2);
    u16 x0 = CSW(GFX_CUR_CLIP_X0), x1 = CSW(GFX_CUR_CLIP_X1);
    if (!slt(bx, x0)) {
        u16 ax = (u16)(bx + cx);
        if (!slt(ax, x1)) {                                    /* touching the right clip counts */
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
        dx = (u16)(cx - ax);                                   /* xchg dx,cx ; sub dx,ax ; add cx,ax */
        cx = ax;
        src = (u16)(src + dx);
        edge = 2;                                              /* mov, not or: bit 0 cleared */
    }
    u16 vis = cx;                                              /* bp-0C */
    u16 skip = dx;                                             /* bp-0E */
    u16 col = bx;                                              /* bp-08 */

    u16 es = CUR_SEG;
    u16 rp = (u16)(row * 2 + CUR_ROWTAB);
    u16 si = src;
    do {
        u16 di = (u16)(CSW(rp) + col);
        if (shift == 0) {
            u16 n = vis;
            if (op == OP_COPY) {
                for (; n; n--) vwr(es, di++, rd8(ds, si++));   /* rep movsb */
            } else {
                do { vwr(es, di, apply(op, vrd(es, di), rd8(ds, si++))); di++; } while (--n);
            }
        } else {
            blit_row_shifted(op, es, di, ds, &si, vis, shift, true, edge);
        }
        si = (u16)(si + skip);
        rp = (u16)(rp + 2);
    } while (dec_jg(&rows));
}

/* Unclipped body: 0x61E9 (COPY), 0x6631 (OR), 0x6777 (AND), 0x68C5 (XOR). */
static void blit_noclip(FarPtr spr, u16 px, u16 py, int op)
{
    u16 ds = spr.seg, so = spr.off;
    u16 rp = (u16)(py * 2 + CUR_ROWTAB);
    u16 w = rd16(ds, so);                                      /* bp-0C */
    u16 es = CUR_SEG;
    u16 col = (u16)((s16)px >> 2);
    u16 rows = rd16(ds, (u16)(so + 2));
    u16 si = (u16)(so + 0x10);
    do {
        u16 di = (u16)(CSW(rp) + col);
        if ((px & 3) == 0) {
            u16 n = w;
            if (op == OP_COPY) {
                for (; n; n--) vwr(es, di++, rd8(ds, si++));
            } else {
                do { vwr(es, di, apply(op, vrd(es, di), rd8(ds, si++))); di++; } while (--n);
            }
        } else {
            blit_row_shifted(op, es, di, ds, &si, w, px & 3, false, 1);
        }
        rp = (u16)(rp + 2);
    } while (dec_jg(&rows));
}

static inline u16 hdr_hot_x(FarPtr s) { return rd16(s.seg, (u16)(s.off + 4)); }
static inline u16 hdr_hot_y(FarPtr s) { return rd16(s.seg, (u16)(s.off + 6)); }
static inline u16 hdr_x(FarPtr s) { return rd16(s.seg, (u16)(s.off + 8)); }
static inline u16 hdr_y(FarPtr s) { return rd16(s.seg, (u16)(s.off + 0x0A)); }

/* Clipped "own" entries use the header x as is; unclipped ones mask it with FFFCh (byte-aligned). */
#define BLIT_FAMILY_CLIP(name_hot, name_raw, name_own, op)                                             \
    void name_hot(FarPtr spr, s16 x, s16 y)                                                            \
    { blit_clip(spr, (u16)((u16)x - hdr_hot_x(spr)), (u16)((u16)y - hdr_hot_y(spr)), op); }           \
    void name_raw(FarPtr spr, s16 x, s16 y) { blit_clip(spr, (u16)x, (u16)y, op); }                    \
    void name_own(FarPtr spr) { blit_clip(spr, hdr_x(spr), hdr_y(spr), op); }

#define BLIT_FAMILY(name_hot, name_raw, name_own, op)                                                  \
    void name_hot(FarPtr spr, s16 x, s16 y)                                                            \
    { blit_noclip(spr, (u16)((u16)x - hdr_hot_x(spr)), (u16)((u16)y - hdr_hot_y(spr)), op); }         \
    void name_raw(FarPtr spr, s16 x, s16 y) { blit_noclip(spr, (u16)x, (u16)y, op); }                  \
    void name_own(FarPtr spr) { blit_noclip(spr, (u16)(hdr_x(spr) & 0xFFFC), hdr_y(spr), op); }

/* 0x5A30/0x5A54/0x5A72 */ BLIT_FAMILY_CLIP(blit_copy_clip_hot, blit_copy_clip_raw, blit_copy_clip_own, OP_COPY)
/* 0x4C0C/0x4C30/0x4C4E */ BLIT_FAMILY_CLIP(blit_or_clip_hot, blit_or_clip_raw, blit_or_clip_own, OP_OR)
/* 0x6B94/0x6BB8/0x6BD6 */ BLIT_FAMILY_CLIP(blit_and_clip_hot, blit_and_clip_raw, blit_and_clip_own, OP_AND)
/* 0x69AA/0x69CE/0x69EC */ BLIT_FAMILY_CLIP(blit_xor_clip_hot, blit_xor_clip_raw, blit_xor_clip_own, OP_XOR)
/* 0x6189/0x61AD/0x61CB */ BLIT_FAMILY(blit_copy_hot, blit_copy_raw, blit_copy_own, OP_COPY)
/* 0x65D1/0x65F5/0x6613 */ BLIT_FAMILY(blit_or_hot, blit_or_raw, blit_or_own, OP_OR)
/* 0x6717/0x673B/0x6759 */ BLIT_FAMILY(blit_and_hot, blit_and_raw, blit_and_own, OP_AND)
/* 0x6865/0x6889/0x68A7 */ BLIT_FAMILY(blit_xor_hot, blit_xor_raw, blit_xor_own, OP_XOR)

/* 0x8006 draw_glyph: XOR hotspot blit of the cursor sprite CS:[CS:7FA6 + idx*2] */
void draw_glyph(s16 x, s16 y, s16 idx)
{
    u16 off = CSW((u16)(GLYPH_SPRITES + (u16)idx * 2));
    blit_xor_hot(far_make(CODE_SEG, off), x, y);
}

/* ------------------------------------------------------------------------------------------------ */
/* 0x6D97 gfx_draw_line                                                                             */

static inline void add32(u16 *frac, u16 *ip, u16 sfrac, u16 sint)   /* add frac ; adc int */
{
    u32 f = (u32)*frac + sfrac;
    *frac = (u16)f;
    *ip = (u16)(*ip + sint + (f >> 16));
}

/* Diagonal plot: OR the pixel mask into the current target (colour ignored -> colour 3). */
static void line_plot(u16 es, u16 y, u16 x)
{
    u16 di = (u16)(row_at(y) + (x >> 2));
    vwr(es, di, (u8)(vrd(es, di) | CSB((u16)(LINE_PIXEL_MASK + (x & 3)))));
}

/* Pixel coordinates; clip x = clip byte columns * 4. Horizontal/vertical lines OR the colour byte
 * (a 2bpp pattern, TDCGA passes FFh) masked to the pixels; see graphics.md for the quirks kept here. */
void gfx_draw_line(s16 x0, s16 y0, s16 x1, s16 y1, u8 colour)
{
    u16 es = CUR_SEG;
    u16 cx0 = (u16)(CSW(GFX_CUR_CLIP_X0) << 2), cx1 = (u16)(CSW(GFX_CUR_CLIP_X1) << 2);   /* bp-14, bp-16 */
    u16 cy0 = CSW(GFX_CUR_CLIP_Y0), cy1 = CSW(GFX_CUR_CLIP_Y1);
    u16 X0 = (u16)x0, Y0 = (u16)y0, X1 = (u16)x1, Y1 = (u16)y1;

    if (Y0 == Y1) {                                            /* 0x6DCA */
        if (slt(Y0, cy0) || !slt(Y0, cy1)) return;
        u16 di = row_at(Y0);
        u16 cx = (u16)(X1 - X0), bx;
        if (cx & 0x8000) { cx = (u16)-cx; bx = X1; } else bx = X0;   /* js */
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
        di = (u16)(di + (bx >> 2));
        u16 b = bx & 3;
        /* first byte: pixels from x & 3 on (CS:6D87) and, if the line ends there, up to its end
         * (CS:6D8F[len + (x & 3)], reached as cx - first + 4) */
        u8 al = (u8)(CSB((u16)(LINE_LEFT_MASK + b)) & colour);
        u8 first = CSB((u16)(LINE_FIRST_LEN + b));
        s32 rem = (s32)(s16)cx - first;                        /* sub cl ; sbb ch,0 ; jl */
        cx = (u16)(cx - first);
        if (rem < 0) {
            cx = (u16)(cx + 4);
            b = cx;                                            /* 0x6E60 */
        } else {
            vwr(es, di, (u8)(vrd(es, di) | al));
            di++;
            al = colour;
            b = cx & 3;
            for (cx >>= 2; cx; cx--) { vwr(es, di, (u8)(vrd(es, di) | al)); di++; }
        }
        al &= CSB((u16)(LINE_RIGHT_MASK + b));                 /* 0x6E55 */
        vwr(es, di, (u8)(vrd(es, di) | al));
        return;
    }

    if (X0 == X1) {                                            /* 0x6E6F */
        if (sle(X0, cx0) || !slt(X0, cx1)) return;             /* jle: x == clip left is rejected */
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
        u16 rp = (u16)(si * 2 + CUR_ROWTAB);
        u16 dxb = (u16)(X0 >> 2);
        u8 al = (u8)(CSB((u16)(LINE_PIXEL_MASK + (X0 & 3))) & colour);
        do {                                                   /* loop */
            u16 d = (u16)(CSW(rp) + dxb);
            vwr(es, d, (u8)(vrd(es, d) | al));
            rp = (u16)(rp + 2);
        } while (--cx);
        return;
    }

    /* Diagonal (0x6EE6). xr = DX register (x in the x-major walk), yv = [bp-0Ch], xv = [bp-08h]. */
    u8 fl_x = 0, fl_y = 0;                                     /* AL, AH */
    u16 fx = 0, fy = 0;                                        /* bp-0A, bp-0E */
    u16 xr, yv, xv, si, sfrac, sint;
    u16 bx = (u16)(X1 - X0);
    if (bx & 0x8000) { bx = (u16)-bx; fl_x = 1; }
    bx++;
    u16 cx = (u16)(Y1 - Y0);
    if (cx & 0x8000) { fl_y = 1; cx = (u16)-cx; }
    cx++;

    if (!slt(bx, cx)) {                                        /* x-major */
        if (fl_x) { xv = X1; yv = Y1; fl_y ^= 1; } else { xv = X0; yv = Y0; }
        if (bx == cx) {
            sfrac = 0;
            sint = fl_y ? 0xFFFF : 1;
        } else {
            u16 q = div32_16((u32)cx << 16, bx, NULL);
            cx = bx;
            sfrac = q;
            sint = 0;
            if (fl_y) { sint = 0xFFFF; sfrac = (u16)-sfrac; }
        }
        xr = xv;
        goto x_phase1;
    }

    /* y-major */
    if (fl_y) { xv = X1; yv = Y1; fl_x ^= 1; } else { xv = X0; yv = Y0; }
    sfrac = div32_16((u32)bx << 16, cx, NULL);
    sint = 0;
    if (fl_x) { sint = 0xFFFF; sfrac = (u16)-sfrac; }
    xr = sint;                                                 /* DX after the division code */
    si = yv;
    for (;;) {                                                 /* 0x701C */
        if (slt(si, cy0)) goto x_skip;                         /* jl 0x6FC5 (into the x-major walk) */
        if (slt(si, cy1)) {
            xr = xv;
            if (!slt(xr, cx0)) {
                if (slt(xr, cx1)) break;
                return;
            }
        }
        si++;
        add32(&fx, &xv, 0xFFEE, 0xFFF0);                       /* assembler typo: constant, not the step */
        if (--cx == 0) return;
    }
    do {                                                       /* 0x704C */
        if (!slt(si, cy1)) return;
        xr = xv;
        if (slt(xr, cx0) || !slt(xr, cx1)) return;
        line_plot(es, si, xr);
        si++;
        add32(&fx, &xv, sfrac, sint);
    } while (--cx);
    return;

    /* x-major walk (0x6FA7). The y-major branch above jumps into its skip step with DX = the integer
     * x step (0 or FFFFh) as x and its x step as the y increment when the line starts above the clip. */
x_skip:
    xr++;
    add32(&fy, &yv, sfrac, sint);
    if (--cx == 0) return;
x_phase1:
    for (;;) {
        if (!slt(yv, cy0) && slt(yv, cy1) && !slt(xr, cx0)) {
            if (slt(xr, cx1)) break;
            return;
        }
        xr++;
        add32(&fy, &yv, sfrac, sint);
        if (--cx == 0) return;
    }
    do {                                                       /* 0x6FD7 */
        if (slt(yv, cy0) || !slt(yv, cy1) || !slt(xr, cx1)) return;
        line_plot(es, yv, xr);
        xr++;
        add32(&fy, &yv, sfrac, sint);
    } while (--cx);
}

/* ------------------------------------------------------------------------------------------------ */
/* Screen grabs                                                                                     */

/* 0x72B0 gfx_grab_screen: B800h through the screen descriptor's row table (CS:575A) -> current target.
 * x values are pixels (>> 2 to byte columns), w is in bytes. */
void gfx_grab_screen(s16 sx, s16 sy, s16 dx, s16 dy, s16 w_bytes, s16 h)
{
    u16 srp = (u16)(sy * 2 + CSW(SCREEN_ROWTAB));              /* bp-08 */
    u16 drp = (u16)(dy * 2 + CUR_ROWTAB);                      /* bp-0A */
    u16 es = CUR_SEG;
    u16 sxb = (u16)(sx >> 2), dxb = (u16)(dx >> 2);
    u16 rows = (u16)h;
    do {
        u16 si = (u16)(CSW(srp) + sxb);
        u16 di = (u16)(CSW(drp) + dxb);
        for (u16 c = (u16)w_bytes; c; c--) vwr(es, di++, vrd(VRAM_SEG, si++));   /* rep movsb */
        srp = (u16)(srp + 2);
        drp = (u16)(drp + 2);
    } while (dec_jg(&rows));
}

/* 0x7360 grab_into_sprite_raw: the CURRENT TARGET (not necessarily the screen) at (x, y) -> the sprite's
 * pixel rows. (0x733C hot and 0x737E own (x & FFFCh) variants exist but are unreferenced.) */
void grab_into_sprite_raw(FarPtr spr, s16 x, s16 y)
{
    u16 ds = spr.seg;
    u16 w = rd16(ds, spr.off), h = rd16(ds, (u16)(spr.off + 2));   /* bp-0E, bp-10 */
    u16 rp = (u16)(y * 2 + CUR_ROWTAB);                        /* bp-0C */
    u16 src = CUR_SEG;
    u16 di = (u16)(spr.off + 0x10);
    u16 col = (u16)(x >> 2);
    u16 rows = h;
    do {
        u16 si = (u16)(CSW(rp) + col);
        for (u16 c = w; c; c--) vwr(ds, di++, vrd(src, si++));
        rp = (u16)(rp + 2);
    } while (dec_jg(&rows));
}
