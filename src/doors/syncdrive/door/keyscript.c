/* keyscript.c -- see keyscript.h. */
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include "keyscript.h"

static const struct {
	const char *name;
	int keycode;
	int ascii;
} names[] = {
	{ "enter", TERMGFX_KEY_ENTER, 0 }, { "esc", TERMGFX_KEY_ESCAPE, 0 },
	{ "space", ' ', ' ' }, { "up", TERMGFX_KEY_UP, 0 }, { "down", TERMGFX_KEY_DOWN, 0 },
	{ "left", TERMGFX_KEY_LEFT, 0 }, { "right", TERMGFX_KEY_RIGHT, 0 },
	{ "home", TERMGFX_KEY_HOME, 0 }, { "end", TERMGFX_KEY_END, 0 },
	{ "pgup", TERMGFX_KEY_PAGEUP, 0 }, { "pgdn", TERMGFX_KEY_PAGEDOWN, 0 }
};

static int parse_token(const char *t, size_t len, keyscript_step_t *st)
{
	size_t k;

	memset(st, 0, sizeof *st);
	st->kind    = KEYSCRIPT_KEY;
	st->ev.type = TERMGFX_EV_KEY_DOWN;
	if (len > 5 && strncmp(t, "wait:", 5) == 0) {
		char *end;
		long  ms = strtol(t + 5, &end, 10);

		if (end != t + len || ms < 0)
			return -1;
		st->kind = KEYSCRIPT_WAIT;
		st->ms   = (int)ms;
		return 0;
	}
	if (len == 4 && strncmp(t, "quit", 4) == 0) {
		st->kind = KEYSCRIPT_QUIT;
		return 0;
	}
	if (len == 6 && strncmp(t, "ctrl-", 5) == 0 && isalpha((unsigned char)t[5])) {
		st->ev.keycode = tolower((unsigned char)t[5]);
		st->ev.mods    = TERMGFX_MOD_CTRL;
		return 0;
	}
	if (len == 2 && t[0] == 'f' && t[1] >= '1' && t[1] <= '9') {
		st->ev.keycode = TERMGFX_KEY_F1 + (t[1] - '1');
		return 0;
	}
	for (k = 0; k < sizeof names / sizeof names[0]; k++) {
		if (strlen(names[k].name) == len && strncmp(t, names[k].name, len) == 0) {
			st->ev.keycode = names[k].keycode;
			st->ev.ascii   = names[k].ascii;
			return 0;
		}
	}
	if (len == 1 && t[0] > ' ' && t[0] < 0x7F && t[0] != ',') {
		st->ev.keycode = (unsigned char)t[0];
		st->ev.ascii   = (unsigned char)t[0];
		return 0;
	}
	return -1;
}

int keyscript_parse(const char *s, keyscript_step_t *out, int max)
{
	int n = 0;

	while (s != NULL && *s != '\0') {
		const char *comma = strchr(s, ',');
		size_t      len   = comma ? (size_t)(comma - s) : strlen(s);

		if (n >= max || parse_token(s, len, &out[n]) != 0)
			return -1;
		n++;
		s = comma ? comma + 1 : s + len;
	}
	return n;
}
