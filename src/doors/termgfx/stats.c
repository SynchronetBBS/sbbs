#include <stdio.h>

#include "stats.h"

/* Clamp the numeric fields at four digits. A strip that grows past the row
 * wraps and scrolls the game view up, so an outlier sample has to be made
 * ugly rather than allowed to be wide. */
#define STATS_CLAMP 9999u

static uint32_t stats_clamp(uint32_t v)
{
	return v > STATS_CLAMP ? STATS_CLAMP : v;
}

void termgfx_stats_frame(termgfx_stats_t *st, uint32_t wire_bytes)
{
	if (st == NULL)
		return;
	if (wire_bytes != 0)
		st->frames++;
	st->bytes += wire_bytes;
}

int termgfx_stats_roll(termgfx_stats_t *st, uint32_t now_ms)
{
	uint32_t span;

	if (st == NULL)
		return 0;
	if (st->at_ms == 0) {
		st->at_ms = now_ms;
		return 0;
	}
	/* Signed difference so a wrapped millisecond clock reads as a small
	 * elapsed rather than ~4 billion -- the idiom idle.c uses. */
	if ((int32_t)(now_ms - st->at_ms) < TERMGFX_STATS_WINDOW_MS)
		return 0;

	span       = now_ms - st->at_ms;
	st->fps    = (uint32_t)(st->frames * 1000u / span);
	st->kbps   = (uint32_t)(st->bytes * 1000u / 1024u / span);
	st->frames = 0;
	st->bytes  = 0;
	st->at_ms  = now_ms;
	return 1;
}

const char *termgfx_stats_rate(char *buf, size_t bufsz, uint32_t kbps)
{
	if (buf == NULL || bufsz == 0)
		return "";
	if (kbps > 999) {
		/* One decimal, truncated: 1024 KiB/s reads "1.0MB/s". The tenths
		 * are computed from the remainder rather than by dividing a
		 * scaled value, which would overflow a uint32_t past ~4 GiB/s. */
		uint32_t mb = kbps / 1024u;
		uint32_t tenths = (kbps % 1024u) * 10u / 1024u;
		snprintf(buf, bufsz, "%u.%uMB/s", stats_clamp(mb), tenths);
	} else {
		snprintf(buf, bufsz, "%uKB/s", kbps);
	}
	return buf;
}

int termgfx_stats_head(char *buf, size_t bufsz, const char *tier,
                       uint32_t fps, uint32_t kbps,
                       uint32_t rtt_ms, uint32_t rtt_min_ms,
                       int depth, int depth_auto)
{
	char rate[TERMGFX_STATS_RATE_MAX];

	if (buf == NULL || bufsz == 0)
		return 0;
	return snprintf(buf, bufsz, " %s %ufps %s lag %u/%ums depth %d%s",
	                tier != NULL ? tier : "?",
	                stats_clamp(fps),
	                termgfx_stats_rate(rate, sizeof rate, kbps),
	                stats_clamp(rtt_ms), stats_clamp(rtt_min_ms),
	                depth, depth_auto ? "/auto" : "");
}

int termgfx_stats_kbd(char *buf, size_t bufsz, int active, int kitty, int native)
{
	if (buf == NULL || bufsz == 0)
		return 0;
	if (!active) {
		buf[0] = '\0';
		return 0;
	}
	return snprintf(buf, bufsz, " %s/%s",
	                kitty ? "kitty" : "evdev",
	                native ? "nat" : "syn");
}

int termgfx_stats_zoom(char *buf, size_t bufsz, int zoom_x, int zoom_y)
{
	if (buf == NULL || bufsz == 0)
		return 0;
	if (zoom_x <= 1 && zoom_y <= 1) {
		buf[0] = '\0';
		return 0;
	}
	if (zoom_x == zoom_y)
		return snprintf(buf, bufsz, " x%d", zoom_x);
	return snprintf(buf, bufsz, " x%dx%d", zoom_x, zoom_y);
}
