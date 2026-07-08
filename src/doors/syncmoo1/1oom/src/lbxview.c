#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "main.h"
#include "cfg.h"
#include "comp.h"
#include "fmt_mus.h"
#include "fmt_pic.h"
#include "fmt_sfx.h"
#include "font8x8_draw.h"
#include "gfxaux.h"
#include "hw.h"
#include "kbd.h"
#include "lbx.h"
#include "lbxfont.h"
#include "lbxgfx.h"
#include "lbxpal.h"
#include "lib.h"
#include "log.h"
#include "options.h"
#include "os.h"
#include "util.h"
#include "util_math.h"

/* -------------------------------------------------------------------------- */

static const char *lbxview_font_filename = 0;

/* -------------------------------------------------------------------------- */

const char *idstr_main = "lbxview";

bool main_use_lbx = true;
bool main_use_cfg = true;
bool ui_use_audio = true;

void (*main_usage)(void) = 0;

const struct cmdline_options_s main_cmdline_options_early[] = {
    { NULL, 0, NULL, NULL, NULL, NULL }
};

const struct cmdline_options_s main_cmdline_options[] = {
    { "-font", 1,
      options_set_str_var, (void *)&lbxview_font_filename,
      "FONT.BIN", "Set 8x8 font filename" },
    { NULL, 0, NULL, NULL, NULL, NULL }
};

const struct cfg_items_s game_cfg_items[] = {
    CFG_ITEM_STR("font", &lbxview_font_filename, 0),
    CFG_ITEM_END
};

bool game_str_patch(const char *strid, const char *patchstr, int i)
{
    /* ignore */
    return true;
}

bool game_num_patch(const char *numid, int32_t *patchnums, int first, int num)
{
    /* ignore */
    return true;
}

void game_apply_rules(void)
{

}

/* -------------------------------------------------------------------------- */

#define UI_SCREEN_W 320
#define UI_SCREEN_H 400

static bool in_lbx = false;
static lbxfile_e cur_lbx;
static int cur_items = LBXFILE_NUM;
static int cursor_i = 0;
static int cursor_offs = 0;
static uint8_t *cur_ptr = NULL;
static uint32_t cur_len = 0;
static bool reset_frame = false;
static bool advance_frame = false;
static bool have_sfx = false;
static bool have_mus = false;
static bool flipx = false;
static bool clearbeforedraw = false;
static int test_rotate = 0;
static int test_scale = 0;
static uint8_t textcolor = 8;
static uint32_t cur_key = 0;
static int cur_xoff = 0;
static int cur_yoff = 0;
static struct gfx_aux_s gfxaux;
static uint8_t romfont_08[256 * 8];
static bool have_romfont = false;

/* -------------------------------------------------------------------------- */

static void drawchar_romfont(int dx, int dy, uint8_t c, uint8_t fg, uint8_t bg)
{
    uint8_t *p = hw_video_get_buf() + dx + dy * 320;
    for (int y = 0; y < 8; ++y) {
        uint8_t b;
        b = romfont_08[c * 8 + y];
        for (int x = 0; x < 8; ++x) {
            p[x] = (b & (1 << (7 - x))) ? fg : bg;
        }
        p += 320;
    }
}

static void drawchar(int dx, int dy, uint8_t c, uint8_t fg, uint8_t bg)
{
    if (have_romfont) {
        drawchar_romfont(dx, dy, c, fg, bg);
    } else {
        font8x8_drawchar(dx, dy, 320, c, fg, bg);
    }
}

static void drawstr(int x, int y, const char *str, uint8_t fg, uint8_t bg)
{
    char c;
    while ((c = *str++)) {
        drawchar(x, y, c, fg, bg);
        x += 8;
        if (x >= 320) {
            x = 0;
            y += 8;
        }
    }
}

static void drawstrlen(int x, int y, const char *str, int len, uint8_t fg, uint8_t bg)
{
    char c;
    while (len--) {
        c = *str++;
        drawchar(x, y, c, fg, bg);
        x += 8;
        if (x >= 320) {
            x = 0;
            y += 8;
        }
    }
}

