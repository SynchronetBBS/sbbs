/* Client-preference tests over separate process registries. Both directions
 * choose the client's first MAC. Actual asymmetric peer coverage is separate. */
#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>

#include "dssh_test.h"
#include "deucessh-auth.h"
#include "deucessh-conn.h"
#include "deucessh-algorithms.h"
#include "ssh-trans.h"
#include "dssh_test_internal.h"
#include "dssh_test_alloc.h"

static int
sock_tx(uint8_t *buf, size_t length, dssh_session sess, void *data)
{
	int fd = *(int *)data;
	size_t offset = 0;
	while (offset < length) {
		if (dssh_session_is_terminated(sess))
			return DSSH_ERROR_TERMINATED;
		ssize_t amount = send(fd, &buf[offset], length - offset, 0);
		if (amount < 0 && errno == EINTR)
			continue;
		if (amount <= 0)
			return DSSH_ERROR_INIT;
		size_t advanced = (size_t)amount;
		if (advanced > length - offset)
			return DSSH_ERROR_INVALID;
		offset += advanced;
	}
	return 0;
}

static int
sock_rx(uint8_t *buf, size_t length, dssh_session sess, void *data)
{
	int fd = *(int *)data;
	size_t offset = 0;
	while (offset < length) {
		if (dssh_session_is_terminated(sess))
			return DSSH_ERROR_TERMINATED;
		ssize_t amount = recv(fd, &buf[offset], length - offset, 0);
		if (amount < 0 && errno == EINTR)
			continue;
		if (amount <= 0)
			return DSSH_ERROR_INIT;
		size_t advanced = (size_t)amount;
		if (advanced > length - offset)
			return DSSH_ERROR_INVALID;
		offset += advanced;
	}
	return 0;
}

static int
register_algorithms(bool prefer_512, bool server)
{
	int ret = dssh_register_curve25519_sha256();
	if (ret == 0)
		ret = dssh_register_ssh_ed25519();
	if (ret == 0)
		ret = dssh_register_aes256_ctr();
	if (ret == 0)
		ret = prefer_512 ? dssh_register_hmac_sha2_512() : dssh_register_hmac_sha2_256();
	if (ret == 0)
		ret = prefer_512 ? dssh_register_hmac_sha2_256() : dssh_register_hmac_sha2_512();
	if (ret == 0)
		ret = dssh_register_none_comp();
	if (ret == 0 && server)
		ret = dssh_ed25519_generate_key();
	if (ret == 0)
		ret = dssh_transport_set_callbacks(sock_tx, sock_rx, NULL, NULL);
	return ret;
}

static int
finish_session(dssh_session sess, int fd, int result)
{
	if (sess != NULL)
		dssh_session_terminate(sess);
	if (shutdown(fd, SHUT_RDWR) < 0 && errno != ENOTCONN)
		result = 1;
	dssh_session_cleanup(sess);
	if (close(fd) < 0)
		result = 1;
	dssh_test_reset_global_config();
	return result;
}

static int
auth_pw_cb(const uint8_t *user, size_t user_len, const uint8_t *password,
    size_t password_len, struct dssh_bytes_result *output, void *data)
{
	(void)output;
	(void)data;
	return user_len == 8 && memcmp(user, "testuser", 8) == 0
	    && password_len == 8 && memcmp(password, "testpass", 8) == 0
	    ? DSSH_AUTH_SUCCESS : DSSH_AUTH_FAILURE;
}

static int
accept_exec_cb(dssh_channel ch, const struct dssh_chan_params *p,
    struct dssh_chan_accept_result *result, void *data)
{
	(void)ch;
	(void)result;
	(void)data;
	return p->command != NULL && strcmp(p->command, "echo") == 0 ? 0 : -1;
}

static int
transfer(dssh_channel ch, uint8_t *buffer, size_t length, bool write_data)
{
	size_t offset = 0;
	while (offset < length) {
		int wanted = write_data ? DSSH_POLL_WRITE : DSSH_POLL_READ;
		int ready = dssh_chan_poll(ch, wanted, 5000);
		if (ready <= 0 || (ready & wanted) == 0)
			return 1;
		int64_t amount = write_data ? dssh_chan_write(ch, 0, &buffer[offset], length - offset)
		    : dssh_chan_read(ch, 0, &buffer[offset], length - offset);
		if (amount <= 0 || amount > length - offset)
			return 1;
		size_t advanced = (size_t)amount;
		offset += advanced;
	}
	return 0;
}

