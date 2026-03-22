/*
 * test_arch.c — Tests for RFC 4251 wire format functions (ssh-arch.c).
 */

#include "dssh_test.h"
#include "deucessh.h"

/* ----------------------------------------------------------------
 * byte
 * ---------------------------------------------------------------- */

static int
test_parse_byte_basic(void)
{
	uint8_t buf[] = { 0x42 };
	dssh_byte val = 0;
	int64_t ret = dssh_parse_byte(buf, sizeof(buf), &val);
	ASSERT_EQ(ret, 1);
	ASSERT_EQ(val, 0x42);
	return TEST_PASS;
}

static int
test_parse_byte_zero(void)
{
	uint8_t buf[] = { 0x00 };
	dssh_byte val = 0xFF;
	int64_t ret = dssh_parse_byte(buf, sizeof(buf), &val);
	ASSERT_EQ(ret, 1);
	ASSERT_EQ(val, 0x00);
	return TEST_PASS;
}

static int
test_parse_byte_max(void)
{
	uint8_t buf[] = { 0xFF };
	dssh_byte val = 0;
	int64_t ret = dssh_parse_byte(buf, sizeof(buf), &val);
	ASSERT_EQ(ret, 1);
	ASSERT_EQ(val, 0xFF);
	return TEST_PASS;
}

static int
test_parse_byte_empty_buffer(void)
{
	uint8_t buf[] = { 0x00 };
	dssh_byte val = 0;
	ASSERT_ERR(dssh_parse_byte(buf, 0, &val), DSSH_ERROR_PARSE);
	return TEST_PASS;
}

static int
test_serialize_byte(void)
{
	uint8_t buf[4] = { 0 };
	size_t pos = 0;
	int ret = dssh_serialize_byte(0xAB, buf, sizeof(buf), &pos);
	ASSERT_EQ(ret, 0);
	ASSERT_EQ_U(pos, 1);
	ASSERT_EQ(buf[0], 0xAB);
	return TEST_PASS;
}

static int
test_serialize_byte_overflow(void)
{
	uint8_t buf[1];
	size_t pos = 1;
	ASSERT_ERR(dssh_serialize_byte(0x01, buf, sizeof(buf), &pos), DSSH_ERROR_TOOLONG);
	return TEST_PASS;
}

static int
test_serialized_byte_length(void)
{
	ASSERT_EQ_U(dssh_serialized_byte_length(0), 1);
	ASSERT_EQ_U(dssh_serialized_byte_length(0xFF), 1);
	return TEST_PASS;
}

static int
test_byte_roundtrip(void)
{
	uint8_t buf[4];
	size_t pos = 0;
	dssh_byte orig = 0xCD;
	dssh_byte parsed = 0;

	ASSERT_EQ(dssh_serialize_byte(orig, buf, sizeof(buf), &pos), 0);
	ASSERT_EQ(dssh_parse_byte(buf, pos, &parsed), 1);
	ASSERT_EQ(parsed, orig);
	return TEST_PASS;
}

/* ----------------------------------------------------------------
 * boolean
 * ---------------------------------------------------------------- */

static int
test_parse_boolean_true(void)
{
	uint8_t buf[] = { 0x01 };
	dssh_boolean val = false;
	int64_t ret = dssh_parse_boolean(buf, sizeof(buf), &val);
	ASSERT_EQ(ret, 1);
	ASSERT_TRUE(val);
	return TEST_PASS;
}

static int
test_parse_boolean_false(void)
{
	uint8_t buf[] = { 0x00 };
	dssh_boolean val = true;
	int64_t ret = dssh_parse_boolean(buf, sizeof(buf), &val);
	ASSERT_EQ(ret, 1);
	ASSERT_FALSE(val);
	return TEST_PASS;
}

static int
test_parse_boolean_nonzero_is_true(void)
{
	/* RFC 4251: any nonzero value = true */
	uint8_t buf[] = { 0x42 };
	dssh_boolean val = false;
	int64_t ret = dssh_parse_boolean(buf, sizeof(buf), &val);
	ASSERT_EQ(ret, 1);
	ASSERT_TRUE(val);
	return TEST_PASS;
}

static int
test_parse_boolean_ff_is_true(void)
{
	uint8_t buf[] = { 0xFF };
	dssh_boolean val = false;
	dssh_parse_boolean(buf, sizeof(buf), &val);
	ASSERT_TRUE(val);
	return TEST_PASS;
}

static int
test_parse_boolean_empty_buffer(void)
{
	uint8_t buf[] = { 0x00 };
	dssh_boolean val = false;
	ASSERT_ERR(dssh_parse_boolean(buf, 0, &val), DSSH_ERROR_PARSE);
	return TEST_PASS;
}

static int
test_serialize_boolean_true(void)
{
	uint8_t buf[4] = { 0 };
	size_t pos = 0;
	ASSERT_EQ(dssh_serialize_boolean(true, buf, sizeof(buf), &pos), 0);
	ASSERT_EQ_U(pos, 1);
	ASSERT_NE(buf[0], 0);
	return TEST_PASS;
}

static int
test_serialize_boolean_false(void)
{
	uint8_t buf[4] = { 0xFF };
	size_t pos = 0;
	ASSERT_EQ(dssh_serialize_boolean(false, buf, sizeof(buf), &pos), 0);
	ASSERT_EQ_U(pos, 1);
	ASSERT_EQ(buf[0], 0);
	return TEST_PASS;
}

static int
test_serialize_boolean_overflow(void)
{
	uint8_t buf[1];
	size_t pos = 1;
	ASSERT_ERR(dssh_serialize_boolean(true, buf, sizeof(buf), &pos), DSSH_ERROR_TOOLONG);
	return TEST_PASS;
}

