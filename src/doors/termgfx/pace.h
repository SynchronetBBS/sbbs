// pace.h -- shared frame-pacing helpers for termgfx game doors.
//
// SyncDuke and SyncDOOM pace frames to the terminal's real render rate with a
// DSR-ack pipeline (send a frame + ESC[6n, wait for the reply), and settle the
// pipeline DEPTH from the measured round-trip with an identical AIMD controller.
// This is the one shared copy of that controller; the DSR ring, RTT bookkeeping,
// reclaim policy and the door-specific tuning (depth ceiling, initial depth) stay
// in each door and are passed in -- this function only nudges the depth.

#ifndef TERMGFX_PACE_H_
#define TERMGFX_PACE_H_

#include <stdint.h>

// Re-evaluate the auto pipeline depth (delay-based AIMD).  A no-op unless `enabled`
// (the door's auto-pace mode) and a baseline RTT has been seen (rtt_min != 0).
// *depth is nudged toward ~one in-flight frame per 40ms of baseline RTT, capped at
// depth_max:
//   - EMA  > 2.00x baseline -> decrease now            (heavy queuing)
//   - EMA  < 1.25x baseline -> probe up, <=once/400ms  (clean)
//   - EMA  > 1.50x baseline -> ease down, <=once/400ms (mild queuing)
//   - 1.25x .. 1.50x        -> hold                    (dead-band)
// Floor is 2 once rt_high is latched (a non-trivial round-trip), else 1.  *adj_at
// carries the last-nudge timestamp (the 400ms rate-limit state) across calls.
void termgfx_aimd_update(int enabled, int *depth, uint32_t *adj_at,
                         uint32_t rtt_ms, uint32_t rtt_min, int rt_high,
                         int depth_max, uint32_t now_ms);

#endif // TERMGFX_PACE_H_
