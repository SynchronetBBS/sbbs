/*
 * Unit tests for src/empire.c
 * Uses the "include the C file" pattern -- no changes to existing sources.
 *
 * Targets four pure Army computation functions (all simple arithmetic):
 *   ArmySpeed    – weighted-average travel speed of a mixed army
 *   ArmyOffense  – total offensive strength
 *   ArmyDefense  – total defensive strength
 *   ArmyVitality – total vitality (result is doubled)
 * and the static duplicate-packet detector:
 *   ValidateIndex – signed-wrap-safe "have we seen this attack index?" check
 *
 * The stat constants (SPD_*, OFF_*, DEF_*, VIT_*) are #define'd at the top
 * of empire.c and become visible in this translation unit via the
 * #include pattern.
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
#include "../fight.h"
#include "../game.h"
#include "../help.h"
#include "../ibbs.h"
#include "../input.h"
#include "../mail.h"
#include "../mstrings.h"
#include "../myopen.h"
#include "../news.h"
#include "../readcfg.h"
#include "../user.h"
#include "../village.h"

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
 * OpenDoor.h stub: od_clr_scr() is called in Empire_Stats and other display
 * functions that we don't exercise here.
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
 * PClan is defined here; GetStat and NumMembers are real implementations
 * because they may be called from empire.c helper code paths.
 * ------------------------------------------------------------------------- */
struct clan PClan;

int8_t GetStat(struct pc *p, char s)  { return p->Attributes[(int)s]; }

