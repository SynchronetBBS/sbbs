#pragma once
/* Real-mode memory model.
 *
 * The unpacked TDEGA.EXE (TDCGA.EXE in CGA builds) load image is placed at segment LOAD_SEG (0x1000,
 * same as the Ghidra project), with its MZ relocations applied, so:
 *   image offset X      = linear 0x10000 + X   (code; the asm keeps state at CS:xxxx)
 *   DS:xxxx (DGROUP)    = segment 0x1C9A, i.e. image offset 0xC9A0 + xxxx   (TDCGA: 0x1A8A, 0xA8A0)
 * All data tables, strings, the font, palettes and road data are read from there. Heap segments
 * (archives, off-screen buffers) are allocated above DGROUP. Video memory (EGA segment 0xA000, CGA
 * 0xB800) is NOT in mem[]: graphics code treats that segment as the screen, exactly like the original.
 */
#include "types.h"

#define MEM_SIZE    0x110000u
#define LOAD_SEG    0x1000
#define CODE_SEG    LOAD_SEG
#define DGROUP      (LOAD_SEG + EGA_CGA(0x0C9A, 0x0A8A))
#define VRAM_SEG    EGA_CGA(0xA000, 0xB800)
#define HEAP_BOTTOM 0x2D00   /* first segment above the 64 KB DGROUP (stack/BSS included) */
#define HEAP_TOP    0xA000   /* DOS memory ends where video memory begins */

extern u8 mem[MEM_SIZE];

static inline u32 lin(u16 seg, u16 off) { return ((u32)seg << 4) + off; }
static inline u8 *mp(u16 seg, u16 off) { return mem + lin(seg, off); }

static inline u8  rd8 (u16 seg, u16 off) { return mem[lin(seg, off)]; }
static inline u16 rd16(u16 seg, u16 off) { u8 *p = mp(seg, off); return (u16)(p[0] | p[1] << 8); }
static inline u32 rd32(u16 seg, u16 off) { u8 *p = mp(seg, off); return p[0] | p[1] << 8 | p[2] << 16 | (u32)p[3] << 24; }
static inline void wr8 (u16 seg, u16 off, u8 v)  { mem[lin(seg, off)] = v; }
static inline void wr16(u16 seg, u16 off, u16 v) { u8 *p = mp(seg, off); p[0] = (u8)v; p[1] = (u8)(v >> 8); }
static inline void wr32(u16 seg, u16 off, u32 v) { u8 *p = mp(seg, off); p[0] = (u8)v; p[1] = (u8)(v >> 8); p[2] = (u8)(v >> 16); p[3] = (u8)(v >> 24); }

/* Lvalue accessors for globals (little-endian host). DS offsets come from symbols.h, e.g.
 *   DSW(DS_road_pos) += 1;   if (DSB(DS_run_state) == 3) ...   DSS(DS_car_x) = -0x236;      */
#define DSB(o) (*(u8  *)mp(DGROUP, (u16)(o)))
#define DSC(o) (*(s8  *)mp(DGROUP, (u16)(o)))
#define DSW(o) (*(u16 *)mp(DGROUP, (u16)(o)))
#define DSS(o) (*(s16 *)mp(DGROUP, (u16)(o)))
#define DSL(o) (*(u32 *)mp(DGROUP, (u16)(o)))
#define CSB(o) (*(u8  *)mp(CODE_SEG, (u16)(o)))
#define CSW(o) (*(u16 *)mp(CODE_SEG, (u16)(o)))
#define CSS(o) (*(s16 *)mp(CODE_SEG, (u16)(o)))

/* 16:16 far pointer as stored in memory (offset first). */
typedef struct { u16 off, seg; } FarPtr;

static inline FarPtr far_rd(u16 seg, u16 off) { FarPtr p = { rd16(seg, off), rd16(seg, (u16)(off + 2)) }; return p; }
static inline void   far_wr(u16 seg, u16 off, FarPtr p) { wr16(seg, off, p.off); wr16(seg, (u16)(off + 2), p.seg); }
static inline u8    *far_mp(FarPtr p) { return mp(p.seg, p.off); }
static inline FarPtr far_make(u16 seg, u16 off) { FarPtr p = { off, seg }; return p; }
static inline u32    far_lin(FarPtr p) { return lin(p.seg, p.off); }
static inline FarPtr far_norm(u32 linear) { FarPtr p = { (u16)(linear & 0x0F), (u16)(linear >> 4) }; return p; }
static inline bool   far_is_null(FarPtr p) { return p.off == 0 && p.seg == 0; }

/* Division as seen by game code with the INT 0 hook installed (DGROUP:1423): a divide error
 * (zero divisor or quotient overflow) leaves AX = 0xFFFF and skips the instruction, so DX
 * (the remainder register) keeps its previous value. Callers pass the prior DX in *rem. */
static inline u16 div32_16(u32 dxax, u16 divisor, u16 *rem)
{
    if (divisor == 0 || dxax / divisor > 0xFFFF) return 0xFFFF;
    if (rem) *rem = (u16)(dxax % divisor);
    return (u16)(dxax / divisor);
}
static inline u16 idiv32_16(s32 dxax, s16 divisor, u16 *rem)
{
    if (divisor == 0) return 0xFFFF;
    s32 q = dxax / divisor;                         /* truncates toward zero, like IDIV */
    if (q > 32767 || q < -32768) return 0xFFFF;
    if (rem) *rem = (u16)(s16)(dxax % divisor);
    return (u16)(s16)q;
}
static inline u16 div16_8(u16 ax, u8 divisor)       /* returns AX: AL = quotient, AH = remainder */
{
    if (divisor == 0 || ax / divisor > 0xFF) return 0xFFFF;
    return (u16)((ax % divisor) << 8 | (ax / divisor));
}
static inline u16 idiv16_8(s16 ax, s8 divisor)
{
    if (divisor == 0) return 0xFFFF;
    int q = ax / divisor;
    if (q > 127 || q < -128) return 0xFFFF;
    return (u16)((u8)(s8)(ax % divisor) << 8 | (u8)(s8)q);
}

/* Name of the original executable this build ports. */
#define TD_EXE_NAME EGA_CGA("TDEGA.EXE", "TDCGA.EXE")

/* Loads TD_EXE_NAME (EXEPACK-packed or already unpacked), relocates it to LOAD_SEG and checks that it
 * is the expected build. Returns false and fills err on failure. */
bool mem_load_exe(const char *path, char *err, size_t errlen);

/* Size of the load image in bytes (code + initialised data). */
extern u32 mem_image_size;
