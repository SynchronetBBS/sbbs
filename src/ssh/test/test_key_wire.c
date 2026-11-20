/* Registered builtin callbacks: independent public vectors and wire contracts. */
#include <limits.h>
#include <string.h>
#include "dssh_test.h"
#include "dssh_test_internal.h"
#include "dssh_test_alloc.h"
#include "dssh_test_ossl.h"
#include "deucessh-algorithms.h"
#include "key_wire_vectors.h"
#include "key_algo/key-wire.h"

struct key_wire_fixture {
	uint8_t key[1024];
	size_t key_length;
	uint8_t signature[1024];
	size_t signature_length;
	uint8_t message[512];
	size_t message_length;
};

static int
hex_digit(char value)
{
	if (value >= '0' && value <= '9')
		return value - '0';
	if (value >= 'a' && value <= 'f')
		return value - 'a' + 10;
	return -1;
}

static bool
decode_hex(const char *text, uint8_t *out, size_t capacity, size_t *length)
{
	size_t chars = strlen(text);
	if (chars % 2 != 0 || chars / 2 > capacity)
		return false;
	for (size_t i = 0; i < chars / 2; i++) {
		int high = hex_digit(text[2 * i]);
		int low = hex_digit(text[2 * i + 1]);
		if (high < 0 || low < 0)
			return false;
		uint8_t value = (uint8_t)(16 * high + low);
		out[i] = value;
	}
	*length = chars / 2;
	return true;
}

/* Independent fixture encoder, with checked extents and explicit wire order. */
static bool
put_field(uint8_t *out, size_t capacity, size_t *pos, const void *data, size_t length)
{
	if (*pos > capacity || capacity - *pos < 4
	    || length > capacity - *pos - 4 || length > UINT32_MAX
	    || (data == NULL && length != 0))
		return false;
	uint32_t wire = (uint32_t)length;
	for (unsigned int i = 0; i < 4; i++) {
		uint8_t byte = (uint8_t)((wire >> (24 - 8 * i)) & UINT32_C(255));
		out[(*pos)++] = byte;
	}
	if (length != 0)
		memcpy(&out[*pos], data, length);
	*pos += length;
	return true;
}

static bool
put_integer(uint8_t *out, size_t capacity, size_t *pos,
    const uint8_t *magnitude, size_t length, unsigned int mode)
{
	uint8_t encoded[514];
	while (length != 0 && magnitude[0] == 0) {
		magnitude++;
		length--;
	}
	if (length > sizeof(encoded) - 2)
		return false;
	size_t pad = length != 0 && magnitude[0] >= 128 ? 1U : 0U;
	if (mode == 1)
		pad++;
	if (mode == 2)
		pad = 0;
	memset(encoded, 0, pad);
	if (length != 0)
		memcpy(&encoded[pad], magnitude, length);
	return put_field(out, capacity, pos, encoded, pad + length);
}

static bool
rsa_fixture(const struct key_wire_vector *v, unsigned int variant, struct key_wire_fixture *f)
{
	uint8_t e[512], n[512], signature[512];
	size_t e_len, n_len, signature_len;
	if (!decode_hex(v->e, e, sizeof(e), &e_len)
	    || !decode_hex(v->n, n, sizeof(n), &n_len)
	    || !decode_hex(v->signature, signature, sizeof(signature), &signature_len)
	    || !decode_hex(v->message, f->message, sizeof(f->message), &f->message_length))
		return false;
	size_t pos = 0;
	const char *name = variant == 4 ? "ssh-rsb" : "ssh-rsa";
	if (!put_field(f->key, sizeof(f->key), &pos, name, 7))
		return false;
	if (variant == 6 || variant == 8) {
		const uint8_t zero = 0;
		if (!put_field(f->key, sizeof(f->key), &pos, &zero, variant == 6 ? 0U : 1U))
			return false;
	}
	else if (!put_integer(f->key, sizeof(f->key), &pos, e, e_len, variant == 2 ? 1U : 0U))
		return false;
	if (variant == 7 || variant == 9) {
		const uint8_t zero = 0;
		if (!put_field(f->key, sizeof(f->key), &pos, &zero, variant == 7 ? 0U : 1U))
			return false;
	}
	else if (!put_integer(f->key, sizeof(f->key), &pos, n, n_len,
	    variant == 1 ? 1U : variant == 3 ? 2U : 0U))
		return false;
	if (variant == 5) {
		if (pos == sizeof(f->key))
			return false;
		f->key[pos++] = 0;
	}
	f->key_length = pos;
	pos = 0;
	if (!put_field(f->signature, sizeof(f->signature), &pos, v->algorithm, strlen(v->algorithm))
	    || !put_field(f->signature, sizeof(f->signature), &pos, signature, signature_len))
		return false;
	f->signature_length = pos;
	return true;
}

