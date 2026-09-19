/* Memory blocks, archives and misc runtime — port of TDEGA 0x6762, 0x78A5, 0x8A5B, 0x8BAF, 0x8CC0, 0x8D8C,
 * 0x94AB, 0x94C7, 0x9A86..0xA111, 0xA280..0xA37E and MSC rand/srand 0xA8FD/0xA90E
 * (port/spec/platform.md §4.7, §2.4; FORMATS.md).
 * TDCGA has the same routines at other addresses (TDCGA 0x5C55, 0x643F, 0x70FC, 0x731F, 0x755B, 0x7627,
 * 0x7C6D, 0x7C89, 0x8174..0x8272, 0x87F1/0x8802); its load_archive reads RLE .CMP files itself and there
 * is no PES unpacker (0x9A86..0xA111). */
#include "res.h"

#include <stdarg.h>
#include <stdio.h>

#include "../host.h"
#include "../symbols.h"
#include "timer.h"

/* host_game_path() returns SDL-allocated memory; declared here so this file does not include SDL. */

#if !TD_CGA
/* ---- DGROUP state of the unpacker without a symbol of its own */
#define DS_lzw_offset      0x69A2   /* bit offset in the code buffer */
#define DS_lzw_size        0x69A4   /* bits available in the code buffer */
#define DS_lzw_rmask       0x6998   /* u8[9] 00 01 03 07 0F 1F 3F 7F FF */
#define DS_huff_bpos       0x746E
#define DS_huff_numnodes   0x7472
#define DS_lzw_n_bits      0x7474
#define DS_huff_cur        0x7476
#define DS_lzw_maxcode     0x747A
#define DS_lzw_clear_flg   0x747C
#define DS_lzw_buf         0x747E   /* u8[12] */
#define DS_lzw_free_ent    0x748A
#define DS_huff_nodes      0x748E   /* s16[2] x 257 */
#define DS_rle_last        0x7892
#define DS_pack_outptr_seg 0x809E
#endif
#define DS_crt_seed        EGA_CGA(0x707A, 0x6F3C)   /* u32 MSC rand seed (srand writes +0, zeroes +2) */

#define CACHE_SLOT_SIZE 16
#define CACHE_SLOTS     30
#define BUF_SLOTS       8

#if !TD_CGA
/* ---- host-side scratch (the original keeps these in the CRT near heap / a DOS handle) */
static FILE *pack_file;
static u8 pack_inbuf[0x400];             /* malloc(0x400), DS:7F3E; the cursor equals DS:8096 */
static u8 lzw_tab[0x438B];               /* malloc(0x438B), DS:6A3C: prefix +0, suffix +0x2000, stack +0x3000 */
#define LZW_SUFFIX 0x2000
#define LZW_STACK  0x3000
static inline u16 lzw_prefix_rd(u16 code) { return (u16)(lzw_tab[code * 2] | lzw_tab[code * 2 + 1] << 8); }
static inline void lzw_prefix_wr(u16 code, u16 v) { lzw_tab[code * 2] = (u8)v; lzw_tab[code * 2 + 1] = (u8)(v >> 8); }
#endif

static const char *ds_str(u16 off) { return (const char *)mp(DGROUP, off); }

/* 0x94AB fatal */
_Noreturn void fatal(const char *fmt, ...)
{
    char msg[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg, sizeof msg, fmt, ap);
    va_end(ap);
    /* PORT: gfx_shutdown (0x50FE, text mode) is left to host_fatal's teardown; abort's R6010 banner dropped. */
    timer_restore();
    host_fatal("%s", msg);
}

/* ---------------------------------------------------------------- memory */

/* 0x8CC0 mem_init */
void mem_init(void)
{
    /* PORT: INT 21h 48h/4Ah/4Ah replaced by the fixed heap range of mem.h. */
    u16 seg = HEAP_BOTTOM;
    DSW(DS_mem_low) = seg;
    DSW(DS_mem_bot) = seg;
    u16 max = (u16)(HEAP_TOP - HEAP_BOTTOM);       /* BX returned by the failing AH=4Ah */
    DSW(DS_mem_high) = (u16)(DSW(DS_mem_bot) + max);
    DSW(DS_mem_top) = DSW(DS_mem_high);
}

