// RFC 8731: curve25519-sha256 Key Exchange

#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <stdlib.h>
#include <string.h>

#include "deucessh.h"

#define X25519_KEY_LEN    32
#define SHA256_DIGEST_LEN 32
#define KEX_NAME          "curve25519-sha256"
#define KEX_NAME_LEN      17

/*
 * Compute the exchange hash:
 * H = SHA256(V_C || V_S || I_C || I_S || K_S || Q_C || Q_S || K_mpint)
 *
 * All string values are encoded with uint32 length prefix.
 */
static int
compute_exchange_hash(
    const char *v_c, size_t v_c_len,
    const char *v_s, size_t v_s_len,
    const uint8_t *i_c, size_t i_c_len,
    const uint8_t *i_s, size_t i_s_len,
    const uint8_t *k_s, size_t k_s_len,
    const uint8_t *q_c, size_t q_c_len,
    const uint8_t *q_s, size_t q_s_len,
    const uint8_t *k_mpint, size_t k_mpint_len,
    uint8_t *hash_out)
{
	EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
	if (mdctx == NULL)
		return DEUCE_SSH_ERROR_ALLOC;

	uint8_t lenbuf[4];
	int ok = 1;

	ok = ok && EVP_DigestInit_ex(mdctx, EVP_sha256(), NULL);

	{ size_t p = 0; deuce_ssh_serialize_uint32((uint32_t)v_c_len, lenbuf, 4, &p); }
	ok = ok && EVP_DigestUpdate(mdctx, lenbuf, 4);
	ok = ok && EVP_DigestUpdate(mdctx, v_c, v_c_len);

	{ size_t p = 0; deuce_ssh_serialize_uint32((uint32_t)v_s_len, lenbuf, 4, &p); }
	ok = ok && EVP_DigestUpdate(mdctx, lenbuf, 4);
	ok = ok && EVP_DigestUpdate(mdctx, v_s, v_s_len);

	{ size_t p = 0; deuce_ssh_serialize_uint32((uint32_t)i_c_len, lenbuf, 4, &p); }
	ok = ok && EVP_DigestUpdate(mdctx, lenbuf, 4);
	ok = ok && EVP_DigestUpdate(mdctx, i_c, i_c_len);

	{ size_t p = 0; deuce_ssh_serialize_uint32((uint32_t)i_s_len, lenbuf, 4, &p); }
	ok = ok && EVP_DigestUpdate(mdctx, lenbuf, 4);
	ok = ok && EVP_DigestUpdate(mdctx, i_s, i_s_len);

	{ size_t p = 0; deuce_ssh_serialize_uint32((uint32_t)k_s_len, lenbuf, 4, &p); }
	ok = ok && EVP_DigestUpdate(mdctx, lenbuf, 4);
	ok = ok && EVP_DigestUpdate(mdctx, k_s, k_s_len);

	{ size_t p = 0; deuce_ssh_serialize_uint32((uint32_t)q_c_len, lenbuf, 4, &p); }
	ok = ok && EVP_DigestUpdate(mdctx, lenbuf, 4);
	ok = ok && EVP_DigestUpdate(mdctx, q_c, q_c_len);

	{ size_t p = 0; deuce_ssh_serialize_uint32((uint32_t)q_s_len, lenbuf, 4, &p); }
	ok = ok && EVP_DigestUpdate(mdctx, lenbuf, 4);
	ok = ok && EVP_DigestUpdate(mdctx, q_s, q_s_len);

	ok = ok && EVP_DigestUpdate(mdctx, k_mpint, k_mpint_len);

	ok = ok && EVP_DigestFinal_ex(mdctx, hash_out, NULL);

	EVP_MD_CTX_free(mdctx);
	return ok ? 0 : DEUCE_SSH_ERROR_INIT;
}

/*
 * Perform X25519 key agreement: generate keypair, derive shared secret.
 * On success, sets *our_pub (32 bytes), *secret and *secret_len.
 * Caller must free *secret.
 */
