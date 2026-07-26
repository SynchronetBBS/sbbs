/* test_sixel_palette_used.c -- SIXEL_PAL_USED emits only the registers an
 * image references, at their original index; FULL emits all 256; NONE emits
 * none. Compiles sixel.c directly (no other termgfx deps). */
#include "sixel.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Count occurrences of "#<reg>;2;" register-definition markers in a buffer. */
static int count_defs(const uint8_t *b, size_t n)
{
	int    defs = 0;
	size_t i;

	for (i = 0; i + 2 < n; i++)
		if (b[i] == '#' && (b[i + 1] >= '0' && b[i + 1] <= '9')) {
			size_t j = i + 1;

			while (j < n && b[j] >= '0' && b[j] <= '9')
				j++;
			if (j + 1 < n && b[j] == ';' && b[j + 1] == '2')
				defs++;
		}
	return defs;
}

/* Is "#<reg>;2;" present for this exact register number? */
static int has_reg(const uint8_t *b, size_t n, int reg)
{
	char pat[16];
	int  pn = snprintf(pat, sizeof pat, "#%d;2;", reg);
	size_t i;

	for (i = 0; i + (size_t)pn <= n; i++)
		if (memcmp(b + i, pat, (size_t)pn) == 0)
			return 1;
	return 0;
}

int main(void)
{
	/* 4x2 image using indices 0, 5, 200 only. */
	uint8_t idx[8] = { 0, 5, 200, 5, 200, 0, 5, 5 };
	uint8_t pal[768];
	uint8_t *buf = NULL;
	size_t   cap = 0, n;
	int      i;

	for (i = 0; i < 768; i++)
		pal[i] = (uint8_t)(i * 7);   /* arbitrary distinct-ish colors */

	/* USED: exactly registers 0, 5, 200 defined -- three, no more. */
	n = sixel_encode(&buf, &cap, idx, 4, 2, pal, SIXEL_PAL_USED);
	assert(count_defs(buf, n) == 3);
	assert(has_reg(buf, n, 0));
	assert(has_reg(buf, n, 5));
	assert(has_reg(buf, n, 200));
	assert(!has_reg(buf, n, 1));
	assert(!has_reg(buf, n, 6));

	/* FULL: all 256 registers defined. */
	n = sixel_encode(&buf, &cap, idx, 4, 2, pal, SIXEL_PAL_FULL);
	assert(count_defs(buf, n) == 256);

	/* NONE: no register definitions at all. */
	n = sixel_encode(&buf, &cap, idx, 4, 2, pal, SIXEL_PAL_NONE);
	assert(count_defs(buf, n) == 0);

	/* DELTA: exactly the flagged registers, whatever the image references.
	 * The point is that the two sets are DIFFERENT -- register 7 is not in the
	 * image at all and must still be defined, and register 200 is all over it
	 * and must not be, because its color did not move. */
	{
		uint8_t changed[256];

		memset(changed, 0, sizeof changed);
		changed[5] = changed[7] = changed[9] = 1;

		n = sixel_encode_delta(&buf, &cap, idx, 4, 2, pal, changed);
		assert(count_defs(buf, n) == 3);
		assert(has_reg(buf, n, 5));
		assert(has_reg(buf, n, 7));
		assert(has_reg(buf, n, 9));
		assert(!has_reg(buf, n, 0));
		assert(!has_reg(buf, n, 200));

		/* An empty mask degrades to NONE rather than to FULL: a frame whose
		 * palette did not move must not pay for one. */
		memset(changed, 0, sizeof changed);
		n = sixel_encode_delta(&buf, &cap, idx, 4, 2, pal, changed);
		assert(count_defs(buf, n) == 0);
	}

	free(buf);
	printf("test_sixel_palette_used: PASS\n");
	return 0;
}
