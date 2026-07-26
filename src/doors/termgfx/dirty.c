/* dirty.c -- see dirty.h.
 *
 * Two steps: mark the cells whose pixels differ, then cover the marked cells
 * with a handful of boxes.
 *
 * The covering is where the judgement is. One bounding box around everything is
 * trivial and useless -- Pac-Man's ghosts scatter to the corners, so their
 * bounding box is the whole maze. Per-cell boxes are the opposite mistake: each
 * one costs a cursor address and an image header, so a few dozen of them cost
 * more than the frame they replace. What works is connected components (things
 * that move are contiguous) followed by merging boxes that nearly touch (a
 * sprite straddling a cell boundary is two components that should be one box).
 */
#include "dirty.h"

#include <stdlib.h>
#include <string.h>

/* Sanity ceiling on the grid's TOTAL size, not its shape -- a wide/short grid
 * (a narrow cell on a wide image) is exactly as valid as a square one. This
 * only stops a runaway allocation on absurd input; any real geometry is well
 * inside it. */
#define TERMGFX_DIRTY_MAX_CELLS 1000000

/* One byte (dirty flag) / one int (flood-fill stack slot) per cell, indexed as
 * [cy * cols + cx]. Grown to fit and never shrunk, so a door that settles at
 * one geometry allocates once and every later call reuses the buffer. */
static uint8_t *g_grid;
static size_t   g_grid_cap;
static uint8_t *g_vis;
static size_t   g_vis_cap;
static int *    g_stack;
static size_t   g_stack_cap;

static int      g_last_reason = TERMGFX_DR_OK;
static int      g_last_pct    = -1;

int termgfx_dirty_last_reason(void) { return g_last_reason; }
int termgfx_dirty_last_pct(void)    { return g_last_pct; }

const char *termgfx_dirty_reason_name(int reason)
{
	switch (reason) {
		case TERMGFX_DR_OK:         return "ok";
		case TERMGFX_DR_ARGS:       return "args";
		case TERMGFX_DR_GRID:       return "grid";
		case TERMGFX_DR_NOTHING:    return "nothing";
		case TERMGFX_DR_COVERAGE:   return "toobig";
		case TERMGFX_DR_COMPONENTS: return "components";
		case TERMGFX_DR_FRAGMENTED: return "frag";
		default:                    return "?";
	}
}

/* Grow *buf to hold at least `need`. Never shrinks. 0 on success, -1 if the
 * allocation failed (buffer and capacity left exactly as they were). */
static int ensure_u8(uint8_t **buf, size_t *cap, size_t need)
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

static int ensure_int(int **buf, size_t *cap, size_t need)
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

/* Do the pixels of cell (cx,cy) differ, or does it draw with a colour that is
 * about to move? Row by row, stopping at the first difference -- an unchanged
 * cell is the common case and costs a full scan, so the inner loop is a memcmp
 * per row rather than per pixel. */
static int cell_differs(const termgfx_dirty_cfg_t *cfg, const uint8_t *cur,
                        const uint8_t *prev, const uint8_t *stale, int cx, int cy)
{
	int x0 = cx * cfg->cell_w;
	int y0 = cy * cfg->cell_h;
	int x1 = x0 + cfg->cell_w;
	int y1 = y0 + cfg->cell_h;
	int x, y;

	if (x1 > cfg->plane_w)
		x1 = cfg->plane_w;          /* the far column/row is usually partial */
	if (y1 > cfg->plane_h)
		y1 = cfg->plane_h;
	for (y = y0; y < y1; y++) {
		size_t off = (size_t)y * cfg->plane_w + x0;

		if (memcmp(cur + off, prev + off, (size_t)(x1 - x0)) != 0)
			return 1;
		if (stale != NULL)          /* same indices, one of them means a new colour */
			for (x = 0; x < x1 - x0; x++)
				if (stale[prev[off + x]])
					return 1;
	}
	return 0;
}

/* Label 4-connected components of dirty cells, each as its bounding box.
 * Iterative flood fill (an explicit stack, not recursion: a full-screen change
 * is one component of every cell, and that is a stack depth no door should
 * take). Returns the count, -1 past `max` components, -2 if a buffer could not
 * be grown. */
