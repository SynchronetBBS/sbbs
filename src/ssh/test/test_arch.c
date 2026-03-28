/*
 * test_arch.c -- Tests for RFC 4251 wire format functions (ssh-arch.c).
 *
 * Only dssh_parse_uint32, dssh_serialize_uint32, and dssh_cleanse
 * remain in the library.  All other parse/serialize functions were
 * removed (dead code, no library callers).
 */

#include "dssh_test.h"
#include "deucessh.h"

/* ----------------------------------------------------------------
 * uint32
 * ---------------------------------------------------------------- */

static int
test_parse_uint32_basic(void)
{
	/* 0x01020304 big-endian */
	uint8_t buf[] = { 0x01, 0x02, 0x03, 0x04 };
	dssh_uint32_t val = 0;
	int64_t ret = dssh_parse_uint32(buf, sizeof(buf), &val);
	ASSERT_EQ(ret, 4);
	ASSERT_EQ_U(val, 0x01020304);
	return TEST_PASS;
}

static int
test_parse_uint32_zero(void)
{
	uint8_t buf[] = { 0x00, 0x00, 0x00, 0x00 };
	dssh_uint32_t val = 1;
	dssh_parse_uint32(buf, sizeof(buf), &val);
	ASSERT_EQ_U(val, 0);
	return TEST_PASS;
}

static int
test_parse_uint32_max(void)
{
	uint8_t buf[] = { 0xFF, 0xFF, 0xFF, 0xFF };
	dssh_uint32_t val = 0;
	dssh_parse_uint32(buf, sizeof(buf), &val);
	ASSERT_EQ_U(val, UINT32_MAX);
	return TEST_PASS;
}

static int
test_parse_uint32_short_buffer(void)
{
	uint8_t buf[] = { 0x01, 0x02, 0x03 };
	dssh_uint32_t val = 0;
	ASSERT_ERR(dssh_parse_uint32(buf, sizeof(buf), &val), DSSH_ERROR_PARSE);
	return TEST_PASS;
}

static int
test_parse_uint32_empty_buffer(void)
{
	uint8_t buf[] = { 0 };
	dssh_uint32_t val = 0;
	ASSERT_ERR(dssh_parse_uint32(buf, 0, &val), DSSH_ERROR_PARSE);
	return TEST_PASS;
}

static int
test_serialize_uint32(void)
{
	uint8_t buf[8] = { 0 };
	size_t pos = 0;
	ASSERT_EQ(dssh_serialize_uint32(0xDEADBEEF, buf, sizeof(buf), &pos), 0);
	ASSERT_EQ_U(pos, 4);
	uint8_t expected[] = { 0xDE, 0xAD, 0xBE, 0xEF };
	ASSERT_MEM_EQ(buf, expected, 4);
	return TEST_PASS;
}

static int
test_serialize_uint32_overflow(void)
{
	uint8_t buf[3];
	size_t pos = 0;
	ASSERT_ERR(dssh_serialize_uint32(1, buf, sizeof(buf), &pos), DSSH_ERROR_TOOLONG);
	return TEST_PASS;
}

static int
test_uint32_roundtrip(void)
{
	uint8_t buf[8];
	size_t pos = 0;
	dssh_uint32_t orig = 0x12345678;
	dssh_uint32_t parsed = 0;

	ASSERT_EQ(dssh_serialize_uint32(orig, buf, sizeof(buf), &pos), 0);
	ASSERT_EQ(dssh_parse_uint32(buf, pos, &parsed), 4);
	ASSERT_EQ_U(parsed, orig);
	return TEST_PASS;
}

/* ----------------------------------------------------------------
 * serialize at offset  --  verify *pos advances correctly
 * ---------------------------------------------------------------- */

static int
test_serialize_at_nonzero_offset(void)
{
	uint8_t buf[16] = { 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC };
	size_t pos = 4;

	ASSERT_EQ(dssh_serialize_uint32(0xAABBCCDD, buf, sizeof(buf), &pos), 0);
	ASSERT_EQ_U(pos, 8);

	/* First 4 bytes should be untouched */
	uint8_t expected_prefix[] = { 0xCC, 0xCC, 0xCC, 0xCC };
	ASSERT_MEM_EQ(buf, expected_prefix, 4);

	uint8_t expected_val[] = { 0xAA, 0xBB, 0xCC, 0xDD };
	ASSERT_MEM_EQ(&buf[4], expected_val, 4);
	return TEST_PASS;
}

