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
 * Whichever mode is negotiated is UNDONE on the way out (`CSI = 2 l` / `CSI = 1 l`
 * for evdev, `CSI < u` to pop the kitty flags), so the BBS gets its terminal back
 * as it lent it. Same as the sibling doors -- with one difference: they restore
 * only on a normal exit, because their hangup path _exit()s on the assumption the
 * socket is already dead. Here sr_door_hangup() runs sr_io_leave() first (bounded
 * drain), so a READ-side hangup -- where the write direction may still be open --
 * still hands the terminal back instead of leaving it in physical-key mode.
 *
 * POSIX only for now, matching syncretro_io.c's stated scope: plain
 * read() on a non-blocking fd, no Winsock recv().
 */
#include "syncretro.h"
#include "libretro.h"    /* RETRO_DEVICE_* / RETRO_DEVICE_ID_JOYPAD_* */

#include <errno.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "caps.h"        /* termgfx: termgfx_caps_parse_jxl */
#include "keymode.h"     /* termgfx: key-mode negotiation + kitty/evdev decode */
#include "syncretro_binds.h"
#include "syncretro_keypad.h"

/* Byte-path auto-release window. Long enough that a tap registers for a frame or
 * three at 60 fps, short enough that a released key doesn't drift. Auto-repeat
 * refreshes it, so a held key stays down. */
#define SR_HOLD_MS 250

/* Cached RetroPad: one port for M1. Index by RETRO_DEVICE_ID_JOYPAD_*. */
#define SR_PAD_IDS (RETRO_DEVICE_ID_JOYPAD_R3 + 1)

static int16_t  g_joypad[SR_PAD_IDS];
static uint32_t g_expire_ms[SR_PAD_IDS];   /* byte path only: 0 = no timer */
static int      g_quit;

static uint32_t sr_in_now_ms(void)
{
	struct timespec t;

	clock_gettime(CLOCK_MONOTONIC, &t);
	return (uint32_t)((uint64_t)t.tv_sec * 1000ULL + (uint64_t)t.tv_nsec / 1000000ULL);
}

int sr_input_quit_requested(void) { return g_quit; }

/* --- pad edges -------------------------------------------------------------- */

/* Native paths (kitty/evdev): held until the explicit key-up, so no timer. */
static void sr_pad_press(int id)
{
	if (id < 0 || id >= SR_PAD_IDS)
		return;
	g_joypad[id]   = 1;
	g_expire_ms[id] = 0;
}

static void sr_pad_release(int id)
{
	if (id < 0 || id >= SR_PAD_IDS)
		return;
	g_joypad[id]   = 0;
	g_expire_ms[id] = 0;
}

/* Byte path: press + arm the auto-release timer (a repeat byte refreshes it). */
static void sr_pad_tap(int id)
{
	if (id < 0 || id >= SR_PAD_IDS)
		return;
	g_joypad[id]    = 1;
	g_expire_ms[id] = sr_in_now_ms() + SR_HOLD_MS;
	if (g_expire_ms[id] == 0)
		g_expire_ms[id] = 1;   /* 0 is the "no timer" sentinel */
}

static void sr_pad_expire(void)
{
	uint32_t now = sr_in_now_ms();
	int      id;

	for (id = 0; id < SR_PAD_IDS; id++)
		if (g_expire_ms[id] != 0 && (int32_t)(now - g_expire_ms[id]) >= 0) {
			g_joypad[id]    = 0;
			g_expire_ms[id] = 0;
		}
	sr_keypad_expire(now);
}

int16_t sr_pad_get(unsigned port, unsigned device, unsigned index, unsigned id)
{
	if (port != 0)
		return 0;
	if (device == RETRO_DEVICE_ANALOG)
		return sr_keypad_analog(index, id);   /* right stick = keypad digits */
	if (device != RETRO_DEVICE_JOYPAD)
		return 0;
	if (id >= SR_PAD_IDS)
		return 0;
	return g_joypad[id];
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
static int g_anykey;      /* a bound key arrived while suspended */

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

static void sr_door_action(int id)
{
	if (id == SR_DOOR_QUIT)
		g_quit = 1;        /* the existing, separately-polled latch */
	else
		g_action = id;
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
	int      id;
	sr_act_t act = sr_bind_lookup(sr_bind_fold(c), &id);

	if (act == SR_ACT_NONE)
		return;            /* unbound: it must never reach the core */

	if (act == SR_ACT_DOOR) {
		if (down)
			sr_door_action(id);
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
				sr_pad_release(id);
			else if (tap)
				sr_pad_tap(id);
			else
				sr_pad_press(id);
			return;
		case SR_ACT_DIGIT:
			if (!down)
				sr_keypad_release(id);
			else if (tap)
				sr_keypad_tap(id, sr_in_now_ms());
			else
				sr_keypad_press(id);
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
	int id;

	for (id = 0; id < SR_PAD_IDS; id++) {
		g_joypad[id]    = 0;
		g_expire_ms[id] = 0;
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
			 * lowercase in sr_bind_lookup(); shifted digits become punctuation and
			 * are simply unbound, which is what a real keypad does too. */
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
	/* While a door screen is up, swallow the arrow rather than hold it -- same
	 * rule sr_key_apply() enforces for every other game key. A press is
	 * remembered so "press any key to return" is true; a release does nothing. */
	if (g_suspended) {
		if (down)
			g_anykey = 1;
		return;
	}
	if (down)
		sr_pad_press(button);
	else
		sr_pad_release(button);
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

static void sr_csi_final(char fin)
{
	int p[16], np;

	switch (fin) {
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
			return;

		case 'n':   /* CTerm state report: our Q;JXL query's reply is ESC[=1;{0,1}n */
			if (csi_len > 0 && csi_par[0] == '=') {
				uint8_t seq[48];
				int     sl = 0, k, r;

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

				if (cp <= 0)
					return;
				if (termgfx_kitty_ctrl(mod) && cp >= 'a' && cp <= 'z')
					cp = cp - 'a' + 1;   /* Ctrl-<letter> -> its C0 code */
				if (cp < 128)
					sr_key_edge(cp, ev != 3);   /* kitty event 3 = release */
			}
			return;

		case 'A':   /* arrows. Under kitty they carry `1;mod:event`; on the byte */
		case 'B':   /* path they are bare, and a press is all we get. */
		case 'C':
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
					sr_pad_release(b);
				else
					sr_pad_press(b);
			} else if (g_suspended) {
				g_anykey = 1;   /* byte-path arrow behind a door screen: swallow */
			} else {
				sr_pad_tap(sr_arrow_button(fin));
			}
			return;

		default:
			return;   /* every other CSI: not ours */
	}
}

/* --- read loop --------------------------------------------------------------- */

void sr_input_pump(void)
{
	uint8_t buf[256];
	ssize_t n;
	int     i, fd = sr_io_get_fd();

	/* Expire byte-path taps even on a pump that reads nothing: a key released
	 * during a quiet moment must still come up. Harmless on the native paths,
	 * where no timer is ever armed. */
	sr_pad_expire();

	if (fd < 0)
		return;

	n = read(fd, buf, sizeof buf);
	if (n < 0) {
		if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
			return;      /* no input yet / transient: not a hangup */
		sr_door_carrier_lost();
		return;
	}
	if (n == 0) {
		sr_door_carrier_lost();   /* peer closed */
		return;
	}

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