/* 0xA280 cache_find: segment of a cached archive (exact 12-char name), 0 if absent */
u16 cache_find(const char *fname)
{
    u16 di = DS_cache_slots;
    for (int dx = CACHE_SLOTS; dx > 0; dx--, di += CACHE_SLOT_SIZE) {
        u16 seg = DSW((u16)(di + 0x0E));
        if (seg == 0) return 0;                         /* first empty slot ends the list */
        for (u16 bx = 0; bx < 12; bx++) {
            u8 c = (u8)fname[bx];
            if (c == 0) {
                if (DSB((u16)(di + bx)) == 0) return seg;
                goto next;
            }
            if (DSB((u16)(di + bx)) != c) goto next;
        }
        return seg;
    next:;
    }
    return 0;
}

/* 0xA2BE cache_alloc: low-end allocation with LIFO eviction */
u16 cache_alloc(const char *fname, u16 paras, u16 reserve)
{
    u16 need = (u16)(paras + reserve);
    u16 di = DS_cache_slots;
    u16 dx = DSW(DS_mem_low);
    int cx = CACHE_SLOTS;
    for (;;) {
        u16 end = (u16)(DSW((u16)(di + 0x0E)) + DSW((u16)(di + 0x0C)));
        if (end == 0) goto have_slot;
        dx = end;
        di = (u16)(di + CACHE_SLOT_SIZE);
        if (--cx == 0) break;
    }
    di = (u16)(di - CACHE_SLOT_SIZE);                   /* all slots used: reuse the last one */
    dx = DSW((u16)(di + 0x0E));
    DSW(DS_mem_low) = dx;
have_slot:
    while ((u16)(DSW(DS_mem_high) - dx) < need) {       /* evict newest entries */
        DSW((u16)(di + 0x0E)) = 0;
        DSW((u16)(di + 0x0C)) = 0;
        di = (u16)(di - CACHE_SLOT_SIZE);
        if (di < DS_cache_slots) fatal(ds_str(EGA_CGA(0x6A5C, 0x691E)), fname);     /* "OUT OF MEMORY LOADING %s\n" */
        dx = DSW((u16)(di + 0x0E));
    }
    for (u16 bx = 0; bx < 12; bx++) {
        u8 c = (u8)fname[bx];
        DSB((u16)(di + bx)) = c;
        if (c == 0) break;
    }
    DSW((u16)(di + 0x0C)) = paras;
    DSW((u16)(di + 0x0E)) = dx;
    DSW(DS_mem_low) = (u16)(paras + dx);
    return dx;
}

/* 0xA340 buf_alloc: high-end buffer, 8 slots */
u16 buf_alloc(u16 paras)
{
    u16 di = DS_buf_slots;
    int cx;
    for (cx = BUF_SLOTS; cx > 0; cx--, di += 4)
        if (DSW(di) == 0) break;
    if (cx == 0) fatal("%s", ds_str(EGA_CGA(0x6A78, 0x693A)));           /* "OUT OF MEMORY BUFFERS" */
    u16 seg = (u16)(DSW(DS_mem_high) - paras);
    if (seg <= DSW(DS_mem_low)) fatal("%s", ds_str(EGA_CGA(0x6A8E, 0x6950)));   /* "OUT OF BUFFER MEMORY" */
    DSW(DS_mem_high) = seg;
    DSW(di) = seg;
    DSW((u16)(di + 2)) = paras;
    return seg;
}

