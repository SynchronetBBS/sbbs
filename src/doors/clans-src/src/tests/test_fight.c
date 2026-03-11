/*
 * Unit tests for src/fight.c
 * Uses the "include the C file" pattern -- no changes to existing sources.
 *
 * fight.c includes <OpenDoor.h> directly.  The binary is compiled with
 * -I$(SRC)odoors/ so the real header is found; we define od_control (the
 * single extern global OpenDoors expects) and stub od_clr_scr().
 *
 * Tested functions (all pure computation, no I/O):
 *   InitClan           – zeroes a clan struct
 *   Fight_Heal         – restores HP to MaxHP for all living members
 *   Fight_Dead         – returns true when no living non-undead members remain
 *   NumUndeadMembers   – counts active undead members (static helper)
 *   FreeClanMembers    – frees and NULLs all member pointers
 *   MineFollowersGained – calculates followers gained from a mine fight
 *   GetDifficulty       – maps a dungeon level to a difficulty range
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
#include "../user.h"
#include "../village.h"
#include "../spells.h"
#include "../items.h"
#include "../news.h"
#include "../help.h"
#include "../input.h"
#include "../mail.h"
#include "../readcfg.h"
#include "../game.h"
#include "../ibbs.h"
#include "../mstrings.h"

#include "test_harness.h"

/*
 * System_Error and CheckMem — forward-declared as noreturn in system.h.
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
#include "../random.c"
#include "../serialize.c"
#include "../deserialize.c"
#include "../myopen.c"
#include "../fight.c"

/*
 * Custom implementations of GetStat and NumMembers (for functions under test)
 */
struct clan PClan;

int8_t GetStat(struct pc *PC, char Stat)
{
	/* Simplified: base attributes only, no weapon/spell modifiers. */
	return PC->Attributes[(int)Stat];
}

int16_t NumMembers(struct clan *Clan, bool OnlyOnesHere)
{
	int16_t cnt = 0;
	for (int i = 0; i < MAX_MEMBERS; i++)
		if (Clan->Member[i] &&
			(!OnlyOnesHere || Clan->Member[i]->Status == Here))
			cnt++;
	return cnt;
}

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

/* Note: test_fight.c defines its own System_Error and CheckMem above,
 * so it does NOT include mocks_system.h (which would cause redefinition).
 * Other required externs are defined in the test file itself. */

#include "mocks_od.h"
#include "mocks_door.h"
#include "mocks_video.h"
#include "mocks_language.h"
#include "mocks_spells.h"
#include "mocks_items.h"
#include "mocks_help.h"
#include "mocks_input.h"
#include "mocks_mail.h"
#include "mocks_news.h"
#include "mocks_village.h"
#include "mocks_readcfg.h"
#include "mocks_game.h"
#include "mocks_ibbs.h"
#include "mocks_quests.h"

/* -------------------------------------------------------------------------
 * Helper: allocate and initialise a minimal struct pc.
 * ------------------------------------------------------------------------- */
static struct pc *make_pc(int16_t hp, int16_t maxhp, int8_t status,
	bool undead, struct clan *clan)
{
	struct pc *p = calloc(1, sizeof(*p));
	p->HP     = hp;
	p->MaxHP  = maxhp;
	p->Status = status;
	p->Undead = undead ? 1 : 0;
	p->MyClan = clan;
	for (int i = 0; i < 10; i++)
		p->SpellsInEffect[i].SpellNum = -1;  /* no spells in effect */
	return p;
}

/* -------------------------------------------------------------------------
 * Tests: InitClan
 * ------------------------------------------------------------------------- */
static void test_initclan_zeros_all(void)
{
	struct clan c;
	/* Poison the memory first. */
	memset(&c, 0xAB, sizeof(c));
	InitClan(&c);
	/* Every member pointer must be NULL after init. */
	for (int i = 0; i < MAX_MEMBERS; i++)
		ASSERT_EQ((long long)(uintptr_t)c.Member[i], 0LL);
}

