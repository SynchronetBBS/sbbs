/*
 * Minimal SSH server: accepts one connection, handles ping/pong, exits.
 * Usage: server [port]
 */

#include <netinet/in.h>
#include <sys/socket.h>
#include <errno.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>
#include <unistd.h>

#include "deucessh.h"
#include "ssh-trans.h"
#include "ssh-auth.h"
#include "ssh-conn.h"
#include "kex/curve25519-sha256.h"
#include "kex/dh-gex-sha256.h"
#include "key_algo/ssh-ed25519.h"
#include "enc/none.h"
#include "enc/aes256-ctr.h"
#include "mac/none.h"
#include "mac/hmac-sha2-256.h"
#include "comp/none.h"

static int conn_fd = -1;
static struct deuce_ssh_session_s sess;

/* ================================================================
 * I/O callbacks
 * ================================================================ */

static int
tx(uint8_t *buf, size_t bufsz, atomic_bool *terminate, void *cbdata)
{
	size_t sent = 0;
	while (sent < bufsz && !(*terminate)) {
		ssize_t n = send(conn_fd, &buf[sent], bufsz - sent, 0);
		if (n > 0)
			sent += n;
		else if (n < 0 && errno != EAGAIN && errno != EINTR)
			return DEUCE_SSH_ERROR_INIT;
	}
	return 0;
}

static int
rx(uint8_t *buf, size_t bufsz, atomic_bool *terminate, void *cbdata)
{
	size_t got = 0;
	while (got < bufsz && !(*terminate)) {
		struct pollfd fd = { .fd = conn_fd, .events = POLLIN };
		int pr = poll(&fd, 1, 5000);
		if (pr < 0)
			return DEUCE_SSH_ERROR_INIT;
		if (pr == 0)
			continue;
		if (fd.revents & (POLLHUP | POLLERR) && !(fd.revents & POLLIN))
			return DEUCE_SSH_ERROR_INIT;
		ssize_t n = recv(conn_fd, &buf[got], bufsz - got, 0);
		if (n > 0)
			got += n;
		else if (n == 0)
			return DEUCE_SSH_ERROR_INIT;
		else if (errno != EAGAIN && errno != EINTR)
			return DEUCE_SSH_ERROR_INIT;
	}
	return 0;
}

static int
rxline(uint8_t *buf, size_t bufsz, size_t *bytes_received,
    atomic_bool *terminate, void *cbdata)
{
	size_t pos = 0;
	bool lastcr = false;
	while (!(*terminate)) {
		int res = rx(&buf[pos], 1, terminate, cbdata);
		if (res < 0)
			return res;
		if (buf[pos] == '\r')
			lastcr = true;
		if (buf[pos] == '\n' && lastcr) {
			*bytes_received = pos + 1;
			return 0;
		}
		if (pos + 1 < bufsz)
			pos++;
		else
			return DEUCE_SSH_ERROR_TOOLONG;
	}
	return DEUCE_SSH_ERROR_TERMINATED;
}

/* ================================================================
 * DH group provider — RFC 3526 group 14 (2048-bit MODP)
 * ================================================================ */

static const uint8_t group14_prime[] = {
	0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
	0xC9,0x0F,0xDA,0xA2,0x21,0x68,0xC2,0x34,
	0xC4,0xC6,0x62,0x8B,0x80,0xDC,0x1C,0xD1,
	0x29,0x02,0x4E,0x08,0x8A,0x67,0xCC,0x74,
	0x02,0x0B,0xBE,0xA6,0x3B,0x13,0x9B,0x22,
	0x51,0x4A,0x08,0x79,0x8E,0x34,0x04,0xDD,
	0xEF,0x95,0x19,0xB3,0xCD,0x3A,0x43,0x1B,
	0x30,0x2B,0x0A,0x6D,0xF2,0x5F,0x14,0x37,
	0x4F,0xE1,0x35,0x6D,0x6D,0x51,0xC2,0x45,
	0xE4,0x85,0xB5,0x76,0x62,0x5E,0x7E,0xC6,
	0xF4,0x4C,0x42,0xE9,0xA6,0x37,0xED,0x6B,
	0x0B,0xFF,0x5C,0xB6,0xF4,0x06,0xB7,0xED,
	0xEE,0x38,0x6B,0xFB,0x5A,0x89,0x9F,0xA5,
	0xAE,0x9F,0x24,0x11,0x7C,0x4B,0x1F,0xE6,
	0x49,0x28,0x66,0x51,0xEC,0xE4,0x5B,0x3D,
	0xC2,0x00,0x7C,0xB8,0xA1,0x63,0xBF,0x05,
	0x98,0xDA,0x48,0x36,0x1C,0x55,0xD3,0x9A,
	0x69,0x16,0x3F,0xA8,0xFD,0x24,0xCF,0x5F,
	0x83,0x65,0x5D,0x23,0xDC,0xA3,0xAD,0x96,
	0x1C,0x62,0xF3,0x56,0x20,0x85,0x52,0xBB,
	0x9E,0xD5,0x29,0x07,0x70,0x96,0x96,0x6D,
	0x67,0x0C,0x35,0x4E,0x4A,0xBC,0x98,0x04,
	0xF1,0x74,0x6C,0x08,0xCA,0x18,0x21,0x7C,
	0x32,0x90,0x5E,0x46,0x2E,0x36,0xCE,0x3B,
	0xE3,0x9E,0x77,0x2C,0x18,0x0E,0x86,0x03,
	0x9B,0x27,0x83,0xA2,0xEC,0x07,0xA2,0x8F,
	0xB5,0xC5,0x5D,0xF0,0x6F,0x4C,0x52,0xC9,
	0xDE,0x2B,0xCB,0xF6,0x95,0x58,0x17,0x18,
	0x39,0x95,0x49,0x7C,0xEA,0x95,0x6A,0xE5,
	0x15,0xD2,0x26,0x18,0x98,0xFA,0x05,0x10,
	0x15,0x72,0x8E,0x5A,0x8A,0xAC,0xAA,0x68,
	0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
};
static const uint8_t group14_generator[] = { 0x02 };

