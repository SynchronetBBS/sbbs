#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bits.h"
#include "fmt_pic.h"
#include "lbxgfx.h"
#include "lib.h"
#include "log.h"
#include "os.h"
#include "types.h"
#include "util.h"

/* -------------------------------------------------------------------------- */

#ifdef FEATURE_MODEBUG
int opt_modebug = 0;
#endif

/* -------------------------------------------------------------------------- */

struct picanim_s {
    struct pic_s pic;
    const char *filename;
    uint8_t *buf;
    int len, alloc;
};

struct animopts_s {
    bool fmt1;
    bool indep;
    int num_frames;
    int loop_frame;
    int extra_start_frame;
    int pal_first;
    int pal_num;
};

/* -------------------------------------------------------------------------- */

static void gfx_pa_add_more(struct picanim_s *pa, int len)
{
    if (pa->alloc < (pa->len + len)) {
        int newlen = ((pa->len + len) | 0xfff) + 1;
        pa->buf = lib_realloc(pa->buf, newlen);
    }
}

static void gfx_pa_add_buf(struct picanim_s *pa, const uint8_t *buf, int len)
{
    gfx_pa_add_more(pa, len);
    memcpy(&(pa->buf[pa->len]), buf, len);
    pa->len += len;
}

static void gfx_pa_add_byte(struct picanim_s *pa, uint8_t b)
{
    gfx_pa_add_more(pa, 1);
    pa->buf[pa->len++] = b;
}