static int
test_serialized_boolean_length(void)
{
	ASSERT_EQ_U(dssh_serialized_boolean_length(true), 1);
	ASSERT_EQ_U(dssh_serialized_boolean_length(false), 1);
	return TEST_PASS;
}

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
test_serialized_uint32_length(void)
{
	ASSERT_EQ_U(dssh_serialized_uint32_length(0), 4);
	ASSERT_EQ_U(dssh_serialized_uint32_length(UINT32_MAX), 4);
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
 * uint64
 * ---------------------------------------------------------------- */

static int
test_parse_uint64_basic(void)
{
	uint8_t buf[] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08 };
	dssh_uint64_t val = 0;
	int64_t ret = dssh_parse_uint64(buf, sizeof(buf), &val);
	ASSERT_EQ(ret, 8);
	ASSERT_TRUE(val == UINT64_C(0x0102030405060708));
	return TEST_PASS;
}

static int
test_parse_uint64_zero(void)
{
	uint8_t buf[] = { 0, 0, 0, 0, 0, 0, 0, 0 };
	dssh_uint64_t val = 1;
	dssh_parse_uint64(buf, sizeof(buf), &val);
	ASSERT_TRUE(val == 0);
	return TEST_PASS;
}

static int
test_parse_uint64_max(void)
{
	uint8_t buf[] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
	dssh_uint64_t val = 0;
	dssh_parse_uint64(buf, sizeof(buf), &val);
	ASSERT_TRUE(val == UINT64_MAX);
	return TEST_PASS;
}

static int
test_parse_uint64_short_buffer(void)
{
	uint8_t buf[] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07 };
	dssh_uint64_t val = 0;
	ASSERT_ERR(dssh_parse_uint64(buf, sizeof(buf), &val), DSSH_ERROR_PARSE);
	return TEST_PASS;
}

static int
test_serialize_uint64(void)
{
	uint8_t buf[16] = { 0 };
	size_t pos = 0;
	ASSERT_EQ(dssh_serialize_uint64(UINT64_C(0xFEDCBA9876543210), buf, sizeof(buf), &pos), 0);
	ASSERT_EQ_U(pos, 8);
	uint8_t expected[] = { 0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10 };
	ASSERT_MEM_EQ(buf, expected, 8);
	return TEST_PASS;
}

static int
test_serialize_uint64_overflow(void)
{
	uint8_t buf[7];
	size_t pos = 0;
	ASSERT_ERR(dssh_serialize_uint64(1, buf, sizeof(buf), &pos), DSSH_ERROR_TOOLONG);
	return TEST_PASS;
}

static int
test_serialized_uint64_length(void)
{
	ASSERT_EQ_U(dssh_serialized_uint64_length(0), 8);
	ASSERT_EQ_U(dssh_serialized_uint64_length(UINT64_MAX), 8);
	return TEST_PASS;
}

static int
test_uint64_roundtrip(void)
{
	uint8_t buf[16];
	size_t pos = 0;
	dssh_uint64_t orig = UINT64_C(0xA1B2C3D4E5F60718);
	dssh_uint64_t parsed = 0;

	ASSERT_EQ(dssh_serialize_uint64(orig, buf, sizeof(buf), &pos), 0);
	ASSERT_EQ(dssh_parse_uint64(buf, pos, &parsed), 8);
	ASSERT_TRUE(parsed == orig);
	return TEST_PASS;
}

/* ----------------------------------------------------------------
 * string
 * ---------------------------------------------------------------- */

static int
test_parse_string_basic(void)
{
	/* length=5, "hello" */
	uint8_t buf[] = { 0x00, 0x00, 0x00, 0x05, 'h', 'e', 'l', 'l', 'o' };
	struct dssh_string_s val = { 0 };
	int64_t ret = dssh_parse_string(buf, sizeof(buf), &val);
	ASSERT_EQ(ret, 9);
	ASSERT_EQ_U(val.length, 5);
	ASSERT_MEM_EQ(val.value, "hello", 5);
	return TEST_PASS;
}

static int
test_parse_string_empty(void)
{
	/* length=0 */
	uint8_t buf[] = { 0x00, 0x00, 0x00, 0x00 };
	struct dssh_string_s val = { 0 };
	int64_t ret = dssh_parse_string(buf, sizeof(buf), &val);
	ASSERT_EQ(ret, 4);
	ASSERT_EQ_U(val.length, 0);
	return TEST_PASS;
}

static int
test_parse_string_short_header(void)
{
	uint8_t buf[] = { 0x00, 0x00, 0x00 };
	struct dssh_string_s val = { 0 };
	ASSERT_ERR(dssh_parse_string(buf, sizeof(buf), &val), DSSH_ERROR_PARSE);
	return TEST_PASS;
}

static int
test_parse_string_truncated_data(void)
{
	/* length says 10 but only 3 bytes of data */
	uint8_t buf[] = { 0x00, 0x00, 0x00, 0x0A, 'a', 'b', 'c' };
	struct dssh_string_s val = { 0 };
	ASSERT_ERR(dssh_parse_string(buf, sizeof(buf), &val), DSSH_ERROR_PARSE);
	return TEST_PASS;
}

static int
test_parse_string_points_into_buf(void)
{
	/* Verify val->value points directly into buf (no copy) */
	uint8_t buf[] = { 0x00, 0x00, 0x00, 0x03, 'x', 'y', 'z' };
	struct dssh_string_s val = { 0 };
	dssh_parse_string(buf, sizeof(buf), &val);
	ASSERT_TRUE(val.value == &buf[4]);
	return TEST_PASS;
}

