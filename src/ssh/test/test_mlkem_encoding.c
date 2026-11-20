/* Independent little-endian bitstream answers for all production field widths. */
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "kex/libcrux_mlkem768_sha3.h"

typedef libcrux_ml_kem_vector_portable_vector_type_PortableVector vector;

struct guarded_vector {
	uint8_t before[16];
	vector value;
	uint8_t after[16];
};

struct guarded_bytes {
	uint8_t before[16];
	uint8_t value[24];
	uint8_t after[16];
};

struct work {
	struct guarded_vector input;
	struct guarded_vector decoded;
	struct guarded_bytes answer;
	struct guarded_bytes encoded;
	vector saved_input;
	uint8_t saved_answer[24];
};

enum control { CONTROL_NONE, CONTROL_ANSWER, CONTROL_GUARD, CONTROL_INPUT };
static enum control control;
static uint64_t cases;
static uint64_t bytes;
static uint64_t fields;
static uint64_t guards;
static uint64_t inputs;

static void
pack(unsigned width, const vector *input, uint8_t *output)
{
	switch (width) {
	case 1:
		libcrux_ml_kem_vector_portable_serialize_1_0d(input, output);
		break;
	case 4:
		libcrux_ml_kem_vector_portable_serialize_4_0d(input, output);
		break;
	case 10:
		libcrux_ml_kem_vector_portable_serialize_10_0d(input, output);
		break;
	case 12:
		libcrux_ml_kem_vector_portable_serialize_12_0d(input, output);
		break;
	}
}

static void
unpack(unsigned width, uint8_t *input, vector *output)
{
	Eurydice_slice wire = Eurydice_array_to_slice(2 * width, input, uint8_t);
	switch (width) {
	case 1:
		libcrux_ml_kem_vector_portable_deserialize_1_0d(wire, output);
		break;
	case 4:
		libcrux_ml_kem_vector_portable_deserialize_4_0d(wire, output);
		break;
	case 10:
		libcrux_ml_kem_vector_portable_deserialize_10_0d(wire, output);
		break;
	case 12:
		libcrux_ml_kem_vector_portable_deserialize_12_0d(wire, output);
		break;
	}
}

static bool
check_guard(const uint8_t before[16], const uint8_t after[16])
{
	bool valid = true;
	for (size_t i = 0; i < 16; i++) {
		if (before[i] != 0xa5 || after[i] != 0x5a)
			valid = false;
	}
	guards++;
	return valid;
}

static void
prepare(struct work *work, unsigned width, uint32_t index)
{
	memset(work, 0xa5, sizeof(*work));
	memset(work->input.after, 0x5a, sizeof(work->input.after));
	memset(work->decoded.after, 0x5a, sizeof(work->decoded.after));
	memset(work->answer.after, 0x5a, sizeof(work->answer.after));
	memset(work->encoded.after, 0x5a, sizeof(work->encoded.after));
	uint32_t limit = UINT32_C(1) << width;
	uint32_t mask = limit - 1;
	for (size_t lane = 0; lane < 16; lane++) {
		uint32_t position = (uint32_t)lane;
		uint32_t value = (index + 17 * position) & mask;
		if (index >= limit) {
			uint32_t basis = index - limit;
			value = basis < 16 * width && position == basis / width
			    ? UINT32_C(1) << (basis % width) : 0;
			if (basis == 16 * width + 1)
				value = mask;
		}
		/* Every value is between0 and4095, on every supported int width. */
		int16_t coefficient = (int16_t)value;
		work->input.value.elements[lane] = coefficient;
	}
	memcpy(&work->saved_input, &work->input.value, sizeof(work->saved_input));
	memset(work->answer.value, 0, 2 * width);
	/* The reference sets one wire bit at a time; it uses no generated codec. */
	for (size_t lane = 0; lane < 16; lane++) {
		uint16_t value = (uint16_t)work->input.value.elements[lane];
		for (unsigned bit = 0; bit < width; bit++) {
			size_t position = lane * width + bit;
			uint8_t one = (uint8_t)((value >> bit) & 1U);
			uint8_t wire_bit = (uint8_t)(one << (position % 8));
			work->answer.value[position / 8] |= wire_bit;
		}
	}
	memcpy(work->saved_answer, work->answer.value, sizeof(work->saved_answer));
}