/* 0xA37E buf_free */
void buf_free(u16 seg)
{
    u16 di = DS_buf_slots;
    int cx;
    for (cx = BUF_SLOTS; cx > 0; cx--, di += 4)
        if (DSW(di) == seg) break;
    if (cx == 0) fatal("%s", ds_str(EGA_CGA(0x6AA4, 0x6966)));           /* "BUFFER NOT FOUND RELEASE ERROR" */
    u16 end = (u16)(DSW((u16)(di + 2)) + seg);
    if (end >= DSW(DS_mem_high)) DSW(DS_mem_high) = end;
    DSW(di) = 0;
    DSW((u16)(di + 2)) = 0;
}

/* ---------------------------------------------------------------- archives */

/* Offset-table relocation shared by 0x78A5 and 0x8A5B: u32 offsets -> normalized far pointers. */
static void archive_relocate(u16 seg, u16 off)
{
    u16 si = (u16)(off + 4);
    u16 count = rd16(seg, si);
    si = (u16)(si + 2);
    u16 ax = (u16)(count << 2);
    si = (u16)(si + ax);                                /* start of the offset table */
    u16 base = (u16)(si + ax);                          /* end of table = offset base */
    u16 n = count;
    do {                                                /* dec/jg: count 0 still runs once */
        u32 l = rd32(seg, si) + base + ((u32)seg << 4);
        wr16(seg, si, (u16)(l & 0x0F));
        wr16(seg, (u16)(si + 2), (u16)(l >> 4));
        si = (u16)(si + 4);
    } while ((s16)--n > 0);
}

static FILE *open_game_file(const char *fname)
{
    char *path = host_game_path(fname, false);
    if (!path) return NULL;
    FILE *f = fopen(path, "rb");
    host_free(path);
    return f;
}

/* Reads n bytes into mem[] at seg:0 (clamped to mem[]); returns false on a read error. */
static bool read_into_mem(FILE *f, u16 seg, u16 n)
{
    u32 l = lin(seg, 0);
    u32 room = l < MEM_SIZE ? MEM_SIZE - l : 0;
    size_t want = n < room ? n : room;
    size_t got = fread(mem + l, 1, want, f);
    return !(got < want && ferror(f));
}

#if TD_CGA
/* TDCGA 0x7285: refill the 256-byte stack buffer; DX = bytes read, SI = 0 */
static void cmp_refill(FILE *f, u8 *buf, u16 *si, u16 *dx, bool *err)
{
    size_t n = fread(buf, 1, 0x100, f);
    if (n == 0 && ferror(f)) *err = true;
    *dx = (u16)n;
    *si = 0;
}

/* RLE body shared by TDCGA 0x70FC and 0x73EA: 83 vv nn = nn copies of vv, anything else is literal.
 * Output starts at es:di; false on a read error. */
static bool cmp_unpack(FILE *f, u16 es, u16 di)
{
    u8 buf[0x100] = { 0 };
    bool rerr = false;
    u16 si = 0, dx = 0;
    for (;;) {
        if ((s16)si >= (s16)dx) {
            cmp_refill(f, buf, &si, &dx, &rerr);
            if (rerr) return false;
            if (dx == 0) return true;                   /* end of file */
        }
        u8 al = buf[si++];
        if (al != 0x83) {
            u32 l = lin(es, di);
            if (l < MEM_SIZE) mem[l] = al;
            if (++di == 0) es = (u16)(es + 0x1000);     /* stosb wrapped: next 64 KB */
            continue;
        }
        if ((s16)si >= (s16)dx) { cmp_refill(f, buf, &si, &dx, &rerr); if (rerr) return false; }
        al = buf[si++ & 0xFF];                          /* a refill at EOF reads a stale byte, as the original */
        if ((s16)si >= (s16)dx) { cmp_refill(f, buf, &si, &dx, &rerr); if (rerr) return false; }
        u8 cl = buf[si++ & 0xFF];
        if (di >= 0xFDE8) {                             /* keep the run inside this segment */
            di = (u16)(di - 0x4000);
            es = (u16)(es + 0x400);
        }
        for (u16 cx = cl; cx; cx--, di++) {             /* rep stosb */
            u32 l = lin(es, di);
            if (l < MEM_SIZE) mem[l] = al;
        }
    }
}

