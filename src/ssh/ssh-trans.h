// RFC-4253

#ifndef DSSH_TRANS_H
#define DSSH_TRANS_H

#include <stdatomic.h>
#include <threads.h>
#include <time.h>

#include "deucessh.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Opaque context types for algorithm modules.
 * Each module defines the actual struct contents. */
typedef struct dssh_enc_ctx dssh_enc_ctx;
typedef struct dssh_mac_ctx dssh_mac_ctx;
typedef struct dssh_key_algo_ctx dssh_key_algo_ctx;
typedef struct dssh_comp_ctx dssh_comp_ctx;

/* Transport layer generic */
#define SSH_MSG_DISCONNECT UINT8_C(1)
#define SSH_MSG_IGNORE UINT8_C(2)
#define SSH_MSG_UNIMPLEMENTED UINT8_C(3)
#define SSH_MSG_DEBUG UINT8_C(4)
#define SSH_MSG_SERVICE_REQUEST UINT8_C(5)
#define SSH_MSG_SERVICE_ACCEPT UINT8_C(6)

/* Transport layer Algorithm negotiation */
#define SSH_MSG_KEXINIT UINT8_C(20)
#define SSH_MSG_NEWKEYS UINT8_C(21)

/* KEX method-specific (ECDH, DH) */
#define SSH_MSG_KEX_ECDH_INIT UINT8_C(30)
#define SSH_MSG_KEX_ECDH_REPLY UINT8_C(31)

/* SSH_MSG_DISCONNECT reason codes */
#define SSH_DISCONNECT_HOST_NOT_ALLOWED_TO_CONNECT UINT32_C(1)
#define SSH_DISCONNECT_PROTOCOL_ERROR UINT32_C(2)
#define SSH_DISCONNECT_KEY_EXCHANGE_FAILED UINT32_C(3)
#define SSH_DISCONNECT_RESERVED UINT32_C(4)
#define SSH_DISCONNECT_MAC_ERROR UINT32_C(5)
#define SSH_DISCONNECT_COMPRESSION_ERROR UINT32_C(6)
#define SSH_DISCONNECT_SERVICE_NOT_AVAILABLE UINT32_C(7)
#define SSH_DISCONNECT_PROTOCOL_VERSION_NOT_SUPPORTED UINT32_C(8)
#define SSH_DISCONNECT_HOST_KEY_NOT_VERIFIABLE UINT32_C(9)
#define SSH_DISCONNECT_CONNECTION_LOST UINT32_C(10)
#define SSH_DISCONNECT_BY_APPLICATION UINT32_C(11)
#define SSH_DISCONNECT_TOO_MANY_CONNECTIONS UINT32_C(12)
#define SSH_DISCONNECT_AUTH_CANCELLED_BY_USER UINT32_C(13)
#define SSH_DISCONNECT_NO_MORE_AUTH_METHODS_AVAILABLE UINT32_C(14)
#define SSH_DISCONNECT_ILLEGAL_USER_NAME UINT32_C(15)

#define DSSH_KEX_FLAG_NEEDS_ENCRYPTION_CAPABLE UINT32_C(1 << 0)
#define DSSH_KEX_FLAG_NEEDS_SIGNATURE_CAPABLE UINT32_C(1 << 1)

#define DSSH_KEY_ALGO_FLAG_ENCRYPTION_CAPABLE UINT32_C(1 << 0)
#define DSSH_KEY_ALGO_FLAG_SIGNATURE_CAPABLE UINT32_C(1 << 1)

/*
 * RFC 4253 s6.1: implementations MUST be able to process packets with
 * an uncompressed payload length of 32768 bytes or less.
 * Total packet = 4 (length) + 1 (padding_length) + payload + padding.
 * With max padding of 255 bytes, minimum buffer = 32768 + 260 = 33028.
 * We round up for alignment.
 */
#define SSH_BPP_PACKET_SIZE_MIN 33280
#define SSH_BPP_PACKET_SIZE_MAX (64 * 1024 * 1024) /* 64 MiB */

