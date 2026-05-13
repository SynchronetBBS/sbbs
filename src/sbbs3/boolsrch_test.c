/* Standalone unit tests for boolsrch.c */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright Rob Swindell - http://www.synchro.net/copyright.html			*
 *																			*
 * This program is free software; you can redistribute it and/or			*
 * modify it under the terms of the GNU General Public License				*
 * as published by the Free Software Foundation; either version 2			*
 * of the License, or (at your option) any later version.					*
 ****************************************************************************/

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Pull in the compiled implementation as a translation-unit include so the
 * test driver can reach file-local helpers (bool_expr_describe,
 * bool_expr_is_simple, bool_expr_simple_text) without exposing them in the
 * public boolsrch.h header. Build this test as a single source file:
 *   cl /Fe:boolsrch_test.exe boolsrch_test.c       (MSVC; see notes in file)
 * — do NOT also pass boolsrch.c on the command line. */
#include "boolsrch.c"

/* Standalone build provides its own strcasestr and safe_snprintf so the test
 * driver doesn't have to pull in xpdev/genwrap.c (which drags in windows.h,
 * ini_file, etc.). The real sbbs build uses xpdev's versions via genwrap.h. */
#if defined(_WIN32)
#include <stdarg.h>
char* strcasestr(const char* haystack, const char* needle)
{
	size_t      nlen = strlen(needle);
	const char* p;
	if (nlen == 0)
		return (char*)haystack;
	for (p = haystack; *p != '\0'; p++) {
		size_t i;
		for (i = 0; i < nlen; i++) {
			if (tolower((unsigned char)p[i]) != tolower((unsigned char)needle[i]))
				break;
		}
		if (i == nlen)
			return (char*)p;
	}
	return NULL;
}
int safe_snprintf(char* dst, size_t size, const char* fmt, ...)
{
	int     rc;
	va_list ap;
	va_start(ap, fmt);
	rc = vsnprintf(dst, size, fmt, ap);
	va_end(ap);
	return rc;
}
#endif

static int  tests_run = 0;
static int  tests_failed = 0;