static void drawscreen_outlbx(void)
{
    char linebuf[40 + 1];
    for (lbxfile_e i = 0; i < LBXFILE_NUM; ++i) {
        drawstr(0, 0, " #  Filename    Type Items", textcolor, 0);
        lib_sprintf(linebuf, sizeof(linebuf), "%x", i);
        drawstr(1 * 8, 8 + ((int)i) * 8, linebuf, textcolor, 0);
        drawstr(4 * 8, 8 + ((int)i) * 8, lbxfile_name(i), textcolor, 0);
        if (lbxfile_exists(i)) {
            lib_sprintf(linebuf, sizeof(linebuf), "%u  %u", lbxfile_type(i), lbxfile_num_items(i));
        } else {
            lib_sprintf(linebuf, sizeof(linebuf), "?  ?");
        }
        drawstr(18 * 8, 8 + ((int)i) * 8, linebuf, textcolor, 0);
        drawchar(0, (cursor_i + 1) * 8, ' ', 0, textcolor);
    }
    for (int k = 0; k < 16; ++k) {
        uint8_t *p, *q;
        p = hw_video_get_buf() + 350 * 320 + k * 18;
        q = &lbxpal_cursors[16 * 16 * k];
        for (int y = 0; y < 16; ++y) {
            for (int x = 0; x < 16; ++x) {
                p[x] = q[x * 16 + y];
            }
            p += 320;
        }
    }
    lbxfont_select(5, 1, 0, 0);
    lbxfont_print_str_normal(0, 300 / (test_scale + 1), "Test string", UI_SCREEN_W, test_scale + 1);
    lib_sprintf(linebuf, sizeof(linebuf), "key 0x%x %c", cur_key, cur_key & 0xff);
    drawstr(0, 320, linebuf, textcolor, 0);
}

static void drawscreen_inlbx_rotate(void)
{
    int x, y, x0, y0, x1, y1, angle, w, h, midx, midy, maxwh;
    midx = UI_SCREEN_W / 2;
    midy = 200 / 2;
    w = lbxgfx_get_w(cur_ptr);
    h = lbxgfx_get_h(cur_ptr);
    maxwh = MAX(w, h) * (test_scale + 1);
    if ((maxwh < 80) && (test_rotate == 1)) {
        angle = cur_xoff % 45;
        while (angle < 0) {
            angle += 360;
        }
        for (int i = 0; i < 8; ++i) {
            int xo, yo;
            x = util_math_angle_dist_cos(angle, maxwh);
            y = util_math_angle_dist_sin(angle, maxwh);
            xo = util_math_angle_dist_cos(angle, 90 - maxwh);
            yo = util_math_angle_dist_sin(angle, 90 - maxwh);
            x0 = (midx + xo - x / 2) / (test_scale + 1);
            y0 = (midy + yo - y / 2) / (test_scale + 1);
            x1 = (midx + xo + x / 2) / (test_scale + 1);
            y1 = (midy + yo + y / 2) / (test_scale + 1);
            gfx_aux_draw_frame_from_rotate_limit(x0, y0, x1, y1, &gfxaux, 0, 0, UI_SCREEN_W - 1, 200 - 1, UI_SCREEN_W, test_scale + 1);
            angle = (angle + 45) % 360;
        }
    } else {
        angle = (cur_xoff + cur_yoff * 45) % 360;
        while (angle < 0) {
            angle += 360;
        }
        x = util_math_angle_dist_cos(angle, maxwh);
        y = util_math_angle_dist_sin(angle, maxwh);
        x0 = (midx - x / 2) / (test_scale + 1);
        y0 = (midy - y / 2) / (test_scale + 1);
        x1 = (midx + x / 2) / (test_scale + 1);
        y1 = (midy + y / 2) / (test_scale + 1);
        gfx_aux_draw_frame_from_rotate_limit(x0, y0, x1, y1, &gfxaux, 0, 0, UI_SCREEN_W - 1, 200 - 1, UI_SCREEN_W, test_scale + 1);
    }
}

static void drawscreen_inlbx_data(const uint8_t *p, int len)
{
    char linebuf[40 + 1];
    int pos = cur_yoff * 40;
    if (pos >= len) {
        pos = (len / 40) * 40;
        if (pos == len) {
            pos -= 40;
        }
        SETMAX(pos, 0);
    }
    len -= pos;
    lib_sprintf(linebuf, sizeof(linebuf), "pos:%i (%x) len:%i", pos, pos, len);
    drawstr(0, 200 + 8 * 2, linebuf, textcolor, 0);
    SETMIN(len, 40 * 25);
    drawstrlen(0, 0, (const char *)&p[pos], len, textcolor, 0);
}

