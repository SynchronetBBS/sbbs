/*
 * Unit tests for src/items.c
 * Uses the "include the C file" pattern -- no changes to existing sources.
 *
 * items.c includes <OpenDoor.h> directly.  The binary is compiled with
 * -I$(SRC)odoors/ so the real header is found; we define od_control (the
 * single extern global OpenDoors expects) and stub od_clr_scr().
 *
 * NOTE: items.c defines the static Items{} struct and all Items_* functions
 * at file scope.  Do NOT define or stub those here.
 *
 * Targeted functions (pure slot-search and attribute-check logic):
 *   GetOpenItemSlot   – first Available=false slot in clan inventory
 *   GetUnusedItem     – first available, unequipped item of given type (static)
 *   GetUsedItem       – first equipped item; ITEM_NONE_EQUIPPED if items exist
 *                       but none are equipped; ITEM_NO_MATCH if no items (static)
 *   GetFirstItem      – first available item of given type (static)
 *   ItemPenalty       – true when PC lacks a required item attribute
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
#include "../game.h"
#include "../help.h"
#include "../input.h"
#include "../items.h"
#include "../language.h"
#include "../mstrings.h"
#include "../myopen.h"
#include "../random.h"
#include "../spells.h"
#include "../user.h"
#include "../village.h"

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
 * OpenDoor stub: od_clr_scr() is the only od_* symbol referenced from items.c
 * paths we might exercise.
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
 * items.c includes clansini.h which declares extern struct IniFile IniFile.
 * clansini.c is NOT in the source chain, so we define IniFile here.
 * ------------------------------------------------------------------------- */
struct IniFile IniFile;
void ClansIni_Init(void)              {}
void ClansIni_Close(void)             {}

/* -------------------------------------------------------------------------
 * spells.h externs + stubs
 * items.c includes spells.h; the pure item functions we test do not call any
 * Spells_* code, but definitions are required to satisfy the linker.
 * ------------------------------------------------------------------------- */
char     Spells_szCastDestination[25] = "";
char     Spells_szCastSource[25]      = "";
int      Spells_CastValue             = 0;
struct Spell *Spells[MAX_SPELLS];  /* all NULL */

void Spells_Init(void)                                               {}
void Spells_Close(void)                                              {}
void Spells_UpdatePCSpells(struct pc *p)                             { (void)p; }
void Spells_ClearSpells(struct clan *c)                              { (void)c; }
void Spells_CastSpell(struct pc *p, struct clan *e, int16_t t, int16_t n)
	{ (void)p; (void)e; (void)t; (void)n; }

/* -------------------------------------------------------------------------
 * village.h externs + stubs
 * ------------------------------------------------------------------------- */
struct village Village;
void Village_Init(void)                                              {}
void Village_Close(void)                                             {}
void Village_Maint(void)                                             {}
void Village_NewRuler(void)                                          {}
int16_t OutsiderTownHallMenu(void)                                   { return 0; }
int16_t TownHallMenu(void)                                           { return 0; }
void Village_Reset(void)                                             {}

/* -------------------------------------------------------------------------
 * user.h externs + stubs
 * PClan is declared here; UnequipItemsFromPC operates on it.
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
 * game.h externs + stubs
 * ------------------------------------------------------------------------- */
struct game Game;
void Game_Init(void)                                                 {}
void Game_Close(void)                                                {}
void Game_Settings(void)                                             {}
void Game_Start(void)                                                {}
void Game_Write(void)                                                {}

/* -------------------------------------------------------------------------
 * help.h and input.h stubs
 * ------------------------------------------------------------------------- */
void MainHelp(void)                                                  {}
void Help(const char *t, char *f)                                    { (void)t; (void)f; }
void GeneralHelp(char *f)                                            { (void)f; }
char GetChoice(char *f, char *p, char *opts[], char *keys, char def, bool t)
	{ (void)f; (void)p; (void)opts; (void)keys; (void)def; (void)t; return 0; }
void GetStr(char *s, int16_t n, bool h)                              { (void)s; (void)n; (void)h; }
long GetLong(const char *p, long d, long m)                          { (void)p; (void)d; (void)m; return 0; }
void GetStringChoice(const char **a, int16_t n, char *p, int16_t *u, bool s,
	int16_t t, bool b)
	{ (void)a; (void)n; (void)p; (void)u; (void)s; (void)t; (void)b; }

