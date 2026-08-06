#ifndef _XP_KEYSET_H
#define _XP_KEYSET_H

#include <stddef.h>

#include "xp_ca.h"

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct xp_keyset *xp_keyset_t;

/*
 * A manifest is an atomic, multi-label working store.  Each private-key
 * record contains either encrypted PKCS#8 DER or a non-secret, versioned
 * xp_key storage reference; certificate records contain DER and are ordered
 * leaf first.  The UTF-8 on-disk grammar is:
 *
 *   xptls-keyset-manifest-v1
 *   K<TAB>base64url(label)<TAB>P|R<TAB>base64url(value)
 *   C<TAB>base64url(label)<TAB>zero-based-index<TAB>base64url(DER)
 *
 * PKCS#12 stores one labelled private key and its leaf-first chain.  xptls
 * writes PBES2 encrypted key material with a SHA-256 integrity MAC and also
 * imports the OpenSSL 3 PBES2 profile and legacy PKCS#12 3DES identities.
 * Password callbacks must return a non-empty secret.
 */

enum xp_keyset_mode {
	XP_KEYSET_CREATE = 1,
	XP_KEYSET_READ_WRITE,
	XP_KEYSET_READ_ONLY,
};

enum xp_keyset_format {
	XP_KEYSET_FORMAT_AUTO = 0,
	XP_KEYSET_FORMAT_MANIFEST,
	XP_KEYSET_FORMAT_PKCS12,
};

enum xp_keyset_object {
	XP_KEYSET_PRIVATE_KEY = 1,
	XP_KEYSET_CERTIFICATE_CHAIN = 2,
};

struct xp_keyset_config {
	const char *path;
	enum xp_keyset_mode mode;
	enum xp_keyset_format format;
	xp_crypto_secret_callback_t password;
	void *password_context;
	/* Used only to resolve versioned xp_key_t storage references. */
	const struct xp_key_store_config *key_store;
};

/* Writable opens hold an exclusive sibling .lock file until close.  Every
 * mutation writes, validates, syncs, and atomically replaces the full store. */

DLLEXPORT int xp_keyset_open(
	xp_keyset_t *out, const struct xp_keyset_config *config);
DLLEXPORT int xp_keyset_close(xp_keyset_t keyset);

DLLEXPORT int xp_keyset_add_private_key(
	xp_keyset_t keyset, const char *label, xp_key_t key);
DLLEXPORT int xp_keyset_get_private_key(
	xp_key_t *out, xp_keyset_t keyset, const char *label);
DLLEXPORT int xp_keyset_add_certificate_chain(
	xp_keyset_t keyset, const char *label,
	const xp_ca_cert_t *certs, size_t count);
DLLEXPORT int xp_keyset_get_certificate_chain(
	xp_ca_cert_t **out, size_t *count,
	xp_keyset_t keyset, const char *label);
DLLEXPORT int xp_keyset_delete(
	xp_keyset_t keyset, const char *label, unsigned objects);

#if defined(__cplusplus)
}
#endif
#endif