static int
x25519_exchange(const uint8_t *peer_pub, size_t peer_pub_len,
    uint8_t *our_pub, uint8_t **secret, size_t *secret_len)
{
	EVP_PKEY *our_key = NULL;
	EVP_PKEY *peer_key = NULL;
	EVP_PKEY_CTX *pctx = NULL;
	int res;

	pctx = EVP_PKEY_CTX_new_id(EVP_PKEY_X25519, NULL);
	if (pctx == NULL)
		return DEUCE_SSH_ERROR_ALLOC;
	if (EVP_PKEY_keygen_init(pctx) != 1 ||
	    EVP_PKEY_keygen(pctx, &our_key) != 1) {
		EVP_PKEY_CTX_free(pctx);
		return DEUCE_SSH_ERROR_INIT;
	}
	EVP_PKEY_CTX_free(pctx);

	size_t pub_len = X25519_KEY_LEN;
	if (EVP_PKEY_get_raw_public_key(our_key, our_pub, &pub_len) != 1) {
		EVP_PKEY_free(our_key);
		return DEUCE_SSH_ERROR_INIT;
	}

	peer_key = EVP_PKEY_new_raw_public_key(EVP_PKEY_X25519, NULL, peer_pub, peer_pub_len);
	if (peer_key == NULL) {
		EVP_PKEY_free(our_key);
		return DEUCE_SSH_ERROR_INIT;
	}

	pctx = EVP_PKEY_CTX_new(our_key, NULL);
	if (pctx == NULL || EVP_PKEY_derive_init(pctx) != 1 ||
	    EVP_PKEY_derive_set_peer(pctx, peer_key) != 1) {
		res = DEUCE_SSH_ERROR_INIT;
		goto done;
	}

	*secret_len = 0;
	if (EVP_PKEY_derive(pctx, NULL, secret_len) != 1) {
		res = DEUCE_SSH_ERROR_INIT;
		goto done;
	}
	*secret = malloc(*secret_len);
	if (*secret == NULL) {
		res = DEUCE_SSH_ERROR_ALLOC;
		goto done;
	}
	if (EVP_PKEY_derive(pctx, *secret, secret_len) != 1) {
		OPENSSL_cleanse(*secret, *secret_len);
		free(*secret);
		*secret = NULL;
		res = DEUCE_SSH_ERROR_INIT;
		goto done;
	}
	res = 0;

done:
	EVP_PKEY_free(our_key);
	EVP_PKEY_free(peer_key);
	EVP_PKEY_CTX_free(pctx);
	return res;
}

/*
 * Encode raw shared secret bytes as SSH mpint, stripping leading zeros.
 * Caller must free *mpint_out.
 */
static int
encode_shared_secret(uint8_t *raw, size_t raw_len,
    uint8_t **ss_out, size_t *ss_len,
    uint8_t **mpint_out, size_t *mpint_len)
{
	uint8_t *start = raw;
	while (raw_len > 1 && start[0] == 0) {
		start++;
		raw_len--;
	}
	*ss_out = malloc(raw_len);
	if (*ss_out == NULL)
		return DEUCE_SSH_ERROR_ALLOC;
	memcpy(*ss_out, start, raw_len);
	*ss_len = raw_len;

	bool need_pad = (start[0] & 0x80) != 0;
	size_t mpint_data_len = raw_len + (need_pad ? 1 : 0);
	*mpint_len = 4 + mpint_data_len;
	*mpint_out = malloc(*mpint_len);
	if (*mpint_out == NULL) {
		free(*ss_out);
		*ss_out = NULL;
		return DEUCE_SSH_ERROR_ALLOC;
	}
	size_t kp = 0;
	deuce_ssh_serialize_uint32((uint32_t)mpint_data_len, *mpint_out, *mpint_len, &kp);
	if (need_pad)
		(*mpint_out)[kp++] = 0;
	memcpy(&(*mpint_out)[kp], start, raw_len);
	return 0;
}

