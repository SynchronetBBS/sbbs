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

#include <stdlib.h>
#include <string.h>

/* Sanity ceiling on the cell grid's TOTAL size, not its shape -- there is no
 * per-dimension cap (a wide/short grid, e.g. a small cell on a wide image, is
 * exactly as valid as a square one). This only stops a runaway allocation on
 * absurd input; any real geometry (a 1000x1000 sixel-safe frame on the
 * smallest plausible cell) is well inside it. The grid/vis/stack buffers below
 * are heap-allocated and grown to fit cols*rows on demand -- see
 * dirty_ensure_u8()/dirty_ensure_int(). */
#define SR_DIRTY_MAX_CELLS 1000000

/* Merge two boxes separated by this many cells or fewer. 1 fuses a sprite split
 * across a cell edge; much more and unrelated activity gets swallowed into one
 * box that spans the gap between them, re-sending everything in between. */
#define SR_DIRTY_MERGE_GAP 1

/* Components tracked before merging. Higher than SR_DIRTY_MAX_RECTS on purpose:
 * scattered activity often merges DOWN to a few boxes, so the component count is
 * not the thing to give up on -- only the post-merge count is. This caps the
 * NUMBER of distinct blobs tracked, unrelated to the grid's cell count, so it
 * stays a fixed on-stack array. */
#define SR_DIRTY_MAX_COMPONENTS 96

typedef struct {
	int x1, y1, x2, y2;   /* cell coords, inclusive */
} sr_cellbox_t;

/* One byte (dirty flag) / one int (flood-fill stack slot) per cell, indexed
 * linearly as [cy * cols + cx]. Heap-allocated and grown to fit cols*rows --
 * grown, never shrunk, so a door that settles at one geometry allocates once
 * and every later call reuses the same buffer. */
static uint8_t *g_grid;
static size_t   g_grid_cap;    /* cells */
static uint8_t *g_vis;
static size_t   g_vis_cap;     /* cells */
static int     *g_stack;
static size_t   g_stack_cap;   /* cells (ints) */

/* Grow *buf to hold at least `need` bytes/ints, realloc()ing up when short --
 * the same idiom as syncretro_io.c's sr_io_ensure(), reproduced locally rather
 * than shared so this file doesn't have to #include syncretro_io.c. Never
 * shrinks. Returns 0 on success, -1 if the allocation failed (in which case
 * the buffer and its capacity are left exactly as they were). */
static int dirty_ensure_u8(uint8_t **buf, size_t *cap, size_t need)
{
	if (*cap < need) {
		uint8_t *p = (uint8_t *)realloc(*buf, need);

		if (p == NULL)
			return -1;
		*buf = p;
		*cap = need;
	}
	return 0;
}

static int dirty_ensure_int(int **buf, size_t *cap, size_t need)
{
	if (*cap < need) {
		int *p = (int *)realloc(*buf, need * sizeof(int));

		if (p == NULL)
			return -1;
		*buf = p;
		*cap = need;
	}
	return 0;
}

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
 * take). Returns the count, -1 if there are more components than we track, or
 * -2 if growing the vis/stack buffers to fit cols*rows failed. */
