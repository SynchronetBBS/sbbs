#ifndef DSSH_SNTRUP_KAT_CASES_H
#define DSSH_SNTRUP_KAT_CASES_H

/* Official round-three response/intermediate files, independently supplied by
 * the NTRU Prime authors. Read one record into reusable bounded storage. */
struct sntrup_kat_record {
	uint8_t seed[48];
	uint8_t pk[1158];
	uint8_t sk[1763];
	uint8_t ct[1039];
	uint8_t ss[32];
	uint8_t r[191];
};

static bool
sntrup_kat_line(FILE *file, char *line, size_t capacity)
{
	if (capacity < 2 || capacity > INT_MAX)
		return false;
	int bound = (int)capacity;
	if (fgets(line, bound, file) == NULL)
		return false;
	size_t length = strlen(line);
	if (length == 0 || line[length - 1] != '\n')
		return false;
	line[--length] = 0;
	if (length != 0 && line[length - 1] == '\r')
		line[length - 1] = 0;
	return true;
}

static int
sntrup_kat_digit(char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	return -1;
}

static bool
sntrup_kat_hex(FILE *file, const char *prefix, uint8_t *out, size_t size)
{
	char line[4096];
	size_t start = strlen(prefix);
	if (!sntrup_kat_line(file, line, sizeof(line)) || start > sizeof(line) - 1
	    || size > (sizeof(line) - 1 - start) / 2
	    || strlen(line) != start + 2 * size || memcmp(line, prefix, start) != 0)
		return false;
	for (size_t i = 0; i < size; i++) {
		int high = sntrup_kat_digit(line[start + 2 * i]);
		int low = sntrup_kat_digit(line[start + 2 * i + 1]);
		if (high < 0 || low < 0)
			return false;
		uint8_t octet = (uint8_t)(16 * high + low); /*0..255. */
		out[i] = octet;
	}
	return true;
}

static bool
sntrup_kat_read(FILE *response, FILE *intermediate, unsigned count,
    struct sntrup_kat_record *record)
{
	char line[64], expected[64];
	int printed = snprintf(expected, sizeof(expected), "count = %u", count);
	if (printed <= 0)
		return false;
	size_t extent = (size_t)printed;
	if (extent >= sizeof(expected) || !sntrup_kat_line(response, line, sizeof(line))
	    || strcmp(line, expected) != 0)
		return false;
	uint8_t repeated[191];
	return sntrup_kat_hex(response, "seed = ", record->seed, sizeof(record->seed))
	    && sntrup_kat_hex(response, "pk = ", record->pk, sizeof(record->pk))
	    && sntrup_kat_hex(response, "sk = ", record->sk, sizeof(record->sk))
	    && sntrup_kat_hex(response, "ct = ", record->ct, sizeof(record->ct))
	    && sntrup_kat_hex(response, "ss = ", record->ss, sizeof(record->ss))
	    && sntrup_kat_line(response, line, sizeof(line)) && line[0] == 0
	    && sntrup_kat_hex(intermediate, "Hide r_enc: ", record->r, sizeof(record->r))
	    && sntrup_kat_hex(intermediate, "Hide r_enc: ", repeated, sizeof(repeated))
	    && memcmp(record->r, repeated, sizeof(repeated)) == 0;
}

static bool
sntrup_kat_encoding(const struct sntrup_kat_record *record)
{
	Inputs input;
	uint8_t ciphertext[1041], encoded[193], secret[34], cache[32];
	memset(ciphertext, 0xa5, sizeof(ciphertext));
	memset(encoded, 0xa5, sizeof(encoded));
	memset(secret, 0xa5, sizeof(secret));
	Small_decode(input, record->r);
	unsigned weight = 0;
	for (size_t i = 0; i < sizeof(input) / sizeof(input[0]); i++) {
		if (input[i] < -1 || input[i] > 1)
			return false;
		if (input[i] != 0)
			weight++;
	}
	bool valid = weight == 286 && Hash_prefix(cache, 4, record->pk, sizeof(record->pk)) == 0
	    && memcmp(cache, &record->sk[1731], sizeof(cache)) == 0
	    && Hide(ciphertext + 1, encoded + 1, input, record->pk, cache) == 0
	    && memcmp(ciphertext + 1, record->ct, sizeof(record->ct)) == 0
	    && memcmp(encoded + 1, record->r, sizeof(record->r)) == 0
	    && HashSession(secret + 1, 1, encoded + 1, ciphertext + 1) == 0
	    && memcmp(secret + 1, record->ss, sizeof(record->ss)) == 0
	    && ciphertext[0] == 0xa5 && ciphertext[sizeof(ciphertext) - 1] == 0xa5
	    && encoded[0] == 0xa5 && encoded[sizeof(encoded) - 1] == 0xa5
	    && secret[0] == 0xa5 && secret[sizeof(secret) - 1] == 0xa5;
	return valid;
}

