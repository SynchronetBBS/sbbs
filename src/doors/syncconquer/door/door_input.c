/*
 * door_input.c -- terminal input parser: CSI/SS3 keys, kitty keyboard
 * protocol, SyncTERM evdev physical-key reports and SGR 1006 mouse, decoded
 * from door_io.c's Task-2 ring buffer (door_input_byte(), door.h) into the
 * small event queue wwkeyboard_termgfx.cpp drains into the engine.
 *
 * Structure and protocol handling cloned/trimmed from syncduke_input.c
 * (src/doors/syncduke/): the kitty CSI-u "key;mods:event" parser
 * (kitty_parse(), was kitty_key()), the CTDA-cap-8 evdev physical-key report
 * shape (evdev_edge()/evdev_report()), the US-QWERTY evdev->ASCII table, and
 * the xterm SGR mouse report ("ESC[<b;col;rowM/m"). DROPPED entirely: the
 * FPS mouse-steer/FOLLOW model and all the Duke-specific scancode/hold-timer
 * machinery (syncduke's engine has no true key-up on the byte path, so it
 * synthesizes one with a frame-counted release timer) -- WWKeyboardClass
 * just wants discrete key/mouse events, so a legacy (non-kitty/evdev) byte
 * becomes an immediate press+release "tap" instead (door_input_key_tap()).
 * The C;L SyncTERM cache-list APC reply syncduke calls out (syncduke_input.c
 * :1536) needs no handling here: door_io.c's own pump already swallows every
 * APC/DCS/OSC/PM string (P_APC/P_APC_ESC, door_io.c) before a single byte of
 * it ever reaches door_input_byte().
 *
 * VK_* codes emitted here are the plain Win32-style values wwkeyboard.h's
 * non-SDL branch defines (our build sets none of SDL_BUILD/SDL1_BUILD/
 * SDL2_BUILD) -- e.g. VK_A=0x41 (== ASCII 'A'), VK_LEFT=0x25. Kept as local
 * macros (not wwkeyboard.h's enum) so this file stays plain C, usable
 * without a C++ compiler -- wwkeyboard_termgfx.cpp passes the values through
 * to Put_Key_Message()/Put_Mouse_Message() verbatim.
 */

#include <stdint.h>

#include "door.h"
#include "door_io.h"
#include "door_input.h"
#include "keymode.h"   /* termgfx: key-mode negotiation + kitty/evdev decode */

/* --- plain Win32-style VK_* values (wwkeyboard.h's non-SDL branch) --------- */
#define VK_LBUTTON   0x01
#define VK_RBUTTON   0x02
#define VK_MBUTTON   0x04
#define VK_BACK      0x08
#define VK_TAB       0x09
#define VK_RETURN    0x0D
#define VK_SHIFT     0x10
#define VK_CONTROL   0x11
#define VK_MENU      0x12   /* Alt */
#define VK_ESCAPE    0x1B
#define VK_SPACE     0x20
#define VK_PRIOR     0x21   /* Page Up */
#define VK_NEXT      0x22   /* Page Down */
#define VK_END       0x23
#define VK_HOME      0x24
#define VK_LEFT      0x25
#define VK_UP        0x26
#define VK_RIGHT     0x27
#define VK_DOWN      0x28
#define VK_INSERT    0x2D
#define VK_DELETE    0x2E
#define VK_0         0x30
#define VK_A         0x41
#define VK_F1        0x70
#define VK_F2        0x71
#define VK_F3        0x72
#define VK_F4        0x73
#define VK_F5        0x74
#define VK_F6        0x75
#define VK_F7        0x76
#define VK_F8        0x77
#define VK_F9        0x78
#define VK_F10       0x79
#define VK_F11       0x7A
#define VK_F12       0x7B
#define VK_NONE_BA   0xBA   /* ; : */
#define VK_NONE_BB   0xBB   /* = + */
#define VK_NONE_BC   0xBC   /* , < */
#define VK_NONE_BD   0xBD   /* - _ */
#define VK_NONE_BE   0xBE   /* . > */
#define VK_NONE_BF   0xBF   /* / ? */
#define VK_NONE_C0   0xC0   /* ` ~ */
#define VK_NONE_DB   0xDB   /* [ { */
#define VK_NONE_DC   0xDC   /* \ | */
#define VK_NONE_DD   0xDD   /* ] } */
#define VK_NONE_DE   0xDE   /* ' " */
#define VK_MOUSEWHEEL_DOWN 0xFD
#define VK_MOUSEWHEEL_UP   0xFE

