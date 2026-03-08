/*
 * Unit tests for src/ibbs.c
 * Uses the "include the C file" pattern -- no changes to existing sources.
 *
 * NOTE: ibbs.c defines struct ibbs IBBS and aszShortMonthName at file scope,
 * as well as all IBBS_* functions, VillageName, and BBSName.  Do NOT define
 * or stub any of those here.
 *
 * Targeted functions (all accessible via the #include pattern):
 *   VillageName  – maps a BBS ID to its village name string
 *   BBSName      – maps a BBS ID to its BBS name string
 *   CheckID      – static; validates a BBS ID and checks Active flag
 *   (CheckSourceID and CheckDestinationID are thin wrappers; tested here too)
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
#include "../empire.h"
#include "../fight.h"
#include "../game.h"
#include "../help.h"
#include "../ibbs.h"
#include "../input.h"
#include "../mail.h"
#include "../misc.h"
#include "../mstrings.h"
#include "../myibbs.h"
#include "../myopen.h"
#include "../news.h"
#include "../parsing.h"
#include "../readcfg.h"
#include "../scores.h"
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
 * video.h externs + stubs
 * ibbs.c calls zputs() and DisplayStr() directly.
 * ------------------------------------------------------------------------- */
int ScreenWidth  = 80;
int ScreenLines  = 24;

void DisplayStr(const char *s)        { (void)s; }
void zputs(const char *s)             { (void)s; }

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
 * empire.h stubs + BuildingType definition
 * ibbs.c calls ArmySpeed/Offense/Defense/Vitality, ProcessAttackPacket,
 * ProcessResultPacket, Empire_Stats/Maint/Create/Manage, DonateToEmpire,
 * and references BuildingType[].
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
 * user.h externs + stubs
 * PClan is defined here (user.c is not included in this test file).
 * ibbs.c calls GetClan, Clan_Update, DeleteClan.  Other User_* functions
 * are declared in user.h but not called by the paths we exercise here.
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
void DeleteClan(int16_t id[2], char *n, bool e)   { (void)id; (void)n; (void)e; }
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
 * village.h externs + stubs
 * ibbs.c calls Village_Reset.
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
 * alliance.h externs + stubs
 * KillAlliances is declared in ibbs.h and defined in alliance.c — stub here.
 * ------------------------------------------------------------------------- */
struct Alliance *Alliances[MAX_ALLIANCES];
int16_t NumAlliances = 0;

void DeleteAlliance(int i)                        { (void)i; }
void Alliances_Init(void)                         {}
void Alliances_Close(void)                        {}
void KillAlliances(void)                          {}

/* -------------------------------------------------------------------------
 * scores.h stubs
 * ibbs.c calls ProcessScoreData, CreateScoreData, SendScoreList.
 * ------------------------------------------------------------------------- */
void DisplayScores(bool m)                                          { (void)m; }
void CreateScoreData(bool l)                                        { (void)l; }
void SendScoreList(void)                                            {}
void LeagueScores(void)                                             {}
void ProcessScoreData(struct UserScore **u)                         { (void)u; }
void RemoveFromIPScores(const int16_t id[2])                        { (void)id; }

/* -------------------------------------------------------------------------
 * news.h stubs
 * ------------------------------------------------------------------------- */
void News_AddNews(char *s)                        { (void)s; }
void News_ReadNews(bool t)                        { (void)t; }
void News_CreateTodayNews(void)                   {}

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
 * Include the source files under test.
 * platform.c brings unix_wrappers.c; random.c provides my_random; misc.c and
 * parsing.c provide string utilities used by ibbs.c; serialize/deserialize/
 * myopen provide the file I/O layer; myibbs.c provides ConvertStringToAddress;
 * ibbs.c is last — it defines IBBS, aszShortMonthName, VillageName, BBSName,
 * and all IBBS_* functions.
 * ------------------------------------------------------------------------- */
#include "../platform.c"
#include "../random.c"
#include "../serialize.c"
#include "../deserialize.c"
#include "../misc.c"
#include "../parsing.c"
#include "../myopen.c"
#include "../myibbs.c"
#include "../ibbs.c"

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
 * Tests: VillageName
 *
 * VillageName(BBSID):
 *   BBSID < 1 || BBSID > MAX_IBBSNODES → "<invalid>"
 *   pszVillageName == NULL             → "<defunct>"
 *   otherwise                          → pszVillageName
 * ------------------------------------------------------------------------- */
static void test_villagename_invalid_low(void)
{
	/* BBSID=0 is below the valid range */
	ASSERT_EQ(strcmp(VillageName(0), "<invalid>"), 0);
}

