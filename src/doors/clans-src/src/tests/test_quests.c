/*
 * Unit tests for src/quests.c flag functions
 * Uses the "include the C file" pattern -- no changes to existing sources.
 *
 * Targeted functions:
 *   ClearFlags  – zeroes all 8 bytes of the flag array
 *   SetFlag     – sets bit WhichFlag in the flag array
 *   ClearFlag   – clears bit WhichFlag (only if currently set)
 *   FlagSet     – returns true if bit WhichFlag is set, false otherwise
 *
 * All functions treat the flag array as a packed bit vector: 8 bytes = 64 bits.
 */

#include <setjmp.h>
#include <stdnoreturn.h>
#include <stdio.h>
#include <string.h>

#include "../defines.h"
#include "../structs.h"
#include "../system.h"
#include "../platform.h"

#include "test_harness.h"

/*
 * System stubs
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
#include "../quests.c"

/* -------------------------------------------------------------------------
 * Tests: ClearFlags
 * Zeroes all 8 bytes of the flag array.
 * ------------------------------------------------------------------------- */
static void test_clearflags_zeroes_all(void)
{
	uint8_t flags[8];

	/* Fill all bytes with 0xFF */
	for (int i = 0; i < 8; i++)
		flags[i] = 0xFF;

	/* Call ClearFlags */
	ClearFlags(flags);

	/* Verify all bytes are now 0x00 */
	for (int i = 0; i < 8; i++)
		ASSERT_EQ((long long)flags[i], 0LL);
}

static void test_clearflags_on_clean_array(void)
{
	uint8_t flags[8] = {0};

	/* Should be a no-op, but call it anyway */
	ClearFlags(flags);

	/* Verify all are still 0 */
	for (int i = 0; i < 8; i++)
		ASSERT_EQ((long long)flags[i], 0LL);
}

/* -------------------------------------------------------------------------
 * Tests: SetFlag
 * Sets bit WhichFlag in the flag array (no bounds checking).
 * Bit index = WhichFlag; byte = WhichFlag/8, bit within byte = WhichFlag%8.
 * ------------------------------------------------------------------------- */
static void test_setflag_bit0(void)
{
	uint8_t flags[8] = {0};

	/* Set bit 0 (byte 0, bit 0) */
	SetFlag(flags, 0);

	/* Bit 0 = 1 << 0 = 0x01 */
	ASSERT_EQ((long long)flags[0], 1LL);
	ASSERT_EQ((long long)flags[1], 0LL);
}

static void test_setflag_bit7(void)
{
	uint8_t flags[8] = {0};

	/* Set bit 7 (byte 0, bit 7) */
	SetFlag(flags, 7);

	/* Bit 7 = 1 << 7 = 0x80 */
	ASSERT_EQ((long long)flags[0], 128LL);
	ASSERT_EQ((long long)flags[1], 0LL);
}

static void test_setflag_bit8(void)
{
	uint8_t flags[8] = {0};

	/* Set bit 8 (byte 1, bit 0) */
	SetFlag(flags, 8);

	/* Should affect byte 1, not byte 0 */
	ASSERT_EQ((long long)flags[0], 0LL);
	ASSERT_EQ((long long)flags[1], 1LL);
}

/* -------------------------------------------------------------------------
 * Tests: ClearFlag
 * Clears bit WhichFlag only if it's currently set (no-op if already clear).
 * Uses XOR when the bit is confirmed set to avoid double-XOR issues.
 * ------------------------------------------------------------------------- */
static void test_clearflag_clears_set_bit(void)
{
	uint8_t flags[8] = {0};

	/* Set bit 0 first */
	flags[0] = 0x01;

	/* Clear it */
	ClearFlag(flags, 0);

	/* Bit 0 should now be 0 */
	ASSERT_EQ((long long)flags[0], 0LL);
}

static void test_clearflag_already_clear_noop(void)
{
	uint8_t flags[8] = {0};

	/* Bit 0 is already clear */
	/* Calling ClearFlag on an already-clear bit should be a no-op */
	ClearFlag(flags, 0);

	/* All bytes should still be 0 */
	for (int i = 0; i < 8; i++)
		ASSERT_EQ((long long)flags[i], 0LL);
}

static void test_clearflag_cross_byte(void)
{
	uint8_t flags[8] = {0};

	/* Set bit 8 (byte 1, bit 0) */
	flags[1] = 0x01;

	/* Clear it */
	ClearFlag(flags, 8);

	/* Byte 1 should now be 0 */
	ASSERT_EQ((long long)flags[1], 0LL);
	ASSERT_EQ((long long)flags[0], 0LL);
}

/* -------------------------------------------------------------------------
 * Tests: FlagSet
 * Returns true if bit WhichFlag is set, false otherwise.
 * ------------------------------------------------------------------------- */
static void test_flagset_true_when_set(void)
{
	uint8_t flags[8] = {0};

	/* Set bit 3 */
	flags[0] = 0x08;   /* 1 << 3 */

	/* Check it */
	ASSERT_EQ(FlagSet(flags, 3), true);
}

static void test_flagset_false_when_clear(void)
{
	uint8_t flags[8] = {0};

	/* Bit 3 is clear */
	flags[0] = 0x00;

	/* Check it */
	ASSERT_EQ(FlagSet(flags, 3), false);
}

static void test_flagset_cross_byte(void)
{
	uint8_t flags[8] = {0};

	/* Set bit 12 (byte 1, bit 4) */
	flags[1] = 0x10;   /* 1 << 4 */

	/* Check it */
	ASSERT_EQ(FlagSet(flags, 12), true);

	/* Check an unset bit */
	ASSERT_EQ(FlagSet(flags, 13), false);
}

/* -------------------------------------------------------------------------
 * main
 * ------------------------------------------------------------------------- */
int main(void)
{
	RUN(clearflags_zeroes_all);
	RUN(clearflags_on_clean_array);
	RUN(setflag_bit0);
	RUN(setflag_bit7);
	RUN(setflag_bit8);
	RUN(clearflag_clears_set_bit);
	RUN(clearflag_already_clear_noop);
	RUN(clearflag_cross_byte);
	RUN(flagset_true_when_set);
	RUN(flagset_false_when_clear);
	RUN(flagset_cross_byte);

	printf("\n%d/%d passed\n", g_tests_run - g_tests_failed, g_tests_run);
	return g_tests_failed ? 1 : 0;
}
