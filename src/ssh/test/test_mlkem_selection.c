/* Fixed-domain functional checks for the production ML-KEM selection helpers. */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "kex/libcrux_mlkem768_sha3.h"

struct secret_bytes {
	uint8_t before[16];
	uint8_t value[32];
	uint8_t after[16];
};

struct selection_work {
	struct secret_bytes left;
	struct secret_bytes right;
	struct secret_bytes output;
	uint8_t left_saved[32];
	uint8_t right_saved[32];
	uint8_t ciphertext[1088];
	uint8_t expected_ciphertext[1088];
};

static void
prepare(struct selection_work *work, unsigned pattern)
{
	memset(work, 0xa5, sizeof(*work));
	memset(work->left.after, 0x5a, sizeof(work->left.after));
	memset(work->right.after, 0x5a, sizeof(work->right.after));
	memset(work->output.after, 0x5a, sizeof(work->output.after));
	for (size_t i = 0; i < sizeof(work->left.value); i++) {
		/* i < 32, so both conversions are within the uint8_t domain. */
		uint8_t value = (uint8_t)(7 * i + pattern);
		work->left.value[i] = value;
		work->right.value[i] = value ^ UINT8_C(0xff);
	}
	memcpy(work->left_saved, work->left.value, sizeof(work->left_saved));
	memcpy(work->right_saved, work->right.value, sizeof(work->right_saved));
	memset(work->ciphertext, 0x69, sizeof(work->ciphertext));
	memcpy(work->expected_ciphertext, work->ciphertext, sizeof(work->ciphertext));
}

static bool
guards_valid(const struct secret_bytes *bytes)
{
	for (size_t i = 0; i < sizeof(bytes->before); i++) {
		if (bytes->before[i] != 0xa5 || bytes->after[i] != 0x5a)
			return false;
	}
	return true;
}

static bool
output_valid(const struct selection_work *work, const struct secret_bytes *output,
    bool choose_right)
{
	const uint8_t *answer = choose_right ? work->right_saved : work->left_saved;
	return memcmp(output->value, answer, sizeof(output->value)) == 0
	    && guards_valid(&work->left) && guards_valid(&work->right)
	    && guards_valid(&work->output)
	    && (output == &work->left || memcmp(work->left.value, work->left_saved, 32) == 0)
	    && (output == &work->right || memcmp(work->right.value, work->right_saved, 32) == 0);
}

int
main(int argc, char **argv)
{
	unsigned negative = 0;
	if (argc == 2 && strcmp(argv[1], "--negative-selection") == 0)
		negative = 1;
	else if (argc == 2 && strcmp(argv[1], "--negative-comparison") == 0)
		negative = 2;
	else if (argc == 2 && strcmp(argv[1], "--negative-guard") == 0)
		negative = 3;
	else if (argc != 1)
		return 2;
	static struct selection_work work;
	bool valid = true;
	unsigned nonzero = 0;
	unsigned selections = 0;
	unsigned comparisons = 0;
	for (unsigned value = 0; value < 256; value++) {
		uint8_t selector = (uint8_t)value;
		valid = libcrux_ml_kem_constant_time_ops_is_non_zero(selector) == (value != 0) && valid;
		nonzero++;
		for (unsigned pattern = 0; pattern < 4; pattern++) {
			for (unsigned alias = 0; alias < 3; alias++) {
				prepare(&work, pattern);
				struct secret_bytes *output = alias == 0 ? &work.output : alias == 1 ? &work.left : &work.right;
				libcrux_ml_kem_constant_time_ops_select_shared_secret_in_constant_time(
				    Eurydice_array_to_slice(32, work.left.value, uint8_t),
				    Eurydice_array_to_slice(32, work.right.value, uint8_t), selector, output->value);
				if (negative == 1 && value == 0 && pattern == 0 && alias == 0)
					output->value[0] ^= 1;
				if (negative == 3 && value == 0 && pattern == 0 && alias == 0)
					output->after[0] ^= 1;
				valid = output_valid(&work, output, value != 0) && valid;
				selections++;
			}
		}
	}
	prepare(&work, 0);
	Eurydice_slice left = Eurydice_array_to_slice(1088, work.ciphertext, uint8_t);
	Eurydice_slice right = Eurydice_array_to_slice(1088, work.expected_ciphertext, uint8_t);
	Eurydice_slice candidate = Eurydice_array_to_slice(32, work.left.value, uint8_t);
	Eurydice_slice rejection = Eurydice_array_to_slice(32, work.right.value, uint8_t);
	valid = libcrux_ml_kem_constant_time_ops_compare_ciphertexts_in_constant_time(left, right) == 0 && valid;
	libcrux_ml_kem_constant_time_ops_compare_ciphertexts_select_shared_secret_in_constant_time(
	    left, right, candidate, rejection, work.output.value);
	valid = output_valid(&work, &work.output, false) && valid;
	comparisons++;
	for (size_t position = 0; position < sizeof(work.ciphertext); position++) {
		for (unsigned bit = 0; bit < 8; bit++) {
			uint8_t difference = (uint8_t)(1U << bit);
			work.expected_ciphertext[position] ^= difference;
			uint8_t result = libcrux_ml_kem_constant_time_ops_compare_ciphertexts_in_constant_time(left, right);
			if (negative == 2 && position == 0 && bit == 0)
				result = 0;
			valid = result == 1 && valid;
			libcrux_ml_kem_constant_time_ops_compare_ciphertexts_select_shared_secret_in_constant_time(
			    left, right, candidate, rejection, work.output.value);
			valid = output_valid(&work, &work.output, true) && valid;
			/* Neither input may be changed by the production operation. */
			for (size_t i = 0; i < sizeof(work.ciphertext); i++) {
				uint8_t expected = i == position ? UINT8_C(0x69) ^ difference : UINT8_C(0x69);
				valid = work.ciphertext[i] == 0x69 && work.expected_ciphertext[i] == expected && valid;
			}
			work.expected_ciphertext[position] ^= difference;
			comparisons++;
		}
	}
	valid = nonzero == 256 && selections == 3072 && comparisons == 8705 && valid;
	int result = valid ? 0 : 1;
	if (printf("ML-KEM selection: nonzero=%u selections=%u comparisons=%u valid=%u\n",
	    nonzero, selections, comparisons, valid ? 1U : 0U) < 0 || fflush(stdout) != 0)
		result = 2;
	dssh_secure_zero(&work, sizeof(work));
	return result;
}
