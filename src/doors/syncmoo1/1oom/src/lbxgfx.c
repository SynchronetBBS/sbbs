#include "config.h"

#include <string.h>

#include "lbxgfx.h"
#include "comp.h"
#include "hw.h"
#include "gfxscale.h"
#include "lbxpal.h"
#include "lib.h"
#include "log.h"
#include "types.h"

/* -------------------------------------------------------------------------- */

static void lbxgfx_draw_pixels_fmt0(uint8_t *pixbuf, uint16_t w, uint8_t *data, uint16_t pitch)
{
    uint8_t *q;
    uint8_t b, /*dh*/mode, /*dl*/len_total, len_run;
    while (w--) {
        q = pixbuf++;
        b = *data++;
        if (b == 0xff) { /* skip column */
            continue;
        }
        mode = b;
        len_total = *data++;
        if ((mode & 0x80) == 0) { /* regular data */
            do {
                len_run = *data++;
                q += pitch * *data++;
                len_total -= len_run + 2;
                do {
                    *q = *data++;
                    q += pitch;
                } while (--len_run);
            } while (len_total >= 1);
        } else {    /* compressed data */
            do {
                len_run = *data++;
                q += pitch * *data++;
                len_total -= len_run + 2;
                do {
                    b = *data++;
                    if (b > 0xdf) { /* b-0xdf pixels, same color */
                        uint8_t len_compr;
                        len_compr = b - 0xdf;
                        --len_run;
                        b = *data++;
                        while (len_compr) {
                            *q = b;
                            q += pitch;
                            --len_compr;
                        }
                    } else {
                        *q = b;
                        q += pitch;
                    }
                } while (--len_run);
            } while (len_total >= 1);
        }
    }
}

static void lbxgfx_draw_pixels_fmt0_scale(uint8_t *pixbuf, uint16_t w, uint8_t *data, uint16_t pitch, int scale)
{
    uint8_t *q;
    uint8_t b, /*dh*/mode, /*dl*/len_total, len_run;
    while (w--) {
        q = pixbuf;
        pixbuf += scale;
        b = *data++;
        if (b == 0xff) { /* skip column */
            continue;
        }
        mode = b;
        len_total = *data++;
        if ((mode & 0x80) == 0) { /* regular data */
            do {
                len_run = *data++;
                q += pitch * scale * *data++;
                len_total -= len_run + 2;
                do {
                    b = *data++;
                    q = gfxscale_draw_pixel(q, b, pitch, scale);
                } while (--len_run);
            } while (len_total >= 1);
        } else {    /* compressed data */
            do {
                len_run = *data++;
                q += pitch * scale * *data++;
                len_total -= len_run + 2;
                do {
                    b = *data++;
                    if (b > 0xdf) { /* b-0xdf pixels, same color */
                        uint8_t len_compr;
                        len_compr = b - 0xdf;
                        --len_run;
                        b = *data++;
                        while (len_compr) {
                            q = gfxscale_draw_pixel(q, b, pitch, scale);
                            --len_compr;
                        }
                    } else {
                        q = gfxscale_draw_pixel(q, b, pitch, scale);
                    }
                } while (--len_run);
            } while (len_total >= 1);
        }
    }
}

