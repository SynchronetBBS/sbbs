#include <assert.h>
#include <string.h>
#include "mouse.h"

int main(void) {
	termgfx_mouse_t m = {0};

	/* enable/restore strings are exactly the DEC modes syncconquer sends */
	assert(strcmp(termgfx_mouse_enable,  "\x1b[?1003h\x1b[?1006h\x1b[?1016h\x1b[?1016$p") == 0);
	assert(strcmp(termgfx_mouse_restore, "\x1b[?1003l\x1b[?1006l\x1b[?1016l") == 0);

	/* starts in cell mode */
	assert(termgfx_mouse_pixels(&m) == 0);

	/* DECRPM confirming 1016 set (Ps=1) latches pixel mode; Ps=2 (reset) does not */
	int reset[2] = {1016, 2};
	termgfx_mouse_on_decrpm(&m, reset, 2);
	assert(termgfx_mouse_pixels(&m) == 0);
	int set[2] = {1016, 1};
	termgfx_mouse_on_decrpm(&m, set, 2);
	assert(termgfx_mouse_pixels(&m) == 1);

	/* auto-detect latch is idempotent */
	termgfx_mouse_t m2 = {0};
	termgfx_mouse_note_pixel_report(&m2);
	assert(termgfx_mouse_pixels(&m2) == 1);

	/* classify: left press, hover (motion), wheel up */
	termgfx_mouse_report_t r;
	termgfx_mouse_report(&m2, 0, 10, 20, 0, &r);
	assert(r.kind == TERMGFX_SGR_BUTTON && r.button == 0 && r.col == 10 && r.row == 20 && r.pixels == 1);
	termgfx_mouse_report(&m2, 35, 10, 20, 0, &r);   /* xterm no-button hover */
	assert(r.kind == TERMGFX_SGR_MOVE);
	termgfx_mouse_report(&m2, 64, 1, 1, 0, &r);
	assert(r.kind == TERMGFX_SGR_WHEEL && r.wheel == -1);

	return 0;
}
