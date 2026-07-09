/* hw_sbbs.c -- 1oom "hw" backend for the syncmoo1 Synchronet door.
 *
 * Task 2 (build-plumbing) stub: a legitimate 1oom hw backend (peer of
 * hw/nop, hw/sdl, hw/alleg) that lets main_1oom() run headlessly so the
 * CMake source-enumeration + link can be proven before any real terminal
 * I/O exists. The simple bodies are cloned from 1oom/src/hw/nop/hwnop.c;
 * the video buffer plumbing below is modeled on hw/sdl/hwsdl_video.c
 * (4 pages, bufi front/back flip) because main_1oom() can hit
 * ui_early_show_message_box() (main.c:78-83, the LBX-not-found path)
 * *before* the real game loop starts, and that renders into whatever
 * hw_video_get_buf() returns -- so it must be a real buffer, not NULL.
 *
 * Task 5: hw_video_draw_buf() now presents the just-drawn back buffer to a
 * real terminal via syncmoo1_io.c (sm_io_present()) before flipping pages,
 * and hw_video_set_palette() feeds it a running RGB888 copy of the palette.
 *
 * Never edit the vendored 1oom tree; this file is ours.
 */
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "hw.h"
#include "cfg.h"
#include "main.h"
#include "options.h"
#include "palette.h"
#include "types.h"

#include "syncmoo1.h"

/* -------------------------------------------------------------------------- */

const struct cmdline_options_s hw_cmdline_options[] = {
    { NULL, 0, NULL, NULL, NULL, NULL }
};

const struct cmdline_options_s hw_cmdline_options_extra[] = {
    { NULL, 0, NULL, NULL, NULL, NULL }
};

const struct cfg_items_s hw_cfg_items[] = {
    CFG_ITEM_END
};

const struct cfg_items_s hw_cfg_items_extra[] = {
    CFG_ITEM_END
};

/* -------------------------------------------------------------------------- */

const char *idstr_hw = "sbbs";

int main(int argc, char **argv)
{
    return main_1oom(argc, argv);
}

int hw_early_init(void)
{
    return 0;
}

int hw_init(void)
{
    return 0;
}

void hw_shutdown(void)
{
}

void hw_log_message(const char *msg)
{
    fputs(msg, stdout);
}

void hw_log_warning(const char *msg)
{
    fputs(msg, stderr);
}

void hw_log_error(const char *msg)
{
    fputs(msg, stderr);
}

int64_t hw_get_time_us(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (int64_t)tv.tv_usec + 1000000ll * (int64_t)tv.tv_sec;
}

/* -------------------------------------------------------------------------- */
/* Video: real page buffers (see file header) -- no terminal presentation. */

static uint8_t *video_buf[4] = { NULL, NULL, NULL, NULL };
static int video_bufi = 0;
static int video_bufw = 0;
static int video_bufh = 0;

/* Running RGB888 copy of the game's palette, fed to sm_io_present() on every
 * flip. ui_palette (palette.h) holds the engine's own 6-bit-per-channel VGA
 * values (its comment says so explicitly, and every real hw backend --
 * hw/sdl/2's video_setpal, hw/sdl/1's video_setpal_8bpp/gl_24bpp/gl_32bpp,
 * hw/alleg4's video_setpal_8bpp -- widens each component through
 * palette_6bit_to_8bit() before handing it to its actual display; termgfx's
 * sixel encoder likewise expects a full 0-255 RGB triple per sixel.h, so the
 * same widening happens here rather than passing the raw 6-bit values
 * through (which would render a washed-out, ~1/4-range palette). */
static uint8_t g_palette[768];

int hw_video_init(int w, int h)
{
    int i;
    size_t sz = (size_t)w * (size_t)h;

    for (i = 0; i < 4; ++i) {
        free(video_buf[i]);
        video_buf[i] = (uint8_t *)calloc(1, sz ? sz : 1);
        if (video_buf[i] == NULL) {
            return -1;
        }
    }
    video_bufw = w;
    video_bufh = h;
    video_bufi = 0;
    return 0;
}

