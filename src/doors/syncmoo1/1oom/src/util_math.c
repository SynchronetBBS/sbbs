#include "config.h"

#include <stdlib.h>

#include "util_math.h"
#include "log.h"
#include "types.h"

/* ------------------------------------------------------------------------- */

/* floor(0x100 * tan((2 * PI * (i + FIRST)) / 360)) */
static const uint8_t tbl_math_tan_0[] = {
    0x00,0x04,0x08,0x0d,0x11,0x16,0x1a,0x1f,0x23,0x28,0x2d,0x31,0x36,0x3b,0x3f,0x44,0x49,0x4e,0x53,0x58,0x5d,0x62
};

static const uint8_t tbl_math_tan_22[] = {
    0x67,0x6c,0x71,0x77,0x7c,0x82,0x88,0x8d,0x93,0x99,0x9f,0xa6,0xac,0xb3,0xb9,0xc0,0xc8,0xcf,0xd6,0xde,0xe6,0xee,0xf7
};

static const uint16_t tbl_math_tan_45[] = {
    0x0100,0x0109,0x0112,0x011c,0x0126,0x0131,0x013c,0x0147,0x0153,0x0160,0x016d,0x017b,0x018a,0x0199,0x01aa,0x01bb,
    0x01cd,0x01e1,0x01f6,0x020c,0x0224,0x023e
};

static const uint16_t tbl_math_tan_67[] = {
    0x025b,0x0279,0x029a,0x02bf,0x02e7,0x0313,0x0345,0x037c,0x03bb,0x0402,0x0454,0x04b4,0x0525,0x05ab,0x0650,0x071d,
    0x0825,0x0983,0x0b6e,0x0e4d,0x1315,0x1ca3,0x394d
};

/* The values of these two differ slightly, probably not important */
static const uint16_t tbl_math_sin[] = {
    0x0000,0x0477,0x08ef,0x0d65,0x11db,0x164f,0x1ac2,0x1f32,0x23a0,0x280c,0x2c74,0x30d8,0x3539,0x3996,0x3dee,0x4242,
    0x4690,0x4ad8,0x4f1b,0x5358,0x578e,0x5bbe,0x5fe6,0x6407,0x681f,0x6c30,0x7039,0x7438,0x782f,0x7c1c,0x8000,0x83d9,
    0x87a8,0x8b6d,0x8f27,0x92d5,0x9679,0x9a10,0x9d9c,0xa11b,0xa48d,0xa7f3,0xab4c,0xae97,0xb1d5,0xb505,0xb826,0xbb3a,
    0xbe3e,0xc134,0xc41b,0xc6f3,0xc9bb,0xcc73,0xcf1b,0xd1b4,0xd43b,0xd6b3,0xd919,0xdb6f,0xddb3,0xdfe7,0xe208,0xe419,
    0xe617,0xe803,0xe9de,0xeba6,0xed5b,0xeeff,0xf08f,0xf20d,0xf378,0xf4d0,0xf615,0xf746,0xf865,0xf970,0xfa67,0xfb4b,
    0xfc1c,0xfcd9,0xfd82,0xfe17,0xfe99,0xff06,0xff60,0xffa6,0xffd8,0xfff6
};

static const uint16_t tbl_math_cos[] = {
    0xfff6,0xffd8,0xffa6,0xff60,0xff06,0xfe98,0xfe17,0xfd82,0xfcd9,0xfc1c,0xfb4b,0xfa67,0xf970,0xf865,0xf746,0xf615,
    0xf4d0,0xf378,0xf20d,0xf08f,0xeeff,0xed5b,0xeba6,0xe9de,0xe803,0xe617,0xe418,0xe208,0xdfe7,0xddb3,0xdb6f,0xd919,
    0xd6b3,0xd43b,0xd1b3,0xcf1b,0xcc73,0xc9bb,0xc6f2,0xc41b,0xc134,0xbe3e,0xbb39,0xb826,0xb504,0xb1d5,0xae97,0xab4c,
    0xa7f3,0xa48d,0xa11b,0x9d9b,0x9a10,0x9678,0x92d5,0x8f27,0x8b6d,0x87a8,0x83d9,0x7fff,0x7c1c,0x782f,0x7438,0x7038,
    0x6c30,0x681f,0x6406,0x5fe6,0x5bbd,0x578e,0x5358,0x4f1b,0x4ad8,0x468f,0x4241,0x3dee,0x3996,0x3539,0x30d8,0x2c73,
    0x280b,0x23a0,0x1f32,0x1ac2,0x164f,0x11db,0x0d65,0x08ee,0x0477,0x0000
};

