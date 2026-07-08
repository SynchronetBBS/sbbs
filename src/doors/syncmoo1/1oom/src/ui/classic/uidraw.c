#include "config.h"

#include <string.h>

#include "uidraw.h"
#include "comp.h"
#include "gfxscale.h"
#include "hw.h"
#include "lbxfont.h"
#include "mouse.h"
#include "rnd.h"
#include "uicursor.h"
#include "uidelay.h"
#include "uidefs.h"
#include "uiobj.h"
#include "uipal.h"

/* -------------------------------------------------------------------------- */

int ui_draw_finish_mode = 0;

const uint8_t tbl_banner_color[BANNER_NUM] = { 0xeb, 0x6f, 0xcd, 0x40, 0x06, 0x50 };
const uint8_t tbl_banner_color2[BANNER_NUM] = { 0xed, 0x71, 0xcd, 0x44, 0xb, 0xa3 };
const uint8_t tbl_banner_fontparam[BANNER_NUM] = { 1, 0xe, 0xc, 5, 0, 0xd };

/* -------------------------------------------------------------------------- */

static const uint8_t colortbl_textbox[5] = { 0x18, 0x17, 0x16, 0x15, 0x14 };

static const uint8_t tbl_color_grain[0x100] = {
    0x29,0x24,0x3f,0x05,0x62,0x6d,0x57,0x2f,0x53,0x11,0x4a,0x72,0x72,0x3c,0x6a,0x6c,
    0x33,0x27,0x5c,0x3d,0x08,0x0d,0x3f,0x1a,0x25,0x5f,0x0e,0x1d,0x07,0x38,0x48,0x5f,
    0x33,0x13,0x4e,0x49,0x44,0x3c,0x0c,0x27,0x20,0x04,0x5b,0x7e,0x0a,0x39,0x26,0x20,
    0x5d,0x55,0x4c,0x7d,0x17,0x76,0x46,0x3c,0x14,0x0e,0x0a,0x0b,0x1d,0x5c,0x2f,0x33,
    0x20,0x1b,0x51,0x6f,0x41,0x79,0x37,0x7e,0x13,0x4a,0x33,0x77,0x1f,0x7e,0x4a,0x5d,
    0x2d,0x50,0x15,0x73,0x45,0x41,0x67,0x51,0x6c,0x45,0x31,0x38,0x33,0x3c,0x22,0x23,
    0x76,0x23,0x12,0x1e,0x62,0x0c,0x20,0x5b,0x31,0x4b,0x1a,0x03,0x3a,0x73,0x1e,0x4a,
    0x2c,0x01,0x7f,0x46,0x1a,0x56,0x6a,0x00,0x33,0x6b,0x4a,0x4d,0x54,0x40,0x68,0x57,
    0x3f,0x15,0x57,0x7f,0x2e,0x5d,0x0f,0x67,0x04,0x70,0x58,0x4a,0x62,0x7f,0x6a,0x10,
    0x61,0x4e,0x52,0x1f,0x1e,0x1d,0x17,0x73,0x73,0x67,0x1e,0x71,0x05,0x50,0x4b,0x78,
    0x02,0x58,0x69,0x3a,0x2d,0x54,0x4c,0x4a,0x13,0x1f,0x34,0x75,0x1f,0x0d,0x75,0x56,
    0x54,0x20,0x55,0x25,0x5a,0x7f,0x36,0x50,0x33,0x23,0x75,0x4d,0x50,0x54,0x11,0x2e,
    0x48,0x54,0x10,0x76,0x67,0x5a,0x1e,0x2b,0x66,0x41,0x78,0x2c,0x79,0x02,0x08,0x45,
    0x0e,0x60,0x51,0x01,0x55,0x62,0x0e,0x3f,0x7c,0x06,0x16,0x08,0x3c,0x34,0x03,0x20,
    0x18,0x71,0x13,0x5b,0x65,0x55,0x4f,0x32,0x06,0x3f,0x6a,0x16,0x79,0x47,0x6b,0x05,
    0x16,0x74,0x0f,0x5a,0x17,0x30,0x68,0x69,0x55,0x78,0x4b,0x4b,0x51,0x58,0x69,0x77
};

