// RFC 9941: sntrup761x25519-sha512 (Botan3 native C++ backend)

#include <botan/pubkey.h>
#include <botan/system_rng.h>

#include <cstdlib>
#include <cstring>
#include <memory>

#include "deucessh-crypto.h"
#include "deucessh.h"
#include "hybrid-pq-kex-ops.h"
#include "sntrup761.h"

#define SNTRUP761_PK_LEN crypto_kem_sntrup761_PUBLICKEYBYTES
#define SNTRUP761_SK_LEN crypto_kem_sntrup761_SECRETKEYBYTES
#define SNTRUP761_CT_LEN crypto_kem_sntrup761_CIPHERTEXTBYTES
#define SNTRUP761_SS_LEN crypto_kem_sntrup761_BYTES

/* --- PQ operations (vendor sntrup761 lib) --- */

static int
sntrup_keygen(uint8_t *pk_out, uint8_t *storage, size_t capacity, void **sk_ctx)
{
	if (storage == NULL || capacity < SNTRUP761_SK_LEN)
		return DSSH_ERROR_INVALID;
	if (crypto_kem_sntrup761_keypair(pk_out, storage) != 0)
		return DSSH_ERROR_INIT;
	*sk_ctx = storage;
	return 0;
}

static int
sntrup_encaps(uint8_t *ct_out, uint8_t *ss_out, const uint8_t *pk, size_t pk_len)
{
	(void)pk_len;
	if (crypto_kem_sntrup761_enc(ct_out, ss_out, pk) != 0)
		return DSSH_ERROR_INIT;
	return 0;
}

static int
sntrup_decaps(uint8_t *ss_out, const uint8_t *ct, size_t ct_len, void *sk_ctx)
{
	(void)ct_len;
	uint8_t *sk = static_cast<uint8_t *>(sk_ctx);

	if (crypto_kem_sntrup761_dec(ss_out, ct, sk) != 0)
		return DSSH_ERROR_INIT;
	return 0;
}

static void
sntrup_free_sk(void *sk_ctx)
{
	/* The shared handler owns/clears the full retained raw-key slot. */
	(void)sk_ctx;
}

static const struct hybrid_pq_params sntrup_params = {
    .kex_name     = "sntrup761x25519-sha512",
    .kex_name_len = 22,
    .hash_name    = "SHA-512",
    .digest_len   = 64,
    .pq_pk_len    = SNTRUP761_PK_LEN,
    .pq_ct_len    = SNTRUP761_CT_LEN,
    .pq_ss_len    = SNTRUP761_SS_LEN,
	.pq_sk_len    = SNTRUP761_SK_LEN,
    .q_c_len      = SNTRUP761_PK_LEN + DSSH_X25519_LEN,
    .q_s_len      = SNTRUP761_CT_LEN + DSSH_X25519_LEN,
};

static const struct hybrid_pq_ops botan_sntrup_ops = {
	.x25519          = &dssh_x25519_backend,
    .pq_keygen        = sntrup_keygen,
    .pq_encaps        = sntrup_encaps,
    .pq_decaps        = sntrup_decaps,
    .pq_free_sk       = sntrup_free_sk,
};

extern "C" int
sntrup761x25519_init(bool client, void *config, void **state)
{
	try {
		return hybrid_pq_init_impl(client, config, state, &sntrup_params, &botan_sntrup_ops);
	}
	catch (...) {
		return DSSH_ERROR_INIT;
	}
}

extern "C" int
dssh_botan_sntrup761x25519_handler(const struct dssh_kex_context *kctx, struct dssh_kex_result *out)
{
	try {
		return hybrid_pq_handler_impl(kctx, out, &sntrup_params, &botan_sntrup_ops);
	}
	catch (...) {
		return DSSH_ERROR_INIT;
	}
}