/* -------------------------------------------------------------------------
 * Include the source files under test.
 * platform.c provides string helpers and DaysSince1970 (called by PS_Sell);
 * random.c provides my_random (used by Items_FindTreasureChest);
 * serialize/deserialize/myopen provide the file I/O layer.
 * items.c is last — it defines the static Items struct and all Items_* fns.
 * ------------------------------------------------------------------------- */
#include "../platform.c"
#include "../random.c"
#include "../serialize.c"
#include "../deserialize.c"
#include "../myopen.c"
#include "../items.c"

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
 * Tests: GetOpenItemSlot
 *
 * An "open slot" is one where Available==false (no item currently stored).
 * The function returns the index of the first such slot, or ITEM_NO_MATCH.
 * ------------------------------------------------------------------------- */
static void test_getopenitemslot_empty_clan(void)
{
	struct clan c = {0};
	/* All Available=false — first slot is free */
	ASSERT_EQ(GetOpenItemSlot(&c), 0);
}

static void test_getopenitemslot_all_occupied(void)
{
	struct clan c = {0};
	for (int i = 0; i < MAX_ITEMS_HELD; i++)
		c.Items[i].Available = true;
	ASSERT_EQ(GetOpenItemSlot(&c), ITEM_NO_MATCH);
}

static void test_getopenitemslot_first_hole(void)
{
	struct clan c = {0};
	c.Items[0].Available = true;
	c.Items[1].Available = true;
	c.Items[2].Available = true;
	/* Slot 3 is the first Available=false slot */
	ASSERT_EQ(GetOpenItemSlot(&c), 3);
}

/* -------------------------------------------------------------------------
 * Tests: GetUnusedItem (static — accessible via #include pattern)
 *
 * Finds the first available item of a given type with UsedBy==0 (not equipped).
 * Returns ITEM_NO_MATCH if none found.
 * ------------------------------------------------------------------------- */
static void test_getunuseditem_none_available(void)
{
	struct clan c = {0};
	/* No Available items at all */
	ASSERT_EQ(GetUnusedItem(&c, I_WEAPON), ITEM_NO_MATCH);
}

static void test_getunuseditem_all_in_use(void)
{
	struct clan c = {0};
	for (int i = 0; i < 3; i++) {
		c.Items[i].Available = true;
		c.Items[i].cType     = I_WEAPON;
		c.Items[i].UsedBy    = 1;  /* equipped by member 1 */
	}
	ASSERT_EQ(GetUnusedItem(&c, I_WEAPON), ITEM_NO_MATCH);
}

static void test_getunuseditem_finds_unused(void)
{
	struct clan c = {0};
	/* Slots 0 and 1 equipped, slot 2 unequipped */
	c.Items[0].Available = true; c.Items[0].cType = I_WEAPON; c.Items[0].UsedBy = 1;
	c.Items[1].Available = true; c.Items[1].cType = I_WEAPON; c.Items[1].UsedBy = 2;
	c.Items[2].Available = true; c.Items[2].cType = I_WEAPON; c.Items[2].UsedBy = 0;
	ASSERT_EQ(GetUnusedItem(&c, I_WEAPON), 2);
}

static void test_getunuseditem_type_filter(void)
{
	struct clan c = {0};
	c.Items[0].Available = true; c.Items[0].cType = I_ARMOR;  c.Items[0].UsedBy = 0;
	c.Items[1].Available = true; c.Items[1].cType = I_WEAPON; c.Items[1].UsedBy = 0;
	/* Searching for I_WEAPON skips slot 0 (I_ARMOR) and returns 1 */
	ASSERT_EQ(GetUnusedItem(&c, I_WEAPON), 1);
}

/* -------------------------------------------------------------------------
 * Tests: GetUsedItem (static — accessible via #include pattern)
 *
 * Returns index of a currently-equipped item.  If items exist but none are
 * equipped: ITEM_NONE_EQUIPPED.  If no items: ITEM_NO_MATCH.
 * Books and scrolls are excluded (cannot be "equipped").
 * ------------------------------------------------------------------------- */
static void test_getuseditem_no_items(void)
{
	struct clan c = {0};
	ASSERT_EQ(GetUsedItem(&c, I_WEAPON), ITEM_NO_MATCH);
}

static void test_getuseditem_none_equipped(void)
{
	struct clan c = {0};
	c.Items[0].Available = true;
	c.Items[0].cType     = I_WEAPON;
	c.Items[0].UsedBy    = 0;   /* present but not equipped */
	ASSERT_EQ(GetUsedItem(&c, I_WEAPON), ITEM_NONE_EQUIPPED);
}

