/* Direct checks of private bundled glue against independent scalar models. */
#include <stdint.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "kex/libcrux_mlkem768_sha3.h"

#define CHECK(expr) do { \
	if (!(expr)) { \
		fprintf(stderr, "check failed at line %d: %s\n", __LINE__, #expr); \
		return 1; \
	} \
} while (0)

static int
test_ranges(void)
{
	static const size_t ends[][2] = {
		{0, 0}, {1, 0}, {SIZE_MAX, 0}, {SIZE_MAX, SIZE_MAX}
	};
	for (size_t i = 0; i < sizeof(ends) / sizeof(ends[0]); i++) {
		core_ops_range_Range_b3 range = {ends[i][0], ends[i][1]};
		for (size_t j = 0; j < 2; j++) {
			Option_b3 next = Eurydice_range_iter_next(&range, size_t, Option_b3);
			CHECK(next.tag == None);
			CHECK(range.start == ends[i][0]);
		}
	}
	core_ops_range_Range_b3 range = {SIZE_MAX - 1, SIZE_MAX};
	Option_b3 next = Eurydice_range_iter_next(&range, size_t, Option_b3);
	CHECK(next.tag == Some);
	CHECK(next.f0 == SIZE_MAX - 1);
	CHECK(range.start == SIZE_MAX);
	next = Eurydice_range_iter_next(&range, size_t, Option_b3);
	CHECK(next.tag == None);
	return 0;
}

static void
pack_11(uint8_t out[352], size_t index, uint32_t value)
{
	for (size_t bit = 0; bit < 11; bit++) {
		size_t offset = index * 11 + bit;
		uint8_t octet = (uint8_t)(((value >> bit) & 1U) << (offset % 8));
		out[offset / 8] |= octet;
	}
}

static int
test_serialization(void)
{
	/* Cover the full coefficient domain in every lane and the entire output. */
	for (uint32_t value = 0; value < 3329; value++) {
		libcrux_ml_kem_polynomial_PolynomialRingElement_f0 ring;
		uint8_t actual[354];
		uint8_t expected[352] = {0};
		memset(actual, 0xa5, sizeof(actual));
		for (size_t index = 0; index < 256; index++) {
			uint32_t lane = (uint32_t)index;
			uint32_t coefficient = (value + lane) % 3329;
			int16_t bounded = (int16_t)coefficient;
			ring.coefficients[index / 16].elements[index % 16] = bounded;
			uint32_t compressed = ((coefficient * UINT32_C(2048) + 1664) / 3329) & 2047;
			pack_11(expected, index, compressed);
		}
		libcrux_ml_kem_serialize_compress_then_serialize_11_e1(&ring, actual + 1);
		CHECK(actual[0] == 0xa5);
		CHECK(actual[353] == 0xa5);
		CHECK(memcmp(actual + 1, expected, sizeof(expected)) == 0);
	}
	return 0;
}

static int
test_shuffle_table(void)
{
	for (uint32_t mask = 0; mask < 256; mask++) {
		size_t offset = 0;
		const uint8_t *row = libcrux_ml_kem_vector_rej_sample_table_REJECTION_SAMPLE_SHUFFLE_TABLE[mask];
		for (uint32_t bit = 0; bit < 8; bit++) {
			if ((mask & (UINT32_C(1) << bit)) != 0) {
				CHECK(row[offset++] == 2 * bit);
				CHECK(row[offset++] == 2 * bit + 1);
			}
		}
		while (offset < 16) {
			CHECK(row[offset] == UINT8_MAX);
			offset++;
		}
	}
	return 0;
}

static int
test_zetas(void)
{
	for (uint32_t index = 0; index < 128; index++) {
		uint32_t reversed = 0;
		for (uint32_t bit = 0; bit < 7; bit++)
			reversed = 2 * reversed + ((index >> bit) & 1U);
		uint32_t power = 1;
		for (uint32_t exponent = 0; exponent < reversed; exponent++)
			power = power * 17 % 3329;
		uint32_t residue = power * (UINT32_C(65536) % 3329) % 3329;
		int32_t centered = (int32_t)residue;
		if (centered > 1664)
			centered -= 3329;
		CHECK(libcrux_ml_kem_polynomial_ZETAS_TIMES_MONTGOMERY_R[index] == centered);
	}
	return 0;
}

static int32_t
floor_power_two(int32_t value, unsigned int shift)
{
	if (shift >= 32)
		return value < 0 ? -1 : 0;
	int64_t divisor = INT64_C(1) << shift;
	int64_t wide = value;
	int64_t quotient = wide / divisor;
	if (wide < 0 && wide % divisor != 0)
		quotient--;
	/* Dividing an int32 value by a positive integer stays in int32 range. */
	int32_t result = (int32_t)quotient;
	return result;
}

