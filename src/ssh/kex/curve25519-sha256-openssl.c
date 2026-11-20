// RFC 8731: curve25519-sha256 Key Exchange (OpenSSL backend)
//
// Provides X25519 crypto operations for the shared protocol handler
// in curve25519-sha256.c.

#include <openssl/crypto.h>
#include <openssl/evp.h>

#include <stdlib.h>
#include <string.h>

#include "deucessh-algorithms.h"
#include "curve25519-sha256-ops.h"
#include "deucessh-crypto.h"
#include "deucessh-kex.h"
#include "deucessh.h"
#ifdef DSSH_TESTING
 #include "ssh-internal.h"
#endif

/*
 * Generate an X25519 keypair via OpenSSL EVP.
 * pub_out receives the 32-byte public key; *priv_ctx receives an
 * opaque handle (EVP_PKEY *) for the private key.
 */
static int
ossl_init(void **backend)
{
	EVP_PKEY_CTX *pctx = EVP_PKEY_CTX_new_from_name(NULL, "X25519", NULL);
	if (pctx == NULL)
		return DSSH_ERROR_ALLOC;
	if (EVP_PKEY_keygen_init(pctx) != 1) {
		EVP_PKEY_CTX_free(pctx);
		return DSSH_ERROR_INIT;
	}
	*backend = pctx;
	return 0;
}

static void
ossl_cleanup(void *backend)
{
	EVP_PKEY_CTX_free(backend);
}

static int
ossl_keygen(void *backend, uint8_t pub_out[DSSH_X25519_LEN], void **priv_ctx)
{
	EVP_PKEY_CTX *pctx = backend;
	EVP_PKEY *key = NULL;
	if (EVP_PKEY_keygen(pctx, &key) != 1) {
		EVP_PKEY_free(key);
		return DSSH_ERROR_INIT;
	}

	size_t pub_len = DSSH_X25519_LEN;

	if (EVP_PKEY_get_raw_public_key(key, pub_out, &pub_len) != 1 || pub_len != DSSH_X25519_LEN) {
		EVP_PKEY_free(key);
		return DSSH_ERROR_INIT;
	}

	*priv_ctx = key;
	return 0;
}

/*
 * Derive shared secret from our private key + peer public key.
 * Does NOT free priv_ctx -- caller must call ossl_free_priv.
 */
static int
ossl_derive(void *priv_ctx, const uint8_t *peer_pub, size_t peer_pub_len, uint8_t secret[DSSH_X25519_LEN])
{
	EVP_PKEY     *our_key  = priv_ctx;
	EVP_PKEY     *peer_key = NULL;
	EVP_PKEY_CTX *pctx     = NULL;
	int           res;
	size_t        derived_len = 0;

	if (priv_ctx == NULL || peer_pub == NULL || secret == NULL || peer_pub_len != DSSH_X25519_LEN)
		return DSSH_ERROR_INVALID;

	peer_key = EVP_PKEY_new_raw_public_key_ex(NULL, "X25519", NULL, peer_pub, peer_pub_len);
	if (peer_key == NULL)
		return DSSH_ERROR_INIT;

	pctx = EVP_PKEY_CTX_new(our_key, NULL);
	if ((pctx == NULL) || (EVP_PKEY_derive_init(pctx) != 1)
	    || (EVP_PKEY_derive_set_peer(pctx, peer_key) != 1)) {
		res = DSSH_ERROR_INIT;
		goto done;
	}

	if (EVP_PKEY_derive(pctx, NULL, &derived_len) != 1 || derived_len != DSSH_X25519_LEN) {
		res = DSSH_ERROR_INIT;
		goto done;
	}
	if (EVP_PKEY_derive(pctx, secret, &derived_len) != 1 || derived_len != DSSH_X25519_LEN) {
		OPENSSL_cleanse(secret, DSSH_X25519_LEN);
		res     = DSSH_ERROR_INIT;
		goto done;
	}
	res = 0;

done:
	EVP_PKEY_free(peer_key);
	EVP_PKEY_CTX_free(pctx);
	return res;
}

static void
ossl_free_priv(void *priv_ctx)
{
	EVP_PKEY_free(priv_ctx);
}

DSSH_PRIVATE const struct dssh_x25519_ops dssh_x25519_backend = {
	.init      = ossl_init,
	.cleanup   = ossl_cleanup,
    .keygen    = ossl_keygen,
    .derive    = ossl_derive,
    .free_priv = ossl_free_priv,
};

DSSH_PRIVATE int
curve25519_init(bool client, void *config, void **state)
{
	return curve25519_init_impl(client, config, state, &dssh_x25519_backend);
}

DSSH_TESTABLE int
curve25519_handler(const struct dssh_kex_context *kctx, struct dssh_kex_result *out)
{
	return curve25519_handler_impl(kctx, out, &dssh_x25519_backend);
}

DSSH_PUBLIC int
dssh_register_curve25519_sha256(void)
{
	struct dssh_kex_s *kex = malloc(sizeof(*kex) + C25519_KEX_NAME_LEN + 1);

	if (kex == NULL)
		return DSSH_ERROR_ALLOC;
	kex->next      = NULL;
	kex->init      = curve25519_init;
	kex->handler   = curve25519_handler;
	kex->cleanup   = curve25519_cleanup;
	kex->flags     = DSSH_KEX_FLAG_NEEDS_SIGNATURE_CAPABLE;
	kex->hash_name = "SHA-256";
	kex->config       = NULL;
	memcpy(kex->name, C25519_KEX_NAME, C25519_KEX_NAME_LEN + 1);
	int ret = dssh_transport_register_kex(kex);
	if (ret < 0)
		free(kex);
	return ret;
}
