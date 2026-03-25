/*
 * Example SSH client using the high-level DeuceSSH API.
 * Usage: client username password [host] [port] [command]
 * If command is given, runs exec mode (no pty).
 * If omitted, opens an interactive shell with pty.
 */

#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <threads.h>
#include <unistd.h>

#include "deucessh-algorithms.h"
#include "deucessh-auth.h"
#include "deucessh-conn.h"
#include "deucessh.h"

static int          sock = -1;
static dssh_session sess;

/* ================================================================
 * Keyboard-interactive callback: answer every prompt with password
 * ================================================================ */
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
	size_t      plen = strlen(password);

	for (uint32_t i = 0; i < num_prompts; i++) {
		responses[i] = malloc(plen);
		if (responses[i] == NULL) {
			for (uint32_t j = 0; j < i; j++)
				free(responses[j]);
			return DSSH_ERROR_ALLOC;
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
tx(uint8_t *buf, size_t bufsz, dssh_session s, void *cbdata)
{
	size_t sent_total = 0;

	while (sent_total < bufsz && !dssh_session_is_terminated(s)) {
		struct pollfd fd = {
			.fd = sock,
			.events = POLLOUT,
			.revents = 0,
		};
		int           pr = poll(&fd, 1, 5000);

		if (pr < 0)
			return DSSH_ERROR_INIT;
		if (pr == 0)
			continue;
		if (fd.revents & (POLLHUP | POLLERR))
			return DSSH_ERROR_INIT;

		ssize_t n = send(sock, &buf[sent_total], bufsz - sent_total, 0);

		if (n > 0) {
			sent_total += n;
		}
		else if (n < 0) {
			if ((errno == EAGAIN) || (errno == EINTR))
				continue;
			return DSSH_ERROR_INIT;
		}
	}
	return 0;
}

static int
rx(uint8_t *buf, size_t bufsz, dssh_session s, void *cbdata)
{
	size_t rxlen = 0;

	while (rxlen < bufsz && !dssh_session_is_terminated(s)) {
		struct pollfd fd = {
			.fd = sock,
			.events = POLLIN,
			.revents = 0,
		};
		int           pr = poll(&fd, 1, 5000);

		if (pr < 0)
			return DSSH_ERROR_INIT;
		if (pr == 0)
			continue;
		if (fd.revents & (POLLHUP | POLLERR) && !(fd.revents & POLLIN))
			return DSSH_ERROR_INIT;

		ssize_t n = recv(sock, &buf[rxlen], bufsz - rxlen, 0);

		if (n > 0) {
			rxlen += n;
		}
		else if (n == 0) {
			return DSSH_ERROR_INIT;
		}
		else {
			if ((errno == EAGAIN) || (errno == EINTR))
				continue;
			return DSSH_ERROR_INIT;
		}
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

static int
extra_line(uint8_t *buf, size_t bufsz, void *cbdata)
{
	fprintf(stdout, "Server: %.*s\n", (int)bufsz, buf);
	return 0;
}

/* ================================================================
 * Stdin writer thread — reads from stdin, writes to channel
 * ================================================================ */
static int
stdin_thread(void *arg)
{
	dssh_channel ch = arg;
	uint8_t      buf[4096];

	for (;;) {
                /* Wait for stdin data */
		struct pollfd fds[1] = {
			{.fd = STDIN_FILENO, .events = POLLIN},
		};
		int           pr = poll(fds, 1, 1000);

		if (dssh_session_is_terminated(sess))
			break;
		if (pr <= 0)
			continue;

		ssize_t n = read(STDIN_FILENO, buf, sizeof(buf));

		if (n <= 0)
			break;

                /* Wait for channel write space, then send */
		while (n > 0) {
			int ev = dssh_session_poll(sess, ch,
			        DSSH_POLL_WRITE, 1000);

			if ((ev < 0) || dssh_session_is_terminated(sess))
				goto done;
			if (!(ev & DSSH_POLL_WRITE))
				continue;

			ssize_t w = dssh_session_write(sess, ch, buf, n);

			if (w < 0)
				goto done;
			n -= w;
		}
	}
done:
	return 0;
}

/* ================================================================
 * Main
 * ================================================================ */
int
main(int argc, char **argv)
{
	const char *host = "127.0.0.1";
	int         port = 22;
	const char *username = NULL;
	const char *password = NULL;
	const char *command = NULL;

        /* client user pass [host] [port] [command] */
	if (argc > 1)
		username = argv[1];
	if (argc > 2)
		password = argv[2];
	if (argc > 3)
		host = argv[3];
	if (argc > 4)
		port = atoi(argv[4]);
	if (argc > 5)
		command = argv[5];

	if ((username == NULL) || (password == NULL)) {
		fprintf(stderr, "Usage: %s username password [host] [port] [command]\n", argv[0]);
		return 1;
	}

        /* Connect */
	char port_str[16];

	snprintf(port_str, sizeof(port_str), "%d", port);

	struct addrinfo  hints = {
		.ai_family = AF_UNSPEC,
		.ai_socktype = SOCK_STREAM,
	};
	struct addrinfo *ai;
	int              gai = getaddrinfo(host, port_str, &hints, &ai);

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
	if (dssh_transport_set_callbacks(tx, rx, rxline, extra_line) != 0)
		return 1;
	if (register_curve25519_sha256() != 0)
		return 1;
	if (register_dh_gex_sha256() != 0)
		return 1;
	if (register_rsa_sha2_256() != 0)
		return 1;
	if (register_ssh_ed25519() != 0)
		return 1;
	if (register_aes256_ctr() != 0)
		return 1;
	if (register_none_enc() != 0)
		return 1;
	if (register_hmac_sha2_256() != 0)
		return 1;
	if (register_none_mac() != 0)
		return 1;
	if (register_none_comp() != 0)
		return 1;

        /* Initialize session */
	sess = dssh_session_init(true, 0);
	if (sess == NULL) {
		fprintf(stderr, "session_init failed\n");
		return 1;
	}

        /* Handshake */
	fprintf(stderr, "Handshake...\n");

	int res = dssh_transport_handshake(sess);

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

        /* Authenticate */
	char methods[256];

	res = dssh_auth_get_methods(sess, username, methods, sizeof(methods));
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
			res = dssh_auth_password(sess, username, password, NULL, NULL);
		if ((res < 0) && strstr(methods, "keyboard-interactive"))
			res = dssh_auth_keyboard_interactive(sess, username, kbi_prompt, (void *)password);
		if (res < 0) {
			fprintf(stderr, "authentication failed: %d\n", res);
			return 1;
		}
		fprintf(stderr, "Authenticated.\n");
	}

        /* Start the demux thread */
	res = dssh_session_start(sess);
	if (res < 0) {
		fprintf(stderr, "session_start failed: %d\n", res);
		return 1;
	}

        /* Open channel */
	dssh_channel ch;

	if (command != NULL) {
		fprintf(stderr, "Executing: %s\n", command);
		ch = dssh_session_open_exec(sess, command);
	}
	else {
		fprintf(stderr, "Opening shell...\n");

		struct dssh_pty_req pty = {
			.term = "xterm-256color",
			.cols = 80, .rows = 24,
			.wpx = 0, .hpx = 0,
		};

		ch = dssh_session_open_shell(sess, &pty);
	}
	if (ch == NULL) {
		fprintf(stderr, "channel open failed\n");
		return 1;
	}

        /* Interactive mode: spawn a stdin writer thread */
	thrd_t writer;
	bool   have_writer = false;

	if (command == NULL) {
		if (thrd_create(&writer, stdin_thread, ch) == thrd_success)
			have_writer = true;
	}

        /* Read stdout/stderr/signals via poll */
	uint8_t buf[4096];
	int     poll_events = DSSH_POLL_READ | DSSH_POLL_READEXT
	    | DSSH_POLL_SIGNAL;

	for (;;) {
		int ev = dssh_session_poll(sess, ch, poll_events, -1);

		if (ev <= 0)
			break;

		if (ev & DSSH_POLL_READ) {
			ssize_t n = dssh_session_read(sess, ch, buf, sizeof(buf));

			if (n <= 0)
				break;
			write(STDOUT_FILENO, buf, n);
		}
		if (ev & DSSH_POLL_READEXT) {
			ssize_t n = dssh_session_read_ext(sess, ch, buf, sizeof(buf));

			if (n > 0)
				write(STDERR_FILENO, buf, n);
		}
		if (ev & DSSH_POLL_SIGNAL) {
			const char *signame;

			if (dssh_session_read_signal(sess, ch, &signame) == 0)
				fprintf(stderr, "[signal: SIG%s]\n", signame);
		}
	}

        /* Clean up */
	if (have_writer) {
		dssh_session_terminate(sess);

		int wr;

		thrd_join(writer, &wr);
	}
	dssh_session_close(sess, ch, 0);
	dssh_session_cleanup(sess);
	close(sock);
	return 0;
}
