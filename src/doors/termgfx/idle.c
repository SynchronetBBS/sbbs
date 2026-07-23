#include <stddef.h>

#include "idle.h"

void termgfx_idle_init(termgfx_idle_t *idle, unsigned threshold_s,
                       unsigned warn_s, uint32_t now_ms)
{
	if (idle == NULL)
		return;
	if (warn_s > threshold_s)
		warn_s = threshold_s;
	idle->threshold_ms = (uint32_t)threshold_s * 1000u;
	idle->warn_ms      = (uint32_t)warn_s * 1000u;
	idle->last_ms      = now_ms;
}

void termgfx_idle_activity(termgfx_idle_t *idle, uint32_t now_ms)
{
	if (idle != NULL)
		idle->last_ms = now_ms;
}

termgfx_idle_state_t termgfx_idle_poll(termgfx_idle_t *idle, uint32_t now_ms,
                                       unsigned *secs_left)
{
	int32_t  elapsed;
	uint32_t remain;

	if (secs_left != NULL)
		*secs_left = 0;
	if (idle == NULL || idle->threshold_ms == 0)
		return TERMGFX_IDLE_ACTIVE;

	/* Signed difference, so a wrapped millisecond clock reads as a small
	 * elapsed rather than ~4 billion. Same idiom as the doors' -t deadlines. */
	elapsed = (int32_t)(now_ms - idle->last_ms);
	if (elapsed < 0)
		elapsed = 0;

	if ((uint32_t)elapsed >= idle->threshold_ms)
		return TERMGFX_IDLE_EXPIRED;
	if ((uint32_t)elapsed < idle->threshold_ms - idle->warn_ms)
		return TERMGFX_IDLE_ACTIVE;

	remain = idle->threshold_ms - (uint32_t)elapsed;
	if (secs_left != NULL)
		*secs_left = (unsigned)((remain + 999u) / 1000u);
	return TERMGFX_IDLE_WARN;
}
