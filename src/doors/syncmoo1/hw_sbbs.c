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
 * hw_video_draw_buf() below flips pages and returns the new back buffer
 * but presents nothing to a terminal yet -- that's syncmoo1_io.c, Task 5.
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
    ui_palette_set(palette, first, num);
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
    /* Present-nothing flip: Task 5 inserts syncmoo1_present() here. */
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