static void lbxgfx_draw_pixels_offs_fmt0(int x0, int y0, int w, int h, int xskip, int yskip, uint8_t *data, uint16_t pitch)
{
    /* FIXME this an unreadable goto mess */
    uint8_t *p = hw_video_get_buf() + y0 * pitch + x0;
    uint8_t *q;
    uint8_t b, mode, len_total, len_run;
    int ylen;
    /* skip columns */
    while (xskip--) {
        b = *data++;
        if (b != 0xff) {
            b = *data++;
            data += b;
        }
    }
    while (w--) {
        q = p++;
        b = *data++;
        if (b == 0xff) { /* skip column */
            continue;
        }
        mode = b;
        len_total = *data++;
        if ((mode & 0x80) == 0) { /* regular data */
            ylen = yskip;
            loc_10dcf:
            if (ylen != 0) {
                ylen -= data[1];
                if (ylen > 0) {
                    len_run = *data++;
                    ylen -= len_run;
                    if (ylen >= 0) {
                        data += len_run + 1;
                        len_total -= len_run + 2;
                        if (len_total != 0) {
                            goto loc_10dcf;
                        } else {
                            /*10e18*/
                            continue; /*goto loc_10e54*/
                        }
                    } else {
                        /*goto loc_10e83;*/
                        ++data;
                        len_total -= len_run + 2;
                        data += ylen + len_run;
                        len_run = -ylen;
                        ylen = h;
                        goto loc_10e46;
                    }
                } else {
                    /*goto loc_10e78;*/
                    len_run = -ylen;
                    ylen = h;
                    goto loc_10e25;
                }
            }
            /*10e1b */
            ylen = h;
            do {
                /*loc_10e20:*/
                len_run = data[1]; /*really skip pixels...*/
                loc_10e25:
                ylen -= len_run;
                q += len_run * pitch;
                len_run = *data++;
                ++data;
                len_total -= len_run + 2;
                loc_10e46:
                do {
                    if (--ylen >= 0) {
                        *q = *data++;
                        q += pitch;
                    } else {
                        data += len_run;
                        break;
                    }
                } while (--len_run);
            } while (len_total >= 1);
        } else {    /* compressed data */
            uint8_t len_compr;
            /*10e97*/
            ylen = yskip;
            loc_10ea2:
            if (ylen != 0) {
                ylen -= data[1];
                if (ylen > 0) {
                    len_run = *data++;
                    ++data;
                    len_total -= len_run + 2;
                    /*loc_10eb6:*/
                    do {
                        b = *data++;
                        if (b <= 0xdf) {
                            if (--ylen >= 0) {
                                /*continue;*/
                            } else {
                                /*goto 10ec7*/
                                len_run += ylen;
                                ylen = h - 1;
                                ++len_run;
                                goto loc_10f38;
                            }
                        } else {
                            /*10ed3*/
                            ++data;
                            ylen -= (b - 0xdf);
                            if (ylen >= 0) {
                                --len_run;
                                /*continue;*/
                            } else {
                                /*goto loc_10ef6;*/
                                --data;
                                b = *data++;
                                --len_run;
                                len_compr = -ylen;
                                ylen = h;
                                goto loc_10f51;
                            }
                        }
                    } while (--len_run);
                    if (len_total >= 1) {
                        goto loc_10ea2;
                    } else {
                        goto loc_10e54;
                    }
                } else {
                    /*goto loc_10eea;*/
                    len_run = -ylen;
                    ylen = h;
                    goto loc_10f0f;
                }
            }
            /*loc_10f05:*/
            ylen = h;
            do {
                /*loc_10f0a:*/
                len_run = data[1]; /*really skip pixels...*/
                loc_10f0f:
                ylen -= len_run;
                q += len_run * pitch;
                len_run = *data++;
                ++data;
                len_total -= len_run + 2;
                /*loc_10f30:*/
                do {
                    if (--ylen >= 0) {
                        b = *data++;
                        if (b <= 0xdf) {
                            loc_10f38:
                            *q = b;
                            q += pitch;
                        } else {
                            len_compr = b - 0xdf;
                            b = *data++;
                            --len_run;
                            ++ylen;
                            loc_10f51:
                            do {
                                if (--ylen >= 0) {
                                    *q = b;
                                    q += pitch;
                                } else {
                                    --len_run;
                                    goto loc_10f7e;
                                }
                            } while (--len_compr);
                        }
                    } else {
                        loc_10f7e:
                        data += len_run;
                        break;
                    }
                } while (--len_run);
            } while (len_total >= 1);
        }
        loc_10e54:
        ;
    }
}

