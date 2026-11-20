/* Direct NIST CAVP answers for the bundled ML-KEM hash primitives.
 * All I/O and general SHAKE128 composition here belong to this test runner. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef DSSH_TEST_MLKEM_HASH_RETIREMENT
#include <inttypes.h>
static void observe_hash_clear(const char *, const char *, const void *, size_t);
#define DSSH_MLKEM_CLEAR_OBSERVER observe_hash_clear
#endif
#include "kex/libcrux_mlkem768_sha3.h"
#ifdef DSSH_TEST_MLKEM_HASH_RETIREMENT
#include "mlkem_hash_retirement.h"
#endif

#define MAX_INPUT 17068
#define MAX_OUTPUT 2048
#define LINE_CAPACITY 35000

enum hash_mode { SHA3_256, SHA3_512, SHAKE128, SHAKE256, MATRIX128, RAW72 };

static const struct {
	const char *name;
	enum hash_mode mode;
	size_t cases;
} datasets[] = {
	{"SHA3_256ShortMsg.rsp", SHA3_256, 137},
	{"SHA3_256LongMsg.rsp", SHA3_256, 100},
	{"SHA3_512ShortMsg.rsp", SHA3_512, 73},
	{"SHA3_512LongMsg.rsp", SHA3_512, 100},
	{"SHAKE128ShortMsg.rsp", SHAKE128, 337},
	{"SHAKE128LongMsg.rsp", SHAKE128, 100},
	{"SHAKE128VariableOut.rsp", SHAKE128, 1126},
	{"SHAKE256ShortMsg.rsp", SHAKE256, 273},
	{"SHAKE256LongMsg.rsp", SHAKE256, 100},
	{"SHAKE256VariableOut.rsp", SHAKE256, 1246},
};

struct hash_work {
	uint8_t input[MAX_INPUT + 2];
	uint8_t saved[MAX_INPUT];
	uint8_t actual[MAX_OUTPUT + 2];
	uint8_t expected[MAX_OUTPUT];
};

static int
read_line(FILE *file, char *line)
{
	size_t used = 0;
	for (;;) {
		int ch = fgetc(file);
		if (ch == EOF) {
			if (ferror(file))
				return -1;
			if (used == 0)
				return 0;
			break;
		}
		if (ch == '\n')
			break;
		if (ch <= 0 || ch > 127 || used >= LINE_CAPACITY - 1)
			return -1;
		/* The retained ASCII response files only use values in 0..127. */
		char octet = (char)ch;
		line[used++] = octet;
	}
	if (used > 0 && line[used - 1] == '\r')
		used--;
	line[used] = '\0';
	return 1;
}

static bool
decimal(const char *text, uint32_t *out)
{
	uint32_t value = 0;
	if (*text == '\0')
		return false;
	for (; *text != '\0'; text++) {
		if (*text < '0' || *text > '9')
			return false;
		uint32_t digit = (uint32_t)(*text - '0');
		if (value > (UINT32_MAX - digit) / 10U)
			return false;
		value = value * 10U + digit;
	}
	*out = value;
	return true;
}

static bool
bit_length(const char *text, size_t limit, size_t *out)
{
	uint32_t bits;
	if (!decimal(text, &bits) || bits % 8U != 0 || bits / 8U > limit)
		return false;
	/* The caller's limits are at most17068, below C17's minimum SIZE_MAX. */
	size_t bytes = (size_t)(bits / 8U);
	*out = bytes;
	return true;
}

static int
hex_digit(char ch)
{
	const char *digits = "0123456789abcdef";
	if (ch == '\0')
		return -1;
	const char *found = strchr(digits, ch);
	if (found == NULL)
		return -1;
	int digit = (int)(found - digits);
	return digit;
}

static bool
hex_bytes(const char *text, uint8_t *out, size_t length)
{
	if (length > MAX_INPUT || strlen(text) != 2 * length)
		return false;
	for (size_t i = 0; i < length; i++) {
		int high = hex_digit(text[2 * i]);
		int low = hex_digit(text[2 * i + 1]);
		if (high < 0 || low < 0)
			return false;
		/* Two validated nibbles form an octet in0..255. */
		uint8_t octet = (uint8_t)(high * 16 + low);
		out[i] = octet;
	}
	return true;
}