/* --- event queue (drained by wwkeyboard_termgfx.cpp) ----------------------- */
#define DOOR_EVQ_SZ 64
static door_input_event_t g_evq[DOOR_EVQ_SZ];
static int                g_evq_head, g_evq_tail;

static void evq_push(const door_input_event_t *ev)
{
	int next = (g_evq_head + 1) % DOOR_EVQ_SZ;
	if (next == g_evq_tail)          /* full: drop the oldest (never expected in practice --
		                               * Fill_Buffer_From_System drains every poll) */
		g_evq_tail = (g_evq_tail + 1) % DOOR_EVQ_SZ;
	g_evq[g_evq_head] = *ev;
	g_evq_head = next;
}

int door_input_next_event(door_input_event_t *ev)
{
	if (g_evq_head == g_evq_tail)
		return 0;
	*ev = g_evq[g_evq_tail];
	g_evq_tail = (g_evq_tail + 1) % DOOR_EVQ_SZ;
	return 1;
}

static void emit_key(unsigned short vk, int release)
{
	door_input_event_t ev;
	ev.kind    = DOOR_IN_KEY;
	ev.vk      = vk;
	ev.release = release;
	ev.x = ev.y = 0;
	evq_push(&ev);
}

/* A legacy terminal byte gives no key-up: queue an immediate press+release. */
static void emit_tap(unsigned short vk)
{
	emit_key(vk, 0);
	emit_key(vk, 1);
}

/* Bracket a tap with a real Shift/Ctrl press+release around it, so
 * Put_Key_Message's own Down(KN_LSHIFT)/Down(KN_LCTRL) check (wwkeyboard.cpp)
 * ORs the right WWKEY_*_BIT in automatically -- the same mechanism a real
 * keyboard driver relies on, just synthesized instantly since a legacy
 * terminal never reports the modifier key on its own. Momentary only: by
 * the time the bracket returns, Down(shift/ctrl) is back to false, so this
 * does NOT give legacy-only clients a true "modifier currently held" signal
 * for gameplay checks that poll Down() directly (e.g. Ctrl-Q quit, shift-
 * click multi-select) -- kitty/evdev clients get that for real, below. */
static void emit_tap_mod(unsigned short vk, unsigned short mod_vk)
{
	emit_key(mod_vk, 0);
	emit_key(vk, 0);
	emit_key(vk, 1);
	emit_key(mod_vk, 1);
}

static void emit_mouse(unsigned short vk, int x, int y, int release)
{
	door_input_event_t ev;
	ev.kind    = DOOR_IN_MOUSE;
	ev.vk      = vk;
	ev.release = release;
	ev.x = x; ev.y = y;
	evq_push(&ev);
}

/* --- tracked pointer (door_core.c's door_mouse_xy forwards here) ----------- */
static int g_mouse_x = DOOR_FB_WIDTH / 2;
static int g_mouse_y = DOOR_FB_HEIGHT / 2;

void door_input_mouse_xy(int *x, int *y)
{
	*x = g_mouse_x;
	*y = g_mouse_y;
}

/* --- ASCII <-> VK table (the sdl_keymap.h equivalent for our VK space) ----- */
int door_input_ascii_from_vk(unsigned short vk, int shifted)
{
	if (vk >= VK_A && vk <= VK_A + 25)
		return shifted ? (int)('A' + (vk - VK_A)) : (int)('a' + (vk - VK_A));
	if (vk >= VK_0 && vk <= VK_0 + 9) {
		if (!shifted)
			return (int)vk;
		switch (vk - VK_0) {
			case 0: return ')'; case 1: return '!'; case 2: return '@';
			case 3: return '#'; case 4: return '$'; case 5: return '%';
			case 6: return '^'; case 7: return '&'; case 8: return '*';
			case 9: return '(';
		}
	}
	switch (vk) {
		case VK_SPACE:    return ' ';
		case VK_RETURN:   return '\r';
		case VK_TAB:      return '\t';
		case VK_BACK:     return '\b';
		case VK_ESCAPE:   return 0x1b;
		case VK_NONE_BA:  return shifted ? ':' : ';';
		case VK_NONE_BB:  return shifted ? '+' : '=';
		case VK_NONE_BC:  return shifted ? '<' : ',';
		case VK_NONE_BD:  return shifted ? '_' : '-';
		case VK_NONE_BE:  return shifted ? '>' : '.';
		case VK_NONE_BF:  return shifted ? '?' : '/';
		case VK_NONE_C0:  return shifted ? '~' : '`';
		case VK_NONE_DB:  return shifted ? '{' : '[';
		case VK_NONE_DC:  return shifted ? '|' : '\\';
		case VK_NONE_DD:  return shifted ? '}' : ']';
		case VK_NONE_DE:  return shifted ? '"' : '\'';
	}
	return 0;   /* no ASCII meaning (arrows, F-keys, Home/End/...) -> KA_NONE */
}

