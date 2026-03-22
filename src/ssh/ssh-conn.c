// RFC 4254: SSH Connection Protocol

#include <string.h>

#include "deucessh.h"
#include "ssh-conn.h"

#define INITIAL_WINDOW_SIZE  0x200000
#define MAX_PACKET_SIZE      0x8000

int
deuce_ssh_conn_open_session(deuce_ssh_session sess,
    struct deuce_ssh_channel_s *ch, uint32_t local_id)
{
	static const char type[] = "session";
	uint8_t msg[256];
	size_t pos = 0;

	ch->local_id = local_id;
	ch->local_window = INITIAL_WINDOW_SIZE;
	ch->open = false;

	msg[pos++] = SSH_MSG_CHANNEL_OPEN;
	deuce_ssh_serialize_uint32((uint32_t)(sizeof(type) - 1), msg, sizeof(msg), &pos);
	memcpy(&msg[pos], type, sizeof(type) - 1);
	pos += sizeof(type) - 1;
	deuce_ssh_serialize_uint32(ch->local_id, msg, sizeof(msg), &pos);
	deuce_ssh_serialize_uint32(ch->local_window, msg, sizeof(msg), &pos);
	deuce_ssh_serialize_uint32(MAX_PACKET_SIZE, msg, sizeof(msg), &pos);

	int res = deuce_ssh_transport_send_packet(sess, msg, pos, NULL);
	if (res < 0)
		return res;

	uint8_t msg_type;
	uint8_t *payload;
	size_t payload_len;
	res = deuce_ssh_transport_recv_packet(sess, &msg_type, &payload, &payload_len);
	if (res < 0)
		return res;
	if (msg_type != SSH_MSG_CHANNEL_OPEN_CONFIRMATION)
		return DEUCE_SSH_ERROR_INIT;

	/* Parse: recipient_channel(4) sender_channel(4) window(4) max_packet(4) */
	if (payload_len < 1 + 16)
		return DEUCE_SSH_ERROR_PARSE;
	size_t rpos = 1;
	uint32_t tmp;
	deuce_ssh_parse_uint32(&payload[rpos], payload_len - rpos, &tmp);
	rpos += 4;
	deuce_ssh_parse_uint32(&payload[rpos], payload_len - rpos, &ch->remote_id);
	rpos += 4;
	deuce_ssh_parse_uint32(&payload[rpos], payload_len - rpos, &ch->remote_window);
	rpos += 4;
	deuce_ssh_parse_uint32(&payload[rpos], payload_len - rpos, &ch->remote_max_packet);

	ch->open = true;
	return 0;
}

int
deuce_ssh_conn_request_exec(deuce_ssh_session sess,
    struct deuce_ssh_channel_s *ch, const char *command)
{
	static const char req[] = "exec";
	size_t cmdlen = strlen(command);
	size_t msg_len = 1 + 4 + 4 + (sizeof(req) - 1) + 1 + 4 + cmdlen;
	uint8_t *msg = malloc(msg_len);
	if (msg == NULL)
		return DEUCE_SSH_ERROR_ALLOC;
	size_t pos = 0;

	msg[pos++] = SSH_MSG_CHANNEL_REQUEST;
	deuce_ssh_serialize_uint32(ch->remote_id, msg, msg_len, &pos);
	deuce_ssh_serialize_uint32((uint32_t)(sizeof(req) - 1), msg, msg_len, &pos);
	memcpy(&msg[pos], req, sizeof(req) - 1);
	pos += sizeof(req) - 1;
	msg[pos++] = 1; /* want_reply = TRUE */
	deuce_ssh_serialize_uint32((uint32_t)cmdlen, msg, msg_len, &pos);
	memcpy(&msg[pos], command, cmdlen);
	pos += cmdlen;

	int res = deuce_ssh_transport_send_packet(sess, msg, pos, NULL);
	free(msg);
	if (res < 0)
		return res;

	uint8_t msg_type;
	uint8_t *payload;
	size_t payload_len;
	for (;;) {
		res = deuce_ssh_transport_recv_packet(sess, &msg_type, &payload, &payload_len);
		if (res < 0)
			return res;
		if (msg_type == SSH_MSG_CHANNEL_WINDOW_ADJUST) {
			if (payload_len < 1 + 8)
				return DEUCE_SSH_ERROR_PARSE;
			uint32_t bytes;
			deuce_ssh_parse_uint32(&payload[5], payload_len - 5, &bytes);
			ch->remote_window += bytes;
			continue;
		}
		break;
	}

