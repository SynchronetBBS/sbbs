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
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "test_harness.h"

/*
 * plat_Delay() mock
 *
 * WaitSemaphor calls plat_Delay(1000) while spinning.  We replace it with
 * a hook so tests can simulate "another process released the lock" without
 * actually blocking.
 */
static void (*g_sleep_hook)(void) = NULL;

/* Override the default no-op plat_Delay before including mocks_platform.h */
#define MOCK_PLAT_DELAY_DEFINED
void plat_Delay(unsigned msec)
{
	(void)msec;
	if (g_sleep_hook)
		g_sleep_hook();
}

/*
 * plat_CreateSemfile() mock
 *
 * Controls return value via g_semfile_result.  Tracks calls via
 * g_semfile_calls and records last arguments for assertions.
 */
static bool g_semfile_result = true;
static int g_semfile_calls = 0;
static char g_semfile_filename[256];
static char g_semfile_content[256];

#define MOCK_PLAT_CREATESEMFILE_DEFINED
bool plat_CreateSemfile(const char *filename, const char *content)
{
	g_semfile_calls++;
	strlcpy(g_semfile_filename, filename, sizeof(g_semfile_filename));
	strlcpy(g_semfile_content, content, sizeof(g_semfile_content));
	return g_semfile_result;
}

#include "mocks_platform.h"
#include "../semfile.c"

/*
 * Custom LogDisplayStr with call counter
 * (prevent redefinition from mocks_door.h)
 */
static int g_log_calls = 0;
void LogDisplayStr(const char *s) { (void)s; g_log_calls++; }

#define MOCKS_DOOR_SKIP_LOG_DISPLAY_STR
#include "mocks_door.h"

/* -------------------------------------------------------------------------
 * Per-test setup / teardown
 * ------------------------------------------------------------------------- */
static void setup(void)
{
	g_log_calls      = 0;
	g_sleep_hook     = NULL;
	g_semfile_result = true;
	g_semfile_calls  = 0;
	g_semfile_filename[0] = 0;
	g_semfile_content[0]  = 0;
}

/* -------------------------------------------------------------------------
 * Tests: CreateSemaphor
 * ------------------------------------------------------------------------- */
static void test_create_success(void)
{
	g_semfile_result = true;
	ASSERT_EQ(CreateSemaphor(1), true);
	ASSERT_EQ(g_semfile_calls, 1);
	ASSERT_EQ(strcmp(g_semfile_filename, "online.flg"), 0);
	ASSERT_EQ(strstr(g_semfile_content, "Node: 1") != NULL, true);
}

static void test_create_exclusive(void)
{
	/* When plat_CreateSemfile returns false, CreateSemaphor must too. */
	g_semfile_result = false;
	ASSERT_EQ(CreateSemaphor(2), false);
	ASSERT_EQ(g_semfile_calls, 1);
}

/* -------------------------------------------------------------------------
 * Tests: RemoveSemaphor
 * ------------------------------------------------------------------------- */
static void test_remove_clears_flag(void)
{
	/* RemoveSemaphor calls plat_DeleteFile("online.flg").
	 * The mock plat_DeleteFile does a real unlink, but there's no
	 * file to delete — just verify it doesn't crash. */
	RemoveSemaphor();
}

static void test_create_after_remove(void)
{
	/* Verify CreateSemaphor works after a RemoveSemaphor cycle. */
	g_semfile_result = true;
	ASSERT_EQ(CreateSemaphor(1), true);
	RemoveSemaphor();
	g_semfile_calls = 0;
	ASSERT_EQ(CreateSemaphor(1), true);
	ASSERT_EQ(g_semfile_calls, 1);
}

/* -------------------------------------------------------------------------
 * Tests: WaitSemaphor
 * ------------------------------------------------------------------------- */
static void test_wait_no_contention(void)
{
	/* plat_CreateSemfile succeeds immediately; no spinning. */
	g_semfile_result = true;
	WaitSemaphor(1);
	ASSERT_EQ(g_semfile_calls, 1);
	ASSERT_EQ(g_log_calls, 0);
}

/*
 * Sleep hook: flip the mock to succeed so WaitSemaphor breaks out.
 */
static void succeed_on_retry_hook(void)
{
	g_semfile_result = true;
}

static void test_wait_spins_until_cleared(void)
{
	/* First call fails, sleep hook flips result, second call succeeds. */
	g_semfile_result = false;
	g_sleep_hook = succeed_on_retry_hook;
	WaitSemaphor(1);
	g_sleep_hook = NULL;

	/* Must have called plat_CreateSemfile at least twice. */
	ASSERT_EQ(g_semfile_calls >= 2, true);
	/* The "Waiting..." message must have been emitted exactly once. */
	ASSERT_EQ(g_log_calls, 1);
}

/* -------------------------------------------------------------------------
 * main
 * ------------------------------------------------------------------------- */
int main(void)
{
	g_test_setup    = setup;
	g_test_teardown = NULL;

	RUN(create_success);
	RUN(create_exclusive);
	RUN(remove_clears_flag);
	RUN(create_after_remove);
	RUN(wait_no_contention);
	RUN(wait_spins_until_cleared);

	printf("\n%d/%d passed\n", g_tests_run - g_tests_failed, g_tests_run);
	return g_tests_failed ? 1 : 0;
}
