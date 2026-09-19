/* alias.c -- see alias.h. The game's name editor accepts 0x20..0x7A. */
#include <stddef.h>
#include "alias.h"

void alias_sanitize(const char *in, char out[ALIAS_FIELD + 1])
{
	int i = 0;

	out[0] = '\0';
	if (in == NULL || *in == '\0')
		return;
	for (; i < ALIAS_FIELD && in[i] != '\0'; i++) {
		unsigned char c = (unsigned char)in[i];

		out[i] = (c >= 0x20 && c <= 0x7A) ? (char)c : '?';
	}
	for (; i < ALIAS_FIELD; i++)
		out[i] = ' ';
	out[ALIAS_FIELD] = '\0';
}
