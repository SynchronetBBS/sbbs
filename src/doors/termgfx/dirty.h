#ifndef TERMGFX_DIRTY_H_
#define TERMGFX_DIRTY_H_

#include <stdint.h>

/* dirty.h -- which cells of an indexed frame changed, for the doors' patch path.
 *
 * A console redraws its whole framebuffer every frame, but very little of it
 * MOVES: a maze is static while five sprites and a scatter of dots are not.
 * Sending the whole picture every time is most of a door's bandwidth, and the
 * terminal is already holding the previous frame -- so the parts that changed
 * can be painted over it and the rest left alone.
 *
 * This is the "which parts" half, in GRID COORDINATES and nothing else: no
 * pixels, no encoding, no terminal. It was written three times before it was
 * written once -- syncconquer's door_io.c, syncretro's syncretro_dirty.c and
 * termgfx_termio.c each grew their own copy, and the same defect had to be
 * found and fixed in them separately (a box against the frame bottom that
 * cannot cover its own changed rows; a palette change that moves what a cell
 * MEANS without moving its indices). Sharing the part that is genuinely common
 * is what stops the next one needing three fixes.
 *
 * WHAT IS COMMON is everything up to a set of boxes on the cell grid: marking
 * the grid, labelling 4-connected components, merging boxes that nearly touch,
 * and the budgets that decide patching is a false economy.
 *
 * WHAT IS NOT is the mapping from grid boxes to display pixels, and the doors
 * genuinely disagree about it: syncretro diffs the ALREADY-SCALED frame on the
 * terminal's real cell grid, so its boxes are placeable as they come;
 * termgfx_termio diffs the NATIVE 320x200 on a fixed 16px tile grid and maps
 * the result through its scaler, so it must snap afterwards. That mapping --
 * with its band alignment, its bottom-edge rules and its own failure modes --
 * stays with each caller.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* A changed region, in CELLS, inclusive on both corners. */
typedef struct {
	int x1, y1, x2, y2;
} termgfx_dirty_box_t;

/* How to diff, and when to give up. Every field is a correctness or economy
 * decision the caller owns -- the defaults differ per door because their grids
 * do (a 16px tile over 320x200 is not a 6x13 text cell over 1254x806). */
typedef struct {
	int cols, rows;         /* grid dimensions; cells may be partial at the far edge */
	int cell_w, cell_h;     /* cell size in plane pixels */
	int plane_w, plane_h;   /* the index plane's own size, for clamping edge cells */
	int merge_gap;          /* union boxes this many cells apart or nearer */
	int full_pct;           /* at or past this % of cells dirty, ask for a full frame */
	int max_components;     /* distinct blobs tracked before giving up */
	int max_boxes;          /* cap AFTER merging; 0 = no cap */
} termgfx_dirty_cfg_t;

/* WHY a call returned 0. TERMGFX_DR_OK is also set on success, so a caller can
 * read it unconditionally. Every one of these means "send a full frame", but
 * they are worth keeping apart: only NOTHING is waste. COVERAGE and FRAGMENTED
 * mean a repaint is genuinely the cheaper answer, and reading one as the other
 * has already cost a wrong conclusion about where a session's bytes were going.
 */
enum {
	TERMGFX_DR_OK = 0,
	TERMGFX_DR_ARGS,        /* NULL or non-positive geometry */
	TERMGFX_DR_GRID,        /* grid past the sanity ceiling, or an allocation failed */
	TERMGFX_DR_NOTHING,     /* not one cell differs */
	TERMGFX_DR_COVERAGE,    /* dirty% >= full_pct */
	TERMGFX_DR_COMPONENTS,  /* more separate blobs than max_components */
	TERMGFX_DR_FRAGMENTED   /* still more than max_boxes after merging */
};

/* Compare `cur` against `prev` (both plane_w*plane_h bytes, one palette index
 * per pixel) and fill `out` with the boxes covering every difference. `out`
 * must hold cfg->max_components entries.
 *
 * `stale` is 256 bytes, nonzero for each palette index whose COLOUR is about to
 * change, or NULL for none. A cell of `prev` drawn with a stale index is dirty
 * even when not one of its pixels changed -- on a shared-register terminal
 * redefining that register recolours what is already drawn, and where the
 * palette is baked into the pixels (JXL) the colour moves under identical
 * indices. Passing this is what lets a caller patch ACROSS a palette change
 * instead of repainting; a caller that repaints on any palette change passes
 * NULL.
 *
 * Returns the box count, or 0 meaning SEND A FULL FRAME -- never an error: the
 * caller's full-frame path is always correct, so every awkward case degrades
 * into it rather than into a wrong picture. Boxes never overlap and come back
 * top-to-bottom, left-to-right, so painting them in order cannot leave a seam.
 */
int termgfx_dirty_find(const termgfx_dirty_cfg_t *cfg, const uint8_t *cur,
                       const uint8_t *prev, const uint8_t *stale,
                       termgfx_dirty_box_t *out);

/* Why the last call returned 0, and what share of cells it found dirty (-1 if
 * it never got that far). File-static rather than out-parameters because every
 * caller would have to thread them through and none of them acts on the
 * result -- only diagnostics read it. Single-threaded, like the doors. */
int         termgfx_dirty_last_reason(void);
int         termgfx_dirty_last_pct(void);
const char *termgfx_dirty_reason_name(int reason);

#ifdef __cplusplus
}
#endif

#endif /* TERMGFX_DIRTY_H_ */
