/* test_termgfx_termio_fitcanvas.c -- fit against what the TERMINAL declared,
 * not against the 640x400 guess.
 *
 * The numbers below are a captured SyncTERM 1.9rc4 session running Maniac
 * Mansion in 80x43. The terminal answered:
 *
 *     ESC[?2;0;640;336S      XTSMGRAPHICS: 640x336 drawable (42 rows x 8px)
 *     ESC[42;80R             CPR: 42 rows, 80 cols (43 less the status line)
 *
 * and never answered ESC[14t, because SyncTERM's CSI t is the CTerm-private
 * palette setter. The door therefore kept the assumed 640x400 canvas and
 * traced `canvas=640x400? emit=640x400@0,0 cell=10` -- a 400-line frame into a
 * 350-line canvas, which SyncTERM 1.9 discards WHOLE (blank screen) and 1.10
 * clips (truncated), with a 10px cell against a real 8 misplacing the sixel
 * CUP origin and every dirty-rect box.
 *
 * Copyright(C) 2026 Rob Swindell.  GPL-2.0.
 */
#include <stdio.h>
#include <string.h>

#include "termgfx_termio.h"   /* termgfx_input_event_t, termgfx_termio_next_event() */

void termgfx_termio_test_set_geom(int canvas_w, int canvas_h, int cell_w, int cell_h,
                                  int cols, int rows, int pixels);
int  termgfx_termio_test_mouse_report(int b, int col, int row, int release);
void termgfx_termio_test_set_syncterm(int is_syncterm);
void termgfx_termio_test_set_gfx_max(int w, int h, int canvas_exact);
void termgfx_termio_test_image_rect(int sw, int sh, int *ew, int *eh,
                                    int *dx, int *dy, int *cellh);

static int failures;

#define CHECK(cond) \
		do { \
			if (!(cond)) { \
				printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
				failures++; \
			} \
		} while (0)

/* The SyncTERM shape: grid and cell size unknown from ESC[16t, canvas NOT
 * exact, drawable area known only as an XTSMGRAPHICS ceiling. */
static void syncterm_80x43(void)
{
	termgfx_termio_test_set_geom(640, 400, /*cell_w*/ 0, /*cell_h*/ 0,
	                             /*cols*/ 80, /*rows*/ 42, /*pixels*/ 0);
	termgfx_termio_test_set_gfx_max(640, 336, /*canvas_exact*/ 0);
	termgfx_termio_test_set_syncterm(1);
}

