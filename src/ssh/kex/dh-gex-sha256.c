// RFC 4419: DH Group Exchange with SHA-256 -- Shared Protocol Logic
//
// Backend-neutral protocol implementation.  All bignum operations
// happen inside the backend ops; this file works exclusively with
// serialized SSH mpint byte arrays.

#include <stdlib.h>
#include <string.h>

#include "deucessh-crypto.h"
#include "deucessh-kex.h"
#include "deucessh.h"
#include "dh-gex-sha256-ops.h"
#include "kex/dh-gex-sha256.h"
#ifdef DSSH_TESTING
 #include "ssh-internal.h"
#endif

/*
 * Compute exchange hash:
 * H = SHA256(V_C || V_S || I_C || I_S || K_S ||
 *            min || n || max || p || g || e || f || K)
 *
 * String fields (V_C..K_S) are length-prefixed.  min/n/max are uint32.
 * p, g, e, f, K are pre-serialized SSH mpint byte arrays (already
 * include the 4-byte length prefix), written directly into the hash.
 */
static int
compute_exchange_hash(const char *v_c, size_t v_c_len, const char *v_s, size_t v_s_len, const uint8_t *i_c,
    size_t i_c_len, const uint8_t *i_s, size_t i_s_len, const uint8_t *k_s, size_t k_s_len, uint32_t gex_min,
    uint32_t gex_n, uint32_t gex_max, const uint8_t *p_mpint, size_t p_mpint_len, const uint8_t *g_mpint,
    size_t g_mpint_len, const uint8_t *e_mpint, size_t e_mpint_len, const uint8_t *f_mpint, size_t f_mpint_len,
    const uint8_t *k_mpint, size_t k_mpint_len, uint8_t *hash_out)
{
	if (v_c_len > UINT32_MAX || v_s_len > UINT32_MAX || i_c_len > UINT32_MAX || i_s_len > UINT32_MAX
	    || k_s_len > UINT32_MAX)
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
#define HASH_U32_ONLY(val)                                              \
	do {                                                            \
		lp = 0;                                                 \
		if (dssh_serialize_uint32((val), lenbuf, 4, &lp) < 0) { \
			ok = 0;                                         \
			break;                                          \
		}                                                       \
		if (dssh_hash_update(hctx, lenbuf, 4) < 0) {            \
			ok = 0;                                         \
			break;                                          \
		}                                                       \
	} while (0)

	HASH_U32((uint32_t)v_c_len, v_c, v_c_len);
	HASH_U32((uint32_t)v_s_len, v_s, v_s_len);
	HASH_U32((uint32_t)i_c_len, i_c, i_c_len);
	HASH_U32((uint32_t)i_s_len, i_s, i_s_len);
	HASH_U32((uint32_t)k_s_len, k_s, k_s_len);
	HASH_U32_ONLY(gex_min);
	HASH_U32_ONLY(gex_n);
	HASH_U32_ONLY(gex_max);

#undef HASH_U32
#undef HASH_U32_ONLY

	/* p, g, e, f, K are already in SSH mpint wire format */
	if (ok)
		ok = (dssh_hash_update(hctx, p_mpint, p_mpint_len) == 0);
	if (ok)
		ok = (dssh_hash_update(hctx, g_mpint, g_mpint_len) == 0);
	if (ok)
		ok = (dssh_hash_update(hctx, e_mpint, e_mpint_len) == 0);
	if (ok)
		ok = (dssh_hash_update(hctx, f_mpint, f_mpint_len) == 0);
	if (ok)
		ok = (dssh_hash_update(hctx, k_mpint, k_mpint_len) == 0);
	if (ok)
		ok = (dssh_hash_final(hctx, hash_out, SHA256_DIGEST_LEN) == 0);

	dssh_hash_free(hctx);
	return ok ? 0 : DSSH_ERROR_INIT;
}

