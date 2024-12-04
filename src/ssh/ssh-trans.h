// RFC-4253

#include "ssh.h"
#include "ssh-arch.h"

#ifndef DEUCE_SSH_TRANS_H
#define DEUCE_SSH_TRANS_H

/* Transport layer generic */
#define SSH_MSG_DISCONNECT      1
#define SSH_MSG_IGNORE          2
#define SSH_MSG_UNIMPLEMENTED   3
#define SSH_MSG_DEBUG           4
#define SSH_MSG_SERVICE_REQUEST 5
#define SSH_MSG_SERVICE_ACCEPT  6
/* Transport layer Algorithm negotiation */
#define SSH_MSG_KEXINIT         20
#define SSH_MSG_NEWKEYS         21

/* SSH_MSG_DISCONNECT reason codes */
#define SSH_DISCONNECT_HOST_NOT_ALLOWED_TO_CONNECT     1
#define SSH_DISCONNECT_PROTOCOL_ERROR                  2
#define SSH_DISCONNECT_KEY_EXCHANGE_FAILED             3
#define SSH_DISCONNECT_RESERVED                        4
#define SSH_DISCONNECT_MAC_ERROR                       5
#define SSH_DISCONNECT_COMPRESSION_ERROR               6
#define SSH_DISCONNECT_SERVICE_NOT_AVAILABLE           7
#define SSH_DISCONNECT_PROTOCOL_VERSION_NOT_SUPPORTED  8
#define SSH_DISCONNECT_HOST_KEY_NOT_VERIFIABLE         9
#define SSH_DISCONNECT_CONNECTION_LOST                10
#define SSH_DISCONNECT_BY_APPLICATION                 11
#define SSH_DISCONNECT_TOO_MANY_CONNECTIONS           12
#define SSH_DISCONNECT_AUTH_CANCELLED_BY_USER         13
#define SSH_DISCONNECT_NO_MORE_AUTH_METHODS_AVAILABLE 14
#define SSH_DISCONNECT_ILLEGAL_USER_NAME              15

typedef struct deuce_ssh_transport_packet {
	deuce_ssh_string_t payload;
	deuce_ssh_string_t random_padding;
	deuce_ssh_string_t mac;
	deuce_ssh_uint32_t packet_length;
	deuce_ssh_byte_t   padding;
} *deuce_ssh_transport_packet_t;

typedef int (*deuce_ssh_kex_handler_t)(deuce_ssh_transport_packet_t pkt, deuce_ssh_session_t sess);
typedef void (*deuce_ssh_kex_cleanup_t)(deuce_ssh_session_t sess);

typedef struct deuce_ssh_transport_state {
	uint32_t    tx_seq;
	uint32_t    rx_seq;
	bool        client;

	/* Transport options */
	thrd_t transport_thread;

	/* KEX options */
	void *kex_cbdata;
	uint32_t kex_state;
	size_t kex_selected;

	/* KEX outputs */
	size_t shared_secret_sz;
	uint8_t *shared_secret;
	size_t exchange_hash_sz;
	uint8_t *exchange_hash;

	size_t enc_selected;
	size_t mac_selected;
	size_t comp_selected;

} *deuce_ssh_transport_state_t;

int deuce_ssh_transport_init(deuce_ssh_session_t sess);
void deuce_ssh_transport_cleanup(deuce_ssh_session_t sess);

#endif
