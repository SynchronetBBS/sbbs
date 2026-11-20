/* Local-only independent-peer fixture over supplied stdin/stdout streams.
 * It listens on no socket and implements only a fixed echo command.
 * This harness uses POSIX I/O; the production library remains portable C17. */
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "deucessh.h"
#include "deucessh-algorithms.h"
#include "deucessh-auth.h"
#include "deucessh-conn.h"
#include "test_comp.h"
#include "../examples/zlib-comp.h"

static int
pipe_io(uint8_t *buf, size_t length, dssh_session sess, bool transmit)
{
	int descriptor = transmit ? STDOUT_FILENO : STDIN_FILENO;
	size_t offset = 0;
	while (offset < length) {
		if (dssh_session_is_terminated(sess))
			return DSSH_ERROR_TERMINATED;
		struct pollfd item = { .fd = descriptor, .events = transmit ? POLLOUT : POLLIN };
		int ready = poll(&item, 1, 100);
		if (ready < 0) {
			if (errno == EINTR)
				continue;
			return DSSH_ERROR_INIT;
		}
		if (ready == 0)
			continue;
		if ((item.revents & item.events) == 0)
			return DSSH_ERROR_INIT;
		size_t count = length - offset;
		if (count > 4096)
			count = 4096;
		ssize_t done = transmit ? write(descriptor, &buf[offset], count) :
		    read(descriptor, &buf[offset], count);
		if (done < 0) {
			if (errno == EINTR || errno == EAGAIN)
				continue;
			return DSSH_ERROR_INIT;
		}
		if (done == 0)
			return DSSH_ERROR_INIT;
		/* A successful POSIX result cannot exceed the supplied 4096 bytes. */
		size_t advanced = (size_t)done;
		if (advanced > count)
			return DSSH_ERROR_INVALID;
		offset += advanced;
	}
	return 0;
}

static int
pipe_tx(uint8_t *buf, size_t length, dssh_session sess, void *data)
{
	(void)data;
	return pipe_io(buf, length, sess, true);
}

static int
pipe_rx(uint8_t *buf, size_t length, dssh_session sess, void *data)
{
	(void)data;
	return pipe_io(buf, length, sess, false);
}

static int
accept_none(const uint8_t *user, size_t length, void *data)
{
	(void)data;
	return length == 5 && memcmp(user, "audit", 5) == 0 ? DSSH_AUTH_SUCCESS : DSSH_AUTH_FAILURE;
}

static int
accept_echo(dssh_channel channel, const struct dssh_chan_params *params,
    struct dssh_chan_accept_result *result, void *data)
{
	(void)channel;
	(void)result;
	(void)data;
	return params->command != NULL && strcmp(params->command, "audit-echo") == 0 ? 0 : -1;
}

static int
echo_channel(dssh_channel channel)
{
	uint8_t buffer[16384];
	for (;;) {
		int ready = dssh_chan_poll(channel, DSSH_POLL_READ, -1);
		if (ready < 0)
			return ready;
		if ((ready & DSSH_POLL_READ) == 0)
			continue;
		int64_t amount = dssh_chan_read(channel, 0, buffer, sizeof(buffer));
		if (amount <= 0) {
			if (amount < INT_MIN)
				return DSSH_ERROR_INVALID;
			int result = (int)amount;
			return result;
		}
		if (amount > sizeof(buffer))
			return DSSH_ERROR_INVALID;
		size_t length = (size_t)amount;
		size_t offset = 0;
		while (offset < length) {
			ready = dssh_chan_poll(channel, DSSH_POLL_WRITE, -1);
			if (ready < 0)
				return ready;
			if ((ready & DSSH_POLL_WRITE) == 0)
				continue;
			amount = dssh_chan_write(channel, 0, &buffer[offset], length - offset);
			if (amount <= 0)
				return DSSH_ERROR_INIT;
			if (amount > length - offset)
				return DSSH_ERROR_INVALID;
			size_t advanced = (size_t)amount;
			offset += advanced;
		}
	}
}

static int
check_negotiated(dssh_session sess, const char *c2s, const char *s2c)
{
	const enum dssh_direction directions[] = {DSSH_CLIENT_TO_SERVER, DSSH_SERVER_TO_CLIENT};
	const char *expected[] = {c2s, s2c};
	for (size_t i = 0; i < 2; i++) {
		const char *comp = dssh_transport_get_comp_name(sess, directions[i]);
		if (comp == NULL || strcmp(comp, expected[i]) != 0)
			return DSSH_ERROR_INVALID;
	}
	return 0;
}

