// RFC 8731: curve25519-sha256 Key Exchange -- Shared Protocol Logic
//
// Backend-neutral protocol implementation.  Crypto operations are
// provided by the backend-specific ops struct (OpenSSL or Botan).

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "curve25519-sha256-ops.h"
#include "deucessh-crypto.h"
#include "deucessh-kex.h"
#include "deucessh.h"
#ifdef DSSH_TESTING
 #include "ssh-internal.h"
#endif

/*
 * Compute the exchange hash:
 * H = SHA256(V_C || V_S || I_C || I_S || K_S || Q_C || Q_S || K_mpint)
 *
 * All string values are encoded with uint32 length prefix.
 */
DSSH_TESTABLE int
compute_exchange_hash_c25519(const char *v_c, size_t v_c_len, const char *v_s, size_t v_s_len, const uint8_t *i_c,
    size_t i_c_len, const uint8_t *i_s, size_t i_s_len, const uint8_t *k_s, size_t k_s_len, const uint8_t *q_c,
    size_t q_c_len, const uint8_t *q_s, size_t q_s_len, const uint8_t *k_mpint, size_t k_mpint_len,
    uint8_t *hash_out)
{
	if (v_c_len > UINT32_MAX || v_s_len > UINT32_MAX || i_c_len > UINT32_MAX || i_s_len > UINT32_MAX
	    || k_s_len > UINT32_MAX || q_c_len > UINT32_MAX || q_s_len > UINT32_MAX)
		return DSSH_ERROR_INVALID;

	dssh_hash_ctx *hctx = NULL;
	size_t         digest_len;
	int            ret;

	ret = dssh_hash_init(&hctx, "SHA-256", &digest_len);
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

#undef HASH_U32

	if (ok)
		ok = (dssh_hash_update(hctx, k_mpint, k_mpint_len) == 0);

	if (ok)
		ok = (dssh_hash_final(hctx, hash_out, SHA256_DIGEST_LEN) == 0);

	dssh_hash_free(hctx);
	return ok ? 0 : DSSH_ERROR_INIT;
}

/*
 * Encode raw shared secret bytes as SSH mpint, stripping leading zeros.
 * Caller must free *mpint_out.
 */
DSSH_TESTABLE int
encode_shared_secret(uint8_t *raw, size_t raw_len, uint8_t **ss_out, size_t *ss_len, uint8_t **mpint_out,
    size_t *mpint_len)
{
	uint8_t *start = raw;

	while (raw_len > 1 && start[0] == 0) {
		start++;
		raw_len--;
	}
	*ss_out = malloc(raw_len);
	if (*ss_out == NULL)
		return DSSH_ERROR_ALLOC;
	memcpy(*ss_out, start, raw_len);
	*ss_len = raw_len;

	bool   need_pad       = (start[0] & DSSH_MPINT_SIGN_BIT) != 0;
	size_t mpint_data_len = raw_len + (need_pad ? 1 : 0);

	*mpint_len = 4 + mpint_data_len;
	*mpint_out = malloc(*mpint_len);
	if (*mpint_out == NULL) {
		free(*ss_out);
		*ss_out = NULL;
		return DSSH_ERROR_ALLOC;
	}

	size_t kp = 0;

	int ret = dssh_serialize_uint32((uint32_t)mpint_data_len, *mpint_out, *mpint_len, &kp);
	if (ret < 0) {
		free(*mpint_out);
		*mpint_out = NULL;
		free(*ss_out);
		*ss_out = NULL;
		return ret;
	}
	if (need_pad)
		(*mpint_out)[kp++] = 0;
	memcpy(&(*mpint_out)[kp], start, raw_len);
	return 0;
}

