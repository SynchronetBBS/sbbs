/* Direct vector results/aliasing against independent wide-integer formulas. */
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "kex/libcrux_mlkem768_sha3.h"

typedef libcrux_ml_kem_vector_portable_vector_type_PortableVector vector;

enum operation { OP_ARRAY, OP_ADD, OP_SUB, OP_MUL, OP_AND, OP_COND, OP_BARRETT,
	OP_MONT, OP_SHIFT, OP_TRAIT_MONT, OP_STANDARD, OP_UNSIGNED, OP_MESSAGE,
	OP_COMPRESS_1, OP_COMPRESS_4, OP_COMPRESS_10, OP_DECOMPRESS_4, OP_DECOMPRESS_10, OP_COUNT };
enum control { CONTROL_NONE, CONTROL_ANSWER, CONTROL_GUARD, CONTROL_INPUT };

struct guarded_vector {
	uint8_t before[16];
	vector value;
	uint8_t after[16];
};

struct workspace {
	struct guarded_vector left;
	struct guarded_vector right;
	struct guarded_vector output;
	vector saved_left;
	vector saved_right;
	int64_t expected[16];
};

static enum control control;
static uint64_t cases;
static uint64_t fields;
static uint64_t guards;
static uint64_t inputs;

static int64_t
montgomery(int64_t value, int64_t constant)
{
	int64_t product = value * constant;
	int64_t correction = (product * 62209) % 65536;
	if (correction < -32768)
		correction += 65536;
	if (correction > 32767)
		correction -= 65536;
	return (product - correction * 3329) / 65536;
}

static int64_t
reference(enum operation op, int32_t left, int32_t right, int16_t constant)
{
	int64_t value = left;
	switch (op) {
	case OP_ARRAY:
		return value;
	case OP_ADD:
		return value + right;
	case OP_SUB:
		return value - right;
	case OP_MUL:
		return value * constant;
	case OP_AND: {
		uint16_t bits = (uint16_t)left;
		return bits & 3329U;
	}
	case OP_COND:
		return value >= 3329 ? value - 3329 : value;
	case OP_BARRETT:
		value %= 3329;
		if (value < -1664)
			value += 3329;
		if (value > 1664)
			value -= 3329;
		return value;
	case OP_MONT:
	case OP_TRAIT_MONT:
		return montgomery(value, constant);
	case OP_SHIFT:
		return value < 0 ? -1 : 0;
	case OP_STANDARD:
		/* R^2 modulo q, independently computed in the reference. */
		return montgomery(value, (INT64_C(1) << 32) % 3329);
	case OP_UNSIGNED:
		return value < 0 ? value + 3329 : value;
	case OP_MESSAGE:
		return value * 1665;
	case OP_COMPRESS_1:
		return ((value * 2 + 1664) / 3329) % 2;
	case OP_COMPRESS_4:
		return ((value * 16 + 1664) / 3329) % 16;
	case OP_COMPRESS_10:
		return ((value * 1024 + 1664) / 3329) % 1024;
	case OP_DECOMPRESS_4:
		return (value * 3329 + 8) / 16;
	case OP_DECOMPRESS_10:
		return (value * 3329 + 512) / 1024;
	case OP_COUNT:
		break;
	}
	return INT64_MAX;
}

static void
evaluate(enum operation op, const vector *left, vector *right, int16_t constant, vector *out)
{
	switch (op) {
	case OP_ARRAY:
		libcrux_ml_kem_vector_portable_from_i16_array_0d(
		    Eurydice_array_to_slice(16, left->elements, int16_t), out);
		break;
	case OP_ADD:
		libcrux_ml_kem_vector_portable_add_0d(left, right, out);
		break;
	case OP_SUB:
		libcrux_ml_kem_vector_portable_sub_0d(left, right, out);
		break;
	case OP_MUL:
		libcrux_ml_kem_vector_portable_multiply_by_constant_0d(left, constant, out);
		break;
	case OP_AND:
		libcrux_ml_kem_vector_portable_bitwise_and_with_constant_0d(left, constant, out);
		break;
	case OP_COND:
		libcrux_ml_kem_vector_portable_cond_subtract_3329_0d(left, out);
		break;
	case OP_BARRETT:
		libcrux_ml_kem_vector_portable_barrett_reduce_0d(left, out);
		break;
	case OP_MONT:
		libcrux_ml_kem_vector_portable_montgomery_multiply_by_constant_0d(left, constant, out);
		break;
	case OP_SHIFT:
		libcrux_ml_kem_vector_portable_shift_right_0d_19(left, out);
		break;
	case OP_TRAIT_MONT:
		libcrux_ml_kem_vector_traits_montgomery_multiply_fe_67(left, constant, out);
		break;
	case OP_STANDARD:
		libcrux_ml_kem_vector_traits_to_standard_domain_59(left, out);
		break;
	case OP_UNSIGNED:
		libcrux_ml_kem_vector_traits_to_unsigned_representative_db(left, out);
		break;
	case OP_MESSAGE:
		libcrux_ml_kem_vector_traits_decompress_1_63(left, out);
		break;
	case OP_COMPRESS_1:
		libcrux_ml_kem_vector_portable_compress_1_0d(left, out);
		break;
	case OP_COMPRESS_4:
		libcrux_ml_kem_vector_portable_compress_0d_281(left, out);
		break;
	case OP_COMPRESS_10:
		libcrux_ml_kem_vector_portable_compress_0d_28(left, out);
		break;
	case OP_DECOMPRESS_4:
		libcrux_ml_kem_vector_portable_decompress_ciphertext_coefficient_0d_5a1(left, out);
		break;
	case OP_DECOMPRESS_10:
		libcrux_ml_kem_vector_portable_decompress_ciphertext_coefficient_0d_5a(left, out);
		break;
	case OP_COUNT:
		break;
	}
}