typedef struct dssh_transport_packet_s {
	dssh_string   payload;
	dssh_string   random_padding;
	dssh_string   mac;
	dssh_uint32_t packet_length;
	dssh_byte     padding;
} *dssh_transport_packet;

/*
 * KEX handler: drives the entire key exchange after KEXINIT
 * negotiation.  Uses send_packet/recv_packet internally.
 * Must populate sess->trans.shared_secret, exchange_hash,
 * and their sizes.
 */
typedef int (*dssh_kex_handler)(dssh_session sess);
typedef void (*dssh_kex_cleanup)(dssh_session sess);

typedef int (*dssh_key_algo_sign)(uint8_t *buf, size_t bufsz, size_t *outlen,
    const uint8_t *data, size_t data_len, dssh_key_algo_ctx *ctx);
typedef int (*dssh_key_algo_verify)(const uint8_t *key_blob, size_t key_blob_len,
    const uint8_t *sig_blob, size_t sig_blob_len,
    const uint8_t *data, size_t data_len);
typedef int (*dssh_key_algo_pubkey)(uint8_t *buf, size_t bufsz, size_t *outlen, dssh_key_algo_ctx *ctx);
typedef int (*dssh_key_algo_haskey)(dssh_key_algo_ctx *ctx);
typedef void (*dssh_key_algo_cleanup)(dssh_key_algo_ctx *ctx);

typedef int (*dssh_enc_init)(const uint8_t *key, const uint8_t *iv, bool encrypt, dssh_enc_ctx **ctx);
typedef int (*dssh_enc_crypt)(uint8_t *buf, size_t bufsz, dssh_enc_ctx *ctx);
typedef void (*dssh_enc_cleanup)(dssh_enc_ctx *ctx);

typedef int (*dssh_mac_init)(const uint8_t *key, dssh_mac_ctx **ctx);
typedef int (*dssh_mac_generate)(const uint8_t *buf, size_t bufsz, uint8_t *outbuf, dssh_mac_ctx *ctx);
typedef void (*dssh_mac_cleanup)(dssh_mac_ctx *ctx);

typedef int (*dssh_comp_compress)(uint8_t *buf, size_t *bufsz, dssh_comp_ctx *ctx);
typedef int (*dssh_comp_uncompress)(uint8_t *buf, size_t *bufsz, dssh_comp_ctx *ctx);
typedef void (*dssh_comp_cleanup)(dssh_comp_ctx *ctx);

typedef struct dssh_kex_s {
	struct dssh_kex_s *next;
	dssh_kex_handler   handler;
	dssh_kex_cleanup   cleanup;
	uint32_t           flags;
	const char        *hash_name; /* OpenSSL digest name, e.g. "SHA256" */
	char               name[];
} *dssh_kex;

typedef struct dssh_key_algo_s {
	struct dssh_key_algo_s *next;
	dssh_key_algo_sign      sign;
	dssh_key_algo_verify    verify;
	dssh_key_algo_pubkey    pubkey;
	dssh_key_algo_haskey    haskey;
	dssh_key_algo_cleanup   cleanup;
	dssh_key_algo_ctx      *ctx;
	uint32_t                flags;
	char                    name[];
} *dssh_key_algo;

typedef struct dssh_enc_s {
	struct dssh_enc_s *next;
	dssh_enc_init      init;
	dssh_enc_crypt     encrypt;
	dssh_enc_crypt     decrypt;
	dssh_enc_cleanup   cleanup;
	uint32_t           flags;
	uint16_t           blocksize;
	uint16_t           key_size;
	char               name[];
} *dssh_enc;

typedef struct dssh_mac_s {
	struct dssh_mac_s *next;
	dssh_mac_init      init;
	dssh_mac_generate  generate;
	dssh_mac_cleanup   cleanup;
	uint16_t           digest_size;
	uint16_t           key_size;
	char               name[];
} *dssh_mac;

