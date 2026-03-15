/*
 * Unit tests for src/maint.c
 * Uses the "include the C file" pattern -- no changes to existing sources.
 */

/*
 * Pre-include project headers whose include guards must fire before we
 * define the System_Error mock.  system.h declares the noreturn prototype.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../defines.h"
#include "../structs.h"   /* needed for System struct definition */
#include "../system.h"

#include "test_harness.h"

/* Define jmp_buf and base externs needed for System_Error macro */
static jmp_buf g_fatal_jmp;
struct system System;
struct game Game;
bool Verbose = false;
int _argc = 0;
static char *_argv_buf[] = {NULL};
char **_argv = _argv_buf;

#define System_Error(msg) longjmp(g_fatal_jmp, 1)   /* NOLINT */

#include "mocks_platform.h"
#include "../maint.c"

#undef System_Error

/* Note: test_maint.c defines its own stubs with call counters, not using mocks. */

/* -------------------------------------------------------------------------
 * Call-counter state
 *
 * Each stub increments its counter so tests can assert how many times
 * Maintenance() invoked each subsystem.
 * ------------------------------------------------------------------------- */
static int32_t g_days_between_result;

static int g_log_calls;
static int g_system_maint_calls;
static int g_village_maint_calls;
static int g_ibbs_maint_calls;
static int g_user_maint_calls;
static int g_mail_maint_calls;
static int g_npc_maint_calls;
static int g_trades_maint_calls;
static int g_ps_maint_calls;
static int g_alliance_maint_calls;
static int g_ibbs_send_queued_calls;
static int g_ibbs_handle_queued_calls;
static int g_game_write_calls;

/* -------------------------------------------------------------------------
 * Stubs
 * ------------------------------------------------------------------------- */
int32_t DaysBetween(char a[], char b[])  { (void)a; (void)b; return g_days_between_result; }
void    LogDisplayStr(const char *s)     { (void)s; g_log_calls++; }
void    System_Maint(void)               { g_system_maint_calls++; }
void    Village_Maint(void)              { g_village_maint_calls++; }
void    IBBS_Maint(void)                 { g_ibbs_maint_calls++; }
void    User_Maint(void)                 { g_user_maint_calls++; }
void    Mail_Maint(void)                 { g_mail_maint_calls++; }
void    NPC_Maint(void)                  { g_npc_maint_calls++; }
void    Trades_Maint(void)               { g_trades_maint_calls++; }
void    PS_Maint(void)                   { g_ps_maint_calls++; }
void    Alliance_Maint(void)             { g_alliance_maint_calls++; }
void    IBBS_SendQueuedResults(void)     { g_ibbs_send_queued_calls++; }
void    IBBS_HandleQueuedPackets(void)   { g_ibbs_handle_queued_calls++; }
void    Game_Write(void)                 { g_game_write_calls++; }

/* -------------------------------------------------------------------------
 * Per-test setup: establish a clean, known state before every test.
 * ------------------------------------------------------------------------- */
static void setup(void)
{
	memset(&Game,   0, sizeof(Game));
	memset(&System, 0, sizeof(System));

	/* Default: maint has not yet run today → DaysBetween > 0. */
	g_days_between_result = 1;

	Game.Data.GameState = 0;     /* game is in progress */
	Game.Data.InterBBS  = false;

	strcpy(System.szTodaysDate,      "2026-03-08");
	strcpy(Game.Data.szTodaysDate,   "2026-03-07");

	g_log_calls              = 0;
	g_system_maint_calls     = 0;
	g_village_maint_calls    = 0;
	g_ibbs_maint_calls       = 0;
	g_user_maint_calls       = 0;
	g_mail_maint_calls       = 0;
	g_npc_maint_calls        = 0;
	g_trades_maint_calls     = 0;
	g_ps_maint_calls         = 0;
	g_alliance_maint_calls   = 0;
	g_ibbs_send_queued_calls = 0;
	g_ibbs_handle_queued_calls = 0;
	g_game_write_calls       = 0;
}

