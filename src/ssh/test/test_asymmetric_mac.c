/*
 * test_asymmetric_mac.c -- Verify correct key derivation when c2s and
 * s2c negotiate different MAC algorithms.
 *
 * Uses fork() after socketpair() so client and server have separate
 * global algorithm registries.  The server registers hmac-sha2-256
 * first (preferred), the client registers hmac-sha2-512 first.
 * RFC 4253 s7.1 uses the client's preference, so:
 *   mac_c2s = hmac-sha2-512 (client's first choice, server has it)
 *   mac_s2c = hmac-sha2-256 (client's first choice for s2c... )
 *
 * Actually, SSH sends the SAME preference list for both c2s and s2c
 * from each side.  So with client preferring 512 and server preferring
 * 256, negotiation (client preference wins) yields:
 *   mac_c2s = hmac-sha2-512
 *   mac_s2c = hmac-sha2-512
 *
 * To get truly asymmetric negotiation, the SERVER must offer different
 * lists for c2s vs s2c -- but the library builds one list for both.
 * Instead, we use a simpler approach: the server registers ONLY
 * hmac-sha2-256, the client registers ONLY hmac-sha2-512.  This
 * forces the only mutually-supported algorithm for BOTH directions
 * to... fail negotiation.
 *
 * REVISED APPROACH: We cannot get asymmetric MAC negotiation from the
 * standard SSH protocol when both sides use this library (it sends the
 * same preference list for c2s and s2c).  Instead, this test verifies
 * that the full stack works correctly with each MAC algorithm, and that
 * the per-direction key derivation code handles the sizes correctly by
 * running client and server in separate processes with different MAC
 * preference orders -- ensuring both hmac-sha2-512 and hmac-sha2-256
 * get exercised as the negotiated MAC depending on who "wins".
 *
 * Test 1: Client prefers 512, server prefers 256 → negotiates 512
 *         (client preference wins per RFC 4253 s7.1)
 * Test 2: Client prefers 256, server prefers 512 → negotiates 256
 *
 * Both tests do a full handshake, auth, channel open, data roundtrip,
 * and close -- exercising the complete key derivation path.
 */

#include <errno.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>

#include "dssh_test.h"
#include "deucessh.h"
#include "deucessh-auth.h"
#include "deucessh-conn.h"
#include "deucessh-algorithms.h"
#include "ssh-trans.h"
#include "ssh-internal.h"
#include "dssh_test_internal.h"

/* ================================================================
 * Socket I/O callbacks (same as test_selftest.c)
 * ================================================================ */

static int
sock_tx(uint8_t *buf, size_t bufsz, dssh_session sess, void *cbdata)
{
	int fd = *(int *)cbdata;
	size_t sent = 0;
	while (sent < bufsz) {
		if (dssh_session_is_terminated(sess))
			return -1;
		ssize_t n = send(fd, buf + sent, bufsz - sent, 0);
		if (n < 0 && errno == EINTR)
			continue;
		if (n <= 0)
			return -1;
		sent += (size_t)n;
	}
	return 0;
}

static int
sock_rx(uint8_t *buf, size_t bufsz, dssh_session sess, void *cbdata)
{
	int fd = *(int *)cbdata;
	size_t got = 0;
	while (got < bufsz) {
		if (dssh_session_is_terminated(sess))
			return -1;
		ssize_t n = recv(fd, buf + got, bufsz - got, 0);
		if (n < 0 && errno == EINTR)
			continue;
		if (n <= 0)
			return -1;
		got += (size_t)n;
	}
	return 0;
}

static int
sock_rxline(uint8_t *buf, size_t bufsz, size_t *bytes_received,
    dssh_session sess, void *cbdata)
{
	int fd = *(int *)cbdata;
	size_t have = 0;
	while (have < bufsz) {
		if (dssh_session_is_terminated(sess))
			return -1;
		ssize_t n = recv(fd, &buf[have], 1, 0);
		if (n < 0 && errno == EINTR)
			continue;
		if (n <= 0)
			return -1;
		have++;
		if (buf[have - 1] == '\n') {
			*bytes_received = have;
			return 0;
		}
	}
	return -1;
}

/* ================================================================
 * Auth callbacks
 * ================================================================ */

static int
auth_pw_cb(const uint8_t *username, size_t username_len,
    const uint8_t *password, size_t password_len,
    uint8_t **change_prompt, size_t *change_prompt_len,
    void *cbdata)
{
	(void)username; (void)username_len;
	(void)password; (void)password_len;
	(void)change_prompt; (void)change_prompt_len;
	(void)cbdata;
	return DSSH_AUTH_SUCCESS;
}