typedef struct dssh_comp_s {
	struct dssh_comp_s  *next;
	dssh_comp_compress   compress;
	dssh_comp_uncompress uncompress;
	dssh_comp_cleanup    cleanup;
	char                 name[];
} *dssh_comp;

typedef struct dssh_language_s {
	struct dssh_language_s *next;
	char                    name[];
} *dssh_language;

/* Rekey thresholds (RFC 4253 s9, RFC 4251 s9.3.2) */
#define DSSH_REKEY_SOFT_LIMIT UINT32_C(0x10000000) /* 2^28 packets */
#define DSSH_REKEY_HARD_LIMIT UINT32_C(0x80000000) /* 2^31 packets */
#define DSSH_REKEY_BYTES UINT64_C(0x40000000)      /* 1 GiB */
#define DSSH_REKEY_SECONDS 3600                    /* 1 hour */

typedef struct dssh_transport_state_s {
	uint32_t       tx_seq;
	uint32_t       rx_seq;
	uint32_t       tx_since_rekey;    /* packets sent since last (re)key */
	uint32_t       rx_since_rekey;    /* packets received since last (re)key */
	uint64_t       bytes_since_rekey; /* bytes sent+received since last (re)key */
	time_t         rekey_time;        /* time of last (re)key */
	uint32_t       last_rx_seq;       /* seq number of last received packet */
	bool           client;
	bool           rekey_in_progress; /* true between KEXINIT and NEWKEYS */
	bool           rekey_pending;     /* deferred auto-rekey (set in recv_packet) */
	cnd_t          rekey_cnd;         /* wakes senders blocked during rekey */

        /* Packet buffers (dynamically allocated) */
	size_t         packet_buf_sz;
	uint8_t       *tx_packet;
	uint8_t       *rx_packet;
	uint8_t       *tx_mac_scratch; /* 4 + packet_buf_sz for MAC computation */
	uint8_t       *rx_mac_scratch;

        /* Concurrency: separate mutexes for send and receive paths */
	mtx_t          tx_mtx;
	mtx_t          rx_mtx;

        /* Transport options */
	size_t         id_str_sz;
	char           id_str[254];

        /* Remote peer information (from version exchange) */
	size_t         remote_id_str_sz;
	char           remote_id_str[254];
	char         **remote_languages;

        /* KEX options */
	void          *kex_ctx;
	dssh_kex       kex_selected;

        /* KEX outputs */
	size_t         shared_secret_sz;
	uint8_t       *shared_secret;
	size_t         exchange_hash_sz;
	uint8_t       *exchange_hash;

        /* Session ID (first exchange hash, persists across rekeys) */
	size_t         session_id_sz;
	uint8_t       *session_id;

        /* Saved KEXINIT payloads for exchange hash computation */
	size_t         our_kexinit_sz;
	uint8_t       *our_kexinit;
	size_t         peer_kexinit_sz;
	uint8_t       *peer_kexinit;
	dssh_key_algo  key_algo_selected;
	dssh_enc_ctx  *enc_c2s_ctx;
	dssh_enc       enc_c2s_selected;
	dssh_enc_ctx  *enc_s2c_ctx;
	dssh_enc       enc_s2c_selected;
	dssh_mac_ctx  *mac_c2s_ctx;
	dssh_mac       mac_c2s_selected;
	dssh_mac_ctx  *mac_s2c_ctx;
	dssh_mac       mac_s2c_selected;
	dssh_comp_ctx *comp_c2s_ctx;
	dssh_comp      comp_c2s_selected;
	dssh_comp_ctx *comp_s2c_ctx;
	dssh_comp      comp_s2c_selected;
} *dssh_transport_state;
/*
 * Algorithm registration — call before any session is initialized.
 * Registration order determines negotiation preference (first registered
 * is most preferred).  Returns 0 on success.  After the first
 * dssh_transport_init() call, registration is locked.
 *
 * Applications may register their own custom modules alongside or
 * instead of the library's built-in algorithms.
 */