static bool
ed_fixture(struct key_wire_fixture *f)
{
	/* RFC 8032 section 7.1, TEST 1: public key/signature, empty message. */
	const char *key = "d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a";
	const char *sig = "e5564300c360ac729086e2cc806e828a84877f1eb8e5d974d873e065224901555f"
	    "b8821590a33bacc61e39701cf9b46bd25bf5f0595bbe24655141438e7a100b";
	uint8_t raw_key[32], raw_sig[64];
	size_t key_len, sig_len;
	if (!decode_hex(key, raw_key, sizeof(raw_key), &key_len)
	    || !decode_hex(sig, raw_sig, sizeof(raw_sig), &sig_len))
		return false;
	size_t pos = 0;
	if (!put_field(f->key, sizeof(f->key), &pos, "ssh-ed25519", 11)
	    || !put_field(f->key, sizeof(f->key), &pos, raw_key, key_len))
		return false;
	f->key_length = pos;
	pos = 0;
	if (!put_field(f->signature, sizeof(f->signature), &pos, "ssh-ed25519", 11)
	    || !put_field(f->signature, sizeof(f->signature), &pos, raw_sig, sig_len))
		return false;
	f->signature_length = pos;
	f->message_length = 0;
	return true;
}

static bool
setup(void)
{
	dssh_test_reset_global_config();
	return dssh_register_ssh_ed25519() == 0 && dssh_register_rsa_sha2_256() == 0
	    && dssh_register_rsa_sha2_512() == 0;
}

static int
test_known_answers(void)
{
	ASSERT_TRUE(setup());
	struct key_wire_fixture f;
	ASSERT_TRUE(ed_fixture(&f));
	dssh_key_algo key = find_key_algo("ssh-ed25519");
	ASSERT_NOT_NULL(key);
	ASSERT_EQ(key->key_bits(f.key, f.key_length), 256);
	ASSERT_EQ(key->verify(f.key, f.key_length, f.signature, f.signature_length, NULL, 0), 0);
	for (size_t i = 0; i < sizeof(key_wire_vectors) / sizeof(key_wire_vectors[0]); i++) {
		const struct key_wire_vector *v = &key_wire_vectors[i];
		ASSERT_TRUE(rsa_fixture(v, 0, &f));
		key = find_key_algo(v->algorithm);
		ASSERT_NOT_NULL(key);
		ASSERT_EQ(key->key_bits(f.key, f.key_length), v->bits);
		int ret = key->verify(f.key, f.key_length, f.signature, f.signature_length, f.message, f.message_length);
		ASSERT_TRUE(v->valid ? ret == 0 : ret < 0);
	}
	printf("key-wire published answers: Ed25519=1 RSA-positive=2 RSA-negative=2\n");
	return TEST_PASS;
}

static int
test_ed_metadata(void)
{
	ASSERT_TRUE(setup());
	struct key_wire_fixture f;
	ASSERT_TRUE(ed_fixture(&f));
	dssh_key_algo key = find_key_algo("ssh-ed25519");
	ASSERT_NOT_NULL(key);
	ASSERT_EQ(key->key_bits(NULL, 0), 0);
	for (size_t n = 0; n < f.key_length; n++)
		ASSERT_EQ(key->key_bits(f.key, n), 0);
	f.key[f.key_length] = 0;
	ASSERT_EQ(key->key_bits(f.key, f.key_length + 1), 0);
	f.key[4] = 'x';
	ASSERT_EQ(key->key_bits(f.key, f.key_length), 0);
	return TEST_PASS;
}