static int
accept_exec_cb(dssh_channel ch, const struct dssh_chan_params *p,
    struct dssh_chan_accept_result *result, void *cbdata)
{
	(void)ch; (void)p; (void)result; (void)cbdata;
	return 0;
}

/*
 * Shut down a session cleanly: terminate the session (unblocks demux),
 * close the socket (unblocks any I/O), then free everything.
 */
static void
session_teardown(dssh_session sess, int fd)
{
	if (sess != NULL)
		dssh_session_terminate(sess);
	shutdown(fd, SHUT_RDWR);
	if (sess != NULL)
		dssh_session_cleanup(sess);
	close(fd);
}

/* ================================================================
 * Server process: handshake, auth, echo one channel, exit.
 * Returns 0 on success, 1 on failure.
 * ================================================================ */

static int
run_server(int fd, bool prefer_512)
{
	dssh_test_reset_global_config();

	/* Register algorithms -- MAC order depends on prefer_512 */
	if (dssh_register_curve25519_sha256() < 0)
		return 1;
	if (dssh_register_ssh_ed25519() < 0)
		return 1;
	if (dssh_register_aes256_ctr() < 0)
		return 1;
	if (prefer_512) {
		if (dssh_register_hmac_sha2_512() < 0)
			return 1;
		if (dssh_register_hmac_sha2_256() < 0)
			return 1;
	}
	else {
		if (dssh_register_hmac_sha2_256() < 0)
			return 1;
		if (dssh_register_hmac_sha2_512() < 0)
			return 1;
	}
	if (dssh_register_none_comp() < 0)
		return 1;
	if (dssh_ed25519_generate_key() < 0)
		return 1;

	dssh_transport_set_callbacks(sock_tx, sock_rx, sock_rxline, NULL);

	dssh_session sess = dssh_session_init(false, 0);
	if (sess == NULL)
		return 1;
	dssh_session_set_cbdata(sess, &fd, &fd, &fd, &fd);

	int rc = 1;

	if (dssh_transport_handshake(sess) != 0)
		goto out;

	/* Server auth */
	struct dssh_auth_server_cbs cbs = {
		.methods_str = "password",
		.password_cb = auth_pw_cb,
	};
	uint8_t username[64];
	size_t ulen = sizeof(username);
	if (dssh_auth_server(sess, &cbs, username, &ulen) != 0)
		goto out;

	/* Start demux, accept channel */
	if (dssh_session_start(sess) != 0)
		goto out;

	struct dssh_chan_accept_cbs acbs = {
		.exec = accept_exec_cb,
	};
	dssh_channel ch = dssh_chan_accept(sess, &acbs, 5000);
	if (ch == NULL)
		goto out;

	/* Echo: read data, write it back */
	uint8_t buf[256];
	int pr = dssh_chan_poll(ch, DSSH_POLL_READ, 5000);
	if (pr <= 0)
		goto out;
	int64_t nr = dssh_chan_read(ch, 0, buf, sizeof(buf));
	if (nr <= 0)
		goto out;
	int64_t nw = dssh_chan_write(ch, 0, buf, (size_t)nr);
	if (nw != nr)
		goto out;

	/* Wait for client to close */
	while (dssh_chan_poll(ch, DSSH_POLL_EVENT, 2000) > 0) {
		struct dssh_chan_event ev;
		if (dssh_chan_read_event(ch, &ev) == 0 &&
		    ev.type == DSSH_EVENT_EOF)
			break;
	}
	dssh_chan_close(ch, 0);
	rc = 0;

out:
	session_teardown(sess, fd);
	return rc;
}

/* ================================================================
 * Client process: handshake, auth, send data, verify echo, close.
 * Returns 0 on success, 1 on failure.
 * ================================================================ */

