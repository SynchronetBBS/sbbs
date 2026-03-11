/*
 * Unit tests for src/scores.c
 * Uses the "include the C file" pattern -- no changes to existing sources.
 *
 * Targets the two pure string-transformation functions:
 *   GetColourString – maps a colour index to an ANSI escape sequence.
 *   PipeToAnsi      – converts pipe/backtick colour codes to ANSI sequences.
 * Both are static; they are directly accessible because we #include scores.c.
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
#include "../readcfg.h"
#include "../ibbs.h"
#include "../village.h"
#include "../user.h"
#include "../fight.h"
#include "../game.h"
#include "../misc.h"
#include "../mstrings.h"

#include "test_harness.h"

/* -------------------------------------------------------------------------
 * System_Error / CheckMem / System_Close — forward-declared as noreturn.
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
 * door.h stubs (scores.c calls rputs and door_pause in display paths)
 * ------------------------------------------------------------------------- */
void rputs(const char *s)           { (void)s; }
void LogDisplayStr(const char *s)   { (void)s; }
void LogStr(const char *s)          { (void)s; }
char GetKey(void)                   { return 0; }
char GetKeyNoWait(void)             { return 0; }
char GetAnswer(const char *s)       { (void)s; return 0; }
void door_pause(void)               {}
void Display(char *f)               { (void)f; }
bool YesNo(char *s)                 { (void)s; return false; }
bool NoYes(char *s)                 { (void)s; return false; }
bool Door_AllowScreenPause(void)    { return false; }
void Door_ToggleScreenPause(void)   {}
void Door_ShowTitle(void)           {}
void InputCallback(void)            {}
void PutCh(char c)                  { (void)c; }
void rawputs(const char *s)         { (void)s; }
int16_t GetHoursLeft(void)          { return 0; }
int16_t GetMinutesLeft(void)        { return 0; }
void Door_Init(bool local)          { (void)local; }
void Door_Close(void)               {}
bool Door_Initialized(void)         { return false; }
void Door_SetColorScheme(int8_t *s) { (void)s; }

/* -------------------------------------------------------------------------
 * video.h externs
 * ------------------------------------------------------------------------- */
int ScreenWidth  = 80;
int ScreenLines  = 24;

/* -------------------------------------------------------------------------
 * readcfg.h stubs
 * ------------------------------------------------------------------------- */
struct config Config;
void AddInboundDir(const char *d)                                    { (void)d; }
bool Config_Init(uint16_t n, struct NodeData *(*f)(int))              { (void)n; (void)f; return false; }
void Config_Close(void)                                               {}

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
const char *VillageName(int16_t id)               { (void)id; return "?"; }

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
 * user.h externs + stubs
 * scores.c calls FreeClanMembers and NumMembers (both fight.h / user.h).
 * ------------------------------------------------------------------------- */
struct clan PClan;

