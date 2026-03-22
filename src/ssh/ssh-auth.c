// RFC 4252: SSH Authentication Protocol

#include <string.h>
#include <stdlib.h>

#include "deucessh.h"
#include "ssh-auth.h"

int
deuce_ssh_auth_request_service(deuce_ssh_session sess, const char *service)
{
	size_t slen = strlen(service);
	size_t msg_len = 1 + 4 + slen;
	uint8_t *msg = malloc(msg_len);
	if (msg == NULL)
		return DEUCE_SSH_ERROR_ALLOC;
	size_t pos = 0;

	msg[pos++] = SSH_MSG_SERVICE_REQUEST;
	deuce_ssh_serialize_uint32((uint32_t)slen, msg, msg_len, &pos);
	memcpy(&msg[pos], service, slen);
	pos += slen;

	int res = deuce_ssh_transport_send_packet(sess, msg, pos, NULL);
	free(msg);
	if (res < 0)
		return res;

	uint8_t msg_type;
	uint8_t *payload;
	size_t payload_len;
	res = deuce_ssh_transport_recv_packet(sess, &msg_type, &payload, &payload_len);
	if (res < 0)
		return res;
	if (msg_type != SSH_MSG_SERVICE_ACCEPT)
		return DEUCE_SSH_ERROR_INIT;

	return 0;
}

int
deuce_ssh_auth_get_methods(deuce_ssh_session sess,
    const char *username, char *methods, size_t methods_sz)
{
	size_t ulen = strlen(username);
	static const char service[] = "ssh-connection";
	static const char method[] = "none";
	size_t msg_len = 1 + 4 + ulen + 4 + (sizeof(service) - 1) + 4 + (sizeof(method) - 1);
	uint8_t *msg = malloc(msg_len);
	if (msg == NULL)
		return DEUCE_SSH_ERROR_ALLOC;
	size_t pos = 0;

	msg[pos++] = SSH_MSG_USERAUTH_REQUEST;
	deuce_ssh_serialize_uint32((uint32_t)ulen, msg, msg_len, &pos);
	memcpy(&msg[pos], username, ulen);
	pos += ulen;
	deuce_ssh_serialize_uint32((uint32_t)(sizeof(service) - 1), msg, msg_len, &pos);
	memcpy(&msg[pos], service, sizeof(service) - 1);
	pos += sizeof(service) - 1;
	deuce_ssh_serialize_uint32((uint32_t)(sizeof(method) - 1), msg, msg_len, &pos);
	memcpy(&msg[pos], method, sizeof(method) - 1);
	pos += sizeof(method) - 1;

	int res = deuce_ssh_transport_send_packet(sess, msg, pos, NULL);
	free(msg);
	if (res < 0)
		return res;

	uint8_t msg_type;
	uint8_t *payload;
	size_t payload_len;
	res = deuce_ssh_transport_recv_packet(sess, &msg_type, &payload, &payload_len);
	if (res < 0)
		return res;

	if (msg_type == SSH_MSG_USERAUTH_SUCCESS) {
		if (methods_sz > 0)
			methods[0] = 0;
		return 0;
	}

	if (msg_type == SSH_MSG_USERAUTH_FAILURE) {
		if (payload_len < 1 + 4)
			return DEUCE_SSH_ERROR_PARSE;
		uint32_t mlen;
		deuce_ssh_parse_uint32(&payload[1], payload_len - 1, &mlen);
		if (1 + 4 + mlen > payload_len)
			return DEUCE_SSH_ERROR_PARSE;
		/* RFC 4251 s6: names MUST NOT contain control chars or DEL */
		for (uint32_t j = 0; j < mlen; j++) {
			uint8_t ch = payload[5 + j];
			if (ch <= ' ' || ch >= 127)
				return DEUCE_SSH_ERROR_PARSE;
		}
		size_t copylen = mlen < methods_sz - 1 ? mlen : methods_sz - 1;
		memcpy(methods, &payload[5], copylen);
		methods[copylen] = 0;
		return 1;
	}

	return DEUCE_SSH_ERROR_INIT;
}

int
deuce_ssh_auth_password(deuce_ssh_session sess,
    const char *username, const char *password)
{
	size_t ulen = strlen(username);
	size_t plen = strlen(password);
	static const char service[] = "ssh-connection";
	static const char method[] = "password";
	size_t msg_len = 1 + 4 + ulen + 4 + (sizeof(service) - 1) +
	    4 + (sizeof(method) - 1) + 1 + 4 + plen;
	uint8_t *msg = malloc(msg_len);
	if (msg == NULL)
		return DEUCE_SSH_ERROR_ALLOC;
	size_t pos = 0;

	msg[pos++] = SSH_MSG_USERAUTH_REQUEST;
	deuce_ssh_serialize_uint32((uint32_t)ulen, msg, msg_len, &pos);
	memcpy(&msg[pos], username, ulen);
	pos += ulen;
	deuce_ssh_serialize_uint32((uint32_t)(sizeof(service) - 1), msg, msg_len, &pos);
	memcpy(&msg[pos], service, sizeof(service) - 1);
	pos += sizeof(service) - 1;
	deuce_ssh_serialize_uint32((uint32_t)(sizeof(method) - 1), msg, msg_len, &pos);
	memcpy(&msg[pos], method, sizeof(method) - 1);
	pos += sizeof(method) - 1;
	msg[pos++] = 0; /* no password change */
	deuce_ssh_serialize_uint32((uint32_t)plen, msg, msg_len, &pos);
	memcpy(&msg[pos], password, plen);
	pos += plen;

	int res = deuce_ssh_transport_send_packet(sess, msg, pos, NULL);
	free(msg);
	if (res < 0)
		return res;

	uint8_t msg_type;
	uint8_t *payload;
	size_t payload_len;
	res = deuce_ssh_transport_recv_packet(sess, &msg_type, &payload, &payload_len);
	if (res < 0)
		return res;

	while (msg_type == SSH_MSG_USERAUTH_BANNER) {
		res = deuce_ssh_transport_recv_packet(sess, &msg_type, &payload, &payload_len);
		if (res < 0)
			return res;
	}

	if (msg_type == SSH_MSG_USERAUTH_SUCCESS)
		return 0;

	return DEUCE_SSH_ERROR_INIT;
}

