/* Direct matrix identities against schoolbook multiplication in Z_q[X]/(X^256+1).
 * The reference has no NTT, Montgomery reduction or generated constants. */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "kex/libcrux_mlkem768_sha3.h"

typedef libcrux_ml_kem_polynomial_PolynomialRingElement_f0 polynomial;

struct inputs {
	polynomial matrix[3][3];
	polynomial secret[3];
	polynomial error[3];
	polynomial error_ntt[3];
	polynomial public_vector[3];
	polynomial u[3];
	polynomial v;
	polynomial message;
};

struct guarded_three {
	uint8_t before[16];
	polynomial value[3];
	uint8_t after[16];
};

struct guarded_one {
	uint8_t before[16];
	polynomial value;
	uint8_t after[16];
};

struct work {
	struct inputs input;
	struct inputs saved;
	struct guarded_three vector_u;
	struct guarded_three vector_as;
	struct guarded_one element_v;
	struct guarded_one message;
	int64_t expected_u[3][256];
	int64_t expected_v[256];
	int64_t expected_message[256];
};

static size_t fields;
static size_t guards;
static size_t preserved;
enum control { CONTROL_NONE, CONTROL_ANSWER, CONTROL_GUARD, CONTROL_INPUT };
static enum control control;

static int16_t
coefficient(const polynomial *value, size_t index)
{
	return value->coefficients[index / 16].elements[index % 16];
}

static int64_t
normalize(int64_t value)
{
	int64_t remainder = value % 3329;
	if (remainder < 0)
		remainder += 3329;
	return remainder;
}

static void
add_product(int64_t out[256], const polynomial *left, const polynomial *right, int sign)
{
	/* At most three products are accumulated. Even allowing every input
	 * coefficient its full int16_t range, 3*256*32768^2 fits in int64_t. */
	for (size_t i = 0; i < 256; i++) {
		int64_t a = coefficient(left, i);
		for (size_t j = 0; j < 256; j++) {
			int64_t b = coefficient(right, j);
			int64_t product = sign * a * b;
			size_t index = i + j;
			if (index < 256)
				out[index] += product;
			else
				out[index - 256] -= product;
		}
	}
}

static void
fill(polynomial *out, unsigned pattern, unsigned tag, bool small)
{
	for (size_t i = 0; i < 256; i++) {
		/* The index is below256 on every size_t width. */
		uint32_t position = (uint32_t)i;
		uint32_t value = (197U * position + 101U * tag + 37U * pattern) % 3329U;
		if (pattern == 0)
			value = 0;
		else if (pattern == 1)
			value = position == 255U - tag % 256U ? 1 : 0;
		else if (pattern == 2)
			value = i % 2 == 0 ? 0 : 3328;
		else if (pattern == 3)
			value = 3328;
		/* Either canonical0..3328 or a centered coefficient in -2..2. */
		int32_t signed_value = (int32_t)value;
		if (small && pattern == 2)
			signed_value = i % 2 == 0 ? -2 : 2;
		else if (small && pattern == 3)
			signed_value = -2;
		else if (small && pattern >= 4)
			signed_value = (int32_t)(value % 5U) - 2;
		int16_t element = (int16_t)signed_value;
		out->coefficients[i / 16].elements[i % 16] = element;
	}
}

static void
prepare(struct work *work, unsigned pattern)
{
	struct inputs *in = &work->input;
	for (unsigned row = 0; row < 3; row++) {
		for (unsigned column = 0; column < 3; column++)
			fill(&in->matrix[row][column], pattern, 3 * row + column, false);
		fill(&in->secret[row], pattern, row + 11, true);
		fill(&in->error[row], pattern, row + 17, true);
		fill(&in->public_vector[row], pattern, row + 23, false);
		fill(&in->u[row], pattern, row + 29, false);
	}
	fill(&in->v, pattern, 31, false);
	fill(&in->message, pattern, 37, false);
	for (size_t i = 0; i < 256; i++)
		in->message.coefficients[i / 16].elements[i % 16] =
		    coefficient(&in->message, i) % 2 == 0 ? 0 : 1665;
	memset(work->expected_u, 0, sizeof(work->expected_u));
	for (size_t row = 0; row < 3; row++) {
		for (size_t column = 0; column < 3; column++)
			add_product(work->expected_u[row], &in->matrix[row][column], &in->secret[column], 1);
		for (size_t i = 0; i < 256; i++)
			work->expected_u[row][i] += coefficient(&in->error[row], i);
	}
	memset(work->expected_v, 0, sizeof(work->expected_v));
	memset(work->expected_message, 0, sizeof(work->expected_message));
	for (size_t row = 0; row < 3; row++) {
		add_product(work->expected_v, &in->public_vector[row], &in->secret[row], 1);
		add_product(work->expected_message, &in->secret[row], &in->u[row], -1);
	}
	for (size_t i = 0; i < 256; i++) {
		work->expected_v[i] += coefficient(&in->error[0], i);
		work->expected_v[i] += coefficient(&in->message, i);
		work->expected_message[i] += coefficient(&in->v, i);
	}
	/* Transform only after the independent coefficient-domain answers exist. */
	memcpy(in->error_ntt, in->error, sizeof(in->error_ntt));
	for (size_t row = 0; row < 3; row++) {
		for (size_t column = 0; column < 3; column++)
			libcrux_ml_kem_ntt_ntt_vector_u_9f(&in->matrix[row][column]);
		libcrux_ml_kem_ntt_ntt_binomially_sampled_ring_element_0f(&in->secret[row]);
		libcrux_ml_kem_ntt_ntt_binomially_sampled_ring_element_0f(&in->error_ntt[row]);
		libcrux_ml_kem_ntt_ntt_vector_u_9f(&in->public_vector[row]);
		libcrux_ml_kem_ntt_ntt_vector_u_9f(&in->u[row]);
	}
	memcpy(&work->saved, in, sizeof(*in));
}

