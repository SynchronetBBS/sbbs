/*
 * KEX interop test — connect to an SSH server and exercise each
 * KEX method individually.  Verifies handshake + auth-method-query
 * succeeds (auth itself will fail without valid credentials).
 * This manual test accepts every host key and is not a trust-policy example.
 *
 * Usage: kex_test [host] [port]
 */

#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "deucessh-algorithms.h"
#include "deucessh-auth.h"
#include "deucessh.h"

static _Atomic int  sock_fd = -1;
static dssh_session sess;

static dssh_hostkey_decision
accept_hostkey(const char *algo_name, unsigned int key_bits,
    const uint8_t *sha256_hash, const uint8_t *key_blob,
    size_t key_blob_len, void *cbdata)
{
	(void)algo_name;
	(void)key_bits;
	(void)sha256_hash;
	(void)key_blob;
	(void)key_blob_len;
	(void)cbdata;
	return DSSH_HOSTKEY_ACCEPT;
}

static void
on_terminate(dssh_session s, void *cbdata)
{
	(void)s;
	(void)cbdata;
	int fd = sock_fd;

	if (fd != -1)
		shutdown(fd, SHUT_RDWR);
}

static int
do_tx(uint8_t *buf, size_t bufsz, dssh_session s, void *cbdata)
{
	(void)s;
	(void)cbdata;
	size_t sent = 0;

	while (sent < bufsz) {
		ssize_t n = send(sock_fd, &buf[sent], bufsz - sent, 0);

		if (n > 0) {
			/* send() returns no more than the requested remaining size. */
			size_t written = (size_t)n;
			sent += written;
		}
		else if (n < 0 && errno == EINTR)
			continue;
		else
			return DSSH_ERROR_INIT;
	}
	return 0;
}

static int
do_rx(uint8_t *buf, size_t bufsz, dssh_session s, void *cbdata)
{
	(void)s;
	(void)cbdata;
	size_t got = 0;

	while (got < bufsz) {
		ssize_t n = recv(sock_fd, &buf[got], bufsz - got, 0);

		if (n > 0) {
			/* recv() returns no more than the requested remaining size. */
			size_t received = (size_t)n;
			got += received;
		}
		else if (n == 0)
			return DSSH_ERROR_INIT;
		else if (errno == EINTR)
			continue;
		else
			return DSSH_ERROR_INIT;
	}
	return 0;
}

static int
do_rxline(uint8_t *buf, size_t bufsz, size_t *out,
    dssh_session s, void *cbdata)
{
	size_t pos = 0;
	bool   cr = false;
	if (bufsz == 0)
		return DSSH_ERROR_INVALID;

	for (;;) {
		int res = do_rx(&buf[pos], 1, s, cbdata);

		if (res < 0)
			return res;
		if (buf[pos] == '\n' && cr) {
			*out = pos + 1;
			return 0;
		}
		cr = buf[pos] == '\r';
		if (pos + 1 < bufsz)
			pos++;
		else
			return DSSH_ERROR_TOOLONG;
	}
}

static int
do_extra(uint8_t *buf, size_t bufsz, void *cbdata)
{
	(void)buf;
	(void)bufsz;
	(void)cbdata;
	return 0;
}

static int
tcp_connect(const char *host, const char *port)
{
	struct addrinfo hints = { .ai_family = AF_UNSPEC, .ai_socktype = SOCK_STREAM,
	    .ai_flags = AI_NUMERICSERV };
	struct addrinfo *ai;

	if (getaddrinfo(host, port, &hints, &ai) != 0)
		return -1;
	int fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);

	if (fd < 0) {
		freeaddrinfo(ai);
		return -1;
	}
	if (connect(fd, ai->ai_addr, ai->ai_addrlen) < 0) {
		close(fd);
		freeaddrinfo(ai);
		return -1;
	}
	freeaddrinfo(ai);
	return fd;
}

struct kex_entry {
	const char *name;
	int       (*reg)(void);
};