DSSH_PRIVATE int
curve25519_handler_impl(struct dssh_kex_context *kctx, const struct dssh_c25519_ops *ops)
{
	int           res;
	dssh_key_algo ka = kctx->key_algo;

	uint8_t  q_c[X25519_KEY_LEN], q_s[X25519_KEY_LEN];
	uint8_t *raw_secret = NULL;
	size_t   raw_secret_len;
	uint8_t *ss_copy = NULL;
	size_t   ss_len;
	uint8_t *k_mpint = NULL;
	size_t   k_mpint_len;

	/* Get host key blob (server signs, client verifies) */
	const uint8_t *k_s_buf = NULL;
	size_t         k_s_len = 0;

	if (kctx->client) {
		/* Client: send ECDH_INIT, receive ECDH_REPLY */
		void *priv_ctx = NULL;

		/* Generate keypair and send Q_C */
		res = ops->keygen(q_c, &priv_ctx);
		if (res < 0)
			return res;

		uint8_t init_msg[1 + 4 + X25519_KEY_LEN];
		size_t  pos = 0;

		init_msg[pos++] = SSH_MSG_KEX_ECDH_INIT;
		res             = dssh_serialize_uint32(X25519_KEY_LEN, init_msg, sizeof(init_msg), &pos);
		if (res < 0) {
			ops->free_priv(priv_ctx);
			return res;
		}
		memcpy(&init_msg[pos], q_c, X25519_KEY_LEN);
		pos += X25519_KEY_LEN;
		res = kctx->send(init_msg, pos, kctx->io_ctx);
		if (res < 0) {
			ops->free_priv(priv_ctx);
			return res;
		}

		/* Receive ECDH_REPLY */
		uint8_t  msg_type;
		uint8_t *reply;
		size_t   reply_len;

		res = kctx->recv(&msg_type, &reply, &reply_len, kctx->io_ctx);
		if (res < 0) {
			ops->free_priv(priv_ctx);
			return res;
		}
		if (msg_type != SSH_MSG_KEX_ECDH_REPLY) {
			ops->free_priv(priv_ctx);
			return DSSH_ERROR_PARSE;
		}

		/* Parse K_S, Q_S, sig */
		size_t   rpos = 1;
		uint32_t ks_len;

		if ((dssh_parse_uint32(&reply[rpos], reply_len - rpos, &ks_len) < 4)
		    || (rpos + 4 + ks_len > reply_len)) {
			ops->free_priv(priv_ctx);
			return DSSH_ERROR_PARSE;
		}
		rpos += 4;

		uint8_t *k_s = &reply[rpos];

		k_s_len = ks_len;
		rpos += ks_len;

		uint32_t qs_len;

		if (rpos + 4 > reply_len) {
			ops->free_priv(priv_ctx);
			return DSSH_ERROR_PARSE;
		}
		if (dssh_parse_uint32(&reply[rpos], reply_len - rpos, &qs_len) < 4) {
			ops->free_priv(priv_ctx);
			return DSSH_ERROR_PARSE;
		}
		if ((qs_len != X25519_KEY_LEN) || (rpos + 4 + qs_len > reply_len)) {
			ops->free_priv(priv_ctx);
			return DSSH_ERROR_PARSE;
		}
		rpos += 4;
		memcpy(q_s, &reply[rpos], X25519_KEY_LEN);
		rpos += qs_len;

		uint32_t sig_len;

		if (rpos + 4 > reply_len) {
			ops->free_priv(priv_ctx);
			return DSSH_ERROR_PARSE;
		}
		if (dssh_parse_uint32(&reply[rpos], reply_len - rpos, &sig_len) < 4) {
			ops->free_priv(priv_ctx);
			return DSSH_ERROR_PARSE;
		}
		if (rpos + 4 + sig_len > reply_len) {
			ops->free_priv(priv_ctx);
			return DSSH_ERROR_PARSE;
		}
		rpos += 4;

		uint8_t *sig_h = &reply[rpos];

		/* Compute shared secret using our private key + Q_S */
		res = ops->derive(priv_ctx, q_s, X25519_KEY_LEN, &raw_secret, &raw_secret_len);
		ops->free_priv(priv_ctx);
		if (res < 0)
			return res;

		res = encode_shared_secret(raw_secret, raw_secret_len, &ss_copy, &ss_len, &k_mpint, &k_mpint_len);
		dssh_cleanse(raw_secret, raw_secret_len);
		free(raw_secret);
		if (res < 0)
			return res;

		/* Compute exchange hash */
		uint8_t hash[SHA256_DIGEST_LEN];

		res = compute_exchange_hash_c25519(kctx->v_c, kctx->v_c_len, kctx->v_s, kctx->v_s_len, kctx->i_c,
		    kctx->i_c_len, kctx->i_s, kctx->i_s_len, k_s, k_s_len, q_c, X25519_KEY_LEN, q_s,
		    X25519_KEY_LEN, k_mpint, k_mpint_len, hash);
		dssh_cleanse(k_mpint, k_mpint_len);
		free(k_mpint);
		if (res < 0) {
			dssh_cleanse(ss_copy, ss_len);
			free(ss_copy);
			return res;
		}

		/* Verify key algorithm and host key */
		if ((ka == NULL) || (ka->verify == NULL)) {
			dssh_cleanse(ss_copy, ss_len);
			free(ss_copy);
			return DSSH_ERROR_INIT;
		}

		if (kctx->hostkey_verify) {
			uint8_t fp[32];

			if (dssh_hash_oneshot("SHA-256", k_s, k_s_len, fp, sizeof(fp)) < 0) {
				dssh_cleanse(ss_copy, ss_len);
				free(ss_copy);
				return DSSH_ERROR_INIT;
			}
			unsigned int bits = ka->key_bits ? ka->key_bits(k_s, k_s_len) : 0;

			if (kctx->hostkey_verify(ka->name, bits, fp, k_s, k_s_len,
			        kctx->hostkey_verify_cbdata) != DSSH_HOSTKEY_ACCEPT) {
				dssh_cleanse(ss_copy, ss_len);
				free(ss_copy);
				return DSSH_ERROR_INVALID;
			}
		}

		/* Verify server signature */
		res = ka->verify(k_s, k_s_len, sig_h, sig_len, hash, SHA256_DIGEST_LEN);
		if (res < 0) {
			dssh_cleanse(ss_copy, ss_len);
			free(ss_copy);
			return res;
		}

		/* Store results */
		kctx->shared_secret    = ss_copy;
		kctx->shared_secret_sz = ss_len;
		kctx->exchange_hash    = malloc(SHA256_DIGEST_LEN);
		if (kctx->exchange_hash == NULL) {
			dssh_cleanse(ss_copy, ss_len);
			free(ss_copy);
			kctx->shared_secret    = NULL;
			kctx->shared_secret_sz = 0;
			return DSSH_ERROR_ALLOC;
		}
		memcpy(kctx->exchange_hash, hash, SHA256_DIGEST_LEN);
		kctx->exchange_hash_sz = SHA256_DIGEST_LEN;
	}
	else {
		if ((ka == NULL) || (ka->pubkey == NULL) || (ka->sign == NULL))
			return DSSH_ERROR_INIT;

		/* Get our host key blob */
		res = ka->pubkey(&k_s_buf, &k_s_len, ka->ctx);
		if (res < 0)
			return res;

		/* Receive ECDH_INIT(Q_C) */
		uint8_t  msg_type;
		uint8_t *init_payload;
		size_t   init_len;

		res = kctx->recv(&msg_type, &init_payload, &init_len, kctx->io_ctx);
		if (res < 0)
			return res;
		if (msg_type != SSH_MSG_KEX_ECDH_INIT)
			return DSSH_ERROR_PARSE;

		size_t   rpos = 1;
		uint32_t qc_len;

		if ((dssh_parse_uint32(&init_payload[rpos], init_len - rpos, &qc_len) < 4)
		    || (qc_len != X25519_KEY_LEN) || (rpos + 4 + qc_len > init_len))
			return DSSH_ERROR_PARSE;
		rpos += 4;
		memcpy(q_c, &init_payload[rpos], X25519_KEY_LEN);

		/* Generate our ephemeral keypair and compute shared secret */
		res = ops->exchange(q_c, X25519_KEY_LEN, q_s, &raw_secret, &raw_secret_len);
		if (res < 0)
			return res;

		res = encode_shared_secret(raw_secret, raw_secret_len, &ss_copy, &ss_len, &k_mpint, &k_mpint_len);
		dssh_cleanse(raw_secret, raw_secret_len);
		free(raw_secret);
		if (res < 0)
			return res;

		/* Compute exchange hash */
		uint8_t hash[SHA256_DIGEST_LEN];

		res = compute_exchange_hash_c25519(kctx->v_c, kctx->v_c_len, kctx->v_s, kctx->v_s_len, kctx->i_c,
		    kctx->i_c_len, kctx->i_s, kctx->i_s_len, k_s_buf, k_s_len, q_c, X25519_KEY_LEN, q_s,
		    X25519_KEY_LEN, k_mpint, k_mpint_len, hash);
		dssh_cleanse(k_mpint, k_mpint_len);
		free(k_mpint);
		if (res < 0) {
			dssh_cleanse(ss_copy, ss_len);
			free(ss_copy);
			return res;
		}

		/* Sign exchange hash */
		uint8_t *sig_buf = NULL;
		size_t   sig_len;

		res = ka->sign(&sig_buf, &sig_len, hash, SHA256_DIGEST_LEN, ka->ctx);
		if (res < 0) {
			dssh_cleanse(ss_copy, ss_len);
			free(ss_copy);
			return res;
		}

		/* Send ECDH_REPLY(K_S, Q_S, sig) */
		if (k_s_len > UINT32_MAX || sig_len > UINT32_MAX) {
			free(sig_buf);
			dssh_cleanse(ss_copy, ss_len);
			free(ss_copy);
			return DSSH_ERROR_INVALID;
		}
		/* 1 (msg) + 4 (k_s len) + k_s + 4 (Q_S len) + Q_S + 4 (sig len) + sig */
		size_t reply_sz = 1 + 4 + 4 + X25519_KEY_LEN + 4;
		if (k_s_len > SIZE_MAX - reply_sz) {
			free(sig_buf);
			dssh_cleanse(ss_copy, ss_len);
			free(ss_copy);
			return DSSH_ERROR_INVALID;
		}
		reply_sz += k_s_len;
		if (sig_len > SIZE_MAX - reply_sz) {
			free(sig_buf);
			dssh_cleanse(ss_copy, ss_len);
			free(ss_copy);
			return DSSH_ERROR_INVALID;
		}
		reply_sz += sig_len;

		uint8_t *reply_msg = malloc(reply_sz);
		if (reply_msg == NULL) {
			free(sig_buf);
			dssh_cleanse(ss_copy, ss_len);
			free(ss_copy);
			return DSSH_ERROR_ALLOC;
		}

		size_t   pos         = 0;
		uint32_t k_s_len_u32 = (uint32_t)k_s_len;
		uint32_t sig_len_u32 = (uint32_t)sig_len;

		reply_msg[pos++] = SSH_MSG_KEX_ECDH_REPLY;
		res              = dssh_serialize_uint32(k_s_len_u32, reply_msg, reply_sz, &pos);
		if (res < 0) {
			free(reply_msg);
			free(sig_buf);
			dssh_cleanse(ss_copy, ss_len);
			free(ss_copy);
			return res;
		}
		memcpy(&reply_msg[pos], k_s_buf, k_s_len);
		pos += k_s_len;
		res = dssh_serialize_uint32(X25519_KEY_LEN, reply_msg, reply_sz, &pos);
		if (res < 0) {
			free(reply_msg);
			free(sig_buf);
			dssh_cleanse(ss_copy, ss_len);
			free(ss_copy);
			return res;
		}
		memcpy(&reply_msg[pos], q_s, X25519_KEY_LEN);
		pos += X25519_KEY_LEN;
		res = dssh_serialize_uint32(sig_len_u32, reply_msg, reply_sz, &pos);
		if (res < 0) {
			free(reply_msg);
			free(sig_buf);
			dssh_cleanse(ss_copy, ss_len);
			free(ss_copy);
			return res;
		}
		memcpy(&reply_msg[pos], sig_buf, sig_len);
		pos += sig_len;
		free(sig_buf);
		res = kctx->send(reply_msg, pos, kctx->io_ctx);
		free(reply_msg);
		if (res < 0) {
			dssh_cleanse(ss_copy, ss_len);
			free(ss_copy);
			return res;
		}

		/* Store results */
		kctx->shared_secret    = ss_copy;
		kctx->shared_secret_sz = ss_len;
		kctx->exchange_hash    = malloc(SHA256_DIGEST_LEN);
		if (kctx->exchange_hash == NULL) {
			dssh_cleanse(ss_copy, ss_len);
			free(ss_copy);
			kctx->shared_secret    = NULL;
			kctx->shared_secret_sz = 0;
			return DSSH_ERROR_ALLOC;
		}
		memcpy(kctx->exchange_hash, hash, SHA256_DIGEST_LEN);
		kctx->exchange_hash_sz = SHA256_DIGEST_LEN;
	}

	return 0;
}
