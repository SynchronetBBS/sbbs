/*
 * Unit tests for src/user.c
 * Uses the "include the C file" pattern -- no changes to existing sources.
 *
 * NOTE: user.c defines struct clan PClan at file scope; do NOT define PClan
 * here — it is provided by the included source.
 *
 * Targeted functions:
 *   PadString   – static; pads a string with spaces to a given printed length,
 *                 computing printed length via RemovePipes (pipe codes stripped)
 *   GetStat     – returns a PC attribute value plus weapon/shield/armor modifiers
 *   NumMembers  – counts non-NULL clan members, optionally filtering to Here-only
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
#include "../alliance.h"
#include "../alliancem.h"
#include "../class.h"
#include "../empire.h"
#include "../fight.h"
#include "../game.h"
#include "../help.h"
#include "../ibbs.h"
#include "../input.h"
#include "../items.h"
#include "../mail.h"
#include "../misc.h"
#include "../mstrings.h"
#include "../myopen.h"
#include "../news.h"
#include "../parsing.h"
#include "../quests.h"
#include "../readcfg.h"
#include "../scores.h"
#include "../spells.h"
#include "../trades.h"
#include "../user.h"
#include "../village.h"
#include "../video.h"

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
 * OpenDoor.h stub
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
	(void)s;
}

/* -------------------------------------------------------------------------
 * spells.h externs + stubs
 * Spells[] is all-NULL; GetStat only accesses Spells[i] when
 * SpellsInEffect[i].SpellNum != -1.  Tests set SpellNum=-1 for all slots.
 * ------------------------------------------------------------------------- */
char     Spells_szCastDestination[25] = "";
char     Spells_szCastSource[25]      = "";
int      Spells_CastValue             = 0;
struct Spell *Spells[MAX_SPELLS];   /* all NULL */

void Spells_Init(void)                                              {}
void Spells_Close(void)                                             {}
void Spells_UpdatePCSpells(struct pc *p)                            { (void)p; }
void Spells_ClearSpells(struct clan *c)                             { (void)c; }
void Spells_CastSpell(struct pc *p, struct clan *e, int16_t t, int16_t n)
	{ (void)p; (void)e; (void)t; (void)n; }

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
 * ibbs.h externs + stubs
 * (PClan is defined by user.c itself; IBBS is not defined by user.c.)
 * ------------------------------------------------------------------------- */
