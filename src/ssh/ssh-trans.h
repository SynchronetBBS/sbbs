// RFC-4253

#ifndef DEUCE_SSH_TRANS_H
#define DEUCE_SSH_TRANS_H

#ifndef DEUCE_SSH_H
#error Only include deucessh.h, do not directly include this file.
#endif

/* Opaque context types for algorithm modules.
 * Each module defines the actual struct contents. */
typedef struct deuce_ssh_enc_ctx deuce_ssh_enc_ctx;
typedef struct deuce_ssh_mac_ctx deuce_ssh_mac_ctx;
typedef struct deuce_ssh_key_algo_ctx deuce_ssh_key_algo_ctx;
typedef struct deuce_ssh_comp_ctx deuce_ssh_comp_ctx;

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

#define DEUCE_SSH_KEX_FLAG_NEEDS_ENCRYPTION_CAPABLE UINT32_C(1<<0)
#define DEUCE_SSH_KEX_FLAG_NEEDS_SIGNATURE_CAPABLE  UINT32_C(1<<1)

#define DEUCE_SSH_KEY_ALGO_FLAG_ENCRYPTION_CAPABLE UINT32_C(1<<0)
#define DEUCE_SSH_KEY_ALGO_FLAG_SIGNATURE_CAPABLE  UINT32_C(1<<1)

/*
 * RFC 4253 s6.1: implementations MUST be able to process packets with
 * an uncompressed payload length of 32768 bytes or less.
 * Total packet = 4 (length) + 1 (padding_length) + payload + padding.
 * With max padding of 255 bytes, minimum buffer = 32768 + 260 = 33028.
 * We round up for alignment.
 */
#define SSH_BPP_PACKET_SIZE_MIN 33280

typedef struct deuce_ssh_transport_packet_s {
	deuce_ssh_string payload;
	deuce_ssh_string random_padding;
	deuce_ssh_string mac;
	deuce_ssh_uint32_t packet_length;
	deuce_ssh_byte   padding;
} *deuce_ssh_transport_packet;

/*
 * KEX handler: drives the entire key exchange after KEXINIT
 * negotiation.  Uses send_packet/recv_packet internally.
 * Must populate sess->trans.shared_secret, exchange_hash,
 * and their sizes.
 */
typedef int (*deuce_ssh_kex_handler)(deuce_ssh_session sess);
typedef void (*deuce_ssh_kex_cleanup)(deuce_ssh_session sess);

typedef int (*deuce_ssh_key_algo_sign)(uint8_t *buf, size_t bufsz, size_t *outlen,
    const uint8_t *data, size_t data_len, deuce_ssh_key_algo_ctx *ctx);
typedef int (*deuce_ssh_key_algo_verify)(const uint8_t *key_blob, size_t key_blob_len,
    const uint8_t *sig_blob, size_t sig_blob_len,
    const uint8_t *data, size_t data_len);
typedef int (*deuce_ssh_key_algo_pubkey)(uint8_t *buf, size_t bufsz, size_t *outlen, deuce_ssh_key_algo_ctx *ctx);
typedef int (*deuce_ssh_key_algo_haskey)(deuce_ssh_key_algo_ctx *ctx);
typedef void (*deuce_ssh_key_algo_cleanup)(deuce_ssh_key_algo_ctx *ctx);

typedef int (*deuce_ssh_enc_init)(const uint8_t *key, const uint8_t *iv, bool encrypt, deuce_ssh_enc_ctx **ctx);
typedef int (*deuce_ssh_enc_crypt)(uint8_t *buf, size_t bufsz, deuce_ssh_enc_ctx *ctx);
typedef void (*deuce_ssh_enc_cleanup)(deuce_ssh_enc_ctx *ctx);

typedef int (*deuce_ssh_mac_generate)(const uint8_t *key, const uint8_t *buf, size_t bufsz, uint8_t *outbuf);
typedef void (*deuce_ssh_mac_cleanup)(deuce_ssh_mac_ctx *ctx);

typedef int (*deuce_ssh_comp_compress)(uint8_t *buf, uint8_t *bufsz, deuce_ssh_comp_ctx *ctx);
typedef int (*deuce_ssh_comp_uncompress)(uint8_t *buf, uint8_t *bufsz, deuce_ssh_comp_ctx *ctx);
typedef void (*deuce_ssh_comp_cleanup)(deuce_ssh_comp_ctx *ctx);

typedef struct deuce_ssh_kex_s {
	struct deuce_ssh_kex_s *next;
	deuce_ssh_kex_handler handler;
	deuce_ssh_kex_cleanup cleanup;
	uint32_t flags;
	const char *hash_name; /* OpenSSL digest name, e.g. "SHA256" */
	char name[];
} *deuce_ssh_kex;

