#include <inttypes.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define DOUBLE_FRACTION_BITS 52
#define DOUBLE_EXPONENT_BIAS 1023
#define DOSBOX_ADJUSTMENT_LIMIT 4096

/*
 * QBSERIAL normalizes both registration names before KNK calculates the
 * key: trim outside spaces, collapse repeated spaces, and use mixed case.
 */
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

/*
 * KNK stores the BBS name on the first line of KNK.REG and the registered
 * person's name on the second.  It evaluates each square root on the x87
 * stack, applies QuickBASIC INT (round toward negative infinity), and stores
 * the resulting integer as an IEEE double.
 */
static uint64_t
registration_code(const char *bbs, const char *sysop)
{
	static const long double key_multiplier = 270439509615.0L;
	const unsigned char *character;
	long double value = 21.0L;

	for (character = (const unsigned char *)bbs;
	    *character != '\0'; character++)
		value += *character * 3;

	value = floorl(sqrtl(value * key_multiplier));

	for (character = (const unsigned char *)sysop;
	    *character != '\0'; character++)
		value += *character * 5;

	value = floorl(sqrtl(value * key_multiplier * 7.0L));
	return (uint64_t)value;
}

static unsigned
uint64_bits(uint64_t value)
{
	unsigned bits = 0;

	while (value != 0) {
		value >>= 1;
		bits++;
	}
	return bits;
}

static uint64_t
double_bits(double value)
{
	uint64_t bits;

	memcpy(&bits, &value, sizeof(bits));
	return bits;
}

static double
double_from_bits(uint64_t bits)
{
	double value;

	memcpy(&value, &bits, sizeof(value));
	return value;
}

/*
 * DOSBox 0.74.x stores each x87 register in a host double.  Its 80-bit load
 * drops the low significand bits instead of rounding them.  QuickBASIC VAL
 * first loads an 18-digit integer as an 80-bit value, so reproduce that
 * conversion before applying its similarly truncated power-of-ten constant.
 */
static double
dosbox_load_uint80(uint64_t value)
{
	uint64_t significand;
	uint64_t bits;
	unsigned exponent;

	exponent = uint64_bits(value) - 1;
	if (exponent > DOUBLE_FRACTION_BITS)
		significand = value >> (exponent - DOUBLE_FRACTION_BITS);
	else
		significand = value << (DOUBLE_FRACTION_BITS - exponent);
	bits = ((uint64_t)(exponent + DOUBLE_EXPONENT_BIAS)
	    << DOUBLE_FRACTION_BITS)
	    | (significand & ((UINT64_C(1) << DOUBLE_FRACTION_BITS) - 1));
	return double_from_bits(bits);
}

static int
dosbox_val_matches(
	uint64_t accumulator,
	unsigned decimal_places,
	uint64_t target_bits)
{
	/*
	 * These are QuickBASIC's 80-bit 10^-7 and 10^-8 table entries after
	 * DOSBox 0.74.x has truncated their significands to a host double.
	 */
	static const uint64_t negative_power_bits[] = {
		UINT64_C(0),
		UINT64_C(0),
		UINT64_C(0),
		UINT64_C(0),
		UINT64_C(0),
		UINT64_C(0),
		UINT64_C(0),
		UINT64_C(0x3e7ad7f29abcaf48),
		UINT64_C(0x3e45798ee2308c39),
	};
	volatile double multiplicand;
	volatile double multiplier;
	volatile double product;

	if (decimal_places < 7 || decimal_places > 8)
		return 0;
	multiplicand = dosbox_load_uint80(accumulator);
	multiplier = double_from_bits(negative_power_bits[decimal_places]);
	product = multiplicand * multiplier;
	return double_bits(product) == target_bits;
}

/*
 * KNK's possible keys have ten or eleven digits.  VAL pads the input to an
 * 18-digit integer and scales it by 10^-8 or 10^-7.  Search nearby decimal
 * accumulators for the closest spelling that DOSBox converts to the exact
 * double which KNK calculated.
 */
static int
format_dosbox_code(
	uint64_t code,
	char *text,
	size_t size)
{
	uint64_t accumulator;
	uint64_t base;
	uint64_t candidate;
	uint64_t fraction;
	uint64_t scale;
	uint64_t target_bits;
	unsigned decimal_digits;
	unsigned decimal_places;
	unsigned adjustment;
	int direction;

	decimal_digits = uint64_bits(code) == 0 ? 1 : 0;
	for (candidate = code; candidate != 0; candidate /= 10)
		decimal_digits++;
	if (decimal_digits > 18)
		return 0;
	decimal_places = 18 - decimal_digits;
	if (decimal_places < 7 || decimal_places > 8)
		return 0;

	scale = decimal_places == 7
	    ? UINT64_C(10000000) : UINT64_C(100000000);
	base = code * scale;
	target_bits = double_bits((double)code);

	for (adjustment = 0; adjustment <= DOSBOX_ADJUSTMENT_LIMIT;
	    adjustment++) {
		for (direction = 1; direction >= -1; direction -= 2) {
			if (adjustment == 0 && direction < 0)
				continue;
			if (direction > 0) {
				accumulator = base + adjustment;
			} else {
				if (base < adjustment)
					continue;
				accumulator = base - adjustment;
			}
			if (!dosbox_val_matches(
			    accumulator, decimal_places, target_bits))
				continue;

			candidate = accumulator / scale;
			fraction = accumulator % scale;
			if (fraction == 0)
				return snprintf(text, size, "%" PRIu64, candidate)
				    > 0;
			return snprintf(
			    text,
			    size,
			    "%" PRIu64 ".%0*" PRIu64,
			    candidate,
			    (int)decimal_places,
			    fraction) > 0;
		}
	}
	return 0;
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
	char dosbox_code[64];
	char standard_code[32];
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
	snprintf(standard_code, sizeof(standard_code), "%" PRIu64, code);
	printf("Code: %s\n", standard_code);
	if (!format_dosbox_code(code, dosbox_code, sizeof(dosbox_code))) {
		fprintf(stderr, "Unable to format a DOSBox registration code.\n");
		return 1;
	}
	if (strcmp(dosbox_code, standard_code) != 0)
		printf("DOSBox 0.74 Code: %s\n", dosbox_code);
	return 0;
}