struct ibbs IBBS;
const char aszShortMonthName[12][4] = {
	"Jan", "Feb", "Mar", "Apr", "May", "Jun",
	"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

void IBBS_Init(void)                              {}
void IBBS_Close(void)                             {}
void IBBS_LoginStats(void)                        {}
void IBBS_Maint(void)                             {}
void IBBS_SendAttackPacket(struct empire *ae, struct Army *aa, int16_t g,
	int16_t e, int16_t tt, int16_t id[2], int16_t d)
	{ (void)ae; (void)aa; (void)g; (void)e; (void)tt; (void)id; (void)d; }
void IBBS_EnqueueOutPacket(int16_t t, size_t l, void *d, int16_t dest)
	{ (void)t; (void)l; (void)d; (void)dest; }
void IBBS_SendSpy(struct empire *e, int16_t d)    { (void)e; (void)d; }
void IBBS_SendQueuedResults(void)                 {}
void IBBS_HandleQueuedPackets(void)               {}
void IBBS_ShowLeagueAscii(void)                   {}
void IBBS_LeagueInfo(void)                        {}
void IBBS_SendPacket(int16_t t, size_t l, void *d, int16_t dest)
	{ (void)t; (void)l; (void)d; (void)dest; }
void IBBS_UpdateReset(void)                       {}
void IBBS_UpdateRecon(void)                       {}
void IBBS_SeeVillages(bool tr)                    { (void)tr; }
void IBBS_CurrentTravelInfo(void)                 {}
void IBBS_SendComeBack(int16_t id, struct clan *c){ (void)id; (void)c; }
bool IBBS_InList(char *n, bool cn)                { (void)n; (void)cn; return false; }
void RemoveFromUList(const int16_t id[2])         { (void)id; }
void IBBS_DistributeNDX(void)                     {}
void IBBS_SendReset(int16_t d)                    { (void)d; }
void IBBS_SendRecon(int16_t d)                    { (void)d; }
void LeagueKillUser(struct UserInfo *u)           { (void)u; }
void IBBS_PacketIn(void)                          {}
void IBBS_LeagueNewUser(struct UserInfo *u)       { (void)u; }
const char *VillageName(int16_t id)               { (void)id; return "?"; }
const char *BBSName(int16_t id)                   { (void)id; return "?"; }

/* -------------------------------------------------------------------------
 * empire.h stubs + BuildingType definition
 * (user.c calls Empire_Stats, Empire_Create, Empire_Maint)
 * ------------------------------------------------------------------------- */
const struct BuildingType BuildingType[NUM_BUILDINGTYPES] = {0};

void Empire_Stats(struct empire *e)               { (void)e; }
void Empire_Maint(struct empire *e)               { (void)e; }
void Empire_Create(struct empire *e, bool u)      { (void)e; (void)u; }
void Empire_Manage(struct empire *e)              { (void)e; }
void DonateToEmpire(struct empire *e)             { (void)e; }
int16_t ArmySpeed(struct Army *a)                 { (void)a; return 0; }
int64_t ArmyOffense(struct Army *a)               { (void)a; return 0; }
int64_t ArmyDefense(struct Army *a)               { (void)a; return 0; }
int64_t ArmyVitality(struct Army *a)              { (void)a; return 0; }
void ProcessAttackPacket(struct AttackPacket *p)  { (void)p; }
void ProcessResultPacket(struct AttackResult *p)  { (void)p; }

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
 * game.h stubs
 * ------------------------------------------------------------------------- */
struct game Game;
void Game_Init(void)                              {}
void Game_Close(void)                             {}
void Game_Settings(void)                          {}
void Game_Start(void)                             {}
void Game_Write(void)                             {}

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
 * readcfg.h stubs
 * ------------------------------------------------------------------------- */
struct config Config;
void AddInboundDir(const char *d)                  { (void)d; }
bool Config_Init(uint16_t n, struct NodeData *(*f)(int))
	{ (void)n; (void)f; return false; }
void Config_Close(void)                            {}

/* -------------------------------------------------------------------------
 * alliance.h + alliancem.h externs + stubs
 * ------------------------------------------------------------------------- */
struct Alliance *Alliances[MAX_ALLIANCES];
int16_t NumAlliances = 0;

void DeleteAlliance(int i)                        { (void)i; }
void Alliances_Init(void)                         {}
void Alliances_Close(void)                        {}
void KillAlliances(void)                          {}

void CreateAlliance(struct Alliance *a)           { (void)a; }
void ShowAlliances(struct clan *c)                { (void)c; }
bool EnterAlliance(struct Alliance *a)            { (void)a; return false; }
void FormAlliance(int16_t id)                     { (void)id; }
void KillAlliance(int16_t w)                      { (void)w; }
void Alliance_Maint(void)                         {}

/* -------------------------------------------------------------------------
 * class.h externs + stubs
 * ------------------------------------------------------------------------- */
struct PClass *PClasses[MAX_PCLASSES];
struct PClass *Races[MAX_PCLASSES];

void PClass_Init(void)                            {}
void PClass_Close(void)                           {}

/* -------------------------------------------------------------------------
 * quests.h externs + stubs
 * ------------------------------------------------------------------------- */
struct Quest    Quests[MAX_QUESTS];
uint8_t         Quests_TFlags[8];

void ClearFlags(uint8_t *f)                       { memset(f, 0, 8); }
bool RunEvent(bool q, char *ef, char *en, struct NPCInfo *n, char *ni)
	{ (void)q; (void)ef; (void)en; (void)n; (void)ni; return false; }
void Quests_GoQuest(void)                         {}
void Quests_Init(void)                            {}
void Quests_Close(void)                           {}

/* -------------------------------------------------------------------------
 * scores.h stubs
 * ------------------------------------------------------------------------- */
void DisplayScores(bool m)                                          { (void)m; }
void CreateScoreData(bool l)                                        { (void)l; }
void SendScoreList(void)                                            {}
void LeagueScores(void)                                             {}
void ProcessScoreData(struct UserScore **u)                         { (void)u; }
void RemoveFromIPScores(const int16_t id[2])                        { (void)id; }

/* -------------------------------------------------------------------------
 * trades.h stubs
 * ------------------------------------------------------------------------- */
void Trades_MakeTrade(void)                       {}
void Trades_CheckTrades(void)                     {}
void Trades_Maint(void)                           {}
void RejectTrade(struct TradeData *t)             { (void)t; }

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
 * news.h stubs
 * ------------------------------------------------------------------------- */
void News_AddNews(char *s)                        { (void)s; }
void News_ReadNews(bool t)                        { (void)t; }
void News_CreateTodayNews(void)                   {}

/* -------------------------------------------------------------------------
 * Include the source files under test.
 * platform.c brings unix_wrappers.c and string helpers (RemovePipes is
 * used by PadString); random.c provides my_random; serialize/deserialize/
 * myopen provide the file I/O layer; user.c is last.
 * Note: user.c defines PClan — do NOT define it in this file.
 * ------------------------------------------------------------------------- */
#include "../platform.c"
#include "../random.c"
#include "../serialize.c"
#include "../deserialize.c"
#include "../myopen.c"
#include "../user.c"

/* -------------------------------------------------------------------------
 * External variable definitions
 * ------------------------------------------------------------------------- */
struct system   System;
bool            Verbose         = false;
int             _argc           = 0;
static char    *_argv_buf[]     = {NULL};
char          **_argv           = _argv_buf;

/* -------------------------------------------------------------------------
 * Helper: initialise all SpellsInEffect slots to "no spell" (-1).
 * Without this, SpellNum defaults to 0 and GetStat would dereference
 * Spells[0] which is NULL.
 * ------------------------------------------------------------------------- */
static void pc_clear_spells(struct pc *p)
{
	for (int i = 0; i < 10; i++)
		p->SpellsInEffect[i].SpellNum = -1;
}

/* -------------------------------------------------------------------------
 * Tests: PadString  (static — accessible via #include pattern)
 *
 * PadString(buf, n) appends spaces to buf until its printed length equals n.
 * Printed length is computed by stripping pipe/backtick colour codes via
 * RemovePipes().  If printed length is already >= n, buf is unchanged.
 * The caller must ensure buf is large enough to hold the padded result.
 * ------------------------------------------------------------------------- */
static void test_padstring_plain(void)
{
	char buf[64];
	strlcpy(buf, "Hello", sizeof(buf));
	PadString(buf, 10);
	/* "Hello" has printed length 5; 5 spaces appended → "Hello     " */
	ASSERT_EQ(strcmp(buf, "Hello     "), 0);
}

static void test_padstring_no_padding_needed(void)
{
	char buf[64];
	strlcpy(buf, "Hello", sizeof(buf));
	PadString(buf, 3);
	/* printed length 5 >= 3 → no change */
	ASSERT_EQ(strcmp(buf, "Hello"), 0);
}

static void test_padstring_empty_string(void)
{
	char buf[64];
	strlcpy(buf, "", sizeof(buf));
	PadString(buf, 5);
	/* printed length 0 < 5 → 5 spaces */
	ASSERT_EQ(strcmp(buf, "     "), 0);
}

static void test_padstring_pipe_codes(void)
{
	char buf[64];
	strlcpy(buf, "|07Hi", sizeof(buf));
	PadString(buf, 7);
	/* RemovePipes strips "|07" → printed length "Hi"=2; 5 spaces appended */
	ASSERT_EQ(strcmp(buf, "|07Hi     "), 0);
}

/* -------------------------------------------------------------------------
 * Tests: GetStat
 *
 * GetStat(pc, stat) = base attribute
 *                   + weapon's Attributes[stat]   (if Weapon != 0)
 *                   + shield's Attributes[stat]   (if Shield != 0)
 *                   + armor's  Attributes[stat]   (if Armor  != 0)
 *                   + sum of spell modifiers       (for active spells)
 *
 * We test with Spells[] all-NULL, SpellNum=-1 for every slot (no spell
 * bonus), so only item modifiers apply.
 * ------------------------------------------------------------------------- */
static void test_getstat_base_only(void)
{
	struct clan c = {0};
	struct pc   p = {0};
	pc_clear_spells(&p);
	p.MyClan = &c;
	p.Attributes[0] = 10;
	/* No weapon/shield/armor, no spells → returns base 10 */
	ASSERT_EQ(GetStat(&p, 0), 10);
}

static void test_getstat_weapon_bonus(void)
{
	struct clan c = {0};
	struct pc   p = {0};
	pc_clear_spells(&p);
	p.MyClan = &c;
	p.Attributes[0] = 5;
	p.Weapon = 1;                        /* weapon → Items[0] */
	c.Items[0].Attributes[0] = 3;        /* weapon adds +3 to stat 0 */
	ASSERT_EQ(GetStat(&p, 0), 8);        /* 5 + 3 */
}

static void test_getstat_armor_bonus(void)
{
	struct clan c = {0};
	struct pc   p = {0};
	pc_clear_spells(&p);
	p.MyClan = &c;
	p.Attributes[0] = 4;
	p.Armor = 2;                         /* armor → Items[1] */
	c.Items[1].Attributes[0] = 2;        /* armor adds +2 to stat 0 */
	ASSERT_EQ(GetStat(&p, 0), 6);        /* 4 + 2 */
}

static void test_getstat_shield_bonus(void)
{
	struct clan c = {0};
	struct pc   p = {0};
	pc_clear_spells(&p);
	p.MyClan = &c;
	p.Attributes[0] = 7;
	p.Shield = 3;                        /* shield → Items[2] */
	c.Items[2].Attributes[0] = -1;       /* shield penalty of -1 */
	ASSERT_EQ(GetStat(&p, 0), 6);        /* 7 + (-1) */
}

/* -------------------------------------------------------------------------
 * Tests: NumMembers
 *
 * NumMembers(clan, OnlyOnesHere):
 *   false → count all non-NULL Member[] slots
 *   true  → count only those with Status == Here
 * ------------------------------------------------------------------------- */
static void test_nummembers_empty_clan(void)
{
	struct clan c = {0};
	ASSERT_EQ(NumMembers(&c, false), 0);
	ASSERT_EQ(NumMembers(&c, true),  0);
}

static void test_nummembers_all_here(void)
{
	struct clan c = {0};
	struct pc p0 = {0}; pc_clear_spells(&p0); p0.Status = Here;
	struct pc p1 = {0}; pc_clear_spells(&p1); p1.Status = Here;
	c.Member[0] = &p0;
	c.Member[1] = &p1;
	ASSERT_EQ(NumMembers(&c, false), 2);
	ASSERT_EQ(NumMembers(&c, true),  2);
}

static void test_nummembers_mixed_status(void)
{
	struct clan c = {0};
	struct pc ph = {0}; pc_clear_spells(&ph); ph.Status = Here;
	struct pc pa = {0}; pc_clear_spells(&pa); pa.Status = RanAway;
	c.Member[0] = &ph;
	c.Member[1] = &pa;
	ASSERT_EQ(NumMembers(&c, true),  1);  /* OnlyOnesHere=true → only Here */
	ASSERT_EQ(NumMembers(&c, false), 2);  /* OnlyOnesHere=false → all non-NULL */
}

static void test_nummembers_gap_in_slots(void)
{
	/* Slots 0 and 3 filled, 1 and 2 empty. */
	struct clan c = {0};
	struct pc p0 = {0}; pc_clear_spells(&p0); p0.Status = Here;
	struct pc p3 = {0}; pc_clear_spells(&p3); p3.Status = Here;
	c.Member[0] = &p0;
	c.Member[3] = &p3;
	ASSERT_EQ(NumMembers(&c, false), 2);
	ASSERT_EQ(NumMembers(&c, true),  2);
}

/* -------------------------------------------------------------------------
 * main
 * ------------------------------------------------------------------------- */
int main(void)
{
	RUN(padstring_plain);
	RUN(padstring_no_padding_needed);
	RUN(padstring_empty_string);
	RUN(padstring_pipe_codes);
	RUN(getstat_base_only);
	RUN(getstat_weapon_bonus);
	RUN(getstat_armor_bonus);
	RUN(getstat_shield_bonus);
	RUN(nummembers_empty_clan);
	RUN(nummembers_all_here);
	RUN(nummembers_mixed_status);
	RUN(nummembers_gap_in_slots);

	printf("\n%d/%d passed\n", g_tests_run - g_tests_failed, g_tests_run);
	return g_tests_failed ? 1 : 0;
}
