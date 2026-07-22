/* syncretro_dirty.c -- see syncretro_dirty.h.
 *
 * Two steps: mark the cells whose pixels differ, then cover the marked cells
 * with a handful of rectangles.
 *
 * The covering is where the judgement is. One bounding box around everything is
 * trivial and useless -- Pac-Man's ghosts scatter to the corners, so their
 * bounding box is the whole maze. Per-cell rectangles are the opposite mistake:
 * each one costs a cursor address and a sixel intro/palette-less header, so a
 * few dozen of them cost more than the frame they replace. What works is
 * connected components (things that move are contiguous) followed by merging
 * boxes that nearly touch (a sprite straddling a cell boundary is two
 * components that should be one rectangle).
 *
 * That is the same shape ../syncconquer/door/door_io.c settled on, arrived at
 * there for a map/sidebar layout rather than sprites; it is reproduced rather
 * than shared because the two doors disagree about what a tile is -- syncconquer
 * diffs its native framebuffer on a fixed 16px grid and maps the result through
 * its scaler, this diffs the SCALED frame on the cell grid so no mapping is
 * needed and every rectangle comes out placeable. Worth folding into termgfx if
 * a third door wants it.
 */
#include "syncretro_dirty.h"

#include <string.h>

/* Cell-grid ceiling. A 1000x1000 frame (the sixel safe maximum) on the smallest
 * plausible cell is well inside this; a frame that would exceed it asks for a
 * full frame rather than diffing a truncated grid. */
#define SR_DIRTY_MAX_COLS 160
#define SR_DIRTY_MAX_ROWS 100

/* Merge two boxes separated by this many cells or fewer. 1 fuses a sprite split
 * across a cell edge; much more and unrelated activity gets swallowed into one
 * box that spans the gap between them, re-sending everything in between. */
#define SR_DIRTY_MERGE_GAP 1

/* Components tracked before merging. Higher than SR_DIRTY_MAX_RECTS on purpose:
 * scattered activity often merges DOWN to a few boxes, so the component count is
 * not the thing to give up on -- only the post-merge count is. */
#define SR_DIRTY_MAX_COMPONENTS 96

typedef struct {
	int x1, y1, x2, y2;   /* cell coords, inclusive */
} sr_cellbox_t;

static uint8_t g_grid[SR_DIRTY_MAX_ROWS][SR_DIRTY_MAX_COLS];

/* Do the pixels of cell (cx,cy) differ? Compares row by row, stopping at the
 * first difference -- an unchanged cell is the common case and costs a full
 * scan, so the inner loop is a memcmp per row rather than per pixel. */
static int cell_differs(const uint8_t *cur, const uint8_t *prev, int w, int h,
                        int cw, int ch, int cx, int cy)
{
	int x0 = cx * cw;
	int y0 = cy * ch;
	int x1 = x0 + cw;
	int y1 = y0 + ch;
	int y;

	if (x1 > w)
		x1 = w;                    /* the last column of cells is usually partial */
	if (y1 > h)
		y1 = h;
	for (y = y0; y < y1; y++) {
		size_t off = (size_t)y * w + x0;

		if (memcmp(cur + off, prev + off, (size_t)(x1 - x0)) != 0)
			return 1;
	}
	return 0;
}

/* Label 4-connected components of dirty cells, each as its bounding box.
 * Iterative flood fill (an explicit stack, not recursion: a full-screen change
 * is one component of every cell, and that is a stack depth no door should
 * take). Returns the count, or -1 if there are more components than we track. */
static int label_components(int cols, int rows, sr_cellbox_t *box, int max)
{
	static uint8_t vis[SR_DIRTY_MAX_ROWS][SR_DIRTY_MAX_COLS];
	static int     stack[SR_DIRTY_MAX_ROWS * SR_DIRTY_MAX_COLS];
	static const int ox[4] = { -1, 1, 0, 0 };
	static const int oy[4] = { 0, 0, -1, 1 };
	int            nb = 0, cx, cy;

	memset(vis, 0, sizeof vis);
	for (cy = 0; cy < rows; cy++) {
		for (cx = 0; cx < cols; cx++) {
			int sp, i;

			if (!g_grid[cy][cx] || vis[cy][cx])
				continue;
			if (nb >= max)
				return -1;
			box[nb].x1 = box[nb].x2 = cx;
			box[nb].y1 = box[nb].y2 = cy;

			sp = 0;
			stack[sp++] = cy * cols + cx;
			vis[cy][cx] = 1;
			while (sp > 0) {
				int cur = stack[--sp];
				int px  = cur % cols;
				int py  = cur / cols;

				if (px < box[nb].x1) box[nb].x1 = px;
				if (px > box[nb].x2) box[nb].x2 = px;
				if (py < box[nb].y1) box[nb].y1 = py;
				if (py > box[nb].y2) box[nb].y2 = py;

				for (i = 0; i < 4; i++) {
					int nx = px + ox[i];
					int ny = py + oy[i];

					if (nx < 0 || ny < 0 || nx >= cols || ny >= rows)
						continue;
					if (!g_grid[ny][nx] || vis[ny][nx])
						continue;
					vis[ny][nx] = 1;
					stack[sp++] = ny * cols + nx;
				}
			}
			nb++;
		}
	}
	return nb;
}

/* Union boxes that overlap or lie within SR_DIRTY_MERGE_GAP cells, repeatedly
 * until nothing more merges (one pass is not enough: merging A into B can bring
 * B within reach of C). */
