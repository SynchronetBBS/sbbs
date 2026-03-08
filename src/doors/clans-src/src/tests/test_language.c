/*
 * Unit tests for src/language.c
 * Uses the "include the C file" pattern -- no changes to existing sources.
 *
 * NOTE: language.c defines the global "struct Language *Language" pointer.
 * Do NOT define Language in this file — the included source provides it.
 *
 * Target function:
 *   LoadStrings – sets each output pointer to the corresponding string
 *                 inside Language->BigString, indexed by Language->StrOffsets.
 *                 Pure pointer arithmetic; no file I/O, no allocation.
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
#include "../language.h"
#include "../myopen.h"

#include "test_harness.h"

/* -------------------------------------------------------------------------
 * System_Error and CheckMem — forward-declared as noreturn in system.h.
 * ------------------------------------------------------------------------- */
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

noreturn void System_Close(void)    { longjmp(g_fatal_jmp, 1); }
void System_Close_AtExit(void)      {}
void System_Init(void)              {}
void System_Maint(void)             {}
char *DupeStr(const char *s)        { return NULL; (void)s; }

/* -------------------------------------------------------------------------
 * door.h stubs
 * language.c calls LogDisplayStr() inside Language_Init (guarded by Verbose).
 * ------------------------------------------------------------------------- */
struct Door Door;

void rputs(const char *s)             { (void)s; }
void LogDisplayStr(const char *s)     { (void)s; }
void LogStr(const char *s)            { (void)s; }
char GetKey(void)                     { return 0; }
char GetKeyNoWait(void)               { return 0; }
char GetAnswer(const char *s)         { (void)s; return 0; }
void door_pause(void)                 {}
void Display(char *f)                 { (void)f; }
bool YesNo(char *s)                   { (void)s; return false; }
bool NoYes(char *s)                   { (void)s; return false; }
bool Door_AllowScreenPause(void)      { return false; }
void Door_ToggleScreenPause(void)     {}
void Door_ShowTitle(void)             {}
void InputCallback(void)              {}
void PutCh(char c)                    { (void)c; }
void rawputs(const char *s)           { (void)s; }
int16_t GetHoursLeft(void)            { return 0; }
int16_t GetMinutesLeft(void)          { return 0; }
void Door_Init(bool local)            { (void)local; }
void Door_Close(void)                 {}
bool Door_Initialized(void)           { return false; }
void Door_SetColorScheme(int8_t *s)   { (void)s; }

/* -------------------------------------------------------------------------
 * video.h externs + stubs
 * language.c includes video.h; DisplayStr/zputs are not called by LoadStrings
 * but must be defined to satisfy the linker.
 * ------------------------------------------------------------------------- */
int ScreenWidth  = 80;
int ScreenLines  = 24;
void DisplayStr(const char *s)        { (void)s; }
void zputs(const char *s)             { (void)s; }

/* -------------------------------------------------------------------------
 * Include the source files under test.
 * platform.c brings unix_wrappers.c (provides delay() called by Language_Init).
 * serialize.c / deserialize.c / myopen.c provide the file I/O layer used by
 * Language_Init.  language.c is last — it defines the Language pointer.
 * ------------------------------------------------------------------------- */
#include "../platform.c"
#include "../serialize.c"
#include "../deserialize.c"
#include "../myopen.c"
#include "../language.c"

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
 * Tests: LoadStrings
 *
 * LoadStrings(StartIndex, NumStrings, arr) sets:
 *   arr[i] = &Language->BigString[Language->StrOffsets[StartIndex + i]]
 *
 * The tests construct a Language struct on the stack, populate BigString
 * and StrOffsets manually, then verify LoadStrings returns the expected
 * pointers.  The Language global (defined by language.c) is pointed at
 * the local struct for each test.
 * ------------------------------------------------------------------------- */
static void test_loadstrings_single(void)
{
	struct Language lang = {0};
	char big[] = "hello";
	lang.BigString    = big;
	lang.StrOffsets[0] = 0;
	Language = &lang;

	char *arr[1] = {NULL};
	LoadStrings(0, 1, arr);

	/* arr[0] must point exactly to big[0] */
	ASSERT_EQ(arr[0] == big, 1);
	ASSERT_EQ(strcmp(arr[0], "hello"), 0);
}

static void test_loadstrings_multiple(void)
{
	struct Language lang = {0};
	/* BigString: "alpha\0beta\0" — offsets 0 and 6 */
	char big[] = "alpha\0beta";
	lang.BigString     = big;
	lang.StrOffsets[0] = 0;
	lang.StrOffsets[1] = 6;
	Language = &lang;

	char *arr[2] = {NULL, NULL};
	LoadStrings(0, 2, arr);

	ASSERT_EQ(strcmp(arr[0], "alpha"), 0);
	ASSERT_EQ(strcmp(arr[1], "beta"),  0);
}

static void test_loadstrings_startindex(void)
{
	struct Language lang = {0};
	/* Three strings packed in BigString: "one\0two\0three\0" */
	char big[] = "one\0two\0three";
	lang.BigString     = big;
	lang.StrOffsets[3] = 0;   /* index 3 → "one" */
	lang.StrOffsets[4] = 4;   /* index 4 → "two" */
	lang.StrOffsets[5] = 8;   /* index 5 → "three" */
	Language = &lang;

	char *arr[2] = {NULL, NULL};
	LoadStrings(4, 2, arr);   /* load indices 4 and 5 */

	ASSERT_EQ(strcmp(arr[0], "two"),   0);
	ASSERT_EQ(strcmp(arr[1], "three"), 0);
}

static void test_loadstrings_zero_count(void)
{
	/* LoadStrings with NumStrings=0 must be a no-op */
	struct Language lang = {0};
	char big[] = "ignored";
	lang.BigString     = big;
	lang.StrOffsets[0] = 0;
	Language = &lang;

	char *arr[1] = {(char *)0xDEAD};
	LoadStrings(0, 0, arr);

	/* arr[0] must be untouched */
	ASSERT_EQ(arr[0] == (char *)0xDEAD, 1);
}

/* -------------------------------------------------------------------------
 * main
 * ------------------------------------------------------------------------- */
int main(void)
{
	RUN(loadstrings_single);
	RUN(loadstrings_multiple);
	RUN(loadstrings_startindex);
	RUN(loadstrings_zero_count);

	printf("\n%d/%d passed\n", g_tests_run - g_tests_failed, g_tests_run);
	return g_tests_failed ? 1 : 0;
}
