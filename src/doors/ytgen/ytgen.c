#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/*
 * Positive Microsoft Binary Format double.  BRUN30 stores a 55-bit
 * fraction with an implicit leading one, for 56 bits of precision total.
 */
typedef struct {
	uint64_t mantissa;
	unsigned exponent;
} mbf64_t;

#define MBF_MANT_BITS 56
#define MBF_HIDDEN    (UINT64_C(1) << 55)
#define LIMB_BITS     28
#define LIMB_MASK     ((UINT64_C(1) << LIMB_BITS) - 1)

static unsigned
uint_bits(uint64_t value)
{
	unsigned bits = 0;

	while (value != 0) {
		value >>= 1;
		bits++;
	}
	return bits;
}

static uint64_t
round_quotient(uint64_t quotient, uint64_t remainder, uint64_t divisor)
{
	uint64_t half = divisor >> 1;

	if (remainder > half
	    || (remainder == half && (quotient & UINT64_C(1)) != 0))
		quotient++;
	return quotient;
}

static mbf64_t
mbf_from_uint(uint64_t value)
{
	mbf64_t result = { 0, 0 };
	unsigned bits;

	if (value == 0)
		return result;

	bits = uint_bits(value);
	result.exponent = 128 + bits;
	if (bits <= MBF_MANT_BITS) {
		result.mantissa = value << (MBF_MANT_BITS - bits);
	} else {
		unsigned shift = bits - MBF_MANT_BITS;
		uint64_t divisor = UINT64_C(1) << shift;

		result.mantissa = round_quotient(
		    value >> shift, value & (divisor - 1), divisor);
		if (result.mantissa == (UINT64_C(1) << MBF_MANT_BITS)) {
			result.mantissa >>= 1;
			result.exponent++;
		}
	}
	return result;
}

static uint64_t
shift_right_sticky(uint64_t value, unsigned shift)
{
	uint64_t discarded;

	if (shift == 0)
		return value;
	if (shift >= 64)
		return value != 0;
	discarded = value & ((UINT64_C(1) << shift) - 1);
	value >>= shift;
	if (discarded != 0)
		value |= 1;
	return value;
}

/* Positive MBF64 addition, including BRUN30's round-to-nearest-even step. */
static mbf64_t
mbf_add(mbf64_t left, mbf64_t right)
{
	mbf64_t result;
	uint64_t extended;
	uint64_t smaller;
	uint64_t remainder;
	unsigned difference;

	if (left.mantissa == 0)
		return right;
	if (right.mantissa == 0)
		return left;
	if (right.exponent > left.exponent) {
		result = left;
		left = right;
		right = result;
	}

	result.exponent = left.exponent;
	difference = left.exponent - right.exponent;
	extended = left.mantissa << 3;
	smaller = shift_right_sticky(right.mantissa << 3, difference);
	extended += smaller;

	if (extended >= (UINT64_C(1) << 59)) {
		extended = shift_right_sticky(extended, 1);
		result.exponent++;
	}

	result.mantissa = extended >> 3;
	remainder = extended & 7;
	result.mantissa = round_quotient(result.mantissa, remainder, 8);
	if (result.mantissa == (UINT64_C(1) << MBF_MANT_BITS)) {
		result.mantissa >>= 1;
		result.exponent++;
	}
	return result;
}

/*
 * Positive MBF64 multiply.  Four base-2^28 limbs hold the full 112-bit
 * product, avoiding dependence on a compiler-specific 128-bit integer.
 */