int
deuce_ssh_auth_keyboard_interactive(deuce_ssh_session sess,
    const char *username, const char *password)
{
	size_t ulen = strlen(username);
	size_t plen = strlen(password);
	static const char service[] = "ssh-connection";
	static const char method[] = "keyboard-interactive";
	size_t msg_len = 1 + 4 + ulen + 4 + (sizeof(service) - 1) +
	    4 + (sizeof(method) - 1) + 4 + 4;
	uint8_t *msg = malloc(msg_len);
	if (msg == NULL)
		return DEUCE_SSH_ERROR_ALLOC;
	size_t pos = 0;

	msg[pos++] = SSH_MSG_USERAUTH_REQUEST;
	deuce_ssh_serialize_uint32((uint32_t)ulen, msg, msg_len, &pos);
	memcpy(&msg[pos], username, ulen);
	pos += ulen;
	deuce_ssh_serialize_uint32((uint32_t)(sizeof(service) - 1), msg, msg_len, &pos);
	memcpy(&msg[pos], service, sizeof(service) - 1);
	pos += sizeof(service) - 1;
	deuce_ssh_serialize_uint32((uint32_t)(sizeof(method) - 1), msg, msg_len, &pos);
	memcpy(&msg[pos], method, sizeof(method) - 1);
	pos += sizeof(method) - 1;
	deuce_ssh_serialize_uint32(0, msg, msg_len, &pos); /* language */
	deuce_ssh_serialize_uint32(0, msg, msg_len, &pos); /* submethods */

	int res = deuce_ssh_transport_send_packet(sess, msg, pos, NULL);
	free(msg);
	if (res < 0)
		return res;

	for (;;) {
		uint8_t msg_type;
		uint8_t *payload;
		size_t payload_len;
		res = deuce_ssh_transport_recv_packet(sess, &msg_type, &payload, &payload_len);
		if (res < 0)
			return res;

		if (msg_type == SSH_MSG_USERAUTH_BANNER)
			continue;

		if (msg_type == SSH_MSG_USERAUTH_SUCCESS)
			return 0;

		if (msg_type == SSH_MSG_USERAUTH_FAILURE)
			return DEUCE_SSH_ERROR_INIT;

		if (msg_type == SSH_MSG_USERAUTH_INFO_REQUEST) {
			size_t rpos = 1;
			uint32_t slen;

			/* Skip name string */
			if (rpos + 4 > payload_len)
				return DEUCE_SSH_ERROR_PARSE;
			deuce_ssh_parse_uint32(&payload[rpos], payload_len - rpos, &slen);
			rpos += 4;
			if (rpos + slen > payload_len)
				return DEUCE_SSH_ERROR_PARSE;
			rpos += slen;

			/* Skip instruction string */
			if (rpos + 4 > payload_len)
				return DEUCE_SSH_ERROR_PARSE;
			deuce_ssh_parse_uint32(&payload[rpos], payload_len - rpos, &slen);
			rpos += 4;
			if (rpos + slen > payload_len)
				return DEUCE_SSH_ERROR_PARSE;
			rpos += slen;

			/* Skip language string */
			if (rpos + 4 > payload_len)
				return DEUCE_SSH_ERROR_PARSE;
			deuce_ssh_parse_uint32(&payload[rpos], payload_len - rpos, &slen);
			rpos += 4;
			if (rpos + slen > payload_len)
				return DEUCE_SSH_ERROR_PARSE;
			rpos += slen;

			/* num_prompts */
			if (rpos + 4 > payload_len)
				return DEUCE_SSH_ERROR_PARSE;
			uint32_t num_prompts;
			deuce_ssh_parse_uint32(&payload[rpos], payload_len - rpos, &num_prompts);
			rpos += 4;

			/* Build response */
			size_t resp_len = 1 + 4 + num_prompts * (4 + plen);
			uint8_t *resp = malloc(resp_len);
			if (resp == NULL)
				return DEUCE_SSH_ERROR_ALLOC;
			size_t rp = 0;
			resp[rp++] = SSH_MSG_USERAUTH_INFO_RESPONSE;
			deuce_ssh_serialize_uint32(num_prompts, resp, resp_len, &rp);
			for (uint32_t i = 0; i < num_prompts; i++) {
				/* Skip prompt string */
				if (rpos + 4 > payload_len) {
					free(resp);
					return DEUCE_SSH_ERROR_PARSE;
				}
				deuce_ssh_parse_uint32(&payload[rpos], payload_len - rpos, &slen);
				rpos += 4;
				if (rpos + slen + 1 > payload_len) {
					free(resp);
					return DEUCE_SSH_ERROR_PARSE;
				}
				rpos += slen + 1; /* string + echo boolean */

				/* Answer with password */
				deuce_ssh_serialize_uint32((uint32_t)plen, resp, resp_len, &rp);
				memcpy(&resp[rp], password, plen);
				rp += plen;
			}
			res = deuce_ssh_transport_send_packet(sess, resp, rp, NULL);
			free(resp);
			if (res < 0)
				return res;
			continue;
		}

		return DEUCE_SSH_ERROR_PARSE;
	}
}