static int label_components(int cols, int rows, sr_cellbox_t *box, int max)
{
	static const int ox[4] = { -1, 1, 0, 0 };
	static const int oy[4] = { 0, 0, -1, 1 };
	int            nb = 0, cx, cy;
	size_t         total = (size_t)cols * (size_t)rows;

	if (dirty_ensure_u8(&g_vis, &g_vis_cap, total) != 0)
		return -2;
	if (dirty_ensure_int(&g_stack, &g_stack_cap, total) != 0)
		return -2;

	memset(g_vis, 0, total);
	for (cy = 0; cy < rows; cy++) {
		for (cx = 0; cx < cols; cx++) {
			int sp, i;
			size_t idx = (size_t)cy * (size_t)cols + (size_t)cx;

			if (!g_grid[idx] || g_vis[idx])
				continue;
			if (nb >= max)
				return -1;
			box[nb].x1 = box[nb].x2 = cx;
			box[nb].y1 = box[nb].y2 = cy;

			sp = 0;
			g_stack[sp++] = cy * cols + cx;
			g_vis[idx] = 1;
			while (sp > 0) {
				int cur = g_stack[--sp];
				int px  = cur % cols;
				int py  = cur / cols;

				if (px < box[nb].x1) box[nb].x1 = px;
				if (px > box[nb].x2) box[nb].x2 = px;
				if (py < box[nb].y1) box[nb].y1 = py;
				if (py > box[nb].y2) box[nb].y2 = py;

				for (i = 0; i < 4; i++) {
					int nx = px + ox[i];
					int ny = py + oy[i];
					size_t nidx;

					if (nx < 0 || ny < 0 || nx >= cols || ny >= rows)
						continue;
					nidx = (size_t)ny * (size_t)cols + (size_t)nx;
					if (!g_grid[nidx] || g_vis[nidx])
						continue;
					g_vis[nidx] = 1;
					g_stack[sp++] = ny * cols + nx;
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

/* See syncretro_dirty.h: WHY the last call returned 0 (or SR_DR_OK), and the
 * dirty percentage it observed. File-static, not thread-safe -- see the header
 * comment for why that is fine here. */
static int g_last_reason = SR_DR_OK;
static int g_last_pct    = -1;

int sr_dirty_last_reason(void) { return g_last_reason; }

int sr_dirty_last_pct(void) { return g_last_pct; }

const char *sr_dirty_reason_name(int reason)
{
	switch (reason) {
		case SR_DR_OK:          return "ok";
		case SR_DR_ARGS:        return "args";
		case SR_DR_GRID:        return "grid";
		case SR_DR_NOTHING:     return "nothing";
		case SR_DR_COVERAGE:    return "coverage";
		case SR_DR_COMPONENTS:  return "components";
		case SR_DR_FRAGMENTED:  return "fragmented";
		case SR_DR_BANDFIT:     return "bandfit";
		default:                return "?";
	}
}

int sr_dirty_find(const uint8_t *cur, const uint8_t *prev, int w, int h,
                  int cw, int ch, int band_align, sr_dirty_rect_t *out)
{
	sr_cellbox_t box[SR_DIRTY_MAX_COMPONENTS];
	int          cols, rows, cx, cy, nb, i, dirty = 0, total;

	g_last_reason = SR_DR_ARGS;
	g_last_pct    = -1;
	if (cur == NULL || prev == NULL || out == NULL)
		return 0;
	if (w <= 0 || h <= 0 || cw <= 0 || ch <= 0)
		return 0;

	cols = (w + cw - 1) / cw;
	rows = (h + ch - 1) / ch;
	/* No per-dimension cap -- only a total-cell sanity ceiling, so a narrow
	 * cell on a wide image (209 cols x 62 rows on a live foot session, over
	 * the old 160-column cap despite its 12,958 cells fitting easily) grids
	 * normally. cols/rows are always >= 1 here (w,h,cw,ch > 0 above), so the
	 * <= 0 check is just future-proofing against a caller changing that. */
	if (cols <= 0 || rows <= 0
	    || (int64_t)cols * (int64_t)rows > SR_DIRTY_MAX_CELLS) {
		g_last_reason = SR_DR_GRID;
		return 0;                      /* absurd geometry: repaint it whole */
	}
	total = cols * rows;

	if (dirty_ensure_u8(&g_grid, &g_grid_cap, (size_t)total) != 0) {
		g_last_reason = SR_DR_GRID;     /* OOM: full frame is always safe */
		return 0;
	}

	for (cy = 0; cy < rows; cy++) {
		for (cx = 0; cx < cols; cx++) {
			int d = cell_differs(cur, prev, w, h, cw, ch, cx, cy);

			g_grid[(size_t)cy * (size_t)cols + (size_t)cx] = (uint8_t)d;
			dirty += d;
		}
	}
	g_last_pct = dirty * 100 / total;
	/* Nothing moved (the caller's whole-frame de-dupe normally catches this
	 * first), or so much did that patching is a false economy. */
	if (dirty == 0) {
		g_last_reason = SR_DR_NOTHING;
		return 0;
	}
	if (g_last_pct >= SR_DIRTY_FULL_PCT) {
		g_last_reason = SR_DR_COVERAGE;
		return 0;
	}

	nb = label_components(cols, rows, box, SR_DIRTY_MAX_COMPONENTS);
	if (nb == -2) {
		g_last_reason = SR_DR_GRID;     /* OOM growing vis/stack: full frame */
		return 0;
	}
	if (nb <= 0) {
		g_last_reason = SR_DR_COMPONENTS;
		return 0;                      /* too many separate components */
	}
	nb = merge_boxes(box, nb);
	if (nb > SR_DIRTY_MAX_RECTS) {
		g_last_reason = SR_DR_FRAGMENTED;
		return 0;                      /* still too fragmented to be worth it */
	}
	sort_boxes(box, nb);

	if (band_align) {
		int vstep = ch / sr_gcd(ch, 6) * 6;   /* LCM(ch, 6): whole cells AND 6px bands */
		/* Last whole-cell row. The caller (syncretro_io.c's geometry routine)
		 * is expected to keep the image height a whole multiple of ch for
		 * every client that takes this band_align path, specifically so that
		 * hcell == h and this clamp is a no-op: a sub-cell remainder here can
		 * never be covered by a cell-aligned patch box, so it would render
		 * stale forever (the bug that motivated that upstream rounding). The
		 * clamp stays regardless, as a defensive net -- h is a plain
		 * parameter, not something this function can enforce on its caller. */
		int hcell = h / ch * ch;

		/* Snap each box to a whole number of vsteps, growing UPWARD from its
		 * changed-rows bottom. Repainting a few unchanged rows ABOVE a box is
		 * always safe, and -- unlike growing downward and clamping at the frame
		 * bottom -- it never leaves a box unable to cover its own changed rows, so
		 * activity in the bottom band no longer strands the whole frame (which had
		 * pinned foot at dr 0% on bottom-active games while SyncTERM, which skips
		 * this path, sat at 75-90%). A box is un-patchable only if a vstep-tall box
		 * covering it won't fit in the frame at all -- a near-full-height change,
		 * already rare and usually caught by the SR_DIRTY_FULL_PCT gate -- and only
		 * that falls back to a full frame. Pre-pass first so the fallback stays
		 * atomic (nothing on the wire yet). */
		for (i = 0; i < nb; i++) {
			int y0   = box[i].y1 * ch;
			int ybot = (box[i].y2 + 1) * ch;
			int rh, ytop;

			if (ybot > hcell)
				ybot = hcell;
			rh   = (ybot - y0 + vstep - 1) / vstep * vstep;
			ytop = ybot - rh;
			if (ytop < 0) {                              /* grew past row 0 */
				rh = (ybot + vstep - 1) / vstep * vstep;
				if (rh > hcell) {
					g_last_reason = SR_DR_BANDFIT;
					return 0;                            /* won't fit vstep-aligned: full frame */
				}
			}
		}
		for (i = 0; i < nb; i++) {
			int x0   = box[i].x1 * cw;
			int x1   = (box[i].x2 + 1) * cw;
			int y0   = box[i].y1 * ch;
			int ybot = (box[i].y2 + 1) * ch;
			int rh, ytop;

			if (x1 > w)
				x1 = w;
			if (ybot > hcell)
				ybot = hcell;
			rh   = (ybot - y0 + vstep - 1) / vstep * vstep;
			ytop = ybot - rh;
			if (ytop < 0) {
				ytop = 0;
				rh   = (ybot + vstep - 1) / vstep * vstep;
			}
			out[i].x   = x0;
			out[i].y   = ytop;
			out[i].w   = x1 - x0;
			out[i].h   = rh;
			out[i].col = box[i].x1;
			out[i].row = ytop / ch;
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
	g_last_reason = SR_DR_OK;
	return nb;
}
