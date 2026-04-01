// draft-ietf-sshm-ntruprime-ssh: sntrup761x25519-sha512 (Botan3 native C++ backend)

#include <botan/pubkey.h>
#include <botan/system_rng.h>
#include <botan/x25519.h>

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

/* --- X25519 operations --- */

static int
botan_x25519_keygen(uint8_t pub_out[HYBRID_PQ_X25519_KEY_LEN], void **priv_ctx)
{
	try {
		auto *priv    = new Botan::X25519_PrivateKey(Botan::system_rng());
		auto  pub_raw = priv->public_value();

		if (pub_raw.size() != HYBRID_PQ_X25519_KEY_LEN) {
			delete priv;
			return DSSH_ERROR_INIT;
		}
		memcpy(pub_out, pub_raw.data(), HYBRID_PQ_X25519_KEY_LEN);
		*priv_ctx = priv;
		return 0;
	}
	catch (...) {
		return DSSH_ERROR_INIT;
	}
}

static int
botan_x25519_derive(void *priv_ctx, const uint8_t *peer_pub, uint8_t ss_out[HYBRID_PQ_X25519_KEY_LEN])
{
	auto *priv = static_cast<Botan::X25519_PrivateKey *>(priv_ctx);

	try {
		Botan::PK_Key_Agreement ka(*priv, Botan::system_rng(), "Raw");
		auto                    ss  = ka.derive_key(0, peer_pub, HYBRID_PQ_X25519_KEY_LEN);
		auto                    raw = ss.bits_of();

		if (raw.size() != HYBRID_PQ_X25519_KEY_LEN)
			return DSSH_ERROR_INIT;
		memcpy(ss_out, raw.data(), HYBRID_PQ_X25519_KEY_LEN);
		return 0;
	}
	catch (...) {
		return DSSH_ERROR_INIT;
	}
}

static int
botan_x25519_exchange(const uint8_t *peer_pub, size_t peer_pub_len, uint8_t *our_pub, uint8_t *ss_out)
{
	if (peer_pub_len != HYBRID_PQ_X25519_KEY_LEN)
		return DSSH_ERROR_INVALID;

	void *priv = NULL;

	int res = botan_x25519_keygen(our_pub, &priv);
	if (res < 0)
		return res;
	res = botan_x25519_derive(priv, peer_pub, ss_out);
	delete static_cast<Botan::X25519_PrivateKey *>(priv);
	return res;
}

static void
botan_x25519_free_priv(void *priv_ctx)
{
	delete static_cast<Botan::X25519_PrivateKey *>(priv_ctx);
}

/* --- PQ operations (vendor sntrup761 lib) --- */

static int
sntrup_keygen(uint8_t *pk_out, void **sk_ctx)
{
	uint8_t *sk = static_cast<uint8_t *>(malloc(SNTRUP761_SK_LEN));

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
	uint8_t *sk = static_cast<uint8_t *>(sk_ctx);

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

static const struct hybrid_pq_params sntrup_params = {
    .kex_name     = "sntrup761x25519-sha512",
    .kex_name_len = 22,
    .hash_name    = "SHA-512",
    .digest_len   = 64,
    .pq_pk_len    = SNTRUP761_PK_LEN,
    .pq_ct_len    = SNTRUP761_CT_LEN,
    .pq_ss_len    = SNTRUP761_SS_LEN,
    .q_c_len      = SNTRUP761_PK_LEN + HYBRID_PQ_X25519_KEY_LEN,
    .q_s_len      = SNTRUP761_CT_LEN + HYBRID_PQ_X25519_KEY_LEN,
};

static const struct hybrid_pq_ops botan_sntrup_ops = {
    .x25519_keygen    = botan_x25519_keygen,
    .x25519_derive    = botan_x25519_derive,
    .x25519_exchange  = botan_x25519_exchange,
    .x25519_free_priv = botan_x25519_free_priv,
    .pq_keygen        = sntrup_keygen,
    .pq_encaps        = sntrup_encaps,
    .pq_decaps        = sntrup_decaps,
    .pq_free_sk       = sntrup_free_sk,
};

extern "C" int
dssh_botan_sntrup761x25519_handler(struct dssh_kex_context *kctx)
{
	try {
		return hybrid_pq_handler_impl(kctx, &sntrup_params, &botan_sntrup_ops);
	}
	catch (...) {
		return DSSH_ERROR_INIT;
	}
}

extern "C" void
dssh_botan_sntrup761x25519_cleanup(void *kex_data)
{
	(void)kex_data;
}
