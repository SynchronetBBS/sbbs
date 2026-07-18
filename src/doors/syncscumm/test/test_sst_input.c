/* Unit test for sst_io's keyboard decode: drives sst_io_test_feed() (the
 * SST_TEST seam) with raw wire bytes -- no socket, no sst_io_init() -- and
 * reads back the queued key events via sst_io_next_event(). Covers the
 * legacy P_NORMAL byte path and the legacy CSI/SS3 nav-key path (kitty and
 * evdev are exercised live, not by this offline byte-feed harness -- both
 * need a negotiated termgfx_keymode_t state this test never sets up).
 * cc'd (with -DSST_TEST) + run by unit_sst_io.sh. */
#include <assert.h>
#include <string.h>
#include "sst_io.h"

int sst_io_test_feed(const char *bytes, size_t n);

int main(void) {
	sst_input_event_t ev;

	/* A printable byte -> KEY_DOWN keycode==ascii=='a'. */
	sst_io_test_feed("a", 1);
	assert(sst_io_next_event(&ev) && ev.type == SST_EV_KEY_DOWN
	       && ev.keycode == 'a' && ev.ascii == 'a');

	/* '+' is no longer eaten as a volume hotkey -- it reaches ScummVM. */
	sst_io_test_feed("+", 1);
	assert(sst_io_next_event(&ev) && ev.type == SST_EV_KEY_DOWN
	       && ev.keycode == '+' && ev.ascii == '+');

	/* legacy arrow: ESC [ A -> KEY_DOWN keycode==SST_KEY_UP. */
	sst_io_test_feed("\x1b[A", 3);
	assert(sst_io_next_event(&ev) && ev.type == SST_EV_KEY_DOWN
	       && ev.keycode == SST_KEY_UP);

	/* legacy F5: ESC [ 1 5 ~ -> SST_KEY_F5. */
	sst_io_test_feed("\x1b[15~", 5);
	assert(sst_io_next_event(&ev) && ev.type == SST_EV_KEY_DOWN
	       && ev.keycode == SST_KEY_F5);

	/* Ctrl-S stays door-reserved (stats toggle) -- NOT forwarded. */
	while (sst_io_next_event(&ev)) {}   /* drain */
	sst_io_test_feed("\x13", 1);
	assert(!sst_io_next_event(&ev));

	/* quit ('q') also stays door-reserved -- NOT forwarded. */
	while (sst_io_next_event(&ev)) {}   /* drain */
	sst_io_test_feed("q", 1);
	assert(!sst_io_next_event(&ev));

	/* Ctrl+letter (not Ctrl-S/Ctrl-C): letter keycode + SST_MOD_CTRL. */
	while (sst_io_next_event(&ev)) {}   /* drain */
	sst_io_test_feed("\x04", 1);   /* Ctrl-D */
	assert(sst_io_next_event(&ev) && ev.type == SST_EV_KEY_DOWN
	       && ev.keycode == 'd' && ev.mods == SST_MOD_CTRL);

	/* Enter/Backspace/Tab -> their SST_KEY_*. */
	while (sst_io_next_event(&ev)) {}   /* drain */
	sst_io_test_feed("\x0d", 1);
	assert(sst_io_next_event(&ev) && ev.keycode == SST_KEY_ENTER);
	sst_io_test_feed("\x08", 1);
	assert(sst_io_next_event(&ev) && ev.keycode == SST_KEY_BACKSPACE);
	sst_io_test_feed("\x09", 1);
	assert(sst_io_next_event(&ev) && ev.keycode == SST_KEY_TAB);

	/* legacy Home/End: ESC [ H / ESC [ F. */
	while (sst_io_next_event(&ev)) {}   /* drain */
	sst_io_test_feed("\x1b[H", 3);
	assert(sst_io_next_event(&ev) && ev.keycode == SST_KEY_HOME);
	sst_io_test_feed("\x1b[F", 3);
	assert(sst_io_next_event(&ev) && ev.keycode == SST_KEY_END);

	/* legacy SS3 F1/F2 (kitty inactive, so these dispatch as a plain tap). */
	while (sst_io_next_event(&ev)) {}   /* drain */
	sst_io_test_feed("\x1bOP", 3);
	assert(sst_io_next_event(&ev) && ev.keycode == SST_KEY_F1);
	sst_io_test_feed("\x1bOQ", 3);
	assert(sst_io_next_event(&ev) && ev.keycode == SST_KEY_F2);

	return 0;
}