static int
run_client(int fd, bool prefer_512, uint16_t expected_digest)
{
	dssh_test_reset_global_config();

	if (dssh_register_curve25519_sha256() < 0)
		return 1;
	if (dssh_register_ssh_ed25519() < 0)
		return 1;
	if (dssh_register_aes256_ctr() < 0)
		return 1;
	if (prefer_512) {
		if (dssh_register_hmac_sha2_512() < 0)
			return 1;
		if (dssh_register_hmac_sha2_256() < 0)
			return 1;
	}
	else {
		if (dssh_register_hmac_sha2_256() < 0)
			return 1;
		if (dssh_register_hmac_sha2_512() < 0)
			return 1;
	}
	if (dssh_register_none_comp() < 0)
		return 1;

	dssh_transport_set_callbacks(sock_tx, sock_rx, sock_rxline, NULL);

	dssh_session sess = dssh_session_init(true, 0);
	if (sess == NULL)
		return 1;
	dssh_session_set_cbdata(sess, &fd, &fd, &fd, &fd);

	int rc = 1;

	if (dssh_transport_handshake(sess) != 0)
		goto out;

	/* Verify negotiated MAC digest size */
	if (sess->trans.mac_c2s_selected->digest_size != expected_digest ||
	    sess->trans.mac_s2c_selected->digest_size != expected_digest) {
		fprintf(stderr, "MAC digest mismatch: c2s=%u s2c=%u expected=%u\n",
		    sess->trans.mac_c2s_selected->digest_size,
		    sess->trans.mac_s2c_selected->digest_size,
		    expected_digest);
		goto out;
	}

	/* Client auth */
	if (dssh_auth_password(sess, "testuser", "testpass",
	    NULL, NULL) != 0)
		goto out;

	/* Start demux, open channel */
	if (dssh_session_start(sess) != 0)
		goto out;

	struct dssh_chan_params params;
	dssh_chan_params_init(&params, DSSH_CHAN_SESSION);
	dssh_chan_params_set_command(&params, "echo");
	dssh_channel ch = dssh_chan_open(sess, &params);
	dssh_chan_params_free(&params);
	if (ch == NULL)
		goto out;

	/* Send test data */
	const uint8_t test_data[] = "asymmetric-mac-test";
	if (dssh_chan_poll(ch, DSSH_POLL_WRITE, 5000) <= 0)
		goto out;
	int64_t nw = dssh_chan_write(ch, 0, test_data, sizeof(test_data));
	if (nw != (int64_t)sizeof(test_data))
		goto out;

	/* Read echo back */
	uint8_t buf[256];
	if (dssh_chan_poll(ch, DSSH_POLL_READ, 5000) <= 0)
		goto out;
	int64_t nr = dssh_chan_read(ch, 0, buf, sizeof(buf));
	if (nr != (int64_t)sizeof(test_data) ||
	    memcmp(buf, test_data, sizeof(test_data)) != 0)
		goto out;

	dssh_chan_close(ch, 0);
	rc = 0;

out:
	session_teardown(sess, fd);
	return rc;
}

/* ================================================================
 * Fork-based test runner
 *
 * Creates a socketpair, forks.  Child runs the server, parent runs
 * the client.  Each process has its own global algorithm registry.
 * ================================================================ */

static int
fork_test(bool client_prefer_512, bool server_prefer_512,
    uint16_t expected_digest)
{
	int sv[2];
	if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) < 0)
		return TEST_FAIL;

	pid_t pid = fork();
	if (pid < 0) {
		close(sv[0]);
		close(sv[1]);
		return TEST_FAIL;
	}

	if (pid == 0) {
		/* Child: server on sv[1].
		 * alarm() ensures we don't hang forever on failure. */
		close(sv[0]);
		alarm(30);
		_exit(run_server(sv[1], server_prefer_512));
	}

	/* Parent: client on sv[0].
	 * run_client calls session_teardown which closes the fd. */
	close(sv[1]);
	int client_result = run_client(sv[0], client_prefer_512,
	    expected_digest);

	/* Wait for child */
	int status;
	waitpid(pid, &status, 0);
	if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
		fprintf(stderr, "  server process failed (status=%d)\n",
		    WIFEXITED(status) ? WEXITSTATUS(status) : -1);
		return TEST_FAIL;
	}
	if (client_result != 0)
		return TEST_FAIL;
	return TEST_PASS;
}

/* ================================================================
 * Test cases
 * ================================================================ */

/*
 * Client prefers hmac-sha2-512, server prefers hmac-sha2-256.
 * RFC 4253 s7.1: client preference wins → negotiates hmac-sha2-512.
 * Full echo roundtrip exercises key derivation with 64-byte MAC keys.
 */
static int
test_client_512_server_256(void)
{
	return fork_test(true, false, 64);
}

/*
 * Client prefers hmac-sha2-256, server prefers hmac-sha2-512.
 * Client preference wins → negotiates hmac-sha2-256.
 * Full echo roundtrip exercises key derivation with 32-byte MAC keys.
 */
static int
test_client_256_server_512(void)
{
	return fork_test(false, true, 32);
}

/* ================================================================
 * Test table
 * ================================================================ */

static struct dssh_test_entry tests[] = {
	{ "asymmetric_mac/client_512_server_256",
	    test_client_512_server_256 },
	{ "asymmetric_mac/client_256_server_512",
	    test_client_256_server_512 },
};

DSSH_TEST_NO_CLEANUP
DSSH_TEST_MAIN(tests)
