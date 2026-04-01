/*
 * deucessh-key-algo.h -- Host key algorithm module interface.
 */

#ifndef DSSH_KEY_ALGO_H
#define DSSH_KEY_ALGO_H

#include <stddef.h>
#include <stdint.h>

#include "deucessh-portable.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct dssh_key_algo_ctx dssh_key_algo_ctx;

typedef int (*dssh_key_algo_sign)(uint8_t **out, size_t *outlen,
    const uint8_t *data, size_t data_len, dssh_key_algo_ctx *ctx);
typedef int (*dssh_key_algo_verify)(const uint8_t *key_blob, size_t key_blob_len,
    const uint8_t *sig_blob, size_t sig_blob_len,
    const uint8_t *data, size_t data_len);
typedef int (*dssh_key_algo_pubkey)(const uint8_t **out, size_t *outlen,
    dssh_key_algo_ctx *ctx);
typedef int (*dssh_key_algo_haskey)(dssh_key_algo_ctx *ctx);
typedef void (*dssh_key_algo_cleanup)(dssh_key_algo_ctx *ctx);

#define DSSH_KEY_ALGO_FLAG_ENCRYPTION_CAPABLE UINT32_C(1 << 0)
#define DSSH_KEY_ALGO_FLAG_SIGNATURE_CAPABLE  UINT32_C(1 << 1)

typedef struct dssh_key_algo_s {
	struct dssh_key_algo_s *next;  /* must be first -- generic traversal assumes offsetof(next) == 0 */
	dssh_key_algo_sign      sign;
	dssh_key_algo_verify    verify;
	dssh_key_algo_pubkey    pubkey;
	dssh_key_algo_haskey    haskey;
	dssh_key_algo_cleanup   cleanup;
	dssh_key_algo_ctx      *ctx;
	uint32_t                flags;
	char                    name[];
} *dssh_key_algo;
static_assert(!offsetof(struct dssh_key_algo_s, next),
    "next must be at offset 0 for generic list traversal");

DSSH_PUBLIC int dssh_transport_register_key_algo(dssh_key_algo key_algo);

#ifdef __cplusplus
}
#endif

#endif /* DSSH_KEY_ALGO_H */
