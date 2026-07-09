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

/* Byte-path auto-release window. Long enough that a tap registers for a frame or
 * three at 60 fps, short enough that a released key doesn't drift. Auto-repeat
 * refreshes it, so a held key stays down. */
#define SR_HOLD_MS 250

/* SyncTERM resyncs the keys held at the instant physical-key reports are
 * enabled, emitting a PRESS for each -- e.g. the Enter still held from picking
 * the door off the BBS menu. Drop non-modifier presses for a short window so a
 * stray held key doesn't press a button before the game has drawn a frame.
 * Releases are always honored, so nothing can stick. */
#define SR_EVDEV_SETTLE_MS 500

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
}

int16_t sr_pad_get(unsigned port, unsigned device, unsigned id)
{
	if (port != 0 || device != RETRO_DEVICE_JOYPAD)
		return 0;
	if (id >= SR_PAD_IDS)
		return 0;
	return g_joypad[id];
}

/* --- the key map: one place, shared by all three paths ---------------------- */

/* An ASCII character (lowercased) -> RetroPad button, or -1. The d-pad also
 * lives on the arrow keys, which every path delivers as a separate case.
 *
 * RetroPad is an abstract SNES-shaped joypad; the core decides what its buttons
 * mean. FreeIntv maps A/B/X to the Intellivision's three side buttons and drives
 * the 16-way disc from the d-pad, so the layout below is playable on it without
 * any per-core knowledge. The Intellivision KEYPAD (12 keys) has no RetroPad
 * home and needs the overlay work tracked as M2. */
static int sr_key_button(int c)
{
	switch (c) {
		case 'w': return RETRO_DEVICE_ID_JOYPAD_UP;
		case 's': return RETRO_DEVICE_ID_JOYPAD_DOWN;
		case 'a': return RETRO_DEVICE_ID_JOYPAD_LEFT;
		case 'd': return RETRO_DEVICE_ID_JOYPAD_RIGHT;
		case ' ':
		case 'z': return RETRO_DEVICE_ID_JOYPAD_A;
		case 'x': return RETRO_DEVICE_ID_JOYPAD_B;
		case 'c': return RETRO_DEVICE_ID_JOYPAD_X;
		case 'v': return RETRO_DEVICE_ID_JOYPAD_Y;
		case 'q': return RETRO_DEVICE_ID_JOYPAD_L;
		case 'e': return RETRO_DEVICE_ID_JOYPAD_R;
		case '\r':
		case '\n': return RETRO_DEVICE_ID_JOYPAD_START;
		case '\t': return RETRO_DEVICE_ID_JOYPAD_SELECT;
		default:   return -1;
	}
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

static char     csi_intro;         /* '[' or 'O' */
static char     csi_par[40];       /* parameter bytes between intro and final */
static int      csi_len;
static unsigned apc_len;           /* bytes swallowed in the current string seq */

static int      g_probe_replied;
static int      g_is_syncterm;
static int      g_have_sixel;
static int      g_jxl_supported;

static int      g_kitty_active;
static int      g_evdev_active;
static unsigned g_evdev_mods;      /* bit0 Shift, bit1 Ctrl, bit2 Alt */
static uint32_t g_evdev_enabled_ms;
static int      g_evdev_settling;

#define SR_EVDEV_MOD_SHIFT 1
#define SR_EVDEV_MOD_CTRL  2
#define SR_EVDEV_MOD_ALT   4

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

/* Decode a kitty CSI-u style parameter list: `key[:alt];[mod[:event]]`. Returns
 * the key codepoint; *mod is the 1-based modifier field, *ev the event type
 * (1 press, 2 repeat, 3 release). Also used for the arrow finals, which kitty
 * reports as `CSI 1;mod:event A`. */
static int sr_kitty_key(int *mod, int *ev)
{
	int key = 0, v = 0, sub = 0, field = 0, in_sub = 0, i;

	*mod = 1;
	*ev  = 1;
	for (i = 0; i <= csi_len; i++) {
		char c = (i < csi_len) ? csi_par[i] : ';';   /* sentinel flushes the last field */

		if (c >= '0' && c <= '9') {
			if (in_sub)
				sub = sub * 10 + (c - '0');
			else
				v = v * 10 + (c - '0');
		} else if (c == ':') {
			in_sub = 1;                              /* a field's first sub-param is the event type */
			sub = 0;
		} else if (c == ';') {
			if (field == 0) {
				key = v;
			} else if (field == 1) {
				if (v)
					*mod = v;
				if (in_sub && sub)
					*ev = sub;
			}
			field++;
			v = sub = in_sub = 0;
		}
	}
	return key;
}

/* kitty's modifier field is 1-based: bit 0 = Shift, bit 1 = Alt, bit 2 = Ctrl. */
static int sr_kitty_ctrl(int mod) { return ((mod - 1) & 4) != 0; }

/* Apply a press/repeat/release edge for a mapped button (kitty event types). */
static void sr_kitty_edge(int button, int ev)
{
	if (button < 0)
		return;
	if (ev == 3)
		sr_pad_release(button);
	else
		sr_pad_press(button);   /* press or repeat: re-assert the hold */
}

/* --- evdev (SyncTERM physical key reports) ---------------------------------- */

/* evdev code -> the unshifted ASCII character it names, for the codes the pad
 * map cares about. Layout-independent by construction: EVDEV_KEY_W is the
 * physical W position on every keyboard layout. Codes outside the table (and
 * the modifiers, handled separately) yield 0. */
static char sr_evdev_ascii(int code)
{
	static const char tbl[58] = {
		0,   27,  '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=',
		8,   '\t',
		'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']',
		'\r',
		0,   /* 29 LeftCtrl */
		'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
		0,   /* 42 LeftShift */
		'\\',
		'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/',
		0,   /* 54 RightShift */
		'*',
		0,   /* 56 LeftAlt */
		' '
	};

	if (code < 0 || code >= (int)sizeof(tbl))
		return 0;
	return tbl[code];
}

