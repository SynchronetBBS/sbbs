/* Unit test for input_term's termgfx-key -> EasyRPG Input::Keys mapping
 * (M2 / Task 5). Two layers:
 *
 *  1. input_term_map_key() driven directly with hand-built
 *     termgfx_input_event_t structs -- covers the full mapping table,
 *     including the NumLock-off numpad/nav-key ambiguity (HOME/END/PAGEUP/
 *     PAGEDOWN/INSERT/DELETE each drive TWO EasyRPG keys) without needing
 *     evdev's CTDA capability negotiation.
 *
 *  2. input_term_pump() driven through termgfx_termio's real parser via the
 *     TERMGFX_TEST seam (termgfx_termio_test_feed(), the same seam
 *     test_termgfx_termio_input.c in syncscumm/test uses) -- confirms the
 *     tap/hold release-synthesis: a legacy CSI byte (no true release) reads
 *     pressed for exactly the poll it arrived in and is released the next;
 *     a kitty CSI-u press held across several polls (simulating the
 *     terminal's own key-repeat) stays pressed the whole time; and a real
 *     kitty release clears it immediately rather than waiting a poll.
 *
 * cc'd (with -DTERMGFX_TEST, C++17) + run by unit_input_term.sh.
 */
#include <cassert>
#include <bitset>

#include "input_term.h"

extern "C" int termgfx_termio_test_feed(const char *bytes, size_t n);

using Input::Keys::InputKey;

static termgfx_input_event_t key_ev(termgfx_ev_type_t type, int keycode, int ascii = 0, int mods = 0)
{
	termgfx_input_event_t ev;
	ev.type = type;
	ev.x = ev.y = ev.button = ev.wheel = 0;
	ev.keycode = keycode;
	ev.ascii = ascii;
	ev.mods = mods;
	return ev;
}

static void drain_termgfx_events(void)
{
	termgfx_input_event_t ev;
	while (termgfx_termio_next_event(&ev)) {}
}