static int
test_serialize_string(void)
{
	uint8_t data[] = "test";
	struct dssh_string_s val = { .value = data, .length = 4 };
	uint8_t buf[16] = { 0 };
	size_t pos = 0;

	ASSERT_EQ(dssh_serialize_string(&val, buf, sizeof(buf), &pos), 0);
	ASSERT_EQ_U(pos, 8);
	uint8_t expected[] = { 0x00, 0x00, 0x00, 0x04, 't', 'e', 's', 't' };
	ASSERT_MEM_EQ(buf, expected, 8);
	return TEST_PASS;
}

static int
test_serialize_string_empty(void)
{
	struct dssh_string_s val = { .value = NULL, .length = 0 };
	uint8_t buf[8] = { 0xFF };
	size_t pos = 0;

	ASSERT_EQ(dssh_serialize_string(&val, buf, sizeof(buf), &pos), 0);
	ASSERT_EQ_U(pos, 4);
	uint8_t expected[] = { 0x00, 0x00, 0x00, 0x00 };
	ASSERT_MEM_EQ(buf, expected, 4);
	return TEST_PASS;
}

static int
test_serialize_string_overflow(void)
{
	uint8_t data[] = "hello";
	struct dssh_string_s val = { .value = data, .length = 5 };
	uint8_t buf[8];
	size_t pos = 0;

	ASSERT_ERR(dssh_serialize_string(&val, buf, sizeof(buf), &pos), DSSH_ERROR_TOOLONG);
	return TEST_PASS;
}

static int
test_serialized_string_length(void)
{
	struct dssh_string_s val = { .value = NULL, .length = 10 };
	ASSERT_EQ_U(dssh_serialized_string_length(&val), 14);

	val.length = 0;
	ASSERT_EQ_U(dssh_serialized_string_length(&val), 4);
	return TEST_PASS;
}

static int
test_string_roundtrip(void)
{
	uint8_t data[] = "roundtrip";
	struct dssh_string_s orig = { .value = data, .length = 9 };
	uint8_t buf[32];
	size_t pos = 0;

	ASSERT_EQ(dssh_serialize_string(&orig, buf, sizeof(buf), &pos), 0);

	struct dssh_string_s parsed = { 0 };
	int64_t ret = dssh_parse_string(buf, pos, &parsed);
	ASSERT_EQ(ret, (int64_t)pos);
	ASSERT_EQ_U(parsed.length, orig.length);
	ASSERT_MEM_EQ(parsed.value, orig.value, orig.length);
	return TEST_PASS;
}

/* ----------------------------------------------------------------
 * bytearray
 * ---------------------------------------------------------------- */

static int
test_parse_bytearray_basic(void)
{
	uint8_t buf[] = { 0xAA, 0xBB, 0xCC, 0xDD };
	struct dssh_string_s val = { .length = 4 };
	int64_t ret = dssh_parse_bytearray(buf, sizeof(buf), &val);
	ASSERT_EQ(ret, 4);
	ASSERT_TRUE(val.value == buf);
	ASSERT_MEM_EQ(val.value, buf, 4);
	return TEST_PASS;
}

static int
test_parse_bytearray_too_short(void)
{
	uint8_t buf[] = { 0xAA, 0xBB };
	struct dssh_string_s val = { .length = 4 };
	ASSERT_ERR(dssh_parse_bytearray(buf, sizeof(buf), &val), DSSH_ERROR_PARSE);
	return TEST_PASS;
}

static int
test_parse_bytearray_min_length(void)
{
	/* Minimum length is 2 */
	uint8_t buf[] = { 0xAA, 0xBB };
	struct dssh_string_s val = { .length = 2 };
	int64_t ret = dssh_parse_bytearray(buf, sizeof(buf), &val);
	ASSERT_EQ(ret, 2);
	return TEST_PASS;
}

static int
test_parse_bytearray_length_too_small(void)
{
	/* val->length < 2 is rejected */
	uint8_t buf[] = { 0xAA };
	struct dssh_string_s val = { .length = 1 };
	ASSERT_ERR(dssh_parse_bytearray(buf, sizeof(buf), &val), DSSH_ERROR_PARSE);
	return TEST_PASS;
}

static int
test_serialize_bytearray(void)
{
	/* serialize_bytearray writes raw bytes, no length prefix */
	uint8_t data[] = { 0x01, 0x02, 0x03 };
	struct dssh_string_s val = { .value = data, .length = 3 };
	uint8_t buf[16] = { 0 };
	size_t pos = 0;

	ASSERT_EQ(dssh_serialize_bytearray(&val, buf, sizeof(buf), &pos), 0);
	ASSERT_EQ_U(pos, 3);
	ASSERT_MEM_EQ(buf, data, 3);
	return TEST_PASS;
}

static int
test_serialize_bytearray_overflow(void)
{
	uint8_t data[] = { 0x01, 0x02, 0x03 };
	struct dssh_string_s val = { .value = data, .length = 3 };
	uint8_t buf[2];
	size_t pos = 0;

	ASSERT_ERR(dssh_serialize_bytearray(&val, buf, sizeof(buf), &pos), DSSH_ERROR_TOOLONG);
	return TEST_PASS;
}

static int
test_serialized_bytearray_length(void)
{
	/* serialized_bytearray_length includes +4 */
	struct dssh_string_s val = { .length = 10 };
	ASSERT_EQ_U(dssh_serialized_bytearray_length(&val), 14);
	return TEST_PASS;
}

/* ----------------------------------------------------------------
 * mpint  —  RFC 4251 section 5 examples
 * ---------------------------------------------------------------- */