static int
select_dh_group(uint32_t min, uint32_t preferred, uint32_t max,
    uint8_t **p, size_t *p_len, uint8_t **g, size_t *g_len, void *cbdata)
{
	/* Only offer group 14 (2048-bit). A real server would select
	 * based on min/preferred/max from a moduli file. */
	if (min > 2048)
		return DEUCE_SSH_ERROR_INIT;
	*p = malloc(sizeof(group14_prime));
	*g = malloc(sizeof(group14_generator));
	if (*p == NULL || *g == NULL) {
		free(*p);
		free(*g);
		return DEUCE_SSH_ERROR_ALLOC;
	}
	memcpy(*p, group14_prime, sizeof(group14_prime));
	*p_len = sizeof(group14_prime);
	memcpy(*g, group14_generator, sizeof(group14_generator));
	*g_len = sizeof(group14_generator);
	return 0;
}

static struct deuce_ssh_dh_gex_provider dh_provider = {
	.select_group = select_dh_group,
	.cbdata = NULL,
};

/* ================================================================
 * Server-side auth callbacks
 * ================================================================ */

static int
auth_none(const uint8_t *username, size_t username_len, void *cbdata)
{
	/* Accept any user without credentials (testing) */
	return DEUCE_SSH_AUTH_SUCCESS;
}

static int
auth_password(const uint8_t *username, size_t username_len,
    const uint8_t *password, size_t password_len,
    uint8_t **change_prompt, size_t *change_prompt_len,
    void *cbdata)
{
	/* Accept any password (testing) */
	return DEUCE_SSH_AUTH_SUCCESS;
}

static struct deuce_ssh_auth_server_cbs auth_cbs = {
	.methods_str = "password",
	.none_cb = auth_none,
	.password_cb = auth_password,
};

