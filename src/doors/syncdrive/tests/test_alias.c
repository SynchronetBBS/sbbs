/* Unit tests for alias.c. */
#include <assert.h>
#include <string.h>
#include "alias.h"

int main(void)
{
	char out[ALIAS_FIELD + 1];

	alias_sanitize("Digital Man", out);
	assert(strcmp(out, "Digital Man    ") == 0);

	alias_sanitize("A Very Long Alias Name", out);
	assert(strcmp(out, "A Very Long Ali") == 0);

	alias_sanitize("x{y}~z|\x81", out);      /* 0x7B..0x7E and high bytes */
	assert(strcmp(out, "x?y??z??       ") == 0);

	alias_sanitize("", out);
	assert(out[0] == '\0');
	alias_sanitize(NULL, out);
	assert(out[0] == '\0');
	return 0;
}
