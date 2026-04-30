/* Reusable ANSI / DEC escape-sequence state machine.
 *
 * Lifted out of ripper.c's `ansi_only` so other consumers
 * (Hook.onMatchClean, speed-watch, future tools) can share the same
 * recogniser.  Behavior under ANSI_FILTER_KEEP_ESC matches ripper's
 * historical `ansi_only` byte-for-byte; the new ANSI_FILTER_KEEP_TEXT
 * is its complement, dropping the bytes that KEEP_ESC keeps. */

#include "ansi_filter.h"

size_t
ansi_filter(const uint8_t *in, size_t in_len, uint8_t *out,
            enum ansi_state *state, enum ansi_filter_mode mode)
{
	uint8_t *out_start = out;

	for (size_t i = 0; i < in_len; i++) {
		uint8_t b = in[i];
		/* Snapshot the entry state before transitioning so the
		 * "should I emit?" decision uses the *pre-transition*
		 * state.  KEEP_TEXT emits only when the state-pre-byte
		 * was NONE *and* the byte itself isn't ESC (which would
		 * start a sequence).  KEEP_ESC emits the byte that
		 * starts the sequence (the ESC) AND every byte while
		 * the state is non-NONE — matching ripper's original. */
		enum ansi_state entry = *state;
		bool emit_text = (mode == ANSI_FILTER_KEEP_TEXT &&
		                   entry == ANSI_STATE_NONE && b != 0x1B);
		bool emit_esc  = (mode == ANSI_FILTER_KEEP_ESC &&
		                   (entry != ANSI_STATE_NONE || b == 0x1B));

		if (out != NULL && (emit_text || emit_esc))
			*out++ = b;

		switch (entry) {
		case ANSI_STATE_NONE:
			if (b == 0x1B)
				*state = ANSI_STATE_GOT_ESC;
			break;

		case ANSI_STATE_GOT_ESC:
			if (b < '0' || b > '~') {
				*state = ANSI_STATE_NONE;
				break;
			}
			switch (b) {
			case 'X':
				*state = ANSI_STATE_GOT_STRING;
				break;
			case 'P':
			case ']':
			case '^':
			case '_':
				*state = ANSI_STATE_GOT_LIMITED_STRING;
				break;
			case '[':
				*state = ANSI_STATE_GOT_CSI;
				break;
			default:
				*state = ANSI_STATE_NONE;
				break;
			}
			break;

		case ANSI_STATE_GOT_LIMITED_STRING:
			if (b == 0x1B) {
				*state = ANSI_STATE_GOT_ESC_IN_STRING;
				break;
			}
			if (b < 0x08 || (b > 0x0d && b < 0x20) || b > 0x7e)
				*state = ANSI_STATE_NONE;
			break;

		case ANSI_STATE_GOT_ESC_IN_LIMITED_STRING:
			*state = ANSI_STATE_NONE;
			break;

		case ANSI_STATE_GOT_ESC_IN_STRING:
			if (b == 'X' || b == '\\')
				*state = ANSI_STATE_NONE;
			break;

		case ANSI_STATE_GOT_STRING:
			if (b == 0x1B)
				*state = ANSI_STATE_GOT_ESC_IN_STRING;
			break;

		case ANSI_STATE_GOT_CSI:
			if (b >= '@' && b <= '~')
				*state = ANSI_STATE_NONE;
			else if (b >= ' ' && b <= '/')
				*state = ANSI_STATE_GOT_IB;
			else if (b < '0' || b > '?')
				*state = ANSI_STATE_NONE;
			break;

		case ANSI_STATE_GOT_IB:
			if (b < '0' || b > '?')
				*state = ANSI_STATE_NONE;
			else if (b < ' ' || b > '/')
				*state = ANSI_STATE_NONE;
			break;
		}
	}

	return out == NULL ? 0 : (size_t)(out - out_start);
}
