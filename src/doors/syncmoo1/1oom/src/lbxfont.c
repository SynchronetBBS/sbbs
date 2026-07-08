#include "config.h"

#include <stdio.h>
#include <string.h>

#include "lbxfont.h"
#include "bits.h"
#include "hw.h"
#include "gfxscale.h"
#include "lbx.h"
#include "lbxpal.h"
#include "lib.h"
#include "types.h"

/* -------------------------------------------------------------------------- */


/* -------------------------------------------------------------------------- */

static uint8_t *lbxfontdata = 0;

static uint8_t lbxfont_temp_color = 0;
static int16_t lbxfont_temp_x = 0;
static int16_t lbxfont_temp_y = 0;
static uint8_t lbxfont_current_fontnum = 0;
static uint8_t lbxfont_current_fonta2 = 0;
static uint8_t lbxfont_current_fonta2b = 0;
static uint8_t lbxfont_current_fonta4 = 0;

#define SPLIT_STR_MAX_LINES 8
#define SPLIT_STR_MAX_LEN   1023

typedef struct split_str_s {
    int num;
    int x0[SPLIT_STR_MAX_LINES];
    int x1[SPLIT_STR_MAX_LINES];
    int y[SPLIT_STR_MAX_LINES];
    int i[SPLIT_STR_MAX_LINES];
    char buf[SPLIT_STR_MAX_LEN + 1];
} split_str_t;

/* -------------------------------------------------------------------------- */

static int lbxfont_print_char_ret_x(int x, int y, char c, uint16_t pitch, int scale)
{
    uint8_t *p = hw_video_get_buf() + (y * pitch + x) * scale;
    return x + lbxfont_plotchar(c, p, pitch, scale);
}

static void lbxfont_plotchar_limit(int x, int y, char c, int xskip, int char_w, int yskip, int char_h, uint16_t pitch, int scale)
{
    uint16_t si = GET_LE_16(&lbxfontdata[0xaa + c * 2]);
    uint8_t *o, *q, *p = &lbxfontdata[si];
    uint8_t *buf = hw_video_get_buf() + (y * pitch + x) * scale;

    while (xskip) {
        if (*p++ == 0x80) {
            --xskip;
        }
    }

    while (char_w) {
        uint8_t ybuf[0x50];
        uint8_t b, h, col;
        for (int i = 0; i < char_h; ++i) {
            ybuf[i] = 0xff;
        }
        q = &ybuf[0];
        while (1) {
            b = *p++;
            if (!(b & 0x80)) {
                h = (b >> 4);
                col = lbxfontdata[b & 0xf];
                do {
                    *q++ = col;
                } while (--h);
            } else if (b & 0x7f) {
                q += (b & 0x7f);
            } else {
                break;
            }
        }
        o = buf;
        buf += scale;
        q = &ybuf[yskip];
        h = char_h - yskip;
        while (h--) {
            b = *q++;
            if (b == 0xff) {
                o += pitch * scale;
            } else {
                o = gfxscale_draw_pixel(o, b, pitch, scale);
            }
        }
        --char_w;
    }
}

static int lbxfont_print_char_ret_x_limit(int x, int y, char c, int lx0, int ly0, int lx1, int ly1, uint16_t pitch, int scale)
{
    c -= 0x20;
    if ((c < 0) || (c > 0x5e)) {
        return x;
    }
    uint16_t char_h = lbxfontdata[0x10];
    uint16_t gap_w = lbxfontdata[0x48];
    int8_t char_w = lbxfontdata[0x4a + c];
    if ((x < lx0) || ((x + char_w) > lx1) || (y < ly0) || ((y + char_h) > ly1)) {
        int xskip, yskip, h, x1 = x + char_w + gap_w;
        if (x < lx0) {
            xskip = lx0 - x;
            if (xskip >= char_w) {
                return x1;
            }
            x = lx0;
            char_w -= xskip;
        } else {
            xskip = 0;
        }
        if ((x + char_w) > lx1) {
            char_w = lx1 - x + 1;
            if (char_w < 1) {
                return x1;
            }
        }
        if (y < ly0) {
            yskip = ly0 - y;
            y = ly0;
        } else {
            yskip = 0;
        }
        if ((y + char_h) > ly1) {
            h = ly1 - y + 1;
        } else {
            h = char_h;
        }
        lbxfont_plotchar_limit(x, y, c, xskip, char_w, yskip, h, pitch, scale);
        return x1;
    } else {
        x = lbxfont_print_char_ret_x(x, y, c + 0x20, pitch, scale);
    }
    return x;
}