void hw_video_set_palette(const uint8_t *palette, int first, int num)
{
    int i;

    ui_palette_set(palette, first, num);   /* engine-side copy: stays 6-bit, as the engine expects */
    for (i = 0; i < num; ++i) {
        int j = (first + i) * 3;
        g_palette[j + 0] = palette_6bit_to_8bit(palette[i * 3 + 0]);
        g_palette[j + 1] = palette_6bit_to_8bit(palette[i * 3 + 1]);
        g_palette[j + 2] = palette_6bit_to_8bit(palette[i * 3 + 2]);
    }
}

void hw_video_set_palette_color(int i, uint8_t r, uint8_t g, uint8_t b)
{
    ui_palette_set_color(i, r, g, b);
}

void hw_video_refresh_palette(void)
{
}

uint8_t *hw_video_get_buf(void)
{
    return video_buf[video_bufi];
}

uint8_t *hw_video_get_buf_front(void)
{
    return video_buf[video_bufi ^ 1];
}

uint8_t *hw_video_draw_buf(void)
{
    /* Present the buffer the engine just finished drawing into (the CURRENT
     * back buffer, before the flip below) -- mirrors hw/sdl/hwsdl_video.c's
     * hw_video_refresh(0), which renders video.buf[video.bufi ^ 0] the same
     * way, before it swaps bufi. */
    sm_io_present(video_buf[video_bufi], g_palette);
    video_bufi ^= 1;
    return video_buf[video_bufi];
}

void hw_video_redraw_front(void)
{
}

void hw_video_copy_buf(void)
{
    memcpy(video_buf[video_bufi], video_buf[video_bufi ^ 1], (size_t)video_bufw * (size_t)video_bufh);
}

void hw_video_copy_buf_out(uint8_t *buf)
{
    memcpy(buf, video_buf[video_bufi], (size_t)video_bufw * (size_t)video_bufh);
}

void hw_video_copy_back_to_page2(void)
{
    memcpy(video_buf[2], video_buf[video_bufi], (size_t)video_bufw * (size_t)video_bufh);
}

void hw_video_copy_back_from_page2(void)
{
    memcpy(video_buf[video_bufi], video_buf[2], (size_t)video_bufw * (size_t)video_bufh);
}

void hw_video_copy_back_to_page3(void)
{
    memcpy(video_buf[3], video_buf[video_bufi], (size_t)video_bufw * (size_t)video_bufh);
}

void hw_video_copy_back_from_page3(void)
{
    memcpy(video_buf[video_bufi], video_buf[3], (size_t)video_bufw * (size_t)video_bufh);
}

/* -------------------------------------------------------------------------- */
/* Everything else: benign stubs (no terminal/input/audio backend yet). */

void hw_opt_menu_make_page_video(void)
{
}

int hw_event_handle(void)
{
    return 0;
}

void hw_textinput_start(void)
{
}

void hw_textinput_stop(void)
{
}

bool hw_kbd_set_repeat(bool enabled)
{
    return false;
}

void hw_mouse_set_xy(int mx, int my)
{
}

int hw_icon_set(const uint8_t *data, const uint8_t *pal, int w, int h)
{
    return 0;
}

int hw_audio_music_init(int mus_index, const uint8_t *data, uint32_t len)
{
    return 0;
}

void hw_audio_music_release(int mus_index)
{
}

void hw_audio_music_play(int mus_index)
{
}

void hw_audio_music_fadeout(void)
{
}

void hw_audio_music_stop(void)
{
}

bool hw_audio_music_volume(int volume)
{
    return true;
}

int hw_audio_sfx_batch_start(int sfx_index_max)
{
    return 0;
}

int hw_audio_sfx_batch_end(void)
{
    return 0;
}

int hw_audio_sfx_init(int sfx_index, const uint8_t *data, uint32_t len)
{
    return 0;
}

void hw_audio_sfx_release(int sfx_index)
{
}

void hw_audio_sfx_play(int sfx_index)
{
}

void hw_audio_sfx_stop(void)
{
}

bool hw_audio_sfx_volume(int volume)
{
    return true;
}
