#ifndef TERMGFX_FOCUS_H_
#define TERMGFX_FOCUS_H_

#include <stddef.h>

/* focus.h -- DECSET 1004 focus-event tracking.
 *
 * With the window unfocused, a terminal still reports pointer motion made
 * for some OTHER window, and a door that reads motion as look-around turns
 * the view while nobody is playing. Mode 1004 is the only way to know: the
 * terminal sends CSI I when the window gains focus and CSI O when it loses
 * it.
 *
 * NOTHING MAY DEPEND ON THE EVENTS ARRIVING. SyncTERM does not implement
 * mode 1004 (src/conio/cterm.adoc), so a door must be fully playable on a
 * terminal that never reports focus at all -- which is why the state starts
 * focused and only a real focus-out can clear it.
 */

typedef struct {
	/* NOTE THE SENSE. What is stored is "focus has been LOST", so that a
	 * zeroed struct means focused -- doors memset their terminal state and
	 * a `focused` field would then start false, which reads as a window
	 * nobody is looking at and silently kills mouse look on every terminal
	 * that does not implement mode 1004. Storing the negative makes the
	 * default correct without anyone having to remember an initializer. */
	int blurred;
	int reported;   /* the terminal has answered at least once */
	int lost;       /* focus-out latch, cleared by termgfx_focus_take_lost */
} termgfx_focus_t;

extern const char *const termgfx_focus_enable;    /* "\x1b[?1004h" */
extern const char *const termgfx_focus_restore;   /* "\x1b[?1004l" */

/* Optional: a zeroed struct is already a valid, focused tracker. */
void termgfx_focus_init(termgfx_focus_t *f);

/* Offer one CSI sequence. Returns 1 if it was a focus event and the state
 * moved, 0 if the caller should keep looking.
 *
 * `par` is the parameter/intermediate run between the CSI and the final.
 * A focus event is a BARE final -- parlen must be 0. That is the whole
 * guard, and it is deliberately stricter than testing for a private prefix:
 * claiming CSI S for F4 turned SyncTERM's ESC[?2;0;640;400S graphics reply
 * into a keypress that dropped the player back to the BBS, and the lesson
 * is to accept only the exact shape wanted.
 */
int termgfx_focus_on_csi(termgfx_focus_t *f, const char *par, size_t parlen,
                         char final);

/* Is the window focused? True before any event has arrived. */
int termgfx_focus_have(const termgfx_focus_t *f);

/* Has this terminal ever reported focus? A door can use this to tell "no
 * events, so assume focused" from "focused, and it said so". */
int termgfx_focus_reported(const termgfx_focus_t *f);

/* Collect the focus-out edge, clearing it. Latched rather than sampled so
 * a focus-out and a focus-in between two polls still releases held keys:
 * if focus is lost mid-stride the key-up may never arrive, and on the byte
 * tier nothing else re-derives it. */
int termgfx_focus_take_lost(termgfx_focus_t *f);

#endif /* TERMGFX_FOCUS_H_ */
