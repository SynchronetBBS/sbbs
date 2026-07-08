#include "config.h"

#include <string.h>

#include "gfxaux.h"
#include "comp.h"
#include "hw.h"
#include "gfxscale.h"
#include "lbxgfx.h"
#include "lbxpal.h"
#include "lib.h"
#include "log.h"
#include "rnd.h"
#include "types.h"
#include "util_math.h"

/* -------------------------------------------------------------------------- */

static const uint8_t tbl_cloak[0x100] = {
    0x2a, 0x44, 0x23, 0x01, 0x46, 0x19, 0x4f, 0x3b, 0x3f, 0x41, 0x06, 0x2e, 0x52, 0x1c, 0x3e, 0x5c,
    0x60, 0x2b, 0x1c, 0x25, 0x5c, 0x05, 0x03, 0x36, 0x5d, 0x53, 0x16, 0x11, 0x13, 0x60, 0x30, 0x1b,
    0x48, 0x27, 0x46, 0x0d, 0x44, 0x64, 0x24, 0x5f, 0x04, 0x0c, 0x17, 0x22, 0x4a, 0x41, 0x2a, 0x0c,
    0x36, 0x45, 0x30, 0x2d, 0x3f, 0x3a, 0x26, 0x3c, 0x18, 0x2a, 0x1e, 0x4f, 0x11, 0x24, 0x5b, 0x2b,
    0x59, 0x07, 0x29, 0x2b, 0x41, 0x31, 0x2f, 0x06, 0x5b, 0x1e, 0x47, 0x33, 0x07, 0x02, 0x5e, 0x31,
    0x1e, 0x18, 0x55, 0x37, 0x39, 0x29, 0x43, 0x4d, 0x20, 0x09, 0x2d, 0x28, 0x1b, 0x18, 0x26, 0x27,
    0x13, 0x53, 0x1e, 0x2a, 0x22, 0x10, 0x28, 0x3b, 0x05, 0x1f, 0x4e, 0x07, 0x4a, 0x57, 0x16, 0x2e,
    0x19, 0x49, 0x47, 0x1e, 0x4e, 0x4a, 0x62, 0x0d, 0x57, 0x5b, 0x3e, 0x25, 0x38, 0x44, 0x38, 0x4b,
    0x20, 0x35, 0x33, 0x33, 0x2a, 0x19, 0x43, 0x1f, 0x08, 0x5c, 0x08, 0x26, 0x3a, 0x58, 0x36, 0x54,
    0x2e, 0x0a, 0x0a, 0x3b, 0x16, 0x59, 0x17, 0x2f, 0x07, 0x1f, 0x0e, 0x45, 0x01, 0x5c, 0x3f, 0x38,
    0x0b, 0x3c, 0x19, 0x26, 0x31, 0x54, 0x60, 0x2a, 0x03, 0x33, 0x5c, 0x25, 0x4b, 0x15, 0x61, 0x16,
    0x31, 0x64, 0x45, 0x55, 0x52, 0x23, 0x36, 0x64, 0x13, 0x27, 0x01, 0x59, 0x1c, 0x44, 0x1d, 0x5e,
    0x31, 0x54, 0x08, 0x16, 0x0b, 0x12, 0x0e, 0x0f, 0x0a, 0x11, 0x24, 0x34, 0x01, 0x32, 0x14, 0x39,
    0x63, 0x04, 0x19, 0x09, 0x2d, 0x0a, 0x5a, 0x03, 0x60, 0x56, 0x5e, 0x2c, 0x18, 0x58, 0x0f, 0x04,
    0x31, 0x01, 0x3b, 0x13, 0x51, 0x61, 0x63, 0x52, 0x5a, 0x63, 0x0a, 0x3a, 0x49, 0x17, 0x27, 0x5d,
    0x27, 0x50, 0x5b, 0x3a, 0x3b, 0x5c, 0x10, 0x59, 0x39, 0x0c, 0x03, 0x23, 0x49, 0x38, 0x1d, 0x2f
};

/* -------------------------------------------------------------------------- */

static void gfx_aux_scale_up(struct gfx_aux_s *aux, int xscale, int yscale)
{
    uint8_t *p, *q, *q2;
    int w = aux->w, h = aux->h, oldw = w, posold = w * h - 1;
    unsigned int xinc, yinc, yacc = 0;
    w = (w * xscale) / 100;
    h = (h * yscale) / 100;
    if ((w * h) > aux->size) {
        LOG_DEBUG((1, "%s: realloc aux %ix%i == %i > %i (current %ix%i)\n", __func__, w, h, w * h, aux->size, aux->w, aux->h));
        aux->data = lib_realloc(aux->data, w * h);
    }
    aux->w = w;
    aux->h = h;
    p = &(aux->data[w * h - 1]);
    q = q2 = &(aux->data[posold]);
    xinc = (100 << 8) / xscale;
    yinc = (100 << 8) / yscale;
    for (int y = 0; y < h; ++y) {
        unsigned int xacc;
        xacc = 0;
        q = q2;
        for (int x = 0; x < w; ++x) {
            *p-- = *q;
            xacc += xinc;
            q -= (xacc >> 8);
            xacc &= 0xff;
        }
        yacc += yinc;
        q2 -= (yacc >> 8) * oldw;
        yacc &= 0xff;
    }
}

