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

// Can the terminal do this upscale for us?  If the fitted emit size (ew x eh) is
// an exact integer multiple of the source, report the per-axis factors in *zx/*zy
// (NULL ok) and return 1: the door can then encode the frame at its NATIVE size
// and let the terminal replicate pixels (graphics-APC ZX/ZY, gated on CTerm >=
// TERMGFX_CTERM_VER_ZOOM), skipping the door-side upscale and shipping far fewer
// bytes -- a 2x frame costs roughly 40% of the pre-upscaled one to encode.
//
// Returns 0 when the fit isn't an exact multiple (or would shrink the source):
// SyncTERM has NO resampler -- its only scaling, sixel raster attributes included,
// is integer pixel replication -- so a non-integer fit still has to be resampled
// door-side and sent at the fitted size, exactly as before.  Quality is identical
// either way: replicating pixels is what the door's nearest-neighbor upscale does.
int termgfx_geom_zoom(int src_w, int src_h, int ew, int eh, int *zx, int *zy);

// The largest sixel raster scale factor (pan/pad) that is still LOSSLESS on this
// axis -- i.e. whose encoded size stays at or above the native frame -- or 1 when
// no factor is.
//
// Two sizes get confused here, so be exact: the ENCODE is what carries the pixels,
// the DISPLAY is encode x factor (the terminal replicates). A bigger factor means a
// smaller encode and fewer bytes -- but the encode is resampled DOWN from the
// native frame, and the moment it falls below native you are throwing away pixels
// the game actually drew. The terminal then replicates them back to the right SIZE,
// so it still looks right-sized while the detail is gone for good.
//
// So: take the biggest factor whose encode still holds the whole native frame.
// `band` rounds the encode down to whole sixel bands (6 rows vertically, 1
// horizontally) BEFORE the comparison, because a partial final band garbles (SF
// syncterm #258) and that rounding can itself push the encode under native.
//
// Per axis, and the axes legitimately disagree: fitting 320x200 into 640x384 gives
// pad=2 (320 encoded columns IS native) but pan=1 (384/2 = 192 < 200). A door that
// hardcodes 2 on both axes silently drops 8 of the 200 source rows there. This is
// SyncMOO1's long-standing rule (sm_geom_aspect), hoisted so every door gets it.
int termgfx_geom_sixel_scale(int displayed, int native, int max_factor, int band);

// The largest sixel we dare emit when the terminal has NOT told us its graphics
// geometry (XTSMGRAPHICS ESC[?2;1S).
//
// This is not a stylistic cap, it is the difference between a picture and a black
// screen. xterm's maxGraphicSize defaults to 1000x1000, and an image whose DECLARED
// raster exceeds it is not clipped -- xterm aborts the parse and draws NOTHING
// (graphics_sixel.c: GetExtent -> finished_parsing + return). Worse, xterm ships
// with window ops DISABLED, so it answers neither ESC[14t nor the XTSMGRAPHICS
// query: a door that sizes from cols*cell (the only geometry it can still get) will
// happily build a 1024-wide sixel for a large window and have every frame thrown
// away. That is exactly what "SyncDOOM is black in a big xterm" was.
//
// So when nothing has advertised a limit, assume xterm's. A terminal that can take
// more will say so and get the bigger image; one that stays silent gets a picture
// instead of a void.
#define TERMGFX_SIXEL_SAFE_MAX 1000

// Center an ew x eh image in a vw x vh canvas.  *dx/*dy = the pixel offset (e.g.
// for a SyncTERM APC DrawJXL DX/DY); *col/*row = the 1-based text-cell origin
// derived from cell_w/cell_h (1,1 -- top-left -- when either cell dim is 0, since
// the cell math would be a guess).  Any out-ptr may be NULL.
void termgfx_geom_center(int vw, int vh, int ew, int eh, int cell_w, int cell_h,
                         int *dx, int *dy, int *col, int *row);

// Same, but with a FRACTIONAL cell size -- and a terminal's cell is very often
// fractional.  A cell is canvas/grid, and 1648 px over 164 columns is 10.05, not
// 10; on a display at 125% or 150% scaling the fraction is bigger still.
//
// Rounding it down costs `cols * frac` pixels of page width -- up to a hundred on
// a wide terminal -- and a caller that then derives its page as cols*cell_w is
// centering the image inside a page NARROWER THAN THE SCREEN.  The picture lands
// half that error to the left, which is exactly the "not quite centered" a player
// sees.  Keep the cell fractional, center against the true canvas, and only
// quantize at the last step, where the cell grid genuinely forces it (a sixel is
// drawn at the text cursor, so the origin must be a whole cell).
//
// termgfx_geom_center() is this with the cell dims widened to double, so existing
// callers behave exactly as before.
void termgfx_geom_center_ex(int vw, int vh, int ew, int eh,
                            double cell_w, double cell_h,
                            int *dx, int *dy, int *col, int *row);

#endif // TERMGFX_GEOMETRY_H_
