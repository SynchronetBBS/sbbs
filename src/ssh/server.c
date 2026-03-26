/*
 * Example SSH server using the high-level DeuceSSH API.
 * Accepts one connection, handles a single session, exits.
 * Usage: server [port]
 */

#include <errno.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "deucessh-algorithms.h"
#include "deucessh-auth.h"
#include "deucessh-conn.h"
#include "deucessh.h"

static int          conn_fd = -1;
static dssh_session sess;

/* ================================================================
 * I/O callbacks
 * ================================================================ */
static int
tx(uint8_t *buf, size_t bufsz, dssh_session s, void *cbdata)
{
	size_t sent = 0;

	while (sent < bufsz && !dssh_session_is_terminated(s)) {
		ssize_t n = send(conn_fd, &buf[sent], bufsz - sent, 0);

		if (n > 0)
			sent += n;
		else if ((n < 0) && (errno != EAGAIN) && (errno != EINTR))
			return DSSH_ERROR_INIT;
	}
	return 0;
}

static int
rx(uint8_t *buf, size_t bufsz, dssh_session s, void *cbdata)
{
	size_t got = 0;

	while (got < bufsz && !dssh_session_is_terminated(s)) {
		struct pollfd fd = {.fd = conn_fd, .events = POLLIN};
		int           pr = poll(&fd, 1, 5000);

		if (pr < 0)
			return DSSH_ERROR_INIT;
		if (pr == 0)
			continue;
		if (fd.revents & (POLLHUP | POLLERR) && !(fd.revents & POLLIN))
			return DSSH_ERROR_INIT;

		ssize_t n = recv(conn_fd, &buf[got], bufsz - got, 0);

		if (n > 0)
			got += n;
		else if (n == 0)
			return DSSH_ERROR_INIT;
		else if ((errno != EAGAIN) && (errno != EINTR))
			return DSSH_ERROR_INIT;
	}
	return 0;
}

static int
rxline(uint8_t *buf, size_t bufsz, size_t *bytes_received,
    dssh_session s, void *cbdata)
{
	size_t pos = 0;
	bool   lastcr = false;

	while (!dssh_session_is_terminated(s)) {
		int res = rx(&buf[pos], 1, s, cbdata);

		if (res < 0)
			return res;
		if (buf[pos] == '\r')
			lastcr = true;
		if ((buf[pos] == '\n') && lastcr) {
			*bytes_received = pos + 1;
			return 0;
		}
		if (pos + 1 < bufsz)
			pos++;
		else
			return DSSH_ERROR_TOOLONG;
	}
	return DSSH_ERROR_TERMINATED;
}

/* ================================================================
 * DH group provider — RFC 3526 group 14 (2048-bit MODP)
 * ================================================================ */

static const uint8_t group14_prime[] = {
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xC9, 0x0F, 0xDA, 0xA2, 0x21, 0x68, 0xC2, 0x34,
	0xC4, 0xC6, 0x62, 0x8B, 0x80, 0xDC, 0x1C, 0xD1,
	0x29, 0x02, 0x4E, 0x08, 0x8A, 0x67, 0xCC, 0x74,
	0x02, 0x0B, 0xBE, 0xA6, 0x3B, 0x13, 0x9B, 0x22,
	0x51, 0x4A, 0x08, 0x79, 0x8E, 0x34, 0x04, 0xDD,
	0xEF, 0x95, 0x19, 0xB3, 0xCD, 0x3A, 0x43, 0x1B,
	0x30, 0x2B, 0x0A, 0x6D, 0xF2, 0x5F, 0x14, 0x37,
	0x4F, 0xE1, 0x35, 0x6D, 0x6D, 0x51, 0xC2, 0x45,
	0xE4, 0x85, 0xB5, 0x76, 0x62, 0x5E, 0x7E, 0xC6,
	0xF4, 0x4C, 0x42, 0xE9, 0xA6, 0x37, 0xED, 0x6B,
	0x0B, 0xFF, 0x5C, 0xB6, 0xF4, 0x06, 0xB7, 0xED,
	0xEE, 0x38, 0x6B, 0xFB, 0x5A, 0x89, 0x9F, 0xA5,
	0xAE, 0x9F, 0x24, 0x11, 0x7C, 0x4B, 0x1F, 0xE6,
	0x49, 0x28, 0x66, 0x51, 0xEC, 0xE4, 0x5B, 0x3D,
	0xC2, 0x00, 0x7C, 0xB8, 0xA1, 0x63, 0xBF, 0x05,
	0x98, 0xDA, 0x48, 0x36, 0x1C, 0x55, 0xD3, 0x9A,
	0x69, 0x16, 0x3F, 0xA8, 0xFD, 0x24, 0xCF, 0x5F,
	0x83, 0x65, 0x5D, 0x23, 0xDC, 0xA3, 0xAD, 0x96,
	0x1C, 0x62, 0xF3, 0x56, 0x20, 0x85, 0x52, 0xBB,
	0x9E, 0xD5, 0x29, 0x07, 0x70, 0x96, 0x96, 0x6D,
	0x67, 0x0C, 0x35, 0x4E, 0x4A, 0xBC, 0x98, 0x04,
	0xF1, 0x74, 0x6C, 0x08, 0xCA, 0x18, 0x21, 0x7C,
	0x32, 0x90, 0x5E, 0x46, 0x2E, 0x36, 0xCE, 0x3B,
	0xE3, 0x9E, 0x77, 0x2C, 0x18, 0x0E, 0x86, 0x03,
	0x9B, 0x27, 0x83, 0xA2, 0xEC, 0x07, 0xA2, 0x8F,
	0xB5, 0xC5, 0x5D, 0xF0, 0x6F, 0x4C, 0x52, 0xC9,
	0xDE, 0x2B, 0xCB, 0xF6, 0x95, 0x58, 0x17, 0x18,
	0x39, 0x95, 0x49, 0x7C, 0xEA, 0x95, 0x6A, 0xE5,
	0x15, 0xD2, 0x26, 0x18, 0x98, 0xFA, 0x05, 0x10,
	0x15, 0x72, 0x8E, 0x5A, 0x8A, 0xAC, 0xAA, 0x68,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
};
static const uint8_t group14_generator[] = {0x02};

