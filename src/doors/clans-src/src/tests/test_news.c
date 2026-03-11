/*
 * Unit tests for src/news.c
 * Uses the "include the C file" pattern -- no changes to existing sources.
 */

/*
 * Pre-include standard and project headers.  Their include guards then
 * prevent any conflicts after our stubs are defined below.
 *
 * <OpenDoor.h> is found via -I../odoors/ in the compile command.
 * ../mstrings.h must already be generated (run gmake in src/ first).
 */
#include <ctype.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <OpenDoor.h>
#include "../platform.h"
#include "../system.h"

#include "test_harness.h"

#include "../news.c"

/*
 * Custom rputs with call counter
 */
static int g_rputs_calls;
void rputs(const char *s) { (void)s; g_rputs_calls++; }

/*
 * Other stubs
 */
FILE *_fsopen(const char *pathname, const char *mode, int flags)
{
	(void)flags;
	return fopen(pathname, mode);
}

void door_pause(void) {}
char GetKey(void) { return 0; }
char GetKeyNoWait(void) { return 0; }
bool Door_AllowScreenPause(void) { return false; }
void InputCallback(void) {}

#include "mocks_od.h"
#include "mocks_language.h"
#include "mocks_system.h"

/* -------------------------------------------------------------------------
 * Per-test setup / teardown: run each test in a fresh temp directory.
 * ------------------------------------------------------------------------- */
static char g_origdir[PATH_MAX];
static char g_tmpdir[PATH_MAX];
static char g_bigstring[1024];

static void setup(void)
{
	char template[sizeof("/tmp/test_news_XXXXXX")];

	g_rputs_calls = 0;

	/* Minimal Language mock: ST_NEWSFOR (index 801) and ST_MORE (index 2)
	 * both map to offset 0 in g_bigstring. */
	memset(&g_lang,    0, sizeof(g_lang));
	memset(g_bigstring, 0, sizeof(g_bigstring));
	strcpy(g_bigstring, "News for %s\n");
	g_lang.BigString       = g_bigstring;
	g_lang.StrOffsets[801] = 0;   /* ST_NEWSFOR */
	g_lang.StrOffsets[2]   = 0;   /* ST_MORE (safe placeholder) */

	memset(&System, 0, sizeof(System));
	strcpy(System.szTodaysDate, "2026-03-08");

	if (!getcwd(g_origdir, sizeof(g_origdir))) { perror("getcwd"); exit(1); }
	strcpy(template, "/tmp/test_news_XXXXXX");
	if (!mkdtemp(template)) { perror("mkdtemp"); exit(1); }
	strcpy(g_tmpdir, template);
	if (chdir(g_tmpdir) != 0) { perror("chdir"); exit(1); }
}

static void teardown(void)
{
	(void)unlink("today.asc");
	(void)unlink("yest.asc");
	if (chdir(g_origdir) != 0) { perror("chdir back"); exit(1); }
	if (rmdir(g_tmpdir)  != 0) perror("rmdir");
}

/* -------------------------------------------------------------------------
 * Helper: read an entire file into buf.  Returns bytes read, -1 on error.
 * ------------------------------------------------------------------------- */
static int read_file(const char *path, char *buf, size_t size)
{
	FILE *f = fopen(path, "r");
	if (!f) return -1;
	size_t n = fread(buf, 1, size - 1, f);
	buf[n] = '\0';
	fclose(f);
	return (int)n;
}

/* -------------------------------------------------------------------------
 * Tests: News_AddNews
 * ------------------------------------------------------------------------- */
static void test_add_news_creates_file(void)
{
	char buf[256];
	News_AddNews("hello\n");
	ASSERT_EQ(read_file("today.asc", buf, sizeof(buf)) > 0, 1);
	ASSERT_EQ(strcmp(buf, "hello\n"), 0);
}

static void test_add_news_appends(void)
{
	char buf[256];
	News_AddNews("line1\n");
	News_AddNews("line2\n");
	ASSERT_EQ(read_file("today.asc", buf, sizeof(buf)) > 0, 1);
	ASSERT_EQ(strcmp(buf, "line1\nline2\n"), 0);
}

static void test_add_news_empty_string(void)
{
	char buf[4];
	News_AddNews("");
	/* File is created (opened in append mode) but contains no bytes. */
	ASSERT_EQ(read_file("today.asc", buf, sizeof(buf)), 0);
}

/* -------------------------------------------------------------------------
 * Tests: News_ReadNews
 * ------------------------------------------------------------------------- */
static void test_read_news_today_no_file(void)
{
	/* No today.asc in the temp dir; must emit the "no news" message. */
	News_ReadNews(true);
	ASSERT_EQ(g_rputs_calls, 1);
}

static void test_read_news_yesterday_no_file(void)
{
	/* No yest.asc either; same "no news" path. */
	News_ReadNews(false);
	ASSERT_EQ(g_rputs_calls, 1);
}

/* -------------------------------------------------------------------------
 * Tests: News_CreateTodayNews
 * ------------------------------------------------------------------------- */
static void test_create_today_news_writes_date(void)
{
	char buf[256];
	News_CreateTodayNews();
	ASSERT_EQ(read_file("today.asc", buf, sizeof(buf)) > 0, 1);
	/* The formatted header must contain today's date. */
	ASSERT_EQ(strstr(buf, System.szTodaysDate) != NULL, 1);
}

static void test_create_today_news_uses_add_news(void)
{
	/* News_CreateTodayNews is layered on News_AddNews; calling it twice
	 * must accumulate two header lines in today.asc. */
	char buf[256];
	News_CreateTodayNews();
	News_CreateTodayNews();
	int n = read_file("today.asc", buf, sizeof(buf));
	ASSERT_EQ(n > 0, 1);
	/* Two copies of the date string must appear. */
	char *first = strstr(buf, System.szTodaysDate);
	ASSERT_EQ(first != NULL, 1);
	char *second = strstr(first + 1, System.szTodaysDate);
	ASSERT_EQ(second != NULL, 1);
}

/* -------------------------------------------------------------------------
 * main
 * ------------------------------------------------------------------------- */
int main(void)
{
	g_test_setup    = setup;
	g_test_teardown = teardown;

	RUN(add_news_creates_file);
	RUN(add_news_appends);
	RUN(add_news_empty_string);
	RUN(read_news_today_no_file);
	RUN(read_news_yesterday_no_file);
	RUN(create_today_news_writes_date);
	RUN(create_today_news_uses_add_news);

	printf("\n%d/%d passed\n", g_tests_run - g_tests_failed, g_tests_run);
	return g_tests_failed ? 1 : 0;
}
