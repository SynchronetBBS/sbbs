/* syncretro_quant.h -- RGB24 -> 256-color indexed, for the sixel tier.
 *
 * The sibling doors (../syncdoom, ../syncduke, ../syncmoo1) all hand termgfx an
 * already-indexed framebuffer, because their engines render into an 8-bit
 * palette. A libretro core does not: it renders truecolor (XRGB8888 / RGB565 /
 * 0RGB1555, all widened to RGB24 by retro_bridge.c). termgfx's sixel encoder
 * (../termgfx/sixel.h) takes indices + a 256-entry palette, so SyncRetro has to
 * quantize. This module is that step, and nothing else -- pure, no I/O, no
 * state, so tests can drive it directly.
 *
 * Two modes, chosen per frame by the actual color count:
 *
 *   EXACT (<= 256 distinct colors) -- the palette IS the frame's colors, so the
 *     image round-trips with zero loss. Legacy consoles live here: Intellivision
 *     has 16 colors, the NES 64, the Master System 64. This is the common case
 *     and the whole reason not to reach for a general quantizer.
 *
 *   CUBE (> 256 distinct colors) -- a fixed 6x6x6 RGB cube plus a 40-step gray
 *     ramp, each pixel mapped to whichever is nearer. The palette is constant,
 *     which matters: sixel color registers are only re-sent when the palette
 *     changes (see sixel.h's emit_palette), so a stable palette keeps the wire
 *     cost of a busy frame down.
 *
 * In EXACT mode the palette is sorted, so it depends only on the SET of colors
 * in the frame, not on the order they were scanned. Two frames drawn from the
 * same colors therefore produce byte-identical palettes and the encoder skips
 * the ~4KB register re-definition. Unused palette entries are zero-filled.
 */
#ifndef SYNCRETRO_QUANT_H_
#define SYNCRETRO_QUANT_H_

#include <stdint.h>

/* Quantize `rgb` (w*h*3, top-down, 8 bits per channel) into `idx` (w*h palette
 * indices) and `pal` (256 RGB triples = 768 bytes). Caller owns both outputs.
 *
 * Returns 1 if the frame was reproduced EXACTLY (<= 256 distinct colors), 0 if
 * it was approximated onto the fixed cube. Both outputs are always valid. */
int sr_quant_rgb_to_indexed(const uint8_t *rgb, int w, int h,
                            uint8_t *idx, uint8_t *pal);

#endif /* SYNCRETRO_QUANT_H_ */
