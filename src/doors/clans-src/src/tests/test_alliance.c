/*
 * Unit tests for src/alliance.c
 * Uses the "include the C file" pattern -- no changes to existing sources.
 *
 * NOTE: alliance.c defines Alliances[], NumAlliances, and Alliances_Initialized
 * at file scope.  Do NOT define or stub these here.
 *
 * Targeted functions:
 *   DeleteAlliance  – removes element at Index from Alliances[], packs the
 *                     remaining pointers leftward with memmove, NULLs the
 *                     last slot, and decrements NumAlliances.
 *   FreeAlliances   – static; NULLs all Alliances[] slots and resets
 *                     NumAlliances / Alliances_Initialized.  Accessible here
 *                     via the #include pattern.
 */

#include <assert.h>
#include <setjmp.h>
#include <stdnoreturn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../defines.h"
#include "../structs.h"
#include "../system.h"
#include "../platform.h"
#include "../alliance.h"
#include "../door.h"
#include "../empire.h"
#include "../fight.h"
#include "../game.h"
#include "../help.h"
#include "../input.h"
#include "../items.h"
#include "../mail.h"
#include "../menus.h"
#include "../parsing.h"
#include "../language.h"
#include "../mstrings.h"
#include "../myopen.h"
#include "../user.h"

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
#include "../serialize.c"
#include "../deserialize.c"
#include "../parsing.c"
#include "../myopen.c"
#include "../alliance.c"

/* Note: test_alliance.c defines its own System_Error and CheckMem above,
 * so it does NOT include mocks_system.h (which would cause redefinition).
 * Other required externs are defined below. */

#include "mocks_door.h"
#include "mocks_language.h"
#include "mocks_empire.h"
#include "mocks_fight.h"
#include "mocks_user.h"
#include "mocks_game.h"
#include "mocks_help.h"
#include "mocks_input.h"
#include "mocks_items.h"
#include "mocks_mail.h"
#include "mocks_quests.h"

/* -------------------------------------------------------------------------
 * Helper: initialise the Alliances[] array to a known state with N malloc'd
 * Alliance objects in slots 0..N-1 and NULL in the remaining slots.
 * Returns the original set of pointers so tests can verify them after delete.
 * ------------------------------------------------------------------------- */
static void setup_alliances(int n, struct Alliance *ptrs[])
{
	/* clear the whole array first */
	for (int i = 0; i < MAX_ALLIANCES; i++)
		Alliances[i] = NULL;

	for (int i = 0; i < n; i++) {
		Alliances[i] = malloc(sizeof(struct Alliance));
		assert(Alliances[i] != NULL);
		memset(Alliances[i], 0, sizeof(struct Alliance));
		Alliances[i]->ID = (int16_t)(i + 1);   /* ID 1-based for easy ID */
		ptrs[i] = Alliances[i];
	}
	NumAlliances = (int16_t)n;
	Alliances_Initialized = true;
}

/* -------------------------------------------------------------------------
 * Tests: DeleteAlliance
 *
 * DeleteAlliance(Index) removes Alliances[Index], shifts the remaining
 * entries left via memmove, NULLs the last slot, and decrements NumAlliances.
 * ------------------------------------------------------------------------- */
static void test_deletealliance_first_of_three(void)
{
	struct Alliance *orig[3];
	setup_alliances(3, orig);

	/* orig[0], orig[1], orig[2] are the three malloc'd pointers */
	DeleteAlliance(0);

	/* slot 0 must now hold what was previously slot 1 */
	ASSERT_EQ(Alliances[0] == orig[1], 1);
	ASSERT_EQ(Alliances[1] == orig[2], 1);
	ASSERT_EQ(Alliances[2], 0);
	ASSERT_EQ(NumAlliances, 2);

	/* clean up */
	for (int i = 0; i < MAX_ALLIANCES; i++) { free(Alliances[i]); Alliances[i] = NULL; }
	NumAlliances = 0;
}

static void test_deletealliance_middle_of_three(void)
{
	struct Alliance *orig[3];
	setup_alliances(3, orig);

	DeleteAlliance(1);   /* delete the middle element */

	ASSERT_EQ(Alliances[0] == orig[0], 1);   /* first unchanged */
	ASSERT_EQ(Alliances[1] == orig[2], 1);   /* third shifted to slot 1 */
	ASSERT_EQ(Alliances[2], 0);
	ASSERT_EQ(NumAlliances, 2);

	for (int i = 0; i < MAX_ALLIANCES; i++) { free(Alliances[i]); Alliances[i] = NULL; }
	NumAlliances = 0;
}

static void test_deletealliance_last_of_three(void)
{
	struct Alliance *orig[3];
	setup_alliances(3, orig);

	DeleteAlliance(2);   /* delete the last element */

	ASSERT_EQ(Alliances[0] == orig[0], 1);
	ASSERT_EQ(Alliances[1] == orig[1], 1);
	ASSERT_EQ(Alliances[2], 0);
	ASSERT_EQ(NumAlliances, 2);

	for (int i = 0; i < MAX_ALLIANCES; i++) { free(Alliances[i]); Alliances[i] = NULL; }
	NumAlliances = 0;
}

static void test_deletealliance_uninitialized_errors(void)
{
	/* Calling DeleteAlliance when Alliances_Initialized=false must fatal */
	Alliances_Initialized = false;
	ASSERT_FATAL(DeleteAlliance(0));
}

/* -------------------------------------------------------------------------
 * Tests: FreeAlliances  (static — accessible via #include pattern)
 *
 * FreeAlliances() frees and NULLs all Alliances[] slots, sets NumAlliances=0,
 * and clears Alliances_Initialized.
 * ------------------------------------------------------------------------- */
static void test_freealliances_clears_array(void)
{
	struct Alliance *orig[2];
	setup_alliances(2, orig);

	FreeAlliances();

	ASSERT_EQ(NumAlliances, 0);
	ASSERT_EQ((int)Alliances_Initialized, 0);
	for (int i = 0; i < MAX_ALLIANCES; i++)
		ASSERT_EQ(Alliances[i], 0);
}

/* -------------------------------------------------------------------------
 * main
 * ------------------------------------------------------------------------- */
int main(void)
{
	RUN(deletealliance_first_of_three);
	RUN(deletealliance_middle_of_three);
	RUN(deletealliance_last_of_three);
	RUN(deletealliance_uninitialized_errors);
	RUN(freealliances_clears_array);

	printf("\n%d/%d passed\n", g_tests_run - g_tests_failed, g_tests_run);
	return g_tests_failed ? 1 : 0;
}
