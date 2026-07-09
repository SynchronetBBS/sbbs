// keymode.c -- terminal keyboard-mode negotiation. See keymode.h: I/O-free, the
// door emits the sequences these build and owns its own key->action map.
//
// Carved out of the four copies in syncdoom.c, syncduke_input.c,
// syncconquer/door/door_input.c and syncretro_input.c. The evdev->ASCII table
// below is byte-for-byte the one all four carried.

#include "keymode.h"

#include <string.h>

// CSI ? u -- "do you speak the kitty keyboard protocol?"
const char *const termgfx_keymode_query_kitty = "\x1b[?u";

// CSI = 1 h  physical key reports on (cterm.adoc)
// CSI = 2 h  suppress the translated byte stream, so a key arrives exactly once
static const char evdev_on[]  = "\x1b[=1h\x1b[=2h";
// Reset in the reverse order: restore translation, then stop the reports.
static const char evdev_off[] = "\x1b[=2l\x1b[=1l";

// CSI > 11 u  push flags: 1 disambiguate | 2 report event types | 8 report all
//                         keys as escape codes.  Bit 2 is the load-bearing one:
//                         it is what turns a key RELEASE into an event.
static const char kitty_on[]  = "\x1b[>11u";
// CSI < u  pop exactly the flags we pushed (kitty keeps a stack).
static const char kitty_off[] = "\x1b[<u";

// Copy a NUL-terminated literal into the caller's buffer. Returns the length, or
// 0 if it wouldn't fit -- callers size with TERMGFX_KEYMODE_SEQ_MAX, so a 0 here
// means a caller bug, and emitting nothing is the safe answer either way.
static size_t emit(char *out, size_t sz, const char *s)
{
	size_t n = strlen(s);

	if (out == NULL || sz < n + 1)
		return 0;
	memcpy(out, s, n + 1);
	return n;
}

int termgfx_keymode_kitty_active(const termgfx_keymode_t *km) { return km->kitty; }
int termgfx_keymode_evdev_active(const termgfx_keymode_t *km) { return km->evdev; }

size_t termgfx_keymode_enable_evdev(termgfx_keymode_t *km, char *out, size_t sz,
                                    uint32_t now_ms)
{
	size_t n;

	if (km->evdev || km->kitty)
		return 0;
	n = emit(out, sz, evdev_on);
	if (n == 0)
		return 0;
	km->evdev      = 1;
	km->enabled_ms = now_ms;
	km->settling   = 1;
	return n;
}

size_t termgfx_keymode_enable_kitty(termgfx_keymode_t *km, char *out, size_t sz)
{
	size_t n;

	if (km->evdev || km->kitty)   // evdev wins: it is strictly better and SyncTERM-only
		return 0;
	n = emit(out, sz, kitty_on);
	if (n == 0)
		return 0;
	km->kitty = 1;
	return n;
}

size_t termgfx_keymode_restore(termgfx_keymode_t *km, char *out, size_t sz)
{
	size_t n = 0;

	// At most one of these is ever set, but both are handled so the restore is
	// total: whatever we turned on, we turn off.
	if (km->kitty) {
		n = emit(out, sz, kitty_off);
		km->kitty = 0;
	} else if (km->evdev) {
		n = emit(out, sz, evdev_off);
		km->evdev = 0;
	}
	km->settling = 0;
	return n;
}

int termgfx_keymode_evdev_settling(termgfx_keymode_t *km, uint32_t now_ms)
{
	if (!km->evdev || !km->settling)
		return 0;
	if ((uint32_t)(now_ms - km->enabled_ms) < TERMGFX_KEYMODE_SETTLE_MS)
		return 1;                 // the terminal's held-key resync: drop this press
	km->settling = 0;             // window closed; every later press is a real one
	return 0;
}

// --- kitty CSI-u parameter decode -------------------------------------------