static int lbxfont_print_str_do(int x, int y, const char *str, bool change_color, int w, uint16_t pitch, int scale)
{
    uint16_t i, v8 = 0, va = 0, vc = 0;
    char c;
    lbxfont_temp_x = x;
    lbxfont_temp_y = y;
    if (w != 0) {
        uint16_t num_space = 0;
        i = 0;
        while (1) {
            c = str[i];
            if ((c == 0) || (c == 0x0d) || (c == 0x14) || (c == 0x19) || (c == 0x15) || (c == 0x1d)) {
                if (num_space == 0) {
                    w = 0;
                }
                w -= lbxfont_calc_str_width(str);
                if (w <= 0) {
                    w = 0;
                } else {
                    va = w % num_space;
                    vc = w / num_space;
                }
                break;
            } else {
                if (c == ' ') {
                    ++num_space;
                }
                ++i;
            }
        }
    }
    i = 0;
    while ((c = str[i]) != 0) {
        switch (c) {
            case 1:
                if (change_color) {
                    lbxfont_select_subcolors_0();
                }
                break;
            case 2:
            case 4:
                if (change_color) {
                    lbxfont_select_subcolors_13not1();
                }
                break;
            case 3:
                if (change_color) {
                    lbxfont_select_subcolors_13not2();
                }
                break;
            case 0x0d:
            case 0x14:
            case 0x19:
            case 0x1d:
                lbxfont_temp_x = ((uint8_t)str[i + 1]) + x;
                ++i;
                break;
            case 0x15:
                return lbxfont_temp_x;
            default:
                lbxfont_temp_x = lbxfont_print_char_ret_x(lbxfont_temp_x, lbxfont_temp_y, c, pitch, scale);
                if ((w != 0) && (c == ' ')) {
                    lbxfont_temp_x += vc;
                    if (v8 < va) {
                        ++lbxfont_temp_x;
                    }
                    ++v8;
                }
                break;
        }
        ++i;
    }
    return lbxfont_temp_x;
}

static int lbxfont_print_str_limit_do(int x, int y, const char *str, bool change_color, int lx0, int ly0, int lx1, int ly1, uint16_t pitch, int scale)
{
    uint8_t h = lbxfontdata[0x10];
    int w;
    lbxfont_temp_x = x;
    lbxfont_temp_y = y;
    char c;
    if (scale != 1) {
        lx0 /= scale;
        lx1 /= scale;
        ly0 /= scale;
        ly1 /= scale;
    }
    if ((y > ly1) || ((y + h) < ly0)) {
        return 0;
    }
    if (x > lx1) {
        return x;
    }
    w = lbxfont_calc_str_width(str);
    if ((x + w) < lx0) {
        return x + w;
    }
    while ((c = *str++) != 0) {
        switch (c) {
            case 1:
                if (change_color) {
                    lbxfont_select_subcolors_0();
                }
                break;
            case 2:
            case 4:
                if (change_color) {
                    lbxfont_select_subcolors_13not1();
                }
                break;
            case 3:
                if (change_color) {
                    lbxfont_select_subcolors_13not2();
                }
                break;
            case 0xd:
                return lbxfont_temp_x;
            default:
                lbxfont_temp_x = lbxfont_print_char_ret_x_limit(lbxfont_temp_x, lbxfont_temp_y, c, lx0, ly0, lx1, ly1, pitch, scale);
                break;
        }
    }
    return lbxfont_temp_x;
}

