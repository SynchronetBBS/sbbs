/* syncretro_input.c -- BBS socket decode -> a cached RetroPad state.
 *
 * sr_input_pump() is the retro_input_poll body: a NON-BLOCKING drain of the
 * client socket, decoding ESC/CSI/APC sequences into a RetroPad bitfield and
 * routing terminal probe replies to syncretro_io.c's geometry setters.
 * sr_pad_get() is the retro_input_state body -- it just reads the cache. See
 * DESIGN.md sec 7.
 *
 * THE KEY-UP PROBLEM. A RetroPad is a HELD-STATE device: the core asks "is UP
 * down right now?" every frame. A plain terminal never says a key came up -- it
 * sends bytes on press and nothing on release. So this module supports three
 * input paths, in descending order of fidelity (ported from
 * ../syncduke/syncduke_input.c, which solved the same problem for Build's
 * scancode queue):
 *
 *   evdev (SyncTERM)  CTDA cap 8 -> we enable `CSI = 1 h` physical key reports
 *                     plus `CSI = 2 h` translated-key suppression. Keys then
 *                     arrive as `CSI = <evdev-code> K` (press) / `k` (release):
 *                     true key-up, and the identity is a LAYOUT-INDEPENDENT
 *                     physical code, so WASD works on AZERTY and Dvorak.
 *
 *   kitty             The terminal answered our `CSI ? u` progressive-enhancement
 *                     query, so we push flags and keys arrive as CSI-u events
 *                     carrying an explicit press/repeat/release type. Also true
 *                     key-up, but keyed by layout-dependent codepoint.
 *
 *   byte path         Neither: a press sets the pad bit and arms a short
 *                     auto-release timer (SR_HOLD_MS), which auto-repeat keeps
 *                     refreshing while the key is physically held. This is a
 *                     genuine approximation -- a tap registers for SR_HOLD_MS,
 *                     and the terminal's own repeat DELAY means a held direction
 *                     stutters once before it flows. It is the best a terminal
 *                     with no key-up can do, and it is why the two native paths
 *                     above are worth the negotiation.
 *
 * The two native paths are mutually exclusive in practice (SyncTERM doesn't
 * speak kitty; kitty terminals don't advertise CTDA cap 8) and the negotiation
 * guards enforce it regardless.
 *
 * The negotiation itself -- the enable/restore sequences, the evdev-wins
 * precedence, the settle window, the kitty CSI-u decode and the evdev keycode
 * tables -- lives in ../termgfx/keymode.h, shared with the sibling doors. What
 * stays here is the only per-game part: the map from a key to a RetroPad button.
 *
 * TWO CONTROLLERS. The Intellivision has two hand controllers, and the door
 * drives BOTH at once so a solo player uses whichever the cart reads -- no swap
 * dance -- and two people can play on one keyboard. Player 1 is the WASD / Z X C
 * / number-row key group (controller 0), player 2 the arrows / , . / / numeric
 * keypad group (controller 1). Player 2's arrows and numpad have no ASCII form,
 * so they arrive only on the CSI / evdev / kitty paths (on a plain byte terminal
 * the numpad is indistinguishable from the number row and falls back to player
 * 1). All state below is per controller; Tab (g_swap) remaps which core port
 * reads which player's keys, applied at read time in sr_pad_get().
 *
 * Whichever mode is negotiated is UNDONE on the way out (`CSI = 2 l` / `CSI = 1 l`
 * for evdev, `CSI < u` to pop the kitty flags), so the BBS gets its terminal back
 * as it lent it. Same as the sibling doors -- with one difference: they restore
 * only on a normal exit, because their hangup path _exit()s on the assumption the
 * socket is already dead. Here sr_door_hangup() runs sr_io_leave() first (bounded
 * drain), so a READ-side hangup -- where the write direction may still be open --
 * still hands the terminal back instead of leaving it in physical-key mode.
 *
 * The socket read, the monotonic clock, and the keytrace-switch existence check
 * go through syncretro_plat.h / xpdev, so this file carries no #ifdef: recv() on
 * a non-blocking Winsock SOCKET, read() on a POSIX fd -- the seam hides which.
 */
#include "syncretro.h"
#include "syncretro_plat.h"
#include "libretro.h"    /* RETRO_DEVICE_* / RETRO_DEVICE_ID_JOYPAD_* */

#include <limits.h>      /* PATH_MAX */
#include <stdarg.h>     /* sr_trace */
#include <stdio.h>       /* SYNCRETRO_KEYTRACE */
#include <stdlib.h>      /* getenv */
#include <string.h>

#include "dirwrap.h"     /* xpdev: fexist() -- the keytrace.on switch check */
#include "caps.h"        /* termgfx: termgfx_caps_parse_jxl */
#include "audio.h"       /* termgfx: termgfx_audio_parse_caps */
#include "keymode.h"     /* termgfx: key-mode negotiation + kitty/evdev decode */
#include "syncretro_binds.h"
#include "syncretro_profile.h"
#include "syncretro_keypad.h"
#include "syncretro_audio.h"

#ifndef PATH_MAX
#define PATH_MAX 4096   /* MSVC's <limits.h> has no PATH_MAX (it uses _MAX_PATH) */
#endif

/* Byte-path auto-release window. Long enough that a tap registers for a frame or
 * three at 60 fps, short enough that a released key doesn't drift. Auto-repeat
 * refreshes it, so a held key stays down. */
#define SR_HOLD_MS 250

/* Cached RetroPad. Index by RETRO_DEVICE_ID_JOYPAD_*. */
#define SR_PAD_IDS (RETRO_DEVICE_ID_JOYPAD_R3 + 1)

/* Two controllers: the door drives BOTH Intellivision hand controllers at once,
 * so a solo player uses whichever the cart reads (no swap needed) and two
 * players can share one keyboard. Player 1 is the WASD / Z X C / number-row key
 * group, player 2 the arrows / , . / / numeric keypad group -- see
 * syncretro_binds.c and M2_INPUT.md. Everything below is indexed by the LOGICAL
 * controller (0 = player 1, 1 = player 2); Tab (g_swap) only remaps which core
 * port reads which logical controller, at read time in sr_pad_get(). */
#define SR_PADS 2

