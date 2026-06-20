// render_text.c -- text/block render tiers for syncdoom.
// The core (lines marked below) is lifted from ludocode/doom-cli (GPL-2.0):
// doomgeneric_cli.c color quantizers + dithering + draw_half/draw_space.
// Adapted: backend/input/main stripped; output drained by the caller; the
// framebuffer source is DG_ScreenBuffer; block glyph is charset-selectable.
// See CREDITS.

#define _GNU_SOURCE     // mempcpy/stpcpy used by the 24-bit color path
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include <limits.h>

#include "doomgeneric.h"     // DG_ScreenBuffer, DOOMGENERIC_RESX/RESY, DG_GetTicksMs
#include "cli_data.h"        // noise_textures, noise_texture_count
#include "render_text.h"

// mempcpy/stpcpy are glibc extensions; MSVC's CRT lacks them. Provide the
// trivial equivalents (each returns a pointer to the byte past what it wrote).
#ifdef _WIN32
static inline void *mempcpy(void *dst, const void *src, size_t n)
{
    return (char *)memcpy(dst, src, n) + n;
}
static inline char *stpcpy(char *dst, const char *src)
{
    size_t n = strlen(src);
    return (char *)memcpy(dst, src, n + 1) + n;
}
#endif

// --- globals the lifted doom-cli renderer expects (were in its backend) ------
static int       columns = 80;
static int       dest_width;
static int       dest_height;
static uint32_t *dest_buffer;

typedef enum { cli_mode_sextant = 1, cli_mode_quadrant, cli_mode_half, cli_mode_space,
               cli_mode_blocks } cli_mode_t;
static cli_mode_t cli_mode = cli_mode_half;

// Active block charset (RT_BLOCKS / half-block pick their glyph table from this).
static rt_charset_t g_charset = RT_UTF8;
// CP437-safe 2x2 block+shade glyph table (set by charset in rt_config); 16 entries
// indexed by the same TL/TR/BL/BR bit pattern draw_subcell() computes.
static const char *const *g_blocks = NULL;

typedef enum { cli_colors_24bit = 1, cli_colors_8bit, cli_colors_4bit, cli_colors_3bit,
               cli_colors_light, cli_colors_dark } cli_colors_t;
static cli_colors_t cli_colors = cli_colors_4bit;

// Upper-half block glyph: UTF-8 U+2580 by default, CP437 byte 0xDF for BBS terms.
#define UPPER_HALF "\xE2\x96\x80"
static const char *g_block_upper     = UPPER_HALF;
static size_t      g_block_upper_len = sizeof(UPPER_HALF) - 1;

// syncdoom pumps input itself; the lifted start_row() calls this.
#define DOOMCLI_READ_INPUT() ((void)0)

// quadrant/sextant draws + their glyph tables are appended at end of file.
static void draw_subcell(const char *const *glyphs);
static void draw_quadrant();
static void draw_blocks();
static void draw_sextant();
static void draw_sextant_bw();

// RT_BLOCKS 2x2 glyph tables, indexed by the TL|TR<<1|BL<<2|BR<<3 luma pattern
// draw_subcell() computes. Axis-aligned halves / full / empty are exact; the
// single-quadrant (1 cell) and diagonal (2 opposite cells) patterns -- which
// CP437/legacy fonts can't draw -- are approximated by light/medium/dark shades.
// UTF-8 variant: U+2580/2584/258C/2590 halves, U+2588 full, U+2591/2592/2593
// shades -- all Unicode 1.0, present in Windows conhost's Consolas etc.
static const char *const blocks_utf8[16] = {
    " ",            "\xE2\x96\x91", "\xE2\x96\x91", "\xE2\x96\x80",  //  0..3
    "\xE2\x96\x91", "\xE2\x96\x8C", "\xE2\x96\x92", "\xE2\x96\x93",  //  4..7
    "\xE2\x96\x91", "\xE2\x96\x92", "\xE2\x96\x90", "\xE2\x96\x93",  //  8..11
    "\xE2\x96\x84", "\xE2\x96\x93", "\xE2\x96\x93", "\xE2\x96\x88",  // 12..15
};
// CP437 variant: the same glyphs as raw code-page bytes (0xB0/B1/B2 shades,
// 0xDB full, 0xDC/DD/DE/DF halves) for a true CP437 terminal.
static const char *const blocks_cp437[16] = {
    " ",     "\xB0", "\xB0", "\xDF",   //  0..3
    "\xB0",  "\xDD", "\xB1", "\xB2",   //  4..7
    "\xB0",  "\xB1", "\xDE", "\xB2",   //  8..11
    "\xDC",  "\xB2", "\xB2", "\xDB",   // 12..15
};

// ======================================================================
// Lifted verbatim from doom-cli doomgeneric_cli.c lines 274-773 (GPL-2.0)
// (u8_to_str, noise/dither, output buffer, color quantizers, output_*,
//  start_row, draw_space, draw_half). See CREDITS.
// ======================================================================
static const char* u8_to_str[] = {
    "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15",
    "16", "17", "18", "19", "20", "21", "22", "23", "24", "25", "26", "27", "28", "29", "30", "31",
    "32", "33", "34", "35", "36", "37", "38", "39", "40", "41", "42", "43", "44", "45", "46", "47",
    "48", "49", "50", "51", "52", "53", "54", "55", "56", "57", "58", "59", "60", "61", "62", "63",
    "64", "65", "66", "67", "68", "69", "70", "71", "72", "73", "74", "75", "76", "77", "78", "79",
    "80", "81", "82", "83", "84", "85", "86", "87", "88", "89", "90", "91", "92", "93", "94", "95",
    "96", "97", "98", "99", "100", "101", "102", "103", "104", "105", "106", "107", "108", "109", "110", "111",
    "112", "113", "114", "115", "116", "117", "118", "119", "120", "121", "122", "123", "124", "125", "126", "127",
    "128", "129", "130", "131", "132", "133", "134", "135", "136", "137", "138", "139", "140", "141", "142", "143",
    "144", "145", "146", "147", "148", "149", "150", "151", "152", "153", "154", "155", "156", "157", "158", "159",
    "160", "161", "162", "163", "164", "165", "166", "167", "168", "169", "170", "171", "172", "173", "174", "175",
    "176", "177", "178", "179", "180", "181", "182", "183", "184", "185", "186", "187", "188", "189", "190", "191",
    "192", "193", "194", "195", "196", "197", "198", "199", "200", "201", "202", "203", "204", "205", "206", "207",
    "208", "209", "210", "211", "212", "213", "214", "215", "216", "217", "218", "219", "220", "221", "222", "223",
    "224", "225", "226", "227", "228", "229", "230", "231", "232", "233", "234", "235", "236", "237", "238", "239",
    "240", "241", "242", "243", "244", "245", "246", "247", "248", "249", "250", "251", "252", "253", "254", "255",
};



