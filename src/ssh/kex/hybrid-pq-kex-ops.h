/*
 * hybrid-pq-kex-ops.h -- Crypto operations interface for hybrid
 * post-quantum key exchange (sntrup761x25519, mlkem768x25519).
 *
 * Private header shared between backend-specific files and the
 * shared protocol implementation (hybrid-pq-kex.c).
 */

#ifndef DSSH_HYBRID_PQ_KEX_OPS_H
#define DSSH_HYBRID_PQ_KEX_OPS_H

#include <stddef.h>
#include <stdint.h>

#include "deucessh-portable.h"

#ifdef __cplusplus
extern "C" {
#endif

#define HYBRID_PQ_X25519_KEY_LEN 32

struct dssh_kex_context;

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
	size_t      q_c_len;   /* pq_pk_len + X25519_KEY_LEN */
	size_t      q_s_len;   /* pq_ct_len + X25519_KEY_LEN */
};

/*
 * Backend-specific crypto operations for hybrid PQ KEX.
 *
 * x25519_keygen:    Generate X25519 keypair.  Write 32-byte public key
 *                   to pub_out, store opaque private key in *priv_ctx.
 * x25519_derive:    Derive 32-byte shared secret from priv_ctx + peer.
 *                   Does NOT free priv_ctx.
 * x25519_exchange:  Combined server-side keygen + derive.
 * x25519_free_priv: Free opaque X25519 private key.  Must be NULL-safe.
 *
 * pq_keygen:   Generate PQ keypair.  Write public key to pk_out (pq_pk_len
 *              bytes), store opaque secret key in *sk_ctx.
 * pq_encaps:   Encapsulate: ct_out (pq_ct_len), ss_out (pq_ss_len).
 * pq_decaps:   Decapsulate: ss_out (pq_ss_len) from ct + sk_ctx.
 * pq_free_sk:  Free opaque PQ secret key.  Must be NULL-safe.
 */
struct hybrid_pq_ops {
	/* X25519 operations */
	int  (*x25519_keygen)(uint8_t pub_out[HYBRID_PQ_X25519_KEY_LEN], void **priv_ctx);
	int  (*x25519_derive)(void *priv_ctx, const uint8_t *peer_pub, uint8_t ss_out[HYBRID_PQ_X25519_KEY_LEN]);
	int  (*x25519_exchange)(const uint8_t *peer_pub, size_t peer_pub_len, uint8_t *our_pub, uint8_t *ss_out);
	void (*x25519_free_priv)(void *priv_ctx);

	/* Post-quantum KEM operations */
	int  (*pq_keygen)(uint8_t *pk_out, void **sk_ctx);
	int  (*pq_encaps)(uint8_t *ct_out, uint8_t *ss_out, const uint8_t *pk, size_t pk_len);
	int  (*pq_decaps)(uint8_t *ss_out, const uint8_t *ct, size_t ct_len, void *sk_ctx);
	void (*pq_free_sk)(void *sk_ctx);
};

/*
 * Shared protocol handler.  Called by backend-specific wrappers.
 */
DSSH_PRIVATE int hybrid_pq_handler_impl(struct dssh_kex_context *kctx, const struct hybrid_pq_params *params,
    const struct hybrid_pq_ops *ops);

#ifdef __cplusplus
}
#endif

#endif /* DSSH_HYBRID_PQ_KEX_OPS_H */
