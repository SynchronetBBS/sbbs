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

/*
 * System_Error / CheckMem / System_Close — forward-declared as noreturn.
 */
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

/* Note: test_scores.c defines its own System_Error and CheckMem above,
 * so it does NOT include mocks_system.h (which would cause redefinition). */

#include "mocks_door.h"
#include "mocks_video.h"
#include "mocks_readcfg.h"
#include "mocks_ibbs.h"
#include "mocks_village.h"
#include "mocks_game.h"
#include "mocks_language.h"

/*
 * Custom implementations for functions under test
 */
struct clan PClan;

int16_t NumMembers(struct clan *c, bool here)
{
	int16_t cnt = 0;
	for (int i = 0; i < MAX_MEMBERS; i++)
		if (c->Member[i] && (!here || c->Member[i]->Status == Here))
			cnt++;
	return cnt;
}

int8_t GetStat(struct pc *p, char s)
{
	return p->Attributes[(int)s];
}

void FreeClanMembers(struct clan *c)
{
	for (int i = 0; i < MAX_MEMBERS; i++) {
		free(c->Member[i]);
		c->Member[i] = NULL;
	}
}

#include "../platform.c"
#include "../misc.c"
#include "../serialize.c"
#include "../deserialize.c"
#include "../myopen.c"
#include "../scores.c"

#include "mocks_user.h"
#include "mocks_fight.h"
#include "mocks_quests.h"

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
	GetColourString(buf, sizeof(buf), 0, false);
	ASSERT_EQ(strcmp(buf, "\x1B[30m"), 0);
}

static void test_colour_regular_clear(void)
{
	/* Colour 7 (white), Clear=true → ESC[0;37m */
	char buf[20];
	GetColourString(buf, sizeof(buf), 7, true);
	ASSERT_EQ(strcmp(buf, "\x1B[0;37m"), 0);
}

static void test_colour_bright(void)
{
	/* Colour 8 (bright black), Clear=false → ESC[1;30m */
	char buf[20];
	GetColourString(buf, sizeof(buf), 8, false);
	ASSERT_EQ(strcmp(buf, "\x1B[1;30m"), 0);
}

static void test_colour_background(void)
{
	/* Colour 16 (bg black), Clear=false → ESC[40m */
	char buf[20];
	GetColourString(buf, sizeof(buf), 16, false);
	ASSERT_EQ(strcmp(buf, "\x1B[40m"), 0);
}

static void test_colour_background_flash(void)
{
	/* Colour 24 (bg black + flash), Clear=false → ESC[5;40m */
	char buf[20];
	GetColourString(buf, sizeof(buf), 24, false);
	ASSERT_EQ(strcmp(buf, "\x1B[5;40m"), 0);
}

/* -------------------------------------------------------------------------
 * Tests: PipeToAnsi
 * -------------------------------------------------------------------------
 */
static void test_pipe_plain_text(void)
{
	/* No colour codes — text passes through unchanged. */
	char out[64];
	PipeToAnsi(out, sizeof(out), "hello world");
	ASSERT_EQ(strcmp(out, "hello world"), 0);
}

static void test_pipe_empty(void)
{
	char out[64];
	PipeToAnsi(out, sizeof(out), "");
	ASSERT_EQ(out[0], '\0');
}

static void test_pipe_text_after_code(void)
{
	/* |07 is white-on-default with clear; should produce ESC[0;37m followed by "hi". */
	char out[64];
	PipeToAnsi(out, sizeof(out), "|07hi");
	/* Verify the full output: ANSI clear + white foreground + plain text. */
	ASSERT_EQ(strcmp(out, "\x1B[0;37mhi"), 0);
}

static void test_pipe_no_code_preserved(void)
{
	/* A lone '|' not followed by two digits is passed through literally. */
	char out[64];
	PipeToAnsi(out, sizeof(out), "a|b");
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