/* -------------------------------------------------------------------------- */

static uint8_t ui_draw_box_fill_tbl[0x40];

static void ui_draw_box_fill_sub1(uint16_t num, const uint8_t *colorptr, uint8_t color0)
{
    uint8_t *p = ui_draw_box_fill_tbl;
    int i;

    if (colorptr) {
        for (i = 0; i < num; ++i) {
            *p++ = colorptr[i];
        }
        --i;
        do {
            *p++ = colorptr[i];
            --i;
        } while (--num);
    } else {
        for (i = 0; i < num; ++i) {
            *p++ = color0++;
        }
        do {
            *p++ = --color0;
        } while (--num);
    }
}

static void ui_draw_line_limit_do(int x0, int y0, int x1, int y1, uint8_t color, const uint8_t *colortbl, int colornum, int colorpos, int scale)
{
    if (x0 == x1) {
        if ((x0 < uiobj_minx) || (x0 > uiobj_maxx)) {
            return;
        }
        if (y1 < y0) {
            int t = y0; y0 = y1; y1 = t;
            colorpos = colornum - 1 - colorpos;
        }
        if ((y1 < uiobj_miny) || (y0 > uiobj_maxy)) {
            return;
        }
        SETMAX(y0, uiobj_miny);
        SETMIN(y1, uiobj_maxy);
    } else {
        int dx, dy;
        if (x1 < x0) {
            int t;
            t = x0; x0 = x1; x1 = t;
            t = y0; y0 = y1; y1 = t;
            colorpos = colornum - 1 - colorpos;
        }
        dy = y1 - y0;
        dx = x1 - x0;
        if (x0 < uiobj_minx) {
            y0 += (dy * (uiobj_minx - x0)) / dx;
            x0 = uiobj_minx;
        }
        if (x0 > x1) {
            return;
        }
        if (x1 > uiobj_maxx) {
            y1 = y0 + (dy * (uiobj_maxx - x0)) / dx;
            x1 = uiobj_maxx;
        }
        if (x1 < x0) {
            return;
        }
    }
    if (y0 == y1) {
        if ((y0 < uiobj_miny) || (y0 > uiobj_maxy)) {
            return;
        }
        if (x1 < x0) {
            int t = x0; x0 = x1; x1 = t;
        }
        if ((x1 < uiobj_minx) || (x0 > uiobj_maxx)) {
            return;
        }
        SETMAX(x0, uiobj_minx);
        SETMIN(x1, uiobj_maxx);
    } else {
        int dx, dy;
        if (y1 < y0) {
            int t;
            t = x0; x0 = x1; x1 = t;
            t = y0; y0 = y1; y1 = t;
        }
        dx = x1 - x0;
        dy = y1 - y0;
        if (y0 < uiobj_miny) {
            x0 += (dx * (uiobj_miny - y0)) / dy;
            y0 = uiobj_miny;
        }
        if (y0 > y1) {
            return;
        }
        if (y1 > uiobj_maxy) {
            x1 = x0 + (dx * (uiobj_maxy - y0)) / dy;
            y1 = uiobj_maxy;
        }
        if (y1 < y0) {
            return;
        }
    }
    if (colortbl) {
        ui_draw_line_ctbl(x0, y0, x1, y1, colortbl, colornum, colorpos, scale);
    } else {
        ui_draw_line1(x0, y0, x1, y1, color, scale);
    }
}

static void ui_draw_copy_line(int x0, int y0, int x1, int y1)
{
    int step, numpixels;
    /* ((x0 <= x1) && (y0 <= y1)) && (((x1 - x0) == 0) || ((y1 - y0) == 0)) */
    {
        int dx, dy;
        dx = x1 - x0;
        dy = y1 - y0;
        if (dx < dy) {
            numpixels = dy + 1;
            step = UI_SCREEN_W;
        } else {
            numpixels = dx + 1;
            step = 1;
        }
    }
    {
        uint8_t *q = hw_video_get_buf() + y0 * UI_SCREEN_W + x0;
        uint8_t *p = hw_video_get_buf_front() + y0 * UI_SCREEN_W + x0;
        while (numpixels--) {
            *p = *q;
            p += step;
            q += step;
        }
    }
}

