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

static char g_rputs_buf[1024];
static size_t g_rputs_len;
void rputs(const char *s)             { size_t n = strlen(s); if (g_rputs_len + n < sizeof(g_rputs_buf)) { memcpy(g_rputs_buf + g_rputs_len, s, n); g_rputs_len += n; } g_rputs_buf[g_rputs_len] = '\0'; }
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

int8_t GetStat(struct pc *p, char s)  { return p->Attributes[(int)s]; }
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
 * Helper: create a minimal Spell struct for testing.
 * Caller must free() the returned pointer.
 * ------------------------------------------------------------------------- */
static struct Spell *make_spell(bool friendly, bool str_reduce, bool wis_reduce,
	const char *wearoff)
{
	struct Spell *sp = calloc(1, sizeof(*sp));
	sp->Friendly = friendly;
	sp->StrengthCanReduce = str_reduce;
	sp->WisdomCanReduce = wis_reduce;
	sp->pszWearoffStr = (char *)wearoff;
	return sp;
}

/* -------------------------------------------------------------------------
 * Setup/teardown for Spells_UpdatePCSpells tests
 * ------------------------------------------------------------------------- */
static void spells_update_setup(void)
{
	g_rputs_buf[0] = '\0';
	g_rputs_len = 0;
	for (int i = 0; i < MAX_SPELLS; i++)
		Spells[i] = NULL;
}

static void spells_update_teardown(void)
{
	for (int i = 0; i < MAX_SPELLS; i++) {
		free(Spells[i]);
		Spells[i] = NULL;
	}
}

/* -------------------------------------------------------------------------
 * Tests: Spells_UpdatePCSpells
 *
 * For each active spell slot (SpellNum != -1):
 *   - Reduces Energy by 10
 *   - If hostile + StrengthCanReduce: also reduces by Strength/2
 *   - If hostile + WisdomCanReduce: also reduces by Wisdom/2
 *   - If Energy <= 0: outputs wearoff string, sets SpellNum = -1
 * Also sets Spells_szCastDestination to PC name.
 * ------------------------------------------------------------------------- */
static void test_updatespells_reduces_energy(void)
{
	Spells[0] = make_spell(true, false, false, "worn off");
	struct pc p = {0};
	strlcpy(p.szName, "TestPC", sizeof(p.szName));
	p.SpellsInEffect[0].SpellNum = 0;
	p.SpellsInEffect[0].Energy = 100;
	/* Mark remaining slots inactive */
	for (int i = 1; i < MAX_SPELLS_IN_EFFECT; i++)
		p.SpellsInEffect[i].SpellNum = -1;

	Spells_UpdatePCSpells(&p);

	/* Friendly spell, no stat reduction: Energy -= 10 only */
	ASSERT_EQ(p.SpellsInEffect[0].Energy, 90);
	ASSERT_EQ(p.SpellsInEffect[0].SpellNum, 0);  /* still active */
	/* Verify destination name was set */
	ASSERT_EQ(strcmp(Spells_szCastDestination, "TestPC"), 0);
}

static void test_updatespells_strength_reduces_hostile(void)
{
	Spells[2] = make_spell(false, true, false, "str wore off");
	struct pc p = {0};
	strlcpy(p.szName, "Warrior", sizeof(p.szName));
	p.Attributes[ATTR_STRENGTH] = 20;  /* reduces by 20/2 = 10 */
	p.SpellsInEffect[0].SpellNum = 2;
	p.SpellsInEffect[0].Energy = 100;
	for (int i = 1; i < MAX_SPELLS_IN_EFFECT; i++)
		p.SpellsInEffect[i].SpellNum = -1;

	Spells_UpdatePCSpells(&p);

	/* Energy: 100 - 10 (base) - 10 (str/2) = 80 */
	ASSERT_EQ(p.SpellsInEffect[0].Energy, 80);
}

static void test_updatespells_wisdom_reduces_hostile(void)
{
	Spells[3] = make_spell(false, false, true, "wis wore off");
	struct pc p = {0};
	strlcpy(p.szName, "Mage", sizeof(p.szName));
	p.Attributes[ATTR_WISDOM] = 30;  /* reduces by 30/2 = 15 */
	p.SpellsInEffect[0].SpellNum = 3;
	p.SpellsInEffect[0].Energy = 100;
	for (int i = 1; i < MAX_SPELLS_IN_EFFECT; i++)
		p.SpellsInEffect[i].SpellNum = -1;

	Spells_UpdatePCSpells(&p);

	/* Energy: 100 - 10 (base) - 15 (wis/2) = 75 */
	ASSERT_EQ(p.SpellsInEffect[0].Energy, 75);
}

