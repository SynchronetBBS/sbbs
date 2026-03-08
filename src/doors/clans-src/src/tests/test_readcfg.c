/*
 * Unit tests for src/readcfg.c
 * Uses the "include the C file" pattern -- no changes to existing sources.
 *
 * readcfg.c forward-declares both CheckMem and System_Error at its top,
 * so both must be provided as real function definitions (the macro trick
 * would corrupt those declarations).  System_Error is defined as noreturn
 * using longjmp; CheckMem longjmps only when passed NULL.
 */

#include <limits.h>
#include <setjmp.h>
#include <stdnoreturn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../defines.h"
#include "../system.h"
#include "../platform.h"
#include "../language.h"
#include "../readcfg.h"

#include "test_harness.h"

/* -------------------------------------------------------------------------
 * System_Error and CheckMem mocks (real functions)
 *
 * readcfg.c declares:
 *   void CheckMem(void *Test);
 *   noreturn void System_Error(char *szErrorMsg);
 * We define both before including the .c files so the forward declarations
 * inside readcfg.c are compatible redeclarations.
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

#include "../platform.c"
#include "../misc.c"
#include "../parsing.c"
#include "../readcfg.c"

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
 * Per-test setup / teardown
 *
 * Each test runs in a fresh temp directory so Config_Init tests can
 * write / read "clans.cfg" without interfering with each other or with
 * any real config file.
 * ------------------------------------------------------------------------- */
static char g_origdir[PATH_MAX];
static char g_tmpdir[PATH_MAX];

static void setup(void)
{
	char tmpl[sizeof("/tmp/test_readcfg_XXXXXX")];

	memset(&Config, 0, sizeof(Config));

	if (!getcwd(g_origdir, sizeof(g_origdir))) { perror("getcwd"); exit(1); }
	strcpy(tmpl, "/tmp/test_readcfg_XXXXXX");
	if (!mkdtemp(tmpl))                         { perror("mkdtemp"); exit(1); }
	strcpy(g_tmpdir, tmpl);
	if (chdir(g_tmpdir) != 0)                   { perror("chdir"); exit(1); }
}

static void teardown(void)
{
	Config_Close();
	(void)unlink("clans.cfg");
	if (chdir(g_origdir) != 0) { perror("chdir back"); exit(1); }
	if (rmdir(g_tmpdir)  != 0) perror("rmdir");
}

/* Helper: write a minimal clans.cfg in the current directory. */
static void write_cfg(const char *contents)
{
	FILE *f = fopen("clans.cfg", "w");
	if (!f) { perror("fopen clans.cfg"); exit(1); }
	fputs(contents, f);
	fclose(f);
}

/* -------------------------------------------------------------------------
 * Tests: AddInboundDir
 * ------------------------------------------------------------------------- */
static void test_addinbounddir_adds_slash(void)
{
	AddInboundDir("/var/spool/fido");
	ASSERT_EQ(Config.NumInboundDirs, 1);
	ASSERT_EQ(strcmp(Config.szInboundDirs[0], "/var/spool/fido/"), 0);
}

static void test_addinbounddir_keeps_trailing_slash(void)
{
	AddInboundDir("/var/spool/fido/");
	ASSERT_EQ(Config.NumInboundDirs, 1);
	ASSERT_EQ(strcmp(Config.szInboundDirs[0], "/var/spool/fido/"), 0);
}

static void test_addinbounddir_backslash_kept(void)
{
	/* Backslash is treated as a path separator; no additional slash added. */
	AddInboundDir("C:\\fido\\");
	ASSERT_EQ(Config.NumInboundDirs, 1);
	ASSERT_EQ(strcmp(Config.szInboundDirs[0], "C:\\fido\\"), 0);
}

static void test_addinbounddir_empty_string(void)
{
	/* Empty string: dirlen==0, addSlash stays false → stored as "". */
	AddInboundDir("");
	ASSERT_EQ(Config.NumInboundDirs, 1);
	ASSERT_EQ(strcmp(Config.szInboundDirs[0], ""), 0);
}

static void test_addinbounddir_multiple(void)
{
	AddInboundDir("/in1");
	AddInboundDir("/in2/");
	ASSERT_EQ(Config.NumInboundDirs, 2);
	ASSERT_EQ(strcmp(Config.szInboundDirs[0], "/in1/"), 0);
	ASSERT_EQ(strcmp(Config.szInboundDirs[1], "/in2/"), 0);
}

static void test_addinbounddir_negative_count_fatal(void)
{
	Config.NumInboundDirs = -1;
	ASSERT_FATAL(AddInboundDir("/any"));
}

/* -------------------------------------------------------------------------
 * Tests: Config_Init
 * ------------------------------------------------------------------------- */
static void test_config_init_no_file(void)
{
	/* No clans.cfg in temp dir → returns false. */
	ASSERT_EQ(Config_Init(1, NULL), false);
}

static void test_config_init_empty_file(void)
{
	write_cfg("");
	ASSERT_EQ(Config_Init(1, NULL), true);
	ASSERT_EQ(Config.Initialized, true);
}

static void test_config_init_sysopname(void)
{
	write_cfg("SysopName TestSysop\n");
	Config_Init(1, NULL);
	ASSERT_EQ(strcmp(Config.szSysopName, "TestSysop"), 0);
}

static void test_config_init_bbsname(void)
{
	write_cfg("BBSName The Test BBS\n");
	Config_Init(1, NULL);
	ASSERT_EQ(strcmp(Config.szBBSName, "The Test BBS"), 0);
}

static void test_config_init_uselog(void)
{
	write_cfg("UseLog Yes\n");
	Config_Init(1, NULL);
	ASSERT_EQ(Config.UseLog, true);
}

static void test_config_init_inbounddir_gets_slash(void)
{
	write_cfg("InboundDir /var/spool/fido\n");
	Config_Init(1, NULL);
	ASSERT_EQ(Config.NumInboundDirs, 1);
	ASSERT_EQ(strcmp(Config.szInboundDirs[0], "/var/spool/fido/"), 0);
}

static void test_config_init_comment_ignored(void)
{
	write_cfg("# This is a comment\nSysopName Sysop1\n");
	Config_Init(1, NULL);
	ASSERT_EQ(strcmp(Config.szSysopName, "Sysop1"), 0);
}

/* -------------------------------------------------------------------------
 * main
 * ------------------------------------------------------------------------- */
int main(void)
{
	g_test_setup    = setup;
	g_test_teardown = teardown;

	RUN(addinbounddir_adds_slash);
	RUN(addinbounddir_keeps_trailing_slash);
	RUN(addinbounddir_backslash_kept);
	RUN(addinbounddir_empty_string);
	RUN(addinbounddir_multiple);
	RUN(addinbounddir_negative_count_fatal);
	RUN(config_init_no_file);
	RUN(config_init_empty_file);
	RUN(config_init_sysopname);
	RUN(config_init_bbsname);
	RUN(config_init_uselog);
	RUN(config_init_inbounddir_gets_slash);
	RUN(config_init_comment_ignored);

	printf("\n%d/%d passed\n", g_tests_run - g_tests_failed, g_tests_run);
	return g_tests_failed ? 1 : 0;
}