/* -------------------------------------------------------------------------
 * Tests: Fight_Heal
 * ------------------------------------------------------------------------- */
static void test_fight_heal_restores_hp(void)
{
	struct clan c;
	InitClan(&c);
	c.Member[0] = make_pc(10, 100, Here, false, &c);
	c.Member[1] = make_pc(50, 200, Here, false, &c);

	Fight_Heal(&c);

	ASSERT_EQ(c.Member[0]->HP, 100);
	ASSERT_EQ(c.Member[1]->HP, 200);

	FreeClanMembers(&c);
}

static void test_fight_heal_skips_away(void)
{
	/* Members with Status != Here must not be healed. */
	struct clan c;
	InitClan(&c);
	c.Member[0] = make_pc(5, 100, RanAway, false, &c);

	Fight_Heal(&c);

	ASSERT_EQ(c.Member[0]->HP, 5);   /* unchanged */
	FreeClanMembers(&c);
}

/* -------------------------------------------------------------------------
 * Tests: Fight_Dead
 * ------------------------------------------------------------------------- */
static void test_fight_dead_empty_clan(void)
{
	struct clan c;
	InitClan(&c);
	/* No members → no living members → clan is "dead". */
	ASSERT_EQ(Fight_Dead(&c), 1);
}

static void test_fight_dead_one_living(void)
{
	struct clan c;
	InitClan(&c);
	c.Member[0] = make_pc(50, 100, Here, false, &c);
	ASSERT_EQ(Fight_Dead(&c), 0);
	FreeClanMembers(&c);
}

static void test_fight_dead_all_undead(void)
{
	/* Undead members do not count as "living" for Fight_Dead. */
	struct clan c;
	InitClan(&c);
	c.Member[0] = make_pc(50, 100, Here, true, &c);  /* undead */
	ASSERT_EQ(Fight_Dead(&c), 1);
	FreeClanMembers(&c);
}

static void test_fight_dead_away_member(void)
{
	/* A non-Here member is not counted as living. */
	struct clan c;
	InitClan(&c);
	c.Member[0] = make_pc(50, 100, Dead, false, &c);
	ASSERT_EQ(Fight_Dead(&c), 1);
	FreeClanMembers(&c);
}

/* -------------------------------------------------------------------------
 * Tests: NumUndeadMembers  (static — accessible via #include pattern)
 * ------------------------------------------------------------------------- */
static void test_numundead_none(void)
{
	struct clan c;
	InitClan(&c);
	c.Member[0] = make_pc(50, 100, Here, false, &c);
	ASSERT_EQ(NumUndeadMembers(&c), 0);
	FreeClanMembers(&c);
}

static void test_numundead_some(void)
{
	struct clan c;
	InitClan(&c);
	c.Member[0] = make_pc(50, 100, Here, false, &c);
	c.Member[1] = make_pc(30,  80, Here, true,  &c);
	c.Member[2] = make_pc(20,  60, Here, true,  &c);
	ASSERT_EQ(NumUndeadMembers(&c), 2);
	FreeClanMembers(&c);
}

/* -------------------------------------------------------------------------
 * Tests: FreeClanMembers
 * ------------------------------------------------------------------------- */
static void test_freeclanmembers_nulls_pointers(void)
{
	struct clan c;
	InitClan(&c);
	c.Member[0] = make_pc(10, 100, Here, false, &c);
	c.Member[2] = make_pc(20, 100, Here, false, &c);

	FreeClanMembers(&c);

	for (int i = 0; i < MAX_MEMBERS; i++)
		ASSERT_EQ((long long)(uintptr_t)c.Member[i], 0LL);
}

/* -------------------------------------------------------------------------
 * Tests: MineFollowersGained  (static — accessible via #include pattern)
 *
 * The function returns a random value in a range; we seed with zero and
 * verify the returned value falls within the documented range for each tier.
 * (my_random(n) with a freshly seeded generator returns 0 for the first
 * call, so the minimum of each range is a reliable bound to test against.)
 * ------------------------------------------------------------------------- */
