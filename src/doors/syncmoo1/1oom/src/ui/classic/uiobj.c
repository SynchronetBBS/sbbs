#include "config.h"

#include <string.h>

#include "uiobj.h"
#include "comp.h"
#include "game_types.h" /* only for UIOBJ_MAX */
#include "hw.h"
#include "kbd.h"
#include "lbxfont.h"
#include "lbxgfx.h"
#include "log.h"
#include "lib.h"
#include "mouse.h"
#include "types.h"
#include "uicursor.h"
#include "uidefs.h"
#include "uidelay.h"
#include "uidraw.h"
#include "uihelp.h"
#include "uipal.h"

/* -------------------------------------------------------------------------- */

/*
    WASBUG? MOO1 allocs 0x1650 bytes fitting 150 which is not always enough.
    Absolute max is planets + orbits + fleets + transports + some buttons.
    This is overkill for actual gameplay but we are not limited by 1993 memory sizes.
*/
#define UIOBJ_MAX   (PLANETS_MAX * 7 + FLEET_ENROUTE_MAX + TRANSPORT_MAX + 32)

#define UIOBJ_OFFSCREEN (320 * UI_SCALE_MAX + 100)

typedef enum {
    UIOBJ_TYPE_BUTTON = 0,
    UIOBJ_TYPE_TOGGLE = 1,
    UIOBJ_TYPE_SET = 2,
    UIOBJ_TYPE_SETVAL = 3,
    UIOBJ_TYPE_TEXTINPUT = 4,
    UIOBJ_TYPE_SLIDER = 6,
    UIOBJ_TYPE_MOUSEAREA = 7,
    UIOBJ_TYPE_ALTSTR = 8,
    UIOBJ_TYPE_TEXTLINE = 0xa,
    UIOBJ_TYPE_SCROLLAREA = 0xb,
    UIOBJ_TYPE_WHEELAREA = 0xc
} uiobj_type_t;

typedef struct uiobj_s {
    /*00*/ uint16_t x0;
    /*02*/ uint16_t y0;
    /*04*/ uint16_t x1;    /* inclusive */
    /*06*/ uint16_t y1;    /* inclusive */
    /* 13 types
        t0..3: buttons with gfx (and optional text) with different highlight conditions
        t1: toggle
        t2: set1
        t3: setval
        t4: textinput
        t6: slider
        t7: mousearea or inputkey
        t8: altstr
        ta: text line
        tb: scroll area
        tc: mouse wheel area
    */
    /*08*/ uint8_t type;
    /*..*/ uint8_t scale;
    /*1a*/ int16_t *vptr;
    /*24*/ uint32_t key;
    union {
        struct {
            /*16*/ const char *str;
            /*20*/ uint8_t *lbxdata;
            /*18*/ int16_t z18; /* t3:uiobji? */
            /*0c*/ uint8_t fontnum; /* t1..3? */
            /*0e*/ uint8_t fonta2; /* t1..3? */
            /*1c*/ bool indep;  /* from lbxgfx offs 0x10 */
        } t0; /* t{0..3} */
        struct {
            /*16*/ char *buf;
            /*22*/ const uint8_t *colortbl;
            /*20*/ uint16_t max_chars;
            /*0c*/ uint8_t fontnum;
            /*0e*/ uint8_t fonta2;
            /*14*/ uint8_t fonta4;
            /*1a*/ uint8_t rectcolor;
            /*1c*/ bool align_right;
            /*..*/ bool allow_lcase;
        } t4;
        struct {
            /*..*/ void (*cb)(void *ctx, uint8_t slideri, int16_t value);
            /*..*/ void *ctx;
            /*1c*/ uint16_t vmin;
            /*1e*/ uint16_t vmax;
            /*..*/ uint8_t slideri;
        } t6;
        struct {
            /*16*/ const char *str;
            /*18*/ uint16_t pos;
            /*1a*/ uint16_t len;
        } t8;
        struct {
            /*16*/ const char *str;
            /*18*/ int16_t z18;
            /*0c*/ uint8_t fontnum;
            /*0e*/ uint8_t fonta2;
            /*14*/ uint8_t fonta2b;
            /*10*/ uint8_t subtype; /* 0, 1, 0xf */
            /*1c*/ uint8_t sp0v;
            /*12*/ bool z12;
        } ta;
        struct {
            /*1c*/ int16_t *xptr;
            /*1e*/ int16_t *yptr;
            /*..*/ uint8_t *zptr;
            /*18*/ uint16_t xdiv;
            /*1a*/ uint16_t ydiv;
            /*..*/ uint8_t zmax;
        } tb;
    };
} uiobj_t;

/* -------------------------------------------------------------------------- */

static uint16_t uiobj_table_num = 0;
static uint16_t uiobj_table_num_old = 0;
static int16_t uiobj_focus_oi = -1;
static int16_t uiobj_clicked_oi = 0;
static int uiobj_xoff = 1;
static int uiobj_yoff = -1;
static int16_t uiobj_mouseoff = 0;
static int16_t uiobj_handle_downcount = 0;
static int16_t uiobj_kbd_alt_oi = 0;
static uint16_t uiobj_delay = 2;
static int16_t uiobj_help_id = -1;
static bool uiobj_flag_select_list_active = false;
static bool uiobj_flag_select_list_multipage = false;
static int16_t uiobj_kbd_movey = -1;
static bool uiobj_flag_skip_delay = false;

static bool uiobj_flag_have_cb = false;
static void (*uiobj_callback)(void *) = NULL;
static void *uiobj_cbdata = NULL;

static uiobj_t uiobj_tbl[UIOBJ_MAX];

/* -------------------------------------------------------------------------- */

int uiobj_minx = 0;
int uiobj_miny = 0;
int uiobj_maxx = 0;
int uiobj_maxy = 0;

/* -------------------------------------------------------------------------- */

static int16_t uiobj_alloc(void)
{
    if (uiobj_table_num < (UIOBJ_MAX - 1)) {
        return uiobj_table_num++;
    } else {
        log_fatal_and_die("uiobj_table size exceeded");
    }
}

static int smidx(const uiobj_t *p)
{
    return p->x0 + (p->x1 - p->x0) / 2;
}

static int smidy(const uiobj_t *p)
{
    return p->y0 + (p->y1 - p->y0) / 2;
}

static int scmidx(const uiobj_t *p)
{
    return (p->x0 + (p->x1 - p->x0) / 2) * p->scale;
}

static int scmidy(const uiobj_t *p)
{
    return (p->y0 + (p->y1 - p->y0) / 2) * p->scale;
}

static int smidtexty(const uiobj_t *p)
{
    int16_t v = lbxfont_get_height();
    --v;
    if (v < 0) {
        ++v;
    }
    v /= 2;
    return smidy(p) - v;
}

static inline bool uiobj_is_at_xy(const uiobj_t *p, int x, int y)
{
    uint8_t scale;
    if (p->x0 == UIOBJ_OFFSCREEN) {
        return false;
    }
    x += uiobj_mouseoff;
    y += uiobj_mouseoff;
    scale = p->scale;
    if (0
      || (x < (p->x0 * scale))
      || (x > (p->x1 * scale + scale - 1))
      || (y < (p->y0 * scale))
      || (y > (p->y1 * scale + scale - 1))
    ) {
        return false;
    }
    return true;
}

static void uiobj_handle_t03_cond(uiobj_t *p, bool cond)
{
    if (cond) {
        lbxgfx_set_frame_0(p->t0.lbxdata);
        lbxgfx_draw_frame(p->x0, p->y0, p->t0.lbxdata, UI_SCREEN_W, p->scale);
        lbxfont_select(p->t0.fontnum, p->t0.fonta2, 0, 0);
        lbxfont_print_str_center(smidx(p), smidtexty(p), p->t0.str, UI_SCREEN_W, p->scale);
    } else {
        if (p->t0.indep == 0) {
            lbxgfx_set_frame_0(p->t0.lbxdata);
            lbxgfx_draw_frame(p->x0, p->y0, p->t0.lbxdata, UI_SCREEN_W, p->scale);
        } else {
            lbxgfx_set_new_frame(p->t0.lbxdata, 1);
        }
        lbxgfx_draw_frame(p->x0, p->y0, p->t0.lbxdata, UI_SCREEN_W, p->scale);
        lbxfont_select(p->t0.fontnum, p->t0.fonta2, 0, 0);
        lbxfont_print_str_center(smidx(p) + uiobj_xoff, smidtexty(p) + uiobj_yoff, p->t0.str, UI_SCREEN_W, p->scale);
    }
}

static void uiobj_handle_t4_sub2(uiobj_t *p, uint16_t len, uint16_t a4, const char *str)
{
    char strbuf[64];
    int16_t si, va;
    si = a4;
    ui_delay_prepare();
    lib_strcpy(strbuf, str, sizeof(strbuf));
    uiobj_do_callback();
    /*ve = p->x1 - p->x0;*/
    lbxfont_select(p->t4.fontnum, p->t4.fonta2, p->t4.fonta4, 0);
    va = lbxfont_get_height() - 1;
    if (p->t4.rectcolor != 0) {
        ui_draw_filled_rect(p->x0, p->y0, p->x1, p->y1, p->t4.rectcolor, p->scale);
    }
    {
        uint16_t l, w, x, vc;
        char c;
        c = strbuf[len];
        strbuf[len] = 0;
        x = lbxfont_calc_str_width(strbuf) + p->x0;
        strbuf[len] = c;
        w = lbxfont_get_char_w(c ? c : ' ');
        if ((si > 0) && (si <= va)) {
            vc = p->y0 + va;
            l = 0;
            while (si > 0) {
                ui_draw_line1(x, vc - si + 1, x + w + 1, vc - si + 1, p->t4.colortbl[l], p->scale);
                ++l;
                --si;
            }
        } else if (si != 0) {
            si = va - (si - va);
            l = 0;
            while (si > 0) {
                ui_draw_line1(x, p->y0 + si - 1, x + w + 1, p->y0 + si - 1, p->t4.colortbl[va - l - 1], p->scale);
                ++l;
                --si;
            }
        }
    }
    lbxfont_select_subcolors_13not1();
    lbxfont_print_str_normal(p->x0, p->y0, strbuf, UI_SCREEN_W, p->scale);
    ui_palette_set_n();
    uiobj_finish_frame();
    ui_delay_ticks_or_click(uiobj_delay);
}