/* Reads the 4-byte unpacked size of a .CMP file (open, read, close); false on error or an empty read. */
static bool cmp_read_size(const char *fname, u32 *size)
{
    u8 hdr[4] = { 0 };
    FILE *f = open_game_file(fname);
    if (!f) return false;
    size_t n = fread(hdr, 1, 4, f);
    fclose(f);
    if (n == 0) return false;                           /* CF or AX == 0 */
    *size = (u32)hdr[0] | (u32)hdr[1] << 8 | (u32)hdr[2] << 16 | (u32)hdr[3] << 24;
    return true;
}

/* TDCGA 0x70FC load_archive: .CMP file = u32 unpacked size (also the archive's first field) followed by an
 * RLE stream (FORMATS.md). */
FarPtr load_archive(const char *fname, u16 reserve)
{
    u16 seg = cache_find(fname);
    if (seg) return far_make(seg, 0);                   /* already relocated */

    u32 size;
    if (!cmp_read_size(fname, &size)) goto err;
    seg = cache_alloc(fname, (u16)((u16)(size >> 4) + 1), reserve);

    FILE *f = open_game_file(fname);
    if (!f) goto err;
    if (fread(mp(seg, 0), 1, 4, f) < 4 && ferror(f)) { fclose(f); goto err; }   /* the size again, into seg:0000 */
    if (!cmp_unpack(f, seg, 4)) { fclose(f); goto err; }
    fclose(f);
    archive_relocate(seg, 0);
    return far_make(seg, 0);
err:
    fatal(ds_str(0x64DE), fname);                       /* "%s FILE ERROR" */
}

/* TDCGA 0x73EA: like load_archive, but unpacks into DGROUP at ds_buf (no cache slot). *len receives the
 * low word of the unpacked size. Returns DGROUP:ds_buf. */
FarPtr load_packed_near(const char *fname, u16 ds_buf, u16 *len)
{
    u32 size;
    if (!cmp_read_size(fname, &size)) goto err;
    FILE *f = open_game_file(fname);
    if (!f) goto err;
    if (fread(mp(DGROUP, ds_buf), 1, 4, f) < 4 && ferror(f)) { fclose(f); goto err; }
    if (!cmp_unpack(f, DGROUP, (u16)(ds_buf + 4))) { fclose(f); goto err; }
    fclose(f);
    archive_relocate(DGROUP, ds_buf);
    *len = (u16)size;
    return far_make(DGROUP, ds_buf);
err:
    fatal(ds_str(0x660E), fname);                       /* "%s FILE ERROR" */
}
#else
/* 0x8A5B load_archive */
FarPtr load_archive(const char *fname, u16 reserve)
{
    u16 seg = cache_find(fname);
    if (seg) return far_make(seg, 0);                   /* already relocated */
    FarPtr a = load_packed_archive(fname, reserve);
    archive_relocate(a.seg, a.off);
    return a;
}
#endif

/* 0x78A5 load_raw_archive (tdsnd.snd) */
FarPtr load_raw_archive(const char *fname, u16 reserve)
{
    u16 seg = cache_find(fname);
    if (seg) return far_make(seg, 0);

    u8 hdr[4] = { 0 };
    FILE *f = open_game_file(fname);
    if (!f) goto err;
    size_t n = fread(hdr, 1, 4, f);
    fclose(f);
    if (n == 0) goto err;                               /* CF or AX == 0 */

    u32 size = (u32)hdr[0] | (u32)hdr[1] << 8 | (u32)hdr[2] << 16 | (u32)hdr[3] << 24;
    u16 paras = (u16)((u16)(size >> 4) + 1);
    seg = cache_alloc(fname, paras, reserve);

    f = open_game_file(fname);
    if (!f) goto err;
    u16 chunks = (u16)(size >> 14);                     /* high word of size << 2 */
    u16 rest = (u16)size;
    u16 ds = seg;
    while ((s16)--chunks >= 0) {
        if (!read_into_mem(f, ds, 0x4000)) { fclose(f); goto err; }
        rest = (u16)(rest - 0x4000);
        ds = (u16)(ds + 0x400);
    }
    if (!read_into_mem(f, ds, rest)) { fclose(f); goto err; }
    fclose(f);
    archive_relocate(seg, 0);
    return far_make(seg, 0);

err:
    fatal(ds_str(EGA_CGA(0x6510, 0x64CE)), fname);                       /* "%s FILE ERROR\n" (literal backslash-n) */
}

