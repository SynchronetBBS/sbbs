/* Exercise the actual wrapper with finite published entropy and a live wipe
 * observer. Only private symbols and crypto endpoints are renamed in this TU. */
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "deucessh.h"
#include "deucessh-crypto.h"
#include "mlkem_acvp_data.c"

enum operation { KEYPAIR, ENCAPSULATE, DECAPSULATE };
enum control { ORDINARY, BAD_WIPE, BAD_OUTPUT, BAD_GUARD, BAD_INPUT };
static enum control control;
static bool control_applied;
struct wrapper_call {
	enum operation operation;
	uint8_t entropy[64];
	size_t entropy_size;
	bool fail_random;
	size_t partial;
	void *random_buffer;
	size_t expected[2];
	bool is_random[2];
	size_t expected_count;
	size_t actual_count;
	size_t random_calls;
	bool valid;
};
static struct wrapper_call call;
static uint64_t cases, failed, random_calls, wipes, wiped_bytes, zero_failures;
static uint64_t owner_calls[5];
static uint64_t guards, inputs, answers, keypairs, encapsulations, decapsulations;
static int wrapper_random(uint8_t *buffer, size_t length);
static void wrapper_cleanse(void *buffer, size_t length);

#define dssh_random wrapper_random
#define dssh_cleanse wrapper_cleanse
#define crypto_kem_mlkem768_keypair wrapper_keypair
#define crypto_kem_mlkem768_enc wrapper_encapsulate
#define crypto_kem_mlkem768_dec wrapper_decapsulate
#include "kex/mlkem768.c"
#undef crypto_kem_mlkem768_dec
#undef crypto_kem_mlkem768_enc
#undef crypto_kem_mlkem768_keypair
#undef dssh_cleanse
#undef dssh_random

struct guarded_pk { uint8_t before[16], value[1184], after[16]; };
struct guarded_sk { uint8_t before[16], value[2400], after[16]; };
struct guarded_ct { uint8_t before[16], value[1088], after[16]; };
struct guarded_ss { uint8_t before[16], value[32], after[16]; };
static struct {
	struct guarded_pk pk;
	struct guarded_sk sk;
	struct guarded_ct ct;
	struct guarded_ss ss;
} work;
static struct dssh_acvp_decoded decoded;

static bool
all_bytes(const void *pointer, size_t length, uint8_t value)
{
	const uint8_t *bytes = pointer;
	bool valid = true;
	for (size_t i = 0; i < length; i++) {
		if (bytes[i] != value)
			valid = false;
	}
	return valid;
}

static int
wrapper_random(uint8_t *buffer, size_t length)
{
	call.random_calls++;
	random_calls++;
	if (length != call.entropy_size || call.random_calls != 1
	    || length > sizeof(call.entropy) || (call.fail_random && call.partial > length)) {
		call.valid = false;
		return DSSH_ERROR_INIT;
	}
	call.random_buffer = buffer;
	memcpy(buffer, call.entropy, call.fail_random ? call.partial : length);
	return call.fail_random ? DSSH_ERROR_INIT : 0;
}

static void
wrapper_cleanse(void *buffer, size_t length)
{
	size_t stage = call.actual_count++;
	if (stage >= call.expected_count || length != call.expected[stage]
	    || (call.is_random[stage] && buffer != call.random_buffer)
	    || (!call.is_random[stage] && buffer == call.random_buffer)) {
		call.valid = false;
		return;
	}
	dssh_cleanse(buffer, length);
	if (control == BAD_WIPE && !control_applied) {
		uint8_t *bytes = buffer;
		bytes[0] = 1;
		control_applied = true;
	}
	bool zero = all_bytes(buffer, length, 0);
	if (!zero) {
		zero_failures++;
		call.valid = false;
	}
	wipes++;
	wiped_bytes += length;
	size_t owner = call.operation == KEYPAIR ? (call.is_random[stage] ? 0U : 1U)
	    : call.operation == ENCAPSULATE ? (call.is_random[stage] ? 2U : 3U) : 4U;
	owner_calls[owner]++;
}

static void
prepare(void)
{
	memset(&work, 0x3c, sizeof(work));
	memset(work.pk.before, 0xa5, sizeof(work.pk.before));
	memset(work.pk.after, 0x5a, sizeof(work.pk.after));
	memset(work.sk.before, 0xa5, sizeof(work.sk.before));
	memset(work.sk.after, 0x5a, sizeof(work.sk.after));
	memset(work.ct.before, 0xa5, sizeof(work.ct.before));
	memset(work.ct.after, 0x5a, sizeof(work.ct.after));
	memset(work.ss.before, 0xa5, sizeof(work.ss.before));
	memset(work.ss.after, 0x5a, sizeof(work.ss.after));
}

