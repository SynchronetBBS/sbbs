// text.h (termgfx) -- text / block-character render tiers for framebuffer game
// doors (SyncDOOM, SyncDuke, ...), lifted and adapted from ludocode/doom-cli
// (GPL-2.0). Converts an RGB888 framebuffer to ANSI block-character terminal
// output (the CP437/ANSI and ASCII fallback tiers for terminals without
// SyncTERM's image-cache / sixel). See CREDITS.
//
// The source framebuffer is passed to rt_render_frame() as RGB888 (game-agnostic;
// each door packs its own framebuffer). Output is built into an internal buffer
// the caller drains via rt_render_frame(); no terminal/stdout I/O here.

#ifndef TERMGFX_TEXT_H_
#define TERMGFX_TEXT_H_

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
// and block charset. Allocates the scaled pixel buffer + cell-diff shadow.
void rt_config(int cols, int rows, rt_mode_t mode, rt_colors_t colors, rt_charset_t charset);

// Render a w*h RGB888 framebuffer to terminal block-character bytes. The source
// is scaled to the configured cell grid internally. Returns a pointer to the
// bytes and sets *len; valid until the next call.
const char *rt_render_frame(const uint8_t *rgb, int src_w, int src_h, size_t *len);

// Cell-diff HUD exclusion: cells the door draws on top (stats overlay, message
// line, chat) which the game render must skip so it never repaints under them.
// Register the active rectangles (cell coords, [c0,c1) on row 'row') before each
// rt_render_frame; rt_exclude_clear() drops them all.
void rt_exclude_clear(void);
void rt_exclude_add(int row, int c0, int c1);

// Force a full repaint next frame (after the terminal is written behind the
// renderer's back, e.g. a manual row clear) so the shadow can't drift from screen.
void rt_invalidate(void);

#endif // TERMGFX_TEXT_H_
