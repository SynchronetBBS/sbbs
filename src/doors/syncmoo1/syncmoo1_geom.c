/* syncmoo1_geom.c -- see syncmoo1_geom.h. */

#include "syncmoo1_geom.h"
#include "geometry.h"   /* termgfx: termgfx_geom_fit_ex */

void sm_geom_fit_page(int pagew, int pageh, int ch, int *ew, int *eh, int *fith)
{
    int w, h, usable;

    if (ch <= 0)
        ch = 1;

    /* Reserve ONE cell row at the bottom before fitting, so the image can never
     * reach the terminal's LAST text row. Guarded on a plausibly-sized page so
     * a tiny or bogus probe result can't reserve away most of the screen. */
    usable = (pageh > ch * 4) ? pageh - ch : pageh;

    termgfx_geom_fit_ex(pagew, usable, SM_FB_W, SM_FB_H,
                        SM_GEOM_SCALE_MAX, SM_GEOM_STRETCH_PCT, &w, &h);

    /* Widen a height-limited fit back out to the native 2x.  The fit above
     * preserves the source aspect, so any page shorter than 400px makes the
     * image narrower than 640 -- and an encode narrower than SM_FB_W means
     * sm_io_scale_indices()'s nearest-neighbour step drops source columns
     * outright, erasing the UI font's 1-pixel strokes.  Losing a whole glyph
     * is far worse than the few percent of aspect the letterbox bar would have
     * bought: MoO1's 320x200 pixels were never square on a CRT anyway, and the
     * vertical is already being resampled here regardless.
     *
     * SM_GEOM_STRETCH_PCT alone can't carry this: an 80x24 page (the same 80x25
     * terminal showing a status line) fits to 588, an 8.8% leftover bar, just
     * past the 8% allowance.  The guarantee has to be stated, not tuned.
     *
     * Only while the frame still gets at least its native height, though --
     * on a degenerately short page, widening to 640 would squash it below
     * 1:1 vertically, which trades one kind of loss for another. */
    if (w < SM_GEOM_NATIVE_EW && pagew >= SM_GEOM_NATIVE_EW && h >= SM_FB_H)
        w = SM_GEOM_NATIVE_EW;

    if (ew)
        *ew = w;
    if (eh)
        *eh = h;
    if (fith)
        *fith = usable;
}

/* Largest aspect factor (<= max) whose encode still covers `native` on this
 * axis, so the resample upsamples rather than dropping source pixels.  `band`
 * is the encode's granularity on the axis (6 for the vertical, whose sixel
 * bands are 6 rows tall; 1 for the horizontal) -- it has to be applied HERE,
 * because trimming a 200-row encode down to a whole 198 would drop two source
 * rows just as surely as a smaller factor would.  Falls back to 1 when even
 * that can't cover the native size -- a page too small for the frame, where a
 * downsample is simply unavoidable. */
static int sm_geom_aspect(int displayed, int native, int max, int band)
{
    int f, n;

    for (f = max; f > 1; --f) {
        n = displayed / f;
        n -= n % band;
        if (n >= native)
            return f;
    }
    return 1;
}

void sm_geom_encode_dims(int ew, int eh, int *sxw, int *sxh, int *pad, int *pan)
{
    int hpad = sm_geom_aspect(ew, SM_FB_W, SM_SIXEL_PAD_MAX, 1);
    int vpan = sm_geom_aspect(eh, SM_FB_H, SM_SIXEL_PAN_MAX, 6);
    int w = ew / hpad;
    int h = eh / vpan;

    h -= h % 6;
    if (sxw)
        *sxw = (w > 0) ? w : 1;
    if (sxh)
        *sxh = (h >= 6) ? h : 6;
    if (pad)
        *pad = hpad;
    if (pan)
        *pan = vpan;
}