#if !TD_CGA
/* 0x9D64 pack_getc */
static u16 pack_getc(void)
{
    DSL(DS_in_count)++;
    if (DSL(DS_packed_left) == 0) return 0xFFFF;
    DSL(DS_packed_left)--;
    if ((s16)DSW(DS_pack_inpos) >= 0x400) {
        if (pack_file) (void)fread(pack_inbuf, 1, 0x400, pack_file);   /* short read leaves stale bytes */
        DSW(DS_pack_inpos) = 0;
    }
    u8 c = pack_inbuf[DSW(DS_pack_inpos) & 0x3FF];
    DSW(DS_pack_inpos)++;
    return c;
}

/* 0x9DD3 pack_putc: huge-pointer output, bytes past unpacked_len dropped */
static void pack_putc(u8 c)
{
    DSL(DS_out_count)++;
    if ((s32)DSL(DS_out_count) <= (s32)DSL(DS_unpacked_len)) {
        u16 off = DSW(DS_pack_outptr), seg = DSW(DS_pack_outptr_seg);
        u32 next = (u32)off + 1;
        DSW(DS_pack_outptr) = (u16)next;
        DSW(DS_pack_outptr_seg) = (u16)(((next >> 16) << 12) + seg);
        u32 l = lin(seg, off);
        if (l < MEM_SIZE) mem[l] = c;
    }
}

/* 0x9CF9 rle90_out */
static void rle90_out(u8 c)
{
    switch (DSW(DS_rle_state)) {
    case 0:
        if (c == 0x90) { DSW(DS_rle_state) = 1; break; }
        DSW(DS_rle_last) = c;
        pack_putc(c);
        break;
    case 1:
        if (c == 0) pack_putc(0x90);
        else while (--c) pack_putc((u8)DSW(DS_rle_last));
        DSW(DS_rle_state) = 0;
        break;
    default:
        fatal(ds_str(0x69C7), DSW(DS_rle_state));       /* "Bad NCR unpacking state %d\n" */
    }
}

/* 0x9C76 huff_read_word */
static u16 huff_read_word(void)
{
    u16 lo = pack_getc();
    u16 hi = pack_getc();
    return (u16)((u16)(hi << 8) | lo);
}

/* 0x9C1E huff_read_tree */
static void huff_read_tree(void)
{
    DSW(DS_huff_bpos) = 0x63;
    DSW(DS_huff_numnodes) = huff_read_word();
    s16 n = (s16)DSW(DS_huff_numnodes);
    if (n < 0 || n >= 0x101) fatal("%s", ds_str(0x69A6));      /* "File has an invalid decode tree\n" */
    DSW(DS_huff_nodes) = 0xFEFF;
    DSW(DS_huff_nodes + 2) = 0xFEFF;
    for (s16 i = 0; (s16)DSW(DS_huff_numnodes) > i; i++) {
        DSW((u16)(DS_huff_nodes + i * 4)) = huff_read_word();
        DSW((u16)(DS_huff_nodes + i * 4 + 2)) = huff_read_word();
    }
}