static int
test_parse_mpint_zero(void)
{
	/* 0x00000000 => value 0 */
	uint8_t buf[] = { 0x00, 0x00, 0x00, 0x00 };
	struct dssh_string_s val = { 0 };
	int64_t ret = dssh_parse_mpint(buf, sizeof(buf), &val);
	ASSERT_EQ(ret, 4);
	ASSERT_EQ_U(val.length, 0);
	return TEST_PASS;
}

static int
test_parse_mpint_rfc4251_9a378f9b2e332a7(void)
{
	/* RFC 4251 s5: value 0x9a378f9b2e332a7 */
	uint8_t buf[] = {
		0x00, 0x00, 0x00, 0x08,
		0x09, 0xa3, 0x78, 0xf9, 0xb2, 0xe3, 0x32, 0xa7
	};
	struct dssh_string_s val = { 0 };
	int64_t ret = dssh_parse_mpint(buf, sizeof(buf), &val);
	ASSERT_EQ(ret, 12);
	ASSERT_EQ_U(val.length, 8);
	ASSERT_MEM_EQ(val.value, &buf[4], 8);
	return TEST_PASS;
}

static int
test_parse_mpint_rfc4251_80(void)
{
	/* RFC 4251 s5: value 0x80 (needs 0x00 prefix since high bit set) */
	uint8_t buf[] = {
		0x00, 0x00, 0x00, 0x02,
		0x00, 0x80
	};
	struct dssh_string_s val = { 0 };
	int64_t ret = dssh_parse_mpint(buf, sizeof(buf), &val);
	ASSERT_EQ(ret, 6);
	ASSERT_EQ_U(val.length, 2);
	ASSERT_EQ(val.value[0], 0x00);
	ASSERT_EQ(val.value[1], 0x80);
	return TEST_PASS;
}

static int
test_parse_mpint_rfc4251_neg1234(void)
{
	/* RFC 4251 s5: value -0x1234 => 0xEDCC */
	uint8_t buf[] = {
		0x00, 0x00, 0x00, 0x02,
		0xED, 0xCC
	};
	struct dssh_string_s val = { 0 };
	int64_t ret = dssh_parse_mpint(buf, sizeof(buf), &val);
	ASSERT_EQ(ret, 6);
	ASSERT_EQ_U(val.length, 2);
	ASSERT_EQ(val.value[0], 0xED);
	ASSERT_EQ(val.value[1], 0xCC);
	return TEST_PASS;
}

static int
test_parse_mpint_rfc4251_neg_deadbeef(void)
{
	/* RFC 4251 s5: value -0xdeadbeef => 0xFF21524111 */
	uint8_t buf[] = {
		0x00, 0x00, 0x00, 0x05,
		0xFF, 0x21, 0x52, 0x41, 0x11
	};
	struct dssh_string_s val = { 0 };
	int64_t ret = dssh_parse_mpint(buf, sizeof(buf), &val);
	ASSERT_EQ(ret, 9);
	ASSERT_EQ_U(val.length, 5);
	ASSERT_EQ(val.value[0], 0xFF);
	return TEST_PASS;
}

static int
test_parse_mpint_unnecessary_leading_zero(void)
{
	/* 0x00 0x7F => leading 0x00 is unnecessary (0x7F has bit 7 clear) */
	uint8_t buf[] = {
		0x00, 0x00, 0x00, 0x02,
		0x00, 0x7F
	};
	struct dssh_string_s val = { 0 };
	ASSERT_ERR(dssh_parse_mpint(buf, sizeof(buf), &val), DSSH_ERROR_INVALID);
	return TEST_PASS;
}

static int
test_parse_mpint_unnecessary_leading_ff(void)
{
	/* 0xFF 0x80 => leading 0xFF is unnecessary (0x80 has bit 7 set) */
	uint8_t buf[] = {
		0x00, 0x00, 0x00, 0x02,
		0xFF, 0x80
	};
	struct dssh_string_s val = { 0 };
	ASSERT_ERR(dssh_parse_mpint(buf, sizeof(buf), &val), DSSH_ERROR_INVALID);
	return TEST_PASS;
}

static int
test_parse_mpint_single_byte(void)
{
	/* Single byte: no leading-byte check applies */
	uint8_t buf[] = {
		0x00, 0x00, 0x00, 0x01,
		0x42
	};
	struct dssh_string_s val = { 0 };
	int64_t ret = dssh_parse_mpint(buf, sizeof(buf), &val);
	ASSERT_EQ(ret, 5);
	ASSERT_EQ_U(val.length, 1);
	ASSERT_EQ(val.value[0], 0x42);
	return TEST_PASS;
}

static int
test_parse_mpint_truncated(void)
{
	/* Length says 4 bytes but only 2 available */
	uint8_t buf[] = {
		0x00, 0x00, 0x00, 0x04,
		0x01, 0x02
	};
	struct dssh_string_s val = { 0 };
	ASSERT_ERR(dssh_parse_mpint(buf, sizeof(buf), &val), DSSH_ERROR_PARSE);
	return TEST_PASS;
}

static int
test_serialize_mpint(void)
{
	/* mpint serializes just like string */
	uint8_t data[] = { 0x09, 0xa3, 0x78, 0xf9 };
	struct dssh_string_s val = { .value = data, .length = 4 };
	uint8_t buf[16] = { 0 };
	size_t pos = 0;

	ASSERT_EQ(dssh_serialize_mpint(&val, buf, sizeof(buf), &pos), 0);
	ASSERT_EQ_U(pos, 8);
	uint8_t expected[] = { 0x00, 0x00, 0x00, 0x04, 0x09, 0xa3, 0x78, 0xf9 };
	ASSERT_MEM_EQ(buf, expected, 8);
	return TEST_PASS;
}