static int
select_dh_group(uint32_t min, uint32_t preferred, uint32_t max,
    uint8_t **p, size_t *p_len, uint8_t **g, size_t *g_len, void *cbdata)
{
	if (min > 2048)
		return DSSH_ERROR_INIT;
	*p = malloc(sizeof(group14_prime));
	*g = malloc(sizeof(group14_generator));
	if ((*p == NULL) || (*g == NULL)) {
		free(*p);
		free(*g);
		return DSSH_ERROR_ALLOC;
	}
	memcpy(*p, group14_prime, sizeof(group14_prime));
	*p_len = sizeof(group14_prime);
	memcpy(*g, group14_generator, sizeof(group14_generator));
	*g_len = sizeof(group14_generator);
	return 0;
}

static struct dssh_dh_gex_provider dh_provider = {
	.select_group = select_dh_group,
	.cbdata = NULL,
};

/* ================================================================
 * Auth callbacks
 * ================================================================ */
static int
auth_none(const uint8_t *username, size_t username_len, void *cbdata)
{
	return DSSH_AUTH_SUCCESS;
}

static int
auth_password(const uint8_t *username, size_t username_len,
    const uint8_t *password, size_t password_len,
    uint8_t **change_prompt, size_t *change_prompt_len,
    void *cbdata)
{
	return DSSH_AUTH_SUCCESS;
}

static struct dssh_auth_server_cbs auth_cbs = {
	.methods_str = "password",
	.none_cb = auth_none,
	.password_cb = auth_password,
};

/* ================================================================
 * Session channel request callback
 * ================================================================ */
static int
channel_request_cb(const char *type, size_t type_len,
    bool want_reply, const uint8_t *data, size_t data_len,
    void *cbdata)
{
	if ((type_len == 7) && (memcmp(type, "pty-req", 7) == 0)) {
		struct dssh_pty_req pty;

		if (dssh_parse_pty_req_data(data, data_len, &pty) == 0) {
			fprintf(stderr, "  PTY: %.*s %ux%u\n",
			    (int)strlen(pty.term), pty.term,
			    pty.cols, pty.rows);
		}
		return 0;
	}
	if ((type_len == 3) && (memcmp(type, "env", 3) == 0)) {
		const uint8_t *name, *value;
		size_t         nlen, vlen;

		if (dssh_parse_env_data(data, data_len,
		    &name, &nlen, &value, &vlen) == 0)
			fprintf(stderr, "  ENV: %.*s=%.*s\n",
			    (int)nlen, name, (int)vlen, value);
		return 0;
	}
	if ((type_len == 5) && (memcmp(type, "shell", 5) == 0))
		return 0;
	if ((type_len == 4) && (memcmp(type, "exec", 4) == 0))
		return 0;
	if ((type_len == 9) && (memcmp(type, "subsystem", 9) == 0))
		return 0;

        /* Reject unknown requests */
	return -1;
}

