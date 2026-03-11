/*
 * Unit tests for src/console.c
 * Uses the "include the C file" pattern -- no changes to existing sources.
 */

/*
 * Pre-include headers so their include guards fire before our mocks
 * are defined.  console.c defines System_Error, so we cannot replace it
 * with a function-like macro the way other test files do.  Instead we
 * intercept exit() -- the common final step of both System_Error and
 * CheckMem -- so ASSERT_FATAL works for both.
 */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../defines.h"
#include "../video.h"
#include "../console.h"
#include "../platform.h"

#include "test_harness.h"

static jmp_buf g_fatal_jmp;

#define exit(code) longjmp(g_fatal_jmp, 1)   /* NOLINT */

#include "../console.c"

#undef exit

/* =========================================================================
 * External variable definitions: video.h and defines.h externs
 * ========================================================================= */
int   ScreenWidth       = 80;
int   ScreenLines       = 24;
int   _argc             = 0;
static char *_argv_buf[] = {NULL};
char **_argv            = _argv_buf;

/* -------------------------------------------------------------------------
 * cio_getch mock: callers prime a key sequence; each call consumes one.
 * ------------------------------------------------------------------------- */
static int g_keys[16];
static int g_key_count;
static int g_key_pos;

static void set_keys(const int *keys, int n)
{
	int i;
	for (i = 0; i < n; i++) g_keys[i] = keys[i];
	g_key_count = n;
	g_key_pos   = 0;
}

int cio_getch(void)
{
	if (g_key_pos < g_key_count) return g_keys[g_key_pos++];
	return 0;
}

/* -------------------------------------------------------------------------
 * Output stubs
 * ------------------------------------------------------------------------- */
static int  g_zputs_calls;
static char g_zputs_last[256];

void zputs(const char *s)
{
	g_zputs_calls++;
	if (s) snprintf(g_zputs_last, sizeof(g_zputs_last), "%s", s);
}

static int  g_putch_count;
static char g_putch_chars[64];

void clans_putch(unsigned char ch)
{
	if (g_putch_count < (int)(sizeof(g_putch_chars) - 1))
		g_putch_chars[g_putch_count] = (char)ch;
	g_putch_count++;
}

static int g_display_calls;
void DisplayStr(const char *s)  { (void)s; g_display_calls++; }
void ShowTextCursor(bool sh)    { (void)sh; }
void Video_Close(void)          {}
void delay(unsigned msec)       { (void)msec; }

/* -------------------------------------------------------------------------
 * Per-test setup: reset all counters and key queue.
 * ------------------------------------------------------------------------- */
static void setup(void)
{
	g_zputs_calls   = 0;
	g_display_calls = 0;
	g_key_count     = 0;
	g_key_pos       = 0;
	g_putch_count   = 0;
	memset(g_zputs_last,  0, sizeof(g_zputs_last));
	memset(g_putch_chars, 0, sizeof(g_putch_chars));
}

/* -------------------------------------------------------------------------
 * Tests: trivial constant-return functions
 * ------------------------------------------------------------------------- */
static void test_door_initialized_is_false(void)
{
	ASSERT_EQ(Door_Initialized(), false);
}

static void test_get_hours_left(void)
{
	ASSERT_EQ(GetHoursLeft(), 23);
}

static void test_get_minutes_left(void)
{
	ASSERT_EQ(GetMinutesLeft(), 59);
}

static void test_get_key_no_wait(void)
{
	ASSERT_EQ(GetKeyNoWait(), 0);
}

/* -------------------------------------------------------------------------
 * Tests: CheckMem
 * ------------------------------------------------------------------------- */
static void test_checkmem_nonnull_no_abort(void)
{
	/* Non-NULL must return silently without calling exit. */
	CheckMem((void *)1);
	ASSERT_EQ(1, 1);   /* reached only if no exit */
}

static void test_checkmem_null_exits(void)
{
	ASSERT_FATAL(CheckMem(NULL));
}

/* -------------------------------------------------------------------------
 * Tests: System_Error
 * ------------------------------------------------------------------------- */
static void test_system_error_exits(void)
{
	char msg[] = "test error";
	ASSERT_FATAL(System_Error(msg));
}

/* -------------------------------------------------------------------------
 * Tests: GetKey
 * ------------------------------------------------------------------------- */
static void test_getkey_returns_char(void)
{
	const int k[] = {'A'};
	set_keys(k, 1);
	ASSERT_EQ(GetKey(), 'A');
}

static void test_getkey_negative_returns_zero(void)
{
	const int k[] = {-1};
	set_keys(k, 1);
	ASSERT_EQ(GetKey(), 0);
}

/* -------------------------------------------------------------------------
 * Tests: GetAnswer
 * ------------------------------------------------------------------------- */
static void test_getanswer_first_valid(void)
{
	const int k[] = {'Y'};
	set_keys(k, 1);
	ASSERT_EQ(GetAnswer("YN"), 'Y');
}

static void test_getanswer_lowercase_uppercased(void)
{
	/* Input is lowercase; result must be uppercase. */
	const int k[] = {'y'};
	set_keys(k, 1);
	ASSERT_EQ(GetAnswer("YN"), 'Y');
}

static void test_getanswer_invalid_then_valid(void)
{
	/* 'X' is not in "YN" so it is ignored; 'n' matches case-insensitively. */
	const int k[] = {'X', 'n'};
	set_keys(k, 2);
	ASSERT_EQ(GetAnswer("YN"), 'N');
}

