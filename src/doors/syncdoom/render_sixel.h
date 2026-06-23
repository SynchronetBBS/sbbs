#ifndef RENDER_SIXEL_H_
#define RENDER_SIXEL_H_

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

#endif // RENDER_SIXEL_H_