static bool uiobj_textinput_do(uiobj_t *p, int w, char *buf, int max_chars, bool allow_lcase, bool copy_truncated)
{
    char strbuf[64];
    int len, pos, fonth, animpos = 0;
    bool flag_mouse_button = false, flag_quit = false, flag_got_first = false;
    mookey_t key = MOO_KEY_UNKNOWN;

    hw_textinput_start();

    lib_strcpy(strbuf, buf, sizeof(strbuf));
    len = strlen(strbuf);
    if (lbxfont_calc_str_width(strbuf) > w) {
        if (len != 0) {
            len = 0;
            strbuf[len] = '\0';
        }
    }
    pos = len;
    if (pos >= max_chars) {
        pos = max_chars;
    }
    if (copy_truncated) {
        lib_strcpy(buf, strbuf, (size_t)max_chars + 1);
    }
    fonth = lbxfont_get_height();
    uiobj_handle_t4_sub2(p, pos, animpos, strbuf);

    while ((key != MOO_KEY_RETURN) && (!flag_mouse_button)) {
        uint32_t keyp;
        bool flag_ok;
        char c;
        while (!(kbd_have_keypress() || flag_mouse_button)) {
            hw_event_handle();
            if ((1/*mouse_flag_initialized*/) && (mouse_buttons || (mouse_getclear_click_hw() != 0))) {
                flag_mouse_button = true;
                break;
            } else {
                ++animpos;
                if (((fonth << 1) - 1) < animpos) {
                    animpos = 0;
                }
                uiobj_handle_t4_sub2(p, pos, animpos, strbuf);
            }
        }
        if (flag_mouse_button) {
            break;
        }
        keyp = kbd_get_keypress();
        key = KBD_GET_KEY(keyp);
        c = KBD_GET_CHAR(keyp);
        switch (key) {
            case MOO_KEY_BACKSPACE:
                if (!flag_got_first) {
                    strbuf[0] = '\0';
                    len = pos = 0;
                    animpos = 0;
                    flag_got_first = true;
                } else if (len > 0) {
                    if (pos >= len) {
                        --len;
                        --pos;
                        animpos = 0;
                    } else if (pos > 0) {
                        for (int i = pos; i < len; ++i) {
                            strbuf[i - 1] = strbuf[i];
                        }
                        --len;
                        --pos;
                    }
                    strbuf[len] = '\0';
                }
                break;
            case MOO_KEY_DELETE:
                if ((len > 0) && (pos < len)) {
                    for (int i = pos; i < len; ++i) {
                        strbuf[i] = strbuf[i + 1];
                    }
                    --len;
                    animpos = 0;
                    strbuf[len] = '\0';
                }
                break;
            case MOO_KEY_LEFT:
                flag_got_first = true;
                if (pos > 0) {
                    --pos;
                    animpos = 0;
                }
                break;
            case MOO_KEY_RIGHT:
                if ((pos < max_chars) && (pos < len)) {
                    ++pos;
                    animpos = 0;
                    if (pos >= len) {
                        strbuf[len] = ' ';
                        strbuf[len + 1] = '\0';
                        if ((pos >= max_chars) || (lbxfont_calc_str_width(strbuf) > w)) {
                            --pos;
                        }
                        strbuf[len] = '\0';
                    }
                }
                break;
            case MOO_KEY_ESCAPE:
                flag_mouse_button = true;
                flag_quit = true;
                break;
            default:
                flag_ok = false;
                if ((!allow_lcase) && (c >= 'a') && (c <= 'z')) {
                    c -= 0x20;    /* az -> AZ */
                }
                if (0
                  || ((c >= 'A') && (c < ']'))
                  || (allow_lcase && (c >= 'a') && (c < '{'))
                  || ((c >= '-') && (c < ';'))
                  || (c == ' ') || (c == '-')
                ) {
                    flag_ok = true;
                }
                if (flag_ok) {
                    flag_got_first = true;
                    strbuf[len] = c;
                    strbuf[len + 1] = '\0';
                    if ((len < max_chars) && (lbxfont_calc_str_width(strbuf) <= w)) {
                        strbuf[len] = '\0';
                        if (pos < len) {
                            for (int i = len; i > pos; --i) {
                                strbuf[i] = strbuf[i - 1];
                            }
                            ++len;
                            strbuf[pos] = c;
                            ++pos;
                        } else {
                            strbuf[len] = c;
                            ++len;
                            strbuf[len] = ' ';
                            strbuf[len + 1] = '\0';
                            if ((len < max_chars) && (lbxfont_calc_str_width(strbuf) <= w)) {
                                ++pos;
                            }
                        }
                        strbuf[len] = '\0';
                        animpos = 0;
                    } else {
                        strbuf[len] = '\0';
                    }
                }
                break;
        }
        uiobj_handle_t4_sub2(p, pos, animpos, strbuf);
    }
    hw_textinput_stop();
    lib_strcpy(buf, strbuf, (size_t)max_chars + 1);
    if (flag_mouse_button) /*&& (mouse_flag_initialized)*/ {
        while (mouse_buttons) {
            hw_event_handle();
        }
    }
    /* TODO ui_cursor_erase0(); */
    uiobj_focus_oi = -1;
    mouse_getclear_click_hw();
    mouse_getclear_click_sw();
    return flag_quit;
}

static void uiobj_handle_t4_sub1(uiobj_t *p)
{
    int w;
    while (mouse_buttons) {
        hw_event_handle();
        uiobj_do_callback();
    }
    w = p->x1 - p->x0;
    lbxfont_select(p->t4.fontnum, p->t4.fonta2, p->t4.fonta4, 0);
    uiobj_textinput_do(p, w, p->t4.buf, p->t4.max_chars, p->t4.allow_lcase, true);
}

static void uiobj_handle_t6_slider_input(uiobj_t *p)
{
    int16_t sliderval, slideroff, di;
    di = moouse_x + uiobj_mouseoff;
    slideroff = ((p->t6.vmax - p->t6.vmin) * (di - p->x0 * p->scale)) / ((p->x1 - p->x0) * p->scale);
    if (p->x1 * p->scale <= di) {
        sliderval = p->t6.vmax;
    } else if (p->x0 >= di) {
        sliderval = p->t6.vmin;
    } else {
        sliderval = p->t6.vmin + slideroff;
    }
    SETMAX(sliderval, p->t6.vmin);
    SETMIN(sliderval, p->t6.vmax);
    *p->vptr = sliderval;
    if (p->t6.cb) {
        p->t6.cb(p->t6.ctx, p->t6.slideri, sliderval);
    }
}

static void uiobj_handle_ta_sub1(int x0, int y0, int x1, int y1, uint8_t subtype, uint8_t p0v, int scale)
{
    switch (subtype) {
        case 1:
            ui_draw_filled_rect(x0, y0, x1, y1, p0v, scale);
            break;
        case 0xf:
            lbxgfx_apply_colortable(x0, y0, x1, y1, p0v, UI_SCREEN_W, scale);
            break;
        default:
            break;
    }
}

/* not a function in MOO1 but part of uiobj_handle_objects */
static inline void uiobj_handle_objects_sub1(int i)
{
    uiobj_t *p = &uiobj_tbl[i];
    switch (p->type) {
        case UIOBJ_TYPE_BUTTON:
            uiobj_handle_t03_cond(p, true);
            break;
        case UIOBJ_TYPE_TOGGLE:
            uiobj_handle_t03_cond(p, *p->vptr == 0);
            break;
        case UIOBJ_TYPE_SET:
            uiobj_handle_t03_cond(p, *p->vptr == 0);
            break;
        case UIOBJ_TYPE_SETVAL:
            uiobj_handle_t03_cond(p, *p->vptr != p->t0.z18);
            break;
        case UIOBJ_TYPE_TEXTLINE:
            lbxfont_select(p->ta.fontnum, p->ta.fonta2, 0, 0);
            if (*p->vptr != p->ta.z18) {
                if (!p->ta.z12) {
                    /* ?? what is the point in this second call? */
                    lbxfont_select(p->ta.fontnum, p->ta.fonta2b, 0, 0);
                }
                /*19ca3*/
                lbxfont_print_str_normal(p->x0, p->y0 + 1, p->ta.str, UI_SCREEN_W, p->scale);
            } else {
                int16_t gap_h, char_h;
                gap_h = lbxfont_get_gap_h();
                if (gap_h < 0) { ++gap_h; }
                gap_h /= 2;
                if (gap_h == 0) {
                    gap_h = 1;
                }
                char_h = lbxfont_get_height();
                /*19ce2*/
                uiobj_handle_ta_sub1(p->x0 - 1, p->y0 - gap_h + 1, p->x1, p->y0 + char_h + 1, p->ta.subtype, p->ta.sp0v, p->scale);
                lbxfont_print_str_normal(p->x0, p->y0 + 1, p->ta.str, UI_SCREEN_W, p->scale);
            }
            break;
        case UIOBJ_TYPE_TEXTINPUT:
            if (uiobj_focus_oi != i) {
                lbxfont_select(p->t4.fontnum, p->t4.fonta2, p->t4.fonta4, 0);
                ui_draw_filled_rect(p->x0, p->y0, p->x1, p->y1, p->t4.rectcolor, p->scale);
                if (!p->t4.align_right) {
                    lbxfont_print_str_normal(p->x0, p->y0, p->t4.buf, UI_SCREEN_W, p->scale);
                } else {
                    lbxfont_print_str_right(p->x1, p->y0, p->t4.buf, UI_SCREEN_W, p->scale);
                }
            }
            break;
        case UIOBJ_TYPE_SLIDER:
            {
                uint16_t v = *p->vptr;
                SETMAX(v, p->t6.vmin);
                SETMIN(v, p->t6.vmax);
                *p->vptr = v;
            }
            break;
        default:
            break;
    }
}

static void uiobj_handle_click(int i, bool in_focus)
{
    uiobj_t *p = &uiobj_tbl[i];
    switch (p->type) {
        case UIOBJ_TYPE_BUTTON:
            uiobj_handle_t03_cond(p, !in_focus);
            break;
        case UIOBJ_TYPE_TOGGLE:
            uiobj_handle_t03_cond(p, (!in_focus) || (*p->vptr == 1));
            break;
        case UIOBJ_TYPE_SET:
            uiobj_handle_t03_cond(p, (!in_focus) && (*p->vptr == 0));
            break;
        case UIOBJ_TYPE_SETVAL:
            if (!in_focus) {
                *p->vptr = UIOBJI_INVALID; /* TODO or other 0xfc18? */
            } else {
                *p->vptr = p->t0.z18;
            }
            uiobj_handle_t03_cond(p, *p->vptr != p->t0.z18);
            break;
        case UIOBJ_TYPE_TEXTLINE:
            if (!in_focus) {
                *p->vptr = 0;
            } else if (p->ta.z12) {
                *p->vptr = p->ta.z18;
            }
            lbxfont_select(p->ta.fontnum, p->ta.fonta2, 0, 0);
            if (*p->vptr != p->ta.z18) {
                if (p->ta.z12) {
                    lbxfont_print_str_normal(p->x0, p->y0, p->ta.str, UI_SCREEN_W, p->scale);
                } else {
                    lbxfont_select(p->ta.fontnum, p->ta.fonta2b, 0, 0);
                    lbxfont_print_str_normal(p->x0, p->y0 + 1, p->ta.str, UI_SCREEN_W, p->scale);
                    lbxfont_select_subcolors_0();
                }
            } else {
                int16_t char_h, gap_h;
                gap_h = lbxfont_get_gap_h();
                if (gap_h < 0) { ++gap_h; }
                gap_h /= 2;
                if (gap_h == 0) {
                    gap_h = 1;
                }
                char_h = lbxfont_get_height();
                uiobj_handle_ta_sub1(p->x0 - 1, p->y0 - gap_h + 1, p->x1, p->y0 + char_h + 1, p->ta.subtype, p->ta.sp0v, p->scale);
                lbxfont_print_str_normal(p->x0, p->y0 + 1, p->ta.str, UI_SCREEN_W, p->scale);
            }
            break;
        case UIOBJ_TYPE_SCROLLAREA:
            if (in_focus) {
                *p->tb.xptr = (moouse_x - p->x0) / p->tb.xdiv;
                *p->tb.yptr = (moouse_y - p->y0) / p->tb.ydiv;
            }
            break;
        case UIOBJ_TYPE_SLIDER:
            /* MOO1 checks for in_focus here which makes dragging the slider to min/max needlessly hard */
            uiobj_handle_t6_slider_input(p);
            break;
        case UIOBJ_TYPE_TEXTINPUT:
            uiobj_handle_t4_sub1(p);
            break;
        default:
            break;
    }
}