/* -------------------------------------------------------------------------
 * Tests: IBBS double-run guard
 * ------------------------------------------------------------------------- */
static void test_ibbs_double_run_errors(void)
{
	/* DaysBetween <= 0 with InterBBS enabled → System_Error must fire. */
	Game.Data.InterBBS    = true;
	g_days_between_result = 0;
	ASSERT_FATAL(Maintenance());

	/* No subsystem maintenance should have been reached. */
	ASSERT_EQ(g_system_maint_calls, 0);
	ASSERT_EQ(g_game_write_calls,   0);
}

static void test_ibbs_negative_days_also_errors(void)
{
	Game.Data.InterBBS    = true;
	g_days_between_result = -5;
	ASSERT_FATAL(Maintenance());
	ASSERT_EQ(g_system_maint_calls, 0);
}

static void test_non_ibbs_runs_despite_same_date(void)
{
	/*
	 * When InterBBS is false the double-run guard is skipped entirely
	 * even if DaysBetween returns 0.  Maintenance should complete.
	 */
	Game.Data.InterBBS    = false;
	g_days_between_result = 0;
	Maintenance();
	ASSERT_EQ(g_system_maint_calls, 1);
	ASSERT_EQ(g_game_write_calls,   1);
}

/* -------------------------------------------------------------------------
 * Tests: game-not-started early return (GameState 1 and 2)
 * ------------------------------------------------------------------------- */
static void test_game_inactive_state1(void)
{
	Game.Data.GameState = 1;
	Maintenance();

	/* Date must be updated even when maintenance is skipped. */
	ASSERT_EQ(strcmp(Game.Data.szTodaysDate, System.szTodaysDate), 0);

	/* Subsystem maintenance must not have run. */
	ASSERT_EQ(g_system_maint_calls, 0);
	ASSERT_EQ(g_game_write_calls,   0);
}

static void test_game_inactive_state2(void)
{
	Game.Data.GameState = 2;
	Maintenance();

	ASSERT_EQ(strcmp(Game.Data.szTodaysDate, System.szTodaysDate), 0);
	ASSERT_EQ(g_system_maint_calls, 0);
	ASSERT_EQ(g_game_write_calls,   0);
}

/* -------------------------------------------------------------------------
 * Tests: full maintenance run
 * ------------------------------------------------------------------------- */
static void test_normal_run_updates_date(void)
{
	Maintenance();
	ASSERT_EQ(strcmp(Game.Data.szTodaysDate, System.szTodaysDate), 0);
}

static void test_normal_run_calls_all_subsystems(void)
{
	Maintenance();

	ASSERT_EQ(g_system_maint_calls,      1);
	ASSERT_EQ(g_village_maint_calls,     1);
	ASSERT_EQ(g_ibbs_maint_calls,        1);
	ASSERT_EQ(g_user_maint_calls,        1);
	ASSERT_EQ(g_mail_maint_calls,        1);
	ASSERT_EQ(g_npc_maint_calls,         1);
	ASSERT_EQ(g_trades_maint_calls,      1);
	ASSERT_EQ(g_ps_maint_calls,          1);
	ASSERT_EQ(g_alliance_maint_calls,    1);
	ASSERT_EQ(g_ibbs_send_queued_calls,  1);
	ASSERT_EQ(g_ibbs_handle_queued_calls,1);
	ASSERT_EQ(g_game_write_calls,        1);
}

/* -------------------------------------------------------------------------
 * main
 * ------------------------------------------------------------------------- */
int main(void)
{
	g_test_setup = setup;

	RUN(ibbs_double_run_errors);
	RUN(ibbs_negative_days_also_errors);
	RUN(non_ibbs_runs_despite_same_date);
	RUN(game_inactive_state1);
	RUN(game_inactive_state2);
	RUN(normal_run_updates_date);
	RUN(normal_run_calls_all_subsystems);

	printf("\n%d/%d passed\n", g_tests_run - g_tests_failed, g_tests_run);
	return g_tests_failed ? 1 : 0;
}
