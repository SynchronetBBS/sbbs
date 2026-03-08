/*
 * Unit tests for src/parsing.c
 * Uses the "include the C file" pattern -- no changes to existing sources.
 */

/*
 * Pre-include project headers so their include guards fire before the
 * C files are included below.
 */
#include <stdio.h>
#include <string.h>

#include "../defines.h"
#include "../language.h"
#include "../system.h"
#include "../platform.h"
#include "../parsing.h"

#include "test_harness.h"

#include "../platform.c"
#include "../parsing.c"

/* -------------------------------------------------------------------------
 * External variable definitions required by included headers.
 * ------------------------------------------------------------------------- */
struct system   System;
bool            Verbose         = false;
int             _argc           = 0;
static char    *_argv_buf[]     = {NULL};
char          **_argv           = _argv_buf;

/* language.h declares extern struct Language *Language. */
static struct Language g_lang;
struct Language       *Language = &g_lang;

/* -------------------------------------------------------------------------
 * Tests: ParseLine
 * ------------------------------------------------------------------------- */
static void test_parseline_plain(void)
{
	char s[] = "hello world";
	ParseLine(s);
	ASSERT_EQ(strcmp(s, "hello world"), 0);
}

static void test_parseline_hash_comment(void)
{
	char s[] = "key  # comment";
	ParseLine(s);
	ASSERT_EQ(strcmp(s, "key"), 0);
}

static void test_parseline_semicolon_comment(void)
{
	char s[] = "value ; comment";
	ParseLine(s);
	ASSERT_EQ(strcmp(s, "value"), 0);
}

static void test_parseline_crlf(void)
{
	char s[] = "data\r\n";
	ParseLine(s);
	ASSERT_EQ(strcmp(s, "data"), 0);
}

static void test_parseline_leading_whitespace(void)
{
	char s[] = "   word";
	ParseLine(s);
	ASSERT_EQ(strcmp(s, "word"), 0);
}

static void test_parseline_trailing_whitespace(void)
{
	char s[] = "word   ";
	ParseLine(s);
	ASSERT_EQ(strcmp(s, "word"), 0);
}

static void test_parseline_quoted_comment(void)
{
	/* '#' inside quotes must not terminate the line. */
	char s[] = "\"hello#world\"";
	ParseLine(s);
	ASSERT_EQ(strcmp(s, "\"hello#world\""), 0);
}

static void test_parseline_all_whitespace(void)
{
	char s[] = "   ";
	ParseLine(s);
	ASSERT_EQ(s[0], '\0');
}

static void test_parseline_empty(void)
{
	char s[] = "";
	ParseLine(s);
	ASSERT_EQ(s[0], '\0');
}

/* -------------------------------------------------------------------------
 * Tests: GetToken
 * ------------------------------------------------------------------------- */
static void test_gettoken_simple(void)
{
	char s[]   = "key value";
	char tok[64];
	GetToken(s, tok);
	ASSERT_EQ(strcmp(tok, "key"),   0);
	ASSERT_EQ(strcmp(s,   "value"), 0);
}

static void test_gettoken_multiple_spaces(void)
{
	char s[]   = "key    value";
	char tok[64];
	GetToken(s, tok);
	ASSERT_EQ(strcmp(tok, "key"),   0);
	ASSERT_EQ(strcmp(s,   "value"), 0);
}

static void test_gettoken_single_token(void)
{
	/* No value after token → remainder is empty. */
	char s[]   = "onlykey";
	char tok[64];
	GetToken(s, tok);
	ASSERT_EQ(strcmp(tok, "onlykey"), 0);
	ASSERT_EQ(s[0], '\0');
}

static void test_gettoken_empty_string(void)
{
	char s[]   = "";
	char tok[64];
	tok[0] = '\x7f';   /* sentinel */
	GetToken(s, tok);
	ASSERT_EQ(tok[0], '\0');
	ASSERT_EQ(s[0],   '\0');
}

static void test_gettoken_leading_whitespace(void)
{
	char s[]   = "   token rest";
	char tok[64];
	GetToken(s, tok);
	ASSERT_EQ(strcmp(tok, "token"), 0);
	ASSERT_EQ(strcmp(s,   "rest"),  0);
}

static void test_gettoken_newline_strips_remainder(void)
{
	/* Newline terminates scan; the part after '\n' is not token or rest. */
	char s[]   = "cmd\nignored";
	char tok[64];
	GetToken(s, tok);
	ASSERT_EQ(strcmp(tok, "cmd"), 0);
	ASSERT_EQ(s[0], '\0');
}

/* -------------------------------------------------------------------------
 * main
 * ------------------------------------------------------------------------- */
int main(void)
{
	RUN(parseline_plain);
	RUN(parseline_hash_comment);
	RUN(parseline_semicolon_comment);
	RUN(parseline_crlf);
	RUN(parseline_leading_whitespace);
	RUN(parseline_trailing_whitespace);
	RUN(parseline_quoted_comment);
	RUN(parseline_all_whitespace);
	RUN(parseline_empty);
	RUN(gettoken_simple);
	RUN(gettoken_multiple_spaces);
	RUN(gettoken_single_token);
	RUN(gettoken_empty_string);
	RUN(gettoken_leading_whitespace);
	RUN(gettoken_newline_strips_remainder);

	printf("\n%d/%d passed\n", g_tests_run - g_tests_failed, g_tests_run);
	return g_tests_failed ? 1 : 0;
}