/* ------------------------------------------------------------------------- */

static int calc_angle_do(unsigned int dx, unsigned int dy)
{
    unsigned int slope;
    int angle;
    if (dx == 0) {
        return 90;
    }
    slope = (dy << 8) / dx;
    if (slope < 0x100) {
        if (slope >= 0x67) {
            angle = 22;
            for (int i = 0; i < 0x17; ++i) {
                if (slope < tbl_math_tan_22[i]) {
                    goto out2;
                }
                ++angle;
            }
            angle = 44;
            out2:
            --angle;
        } else {
            angle = 0;
            for (int i = 0; i < 0x16; ++i) {
                if (slope < tbl_math_tan_0[i]) {
                    goto out1;
                }
                ++angle;
            }
            angle = 22;
            out1:
            --angle;
        }
    } else {
        if (slope >= 0x258) {
            angle = 0;
            for (int i = 0; i < 0x17; ++i) {
                if (slope < tbl_math_tan_67[i]) {
                    goto out4;
                }
                ++angle;
            }
            return 90;
            out4:
            --angle;
            angle += 67;
        } else {
            angle = 0;
            for (int i = 0; i < 0x16; ++i) {
                if (slope < tbl_math_tan_45[i]) {
                    goto out3;
                }
                ++angle;
            }
            return 66;
            out3:
            --angle;
            angle += 45;
        }
    }
#ifdef FEATURE_MODEBUG
    if ((angle < 0) || (angle >= 180)) {
        LOG_DEBUG((3, "%s: dx:%i dy:%i -> slope:%i angle:%i\n", dx, dy, slope, angle));
    }
#endif
    return angle;
}

static inline int util_math_route_step_len(int adx, int ady)
{
    int v = adx + ady;
    return v ? (v + 1) : 3;
}

/* ------------------------------------------------------------------------- */

int util_math_calc_angle(int dx, int dy)
{
    if (dx >= 0) {
        if (dy >= 0) {
            return calc_angle_do(dx, dy);
        } else {
            return 360 - calc_angle_do(dx, -dy);
        }
    } else {
        if (dy >= 0) {
            return 180 - calc_angle_do(-dx, dy);
        } else {
            return 180 + calc_angle_do(-dx, -dy);
        }
    }
}

int util_math_angle_dist_cos(int angle, int dist)
{
    int res = 0;
    bool negate = false;
    if (angle < 91) {
        /* nop */
    } else if (angle < 181) {
        angle = 180 - angle;
        negate = true;
    } else if (angle < 271) {
        angle -= 180;
        negate = true;
    } else {
        angle = 360 - angle;
    }
    if (angle == 0) {
        res = dist;
    } else {
        res = (dist * tbl_math_cos[angle - 1]) / 0x10000;
    }
    if (negate) {
        res = -res;
    }
    return res;
}

int util_math_angle_dist_sin(int angle, int dist)
{
    int res = 0;
    bool negate = false;
    if (angle < 91) {
        /* nop */
    } else if (angle < 181) {
        angle = 180 - angle;
    } else if (angle < 271) {
        angle -= 180;
        negate = true;
    } else {
        angle = 360 - angle;
        negate = true;
    }
    if (angle == 90) {
        res = dist;
    } else {
        res = (dist * tbl_math_sin[angle]) / 0x10000;
    }
    if (negate) {
        res = -res;
    }
    return res;
}

