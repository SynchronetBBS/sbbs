/*
 * Unit tests for src/npc.c
 * Uses the "include the C file" pattern -- no changes to existing sources.
 *
 * npc.c includes <OpenDoor.h> directly.  The binary is compiled with
 * -I$(SRC)odoors/ so the real header is found; we define od_control (the
 * single extern global OpenDoors expects) and stub od_clr_scr().
 *
 * Target function:
 *   NPC_ResetNPCClan – frees all malloc'd Member[] pointers, NULLs them,
 *                      zeroes szName, and sets all Items[].Available to false.
 *                      Pure in-memory cleanup; no file I/O.
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
#include "../clansini.h"
#include "../door.h"
#include "../fight.h"
#include "../game.h"
#include "../input.h"
#include "../language.h"
#include "../mstrings.h"
#include "../myopen.h"
#include "../news.h"
#include "../npc.h"
#include "../quests.h"
#include "../random.h"
#include "../user.h"
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
 * OpenDoor stub
 * ------------------------------------------------------------------------- */
void od_clr_scr(void) {}

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
 * npc.c includes video.h; NPC_ResetNPCClan doesn't call any video functions,
 * but they must be defined to satisfy the linker.
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
 * npc.c includes clansini.h which declares extern struct IniFile IniFile.
 * clansini.c is NOT in the source chain, so we define IniFile here.
 * ------------------------------------------------------------------------- */
struct IniFile IniFile;
void ClansIni_Init(void)              {}
void ClansIni_Close(void)             {}

/* -------------------------------------------------------------------------
 * fight.h stubs
 * npc.c includes fight.h; NPC_ResetNPCClan does not call any Fight_*
 * functions, but they must be defined to satisfy the linker.
 * ------------------------------------------------------------------------- */
void Fight_Monster(int16_t l, char *f)          { (void)l; (void)f; }
void FreeClan(struct clan *c)                   { (void)c; }
void FreeClanMembers(struct clan *c)            { (void)c; }
void InitClan(struct clan *c)                   { (void)c; }
void Fight_CheckLevelUp(void)                   {}
int16_t Fight_Fight(struct clan *a, struct clan *d,
	bool h, bool r, bool af)
	{ (void)a; (void)d; (void)h; (void)r; (void)af; return 0; }
void Fight_Clan(void)                           {}
void Fight_Heal(struct clan *c)                 { (void)c; }

/* -------------------------------------------------------------------------
 * game.h externs + stubs
 * ------------------------------------------------------------------------- */
struct game Game;
void Game_Init(void)                            {}
void Game_Close(void)                           {}
void Game_Settings(void)                        {}
void Game_Start(void)                           {}
void Game_Write(void)                           {}

/* -------------------------------------------------------------------------
 * input.h stubs
 * ------------------------------------------------------------------------- */
char GetChoice(char *f, char *p, char *opts[], char *keys, char def, bool t)
	{ (void)f; (void)p; (void)opts; (void)keys; (void)def; (void)t; return 0; }
void GetStr(char *s, int16_t n, bool h)         { (void)s; (void)n; (void)h; }
long GetLong(const char *p, long d, long m)     { (void)p; (void)d; (void)m; return 0; }
void GetStringChoice(const char **a, int16_t n, char *p, int16_t *u, bool s,
	int16_t t, bool b)
	{ (void)a; (void)n; (void)p; (void)u; (void)s; (void)t; (void)b; }

/* -------------------------------------------------------------------------
 * news.h stubs
 * ------------------------------------------------------------------------- */
void News_AddNews(char *s)            { (void)s; }
void News_ReadNews(bool t)            { (void)t; }
void News_CreateTodayNews(void)       {}

/* -------------------------------------------------------------------------
 * quests.h stubs
 * npc.c includes quests.h; NPC_ChatNPC calls RunEvent, but NPC_ResetNPCClan
 * does not.  Quests[] and Quests_TFlags are defined after source includes.
 * ------------------------------------------------------------------------- */
void ClearFlags(uint8_t *f)           { (void)f; }
bool RunEvent(bool q, bool reset_hp, char *ef, char *en, struct NPCInfo *ni, char *ni2)
	{ (void)q; (void)reset_hp; (void)ef; (void)en; (void)ni; (void)ni2; return false; }
void Quests_GoQuest(void)             {}
void Quests_Init(void)                {}
void Quests_Close(void)               {}

/* -------------------------------------------------------------------------
 * user.h externs + stubs
 * PClan is declared here since user.c is not in the source chain.
 * ------------------------------------------------------------------------- */
struct clan PClan;

int8_t GetStat(struct pc *p, char s)  { (void)p; (void)s; return 0; }
int16_t NumMembers(struct clan *c, bool h)
	{ (void)c; (void)h; return 0; }
void User_Maint(void)                                                {}
bool User_Init(void)                                                 { return false; }
void User_FirstTimeToday(void)                                       {}
void User_Close(void)                                                {}
void ClanStats(struct clan *c, bool b)                               { (void)c; (void)b; }
bool GetClanID(int16_t id[2], bool a, bool b, int16_t c, bool d)
	{ (void)id; (void)a; (void)b; (void)c; (void)d; return false; }
