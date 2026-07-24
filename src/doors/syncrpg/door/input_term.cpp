/* SyncRPG -- termgfx keyboard FIFO -> EasyRPG Input::Keys (M2 / Task 5). See
 * input_term.h for the module's contract; this file is the mapping table +
 * the tap/hold release-synthesis pump. GPLv3+, like the EasyRPG tree.
 *
 * -- The mapping table --
 * Yume Nikki's own README ("Controls" section) documents:
 *   Arrow keys                 Move cursor, Move character
 *   ESC                        Open Menu, Cancel
 *   ENTER                      Confirm, Check environment
 *   Number Key 9               Pinch cheek (wake from the dream world)
 *   Number Key 1 or 3          Some Effects' animations/special actions
 *   Number Key 5               Drop the currently equipped Effect
 *   (also) Z or Spacebar in place of Enter; Insert or [X,C,V,B,N] in place
 *   of Cancel; the number-pad OR top-row number keys either work.
 * This lines up exactly with EasyRPG's own default desktop button table
 * (easyrpg/src/input_buttons_desktop.cpp): DECISION = Z/SPACE/RETURN,
 * CANCEL = X/C/V/B/N/ESCAPE/KP0, UP/DOWN/LEFT/RIGHT = the arrows (+WASD/HJKL,
 * which we get for free via the generic ASCII fallback below), and
 * N0..N9/KP0..KP9 both drive the game's own "Numbers" key-input check
 * (game_interpreter.cpp's KeyInputState::CheckInput(), which walks
 * Input::N0..N9 -- bound to BOTH the top-row and numpad Keys by that same
 * default table). So this table only needs to (a) turn TERMGFX_KEY_* nav
 * codes into their EasyRPG Keys::InputKey counterparts and (b) turn a
 * plain printable byte into its Keys::A..Z / N0..N9 / SPACE equivalent --
 * EasyRPG's button layer already does the Numbers/Decision/Cancel wiring.
 *
 * -- The NumLock-off numpad ambiguity --
 * termgfx's evdev decode (termgfx_termio.c's termgfx_evdev_edge()) reports a
 * physical numpad key with NumLock off as its NAVIGATION function, not a
 * digit: KP9->PAGEUP, KP1->END, KP3->PAGEDOWN, KP5->TERMGFX_KEY_KP5,
 * KP7->HOME, KP0->INSERT, KP-dot->DELETE. Kitty's CSI-u NumLock-off keypad
 * PUA codes decode to the very same TERMGFX_KEY_* values (termgfx_termio.c's
 * "u" case). That is indistinguishable, at this layer, from the dedicated
 * PageUp/End/PageDown/Home/Insert/Delete keys most keyboards also have --
 * termgfx's API is frozen, so this ambiguity isn't ours to resolve. Rather
 * than guess, we set BOTH the plain nav key (PGUP/ENDS/PGDN/HOME/INSERT/DEL)
 * and the matching numpad key (KP9/KP1/KP3/KP7/KP0/KP_PERIOD) from one
 * event: only one of the two is ever meaningful in a given game context
 * (Yume Nikki reads the numpad Buttons; EasyRPG's save/message-log scenes
 * read PAGE_UP/PAGE_DOWN), so setting both is harmless and covers whichever
 * the terminal actually meant. TERMGFX_KEY_KP5 has no separate "real key"
 * counterpart (it IS the numpad-5-center code), so it maps to Keys::KP5
 * alone.
 */
#include "input_term.h"
#include "help_term.h"   /* the door's help overlay intercepts F1 / any-key-to-close */
#include "video_term.h"  /* video_term_cycle_resolution() -- the F4 hotkey */

using Input::Keys::InputKey;

/* Printable-byte fallback: A-Z (case-folded), 0-9, space. Covers Yume
 * Nikki's Decision/Cancel letter keys (Z, Space, X/C/V/B/N) and the
 * top-row digit keys, plus -- for free -- EasyRPG's default WASD/HJKL
 * movement aliases and anything else a scene's own Keys::A.. binding
 * wants. Anything else (punctuation, etc.) is out of scope for this task;
 * returns Keys::NONE. */
static InputKey ascii_to_key(int c)
{
	if (c >= 'a' && c <= 'z')
		c -= ('a' - 'A');   /* fold to upper so one range check covers both */
	if (c >= 'A' && c <= 'Z')
		return static_cast<InputKey>(Input::Keys::A + (c - 'A'));
	if (c >= '0' && c <= '9')
		return static_cast<InputKey>(Input::Keys::N0 + (c - '0'));
	if (c == ' ')
		return Input::Keys::SPACE;
	return Input::Keys::NONE;
}

