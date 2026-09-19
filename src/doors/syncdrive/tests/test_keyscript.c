/* Unit tests for keyscript.c: the SYNCDRIVE_KEYS headless key script. */
#include <assert.h>
#include "keyscript.h"

int main(void)
{
	keyscript_step_t s[16];
	int              n;

	n = keyscript_parse("wait:1500,enter,right,a,ctrl-p,f2,space,quit", s, 16);
	assert(n == 8);
	assert(s[0].kind == KEYSCRIPT_WAIT && s[0].ms == 1500);
	assert(s[1].kind == KEYSCRIPT_KEY && s[1].ev.keycode == TERMGFX_KEY_ENTER);
	assert(s[1].ev.type == TERMGFX_EV_KEY_DOWN);
	assert(s[2].kind == KEYSCRIPT_KEY && s[2].ev.keycode == TERMGFX_KEY_RIGHT);
	assert(s[3].ev.keycode == 'a' && s[3].ev.ascii == 'a');
	assert(s[4].ev.keycode == 'p' && s[4].ev.mods == TERMGFX_MOD_CTRL && s[4].ev.ascii == 0);
	assert(s[5].ev.keycode == TERMGFX_KEY_F2);
	assert(s[6].ev.keycode == ' ' && s[6].ev.ascii == ' ');
	assert(s[7].kind == KEYSCRIPT_QUIT);

	assert(keyscript_parse("", s, 16) == 0);
	assert(keyscript_parse("esc,up,down,left,home,end,pgup,pgdn", s, 16) == 8);
	assert(s[0].ev.keycode == TERMGFX_KEY_ESCAPE);
	assert(s[7].ev.keycode == TERMGFX_KEY_PAGEDOWN);

	/* Errors: unknown token, bad wait, overflow. */
	assert(keyscript_parse("bogus", s, 16) == -1);
	assert(keyscript_parse("wait:x", s, 16) == -1);
	assert(keyscript_parse("a,b,c", s, 2) == -1);
	return 0;
}
