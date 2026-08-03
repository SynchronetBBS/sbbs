#ifndef _XP_DIGEST_H
#define _XP_DIGEST_H

#include <stddef.h>
#include "wrapdll.h"

#if defined(__cplusplus)
extern "C" {
#endif

enum xp_digest_algorithm {
	XP_DIGEST_SHA256 = 1,
	XP_DIGEST_SHA384,
	XP_DIGEST_SHA512,
};

typedef struct xp_digest_context *xp_digest_t;

/* A final() call with an output buffer consumes the state; free it afterward. */

DLLEXPORT size_t xp_digest_size(enum xp_digest_algorithm algorithm);
DLLEXPORT xp_digest_t xp_digest_create(enum xp_digest_algorithm algorithm);
DLLEXPORT int xp_digest_update(xp_digest_t context, const void *data, size_t len);
DLLEXPORT int xp_digest_final(xp_digest_t context, void *out, size_t *len);
DLLEXPORT void xp_digest_free(xp_digest_t context);
DLLEXPORT int xp_digest(
	enum xp_digest_algorithm algorithm, const void *data, size_t len,
	void *out, size_t *out_len);

#if defined(__cplusplus)
}
#endif
#endif