static int
test_serialize_mpint_zero(void)
{
	struct dssh_string_s val = { .value = NULL, .length = 0 };
	uint8_t buf[8] = { 0 };
	size_t pos = 0;

	ASSERT_EQ(dssh_serialize_mpint(&val, buf, sizeof(buf), &pos), 0);
	ASSERT_EQ_U(pos, 4);
	uint8_t expected[] = { 0x00, 0x00, 0x00, 0x00 };
	ASSERT_MEM_EQ(buf, expected, 4);
	return TEST_PASS;
}

static int
test_serialize_mpint_overflow(void)
{
	uint8_t data[] = { 0x01, 0x02, 0x03, 0x04 };
	struct dssh_string_s val = { .value = data, .length = 4 };
	uint8_t buf[7];
	size_t pos = 0;

	ASSERT_ERR(dssh_serialize_mpint(&val, buf, sizeof(buf), &pos), DSSH_ERROR_TOOLONG);
	return TEST_PASS;
}

static int
test_serialized_mpint_length(void)
{
	struct dssh_string_s val = { .length = 8 };
	ASSERT_EQ_U(dssh_serialized_mpint_length(&val), 12);

	val.length = 0;
	ASSERT_EQ_U(dssh_serialized_mpint_length(&val), 4);
	return TEST_PASS;
}

static int
test_mpint_roundtrip(void)
{
	uint8_t data[] = { 0x00, 0x80, 0x01, 0x02 };
	struct dssh_string_s orig = { .value = data, .length = 4 };
	uint8_t buf[16];
	size_t pos = 0;

	ASSERT_EQ(dssh_serialize_mpint(&orig, buf, sizeof(buf), &pos), 0);

	struct dssh_string_s parsed = { 0 };
	int64_t ret = dssh_parse_mpint(buf, pos, &parsed);
	ASSERT_EQ(ret, (int64_t)pos);
	ASSERT_EQ_U(parsed.length, orig.length);
	ASSERT_MEM_EQ(parsed.value, orig.value, orig.length);
	return TEST_PASS;
}

/* ----------------------------------------------------------------
 * namelist
 * ---------------------------------------------------------------- */

static int
test_parse_namelist_basic(void)
{
	/* "zlib,none" */
	uint8_t buf[] = {
		0x00, 0x00, 0x00, 0x09,
		'z', 'l', 'i', 'b', ',', 'n', 'o', 'n', 'e'
	};
	struct dssh_namelist_s val = { 0 };
	int64_t ret = dssh_parse_namelist(buf, sizeof(buf), &val);
	ASSERT_EQ(ret, 13);
	ASSERT_EQ_U(val.length, 9);
	ASSERT_EQ_U(val.next, 0);
	return TEST_PASS;
}

static int
test_parse_namelist_single(void)
{
	/* "ssh-ed25519" */
	uint8_t buf[] = {
		0x00, 0x00, 0x00, 0x0B,
		's', 's', 'h', '-', 'e', 'd', '2', '5', '5', '1', '9'
	};
	struct dssh_namelist_s val = { 0 };
	int64_t ret = dssh_parse_namelist(buf, sizeof(buf), &val);
	ASSERT_EQ(ret, 15);
	ASSERT_EQ_U(val.length, 11);
	return TEST_PASS;
}

static int
test_parse_namelist_empty(void)
{
	uint8_t buf[] = { 0x00, 0x00, 0x00, 0x00 };
	struct dssh_namelist_s val = { 0 };
	int64_t ret = dssh_parse_namelist(buf, sizeof(buf), &val);
	ASSERT_EQ(ret, 4);
	ASSERT_EQ_U(val.length, 0);
	return TEST_PASS;
}

static int
test_parse_namelist_leading_comma(void)
{
	uint8_t buf[] = {
		0x00, 0x00, 0x00, 0x04,
		',', 'a', 'b', 'c'
	};
	struct dssh_namelist_s val = { 0 };
	ASSERT_ERR(dssh_parse_namelist(buf, sizeof(buf), &val), DSSH_ERROR_PARSE);
	return TEST_PASS;
}

static int
test_parse_namelist_trailing_comma(void)
{
	uint8_t buf[] = {
		0x00, 0x00, 0x00, 0x04,
		'a', 'b', 'c', ','
	};
	struct dssh_namelist_s val = { 0 };
	ASSERT_ERR(dssh_parse_namelist(buf, sizeof(buf), &val), DSSH_ERROR_PARSE);
	return TEST_PASS;
}

static int
test_parse_namelist_double_comma(void)
{
	uint8_t buf[] = {
		0x00, 0x00, 0x00, 0x05,
		'a', ',', ',', 'b', 'c'
	};
	struct dssh_namelist_s val = { 0 };
	ASSERT_ERR(dssh_parse_namelist(buf, sizeof(buf), &val), DSSH_ERROR_PARSE);
	return TEST_PASS;
}

static int
test_parse_namelist_control_char_space(void)
{
	/* space (0x20) is <= ' ', so rejected */
	uint8_t buf[] = {
		0x00, 0x00, 0x00, 0x03,
		'a', ' ', 'b'
	};
	struct dssh_namelist_s val = { 0 };
	ASSERT_ERR(dssh_parse_namelist(buf, sizeof(buf), &val), DSSH_ERROR_PARSE);
	return TEST_PASS;
}

static int
test_parse_namelist_control_char_tab(void)
{
	uint8_t buf[] = {
		0x00, 0x00, 0x00, 0x03,
		'a', '\t', 'b'
	};
	struct dssh_namelist_s val = { 0 };
	ASSERT_ERR(dssh_parse_namelist(buf, sizeof(buf), &val), DSSH_ERROR_PARSE);
	return TEST_PASS;
}