static mbf64_t
mbf_multiply(mbf64_t left, mbf64_t right)
{
	mbf64_t result = { 0, 0 };
	uint64_t a0, a1, b0, b1;
	uint64_t limb[4];
	uint64_t work;
	uint64_t quotient;
	int shift;
	int sticky;
	int guard;

	if (left.mantissa == 0 || right.mantissa == 0)
		return result;

	a0 = left.mantissa & LIMB_MASK;
	a1 = left.mantissa >> LIMB_BITS;
	b0 = right.mantissa & LIMB_MASK;
	b1 = right.mantissa >> LIMB_BITS;

	work = a0 * b0;
	limb[0] = work & LIMB_MASK;
	work >>= LIMB_BITS;

	work += a0 * b1 + a1 * b0;
	limb[1] = work & LIMB_MASK;
	work >>= LIMB_BITS;

	work += a1 * b1;
	limb[2] = work & LIMB_MASK;
	limb[3] = work >> LIMB_BITS;

	shift = (limb[3] & (UINT64_C(1) << 27)) != 0 ? 56 : 55;
	if (shift == 56) {
		quotient = limb[2] | (limb[3] << 28);
		guard = (limb[1] & (UINT64_C(1) << 27)) != 0;
		sticky = limb[0] != 0
		    || (limb[1] & ((UINT64_C(1) << 27) - 1)) != 0;
	} else {
		quotient = (limb[1] >> 27)
		    | (limb[2] << 1) | (limb[3] << 29);
		guard = (limb[1] & (UINT64_C(1) << 26)) != 0;
		sticky = limb[0] != 0
		    || (limb[1] & ((UINT64_C(1) << 26) - 1)) != 0;
	}

	if (guard && (sticky || (quotient & UINT64_C(1)) != 0))
		quotient++;

	result.exponent = left.exponent + right.exponent + shift - 184;
	result.mantissa = quotient;
	if (result.mantissa == (UINT64_C(1) << MBF_MANT_BITS)) {
		result.mantissa >>= 1;
		result.exponent++;
	}
	return result;
}

/* Positive MBF64 division, generating all 56 quotient bits plus guard/sticky. */
static mbf64_t
mbf_divide(mbf64_t numerator, mbf64_t denominator)
{
	mbf64_t result = { 0, 0 };
	uint64_t remainder;
	uint64_t quotient;
	uint64_t normalized;
	int adjusted;
	int bit;
	int guard;

	if (numerator.mantissa == 0)
		return result;

	adjusted = numerator.mantissa < denominator.mantissa;
	normalized = numerator.mantissa << adjusted;
	remainder = normalized - denominator.mantissa;
	quotient = MBF_HIDDEN;

	for (bit = 54; bit >= 0; bit--) {
		remainder <<= 1;
		if (remainder >= denominator.mantissa) {
			remainder -= denominator.mantissa;
			quotient |= UINT64_C(1) << bit;
		}
	}

	remainder <<= 1;
	guard = remainder >= denominator.mantissa;
	if (guard)
		remainder -= denominator.mantissa;
	if (guard && (remainder != 0 || (quotient & UINT64_C(1)) != 0))
		quotient++;

	result.exponent = 129 + numerator.exponent
	    - denominator.exponent - adjusted;
	result.mantissa = quotient;
	if (result.mantissa == (UINT64_C(1) << MBF_MANT_BITS)) {
		result.mantissa >>= 1;
		result.exponent++;
	}
	return result;
}

/*
 * Port of BRUN30 image B9E8h.  It consumes the high 24 significand bits,
 * performs 25 restoring-square-root steps, and leaves a 24-bit MBF seed.
 */
static mbf64_t
brun_sqrt_seed(mbf64_t value)
{
	mbf64_t result;
	uint32_t radicand;
	uint32_t remainder = 0;
	uint32_t root = 0;
	uint64_t combined;
	unsigned count;
	unsigned odd;
	unsigned carry;
	uint16_t low;

	odd = value.exponent & 1;
	radicand = (uint32_t)(value.mantissa >> 32) << 8;
	if (odd)
		radicand >>= 1;
	result.exponent = (value.exponent >> 1) + 0x40 + odd;

	for (count = 0; count < 25; count++) {
		combined = ((uint64_t)remainder << 32) | radicand;
		combined <<= 2;
		remainder = (uint32_t)(combined >> 32);
		radicand = (uint32_t)combined;

		root = (root << 1) | 1;
		low = (uint16_t)root;
		if (remainder < root) {
			low--;
		} else {
			remainder -= root;
			low++;
		}
		root = (root & UINT32_C(0xffff0000)) | low;
	}

	carry = (root >> 1) & 1;
	root >>= 2;
	root += carry;
	result.mantissa = MBF_HIDDEN
	    | ((uint64_t)(root & UINT32_C(0x007fffff)) << 32);
	return result;
}

/*
 * Port of BRUN30 SQR_DOUBLE at image BE7Eh: one B9E8h seed followed by
 * exactly two MBF-rounded Newton iterations.
 */