static void drawscreen_inlbx(void)
{
    char linebuf[256];
    if (cursor_i >= 19) {
        cursor_offs = cursor_i - 19;
    }
    if (cursor_i < cursor_offs) {
        cursor_offs = 0;
    }
    drawstr(8, 200 + 8 * 4, "#  Name", textcolor, 0);
    for (int i = 0; i < 20; ++i) {
        int j = i + cursor_offs;
        if (j >= cur_items) {
            break;
        }
        lib_sprintf(linebuf, sizeof(linebuf), "%2x ", j);
        const char *name = lbxfile_item_name(cur_lbx, j);
        for (int k = 0; k < 32; ++k) {
            linebuf[3 + k] = name[k] ? name[k] : ' ';
        }
        linebuf[3 + 32] = 0;
        drawstr(8, 200 + 8 * (5 + i), linebuf, textcolor, 0);
        drawchar(0, 200 + (cursor_i - cursor_offs + 5) * 8, ' ', 0, textcolor);
    }
    {
        uint32_t offs, len;
        offs = lbxfile_item_offs(cur_lbx, cursor_i);
        len = lbxfile_item_len(cur_lbx, cursor_i);
        lib_sprintf(linebuf, sizeof(linebuf), "offs:%x:.%x len:%x (%i)", offs, offs+len, len, len);
        drawstr(0, 200 + 8 * 0, linebuf, textcolor, 0);
    }
    if (lbxfile_type(cur_lbx) == LBX_TYPE_GFX) {
        uint8_t *p = cur_ptr;
        uint8_t frame;
        frame = lbxgfx_get_frame(p);
        if (reset_frame) {
            reset_frame = false;
            lbxgfx_set_frame_0(p);
            frame = 0;
        }
        if (clearbeforedraw) {
            memset(hw_video_get_buf(), 0, 320 * 200);
            gfx_aux_setup(&gfxaux, p, 0);
            lbxgfx_draw_frame_do(gfxaux.data, p, gfxaux.w, test_scale + 1);
        } else {
            gfx_aux_draw_frame_to(p, &gfxaux);
        }
        if (flipx) {
            gfx_aux_flipx(&gfxaux);
        }
        if (test_rotate) {
            drawscreen_inlbx_rotate();
        } else {
            gfx_aux_draw_frame_from_limit(cur_xoff, cur_yoff, &gfxaux, 0, 0, UI_SCREEN_W - 1, 200 - 1, UI_SCREEN_W, test_scale + 1);
        }
        if (advance_frame) {
            advance_frame = false;
        } else {
            lbxgfx_set_frame(p, frame);
        }
        lib_sprintf(linebuf, sizeof(linebuf), "%ix%i f:%i/%i(%i)%c (%x)(%x)(%x) if:%i fmt:%i | %i %i",
                lbxgfx_get_w(p), lbxgfx_get_h(p),
                frame, lbxgfx_get_frames(p), lbxgfx_get_frames2(p),
                lbxgfx_get_frameclearflag(p, frame) ? 'c' : '-',
                lbxgfx_get_ehandle(p), lbxgfx_get_epage(p), lbxgfx_get_offs0c(p),
                lbxgfx_get_indep(p), lbxgfx_get_format(p), cur_xoff, cur_yoff
               );
        drawstr(0, 200 + 8 * 1, linebuf, textcolor, 0);
        if (lbxgfx_has_palette(p)) {
            lib_sprintf(linebuf, sizeof(linebuf), "pal o:%x do:%x f:%i n:%i (%02x)",
                    lbxgfx_get_paloffs(p),
                    lbxgfx_get_paldataoffs(p), lbxgfx_get_palfirst(p),
                    lbxgfx_get_palnum(p), lbxgfx_get_paloffs06(p)
                   );
            drawstr(0, 200 + 8 * 2, linebuf, textcolor, 0);
        }
    } else if (lbxfile_type(cur_lbx) == LBX_TYPE_DATA) {
        uint16_t num, size, view;
        uint8_t *p = cur_ptr;
        num = GET_LE_16(p);
        size = GET_LE_16(p + 2);
        view = cur_xoff;
        SETRANGE(view, 0, num - 1);
        lib_sprintf(linebuf, sizeof(linebuf), "num:%i sz:%x v:%i", num, size, view);
        drawstr(0, 200 + 8 * 1, linebuf, textcolor, 0);
        p = cur_ptr + 4 + view * size;
        drawscreen_inlbx_data(p, size);
    } else {
        int len = lbxfile_item_len(cur_lbx, cursor_i);
        drawscreen_inlbx_data(cur_ptr, len);
    }
}

