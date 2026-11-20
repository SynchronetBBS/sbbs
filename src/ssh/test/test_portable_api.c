/* Public-header-only controls; no private header, backend or library link.
 * The span oracle uses checked addition, independent of the subtraction helper. */
#include <stdint.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "deucessh-portable.h"

static bool
span_answer(size_t length, size_t offset, size_t count)
{
	uintmax_t limit = (uintmax_t)length;
	uintmax_t start = (uintmax_t)offset;
	uintmax_t extent = (uintmax_t)count;
	if (extent > UINTMAX_MAX - start)
		return false;
	return start + extent <= limit;
}

int
main(void)
{
	uint64_t spans = 0;
	for (size_t length = 0; length < 256; length++) {
		for (size_t offset = 0; offset < 256; offset++) {
			for (size_t count = 0; count < 256; count++) {
				if (dssh_buffer_has(length, offset, count)
				    != span_answer(length, offset, count))
					return 1;
				/* Exactly 2^24 tuples; this counter cannot overflow. */
				spans++;
			}
		}
	}
	const size_t edges[] = {0, 1, 2, 255, 256, SIZE_MAX / 2, SIZE_MAX - 1, SIZE_MAX};
	for (size_t a = 0; a < sizeof(edges) / sizeof(edges[0]); a++) {
		for (size_t b = 0; b < sizeof(edges) / sizeof(edges[0]); b++) {
			for (size_t c = 0; c < sizeof(edges) / sizeof(edges[0]); c++) {
				if (dssh_buffer_has(edges[a], edges[b], edges[c])
				    != span_answer(edges[a], edges[b], edges[c]))
					return 1;
				spans++;
			}
		}
	}
	unsigned wipes = 0;
	for (size_t offset = 0; offset <= 64; offset++) {
		for (size_t length = 0; length <= 64 - offset; length++) {
			unsigned char frame[66];
			unsigned char expected[66];
			memset(frame, 0x93, sizeof(frame));
			memset(expected, 0x93, sizeof(expected));
			memset(expected + 1 + offset, 0, length);
			dssh_secure_zero(frame + 1 + offset, length);
			if (memcmp(frame, expected, sizeof(frame)) != 0)
				return 1;
			wipes++;
		}
	}
	/* Observe the existing empty-span behavior without dereferencing NULL. */
	dssh_secure_zero(NULL, 0);
	int printed = printf("portable_api spans=%" PRIu64 " wipes=%u guards=1 null_zero=1 valid=1\n",
	    spans, wipes);
	if (printed < 0 || fflush(stdout) == EOF)
		return 1;
	return spans == UINT64_C(16777728) && wipes == 2145 ? 0 : 1;
}