static void lbxgfx_draw_pixels_offs_fmt0_scale(int x0, int y0, int w, int h, int xskip, int yskip, uint8_t *data, uint16_t pitch, int scale)
{
    /* FIXME this an unreadable goto mess */
    uint8_t *p = hw_video_get_buf() + (y0 * pitch + x0) * scale;
    uint8_t *q;
    uint8_t b, mode, len_total, len_run;
    int ylen;
    /* skip columns */
    while (xskip--) {
        b = *data++;
        if (b != 0xff) {
            b = *data++;
            data += b;
        }
    }
    while (w--) {
        q = p;
        p += scale;
        b = *data++;
        if (b == 0xff) { /* skip column */
            continue;
        }
        mode = b;
        len_total = *data++;
        if ((mode & 0x80) == 0) { /* regular data */
            ylen = yskip;
            locs_10dcf:
            if (ylen != 0) {
                ylen -= data[1];
                if (ylen > 0) {
                    len_run = *data++;
                    ylen -= len_run;
                    if (ylen >= 0) {
                        data += len_run + 1;
                        len_total -= len_run + 2;
                        if (len_total != 0) {
                            goto locs_10dcf;
                        } else {
                            continue;
                        }
                    } else {
                        ++data;
                        len_total -= len_run + 2;
                        data += ylen + len_run;
                        len_run = -ylen;
                        ylen = h;
                        goto locs_10e46;
                    }
                } else {
                    len_run = -ylen;
                    ylen = h;
                    goto locs_10e25;
                }
            }
            ylen = h;
            do {
                len_run = data[1]; /*really skip pixels...*/
                locs_10e25:
                ylen -= len_run;
                q += len_run * pitch * scale;
                len_run = *data++;
                ++data;
                len_total -= len_run + 2;
                locs_10e46:
                do {
                    if (--ylen >= 0) {
                        q = gfxscale_draw_pixel(q, *data++, pitch, scale);
                    } else {
                        data += len_run;
                        break;
                    }
                } while (--len_run);
            } while (len_total >= 1);
        } else {    /* compressed data */
            uint8_t len_compr;
            ylen = yskip;
            locs_10ea2:
            if (ylen != 0) {
                ylen -= data[1];
                if (ylen > 0) {
                    len_run = *data++;
                    ++data;
                    len_total -= len_run + 2;
                    do {
                        b = *data++;
                        if (b <= 0xdf) {
                            if (--ylen >= 0) {
                                /*continue;*/
                            } else {
                                len_run += ylen;
                                ylen = h - 1;
                                ++len_run;
                                goto locs_10f38;
                            }
                        } else {
                            ++data;
                            ylen -= (b - 0xdf);
                            if (ylen >= 0) {
                                --len_run;
                                /*continue;*/
                            } else {
                                --data;
                                b = *data++;
                                --len_run;
                                len_compr = -ylen;
                                ylen = h;
                                goto locs_10f51;
                            }
                        }
                    } while (--len_run);
                    if (len_total >= 1) {
                        goto locs_10ea2;
                    } else {
                        goto locs_10e54;
                    }
                } else {
                    len_run = -ylen;
                    ylen = h;
                    goto locs_10f0f;
                }
            }
            ylen = h;
            do {
                len_run = data[1]; /*really skip pixels...*/
                locs_10f0f:
                ylen -= len_run;
                q += len_run * pitch * scale;
                len_run = *data++;
                ++data;
                len_total -= len_run + 2;
                do {
                    if (--ylen >= 0) {
                        b = *data++;
                        if (b <= 0xdf) {
                            locs_10f38:
                            q = gfxscale_draw_pixel(q, b, pitch, scale);
                        } else {
                            len_compr = b - 0xdf;
                            b = *data++;
                            --len_run;
                            ++ylen;
                            locs_10f51:
                            do {
                                if (--ylen >= 0) {
                                    q = gfxscale_draw_pixel(q, b, pitch, scale);
                                } else {
                                    --len_run;
                                    goto locs_10f7e;
                                }
                            } while (--len_compr);
                        }
                    } else {
                        locs_10f7e:
                        data += len_run;
                        break;
                    }
                } while (--len_run);
            } while (len_total >= 1);
        }
        locs_10e54:
        ;
    }
}