static bool
hash_bytes_primitive(enum hash_mode mode, uint8_t *input, size_t length, uint8_t *out, size_t outlen)
{
	Eurydice_slice data = {input, length};
	Eurydice_slice digest = {out, outlen};
	if (mode == SHA3_256) {
		if (outlen != 32)
			return false;
		libcrux_sha3_portable_sha256(digest, data);
	}
	else if (mode == SHA3_512) {
		if (outlen != 64)
			return false;
		libcrux_sha3_portable_sha512(digest, data);
	}
	else if (mode == SHAKE256)
		libcrux_sha3_portable_shake256(digest, data);
	else if (mode == RAW72) {
		Eurydice_slice inputs[1] = {data};
		Eurydice_slice outputs[1] = {digest};
		libcrux_sha3_generic_keccak_keccak_e9(inputs, outputs);
	}
	else if (mode == MATRIX128) {
		if (length != 102 || outlen != 2016)
			return false;
		uint8_t lanes[3][34];
		uint8_t first[3][504];
		uint8_t next[3][168];
		memcpy(lanes, input, sizeof(lanes));
		libcrux_ml_kem_hash_functions_portable_PortableHash_58 state =
		    libcrux_ml_kem_hash_functions_portable_shake128_init_absorb_f1_8c(lanes);
		libcrux_ml_kem_hash_functions_portable_shake128_squeeze_three_blocks_f1_69(&state, first);
		libcrux_ml_kem_hash_functions_portable_shake128_squeeze_block_f1_60(&state, next);
		for (size_t i = 0; i < 3; i++) {
			memcpy(out + i * 672, first[i], 504);
			memcpy(out + i * 672 + 504, next[i], 168);
		}
		dssh_secure_zero(&state, sizeof(state));
	}
	else if (mode == SHAKE128) {
		/* The production matrix input has34 bytes. Longer NIST messages use
		 * this test-only composition of the same load/permutation/final
		 * primitives; no general hash interface is added to the library. */
		libcrux_sha3_generic_keccak_KeccakState_48 state =
		    libcrux_sha3_portable_incremental_shake128_init();
		size_t offset = 0;
		while (length - offset >= 168) {
			Eurydice_slice block[1] = {{input + offset, 168}};
			libcrux_sha3_portable_keccak_load_block_2c1(state.st, block);
			libcrux_sha3_generic_keccak_keccakf1600_21(&state);
			offset += 168;
		}
		Eurydice_slice tail = {input + offset, length - offset};
		libcrux_sha3_portable_incremental_shake128_absorb_final(&state, tail);
		uint8_t block[168];
		Eurydice_slice destination[1] = {{block, sizeof(block)}};
		libcrux_sha3_generic_keccak_squeeze_first_block_841(&state, destination);
		offset = 0;
		while (offset < outlen) {
			size_t count = outlen - offset;
			if (count > sizeof(block))
				count = sizeof(block);
			memcpy(out + offset, block, count);
			offset += count;
			if (offset < outlen)
				libcrux_sha3_portable_incremental_shake128_squeeze_next_block(&state, destination[0]);
		}
		dssh_secure_zero(&state, sizeof(state));
		dssh_secure_zero(block, sizeof(block));
	}
	else
		return false;
	return true;
}

#ifdef DSSH_TEST_MLKEM_HASH_RETIREMENT
static struct dssh_hash_wipes *active_hash_wipes;
static uint64_t hash_retired_cases;
static uint64_t hash_retired_clears;
static uint32_t hash_seen_sites;
static bool hash_retirement_valid = true;

static void
observe_hash_clear(const char *function, const char *owner, const void *buffer, size_t bytes)
{
	if (active_hash_wipes != NULL
	    && !dssh_hash_wipe_record(active_hash_wipes, function, owner, buffer, bytes))
		active_hash_wipes->valid = false;
}