static void test_villagename_invalid_high(void)
{
	/* BBSID=MAX_IBBSNODES+1 is above the valid range */
	ASSERT_EQ(strcmp(VillageName(MAX_IBBSNODES + 1), "<invalid>"), 0);
}

static void test_villagename_null(void)
{
	/* BBSID=1, pszVillageName=NULL (zero-initialised) → "<defunct>" */
	memset(&IBBS, 0, sizeof(IBBS));
	ASSERT_EQ(strcmp(VillageName(1), "<defunct>"), 0);
}

static void test_villagename_named(void)
{
	/* BBSID=1 with a non-NULL name → returns that name */
	memset(&IBBS, 0, sizeof(IBBS));
	IBBS.Data.Nodes[0].Info.pszVillageName = "Stonehaven";
	ASSERT_EQ(strcmp(VillageName(1), "Stonehaven"), 0);
}

/* -------------------------------------------------------------------------
 * Tests: BBSName
 *
 * Same sentinel logic as VillageName, applied to pszBBSName.
 * ------------------------------------------------------------------------- */
static void test_bbsname_invalid_low(void)
{
	ASSERT_EQ(strcmp(BBSName(0), "<invalid>"), 0);
}

static void test_bbsname_null(void)
{
	memset(&IBBS, 0, sizeof(IBBS));
	ASSERT_EQ(strcmp(BBSName(2), "<defunct>"), 0);
}

static void test_bbsname_named(void)
{
	memset(&IBBS, 0, sizeof(IBBS));
	IBBS.Data.Nodes[1].Info.pszBBSName = "TestBBS";
	ASSERT_EQ(strcmp(BBSName(2), "TestBBS"), 0);
}

/* -------------------------------------------------------------------------
 * Tests: CheckID (static, tested via CheckSourceID / CheckDestinationID)
 *
 * CheckID(ID, name):
 *   ID < 1 || ID > MAX_IBBSNODES → 0 (false)
 *   !IBBS.Data.Nodes[ID-1].Active → 0
 *   otherwise                     → (int16_t)ID
 * ------------------------------------------------------------------------- */
static void test_checkid_invalid_low(void)
{
	/* ID=0 is outside the valid BBS ID range */
	memset(&IBBS, 0, sizeof(IBBS));
	ASSERT_EQ(CheckSourceID(0), 0);
}

static void test_checkid_invalid_high(void)
{
	/* ID=MAX_IBBSNODES+1 is outside the valid BBS ID range */
	memset(&IBBS, 0, sizeof(IBBS));
	ASSERT_EQ(CheckSourceID(MAX_IBBSNODES + 1), 0);
}

static void test_checkid_inactive(void)
{
	/* Valid ID but node is not Active → 0 */
	memset(&IBBS, 0, sizeof(IBBS));
	IBBS.Data.Nodes[0].Active = false;
	ASSERT_EQ(CheckSourceID(1), 0);
}

static void test_checkid_active(void)
{
	/* Valid ID and Active=true → returns the ID */
	memset(&IBBS, 0, sizeof(IBBS));
	IBBS.Data.Nodes[0].Active = true;
	ASSERT_EQ(CheckSourceID(1), 1);
}

static void test_checkdestid_inactive(void)
{
	/* CheckDestinationID wraps CheckID with a different label string */
	memset(&IBBS, 0, sizeof(IBBS));
	IBBS.Data.Nodes[3].Active = false;
	ASSERT_EQ(CheckDestinationID(4), 0);
}

static void test_checkdestid_active(void)
{
	memset(&IBBS, 0, sizeof(IBBS));
	IBBS.Data.Nodes[3].Active = true;
	ASSERT_EQ(CheckDestinationID(4), 4);
}

/* -------------------------------------------------------------------------
 * main
 * ------------------------------------------------------------------------- */
int main(void)
{
	RUN(villagename_invalid_low);
	RUN(villagename_invalid_high);
	RUN(villagename_null);
	RUN(villagename_named);
	RUN(bbsname_invalid_low);
	RUN(bbsname_null);
	RUN(bbsname_named);
	RUN(checkid_invalid_low);
	RUN(checkid_invalid_high);
	RUN(checkid_inactive);
	RUN(checkid_active);
	RUN(checkdestid_inactive);
	RUN(checkdestid_active);

	printf("\n%d/%d passed\n", g_tests_run - g_tests_failed, g_tests_run);
	return g_tests_failed ? 1 : 0;
}