static bool
run_case(struct work *work, unsigned width, uint32_t index)
{
	prepare(work, width, index);
	pack(width, &work->input.value, work->encoded.value);
	if (width == 1 && index == 0) {
		if (control == CONTROL_ANSWER)
			work->encoded.value[0] ^= 1;
		if (control == CONTROL_GUARD)
			work->encoded.after[0] = 0;
		if (control == CONTROL_INPUT)
			work->saved_input.elements[0] ^= 1;
	}
	bool valid = true;
	for (size_t i = 0; i < 2 * width; i++) {
		if (work->encoded.value[i] != work->answer.value[i])
			valid = false;
		bytes++;
	}
	for (size_t i = 2 * width; i < sizeof(work->encoded.value); i++) {
		if (work->encoded.value[i] != 0xa5)
			valid = false;
	}
	valid = check_guard(work->input.before, work->input.after) && valid;
	valid = check_guard(work->encoded.before, work->encoded.after) && valid;
	valid = memcmp(&work->input.value, &work->saved_input, sizeof(work->saved_input)) == 0 && valid;
	inputs++;
	/* Decode independent reference bytes, so a shared round-trip error cannot pass. */
	unpack(width, work->answer.value, &work->decoded.value);
	for (size_t i = 0; i < 16; i++) {
		if (work->decoded.value.elements[i] != work->input.value.elements[i])
			valid = false;
		fields++;
	}
	valid = check_guard(work->decoded.before, work->decoded.after) && valid;
	valid = check_guard(work->answer.before, work->answer.after) && valid;
	valid = memcmp(work->answer.value, work->saved_answer, sizeof(work->saved_answer)) == 0 && valid;
	inputs++;
	cases++;
	return valid;
}

int
main(int argc, char **argv)
{
	if (argc == 2 && strcmp(argv[1], "--negative") == 0)
		control = CONTROL_ANSWER;
	else if (argc == 2 && strcmp(argv[1], "--negative-guard") == 0)
		control = CONTROL_GUARD;
	else if (argc == 2 && strcmp(argv[1], "--negative-input") == 0)
		control = CONTROL_INPUT;
	else if (argc != 1)
		return 2;
	static const unsigned widths[] = {1, 4, 10, 12};
	/* Reuse one bounded workspace; all input values are deterministic public fixtures. */
	static struct work storage;
	bool valid = true;
	bool io_failed = false;
	bool reported = false;
	for (size_t mode = 0; mode < sizeof(widths) / sizeof(widths[0]) && !io_failed; mode++) {
		unsigned width = widths[mode];
		uint32_t count = (UINT32_C(1) << width) + 16 * width + 2;
		for (uint32_t index = 0; index < count; index++) {
			if (!run_case(&storage, width, index)) {
				valid = false;
				if (!reported) {
					reported = true;
					if (fprintf(stderr, "ML-KEM encoding mismatch: width=%u case=%" PRIu32 "\n", width, index) < 0) {
						io_failed = true;
						break;
					}
				}
			}
		}
	}
	valid = valid && cases == 5578 && bytes == 127376 && fields == 89248 && guards == 22312 && inputs == 11156;
	int result = io_failed ? 2 : valid ? 0 : 1;
	if (printf("ML-KEM encodings: widths=4 cases=%" PRIu64 " bytes=%" PRIu64 " fields=%" PRIu64
	    " guards=%" PRIu64 " inputs=%" PRIu64 " valid=%u\n", cases, bytes, fields, guards, inputs, valid ? 1U : 0U) < 0
	    || fflush(stdout) != 0)
		result = 2;
	dssh_secure_zero(&storage, sizeof(storage));
	return result;
}