typedef struct deuce_ssh_key_algo_s {
	struct deuce_ssh_key_algo_s *next;
	deuce_ssh_key_algo_sign sign;
	deuce_ssh_key_algo_verify verify;
	deuce_ssh_key_algo_pubkey pubkey;
	deuce_ssh_key_algo_haskey haskey;
	deuce_ssh_key_algo_cleanup cleanup;
	uint32_t flags;
	char name[];
} *deuce_ssh_key_algo;

typedef struct deuce_ssh_enc_s {
	struct deuce_ssh_enc_s *next;
	deuce_ssh_enc_init init;
	deuce_ssh_enc_crypt encrypt;
	deuce_ssh_enc_crypt decrypt;
	deuce_ssh_enc_cleanup cleanup;
	uint32_t flags;
	uint16_t blocksize;
	uint16_t key_size;
	char name[];
} *deuce_ssh_enc;

typedef struct deuce_ssh_mac_s {
	struct deuce_ssh_mac_s *next;
	deuce_ssh_mac_generate generate;
	deuce_ssh_mac_cleanup cleanup;
	uint16_t digest_size;
	uint16_t key_size;
	char name[];
} *deuce_ssh_mac;

typedef struct deuce_ssh_comp_s {
	struct deuce_ssh_comp_s *next;
	deuce_ssh_comp_compress compress;
	deuce_ssh_comp_uncompress uncompress;
	deuce_ssh_comp_cleanup cleanup;
	char name[];
} *deuce_ssh_comp;

typedef struct deuce_ssh_language_s {
	struct deuce_ssh_language_s *next;
	char name[];
} *deuce_ssh_language;

typedef struct deuce_ssh_transport_state_s {
	uint32_t    tx_seq;
	uint32_t    rx_seq;
	bool        client;

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
	thrd_t transport_thread;
	size_t id_str_sz;
	char id_str[254];

	/* Remote peer information (from version exchange) */
	size_t remote_id_str_sz;
	char remote_id_str[254];
	char **remote_languages;

	/* KEX options */
	void *kex_ctx;
	deuce_ssh_kex kex_selected;

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

	deuce_ssh_key_algo_ctx *key_algo_ctx;
	deuce_ssh_key_algo key_algo_selected;

	deuce_ssh_enc_ctx *enc_c2s_ctx;
	deuce_ssh_enc enc_c2s_selected;
	deuce_ssh_enc_ctx *enc_s2c_ctx;
	deuce_ssh_enc enc_s2c_selected;

	deuce_ssh_mac_ctx *mac_c2s_ctx;
	deuce_ssh_mac mac_c2s_selected;
	deuce_ssh_mac_ctx *mac_s2c_ctx;
	deuce_ssh_mac mac_s2c_selected;

	deuce_ssh_comp_ctx *comp_c2s_ctx;
	deuce_ssh_comp comp_c2s_selected;
	deuce_ssh_comp_ctx *comp_s2c_ctx;
	deuce_ssh_comp comp_s2c_selected;
} *deuce_ssh_transport_state;

/* Registration (before any session starts) */
int deuce_ssh_transport_register_kex(deuce_ssh_kex kex);
int deuce_ssh_transport_register_key_algo(deuce_ssh_key_algo key_algo);
int deuce_ssh_transport_register_enc(deuce_ssh_enc enc);
int deuce_ssh_transport_register_mac(deuce_ssh_mac mac);
int deuce_ssh_transport_register_comp(deuce_ssh_comp comp);
int deuce_ssh_transport_register_lang(deuce_ssh_language lang);

/* Transport protocol */
int deuce_ssh_transport_init(deuce_ssh_session sess, size_t max_packet_size);
void deuce_ssh_transport_cleanup(deuce_ssh_session sess);
int deuce_ssh_transport_version_exchange(deuce_ssh_session sess);
int deuce_ssh_transport_send_packet(deuce_ssh_session sess,
    const uint8_t *payload, size_t payload_len, uint32_t *seq_out);
int deuce_ssh_transport_recv_packet(deuce_ssh_session sess,
    uint8_t *msg_type, uint8_t **payload, size_t *payload_len);
int deuce_ssh_transport_kexinit(deuce_ssh_session sess);
int deuce_ssh_transport_kex(deuce_ssh_session sess);
int deuce_ssh_transport_newkeys(deuce_ssh_session sess);
int deuce_ssh_transport_disconnect(deuce_ssh_session sess,
    uint32_t reason, const char *desc);
const char *deuce_ssh_transport_get_remote_version(deuce_ssh_session sess);
const char *deuce_ssh_transport_get_kex_name(deuce_ssh_session sess);
const char *deuce_ssh_transport_get_hostkey_name(deuce_ssh_session sess);
const char *deuce_ssh_transport_get_enc_name(deuce_ssh_session sess);
const char *deuce_ssh_transport_get_mac_name(deuce_ssh_session sess);

#endif
