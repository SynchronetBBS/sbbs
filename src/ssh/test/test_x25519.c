/* Actual selected adapter callbacks, using native imports of public vectors. */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "dssh_test.h"
#include "kex/x25519-ops.h"
#include "x25519_fixture.h"
#include "x25519_vectors.h"

DSSH_TEST_NO_CLEANUP

static bool
decode(const char *hex, uint8_t bytes[32])
{
	if (strlen(hex) != 64)
		return false;
	for (size_t i = 0; i < 32; i++) {
		unsigned value = 0;
		for (size_t half = 0; half < 2; half++) {
			char digit = hex[2 * i + half];
			unsigned nibble;
			if (digit >= '0' && digit <= '9')
				nibble = (unsigned)(digit - '0');
			else if (digit >= 'a' && digit <= 'f')
				nibble = 10U + (unsigned)(digit - 'a');
			else
				return false;
			value = value * 16 + nibble;
		}
		uint8_t narrowed = (uint8_t)value; /* Two hexadecimal digits, 0..255. */
		bytes[i] = narrowed;
	}
	return true;
}

static bool
zero(const uint8_t bytes[32])
{
	uint8_t combined = 0;
	for (size_t i = 0; i < 32; i++)
		combined |= bytes[i];
	return combined == 0;
}

static bool
derive(void *key, const uint8_t peer[32], const uint8_t expected[32])
{
	uint8_t output[34];
	memset(output, 0x5a, sizeof(output));
	int ret = dssh_x25519_backend.derive(key, peer, 32, &output[1]);
	bool valid = output[0] == 0x5a && output[33] == 0x5a;
	/* SSH rejects zero agreement; a provider may reject it itself or return
	 * zero for the shared method's mandatory check. Nonzero answers must succeed. */
	valid &= (zero(expected) && ret < 0)
	    || (ret == 0 && memcmp(&output[1], expected, 32) == 0);
	dssh_cleanse(output, sizeof(output));
	return valid;
}

static int
test_vectors(void)
{
	bool valid = true;
	unsigned cases = 0;
	for (unsigned i = 0; i < 2; i++) {
		uint8_t scalar[32], peer[32], expected[32];
		valid &= decode(x25519_primitive[i][0], scalar)
		    && decode(x25519_primitive[i][1], peer) && decode(x25519_primitive[i][2], expected);
		void *key = NULL;
		int ret = valid ? test_x25519_import(scalar, &key) : DSSH_ERROR_INVALID;
		if (ret == 0) {
			valid &= derive(key, peer, expected);
			peer[31] ^= 0x80;
			valid &= derive(key, peer, expected);
			cases += 2;
		}
		else
			valid = false;
		dssh_x25519_backend.free_priv(key);
		dssh_cleanse(scalar, sizeof(scalar));
	}
	for (unsigned party = 0; party < 2; party++) {
		uint8_t scalar[32], peer[32] = {9}, expected[32];
		valid &= decode(x25519_private[party], scalar) && decode(x25519_public[party], expected);
		void *key = NULL;
		int ret = valid ? test_x25519_import(scalar, &key) : DSSH_ERROR_INVALID;
		if (ret == 0) {
			valid &= derive(key, peer, expected);
			valid &= decode(x25519_public[1 - party], peer) && decode(x25519_shared, expected);
			valid &= derive(key, peer, expected);
			cases += 2;
		}
		else
			valid = false;
		dssh_x25519_backend.free_priv(key);
		dssh_cleanse(scalar, sizeof(scalar));
	}
	if (printf("x25519_vectors cases=%u valid=%d\n", cases, valid) < 0)
		valid = false;
	ASSERT_TRUE(valid && cases == 8);
	return TEST_PASS;
}

static int
test_encodings(void)
{
	bool valid = true;
	unsigned cases = 0;
	for (unsigned party = 0; party < 2; party++) {
		uint8_t scalar[32];
		valid &= decode(x25519_private[party], scalar);
		void *key = NULL;
		int ret = valid ? test_x25519_import(scalar, &key) : DSSH_ERROR_INVALID;
		if (ret == 0) {
			for (unsigned coordinate = 0; coordinate < 19; coordinate++) {
				uint8_t expected[32];
				valid &= decode(x25519_small[party][coordinate], expected);
				for (unsigned variant = 0; variant < 4; variant++) {
					uint8_t peer[32];
					memset(peer, variant >= 2 ? 0xff : 0, sizeof(peer));
					uint8_t first = (uint8_t)(coordinate + (variant >= 2 ? 0xedU : 0U));
					peer[0] = first; /* Maximum 18+237=255. */
					peer[31] = variant >= 2 ? 0x7f : 0;
					if ((variant & 1) != 0)
						peer[31] |= 0x80;
					valid &= derive(key, peer, expected);
					cases++;
				}
			}
			uint8_t minus_one[32], expected_zero[32] = {0};
			memset(minus_one, 0xff, sizeof(minus_one));
			minus_one[0] = 0xec;
			minus_one[31] = 0x7f;
			valid &= derive(key, minus_one, expected_zero);
			minus_one[31] |= 0x80;
			valid &= derive(key, minus_one, expected_zero);
			cases += 2;
		}
		else
			valid = false;
		dssh_x25519_backend.free_priv(key);
		dssh_cleanse(scalar, sizeof(scalar));
	}
	if (printf("x25519_encodings cases=%u valid=%d\n", cases, valid) < 0)
		valid = false;
	ASSERT_TRUE(valid && cases == 156);
	return TEST_PASS;
}

