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
 * language.h externs + stubs
 * alliance.c includes language.h; Language pointer declared extern there.
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
 * empire.h stubs
 * alliance.c includes empire.h but doesn't call these functions.
 * They are declared here to avoid link errors if any other included header
 * or macro expands to reference them.
 * ------------------------------------------------------------------------- */
const struct BuildingType BuildingType[NUM_BUILDINGTYPES] = {0};

int16_t ArmySpeed(struct Army *a)                 { (void)a; return 0; }
int64_t ArmyOffense(struct Army *a)               { (void)a; return 0; }
int64_t ArmyDefense(struct Army *a)               { (void)a; return 0; }
int64_t ArmyVitality(struct Army *a)              { (void)a; return 0; }
void ProcessAttackPacket(struct AttackPacket *p)  { (void)p; }
void ProcessResultPacket(struct AttackResult *p)  { (void)p; }
void Empire_Stats(struct empire *e)               { (void)e; }
void Empire_Maint(struct empire *e)               { (void)e; }
void Empire_Create(struct empire *e, bool u)      { (void)e; (void)u; }
void Empire_Manage(struct empire *e)              { (void)e; }
void DonateToEmpire(struct empire *e)             { (void)e; }

/* -------------------------------------------------------------------------
 * fight.h stubs
 * ------------------------------------------------------------------------- */
void FreeClan(struct clan *c)         { (void)c; }
void FreeClanMembers(struct clan *c)  { (void)c; }
void InitClan(struct clan *c)         { memset(c, 0, sizeof(*c)); }
void Fight_CheckLevelUp(void)         {}
int16_t Fight_Fight(struct clan *a, struct clan *d, bool he, bool cr, bool af)
	{ (void)a; (void)d; (void)he; (void)cr; (void)af; return 0; }
void Fight_Clan(void)                 {}
void Fight_Heal(struct clan *c)       { (void)c; }
void Fight_Monster(int16_t l, char *f){ (void)l; (void)f; }

/* -------------------------------------------------------------------------
 * user.h stubs
 * alliance.c includes user.h; PClan declared here.
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
 * game.h stubs
 * ------------------------------------------------------------------------- */
struct game Game;
void Game_Init(void)                              {}
void Game_Close(void)                             {}
void Game_Settings(void)                          {}
void Game_Start(void)                             {}
void Game_Write(void)                             {}

/* -------------------------------------------------------------------------
 * help.h stubs
 * ------------------------------------------------------------------------- */
void MainHelp(void)                               {}
void Help(const char *t, char *f)                 { (void)t; (void)f; }
void GeneralHelp(char *f)                         { (void)f; }

/* -------------------------------------------------------------------------
 * input.h stubs
 * ------------------------------------------------------------------------- */
char GetChoice(char *f, char *p, char *opts[], char *keys, char def, bool t)
	{ (void)f; (void)p; (void)opts; (void)keys; (void)def; (void)t; return 0; }
void GetStr(char *s, int16_t n, bool h)           { (void)s; (void)n; (void)h; }
long GetLong(const char *p, long d, long m)       { (void)p; (void)d; (void)m; return 0; }
void GetStringChoice(const char **a, int16_t n, char *p, int16_t *u, bool s,
	int16_t t, bool b)
	{ (void)a; (void)n; (void)p; (void)u; (void)s; (void)t; (void)b; }

/* -------------------------------------------------------------------------
 * items.h stubs
 * ------------------------------------------------------------------------- */
void    Items_Close(void)                                           {}
bool    ItemPenalty(struct pc *p, struct item_data *i)
	{ (void)p; (void)i; return false; }
void    Items_GiveItem(char *s)                                     { (void)s; }
void    Items_ReadScroll(struct pc *p, struct clan *c, int16_t t, int16_t n)
	{ (void)p; (void)c; (void)t; (void)n; }
void    Item_BuyItem(signed char t)                                 { (void)t; }
void    ShowItemStats(struct item_data *i, struct clan *c)          { (void)i; (void)c; }
void    Items_FindTreasureChest(void)                               {}
void    ReadBook(void)                                              {}
int16_t GetOpenItemSlot(struct clan *c)                             { (void)c; return -1; }
int16_t ChooseItem(const char *p, struct clan *c, int16_t t, int f)
	{ (void)p; (void)c; (void)t; (void)f; return ITEM_NO_MATCH; }
void    UnequipItemsFromPC(int16_t n)                               { (void)n; }

/* -------------------------------------------------------------------------
 * mail.h stubs
 * ------------------------------------------------------------------------- */
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

/* -------------------------------------------------------------------------
 * menus.h stubs
 * ------------------------------------------------------------------------- */
void GameLoop(void)                               {}
void Menus_ChatRoom(char *f)                      { (void)f; }

/* -------------------------------------------------------------------------
 * Include the source files under test.
 * platform.c and serialize/deserialize/myopen provide the I/O layer used by
 * Alliances_Init / Alliances_Close.  parsing.c is needed because alliance.c
 * includes parsing.h.  alliance.c is last — it defines Alliances[],
 * NumAlliances, Alliances_Initialized, and all alliance functions.
 * ------------------------------------------------------------------------- */
#include "../platform.c"
#include "../serialize.c"
#include "../deserialize.c"
#include "../parsing.c"
#include "../myopen.c"
#include "../alliance.c"

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
