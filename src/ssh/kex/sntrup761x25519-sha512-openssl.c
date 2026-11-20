// RFC 9941: sntrup761x25519-sha512 (OpenSSL backend)
//
// Provides X25519 and sntrup761 crypto operations for the shared
// protocol handler in hybrid-pq-kex.c.

#include <stdlib.h>
#include <string.h>

#include "deucessh-algorithms.h"
#include "deucessh-crypto.h"
#include "deucessh-kex.h"
#include "deucessh.h"
#include "hybrid-pq-kex-ops.h"
#include "sntrup761.h"
#ifdef DSSH_TESTING
 #include "ssh-internal.h"
#endif

#define KEX_NAME     "sntrup761x25519-sha512"
#define KEX_NAME_LEN 22

#define SNTRUP761_PK_LEN crypto_kem_sntrup761_PUBLICKEYBYTES  /* 1158 */
#define SNTRUP761_SK_LEN crypto_kem_sntrup761_SECRETKEYBYTES  /* 1763 */
#define SNTRUP761_CT_LEN crypto_kem_sntrup761_CIPHERTEXTBYTES /* 1039 */
#define SNTRUP761_SS_LEN crypto_kem_sntrup761_BYTES           /* 32 */

static const struct hybrid_pq_params sntrup_params = {
    .kex_name     = KEX_NAME,
    .kex_name_len = KEX_NAME_LEN,
    .hash_name    = "SHA-512",
    .digest_len   = 64,
    .pq_pk_len    = SNTRUP761_PK_LEN,
    .pq_ct_len    = SNTRUP761_CT_LEN,
    .pq_ss_len    = SNTRUP761_SS_LEN,
	.pq_sk_len    = SNTRUP761_SK_LEN,
    .q_c_len      = SNTRUP761_PK_LEN + DSSH_X25519_LEN,
    .q_s_len      = SNTRUP761_CT_LEN + DSSH_X25519_LEN,
};

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
	uint8_t *sk = sk_ctx;

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

static const struct hybrid_pq_ops ossl_sntrup_ops = {
	.x25519          = &dssh_x25519_backend,
    .pq_keygen        = sntrup_keygen,
    .pq_encaps        = sntrup_encaps,
    .pq_decaps        = sntrup_decaps,
    .pq_free_sk       = sntrup_free_sk,
};

DSSH_PRIVATE int
sntrup761x25519_init(bool client, void *config, void **state)
{
	return hybrid_pq_init_impl(client, config, state, &sntrup_params, &ossl_sntrup_ops);
}

DSSH_TESTABLE int
sntrup761x25519_handler(const struct dssh_kex_context *kctx, struct dssh_kex_result *out)
{
	return hybrid_pq_handler_impl(kctx, out, &sntrup_params, &ossl_sntrup_ops);
}

DSSH_PUBLIC int
dssh_register_sntrup761x25519_sha512(void)
{
	struct dssh_kex_s *kex = malloc(sizeof(*kex) + KEX_NAME_LEN + 1);

	if (kex == NULL)
		return DSSH_ERROR_ALLOC;
	kex->next      = NULL;
	kex->init      = sntrup761x25519_init;
	kex->handler   = sntrup761x25519_handler;
	kex->cleanup   = hybrid_pq_cleanup;
	kex->flags     = DSSH_KEX_FLAG_NEEDS_SIGNATURE_CAPABLE | DSSH_KEX_FLAG_K_ENCODING_STRING;
	kex->hash_name = "SHA-512";
	kex->config       = NULL;
	memcpy(kex->name, KEX_NAME, KEX_NAME_LEN + 1);
	int ret = dssh_transport_register_kex(kex);
	if (ret < 0)
		free(kex);
	return ret;
}
