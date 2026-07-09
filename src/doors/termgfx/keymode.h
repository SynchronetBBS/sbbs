#ifndef TERMGFX_KEYMODE_H_
#define TERMGFX_KEYMODE_H_

#include <stddef.h>
#include <stdint.h>

// keymode.h -- terminal keyboard-mode negotiation for termgfx game doors.
//
// A terminal sends bytes on key PRESS and nothing on release. Games want key-up
// (hold-to-move, held-state pads), so every door here climbs the same ladder,
// best first:
//
//   evdev   SyncTERM only. Its CTDA reply advertises cap 8, so the door enables
//           physical key reports (CSI = 1 h) and suppresses the translated byte
//           stream (CSI = 2 h). Keys then arrive as CSI = <evdev-code> K (press)
//           / k (release). True key-up, and the identity is a LAYOUT-INDEPENDENT
//           physical keycode, so WASD works on AZERTY and Dvorak.
//
//   kitty   The terminal answered the CSI ? u progressive-enhancement query, so
//           the door pushes flags (CSI > 11 u: disambiguate | report-events |
//           report-all-keys) and keys arrive as CSI-u events with an explicit
//           press/repeat/release type. True key-up, keyed by codepoint.
//
//   bytes   Neither. The door approximates a hold with an auto-release timer.
//
// The two native paths are mutually exclusive in practice (SyncTERM doesn't
// speak kitty; kitty terminals don't advertise CTDA cap 8); the enable functions
// below enforce that regardless, evdev winning.
//
// I/O-free, like caps.h and term.h: these build the bytes and own the state, the
// door emits through its own output path and does its own socket reads. What
// stays door-side is the only genuinely per-game part -- the map from a key to
// that game's action.
//
// This was four near-identical copies (syncdoom, syncduke, syncconquer,
// syncretro) before it was one. The copies had already drifted: syncconquer
// never armed the enable-time settle window below, and syncretro's modifier
// decode ignored the right-hand Ctrl and Alt keycodes.

// Modifier bits reported by termgfx_evdev_modifier() and carried in a door's own
// held-modifier mask.
#define TERMGFX_MOD_SHIFT 1
#define TERMGFX_MOD_CTRL  2
#define TERMGFX_MOD_ALT   4

// The longest sequence any function here emits, plus a NUL. Size the caller's
// buffer with this and the length checks can never bite.
#define TERMGFX_KEYMODE_SEQ_MAX 16

// How long, after physical key reports are enabled, a door drops PRESS edges.
// SyncTERM resyncs the keys held at that instant, emitting a press report for
// each -- e.g. the Enter still held from selecting the door off the BBS menu.
// Acting on those skips an intro, fires a weapon, or presses Start. Releases and
// modifier tracking are always honored, so nothing can stick down; evdev sends
// no auto-repeat, so one drop suffices even if the key stays physically down.
#define TERMGFX_KEYMODE_SETTLE_MS 500

// Which key mode is in force. Zero-initialize; the enable/restore calls own it.
typedef struct {
	int kitty;             // kitty flags pushed
	int evdev;             // physical key reports enabled
	int settling;          // inside the post-enable settle window
	uint32_t enabled_ms;   // when evdev was enabled (the door's monotonic ms clock)
} termgfx_keymode_t;

// Emit once at term-enter: "does this terminal speak the kitty keyboard
// protocol?" A CSI ? <flags> u reply means yes -> termgfx_keymode_enable_kitty().
extern const char *const termgfx_keymode_query_kitty;

// Enable SyncTERM physical key reports. Call when the CTDA reply advertises cap
// 8. A no-op (returns 0) if either native mode is already active. Writes the
// enable sequence to `out` and arms the settle window off `now_ms`.
size_t termgfx_keymode_enable_evdev(termgfx_keymode_t *km, char *out, size_t sz,
                                    uint32_t now_ms);

// Push the kitty keyboard flags. Call on the CSI ? <flags> u reply. A no-op
// (returns 0) if either native mode is already active -- evdev wins, and a
// SyncTERM that advertises cap 8 doesn't speak kitty anyway.
size_t termgfx_keymode_enable_kitty(termgfx_keymode_t *km, char *out, size_t sz);

// Undo whatever was enabled, so the BBS gets its terminal back as it lent it,
// and clear the state (idempotent). Writes only the sequences that are actually
// needed, so a door can call this unconditionally from its leave path. Returns
// 0 when neither native mode was ever negotiated.
size_t termgfx_keymode_restore(termgfx_keymode_t *km, char *out, size_t sz);

// Should this evdev PRESS edge be dropped as part of the enable-time resync?
// Call once per non-modifier press; it self-disarms on the first press past the
// window. Always 0 when evdev isn't active.
int termgfx_keymode_evdev_settling(termgfx_keymode_t *km, uint32_t now_ms);

int termgfx_keymode_kitty_active(const termgfx_keymode_t *km);
int termgfx_keymode_evdev_active(const termgfx_keymode_t *km);

// --- pure decoders (no state) ------------------------------------------------

// Decode a kitty CSI-u parameter list, `key[:alt][;mod[:event]]` -- `par`/`len`
// are the CSI parameter bytes, without the intro or the final. Returns the key
// codepoint (0 if absent). *mod is the 1-based modifier field (1 = none), *ev
// the event type: 1 press, 2 repeat, 3 release. Also decodes the arrow finals,
// which kitty reports as CSI 1;mod:event A.
int termgfx_kitty_parse(const char *par, int len, int *mod, int *ev);

// Is Ctrl held, per kitty's 1-based modifier field? (bit 0 Shift, 1 Alt, 2 Ctrl)
int termgfx_kitty_ctrl(int mod);
int termgfx_kitty_shift(int mod);
int termgfx_kitty_alt(int mod);

// US-QWERTY evdev keycode -> ASCII, or 0 for a key with no simple ASCII form
// (arrows, nav, F-keys -- decode those by keycode). `shifted` picks the shifted
// column. Generic to the Linux evdev main block, not engine-specific.
char termgfx_evdev_ascii(int code, int shifted);

// Is `code` a modifier key? Returns TERMGFX_MOD_SHIFT/_CTRL/_ALT, or 0. Both the
// left and right keycodes of each pair are recognized.
int termgfx_evdev_modifier(int code);

#endif // TERMGFX_KEYMODE_H_