unsigned short door_input_vk_from_ascii(int ch, int *shift_out)
{
	*shift_out = 0;
	if (ch >= 'a' && ch <= 'z')
		return (unsigned short)(VK_A + (ch - 'a'));
	if (ch >= 'A' && ch <= 'Z') {
		*shift_out = 1;
		return (unsigned short)(VK_A + (ch - 'A'));
	}
	if (ch >= '0' && ch <= '9')
		return (unsigned short)(VK_0 + (ch - '0'));
	switch (ch) {
		case ' ':  return VK_SPACE;
		case '\r': case '\n': return VK_RETURN;
		case '\t': return VK_TAB;
		case '\b': return VK_BACK;
		case 0x7f: return VK_DELETE;
		case 0x1b: return VK_ESCAPE;
		case ';':  return VK_NONE_BA;
		case ':':  *shift_out = 1; return VK_NONE_BA;
		case '=':  return VK_NONE_BB;
		case '+':  *shift_out = 1; return VK_NONE_BB;
		case ',':  return VK_NONE_BC;
		case '<':  *shift_out = 1; return VK_NONE_BC;
		case '-':  return VK_NONE_BD;
		case '_':  *shift_out = 1; return VK_NONE_BD;
		case '.':  return VK_NONE_BE;
		case '>':  *shift_out = 1; return VK_NONE_BE;
		case '/':  return VK_NONE_BF;
		case '?':  *shift_out = 1; return VK_NONE_BF;
		case '`':  return VK_NONE_C0;
		case '~':  *shift_out = 1; return VK_NONE_C0;
		case '[':  return VK_NONE_DB;
		case '{':  *shift_out = 1; return VK_NONE_DB;
		case '\\': return VK_NONE_DC;
		case '|':  *shift_out = 1; return VK_NONE_DC;
		case ']':  return VK_NONE_DD;
		case '}':  *shift_out = 1; return VK_NONE_DD;
		case '\'': return VK_NONE_DE;
		case '"':  *shift_out = 1; return VK_NONE_DE;
		case '!':  *shift_out = 1; return VK_0 + 1;
		case '@':  *shift_out = 1; return VK_0 + 2;
		case '#':  *shift_out = 1; return VK_0 + 3;
		case '$':  *shift_out = 1; return VK_0 + 4;
		case '%':  *shift_out = 1; return VK_0 + 5;
		case '^':  *shift_out = 1; return VK_0 + 6;
		case '&':  *shift_out = 1; return VK_0 + 7;
		case '*':  *shift_out = 1; return VK_0 + 8;
		case '(':  *shift_out = 1; return VK_0 + 9;
		case ')':  *shift_out = 1; return VK_0 + 0;
	}
	return 0;
}

/* Encode+queue one legacy ASCII byte as a shift-bracketed tap (see
 * emit_tap_mod() above); a 0 return from door_input_vk_from_ascii() (no
 * mapping) is silently dropped. Legacy control bytes 0x01-0x1A map to
 * Ctrl+letter (except bytes already handled: 0x04/0x06/0x10/0x15 consumed by
 * door_io.c, and 0x08/0x09/0x0A/0x0D kept via their normal paths). */
static void door_input_key_tap(int ch)
{
	int            shift;
	unsigned short vk;

	/* Legacy control bytes 0x01-0x1A: map to Ctrl+letter (skip the door
	 * hotkeys 0x04 F4 fallback, 0x06 fit toggle, 0x10/0x15 node Ctrl-P/Ctrl-U,
	 * 0x13 Ctrl-S stats -- door_io.c consumes those via door_io_hotkey() -- and
	 * keep 0x08 \b, 0x09 \t, 0x0A \n, 0x0D \r via their normal paths). */
	if (ch >= 0x01 && ch <= 0x1A) {
		if (ch != 0x04 && ch != 0x06 && ch != 0x10 && ch != 0x15 && ch != 0x13
		    && ch != 0x08 && ch != 0x09 && ch != 0x0A && ch != 0x0D) {
			emit_tap_mod((unsigned short)(VK_A + (ch - 1)), VK_CONTROL);
			return;
		}
	}

	vk = door_input_vk_from_ascii(ch, &shift);
	if (vk == 0)
		return;
	if (shift)
		emit_tap_mod(vk, VK_SHIFT);
	else
		emit_tap(vk);
}

