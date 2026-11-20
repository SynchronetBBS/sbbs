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
botan_init(void **backend)
{
	*backend = nullptr;
	return 0;
}

static void
botan_cleanup(void *backend)
{
	(void)backend;
}

static int
botan_keygen(void *backend, uint8_t pub_out[DSSH_X25519_LEN], void **priv_ctx)
{
	(void)backend;
	try {
		auto  priv    = std::make_unique<Botan::X25519_PrivateKey>(Botan::system_rng());
		auto  pub_raw = priv->public_value();

		if (pub_raw.size() != DSSH_X25519_LEN) {
			return DSSH_ERROR_INIT;
		}
		memcpy(pub_out, pub_raw.data(), DSSH_X25519_LEN);
		*priv_ctx = priv.release();
		return 0;
	}
	catch (...) {
		return DSSH_ERROR_INIT;
	}
}

static int
botan_derive(void *priv_ctx, const uint8_t *peer_pub, size_t peer_pub_len, uint8_t secret[DSSH_X25519_LEN])
{
	auto *priv = static_cast<Botan::X25519_PrivateKey *>(priv_ctx);
	if (priv == nullptr || peer_pub == nullptr || secret == nullptr || peer_pub_len != DSSH_X25519_LEN)
		return DSSH_ERROR_INVALID;

	try {
		Botan::PK_Key_Agreement ka(*priv, Botan::system_rng(), "Raw");
		auto                    ss  = ka.derive_key(0, peer_pub, peer_pub_len);
		if (ss.length() != DSSH_X25519_LEN)
			return DSSH_ERROR_INIT;
		memcpy(secret, ss.begin(), DSSH_X25519_LEN);
		return 0;
	}
	catch (...) {
		return DSSH_ERROR_INIT;
	}
}

static void
botan_free_priv(void *priv_ctx)
{
	delete static_cast<Botan::X25519_PrivateKey *>(priv_ctx);
}

extern "C" DSSH_PRIVATE const struct dssh_x25519_ops dssh_x25519_backend = {
	.init      = botan_init,
	.cleanup   = botan_cleanup,
    .keygen    = botan_keygen,
    .derive    = botan_derive,
    .free_priv = botan_free_priv,
};

extern "C" int
dssh_botan_curve25519_init(bool client, void *config, void **state)
{
	try {
		return curve25519_init_impl(client, config, state, &dssh_x25519_backend);
	}
	catch (...) {
		return DSSH_ERROR_INIT;
	}
}

static int
curve25519_handler(const struct dssh_kex_context *kctx, struct dssh_kex_result *out)
{
	return curve25519_handler_impl(kctx, out, &dssh_x25519_backend);
}

/* Called from the .c registration wrapper */
extern "C" int
dssh_botan_curve25519_handler(const struct dssh_kex_context *kctx, struct dssh_kex_result *out)
{
	try {
		return curve25519_handler(kctx, out);
	}
	catch (...) {
		return DSSH_ERROR_INIT;
	}
}