static int16_t uiobj_kbd_dir_key_dy_list(int diry)
{
    int16_t oi2 = uiobj_at_cursor();
    int16_t oi = oi2;
    uiobj_t *p;
    if (oi != 0) {
        if (diry == 1) {
            while ((++oi < (uiobj_table_num - 1)) && (uiobj_tbl[oi].type == UIOBJ_TYPE_TEXTLINE)) {
                if (uiobj_tbl[oi].ta.z12) {
                    break;
                }
            }
            if (!((oi < (uiobj_table_num - 1)) && (uiobj_tbl[oi].type == UIOBJ_TYPE_TEXTLINE))) {
                if (uiobj_flag_select_list_multipage) {
                    oi = oi2;
                    uiobj_kbd_movey = 1;
                } else {
                    oi = 0;
                    while (oi < uiobj_table_num) {
                        ++oi;
                        if (/*not tested in MOO1!*/(uiobj_tbl[oi].type == UIOBJ_TYPE_TEXTLINE) && uiobj_tbl[oi].ta.z12) {
                            break;
                        }
                    }
                    if (oi >= uiobj_table_num) {
                        oi = oi2;
                    }
                }
            }
        } else {
            if (uiobj_flag_select_list_multipage && (oi == 1)) {
                uiobj_kbd_movey = -1;
                oi = 1;
            } else {
                if (oi > 1) {
                    --oi;
                } else {
                    oi = uiobj_table_num - 1 - 1;
                }
                while (oi && (uiobj_tbl[oi].type != UIOBJ_TYPE_TEXTLINE)) {
                    --oi;
                }
                while (oi && /*not tested in MOO1!*/(uiobj_tbl[oi].type == UIOBJ_TYPE_TEXTLINE) && !uiobj_tbl[oi].ta.z12) {
                    --oi;
                }
                if (oi <= 0) {
                    if (uiobj_flag_select_list_multipage) {
                        uiobj_kbd_movey = -1;
                        oi = 1;
                    } else {
                        oi = uiobj_table_num - 1 - 1;
                        while (oi && /*not tested in MOO1!*/(uiobj_tbl[oi].type == UIOBJ_TYPE_TEXTLINE) && uiobj_tbl[oi].ta.z12) {
                            --oi;
                        }
                        while (oi && /*not tested in MOO1!*/(uiobj_tbl[oi].type == UIOBJ_TYPE_TEXTLINE) && !uiobj_tbl[oi].ta.z12) {
                            --oi;
                        }
                        if (oi == 0) {
                            oi = oi2;
                        }
                    }
                }
            }
        }
    } else {
        p = &uiobj_tbl[1];
        if (p->vptr && (*p->vptr >= 0)) {
            oi2 = *p->vptr + 1;
            if (oi2 >= uiobj_table_num) {
                oi2 = 0;
            }
            oi = oi2;
            if (diry == 1) {
                while ((++oi < (uiobj_table_num - 1)) && (uiobj_tbl[oi].type == UIOBJ_TYPE_TEXTLINE)) {
                    if (uiobj_tbl[oi].ta.z12) {
                        break;
                    }
                }
                if (!((oi < uiobj_table_num) && (uiobj_tbl[oi].type == UIOBJ_TYPE_TEXTLINE))) {
                    if (uiobj_flag_select_list_multipage) {
                        uiobj_kbd_movey = 1;
                    } else if (oi < uiobj_table_num) {
                        oi = 1;
                        while (oi && /*not tested in MOO1!*/(uiobj_tbl[oi].type == UIOBJ_TYPE_TEXTLINE) && !uiobj_tbl[oi].ta.z12) {
                            ++oi;
                        }
                        if (oi >= uiobj_table_num) {
                            oi = oi2;
                        }
                    }
                }
            } else {
                if ((oi == 1) && uiobj_flag_select_list_multipage) {
                    uiobj_kbd_movey = -1;
                } else {
                    if (oi <= 1) {
                        oi = uiobj_table_num - 1 - 1;
                    } else {
                        --oi;
                    }
                    while (oi && /*not tested in MOO1!*/(uiobj_tbl[oi].type == UIOBJ_TYPE_TEXTLINE) && !uiobj_tbl[oi].ta.z12) {
                        --oi;
                    }
                    if (oi == 0) {
                        oi = uiobj_table_num - 1 - 1;
                        while (oi && /*not tested in MOO1!*/(uiobj_tbl[oi].type == UIOBJ_TYPE_TEXTLINE) && !uiobj_tbl[oi].ta.z12) {
                            --oi;
                        }
                        if (oi == 0) {
                            oi = oi2;
                        }
                    }
                }
            }
        } else {
            for (oi = 1; oi < uiobj_table_num; ++oi) {
                if (/*not tested in MOO1!*/(uiobj_tbl[oi].type == UIOBJ_TYPE_TEXTLINE) && uiobj_tbl[oi].ta.z12) {
                    break;
                }
            }
            if (oi >= uiobj_table_num) {
                oi = 0;
            }
        }
    }
    if ((oi < 0) || (oi >= uiobj_table_num)) {
        oi = 0;
    }
    if (oi > 0) {
        p = &uiobj_tbl[oi];
        mouse_stored_x = scmidx(p);
        mouse_stored_y = scmidy(p);
        if ((moouse_x != mouse_stored_x) || (moouse_y != mouse_stored_y)) {
            ui_cursor_update_gfx_i(mouse_stored_x, mouse_stored_y);
            uiobj_mouseoff = ui_cursor_mouseoff;
            mouse_stored_x -= uiobj_mouseoff;
            mouse_stored_y -= uiobj_mouseoff;
            hw_mouse_set_xy(mouse_stored_x, mouse_stored_y);
            if (p->type == UIOBJ_TYPE_TEXTLINE) {
                *p->vptr = p->ta.z18;
            }
        }
    }
    return oi;
}

static inline bool uiobj_kbd_dir_obj_ok(const uiobj_t *p)
{
    return ((p->type < UIOBJ_TYPE_SCROLLAREA) && (p->x0 != UIOBJ_OFFSCREEN) && ((p->type != UIOBJ_TYPE_TEXTLINE) || p->ta.z12));
}

static int16_t uiobj_kbd_dir_key_dxdy(int dirx, int diry, int16_t oi2, int mx, int my)
{
    int dx = UIOBJ_OFFSCREEN, dy = UIOBJ_OFFSCREEN;
    int slope = UIOBJ_OFFSCREEN;
    int mind = UIOBJ_OFFSCREEN * UIOBJ_OFFSCREEN;
    int dist;
    int16_t oi = oi2;
    uiobj_t *p;
    if (ui_fixbugs_enabled) {
        mx += ui_cursor_mouseoff;
        my += ui_cursor_mouseoff;
    }
    {
        bool flag_found = false;
        for (int i = 1; i < uiobj_table_num; ++i) {
            p = &uiobj_tbl[i];
            if (uiobj_kbd_dir_obj_ok(p)) {
                flag_found = true;
                break;
            }
        }
        if (!flag_found) {
            return UIOBJI_NONE;
        }
    }
    if ((diry != 0) && (dirx == 0)) {
        for (int i = 1; i < uiobj_table_num; ++i) {
            if (ui_kbd_cursor_keys_fix) {
                break;
            }
            if (i == oi2) {
                if (i != (uiobj_table_num - 1)) {
                    ++i;
                } else {
                    break;
                }
            }
            p = &uiobj_tbl[i];
            if (uiobj_kbd_dir_obj_ok(p)) {
                dy = (diry < 0) ? (my - scmidy(p)) : (scmidy(p) - my);
                dx = scmidx(p) - mx;
                if ((p->x0 * p->scale <= mx) && (p->x1 * p->scale >= mx) && ((dx < -6 * p->scale) || (dx > 6 * p->scale))) {
                    dx = 6 * p->scale;
                }
                if ((dx > -6 * p->scale) && (dx < 6 * p->scale) && (dy > 0) && (dy < mind)) {
                    mind = dy;
                    oi = i;
                }
            }
        }
        if (oi == oi2 || ui_kbd_cursor_keys_fix) {
            for (int i = 1; i < uiobj_table_num; ++i) {
                if (i == oi2) {
                    if (i != (uiobj_table_num - 1)) {
                        ++i;
                    } else {
                        break;
                    }
                }
                p = &uiobj_tbl[i];
                if (uiobj_kbd_dir_obj_ok(p)) {
                    dy = (diry < 0) ? (my - scmidy(p)) : (scmidy(p) - my);
                    dx = scmidx(p) - mx;
                    if (!ui_kbd_cursor_keys_fix && (p->x0 * p->scale <= mx) && (p->x1 * p->scale >= mx) && ((dx < -6 * p->scale) || (dx > 6 * p->scale))) {
                        dx = 6 * p->scale;
                    }
                    if (dx < 0) {
                        dx = -dx;
                    }
                    if (dy < 0) {
                        dx = UIOBJ_OFFSCREEN;
                    }
                    if (dy == 0) {
                        if (ui_kbd_cursor_keys_fix) {
                            continue;
                        } else {
                            dy = 1;
                        }
                    }
                    slope = (dx * 100) / dy;
                    if ((slope >= 0) && (slope < 0x2c)) {
                        dist = (dx * dx) + (dy * dy);
                        if (dist < mind) {
                            mind = dist;
                            oi = i;
                        }
                    }
                }
            }
        }
    }
    if ((dirx != 0) && (diry == 0)) {
        for (int i = 1; i < uiobj_table_num; ++i) {
            if (ui_kbd_cursor_keys_fix) {
                break;
            }
            if (i == oi2) {
                if (i != (uiobj_table_num - 1)) {
                    ++i;
                } else {
                    break;
                }
            }
            p = &uiobj_tbl[i];
            if (uiobj_kbd_dir_obj_ok(p)) {
                dx = (dirx < 0) ? (mx - scmidx(p)) : (scmidx(p) - mx);
                dy = scmidy(p) - my;
                if ((p->y0 * p->scale <= my) && (p->y1 * p->scale >= my) && ((dy * p->scale < -6) || (dy * p->scale > 6))) {
                    dy = 6 * p->scale;
                }
                if ((dy > -6 * p->scale) && (dy < 6 * p->scale) && (dx > 0) && (dx < mind)) {
                    mind = dx;
                    oi = i;
                }
            }
        }
        if (oi == oi2 || ui_kbd_cursor_keys_fix) {
            for (int i = 1; i < uiobj_table_num; ++i) {
                if (i == oi2) {
                    if (i != (uiobj_table_num - 1)) {
                        ++i;
                    } else {
                        break;
                    }
                }
                p = &uiobj_tbl[i];
                if (uiobj_kbd_dir_obj_ok(p)) {
                    dx = (dirx < 0) ? (mx - scmidx(p)) : (scmidx(p) - mx);
                    dy = scmidy(p) - my;
                    if (!ui_kbd_cursor_keys_fix && (p->y0 * p->scale <= my) && (p->y1 * p->scale >= my) && ((dy < -6 * p->scale) || (dy > 6 * p->scale))) {
                        dy = 6 * p->scale;
                    }
                    if (dy < 0) {
                        dy = -dy;
                    }
                    if (dx < 0) {
                        dy = UIOBJ_OFFSCREEN;
                    }
                    if (dx == 0) {
                        if (ui_kbd_cursor_keys_fix) {
                            continue;
                        } else {
                            dx = 1;
                        }
                    }
                    slope = (dy * 100) / dx;
                    if ((slope >= 0) && (slope < 0x2c)) {
                        dist = (dx * dx) + (dy * dy);
                        if (dist < mind) {
                            mind = dist;
                            oi = i;
                        }
                    }
                }
            }
        }
    }
    if ((dirx != 0) && (diry != 0)) {
        for (int i = 1; i < uiobj_table_num; ++i) {
            if (i == oi2) {
                if (i != (uiobj_table_num - 1)) {
                    ++i;
                } else {
                    break;
                }
            }
            p = &uiobj_tbl[i];
            if (uiobj_kbd_dir_obj_ok(p)) {
                dx = (dirx < 0) ? (mx - scmidx(p)) : (scmidx(p) - mx);
                dy = (diry < 0) ? (my - scmidy(p)) : (scmidy(p) - my);
                if ((dx < 0) || (dy < 0)) {
                    slope = UIOBJ_OFFSCREEN;
                    continue;
                }
                if ((dx >= dy) && (dy != 0)) {
                    slope = (dy * 100) / dx;
                }
                if ((dy > dx) && (dx != 0)) {
                    slope = (dx * 100) / dy;
                }
                if ((dx == 0) || (dy == 0)) {
                    slope = UIOBJ_OFFSCREEN;
                }
                if ((slope >= 34) && (slope <= 105)) {
                    dist = (dx * dx) + (dy * dy);
                    if (dist < mind) {
                        mind = dist;
                        oi = i;
                    }
                }
            }
        }
    }
    return oi;
}

