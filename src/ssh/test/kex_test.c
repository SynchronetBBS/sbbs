/*
 * KEX interop test — connect to an SSH server and exercise each
 * KEX method individually.  Verifies handshake + auth-method-query
 * succeeds (auth itself will fail without valid credentials).
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
	int fd = sock_fd;

	if (fd != -1)
		shutdown(fd, SHUT_RDWR);
}

static int
do_tx(uint8_t *buf, size_t bufsz, dssh_session s, void *cbdata)
{
	size_t sent = 0;

	while (sent < bufsz) {
		ssize_t n = send(sock_fd, &buf[sent], bufsz - sent, 0);

		if (n > 0)
			sent += n;
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
	size_t got = 0;

	while (got < bufsz) {
		ssize_t n = recv(sock_fd, &buf[got], bufsz - got, 0);

		if (n > 0)
			got += n;
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

	for (;;) {
		int res = do_rx(&buf[pos], 1, s, cbdata);

		if (res < 0)
			return res;
		if (buf[pos] == '\r')
			cr = true;
		if (buf[pos] == '\n' && cr) {
			*out = pos + 1;
			return 0;
		}
		if (pos + 1 < bufsz)
			pos++;
		else
			return DSSH_ERROR_TOOLONG;
	}
}

static int
do_extra(uint8_t *buf, size_t bufsz, void *cbdata)
{
	return 0;
}

static int
tcp_connect(const char *host, int port)
{
	char portstr[16];

	snprintf(portstr, sizeof(portstr), "%d", port);

	struct addrinfo hints = { .ai_family = AF_UNSPEC, .ai_socktype = SOCK_STREAM };
	struct addrinfo *ai;

	if (getaddrinfo(host, portstr, &hints, &ai) != 0)
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
	{ "sntrup761x25519-sha512",      dssh_register_sntrup761x25519_sha512 },
	{ "mlkem768x25519-sha256",       dssh_register_mlkem768x25519_sha256 },
};

extern void dssh_test_reset_global_config(void);

int
main(int argc, char **argv)
{
	const char *host = "127.0.0.1";
	int         port = 22;
	int         failures = 0;
	size_t      nkex = sizeof(kex_methods) / sizeof(kex_methods[0]);

	if (argc > 1)
		host = argv[1];
	if (argc > 2)
		port = atoi(argv[2]);

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
		dssh_session_set_terminate_cb(sess, on_terminate, NULL);
		dssh_session_set_hostkey_verify_cb(sess, accept_hostkey, NULL);

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
		if (strcmp(neg_kex, kex_methods[i].name) != 0) {
			printf("FAIL (got %s)\n", neg_kex);
			dssh_session_cleanup(sess);
			close(sock_fd);
			sock_fd = -1;
			failures++;
			continue;
		}

		/* Try auth query — exercises encrypted transport */
		char methods[256];

		res = dssh_auth_get_methods(sess, "test", methods, sizeof(methods));
		if (res < 0) {
			printf("FAIL (auth query: %d)\n", res);
			dssh_session_cleanup(sess);
			close(sock_fd);
			sock_fd = -1;
			failures++;
			continue;
		}

		printf("PASS  kex=%s hk=%s\n", neg_kex, neg_hk);

		dssh_transport_disconnect(sess, 0, "test done");
		dssh_session_cleanup(sess);
		close(sock_fd);
		sock_fd = -1;
	}

	printf("\n%zu KEX methods tested, %d failures\n", nkex, failures);
	return failures;
}
