// pace.c -- see pace.h.  Shared AIMD pipeline-depth controller for termgfx doors.

#include "pace.h"

void termgfx_aimd_update(int enabled, int *depth, uint32_t *adj_at,
                         uint32_t rtt_ms, uint32_t rtt_min, int rt_high,
                         int depth_max, uint32_t now_ms)
{
	int d, ceil_d, floor_d;

	if (!enabled || rtt_min == 0)
		return;
	d = *depth;
	if (rtt_ms > rtt_min * 2) {                          // heavy queuing -> drain now
		if (d > 1)
			d--;
		*adj_at = now_ms;
	} else if ((uint32_t)(now_ms - *adj_at) >= 400) {    // else nudge, rate-limited
		*adj_at = now_ms;
		if (rtt_ms > rtt_min + (rtt_min >> 1))           // >1.5x -> ease down
			d--;
		else if (rtt_ms < rtt_min + (rtt_min >> 2))      // <1.25x -> probe up
			d++;
		// dead-band 1.25x..1.5x -> hold
	}
	ceil_d = (int)((rtt_min + 39) / 40);
	if (ceil_d > depth_max)
		ceil_d = depth_max;
	if (rt_high && ceil_d < 2)
		ceil_d = 2;                     // a transiently-low baseline can't strand us at depth 1
	floor_d = rt_high ? 2 : 1;          // sticky: once the round-trip is non-trivial, never depth 1
	if (floor_d > ceil_d)
		floor_d = ceil_d;
	if (d > ceil_d)
		d = ceil_d;
	if (d < floor_d)
		d = floor_d;
	*depth = d;
}

int termgfx_rtt_sample(uint32_t *rtt_ms, uint32_t *rtt_min, uint32_t *rtt_min_at,
                       int *rt_high, uint32_t rtt, uint32_t now_ms,
                       int stale_reject, uint32_t rtt_min_window_ms)
{
	if (stale_reject && *rtt_ms != 0 && rtt < *rtt_ms / 3)
		return 0;                       // stale: a late reply for a reclaimed frame
	*rtt_ms = *rtt_ms ? (uint32_t)((*rtt_ms * 3 + rtt) / 4) : rtt;   // 3/4 EMA
	if (*rtt_ms > 40)
		*rt_high = 1;                   // latched: a non-trivial round-trip floors depth at 2
	if (*rtt_min == 0 || rtt <= *rtt_min
	    || (rtt_min_window_ms && (uint32_t)(now_ms - *rtt_min_at) > rtt_min_window_ms)) {
		*rtt_min    = rtt;
		*rtt_min_at = now_ms;
	}
	return 1;
}
