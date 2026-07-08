// geometry.c -- see geometry.h.  Shared image fit/center math for termgfx doors.

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

void termgfx_geom_center(int vw, int vh, int ew, int eh, int cell_w, int cell_h,
                         int *dx, int *dy, int *col, int *row)
{
	int x = vw > ew ? (vw - ew) / 2 : 0;
	int y = vh > eh ? (vh - eh) / 2 : 0;

	if (dx)
		*dx = x;
	if (dy)
		*dy = y;
	if (col)
		*col = (cell_w > 0) ? 1 + x / cell_w : 1;
	if (row)
		*row = (cell_h > 0) ? 1 + y / cell_h : 1;
}