static int gfx_convert(const char *filename_out, const char **filenames_in, int num_in, struct animopts_s *opts)
{
    struct picanim_s *picanim = lib_malloc(sizeof(struct picanim_s) * opts->num_frames);
    uint8_t *bufdiff = NULL;
    uint8_t *hdr = NULL;
    FILE *fd = NULL;
    int res = 1, w = 1, h = 1, hdrlen;
    for (int frame = 0; frame < num_in; ++frame) {
        struct picanim_s *pa = &(picanim[frame]);
        pa->filename = filenames_in[frame];
        if (!fmt_pic_load(pa->filename, &(pa->pic))) {
            goto fail;
        }
        if (frame == 0) {
            w = pa->pic.w;
            h = pa->pic.h;
        } else if ((pa->pic.w != w) || (pa->pic.h != h)) {
            log_error("size mismatch in '%s' (%ix%i != %ix%i) '%s'\n", pa->filename, pa->pic.w, picanim[frame].pic.h, w, h, picanim[0].filename);
            goto fail;
        }
    }
    for (int frame = num_in; frame < opts->num_frames; ++frame) {
        struct picanim_s *pa = &(picanim[frame]);
        pa->filename = "(none)";
    }
    bufdiff = lib_malloc(w * h);
    for (int frame = 0; frame < opts->num_frames; ++frame) {
        struct picanim_s *pa = &(picanim[frame]);
        if ((frame == 0) || (opts->indep) || (frame == opts->extra_start_frame)) {
            for (int i = 0; i < (w * h); ++i) {
                bufdiff[i] = (pa->pic.pix[i] != 0);
            }
            gfx_pa_add_byte(pa, 1);
        } else if (frame >= num_in) {
            memset(bufdiff, 0, w * h);
            gfx_pa_add_byte(pa, 0);
        } else {
            for (int i = 0; i < (w * h); ++i) {
                bufdiff[i] = (picanim[frame - 1].pic.pix[i] != pa->pic.pix[i]);
            }
            gfx_pa_add_byte(pa, 0);
        }
        for (int x = 0; x < w; ++x) {
            uint8_t colbufu[256];
            uint8_t colbufc[512];
            uint8_t *pc, *pu;
            int y, lenc, lenu;
            pu = colbufu;
            pc = colbufc;
            y = lenc = lenu = 0;
            *pu = 0x00;
            pu += 2;
            *pc = 0x80;
            pc += 2;
            while (y < h) {
                int skiplen;
                skiplen = 0;
                while ((y < h) && (bufdiff[y * w + x] == 0)) {
                    ++skiplen;
                    ++y;
                }
                if (y == h) {
                    if (lenu == 0) {
                        colbufu[0] = 0xff;
                        colbufc[0] = 0xff;
                        lenu = 1;
                        lenc = 1;
                    }
                } else {
                    int rlen;
                    rlen = 1;
                    while (((y + rlen) < h) && (bufdiff[(y + rlen) * w + x] != 0)) {
                        ++rlen;
                    }
                    /* uncompressed */
                    if ((lenu + rlen + 2) < 256) {
                        *pu++ = rlen;
                        *pu++ = skiplen;
                        for (int i = 0; i < rlen; ++i) {
                            *pu++ = pa->pic.pix[(y + i) * w + x];
                        }
                        lenu += rlen + 2;
                    } else {
                        lenu = 512;
                    }
                    /* compressed */
                    if (lenc < 255) {
                        uint8_t *p;
                        int clen;
                        pc[1] = skiplen;
                        p = pc + 2;
                        clen = 0;
                        for (int i = 0; i < rlen;) {
                            uint8_t n, c;
                            n = 1;
                            c = pa->pic.pix[(y + i) * w + x];
                            while ((n < 0x20) && ((i + n) < rlen) && (c == pa->pic.pix[(y + i + n) * w + x])) {
                                ++n;
                            }
                            if ((n > 1) || (c > 0xdf)) {
                                *p++ = n + 0xdf;
                                ++clen;
                            }
                            *p++ = c;
                            ++clen;
                            i += n;
                        }
                        pc[0] = clen;
                        lenc += clen + 2;
                        pc += clen + 2;
                    } else {
                        lenc = 512;
                    }
                    y += rlen;
                }
            }
            if ((lenc > 255) && (lenu > 255)) {
                log_error("column %i in '%s' encoded is too long (h %i, un %i, compr %i)\n", x, pa->filename, h, lenu, lenc);
                goto fail;
            }
            colbufu[1] = lenu;
            colbufc[1] = lenc;
            if (lenc || lenu) {
                const uint8_t *b;
                int l;
                if (lenc < lenu) {
                    l = lenc;
                    b = colbufc;
                } else {
                    l = lenu;
                    b = colbufu;
                }
                if (l > 1) {
                    l += 2;
                }
                gfx_pa_add_buf(pa, b, l);
            }
        }
    }
    {
        int paloffs = 0, offs;
        hdrlen = 0x12 + 4 * (opts->num_frames + 1);
        if (opts->pal_num) {
            paloffs = hdrlen;
            hdrlen += 8 + 3 * opts->pal_num;
        }
        hdr = lib_malloc(hdrlen);
        SET_LE_16(&(hdr[0]), w);
        SET_LE_16(&(hdr[2]), h);
        SET_LE_16(&(hdr[6]), opts->num_frames);
        {
            int loop_frame = opts->loop_frame;
            if (loop_frame >= opts->num_frames) {
                loop_frame = opts->num_frames;
            }
            SET_LE_16(&(hdr[8]), loop_frame);
        }
        SET_LE_16(&(hdr[0xe]), paloffs);
        hdr[0x10] = opts->indep ? 1 : 0;
        hdr[0x11] = opts->fmt1 ? 1 : 0;
        offs = hdrlen;
        for (int frame = 0; frame < opts->num_frames; ++frame) {
            SET_LE_32(&(hdr[0x12 + 4 * frame]), offs);
            offs += picanim[frame].len;
        }
        SET_LE_32(&(hdr[0x12 + 4 * opts->num_frames]), offs);
        if (opts->pal_num) {
            SET_LE_16(&(hdr[paloffs + 0]), paloffs + 8);
            SET_LE_16(&(hdr[paloffs + 2]), opts->pal_first);
            SET_LE_16(&(hdr[paloffs + 4]), opts->pal_num);
            memcpy(&(hdr[paloffs + 8]), &(picanim[0].pic.pal[opts->pal_first * 3]), opts->pal_num * 3);
        }
    }
    fd = fopen(filename_out, "wb");
    if (0
      || (!fd)
      || (fwrite(hdr, hdrlen, 1, fd) != 1)
    ) {
        log_error("creating file '%s'\n", filename_out);
        goto fail;
    }
    for (int frame = 0; frame < opts->num_frames; ++frame) {
        struct picanim_s *pa = &(picanim[frame]);
        if (fwrite(pa->buf, pa->len, 1, fd) != 1) {
            log_error("writing file '%s'\n", filename_out);
            goto fail;
        }
    }
    res = 0;
fail:
    if (fd) {
        fclose(fd);
        fd = NULL;
    }
    for (int frame = 0; frame < opts->num_frames; ++frame) {
        fmt_pic_free(&(picanim[frame].pic));
        lib_free(picanim[frame].buf);
        picanim[frame].buf = NULL;
    }
    lib_free(picanim);
    picanim = NULL;
    lib_free(bufdiff);
    bufdiff = NULL;
    lib_free(hdr);
    hdr = NULL;
    return res;
}

