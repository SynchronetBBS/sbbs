/* syncmoo1_geom.h -- pure sixel fit/encode geometry for the syncmoo1 door.
 *
 * Split out of syncmoo1_io.c so it can be unit-tested without linking the io
 * module (which pulls in termgfx's audio manager, and with it C++/libADLMIDI).
 * Same rationale as syncmoo1_map.c: pure integer math, no I/O, no engine.
 *
 * Two steps, in order:
 *
 *   sm_geom_fit_page()    usable text page (px) -> displayed image size
 *   sm_geom_encode_dims() displayed image size  -> sixel raster to encode
 *
 * The split matters because the terminal, not the door, applies the final
 * pixel-aspect upscale: a sixel encoded W x H with pan/pad = 2 DISPLAYS at
 * 2W x 2H.  So the encode is always the displayed size divided by 2, and the
 * frame the game drew (SM_FB_W x SM_FB_H) has to be resampled into it.
 */
#ifndef SYNCMOO1_GEOM_H_
#define SYNCMOO1_GEOM_H_

/* 1oom's native indexed framebuffer ("320x200, 8-bit palettized"). */
#define SM_FB_W 320
#define SM_FB_H 200

/* The pixel-aspect upscale we ASK the terminal for when the displayed size can
 * afford it: SyncTERM/cterm renders each encoded sixel pixel as a pad x pan
 * block, so a 320x200 encode at 2/2 DISPLAYS at 640x400 while sending ~1/4 the
 * raster bytes of a pre-upscaled 640x400 sixel -- the terminal does the
 * nearest-neighbor doubling (sixel.h, termgfx_geom doc).
 *
 * These are a MAXIMUM, not a constant: sm_geom_encode_dims() drops an axis to
 * 1 when 2 would force the encode below the native frame on that axis.  See
 * the loss note there. */
#define SM_SIXEL_PAN_MAX 2
#define SM_SIXEL_PAD_MAX 2

/* The displayed width at which the encode is exactly the native frame, so the
 * horizontal resample degenerates into a straight copy.  See the loss note in
 * sm_geom_fit_page(). */
#define SM_GEOM_NATIVE_EW (SM_FB_W * SM_SIXEL_PAD_MAX)

/* Generous upper bound on the fitted sixel width -- termgfx_geom_fit_ex() just
 * needs SOME cap (0 would mean uncapped); way above any real terminal canvas
 * this door will ever be centered in. */
#define SM_GEOM_SCALE_MAX 2048

/* Leftover-letterbox stretch allowance handed to termgfx_geom_fit_ex().
 * (termgfx_geom_fit()'s own wrapper bakes in this same 8.) */
#define SM_GEOM_STRETCH_PCT 8

/* Fit the native frame into a text page `pagew` x `pageh` pixels, whose cell
 * height is `ch`.  Reserves one cell row off the bottom (a sixel whose final
 * band lands on the last text row scrolls the page, and the continuously
 * re-presenting game then jitters); *fith is that reserved height, which the
 * caller centers within.  *ew x *eh is the DISPLAYED image size.
 *
 * Any of ew/eh/fith may be NULL.
 */
void sm_geom_fit_page(int pagew, int pageh, int ch, int *ew, int *eh, int *fith);

/* Sixel raster to encode for a displayed image of `ew` x `eh`, and the
 * pixel-aspect the terminal must apply to it: the image displays at
 * (*sxw * *pad) x (*sxh * *pan).
 *
 * `is_syncterm` selects the strategy. On SyncTERM (and cterm), each encoded
 * sixel pixel renders as a pad x pan block, so each axis takes the upscale (2)
 * whenever the resulting encode still covers the native frame on that axis --
 * ~1/4 the raster bytes. This is not merely an optimization: the resample in
 * sm_io_scale_indices() is nearest-neighbour, so an encode smaller than the
 * source DELETES rows/columns and the UI font's 1-pixel strokes go with them;
 * upsampling duplicates and never loses.
 *
 * On any OTHER sixel terminal (Windows Terminal, xterm, ...) pad/pan are BOTH
 * forced to 1 and the encode is the full displayed size. Those terminals read
 * pan;pad as a 1:1 pixel aspect and draw the sixel at its encoded size rather
 * than integer-upscaling it -- so a pad/pan=2 encode would show at half size
 * AND desync the mouse mapping (which assumes the image fills ew x eh). The 1:1
 * encode costs ~4x the bytes but fills the fitted rect and keeps the cursor
 * aligned on every terminal.
 *
 * The height is clamped DOWN to a whole number of 6-row sixel bands (a partial
 * final band renders wrong under pan>1).  Both are floored at one pixel / one
 * band so a degenerate probe can't produce a zero-sized encode.
 *
 * Any of sxw/sxh/pad/pan may be NULL.
 */
void sm_geom_encode_dims(int ew, int eh, int is_syncterm, int vscale_ok,
                         int *sxw, int *sxh, int *pad, int *pan);

#endif /* SYNCMOO1_GEOM_H_ */
