// RFC 8731: curve25519-sha256 Key Exchange (Botan3 native C++ backend)
//
// Provides X25519 crypto operations for the shared protocol handler
// in curve25519-sha256.c.

#include <botan/pubkey.h>
#include <botan/system_rng.h>
#include <botan/x25519.h>

#include <cstdlib>
#include <cstring>
#include <memory>

#include "curve25519-sha256-ops.h"
#include "deucessh-crypto.h"
#include "deucessh.h"

static int
botan_keygen(uint8_t pub_out[X25519_KEY_LEN], void **priv_ctx)
{
	try {
		auto *priv    = new Botan::X25519_PrivateKey(Botan::system_rng());
		auto  pub_raw = priv->public_value();

		if (pub_raw.size() != X25519_KEY_LEN) {
			delete priv;
			return DSSH_ERROR_INIT;
		}
		memcpy(pub_out, pub_raw.data(), X25519_KEY_LEN);
		*priv_ctx = priv;
		return 0;
	}
	catch (...) {
		return DSSH_ERROR_INIT;
	}
}

static int
botan_derive(void *priv_ctx, const uint8_t *peer_pub, size_t peer_pub_len, uint8_t **secret, size_t *secret_len)
{
	auto *priv = static_cast<Botan::X25519_PrivateKey *>(priv_ctx);

	try {
		Botan::PK_Key_Agreement ka(*priv, Botan::system_rng(), "Raw");
		auto                    ss  = ka.derive_key(0, peer_pub, peer_pub_len);
		auto                    raw = ss.bits_of();

		uint8_t *buf = static_cast<uint8_t *>(malloc(raw.size()));

		if (buf == NULL)
			return DSSH_ERROR_ALLOC;
		memcpy(buf, raw.data(), raw.size());
		*secret     = buf;
		*secret_len = raw.size();
		return 0;
	}
	catch (...) {
		return DSSH_ERROR_INIT;
	}
}

static int
botan_exchange(const uint8_t *peer_pub, size_t peer_pub_len, uint8_t *our_pub, uint8_t **secret,
    size_t *secret_len)
{
	void *priv_ctx = NULL;

	int res = botan_keygen(our_pub, &priv_ctx);
	if (res < 0)
		return res;
	res = botan_derive(priv_ctx, peer_pub, peer_pub_len, secret, secret_len);
	delete static_cast<Botan::X25519_PrivateKey *>(priv_ctx);
	return res;
}

static void
botan_free_priv(void *priv_ctx)
{
	delete static_cast<Botan::X25519_PrivateKey *>(priv_ctx);
}

static const struct dssh_c25519_ops botan_ops = {
    .keygen    = botan_keygen,
    .derive    = botan_derive,
    .exchange  = botan_exchange,
    .free_priv = botan_free_priv,
};

static int
curve25519_handler(struct dssh_kex_context *kctx)
{
	return curve25519_handler_impl(kctx, &botan_ops);
}

static void
kex_cleanup(void *kex_data)
{
	(void)kex_data;
}

/* Called from the .c registration wrapper */
extern "C" int
dssh_botan_curve25519_handler(struct dssh_kex_context *kctx)
{
	try {
		return curve25519_handler(kctx);
	}
	catch (...) {
		return DSSH_ERROR_INIT;
	}
}

extern "C" void
dssh_botan_curve25519_cleanup(void *kex_data)
{
	kex_cleanup(kex_data);
}