static void drawscreen(void)
{
    memset(hw_video_get_buf(), 0, 320 * 400);
    if (!in_lbx) {
        drawscreen_outlbx();
    } else {
        drawscreen_inlbx();
    }
}

static void do_lbx_sound(uint32_t k)
{
    if ((fmt_sfx_detect(cur_ptr, cur_len) != SFX_TYPE_UNKNOWN) && !KBD_MOD_ONLY_CTRL(k)) {
        if (KBD_GET_MOD(k) == 0) {
            if (!have_sfx) {
                if (hw_audio_sfx_init(0, cur_ptr, cur_len) == 0) {
                    have_sfx = true;
                }
            }
            if (have_sfx) {
                hw_audio_sfx_play(0);
            }
        }
        if (KBD_MOD_ONLY_SHIFT(k)) {
            uint8_t *sdata = NULL;
            uint32_t slen = 0;
            if (fmt_sfx_convert(cur_ptr, cur_len, &sdata, &slen, NULL, 48000, true)) {
                util_file_save("z1.wav", sdata, slen);
            }
        }
    } else if (fmt_mus_detect(cur_ptr, cur_len) != MUS_TYPE_UNKNOWN) {
        if ((KBD_GET_MOD(k) == 0) || KBD_MOD_ONLY_CTRL(k)) {
            if (!have_mus) {
                if (hw_audio_music_init(0, cur_ptr, cur_len) == 0) {
                    have_mus = true;
                }
            }
            if (have_mus) {
                hw_audio_music_play(0);
            }
        }
        if (KBD_MOD_ONLY_SHIFT(k)) {
            uint8_t *sdata = NULL;
            uint32_t slen = 0;
            bool loops = false;
            if (fmt_mus_convert_xmid(cur_ptr, cur_len, &sdata, &slen, &loops)) {
                util_file_save("z1.mid", sdata, slen);
            }
        }
    }
}

static void do_lbx_gfx(uint32_t k)
{
    int w, h;
    w = lbxgfx_get_w(cur_ptr);
    h = lbxgfx_get_h(cur_ptr);
    if ((w > 0) && (w <= 320) && (h > 0) && (h <= 200)) {
        struct pic_s pic;
        pic.type = PIC_TYPE_PCX;
        pic.w = w;
        pic.h = h;
        pic.pitch = UI_SCREEN_W;
        pic.pal = lbxpal_palette;
        if (KBD_MOD_ONLY_SHIFT(k)) {
            pic.pix = hw_video_get_buf_front();
            fmt_pic_save("z0.pcx", &pic);
        } else if (KBD_MOD_ONLY_CTRL(k)) {
            char bufname[16];
            int frames = lbxgfx_get_frames(cur_ptr);
            lib_strcpy(bufname, lbxfile_name(cur_lbx), sizeof(bufname));
            {
                char *p;
                p = strchr(bufname, '.');
                if (p) { *p = 0; }
            }
            lbxgfx_set_frame_0(cur_ptr);
            memset(hw_video_get_buf(), 0, 320 * 200);
            pic.pix = hw_video_get_buf();
            for (int f = 0; f < frames; ++f) {
                char fname[32];
                lbxgfx_draw_frame(0, 0, cur_ptr, UI_SCREEN_W, 1);
                lib_sprintf(fname, sizeof(fname), "z_%s_%02x_%03i.pcx", bufname, cursor_i, f);
                fmt_pic_save(fname, &pic);
            }
        }
    }
}