static void test_mine_followers_level1(void)
{
	int32_t v = MineFollowersGained(1);
	ASSERT_EQ(v >= 4, 1);
	ASSERT_EQ(v <= 6, 1);   /* 4 + random(3) → [4,6] */
}

static void test_mine_followers_level8(void)
{
	int32_t v = MineFollowersGained(8);
	ASSERT_EQ(v >= 7, 1);
	ASSERT_EQ(v <= 9, 1);   /* 7 + random(3) → [7,9] */
}

static void test_mine_followers_level12(void)
{
	int32_t v = MineFollowersGained(12);
	ASSERT_EQ(v >= 11, 1);
	ASSERT_EQ(v <= 13, 1);  /* 11 + random(3) → [11,13] */
}

static void test_mine_followers_level18(void)
{
	int32_t v = MineFollowersGained(18);
	ASSERT_EQ(v >= 14, 1);
	ASSERT_EQ(v <= 16, 1);  /* 14 + random(3) → [14,16] */
}

static void test_mine_followers_level25(void)
{
	/* Level > 20 is not in any tier — returns 0. */
	ASSERT_EQ(MineFollowersGained(25), 0);
}

/* -------------------------------------------------------------------------
 * Tests: GetDifficulty  (static — accessible via #include pattern)
 * ------------------------------------------------------------------------- */
static void test_difficulty_level1(void)
{
	int16_t d = GetDifficulty(1);
	ASSERT_EQ(d >= 5, 1);
	ASSERT_EQ(d <= 7, 1);   /* 5 + random(3) */
}

static void test_difficulty_level3(void)
{
	int16_t d = GetDifficulty(3);
	ASSERT_EQ(d >= 7, 1);
	ASSERT_EQ(d <= 10, 1);  /* 7 + random(4) */
}

static void test_difficulty_level5(void)
{
	int16_t d = GetDifficulty(5);
	ASSERT_EQ(d >= 20, 1);
	ASSERT_EQ(d <= 29, 1);  /* 20 + random(10) */
}

static void test_difficulty_level10(void)
{
	int16_t d = GetDifficulty(10);
	ASSERT_EQ(d >= 30, 1);
	ASSERT_EQ(d <= 39, 1);  /* 30 + random(10) */
}

static void test_difficulty_level15(void)
{
	int16_t d = GetDifficulty(15);
	ASSERT_EQ(d >= 45, 1);
	ASSERT_EQ(d <= 59, 1);  /* 45 + random(15) */
}

static void test_difficulty_level20(void)
{
	int16_t d = GetDifficulty(20);
	ASSERT_EQ(d >= 90, 1);
	ASSERT_EQ(d <= 119, 1); /* 90 + random(30) */
}

static void test_difficulty_level25(void)
{
	/* Level > 20: no branch matches, returns 0. */
	ASSERT_EQ(GetDifficulty(25), 0);
}

/* -------------------------------------------------------------------------
 * main
 * ------------------------------------------------------------------------- */
int main(void)
{
	RUN(initclan_zeros_all);
	RUN(fight_heal_restores_hp);
	RUN(fight_heal_skips_away);
	RUN(fight_dead_empty_clan);
	RUN(fight_dead_one_living);
	RUN(fight_dead_all_undead);
	RUN(fight_dead_away_member);
	RUN(numundead_none);
	RUN(numundead_some);
	RUN(freeclanmembers_nulls_pointers);
	RUN(mine_followers_level1);
	RUN(mine_followers_level8);
	RUN(mine_followers_level12);
	RUN(mine_followers_level18);
	RUN(mine_followers_level25);
	RUN(difficulty_level1);
	RUN(difficulty_level3);
	RUN(difficulty_level5);
	RUN(difficulty_level10);
	RUN(difficulty_level15);
	RUN(difficulty_level20);
	RUN(difficulty_level25);

	printf("\n%d/%d passed\n", g_tests_run - g_tests_failed, g_tests_run);
	return g_tests_failed ? 1 : 0;
}
