/* Same canonical source, independently compiled native/generic arithmetic. */
#include <stdint.h>
#include <inttypes.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "dssh_test.h"
#include "deucessh-crypto.h"

#ifdef DSSH_SNTRUP_DETERMINISTIC
 #include "sntrup_drbg.h"
 #define dssh_random sntrup_deterministic_random
#endif

/* No DSSH_TESTING: exercise the ordinary primitive/helper implementation. */
#include "../kex/sntrup761.c"

#ifdef DSSH_SNTRUP_DETERMINISTIC
 #undef dssh_random
#endif

#undef p
#undef q
#undef w

static uint32_t
next_word(uint32_t *state)
{
	uint64_t next = UINT64_C(1664525) * *state + UINT64_C(1013904223);
	uint32_t word = (uint32_t)(next & UINT32_MAX);
	*state = word;
	return word;
}

static int64_t
sample_signed64(uint32_t *state)
{
	uint64_t high = next_word(state);
	uint64_t low = next_word(state);
	uint64_t bits = (high << 32) | low;
	if (bits <= INT64_MAX) {
		int64_t positive = (int64_t)bits;
		return positive;
	}
	uint64_t magnitude = UINT64_MAX - bits;
	int64_t positive = (int64_t)magnitude;
	return -1 - positive;
}

static int64_t
floor_oracle(int64_t value, int64_t count)
{
	int64_t normalized = count % 64;
	if (normalized < 0)
		normalized += 64;
	if (normalized == 0)
		return value;
	if (normalized == 63)
		return value < 0 ? -1 : 0;
	int64_t divisor = INT64_C(1) << normalized;
	int64_t quotient = value / divisor;
	if (value < 0 && value % divisor != 0)
		quotient--;
	return quotient;
}