/* --- SGR mouse: cell coords -> game pixel coords via door_io's rect ------- */
static void mouse_event(int button, int col, int row, int release)
{
	int ew = 0, eh = 0, dx = 0, dy = 0, cw = 8, ch = 16, px, py, gx, gy;
	int gc = 0, gr = 0;

	/* Auto-detect SGR-Pixels: a reported coord past the text grid proves the
	 * terminal is sending PIXELS (mode 1016 live) even if the DECRQM handshake
	 * never confirmed it -- some terminals honor ?1016h but don't answer
	 * ?1016$p. Cell coords never exceed the grid, so this can't false-trigger
	 * on a genuinely cell-granular terminal. */
	door_io_grid(&gc, &gr);
	if (!door_io_mouse_pixels() && ((gc > 0 && col > gc) || (gr > 0 && row > gr)))
		door_io_note_pixel_report();

	door_io_get_rect(&ew, &eh, &dx, &dy);
	door_io_cell_size(&cw, &ch);
	if (ew <= 0)
		ew = DOOR_FB_WIDTH;
	if (eh <= 0)
		eh = DOOR_FB_HEIGHT;

	/* Under SGR-Pixels (DEC 1016, Windows Terminal) the report ALREADY carries
	 * 1-based canvas pixels -- use them directly for per-pixel precision (a
	 * draggable volume slider). Otherwise SGR reports a 1-based text CELL:
	 * convert to the cell centre in the terminal's OWN canvas pixels using its
	 * real cell size (door_io_cell_size(): canvas/grid derived, not assumed
	 * 8x16 -- a scaled canvas or non-8x16 font otherwise misplaces the cursor).
	 * Either way px/py end up in canvas pixels; clamp to the displayed-image
	 * rect, then rescale into game (DOOR_FB_WIDTH x DOOR_FB_HEIGHT) coords. */
	if (door_io_mouse_pixels()) {
		px = col - 1;
		py = row - 1;
	} else {
		px = (col - 1) * cw + cw / 2;
		py = (row - 1) * ch + ch / 2;
		/* A cell-granular terminal (SyncTERM) can only place the cursor at cell
		 * CENTRES. The LAST row/col centre sits cw/2 or ch/2 px SHORT of the far
		 * canvas edge -- too far in for RA's edge-scroll zone -- so snap those to
		 * the image-rect edge to make scroll-down/right reachable (this happened
		 * in sixel by accident via its reserved-bottom-row clamp). Do NOT snap
		 * the FIRST row/col: their centre is already near the top/left edge, and
		 * snapping the top row to y=0 dragged tab clicks into the scroll-up zone
		 * so the menu tabs couldn't be clicked -- keep the cell centre there. */
		if (gc > 0 && col >= gc)
			px = dx + ew - 1;
		if (gr > 0 && row >= gr)
			py = dy + eh - 1;
	}
	if (px < dx)
		px = dx;
	if (px >= dx + ew)
		px = dx + ew - 1;
	if (py < dy)
		py = dy;
	if (py >= dy + eh)
		py = dy + eh - 1;

	gx = (px - dx) * DOOR_FB_WIDTH  / ew;
	gy = (py - dy) * DOOR_FB_HEIGHT / eh;
	if (gx < 0)
		gx = 0;
	if (gx >= DOOR_FB_WIDTH)
		gx = DOOR_FB_WIDTH - 1;
	if (gy < 0)
		gy = 0;
	if (gy >= DOOR_FB_HEIGHT)
		gy = DOOR_FB_HEIGHT - 1;

	g_mouse_x = gx;
	g_mouse_y = gy;

	{
		int base = button & ~28;   /* strip the SGR modifier bits (4 Shift,
		                            * 8 Alt/Meta, 16 Ctrl) to classify the button */

		if (!(base & 32)) {              /* a real press/release, not a motion report */
			int            b  = base & 3;   /* 0 left, 1 middle, 2 right (xterm SGR) */
			unsigned short vk = (b == 0) ? VK_LBUTTON : (b == 1) ? VK_MBUTTON : VK_RBUTTON;
			/* On the LEGACY key path (no evdev/kitty native modifier tracking)
			 * synthesize the held modifier from the SGR report's OWN bits, so
			 * Shift+click (add-to-selection), Ctrl+click (force-attack) and
			 * Alt+click (force-move) work without native key events -- the
			 * engine polls Keyboard->Down() for these, which a momentary legacy
			 * key tap can't satisfy. evdev/kitty already carry the real
			 * modifier keys, so don't double-apply (it would clobber a genuinely
			 * held modifier). Bracket the click so Down() reads the modifier
			 * held while the engine processes it. */
			int leg = !door_io_evdev_active() && !door_input_kitty_active();
			int ms  = leg && (button & 4);
			int ma  = leg && (button & 8);
			int mc  = leg && (button & 16);

			if (mc)
				emit_key(VK_CONTROL, 0);
			if (ms)
				emit_key(VK_SHIFT, 0);
			if (ma)
				emit_key(VK_MENU, 0);
			emit_mouse(vk, gx, gy, release);
			if (ma)
				emit_key(VK_MENU, 1);
			if (ms)
				emit_key(VK_SHIFT, 1);
			if (mc)
				emit_key(VK_CONTROL, 1);
		} else if (base == 64) {
			emit_key(VK_MOUSEWHEEL_UP, 0);
		} else if (base == 65) {
			emit_key(VK_MOUSEWHEEL_DOWN, 0);
		}
		/* else: plain motion, no button -- position updated above, nothing to queue */
	}
}

