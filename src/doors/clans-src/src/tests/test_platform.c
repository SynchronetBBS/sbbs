/*
 * Unit tests for src/platform.c
 * Uses the "include the C file" pattern -- no changes to existing sources.
 *
 * platform.c is self-contained: it includes unix_wrappers.c directly, so
 * this single #include brings in the entire platform layer.
 */

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

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
 * Tests: IsLeapYear
 * ------------------------------------------------------------------------- */
static void test_isleapyear_not_div4(void)
{
	/* 2023 is not divisible by 4 → false */
	ASSERT_EQ(IsLeapYear(2023), false);
}

static void test_isleapyear_div4_not_100(void)
{
	/* 2024 is divisible by 4 but not 100 → true */
	ASSERT_EQ(IsLeapYear(2024), true);
}

static void test_isleapyear_div100_not_400(void)
{
	/* 1900 is divisible by 100 but not 400 → false */
	ASSERT_EQ(IsLeapYear(1900), false);
}

static void test_isleapyear_div400(void)
{
	/* 2000 is divisible by 400 → true */
	ASSERT_EQ(IsLeapYear(2000), true);
}

/* -------------------------------------------------------------------------
 * Tests: DaysSinceJan1
 * ------------------------------------------------------------------------- */
static void test_dayssincejana1_jan1(void)
{
	char d[] = "01/01/2023";
	ASSERT_EQ(DaysSinceJan1(d), 1);
}

static void test_dayssincejana1_feb1(void)
{
	char d[] = "02/01/2023";
	/* Jan has 31 days, so Feb 1 is day 32 */
	ASSERT_EQ(DaysSinceJan1(d), 32);
}

static void test_dayssincejana1_mar1_nonleap(void)
{
	char d[] = "03/01/2023";
	/* Jan 31 + Feb 28 = 59 days, Mar 1 is day 60 */
	ASSERT_EQ(DaysSinceJan1(d), 60);
}

static void test_dayssincejana1_mar1_leap(void)
{
	char d[] = "03/01/2024";
	/* 2024 is a leap year, so Jan 31 + Feb 29 = 60 days, Mar 1 is day 61 */
	ASSERT_EQ(DaysSinceJan1(d), 61);
}

static void test_dayssincejana1_dec31_nonleap(void)
{
	char d[] = "12/31/2023";
	/* 365 days in non-leap year */
	ASSERT_EQ(DaysSinceJan1(d), 365);
}

/* -------------------------------------------------------------------------
 * Tests: FileName
 * ------------------------------------------------------------------------- */
static void test_filename_with_slash(void)
{
	const char *r = FileName("/foo/bar");
	ASSERT_EQ(strcmp(r, "bar"), 0);
}

static void test_filename_no_slash(void)
{
	const char *r = FileName("file.c");
	ASSERT_EQ(strcmp(r, "file.c"), 0);
}

static void test_filename_multiple_slashes(void)
{
	const char *r = FileName("/foo/bar/baz");
	ASSERT_EQ(strcmp(r, "baz"), 0);
}

/* -------------------------------------------------------------------------
 * Tests: FreeFileList
 * Note: FreeFileList is accessed via platform.c -> unix_wrappers.c
 * These tests manually allocate and free to verify correct behavior.
 * We cannot test the internal details (it calls free on each entry),
 * but we can verify it doesn't crash and basic operations succeed.
 * For true coverage, this would need integration tests with FilesOrderedByDate.
 * We test the skeleton: single entry and empty list.
 * ------------------------------------------------------------------------- */
static void test_freefilelist_single_entry(void)
{
	/* Allocate: array of 2 pointers (entry + NULL) */
	char **fl = (char **)malloc(2 * sizeof(char *));
	fl[0] = (char *)malloc(8);
	strcpy(fl[0], "file.txt");
	fl[1] = NULL;
	/* Call FreeFileList -- it should free fl[0] then fl */
	FreeFileList(fl);
	/* Test passes if no crash; we cannot verify the free() call. */
}