static void
begin(enum operation operation, const struct dssh_acvp_decoded *bytes, bool fail_random, size_t partial)
{
	call = (struct wrapper_call){0};
	call.operation = operation;
	call.fail_random = fail_random;
	call.partial = partial;
	call.valid = true;
	if (operation == DECAPSULATE) {
		call.expected[0] = sizeof(libcrux_ml_kem_types_MlKemPrivateKey_55);
		call.expected_count = 1;
		return;
	}
	call.entropy_size = operation == KEYPAIR ? 64U : 32U;
	if (operation == KEYPAIR) {
		memcpy(call.entropy, bytes->d, 32);
		memcpy(call.entropy + 32, bytes->z, 32);
	}
	else
		memcpy(call.entropy, bytes->m, 32);
	size_t stage = 0;
	if (!fail_random) {
		call.expected[stage++] = operation == KEYPAIR
		    ? sizeof(libcrux_ml_kem_mlkem768_MlKem768KeyPair) : sizeof(tuple_3c);
	}
	call.expected[stage] = call.entropy_size;
	call.is_random[stage] = true;
	call.expected_count = stage + 1;
}

static bool
guard(const uint8_t before[16], const uint8_t after[16])
{
	guards++;
	bool first = all_bytes(before, 16, 0xa5);
	bool second = all_bytes(after, 16, 0x5a);
	return first && second;
}

static bool
input(const void *actual, const void *expected, size_t length)
{
	inputs++;
	return memcmp(actual, expected, length) == 0;
}

static bool
answer(const void *actual, const void *expected, size_t length)
{
	answers++;
	return memcmp(actual, expected, length) == 0;
}

static bool
end(bool expects_random)
{
	return call.valid && call.actual_count == call.expected_count
	    && call.random_calls == (expects_random ? 1U : 0U);
}

static bool
keypair(const struct dssh_acvp_decoded *bytes, bool failure, size_t partial)
{
	prepare();
	begin(KEYPAIR, bytes, failure, partial);
	int result = wrapper_keypair(work.pk.value, work.sk.value);
	keypairs++;
	if (control == BAD_OUTPUT && !control_applied) {
		work.pk.value[0] ^= 1;
		control_applied = true;
	}
	if (control == BAD_GUARD && !control_applied) {
		work.pk.after[0] = 0;
		control_applied = true;
	}
	bool valid = result == (failure ? -1 : 0) && end(true);
	valid = guard(work.pk.before, work.pk.after) && valid;
	valid = guard(work.sk.before, work.sk.after) && valid;
	if (failure) {
		valid = all_bytes(work.pk.value, sizeof(work.pk.value), 0x3c) && valid;
		valid = all_bytes(work.sk.value, sizeof(work.sk.value), 0x3c) && valid;
	}
	else {
		valid = answer(work.pk.value, bytes->ek, sizeof(work.pk.value)) && valid;
		valid = answer(work.sk.value, bytes->dk, sizeof(work.sk.value)) && valid;
	}
	return valid;
}

static bool
encapsulate(const struct dssh_acvp_decoded *bytes, bool failure, size_t partial, bool admission, bool accepted)
{
	prepare();
	memcpy(work.pk.value, bytes->ek, sizeof(work.pk.value));
	begin(ENCAPSULATE, bytes, failure, partial);
	if (!accepted)
		call.expected_count = 0;
	int result = wrapper_encapsulate(work.ct.value, work.ss.value, work.pk.value);
	encapsulations++;
	if (control == BAD_INPUT && !control_applied) {
		work.pk.value[0] ^= 1;
		control_applied = true;
	}
	bool valid = result == (failure || !accepted ? -1 : 0) && end(accepted);
	valid = guard(work.pk.before, work.pk.after) && valid;
	valid = guard(work.ct.before, work.ct.after) && valid;
	valid = guard(work.ss.before, work.ss.after) && valid;
	valid = input(work.pk.value, bytes->ek, sizeof(work.pk.value)) && valid;
	if (failure || !accepted) {
		valid = all_bytes(work.ct.value, sizeof(work.ct.value), 0x3c) && valid;
		valid = all_bytes(work.ss.value, sizeof(work.ss.value), 0x3c) && valid;
	}
	else if (!admission) {
		valid = answer(work.ct.value, bytes->c, sizeof(work.ct.value)) && valid;
		valid = answer(work.ss.value, bytes->k, sizeof(work.ss.value)) && valid;
	}
	return valid;
}