/* --- kitty keyboard protocol ------------------------------------------------
 * Negotiated by sending "ESC[?u" at term-enter (door_io.c); a reply carrying
 * a '?' marker means the terminal understands it -- push our flags (1
 * disambiguate | 2 report event types | 8 report all keys, incl. modifier
 * keys themselves, as their own events) so every key from here on arrives as
 * a CSI-u/CSI-letter/CSI-~ event with a real ";mods:event" tail (event 1
 * press, 2 repeat, 3 release) -- true key-up, unlike a plain terminal. */
int door_input_kitty_active(void)
{
	return termgfx_keymode_kitty_active(door_io_keymode());
}

static int  g_csi_len;
static char g_csi_par[64];

/* The kitty CSI-u parameter decode lives in termgfx (keymode.h), shared with the
 * sibling doors; this wrapper just feeds it this module's CSI accumulator. */
static int kitty_parse(int *mod, int *ev)
{
	return termgfx_kitty_parse(g_csi_par, g_csi_len, mod, ev);
}

/* Parse up to `max` ';'-separated decimal params from g_csi_par (a leading
 * non-digit marker byte like '<'/'='/'?' is simply skipped). */
static int door_in_csi_params(int *out, int max)
{
	int n = 0, i, have = 0, v = 0;
	for (i = 0; i <= g_csi_len && n < max; i++) {
		char c = (i < g_csi_len) ? g_csi_par[i] : ';';
		if (c >= '0' && c <= '9') { v = v * 10 + (c - '0'); have = 1; }
		else if (c == ';') {
			if (have)
				out[n++] = v;
			v = 0;
			have = 0;
		}
	}
	return n;
}

/* kitty reports the numpad with two different codepoint sets depending on
 * NumLock; fold the NumLock-off function set back to the digit-key PUA
 * codepoint so one map below serves both (syncduke_input.c's
 * kitty_kp_normalize -- same data, generic to the protocol). */
static int kitty_kp_normalize(int cp)
{
	switch (cp) {
		case 57417: return 57403; case 57418: return 57405;
		case 57419: return 57407; case 57420: return 57401;
		case 57421: return 57408; case 57422: return 57402;
		case 57423: return 57406; case 57424: return 57400;
		case 57425: return 57399; case 57426: return 57409;
		case 57427: return 57404;
	}
	return cp;
}

/* Kitty's own PUA codepoints for the modifier keys (flag 8, "report all
 * keys"): pressing/releasing Shift/Ctrl/Alt arrives as its own event, so we
 * can give a kitty client TRUE Down(KN_LSHIFT)-style hold tracking (not just
 * a per-keystroke bracket) by injecting the matching VK_SHIFT/CONTROL/MENU
 * press/release ourselves -- wwkeyboard.h's non-SDL VK space has no
 * separate Left/Right code (KN_LSHIFT==KN_RSHIFT==VK_SHIFT etc.), so both
 * sides fold to the same VK. */
static int kitty_mod_vk(int cp)
{
	switch (cp) {
		case 57441: case 57447: return VK_SHIFT;
		case 57442: case 57448: return VK_CONTROL;
		case 57443: case 57449: return VK_MENU;
	}
	return 0;
}

