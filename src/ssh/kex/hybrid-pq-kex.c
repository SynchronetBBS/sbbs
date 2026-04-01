// Hybrid post-quantum KEX -- Shared Protocol Logic
//
// Backend-neutral protocol for sntrup761x25519-sha512 and
// mlkem768x25519-sha256.  Parameterized by sizes and hash algorithm;
// crypto operations provided by the backend-specific ops struct.

#include <stdlib.h>
#include <string.h>

#include "deucessh-crypto.h"
#include "deucessh-kex.h"
#include "deucessh.h"
#include "hybrid-pq-kex-ops.h"
#ifdef DSSH_TESTING
 #include "ssh-internal.h"
#endif

/*
 * Compute exchange hash:
 * H = HASH(V_C || V_S || I_C || I_S || K_S || Q_C || Q_S || K)
 *
 * All string values are encoded with uint32 length prefix.
 * K is encoded as string (not mpint) for hybrid PQ methods.
 */
static int
compute_exchange_hash(const struct hybrid_pq_params *p, const char *v_c, size_t v_c_len, const char *v_s,
    size_t v_s_len, const uint8_t *i_c, size_t i_c_len, const uint8_t *i_s, size_t i_s_len, const uint8_t *k_s,
    size_t k_s_len, const uint8_t *q_c, size_t q_c_len, const uint8_t *q_s, size_t q_s_len, const uint8_t *k,
    size_t k_len, uint8_t *hash_out)
{
	if (v_c_len > UINT32_MAX || v_s_len > UINT32_MAX || i_c_len > UINT32_MAX || i_s_len > UINT32_MAX
	    || k_s_len > UINT32_MAX || q_c_len > UINT32_MAX || q_s_len > UINT32_MAX || k_len > UINT32_MAX)
		return DSSH_ERROR_INVALID;

	dssh_hash_ctx *hctx = NULL;
	size_t         digest_len;
	int            ret;

	ret = dssh_hash_init(&hctx, p->hash_name, &digest_len);
	if (ret < 0)
		return ret;

	uint8_t lenbuf[4];
	size_t  lp;
	int     ok = 1;

#define HASH_U32(val, data, len)                                                  \
	do {                                                                      \
		lp = 0;                                                           \
		if (dssh_serialize_uint32((val), lenbuf, 4, &lp) < 0) {           \
			ok = 0;                                                   \
			break;                                                    \
		}                                                                 \
		if (dssh_hash_update(hctx, lenbuf, 4) < 0) {                      \
			ok = 0;                                                   \
			break;                                                    \
		}                                                                 \
		if (dssh_hash_update(hctx, (const uint8_t *)(data), (len)) < 0) { \
			ok = 0;                                                   \
			break;                                                    \
		}                                                                 \
	} while (0)

	HASH_U32((uint32_t)v_c_len, v_c, v_c_len);
	HASH_U32((uint32_t)v_s_len, v_s, v_s_len);
	HASH_U32((uint32_t)i_c_len, i_c, i_c_len);
	HASH_U32((uint32_t)i_s_len, i_s, i_s_len);
	HASH_U32((uint32_t)k_s_len, k_s, k_s_len);
	HASH_U32((uint32_t)q_c_len, q_c, q_c_len);
	HASH_U32((uint32_t)q_s_len, q_s, q_s_len);
	HASH_U32((uint32_t)k_len, k, k_len);

#undef HASH_U32

	if (ok)
		ok = (dssh_hash_final(hctx, hash_out, p->digest_len) == 0);

	dssh_hash_free(hctx);
	return ok ? 0 : DSSH_ERROR_INIT;
}

/*
 * Compute shared secret K = HASH(ss_pq || ss_cl).
 * Returns malloc'd buffer in *k_out, caller frees.
 */
