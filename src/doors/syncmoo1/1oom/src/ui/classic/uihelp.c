#include "config.h"

#include <stdlib.h>

#include "uihelp.h"
#include "bits.h"
#include "hw.h"
#include "lbx.h"
#include "lbxfont.h"
#include "lbxpal.h"
#include "log.h"
#include "uidraw.h"
#include "uidefs.h"
#include "uiobj.h"

/* -------------------------------------------------------------------------- */

#define DEBUGLEVEL_HELPUI   4

#define HELP_OFFS_10    0x10
#define HELP_OFFS_XTBL  0x2c
#define HELP_OFFS_YTBL  0x44
#define HELP_OFFS_WTBL  0x5c
#define HELP_OFFS_LXTBL 0x74
#define HELP_OFFS_LYTBL 0x8c
#define HELP_OFFS_LTTBL 0xa4
#define HELP_OFFS_STR0  0xbc
#define HELP_OFFS_STR1  0xe4
#define HELP_OFFS_STR2  0x21e
#define HELP_OFFS_STR3  0x246
#define HELP_OFFS_STBL  0x380
#define HELP_OFFS_NEXT  0x5f6
#define HELP_ITEM_SIZE  0x5f8
#define HELP_STBL_SIZE  70

/* -------------------------------------------------------------------------- */

static void ui_help_draw(const char *str0, const char *str1, int x, int y, int w, int ltype, int lx1, int ly1, const uint8_t *ctbl, uint8_t c0, uint8_t c1, uint8_t c2, uint8_t c3, uint8_t c4, uint8_t c5, uint8_t c6, uint8_t colorpos)
{
    int x1, y1;
    LOG_DEBUG((DEBUGLEVEL_HELPUI, "%s: cp:%i (%i,%i) w:%i lt:%i lx:%i ly:%i '%s' '%s'\n", __func__, colorpos, x, y, w, ltype, lx1, ly1, str0, str1));
    if (str1[0] == '\0') {
        return;
    }
    x1 = x + w - 1;
    y1 = lbxfont_calc_split_str_h(w - 12, str1) + y + 11;
    if (str0[0] != '\0') {
        y1 += lbxfont_get_height() + 4;
    }
    ui_draw_box_fill(x, y, x1, y1, ctbl, 0, 5, 1, colorpos, ui_scale);
    if (ltype != 0) {
        if ((lx1 >= x) && (lx1 <= x1)) {
            int lx0, ly0;
            lx0 = (x + x1) / 2;
            ly0 = (ly1 < y) ? (y - 1) : (y1 + 1);
            ui_draw_line1(lx0, ly0 + 1, lx1, ly1 + 1, 0, ui_scale);
            ui_draw_line1(lx0, ly0, lx1, ly1 + 1, c6, ui_scale);
        } else {
            int lx0, ly0;
            ly0 = (y + y1) / 2;
            if (lx1 < x) {
                if (ltype == 1) {
                    lx0 = x - 1;
                    ui_draw_line1(lx0, ly0 + 1, lx1, ly1 + 1, 0, ui_scale);
                    ui_draw_line1(lx0, ly0, lx1, ly1 + 1, c6, ui_scale);
                } else {
                    int v;
                    v = lx1 + abs(ly1 - ly0);
                    if (v > x) {
                        lx0 = x - 1;
                        ui_draw_line1(lx0, ly0 + 1, lx1, ly1 + 1, 0, ui_scale);
                        ui_draw_line1(lx0, ly0, lx1, ly1 + 1, c6, ui_scale);
                    } else {
                        ui_draw_line1(v, ly0 + 1, x - 1, ly0 + 1, 0, ui_scale);
                        ui_draw_line1(lx1, ly1 + 1, v, ly0 + 1, 0, ui_scale);
                        ui_draw_line1(v, ly0, x - 1, ly0, c6, ui_scale);
                        ui_draw_line1(lx1, ly1, v, ly0, c6, ui_scale);
                    }
                }
            } else {
                if (ltype == 1) {
                    lx0 = x1 + 1;
                    ui_draw_line1(lx0, ly0 + 1, lx1, ly1 + 1, 0, ui_scale);
                    ui_draw_line1(lx0, ly0, lx1, ly1 + 1, c6, ui_scale);
                } else {
                    int v;
                    v = lx1 - abs(ly1 - ly0);
                    if (v < x1) {
                        lx0 = x1 + 1;
                        ui_draw_line1(lx0, ly0 + 1, lx1, ly1 + 1, 0, ui_scale);
                        ui_draw_line1(lx0, ly0, lx1, ly1 + 1, c6, ui_scale);
                    } else {
                        ui_draw_line1(x1 + 1, ly0 + 1, v, ly0 + 1, 0, ui_scale);
                        ui_draw_line1(v, ly0 + 1, lx1, ly1 + 1, 0, ui_scale);
                        ui_draw_line1(x1 + 1, ly0, v, ly0, c6, ui_scale);
                        ui_draw_line1(v, ly0, lx1, ly1, c6, ui_scale);
                    }
                }
            }
        }
    }
    /*2e3d4*/
    ui_draw_box2(x, y, x1, y1, c0, c1, c2, c3, ui_scale);
    ui_draw_pixel(x, y, c4, ui_scale);
    ui_draw_pixel(x + 1, y + 1, c5, ui_scale);
    ui_draw_pixel(x + 2, y + 1, c5, ui_scale);
    ui_draw_pixel(x + 1, y + 2, c5, ui_scale);
    ui_draw_pixel(x + 1, y + 3, c5, ui_scale);
    ui_draw_line1(x + 1, y1 + 1, x1 + 1, y1 + 1, 0, ui_scale);
    ui_draw_line1(x1 + 1, y + 1, x1 + 1, y1, 0, ui_scale);
    if (str0[0] != '\0') {
        lbxfont_select_subcolors_13not1();
        lbxfont_print_str_center(x + w / 2, y + 6, str0, UI_SCREEN_W, ui_scale);
        lbxfont_select_subcolors_0();
        y += lbxfont_get_height() + 5;
    }
    /*2e4ff*/
    lbxfont_print_str_split(x + 6, y + 6, w - 12, str1, 0, UI_SCREEN_W, UI_SCREEN_H, ui_scale);
}

