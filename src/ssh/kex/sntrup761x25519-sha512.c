// draft-ietf-sshm-ntruprime-ssh: sntrup761x25519-sha512

#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <stdlib.h>
#include <string.h>

#include "deucessh.h"
#include "deucessh-kex.h"
#include "sntrup761.h"
#ifdef DSSH_TESTING
#include "ssh-internal.h"
#endif

#define KEX_NAME         "sntrup761x25519-sha512"
#define KEX_NAME_LEN     22
#define SHA512_DIGEST_LEN 64
#define X25519_KEY_LEN    32

#define SNTRUP761_PK_LEN  crypto_kem_sntrup761_PUBLICKEYBYTES   /* 1158 */
#define SNTRUP761_SK_LEN  crypto_kem_sntrup761_SECRETKEYBYTES   /* 1763 */
#define SNTRUP761_CT_LEN  crypto_kem_sntrup761_CIPHERTEXTBYTES  /* 1039 */
#define SNTRUP761_SS_LEN  crypto_kem_sntrup761_BYTES            /* 32 */

#define Q_C_LEN (SNTRUP761_PK_LEN + X25519_KEY_LEN)  /* 1190 */
#define Q_S_LEN (SNTRUP761_CT_LEN + X25519_KEY_LEN)  /* 1071 */

/*
 * Compute exchange hash:
 * H = SHA-512(V_C || V_S || I_C || I_S || K_S || Q_C || Q_S || K)
 *
 * All values are string-encoded (uint32 length prefix + data).
 * K is the 64-byte SHA-512 output of the combined shared secrets.
 */
DSSH_TESTABLE int
compute_exchange_hash_sntrup(
    const char *v_c, size_t v_c_len,
    const char *v_s, size_t v_s_len,
    const uint8_t *i_c, size_t i_c_len,
    const uint8_t *i_s, size_t i_s_len,
    const uint8_t *k_s, size_t k_s_len,
    const uint8_t *q_c, size_t q_c_len,
    const uint8_t *q_s, size_t q_s_len,
    const uint8_t *k, size_t k_len,
    uint8_t *hash_out)
{
	if (v_c_len > UINT32_MAX || v_s_len > UINT32_MAX
	    || i_c_len > UINT32_MAX || i_s_len > UINT32_MAX
	    || k_s_len > UINT32_MAX || q_c_len > UINT32_MAX
	    || q_s_len > UINT32_MAX || k_len > UINT32_MAX)
		return DSSH_ERROR_INVALID;

	EVP_MD_CTX *mdctx = EVP_MD_CTX_new();

	if (mdctx == NULL)
		return DSSH_ERROR_ALLOC;

	uint8_t lenbuf[4];
	size_t  lp;
	int     ok = EVP_DigestInit_ex(mdctx, EVP_sha512(), NULL);

#define HASH_U32(val, data, len) do { \
	lp = 0; \
	if (dssh_serialize_uint32((val), lenbuf, 4, &lp) < 0) { ok = 0; break; } \
	ok = ok && EVP_DigestUpdate(mdctx, lenbuf, 4); \
	ok = ok && EVP_DigestUpdate(mdctx, (data), (len)); \
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

	ok = ok && EVP_DigestFinal_ex(mdctx, hash_out, NULL);

	EVP_MD_CTX_free(mdctx);
	return ok ? 0 : DSSH_ERROR_INIT;
}

/*
 * Compute shared secret K = SHA-512(ss_pq || ss_cl).
 * Returns malloc'd 64-byte buffer in *k_out, caller frees.
 */
static int
compute_shared_secret(const uint8_t *ss_pq, const uint8_t *ss_cl,
    uint8_t **k_out, size_t *k_len)
{
	uint8_t combined[SNTRUP761_SS_LEN + X25519_KEY_LEN]; /* 64 */

	memcpy(combined, ss_pq, SNTRUP761_SS_LEN);
	memcpy(combined + SNTRUP761_SS_LEN, ss_cl, X25519_KEY_LEN);

	uint8_t *k = malloc(SHA512_DIGEST_LEN);

	if (k == NULL) {
		OPENSSL_cleanse(combined, sizeof(combined));
		return DSSH_ERROR_ALLOC;
	}

	unsigned int olen = 0;
	int ok = EVP_Digest(combined, sizeof(combined), k, &olen,
	    EVP_sha512(), NULL);

	OPENSSL_cleanse(combined, sizeof(combined));

	if (ok != 1) {
		free(k);
		return DSSH_ERROR_INIT;
	}
	*k_out = k;
	*k_len = SHA512_DIGEST_LEN;
	return 0;
}

