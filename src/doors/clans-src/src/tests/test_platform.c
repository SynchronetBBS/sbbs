/*
 * Unit tests for src/platform.c
 * Uses the "include the C file" pattern -- no changes to existing sources.
 *
 * platform.c is self-contained: it includes unix_wrappers.c directly, so
 * this single #include brings in the entire platform layer.
 */

#include <stdio.h>
#include <string.h>

#include "../defines.h"
#include "../structs.h"
#include "../platform.h"

#include "test_harness.h"

#include "../platform.c"

#include "mocks_system.h"

/* -------------------------------------------------------------------------
 * Tests: strlcpy
 * ------------------------------------------------------------------------- */
static void test_strlcpy_basic(void)
{
	char dst[16];
	size_t r = strlcpy(dst, "hello", sizeof(dst));
	ASSERT_EQ((long long)r, 5LL);
	ASSERT_EQ(strcmp(dst, "hello"), 0);
}

static void test_strlcpy_truncates(void)
{
	char dst[4];
	size_t r = strlcpy(dst, "hello", sizeof(dst));
	/* Returns full source length even when truncated; dst is NUL-terminated. */
	ASSERT_EQ((long long)r, 5LL);
	ASSERT_EQ(strcmp(dst, "hel"), 0);
}

static void test_strlcpy_empty_src(void)
{
	char dst[8] = "garbage";
	strlcpy(dst, "", sizeof(dst));
	ASSERT_EQ(dst[0], '\0');
}

/* -------------------------------------------------------------------------
 * Tests: strlcat
 * ------------------------------------------------------------------------- */
static void test_strlcat_basic(void)
{
	char dst[16] = "hello";
	size_t r = strlcat(dst, " world", sizeof(dst));
	ASSERT_EQ((long long)r, 11LL);
	ASSERT_EQ(strcmp(dst, "hello world"), 0);
}

static void test_strlcat_truncates(void)
{
	char dst[8] = "hello";   /* room for 2 more chars + NUL */
	size_t r = strlcat(dst, " world", sizeof(dst));
	/* Returns what the total length would have been. */
	ASSERT_EQ((long long)r, 11LL);
	ASSERT_EQ(strcmp(dst, "hello w"), 0);
}

/* -------------------------------------------------------------------------
 * Tests: CRCValue
 * ------------------------------------------------------------------------- */
static void test_crc_empty(void)
{
	ASSERT_EQ(CRCValue("", 0), 0);
}

static void test_crc_single_byte(void)
{
	const char buf[] = {65};   /* 'A' */
	ASSERT_EQ(CRCValue(buf, 1), 65);
}

static void test_crc_two_bytes(void)
{
	const char buf[] = {65, 66};   /* "AB" */
	ASSERT_EQ(CRCValue(buf, 2), 65 + 66);
}

/* -------------------------------------------------------------------------
 * Tests: isRelative
 * ------------------------------------------------------------------------- */
static void test_isrelative_absolute(void)
{
	ASSERT_EQ(isRelative("/etc/passwd"), false);
}

static void test_isrelative_relative(void)
{
	ASSERT_EQ(isRelative("some/path"), true);
}

static void test_isrelative_bare_filename(void)
{
	ASSERT_EQ(isRelative("file.txt"), true);
}

/* -------------------------------------------------------------------------
 * Tests: Strip
 * ------------------------------------------------------------------------- */
static void test_strip_leading_trailing(void)
{
	char s[] = "   hello   ";
	Strip(s);
	ASSERT_EQ(strcmp(s, "hello"), 0);
}

static void test_strip_all_spaces(void)
{
	char s[] = "   ";
	Strip(s);
	ASSERT_EQ(s[0], '\0');
}

static void test_strip_empty(void)
{
	char s[] = "";
	Strip(s);
	ASSERT_EQ(s[0], '\0');
}

static void test_strip_no_whitespace(void)
{
	char s[] = "word";
	Strip(s);
	ASSERT_EQ(strcmp(s, "word"), 0);
}

/* -------------------------------------------------------------------------
 * Tests: iscodechar
 * ------------------------------------------------------------------------- */
static void test_iscodechar_uppercase_af(void)
{
	ASSERT_EQ(iscodechar('A'), true);
	ASSERT_EQ(iscodechar('F'), true);
}

static void test_iscodechar_digits(void)
{
	ASSERT_EQ(iscodechar('0'), true);
	ASSERT_EQ(iscodechar('9'), true);
}

static void test_iscodechar_invalid(void)
{
	ASSERT_EQ(iscodechar('G'), false);
	ASSERT_EQ(iscodechar('a'), false);
}

/* -------------------------------------------------------------------------
 * Tests: RemovePipes
 * ------------------------------------------------------------------------- */