static bool
hash_bytes(enum hash_mode mode, uint8_t *input, size_t length, uint8_t *out, size_t outlen)
{
	struct dssh_hash_wipes actual = {.valid = true};
	struct dssh_hash_wipes expected = {.valid = true};
	active_hash_wipes = &actual;
	bool result = hash_bytes_primitive(mode, input, length, out, outlen);
	active_hash_wipes = NULL;
	if (!result)
		return false;
	if (hash_retired_cases == UINT64_MAX) {
		hash_retirement_valid = false;
		return false;
	}
	hash_retired_cases++;
	size_t rate = mode == SHA3_512 || mode == RAW72 ? 72 : mode == SHAKE128 || mode == MATRIX128 ? 168 : 136;
	/* run_hash bounds input/output at17068/2048. The largest quotient sum
	 * is below300 on every supported size_t width. */
	size_t rounds = length / rate + 1;
	if (outlen != 0)
		rounds += (outlen - 1) / rate;
	if (mode == MATRIX128)
		rounds = 12;
	uint32_t permutations = (uint32_t)rounds;
	expected.calls[DSSH_HASH_C] = permutations;
	expected.calls[DSSH_HASH_T] = permutations;
	expected.calls[DSSH_HASH_OLD] = permutations;
	if (mode == SHAKE128 || mode == MATRIX128)
		expected.calls[DSSH_HASH_FINAL168] = mode == MATRIX128 ? 3 : 1;
	else {
		size_t final = mode == SHA3_512 || mode == RAW72 ? DSSH_HASH_FINAL72
		    : mode == SHA3_256 ? DSSH_HASH_FINAL136_SHA : DSSH_HASH_FINAL136_SHAKE;
		expected.calls[final] = 1;
		size_t state = mode == SHA3_512 || mode == RAW72 ? DSSH_HASH_STATE72
		    : mode == SHA3_256 ? DSSH_HASH_STATE136_SHA : DSSH_HASH_STATE136_SHAKE;
		expected.calls[state] = 1;
		if (outlen < rate)
			expected.calls[rate == 72 ? DSSH_HASH_FIRST_LAST72 : DSSH_HASH_FIRST_LAST136] = 1;
		else if (outlen % rate != 0)
			expected.calls[rate == 72 ? DSSH_HASH_LAST72 : DSSH_HASH_LAST136] = 1;
	}
	uint64_t clears = dssh_hash_wipe_total(&actual);
	if (hash_retired_clears > UINT64_MAX - clears) {
		hash_retirement_valid = false;
		return false;
	}
	hash_retired_clears += clears;
	for (size_t i = 0; i < DSSH_HASH_WIPE_SLOTS; i++) {
		if (actual.calls[i] != 0)
			hash_seen_sites |= UINT32_C(1) << i;
	}
	if (!dssh_hash_wipe_match(&actual, &expected))
		hash_retirement_valid = false;
	return hash_retirement_valid;
}
#else
#define hash_bytes hash_bytes_primitive
#endif

static bool
run_hash(struct hash_work *work, enum hash_mode mode, size_t length, size_t outlen)
{
	if (length > MAX_INPUT || outlen > MAX_OUTPUT)
		return false;
	memcpy(work->saved, work->input + 1, length);
	work->input[0] = 0x6d;
	memset(work->input + length + 1, 0x6d, MAX_INPUT + 1 - length);
	memset(work->actual, 0xa5, sizeof(work->actual));
	if (!hash_bytes(mode, work->input + 1, length, work->actual + 1, outlen)
	    || memcmp(work->input + 1, work->saved, length) != 0
	    || work->input[0] != 0x6d || work->actual[0] != 0xa5)
		return false;
	for (size_t i = length + 1; i < sizeof(work->input); i++) {
		if (work->input[i] != 0x6d)
			return false;
	}
	for (size_t i = outlen + 1; i < sizeof(work->actual); i++) {
		if (work->actual[i] != 0xa5)
			return false;
	}
	return true;
}