static bool
sntrup_kat_decapsulation(const struct sntrup_kat_record *record)
{
	uint8_t actual[34];
	memset(actual, 0xa5, sizeof(actual));
	return crypto_kem_sntrup761_dec(actual + 1, record->ct, record->sk) == 0
	    && memcmp(actual + 1, record->ss, sizeof(record->ss)) == 0
	    && actual[0] == 0xa5 && actual[sizeof(actual) - 1] == 0xa5;
}

#ifdef DSSH_SNTRUP_DETERMINISTIC
static bool
sntrup_kat_generated(const struct sntrup_kat_record *record, struct sntrup_drbg *seeds)
{
	uint8_t seed[48];
	if (!sntrup_drbg_generate(seeds, seed, sizeof(seed))
	    || memcmp(seed, record->seed, sizeof(seed)) != 0)
		return false;
	struct sntrup_drbg state;
	if (sntrup_active_drbg != NULL || !sntrup_drbg_init(&state, seeds->cipher, seed))
		return false;
	uint8_t pk[1160], sk[1765], ct[1041], secret[34], recovered[34];
	memset(pk, 0xa5, sizeof(pk));
	memset(sk, 0xa5, sizeof(sk));
	memset(ct, 0xa5, sizeof(ct));
	memset(secret, 0xa5, sizeof(secret));
	memset(recovered, 0xa5, sizeof(recovered));
	sntrup_active_drbg = &state;
	bool valid = crypto_kem_sntrup761_keypair(pk + 1, sk + 1) == 0
	    && state.valid && state.rho && state.words >= 2 && state.calls == state.words + 1
	    && memcmp(pk + 1, record->pk, sizeof(record->pk)) == 0
	    && memcmp(sk + 1, record->sk, sizeof(record->sk)) == 0;
	unsigned calls = state.calls;
	state.encapsulating = true;
	if (valid)
		valid = crypto_kem_sntrup761_enc(ct + 1, secret + 1, pk + 1) == 0
		    && state.valid && state.calls == calls + 1
		    && memcmp(ct + 1, record->ct, sizeof(record->ct)) == 0
		    && memcmp(secret + 1, record->ss, sizeof(record->ss)) == 0;
	if (valid)
		valid = crypto_kem_sntrup761_dec(recovered + 1, ct + 1, sk + 1) == 0
		    && state.calls == calls + 1 && memcmp(recovered + 1, record->ss, sizeof(record->ss)) == 0;
	sntrup_active_drbg = NULL;
	valid = pk[0] == 0xa5 && pk[sizeof(pk) - 1] == 0xa5
	    && sk[0] == 0xa5 && sk[sizeof(sk) - 1] == 0xa5
	    && ct[0] == 0xa5 && ct[sizeof(ct) - 1] == 0xa5
	    && secret[0] == 0xa5 && secret[sizeof(secret) - 1] == 0xa5
	    && recovered[0] == 0xa5 && recovered[sizeof(recovered) - 1] == 0xa5 && valid;
	dssh_cleanse(&state, sizeof(state));
	dssh_cleanse(seed, sizeof(seed));
	dssh_cleanse(pk, sizeof(pk));
	dssh_cleanse(sk, sizeof(sk));
	dssh_cleanse(ct, sizeof(ct));
	dssh_cleanse(secret, sizeof(secret));
	dssh_cleanse(recovered, sizeof(recovered));
	return valid;
}
#endif