static int
test_masks16(void)
{
	bool valid = true;
	for (int32_t wide = INT16_MIN; wide <= INT16_MAX; wide++) {
		int16_t value = (int16_t)wide;
		valid = valid && crypto_int16_negative_mask(value) == (value < 0 ? -1 : 0)
		    && crypto_int16_nonzero_mask(value) == (value != 0 ? -1 : 0);
	}
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

static bool
minmax_pair(int32_t left, int32_t right)
{
	int32_t expected_low = left < right ? left : right;
	int32_t expected_high = left < right ? right : left;
	int32_t low = left, high = right;
	crypto_int32_minmax(&low, &high);
	return low == expected_low && high == expected_high
	    && crypto_int32_negative_mask(left) == (left < 0 ? -1 : 0);
}

static int
compare_unsigned(const void *left, const void *right)
{
	const uint32_t *left_word = left;
	const uint32_t *right_word = right;
	if (*left_word < *right_word)
		return -1;
	if (*left_word > *right_word)
		return 1;
	return 0;
}

static int
test_minmax_sort(void)
{
	const int32_t edges[] = {INT32_MIN, INT32_MIN + 1, -1073741824, -65536,
	    -32768, -129, -1, 0, 1, 127, 128, 32767, 65536, 1073741824, INT32_MAX - 1, INT32_MAX};
	bool valid = true;
	for (size_t first = 0; first < sizeof(edges) / sizeof(edges[0]); first++) {
		for (size_t second = 0; second < sizeof(edges) / sizeof(edges[0]); second++)
			valid = minmax_pair(edges[first], edges[second]) && valid;
	}
	uint32_t state = UINT32_C(0xc715bc39);
	for (uint32_t i = 0; i < UINT32_C(65536); i++) {
		int64_t left_wide = next_word(&state);
		int64_t right_wide = next_word(&state);
		left_wide -= INT64_C(2147483648);
		right_wide -= INT64_C(2147483648);
		int32_t left = (int32_t)left_wide;
		int32_t right = (int32_t)right_wide;
		valid = minmax_pair(left, right) && valid;
	}
	const size_t lengths[] = {0, 1, 2, 3, 4, 7, 16, 31, 64, 127, 256, 511, 760, 761};
	uint32_t actual[761], expected[761];
	for (unsigned int mode = 0; mode < 4; mode++) {
		for (size_t n = 0; n < sizeof(lengths) / sizeof(lengths[0]); n++) {
			size_t length = lengths[n];
			for (size_t i = 0; i < length; i++) {
				uint32_t value = next_word(&state);
				if (mode == 0)
					value = UINT32_MAX;
				if (mode == 1)
					value = i % 2 == 0 ? 0 : UINT32_MAX;
				if (mode == 2) {
					uint32_t index = (uint32_t)i;
					value = UINT32_MAX - index;
				}
				actual[i] = value;
				expected[i] = value;
			}
			qsort(expected, length, sizeof(expected[0]), compare_unsigned);
			/* Every count is bounded by the761-element owner. */
			long long count = (long long)length;
			crypto_sort_uint32(actual, count);
			valid = memcmp(actual, expected, length * sizeof(actual[0])) == 0 && valid;
		}
	}
	small sampled[761];
	for (unsigned int mode = 0; mode < 4; mode++) {
		for (size_t i = 0; i < 761; i++) {
			actual[i] = mode == 0 ? 0 : mode == 1 ? UINT32_MAX : next_word(&state);
			expected[i] = i < 286 ? actual[i] - actual[i] % 2
			    : actual[i] - actual[i] % 4 + 1;
		}
		qsort(expected, 761, sizeof(expected[0]), compare_unsigned);
		Short_fromlist(sampled, actual);
		unsigned int weight = 0;
		for (size_t i = 0; i < 761; i++) {
			int expected_value = (int)(expected[i] % 4);
			valid = sampled[i] == expected_value - 1 && valid;
			if (sampled[i] != 0)
				weight++;
		}
		valid = weight == 286 && valid;
	}
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

static bool
shift_value(int64_t value)
{
	bool valid = crypto_int64_bottombit_01(value) == (value % 2 != 0 ? 1 : 0)
	    && crypto_int64_bottombit_mask(value) == (value % 2 != 0 ? -1 : 0);
	for (int64_t count = -128; count <= 127; count++) {
		int64_t expected = floor_oracle(value, count);
		valid = crypto_int64_shrmod(value, count) == expected && valid;
		valid = crypto_int64_bitmod_01(value, count) == (expected % 2 != 0 ? 1 : 0) && valid;
	}
	const int64_t counts[] = {INT64_MIN, INT64_MIN + 1, INT64_MAX - 1, INT64_MAX};
	for (size_t i = 0; i < sizeof(counts) / sizeof(counts[0]); i++)
		valid = crypto_int64_shrmod(value, counts[i]) == floor_oracle(value, counts[i]) && valid;
	for (int64_t count = 0; count < 64; count++) {
		int64_t expected = floor_oracle(value, count);
		valid = crypto_int64_bitinrangepublicpos_mask(value, count) == (expected % 2 != 0 ? -1 : 0) && valid;
	}
	return valid;
}

static int
test_shifts64(void)
{
	const int64_t edges[] = {INT64_MIN, INT64_MIN + 1, -INT64_C(4294967296),
	    INT32_MIN, -129, -65, -64, -63, -3, -2, -1, 0, 1, 2, 3, 63, 64,
	    65, 127, 128, INT32_MAX, INT64_C(4294967296), INT64_MAX - 1, INT64_MAX};
	bool valid = true;
	for (size_t i = 0; i < sizeof(edges) / sizeof(edges[0]); i++)
		valid = shift_value(edges[i]) && valid;
	uint32_t state = UINT32_C(0x193dfe68);
	for (unsigned int i = 0; i < 256; i++)
		valid = shift_value(sample_signed64(&state)) && valid;
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

static int
test_field_reduction(void)
{
	bool valid = true;
	const uint32_t planes[] = {0, UINT32_C(0x00010000), UINT32_C(0x7fff0000),
	    UINT32_C(0x80000000), UINT32_C(0xfffe0000), UINT32_C(0xffff0000)};
	for (size_t n = 0; n < sizeof(planes) / sizeof(planes[0]); n++) {
		for (uint32_t low = 0; low < UINT32_C(65536); low++) {
			uint32_t bits = planes[n] | low;
			int64_t expected = bits;
			if (expected > INT32_MAX)
				expected -= INT64_C(4294967296);
			valid = sntrup_signed32(bits) == expected && valid;
		}
	}
	const int32_t edges[] = {INT32_MIN, INT32_MIN + 1, -65536, -32768,
	    -3, -1, 0, 1, 3, 32767, 65536, INT32_MAX - 1, INT32_MAX};
	for (size_t i = 0; i < sizeof(edges) / sizeof(edges[0]); i++) {
		for (unsigned int count = 0; count <= 33; count++) {
			int64_t expected = floor_oracle(edges[i], count);
			valid = sntrup_floor32(edges[i], count) == expected && valid;
		}
		valid = sntrup_floor32(edges[i], UINT_MAX) == (edges[i] < 0 ? -1 : 0) && valid;
	}
	for (int32_t value = INT16_MIN; value <= INT16_MAX; value++) {
		int16_t input = (int16_t)value;
		int32_t actual = F3_freeze(input);
		valid = actual >= -2 && actual <= 2 && (actual - value) % 3 == 0 && valid;
		if (value >= -16383 && value <= 16383) {
			int32_t expected = value % 3;
			if (expected > 1)
				expected -= 3;
			if (expected < -1)
				expected += 3;
			valid = actual == expected && valid;
		}
	}
	/* The largest reviewed reciprocal update is2*2295*2295. */
	for (int32_t input = -INT32_C(10534050); input <= INT32_C(10534050); input++) {
		int32_t expected = input % 4591;
		if (expected > 2295)
			expected -= 4591;
		if (expected < -2295)
			expected += 4591;
		valid = Fq_freeze(input) == expected && valid;
	}
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

static bool
division_matches(uint32_t input, uint16_t modulus)
{
	uint32_t quotient = 0;
	uint16_t remainder = 0;
	uint32_divmod_uint14(&quotient, &remainder, input, modulus);
	return quotient == input / modulus && remainder == input % modulus;
}

static int
test_division_encoding(void)
{
	const uint16_t moduli[] = {1, 2, 3, 255, 256, 257, 1531, 4591, 8191, 16383};
	const uint32_t edges[] = {0, 1, 2, 3, 255, 256, 65535, 65536,
	    UINT32_C(2147483647), UINT32_C(2147483648), UINT32_MAX - 1, UINT32_MAX};
	bool valid = true;
	uint32_t state = UINT32_C(0x783c419f);
	for (size_t n = 0; n < sizeof(moduli) / sizeof(moduli[0]); n++) {
		for (uint32_t i = 0; i < UINT32_C(65536); i++) {
			uint32_t input = next_word(&state);
			if (i < sizeof(edges) / sizeof(edges[0]))
				input = edges[i];
			valid = division_matches(input, moduli[n]) && valid;
		}
	}
	for (uint32_t value = 1; value < UINT32_C(16384); value++) {
		uint16_t modulus = (uint16_t)value;
		for (size_t i = 0; i < sizeof(edges) / sizeof(edges[0]); i++)
			valid = division_matches(edges[i], modulus) && valid;
	}
	small coefficients[761], decoded[761];
	unsigned char encoded[193];
	for (unsigned int pattern = 0; pattern < 81; pattern++) {
		unsigned int digits = pattern;
		for (size_t i = 0; i < 761; i++) {
			if (i % 4 == 0)
				digits = pattern;
			int digit = (int)(digits % 3);
			small coefficient = (small)(digit - 1);
			coefficients[i] = coefficient;
			digits /= 3;
		}
		memset(encoded, 0xa5, sizeof(encoded));
		Small_encode(encoded + 1, coefficients);
		Small_decode(decoded, encoded + 1);
		valid = memcmp(coefficients, decoded, sizeof(coefficients)) == 0
		    && encoded[0] == 0xa5 && encoded[192] == 0xa5 && valid;
		for (size_t i = 0; i < 191; i++) {
			unsigned int expected = 0, radix = 1;
			for (size_t j = 0; j < 4 && i * 4 + j < 761; j++) {
				unsigned int digit = (unsigned int)(coefficients[i * 4 + j] + 1);
				expected += digit * radix;
				radix *= 4;
			}
			valid = encoded[i + 1] == expected && valid;
		}
	}
	Fq rounded[761], unrounded[761];
	uint8_t packed[1009];
	for (int32_t value = -2295; value <= 2295; value += 3) {
		for (size_t i = 0; i < 761; i++) {
			Fq coefficient = (Fq)value;
			rounded[i] = coefficient;
		}
		memset(packed, 0xa5, sizeof(packed));
		Rounded_encode(packed + 1, rounded);
		Rounded_decode(unrounded, packed + 1);
		valid = memcmp(rounded, unrounded, sizeof(rounded)) == 0
		    && packed[0] == 0xa5 && packed[1008] == 0xa5 && valid;
	}
	uint8_t first[crypto_kem_sntrup761_CIPHERTEXTBYTES] = {0};
	uint8_t second[crypto_kem_sntrup761_CIPHERTEXTBYTES] = {0};
	for (unsigned int value = 0; value < 256; value++) {
		uint8_t octet = (uint8_t)value;
		second[sizeof(second) - 1] = octet;
		valid = Ciphertexts_diff_mask(first, second) == (value == 0 ? 0 : -1) && valid;
	}
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

static int
test_random_words(void)
{
	struct {
		uint32_t before;
		uint32_t words[761];
		uint32_t after;
	} storage;
	unsigned char *bytes = (unsigned char *)storage.words;
	uint32_t checked = 0;
	bool valid = true;
	for (unsigned pattern = 0; pattern < 256; pattern++) {
		storage.before = UINT32_C(0x13579bdf);
		storage.after = UINT32_C(0x2468ace0);
		for (size_t i = 0; i < 761; i++) {
			unsigned char first = (unsigned char)pattern; /*0..255. */
			unsigned char second = (unsigned char)(255 - pattern);
			unsigned char third = (unsigned char)(i % 256);
			unsigned char fourth = (unsigned char)((pattern + i / 256) % 256);
			bytes[4 * i] = first;
			bytes[4 * i + 1] = second;
			bytes[4 * i + 2] = third;
			bytes[4 * i + 3] = fourth;
		}
		decode_random_words(storage.words);
		for (size_t i = 0; i < 761; i++) {
			/* An independent base256 arithmetic oracle, without shifts or
			 * inspection of the native word's object representation. */
			uint64_t expected = pattern + UINT64_C(256) * (255 - pattern)
			    + UINT64_C(65536) * (i % 256) + UINT64_C(16777216) * ((pattern + i / 256) % 256);
			if (expected > UINT32_MAX)
				valid = false;
			else {
				uint32_t word = (uint32_t)expected;
				valid = storage.words[i] == word && valid;
			}
			checked++;
		}
		valid = storage.before == UINT32_C(0x13579bdf) && storage.after == UINT32_C(0x2468ace0) && valid;
	}
	int printed = printf("sntrup_words patterns=256 words=%" PRIu32 " guards=1 valid=%d\n", checked, valid);
	ASSERT_TRUE(valid && checked == UINT32_C(194816) && printed > 0);
	return TEST_PASS;
}

static int
test_kem(void)
{
	uint8_t public_key[crypto_kem_sntrup761_PUBLICKEYBYTES];
	uint8_t private_key[crypto_kem_sntrup761_SECRETKEYBYTES];
	uint8_t ciphertext[crypto_kem_sntrup761_CIPHERTEXTBYTES];
	uint8_t shared[crypto_kem_sntrup761_BYTES], recovered[crypto_kem_sntrup761_BYTES];
	bool valid = crypto_kem_sntrup761_keypair(public_key, private_key) == 0;
	if (valid)
		valid = crypto_kem_sntrup761_enc(ciphertext, shared, public_key) == 0;
	if (valid)
		valid = crypto_kem_sntrup761_dec(recovered, ciphertext, private_key) == 0
		    && memcmp(shared, recovered, sizeof(shared)) == 0;
	dssh_cleanse(public_key, sizeof(public_key));
	dssh_cleanse(private_key, sizeof(private_key));
	dssh_cleanse(ciphertext, sizeof(ciphertext));
	dssh_cleanse(shared, sizeof(shared));
	dssh_cleanse(recovered, sizeof(recovered));
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

#include "sntrup_kat_cases.h"

static struct dssh_test_entry tests[] = {
#ifdef DSSH_SNTRUP_DETERMINISTIC
	{"kat/generation", test_sntrup_kat_generation},
#endif
	{"kat/decapsulation", test_sntrup_kat_decapsulation},
	{"kat/encoding", test_sntrup_kat_encoding},
	{"kat/rejection", test_sntrup_kat_rejection},
	{"arithmetic/masks16", test_masks16},
	{"arithmetic/minmax_sort", test_minmax_sort},
	{"arithmetic/shifts64", test_shifts64},
	{"arithmetic/field_reduction", test_field_reduction},
	{"arithmetic/division_encoding", test_division_encoding},
	{"arithmetic/random_words", test_random_words},
	{"arithmetic/kem", test_kem},
};

DSSH_TEST_NO_CLEANUP
DSSH_TEST_MAIN(tests)
