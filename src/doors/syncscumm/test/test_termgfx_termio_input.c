/* Unit test for termgfx_termio's keyboard decode: drives termgfx_termio_test_feed() (the
 * SST_TEST seam) with raw wire bytes -- no socket, no termgfx_termio_init() -- and
 * reads back the queued key events via termgfx_termio_next_event(). Covers the
 * legacy P_NORMAL byte path, the legacy CSI/SS3 nav-key path, and (by
 * feeding the same "CSI?u" negotiation reply csi_final() parses) the kitty
 * CSI-u key-report path -- no new seam needed, termgfx_termio_test_feed() reaches
 * all of it through the real parser. The evdev physical-key path still
 * needs its own fresh process (see test_termgfx_termio_input_evdev.c): once kitty is
 * enabled here, termgfx's "evdev wins" guard would refuse to also enable
 * evdev in the same g_km. cc'd (with -DSST_TEST) + run by unit_termgfx_termio.sh. */
#include <assert.h>
#include <string.h>
#include "termgfx_termio.h"

int termgfx_termio_test_feed(const char *bytes, size_t n);

int main(void) {
	termgfx_input_event_t ev;

	/* A printable byte -> KEY_DOWN keycode==ascii=='a'. */
	termgfx_termio_test_feed("a", 1);
	assert(termgfx_termio_next_event(&ev) && ev.type == TERMGFX_EV_KEY_DOWN
	       && ev.keycode == 'a' && ev.ascii == 'a');

	/* '+' is no longer eaten as a volume hotkey -- it reaches ScummVM. */
	termgfx_termio_test_feed("+", 1);
	assert(termgfx_termio_next_event(&ev) && ev.type == TERMGFX_EV_KEY_DOWN
	       && ev.keycode == '+' && ev.ascii == '+');

	/* legacy arrow: ESC [ A -> KEY_DOWN keycode==TERMGFX_KEY_UP. */
	termgfx_termio_test_feed("\x1b[A", 3);
	assert(termgfx_termio_next_event(&ev) && ev.type == TERMGFX_EV_KEY_DOWN
	       && ev.keycode == TERMGFX_KEY_UP);

	/* legacy F5: ESC [ 1 5 ~ -> TERMGFX_KEY_F5. */
	termgfx_termio_test_feed("\x1b[15~", 5);
	assert(termgfx_termio_next_event(&ev) && ev.type == TERMGFX_EV_KEY_DOWN
	       && ev.keycode == TERMGFX_KEY_F5);

	/* Ctrl-S stays door-reserved (stats toggle) -- NOT forwarded. */
	while (termgfx_termio_next_event(&ev)) {}   /* drain */
	termgfx_termio_test_feed("\x13", 1);
	assert(!termgfx_termio_next_event(&ev));

	/* Ctrl+letter (not Ctrl-S/Ctrl-C): letter keycode + TERMGFX_MOD_CTRL. */
	while (termgfx_termio_next_event(&ev)) {}   /* drain */
	termgfx_termio_test_feed("\x04", 1);   /* Ctrl-D */
	assert(termgfx_termio_next_event(&ev) && ev.type == TERMGFX_EV_KEY_DOWN
	       && ev.keycode == 'd' && ev.mods == TERMGFX_MOD_CTRL);

	/* Enter/Backspace/Tab -> their TERMGFX_KEY_*. */
	while (termgfx_termio_next_event(&ev)) {}   /* drain */
	termgfx_termio_test_feed("\x0d", 1);
	assert(termgfx_termio_next_event(&ev) && ev.keycode == TERMGFX_KEY_ENTER);
	termgfx_termio_test_feed("\x08", 1);
	assert(termgfx_termio_next_event(&ev) && ev.keycode == TERMGFX_KEY_BACKSPACE);
	termgfx_termio_test_feed("\x09", 1);
	assert(termgfx_termio_next_event(&ev) && ev.keycode == TERMGFX_KEY_TAB);

	/* legacy Home/End: ESC [ H / ESC [ F. */
	while (termgfx_termio_next_event(&ev)) {}   /* drain */
	termgfx_termio_test_feed("\x1b[H", 3);
	assert(termgfx_termio_next_event(&ev) && ev.keycode == TERMGFX_KEY_HOME);
	termgfx_termio_test_feed("\x1b[F", 3);
	assert(termgfx_termio_next_event(&ev) && ev.keycode == TERMGFX_KEY_END);

	/* legacy SS3 F1/F2 (kitty inactive, so these dispatch as a plain tap). */
	while (termgfx_termio_next_event(&ev)) {}   /* drain */
	termgfx_termio_test_feed("\x1bOP", 3);
	assert(termgfx_termio_next_event(&ev) && ev.keycode == TERMGFX_KEY_F1);
	termgfx_termio_test_feed("\x1bOQ", 3);
	assert(termgfx_termio_next_event(&ev) && ev.keycode == TERMGFX_KEY_F2);

	/* --- kitty CSI-u path: negotiate kitty on via the same "CSI?u" reply
	 * csi_final() itself parses (g_csi_par[0] == '?'), then drive real
	 * kitty key reports through parse_bytes()/csi_final() -- no new
	 * SST_TEST seam needed, termgfx_termio_test_feed() reaches all of it already.
	 * Regression coverage for the review fix: bare 'q' must NOT be the
	 * quit hotkey once ScummVM's Global Main Menu save-name field needs to
	 * accept it; only Ctrl-Q (and Ctrl-C) may quit. */
	termgfx_termio_test_feed("\x1b[?u", 4);              /* enable kitty */

	/* bare 'q' (key 113, no modifier field): forwarded as an ordinary key,
	 * NOT the quit hotkey. */
	while (termgfx_termio_next_event(&ev)) {}   /* drain */
	termgfx_termio_test_feed("\x1b[113u", 6);
	assert(termgfx_termio_next_event(&ev) && ev.type == TERMGFX_EV_KEY_DOWN
	       && ev.keycode == 'q' && ev.ascii == 'q' && ev.mods == 0);
	assert(!termgfx_termio_quit_requested());

	/* Ctrl-Q (key 113, kitty modifier field 5 == 1 + Ctrl bit 4): quits,
	 * and is swallowed -- not forwarded as a key. */
	while (termgfx_termio_next_event(&ev)) {}   /* drain */
	termgfx_termio_test_feed("\x1b[113;5u", 8);
	assert(!termgfx_termio_next_event(&ev));
	assert(termgfx_termio_quit_requested());

	/* legacy P_NORMAL quit ('q', unmodified) also stays door-reserved --
	 * NOT forwarded. Out of scope for the kitty/evdev review fix (P_NORMAL
	 * is the brief-scoped legacy behavior), so it stays as-is; ordered
	 * last because g_quit is a one-way latch and every check above this
	 * point depends on quit NOT having fired yet. */
	while (termgfx_termio_next_event(&ev)) {}   /* drain */
	termgfx_termio_test_feed("q", 1);
	assert(!termgfx_termio_next_event(&ev));

	return 0;
}