int termgfx_kitty_parse(const char *par, int len, int *mod, int *ev)
{
	int key = 0, v = 0, sub = 0, field = 0, in_sub = 0, i;

	*mod = 1;
	*ev  = 1;
	for (i = 0; i <= len; i++) {
		char c = (i < len) ? par[i] : ';';   // sentinel flushes the last field

		if (c >= '0' && c <= '9') {
			if (in_sub)
				sub = sub * 10 + (c - '0');
			else
				v = v * 10 + (c - '0');
		} else if (c == ':') {
			in_sub = 1;                      // a field's first sub-param is the event type
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

// kitty's modifier field is 1-based: bit 0 Shift, bit 1 Alt, bit 2 Ctrl.
int termgfx_kitty_shift(int mod) { return ((mod - 1) & 1) != 0; }
int termgfx_kitty_alt(int mod)   { return ((mod - 1) & 2) != 0; }
int termgfx_kitty_ctrl(int mod)  { return ((mod - 1) & 4) != 0; }

// --- evdev keycode decode ----------------------------------------------------

// US-QWERTY evdev keycode -> { unshifted, shifted }. 0 = no simple ASCII form.
static const char evdev_ascii[58][2] = {
	{ 0, 0 },                                                                                /* 0  reserved */
	{ 27, 27 },                                                                              /* 1  ESC */
	{ '1', '!' }, { '2', '@' }, { '3', '#' }, { '4', '$' }, { '5', '%' },                    /* 2-6 */
	{ '6', '^' }, { '7', '&' }, { '8', '*' }, { '9', '(' }, { '0', ')' },                    /* 7-11 */
	{ '-', '_' }, { '=', '+' },                                                              /* 12-13 */
	{ 8, 8 }, { 9, 9 },                                                                      /* 14 BkSp, 15 Tab */
	{ 'q', 'Q' }, { 'w', 'W' }, { 'e', 'E' }, { 'r', 'R' }, { 't', 'T' }, { 'y', 'Y' },      /* 16-21 */
	{ 'u', 'U' }, { 'i', 'I' }, { 'o', 'O' }, { 'p', 'P' },                                  /* 22-25 */
	{ '[', '{' }, { ']', '}' },                                                              /* 26-27 */
	{ '\r', '\r' },                                                                          /* 28 Enter */
	{ 0, 0 },                                                                                /* 29 LeftCtrl */
	{ 'a', 'A' }, { 's', 'S' }, { 'd', 'D' }, { 'f', 'F' }, { 'g', 'G' },                    /* 30-34 */
	{ 'h', 'H' }, { 'j', 'J' }, { 'k', 'K' }, { 'l', 'L' },                                  /* 35-38 */
	{ ';', ':' }, { '\'', '"' }, { '`', '~' },                                               /* 39-41 */
	{ 0, 0 },                                                                                /* 42 LeftShift */
	{ '\\', '|' },                                                                           /* 43 */
	{ 'z', 'Z' }, { 'x', 'X' }, { 'c', 'C' }, { 'v', 'V' }, { 'b', 'B' },                    /* 44-48 */
	{ 'n', 'N' }, { 'm', 'M' },                                                              /* 49-50 */
	{ ',', '<' }, { '.', '>' }, { '/', '?' },                                                /* 51-53 */
	{ 0, 0 },                                                                                /* 54 RightShift */
	{ '*', '*' },                                                                            /* 55 KP* */
	{ 0, 0 },                                                                                /* 56 LeftAlt */
	{ ' ', ' ' },                                                                            /* 57 Space */
};

char termgfx_evdev_ascii(int code, int shifted)
{
	if (code < 0 || code >= (int)(sizeof evdev_ascii / sizeof evdev_ascii[0]))
		return 0;
	return evdev_ascii[code][shifted ? 1 : 0];
}

int termgfx_evdev_modifier(int code)
{
	switch (code) {
		case 29: case 97:  return TERMGFX_MOD_CTRL;    // L/R Ctrl
		case 42: case 54:  return TERMGFX_MOD_SHIFT;   // L/R Shift
		case 56: case 100: return TERMGFX_MOD_ALT;     // L/R Alt
		default:           return 0;
	}
}
