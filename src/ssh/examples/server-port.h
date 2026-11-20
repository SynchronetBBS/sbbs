#ifndef DSSH_EXAMPLE_SERVER_PORT_H
#define DSSH_EXAMPLE_SERVER_PORT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

static inline bool
example_parse_port(const char *text, uint16_t *port)
{
	if (text == NULL || port == NULL || *text == '\0')
		return false;
	uint32_t value = 0;
	for (const char *p = text; *p != '\0'; p++) {
		if (*p < '0' || *p > '9')
			return false;
		uint32_t digit = (uint32_t)(*p - '0');
		if (value > (UINT16_MAX - digit) / 10)
			return false;
		value = value * 10 + digit;
	}
	uint16_t result = (uint16_t)value;
	*port = result;
	return true;
}

#endif
