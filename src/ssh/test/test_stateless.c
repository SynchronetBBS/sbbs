/* Public-header-only packet adapters. NULL is the successful context, and
 * the nonempty MAC has a zero-byte key. This code has no cryptographic value. */
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

#include "deucessh.h"
#include "deucessh-enc.h"
#include "deucessh-mac.h"
#include "test_stateless.h"

static atomic_uint enc_inits, enc_cleanups, encryptions, decryptions;
static atomic_uint mac_inits, mac_cleanups, generations, errors;

int
dssh_test_stateless_reset(void)
{
	if (atomic_load(&enc_inits) != atomic_load(&enc_cleanups)
	    || atomic_load(&mac_inits) != atomic_load(&mac_cleanups))
		return DSSH_ERROR_INIT;
	atomic_store(&enc_inits, 0);
	atomic_store(&enc_cleanups, 0);
	atomic_store(&encryptions, 0);
	atomic_store(&decryptions, 0);
	atomic_store(&mac_inits, 0);
	atomic_store(&mac_cleanups, 0);
	atomic_store(&generations, 0);
	atomic_store(&errors, 0);
	return 0;
}

void
dssh_test_stateless_stats(struct dssh_test_stateless_stats *out)
{
	out->enc_init = atomic_load(&enc_inits);
	out->enc_cleanup = atomic_load(&enc_cleanups);
	out->encrypt = atomic_load(&encryptions);
	out->decrypt = atomic_load(&decryptions);
	out->mac_init = atomic_load(&mac_inits);
	out->mac_cleanup = atomic_load(&mac_cleanups);
	out->generate = atomic_load(&generations);
	out->errors = atomic_load(&errors);
}

static int
stateless_enc_init(const uint8_t *key, const uint8_t *iv, bool encrypt, dssh_enc_ctx **out)
{
	(void)key;
	(void)iv;
	(void)encrypt;
	*out = NULL;
	atomic_fetch_add(&enc_inits, 1);
	return 0;
}

static int
stateless_crypt(uint8_t *buf, size_t length, dssh_enc_ctx *ctx)
{
	if (ctx != NULL || length % 16 != 0) {
		atomic_fetch_add(&errors, 1);
		return DSSH_ERROR_INVALID;
	}
	for (size_t i = 0; i < length; i++)
		buf[i] ^= 0x5a;
	return 0;
}

static int
stateless_encrypt(uint8_t *buf, size_t length, dssh_enc_ctx *ctx)
{
	atomic_fetch_add(&encryptions, 1);
	return stateless_crypt(buf, length, ctx);
}

static int
stateless_decrypt(uint8_t *buf, size_t length, dssh_enc_ctx *ctx)
{
	atomic_fetch_add(&decryptions, 1);
	return stateless_crypt(buf, length, ctx);
}

static void
stateless_enc_cleanup(dssh_enc_ctx *ctx)
{
	if (ctx != NULL)
		atomic_fetch_add(&errors, 1);
	atomic_fetch_add(&enc_cleanups, 1);
}

static int
stateless_mac_init(const uint8_t *key, dssh_mac_ctx **out)
{
	(void)key;
	*out = NULL;
	atomic_fetch_add(&mac_inits, 1);
	return 0;
}

static int
stateless_generate(const uint8_t *buf, size_t length, uint8_t *out, dssh_mac_ctx *ctx)
{
	if (ctx != NULL) {
		atomic_fetch_add(&errors, 1);
		return DSSH_ERROR_INVALID;
	}
	memset(out, 0, 16);
	for (size_t i = 0; i < length; i++)
		out[i % 16] ^= buf[i];
	atomic_fetch_add(&generations, 1);
	return 0;
}

static void
stateless_mac_cleanup(dssh_mac_ctx *ctx)
{
	if (ctx != NULL)
		atomic_fetch_add(&errors, 1);
	atomic_fetch_add(&mac_cleanups, 1);
}

int
dssh_test_register_stateless(void)
{
	dssh_enc enc = calloc(1, sizeof(*enc) + sizeof(DSSH_TEST_STATELESS_NAME));
	if (enc == NULL)
		return DSSH_ERROR_ALLOC;
	enc->init = stateless_enc_init;
	enc->encrypt = stateless_encrypt;
	enc->decrypt = stateless_decrypt;
	enc->cleanup = stateless_enc_cleanup;
	enc->bytes_per_key = UINT64_MAX;
	enc->blocksize = 16;
	memcpy(enc->name, DSSH_TEST_STATELESS_NAME, sizeof(DSSH_TEST_STATELESS_NAME));
	int ret = dssh_transport_register_enc(enc);
	if (ret != 0) {
		free(enc);
		return ret;
	}
	dssh_mac mac = calloc(1, sizeof(*mac) + sizeof(DSSH_TEST_STATELESS_NAME));
	if (mac == NULL)
		return DSSH_ERROR_ALLOC;
	mac->init = stateless_mac_init;
	mac->generate = stateless_generate;
	mac->cleanup = stateless_mac_cleanup;
	mac->digest_size = 16;
	memcpy(mac->name, DSSH_TEST_STATELESS_NAME, sizeof(DSSH_TEST_STATELESS_NAME));
	ret = dssh_transport_register_mac(mac);
	if (ret != 0)
		free(mac);
	return ret;
}
