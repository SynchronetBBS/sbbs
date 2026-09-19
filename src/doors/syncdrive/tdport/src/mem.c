#include "mem.h"

#include <SDL3/SDL.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

u8 mem[MEM_SIZE];
u32 mem_image_size;

static void set_err(char *err, size_t n, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(err, n, fmt, ap);
    va_end(ap);
}

static u16 le16(const u8 *p) { return (u16)(p[0] | p[1] << 8); }

typedef struct { u16 seg, off; } Reloc;

/* Microsoft EXEPACK: decompresses the load image in place (backwards) and returns the relocation
 * table kept in the stub. Port of tools/unexepack.py. */
static bool exepack_unpack(const u8 *image, size_t image_len, u16 cs,
                           u8 **out, size_t *out_len, Reloc **relocs, int *nrel,
                           char *err, size_t errlen)
{
    size_t ep = (size_t)cs * 16;
    if (ep + 18 > image_len) { set_err(err, errlen, "EXEPACK header out of range"); return false; }
    size_t sig = (memcmp(image + ep + 16, "RB", 2) == 0) ? ep + 16 : ep + 14;
    if (memcmp(image + sig, "RB", 2) != 0) { set_err(err, errlen, "not an EXEPACK file"); return false; }
    u16 exepack_size = le16(image + ep + 6);
    u16 dest_len     = le16(image + ep + 12);
    u16 skip_len     = (sig == ep + 16) ? le16(image + ep + 14) : 1;

    static const char msg[] = "Packed file is corrupt";
    const u8 *stub = image + ep;
    size_t stub_len = exepack_size;
    if (ep + stub_len > image_len) stub_len = image_len - ep;
    size_t rel = 0;
    for (size_t i = 0; i + sizeof msg - 1 <= stub_len; i++)
        if (memcmp(stub + i, msg, sizeof msg - 1) == 0) { rel = i + sizeof msg - 1; break; }
    if (!rel) { set_err(err, errlen, "EXEPACK relocation table not found"); return false; }

    *relocs = NULL;
    *nrel = 0;
    size_t p = rel;
    for (int seg = 0; seg < 16; seg++) {
        if (p + 2 > stub_len) { set_err(err, errlen, "EXEPACK relocation table truncated"); return false; }
        u16 n = le16(stub + p); p += 2;
        Reloc *grown = SDL_realloc(*relocs, sizeof(Reloc) * (size_t)(*nrel + n + 1));
        if (!grown) { set_err(err, errlen, "out of memory"); return false; }
        *relocs = grown;
        for (u16 k = 0; k < n; k++, p += 2) {
            (*relocs)[*nrel].seg = (u16)(seg * 0x1000);
            (*relocs)[*nrel].off = le16(stub + p);
            (*nrel)++;
        }
    }

    size_t packed_len = ep - (size_t)(skip_len - 1) * 16;
    size_t unpacked_len = (size_t)dest_len * 16;
    size_t buf_len = packed_len > unpacked_len ? packed_len : unpacked_len;
    u8 *buf = SDL_calloc(1, buf_len);
    if (!buf) { set_err(err, errlen, "out of memory"); return false; }
    memcpy(buf, image, packed_len);
    size_t src = packed_len, dst = unpacked_len;
    while (src > 0 && buf[src - 1] == 0xFF) src--;
    for (;;) {
        if (src < 3) goto corrupt;
        u8 cmd = buf[--src];
        u16 len = (u16)(buf[src - 1] << 8 | buf[src - 2]);
        src -= 2;
        if ((cmd & 0xFE) == 0xB0) {
            if (src < 1 || dst < len) goto corrupt;
            u8 fill = buf[--src];
            dst -= len;
            memset(buf + dst, fill, len);
        } else if ((cmd & 0xFE) == 0xB2) {
            if (src < len || dst < len) goto corrupt;
            src -= len; dst -= len;
            memmove(buf + dst, buf + src, len);
        } else goto corrupt;
        if (cmd & 1) break;
    }
    *out = buf;
    *out_len = unpacked_len;
    return true;
corrupt:
    SDL_free(buf);
    set_err(err, errlen, "EXEPACK data is corrupt");
    return false;
}