static int
handle_channel(void)
{
	uint8_t msg_type;
	uint8_t *payload;
	size_t payload_len;
	int res;

	/* Receive CHANNEL_OPEN */
	res = deuce_ssh_transport_recv_packet(&sess, &msg_type, &payload, &payload_len);
	if (res < 0)
		return res;
	if (msg_type != SSH_MSG_CHANNEL_OPEN)
		return DEUCE_SSH_ERROR_PARSE;

	/* Parse sender_channel from the open request */
	/* Skip: msg_type(1) + string channel_type */
	size_t rpos = 1;
	uint32_t type_len;
	if (rpos + 4 > payload_len)
		return DEUCE_SSH_ERROR_PARSE;
	deuce_ssh_parse_uint32(&payload[rpos], payload_len - rpos, &type_len);
	rpos += 4 + type_len;

	if (rpos + 12 > payload_len)
		return DEUCE_SSH_ERROR_PARSE;
	uint32_t remote_channel;
	deuce_ssh_parse_uint32(&payload[rpos], payload_len - rpos, &remote_channel);
	rpos += 4;
	uint32_t remote_window;
	deuce_ssh_parse_uint32(&payload[rpos], payload_len - rpos, &remote_window);

	/* Send CHANNEL_OPEN_CONFIRMATION */
	{
		uint8_t confirm[32];
		size_t pos = 0;
		confirm[pos++] = SSH_MSG_CHANNEL_OPEN_CONFIRMATION;
		deuce_ssh_serialize_uint32(remote_channel, confirm, sizeof(confirm), &pos);
		deuce_ssh_serialize_uint32(0, confirm, sizeof(confirm), &pos); /* our channel */
		deuce_ssh_serialize_uint32(0x200000, confirm, sizeof(confirm), &pos); /* window */
		deuce_ssh_serialize_uint32(0x8000, confirm, sizeof(confirm), &pos); /* max packet */
		res = deuce_ssh_transport_send_packet(&sess, confirm, pos, NULL);
		if (res < 0)
			return res;
	}

	/* Receive CHANNEL_REQUEST (exec) */
	res = deuce_ssh_transport_recv_packet(&sess, &msg_type, &payload, &payload_len);
	if (res < 0)
		return res;
	if (msg_type != SSH_MSG_CHANNEL_REQUEST)
		return DEUCE_SSH_ERROR_PARSE;

	/* Parse: channel(4) + string request_type + bool want_reply + string command */
	rpos = 1 + 4; /* skip msg_type + channel */
	uint32_t req_len;
	if (rpos + 4 > payload_len)
		return DEUCE_SSH_ERROR_PARSE;
	deuce_ssh_parse_uint32(&payload[rpos], payload_len - rpos, &req_len);
	rpos += 4 + req_len + 1; /* skip request_type + want_reply */

	uint32_t cmd_len = 0;
	const uint8_t *cmd = NULL;
	if (rpos + 4 <= payload_len) {
		deuce_ssh_parse_uint32(&payload[rpos], payload_len - rpos, &cmd_len);
		rpos += 4;
		if (rpos + cmd_len <= payload_len)
			cmd = &payload[rpos];
	}

	/* Send CHANNEL_SUCCESS */
	{
		uint8_t success[8];
		size_t pos = 0;
		success[pos++] = SSH_MSG_CHANNEL_SUCCESS;
		deuce_ssh_serialize_uint32(remote_channel, success, sizeof(success), &pos);
		res = deuce_ssh_transport_send_packet(&sess, success, pos, NULL);
		if (res < 0)
			return res;
	}

	/* Handle the command */
	const char *response;
	if (cmd != NULL && cmd_len == 4 && memcmp(cmd, "ping", 4) == 0)
		response = "pong\n";
	else
		response = "unknown command\n";

	/* Send response as CHANNEL_DATA */
	size_t resp_len = strlen(response);
	size_t data_msg_len = 1 + 4 + 4 + resp_len;
	uint8_t *data_msg = malloc(data_msg_len);
	if (data_msg == NULL)
		return DEUCE_SSH_ERROR_ALLOC;
	{
		size_t pos = 0;
		data_msg[pos++] = SSH_MSG_CHANNEL_DATA;
		deuce_ssh_serialize_uint32(remote_channel, data_msg, data_msg_len, &pos);
		deuce_ssh_serialize_uint32((uint32_t)resp_len, data_msg, data_msg_len, &pos);
		memcpy(&data_msg[pos], response, resp_len);
		pos += resp_len;
		res = deuce_ssh_transport_send_packet(&sess, data_msg, pos, NULL);
	}
	free(data_msg);
	if (res < 0)
		return res;

	/* Send exit-status (0) so the client knows the command succeeded */
	{
		static const char req[] = "exit-status";
		uint8_t status_msg[32];
		size_t pos = 0;
		status_msg[pos++] = SSH_MSG_CHANNEL_REQUEST;
		deuce_ssh_serialize_uint32(remote_channel, status_msg, sizeof(status_msg), &pos);
		deuce_ssh_serialize_uint32(sizeof(req) - 1, status_msg, sizeof(status_msg), &pos);
		memcpy(&status_msg[pos], req, sizeof(req) - 1);
		pos += sizeof(req) - 1;
		status_msg[pos++] = 0; /* want_reply = false */
		deuce_ssh_serialize_uint32(0, status_msg, sizeof(status_msg), &pos); /* exit code 0 */
		res = deuce_ssh_transport_send_packet(&sess, status_msg, pos, NULL);
		if (res < 0)
			return res;
	}

	/* Send CHANNEL_EOF */
	{
		uint8_t eof[8];
		size_t pos = 0;
		eof[pos++] = SSH_MSG_CHANNEL_EOF;
		deuce_ssh_serialize_uint32(remote_channel, eof, sizeof(eof), &pos);
		res = deuce_ssh_transport_send_packet(&sess, eof, pos, NULL);
		if (res < 0)
			return res;
	}

	/* Send CHANNEL_CLOSE */
	{
		uint8_t cls[8];
		size_t pos = 0;
		cls[pos++] = SSH_MSG_CHANNEL_CLOSE;
		deuce_ssh_serialize_uint32(remote_channel, cls, sizeof(cls), &pos);
		res = deuce_ssh_transport_send_packet(&sess, cls, pos, NULL);
		if (res < 0)
			return res;
	}

	/* Wait for client's CHANNEL_CLOSE (may receive EOF or other messages first) */
	for (;;) {
		res = deuce_ssh_transport_recv_packet(&sess, &msg_type, &payload, &payload_len);
		if (res < 0)
			return res;
		if (msg_type == SSH_MSG_CHANNEL_CLOSE)
			break;
	}

	return 0;
}

