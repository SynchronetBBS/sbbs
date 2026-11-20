// RFC 10042: mlkem768x25519-sha256 (backend-neutral ML-KEM adapter)
//
// Combines the selected backend's X25519 with the bundled ML-KEM-768 for the shared
// protocol handler in hybrid-pq-kex.c.

#include <stdlib.h>
#include <string.h>

#include "deucessh-algorithms.h"
#include "deucessh-crypto.h"
#include "deucessh-kex.h"
#include "deucessh.h"
#include "hybrid-pq-kex-ops.h"
#include "mlkem768.h"
#ifdef DSSH_TESTING
 #include "ssh-internal.h"
#endif

#define KEX_NAME     "mlkem768x25519-sha256"
#define KEX_NAME_LEN 21

#define MLKEM_PK_LEN crypto_kem_mlkem768_PUBLICKEYBYTES  /* 1184 */
#define MLKEM_SK_LEN crypto_kem_mlkem768_SECRETKEYBYTES  /* 2400 */
#define MLKEM_CT_LEN crypto_kem_mlkem768_CIPHERTEXTBYTES /* 1088 */
#define MLKEM_SS_LEN crypto_kem_mlkem768_BYTES           /* 32 */

static const struct hybrid_pq_params mlkem_params = {
    .kex_name     = KEX_NAME,
    .kex_name_len = KEX_NAME_LEN,
    .hash_name    = "SHA-256",
    .digest_len   = 32,
    .pq_pk_len    = MLKEM_PK_LEN,
    .pq_ct_len    = MLKEM_CT_LEN,
    .pq_ss_len    = MLKEM_SS_LEN,
	.pq_sk_len    = MLKEM_SK_LEN,
    .q_c_len      = MLKEM_PK_LEN + DSSH_X25519_LEN,
    .q_s_len      = MLKEM_CT_LEN + DSSH_X25519_LEN,
};

/* --- PQ operations (bundled C17 ML-KEM) --- */

static int
mlkem_keygen(uint8_t *pk_out, uint8_t *storage, size_t capacity, void **sk_ctx)
{
	if (storage == NULL || capacity < MLKEM_SK_LEN)
		return DSSH_ERROR_INVALID;
	if (crypto_kem_mlkem768_keypair(pk_out, storage) != 0)
		return DSSH_ERROR_INIT;
	*sk_ctx = storage;
	return 0;
}

static int
mlkem_encaps(uint8_t *ct_out, uint8_t *ss_out, const uint8_t *pk, size_t pk_len)
{
	(void)pk_len;
	if (crypto_kem_mlkem768_enc(ct_out, ss_out, pk) != 0)
		return DSSH_ERROR_INIT;
	return 0;
}

static int
mlkem_decaps(uint8_t *ss_out, const uint8_t *ct, size_t ct_len, void *sk_ctx)
{
	(void)ct_len;
	uint8_t *sk = sk_ctx;

	if (crypto_kem_mlkem768_dec(ss_out, ct, sk) != 0)
		return DSSH_ERROR_INIT;
	return 0;
}

static void
mlkem_free_sk(void *sk_ctx)
{
	/* The shared handler owns/clears the full retained raw-key slot. */
	(void)sk_ctx;
}

#ifdef DSSH_TESTING
/* Build a valid client fixture through the implementation selected by
 * this backend.  Allocation tests use this instead of reaching through
 * the backend boundary to a particular ML-KEM implementation. */
DSSH_TESTABLE int
dssh_test_mlkem768_public_key(uint8_t *pk_out, size_t pk_len)
{
	if (pk_out == NULL || pk_len != MLKEM_PK_LEN)
		return DSSH_ERROR_INVALID;

	uint8_t storage[MLKEM_SK_LEN];
	void *sk_ctx = NULL;
	int ret = mlkem_keygen(pk_out, storage, sizeof(storage), &sk_ctx);
	mlkem_free_sk(sk_ctx);
	dssh_cleanse(storage, sizeof(storage));
	return ret;
}
#endif

static const struct hybrid_pq_ops mlkem_ops = {
	.x25519          = &dssh_x25519_backend,
    .pq_keygen        = mlkem_keygen,
    .pq_encaps        = mlkem_encaps,
    .pq_decaps        = mlkem_decaps,
    .pq_free_sk       = mlkem_free_sk,
};

DSSH_PRIVATE int
mlkem768x25519_init(bool client, void *config, void **state)
{
	return hybrid_pq_init_impl(client, config, state, &mlkem_params, &mlkem_ops);
}

DSSH_TESTABLE int
mlkem768x25519_handler(const struct dssh_kex_context *kctx, struct dssh_kex_result *out)
{
	return hybrid_pq_handler_impl(kctx, out, &mlkem_params, &mlkem_ops);
}

DSSH_PUBLIC int
dssh_register_mlkem768x25519_sha256(void)
{
	struct dssh_kex_s *kex = malloc(sizeof(*kex) + KEX_NAME_LEN + 1);

	if (kex == NULL)
		return DSSH_ERROR_ALLOC;
	kex->next      = NULL;
	kex->init      = mlkem768x25519_init;
	kex->handler   = mlkem768x25519_handler;
	kex->cleanup   = hybrid_pq_cleanup;
	kex->flags     = DSSH_KEX_FLAG_NEEDS_SIGNATURE_CAPABLE | DSSH_KEX_FLAG_K_ENCODING_STRING;
	kex->hash_name = "SHA-256";
	kex->config       = NULL;
	memcpy(kex->name, KEX_NAME, KEX_NAME_LEN + 1);
	int ret = dssh_transport_register_kex(kex);
	if (ret < 0)
		free(kex);
	return ret;
}
