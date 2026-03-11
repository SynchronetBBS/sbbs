/*
 * Unit tests for src/spells.c
 * Uses the "include the C file" pattern -- no changes to existing sources.
 *
 * NOTE: spells.c defines Spells[], Spells_szCastDestination,
 * Spells_szCastSource, and Spells_CastValue at file scope.  Do NOT define
 * or stub these here.
 *
 * Targeted functions:
 *   Spells_ClearSpells – iterates over all Member[] slots in a clan and sets
 *                        every SpellsInEffect[i].SpellNum to -1 (inactive).
 *                        Pure loop with no I/O or allocations.
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
#include "../mstrings.h"
#include "../myopen.h"
#include "../user.h"
#include "../village.h"
#include "../video.h"

#include "test_harness.h"

/* -------------------------------------------------------------------------
 * System stubs
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
 * door.h externs + stubs
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
 * spells.c includes video.h; Spells_ClearSpells doesn't call any video
 * functions, but they must be defined to satisfy the linker for other
 * spells.c paths.
 * ------------------------------------------------------------------------- */
int ScreenWidth  = 80;
int ScreenLines  = 24;
void DisplayStr(const char *s)        { (void)s; }
void zputs(const char *s)             { (void)s; }

/* -------------------------------------------------------------------------
 * language.h externs + stubs
 * ------------------------------------------------------------------------- */
static struct Language g_lang;
struct Language *Language = &g_lang;

void Language_Init(char *f)           { (void)f; }
void Language_Close(void)             {}
void LoadStrings(int16_t s, int16_t n, char *arr[])
{
	for (int16_t i = 0; i < n; i++) arr[i] = "";
	(void)s;
}

/* -------------------------------------------------------------------------
 * clansini.h externs + stubs
 * spells.c includes clansini.h which declares extern struct IniFile IniFile.
 * ------------------------------------------------------------------------- */
struct IniFile IniFile;
void ClansIni_Init(void)              {}
void ClansIni_Close(void)             {}

/* -------------------------------------------------------------------------
 * user.h stubs
 * spells.c calls GetStat() inside Spells_UpdatePCSpells.  Since we only
 * test Spells_ClearSpells here (which never calls GetStat), a stub suffices.
 * PClan is declared here since user.c is not in the source chain.
 * ------------------------------------------------------------------------- */
struct clan PClan;

int8_t GetStat(struct pc *p, char s)  { (void)p; (void)s; return 0; }
int16_t NumMembers(struct clan *c, bool h)
	{ (void)c; (void)h; return 0; }

void User_Maint(void)                             {}
bool User_Init(void)                              { return false; }
void User_FirstTimeToday(void)                    {}
void User_Close(void)                             {}
void ClanStats(struct clan *c, bool b)            { (void)c; (void)b; }
bool GetClanID(int16_t id[2], bool a, bool b, int16_t c, bool d)
	{ (void)id; (void)a; (void)b; (void)c; (void)d; return false; }
void ShowPlayerStats(struct pc *p, bool b)        { (void)p; (void)b; }
void ListItems(struct clan *c)                    { (void)c; }
bool GetClanNameID(char *n, size_t s, int16_t id[2])
	{ (void)n; (void)s; (void)id; return false; }
bool GetClan(int16_t id[2], struct clan *c)       { (void)id; (void)c; return false; }
bool ClanExists(int16_t id[2])                    { (void)id; return false; }
void PC_Create(struct pc *p, bool b)              { (void)p; (void)b; }
void Clan_Update(struct clan *c)                  { (void)c; }
void ShowVillageStats(void)                       {}
void User_List(void)                              {}
void User_ResetAllVotes(void)                     {}
void DeleteClan(int16_t id[2], char *n)           { (void)id; (void)n; }
void User_Destroy(void)                           {}
void User_Write(void)                             {}

/* -------------------------------------------------------------------------
 * village.h externs + stubs
 * ------------------------------------------------------------------------- */