int main(void)
{
	InputKey out[INPUT_TERM_MAX_KEYS];
	int n;

	/* --- Layer 1: input_term_map_key(), pure, no termgfx state --- */

	/* Arrows. */
	n = input_term_map_key(key_ev(TERMGFX_EV_KEY_DOWN, TERMGFX_KEY_UP), out);
	assert(n == 1 && out[0] == Input::Keys::UP);
	n = input_term_map_key(key_ev(TERMGFX_EV_KEY_DOWN, TERMGFX_KEY_DOWN), out);
	assert(n == 1 && out[0] == Input::Keys::DOWN);
	n = input_term_map_key(key_ev(TERMGFX_EV_KEY_DOWN, TERMGFX_KEY_LEFT), out);
	assert(n == 1 && out[0] == Input::Keys::LEFT);
	n = input_term_map_key(key_ev(TERMGFX_EV_KEY_DOWN, TERMGFX_KEY_RIGHT), out);
	assert(n == 1 && out[0] == Input::Keys::RIGHT);

	/* Decision: Enter/Z/Space (EasyRPG default DECISION binding). */
	n = input_term_map_key(key_ev(TERMGFX_EV_KEY_DOWN, TERMGFX_KEY_ENTER), out);
	assert(n == 1 && out[0] == Input::Keys::RETURN);
	n = input_term_map_key(key_ev(TERMGFX_EV_KEY_DOWN, 'z', 'z'), out);
	assert(n == 1 && out[0] == Input::Keys::Z);
	n = input_term_map_key(key_ev(TERMGFX_EV_KEY_DOWN, 'Z', 'Z'), out);
	assert(n == 1 && out[0] == Input::Keys::Z);   /* case-folded */
	n = input_term_map_key(key_ev(TERMGFX_EV_KEY_DOWN, ' ', ' '), out);
	assert(n == 1 && out[0] == Input::Keys::SPACE);

	/* Cancel: Esc + the README's [X, C, V, B, N] (EasyRPG default CANCEL). */
	n = input_term_map_key(key_ev(TERMGFX_EV_KEY_DOWN, TERMGFX_KEY_ESCAPE), out);
	assert(n == 1 && out[0] == Input::Keys::ESCAPE);
	{
		const char letters[] = "xcvbn";
		const InputKey keys_[] = {
			Input::Keys::X, Input::Keys::C, Input::Keys::V,
			Input::Keys::B, Input::Keys::N
		};
		for (int i = 0; i < 5; i++) {
			n = input_term_map_key(key_ev(TERMGFX_EV_KEY_DOWN, letters[i], letters[i]), out);
			assert(n == 1 && out[0] == keys_[i]);
		}
	}

	/* Top-row digits -> N0..N9 (the game's "Numbers" key input, and the
	 * README's "or the normal number keys"). */
	for (int d = 0; d <= 9; d++) {
		char c = (char)('0' + d);
		n = input_term_map_key(key_ev(TERMGFX_EV_KEY_DOWN, c, c), out);
		assert(n == 1 && out[0] == (InputKey)(Input::Keys::N0 + d));
	}

	/* NumLock-off numpad / dedicated-key ambiguity: each drives BOTH the
	 * plain nav key and the numpad twin Yume Nikki actually reads. */
	n = input_term_map_key(key_ev(TERMGFX_EV_KEY_DOWN, TERMGFX_KEY_PAGEUP), out);   /* wake */
	assert(n == 2 && out[0] == Input::Keys::PGUP && out[1] == Input::Keys::KP9);
	n = input_term_map_key(key_ev(TERMGFX_EV_KEY_DOWN, TERMGFX_KEY_END), out);      /* Effect action */
	assert(n == 2 && out[0] == Input::Keys::ENDS && out[1] == Input::Keys::KP1);
	n = input_term_map_key(key_ev(TERMGFX_EV_KEY_DOWN, TERMGFX_KEY_PAGEDOWN), out); /* Effect action */
	assert(n == 2 && out[0] == Input::Keys::PGDN && out[1] == Input::Keys::KP3);
	n = input_term_map_key(key_ev(TERMGFX_EV_KEY_DOWN, TERMGFX_KEY_HOME), out);
	assert(n == 2 && out[0] == Input::Keys::HOME && out[1] == Input::Keys::KP7);
	n = input_term_map_key(key_ev(TERMGFX_EV_KEY_DOWN, TERMGFX_KEY_INSERT), out);
	assert(n == 2 && out[0] == Input::Keys::INSERT && out[1] == Input::Keys::KP0);
	n = input_term_map_key(key_ev(TERMGFX_EV_KEY_DOWN, TERMGFX_KEY_DELETE), out);
	assert(n == 2 && out[0] == Input::Keys::DEL && out[1] == Input::Keys::KP_PERIOD);

	/* KP5 (drop Effect): no plain-key twin, unlike the others above. */
	n = input_term_map_key(key_ev(TERMGFX_EV_KEY_DOWN, TERMGFX_KEY_KP5), out);
	assert(n == 1 && out[0] == Input::Keys::KP5);

	/* Ctrl+letter: ascii is 0 (termgfx's own convention), but keycode is
	 * still the plain letter -- must still map (dropping the Ctrl accent,
	 * out of scope for this task). */
	n = input_term_map_key(key_ev(TERMGFX_EV_KEY_DOWN, 'd', 0, TERMGFX_MOD_CTRL), out);
	assert(n == 1 && out[0] == Input::Keys::D);

	/* Non-key events and unmapped keys: no mapping. */
	{
		termgfx_input_event_t mv;
		mv.type = TERMGFX_EV_MOUSE_MOVE;
		mv.x = mv.y = mv.button = mv.wheel = mv.keycode = mv.ascii = mv.mods = 0;
		assert(input_term_map_key(mv, out) == 0);
	}
	/* F1..F9 must NOT be forwarded (security -- door-hotkey audit): EasyRPG's
	 * default desktop button table binds them to engine functions (F1 ->
	 * SETTINGS_MENU, F9 -> DEBUG_MENU, etc.), never game controls, so
	 * input_term_map_key() must report no mapping for any of them. */
	for (int f = TERMGFX_KEY_F1; f <= TERMGFX_KEY_F9; f++) {
		n = input_term_map_key(key_ev(TERMGFX_EV_KEY_DOWN, f), out);
		assert(n == 0);
	}

	/* --- Layer 2: input_term_pump() through termgfx_termio's real parser --- */
	std::bitset<Input::Keys::KEYS_COUNT> keys;

	/* Legacy CSI arrow (no true release on the wire): pressed for exactly
	 * the poll it arrived in, released the next. */
	termgfx_termio_test_feed("\x1b[A", 3);
	input_term_pump(keys);
	assert(keys[Input::Keys::UP]);
	input_term_pump(keys);   /* no new events queued */
	assert(!keys[Input::Keys::UP]);

	/* Legacy CSI PAGEUP (ESC[5~): drives PGUP + KP9 (Yume Nikki wake) both,
	 * same tap timing as above. */
	termgfx_termio_test_feed("\x1b[5~", 4);
	input_term_pump(keys);
	assert(keys[Input::Keys::PGUP] && keys[Input::Keys::KP9]);
	input_term_pump(keys);
	assert(!keys[Input::Keys::PGUP] && !keys[Input::Keys::KP9]);

	/* Reserved door hotkeys (Ctrl-S stats, bare 'q' quit) never reach the
	 * FIFO -- termgfx swallows them before next_event() -- so they must
	 * leave no trace in EasyRPG's key state either. */
	drain_termgfx_events();
	termgfx_termio_test_feed("\x13", 1);   /* Ctrl-S */
	input_term_pump(keys);
	assert(keys.none());
	termgfx_termio_test_feed("q", 1);
	input_term_pump(keys);
	assert(keys.none());

	/* Kitty CSI-u: negotiate on, then a press held across several polls
	 * (simulating the terminal's own key-repeat, termgfx_termio.c's
	 * "ev != 3" -> down for both press(1) and repeat(2)) must stay
	 * pressed the WHOLE time -- not just the one poll a legacy tap gets. */
	termgfx_termio_test_feed("\x1b[?u", 4);   /* enable kitty */
	drain_termgfx_events();
	termgfx_termio_test_feed("\x1b[57427u", 8);   /* KP5 press, mod=1 ev=1 default */
	input_term_pump(keys);
	assert(keys[Input::Keys::KP5]);
	for (int i = 0; i < 3; i++) {
		/* No new event this poll (simulates the gap between repeats) --
		 * the key still releases per the one-poll-tap rule: termgfx gave
		 * us no way to tell "still held, repeat just hasn't arrived yet"
		 * from "released" without a repeat event actually landing. */
		input_term_pump(keys);
	}
	assert(!keys[Input::Keys::KP5]);

	/* Re-press, then a GENUINE kitty release (event type 3): must clear
	 * immediately, on the very poll it arrives -- not wait for the
	 * next-poll tap timeout. */
	termgfx_termio_test_feed("\x1b[57427u", 8);
	input_term_pump(keys);
	assert(keys[Input::Keys::KP5]);
	termgfx_termio_test_feed("\x1b[57427;1:3u", 12);   /* release */
	input_term_pump(keys);
	assert(!keys[Input::Keys::KP5]);

	/* A kitty repeat arriving each poll keeps the key pressed continuously
	 * (this is what a real physical hold looks like on this path). */
	for (int i = 0; i < 4; i++) {
		termgfx_termio_test_feed("\x1b[57427u", 8);   /* press/repeat, both down=true */
		input_term_pump(keys);
		assert(keys[Input::Keys::KP5]);
	}
	input_term_pump(keys);   /* repeats stop: releases like any other tap */
	assert(!keys[Input::Keys::KP5]);

	return 0;
}