static int16_t uiobj_kbd_dir_key(int dirx, int diry)
{
    if (uiobj_flag_select_list_active && (diry != 0)) {
        return uiobj_kbd_dir_key_dy_list(diry);
    } else {
        int mx, my;
        int16_t oi, oi2;
        uiobj_t *p;
        if (1/*mouse_initialized*/) {
            mx = moouse_x;
            my = moouse_y;
        }  else {
            mx = mouse_stored_x;
            my = mouse_stored_y;
        }
        oi2 = 0;
        oi = uiobj_table_num - 1;
        while (oi > 0) {
            p = &uiobj_tbl[oi];
            if (p->type < UIOBJ_TYPE_SCROLLAREA) {
                if (uiobj_is_at_xy(p, mx, my)) {
                    if (p->type == UIOBJ_TYPE_TEXTLINE) {
                        if (p->ta.z12) {
                            oi2 = oi;
                        }
                    } else {
                        oi2 = oi;
                    }
                }
            }
            --oi;
        }
        /*15a7a*/
        oi = oi2;
        if ((dirx == 0) && (diry == 0)) {
            return oi;
        }
        oi = uiobj_kbd_dir_key_dxdy(dirx, diry, oi2, mx, my);
        if ((oi != oi2) && (oi != UIOBJI_NONE)) {
            uiobj_set_focus_forced(oi);
        }
        return oi;
    }
}

static char uiobj_get_keychar(uint32_t key)
{
    mookey_t k = KBD_GET_KEY(key);
    if (0
      || ((k >= MOO_KEY_SPACE) && (k < MOO_KEY_a))
      || ((k >= MOO_KEY_KP0) && (k <= MOO_KEY_KP_EQUALS))
    ) {
        return KBD_GET_CHAR(key);
    }
    return 0;
}

static int16_t uiobj_handle_kbd_find_alt(int16_t oi, uint32_t key)
{
    const uiobj_t *p = &uiobj_tbl[oi];
    mookey_t k = KBD_GET_KEY(key);
    uint32_t kmod = KBD_GET_KEYMOD(key);
    char c = uiobj_get_keychar(key);
    while (1
      && (oi != uiobj_table_num)
      && (!((p->type != UIOBJ_TYPE_ALTSTR) && ((kmod == p->key) || (c && (c == p->key)))))
    ) {
        if ((p->type == UIOBJ_TYPE_ALTSTR) && KBD_MOD_ONLY_ALT(key) && (k == p->key)) {
            break;
        }
        ++oi;
        p = &uiobj_tbl[oi];
    }
    return oi;
}

static uint32_t uiobj_handle_kbd(int16_t *oiptr)
{
    uint32_t key = kbd_get_keypress();
    uiobj_t *p;
    int16_t /*si*/oi, /*di*/oi2;
    bool flag_reset_alt_str;
#ifdef FEATURE_MODEBUG
    if (KBD_GET_KEY(key) == 0) {
        LOG_DEBUG((0, "%s: got 0 key 0x%x\n", __func__, KBD_GET_KEY(key), key));
    }
#endif
    if (uiobj_kbd_alt_oi >= uiobj_table_num) {
        uiobj_kbd_alt_oi = 0;
    }
    oi = uiobj_kbd_alt_oi + 1;
    oi = uiobj_handle_kbd_find_alt(oi, key);
    p = &uiobj_tbl[oi];
    if (oi == uiobj_table_num) {
        oi = uiobj_handle_kbd_find_alt(1, key);
        p = &uiobj_tbl[oi];
    }
    uiobj_kbd_alt_oi = oi;
    flag_reset_alt_str = true;
    if (oi < uiobj_table_num) {
        *oiptr = oi;
        uiobj_set_focus(oi);
        p = &uiobj_tbl[oi];
        if (p->type == UIOBJ_TYPE_ALTSTR) {
            if (++p->t8.pos >= p->t8.len) {
                p->t8.pos = 0;
            } else {
                *oiptr = 0;
                key = MOO_KEY_UNKNOWN;
            }
            p->key = p->t8.str[p->t8.pos];
            flag_reset_alt_str = false;
        }
    } else {
        int dirx, diry;
        dirx = 0;
        diry = 0;
        oi2 = *oiptr;
        switch (KBD_GET_KEYMOD(key)) {
            case MOO_KEY_LEFT:
            case MOO_KEY_KP4:
                dirx = -1;
                break;
            case MOO_KEY_RIGHT:
            case MOO_KEY_KP6:
                dirx = 1;
                break;
            case MOO_KEY_UP:
            case MOO_KEY_KP8:
                diry = -1;
                break;
            case MOO_KEY_DOWN:
            case MOO_KEY_KP2:
                diry = 1;
                break;
            case MOO_KEY_KP7:
                dirx = -1;
                diry = -1;
                break;
            case MOO_KEY_KP9:
                dirx = 1;
                diry = -1;
                break;
            case MOO_KEY_KP1:
                dirx = -1;
                diry = 1;
                break;
            case MOO_KEY_KP3:
                dirx = 1;
                diry = 1;
                break;
            default:
                break;
        }
        if ((dirx || diry) && (KBD_GET_MOD(key) == 0)) {
            oi2 = uiobj_kbd_dir_key(dirx, diry);
        }
        *oiptr = oi2;
    }
    if (flag_reset_alt_str) {
        for (int16_t oi3 = 0; oi3 < uiobj_table_num; ++oi3) {
            p = &uiobj_tbl[oi3];
            if (p->type == UIOBJ_TYPE_ALTSTR) {
                p->t8.pos = 0;
                p->key = p->t8.str[0];
            }
        }
    }
    return key;
}

static void uiobj_click_obj(int16_t oi, int mx, int my)
{
    if ((mx < 0) || (mx >= UI_SCREEN_W) || (my < 0) || (my >= UI_SCREEN_H)) {
        return;
    }
    if (1/*mouse_flag_initialized*/) {
        uiobj_t *p = &uiobj_tbl[oi];
        if (uiobj_focus_oi != oi) {
            if (uiobj_focus_oi != -1) {
                uiobj_t *q = &uiobj_tbl[uiobj_focus_oi];
                /*if (uiobj_focus_oi != oi) {  redundant, checked above */
                if ((q->type != UIOBJ_TYPE_SETVAL) || (p->type == UIOBJ_TYPE_SETVAL)) {
                    if (q->type == UIOBJ_TYPE_TEXTLINE) {
                        if ((p->type == UIOBJ_TYPE_TEXTLINE) && p->ta.z12) {
                            uiobj_handle_click(uiobj_focus_oi, false);
                        }
                    } else {
                        uiobj_handle_click(uiobj_focus_oi, false);
                    }
                }
            }
            uiobj_focus_oi = oi;
            uiobj_handle_click(oi, true);
            if (p->type == UIOBJ_TYPE_TEXTINPUT) {
                mx = moouse_x;
                my = moouse_y;
            }
            if (!ui_mouse_warp_disabled) {
                hw_mouse_set_xy(mx, my);
            }
        }
    } else {
        /*don't care*/
    }
}

static void uiobj_finish_callback_delay_p(int delay)
{
    if (uiobj_flag_have_cb) {
        ui_delay_prepare();
        uiobj_do_callback();
        ui_palette_set_n();
        uiobj_finish_frame();
        ui_delay_ticks_or_click(delay);
    } else {
        ui_delay_prepare();
        ui_palette_set_n();
        uiobj_finish_frame();
        ui_delay_ticks_or_click(delay); /* MOO1 does not do this, but we need it to update mouse_* etc */
    }
}

static void uiobj_finish_callback_delay_1(void)
{
    uiobj_finish_callback_delay_p(1);
}