static int merge_boxes(sr_cellbox_t *box, int nb)
{
	int merged = 1;

	while (merged) {
		int i, j;

		merged = 0;
		for (i = 0; i < nb && !merged; i++) {
			for (j = i + 1; j < nb && !merged; j++) {
				const int g = SR_DIRTY_MERGE_GAP;

				if (box[i].x1 - g > box[j].x2 || box[j].x1 - g > box[i].x2)
					continue;                       /* disjoint horizontally */
				if (box[i].y1 - g > box[j].y2 || box[j].y1 - g > box[i].y2)
					continue;                       /* disjoint vertically */
				if (box[j].x1 < box[i].x1) box[i].x1 = box[j].x1;
				if (box[j].y1 < box[i].y1) box[i].y1 = box[j].y1;
				if (box[j].x2 > box[i].x2) box[i].x2 = box[j].x2;
				if (box[j].y2 > box[i].y2) box[i].y2 = box[j].y2;
				box[j] = box[nb - 1];
				nb--;
				merged = 1;
			}
		}
	}
	return nb;
}

/* Sort top-to-bottom, left-to-right. Insertion sort: nb is at most
 * SR_DIRTY_MAX_RECTS, so anything cleverer is longer than it is fast. */
static void sort_boxes(sr_cellbox_t *box, int nb)
{
	int i, j;

	for (i = 1; i < nb; i++) {
		sr_cellbox_t k = box[i];

		for (j = i - 1; j >= 0 && (box[j].y1 > k.y1
		                           || (box[j].y1 == k.y1 && box[j].x1 > k.x1)); j--)
			box[j + 1] = box[j];
		box[j + 1] = k;
	}
}

/* Greatest common divisor, for LCM(ch,6) in the band_align path below. */
static int sr_gcd(int a, int b) { while (b) { int t = a % b; a = b; b = t; } return a; }

int sr_dirty_find(const uint8_t *cur, const uint8_t *prev, int w, int h,
                  int cw, int ch, int band_align, sr_dirty_rect_t *out)
{
	sr_cellbox_t box[SR_DIRTY_MAX_COMPONENTS];
	int          cols, rows, cx, cy, nb, i, dirty = 0, total;

	if (cur == NULL || prev == NULL || out == NULL)
		return 0;
	if (w <= 0 || h <= 0 || cw <= 0 || ch <= 0)
		return 0;

	cols = (w + cw - 1) / cw;
	rows = (h + ch - 1) / ch;
	if (cols > SR_DIRTY_MAX_COLS || rows > SR_DIRTY_MAX_ROWS)
		return 0;                      /* bigger than we grid: repaint it whole */
	total = cols * rows;

	for (cy = 0; cy < rows; cy++) {
		for (cx = 0; cx < cols; cx++) {
			int d = cell_differs(cur, prev, w, h, cw, ch, cx, cy);

			g_grid[cy][cx] = (uint8_t)d;
			dirty += d;
		}
	}
	/* Nothing moved (the caller's whole-frame de-dupe normally catches this
	 * first), or so much did that patching is a false economy. */
	if (dirty == 0 || dirty * 100 / total >= SR_DIRTY_FULL_PCT)
		return 0;

	nb = label_components(cols, rows, box, SR_DIRTY_MAX_COMPONENTS);
	if (nb <= 0)
		return 0;                      /* too many separate components */
	nb = merge_boxes(box, nb);
	if (nb > SR_DIRTY_MAX_RECTS)
		return 0;                      /* still too fragmented to be worth it */
	sort_boxes(box, nb);

	if (band_align) {
		int vstep = ch / sr_gcd(ch, 6) * 6;   /* LCM(ch, 6) */

		/* Pre-pass: if any bottom-clamped box can't cover its own changed
		 * rows at a whole vstep, bail to a full frame (return 0) before
		 * writing any rect -- the caller's whole-frame path is always right. */
		for (i = 0; i < nb; i++) {
			int y0  = box[i].y1 * ch;
			int y2  = (box[i].y2 + 1) * ch;   /* this box's changed-rows bottom */
			int rh;

			if (y2 > h)
				y2 = h;
			rh = (y2 - y0 + vstep - 1) / vstep * vstep;
			if (y0 + rh > h)
				rh = (h - y0) / vstep * vstep;   /* stay vstep-aligned at frame bottom */
			if (y0 + rh < y2)
				return 0;                         /* stranded -> full frame */
		}
		for (i = 0; i < nb; i++) {
			int x0 = box[i].x1 * cw;
			int y0 = box[i].y1 * ch;
			int x1 = (box[i].x2 + 1) * cw;
			int y2 = (box[i].y2 + 1) * ch;
			int rh;

			if (x1 > w) x1 = w;
			if (y2 > h) y2 = h;
			rh = (y2 - y0 + vstep - 1) / vstep * vstep;
			if (y0 + rh > h)
				rh = (h - y0) / vstep * vstep;
			out[i].x   = x0;
			out[i].y   = y0;
			out[i].w   = x1 - x0;
			out[i].h   = rh;
			out[i].col = box[i].x1;
			out[i].row = box[i].y1;
		}
	} else {
		for (i = 0; i < nb; i++) {
			int x0 = box[i].x1 * cw;
			int y0 = box[i].y1 * ch;
			int x1 = (box[i].x2 + 1) * cw;
			int y1 = (box[i].y2 + 1) * ch;

			if (x1 > w) x1 = w;            /* clamp the partial last cell to the frame */
			if (y1 > h) y1 = h;
			out[i].x   = x0;
			out[i].y   = y0;
			out[i].w   = x1 - x0;
			out[i].h   = y1 - y0;
			out[i].col = box[i].x1;
			out[i].row = box[i].y1;
		}
	}
	return nb;
}