/* -------------------------------------------------------------------------- */

void ui_draw_erase_buf(void)
{
    memset(hw_video_get_buf(), 0, UI_SCREEN_W * UI_SCREEN_H);
}

void ui_draw_copy_buf(void)
{
    hw_video_copy_buf();
}

void ui_draw_color_buf(uint8_t color)
{
    memset(hw_video_get_buf(), color, UI_SCREEN_W * UI_SCREEN_H);
}

void ui_draw_pixel(int x, int y, uint8_t color, int scale)
{
    uint8_t *p = hw_video_get_buf();
    if (scale == 1) {
        p[y * UI_SCREEN_W + x] = color;
    } else {
        gfxscale_draw_pixel(p + (y * UI_SCREEN_W + x) * scale, color, UI_SCREEN_W, scale);
    }
}

void ui_draw_filled_rect(int x0, int y0, int x1, int y1, uint8_t color, int scale)
{
    uint8_t *s = hw_video_get_buf();
    int w;
    if (x1 < x0) {
        return;
    }
    w = (x1 - x0 + 1) * scale;
    x0 *= scale;
    y0 *= scale;
    y1 *= scale;
    y1 += scale - 1;
    s += y0 * UI_SCREEN_W + x0;
    for (; y0 <= y1; ++y0) {
        memset(s, color, w);
        s += UI_SCREEN_W;
    }
}

void ui_draw_line1(int x0, int y0, int x1, int y1, uint8_t color, int scale)
{
    int xslope = 0, yslope = 0, yinc, numpixels;    /* BUG? xslope and yslope not cleared by MOO1 */

    if (x1 < x0) {
        int t;
        t = x1; x1 = x0; x0 = t;
        t = y1; y1 = y0; y0 = t;
    }

    {
        int dx, dy;
        dx = x1 - x0;
        dy = y1 - y0;
        yinc = UI_SCREEN_W * scale;
        if (dy < 0) {
            dy = -dy;
            yinc = -UI_SCREEN_W * scale;
        }
        if (dx < dy) {
            numpixels = dy + 1;
            yslope = 0x100;
            if (dy != 0) {
                xslope = (dx << 8) / dy;
            }
        } else {
            numpixels = dx + 1;
            if (dx != 0) {
                xslope = 0x100;
                yslope = (dy << 8) / dx;
            }
        }
    }

    {
        uint8_t *p = hw_video_get_buf() + (y0 * UI_SCREEN_W + x0) * scale;
        int xerr, yerr;

        xerr = 0x100 / 2;
        yerr = 0x100 / 2;

        while (numpixels--) {
            if (scale == 1) {
                *p = color;
            } else {
                gfxscale_draw_pixel(p, color, UI_SCREEN_W, scale);
            }
            xerr += xslope;
            if ((xerr & 0xff00) != 0) {
                xerr &= 0xff;
                p += scale;
            }
            yerr += yslope;
            if ((yerr & 0xff00) != 0) {
                yerr &= 0xff;
                p += yinc;
            }
        }
    }
}

void ui_draw_line_ctbl(int x0, int y0, int x1, int y1, const uint8_t *colortbl, int colornum, int pos, int scale)
{
    int xslope = 0, yslope = 0, yinc, numpixels;    /* BUG? xslope and yslope not cleared by MOO1 */

    if (x1 < x0) {
        int t;
        t = x1; x1 = x0; x0 = t;
        t = y1; y1 = y0; y0 = t;
    }

    {
        int dx, dy;
        dx = x1 - x0;
        dy = y1 - y0;
        yinc = UI_SCREEN_W * scale;
        if (dy < 0) {
            dy = -dy;
            yinc = -UI_SCREEN_W * scale;
        }
        if (dx < dy) {
            numpixels = dy + 1;
            yslope = 0x100;
            if (dy != 0) {
                xslope = (dx << 8) / dy;
            }
        } else {
            numpixels = dx + 1;
            if (dx != 0) {
                xslope = 0x100;
                yslope = (dy << 8) / dx;
            }
        }
    }

    {
        uint8_t *p = hw_video_get_buf() + (y0 * UI_SCREEN_W + x0) * scale;
        int xerr, yerr;

        xerr = 0x100 / 2;
        yerr = 0x100 / 2;

        while (numpixels--) {
            uint8_t color;
            color = colortbl[pos++];
            if (pos >= colornum) { pos = 0; }
            if (color != 0) {
                if (scale == 1) {
                    *p = color;
                } else {
                    gfxscale_draw_pixel(p, color, UI_SCREEN_W, scale);
                }
            }
            xerr += xslope;
            if ((xerr & 0xff00) != 0) {
                xerr &= 0xff;
                p += scale;
            }
            yerr += yslope;
            if ((yerr & 0xff00) != 0) {
                yerr &= 0xff;
                p += yinc;
            }
        }
    }
}