static int label_components(int cols, int rows, termgfx_dirty_box_t *box, int max)
{
	static const int ox[4] = { -1, 1, 0, 0 };
	static const int oy[4] = { 0, 0, -1, 1 };
	int              nb = 0, cx, cy;
	size_t           total = (size_t)cols * (size_t)rows;

	if (ensure_u8(&g_vis, &g_vis_cap, total) != 0)
		return -2;
	if (ensure_int(&g_stack, &g_stack_cap, total) != 0)
		return -2;

	memset(g_vis, 0, total);
	for (cy = 0; cy < rows; cy++) {
		for (cx = 0; cx < cols; cx++) {
			int    sp, i;
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
				int cur_ = g_stack[--sp];
				int px   = cur_ % cols;
				int py   = cur_ / cols;

				if (px < box[nb].x1)
					box[nb].x1 = px;
				if (px > box[nb].x2)
					box[nb].x2 = px;
				if (py < box[nb].y1)
					box[nb].y1 = py;
				if (py > box[nb].y2)
					box[nb].y2 = py;

				for (i = 0; i < 4; i++) {
					int    nx = px + ox[i];
					int    ny = py + oy[i];
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

/* Union boxes that overlap or lie within `gap` cells, repeatedly until nothing
 * more merges (one pass is not enough: merging A into B can bring B within
 * reach of C). */
static int merge_boxes(termgfx_dirty_box_t *box, int nb, int gap)
{
	int merged = 1;

	while (merged) {
		int i, j;

		merged = 0;
		for (i = 0; i < nb && !merged; i++) {
			for (j = i + 1; j < nb && !merged; j++) {
				if (box[i].x1 - gap > box[j].x2 || box[j].x1 - gap > box[i].x2)
					continue;                       /* disjoint horizontally */
				if (box[i].y1 - gap > box[j].y2 || box[j].y1 - gap > box[i].y2)
					continue;                       /* disjoint vertically */
				if (box[j].x1 < box[i].x1)
					box[i].x1 = box[j].x1;
				if (box[j].y1 < box[i].y1)
					box[i].y1 = box[j].y1;
				if (box[j].x2 > box[i].x2)
					box[i].x2 = box[j].x2;
				if (box[j].y2 > box[i].y2)
					box[i].y2 = box[j].y2;
				box[j] = box[nb - 1];
				nb--;
				merged = 1;
			}
		}
	}
	return nb;
}

/* Top-to-bottom, left-to-right. Insertion sort: nb is a handful by
 * construction, so anything cleverer is longer than it is fast. */
static void sort_boxes(termgfx_dirty_box_t *box, int nb)
{
	int i, j;

	for (i = 1; i < nb; i++) {
		termgfx_dirty_box_t k = box[i];

		for (j = i - 1; j >= 0 && (box[j].y1 > k.y1
		                           || (box[j].y1 == k.y1 && box[j].x1 > k.x1)); j--)
			box[j + 1] = box[j];
		box[j + 1] = k;
	}
}

int termgfx_dirty_find(const termgfx_dirty_cfg_t *cfg, const uint8_t *cur,
                       const uint8_t *prev, const uint8_t *stale,
                       termgfx_dirty_box_t *out)
{
	int cx, cy, nb, dirty = 0, total;

	g_last_reason = TERMGFX_DR_ARGS;
	g_last_pct    = -1;
	if (cfg == NULL || cur == NULL || prev == NULL || out == NULL)
		return 0;
	if (cfg->cols <= 0 || cfg->rows <= 0 || cfg->cell_w <= 0 || cfg->cell_h <= 0)
		return 0;
	if (cfg->plane_w <= 0 || cfg->plane_h <= 0 || cfg->max_components <= 0)
		return 0;
	if ((int64_t)cfg->cols * (int64_t)cfg->rows > TERMGFX_DIRTY_MAX_CELLS) {
		g_last_reason = TERMGFX_DR_GRID;   /* absurd geometry: repaint it whole */
		return 0;
	}
	total = cfg->cols * cfg->rows;

	if (ensure_u8(&g_grid, &g_grid_cap, (size_t)total) != 0) {
		g_last_reason = TERMGFX_DR_GRID;   /* OOM: a full frame is always safe */
		return 0;
	}

	for (cy = 0; cy < cfg->rows; cy++) {
		for (cx = 0; cx < cfg->cols; cx++) {
			int d = cell_differs(cfg, cur, prev, stale, cx, cy);

			g_grid[(size_t)cy * (size_t)cfg->cols + (size_t)cx] = (uint8_t)d;
			dirty += d;
		}
	}
	g_last_pct = dirty * 100 / total;

	/* Nothing moved (the caller's whole-frame de-dupe normally catches this
	 * first), or so much did that patching is a false economy. */
	if (dirty == 0) {
		g_last_reason = TERMGFX_DR_NOTHING;
		return 0;
	}
	if (g_last_pct >= cfg->full_pct) {
		g_last_reason = TERMGFX_DR_COVERAGE;
		return 0;
	}

	nb = label_components(cfg->cols, cfg->rows, out, cfg->max_components);
	if (nb == -2) {
		g_last_reason = TERMGFX_DR_GRID;
		return 0;
	}
	if (nb <= 0) {
		g_last_reason = TERMGFX_DR_COMPONENTS;
		return 0;
	}
	nb = merge_boxes(out, nb, cfg->merge_gap);
	if (cfg->max_boxes > 0 && nb > cfg->max_boxes) {
		g_last_reason = TERMGFX_DR_FRAGMENTED;
		return 0;
	}
	sort_boxes(out, nb);

	g_last_reason = TERMGFX_DR_OK;
	return nb;
}
