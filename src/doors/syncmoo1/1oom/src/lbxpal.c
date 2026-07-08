#include "config.h"

#include <string.h>
#include <stdlib.h>

#include "lbxpal.h"
#include "hw.h"
#include "lbx.h"
#include "lbxfont.h"
#include "log.h"
#include "types.h"

/* -------------------------------------------------------------------------- */

uint8_t lbxpal_palette[256 * 3];
uint8_t lbxpal_update_flag[256];
uint8_t lbxpal_colortable[0x18][256];

uint8_t *lbxpal_palette_inlbx = 0;
uint8_t *lbxpal_fontcolors = 0;
uint8_t *lbxpal_cursors = 0;

/* -------------------------------------------------------------------------- */

static uint8_t *lbxpal_ctableparam = 0;

static void lbxpal_build_colortable(int ctablei, uint8_t pr, uint8_t pg, uint8_t pb, uint8_t percent)
{
    uint8_t *tbl = lbxpal_colortable[ctablei];
    int min_error_i, min_error_v;
    int v = 100 - percent;
    if (v > 0) {
        if (v < 100) {
            uint8_t temp_scale = (v * 0x100) / 100;
            uint8_t temp_mul = (percent * 0x100) / 100;
            uint8_t r_add = (pr * temp_mul) >> 8;
            uint8_t g_add = (pg * temp_mul) >> 8;
            uint8_t b_add = (pb * temp_mul) >> 8;
            for (int k = 0; k < 256; ++k) {
                uint8_t *p;
                uint8_t r, g, b;
                p = &lbxpal_palette_inlbx[k * 3];
                r = (((*p++ * temp_scale) >> 8) + r_add) & 0xff;
                g = (((*p++ * temp_scale) >> 8) + g_add) & 0xff;
                b = (((*p++ * temp_scale) >> 8) + b_add) & 0xff;
                p = &lbxpal_palette_inlbx[0];
                min_error_i = 0;
                min_error_v = 0x2710;
                for (int j = 0; j < 256; ++j) {
                    uint16_t col_error;
                    v = *p++ - r;
                    if (v < 0) { v = -v; }
                    p += 2;
                    if (v >= 0x15) {
                        continue;
                    }
                    col_error = v * v;
                    p -= 2;
                    v = *p++ - g;
                    if (v < 0) { v = -v; }
                    p += 1;
                    if (v >= 0x15) {
                        continue;
                    }
                    col_error += v * v;
                    p -= 1;
                    v = *p++ - b;
                    if (v < 0) { v = -v; }
                    if (v >= 0x15) {
                        continue;
                    }
                    col_error += v * v;
                    if (col_error < min_error_v) {
                        min_error_v = col_error;
                        min_error_i = j;
                    }
                }
                tbl[k] = min_error_i;
            }
        } else {
            for (int i = 0; i < 256; ++i) {
                tbl[i] = i;
            }
        }
    } else {
        uint8_t *p;
        p = &lbxpal_palette_inlbx[0];
        min_error_i = 0;
        min_error_v = 0x2710;
        for (int j = 0; j < 256; ++j) {
            uint16_t col_error;
            v = *p++ - pr;
            if (v < 0) { v = -v; }
            p += 2;
            if (v >= 0x15) {
                continue;
            }
            col_error = v * v;
            p -= 2;
            v = *p++ - pg;
            if (v < 0) { v = -v; }
            p += 1;
            if (v >= 0x15) {
                continue;
            }
            col_error += v * v;
            p -= 1;
            v = *p++ - pb;
            if (v < 0) { v = -v; }
            if (v >= 0x15) {
                continue;
            }
            col_error += v * v;
            if (col_error < min_error_v) {
                min_error_v = col_error;
                min_error_i = j;
            }
        }
        memset(tbl, min_error_i, 256);
    }
}

/* -------------------------------------------------------------------------- */

void lbxpal_select(int pal_index, int first/*or -1*/, int last)
{
    uint8_t *pal = lbxfile_item_get(LBXFILE_FONTS, pal_index + 2);
    if (lbxpal_palette_inlbx) {
        lbxfile_item_release(LBXFILE_FONTS, lbxpal_palette_inlbx);
    }
    lbxpal_palette_inlbx = pal;
    lbxpal_fontcolors = pal + 0x300;
    lbxpal_cursors = pal + 0x500;
    lbxpal_ctableparam = pal + 0x1500;
    {
        int i, j, num;
        if (first < 0) {
            first = 0;
            num = 256;
        } else {
            num = last - first + 1;
        }
        i = first * 3;
        j = first;
        while (num) {
            uint8_t b;
            b = pal[i];
            if (b != lbxpal_palette[i]) {
                lbxpal_update_flag[j] = 1;
            }
            lbxpal_palette[i++] = b;
            b = pal[i];
            if (b != lbxpal_palette[i]) {
                lbxpal_update_flag[j] = 1;
            }
            lbxpal_palette[i++] = b;
            b = pal[i];
            if (b != lbxpal_palette[i]) {
                lbxpal_update_flag[j] = 1;
            }
            lbxpal_palette[i++] = b;
            ++j;
            --num;
        }
        /* not done in MOO */
        /*hw_video_set_palette(lbxpal_palette, 0, 256);*/
    }
    lbxfont_select(0, 0, 0, 0);
}

void lbxpal_set_palette(uint8_t *pal, int first, int num)
{
    memcpy(&lbxpal_palette[first * 3], pal, num * 3);
    lbxpal_set_update_range(first, first + num - 1);
}

void lbxpal_set_update_range(int from, int to)
{
    while (from <= to) {
        lbxpal_update_flag[from++] = 1;
    }
}

void lbxpal_update(void)
{
    memset(lbxpal_update_flag, 0, sizeof(lbxpal_update_flag));
    hw_video_set_palette(lbxpal_palette, 0, 256);
}

void lbxpal_build_colortables(void)
{
    lbxpal_build_colortable(0, 0, 0, 0, 0x32);
    for (int i = 1; i < 0x18; ++i) {
        uint8_t *p;
        p = lbxpal_ctableparam + i * 4;
        lbxpal_build_colortable(i, p[0], p[1], p[2], p[3]);
    }
}

uint8_t lbxpal_find_closest(uint8_t r, uint8_t g, uint8_t b)
{
    uint8_t min_c = 0;
    int min_dist = 10000;
    uint8_t *p = lbxpal_palette;
    for (int i = 0; i < 256; ++i) {
        int dist;
        dist = abs(r - *p++);
        dist += abs(g - *p++);
        dist += abs(b - *p++);
        if (dist < min_dist) {
            min_dist = dist;
            min_c = i;
            if (dist == 0) {
                break;
            }
        }
    }
    return min_c;
}

int lbxpal_init(void)
{
    memset(lbxpal_palette, 0, sizeof(lbxpal_palette));
    lbxpal_update();
    return 0;
}

void lbxpal_shutdown(void)
{
    if (lbxpal_palette_inlbx) {
        lbxfile_item_release(LBXFILE_FONTS, lbxpal_palette_inlbx);
        lbxpal_palette_inlbx = 0;
    }
}
