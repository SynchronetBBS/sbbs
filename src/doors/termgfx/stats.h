#ifndef TERMGFX_STATS_H_
#define TERMGFX_STATS_H_

#include <stddef.h>
#include <stdint.h>

/* stats.h -- the shared live-stats strip for termgfx game doors.
 *
 * SyncDOOM, SyncDuke and SyncRetro all draw the same Ctrl-S status strip on
 * the bottom row, and all three grew their own copy of it: a rolling ~2s
 * window over emitted frames and wire bytes, and a readout that starts
 *
 *     " <tier> <fps>fps <rate> lag <cur>/<min>ms d<n>[/auto]"
 *
 * before each door appends its own fields. Three copies meant three chances
 * to drift, and they had: SyncRetro printed raw KB/s, so a fast link showed
 * "1382KB/s" where its siblings showed "1.3MB/s", and it clamped none of the
 * numbers, so an outlier could push the strip wider than the row.
 *
 * What belongs here is the part that must READ the same everywhere -- the
 * window arithmetic and the field formatting. What does NOT is painting: the
 * doors disagree about which row is free, how output reaches the wire, and
 * what else competes for the row (SyncRetro shares it with a volume toast).
 * Each door keeps its own emit/toggle for that reason.
 *
 * No clock of its own: the caller supplies a monotonic millisecond stamp,
 * the same contract as idle.h.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* The window the doors settled on independently. Long enough that a de-duped
 * still scene decays visibly instead of flickering, short enough to react. */
#define TERMGFX_STATS_WINDOW_MS 2000

/* Widest a rate token can get, including the NUL: "1023.9MB/s". */
#define TERMGFX_STATS_RATE_MAX  12

typedef struct {
	uint32_t at_ms;                 /* start of the open window; 0 = not started */
	uint32_t frames;                /* frames emitted in the open window         */
	uint64_t bytes;                 /* wire bytes in the open window             */
	uint32_t fps;                   /* result of the last window that closed     */
	uint32_t kbps;                  /* ditto, KiB/s of wire bytes                */
} termgfx_stats_t;

/* Account for one iteration of the frame loop. wire_bytes == 0 means nothing
 * was sent (a de-duped frame): the window keeps running but no frame is
 * counted, which is what makes a still scene decay toward 0fps rather than
 * freezing the readout at the last value it happened to reach. */
void termgfx_stats_frame(termgfx_stats_t *st, uint32_t wire_bytes);

/* Close the window if TERMGFX_STATS_WINDOW_MS has passed, recomputing fps and
 * kbps and starting a fresh one. Returns 1 when it recomputed -- doors that
 * roll their own per-window numbers (SyncRetro's dirty-rect share) hang them
 * off that. The first call only starts the window, so the readout has the same
 * ~2s "nothing yet" gap in every door. */
int termgfx_stats_roll(termgfx_stats_t *st, uint32_t now_ms);

/* Throughput token: "%uKB/s" up to 999, then fractional "%u.%uMB/s", so the
 * field stays narrow on a fast link (KiB/MiB of wire bytes throughout).
 * Returns buf, so it can be used inline in a snprintf argument list. */
const char *termgfx_stats_rate(char *buf, size_t bufsz, uint32_t kbps);

/* The head every door's strip shares, with a leading and no trailing space:
 *
 *     " sixel 30fps 412KB/s lag 42/12ms d3/auto"
 *
 * depth_auto appends "/auto" (the AIMD pipeline depth); pass 0 where the door
 * has no auto mode. The depth is spelled "d3" rather than "depth 3" to match
 * termgfx_termio's row and to leave the tail fields room on an 80-column
 * terminal, which the long spelling did not. fps and both round-trips are
 * clamped to 9999 so a garbage sample cannot widen the row. Returns the
 * snprintf length. */
int termgfx_stats_head(char *buf, size_t bufsz, const char *tier,
                       uint32_t fps, uint32_t kbps,
                       uint32_t rtt_ms, uint32_t rtt_min_ms,
                       int depth, int depth_auto);

/* " evdev turn=nat", " kitty turn=syn", or "" when active is 0 (neither
 * protocol up, the byte path): the keyboard protocol, plus how the door drives
 * TURNING on a laggy link -- native (the held key) or synthetic (a constant
 * injected turn, once the release edge arrives too late to stop on target).
 * Shared by the doors that hold a key down.
 *
 * The turn model is spelled out rather than left as a bare second field: it
 * used to read " kitty/nat", one character away from another door's
 * " kitty/cell", which is the MOUSE mode. Two unrelated facts in the same
 * shape and the same place on the row is a readout nobody can trust. */
int termgfx_stats_kbd(char *buf, size_t bufsz, int active, int kitty, int native);

/* " mouse=px" (SGR-Pixels/DEC 1016 live: reports are canvas pixels) or
 * " mouse=cell" (the terminal can only name a text cell, so a click is
 * quantised to one -- ~10 game pixels on a 20px cell). "" when the door
 * asked for no mouse at all, so a keyboard-only door does not carry a field
 * about an input it never reads. */
int termgfx_stats_mouse(char *buf, size_t bufsz, int enabled, int pixels);

/* " x2" / " x2x3" for terminal-side upscale (APC ZX/ZY), "" at 1:1. */
int termgfx_stats_zoom(char *buf, size_t bufsz, int zoom_x, int zoom_y);

/* The dirty-rect share: " dr 84%", " dr n/a" when the client has no cell grid
 * to place a patch in, " dr off" where the door lets a sysop disable patching,
 * and "" before the first window has closed.
 *
 * Here rather than in each door for the reason this file exists: the states
 * have to read the SAME everywhere or the field is worse than useless, and
 * they had already drifted -- one door printed "dr84%" against another's
 * "dr 84%" for the same quantity. Pass state < 0 for "no window yet", and
 * TERMGFX_STATS_DR_NA / _OFF for the two non-numeric ones. */
#define TERMGFX_STATS_DR_NONE (-1)      /* no window has closed yet */
#define TERMGFX_STATS_DR_NA   (-2)      /* no cell grid: cannot patch at all */
#define TERMGFX_STATS_DR_OFF  (-3)      /* the sysop turned patching off */
int termgfx_stats_dr(char *buf, size_t bufsz, int pct_or_state);

/* Clip an assembled strip to `cols` columns in place, returning its new length;
 * cols <= 0 (width not known yet) leaves it alone. The last thing a door does
 * before painting, because the strip sits on the LAST row of the screen: one
 * column too wide and it either loses its tail to the margin or wraps and
 * scrolls the game view up, and which of the two happens is the terminal's
 * choice, not ours. Clamping the individual numbers is not enough -- the
 * fields accumulate, and an 80-column SyncTERM overflowed by gaining one.
 *
 * Cuts at a field boundary (fields begin with a space) so a survivor is never a
 * half-printed value. The strip is ASCII, so a column is a byte. */
size_t termgfx_stats_clip(char *buf, int cols);

#ifdef __cplusplus
}
#endif

#endif /* TERMGFX_STATS_H_ */