static void gfx_aux_scale_down(struct gfx_aux_s *aux, int xscale, int yscale)
{
    uint8_t *p = aux->data, *q = p, *q2 = p;
    int w = aux->w, h = aux->h, oldw = w;
    unsigned int xinc, yinc, yacc = 0;
    aux->w = w = (w * xscale) / 100;
    aux->h = h = (h * yscale) / 100;
    xinc = (100 << 8) / xscale;
    yinc = (100 << 8) / yscale;
    for (int y = 0; y < h; ++y) {
        unsigned int xacc;
        xacc = 0;
        q = q2;
        for (int x = 0; x < w; ++x) {
            *p++ = *q;
            xacc += xinc;
            q += (xacc >> 8);
            xacc &= 0xff;
        }
        yacc += yinc;
        q2 += (yacc >> 8) * oldw;
        yacc &= 0xff;
    }
}

static void gfx_aux_overlay_do_normal(uint8_t *dest, const uint8_t *src, int w, int h, int destskipw, int srcskipw)
{
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            uint8_t c;
            c = *src++;
            if (c != 0) {
                *dest++ = c;
            } else {
                ++dest;
            }
        }
        src += srcskipw;
        dest += destskipw;
    }
}

static void gfx_aux_overlay_do_clear(uint8_t *dest, const uint8_t *src, int w, int h, int destskipw, int srcskipw)
{
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            uint8_t c;
            c = *src++;
            if (c == 0) {
                *dest++ = c;
            } else {
                ++dest;
            }
        }
        src += srcskipw;
        dest += destskipw;
    }
}

static void gfx_aux_overlay_do(int x, int y, struct gfx_aux_s *dest, struct gfx_aux_s *src, void (*fptr)(uint8_t *dest, const uint8_t *src, int w, int h, int destskipw, int srcskipw))
{
    int destw, desth, srcx1, srcy1, xskip, x0, yskip, y0, w, h, destskip0, srcskip0, destskipw, srcskipw;
    destw = dest->w;
    if (destw <= x) {
        return;
    }
    desth = dest->h;
    if (desth <= y) {
        return;
    }
    srcx1 = src->w + x - 1;
    if (srcx1 < 0) {
        return;
    }
    srcy1 = src->h + y - 1;
    if (srcy1 < 0) {
        return;
    }
    if (x >= 0) {
        xskip = 0;
        x0 = x;
    } else {
        xskip = -x;
        x0 = 0;
    }
    if (y >= 0) {
        yskip = 0;
        y0 = y;
    } else {
        yskip = -y;
        y0 = 0;
    }
    if ((destw - 1) > srcx1) {
        w = srcx1 - x0 + 1;
    } else {
        w = destw - 1 - x0 + 1;
    }
    SETMIN(w, destw);
    if ((desth - 1) > srcy1) {
        h = srcy1 - y0 + 1;
    } else {
        h = desth - 1 - y0 + 1;
    }
    SETMIN(h, desth);
    destskip0 = destw * y0 + x0;
    destskipw = destw - w;
    srcskip0 = src->h * yskip + xskip;
    srcskipw = src->w - w;
    fptr(&(dest->data[destskip0]), &(src->data[srcskip0]), w, h, destskipw, srcskipw);
}

static inline void gfx_aux_draw_frame_1x(const uint8_t *q, uint8_t *p, int w, int h, uint16_t pitch_aux, uint16_t pitch_hw)
{
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            uint8_t b = q[x];
            if (b) {
                p[x] = b;
            }
        }
        p += pitch_hw;
        q += pitch_aux;
    }
}

static inline void gfx_aux_draw_frame_nx(const uint8_t *q, uint8_t *p, int w, int h, uint16_t pitch_aux, uint16_t pitch_hw, int scale)
{
    for (int y = 0; y < h; ++y) {
        uint8_t *o;
        o = p;
        for (int x = 0; x < w; ++x) {
            uint8_t b = q[x];
            if (b) {
                for (int sy = 0; sy < scale; ++sy) {
                    for (int sx = 0; sx < scale; ++sx) {
                        o[sx + sy * pitch_hw] = b;
                    }
                }
            }
            o += scale;
        }
        p += pitch_hw * scale;
        q += pitch_aux;
    }
}

static void gfx_aux_draw_frame_from_limit_do(int x, int y, int w, int h, int xskip, int yskip, struct gfx_aux_s *aux, uint16_t pitch_hw, int scale)
{
    uint8_t *p, *q;
    uint16_t pitch_aux = aux->w;
    p = hw_video_get_buf() + (y * pitch_hw + x) * scale;
    q = aux->data + yskip * pitch_aux + xskip;
    if (scale == 1) {
        gfx_aux_draw_frame_1x(q, p, w, h, pitch_aux, pitch_hw);
    } else {
        gfx_aux_draw_frame_nx(q, p, w, h, pitch_aux, pitch_hw, scale);
    }
}

