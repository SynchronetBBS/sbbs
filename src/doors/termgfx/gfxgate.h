#ifndef TERMGFX_GFXGATE_H_
#define TERMGFX_GFXGATE_H_

#include <stdint.h>

/* gfxgate.h -- the shared "this terminal cannot display the game" gate.
 *
 * Every graphical door here renders only through sixel or JXL, so a terminal
 * that advertises neither has to be turned away with a readable notice rather
 * than sprayed with DCS it will print as garbage. The decision has three
 * parts -- when the verdict may be trusted, what the verdict is, and what the
 * player is told -- and only the last is a matter of taste, so the first two
 * live here and every door shares them.
 *
 * No clock and no I/O of its own: the caller passes a monotonic millisecond
 * stamp and emits the notice through whatever output path it already owns
 * (the doors disagree about that -- some stage into a frame buffer, some
 * write the socket directly). Same contract as idle.h and stats.h.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* How long to keep waiting for a JXL capability reply after the terminal has
 * answered device-attributes without claiming sixel. A SyncTERM that supports
 * JXL but not sixel answers the two probes separately, so concluding "no
 * graphics" the instant DA1 lands would turn it away. */
#define TERMGFX_GFX_SETTLE_MS  2000

enum {
	TERMGFX_GFXGATE_PROCEED = 0,  /* a graphics tier, or a silent terminal: draw */
	TERMGFX_GFXGATE_WAIT,         /* a reply may still be in flight: draw nothing */
	TERMGFX_GFXGATE_REJECT        /* neither tier: show the notice and quit */
};

/* The verdict.
 *
 * `have_graphics` is the door's own answer to "can I draw for this terminal
 * at all" -- the OR of whichever tiers it actually implements. Which those
 * are is not this module's business and differs between doors: today they all
 * mean sixel or JXL, but that has changed before and will again. Deciding it
 * here instead would mean this file enumerating every door's tier ladder and
 * going stale each time one gains or loses a tier -- as this very comment did,
 * by naming a PPM tier that no longer exists anywhere.
 *
 * `jxl_answered` means the JXL query came back at all (either way), not that
 * JXL is supported. It is the late reply: a terminal answers device-
 * attributes promptly but its JXL capability separately, so a door with no
 * other tier must wait out the window before concluding anything.
 *
 * A terminal that never answered device-attributes is PROCEED, not REJECT:
 * silence is not a denial, and a graphics terminal that simply doesn't
 * implement DA1 must not be locked out. Only a terminal that answered, and
 * whose answer left the door with nothing to draw with, is rejected. */
int termgfx_gfxgate(int have_graphics, int probe_replied, int jxl_answered,
                    uint32_t probe_start_ms, uint32_t now_ms);

/* The notice text, resolved ONCE per process and cached; later calls return
 * the first result and ignore their arguments. Ready to write to the terminal
 * as-is (CRLF line endings, SGR reset around it).
 *
 * Resolution order, most specific first:
 *   1. `file`   -- its contents, verbatim apart from bare LF -> CRLF. The
 *                  sysop owns the layout, so multi-line and ANSI art both
 *                  work. Unreadable or oversized falls through with a
 *                  complaint on stderr rather than silently.
 *   2. `text`   -- a one-line replacement for the built-in wording, shown
 *                  with the same indent and surrounding blank lines.
 *   3. the built-in default.
 *
 * Either may be NULL or empty to skip that tier. */
const char *termgfx_gfxgate_notice(const char *file, const char *text);

/* The line that follows the notice while the door waits to be dismissed.
 *
 * The pause is not decoration: the BBS repaints its own menu the instant the
 * door returns, so a notice sent and immediately exited on is wiped before it
 * can be read -- the player sees a flicker and lands back at the prompt with
 * no idea why. The wait itself belongs to the door (each owns its socket and
 * its read path); this is only the wording, kept here so the doors agree.
 *
 * Deliberately separate from the notice above rather than appended to it: a
 * sysop who replaces the notice should not have to remember to re-add the
 * prompt, and a door configured not to pause should not print it. */
extern const char termgfx_gfxgate_prompt[];

/* How long a door should wait for that keypress before leaving anyway, when
 * the sysop has expressed no preference. A dead or unattended client must not
 * hold a node open indefinitely. */
#define TERMGFX_GFXGATE_PAUSE_SECS  15

/* The notice file a door looks for when [text] no_graphics_file is unset.
 * Shared so that "drop this file next to the ini" means the same thing at
 * every door, rather than each inventing its own name. */
#define TERMGFX_GFXGATE_FILE  "nographics.txt"

#ifdef __cplusplus
}
#endif

#endif /* TERMGFX_GFXGATE_H_ */