static int
test_rsa_metadata(void)
{
	ASSERT_TRUE(setup());
	for (size_t i = 0; i < sizeof(key_wire_vectors) / sizeof(key_wire_vectors[0]); i++) {
		const struct key_wire_vector *v = &key_wire_vectors[i];
		if (!v->valid)
			continue;
		struct key_wire_fixture f;
		ASSERT_TRUE(rsa_fixture(v, 0, &f));
		dssh_key_algo key = find_key_algo(v->algorithm);
		ASSERT_NOT_NULL(key);
		ASSERT_EQ(key->key_bits(NULL, 0), 0);
		for (size_t n = 0; n < f.key_length; n++)
			ASSERT_EQ(key->key_bits(f.key, n), 0);
		for (unsigned int variant = 1; variant <= 9; variant++) {
			ASSERT_TRUE(rsa_fixture(v, variant, &f));
			ASSERT_EQ(key->key_bits(f.key, f.key_length), 0);
		}
	}
	return TEST_PASS;
}

static int
test_rsa_framing(void)
{
	ASSERT_TRUE(setup());
	for (size_t i = 0; i < sizeof(key_wire_vectors) / sizeof(key_wire_vectors[0]); i++) {
		const struct key_wire_vector *v = &key_wire_vectors[i];
		if (!v->valid)
			continue;
		dssh_key_algo key = find_key_algo(v->algorithm);
		ASSERT_NOT_NULL(key);
		for (unsigned int variant = 1; variant <= 9; variant++) {
			struct key_wire_fixture f;
			ASSERT_TRUE(rsa_fixture(v, variant, &f));
			int expected = variant == 5 ? DSSH_ERROR_PARSE : DSSH_ERROR_INVALID;
			ASSERT_EQ(key->verify(f.key, f.key_length, f.signature, f.signature_length,
			    f.message, f.message_length), expected);
		}
	}
	return TEST_PASS;
}

static int
test_metadata_allocations(void)
{
	ASSERT_TRUE(setup());
	for (size_t i = 0; i < sizeof(key_wire_vectors) / sizeof(key_wire_vectors[0]); i++) {
		const struct key_wire_vector *v = &key_wire_vectors[i];
		if (!v->valid)
			continue;
		struct key_wire_fixture f;
		ASSERT_TRUE(rsa_fixture(v, 0, &f));
		dssh_key_algo key = find_key_algo(v->algorithm);
		ASSERT_NOT_NULL(key);
		dssh_test_alloc_fail_after(0);
		dssh_test_ossl_fail_after(0);
		unsigned int bits = key->key_bits(f.key, f.key_length);
		int allocations = dssh_test_alloc_count();
		int native_calls = dssh_test_ossl_count();
		dssh_test_alloc_reset();
		dssh_test_ossl_reset();
		ASSERT_EQ(bits, v->bits);
		ASSERT_EQ(allocations, 0);
		ASSERT_EQ(native_calls, 0);
	}
	return TEST_PASS;
}

static int
test_callback_parameters(void)
{
	ASSERT_TRUE(setup());
	const char *names[] = { "ssh-ed25519", "rsa-sha2-256", "rsa-sha2-512" };
	for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
		struct key_wire_fixture f;
		bool ready = i == 0 ? ed_fixture(&f) : false;
		for (size_t j = 0; !ready && j < sizeof(key_wire_vectors) / sizeof(key_wire_vectors[0]); j++) {
			const struct key_wire_vector *v = &key_wire_vectors[j];
			if (v->valid && strcmp(v->algorithm, names[i]) == 0)
				ready = rsa_fixture(v, 0, &f);
		}
		ASSERT_TRUE(ready);
		dssh_key_algo key = find_key_algo(names[i]);
		ASSERT_NOT_NULL(key);
		ASSERT_EQ(key->verify(f.key, f.key_length, f.signature, f.signature_length,
		    f.message, f.message_length), 0);
		ASSERT_EQ(key->verify(NULL, 0, f.signature, f.signature_length, f.message, f.message_length), DSSH_ERROR_INVALID);
		ASSERT_EQ(key->verify(NULL, 5, f.signature, f.signature_length, f.message, f.message_length), DSSH_ERROR_INVALID);
		ASSERT_EQ(key->verify(f.key, f.key_length, NULL, 0, f.message, f.message_length), DSSH_ERROR_INVALID);
		ASSERT_EQ(key->verify(f.key, f.key_length, NULL, 5, f.message, f.message_length), DSSH_ERROR_INVALID);
		ASSERT_EQ(key->verify(f.key, f.key_length, f.signature, f.signature_length, NULL, 1), DSSH_ERROR_INVALID);
		ASSERT_EQ(key->key_bits(NULL, 5), 0);
	}
	return TEST_PASS;
}