static int16_t  g_joypad[SR_PADS][SR_PAD_IDS];
static uint32_t g_expire_ms[SR_PADS][SR_PAD_IDS]; /* byte path only: 0 = no timer */
static int      g_swap;   /* Tab: 1 swaps which core port each player drives */
static int      g_quit;

#define sr_in_now_ms()   sr_plat_now_ms()

int sr_input_quit_requested(void) { return g_quit; }

/* --- pad edges -------------------------------------------------------------- */

static int sr_pad_bad(int port, int id)
{
	return port < 0 || port >= SR_PADS || id < 0 || id >= SR_PAD_IDS;
}

/* Native paths (kitty/evdev): held until the explicit key-up, so no timer. */
static void sr_pad_press(int port, int id)
{
	if (sr_pad_bad(port, id))
		return;
	g_joypad[port][id]    = 1;
	g_expire_ms[port][id] = 0;
}

static void sr_pad_release(int port, int id)
{
	if (sr_pad_bad(port, id))
		return;
	g_joypad[port][id]    = 0;
	g_expire_ms[port][id] = 0;
}

/* Byte path: press + arm the auto-release timer (a repeat byte refreshes it). */
static void sr_pad_tap(int port, int id)
{
	if (sr_pad_bad(port, id))
		return;
	g_joypad[port][id]    = 1;
	g_expire_ms[port][id] = sr_in_now_ms() + SR_HOLD_MS;
	if (g_expire_ms[port][id] == 0)
		g_expire_ms[port][id] = 1;   /* 0 is the "no timer" sentinel */
}

static void sr_pad_expire(void)
{
	uint32_t now = sr_in_now_ms();
	int      port, id;

	for (port = 0; port < SR_PADS; port++)
		for (id = 0; id < SR_PAD_IDS; id++)
			if (g_expire_ms[port][id] != 0 && (int32_t)(now - g_expire_ms[port][id]) >= 0) {
				g_joypad[port][id]    = 0;
				g_expire_ms[port][id] = 0;
			}
	sr_keypad_expire(now);
}

/* --- disc sweep (paddle carts) ----------------------------------------------
 *
 * The Intellivision disc is a 16-position ring, and a paddle game like Brickout!
 * moves its paddle ONE STEP per CHANGE of the disc's value, in the direction of
 * that value's horizontal component. A held cardinal direction is a constant
 * value, so the paddle takes one step and stops dead -- which is what a keyboard
 * player sees, because four keys cannot rotate a disc.
 *
 * So for carts listed in [disc] rotate, a held Left/Right does not pin the disc:
 * it ALTERNATES between two adjacent detents on that side (0 deg / 22.5 deg for
 * right, 180 deg / 202.5 deg for left). Every toggle is a fresh disc value whose
 * horizontal component points the same way, so the paddle steps again and again
 * and the travel is smooth.
 *
 * Measured against FreeIntv with Brickout! (a headless probe driving the core
 * directly): over 120 frames from the same start, holding the d-pad travels 2 px,
 * pulsing the d-pad bit travels 57 px, and alternating two adjacent detents
 * travels 115 px. One toggle per 4 frames beats both 2 and 6 -- toggle faster and
 * the core samples past the change, slower and you simply get fewer steps.
 *
 * The disc rides the LEFT analog stick, which FreeIntv reads as the disc and
 * which this door otherwise leaves at rest (the RIGHT stick carries the keypad).
 * While sweeping, the d-pad's own Left/Right bits are withheld: the core would
 * fold them back into the disc and fight the sweep. */
#define SR_DISC_TOGGLE_FRAMES 4

/* cos/sin * 32000 (just inside INT16_MAX) at 0, 22.5, 180 and 202.5 degrees.
 * Integer literals so the door pulls in no libm for four constants. */
static const int16_t g_disc_vec[2][2][2] = {   /* [left?][phase][x,y] */
	{ {  32000,      0 }, {  29565,  12246 } },   /* right: 0 deg, 22.5 deg */
	{ { -32000,      0 }, { -29565, -12246 } }    /* left: 180 deg, 202.5 deg */
};

static int           g_disc_phase[SR_PADS]; /* which of the two detents is asserted */
static int           g_disc_dir[SR_PADS];   /* -1 left, +1 right, 0 not sweeping */

/* Once per frame, per controller, from sr_input_pump() (the core's input_poll). */
static void sr_disc_tick_port(int port)
{
	static unsigned frames[SR_PADS];
	int             left, right;

	if (!sr_config_disc_rotate()) {
		g_disc_dir[port] = 0;
		return;
	}
	left  = g_joypad[port][RETRO_DEVICE_ID_JOYPAD_LEFT];
	right = g_joypad[port][RETRO_DEVICE_ID_JOYPAD_RIGHT];

	if (left == right) {          /* neither, or both: no direction to sweep */
		g_disc_dir[port] = 0;
		return;
	}
	if (g_disc_dir[port] == 0)
		frames[port] = 0;         /* a fresh press steps on its very first frame */
	g_disc_dir[port] = right ? 1 : -1;
	if ((frames[port]++ % SR_DISC_TOGGLE_FRAMES) == 0)
		g_disc_phase[port] ^= 1;
}

static void sr_disc_tick(void)
{
	int port;

	for (port = 0; port < SR_PADS; port++)
		sr_disc_tick_port(port);
}

int16_t sr_pad_get(unsigned port, unsigned device, unsigned index, unsigned id)
{
	unsigned p;

	if (port >= SR_PADS)
		return 0;
	p = port ^ (unsigned)g_swap;   /* which logical controller feeds this core port */

	/* The analog stick is the Intellivision's disc and keypad. On a gamepad
	 * console it is DEAD -- a core that polls it must read a centred stick, or a
	 * game that reads both would see a phantom deflection. */
	if (device == RETRO_DEVICE_ANALOG) {
		if (!sr_profile_analog())
			return 0;
		if (index == RETRO_DEVICE_INDEX_ANALOG_LEFT) {
			if (g_disc_dir[p] == 0)
				return 0;         /* not sweeping: leave the disc alone */
			if (id == RETRO_DEVICE_ID_ANALOG_X)
				return g_disc_vec[g_disc_dir[p] < 0][g_disc_phase[p]][0];
			if (id == RETRO_DEVICE_ID_ANALOG_Y)
				return g_disc_vec[g_disc_dir[p] < 0][g_disc_phase[p]][1];
			return 0;
		}
		return sr_keypad_analog((int)p, index, id);   /* right stick = keypad digits */
	}
	if (device != RETRO_DEVICE_JOYPAD)
		return 0;
	if (id >= SR_PAD_IDS)
		return 0;
	/* While the disc is being swept, the d-pad must not also claim it. (Only the
	 * Intellivision ever sweeps; on a pad profile g_disc_dir stays 0.) */
	if (sr_profile_analog() && g_disc_dir[p] != 0
	    && (id == RETRO_DEVICE_ID_JOYPAD_LEFT || id == RETRO_DEVICE_ID_JOYPAD_RIGHT))
		return 0;
	return g_joypad[p][id];
}