static void lbxgfx_draw_pixels_fmt1(uint8_t *pixbuf, uint16_t w, uint8_t *data, uint16_t pitch)
{
    uint8_t *q;
    uint8_t b, /*dh*/mode, /*dl*/len_total, len_run;
    while (w--) {
        q = pixbuf++;
        b = *data++;
        if (b == 0xff) { /* skip column */
            continue;
        }
        mode = b;
        len_total = *data++;
        if ((mode & 0x80) == 0) { /* regular(ish) data */
            do {
                len_run = *data++;
                q += pitch * *data++;
                len_total -= len_run + 2;
                do {
                    /*loc_df66:*/
                    b = *data++;
                    if (b >= 0xe8) {
                        /*goto locs_df9c;*/
                        b -= 0xe8;
                        b = lbxpal_colortable[b][*q];
                    }
                    *q = b;
                    q += pitch;
                } while (--len_run);
            } while (len_total >= 1);
        } else {    /* compressed data */
            do {
                len_run = *data++;
                q += pitch * *data++;
                len_total -= len_run + 2;
                do {
                    b = *data++;
                    if (b > 0xdf) { /* b-0xdf pixels, same color */
                        uint8_t len_compr;
                        len_compr = b - 0xdf;
                        --len_run;
                        b = *data++;
                        if (b >= 0xe8) {
                            uint8_t *tbl;
                            b -= 0xe8;
                            tbl = lbxpal_colortable[b];
                            while (len_compr) {
                                b = tbl[*q];
                                *q = b;
                                q += pitch;
                                --len_compr;
                            }
                        } else {
                            while (len_compr) {
                                *q = b;
                                q += pitch;
                                --len_compr;
                            }
                        }
                    } else {
                        /* here is a test for b >= 0xe8 which is always false */
                        *q = b;
                        q += pitch;
                    }
                } while (--len_run);
            } while (len_total >= 1);
        }
    }
}

static void lbxgfx_draw_pixels_fmt1_scale(uint8_t *pixbuf, uint16_t w, uint8_t *data, uint16_t pitch, int scale)
{
    uint8_t *q;
    uint8_t b, /*dh*/mode, /*dl*/len_total, len_run;
    while (w--) {
        q = pixbuf;
        pixbuf += scale;
        b = *data++;
        if (b == 0xff) { /* skip column */
            continue;
        }
        mode = b;
        len_total = *data++;
        if ((mode & 0x80) == 0) { /* regular(ish) data */
            do {
                len_run = *data++;
                q += pitch * scale * *data++;
                len_total -= len_run + 2;
                do {
                    b = *data++;
                    if (b >= 0xe8) {
                        uint8_t *tbl;
                        b -= 0xe8;
                        tbl = lbxpal_colortable[b];
                        for (int sy = 0; sy < scale; ++sy) {
                            for (int sx = 0; sx < scale; ++sx) {
                                q[sx] = tbl[q[sx]];
                            }
                            q += pitch;
                        }
                    } else {
                        q = gfxscale_draw_pixel(q, b, pitch, scale);
                    }
                } while (--len_run);
            } while (len_total >= 1);
        } else {    /* compressed data */
            do {
                len_run = *data++;
                q += pitch * scale * *data++;
                len_total -= len_run + 2;
                do {
                    b = *data++;
                    if (b > 0xdf) { /* b-0xdf pixels, same color */
                        uint8_t len_compr;
                        len_compr = b - 0xdf;
                        --len_run;
                        b = *data++;
                        if (b >= 0xe8) {
                            uint8_t *tbl;
                            b -= 0xe8;
                            tbl = lbxpal_colortable[b];
                            while (len_compr) {
                                for (int sy = 0; sy < scale; ++sy) {
                                    for (int sx = 0; sx < scale; ++sx) {
                                        q[sx] = tbl[q[sx]];
                                    }
                                    q += pitch;
                                }
                                --len_compr;
                            }
                        } else {
                            while (len_compr) {
                                q = gfxscale_draw_pixel(q, b, pitch, scale);
                                --len_compr;
                            }
                        }
                    } else {
                        /* here is a test for b >= 0xe8 which is always false */
                        q = gfxscale_draw_pixel(q, b, pitch, scale);
                    }
                } while (--len_run);
            } while (len_total >= 1);
        }
    }
}

/* -------------------------------------------------------------------------- */

void lbxgfx_draw_frame_do(uint8_t *p, uint8_t *data, uint16_t pitch, int scale)
{
    uint16_t frame, next_frame, w;
    uint8_t *frameptr;
    w = lbxgfx_get_w(data);
    frame = lbxgfx_get_frame(data);
    frameptr = lbxgfx_get_frameptr(data, frame) + 1;
    if (lbxgfx_get_format(data) == 0) {
        if (scale == 1) {
            lbxgfx_draw_pixels_fmt0(p, w, frameptr, pitch);
        } else {
            lbxgfx_draw_pixels_fmt0_scale(p, w, frameptr, pitch, scale);
        }
    } else {
        if (scale == 1) {
            lbxgfx_draw_pixels_fmt1(p, w, frameptr, pitch);
        } else {
            lbxgfx_draw_pixels_fmt1_scale(p, w, frameptr, pitch, scale);
        }
    }
    next_frame = frame + 1;
    lbxgfx_set_frame(data, next_frame);
    if (next_frame >= lbxgfx_get_frames(data)) {
        next_frame = lbxgfx_get_frames2(data);
    }
    lbxgfx_set_frame(data, next_frame);
}