DSSH_PUBLIC int dssh_transport_register_kex(dssh_kex kex);
DSSH_PUBLIC int dssh_transport_register_key_algo(dssh_key_algo key_algo);
DSSH_PUBLIC int dssh_transport_register_enc(dssh_enc enc);
DSSH_PUBLIC int dssh_transport_register_mac(dssh_mac mac);
DSSH_PUBLIC int dssh_transport_register_comp(dssh_comp comp);
DSSH_PUBLIC int dssh_transport_register_lang(dssh_language lang);
/*
 * Initialize transport state for a session.  Allocates packet buffers
 * sized to max_packet_size (pass 0 for the RFC minimum of 33280 bytes).
 * Locks the algorithm registry on first call.  Returns 0 on success.
 */
/*
 * Perform the complete SSH transport handshake: version exchange,
 * algorithm negotiation, key exchange, and NEWKEYS.  On return,
 * the encrypted transport is active and ready for authentication.
 * Returns 0 on success.
 */
DSSH_PUBLIC int dssh_transport_handshake(dssh_session sess);
/*
 * Send SSH_MSG_DISCONNECT (RFC 4253 s11.1) and set terminate flag.
 * The desc string is clamped to 230 bytes.  The send is best-effort
 * (errors are ignored since we're disconnecting).
 */
DSSH_PUBLIC int dssh_transport_disconnect(dssh_session sess,
    uint32_t reason, const char *desc);
/* Query functions — return negotiated algorithm names or NULL. */
DSSH_PUBLIC const char *dssh_transport_get_remote_version(dssh_session sess);
DSSH_PUBLIC const char *dssh_transport_get_kex_name(dssh_session sess);
DSSH_PUBLIC const char *dssh_transport_get_hostkey_name(dssh_session sess);
DSSH_PUBLIC const char *dssh_transport_get_enc_name(dssh_session sess);
DSSH_PUBLIC const char *dssh_transport_get_mac_name(dssh_session sess);
/* ================================================================
 * Internal functions — used by other library modules, not by
 * applications.  DSSH_PRIVATE in shared builds.
 * ================================================================ */
DSSH_PRIVATE int dssh_transport_init(dssh_session sess, size_t max_packet_size);
DSSH_PRIVATE void dssh_transport_cleanup(dssh_session sess);
DSSH_PRIVATE int dssh_transport_send_packet(dssh_session sess,
    const uint8_t *payload, size_t payload_len, uint32_t *seq_out);
DSSH_PRIVATE int dssh_transport_recv_packet(dssh_session sess,
    uint8_t *msg_type, uint8_t **payload, size_t *payload_len);
DSSH_PRIVATE int dssh_transport_send_unimplemented(dssh_session sess,
    uint32_t                                                    rejected_seq);
DSSH_PRIVATE int dssh_transport_version_exchange(dssh_session sess);
DSSH_PRIVATE int dssh_transport_kexinit(dssh_session sess);
DSSH_PRIVATE int dssh_transport_kex(dssh_session sess);
DSSH_PRIVATE int dssh_transport_newkeys(dssh_session sess);
DSSH_PRIVATE int dssh_transport_rekey(dssh_session sess);
DSSH_PRIVATE bool dssh_transport_rekey_needed(dssh_session sess);
DSSH_PRIVATE dssh_key_algo dssh_transport_find_key_algo(const char *name);

/*
 * Global algorithm registry and I/O callback config.
 * In library code this is a file-scope variable in ssh-trans.c.
 * Under DSSH_TESTING it is externally visible for test access.
 */
