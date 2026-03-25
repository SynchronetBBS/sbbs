// RFC 4419: Diffie-Hellman Group Exchange with SHA-256

#include <openssl/bn.h>
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/param_build.h>
#include <stdlib.h>
#include <string.h>

#include "kex/dh-gex-sha256.h"
#include "ssh-internal.h"

#define SHA256_DIGEST_LEN 32
#define KEX_NAME "diffie-hellman-group-exchange-sha256"
#define KEX_NAME_LEN 36

#define SSH_MSG_KEX_DH_GEX_REQUEST UINT8_C(34)
#define SSH_MSG_KEX_DH_GEX_GROUP UINT8_C(31)
#define SSH_MSG_KEX_DH_GEX_INIT UINT8_C(32)
#define SSH_MSG_KEX_DH_GEX_REPLY UINT8_C(33)

#define GEX_MIN 2048
#define GEX_N 4096
#define GEX_MAX 8192

DSSH_TESTABLE int
serialize_bn_mpint(const BIGNUM *bn, uint8_t *buf, size_t bufsz, size_t *pos)
{
	int      bn_bytes = BN_num_bytes(bn);
	uint8_t *tmp = malloc(bn_bytes);

	if (tmp == NULL)
		return DSSH_ERROR_ALLOC;
	BN_bn2bin(bn, tmp);

	bool     need_pad = (bn_bytes > 0 && (tmp[0] & 0x80));
	uint32_t mpint_len = bn_bytes + (need_pad ? 1 : 0);

	if (*pos + 4 + mpint_len > bufsz) {
		free(tmp);
		return DSSH_ERROR_TOOLONG;
	}

	dssh_serialize_uint32(mpint_len, buf, bufsz, pos);
	if (need_pad)
		buf[(*pos)++] = 0;
	memcpy(&buf[*pos], tmp, bn_bytes);
	*pos += bn_bytes;

	OPENSSL_cleanse(tmp, bn_bytes);
	free(tmp);
	return 0;
}

DSSH_TESTABLE int64_t
parse_bn_mpint(const uint8_t *buf, size_t bufsz, BIGNUM **bn)
{
	if (bufsz < 4)
		return DSSH_ERROR_PARSE;

	uint32_t len;

        /* dssh_parse_uint32 cannot fail here: it only fails when
         * bufsz < 4, which is already ruled out above. */
#ifndef DSSH_TESTING
	if (dssh_parse_uint32(buf, bufsz, &len) < 4)
		return DSSH_ERROR_PARSE;
#else
	dssh_parse_uint32(buf, bufsz, &len);
#endif
	if (4 + len > bufsz)
		return DSSH_ERROR_PARSE;
	*bn = BN_bin2bn(&buf[4], len, NULL);
	if (*bn == NULL)
		return DSSH_ERROR_ALLOC;
	return 4 + len;
}

/*
 * RFC 4253 s8: Values of e or f not in [1, p-1] MUST NOT be
 * sent or accepted.
 */
DSSH_TESTABLE bool
dh_value_valid(const BIGNUM *val, const BIGNUM *p)
{
	if (BN_is_zero(val) || BN_is_negative(val))
		return false;
	if (BN_cmp(val, p) >= 0)
		return false;
	return true;
}

/*
 * Compute exchange hash:
 * H = SHA256(V_C || V_S || I_C || I_S || K_S ||
 *            min || n || max || p || g || e || f || K)
 */