/* FIXME
    MOO1 stores the aux gfx in columnwise order. The rotate functions below currently do the same.
    This results in the following ugly hack:
        b = gfx[(j % r->srch) * r->srcw + (j / r->srch)];
    This can be optimized to "b = *q" when the rotation is rewritten to regular pixel order.
*/
#define DEBUGLEVEL_ROTATE   5

struct rotate_param_s {
    int destx;
    int desty;
    int w;
    int h;
    int hfrac;
    int destxstep;
    int destxfrac;
    int destxadd;
    int srcstart;
    /*srcdataseg*/
    int srcxstep;
    int srcxfrac2;
    int srcxadd2;
    int srcxfrac1;
    int srcxadd1;
    int srcystep;
    int srcyfrac2;
    int srcyadd2;
    int srcyfrac1;
    int srcyadd1;
    int destxskip;
    int destmin;
    int destmax;
    /* only needed for columnwise -> regular */
    int srcw;
    int srch;
};

static void gfx_aux_draw_rotate_sub1(struct rotate_param_s *r, const uint8_t *gfx, uint16_t pitch_hw, int scale)
{
    int destxskip = r->destxskip, edest = 0x80, ex1 = 0x80, ex2 = 0x80, hx100 = r->h * 0x100, w = r->w, di, si;
    int sw = r->srcw, sh = r->srch;
    uint8_t *dest;
    dest = hw_video_get_buf();
    di = (r->desty * pitch_hw + r->destx) * scale;
    si = r->srcstart;
LOG_DEBUG((DEBUGLEVEL_ROTATE, "r: hf:%i S:%i d:%i,%i,0x%x  s:%i y:%i,%i,0x%x,%i,0x%x  x:%i,%i,0x%x,%i,0x%x\n", r->hfrac, r->destxskip, r->destxstep, r->destxadd, r->destxfrac, r->srcstart, r->srcystep, r->srcyadd1, r->srcyfrac1, r->srcyadd2, r->srcyfrac2, r->srcxstep, r->srcxadd1, r->srcxfrac1, r->srcxadd2, r->srcxfrac2));
    while (1) {
        if (destxskip < 0) {
            int loops, esy1, esy2, i, j;
            loops = hx100 / 0x100;
            i = di;
            j = si;
            esy1 = esy2 = 0;
            do {
                uint8_t b;
                b = gfx[(j % sh) * sw + (j / sh)];   /* FIXME HACK columnwise -> regular */
                if (b != 0) {
                    for (int y = 0; y < scale; ++y) {
                        for (int x = 0; x < scale; ++x) {
                            int xi;
                            xi = i + x;
                            if ((xi >= r->destmin) && (xi < r->destmax)) {
                                dest[xi] = b;
                            }
                        }
                        i += pitch_hw;
                    }
                } else {
                    i += pitch_hw * scale;
                }
                j += r->srcystep;
                if ((esy1 += r->srcyfrac1) >= 0x100) {
                    esy1 &= 0xff;
                    j += r->srcyadd1;
                }
                if ((esy2 += r->srcyfrac2) >= 0x100) {
                    esy2 &= 0xff;
                    j += r->srcyadd2;
                }
            } while (--loops);
        }
        --destxskip;
        if (--w <= 0) {
            break;
        }
        di += (r->destxstep + 1 ) * scale;
        if ((edest += r->destxfrac) >= 0x100) {
            edest &= 0xff;
            di += r->destxadd * scale;
        }
        hx100 += r->hfrac;
        si += r->srcxstep;
        if ((ex1 += r->srcxfrac1) >= 0x100) {
            ex1 &= 0xff;
            si += r->srcxadd1;
        }
        if ((ex2 += r->srcxfrac2) >= 0x100) {
            ex2 &= 0xff;
            si += r->srcxadd2;
        }
    }
}

