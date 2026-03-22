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
#define SSH_MSG_DISCONNECT      UINT8_C(1)
#define SSH_MSG_IGNORE          UINT8_C(2)
#define SSH_MSG_UNIMPLEMENTED   UINT8_C(3)
#define SSH_MSG_DEBUG           UINT8_C(4)
#define SSH_MSG_SERVICE_REQUEST UINT8_C(5)
#define SSH_MSG_SERVICE_ACCEPT  UINT8_C(6)
/* Transport layer Algorithm negotiation */
#define SSH_MSG_KEXINIT         UINT8_C(20)
#define SSH_MSG_NEWKEYS         UINT8_C(21)

/* KEX method-specific (ECDH, DH) */
#define SSH_MSG_KEX_ECDH_INIT   UINT8_C(30)
#define SSH_MSG_KEX_ECDH_REPLY  UINT8_C(31)

/* SSH_MSG_DISCONNECT reason codes */
#define SSH_DISCONNECT_HOST_NOT_ALLOWED_TO_CONNECT    UINT32_C(1)
#define SSH_DISCONNECT_PROTOCOL_ERROR                 UINT32_C(2)
#define SSH_DISCONNECT_KEY_EXCHANGE_FAILED            UINT32_C(3)
#define SSH_DISCONNECT_RESERVED                       UINT32_C(4)
#define SSH_DISCONNECT_MAC_ERROR                      UINT32_C(5)
#define SSH_DISCONNECT_COMPRESSION_ERROR              UINT32_C(6)
#define SSH_DISCONNECT_SERVICE_NOT_AVAILABLE          UINT32_C(7)
#define SSH_DISCONNECT_PROTOCOL_VERSION_NOT_SUPPORTED UINT32_C(8)
#define SSH_DISCONNECT_HOST_KEY_NOT_VERIFIABLE        UINT32_C(9)
#define SSH_DISCONNECT_CONNECTION_LOST                UINT32_C(10)
#define SSH_DISCONNECT_BY_APPLICATION                 UINT32_C(11)
#define SSH_DISCONNECT_TOO_MANY_CONNECTIONS           UINT32_C(12)
#define SSH_DISCONNECT_AUTH_CANCELLED_BY_USER         UINT32_C(13)
#define SSH_DISCONNECT_NO_MORE_AUTH_METHODS_AVAILABLE UINT32_C(14)
#define SSH_DISCONNECT_ILLEGAL_USER_NAME              UINT32_C(15)

#define DSSH_KEX_FLAG_NEEDS_ENCRYPTION_CAPABLE UINT32_C(1<<0)
#define DSSH_KEX_FLAG_NEEDS_SIGNATURE_CAPABLE  UINT32_C(1<<1)

#define DSSH_KEY_ALGO_FLAG_ENCRYPTION_CAPABLE UINT32_C(1<<0)
#define DSSH_KEY_ALGO_FLAG_SIGNATURE_CAPABLE  UINT32_C(1<<1)

/*
 * RFC 4253 s6.1: implementations MUST be able to process packets with
 * an uncompressed payload length of 32768 bytes or less.
 * Total packet = 4 (length) + 1 (padding_length) + payload + padding.
 * With max padding of 255 bytes, minimum buffer = 32768 + 260 = 33028.
 * We round up for alignment.
 */
#define SSH_BPP_PACKET_SIZE_MIN  33280
#define SSH_BPP_PACKET_SIZE_MAX  (64 * 1024 * 1024)  /* 64 MiB */

typedef struct dssh_transport_packet_s {
	dssh_string payload;
	dssh_string random_padding;
	dssh_string mac;
	dssh_uint32_t packet_length;
	dssh_byte   padding;
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
	dssh_kex_handler handler;
	dssh_kex_cleanup cleanup;
	uint32_t flags;
	const char *hash_name; /* OpenSSL digest name, e.g. "SHA256" */
	char name[];
} *dssh_kex;

typedef struct dssh_key_algo_s {
	struct dssh_key_algo_s *next;
	dssh_key_algo_sign sign;
	dssh_key_algo_verify verify;
	dssh_key_algo_pubkey pubkey;
	dssh_key_algo_haskey haskey;
	dssh_key_algo_cleanup cleanup;
	dssh_key_algo_ctx *ctx;
	uint32_t flags;
	char name[];
} *dssh_key_algo;

typedef struct dssh_enc_s {
	struct dssh_enc_s *next;
	dssh_enc_init init;
	dssh_enc_crypt encrypt;
	dssh_enc_crypt decrypt;
	dssh_enc_cleanup cleanup;
	uint32_t flags;
	uint16_t blocksize;
	uint16_t key_size;
	char name[];
} *dssh_enc;

