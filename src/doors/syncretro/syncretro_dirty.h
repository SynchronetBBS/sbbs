/* syncretro_dirty.h -- which parts of the frame actually changed.
 *
 * A console redraws its whole framebuffer every frame, but very little of it
 * MOVES: Pac-Man's maze is static while five sprites and a scatter of dots are
 * not. Sending the whole picture every time is most of the door's bandwidth,
 * and the terminal is already holding the previous frame -- a sixel image can
 * be drawn at any cell position, so the parts that changed can be painted over
 * it and the rest left alone.
 *
 * This module is the "which parts" half, and nothing else: no encoding, no
 * terminal, no I/O. It compares two equally-sized index buffers and returns a
 * few rectangles covering everything that differs. syncretro_io.c owns the
 * decision to use them and the emitting. Keeping the geometry separable is what
 * lets test_dirty.c pin the awkward cases (a moving sprite, scattered dots, a
 * full-screen flash) without a terminal.
 *
 * THE GRID IS THE TERMINAL'S CELL GRID, not an arbitrary tile size. A sixel
 * image can only be placed at a character cell, so a rectangle that is not
 * cell-aligned has to be snapped outward before it can be drawn -- and doing
 * that after the fact is where the off-by-ones live. Diffing on cell
 * boundaries in the first place means every rectangle this returns is already
 * placeable, and the caller never adjusts one.
 */
#ifndef SYNCRETRO_DIRTY_H_
#define SYNCRETRO_DIRTY_H_

#include <stdint.h>

/* A changed region, in PIXELS of the scaled frame. Always cell-aligned on its
 * top-left, and clamped to the frame on its bottom-right (the last row/column
 * of cells is usually a partial one). */
typedef struct {
	int x, y;      /* top-left, a cell corner */
	int w, h;      /* size in pixels, >= 1 */
	int col, row;  /* the same top-left, in cells -- what the caller positions at */
} sr_dirty_rect_t;

/* The most rectangles sr_dirty_find() will return. Past this the frame is too
 * fragmented to be worth patching and a full frame is cheaper -- see the
 * fragmentation note in the .c. */
#define SR_DIRTY_MAX_RECTS 12

/* Compare `cur` against `prev` (both w*h bytes, one palette index per pixel) on
 * a cw x ch cell grid and fill `out` with up to SR_DIRTY_MAX_RECTS rectangles
 * covering every difference.
 *
 * Returns the rectangle count, or 0 meaning SEND A FULL FRAME -- which covers
 * "nothing changed", "so much changed that patching costs more than repainting",
 * and "changed in too many separate places to describe cheaply". 0 is never an
 * error: the caller's full-frame path is always correct, so every awkward case
 * degrades into it rather than into a wrong picture.
 *
 * Rectangles never overlap and are returned in top-to-bottom, left-to-right
 * order, so painting them in order cannot leave a seam.
 *
 * band_align: when nonzero, snap each rect height to LCM(ch,6) -- a whole
 * number of BOTH text cells and 6px sixel bands -- so a cell-anchored terminal
 * (foot) has no partial trailing band to backfill as a black strip, and return
 * 0 (caller repaints a full frame) if a bottom-clamped rect cannot cover its
 * own changed rows. Pass 0 for terminals that don't need it (SyncTERM), which
 * keeps the cell-only geometry unchanged. */
int sr_dirty_find(const uint8_t *cur, const uint8_t *prev, int w, int h,
                  int cw, int ch, int band_align, sr_dirty_rect_t *out);

/* Percentage of the frame's cells that must change before sr_dirty_find() gives
 * up and asks for a full frame. Exposed for the test. */
#define SR_DIRTY_FULL_PCT 40

#endif /* SYNCRETRO_DIRTY_H_ */