/* --- the key map: one table, shared by all three paths ---------------------- */

/* The bindings themselves live in syncretro_binds.c; this is only the glue that
 * turns a resolved action into pad / keypad / door state.
 *
 * RetroPad is an abstract SNES-shaped joypad and the core decides what its
 * buttons mean. FreeIntv puts the three action buttons on A/B/Y, drives the
 * 16-way disc from the d-pad, and reaches all twelve keypad keys through the
 * right analog stick plus R3/L3/L2/R2 -- so a bare keyboard plays the whole
 * console. See M2_INPUT.md. */

static int g_action;      /* pending SR_DOOR_*, consumed by sr_input_take_action() */
static int g_suspended;   /* a door screen is up: game keys are swallowed */
static int g_anykey;      /* a key arrived while suspended (bound or not) */

int sr_input_take_action(void)
{
	int a = g_action;

	g_action = SR_DOOR_NONE;
	return a;
}

void sr_input_set_suspended(int on)
{
	g_suspended = on;
	g_anykey    = 0;   /* the keystroke that OPENED the screen must not close it */
}

int sr_input_take_anykey(void)
{
	int a = g_anykey;

	g_anykey = 0;
	return a;
}

static void sr_f4(void);   /* defined with the CSI dispatch; the evdev path needs it too */

static void sr_door_action(int id)
{
	if (id == SR_DOOR_QUIT)
		g_quit = 1;        /* the existing, separately-polled latch */
	else
		g_action = id;
}

/* Tab: swap which core port each player's keys drive, then clear both
 * controllers so nothing that was held before the remap sticks after it. */
static void sr_do_swap(void)
{
	g_swap ^= 1;
	sr_input_release_all();
}

/* Resolve one key press/release into pad, keypad or door state. `down` is 1 for
 * a press or repeat, 0 for a release; `tap` selects the byte path's
 * auto-release dwell over a held press. Door actions fire on the press edge.
 *
 * While a door screen is up, a game key is swallowed rather than pressed --
 * otherwise a key struck behind the pause screen is still held on resume -- but
 * it is remembered, so "press any key to return" is true. */
static void sr_key_apply(int c, int down, int tap)
{
	int      id, port;
	sr_act_t act = sr_bind_lookup(sr_bind_fold(c), &id, &port);

	/* WHAT THE KEY RESOLVED TO. The trace logged the byte arriving and a
	 * once-a-second direction sample, and neither can answer the question a
	 * player actually asks -- "I pressed X and nothing happened" -- because a
	 * button tap is long gone by the next sample. This line closes that gap. */
	sr_trace("key %-4s c=0x%02x act=%d id=%d port=%d%s",
	         down ? "down" : "up", c, (int)act, id, port, tap ? " (tap)" : "");

	if (act == SR_ACT_NONE) {
		/* Unbound: it must never reach the core. But a player pressing a key
		 * that does nothing is asking "what ARE the keys?", so answer -- the
		 * legend costs a screen and a keystroke to dismiss.
		 *
		 * Printable keys only. CSI sequences (arrows, the DSR pacing replies)
		 * never arrive here, but a bare C0 byte from the terminal can, and that
		 * is noise rather than a question. */
		if (down && c >= 0x20 && c < 0x7f) {
			if (g_suspended)
				g_anykey = 1;              /* a screen is up: any key returns */
			else
				sr_door_action(SR_DOOR_HELP);
		}
		return;
	}

	if (act == SR_ACT_DOOR) {
		if (down)
			sr_door_action(id);
		return;
	}
	if (act == SR_ACT_SWAP) {
		/* Behind a door screen Tab just dismisses it, like any other key;
		 * otherwise it swaps the two controllers. Acts on the press edge. */
		if (down) {
			if (g_suspended)
				g_anykey = 1;
			else
				sr_do_swap();
		}
		return;
	}
	if (g_suspended) {
		if (down)
			g_anykey = 1;
		return;
	}
	switch (act) {
		case SR_ACT_PAD:
			if (!down)
				sr_pad_release(port, id);
			else if (tap)
				sr_pad_tap(port, id);
			else
				sr_pad_press(port, id);
			return;
		case SR_ACT_DIGIT:
			if (!down)
				sr_keypad_release(port, id);
			else if (tap)
				sr_keypad_tap(port, id, sr_in_now_ms());
			else
				sr_keypad_press(port, id);
			return;
		default:
			return;
	}
}

/* A key edge from a native path (evdev / kitty): a real key-up is coming. */
static void sr_key_edge(int c, int down) { sr_key_apply(c, down, 0); }

/* A key byte from the byte path: no key-up is coming, so arm the dwell
 * (auto-repeat refreshes it while the key is physically held). */
static void sr_key_byte(int c) { sr_key_apply(c, 1, 1); }

void sr_input_release_all(void)
{
	int port, id;

	for (port = 0; port < SR_PADS; port++)
		for (id = 0; id < SR_PAD_IDS; id++) {
			g_joypad[port][id]    = 0;
			g_expire_ms[port][id] = 0;
		}
	sr_keypad_release_all();
}

/* CSI final byte for an arrow key -> RetroPad d-pad direction, or -1. */
static int sr_arrow_button(char fin)
{
	switch (fin) {
		case 'A': return RETRO_DEVICE_ID_JOYPAD_UP;
		case 'B': return RETRO_DEVICE_ID_JOYPAD_DOWN;
		case 'C': return RETRO_DEVICE_ID_JOYPAD_RIGHT;
		case 'D': return RETRO_DEVICE_ID_JOYPAD_LEFT;
		default:  return -1;
	}
}