static int
expect_event(dssh_channel ch, int type)
{
	int ready = dssh_chan_poll(ch, DSSH_POLL_EVENT, 5000);
	if (ready <= 0 || (ready & DSSH_POLL_EVENT) == 0)
		return 1;
	struct dssh_chan_event event;
	int ret = dssh_chan_read_event(ch, &event);
	return ret == 0 && event.type == type
	    && (type != DSSH_EVENT_EXIT_STATUS || event.exit_status.exit_code == 0) ? 0 : 1;
}

static int
wait_for_disconnect(dssh_session sess, int fd)
{
	for (unsigned attempt = 0; attempt < 150; attempt++) {
		if (dssh_session_is_terminated(sess))
			return 0;
		struct pollfd item = {.fd = fd};
		int ready = poll(&item, 1, 100);
		if (ready < 0) {
			if (errno == EINTR)
				continue;
			return 1;
		}
		if (ready > 0) {
			if ((item.revents & POLLNVAL) != 0)
				return 1;
			if ((item.revents & (POLLHUP | POLLERR)) != 0)
				return 0;
		}
	}
	return 1;
}

static const uint8_t message[] = "mac-preference-test";

static int
run_server(int fd, bool prefer_512)
{
	dssh_test_reset_global_config();
	dssh_session sess = NULL;
	int result = 1;
	if (register_algorithms(prefer_512, true) != 0)
		goto done;
	sess = dssh_session_init(false, 0);
	if (sess == NULL || dssh_session_set_cbdata(sess, &fd, &fd, &fd, &fd) != 0
	    || dssh_transport_handshake(sess) != 0)
		goto done;
	struct dssh_auth_server_cbs auth = {.methods_str = "password", .password_cb = auth_pw_cb};
	if (dssh_auth_server(sess, &auth, NULL, NULL) != 0 || dssh_session_start(sess) != 0)
		goto done;
	struct dssh_chan_accept_cbs accept = {.exec = accept_exec_cb};
	dssh_channel ch = dssh_chan_accept(sess, &accept, 5000);
	if (ch == NULL)
		goto done;
	uint8_t buffer[sizeof(message)];
	if (transfer(ch, buffer, sizeof(buffer), false) != 0
	    || memcmp(buffer, message, sizeof(buffer)) != 0
	    || transfer(ch, buffer, sizeof(buffer), true) != 0
	    || expect_event(ch, DSSH_EVENT_EOF) != 0)
		goto done;
	if (dssh_chan_close(ch, 0) != 0)
		goto done;
	result = wait_for_disconnect(sess, fd);
done:
	return finish_session(sess, fd, result);
}

static int
run_client(int fd, bool prefer_512, uint16_t expected_digest)
{
	dssh_test_reset_global_config();
	dssh_session sess = NULL;
	int result = 1;
	if (register_algorithms(prefer_512, false) != 0)
		goto done;
	sess = dssh_session_init(true, 0);
	if (sess == NULL || dssh_session_set_cbdata(sess, &fd, &fd, &fd, &fd) != 0
	    || dssh_session_set_hostkey_verify_cb(sess, dssh_test_accept_hostkey, NULL) != 0
	    || dssh_transport_handshake(sess) != 0)
		goto done;
	if (sess->trans.mac_c2s_selected->digest_size != expected_digest
	    || sess->trans.mac_s2c_selected->digest_size != expected_digest)
		goto done;
	if (dssh_auth_password(sess, "testuser", "testpass", NULL, NULL) != 0
	    || dssh_session_start(sess) != 0)
		goto done;
	struct dssh_chan_params params;
	dssh_chan_params_init(&params, DSSH_CHAN_SESSION);
	int ret = dssh_chan_params_set_command(&params, "echo");
	dssh_channel ch = ret == 0 ? dssh_chan_open(sess, &params) : NULL;
	dssh_chan_params_free(&params);
	if (ch == NULL)
		goto done;
	uint8_t buffer[sizeof(message)];
	memcpy(buffer, message, sizeof(buffer));
	if (transfer(ch, buffer, sizeof(buffer), true) != 0
	    || transfer(ch, buffer, sizeof(buffer), false) != 0
	    || memcmp(buffer, message, sizeof(buffer)) != 0
	    || dssh_chan_shutwr(ch) != 0
	    || expect_event(ch, DSSH_EVENT_EXIT_STATUS) != 0
	    || expect_event(ch, DSSH_EVENT_EOF) != 0
	    || expect_event(ch, DSSH_EVENT_CLOSE) != 0
	    || dssh_chan_close(ch, -1) != 0)
		goto done;
	result = 0;
done:
	return finish_session(sess, fd, result);
}