typedef struct dssh_mac_s {
	struct dssh_mac_s *next;
	dssh_mac_init init;
	dssh_mac_generate generate;
	dssh_mac_cleanup cleanup;
	uint16_t digest_size;
	uint16_t key_size;
	char name[];
} *dssh_mac;

typedef struct dssh_comp_s {
	struct dssh_comp_s *next;
	dssh_comp_compress compress;
	dssh_comp_uncompress uncompress;
	dssh_comp_cleanup cleanup;
	char name[];
} *dssh_comp;

typedef struct dssh_language_s {
	struct dssh_language_s *next;
	char name[];
} *dssh_language;

/* Rekey thresholds (RFC 4253 s9, RFC 4251 s9.3.2) */
#define DSSH_REKEY_SOFT_LIMIT  UINT32_C(0x10000000)  /* 2^28 packets */
#define DSSH_REKEY_HARD_LIMIT  UINT32_C(0x80000000)  /* 2^31 packets */
#define DSSH_REKEY_BYTES       UINT64_C(0x40000000)  /* 1 GiB */
#define DSSH_REKEY_SECONDS     3600                   /* 1 hour */

typedef struct dssh_transport_state_s {
	uint32_t    tx_seq;
	uint32_t    rx_seq;
	uint32_t    tx_since_rekey;  /* packets sent since last (re)key */
	uint32_t    rx_since_rekey;  /* packets received since last (re)key */
	uint64_t    bytes_since_rekey; /* bytes sent+received since last (re)key */
	time_t      rekey_time;     /* time of last (re)key */
	uint32_t    last_rx_seq;    /* seq number of last received packet */
	bool        client;
	bool        rekey_in_progress; /* true between KEXINIT and NEWKEYS */
	bool        rekey_pending;     /* deferred auto-rekey (set in recv_packet) */
	cnd_t       rekey_cnd;     /* wakes senders blocked during rekey */

	/* Packet buffers (dynamically allocated) */
	size_t packet_buf_sz;
	uint8_t *tx_packet;
	uint8_t *rx_packet;
	uint8_t *tx_mac_scratch;  /* 4 + packet_buf_sz for MAC computation */
	uint8_t *rx_mac_scratch;

	/* Concurrency: separate mutexes for send and receive paths */
	mtx_t tx_mtx;
	mtx_t rx_mtx;

	/* Transport options */
	size_t id_str_sz;
	char id_str[254];

	/* Remote peer information (from version exchange) */
	size_t remote_id_str_sz;
	char remote_id_str[254];
	char **remote_languages;

	/* KEX options */
	void *kex_ctx;
	dssh_kex kex_selected;

	/* KEX outputs */
	size_t shared_secret_sz;
	uint8_t *shared_secret;
	size_t exchange_hash_sz;
	uint8_t *exchange_hash;

	/* Session ID (first exchange hash, persists across rekeys) */
	size_t session_id_sz;
	uint8_t *session_id;

	/* Saved KEXINIT payloads for exchange hash computation */
	size_t our_kexinit_sz;
	uint8_t *our_kexinit;
	size_t peer_kexinit_sz;
	uint8_t *peer_kexinit;

	dssh_key_algo key_algo_selected;

	dssh_enc_ctx *enc_c2s_ctx;
	dssh_enc enc_c2s_selected;
	dssh_enc_ctx *enc_s2c_ctx;
	dssh_enc enc_s2c_selected;

	dssh_mac_ctx *mac_c2s_ctx;
	dssh_mac mac_c2s_selected;
	dssh_mac_ctx *mac_s2c_ctx;
	dssh_mac mac_s2c_selected;

	dssh_comp_ctx *comp_c2s_ctx;
	dssh_comp comp_c2s_selected;
	dssh_comp_ctx *comp_s2c_ctx;
	dssh_comp comp_s2c_selected;
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
    uint32_t rejected_seq);
DSSH_PRIVATE int dssh_transport_version_exchange(dssh_session sess);
DSSH_PRIVATE int dssh_transport_kexinit(dssh_session sess);
DSSH_PRIVATE int dssh_transport_kex(dssh_session sess);
DSSH_PRIVATE int dssh_transport_newkeys(dssh_session sess);
DSSH_PRIVATE int dssh_transport_rekey(dssh_session sess);
DSSH_PRIVATE bool dssh_transport_rekey_needed(dssh_session sess);
DSSH_PRIVATE dssh_key_algo dssh_transport_find_key_algo(const char *name);

#ifdef __cplusplus
}
#endif

#endif