static void test_getanswer_extended_key_0x00_skipped(void)
{
	/* 0x00 prefix signals a two-byte scan code; both bytes are discarded. */
	const int k[] = {0x00, 72, 'Y'};
	set_keys(k, 3);
	ASSERT_EQ(GetAnswer("Y"), 'Y');
}

static void test_getanswer_extended_key_0xe0_skipped(void)
{
	const int k[] = {0xE0, 80, 'N'};
	set_keys(k, 3);
	ASSERT_EQ(GetAnswer("N"), 'N');
}

static void test_getanswer_enter_allowed(void)
{
	/* '\r' must be accepted when it appears in the allowable set. */
	const int k[] = {'\r'};
	set_keys(k, 1);
	ASSERT_EQ(GetAnswer("YN\r\n"), '\r');
}

/* -------------------------------------------------------------------------
 * Tests: YesNo / NoYes
 * ------------------------------------------------------------------------- */
static void test_yesno_y_returns_true(void)
{
	const int k[] = {'y'};
	set_keys(k, 1);
	ASSERT_EQ(YesNo(""), true);
}

static void test_yesno_n_returns_false(void)
{
	const int k[] = {'n'};
	set_keys(k, 1);
	ASSERT_EQ(YesNo(""), false);
}

static void test_yesno_enter_returns_default_true(void)
{
	/* Enter (neither Y nor N) → returns the YesNo default of true. */
	const int k[] = {'\r'};
	set_keys(k, 1);
	ASSERT_EQ(YesNo(""), true);
}

static void test_noyes_y_returns_true(void)
{
	const int k[] = {'y'};
	set_keys(k, 1);
	ASSERT_EQ(NoYes(""), true);
}

static void test_noyes_n_returns_false(void)
{
	const int k[] = {'n'};
	set_keys(k, 1);
	ASSERT_EQ(NoYes(""), false);
}

static void test_noyes_enter_returns_default_false(void)
{
	/* Enter → returns the NoYes default of false. */
	const int k[] = {'\r'};
	set_keys(k, 1);
	ASSERT_EQ(NoYes(""), false);
}

/* -------------------------------------------------------------------------
 * Tests: rputs
 * ------------------------------------------------------------------------- */
static void test_rputs_delegates_to_zputs(void)
{
	rputs("hello");
	ASSERT_EQ(g_zputs_calls, 1);
	ASSERT_EQ(strcmp(g_zputs_last, "hello"), 0);
}

/* -------------------------------------------------------------------------
 * Tests: PutCh
 * ------------------------------------------------------------------------- */
static void test_putch_single_char(void)
{
	PutCh('A');
	ASSERT_EQ(g_zputs_calls, 1);
	ASSERT_EQ((int)g_zputs_last[0], 'A');
	ASSERT_EQ((int)g_zputs_last[1], '\0');
}

/* -------------------------------------------------------------------------
 * Tests: rawputs
 * ------------------------------------------------------------------------- */
static void test_rawputs_sends_each_char(void)
{
	rawputs("abc");
	ASSERT_EQ(g_putch_count, 3);
	ASSERT_EQ((int)g_putch_chars[0], 'a');
	ASSERT_EQ((int)g_putch_chars[1], 'b');
	ASSERT_EQ((int)g_putch_chars[2], 'c');
}

static void test_rawputs_empty_string(void)
{
	rawputs("");
	ASSERT_EQ(g_putch_count, 0);
}

/* -------------------------------------------------------------------------
 * Tests: LogDisplayStr
 * ------------------------------------------------------------------------- */
static void test_logdisplaystr_calls_displaystr(void)
{
	LogDisplayStr("msg");
	ASSERT_EQ(g_display_calls, 1);
}

/* -------------------------------------------------------------------------
 * Tests: Door_AllowScreenPause / Door_ToggleScreenPause
 * ------------------------------------------------------------------------- */
static void test_screen_pause_toggle(void)
{
	bool orig = Door_AllowScreenPause();
	Door_ToggleScreenPause();
	ASSERT_EQ(Door_AllowScreenPause(), !orig);
	Door_ToggleScreenPause();  /* restore for subsequent tests */
	ASSERT_EQ(Door_AllowScreenPause(), orig);
}

/* -------------------------------------------------------------------------
 * main
 * ------------------------------------------------------------------------- */
int main(void)
{
	g_test_setup = setup;

	RUN(door_initialized_is_false);
	RUN(get_hours_left);
	RUN(get_minutes_left);
	RUN(get_key_no_wait);
	RUN(checkmem_nonnull_no_abort);
	RUN(checkmem_null_exits);
	RUN(system_error_exits);
	RUN(getkey_returns_char);
	RUN(getkey_negative_returns_zero);
	RUN(getanswer_first_valid);
	RUN(getanswer_lowercase_uppercased);
	RUN(getanswer_invalid_then_valid);
	RUN(getanswer_extended_key_0x00_skipped);
	RUN(getanswer_extended_key_0xe0_skipped);
	RUN(getanswer_enter_allowed);
	RUN(yesno_y_returns_true);
	RUN(yesno_n_returns_false);
	RUN(yesno_enter_returns_default_true);
	RUN(noyes_y_returns_true);
	RUN(noyes_n_returns_false);
	RUN(noyes_enter_returns_default_false);
	RUN(rputs_delegates_to_zputs);
	RUN(putch_single_char);
	RUN(rawputs_sends_each_char);
	RUN(rawputs_empty_string);
	RUN(logdisplaystr_calls_displaystr);
	RUN(screen_pause_toggle);

	printf("\n%d/%d passed\n", g_tests_run - g_tests_failed, g_tests_run);
	return g_tests_failed ? 1 : 0;
}
