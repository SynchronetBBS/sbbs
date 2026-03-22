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

/*
 * Channel state.  Allocate one per channel.  The caller provides
 * the local_id (must be unique within the session).  The remaining
 * fields are populated by deuce_ssh_conn_open_session().
 */
typedef struct deuce_ssh_channel_s {
	uint32_t local_id;
	uint32_t remote_id;
	uint32_t local_window;
	uint32_t remote_window;
	uint32_t remote_max_packet;
	bool open;
} *deuce_ssh_channel;

/*
 * Open a "session" channel (RFC 4254 s6.1).
 * Sends CHANNEL_OPEN and waits for CHANNEL_OPEN_CONFIRMATION.
 * Populates ch->remote_id, remote_window, remote_max_packet.
 * Returns 0 on success.
 */
int deuce_ssh_conn_open_session(deuce_ssh_session sess,
    struct deuce_ssh_channel_s *ch, uint32_t local_id);

/*
 * Request remote command execution on an open channel (RFC 4254 s6.5).
 * Sends CHANNEL_REQUEST "exec" with want_reply=true and waits for
 * CHANNEL_SUCCESS.  Handles WINDOW_ADJUST received while waiting.
 * Returns 0 on success.
 */
int deuce_ssh_conn_request_exec(deuce_ssh_session sess,
    struct deuce_ssh_channel_s *ch, const char *command);

/*
 * Send channel data (RFC 4254 s5.2).
 * Checks that len does not exceed remote_window or remote_max_packet.
 * Decrements remote_window by len.  Returns 0 on success.
 */
int deuce_ssh_conn_send_data(deuce_ssh_session sess,
    struct deuce_ssh_channel_s *ch, const uint8_t *data, size_t len);

/*
 * Receive the next channel message.
 * On return, *msg_type is the SSH message type.  For CHANNEL_DATA
 * and CHANNEL_EXTENDED_DATA, *data and *data_len point to the
 * payload content (valid until the next recv call).  For
 * CHANNEL_WINDOW_ADJUST, the channel's remote_window is updated
 * automatically.  For other message types (EOF, CLOSE, etc.),
 * *data points to the raw payload.
 */
int deuce_ssh_conn_recv(deuce_ssh_session sess,
    struct deuce_ssh_channel_s *ch, uint8_t *msg_type,
    uint8_t **data, size_t *data_len);

/*
 * Send CHANNEL_WINDOW_ADJUST to increase the peer's send window.
 * Adds bytes to local_window.  Call this when local_window gets low.
 */
int deuce_ssh_conn_send_window_adjust(deuce_ssh_session sess,
    struct deuce_ssh_channel_s *ch, uint32_t bytes);

/*
 * Send CHANNEL_CLOSE for a channel.  Sets ch->open to false.
 * The peer should respond with its own CLOSE.
 */
int deuce_ssh_conn_close(deuce_ssh_session sess,
    struct deuce_ssh_channel_s *ch);

#endif
