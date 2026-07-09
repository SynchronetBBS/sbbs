// door_io.h -- internal seam between door_core.c (the PUBLIC door.h API that
// video_termgfx.cpp / wwkeyboard_termgfx.cpp call) and door_io.c (the socket
// setup, present/pacing/tier pipeline, and the minimal probe-reply input
// parser). Not part of the engine-facing seam -- only door_core.c includes
// this header.
#ifndef DOOR_IO_H_
#define DOOR_IO_H_

#include <stddef.h>
#include <stdint.h>

#include "keymode.h"   // termgfx: termgfx_keymode_t (the negotiated key mode)

#ifdef __cplusplus
extern "C" {
#endif

// Opaque handle for termgfx's SyncTERM audio-APC manager (termgfx/audio_mgr.h)
// -- forward-declared here (matching audio_mgr.h's own typedef) so this header
// stays a plain seam header: door_core.c/door_input.c, which already include
// it, don't need to also pull in audio_mgr.h just to see this pointer type.
typedef struct termgfx_audio termgfx_audio_t;

// Implementations of door_present()/door_pump() (door.h) -- door_core.c is a
// thin forwarder so the public API names/signatures never move even though
// the real work grew past the point of fitting in one file.
void door_io_present(const uint8_t *fb, const uint8_t *pal768);
void door_io_pump(void);

// door_input.c's seam into door_io.c (not part of the door.h engine-facing
// API either -- only door_input.c calls these):
//
// Pop one byte off the Task-2 ring door_input_byte() (door.h) fills; returns
// 0 (and leaves *b untouched) once it's empty.
int door_io_input_pop(uint8_t *b);

// Inject bytes into the staged output buffer -- door_input.c has no socket
// of its own (kitty keyboard-protocol push-flags reply, "\x1b[>11u").
void door_io_send(const void *buf, size_t len);

// True once the CTDA capability reply advertised cap 8 (SyncTERM physical
// key/evdev reports) and door_io.c enabled them.
int door_io_evdev_active(void);

// The negotiated terminal key mode (../../termgfx/keymode.h). Owned by door_io.c
// -- it sees the CTDA reply that enables evdev and owns the leave path that
// undoes it -- and shared with door_input.c, which drives the kitty side and
// checks the enable-time settle window.
termgfx_keymode_t *door_io_keymode(void);

// door_io.c's monotonic millisecond clock, exposed so door_input.c's evdev
// settle-window check runs in the same clock domain as the pacing.
uint32_t door_io_now_ms(void);

// Advance to the next available graphics tier (the F4 action). door_io.c's own
// probe-reply parser handles the SS3/CSI/kitty "S" and legacy Ctrl-D forms of
// F4; door_input.c calls this for the forms that reach IT instead -- an evdev
// keycode (SyncTERM cap-8 mode) or an rxvt-style ESC[14~.
void door_io_tier_cycle(void);

// The image-tier fit rectangle for the CURRENT canvas/fit-mode state
// (termgfx_geom_fit_ex()/termgfx_geom_center(), same math door_io_present()
// itself uses to size/center the next frame it sends) -- door_input.c's SGR
// mouse mapper calls this to turn a reported terminal cell into a game pixel
// coordinate, reusing door_io.c's canvas/fit-mode STATE instead of
// re-deriving it. ew/eh/dx/dy are all >=0; 0 before the first probe reply
// lands (caller should fall back to the native DOOR_FB_WIDTH x
// DOOR_FB_HEIGHT).
void door_io_get_rect(int *ew, int *eh, int *dx, int *dy);

// The terminal's real character-cell pixel size, derived from the probed
// canvas/grid (falls back to 8x16 before the probes answer). door_input.c's SGR
// mouse mapper uses it so a reported cell maps to a canvas pixel in the same
// space door_io_get_rect()'s rect lives in -- assuming 8x16 misplaces the
// cursor on a scaled canvas / non-8x16 font.
void door_io_cell_size(int *cw, int *ch);

// True once SGR-Pixels mouse (DEC private mode 1016) is confirmed active, so
// SGR mouse reports carry canvas PIXELS instead of text cells -- the mapper
// then skips the cell->pixel step. Windows Terminal supports it; SyncTERM does
// not (stays cell-granular). False until/unless the DECRQM reply confirms it.
int door_io_mouse_pixels(void);

// The text grid (cols x rows) as last probed, or 0 before a reply lands.
void door_io_grid(int *cols, int *rows);

// door_input.c reports that a mouse coordinate exceeded the text grid -- proof
// the terminal is sending PIXELS (SGR-Pixels live) even if the DECRQM handshake
// didn't confirm it. Latches pixel mode.
void door_io_note_pixel_report(void);

// The audio manager (Task 4) -- owned/created by door_io_init() (same
// lifetime/ownership pattern as syncduke_io.c's sd_audio). door/
// soundio_termgfx.cpp (the engine's SoundImp_* audio backend) calls this to
// reach it; NULL only before the very first door_io_present()/door_io_pump()
// call has run door_io_init().
termgfx_audio_t *door_io_audio(void);

// The user's alias/handle parsed from DOOR32.SYS line 7 (used for Ctrl-P page
// "from" field), or "" if no DOOR32.SYS was given or the alias line was empty.
// Mirrors syncduke_door_alias() (syncduke.h).
const char *door_io_alias(void);

#ifdef __cplusplus
}
#endif

#endif // DOOR_IO_H_
