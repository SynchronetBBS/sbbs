/* Fresh-process production consumer: configure the native RNG before use. */
#include <stdio.h>
#include <string.h>

#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/rand.h>

#include "deucessh.h"
#include "deucessh-crypto.h"

#define REQUIRE(condition) do { \
	if (!(condition)) { \
		if (fprintf(stderr, "RNG provider check failed at line %d: %s\n", \
			__LINE__, #condition) < 0) \
			return 2; \
		return 1; \
	} \
} while (0)

int
main(int argc, char **argv)
{
	if (argc != 2)
		return 2;
	const char *cipher;
	unsigned int strength;
	if (strcmp(argv[1], "aes128") == 0) {
		cipher = "AES-128-CTR";
		strength = 128;
	}
	else if (strcmp(argv[1], "aes192") == 0) {
		cipher = "AES-192-CTR";
		strength = 192;
	}
	else if (strcmp(argv[1], "aes256") == 0) {
		cipher = "AES-256-CTR";
		strength = 256;
	}
	else if (strcmp(argv[1], "invalid") == 0) {
		cipher = "DEUCESSH-INVALID-CIPHER";
		strength = 0;
	}
	else {
		return 2;
	}

	REQUIRE(RAND_set_DRBG_type(NULL, "CTR-DRBG", NULL, cipher, NULL) == 1);
	uint8_t frame[34];
	memset(frame, 0xa5, sizeof(frame));
	REQUIRE(dssh_random(NULL, 32) == DSSH_ERROR_INVALID);
	REQUIRE(dssh_random(frame + 1, 0) == 0);
	/* A second configuration succeeds only before native RNG creation. */
	REQUIRE(RAND_set_DRBG_type(NULL, "CTR-DRBG", NULL, cipher, NULL) == 1);
	uint8_t native[32];
	int native_result = RAND_priv_bytes_ex(NULL, native, sizeof(native), 0);
	EVP_RAND_CTX *context = RAND_get0_private(NULL);
	if (strength == 0) {
		REQUIRE(native_result != 1);
		REQUIRE(context == NULL);
	}
	else {
		REQUIRE(native_result == 1);
		REQUIRE(context != NULL);
		REQUIRE(EVP_RAND_get_strength(context) == strength);
	}
	ERR_clear_error();
	int expected = strength == 256 ? 0 : DSSH_ERROR_INIT;
	unsigned isolated = 0;
#ifdef DSSH_TEST_RNG_PRIVATE_ARCHIVE
	/* The application's configured provider is a separate archive instance. */
	expected = 0;
	isolated = 1;
#endif
	int actual = dssh_random(frame + 1, 32);
	REQUIRE(actual == expected);
	REQUIRE(frame[0] == 0xa5 && frame[33] == 0xa5);
	if (expected != 0) {
		/* These native configuration refusals occur before output generation. */
		for (size_t i = 1; i < 33; i++)
			REQUIRE(frame[i] == 0xa5);
	}
	if (strength != 0) {
		ERR_clear_error();
		REQUIRE(RAND_priv_bytes_ex(NULL, native, sizeof(native), 0) == 1);
		REQUIRE(EVP_RAND_get_strength(context) == strength);
	}
	else {
		ERR_clear_error();
		REQUIRE(RAND_priv_bytes_ex(NULL, native, sizeof(native), 0) != 1);
		REQUIRE(RAND_get0_private(NULL) == NULL);
	}
	dssh_cleanse(native, sizeof(native));
	dssh_cleanse(frame, sizeof(frame));
	if (printf("rng_policy=1 mode=%s production=1 bits=%u isolated=%u\n",
	    argv[1], strength, isolated) < 0 || fflush(stdout) != 0)
		return 2;
	return 0;
}

#undef REQUIRE