/* --- byte state machine ------------------------------------------------------ */

static enum { SR_P_NORMAL, SR_P_ESC, SR_P_CSI, SR_P_APC, SR_P_APC_ESC } pstate;

static char              csi_intro; /* '[' or 'O' */
static char              csi_par[40]; /* parameter bytes between intro and final */
static int               csi_len;
static unsigned          apc_len;  /* bytes swallowed in the current string seq */

static int               g_probe_replied;
static int               g_is_syncterm;
static int               g_have_sixel;
static int               g_jxl_supported;

static termgfx_keymode_t g_km;     /* which key mode is in force (termgfx) */
static unsigned          g_evdev_mods; /* held modifiers, TERMGFX_MOD_* */

int sr_input_is_syncterm(void) { return g_is_syncterm; }
int sr_input_has_sixel(void)   { return g_have_sixel; }
int sr_input_probe_replied(void) { return g_probe_replied; }

const char *sr_input_keymode_name(void)
{
	if (termgfx_keymode_evdev_active(&g_km))
		return "evdev";
	if (termgfx_keymode_kitty_active(&g_km))
		return "kitty";
	return "bytes";
}

/* Parse up to `max` ';'-separated decimal params from csi_par; return the count.
 * A leading non-digit marker byte (SGR mouse's '<', DA1's '='/'?') is skipped. */
static int sr_csi_params(int *out, int max)
{
	int n = 0, i, have = 0, v = 0;

	for (i = 0; i <= csi_len && n < max; i++) {
		char c = (i < csi_len) ? csi_par[i] : ';';   /* sentinel terminator */

		if (c >= '0' && c <= '9') {
			v = v * 10 + (c - '0');
			have = 1;
		} else if (c == ';') {
			if (have)
				out[n++] = v;
			v = 0;
			have = 0;
		}
	}
	return n;
}

/* --- evdev (SyncTERM physical key reports) ---------------------------------- */

/* Linux evdev codes for the keys with no ASCII form that we still map (the
 * modifiers are classified by termgfx_evdev_modifier(), not named here). */
#define SR_EVDEV_UP    103
#define SR_EVDEV_LEFT  105
#define SR_EVDEV_RIGHT 106
#define SR_EVDEV_DOWN  108

/* --- player 2's keypad (the numeric keypad, native paths only) -------------
 *
 * Only SyncTERM's evdev reports and the kitty keyboard protocol distinguish the
 * numeric keypad from the number row (a plain byte terminal sends the same
 * "1".."0" for both, so there the numpad falls back to player 1's keypad). When
 * they do, the numpad drives controller 1's keypad: the analog digits through
 * the keypad module, 5/0/Clear/Enter as the same R3/L3/L2/R2 button bits player
 * 1 reaches with 5/0/Backspace/Enter. `key` is 0-9, or one of: */
#define SR_KP_CLEAR (-1)
#define SR_KP_ENTER (-2)

/* On a GAMEPAD console there is no keypad to drive, and routing the numpad into
 * the Intellivision's would be nonsense. It becomes what a player actually
 * reaches for: a second d-pad. 8/4/6/2 are the directions (and the diagonals
 * press both, as a real d-pad can), numpad Enter is Start, 0 is Select, 5 is the
 * dead centre it looks like. All on the player's own controller. */
static void sr_numpad_pad(int key, int down)
{
	int a = -1, b = -1;
	int port = sr_profile_arrow_port();   /* 0 on a gamepad: the player's own */

	switch (key) {
		case 8: a = RETRO_DEVICE_ID_JOYPAD_UP;    break;
		case 2: a = RETRO_DEVICE_ID_JOYPAD_DOWN;  break;
		case 4: a = RETRO_DEVICE_ID_JOYPAD_LEFT;  break;
		case 6: a = RETRO_DEVICE_ID_JOYPAD_RIGHT; break;
		case 7: a = RETRO_DEVICE_ID_JOYPAD_UP;    b = RETRO_DEVICE_ID_JOYPAD_LEFT;  break;
		case 9: a = RETRO_DEVICE_ID_JOYPAD_UP;    b = RETRO_DEVICE_ID_JOYPAD_RIGHT; break;
		case 1: a = RETRO_DEVICE_ID_JOYPAD_DOWN;  b = RETRO_DEVICE_ID_JOYPAD_LEFT;  break;
		case 3: a = RETRO_DEVICE_ID_JOYPAD_DOWN;  b = RETRO_DEVICE_ID_JOYPAD_RIGHT; break;
		case 0:           a = RETRO_DEVICE_ID_JOYPAD_SELECT; break;
		case SR_KP_ENTER: a = RETRO_DEVICE_ID_JOYPAD_START;  break;
		default:          return;   /* 5, Clear: nothing to press */
	}
	if (down) {
		sr_pad_press(port, a);
		if (b >= 0)
			sr_pad_press(port, b);
	} else {
		sr_pad_release(port, a);
		if (b >= 0)
			sr_pad_release(port, b);
	}
}

static void sr_p2_keypad(int key, int down)
{
	int id = -1;

	if (g_suspended) {          /* a door screen is up: swallow, don't hold */
		if (down)
			g_anykey = 1;
		return;
	}
	/* The Intellivision keypad is the Intellivision's. Every other console gets a
	 * d-pad out of the numpad instead. */
	if (sr_profile() != SR_PROFILE_INTV) {
		sr_numpad_pad(key, down);
		return;
	}
	switch (key) {
		case 5:           id = RETRO_DEVICE_ID_JOYPAD_R3; break;
		case 0:           id = RETRO_DEVICE_ID_JOYPAD_L3; break;
		case SR_KP_CLEAR: id = RETRO_DEVICE_ID_JOYPAD_L2; break;
		case SR_KP_ENTER: id = RETRO_DEVICE_ID_JOYPAD_R2; break;
		default:
			if (key >= 1 && key <= 9) {
				if (down)
					sr_keypad_press(1, key);
				else
					sr_keypad_release(1, key);
			}
			return;
	}
	if (down)
		sr_pad_press(1, id);
	else
		sr_pad_release(1, id);
}