static int
test_scalars(void)
{
	bool valid = true;
	unsigned cases = 0;
	for (unsigned party = 0; party < 2; party++) {
		for (unsigned variant = 0; variant < 32; variant++) {
			uint8_t scalar[32], peer[32] = {9}, expected[32];
			valid &= decode(x25519_private[party], scalar) && decode(x25519_public[party], expected);
			uint8_t first = (uint8_t)((scalar[0] & 0xf8U) | (variant & 7U));
			uint8_t last = (uint8_t)((scalar[31] & 0x3fU) | ((variant >> 3) << 6));
			scalar[0] = first;
			scalar[31] = last;
			void *key = NULL;
			int ret = valid ? test_x25519_import(scalar, &key) : DSSH_ERROR_INVALID;
			if (ret == 0) {
				valid &= derive(key, peer, expected);
				valid &= decode(x25519_public[1 - party], peer) && decode(x25519_shared, expected);
				valid &= derive(key, peer, expected);
				cases += 2;
			}
			else
				valid = false;
			dssh_x25519_backend.free_priv(key);
			dssh_cleanse(scalar, sizeof(scalar));
		}
	}
	if (printf("x25519_scalars cases=%u valid=%d\n", cases, valid) < 0)
		valid = false;
	ASSERT_TRUE(valid && cases == 128);
	return TEST_PASS;
}

static int
test_arguments(void)
{
	uint8_t scalar[32], peer[32] = {9}, output[34];
	bool valid = decode(x25519_private[0], scalar);
	void *key = NULL;
	int ret = valid ? test_x25519_import(scalar, &key) : DSSH_ERROR_INVALID;
	valid &= ret == 0;
	unsigned cases = 0;
	if (ret == 0) {
		struct arguments {
			void *key;
			const uint8_t *peer;
			size_t length;
			uint8_t *out;
		};
		const struct arguments arguments[] = {
			{NULL, peer, 32, &output[1]}, {key, NULL, 32, &output[1]},
			{key, peer, 32, NULL}, {key, peer, 0, &output[1]},
			{key, peer, 1, &output[1]}, {key, peer, 31, &output[1]},
			{key, peer, 33, &output[1]}, {key, peer, SIZE_MAX, &output[1]},
		};
		for (size_t i = 0; i < sizeof(arguments) / sizeof(arguments[0]); i++) {
			memset(output, 0x5a, sizeof(output));
			ret = dssh_x25519_backend.derive(arguments[i].key, arguments[i].peer,
			    arguments[i].length, arguments[i].out);
			valid &= ret == DSSH_ERROR_INVALID;
			for (size_t j = 0; j < sizeof(output); j++)
				valid &= output[j] == 0x5a;
			cases++;
		}
	}
	dssh_x25519_backend.free_priv(key);
	dssh_cleanse(scalar, sizeof(scalar));
	if (printf("x25519_arguments cases=%u valid=%d\n", cases, valid) < 0)
		valid = false;
	ASSERT_TRUE(valid && cases == 8);
	return TEST_PASS;
}

static int
test_iterations(void)
{
	const char *extended = getenv("DSSH_TEST_X25519_ITERATIONS");
	ASSERT_TRUE(extended == NULL || strcmp(extended, "1000000") == 0);
	unsigned long limit = extended == NULL ? 1000UL : 1000000UL;
	uint8_t scalar[32] = {9}, peer[32] = {9}, output[32], expected[32];
	bool valid = true;
	unsigned long completed = 0;
	for (unsigned long iteration = 1; iteration <= limit; iteration++) {
		void *key = NULL;
		int ret = test_x25519_import(scalar, &key);
		if (ret == 0)
			ret = dssh_x25519_backend.derive(key, peer, sizeof(peer), output);
		dssh_x25519_backend.free_priv(key);
		if (ret != 0) {
			valid = false;
			break;
		}
		memcpy(peer, scalar, sizeof(peer));
		memcpy(scalar, output, sizeof(scalar));
		completed = iteration;
		if (iteration == 1 || iteration == 1000 || iteration == 1000000) {
			unsigned index = iteration == 1 ? 0U : iteration == 1000 ? 1U : 2U;
			valid &= decode(x25519_iterated[index], expected) && memcmp(scalar, expected, sizeof(scalar)) == 0;
		}
		if (iteration % 100000UL == 0) {
			int written = printf("x25519_iteration_progress=%lu\n", iteration);
			if (written < 0 || fflush(stdout) != 0) {
				valid = false;
				break;
			}
		}
	}
	dssh_cleanse(scalar, sizeof(scalar));
	dssh_cleanse(peer, sizeof(peer));
	dssh_cleanse(output, sizeof(output));
	if (printf("x25519_iterations completed=%lu valid=%d\n", completed, valid) < 0)
		valid = false;
	ASSERT_TRUE(valid && completed == limit);
	return TEST_PASS;
}

static struct dssh_test_entry tests[] = {
	{ "x25519/vectors", test_vectors },
	{ "x25519/encodings", test_encodings },
	{ "x25519/scalars", test_scalars },
	{ "x25519/arguments", test_arguments },
	{ "x25519/iterations", test_iterations },
};

DSSH_TEST_MAIN(tests)