static int
test_bit_limits(void)
{
	/* Independent numeric threshold oracle for all 16-bit magnitudes. The
	 * production helper instead counts a bounded borrowed byte sequence. */
	const unsigned int limits[] = { 0, 7, 8, 9, 15, 16 };
	for (uint32_t value = 0; value <= UINT16_MAX; value++) {
		uint8_t raw[2];
		uint8_t high = (uint8_t)(value >> 8);
		uint8_t low = (uint8_t)(value & UINT32_C(255));
		raw[0] = high;
		raw[1] = low;
		struct dssh_bytes magnitude = {raw + (high == 0 ? 1 : 0), high == 0 ? 1U : 2U};
		unsigned int expected = 0;
		for (unsigned int bit = 0; bit < 16; bit++) {
			if (value >= (UINT32_C(1) << bit))
				expected = bit + 1;
		}
		for (size_t i = 0; i < sizeof(limits) / sizeof(limits[0]); i++)
			ASSERT_EQ(dssh_key_wire_bits(magnitude, limits[i]), expected > limits[i] ? 0 : expected);
	}
	ASSERT_TRUE(setup());
	/* One concrete allocation owns both disjoint spans. Its constant size
	 * fits the minimum C17 size_t; no fictitious oversized object is used. */
	uint8_t *storage = malloc(16693);
	ASSERT_NOT_NULL(storage);
	uint8_t *raw = storage;
	uint8_t *blob = storage + 8193;
	memset(raw, 1, 8193);
	raw[0] = 64;
	struct dssh_bytes magnitude = {raw, 8192};
	bool valid = dssh_key_wire_bits(magnitude, 65535U) == 65535U;
	for (unsigned int mode = 0; mode < 2; mode++) {
		size_t length = mode == 0 ? 8192U : 8193U;
		if (mode == 1) {
			raw[0] = 0;
			raw[1] = 128;
			magnitude.data = &raw[1];
			valid = valid && dssh_key_wire_bits(magnitude, 65535U) == 0;
		}
		size_t pos = 0;
		const uint8_t e = 3;
		valid = valid && put_field(blob, 8500, &pos, "ssh-rsa", 7)
		    && put_field(blob, 8500, &pos, &e, 1)
		    && put_field(blob, 8500, &pos, raw, length);
		unsigned int expected = 65535U;
		if (mode == 1) {
			expected = 0;
#if UINT_MAX > UINT16_MAX
			expected = 65536;
#endif
		}
		const char *names[] = { "rsa-sha2-256", "rsa-sha2-512" };
		for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
			dssh_key_algo key = find_key_algo(names[i]);
			valid = valid && key != NULL && key->key_bits(blob, pos) == expected;
		}
	}
	free(storage);
	printf("key-wire bits: numeric-cases=65536 limits=6 real-boundaries=65535/65536 valid=%u\n", valid ? 1U : 0U);
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

static struct dssh_test_entry tests[] = {
	{ "known_answers", test_known_answers },
	{ "ed_metadata", test_ed_metadata },
	{ "rsa_metadata", test_rsa_metadata },
	{ "rsa_framing", test_rsa_framing },
	{ "metadata_allocations", test_metadata_allocations },
	{ "callback_parameters", test_callback_parameters },
	{ "bit_limits", test_bit_limits },
};

DSSH_TEST_NO_CLEANUP
DSSH_TEST_MAIN(tests)
