#ifndef TERMGFX_IDLE_H_
#define TERMGFX_IDLE_H_

#include <stdint.h>

/* idle.h -- the shared idle-USER clock for termgfx game doors.
 *
 * WHY THIS EXISTS AT ALL, given that the BBS already has an inactivity
 * timeout: Synchronet's max_inactivity is enforced in input_thread(), against
 * a counter reset by any successful socket read. Every termgfx door paces its
 * frame loop with DSR acks -- it emits a frame plus ESC[6n and the terminal
 * answers, unprompted, about ten times a second -- so that counter is reset
 * continuously for the whole session and the threshold is never reached. The
 * BBS setting reads as configured and does nothing.
 *
 * So presence has to be judged on REAL INPUT, which only the door can tell
 * apart from its own pacing acks. Feed termgfx_idle_activity() from the key
 * and mouse dispatch ONLY -- never from the CSI path that handles the 'R'
 * pace-ack, 'c'/'S'/'t'/'u'/'y' capability replies, or the audio DSR ack, and
 * never from a synthetic key-up expiry timer, which fires for a user who has
 * already left.
 *
 * No I/O, no config parsing, no rendering, and no clock of its own: the caller
 * supplies a monotonic millisecond stamp. That keeps this usable both by the
 * doors that own their I/O loop and by those driven through termgfx_termio.
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	TERMGFX_IDLE_ACTIVE = 0,        /* within the threshold                    */
	TERMGFX_IDLE_WARN,              /* inside the grace window; see secs_left  */
	TERMGFX_IDLE_EXPIRED            /* grace elapsed; the caller should exit   */
} termgfx_idle_state_t;

typedef struct {
	uint32_t threshold_ms;          /* 0 = feature disabled */
	uint32_t warn_ms;               /* clamped to threshold_ms at init */
	uint32_t last_ms;               /* stamp of the last real user input */
} termgfx_idle_t;

/* threshold_s == 0 disables: poll() then always returns ACTIVE. warn_s is
 * clamped to threshold_s, so a misconfigured warn > timeout degrades to "warn
 * immediately, exit at the threshold" rather than underflowing. */
void termgfx_idle_init(termgfx_idle_t *idle, unsigned threshold_s,
                       unsigned warn_s, uint32_t now_ms);

/* Call on GENUINE user input only. Resets the clock; no penalty for warning. */
void termgfx_idle_activity(termgfx_idle_t *idle, uint32_t now_ms);

/* Current verdict. *secs_left (may be NULL) is the countdown in whole seconds,
 * rounded up, and is meaningful only for TERMGFX_IDLE_WARN; it is 0 otherwise. */
termgfx_idle_state_t termgfx_idle_poll(termgfx_idle_t *idle, uint32_t now_ms,
                                       unsigned *secs_left);

#ifdef __cplusplus
}
#endif

#endif /* TERMGFX_IDLE_H_ */
