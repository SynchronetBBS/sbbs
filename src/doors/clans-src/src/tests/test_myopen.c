/*
 * Unit tests for src/myopen.c  (EncryptWrite / EncryptRead)
 * Uses the "include the C file" pattern -- no changes to existing sources.
 *
 * System_Error is defined as a real noreturn function (not a macro) because
 * deserialize.c forward-declares it, making the macro trick inapplicable.
 * exit() interception is not needed here; longjmp from System_Error is
 * sufficient for ASSERT_FATAL.
 */

#include <setjmp.h>
#include <stdnoreturn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../defines.h"
#include "../system.h"
#include "../platform.h"
#include "../video.h"

#include "test_harness.h"

/* -------------------------------------------------------------------------
 * System_Error mock (real noreturn)
 *
 * Called by EncryptWrite/EncryptRead/cipher when given bad parameters.
 * ASSERT_FATAL arms g_fatal_jmp so the longjmp is caught.
 * ------------------------------------------------------------------------- */
static jmp_buf g_fatal_jmp;

noreturn void System_Error(char *szErrorMsg)
{
	(void)szErrorMsg;
	longjmp(g_fatal_jmp, 1);
}

/* CheckMem is declared in system.h.  myopen.c does not call it, but the
 * linker needs a definition to resolve the symbol. */
void CheckMem(void *ptr)
{
	if (!ptr) longjmp(g_fatal_jmp, 1);
}

#include "../platform.c"
#include "../serialize.c"
#include "../deserialize.c"
#include "../myopen.c"

/* -------------------------------------------------------------------------
 * External variable definitions required by included headers.
 * ------------------------------------------------------------------------- */
struct system   System;
bool            Verbose         = false;
int             _argc           = 0;
static char    *_argv_buf[]     = {NULL};
char          **_argv           = _argv_buf;

/* video.h declares ScreenWidth and ScreenLines. */
int ScreenWidth  = 80;
int ScreenLines  = 24;

/* language.h (via include chain) declares extern Language. */
static struct Language g_lang;
struct Language       *Language = &g_lang;

/* quests.h declares these externs. */
struct Quest    Quests[MAX_QUESTS];
uint8_t         Quests_TFlags[8];

/* -------------------------------------------------------------------------
 * Tests: EncryptWrite -- invalid parameters trigger System_Error
 * ------------------------------------------------------------------------- */
static void test_encryptwrite_zero_size_fatal(void)
{
	char data[4] = {1, 2, 3, 4};
	FILE *f = tmpfile();
	ASSERT_EQ(f != NULL, 1);
	ASSERT_FATAL(EncryptWrite(data, 0, f, XOR_GAME));
	fclose(f);
}

static void test_encryptwrite_null_data_fatal(void)
{
	FILE *f = tmpfile();
	ASSERT_EQ(f != NULL, 1);
	ASSERT_FATAL(EncryptWrite(NULL, 4, f, XOR_GAME));
	fclose(f);
}

static void test_encryptwrite_zero_key_fatal(void)
{
	char data[4] = {1, 2, 3, 4};
	FILE *f = tmpfile();
	ASSERT_EQ(f != NULL, 1);
	ASSERT_FATAL(EncryptWrite(data, sizeof(data), f, 0));
	fclose(f);
}

/* -------------------------------------------------------------------------
 * Tests: EncryptRead -- invalid parameters trigger System_Error
 * ------------------------------------------------------------------------- */
static void test_encryptread_zero_size_fatal(void)
{
	char data[4];
	FILE *f = tmpfile();
	ASSERT_EQ(f != NULL, 1);
	ASSERT_FATAL(EncryptRead(data, 0, f, XOR_GAME));
	fclose(f);
}

static void test_encryptread_null_data_fatal(void)
{
	FILE *f = tmpfile();
	ASSERT_EQ(f != NULL, 1);
	ASSERT_FATAL(EncryptRead(NULL, 4, f, XOR_GAME));
	fclose(f);
}

static void test_encryptread_zero_key_fatal(void)
{
	char data[4];
	FILE *f = tmpfile();
	ASSERT_EQ(f != NULL, 1);
	ASSERT_FATAL(EncryptRead(data, sizeof(data), f, 0));
	fclose(f);
}

/* -------------------------------------------------------------------------
 * Tests: EncryptWrite / EncryptRead roundtrip
 * ------------------------------------------------------------------------- */
static void test_encryptwrite_read_roundtrip(void)
{
	const char xv     = XOR_VILLAGE;
	char       orig[] = "Hello, encrypted world!";
	char       readback[sizeof(orig)];
	bool       ok;

	FILE *f = tmpfile();
	ASSERT_EQ(f != NULL, 1);

	ok = EncryptWrite(orig, sizeof(orig), f, xv);
	ASSERT_EQ(ok, true);

	/* Verify EncryptWrite restored the buffer (XOR applied and undone). */
	ASSERT_EQ(strcmp(orig, "Hello, encrypted world!"), 0);

	rewind(f);
	ok = EncryptRead(readback, sizeof(readback), f, xv);
	ASSERT_EQ(ok, true);
	ASSERT_EQ(strcmp(readback, "Hello, encrypted world!"), 0);
	fclose(f);
}

static void test_encryptwrite_read_roundtrip_ibbs_key(void)
{
	const char xv = XOR_IBBS;
	char data[8]  = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
	char out[8];
	int i;

	FILE *f = tmpfile();
	ASSERT_EQ(f != NULL, 1);
	EncryptWrite(data, sizeof(data), f, xv);
	rewind(f);
	EncryptRead(out, sizeof(out), f, xv);
	fclose(f);

	for (i = 0; i < (int)sizeof(data); i++)
		ASSERT_EQ((int)(unsigned char)out[i], (int)(unsigned char)data[i]);
}

/* -------------------------------------------------------------------------
 * main
 * ------------------------------------------------------------------------- */
int main(void)
{
	RUN(encryptwrite_zero_size_fatal);
	RUN(encryptwrite_null_data_fatal);
	RUN(encryptwrite_zero_key_fatal);
	RUN(encryptread_zero_size_fatal);
	RUN(encryptread_null_data_fatal);
	RUN(encryptread_zero_key_fatal);
	RUN(encryptwrite_read_roundtrip);
	RUN(encryptwrite_read_roundtrip_ibbs_key);

	printf("\n%d/%d passed\n", g_tests_run - g_tests_failed, g_tests_run);
	return g_tests_failed ? 1 : 0;
}
