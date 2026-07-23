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
 *     " <tier> <fps>fps <rate> lag <cur>/<min>ms depth <n>[/auto]"
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
 *     " sixel 30fps 412KB/s lag 42/12ms depth 3/auto"
 *
 * depth_auto appends "/auto" (the AIMD pipeline depth); pass 0 where the door
 * has no auto mode. fps and both round-trips are clamped to 9999 so a garbage
 * sample cannot widen the row. Returns the snprintf length. */
int termgfx_stats_head(char *buf, size_t bufsz, const char *tier,
                       uint32_t fps, uint32_t kbps,
                       uint32_t rtt_ms, uint32_t rtt_min_ms,
                       int depth, int depth_auto);

/* " evdev/nat", " kitty/syn", or "" when active is 0 (neither protocol up, the
 * byte path): the keyboard protocol plus its turn-key model (native key-up vs
 * synthetic repeat) in one token. Shared by the doors that hold a key down. */
int termgfx_stats_kbd(char *buf, size_t bufsz, int active, int kitty, int native);

/* " x2" / " x2x3" for terminal-side upscale (APC ZX/ZY), "" at 1:1. */
int termgfx_stats_zoom(char *buf, size_t bufsz, int zoom_x, int zoom_y);

#ifdef __cplusplus
}
#endif

#endif /* TERMGFX_STATS_H_ */
