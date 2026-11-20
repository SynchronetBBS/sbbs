/* Production-library C17 entry: native keys, callback unwinding and provider
 * ownership are checked without including any private library declaration. */
#if defined(__linux__) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "deucessh.h"
#include "deucessh-algorithms.h"
#include "deucessh-crypto.h"
#ifdef DSSH_PROVIDER_SYSTEM_OPENSSL
#include <dlfcn.h>
#include <openssl/evp.h>
#endif
#ifdef DSSH_PROVIDER_CPP_CONTROLS
void dssh_provider_cpp_arm(void);
void dssh_provider_cpp_disable(void);
bool dssh_provider_cpp_threw(void);
#endif

struct provider_key {
	int (*reg)(void);
	int (*generate)(void);
	int (*load)(const char *, dssh_pem_password_cb, void *);
	int (*save)(const char *, dssh_pem_password_cb, void *);
	int64_t (*text)(char *, size_t);
	unsigned cache;
};
static int
provider_rsa256_generate(void)
{
	return dssh_rsa_sha2_256_generate_key(2048);
}
static int
provider_rsa512_generate(void)
{
	return dssh_rsa_sha2_512_generate_key(2048);
}
static const struct provider_key keys[] = {
	{dssh_register_ssh_ed25519, dssh_ed25519_generate_key, dssh_ed25519_load_key_file,
	    dssh_ed25519_save_key_file, dssh_ed25519_get_pub_str, 2},
	{dssh_register_rsa_sha2_256, provider_rsa256_generate, dssh_rsa_sha2_256_load_key_file,
	    dssh_rsa_sha2_256_save_key_file, dssh_rsa_sha2_256_get_pub_str, 3},
	{dssh_register_rsa_sha2_512, provider_rsa512_generate, dssh_rsa_sha2_512_load_key_file,
	    dssh_rsa_sha2_512_save_key_file, dssh_rsa_sha2_512_get_pub_str, 3},
};
struct provider_password {
	unsigned calls;
	bool writing;
	bool valid;
	bool reject;
	bool wrong;
	bool arm;
};

static int
provider_password(char *buffer, int capacity, int writing, void *opaque)
{
	struct provider_password *state = opaque;
	if (buffer == NULL || capacity < 256 || (writing != 0 && writing != 1)
	    || (writing != 0) != state->writing || state->calls == UINT_MAX) {
		state->valid = false;
		return DSSH_ERROR_INVALID;
	}
	state->calls++;
	if (state->reject)
		return DSSH_ERROR_INVALID;
	memset(buffer, 'A', 256);
	buffer[128] = 0;
	if (state->wrong)
		buffer[0] = 'B';
#ifdef DSSH_PROVIDER_CPP_CONTROLS
	if (state->arm) {
		state->arm = false;
		dssh_provider_cpp_arm();
	}
#endif
	return 256;
}

static bool
provider_key_snapshot(const struct provider_key *key, char **text,
    size_t *capacity, uint8_t digest[32])
{
	int64_t count = key->text(NULL, 0);
	if (count < 1)
		return false;
#if SIZE_MAX < INT64_MAX
	if (count > SIZE_MAX)
		return false;
#endif
	size_t needed = (size_t)count;
	if (needed > *capacity) {
		char *replacement = realloc(*text, needed);
		if (replacement == NULL)
			return false;
		*text = replacement;
		*capacity = needed;
	}
	const uint8_t *bytes = (const uint8_t *)*text;
	return key->text(*text, *capacity) == count && (*text)[needed - 1] == 0
	    && dssh_hash_oneshot("SHA-256", bytes, needed, digest, 32) == 0;
}

static bool
provider_hash_and_owner(void)
{
#ifdef DSSH_PROVIDER_SYSTEM_OPENSSL
	Dl_info library, provider;
	void *library_symbol = dlsym(RTLD_DEFAULT, "dssh_hash_oneshot");
	void *provider_symbol = dlsym(RTLD_DEFAULT, "EVP_Digest");
	bool separate = library_symbol != NULL && provider_symbol != NULL
	    && dladdr(library_symbol, &library) != 0 && dladdr(provider_symbol, &provider) != 0
	    && library.dli_fbase != provider.dli_fbase;
	if (printf("provider_owner separate=%u\n", separate ? 1U : 0U) < 0 || fflush(stdout) != 0)
		return false;
	if (!separate)
		return false;
#endif
	static const uint8_t message[] = {'a', 'b', 'c'};
	static const uint8_t expected[32] = {
		0xba,0x78,0x16,0xbf,0x8f,0x01,0xcf,0xea,0x41,0x41,0x40,0xde,0x5d,0xae,0x22,0x23,
		0xb0,0x03,0x61,0xa3,0x96,0x17,0x7a,0x9c,0xb4,0x10,0xff,0x61,0xf2,0x00,0x15,0xad,
	};
	uint8_t actual[32];
	if (dssh_hash_oneshot("SHA-256", message, sizeof(message), actual, sizeof(actual)) != 0
	    || memcmp(actual, expected, sizeof(actual)) != 0)
		return false;
#ifdef DSSH_PROVIDER_SYSTEM_OPENSSL
	const EVP_MD *algorithm = EVP_sha256();
	uint8_t external[EVP_MAX_MD_SIZE];
	unsigned int length = 0;
	if (algorithm == NULL || EVP_Digest(message, sizeof(message), external, &length, algorithm, NULL) != 1
	    || length != sizeof(expected) || memcmp(external, expected, sizeof(expected)) != 0)
		return false;
#endif
	return true;
}