typedef struct dssh_transport_global_config {
	atomic_bool              used;
	const char              *software_version;
	const char              *version_comment;
	dssh_transport_io_cb     tx;
	dssh_transport_io_cb     rx;
	dssh_transport_rxline_cb rx_line;

	int (*extra_line_cb)(uint8_t *buf, size_t bufsz, void *cbdata);

	size_t                   kex_entries;
	dssh_kex                 kex_head;
	dssh_kex                 kex_tail;
	size_t                   key_algo_entries;
	dssh_key_algo            key_algo_head;
	dssh_key_algo            key_algo_tail;
	size_t                   enc_entries;
	dssh_enc                 enc_head;
	dssh_enc                 enc_tail;
	size_t                   mac_entries;
	dssh_mac                 mac_head;
	dssh_mac                 mac_tail;
	size_t                   comp_entries;
	dssh_comp                comp_head;
	dssh_comp                comp_tail;
	size_t                   lang_entries;
	dssh_language            lang_head;
	dssh_language            lang_tail;
} *dssh_transport_global_config;

/*
 * OpenSSL failure injection.  Under DSSH_TESTING, failable OpenSSL
 * calls are redirected to countdown wrappers in dssh_test_ossl.c.
 * Algorithm module files (enc/, mac/, kex/, key_algo/) include this
 * header, so the macros apply to all library OpenSSL usage.
 * OpenSSL's own internal calls are unaffected (it doesn't include
 * this header).  Free/destroy/cleanse are NOT redirected.
 */
#ifdef DSSH_TESTING

 #include <openssl/bn.h>
 #include <openssl/evp.h>
 #include <openssl/params.h>
/* Creation wrappers (return NULL on failure) */
EVP_MD_CTX *dssh_test_EVP_MD_CTX_new(void);
EVP_CIPHER_CTX *dssh_test_EVP_CIPHER_CTX_new(void);
EVP_MAC_CTX *dssh_test_EVP_MAC_CTX_new(EVP_MAC *mac);
EVP_MAC *dssh_test_EVP_MAC_fetch(OSSL_LIB_CTX *ctx, const char *algorithm, const char *properties);
EVP_MD *dssh_test_EVP_MD_fetch(OSSL_LIB_CTX *ctx, const char *algorithm, const char *properties);
EVP_PKEY_CTX *dssh_test_EVP_PKEY_CTX_new(EVP_PKEY *pkey, ENGINE *e);
EVP_PKEY_CTX *dssh_test_EVP_PKEY_CTX_new_id(int id, ENGINE *e);
EVP_PKEY_CTX *dssh_test_EVP_PKEY_CTX_new_from_name(OSSL_LIB_CTX *ctx, const char *name, const char *propquery);
EVP_PKEY *dssh_test_EVP_PKEY_new_raw_public_key(int type, ENGINE *e, const unsigned char *pub, size_t len);
BN_CTX *dssh_test_BN_CTX_new(void);
BIGNUM *dssh_test_BN_new(void);
OSSL_PARAM_BLD *dssh_test_OSSL_PARAM_BLD_new(void);
OSSL_PARAM *dssh_test_OSSL_PARAM_BLD_to_param(OSSL_PARAM_BLD *bld);

 #define EVP_MD_CTX_new() dssh_test_EVP_MD_CTX_new()
 #define EVP_CIPHER_CTX_new() dssh_test_EVP_CIPHER_CTX_new()
 #define EVP_MAC_CTX_new(m) dssh_test_EVP_MAC_CTX_new(m)
 #define EVP_MAC_fetch(c, a, p) dssh_test_EVP_MAC_fetch(c, a, p)
 #define EVP_MD_fetch(c, a, p) dssh_test_EVP_MD_fetch(c, a, p)
 #define EVP_PKEY_CTX_new(k, e) dssh_test_EVP_PKEY_CTX_new(k, e)
 #define EVP_PKEY_CTX_new_id(i, e) dssh_test_EVP_PKEY_CTX_new_id(i, e)
 #define EVP_PKEY_CTX_new_from_name(c, n, p) dssh_test_EVP_PKEY_CTX_new_from_name(c, n, p)
 #define EVP_PKEY_new_raw_public_key(t, e, p, l) dssh_test_EVP_PKEY_new_raw_public_key(t, e, p, l)
 #define BN_CTX_new() dssh_test_BN_CTX_new()
 #define BN_new() dssh_test_BN_new()
 #define OSSL_PARAM_BLD_new() dssh_test_OSSL_PARAM_BLD_new()
 #define OSSL_PARAM_BLD_to_param(b) dssh_test_OSSL_PARAM_BLD_to_param(b)
