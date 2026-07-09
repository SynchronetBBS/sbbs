/* Pure unit tests for syncmoo1_geom.c -- the sixel fit/encode math.
 *
 * The property that matters, and the bug these tests lock down: the horizontal
 * resample in sm_io_scale_indices() is nearest-neighbour, so an encode
 * NARROWER than the native frame silently DELETES source columns. MoO1's UI
 * font draws 1-pixel-wide strokes ('1', 'l', 'I'), which then vanish outright
 * -- "SFX volume 100" renders as "SFX volume 00", "1oom v1.11.8" as "v1.1.8".
 *
 * So: whenever the page is wide enough to show the frame at its native 2x,
 * sm_geom_fit_page() must yield exactly that width, even if a true-aspect fit
 * (which is height-limited on any page shorter than 400px) would be narrower.
 */
#include <assert.h>
#include "syncmoo1_geom.h"

/* Encoded width for a page, the way syncmoo1_io.c composes the two calls. */
static int encode_w(int pagew, int pageh, int ch)
{
    int ew, eh, sxw;

    sm_geom_fit_page(pagew, pageh, ch, &ew, &eh, 0);
    sm_geom_encode_dims(ew, eh, &sxw, 0, 0, 0);
    return sxw;
}

/* No source row or column may be dropped: an encode at least as large as the
 * native frame on both axes is exactly the condition for that. */
static void assert_lossless(int pagew, int pageh, int ch)
{
    int ew, eh, sxw, sxh;

    sm_geom_fit_page(pagew, pageh, ch, &ew, &eh, 0);
    sm_geom_encode_dims(ew, eh, &sxw, &sxh, 0, 0);
    assert(sxw >= SM_FB_W);
    assert(sxh >= SM_FB_H);
}

int main(void)
{
    int ew, eh, fith, sxw, sxh, pad, pan;

    /* 80x25 at an 8x16 font: the 640x400 canvas the door assumes by default.
     * The bottom-row reserve leaves 640x384; the <=8% stretch fills the width
     * back out to an exact 2x. Nothing is resampled horizontally. */
    sm_geom_fit_page(640, 400, 16, &ew, &eh, &fith);
    assert(fith == 384);
    assert(ew == 640 && eh == 384);
    /* Width affords the 2x aspect (640/2 == the native 320). Height does not:
     * 384/2 is 192, eight rows short of the native 200, and those eight rows
     * would be DROPPED. So the vertical takes pan=1 and upsamples instead. */
    sm_geom_encode_dims(ew, eh, &sxw, &sxh, &pad, &pan);
    assert(pad == 2 && sxw == SM_FB_W);
    assert(pan == 1 && sxh == 384);
    assert(sxw * pad == ew && sxh * pan == eh);   /* displays at the fitted size */

    /* 80x24 -- the same terminal showing a status line. The page is 384px, the
     * reserve leaves 368, and a true-aspect fit is 588 wide: an 8.8% leftover
     * bar, just past the 8% stretch allowance. Fitting to 588 encodes 294 px
     * and drops 26 of the 320 source columns. Must widen to the native 2x. */
    sm_geom_fit_page(640, 384, 16, &ew, &eh, &fith);
    assert(fith == 368);
    assert(ew == SM_GEOM_NATIVE_EW);
    assert(eh == 368);
    assert(encode_w(640, 384, 16) == SM_FB_W);
    /* 368 is not a multiple of 6: the band clamp trims the encode to 366, so
     * the image displays two pixels shy of the fit. */
    sm_geom_encode_dims(ew, eh, &sxw, &sxh, &pad, &pan);
    assert(pan == 1 && sxh == 366);

    /* Every 8x16 page from 20 rows up (any status-line/menu-bar arrangement a
     * sysop's terminal might have) is lossless on both axes. */
    for (int rows = 20; rows <= 60; ++rows) {
        assert(encode_w(640, rows * 16, 16) == SM_FB_W);
        assert_lossless(640, rows * 16, 16);
    }

    /* A wider page still fills edge to edge: 116x36 at a 6x13 cell (a real
     * foot session's probe reply). 348 > 320 is an UPscale -- nearest
     * neighbour duplicates columns there, it never drops one, so it is safe
     * and the picture fills the terminal. */
    sm_geom_fit_page(696, 468, 13, &ew, &eh, &fith);
    assert(fith == 455);
    assert(ew == 696 && eh == 455);
    /* A page this tall clears the native height at pan=2 (455/2 == 227 >= 200),
     * so it keeps the cheap 2x on BOTH axes -- no doubled raster here. */
    sm_geom_encode_dims(ew, eh, &sxw, &sxh, &pad, &pan);
    assert(pad == 2 && pan == 2);
    assert(sxw == 348 && sxh == 222);
    assert_lossless(696, 468, 13);

    /* Narrower than the native 2x: the widening cannot apply (it would run off
     * the page). The HORIZONTAL then takes pad=1 for the same reason the
     * vertical takes pan=1 -- 480/2 would encode 240 columns and drop 80 of
     * the 320. At pad=1 it upsamples to 480 and stays lossless. */
    sm_geom_fit_page(480, 400, 16, &ew, &eh, 0);
    assert(ew <= 480 && ew > 0);
    assert(ew == 480 && eh == 300);
    sm_geom_encode_dims(ew, eh, &sxw, &sxh, &pad, &pan);
    assert(pad == 1 && sxw == 480);
    assert(pan == 1 && sxh == 300);
    assert_lossless(480, 400, 16);

    /* Degenerate short page (10 rows): widening to 640 here would squash the
     * frame below its own native height. Leave the true-aspect fit alone. */
    sm_geom_fit_page(640, 160, 16, &ew, &eh, 0);
    assert(ew < SM_GEOM_NATIVE_EW);
    assert(eh <= 160);

    /* A page too small to hold the native frame at all: both axes are already
     * at 1, and the downsample (with its dropped rows/columns) is unavoidable.
     * Nothing to guarantee here except that the encode stays inside the fit. */
    sm_geom_fit_page(640, 160, 16, &ew, &eh, 0);
    sm_geom_encode_dims(ew, eh, &sxw, &sxh, &pad, &pan);
    assert(pad == 1 && pan == 1);
    assert(sxw <= ew && sxh <= eh);

    /* Band clamp and the degenerate floors. */
    sm_geom_encode_dims(640, 800, &sxw, &sxh, &pad, &pan);
    assert(pan == 2 && sxh == 396);   /* 400 -> 396: whole 6-row bands only */
    sm_geom_encode_dims(1, 1, &sxw, &sxh, &pad, &pan);
    assert(sxw == 1 && sxh == 6);

    return 0;
}