bool mem_load_exe(const char *path, char *err, size_t errlen)
{
    size_t file_len;
    u8 *file = SDL_LoadFile(path, &file_len);
    if (!file) { set_err(err, errlen, "cannot read %s: %s", path, SDL_GetError()); return false; }

    bool ok = false;
    u8 *image = NULL;
    Reloc *relocs = NULL;
    int nrel = 0;

    if (file_len < 0x1C || le16(file) != 0x5A4D) { set_err(err, errlen, "%s is not a DOS executable", path); goto done; }
    u16 cblp = le16(file + 2), cp = le16(file + 4), crlc = le16(file + 6), cparhdr = le16(file + 8);
    u16 cs = le16(file + 0x16), lfarlc = le16(file + 0x18);
    size_t mz_len = (size_t)cp * 512 - (cblp ? 512 - cblp : 0);
    size_t hdr_len = (size_t)cparhdr * 16;
    if (mz_len > file_len) mz_len = file_len;
    if (hdr_len >= mz_len) { set_err(err, errlen, "%s: bad MZ header", path); goto done; }
    const u8 *body = file + hdr_len;
    size_t body_len = mz_len - hdr_len;

    size_t image_len;
    if ((size_t)cs * 16 + 18 <= body_len &&
        (memcmp(body + (size_t)cs * 16 + 14, "RB", 2) == 0 || memcmp(body + (size_t)cs * 16 + 16, "RB", 2) == 0)) {
        if (!exepack_unpack(body, body_len, cs, &image, &image_len, &relocs, &nrel, err, errlen)) goto done;
    } else {
        image = SDL_malloc(body_len);
        if (!image) { set_err(err, errlen, "out of memory"); goto done; }
        memcpy(image, body, body_len);
        image_len = body_len;
        relocs = SDL_malloc(sizeof(Reloc) * (crlc + 1u));
        for (u16 i = 0; i < crlc && lfarlc + 4u * i + 4 <= file_len; i++) {
            relocs[nrel].off = le16(file + lfarlc + 4 * i);
            relocs[nrel].seg = le16(file + lfarlc + 4 * i + 2);
            nrel++;
        }
    }

    if (lin(LOAD_SEG, 0) + image_len > lin(DGROUP, 0) + 0x10000u) { set_err(err, errlen, "%s: load image too large", path); goto done; }
    memset(mem, 0, sizeof mem);
    memcpy(mp(LOAD_SEG, 0), image, image_len);
    for (int i = 0; i < nrel; i++) {
        u32 at = lin(LOAD_SEG, 0) + lin(relocs[i].seg, relocs[i].off);
        if (at + 2 > lin(LOAD_SEG, 0) + image_len) continue;
        u16 v = (u16)(mem[at] | mem[at + 1] << 8);
        v = (u16)(v + LOAD_SEG);
        mem[at] = (u8)v;
        mem[at + 1] = (u8)(v >> 8);
    }
    mem_image_size = (u32)image_len;

    /* Identify the build: each executable keeps these strings at fixed DGROUP offsets. */
#if TD_CGA
    if (memcmp(mp(DGROUP, 0x08DD), "ROADDATA.SHP", 12) != 0 || memcmp(mp(DGROUP, 0x0063), "xroada.cmp", 10) != 0) {
        set_err(err, errlen, "%s is not the expected TDCGA.EXE (CGA build of Test Drive 1987)", path);
        goto done;
    }
#else
    if (memcmp(mp(DGROUP, 0x08F5), "ROADDATA.SHP", 12) != 0 || memcmp(mp(DGROUP, 0x00DE), "94857102387604294775", 20) != 0) {
        set_err(err, errlen, "%s is not the expected TDEGA.EXE (EGA build of Test Drive 1987)", path);
        goto done;
    }
#endif
    ok = true;
done:
    SDL_free(relocs);
    SDL_free(image);
    SDL_free(file);
    return ok;
}
