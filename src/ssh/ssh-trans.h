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

typedef int (*deuce_ssh_mac_generate_t)(uint8_t *key, uint8_t *buf, uint8_t *bufsz, uint8_t *outbuf, deuce_ssh_session_t sess);
typedef void (*deuce_ssh_mac_cleanup_t)(deuce_ssh_session_t sess);

typedef int (*deuce_ssh_comp_compress_t)(uint8_t *buf, uint8_t *bufsz, deuce_ssh_session_t sess);
typedef int (*deuce_ssh_comp_uncompress_t)(uint8_t *buf, uint8_t *bufsz, deuce_ssh_session_t sess);
typedef void (*deuce_ssh_comp_cleanup_t)(deuce_ssh_session_t sess);

typedef struct deuce_ssh_transport_state {
	uint32_t    tx_seq;
	uint32_t    rx_seq;
	bool        client;

	/* Transport options */
	thrd_t transport_thread;

	/* KEX options */
	void *kex_cbdata;
	size_t kex_selected;

	/* KEX outputs */
	size_t shared_secret_sz;
	uint8_t *shared_secret;
	size_t exchange_hash_sz;
	uint8_t *exchange_hash;

	/* Public Key Algorithms */
	void *key_algo_cbdata;
	size_t key_algo_selected;

	void *enc_cbdata;
	size_t enc_selected;
	void *mac_cbdata;
	size_t mac_selected;
	void *comp_cbdata;
	size_t comp_selected;

} *deuce_ssh_transport_state_t;

int deuce_ssh_transport_init(deuce_ssh_session_t sess);
void deuce_ssh_transport_cleanup(deuce_ssh_session_t sess);

#endif
