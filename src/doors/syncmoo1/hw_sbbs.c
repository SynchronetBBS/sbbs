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
 * Task 7: main() now does real DOOR32.SYS/-s<fd> door setup (syncmoo1_door.c)
 * before handing off to main_1oom() -- see main() below for the call order
 * and rationale. hw_video_draw_buf() also calls sm_door_check_time() on
 * every present tick to enforce the DOOR32 session time limit.
 *
 * Task 8: main() now also calls sm_config_apply() (syncmoo1_config.c) after
 * the door setup/sanitize and before sm_io_init()/main_1oom() -- resolves the
 * shared MoO1 LBX data path and chdirs into the per-user -home sandbox, both
 * BEFORE main_1oom() ever reaches its LBX check (main.c's lbxfile_find_dir()).
 *
 * Never edit the vendored 1oom tree; this file is ours.
 */
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#include "hw.h"
#include "cfg.h"
#include "main.h"
#include "options.h"
#include "palette.h"
#include "types.h"

#include "syncmoo1.h"
#include "syncmoo1_audio.h"
#include "syncmoo1_config.h"
#include "syncmoo1_door.h"
#include "syncmoo1_input.h"

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

/* We own main() directly (this hw backend defines it, unlike syncduke's
 * engine which owns its own main() and needs a pre-main constructor to grab
 * argv -- DESIGN.md §14 open question #3, resolved: no constructor dance
 * needed here). Door setup happens inline, in order, before the engine ever
 * sees argv or runs its own (slow, LBX-scanning) init:
 *   1. sm_door_setup()        -- resolve DOOR32.SYS/-s<fd> socket + time
 *                                 limit + alias, configure the socket
 *                                 (non-blocking/TCP_NODELAY/SIGPIPE-ignore),
 *                                 paint the instant splash. A nonzero return
 *                                 (-help/--help/-?) means main() is done.
 *   2. sm_door_sanitize_argv() -- strip the door's own arguments so 1oom's
 *                                 options_parse() (main.c) never sees an
 *                                 unrecognized "-" flag or an absolute
 *                                 dropfile path and aborts main_1oom() (see
 *                                 syncmoo1_door.c's sanitize doc comment).
 *   3. sm_config_apply()       -- (Task 8, syncmoo1_config.c) resolve the
 *                                 shared LBX data path (SYNCMOO1_LBX, else
 *                                 this launch dir) and chdir into the per-user
 *                                 -home sandbox. Must run after sanitize_argv
 *                                 (which is what strips -home from argv;
 *                                 sm_door_home() below reads the value
 *                                 sm_door_resolve() captured earlier, not
 *                                 argv), and before the chdir can move cwd.
 *                                 The resolved data path is APPLIED later, by
 *                                 hw_init() -> sm_config_apply_data_path();
 *                                 see there and in syncmoo1_config.c for why.
 *   4. sm_io_init()            -- adopt the REAL socket fd (sm_door_socket())
 *                                 BEFORE the first present -- fixes the
 *                                 Task 5/6 carry-over where sm_io_get_fd()
 *                                 still returned the stdout fallback because
 *                                 nothing had resolved the door socket yet.
 *   5. main_1oom()             -- engine drives from here. */
int main(int argc, char **argv)
{
    if (sm_door_setup(argc, argv))
        return 1;
    sm_door_sanitize_argv(&argc, argv);
    sm_config_apply();
    sm_io_init(sm_door_socket());
    return main_1oom(argc, argv);
}

int hw_early_init(void)
{
    return 0;
}

/* main_1oom() -> main_init() -> here. This is deliberately where the shared
 * LBX data dir resolved by sm_config_apply() gets handed to 1oom: it is the
 * only hook that runs AFTER options_parse_early()'s cfg_load() (which would
 * otherwise overwrite the data path with the one remembered in the player's
 * 1oom config file) and BEFORE options_parse() applies 1oom's own -data
 * option, which is meant to win. See syncmoo1_config.c. */
int hw_init(void)
{
    sm_config_apply_data_path();
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

/* The buffer whose pixels are currently ON SCREEN (the one the last
 * hw_video_draw_buf() presented, before it flipped). hw_video_refresh_palette()
 * re-presents it when the engine changes the palette without redrawing --
 * see there. NULL until the first present. */
static uint8_t *video_shown = NULL;
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

/* Engine-side only, exactly as hw/sdl/hwsdl_video.c does it: this updates
 * ui_palette and nothing else. The push to the display is hw_video_refresh_palette()
 * below, which uipal.c's ui_palette_update() calls once after a batch of these. */
void hw_video_set_palette_color(int i, uint8_t r, uint8_t g, uint8_t b)
{
    ui_palette_set_color(i, r, g, b);
}

/* THE palette push (hw/sdl's "video.setpal(ui_palette, 0, 256)"). Rebuild our
 * 8-bit sixel copy from the engine's authoritative 6-bit ui_palette.
 *
 * This must not be a no-op: ui_palette_set_n()/ui_palette_fade_n()
 * (1oom/src/ui/classic/uipal.c) drive every fade and palette animation through
 * hw_video_set_palette_color() -- which only writes ui_palette -- and then call
 * ui_palette_update() -> here to make it visible. 1oom's very first present
 * happens while the palette is still all-black (it fades the picture up), so a
 * backend that ignores this hook shows the game drawing correctly into an
 * all-black palette: a blank screen, forever. */
void hw_video_refresh_palette(void)
{
    int i;

    for (i = 0; i < 256 * 3; ++i)
        g_palette[i] = palette_6bit_to_8bit(ui_palette[i]);

    /* ...and PUSH it, which is the whole point of this hook. hw/sdl's
     * video.setpal() hands the palette to the display right here, so a palette
     * change is visible without redrawing anything.
     *
     * 1oom leans on that hard: every fade is a palette-only animation.
     * fadein_do()/fadeout_do() (ui/classic/uipal.c) loop ui_palette_fade_n() +
     * a delay and never call hw_video_draw_buf(). The intro
     * (ui/classic/uiintro.c) draws its "Loading" screen ONCE, then fades it in
     * and out purely this way. A backend that only records the palette here and
     * waits for the next draw_buf() shows a BLACK SCREEN for the whole intro --
     * the pixels are there, painted through an all-black palette, and nothing
     * reaches the wire for several seconds.
     *
     * So re-present the buffer that is currently on screen with the new
     * palette. sm_io_present()'s de-dupe sees the palette changed and emits;
     * its DSR-ACK pacing drops frames if a fade outruns the link, so a 60-step
     * fade degrades to fewer, coarser steps rather than flooding. Nothing to do
     * before the first present (video_shown == NULL) -- lbxpal_init() and the
     * first fadeout land there, and the following draw_buf() carries the
     * palette anyway. */
    if (video_shown != NULL)
        sm_io_present(video_shown, g_palette);
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
    /* DOOR32 session time limit (Task 7, DESIGN.md §8): checked once per
     * present tick rather than per input poll -- exits (cleanly; see
     * sm_door_check_time()'s doc comment) the moment the clock the dropfile
     * gave us runs out. A no-op when no -t<seconds>/DOOR32.SYS time limit
     * was given. */
    sm_door_check_time();

    /* Present the buffer the engine just finished drawing into (the CURRENT
     * back buffer, before the flip below) -- mirrors hw/sdl/hwsdl_video.c's
     * hw_video_refresh(0), which renders video.buf[video.bufi ^ 0] the same
     * way, before it swaps bufi. */
    video_shown = video_buf[video_bufi];
    sm_io_present(video_shown, g_palette);
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
    /* sm_io_get_fd() is whatever sm_io_init() adopted -- the door socket
     * (dropfile/-s<fd> resolution, Task 7), or fd 1 (stdout) in dev/tty
     * fallback. A negative return means the peer hung up or a real socket
     * error occurred: go through the single canonical hangup path
     * (syncmoo1_door.c), which restores the BBS terminal (bounded drain +
     * mode-restore) before _exit() -- the read side may still have a live
     * write direction to restore to. */
    sm_audio_pump();   /* Stores the queued samples once the tier reply lands */
    if (sm_input_pump(sm_io_get_fd()) < 0)
        sm_door_hangup("input pump: peer hung up or socket error");
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
    return sm_audio_batch_start(sfx_index_max);
}

int hw_audio_sfx_batch_end(void)
{
    return 0;
}

int hw_audio_sfx_init(int sfx_index, const uint8_t *data, uint32_t len)
{
    return sm_audio_init(sfx_index, data, len);
}

void hw_audio_sfx_release(int sfx_index)
{
    sm_audio_release(sfx_index);
}

void hw_audio_sfx_play(int sfx_index)
{
    sm_audio_play(sfx_index);
}

void hw_audio_sfx_stop(void)
{
    sm_audio_stop();
}

bool hw_audio_sfx_volume(int volume)
{
    sm_audio_set_volume(volume);
    return true;
}