void ui_draw_line_limit(int x0, int y0, int x1, int y1, uint8_t color, int scale)
{
    ui_draw_line_limit_do(x0, y0, x1, y1, color, NULL, 0, 0, scale);
}

void ui_draw_line_limit_ctbl(int x0, int y0, int x1, int y1, const uint8_t *colortbl, int colornum, int pos, int scale)
{
    ui_draw_line_limit_do(x0, y0, x1, y1, 0, colortbl, colornum, pos, scale);
}

void ui_draw_slider(int x, int y, int w, int wdiv, int xoff, uint8_t color, int scale)
{
    int y1, x1;
    x1 = x * scale + xoff + (w * scale) / wdiv;
    x *= scale;
    y *= scale;
    y1 = y + 2 * scale + scale - 1;
    if (x1 < x) {
        int t;
        t = x1; x1 = x; x = t;
    }
    ui_draw_filled_rect(x, y, x1, y1, color, 1);
}

void ui_draw_box1(int x0, int y0, int x1, int y1, uint8_t color1, uint8_t color2, int scale)
{
    ui_draw_line1(x0, y0, x1, y0, color1, scale);
    ui_draw_line1(x0, y0, x0, y1, color1, scale);
    ui_draw_line1(x0 + 1, y1, x1, y1, color2, scale);
    ui_draw_line1(x1, y0 + 1, x1, y1, color2, scale);
}

void ui_draw_box2(int x0, int y0, int x1, int y1, uint8_t color1, uint8_t color2, uint8_t color3, uint8_t color4, int scale)
{
    ui_draw_box1(x0, y0, x1, y1, color1, color3, scale);
    ++x0; ++y0; --x1; --y1;
    ui_draw_box1(x0, y0, x1, y1, color2, color4, scale);
}

void ui_draw_box_fill(int x0, int y0, int x1, int y1, const uint8_t *colorptr, uint8_t color0, uint16_t colorhalf, uint16_t ac, uint8_t colorpos, int scale)
{
    uint8_t *s = hw_video_get_buf() + (y0 * UI_SCREEN_W + x0) * scale;
    uint16_t xstep, ystep, vx, vy, h, w, colornum;
    colornum = colorhalf << 1;
    /*v12 = colorpos & 0xff;*/
    ui_draw_box_fill_sub1(colorhalf, colorptr, color0);
    w = x1 - x0 + 1;
    xstep = (colorhalf * 0x80 * ac) / w;
    h = y1 - y0 + 1;
    ystep = (colorhalf * 0x80 * ac) / h;
    vx = 0;
    do {
        uint8_t *p;
        vx = vy = vx + xstep;
        p = s;
        s += scale;
        for (int y = 0; y < h; ++y) {
            uint8_t c;
            vy += ystep;
            c = (((((uint16_t)tbl_color_grain[colorpos++]) << 1) + vy) >> 8) & 0x3f;
            while (c >= colornum) {
                c -= colornum;
            }
            c = ui_draw_box_fill_tbl[c];
            if (scale == 1) {
                *p = c;
                p += UI_SCREEN_W;
            } else {
                p = gfxscale_draw_pixel(p, c, UI_SCREEN_W, scale);
            }
        }
    } while (--w);
}

