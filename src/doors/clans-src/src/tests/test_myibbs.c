/*
 * Unit tests for src/myibbs.c
 * Uses the "include the C file" pattern -- no changes to existing sources.
 *
 * Tests the Fidonet address parser (ConvertStringToAddress) and the path
 * builder (MakeFilename, a static helper).  These are pure string functions
 * with no I/O or display side-effects.
 */

#include <assert.h>
#include <setjmp.h>
#include <stdnoreturn.h>
#include <stdio.h>
#include <string.h>

#include "../defines.h"
#include "../structs.h"
#include "../system.h"
#include "../platform.h"
#include "../readcfg.h"
#include "../ibbs.h"
#include "../video.h"

#include "test_harness.h"

/*
 * System_Error and CheckMem — forward-declared by system.h as noreturn.
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

#include "../platform.c"
#include "../serialize.c"
#include "../deserialize.c"
#include "../myopen.c"
#include "../myibbs.c"

/* Note: test_myibbs.c defines its own System_Error and CheckMem,
 * so it does NOT include mocks_system.h (which would cause redefinition). */

#include "mocks_video.h"
#include "mocks_readcfg.h"
#include "mocks_ibbs.h"
#include "mocks_language.h"
#include "mocks_quests.h"

/* -------------------------------------------------------------------------
 * Tests: ConvertStringToAddress
 * ------------------------------------------------------------------------- */
static void test_full_address(void)
{
	tFidoNode n;
	ConvertStringToAddress(&n, "1:234/567.8");
	ASSERT_EQ(n.wZone,  1);
	ASSERT_EQ(n.wNet,   234);
	ASSERT_EQ(n.wNode,  567);
	ASSERT_EQ(n.wPoint, 8);
}

static void test_address_no_point(void)
{
	tFidoNode n;
	ConvertStringToAddress(&n, "1:234/567");
	ASSERT_EQ(n.wZone,  1);
	ASSERT_EQ(n.wNet,   234);
	ASSERT_EQ(n.wNode,  567);
	ASSERT_EQ(n.wPoint, 0);
}

static void test_address_all_zeros(void)
{
	tFidoNode n;
	ConvertStringToAddress(&n, "0:0/0.0");
	ASSERT_EQ(n.wZone,  0);
	ASSERT_EQ(n.wNet,   0);
	ASSERT_EQ(n.wNode,  0);
	ASSERT_EQ(n.wPoint, 0);
}

static void test_address_large_values(void)
{
	tFidoNode n;
	ConvertStringToAddress(&n, "20:300/400.5");
	ASSERT_EQ(n.wZone,  20);
	ASSERT_EQ(n.wNet,   300);
	ASSERT_EQ(n.wNode,  400);
	ASSERT_EQ(n.wPoint, 5);
}

static void test_address_node_only(void)
{
	/* Bare "zone:net/node" with no point component — point must default 0. */
	tFidoNode n;
	ConvertStringToAddress(&n, "3:100/200");
	ASSERT_EQ(n.wZone,  3);
	ASSERT_EQ(n.wNet,   100);
	ASSERT_EQ(n.wNode,  200);
	ASSERT_EQ(n.wPoint, 0);
}

/* -------------------------------------------------------------------------
 * Tests: MakeFilename  (static helper — accessible via #include pattern)
 * ------------------------------------------------------------------------- */
static void test_makefn_appends_slash(void)
{
	char out[64];
	MakeFilename("path", "file.txt", out, sizeof(out));
	ASSERT_EQ(strcmp(out, "path/file.txt"), 0);
}

static void test_makefn_no_double_slash(void)
{
	char out[64];
	MakeFilename("path/", "file.txt", out, sizeof(out));
	ASSERT_EQ(strcmp(out, "path/file.txt"), 0);
}

static void test_makefn_backslash_no_extra(void)
{
	/* A trailing backslash is already a separator; no '/' added. */
	char out[64];
	MakeFilename("path\\", "file.txt", out, sizeof(out));
	ASSERT_EQ(strcmp(out, "path\\file.txt"), 0);
}

static void test_makefn_absolute_path(void)
{
	char out[128];
	MakeFilename("/var/run", "clans.pak", out, sizeof(out));
	ASSERT_EQ(strcmp(out, "/var/run/clans.pak"), 0);
}

/* -------------------------------------------------------------------------
 * main
 * ------------------------------------------------------------------------- */
int main(void)
{
	RUN(full_address);
	RUN(address_no_point);
	RUN(address_all_zeros);
	RUN(address_large_values);
	RUN(address_node_only);
	RUN(makefn_appends_slash);
	RUN(makefn_no_double_slash);
	RUN(makefn_backslash_no_extra);
	RUN(makefn_absolute_path);

	printf("\n%d/%d passed\n", g_tests_run - g_tests_failed, g_tests_run);
	return g_tests_failed ? 1 : 0;
}
