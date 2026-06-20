// Text / block-character render tiers for syncdoom, lifted and adapted from
// ludocode/doom-cli (GPL-2.0). Converts the doomgeneric framebuffer to ANSI
// block-character terminal output (the CP437/ANSI and ASCII fallback tiers for
// terminals without SyncTERM's image-cache / sixel). See CREDITS.
//
// This first cut implements the HALF-block and SPACE modes (quadrant/sextant
// UTF-8 hi-res modes to follow). Output is built into an internal buffer the
// caller drains via rt_render_frame(); no terminal/stdout I/O here.

#ifndef SYNCDOOM_RENDER_TEXT_H_
#define SYNCDOOM_RENDER_TEXT_H_

#include <stdint.h>
#include <stddef.h>

// RT_BLOCKS: 2x2 sub-cell detail like RT_QUADRANT, but rendered with only the
// classic CP437 block + shade glyphs (halves/full/empty exact; single-quadrant
// and diagonal patterns approximated by light/medium/dark shades). Unlike the
// quadrant-combination glyphs (U+2596-259F), every glyph it uses exists in
// legacy fonts (Windows conhost included) and as CP437 bytes -- so it renders
// where RT_QUADRANT/RT_SEXTANT show missing-glyph boxes.
typedef enum { RT_HALF = 1, RT_SPACE, RT_QUADRANT, RT_SEXTANT, RT_BLOCKS } rt_mode_t;
typedef enum { RT_24BIT = 1, RT_8BIT, RT_4BIT, RT_3BIT } rt_colors_t;
typedef enum { RT_UTF8 = 0, RT_CP437 } rt_charset_t;       // block glyph: U+2580 vs 0xDF

// Configure the canvas: cols x rows character cells, render mode, color depth,
// and block charset. Allocates the scaled pixel buffer and primes dithering.
void rt_config(int cols, int rows, rt_mode_t mode, rt_colors_t colors, rt_charset_t charset);

// Dither override: -1 auto (color-depth default), 0 off, 1 on. Re-derives the
// dither state immediately (cheap). Set from [video] dither / the Ctrl-N toggle.
void rt_set_dither(int pref);

// Render the current doomgeneric frame (DG_ScreenBuffer) to terminal bytes.
// Returns a pointer to the bytes and sets *len; valid until the next call.
const char *rt_render_frame(size_t *len);

#endif // SYNCDOOM_RENDER_TEXT_H_