static void test_getuseditem_finds_equipped(void)
{
	struct clan c = {0};
	c.Items[1].Available = true;
	c.Items[1].cType     = I_WEAPON;
	c.Items[1].UsedBy    = 2;   /* equipped by member 2 */
	ASSERT_EQ(GetUsedItem(&c, I_WEAPON), 1);
}

static void test_getuseditem_skips_books_scrolls(void)
{
	struct clan c = {0};
	/* A book and a scroll with UsedBy set — should NOT count as "equippable" */
	c.Items[0].Available = true; c.Items[0].cType = I_BOOK;   c.Items[0].UsedBy = 1;
	c.Items[1].Available = true; c.Items[1].cType = I_SCROLL; c.Items[1].UsedBy = 1;
	/* No equippable items present → ITEM_NO_MATCH */
	ASSERT_EQ(GetUsedItem(&c, I_ITEM), ITEM_NO_MATCH);
}

/* -------------------------------------------------------------------------
 * Tests: GetFirstItem (static — accessible via #include pattern)
 *
 * Returns the first available item of the given type (I_ITEM matches all).
 * ------------------------------------------------------------------------- */
static void test_getfirstitem_none(void)
{
	struct clan c = {0};
	ASSERT_EQ(GetFirstItem(&c, I_WEAPON), ITEM_NO_MATCH);
}

static void test_getfirstitem_any_type(void)
{
	struct clan c = {0};
	c.Items[2].Available = true;
	c.Items[2].cType     = I_ARMOR;
	/* I_ITEM (0) matches all types */
	ASSERT_EQ(GetFirstItem(&c, I_ITEM), 2);
}

static void test_getfirstitem_specific_type(void)
{
	struct clan c = {0};
	c.Items[0].Available = true; c.Items[0].cType = I_ARMOR;
	c.Items[1].Available = true; c.Items[1].cType = I_WEAPON;
	/* Searching for I_WEAPON skips slot 0 */
	ASSERT_EQ(GetFirstItem(&c, I_WEAPON), 1);
}

/* -------------------------------------------------------------------------
 * Tests: ItemPenalty
 *
 * Returns true if any PC attribute is below the item's required attribute.
 * Zero requirements are ignored (the condition is `ReqAttributes[i] != 0`).
 * ------------------------------------------------------------------------- */
static void test_itempenalty_no_penalty(void)
{
	struct pc         pc   = {0};
	struct item_data  item = {0};
	pc.Attributes[0]      = 10;
	item.ReqAttributes[0] =  5;   /* PC meets the requirement */
	ASSERT_EQ(ItemPenalty(&pc, &item), false);
}

static void test_itempenalty_has_penalty(void)
{
	struct pc         pc   = {0};
	struct item_data  item = {0};
	pc.Attributes[1]      = 3;
	item.ReqAttributes[1] = 7;   /* PC is below the requirement */
	ASSERT_EQ(ItemPenalty(&pc, &item), true);
}

static void test_itempenalty_zero_requirements(void)
{
	struct pc         pc   = {0};
	struct item_data  item = {0};
	/* All ReqAttributes are 0: condition `ReqAttributes[i] != 0` is false */
	ASSERT_EQ(ItemPenalty(&pc, &item), false);
}

/* -------------------------------------------------------------------------
 * main
 * ------------------------------------------------------------------------- */
int main(void)
{
	RUN(getopenitemslot_empty_clan);
	RUN(getopenitemslot_all_occupied);
	RUN(getopenitemslot_first_hole);
	RUN(getunuseditem_none_available);
	RUN(getunuseditem_all_in_use);
	RUN(getunuseditem_finds_unused);
	RUN(getunuseditem_type_filter);
	RUN(getuseditem_no_items);
	RUN(getuseditem_none_equipped);
	RUN(getuseditem_finds_equipped);
	RUN(getuseditem_skips_books_scrolls);
	RUN(getfirstitem_none);
	RUN(getfirstitem_any_type);
	RUN(getfirstitem_specific_type);
	RUN(itempenalty_no_penalty);
	RUN(itempenalty_has_penalty);
	RUN(itempenalty_zero_requirements);

	printf("\n%d/%d passed\n", g_tests_run - g_tests_failed, g_tests_run);
	return g_tests_failed ? 1 : 0;
}
