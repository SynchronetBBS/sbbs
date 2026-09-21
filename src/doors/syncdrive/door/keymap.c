/* keymap.c -- see keymap.h. Key words follow upstream host.c bios_key(). */
#include <string.h>
#include "keymap.h"

static const uint8_t letter_scan[26] = {
	0x1E, 0x30, 0x2E, 0x20, 0x12, 0x21, 0x22, 0x23, 0x17, 0x24, 0x25, 0x26, 0x32,
	0x31, 0x18, 0x19, 0x10, 0x13, 0x1F, 0x14, 0x16, 0x2F, 0x11, 0x2D, 0x15, 0x2C
};

/* Printable non-letters, both shift states, by the character received. */
static const struct {
	char ch;
	uint8_t scan;
} char_scan[] = {
	{ '1', 0x02 }, { '!', 0x02 }, { '2', 0x03 }, { '@', 0x03 }, { '3', 0x04 }, { '#', 0x04 },
	{ '4', 0x05 }, { '$', 0x05 }, { '5', 0x06 }, { '%', 0x06 }, { '6', 0x07 }, { '^', 0x07 },
	{ '7', 0x08 }, { '&', 0x08 }, { '8', 0x09 }, { '*', 0x09 }, { '9', 0x0A }, { '(', 0x0A },
	{ '0', 0x0B }, { ')', 0x0B }, { '-', 0x0C }, { '_', 0x0C }, { '=', 0x0D }, { '+', 0x0D },
	{ '[', 0x1A }, { '{', 0x1A }, { ']', 0x1B }, { '}', 0x1B }, { ';', 0x27 }, { ':', 0x27 },
	{ '\'', 0x28 }, { '"', 0x28 }, { '`', 0x29 }, { '~', 0x29 }, { '\\', 0x2B }, { '|', 0x2B },
	{ ',', 0x33 }, { '<', 0x33 }, { '.', 0x34 }, { '>', 0x34 }, { '/', 0x35 }, { '?', 0x35 },
	{ ' ', 0x39 }
};

static void special(keymap_result_t *r, int keycode)
{
	switch (keycode) {
		case TERMGFX_KEY_UP:        r->bios = 0x4800; r->xt = 0x48; break;
		case TERMGFX_KEY_DOWN:      r->bios = 0x5000; r->xt = 0x50; break;
		case TERMGFX_KEY_LEFT:      r->bios = 0x4B00; r->xt = 0x4B; break;
		case TERMGFX_KEY_RIGHT:     r->bios = 0x4D00; r->xt = 0x4D; break;
		case TERMGFX_KEY_HOME:      r->bios = 0x4700; r->xt = 0x47; break;
		case TERMGFX_KEY_END:       r->bios = 0x4F00; r->xt = 0x4F; break;
		case TERMGFX_KEY_PAGEUP:    r->bios = 0x4900; r->xt = 0x49; break;
		case TERMGFX_KEY_PAGEDOWN:  r->bios = 0x5100; r->xt = 0x51; break;
		case TERMGFX_KEY_KP5:       r->bios = 0x4C00; r->xt = 0x4C; break;
		case TERMGFX_KEY_INSERT:    r->bios = 0x5200; break;
		case TERMGFX_KEY_DELETE:    r->bios = 0x5300; break;
		case TERMGFX_KEY_ENTER:     r->bios = 0x1C0D; break;
		case TERMGFX_KEY_ESCAPE:    r->bios = 0x011B; break;
		case TERMGFX_KEY_BACKSPACE: r->bios = 0x0E08; break;
		case TERMGFX_KEY_TAB:       r->bios = 0x0F09; break;
		case TERMGFX_KEY_F1:        r->action = KEYMAP_ACT_HELP; break;
		case TERMGFX_KEY_F2:        r->action = KEYMAP_ACT_SOUND_TOGGLE; break;
		default:
			if (keycode >= TERMGFX_KEY_F1 && keycode <= TERMGFX_KEY_F9)
				r->bios = (uint16_t)((0x3B + (keycode - TERMGFX_KEY_F1)) << 8);
			break;
	}
}

static void printable(keymap_result_t *r, int keycode, int ascii, int mods)
{
	int    lower = (keycode >= 'A' && keycode <= 'Z') ? keycode + ('a' - 'A') : keycode;
	size_t k;

	if (lower >= 'a' && lower <= 'z') {
		int     i    = lower - 'a';
		uint8_t scan = letter_scan[i];

		if (lower == 'a' || lower == 'z')
			r->xt = scan;
		if (mods & TERMGFX_MOD_CTRL) {
			if (lower == 'f')
				r->action = KEYMAP_ACT_FIT_CYCLE;
			else if (lower == 'k')
				r->action = KEYMAP_ACT_HELP;
			else if (lower != 'j')
				r->bios = (uint16_t)(scan << 8 | (i + 1));
			return;
		}
		if (mods & TERMGFX_MOD_ALT) {
			r->bios = (uint16_t)(scan << 8);
			return;
		}
		r->bios = (uint16_t)(scan << 8 | (uint8_t)(ascii ? ascii : keycode));
		return;
	}
	for (k = 0; k < sizeof char_scan / sizeof char_scan[0]; k++) {
		if (char_scan[k].ch == keycode) {
			r->bios = (uint16_t)(char_scan[k].scan << 8 | (uint8_t)keycode);
			return;
		}
	}
}

keymap_result_t keymap_translate(const termgfx_input_event_t *ev)
{
	keymap_result_t r;

	memset(&r, 0, sizeof r);
	if (ev->type != TERMGFX_EV_KEY_DOWN && ev->type != TERMGFX_EV_KEY_UP)
		return r;
	r.down = ev->type == TERMGFX_EV_KEY_DOWN;
	if (ev->keycode >= TERMGFX_KEY_FIRST)
		special(&r, ev->keycode);
	else
		printable(&r, ev->keycode, ev->ascii, ev->mods);
	if (!r.down) {
		r.bios   = 0;
		r.action = KEYMAP_ACT_NONE;
	}
	return r;
}

uint16_t keymap_sound_toggle_key(int sound_on)
{
	return sound_on ? 0x1011 : 0x1F13;
}
