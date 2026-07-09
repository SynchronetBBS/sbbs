/* syncmoo1.h -- cross-module contract for the syncmoo1 door.
 *
 * DESIGN.md §4: a SINGLE header shared by every syncmoo1_*.c module (plus
 * hw_sbbs.c, the 1oom `hw` backend), so a function that crosses a module
 * boundary has ONE declaration and a provenance comment saying which .c
 * provides it and why -- unlike syncduke/syncconquer's per-file private
 * headers. As later tasks add syncmoo1_input.c/_door.c/_config.c/_node.c,
 * their cross-module entry points get added here too; Task 5 (this file)
 * covers the terminal I/O module only.
 *
 * sm_geom_t (the image-rect type shared between the present path and the
 * input module's mouse mapper) is already defined in syncmoo1_map.h (Task 4)
 * -- pulled in below so every consumer of this header gets it too, instead
 * of a second competing definition here.
 */
#ifndef SYNCMOO1_H
#define SYNCMOO1_H

#include <stddef.h>
#include <stdint.h>

#include "syncmoo1_map.h"   /* sm_geom_t */

/* --- syncmoo1_io.c: terminal out-buffer, enter/probe/leave, sixel present --
 *
 * Consumed by hw_sbbs.c (the 1oom hw backend): hw_video_draw_buf() calls
 * sm_io_present() on every flip, hw_video_set_palette() feeds it a running
 * 768-byte RGB888 palette. Task 6's input module reads sm_io_geom() for the
 * SGR-mouse -> game-pixel mapping.
 */

/* One-time setup: adopt `sockfd` as the door's I/O descriptor (<0 falls back
 * to stdout, fd 1 -- dev/tty use), set it non-blocking, ignore SIGPIPE,
 * resolve the SYNCMOO1_SIXELOUT capture-mode override (see syncmoo1_io.c),
 * and register sm_io_leave() via atexit(). Idempotent -- safe to call more
 * than once (a later call is a no-op). Returns 0. */
int sm_io_init(int sockfd);

/* Encode `idx320x200` (320x200 8-bit palette indices, row-major) through
 * `pal768` (256 RGB888 triples) as a DECSIXEL frame centered in the terminal
 * canvas (a sane 640x400/80x25 default until a later task's probe-reply
 * parser narrows it), stage it, and flush it to the I/O descriptor. Lazily
 * runs sm_io_enter() on the first call, so a caller need not sequence that by
 * hand. The 768-byte palette is (re)defined in the sixel registers only when
 * it actually changed since the last frame (memcmp) -- SyncTERM garbles its
 * decoder if the registers are redefined every frame. M1 (sixel-tier only,
 * DESIGN.md §10): no whole-frame de-dupe and no DSR-ACK pacing -- those are a
 * later task (§5, §9); this is a straight encode+emit every call. */
void sm_io_present(const uint8_t *idx320x200, const uint8_t *pal768);

/* Terminal setup / teardown (DESIGN.md §9). Both idempotent (each runs its
 * work exactly once, however many times it's called): sm_io_present() calls
 * sm_io_enter() lazily on its first invocation; sm_io_init() registers
 * sm_io_leave() via atexit() so a normal exit restores the BBS's terminal
 * (sixel-scroll/autowrap/cursor, mouse tracking off) even if nothing else
 * calls it explicitly. */
void sm_io_enter(void);
void sm_io_leave(void);

/* Append `len` bytes to the staged, grow-only out-buffer (no I/O). */
void sm_out_put(const void *buf, size_t len);

/* Non-blocking drain of the staged out-buffer to the I/O descriptor (or, in
 * SYNCMOO1_SIXELOUT capture mode, a truncate-write of the capture file).
 * EAGAIN/EWOULDBLOCK/EINTR just leave the remainder pending for the next
 * call (a slow client, not an error); a real write error hangs up the
 * session. Returns 0. */
int sm_io_out_flush(void);

/* The image rect the last sm_io_present() drew (or the sane 640x400/80x25
 * default before any frame has been drawn) -- Task 6's sm_map_mouse() reads
 * this so a click maps against the SAME geometry the frame was drawn in.
 * Never NULL; ew/eh are always > 0. */
const sm_geom_t *sm_io_geom(void);

/* The I/O descriptor sm_io_init() adopted (the door socket, or fd 1 in
 * dev/tty use). Task 6's hw_event_handle() wiring reads this to know what to
 * hand sm_input_pump(). */
int sm_io_get_fd(void);

/* Probe-reply setters (Task 6, DESIGN.md Sec9): syncmoo1_input.c's CSI
 * handler calls these as the startup probe replies land (ESC[14t canvas px,
 * the ESC[6n->ESC[r;cR grid, DECRPM ?1016 SGR-Pixels confirmation), so
 * sm_io_geom()'s image rect + real cell size -- and therefore sm_map_mouse()
 * -- become probe-driven instead of the 640x400/80x25/8x16 default. Each
 * recomputes the image rect immediately; a malformed/zero w/h/rows/cols is
 * ignored (keeps the prior value) rather than corrupting the geometry. */
void sm_io_set_canvas(int w, int h);
void sm_io_set_grid(int rows, int cols);
void sm_io_set_pixel_mode(int on);

/* --- syncmoo1_input.c: socket read loop, ESC/CSI/APC state machine, key +
 * mouse decode, capability-probe reply parsing --
 *
 * Consumed by hw_sbbs.c: hw_event_handle() calls sm_input_pump(sm_io_get_fd())
 * on every 1oom engine poll. Drains `sockfd` non-blocking, runs the bytes
 * through the ESC/CSI/APC(DCS/OSC/PM) state machine, and injects the result
 * into 1oom's global input state: keys via kbd_add_keypress() (kbd.h), mouse
 * position/buttons/wheel via mouse_set_xy_from_hw()/mouse_set_buttons_from_hw()/
 * mouse_set_scroll_from_hw() (mouse.h). Capability-probe CSI replies (ESC[14t,
 * the grid ESC[r;cR, DECRPM ?1016 y, DA1/CTDA c, the JXL-cap n) are parsed
 * here and fed into sm_io via the setters above rather than delivered as
 * keys; APC/DCS/OSC/PM string replies (e.g. SyncTERM's C;L cache-list) are
 * swallowed to their ST terminator so they never leak stray keystrokes.
 * Returns 0 normally, <0 if the peer hung up or a real socket read error
 * occurred (the caller should treat the door session as over). */
int sm_input_pump(int sockfd);

#endif /* SYNCMOO1_H */