static int lbxfont_print_str_normal_do(int x, int y, const char *str, int w, uint16_t pitch, int scale)
{
    uint16_t v2;
    v2 = lbxfontdata[0x12];
    if (v2 != 0) {
        for (int i = 0; i < 0x10; ++i) {
            lbxfontdata[i] = lbxfont_temp_color;
        }
        if (v2 != 2) {
            lbxfont_print_str_do(x + 1, y + 1, str, false, w, pitch, scale);
            lbxfont_print_str_do(x, y + 1, str, false, w, pitch, scale);
            lbxfont_print_str_do(x + 1, y, str, false, w, pitch, scale);
        }
        if ((v2 != 1) && (v2 != 3)) {
            lbxfont_print_str_do(x - 1, y, str, false, w, pitch, scale);
            lbxfont_print_str_do(x - 1, y - 1, str, false, w, pitch, scale);
            lbxfont_print_str_do(x, y - 1, str, false, w, pitch, scale);
        }
        if ((v2 == 3) || (v2 == 5)) {
            lbxfont_print_str_do(x + 2, y + 2, str, false, w, pitch, scale);
            lbxfont_print_str_do(x + 1, y + 2, str, false, w, pitch, scale);
            lbxfont_print_str_do(x + 2, y + 1, str, false, w, pitch, scale);
        }
        if (v2 > 3) {
            lbxfont_print_str_do(x + 1, y - 1, str, false, w, pitch, scale);
            lbxfont_print_str_do(x - 1, y + 1, str, false, w, pitch, scale);
        }
        if (v2 == 5) {
            lbxfont_print_str_do(x + 2, y, str, false, w, pitch, scale);
            lbxfont_print_str_do(x, y + 2, str, false, w, pitch, scale);
        }
        lbxfont_select_subcolors(lbxfontdata[0x13]);
    }
    return lbxfont_print_str_do(x, y, str, true, w, pitch, scale);
}

static int lbxfont_print_str_normal_do_w0(int x, int y, const char *str, int w, uint16_t pitch, int scale)
{
    if (w < 0) {
        w = 0;
    }
    return lbxfont_print_str_normal_do(x, y, str, w, pitch, scale);
}

static void lbxfont_split_str(int x, int y, int maxw, const char *str, split_str_t *s, uint16_t maxy)
{
    int i = 0, j, x0, x1, w, pos_space, last_c, ty = y, xnext = -1;
    uint16_t line_h = lbxfontdata[0x44];
    int16_t gap_w = lbxfontdata[0x48];
    uint8_t char_h = lbxfontdata[0x10];
    s->num = 0;

    strncpy(s->buf, str, SPLIT_STR_MAX_LEN);
    s->buf[SPLIT_STR_MAX_LEN] = 0;

    while (s->buf[i] != '\0') {
        if (xnext == -1) {
            x0 = x;
        } else {
            x0 = xnext;
            xnext = -1;
        }
        x1 = x + maxw - 1;
        {
            if ((ty + char_h) >= maxy) {
                s->num = 0;
                return;
            }
            w = x1 - x0 + 1;
            pos_space = -1;
            last_c = -1;
            for (j = i; -gap_w <= w; ++j) {
                uint8_t c;
                int cw;
                c = (uint8_t)s->buf[j];
                if (c >= 0x20) {
                    cw = lbxfontdata[0x4a + (c - 0x20)] + gap_w;
                    w -= cw;
                    if (c == 0x20) {
                        pos_space = j;
                    }
                } else {
                    if ((c == 0x14) || (c == 0xd) || (c == 0) || (c == 0x19) || (c == 0x1d)) {
                        w = -gap_w - 1;
                        last_c = c;
                    }
                }
            }
            if (last_c == 0) {
                --j;
            }
            s->x0[s->num] = x0;
            s->x1[s->num] = x1;
            s->y[s->num] = ty;
            s->i[s->num] = i;
            ++s->num;
            if (last_c != -1) {
                if (last_c == 0x19) {
                    int v;
                    v = (s->buf[j] - 0x30) * 10 + (s->buf[j/*bug?*/] - 0x30);
                    ty = v - line_h;
                    j += 2;
                } else if (last_c == 0x1d) {
                    int v;
                    v = (s->buf[j] - 0x30) * 10 + (s->buf[j/*bug?*/] - 0x30);
                    ty -= line_h;
                    xnext = v;
                    j += 2;
                }
                i = j;
            } else {
                if (pos_space != -1) {
                    s->buf[pos_space] = 0x15;
                    i = pos_space + 1;
                }
            }
        }
        if ((x + maxw - 1) > x1) {
            xnext = x1 + 1;
        } else {
            ty += line_h;
        }
    }
}

