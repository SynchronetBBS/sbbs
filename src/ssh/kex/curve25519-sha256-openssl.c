// RFC 8731: curve25519-sha256 Key Exchange (OpenSSL backend)
//
// Provides X25519 crypto operations for the shared protocol handler
// in curve25519-sha256.c.

#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <stdlib.h>
#include <string.h>

#include "deucessh.h"
#include "deucessh-crypto.h"
#include "deucessh-kex.h"
#include "curve25519-sha256-ops.h"
#ifdef DSSH_TESTING
#include "ssh-internal.h"
#endif

/*
 * Generate an X25519 keypair via OpenSSL EVP.
 * pub_out receives the 32-byte public key; *priv_ctx receives an
 * opaque handle (EVP_PKEY *) for the private key.
 */
static int
ossl_keygen(uint8_t pub_out[X25519_KEY_LEN], void **priv_ctx)
{
	EVP_PKEY_CTX *pctx = EVP_PKEY_CTX_new_from_name(NULL, "X25519", NULL);
	EVP_PKEY     *key = NULL;

	if (pctx == NULL)
		return DSSH_ERROR_ALLOC;
	if ((EVP_PKEY_keygen_init(pctx) != 1)
	    || (EVP_PKEY_keygen(pctx, &key) != 1)) {
		EVP_PKEY_CTX_free(pctx);
		return DSSH_ERROR_INIT;
	}
	EVP_PKEY_CTX_free(pctx);

	size_t pub_len = X25519_KEY_LEN;

	if (EVP_PKEY_get_raw_public_key(key, pub_out, &pub_len) != 1) {
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
ossl_derive(void *priv_ctx, const uint8_t *peer_pub,
    size_t peer_pub_len, uint8_t **secret, size_t *secret_len)
{
	EVP_PKEY     *our_key = priv_ctx;
	EVP_PKEY     *peer_key = NULL;
	EVP_PKEY_CTX *pctx = NULL;
	int           res;

	peer_key = EVP_PKEY_new_raw_public_key_ex(NULL, "X25519", NULL,
	    peer_pub, peer_pub_len);
	if (peer_key == NULL)
		return DSSH_ERROR_INIT;

	pctx = EVP_PKEY_CTX_new(our_key, NULL);
	if ((pctx == NULL) || (EVP_PKEY_derive_init(pctx) != 1)
	    || (EVP_PKEY_derive_set_peer(pctx, peer_key) != 1)) {
		res = DSSH_ERROR_INIT;
		goto done;
	}

	*secret_len = 0;
	if (EVP_PKEY_derive(pctx, NULL, secret_len) != 1) {
		res = DSSH_ERROR_INIT;
		goto done;
	}
	*secret = malloc(*secret_len);
	if (*secret == NULL) {
		res = DSSH_ERROR_ALLOC;
		goto done;
	}
	if (EVP_PKEY_derive(pctx, *secret, secret_len) != 1) {
		OPENSSL_cleanse(*secret, *secret_len);
		free(*secret);
		*secret = NULL;
		res = DSSH_ERROR_INIT;
		goto done;
	}
	res = 0;

done:
	EVP_PKEY_free(peer_key);
	EVP_PKEY_CTX_free(pctx);
	return res;
}

/*
 * Combined keygen + derive (server-side helper).
 * Generates an X25519 keypair, writes our public key to our_pub,
 * and computes the shared secret from peer_pub.
 */
DSSH_TESTABLE int
x25519_exchange(const uint8_t *peer_pub, size_t peer_pub_len,
    uint8_t *our_pub, uint8_t **secret, size_t *secret_len)
{
	void *priv_ctx = NULL;

	int res = ossl_keygen(our_pub, &priv_ctx);
	if (res < 0)
		return res;
	res = ossl_derive(priv_ctx, peer_pub, peer_pub_len, secret, secret_len);
	EVP_PKEY_free(priv_ctx);
	return res;
}

static void
ossl_free_priv(void *priv_ctx)
{
	EVP_PKEY_free(priv_ctx);
}

static const struct dssh_c25519_ops ossl_ops = {
	.keygen    = ossl_keygen,
	.derive    = ossl_derive,
	.exchange  = x25519_exchange,
	.free_priv = ossl_free_priv,
};

DSSH_TESTABLE int
curve25519_handler(struct dssh_kex_context *kctx)
{
	return curve25519_handler_impl(kctx, &ossl_ops);
}

static void
kex_cleanup(void *kex_data)
{
	(void)kex_data;
}

DSSH_PUBLIC int
dssh_register_curve25519_sha256(void)
{
	struct dssh_kex_s *kex = malloc(sizeof(*kex) + C25519_KEX_NAME_LEN + 1);

	if (kex == NULL)
		return DSSH_ERROR_ALLOC;
	kex->next = NULL;
	kex->handler = curve25519_handler;
	kex->cleanup = kex_cleanup;
	kex->flags = DSSH_KEX_FLAG_NEEDS_SIGNATURE_CAPABLE;
	kex->hash_name = "SHA-256";
	kex->ctx = NULL;
	memcpy(kex->name, C25519_KEX_NAME, C25519_KEX_NAME_LEN + 1);
	return dssh_transport_register_kex(kex);
}
