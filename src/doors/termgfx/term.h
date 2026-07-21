#ifndef TERMGFX_TERM_H_
#define TERMGFX_TERM_H_

#include <stddef.h>
#include <stdint.h>

// term.h -- canonical terminal control strings for a full-screen sixel game
// door (SyncDOOM, SyncDuke, ...). Each is a NUL-terminated ASCII/C0 string with
// no embedded NULs; the caller emits it through its own output path (e.g.
// out_put(s, strlen(s))) so there are no hand-counted byte lengths to get wrong.
//
// term_enter sets DECSDM (DEC private mode 80, "sixel scrolling") via ?80l.
// Despite the name, its load-bearing effect for us is POSITIONING: under ?80l a
// non-SyncTERM sixel terminal (e.g. Windows Terminal) draws the image at the TEXT
// CURSOR, so a door centers it by parking the cursor at the centered cell -- drop
// ?80l and those terminals draw at the origin (top-left). (SyncTERM's cterm instead
// ignores the cursor under ?80l and anchors top-left, which the door accounts for
// separately.) Scroll-prevention -- a sixel reaching the bottom row scrolling the
// page -- is now handled by keeping the image off the LAST text row (the bottom-cell
// reserve in the image fit), not by ?80. NB cterm reversed mode 80's set/reset sense
// in rev 1.328 (2026); moot here since SyncTERM uses JXL and the reserve covers
// scrolling regardless. termgfx_term_leave restores ?80h for the BBS.

// term_enter also sends DECSET 1070 RESET (?1070l) to select SHARED sixel colour
// registers. This is load-bearing for every door here, because they all re-send the
// 256-colour palette only when it CHANGES (see sixel.h's emit_palette: SyncTERM
// garbles its decoder if the registers are redefined every frame). That optimization
// silently assumes the registers persist across images -- true of SyncTERM's cterm,
// which has no per-image palette reset, and FALSE by default elsewhere.
//
// foot defaults to PRIVATE colour registers (use_private_palette = true) and, in
// sixel_init(), allocates a fresh palette per image seeded from only its 16 default
// sixel colours; palette_size is 256, so indices 16-255 come from xcalloc -- zero,
// i.e. fully TRANSPARENT. A palette-less frame therefore renders almost entirely
// see-through and the text grid shows through it. Alternating those with the
// occasional palette-carrying frame reads as the picture flickering against the
// terminal's previous text screen (observed in syncmoo1 on foot; 126 of the 140
// colour indices in a real frame are >= 16). With ?1070l foot takes its shared-palette
// branch, whose own comment reads "Shared palette - do *not* reset palette for new
// sixels" -- exactly the persistence the doors rely on. xterm implements 1070 too;
// cterm has no case 1070 and ignores it, so SyncTERM is unaffected either way.
// term_leave restores the ?1070h default.

// Enter graphics mode: clear screen + home cursor, hide the cursor (DECTCEM),
// disable autowrap (DECAWM) so a full-width frame can't wrap/scroll, set
// DECSDM ?80l (sixel-drawn-at-cursor, for centering -- see above), and select
// shared sixel colour registers (?1070l -- see above). Emit once on entry, before
// the first sixel: a private-register terminal resets the palette per IMAGE, so
// this has to precede the frame that defines it.
extern const char *const termgfx_term_enter;

// Probe the terminal's pixel canvas: ESC[14t (text-area size in pixels) with a
// cursor-extreme + DSR fallback (park cursor at 999;999, ask its position). The
// reply (ESC[4;H;Wt or ESC[r;cR) is parsed by the door's input layer. Wrapped in
// ESC7/ESC8 so the real cursor position is preserved.
extern const char *const termgfx_term_probe;

// Leave graphics mode: restore private sixel colour registers (?1070h, the
// default), sixel scrolling (?80h), autowrap (?7h), and the cursor (?25h) so the
// BBS prompt behaves normally after the door exits.
extern const char *const termgfx_term_leave;

// Status line (DECSSDT). A terminal that shows a status line reserves its
// bottom text row for it (SyncTERM's default: an 80x25 terminal draws to an
// 80x24 / 640x384 canvas), so a 640x400 game fractionally downscales and loses
// single-pixel detail. termgfx_term_status_off hides it (DECSSDT Ps=0) so the
// door draws the full canvas 1:1 -- emit it BEFORE the canvas probe so the
// probe reports the reclaimed size. It is prefixed with a DECRQSS query of the
// current setting; feed inbound bytes to termgfx_term_parse_status() to capture
// the reply (the pre-door status type, or -1 if unsupported), then restore it
// on exit with a termgfx_term_status_set() string. Ignored by terminals with no
// status line / no DECSSDT -- harmless.
extern const char *const termgfx_term_status_off;
size_t termgfx_term_status_set(char *out, size_t sz, int type);
int    termgfx_term_parse_status(const uint8_t *acc, int len);

// XTVERSION (xterm identification). Query with "\x1b[>0q"; the reply is a DCS
// string > | <name>(<version>) ST. Feed inbound bytes to
// termgfx_term_parse_xtversion() (a rolling window) to learn whether the
// terminal is xterm: 1 = xterm, 0 = a different, self-identified terminal,
// -1 = no complete reply yet. A door needs this to decide the SIXEL ceiling --
// xterm's ESC[14t text-area size is NOT its graphics ceiling (it discards a
// sixel larger than ~1000x1000 whole), so a positively-identified xterm must
// fall back to TERMGFX_SIXEL_SAFE_MAX rather than trust its own big canvas.
int    termgfx_term_parse_xtversion(const uint8_t *acc, int len);

#endif // TERMGFX_TERM_H_