static int
test_signed_glue(void)
{
	static const uint32_t upper[] = {0, UINT32_C(0x12340000),
		UINT32_C(0x80000000), UINT32_C(0xffff0000)};
	for (uint32_t low = 0; low < UINT32_C(65536); low++) {
		int32_t expected = (int32_t)low;
		if (expected > INT16_MAX)
			expected -= INT32_C(65536);
		for (size_t i = 0; i < sizeof(upper) / sizeof(upper[0]); i++)
			CHECK(dssh_mlkem_low_i16(upper[i] | low) == expected);
		static const unsigned int shifts[] = {15, 16, 26};
		for (size_t i = 0; i < sizeof(shifts) / sizeof(shifts[0]); i++)
			CHECK(dssh_mlkem_sar32(expected, shifts[i]) == floor_power_two(expected, shifts[i]));
	}
	static const int32_t boundary[] = {INT32_MIN, INT32_MIN + 1, -65537,
		-65536, -65535, -32769, -32768, -32767, -1, 0, 1,
		32767, 32768, 65535, 65536, 65537, INT32_MAX - 1, INT32_MAX};
	for (size_t i = 0; i < sizeof(boundary) / sizeof(boundary[0]); i++) {
		for (unsigned int shift = 0; shift < 34; shift++)
			CHECK(dssh_mlkem_sar32(boundary[i], shift) == floor_power_two(boundary[i], shift));
		CHECK(dssh_mlkem_sar32(boundary[i], UINT_MAX) == (boundary[i] < 0 ? -1 : 0));
	}
	return 0;
}

static int
test_reductions(void)
{
	for (int32_t value = INT16_MIN; value <= INT16_MAX; value++) {
		int16_t narrowed = (int16_t)value;
		int32_t expected = value % INT32_C(3329);
		if (expected < -1664)
			expected += 3329;
		if (expected > 1664)
			expected -= 3329;
		CHECK(libcrux_ml_kem_vector_portable_arithmetic_barrett_reduce_element(narrowed) == expected);
	}
	static const int32_t high[] = {-32768, -16384, -3329, -1, 0, 1, 3329, 16384, 32767};
	for (size_t i = 0; i < sizeof(high) / sizeof(high[0]); i++) {
		for (uint32_t low = 0; low < UINT32_C(65536); low++) {
			int64_t value = high[i] * INT64_C(65536) + low;
			/* Each high/low pair lies in the complete int32 range. */
			int32_t narrowed = (int32_t)value;
			int64_t correction = (value * INT64_C(62209)) % INT64_C(65536);
			if (correction < INT16_MIN)
				correction += INT64_C(65536);
			if (correction > INT16_MAX)
				correction -= INT64_C(65536);
			int64_t numerator = value - correction * INT64_C(3329);
			CHECK(numerator % INT64_C(65536) == 0);
			int64_t expected = numerator / INT64_C(65536);
			if (expected < INT16_MIN)
				expected += INT64_C(65536);
			if (expected > INT16_MAX)
				expected -= INT64_C(65536);
			CHECK(libcrux_ml_kem_vector_portable_arithmetic_montgomery_reduce_element(narrowed) == expected);
		}
	}
	return 0;
}

static int
test_sign_and_compression(void)
{
	for (int32_t start = INT16_MIN; start <= INT16_MAX; start += 16) {
		libcrux_ml_kem_vector_portable_vector_type_PortableVector vector;
		for (size_t lane = 0; lane < 16; lane++) {
			int32_t offset = (int32_t)lane;
			int16_t value = (int16_t)(start + offset);
			vector.elements[lane] = value;
		}
		libcrux_ml_kem_vector_portable_vector_type_PortableVector masks;
		libcrux_ml_kem_vector_portable_arithmetic_shift_right_94(&vector, &masks);
		libcrux_ml_kem_vector_portable_vector_type_PortableVector representatives;
		libcrux_ml_kem_vector_traits_to_unsigned_representative_db(&vector, &representatives);
		for (size_t lane = 0; lane < 16; lane++) {
			CHECK(masks.elements[lane] == (vector.elements[lane] < 0 ? -1 : 0));
			int32_t expected = vector.elements[lane];
			if (expected < 0)
				expected += 3329;
			CHECK(representatives.elements[lane] == expected);
		}
	}
	static const uint8_t counts[] = {4, 5, 10, 11};
	for (uint32_t value = 0; value < 3329; value++) {
		uint16_t coefficient = (uint16_t)value;
		uint32_t one_bit = ((2 * value + 1664) / 3329) & 1U;
		CHECK(libcrux_ml_kem_vector_portable_compress_compress_message_coefficient(coefficient) == one_bit);
		for (size_t i = 0; i < sizeof(counts) / sizeof(counts[0]); i++) {
			uint32_t scale = UINT32_C(1) << counts[i];
			uint32_t expected = ((scale * value + 1664) / 3329) % scale;
			CHECK(libcrux_ml_kem_vector_portable_compress_compress_ciphertext_coefficient(counts[i], coefficient) == expected);
		}
	}
	for (unsigned int n = 0; n <= UINT8_MAX; n++) {
		uint8_t count = (uint8_t)n;
		uint32_t expected = UINT32_MAX;
		if (n < 32) {
			uint64_t modulus = UINT64_C(1) << n;
			uint64_t residue = UINT32_MAX % modulus;
			expected = (uint32_t)residue;
		}
		CHECK(libcrux_ml_kem_vector_portable_arithmetic_get_n_least_significant_bits(count, UINT32_MAX) == expected);
	}
	return 0;
}

int
main(void)
{
	if (test_ranges() != 0 || test_serialization() != 0
	    || test_shuffle_table() != 0 || test_zetas() != 0
	    || test_signed_glue() != 0 || test_reductions() != 0
	    || test_sign_and_compression() != 0)
		return 1;
	puts("vendor glue: range boundaries, 3329 polynomial encodings, 256 shuffle rows and 128 zetas passed");
	puts("vendor arithmetic: 262144 low words, 65536 Barrett values, 589824 Montgomery values and all coefficient/sign domains passed");
	return 0;
}