static void uiobj_finish_callback_delay_stored(void)
{
    uiobj_finish_callback_delay_p(uiobj_delay);
}

static void uiobj_slider_plus(uiobj_t *p, int adj)
{
    uint16_t vmin = p->t6.vmin;
    uint16_t value = *p->vptr;
    uint16_t vdiff = p->t6.vmax - vmin;
    int newval = ((value - vmin) * 100) / vdiff + adj;
    if (newval <= 100) {
        newval = (newval * vdiff) / 100 + vmin;
        if (newval == value) {
            ++newval;
        }
    } else {
        newval = p->t6.vmax;
    }
    value = newval;
    SETMIN(value, p->t6.vmax);
    *p->vptr = value;
    if (p->t6.cb) {
        p->t6.cb(p->t6.ctx, p->t6.slideri, value);
    }
}

static void uiobj_slider_minus(uiobj_t *p, int adj)
{
    uint16_t vmin = p->t6.vmin;
    uint16_t value = *p->vptr;
    uint16_t vdiff = p->t6.vmax - vmin;
    int newval = ((value - vmin) * 100) / vdiff - adj;
    if (newval >= 0) {
        newval = (newval * vdiff) / 100 + vmin;
    } else {
        newval = p->t6.vmin;
    }
    value = newval;
    SETMAX(value, p->t6.vmin);
    *p->vptr = value;
    if (p->t6.cb) {
        p->t6.cb(p->t6.ctx, p->t6.slideri, value);
    }
}

static int16_t uiobj_handle_input_sub0(void)
{
    int16_t oi = 0;
    uiobj_t *p, *q;
    int mx = moouse_x, my = moouse_y, mb;
    uiobj_focus_oi = -1;
    uiobj_clicked_oi = 0;
    uiobj_mouseoff = ui_cursor_mouseoff;
    if (kbd_have_keypress()) {
        uint32_t key = uiobj_handle_kbd(&oi);
        mookey_t k = KBD_GET_KEY(key);
        uint32_t kmod = KBD_GET_KEYMOD(key);
        char c = uiobj_get_keychar(key);
        if (k == MOO_KEY_UNKNOWN) {
            return 0;
        }
        /* checks for F11 and F12 debug keys omitted */
        if (kmod == MOO_KEY_F1) {
            if (uiobj_help_id != -1) {
                ui_help(uiobj_help_id);
            }
            return 0;
        }
        if (kmod == MOO_KEY_ESCAPE) {
            return -1;
        }
        p = &uiobj_tbl[oi];
        if (p->type == UIOBJ_TYPE_ALTSTR) {
            return oi;
        }
        if ((kmod == p->key) || (c && (c == p->key))) {
            if (p->type == UIOBJ_TYPE_SLIDER) {
                return 0;
            }
            if (oi != 0) {
                if (p->x0 == UIOBJ_OFFSCREEN) {
                    mx = UIOBJ_OFFSCREEN;
                    my = UIOBJ_OFFSCREEN;
                } else {
                    mx = scmidx(p);
                    my = scmidy(p);
                }
                uiobj_click_obj(oi, mx, my);
                if (p->type == UIOBJ_TYPE_TOGGLE) {
                    if (*p->vptr == 0) {
                        *p->vptr = 1;
                    } else {
                        *p->vptr = 0;
                    }
                } else if (p->type == UIOBJ_TYPE_SET) {
                    if (*p->vptr == 0) {
                        *p->vptr = 1;
                    }
                }
            }
            uiobj_finish_callback_delay_1();
            uiobj_focus_oi = -1;
            return oi;
        }
        if (k == MOO_KEY_RETURN) {
            oi = uiobj_find_obj_at_cursor();
            if (oi != 0) {
                p = &uiobj_tbl[oi];
                if (p->type != UIOBJ_TYPE_SLIDER) {
                    uiobj_click_obj(oi, mx, my);
                }
                if (p->type == UIOBJ_TYPE_TOGGLE) {
                    if (*p->vptr == 0) {
                        *p->vptr = 1;
                    } else {
                        *p->vptr = 0;
                    }
                } else if (p->type == UIOBJ_TYPE_SET) {
                    if (*p->vptr == 0) {
                        *p->vptr = 1;
                    }
                }
                if (uiobj_flag_skip_delay == 0) {
                    uiobj_finish_callback_delay_1();
                }
                uiobj_focus_oi = -1;
                return oi;
            } else {
                if (uiobj_flag_select_list_active) {
                    for (oi = 1; oi < uiobj_table_num; ++oi) {
                        p = &uiobj_tbl[oi];
                        if ((p->type == UIOBJ_TYPE_TEXTLINE) && (*p->vptr == p->ta.z18) && p->ta.z12) {
                            uiobj_focus_oi = -1;
                            return oi;
                        }
                    }
                }
            }
        }
        if ((c == '+') || (c == '-')) {
            oi = uiobj_find_obj_at_cursor();
            if (oi != 0) {
                p = &uiobj_tbl[oi];
                if (p->type == UIOBJ_TYPE_SLIDER) {
                    if (c == '+') {
                        uiobj_slider_plus(p, 5);
                    } else {
                        uiobj_slider_minus(p, 5);
                    }
                    uiobj_focus_oi = -1;
                    return oi;
                }
            }
        }
        uiobj_focus_oi = -1;
        return 0;
    }
    if (mouse_scroll) {
        int scroll = mouse_scroll;
        mouse_scroll = 0;
        uiobj_focus_oi = -1;
        oi = 0;
        ui_cursor_update_gfx_i(mx, my);
        uiobj_mouseoff = ui_cursor_mouseoff;
        for (int i = 1; i < uiobj_table_num; ++i) {
            p = &uiobj_tbl[i];
            if (((p->type == UIOBJ_TYPE_SLIDER) || (p->type == UIOBJ_TYPE_SCROLLAREA) || (p->type == UIOBJ_TYPE_WHEELAREA)) && uiobj_is_at_xy(p, mx, my)) {
                oi = i;
                break;
            }
        }
        if (oi != 0) {
            if (p->type == UIOBJ_TYPE_SLIDER) {
                if (ui_mwi_slider) {
                    scroll = -scroll;
                }
                if (scroll > 0) {
                    uiobj_slider_plus(p, scroll);
                } else {
                    uiobj_slider_minus(p, -scroll);
                }
                return oi;
            } else if (p->type == UIOBJ_TYPE_SCROLLAREA) {
                uint8_t z = *p->tb.zptr;
                if (scroll < 0) {
                    if (z < p->tb.zmax) {
                        ++z;
                    } else {
                        return 0;
                    }
                } else {
                   if (z > 1) {
                        --z;
                    } else {
                        return 0;
                    }
                }
                *p->tb.zptr = z;
                *p->tb.xptr = -1;
                *p->tb.yptr = -1;
                return oi;
            } else if (p->type == UIOBJ_TYPE_WHEELAREA) {
                *p->vptr += scroll;
                return oi;
            }
        }
        return 0;
    }
    if (mouse_buttons == 0) {
        if (!mouse_getclear_click_hw()) {
            return 0;
        }
        mb = mouse_click_buttons;
        if (mb == MOUSE_BUTTON_MASK_RIGHT) {
            mouse_getclear_click_hw();
            mouse_getclear_click_sw();
            return -1;
        } else {
            mx = mouse_click_x;
            my = mouse_click_y;
            oi = 0;
            /*key = 0;*/
            ui_cursor_update_gfx_i(mx, my);
            uiobj_mouseoff = ui_cursor_mouseoff;
            for (int oi2 = 0; oi2 < uiobj_table_num; ++oi2) {
                p = &uiobj_tbl[oi2];
                if (!uiobj_is_at_xy(p, mx, my)) {
                    continue;
                }
                oi = oi2;
                break;
            }
            p = &uiobj_tbl[oi];
            if (oi != 0) {
                uiobj_clicked_oi = oi;
                uiobj_click_obj(oi, mx, my);
                uiobj_finish_callback_delay_1();
            }
            uiobj_focus_oi = -1;
            if (oi != 0) {
                mouse_getclear_click_sw();
            }
            {
                uiobj_clicked_oi = oi;
                if (mb == MOUSE_BUTTON_MASK_RIGHT) {
                    return -oi;
                } else {
                    return oi;
                }
            }
        }
    } else {
        mb = mouse_buttons;
        if (mb == MOUSE_BUTTON_MASK_RIGHT) {
            while ((mb = mouse_buttons) == MOUSE_BUTTON_MASK_RIGHT) {
                uiobj_finish_callback_delay_1();
            }
            mouse_getclear_click_hw();
            mouse_getclear_click_sw();
            return -1;
        }
        if (ui_mouse_lmb_fix) {
            oi = uiobj_find_obj_at_cursor();
        }
        while (mouse_buttons != 0) {
            mx = moouse_x;
            my = moouse_y;
            uiobj_mouseoff = ui_cursor_mouseoff;
            if (ui_mouse_lmb_fix) {
                if ((uiobj_tbl[oi].type == UIOBJ_TYPE_TEXTLINE)
                  ||(uiobj_tbl[oi].type == UIOBJ_TYPE_SETVAL))
                {
                    int oi_next = uiobj_find_obj_at_cursor();
                    if ((oi_next != 0) && (uiobj_tbl[oi].type == uiobj_tbl[oi_next].type)) {
                        oi = oi_next;
                    }
                }
            } else {
                oi = uiobj_find_obj_at_cursor();
            }
            if (oi == 0) {
                if (uiobj_focus_oi != -1) {
                    p = &uiobj_tbl[uiobj_focus_oi];
                    if (p->type == UIOBJ_TYPE_SLIDER) {
                        uiobj_do_callback();
                    }
                    if ((p->type != UIOBJ_TYPE_SETVAL) && (p->type != UIOBJ_TYPE_TEXTLINE)) {
                        uiobj_handle_click(uiobj_focus_oi, false);
                        hw_mouse_set_xy(mx, my);
                    }
                    uiobj_focus_oi = -1;
                }
                mouse_set_click_xy(mx, my);
                break;
            }
            p = &uiobj_tbl[oi];
            if ((oi != uiobj_focus_oi) && (p->type != UIOBJ_TYPE_TEXTINPUT)) {
                if (uiobj_focus_oi >= 0 && uiobj_tbl[uiobj_focus_oi].type == UIOBJ_TYPE_SLIDER) {
                    uiobj_do_callback();
                }
                uiobj_click_obj(oi, mx, my);
            }
            uiobj_clicked_oi = oi;
            if (uiobj_flag_skip_delay != 0) {
                break;
            }
            if (mouse_buttons != 0) {
                uiobj_finish_callback_delay_stored();
            }
        }
        q = &uiobj_tbl[uiobj_clicked_oi];
        if (q->type == UIOBJ_TYPE_SLIDER) {
            uiobj_do_callback();
        }
        uiobj_clicked_oi = 0;
        if (oi != 0) {
            mouse_getclear_click_hw();
            mouse_getclear_click_sw();
            switch (p->type) {
                case UIOBJ_TYPE_SET:
                    if (*p->vptr == 0) {
                        *p->vptr = 1;
                    }
                    break;
                case UIOBJ_TYPE_TOGGLE:
                    if (*p->vptr == 0) {
                        *p->vptr = 1;
                    } else {
                        *p->vptr = 0;
                    }
                    break;
                case UIOBJ_TYPE_TEXTINPUT:
                    uiobj_click_obj(oi, mx, my);
                default:
                    break;
            }
        }
        uiobj_focus_oi = -1;
        if (ui_mouse_lmb_fix && (oi != uiobj_find_obj_at_cursor()) && (uiobj_tbl[oi].type != UIOBJ_TYPE_TEXTINPUT)) {
            return 0;
        }
        if (mb == MOUSE_BUTTON_MASK_RIGHT) {
            return -oi;
        } else {
            return oi;
        }
    }

    return 0;
}