static struct dssh_server_session_cbs session_cbs = {
	.request_cb = channel_request_cb,
};

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
	if (dssh_transport_set_callbacks(tx, rx, rxline, NULL) != 0)
		return 1;
	if (dssh_register_mlkem768x25519_sha256() != 0)
		return 1;
	if (dssh_register_sntrup761x25519_sha512() != 0)
		return 1;
	if (dssh_register_curve25519_sha256() != 0)
		return 1;
	if (dssh_register_dh_gex_sha256() != 0)
		return 1;
	if (dssh_register_ssh_ed25519() != 0)
		return 1;
	if (dssh_register_rsa_sha2_256() != 0)
		return 1;
	if (dssh_register_aes256_ctr() != 0)
		return 1;
	if (dssh_register_hmac_sha2_256() != 0)
		return 1;
	if (dssh_register_none_comp() != 0)
		return 1;

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
		.sin_addr = {htonl(INADDR_ANY)},
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
	sess = dssh_session_init(false, 0);
	if (sess == NULL) {
		fprintf(stderr, "session_init failed\n");
		return 1;
	}

        /* Generate ephemeral host keys and set DH group provider */
	int res = dssh_ed25519_generate_key();

	if (res < 0) {
		fprintf(stderr, "ed25519 key gen failed: %d\n", res);
		return 1;
	}
	res = dssh_rsa_sha2_256_generate_key(4096);
	if (res < 0) {
		fprintf(stderr, "rsa key gen failed: %d\n", res);
		return 1;
	}
	dssh_dh_gex_set_provider(sess, &dh_provider);

        /* Handshake */
	fprintf(stderr, "Handshake...\n");
	res = dssh_transport_handshake(sess);
	if (res < 0) {
		fprintf(stderr, "handshake failed: %d\n", res);
		return 1;
	}
	fprintf(stderr, "Remote: %s\n", dssh_transport_get_remote_version(sess));
	fprintf(stderr, "KEX: %s, Host key: %s, Enc: %s, MAC: %s\n",
	    dssh_transport_get_kex_name(sess),
	    dssh_transport_get_hostkey_name(sess),
	    dssh_transport_get_enc_name(sess),
	    dssh_transport_get_mac_name(sess));

        /* Auth */
	fprintf(stderr, "Handling auth...\n");

	uint8_t auth_user[256];
	size_t  auth_user_len;

	res = dssh_auth_server(sess, &auth_cbs, auth_user, &auth_user_len);
	if (res < 0) {
		fprintf(stderr, "auth failed: %d\n", res);
		return 1;
	}
	fprintf(stderr, "Client authenticated.\n");

        /* Start demux thread */
	res = dssh_session_start(sess);
	if (res < 0) {
		fprintf(stderr, "session_start failed: %d\n", res);
		return 1;
	}

        /* Accept incoming channel */
	struct dssh_incoming_open *inc;

	res = dssh_session_accept(sess, &inc, -1);
	if (res < 0) {
		fprintf(stderr, "accept failed: %d\n", res);
		return 1;
	}

        /* Accept as session channel */
	const char *req_type, *req_data;

	fprintf(stderr, "Handling channel...\n");

	dssh_channel ch = dssh_session_accept_channel(sess, inc,
	        &session_cbs, &req_type, &req_data);

	if (ch == NULL) {
		fprintf(stderr, "session_accept_channel failed\n");
		return 1;
	}
	fprintf(stderr, "  Request: %s %s\n", req_type, req_data);

	if (strcmp(req_type, "exec") == 0) {
                /* Execute command — respond and close */
		if (strcmp(req_data, "ping") == 0) {
			dssh_session_write(sess, ch,
			    (const uint8_t *)"pong\n", 5);
			dssh_session_write_ext(sess, ch,
			    (const uint8_t *)"gnop\n", 5);
		}
		else {
			dssh_session_write(sess, ch,
			    (const uint8_t *)"unknown command\n", 16);
		}
	}
	else {
                /* Shell mode — simple line-based command parser */
		uint8_t buf[4096];
		size_t  line_len = 0;

		dssh_session_write(sess, ch,
		    (const uint8_t *)"> ", 2);

		for (;;) {
			int ev = dssh_session_poll(sess, ch,
			        DSSH_POLL_READ, -1);

			if (ev <= 0)
				break;

			ssize_t n = dssh_session_read(sess, ch,
			        &buf[line_len],
			        sizeof(buf) - line_len - 1);

			if (n <= 0)
				break;

			/* Echo input back, translating \r to \r\n */
			for (ssize_t ei = 0; ei < n; ei++) {
				if (buf[line_len + ei] == '\r')
					dssh_session_write(sess, ch,
					    (const uint8_t *)"\r\n", 2);
				else
					dssh_session_write(sess, ch,
					    &buf[line_len + ei], 1);
			}

			line_len += n;

			/* Look for newline */
			if (line_len == 0
			    || (buf[line_len - 1] != '\n'
			        && buf[line_len - 1] != '\r'))
				continue;

			/* Strip trailing CR/LF */
			while (line_len > 0
			    && (buf[line_len - 1] == '\n'
			        || buf[line_len - 1] == '\r'))
				line_len--;
			buf[line_len] = '\0';

			if (strcmp((char *)buf, "ping") == 0) {
				dssh_session_write(sess, ch,
				    (const uint8_t *)"pong\r\n", 6);
			}
			else if (strcmp((char *)buf, "quit") == 0
			    || strcmp((char *)buf, "exit") == 0) {
				dssh_session_write(sess, ch,
				    (const uint8_t *)"bye\r\n", 5);
				break;
			}
			else if (line_len > 0) {
				static const char help[] =
				    "commands: ping, quit, exit\r\n";
				dssh_session_write(sess, ch,
				    (const uint8_t *)help, sizeof(help) - 1);
			}

			line_len = 0;
			dssh_session_write(sess, ch,
			    (const uint8_t *)"> ", 2);
		}
	}

        /* Clean shutdown */
	fprintf(stderr, "Channel closed.\n");
	dssh_session_close(sess, ch, 0);
	dssh_session_cleanup(sess);
	close(conn_fd);
	fprintf(stderr, "Server shutdown complete.\n");
	return 0;
}
