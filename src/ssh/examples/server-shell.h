#ifndef DSSH_EXAMPLE_SERVER_SHELL_H
#define DSSH_EXAMPLE_SERVER_SHELL_H

#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* Match only this demo's finite command vocabulary. An arbitrary-length line
 * needs no retained input or allocation; unmatched input is drained to newline. */
enum example_shell_event {
	EXAMPLE_SHELL_DATA, EXAMPLE_SHELL_SKIP_LF, EXAMPLE_SHELL_EMPTY,
	EXAMPLE_SHELL_PING, EXAMPLE_SHELL_QUIT, EXAMPLE_SHELL_STOP_PARENT,
	EXAMPLE_SHELL_UNKNOWN,
};
static const struct {
	const char *text;
	enum example_shell_event event;
} example_shell_commands[] = {
	{ "ping", EXAMPLE_SHELL_PING },
	{ "quit", EXAMPLE_SHELL_QUIT },
	{ "exit", EXAMPLE_SHELL_QUIT },
	{ "diediedie", EXAMPLE_SHELL_STOP_PARENT },
};
#define EXAMPLE_SHELL_COUNT (sizeof(example_shell_commands) / sizeof(example_shell_commands[0]))
_Static_assert(EXAMPLE_SHELL_COUNT < sizeof(unsigned) * CHAR_BIT, "command mask must fit");
struct example_shell_line {
	size_t position;
	unsigned candidates;
	bool nonempty;
	bool after_cr;
};
#define EXAMPLE_SHELL_INITIALIZER { .candidates = (1u << EXAMPLE_SHELL_COUNT) - 1u }

static inline enum example_shell_event
example_shell_byte(struct example_shell_line *line, uint8_t byte)
{
	if (line->after_cr) {
		line->after_cr = false;
		if (byte == '\n')
			return EXAMPLE_SHELL_SKIP_LF;
	}
	if (byte != '\r' && byte != '\n') {
		line->nonempty = true;
		if (line->candidates == 0)
			return EXAMPLE_SHELL_DATA;
		for (size_t i = 0; i < EXAMPLE_SHELL_COUNT; i++) {
			unsigned bit = 1u << i;
			const char *text = example_shell_commands[i].text;
			if ((line->candidates & bit) != 0
			    && (line->position >= strlen(text) || byte != text[line->position]))
				line->candidates &= ~bit;
		}
		/* A surviving candidate proves position < its finite string length. */
		if (line->candidates != 0)
			line->position++;
		return EXAMPLE_SHELL_DATA;
	}
	enum example_shell_event event = line->nonempty ? EXAMPLE_SHELL_UNKNOWN : EXAMPLE_SHELL_EMPTY;
	for (size_t i = 0; i < EXAMPLE_SHELL_COUNT; i++) {
		if ((line->candidates & (1u << i)) != 0
		    && line->position == strlen(example_shell_commands[i].text))
			event = example_shell_commands[i].event;
	}
	*line = (struct example_shell_line)EXAMPLE_SHELL_INITIALIZER;
	line->after_cr = byte == '\r';
	return event;
}

#endif