/*
 * Perform X25519 key exchange: generate our keypair, derive shared secret.
 * On success, q_out receives our 32-byte public key, ss_out the 32-byte secret.
 */
static int
x25519_exchange(const uint8_t *peer_pk, size_t peer_pk_len,
    uint8_t *q_out, uint8_t *ss_out)
{
	EVP_PKEY_CTX *pctx = EVP_PKEY_CTX_new_from_name(NULL, "X25519", NULL);

	if (pctx == NULL)
		return DSSH_ERROR_ALLOC;
	EVP_PKEY *our_key = NULL;

	if ((EVP_PKEY_keygen_init(pctx) != 1)
	    || (EVP_PKEY_keygen(pctx, &our_key) != 1)) {
		EVP_PKEY_CTX_free(pctx);
		return DSSH_ERROR_INIT;
	}
	EVP_PKEY_CTX_free(pctx);

	/* Extract our public key */
	size_t pub_len = X25519_KEY_LEN;

	if (EVP_PKEY_get_raw_public_key(our_key, q_out, &pub_len) != 1) {
		EVP_PKEY_free(our_key);
		return DSSH_ERROR_INIT;
	}

	/* Create peer key from raw bytes */
	EVP_PKEY *peer_key = EVP_PKEY_new_raw_public_key_ex(
	    NULL, "X25519", NULL, peer_pk, peer_pk_len);

	if (peer_key == NULL) {
		EVP_PKEY_free(our_key);
		return DSSH_ERROR_INIT;
	}

	/* Derive shared secret */
	EVP_PKEY_CTX *dctx = EVP_PKEY_CTX_new(our_key, NULL);

	if (dctx == NULL) {
		EVP_PKEY_free(our_key);
		EVP_PKEY_free(peer_key);
		return DSSH_ERROR_ALLOC;
	}
	size_t ss_len = X25519_KEY_LEN;

	if ((EVP_PKEY_derive_init(dctx) != 1)
	    || (EVP_PKEY_derive_set_peer(dctx, peer_key) != 1)
	    || (EVP_PKEY_derive(dctx, ss_out, &ss_len) != 1)) {
		EVP_PKEY_CTX_free(dctx);
		EVP_PKEY_free(our_key);
		EVP_PKEY_free(peer_key);
		return DSSH_ERROR_INIT;
	}

	EVP_PKEY_CTX_free(dctx);
	EVP_PKEY_free(our_key);
	EVP_PKEY_free(peer_key);
	return 0;
}

