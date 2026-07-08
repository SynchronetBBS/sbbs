#include "config.h"

#include <stdio.h>
#include <string.h>

#include "fmt_pic.h"
#include "bits.h"
#include "lib.h"
#include "log.h"
#include "types.h"
#include "util.h"

/* -------------------------------------------------------------------------- */

#define DEBUGLEVEL_FMTPIC   4

/* -------------------------------------------------------------------------- */

static bool fmt_pic_pcx_detect(const uint8_t *data, uint32_t len)
{
    int xmin, xmax, ymin, ymax;
    if (0
      || (len < 128)
      || (data[0] != 10)
      || (data[1] > 5)
      || (data[2] != 1)
      || (data[3] != 8)
      || (data[65] != 1)
    ) {
        return false;
    }
    xmin = GET_LE_16(&data[4]);
    ymin = GET_LE_16(&data[6]);
    xmax = GET_LE_16(&data[8]);
    ymax = GET_LE_16(&data[10]);
    if ((xmin > xmax) || (ymin > ymax)) {
        return false;
    }
    return true;
}

static bool fmt_pic_pcx_decode(struct pic_s *pic)
{
    int bpl, w, h, len_in = pic->len;
    const uint8_t *data = pic->coded;
    bpl = GET_LE_16(&data[66]);
    {
        int xmin, xmax, ymin, ymax;
        xmin = GET_LE_16(&data[4]);
        ymin = GET_LE_16(&data[6]);
        xmax = GET_LE_16(&data[8]);
        ymax = GET_LE_16(&data[10]);
        pic->w = w = xmax - xmin + 1;
        pic->pitch = w;
        pic->h = h = ymax - ymin + 1;
        LOG_DEBUG((DEBUGLEVEL_FMTPIC, "%s: [%i,%i]-[%i,%i] w:%i bpl:%i\n", __func__, xmin, ymin, xmax, ymax, w, bpl));
    }
    data += 128;
    len_in -= 128;
    pic->pix = lib_malloc(w * h + 1/*bpl padding*/);
    for (int y = 0; y < h; ++y) {
        uint8_t *p;
        p = &(pic->pix[y * w]);
        for (int x = 0; x < bpl;) {
            uint8_t c, n;
            if (len_in-- == 0) {
                return false;
            }
            c = *data++;
            if (c & 0xc0) {
                n = c & 0x3f;
                if (len_in-- == 0) {
                    return false;
                }
                c = *data++;
            } else {
                n = 1;
            }
            x += n;
            while (n--) {
                *p++ = c;
            }
        }
    }
    if ((len_in > 0) && (*data++ == 12) && (--len_in >= (256 * 3))) {
        uint8_t *p;
        pic->pal = p = lib_malloc(256 * 3);
        for (int i = 0; i < (256 * 3); ++i) {
            *p++ = *data++ / 4;
        }
        len_in -= 256 * 3;
    } else {
        pic->pal = NULL;
    }
    return true;
}

static bool fmt_pic_pcx_encode(struct pic_s *pic)
{
    int len = 0, w = pic->w, h = pic->h;
    uint8_t *p;
    pic->coded = p = lib_malloc(128 + w * h * 2 + (pic->pal ? (3 * 256 + 1) : 0));
    SET_BE_32(p, 0x0a050108);
    SET_LE_16(&p[8], w - 1);
    SET_LE_16(&p[10], h - 1);
    SET_LE_16(&p[12], 320);
    SET_LE_16(&p[14], 200);
    p[65] = 1;
    SET_LE_16(&p[66], w);
    SET_LE_16(&p[68], 1);
    SET_LE_16(&p[70], 320);
    SET_LE_16(&p[72], 200);
    len += 128;
    p += 128;
    for (int y = 0; y < h; ++y) {
        const uint8_t *q = &(pic->pix[y * pic->pitch]);
        int x;
        x = 0;
        while (x < w) {
            uint8_t n, c;
            n = 1;
            c = *q;
            while ((n < 0x3f) && ((x + n) < w) && (c == q[n])) {
                ++n;
            }
            if ((n > 1) || (c & 0xc0)) {
                *p++ = n | 0xc0;
                ++len;
            }
            *p++ = c;
            x += n;
            q += n;
            ++len;
        }
    }
    if (pic->pal) {
        const uint8_t *q = pic->pal;
        *p++ = 12;
        for (int i = 0; i < (256 * 3); ++i) {
            *p++ = *q++ * 4;
        }
        len += 256 * 3 + 1;
    }
    pic->coded = lib_realloc(pic->coded, len);
    pic->len = len;
    return true;
}