/* Linux evdev keycode -> numpad key (0-9 / SR_KP_CLEAR / SR_KP_ENTER), or
 * INT_MIN when `code` is not a numeric-keypad key. */
static int sr_evdev_numpad(int code)
{
	switch (code) {
		case 71: return 7;   case 72: return 8;   case 73: return 9;
		case 75: return 4;   case 76: return 5;   case 77: return 6;
		case 79: return 1;   case 80: return 2;   case 81: return 3;
		case 82: return 0;
		case 83: return SR_KP_CLEAR;   /* KP Del / . */
		case 96: return SR_KP_ENTER;   /* KP Enter */
		default: return INT_MIN;
	}
}

/* kitty functional codepoint -> numpad key, or INT_MIN. KP_0..KP_9 are the
 * contiguous 57399..57408; KP_DECIMAL (57409) is Clear, KP_ENTER (57414). */
/* NUMLOCK CHANGES WHAT KITTY SENDS, AND A GAMEPAD DOES NOT CARE.
 *
 * With NumLock ON the numeric keypad reports KP_0..KP_9 (57399-57408). With it
 * OFF the very same physical keys report the NAVIGATION functions instead --
 * KP_UP 57419, KP_LEFT 57417, KP_HOME 57423, and so on. This decoder knew only
 * the digits, so with NumLock off every numpad key fell through and was dropped:
 * on a kitty terminal the numpad simply did nothing, while SyncTERM's evdev path
 * (physical keycodes, which NumLock does not touch) worked fine. That is exactly
 * how it looked from the outside -- arrows drive the bike, the numpad does not.
 *
 * Both spellings fold to the same key here, deliberately. NumLock is a typing
 * distinction: it decides whether the key means "8" or "up". On a game controller
 * that key IS up -- it is the top of a d-pad, or the 8 of the Intellivision's
 * keypad -- and a player should not have to think about a lock light to steer.
 * (SyncConquer, which is a strategy game with a text UI, honors the distinction;
 * a console door has no reason to.) */
static int sr_kitty_numpad(int cp)
{
	if (cp >= 57399 && cp <= 57408)
		return cp - 57399;          /* NumLock ON: KP_0 .. KP_9 */
	if (cp == 57409)
		return SR_KP_CLEAR;         /* KP_DECIMAL */
	if (cp == 57414)
		return SR_KP_ENTER;

	/* NumLock OFF: the same keys, reported as what they navigate. */
	switch (cp) {
		case 57417: return 4;              /* KP_LEFT      */
		case 57418: return 6;              /* KP_RIGHT     */
		case 57419: return 8;              /* KP_UP        */
		case 57420: return 2;              /* KP_DOWN      */
		case 57421: return 9;              /* KP_PAGE_UP   */
		case 57422: return 3;              /* KP_PAGE_DOWN */
		case 57423: return 7;              /* KP_HOME      */
		case 57424: return 1;              /* KP_END       */
		case 57425: return 0;              /* KP_INSERT    */
		case 57426: return SR_KP_CLEAR;    /* KP_DELETE    */
		case 57427: return 5;              /* KP_BEGIN     */
	}
	return INT_MIN;
}

static void sr_evdev_edge(int code, int down)
{
	int  button = -1;
	int  mod    = termgfx_evdev_modifier(code);
	char c;

	/* Modifiers are their own keys on this path: track them, never map them.
	 * Both the left and right keycode of each pair count (termgfx). */
	if (mod != 0) {
		if (down)
			g_evdev_mods |= (unsigned)mod;
		else
			g_evdev_mods &= ~(unsigned)mod;
		return;
	}

	/* Drop the enable-time resync's press edges (termgfx_keymode.h). */
	if (down && termgfx_keymode_evdev_settling(&g_km, sr_in_now_ms()))
		return;

	/* F4 (evdev KEY_F4). On this path the key arrives as a physical keycode rather
	 * than an escape sequence, so it has to be caught here as well -- and only on
	 * the press, or the release would cycle the tier a second time. */
	if (code == 62) {
		if (down)
			sr_f4();
		return;
	}

	/* The numeric keypad drives PLAYER 2's keypad (a distinct physical key from
	 * the number row, which is player 1's). */
	{
		int kp = sr_evdev_numpad(code);

		if (kp != INT_MIN) {
			sr_p2_keypad(kp, down);
			return;
		}
	}

	switch (code) {
		case SR_EVDEV_UP:    button = RETRO_DEVICE_ID_JOYPAD_UP;    break;
		case SR_EVDEV_DOWN:  button = RETRO_DEVICE_ID_JOYPAD_DOWN;  break;
		case SR_EVDEV_LEFT:  button = RETRO_DEVICE_ID_JOYPAD_LEFT;  break;
		case SR_EVDEV_RIGHT: button = RETRO_DEVICE_ID_JOYPAD_RIGHT; break;
		default:
			/* Ask for the SHIFTED column when shift is held: the help key is '?',
			 * i.e. shift-'/', and evdev reports the physical key. Without this the
			 * door sees '/' -- unbound -- and help is unreachable on SyncTERM, the
			 * one terminal that negotiates this path. Shifted letters fold back to
			 * lowercase in sr_bind_lookup(); shifted digits become punctuation,
			 * which is unbound and so raises the key legend (sr_key_apply). */
			c = termgfx_evdev_ascii(code, (g_evdev_mods & TERMGFX_MOD_SHIFT) != 0);
			if (c == 0)
				return;
			if (g_evdev_mods & TERMGFX_MOD_CTRL) {
				/* Ctrl-<letter> becomes its C0 control code, and several of those
				 * codes are PAD bindings, not door actions: Backspace (Ctrl-H =
				 * 0x08) is keypad Clear, Tab (Ctrl-I = 0x09) is Select, Enter
				 * (Ctrl-J/M = 0x0a/0x0d) is keypad Enter. So both edges must be
				 * delivered -- send only the press and the pad bit sticks down
				 * forever. Door actions (Ctrl-Q/R) act on the press edge alone, so
				 * their release edge is harmless. Fold first: Ctrl-Shift-Q must
				 * still quit. */
				c = sr_bind_fold(c);
				if (c >= 'a' && c <= 'z')
					sr_key_edge(c - 'a' + 1, down);
				return;
			}
			sr_key_edge(c, down);
			return;
	}
	if (button < 0)
		return;
	/* Which controller the arrows drive is the PROFILE's call: the Intellivision
	 * runs both hand controllers at once, so they are player 2's disc (port 1);
	 * on a gamepad console there is one player at one keyboard and they are HIS
	 * d-pad (port 0). This is the evdev path -- the one SyncTERM negotiates -- and
	 * it hardcoded port 1, so arrows were dead on the NES while WASD worked.
	 *
	 * While a door screen is up, swallow the arrow rather than hold it -- same
	 * rule sr_key_apply() enforces for every other game key. A press is remembered
	 * so "press any key to return" is true; a release does nothing. */
	if (g_suspended) {
		if (down)
			g_anykey = 1;
		return;
	}
	if (down)
		sr_pad_press(sr_profile_arrow_port(), button);
	else
		sr_pad_release(sr_profile_arrow_port(), button);
}