static int
test_serialize_overflow_at_offset(void)
{
	uint8_t buf[8];
	size_t pos = 6;

	ASSERT_ERR(dssh_serialize_uint32(1, buf, sizeof(buf), &pos), DSSH_ERROR_TOOLONG);
	/* pos should not advance on error */
	ASSERT_EQ_U(pos, 6);
	return TEST_PASS;
}

static int
test_serialize_pos_past_bufsz(void)
{
	uint8_t buf[8];
	size_t pos = 10; /* pos > bufsz */

	ASSERT_ERR(dssh_serialize_uint32(1, buf, sizeof(buf), &pos), DSSH_ERROR_TOOLONG);
	ASSERT_EQ_U(pos, 10);
	return TEST_PASS;
}

/* ----------------------------------------------------------------
 * dssh_cleanse
 * ---------------------------------------------------------------- */
static int
test_cleanse_basic(void)
{
	uint8_t buf[16];

	memset(buf, 0xAA, sizeof(buf));
	dssh_cleanse(buf, sizeof(buf));

	/* Verify all bytes zeroed (OPENSSL_cleanse writes zeros) */
	for (size_t i = 0; i < sizeof(buf); i++)
		ASSERT_EQ(buf[i], 0);
	return TEST_PASS;
}

static int
test_cleanse_null(void)
{
	/* Should not crash */
	dssh_cleanse(NULL, 0);
	dssh_cleanse(NULL, 100);
	return TEST_PASS;
}

static int
test_cleanse_zero_len(void)
{
	uint8_t buf[4] = {0xAA, 0xBB, 0xCC, 0xDD};

	dssh_cleanse(buf, 0);

	/* Buffer unchanged */
	ASSERT_EQ(buf[0], 0xAA);
	ASSERT_EQ(buf[3], 0xDD);
	return TEST_PASS;
}

static int
test_parse_uint32_null(void)
{
	uint8_t buf[4] = {0, 0, 0, 1};
	dssh_uint32_t val;

	ASSERT_EQ(dssh_parse_uint32(buf, 4, NULL), DSSH_ERROR_INVALID);
	ASSERT_EQ(dssh_parse_uint32(NULL, 4, &val), DSSH_ERROR_INVALID);
	return TEST_PASS;
}

static int
test_serialize_uint32_null(void)
{
	uint8_t buf[4];
	size_t pos = 0;

	ASSERT_EQ(dssh_serialize_uint32(1, buf, 4, NULL), DSSH_ERROR_INVALID);
	ASSERT_EQ(dssh_serialize_uint32(1, NULL, 4, &pos), DSSH_ERROR_INVALID);
	return TEST_PASS;
}

/* ----------------------------------------------------------------
 * test table
 * ---------------------------------------------------------------- */

static struct dssh_test_entry tests[] = {
	/* uint32 */
	{ "parse_uint32_basic",             test_parse_uint32_basic },
	{ "parse_uint32_zero",              test_parse_uint32_zero },
	{ "parse_uint32_max",               test_parse_uint32_max },
	{ "parse_uint32_short_buffer",      test_parse_uint32_short_buffer },
	{ "parse_uint32_empty_buffer",      test_parse_uint32_empty_buffer },
	{ "serialize_uint32",               test_serialize_uint32 },
	{ "serialize_uint32_overflow",      test_serialize_uint32_overflow },
	{ "parse_uint32_null",             test_parse_uint32_null },
	{ "serialize_uint32_null",         test_serialize_uint32_null },
	{ "uint32_roundtrip",              test_uint32_roundtrip },

	/* offset */
	{ "serialize_at_nonzero_offset",    test_serialize_at_nonzero_offset },
	{ "serialize_overflow_at_offset",   test_serialize_overflow_at_offset },
	{ "serialize_pos_past_bufsz",       test_serialize_pos_past_bufsz },

	/* dssh_cleanse */
	{ "cleanse_basic",                  test_cleanse_basic },
	{ "cleanse_null",                   test_cleanse_null },
	{ "cleanse_zero_len",               test_cleanse_zero_len },
};

DSSH_TEST_MAIN(tests)
