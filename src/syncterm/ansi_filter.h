#ifndef ANSI_FILTER_H
#define ANSI_FILTER_H

/* Reusable ANSI / DEC escape-sequence state machine.
 *
 * One state field per stream — the caller owns it; calls are
 * re-entrant across streams.  Two filtering modes:
 *
 *   ANSI_FILTER_KEEP_ESC   — emit only escape-sequence bytes (the
 *                            ESC, intermediate, and final bytes).
 *                            Drops every plain-text byte.  Used by
 *                            ripper.c to extract the ANSI envelope
 *                            from a mixed RIP / ANSI stream.
 *
 *   ANSI_FILTER_KEEP_TEXT  — emit only plain-text bytes.  Drops the
 *                            ESC and every byte that follows up to
 *                            and including the final byte of the
 *                            sequence.  Used by Hook.onMatchClean
 *                            so a regex matches the visible text
 *                            even when colour codes split it.
 *
 * The state machine recognises:
 *   - 7-bit C1 escapes:  ESC <intermediate>* <final>
 *   - CSI sequences:     ESC [ <param>* <intermediate>* <final>
 *   - DCS/OSC/PM/APC:    ESC P|]|^|_ <body until ESC \>
 *   - SOS-style strings: ESC X <body until ESC \>
 *
 * It does NOT model 8-bit C1 (single-byte CSI/OSC at 0x9B etc.) —
 * the streams it sees are the standard 7-bit form BBSes use.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum ansi_state {
	ANSI_STATE_NONE = 0,
	ANSI_STATE_GOT_ESC,
	ANSI_STATE_GOT_CSI,
	ANSI_STATE_GOT_STRING,
	ANSI_STATE_GOT_LIMITED_STRING,
	ANSI_STATE_GOT_ESC_IN_STRING,
	ANSI_STATE_GOT_ESC_IN_LIMITED_STRING,
	ANSI_STATE_GOT_IB
};

enum ansi_filter_mode {
	ANSI_FILTER_KEEP_ESC,
	ANSI_FILTER_KEEP_TEXT
};

/* Filter `in_len` bytes from `in` through the state machine pointed
 * to by `state`, writing kept bytes to `out`.  `out` may alias `in`
 * (in-place filtering).  Returns the number of bytes written to
 * `out`.  Updates `*state` so the caller can resume on the next
 * chunk. */
size_t ansi_filter(const uint8_t *in, size_t in_len, uint8_t *out,
                   enum ansi_state *state, enum ansi_filter_mode mode);

/* True when `state` is mid-sequence (i.e. the last byte fed was
 * inside an escape — anything from ESC through the final byte).
 * Useful for callers that buffer sequence bytes themselves and want
 * a "did the sequence just end?" test:
 *
 *   bool was_in = ansi_in_sequence(state);
 *   ansi_filter(&byte, 1, NULL, &state, ANSI_FILTER_KEEP_ESC);
 *   if (was_in && !ansi_in_sequence(state)) { ...sequence complete... }
 */
static inline bool
ansi_in_sequence(enum ansi_state state)
{
	return state != ANSI_STATE_NONE;
}

#endif /* ANSI_FILTER_H */