/* `CSI = Pk[;Pk...] K|k` -- one sequence may carry several edges at once.
 * sr_csi_params() skips the leading '=' marker, so it yields just the codes. */
static void sr_evdev_report(int down)
{
	int codes[16], n, i;

	n = sr_csi_params(codes, 16);
	for (i = 0; i < n; i++)
		sr_evdev_edge(codes[i], down);
}

/* --- CSI dispatch ----------------------------------------------------------- */

/* Is the escape sequence we are holding a key PRESS, or the same key coming back
 * up (or auto-repeating)?
 *
 * With the kitty protocol negotiated we asked for event types, so F4 arrives
 * TWICE per press: bare "CSI S" going down, "CSI 1;1:3S" coming up (the ":<n>"
 * sub-parameter is the event -- 1 press, 2 repeat, 3 release). A tier cycle that
 * fires on both steps two tiers per keypress and looks like it is skipping them.
 * SyncConquer hit exactly this and fixed it the same way (5ff3d05955).
 *
 * A legacy terminal sends no event sub-parameter at all, and termgfx's decoder
 * reports that as a press -- which it is. */
static int sr_key_is_press(void)
{
	int mod, ev = 1;

	if (csi_len > 0)
		(void)termgfx_kitty_parse(csi_par, csi_len, &mod, &ev);
	return ev == 1;
}

/* F4 -- the render-tier cycle. It has no byte form, so it never touches the bind
 * table; it arrives as one of four different escape sequences depending on what
 * the terminal thinks it is, and (on SyncTERM) as a physical-key report instead.
 * Every spelling has to land in the same place. */
static void sr_f4(void)
{
	sr_door_action(SR_DOOR_TIER);
}

