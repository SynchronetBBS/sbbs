/*
 * Unit tests for src/tools.c
 * Uses the "include the C file" pattern -- no changes to existing sources.
 */

/*
 * Pre-include every header that tools.c uses.  Their include guards then
 * prevent redeclaration of exit() after we redefine it as a macro below.
 */
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

#include "test_harness.h"

/* -------------------------------------------------------------------------
 * exit() mock
 *
 * When tools.c calls exit(), we longjmp back to the test site instead of
 * terminating the process.  The jmp_buf is re-armed by each ASSERT_FATAL.
 * ------------------------------------------------------------------------- */
static jmp_buf g_fatal_jmp;

#define exit(code) longjmp(g_fatal_jmp, 1)   /* NOLINT */

#include "../tools.c"

#undef exit

/* -------------------------------------------------------------------------
 * Tests: ato8
 * ------------------------------------------------------------------------- */
static void test_ato8_valid(void)
{
	ASSERT_EQ(ato8("0",    "t"),   0);
	ASSERT_EQ(ato8("127",  "t"),  127);
	ASSERT_EQ(ato8("-128", "t"), INT8_MIN);
	ASSERT_EQ(ato8("42",   "t"),  42);
	ASSERT_EQ(ato8("-1",   "t"),  -1);
}

static void test_ato8_overflow(void)
{
	ASSERT_FATAL(ato8("128",  "t"));
	ASSERT_FATAL(ato8("-129", "t"));
	ASSERT_FATAL(ato8("9999", "t"));
}

/* -------------------------------------------------------------------------
 * Tests: atoc
 * ------------------------------------------------------------------------- */
static void test_atoc_valid(void)
{
	ASSERT_EQ(atoc("0",  "t"),  0);
	ASSERT_EQ(atoc("65", "t"), 65);   /* 'A' in ASCII */
}

static void test_atoc_overflow(void)
{
#if CHAR_MIN < 0   /* signed char */
	ASSERT_FATAL(atoc("128",  "t"));
	ASSERT_FATAL(atoc("-129", "t"));
#else              /* unsigned char */
	ASSERT_FATAL(atoc("256",  "t"));
	ASSERT_FATAL(atoc("-1",   "t"));
#endif
}

/* -------------------------------------------------------------------------
 * Tests: ato16
 * ------------------------------------------------------------------------- */
static void test_ato16_valid(void)
{
	ASSERT_EQ(ato16("0",      "t"),       0);
	ASSERT_EQ(ato16("32767",  "t"),   32767);
	ASSERT_EQ(ato16("-32768", "t"), INT16_MIN);
	ASSERT_EQ(ato16("100",    "t"),     100);
}

static void test_ato16_overflow(void)
{
	ASSERT_FATAL(ato16("32768",  "t"));
	ASSERT_FATAL(ato16("-32769", "t"));
}

/* -------------------------------------------------------------------------
 * Tests: ato32
 * ------------------------------------------------------------------------- */
static void test_ato32_valid(void)
{
	ASSERT_EQ(ato32("0",           "t"),          0);
	ASSERT_EQ(ato32("2147483647",  "t"), INT32_MAX);
	ASSERT_EQ(ato32("-2147483648", "t"), INT32_MIN);
	ASSERT_EQ(ato32("1000",        "t"),       1000);
}

/*
 * ato32 uses atol() to parse; out-of-int32 values are only representable
 * (and thus detectable) when long is wider than 32 bits.  On ILP32
 * platforms the inputs below would cause UB in atol(), so skip the test.
 */
#if LONG_MAX > INT32_MAX
static void test_ato32_overflow(void)
{
	ASSERT_FATAL(ato32("2147483648",  "t"));
	ASSERT_FATAL(ato32("-2147483649", "t"));
}
#endif

/* -------------------------------------------------------------------------
 * main
 * ------------------------------------------------------------------------- */
int main(void)
{
	RUN(ato8_valid);
	RUN(ato8_overflow);
	RUN(atoc_valid);
	RUN(atoc_overflow);
	RUN(ato16_valid);
	RUN(ato16_overflow);
	RUN(ato32_valid);
#if LONG_MAX > INT32_MAX
	RUN(ato32_overflow);
#endif

	printf("\n%d/%d passed\n", g_tests_run - g_tests_failed, g_tests_run);
	return g_tests_failed ? 1 : 0;
}
