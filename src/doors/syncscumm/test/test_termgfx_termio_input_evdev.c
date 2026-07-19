/* Unit test for termgfx_termio's evdev physical-key decode path (M3 Task 5 review
 * fix): negotiates evdev on via the same CTDA cap-8 reply csi_final()
 * parses (g_csi_par[0] == '<', cap 8 among the params), waits out the
 * held-key settle window (TERMGFX_KEYMODE_SETTLE_MS, ../../termgfx/keymode.h)
 * so press edges aren't dropped as re-reported already-held keys, then
 * drives real evdev press/release reports through
 * parse_bytes()/sst_evdev_edge() -- no new SST_TEST seam needed,
 * termgfx_termio_test_feed() reaches all of it already. Own process/binary:
 * enabling evdev here would be refused if kitty (test_termgfx_termio_input.c) had
 * already won the same g_km in that process. cc'd (with -DSST_TEST) + run
 * by unit_termgfx_termio.sh. */
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include "termgfx_termio.h"

int termgfx_termio_test_feed(const char *bytes, size_t n);

int main(void) {
	termgfx_input_event_t ev;

	/* CTDA reply advertising cap 8 (physical key reports): enables evdev. */
	termgfx_termio_test_feed("\x1b[<8c", 5);

	/* SyncTERM re-reports already-held keys right after evdev turns on;
	 * termgfx_termio deliberately drops press edges during that settle window. Wait
	 * it out so this test exercises steady-state behavior, not the
	 * held-key-resync special case. */
	usleep(520000);

	/* bare 'q' (evdev code 16, no Ctrl modifier held): forwarded as an
	 * ordinary key, NOT the quit hotkey. */
	while (termgfx_termio_next_event(&ev)) {}   /* drain */
	termgfx_termio_test_feed("\x1b[=16K", 6);   /* press */
	assert(termgfx_termio_next_event(&ev) && ev.type == TERMGFX_EV_KEY_DOWN
	       && ev.keycode == 'q' && ev.ascii == 'q' && ev.mods == 0);
	assert(!termgfx_termio_quit_requested());
	termgfx_termio_test_feed("\x1b[=16k", 6);   /* release */
	while (termgfx_termio_next_event(&ev)) {}   /* drain */

	/* Ctrl-Q: hold LeftCtrl (evdev code 29) then press 'q' (code 16) --
	 * quits, and is swallowed, not forwarded as a key. */
	termgfx_termio_test_feed("\x1b[=29K", 6);   /* LeftCtrl down */
	termgfx_termio_test_feed("\x1b[=16K", 6);   /* 'q' down, with ctrl held */
	assert(!termgfx_termio_next_event(&ev));
	assert(termgfx_termio_quit_requested());

	return 0;
}
