/* Shared direct-test observations of actual bundled hash owner storage.
 * Include after the generated implementation's type declarations. */
#ifndef DSSH_TEST_MLKEM_HASH_RETIREMENT_H
#define DSSH_TEST_MLKEM_HASH_RETIREMENT_H

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

enum dssh_hash_wipe_slot {
	DSSH_HASH_C,
	DSSH_HASH_T,
	DSSH_HASH_OLD,
	DSSH_HASH_FINAL72,
	DSSH_HASH_FINAL136_SHA,
	DSSH_HASH_FINAL136_SHAKE,
	DSSH_HASH_FINAL168,
	DSSH_HASH_FIRST_LAST72,
	DSSH_HASH_FIRST_LAST136,
	DSSH_HASH_LAST72,
	DSSH_HASH_LAST136,
	DSSH_HASH_STATE72,
	DSSH_HASH_STATE136_SHA,
	DSSH_HASH_STATE136_SHAKE,
	DSSH_HASH_WIPE_SLOTS
};

static const struct {
	const char *function;
	const char *owner;
	size_t bytes;
} dssh_hash_wipe_sites[DSSH_HASH_WIPE_SLOTS] = {
	{"libcrux_sha3_generic_keccak_keccakf1600_21", "c", sizeof(uint64_t[5])},
	{"libcrux_sha3_generic_keccak_keccakf1600_21", "t", sizeof(uint64_t[5])},
	{"libcrux_sha3_generic_keccak_keccakf1600_21", "old", sizeof(uint64_t[5][5])},
	{"libcrux_sha3_generic_keccak_absorb_final_c7", "blocks", sizeof(uint8_t[1][200])},
	{"libcrux_sha3_generic_keccak_absorb_final_c70", "blocks", sizeof(uint8_t[1][200])},
	{"libcrux_sha3_generic_keccak_absorb_final_c71", "blocks", sizeof(uint8_t[1][200])},
	{"libcrux_sha3_generic_keccak_absorb_final_c72", "blocks", sizeof(uint8_t[1][200])},
	{"libcrux_sha3_generic_keccak_squeeze_first_and_last_c5", "b", sizeof(uint8_t[1][200])},
	{"libcrux_sha3_generic_keccak_squeeze_first_and_last_c50", "b", sizeof(uint8_t[1][200])},
	{"libcrux_sha3_generic_keccak_squeeze_last_cf", "b", sizeof(uint8_t[1][200])},
	{"libcrux_sha3_generic_keccak_squeeze_last_cf0", "b", sizeof(uint8_t[1][200])},
	{"libcrux_sha3_generic_keccak_keccak_e9", "s", sizeof(libcrux_sha3_generic_keccak_KeccakState_48)},
	{"libcrux_sha3_generic_keccak_keccak_e90", "s", sizeof(libcrux_sha3_generic_keccak_KeccakState_48)},
	{"libcrux_sha3_generic_keccak_keccak_e91", "s", sizeof(libcrux_sha3_generic_keccak_KeccakState_48)},
};

struct dssh_hash_wipes {
	uint32_t calls[DSSH_HASH_WIPE_SLOTS];
	bool valid;
};

/* Recognition and validity are separate, so a caller can also observe the
 * distinct CCA sites. Unknown sites must be refused by the outer observer. */
static bool
dssh_hash_wipe_record(struct dssh_hash_wipes *observation, const char *function,
    const char *owner, const void *buffer, size_t bytes)
{
	for (size_t i = 0; i < DSSH_HASH_WIPE_SLOTS; i++) {
		if (strcmp(function, dssh_hash_wipe_sites[i].function) != 0
		    || strcmp(owner, dssh_hash_wipe_sites[i].owner) != 0)
			continue;
		if (buffer == NULL || bytes != dssh_hash_wipe_sites[i].bytes
		    || observation->calls[i] == UINT32_MAX) {
			observation->valid = false;
			return true;
		}
		const unsigned char *octets = buffer;
		for (size_t j = 0; j < bytes; j++) {
			if (octets[j] != 0)
				observation->valid = false;
		}
		observation->calls[i]++;
		return true;
	}
	return false;
}

static bool
dssh_hash_wipe_match(const struct dssh_hash_wipes *actual, const struct dssh_hash_wipes *expected)
{
	if (!actual->valid)
		return false;
	for (size_t i = 0; i < DSSH_HASH_WIPE_SLOTS; i++) {
		if (actual->calls[i] != expected->calls[i])
			return false;
	}
	return true;
}

static uint64_t
dssh_hash_wipe_total(const struct dssh_hash_wipes *observation)
{
	uint64_t total = 0;
	/* Fourteen uint32_t values sum to less than2^36. */
	for (size_t i = 0; i < DSSH_HASH_WIPE_SLOTS; i++)
		total += observation->calls[i];
	return total;
}

static bool
dssh_hash_wipe_controls(void)
{
	uint8_t buffer[sizeof(uint64_t[5])] = {0};
	for (unsigned scenario = 0; scenario < 5; scenario++) {
		struct dssh_hash_wipes observation = {.valid = true};
		buffer[0] = scenario == 1 ? 1 : 0;
		const char *function = scenario == 3 ? "unknown" : dssh_hash_wipe_sites[0].function;
		size_t bytes = scenario == 2 ? sizeof(buffer) - 1 : sizeof(buffer);
		if (scenario == 4)
			observation.calls[0] = UINT32_MAX;
		bool known = dssh_hash_wipe_record(&observation, function, "c", buffer, bytes);
		if (scenario == 3) {
			if (known || !observation.valid || observation.calls[0] != 0)
				return false;
		}
		else if (scenario == 0) {
			if (!known || !observation.valid || observation.calls[0] != 1)
				return false;
		}
		else if (!known || observation.valid)
			return false;
	}
	return true;
}

#endif
