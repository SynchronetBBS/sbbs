// geometry.c -- see geometry.h.  Shared image fit/center math for termgfx doors.

#include <stddef.h>
#include "geometry.h"

void termgfx_geom_fit(int vw, int vh, int src_w, int src_h, int scale_max,
                      int *ew, int *eh)
{
	// 8% is the historical built-in allowance -- keep every existing caller
	// (SyncDOOM, SyncDuke) behaving exactly as before.
	termgfx_geom_fit_ex(vw, vh, src_w, src_h, scale_max, 8, ew, eh);
}

void termgfx_geom_fit_ex(int vw, int vh, int src_w, int src_h, int scale_max,
                         int max_stretch_pct, int *ew, int *eh)
{
	int wmax, w, h;

	if (src_w < 1)
		src_w = 1;
	if (src_h < 1)
		src_h = 1;
	if (vw < 1)
		vw = 1;
	if (vh < 1)
		vh = 1;
	if (max_stretch_pct < 0)
		max_stretch_pct = 0;

	// Widest we'll emit: the canvas width, capped at scale_max.
	wmax = (scale_max > 0 && vw > scale_max) ? scale_max : vw;
	w = wmax;
	h = (int)((long)w * src_h / src_w);          // preserve the source aspect
	if (h > vh) {                                // height-limited instead
		h = vh;
		w = (int)((long)h * src_w / src_h);
	}
	if (w < 1)
		w = 1;
	if (h < 1)
		h = 1;

	// Stretch away a thin (<=max_stretch_pct%) leftover bar so a near-fit
	// fills cleanly.  max_stretch_pct == 0 means true aspect: no stretch,
	// letterbox bars stay full size.
	if (max_stretch_pct > 0) {
		if (vw > w && (long)(vw - w) * 100 <= (long)w * max_stretch_pct)
			w = vw;
		if (vh > h && (long)(vh - h) * 100 <= (long)h * max_stretch_pct)
			h = vh;
	}

	if (ew)
		*ew = w;
	if (eh)
		*eh = h;
}

int termgfx_geom_zoom(int src_w, int src_h, int ew, int eh, int *zx, int *zy)
{
	if (src_w < 1 || src_h < 1)
		return 0;
	// The terminal can only replicate whole pixels: no downscale, no remainder.
	if (ew < src_w || eh < src_h)
		return 0;
	if ((ew % src_w) != 0 || (eh % src_h) != 0)
		return 0;

	if (zx)
		*zx = ew / src_w;
	if (zy)
		*zy = eh / src_h;
	return 1;
}

int termgfx_geom_sixel_scale(int displayed, int native, int max_factor, int band)
{
	int f, n;

	if (native < 1 || displayed < 1 || band < 1)
		return 1;
	for (f = max_factor; f > 1; --f) {
		n = displayed / f;          // what we would encode at this factor
		n -= n % band;              // whole sixel bands only
		if (n >= native)            // still holds every source pixel?
			return f;
	}
	return 1;                       // no factor is lossless -> encode 1:1
}

void termgfx_geom_gfx_clamp(int gmax_w, int gmax_h, int *ew, int *eh)
{
	int w, h;

	if (ew == NULL || eh == NULL || gmax_w < 1 || gmax_h < 1)
		return;
	w = *ew;
	h = *eh;
	if (w < 1 || h < 1)
		return;

	if (w > gmax_w) {                   // too wide: scale the height with it
		h = (int)((long)h * gmax_w / w);
		w = gmax_w;
	}
	if (h > gmax_h) {                   // still too tall: scale the width with it
		w = (int)((long)w * gmax_h / h);
		h = gmax_h;
	}
	*ew = w < 1 ? 1 : w;
	*eh = h < 1 ? 1 : h;
}

void termgfx_geom_center_ex(int vw, int vh, int ew, int eh,
                            double cell_w, double cell_h,
                            int *dx, int *dy, int *col, int *row)
{
	int x = vw > ew ? (vw - ew) / 2 : 0;
	int y = vh > eh ? (vh - eh) / 2 : 0;

	if (dx)
		*dx = x;
	if (dy)
		*dy = y;
	if (col)
		*col = (cell_w > 0.0) ? 1 + (int)(x / cell_w) : 1;
	if (row)
		*row = (cell_h > 0.0) ? 1 + (int)(y / cell_h) : 1;
}

void termgfx_geom_center(int vw, int vh, int ew, int eh, int cell_w, int cell_h,
                         int *dx, int *dy, int *col, int *row)
{
	termgfx_geom_center_ex(vw, vh, ew, eh, (double)cell_w, (double)cell_h,
	                       dx, dy, col, row);
}