/* ================================================================
 * Main
 * ================================================================ */

int
main(int argc, char **argv)
{
	int port = 2222;
	if (argc > 1)
		port = atoi(argv[1]);

	/* Register algorithms */
	if (deuce_ssh_transport_set_callbacks(tx, rx, rxline, NULL) != 0)
		return 1;
	if (register_curve25519_sha256() != 0) return 1;
	if (register_dh_gex_sha256() != 0) return 1;
	if (register_ssh_ed25519() != 0) return 1;
	if (register_aes256_ctr() != 0) return 1;
	if (register_none_enc() != 0) return 1;
	if (register_hmac_sha2_256() != 0) return 1;
	if (register_none_mac() != 0) return 1;
	if (register_none_comp() != 0) return 1;

	/* Listen */
	int listen_fd = socket(PF_INET, SOCK_STREAM, 0);
	if (listen_fd == -1) {
		perror("socket");
		return 1;
	}
	int opt = 1;
	setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

	struct sockaddr_in sa = {
		.sin_family = AF_INET,
		.sin_port = htons(port),
		.sin_addr = { htonl(INADDR_ANY) },
	};
	if (bind(listen_fd, (struct sockaddr *)&sa, sizeof(sa)) == -1) {
		perror("bind");
		return 1;
	}
	if (listen(listen_fd, 1) == -1) {
		perror("listen");
		return 1;
	}
	fprintf(stderr, "Listening on port %d...\n", port);

	conn_fd = accept(listen_fd, NULL, NULL);
	close(listen_fd);
	if (conn_fd == -1) {
		perror("accept");
		return 1;
	}
	fprintf(stderr, "Connection accepted.\n");

	/* Initialize session */
	memset(&sess, 0, sizeof(sess));
	sess.trans.client = false;
	int res = deuce_ssh_session_init(&sess);
	if (res < 0) {
		fprintf(stderr, "session_init failed: %d\n", res);
		return 1;
	}

	/* Generate ephemeral host key and set DH group provider */
	res = ssh_ed25519_generate_key(&sess);
	if (res < 0) {
		fprintf(stderr, "host key generation failed: %d\n", res);
		return 1;
	}

	deuce_ssh_dh_gex_set_provider(&sess, &dh_provider);

	/* Handshake */
	fprintf(stderr, "Version exchange...\n");
	res = deuce_ssh_transport_version_exchange(&sess);
	if (res < 0) {
		fprintf(stderr, "version_exchange failed: %d\n", res);
		return 1;
	}
	fprintf(stderr, "Remote: %s\n", deuce_ssh_transport_get_remote_version(&sess));

	fprintf(stderr, "KEXINIT...\n");
	res = deuce_ssh_transport_kexinit(&sess);
	if (res < 0) {
		fprintf(stderr, "kexinit failed: %d\n", res);
		return 1;
	}
	fprintf(stderr, "KEX: %s, Host key: %s, Enc: %s, MAC: %s\n",
	    deuce_ssh_transport_get_kex_name(&sess),
	    deuce_ssh_transport_get_hostkey_name(&sess),
	    deuce_ssh_transport_get_enc_name(&sess),
	    deuce_ssh_transport_get_mac_name(&sess));

	fprintf(stderr, "Key exchange...\n");
	res = deuce_ssh_transport_kex(&sess);
	if (res < 0) {
		fprintf(stderr, "kex failed: %d\n", res);
		return 1;
	}

	fprintf(stderr, "NEWKEYS...\n");
	res = deuce_ssh_transport_newkeys(&sess);
	if (res < 0) {
		fprintf(stderr, "newkeys failed: %d\n", res);
		return 1;
	}
	fprintf(stderr, "Encrypted transport established.\n");

	/* Auth */
	fprintf(stderr, "Handling auth...\n");
	uint8_t auth_user[256];
	size_t auth_user_len;
	res = deuce_ssh_auth_server(&sess, &auth_cbs, auth_user, &auth_user_len);
	if (res < 0) {
		fprintf(stderr, "auth failed: %d\n", res);
		return 1;
	}
	fprintf(stderr, "Client authenticated.\n");

	/* Channel */
	fprintf(stderr, "Handling channel...\n");
	res = handle_channel();
	if (res < 0) {
		fprintf(stderr, "channel failed: %d\n", res);
		return 1;
	}
	fprintf(stderr, "Channel closed.\n");

	/* Clean shutdown */
	deuce_ssh_session_cleanup(&sess);
	close(conn_fd);
	fprintf(stderr, "Server shutdown complete.\n");
	return 0;
}