static int
test_parse_namelist_high_byte(void)
{
	/* byte >= 127 (0x7F) is rejected */
	uint8_t buf[] = {
		0x00, 0x00, 0x00, 0x03,
		'a', 0x7F, 'b'
	};
	struct dssh_namelist_s val = { 0 };
	ASSERT_ERR(dssh_parse_namelist(buf, sizeof(buf), &val), DSSH_ERROR_PARSE);
	return TEST_PASS;
}

static int
test_parse_namelist_high_byte_0x80(void)
{
	uint8_t buf[] = {
		0x00, 0x00, 0x00, 0x02,
		'a', 0x80
	};
	struct dssh_namelist_s val = { 0 };
	ASSERT_ERR(dssh_parse_namelist(buf, sizeof(buf), &val), DSSH_ERROR_PARSE);
	return TEST_PASS;
}

static int
test_parse_namelist_nul_byte(void)
{
	uint8_t buf[] = {
		0x00, 0x00, 0x00, 0x03,
		'a', 0x00, 'b'
	};
	struct dssh_namelist_s val = { 0 };
	ASSERT_ERR(dssh_parse_namelist(buf, sizeof(buf), &val), DSSH_ERROR_PARSE);
	return TEST_PASS;
}

static int
test_serialize_namelist(void)
{
	uint8_t data[] = "zlib,none";
	struct dssh_namelist_s val = { .value = data, .length = 9, .next = 0 };
	uint8_t buf[32] = { 0 };
	size_t pos = 0;

	ASSERT_EQ(dssh_serialize_namelist(&val, buf, sizeof(buf), &pos), 0);
	ASSERT_EQ_U(pos, 13);
	uint8_t expected[] = {
		0x00, 0x00, 0x00, 0x09,
		'z', 'l', 'i', 'b', ',', 'n', 'o', 'n', 'e'
	};
	ASSERT_MEM_EQ(buf, expected, 13);
	return TEST_PASS;
}

static int
test_serialize_namelist_empty(void)
{
	struct dssh_namelist_s val = { .value = NULL, .length = 0, .next = 0 };
	uint8_t buf[8] = { 0 };
	size_t pos = 0;

	ASSERT_EQ(dssh_serialize_namelist(&val, buf, sizeof(buf), &pos), 0);
	ASSERT_EQ_U(pos, 4);
	uint8_t expected[] = { 0x00, 0x00, 0x00, 0x00 };
	ASSERT_MEM_EQ(buf, expected, 4);
	return TEST_PASS;
}

static int
test_serialize_namelist_overflow(void)
{
	uint8_t data[] = "abc,def";
	struct dssh_namelist_s val = { .value = data, .length = 7, .next = 0 };
	uint8_t buf[10];
	size_t pos = 0;

	ASSERT_ERR(dssh_serialize_namelist(&val, buf, sizeof(buf), &pos), DSSH_ERROR_TOOLONG);
	return TEST_PASS;
}

static int
test_serialized_namelist_length(void)
{
	struct dssh_namelist_s val = { .length = 9 };
	ASSERT_EQ_U(dssh_serialized_namelist_length(&val), 13);

	val.length = 0;
	ASSERT_EQ_U(dssh_serialized_namelist_length(&val), 4);
	return TEST_PASS;
}

static int
test_namelist_roundtrip(void)
{
	uint8_t data[] = "aes256-ctr,chacha20-poly1305@openssh.com";
	struct dssh_namelist_s orig = { .value = data, .length = 39, .next = 0 };
	uint8_t buf[64];
	size_t pos = 0;

	ASSERT_EQ(dssh_serialize_namelist(&orig, buf, sizeof(buf), &pos), 0);

	struct dssh_namelist_s parsed = { 0 };
	int64_t ret = dssh_parse_namelist(buf, pos, &parsed);
	ASSERT_EQ(ret, (int64_t)pos);
	ASSERT_EQ_U(parsed.length, orig.length);
	ASSERT_MEM_EQ(parsed.value, orig.value, orig.length);
	return TEST_PASS;
}

/* ----------------------------------------------------------------
 * namelist_next  —  iteration
 * ---------------------------------------------------------------- */

static int
test_namelist_next_basic(void)
{
	uint8_t buf[] = {
		0x00, 0x00, 0x00, 0x09,
		'a', 'b', 'c', ',', 'd', 'e', 'f', ',', 'g'
	};
	struct dssh_namelist_s nl = { 0 };
	dssh_parse_namelist(buf, sizeof(buf), &nl);

	struct dssh_string_s name = { 0 };

	/* First: "abc" */
	int64_t ret = dssh_parse_namelist_next(&name, &nl);
	ASSERT_OK(ret);
	ASSERT_EQ_U(name.length, 3);
	ASSERT_MEM_EQ(name.value, "abc", 3);

	/* Second: "def" */
	ret = dssh_parse_namelist_next(&name, &nl);
	ASSERT_OK(ret);
	ASSERT_EQ_U(name.length, 3);
	ASSERT_MEM_EQ(name.value, "def", 3);

	/* Third: "g" */
	ret = dssh_parse_namelist_next(&name, &nl);
	ASSERT_OK(ret);
	ASSERT_EQ_U(name.length, 1);
	ASSERT_MEM_EQ(name.value, "g", 1);

	/* Exhausted */
	ASSERT_ERR(dssh_parse_namelist_next(&name, &nl), DSSH_ERROR_NOMORE);
	return TEST_PASS;
}