static int
wait_for_peer_disconnect(dssh_session sess)
{
	/* Keep the transport available while the independent client completes
	 * reciprocal channel close and disconnect. The demux remains its sole
	 * reader; this bounded wait only observes termination/socket shutdown. */
	for (unsigned attempt = 0; attempt < 150; attempt++) {
		if (dssh_session_is_terminated(sess))
			return 0;
		struct pollfd item = { .fd = STDIN_FILENO, .events = 0 };
		int ready = poll(&item, 1, 100);
		if (ready < 0) {
			if (errno == EINTR)
				continue;
			return DSSH_ERROR_INIT;
		}
		if (ready > 0) {
			if ((item.revents & POLLNVAL) != 0)
				return DSSH_ERROR_INIT;
			if ((item.revents & (POLLHUP | POLLERR)) != 0)
				return 0;
		}
	}
	return dssh_session_is_terminated(sess) ? 0 : DSSH_ERROR_TIMEOUT;
}

int
main(int argc, char **argv)
{
	if (argc != 3 || (strcmp(argv[1], "none") != 0 && strcmp(argv[1], "zlib") != 0)
	    || (strcmp(argv[2], "none") != 0 && strcmp(argv[2], "zlib") != 0))
		return 1;
	/* A peer may close between readiness and write. Let the I/O callback
	 * return the write error instead of terminating this fixture process. */
	if (signal(SIGPIPE, SIG_IGN) == SIG_ERR)
		return 1;
	for (int descriptor = STDIN_FILENO; descriptor <= STDOUT_FILENO; descriptor++) {
		int flags = fcntl(descriptor, F_GETFL);
		if (flags < 0 || fcntl(descriptor, F_SETFL, flags | O_NONBLOCK) < 0)
			return 1;
	}
	if (dssh_transport_set_callbacks(pipe_tx, pipe_rx, NULL, NULL) != 0
	    || dssh_register_curve25519_sha256() != 0
	    || dssh_register_ssh_ed25519() != 0
	    || dssh_register_aes256_ctr() != 0
	    || dssh_register_hmac_sha2_256() != 0
	    || dssh_test_register_zlib_comp() != 0
	    || dssh_register_none_comp() != 0
	    || dssh_ed25519_generate_key() != 0)
		return 1;
	dssh_session sess = dssh_session_init(false, 0);
	if (sess == NULL)
		return 1;
	struct dssh_auth_server_cbs auth = { .methods_str = "", .none_cb = accept_none };
	struct dssh_chan_accept_cbs accept = { .exec = accept_echo };
	const char *stage = "handshake";
	unsigned query_checks = 0;
	int ret = dssh_transport_handshake(sess);
	if (ret == 0) {
		ret = check_negotiated(sess, argv[1], argv[2]);
		if (ret == 0)
			query_checks++;
	}
	if (ret == 0) {
		stage = "auth";
		ret = dssh_auth_server(sess, &auth, NULL, NULL);
	}
	if (ret == 0) {
		stage = "start";
		ret = dssh_session_start(sess);
	}
	if (ret == 0) {
		stage = "accept";
		dssh_channel channel = dssh_chan_accept(sess, &accept, 10000);
		if (channel == NULL)
			ret = DSSH_ERROR_INIT;
		else {
			stage = "echo";
			ret = echo_channel(channel);
			int close_ret = dssh_chan_close(channel, ret == 0 ? 0 : 1);
			if (ret == 0)
				ret = close_ret;
		}
	}
	if (ret == 0) {
		stage = "peer-disconnect";
		ret = wait_for_peer_disconnect(sess);
	}
	if (ret == 0) {
		ret = check_negotiated(sess, argv[1], argv[2]);
		if (ret == 0)
			query_checks++;
	}
	dssh_session_cleanup(sess);
	unsigned allocated, live, errors;
	dssh_test_zlib_stats(&allocated, &live, &errors);
	unsigned resets = dssh_example_zlib_resets();
	fprintf(stderr, "DeuceSSH echo: stage=%s result=%d zlib-allocations=%u live=%u cleanup-errors=%u resets=%u query-checks=%u\n",
	    stage, ret, allocated, live, errors, resets, query_checks);
	/* The independent peer must actually exercise rekey and both dictionaries. */
	return ret == 0 && live == 0 && errors == 0 && resets >= 2 && query_checks == 2 ? 0 : 1;
}
