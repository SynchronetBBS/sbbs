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
// sixel terminal draws the image at the TEXT CURSOR, so a door centers it by
// parking the cursor at the centered cell, and paints a dirty-rect patch by
// addressing the cell it belongs over -- drop ?80l and the terminal draws at the
// origin (top-left) instead, ignoring the cursor entirely. Scroll-prevention -- a
// sixel reaching the bottom row scrolling the page -- is handled by keeping the
// image off the LAST text row (the bottom-cell reserve in the image fit), not by ?80.
//
// WHICH SEQUENCE MEANS "AT THE CURSOR" DEPENDS ON THE PEER. cterm reversed mode 80's
// set/reset sense in revision 1.328, so on 1.327 and below -- which includes
// SyncTERM 1.8, the current release -- ?80l is what selects origin-anchored
// drawing and ?80h selects the cursor. term_enter has to commit to one before the
// terminal has identified itself, and picks the DEC/xterm/foot-correct ?80l; a
// door that learns the peer's cterm revision (from its DA1 reply) then sends
// termgfx_term_sixel_at_cursor() to correct it.
//
// Nothing is sent on the way out, and termgfx_term_leave carries no mode-80
// sequence of its own: the correction above already leaves the terminal drawing at the
// cursor, which is every terminal's own power-on default (cterm_reset() sets
// SXSCROLL; DECSDM defaults to reset), so there is nothing left to hand back.
// The ?80h term_leave used to carry was the old polarity's idea of that default,
// and on a cterm >= 1.328 it set origin-anchored drawing on the way out instead.

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
// default), autowrap (?7h), and the cursor (?25h) so the BBS prompt behaves
// normally after the door exits. Mode 80 is deliberately NOT in here -- see the
// note above: the terminal is already at its default by then.
extern const char *const termgfx_term_leave;

// The DECSDM sequence that makes THIS peer draw a sixel at the text cursor
// rather than at the screen origin, given the cterm revision its DA1 reply
// carried (termgfx_caps_cterm_version(); <= 0 for "not cterm, or not answered").
// "\x1b[?80h" below TERMGFX_CTERM_VER_SDM, "\x1b[?80l" at or above it and for
// every non-cterm terminal. Never NULL, always safe to re-send.
//
// Send it once the DA1 reply lands, and repaint if it differs from what
// term_enter sent, since everything drawn until then went to the wrong place.
// That also settles the exit: draw-at-cursor IS every terminal's default, so a
// door that has corrected the mode has nothing to restore on the way out.
const char *termgfx_term_sixel_at_cursor(int cterm_ver);

// The same answer, preferring what the terminal was MEASURED to do over what its
// version implies. `probed` is termgfx_sixel_sdm_verdict()'s verdict (1 = ?80h,
// 0 = ?80l, -1 = not measured / does not matter); `cterm_ver` is the fallback.
//
// Measurement wins because it covers what a revision number cannot: a terminal
// that reports no CTerm version at all, and one built from the VT340 manual,
// which documents mode 80 backwards -- those read the two sequences the way
// cterm <= 1.327 does, and nothing in their DA1 reply says so.
const char *termgfx_term_sixel_at_cursor_probed(int probed, int cterm_ver);

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