static void sr_csi_final(char fin)
{
	int p[16], np;

	switch (fin) {
		case 'S':   /* F4: SS3 "ESC O S" (VT100 PF4 / xterm), and the same final byte
		             * under xterm's modified-key form and kitty's ("CSI 1;m:ev S").
		             * No terminal SENDS a CSI S (scroll-up) as input, so this is
		             * unambiguous. */
			if (!sr_key_is_press())
				return;                 /* a repeat or a release -- see sr_key_is_press */
			sr_f4();
			return;

		case '~':   /* VT220-style function keys: F4 is ESC[14~ (and, under kitty,
		             * "ESC[14;1:3~" on the way back up). */
			np = sr_csi_params(p, 2);
			if (np >= 1 && p[0] == 14 && sr_key_is_press())
				sr_f4();
			return;

		case 't':   /* window-op reply: ESC[4;h;wt = exact text-area size in pixels */
			np = sr_csi_params(p, 4);
			if (np >= 3 && p[0] == 4)
				sr_io_set_canvas(p[2], p[1]);   /* p[1]=height px, p[2]=width px */
			return;

		case 'R':   /* ESC[rows;colsR: cursor-position report */
			np = sr_csi_params(p, 2);
			if (sr_io_take_grid_probe()) {
				/* The reply to the startup 999;999 grid probe -- NOT a pace-ack:
				 * no present()-sent frame exists to retire. The one-shot flag has
				 * already been consumed, so a malformed reply here (np < 2) just
				 * keeps the default grid and the NEXT R routes to the ack below. */
				if (np >= 2)
					sr_io_set_grid(p[0], p[1]);   /* rows, cols */
				return;
			}
			sr_io_pace_ack();
			return;

		case 'c':   /* device-attributes reply: DA1 (ESC[c) / CTDA (ESC[<c) */
			g_probe_replied = 1;
			if (csi_len > 0 && (csi_par[0] == '<' || csi_par[0] == '='))
				g_is_syncterm = 1;   /* '<'/'=' marker: SyncTERM-flavored reply */
			np = sr_csi_params(p, 16);
			{
				int k;

				for (k = 0; k < np; k++) {
					if (p[k] == 4)
						g_have_sixel = 1;   /* DA1 param 4 / CTDA cap 4 = sixel */
					/* CTDA cap 8 = physical key (evdev) reports. Prefer them over
					 * kitty (a SyncTERM that advertises this doesn't speak kitty):
					 * enable the reports and suppress the translated byte stream,
					 * so each key arrives exactly once, as an edge. */
					if (p[k] == 8 && csi_par[0] == '<') {
						char   seq[TERMGFX_KEYMODE_SEQ_MAX];
						size_t sn = termgfx_keymode_enable_evdev(&g_km, seq, sizeof seq,
						                                         sr_in_now_ms());

						if (sn > 0) {
							sr_out_put(seq, sn);
							sr_io_out_flush();
						}
					}
				}
			}
			/* CTerm >= 1.329: stream game audio inline via A;LoadBlob (no cache
			 * ring). Version is in the DA1 reply "ESC[=67;...;MAJ;MIN;...c". */
			if (termgfx_caps_cterm_version(p, np, (char)csi_par[0]) >= TERMGFX_CTERM_VER_BLOB)
				sr_audio_set_blob_ok(1);
			return;

		case 'n':   /* CTerm state report: JXL cap, audio cap, audio channel state */
			if (csi_len > 0 && csi_par[0] == '=') {
				uint8_t seq[48];
				int     sl = 0, k, r, p[8], np;

				seq[sl++] = 0x1b;
				seq[sl++] = (uint8_t)csi_intro;
				for (k = 0; k < csi_len && sl < (int)sizeof(seq) - 1; k++)
					seq[sl++] = (uint8_t)csi_par[k];
				seq[sl++] = (uint8_t)fin;

				r = termgfx_caps_parse_jxl(seq, sl);
				if (r >= 0) {
					g_is_syncterm   = 1;
					g_jxl_supported = (r == 1);
				}

				/* `CSI = 7 ; 100 ; {0,1} n` -- libsndfile present? Matches only
				 * feature id 100, so it never eats the channel report below. */
				r = termgfx_audio_parse_caps(seq, sl);
				if (r >= 0) {
					g_is_syncterm = 1;
					sr_audio_caps(r);
				}

				/* `CSI = 7 ; <ch> ; 0 n` -- that channel's FIFO drained. */
				np = sr_csi_params(p, 8);
				if (np >= 3 && p[0] == 7 && p[1] != 100 && p[2] == 0)
					sr_audio_underrun(p[1]);
			}
			return;

		case 'K':   /* SyncTERM physical key PRESS report: CSI = Pk[;Pk...] K */
			if (termgfx_keymode_evdev_active(&g_km) && csi_len > 0 && csi_par[0] == '=')
				sr_evdev_report(1);
			return;

		case 'k':   /* SyncTERM physical key RELEASE report */
			if (termgfx_keymode_evdev_active(&g_km) && csi_len > 0 && csi_par[0] == '=')
				sr_evdev_report(0);
			return;

		case 'u':   /* kitty keyboard protocol */
			if (csi_len > 0 && csi_par[0] == '?') {
				/* The reply to our `CSI ? u` query: the terminal speaks kitty.
				 * Push our flags (1 = disambiguate, 2 = report event types --
				 * the release edges this pad needs, 8 = report all keys as
				 * escape codes). evdev, if already negotiated, wins. */
				{
					char   seq[TERMGFX_KEYMODE_SEQ_MAX];
					size_t sn = termgfx_keymode_enable_kitty(&g_km, seq, sizeof seq);

					if (sn > 0) {
						sr_out_put(seq, sn);
						sr_io_out_flush();
					}
				}
				return;
			}
			{
				int mod, ev, cp = termgfx_kitty_parse(csi_par, csi_len, &mod, &ev);
				int kp;

				if (cp <= 0)
					return;
				kp = sr_kitty_numpad(cp);
				if (kp != INT_MIN) {
					sr_p2_keypad(kp, ev != 3);   /* numpad -> player 2's keypad */
					return;
				}
				if (termgfx_kitty_ctrl(mod) && cp >= 'a' && cp <= 'z')
					cp = cp - 'a' + 1;   /* Ctrl-<letter> -> its C0 code */
				if (cp < 128)
					sr_key_edge(cp, ev != 3);   /* kitty event 3 = release */
			}
			return;

		case 'A':   /* arrows = PLAYER 2's disc. Under kitty they carry */
		case 'B':   /* `1;mod:event`; on the byte path they are bare, and a */
		case 'C':   /* press is all we get. */
		case 'D':
			if (termgfx_keymode_kitty_active(&g_km)) {
				int mod, ev, b = sr_arrow_button(fin);

				(void)termgfx_kitty_parse(csi_par, csi_len, &mod, &ev);
				/* kitty event 3 = release; press or repeat re-asserts the hold.
				 * sr_pad_press/release guard a negative button internally. While
				 * suspended, swallow it (press -> anykey) instead of holding. */
				if (g_suspended) {
					if (ev != 3)
						g_anykey = 1;
				} else if (ev == 3)
					sr_pad_release(sr_profile_arrow_port(), b);
				else
					sr_pad_press(sr_profile_arrow_port(), b);
			} else if (g_suspended) {
				g_anykey = 1;   /* byte-path arrow behind a door screen: swallow */
			} else {
				sr_pad_tap(sr_profile_arrow_port(), sr_arrow_button(fin));
			}
			return;

		default:
			return;   /* every other CSI: not ours */
	}
}

/* --- input trace (diagnostic) ------------------------------------------------ */

/* Log every inbound byte -- printables literally, the rest as \xNN -- tagged with
 * the key mode in force. Answers the one question the source cannot: what a given
 * terminal actually sends for a given key.
 *
 * Two ways in. SYNCRETRO_KEYTRACE names the file outright, but bbs.exec() runs a
 * door through execvp() with the BBS's own environment, so a sysop can't set it
 * without a restart; failing that, a `keytrace.on` file in the DOOR's directory
 * turns it on and the trace lands beside it. Neither -> not even a file handle.
 *
 * Both are anchored to sr_config_launch_dir(), never to cwd: sr_config_apply()
 * has already chdir'd into the per-user sandbox by the time a key arrives, so a
 * relative path here would quietly resolve under data/user/NNNN/. */
static FILE *sr_trace_fp(void)
{
	static FILE *fp;
	static int   tried;

	if (!tried) {
		const char *path = getenv("SYNCRETRO_KEYTRACE");
		const char *dir  = sr_config_launch_dir();
		char        on[PATH_MAX], log[PATH_MAX];

		tried = 1;
		if ((path == NULL || *path == '\0') && dir != NULL) {
			snprintf(on, sizeof on, "%s/keytrace.on", dir);
			snprintf(log, sizeof log, "%s/keytrace.log", dir);
			if (fexist(on))
				path = log;
		}
		if (path != NULL && *path != '\0' && (fp = fopen(path, "w")) != NULL)
			setvbuf(fp, NULL, _IOLBF, 0);   /* survive a kill mid-session */
	}
	return fp;
}

/* Raw OUTBOUND capture, so the exact byte stream the door sends can be replayed
 * against a terminal WITHOUT the door. Framed as "T <ms> <len>\n" followed by
 * len raw bytes, in keytrace.out beside keytrace.log. Capped: a door streaming
 * audio writes megabytes a minute, and a diagnostic must not fill a disk. */
#define SR_WIRE_CAP (32u * 1024u * 1024u)

