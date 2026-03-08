/*
 * Unit tests for src/semfile.c
 * Uses the "include the C file" pattern -- no changes to existing sources.
 */

/*
 * Pre-include every header that semfile.c uses.  Their include guards then
 * prevent redeclaration after our mocks are defined below.
 */
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "test_harness.h"

/* -------------------------------------------------------------------------
 * sleep() mock
 *
 * WaitSemaphor calls sleep(1) while spinning.  We replace it with a hook
 * so tests can simulate "another process released the lock" without
 * actually blocking.
 * ------------------------------------------------------------------------- */
static void (*g_sleep_hook)(void) = NULL;

#define sleep(x) (g_sleep_hook ? (g_sleep_hook(), 0) : 0)   /* NOLINT */

#include "../semfile.c"

#undef sleep

/* -------------------------------------------------------------------------
 * Stubs for external symbols referenced by semfile.c
 * ------------------------------------------------------------------------- */

/* LogDisplayStr is declared in door.h and called from WaitSemaphor. */
static int g_log_calls = 0;
void LogDisplayStr(const char *s) { (void)s; g_log_calls++; }

/* door.h declares "extern struct Door Door;" -- provide the storage. */
struct Door Door;

/* -------------------------------------------------------------------------
 * Per-test setup / teardown: run each test inside a fresh temp directory.
 * ------------------------------------------------------------------------- */
static char g_origdir[PATH_MAX];
static char g_tmpdir[PATH_MAX];

static void setup(void)
{
	char template[sizeof("/tmp/test_semfile_XXXXXX")];

	g_log_calls  = 0;
	g_sleep_hook = NULL;
	if (!getcwd(g_origdir, sizeof(g_origdir))) {
		perror("getcwd");
		exit(1);
	}
	strcpy(template, "/tmp/test_semfile_XXXXXX");
	if (!mkdtemp(template)) {
		perror("mkdtemp");
		exit(1);
	}
	strcpy(g_tmpdir, template);
	if (chdir(g_tmpdir) != 0) {
		perror("chdir into tmpdir");
		exit(1);
	}
}

static void teardown(void)
{
	/* Remove any leftover semaphore file (no-op if already gone). */
	(void)unlink("online.flg");
	if (chdir(g_origdir) != 0) {
		perror("chdir back");
		exit(1);
	}
	/* Temp directory should now be empty. */
	if (rmdir(g_tmpdir) != 0)
		perror("rmdir");
}

/* -------------------------------------------------------------------------
 * Helper
 * ------------------------------------------------------------------------- */
static int file_exists(const char *path)
{
	struct stat st;
	return stat(path, &st) == 0;
}

/* -------------------------------------------------------------------------
 * Tests: CreateSemaphor
 * ------------------------------------------------------------------------- */
static void test_create_success(void)
{
	ASSERT_EQ(CreateSemaphor(1), true);
	ASSERT_EQ(file_exists("online.flg"), 1);
	RemoveSemaphor();
}

static void test_create_exclusive(void)
{
	/* A second call while the flag is held must fail. */
	ASSERT_EQ(CreateSemaphor(1), true);
	ASSERT_EQ(CreateSemaphor(2), false);
	RemoveSemaphor();
}

/* -------------------------------------------------------------------------
 * Tests: RemoveSemaphor
 * ------------------------------------------------------------------------- */
static void test_remove_clears_flag(void)
{
	ASSERT_EQ(CreateSemaphor(1), true);
	RemoveSemaphor();
	ASSERT_EQ(file_exists("online.flg"), 0);
}

static void test_create_after_remove(void)
{
	/* Flag can be re-acquired after release. */
	ASSERT_EQ(CreateSemaphor(1), true);
	RemoveSemaphor();
	ASSERT_EQ(CreateSemaphor(1), true);
	RemoveSemaphor();
}

/* -------------------------------------------------------------------------
 * Tests: WaitSemaphor
 * ------------------------------------------------------------------------- */
static void test_wait_no_contention(void)
{
	/* No existing flag; WaitSemaphor must return immediately. */
	WaitSemaphor(1);
	ASSERT_EQ(file_exists("online.flg"), 1);
	ASSERT_EQ(g_log_calls, 0);   /* no "Waiting..." message when uncontested */
	RemoveSemaphor();
}

/*
 * Sleep hook: clear the flag so the next CreateSemaphor call inside
 * WaitSemaphor will succeed on the following iteration.
 */
static void clear_flag_hook(void)
{
	(void)unlink("online.flg");
}

static void test_wait_spins_until_cleared(void)
{
	FILE *fp;

	/* Occupy the flag without going through CreateSemaphor. */
	fp = fopen("online.flg", "w");
	if (!fp) { g_tests_failed++; return; }
	fclose(fp);

	g_sleep_hook = clear_flag_hook;
	WaitSemaphor(1);
	g_sleep_hook = NULL;

	/* WaitSemaphor must have acquired the flag. */
	ASSERT_EQ(file_exists("online.flg"), 1);
	/* The "Waiting..." message must have been emitted exactly once. */
	ASSERT_EQ(g_log_calls, 1);
	RemoveSemaphor();
}

/* -------------------------------------------------------------------------
 * main
 * ------------------------------------------------------------------------- */
int main(void)
{
	g_test_setup    = setup;
	g_test_teardown = teardown;

	RUN(create_success);
	RUN(create_exclusive);
	RUN(remove_clears_flag);
	RUN(create_after_remove);
	RUN(wait_no_contention);
	RUN(wait_spins_until_cleared);

	printf("\n%d/%d passed\n", g_tests_run - g_tests_failed, g_tests_run);
	return g_tests_failed ? 1 : 0;
}