/* Linux evdev codes for the keys with no ASCII form that we still map. */
#define SR_EVDEV_ESC       1
#define SR_EVDEV_LEFTCTRL 29
#define SR_EVDEV_LEFTSHIFT 42
#define SR_EVDEV_RIGHTSHIFT 54
#define SR_EVDEV_LEFTALT  56
#define SR_EVDEV_UP      103
#define SR_EVDEV_LEFT    105
#define SR_EVDEV_RIGHT   106
#define SR_EVDEV_DOWN    108

static void sr_evdev_edge(int code, int down)
{
	int  button = -1;
	char c;

	/* Modifiers are their own keys on this path: track them, never map them. */
	switch (code) {
		case SR_EVDEV_LEFTCTRL:
			if (down)
				g_evdev_mods |= SR_EVDEV_MOD_CTRL;
			else
				g_evdev_mods &= ~SR_EVDEV_MOD_CTRL;
			return;
		case SR_EVDEV_LEFTSHIFT:
		case SR_EVDEV_RIGHTSHIFT:
			if (down)
				g_evdev_mods |= SR_EVDEV_MOD_SHIFT;
			else
				g_evdev_mods &= ~SR_EVDEV_MOD_SHIFT;
			return;
		case SR_EVDEV_LEFTALT:
			if (down)
				g_evdev_mods |= SR_EVDEV_MOD_ALT;
			else
				g_evdev_mods &= ~SR_EVDEV_MOD_ALT;
			return;
		default:
			break;
	}

	/* Drop the enable-time resync's press edges (see SR_EVDEV_SETTLE_MS). */
	if (down && g_evdev_settling) {
		if ((uint32_t)(sr_in_now_ms() - g_evdev_enabled_ms) < SR_EVDEV_SETTLE_MS)
			return;
		g_evdev_settling = 0;
	}

	switch (code) {
		case SR_EVDEV_UP:    button = RETRO_DEVICE_ID_JOYPAD_UP;    break;
		case SR_EVDEV_DOWN:  button = RETRO_DEVICE_ID_JOYPAD_DOWN;  break;
		case SR_EVDEV_LEFT:  button = RETRO_DEVICE_ID_JOYPAD_LEFT;  break;
		case SR_EVDEV_RIGHT: button = RETRO_DEVICE_ID_JOYPAD_RIGHT; break;
		default:
			c = sr_evdev_ascii(code);
			if (c == 0)
				return;
			if (down && (g_evdev_mods & SR_EVDEV_MOD_CTRL) && c == 'q') {
				g_quit = 1;   /* Ctrl-Q */
				return;
			}
			button = sr_key_button(c);
			break;
	}
	if (button < 0)
		return;
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
					if (p[k] == 8 && csi_par[0] == '<' && !g_evdev_active && !g_kitty_active) {
						g_evdev_active     = 1;
						g_evdev_enabled_ms = sr_in_now_ms();
						g_evdev_settling   = 1;
						sr_out_put("\x1b[=1h\x1b[=2h", 10);
						sr_io_out_flush();
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
			if (g_evdev_active && csi_len > 0 && csi_par[0] == '=')
				sr_evdev_report(1);
			return;

		case 'k':   /* SyncTERM physical key RELEASE report */
			if (g_evdev_active && csi_len > 0 && csi_par[0] == '=')
				sr_evdev_report(0);
			return;

		case 'u':   /* kitty keyboard protocol */
			if (csi_len > 0 && csi_par[0] == '?') {
				/* The reply to our `CSI ? u` query: the terminal speaks kitty.
				 * Push our flags (1 = disambiguate, 2 = report event types --
				 * the release edges this pad needs, 8 = report all keys as
				 * escape codes). evdev, if already negotiated, wins. */
				if (!g_kitty_active && !g_evdev_active) {
					g_kitty_active = 1;
					sr_out_put("\x1b[>11u", 6);
					sr_io_out_flush();
				}
				return;
			}
			{
				int mod, ev, cp = sr_kitty_key(&mod, &ev);

				if (cp <= 0)
					return;
				if (ev != 3 && sr_kitty_ctrl(mod) && (cp | 0x20) == 'q') {
					g_quit = 1;   /* Ctrl-Q */
					return;
				}
				if (cp < 128)
					sr_kitty_edge(sr_key_button(cp | 0x20), ev);
			}
			return;

		case 'A':   /* arrows. Under kitty they carry `1;mod:event`; on the byte */
		case 'B':   /* path they are bare, and a press is all we get. */
		case 'C':
		case 'D':
			if (g_kitty_active) {
				int mod, ev;

				(void)sr_kitty_key(&mod, &ev);
				sr_kitty_edge(sr_arrow_button(fin), ev);
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
					g_quit = 1;             /* Ctrl-Q */
				} else if (!g_evdev_active && !g_kitty_active) {
					/* Byte path only: on a native path the same keystroke also
					 * arrives as an edge, and acting on both would double-press
					 * (and, worse, arm a timer that fights the real key-up).
					 * SyncTERM's `CSI = 2 h` suppresses these bytes entirely;
					 * kitty's flag 8 does the same. The guard makes it explicit
					 * rather than relying on the terminal to honor them. */
					sr_pad_tap(sr_key_button(c | 0x20));
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
	if (g_evdev_active) {
		sr_out_put("\x1b[=2l\x1b[=1l", 10);   /* restore translated keys, disable reports */
		g_evdev_active = 0;
	}
	if (g_kitty_active) {
		sr_out_put("\x1b[<u", 4);             /* pop the kitty flags we pushed */
		g_kitty_active = 0;
	}
}
