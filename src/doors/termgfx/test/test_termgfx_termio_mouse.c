/* Unit test for termgfx_termio's SGR mouse mapper: drives termgfx_termio_mouse_report() through
 * the TERMGFX_TEST seams (termgfx_termio_test_set_geom()/termgfx_termio_test_mouse_report()) and
 * reads back the queued input events via termgfx_termio_next_event() -- no socket,
 * no termgfx_termio_init(). cc'd (with -DTERMGFX_TEST) + run by unit_termgfx_termio.sh. */
#include <assert.h>
#include "termgfx_termio.h"
int termgfx_termio_test_set_geom(int canvas_w, int canvas_h, int cell_w, int cell_h,
                         int cols, int rows, int pixels);
int termgfx_termio_test_mouse_report(int b, int col, int row, int release);

int main(void) {
	termgfx_input_event_t ev;

	/* Cell mode: 640x400 canvas, 8x16 cell, 80x25 grid, image fills canvas.
	 * A click at the center cell maps near the center of the 320x200 game. */
	termgfx_termio_test_set_geom(640, 400, 8, 16, 80, 25, /*pixels=*/0);
	termgfx_termio_test_mouse_report(/*b=*/0, /*col=*/40, /*row=*/13, /*release=*/0);
	assert(termgfx_termio_next_event(&ev) && ev.type == TERMGFX_EV_MOUSE_MOVE);
	assert(ev.x > 140 && ev.x < 180 && ev.y > 80 && ev.y < 120);
	assert(termgfx_termio_next_event(&ev) && ev.type == TERMGFX_EV_MOUSE_DOWN && ev.button == 0);
	assert(!termgfx_termio_next_event(&ev));   /* queue drained */

	/* Pixel mode: report already in canvas pixels. Top-left maps to (0,0). */
	termgfx_termio_test_set_geom(1280, 800, 8, 16, 160, 50, /*pixels=*/1);
	termgfx_termio_test_mouse_report(0, 1, 1, 0);
	assert(termgfx_termio_next_event(&ev) && ev.type == TERMGFX_EV_MOUSE_MOVE && ev.x == 0 && ev.y == 0);

	/* Auto-detect: cell-mode geom but a report past the grid latches pixels. */
	termgfx_termio_test_set_geom(1280, 800, 8, 16, 160, 50, /*pixels=*/0);
	termgfx_termio_test_mouse_report(35, 900, 400, 0);   /* col 900 > 160 cols → pixels */
	assert(termgfx_termio_next_event(&ev) && ev.type == TERMGFX_EV_MOUSE_MOVE);
	/* now interpreted as pixels: x ≈ 900/1280*320 */
	assert(ev.x > 210 && ev.x < 240);

	/* Cell-mode EDGE reachability. In cell mode the reported coordinate is a
	 * text cell whose CENTER sits cw/2 or ch/2 px inside each canvas edge, so
	 * the first and last game rows/cols must snap out to the image edges or
	 * they are unreachable. The first row reaching game y=0 is what makes
	 * Beneath a Steel Sky's top inventory bar drop -- before the snap it
	 * mapped to ~y=4 and the bar never appeared. Motion-only reports (b=32),
	 * so each queues just a MOVE; drain any leftover first. */
	while (termgfx_termio_next_event(&ev)) { }
	termgfx_termio_test_set_geom(640, 400, 8, 16, 80, 25, /*pixels=*/0);
	termgfx_termio_test_mouse_report(/*b=*/32, /*col=*/1, /*row=*/1, /*release=*/0);
	assert(termgfx_termio_next_event(&ev) && ev.type == TERMGFX_EV_MOUSE_MOVE && ev.x == 0 && ev.y == 0);
	assert(!termgfx_termio_next_event(&ev));
	termgfx_termio_test_mouse_report(/*b=*/32, /*col=*/80, /*row=*/25, /*release=*/0);
	assert(termgfx_termio_next_event(&ev) && ev.type == TERMGFX_EV_MOUSE_MOVE && ev.x > 300 && ev.y > 190);
	assert(!termgfx_termio_next_event(&ev));
	return 0;
}