int main(void)
{
	int ew, eh, dx, dy, cellh;

	/* SyncSCUMM: a 320x200 source, the case that went blank. */
	syncterm_80x43();
	termgfx_termio_test_image_rect(320, 200, &ew, &eh, &dx, &dy, &cellh);
	printf("320x200 -> %dx%d @%d,%d cell=%d\n", ew, eh, dx, dy, cellh);
	CHECK(eh <= 336);            /* never past what the terminal declared */
	CHECK(ew <= 640);
	CHECK(cellh == 8);           /* 336/42, not the old 400/42 = 10 */
	CHECK(dy + eh <= 336);       /* centered INSIDE the drawable area */
	CHECK(dx + ew <= 640);

	/* SyncRPG: a 4:3 source. Same canvas, same ceiling. */
	syncterm_80x43();
	termgfx_termio_test_image_rect(320, 240, &ew, &eh, &dx, &dy, &cellh);
	printf("320x240 -> %dx%d @%d,%d cell=%d\n", ew, eh, dx, dy, cellh);
	CHECK(eh <= 336);
	CHECK(dy + eh <= 336);
	CHECK(cellh == 8);

	/* SHAPE, on the 80x43 canvas. The emitted rect takes the CANVAS's
	 * proportions, not the source's 1.6: SyncTERM shows 640x336 at 4:3, so its
	 * pixels are 0.70 wide/tall and a 4:3 source has to be drawn wider to come
	 * out 4:3 on screen. Uncorrected this was 524x328 (1.60) -- 14% too tall. */
	syncterm_80x43();
	termgfx_termio_test_image_rect(320, 200, &ew, &eh, &dx, &dy, &cellh);
	printf("80x43 shape -> %dx%d (canvas 640x336 = %.3f, got %.3f)\n",
	       ew, eh, 640.0 / 336.0, (double)ew / eh);
	CHECK((double)ew / eh > (640.0 / 336.0) * 0.97);
	CHECK((double)ew / eh < (640.0 / 336.0) * 1.03);

	/* A WIDE mode: 132x43 is 1056 px across, where the built-in guess is 640.
	 * The declared area has to be ADOPTED, not just clamped to -- clamping only
	 * ever shrinks, so the door went on fitting and centering inside a phantom
	 * 640-wide canvas and the picture sat against the left edge with the right
	 * half of the screen empty. Captured from a 1.10a session with the status
	 * line hidden: 132 cols x 43 rows, area 1056x344. */
	termgfx_termio_test_set_geom(640, 400, 0, 0, /*cols*/ 132, /*rows*/ 43, 0);
	termgfx_termio_test_set_gfx_max(1056, 344, /*canvas_exact*/ 0);
	termgfx_termio_test_set_syncterm(1);
	termgfx_termio_test_image_rect(320, 200, &ew, &eh, &dx, &dy, &cellh);
	printf("132x43 320x200 -> %dx%d @%d,%d cell=%d\n", ew, eh, dx, dy, cellh);
	CHECK(cellh == 8);                       /* 344/43, not 400/43 = 9 */
	/* SHAPE. SyncTERM shows this 1056x344 canvas at 4:3, so its pixels are 0.43
	 * wide/tall and a 4:3 source drawn to fill it comes out 4:3 on screen. The
	 * invariant: for a source whose intended shape matches the canvas's display
	 * aspect, the emitted rect has the CANVAS's proportions -- the 1.6 reference
	 * cancels. Uncorrected this was 537x336 (1.60), badly too tall. */
	CHECK((double)ew / eh > (1056.0 / 344.0) * 0.97);
	CHECK((double)ew / eh < (1056.0 / 344.0) * 1.03);
	CHECK(ew > 950);                         /* very nearly fills the width */
	/* Centered in 1056, not in the 640 guess. The image is legitimately
	 * NARROWER than 640 here (height-limited by the 344-line canvas), so its
	 * width proves nothing -- the offset is what shows which canvas was used:
	 * 259 centers in 1056, the old code's 51 centered in 640. */
	CHECK(dx * 2 + ew >= 1056 - 2 && dx * 2 + ew <= 1056 + 2);
	CHECK(dx + ew <= 1056);

	/* An EXACT canvas keeps the reply as a ceiling and is clamped DOWN to it --
	 * the xterm case, where the raster limit is smaller than the window it
	 * reports. Adopting there would emit past what xterm will accept. */
	termgfx_termio_test_set_geom(1368, 906, 0, 0, 171, 56, 0);   /* sets exact */
	termgfx_termio_test_set_gfx_max(1000, 1000, /*canvas_exact*/ 1);
	termgfx_termio_test_set_syncterm(0);     /* square-pixel terminal: no correction */
	termgfx_termio_test_image_rect(320, 200, &ew, &eh, &dx, &dy, &cellh);
	printf("xterm 1368x906 cap 1000 -> %dx%d @%d,%d\n", ew, eh, dx, dy);
	CHECK(ew <= 1000 && eh <= 1000);

	/* 80x25 -- the mode where the old assumption happened to be right -- must
	 * be unchanged: 640x400 canvas, 640x400 ceiling, 16px cell. */
	termgfx_termio_test_set_geom(640, 400, 0, 0, 80, 25, 0);
	termgfx_termio_test_set_gfx_max(640, 400, 0);
	termgfx_termio_test_image_rect(320, 200, &ew, &eh, &dx, &dy, &cellh);
	printf("80x25 320x200 -> %dx%d @%d,%d cell=%d\n", ew, eh, dx, dy, cellh);
	CHECK(cellh == 16);
	CHECK(eh <= 400);

	/* A terminal that declares NO ceiling keeps the old behaviour rather than
	 * being clamped to nothing -- silence is not a ceiling of zero. */
	termgfx_termio_test_set_geom(640, 400, 0, 0, 80, 25, 0);
	termgfx_termio_test_set_gfx_max(0, 0, 0);
	termgfx_termio_test_image_rect(320, 200, &ew, &eh, &dx, &dy, &cellh);
	printf("no ceiling 320x200 -> %dx%d @%d,%d cell=%d\n", ew, eh, dx, dy, cellh);
	CHECK(ew > 0 && eh > 0);
	CHECK(eh <= 400);

	/* An ESC[16t cell report outranks any derivation. */
	termgfx_termio_test_set_geom(640, 400, /*cell_w*/ 8, /*cell_h*/ 8, 80, 42, 0);
	termgfx_termio_test_set_gfx_max(640, 336, 0);
	termgfx_termio_test_image_rect(320, 200, &ew, &eh, &dx, &dy, &cellh);
	CHECK(cellh == 8);

	/* The POINTER has to land where the picture is.
	 *
	 * Converting a text cell to canvas pixels with an over-tall cell walks off
	 * the bottom of the image and CLAMPS there, so the lower half of the screen
	 * all maps to the last game row -- a plateau, not a slope. That is what a
	 * player feels as the pointer drifting further out the further down he
	 * moves. Endpoints prove nothing here: rows 1 and `rows` are deliberately
	 * snapped to the image edges (so the extreme game rows stay reachable on a
	 * cell-granular terminal) and land right whatever the cell size, and
	 * (row-1)*anything is monotonic. The mid-screen row and the count of
	 * distinct values are what actually separate a correct mapping from a
	 * saturated one: with the old 16px guess against this 8px cell, row 21 of
	 * 42 lands at game y 199 instead of 100. */
	{
		termgfx_input_event_t ev;
		int ys[43];
		int row, distinct = 0, i, j;

		syncterm_80x43();
		while (termgfx_termio_next_event(&ev)) { }   /* drain */
		for (row = 1; row <= 42; row++) {
			ys[row] = -1;
			termgfx_termio_test_mouse_report(35, 40, row, 0);
			while (termgfx_termio_next_event(&ev)) {
				if (ev.type == TERMGFX_EV_MOUSE_MOVE)
					ys[row] = ev.y;
			}
			CHECK(ys[row] >= 0);
			if (row > 1)
				CHECK(ys[row] >= ys[row - 1]);   /* monotonic down the screen */
		}
		printf("mouse rows 1..42 -> game y %d..%d (mid row 21 -> %d)\n",
		       ys[1], ys[42], ys[21]);

		/* Mid-screen maps mid-picture, within a row's worth of slack. */
		CHECK(ys[21] > 80 && ys[21] < 120);

		/* No plateau: nearly every row should land somewhere new. */
		for (i = 1; i <= 42; i++) {
			for (j = 1; j < i; j++)
				if (ys[j] == ys[i])
					break;
			if (j == i)
				distinct++;
		}
		CHECK(distinct >= 38);
	}

	printf("%s: test_termgfx_termio_fitcanvas\n", failures ? "FAILED" : "ok");
	return failures ? 1 : 0;
}
