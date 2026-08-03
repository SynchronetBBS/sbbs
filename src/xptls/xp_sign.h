#ifndef _XP_SIGN_H
#define _XP_SIGN_H

#include <stddef.h>
#include "xp_key.h"

#if defined(__cplusplus)
extern "C" {
#endif

enum xp_sign_algorithm {
	XP_SIGN_RSA_PKCS1_SHA256 = 1,
	XP_SIGN_RSA_PKCS1_SHA384,
	XP_SIGN_RSA_PKCS1_SHA512,
	XP_SIGN_ECDSA_SHA256,
	XP_SIGN_ECDSA_SHA384,
	XP_SIGN_ECDSA_SHA512,
	XP_SIGN_ED25519,
};

enum xp_signature_encoding {
	XP_SIGNATURE_ENCODING_STANDARD = 0, /* DER for ECDSA; raw for RSA and Ed25519. */
	XP_SIGNATURE_ENCODING_P1363,
};

/*
 * These are complete message-level signature schemes: RSA and ECDSA hash the
 * supplied message internally; Ed25519 is PureEdDSA over the supplied bytes.
 * P1363 is valid only for ECDSA. Passing signature == NULL reports a safe
 * maximum size; a successful signing call replaces it with the actual size.
 */

DLLEXPORT int xp_sign(
	xp_key_t key, enum xp_sign_algorithm algorithm,
	enum xp_signature_encoding format, const void *data, size_t data_len,
	void *signature, size_t *signature_len);
DLLEXPORT int xp_verify(
	xp_key_t key, enum xp_sign_algorithm algorithm,
	enum xp_signature_encoding format, const void *data, size_t data_len,
	const void *signature, size_t signature_len);

#if defined(__cplusplus)
}
#endif
#endif