/*
 * Noise
 *
 * For the paletted modes, we dither by sampling blue noise. The noise is a set
 * of 64 16x16 blue noise images which we rotate through. The noise textures
 * are at the bottom of the file because they're huge.
 *
 * TODO the way the noise is applied right now is bad. It's applied per
 * character; it needs to be applied per pixel. Should just make an apply noise
 * function that takes rgb pointers, force inline it (or wrap it in a macro
 * that checks whether noise is enabled)
 */

bool noise_enabled = true;
static int noise_current = 0;
static uint32_t noise_last_time = 0;
static int noise_speed = 75; // milliseconds
static bool noise_scaled = false;   // the scaling in init_noise() rewrites
                                    // noise_textures in place, so it must run at
                                    // most once -- re-running it (rt_config is
                                    // called on every F4 tier cycle) re-scales
                                    // already-scaled data and washes the dither
                                    // out until it vanishes for good.
static int  rt_dither_pref = -1;    // -1 auto (color-depth default), 0 off, 1 on;
                                    // set by the door from [video] dither / Ctrl-N.

#define NOISE_SAMPLE(x, y) \
        noise_textures[noise_current][((x) & 15) + (((y) & 15) * 16)]
/*
 * Initializes the noise.
 *
 * All values are scaled such that, when 128 is subtracted from them, they
 * become an offset to add to a color channel to dither it.
 */
static void init_noise(void) {
    noise_last_time = DG_GetTicksMs();

    int scale = 0; // percent
    switch (cli_colors) {
        case cli_colors_dark:
        case cli_colors_light:
            scale = 95;
            break;
        case cli_colors_3bit: scale = 30; break;
        case cli_colors_4bit: scale = 0; break;  // 16-color dithering reads as grainy/busy -> off
        case cli_colors_8bit: scale = 2; break;
        default: break;
    }
    if (scale == 0) {
        noise_enabled = false;       // this color depth doesn't dither at all
        return;
    }
    // depth supports dither -- honor the override: off = forced off; auto/on = on.
    noise_enabled = (rt_dither_pref != 0);

    // cli_colors is fixed for the life of the process (set once from -colors /
    // terminal.ini), so the textures only need scaling once. The scale below is
    // destructive (noise_textures rewritten in place); re-applying it on a later
    // rt_config (F4 tier cycle) flattens the noise until it disappears.
    if (noise_scaled)
        return;
    noise_scaled = true;

    int base = 255 * (100 - scale) / 200;

    for (size_t tex = 0; tex < noise_texture_count; ++ tex) {
        for (size_t i = 0; i < 16*16; ++i) {
            uint32_t val = noise_textures[tex][i];
            uint32_t blue = (val >> 16) & 0xff;
            uint32_t green = (val >> 8) & 0xff;
            uint32_t red = val & 0xff;
            blue = blue * scale / 100 + base;
            green = green * scale / 100 + base;
            red = red * scale / 100 + base;
            //printf("red: orig %u base %u scale %u%% result %u\n", val&0xff,base,scale,red);
            val = (blue << 16) | (green << 8) | red;
            noise_textures[tex][i] = val;
        }
    }
}

// Set the dither preference (-1 auto, 0 off, 1 on) and re-derive noise_enabled
// for the current color depth. Cheap: the textures are scaled at most once (the
// noise_scaled guard), so this just flips whether the (already-prepared) dither
// is applied. Called by the door from the [video] dither config and the Ctrl-N
// toggle. (Has no visible effect at a depth that never dithers, e.g. 16-color.)
void rt_set_dither(int pref)
{
    rt_dither_pref = pref;
    init_noise();
}



/*
 * Output buffering
 *
 * We buffer the output ourselves in an attempt to prevent flickering.
 */

static char* buffer;
static size_t buffer_capacity;
static size_t buffer_count;

static void buffer_append(const char* bytes, size_t count) {
    size_t total = buffer_count + count;
    if (total > buffer_capacity) {
        while (total > buffer_capacity)
            buffer_capacity *= 2;
        buffer = realloc(buffer, buffer_capacity);
        if (buffer == NULL) {
            fprintf(stderr, "Out of memory re-allocating output buffer!\n");
            abort();
        }
    }
    memcpy(buffer + buffer_count, bytes, count);
    buffer_count = total;
}

#define buffer_append_literal(str) buffer_append(str, sizeof(str) - 1)

static void buffer_append_cstr(const char* bytes) {
    buffer_append(bytes, strlen(bytes));
}

static void buffer_append_pad(size_t actual, size_t desired) {
    while (actual++ < desired)
        buffer_append(" ", 1);
}

static void buffer_append_byte_decimal(uint32_t value) {
    buffer_append_cstr(u8_to_str[value]);
}



/*
 * Colors
 */

/*
 * Weights for calculating brightness of pixel, as a fraction of 255. These
 * numbers are from ITU BT.709.
 */
#define LUMA_WEIGHT_RED 54      // 0.2126
#define LUMA_WEIGHT_GREEN 183   // 0.7152
#define LUMA_WEIGHT_BLUE 18     // 0.0722

#define LUMA_P(x) \
    (x[0] * LUMA_WEIGHT_BLUE + x[1] * LUMA_WEIGHT_GREEN + x[2] * LUMA_WEIGHT_RED)

#define LUMA(r, g, b) \
    (b * LUMA_WEIGHT_BLUE + g * LUMA_WEIGHT_GREEN + r * LUMA_WEIGHT_RED)

/*
 * Weights for calculating color differences, as a fraction of 16.
 */
#define DIFF_WEIGHT_RED 5
#define DIFF_WEIGHT_GREEN 7
#define DIFF_WEIGHT_BLUE 4

/**
 * Searches for the closest color in the given colors array and returns its code.
 */
static int color_search(const uint8_t* colors, size_t color_count,
        int red, int green, int blue)
{
    int best_code = 0;
    int best_error = INT_MAX;
    const uint8_t* end = colors + color_count * 4;
    while (colors != end) {
        int rc = colors[1];
        int gc = colors[2];
        int bc = colors[3];
        int rcd = ((red - rc) * DIFF_WEIGHT_RED);
        int gcd = ((green - gc) * DIFF_WEIGHT_GREEN);
        int bcd = ((blue - bc) * DIFF_WEIGHT_BLUE);
        int error = rcd * rcd + gcd * gcd + bcd * bcd;
        //int error = abs(rcd) + abs(gcd) + abs(bcd);
        if (error < best_error) {
            best_error = error;
            best_code = *colors;
        }
        colors += 4;
    }
    return best_code;
}

/**
 * Returns the 3-bit ANSI foreground color code for the given color.
 *
 * For background colors, add 10.
 */