void ShowPlayerStats(struct pc *p, bool b)                           { (void)p; (void)b; }
void ListItems(struct clan *c)                                       { (void)c; }
bool GetClanNameID(char *n, size_t s, int16_t id[2])
	{ (void)n; (void)s; (void)id; return false; }
bool GetClan(int16_t id[2], struct clan *c)                          { (void)id; (void)c; return false; }
bool ClanExists(int16_t id[2])                                       { (void)id; return false; }
void PC_Create(struct pc *p, bool b)                                 { (void)p; (void)b; }
void Clan_Update(struct clan *c)                                     { (void)c; }
void ShowVillageStats(void)                                          {}
void User_List(void)                                                 {}
void User_ResetAllVotes(void)                                        {}
void DeleteClan(int16_t id[2], char *n, bool e)                      { (void)id; (void)n; (void)e; }
void User_Destroy(void)                                              {}
void User_Write(void)                                                {}

/* -------------------------------------------------------------------------
 * Include the source files under test.
 * platform.c provides string helpers; random.c provides my_random;
 * serialize/deserialize/myopen provide the file I/O layer.
 * npc.c is last — it defines NPC_ResetNPCClan and all NPC_* functions.
 * ------------------------------------------------------------------------- */
#include "../platform.c"
#include "../random.c"
#include "../serialize.c"
#include "../deserialize.c"
#include "../myopen.c"
#include "../npc.c"

/* -------------------------------------------------------------------------
 * External variable definitions
 * ------------------------------------------------------------------------- */
struct system   System;
bool            Verbose         = false;
int             _argc           = 0;
static char    *_argv_buf[]     = {NULL};
char          **_argv           = _argv_buf;

/* quests.h externs (pulled in through npc.c and the deserialize chain) */
struct Quest    Quests[MAX_QUESTS];
uint8_t         Quests_TFlags[8];

/* -------------------------------------------------------------------------
 * Tests: NPC_ResetNPCClan
 *
 * NPC_ResetNPCClan(clan) frees each non-NULL Member[], NULLs all Member[]
 * slots, zeroes szName[0], and clears Available on every Items[] entry.
 * ------------------------------------------------------------------------- */
static void test_npc_reset_empty_clan(void)
{
	/* All members NULL — no crash, name cleared, items unavailable */
	struct clan c = {0};
	strlcpy(c.szName, "Ghost Clan", sizeof(c.szName));
	c.Items[0].Available = true;
	c.Items[5].Available = true;

	NPC_ResetNPCClan(&c);

	ASSERT_EQ(c.szName[0], 0);
	for (int i = 0; i < MAX_ITEMS_HELD; i++)
		ASSERT_EQ((int)c.Items[i].Available, 0);
}

static void test_npc_reset_frees_members(void)
{
	struct clan c = {0};
	/* Allocate two members */
	c.Member[0] = malloc(sizeof(struct pc));
	c.Member[2] = malloc(sizeof(struct pc));
	assert(c.Member[0] != NULL);
	assert(c.Member[2] != NULL);

	NPC_ResetNPCClan(&c);

	/* All slots must be NULL after reset */
	for (int i = 0; i < MAX_MEMBERS; i++)
		ASSERT_EQ(c.Member[i], 0);
}

static void test_npc_reset_clears_name(void)
{
	struct clan c = {0};
	strlcpy(c.szName, "Orc Warband", sizeof(c.szName));

	NPC_ResetNPCClan(&c);

	ASSERT_EQ(c.szName[0], 0);
}

static void test_npc_reset_clears_items(void)
{
	struct clan c = {0};
	/* Mark several items available */
	for (int i = 0; i < MAX_ITEMS_HELD; i += 3)
		c.Items[i].Available = true;

	NPC_ResetNPCClan(&c);

	for (int i = 0; i < MAX_ITEMS_HELD; i++)
		ASSERT_EQ((int)c.Items[i].Available, 0);
}

static void test_npc_reset_idempotent(void)
{
	/* Calling reset twice must not crash (no double-free) */
	struct clan c = {0};
	c.Member[0] = malloc(sizeof(struct pc));
	assert(c.Member[0] != NULL);

	NPC_ResetNPCClan(&c);
	NPC_ResetNPCClan(&c);   /* all Members are NULL — should be a no-op */

	for (int i = 0; i < MAX_MEMBERS; i++)
		ASSERT_EQ(c.Member[i], 0);
}

/* -------------------------------------------------------------------------
 * main
 * ------------------------------------------------------------------------- */
int main(void)
{
	RUN(npc_reset_empty_clan);
	RUN(npc_reset_frees_members);
	RUN(npc_reset_clears_name);
	RUN(npc_reset_clears_items);
	RUN(npc_reset_idempotent);

	printf("\n%d/%d passed\n", g_tests_run - g_tests_failed, g_tests_run);
	return g_tests_failed ? 1 : 0;
}