static void
close_failed_pair(int descriptors[2])
{
	for (size_t i = 0; i < 2; i++) {
		if (close(descriptors[i]) < 0)
			perror("close fixture socket");
	}
}

static pid_t
wait_child(pid_t child, int *status, bool inject_interrupt)
{
	pid_t waited;
	do {
		if (inject_interrupt) {
			inject_interrupt = false;
			errno = EINTR;
			waited = -1;
		}
		else
			waited = waitpid(child, status, 0);
	} while (waited < 0 && errno == EINTR);
	return waited;
}

static int
fork_test(bool client_prefer_512, bool server_prefer_512,
    uint16_t expected_digest, bool fail_client)
{
	int descriptors[2];
	if (socketpair(AF_UNIX, SOCK_STREAM, 0, descriptors) < 0)
		return TEST_FAIL;
	const struct timeval timeout = {.tv_sec = 10};
	for (size_t i = 0; i < 2; i++) {
		if (setsockopt(descriptors[i], SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0
		    || setsockopt(descriptors[i], SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) < 0) {
			close_failed_pair(descriptors);
			return TEST_FAIL;
		}
	}
	pid_t child = fork();
	if (child < 0) {
		close_failed_pair(descriptors);
		return TEST_FAIL;
	}
	if (child == 0) {
		if (close(descriptors[0]) < 0)
			_exit(1);
		alarm(30);
		_exit(run_server(descriptors[1], server_prefer_512));
	}
	if (close(descriptors[1]) < 0) {
		if (shutdown(descriptors[0], SHUT_RDWR) < 0)
			perror("shutdown fixture socket");
		if (close(descriptors[0]) < 0)
			perror("close fixture socket");
		if (kill(child, SIGTERM) < 0 && errno != ESRCH)
			perror("kill fixture child");
		int status = 0;
		if (wait_child(child, &status, false) != child)
			perror("wait fixture child");
		return TEST_FAIL;
	}
	if (fail_client)
		dssh_test_alloc_fail_after(0);
	int result = run_client(descriptors[0], client_prefer_512, expected_digest);
	int allocations = fail_client ? dssh_test_alloc_count() : 0;
	dssh_test_alloc_reset();
	if (result != 0 && kill(child, SIGTERM) < 0 && errno != ESRCH)
		perror("kill fixture child");
	int status = 0;
	pid_t waited = wait_child(child, &status, false);
	bool success = waited == child && WIFEXITED(status) && WEXITSTATUS(status) == 0;
	if (!success && !fail_client) {
		intmax_t waited_count = (intmax_t)waited;
		intmax_t child_count = (intmax_t)child;
		fprintf(stderr, "fixture child wait=%jd expected=%jd status=%d\n",
		    waited_count, child_count, status);
	}
	if (fail_client) {
		int child_exit = waited == child && WIFEXITED(status) ? WEXITSTATUS(status) : -1;
		int child_signal = waited == child && WIFSIGNALED(status) ? WTERMSIG(status) : 0;
		printf("preference_setup_failure client=%d allocations=%d reaped=%u child-exit=%d child-signal=%d\n",
		    result, allocations, waited == child ? 1U : 0U, child_exit, child_signal);
		return result == 1 && allocations == 1 && waited == child ? TEST_PASS : TEST_FAIL;
	}
	return result == 0 && success ? TEST_PASS : TEST_FAIL;
}

static int
test_child_status(void)
{
	pid_t child = fork();
	if (child < 0)
		return TEST_FAIL;
	if (child == 0)
		_exit(7);
	int status = 0;
	pid_t waited = wait_child(child, &status, true);
	bool valid = waited == child && WIFEXITED(status) && WEXITSTATUS(status) == 7;
	waited = wait_child(child, &status, false);
	valid = valid && waited < 0 && errno == ECHILD;
	printf("preference_child_status exit-seven=1 interrupted-wait=1 repeated-echild=1 valid=%u\n",
	    valid ? 1U : 0U);
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

static int
test_client_512_server_256(void)
{
	return fork_test(true, false, 64, false);
}

static int
test_client_256_server_512(void)
{
	return fork_test(false, true, 32, false);
}

static int
test_client_setup_failure(void)
{
	return fork_test(false, true, 32, true);
}

static struct dssh_test_entry tests[] = {
	{"asymmetric_mac/child_status", test_child_status},
	{"asymmetric_mac/client_setup_failure", test_client_setup_failure},
	{"asymmetric_mac/client_512_server_256", test_client_512_server_256},
	{"asymmetric_mac/client_256_server_512", test_client_256_server_512},
};
DSSH_TEST_NO_CLEANUP
DSSH_TEST_MAIN(tests)
