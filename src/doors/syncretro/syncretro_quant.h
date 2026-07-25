/* syncretro_quant.h -- RGB24 -> 256-color indexed, for the sixel tier.
 *
 * The sibling doors (../syncdoom, ../syncduke, ../syncmoo1) all hand termgfx an
 * already-indexed framebuffer, because their engines render into an 8-bit
 * palette. A libretro core does not: it renders truecolor (XRGB8888 / RGB565 /
 * 0RGB1555, all widened to RGB24 by retro_bridge.c). termgfx's sixel encoder
 * (../termgfx/sixel.h) takes indices + a 256-entry palette, so SyncRetro has to
 * quantize. This module is that step, and nothing else -- no I/O, and no state
 * beyond the color->register assignment described below, so tests drive it
 * directly.
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
 * EXACT mode assigns each color a sixel color REGISTER and never moves it: a
 * color seen in frame N holds the same register in frame N+1, whatever else
 * joins or leaves the frame. New colors take the lowest free register. Unused
 * entries are zero-filled.
 *
 * That stability is not a nicety, it is correctness. syncretro_io.c asks the
 * terminal for SHARED color registers (`CSI ? 1070 l`, without which foot and
 * xterm reset the palette on every image), so redefining register k recolors
 * every pixel ALREADY ON SCREEN that was drawn with k -- and the palette is sent
 * before the new pixel data. A register that changes meaning therefore paints
 * the previous frame in the wrong colors, briefly, every time it happens.
 *
 * This module used to sort the palette by color value, which made it a function
 * of the color SET alone. That is stable only while the set is: a frame that
 * gains one color shifts every register above the newcomer's sort position. It
 * looked fine on games with a fixed color set, and flickered at ~12 Hz on
 * 4-TRIS's title screen, whose animation cycles between 8 and 11 colors.
 *
 * The register map is per session. A console with a small fixed palette
 * (Intellivision: 16, NES: 64) never fills it. A console whose games fade or
 * cycle their palette fills it in seconds -- Street Fighter II's attract mode
 * re-tints every color of a ~150-color screen every frame -- so what happens
 * when the 256 registers run out is not an edge case there, it is the steady
 * state. A color new to a full map takes the LEAST RECENTLY USED register,
 * and never one belonging to this frame or to the frame the terminal is
 * displaying.
 *
 * When a fade leaves nothing else to take -- the displayed frame's colors and
 * the incoming frame's together outnumber the registers -- the incoming color
 * is DRAWN WITH ITS NEAREST NEIGHBOR instead. The frame is then no longer
 * pixel-exact, and that is the deliberate trade: an approximated color is a
 * couple of percent of the pixels a few RGB steps off, where redefining a
 * register the terminal is displaying recolors the whole picture until the
 * repaint catches up -- and the repaint is tens of kilobytes, so the bottom of
 * the image stays wrong the longest. Accuracy is the cheaper thing to give up.
 * It is also self-limiting: once the palette settles, the registers the fade
 * abandoned age out and the colors come back exact.
 */
#ifndef SYNCRETRO_QUANT_H_
#define SYNCRETRO_QUANT_H_

#include <stdint.h>

/* Quantize `rgb` (w*h*3, top-down, 8 bits per channel) into `idx` (w*h palette
 * indices) and `pal` (256 RGB triples = 768 bytes). Caller owns both outputs.
 *
 * Returns 1 if the palette is this frame's own colors (<= 256 distinct), 0 if
 * the frame was mapped onto the fixed cube. Both outputs are always valid. A 1
 * is NOT a promise of a pixel-exact frame: see the register map above for the
 * one case (a saturated map) where some colors are drawn with a neighbor.
 *
 * EXACT mode carries PER-SESSION STATE: a color keeps the register it was first
 * assigned, for as long as the session lives. See the header comment above for
 * why that, and not a sorted palette, is what a shared-register terminal needs. */
int sr_quant_rgb_to_indexed(const uint8_t *rgb, int w, int h,
                            uint8_t *idx, uint8_t *pal);

/* Forget every color->register assignment. Call once per session, before the
 * first frame; the tests also use it to isolate cases. */
void sr_quant_reset(void);

#endif /* SYNCRETRO_QUANT_H_ */