/* --- SyncTERM evdev physical-key reports ------------------------------------
 * Negotiated by door_io.c when the CTDA reply advertises cap 8 (enables
 * "\x1b[=1h\x1b[=2h": physical-key reports on, translated-byte stream off).
 * Reports arrive as "CSI = code[;code...] K" (press) / "...k" (release);
 * door_io_evdev_active() mirrors door_io.c's flag so a coincidental bare
 * 'K'/'k' from some other source still falls back to its CSI meaning
 * (End / ignored) instead of being misread as an evdev report. */
static unsigned g_evdev_mods;



/* Apply one physical key edge. Modifier codes update g_evdev_mods AND inject
 * a real VK_SHIFT/CONTROL/MENU press/release (true Down()-state hold, unlike
 * the legacy bracket) since evdev reports genuine press/release per key. */
static void evdev_edge(int code, int down)
{
	int            shift;
	unsigned short vk = 0;
	int            c;
	int            mod = termgfx_evdev_modifier(code);   /* L/R Ctrl, Shift, Alt */

	/* Modifier codes update g_evdev_mods AND inject a real VK_SHIFT/CONTROL/MENU
	 * press/release (true Down()-state hold, unlike the legacy bracket) since
	 * evdev reports genuine press/release per key. */
	if (mod != 0) {
		if (down)
			g_evdev_mods |= (unsigned)mod;
		else
			g_evdev_mods &= ~(unsigned)mod;
		emit_key(mod == TERMGFX_MOD_CTRL ? VK_CONTROL
		         : mod == TERMGFX_MOD_SHIFT ? VK_SHIFT : VK_MENU, !down);
		return;
	}

	/* SyncTERM re-reports the keys already held when physical reports are
	 * enabled -- the Enter still down from the BBS menu. Drop those press edges
	 * (termgfx/keymode.h); releases and the modifier tracking above always run,
	 * so nothing can stick. */
	if (down && termgfx_keymode_evdev_settling(door_io_keymode(), door_io_now_ms()))
		return;

	switch (code) {
		case 103: case 72: vk = VK_UP;    break;   /* Up (+ numpad alias, NumLock off) */
		case 108: case 80: vk = VK_DOWN;  break;
		case 105: case 75: vk = VK_LEFT;  break;
		case 106: case 77: vk = VK_RIGHT; break;
		case 102: vk = VK_HOME;   break;
		case 107: vk = VK_END;    break;
		case 104: vk = VK_PRIOR;  break;           /* Page Up */
		case 109: vk = VK_NEXT;   break;           /* Page Down */
		case 110: vk = VK_INSERT; break;
		case 111: vk = VK_DELETE; break;
		case 59:  vk = VK_F1;  break;
		case 60:  vk = VK_F2;  break;
		case 61:  vk = VK_F3;  break;
		case 62:  if (down)
				door_io_tier_cycle(); return;               /* F4: cycle tier on press only (never forwarded) */
		case 63:  vk = VK_F5;  break;
		case 64:  vk = VK_F6;  break;
		case 65:  vk = VK_F7;  break;
		case 66:  vk = VK_F8;  break;
		case 67:  vk = VK_F9;  break;
		case 68:  vk = VK_F10; break;
		case 87:  vk = VK_F11; break;
		case 88:  vk = VK_F12; break;
	}
	if (vk) {
		emit_key(vk, !down);
		return;
	}

	c = termgfx_evdev_ascii(code, (g_evdev_mods & TERMGFX_MOD_SHIFT) != 0);
	if (c == 0)
		return;
	if ((g_evdev_mods & TERMGFX_MOD_CTRL) && (c | 0x20) >= 'a' && (c | 0x20) <= 'z') {
		/* Door-level Ctrl+letter hotkeys (Ctrl-U/P/S/F/D) are consumed here and
		 * never forwarded to the game -- door_io.c's raw-byte branch only catches
		 * them in legacy mode, not evdev (SyncTERM). Fire on the press edge,
		 * swallow the release too (commit=down). */
		if (door_io_hotkey(c | 0x20, down))
			return;
		/* Ctrl+letter: the real Ctrl press already went out above (true
		 * hold), so just inject the plain letter -- Put_Key_Message's
		 * own Down(KN_LCTRL) OR's WWKEY_CTRL_BIT in. */
		vk = door_input_vk_from_ascii(c | 0x20, &shift);
		if (vk)
			emit_key(vk, !down);
		return;
	}
	vk = door_input_vk_from_ascii(c, &shift);
	if (vk == 0)
		return;
	if (shift && !(g_evdev_mods & TERMGFX_MOD_SHIFT)) {
		/* evdev already gives the correctly-shifted ASCII (the table above
		 * has both halves); a shift-needing VK with the modifier NOT
		 * currently down would be table/edge data disagreeing -- fall back
		 * to the legacy bracket rather than drop the key. */
		emit_tap_mod(vk, VK_SHIFT);
		return;
	}
	emit_key(vk, !down);
}

