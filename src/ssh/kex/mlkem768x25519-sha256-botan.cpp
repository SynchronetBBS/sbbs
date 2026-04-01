// mlkem768x25519-sha256: ML-KEM-768 + X25519 hybrid PQ KEX (Botan3 native C++ backend)

#include <botan/ml_kem.h>
#include <botan/pk_keys.h>
#include <botan/pubkey.h>
#include <botan/system_rng.h>
#include <botan/x25519.h>

#include <cstdlib>
#include <cstring>
#include <memory>

#include "deucessh-crypto.h"
#include "deucessh.h"
#include "hybrid-pq-kex-ops.h"

#define MLKEM_PK_LEN 1184
#define MLKEM_CT_LEN 1088
#define MLKEM_SS_LEN 32

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

/* --- PQ operations (Botan3 native ML-KEM-768) --- */

static int
mlkem_keygen_botan(uint8_t *pk_out, void **sk_ctx)
{
	try {
		auto *priv = new Botan::ML_KEM_PrivateKey(Botan::system_rng(), Botan::ML_KEM_Mode::ML_KEM_768);

		auto pub_raw = priv->raw_public_key_bits();

		if (pub_raw.size() != MLKEM_PK_LEN) {
			delete priv;
			return DSSH_ERROR_INIT;
		}
		memcpy(pk_out, pub_raw.data(), MLKEM_PK_LEN);
		*sk_ctx = priv;
		return 0;
	}
	catch (...) {
		return DSSH_ERROR_INIT;
	}
}

static int
mlkem_encaps_botan(uint8_t *ct_out, uint8_t *ss_out, const uint8_t *pk, size_t pk_len)
{
	try {
		Botan::ML_KEM_PublicKey pub(std::span<const uint8_t>(pk, pk_len), Botan::ML_KEM_Mode::ML_KEM_768);

		Botan::PK_KEM_Encryptor enc(pub, "Raw");
		auto                    result = enc.encrypt(Botan::system_rng(), MLKEM_SS_LEN);

		if (result.shared_key().size() != MLKEM_SS_LEN)
			return DSSH_ERROR_INIT;
		if (result.encapsulated_shared_key().size() != MLKEM_CT_LEN)
			return DSSH_ERROR_INIT;

		memcpy(ss_out, result.shared_key().data(), MLKEM_SS_LEN);
		memcpy(ct_out, result.encapsulated_shared_key().data(), MLKEM_CT_LEN);
		return 0;
	}
	catch (...) {
		return DSSH_ERROR_INIT;
	}
}

static int
mlkem_decaps_botan(uint8_t *ss_out, const uint8_t *ct, size_t ct_len, void *sk_ctx)
{
	auto *priv = static_cast<Botan::ML_KEM_PrivateKey *>(sk_ctx);

	try {
		Botan::PK_KEM_Decryptor dec(*priv, Botan::system_rng(), "Raw");
		auto                    ss = dec.decrypt(ct, ct_len, MLKEM_SS_LEN);

		if (ss.size() != MLKEM_SS_LEN)
			return DSSH_ERROR_INIT;
		memcpy(ss_out, ss.data(), MLKEM_SS_LEN);
		return 0;
	}
	catch (...) {
		return DSSH_ERROR_INIT;
	}
}

static void
mlkem_free_sk_botan(void *sk_ctx)
{
	delete static_cast<Botan::ML_KEM_PrivateKey *>(sk_ctx);
}

static const struct hybrid_pq_params mlkem_params = {
    .kex_name     = "mlkem768x25519-sha256",
    .kex_name_len = 21,
    .hash_name    = "SHA-256",
    .digest_len   = 32,
    .pq_pk_len    = MLKEM_PK_LEN,
    .pq_ct_len    = MLKEM_CT_LEN,
    .pq_ss_len    = MLKEM_SS_LEN,
    .q_c_len      = MLKEM_PK_LEN + HYBRID_PQ_X25519_KEY_LEN,
    .q_s_len      = MLKEM_CT_LEN + HYBRID_PQ_X25519_KEY_LEN,
};

static const struct hybrid_pq_ops botan_mlkem_ops = {
    .x25519_keygen    = botan_x25519_keygen,
    .x25519_derive    = botan_x25519_derive,
    .x25519_exchange  = botan_x25519_exchange,
    .x25519_free_priv = botan_x25519_free_priv,
    .pq_keygen        = mlkem_keygen_botan,
    .pq_encaps        = mlkem_encaps_botan,
    .pq_decaps        = mlkem_decaps_botan,
    .pq_free_sk       = mlkem_free_sk_botan,
};

extern "C" int
dssh_botan_mlkem768x25519_handler(struct dssh_kex_context *kctx)
{
	try {
		return hybrid_pq_handler_impl(kctx, &mlkem_params, &botan_mlkem_ops);
	}
	catch (...) {
		return DSSH_ERROR_INIT;
	}
}

extern "C" void
dssh_botan_mlkem768x25519_cleanup(void *kex_data)
{
	(void)kex_data;
}