static uint16_t lbxfont_plotchar_1x(char c, uint8_t *buf, uint16_t pitch)
{
    uint8_t char_w = lbxfontdata[0x4a + c];
    uint16_t gap_w = lbxfontdata[0x48];
    uint16_t w = char_w + gap_w;

    uint16_t si = GET_LE_16(&lbxfontdata[0xaa + c * 2]);
    uint8_t *p = &lbxfontdata[si];
    uint8_t *q = buf;

    while (char_w) {
        uint8_t b, h, col;
        b = *p++;
        if (!(b & 0x80)) {
            h = (b >> 4);
            col = lbxfontdata[b & 0xf];
            do {
                *q = col;
                q += pitch;
            } while (--h);
        } else if (b & 0x7f) {
            q += (b & 0x7f) * pitch;
        } else {
            ++buf;
            q = buf;
            --char_w;
        }
    }
    return w;
}

static uint16_t lbxfont_plotchar_nx(char c, uint8_t *buf, uint16_t pitch, int scale)
{
    uint8_t char_w = lbxfontdata[0x4a + c];
    uint16_t gap_w = lbxfontdata[0x48];
    uint16_t w = char_w + gap_w;

    uint16_t si = GET_LE_16(&lbxfontdata[0xaa + c * 2]);
    uint8_t *p = &lbxfontdata[si];
    uint8_t *q = buf;

    while (char_w) {
        uint8_t b, h, col;
        b = *p++;
        if (!(b & 0x80)) {
            h = (b >> 4);
            col = lbxfontdata[b & 0xf];
            do {
                q = gfxscale_draw_pixel(q, col, pitch, scale);
            } while (--h);
        } else if (b & 0x7f) {
            q += (b & 0x7f) * pitch * scale;
        } else {
            buf += scale;
            q = buf;
            --char_w;
        }
    }
    return w;
}

/* -------------------------------------------------------------------------- */

int lbxfont_init(void)
{
    uint8_t *fontdata = lbxfile_item_get(LBXFILE_FONTS, 0);
    lbxfontdata = fontdata;
    return 0;
}

void lbxfont_shutdown(void)
{
    if (lbxfontdata) {
        lbxfile_item_release(LBXFILE_FONTS, lbxfontdata);
        lbxfontdata = 0;
    }
}