static int
compute_shared_secret(const struct hybrid_pq_params *p, const uint8_t *ss_pq, const uint8_t *ss_cl,
    uint8_t **k_out, size_t *k_len)
{
	if (p->pq_ss_len > SIZE_MAX - HYBRID_PQ_X25519_KEY_LEN)
		return DSSH_ERROR_INVALID;
	size_t   combined_len = p->pq_ss_len + HYBRID_PQ_X25519_KEY_LEN;
	uint8_t *combined     = malloc(combined_len);

	if (combined == NULL)
		return DSSH_ERROR_ALLOC;

	memcpy(combined, ss_pq, p->pq_ss_len);
	memcpy(combined + p->pq_ss_len, ss_cl, HYBRID_PQ_X25519_KEY_LEN);

	dssh_hash_ctx *hctx = NULL;
	size_t         digest_len;
	int            ret;

	ret = dssh_hash_init(&hctx, p->hash_name, &digest_len);
	if (ret < 0) {
		dssh_cleanse(combined, combined_len);
		free(combined);
		return ret;
	}

	uint8_t *k = malloc(p->digest_len);

	if (k == NULL) {
		dssh_cleanse(combined, combined_len);
		free(combined);
		dssh_hash_free(hctx);
		return DSSH_ERROR_ALLOC;
	}

	ret = dssh_hash_update(hctx, combined, combined_len);
	dssh_cleanse(combined, combined_len);
	free(combined);
	if (ret < 0) {
		dssh_hash_free(hctx);
		free(k);
		return DSSH_ERROR_INIT;
	}
	ret = dssh_hash_final(hctx, k, p->digest_len);
	dssh_hash_free(hctx);
	if (ret < 0) {
		free(k);
		return DSSH_ERROR_INIT;
	}
	*k_out = k;
	*k_len = p->digest_len;
	return 0;
}