static int
test_namelist_next_single(void)
{
	uint8_t buf[] = {
		0x00, 0x00, 0x00, 0x04,
		'n', 'o', 'n', 'e'
	};
	struct dssh_namelist_s nl = { 0 };
	dssh_parse_namelist(buf, sizeof(buf), &nl);

	struct dssh_string_s name = { 0 };

	int64_t ret = dssh_parse_namelist_next(&name, &nl);
	ASSERT_OK(ret);
	ASSERT_EQ_U(name.length, 4);
	ASSERT_MEM_EQ(name.value, "none", 4);

	ASSERT_ERR(dssh_parse_namelist_next(&name, &nl), DSSH_ERROR_NOMORE);
	return TEST_PASS;
}

static int
test_namelist_next_empty(void)
{
	uint8_t buf[] = { 0x00, 0x00, 0x00, 0x00 };
	struct dssh_namelist_s nl = { 0 };
	dssh_parse_namelist(buf, sizeof(buf), &nl);

	struct dssh_string_s name = { 0 };
	ASSERT_ERR(dssh_parse_namelist_next(&name, &nl), DSSH_ERROR_NOMORE);
	return TEST_PASS;
}

static int
test_namelist_next_long_name(void)
{
	/* A name > 64 chars should fail per RFC 4251 s6 */
	char longname[66];
	memset(longname, 'x', 65);
	longname[65] = '\0';

	/* Build wire format: length prefix + 65-char name */
	uint8_t buf[128];
	buf[0] = 0;
	buf[1] = 0;
	buf[2] = 0;
	buf[3] = 65;
	memcpy(&buf[4], longname, 65);

	struct dssh_namelist_s nl = { 0 };
	/* parse_namelist won't reject long names; namelist_next does */
	int64_t ret = dssh_parse_namelist(buf, 69, &nl);
	ASSERT_OK(ret);

	struct dssh_string_s name = { 0 };
	ASSERT_ERR(dssh_parse_namelist_next(&name, &nl), DSSH_ERROR_PARSE);
	return TEST_PASS;
}

static int
test_namelist_next_exactly_64(void)
{
	/* Exactly 64 chars is valid */
	char name64[65];
	memset(name64, 'a', 64);
	name64[64] = '\0';

	uint8_t buf[128];
	buf[0] = 0;
	buf[1] = 0;
	buf[2] = 0;
	buf[3] = 64;
	memcpy(&buf[4], name64, 64);

	struct dssh_namelist_s nl = { 0 };
	dssh_parse_namelist(buf, 68, &nl);

	struct dssh_string_s name = { 0 };
	int64_t ret = dssh_parse_namelist_next(&name, &nl);
	ASSERT_OK(ret);
	ASSERT_EQ_U(name.length, 64);

	ASSERT_ERR(dssh_parse_namelist_next(&name, &nl), DSSH_ERROR_NOMORE);
	return TEST_PASS;
}

/* ----------------------------------------------------------------
 * serialize at offset  —  verify *pos advances correctly
 * ---------------------------------------------------------------- */

static int
test_serialize_multiple_fields(void)
{
	/* Serialize byte + uint32 + string into one buffer */
	uint8_t buf[32] = { 0 };
	size_t pos = 0;

	ASSERT_EQ(dssh_serialize_byte(0x05, buf, sizeof(buf), &pos), 0);
	ASSERT_EQ_U(pos, 1);

	ASSERT_EQ(dssh_serialize_uint32(0x00000001, buf, sizeof(buf), &pos), 0);
	ASSERT_EQ_U(pos, 5);

	uint8_t sdata[] = "hi";
	struct dssh_string_s str = { .value = sdata, .length = 2 };
	ASSERT_EQ(dssh_serialize_string(&str, buf, sizeof(buf), &pos), 0);
	ASSERT_EQ_U(pos, 11);

	/* Verify the whole buffer */
	uint8_t expected[] = {
		0x05,
		0x00, 0x00, 0x00, 0x01,
		0x00, 0x00, 0x00, 0x02, 'h', 'i'
	};
	ASSERT_MEM_EQ(buf, expected, 11);
	return TEST_PASS;
}

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

/* ----------------------------------------------------------------
 * test table
 * ---------------------------------------------------------------- */

static struct dssh_test_entry tests[] = {
	/* byte */
	{ "parse_byte_basic",               test_parse_byte_basic },
	{ "parse_byte_zero",                test_parse_byte_zero },
	{ "parse_byte_max",                 test_parse_byte_max },
	{ "parse_byte_empty_buffer",        test_parse_byte_empty_buffer },
	{ "serialize_byte",                 test_serialize_byte },
	{ "serialize_byte_overflow",        test_serialize_byte_overflow },
	{ "serialized_byte_length",         test_serialized_byte_length },
	{ "byte_roundtrip",                 test_byte_roundtrip },

	/* boolean */
	{ "parse_boolean_true",             test_parse_boolean_true },
	{ "parse_boolean_false",            test_parse_boolean_false },
	{ "parse_boolean_nonzero_is_true",  test_parse_boolean_nonzero_is_true },
	{ "parse_boolean_ff_is_true",       test_parse_boolean_ff_is_true },
	{ "parse_boolean_empty_buffer",     test_parse_boolean_empty_buffer },
	{ "serialize_boolean_true",         test_serialize_boolean_true },
	{ "serialize_boolean_false",        test_serialize_boolean_false },
	{ "serialize_boolean_overflow",     test_serialize_boolean_overflow },
	{ "serialized_boolean_length",      test_serialized_boolean_length },