static void test_removepipes_no_codes(void)
{
	char dst[32];
	RemovePipes("plain text", dst);
	ASSERT_EQ(strcmp(dst, "plain text"), 0);
}

static void test_removepipes_pipe_code(void)
{
	/* "|0A" is a 3-char pipe code: '|', digit, alnum. */
	char dst[32];
	RemovePipes("|0Ahello", dst);
	ASSERT_EQ(strcmp(dst, "hello"), 0);
}

static void test_removepipes_backtick_code(void)
{
	/* "`F5" is a 3-char backtick code: F and 5 are both iscodechar. */
	char dst[32];
	RemovePipes("`F5text", dst);
	ASSERT_EQ(strcmp(dst, "text"), 0);
}

static void test_removepipes_empty(void)
{
	char dst[8];
	RemovePipes("", dst);
	ASSERT_EQ(dst[0], '\0');
}

/* -------------------------------------------------------------------------
 * Tests: DaysBetween / DaysSince1970  (date format: MM/DD/YYYY)
 * ------------------------------------------------------------------------- */
static void test_daysbetween_same_date(void)
{
	char d[] = "01/01/1970";
	ASSERT_EQ(DaysBetween(d, d), 0);
}

static void test_daysbetween_one_day(void)
{
	char d1[] = "01/01/1970";
	char d2[] = "01/02/1970";
	ASSERT_EQ(DaysBetween(d1, d2), 1);
}

static void test_daysbetween_month_boundary(void)
{
	char d1[] = "01/31/1970";
	char d2[] = "02/01/1970";
	ASSERT_EQ(DaysBetween(d1, d2), 1);
}

static void test_dayssince1970_epoch(void)
{
	/* Jan 1 1970 = day 1 in the game's counting (DaysSinceJan1 returns 1). */
	char d[] = "01/01/1970";
	ASSERT_EQ(DaysSince1970(d), 1);
}

/* -------------------------------------------------------------------------
 * Tests: DirExists
 * ------------------------------------------------------------------------- */
static void test_direxists_null(void)
{
	ASSERT_EQ(DirExists(NULL), false);
}

static void test_direxists_tmp(void)
{
	ASSERT_EQ(DirExists("/tmp"), true);
}

static void test_direxists_missing(void)
{
	ASSERT_EQ(DirExists("/nonexistent_xyz_clans_test"), false);
}

static void test_direxists_trailing_slash(void)
{
	/* Trailing slash is stripped before stat(). */
	ASSERT_EQ(DirExists("/tmp/"), true);
}

/* -------------------------------------------------------------------------
 * Tests: SameFile
 * ------------------------------------------------------------------------- */
static void test_samefile_identical(void)
{
	ASSERT_EQ(SameFile("/path/to/file.txt", "/path/to/file.txt"), true);
}

static void test_samefile_different(void)
{
	ASSERT_EQ(SameFile("/path/a.txt", "/path/b.txt"), false);
}

static void test_samefile_case_insensitive(void)
{
	/* Full-path strcasecmp matches regardless of case. */
	ASSERT_EQ(SameFile("/path/to/File.TXT", "/path/to/file.txt"), true);
}

static void test_samefile_relative_vs_absolute(void)
{
	/* One relative → compare basenames only. */
	ASSERT_EQ(SameFile("data.txt", "/some/path/data.txt"), true);
}

/* -------------------------------------------------------------------------
 * main
 * ------------------------------------------------------------------------- */
int main(void)
{
	RUN(strlcpy_basic);
	RUN(strlcpy_truncates);
	RUN(strlcpy_empty_src);
	RUN(strlcat_basic);
	RUN(strlcat_truncates);
	RUN(crc_empty);
	RUN(crc_single_byte);
	RUN(crc_two_bytes);
	RUN(isrelative_absolute);
	RUN(isrelative_relative);
	RUN(isrelative_bare_filename);
	RUN(strip_leading_trailing);
	RUN(strip_all_spaces);
	RUN(strip_empty);
	RUN(strip_no_whitespace);
	RUN(iscodechar_uppercase_af);
	RUN(iscodechar_digits);
	RUN(iscodechar_invalid);
	RUN(removepipes_no_codes);
	RUN(removepipes_pipe_code);
	RUN(removepipes_backtick_code);
	RUN(removepipes_empty);
	RUN(daysbetween_same_date);
	RUN(daysbetween_one_day);
	RUN(daysbetween_month_boundary);
	RUN(dayssince1970_epoch);
	RUN(direxists_null);
	RUN(direxists_tmp);
	RUN(direxists_missing);
	RUN(direxists_trailing_slash);
	RUN(samefile_identical);
	RUN(samefile_different);
	RUN(samefile_case_insensitive);
	RUN(samefile_relative_vs_absolute);

	printf("\n%d/%d passed\n", g_tests_run - g_tests_failed, g_tests_run);
	return g_tests_failed ? 1 : 0;
}