static bool
bracket_length(char *line, const char *prefix, size_t limit, size_t *out)
{
	size_t length = strlen(line);
	if (strncmp(line, prefix, strlen(prefix)) != 0 || length == 0 || line[length - 1] != ']')
		return false;
	line[length - 1] = '\0';
	return bit_length(line + strlen(prefix), limit, out);
}

static int
run_file(const char *directory, size_t index, bool negative, struct hash_work *work, char *line)
{
	char path[1024];
	int written = snprintf(path, sizeof(path), "%s/%s", directory, datasets[index].name);
	if (written < 0 || written >= 1024)
		return 2;
	FILE *file = fopen(path, "r");
	if (file == NULL)
		return 2;
	bool variable = strstr(datasets[index].name, "VariableOut") != NULL;
	size_t length = 0;
	size_t outlen = datasets[index].mode == SHA3_256 ? 32 : 64;
	bool have_length = false;
	bool have_output = datasets[index].mode == SHA3_256 || datasets[index].mode == SHA3_512;
	bool have_message = false;
	bool have_count = false;
	size_t count = 0;
	int result = 0;
	int read;
	while ((read = read_line(file, line)) == 1) {
		if (line[0] == '\0' || line[0] == '#')
			continue;
		if (line[0] == '[') {
			if (strncmp(line, "[Outputlen = ", 13) == 0) {
				have_output = bracket_length(line, "[Outputlen = ", MAX_OUTPUT, &outlen);
				if (!have_output)
					result = 2;
			}
			else if (strncmp(line, "[Input Length = ", 16) == 0) {
				have_length = bracket_length(line, "[Input Length = ", MAX_INPUT, &length);
				if (!have_length)
					result = 2;
			}
			else if (strncmp(line, "[L = ", 5) != 0
			    && strcmp(line, "[Tested for Output of byte-oriented messages]") != 0
			    && strncmp(line, "[Minimum Output Length (bits) = ", 32) != 0
			    && strncmp(line, "[Maximum Output Length (bits) = ", 32) != 0)
				result = 2;
		}
		else if (strncmp(line, "COUNT = ", 8) == 0) {
			uint32_t ordinal;
			if (!variable || have_count || !decimal(line + 8, &ordinal) || ordinal != count)
				result = 2;
			have_count = true;
		}
		else if (strncmp(line, "Len = ", 6) == 0) {
			if (variable || have_length || !bit_length(line + 6, MAX_INPUT, &length))
				result = 2;
			have_length = true;
		}
		else if (strncmp(line, "Outputlen = ", 12) == 0) {
			if (!variable || !have_count || have_output
			    || !bit_length(line + 12, MAX_OUTPUT, &outlen))
				result = 2;
			have_output = true;
		}
		else if (strncmp(line, "Msg = ", 6) == 0) {
			if (have_message || !have_length || !have_output
			    || (length == 0 ? strcmp(line + 6, "00") != 0
			        : !hex_bytes(line + 6, work->input + 1, length)))
				result = 2;
			have_message = true;
		}
		else if (strncmp(line, "MD = ", 5) == 0 || strncmp(line, "Output = ", 9) == 0) {
			const char *answer = line + (line[0] == 'M' ? 5 : 9);
			if (!have_message || outlen == 0 || (variable && !have_count)
			    || !hex_bytes(answer, work->expected, outlen)
			    || !run_hash(work, datasets[index].mode, length, outlen)) {
				result = 2;
				break;
			}
			if (negative)
				work->expected[0] ^= 1;
			bool matched = memcmp(work->actual + 1, work->expected, outlen) == 0;
			count++;
			if (!matched) {
				result = 1;
				if (fprintf(stderr, "ML-KEM hash mismatch: file=%s case=%zu\n", datasets[index].name, count) < 0)
					result = 2;
			}
			if (negative || !matched)
				break;
			have_message = false;
			have_count = false;
			if (variable)
				have_output = false;
			else
				have_length = false;
		}
		else
			result = 2;
		if (result != 0)
			break;
	}
	if (read < 0 || (result == 0 && (have_message || have_count
	    || (!variable && have_length) || (variable && have_output)
	    || (!negative && count != datasets[index].cases))))
		result = 2;
	if (fclose(file) != 0)
		result = 2;
	if (result == 0 && printf("ML-KEM hash file: %s cases=%zu passed\n", datasets[index].name, count) < 0)
		result = 2;
	return result;
}

