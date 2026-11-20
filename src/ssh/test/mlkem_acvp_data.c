#include <stdio.h>
#include <string.h>

#include "mlkem_acvp.h"
#ifndef DSSH_ACVP_CASES
#include "mlkem_acvp_vectors.h"
#define DSSH_ACVP_CASES acvp_cases
#endif

#ifndef DSSH_ACVP_PRINTF
#define DSSH_ACVP_PRINTF printf
#endif
#ifndef DSSH_ACVP_FPRINTF
#define DSSH_ACVP_FPRINTF fprintf
#endif
#ifndef DSSH_ACVP_FFLUSH
#define DSSH_ACVP_FFLUSH fflush
#endif

static int
hex_digit(char byte)
{
	static const char digits[] = "0123456789ABCDEF";
	if (byte == '\0')
		return -1;
	const char *found = strchr(digits, byte);
	if (found == NULL)
		return -1;
	/* A match precedes the terminator, so its ordinal is in 0..15. */
	int value = (int)(found - digits);
	return value;
}

static bool
decode(const struct dssh_acvp_hex *source, uint8_t *out, size_t capacity)
{
	if (source->length == 0)
		return source->chunks[0] == NULL && source->chunks[1] == NULL && source->chunks[2] == NULL;
	if (source->length != capacity)
		return false;
	size_t written = 0;
	bool ended = false;
	for (size_t i = 0; i < 3; i++) {
		if (source->chunks[i] == NULL) {
			ended = true;
			continue;
		}
		if (ended)
			return false;
		size_t length = strlen(source->chunks[i]);
		if (length == 0 || length > 2048 || length % 2 != 0
		    || length / 2 > capacity - written)
			return false;
		for (size_t j = 0; j < length; j += 2) {
			int high = hex_digit(source->chunks[i][j]);
			int low = hex_digit(source->chunks[i][j + 1]);
			if (high < 0 || low < 0)
				return false;
			/* Both digits are in 0..15, so the combined value is in 0..255. */
			uint8_t value = (uint8_t)(16 * high + low);
			out[written++] = value;
		}
	}
	return written == source->length;
}

static bool
decode_case(const struct dssh_acvp_case *test, struct dssh_acvp_decoded *out)
{
	const struct dssh_acvp_hex *fields[] = {&test->d, &test->z, &test->m,
	    &test->ek, &test->dk, &test->c, &test->k};
	uint8_t *destinations[] = {out->d, out->z, out->m, out->ek, out->dk, out->c, out->k};
	const size_t capacities[] = {sizeof(out->d), sizeof(out->z), sizeof(out->m),
	    sizeof(out->ek), sizeof(out->dk), sizeof(out->c), sizeof(out->k)};
	const unsigned required[] = {0, 0x1b, 0x6c, 0x70, 0x7b, 0x08, 0x10};
	if (test->kind < DSSH_ACVP_KEYGEN || test->kind >= DSSH_ACVP_KIND_COUNT)
		return false;
	unsigned seen = 0;
	for (size_t i = 0; i < sizeof(fields) / sizeof(fields[0]); i++) {
		if (!decode(fields[i], destinations[i], capacities[i]))
			return false;
		if (fields[i]->length != 0)
			seen |= 1U << i;
	}
	return seen == required[test->kind];
}

enum oracle_mutation {
	ORACLE_EK,
	ORACLE_DK,
	ORACLE_C,
	ORACLE_K,
	ORACLE_ADMISSION
};

struct oracle_control {
	const char *name;
	enum dssh_acvp_kind kind;
	enum oracle_mutation mutation;
	bool implicit_only;
};

int
dssh_acvp_run(int argc, char **argv, const char *provider, dssh_acvp_evaluate evaluate)
{
	static const struct oracle_control controls[] = {
		{"keygen", DSSH_ACVP_KEYGEN, ORACLE_EK, false},
		{"keygen-private", DSSH_ACVP_KEYGEN, ORACLE_DK, false},
		{"encap", DSSH_ACVP_ENCAP, ORACLE_C, false},
		{"encap-secret", DSSH_ACVP_ENCAP, ORACLE_K, false},
		{"decap", DSSH_ACVP_DECAP, ORACLE_K, false},
		{"implicit-reject", DSSH_ACVP_DECAP, ORACLE_K, true},
		{"seed-decap", DSSH_ACVP_DECAP_SEED, ORACLE_K, false},
		{"public-check", DSSH_ACVP_PUBLIC_CHECK, ORACLE_ADMISSION, false},
		{"private-check", DSSH_ACVP_PRIVATE_CHECK, ORACLE_ADMISSION, false},
	};
	const struct oracle_control *negative = NULL;
	if (argc == 3 && strcmp(argv[1], "--negative") == 0) {
		for (size_t i = 0; i < sizeof(controls) / sizeof(controls[0]); i++) {
			if (strcmp(argv[2], controls[i].name) == 0)
				negative = &controls[i];
		}
		if (negative == NULL)
			return 2;
	}
	else if (argc != 1)
		return 2;

	size_t counts[DSSH_ACVP_KIND_COUNT] = {0};
	size_t implicit = 0;
	size_t failed = 0;
	size_t total = sizeof(DSSH_ACVP_CASES) / sizeof(DSSH_ACVP_CASES[0]);
	if (total != 145 || evaluate == NULL)
		return 2;
	struct dssh_acvp_decoded bytes;
	for (size_t i = 0; i < total; i++) {
		const struct dssh_acvp_case *test = DSSH_ACVP_CASES[i];
		memset(&bytes, 0, sizeof(bytes));
		if (!decode_case(test, &bytes)) {
			if (DSSH_ACVP_FPRINTF(stderr, "Invalid ACVP fixture at index %zu\n", i) < 0
			    || DSSH_ACVP_FFLUSH(stderr) != 0)
				return 2;
			return 2;
		}
		if (negative != NULL && (test->kind != negative->kind
		    || (negative->implicit_only && !test->implicit_rejection)))
			continue;
		struct dssh_acvp_case control = *test;
		if (negative != NULL) {
			switch (negative->mutation) {
				case ORACLE_EK:
					bytes.ek[0] ^= 1;
					break;
				case ORACLE_DK:
					bytes.dk[0] ^= 1;
					break;
				case ORACLE_C:
					bytes.c[0] ^= 1;
					break;
				case ORACLE_K:
					bytes.k[0] ^= 1;
					break;
				case ORACLE_ADMISSION:
					control.valid = !control.valid;
					break;
				default:
					return 2;
			}
		}
		bool matched = evaluate(&control, &bytes);
		if (!matched && (DSSH_ACVP_FPRINTF(stderr, "ACVP mismatch: %s group %u case %u\n",
		    test->profile, test->group, test->id) < 0 || DSSH_ACVP_FFLUSH(stderr) != 0))
			return 2;
		if (negative != NULL)
			return matched ? 0 : 1;
		counts[test->kind]++;
		if (test->implicit_rejection)
			implicit++;
		if (!matched)
			failed++;
	}
	const size_t expected[] = {0, 25, 50, 20, 10, 20, 20};
	for (size_t i = 0; i < DSSH_ACVP_KIND_COUNT; i++) {
		if (counts[i] != expected[i])
			return 2;
	}
	if (implicit != 15)
		return 2;
	if (DSSH_ACVP_PRINTF("ML-KEM ACVP %s: %zu passed, %zu failed; keygen=25 encap=50 decap=30 public=20 private=20 implicit=15\n",
	    provider, total - failed, failed) < 0 || DSSH_ACVP_FFLUSH(stdout) != 0)
		return 2;
	return failed == 0 ? 0 : 1;
}