int16_t NumMembers(struct clan *c, bool here)
{
	int16_t cnt = 0;
	for (int i = 0; i < MAX_MEMBERS; i++)
		if (c->Member[i] && (!here || c->Member[i]->Status == Here))
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
int8_t GetStat(struct pc *p, char s)              { return p->Attributes[(int)s]; }
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
 * fight.h stubs (FreeClanMembers is called from scores.c:DisplayScores)
 * ------------------------------------------------------------------------- */
void FreeClanMembers(struct clan *c)
{
	for (int i = 0; i < MAX_MEMBERS; i++) {
		free(c->Member[i]);
		c->Member[i] = NULL;
	}
}
void FreeClan(struct clan *c)       { FreeClanMembers(c); }
void InitClan(struct clan *c)       { memset(c, 0, sizeof(*c)); }
void Fight_CheckLevelUp(void)       {}
int16_t Fight_Fight(struct clan *a, struct clan *d, bool he, bool cr, bool af)
	{ (void)a; (void)d; (void)he; (void)cr; (void)af; return 0; }
void Fight_Clan(void)               {}
void Fight_Heal(struct clan *c)     { (void)c; }
void Fight_Monster(int16_t l, char *f){ (void)l; (void)f; }

/* -------------------------------------------------------------------------
 * game.h stubs
 * ------------------------------------------------------------------------- */
struct game Game;
void Game_Init(void)                {}
void Game_Close(void)               {}
void Game_Settings(void)            {}
void Game_Start(void)               {}
void Game_Write(void)               {}

/* -------------------------------------------------------------------------
 * language.h stubs
 * ------------------------------------------------------------------------- */
static struct Language g_lang;
struct Language *Language = &g_lang;

void Language_Init(char *f)         { (void)f; }
void Language_Close(void)           {}
void LoadStrings(int16_t s, int16_t n, char *arr[])
{
	for (int16_t i = 0; i < n; i++) arr[i] = "";
}

/* -------------------------------------------------------------------------
 * Include the source files under test.
 * ------------------------------------------------------------------------- */
#include "../platform.c"
#include "../misc.c"
#include "../serialize.c"
#include "../deserialize.c"
#include "../myopen.c"
#include "../scores.c"

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
 * Tests: GetColourString
 *
 * char[20] output buffer — no truncation because sizeof() is the actual size.
 * Colour ranges (from the source):
 *   0–7:   regular foreground
 *   8–15:  bright foreground (adds "1;" prefix)
 *   16–23: background
 *   24–31: background + flash (adds "5;" prefix)
 * ------------------------------------------------------------------------- */
static void test_colour_regular(void)
{
	/* Colour 0, Clear=false → ESC[30m */
	char buf[20];
	GetColourString(buf, 0, false);
	ASSERT_EQ(strcmp(buf, "\x1B[30m"), 0);
}

static void test_colour_regular_clear(void)
{
	/* Colour 7 (white), Clear=true → ESC[0;37m */
	char buf[20];
	GetColourString(buf, 7, true);
	ASSERT_EQ(strcmp(buf, "\x1B[0;37m"), 0);
}

static void test_colour_bright(void)
{
	/* Colour 8 (bright black), Clear=false → ESC[1;30m */
	char buf[20];
	GetColourString(buf, 8, false);
	ASSERT_EQ(strcmp(buf, "\x1B[1;30m"), 0);
}

static void test_colour_background(void)
{
	/* Colour 16 (bg black), Clear=false → ESC[40m */
	char buf[20];
	GetColourString(buf, 16, false);
	ASSERT_EQ(strcmp(buf, "\x1B[40m"), 0);
}

static void test_colour_background_flash(void)
{
	/* Colour 24 (bg black + flash), Clear=false → ESC[5;40m */
	char buf[20];
	GetColourString(buf, 24, false);
	ASSERT_EQ(strcmp(buf, "\x1B[5;40m"), 0);
}

/* -------------------------------------------------------------------------
 * Tests: PipeToAnsi
 *
 * NOTE: PipeToAnsi contains a known bug: it passes sizeof(pcOut) (= 8 on
 * 64-bit) as the size to strlcat where pcOut is a char*.  This limits each
 * ANSI escape chunk to ≤ 7 bytes.  The tests below only verify the
 * well-defined, non-truncated cases.
 * ------------------------------------------------------------------------- */
static void test_pipe_plain_text(void)
{
	/* No colour codes — text passes through unchanged. */
	char out[64];
	PipeToAnsi(out, "hello world");
	ASSERT_EQ(strcmp(out, "hello world"), 0);
}

static void test_pipe_empty(void)
{
	char out[64];
	PipeToAnsi(out, "");
	ASSERT_EQ(out[0], '\0');
}

static void test_pipe_text_after_code(void)
{
	/* |07 is white-on-default with clear; output must end with "hi". */
	char out[64];
	PipeToAnsi(out, "|07hi");
	/* Verify the plain-text tail. */
	size_t len = strlen(out);
	ASSERT_EQ(len >= 2, 1);
	ASSERT_EQ(strcmp(out + len - 2, "hi"), 0);
}

static void test_pipe_no_code_preserved(void)
{
	/* A lone '|' not followed by two digits is passed through literally. */
	char out[64];
	PipeToAnsi(out, "a|b");
	ASSERT_EQ(strcmp(out, "a|b"), 0);
}

/* -------------------------------------------------------------------------
 * main
 * ------------------------------------------------------------------------- */
int main(void)
{
	RUN(colour_regular);
	RUN(colour_regular_clear);
	RUN(colour_bright);
	RUN(colour_background);
	RUN(colour_background_flash);
	RUN(pipe_plain_text);
	RUN(pipe_empty);
	RUN(pipe_text_after_code);
	RUN(pipe_no_code_preserved);

	printf("\n%d/%d passed\n", g_tests_run - g_tests_failed, g_tests_run);
	return g_tests_failed ? 1 : 0;
}