static void uiobj_add_set_xys(uiobj_t *p, uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint8_t scale)
{
    p->scale = scale;
    p->x0 = x0;
    p->y0 = y0;
    p->x1 = x1;
    p->y1 = y1;
}

static void uiobj_add_t03_do(uint16_t x, uint16_t y, const char *str, uint8_t *lbxdata, mookey_t key, uint8_t scale)
{
    uiobj_t *p = &uiobj_tbl[uiobj_table_num];
    uiobj_add_set_xys(p, x, y, x + lbxgfx_get_w(lbxdata) - 1, y + lbxgfx_get_h(lbxdata) - 1, scale);
    p->t0.str = str;
    p->t0.fontnum = lbxfont_get_current_fontnum();
    p->t0.fonta2 = lbxfont_get_current_fonta2();
    p->t0.lbxdata = lbxdata;
    p->t0.indep = lbxgfx_get_indep(lbxdata);
    p->key = key;
}

static void uiobj_handle_objects(void)
{
    for (int i = 1; i < uiobj_table_num; ++i) {
        uiobj_t *p = &uiobj_tbl[i];
        if ((i == uiobj_focus_oi) && (p->type != UIOBJ_TYPE_TEXTINPUT)) {
            uiobj_handle_click(i, true);
        } else {
            uiobj_handle_objects_sub1(i);
        }
    }
}

/* -------------------------------------------------------------------------- */

void uiobj_table_clear(void)
{
    uiobj_table_num = 1;
    uiobj_focus_oi = -1;
    uiobj_clicked_oi = 0;
}

void uiobj_table_set_last(int16_t oi)
{
    uiobj_table_num = oi + 1;
    uiobj_focus_oi = -1;
}

void uiobj_table_num_store(void)
{
    uiobj_table_num_old = uiobj_table_num;
    uiobj_table_num = 0;
    uiobj_flag_have_cb = false;
}

void uiobj_table_num_restore(void)
{
    uiobj_table_num = uiobj_table_num_old;
    if (uiobj_callback) {
        uiobj_flag_have_cb = true;
    }
}

int16_t uiobj_handle_input_cond(void)
{
    if (uiobj_handle_downcount > 0) {
        --uiobj_handle_downcount;
        return 0;
    }
    uiobj_handle_downcount = 0;
    if (uiobj_table_num <= 1) {
        return 0;
    }
    if (1/*mouse_initialized*/) {
        return uiobj_handle_input_sub0();
    } else {
        return 0;/*uiobj_handle_input_sub1();*/
    }
}

void uiobj_finish_frame(void)
{
    int mx, my;
    mx = moouse_x;
    my = moouse_y;
    uiobj_handle_objects();
    ui_cursor_update_gfx_i(mx, my);
    ui_cursor_store_bg1(mx, my);
    ui_cursor_draw1(mx, my);
    hw_video_draw_buf();
    /* HACK MOO1 maintains the bg for both buffers, we simply erase the cursor right after draw. */
    ui_cursor_copy_bg1_to_bg0();
    ui_cursor_erase0();
}

void uiobj_set_downcount(int16_t v)
{
    uiobj_handle_downcount = v;
    mouse_getclear_click_hw();
    mouse_getclear_click_sw();
}

void uiobj_set_xyoff(int xoff, int yoff)
{
    uiobj_xoff = xoff;
    uiobj_yoff = yoff;
}

void uiobj_set_limits(int minx, int miny, int maxx, int maxy)
{
    SETMAX(minx, 0);
    SETMAX(miny, 0);
    SETMIN(maxx, UI_SCREEN_W - 1);
    SETMIN(maxy, UI_SCREEN_H - 1);
    if (minx > maxx) { int t = minx; minx = maxx; maxx = t; }
    if (miny > maxy) { int t = miny; miny = maxy; maxy = t; }
    uiobj_minx = minx;
    uiobj_miny = miny;
    uiobj_maxx = maxx;
    uiobj_maxy = maxy;
}

void uiobj_set_limits_all(void)
{
    uiobj_minx = 0;
    uiobj_miny = 0;
    uiobj_maxx = UI_SCREEN_W - 1;
    uiobj_maxy = UI_SCREEN_H - 1;
}

void uiobj_set_help_id(int16_t v)
{
    uiobj_help_id = v;
}

int16_t uiobj_get_clicked_oi(void)
{
    return uiobj_clicked_oi;
}

void uiobj_set_skip_delay(bool v)
{
    uiobj_flag_skip_delay = v;
}

void uiobj_set_callback_and_delay(void (*cb)(void *), void *data, uint16_t delay)
{
    uiobj_callback = cb;
    uiobj_cbdata = data;
    uiobj_flag_have_cb = true;
    uiobj_delay = ((delay > 0) && (delay < 10)) ? delay : 2;
}

void uiobj_unset_callback(void)
{
    uiobj_callback = NULL;
    uiobj_cbdata = NULL;
    uiobj_flag_have_cb = false;
}

void uiobj_do_callback(void)
{
    if (uiobj_flag_have_cb) {
        uiobj_callback(uiobj_cbdata);
    }
}

void uiobj_set_focus_forced(int16_t uiobji)
{
    uiobj_t *p = &uiobj_tbl[uiobji];
    int x, y;
    x = scmidx(p);
    y = scmidy(p);
    if ((y < 0) || (y >= UI_SCREEN_H) || (x < 0) || (x >= UI_SCREEN_W)) {
        return;
    }
    ui_cursor_update_gfx_i(x, y);
    uiobj_mouseoff = ui_cursor_mouseoff;
    x -= uiobj_mouseoff;
    y -= uiobj_mouseoff;
    hw_mouse_set_xy(x, y);
    /* needed anywhere? */
    mouse_stored_x = x;
    mouse_stored_y = y;
}

void uiobj_set_focus(int16_t uiobji)
{
    if (!ui_mouse_warp_disabled) {
        uiobj_set_focus_forced(uiobji);
    }
}

int16_t uiobj_find_obj_at_cursor(void)
{
    int x = moouse_x, y = moouse_y;
    ui_cursor_update_gfx_i(x, y);
    uiobj_mouseoff = ui_cursor_mouseoff;
    for (int i = 1; i < uiobj_table_num; ++i) {
        uiobj_t *p = &uiobj_tbl[i];
        if (!uiobj_is_at_xy(p, x, y)) {
            continue;
        }
        return i;
    }
    return 0;
}

int16_t uiobj_at_cursor(void)
{
    uiobj_t *p;
    int i, x = moouse_x, y = moouse_y;
    ui_cursor_update_gfx_i(x, y);
    uiobj_mouseoff = ui_cursor_mouseoff;
    i = uiobj_find_obj_at_cursor();
    p = &uiobj_tbl[i];
    if ((p->type == UIOBJ_TYPE_TEXTLINE) && !p->ta.z12) {
        i = 0;
    }
    return i;
}

int16_t uiobj_add_t0(uint16_t x, uint16_t y, const char *str, uint8_t *lbxdata, mookey_t key)
{
    uiobj_t *p = &uiobj_tbl[uiobj_table_num];
    uiobj_add_t03_do(x, y, str, lbxdata, key, ui_scale);
    p->type = UIOBJ_TYPE_BUTTON;
    p->vptr = 0;
    return uiobj_alloc();
}

int16_t uiobj_add_t1(uint16_t x, uint16_t y, const char *str, uint8_t *lbxdata, int16_t *vptr, mookey_t key)
{
    uiobj_t *p = &uiobj_tbl[uiobj_table_num];
    uiobj_add_t03_do(x, y, str, lbxdata, key, ui_scale);
    p->type = UIOBJ_TYPE_TOGGLE;
    p->vptr = vptr;
    return uiobj_alloc();
}

int16_t uiobj_add_t2(uint16_t x, uint16_t y, const char *str, uint8_t *lbxdata, int16_t *vptr, mookey_t key)
{
    uiobj_t *p = &uiobj_tbl[uiobj_table_num];
    uiobj_add_t03_do(x, y, str, lbxdata, key, ui_scale);
    p->type = UIOBJ_TYPE_SET;
    p->vptr = vptr;
    return uiobj_alloc();
}

int16_t uiobj_add_t3(uint16_t x, uint16_t y, const char *str, uint8_t *lbxdata, int16_t *vptr, int16_t z18, mookey_t key)
{
    uiobj_t *p = &uiobj_tbl[uiobj_table_num];
    uiobj_add_t03_do(x, y, str, lbxdata, key, ui_scale);
    p->type = UIOBJ_TYPE_SETVAL;
    p->vptr = vptr;
    p->t0.z18 = z18;
    return uiobj_alloc();
}

int16_t uiobj_add_textinput(int x, int y, int w, char *buf, uint16_t max_chars, uint8_t rcolor, bool alignr, bool allow_lcase, const uint8_t *colortbl, mookey_t key)
{
    uiobj_t *p = &uiobj_tbl[uiobj_table_num];
    uiobj_add_set_xys(p, x, y, x + w, y + lbxfont_get_height(), ui_scale);
    p->t4.fontnum = lbxfont_get_current_fontnum();
    p->t4.fonta2 = lbxfont_get_current_fonta2();
    p->t4.fonta4 = lbxfont_get_current_fonta2b();
    p->t4.max_chars = max_chars;
    p->t4.buf = buf;
    p->t4.rectcolor = rcolor;
    p->t4.align_right = alignr;
    p->t4.allow_lcase = allow_lcase;
    p->t4.colortbl = colortbl;
    p->type = UIOBJ_TYPE_TEXTINPUT;
    p->vptr = 0;
    p->key = key;
    return uiobj_alloc();
}

