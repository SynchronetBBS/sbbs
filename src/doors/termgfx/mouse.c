#include "mouse.h"

const char *const termgfx_mouse_enable  = "\x1b[?1003h\x1b[?1006h\x1b[?1016h\x1b[?1016$p";
const char *const termgfx_mouse_restore = "\x1b[?1003l\x1b[?1006l\x1b[?1016l";

void termgfx_mouse_on_decrpm(termgfx_mouse_t *m, const int *params, int n)
{
	// ESC[?1016;Ps$y -- Ps: 0 not recognized, 1 set, 2 reset, 3 permanently set,
	// 4 permanently reset. Latch pixel mode only when 1016 is (permanently) set.
	if (n >= 2 && params[0] == 1016 && (params[1] == 1 || params[1] == 3))
		m->pixels = 1;
}

void termgfx_mouse_note_pixel_report(termgfx_mouse_t *m)
{
	m->pixels = 1;
}

int termgfx_mouse_pixels(const termgfx_mouse_t *m)
{
	return m->pixels;
}

void termgfx_mouse_report(termgfx_mouse_t *m, int b, int col, int row,
                          int release, termgfx_mouse_report_t *out)
{
	(void)release;
	out->kind   = termgfx_sgr_classify(b, &out->button, &out->wheel);
	out->col    = col;
	out->row    = row;
	out->pixels = m->pixels;
}