void util_math_go_line_dist(int *x0ptr, int *y0ptr, int x1, int y1, int dist)
{
    int x, y, dx, dy, angle;
    if (dist <= 0) {
        return;
    }
    x = *x0ptr;
    y = *y0ptr;
    if ((x == x1) && (y == y1)) {
        return;
    }
    dx = x1 - x;
    dy = y1 - y;
    if ((abs(dx) > 0xff) || (abs(dy) > 0xff)) {
        dx /= 2;
        dy /= 2;
    }
    angle = util_math_calc_angle(dx, dy);
    x += util_math_angle_dist_cos(angle, dist);
    if (dx >= 0) {
        if (x > x1) {
            x = x1;
        }
    } else {
        if (x < x1) {
            x = x1;
        }
    }
    y += util_math_angle_dist_sin(angle, dist);
    if (dy >= 0) {
        if (y > y1) {
            y = y1;
        }
    } else {
        if (y < y1) {
            y = y1;
        }
    }
    *x0ptr = x;
    *y0ptr = y;
}

int util_math_dist_steps(int x0, int y0, int x1, int y1)
{
    int x, y, num = 0;
    x = x0;
    y = y0;
    while ((x != x1) || (y != y1)) {
        util_math_go_line_dist(&x, &y, x1, y1, 5);
        if ((x != x1) || (y != y1)) {
            util_math_go_line_dist(&x, &y, x1, y1, 6);
        }
        ++num;
    }
    return num;
}

int util_math_dist_fast(int x0, int y0, int x1, int y1)
{
    int dx, dy;
    dx = x1 - x0;
    if (dx < 0) {
        dx = -dx;
    }
    dy = y1 - y0;
    if (dy < 0) {
        dy = -dy;
    }
    return (dx > dy) ? (dx + dy / 2) : (dy + dx / 2);
}

int util_math_dist_maxabs(int x0, int y0, int x1, int y1)
{
    int dx, dy;
    dx = x1 - x0;
    if (dx < 0) {
        dx = -dx;
    }
    dy = y1 - y0;
    if (dy < 0) {
        dy = -dy;
    }
    return (dx > dy) ? dx : dy;
}

int util_math_line_plot(int x0, int y0, int x1, int y1, int *tblx, int *tbly)
{
    int len = 0, dx, dy, dirx, diry, slope, zerr = 0x8000;
    dx = x1 - x0;
    if (dx < 0) {
        dx = -dx;
        dirx = -1;
    } else {
        dirx = 1;
    }
    dy = y1 - y0;
    if (dy < 0) {
        dy = -dy;
        diry = -1;
    } else {
        diry = 1;
    }
    if (dx < dy) {
        slope = (dx << 16) / dy;
        while (len < dy) {
            y0 += diry;
            zerr += slope;
            if (zerr >= 0x10000) {
                x0 += dirx;
            }
            zerr &= 0xffff;
            tblx[len] = x0;
            tbly[len] = y0;
            ++len;
        }
    } else if (dy < dx) {
        slope = (dy << 16) / dx;
        while (len < dx) {
            x0 += dirx;
            zerr += slope;
            if (zerr >= 0x10000) {
                y0 += diry;
            }
            zerr &= 0xffff;
            tblx[len] = x0;
            tbly[len] = y0;
            ++len;
        }
    } else {
        while (len < dy) {
            x0 += dirx;
            y0 += diry;
            tblx[len] = x0;
            tbly[len] = y0;
            ++len;
        }
    }
    return len;
}

int util_math_get_route_len(int x0, int y0, const int *tblx, const int *tbly, int len)
{
    int l;
    l = util_math_route_step_len(abs(x0 - tblx[0]), abs(y0 - tbly[0]));
    for (int i = 1; i < len; ++i) {
        l += util_math_route_step_len(abs(tblx[i - 1] - tblx[i]), abs(tbly[i - 1] - tbly[i]));
    }
    return l;
}
