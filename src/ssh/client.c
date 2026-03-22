#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <errno.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>
#include <unistd.h>

#include "deucessh.h"
#include "ssh-auth.h"
#include "ssh-conn.h"
#include "kex/curve25519-sha256.h"
#include "kex/dh-gex-sha256.h"
#include "key_algo/ssh-ed25519.h"
#include "key_algo/rsa-sha2-256.h"
#include "enc/none.h"
#include "enc/aes256-ctr.h"
#include "mac/none.h"
#include "mac/hmac-sha2-256.h"
#include "comp/none.h"

static int sock = -1;
struct deuce_ssh_session_s sess;

/* Keyboard-interactive callback: answer every prompt with the password */
static int
kbi_prompt(const uint8_t *name, size_t name_len,
    const uint8_t *instruction, size_t instruction_len,
    uint32_t num_prompts,
    const uint8_t **prompts, const size_t *prompt_lens,
    const bool *echo,
    uint8_t **responses, size_t *response_lens,
    void *cbdata)
{
	const char *password = cbdata;
	size_t plen = strlen(password);
	for (uint32_t i = 0; i < num_prompts; i++) {
		responses[i] = malloc(plen);
		if (responses[i] == NULL) {
			for (uint32_t j = 0; j < i; j++)
				free(responses[j]);
			return DEUCE_SSH_ERROR_ALLOC;
		}
		memcpy(responses[i], password, plen);
		response_lens[i] = plen;
	}
	return 0;
}

/* ================================================================
 * I/O callbacks for the transport layer
 * ================================================================ */

static int
tx(uint8_t *buf, size_t bufsz, atomic_bool *terminate, void *cbdata)
{
	size_t sent_total = 0;
	while (sent_total < bufsz && !(*terminate)) {
		struct pollfd fd = {
			.fd = sock,
			.events = POLLOUT,
			.revents = 0,
		};
		int pr = poll(&fd, 1, 5000);
		if (pr < 0)
			return DEUCE_SSH_ERROR_INIT;
		if (pr == 0)
			continue;
		if (fd.revents & (POLLHUP | POLLERR))
			return DEUCE_SSH_ERROR_INIT;
		ssize_t n = send(sock, &buf[sent_total], bufsz - sent_total, 0);
		if (n > 0) {
			sent_total += n;
		}
		else if (n < 0) {
			if (errno == EAGAIN || errno == EINTR)
				continue;
			return DEUCE_SSH_ERROR_INIT;
		}
	}
	return 0;
}