/* Operation wrappers (return 0/<=0 on failure) */
int dssh_test_EVP_DigestInit_ex(EVP_MD_CTX *ctx, const EVP_MD *type, ENGINE *impl);
int dssh_test_EVP_DigestUpdate(EVP_MD_CTX *ctx, const void *d, size_t cnt);
int dssh_test_EVP_DigestFinal_ex(EVP_MD_CTX *ctx, unsigned char *md, unsigned int *s);
int dssh_test_EVP_DigestSignInit(EVP_MD_CTX *ctx, EVP_PKEY_CTX **pctx, const EVP_MD *type, ENGINE *e, EVP_PKEY *pkey);
int dssh_test_EVP_DigestSign(EVP_MD_CTX *ctx,
    unsigned char                       *sigret,
    size_t                              *siglen,
    const unsigned char                 *tbs,
    size_t                               tbslen);
int dssh_test_EVP_DigestVerifyInit(EVP_MD_CTX *ctx, EVP_PKEY_CTX **pctx, const EVP_MD *type, ENGINE *e, EVP_PKEY *pkey);
int dssh_test_EVP_DigestVerify(EVP_MD_CTX *ctx,
    const unsigned char                   *sigret,
    size_t                                 siglen,
    const unsigned char                   *tbs,
    size_t                                 tbslen);
int dssh_test_EVP_EncryptInit_ex(EVP_CIPHER_CTX *ctx,
    const EVP_CIPHER                            *cipher,
    ENGINE                                      *impl,
    const unsigned char                         *key,
    const unsigned char                         *iv);