static bool
guard(const struct guarded_vector *value)
{
	bool valid = true;
	for (size_t i = 0; i < 16; i++) {
		if (value->before[i] != 0xa5 || value->after[i] != 0x5a)
			valid = false;
	}
	guards++;
	return valid;
}

static bool
run_case(struct workspace *work, enum operation op, unsigned alias, int32_t base)
{
	memset(work, 0xa5, sizeof(*work));
	memset(work->left.after, 0x5a, sizeof(work->left.after));
	memset(work->right.after, 0x5a, sizeof(work->right.after));
	memset(work->output.after, 0x5a, sizeof(work->output.after));
	int16_t constant = (int16_t)(base % 2 == 0 ? -758 : 1441);
	if (op == OP_MUL)
		constant = -1600;
	if (op == OP_AND)
		constant = 3329;
	for (size_t i = 0; i < 16; i++) {
		int32_t lane = (int32_t)i;
		int32_t left = base + lane;
		if (left > 32767)
			left -= 65536;
		if (op == OP_MUL)
			left = (left + 32768) % 5 - 2;
		if (op == OP_COND)
			left = (left + 32768) % 4096;
		if (op == OP_MESSAGE)
			left = (left + 32768) % 2;
		if (op >= OP_COMPRESS_1 && op <= OP_COMPRESS_10)
			left = (base + lane) % 3329;
		if (op == OP_DECOMPRESS_4)
			left = (base + lane) % 16;
		if (op == OP_DECOMPRESS_10)
			left = (base + lane) % 1024;
		int32_t right = op == OP_ADD ? -(left / 2) : left / 2;
		/* Generated left/right values and their selected operation outputs
		 * stay in int16; the reference keeps its result wide regardless. */
		int16_t left_value = (int16_t)left;
		int16_t right_value = (int16_t)right;
		work->left.value.elements[i] = left_value;
		work->right.value.elements[i] = right_value;
		work->expected[i] = reference(op, left, right, constant);
	}
	memcpy(&work->saved_left, &work->left.value, sizeof(work->saved_left));
	memcpy(&work->saved_right, &work->right.value, sizeof(work->saved_right));
	vector *out = alias == 0 ? &work->output.value : alias == 1 ? &work->left.value : &work->right.value;
	evaluate(op, &work->left.value, &work->right.value, constant, out);
	if (op == OP_ARRAY && alias == 0 && base == -32768) {
		if (control == CONTROL_ANSWER)
			out->elements[0] ^= 1;
		if (control == CONTROL_GUARD)
			work->output.after[0] = 0;
		if (control == CONTROL_INPUT)
			work->saved_left.elements[0] ^= 1;
	}
	bool valid = true;
	for (size_t i = 0; i < 16; i++) {
		if (out->elements[i] != work->expected[i])
			valid = false;
		fields++;
	}
	valid = guard(&work->left) && valid;
	valid = guard(&work->right) && valid;
	valid = guard(&work->output) && valid;
	if (alias != 1) {
		valid = memcmp(&work->left.value, &work->saved_left, sizeof(work->saved_left)) == 0 && valid;
		inputs++;
	}
	if (alias != 2) {
		valid = memcmp(&work->right.value, &work->saved_right, sizeof(work->saved_right)) == 0 && valid;
		inputs++;
	}
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
	static struct workspace work;
	bool valid = true;
	bool io_failed = false;
	bool reported = false;
	for (enum operation op = OP_ARRAY; op < OP_COUNT && !io_failed; op++) {
		unsigned aliases = op == OP_ADD || op == OP_SUB ? 3 : 2;
		int32_t first = op >= OP_COMPRESS_1 ? 0 : -32768;
		int32_t limit = op <= OP_MESSAGE ? 32768 : op <= OP_COMPRESS_10 ? 3329 : op == OP_DECOMPRESS_4 ? 16 : 1024;
		for (unsigned alias = 0; alias < aliases && !io_failed; alias++) {
			for (int32_t value = first; value < limit; value++) {
				if (!run_case(&work, op, alias, value)) {
					valid = false;
					if (!reported) {
						reported = true;
						int op_value = (int)op;
						if (fprintf(stderr, "ML-KEM vector mismatch: operation=%d alias=%u value=%" PRIi32 "\n", op_value, alias, value) < 0) {
							io_failed = true;
							break;
						}
					}
				}
			}
		}
	}
	valid = valid && cases == 1857062 && fields == 29712992 && guards == 5571186 && inputs == 2720057;
	int result = io_failed ? 2 : valid ? 0 : 1;
	if (printf("ML-KEM vectors: operations=18 cases=%" PRIu64 " fields=%" PRIu64 " guards=%" PRIu64
	    " inputs=%" PRIu64 " valid=%u\n", cases, fields, guards, inputs, valid ? 1U : 0U) < 0 || fflush(stdout) != 0)
		result = 2;
	dssh_secure_zero(&work, sizeof(work));
	return result;
}
