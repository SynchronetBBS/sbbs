/*
 * hybrid-pq-kex-ops.h -- Crypto operations interface for hybrid
 * post-quantum key exchange (sntrup761x25519, mlkem768x25519).
 *
 * Private header shared between backend-specific files and the
 * shared protocol implementation (hybrid-pq-kex.c).
 */

#ifndef DSSH_HYBRID_PQ_KEX_OPS_H
#define DSSH_HYBRID_PQ_KEX_OPS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "deucessh-portable.h"
#include "x25519-ops.h"

#ifdef __cplusplus
extern "C" {
#endif

struct dssh_kex_context;
struct dssh_kex_result;

DSSH_PRIVATE int sntrup761x25519_init(bool client, void *config, void **state);
DSSH_PRIVATE int mlkem768x25519_init(bool client, void *config, void **state);
DSSH_PRIVATE void hybrid_pq_cleanup(void *state);

/*
 * Size parameters for a hybrid PQ KEX method.
 * These differ between sntrup761 and mlkem768.
 */
struct hybrid_pq_params {
	const char *kex_name;
	size_t      kex_name_len;
	const char *hash_name; /* "SHA-256" or "SHA-512" */
	size_t      digest_len;
	size_t      pq_pk_len; /* PQ public key size */
	size_t      pq_ct_len; /* PQ ciphertext size */
	size_t      pq_ss_len; /* PQ shared secret size */
	size_t      pq_sk_len; /* raw secret-key storage; zero for native opaque keys */
	size_t      q_c_len;   /* pq_pk_len + DSSH_X25519_LEN */
	size_t      q_s_len;   /* pq_ct_len + DSSH_X25519_LEN */
};

/*
 * Backend-specific crypto operations for hybrid PQ KEX.
 *
 * x25519: selected immutable private generation/derive/lifecycle operations.
 *
 * pq_keygen:   Generate PQ keypair.  Write public key to pk_out (pq_pk_len
 *              bytes), store cleanup-valid opaque secret key in *sk_ctx.
 *              Raw key implementations use the supplied secret storage/capacity;
 *              native opaque keys use NULL/zero and create fresh provider keys.
 * pq_encaps:   Encapsulate: ct_out (pq_ct_len), ss_out (pq_ss_len).
 * pq_decaps:   Decapsulate: ss_out (pq_ss_len) from ct + sk_ctx.
 * pq_free_sk:  Release a native opaque key. For borrowed raw storage this is a
 *              no-op; the shared handler clears its actual workspace capacity.
 *              Must accept NULL, including partial/error publication.
 */
struct hybrid_pq_ops {
	const struct dssh_x25519_ops *x25519;

	/* Post-quantum KEM operations */
	int  (*pq_keygen)(uint8_t *pk_out, uint8_t *secret_storage, size_t capacity, void **sk_ctx);
	int  (*pq_encaps)(uint8_t *ct_out, uint8_t *ss_out, const uint8_t *pk, size_t pk_len);
	int  (*pq_decaps)(uint8_t *ss_out, const uint8_t *ct, size_t ct_len, void *sk_ctx);
	void (*pq_free_sk)(void *sk_ctx);
};

DSSH_PRIVATE int hybrid_pq_init_impl(bool client, void *config, void **state,
    const struct hybrid_pq_params *params, const struct hybrid_pq_ops *ops);

/*
 * Shared protocol handler.  Called by backend-specific wrappers.
 */
DSSH_PRIVATE int hybrid_pq_handler_impl(const struct dssh_kex_context *kctx, struct dssh_kex_result *out, const struct hybrid_pq_params *params,
    const struct hybrid_pq_ops *ops);

#ifdef __cplusplus
}
#endif

#endif /* DSSH_HYBRID_PQ_KEX_OPS_H */
