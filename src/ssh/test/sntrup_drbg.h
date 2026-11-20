#ifndef DSSH_SNTRUP_DRBG_H
#define DSSH_SNTRUP_DRBG_H

#include "deucessh-algorithms.h"
#include "dssh_test_internal.h"

/* Test-only reproduction of the submission's AES-256 CTR DRBG, without a
 * derivation function or personalization. CTR over zeros supplies AES(K,V).
 * Every generator operation uses a new key/counter pair through the existing
 * module API. Nothing here is compiled into the library. */
struct sntrup_drbg {
	dssh_enc cipher;
	uint8_t key[32];
	uint8_t counter[16];
	unsigned words;
	unsigned calls;
	bool rho;
	bool encapsulating;
	bool valid;
};

static struct sntrup_drbg *sntrup_active_drbg;

static void
sntrup_drbg_increment(uint8_t counter[16])
{
	for (size_t i = 16; i != 0; i--) {
		if (counter[i - 1] == 255)
			counter[i - 1] = 0;
		else {
			counter[i - 1]++;
			break;
		}
	}
}

static bool
sntrup_drbg_bytes(struct sntrup_drbg *state, uint8_t *out, size_t length)
{
	if (out == NULL || length == 0 || length > 3044)
		return false;
	uint8_t initial[16];
	memcpy(initial, state->counter, sizeof(initial));
	sntrup_drbg_increment(initial);
	dssh_enc_ctx *context = NULL;
	int ret = state->cipher->init(state->key, initial, true, &context);
	if (ret == 0 && context == NULL)
		ret = DSSH_ERROR_INIT;
	if (ret == 0) {
		memset(out, 0, length);
		ret = state->cipher->encrypt(out, length, context);
	}
	if (context != NULL)
		state->cipher->cleanup(context);
	if (ret != 0)
		return false;
	/* Each partial final AES block still advances the reference counter. */
	size_t blocks = length / 16 + (length % 16 != 0);
	for (size_t i = 0; i < blocks; i++)
		sntrup_drbg_increment(state->counter);
	return true;
}

static bool
sntrup_drbg_update(struct sntrup_drbg *state, const uint8_t *seed)
{
	uint8_t temporary[48];
	bool valid = sntrup_drbg_bytes(state, temporary, sizeof(temporary));
	if (valid) {
		if (seed != NULL) {
			for (size_t i = 0; i < sizeof(temporary); i++)
				temporary[i] ^= seed[i];
		}
		memcpy(state->key, temporary, sizeof(state->key));
		memcpy(state->counter, temporary + sizeof(state->key), sizeof(state->counter));
	}
	dssh_cleanse(temporary, sizeof(temporary));
	return valid;
}

static bool
sntrup_drbg_init(struct sntrup_drbg *state, dssh_enc cipher, const uint8_t seed[48])
{
	memset(state, 0, sizeof(*state));
	state->cipher = cipher;
	state->valid = cipher != NULL && cipher->init != NULL && cipher->encrypt != NULL
	    && cipher->cleanup != NULL && cipher->blocksize == 16 && cipher->key_size == 32;
	if (state->valid)
		state->valid = sntrup_drbg_update(state, seed);
	return state->valid;
}

static bool
sntrup_drbg_generate(struct sntrup_drbg *state, uint8_t *out, size_t length)
{
	if (!state->valid)
		return false;
	state->valid = sntrup_drbg_bytes(state, out, length) && sntrup_drbg_update(state, NULL);
	return state->valid;
}

static int
sntrup_deterministic_random(uint8_t *out, size_t length)
{
	struct sntrup_drbg *state = sntrup_active_drbg;
	if (state == NULL)
		return dssh_random(out, length);
	if (!state->valid || state->calls >= 32)
		return DSSH_ERROR_INVALID;
	/* Key generation draws whole word arrays, then one191-byte rho. The
	 * subsequent encapsulation draws exactly one additional word array. */
	if (state->encapsulating) {
		if (!state->rho || length != 3044)
			state->valid = false;
	}
	else if (length == 191 && state->words >= 2 && !state->rho)
		state->rho = true;
	else if (length != 3044 || state->rho)
		state->valid = false;
	if (!state->valid)
		return DSSH_ERROR_INVALID;
	if (length == 3044)
		state->words++;
	state->calls++;
	/* The2020 submission's urandom32 invokes its non-chunk-independent
	 * DRBG once per four bytes. Preserve those request boundaries while
	 * supplying the library's unchanged single-array random request. */
	if (length == 3044) {
		for (size_t offset = 0; offset < length; offset += 4) {
			if (!sntrup_drbg_generate(state, &out[offset], 4))
				return DSSH_ERROR_INIT;
		}
		return 0;
	}
	return sntrup_drbg_generate(state, out, length) ? 0 : DSSH_ERROR_INIT;
}

#endif