void lbxgfx_draw_frame(int x, int y, uint8_t *data, uint16_t pitch, int scale)
{
    uint8_t *p = hw_video_get_buf() + (y * pitch + x) * scale;
    lbxgfx_draw_frame_do(p, data, pitch, scale);
}

void lbxgfx_draw_frame_pal(int x, int y, uint8_t *data, uint16_t pitch, int scale)
{
    uint8_t *p;
    uint16_t frame;
    frame = lbxgfx_get_frame(data);
    if (frame == 0) {
        lbxgfx_apply_palette(data);
    }
    p = hw_video_get_buf() + (y * pitch + x) * scale;
    lbxgfx_draw_frame_do(p, data, pitch, scale);
}

void lbxgfx_draw_frame_offs_delay(int x, int y, bool next, uint8_t *data, int lx0, int ly0, int lx1, int ly1, uint16_t pitch, int scale)
{
    int xskip, yskip, x0, y0, x1, y1, w, h;

    if ((x > lx1) || (y > ly1)) {
        return;
    }
    x1 = x + lbxgfx_get_w(data) - 1;
    if (x1 < lx0) {
        return;
    }
    y1 = y + lbxgfx_get_h(data) - 1;
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

    if (lbxgfx_get_epage(data) == 0) {
        lbxgfx_set_epage(data, 1);
        lbxgfx_apply_palette(data);
    }

    uint16_t frame;
    frame = lbxgfx_get_frame(data);

    if (lbxgfx_get_format(data) == 0) {
        if (scale == 1) {
            lbxgfx_draw_pixels_offs_fmt0(x0, y0, w, h, xskip, yskip, lbxgfx_get_frameptr(data, frame) + 1, pitch);
        } else {
            lbxgfx_draw_pixels_offs_fmt0_scale(x0, y0, w, h, xskip, yskip, lbxgfx_get_frameptr(data, frame) + 1, pitch, scale);
        }
    } else {
        log_fatal_and_die("%s: offs_fmt1 unimpl\n", __func__);
    }

    if (next) {
        ++frame;
        if (frame >= lbxgfx_get_frames(data)) {
            frame = lbxgfx_get_frames2(data);
        }
        lbxgfx_set_frame(data, frame);
    }
}

void lbxgfx_draw_frame_offs(int x, int y, uint8_t *data, int lx0, int ly0, int lx1, int ly1, uint16_t pitch, int scale)
{
    lbxgfx_draw_frame_offs_delay(x, y, 1, data, lx0, ly0, lx1, ly1, pitch, scale);
}

void lbxgfx_set_new_frame(uint8_t *data, uint16_t newframe)
{
    uint16_t frames = lbxgfx_get_frames(data);
    if (newframe >= frames) {
        uint16_t frames2 = lbxgfx_get_frames2(data);
        newframe = frames2 + (newframe - frames) % (frames - frames2);
    }
    lbxgfx_set_frame(data, newframe);
}

void lbxgfx_apply_colortable(int x0, int y0, int x1, int y1, uint8_t ctbli, uint16_t pitch, int scale)
{
    uint8_t *pixbuf = hw_video_get_buf();
    const uint8_t *tbl = lbxpal_colortable[ctbli];
    x0 *= scale;
    y0 *= scale;
    x1 = x1 * scale + scale - 1;
    y1 = y1 * scale + scale - 1;
    for (int y = y0; y <= y1; ++y) {
        for (int x = x0; x <= x1; ++x) {
            pixbuf[y * pitch + x] = tbl[pixbuf[y * pitch + x]];
        }
    }
}

void lbxgfx_apply_palette(uint8_t *data)
{
    if (lbxgfx_has_palette(data)) {
        uint8_t *p = lbxgfx_get_palptr(data);
        int first = lbxgfx_get_palfirst(data);
        int num = lbxgfx_get_palnum(data);
        lbxpal_set_palette(p, first, num);
    }
}