static void gfx_aux_draw_frame_from_rotate_limit_do(int x0, int y0, int x1, int y1, int x2, int y2, int x3, int y3, struct gfx_aux_s *aux, int lx0, int ly0, int lx1, int ly1, uint16_t pitch_hw, int scale)
{
    struct rotate_param_s r;
    int tx[4] = { x0, x1, x2, x3 };
    int ty[4] = { y0, y1, y2, y3 };
    int ti[4] = { 1, 2, 3, 4 };
    int maxy = 0, miny = 10000;   /* WASBUG initial values made the following SET* ineffective */
    int w = aux->w, h = aux->h;
    for (int i = 0; i < 4; ++i) {
        SETMIN(miny, ty[i]);
        SETMAX(maxy, ty[i]);
        for (int j = 0; j < 3; ++j) {
            int t;
            t = tx[j];
            if (t > tx[j + 1]) {
                tx[j] = tx[j + 1]; tx[j + 1] = t;
                t = ty[j]; ty[j] = ty[j + 1]; ty[j + 1] = t;
                t = ti[j]; ti[j] = ti[j + 1]; ti[j + 1] = t;
            }
        }
    }
    if ((tx[0] > lx1) || (tx[3] < lx0) || (miny > ly1) || (maxy < ly0)) {
        return;
    }
    memset(&r, 0, sizeof(r));
    r.srcw = aux->w;
    r.srch = aux->h;
    r.destmin = lx0 * pitch_hw * scale;
    r.destmax = (lx1 + 1) * pitch_hw * scale;
    if (tx[0] == tx[1]) {
        if (ty[0] > ty[1]) {
            int t;
            t = ty[0]; ty[0] = ty[1]; ty[1] = t;
            t = ti[0]; ti[0] = ti[1]; ti[1] = t;
        }
LOG_DEBUG((DEBUGLEVEL_ROTATE, "%s: case %i  y1:%i y2:%i -> %i\n", __func__, ti[0], ty[1], ty[2], ty[1] < ty[2]));
        r.destx = tx[0];
        r.desty = ty[0];
        r.h = ty[1] - ty[0] + 1;
        r.w = tx[2] - tx[0] + 1;
        switch (ti[0]) {
            default:
            case 1:
                r.srcstart = 0;
                r.srcyadd2 = 1;
                r.srcxadd1 = h;
                r.srcxfrac1 = (w << 8) / r.w;
                r.srcyfrac2 = (h << 8) / r.h;
                break;
            case 2:
                r.srcstart = (w - 1) * h;
                r.srcyadd2 = -h;
                r.srcxadd1 = 1;
                r.srcxfrac1 = (h << 8) / r.w;
                r.srcyfrac2 = (w << 8) / r.h;
                break;
            case 3:
                r.srcstart = w * h - 1;
                r.srcyadd2 = -1;
                r.srcxadd1 = -h;
                r.srcxfrac1 = (w << 8) / r.w;
                r.srcyfrac2 = (h << 8) / r.h;
                break;
            case 4:
                r.srcstart = h - 1;
                r.srcyadd2 = h;
                r.srcxadd1 = -1;
                r.srcxfrac1 = (h << 8) / r.w;
                r.srcyfrac2 = (w << 8) / r.h;
                break;
        }
        r.srcxstep = 0;
        if (r.srcxfrac1 >= 0x100) {
            r.srcxstep = (r.srcxfrac1 / 0x100) * r.srcxadd1;
            r.srcxfrac1 = r.srcxfrac1 % 0x100;
        }
        r.srcystep = 0;
        if (r.srcyfrac2 >= 0x100) {
            r.srcystep = (r.srcyfrac2 / 0x100) * r.srcyadd2;
            r.srcyfrac2 = r.srcyfrac2 % 0x100;
        }
        if ((tx[2] < lx0) || (tx[0] > lx1)) {
            return;
        }
        r.destxskip = lx0 - tx[0] - 1;
        if (lx1 < tx[2]) {
            r.w = lx1 - tx[0] + 1;
        }
        r.srcxadd2 = r.srcyadd2;
        r.srcyadd1 = r.srcxadd1;
        gfx_aux_draw_rotate_sub1(&r, aux->data, pitch_hw, scale);
    } else {
        int v4a;
LOG_DEBUG((DEBUGLEVEL_ROTATE, "%s: case %i  y1:%i y2:%i -> %i\n", __func__, ti[0], ty[1], ty[2], ty[1] < ty[2]));
        r.destx = tx[0];
        r.desty = ty[0];
        r.w = tx[1] - tx[0] + 1;
        if (ty[1] < ty[0]) {
            v4a = ((ty[2] - ty[0]) * (tx[1] - tx[0])) / (tx[2] - tx[0]) + ty[0] - ty[1] + 1;
        } else {
            v4a = ((ty[0] - ty[2]) * (tx[1] - tx[0])) / (tx[2] - tx[0]) + ty[1] - ty[0] + 1;
        }
        r.h = 1;
        r.hfrac = ((v4a - r.h) << 8) / (r.w - 1);
        r.destxadd = -pitch_hw;
        if (ty[1] < ty[2]) {
            r.destxfrac = ((ty[0] - ty[1]) << 8) / (tx[1] - tx[0]);
        } else {
            r.destxfrac = ((ty[0] - ty[2]) << 8) / (tx[2] - tx[0]);
        }
        if (r.destxfrac >= 0x100) {
            r.destxstep = (r.destxfrac / 0x100) * -pitch_hw;
            r.destxfrac = r.destxfrac % 0x100;
        } else {
            r.destxstep = 0;
        }
        if ((ti[0] == 1) || (ti[0] == 3)) {
            if (ty[1] < ty[2]) {
                /*21e55*/
                r.srcyfrac1 = ((w - 1) << 8) / (v4a - 1);
                r.srcyfrac2 = (((tx[1] - tx[0]) * ((h - 1) << 8)) / (v4a - 1)) / (tx[2] - tx[0]);
            } else {
                /*21ead*/
                r.srcyfrac1 = (((tx[1] - tx[0]) * ((w - 1) << 8)) / (v4a - 1)) / (tx[2] - tx[0]);
                r.srcyfrac2 = ((h - 1) << 8) / (v4a - 1);
            }
        } else {
            /*21f10*/
            if (ty[1] < ty[2]) {
                r.srcyfrac1 = (((tx[3] - tx[2]) * ((w - 1) << 8)) / (v4a - 1)) / (tx[3] - tx[1]);
                r.srcyfrac2 = ((h - 1) << 8) / (v4a - 1);
            } else {
                r.srcyfrac1 = ((w - 1) << 8) / (v4a - 1);
                r.srcyfrac2 = (((tx[1] - tx[0]) * ((h - 1) << 8)) / (v4a - 1)) / (tx[2] - tx[0]);
            }
        }
        /*21fd0*/
        switch (ti[0]) {
            default:
            case 1:
                r.srcstart = 0;
                r.srcxadd2 = 1;
                r.srcxadd1 = h;
                r.srcyadd2 = 1;
                r.srcyadd1 = -h;
                r.srcxfrac2 = 0;
                r.srcxfrac1 = ((w - 1) << 8) / (((ty[1] < ty[2]) ? tx[1] : tx[2]) - tx[0]);
                break;
            case 2:
                r.srcstart = (w - 1) * h;
                r.srcxadd2 = 1;
                r.srcxadd1 = -h;
                r.srcyadd2 = -1;
                r.srcyadd1 = -h;
                r.srcxfrac1 = 0;
                r.srcxfrac2 = ((h - 1) << 8) / (((ty[1] < ty[2]) ? tx[1] : tx[2]) - tx[0]);
                break;
            case 3:
                r.srcstart = w * h - 1;
                r.srcxadd2 = -1;
                r.srcxadd1 = -h;
                r.srcyadd2 = -1;
                r.srcyadd1 = h;
                r.srcxfrac2 = 0;
                r.srcxfrac1 = ((w - 1) << 8) / (((ty[1] < ty[2]) ? tx[1] : tx[2]) - tx[0]);
                break;
            case 4:
                r.srcstart = h - 1;
                r.srcxadd2 = -1;
                r.srcxadd1 = h;
                r.srcyadd2 = 1;
                r.srcyadd1 = h;
                r.srcxfrac1 = 0;
                r.srcxfrac2 = ((h - 1) << 8) / (((ty[1] < ty[2]) ? tx[1] : tx[2]) - tx[0]);
                break;
        }
        r.srcxstep = 0;
        if (r.srcxfrac2 >= 0x100) {
            r.srcxstep = (r.srcxfrac2 / 0x100) * r.srcxadd2;
            r.srcxfrac2 = r.srcxfrac2 % 0x100;
        }
        if (r.srcxfrac1 >= 0x100) {
            r.srcxstep += (r.srcxfrac1 / 0x100) * r.srcxadd1;
            r.srcxfrac1 = r.srcxfrac1 % 0x100;
        }
        r.srcystep = 0;
        if (r.srcyfrac2 >= 0x100) {
            r.srcystep = (r.srcyfrac2 / 0x100) * r.srcyadd2;
            r.srcyfrac2 = r.srcyfrac2 % 0x100;
        }
        if (r.srcyfrac1 >= 0x100) {
            r.srcystep += (r.srcyfrac1 / 0x100) * r.srcyadd1;
            r.srcyfrac1 = r.srcyfrac1 % 0x100;
        }
        if ((tx[1] >= lx0) && (tx[0] <= lx1)) {
            r.destxskip = lx0 - tx[0] - 1;
            if (lx1 < tx[1]) {
                r.w = lx1 - tx[0] + 1;
            }
            gfx_aux_draw_rotate_sub1(&r, aux->data, pitch_hw, scale);
        }
        /*22297*/
        r.destx = tx[1];
        r.w = tx[2] - tx[1] + 1;
        r.h = v4a;
        r.hfrac = 0;
        if (ty[1] < ty[2]) {
            r.desty = ty[1];
            r.destxadd = pitch_hw;
            r.destxfrac = ((ty[3] - ty[1]) << 8) / (tx[3] - tx[1]);
            if (r.destxfrac >= 0x100) {
                r.destxstep = (r.destxfrac / 0x100) * pitch_hw;
                r.destxfrac = r.destxfrac % 0x100;
            } else {
                r.destxstep = 0;
            }
        } else {
            /*22309*/
            r.desty = ty[0] - ((ty[0] - ty[2]) * (tx[1] - tx[0])) / (tx[2] - tx[0]);
        }
        /*22329*/
        switch (ti[0]) {
            default:
            case 1:
                if (ty[1] < ty[2]) {
                    r.srcstart = (w - 1) * h;
                    r.srcxfrac1 = 0;
                    r.srcxfrac2 = ((h - 1) << 8) / (tx[3] - tx[1]);
                } else {
                    r.srcstart = h * ((((w - 1) * (tx[1] - tx[0])) / (tx[2] - tx[0])));
                }
                break;
            case 2:
                if (ty[1] < ty[2]) {
                    r.srcstart = w * h - 1;
                    r.srcxfrac1 = ((w - 1) << 8) / (tx[3] - tx[1]);
                    r.srcxfrac2 = 0;
                } else {
                    r.srcstart = (w - 1) * h + (((h - 1) * (tx[1] - tx[0])) / (tx[2] - tx[0]));
                }
                break;
            case 3:
                if (ty[1] < ty[2]) {
                    r.srcstart = h - 1;
                    r.srcxfrac1 = 0;
                    r.srcxfrac2 = ((h - 1) << 8) / (tx[3] - tx[1]);
                } else {
                    r.srcstart = w * h - 1 - h * (((w - 1) * (tx[1] - tx[0])) / (tx[2] - tx[0]));
                }
                break;
            case 4:
                if (ty[1] < ty[2]) {
                    r.srcstart = 0;
                    r.srcxfrac1 = ((w - 1) << 8) / (tx[3] - tx[1]);
                    r.srcxfrac2 = 0;
                } else {
                    r.srcstart = h - 1 - (((h - 1) * (tx[1] - tx[0])) / (tx[2] - tx[0]));
                }
                break;
        }
        /*224a0*/
        if (ty[1] < ty[2]) {
            r.srcxstep = 0;
            if (r.srcxfrac2 >= 0x100) {
                r.srcxstep = (r.srcxfrac2 / 0x100) * r.srcxadd2;
                r.srcxfrac2 = r.srcxfrac2 % 0x100;
            }
            if (r.srcxfrac1 >= 0x100) {
                r.srcxstep += (r.srcxfrac1 / 0x100) * r.srcxadd1;
                r.srcxfrac1 = r.srcxfrac1 % 0x100;
            }
        }
        /*224ed*/
        if ((tx[2] >= lx0) && (tx[1] <= lx1)) {
            r.destxskip = lx0 - tx[1] - 1;
            if (lx1 < tx[2]) {
                r.w = lx1 - tx[1] + 1;
            }
            gfx_aux_draw_rotate_sub1(&r, aux->data, pitch_hw, scale);
        }
        /*225da*/
        r.destx = tx[2];
        r.w = tx[3] - tx[2] + 1;
        r.hfrac = -(((v4a - 1) << 8) / (r.w - 1));
        if (ty[1] < ty[2]) {
            r.desty = ty[3] - ((ty[3] - ty[1]) * (tx[3] - tx[2])) / (tx[3] - tx[1]);
        } else {
            r.desty = ty[2];
            r.destxadd = pitch_hw;
            r.destxfrac = ((ty[3] - ty[2]) << 8) / (tx[3] - tx[2]);
            if (r.destxfrac >= 0x100) {
                r.destxstep = (r.destxfrac / 0x100) * pitch_hw;
                r.destxfrac = r.destxfrac % 0x100;
            } else {
                r.destxstep = 0;
            }
        }
        /*22676*/
        switch (ti[0]) {
            default:
            case 1:
                if (ty[1] < ty[2]) {
                    r.srcstart = (w - 1) * h + (((h - 1) * (tx[2] - tx[1])) / (tx[3] - tx[1]));
                } else {
                    r.srcstart = (w - 1) * h;
                    r.srcxfrac1 = 0;
                    r.srcxfrac2 = ((h - 1) << 8) / (tx[3] - tx[2]);
                }
                break;
            case 2:
                if (ty[1] < ty[2]) {
                    r.srcstart = (w * h - 1) - h * (((w - 1) * (tx[2] - tx[1])) / (tx[3] - tx[1]));
                } else {
                    r.srcstart = w * h - 1;
                    r.srcxfrac1 = ((w - 1) << 8) / (tx[3] - tx[2]);
                    r.srcxfrac2 = 0;
                }
                break;
            case 3:
                if (ty[1] < ty[2]) {
                    r.srcstart = h - 1 - (((h - 1) * (tx[2] - tx[1])) / (tx[3] - tx[1]));
                } else {
                    r.srcstart = h - 1;
                    r.srcxfrac1 = 0;
                    r.srcxfrac2 = ((h - 1) << 8) / (tx[3] - tx[2]);
                }
                break;
            case 4:
                if (ty[1] < ty[2]) {
                    r.srcstart = h * (((w - 1) * (tx[2] - tx[1])) / (tx[3] - tx[1]));
                } else {
                    r.srcstart = 0;
                    r.srcxfrac1 = ((w - 1) << 8) / (tx[3] - tx[2]);
                    r.srcxfrac2 = 0;
                }
                break;
        }
        /*227ed*/
        if (ty[1] >= ty[2]) {
            r.srcxstep = 0;
            if (r.srcxfrac2 >= 0x100) {
                r.srcxstep = (r.srcxfrac2 / 0x100) * r.srcxadd2;
                r.srcxfrac2 = r.srcxfrac2 % 0x100;
            }
            if (r.srcxfrac1 >= 0x100) {
                r.srcxstep += (r.srcxfrac1 / 0x100) * r.srcxadd1;
                r.srcxfrac1 = r.srcxfrac1 % 0x100;
            }
        }
        /*2283a*/
        if ((tx[3] >= lx0) && (tx[2] <= lx1)) {
            r.destxskip = lx0 - tx[2] - 1;
            if (lx1 < tx[3]) {
                r.w = lx1 - tx[2] + 1;
            }
            gfx_aux_draw_rotate_sub1(&r, aux->data, pitch_hw, scale);
        }
    }
}