int16_t uiobj_add_slider_int(uint16_t x0, uint16_t y0, uint16_t vmin, uint16_t vmax, uint16_t w, uint16_t h, int16_t *vptr)
{
    uiobj_t *p = &uiobj_tbl[uiobj_table_num];
    uiobj_add_set_xys(p, x0, y0, x0 + w, y0 + h, ui_scale);
    p->t6.vmin = vmin;
    p->t6.vmax = vmax;
    p->type = UIOBJ_TYPE_SLIDER;
    p->key = MOO_KEY_UNKNOWN;
    p->vptr = vptr;
    p->t6.cb = 0;
    p->t6.ctx = 0;
    p->t6.slideri = 0;
    return uiobj_alloc();
}

int16_t uiobj_add_slider_func(uint16_t x0, uint16_t y0, uint16_t vmin, uint16_t vmax, uint16_t w, uint16_t h, int16_t *vptr, void (*cb)(void *ctx, uint8_t slideri, int16_t value), void *ctx, uint8_t slideri)
{
    uiobj_t *p = &uiobj_tbl[uiobj_table_num];
    uiobj_add_set_xys(p, x0, y0, x0 + w, y0 + h, ui_scale);
    p->t6.vmin = vmin;
    p->t6.vmax = vmax;
    p->type = UIOBJ_TYPE_SLIDER;
    p->key = MOO_KEY_UNKNOWN;
    p->vptr = vptr;
    p->t6.cb = cb;
    p->t6.ctx = ctx;
    p->t6.slideri = slideri;
    return uiobj_alloc();
}

int16_t uiobj_add_mousearea(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, mookey_t key)
{
    uiobj_t *p = &uiobj_tbl[uiobj_table_num];
    uiobj_add_set_xys(p, x0, y0, x1, y1, ui_scale);
    p->type = UIOBJ_TYPE_MOUSEAREA;
    p->vptr = 0;
    p->key = key;
    return uiobj_alloc();
}

int16_t uiobj_add_mousearea_limited(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint8_t scale, mookey_t key)
{
    uiobj_t *p;
    if ((x1 < uiobj_minx) || (x0 > uiobj_maxx) || (y1 < uiobj_miny) || (y0 > uiobj_maxy)) {
        return UIOBJI_OUTSIDE;
    }
    p = &uiobj_tbl[uiobj_table_num];
    x0 = MAX(x0, uiobj_minx);
    x1 = MIN(x1, uiobj_maxx);
    y0 = MAX(y0, uiobj_miny);
    y1 = MIN(y1, uiobj_maxy);
    uiobj_add_set_xys(p, x0, y0, x1, y1, scale);
    p->type = UIOBJ_TYPE_MOUSEAREA;
    p->vptr = 0;
    p->key = key;
    return uiobj_alloc();
}

int16_t uiobj_add_mousewheel(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, int16_t *vptr)
{
    uiobj_t *p = &uiobj_tbl[uiobj_table_num];
    uiobj_add_set_xys(p, x0, y0, x1, y1, ui_scale);
    p->type = UIOBJ_TYPE_WHEELAREA;
    p->vptr = vptr;
    p->key = MOO_KEY_UNKNOWN;
    return uiobj_alloc();
}

int16_t uiobj_add_mousearea_all(mookey_t key)
{
    uiobj_t *p = &uiobj_tbl[uiobj_table_num];
    uiobj_add_set_xys(p, 0, 0, UI_SCREEN_W - 1, UI_SCREEN_H - 1, 1);
    p->type = UIOBJ_TYPE_MOUSEAREA;
    p->vptr = 0;
    p->key = key;
    return uiobj_alloc();
}

int16_t uiobj_add_inputkey(uint32_t key)
{
    uiobj_t *p = &uiobj_tbl[uiobj_table_num];
    p->x0 = UIOBJ_OFFSCREEN;
    p->y0 = UIOBJ_OFFSCREEN;
    p->x1 = UIOBJ_OFFSCREEN;
    p->y1 = UIOBJ_OFFSCREEN;
    p->scale = 1;
    p->type = UIOBJ_TYPE_MOUSEAREA;
    p->vptr = 0;
    p->key = key;
    return uiobj_alloc();
}

int16_t uiobj_add_alt_str(const char *str)
{
    uiobj_t *p = &uiobj_tbl[uiobj_table_num];
    int len = 0;
    while ((str[len] != 0) && (len < 0x1e)) {
        ++len;
    }
    p->x0 = UIOBJ_OFFSCREEN;
    p->y0 = UIOBJ_OFFSCREEN;
    p->x1 = UIOBJ_OFFSCREEN;
    p->y1 = UIOBJ_OFFSCREEN;
    p->scale = 1;
    p->type = UIOBJ_TYPE_ALTSTR;
    p->vptr = 0;
    p->t8.str = str;
    p->t8.pos = 0;
    p->t8.len = len;
    {
        char b = *str;
        if ((b >= 'a') && (b <= 'z')) {
            b -= 'a' - 'A';
        }
        p->key = b;
    }
    return uiobj_alloc();
}

int16_t uiobj_add_ta(uint16_t x, uint16_t y, uint16_t w, const char *str, bool z12, int16_t *vptr, int16_t z18, uint8_t subtype, uint8_t sp0v, mookey_t key)
{
    uiobj_t *p = &uiobj_tbl[uiobj_table_num];
    uiobj_add_set_xys(p, x, y - 1, x + w, y + lbxfont_get_height() + 1, ui_scale);
    p->ta.fontnum = lbxfont_get_current_fontnum();
    p->ta.fonta2 = lbxfont_get_current_fonta2();
    p->ta.fonta2b = lbxfont_get_current_fonta2b();
    p->ta.z18 = z18;
    p->ta.z12 = z12;
    p->ta.str = str;
    p->ta.subtype = subtype;
    p->ta.sp0v = sp0v;
    p->type = UIOBJ_TYPE_TEXTLINE;
    p->vptr = vptr;
    p->key = key;
    return uiobj_alloc();
}

int16_t uiobj_add_tb(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t xscale, uint16_t yscale, int16_t *xptr, int16_t *yptr, uint8_t *zptr, uint8_t zmax)
{
    uiobj_t *p = &uiobj_tbl[uiobj_table_num];
    uiobj_add_set_xys(p, x, y, x + w * xscale, y + h * yscale, ui_scale);
    p->tb.xdiv = w * ui_scale;
    p->tb.ydiv = h * ui_scale;
    p->tb.xptr = xptr;
    p->tb.yptr = yptr;
    p->tb.zptr = zptr;
    p->tb.zmax = zmax;
    p->type = UIOBJ_TYPE_SCROLLAREA;
    p->vptr = 0;
    p->key = MOO_KEY_UNKNOWN;
    return uiobj_alloc();
}

void uiobj_dec_y1(int16_t oi)
{
    uiobj_t *p = &uiobj_tbl[oi];
    --p->y1;
}

void uiobj_ta_set_val_0(int16_t oi)
{
    uiobj_t *p = &uiobj_tbl[oi];
    if (p->type == UIOBJ_TYPE_TEXTLINE) {
        *p->vptr = 0;
    }
}

void uiobj_ta_set_val_1(int16_t oi)
{
    uiobj_t *p = &uiobj_tbl[oi];
    if (p->type == UIOBJ_TYPE_TEXTLINE) {
        *p->vptr = 1;
    }
}

int16_t uiobj_select_from_list3(int x, int y, int w, const char *title, char const * const *strtbl, int16_t *selptr, const bool *condtbl, uint8_t subtype, uint8_t sp0v, bool update_at_cursor, bool add_switch, int sx, int sy)
{
    int h, dy, ty = y, di = -1;
    bool flag_done = false, toz12, flag_copy_buf = false;
    uint16_t itemi = 0, v6 = 0;
    int16_t oi = 0, oi_title, v18 = 0;
    int16_t oi_switch = UIOBJI_INVALID;
    char const * const *s = strtbl;

    uiobj_flag_select_list_active = true;
    uiobj_set_downcount(1);
    uiobj_table_clear();
    h = lbxfont_get_height();
    dy = lbxfont_get_gap_h() + h;

    while (!flag_done) {
        if (*s == 0) {
            flag_done = true;
            break;
        }
        if (!v6) {
            if ((!condtbl) || condtbl[itemi]) {
                di = itemi;
                v6 = 1;
            }
        }
        /*18a50*/
        ty += dy;
        if (!condtbl) {
            toz12 = true;
        } else {
            toz12 = condtbl[itemi];
        }
        uiobj_add_ta(x, ty, w, *s, toz12, selptr, itemi, subtype, sp0v, MOO_KEY_UNKNOWN);
        ++itemi;
        ++s;
    }

    v6 = itemi;
    lbxfont_select(lbxfont_get_current_fontnum(), lbxfont_get_current_fonta2(), lbxfont_get_current_fonta4(), 0);
    oi_title = uiobj_add_ta(x, y, w, title, false, &v18, 1, 0, 0, MOO_KEY_UNKNOWN);
    if (add_switch) {
        oi_switch = uiobj_add_mousearea(120, sy + 3, 200, sy + 11, MOO_KEY_UNKNOWN);
    }

    if ((*selptr < 0) || (*selptr >= v6) || (*selptr < di)) {
        if ((di >= 0) && (di < v6)) {
            *selptr = uiobj_tbl[di + 1].ta.z18;
        } else {
            *selptr = -1;
        }
    }

    flag_done = false;
    oi = 0;

    while (!flag_done) {
        ui_delay_prepare();
        oi = uiobj_handle_input_cond();
        if (oi == oi_switch) {
            flag_done = true;
            break;
        }
        if (oi != 0) {
            flag_done = true;
        }
        if ((oi == oi_title) || ((oi > 0) && condtbl && !condtbl[oi - 1])) {
            flag_done = false;
        }
        if (flag_done) {
            break;
        }
        uiobj_do_callback();
        if (update_at_cursor) {
            int oi2;
            oi2 = uiobj_at_cursor();
            if (oi2 > 0) {
                *selptr = uiobj_tbl[oi2].ta.z18;
            }
        }
        ui_palette_set_n();
        uiobj_finish_frame();
        if (!flag_copy_buf) {
            ui_draw_copy_buf();
            flag_copy_buf = true;
        }
        ui_delay_ticks_or_click(uiobj_delay);
    }
    uiobj_table_clear();
    uiobj_flag_select_list_active = false;
    mouse_getclear_click_hw();
    mouse_getclear_click_sw();
    if (oi == oi_switch) {
        return -2;
    }
    if (oi < 0) {
        return -1;
    }
    return oi - 1;
}

int16_t uiobj_select_from_list1(int x, int y, int w, const char *title, char const * const *strtbl, int16_t *selptr, const bool *condtbl, uint8_t subtype, uint8_t sp0v, bool update_at_cursor)
{
    return uiobj_select_from_list3(x, y, w, title, strtbl, selptr, condtbl, subtype, sp0v, update_at_cursor, false, 0, 0);
}

