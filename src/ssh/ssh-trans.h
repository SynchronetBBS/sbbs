// RFC-4253

#include "ssh.h"
#include "ssh-arch.h"

#ifndef DEUCE_SSH_TRANS_H
#define DEUCE_SSH_TRANS_H

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

#ifndef SSH_TOTAL_BPP_PACKET_SIZE_MAX
/*
 * Defined in RFC 4253 s6.1, but may be larger "where they might be needed"
 * and "if the identification string indicates that the other party is able to process them"
 * which means that we would need to maintain a list of implementation id
 * strings that support larger packets, *and* understand which message
 * "need" a larger packet.
 * 
 * That seems like a never-ending job, so I'll just allow the user to
 * override this and let them sort it out if the need to.
 */
#define SSH_TOTAL_BPP_PACKET_SIZE_MAX 35000
#endif

typedef struct deuce_ssh_transport_packet {
	deuce_ssh_string_t payload;
	deuce_ssh_string_t random_padding;
	deuce_ssh_string_t mac;
	deuce_ssh_uint32_t packet_length;
	deuce_ssh_byte_t   padding;
} *deuce_ssh_transport_packet_t;

typedef int (*deuce_ssh_kex_handler_t)(deuce_ssh_transport_packet_t pkt, deuce_ssh_session_t sess);
typedef void (*deuce_ssh_kex_cleanup_t)(deuce_ssh_session_t sess);

typedef int (*deuce_ssh_key_algo_sign_t)(uint8_t *buf, uint8_t *bufsz, deuce_ssh_session_t sess);
typedef int (*deuce_ssh_key_algo_haskey_t)(deuce_ssh_session_t sess);
typedef void (*deuce_ssh_key_algo_cleanup_t)(deuce_ssh_session_t sess);

typedef int (*deuce_ssh_enc_encrypt_t)(uint8_t *buf, uint8_t *bufsz, deuce_ssh_session_t sess);
typedef int (*deuce_ssh_enc_decrypt_t)(uint8_t *buf, uint8_t *bufsz, deuce_ssh_session_t sess);
typedef void (*deuce_ssh_enc_cleanup_t)(deuce_ssh_session_t sess);

typedef int (*deuce_ssh_mac_generate_t)(uint8_t *key, uint8_t *buf, size_t bufsz, uint8_t *outbuf, deuce_ssh_session_t sess);
typedef void (*deuce_ssh_mac_cleanup_t)(deuce_ssh_session_t sess);

typedef int (*deuce_ssh_comp_compress_t)(uint8_t *buf, uint8_t *bufsz, deuce_ssh_session_t sess);
typedef int (*deuce_ssh_comp_uncompress_t)(uint8_t *buf, uint8_t *bufsz, deuce_ssh_session_t sess);
typedef void (*deuce_ssh_comp_cleanup_t)(deuce_ssh_session_t sess);

typedef struct deuce_ssh_kex {
	struct deuce_ssh_kex *next;
	deuce_ssh_kex_handler_t handler;
	deuce_ssh_kex_cleanup_t cleanup;
	uint32_t flags;
	char name[];
} *deuce_ssh_kex_t;

typedef struct deuce_ssh_key_algo {
	struct deuce_ssh_key_algo *next;
	deuce_ssh_key_algo_sign_t sign;
	deuce_ssh_key_algo_haskey_t haskey;
	deuce_ssh_key_algo_cleanup_t cleanup;
	uint32_t flags;
	char name[];
} *deuce_ssh_key_algo_t;

typedef struct deuce_ssh_enc {
	struct deuce_ssh_enc *next;
	deuce_ssh_enc_encrypt_t encrypt;
	deuce_ssh_enc_decrypt_t decrypt;
	deuce_ssh_enc_cleanup_t cleanup;
	uint32_t flags;
	uint16_t blocksize;
	uint16_t key_size;
	char name[];
} *deuce_ssh_enc_t;

typedef struct deuce_ssh_mac {
	struct deuce_ssh_mac *next;
	deuce_ssh_mac_generate_t generate;
	deuce_ssh_mac_cleanup_t cleanup;
	uint16_t digest_size;
	uint16_t key_size;
	char name[];
} *deuce_ssh_mac_t;

typedef struct deuce_ssh_comp {
	struct deuce_ssh_comp *next;
	deuce_ssh_comp_compress_t compress;
	deuce_ssh_comp_uncompress_t uncompress;
	deuce_ssh_comp_cleanup_t cleanup;
	char name[];
} *deuce_ssh_comp_t;

typedef struct deuce_ssh_language {
	struct deuce_ssh_language *next;
	char name;
} *deuce_ssh_language_t;

typedef struct deuce_ssh_transport_state {
	uint32_t    tx_seq;
	uint32_t    rx_seq;
	bool        client;

	/* Transport options */
	thrd_t transport_thread;
	size_t id_str_sz;
	char id_str[254];

	/* KEX options */
	void *kex_cbdata;
	deuce_ssh_kex_t kex_selected;

	/* KEX outputs */
	size_t shared_secret_sz;
	uint8_t *shared_secret;
	size_t exchange_hash_sz;
	uint8_t *exchange_hash;

	void *key_algo_cbdata;
	deuce_ssh_key_algo_t key_algo_selected;
	void *enc_cbdata;
	deuce_ssh_enc_t enc_selected;
	void *mac_cbdata;
	deuce_ssh_mac_t mac_selected;
	void *comp_cbdata;
	deuce_ssh_comp_t comp_selected;

} *deuce_ssh_transport_state_t;

int deuce_ssh_transport_init(deuce_ssh_session_t sess);
void deuce_ssh_transport_cleanup(deuce_ssh_session_t sess);
int deuce_ssh_transport_register_kex(deuce_ssh_kex_t kex);
int deuce_ssh_transport_register_key_algo(deuce_ssh_key_algo_t key_algo);
int deuce_ssh_transport_register_enc(deuce_ssh_enc_t enc);
int deuce_ssh_transport_register_mac(deuce_ssh_mac_t mac);
int deuce_ssh_transport_register_comp(deuce_ssh_comp_t comp);
int deuce_ssh_transport_register_lang(deuce_ssh_language_t lang);

#endif