static int
hybrid_pq_client(struct dssh_kex_context *kctx, const struct hybrid_pq_params *p, const struct hybrid_pq_ops *ops)
{
	dssh_key_algo ka = kctx->key_algo;
	int           res;
	uint8_t      *pq_pk       = NULL;
	void         *pq_sk_ctx   = NULL;
	void         *x25519_priv = NULL;
	uint8_t      *ss_pq       = NULL;
	uint8_t      *K           = NULL;
	uint8_t      *q_c_raw     = NULL;
	uint8_t      *hash        = NULL;
	size_t        K_len       = 0;
	uint8_t       x25519_pk[HYBRID_PQ_X25519_KEY_LEN];
	uint8_t       x25519_ss[HYBRID_PQ_X25519_KEY_LEN];

	memset(x25519_ss, 0, sizeof(x25519_ss));

	/* 1. Generate PQ keypair */
	pq_pk = malloc(p->pq_pk_len);
	if (pq_pk == NULL)
		return DSSH_ERROR_ALLOC;

	res = ops->pq_keygen(pq_pk, &pq_sk_ctx);
	if (res < 0)
		goto cleanup;

	/* 2. Generate X25519 ephemeral keypair */
	res = ops->x25519_keygen(x25519_pk, &x25519_priv);
	if (res < 0)
		goto cleanup;

	/* 3. Send INIT: Q_C = pq_pk || x25519_pk */
	{
		size_t   init_sz  = 1 + 4 + p->q_c_len;
		uint8_t *init_msg = malloc(init_sz);

		if (init_msg == NULL) {
			res = DSSH_ERROR_ALLOC;
			goto cleanup;
		}

		size_t   pos         = 0;
		uint32_t q_c_len_u32 = (uint32_t)p->q_c_len;

		init_msg[pos++] = SSH_MSG_KEX_ECDH_INIT;
		res             = dssh_serialize_uint32(q_c_len_u32, init_msg, init_sz, &pos);
		if (res < 0) {
			free(init_msg);
			goto cleanup;
		}
		memcpy(&init_msg[pos], pq_pk, p->pq_pk_len);
		pos += p->pq_pk_len;
		memcpy(&init_msg[pos], x25519_pk, HYBRID_PQ_X25519_KEY_LEN);
		pos += HYBRID_PQ_X25519_KEY_LEN;

		res = kctx->send(init_msg, pos, kctx->io_ctx);
		free(init_msg);
		if (res < 0)
			goto cleanup;
	}

	/* 4. Receive REPLY */
	uint8_t  msg_type;
	uint8_t *reply;
	size_t   reply_len;

	res = kctx->recv(&msg_type, &reply, &reply_len, kctx->io_ctx);
	if (res < 0)
		goto cleanup;
	if (msg_type != SSH_MSG_KEX_ECDH_REPLY) {
		res = DSSH_ERROR_PARSE;
		goto cleanup;
	}

	/* Parse K_S */
	size_t   rpos = 1;
	uint32_t ks_len;

	if (dssh_parse_uint32(&reply[rpos], reply_len - rpos, &ks_len) < 4) {
		res = DSSH_ERROR_PARSE;
		goto cleanup;
	}
	rpos += 4;
	if (rpos + ks_len > reply_len) {
		res = DSSH_ERROR_PARSE;
		goto cleanup;
	}
	const uint8_t *k_s     = &reply[rpos];
	size_t         k_s_len = ks_len;

	rpos += ks_len;

	/* Parse Q_S */
	uint32_t qs_len;

	if (dssh_parse_uint32(&reply[rpos], reply_len - rpos, &qs_len) < 4) {
		res = DSSH_ERROR_PARSE;
		goto cleanup;
	}
	if (qs_len != (uint32_t)p->q_s_len) {
		res = DSSH_ERROR_PARSE;
		goto cleanup;
	}
	rpos += 4;
	if (rpos + qs_len > reply_len) {
		res = DSSH_ERROR_PARSE;
		goto cleanup;
	}
	const uint8_t *pq_ct          = &reply[rpos];
	const uint8_t *peer_x25519_pk = &reply[rpos + p->pq_ct_len];

	rpos += qs_len;

	/* Parse signature */
	uint32_t sig_len;

	if (dssh_parse_uint32(&reply[rpos], reply_len - rpos, &sig_len) < 4) {
		res = DSSH_ERROR_PARSE;
		goto cleanup;
	}
	rpos += 4;
	if (rpos + sig_len > reply_len) {
		res = DSSH_ERROR_PARSE;
		goto cleanup;
	}
	const uint8_t *sig_h = &reply[rpos];

	/* 5. PQ decapsulate */
	ss_pq = malloc(p->pq_ss_len);
	if (ss_pq == NULL) {
		res = DSSH_ERROR_ALLOC;
		goto cleanup;
	}
	res = ops->pq_decaps(ss_pq, pq_ct, p->pq_ct_len, pq_sk_ctx);
	ops->pq_free_sk(pq_sk_ctx);
	pq_sk_ctx = NULL;
	if (res < 0)
		goto cleanup;

	/* 6. X25519 derive */
	res = ops->x25519_derive(x25519_priv, peer_x25519_pk, x25519_ss);
	ops->x25519_free_priv(x25519_priv);
	x25519_priv = NULL;
	if (res < 0)
		goto cleanup;

	/* 7. K = HASH(ss_pq || ss_cl) */
	res = compute_shared_secret(p, ss_pq, x25519_ss, &K, &K_len);
	dssh_cleanse(ss_pq, p->pq_ss_len);
	free(ss_pq);
	ss_pq = NULL;
	dssh_cleanse(x25519_ss, sizeof(x25519_ss));
	if (res < 0)
		goto cleanup;

	/* 8. Exchange hash -- build Q_C = pq_pk || x25519_pk */
	q_c_raw = malloc(p->q_c_len);
	if (q_c_raw == NULL) {
		res = DSSH_ERROR_ALLOC;
		goto cleanup;
	}
	memcpy(q_c_raw, pq_pk, p->pq_pk_len);
	memcpy(q_c_raw + p->pq_pk_len, x25519_pk, HYBRID_PQ_X25519_KEY_LEN);
	free(pq_pk);
	pq_pk = NULL;

	hash = malloc(p->digest_len);
	if (hash == NULL) {
		res = DSSH_ERROR_ALLOC;
		goto cleanup;
	}

	res =
	    compute_exchange_hash(p, kctx->v_c, kctx->v_c_len, kctx->v_s, kctx->v_s_len, kctx->i_c, kctx->i_c_len,
		kctx->i_s, kctx->i_s_len, k_s, k_s_len, q_c_raw, p->q_c_len, pq_ct, p->q_s_len, K, K_len, hash);
	free(q_c_raw);
	q_c_raw = NULL;
	if (res < 0)
		goto cleanup;

	/* 9. Verify signature */
	if (!ka || !ka->verify) {
		res = DSSH_ERROR_INIT;
		goto cleanup;
	}
	res = ka->verify(k_s, k_s_len, sig_h, sig_len, hash, p->digest_len);
	if (res < 0)
		goto cleanup;

	/* 10. Store results */
	kctx->shared_secret    = K;
	kctx->shared_secret_sz = K_len;
	kctx->exchange_hash    = hash;
	kctx->exchange_hash_sz = p->digest_len;
	K                      = NULL;
	hash                   = NULL;
	res                    = 0;

cleanup:
	free(pq_pk);
	free(q_c_raw);
	if (ss_pq) {
		dssh_cleanse(ss_pq, p->pq_ss_len);
		free(ss_pq);
	}
	dssh_cleanse(x25519_ss, sizeof(x25519_ss));
	if (K) {
		dssh_cleanse(K, K_len);
		free(K);
	}
	free(hash);
	if (pq_sk_ctx)
		ops->pq_free_sk(pq_sk_ctx);
	if (x25519_priv)
		ops->x25519_free_priv(x25519_priv);
	return res;
}