	/* uint32 */
	{ "parse_uint32_basic",             test_parse_uint32_basic },
	{ "parse_uint32_zero",              test_parse_uint32_zero },
	{ "parse_uint32_max",               test_parse_uint32_max },
	{ "parse_uint32_short_buffer",      test_parse_uint32_short_buffer },
	{ "parse_uint32_empty_buffer",      test_parse_uint32_empty_buffer },
	{ "serialize_uint32",               test_serialize_uint32 },
	{ "serialize_uint32_overflow",      test_serialize_uint32_overflow },
	{ "serialized_uint32_length",       test_serialized_uint32_length },
	{ "uint32_roundtrip",              test_uint32_roundtrip },

	/* uint64 */
	{ "parse_uint64_basic",             test_parse_uint64_basic },
	{ "parse_uint64_zero",              test_parse_uint64_zero },
	{ "parse_uint64_max",               test_parse_uint64_max },
	{ "parse_uint64_short_buffer",      test_parse_uint64_short_buffer },
	{ "serialize_uint64",               test_serialize_uint64 },
	{ "serialize_uint64_overflow",      test_serialize_uint64_overflow },
	{ "serialized_uint64_length",       test_serialized_uint64_length },
	{ "uint64_roundtrip",              test_uint64_roundtrip },

	/* string */
	{ "parse_string_basic",             test_parse_string_basic },
	{ "parse_string_empty",             test_parse_string_empty },
	{ "parse_string_short_header",      test_parse_string_short_header },
	{ "parse_string_truncated_data",    test_parse_string_truncated_data },
	{ "parse_string_points_into_buf",   test_parse_string_points_into_buf },
	{ "serialize_string",               test_serialize_string },
	{ "serialize_string_empty",         test_serialize_string_empty },
	{ "serialize_string_overflow",      test_serialize_string_overflow },
	{ "serialized_string_length",       test_serialized_string_length },
	{ "string_roundtrip",              test_string_roundtrip },

	/* bytearray */
	{ "parse_bytearray_basic",          test_parse_bytearray_basic },
	{ "parse_bytearray_too_short",      test_parse_bytearray_too_short },
	{ "parse_bytearray_min_length",     test_parse_bytearray_min_length },
	{ "parse_bytearray_length_too_small", test_parse_bytearray_length_too_small },
	{ "serialize_bytearray",            test_serialize_bytearray },
	{ "serialize_bytearray_overflow",   test_serialize_bytearray_overflow },
	{ "serialized_bytearray_length",    test_serialized_bytearray_length },

	/* mpint */
	{ "parse_mpint_zero",               test_parse_mpint_zero },
	{ "parse_mpint_rfc4251_9a378f9b2e332a7", test_parse_mpint_rfc4251_9a378f9b2e332a7 },
	{ "parse_mpint_rfc4251_80",         test_parse_mpint_rfc4251_80 },
	{ "parse_mpint_rfc4251_neg1234",    test_parse_mpint_rfc4251_neg1234 },
	{ "parse_mpint_rfc4251_neg_deadbeef", test_parse_mpint_rfc4251_neg_deadbeef },
	{ "parse_mpint_unnecessary_leading_zero", test_parse_mpint_unnecessary_leading_zero },
	{ "parse_mpint_unnecessary_leading_ff", test_parse_mpint_unnecessary_leading_ff },
	{ "parse_mpint_single_byte",        test_parse_mpint_single_byte },
	{ "parse_mpint_truncated",          test_parse_mpint_truncated },
	{ "serialize_mpint",                test_serialize_mpint },
	{ "serialize_mpint_zero",           test_serialize_mpint_zero },
	{ "serialize_mpint_overflow",       test_serialize_mpint_overflow },
	{ "serialized_mpint_length",        test_serialized_mpint_length },
	{ "mpint_roundtrip",               test_mpint_roundtrip },

	/* namelist */
	{ "parse_namelist_basic",           test_parse_namelist_basic },
	{ "parse_namelist_single",          test_parse_namelist_single },
	{ "parse_namelist_empty",           test_parse_namelist_empty },
	{ "parse_namelist_leading_comma",   test_parse_namelist_leading_comma },
	{ "parse_namelist_trailing_comma",  test_parse_namelist_trailing_comma },
	{ "parse_namelist_double_comma",    test_parse_namelist_double_comma },
	{ "parse_namelist_control_char_space", test_parse_namelist_control_char_space },
	{ "parse_namelist_control_char_tab", test_parse_namelist_control_char_tab },
	{ "parse_namelist_high_byte",       test_parse_namelist_high_byte },
	{ "parse_namelist_high_byte_0x80",  test_parse_namelist_high_byte_0x80 },
	{ "parse_namelist_nul_byte",        test_parse_namelist_nul_byte },
	{ "serialize_namelist",             test_serialize_namelist },
	{ "serialize_namelist_empty",       test_serialize_namelist_empty },
	{ "serialize_namelist_overflow",    test_serialize_namelist_overflow },
	{ "serialized_namelist_length",     test_serialized_namelist_length },
	{ "namelist_roundtrip",            test_namelist_roundtrip },

	/* namelist_next iteration */
	{ "namelist_next_basic",            test_namelist_next_basic },
	{ "namelist_next_single",           test_namelist_next_single },
	{ "namelist_next_empty",            test_namelist_next_empty },
	{ "namelist_next_long_name",        test_namelist_next_long_name },
	{ "namelist_next_exactly_64",       test_namelist_next_exactly_64 },

	/* multi-field / offset */
	{ "serialize_multiple_fields",      test_serialize_multiple_fields },
	{ "serialize_at_nonzero_offset",    test_serialize_at_nonzero_offset },
	{ "serialize_overflow_at_offset",   test_serialize_overflow_at_offset },
};

DSSH_TEST_MAIN(tests)
