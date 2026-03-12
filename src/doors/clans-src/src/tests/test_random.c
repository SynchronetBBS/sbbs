/*
 * Unit tests for src/random.c
 * Uses the "include the C file" pattern -- no changes to existing sources.
 */

/*
 * Pre-include every header that random.c uses.  Their include guards then
 * prevent redeclaration of System_Error() after we redefine it as a macro
 * below.
 */
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "../defines.h"
#include "../video.h"
#include "../console.h"

#include "test_harness.h"

/*
 * System_Error() mock (Strategy B: macro interception)
 *
 * random.c calls System_Error() for invalid arguments.  We longjmp back to
 * the test site instead of terminating.  The jmp_buf is re-armed by each
 * ASSERT_FATAL.
 */
static jmp_buf g_fatal_jmp;

#define System_Error(msg) longjmp(g_fatal_jmp, 1)   /* NOLINT */

#include "../random.c"

#undef System_Error

#include "mocks_video.h"

/* -------------------------------------------------------------------------
 * Tests: invalid arguments trigger System_Error
 *
 * Note: my_random() computes (unsigned)(limit - 1) before the guard check,
 * so only values where limit - 1 is defined (i.e. limit > INT_MIN) are
 * safe to pass.
 * ------------------------------------------------------------------------- */
static void test_zero_limit(void)
{
	ASSERT_FATAL(my_random(0));
}

static void test_negative_limit(void)
{
	ASSERT_FATAL(my_random(-1));
	ASSERT_FATAL(my_random(-100));
	ASSERT_FATAL(my_random(INT_MIN));  /* previously UB before guard; now safe */
}

static void test_rand_max_limit(void)
{
	/* Limit must be strictly less than RAND_MAX. */
	ASSERT_FATAL(my_random(RAND_MAX));
}

/* -------------------------------------------------------------------------
 * Tests: valid boundary cases
 * ------------------------------------------------------------------------- */
static void test_limit_one(void)
{
	/* Only one possible outcome. */
	ASSERT_EQ(my_random(1), 0);
	ASSERT_EQ(my_random(1), 0);
	ASSERT_EQ(my_random(1), 0);
}

/* -------------------------------------------------------------------------
 * Tests: range and full-coverage for various limits
 *
 * Each test seeds rand() for reproducibility, runs enough trials to make
 * the probability of missing any bucket negligible, and asserts that:
 *   1. every returned value is in [0, limit-1], and
 *   2. every value in that range is seen at least once.
 * ------------------------------------------------------------------------- */

/* Power-of-two limit: exercises the common aligned-mask path. */
static void test_range_power_of_two(void)
{
	const int limit   = 4;
	const int trials  = 400;
	bool      seen[4] = {false};
	srand(1);
	for (int i = 0; i < trials; i++) {
		int v = my_random(limit);
		ASSERT_EQ(v >= 0, 1);
		ASSERT_EQ(v < limit, 1);
		seen[v] = true;
	}
	for (int i = 0; i < limit; i++)
		ASSERT_EQ((int)seen[i], 1);
}

/* Non-power-of-two limit: exercises the rejection-sampling loop. */
static void test_range_non_power_of_two(void)
{
	const int limit   = 6;
	const int trials  = 600;
	bool      seen[6] = {false};
	srand(1);
	for (int i = 0; i < trials; i++) {
		int v = my_random(limit);
		ASSERT_EQ(v >= 0, 1);
		ASSERT_EQ(v < limit, 1);
		seen[v] = true;
	}
	for (int i = 0; i < limit; i++)
		ASSERT_EQ((int)seen[i], 1);
}

/* Larger limit: stress the mask/shift computation further. */
static void test_range_large(void)
{
	const int limit     = 100;
	const int trials    = 10000;
	bool      seen[100] = {false};
	srand(1);
	for (int i = 0; i < trials; i++) {
		int v = my_random(limit);
		ASSERT_EQ(v >= 0, 1);
		ASSERT_EQ(v < limit, 1);
		seen[v] = true;
	}
	for (int i = 0; i < limit; i++)
		ASSERT_EQ((int)seen[i], 1);
}

/* -------------------------------------------------------------------------
 * main
 * ------------------------------------------------------------------------- */
int main(void)
{
	RUN(zero_limit);
	RUN(negative_limit);
	RUN(rand_max_limit);
	RUN(limit_one);
	RUN(range_power_of_two);
	RUN(range_non_power_of_two);
	RUN(range_large);

	printf("\n%d/%d passed\n", g_tests_run - g_tests_failed, g_tests_run);
	return g_tests_failed ? 1 : 0;
}