void ui_draw_text_overlay(int x, int y, const char *str)
{
    int x0, x1, y0, y1, w, h;
    lbxfont_select(0, 0, 0, 0);
    h = lbxfont_get_height();
    w = lbxfont_calc_str_width(str);
    x0 = x - 3;
    SETMAX(x0, 0);
    y0 = y - 3;
    SETMAX(y0, 0);
    x1 = x + w + 4;
    SETMIN(x1, UI_VGA_W - 1);
    y1 = y + h + 5;
    SETMIN(y1, UI_VGA_H - 1);
    ui_draw_filled_rect(x0, y0, x1, y1, 0, ui_scale);
    lbxfont_print_str_normal(x + 1, y + 1, str, UI_SCREEN_W, ui_scale);
}

void ui_draw_box_grain(int x0, int y0, int x1, int y1, uint8_t color0, uint8_t color1, uint8_t ae, int scale)
{
    if ((x0 / 4) == (x1 / 4)) {
        ui_draw_filled_rect(x0, y0, x1, y1, color0, scale);
    } else {
        const uint8_t vga_tbl_mask_x0[4] = { 0xf, 0xe, 0xc, 0x8 };
        const uint8_t vga_tbl_mask_x1[4] = { 0x1, 0x3, 0x7, 0xf };

        uint8_t *s = hw_video_get_buf() + (y0 * UI_SCREEN_W) * scale;
        uint8_t *p;
        uint16_t v6, h = y1 - y0 + 1;
        uint8_t ah, bl;

        v6 = rnd_bitfiddle(ae);
        p = s + (x0 & ~3) * scale;
        ah = vga_tbl_mask_x0[x0 & 3];
        bl = v6 & 0xff;
        for (int y = 0; y < h; ++y) {
            uint8_t m;
            m = ah;
            for (int xa = 0; xa < 4; ++xa, m >>= 1) {
                if (m & 0x1) {
                    if (scale == 1) {
                        p[xa] = color0;
                    } else {
                        for (int sy = 0; sy < scale; ++sy) {
                            for (int sx = 0; sx < scale; ++sx) {
                                p[sy * UI_SCREEN_W + xa * scale + sx] = color0;
                            }
                        }
                    }
                }
            }
            m = tbl_color_grain[bl] & ah;
            for (int xa = 0; xa < 4; ++xa, m >>= 1) {
                if (m & 0x1) {
                    if (scale == 1) {
                        p[xa] = color1;
                    } else {
                        for (int sy = 0; sy < scale; ++sy) {
                            for (int sx = 0; sx < scale; ++sx) {
                                p[sy * UI_SCREEN_W + xa * scale + sx] = color1;
                            }
                        }
                    }
                }
            }
            ++bl;
            p += UI_SCREEN_W * scale;
        }

        v6 = rnd_bitfiddle(v6);

        p = s + (x1 & ~3) * scale;
        ah = vga_tbl_mask_x1[x1 & 3];
        bl = v6 & 0xff;
        for (int y = 0; y < h; ++y) {
            uint8_t m;
            m = ah;
            for (int xa = 0; xa < 4; ++xa, m >>= 1) {
                if (m & 0x1) {
                    if (scale == 1) {
                        p[xa] = color0;
                    } else {
                        for (int sy = 0; sy < scale; ++sy) {
                            for (int sx = 0; sx < scale; ++sx) {
                                p[sy * UI_SCREEN_W + xa * scale + sx] = color0;
                            }
                        }
                    }
                }
            }
            m = tbl_color_grain[bl] & ah;
            for (int xa = 0; xa < 4; ++xa, m >>= 1) {
                if (m & 0x1) {
                    if (scale == 1) {
                        p[xa] = color1;
                    } else {
                        for (int sy = 0; sy < scale; ++sy) {
                            for (int sx = 0; sx < scale; ++sx) {
                                p[sy * UI_SCREEN_W + xa * scale + sx] = color1;
                            }
                        }
                    }
                }
            }
            ++bl;
            p += UI_SCREEN_W * scale;
        }

        v6 = rnd_bitfiddle(v6);

        uint16_t wp4 = (x1 / 4) - (x0 / 4) - 1;
        if (wp4 == 0) {
            return;
        }

        p = s + ((x0 & ~3) + 4/* skip x0..x0+3 */) * scale;

        for (int y = 0; y < h; ++y) {
            bl = v6 & 0xff;
            for (int xq = 0; xq < wp4; ++xq) {
                uint8_t m;

                if (scale == 1) {
                    p[0] = color0;
                    p[1] = color0;
                    p[2] = color0;
                    p[3] = color0;
                } else {
                    for (int sy = 0; sy < scale; ++sy) {
                        memset(&p[sy * UI_SCREEN_W], color0, 4 * scale);
                    }
                }

                m = tbl_color_grain[bl] & 0xf;
                for (int xa = 0; xa < 4; ++xa, m >>= 1) {
                    if (m & 0x1) {
                        if (scale == 1) {
                            p[xa] = color1;
                        } else {
                            for (int sy = 0; sy < scale; ++sy) {
                                for (int sx = 0; sx < scale; ++sx) {
                                    p[sy * UI_SCREEN_W + xa * scale + sx] = color1;
                                }
                            }
                        }
                    }
                }
                ++bl;
                p += 4 * scale;
            }
            p += (UI_SCREEN_W - wp4 * 4) * scale;
            v6 = rnd_bitfiddle(v6);
        }
    }
}