static void gfx_aux_make_paltbl(const struct gfx_aux_s *aux, uint8_t *tbl)
{
    const uint8_t *p = aux->data, *q = lbxpal_palette;;
    int len = aux->w * aux->h;
    memset(tbl, 0xff, 0x100);
    for (int i = 0; i < len; ++i) {
        uint8_t c;
        c = *p++;
        if (c) {
            tbl[c] = 0;
        }
    }
    /* FIXME only != 0xff tested in callers */
    for (int i = 0; i < 0x100; ++i, q += 3) {
        if (tbl[i] != 0xff) {
            tbl[i] = q[0] + q[1] + q[2] / 2;
        }
    }
}

/* -------------------------------------------------------------------------- */

void gfx_aux_setup_wh(struct gfx_aux_s *aux, int w, int h)
{
    int size = w * h;
    aux->w = w;
    aux->h = h;
    if (size > aux->size) {
        lib_free(aux->data);
        aux->data = lib_malloc(size);
        aux->size = size;
    } else {
        memset(aux->data, 0, size);
    }
    aux->frame = 0;
}

void gfx_aux_setup(struct gfx_aux_s *aux, const uint8_t *data, int frame)
{
    int w = lbxgfx_get_w(data);
    int h = lbxgfx_get_h(data);
    gfx_aux_setup_wh(aux, w, h);
    aux->frame = frame;
}