static bool
check_guards(const uint8_t before[16], const uint8_t after[16])
{
	bool valid = true;
	for (size_t i = 0; i < 16; i++) {
		if (before[i] != 0xa5 || after[i] != 0x5a)
			valid = false;
	}
	guards++;
	return valid;
}

static bool
check_inputs(const struct work *work)
{
	preserved++;
	return memcmp(&work->input, &work->saved, sizeof(work->input)) == 0;
}

static bool
check_polynomial(const polynomial *actual, const int64_t expected[256], bool raw_inverse)
{
	bool valid = true;
	for (size_t i = 0; i < 256; i++) {
		int64_t value = coefficient(actual, i);
		/* The raw inverse of a standard-domain NTT has a128 factor.
		 * 128*3303 ==1 mod3329; no production reduction helper is used. */
		if (raw_inverse)
			value *= 3303;
		if (normalize(value) != normalize(expected[i]))
			valid = false;
		fields++;
	}
	return valid;
}

static bool
run_pattern(struct work *work, unsigned pattern)
{
	bool valid = true;
	prepare(work, pattern);
	if (pattern == 0 && control == CONTROL_ANSWER)
		work->expected_u[0][0] = 1;
	if (pattern == 0 && control == CONTROL_INPUT)
		work->saved.message.coefficients[0].elements[0] = 1;
	memset(&work->vector_u, 0xa5, sizeof(work->vector_u));
	memset(work->vector_u.after, 0x5a, sizeof(work->vector_u.after));
	memset(&work->vector_as, 0xa5, sizeof(work->vector_as));
	memset(work->vector_as.after, 0x5a, sizeof(work->vector_as.after));
	memset(&work->element_v, 0xa5, sizeof(work->element_v));
	memset(work->element_v.after, 0x5a, sizeof(work->element_v.after));
	memset(&work->message, 0xa5, sizeof(work->message));
	memset(work->message.after, 0x5a, sizeof(work->message.after));
	struct inputs *in = &work->input;
	libcrux_ml_kem_matrix_compute_vector_u_59(in->matrix, in->secret, in->error, work->vector_u.value);
	valid = check_inputs(work) && valid;
	for (size_t row = 0; row < 3; row++)
		valid = check_polynomial(&work->vector_u.value[row], work->expected_u[row], false) && valid;
	valid = check_guards(work->vector_u.before, work->vector_u.after) && valid;
	libcrux_ml_kem_matrix_compute_As_plus_e_60(in->matrix, in->secret, in->error_ntt, work->vector_as.value);
	valid = check_inputs(work) && valid;
	for (size_t row = 0; row < 3; row++) {
		libcrux_ml_kem_invert_ntt_invert_ntt_montgomery_f6(&work->vector_as.value[row]);
		valid = check_polynomial(&work->vector_as.value[row], work->expected_u[row], true) && valid;
	}
	valid = check_guards(work->vector_as.before, work->vector_as.after) && valid;
	libcrux_ml_kem_matrix_compute_ring_element_v_54(
	    in->public_vector, in->secret, &in->error[0], &in->message, &work->element_v.value);
	valid = check_inputs(work) && valid;
	valid = check_polynomial(&work->element_v.value, work->expected_v, false) && valid;
	valid = check_guards(work->element_v.before, work->element_v.after) && valid;
	libcrux_ml_kem_matrix_compute_message_b3(&in->v, in->secret, in->u, &work->message.value);
	valid = check_inputs(work) && valid;
	valid = check_polynomial(&work->message.value, work->expected_message, false) && valid;
	if (pattern == 0 && control == CONTROL_GUARD)
		work->message.after[0] = 0;
	valid = check_guards(work->message.before, work->message.after) && valid;
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
	/* All dimensions are fixed by ML-KEM-768. Reuse one bounded workspace. */
	static struct work storage;
	struct work *work = &storage;
	bool valid = normalize(128 * INT64_C(3303)) == 1;
	bool io_failed = false;
	for (unsigned pattern = 0; pattern < 6; pattern++) {
		if (!run_pattern(work, pattern)) {
			valid = false;
			if (fprintf(stderr, "ML-KEM polynomial relation mismatch: pattern=%u\n", pattern) < 0) {
				io_failed = true;
				break;
			}
		}
	}
	valid = valid && fields == 12288 && guards == 24 && preserved == 24;
	int result = io_failed ? 2 : valid ? 0 : 1;
	if (printf("ML-KEM polynomial relations: patterns=6 fields=%zu guards=%zu inputs=%zu valid=%u\n",
	    fields, guards, preserved, valid ? 1U : 0U) < 0 || fflush(stdout) != 0)
		result = 2;
	dssh_secure_zero(work, sizeof(*work));
	return result;
}
