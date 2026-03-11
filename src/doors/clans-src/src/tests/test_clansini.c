/*
 * Unit tests for src/clansini.c
 * Uses the "include the C file" pattern -- no changes to existing sources.
 *
 * NOTE: clansini.c defines the global "struct IniFile IniFile".
 * Do NOT define IniFile in this file — the included source provides it.
 *
 * Target function:
 *   ClansIni_Close – frees all malloc'd string pointers in IniFile and sets
 *                    IniFile.Initialized to false.  If Initialized is already
 *                    false the function is a no-op.
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
#include "../clansini.h"
#include "../door.h"
#include "../language.h"
#include "../video.h"

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
#include "../clansini.c"

/* Note: test_clansini.c defines its own System_Error and CheckMem above,
 * plus its own System, Verbose, _argc, _argv below, so it does NOT include
 * mocks_system.h (which would cause redefinition). */

#include "mocks_door.h"
#include "mocks_video.h"
#include "mocks_language.h"
#include "mocks_quests.h"

/* -------------------------------------------------------------------------
 * External variable definitions
 * ------------------------------------------------------------------------- */
struct system   System;
bool            Verbose         = false;
int             _argc           = 0;
static char    *_argv_buf[]     = {NULL};
char          **_argv           = _argv_buf;

/* quests.h externs (pulled in through the deserialize include chain) */
struct Quest    Quests[MAX_QUESTS];
uint8_t         Quests_TFlags[8];

/* -------------------------------------------------------------------------
 * Tests: ClansIni_Close
 *
 * ClansIni_Close checks IniFile.Initialized; if false it returns immediately.
 * Otherwise it frees every non-NULL string pointer in IniFile and sets
 * Initialized to false.
 * ------------------------------------------------------------------------- */
static void test_close_uninitialized_noop(void)
{
	/* IniFile.Initialized is false from zeroing — Close must be a no-op */
	IniFile.Initialized = false;
	ClansIni_Close();
	ASSERT_EQ((int)IniFile.Initialized, 0);
}

static void test_close_sets_initialized_false(void)
{
	/* Initialized with all NULL pointers — Close should clear the flag */
	memset(&IniFile, 0, sizeof(IniFile));
	IniFile.Initialized = true;

	ClansIni_Close();

	ASSERT_EQ((int)IniFile.Initialized, 0);
}

static void test_close_frees_language(void)
{
	/* pszLanguage set to a malloc'd string — Close must free it without crash */
	memset(&IniFile, 0, sizeof(IniFile));
	IniFile.Initialized   = true;
	IniFile.pszLanguage   = strdup("lang.xl");
	assert(IniFile.pszLanguage != NULL);

	ClansIni_Close();

	ASSERT_EQ((int)IniFile.Initialized, 0);
	/* No crash == pass (double-free would crash or ASAN would catch it) */
}

static void test_close_frees_npc_and_spell_files(void)
{
	/* Set a few NPC and spell filenames then close */
	memset(&IniFile, 0, sizeof(IniFile));
	IniFile.Initialized        = true;
	IniFile.pszNPCFileName[0]  = strdup("monsters.npc");
	IniFile.pszNPCFileName[1]  = strdup("bosses.npc");
	IniFile.pszSpells[0]       = strdup("spells.spl");
	assert(IniFile.pszNPCFileName[0] != NULL);
	assert(IniFile.pszNPCFileName[1] != NULL);
	assert(IniFile.pszSpells[0]      != NULL);

	ClansIni_Close();

	ASSERT_EQ((int)IniFile.Initialized, 0);
}

static void test_close_idempotent(void)
{
	/* A second call to ClansIni_Close must be harmless (early return) */
	memset(&IniFile, 0, sizeof(IniFile));
	IniFile.Initialized = true;
	IniFile.pszLanguage = strdup("lang.xl");
	assert(IniFile.pszLanguage != NULL);

	ClansIni_Close();   /* first call frees strings, clears Initialized */
	ClansIni_Close();   /* second call must be a no-op */

	ASSERT_EQ((int)IniFile.Initialized, 0);
}

/* -------------------------------------------------------------------------
 * main
 * ------------------------------------------------------------------------- */
int main(void)
{
	RUN(close_uninitialized_noop);
	RUN(close_sets_initialized_false);
	RUN(close_frees_language);
	RUN(close_frees_npc_and_spell_files);
	RUN(close_idempotent);

	printf("\n%d/%d passed\n", g_tests_run - g_tests_failed, g_tests_run);
	return g_tests_failed ? 1 : 0;
}