static mbf64_t
brun_sqrt(mbf64_t value)
{
	mbf64_t guess;
	unsigned iteration;

	if (value.mantissa == 0)
		return value;
	guess = brun_sqrt_seed(value);
	for (iteration = 0; iteration < 2; iteration++) {
		guess = mbf_add(guess, mbf_divide(value, guess));
		guess.exponent--; /* BRUN divides by two by decrementing exponent. */
	}
	return guess;
}

static uint64_t
mbf_floor_uint(mbf64_t value)
{
	unsigned fractional_bits;

	if (value.mantissa == 0 || value.exponent <= 128)
		return 0;
	if (value.exponent >= 184)
		return value.mantissa << (value.exponent - 184);
	fractional_bits = 184 - value.exponent;
	if (fractional_bits >= MBF_MANT_BITS)
		return 0;
	return value.mantissa >> fractional_bits;
}

static mbf64_t
brun_int(mbf64_t value)
{
	return mbf_from_uint(mbf_floor_uint(value));
}

static void
canonicalize(char *value)
{
	unsigned char *source = (unsigned char *)value;
	unsigned char *destination = (unsigned char *)value;
	unsigned char *end;
	int lowercase_following = 0;
	unsigned char input;
	unsigned char output;

	while (*source == ' ')
		source++;
	end = source + strlen((char *)source);
	while (end > source && end[-1] == ' ')
		end--;

	while (source < end) {
		input = *source++;
		if (input == ' ' && destination > (unsigned char *)value
		    && destination[-1] == ' ')
			continue;

		output = input;
		if (!lowercase_following && input >= 'a' && input <= 'z')
			output = input - ('a' - 'A');
		else if (lowercase_following && input >= 'A' && input <= 'Z')
			output = input + ('a' - 'A');
		*destination++ = output;

		lowercase_following = input == '\''
		    || (input >= 'A' && input <= 'Z')
		    || (input >= 'a' && input <= 'z');
	}
	*destination = '\0';
}

static uint64_t
registration_code(const char *bbs, const char *sysop)
{
	static const uint64_t key_multiplier = UINT64_C(111355062389);
	const unsigned char *character;
	mbf64_t value = mbf_from_uint(21);
	mbf64_t multiplier = mbf_from_uint(key_multiplier);

	for (character = (const unsigned char *)bbs;
	    *character != '\0'; character++)
		value = mbf_add(value, mbf_from_uint(*character * 3));

	value = mbf_multiply(multiplier, value);
	value = brun_int(brun_sqrt(value));

	for (character = (const unsigned char *)sysop;
	    *character != '\0'; character++)
		value = mbf_add(value, mbf_from_uint(*character * 5));

	value = mbf_multiply(multiplier, value);
	value = mbf_multiply(value, mbf_from_uint(7));
	value = brun_int(brun_sqrt(value));
	return mbf_floor_uint(value);
}

static int
read_name(const char *prompt, char *value, size_t size)
{
	size_t length;

	fputs(prompt, stdout);
	fflush(stdout);
	if (fgets(value, (int)size, stdin) == NULL)
		return 0;

	length = strlen(value);
	if (length != 0 && value[length - 1] == '\n')
		value[--length] = '\0';
	if (length != 0 && value[length - 1] == '\r')
		value[--length] = '\0';
	return length <= 40;
}

int
main(int argc, char **argv)
{
	char sysop[42];
	char bbs[42];
	uint64_t code;

	if (argc == 3) {
		if (strlen(argv[1]) > 40 || strlen(argv[2]) > 40) {
			fprintf(stderr, "Names are limited to 40 bytes.\n");
			return 1;
		}
		strcpy(sysop, argv[1]);
		strcpy(bbs, argv[2]);
	} else if (argc == 1) {
		if (!read_name("Sysop Name (40 chars max): ",
		    sysop, sizeof(sysop))
		    || !read_name("BBS Name (40 chars max): ",
		    bbs, sizeof(bbs)))
			return 1;
	} else {
		fprintf(stderr, "usage: %s [sysop-name bbs-name]\n", argv[0]);
		return 1;
	}

	canonicalize(sysop);
	canonicalize(bbs);
	code = registration_code(bbs, sysop);
	printf("Code: %" PRIu64 "\n", code);
	return 0;
}
