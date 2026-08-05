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
#define SIXEL_PAL_NONE  0
#define SIXEL_PAL_FULL  1
#define SIXEL_PAL_USED  2
#define SIXEL_PAL_DELTA 3        // sixel_encode_delta() only -- takes a mask

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

// As sixel_encode, but (re)defines exactly the registers flagged in `changed`
// (256 bytes, nonzero = emit), whatever this image happens to reference.
//
// For a PERSISTENT-register terminal (SyncTERM) that is being sent a dirty box
// rather than a whole frame: the box itself carries no palette, so when the
// palette moves, the registers it moved have to reach the terminal somehow, and
// all 256 is most of a small box's cost. NONE/FULL/USED cannot express this --
// USED is driven by the image's own pixels, and the registers that CHANGED are
// not the same set.
//
// The caller owes the other half of the contract: a register redefined here
// recolors every pixel already on screen drawn with it, so every such cell must
// be inside the boxes being painted. See syncretro_dirty.h's `stale` argument.
size_t sixel_encode_delta(uint8_t **buf, size_t *cap, const uint8_t *idx, int w, int h,
                          const uint8_t *pal, const uint8_t *changed);

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
// The probe needs the terminal drawing sixels AT THE CURSOR, or the cursor never
// advances and it reads "does not scale" on a terminal that does. Which mode-80
// sequence asks for that is itself unknown for a terminal we cannot identify, so
// the probe DOES NOT ASSUME one: it runs the whole measurement twice, once under
// ?80h and once under ?80l, and reports which half moved the cursor. That answers
// both questions from one burst -- see termgfx_sixel_sdm_parse() below.
//
// The technique is hackerb9's testdecsdm.sh (github.com/hackerb9/vt340test),
// pointed out by Deuce: a terminal drawing at the cursor leaves it at the bottom
// of the image, one anchoring at the screen origin leaves it untouched, and that
// difference is visible over the wire where the mode's polarity is not. It matters
// beyond cterm, whose revision we can read: the VT340 manual documents mode 80
// backwards, so ANY terminal built from those specs reads the two sequences the
// way cterm <= 1.327 does, with no version to gate on.
//
// Six cursor reports come back, in this order:
//   [0] baseline under ?80h   [1] after a pan=1 sliver   [2] after a pan=2 sliver
//   [3] baseline under ?80l   [4] after a pan=1 sliver   [5] after a pan=2 sliver
// They pipeline: one write, one round trip, six replies.
//
// Writes the probe into buf (needs ~384 bytes); returns its length, 0 if cap is
// too small.
size_t termgfx_sixel_vscale_probe(char *buf, size_t cap);

// WHICH DOORS RUN THIS. SyncRetro, SyncDOOM, SyncDuke and SyncMOO1 do; the
// termgfx_termio doors (SyncSCUMM, SyncRPG) deliberately do NOT, and stay on the
// CTerm-revision inference alone (termgfx_term_sixel_at_cursor()). The probe is a
// SIXEL, so its slivers necessarily reach the wire ahead of the first frame, and
// this module's test suite pins "the first DCS on the wire is the frame" in five
// separate raster parsers plus an image count. That is a decision, not an
// oversight: those two doors are correct on every CTerm client and on every
// terminal that reads DECSDM the modern way, and wrong only on a NON-CTerm one
// built from the VT340 manual -- for which no named example is known.
//
// The mode-80 sequence the probe LEAVES the terminal in, being the last one its
// two halves assert. A door that remembers what it last sent (so it can re-assert
// the mode only when it changes) MUST record this the moment it emits the probe:
// the probe sets the mode behind that bookkeeping's back, and a door still
// believing its own older value will skip the very re-assert the probe made
// necessary -- and sit in the wrong mode for the rest of the session.
const char *termgfx_sixel_probe_trailing_sdm(void);

// Which mode-80 sequence this terminal reads as draw-at-cursor, from the probe
// replies accumulated so far. Returns 1 for "\x1b[?80h" (the backwards-from-VT340
// reading: cterm <= 1.327, and anything built from the VT340 manual), 0 for
// "\x1b[?80l" (a genuine VT340, and cterm >= 1.328), and -1 when the answer is not
// in hand or does not matter -- fewer than six reports so far, or a terminal that
// drew at the cursor under BOTH sequences (mode 80 unimplemented, so either one is
// safe) or under NEITHER (it will not draw at the cursor at all; nothing to pick).
// A door treats -1 as "keep what termgfx_term_sixel_at_cursor() already chose".
int termgfx_sixel_sdm_parse(const char *acc, size_t len);

// The verdict half of the above, over rows already extracted (unit-testable).
int termgfx_sixel_sdm_verdict(const int *rows, int nrows);

// How many cursor reports the probe produces (two halves of three). A door sizing
// its own row buffer, or deciding it has heard enough, uses this.
#define TERMGFX_SIXEL_PROBE_ROWS 6

// Feed the accumulated input bytes. Returns 1 if the terminal scales vertically,
// 0 if it does not, and -1 while the cursor reports have not all arrived yet
// (keep reading until your deadline; on timeout treat it as 0 -- the safe answer,
// since a full-size encode is correct on every terminal, merely fatter).
// Idempotent over a growing buffer. Read off whichever probe half drew at the
// cursor, so it no longer depends on the caller having guessed the mode right.
int termgfx_sixel_vscale_parse(const char *acc, size_t len);

// The same verdict, for a door whose input state machine has already parsed the
// cursor reports into row numbers (SyncDuke, SyncMOO1) rather than handing us raw
// bytes. `rows` are the reported rows in order; returns 1/0 as above, or -1 if
// fewer than TERMGFX_SIXEL_PROBE_ROWS are in hand yet.
int termgfx_sixel_vscale_verdict(const int *rows, int nrows);

#endif // SIXEL_H_