/* 0x9C90 huff_decode: next symbol, 0xFFFF at EOF */
static u16 huff_decode(void)
{
    s16 i = 0;
    for (;;) {
        DSW(DS_huff_bpos)++;
        if ((s16)DSW(DS_huff_bpos) > 7) {
            u16 c = pack_getc();
            DSW(DS_huff_cur) = c;
            if (c == 0xFFFF) return 0xFFFF;
            DSW(DS_huff_bpos) = 0;
        } else {
            DSW(DS_huff_cur) = (u16)((s16)DSW(DS_huff_cur) >> 1);
        }
        u16 bx = (u16)(i * 4);
        u16 di = (u16)((DSW(DS_huff_cur) & 1) * 2);
        i = DSS((u16)(DS_huff_nodes + bx + di));
        if (i >= 0) continue;
        i = (s16)-(i + 1);
        return (i == 0x100) ? 0xFFFF : (u16)i;
    }
}

/* 0x9FA2 lzw_getcode */
static u16 lzw_getcode(void)
{
    if ((s16)DSW(DS_lzw_clear_flg) > 0 || (s16)DSW(DS_lzw_offset) >= (s16)DSW(DS_lzw_size) ||
        (s16)DSW(DS_lzw_free_ent) > (s16)DSW(DS_lzw_maxcode)) {
        if ((s16)DSW(DS_lzw_free_ent) > (s16)DSW(DS_lzw_maxcode)) {
            DSW(DS_lzw_n_bits)++;
            if (DSW(DS_lzw_n_bits) == 12) DSW(DS_lzw_maxcode) = 0x1000;
            else DSW(DS_lzw_maxcode) = (u16)((1u << (DSB(DS_lzw_n_bits) & 0x0F)) - 1);
        }
        if ((s16)DSW(DS_lzw_clear_flg) > 0) {
            DSW(DS_lzw_n_bits) = 9;
            DSW(DS_lzw_maxcode) = 511;
            DSW(DS_lzw_clear_flg) = 0;
        }
        DSW(DS_lzw_size) = 0;
        while ((s16)DSW(DS_lzw_size) < (s16)DSW(DS_lzw_n_bits)) {
            u16 c = pack_getc();
            if (c == 0xFFFF) break;
            DSB((u16)(DS_lzw_buf + DSW(DS_lzw_size))) = (u8)c;
            DSW(DS_lzw_size)++;
        }
        if ((s16)DSW(DS_lzw_size) <= 0) return 0xFFFF;
        DSW(DS_lzw_offset) = 0;
        DSW(DS_lzw_size) = (u16)((DSW(DS_lzw_size) << 3) - DSW(DS_lzw_n_bits) + 1);
    }
    u16 r_off = DSW(DS_lzw_offset);
    s16 bits = (s16)DSW(DS_lzw_n_bits);
    u16 bp = (u16)(DS_lzw_buf + ((s16)r_off >> 3));
    r_off &= 7;
    u16 gcode = (u16)(DSB(bp) >> r_off);
    bp++;
    r_off = (u16)(8 - r_off);
    bits = (s16)(bits - r_off);
    if (bits >= 8) {
        gcode |= (u16)(DSB(bp) << r_off);
        bp++;
        r_off = (u16)(r_off + 8);
        bits = (s16)(bits - 8);
    }
    gcode |= (u16)((DSB((u16)(DS_lzw_rmask + bits)) & DSB(bp)) << r_off);
    DSW(DS_lzw_offset) = (u16)(DSW(DS_lzw_offset) + DSW(DS_lzw_n_bits));
    return gcode;
}