int16_t NumMembers(struct clan *c, bool h)
{
	int16_t n = 0;
	for (int i = 0; i < MAX_MEMBERS; i++)
		if (c->Member[i] && (!h || c->Member[i]->Status == Here))
			n++;
	return n;
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

/* -------------------------------------------------------------------------
 * ibbs.h externs + stubs
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
 * alliance.h externs + stubs
 * ------------------------------------------------------------------------- */
struct Alliance *Alliances[MAX_ALLIANCES];
int16_t NumAlliances = 0;

void DeleteAlliance(int i)                        { (void)i; }
void Alliances_Init(void)                         {}
void Alliances_Close(void)                        {}
void KillAlliances(void)                          {}

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
 * platform.c brings unix_wrappers.c; random.c provides my_random (called
 * inside empire.c's combat functions); serialize/deserialize/myopen provide
 * the file I/O layer.  empire.c is last.
 * ------------------------------------------------------------------------- */
#include "../platform.c"
#include "../random.c"
#include "../serialize.c"
#include "../deserialize.c"
#include "../myopen.c"
#include "../empire.c"

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
 * Tests: ArmySpeed
 *
 * SPD_FOOTMEN=5, SPD_AXEMEN=3, SPD_KNIGHTS=7
 * Speed = (F*5 + A*3 + K*7) / (F+A+K), or 0 if no troops.
 * ------------------------------------------------------------------------- */
static void test_speed_empty(void)
{
	struct Army a = {0};
	ASSERT_EQ(ArmySpeed(&a), 0);
}

static void test_speed_footmen_only(void)
{
	struct Army a = {0};
	a.Footmen = 100;
	/* (100*5) / 100 = 5 */
	ASSERT_EQ(ArmySpeed(&a), 5);
}

static void test_speed_axemen_only(void)
{
	struct Army a = {0};
	a.Axemen = 100;
	/* (100*3) / 100 = 3 */
	ASSERT_EQ(ArmySpeed(&a), 3);
}

static void test_speed_knights_only(void)
{
	struct Army a = {0};
	a.Knights = 100;
	/* (100*7) / 100 = 7 */
	ASSERT_EQ(ArmySpeed(&a), 7);
}

static void test_speed_mixed(void)
{
	struct Army a = {0};
	a.Footmen = 60; a.Axemen = 30; a.Knights = 10;
	/* (60*5 + 30*3 + 10*7) / 100 = (300+90+70)/100 = 460/100 = 4 */
	ASSERT_EQ(ArmySpeed(&a), 4);
}

/* -------------------------------------------------------------------------
 * Tests: ArmyOffense
 *
 * OFF_FOOTMEN=6, OFF_AXEMEN=8, OFF_KNIGHTS=9
 * Offense = F*6 + A*8 + K*9
 * ------------------------------------------------------------------------- */
static void test_offense_empty(void)
{
	struct Army a = {0};
	ASSERT_EQ(ArmyOffense(&a), 0);
}

static void test_offense_footmen(void)
{
	struct Army a = {0};
	a.Footmen = 10;
	ASSERT_EQ(ArmyOffense(&a), 60); /* 10*6 */
}

static void test_offense_axemen(void)
{
	struct Army a = {0};
	a.Axemen = 10;
	ASSERT_EQ(ArmyOffense(&a), 80); /* 10*8 */
}

static void test_offense_knights(void)
{
	struct Army a = {0};
	a.Knights = 10;
	ASSERT_EQ(ArmyOffense(&a), 90); /* 10*9 */
}

static void test_offense_mixed(void)
{
	struct Army a = {0};
	a.Footmen = 1; a.Axemen = 2; a.Knights = 3;
	/* 1*6 + 2*8 + 3*9 = 6+16+27 = 49 */
	ASSERT_EQ(ArmyOffense(&a), 49);
}

/* -------------------------------------------------------------------------
 * Tests: ArmyDefense
 *
 * DEF_FOOTMEN=2, DEF_AXEMEN=4, DEF_KNIGHTS=1
 * Defense = F*2 + A*4 + K*1
 * ------------------------------------------------------------------------- */
static void test_defense_empty(void)
{
	struct Army a = {0};
	ASSERT_EQ(ArmyDefense(&a), 0);
}

static void test_defense_footmen(void)
{
	struct Army a = {0};
	a.Footmen = 10;
	ASSERT_EQ(ArmyDefense(&a), 20); /* 10*2 */
}

static void test_defense_axemen(void)
{
	struct Army a = {0};
	a.Axemen = 10;
	ASSERT_EQ(ArmyDefense(&a), 40); /* 10*4 */
}

static void test_defense_knights(void)
{
	struct Army a = {0};
	a.Knights = 10;
	ASSERT_EQ(ArmyDefense(&a), 10); /* 10*1 */
}

static void test_defense_mixed(void)
{
	struct Army a = {0};
	a.Footmen = 1; a.Axemen = 2; a.Knights = 3;
	/* 1*2 + 2*4 + 3*1 = 2+8+3 = 13 */
	ASSERT_EQ(ArmyDefense(&a), 13);
}

/* -------------------------------------------------------------------------
 * Tests: ArmyVitality
 *
 * VIT_FOOTMEN=8, VIT_AXEMEN=7, VIT_KNIGHTS=6
 * Vitality = 2 * (F*8 + A*7 + K*6)
 * ------------------------------------------------------------------------- */
static void test_vitality_empty(void)
{
	struct Army a = {0};
	ASSERT_EQ(ArmyVitality(&a), 0);
}

static void test_vitality_footmen(void)
{
	struct Army a = {0};
	a.Footmen = 5;
	ASSERT_EQ(ArmyVitality(&a), 80); /* 2*(5*8) = 80 */
}

static void test_vitality_axemen(void)
{
	struct Army a = {0};
	a.Axemen = 5;
	ASSERT_EQ(ArmyVitality(&a), 70); /* 2*(5*7) = 70 */
}

static void test_vitality_mixed(void)
{
	struct Army a = {0};
	a.Footmen = 1; a.Axemen = 2; a.Knights = 3;
	/* 2*(1*8 + 2*7 + 3*6) = 2*(8+14+18) = 2*40 = 80 */
	ASSERT_EQ(ArmyVitality(&a), 80);
}

/* -------------------------------------------------------------------------
 * Tests: ValidateIndex  (static — accessible via #include pattern)
 *
 * ValidateIndex(fid, iidx) rejects replayed attack packets by comparing
 * iidx to the stored ReceiveIndex for BBS node fid.  On the first call
 * with ReceiveIndex=0, only iidx=0 is "old" (ridx==iidx branch).
 * A fresh node has ReceiveIndex=0 by zero-initialisation.
 * ------------------------------------------------------------------------- */
static void test_validateindex_fresh_new(void)
{
	/* Node 1: ReceiveIndex=0; incoming index 1 is new → returns true */
	memset(&IBBS, 0, sizeof(IBBS));
	ASSERT_EQ(ValidateIndex(1, 1), 1);
	/* ReceiveIndex must be updated to 1 */
	ASSERT_EQ(IBBS.Data.Nodes[0].Attack.ReceiveIndex, 1);
}

static void test_validateindex_duplicate(void)
{
	/* Same index as ReceiveIndex → duplicate, returns false */
	memset(&IBBS, 0, sizeof(IBBS));
	IBBS.Data.Nodes[0].Attack.ReceiveIndex = 5;
	ASSERT_EQ(ValidateIndex(1, 5), 0);
	/* ReceiveIndex must remain unchanged */
	ASSERT_EQ(IBBS.Data.Nodes[0].Attack.ReceiveIndex, 5);
}

static void test_validateindex_old_within_window(void)
{
	/* ridx=300, iidx=80: iidx <= ridx and iidx >= ridx-255=45 → old */
	memset(&IBBS, 0, sizeof(IBBS));
	IBBS.Data.Nodes[0].Attack.ReceiveIndex = 300;
	ASSERT_EQ(ValidateIndex(1, 80), 0);
}

static void test_validateindex_valid_advance(void)
{
	/* iidx well above ReceiveIndex → valid, ReceiveIndex updated */
	memset(&IBBS, 0, sizeof(IBBS));
	IBBS.Data.Nodes[0].Attack.ReceiveIndex = 100;
	ASSERT_EQ(ValidateIndex(1, 200), 1);
	ASSERT_EQ(IBBS.Data.Nodes[0].Attack.ReceiveIndex, 200);
}

/* -------------------------------------------------------------------------
 * main
 * ------------------------------------------------------------------------- */
int main(void)
{
	RUN(speed_empty);
	RUN(speed_footmen_only);
	RUN(speed_axemen_only);
	RUN(speed_knights_only);
	RUN(speed_mixed);
	RUN(offense_empty);
	RUN(offense_footmen);
	RUN(offense_axemen);
	RUN(offense_knights);
	RUN(offense_mixed);
	RUN(defense_empty);
	RUN(defense_footmen);
	RUN(defense_axemen);
	RUN(defense_knights);
	RUN(defense_mixed);
	RUN(vitality_empty);
	RUN(vitality_footmen);
	RUN(vitality_axemen);
	RUN(vitality_mixed);
	RUN(validateindex_fresh_new);
	RUN(validateindex_duplicate);
	RUN(validateindex_old_within_window);
	RUN(validateindex_valid_advance);

	printf("\n%d/%d passed\n", g_tests_run - g_tests_failed, g_tests_run);
	return g_tests_failed ? 1 : 0;
}
