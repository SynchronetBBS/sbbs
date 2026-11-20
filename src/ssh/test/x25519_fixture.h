#ifndef DSSH_TEST_X25519_FIXTURE_H
#define DSSH_TEST_X25519_FIXTURE_H

#include <stdint.h>
#include "deucessh.h"

#ifdef DSSH_CRYPTO_OPENSSL
#include <openssl/evp.h>
static inline int
test_x25519_import(const uint8_t scalar[32], void **out)
{
	EVP_PKEY *key = EVP_PKEY_new_raw_private_key_ex(NULL, "X25519", NULL, scalar, 32);
	if (key == NULL)
		return DSSH_ERROR_INIT;
	*out = key;
	return 0;
}
#else
#ifdef __cplusplus
extern "C" {
#endif
int test_x25519_import(const uint8_t scalar[32], void **out);
#ifdef __cplusplus
}
#endif
#endif

#endif
