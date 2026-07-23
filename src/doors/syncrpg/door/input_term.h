/* SyncRPG -- termgfx keyboard FIFO -> EasyRPG Input::Keys (M2 / Task 5).
 *
 * termgfx_termio's input FIFO (termgfx_termio_next_event()) hands us discrete
 * TERMGFX_EV_KEY_DOWN/UP events; EasyRPG's Input layer instead polls a
 * continuous bool-per-key snapshot once a frame (BaseUi::keys, a
 * bitset<Input::Keys::KEYS_COUNT> -- see easyrpg/src/baseui.h /
 * easyrpg/src/input.cpp's Input::Update(), which reads
 * DisplayUi->GetKeyStates()). This module bridges the two: a pure mapping
 * function the unit test drives directly, and a pump that also owns the
 * tap/hold release-synthesis state (see input_term.cpp's file doc). GPLv3+,
 * like the EasyRPG tree this links into.
 */
#ifndef SYNCRPG_INPUT_TERM_H_
#define SYNCRPG_INPUT_TERM_H_

#include <bitset>

#include "keys.h"

extern "C" {
#include "termgfx_termio.h"
}

/* A single termgfx key event can drive more than one EasyRPG raw key -- the
 * evdev/kitty NumLock-off numpad codes double as dedicated navigation keys
 * (TERMGFX_KEY_PAGEUP is both a physical PageUp key AND numpad-9-without-
 * NumLock; see input_term.cpp's table), so we set both the nav key and the
 * numpad key bit rather than guess which the terminal meant. */
#define INPUT_TERM_MAX_KEYS 2

/* Pure mapping: given one termgfx KEY_DOWN/KEY_UP event, fill out[] with the
 * Input::Keys::InputKey code(s) it drives and return how many (0 if the
 * event isn't a key event, or is a key this table doesn't route). No side
 * effects and no static state -- the unit test calls this directly with
 * synthetic events. */
int input_term_map_key(const termgfx_input_event_t &ev,
	Input::Keys::InputKey out[INPUT_TERM_MAX_KEYS]);

/* Drain every event termgfx_termio_next_event() has queued and apply it to
 * `keys` (BaseUi::keys). Call once per ProcessEvents() poll, after
 * termgfx_termio_pump() has had a chance to fill the FIFO.
 *
 * termgfx's legacy/CSI byte path (termgfx_termio.c's termgfx_key_press()) emits
 * KEY_DOWN with no matching KEY_UP ever -- a plain terminal byte carries no
 * release -- while its kitty CSI-u and SyncTERM evdev paths emit true
 * KEY_DOWN/KEY_UP pairs (plus, for a terminal that reports key-repeat,
 * periodic KEY_DOWN "repeats" while genuinely held). Since EasyRPG samples
 * `keys` as continuous held-state once per frame, a key this function sets
 * from a KEY_DOWN is released again at the START of the FOLLOWING call
 * unless a fresh event re-affirms it first (another KEY_DOWN/repeat, or the
 * matching KEY_UP, which releases it immediately instead). That gives a
 * legacy tap exactly one frame of visibility -- the "one-poll key tap" the
 * task brief asks for -- while a genuine kitty/evdev hold keeps reading
 * pressed for as long as repeats (or the still-open press) keep arriving.
 * Mirrors syncscumm's mouse-tap synthesis, adapted for a continuous rather
 * than edge-triggered consumer. */
void input_term_pump(std::bitset<Input::Keys::KEYS_COUNT> &keys);

#endif /* SYNCRPG_INPUT_TERM_H_ */