int input_term_map_key(const termgfx_input_event_t &ev, InputKey out[INPUT_TERM_MAX_KEYS])
{
	if (ev.type != TERMGFX_EV_KEY_DOWN && ev.type != TERMGFX_EV_KEY_UP)
		return 0;   /* mouse/wheel event: not this module's concern */

	switch (ev.keycode) {
	case TERMGFX_KEY_UP:        out[0] = Input::Keys::UP;        return 1;
	case TERMGFX_KEY_DOWN:      out[0] = Input::Keys::DOWN;      return 1;
	case TERMGFX_KEY_LEFT:      out[0] = Input::Keys::LEFT;      return 1;
	case TERMGFX_KEY_RIGHT:     out[0] = Input::Keys::RIGHT;     return 1;
	case TERMGFX_KEY_ENTER:     out[0] = Input::Keys::RETURN;    return 1;
	case TERMGFX_KEY_ESCAPE:    out[0] = Input::Keys::ESCAPE;    return 1;
	case TERMGFX_KEY_BACKSPACE: out[0] = Input::Keys::BACKSPACE; return 1;
	case TERMGFX_KEY_TAB:       out[0] = Input::Keys::TAB;       return 1;
	/* F1..F9 are DELIBERATELY not forwarded (security -- door-hotkey audit).
	 * Yume Nikki/RPG Maker 2000/2003 never bind game controls to function
	 * keys (see this file's own header doc), but EasyRPG's default desktop
	 * button table (input_buttons_desktop.cpp) does: F1=SETTINGS_MENU,
	 * F2=TOGGLE_FPS, F3=SHOW_LOG, F4=TOGGLE_FULLSCREEN, F5=TOGGLE_ZOOM,
	 * F7=TAKE_SCREENSHOT, F9=DEBUG_MENU (F10/F11 -> DEBUG_ABORT_EVENT/
	 * DEBUG_SAVE, F12 -> RESET don't even exist as TERMGFX_KEY_* codes).
	 * Forwarding them would hand a locked-down BBS user the engine's
	 * settings/debug UI and a game reset with no gameplay upside, so this
	 * table simply drops them -- same class of exposure as an unintended
	 * launcher hotkey.
	 *
	 * F1 (SETTINGS_MENU) was trialled and REJECTED: Scene_Settings carries far
	 * more than a BBS caller should reach -- video/audio/input settings, key
	 * rebinding, an engine hotkey list naming the F-keys above -- and it can
	 * write a config to a SYSTEM-WIDE profile directory, escaping the door's
	 * per-user sandbox entirely. If the door ever needs an about/version page
	 * of its own, it should draw one door-side (cf. syncretro/main.c's pause
	 * box) rather than open the engine's.
	 */
	/* NumLock-off numpad / dedicated-key ambiguity -- see file doc above:
	 * set both the plain nav key and Yume Nikki's numpad Effect/wake key. */
	case TERMGFX_KEY_HOME:      out[0] = Input::Keys::HOME; out[1] = Input::Keys::KP7; return 2;
	case TERMGFX_KEY_END:       out[0] = Input::Keys::ENDS; out[1] = Input::Keys::KP1; return 2;   /* Effect action 1/3 */
	case TERMGFX_KEY_PAGEUP:    out[0] = Input::Keys::PGUP; out[1] = Input::Keys::KP9; return 2;   /* wake ("pinch cheek") */
	case TERMGFX_KEY_PAGEDOWN:  out[0] = Input::Keys::PGDN; out[1] = Input::Keys::KP3; return 2;   /* Effect action 1/3 */
	case TERMGFX_KEY_INSERT:    out[0] = Input::Keys::INSERT; out[1] = Input::Keys::KP0; return 2;
	case TERMGFX_KEY_DELETE:    out[0] = Input::Keys::DEL; out[1] = Input::Keys::KP_PERIOD; return 2;
	case TERMGFX_KEY_KP5:       out[0] = Input::Keys::KP5; return 1;   /* drop Effect -- no plain-key twin */
	default:
		break;
	}

	if (ev.keycode < TERMGFX_KEY_FIRST) {
		/* Printable byte or Ctrl+letter (Ctrl+letter's ascii is 0, but its
		 * keycode is still the plain letter -- termgfx_termio.c's
		 * termgfx_key_byte() -- so keying off keycode, not ascii, covers both). */
		InputKey k = ascii_to_key(ev.keycode);
		if (k != Input::Keys::NONE) {
			out[0] = k;
			return 1;
		}
	}
	return 0;
}

/* Keys we ourselves set from a KEY_DOWN and must auto-release if nothing
 * reaffirms them by the next poll (see input_term.h's doc on the tap/hold
 * model). File-static, like termgfx_termio's own session state -- one
 * BaseUi_termgfx per process. */
static std::bitset<Input::Keys::KEYS_COUNT> g_pending;

void input_term_pump(std::bitset<Input::Keys::KEYS_COUNT> &keys)
{
	for (size_t i = 0; i < Input::Keys::KEYS_COUNT; i++) {
		if (g_pending[i]) {
			keys[i] = false;
			g_pending[i] = false;
		}
	}

	termgfx_input_event_t ev;
	InputKey mapped[INPUT_TERM_MAX_KEYS];
	while (termgfx_termio_next_event(&ev)) {
		/* The door's help overlay (help_term.h) sits in front of the engine:
		 * F1 raises it, and while it is up EVERY key press closes it and is
		 * swallowed, so the keystroke that dismisses the page cannot also act
		 * on the game behind it. Releases are ignored on both counts -- acting
		 * on the DOWN edge and then eating the matching UP is what keeps a
		 * single F1 tap from opening and instantly closing the page. */
		if (ev.type == TERMGFX_EV_KEY_DOWN) {
			if (help_term_active()) {
				help_term_dismiss();
				continue;
			}
			if (ev.keycode == TERMGFX_KEY_F1) {
				help_term_show();
				continue;
			}
			/* F4 -- door-owned resolution cycle (Original/Widescreen/Ultrawide).
			 * The door consumes it here rather than forwarding: EasyRPG's F4 is
			 * TOGGLE_FULLSCREEN (meaningless in a terminal), and all of F1..F9 are
			 * dropped by input_term_map_key() below anyway. See video_term.cpp. */
			if (ev.keycode == TERMGFX_KEY_F4) {
				video_term_cycle_resolution();
				continue;
			}
		} else if (ev.type == TERMGFX_EV_KEY_UP && help_term_active()) {
			continue;
		}

		int n = input_term_map_key(ev, mapped);
		if (n == 0)
			continue;
		bool down = (ev.type == TERMGFX_EV_KEY_DOWN);
		for (int i = 0; i < n; i++) {
			keys[mapped[i]] = down;
			g_pending[mapped[i]] = down;
		}
	}
}
