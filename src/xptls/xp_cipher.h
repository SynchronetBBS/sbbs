#ifndef _XP_CIPHER_H
#define _XP_CIPHER_H

#include <stdbool.h>
#include <stddef.h>

#include "xp_crypto.h"

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct xp_cipher_context *xp_cipher_t;

enum xp_cipher_algorithm {
	XP_CIPHER_AES = 1,
	XP_CIPHER_CHACHA20,
};

enum xp_cipher_mode {
	XP_CIPHER_MODE_CBC = 1,
	XP_CIPHER_MODE_CFB,
	XP_CIPHER_MODE_GCM,
	XP_CIPHER_MODE_STREAM,
};

enum xp_cipher_direction {
	XP_CIPHER_ENCRYPT = 1,
	XP_CIPHER_DECRYPT,
};

enum xp_cipher_padding {
	XP_CIPHER_PADDING_NONE = 0,
	XP_CIPHER_PADDING_PKCS7,
};

struct xp_cipher_config {
	enum xp_cipher_algorithm algorithm;
	enum xp_cipher_mode mode;
	enum xp_cipher_direction direction;
	enum xp_cipher_padding padding;
	const void *key;
	size_t key_len;
	const void *iv;
	size_t iv_len;
	size_t tag_len; /* GCM only; 12 through 16, default 16. */
};

DLLEXPORT bool xp_cipher_supported(
	enum xp_cipher_algorithm algorithm, enum xp_cipher_mode mode);
DLLEXPORT int xp_cipher_create(
	xp_cipher_t *out, const struct xp_cipher_config *config);
DLLEXPORT int xp_cipher_aad(xp_cipher_t context, const void *data, size_t len);

/*
 * Input may be supplied in arbitrary fragments.  This authenticated-safe
 * interface buffers until final(), so update always reports *out_len == 0.
 */
DLLEXPORT int xp_cipher_update(
	xp_cipher_t context, const void *input, size_t input_len,
	void *out, size_t *out_len);
DLLEXPORT int xp_cipher_set_tag(xp_cipher_t context, const void *tag, size_t len);
DLLEXPORT int xp_cipher_final(xp_cipher_t context, void *out, size_t *out_len);
DLLEXPORT int xp_cipher_get_tag(xp_cipher_t context, void *tag, size_t *len);
DLLEXPORT const char *xp_cipher_errstr(xp_cipher_t context);
DLLEXPORT void xp_cipher_free(xp_cipher_t context);

#if defined(__cplusplus)
}
#endif

#endif