static const struct kex_entry kex_methods[] = {
	{ "curve25519-sha256",           dssh_register_curve25519_sha256 },
	{ "diffie-hellman-group-exchange-sha256", dssh_register_dh_gex_sha256 },
	{ "diffie-hellman-group14-sha256", dssh_register_dh_group14_sha256 },
	{ "diffie-hellman-group16-sha512", dssh_register_dh_group16_sha512 },
	{ "sntrup761x25519-sha512",      dssh_register_sntrup761x25519_sha512 },
	{ "mlkem768x25519-sha256",       dssh_register_mlkem768x25519_sha256 },
};

extern void dssh_test_reset_global_config(void);

int
main(int argc, char **argv)
{
	const char *host = "127.0.0.1";
	const char *port = "22";
	int         failures = 0;
	size_t      nkex = sizeof(kex_methods) / sizeof(kex_methods[0]);

	if (argc > 3) {
		fprintf(stderr, "Usage: %s [host] [port]\n", argv[0]);
		return EXIT_FAILURE;
	}
	if (argc > 1)
		host = argv[1];
	if (argc > 2) {
		char *end;
		errno = 0;
		unsigned long value = strtoul(argv[2], &end, 10);
		if (argv[2][0] < '0' || argv[2][0] > '9' || errno != 0
		    || end == argv[2] || *end != '\0' || value == 0 || value > 65535) {
			fprintf(stderr, "Invalid TCP port\n");
			return EXIT_FAILURE;
		}
		port = argv[2];
	}

	for (size_t i = 0; i < nkex; i++) {
		printf("%-45s ", kex_methods[i].name);
		fflush(stdout);

		dssh_test_reset_global_config();

		/* Register only this one KEX + required algos */
		if (dssh_transport_set_callbacks(do_tx, do_rx, do_rxline, do_extra) != 0
		    || kex_methods[i].reg() != 0
		    || dssh_register_ssh_ed25519() != 0
		    || dssh_register_rsa_sha2_256() != 0
		    || dssh_register_aes256_ctr() != 0
		    || dssh_register_hmac_sha2_256() != 0
		    || dssh_register_none_comp() != 0) {
			printf("FAIL (register)\n");
			failures++;
			continue;
		}

		sock_fd = tcp_connect(host, port);
		if (sock_fd < 0) {
			printf("FAIL (connect)\n");
			failures++;
			continue;
		}

		sess = dssh_session_init(true, 0);
		if (sess == NULL) {
			close(sock_fd);
			sock_fd = -1;
			printf("FAIL (init)\n");
			failures++;
			continue;
		}
		if (dssh_session_set_terminate_cb(sess, on_terminate, NULL) != 0
		    || dssh_session_set_hostkey_verify_cb(sess, accept_hostkey, NULL) != 0) {
			printf("FAIL (callbacks)\n");
			dssh_session_cleanup(sess);
			close(sock_fd);
			sock_fd = -1;
			failures++;
			continue;
		}

		int res = dssh_transport_handshake(sess);

		if (res < 0) {
			printf("FAIL (handshake: %d)\n", res);
			dssh_session_cleanup(sess);
			close(sock_fd);
			sock_fd = -1;
			failures++;
			continue;
		}

		const char *neg_kex = dssh_transport_get_kex_name(sess);
		const char *neg_hk = dssh_transport_get_hostkey_name(sess);

		/* Verify the KEX we forced is the one negotiated */
		if (neg_kex == NULL || neg_hk == NULL || strcmp(neg_kex, kex_methods[i].name) != 0) {
			printf("FAIL (unexpected negotiated algorithms)\n");
			dssh_session_cleanup(sess);
			close(sock_fd);
			sock_fd = -1;
			failures++;
			continue;
		}

		/* Try auth query — exercises encrypted transport */
		res = dssh_auth_begin(sess, "test");
		if (res < 0) {
			printf("FAIL (auth query: %d)\n", res);
			dssh_session_cleanup(sess);
			close(sock_fd);
			sock_fd = -1;
			failures++;
			continue;
		}

		res = dssh_transport_disconnect(sess, DSSH_DISCONNECT_BY_APPLICATION, "test done");
		if (res != 0) {
			printf("FAIL (disconnect: %d)\n", res);
			failures++;
		}
		else
			printf("PASS  kex=%s hk=%s\n", neg_kex, neg_hk);
		dssh_session_cleanup(sess);
		close(sock_fd);
		sock_fd = -1;
	}

	printf("\n%zu KEX methods tested, %d failures\n", nkex, failures);
	return failures;
}