static void ui_draw_finish_wipe_anim_do(int x, int y, int f)
{
    int vx, vy;
    vx = x + 19;
    vy = y + 19;
    x += f;
    y += f;
    vx -= f;
    vy -= f;
    ui_draw_copy_line(x, y, vx, y);
    ui_draw_copy_line(x, y, x, vy);
    ui_draw_copy_line(vx, y, vx, vy);
}

static void ui_draw_finish_wipe_anim(void)
{
    for (int f = 0; f < 10; ++f) {
        ui_delay_prepare();
        for (int x = 0; x < UI_SCREEN_W; x += 20) {
            for (int y = 0; y < UI_SCREEN_H; y += 20) {
                ui_draw_finish_wipe_anim_do(x, y, f);
            }
        }
        hw_video_redraw_front();
        ui_delay_us_or_click(MOO_TICKS_TO_US(1) / 2);
    }
    ui_cursor_store_bg0(moouse_x, moouse_y);
    hw_video_draw_buf();
    ui_cursor_setup_area(1, &ui_cursor_area_tbl[0]);    /* HACK enable cursor for -nextturn */
}

void ui_draw_finish(void)
{
    if (ui_draw_finish_mode == 0) {
        ui_palette_set_n();
        uiobj_finish_frame();
    } else if (ui_draw_finish_mode == 1) {
        ui_draw_finish_wipe_anim();
    } else if (ui_draw_finish_mode == 2) {
        uiobj_finish_frame();
        ui_palette_fadein_4b_19_1();
    }
    ui_draw_finish_mode = 0;
}