void gfx_aux_free(struct gfx_aux_s *aux)
{
    if (aux) {
        if (aux->data) {
            lib_free(aux->data);
            aux->data = 0;
            aux->size = 0;
        }
    }
}

void gfx_aux_flipx(struct gfx_aux_s *aux)
{
    uint8_t *p = aux->data;
    int w = aux->w;
    for (int y = 0; y < aux->h; ++y) {
        for (int x = 0; x < (w / 2); ++x) {
            uint8_t t;
            t = p[x];
            p[x] = p[w - 1 - x];
            p[w - 1 - x] = t;
        }
        p += w;
    }
}

void gfx_aux_scale(struct gfx_aux_s *aux, int xscale, int yscale)
{
    int w = aux->w, h = aux->h;
    if ((xscale <= 0) || (yscale <= 0) || (((w * xscale) / 100) < 1) || (((h * yscale) / 100) < 1)) {
        gfx_aux_setup_wh(aux, w, h);
    } else {
        int xs = xscale, ys = yscale;
        if ((xs < 100) && (ys > 100)) {
            xs = 100;
        }
        if ((xs > 100) && (ys < 100)) {
            ys = 100;
        }
        if ((xs > 100) || (ys > 100)) {
            gfx_aux_scale_up(aux, xs, ys);
        }
        xs = MIN(xscale, 100);
        ys = MIN(yscale, 100);
        if ((xs < 100) || (ys < 100)) {
            gfx_aux_scale_down(aux, xs, ys);
        }
    }
}