void lbxfont_select(uint8_t a0, uint8_t a1, uint8_t a2, uint8_t a3)
{
    uint16_t t;

    if (a1 >= 0x10) {
        a1 = 0;
    }
    if (a2 >= 0x10) {
        a2 = 0;
    }
    if (a3 >= 0x10) {
        a3 = 0;
    }

    lbxfont_current_fontnum = a0;
    lbxfont_current_fonta2 = a1;
    lbxfont_current_fonta2b = a2;
    lbxfont_current_fonta4 = a3;

    memcpy(&(lbxfontdata[0x00]), &(lbxpal_fontcolors[a1 << 4]), 0x10);
    memcpy(&(lbxfontdata[0x14]), &(lbxpal_fontcolors[a1 << 4]), 0x10);
    memcpy(&(lbxfontdata[0x24]), &(lbxpal_fontcolors[a2 << 4]), 0x10);
    memcpy(&(lbxfontdata[0x34]), &(lbxpal_fontcolors[a3 << 4]), 0x10);
    memcpy(&(lbxfontdata[0x10]), &(lbxfontdata[(a0 << 1) + 0x16a]), 0x2);
    t = GET_LE_16(&(lbxfontdata[(a0 << 1) + 0x18a]));
    SET_LE_16(&(lbxfontdata[0x46]), t);
    t += GET_LE_16(&(lbxfontdata[0x10]));
    SET_LE_16(&(lbxfontdata[0x44]), t);
    t = GET_LE_16(&(lbxfontdata[(a0 << 1) + 0x17a]));
    SET_LE_16(&(lbxfontdata[0x48]), t);
    SET_LE_16(&(lbxfontdata[0x12]), 0);
    memcpy(&(lbxfontdata[0x4a]), &(lbxfontdata[(a0 * 0x60) + 0x19a]), 0x60);
    memcpy(&(lbxfontdata[0xaa]), &(lbxfontdata[(a0 * 0xc0) + 0x49a]), 0xc0);
}

void lbxfont_select_set_12_1(uint8_t a0, uint8_t a1, uint8_t a2, uint8_t a3)
{
    lbxfont_select(a0, a1, a2, a3);
    SET_LE_16(&(lbxfontdata[0x12]), 1);
}

void lbxfont_select_set_12_4(uint8_t a0, uint8_t a1, uint8_t a2, uint8_t a3)
{
    lbxfont_select(a0, a1, a2, a3);
    SET_LE_16(&(lbxfontdata[0x12]), 4);
}

void lbxfont_select_set_12_5(uint8_t a0, uint8_t a1, uint8_t a2, uint8_t a3)
{
    lbxfont_select(a0, a1, a2, a3);
    SET_LE_16(&(lbxfontdata[0x12]), 5);
}

uint16_t lbxfont_get_height(void)
{
    return GET_LE_16(&(lbxfontdata[0x10]));
}

int lbxfont_get_gap_h(void)
{
    return ((uint16_t)lbxfontdata[0x44]) - GET_LE_16(&(lbxfontdata[0x10]));
}

void lbxfont_set_gap_h(uint16_t value)
{
    value += GET_LE_16(&(lbxfontdata[0x10]));
    SET_LE_16(&(lbxfontdata[0x44]), value);
}

void lbxfont_select_subcolors(uint16_t a0)
{
    memcpy(&(lbxfontdata[0x00]), &(lbxfontdata[(a0 << 4) + 0x14]), 0x10);
    lbxfontdata[0x13] = a0;
}

void lbxfont_select_subcolors_0(void)
{
    lbxfont_select_subcolors(0);
}

void lbxfont_select_subcolors_13not1(void)
{
    lbxfont_select_subcolors((lbxfontdata[0x13] == 1) ? 0 : 1);
}

void lbxfont_select_subcolors_13not2(void)
{
    lbxfont_select_subcolors((lbxfontdata[0x13] == 2) ? 0 : 2);
}

void lbxfont_set_colors(const uint8_t *colorptr)
{
    memcpy(&(lbxfontdata[0x00]), colorptr, 0x10);
}

void lbxfont_set_colors_n(const uint8_t *colorptr, int num)
{
    memcpy(&(lbxfontdata[0x00]), colorptr, num);
}

void lbxfont_set_color_c_n(uint8_t color, int num)
{
    memset(&(lbxfontdata[0x00]), color, num);
}

void lbxfont_set_color0(uint8_t color)
{
    lbxfontdata[0x00] = color;
}

void lbxfont_set_temp_color(uint8_t color)
{
    lbxfont_temp_color = color;
}

void lbxfont_set_14_24(uint8_t color1, uint8_t color2)
{
    for (int i = 0; i < 0x10; ++i) {
        lbxfontdata[0x14 + i] = color1;
        lbxfontdata[0x24 + i] = color2;
    }
}