static int color_3bit(int red, int green, int blue) {

    // These are not the real colors; we've multiplied the VGA palette by 1.5.
    // Really we just want as much contrast as possible. We send bold as well
    // to try to brighten the screen.
    static const uint8_t colors[] = {
        // code     red    green     blue
            30,       0,       0,       0,
            31,     255,       0,       0,
            32,       0,     255,       0,
            33,     255,     128,       0,
            34,       0,       0,     255,
            35,     255,       0,     255,
            36,       0,     255,     255,
            37,     255,     255,     255,
    };

    return color_search(colors, sizeof(colors) / 4, red, green, blue);
}

/**
 * Returns the 4-bit ANSI foreground color code for the given color.
 *
 * For background colors, add 10.
 */
static int color_4bit(int red, int green, int blue) {

    // These are the real VGA colors.
    static const uint8_t colors[] = {
        // code     red    green     blue
            30,       0,       0,       0,
            31,     170,       0,       0,
            32,       0,     170,       0,
            33,     170,      85,       0,  // dark orange, not dark yellow
            34,       0,       0,     170,
            35,     170,       0,     170,
            36,       0,     170,     170,
            37,     170,     170,     170,
            90,      85,      85,      85,
            91,     255,      85,      85,
            92,      85,     255,      85,
            93,     255,     255,      85,
            94,      85,      85,     255,
            95,     255,      85,     255,
            96,      85,     255,     255,
            97,     255,     255,     255,
    };
    return color_search(colors, sizeof(colors) / 4, red, green, blue);
}

