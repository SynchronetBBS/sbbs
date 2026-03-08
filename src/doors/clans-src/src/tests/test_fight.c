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

/* OpenDoor.h is found via -I$(SRC)odoors/ in the makefile rule. */
#include <OpenDoor.h>

/* Provide the storage for the single extern OpenDoors global. */
tODControl od_control;

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
 * OpenDoor.h stub: od_clr_scr() is the only od_* symbol called directly
 * by fight.c (in Fight_Monster, which we don't exercise in these tests).
 * ------------------------------------------------------------------------- */
void od_clr_scr(void) {}

/* -------------------------------------------------------------------------
 * door.h stubs
 * ------------------------------------------------------------------------- */
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
 * video.h externs
 * ------------------------------------------------------------------------- */
int ScreenWidth  = 80;
int ScreenLines  = 24;

/* -------------------------------------------------------------------------
 * language.h stubs
 * ------------------------------------------------------------------------- */
static struct Language g_lang;
struct Language *Language = &g_lang;

void Language_Init(char *f)           { (void)f; }
void Language_Close(void)             {}
void LoadStrings(int16_t s, int16_t n, char *arr[])
{
	for (int16_t i = 0; i < n; i++) arr[i] = "";
}

/* -------------------------------------------------------------------------
 * spells.h externs + stubs
 * The Spells[] array is all-NULL (no spells loaded); code in Fight_Dead,
 * Fight_Heal, etc. never indexes it for the test cases below.
 * ------------------------------------------------------------------------- */
char     Spells_szCastDestination[25] = "";
char     Spells_szCastSource[25]      = "";
int      Spells_CastValue             = 0;
struct Spell *Spells[MAX_SPELLS];  /* all NULL */

void Spells_Init(void)                              {}
void Spells_Close(void)                             {}
void Spells_UpdatePCSpells(struct pc *p)            { (void)p; }
void Spells_ClearSpells(struct clan *c)             { (void)c; }
void Spells_CastSpell(struct pc *p, struct clan *e, int16_t t, int16_t n)
	{ (void)p; (void)e; (void)t; (void)n; }

/* -------------------------------------------------------------------------
 * items.h stubs
 * ------------------------------------------------------------------------- */
void    Items_Close(void)                         {}
bool    ItemPenalty(struct pc *p, struct item_data *i)
	{ (void)p; (void)i; return false; }
void    Items_GiveItem(char *s)                   { (void)s; }
void    Items_ReadScroll(struct pc *p, struct clan *c, int16_t t, int16_t n)
	{ (void)p; (void)c; (void)t; (void)n; }
void    Item_BuyItem(signed char t)               { (void)t; }
void    ShowItemStats(struct item_data *i, struct clan *c) { (void)i; (void)c; }
void    Items_FindTreasureChest(void)             {}
void    ReadBook(void)                            {}
int16_t GetOpenItemSlot(struct clan *c)           { (void)c; return -1; }
int16_t ChooseItem(const char *p, struct clan *c, int16_t t, int f)
	{ (void)p; (void)c; (void)t; (void)f; return ITEM_NO_MATCH; }
void    UnequipItemsFromPC(int16_t n)             { (void)n; }

/* -------------------------------------------------------------------------
 * help.h, input.h, mail.h, news.h stubs
 * ------------------------------------------------------------------------- */
void MainHelp(void)                               {}
void Help(const char *t, char *f)                 { (void)t; (void)f; }
void GeneralHelp(char *f)                         { (void)f; }
char GetChoice(char *f, char *p, char *opts[], char *keys, char def, bool t)
	{ (void)f; (void)p; (void)opts; (void)keys; (void)def; (void)t; return 0; }
void GetStr(char *s, int16_t n, bool h)           { (void)s; (void)n; (void)h; }
long GetLong(const char *p, long d, long m)       { (void)p; (void)d; (void)m; return 0; }
void GetStringChoice(const char **a, int16_t n, char *p, int16_t *u, bool s,
	int16_t t, bool b)
	{ (void)a; (void)n; (void)p; (void)u; (void)s; (void)t; (void)b; }

bool    Mail_Read(void)                           { return false; }
void    Mail_Write(int16_t t)                     { (void)t; }
void    Mail_RequestAlliance(struct Alliance *a)  { (void)a; }
void    Mail_WriteToAllies(struct Alliance *a)    { (void)a; }
void    GenericMessage(char *s, int16_t to[2], int16_t from[2], char *f, bool r)
	{ (void)s; (void)to; (void)from; (void)f; (void)r; }
void    MyWriteMessage2(int16_t id[2], bool a, bool req, int16_t aid,
	char *an, bool g, int16_t wv)
	{ (void)id; (void)a; (void)req; (void)aid; (void)an; (void)g; (void)wv; }
void    PostMsj(struct Message *m)                { (void)m; }
void    Mail_Maint(void)                          {}
void    GlobalMsgPost(void)                       {}

void News_AddNews(char *s)                        { (void)s; }
void News_ReadNews(bool t)                        { (void)t; }
void News_CreateTodayNews(void)                   {}

/* -------------------------------------------------------------------------
 * village.h externs + stubs
 * fight.c reads Village.Data.ConscriptionRate and Village.Data.TaxRate in
 * Fight_GiveFollowers and Fight_BattleAttack; we zero-init Village so both
 * are 0 (no conscription, no tax).
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
 * user.h externs + stubs
 * PClan is defined here; GetStat and NumMembers are provided as real
 * implementations because they are called from functions under test.
 * ------------------------------------------------------------------------- */
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
void DeleteClan(int16_t id[2], char *n, bool e)   { (void)id; (void)n; (void)e; }
void User_Destroy(void)                           {}
void User_Write(void)                             {}

/* -------------------------------------------------------------------------
 * readcfg.h stubs
 * ------------------------------------------------------------------------- */
struct config Config;
void AddInboundDir(const char *d)                  { (void)d; }
bool Config_Init(uint16_t n, struct NodeData *(*f)(int))
	{ (void)n; (void)f; return false; }
void Config_Close(void)                            {}

/* -------------------------------------------------------------------------
 * game.h stubs
 * ------------------------------------------------------------------------- */
struct game Game;
void Game_Init(void)                               {}
void Game_Close(void)                              {}
void Game_Settings(void)                           {}
void Game_Start(void)                              {}
void Game_Write(void)                              {}

/* -------------------------------------------------------------------------
 * ibbs.h externs + stubs (fight.c includes ibbs.h but never calls IBBS_*)
 * ------------------------------------------------------------------------- */
struct ibbs IBBS;
const char aszShortMonthName[12][4] = {
	"Jan", "Feb", "Mar", "Apr", "May", "Jun",
	"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

/* -------------------------------------------------------------------------
 * Include the source files under test.
 * platform.c brings unix_wrappers.c; random.c provides my_random.
 * serialize.c/deserialize.c/myopen.c provide the file I/O layer used by
 * fight.c (Fight_GetMonster etc.).  fight.c is last.
 * ------------------------------------------------------------------------- */
#include "../platform.c"
#include "../random.c"
#include "../serialize.c"
#include "../deserialize.c"
#include "../myopen.c"
#include "../fight.c"

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