static bool fmt_pic_equaldef_decode(struct pic_s *pic)
{
    const char *p = (const char *)pic->coded + 1;
    char c;
    int v, w, h;
    v = 0;
    while ((c = *p++) != 'x') {
        if ((c >= '0') && (c <= '9')) {
            v *= 10;
            v += c - '0';
        } else {
            break;
        }
    }
    if (c != 'x') {
        return false;
    }
    pic->w = w = v;
    v = 0;
    while ((c = *p++) != 'c') {
        if ((c >= '0') && (c <= '9')) {
            v *= 10;
            v += c - '0';
        } else {
            break;
        }
    }
    if (c != 'c') {
        return false;
    }
    pic->h = h = v;
    v = 0;
    while ((c = *p++) != '\0') {
        if ((c >= '0') && (c <= '9')) {
            v *= 10;
            v += c - '0';
        } else {
            break;
        }
    }
    if (c != '\0') {
        return false;
    }
    pic->pix = lib_malloc(w * h);
    memset(pic->pix, v, w * h);
    pic->pal = NULL;
    return true;
}

static pic_type_t fmt_pic_detect(const uint8_t *data, uint32_t len)
{
    if ((len == 0) && (data[0] == '=')) {
        return PIC_TYPE_EQUALDEF;
    }
    if (len < 32) {
        return PIC_TYPE_UNKNOWN;
    }
    if (fmt_pic_pcx_detect(data, len)) {
        return PIC_TYPE_PCX;
    }
    return PIC_TYPE_UNKNOWN;
}

static bool fmt_pic_decode(struct pic_s *pic)
{
    pic_type_t type;
    pic->type = PIC_TYPE_UNKNOWN;
    type = fmt_pic_detect(pic->coded, pic->len);
    if ((type == PIC_TYPE_PCX) && fmt_pic_pcx_decode(pic)) {
        pic->type = PIC_TYPE_PCX;
        return true;
    } else  if ((type == PIC_TYPE_EQUALDEF) && fmt_pic_equaldef_decode(pic)) {
        pic->type = PIC_TYPE_EQUALDEF;
        return true;
    }
    return false;
}

/* -------------------------------------------------------------------------- */

bool fmt_pic_load(const char *filename, struct pic_s *pic)
{
#define BUFSIZE (128 + 320 * 200 * 2 + 3 * 256 + 200)
    FILE *fd = 0;
    uint8_t *buf;
    bool res = false;
    int len, l;
    if (filename[0] == '=') {
        len = 0;
        buf = (uint8_t *)lib_stralloc(filename);
        goto ready;
    }
    buf = lib_malloc(128);
    if (0
      || ((fd = fopen(filename, "rb")) == 0)
      || ((len = fread(buf, 1, 128, fd)) < 32)
    ) {
        log_error("loading '%s'\n", filename);
        goto fail;
    }
    if (!fmt_pic_detect(buf, len)) {
        log_error("unsupported picture format in '%s'\n", filename);
        goto fail;
    }
    buf = lib_realloc(buf, BUFSIZE);
    if ((l = fread(&buf[len], 1, BUFSIZE - 128, fd)) < 1) {
        log_error("loading '%s'\n", filename);
        goto fail;
    }
    if (l == (BUFSIZE - 128)) {
        log_error("suspiciosly large picture file '%s'\n", filename);
        goto fail;
    }
    len += l;
ready:
    pic->len = len;
    pic->coded = buf;
    if (!fmt_pic_decode(pic)) {
        log_error("problem decoding '%s'\n", filename);
        goto fail;
    }
    res = true;
fail:
    lib_free(buf);
    buf = NULL;
    pic->coded = NULL;
    if (fd) {
        fclose(fd);
        fd = NULL;
    }
    return res;
#undef BUFSIZE
}

bool fmt_pic_save(const char *filename, struct pic_s *pic)
{
    int res;
    if (pic->type == PIC_TYPE_PCX) {
        if (!fmt_pic_pcx_encode(pic)) {
            log_error("problem encoding '%s'\n", filename);
            return false;
        }
    } else {
        log_error("no type set for '%s'\n", filename);
        return false;
    }
    res = util_file_save(filename, pic->coded, pic->len);
    if (res != 0) {
        log_error("problem saving '%s'\n", filename);
    }
    lib_free(pic->coded);
    pic->coded = NULL;
    return (res == 0);
}

void fmt_pic_free(struct pic_s *pic)
{
    if (pic) {
        lib_free(pic->pix);
        pic->pix = NULL;
        lib_free(pic->pal);
        pic->pal = NULL;
        lib_free(pic->coded);
        pic->coded = NULL;
    }
}