/* Bit-length of an SSH mpint value body.  Skips leading zero bytes so
 * the at-most-one 0x00 byte the wire format prepends to keep a positive
 * integer out of the sign bit doesn't inflate the count. */
static size_t
mpint_bits(const uint8_t *data, size_t data_len)
{
	size_t i = 0;
	while (i < data_len && data[i] == 0)
		i++;
	if (i == data_len)
		return 0;
	size_t  bits = (data_len - i - 1) * 8;
	uint8_t top  = data[i];
	while (top) {
		bits++;
		top >>= 1;
	}
	return bits;
}

static int
dhgex_client(struct dssh_kex_context *kctx, const struct dhgex_ops *ops)
{
	int           res;
	dssh_key_algo ka          = kctx->key_algo;
	void         *dh_ctx      = NULL;
	uint8_t      *e_mpint     = NULL;
	uint8_t      *p_mpint     = NULL;
	uint8_t      *g_mpint     = NULL;
	uint8_t      *k_raw       = NULL;
	uint8_t      *k_mpint     = NULL;
	size_t        e_mpint_len = 0;
	size_t        k_raw_len   = 0;
	size_t        k_mpint_len = 0;

	uint32_t client_min = GEX_MIN;
	uint32_t client_n   = GEX_N;
	uint32_t client_max = GEX_MAX;
	uint8_t  msg_type;
	uint8_t *payload;
	size_t   payload_len;

	/* 1. Send GEX_REQUEST */
	{
		uint8_t msg[16];
		size_t  pos = 0;

		msg[pos++] = SSH_MSG_KEX_DH_GEX_REQUEST;
		res        = dssh_serialize_uint32(client_min, msg, sizeof(msg), &pos);
		if (res < 0)
			return res;
		res = dssh_serialize_uint32(client_n, msg, sizeof(msg), &pos);
		if (res < 0)
			return res;
		res = dssh_serialize_uint32(client_max, msg, sizeof(msg), &pos);
		if (res < 0)
			return res;
		res = kctx->send(msg, pos, kctx->io_ctx);
		if (res < 0)
			return res;
	}

	/* 2. Receive GEX_GROUP(p, g) */
	res = kctx->recv(&msg_type, &payload, &payload_len, kctx->io_ctx);
	if (res < 0)
		return res;
	if (msg_type != SSH_MSG_KEX_DH_GEX_GROUP)
		return DSSH_ERROR_PARSE;
	if (payload_len < 2)
		return DSSH_ERROR_PARSE;

	/* Reject a weak group before spending entropy on keygen.  RFC 4419
	 * §3 says the server SHOULD honor client_min, but nothing in the
	 * wire protocol forces it — the client must enforce, otherwise a
	 * hostile or misconfigured server can downgrade to a group small
	 * enough for Logjam-style precomputation. */
	if (payload_len < 1 + 4)
		return DSSH_ERROR_PARSE;
	uint32_t p_len_prefix;
	if (dssh_parse_uint32(&payload[1], payload_len - 1, &p_len_prefix) < 4)
		return DSSH_ERROR_PARSE;
	if ((size_t)p_len_prefix > payload_len - 1 - 4)
		return DSSH_ERROR_PARSE;
	if (mpint_bits(&payload[1 + 4], p_len_prefix) < client_min)
		return DSSH_ERROR_INVALID;

	/* 3. Parse group, generate keypair → e_mpint */
	size_t group_consumed;

	res = ops->client_keygen(&payload[1], payload_len - 1, &group_consumed, &e_mpint, &e_mpint_len, &dh_ctx);
	if (res < 0)
		return res;

	/* Copy p,g mpint byte ranges for exchange hash.
	 * They're in the payload at offset 1, total group_consumed bytes.
	 * Must copy now because the next recv will overwrite the payload
	 * buffer. */
	const uint8_t *pg_start = &payload[1];
	uint32_t       p_mpint_data_len;

	if (dssh_parse_uint32(pg_start, group_consumed, &p_mpint_data_len) < 4) {
		res = DSSH_ERROR_PARSE;
		goto cleanup;
	}
	if (p_mpint_data_len > group_consumed - 4) {
		res = DSSH_ERROR_PARSE;
		goto cleanup;
	}
	size_t p_mpint_len = 4 + p_mpint_data_len;
	size_t g_mpint_len = group_consumed - p_mpint_len;

	p_mpint = malloc(p_mpint_len);
	g_mpint = malloc(g_mpint_len);

	if (p_mpint == NULL || g_mpint == NULL) {
		res = DSSH_ERROR_ALLOC;
		goto cleanup;
	}
	memcpy(p_mpint, pg_start, p_mpint_len);
	memcpy(g_mpint, pg_start + p_mpint_len, g_mpint_len);

	/* 4. Send GEX_INIT(e) */
	{
		size_t   init_sz  = 1 + e_mpint_len;
		uint8_t *init_msg = malloc(init_sz);

		if (init_msg == NULL) {
			res = DSSH_ERROR_ALLOC;
			goto cleanup;
		}
		init_msg[0] = SSH_MSG_KEX_DH_GEX_INIT;
		memcpy(&init_msg[1], e_mpint, e_mpint_len);
		res = kctx->send(init_msg, init_sz, kctx->io_ctx);
		free(init_msg);
		if (res < 0)
			goto cleanup;
	}

	/* 5. Receive GEX_REPLY(K_S, f, sig) */
	res = kctx->recv(&msg_type, &payload, &payload_len, kctx->io_ctx);
	if (res < 0)
		goto cleanup;
	if (msg_type != SSH_MSG_KEX_DH_GEX_REPLY) {
		res = DSSH_ERROR_PARSE;
		goto cleanup;
	}

	/* Parse K_S */
	size_t   rpos = 1;
	uint32_t ks_len;

	if (rpos + 4 > payload_len) {
		res = DSSH_ERROR_PARSE;
		goto cleanup;
	}
	if (dssh_parse_uint32(&payload[rpos], payload_len - rpos, &ks_len) < 4) {
		res = DSSH_ERROR_PARSE;
		goto cleanup;
	}
	if (rpos + 4 + ks_len > payload_len) {
		res = DSSH_ERROR_PARSE;
		goto cleanup;
	}
	rpos += 4;
	const uint8_t *k_s = &payload[rpos];

	rpos += ks_len;

	/* 6. Derive K from f → also validates f */
	int64_t f_consumed;

	res = ops->client_derive(dh_ctx, &payload[rpos], payload_len - rpos, &f_consumed, &k_raw, &k_raw_len,
	    &k_mpint, &k_mpint_len);
	ops->free_ctx(dh_ctx);
	dh_ctx = NULL;
	if (res < 0)
		goto cleanup;

	/* f mpint for the hash */
	const uint8_t *f_mpint     = &payload[rpos];
	size_t         f_mpint_len = (size_t)f_consumed;

	rpos += f_mpint_len;

	/* Parse signature */
	uint32_t sig_len;

	if (rpos + 4 > payload_len) {
		res = DSSH_ERROR_PARSE;
		goto cleanup;
	}
	if (dssh_parse_uint32(&payload[rpos], payload_len - rpos, &sig_len) < 4) {
		res = DSSH_ERROR_PARSE;
		goto cleanup;
	}
	if (rpos + 4 + sig_len > payload_len) {
		res = DSSH_ERROR_PARSE;
		goto cleanup;
	}
	rpos += 4;
	const uint8_t *sig_h = &payload[rpos];

	/* 7. Compute exchange hash */
	uint8_t hash[SHA256_DIGEST_LEN];

	res = compute_exchange_hash(kctx->v_c, kctx->v_c_len, kctx->v_s, kctx->v_s_len, kctx->i_c, kctx->i_c_len,
	    kctx->i_s, kctx->i_s_len, k_s, ks_len, client_min, client_n, client_max, p_mpint, p_mpint_len, g_mpint,
	    g_mpint_len, e_mpint, e_mpint_len, f_mpint, f_mpint_len, k_mpint, k_mpint_len, hash);
	free(p_mpint);
	p_mpint = NULL;
	free(g_mpint);
	g_mpint = NULL;
	free(e_mpint);
	e_mpint = NULL;
	dssh_cleanse(k_mpint, k_mpint_len);
	free(k_mpint);
	k_mpint = NULL;
	if (res < 0)
		goto cleanup;

	/* 8. Verify key algorithm and host key */
	if (!ka || !ka->verify) {
		res = DSSH_ERROR_INIT;
		goto cleanup;
	}

	if (kctx->hostkey_verify) {
		uint8_t fp[32];

		if (dssh_hash_oneshot("SHA-256", k_s, ks_len, fp, sizeof(fp)) < 0) {
			res = DSSH_ERROR_INIT;
			goto cleanup;
		}
		unsigned int bits = ka->key_bits ? ka->key_bits(k_s, ks_len) : 0;

		if (kctx->hostkey_verify(ka->name, bits, fp, k_s, ks_len,
		        kctx->hostkey_verify_cbdata) != DSSH_HOSTKEY_ACCEPT) {
			res = DSSH_ERROR_INVALID;
			goto cleanup;
		}
	}

	/* 9. Verify signature */
	res = ka->verify(k_s, ks_len, sig_h, sig_len, hash, SHA256_DIGEST_LEN);
	if (res < 0)
		goto cleanup;

	/* 9. Store results */
	kctx->exchange_hash = malloc(SHA256_DIGEST_LEN);
	if (!kctx->exchange_hash) {
		res = DSSH_ERROR_ALLOC;
		goto cleanup;
	}
	memcpy(kctx->exchange_hash, hash, SHA256_DIGEST_LEN);
	kctx->exchange_hash_sz = SHA256_DIGEST_LEN;
	kctx->shared_secret    = k_raw;
	kctx->shared_secret_sz = k_raw_len;
	k_raw                  = NULL;
	res                    = 0;

cleanup:
	free(e_mpint);
	free(p_mpint);
	free(g_mpint);
	if (k_mpint) {
		dssh_cleanse(k_mpint, k_mpint_len);
		free(k_mpint);
	}
	if (k_raw) {
		dssh_cleanse(k_raw, k_raw_len);
		free(k_raw);
	}
	if (dh_ctx)
		ops->free_ctx(dh_ctx);
	return res;
}