static bool
sntrup_kat_rejection(const struct sntrup_kat_record *record, unsigned index,
    FILE *answers, unsigned *cases, unsigned *controls)
{
	if (!sntrup_kat_decapsulation(record))
		return false;
	(*controls)++;
	uint8_t key[1765], ciphertext[1041], actual[34], expected[32];
	unsigned limit = index == 0 ? 1266U : 3U;
	for (unsigned which = 0; which < limit; which++) {
		const char *kind;
		unsigned slot = 0;
		size_t position;
		uint8_t mask;
		if (which == 0) {
			kind = "first";
			position = 0;
			mask = 1;
		}
		else if (which == 1) {
			kind = "last";
			position = 1038;
			mask = 128;
		}
		else if (which == 2) {
			kind = "zero";
			position = 0;
			mask = 0;
		}
		else {
			bool confirm = which >= 1010;
			kind = confirm ? "confirm" : "body";
			slot = which - (confirm ? 1010U : 3U);
			position = confirm ? 1007 + slot / 8 : slot;
			uint8_t bit = (uint8_t)(1U << (slot % 8)); /*1..128. */
			mask = bit;
		}
		char line[64], label[64];
		int printed = snprintf(label, sizeof(label), "case = %u:%s:%u", index, kind, slot);
		if (printed <= 0)
			return false;
		size_t extent = (size_t)printed;
		if (extent >= sizeof(label) || !sntrup_kat_line(answers, line, sizeof(line))
		    || strcmp(line, label) != 0 || !sntrup_kat_hex(answers, "fallback = ", expected, sizeof(expected)))
			return false;
		memset(key, 0xa5, sizeof(key));
		memset(ciphertext, 0xa5, sizeof(ciphertext));
		memset(actual, 0xa5, sizeof(actual));
		memcpy(key + 1, record->sk, sizeof(record->sk));
		if (which == 2)
			memset(ciphertext + 1, 0, sizeof(record->ct));
		else {
			memcpy(ciphertext + 1, record->ct, sizeof(record->ct));
			ciphertext[1 + position] ^= mask;
		}
		bool valid = crypto_kem_sntrup761_dec(actual + 1, ciphertext + 1, key + 1) == 0
		    && memcmp(actual + 1, expected, sizeof(expected)) == 0
		    && memcmp(actual + 1, record->ss, sizeof(record->ss)) != 0
		    && memcmp(key + 1, record->sk, sizeof(record->sk)) == 0
		    && key[0] == 0xa5 && key[sizeof(key) - 1] == 0xa5
		    && ciphertext[0] == 0xa5 && ciphertext[sizeof(ciphertext) - 1] == 0xa5
		    && actual[0] == 0xa5 && actual[sizeof(actual) - 1] == 0xa5;
		for (size_t i = 0; i < sizeof(record->ct); i++) {
			uint8_t wanted = which == 2 ? 0 : record->ct[i];
			if (i == position)
				wanted ^= mask;
			valid = ciphertext[i + 1] == wanted && valid;
		}
		if (!valid)
			return false;
		(*cases)++;
	}
	if (!sntrup_kat_decapsulation(record))
		return false;
	(*controls)++;
	return true;
}