int dssh_test_EVP_EncryptUpdate(EVP_CIPHER_CTX *ctx, unsigned char *out, int *outl, const unsigned char *in, int inl);
int dssh_test_EVP_MAC_init(EVP_MAC_CTX *ctx, const unsigned char *key, size_t keylen, const OSSL_PARAM params[]);
int dssh_test_EVP_MAC_update(EVP_MAC_CTX *ctx, const unsigned char *data, size_t datalen);
int dssh_test_EVP_MAC_final(EVP_MAC_CTX *ctx, unsigned char *out, size_t *outl, size_t outsize);
int dssh_test_EVP_PKEY_keygen_init(EVP_PKEY_CTX *ctx);
int dssh_test_EVP_PKEY_keygen(EVP_PKEY_CTX *ctx, EVP_PKEY **ppkey);
int dssh_test_EVP_PKEY_derive_init(EVP_PKEY_CTX *ctx);
int dssh_test_EVP_PKEY_derive_set_peer(EVP_PKEY_CTX *ctx, EVP_PKEY *peer);
int dssh_test_EVP_PKEY_derive(EVP_PKEY_CTX *ctx, unsigned char *key, size_t *keylen);
int dssh_test_EVP_PKEY_fromdata_init(EVP_PKEY_CTX *ctx);
int dssh_test_EVP_PKEY_fromdata(EVP_PKEY_CTX *ctx, EVP_PKEY **ppkey, int selection, OSSL_PARAM params[]);
int dssh_test_EVP_PKEY_get_raw_public_key(const EVP_PKEY *pkey, unsigned char *pub, size_t *len);
int dssh_test_EVP_PKEY_get_bn_param(const EVP_PKEY *pkey, const char *key_name, BIGNUM **bn);
int dssh_test_RAND_bytes(unsigned char *buf, int num);
int dssh_test_BN_mod_exp(BIGNUM *r, const BIGNUM *a, const BIGNUM *p, const BIGNUM *m, BN_CTX *ctx);
int dssh_test_BN_rand(BIGNUM *rnd, int bits, int top, int bottom);
BIGNUM *dssh_test_BN_bin2bn(const unsigned char *s, int len, BIGNUM *ret);
int dssh_test_OSSL_PARAM_BLD_push_BN(OSSL_PARAM_BLD *bld, const char *key, const BIGNUM *bn);
int dssh_test_EVP_CIPHER_CTX_set_padding(EVP_CIPHER_CTX *ctx, int pad);
int dssh_test_EVP_PKEY_CTX_set_params(EVP_PKEY_CTX *ctx, const OSSL_PARAM *params);

 #define EVP_DigestInit_ex(c, t, i) dssh_test_EVP_DigestInit_ex(c, t, i)
 #define EVP_DigestUpdate(c, d, n) dssh_test_EVP_DigestUpdate(c, d, n)
 #define EVP_DigestFinal_ex(c, m, s) dssh_test_EVP_DigestFinal_ex(c, m, s)
 #define EVP_DigestSignInit(c, p, t, e, k) dssh_test_EVP_DigestSignInit(c, p, t, e, k)
 #define EVP_DigestSign(c, s, sl, t, tl) dssh_test_EVP_DigestSign(c, s, sl, t, tl)
 #define EVP_DigestVerifyInit(c, p, t, e, k) dssh_test_EVP_DigestVerifyInit(c, p, t, e, k)
 #define EVP_DigestVerify(c, s, sl, t, tl) dssh_test_EVP_DigestVerify(c, s, sl, t, tl)
 #define EVP_EncryptInit_ex(c, ci, i, k, iv) dssh_test_EVP_EncryptInit_ex(c, ci, i, k, iv)
 #define EVP_EncryptUpdate(c, o, ol, i, il) dssh_test_EVP_EncryptUpdate(c, o, ol, i, il)
 #define EVP_MAC_init(c, k, kl, p) dssh_test_EVP_MAC_init(c, k, kl, p)
 #define EVP_MAC_update(c, d, dl) dssh_test_EVP_MAC_update(c, d, dl)
 #define EVP_MAC_final(c, o, ol, os) dssh_test_EVP_MAC_final(c, o, ol, os)
 #define EVP_PKEY_keygen_init(c) dssh_test_EVP_PKEY_keygen_init(c)
 #define EVP_PKEY_keygen(c, p) dssh_test_EVP_PKEY_keygen(c, p)
 #define EVP_PKEY_derive_init(c) dssh_test_EVP_PKEY_derive_init(c)
 #define EVP_PKEY_derive_set_peer(c, p) dssh_test_EVP_PKEY_derive_set_peer(c, p)
 #define EVP_PKEY_derive(c, k, kl) dssh_test_EVP_PKEY_derive(c, k, kl)
 #define EVP_PKEY_fromdata_init(c) dssh_test_EVP_PKEY_fromdata_init(c)
 #define EVP_PKEY_fromdata(c, p, s, pa) dssh_test_EVP_PKEY_fromdata(c, p, s, pa)
 #define EVP_PKEY_get_raw_public_key(p, b, l) dssh_test_EVP_PKEY_get_raw_public_key(p, b, l)
 #define EVP_PKEY_get_bn_param(p, k, b) dssh_test_EVP_PKEY_get_bn_param(p, k, b)
 #define RAND_bytes(b, n) dssh_test_RAND_bytes(b, n)
 #define BN_mod_exp(r, a, p, m, c) dssh_test_BN_mod_exp(r, a, p, m, c)
 #define BN_rand(r, b, t, bo) dssh_test_BN_rand(r, b, t, bo)
 #define BN_bin2bn(s, l, r) dssh_test_BN_bin2bn(s, l, r)
 #define OSSL_PARAM_BLD_push_BN(b, k, bn) dssh_test_OSSL_PARAM_BLD_push_BN(b, k, bn)
 #define EVP_CIPHER_CTX_set_padding(c, p) dssh_test_EVP_CIPHER_CTX_set_padding(c, p)
 #define EVP_PKEY_CTX_set_params(c, p) dssh_test_EVP_PKEY_CTX_set_params(c, p)

#endif /* DSSH_TESTING */

#ifdef __cplusplus
}
#endif

#endif // ifndef DSSH_TRANS_H