int
main(int argc, char **argv)
{
	if (argc != 4 || !provider_hash_and_owner())
		return 1;
	unsigned faults = 0;
#ifdef DSSH_PROVIDER_CPP_CONTROLS
	dssh_hash_ctx *hash = NULL;
	size_t length = 123;
	dssh_provider_cpp_arm();
	int status = dssh_hash_init(&hash, "SHA-256", &length);
	bool threw = dssh_provider_cpp_threw();
	dssh_provider_cpp_disable();
	bool untouched = hash == NULL && length == 123;
	dssh_hash_free(hash);
	if (status != DSSH_ERROR_ALLOC || !untouched || !threw)
		return 1;
	faults++;
#endif
	char *text = NULL;
	size_t capacity = 0;
	bool valid = true;
	bool touched = false;
	unsigned completed = 0, rejected = 0, wrong = 0;
	for (size_t i = 0; valid && i < sizeof(keys) / sizeof(keys[0]); i++) {
		const struct provider_key *key = &keys[i];
		uint8_t original[32], candidate[32], current[32];
		valid = key->reg() == 0 && key->load(argv[key->cache], NULL, NULL) == 0
		    && provider_key_snapshot(key, &text, &capacity, original);
		if (valid)
			valid = key->generate() == 0 && provider_key_snapshot(key, &text, &capacity, candidate)
			    && memcmp(original, candidate, sizeof(original)) != 0;
		struct provider_password password = {.writing = true, .valid = true};
		if (valid) {
			touched = true;
			valid = key->save(argv[1], provider_password, &password) == 0
			    && password.valid && password.calls != 0;
		}
		if (valid)
			valid = key->load(argv[key->cache], NULL, NULL) == 0
			    && provider_key_snapshot(key, &text, &capacity, current)
			    && memcmp(original, current, sizeof(original)) == 0;
		password = (struct provider_password){.valid = true, .reject = true};
		if (valid) {
			valid = key->load(argv[1], provider_password, &password) < 0
			    && password.valid && password.calls != 0
			    && provider_key_snapshot(key, &text, &capacity, current)
			    && memcmp(original, current, sizeof(original)) == 0;
			if (valid)
				rejected++;
		}
		password = (struct provider_password){.valid = true, .wrong = true};
		if (valid) {
			valid = key->load(argv[1], provider_password, &password) < 0
			    && password.valid && password.calls != 0
			    && provider_key_snapshot(key, &text, &capacity, current)
			    && memcmp(original, current, sizeof(original)) == 0;
			if (valid)
				wrong++;
		}
#ifdef DSSH_PROVIDER_CPP_CONTROLS
		password = (struct provider_password){.valid = true, .arm = true};
		if (valid) {
			int ret = key->load(argv[1], provider_password, &password);
			bool failed = dssh_provider_cpp_threw();
			dssh_provider_cpp_disable();
			valid = ret < 0 && failed && password.valid && password.calls != 0
			    && provider_key_snapshot(key, &text, &capacity, current)
			    && memcmp(original, current, sizeof(original)) == 0;
			if (valid)
				faults++;
		}
#endif
		password = (struct provider_password){.valid = true};
		if (valid) {
			valid = key->load(argv[1], provider_password, &password) == 0
			    && password.valid && password.calls != 0
			    && provider_key_snapshot(key, &text, &capacity, current)
			    && memcmp(candidate, current, sizeof(candidate)) == 0;
			if (valid)
				completed++;
		}
	}
	free(text);
	if (touched && remove(argv[1]) != 0)
		valid = false;
	unsigned cpp = 0;
#ifdef DSSH_PROVIDER_CPP_CONTROLS
	cpp = 1;
#endif
	valid = valid && completed == 3 && rejected == 3 && wrong == 3 && faults == cpp * 4;
	if (printf("provider_boundary algorithms=%u rejected=%u wrong=%u cpp=%u faults=%u valid=%u\n",
	    completed, rejected, wrong, cpp, faults, valid ? 1U : 0U) < 0 || fflush(stdout) != 0)
		return 2;
	return valid ? 0 : 1;
}