uint16_t lbxfont_plotchar(char c, uint8_t *buf, uint16_t pitch, int scale)
{
    if (c < 0x20) {
        return 0;
    }
    c -= 0x20;
    if (scale == 1) {
        return lbxfont_plotchar_1x(c, buf, pitch);
    } else {
        return lbxfont_plotchar_nx(c, buf, pitch, scale);
    }
}

void lbxfont_set_space_w(int w)
{
    lbxfontdata[0x4a] = w;
}

int lbxfont_get_gap_w(void)
{
    return lbxfontdata[0x48];
}

int lbxfont_get_char_w(char c)
{
    if ((c >= 0x20) && (c <= 0x7e)) {
        return lbxfontdata[0x4a + c - 0x20];
    }
    return lbxfontdata[0x48];
}

int lbxfont_calc_str_width(const char *str)
{
    uint16_t gap_w = lbxfontdata[0x48];
    uint16_t w = 0;
    int16_t c;
    while (1) {
        c = *str++;
        c -= 0x20;
        if (c < 0) {
            c += 0x20;
            if ((c == 0) || (c == 0xd) || (c == 0x14) || (c == 0x15) || (c == 0x19) || (c == 0x1d)) {
                return w - gap_w;
            }
        } else {
            if (c <= 0x5e) {
                w += lbxfontdata[0x4a + c];
            }
            w += gap_w;
        }
    }
}

int lbxfont_calc_split_str_h(int maxw, const char *str)
{
    split_str_t s;
    lbxfont_split_str(0, 0, maxw, str, &s, 200/*irrelevant*/);
    return s.y[s.num - 1] + GET_LE_16(&(lbxfontdata[0x10]));
}

int lbxfont_print_str_normal(int x, int y, const char *str, uint16_t pitch, int scale)
{
    return lbxfont_print_str_normal_do(x, y, str, 0, pitch, scale);
}

int lbxfont_print_str_center(int x, int y, const char *str, uint16_t pitch, int scale)
{
    int w = lbxfont_calc_str_width(str);
    return lbxfont_print_str_normal(x - w / 2, y, str, pitch, scale);
}

int lbxfont_print_str_right(int x, int y, const char *str, uint16_t pitch, int scale)
{
    int w = lbxfont_calc_str_width(str) - 1;
    return lbxfont_print_str_normal(x - w, y, str, pitch, scale);
}

int lbxfont_print_str_normal_limit(int x, int y, const char *str, int lx0, int ly0, int lx1, int ly1, uint16_t pitch, int scale)
{
    uint16_t v2;
    v2 = lbxfontdata[0x12];
    if (v2 != 0) {
        for (int i = 0; i < 0x10; ++i) {
            lbxfontdata[i] = lbxfont_temp_color;
        }
        if (v2 != 2) {
            lbxfont_print_str_limit_do(x + 1, y + 1, str, false, lx0, ly0, lx1, ly1, pitch, scale);
            lbxfont_print_str_limit_do(x, y + 1, str, false, lx0, ly0, lx1, ly1, pitch, scale);
            lbxfont_print_str_limit_do(x + 1, y, str, false, lx0, ly0, lx1, ly1, pitch, scale);
        }
        if ((v2 != 1) && (v2 != 3)) {
            lbxfont_print_str_limit_do(x - 1, y, str, false, lx0, ly0, lx1, ly1, pitch, scale);
            lbxfont_print_str_limit_do(x - 1, y - 1, str, false, lx0, ly0, lx1, ly1, pitch, scale);
            lbxfont_print_str_limit_do(x, y - 1, str, false, lx0, ly0, lx1, ly1, pitch, scale);
        }
        if ((v2 == 3) || (v2 == 5)) {
            lbxfont_print_str_limit_do(x + 2, y + 2, str, false, lx0, ly0, lx1, ly1, pitch, scale);
            lbxfont_print_str_limit_do(x + 1, y + 2, str, false, lx0, ly0, lx1, ly1, pitch, scale);
            lbxfont_print_str_limit_do(x + 2, y + 1, str, false, lx0, ly0, lx1, ly1, pitch, scale);
        }
        if (v2 > 3) {
            lbxfont_print_str_limit_do(x + 1, y - 1, str, false, lx0, ly0, lx1, ly1, pitch, scale);
            lbxfont_print_str_limit_do(x - 1, y + 1, str, false, lx0, ly0, lx1, ly1, pitch, scale);
        }
        if (v2 == 5) {
            lbxfont_print_str_limit_do(x + 2, y, str, false, lx0, ly0, lx1, ly1, pitch, scale);
            lbxfont_print_str_limit_do(x, y + 2, str, false, lx0, ly0, lx1, ly1, pitch, scale);
        }
        lbxfont_select_subcolors(lbxfontdata[0x13]);
    }
    return lbxfont_print_str_limit_do(x, y, str, true, lx0, ly0, lx1, ly1, pitch, scale);
}

