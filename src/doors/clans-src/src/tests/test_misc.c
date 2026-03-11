/*
 * Unit tests for src/misc.c
 * Uses the "include the C file" pattern -- no changes to existing sources.
 */

/*
 * Pre-include project headers so their include guards fire before the
 * System_Error macro is defined below.
 */
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

#include "../defines.h"
#include "../structs.h"   /* defines struct system (needed for extern below) */
#include "../system.h"    /* declares noreturn System_Error */
#include "../misc.h"

#include "test_harness.h"
#include "mocks_system.h"

#define System_Error(msg) longjmp(g_fatal_jmp, 1)   /* NOLINT */

#include "../misc.c"

#undef System_Error

/* -------------------------------------------------------------------------
 * Tests: atoc  (char range)
 * ------------------------------------------------------------------------- */
static void test_atoc_valid(void)
{
	ASSERT_EQ(atoc("0",    "t", __func__), 0);
	ASSERT_EQ(atoc("127",  "t", __func__), 127);
	ASSERT_EQ(atoc("-128", "t", __func__), (char)-128);
}

static void test_atoc_overflow_high(void)
{
	ASSERT_FATAL(atoc("200", "t", __func__));
}

static void test_atoc_overflow_low(void)
{
	ASSERT_FATAL(atoc("-200", "t", __func__));
}

/* -------------------------------------------------------------------------
 * Tests: ato8  (int8_t)
 * ------------------------------------------------------------------------- */
static void test_ato8_valid(void)
{
	ASSERT_EQ(ato8("0",    "t", __func__), 0);
	ASSERT_EQ(ato8("127",  "t", __func__), 127);
	ASSERT_EQ(ato8("-128", "t", __func__), -128);
}

static void test_ato8_overflow(void)
{
	ASSERT_FATAL(ato8("128",  "t", __func__));
	ASSERT_FATAL(ato8("-129", "t", __func__));
}

/* -------------------------------------------------------------------------
 * Tests: atou8  (uint8_t)
 * ------------------------------------------------------------------------- */
static void test_atou8_valid(void)
{
	ASSERT_EQ(atou8("0",   "t", __func__), 0);
	ASSERT_EQ(atou8("255", "t", __func__), 255);
}

static void test_atou8_overflow(void)
{
	ASSERT_FATAL(atou8("256", "t", __func__));
	ASSERT_FATAL(atou8("-1",  "t", __func__));
}

/* -------------------------------------------------------------------------
 * Tests: ato16  (int16_t)
 * ------------------------------------------------------------------------- */
static void test_ato16_valid(void)
{
	ASSERT_EQ(ato16("0",      "t", __func__), 0);
	ASSERT_EQ(ato16("32767",  "t", __func__), 32767);
	ASSERT_EQ(ato16("-32768", "t", __func__), -32768);
}

static void test_ato16_overflow(void)
{
	ASSERT_FATAL(ato16("32768",  "t", __func__));
	ASSERT_FATAL(ato16("-32769", "t", __func__));
}

/* -------------------------------------------------------------------------
 * Tests: atou16  (uint16_t)
 * ------------------------------------------------------------------------- */
static void test_atou16_valid(void)
{
	ASSERT_EQ(atou16("0",     "t", __func__), 0);
	ASSERT_EQ(atou16("65535", "t", __func__), 65535);
}

static void test_atou16_overflow(void)
{
	ASSERT_FATAL(atou16("65536", "t", __func__));
	ASSERT_FATAL(atou16("-1",    "t", __func__));
}

/* -------------------------------------------------------------------------
 * Tests: ato32  (int32_t)
 * ------------------------------------------------------------------------- */
static void test_ato32_valid(void)
{
	ASSERT_EQ(ato32("0",           "t", __func__),  0);
	ASSERT_EQ(ato32("2147483647",  "t", __func__),  2147483647);
	ASSERT_EQ(ato32("-2147483648", "t", __func__), (int32_t)-2147483648LL);
}

/* -------------------------------------------------------------------------
 * Tests: atoul  (unsigned long) -- errors on unparsable input
 * ------------------------------------------------------------------------- */
static void test_atoul_valid(void)
{
	ASSERT_EQ((long)atoul("0",     "t", __func__), 0L);
	ASSERT_EQ((long)atoul("12345", "t", __func__), 12345L);
}

static void test_atoul_unparsable(void)
{
	ASSERT_FATAL(atoul("xyz", "t", __func__));
}

/* -------------------------------------------------------------------------
 * Tests: atoull  (unsigned long long)
 * ------------------------------------------------------------------------- */
static void test_atoull_valid(void)
{
	ASSERT_EQ((long long)atoull("0",     "t", __func__), 0LL);
	ASSERT_EQ((long long)atoull("99999", "t", __func__), 99999LL);
}

static void test_atoull_unparsable(void)
{
	ASSERT_FATAL(atoull("xyz", "t", __func__));
}

/* -------------------------------------------------------------------------
 * main
 * ------------------------------------------------------------------------- */
int main(void)
{
	RUN(atoc_valid);
	RUN(atoc_overflow_high);
	RUN(atoc_overflow_low);
	RUN(ato8_valid);
	RUN(ato8_overflow);
	RUN(atou8_valid);
	RUN(atou8_overflow);
	RUN(ato16_valid);
	RUN(ato16_overflow);
	RUN(atou16_valid);
	RUN(atou16_overflow);
	RUN(ato32_valid);
	RUN(atoul_valid);
	RUN(atoul_unparsable);
	RUN(atoull_valid);
	RUN(atoull_unparsable);

	printf("\n%d/%d passed\n", g_tests_run - g_tests_failed, g_tests_run);
	return g_tests_failed ? 1 : 0;
}