static int
dhgex_server(struct dssh_kex_context *kctx, const struct dhgex_ops *ops)
{
	int           res;
	dssh_key_algo ka          = kctx->key_algo;
	void         *dh_ctx      = NULL;
	uint8_t      *p_mpint     = NULL;
	uint8_t      *g_mpint     = NULL;
	uint8_t      *f_mpint     = NULL;
	uint8_t      *k_raw       = NULL;
	uint8_t      *k_mpint     = NULL;
	uint8_t      *sig_buf     = NULL;
	size_t        p_mpint_len = 0;
	size_t        g_mpint_len = 0;
	size_t        f_mpint_len = 0;
	size_t        k_raw_len   = 0;
	size_t        k_mpint_len = 0;
	size_t        sig_len     = 0;

	uint32_t client_min, client_n, client_max;
	uint8_t  msg_type;
	uint8_t *payload;
	size_t   payload_len;

	if (!ka || !ka->pubkey || !ka->sign)
		return DSSH_ERROR_INIT;

	const uint8_t *k_s_buf = NULL;
	size_t         k_s_len;

	res = ka->pubkey(&k_s_buf, &k_s_len, ka->ctx);
	if (res < 0)
		return res;

	/* 1. Receive GEX_REQUEST(min, n, max) */
	res = kctx->recv(&msg_type, &payload, &payload_len, kctx->io_ctx);
	if (res < 0)
		return res;
	if (msg_type != SSH_MSG_KEX_DH_GEX_REQUEST)
		return DSSH_ERROR_PARSE;
	if (payload_len < 1 + 12)
		return DSSH_ERROR_PARSE;
	if (dssh_parse_uint32(&payload[1], payload_len - 1, &client_min) < 4
	    || dssh_parse_uint32(&payload[5], payload_len - 5, &client_n) < 4
	    || dssh_parse_uint32(&payload[9], payload_len - 9, &client_max) < 4)
		return DSSH_ERROR_PARSE;

	/* 2. Select group via provider */
	struct dssh_dh_gex_provider *prov = (struct dssh_dh_gex_provider *)kctx->kex_data;

	if ((prov == NULL) || (prov->select_group == NULL))
		return DSSH_ERROR_INIT;

	uint8_t *p_bytes = NULL, *g_bytes = NULL;
	size_t   p_len, g_len;

	res =
	    prov->select_group(client_min, client_n, client_max, &p_bytes, &p_len, &g_bytes, &g_len, prov->cbdata);
	if (res < 0)
		return res;

	/* 3. Load group, generate keypair → p_mpint, g_mpint, f_mpint */
	res = ops->server_keygen(p_bytes, p_len, g_bytes, g_len, &p_mpint, &p_mpint_len, &g_mpint, &g_mpint_len,
	    &f_mpint, &f_mpint_len, &dh_ctx);
	free(p_bytes);
	free(g_bytes);
	if (res < 0)
		return res;

	/* 4. Send GEX_GROUP(p, g) */
	{
		size_t   group_sz  = 1 + p_mpint_len + g_mpint_len;
		uint8_t *group_msg = malloc(group_sz);

		if (group_msg == NULL) {
			res = DSSH_ERROR_ALLOC;
			goto cleanup;
		}
		size_t pos = 0;

		group_msg[pos++] = SSH_MSG_KEX_DH_GEX_GROUP;
		memcpy(&group_msg[pos], p_mpint, p_mpint_len);
		pos += p_mpint_len;
		memcpy(&group_msg[pos], g_mpint, g_mpint_len);
		pos += g_mpint_len;
		res = kctx->send(group_msg, pos, kctx->io_ctx);
		free(group_msg);
		if (res < 0)
			goto cleanup;
	}

	/* 5. Receive GEX_INIT(e) */
	res = kctx->recv(&msg_type, &payload, &payload_len, kctx->io_ctx);
	if (res < 0)
		goto cleanup;
	if (msg_type != SSH_MSG_KEX_DH_GEX_INIT) {
		res = DSSH_ERROR_PARSE;
		goto cleanup;
	}

	/* 6. Parse e, validate, derive K */
	int64_t e_consumed;

	res = ops->server_derive(dh_ctx, &payload[1], payload_len - 1, &e_consumed, &k_raw, &k_raw_len, &k_mpint,
	    &k_mpint_len);
	ops->free_ctx(dh_ctx);
	dh_ctx = NULL;
	if (res < 0)
		goto cleanup;

	/* e mpint for the hash */
	const uint8_t *e_mpint     = &payload[1];
	size_t         e_mpint_len = (size_t)e_consumed;

	/* 7. Compute exchange hash */
	uint8_t hash[SHA256_DIGEST_LEN];

	res = compute_exchange_hash(kctx->v_c, kctx->v_c_len, kctx->v_s, kctx->v_s_len, kctx->i_c, kctx->i_c_len,
	    kctx->i_s, kctx->i_s_len, k_s_buf, k_s_len, client_min, client_n, client_max, p_mpint, p_mpint_len,
	    g_mpint, g_mpint_len, e_mpint, e_mpint_len, f_mpint, f_mpint_len, k_mpint, k_mpint_len, hash);
	free(p_mpint);
	p_mpint = NULL;
	free(g_mpint);
	g_mpint = NULL;
	dssh_cleanse(k_mpint, k_mpint_len);
	free(k_mpint);
	k_mpint = NULL;
	if (res < 0)
		goto cleanup;

	/* 8. Sign exchange hash */
	res = ka->sign(&sig_buf, &sig_len, hash, SHA256_DIGEST_LEN, ka->ctx);
	if (res < 0)
		goto cleanup;

	/* 9. Send GEX_REPLY(K_S, f, sig) */
	if (k_s_len > UINT32_MAX || sig_len > UINT32_MAX) {
		res = DSSH_ERROR_INVALID;
		goto cleanup;
	}
	{
		/* 1 (msg) + 4 (k_s len) + k_s + f_mpint + 4 (sig len) + sig */
		size_t reply_sz = 9;

		if (k_s_len > SIZE_MAX - reply_sz) {
			res = DSSH_ERROR_INVALID;
			goto cleanup;
		}
		reply_sz += k_s_len;
		if (f_mpint_len > SIZE_MAX - reply_sz) {
			res = DSSH_ERROR_INVALID;
			goto cleanup;
		}
		reply_sz += f_mpint_len;
		if (sig_len > SIZE_MAX - reply_sz) {
			res = DSSH_ERROR_INVALID;
			goto cleanup;
		}
		reply_sz += sig_len;
		uint8_t *reply = malloc(reply_sz);

		if (!reply) {
			res = DSSH_ERROR_ALLOC;
			goto cleanup;
		}

		size_t   pos         = 0;
		uint32_t k_s_len_u32 = (uint32_t)k_s_len;
		uint32_t sig_len_u32 = (uint32_t)sig_len;

		reply[pos++] = SSH_MSG_KEX_DH_GEX_REPLY;
		res          = dssh_serialize_uint32(k_s_len_u32, reply, reply_sz, &pos);
		if (res < 0) {
			free(reply);
			goto cleanup;
		}
		memcpy(&reply[pos], k_s_buf, k_s_len);
		pos += k_s_len;
		memcpy(&reply[pos], f_mpint, f_mpint_len);
		pos += f_mpint_len;
		free(f_mpint);
		f_mpint = NULL;
		res     = dssh_serialize_uint32(sig_len_u32, reply, reply_sz, &pos);
		if (res < 0) {
			free(reply);
			goto cleanup;
		}
		memcpy(&reply[pos], sig_buf, sig_len);
		pos += sig_len;
		free(sig_buf);
		sig_buf = NULL;
		res     = kctx->send(reply, pos, kctx->io_ctx);
		free(reply);
		if (res < 0)
			goto cleanup;
	}

	/* 10. Store results */
	kctx->exchange_hash = malloc(SHA256_DIGEST_LEN);
	if (!kctx->exchange_hash) {
		res = DSSH_ERROR_ALLOC;
		goto cleanup;
	}
	memcpy(kctx->exchange_hash, hash, SHA256_DIGEST_LEN);
	kctx->exchange_hash_sz = SHA256_DIGEST_LEN;
	kctx->shared_secret    = k_raw;
	kctx->shared_secret_sz = k_raw_len;
	k_raw                  = NULL;
	res                    = 0;

cleanup:
	free(p_mpint);
	free(g_mpint);
	free(f_mpint);
	free(sig_buf);
	if (k_mpint) {
		dssh_cleanse(k_mpint, k_mpint_len);
		free(k_mpint);
	}
	if (k_raw) {
		dssh_cleanse(k_raw, k_raw_len);
		free(k_raw);
	}
	if (dh_ctx)
		ops->free_ctx(dh_ctx);
	return res;
}

DSSH_PRIVATE int
dhgex_handler_impl(struct dssh_kex_context *kctx, const struct dhgex_ops *ops)
{
	if (kctx->client)
		return dhgex_client(kctx, ops);
	return dhgex_server(kctx, ops);
}
