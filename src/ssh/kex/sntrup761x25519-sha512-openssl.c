// draft-ietf-sshm-ntruprime-ssh: sntrup761x25519-sha512 (OpenSSL backend)
//
// Provides X25519 and sntrup761 crypto operations for the shared
// protocol handler in hybrid-pq-kex.c.

#include <openssl/crypto.h>
#include <openssl/evp.h>

#include <stdlib.h>
#include <string.h>

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
    .q_c_len      = SNTRUP761_PK_LEN + HYBRID_PQ_X25519_KEY_LEN,
    .q_s_len      = SNTRUP761_CT_LEN + HYBRID_PQ_X25519_KEY_LEN,
};

/* --- X25519 operations (OpenSSL EVP) --- */

static int
ossl_x25519_keygen(uint8_t pub_out[HYBRID_PQ_X25519_KEY_LEN], void **priv_ctx)
{
	EVP_PKEY_CTX *pctx = EVP_PKEY_CTX_new_from_name(NULL, "X25519", NULL);
	EVP_PKEY     *key  = NULL;

	if (pctx == NULL)
		return DSSH_ERROR_ALLOC;
	if ((EVP_PKEY_keygen_init(pctx) != 1) || (EVP_PKEY_keygen(pctx, &key) != 1)) {
		EVP_PKEY_CTX_free(pctx);
		return DSSH_ERROR_INIT;
	}
	EVP_PKEY_CTX_free(pctx);

	size_t pub_len = HYBRID_PQ_X25519_KEY_LEN;

	if (EVP_PKEY_get_raw_public_key(key, pub_out, &pub_len) != 1) {
		EVP_PKEY_free(key);
		return DSSH_ERROR_INIT;
	}
	*priv_ctx = key;
	return 0;
}

static int
ossl_x25519_derive(void *priv_ctx, const uint8_t *peer_pub, uint8_t ss_out[HYBRID_PQ_X25519_KEY_LEN])
{
	EVP_PKEY     *our_key  = priv_ctx;
	EVP_PKEY     *peer_key = NULL;
	EVP_PKEY_CTX *dctx     = NULL;

	peer_key = EVP_PKEY_new_raw_public_key_ex(NULL, "X25519", NULL, peer_pub, HYBRID_PQ_X25519_KEY_LEN);
	if (peer_key == NULL)
		return DSSH_ERROR_INIT;

	dctx = EVP_PKEY_CTX_new(our_key, NULL);
	if (dctx == NULL) {
		EVP_PKEY_free(peer_key);
		return DSSH_ERROR_ALLOC;
	}

	size_t ss_len = HYBRID_PQ_X25519_KEY_LEN;

	if ((EVP_PKEY_derive_init(dctx) != 1) || (EVP_PKEY_derive_set_peer(dctx, peer_key) != 1)
	    || (EVP_PKEY_derive(dctx, ss_out, &ss_len) != 1)) {
		EVP_PKEY_CTX_free(dctx);
		EVP_PKEY_free(peer_key);
		return DSSH_ERROR_INIT;
	}
	EVP_PKEY_CTX_free(dctx);
	EVP_PKEY_free(peer_key);
	return 0;
}

static int
ossl_x25519_exchange(const uint8_t *peer_pub, size_t peer_pub_len, uint8_t *our_pub, uint8_t *ss_out)
{
	if (peer_pub_len != HYBRID_PQ_X25519_KEY_LEN)
		return DSSH_ERROR_INVALID;

	void *priv = NULL;

	int res = ossl_x25519_keygen(our_pub, &priv);
	if (res < 0)
		return res;
	res = ossl_x25519_derive(priv, peer_pub, ss_out);
	EVP_PKEY_free(priv);
	return res;
}

static void
ossl_x25519_free_priv(void *priv_ctx)
{
	EVP_PKEY_free(priv_ctx);
}

/* --- PQ operations (vendor sntrup761 lib) --- */

static int
sntrup_keygen(uint8_t *pk_out, void **sk_ctx)
{
	uint8_t *sk = malloc(SNTRUP761_SK_LEN);

	if (sk == NULL)
		return DSSH_ERROR_ALLOC;
	if (crypto_kem_sntrup761_keypair(pk_out, sk) != 0) {
		dssh_cleanse(sk, SNTRUP761_SK_LEN);
		free(sk);
		return DSSH_ERROR_INIT;
	}
	*sk_ctx = sk;
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
	if (sk_ctx != NULL) {
		dssh_cleanse(sk_ctx, SNTRUP761_SK_LEN);
		free(sk_ctx);
	}
}

static const struct hybrid_pq_ops ossl_sntrup_ops = {
    .x25519_keygen    = ossl_x25519_keygen,
    .x25519_derive    = ossl_x25519_derive,
    .x25519_exchange  = ossl_x25519_exchange,
    .x25519_free_priv = ossl_x25519_free_priv,
    .pq_keygen        = sntrup_keygen,
    .pq_encaps        = sntrup_encaps,
    .pq_decaps        = sntrup_decaps,
    .pq_free_sk       = sntrup_free_sk,
};

DSSH_TESTABLE int
sntrup761x25519_handler(struct dssh_kex_context *kctx)
{
	return hybrid_pq_handler_impl(kctx, &sntrup_params, &ossl_sntrup_ops);
}

static void
kex_cleanup(void *kex_data)
{
	(void)kex_data;
}

DSSH_PUBLIC int
dssh_register_sntrup761x25519_sha512(void)
{
	struct dssh_kex_s *kex = malloc(sizeof(*kex) + KEX_NAME_LEN + 1);

	if (kex == NULL)
		return DSSH_ERROR_ALLOC;
	kex->next      = NULL;
	kex->handler   = sntrup761x25519_handler;
	kex->cleanup   = kex_cleanup;
	kex->flags     = DSSH_KEX_FLAG_NEEDS_SIGNATURE_CAPABLE | DSSH_KEX_FLAG_K_ENCODING_STRING;
	kex->hash_name = "SHA-512";
	kex->ctx       = NULL;
	memcpy(kex->name, KEX_NAME, KEX_NAME_LEN + 1);
	return dssh_transport_register_kex(kex);
}
