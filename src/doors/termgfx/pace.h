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

// Fold one DSR round-trip sample (`rtt`, ms) into the smoothed RTT (*rtt_ms, a 3/4
// EMA) and the windowed baseline (*rtt_min / *rtt_min_at, the unloaded latency).
// Latches *rt_high once the smoothed RTT exceeds 40ms (a non-trivial round-trip ->
// the caller floors the pipeline depth at 2).  Returns 1 if the sample was accepted
// (the caller should then run termgfx_aimd_update), 0 if rejected.
//   stale_reject != 0  -> drop a sample far below the EMA (rtt < rtt_ms/3): a late
//                         reply for a reclaimed frame reads absurdly low and would
//                         poison the baseline.
//   rtt_min_window_ms   -> re-seed the baseline if no lower sample arrived for this
//                         long, so a genuinely-risen baseline isn't mistaken for
//                         permanent queuing.  0 = never re-seed (baseline only
//                         ratchets down).
int termgfx_rtt_sample(uint32_t *rtt_ms, uint32_t *rtt_min, uint32_t *rtt_min_at,
                       int *rt_high, uint32_t rtt, uint32_t now_ms,
                       int stale_reject, uint32_t rtt_min_window_ms);

#endif // TERMGFX_PACE_H_
