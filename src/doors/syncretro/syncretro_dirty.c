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
#include "dirty.h"     /* termgfx: the shared grid-domain diff */

#include <stdlib.h>
#include <string.h>

/* The grid-domain half -- marking, labelling, merging, budgets -- now lives in
 * ../termgfx/dirty.h, shared with termgfx_termio's own patch path. What stays
 * here is the part the two doors genuinely disagree about: turning cell boxes
 * into placeable pixel rectangles, including the band alignment below. See
 * dirty.h for why the split falls there. */
#define SR_DIRTY_MERGE_GAP      1
#define SR_DIRTY_MAX_COMPONENTS 96

static int sr_gcd(int a, int b) { while (b) { int t = a % b; a = b; b = t; } return a; }

/* See syncretro_dirty.h: WHY the last call returned 0 (or SR_DR_OK), and the
 * dirty percentage it observed. File-static, not thread-safe -- see the header
 * comment for why that is fine here. */
/* The reason and the dirty share come from the shared finder, except for
 * SR_DR_BANDFIT, which is decided here (it is a property of THIS door's rect
 * mapping, not of the diff). */
static int g_bandfit_failed;

int sr_dirty_last_reason(void)
{
	if (g_bandfit_failed)
		return SR_DR_BANDFIT;
	switch (termgfx_dirty_last_reason()) {
		case TERMGFX_DR_OK:         return SR_DR_OK;
		case TERMGFX_DR_ARGS:       return SR_DR_ARGS;
		case TERMGFX_DR_GRID:       return SR_DR_GRID;
		case TERMGFX_DR_NOTHING:    return SR_DR_NOTHING;
		case TERMGFX_DR_COVERAGE:   return SR_DR_COVERAGE;
		case TERMGFX_DR_COMPONENTS: return SR_DR_COMPONENTS;
		default:                    return SR_DR_FRAGMENTED;
	}
}

int sr_dirty_last_pct(void) { return termgfx_dirty_last_pct(); }

const char *sr_dirty_reason_name(int reason)
{
	switch (reason) {
		case SR_DR_OK:         return "ok";
		case SR_DR_ARGS:       return "args";
		case SR_DR_GRID:       return "grid";
		case SR_DR_NOTHING:    return "nothing";
		case SR_DR_COVERAGE:   return "coverage";
		case SR_DR_COMPONENTS: return "components";
		case SR_DR_FRAGMENTED: return "fragmented";
		case SR_DR_BANDFIT:    return "bandfit";
		default:               return "?";
	}
}

int sr_dirty_find(const uint8_t *cur, const uint8_t *prev, int w, int h,
                  int cw, int ch, int band_align, const uint8_t *stale,
                  sr_dirty_rect_t *out)
{
	termgfx_dirty_cfg_t cfg;
	termgfx_dirty_box_t box[SR_DIRTY_MAX_COMPONENTS];
	int                 cols, rows, nb, i;

	g_bandfit_failed = 0;
	if (cur == NULL || prev == NULL || out == NULL)
		return 0;
	if (w <= 0 || h <= 0 || cw <= 0 || ch <= 0)
		return 0;

	cols = (w + cw - 1) / cw;
	rows = (h + ch - 1) / ch;

	memset(&cfg, 0, sizeof cfg);
	cfg.cols           = cols;
	cfg.rows           = rows;
	cfg.cell_w         = cw;
	cfg.cell_h         = ch;
	cfg.plane_w        = w;
	cfg.plane_h        = h;
	cfg.merge_gap      = SR_DIRTY_MERGE_GAP;
	cfg.full_pct       = SR_DIRTY_FULL_PCT;
	cfg.max_components = SR_DIRTY_MAX_COMPONENTS;
	cfg.max_boxes      = SR_DIRTY_MAX_RECTS;

	nb = termgfx_dirty_find(&cfg, cur, prev, stale, box);
	if (nb <= 0)
		return 0;

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
					g_bandfit_failed = 1;
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
	return nb;
}
