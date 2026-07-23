#ifndef SIXEL_H_
#define SIXEL_H_

#include <stddef.h>
#include <stdint.h>

// emit_palette mode for sixel_encode()/sixel_encode_aspect(). NONE/FULL are
// the legacy 0/1: NONE emits no register definitions (reuse the terminal's
// persisted registers -- SyncTERM dirty boxes); FULL (re)defines all 256
// (persistent-terminal full frames, so later palette-less boxes can rely on
// them). USED (re)defines ONLY the registers this image references, at their
// original index -- for register-resetting terminals (xterm/foot/WT), where
// every image must self-describe its palette and all 256 is wasted bytes.
// Never pass USED where a later palette-less image will reuse these registers.
#define SIXEL_PAL_NONE 0
#define SIXEL_PAL_FULL 1
#define SIXEL_PAL_USED 2

// Encode a w*h PALETTED image as a complete DECSIXEL sequence. `idx` is w*h
// palette indices (0-255); `pal` is 256 RGB triples (768 bytes) giving each
// index's color. Sixel color registers map 1:1 to indices (register N == Doom
// palette index N), so the palette is identical frame to frame.
//
// `emit_palette` selects which register definitions are written
// (`SIXEL_PAL_NONE`/`FULL`/`USED`): pass `SIXEL_PAL_FULL` on the first frame
// and whenever the palette actually changes (Doom only swaps it for
// damage/pickup/radsuit/menu tints); pass `SIXEL_PAL_NONE` otherwise, so the
// frame carries raster + band data ONLY and reuses the registers the terminal
// still holds from the previous image. That avoids re-sending ~4KB of palette
// every frame (and keeps the per-frame sixel string smaller and stable).
//
// Output is intro (ESC P 0;1;0 q), raster attributes, optional palette, the pixel
// bands, and ST (ESC \) -- all printable ASCII / C0, telnet-safe, no base64.
// P2=1 (transparent) tells terminals not to pre-fill/pre-clear the raster.
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

// --- does this terminal honor the raster attribute's VERTICAL scaling? --------
//
// Whether sixel_encode_aspect()'s pan is obeyed is NOT a property you can infer
// from the terminal's identity, and the doors have been guessing:
//
//   SyncTERM  scales BOTH axes (pan and pad as integer scales -- a cterm extension)
//   foot, Contour, Windows Terminal   honor pan only (the DEC vertical pixel aspect)
//   xterm, WezTerm                    honor neither -- they draw at the ENCODED size
//
// Send a half-height sixel to that last pair and the picture comes out half-height.
// So MEASURE it, rather than sniffing a name that rots: draw the same tiny sixel
// twice -- once with pan=1, once with pan=2 -- with a cursor-position report (DSR)
// before and after each. A terminal that honors pan advances the cursor twice as
// far for the pan=2 copy; one that ignores it advances the same. Comparing the two
// advances is self-calibrating: it needs no knowledge of the cell height, survives
// whatever the terminal's cursor-after-sixel convention is, and tests the BEHAVIOR
// we actually depend on.
//
// Requires sixel scrolling ENABLED (DECSDM reset -- "\x1b[?80l", which
// termgfx_term_enter already sends), or the cursor never moves and the probe reads
// "does not scale". Send it AFTER term_enter and BEFORE the first frame or any
// DSR-based frame pacing -- the probe's own CPR replies would otherwise be eaten by
// the pacing loop. It paints two thin black slivers at the top-left; the door's
// first full frame covers them.
//
// KNOWN BLIND SPOT -- the probe measures the SPACE the image occupies, not whether
// the content was STRETCHED to fill it. A terminal that honored pan by reserving a
// taller box and letterboxing the pixels inside it would advance the cursor exactly
// as far as one that scales, and we would read it as "scales" and send a half-height
// encode. Nothing on the wire can tell the two apart: the cursor position is the ONLY
// thing a terminal ever reports back about a sixel -- there is no pixel read-back and
// no query for how the raster attribute was interpreted -- so any probe built on this
// protocol inherits the hole. None of the terminals characterized so far behaves this
// way (see the matrix in README.md), and the failure is loud rather than silent: the
// picture appears half-size inside a full-size box. The remedy is the door's manual
// full-res tier (F4), which encodes 1:1 and is correct on ANY terminal regardless of
// what it does with pan -- which is precisely why that manual override earns its keep
// alongside the auto-detection.
//
// Writes the probe into buf (needs ~128 bytes); returns its length, 0 if cap is
// too small.
size_t termgfx_sixel_vscale_probe(char *buf, size_t cap);

// Feed the accumulated input bytes. Returns 1 if the terminal scales vertically,
// 0 if it does not, and -1 while the three cursor reports have not all arrived yet
// (keep reading until your deadline; on timeout treat it as 0 -- the safe answer,
// since a full-size encode is correct on every terminal, merely fatter).
// Idempotent over a growing buffer.
int termgfx_sixel_vscale_parse(const char *acc, size_t len);

// The same verdict, for a door whose input state machine has already parsed the
// cursor reports into row numbers (SyncDuke, SyncMOO1) rather than handing us raw
// bytes. `rows` are the three reported rows in order; returns 1/0 as above, or -1
// if fewer than three are in hand yet.
int termgfx_sixel_vscale_verdict(const int *rows, int nrows);

#endif // SIXEL_H_