int16_t uiobj_select_from_list4(int x, int y, int w, const char *title, char const * const *strtbl, int16_t *selptr, const bool *condtbl, int linenum, int upx, int upy, uint8_t *uplbx, int dnx, int dny, uint8_t *dnlbx, uint8_t subtype, uint8_t sp0v, bool update_at_cursor, bool add_switch, int sx, int sy)
{
    int h, dy, ty, linei = 0, itemi = 0, itemnum, itemoffs, foundi = 0;
    bool flag_done = false, flag_copy_buf = false, flag_found = false;
    uint16_t fonta4, fonta2b;
    int16_t oi = 0, oi_title, oi_up, oi_dn, oi_wheel, v18 = 0, upvar, dnvar, curval, scroll = 0;
    int16_t oi_switch = UIOBJI_INVALID;
    char const * const *s = strtbl;

    uiobj_flag_select_list_active = true;
    uiobj_flag_select_list_multipage = true;
    uiobj_kbd_movey = 0;
    fonta4 = lbxfont_get_current_fonta4();
    fonta2b = lbxfont_get_current_fonta2b();
    uiobj_set_downcount(1);
    uiobj_table_clear();
    h = lbxfont_get_height();
    dy = lbxfont_get_gap_h() + h;
    ty = y + dy;

    while (!flag_done) {
        if (*s == 0) {
            flag_done = true;
            break;
        }
        if (!flag_found) {
            if ((!condtbl) || condtbl[itemi]) {
                foundi = itemi;
                flag_found = true;
            }
        }
        ++itemi;
        ++s;
    }
    itemnum = itemi;

    {
        int i;
        i = *selptr;
        if ((i < 0) || (i >= itemnum)) {
            i = 0;
        }
        if ((i + linenum) > itemnum) {
            i = itemnum - linenum;
            SETMAX(i, 0);
        }
        itemoffs = i;
    }
    s = &strtbl[itemoffs];
    for (itemi = itemoffs; (itemi < itemnum) && (linei < linenum); ++itemi, ++linei, ++s, ty += dy) {
        uiobj_add_ta(x, ty, w, *s, (!condtbl) || condtbl[itemi], selptr, itemi, subtype, sp0v, MOO_KEY_UNKNOWN);
    }

    if ((*selptr < 0) || (*selptr >= itemnum)) {
        if ((foundi >= 0) && (foundi < itemnum)) {
            *selptr = itemoffs;
        } else {
            *selptr = -1;
        }
    }

    lbxfont_select(lbxfont_get_current_fontnum(), lbxfont_get_current_fonta2(), fonta4, 0);
    oi_title = uiobj_add_ta(x, y, w, title, false, &v18, 1, 0, 0, MOO_KEY_UNKNOWN);

    upvar = (itemoffs == 0) ? 1 : 0;
    dnvar = (itemi >= itemnum) ? 1 : 0;
    oi_up = uiobj_add_t2(upx, upy, "", uplbx, &upvar, MOO_KEY_PAGEUP);
    oi_dn = uiobj_add_t2(dnx, dny, "", dnlbx, &dnvar, MOO_KEY_PAGEDOWN);
    if (add_switch) {
        oi_switch = uiobj_add_mousearea(120, sy + 3, 200, sy + 11, MOO_KEY_UNKNOWN);
    }
    oi_wheel = uiobj_add_mousewheel(0, 0, 319, 199, &scroll);

    flag_done = false;
    curval = *selptr;

    while (!flag_done) {
        bool flag_rebuild;
        ui_delay_prepare();
        oi = uiobj_handle_input_cond();
        if ((oi < 0) || ((oi > 0) && (oi < oi_title) && uiobj_tbl[oi].ta.z12)) {
            flag_done = true;
            break;
        }
        flag_rebuild = false;
        if (oi == oi_up) {
            itemoffs -= linenum;
            SETMAX(itemoffs, 0);
            flag_rebuild = true;
        } else if (oi == oi_dn) {
            int i;
            i = itemoffs + linenum;
            if ((i + linenum) > itemnum) {
                i = itemnum - linenum;
                SETMAX(i, 0);
            }
            if (i >= itemnum) {
                i = itemoffs;
            }
            itemoffs = i;
            flag_rebuild = true;
        } else if (oi == oi_wheel) {
            int i;
            i = itemoffs + scroll;
            scroll = 0;
            if ((i + linenum) > itemnum) {
                i = itemnum - linenum;
            }
            SETMAX(i, 0);
            if (i >= itemnum) {
                i = itemoffs;
            }
            itemoffs = i;
            flag_rebuild = true;
        } else if (oi == oi_switch) {
            flag_done = true;
            break;
        }
        if (uiobj_kbd_movey == 1) {
            int i, j;
            i = itemoffs + 1;
            j = itemoffs + linenum;
            while ((j < itemnum) && condtbl && (!condtbl[j])) {
                ++j;
                ++i;
            }
            if (j >= itemnum) {
                i = itemoffs;
            }
            if ((i + linenum) > itemnum) {
                i = itemnum - linenum;
                SETMAX(i, 0);
            }
            itemoffs = i;
            flag_rebuild = true;
        } else if (uiobj_kbd_movey == -1) {
            int i;
            i = itemoffs - 1;
            SETMAX(i, 0);
            while ((i >= 0) && condtbl && (!condtbl[i])) {
                --i;
            }
            SETMAX(i, 0);
            itemoffs = i;
            flag_rebuild = true;
        }
        if (flag_rebuild) {
            uiobj_table_clear();
            lbxfont_select(lbxfont_get_current_fontnum(), lbxfont_get_current_fonta2(), fonta2b, 0);
            *selptr = -1;
            if (uiobj_kbd_movey == 1) {
                if ((!condtbl) || condtbl[itemoffs + linenum - 1]) {
                    *selptr = itemoffs + linenum - 1;
                } else {
                    for (int i = itemoffs + linenum - 1; i > 0; --i) {
                        if ((!condtbl) || condtbl[i]) {
                            *selptr = i;
                            break;
                        }
                    }
                }
            } else {
                if ((!condtbl) || condtbl[itemoffs]) {
                    *selptr = itemoffs;
                } else {
                    for (int i = itemoffs; i < (itemoffs + linenum); ++i) {
                        if ((!condtbl) || condtbl[i]) {
                            *selptr = i;
                            break;
                        }
                    }
                }
            }
            s = &strtbl[itemoffs];
            linei = 0;
            ty = y + dy;
            for (itemi = itemoffs; (itemi < itemnum) && (linei < linenum); ++itemi, ++linei, ++s, ty += dy) {
                uiobj_add_ta(x, ty, w, *s, (!condtbl) || condtbl[itemi], selptr, itemi, subtype, sp0v, MOO_KEY_UNKNOWN);
            }
            lbxfont_select(lbxfont_get_current_fontnum(), lbxfont_get_current_fonta2(), fonta4, 0);
            oi_title = uiobj_add_ta(x, y, w, title, false, &v18, 1, 0, 0, MOO_KEY_UNKNOWN);
            upvar = (itemoffs == 0) ? 1 : 0;
            dnvar = (itemi >= itemnum) ? 1 : 0;
            oi_up = uiobj_add_t2(upx, upy, "", uplbx, &upvar, MOO_KEY_PAGEUP);
            oi_dn = uiobj_add_t2(dnx, dny, "", dnlbx, &dnvar, MOO_KEY_PAGEDOWN);
            if (add_switch) {
                oi_switch = uiobj_add_mousearea(120, sy + 3, 200, sy + 11, MOO_KEY_UNKNOWN);
            }
            oi_wheel = uiobj_add_mousewheel(0, 0, 319, 199, &scroll);
        }
        uiobj_kbd_movey = 0;
        if (update_at_cursor) {
            int oi2;
            oi2 = uiobj_at_cursor();
            if (oi2 > 0) {
                *selptr = uiobj_tbl[oi2].ta.z18;
            }
        }
        uiobj_do_callback();
        ui_palette_set_n();
        uiobj_finish_frame();
        if (!flag_copy_buf) {
            ui_draw_copy_buf();
            flag_copy_buf = true;
        }
        ui_delay_ticks_or_click(uiobj_delay);
    }
    uiobj_table_clear();
    uiobj_flag_select_list_active = false;
    uiobj_flag_select_list_multipage = false;
    mouse_getclear_click_hw();
    mouse_getclear_click_sw();
    if (oi == oi_switch) {
        *selptr = curval;
        return -2;
    }
    if (oi < 0) {
        *selptr = curval;
        return -1;
    }
    return oi + itemoffs - 1;
}

int16_t uiobj_select_from_list2(int x, int y, int w, const char *title, char const * const *strtbl, int16_t *selptr, const bool *condtbl, int linenum, int upx, int upy, uint8_t *uplbx, int dnx, int dny, uint8_t *dnlbx, uint8_t subtype, uint8_t sp0v, bool update_at_cursor)
{
    return uiobj_select_from_list4(x, y, w, title, strtbl, selptr, condtbl, linenum, upx, upy, uplbx, dnx, dny, dnlbx, subtype, sp0v, update_at_cursor, false, 0, 0);
}


bool uiobj_read_str(int x, int y, int w, char *buf, int max_chars, uint8_t rcolor, bool alignr, const uint8_t *ctbl)
{
    bool flag_quit = false;
    uiobj_t *p;
    uiobj_table_clear();
    if (1/*mouse_flag_initialized*/) {
        while (mouse_buttons) {
            hw_event_handle();
        }
        mouse_getclear_click_hw();
        mouse_getclear_click_sw();
    }
    uiobj_set_downcount(1);
    {
        int16_t oi = uiobj_add_textinput(x, y, w, buf, max_chars, rcolor, alignr, false, ctbl, MOO_KEY_UNKNOWN);
        uiobj_focus_oi = oi;
        p = &uiobj_tbl[oi];
    }
    flag_quit = uiobj_textinput_do(p, w, buf, max_chars, true, false);
    uiobj_table_clear();
    return !flag_quit;
}

void uiobj_input_flush(void)
{
    uiobj_clicked_oi = 0;
    while (kbd_have_keypress()) {
        kbd_get_keypress();
    }
    while (mouse_buttons) {
        uiobj_finish_callback_delay_stored();
    }
}

void uiobj_input_wait(void)
{
    bool got_any = false, got_mb = false;
    uiobj_input_flush();
    while (!got_any) {
        if (mouse_buttons || mouse_getclear_click_hw()) {
            got_any = true;
            got_mb = true;
        }
        if (kbd_have_keypress()) {
            uint32_t kp;
            mookey_t k;
            kp = kbd_get_keypress();
            k = KBD_GET_KEY(kp);
            if ((k != MOO_KEY_UNKNOWN) && ((k < MOO_KEY_NUMLOCK) || (k > MOO_KEY_COMPOSE))) {
                got_any = true;
                mouse_getclear_click_hw();
            }
        }
        uiobj_finish_callback_delay_stored();
    }
    if (got_mb) {
        while (mouse_buttons) {
            uiobj_finish_callback_delay_stored();
        }
    }
    mouse_getclear_click_hw();
    mouse_getclear_click_sw();
}
