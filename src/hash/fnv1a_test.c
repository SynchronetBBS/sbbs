/* Unit test for fnv1a.c -- build and run with: gmake fnv1a_test && ./fnv1a_test
 *
 * The published vectors matter more here than in a typical unit test. FNV-1a's
 * jobs in this tree -- content-addressed cache names, change-detection
 * signatures -- are all PROMISES that identical bytes yield an identical value,
 * across sessions, across hosts and across programs. Drift a constant and every
 * cache silently misses; nothing errors, it just quietly stops working. So pin
 * the canonical values rather than whatever the code happens to produce.
 */

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "fnv1a.h"

int main(void)
{
	static const char *const words[] = { "", "a", "foobar", "d_deadbeef", "MENU.MID" };
	static const char        buf[]   = "the quick brown fox";
	const size_t             len     = sizeof buf - 1;
	uint32_t                 h32;
	uint64_t                 h64;
	size_t                   n, i;

	/* Canonical FNV-1a vectors (http://www.isthe.com/chongo/tech/comp/fnv/). */
	assert(fnv1a32("", 0)       == 0x811c9dc5u);
	assert(fnv1a32("a", 1)      == 0xe40c292cu);
	assert(fnv1a32("foobar", 6) == 0xbf9cf968u);

	assert(fnv1a64("", 0)       == 0xcbf29ce484222325ull);
	assert(fnv1a64("a", 1)      == 0xaf63dc4c8601ec8cull);
	assert(fnv1a64("foobar", 6) == 0x85944171f73967e8ull);

	/* The exported bases ARE the empty-input hashes -- an _update() chain that
	 * starts there is the same function as the one-shot form. */
	assert(FNV1A32_INIT == fnv1a32("", 0));
	assert(FNV1A64_INIT == fnv1a64("", 0));

	/* The two widths are independent functions, not one truncated to the other:
	 * different bases, different primes. Guards against someone "simplifying"
	 * fnv1a32() into a cast of fnv1a64(), which would break every stored name. */
	assert((uint32_t)fnv1a64("foobar", 6) != fnv1a32("foobar", 6));

	/* Binary-safe: an embedded NUL is data, not a terminator. Callers hash MIDI
	 * lumps and PCM full of them, so a NUL-stopping hash would collapse
	 * unrelated content onto one name. */
	assert(fnv1a32("a\0b", 3) != fnv1a32("a\0c", 3));
	assert(fnv1a32("a\0b", 3) != fnv1a32("a", 1));
	assert(fnv1a64("a\0b", 3) != fnv1a64("a\0c", 3));

	/* _str() is the NUL-terminated spelling of the same hash: it must agree with
	 * strlen()-ing it by hand, or switching between the two would silently
	 * change a cache name. */
	for (i = 0; i < sizeof words / sizeof *words; i++) {
		assert(fnv1a32_str(words[i]) == fnv1a32(words[i], strlen(words[i])));
		assert(fnv1a64_str(words[i]) == fnv1a64(words[i], strlen(words[i])));
	}

	/* ...and it stops at the terminator rather than hashing it. */
	assert(fnv1a32_str("ab") == fnv1a32("ab\0cd", 2));
	assert(fnv1a64_str("ab") == fnv1a64("ab\0cd", 2));

	/* Chaining is exact: ANY split of a buffer hashes to the whole-buffer value.
	 * That is what lets a caller fold in a count, then a field, then a string
	 * without marshalling them into one scratch buffer first. */
	for (n = 0; n <= len; n++) {
		h32 = fnv1a32_update(FNV1A32_INIT, buf, n);
		h32 = fnv1a32_update(h32, buf + n, len - n);
		assert(h32 == fnv1a32(buf, len));

		h64 = fnv1a64_update(FNV1A64_INIT, buf, n);
		h64 = fnv1a64_update(h64, buf + n, len - n);
		assert(h64 == fnv1a64(buf, len));
	}

	/* A zero-length fold is the identity -- an empty piece must not perturb a
	 * running hash. */
	assert(fnv1a32_update(0x12345678u, "", 0) == 0x12345678u);
	assert(fnv1a64_update(0x123456789abcdefull, "", 0) == 0x123456789abcdefull);

	printf("fnv1a: all tests passed\n");
	return 0;
}
