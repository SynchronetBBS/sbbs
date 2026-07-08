// geometry.h -- shared image sizing/centering math for termgfx game doors.
//
// SyncDOOM and SyncDuke both letter-box a game framebuffer into the terminal's
// graphics canvas: scale it up to fill, then center it.  That math used to live
// only in syncdoom.c (compute_geometry), so SyncDuke diverged (small, top-left).
// These helpers are the shared core both doors call, so the behavior is identical
// by construction instead of copied.  Pure functions -- no terminal I/O, no state;
// each door learns the canvas/cell pixels its own way and passes them in.
//
// All dimensions are pixels.  A 0 input means "unknown" and the helpers degrade
// gracefully (e.g. unknown cell size -> top-left placement).

#ifndef TERMGFX_GEOMETRY_H_
#define TERMGFX_GEOMETRY_H_

// Fit a src_w x src_h image into a vw x vh canvas, preserving the source aspect
// and scaling to fill, with the emitted width capped at scale_max (0 = uncapped).
// A small (<=8%) leftover letterbox bar is stretched away to the canvas edge so a
// near-fit doesn't leave a thin border.  Results (>=1) go to *ew/*eh (NULL ok).
// A thin wrapper over termgfx_geom_fit_ex() with that 8% allowance baked in --
// existing callers (SyncDOOM, SyncDuke) are unaffected by anything below.
void termgfx_geom_fit(int vw, int vh, int src_w, int src_h, int scale_max,
                      int *ew, int *eh);

// Same fit as termgfx_geom_fit(), but with the leftover-bar stretch allowance
// given explicitly as max_stretch_pct (0 = true aspect, no stretch at all --
// letterbox bars stay full size; termgfx_geom_fit() calls this with 8).
void termgfx_geom_fit_ex(int vw, int vh, int src_w, int src_h, int scale_max,
                         int max_stretch_pct, int *ew, int *eh);

// Center an ew x eh image in a vw x vh canvas.  *dx/*dy = the pixel offset (e.g.
// for a SyncTERM APC DrawJXL DX/DY); *col/*row = the 1-based text-cell origin
// derived from cell_w/cell_h (1,1 -- top-left -- when either cell dim is 0, since
// the cell math would be a guess).  Any out-ptr may be NULL.
void termgfx_geom_center(int vw, int vh, int ew, int eh, int cell_w, int cell_h,
                         int *dx, int *dy, int *col, int *row);

#endif // TERMGFX_GEOMETRY_H_