struct village Village;
void Village_Init(void)                           {}
void Village_Close(void)                          {}
void Village_Maint(void)                          {}
void Village_NewRuler(void)                       {}
int16_t OutsiderTownHallMenu(void)                { return 0; }
int16_t TownHallMenu(void)                        { return 0; }
void Village_Reset(void)                          {}

/* -------------------------------------------------------------------------
 * Include the source files under test.
 * platform.c provides string helpers; random.c provides my_random (used by
 * Spells_CastSpell); serialize/deserialize/myopen provide the file I/O layer.
 * spells.c is last — it defines Spells[] and all Spells_* functions.
 * ------------------------------------------------------------------------- */
#include "../platform.c"
#include "../random.c"
#include "../serialize.c"
#include "../deserialize.c"
#include "../myopen.c"
#include "../spells.c"

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
 * Helper: initialise all SpellsInEffect slots in a pc to "active" (SpellNum=0)
 * so that Spells_ClearSpells has something to clear.
 * ------------------------------------------------------------------------- */
static void pc_activate_spells(struct pc *p)
{
	for (int i = 0; i < 10; i++) {
		p->SpellsInEffect[i].SpellNum  = 0;
		p->SpellsInEffect[i].Energy    = 100;
	}
}

/* -------------------------------------------------------------------------
 * Tests: Spells_ClearSpells
 *
 * Spells_ClearSpells(clan) iterates over Member[0..MAX_MEMBERS-1].  For each
 * non-NULL member it sets SpellsInEffect[i].SpellNum = -1 for all 10 slots.
 * NULL members are silently skipped.
 * ------------------------------------------------------------------------- */
static void test_clearspells_empty_clan(void)
{
	/* All Member[] slots are NULL — no crash, no change */
	struct clan c = {0};
	Spells_ClearSpells(&c);
	/* Just verifying no crash (no ASSERT needed beyond reaching here) */
	ASSERT_EQ(1, 1);
}

static void test_clearspells_one_member(void)
{
	struct clan c = {0};
	struct pc   p = {0};
	pc_activate_spells(&p);
	c.Member[0] = &p;

	Spells_ClearSpells(&c);

	for (int i = 0; i < 10; i++)
		ASSERT_EQ(p.SpellsInEffect[i].SpellNum, -1);
}

static void test_clearspells_multiple_members(void)
{
	struct clan c  = {0};
	struct pc   p0 = {0};
	struct pc   p1 = {0};
	pc_activate_spells(&p0);
	pc_activate_spells(&p1);
	c.Member[0] = &p0;
	c.Member[1] = &p1;

	Spells_ClearSpells(&c);

	for (int i = 0; i < 10; i++) {
		ASSERT_EQ(p0.SpellsInEffect[i].SpellNum, -1);
		ASSERT_EQ(p1.SpellsInEffect[i].SpellNum, -1);
	}
}

static void test_clearspells_null_member_slots_skipped(void)
{
	/* Slots 0 and 3 filled; slots 1 and 2 NULL */
	struct clan c  = {0};
	struct pc   p0 = {0};
	struct pc   p3 = {0};
	pc_activate_spells(&p0);
	pc_activate_spells(&p3);
	c.Member[0] = &p0;
	c.Member[3] = &p3;
	/* Members 1 and 2 remain NULL */

	Spells_ClearSpells(&c);

	for (int i = 0; i < 10; i++) {
		ASSERT_EQ(p0.SpellsInEffect[i].SpellNum, -1);
		ASSERT_EQ(p3.SpellsInEffect[i].SpellNum, -1);
	}
	/* Verify NULL slots weren't touched (no crash is the real check here) */
	ASSERT_EQ(c.Member[1], 0);
	ASSERT_EQ(c.Member[2], 0);
}

/* -------------------------------------------------------------------------
 * main
 * ------------------------------------------------------------------------- */
int main(void)
{
	RUN(clearspells_empty_clan);
	RUN(clearspells_one_member);
	RUN(clearspells_multiple_members);
	RUN(clearspells_null_member_slots_skipped);

	printf("\n%d/%d passed\n", g_tests_run - g_tests_failed, g_tests_run);
	return g_tests_failed ? 1 : 0;
}