	if (msg_type == SSH_MSG_CHANNEL_SUCCESS)
		return 0;
	return DEUCE_SSH_ERROR_INIT;
}

int
deuce_ssh_conn_send_data(deuce_ssh_session sess,
    struct deuce_ssh_channel_s *ch, const uint8_t *data, size_t len)
{
	if (len > ch->remote_window || len > ch->remote_max_packet)
		return DEUCE_SSH_ERROR_TOOLONG;

	size_t msg_len = 9 + len;
	uint8_t *msg = malloc(msg_len);
	if (msg == NULL)
		return DEUCE_SSH_ERROR_ALLOC;

	size_t pos = 0;
	msg[pos++] = SSH_MSG_CHANNEL_DATA;
	deuce_ssh_serialize_uint32(ch->remote_id, msg, msg_len, &pos);
	deuce_ssh_serialize_uint32((uint32_t)len, msg, msg_len, &pos);
	memcpy(&msg[pos], data, len);
	pos += len;

	ch->remote_window -= (uint32_t)len;
	int res = deuce_ssh_transport_send_packet(sess, msg, pos, NULL);
	free(msg);
	return res;
}

int
deuce_ssh_conn_recv(deuce_ssh_session sess,
    struct deuce_ssh_channel_s *ch, uint8_t *msg_type,
    uint8_t **data, size_t *data_len)
{
	uint8_t *payload;
	size_t payload_len;
	int res = deuce_ssh_transport_recv_packet(sess, msg_type, &payload, &payload_len);
	if (res < 0)
		return res;

	if (*msg_type == SSH_MSG_CHANNEL_DATA) {
		/* Parse: msg_type(1) + channel(4) + string data */
		if (payload_len < 1 + 4 + 4)
			return DEUCE_SSH_ERROR_PARSE;
		size_t rpos = 1 + 4;
		uint32_t dlen;
		deuce_ssh_parse_uint32(&payload[rpos], payload_len - rpos, &dlen);
		rpos += 4;
		if (rpos + dlen > payload_len)
			return DEUCE_SSH_ERROR_PARSE;
		*data = &payload[rpos];
		*data_len = dlen;
		ch->local_window -= dlen;
	}
	else if (*msg_type == SSH_MSG_CHANNEL_EXTENDED_DATA) {
		/* Parse: msg_type(1) + channel(4) + type(4) + string data */
		if (payload_len < 1 + 4 + 4 + 4)
			return DEUCE_SSH_ERROR_PARSE;
		size_t rpos = 1 + 4 + 4;
		uint32_t dlen;
		deuce_ssh_parse_uint32(&payload[rpos], payload_len - rpos, &dlen);
		rpos += 4;
		if (rpos + dlen > payload_len)
			return DEUCE_SSH_ERROR_PARSE;
		*data = &payload[rpos];
		*data_len = dlen;
		ch->local_window -= dlen;
	}
	else if (*msg_type == SSH_MSG_CHANNEL_WINDOW_ADJUST) {
		if (payload_len < 1 + 4 + 4)
			return DEUCE_SSH_ERROR_PARSE;
		uint32_t bytes;
		deuce_ssh_parse_uint32(&payload[5], payload_len - 5, &bytes);
		ch->remote_window += bytes;
		*data = NULL;
		*data_len = 0;
	}
	else {
		*data = payload;
		*data_len = payload_len;
	}

	return 0;
}

int
deuce_ssh_conn_send_window_adjust(deuce_ssh_session sess,
    struct deuce_ssh_channel_s *ch, uint32_t bytes)
{
	uint8_t msg[32];
	size_t pos = 0;

	msg[pos++] = SSH_MSG_CHANNEL_WINDOW_ADJUST;
	deuce_ssh_serialize_uint32(ch->remote_id, msg, sizeof(msg), &pos);
	deuce_ssh_serialize_uint32(bytes, msg, sizeof(msg), &pos);

	ch->local_window += bytes;
	return deuce_ssh_transport_send_packet(sess, msg, pos, NULL);
}

int
deuce_ssh_conn_close(deuce_ssh_session sess,
    struct deuce_ssh_channel_s *ch)
{
	uint8_t msg[8];
	size_t pos = 0;

	msg[pos++] = SSH_MSG_CHANNEL_CLOSE;
	deuce_ssh_serialize_uint32(ch->remote_id, msg, sizeof(msg), &pos);

	ch->open = false;
	return deuce_ssh_transport_send_packet(sess, msg, pos, NULL);
}