static int loadfont(void)
{
    FILE *fd;
    bool is_default;
    const char *fname;
    char *fnamealloc = 0;
    int res = -1;
    if (!lbxview_font_filename) {
        fnamealloc = util_concat(os_get_path_data(), FSDEV_DIR_SEP_STR, "romfont.bin", NULL);
        fname = fnamealloc;
        is_default = true;
    } else {
        fname = lbxview_font_filename;
        is_default = false;
    }
    log_message("Load font '%s'\n", fname);
    fd = fopen(fname, "rb");
    if (fd && (fread(romfont_08, 256 * 8, 1, fd) == 1)) {
        res = 0;
    } else {
        log_error("loading font '%s'!\n", fname);
        if (is_default) {
            log_message("!! You need a 8x8 1bpp font (2048 bytes) file to use this program.\n"
                        "!! If you have one ready elsewhere, use -font <file> to use it.\n"
                        "!! If you have DOSBox and base64;\n"
                        "!! 1) Generate a font dumper:  (source in doc/ext/fontdump.asm)\n"
                        "!!  echo uDARtwPNEAa6MwExybQ8zSFyGonDuQAIieoftEDNIXIMOch1CLQ+zSGwAHMCsAG0TM0hci5iaW4A | base64 -d > fontdump.com\n"
                        "!! 2) Run fontdump.com in DOSBox to generate the font file r.bin.\n"
                        "!! 3) Run lbxview again with -font r.bin\n"
                       );
        }
    }
    if (fd) {
        fclose(fd);
        fd = NULL;
    }
    lib_free(fnamealloc);
    fnamealloc = NULL;
    fname = NULL;
    return res;
}

void ui_early_show_message_box(const char *msg)
{

}

/* -------------------------------------------------------------------------- */

int main_handle_option(const char *argv)
{
    static int optn = 0;
    uint32_t v;
    if (!util_parse_number(argv, &v)) {
        log_error("parsing number '%s'\n", argv);
        return -1;
    }
    switch (optn) {
        case 0:
            if (v >= LBXFILE_NUM) {
                log_error("invalid LBX file ID 0x%x\n", v);
                return -1;
            }
            if (!lbxfile_exists(v)) {
                log_error("file %s missing\n", lbxfile_name(v));
                return -1;
            }
            in_lbx = true;
            cur_lbx = v;
            cur_items = lbxfile_num_items(cur_lbx);
            cur_ptr = lbxfile_item_get_with_len(cur_lbx, cursor_i, &cur_len);
            ++optn;
            break;
        case 1:
            cursor_i = v;
            cursor_offs = v;
            {
                uint8_t *p = lbxfile_item_get_with_len(cur_lbx, cursor_i, &cur_len);
                if (cur_ptr) {
                    lbxfile_item_release(cur_lbx, cur_ptr);
                }
                cur_ptr = p;
            }
            break;
        default:
            log_error("too many params'\n");
            return -1;
    }
    return 0;
}

void main_do_shutdown(void)
{
}

