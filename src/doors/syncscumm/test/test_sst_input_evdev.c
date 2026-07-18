/* Unit test for sst_io's evdev physical-key decode path (M3 Task 5 review
 * fix): negotiates evdev on via the same CTDA cap-8 reply csi_final()
 * parses (g_csi_par[0] == '<', cap 8 among the params), waits out the
 * held-key settle window (TERMGFX_KEYMODE_SETTLE_MS, ../../termgfx/keymode.h)
 * so press edges aren't dropped as re-reported already-held keys, then
 * drives real evdev press/release reports through
 * parse_bytes()/sst_evdev_edge() -- no new SST_TEST seam needed,
 * sst_io_test_feed() reaches all of it already. Own process/binary:
 * enabling evdev here would be refused if kitty (test_sst_input.c) had
 * already won the same g_km in that process. cc'd (with -DSST_TEST) + run
 * by unit_sst_io.sh. */
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include "sst_io.h"

int sst_io_test_feed(const char *bytes, size_t n);

int main(void) {
	sst_input_event_t ev;

	/* CTDA reply advertising cap 8 (physical key reports): enables evdev. */
	sst_io_test_feed("\x1b[<8c", 5);

	/* SyncTERM re-reports already-held keys right after evdev turns on;
	 * sst_io deliberately drops press edges during that settle window. Wait
	 * it out so this test exercises steady-state behavior, not the
	 * held-key-resync special case. */
	usleep(520000);

	/* bare 'q' (evdev code 16, no Ctrl modifier held): forwarded as an
	 * ordinary key, NOT the quit hotkey. */
	while (sst_io_next_event(&ev)) {}   /* drain */
	sst_io_test_feed("\x1b[=16K", 6);   /* press */
	assert(sst_io_next_event(&ev) && ev.type == SST_EV_KEY_DOWN
	       && ev.keycode == 'q' && ev.ascii == 'q' && ev.mods == 0);
	assert(!sst_io_quit_requested());
	sst_io_test_feed("\x1b[=16k", 6);   /* release */
	while (sst_io_next_event(&ev)) {}   /* drain */

	/* Ctrl-Q: hold LeftCtrl (evdev code 29) then press 'q' (code 16) --
	 * quits, and is swallowed, not forwarded as a key. */
	sst_io_test_feed("\x1b[=29K", 6);   /* LeftCtrl down */
	sst_io_test_feed("\x1b[=16K", 6);   /* 'q' down, with ctrl held */
	assert(!sst_io_next_event(&ev));
	assert(sst_io_quit_requested());

	return 0;
}