static int
hybrid_pq_server(struct dssh_kex_context *kctx, const struct hybrid_pq_params *p, const struct hybrid_pq_ops *ops)
{
	dssh_key_algo ka = kctx->key_algo;
	int           res;
	uint8_t      *pq_ct   = NULL;
	uint8_t      *ss_pq   = NULL;
	uint8_t      *K       = NULL;
	uint8_t      *q_s_raw = NULL;
	uint8_t      *hash    = NULL;
	uint8_t      *sig_buf = NULL;
	size_t        K_len   = 0;
	size_t        sig_len = 0;
	uint8_t       x25519_ss[HYBRID_PQ_X25519_KEY_LEN];

	memset(x25519_ss, 0, sizeof(x25519_ss));

	/* 1. Get host key blob */
	if (!ka || !ka->pubkey || !ka->sign)
		return DSSH_ERROR_INIT;

	const uint8_t *k_s_buf = NULL;
	size_t         k_s_len;

	res = ka->pubkey(&k_s_buf, &k_s_len, ka->ctx);
	if (res < 0)
		return res;

	/* 2. Receive INIT */
	uint8_t  msg_type;
	uint8_t *payload;
	size_t   payload_len;

	res = kctx->recv(&msg_type, &payload, &payload_len, kctx->io_ctx);
	if (res < 0)
		return res;
	if (msg_type != SSH_MSG_KEX_ECDH_INIT)
		return DSSH_ERROR_PARSE;

	/* 3. Parse Q_C */
	size_t   rpos = 1;
	uint32_t qc_len;

	if (dssh_parse_uint32(&payload[rpos], payload_len - rpos, &qc_len) < 4)
		return DSSH_ERROR_PARSE;
	if (qc_len != (uint32_t)p->q_c_len)
		return DSSH_ERROR_PARSE;
	rpos += 4;
	if (rpos + qc_len > payload_len)
		return DSSH_ERROR_PARSE;

	const uint8_t *client_pq_pk     = &payload[rpos];
	const uint8_t *client_x25519_pk = &payload[rpos + p->pq_pk_len];
	const uint8_t *q_c_raw          = &payload[rpos];

	/* 4. PQ encapsulate */
	pq_ct = malloc(p->pq_ct_len);
	ss_pq = malloc(p->pq_ss_len);

	if (pq_ct == NULL || ss_pq == NULL) {
		res = DSSH_ERROR_ALLOC;
		goto cleanup;
	}

	res = ops->pq_encaps(pq_ct, ss_pq, client_pq_pk, p->pq_pk_len);
	if (res < 0)
		goto cleanup;

	/* 5. X25519 exchange */
	uint8_t x25519_pk[HYBRID_PQ_X25519_KEY_LEN];

	res = ops->x25519_exchange(client_x25519_pk, HYBRID_PQ_X25519_KEY_LEN, x25519_pk, x25519_ss);
	if (res < 0)
		goto cleanup;

	/* 6. K = HASH(ss_pq || ss_cl) */
	res = compute_shared_secret(p, ss_pq, x25519_ss, &K, &K_len);
	dssh_cleanse(ss_pq, p->pq_ss_len);
	free(ss_pq);
	ss_pq = NULL;
	dssh_cleanse(x25519_ss, sizeof(x25519_ss));
	if (res < 0)
		goto cleanup;

	/* 7. Build Q_S = pq_ct || x25519_pk */
	q_s_raw = malloc(p->q_s_len);
	if (q_s_raw == NULL) {
		res = DSSH_ERROR_ALLOC;
		goto cleanup;
	}
	memcpy(q_s_raw, pq_ct, p->pq_ct_len);
	memcpy(q_s_raw + p->pq_ct_len, x25519_pk, HYBRID_PQ_X25519_KEY_LEN);
	free(pq_ct);
	pq_ct = NULL;

	/* 8. Exchange hash */
	hash = malloc(p->digest_len);
	if (hash == NULL) {
		res = DSSH_ERROR_ALLOC;
		goto cleanup;
	}

	res = compute_exchange_hash(p, kctx->v_c, kctx->v_c_len, kctx->v_s, kctx->v_s_len, kctx->i_c,
	    kctx->i_c_len, kctx->i_s, kctx->i_s_len, k_s_buf, k_s_len, q_c_raw, p->q_c_len, q_s_raw, p->q_s_len, K,
	    K_len, hash);
	if (res < 0)
		goto cleanup;

	/* 9. Sign exchange hash */
	res = ka->sign(&sig_buf, &sig_len, hash, p->digest_len, ka->ctx);
	if (res < 0)
		goto cleanup;

	/* 10. Send REPLY */
	{
		size_t   reply_sz  = 1 + 4 + k_s_len + 4 + p->q_s_len + 4 + sig_len;
		uint8_t *reply_msg = malloc(reply_sz);

		if (reply_msg == NULL) {
			res = DSSH_ERROR_ALLOC;
			goto cleanup;
		}

		size_t rp = 0;

		reply_msg[rp++] = SSH_MSG_KEX_ECDH_REPLY;

#define REPLY_SER(v)                                                        \
	do {                                                                \
		res = dssh_serialize_uint32((v), reply_msg, reply_sz, &rp); \
		if (res < 0) {                                              \
			free(reply_msg);                                    \
			goto cleanup;                                       \
		}                                                           \
	} while (0)

		REPLY_SER((uint32_t)k_s_len);
		memcpy(&reply_msg[rp], k_s_buf, k_s_len);
		rp += k_s_len;
		REPLY_SER((uint32_t)p->q_s_len);
		memcpy(&reply_msg[rp], q_s_raw, p->q_s_len);
		rp += p->q_s_len;
		REPLY_SER((uint32_t)sig_len);
		memcpy(&reply_msg[rp], sig_buf, sig_len);
		rp += sig_len;
		free(sig_buf);
		sig_buf = NULL;
		free(q_s_raw);
		q_s_raw = NULL;

#undef REPLY_SER

		res = kctx->send(reply_msg, rp, kctx->io_ctx);
		free(reply_msg);
		if (res < 0)
			goto cleanup;
	}

	/* Store results */
	kctx->shared_secret    = K;
	kctx->shared_secret_sz = K_len;
	kctx->exchange_hash    = hash;
	kctx->exchange_hash_sz = p->digest_len;
	K                      = NULL;
	hash                   = NULL;
	res                    = 0;

cleanup:
	free(pq_ct);
	if (ss_pq) {
		dssh_cleanse(ss_pq, p->pq_ss_len);
		free(ss_pq);
	}
	dssh_cleanse(x25519_ss, sizeof(x25519_ss));
	free(q_s_raw);
	free(sig_buf);
	if (K) {
		dssh_cleanse(K, K_len);
		free(K);
	}
	free(hash);
	return res;
}

DSSH_PRIVATE int
hybrid_pq_handler_impl(struct dssh_kex_context *kctx, const struct hybrid_pq_params *p,
    const struct hybrid_pq_ops *ops)
{
	if (kctx->client)
		return hybrid_pq_client(kctx, p, ops);
	return hybrid_pq_server(kctx, p, ops);
}