void gfx_aux_color_replace(struct gfx_aux_s *aux, uint8_t from, uint8_t to)
{
    uint8_t *p = aux->data;
    int len = aux->w * aux->h;
    for (int i = 0; i < len; ++i, ++p) {
        if (*p == from) {
            *p = to;
        }
    }
}

void gfx_aux_color_non0(struct gfx_aux_s *aux, uint8_t color)
{
    uint8_t *p = aux->data;
    int len = aux->w * aux->h;
    for (int i = 0; i < len; ++i, ++p) {
        if (*p != 0) {
            *p = color;
        }
    }
}

void gfx_aux_recolor_ctbl(struct gfx_aux_s *aux, const uint8_t *ctbl, int ctbllen)
{
    uint8_t paltbl[0x100], cmax = 0, cmin = 0xff, crange;
    if (ctbllen <= 0) {
        return;
    }
    gfx_aux_make_paltbl(aux, paltbl);
    for (int i = 0; i < 0x100; ++i) {
        uint8_t c;
        c = paltbl[i];
        if (c != 0xff) {
            SETMIN(cmin, c);
            SETMAX(cmax, c);
        }
    }
    crange = cmax - cmin;
    if (crange != 0) {
        for (int i = 0; i < 0x100; ++i) { /* FIXME only the range */
            uint8_t c;
            c = paltbl[i];
            if (c != 0xff) {
                int v;
                v = c + (crange / ctbllen) / 2 - cmin;
                v = (v * (ctbllen - 1)) / crange;
                if ((v >= 0) && (v < ctbllen)) {
                    paltbl[i] = ctbl[v];
                } else {
                    paltbl[i] = 0xff;
                }
            }
        }
    } else {
        /*2336d*/
        for (int i = 0; i < 0x100; ++i) { /* FIXME only the range */
            if (paltbl[i] != 0xff) {
                paltbl[i] = ctbl[0];
            }
        }
    }
    /*23385*/
    for (int i = 0; i < 0x100; ++i) {   /* FIXME only the range */
        uint8_t c;
        c = paltbl[i];
        if (c != 0xff) {
            gfx_aux_color_replace(aux, i, c);
        }
    }
}

void gfx_aux_overlay(int x, int y, struct gfx_aux_s *dest, struct gfx_aux_s *src)
{
    gfx_aux_overlay_do(x, y, dest, src, gfx_aux_overlay_do_normal);
}

