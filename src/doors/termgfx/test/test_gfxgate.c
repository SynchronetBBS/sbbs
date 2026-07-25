/* Unit test for gfxgate.c -- the shared no-graphics verdict and the notice
 * the sysop can override.
 *
 * termgfx_gfxgate_notice() resolves ONCE per process and caches, so a single
 * binary can only ever exercise one of its three tiers. GFXGATE_CASE picks
 * which; the CMake side builds the same source three times. The verdict half
 * is stateless and runs in every case. */

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gfxgate.h"

#ifndef GFXGATE_CASE
#define GFXGATE_CASE  0
#endif

#define NOTICE_FILE  "gfxgate_notice_test.txt"

static void verdicts(void)
{
	const uint32_t t0 = 100000;   /* arbitrary probe-send stamp */

	/* A terminal that never answered device-attributes draws: silence is not
	 * a denial, and a graphics terminal that doesn't implement DA1 must not
	 * be locked out. True however long ago the probe went out. */
	assert(termgfx_gfxgate(0, 0, 0, 0, t0, t0) == TERMGFX_GFXGATE_PROCEED);
	assert(termgfx_gfxgate(0, 0, 0, 0, t0, t0 + 60000) == TERMGFX_GFXGATE_PROCEED);

	/* Either tier is enough, settled or not. */
	assert(termgfx_gfxgate(1, 0, 1, 0, t0, t0) == TERMGFX_GFXGATE_PROCEED);
	assert(termgfx_gfxgate(0, 1, 1, 0, t0, t0) == TERMGFX_GFXGATE_PROCEED);
	assert(termgfx_gfxgate(1, 1, 1, 1, t0, t0 + 60000) == TERMGFX_GFXGATE_PROCEED);

	/* Answered, claimed neither, JXL query still outstanding: hold. This is
	 * the JXL-but-no-sixel SyncTERM that would otherwise be turned away in
	 * the gap between its two replies. */
	assert(termgfx_gfxgate(0, 0, 1, 0, t0, t0) == TERMGFX_GFXGATE_WAIT);
	assert(termgfx_gfxgate(0, 0, 1, 0, t0, t0 + TERMGFX_GFX_SETTLE_MS)
	       == TERMGFX_GFXGATE_WAIT);

	/* One millisecond past the window, or a JXL reply that came back
	 * negative: reject. */
	assert(termgfx_gfxgate(0, 0, 1, 0, t0, t0 + TERMGFX_GFX_SETTLE_MS + 1)
	       == TERMGFX_GFXGATE_REJECT);
	assert(termgfx_gfxgate(0, 0, 1, 1, t0, t0) == TERMGFX_GFXGATE_REJECT);

	/* The elapsed test is signed-difference, so it survives a millisecond
	 * counter wrapping between the probe and the check. */
	assert(termgfx_gfxgate(0, 0, 1, 0, 0xfffffff0u, 0x00000010u)
	       == TERMGFX_GFXGATE_WAIT);
	assert(termgfx_gfxgate(0, 0, 1, 0, 0xfffffff0u, 0x00000010u + TERMGFX_GFX_SETTLE_MS)
	       == TERMGFX_GFXGATE_REJECT);
}

int main(void)
{
	const char *n;

	verdicts();

#if GFXGATE_CASE == 0
	/* Built-in: neither override supplied. Both an absent path and an empty
	 * string fall through to it. */
	n = termgfx_gfxgate_notice(NULL, NULL);
	assert(strstr(n, "sixel or JXL graphics support") != NULL);
	assert(strncmp(n, "\r\n\x1b[0m\r\n", 8) == 0);
	/* Cached: a later call with a real override still returns the first. */
	assert(termgfx_gfxgate_notice("/nonexistent", "ignored") == n);

#elif GFXGATE_CASE == 1
	/* A one-line [text] replacement, shown with the built-in's framing. An
	 * unreadable file path falls through to it rather than masking it. */
	n = termgfx_gfxgate_notice("/nonexistent/gfxgate", "Sorry, graphics only.");
	assert(strcmp(n, "\r\n\x1b[0m\r\n  Sorry, graphics only.\r\n\r\n") == 0);

#elif GFXGATE_CASE == 2
	{   /* A file wins over the [text] key, keeps its own layout, and gets
		 * bare LFs rewritten without doubling an existing CRLF. The last
		 * line deliberately has no newline of its own. */
		FILE *f = fopen(NOTICE_FILE, "wb");

		assert(f != NULL);
		fputs("Line one\nLine two\r\nLine three", f);
		fclose(f);

		n = termgfx_gfxgate_notice(NOTICE_FILE, "not this one");
		remove(NOTICE_FILE);

		assert(strstr(n, "Line one\r\nLine two\r\nLine three") != NULL);
		assert(strstr(n, "\r\r\n") == NULL);          /* no doubled CR */
		assert(strstr(n, "not this one") == NULL);    /* file beat the key */
		/* Bracketed by an SGR reset so a half-attributed .ans cannot leak
		 * into the BBS prompt the player lands back on. */
		assert(strncmp(n, "\r\n\x1b[0m\r\n", 8) == 0);
		assert(strcmp(n + strlen(n) - 6, "\x1b[0m\r\n") == 0);
	}
#endif

	return 0;
}