static int gfx_dump(const char *filename)
{
    uint8_t *data, *p;
    int frames, w, h;
    uint32_t len;
    p = data = util_file_load(filename, &len);
    if (!data) {
        return 1;
    }
    w = lbxgfx_get_w(p);
    h = lbxgfx_get_h(p);
    frames = lbxgfx_get_frames(p);
    log_message("%ix%i %i frames\n", w, h, frames);
    for (int f = 0; f < frames; ++f) {
        p = lbxgfx_get_frameptr(data, f);
        ++p;
        for (int x = 0; x < w; ++x) {
            int l;
            uint8_t b;
            b = *p;
            if (b == 0xff) {
                l = 1;
            } else {
                l = 2 + p[1];
            }
            log_message("f:%i x:%i l:%i", f, x, l);
            for (int i = 0; i < l; ++i) {
                log_message(" %02x", *p++);
            }
            log_message("\n");
        }
    }
    lib_free(data);
    return 0;
}

static void show_usage(void)
{
    fprintf(stderr, "Usage:\n"
                    "    1oom_gfxconv [OPTIONS] OUT.BIN IN.PCX|=WxHcC [INn.PCX]*\n"
                    "    1oom_gfxconv -d IN.BIN\n"
                    "Options:\n"
                    "    -f       Make format 1 binary (only council.lbx item 1)\n"
                    "    -i       All independent frames (winlose.lbx items 1-...)\n"
                    "    -e N     Extra independent frame (embassy.lbx items 2-...)\n"
                    "    -n N     Set number of frames (default N = number of input files)\n"
                    "    -l N     Set loop frame\n"
                    "    -p F N   Include palette ; First, Number of colors\n"
                    "    -d       Dump converted file for debugging\n"
           );
}

/* -------------------------------------------------------------------------- */

int main_1oom(int argc, char **argv)
{
    struct animopts_s opts = { false, false, 0, 0, 0, 0, 0 };
    int i;
    bool mode_dump = false;
    i = 1;
    if (i >= argc) {
        show_usage();
        return 1;
    }
    while (argv[i][0] == '-') {
        uint32_t v;
        if (argv[i][2] != '\0') {
            show_usage();
            return 1;
        }
        switch (argv[i][1]) {
            case 'd':
                mode_dump = true;
                break;
            case 'f':
                opts.fmt1 = true;
                break;
            case 'i':
                opts.indep = true;
                break;
            case 'n':
                ++i;
                if ((i == argc) || (!util_parse_number(argv[i], &v))) {
                    show_usage();
                    return 1;
                }
                opts.num_frames = v;
                break;
            case 'e':
                ++i;
                if ((i == argc) || (!util_parse_number(argv[i], &v))) {
                    show_usage();
                    return 1;
                }
                opts.extra_start_frame = v;
                break;
            case 'l':
                ++i;
                if ((i == argc) || (!util_parse_number(argv[i], &v))) {
                    show_usage();
                    return 1;
                }
                opts.loop_frame = v;
                break;
            case 'p':
                ++i;
                if ((i == argc) || (!util_parse_number(argv[i], &v)) || (v > 255)) {
                    show_usage();
                    return 1;
                }
                opts.pal_first = v;
                ++i;
                if ((i == argc) || (!util_parse_number(argv[i], &v)) || (v > 256) || ((opts.pal_first + v) > 256)) {
                    show_usage();
                    return 1;
                }
                opts.pal_num = v;
                break;
            default:
                show_usage();
                return 1;
        }
        ++i;
    }
    if (mode_dump) {
        const char *filename_in;
        filename_in = argv[i++];
        return gfx_dump(filename_in);
    } else {
        const char *filename_out;
        int num_in;
        filename_out = argv[i++];
        num_in = argc - i;
        if (num_in < 1) {
            show_usage();
            return 1;
        }
        if (opts.num_frames == 0) {
            opts.num_frames = num_in;
        } else if (opts.num_frames < num_in) {
            num_in = opts.num_frames;
        }
        return gfx_convert(filename_out, (const char **)&argv[i], num_in, &opts);
    }
}