/* 0x9E1D lzw_decompress (method 8), output through rle90_out */
static void lzw_decompress(void)
{
    /* 0xA0CF lzw_alloc_tables: PORT host-side table (DS:6A3C near-heap pointer not modelled). */
    DSW(DS_lzw_size) = 0;
    DSW(DS_lzw_offset) = 0;
    u16 c = pack_getc();
    if (c != 12) fatal(ds_str(0x69E3), (s16)c);         /* " File packed with too many bits %d\n" */
    DSW(DS_lzw_n_bits) = 9;
    DSW(DS_lzw_clear_flg) = 0;
    DSW(DS_lzw_maxcode) = 511;
    for (s16 code = 0xFF; code >= 0; code--) {
        lzw_prefix_wr((u16)code, 0);
        lzw_tab[LZW_SUFFIX + code] = (u8)code;
    }
    DSW(DS_lzw_free_ent) = 0x101;
    u16 oldcode = lzw_getcode();
    u16 finchar = oldcode;
    if (oldcode == 0xFFFF) fatal("%s", ds_str(0x6A07));  /* "Early EOF in decompress" */
    rle90_out((u8)finchar);
    u16 stackp = LZW_STACK;

    for (;;) {
        u16 code = lzw_getcode();
        if ((s16)code <= -1) break;
        if (code == 0x100) {
            for (s16 k = 0xFF; k >= 0; k--) lzw_prefix_wr((u16)k, 0);
            DSW(DS_lzw_clear_flg) = 1;
            DSW(DS_lzw_free_ent) = 0x100;
            code = lzw_getcode();
            if (code == 0xFFFF) break;
        }
        u16 incode = code;
        if ((s16)code >= (s16)DSW(DS_lzw_free_ent)) {  /* KwKwK */
            lzw_tab[stackp++] = (u8)finchar;
            code = oldcode;
        }
        while ((s16)code >= 0x100) {
            lzw_tab[stackp++] = lzw_tab[LZW_SUFFIX + (code & 0x0FFF)];
            code = lzw_prefix_rd(code & 0x0FFF);
        }
        finchar = lzw_tab[LZW_SUFFIX + (code & 0x0FFF)];
        lzw_tab[stackp++] = (u8)finchar;
        do {
            stackp--;
            rle90_out(lzw_tab[stackp]);
        } while (stackp > LZW_STACK);
        code = DSW(DS_lzw_free_ent);
        if ((s16)code < 0x1000) {
            lzw_prefix_wr(code, oldcode);
            lzw_tab[LZW_SUFFIX + code] = (u8)finchar;
            DSW(DS_lzw_free_ent) = (u16)(code + 1);
        }
        oldcode = incode;
    }
    /* 0xA111 lzw_free_tables */
}

/* 0x9A86 load_packed_archive */
FarPtr load_packed_archive(const char *fname, u16 reserve)
{
    u16 seg = cache_find(fname);
    if (seg) return far_make(seg, 0);

    /* PORT: MSC open() returns -1 for a missing file and the original only tests for 0 ("%s FILE OPEN
     * ERROR" is unreachable), so the failure surfaces as the short header read below. */
    pack_file = open_game_file(fname);
    u8 h[16];
    size_t n = pack_file ? fread(h, 1, 16, pack_file) : 0;
    if (n != 16) fatal(ds_str(0x693A), fname);          /* "%s FILE READ ERROR\n" */
    u32 packed   = (u32)h[4] | (u32)h[5] << 8 | (u32)h[6] << 16 | (u32)h[7] << 24;
    u32 unpacked = (u32)h[8] | (u32)h[9] << 8 | (u32)h[10] << 16 | (u32)h[11] << 24;
    u16 method   = (u16)(h[12] | h[13] << 8);           /* CRC at +14 is not checked */

    u32 paras = (unpacked >> 4) + 1;
    seg = cache_alloc(fname, (u16)paras, reserve);

    DSL(DS_in_count) = 0;
    DSL(DS_out_count) = 0;
    DSL(DS_packed_left) = packed;
    DSL(DS_unpacked_len) = unpacked;
    DSW(DS_pack_outptr) = 0;
    DSW(DS_pack_outptr_seg) = seg;
    DSW(DS_pack_inpos) = 0x400;                         /* force a refill */
    DSW(DS_rle_state) = 0;

    u16 c;
    switch (method) {
    case 2: while ((c = pack_getc()) != 0xFFFF) pack_putc((u8)c); break;   /* stored */
    case 3: while ((c = pack_getc()) != 0xFFFF) rle90_out((u8)c); break;   /* RLE90 only */
    case 4: huff_read_tree(); while ((c = huff_decode()) != 0xFFFF) rle90_out((u8)c); break;
    case 8: lzw_decompress(); break;
    default:
        fclose(pack_file);
        pack_file = NULL;
        fatal(ds_str(0x696A), method);                  /* "UNKNOWN PACK MODE %d\n" */
    }
    fclose(pack_file);
    pack_file = NULL;
    if (DSL(DS_out_count) != DSL(DS_unpacked_len))
        fatal(ds_str(0x6980), (unsigned)(u16)DSL(DS_out_count));   /* "%x UNPACKED SIZE ERROR\n" (low word) */
    return far_make(seg, 0);
}
#endif