static int
run_oracle(struct hash_work *work, char *line)
{
	static const char *names[] = {"sha3-256", "sha3-512", "shake128", "shake256", "matrix128", "raw72"};
	size_t count = 0;
	int read;
	while ((read = read_line(stdin, line)) == 1) {
		char *name = strtok(line, " ");
		char *output = strtok(NULL, " ");
		char *input = strtok(NULL, " ");
		uint32_t requested;
		if (name == NULL || output == NULL || input == NULL || strtok(NULL, " ") != NULL
		    || !decimal(output, &requested) || requested > MAX_OUTPUT)
			return 2;
		size_t mode;
		for (mode = 0; mode < sizeof(names) / sizeof(names[0]); mode++) {
			if (strcmp(names[mode], name) == 0)
				break;
		}
		if (mode >= sizeof(names) / sizeof(names[0]))
			return 2;
		/* The table index has been checked against six enum values. */
		enum hash_mode selected = (enum hash_mode)mode;
		size_t outlen = (size_t)requested;
		size_t hexlen = strcmp(input, "-") == 0 ? 0 : strlen(input);
		if (hexlen % 2 != 0 || hexlen / 2 > MAX_INPUT)
			return 2;
		size_t length = hexlen / 2;
		if ((length != 0 && !hex_bytes(input, work->input + 1, length))
		    || !run_hash(work, selected, length, outlen))
			return 2;
		for (size_t i = 0; i < outlen; i++) {
			if (printf("%02x", work->actual[i + 1]) < 0)
				return 2;
		}
		if (putchar('\n') == EOF)
			return 2;
		if (count == SIZE_MAX)
			return 2;
		count++;
	}
	if (read < 0 || count == 0)
		return 2;
	if (fprintf(stderr, "ML-KEM hash oracle: cases=%zu valid=1\n", count) < 0)
		return 2;
	return 0;
}

int
main(int argc, char **argv)
{
	struct hash_work *work = malloc(sizeof(*work));
	char *line = malloc(LINE_CAPACITY);
	int result = 2;
	if (work == NULL || line == NULL)
		goto done;
#ifdef DSSH_TEST_MLKEM_HASH_RETIREMENT
	if (!dssh_hash_wipe_controls())
		goto done;
#endif
	if (argc == 2 && strcmp(argv[1], "--oracle") == 0)
		result = run_oracle(work, line);
	else if (argc == 2) {
		result = 0;
		for (size_t i = 0; i < sizeof(datasets) / sizeof(datasets[0]); i++) {
			result = run_file(argv[1], i, false, work, line);
			if (result != 0)
				break;
		}
		if (result == 0 && printf("ML-KEM hash CAVP: 3592 passed, 0 failed; files=10 guards=1\n") < 0)
			result = 2;
	}
	else if (argc == 4 && strcmp(argv[2], "--negative") == 0) {
		for (size_t i = 0; i < sizeof(datasets) / sizeof(datasets[0]); i++) {
			if (strcmp(argv[3], datasets[i].name) == 0) {
				result = run_file(argv[1], i, true, work, line);
				break;
			}
		}
	}
	done:
#ifdef DSSH_TEST_MLKEM_HASH_RETIREMENT
	if (fprintf(stderr, "ML-KEM hash retirement: cases=%" PRIu64 " sites=14 clears=%" PRIu64
	    " seen=%" PRIu32 " active=%u valid=%u\n", hash_retired_cases, hash_retired_clears,
	    hash_seen_sites, active_hash_wipes != NULL ? 1U : 0U, hash_retirement_valid ? 1U : 0U) < 0)
		result = 2;
#endif
	if (work != NULL)
		dssh_secure_zero(work, sizeof(*work));
	if (line != NULL)
		dssh_secure_zero(line, LINE_CAPACITY);
	free(work);
	free(line);
	if (fflush(stdout) != 0 || fflush(stderr) != 0)
		return 2;
	return result;
}
