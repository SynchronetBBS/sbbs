/* Unit test for sst_io's SGR mouse mapper: drives sst_mouse_report() through
 * the SST_TEST seams (sst_io_test_set_geom()/sst_io_test_mouse_report()) and
 * reads back the queued input events via sst_io_next_event() -- no socket,
 * no sst_io_init(). cc'd (with -DSST_TEST) + run by unit_sst_io.sh. */
#include <assert.h>
#include "sst_io.h"
int sst_io_test_set_geom(int canvas_w, int canvas_h, int cell_w, int cell_h,
                         int cols, int rows, int pixels);
int sst_io_test_mouse_report(int b, int col, int row, int release);

int main(void) {
	sst_input_event_t ev;

	/* Cell mode: 640x400 canvas, 8x16 cell, 80x25 grid, image fills canvas.
	 * A click at the center cell maps near the center of the 320x200 game. */
	sst_io_test_set_geom(640, 400, 8, 16, 80, 25, /*pixels=*/0);
	sst_io_test_mouse_report(/*b=*/0, /*col=*/40, /*row=*/13, /*release=*/0);
	assert(sst_io_next_event(&ev) && ev.type == SST_EV_MOUSE_MOVE);
	assert(ev.x > 140 && ev.x < 180 && ev.y > 80 && ev.y < 120);
	assert(sst_io_next_event(&ev) && ev.type == SST_EV_MOUSE_DOWN && ev.button == 0);
	assert(!sst_io_next_event(&ev));   /* queue drained */

	/* Pixel mode: report already in canvas pixels. Top-left maps to (0,0). */
	sst_io_test_set_geom(1280, 800, 8, 16, 160, 50, /*pixels=*/1);
	sst_io_test_mouse_report(0, 1, 1, 0);
	assert(sst_io_next_event(&ev) && ev.type == SST_EV_MOUSE_MOVE && ev.x == 0 && ev.y == 0);

	/* Auto-detect: cell-mode geom but a report past the grid latches pixels. */
	sst_io_test_set_geom(1280, 800, 8, 16, 160, 50, /*pixels=*/0);
	sst_io_test_mouse_report(35, 900, 400, 0);   /* col 900 > 160 cols → pixels */
	assert(sst_io_next_event(&ev) && ev.type == SST_EV_MOUSE_MOVE);
	/* now interpreted as pixels: x ≈ 900/1280*320 */
	assert(ev.x > 210 && ev.x < 240);
	return 0;
}