static bool
sntrup_kat_run(unsigned mode, unsigned *completed)
{
	FILE *response = fopen(DSSH_SNTRUP_KAT_RESPONSE, "rb");
	FILE *intermediate = fopen(DSSH_SNTRUP_KAT_INTERMEDIATE, "rb");
	FILE *answers = mode == 3 ? fopen(DSSH_SNTRUP_REJECTION_ANSWERS, "rb") : NULL;
	bool valid = response != NULL && intermediate != NULL && (mode != 3 || answers != NULL);
	struct sntrup_kat_record record;
	char line[64];
	unsigned rejected = 0, controls = 0;
#ifdef DSSH_SNTRUP_DETERMINISTIC
	struct sntrup_drbg seeds = {0};
	if (mode == 2) {
		dssh_test_reset_global_config();
		valid = dssh_register_aes256_ctr() == 0 && valid;
		uint8_t entropy[48];
		for (size_t i = 0; i < sizeof(entropy); i++) {
			uint8_t octet = (uint8_t)i; /*0..47. */
			entropy[i] = octet;
		}
		if (valid)
			valid = sntrup_drbg_init(&seeds, gconf.enc_head, entropy);
		dssh_cleanse(entropy, sizeof(entropy));
	}
#endif
	if (valid)
		valid = sntrup_kat_line(response, line, sizeof(line)) && strcmp(line, "# kem/sntrup761") == 0
		    && sntrup_kat_line(response, line, sizeof(line)) && line[0] == 0;
	if (valid && mode == 3)
		valid = sntrup_kat_line(answers, line, sizeof(line))
		    && strcmp(line, "# DeuceSSH sntrup761 rejection answers v1") == 0
		    && sntrup_kat_line(answers, line, sizeof(line)) && line[0] == 0;
	*completed = 0;
	for (unsigned i = 0; valid && i < 100; i++) {
		valid = sntrup_kat_read(response, intermediate, i, &record)
		    && memcmp(record.pk, &record.sk[382], sizeof(record.pk)) == 0;
		if (valid) {
#ifdef DSSH_SNTRUP_DETERMINISTIC
			if (mode == 2)
				valid = sntrup_kat_generated(&record, &seeds);
			else
#endif
				valid = mode == 3 ? sntrup_kat_rejection(&record, i, answers, &rejected, &controls)
				    : mode == 1 ? sntrup_kat_encoding(&record) : sntrup_kat_decapsulation(&record);
		}
		if (valid)
			(*completed)++;
	}
	if (valid)
		valid = fgetc(response) == EOF && !ferror(response)
		    && fgetc(intermediate) == EOF && !ferror(intermediate);
	if (valid && mode == 3)
		valid = fgetc(answers) == EOF && !ferror(answers);
	if (response != NULL && fclose(response) != 0)
		valid = false;
	if (intermediate != NULL && fclose(intermediate) != 0)
		valid = false;
	if (answers != NULL && fclose(answers) != 0)
		valid = false;
	dssh_cleanse(&record, sizeof(record));
#ifdef DSSH_SNTRUP_DETERMINISTIC
	if (mode == 2) {
		valid = sntrup_active_drbg == NULL && valid;
		dssh_cleanse(&seeds, sizeof(seeds));
		dssh_test_reset_global_config();
	}
#endif
	if (mode == 3) {
		valid = valid && rejected == 1563 && controls == 200;
		int printed = printf("sntrup_rejection records=%u cases=%u controls=%u valid=%d\n",
		    *completed, rejected, controls, valid);
		valid = printed > 0 && valid;
	}
	return valid && *completed == 100;
}

static int
test_sntrup_kat_rejection(void)
{
	unsigned completed = 0;
	bool valid = sntrup_kat_run(3, &completed);
	ASSERT_TRUE(valid && completed == 100);
	return TEST_PASS;
}

static int
test_sntrup_kat_decapsulation(void)
{
	unsigned completed = 0;
	bool valid = sntrup_kat_run(false, &completed);
	int printed = printf("sntrup_kat kind=decapsulation vectors=%u valid=%d\n", completed, valid);
	ASSERT_TRUE(valid && completed == 100 && printed > 0);
	return TEST_PASS;
}

static int
test_sntrup_kat_encoding(void)
{
	unsigned completed = 0;
	bool valid = sntrup_kat_run(true, &completed);
	int printed = printf("sntrup_kat kind=encoding vectors=%u valid=%d\n", completed, valid);
	ASSERT_TRUE(valid && completed == 100 && printed > 0);
	return TEST_PASS;
}

#ifdef DSSH_SNTRUP_DETERMINISTIC
static int
test_sntrup_kat_generation(void)
{
	unsigned completed = 0;
	bool valid = sntrup_kat_run(2, &completed);
	int printed = printf("sntrup_kat kind=generation vectors=%u seeds=%u valid=%d\n", completed, completed, valid);
	ASSERT_TRUE(valid && completed == 100 && printed > 0);
	return TEST_PASS;
}
#endif

#endif