int lbxfont_print_str_center_limit(int x, int y, const char *str, int lx0, int ly0, int lx1, int ly1, uint16_t pitch, int scale)
{
    int w = lbxfont_calc_str_width(str);
    return lbxfont_print_str_normal_limit(x - w / 2, y, str, lx0, ly0, lx1, ly1, pitch, scale);
}

void lbxfont_print_str_split(int x, int y, int maxw, const char *str, int type, uint16_t pitch, uint16_t maxy, int scale)
{
    split_str_t s;
    lbxfont_split_str(x, y, maxw, str, &s, maxy);
    for (int i = 0; i < s.num; ++i) {
        switch (type) {
            case 0:
                lbxfont_print_str_normal(s.x0[i], s.y[i], &s.buf[s.i[i]], pitch, scale);
                break;
            case 1:
                lbxfont_print_str_right(s.x1[i], s.y[i], &s.buf[s.i[i]], pitch, scale);
                break;
            case 2:
                lbxfont_print_str_center((s.x0[i] + s.x1[i]) / 2, s.y[i], &s.buf[s.i[i]], pitch, scale);
                break;
            case 3:
                if (i != (s.num - 1)) {
                    lbxfont_print_str_normal_do_w0(s.x0[i], s.y[i], &s.buf[s.i[i]], s.x1[i] - s.x0[i], pitch, scale);
                } else {
                    lbxfont_print_str_normal(s.x0[i], s.y[i], &s.buf[s.i[i]], pitch, scale);
                }
                break;
            default:
                break;
        }
    }
}

int lbxfont_print_num_normal(int x, int y, int num, uint16_t pitch, int scale)
{
    char buf[16];
    lib_sprintf(buf, sizeof(buf), "%i", num);
    return lbxfont_print_str_normal(x, y, buf, pitch, scale);
}

int lbxfont_print_num_center(int x, int y, int num, uint16_t pitch, int scale)
{
    char buf[16];
    lib_sprintf(buf, sizeof(buf), "%i", num);
    return lbxfont_print_str_center(x, y, buf, pitch, scale);
}

int lbxfont_print_num_right(int x, int y, int num, uint16_t pitch, int scale)
{
    char buf[16];
    lib_sprintf(buf, sizeof(buf), "%i", num);
    return lbxfont_print_str_right(x, y, buf, pitch, scale);
}

int lbxfont_print_range_right(int x, int y, int num0, int num1, uint16_t pitch, int scale)
{
    char buf[32];
    lib_sprintf(buf, sizeof(buf), "%i-%i", num0, num1);
    return lbxfont_print_str_right(x, y, buf, pitch, scale);
}

uint8_t lbxfont_get_current_fontnum(void)
{
    return lbxfont_current_fontnum;
}

uint8_t lbxfont_get_current_fonta2(void)
{
    return lbxfont_current_fonta2;
}

uint8_t lbxfont_get_current_fonta2b(void)
{
    return lbxfont_current_fonta2b;
}

uint8_t lbxfont_get_current_fonta4(void)
{
    return lbxfont_current_fonta4;
}