static int
handler(deuce_ssh_session sess)
{
	int res;
	const char *v_c, *v_s;
	size_t v_c_len, v_s_len;
	const uint8_t *i_c, *i_s;
	size_t i_c_len, i_s_len;

	if (sess->trans.client) {
		v_c = sess->trans.id_str;        v_c_len = sess->trans.id_str_sz;
		v_s = sess->trans.remote_id_str; v_s_len = sess->trans.remote_id_str_sz;
		i_c = sess->trans.our_kexinit;   i_c_len = sess->trans.our_kexinit_sz;
		i_s = sess->trans.peer_kexinit;  i_s_len = sess->trans.peer_kexinit_sz;
	}
	else {
		v_c = sess->trans.remote_id_str; v_c_len = sess->trans.remote_id_str_sz;
		v_s = sess->trans.id_str;        v_s_len = sess->trans.id_str_sz;
		i_c = sess->trans.peer_kexinit;  i_c_len = sess->trans.peer_kexinit_sz;
		i_s = sess->trans.our_kexinit;   i_s_len = sess->trans.our_kexinit_sz;
	}

	uint8_t q_c[X25519_KEY_LEN], q_s[X25519_KEY_LEN];
	uint8_t *raw_secret = NULL;
	size_t raw_secret_len;
	uint8_t *ss_copy = NULL;
	size_t ss_len;
	uint8_t *k_mpint = NULL;
	size_t k_mpint_len;

	/* Get host key blob (server signs, client verifies) */
	uint8_t k_s_buf[1024];
	size_t k_s_len = 0;
	deuce_ssh_key_algo ka = sess->trans.key_algo_selected;

	if (sess->trans.client) {
		/* Client: send ECDH_INIT, receive ECDH_REPLY */
		uint8_t our_pub[X25519_KEY_LEN];

		/* Generate keypair and send Q_C */
		EVP_PKEY_CTX *pctx = EVP_PKEY_CTX_new_id(EVP_PKEY_X25519, NULL);
		EVP_PKEY *tmp_key = NULL;
		if (pctx == NULL)
			return DEUCE_SSH_ERROR_ALLOC;
		if (EVP_PKEY_keygen_init(pctx) != 1 ||
		    EVP_PKEY_keygen(pctx, &tmp_key) != 1) {
			EVP_PKEY_CTX_free(pctx);
			return DEUCE_SSH_ERROR_INIT;
		}
		EVP_PKEY_CTX_free(pctx);
		size_t pub_len = X25519_KEY_LEN;
		if (EVP_PKEY_get_raw_public_key(tmp_key, our_pub, &pub_len) != 1) {
			EVP_PKEY_free(tmp_key);
			return DEUCE_SSH_ERROR_INIT;
		}
		memcpy(q_c, our_pub, X25519_KEY_LEN);

		uint8_t init_msg[1 + 4 + X25519_KEY_LEN];
		size_t pos = 0;
		init_msg[pos++] = SSH_MSG_KEX_ECDH_INIT;
		deuce_ssh_serialize_uint32(X25519_KEY_LEN, init_msg, sizeof(init_msg), &pos);
		memcpy(&init_msg[pos], q_c, X25519_KEY_LEN);
		pos += X25519_KEY_LEN;
		res = deuce_ssh_transport_send_packet(sess, init_msg, pos, NULL);
		if (res < 0) {
			EVP_PKEY_free(tmp_key);
			return res;
		}

		/* Receive ECDH_REPLY */
		uint8_t msg_type;
		uint8_t *reply;
		size_t reply_len;
		res = deuce_ssh_transport_recv_packet(sess, &msg_type, &reply, &reply_len);
		if (res < 0) { EVP_PKEY_free(tmp_key); return res; }
		if (msg_type != SSH_MSG_KEX_ECDH_REPLY) { EVP_PKEY_free(tmp_key); return DEUCE_SSH_ERROR_PARSE; }

		/* Parse K_S, Q_S, sig */
		size_t rpos = 1;
		uint32_t ks_len;
		if (deuce_ssh_parse_uint32(&reply[rpos], reply_len - rpos, &ks_len) < 4 ||
		    rpos + 4 + ks_len > reply_len) { EVP_PKEY_free(tmp_key); return DEUCE_SSH_ERROR_PARSE; }
		rpos += 4;
		uint8_t *k_s = &reply[rpos];
		k_s_len = ks_len;
		rpos += ks_len;

		uint32_t qs_len;
		if (rpos + 4 > reply_len ||
		    deuce_ssh_parse_uint32(&reply[rpos], reply_len - rpos, &qs_len) < 4 ||
		    qs_len != X25519_KEY_LEN || rpos + 4 + qs_len > reply_len) { EVP_PKEY_free(tmp_key); return DEUCE_SSH_ERROR_PARSE; }
		rpos += 4;
		memcpy(q_s, &reply[rpos], X25519_KEY_LEN);
		rpos += qs_len;

		uint32_t sig_len;
		if (rpos + 4 > reply_len ||
		    deuce_ssh_parse_uint32(&reply[rpos], reply_len - rpos, &sig_len) < 4 ||
		    rpos + 4 + sig_len > reply_len) { EVP_PKEY_free(tmp_key); return DEUCE_SSH_ERROR_PARSE; }
		rpos += 4;
		uint8_t *sig_h = &reply[rpos];

		/* Compute shared secret using our private key + Q_S */
		EVP_PKEY *peer_key = EVP_PKEY_new_raw_public_key(EVP_PKEY_X25519, NULL, q_s, X25519_KEY_LEN);
		pctx = EVP_PKEY_CTX_new(tmp_key, NULL);
		if (!peer_key || !pctx || EVP_PKEY_derive_init(pctx) != 1 ||
		    EVP_PKEY_derive_set_peer(pctx, peer_key) != 1) {
			EVP_PKEY_free(tmp_key); EVP_PKEY_free(peer_key); EVP_PKEY_CTX_free(pctx);
			return DEUCE_SSH_ERROR_INIT;
		}
		raw_secret_len = 0;
		if (EVP_PKEY_derive(pctx, NULL, &raw_secret_len) != 1) {
			EVP_PKEY_free(tmp_key); EVP_PKEY_free(peer_key); EVP_PKEY_CTX_free(pctx);
			return DEUCE_SSH_ERROR_INIT;
		}
		raw_secret = malloc(raw_secret_len);
		if (!raw_secret || EVP_PKEY_derive(pctx, raw_secret, &raw_secret_len) != 1) {
			OPENSSL_cleanse(raw_secret, raw_secret_len);
			free(raw_secret);
			EVP_PKEY_free(tmp_key); EVP_PKEY_free(peer_key); EVP_PKEY_CTX_free(pctx);
			return DEUCE_SSH_ERROR_INIT;
		}
		EVP_PKEY_free(tmp_key);
		EVP_PKEY_free(peer_key);
		EVP_PKEY_CTX_free(pctx);

		res = encode_shared_secret(raw_secret, raw_secret_len,
		    &ss_copy, &ss_len, &k_mpint, &k_mpint_len);
		OPENSSL_cleanse(raw_secret, raw_secret_len);
		free(raw_secret);
		if (res < 0)
			return res;

		/* Compute exchange hash */
		uint8_t hash[SHA256_DIGEST_LEN];
		res = compute_exchange_hash(v_c, v_c_len, v_s, v_s_len,
		    i_c, i_c_len, i_s, i_s_len,
		    k_s, k_s_len, q_c, X25519_KEY_LEN, q_s, X25519_KEY_LEN,
		    k_mpint, k_mpint_len, hash);
		OPENSSL_cleanse(k_mpint, k_mpint_len);
		free(k_mpint);
		if (res < 0) { OPENSSL_cleanse(ss_copy, ss_len); free(ss_copy); return res; }

		/* Verify server signature */
		if (ka == NULL || ka->verify == NULL) { OPENSSL_cleanse(ss_copy, ss_len); free(ss_copy); return DEUCE_SSH_ERROR_INIT; }
		res = ka->verify(k_s, k_s_len, sig_h, sig_len, hash, SHA256_DIGEST_LEN);
		if (res < 0) { OPENSSL_cleanse(ss_copy, ss_len); free(ss_copy); return res; }

		/* Store results */
		sess->trans.shared_secret = ss_copy;
		sess->trans.shared_secret_sz = ss_len;
		sess->trans.exchange_hash = malloc(SHA256_DIGEST_LEN);
		if (sess->trans.exchange_hash == NULL) { OPENSSL_cleanse(ss_copy, ss_len); free(ss_copy); return DEUCE_SSH_ERROR_ALLOC; }
		memcpy(sess->trans.exchange_hash, hash, SHA256_DIGEST_LEN);
		sess->trans.exchange_hash_sz = SHA256_DIGEST_LEN;
	}
	else {
		/* Server: receive ECDH_INIT, send ECDH_REPLY */
		if (ka == NULL || ka->pubkey == NULL || ka->sign == NULL)
			return DEUCE_SSH_ERROR_INIT;

		/* Get our host key blob */
		res = ka->pubkey(k_s_buf, sizeof(k_s_buf), &k_s_len, ka->ctx);
		if (res < 0)
			return res;

		/* Receive ECDH_INIT(Q_C) */
		uint8_t msg_type;
		uint8_t *init_payload;
		size_t init_len;
		res = deuce_ssh_transport_recv_packet(sess, &msg_type, &init_payload, &init_len);
		if (res < 0)
			return res;
		if (msg_type != SSH_MSG_KEX_ECDH_INIT)
			return DEUCE_SSH_ERROR_PARSE;

		size_t rpos = 1;
		uint32_t qc_len;
		if (deuce_ssh_parse_uint32(&init_payload[rpos], init_len - rpos, &qc_len) < 4 ||
		    qc_len != X25519_KEY_LEN || rpos + 4 + qc_len > init_len)
			return DEUCE_SSH_ERROR_PARSE;
		rpos += 4;
		memcpy(q_c, &init_payload[rpos], X25519_KEY_LEN);

		/* Generate our ephemeral keypair and compute shared secret */
		res = x25519_exchange(q_c, X25519_KEY_LEN, q_s, &raw_secret, &raw_secret_len);
		if (res < 0)
			return res;

		res = encode_shared_secret(raw_secret, raw_secret_len,
		    &ss_copy, &ss_len, &k_mpint, &k_mpint_len);
		OPENSSL_cleanse(raw_secret, raw_secret_len);
		free(raw_secret);
		if (res < 0)
			return res;

		/* Compute exchange hash */
		uint8_t hash[SHA256_DIGEST_LEN];
		res = compute_exchange_hash(v_c, v_c_len, v_s, v_s_len,
		    i_c, i_c_len, i_s, i_s_len,
		    k_s_buf, k_s_len, q_c, X25519_KEY_LEN, q_s, X25519_KEY_LEN,
		    k_mpint, k_mpint_len, hash);
		OPENSSL_cleanse(k_mpint, k_mpint_len);
		free(k_mpint);
		if (res < 0) { OPENSSL_cleanse(ss_copy, ss_len); free(ss_copy); return res; }

		/* Sign exchange hash */
		uint8_t sig_buf[1024];
		size_t sig_len;
		res = ka->sign(sig_buf, sizeof(sig_buf), &sig_len,
		    hash, SHA256_DIGEST_LEN, ka->ctx);
		if (res < 0) { OPENSSL_cleanse(ss_copy, ss_len); free(ss_copy); return res; }

		/* Send ECDH_REPLY(K_S, Q_S, sig) */
		size_t reply_sz = 1 + 4 + k_s_len + 4 + X25519_KEY_LEN + 4 + sig_len;
		uint8_t *reply_msg = malloc(reply_sz);
		if (reply_msg == NULL) { OPENSSL_cleanse(ss_copy, ss_len); free(ss_copy); return DEUCE_SSH_ERROR_ALLOC; }
		size_t pos = 0;
		reply_msg[pos++] = SSH_MSG_KEX_ECDH_REPLY;
		deuce_ssh_serialize_uint32((uint32_t)k_s_len, reply_msg, reply_sz, &pos);
		memcpy(&reply_msg[pos], k_s_buf, k_s_len);
		pos += k_s_len;
		deuce_ssh_serialize_uint32(X25519_KEY_LEN, reply_msg, reply_sz, &pos);
		memcpy(&reply_msg[pos], q_s, X25519_KEY_LEN);
		pos += X25519_KEY_LEN;
		deuce_ssh_serialize_uint32((uint32_t)sig_len, reply_msg, reply_sz, &pos);
		memcpy(&reply_msg[pos], sig_buf, sig_len);
		pos += sig_len;
		res = deuce_ssh_transport_send_packet(sess, reply_msg, pos, NULL);
		free(reply_msg);
		if (res < 0) { OPENSSL_cleanse(ss_copy, ss_len); free(ss_copy); return res; }

		/* Store results */
		sess->trans.shared_secret = ss_copy;
		sess->trans.shared_secret_sz = ss_len;
		sess->trans.exchange_hash = malloc(SHA256_DIGEST_LEN);
		if (sess->trans.exchange_hash == NULL) { OPENSSL_cleanse(ss_copy, ss_len); free(ss_copy); return DEUCE_SSH_ERROR_ALLOC; }
		memcpy(sess->trans.exchange_hash, hash, SHA256_DIGEST_LEN);
		sess->trans.exchange_hash_sz = SHA256_DIGEST_LEN;
	}

	return 0;
}

static void
kex_cleanup(deuce_ssh_session sess)
{
	return;
}

DEUCE_SSH_PUBLIC int
register_curve25519_sha256(void)
{
	struct deuce_ssh_kex_s *kex = malloc(sizeof(*kex) + KEX_NAME_LEN + 1);
	if (kex == NULL)
		return DEUCE_SSH_ERROR_ALLOC;
	kex->next = NULL;
	kex->handler = handler;
	kex->cleanup = kex_cleanup;
	kex->flags = DEUCE_SSH_KEX_FLAG_NEEDS_SIGNATURE_CAPABLE;
	kex->hash_name = "SHA256";
	memcpy(kex->name, KEX_NAME, KEX_NAME_LEN + 1);
	return deuce_ssh_transport_register_kex(kex);
}