static void test_freefilelist_empty(void)
{
	/* Allocate: just a NULL-sentinel */
	char **fl = (char **)malloc(sizeof(char *));
	fl[0] = NULL;
	/* Call FreeFileList -- it should just free the array */
	FreeFileList(fl);
	/* Test passes if no crash. */
}

/* -------------------------------------------------------------------------
 * Tests: fullpath
 * Tests include absolute paths, relative paths, and normalization.
 * For relative tests, we use mkdtemp + chdir pattern.
 * Note: These tests modify the current directory temporarily.
 * The test harness does not provide setup/teardown, so we do it inline.
 * ------------------------------------------------------------------------- */
static void test_fullpath_absolute_passthrough(void)
{
	char buf[256];
	char *r = fullpath(buf, "/foo/bar", sizeof(buf));
	ASSERT_EQ(strcmp(buf, "/foo/bar"), 0);
	ASSERT_EQ(r != NULL, true);
}

static void test_fullpath_collapse_double_slash(void)
{
	char buf[256];
	char *r = fullpath(buf, "/foo//bar", sizeof(buf));
	ASSERT_EQ(strcmp(buf, "/foo/bar"), 0);
	ASSERT_EQ(r != NULL, true);
}

static void test_fullpath_remove_dotslash(void)
{
	char buf[256];
	char *r = fullpath(buf, "/foo/./bar", sizeof(buf));
	ASSERT_EQ(strcmp(buf, "/foo/bar"), 0);
	ASSERT_EQ(r != NULL, true);
}

static void test_fullpath_resolve_dotdot(void)
{
	char buf[256];
	char *r = fullpath(buf, "/foo/bar/../baz", sizeof(buf));
	ASSERT_EQ(strcmp(buf, "/foo/baz"), 0);
	ASSERT_EQ(r != NULL, true);
}

static void test_fullpath_relative(void)
{
	/* Create a temp directory, chdir to it, test relative path resolution */
	char tmpdir[] = "/tmp/test_fullpath_XXXXXX";
	char orig_cwd[256];
	char buf[PATH_MAX];
	char *r;

	if (getcwd(orig_cwd, sizeof(orig_cwd)) == NULL) {
		return;  /* Cannot proceed without saving orig_cwd */
	}

	if (mkdtemp(tmpdir) == NULL) {
		return;  /* Cannot create temp directory */
	}

	if (chdir(tmpdir) != 0) {
		rmdir(tmpdir);
		return;  /* Cannot chdir */
	}

	/* Now resolve a relative path */
	r = fullpath(buf, "subdir/../file.txt", sizeof(buf));

	/* Verify it resolved to an absolute path */
	ASSERT_EQ(r != NULL, true);
	ASSERT_EQ(buf[0], '/');  /* Should be absolute */

	/* Cleanup */
	chdir(orig_cwd);
	rmdir(tmpdir);
}

static void test_fullpath_null_target(void)
{
	/* Pass NULL target; fullpath should allocate PATH_MAX+1 bytes */
	char *r = fullpath(NULL, "/foo/bar", PATH_MAX + 1);
	if (r != NULL) {
		ASSERT_EQ(strcmp(r, "/foo/bar"), 0);
		free(r);
	}
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
	RUN(isleapyear_not_div4);
	RUN(isleapyear_div4_not_100);
	RUN(isleapyear_div100_not_400);
	RUN(isleapyear_div400);
	RUN(dayssincejana1_jan1);
	RUN(dayssincejana1_feb1);
	RUN(dayssincejana1_mar1_nonleap);
	RUN(dayssincejana1_mar1_leap);
	RUN(dayssincejana1_dec31_nonleap);
	RUN(filename_with_slash);
	RUN(filename_no_slash);
	RUN(filename_multiple_slashes);
	RUN(freefilelist_single_entry);
	RUN(freefilelist_empty);
	RUN(fullpath_absolute_passthrough);
	RUN(fullpath_collapse_double_slash);
	RUN(fullpath_remove_dotslash);
	RUN(fullpath_resolve_dotdot);
	RUN(fullpath_relative);
	RUN(fullpath_null_target);

	printf("\n%d/%d passed\n", g_tests_run - g_tests_failed, g_tests_run);
	return g_tests_failed ? 1 : 0;
}
