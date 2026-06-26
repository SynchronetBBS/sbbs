#ifndef TERMGFX_TERM_H_
#define TERMGFX_TERM_H_

// term.h -- canonical terminal control strings for a full-screen sixel game
// door (SyncDOOM, SyncDuke, ...). Each is a NUL-terminated ASCII/C0 string with
// no embedded NULs; the caller emits it through its own output path (e.g.
// out_put(s, strlen(s))) so there are no hand-counted byte lengths to get wrong.
//
// The important, easy-to-miss one is DECSDM (DEC private mode 80, "sixel
// scrolling"), which defaults to SET: whenever a sixel image reaches the bottom
// row the terminal scrolls the page up and appends a newline. For a door that
// emits a full-screen sixel every frame that means the picture visibly
// scrolls/jumps each frame. termgfx_term_enter RESETS it (?80l) so the sixel
// origin pins to the page top-left and frames overdraw in place; termgfx_term_leave
// restores the default (?80h) for the BBS.

// Enter graphics mode: clear screen + home cursor, hide the cursor (DECTCEM),
// disable autowrap (DECAWM) so a full-width frame can't wrap/scroll, and reset
// sixel scrolling (DECSDM ?80l). Emit once on entry.
extern const char *const termgfx_term_enter;

// Probe the terminal's pixel canvas: ESC[14t (text-area size in pixels) with a
// cursor-extreme + DSR fallback (park cursor at 999;999, ask its position). The
// reply (ESC[4;H;Wt or ESC[r;cR) is parsed by the door's input layer. Wrapped in
// ESC7/ESC8 so the real cursor position is preserved.
extern const char *const termgfx_term_probe;

// Leave graphics mode: restore sixel scrolling (?80h), autowrap (?7h), and the
// cursor (?25h) so the BBS prompt behaves normally after the door exits.
extern const char *const termgfx_term_leave;

#endif // TERMGFX_TERM_H_
