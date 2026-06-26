#ifndef SIXEL_H_
#define SIXEL_H_

#include <stddef.h>
#include <stdint.h>

// Encode a w*h PALETTED image as a complete DECSIXEL sequence. `idx` is w*h
// palette indices (0-255); `pal` is 256 RGB triples (768 bytes) giving each
// index's color. Sixel color registers map 1:1 to indices (register N == Doom
// palette index N), so the palette is identical frame to frame.
//
// `emit_palette` controls whether the 256 register definitions are written: pass
// nonzero on the first frame and whenever the palette actually changes (Doom only
// swaps it for damage/pickup/radsuit/menu tints); pass zero otherwise, so the
// frame carries raster + band data ONLY and reuses the registers the terminal
// still holds from the previous image. That avoids re-sending ~4KB of palette
// every frame (and keeps the per-frame sixel string smaller and stable).
//
// Output is intro (ESC P q), raster attributes, optional palette, the pixel
// bands, and ST (ESC \) -- all printable ASCII / C0, telnet-safe, no base64.
// Bytes go into *buf (grown via realloc, *cap updated); returns the length.
size_t sixel_encode(uint8_t **buf, size_t *cap, const uint8_t *idx, int w, int h,
                    const uint8_t *pal, int emit_palette);

// As sixel_encode, but writes the raster pixel-aspect attribute "pan;pad instead
// of "1;1. SyncTERM/cterm renders each sixel pixel as a pan(tall) x pad(wide) block
// (see cterm_dec.c: sx_iv=pan, sx_ih=pad), so a w*h image with pan=pad=2 DISPLAYS
// at 2w x 2h while sending only ~1/2 the bytes of a pre-upscaled w*2 x h*2 sixel --
// the terminal does the nearest-neighbor doubling. NOTE: this is cterm's integer-
// scale interpretation; a strict-DEC terminal treats pan;pad as a 1:1-ratio aspect
// and won't upscale (it'd draw the image at native w*h). Targeted at SyncTERM.
size_t sixel_encode_aspect(uint8_t **buf, size_t *cap, const uint8_t *idx, int w, int h,
                           int pan, int pad, const uint8_t *pal, int emit_palette);

#endif // SIXEL_H_