/* ---------------------------------------------------------------- lookup */

/* 0x6762 res_find */
FarPtr res_find(FarPtr arc, char *name4)
{
    for (int i = 0; i < 4; i++) {
        if (name4[i] == 0) {                            /* space-pad the caller's string in place */
            for (; i < 4; i++) name4[i] = ' ';
            break;
        }
    }
    u16 dx = rd16(arc.seg, (u16)(arc.off + 4));
    u16 bx = (u16)(arc.off + 6);
    do {                                                /* dec/jge: count+1 entries */
        int j;
        for (j = 0; j < 4; j++) {
            u8 a = rd8(arc.seg, (u16)(bx + j));
            u8 q = (u8)name4[j];
            if (a == q) continue;
            if (a == 0 && q == 0x20) j = 4;             /* NUL in the archive matches a padding space */
            break;
        }
        if (j >= 4) {
            u16 count = rd16(arc.seg, (u16)(arc.off + 4));
            return far_rd(arc.seg, (u16)(bx + (u16)(count << 2)));
        }
        bx = (u16)(bx + 4);
    } while ((s16)--dx >= 0);
    fatal(ds_str(EGA_CGA(0x63CE, 0x638C)), name4);                       /* "%-4.4s SHAPE OR SOUND NOT FOUND\n" */
}

/* 0x94C7 res_find_list: 4-char names until a NUL first byte; far pointers to out_seg:out_off[i] */
void res_find_list(FarPtr arc, const char *names, u16 out_seg, u16 out_off)
{
    char *p = (char *)names;                            /* res_find pads in place, like the original */
    for (u16 i = 0; *p; i++, p += 4) {
        FarPtr r = res_find(arc, p);
        far_wr(out_seg, (u16)(out_off + i * 4), r);
    }
}

/* ---------------------------------------------------------------- misc */

/* 0x8D8C crc8_b8: reflected CRC-8, polynomial 0xB8 (len 0 still processes one byte) */
u8 crc8_b8(const u8 *buf, int len, u8 seed)
{
    u8 ah = seed;
    u16 dx = (u16)len;
    do {
        ah ^= *buf++;
        for (int k = 0; k < 8; k++) {
            bool carry = ah & 1;
            ah >>= 1;
            if (carry) ah ^= 0xB8;
        }
    } while ((s16)--dx > 0);
    return ah;
}

/* 0x8BAF rand8: table PRNG */
u8 rand8(void)
{
    DSW(DS_rand8_idx)--;
    u16 si = DSW(DS_rand8_idx) & 0xFF;
    u8 al = DSB((u16)(DS_rand8_tab + si));
    u8 carry = al < 0x80;                               /* cmp al,80h ; rcl al,1 */
    al = (u8)(al << 1 | carry);
    DSB((u16)(DS_rand8_tab + si)) = al;
    return al;
}

/* 0xA90E MSC rand */
int crt_rand(void)
{
    u32 seed = DSL(DS_crt_seed) * 0x343FDu + 0x269EC3u;
    DSL(DS_crt_seed) = seed;
    return (int)((seed >> 16) & 0x7FFF);
}

/* 0xA8FD MSC srand */
void crt_srand(u16 seed)
{
    DSW(DS_crt_seed) = seed;
    DSW(DS_crt_seed + 2) = 0;
}