static bool
decapsulate(const struct dssh_acvp_decoded *bytes)
{
	prepare();
	memcpy(work.sk.value, bytes->dk, sizeof(work.sk.value));
	memcpy(work.ct.value, bytes->c, sizeof(work.ct.value));
	begin(DECAPSULATE, bytes, false, 0);
	int result = wrapper_decapsulate(work.ss.value, work.ct.value, work.sk.value);
	decapsulations++;
	bool valid = result == 0 && end(false);
	valid = guard(work.sk.before, work.sk.after) && valid;
	valid = guard(work.ct.before, work.ct.after) && valid;
	valid = guard(work.ss.before, work.ss.after) && valid;
	valid = input(work.sk.value, bytes->dk, sizeof(work.sk.value)) && valid;
	valid = input(work.ct.value, bytes->c, sizeof(work.ct.value)) && valid;
	valid = answer(work.ss.value, bytes->k, sizeof(work.ss.value)) && valid;
	return valid;
}

static void
record(bool valid)
{
	cases++;
	if (!valid)
		failed++;
}

int
main(int argc, char **argv)
{
	if (argc == 2 && strcmp(argv[1], "--negative-wipe") == 0)
		control = BAD_WIPE;
	else if (argc == 2 && strcmp(argv[1], "--negative-output") == 0)
		control = BAD_OUTPUT;
	else if (argc == 2 && strcmp(argv[1], "--negative-guard") == 0)
		control = BAD_GUARD;
	else if (argc == 2 && strcmp(argv[1], "--negative-input") == 0)
		control = BAD_INPUT;
	else if (argc != 1)
		return 2;
	size_t excluded = 0;
	for (size_t i = 0; i < sizeof(acvp_cases) / sizeof(acvp_cases[0]); i++) {
		const struct dssh_acvp_case *test = acvp_cases[i];
		if (test->kind == DSSH_ACVP_PRIVATE_CHECK) {
			excluded++;
			continue;
		}
		memset(&decoded, 0, sizeof(decoded));
		if (!decode_case(test, &decoded))
			return 2;
		bool valid;
		if (test->kind == DSSH_ACVP_KEYGEN)
			valid = keypair(&decoded, false, 0);
		else if (test->kind == DSSH_ACVP_ENCAP)
			valid = encapsulate(&decoded, false, 0, false, true);
		else if (test->kind == DSSH_ACVP_DECAP)
			valid = decapsulate(&decoded);
		else if (test->kind == DSSH_ACVP_DECAP_SEED) {
			valid = keypair(&decoded, false, 0);
			valid = decapsulate(&decoded) && valid;
		}
		else if (test->kind == DSSH_ACVP_PUBLIC_CHECK)
			valid = encapsulate(&decoded, false, 0, true, test->valid);
		else
			return 2;
		record(valid);
	}
	if (!decode_case(acvp_cases[0], &decoded) || acvp_cases[0]->kind != DSSH_ACVP_KEYGEN)
		return 2;
	for (size_t partial = 0; partial <= 64; partial++)
		record(keypair(&decoded, true, partial));
	for (size_t partial = 0; partial <= 32; partial++)
		record(encapsulate(&decoded, true, partial, false, true));
	/* Counts are fixed by the corpus; object extents follow the actual ABI. */
	uint64_t expected_bytes = UINT64_C(100) * 64 + UINT64_C(35) * sizeof(libcrux_ml_kem_mlkem768_MlKem768KeyPair)
	    + UINT64_C(93) * 32 + UINT64_C(60) * sizeof(tuple_3c) + UINT64_C(30) * sizeof(libcrux_ml_kem_types_MlKemPrivateKey_55);
	bool valid = failed == 0 && cases == 223 && excluded == 20 && keypairs == 100
	    && encapsulations == 103 && decapsulations == 30 && random_calls == 193
	    && wipes == 318 && wiped_bytes == expected_bytes && zero_failures == 0
	    && owner_calls[0] == 100 && owner_calls[1] == 35 && owner_calls[2] == 93
	    && owner_calls[3] == 60 && owner_calls[4] == 30 && guards == 599 && inputs == 163 && answers == 200
	    && (control == ORDINARY || control_applied);
	int result = valid ? 0 : 1;
	if (printf("ML-KEM wrappers: cases=%" PRIu64 " failed=%" PRIu64 " excluded=%zu random=%" PRIu64
	    " wipes=%" PRIu64 " bytes=%" PRIu64 " zero_failures=%" PRIu64 " guards=%" PRIu64
	    " inputs=%" PRIu64 " answers=%" PRIu64 " owners=%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 " valid=%u\n", cases, failed, excluded, random_calls,
	    wipes, wiped_bytes, zero_failures, guards, inputs, answers, owner_calls[0], owner_calls[1], owner_calls[2], owner_calls[3], owner_calls[4], valid ? 1U : 0U) < 0 || fflush(stdout) != 0)
		result = 2;
	dssh_cleanse(&work, sizeof(work));
	dssh_cleanse(&decoded, sizeof(decoded));
	dssh_cleanse(&call, sizeof(call));
	return result;
}