void gfx_aux_overlay_clear_unused(int x, int y, struct gfx_aux_s *dest, struct gfx_aux_s *src)
{
    gfx_aux_overlay_do(x, y, dest, src, gfx_aux_overlay_do_clear);
}

void gfx_aux_copy(struct gfx_aux_s *dest, struct gfx_aux_s *src)
{
    int w = src->w, h = src->h;
    int size = w * h;
    if (size > dest->size) {
        lib_free(dest->data);
        dest->data = lib_malloc(size);
        dest->size = size;
    }
    dest->w = w;
    dest->h = h;
    dest->frame = src->frame;
    memcpy(dest->data, src->data, size);
}

void gfx_aux_draw_cloak(struct gfx_aux_s *aux, uint8_t percent, uint16_t rndv)
{
    uint8_t pos, v;
    for (int y = 0; y < aux->h; ++y) {
        rndv = rnd_bitfiddle(rndv);
        pos = (rndv >> 8) & 0xff;
        for (int x = 0; x < aux->w; ++x) {
            v = tbl_cloak[pos++];
            if (v > percent) {
                aux->data[y * aux->w + x] = 0;
            }
        }
    }
}

void gfx_aux_draw_frame_to(uint8_t *data, struct gfx_aux_s *aux)
{
    uint16_t f, frame;
    frame = lbxgfx_get_frame(data);
    if (lbxgfx_get_indep(data)) {
        f = frame;
    } else {
        f = 0;
        lbxgfx_set_frame_0(data);
    }
    if (f == 0) {
        lbxgfx_apply_palette(data);
    }
    gfx_aux_setup(aux, data, frame);
    for (; f <= frame; ++f) {
        lbxgfx_draw_frame_do(aux->data, data, aux->w, 1);
    }
}

void gfx_aux_draw_frame_from(int x, int y, struct gfx_aux_s *aux, uint16_t pitch, int scale)
{
    uint8_t *p, *q;
    p = hw_video_get_buf() + (y * pitch + x) * scale;
    q = aux->data;
    if (scale == 1) {
        gfx_aux_draw_frame_1x(q, p, aux->w, aux->h, aux->w, pitch);
    } else {
        gfx_aux_draw_frame_nx(q, p, aux->w, aux->h, aux->w, pitch, scale);
    }
}

void gfx_aux_draw_frame_from_limit(int x, int y, struct gfx_aux_s *aux, int lx0, int ly0, int lx1, int ly1, uint16_t pitch, int scale)
{
    int xskip, yskip, x0, y0, x1, y1, w, h;

    if ((x > lx1) || (y > ly1)) {
        return;
    }
    x1 = x + aux->w - 1;
    if (x1 < lx0) {
        return;
    }
    y1 = y + aux->h - 1;
    if (y1 < ly0) {
        return;
    }
    if (x >= lx0) {
        xskip = 0;
        x0 = x;
    } else {
        xskip = lx0 - x;
        x0 = lx0;
    }
    if (y >= ly0) {
        yskip = 0;
        y0 = y;
    } else {
        yskip = ly0 - y;
        y0 = ly0;
    }
    w = ((x1 < lx1) ? x1 : lx1) - x0 + 1;
    h = ((y1 < ly1) ? y1 : ly1) - y0 + 1;

    gfx_aux_draw_frame_from_limit_do(x0, y0, w, h, xskip, yskip, aux, pitch, scale);
}

void gfx_aux_draw_frame_from_rotate_limit(int x0, int y0, int x1, int y1, struct gfx_aux_s *aux, int lx0, int ly0, int lx1, int ly1, uint16_t pitch, int scale)
{
    int h = aux->h, angle, angle2, x2, y2, x3, y3, xo, yo, v;
    angle = util_math_calc_angle(x1 - x0, y1 - y0);
    angle2 = 90 - angle;
    if (angle2 < 0) {
        angle2 += 360;
    }
    xo = util_math_angle_dist_cos(angle2, h) / 2;
    yo = util_math_angle_dist_sin(angle2, h) / 2;
    LOG_DEBUG((DEBUGLEVEL_ROTATE, "%s:o %i,%i  %i,%i  %i,%i  %i,%i\n", __func__, x0, y0, x1, y1, xo, yo, angle, angle2));
    x0 += xo;
    y0 -= yo;
    x1 += xo;
    y1 -= yo;
    v = util_math_angle_dist_sin(angle, h * 12) / 10;
    x2 = x1 - v;
    x3 = x0 - v;
    v = util_math_angle_dist_cos(angle, h);
    y2 = y1 + v;
    y3 = y0 + v;
    LOG_DEBUG((DEBUGLEVEL_ROTATE, "%s:t %i,%i  %i,%i  %i,%i  %i,%i\n", __func__, x0, y0, x1, y1, x2, y2, x3, y3));
    gfx_aux_draw_frame_from_rotate_limit_do(x0, y0, x1, y1, x2, y2, x3, y3, aux, lx0, ly0, lx1, ly1, pitch, scale);
}