static int
rx(uint8_t *buf, size_t bufsz, atomic_bool *terminate, void *cbdata)
{
	size_t rxlen = 0;
	while (rxlen < bufsz && !(*terminate)) {
		struct pollfd fd = {
			.fd = sock,
			.events = POLLIN,
			.revents = 0,
		};
		int pr = poll(&fd, 1, 5000);
		if (pr < 0)
			return DEUCE_SSH_ERROR_INIT;
		if (pr == 0)
			continue;
		if (fd.revents & (POLLHUP | POLLERR) && !(fd.revents & POLLIN))
			return DEUCE_SSH_ERROR_INIT;
		ssize_t n = recv(sock, &buf[rxlen], bufsz - rxlen, 0);
		if (n > 0) {
			rxlen += n;
		}
		else if (n == 0) {
			return DEUCE_SSH_ERROR_INIT;
		}
		else {
			if (errno == EAGAIN || errno == EINTR)
				continue;
			return DEUCE_SSH_ERROR_INIT;
		}
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

static int
extra_line(uint8_t *buf, size_t bufsz, void *cbdata)
{
	fprintf(stdout, "Server: %.*s\n", (int)bufsz, buf);
	return 0;
}

/* ================================================================
 * Receive thread — reads channel data and writes to stdout/stderr
 * ================================================================ */

struct rx_thread_args {
	struct deuce_ssh_channel_s *ch;
};

static int
rx_thread(void *arg)
{
	struct rx_thread_args *a = arg;
	struct deuce_ssh_channel_s *ch = a->ch;

	for (;;) {
		uint8_t msg_type;
		uint8_t *data;
		size_t data_len;

		int res = deuce_ssh_conn_recv(&sess, ch, &msg_type, &data, &data_len);
		if (res < 0)
			break;

		if (msg_type == SSH_MSG_CHANNEL_DATA) {
			write(STDOUT_FILENO, data, data_len);
			if (ch->local_window < 0x100000)
				deuce_ssh_conn_send_window_adjust(&sess, ch, 0x200000);
		}
		else if (msg_type == SSH_MSG_CHANNEL_EXTENDED_DATA) {
			if (data != NULL && data_len > 0)
				write(STDERR_FILENO, data, data_len);
		}
		else if (msg_type == SSH_MSG_CHANNEL_EOF ||
		         msg_type == SSH_MSG_CHANNEL_CLOSE) {
			break;
		}
	}

	return 0;
}

/* ================================================================
 * Transmit thread — reads from stdin and sends as channel data
 * ================================================================ */

static int
tx_thread(void *arg)
{
	struct rx_thread_args *a = arg;
	struct deuce_ssh_channel_s *ch = a->ch;
	uint8_t buf[4096];

	for (;;) {
		struct pollfd fds[1] = {
			{ .fd = STDIN_FILENO, .events = POLLIN },
		};
		int pr = poll(fds, 1, 1000);
		if (sess.terminate)
			break;
		if (pr <= 0)
			continue;

		ssize_t n = read(STDIN_FILENO, buf, sizeof(buf));
		if (n <= 0)
			break;

		int res = deuce_ssh_conn_send_data(&sess, ch, buf, n);
		if (res < 0)
			break;
	}

	/* Signal EOF to the remote side */
	uint8_t eof_msg[8];
	size_t pos = 0;
	eof_msg[pos++] = SSH_MSG_CHANNEL_EOF;
	deuce_ssh_serialize_uint32(ch->remote_id, eof_msg, sizeof(eof_msg), &pos);
	deuce_ssh_transport_send_packet(&sess, eof_msg, pos, NULL);

	return 0;
}

/* ================================================================
 * Main
 * ================================================================ */

int
main(int argc, char **argv)
{
	const char *host = "127.0.0.1";
	int port = 22;
	const char *username = NULL;
	const char *password = NULL;
	const char *command = NULL;

	/* client user pass [host] [port] [command]
	 * If command is omitted, forward stdin/stdout interactively. */
	if (argc > 1) username = argv[1];
	if (argc > 2) password = argv[2];
	if (argc > 3) host = argv[3];
	if (argc > 4) port = atoi(argv[4]);
	if (argc > 5) command = argv[5];

	if (username == NULL || password == NULL) {
		fprintf(stderr, "Usage: %s username password [host] [port] [command]\n", argv[0]);
		return 1;
	}

	/* Connect */
	char port_str[16];
	snprintf(port_str, sizeof(port_str), "%d", port);
	struct addrinfo hints = {
		.ai_family = AF_UNSPEC,
		.ai_socktype = SOCK_STREAM,
	};
	struct addrinfo *ai;
	int gai = getaddrinfo(host, port_str, &hints, &ai);
	if (gai != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(gai));
		return 1;
	}
	sock = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
	if (sock == -1) {
		perror("socket");
		freeaddrinfo(ai);
		return 1;
	}
	if (connect(sock, ai->ai_addr, ai->ai_addrlen) == -1) {
		perror("connect");
		freeaddrinfo(ai);
		return 1;
	}
	freeaddrinfo(ai);
	fprintf(stderr, "Connected to %s:%d\n", host, port);

	/* Register algorithms */
	if (deuce_ssh_transport_set_callbacks(tx, rx, rxline, extra_line) != 0)
		return 1;
	if (register_curve25519_sha256() != 0) return 1;
	if (register_dh_gex_sha256() != 0) return 1;
	if (register_rsa_sha2_256() != 0) return 1;
	if (register_ssh_ed25519() != 0) return 1;
	if (register_aes256_ctr() != 0) return 1;
	if (register_none_enc() != 0) return 1;
	if (register_hmac_sha2_256() != 0) return 1;
	if (register_none_mac() != 0) return 1;
	if (register_none_comp() != 0) return 1;

	/* Initialize session */
	memset(&sess, 0, sizeof(sess));
	sess.trans.client = true;
	int res = deuce_ssh_session_init(&sess);
	if (res < 0) {
		fprintf(stderr, "session_init failed: %d\n", res);
		return 1;
	}

	/* ---- Single-threaded handshake ---- */

	/* Version exchange */
	fprintf(stderr, "Version exchange...\n");
	res = deuce_ssh_transport_version_exchange(&sess);
	if (res < 0) {
		fprintf(stderr, "version_exchange failed: %d\n", res);
		return 1;
	}
	fprintf(stderr, "Remote: %s\n", deuce_ssh_transport_get_remote_version(&sess));

	/* KEXINIT */
	fprintf(stderr, "KEXINIT...\n");
	res = deuce_ssh_transport_kexinit(&sess);
	if (res < 0) {
		fprintf(stderr, "kexinit failed: %d\n", res);
		return 1;
	}
	fprintf(stderr, "KEX algorithm: %s\n", deuce_ssh_transport_get_kex_name(&sess));
	fprintf(stderr, "Host key: %s\n", deuce_ssh_transport_get_hostkey_name(&sess));
	fprintf(stderr, "Enc C2S: %s\n", deuce_ssh_transport_get_enc_name(&sess));
	fprintf(stderr, "MAC C2S: %s\n", deuce_ssh_transport_get_mac_name(&sess));

	/* Key exchange */
	fprintf(stderr, "Key exchange...\n");
	res = deuce_ssh_transport_kex(&sess);
	if (res < 0) {
		fprintf(stderr, "kex failed: %d\n", res);
		return 1;
	}

	/* NEWKEYS */
	fprintf(stderr, "NEWKEYS...\n");
	res = deuce_ssh_transport_newkeys(&sess);
	if (res < 0) {
		fprintf(stderr, "newkeys failed: %d\n", res);
		return 1;
	}
	fprintf(stderr, "Encrypted transport established.\n");

	/* Request ssh-userauth service */
	fprintf(stderr, "Requesting ssh-userauth...\n");
	res = deuce_ssh_auth_request_service(&sess, "ssh-userauth");
	if (res < 0) {
		fprintf(stderr, "service request failed: %d\n", res);
		return 1;
	}

	/* Query available auth methods */
	char methods[256];
	res = deuce_ssh_auth_get_methods(&sess, username, methods, sizeof(methods));
	if (res < 0) {
		fprintf(stderr, "auth method query failed: %d\n", res);
		return 1;
	}
	if (res == 0) {
		fprintf(stderr, "No authentication required.\n");
	}
	else {
		fprintf(stderr, "Auth methods: %s\n", methods);
		res = -1;
		if (strstr(methods, "password"))
			res = deuce_ssh_auth_password(&sess, username, password, NULL, NULL);
		if (res < 0 && strstr(methods, "keyboard-interactive"))
			res = deuce_ssh_auth_keyboard_interactive(&sess, username, kbi_prompt, (void *)password);
		if (res < 0) {
			fprintf(stderr, "authentication failed: %d\n", res);
			return 1;
		}
		fprintf(stderr, "Authenticated.\n");
	}

	/* Open channel */
	struct deuce_ssh_channel_s ch;
	fprintf(stderr, "Opening channel...\n");
	res = deuce_ssh_conn_open_session(&sess, &ch, 0);
	if (res < 0) {
		fprintf(stderr, "channel open failed: %d\n", res);
		return 1;
	}

	if (command != NULL) {
		/* Exec mode: run command, read output */
		fprintf(stderr, "Executing: %s\n", command);
		res = deuce_ssh_conn_request_exec(&sess, &ch, command);
		if (res < 0) {
			fprintf(stderr, "exec failed: %d\n", res);
			return 1;
		}
	}

	/* ---- Two-threaded I/O ---- */

	struct rx_thread_args args = { .ch = &ch };
	thrd_t rx_thrd, tx_thrd;

	if (thrd_create(&rx_thrd, rx_thread, &args) != thrd_success) {
		fprintf(stderr, "failed to create rx thread\n");
		return 1;
	}

	if (command == NULL) {
		/* Interactive mode: forward stdin to channel */
		if (thrd_create(&tx_thrd, tx_thread, &args) != thrd_success) {
			fprintf(stderr, "failed to create tx thread\n");
			sess.terminate = true;
			thrd_join(rx_thrd, &res);
			return 1;
		}
		int tx_res;
		thrd_join(tx_thrd, &tx_res);
	}

	/* Wait for receive thread to finish */
	int rx_res;
	thrd_join(rx_thrd, &rx_res);

	deuce_ssh_conn_close(&sess, &ch);
	deuce_ssh_session_cleanup(&sess);
	close(sock);
	return 0;
}