DSSH_TESTABLE int
compute_exchange_hash(const char *v_c, size_t v_c_len,
    const char *v_s, size_t v_s_len,
    const uint8_t *i_c, size_t i_c_len,
    const uint8_t *i_s, size_t i_s_len,
    const uint8_t *k_s, size_t k_s_len,
    uint32_t gex_min, uint32_t gex_n, uint32_t gex_max,
    const BIGNUM *p, const BIGNUM *g,
    const BIGNUM *e, const BIGNUM *f, const BIGNUM *k,
    uint8_t *hash_out)
{
	EVP_MD_CTX *mdctx = EVP_MD_CTX_new();

	if (mdctx == NULL)
		return DSSH_ERROR_ALLOC;

	uint8_t lenbuf[4];
	int     ok = EVP_DigestInit_ex(mdctx, EVP_sha256(), NULL);

	{
		size_t tp = 0;

		dssh_serialize_uint32((uint32_t)v_c_len, lenbuf, 4, &tp);
	}

	ok = ok && EVP_DigestUpdate(mdctx, lenbuf, 4);
	ok = ok && EVP_DigestUpdate(mdctx, v_c, v_c_len);

	{
		size_t tp = 0;

		dssh_serialize_uint32((uint32_t)v_s_len, lenbuf, 4, &tp);
	}
	ok = ok && EVP_DigestUpdate(mdctx, lenbuf, 4);
	ok = ok && EVP_DigestUpdate(mdctx, v_s, v_s_len);

	{
		size_t tp = 0;

		dssh_serialize_uint32((uint32_t)i_c_len, lenbuf, 4, &tp);
	}
	ok = ok && EVP_DigestUpdate(mdctx, lenbuf, 4);
	ok = ok && EVP_DigestUpdate(mdctx, i_c, i_c_len);

	{
		size_t tp = 0;

		dssh_serialize_uint32((uint32_t)i_s_len, lenbuf, 4, &tp);
	}
	ok = ok && EVP_DigestUpdate(mdctx, lenbuf, 4);
	ok = ok && EVP_DigestUpdate(mdctx, i_s, i_s_len);

	{
		size_t tp = 0;

		dssh_serialize_uint32((uint32_t)k_s_len, lenbuf, 4, &tp);
	}
	ok = ok && EVP_DigestUpdate(mdctx, lenbuf, 4);
	ok = ok && EVP_DigestUpdate(mdctx, k_s, k_s_len);

	{
		size_t tp = 0;

		dssh_serialize_uint32(gex_min, lenbuf, 4, &tp);
	}
	ok = ok && EVP_DigestUpdate(mdctx, lenbuf, 4);
	{
		size_t tp = 0;

		dssh_serialize_uint32(gex_n, lenbuf, 4, &tp);
	}
	ok = ok && EVP_DigestUpdate(mdctx, lenbuf, 4);
	{
		size_t tp = 0;

		dssh_serialize_uint32(gex_max, lenbuf, 4, &tp);
	}
	ok = ok && EVP_DigestUpdate(mdctx, lenbuf, 4);

	{
		uint8_t mpbuf[4096];
		size_t  mp;
		int     mres;

		mp = 0;
		mres = serialize_bn_mpint(p, mpbuf, sizeof(mpbuf), &mp);
		ok = ok && (mres == 0) && EVP_DigestUpdate(mdctx, mpbuf, mp);

		mp = 0;
		mres = serialize_bn_mpint(g, mpbuf, sizeof(mpbuf), &mp);
		ok = ok && (mres == 0) && EVP_DigestUpdate(mdctx, mpbuf, mp);

		mp = 0;
		mres = serialize_bn_mpint(e, mpbuf, sizeof(mpbuf), &mp);
		ok = ok && (mres == 0) && EVP_DigestUpdate(mdctx, mpbuf, mp);

		mp = 0;
		mres = serialize_bn_mpint(f, mpbuf, sizeof(mpbuf), &mp);
		ok = ok && (mres == 0) && EVP_DigestUpdate(mdctx, mpbuf, mp);

		mp = 0;
		mres = serialize_bn_mpint(k, mpbuf, sizeof(mpbuf), &mp);
		ok = ok && (mres == 0) && EVP_DigestUpdate(mdctx, mpbuf, mp);

		OPENSSL_cleanse(mpbuf, sizeof(mpbuf));
	}

	ok = ok && EVP_DigestFinal_ex(mdctx, hash_out, NULL);
	EVP_MD_CTX_free(mdctx);
	return ok ? 0 : DSSH_ERROR_INIT;
}