/* A physical key report ("CSI = code[;code...] K/k"): apply every coalesced
 * edge. door_in_csi_params() skips the leading '=' marker. */
static void evdev_report(int down)
{
	int codes[16], nc, i;
	nc = door_in_csi_params(codes, 16);
	for (i = 0; i < nc; i++)
		evdev_edge(codes[i], down);
}

/* --- CSI/SS3 final-byte dispatch -------------------------------------------
 * Runs on whatever door_io.c's own pump didn't already consume (probe
 * replies, F4, Ctrl-F never reach here -- door_io.c's door_csi_final()
 * handles R/t/c/n/S itself; everything else, including SS3, is forwarded
 * byte-for-byte via door_input_byte()). Legacy F3 (plain CSI/SS3 'R') is
 * deliberately NOT mapped here, matching syncduke_input.c's own choice: it
 * would collide with the DSR cursor-position report both doors also use
 * 'R' for. F3 still works correctly via kitty (CSI-u) or evdev (code 61). */
static void csi_final(char fin)
{
	int p[4];

	switch (fin) {
		case 'A': case 'B': case 'C': case 'D': {
			unsigned short vk = (fin == 'A') ? VK_UP : (fin == 'B') ? VK_DOWN
			                   : (fin == 'C') ? VK_RIGHT : VK_LEFT;
			if (door_input_kitty_active()) {
				int mod, ev;
				kitty_parse(&mod, &ev);
				(void)mod;   /* arrows carry no ctrl/alt action -- only the event type matters */
				emit_key(vk, ev == 3);
			} else
				emit_tap(vk);
			return;
		}
		case 'H': emit_tap(VK_HOME); return;
		case 'F': emit_tap(VK_END);  return;
		case 'V': emit_tap(VK_PRIOR); return;   /* cterm: PgUp */
		case 'U': emit_tap(VK_NEXT);  return;   /* cterm: PgDn */
		case 'K':                               /* evdev press report, else cterm End */
			if (door_io_evdev_active() && g_csi_len > 0 && g_csi_par[0] == '=') {
				evdev_report(1);
				return;
			}
			emit_tap(VK_END);
			return;
		case 'k':                               /* evdev release report */
			if (door_io_evdev_active() && g_csi_len > 0 && g_csi_par[0] == '=')
				evdev_report(0);
			return;
		case 'P':                               /* F1 (CSI P / SS3 O P) */
		case 'Q': {                              /* F2 (CSI Q / SS3 O Q) */
			unsigned short vk = (fin == 'P') ? VK_F1 : VK_F2;
			if (door_input_kitty_active()) {
				int mod, ev;
				kitty_parse(&mod, &ev);
				(void)mod;   /* F1/F2 carry no ctrl/alt action -- only the event type matters */
				emit_key(vk, ev == 3);
			} else
				emit_tap(vk);
			return;
		}
		case '~': {
			int            np = door_in_csi_params(p, 1);
			int            mod = 1, ev = 1;
			unsigned short vk = 0;
			if (door_input_kitty_active())
				kitty_parse(&mod, &ev);
			(void)mod;   /* nav/F-keys carry no ctrl/alt action -- only the event type matters */
			if (np < 1)
				return;
			switch (p[0]) {
				case 1: case 7:  vk = VK_HOME;   break;
				case 2:          vk = VK_INSERT; break;
				case 3:          vk = VK_DELETE; break;
				case 4: case 8:  vk = VK_END;    break;
				case 5:          vk = VK_PRIOR;  break;
				case 6:          vk = VK_NEXT;   break;
				case 11:         vk = VK_F1;     break;
				case 12:         vk = VK_F2;     break;
				case 13:         vk = VK_F3;     break;
				case 14:         door_io_tier_cycle(); return;   /* F4: cycle tier (rxvt ESC[14~) */
				case 15:         vk = VK_F5;     break;
				case 17:         vk = VK_F6;     break;
				case 18:         vk = VK_F7;     break;
				case 19:         vk = VK_F8;     break;
				case 20:         vk = VK_F9;     break;
				case 21:         vk = VK_F10;    break;
				case 23:         vk = VK_F11;    break;
				case 24:         vk = VK_F12;    break;
				default:         return;
			}
			if (door_input_kitty_active())
				emit_key(vk, ev == 3);
			else
				emit_tap(vk);
			return;
		}
		case 'u':
			/* CSI?u negotiation reply: enable the protocol and push our
			 * flags (1 disambiguate | 2 event types | 8 all keys incl.
			 * modifiers). door_io.c has no way to see this reply itself
			 * ('u' isn't one of its own probe finals), so it's on us to
			 * both detect it and answer it (door_io_send(): we have no
			 * socket of our own). The push itself, and the evdev-wins
			 * guard, live in termgfx (keymode.h). */
			if (g_csi_len > 0 && g_csi_par[0] == '?') {
				char   ks[TERMGFX_KEYMODE_SEQ_MAX];
				size_t kn = termgfx_keymode_enable_kitty(door_io_keymode(),
				                                         ks, sizeof ks);

				if (kn > 0)
					door_io_send(ks, kn);   /* a no-op if evdev already won */
				return;
			}
			{
				int            mod, ev, cp = kitty_parse(&mod, &ev), modvk, shift;
				unsigned short vk;
				if (cp <= 0)
					return;
				modvk = kitty_mod_vk(cp);
				if (modvk) {
					emit_key((unsigned short)modvk, ev == 3);
					return;
				}
				cp = kitty_kp_normalize(cp);
				if (cp >= 57399 && cp <= 57414) {   /* keypad PUA -> ASCII */
					static const char kp[] = "0123456789./*-+\r";
					cp = (unsigned char)kp[cp - 57399];
				}
				if (cp >= 128)
					return;   /* other non-ASCII special key: not in the map */
				/* Door-level Ctrl+letter hotkeys (Ctrl-U/P/S/F/D). kitty carries
				 * Ctrl as its own modifier event, so the letter reaches here with
				 * the base codepoint and termgfx_kitty_ctrl(mod) set; consume it
				 * across all edges (fire on press, ev==1) so a held repeat can't
				 * leak the letter to the game. Mirrors the evdev path and
				 * door_io.c's raw byte. */
				if (termgfx_kitty_ctrl(mod)
				    && (cp | 0x20) >= 'a' && (cp | 0x20) <= 'z'
				    && door_io_hotkey(cp | 0x20, ev == 1))
					return;
				vk = door_input_vk_from_ascii(cp, &shift);
				if (vk == 0)
					return;
				/* kitty's own codepoint already reflects shift (e.g. 'A' vs
				 * 'a'); shift isn't consulted further -- Down(KN_LSHIFT) only
				 * matters for a legacy bracket tap, not a real kitty event. */
				(void)shift;
				emit_key(vk, ev == 3);
			}
			return;
		case 'M': case 'm':                     /* xterm SGR mouse */
			if (g_csi_len > 0 && g_csi_par[0] == '<' && door_in_csi_params(p, 3) >= 3)
				mouse_event(p[0], p[1], p[2], fin == 'm');
			return;
		default:
			return;   /* other/unknown sequences: ignore */
	}
}

/* --- byte-level drain: door_io.c's ring -> csi_final()/plain bytes --------- */
static enum { DIN_NORMAL, DIN_ESC, DIN_CSI } g_pstate;

void door_input_drain(void)
{
	uint8_t b;

	while (door_io_input_pop(&b)) {
		switch (g_pstate) {
			case DIN_NORMAL:
				if (b == 0x1b)
					g_pstate = DIN_ESC;
				else
					door_input_key_tap(b);
				break;
			case DIN_ESC:
				if (b == '[' || b == 'O') {
					g_pstate  = DIN_CSI;
					g_csi_len = 0;
				} else {
					emit_tap(VK_ESCAPE);
					g_pstate = DIN_NORMAL;
					if (b == 0x1b)
						g_pstate = DIN_ESC;    /* ESC ESC ...: stay armed */
					else
						door_input_key_tap(b);
				}
				break;
			case DIN_CSI:
				if (b >= 0x40 && b <= 0x7e) {
					csi_final((char)b);
					g_pstate = DIN_NORMAL;
				} else if (g_csi_len < (int)sizeof g_csi_par) {
					g_csi_par[g_csi_len++] = (char)b;
				}
				break;
		}
	}
}