/* -------------------------------------------------------------------------- */

void ui_help(int help_index)
{
    uint8_t ctbl[5], c0, c1, c3, c4, c5, c8;
    uint8_t *helplbx;
    LOG_DEBUG((DEBUGLEVEL_HELPUI, "%s: %i\n", __func__, help_index));
    if ((help_index < 0) || (!ui_data.have_help)) {
        return;
    }
    for (int i = 0; i < 5; ++i) {
        ctbl[i] = lbxpal_find_closest(i * 3 + 7, i * 3 + 7, i * 4 + 15);
    }
    c0 = lbxpal_find_closest(0x3f, 0x3f, 0x3f);
    c1 = lbxpal_find_closest(0x31, 0x31, 0x31);
    /*c2 = c0 = lbxpal_find_closest(0x3f, 0x3f, 0x3f);*/
    c3 = lbxpal_find_closest(0x1d, 0x1d, 0x27);
    c4 = lbxpal_find_closest(0x27, 0x27, 0x2f);
    c5 = lbxpal_find_closest(0x19, 0x19, 0x23);
    /*c6 = c3 = lbxpal_find_closest(0x1d, 0x1d, 0x27);*/
    /*c7 = c4 = lbxpal_find_closest(0x27, 0x27, 0x2f);*/
    c8 = lbxpal_find_closest(0x2d, 0x2d, 0x33);
    uiobj_table_num_store();
    hw_video_copy_buf();
    hw_video_copy_back_to_page3();
    helplbx = lbxfile_item_get(LBXFILE_HELP, 0);
    do {
        const uint8_t *p;
        uint8_t old_fontnum, old_fonta2;
        old_fontnum = lbxfont_get_current_fontnum();
        old_fonta2 = lbxfont_get_current_fonta2();
        /* old_vgabuf_seg = vgabuf_seg; */
        lbxfont_select(0, 0, 0, 0);
        lbxfont_set_gap_h(3);
        lbxfont_set_14_24(c1, c0);
        p = &(helplbx[4 + HELP_ITEM_SIZE * help_index]);
        ui_help_draw((const char *)&p[HELP_OFFS_STR0], (const char *)&p[HELP_OFFS_STR1],
                    GET_LE_16(&p[HELP_OFFS_XTBL]),
                    GET_LE_16(&p[HELP_OFFS_YTBL]),
                    GET_LE_16(&p[HELP_OFFS_WTBL]),
                    GET_LE_16(&p[HELP_OFFS_LTTBL]),
                    GET_LE_16(&p[HELP_OFFS_LXTBL]),
                    GET_LE_16(&p[HELP_OFFS_LYTBL]),
                    ctbl, c3, c4, c5, c3, c4, c8, c0, 1
                    );
        if (GET_LE_16(&p[HELP_OFFS_10]) < 3) {
            for (int i = 0; i < 9; ++i) {
                ui_help_draw("", (const char *)&p[HELP_OFFS_STBL + i * HELP_STBL_SIZE],
                    GET_LE_16(&p[HELP_OFFS_XTBL + i * 2 + 4]),
                    GET_LE_16(&p[HELP_OFFS_YTBL + i * 2 + 4]),
                    GET_LE_16(&p[HELP_OFFS_WTBL + i * 2 + 4]),
                    GET_LE_16(&p[HELP_OFFS_LTTBL + i * 2 + 4]),
                    GET_LE_16(&p[HELP_OFFS_LXTBL + i * 2 + 4]),
                    GET_LE_16(&p[HELP_OFFS_LYTBL + i * 2 + 4]),
                    ctbl, c3, c4, c5, c3, c4, c8, c0, (i + 10040) & 0xff
                    );
            }
        } else {
            ui_help_draw((const char *)&p[HELP_OFFS_STR2], (const char *)&p[HELP_OFFS_STR3],
                    GET_LE_16(&p[HELP_OFFS_XTBL + 2]),
                    GET_LE_16(&p[HELP_OFFS_YTBL + 2]),
                    GET_LE_16(&p[HELP_OFFS_WTBL + 2]),
                    GET_LE_16(&p[HELP_OFFS_LTTBL + 2]),
                    GET_LE_16(&p[HELP_OFFS_LXTBL + 2]),
                    GET_LE_16(&p[HELP_OFFS_LYTBL + 2]),
                    ctbl, c3, c4, c5, c3, c4, c8, c0, 10040 & 0xff
                    );
        }
        lbxfont_select(old_fontnum, old_fonta2, 0, 0);
        /* vgabuf_seg = old_vgabuf_seg; */
        uiobj_finish_frame();
        ui_draw_copy_buf();
        uiobj_input_wait();
        hw_video_copy_back_from_page3();
        help_index = GET_LE_16(&p[HELP_OFFS_NEXT]);
    } while (help_index != 0);
    uiobj_table_num_restore();
    lbxfile_item_release(LBXFILE_HELP, helplbx);
}
