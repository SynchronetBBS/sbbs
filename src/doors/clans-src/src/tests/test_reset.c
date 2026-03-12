/*
 * Unit tests for src/reset.c pure functions
 * Uses the "include the C file" pattern -- no changes to existing sources.
 *
 * Targeted functions:
 *   InitEmpire      – initializes struct empire with default values
 *   GenerateGameID  – generates a 14-char timestamp string YYYYMMDDHHmmss
 *   ClearFlags      – zeroes all 8 bytes of the flag array
 *
 * Note: ClearFlags is defined identically in both quests.c and reset.c,
 * as separate definitions in different translation units.
 */

#include <setjmp.h>
#include <stdnoreturn.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "../defines.h"
#include "../structs.h"
#include "../system.h"
#include "../platform.h"

#include "test_harness.h"

/*
 * System stubs
 */
static jmp_buf g_fatal_jmp;

noreturn void System_Error(char *szErrorMsg)
{
	(void)szErrorMsg;
	longjmp(g_fatal_jmp, 1);
}

void CheckMem(void *ptr)
{
	if (!ptr) longjmp(g_fatal_jmp, 1);
}

#include "../platform.c"

/* reset.c defines its own main(); rename it before including */
#define main reset_main
#include "../reset.c"
#undef main

/* -------------------------------------------------------------------------
 * Tests: InitEmpire
 * Initializes struct empire with:
 *   UserEmpire=1 → Land=100; UserEmpire=0 → Land=0
 *   All other fields: Points=0, Buildings all 0, WorkerEnergy=100,
 *   Army counts=0, Strategy defaults, today-counters=0
 * ------------------------------------------------------------------------- */
static void test_initempire_user(void)
{
	struct empire e = {0};

	/* Initialize with UserEmpire=1 */
	InitEmpire(&e, 1);

	/* Verify Land=100 */
	ASSERT_EQ((long long)e.Land, 100LL);

	/* Verify Points=0 */
	ASSERT_EQ((long long)e.Points, 0LL);

	/* Verify WorkerEnergy=100 */
	ASSERT_EQ((long long)e.WorkerEnergy, 100LL);

	/* Verify Army.Rating=100 */
	ASSERT_EQ((long long)e.Army.Rating, 100LL);

	/* Verify all Buildings are 0 */
	for (int i = 0; i < MAX_BUILDINGS; i++)
		ASSERT_EQ((long long)e.Buildings[i], 0LL);

	/* Verify today-counters are 0 */
	ASSERT_EQ((long long)e.SpiesToday, 0LL);
	ASSERT_EQ((long long)e.AttacksToday, 0LL);
	ASSERT_EQ((long long)e.LandDevelopedToday, 0LL);
}

static void test_initempire_npc(void)
{
	struct empire e = {0};

	/* Initialize with UserEmpire=0 */
	InitEmpire(&e, 0);

	/* Verify Land=0 (NPC empires start with no land) */
	ASSERT_EQ((long long)e.Land, 0LL);

	/* All other fields same as user */
	ASSERT_EQ((long long)e.Points, 0LL);
	ASSERT_EQ((long long)e.WorkerEnergy, 100LL);
	ASSERT_EQ((long long)e.Army.Rating, 100LL);
}

static void test_initempire_strategy_defaults(void)
{
	struct empire e = {0};

	InitEmpire(&e, 1);

	/* Verify strategy defaults */
	ASSERT_EQ((long long)e.Army.Strategy.AttackLength, 10LL);
	ASSERT_EQ((long long)e.Army.Strategy.DefendLength, 10LL);
	ASSERT_EQ((long long)e.Army.Strategy.LootLevel, 0LL);
	ASSERT_EQ((long long)e.Army.Strategy.AttackIntensity, 50LL);
	ASSERT_EQ((long long)e.Army.Strategy.DefendIntensity, 50LL);
}

/* -------------------------------------------------------------------------
 * Tests: GenerateGameID
 * Generates a 14-character string in format YYYYMMDDHHmmss (all digits).
 * We test length and all-digits, but cannot test the exact values since
 * they depend on the current time (which changes between test runs).
 * ------------------------------------------------------------------------- */
static void test_generategameid_length(void)
{
	char buf[32];

	GenerateGameID(buf, sizeof(buf));

	/* Verify the string is exactly 14 characters (not including NUL) */
	ASSERT_EQ((long long)strlen(buf), 14LL);
}

static void test_generategameid_all_digits(void)
{
	char buf[32];

	GenerateGameID(buf, sizeof(buf));

	/* Verify all 14 characters are digits */
	for (int i = 0; i < 14; i++) {
		char c = buf[i];
		ASSERT_EQ(c >= '0' && c <= '9', 1);
	}
}

/* -------------------------------------------------------------------------
 * Tests: ClearFlags
 * Identical to quests.c version: zeroes all 8 bytes of flag array.
 * Tests it here to verify the reset.c translation unit provides it.
 * We test just the basic case to ensure the function is present and works.
 * Full coverage already tested in test_quests.c
 * ------------------------------------------------------------------------- */
static void test_clearflags_reset_version(void)
{
	uint8_t flags[8];

	/* Fill with 0xFF */
	for (int i = 0; i < 8; i++)
		flags[i] = 0xFF;

	/* Call ClearFlags */
	ClearFlags(flags);

	/* Verify all bytes are now 0 */
	for (int i = 0; i < 8; i++)
		ASSERT_EQ((long long)flags[i], 0LL);
}

/* -------------------------------------------------------------------------
 * main
 * ------------------------------------------------------------------------- */
int main(void)
{
	RUN(initempire_user);
	RUN(initempire_npc);
	RUN(initempire_strategy_defaults);
	RUN(generategameid_length);
	RUN(generategameid_all_digits);
	RUN(clearflags_reset_version);

	printf("\n%d/%d passed\n", g_tests_run - g_tests_failed, g_tests_run);
	return g_tests_failed ? 1 : 0;
}