static inline int clamp(int value, int min, int max) {
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

/**
 * Returns the 8-bit ANSI color code for the given color.
 *
 * We calculate the nearest color from the 6x6x6 color cube and the nearest
 * of the 24 grayscale colors. We use whichever is closer, except we bias
 * towards grayscale for darker areas.
 *
 * TODO fix, bias towards grayscale shouldn't be necessary. if we do bias we
 * should just brighten up the darkest colors. maybe even just adding 15 to
 * each color channel, or scale 0-255 to say 20-255, to bias towards having any
 * color at all. not sure how to handle error calculation in this case
 *
 * TODO another option is to select between grayscale and color based on how
 * much color there is. e.g. (r-b)^2+(r-g)^2+(b-g)^2 is amount of color, have
 * some threshold to use the color channels. probably no point if we do the
 * error calculation correctly
 *
 * TODO the ends of the 24-shade grayscale palette are #080808 and #eeeeee. We
 * don't need currently need any special cases for this though because #000000
 * and #ffffff exist in the color cube... but we might need a special case if
 * we overly bias it.
 */
static int color_8bit(int red, int green, int blue) {
    red = clamp(red, 0, 255);
    green = clamp(green, 0, 255);
    blue = clamp(blue, 0, 255);

    // TODO add x/y coordinate parameters and sample blue noise

    // TODO maybe replace these divisions with a multiplication and shift (good
    // compilers can do this for us but toy compilers won't)


    // TODO these next two blocks don't do a very good job of picking colors.
    // e.g. rgb of 127 gives us the grayscale block 112, not 128. needs fixing.

    // calculate 6x6x6 color cube
    // TODO color cube colors are not linear! maybe the simplest and fastest
    // way to do this is to manually create a lookup table to convert each
    // channel. we could put the bias right in the table.
    int r6 = red / 43;
    int g6 = green / 43;
    int b6 = blue / 43;
    int r6d = ((red - r6 * 43) * DIFF_WEIGHT_RED);
    int g6d = ((green - g6 * 43) * DIFF_WEIGHT_GREEN);
    int b6d = ((blue - b6 * 43) * DIFF_WEIGHT_BLUE);
    int error6 = r6d * r6d + g6d * g6d + b6d * b6d;
    //int error6 = abs(r6d) + abs(g6d) + abs(b6d);
    //printf("\033[0m   %i %i %i\n", r6d,g6d,b6d);

    // calculate grayscale
    // TODO these calculations are wrong, colors range from #08 to #ee.
    // probably should also just use a lookup table
    int luma = LUMA(red, green, blue);
    int gray = (luma * 24) >> 16;
    int gray256 = gray * 256 / 24;
    int rgd = ((red - gray256) * DIFF_WEIGHT_RED);
    int ggd = ((green - gray256) * DIFF_WEIGHT_GREEN);
    int bgd = ((blue - gray256) * DIFF_WEIGHT_BLUE);
    int errorg = rgd * rgd + ggd * ggd + bgd * bgd;
    //int errorg = abs(rgd) + abs(ggd) + abs(bgd);
    //printf("\033[0m   %i %i %i\n", rgd,ggd,bgd);

    // if luma is small, reduce the grayscale error
    /*
    if (luma < 16384) {
        errorg *= luma >> 8;
        errorg >>= 4;
    }
    */

    // TODO this comment is wrong, we DON'T want to bias above the 1/8 or 1/4 threshold
        // Bias grayscale based on luma. This decreases errorg in the bottom 1/8 of
        // luma and increases it otherwise. This ensures dark areas are playable
        // while basically never using grayscale in brighter ares (unless the color
        // is actually gray.)

    //printf("\033[0m%u %u   %u %u %u   %u    %u\n", error6, errorg,r6,g6,b6,gray,luma);

    return ((error6 < errorg)) ?
            16 + b6 + g6 * 6 + r6 * 36 : // color cube
            232 + gray;                  // grayscale
}

// --- SGR change-detection cache ---------------------------------------------
// A terminal retains the last colors it was given, so re-sending an identical
// SGR escape for every cell is pure wasted bandwidth -- expensive over a BBS
// link. We remember the fg and bg actually emitted and skip any component that
// hasn't changed (in a flat region, that means emitting *no* escape at all --
// just the glyph). The keys are reset to -1 ("unknown") wherever we return the
// terminal to its default colors: each \033[0m (output_newline) and the top of
// every frame (the prior frame ends on a \033[0m, so colors are default there).
static long rt_last_fg = -1;
static long rt_last_bg = -1;

static void rt_reset_color_cache(void) { rt_last_fg = -1; rt_last_bg = -1; }

// Writes the SGR parameter list (no leading "\033[", no trailing 'm') that
// selects red,green,blue as a foreground (is_bg=false) or background
// (is_bg=true) color in the active color mode, and returns an integer key that
// uniquely identifies it for change detection. 'out' needs room for ~18 bytes.
static long rt_color_params(char *out, int red, int green, int blue, bool is_bg) {
    switch (cli_colors) {
        case cli_colors_8bit: {
            int code = color_8bit(red, green, blue);
            char *p = out;
            *p++ = is_bg ? '4' : '3';
            p = mempcpy(p, "8;5;", 4);
            stpcpy(p, u8_to_str[code]);
            return code;
        }
        case cli_colors_4bit:
        case cli_colors_3bit: {
            int code = (cli_colors == cli_colors_4bit)
                     ? color_4bit(red, green, blue)
                     : color_3bit(red, green, blue);
            int sgr = is_bg ? code + 10 : code;
            strcpy(out, u8_to_str[sgr]);
            return sgr;
        }
        case cli_colors_24bit:
        default: {
            int r = red & 0xff, g = green & 0xff, b = blue & 0xff;
            char *p = out;
            p = mempcpy(p, is_bg ? "48;2;" : "38;2;", 5);
            p = stpcpy(p, u8_to_str[r]); *p++ = ';';
            p = stpcpy(p, u8_to_str[g]); *p++ = ';';
            p = stpcpy(p, u8_to_str[b]); *p   = '\0';
            return ((long)r << 16) | (g << 8) | b;
        }
    }
}

// Outputs a background color (skipped if unchanged since the last emit).
static void output_bg_color(int x, int y, int red, int green, int blue) {
    if (noise_enabled) {
        uint32_t noise_color = NOISE_SAMPLE(x, y);
        red += (noise_color >> 16 & 0xff) - 128;
        green += ((noise_color >> 8) & 0xff) - 128;
        blue += ((noise_color) & 0xff) - 128;
    }

    char bp[24];
    long key = rt_color_params(bp, red, green, blue, true);
    if (key == rt_last_bg)
        return;                       // unchanged -> emit nothing
    rt_last_bg = key;
    buffer_append_literal("\033[");
    buffer_append_cstr(bp);
    buffer_append_literal("m");
}

// Outputs both background and foreground colors.
static void output_colors(
        int x, int y,
        int fg_red, int fg_green, int fg_blue,
        int bg_red, int bg_green, int bg_blue)
{
    // TODO this applies noise per character which is not what we should be
    // doing. We would get much better noise quality if we applied it per
    // pixel.
    if (noise_enabled) {
        uint32_t noise_color = NOISE_SAMPLE(x, y);

        fg_red += ((noise_color >> 16) & 0xff) - 128;
        bg_red += ((noise_color >> 16) & 0xff) - 128;
        fg_green += ((noise_color >> 8) & 0xff) - 128;
        bg_green += ((noise_color >> 8) & 0xff) - 128;
        fg_blue += ((noise_color) & 0xff) - 128;
        bg_blue += ((noise_color) & 0xff) - 128;

        /*
        fg_red = (noise_color & 0xff);
        bg_red = (noise_color & 0xff);
        fg_green = ((noise_color >> 8) & 0xff);
        bg_green = ((noise_color >> 8) & 0xff);
        fg_blue = ((noise_color >> 16) & 0xff);
        bg_blue = ((noise_color >> 16) & 0xff);
        */
    }

    char fp[24], bp[24];
    long fk = rt_color_params(fp, fg_red, fg_green, fg_blue, false);
    long bk = rt_color_params(bp, bg_red, bg_green, bg_blue, true);
    bool need_fg = (fk != rt_last_fg);
    bool need_bg = (bk != rt_last_bg);
    if (!need_fg && !need_bg)
        return;                       // both unchanged -> emit nothing
    rt_last_fg = fk;
    rt_last_bg = bk;

    // One CSI carrying only the component(s) that changed: "\033[<fg>;<bg>m",
    // "\033[<fg>m", or "\033[<bg>m".
    buffer_append_literal("\033[");
    if (need_fg)
        buffer_append_cstr(fp);
    if (need_fg && need_bg)
        buffer_append_literal(";");
    if (need_bg)
        buffer_append_cstr(bp);
    buffer_append_literal("m");
}

static void output_newline(void) {
    // CR+LF, not bare LF: over a BBS the door's output isn't LF->CRLF translated,
    // so we must return to column 1 ourselves (with auto-wrap off, a bare LF would
    // leave the cursor at the right margin and all next-row cells clamp there).
    buffer_append_literal("\033[0m\r\n");
    rt_reset_color_cache();           // \033[0m returned the terminal to default
}



/*
 * Rendering
 */

static void start_row() {
    DOOMCLI_READ_INPUT();
    if (cli_colors == cli_colors_3bit) {
        // send bold, hopefully the terminal interprets it as bright
        buffer_append_literal("\033[1m");
    }
}

static void draw_space() {
    uint32_t* dest_pixel = dest_buffer;
    for (int y = 0; y < dest_height; ++y) {
        start_row();
        for (int x = 0; x < dest_width; ++x) {
            uint8_t* pixel = (uint8_t*)dest_pixel;
            output_bg_color(x, y, pixel[2], pixel[1], pixel[0]);
            //printf("\033[48;5;%um ", 16 + (pixel[0] / 43) + (pixel[1] / 43) * 6 + (pixel[2] / 43) * 36);
            buffer_append_literal(" ");
            ++dest_pixel;
        }
        output_newline();
    }
}

static void draw_half() {
    uint32_t* top = dest_buffer;
    uint32_t* bot = dest_buffer + dest_width;

    for (int y = 0; y < dest_height; y += 2) {
        start_row();
        for (int x = 0; x < dest_width; ++x) {
            //printf("\033[48;2;%u;%u;%um ", pixel[2], pixel[1], pixel[0]);
            //printf("\033[48;5;%um ", 16 + (pixel[0] / 43) + (pixel[1] / 43) * 6 + (pixel[2] / 43) * 36);

            output_colors(x, y,
                    ((uint8_t*)top)[2], ((uint8_t*)top)[1], ((uint8_t*)top)[0],
                    ((uint8_t*)bot)[2], ((uint8_t*)bot)[1], ((uint8_t*)bot)[0]);
            if (cli_mode == cli_mode_half) {
                buffer_append(g_block_upper, g_block_upper_len);
            }

            ++top;
            ++bot;
        }

        output_newline();
        top += dest_width;
        bot += dest_width;
    }
}

// ======================================================================
// syncdoom integration: config + per-frame entry (DG_ScreenBuffer source)
// ======================================================================

void rt_config(int cols, int rows, rt_mode_t mode, rt_colors_t colors, rt_charset_t charset)
{
    columns = cols;
    // pixels-per-cell sets the scaled buffer dimensions per mode
    switch (mode) {
        case RT_SPACE:    cli_mode = cli_mode_space;    dest_width = cols;     dest_height = rows;     break;
        case RT_QUADRANT: cli_mode = cli_mode_quadrant; dest_width = cols * 2; dest_height = rows * 2; break;
        case RT_SEXTANT:  cli_mode = cli_mode_sextant;  dest_width = cols * 2; dest_height = rows * 3; break;
        case RT_BLOCKS:   cli_mode = cli_mode_blocks;   dest_width = cols * 2; dest_height = rows * 2; break;
        default:          cli_mode = cli_mode_half;     dest_width = cols;     dest_height = rows * 2; break;
    }
    switch (colors) {
        case RT_24BIT: cli_colors = cli_colors_24bit; break;
        case RT_8BIT:  cli_colors = cli_colors_8bit;  break;
        case RT_3BIT:  cli_colors = cli_colors_3bit;  break;
        default:       cli_colors = cli_colors_4bit;  break;
    }
    // CP437 has only the half-block glyph (0xDF); quadrant/sextant are UTF-8-only.
    g_charset = charset;
    if (charset == RT_CP437) {
        g_block_upper = "\xDF"; g_block_upper_len = 1;
        g_blocks = blocks_cp437;
    } else {
        g_block_upper = UPPER_HALF; g_block_upper_len = sizeof(UPPER_HALF) - 1;
        g_blocks = blocks_utf8;
    }

    free(dest_buffer);
    dest_buffer = malloc(sizeof(uint32_t) * (size_t)dest_width * dest_height);

    if (buffer == NULL) { buffer_capacity = 1u << 16; buffer = malloc(buffer_capacity); }

    noise_enabled = true;   // init_noise may disable it (e.g. 24-bit)
    init_noise();
}

const char *rt_render_frame(size_t *len)
{
    int x, y;
    uint32_t *dp = dest_buffer;

    if (noise_enabled) {
        uint32_t t = DG_GetTicksMs();
        if (t - noise_last_time > (uint32_t)noise_speed) {
            noise_last_time = t;
            noise_current = (noise_current + 1) % noise_texture_count;
        }
    }
    for (y = 0; y < dest_height; ++y)
        for (x = 0; x < dest_width; ++x) {
            int sy = y * DOOMGENERIC_RESY / dest_height;
            int sx = x * DOOMGENERIC_RESX / dest_width;
            *dp++ = DG_ScreenBuffer[sy * DOOMGENERIC_RESX + sx];
        }

    buffer_count = 0;
    buffer_append_literal("\033[H");   // home; every cell is overwritten each frame
    rt_reset_color_cache();            // prior frame ended on \033[0m -> default colors
    switch (cli_mode) {
        case cli_mode_space:    draw_space();    break;
        case cli_mode_quadrant: draw_quadrant(); break;
        case cli_mode_blocks:   draw_blocks();   break;
        case cli_mode_sextant:  draw_sextant();  break;
        default:                draw_half();     break;
    }

    // Drop the trailing CR+LF: a newline after the LAST row pushes the cursor past
    // the bottom margin and scrolls the screen, making the image jump every frame.
    if (buffer_count >= 2 && buffer[buffer_count-1] == '\n' && buffer[buffer_count-2] == '\r')
        buffer_count -= 2;

    *len = buffer_count;
    return buffer;
}

// ===== quadrant/sextant tiers, lifted from doom-cli (GPL-2.0) =====
const char* quadrants[] = {
    " ",             // U+0020: SPACE
    "\xE2\x96\x98",  // U+2598: QUADRANT UPPER LEFT                                  ▘
    "\xE2\x96\x9D",  // U+259D: QUADRANT UPPER RIGHT                                 ▝
    "\xE2\x96\x80",  // U+2580: UPPER HALF BLOCK                                     ▀
    "\xE2\x96\x96",  // U+2596: QUADRANT LOWER LEFT                                  ▖
    "\xE2\x96\x8C",  // U+258C: LEFT HALF BLOCK                                      ▌
    "\xE2\x96\x9E",  // U+259E: QUADRANT UPPER RIGHT AND LOWER LEFT                  ▞
    "\xE2\x96\x9B",  // U+259B: QUADRANT UPPER LEFT AND UPPER RIGHT AND LOWER LEFT   ▛
    "\xE2\x96\x97",  // U+2597: QUADRANT LOWER RIGHT                                 ▗
    "\xE2\x96\x9A",  // U+259A: QUADRANT UPPER LEFT AND LOWER RIGHT                  ▚
    "\xE2\x96\x90",  // U+2590: RIGHT HALF BLOCK                                     ▐
    "\xE2\x96\x9C",  // U+259C: QUADRANT UPPER LEFT AND UPPER RIGHT AND LOWER RIGHT  ▜
    "\xE2\x96\x84",  // U+2584: LOWER HALF BLOCK                                     ▄
    "\xE2\x96\x99",  // U+2599: QUADRANT UPPER LEFT AND LOWER LEFT AND LOWER RIGHT   ▙
    "\xE2\x96\x9F",  // U+259F: QUADRANT UPPER RIGHT AND LOWER LEFT AND LOWER RIGHT  ▟
    "\xE2\x96\x88",  // U+2588: FULL BLOCK                                           █
};
const char* sextants[] = {
    " ",                 // U+0020:  SPACE
    "\xF0\x9F\xAC\x80",  // U+1FB00: BLOCK SEXTANT-1      🬀
    "\xF0\x9F\xAC\x81",  // U+1FB01: BLOCK SEXTANT-2      🬁
    "\xF0\x9F\xAC\x82",  // U+1FB02: BLOCK SEXTANT-12     🬂
    "\xF0\x9F\xAC\x83",  // U+1FB03: BLOCK SEXTANT-3      🬃
    "\xF0\x9F\xAC\x84",  // U+1FB04: BLOCK SEXTANT-13     🬄
    "\xF0\x9F\xAC\x85",  // U+1FB05: BLOCK SEXTANT-23     🬅
    "\xF0\x9F\xAC\x86",  // U+1FB06: BLOCK SEXTANT-123    🬆
    "\xF0\x9F\xAC\x87",  // U+1FB07: BLOCK SEXTANT-4      🬇
    "\xF0\x9F\xAC\x88",  // U+1FB08: BLOCK SEXTANT-14     🬈
    "\xF0\x9F\xAC\x89",  // U+1FB09: BLOCK SEXTANT-24     🬉
    "\xF0\x9F\xAC\x8A",  // U+1FB0A: BLOCK SEXTANT-124    🬊
    "\xF0\x9F\xAC\x8B",  // U+1FB0B: BLOCK SEXTANT-34     🬋
    "\xF0\x9F\xAC\x8C",  // U+1FB0C: BLOCK SEXTANT-134    🬌
    "\xF0\x9F\xAC\x8D",  // U+1FB0D: BLOCK SEXTANT-234    🬍
    "\xF0\x9F\xAC\x8E",  // U+1FB0E: BLOCK SEXTANT-1234   🬎
    "\xF0\x9F\xAC\x8F",  // U+1FB0F: BLOCK SEXTANT-5      🬏
    "\xF0\x9F\xAC\x90",  // U+1FB10: BLOCK SEXTANT-15     🬐
    "\xF0\x9F\xAC\x91",  // U+1FB11: BLOCK SEXTANT-25     🬑
    "\xF0\x9F\xAC\x92",  // U+1FB12: BLOCK SEXTANT-125    🬒
    "\xF0\x9F\xAC\x93",  // U+1FB13: BLOCK SEXTANT-35     🬓
    "\xE2\x96\x8C",      // U+258C:  LEFT HALF BLOCK      ▌
    "\xF0\x9F\xAC\x94",  // U+1FB14: BLOCK SEXTANT-235    🬔
    "\xF0\x9F\xAC\x95",  // U+1FB15: BLOCK SEXTANT-1235   🬕
    "\xF0\x9F\xAC\x96",  // U+1FB16: BLOCK SEXTANT-45     🬖
    "\xF0\x9F\xAC\x97",  // U+1FB17: BLOCK SEXTANT-145    🬗
    "\xF0\x9F\xAC\x98",  // U+1FB18: BLOCK SEXTANT-245    🬘
    "\xF0\x9F\xAC\x99",  // U+1FB19: BLOCK SEXTANT-1245   🬙
    "\xF0\x9F\xAC\x9A",  // U+1FB1A: BLOCK SEXTANT-345    🬚
    "\xF0\x9F\xAC\x9B",  // U+1FB1B: BLOCK SEXTANT-1345   🬛
    "\xF0\x9F\xAC\x9C",  // U+1FB1C: BLOCK SEXTANT-2345   🬜
    "\xF0\x9F\xAC\x9D",  // U+1FB1D: BLOCK SEXTANT-12345  🬝
    "\xF0\x9F\xAC\x9E",  // U+1FB1E: BLOCK SEXTANT-6      🬞
    "\xF0\x9F\xAC\x9F",  // U+1FB1F: BLOCK SEXTANT-16     🬟
    "\xF0\x9F\xAC\xA0",  // U+1FB20: BLOCK SEXTANT-26     🬠
    "\xF0\x9F\xAC\xA1",  // U+1FB21: BLOCK SEXTANT-126    🬡
    "\xF0\x9F\xAC\xA2",  // U+1FB22: BLOCK SEXTANT-36     🬢
    "\xF0\x9F\xAC\xA3",  // U+1FB23: BLOCK SEXTANT-136    🬣
    "\xF0\x9F\xAC\xA4",  // U+1FB24: BLOCK SEXTANT-236    🬤
    "\xF0\x9F\xAC\xA5",  // U+1FB25: BLOCK SEXTANT-1236   🬥
    "\xF0\x9F\xAC\xA6",  // U+1FB26: BLOCK SEXTANT-46     🬦
    "\xF0\x9F\xAC\xA7",  // U+1FB27: BLOCK SEXTANT-146    🬧
    "\xE2\x96\x90",      // U+2590:  RIGHT HALF BLOCK     ▐
    "\xF0\x9F\xAC\xA8",  // U+1FB28: BLOCK SEXTANT-1246   🬨
    "\xF0\x9F\xAC\xA9",  // U+1FB29: BLOCK SEXTANT-346    🬩
    "\xF0\x9F\xAC\xAA",  // U+1FB2A: BLOCK SEXTANT-1346   🬪
    "\xF0\x9F\xAC\xAB",  // U+1FB2B: BLOCK SEXTANT-2346   🬫
    "\xF0\x9F\xAC\xAC",  // U+1FB2C: BLOCK SEXTANT-12346  🬬
    "\xF0\x9F\xAC\xAD",  // U+1FB2D: BLOCK SEXTANT-56     🬭
    "\xF0\x9F\xAC\xAE",  // U+1FB2E: BLOCK SEXTANT-156    🬮
    "\xF0\x9F\xAC\xAF",  // U+1FB2F: BLOCK SEXTANT-256    🬯
    "\xF0\x9F\xAC\xB0",  // U+1FB30: BLOCK SEXTANT-1256   🬰
    "\xF0\x9F\xAC\xB1",  // U+1FB31: BLOCK SEXTANT-356    🬱
    "\xF0\x9F\xAC\xB2",  // U+1FB32: BLOCK SEXTANT-1356   🬲
    "\xF0\x9F\xAC\xB3",  // U+1FB33: BLOCK SEXTANT-2356   🬳
    "\xF0\x9F\xAC\xB4",  // U+1FB34: BLOCK SEXTANT-12356  🬴
    "\xF0\x9F\xAC\xB5",  // U+1FB35: BLOCK SEXTANT-456    🬵
    "\xF0\x9F\xAC\xB6",  // U+1FB36: BLOCK SEXTANT-1456   🬶
    "\xF0\x9F\xAC\xB7",  // U+1FB37: BLOCK SEXTANT-2456   🬷
    "\xF0\x9F\xAC\xB8",  // U+1FB38: BLOCK SEXTANT-12456  🬸
    "\xF0\x9F\xAC\xB9",  // U+1FB39: BLOCK SEXTANT-3456   🬹
    "\xF0\x9F\xAC\xBA",  // U+1FB3A: BLOCK SEXTANT-13456  🬺
    "\xF0\x9F\xAC\xBB",  // U+1FB3B: BLOCK SEXTANT-23456  🬻
    "\xE2\x96\x88",      // U+2588:  FULL BLOCK           █
};
// 2x2 sub-cell renderer shared by the quadrant and blocks tiers: the only
// difference is the 16-entry glyph table (UTF-8 quadrants vs CP437-safe blocks).
static void draw_subcell(const char *const *glyphs) {
    uint32_t* top = dest_buffer;
    uint32_t* bot = dest_buffer + dest_width;

    for (int y = 0; y < dest_height; y += 2) {
        start_row();
        for (int x = 0; x < dest_width; x += 2) {

            // get pixels
            uint8_t* tl = (uint8_t*)&top[0];
            uint8_t* tr = (uint8_t*)&top[1];
            uint8_t* bl = (uint8_t*)&bot[0];
            uint8_t* br = (uint8_t*)&bot[1];
            /*
            printf("%u %u %u   ", tl[0], tl[1], tl[2]);
            printf("%u %u %u\n", tr[0], tr[1], tr[2]);
            printf("%u %u %u   ", bl[0], bl[1], bl[2]);
            printf("%u %u %u\n", br[0], br[1], br[2]);
            */

            // calculate average luma
            uint32_t l_tl = LUMA_P(tl);
            uint32_t l_tr = LUMA_P(tr);
            uint32_t l_bl = LUMA_P(bl);
            uint32_t l_br = LUMA_P(br);
            //printf("luma %u %u %u %u\n", l_tl, l_tr, l_bl, l_br);
            uint32_t l_avg = (l_tl + l_tr + l_bl + l_br) >> 2;
            //printf("luma avg %u\n", l_avg);
            //l_avg=127*256;

            // foreground is bright, background is dark
            int index =
                 (l_tl > l_avg) |
                ((l_tr > l_avg) << 1) |
                ((l_bl > l_avg) << 2) |
                ((l_br > l_avg) << 3);

            // if all bits are set, we use only background (space)
            // TODO fix this later, need to also clear luma or check bits in below conditionals
            /*if (index == 0b1111) {
                index = 0;
            }*/

        //fputs("\033[0m", stdout);
            //printf("index %i char %s\n", index, quadrants[index]);

            /*
            bool on_tl = l_tl > l_avg;
            bool on_tr = l_tr > l_avg;
            bool on_bl = l_bl > l_avg;
            bool on_br = l_br > l_avg;
            */

            // choose foreground and background colors
            uint32_t fg_red = 0;
            uint32_t fg_green = 0;
            uint32_t fg_blue = 0;
            int fg_count = 0;
            uint32_t bg_red = 0;
            uint32_t bg_green = 0;
            uint32_t bg_blue = 0;
            int bg_count = 0;

            /*
            uint8_t blue  = (uint8_t)((tl[0] + tr[0] + bl[0] + br[0]) >> 2);
            uint8_t green = (uint8_t)((tl[1] + tr[1] + bl[1] + br[1]) >> 2);
            uint8_t red   = (uint8_t)((tl[2] + tr[2] + bl[2] + br[2]) >> 2);
            */

            if (l_tl > l_avg) {
                fg_blue += tl[0];
                fg_green += tl[1];
                fg_red += tl[2];
                ++fg_count;
            } else {
                bg_blue += tl[0];
                bg_green += tl[1];
                bg_red += tl[2];
                ++bg_count;
            }

            if (l_tr > l_avg) {
                fg_blue += tr[0];
                fg_green += tr[1];
                fg_red += tr[2];
                ++fg_count;
            } else {
                bg_blue += tr[0];
                bg_green += tr[1];
                bg_red += tr[2];
                ++bg_count;
            }

            if (l_bl > l_avg) {
                fg_blue += bl[0];
                fg_green += bl[1];
                fg_red += bl[2];
                ++fg_count;
            } else {
                bg_blue += bl[0];
                bg_green += bl[1];
                bg_red += bl[2];
                ++bg_count;
            }

            if (l_br > l_avg) {
                fg_blue += br[0];
                fg_green += br[1];
                fg_red += br[2];
                ++fg_count;
            } else {
                bg_blue += br[0];
                bg_green += br[1];
                bg_red += br[2];
                ++bg_count;
            }

            if (fg_count > 0) {
                fg_blue /= fg_count;
                fg_green /= fg_count;
                fg_red /= fg_count;
            }

            if (bg_count > 0) {
                bg_blue /= bg_count;
                bg_green /= bg_count;
                bg_red /= bg_count;
            }




            //printf("\033[48;2;%u;%u;%um ", pixel[2], pixel[1], pixel[0]);
            //printf("\033[48;5;%um ", 16 + (pixel[0] / 43) + (pixel[1] / 43) * 6 + (pixel[2] / 43) * 36);

            /*
            printf("\033[38;2;%u;%u;%um\033[48;2;%u;%u;%um" UPPER_HALF,
                    ((uint8_t*)top)[2], ((uint8_t*)top)[1], ((uint8_t*)top)[0],
                    ((uint8_t*)bot)[2], ((uint8_t*)bot)[1], ((uint8_t*)bot)[0]);
                    */

            if (index == 0) {
                output_bg_color(x, y, bg_red, bg_green, bg_blue);
                buffer_append_literal(" ");
            } else {
                output_colors(x, y,
                        fg_red, fg_green, fg_blue,
                        bg_red, bg_green, bg_blue);
                buffer_append_cstr(glyphs[index]);
            }

            top += 2;
            bot += 2;
        }

        output_newline();
        top += dest_width;
        bot += dest_width;
    }
}

// The quadrant (UTF-8 hi-res) and blocks (CP437-safe) tiers are the same 2x2
// renderer with different glyph tables.
static void draw_quadrant() { draw_subcell(quadrants); }
static void draw_blocks()   { draw_subcell(g_blocks); }

static void draw_sextant_bw() {
//printf("%s %i  draw sextant\n",__func__, DG_GetTicksMs());
    uint32_t* top = dest_buffer;
    uint32_t* mid = top + dest_width;
    uint32_t* bot = mid + dest_width;

    for (int y = 0; y < dest_height; y += 3) {
        start_row();
        for (int x = 0; x < dest_width; x += 2) {

            // get pixels
            uint8_t* tl = (uint8_t*)&top[0];
            uint8_t* tr = (uint8_t*)&top[1];
            uint8_t* ml = (uint8_t*)&mid[0];
            uint8_t* mr = (uint8_t*)&mid[1];
            uint8_t* bl = (uint8_t*)&bot[0];
            uint8_t* br = (uint8_t*)&bot[1];

            // calculate luma
            uint32_t l_tl = LUMA_P(tl) >> 8;
            uint32_t l_tr = LUMA_P(tr) >> 8;
            uint32_t l_ml = LUMA_P(ml) >> 8;
            uint32_t l_mr = LUMA_P(mr) >> 8;
            uint32_t l_bl = LUMA_P(bl) >> 8;
            uint32_t l_br = LUMA_P(br) >> 8;

            if (noise_enabled) {
                // We use only the red channel of noise.
                //printf("pixel x %i y %i luma %i noise raw %i noise actual %i\n", x,y,l_tl,NOISE_SAMPLE(x    , y    )&0xff,(NOISE_SAMPLE(x    , y    ) & 0xff) - 128);
                l_tl = clamp(l_tl + (NOISE_SAMPLE(x    , y    ) & 0xff) - 128, 0, 255);
                l_tr = clamp(l_tr + (NOISE_SAMPLE(x + 1, y    ) & 0xff) - 128, 0, 255);
                l_ml = clamp(l_ml + (NOISE_SAMPLE(x    , y + 1) & 0xff) - 128, 0, 255);
                l_mr = clamp(l_mr + (NOISE_SAMPLE(x + 1, y + 1) & 0xff) - 128, 0, 255);
                l_bl = clamp(l_bl + (NOISE_SAMPLE(x    , y + 2) & 0xff) - 128, 0, 255);
                l_br = clamp(l_br + (NOISE_SAMPLE(x + 1, y + 2) & 0xff) - 128, 0, 255);
            }

            // calculate index
            //printf("%i %i %i %i %i %i\n", l_tl,l_tr,l_ml,l_mr,l_bl,l_br);
            const uint32_t threshold = 127;
            int index =
                ((l_tl > threshold)     ) |
                ((l_tr > threshold) << 1) |
                ((l_ml > threshold) << 2) |
                ((l_mr > threshold) << 3) |
                ((l_bl > threshold) << 4) |
                ((l_br > threshold) << 5);

            if (cli_colors == cli_colors_light) {
                index = (~index) & 0x3f;
            }

            buffer_append_cstr(sextants[index]);

            top += 2;
            mid += 2;
            bot += 2;
        }

        output_newline();
        top += dest_width << 1;
        mid += dest_width << 1;
        bot += dest_width << 1;
    }
}

static void draw_sextant() {
//printf("%s %i  draw sextant\n",__func__, DG_GetTicksMs());
    uint32_t* top = dest_buffer;
    uint32_t* mid = top + dest_width;
    uint32_t* bot = mid + dest_width;

    for (int y = 0; y < dest_height; y += 3) {
        start_row();
        for (int x = 0; x < dest_width; x += 2) {

            // get pixels
            uint8_t* tl = (uint8_t*)&top[0];
            uint8_t* tr = (uint8_t*)&top[1];
            uint8_t* ml = (uint8_t*)&mid[0];
            uint8_t* mr = (uint8_t*)&mid[1];
            uint8_t* bl = (uint8_t*)&bot[0];
            uint8_t* br = (uint8_t*)&bot[1];
            /*
            printf("%u %u %u   ", tl[0], tl[1], tl[2]);
            printf("%u %u %u\n", tr[0], tr[1], tr[2]);
            printf("%u %u %u   ", bl[0], bl[1], bl[2]);
            printf("%u %u %u\n", br[0], br[1], br[2]);
            */

            // calculate average luma
            uint32_t l_tl = LUMA_P(tl);
            uint32_t l_tr = LUMA_P(tr);
            uint32_t l_ml = LUMA_P(ml);
            uint32_t l_mr = LUMA_P(mr);
            uint32_t l_bl = LUMA_P(bl);
            uint32_t l_br = LUMA_P(br);
            //printf("luma %u %u %u %u\n", l_tl, l_tr, l_bl, l_br);
            uint32_t l_avg = (l_tl + l_tr + l_ml + l_mr + l_bl + l_br) / 6;
            //printf("luma avg %u\n", l_avg);
            //l_avg=127*256;

            // foreground is bright, background is dark
            int index = 0;
            /*
            int index =
                 (l_tl > l_avg) |
                ((l_tr > l_avg) << 1) |
                ((l_ml > l_avg) << 2) |
                ((l_mr > l_avg) << 3) |
                ((l_bl > l_avg) << 4) |
                ((l_br > l_avg) << 5);
                */

            // if all bits are set, we use only background (space)
            // TODO fix this later, need to also clear luma or check bits in below conditionals
            /*if (index == 0b111111) {
                index = 0;
            }*/

        //fputs("\033[0m", stdout);
            //printf("index %i char %s\n", index, quadrants[index]);

            /*
            bool on_tl = l_tl > l_avg;
            bool on_tr = l_tr > l_avg;
            bool on_bl = l_bl > l_avg;
            bool on_br = l_br > l_avg;
            */

            // choose foreground and background colors
            uint32_t fg_red = 0;
            uint32_t fg_green = 0;
            uint32_t fg_blue = 0;
            int fg_count = 0;
            uint32_t bg_red = 0;
            uint32_t bg_green = 0;
            uint32_t bg_blue = 0;
            int bg_count = 0;

            /*
            uint8_t blue  = (uint8_t)((tl[0] + tr[0] + bl[0] + br[0]) >> 2);
            uint8_t green = (uint8_t)((tl[1] + tr[1] + bl[1] + br[1]) >> 2);
            uint8_t red   = (uint8_t)((tl[2] + tr[2] + bl[2] + br[2]) >> 2);
            */

            if (l_tl > l_avg) {
                fg_blue += tl[0];
                fg_green += tl[1];
                fg_red += tl[2];
                ++fg_count;
                index |= 1;
            } else {
                bg_blue += tl[0];
                bg_green += tl[1];
                bg_red += tl[2];
                ++bg_count;
            }

            if (l_tr > l_avg) {
                fg_blue += tr[0];
                fg_green += tr[1];
                fg_red += tr[2];
                ++fg_count;
                index |= 2;
            } else {
                bg_blue += tr[0];
                bg_green += tr[1];
                bg_red += tr[2];
                ++bg_count;
            }

            if (l_ml > l_avg) {
                fg_blue += ml[0];
                fg_green += ml[1];
                fg_red += ml[2];
                ++fg_count;
                index |= 4;
            } else {
                bg_blue += ml[0];
                bg_green += ml[1];
                bg_red += ml[2];
                ++bg_count;
            }

            if (l_mr > l_avg) {
                fg_blue += mr[0];
                fg_green += mr[1];
                fg_red += mr[2];
                ++fg_count;
                index |= 8;
            } else {
                bg_blue += mr[0];
                bg_green += mr[1];
                bg_red += mr[2];
                ++bg_count;
            }

            if (l_bl > l_avg) {
                fg_blue += bl[0];
                fg_green += bl[1];
                fg_red += bl[2];
                ++fg_count;
                index |= 16;
            } else {
                bg_blue += bl[0];
                bg_green += bl[1];
                bg_red += bl[2];
                ++bg_count;
            }

            if (l_br > l_avg) {
                fg_blue += br[0];
                fg_green += br[1];
                fg_red += br[2];
                ++fg_count;
                index |= 32;
            } else {
                bg_blue += br[0];
                bg_green += br[1];
                bg_red += br[2];
                ++bg_count;
            }

            if (fg_count > 0) {
                fg_blue /= fg_count;
                fg_green /= fg_count;
                fg_red /= fg_count;
            }

            if (bg_count > 0) {
                bg_blue /= bg_count;
                bg_green /= bg_count;
                bg_red /= bg_count;
            }




            //printf("\033[48;2;%u;%u;%um ", pixel[2], pixel[1], pixel[0]);
            //printf("\033[48;5;%um ", 16 + (pixel[0] / 43) + (pixel[1] / 43) * 6 + (pixel[2] / 43) * 36);

            /*
            printf("\033[38;2;%u;%u;%um\033[48;2;%u;%u;%um" UPPER_HALF,
                    ((uint8_t*)top)[2], ((uint8_t*)top)[1], ((uint8_t*)top)[0],
                    ((uint8_t*)bot)[2], ((uint8_t*)bot)[1], ((uint8_t*)bot)[0]);
                    */

//if (x < 10){
            if (index == 0) {
                output_bg_color(x, y, bg_red, bg_green, bg_blue);
                buffer_append_literal(" ");
            } else {
                //buffer_append_format("\033[0m%u",index);
                output_colors(x, y,
                        fg_red, fg_green, fg_blue,
                        bg_red, bg_green, bg_blue);
                buffer_append_cstr(sextants[index]);
            }
//}

            top += 2;
            mid += 2;
            bot += 2;
        }

        output_newline();
        top += dest_width << 1;
        mid += dest_width << 1;
        bot += dest_width << 1;
    }
//printf("%s %i  done\n",__func__, DG_GetTicksMs());
}