void ui_draw_stars(int x, int y, int xoff1, int xoff2, int scale)
{
    const int sx1[16] = { 2, 6, 30, 34, 48, 74, 88, 96, 99, 103, 119, 123, 136, 137, 152, 159 };
    const int sy1[16] = { 15, 2, 16, 24, 19, 4, 11, 23, 22, 10, 21, 11, 4, 12, 22, 10 };
    const int sx2[23] = { 0, 6, 11, 33, 36, 46, 52, 67, 84, 86, 91, 95, 98, 103, 107, 112, 123, 125, 139, 142, 148, 151, 159 };
    const int sy2[23] = { 22, 8, 18, 19, 3, 18, 7, 24, 14, 17, 11, 1, 13, 15, 5, 6, 19, 1, 13, 10, 6, 23, 13 };
    int xo1, xo2;
    xo1 = (ui_data.starmap.stars_xoff1 + xoff1) % (320 / 2);
    xo2 = (ui_data.starmap.stars_xoff2 + xoff1 * 2) % (320 / 2);
    if (((UI_SCREEN_W / 2) - xoff2) > xo1) {
        int tx = xo1 + xoff2;
        for (int i = 0; i < 16; ++i) {
            int sx = sx1[i];
            if ((sx >= xo1) && (sx < tx)) {
                ui_draw_pixel(sx - xo1 + x, sy1[i] + y, 4, scale);
            }
        }
    } else {
        int tx;
        for (int i = 0; i < 16; ++i) {
            int sx = sx1[i];
            if (sx >= xo1) {
                ui_draw_pixel(sx - xo1 + x, sy1[i] + y, 4, scale);
            }
        }
        tx = (xo1 + xoff2) % (320 / 2);
        for (int i = 0; i < 16; ++i) {
            int sx = sx1[i];
            if (sx < tx) {
                ui_draw_pixel(sx - xo1 + x + 160, sy1[i] + y, 4, scale);
            }
        }
    }
    if (((320 / 2) - xoff2) > xo2) {
        int tx = xo2 + xoff2;
        for (int i = 0; i < 23; ++i) {
            int sx = sx2[i];
            if ((sx >= xo2) && (sx < tx)) {
                ui_draw_pixel(sx - xo2 + x, sy2[i] + y, 6, scale);
            }
        }
    } else {
        int tx;
        for (int i = 0; i < 23; ++i) {
            int sx = sx2[i];
            if (sx >= xo2) {
                ui_draw_pixel(sx - xo2 + x, sy2[i] + y, 6, scale);
            }
        }
        tx = (xo2 + xoff2) % (320 / 2);
        for (int i = 0; i < 23; ++i) {
            int sx = sx2[i];
            if (sx < tx) {
                ui_draw_pixel(sx - xo2 + x + 160, sy2[i] + y, 6, scale);
            }
        }
    }
}

void ui_draw_set_stars_xoffs(bool flag_right)
{
    int x1, x2;
    if (flag_right) {
        x1 = 159;
        x2 = 158;
    } else {
        x1 = 1;
        x2 = 2;
    }
    ui_data.starmap.stars_xoff1 = (ui_data.starmap.stars_xoff1 + x1) % (UI_SCREEN_W / 2);
    ui_data.starmap.stars_xoff2 = (ui_data.starmap.stars_xoff2 + x2) % (UI_SCREEN_W / 2);
}

void ui_draw_textbox_2str(const char *str1, const char *str2, int y0, int scale)
{
    int x0 = 48, w = 132, x1 = x0 + w - 1, y1;
    lbxfont_select_set_12_1(0, 0, 0, 0);
    lbxfont_set_gap_h(3);
    lbxfont_set_14_24(0xb, 0xe);
    y1 = lbxfont_calc_split_str_h(w - 8, str2) + y0 + 7;
    if (*str1 != '\0') {
        y1 += lbxfont_get_height() + 4;
    }
    ui_draw_box_fill(x0 + 2, y0 + 2, x1 - 2, y1 - 2, colortbl_textbox, 0, 5, 1, (0x2737 & 0xffu), scale);
    ui_draw_box2(x0, y0, x1, y1, 0x7, 0x10, 0x13, 0x12, scale);
    ui_draw_pixel(x0, y0, 0x10, scale);
    ui_draw_pixel(x0 + 1, y0 + 1, 0xf, scale);
    ui_draw_pixel(x0 + 2, y0 + 1, 0xf, scale);
    ui_draw_pixel(x0 + 1, y0 + 2, 0xf, scale);
    ui_draw_pixel(x0 + 1, y0 + 3, 0xf, scale);
    ui_draw_line1(x0 + 1, y1 + 1, x1 + 1, y1 + 1, 0, scale);
    ui_draw_line1(x1 + 1, y0 + 1, x1 + 1, y1, 0, scale);
    if (*str1 != '\0') {
        lbxfont_select_subcolors_13not1();
        lbxfont_print_str_center(x0 + w / 2, y0 + 4, str1, UI_SCREEN_W, scale);
        lbxfont_select_subcolors_0();
        y0 += lbxfont_get_height() + 5;
    }
    lbxfont_print_str_split(x0 + 4, y0 + 4, w - 8, str2, 2, UI_SCREEN_W, UI_SCREEN_H, scale);
}
