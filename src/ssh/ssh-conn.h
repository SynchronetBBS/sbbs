// RFC 4254: SSH Connection Protocol

#ifndef DEUCE_SSH_CONN_H
#define DEUCE_SSH_CONN_H

#include "deucessh.h"

#define SSH_MSG_GLOBAL_REQUEST            UINT8_C(80)
#define SSH_MSG_REQUEST_SUCCESS           UINT8_C(81)
#define SSH_MSG_REQUEST_FAILURE           UINT8_C(82)
#define SSH_MSG_CHANNEL_OPEN              UINT8_C(90)
#define SSH_MSG_CHANNEL_OPEN_CONFIRMATION UINT8_C(91)
#define SSH_MSG_CHANNEL_OPEN_FAILURE      UINT8_C(92)
#define SSH_MSG_CHANNEL_WINDOW_ADJUST     UINT8_C(93)
#define SSH_MSG_CHANNEL_DATA              UINT8_C(94)
#define SSH_MSG_CHANNEL_EXTENDED_DATA     UINT8_C(95)
#define SSH_MSG_CHANNEL_EOF               UINT8_C(96)
#define SSH_MSG_CHANNEL_CLOSE             UINT8_C(97)
#define SSH_MSG_CHANNEL_REQUEST           UINT8_C(98)
#define SSH_MSG_CHANNEL_SUCCESS           UINT8_C(99)
#define SSH_MSG_CHANNEL_FAILURE           UINT8_C(100)

typedef struct deuce_ssh_channel_s {
	uint32_t local_id;
	uint32_t remote_id;
	uint32_t local_window;
	uint32_t remote_window;
	uint32_t remote_max_packet;
	bool open;
} *deuce_ssh_channel;

int deuce_ssh_conn_open_session(deuce_ssh_session sess,
    struct deuce_ssh_channel_s *ch, uint32_t local_id);
int deuce_ssh_conn_request_exec(deuce_ssh_session sess,
    struct deuce_ssh_channel_s *ch, const char *command);
int deuce_ssh_conn_send_data(deuce_ssh_session sess,
    struct deuce_ssh_channel_s *ch, const uint8_t *data, size_t len);
int deuce_ssh_conn_recv(deuce_ssh_session sess,
    struct deuce_ssh_channel_s *ch, uint8_t *msg_type,
    uint8_t **data, size_t *data_len);
int deuce_ssh_conn_send_window_adjust(deuce_ssh_session sess,
    struct deuce_ssh_channel_s *ch, uint32_t bytes);
int deuce_ssh_conn_close(deuce_ssh_session sess,
    struct deuce_ssh_channel_s *ch);

#endif
