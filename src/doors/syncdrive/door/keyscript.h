/* keyscript.h -- SYNCDRIVE_KEYS: a comma-separated key script for headless
 * (capture-mode) test runs, e.g. "wait:3000,enter,wait:2000,right,quit".
 * Tokens: wait:<ms>, quit, enter, esc, space, up, down, left, right, home,
 * end, pgup, pgdn, f1..f9, ctrl-<letter>, or one printable character. */
#ifndef SYNCDRIVE_KEYSCRIPT_H_
#define SYNCDRIVE_KEYSCRIPT_H_

#include "termgfx_termio.h"

enum { KEYSCRIPT_WAIT, KEYSCRIPT_KEY, KEYSCRIPT_QUIT };

typedef struct {
	int kind;
	int ms;
	termgfx_input_event_t ev;
} keyscript_step_t;

/* Returns the number of steps, or -1 on an unknown token, a bad wait, or more
 * than `max` steps. */
int keyscript_parse(const char *s, keyscript_step_t *out, int max);

#endif