#define CHECK(cond, fmt, ...)                                                 \
	do {                                                                      \
		tests_run++;                                                          \
		if (!(cond)) {                                                        \
			tests_failed++;                                                   \
			fprintf(stderr, "FAIL: " fmt "\n", ##__VA_ARGS__);                \
		}                                                                     \
	} while (0)

/* Compile and test a single haystack expectation. */
static void check_match(const char* query, const char* haystack, bool expected)
{
	char*        err  = NULL;
	bool_expr_t* expr = bool_expr_compile(query, &err);
	if (expr == NULL) {
		tests_run++;
		tests_failed++;
		fprintf(stderr, "FAIL: compile(%s) failed: %s\n",
		    query, err != NULL ? err : "(?)");
		free(err);
		return;
	}
	bool actual = bool_expr_match(expr, haystack);
	CHECK(actual == expected,
	    "match(%s, %s) expected %d got %d",
	    query, haystack, expected, actual);
	bool_expr_free(expr);
	free(err);
}

static void check_match_fields(const char* query, const char* const* fields,
                               size_t nfields, bool expected)
{
	char*        err  = NULL;
	bool_expr_t* expr = bool_expr_compile(query, &err);
	if (expr == NULL) {
		tests_run++;
		tests_failed++;
		fprintf(stderr, "FAIL: compile(%s) failed: %s\n",
		    query, err != NULL ? err : "(?)");
		free(err);
		return;
	}
	bool actual = bool_expr_match_fields(expr, fields, nfields);
	CHECK(actual == expected,
	    "match_fields(%s) expected %d got %d",
	    query, expected, actual);
	bool_expr_free(expr);
	free(err);
}

static void check_compile_error(const char* query, const char* what)
{
	char*        err  = NULL;
	bool_expr_t* expr = bool_expr_compile(query, &err);
	if (expr != NULL) {
		tests_run++;
		tests_failed++;
		fprintf(stderr, "FAIL: compile(%s) expected error (%s) but succeeded\n",
		    query, what);
		bool_expr_free(expr);
		free(err);
		return;
	}
	CHECK(err != NULL, "compile(%s): expected error message for %s",
	    query, what);
	free(err);
}

static void check_describe(const char* query, const char* expected)
{
	char*        err  = NULL;
	bool_expr_t* expr = bool_expr_compile(query, &err);
	if (expr == NULL) {
		tests_run++;
		tests_failed++;
		fprintf(stderr, "FAIL: compile(%s) failed: %s\n",
		    query, err != NULL ? err : "(?)");
		free(err);
		return;
	}
	char buf[256];
	bool_expr_describe(expr, buf, sizeof buf);
	CHECK(strcmp(buf, expected) == 0,
	    "describe(%s):\n  expected: %s\n  actual:   %s",
	    query, expected, buf);
	bool_expr_free(expr);
	free(err);
}

int main(void)
{
	/* --- bare-phrase / substring fidelity --- */
	check_match("monitor", "VGA monitor here", true);
	check_match("monitor", "vga MONITOR here", true); /* case-insens */
	check_match("monitor", "tv display only",  false);
	check_match("VGA monitor", "the VGA monitor here", true); /* phrase incl space */
	check_match("VGA monitor", "the VGA in monitor",   false); /* not consecutive */

	/* --- symbol AND --- */
	check_match("text & edit",   "any text editor here", true);
	check_match("text & edit",   "any text here",        false);
	check_match("disk & format & floppy", "format the floppy disk", true);
	check_match("disk & format & floppy", "format the disk",         false);

	/* --- keyword AND --- */
	check_match("text and edit", "any text editor here", true);
	check_match("TEXT AND EDIT", "any text editor here", true);

	/* --- OR (symbol + keyword) --- */
	check_match("hard disk | hard drive", "swap your hard drive",     true);
	check_match("hard disk | hard drive", "swap your hard disk",      true);
	check_match("hard disk | hard drive", "swap your floppy",         false);
	check_match("hard disk or hard drive", "swap your hard drive",    true);

	/* --- NOT (symbol + keyword) --- */
	check_match("! 320x200",  "GIF 320x200 image",  false);
	check_match("! 320x200",  "GIF 1024x768 image", true);
	check_match("not 320x200", "GIF 320x200 image", false);

	/* --- combined --- */
	check_match("1024x768 &! swim",   "GIF 1024x768 wallpaper",        true);
	check_match("1024x768 &! swim",   "GIF 1024x768 swim suit photo",  false);
	check_match("(windows | dos) & modem !os/2",
	    "Windows modem driver", true);
	check_match("(windows | dos) & modem !os/2",
	    "DOS modem driver", true);
	check_match("(windows | dos) & modem !os/2",
	    "OS/2 modem driver", false);
	check_match("(windows | dos) & modem !os/2",
	    "Mac modem driver", false);

	/* --- quoted literal preserves special chars / spaces --- */
	check_match("\"smith & jones\"", "Smith & Jones report",       true);
	check_match("\"smith & jones\"", "Smith and Jones report",     false);
	check_match("\"new york\"",      "New York City",              true);

	/* --- whole-word keyword discrimination --- */
	check_match("bandit", "the bandit returned", true);
	/* 'bandit' must not parse as 'b' AND 'it' */
	check_match("bandit", "b plus it",           false);
	check_match("band and it", "the band and it returned", true);
	check_match("band and it", "the band returned",        false);

	/* --- quoted-string: whole-word semantics with whitespace-suppresses-boundary rule --- */
	/* "TEST" — boundary check both sides */
	check_match("\"TEST\"",   "TESTING",         false);  /* trailing word-char fails */
	check_match("\"TEST\"",   "BACKWARD",        false);  /* TEST not in haystack anyway */
	check_match("\"TEST\"",   "the TEST here",   true);   /* both bounded by space */
	check_match("\"TEST\"",   "TEST",            true);   /* start- and end-of-haystack */
	check_match("\"TEST\"",   "(TEST)",          true);   /* paren chars are non-word */
	check_match("\"TEST\"",   "TEST.",           true);   /* '.' is non-word */
	check_match("\"TEST\"",   "PRETEST",         false);  /* leading 'E' is word char */
	check_match("\"text\"",   "texting",         false);  /* case-insens + boundary fails */
	/* "TEST " — leading boundary, no trailing boundary */
	check_match("\"TEST \"",  "TESTING",         false);  /* substring 'TEST ' not present */
	check_match("\"TEST \"",  "the TEST here",   true);   /* leading bounded; trailing skipped */
	check_match("\"TEST \"",  "TEST",            false);  /* substring 'TEST ' not present */
	/* " TEST" — no leading boundary, trailing boundary */
	check_match("\" TEST\"",  "TESTING",         false);  /* substring ' TEST' not present */
	check_match("\" TEST\"",  "the TEST here",   true);   /* substring ' TEST' present, trailing OK */
	check_match("\" TEST\"",  "the TESTING here", false); /* substring ' TEST' present, trailing 'I' is word */
	/* " TEST " — escape hatch back to pure substring */
	check_match("\" TEST \"", "the TESTING here", false); /* substring ' TEST ' not present (no space after) */
	check_match("\" TEST \"", "the TEST here",   true);   /* ' TEST ' present */
	/* Multi-word phrases: internal whitespace preserved, outer bounded */
	check_match("\"hard disk\"",  "the hard disk here", true);
	check_match("\"hard disk\"",  "hard disks here",    false);  /* trailing 's' is word */
	check_match("\"hard disk\"",  "hard diskette",      false);  /* trailing 'e' is word */
	check_match("\"hard disk\"",  "the harddisk here",  false);  /* no 'hard disk' substring */
	check_match("\"new york\"",   "New York City",      true);
	/* Quoted with special chars — escape semantics preserved on top of whole-word */
	check_match("\"smith & jones\"", "Smith & Jones report", true);
	check_match("\"smith & jones\"", "blacksmith & jones",   false); /* leading 'k' is word */
	/* --- AND+NOT regression (user-reported live BBS case) --- */
	check_match("yahoo and NOT no-such-strring-random",
	    "From: Yahoo Mail",                    true);
	check_match("yahoo and NOT yahoo",
	    "From: Yahoo Mail",                    false);
	check_match("yahoo and !no-such-strring-random",
	    "From: Yahoo Mail",                    true);
	check_match("yahoo & !no-such-strring-random",
	    "From: Yahoo Mail",                    true);
	check_describe("yahoo and NOT no-such-strring-random",
	    "(AND \"yahoo\" (NOT \"no-such-strring-random\"))");

	/* AST shape via describe — Q prefix marks quoted */
	check_describe("text",     "\"text\"");
	check_describe("\"text\"", "Q\"text\"");

	/* --- parens around a single term should be transparent --- */
	check_match("(WARD)",     "BACKWARD",        true);  /* WARD is a substring of BACKWARD */
	check_match("(WORD)",     "BACKWARD",        false); /* WORD is NOT a substring of BACKWARD */
	check_match("(ward)",     "the bandit",      false);
	check_match("(text)",     "any text editor", true);
	check_match("((WARD))",   "BACKWARD",        true);
	check_describe("(WARD)",  "\"WARD\"");
	check_describe("((WARD))","\"WARD\"");

	/* --- multi-field matching --- */
	{
		const char* fields[] = { "subject line", "body text here", "tag1 tag2" };
		check_match_fields("subject & body", fields, 3, true);
		check_match_fields("subject & missing", fields, 3, false);
		check_match_fields("tag1", fields, 3, true);
	}

	/* --- empty query: NULL --- */
	check_compile_error("",     "empty query");
	check_compile_error("   ",  "whitespace-only query");
	check_compile_error(NULL,   "NULL query");

	/* --- syntax errors --- */
	check_compile_error("(unbalanced",      "unbalanced paren");
	check_compile_error("\"unterminated",  "unterminated quote");
	check_compile_error("&dangling",        "dangling operator at start");
	check_compile_error("trailing &",       "dangling operator at end");
	check_compile_error("foo & & bar",      "consecutive operators");
	check_compile_error("()",               "empty parens");
	check_compile_error("\"\"",             "empty quoted string");
	check_compile_error("a unexpected ) b", "stray close paren");

	/* --- describe: confirms AST shape --- */
	check_describe("foo",                       "\"foo\"");
	check_describe("foo & bar",                 "(AND \"foo\" \"bar\")");
	check_describe("foo | bar",                 "(OR \"foo\" \"bar\")");
	check_describe("!foo",                      "(NOT \"foo\")");
	check_describe("foo & bar | baz",
	    "(OR (AND \"foo\" \"bar\") \"baz\")"); /* AND tighter than OR */
	check_describe("foo | bar & baz",
	    "(OR \"foo\" (AND \"bar\" \"baz\"))"); /* AND tighter than OR */
	check_describe("(foo | bar) & baz",
	    "(AND (OR \"foo\" \"bar\") \"baz\")");
	check_describe("!!foo",                     "\"foo\"");  /* double-negation collapses */
	check_describe("foo and bar",               "(AND \"foo\" \"bar\")");

	/* --- is_simple --- */
	{
		char*        err = NULL;
		bool_expr_t* e   = bool_expr_compile("just one phrase", &err);
		CHECK(e != NULL && bool_expr_is_simple(e),
		    "is_simple: a bare phrase should report simple");
		bool_expr_free(e);
		free(err);

		err = NULL;
		e   = bool_expr_compile("a & b", &err);
		CHECK(e != NULL && !bool_expr_is_simple(e),
		    "is_simple: an AND expr should not report simple");
		bool_expr_free(e);
		free(err);
	}

	/* --- NULL field tolerance --- */
	{
		const char* fields[] = { NULL, "body text", NULL };
		check_match_fields("body", fields, 3, true);
		check_match_fields("missing", fields, 3, false);
	}

	printf("%d tests run, %d failed\n", tests_run, tests_failed);
	return tests_failed == 0 ? 0 : 1;
}