int main_do(void)
{
    if (!loadfont()) {
        have_romfont = true;
    }
    if (hw_video_init(320, 400)) {
        return 1;
    }
    lbxfont_init();
    lbxpal_init();
    lbxpal_select(0, -1, 0);
    if (cur_ptr && (lbxfile_type(cur_lbx) == LBX_TYPE_GFX)) {
        lbxgfx_apply_palette(cur_ptr);
    }
    lbxpal_update();
    textcolor = lbxpal_find_closest(0x1f, 0x1f, 0x1f);
    drawscreen();
    hw_video_draw_buf();
    while (1) {
        bool change_cur_ptr;
        hw_event_handle();
        if (kbd_have_keypress()) {
            uint32_t k;
            k = kbd_get_keypress();
            cur_key = k;
            change_cur_ptr = false;
            switch (k & 0xff) {
                case MOO_KEY_ESCAPE:
                    if (in_lbx) {
                        in_lbx = false;
                        cursor_i = (int)cur_lbx;
                        cursor_offs = 0;
                        cur_items = LBXFILE_NUM;
                        lbxfile_item_release(cur_lbx, cur_ptr);
                        cur_ptr = NULL;
                    } else {
                        goto done;
                    }
                    break;
                case MOO_KEY_UP:
                    if (--cursor_i < 0) { cursor_i = cur_items - 1; }
                    change_cur_ptr = in_lbx;
                    break;
                case MOO_KEY_DOWN:
                    if (++cursor_i >= cur_items) { cursor_i = 0; }
                    change_cur_ptr = in_lbx;
                    break;
                case MOO_KEY_PAGEUP:
                    cursor_i -= 15;
                    if (cursor_i < 0) { cursor_i = cur_items - 1; }
                    change_cur_ptr = in_lbx;
                    break;
                case MOO_KEY_PAGEDOWN:
                    cursor_i += 15;
                    if (cursor_i >= cur_items) { cursor_i = 0; }
                    change_cur_ptr = in_lbx;
                    break;
                case MOO_KEY_RIGHT:
                    advance_frame = true;
                    break;
                case MOO_KEY_LEFT:
                    reset_frame = true;
                    break;
                case MOO_KEY_RETURN:
                    if (!in_lbx) {
                        if (lbxfile_exists(cursor_i)) {
                            in_lbx = true;
                            cur_lbx = cursor_i;
                            cur_items = lbxfile_num_items(cur_lbx);
                            cursor_i = 0;
                            cursor_offs = 0;
                            change_cur_ptr = true;
                        }
                    } else {
                        if (lbxfile_type(cur_lbx) == LBX_TYPE_SOUND) {
                            do_lbx_sound(k);
                        }
                        if (lbxfile_type(cur_lbx) == LBX_TYPE_GFX) {
                            do_lbx_gfx(k);
                        }
                    }
                    break;
                case MOO_KEY_w:
                    --cur_yoff;
                    break;
                case MOO_KEY_s:
                    ++cur_yoff;
                    break;
                case MOO_KEY_a:
                    --cur_xoff;
                    break;
                case MOO_KEY_d:
                    ++cur_xoff;
                    break;
                case MOO_KEY_f:
                    hw_audio_music_fadeout();
                    break;
                case MOO_KEY_q:
                    test_rotate = (test_rotate + 1) % 3;
                    break;
                case MOO_KEY_z:
                    test_scale = (test_scale + 1) % 3;
                    break;
                case MOO_KEY_c:
                    clearbeforedraw = !clearbeforedraw;
                    break;
                case MOO_KEY_0:
                case MOO_KEY_1:
                case MOO_KEY_2:
                case MOO_KEY_3:
                case MOO_KEY_4:
                case MOO_KEY_5:
                case MOO_KEY_6:
                case MOO_KEY_7:
                case MOO_KEY_8:
                case MOO_KEY_9:
                    lbxpal_select((k & 0xff) - MOO_KEY_0, -1, 0);
                    lbxpal_update();
                    textcolor = lbxpal_find_closest(0x1f, 0x1f, 0x1f);
                    break;
                case MOO_KEY_e:
                    if (in_lbx) {
                        char bufname[16];
                        char bufnum[4];
                        char *p, *name;
                        lib_sprintf(bufnum, sizeof(bufnum), "%02x", cursor_i);
                        lib_strcpy(bufname, lbxfile_name(cur_lbx), sizeof(bufname));
                        p = strchr(bufname, '.');
                        if (p) { *p = 0; }
                        name = util_concat("z_", bufname, "_", bufnum, ".bin", NULL);
                        util_file_save(name, cur_ptr, cur_len);
                        lib_free(name);
                    }
                    break;
            }
            if (change_cur_ptr) {
                uint8_t *p = lbxfile_item_get_with_len(cur_lbx, cursor_i, &cur_len);
                if (cur_ptr) {
                    lbxfile_item_release(cur_lbx, cur_ptr);
                }
                cur_ptr = p;
                if (lbxfile_type(cur_lbx) == LBX_TYPE_GFX) {
                    lbxgfx_apply_palette(cur_ptr);
                    lbxpal_update();
                }
                reset_frame = true;
                advance_frame = false;
                cur_xoff = 0;
                cur_yoff = 0;
                if (have_sfx) {
                    have_sfx = false;
                    hw_audio_sfx_release(0);
                }
                if (have_mus) {
                    have_mus = false;
                    /* hw_audio_music_release(0); */
                }
                textcolor = lbxpal_find_closest(0x1f, 0x1f, 0x1f);
            }
            drawscreen();
            hw_video_draw_buf();
        } else {
            hw_video_redraw_front();
        }
    }
done:
    if (have_sfx) {
        have_sfx = false;
        hw_audio_sfx_release(0);
    }
    if (have_mus) {
        have_mus = false;
        hw_audio_music_release(0);
    }
    lbxpal_shutdown();
    lbxfont_shutdown();
    return 0;
}
