/*
 * Unit tests for src/serialize.c and src/deserialize.c
 * Uses the "include the C file" pattern -- no changes to existing sources.
 *
 * Strategy: test roundtrips of small structs that carry no CRC field, so
 * System_Error is never triggered by a checksum mismatch.  System_Error is
 * still defined (as a real noreturn function) because deserialize.c forward-
 * declares it and the linker must resolve the symbol.
 */

/*
 * Pre-include project headers so their include guards fire before the C
 * files are included.  system.h declares noreturn System_Error; pre-loading
 * it ensures the guard fires before the forward-declaration inside
 * deserialize.c, keeping both compatible.
 */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "../defines.h"
#include "../structs.h"
#include "../system.h"
#include "../platform.h"
#include "../language.h"
#include "../quests.h"

#include "test_harness.h"
#include "mocks_system.h"

#include "../platform.c"
#include "../serialize.c"
#include "../deserialize.c"

/* -------------------------------------------------------------------------
 * External variable definitions required by included headers.
 * myopen.h declares serBuf/erRet; myopen.c is not included here.
 * ------------------------------------------------------------------------- */
uint8_t         serBuf[4096];
bool            erRet;

/* -------------------------------------------------------------------------
 * Tests: ibbs_node_attack  roundtrip
 * ------------------------------------------------------------------------- */
static void test_ibbs_node_attack_roundtrip(void)
{
	uint8_t buf[BUF_SIZE_ibbs_node_attack];
	struct ibbs_node_attack src, dst;

	memset(&src, 0, sizeof(src));
	memset(&dst, 0, sizeof(dst));
	src.ReceiveIndex = 42;
	src.SendIndex    = -7;

	s_ibbs_node_attack_s(&src, buf, sizeof(buf));
	s_ibbs_node_attack_d(buf, sizeof(buf), &dst);

	ASSERT_EQ(dst.ReceiveIndex, 42);
	ASSERT_EQ(dst.SendIndex,    -7);
}

/* -------------------------------------------------------------------------
 * Tests: ibbs_node_reset  roundtrip
 * ------------------------------------------------------------------------- */
static void test_ibbs_node_reset_roundtrip(void)
{
	uint8_t buf[BUF_SIZE_ibbs_node_reset];
	struct ibbs_node_reset src, dst;

	memset(&src, 0, sizeof(src));
	memset(&dst, 0, sizeof(dst));
	src.Received = 100;
	src.LastSent = 999999;

	s_ibbs_node_reset_s(&src, buf, sizeof(buf));
	s_ibbs_node_reset_d(buf, sizeof(buf), &dst);

	ASSERT_EQ(dst.Received, 100);
	ASSERT_EQ(dst.LastSent, 999999);
}

/* -------------------------------------------------------------------------
 * Tests: Strategy  roundtrip
 * ------------------------------------------------------------------------- */
static void test_strategy_roundtrip(void)
{
	uint8_t buf[BUF_SIZE_Strategy];
	struct Strategy src, dst;

	memset(&src, 0, sizeof(src));
	memset(&dst, 0, sizeof(dst));
	src.AttackLength    = 5;
	src.AttackIntensity = 3;
	src.LootLevel       = 2;
	src.DefendLength    = 7;
	src.DefendIntensity = 1;

	s_Strategy_s(&src, buf, sizeof(buf));
	s_Strategy_d(buf, sizeof(buf), &dst);

	ASSERT_EQ(dst.AttackLength,    5);
	ASSERT_EQ(dst.AttackIntensity, 3);
	ASSERT_EQ(dst.LootLevel,       2);
	ASSERT_EQ(dst.DefendLength,    7);
	ASSERT_EQ(dst.DefendIntensity, 1);
}

/* -------------------------------------------------------------------------
 * Tests: SpellsInEffect  roundtrip
 * ------------------------------------------------------------------------- */
static void test_spellsineffect_roundtrip(void)
{
	uint8_t buf[BUF_SIZE_SpellsInEffect];
	struct SpellsInEffect src, dst;

	memset(&src, 0, sizeof(src));
	memset(&dst, 0, sizeof(dst));
	src.SpellNum = 12;
	src.Energy   = 500;

	s_SpellsInEffect_s(&src, buf, sizeof(buf));
	s_SpellsInEffect_d(buf, sizeof(buf), &dst);

	ASSERT_EQ(dst.SpellNum, 12);
	ASSERT_EQ(dst.Energy,   500);
}

/* -------------------------------------------------------------------------
 * Tests: FileHeader  roundtrip
 *
 * The fp field is a pointer and is serialized as a no-op (pack_ptr), so it
 * is not checked after deserialization.
 * ------------------------------------------------------------------------- */
static void test_fileheader_roundtrip(void)
{
	uint8_t buf[BUF_SIZE_FileHeader];
	struct FileHeader src, dst;

	memset(&src, 0, sizeof(src));
	memset(&dst, 0, sizeof(dst));
	src.fp        = NULL;
	strlcpy(src.szFileName, "/testfile.dat", sizeof(src.szFileName));
	src.lStart    = 128;
	src.lEnd      = 4096;
	src.lFileSize = 3968;

	s_FileHeader_s(&src, buf, sizeof(buf));
	s_FileHeader_d(buf, sizeof(buf), &dst);

	ASSERT_EQ(strcmp(dst.szFileName, "/testfile.dat"), 0);
	ASSERT_EQ(dst.lStart,    128);
	ASSERT_EQ(dst.lEnd,      4096);
	ASSERT_EQ(dst.lFileSize, 3968);
}

/* -------------------------------------------------------------------------
 * Tests: buffer-too-small returns SIZE_MAX  (release mode only)
 *
 * serialize.c uses assert() before the SIZE_MAX guard, so this path is
 * unreachable in debug builds.  Only compile and run it when NDEBUG is
 * defined (i.e. the test binary was built for release).
 * ------------------------------------------------------------------------- */
#ifdef NDEBUG
static void test_ibbs_node_attack_underflow(void)
{
	uint8_t buf[1];   /* too small for 2×int16_t */
	struct ibbs_node_attack src;
	memset(&src, 0, sizeof(src));
	size_t r = s_ibbs_node_attack_s(&src, buf, sizeof(buf));
	ASSERT_EQ((long long)(r == SIZE_MAX), 1LL);
}
#endif /* NDEBUG */

/* -------------------------------------------------------------------------
 * main
 * ------------------------------------------------------------------------- */
int main(void)
{
	RUN(ibbs_node_attack_roundtrip);
	RUN(ibbs_node_reset_roundtrip);
	RUN(strategy_roundtrip);
	RUN(spellsineffect_roundtrip);
	RUN(fileheader_roundtrip);
#ifdef NDEBUG
	RUN(ibbs_node_attack_underflow);
#endif

	printf("\n%d/%d passed\n", g_tests_run - g_tests_failed, g_tests_run);
	return g_tests_failed ? 1 : 0;
}
