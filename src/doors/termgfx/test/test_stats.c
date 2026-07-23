#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "stats.h"

int main(void) {
	termgfx_stats_t st;
	char            buf[64];

	/* --- the rate token: the bug this module was extracted to fix -------- */

	/* Under the threshold it is whole KB/s, exactly as before. */
	assert(strcmp(termgfx_stats_rate(buf, sizeof buf, 0), "0KB/s") == 0);
	assert(strcmp(termgfx_stats_rate(buf, sizeof buf, 412), "412KB/s") == 0);

	/* 999 is the last KB/s value; 1000 already abbreviates. SyncRetro used to
	 * print "1382KB/s" here while its siblings printed "1.3MB/s". */
	assert(strcmp(termgfx_stats_rate(buf, sizeof buf, 999), "999KB/s") == 0);
	assert(strcmp(termgfx_stats_rate(buf, sizeof buf, 1000), "0.9MB/s") == 0);
	assert(strcmp(termgfx_stats_rate(buf, sizeof buf, 1024), "1.0MB/s") == 0);
	assert(strcmp(termgfx_stats_rate(buf, sizeof buf, 1382), "1.3MB/s") == 0);

	/* Tenths come off the remainder, so a rate big enough to overflow a
	 * scaled uint32_t (past ~4 GiB/s) still formats. */
	assert(strcmp(termgfx_stats_rate(buf, sizeof buf, 4194304u), "4096.0MB/s") == 0);

	/* --- the shared head ------------------------------------------------- */

	termgfx_stats_head(buf, sizeof buf, "sixel", 30, 412, 42, 12, 3, 0);
	assert(strcmp(buf, " sixel 30fps 412KB/s lag 42/12ms depth 3") == 0);

	termgfx_stats_head(buf, sizeof buf, "jxl", 35, 1382, 42, 12, 4, 1);
	assert(strcmp(buf, " jxl 35fps 1.3MB/s lag 42/12ms depth 4/auto") == 0);

	/* Clamped at four digits: a garbage sample must not widen the row past
	 * the terminal and wrap the strip onto the game view. */
	termgfx_stats_head(buf, sizeof buf, "text", 123456, 0, 999999, 70000, 1, 0);
	assert(strcmp(buf, " text 9999fps 0KB/s lag 9999/9999ms depth 1") == 0);

	/* --- the rolling window ---------------------------------------------- */

	memset(&st, 0, sizeof st);

	/* The first roll only opens the window: nothing to report yet, the same
	 * ~2s warm-up gap in every door. */
	assert(termgfx_stats_roll(&st, 1000) == 0);
	assert(st.fps == 0 && st.kbps == 0);

	/* 20 frames of 100 KiB over exactly 2s = 10fps, 1000KB/s. */
	{
		int i;
		for (i = 0; i < 20; i++)
			termgfx_stats_frame(&st, 102400);
	}
	assert(termgfx_stats_roll(&st, 2999) == 0);       /* window still open */
	assert(termgfx_stats_roll(&st, 3000) == 1);
	assert(st.fps == 10);
	assert(st.kbps == 1000);
	assert(st.frames == 0 && st.bytes == 0);          /* fresh window */

	/* A de-duped iteration (0 bytes) counts no frame but keeps the window
	 * running, so a still scene decays to 0fps instead of freezing. */
	{
		int i;
		for (i = 0; i < 20; i++)
			termgfx_stats_frame(&st, 0);
	}
	assert(termgfx_stats_roll(&st, 5000) == 1);
	assert(st.fps == 0);
	assert(st.kbps == 0);

	/* A wrapped millisecond clock reads as a small elapsed, not ~4 billion:
	 * the window must not close early on the wrap. */
	memset(&st, 0, sizeof st);
	termgfx_stats_roll(&st, 0xFFFFF000u);                /* window opens near the wrap */
	termgfx_stats_frame(&st, 4352u * 1024u);             /* one 4352KiB frame */
	assert(termgfx_stats_roll(&st, 0xFFFFF700u) == 0);   /* 1792ms: still open */
	assert(termgfx_stats_roll(&st, 0x00000100u) == 1);   /* 4352ms, across the wrap */
	assert(st.kbps == 1000);                             /* i.e. the span really was 4352ms */

	/* --- the two shared tokens ------------------------------------------- */

	assert(termgfx_stats_kbd(buf, sizeof buf, 0, 0, 0) == 0 && buf[0] == '\0');
	termgfx_stats_kbd(buf, sizeof buf, 1, 0, 1);
	assert(strcmp(buf, " evdev/nat") == 0);
	termgfx_stats_kbd(buf, sizeof buf, 1, 1, 0);
	assert(strcmp(buf, " kitty/syn") == 0);

	assert(termgfx_stats_zoom(buf, sizeof buf, 1, 1) == 0 && buf[0] == '\0');
	termgfx_stats_zoom(buf, sizeof buf, 2, 2);
	assert(strcmp(buf, " x2") == 0);
	termgfx_stats_zoom(buf, sizeof buf, 2, 3);
	assert(strcmp(buf, " x2x3") == 0);

	return 0;
}