DSSH_TESTABLE int
dhgex_handler(dssh_session sess)
{
	int            res;
	BIGNUM        *p = NULL, *g = NULL, *e_bn = NULL, *f_bn = NULL, *k_bn = NULL;
	BN_CTX        *bnctx = NULL;

	const char    *v_c, *v_s;
	size_t         v_c_len, v_s_len;
	const uint8_t *i_c, *i_s;
	size_t         i_c_len, i_s_len;

	if (sess->trans.client) {
		v_c = sess->trans.id_str;
		v_c_len = sess->trans.id_str_sz;
		v_s = sess->trans.remote_id_str;
		v_s_len = sess->trans.remote_id_str_sz;
		i_c = sess->trans.our_kexinit;
		i_c_len = sess->trans.our_kexinit_sz;
		i_s = sess->trans.peer_kexinit;
		i_s_len = sess->trans.peer_kexinit_sz;
	}
	else {
		v_c = sess->trans.remote_id_str;
		v_c_len = sess->trans.remote_id_str_sz;
		v_s = sess->trans.id_str;
		v_s_len = sess->trans.id_str_sz;
		i_c = sess->trans.peer_kexinit;
		i_c_len = sess->trans.peer_kexinit_sz;
		i_s = sess->trans.our_kexinit;
		i_s_len = sess->trans.our_kexinit_sz;
	}

	uint32_t client_min, client_n, client_max;
	uint8_t  msg_type;
	uint8_t *payload;
	size_t   payload_len;

	if (sess->trans.client) {
                /* ---- CLIENT SIDE ---- */
		client_min = GEX_MIN;
		client_n = GEX_N;
		client_max = GEX_MAX;

                /* 1. Send GEX_REQUEST */
		{
			uint8_t msg[16];
			size_t  pos = 0;

			msg[pos++] = SSH_MSG_KEX_DH_GEX_REQUEST;
			dssh_serialize_uint32(client_min, msg, sizeof(msg), &pos);
			dssh_serialize_uint32(client_n, msg, sizeof(msg), &pos);
			dssh_serialize_uint32(client_max, msg, sizeof(msg), &pos);
			res = dssh_transport_send_packet(sess, msg, pos, NULL);
			if (res < 0)
				return res;
		}

                /* 2. Receive GEX_GROUP(p, g) */
		res = dssh_transport_recv_packet(sess, &msg_type, &payload, &payload_len);
		if (res < 0)
			return res;
		if (msg_type != SSH_MSG_KEX_DH_GEX_GROUP)
			return DSSH_ERROR_PARSE;
		{
			size_t rpos = 1;

			if (rpos >= payload_len)
				return DSSH_ERROR_PARSE;

			int64_t n = parse_bn_mpint(&payload[rpos], payload_len - rpos, &p);

			if (n < 0)
				return (int)n;
			rpos += n;
			if (rpos >= payload_len) {
				BN_free(p);
				return DSSH_ERROR_PARSE;
			}
			n = parse_bn_mpint(&payload[rpos], payload_len - rpos, &g);
			if (n < 0) {
				BN_free(p);
				return (int)n;
			}
		}

                /* 3. Generate x, compute e = g^x mod p */
		bnctx = BN_CTX_new();
		if (!bnctx) {
			res = DSSH_ERROR_ALLOC;
			goto cleanup;
		}
		e_bn = BN_new();
		if (!e_bn) {
			res = DSSH_ERROR_ALLOC;
			goto cleanup;
		}

		BIGNUM *x = BN_new();

		if (!x) {
			res = DSSH_ERROR_ALLOC;
			goto cleanup;
		}
		if ((BN_rand(x, BN_num_bits(p) - 1, BN_RAND_TOP_ONE, BN_RAND_BOTTOM_ANY) != 1)
		    || (BN_mod_exp(e_bn, g, x, p, bnctx) != 1)) {
			BN_clear_free(x);
			res = DSSH_ERROR_INIT;
			goto cleanup;
		}

                /* 4. Send GEX_INIT(e) */
		{
			uint8_t init_msg[4096];
			size_t  pos = 0;

                        /* 4096-byte buffer is adequate for DH e value. */
			init_msg[pos++] = SSH_MSG_KEX_DH_GEX_INIT;
			res = serialize_bn_mpint(e_bn, init_msg, sizeof(init_msg), &pos);
			if (res < 0) {
				BN_clear_free(x);
				goto cleanup;
			}
			res = dssh_transport_send_packet(sess, init_msg, pos, NULL);
			if (res < 0) {
				BN_clear_free(x);
				goto cleanup;
			}
		}

                /* 5. Receive GEX_REPLY(K_S, f, sig) */
		res = dssh_transport_recv_packet(sess, &msg_type, &payload, &payload_len);
		if (res < 0) {
			BN_clear_free(x);
			goto cleanup;
		}
		if (msg_type != SSH_MSG_KEX_DH_GEX_REPLY) {
			BN_clear_free(x);
			res = DSSH_ERROR_PARSE;
			goto cleanup;
		}

		size_t   rpos = 1;
		uint32_t ks_len;

		if (rpos + 4 > payload_len) {
			BN_clear_free(x);
			res = DSSH_ERROR_PARSE;
			goto cleanup;
		}

                /* dssh_parse_uint32 cannot fail here: bufsz >= 4
                 * guaranteed by the check above. */
#ifndef DSSH_TESTING
		if (dssh_parse_uint32(&payload[rpos], payload_len - rpos, &ks_len) < 4) {
			BN_clear_free(x);
			res = DSSH_ERROR_PARSE;
			goto cleanup;
		}
#else
		dssh_parse_uint32(&payload[rpos], payload_len - rpos, &ks_len);
#endif
		if (rpos + 4 + ks_len > payload_len) {
			BN_clear_free(x);
			res = DSSH_ERROR_PARSE;
			goto cleanup;
		}
		rpos += 4;

		const uint8_t *k_s = &payload[rpos];

		rpos += ks_len;

		int64_t fn = parse_bn_mpint(&payload[rpos], payload_len - rpos, &f_bn);

		if (fn < 0) {
			BN_clear_free(x);
			res = (int)fn;
			goto cleanup;
		}
		rpos += fn;

                /* RFC 4253 s8: f MUST be in [1, p-1] */
		if (!dh_value_valid(f_bn, p)) {
			BN_clear_free(x);
			res = DSSH_ERROR_INVALID;
			goto cleanup;
		}

		uint32_t sig_len;

		if (rpos + 4 > payload_len) {
			BN_clear_free(x);
			res = DSSH_ERROR_PARSE;
			goto cleanup;
		}
#ifndef DSSH_TESTING
		if (dssh_parse_uint32(&payload[rpos], payload_len - rpos, &sig_len) < 4) {
			BN_clear_free(x);
			res = DSSH_ERROR_PARSE;
			goto cleanup;
		}
#else
		dssh_parse_uint32(&payload[rpos], payload_len - rpos, &sig_len);
#endif
		if (rpos + 4 + sig_len > payload_len) {
			BN_clear_free(x);
			res = DSSH_ERROR_PARSE;
			goto cleanup;
		}
		rpos += 4;

		const uint8_t *sig_h = &payload[rpos];

                /* 6. Compute K = f^x mod p */
		k_bn = BN_new();
		if (!k_bn || (BN_mod_exp(k_bn, f_bn, x, p, bnctx) != 1)) {
			BN_clear_free(x);
			res = DSSH_ERROR_INIT;
			goto cleanup;
		}
		BN_clear_free(x);

                /* Store shared secret */
		int k_bytes = BN_num_bytes(k_bn);

		sess->trans.shared_secret = malloc(k_bytes);
		if (!sess->trans.shared_secret) {
			res = DSSH_ERROR_ALLOC;
			goto cleanup;
		}
		BN_bn2bin(k_bn, sess->trans.shared_secret);
		sess->trans.shared_secret_sz = k_bytes;

                /* 7. Compute and verify exchange hash */
		uint8_t hash[SHA256_DIGEST_LEN];

		res = compute_exchange_hash(v_c, v_c_len, v_s, v_s_len,
		        i_c, i_c_len, i_s, i_s_len, k_s, ks_len,
		        client_min, client_n, client_max, p, g, e_bn, f_bn, k_bn, hash);
		if (res < 0)
			goto cleanup;

                /* ka always set by negotiation; all key algos have verify. */
		dssh_key_algo ka = sess->trans.key_algo_selected;

		if (!ka || !ka->verify) {
			res = DSSH_ERROR_INIT;
			goto cleanup;
		}
		res = ka->verify(k_s, ks_len, sig_h, sig_len, hash, SHA256_DIGEST_LEN);
		if (res < 0)
			goto cleanup;

		sess->trans.exchange_hash = malloc(SHA256_DIGEST_LEN);
		if (!sess->trans.exchange_hash) {
			res = DSSH_ERROR_ALLOC;
			goto cleanup;
		}
		memcpy(sess->trans.exchange_hash, hash, SHA256_DIGEST_LEN);
		sess->trans.exchange_hash_sz = SHA256_DIGEST_LEN;
		res = 0;
	}
	else {
                /* ---- SERVER SIDE ----
                 * ka and its function pointers are always set by negotiation.
                 * Own key pubkey call always succeeds after keygen. */
		dssh_key_algo ka = sess->trans.key_algo_selected;

		if (!ka || !ka->pubkey || !ka->sign)
			return DSSH_ERROR_INIT;

                /* Get host key blob */
		uint8_t k_s_buf[1024];
		size_t  k_s_len;

		res = ka->pubkey(k_s_buf, sizeof(k_s_buf), &k_s_len, ka->ctx);
		if (res < 0)
			return res;

                /* 1. Receive GEX_REQUEST(min, n, max) */
		res = dssh_transport_recv_packet(sess, &msg_type, &payload, &payload_len);
		if (res < 0)
			return res;
		if (msg_type != SSH_MSG_KEX_DH_GEX_REQUEST)
			return DSSH_ERROR_PARSE;
		if (payload_len < 1 + 12)
			return DSSH_ERROR_PARSE;
		dssh_parse_uint32(&payload[1], payload_len - 1, &client_min);
		dssh_parse_uint32(&payload[5], payload_len - 5, &client_n);
		dssh_parse_uint32(&payload[9], payload_len - 9, &client_max);

                /* 2. Select and send a DH group via the application's provider. */
		struct dssh_dh_gex_provider *prov =
		    (struct dssh_dh_gex_provider *)sess->trans.kex_ctx;

		if ((prov == NULL) || (prov->select_group == NULL)) {
			res = DSSH_ERROR_INIT;
			goto cleanup;
		}

		uint8_t *p_bytes = NULL, *g_bytes = NULL;
		size_t   p_len, g_len;

		res = prov->select_group(client_min, client_n, client_max,
		        &p_bytes, &p_len, &g_bytes, &g_len, prov->cbdata);
		if (res < 0)
			goto cleanup;
		p = BN_bin2bn(p_bytes, p_len, NULL);
		free(p_bytes);
		if (!p) {
			free(g_bytes);
			res = DSSH_ERROR_ALLOC;
			goto cleanup;
		}
		g = BN_bin2bn(g_bytes, g_len, NULL);
		free(g_bytes);
		if (!g) {
			res = DSSH_ERROR_ALLOC;
			goto cleanup;
		}

		{
			uint8_t group_msg[4096];
			size_t  pos = 0;

                        /* 4096-byte buffer is adequate for DH primes. */
			group_msg[pos++] = SSH_MSG_KEX_DH_GEX_GROUP;
			res = serialize_bn_mpint(p, group_msg, sizeof(group_msg), &pos);
			if (res < 0)
				goto cleanup;
			res = serialize_bn_mpint(g, group_msg, sizeof(group_msg), &pos);
			if (res < 0)
				goto cleanup;
			res = dssh_transport_send_packet(sess, group_msg, pos, NULL);
			if (res < 0)
				goto cleanup;
		}

                /* 3. Receive GEX_INIT(e) */
		res = dssh_transport_recv_packet(sess, &msg_type, &payload, &payload_len);
		if (res < 0)
			goto cleanup;
		if (msg_type != SSH_MSG_KEX_DH_GEX_INIT) {
			res = DSSH_ERROR_PARSE;
			goto cleanup;
		}
		{
			size_t  rpos = 1;
			int64_t n = parse_bn_mpint(&payload[rpos], payload_len - rpos, &e_bn);

			if (n < 0) {
				res = (int)n;
				goto cleanup;
			}
		}

                /* RFC 4253 s8: e MUST be in [1, p-1] */
		if (!dh_value_valid(e_bn, p)) {
			res = DSSH_ERROR_INVALID;
			goto cleanup;
		}

                /* 4. Generate y, compute f = g^y mod p, K = e^y mod p */
		bnctx = BN_CTX_new();
		if (!bnctx) {
			res = DSSH_ERROR_ALLOC;
			goto cleanup;
		}
		f_bn = BN_new();
		if (!f_bn) {
			res = DSSH_ERROR_ALLOC;
			goto cleanup;
		}
		k_bn = BN_new();
		if (!k_bn) {
			res = DSSH_ERROR_ALLOC;
			goto cleanup;
		}

		BIGNUM *y = BN_new();

		if (!y) {
			res = DSSH_ERROR_ALLOC;
			goto cleanup;
		}
		if ((BN_rand(y, BN_num_bits(p) - 1, BN_RAND_TOP_ONE, BN_RAND_BOTTOM_ANY) != 1)
		    || (BN_mod_exp(f_bn, g, y, p, bnctx) != 1)
		    || (BN_mod_exp(k_bn, e_bn, y, p, bnctx) != 1)) {
			BN_clear_free(y);
			res = DSSH_ERROR_INIT;
			goto cleanup;
		}
		BN_clear_free(y);

                /* Store shared secret */
		int k_bytes = BN_num_bytes(k_bn);

		sess->trans.shared_secret = malloc(k_bytes);
		if (!sess->trans.shared_secret) {
			res = DSSH_ERROR_ALLOC;
			goto cleanup;
		}
		BN_bn2bin(k_bn, sess->trans.shared_secret);
		sess->trans.shared_secret_sz = k_bytes;

                /* 5. Compute exchange hash */
		uint8_t hash[SHA256_DIGEST_LEN];

		res = compute_exchange_hash(v_c, v_c_len, v_s, v_s_len,
		        i_c, i_c_len, i_s, i_s_len, k_s_buf, k_s_len,
		        client_min, client_n, client_max, p, g, e_bn, f_bn, k_bn, hash);
		if (res < 0)
			goto cleanup;

                /* 6. Sign exchange hash */
		uint8_t sig_buf[1024];
		size_t  sig_len;

		res = ka->sign(sig_buf, sizeof(sig_buf), &sig_len,
		        hash, SHA256_DIGEST_LEN, ka->ctx);
		if (res < 0)
			goto cleanup;

                /* 7. Send GEX_REPLY(K_S, f, sig) */
		{
			size_t   reply_sz = 1 + 4 + k_s_len + 4096 + 4 + sig_len;
			uint8_t *reply = malloc(reply_sz);

			if (!reply) {
				res = DSSH_ERROR_ALLOC;
				goto cleanup;
			}

			size_t pos = 0;

			reply[pos++] = SSH_MSG_KEX_DH_GEX_REPLY;
			dssh_serialize_uint32((uint32_t)k_s_len, reply, reply_sz, &pos);
			memcpy(&reply[pos], k_s_buf, k_s_len);
			pos += k_s_len;

                        /* Buffer is adequately sized for the DH value. */
			res = serialize_bn_mpint(f_bn, reply, reply_sz, &pos);
			if (res < 0) {
				free(reply);
				goto cleanup;
			}
			dssh_serialize_uint32((uint32_t)sig_len, reply, reply_sz, &pos);
			memcpy(&reply[pos], sig_buf, sig_len);
			pos += sig_len;
			res = dssh_transport_send_packet(sess, reply, pos, NULL);
			free(reply);
			if (res < 0)
				goto cleanup;
		}

		sess->trans.exchange_hash = malloc(SHA256_DIGEST_LEN);
		if (!sess->trans.exchange_hash) {
			res = DSSH_ERROR_ALLOC;
			goto cleanup;
		}
		memcpy(sess->trans.exchange_hash, hash, SHA256_DIGEST_LEN);
		sess->trans.exchange_hash_sz = SHA256_DIGEST_LEN;
		res = 0;
	}

cleanup:
	BN_CTX_free(bnctx);
	BN_free(p);
	BN_free(g);
	BN_free(e_bn);
	BN_free(f_bn);
	BN_clear_free(k_bn);
	return res;
}

static void
kex_cleanup(dssh_session sess)
{
}

DSSH_PUBLIC void
dssh_dh_gex_set_provider(dssh_session sess,
    struct dssh_dh_gex_provider      *provider)
{
	sess->trans.kex_ctx = provider;
}

DSSH_PUBLIC int
register_dh_gex_sha256(void)
{
	struct dssh_kex_s *kex = malloc(sizeof(*kex) + KEX_NAME_LEN + 1);

	if (kex == NULL)
		return DSSH_ERROR_ALLOC;
	kex->next = NULL;
	kex->handler = dhgex_handler;
	kex->cleanup = kex_cleanup;
	kex->flags = DSSH_KEX_FLAG_NEEDS_SIGNATURE_CAPABLE;
	kex->hash_name = "SHA256";
	memcpy(kex->name, KEX_NAME, KEX_NAME_LEN + 1);
	return dssh_transport_register_kex(kex);
}
