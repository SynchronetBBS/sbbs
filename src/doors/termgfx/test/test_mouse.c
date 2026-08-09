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

	/* Ps=3 (permanently set) also latches pixel mode */
	termgfx_mouse_t m3 = {0};
	int             permaset[2] = {1016, 3};
	termgfx_mouse_on_decrpm(&m3, permaset, 2);
	assert(termgfx_mouse_pixels(&m3) == 1);

	/* auto-detect latch is idempotent */
	termgfx_mouse_t m2 = {0};
	termgfx_mouse_note_pixel_report(&m2);
	assert(termgfx_mouse_pixels(&m2) == 1);
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
	termgfx_mouse_report(&m2, 65, 1, 1, 0, &r);
	assert(r.kind == TERMGFX_SGR_WHEEL && r.wheel == 1);

	/* 96 and 97 are BOTH pointer motion. 97 was briefly decoded as
	   wheel-down-with-motion; what that produced was a phantom scroll on
	   every pointer movement, so a door's selection changed whenever the
	   mouse moved and was correct only while the hand was held still.
	   A live capture settled it: 1492 clean 64/65 detents against 16 of
	   97, several of them 6-8 ms apart with the coordinates walking. */
	termgfx_mouse_report(&m2, 97, 1, 1, 0, &r);
	assert(r.kind == TERMGFX_SGR_MOVE);
	termgfx_mouse_report(&m2, 97 | 4, 1, 1, 0, &r); /* ...with shift held */
	assert(r.kind == TERMGFX_SGR_MOVE);
	termgfx_mouse_report(&m2, 96, 1, 1, 0, &r);
	assert(r.kind == TERMGFX_SGR_MOVE);

	/* And a real detent is still a detent. */
	termgfx_mouse_report(&m2, 64, 1, 1, 0, &r);
	assert(r.kind == TERMGFX_SGR_WHEEL && r.wheel == -1);
	termgfx_mouse_report(&m2, 65, 1, 1, 0, &r);
	assert(r.kind == TERMGFX_SGR_WHEEL && r.wheel == 1);

	/* Drags keep their buttons -- 32|btn must not be read as a wheel. */
	termgfx_mouse_report(&m2, 32, 1, 1, 0, &r);
	assert(r.kind == TERMGFX_SGR_MOVE);
	termgfx_mouse_report(&m2, 33, 1, 1, 0, &r);
	assert(r.kind == TERMGFX_SGR_MOVE);
	termgfx_mouse_report(&m2, 34, 1, 1, 0, &r);
	assert(r.kind == TERMGFX_SGR_MOVE);

	return 0;
}