DSSH_TESTABLE int
sntrup761x25519_handler(struct dssh_kex_context *kctx)
{
	dssh_key_algo ka = kctx->key_algo;
	int res;

	if (kctx->client) {
		/* ============================================================
		 * CLIENT
		 * ============================================================ */

		/* 1. Generate sntrup761 keypair */
		unsigned char sntrup_pk[SNTRUP761_PK_LEN];
		unsigned char sntrup_sk[SNTRUP761_SK_LEN];

		if (crypto_kem_sntrup761_keypair(sntrup_pk, sntrup_sk) != 0) {
			OPENSSL_cleanse(sntrup_sk, sizeof(sntrup_sk));
			return DSSH_ERROR_INIT;
		}

		/* 2. Generate X25519 ephemeral keypair */
		uint8_t x25519_pk[X25519_KEY_LEN];
		uint8_t x25519_ss[X25519_KEY_LEN];

		/* We don't have peer yet -- generate keypair only.
		 * Use EVP_PKEY_keygen + store for later derive. */
		EVP_PKEY_CTX *pctx = EVP_PKEY_CTX_new_from_name(NULL, "X25519", NULL);

		if (pctx == NULL) {
			OPENSSL_cleanse(sntrup_sk, sizeof(sntrup_sk));
			return DSSH_ERROR_ALLOC;
		}

		EVP_PKEY *x_key = NULL;

		if ((EVP_PKEY_keygen_init(pctx) != 1)
		    || (EVP_PKEY_keygen(pctx, &x_key) != 1)) {
			EVP_PKEY_CTX_free(pctx);
			OPENSSL_cleanse(sntrup_sk, sizeof(sntrup_sk));
			return DSSH_ERROR_INIT;
		}
		EVP_PKEY_CTX_free(pctx);

		size_t pub_len = X25519_KEY_LEN;

		if (EVP_PKEY_get_raw_public_key(x_key, x25519_pk, &pub_len) != 1) {
			EVP_PKEY_free(x_key);
			OPENSSL_cleanse(sntrup_sk, sizeof(sntrup_sk));
			return DSSH_ERROR_INIT;
		}

		/* 3. Send INIT: Q_C = sntrup_pk || x25519_pk */
		uint8_t init_msg[1 + 4 + Q_C_LEN];
		size_t  pos = 0;

		init_msg[pos++] = SSH_MSG_KEX_ECDH_INIT;
		res = dssh_serialize_uint32(Q_C_LEN, init_msg, sizeof(init_msg), &pos);
		if (res < 0) {
			EVP_PKEY_free(x_key);
			OPENSSL_cleanse(sntrup_sk, sizeof(sntrup_sk));
			return res;
		}
		memcpy(&init_msg[pos], sntrup_pk, SNTRUP761_PK_LEN);
		pos += SNTRUP761_PK_LEN;
		memcpy(&init_msg[pos], x25519_pk, X25519_KEY_LEN);
		pos += X25519_KEY_LEN;

		res = kctx->send(init_msg, pos, kctx->io_ctx);
		if (res < 0) {
			EVP_PKEY_free(x_key);
			OPENSSL_cleanse(sntrup_sk, sizeof(sntrup_sk));
			return res;
		}

		/* 4. Receive REPLY */
		uint8_t  msg_type;
		uint8_t *reply;
		size_t   reply_len;

		res = kctx->recv(&msg_type, &reply, &reply_len, kctx->io_ctx);
		if (res < 0) {
			EVP_PKEY_free(x_key);
			OPENSSL_cleanse(sntrup_sk, sizeof(sntrup_sk));
			return res;
		}
		if (msg_type != SSH_MSG_KEX_ECDH_REPLY) {
			EVP_PKEY_free(x_key);
			OPENSSL_cleanse(sntrup_sk, sizeof(sntrup_sk));
			return DSSH_ERROR_PARSE;
		}

		/* Parse K_S */
		size_t   rpos = 1; /* skip msg_type byte */
		uint32_t ks_len;

		if (dssh_parse_uint32(&reply[rpos], reply_len - rpos, &ks_len) < 4) {
			EVP_PKEY_free(x_key);
			OPENSSL_cleanse(sntrup_sk, sizeof(sntrup_sk));
			return DSSH_ERROR_PARSE;
		}
		rpos += 4;
		if (rpos + ks_len > reply_len) {
			EVP_PKEY_free(x_key);
			OPENSSL_cleanse(sntrup_sk, sizeof(sntrup_sk));
			return DSSH_ERROR_PARSE;
		}
		const uint8_t *k_s = &reply[rpos];
		size_t         k_s_len = ks_len;

		rpos += ks_len;

		/* Parse Q_S */
		uint32_t qs_len;

		if (dssh_parse_uint32(&reply[rpos], reply_len - rpos, &qs_len) < 4) {
			EVP_PKEY_free(x_key);
			OPENSSL_cleanse(sntrup_sk, sizeof(sntrup_sk));
			return DSSH_ERROR_PARSE;
		}
		if (qs_len != Q_S_LEN) {
			EVP_PKEY_free(x_key);
			OPENSSL_cleanse(sntrup_sk, sizeof(sntrup_sk));
			return DSSH_ERROR_PARSE;
		}
		rpos += 4;
		if (rpos + qs_len > reply_len) {
			EVP_PKEY_free(x_key);
			OPENSSL_cleanse(sntrup_sk, sizeof(sntrup_sk));
			return DSSH_ERROR_PARSE;
		}
		const uint8_t *sntrup_ct = &reply[rpos];
		const uint8_t *peer_x25519_pk = &reply[rpos + SNTRUP761_CT_LEN];

		rpos += qs_len;

		/* Parse signature */
		uint32_t sig_len;

		if (dssh_parse_uint32(&reply[rpos], reply_len - rpos, &sig_len) < 4) {
			EVP_PKEY_free(x_key);
			OPENSSL_cleanse(sntrup_sk, sizeof(sntrup_sk));
			return DSSH_ERROR_PARSE;
		}
		rpos += 4;
		if (rpos + sig_len > reply_len) {
			EVP_PKEY_free(x_key);
			OPENSSL_cleanse(sntrup_sk, sizeof(sntrup_sk));
			return DSSH_ERROR_PARSE;
		}
		const uint8_t *sig_h = &reply[rpos];

		/* 5. Decapsulate sntrup761 */
		uint8_t ss_pq[SNTRUP761_SS_LEN];

		if (crypto_kem_sntrup761_dec(ss_pq, sntrup_ct, sntrup_sk) != 0) {
			EVP_PKEY_free(x_key);
			OPENSSL_cleanse(sntrup_sk, sizeof(sntrup_sk));
			OPENSSL_cleanse(ss_pq, sizeof(ss_pq));
			return DSSH_ERROR_INIT;
		}
		OPENSSL_cleanse(sntrup_sk, sizeof(sntrup_sk));

		/* 6. X25519 derive */
		EVP_PKEY *peer_key = EVP_PKEY_new_raw_public_key_ex(
		    NULL, "X25519", NULL, peer_x25519_pk, X25519_KEY_LEN);

		if (peer_key == NULL) {
			OPENSSL_cleanse(ss_pq, sizeof(ss_pq));
			EVP_PKEY_free(x_key);
			return DSSH_ERROR_INIT;
		}

		EVP_PKEY_CTX *dctx = EVP_PKEY_CTX_new(x_key, NULL);

		if (dctx == NULL) {
			OPENSSL_cleanse(ss_pq, sizeof(ss_pq));
			EVP_PKEY_free(x_key);
			EVP_PKEY_free(peer_key);
			return DSSH_ERROR_ALLOC;
		}

		size_t ss_cl_len = X25519_KEY_LEN;

		if ((EVP_PKEY_derive_init(dctx) != 1)
		    || (EVP_PKEY_derive_set_peer(dctx, peer_key) != 1)
		    || (EVP_PKEY_derive(dctx, x25519_ss, &ss_cl_len) != 1)) {
			EVP_PKEY_CTX_free(dctx);
			EVP_PKEY_free(x_key);
			EVP_PKEY_free(peer_key);
			OPENSSL_cleanse(ss_pq, sizeof(ss_pq));
			OPENSSL_cleanse(x25519_ss, sizeof(x25519_ss));
			return DSSH_ERROR_INIT;
		}
		EVP_PKEY_CTX_free(dctx);
		EVP_PKEY_free(x_key);
		EVP_PKEY_free(peer_key);

		/* 7. K = SHA-512(ss_pq || ss_cl) */
		uint8_t *K;
		size_t   K_len;

		res = compute_shared_secret(ss_pq, x25519_ss, &K, &K_len);
		OPENSSL_cleanse(ss_pq, sizeof(ss_pq));
		OPENSSL_cleanse(x25519_ss, sizeof(x25519_ss));
		if (res < 0)
			return res;

		/* 8. Exchange hash */
		uint8_t hash[SHA512_DIGEST_LEN];

		/* Q_C as sent (without length prefix) */
		uint8_t q_c_raw[Q_C_LEN];

		memcpy(q_c_raw, sntrup_pk, SNTRUP761_PK_LEN);
		memcpy(q_c_raw + SNTRUP761_PK_LEN, x25519_pk, X25519_KEY_LEN);

		/* sntrup_ct points to start of Q_S data (ct || x25519_pk) */
		res = compute_exchange_hash_sntrup(
		    kctx->v_c, kctx->v_c_len,
		    kctx->v_s, kctx->v_s_len,
		    kctx->i_c, kctx->i_c_len,
		    kctx->i_s, kctx->i_s_len,
		    k_s, k_s_len,
		    q_c_raw, Q_C_LEN,
		    sntrup_ct, Q_S_LEN,
		    K, K_len,
		    hash);
		if (res < 0) {
			OPENSSL_cleanse(K, K_len);
			free(K);
			return res;
		}

		/* 9. Verify signature */
		res = ka->verify(k_s, k_s_len, sig_h, sig_len,
		    hash, SHA512_DIGEST_LEN);
		if (res < 0) {
			OPENSSL_cleanse(K, K_len);
			free(K);
			return res;
		}

		/* 10. Store results -- K is the raw shared secret (string, not mpint) */
		kctx->shared_secret = K;
		kctx->shared_secret_sz = K_len;

		kctx->exchange_hash = malloc(SHA512_DIGEST_LEN);
		if (kctx->exchange_hash == NULL) {
			OPENSSL_cleanse(K, K_len);
			free(K);
			kctx->shared_secret = NULL;
			kctx->shared_secret_sz = 0;
			return DSSH_ERROR_ALLOC;
		}
		memcpy(kctx->exchange_hash, hash, SHA512_DIGEST_LEN);
		kctx->exchange_hash_sz = SHA512_DIGEST_LEN;
		return 0;
	}
	else {
		/* ============================================================
		 * SERVER
		 * ============================================================ */

		/* 1. Get host key blob */
		const uint8_t *k_s_buf = NULL;
		size_t  k_s_len;

		res = ka->pubkey(&k_s_buf, &k_s_len, ka->ctx);
		if (res < 0)
			return res;

		/* 2. Receive INIT */
		uint8_t  msg_type;
		uint8_t *payload;
		size_t   payload_len;

		res = kctx->recv(&msg_type, &payload, &payload_len, kctx->io_ctx);
		if (res < 0) {
			return res;
		}
		if (msg_type != SSH_MSG_KEX_ECDH_INIT) {
			return DSSH_ERROR_PARSE;
		}

		/* 3. Parse Q_C */
		size_t   rpos = 1; /* skip msg_type byte */
		uint32_t qc_len;

		if (dssh_parse_uint32(&payload[rpos], payload_len - rpos, &qc_len) < 4) {
			return DSSH_ERROR_PARSE;
		}
		if (qc_len != Q_C_LEN) {
			return DSSH_ERROR_PARSE;
		}
		rpos += 4;
		if (rpos + qc_len > payload_len)
			return DSSH_ERROR_PARSE;

		const uint8_t *client_sntrup_pk = &payload[rpos];
		const uint8_t *client_x25519_pk = &payload[rpos + SNTRUP761_PK_LEN];
		const uint8_t *q_c_raw = &payload[rpos];

		/* 4. Encapsulate sntrup761 */
		uint8_t sntrup_ct[SNTRUP761_CT_LEN];
		uint8_t ss_pq[SNTRUP761_SS_LEN];

		if (crypto_kem_sntrup761_enc(sntrup_ct, ss_pq, client_sntrup_pk) != 0) {
			OPENSSL_cleanse(ss_pq, sizeof(ss_pq));
			return DSSH_ERROR_INIT;
		}

		/* 5. X25519 exchange */
		uint8_t x25519_pk[X25519_KEY_LEN];
		uint8_t x25519_ss[X25519_KEY_LEN];

		res = x25519_exchange(client_x25519_pk, X25519_KEY_LEN,
		    x25519_pk, x25519_ss);
		if (res < 0) {
			OPENSSL_cleanse(ss_pq, sizeof(ss_pq));
			return res;
		}

		/* 6. K = SHA-512(ss_pq || ss_cl) */
		uint8_t *K;
		size_t   K_len;

		res = compute_shared_secret(ss_pq, x25519_ss, &K, &K_len);
		OPENSSL_cleanse(ss_pq, sizeof(ss_pq));
		OPENSSL_cleanse(x25519_ss, sizeof(x25519_ss));
		if (res < 0)
			return res;

		/* 7. Q_S = ct || x25519_pk */
		uint8_t q_s_raw[Q_S_LEN];

		memcpy(q_s_raw, sntrup_ct, SNTRUP761_CT_LEN);
		memcpy(q_s_raw + SNTRUP761_CT_LEN, x25519_pk, X25519_KEY_LEN);

		/* 8. Exchange hash */
		uint8_t hash[SHA512_DIGEST_LEN];

		res = compute_exchange_hash_sntrup(
		    kctx->v_c, kctx->v_c_len,
		    kctx->v_s, kctx->v_s_len,
		    kctx->i_c, kctx->i_c_len,
		    kctx->i_s, kctx->i_s_len,
		    k_s_buf, k_s_len,
		    q_c_raw, Q_C_LEN,
		    q_s_raw, Q_S_LEN,
		    K, K_len,
		    hash);
		if (res < 0) {
			OPENSSL_cleanse(K, K_len);
			free(K);
			return res;
		}

		/* 9. Sign exchange hash */
		uint8_t *sig_buf = NULL;
		size_t  sig_len;

		res = ka->sign(&sig_buf, &sig_len,
		    hash, SHA512_DIGEST_LEN, ka->ctx);
		if (res < 0) {
			OPENSSL_cleanse(K, K_len);
			free(K);
			return res;
		}

		/* 10. Send REPLY */
		/* k_s_len <= 1024, sig_len <= 1024; sum <= ~3200 */
		size_t   reply_sz = 1 + 4 + k_s_len + 4 + Q_S_LEN + 4 + sig_len;
		uint8_t *reply_msg = malloc(reply_sz);

		if (reply_msg == NULL) {
			free(sig_buf);
			OPENSSL_cleanse(K, K_len);
			free(K);
			return DSSH_ERROR_ALLOC;
		}

		size_t rp = 0;

		reply_msg[rp++] = SSH_MSG_KEX_ECDH_REPLY;

#define REPLY_SER(v) do { res = dssh_serialize_uint32((v), reply_msg, reply_sz, &rp); \
	if (res < 0) { free(reply_msg); free(sig_buf); OPENSSL_cleanse(K, K_len); free(K); return res; } } while (0)

		REPLY_SER((uint32_t)k_s_len);
		memcpy(&reply_msg[rp], k_s_buf, k_s_len);
		rp += k_s_len;
		REPLY_SER(Q_S_LEN);
		memcpy(&reply_msg[rp], q_s_raw, Q_S_LEN);
		rp += Q_S_LEN;
		REPLY_SER((uint32_t)sig_len);
		memcpy(&reply_msg[rp], sig_buf, sig_len);
		rp += sig_len;
		free(sig_buf);

#undef REPLY_SER

		res = kctx->send(reply_msg, rp, kctx->io_ctx);
		free(reply_msg);
		if (res < 0) {
			OPENSSL_cleanse(K, K_len);
			free(K);
			return res;
		}

		/* Store results */
		kctx->shared_secret = K;
		kctx->shared_secret_sz = K_len;

		kctx->exchange_hash = malloc(SHA512_DIGEST_LEN);
		if (kctx->exchange_hash == NULL) {
			OPENSSL_cleanse(K, K_len);
			free(K);
			kctx->shared_secret = NULL;
			kctx->shared_secret_sz = 0;
			return DSSH_ERROR_ALLOC;
		}
		memcpy(kctx->exchange_hash, hash, SHA512_DIGEST_LEN);
		kctx->exchange_hash_sz = SHA512_DIGEST_LEN;
		return 0;
	}
}

static void
kex_cleanup(void *kex_data)
{
	(void)kex_data;
}

static int
register_kex(const char *name, size_t name_len)
{
	struct dssh_kex_s *kex = malloc(sizeof(*kex) + name_len + 1);

	if (kex == NULL)
		return DSSH_ERROR_ALLOC;
	kex->next = NULL;
	kex->handler = sntrup761x25519_handler;
	kex->cleanup = kex_cleanup;
	kex->flags = DSSH_KEX_FLAG_NEEDS_SIGNATURE_CAPABLE
	    | DSSH_KEX_FLAG_K_ENCODING_STRING;
	kex->hash_name = "SHA512";
	memcpy(kex->name, name, name_len + 1);
	return dssh_transport_register_kex(kex);
}

DSSH_PUBLIC int
dssh_register_sntrup761x25519_sha512(void)
{
	return register_kex(KEX_NAME, KEX_NAME_LEN);
}