void sr_trace_wire(const void *buf, size_t len)
{
	static FILE *   fp;
	static int      tried;
	static unsigned written;
	char            hdr[64];
	int             n;

	if (sr_trace_fp() == NULL || len == 0)
		return;                 /* one switch governs both files */
	if (!tried) {
		const char *dir = sr_config_launch_dir();
		char        path[PATH_MAX];

		tried = 1;
		if (dir != NULL) {
			snprintf(path, sizeof path, "%s/keytrace.out", dir);
			fp = fopen(path, "wb");
		}
	}
	if (fp == NULL || written >= SR_WIRE_CAP)
		return;

	n = snprintf(hdr, sizeof hdr, "T %u %zu\n", (unsigned)sr_in_now_ms(), len);
	fwrite(hdr, 1, (size_t)n, fp);
	fwrite(buf, 1, len, fp);
	fflush(fp);
	written += (unsigned)len;
}

/* Free-form trace line, shared with syncretro_io.c's pacing probes. */
void sr_trace(const char *fmt, ...)
{
	FILE *  fp = sr_trace_fp();
	va_list ap;

	if (fp == NULL)
		return;
	fprintf(fp, "%10u ", (unsigned)sr_in_now_ms());
	va_start(ap, fmt);
	vfprintf(fp, fmt, ap);
	va_end(ap);
	fputc('\n', fp);
}

static void sr_keytrace(const uint8_t *buf, int n)
{
	FILE *fp = sr_trace_fp();
	int   i;

	if (fp == NULL)
		return;

	fprintf(fp, "%10u %s in  ", (unsigned)sr_in_now_ms(),
	        termgfx_keymode_evdev_active(&g_km) ? "evdev" :
	        termgfx_keymode_kitty_active(&g_km) ? "kitty" : "bytes");
	for (i = 0; i < n; i++)
		if (buf[i] >= 0x20 && buf[i] < 0x7f)
			fputc(buf[i], fp);
		else
			fprintf(fp, "\\x%02x", buf[i]);
	fputc('\n', fp);
}

/* --- read loop --------------------------------------------------------------- */

void sr_input_pump(void)
{
	uint8_t buf[256];
	int     n;
	int     i, fd = sr_io_in_fd();   /* NOT get_fd(): a stdio door reads fd 0, writes fd 1 */

	/* Expire byte-path taps even on a pump that reads nothing: a key released
	 * during a quiet moment must still come up. Harmless on the native paths,
	 * where no timer is ever armed. */
	sr_pad_expire();

	/* input_poll runs once per emulated frame, which is the sweep's clock. It
	 * must follow sr_pad_expire(): a tap that just timed out is not held. Only
	 * the Intellivision has a disc to sweep. */
	if (sr_profile_analog())
		sr_disc_tick();

	if (fd < 0)
		return;

	n = sr_plat_read(fd, buf, sizeof buf);
	if (n < 0) {
		if (n == SR_IO_AGAIN || n == SR_IO_INTR)
			return;      /* no input yet / transient: not a hangup */
		sr_door_carrier_lost();
		return;
	}
	if (n == 0) {
		sr_door_carrier_lost();   /* peer closed */
		return;
	}
	sr_keytrace(buf, (int)n);

	for (i = 0; i < n; i++) {
		uint8_t c = buf[i];

		switch (pstate) {
			case SR_P_NORMAL:
				if (c == 0x1b) {
					pstate = SR_P_ESC;
				} else if (c == 0x11) {
					/* Ctrl-Q is the emergency exit and is exempt from the native-path
					 * guard below on purpose. A native path (evdev/kitty) normally
					 * reports Ctrl-Q as an escape sequence, and that path still quits
					 * fine via the table (sr_key_edge()/sr_evdev_edge()). But if a
					 * bare 0x11 shows up anyway, the terminal isn't honoring the key
					 * mode it negotiated -- and that is exactly when a BBS user needs
					 * a guaranteed way out, so this byte always quits. */
					g_quit = 1;
				} else if (!termgfx_keymode_evdev_active(&g_km)
				           && !termgfx_keymode_kitty_active(&g_km)) {
					/* Byte path only: on a native path the same keystroke also
					 * arrives as an edge, and acting on both would double-press
					 * (and, worse, arm a timer that fights the real key-up). */
					sr_key_byte(c);
				}
				break;

			case SR_P_ESC:
				if (c == '[' || c == 'O') {
					pstate    = SR_P_CSI;
					csi_intro = (char)c;
					csi_len   = 0;
				} else if (c == '_' || c == 'P' || c == ']' || c == '^') {
					/* String sequences -- APC (_), DCS (P), OSC (]), PM (^) -- end
					 * at ST (ESC \) and carry no keys; swallow them. SyncTERM's
					 * C;L cache-list reply is an APC literally containing "C;L",
					 * so passing it through would press buttons. */
					pstate  = SR_P_APC;
					apc_len = 0;
				} else {
					/* Lone ESC: nothing on a RetroPad. Reprocess c in NORMAL. */
					pstate = SR_P_NORMAL;
					i--;
				}
				break;

			case SR_P_APC:
				if (c == 0x1b)
					pstate = SR_P_APC_ESC;              /* maybe the ST terminator */
				else if (c == 0x07)
					pstate = SR_P_NORMAL;               /* BEL also ends OSC/strings */
				else if (++apc_len > (1u << 20))
					pstate = SR_P_NORMAL;               /* unterminated -> bail */
				break;

			case SR_P_APC_ESC:
				pstate = (c == '\\') ? SR_P_NORMAL : SR_P_APC;   /* ESC '\' = ST */
				break;

			case SR_P_CSI:
				if (c >= 0x40 && c <= 0x7e) {
					sr_csi_final((char)c);
					pstate = SR_P_NORMAL;
				} else if (csi_len < (int)sizeof(csi_par)) {
					csi_par[csi_len++] = (char)c;
				}
				break;
		}
	}
}

/* Restore whatever key mode we negotiated, so the BBS gets its terminal back the
 * way it lent it to us. Called from syncretro_io.c's leave path, which stages
 * these through the same out-buffer as everything else. */
void sr_input_restore_keys(void)
{
	char   seq[TERMGFX_KEYMODE_SEQ_MAX];
	size_t n = termgfx_keymode_restore(&g_km, seq, sizeof seq);

	if (n > 0)
		sr_out_put(seq, n);
}
