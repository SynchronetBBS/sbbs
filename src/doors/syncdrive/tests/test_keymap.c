/* Unit tests for keymap.c: termgfx key events -> BIOS key words. */
#include <assert.h>
#include <string.h>
#include "keymap.h"

static keymap_result_t key(int type, int keycode, int ascii, int mods)
{
	termgfx_input_event_t ev;

	memset(&ev, 0, sizeof ev);
	ev.type    = type;
	ev.keycode = keycode;
	ev.ascii   = ascii;
	ev.mods    = mods;
	return keymap_translate(&ev);
}

#define DOWN(k, a, m) key(TERMGFX_EV_KEY_DOWN, (k), (a), (m))
#define UP(k, a, m)   key(TERMGFX_EV_KEY_UP, (k), (a), (m))

int main(void)
{
	keymap_result_t r;

	/* Cursor block: key word and held-key scan code. */
	r = DOWN(TERMGFX_KEY_UP, 0, 0);
	assert(r.bios == 0x4800 && r.xt == 0x48 && r.down == 1);
	r = DOWN(TERMGFX_KEY_LEFT, 0, 0);
	assert(r.bios == 0x4B00 && r.xt == 0x4B);
	r = DOWN(TERMGFX_KEY_RIGHT, 0, 0);
	assert(r.bios == 0x4D00 && r.xt == 0x4D);
	r = DOWN(TERMGFX_KEY_DOWN, 0, 0);
	assert(r.bios == 0x5000 && r.xt == 0x50);
	r = DOWN(TERMGFX_KEY_HOME, 0, 0);
	assert(r.bios == 0x4700 && r.xt == 0x47);
	r = DOWN(TERMGFX_KEY_PAGEUP, 0, 0);
	assert(r.bios == 0x4900 && r.xt == 0x49);
	r = DOWN(TERMGFX_KEY_END, 0, 0);
	assert(r.bios == 0x4F00 && r.xt == 0x4F);
	r = DOWN(TERMGFX_KEY_PAGEDOWN, 0, 0);
	assert(r.bios == 0x5100 && r.xt == 0x51);
	r = DOWN(TERMGFX_KEY_KP5, 0, 0);
	assert(r.bios == 0x4C00 && r.xt == 0);

	/* A release carries only the scan code, for the held-key table. */
	r = UP(TERMGFX_KEY_UP, 0, 0);
	assert(r.bios == 0 && r.xt == 0x48 && r.down == 0 && r.action == KEYMAP_ACT_NONE);

	/* A and Z shift gears; both cases, held scan codes. */
	r = DOWN('a', 'a', 0);
	assert(r.bios == 0x1E61 && r.xt == 0x1E);
	r = DOWN('A', 'A', TERMGFX_MOD_SHIFT);
	assert(r.bios == 0x1E41 && r.xt == 0x1E);
	r = DOWN('z', 'z', 0);
	assert(r.bios == 0x2C7A && r.xt == 0x2C);
	r = DOWN('q', 'q', 0);
	assert(r.bios == 0x1071 && r.xt == 0);

	/* Menu and text-entry keys. */
	assert(DOWN(TERMGFX_KEY_ENTER, 0, 0).bios == 0x1C0D);
	assert(DOWN(TERMGFX_KEY_ESCAPE, 0, 0).bios == 0x011B);
	assert(DOWN(TERMGFX_KEY_BACKSPACE, 0, 0).bios == 0x0E08);
	assert(DOWN(TERMGFX_KEY_TAB, 0, 0).bios == 0x0F09);
	assert(DOWN(TERMGFX_KEY_INSERT, 0, 0).bios == 0x5200);
	assert(DOWN(TERMGFX_KEY_DELETE, 0, 0).bios == 0x5300);
	assert(DOWN(' ', ' ', 0).bios == 0x3920);
	assert(DOWN('1', '1', 0).bios == 0x0231);
	assert(DOWN('0', '0', 0).bios == 0x0B30);
	assert(DOWN('!', '!', TERMGFX_MOD_SHIFT).bios == 0x0221);
	assert(DOWN('.', '.', 0).bios == 0x342E);
	assert(DOWN('?', '?', TERMGFX_MOD_SHIFT).bios == 0x353F);
	assert(DOWN(TERMGFX_KEY_F1, 0, 0).bios == 0x3B00);
	assert(DOWN(TERMGFX_KEY_F9, 0, 0).bios == 0x4300);

	/* Ctrl letters: the game's Ctrl-P pause and Ctrl-K pass through. */
	assert(DOWN('p', 0, TERMGFX_MOD_CTRL).bios == 0x1910);
	assert(DOWN('k', 0, TERMGFX_MOD_CTRL).bios == 0x250B);

	/* Door keys: F2 sound toggle, Ctrl-F fit toggle; neither reaches the game. */
	r = DOWN(TERMGFX_KEY_F2, 0, 0);
	assert(r.bios == 0 && r.action == KEYMAP_ACT_SOUND_TOGGLE);
	r = DOWN('f', 0, TERMGFX_MOD_CTRL);
	assert(r.bios == 0 && r.action == KEYMAP_ACT_FIT_CYCLE);
	r = UP(TERMGFX_KEY_F2, 0, 0);
	assert(r.action == KEYMAP_ACT_NONE);

	/* Ctrl-J (joystick mode) is dropped. */
	r = DOWN('j', 0, TERMGFX_MOD_CTRL);
	assert(r.bios == 0 && r.action == KEYMAP_ACT_NONE);

	/* Mouse events are not keys. */
	r = key(TERMGFX_EV_MOUSE_DOWN, 0, 0, 0);
	assert(r.bios == 0 && r.xt == 0 && r.action == KEYMAP_ACT_NONE);

	/* The sound toggle sends the game's own Ctrl-Q (off) or Ctrl-S (on). */
	assert(keymap_sound_toggle_key(1) == 0x1011);
	assert(keymap_sound_toggle_key(0) == 0x1F13);
	return 0;
}