static void test_updatespells_both_reduce(void)
{
	Spells[4] = make_spell(false, true, true, "both wore off");
	struct pc p = {0};
	strlcpy(p.szName, "Paladin", sizeof(p.szName));
	p.Attributes[ATTR_STRENGTH] = 10;  /* 10/2 = 5 */
	p.Attributes[ATTR_WISDOM]   = 10;  /* 10/2 = 5 */
	p.SpellsInEffect[0].SpellNum = 4;
	p.SpellsInEffect[0].Energy = 100;
	for (int i = 1; i < MAX_SPELLS_IN_EFFECT; i++)
		p.SpellsInEffect[i].SpellNum = -1;

	Spells_UpdatePCSpells(&p);

	/* Energy: 100 - 10 - 5 - 5 = 80 */
	ASSERT_EQ(p.SpellsInEffect[0].Energy, 80);
}

static void test_updatespells_friendly_no_stat_reduce(void)
{
	/* Friendly spells never get stat reduction even if flags are set */
	Spells[5] = make_spell(true, true, true, "friendly off");
	struct pc p = {0};
	strlcpy(p.szName, "Cleric", sizeof(p.szName));
	p.Attributes[ATTR_STRENGTH] = 50;
	p.Attributes[ATTR_WISDOM]   = 50;
	p.SpellsInEffect[0].SpellNum = 5;
	p.SpellsInEffect[0].Energy = 100;
	for (int i = 1; i < MAX_SPELLS_IN_EFFECT; i++)
		p.SpellsInEffect[i].SpellNum = -1;

	Spells_UpdatePCSpells(&p);

	/* Friendly: only base 10 reduction, no stat effects */
	ASSERT_EQ(p.SpellsInEffect[0].Energy, 90);
}

static void test_updatespells_wearoff(void)
{
	Spells[1] = make_spell(true, false, false, "[WORN]");
	struct pc p = {0};
	strlcpy(p.szName, "Fading", sizeof(p.szName));
	p.SpellsInEffect[0].SpellNum = 1;
	p.SpellsInEffect[0].Energy = 10;  /* exactly 10 → goes to 0 */
	for (int i = 1; i < MAX_SPELLS_IN_EFFECT; i++)
		p.SpellsInEffect[i].SpellNum = -1;

	Spells_UpdatePCSpells(&p);

	/* Spell should be deactivated */
	ASSERT_EQ(p.SpellsInEffect[0].SpellNum, -1);
	ASSERT_EQ(p.SpellsInEffect[0].Energy, 0);
	/* Wearoff string should have been output */
	ASSERT_EQ(strstr(g_rputs_buf, "[WORN]") != NULL, 1);
}

static void test_updatespells_skips_inactive_slots(void)
{
	Spells[0] = make_spell(true, false, false, "worn");
	struct pc p = {0};
	strlcpy(p.szName, "Skip", sizeof(p.szName));
	/* All slots inactive */
	for (int i = 0; i < MAX_SPELLS_IN_EFFECT; i++)
		p.SpellsInEffect[i].SpellNum = -1;

	Spells_UpdatePCSpells(&p);

	/* Nothing should change, no crash */
	for (int i = 0; i < MAX_SPELLS_IN_EFFECT; i++)
		ASSERT_EQ(p.SpellsInEffect[i].SpellNum, -1);
}

static void test_updatespells_multiple_spells(void)
{
	Spells[0] = make_spell(true, false, false, "[WORN0]");
	Spells[1] = make_spell(true, false, false, "[WORN1]");
	struct pc p = {0};
	strlcpy(p.szName, "Multi", sizeof(p.szName));
	p.SpellsInEffect[0].SpellNum = 0;
	p.SpellsInEffect[0].Energy = 5;   /* will wear off (5-10 <= 0) */
	p.SpellsInEffect[1].SpellNum = 1;
	p.SpellsInEffect[1].Energy = 50;  /* stays active */
	for (int i = 2; i < MAX_SPELLS_IN_EFFECT; i++)
		p.SpellsInEffect[i].SpellNum = -1;

	Spells_UpdatePCSpells(&p);

	ASSERT_EQ(p.SpellsInEffect[0].SpellNum, -1);   /* wore off */
	ASSERT_EQ(p.SpellsInEffect[1].SpellNum, 1);     /* still active */
	ASSERT_EQ(p.SpellsInEffect[1].Energy, 40);      /* 50 - 10 */
	ASSERT_EQ(strstr(g_rputs_buf, "[WORN0]") != NULL, 1);
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

	g_test_setup = spells_update_setup;
	g_test_teardown = spells_update_teardown;
	RUN(updatespells_reduces_energy);
	RUN(updatespells_strength_reduces_hostile);
	RUN(updatespells_wisdom_reduces_hostile);
	RUN(updatespells_both_reduce);
	RUN(updatespells_friendly_no_stat_reduce);
	RUN(updatespells_wearoff);
	RUN(updatespells_skips_inactive_slots);
	RUN(updatespells_multiple_spells);

	printf("\n%d/%d passed\n", g_tests_run - g_tests_failed, g_tests_run);
	return g_tests_failed ? 1 : 0;
}
