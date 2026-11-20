/*
 * test_selftest.c -- Tier 3a integration tests: DeuceSSH client <-> server
 * over socketpair(AF_UNIX, SOCK_STREAM).
 *
 * Each test creates a Unix socketpair, runs client and server in
 * separate threads, and exercises the full SSH stack: handshake,
 * authentication, channel I/O, signals, window-change, rekey, and
 * graceful disconnect.
 */

#include <errno.h>
#include <limits.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string.h>
#include <threads.h>

#include "dssh_test.h"
#include "dssh_test_wait.h"
#include "deucessh.h"
#include "deucessh-auth.h"
#include "deucessh-conn.h"
#include "deucessh-algorithms.h"
#include "ssh-trans.h"
#include "ssh-internal.h"
#include "dssh_test_internal.h"
#include "dssh_test_ossl.h"
#include "test_dhgex_provider.h"
#include "test_comp.h"
#include "compression_fixture.h"
#include "fixture_writer.h"
#include "selftest_metadata.h"
#include "selftest_rekey.h"

/* ================================================================
 * Socket I/O callbacks
 *
 * cbdata is a pointer to the socket fd for this session.
 * Both client and server use the same callback functions.
 * ================================================================ */

#include "auth_result_fixture.h"

static _Atomic(dssh_session) delayed_tx_sess;
static atomic_int   delayed_tx_countdown;
static atomic_bool  delayed_tx_seen;
static atomic_bool  delayed_tx_release;
static atomic_bool  delayed_tx_done;
static atomic_bool  delayed_rx_seen;
static _Atomic(dssh_session) callback_probe_sess;
static _Atomic(dssh_session) callback_probe_other_sess;
static atomic_int   callback_probe_kind;
static atomic_int   callback_probe_api_result;
static atomic_int   callback_probe_other_api_result;
static atomic_int   callback_probe_lock_result;
static atomic_bool  callback_probe_is_terminated;
static atomic_bool  callback_probe_terminate_result;

enum callback_probe_kind {
	CALLBACK_PROBE_NONE,
	CALLBACK_PROBE_TX,
	CALLBACK_PROBE_RX,
	CALLBACK_PROBE_RUNNING
};

/* Keep the application callbacks interruptible by session termination.
 * Do not depend on cross-thread shutdown/close ordering to wake a blocking
 * AF_UNIX socket operation on every supported platform. */
static int
socket_wait(int fd, short events, dssh_session sess)
{
	if (fd < 0 || sess == NULL || (events != POLLIN && events != POLLOUT))
		return -1;
	for (;;) {
		if (dssh_session_is_terminated(sess))
			return -1;
		struct pollfd pfd = {
			.fd = fd,
			.events = events,
		};
		int ret = poll(&pfd, 1, 100);

		if (ret < 0 && errno == EINTR)
			continue;
		if (ret < 0)
			return -1;
		if (ret == 0)
			continue;
		if (ret != 1 || (pfd.revents & POLLNVAL) != 0)
			return -1;
		if ((pfd.revents & events) != 0
		    || (pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) != 0)
			return 0;
	}
}

static void
run_callback_probe(dssh_session sess, enum callback_probe_kind kind)
{
	if (sess != atomic_load(&callback_probe_sess))
		return;
	int expected = (int)kind;

	if (!atomic_compare_exchange_strong(&callback_probe_kind, &expected, CALLBACK_PROBE_RUNNING))
		return;
	atomic_store(&callback_probe_is_terminated, dssh_session_is_terminated(sess));
	atomic_store(&callback_probe_api_result, dssh_session_set_rekey_seconds(sess, 1));
	atomic_store(&callback_probe_other_api_result,
	    dssh_session_set_rekey_seconds(atomic_load(&callback_probe_other_sess), 1));
	atomic_store(&callback_probe_terminate_result, dssh_session_terminate(sess));
	int lock_result = mtx_trylock(&sess->trans.tx_mtx);

	atomic_store(&callback_probe_lock_result, lock_result);
	if (lock_result == thrd_success && mtx_unlock(&sess->trans.tx_mtx) != thrd_success)
		abort();
	atomic_store(&callback_probe_kind, CALLBACK_PROBE_NONE);
}

static size_t
socket_request(size_t remaining)
{
	uintmax_t maximum = (uintmax_t)SSIZE_MAX;
	if (remaining > maximum) {
		size_t bounded = (size_t)maximum;
		return bounded;
	}
	return remaining;
}

static bool
socket_count(ssize_t value, size_t requested, size_t *count)
{
	if (value <= 0)
		return false;
	uintmax_t magnitude = (uintmax_t)value;
	if (magnitude > requested)
		return false;
	size_t bounded = (size_t)magnitude;
	*count = bounded;
	return true;
}

static bool
socket_parameters(const uint8_t *buf, size_t length, dssh_session sess, void *data)
{
	return sess != NULL && data != NULL && *(int *)data >= 0
	    && (length == 0 || buf != NULL);
}

static int
socket_tx(uint8_t *buf, size_t bufsz, dssh_session sess, void *cbdata)
{
	if (!socket_parameters(buf, bufsz, sess, cbdata))
		return -1;
	run_callback_probe(sess, CALLBACK_PROBE_TX);
	int fd = *(int *)cbdata;
	size_t sent = 0;
	while (sent < bufsz) {
		if (socket_wait(fd, POLLOUT, sess) < 0)
			return -1;
		size_t requested = socket_request(bufsz - sent);
		ssize_t value = send(fd, buf + sent, requested, 0);
		if (value < 0 && errno == EINTR)
			continue;
		size_t count;
		if (!socket_count(value, requested, &count))
			return -1;
		sent += count;
	}
	if (sess == atomic_load(&delayed_tx_sess)) {
		int remaining = atomic_load(&delayed_tx_countdown);
		while (remaining > 0) {
			if (!atomic_compare_exchange_weak(&delayed_tx_countdown, &remaining, remaining - 1))
				continue;
			if (remaining == 1) {
				atomic_store(&delayed_tx_seen, true);
				while (!atomic_load(&delayed_tx_release)) {
					if (dssh_session_is_terminated(sess))
						return -1;
					thrd_yield();
				}
				atomic_store(&delayed_tx_done, true);
			}
			break;
		}
	}
	return 0;
}

static int
socket_rx(uint8_t *buf, size_t bufsz, dssh_session sess, void *cbdata)
{
	if (!socket_parameters(buf, bufsz, sess, cbdata))
		return -1;
	run_callback_probe(sess, CALLBACK_PROBE_RX);
	int fd = *(int *)cbdata;
	size_t got = 0;
	while (got < bufsz) {
		if (socket_wait(fd, POLLIN, sess) < 0)
			return -1;
		size_t requested = socket_request(bufsz - got);
		ssize_t value = recv(fd, buf + got, requested, 0);
		if (value < 0 && errno == EINTR)
			continue;
		size_t count;
		if (!socket_count(value, requested, &count))
			return -1;
		got += count;
	}
	if (bufsz != 0 && sess == atomic_load(&delayed_tx_sess)
	    && atomic_load(&delayed_tx_seen) && !atomic_load(&delayed_tx_release))
		atomic_store(&delayed_rx_seen, true);
	return 0;
}

static int
socket_rxline(uint8_t *buf, size_t bufsz, size_t *bytes_received,
    dssh_session sess, void *cbdata)
{
	if (bytes_received == NULL)
		return -1;
	*bytes_received = 0;
	if (bufsz == 0 || !socket_parameters(buf, bufsz, sess, cbdata))
		return -1;
	int fd = *(int *)cbdata;
	size_t have = 0;
	while (have < bufsz) {
		if (socket_wait(fd, POLLIN, sess) < 0)
			return -1;
		ssize_t value = recv(fd, &buf[have], 1, 0);
		if (value < 0 && errno == EINTR)
			continue;
		if (value != 1)
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
 * Algorithm registration helper
 * ================================================================ */

static dssh_session
init_server_session(void)
{
	dssh_session s = dssh_session_init(false, 0);
	if (s != NULL)
		test_dhgex_setup(s);
	return s;
}

static int
register_all_algorithms(void)
{
	int res;
	res = test_register_kex();
	if (res < 0)
		return res;
	res = test_register_key_algos();
	if (res < 0)
		return res;
	res = dssh_register_aes256_ctr();
	if (res < 0)
		return res;
	res = test_register_mac_algos();
	if (res < 0)
		return res;
	res = dssh_test_register_optional_comp();
	if (res < 0)
		return res;
	res = dssh_register_none_comp();
	if (res < 0)
		return res;
	return 0;
}

/* ================================================================
 * Self-test context and setup/teardown
 * ================================================================ */

struct selftest_ctx {
	int client_fd;
	int server_fd;
	dssh_session client;
	dssh_session server;
	/* Main owns the client slot and params; the server worker owns its slot.
	 * Cleanup may inspect the server slot only after joining that worker. */
	dssh_channel client_channel;
	dssh_channel server_channel;
	struct dssh_chan_params params;
	bool params_active;
	int client_hs_result;
	int server_hs_result;
	int client_auth_result;
	int server_auth_result;
	thrd_t setup_thread[2];
	bool setup_thread_active[2];
	thrd_t server_thread;
	bool server_thread_active;
	bool registered;
};

static struct selftest_ctx *g_active_ctx;

static int
selftest_params_init(struct selftest_ctx *ctx, enum dssh_chan_type type)
{
	if (ctx->params_active)
		abort();
	ctx->params_active = true;
	return dssh_chan_params_init(&ctx->params, type);
}

static void
selftest_params_free(struct selftest_ctx *ctx)
{
	if (!ctx->params_active)
		return;
	ctx->params_active = false;
	dssh_chan_params_free(&ctx->params);
}

static dssh_channel
selftest_open(struct selftest_ctx *ctx, const struct dssh_chan_params *params)
{
	if (ctx->client_channel != NULL)
		abort();
	ctx->client_channel = dssh_chan_open(ctx->client, params);
	return ctx->client_channel;
}

static dssh_channel
selftest_accept(struct selftest_ctx *ctx, const struct dssh_chan_accept_cbs *cbs,
    int timeout_ms)
{
	if (ctx->server_channel != NULL)
		abort();
	ctx->server_channel = dssh_chan_accept(ctx->server, cbs, timeout_ms);
	return ctx->server_channel;
}

static int
selftest_channel_close(struct selftest_ctx *ctx, bool client,
    dssh_channel ch, int64_t exit_code)
{
	dssh_channel *owner = client ? &ctx->client_channel : &ctx->server_channel;
	if (ch == NULL || *owner != ch)
		abort();
	/* Ordinary close consumes the handle on every return. */
	*owner = NULL;
	return dssh_chan_close(ch, exit_code);
}

static void
selftest_channel_cleanup(struct selftest_ctx *ctx, bool client)
{
	dssh_channel ch = client ? ctx->client_channel : ctx->server_channel;
	if (ch == NULL)
		return;
	int ret = selftest_channel_close(ctx, client, ch, -1);
	if (ret != 0 && ret != DSSH_ERROR_TERMINATED)
		abort();
}

static void
selftest_shutdown(int fd)
{
	if (fd >= 0 && shutdown(fd, SHUT_RDWR) < 0 && errno != ENOTCONN)
		abort();
}

static void
selftest_cancel(struct selftest_ctx *ctx)
{
	/* Keep descriptors allocated until every borrower has joined. */
	selftest_shutdown(ctx->client_fd);
	selftest_shutdown(ctx->server_fd);
	if (ctx->server)
		dssh_session_terminate(ctx->server);
	if (ctx->client)
		dssh_session_terminate(ctx->client);
}

static void
selftest_join(thrd_t *thread, bool *active)
{
	if (!*active)
		return;
	/* An unsuccessful native join leaves ownership unknown. Do not retry
	 * the handle or retire any storage that its worker may still borrow. */
	if (thrd_join(*thread, NULL) != thrd_success)
		abort();
	*active = false;
}

static void
selftest_cleanup(struct selftest_ctx *ctx)
{
	if (!ctx->registered)
		return;
	if (g_active_ctx != ctx)
		abort();
	atomic_store(&delayed_tx_release, true);
	dssh_test_clock_reset();
	dssh_test_wait_observe_end();
	dssh_test_channel_observe_end();
	selftest_cancel(ctx);
	for (unsigned i = 0; i < 2; i++)
		selftest_join(&ctx->setup_thread[i], &ctx->setup_thread_active[i]);
	selftest_join(&ctx->server_thread, &ctx->server_thread_active);
	selftest_channel_cleanup(ctx, false);
	selftest_channel_cleanup(ctx, true);
	selftest_params_free(ctx);
	dssh_session server = ctx->server;
	dssh_session client = ctx->client;
	ctx->server = NULL;
	ctx->client = NULL;
	dssh_session_cleanup(server);
	dssh_session_cleanup(client);
	int fds[] = { ctx->client_fd, ctx->server_fd };
	ctx->client_fd = -1;
	ctx->server_fd = -1;
	for (unsigned i = 0; i < 2; i++) {
		/* Do not retry a native release with an unknown outcome. */
		if (fds[i] >= 0 && close(fds[i]) < 0)
			abort();
	}
	atomic_store(&delayed_tx_sess, NULL);
	atomic_store(&delayed_tx_countdown, 0);
	atomic_store(&delayed_tx_seen, false);
	atomic_store(&delayed_tx_done, false);
	atomic_store(&delayed_rx_seen, false);
	atomic_store(&callback_probe_sess, NULL);
	atomic_store(&callback_probe_other_sess, NULL);
	atomic_store(&callback_probe_kind, CALLBACK_PROBE_NONE);
	dssh_test_reset_global_config();
	ctx->registered = false;
	g_active_ctx = NULL;
}

static int
selftest_start_thread(struct selftest_ctx *ctx,
    thrd_start_t func, void *arg)
{
	if (!ctx->registered || ctx->server_thread_active || func == NULL)
		return -1;
	if (thrd_create(&ctx->server_thread, func, arg) != thrd_success)
		return -1;
	ctx->server_thread_active = true;
	return 0;
}

/* Server auth callback: accept any password */
static int
auth_password_cb(const uint8_t *username, size_t username_len,
    const uint8_t *password, size_t password_len,
    struct dssh_bytes_result *output_result,
    void *cbdata)
{
	uint8_t *fixture_output = NULL;
	size_t fixture_length = 0;
	uint8_t **change_prompt = &fixture_output;
	size_t *change_prompt_len = &fixture_length;
	(void)change_prompt;
	(void)change_prompt_len;

	return test_auth_output_finish(output_result, fixture_output, fixture_length, DSSH_AUTH_SUCCESS);
}

static struct dssh_auth_server_cbs selftest_auth_cbs = {
	.methods_str = "password",
	.password_cb = auth_password_cb,
};

/* ================================================================
 * Handshake threads
 * ================================================================ */

static int
client_handshake_thread(void *arg)
{
	struct selftest_ctx *ctx = arg;
	ctx->client_hs_result = dssh_transport_handshake(ctx->client);
	if (ctx->client_hs_result != 0)
		selftest_cancel(ctx);
	return 0;
}

static int
server_handshake_thread(void *arg)
{
	struct selftest_ctx *ctx = arg;
	ctx->server_hs_result = dssh_transport_handshake(ctx->server);
	if (ctx->server_hs_result != 0)
		selftest_cancel(ctx);
	return 0;
}

/* ================================================================
 * Auth threads
 * ================================================================ */

static int
client_auth_thread(void *arg)
{
	struct selftest_ctx *ctx = arg;
	ctx->client_auth_result = dssh_auth_password(ctx->client,
	    "testuser", "testpass", NULL, NULL);
	if (ctx->client_auth_result != 0)
		selftest_cancel(ctx);
	return 0;
}

static int
server_auth_thread(void *arg)
{
	struct selftest_ctx *ctx = arg;
	uint8_t username[256];
	size_t ulen = sizeof(username);
	ctx->server_auth_result = dssh_auth_server(ctx->server,
	    &selftest_auth_cbs, username, &ulen);
	if (ctx->server_auth_result != 0)
		selftest_cancel(ctx);
	return 0;
}

static int
selftest_start_setup_thread(struct selftest_ctx *ctx, unsigned index,
    thrd_start_t operation)
{
	if (!ctx->registered || index >= 2 || ctx->setup_thread_active[index]
	    || operation == NULL)
		return -1;
	if (thrd_create(&ctx->setup_thread[index], operation, ctx) != thrd_success)
		return -1;
	ctx->setup_thread_active[index] = true;
	return 0;
}

static int
selftest_run_setup_pair(struct selftest_ctx *ctx,
    thrd_start_t client, thrd_start_t server)
{
	if (selftest_start_setup_thread(ctx, 0, client) < 0
	    || selftest_start_setup_thread(ctx, 1, server) < 0)
		return -1;
	for (unsigned i = 0; i < 2; i++)
		selftest_join(&ctx->setup_thread[i], &ctx->setup_thread_active[i]);
	return 0;
}

/* ================================================================
 * selftest_setup: socketpair, register algos, handshake, auth
 *
 * Returns 0 on success with sessions ready for session_start().
 * On failure returns -1 and cleans up.
 * ================================================================ */

static void
selftest_begin(struct selftest_ctx *ctx)
{
	if (g_active_ctx != NULL)
		abort();
	*ctx = (struct selftest_ctx){.client_fd = -1, .server_fd = -1,
	    .client_hs_result = DSSH_ERROR_INIT, .server_hs_result = DSSH_ERROR_INIT,
	    .client_auth_result = DSSH_ERROR_INIT, .server_auth_result = DSSH_ERROR_INIT,
	    .registered = true};
	g_active_ctx = ctx;

}

static int
selftest_standalone(struct selftest_ctx *ctx)
{
	selftest_begin(ctx);
	dssh_test_reset_global_config();
	ctx->client = dssh_session_init(true, 0);
	if (ctx->client != NULL)
		return 0;
	selftest_cleanup(ctx);
	return -1;
}

static int
selftest_prepare(struct selftest_ctx *ctx)
{
	selftest_begin(ctx);
	dssh_test_reset_global_config();

	if (register_all_algorithms() < 0)
		goto fail;
	if (test_generate_host_key() < 0)
		goto fail;

	int fds[2];
	if (socketpair(AF_UNIX, SOCK_STREAM, 0, fds) < 0)
		goto fail;
	ctx->client_fd = fds[0];
	ctx->server_fd = fds[1];

	if (dssh_transport_set_callbacks(socket_tx, socket_rx,
	        socket_rxline, NULL) != 0)
		goto fail;

	ctx->client = dssh_session_init(true, 0);
	if (ctx->client == NULL)
		goto fail;
	if (dssh_session_set_cbdata(ctx->client,
	    &ctx->client_fd, &ctx->client_fd,
	    &ctx->client_fd, &ctx->client_fd) != 0)
		goto fail;
	if (dssh_session_set_hostkey_verify_cb(ctx->client,
	        dssh_test_accept_hostkey, NULL) != 0)
		goto fail;

	ctx->server = init_server_session();
	if (ctx->server == NULL)
		goto fail;
	if (dssh_session_set_cbdata(ctx->server,
	    &ctx->server_fd, &ctx->server_fd,
	    &ctx->server_fd, &ctx->server_fd) != 0)
		goto fail;
	return 0;

fail:
	selftest_cleanup(ctx);
	return -1;
}

static int
selftest_setup(struct selftest_ctx *ctx)
{
	if (selftest_prepare(ctx) < 0)
		return -1;

	/* Run handshake in two threads */
	if (selftest_run_setup_pair(ctx, client_handshake_thread,
	        server_handshake_thread) < 0)
		goto fail;

	if (ctx->client_hs_result != 0 || ctx->server_hs_result != 0
	    || !compression_fixture_selected(ctx->client, ctx->server))
		goto fail;

	/* Run auth in two threads */
	if (selftest_run_setup_pair(ctx, client_auth_thread,
	        server_auth_thread) < 0)
		goto fail;

	if (ctx->client_auth_result != 0 || ctx->server_auth_result != 0)
		goto fail;

	return 0;

fail:
	selftest_cleanup(ctx);
	return -1;
}

/* ================================================================
 * Server accept callbacks
 * ================================================================ */

static int
selftest_accept_shell(dssh_channel ch, const struct dssh_chan_params *p,
    struct dssh_chan_accept_result *result, void *cbdata)
{
	(void)ch; (void)p; (void)result; (void)cbdata;
	return 0;
}

static int
selftest_accept_exec(dssh_channel ch, const struct dssh_chan_params *p,
    struct dssh_chan_accept_result *result, void *cbdata)
{
	(void)ch; (void)p; (void)result; (void)cbdata;
	return 0;
}

static struct dssh_chan_accept_cbs selftest_accept_cbs = {
	.shell = selftest_accept_shell,
	.exec = selftest_accept_exec,
};

/* Validate the native API extent before narrowing or advancing a buffer. */
static bool
selftest_io_count(int64_t value, size_t capacity, size_t *count)
{
	if (value < 0)
		return false;
	uintmax_t magnitude = (uintmax_t)value;
	if (magnitude > capacity)
		return false;
	size_t bounded = (size_t)magnitude;
	*count = bounded;
	return true;
}

static bool
selftest_write_all(dssh_channel ch, const uint8_t *buf, size_t length,
    int timeout_ms)
{
	if (length != 0 && buf == NULL)
		return false;
	size_t offset = 0;
	while (offset < length) {
		if (dssh_chan_poll(ch, DSSH_POLL_WRITE, timeout_ms) != DSSH_POLL_WRITE)
			return false;
		int64_t value = dssh_chan_write(ch, 0, buf + offset, length - offset);
		size_t count;
		if (!selftest_io_count(value, length - offset, &count) || count == 0)
			return false;
		offset += count;
	}
	return true;
}

static bool
selftest_read_some(dssh_channel ch, uint8_t *buf, size_t capacity,
    size_t *count, int timeout_ms)
{
	if (capacity == 0 || buf == NULL)
		return false;
	if (dssh_chan_poll(ch, DSSH_POLL_READ, timeout_ms) != DSSH_POLL_READ)
		return false;
	int64_t value = dssh_chan_read(ch, 0, buf, capacity);
	return selftest_io_count(value, capacity, count);
}

static bool
selftest_read_exact(dssh_channel ch, uint8_t *buf, size_t length,
    int timeout_ms)
{
	if (length != 0 && buf == NULL)
		return false;
	size_t offset = 0;
	while (offset < length) {
		size_t count;
		if (!selftest_read_some(ch, buf + offset, length - offset, &count,
		        timeout_ms) || count == 0)
			return false;
		offset += count;
	}
	return true;
}

static bool
selftest_read_eof(dssh_channel ch, int timeout_ms)
{
	uint8_t byte;
	size_t count;
	return selftest_read_some(ch, &byte, sizeof(byte), &count, timeout_ms)
	    && count == 0;
}

static bool
selftest_echo(dssh_channel ch)
{
	uint8_t buf[4096];
	for (;;) {
		size_t count;
		if (!selftest_read_some(ch, buf, sizeof(buf), &count, 30000))
			return false;
		if (count == 0)
			return true;
		if (!selftest_write_all(ch, buf, count, 30000))
			return false;
	}
}

/* Collect a complete peer close, including every expected byte and event.
 * Once read EOF is observed, poll only events so EOF readiness cannot spin
 * past a CLOSE packet that the demux has not dispatched yet. */
static bool
selftest_collect(dssh_channel ch, uint8_t *output, size_t length,
    const char *signal, int64_t status)
{
	size_t used = 0;
	bool got_signal = false;
	bool got_status = false;
	bool got_eof = false;
	bool got_close = false;
	bool read_eof = false;
	while (!got_eof || !got_close || !read_eof) {
		int wanted = DSSH_POLL_EVENT | (read_eof ? 0 : DSSH_POLL_READ);
		int events = dssh_chan_poll(ch, wanted, 1000);
		if (events <= 0 || (events & ~wanted) != 0)
			return false;
		if (events & DSSH_POLL_EVENT) {
			struct dssh_chan_event event;
			if (dssh_chan_read_event(ch, &event) != 0
			    || event.stdout_pos > length - used || event.stderr_pos != 0)
				return false;
			switch (event.type) {
				case DSSH_EVENT_SIGNAL:
					if (got_signal || signal == NULL
					    || !dssh_test_bytes_equal(event.signal.name, signal, strlen(signal)))
						return false;
					got_signal = true;
					break;
				case DSSH_EVENT_EXIT_STATUS:
					if (got_status || status < 0 || event.exit_status.exit_code != status)
						return false;
					got_status = true;
					break;
				case DSSH_EVENT_EOF:
					if (got_eof)
						return false;
					got_eof = true;
					break;
				case DSSH_EVENT_CLOSE:
					if (got_close)
						return false;
					got_close = true;
					break;
				default:
					return false;
			}
		}
		if (events & DSSH_POLL_READ) {
			uint8_t extra;
			size_t room = used < length ? length - used : 1;
			uint8_t *destination = used < length ? output + used : &extra;
			int64_t value = dssh_chan_read(ch, 0, destination, room);
			size_t count;
			if (!selftest_io_count(value, room, &count) || count > length - used)
				return false;
			used += count;
			read_eof = count == 0;
		}
	}
	return used == length && got_signal == (signal != NULL)
	    && got_status == (status >= 0);
}

static bool
selftest_pattern_roundtrip(dssh_channel ch, uint32_t total, uint32_t *completed)
{
	/* Each object fits 16-bit size_t; the cumulative workload is never narrowed.
	 * Retain both buffers for all chunks and compare every received byte. */
	uint8_t sent[4096];
	uint8_t received[4096];
	*completed = 0;
	while (*completed < total) {
		uint32_t remaining = total - *completed;
		size_t length = sizeof(sent);
		if (remaining < length) {
			size_t tail = (size_t)remaining;
			length = tail;
		}
		for (size_t i = 0; i < length; i++) {
			uint32_t offset = (uint32_t)i;
			uint8_t byte = (uint8_t)((*completed + offset) & UINT32_C(255));
			sent[i] = byte;
		}
		if (!selftest_write_all(ch, sent, length, 30000)
		    || !selftest_read_exact(ch, received, length, 30000)
		    || memcmp(sent, received, length) != 0)
			return false;
		uint32_t step = (uint32_t)length; /* length <= 4096 and remaining */
		*completed += step;
	}
	return true;
}

/* ================================================================
 * Server-side thread helpers
 * ================================================================ */

/* Server: accept channel, echo all input, close with exit_code 0 */
struct server_echo_arg {
	struct selftest_ctx *ctx;
	int result;
};

static int
server_echo_thread(void *arg)
{
	struct server_echo_arg *a = arg;
	struct selftest_ctx *ctx = a->ctx;
	a->result = -1;

	dssh_channel ch = selftest_accept(ctx,
	    &selftest_accept_cbs, 30000);

	if (ch == NULL)
		return 0;

	bool complete = selftest_echo(ch) && !dssh_session_is_terminated(ctx->server);
	int ret = selftest_channel_close(ctx, false, ch, 0);
	if (complete && ret == 0)
		a->result = 0;
	return 0;
}

static int
fail_with_active_server(void)
{
	struct selftest_ctx ctx;

	if (selftest_setup(&ctx) < 0)
		return TEST_SKIP;
	ASSERT_EQ(dssh_session_start(ctx.client), 0);
	ASSERT_EQ(dssh_session_start(ctx.server), 0);
	struct server_echo_arg sarg = { .ctx = &ctx };

	ASSERT_EQ(selftest_start_thread(&ctx, server_echo_thread, &sarg), 0);
	/* The failure hook must join the server before sarg leaves scope. */
	ASSERT_TRUE(false);
	return TEST_PASS;
}

static int
test_assertion_cleanup(void)
{
	ASSERT_EQ(fail_with_active_server(), TEST_FAIL);
	ASSERT_NULL(g_active_ctx);
	return TEST_PASS;
}

/* Server: accept channel, close immediately with specific exit code */
struct server_exit_arg {
	struct selftest_ctx *ctx;
	uint32_t exit_code;
	int result;
};

static int
server_exit_thread(void *arg)
{
	struct server_exit_arg *a = arg;
	struct selftest_ctx *ctx = a->ctx;
	a->result = -1;

	dssh_channel ch = selftest_accept(ctx,
	    &selftest_accept_cbs, 30000);

	if (ch == NULL)
		return 0;

	int64_t exit_code = (int64_t)a->exit_code;
	a->result = selftest_channel_close(ctx, false, ch, exit_code);
	return 0;
}

/* Server: accept channel, write data, send signal, write more, close */
struct server_signal_arg {
	struct selftest_ctx *ctx;
	const char *signal_name;
	int result;
};

static int
server_signal_thread(void *arg)
{
	struct server_signal_arg *a = arg;
	struct selftest_ctx *ctx = a->ctx;
	a->result = -1;
	dssh_channel ch = selftest_accept(ctx, &selftest_accept_cbs, 30000);
	if (ch == NULL)
		return 0;
	const uint8_t before[] = "before-signal";
	const uint8_t after[] = "after-signal";
	bool complete = selftest_write_all(ch, before, sizeof(before) - 1, 30000)
	    && dssh_chan_send_signal(ch, a->signal_name) == 0
	    && selftest_write_all(ch, after, sizeof(after) - 1, 30000);
	int ret = selftest_channel_close(ctx, false, ch, 0);
	if (complete && ret == 0)
		a->result = 0;
	return 0;
}


/* Server: accept channel, poll for window-change event, read until EOF */
struct server_wc_arg {
	struct selftest_ctx *ctx;
	bool wc_received;
	uint32_t wc_cols, wc_rows, wc_wpx, wc_hpx;
	int result;
};

static int
server_wc_thread(void *arg)
{
	struct server_wc_arg *a = arg;
	struct selftest_ctx *ctx = a->ctx;
	a->result = -1;
	a->wc_received = false;

	dssh_channel ch = selftest_accept(ctx,
	    &selftest_accept_cbs, 30000);

	if (ch == NULL)
		return 0;

	uint8_t buf[256];
	bool complete = false;

	for (;;) {
		int ev = dssh_chan_poll(ch,
		    DSSH_POLL_READ | DSSH_POLL_EVENT, 30000);

		if (ev <= 0)
			break;

		if (ev & DSSH_POLL_EVENT) {
			struct dssh_chan_event event;

			if (dssh_chan_read_event(ch, &event) != 0)
				break;
			if (event.type == DSSH_EVENT_WINDOW_CHANGE) {
				a->wc_received = true;
				a->wc_cols = event.window_change.cols;
				a->wc_rows = event.window_change.rows;
				a->wc_wpx = event.window_change.wpx;
				a->wc_hpx = event.window_change.hpx;
			}
			continue;
		}

		if (ev & DSSH_POLL_READ) {
			int64_t n = dssh_chan_read(ch, 0,
			    buf, sizeof(buf));

			/* This scenario sends no channel data. Only real EOF completes it. */
			complete = n == 0 && !dssh_session_is_terminated(ctx->server);
			break;
		}
	}

	int ret = selftest_channel_close(ctx, false, ch, 0);
	if (complete && a->wc_received && ret == 0)
		a->result = 0;
	return 0;
}

/* Server: accept two channels sequentially, echo one message each */
struct server_multi_arg {
	struct selftest_ctx *ctx;
	int channels_handled;
	int result;
};

static int
server_multi_thread(void *arg)
{
	struct server_multi_arg *a = arg;
	struct selftest_ctx *ctx = a->ctx;
	a->result = -1;
	a->channels_handled = 0;
	for (int i = 0; i < 2; i++) {
		dssh_channel ch = selftest_accept(ctx, &selftest_accept_cbs, 30000);
		if (ch == NULL)
			return 0;
		const char *expected = i == 0 ? "channel-one" : "channel-two";
		uint8_t buf[sizeof("channel-one") - 1];
		bool complete = selftest_read_exact(ch, buf, sizeof(buf), 30000)
		    && memcmp(buf, expected, sizeof(buf)) == 0
		    && selftest_write_all(ch, buf, sizeof(buf), 30000);
		int ret = selftest_channel_close(ctx, false, ch, 0);
		if (!complete || ret != 0)
			return 0;
		a->channels_handled++;
	}
	a->result = 0;
	return 0;
}


/* Server: accept channel, wait for disconnect */
struct server_disconnect_arg {
	struct selftest_ctx *ctx;
	int result;
};

static int
server_disconnect_thread(void *arg)
{
	struct server_disconnect_arg *a = arg;
	struct selftest_ctx *ctx = a->ctx;
	a->result = -1;

	dssh_channel ch = selftest_accept(ctx,
	    &selftest_accept_cbs, 30000);

	if (ch == NULL) {
		if (dssh_session_is_terminated(ctx->server))
			a->result = 0;
		return 0;
	}

	uint8_t buf[256];

	size_t count;
	bool complete = selftest_read_some(ch, buf, sizeof(buf), &count, 30000)
	    && count == 0 && dssh_session_is_terminated(ctx->server);
	int ret = selftest_channel_close(ctx, false, ch, 0);
	if (complete && (ret == 0 || ret == DSSH_ERROR_TERMINATED))
		a->result = 0;
	return 0;
}

/* Server: accept channel, read all input, write reply, close */
struct server_halfclose_arg {
	struct selftest_ctx *ctx;
	int result;
};

static int
server_halfclose_thread(void *arg)
{
	struct server_halfclose_arg *a = arg;
	struct selftest_ctx *ctx = a->ctx;
	a->result = -1;
	dssh_channel ch = selftest_accept(ctx, &selftest_accept_cbs, 30000);
	if (ch == NULL)
		return 0;
	const uint8_t expected[] = "client-data";
	const uint8_t reply[] = "server-reply-after-eof";
	uint8_t input[sizeof(expected) - 1];
	bool complete = selftest_read_exact(ch, input, sizeof(input), 30000)
	    && memcmp(input, expected, sizeof(input)) == 0
	    && selftest_read_eof(ch, 30000)
	    && !dssh_session_is_terminated(ctx->server)
	    && selftest_write_all(ch, reply, sizeof(reply) - 1, 30000);
	int ret = selftest_channel_close(ctx, false, ch, 0);
	if (complete && ret == 0)
		a->result = 0;
	return 0;
}


/* ================================================================
 * Helpers for opening channels
 * ================================================================ */

static dssh_channel
open_exec(struct selftest_ctx *ctx, const char *cmd)
{
	struct dssh_chan_params *p = &ctx->params;
	int ret = selftest_params_init(ctx, DSSH_CHAN_EXEC);
	if (ret == 0)
		ret = dssh_chan_params_set_command(p, cmd);
	if (ret == 0)
		ret = dssh_chan_params_set_pty(p, false);
	dssh_channel ch = NULL;
	if (ret == 0)
		ch = selftest_open(ctx, p);
	selftest_params_free(ctx);
	return ch;
}

static dssh_channel
open_shell(struct selftest_ctx *ctx, const char *term,
    uint32_t cols, uint32_t rows)
{
	struct dssh_chan_params *p = &ctx->params;
	int ret = selftest_params_init(ctx, DSSH_CHAN_SHELL);
	if (ret == 0)
		ret = dssh_chan_params_set_term(p, term);
	if (ret == 0)
		ret = dssh_chan_params_set_size(p, cols, rows, 0, 0);
	dssh_channel ch = NULL;
	if (ret == 0)
		ch = selftest_open(ctx, p);
	selftest_params_free(ctx);
	return ch;
}

/* ================================================================
 * Test: handshake with the selected algorithms
 * ================================================================ */

static int
test_self_handshake_curve25519(void)
{
	struct selftest_ctx ctx;
	if (selftest_setup(&ctx) < 0)
		return TEST_FAIL;

	/* Verify negotiated algorithms */
	ASSERT_NOT_NULL(dssh_transport_get_kex_name(ctx.client));
	ASSERT_NOT_NULL(dssh_transport_get_hostkey_name(ctx.client));
	ASSERT_NOT_NULL(dssh_transport_get_enc_name(ctx.client, DSSH_CLIENT_TO_SERVER));
	ASSERT_NOT_NULL(dssh_transport_get_mac_name(ctx.client, DSSH_CLIENT_TO_SERVER));

	ASSERT_STR_EQ(dssh_transport_get_kex_name(ctx.client), test_kex_algo_name());
	ASSERT_STR_EQ(dssh_transport_get_kex_name(ctx.server), test_kex_algo_name());
	ASSERT_STR_EQ(dssh_transport_get_hostkey_name(ctx.client),
	    test_key_algo_name());
	ASSERT_STR_EQ(dssh_transport_get_hostkey_name(ctx.server), test_key_algo_name());
	const enum dssh_direction directions[] = { DSSH_CLIENT_TO_SERVER, DSSH_SERVER_TO_CLIENT };
	for (size_t i = 0; i < 2; i++) {
		const char *client_enc = dssh_transport_get_enc_name(ctx.client, directions[i]);
		const char *server_enc = dssh_transport_get_enc_name(ctx.server, directions[i]);
		const char *client_mac = dssh_transport_get_mac_name(ctx.client, directions[i]);
		const char *server_mac = dssh_transport_get_mac_name(ctx.server, directions[i]);
		const char *client_comp = dssh_transport_get_comp_name(ctx.client, directions[i]);
		const char *server_comp = dssh_transport_get_comp_name(ctx.server, directions[i]);
		ASSERT_NOT_NULL(client_enc);
		ASSERT_NOT_NULL(server_enc);
		ASSERT_NOT_NULL(client_mac);
		ASSERT_NOT_NULL(server_mac);
		ASSERT_NOT_NULL(client_comp);
		ASSERT_NOT_NULL(server_comp);
		ASSERT_STR_EQ(client_enc, server_enc);
		ASSERT_STR_EQ(client_mac, server_mac);
		ASSERT_STR_EQ(client_comp, server_comp);
	}

	selftest_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Test: Password authentication succeeds
 * ================================================================ */

static int
test_self_auth_password(void)
{
	struct selftest_ctx ctx;
	if (selftest_setup(&ctx) < 0)
		return TEST_FAIL;

	/* Auth already passed in setup -- verify sessions are live */
	ASSERT_FALSE(dssh_session_is_terminated(ctx.client));
	ASSERT_FALSE(dssh_session_is_terminated(ctx.server));

	selftest_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Test: exec echo -- client exec, server echoes input
 * ================================================================ */

static int
test_self_exec_echo(void)
{
	struct selftest_ctx ctx;
	if (selftest_setup(&ctx) < 0)
		return TEST_FAIL;

	ASSERT_EQ(dssh_session_start(ctx.client), 0);
	ASSERT_EQ(dssh_session_start(ctx.server), 0);
	struct server_echo_arg sarg = { .ctx = &ctx };

	ASSERT_EQ(selftest_start_thread(&ctx, server_echo_thread, &sarg), 0);

	dssh_channel ch = open_exec(&ctx,"echo");
	ASSERT_NOT_NULL(ch);

	/* Wait for server's WINDOW_ADJUST (remote_window starts at 0) */
	int wev = dssh_chan_poll(ch, DSSH_POLL_WRITE, 5000);
	ASSERT_EQ(wev, DSSH_POLL_WRITE);

	const uint8_t data[] = "Hello, DeuceSSH!";
	ASSERT_TRUE(selftest_write_all(ch, data, sizeof(data) - 1, 5000));

	/* Read echo */
	uint8_t buf[256];
	size_t recvd = 0;
	for (int i = 0; i < 50; i++) {
		int ev = dssh_chan_poll(ch,
		    DSSH_POLL_READ, 1000);
		ASSERT_TRUE(ev == 0 || ev == DSSH_POLL_READ);
		if (ev == 0)
			continue;
		int64_t n = dssh_chan_read(ch, 0,
		    buf + recvd, sizeof(buf) - recvd);
		if (n <= 0)
			break;
		size_t count;
		ASSERT_TRUE(selftest_io_count(n, sizeof(buf) - recvd, &count));
		recvd += count;
		if (recvd >= sizeof(data) - 1)
			break;
	}
	ASSERT_EQ_U(recvd, sizeof(data) - 1);
	ASSERT_MEM_EQ(buf, data, sizeof(data) - 1);

	ASSERT_EQ(selftest_channel_close(&ctx, true, ch, 0), 0);
	selftest_join(&ctx.server_thread, &ctx.server_thread_active);
	ASSERT_EQ(sarg.result, 0);

	selftest_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Test: data races back while WINDOW_ADJUST tx callback is active
 * ================================================================ */

static int
test_self_window_adjust_race(void)
{
	struct selftest_ctx ctx;
	if (selftest_setup(&ctx) != 0)
		return TEST_FAIL;
	atomic_store(&delayed_tx_sess, ctx.server);
	atomic_store(&delayed_tx_release, false);
	ASSERT_EQ(dssh_session_start(ctx.client), 0);
	ASSERT_EQ(dssh_session_start(ctx.server), 0);
	struct server_echo_arg arg = { .ctx = &ctx };
	ASSERT_EQ(selftest_start_thread(&ctx, server_echo_thread, &arg), 0);
	dssh_channel ch = open_exec(&ctx, "window-race");
	ASSERT_NOT_NULL(ch);
	const uint8_t first[] = "window-race";
	const uint8_t second[] = "-while-tx-active";
	atomic_store(&delayed_tx_countdown, 1);
	ASSERT_TRUE(selftest_write_all(ch, first, sizeof(first) - 1, 5000));
	ASSERT_TRUE(dssh_test_wait_bool(&delayed_tx_seen, true));
	ASSERT_FALSE(atomic_load(&delayed_tx_done));
	ASSERT_TRUE(selftest_write_all(ch, second, sizeof(second) - 1, 5000));
	ASSERT_TRUE(dssh_test_wait_bool(&delayed_rx_seen, true));
	ASSERT_FALSE(atomic_load(&delayed_tx_release));
	atomic_store(&delayed_tx_release, true);
	const uint8_t expected[] = "window-race-while-tx-active";
	uint8_t received[sizeof(expected) - 1];
	ASSERT_TRUE(selftest_read_exact(ch, received, sizeof(received), 5000));
	ASSERT_MEM_EQ(received, expected, sizeof(received));
	ASSERT_TRUE(atomic_load(&delayed_tx_done));
	ASSERT_EQ(selftest_channel_close(&ctx, true, ch, 0), 0);
	selftest_join(&ctx.server_thread, &ctx.server_thread_active);
	ASSERT_EQ(arg.result, 0);
	selftest_cleanup(&ctx);
	return TEST_PASS;
}


/* ================================================================
 * Test: exec with exit code -- server closes with exit_code=42
 * ================================================================ */

static int
test_self_exec_exit_code(void)
{
	struct selftest_ctx ctx;
	if (selftest_setup(&ctx) != 0)
		return TEST_FAIL;
	ASSERT_EQ(dssh_session_start(ctx.client), 0);
	ASSERT_EQ(dssh_session_start(ctx.server), 0);
	struct server_exit_arg arg = { .ctx = &ctx, .exit_code = 42 };
	ASSERT_EQ(selftest_start_thread(&ctx, server_exit_thread, &arg), 0);
	dssh_channel ch = open_exec(&ctx, "exit42");
	ASSERT_NOT_NULL(ch);
	ASSERT_TRUE(selftest_collect(ch, NULL, 0, NULL, 42));
	ASSERT_FALSE(dssh_session_is_terminated(ctx.client));
	ASSERT_EQ(selftest_channel_close(&ctx, true, ch, 0), 0);
	selftest_join(&ctx.server_thread, &ctx.server_thread_active);
	ASSERT_EQ(arg.result, 0);
	selftest_cleanup(&ctx);
	return TEST_PASS;
}


/* ================================================================
 * Test: shell echo -- client opens shell, server echoes
 * ================================================================ */

static int
test_self_shell_echo(void)
{
	struct selftest_ctx ctx;
	if (selftest_setup(&ctx) < 0)
		return TEST_FAIL;

	ASSERT_EQ(dssh_session_start(ctx.client), 0);
	ASSERT_EQ(dssh_session_start(ctx.server), 0);

	struct server_echo_arg sarg = { .ctx = &ctx };
	ASSERT_EQ(selftest_start_thread(&ctx, server_echo_thread, &sarg), 0);

	dssh_channel ch = open_shell(&ctx, "xterm", 80, 24);
	ASSERT_NOT_NULL(ch);

	/* Wait for server's WINDOW_ADJUST (remote_window starts at 0) */
	int wev = dssh_chan_poll(ch, DSSH_POLL_WRITE, 5000);
	ASSERT_EQ(wev, DSSH_POLL_WRITE);

	const uint8_t data[] = "shell-test";
	ASSERT_TRUE(selftest_write_all(ch, data, sizeof(data) - 1, 5000));

	uint8_t buf[256];
	size_t recvd = 0;
	for (int i = 0; i < 50; i++) {
		int ev = dssh_chan_poll(ch,
		    DSSH_POLL_READ, 1000);
		ASSERT_TRUE(ev == 0 || ev == DSSH_POLL_READ);
		if (ev == 0)
			break;
		int64_t n = dssh_chan_read(ch, 0,
		    buf + recvd, sizeof(buf) - recvd);
		if (n <= 0)
			break;
		size_t count;
		ASSERT_TRUE(selftest_io_count(n, sizeof(buf) - recvd, &count));
		recvd += count;
		if (recvd >= sizeof(data) - 1)
			break;
	}
	ASSERT_EQ_U(recvd, sizeof(data) - 1);
	ASSERT_MEM_EQ(buf, data, sizeof(data) - 1);

	ASSERT_EQ(selftest_channel_close(&ctx, true, ch, 0), 0);
	selftest_join(&ctx.server_thread, &ctx.server_thread_active);
	ASSERT_EQ(sarg.result, 0);

	selftest_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Test: shell large data -- send 64KB through shell channel
 * ================================================================ */

static int
test_self_shell_large_data(void)
{
	struct selftest_ctx ctx;
	if (selftest_setup(&ctx) < 0)
		return TEST_FAIL;
	ASSERT_EQ(dssh_session_start(ctx.client), 0);
	ASSERT_EQ(dssh_session_start(ctx.server), 0);
	struct server_echo_arg sarg = { .ctx = &ctx };
	ASSERT_EQ(selftest_start_thread(&ctx, server_echo_thread, &sarg), 0);
	dssh_channel ch = open_shell(&ctx, "xterm", 80, 24);
	ASSERT_NOT_NULL(ch);
	uint32_t completed;
	ASSERT_TRUE(selftest_pattern_roundtrip(ch, UINT32_C(65536), &completed));
	ASSERT_EQ_U(completed, UINT32_C(65536));
	ASSERT_EQ(selftest_channel_close(&ctx, true, ch, 0), 0);
	selftest_join(&ctx.server_thread, &ctx.server_thread_active);
	ASSERT_EQ(sarg.result, 0);
	selftest_cleanup(&ctx);
	return TEST_PASS;
}


/* ================================================================
 * Test: signal -- server sends signal, client receives after drain
 * ================================================================ */

static int
test_self_signal(void)
{
	struct selftest_ctx ctx;
	if (selftest_setup(&ctx) != 0)
		return TEST_FAIL;
	ASSERT_EQ(dssh_session_start(ctx.client), 0);
	ASSERT_EQ(dssh_session_start(ctx.server), 0);
	struct server_signal_arg arg = { .ctx = &ctx, .signal_name = "TERM" };
	ASSERT_EQ(selftest_start_thread(&ctx, server_signal_thread, &arg), 0);
	dssh_channel ch = open_exec(&ctx, "signal-test");
	ASSERT_NOT_NULL(ch);
	const uint8_t expected[] = "before-signalafter-signal";
	uint8_t received[sizeof(expected) - 1];
	ASSERT_TRUE(selftest_collect(ch, received, sizeof(received), "TERM", 0));
	ASSERT_MEM_EQ(received, expected, sizeof(received));
	ASSERT_FALSE(dssh_session_is_terminated(ctx.client));
	ASSERT_EQ(selftest_channel_close(&ctx, true, ch, 0), 0);
	selftest_join(&ctx.server_thread, &ctx.server_thread_active);
	ASSERT_EQ(arg.result, 0);
	selftest_cleanup(&ctx);
	return TEST_PASS;
}


/* ================================================================
 * Test: window change -- client sends window-change request
 * ================================================================ */

static int
test_self_window_change(void)
{
	struct selftest_ctx ctx;
	if (selftest_setup(&ctx) < 0)
		return TEST_FAIL;

	ASSERT_EQ(dssh_session_start(ctx.client), 0);
	ASSERT_EQ(dssh_session_start(ctx.server), 0);

	struct server_wc_arg sarg = { .ctx = &ctx };
	ASSERT_EQ(selftest_start_thread(&ctx, server_wc_thread, &sarg), 0);

	dssh_channel ch = open_shell(&ctx, "xterm", 80, 24);

	ASSERT_NOT_NULL(ch);

	/* Send window-change */
	ASSERT_EQ(dssh_chan_send_window_change(ch,
	    132, 50, 1056, 800), 0);

	ASSERT_EQ(selftest_channel_close(&ctx, true, ch, 0), 0);
	selftest_join(&ctx.server_thread, &ctx.server_thread_active);

	ASSERT_EQ(sarg.result, 0);
	ASSERT_TRUE(sarg.wc_received);
	ASSERT_EQ_U(sarg.wc_cols, 132);
	ASSERT_EQ_U(sarg.wc_rows, 50);
	ASSERT_EQ_U(sarg.wc_wpx, 1056);
	ASSERT_EQ_U(sarg.wc_hpx, 800);

	selftest_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Test: multiple channels -- open 2 channels sequentially
 * ================================================================ */

static int
test_self_multiple_channels(void)
{
	struct selftest_ctx ctx;
	if (selftest_setup(&ctx) < 0)
		return TEST_FAIL;

	ASSERT_EQ(dssh_session_start(ctx.client), 0);
	ASSERT_EQ(dssh_session_start(ctx.server), 0);

	struct server_multi_arg sarg = { .ctx = &ctx };
	ASSERT_EQ(selftest_start_thread(&ctx, server_multi_thread, &sarg), 0);

	/* First channel */
	dssh_channel ch1 = open_exec(&ctx,"cmd1");
	ASSERT_NOT_NULL(ch1);

	const uint8_t data1[] = "channel-one";
	ASSERT_TRUE(selftest_write_all(ch1, data1, sizeof(data1) - 1, 5000));

	uint8_t buf[256];
	size_t recvd = 0;
	for (int i = 0; i < 50; i++) {
		int ev = dssh_chan_poll(ch1,
		    DSSH_POLL_READ, 1000);
		ASSERT_TRUE(ev == 0 || ev == DSSH_POLL_READ);
		if (ev == 0)
			break;
		int64_t n = dssh_chan_read(ch1, 0,
		    buf + recvd, sizeof(buf) - recvd);
		if (n <= 0)
			break;
		size_t count;
		ASSERT_TRUE(selftest_io_count(n, sizeof(buf) - recvd, &count));
		recvd += count;
		if (recvd >= sizeof(data1) - 1)
			break;
	}
	ASSERT_EQ_U(recvd, sizeof(data1) - 1);
	ASSERT_MEM_EQ(buf, data1, sizeof(data1) - 1);

	ASSERT_EQ(selftest_channel_close(&ctx, true, ch1, 0), 0);

	/* Second channel */
	dssh_channel ch2 = open_exec(&ctx,"cmd2");
	ASSERT_NOT_NULL(ch2);

	const uint8_t data2[] = "channel-two";
	ASSERT_TRUE(selftest_write_all(ch2, data2, sizeof(data2) - 1, 5000));

	recvd = 0;
	for (int i = 0; i < 50; i++) {
		int ev = dssh_chan_poll(ch2,
		    DSSH_POLL_READ, 1000);
		ASSERT_TRUE(ev == 0 || ev == DSSH_POLL_READ);
		if (ev == 0)
			break;
		int64_t n = dssh_chan_read(ch2, 0,
		    buf + recvd, sizeof(buf) - recvd);
		if (n <= 0)
			break;
		size_t count;
		ASSERT_TRUE(selftest_io_count(n, sizeof(buf) - recvd, &count));
		recvd += count;
		if (recvd >= sizeof(data2) - 1)
			break;
	}
	ASSERT_EQ_U(recvd, sizeof(data2) - 1);
	ASSERT_MEM_EQ(buf, data2, sizeof(data2) - 1);

	ASSERT_EQ(selftest_channel_close(&ctx, true, ch2, 0), 0);

	selftest_join(&ctx.server_thread, &ctx.server_thread_active);
	ASSERT_EQ(sarg.result, 0);
	ASSERT_EQ(sarg.channels_handled, 2);

	selftest_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Test: graceful disconnect -- client disconnects, server detects
 * ================================================================ */

static int
test_self_graceful_disconnect(void)
{
	struct selftest_ctx ctx;
	if (selftest_setup(&ctx) < 0)
		return TEST_FAIL;

	ASSERT_EQ(dssh_session_start(ctx.client), 0);
	ASSERT_EQ(dssh_session_start(ctx.server), 0);

	struct server_disconnect_arg sarg = { .ctx = &ctx };
	ASSERT_EQ(selftest_start_thread(&ctx, server_disconnect_thread, &sarg), 0);

	dssh_channel ch = open_exec(&ctx,"wait");
	ASSERT_NOT_NULL(ch);

	/* Disconnect gracefully */
	ASSERT_EQ(dssh_transport_disconnect(ctx.client,
	    SSH_DISCONNECT_BY_APPLICATION, "test done"), 0);

	selftest_join(&ctx.server_thread, &ctx.server_thread_active);

	ASSERT_TRUE(dssh_session_is_terminated(ctx.client));
	ASSERT_EQ(sarg.result, 0);

	selftest_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Test: EOF half-close -- client sends EOF, then reads the server reply
 * ================================================================ */

static int
test_self_eof_half_close(void)
{
	struct selftest_ctx ctx;
	if (selftest_setup(&ctx) < 0)
		return TEST_FAIL;
	ASSERT_EQ(dssh_session_start(ctx.client), 0);
	ASSERT_EQ(dssh_session_start(ctx.server), 0);
	struct server_halfclose_arg sarg = { .ctx = &ctx };
	ASSERT_EQ(selftest_start_thread(&ctx, server_halfclose_thread, &sarg), 0);
	dssh_channel ch = open_exec(&ctx, "halfclose");
	ASSERT_NOT_NULL(ch);
	const uint8_t data[] = "client-data";
	ASSERT_TRUE(selftest_write_all(ch, data, sizeof(data) - 1, 5000));
	ASSERT_EQ(dssh_chan_shutwr(ch), 0);
	/* The handle remains owned and readable after sending EOF. */
	const uint8_t expected[] = "server-reply-after-eof";
	uint8_t reply[sizeof(expected) - 1];
	ASSERT_TRUE(selftest_read_exact(ch, reply, sizeof(reply), 5000));
	ASSERT_MEM_EQ(reply, expected, sizeof(reply));
	ASSERT_TRUE(selftest_read_eof(ch, 5000));
	ASSERT_FALSE(dssh_session_is_terminated(ctx.client));
	ASSERT_EQ(selftest_channel_close(&ctx, true, ch, 0), 0);
	selftest_join(&ctx.server_thread, &ctx.server_thread_active);
	ASSERT_EQ(sarg.result, 0);
	selftest_cleanup(&ctx);
	return TEST_PASS;
}


/* ================================================================
 * Test: rekey during data -- send enough to trigger auto-rekey
 * ================================================================ */

static int
test_self_rekey_during_data(void)
{
	struct selftest_ctx ctx;
	if (selftest_setup(&ctx) < 0)
		return TEST_FAIL;

	ASSERT_EQ(dssh_session_start(ctx.client), 0);
	ASSERT_EQ(dssh_session_start(ctx.server), 0);

	struct server_echo_arg sarg = { .ctx = &ctx };
	ASSERT_EQ(selftest_start_thread(&ctx, server_echo_thread, &sarg), 0);

	dssh_channel ch = open_shell(&ctx, "xterm", 80, 24);
	ASSERT_NOT_NULL(ch);

	/* Arm the exact threshold at the next client receive, independent of
	 * compression ratio and encrypted packet overhead. */
	unsigned rekeys_before[2];
	ASSERT_TRUE(selftest_rekey_snapshot(ctx.client, ctx.server, rekeys_before));
	unsigned resets_before = compression_fixture_reset_count();
	uint64_t rx_count = ctx.client->trans.enc_s2c_selected->bytes_per_key;
	dssh_test_arm_rx_bytes_before_count(ctx.client, rx_count);

	uint8_t data[256];
	memset(data, 'R', sizeof(data));
	ASSERT_TRUE(selftest_write_all(ch, data, sizeof(data), 5000));

	/* Wait for echo -- the client demux must receive a response,
	 * triggering the auto-rekey check. */
	uint8_t buf[256];
	size_t recvd = 0;
	for (int i = 0; i < 100; i++) {
		int ev = dssh_chan_poll(ch,
		    DSSH_POLL_READ, 1000);
		ASSERT_TRUE(ev == 0 || ev == DSSH_POLL_READ);
		if (ev == 0)
			continue;
		int64_t n = dssh_chan_read(ch, 0,
		    buf + recvd, sizeof(buf) - recvd);
		if (n <= 0)
			break;
		size_t count;
		ASSERT_TRUE(selftest_io_count(n, sizeof(buf) - recvd, &count));
		recvd += count;
		if (recvd >= sizeof(data))
			break;
	}
	ASSERT_EQ_U(recvd, sizeof(data));
	ASSERT_MEM_EQ(buf, data, sizeof(data));

	/* Verify the connection survived rekey */
	ASSERT_TRUE(selftest_rekey_wait(ctx.client, ctx.server, rekeys_before));
	ASSERT_TRUE(compression_fixture_wait_reset(resets_before));
	ASSERT_FALSE(dssh_session_is_terminated(ctx.client));

	ASSERT_EQ(selftest_channel_close(&ctx, true, ch, 0), 0);
	selftest_join(&ctx.server_thread, &ctx.server_thread_active);
	ASSERT_EQ(sarg.result, 0);

	selftest_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Test: explicit receive threshold triggers asynchronous rekey
 * ================================================================ */

static int
test_self_rekey_manual(void)
{
	struct selftest_ctx ctx;
	if (selftest_setup(&ctx) < 0)
		return TEST_FAIL;

	ASSERT_EQ(dssh_session_start(ctx.client), 0);
	ASSERT_EQ(dssh_session_start(ctx.server), 0);

	struct server_echo_arg sarg = { .ctx = &ctx };
	ASSERT_EQ(selftest_start_thread(&ctx, server_echo_thread, &sarg), 0);

	dssh_channel ch = open_exec(&ctx,"rekey");
	ASSERT_NOT_NULL(ch);

	/* Count at the exact threshold: even a compressed small packet must
	 * trigger the next client demux rekey. */
	unsigned rekeys_before[2];
	ASSERT_TRUE(selftest_rekey_snapshot(ctx.client, ctx.server, rekeys_before));
	unsigned resets_before = compression_fixture_reset_count();
	uint64_t rx_count = ctx.client->trans.enc_s2c_selected->bytes_per_key;
	dssh_test_arm_rx_bytes_before_count(ctx.client, rx_count);

	const uint8_t data[] = "after-rekey";
	ASSERT_TRUE(selftest_write_all(ch, data, sizeof(data) - 1, 5000));

	/* Wait for echo */
	uint8_t buf[256];
	size_t recvd = 0;
	for (int i = 0; i < 100; i++) {
		int ev = dssh_chan_poll(ch,
		    DSSH_POLL_READ, 1000);
		ASSERT_TRUE(ev == 0 || ev == DSSH_POLL_READ);
		if (ev == 0)
			continue;
		int64_t n = dssh_chan_read(ch, 0,
		    buf + recvd, sizeof(buf) - recvd);
		if (n <= 0)
			break;
		size_t count;
		ASSERT_TRUE(selftest_io_count(n, sizeof(buf) - recvd, &count));
		recvd += count;
		if (recvd >= sizeof(data) - 1)
			break;
	}
	ASSERT_EQ_U(recvd, sizeof(data) - 1);

	ASSERT_MEM_EQ(buf, data, sizeof(data) - 1);
	/* The echo, both completed exchanges and compression resets must finish before
	 * closing the channel can hide an unfinished key transition. */
	ASSERT_TRUE(selftest_rekey_wait(ctx.client, ctx.server, rekeys_before));
	ASSERT_TRUE(compression_fixture_wait_reset(resets_before));
	ASSERT_FALSE(dssh_session_is_terminated(ctx.client));

	ASSERT_EQ(selftest_channel_close(&ctx, true, ch, 0), 0);
	selftest_join(&ctx.server_thread, &ctx.server_thread_active);
	ASSERT_EQ(sarg.result, 0);

	selftest_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Test: rekey preserves channels -- data survives rekey
 * ================================================================ */

static int
test_self_rekey_preserves_channels(void)
{
	struct selftest_ctx ctx;
	if (selftest_setup(&ctx) < 0)
		return TEST_FAIL;

	ASSERT_EQ(dssh_session_start(ctx.client), 0);
	ASSERT_EQ(dssh_session_start(ctx.server), 0);

	struct server_echo_arg sarg = { .ctx = &ctx };
	ASSERT_EQ(selftest_start_thread(&ctx, server_echo_thread, &sarg), 0);

	dssh_channel ch = open_shell(&ctx, "xterm", 80, 24);
	ASSERT_NOT_NULL(ch);

	/* Send data before rekey */
	const uint8_t before[] = "before-rekey-data";
	ASSERT_TRUE(selftest_write_all(ch, before, sizeof(before) - 1, 5000));

	/* Read echo */
	uint8_t buf[256];
	size_t recvd = 0;
	for (int i = 0; i < 50; i++) {
		int ev = dssh_chan_poll(ch,
		    DSSH_POLL_READ, 1000);
		ASSERT_TRUE(ev == 0 || ev == DSSH_POLL_READ);
		if (ev == 0)
			break;
		int64_t n = dssh_chan_read(ch, 0,
		    buf + recvd, sizeof(buf) - recvd);
		if (n <= 0)
			break;
		size_t count;
		ASSERT_TRUE(selftest_io_count(n, sizeof(buf) - recvd, &count));
		recvd += count;
		if (recvd >= sizeof(before) - 1)
			break;
	}
	ASSERT_EQ_U(recvd, sizeof(before) - 1);
	ASSERT_MEM_EQ(buf, before, sizeof(before) - 1);

	/* Save session ID */
	size_t sid_sz = 0;
	uint8_t sid_before[64];
	ASSERT_TRUE(selftest_snapshot_id(ctx.client->trans.session_id,
	    ctx.client->trans.session_id_sz, sid_before, sizeof(sid_before), &sid_sz));

	/* Force rekey */
	unsigned rekeys_before[2];
	ASSERT_TRUE(selftest_rekey_snapshot(ctx.client, ctx.server, rekeys_before));
	unsigned resets_before = compression_fixture_reset_count();
	uint64_t rx_count = ctx.client->trans.enc_s2c_selected->bytes_per_key;
	dssh_test_arm_rx_bytes_before_count(ctx.client, rx_count);

	/* Send data to trigger rekey */
	const uint8_t after[] = "after-rekey-data!";
	ASSERT_TRUE(selftest_write_all(ch, after, sizeof(after) - 1, 5000));

	/* Read echo of post-rekey data.  The rekey runs asynchronously
	 * in the demux thread, so allow extra time. */
	recvd = 0;
	for (int i = 0; i < 100; i++) {
		int ev = dssh_chan_poll(ch,
		    DSSH_POLL_READ, 1000);
		ASSERT_TRUE(ev == 0 || ev == DSSH_POLL_READ);
		if (ev == 0)
			continue;
		int64_t n = dssh_chan_read(ch, 0,
		    buf + recvd, sizeof(buf) - recvd);
		if (n <= 0)
			break;
		size_t count;
		ASSERT_TRUE(selftest_io_count(n, sizeof(buf) - recvd, &count));
		recvd += count;
		if (recvd >= sizeof(after) - 1)
			break;
	}
	ASSERT_EQ_U(recvd, sizeof(after) - 1);
	ASSERT_MEM_EQ(buf, after, sizeof(after) - 1);

	ASSERT_TRUE(selftest_rekey_wait(ctx.client, ctx.server, rekeys_before));
	ASSERT_TRUE(compression_fixture_wait_reset(resets_before));

	/* Session ID must be unchanged after rekey */
	ASSERT_EQ_U(ctx.client->trans.session_id_sz, sid_sz);
	ASSERT_MEM_EQ(ctx.client->trans.session_id, sid_before, sid_sz);

	ASSERT_EQ(selftest_channel_close(&ctx, true, ch, 0), 0);
	selftest_join(&ctx.server_thread, &ctx.server_thread_active);
	ASSERT_EQ(sarg.result, 0);

	selftest_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Test: rekey in-flight data -- data sent by peer before seeing
 * our KEXINIT must not be dropped (close item 69)
 * ================================================================ */

/* Server: accept channel, send burst of data, close */
struct server_burst_arg {
	struct selftest_ctx *ctx;
	atomic_bool start;
	int result;
};

static int
server_burst_thread(void *arg)
{
	struct server_burst_arg *a = arg;
	struct selftest_ctx *ctx = a->ctx;
	a->result = -1;

	dssh_channel ch = selftest_accept(ctx,
	    &selftest_accept_cbs, 30000);

	if (ch == NULL)
		return 0;

	while (!atomic_load_explicit(&a->start, memory_order_acquire)) {
		if (dssh_session_is_terminated(ctx->server))
			return 0;
		thrd_yield();
	}

	/* Send all ten spans, including any short writes. */
	uint8_t chunk[50];
	memset(chunk, 'D', sizeof(chunk));
	bool complete = true;
	for (int i = 0; i < 10; i++) {
		if (!selftest_write_all(ch, chunk, sizeof(chunk), 30000)) {
			complete = false;
			break;
		}
	}
	int ret = selftest_channel_close(ctx, false, ch, 0);
	if (complete && ret == 0)
		a->result = 0;
	return 0;
}

static int
test_self_rekey_inflight_data(void)
{
	struct selftest_ctx ctx;
	if (selftest_setup(&ctx) < 0)
		return TEST_FAIL;

	ASSERT_EQ(dssh_session_start(ctx.client), 0);
	ASSERT_EQ(dssh_session_start(ctx.server), 0);

	struct server_burst_arg sarg = { .ctx = &ctx };
	atomic_init(&sarg.start, false);
	ASSERT_EQ(selftest_start_thread(&ctx, server_burst_thread, &sarg), 0);

	dssh_channel ch = open_exec(&ctx,"burst");
	ASSERT_NOT_NULL(ch);

	/* Arm the threshold so the FIRST received packet triggers
	 * rekey_pending regardless of compression. The client dispatches that
	 * packet, then on the next recv_packet call starts rekey.
	 * Remaining packets in the socket buffer arrive during the
	 * kexinit wait loop. */
	unsigned rekeys_before[2];
	ASSERT_TRUE(selftest_rekey_snapshot(ctx.client, ctx.server, rekeys_before));
	unsigned resets_before = compression_fixture_reset_count();
	uint64_t rx_count = ctx.client->trans.enc_s2c_selected->bytes_per_key;
	dssh_test_arm_rx_bytes_before_count(ctx.client, rx_count);
	atomic_store_explicit(&sarg.start, true, memory_order_release);

	/* Collect all data -- expect 500 bytes total */
	uint8_t buf[1024];
	size_t recvd = 0;
	const size_t expected = 10 * 50;
	for (int i = 0; i < 200; i++) {
		int ev = dssh_chan_poll(ch,
		    DSSH_POLL_READ, 1000);
		ASSERT_TRUE(ev == 0 || ev == DSSH_POLL_READ);
		if (ev == 0)
			continue;
		int64_t n = dssh_chan_read(ch, 0,
		    buf + recvd, sizeof(buf) - recvd);
		if (n <= 0)
			break;
		size_t count;
		ASSERT_TRUE(selftest_io_count(n, sizeof(buf) - recvd, &count));
		recvd += count;
		if (recvd >= expected)
			break;
	}
	ASSERT_EQ_U(recvd, expected);

	/* Verify data integrity */
	for (size_t i = 0; i < recvd; i++)
		ASSERT_EQ(buf[i], 'D');

	ASSERT_TRUE(selftest_rekey_wait(ctx.client, ctx.server, rekeys_before));
	ASSERT_TRUE(compression_fixture_wait_reset(resets_before));
	ASSERT_FALSE(dssh_session_is_terminated(ctx.client));

	ASSERT_EQ(selftest_channel_close(&ctx, true, ch, 0), 0);
	selftest_join(&ctx.server_thread, &ctx.server_thread_active);
	ASSERT_EQ(sarg.result, 0);

	selftest_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Test: unexpected connection drop -- socket dies mid-session
 * ================================================================ */

static int
test_self_connection_drop(void)
{
	struct selftest_ctx ctx;
	if (selftest_setup(&ctx) < 0)
		return TEST_FAIL;

	ASSERT_EQ(dssh_session_start(ctx.client), 0);
	ASSERT_EQ(dssh_session_start(ctx.server), 0);

	struct server_echo_arg sarg = { .ctx = &ctx };
	ASSERT_EQ(selftest_start_thread(&ctx, server_echo_thread, &sarg), 0);

	dssh_channel ch = open_exec(&ctx,"drop");
	ASSERT_NOT_NULL(ch);

	/* Write some data to confirm the channel works */
	const uint8_t data[] = "before drop";
	ASSERT_TRUE(selftest_write_all(ch, data, sizeof(data) - 1, 5000));

	/* Wait for echo to confirm data arrived */
	uint8_t buf[64];
	size_t recvd = 0;
	for (int i = 0; i < 50; i++) {
		int ev = dssh_chan_poll(ch,
		    DSSH_POLL_READ, 1000);
		ASSERT_TRUE(ev == 0 || ev == DSSH_POLL_READ);
		if (ev == 0)
			continue;
		int64_t n = dssh_chan_read(ch, 0,
		    buf + recvd, sizeof(buf) - recvd);
		if (n <= 0)
			break;
		size_t count;
		ASSERT_TRUE(selftest_io_count(n, sizeof(buf) - recvd, &count));
		recvd += count;
		if (recvd >= sizeof(data) - 1)
			break;
	}
	ASSERT_EQ_U(recvd, sizeof(data) - 1);
	ASSERT_MEM_EQ(buf, data, sizeof(data) - 1);

	/* Yank the socket out from under both sessions.
	 * This simulates a VPN drop, cable pull, or peer crash.
	 * The application detects the dead socket in its I/O
	 * callbacks (recv returns 0 / send returns EPIPE). */
	selftest_shutdown(ctx.client_fd);
	selftest_shutdown(ctx.server_fd);

	/* Wait for both demux threads to observe the transport failure. */
	ASSERT_TRUE(dssh_test_wait_bool(&ctx.client->terminate, true));
	ASSERT_TRUE(dssh_test_wait_bool(&ctx.server->terminate, true));

	/* Both sessions should be terminated */
	ASSERT_TRUE(dssh_session_is_terminated(ctx.client));
	ASSERT_TRUE(dssh_session_is_terminated(ctx.server));

	/* Poll and write must not hang */
	int ev = dssh_chan_poll(ch,
	    DSSH_POLL_READ, 1000);
	ASSERT_EQ(ev, DSSH_POLL_READ);
	uint8_t extra;
	ASSERT_EQ(dssh_chan_read(ch, 0, &extra, sizeof(extra)), 0);

	const uint8_t byte[] = "x";
	int64_t w = dssh_chan_write(ch, 0, byte, 1);
	ASSERT_ERR(w, DSSH_ERROR_TERMINATED);

	/* Server echo thread should have exited */
	selftest_join(&ctx.server_thread, &ctx.server_thread_active);

	/* Cleanup must succeed without hanging */
	selftest_cleanup(&ctx);
	return TEST_PASS;
}

static int
test_self_socket_boundaries(void)
{
	struct selftest_ctx ctx;
	ASSERT_EQ(selftest_standalone(&ctx), 0);
	int fd = 0;
	uint8_t byte = 0;
	size_t received = 42;
	ASSERT_EQ(socket_tx(NULL, 1, ctx.client, &fd), -1);
	ASSERT_EQ(socket_rx(NULL, 1, ctx.client, &fd), -1);
	ASSERT_EQ(socket_tx(&byte, 1, NULL, &fd), -1);
	ASSERT_EQ(socket_rx(&byte, 1, ctx.client, NULL), -1);
	ASSERT_EQ(socket_tx(NULL, 0, ctx.client, &fd), 0);
	ASSERT_EQ(socket_rx(NULL, 0, ctx.client, &fd), 0);
	ASSERT_EQ(socket_rxline(&byte, 1, NULL, ctx.client, &fd), -1);
	ASSERT_EQ(socket_rxline(NULL, 1, &received, ctx.client, &fd), -1);
	ASSERT_EQ_U(received, 0);
	ASSERT_EQ(socket_rxline(&byte, 0, &received, ctx.client, &fd), -1);
	fd = -1;
	ASSERT_EQ(socket_tx(&byte, 1, ctx.client, &fd), -1);
	ASSERT_EQ(socket_rx(&byte, 1, ctx.client, &fd), -1);
	ASSERT_EQ(socket_wait(-1, POLLIN, ctx.client), -1);
	ASSERT_EQ(socket_wait(0, 0, ctx.client), -1);
	size_t count = 42;
	ASSERT_FALSE(socket_count(-1, 8, &count));
	ASSERT_EQ_U(count, 42);
	ASSERT_FALSE(socket_count(0, 8, &count));
	ASSERT_EQ_U(count, 42);
	ASSERT_FALSE(socket_count(9, 8, &count));
	ASSERT_EQ_U(count, 42);
	ASSERT_TRUE(socket_count(8, 8, &count));
	ASSERT_EQ_U(count, 8);
	ASSERT_EQ_U(socket_request(0), 0);
	ASSERT_EQ_U(socket_request(8), 8);
	uintmax_t native_max = (uintmax_t)SSIZE_MAX;
	uintmax_t expected = native_max < SIZE_MAX ? native_max : SIZE_MAX;
	ASSERT_EQ_U(socket_request(SIZE_MAX), expected);
	selftest_cleanup(&ctx);
	return TEST_PASS;
}

/*
 * Verify all public API functions in ssh.c handle NULL session gracefully.
 * Covers 7 NULL-guard branches + dssh_cleanse(NULL).
 */
static int
test_null_session_api(void)
{
	/* dssh_session_terminate(NULL) returns false */
	ASSERT_FALSE(dssh_session_terminate(NULL));

	/* dssh_session_is_terminated(NULL) returns true */
	ASSERT_TRUE(dssh_session_is_terminated(NULL));

	/* dssh_session_cleanup(NULL) is a no-op (must not crash) */
	dssh_session_cleanup(NULL);

	/* Invalid session setters must report their documented error. */
	ASSERT_ERR(dssh_session_set_cbdata(NULL, NULL, NULL, NULL, NULL), DSSH_ERROR_INVALID);
	ASSERT_ERR(dssh_session_set_debug_cb(NULL, NULL, NULL), DSSH_ERROR_INVALID);
	ASSERT_ERR(dssh_session_set_log_cb(NULL, NULL, NULL), DSSH_ERROR_INVALID);
	ASSERT_ERR(dssh_session_set_log_level(NULL, DSSH_LOG_DEBUG), DSSH_ERROR_INVALID);
	ASSERT_ERR(dssh_session_set_unimplemented_cb(NULL, NULL, NULL), DSSH_ERROR_INVALID);
	ASSERT_ERR(dssh_session_set_banner_cb(NULL, NULL, NULL), DSSH_ERROR_INVALID);
	ASSERT_ERR(dssh_session_set_global_request_cb(NULL, NULL, NULL), DSSH_ERROR_INVALID);
	ASSERT_ERR(dssh_session_set_terminate_cb(NULL, NULL, NULL), DSSH_ERROR_INVALID);

	/* dssh_cleanse(NULL, ...) is a no-op (must not crash) */
	dssh_cleanse(NULL, 10);

	return TEST_PASS;
}

struct log_test_state {
	int             calls;
	bool            valid;
	dssh_log_action action;
	dssh_log_level  level;
	int             error_code;
	bool            always_display;
	char            message[DSSH_LOG_MESSAGE_MAX + 1];
};

static dssh_log_action
log_test_cb(const struct dssh_log_record *record, void *cbdata)
{
	struct log_test_state *state = cbdata;
	if (record == NULL || state == NULL)
		abort();
	if (record->message_len > DSSH_LOG_MESSAGE_MAX
	    || (record->message_len != 0 && record->message == NULL)) {
		state->valid = false;
		return DSSH_LOG_LOCAL_ONLY;
	}

	if (state->calls == INT_MAX)
		abort();
	state->calls++;
	state->level          = record->level;
	state->error_code     = record->error_code;
	state->always_display = record->always_display;
	size_t len = record->message_len;

	if (len > 0)
		memcpy(state->message, record->message, len);
	state->message[len] = 0;
	return state->action;
}

static int log_format_side_effects;

static int
log_format_side_effect(void)
{
	if (log_format_side_effects == INT_MAX)
		abort();
	log_format_side_effects++;
	return 42;
}

static int
test_log_api(void)
{
	ASSERT_STR_EQ(dssh_strerror(DSSH_ERROR_ALLOC), "Memory allocation failure");
	ASSERT_STR_EQ(dssh_strerror(12345), "Unknown DeuceSSH error");
	struct selftest_ctx ctx;
	ASSERT_EQ(selftest_standalone(&ctx), 0);
	dssh_session sess = ctx.client;
	ASSERT_TRUE(sess->log_forward == NULL);

	log_format_side_effects = 0;
	DSSH_LOGF(sess, DSSH_LOG_DEBUG, DSSH_ERROR_NONE, "suppressed %d", log_format_side_effect());
	ASSERT_EQ(log_format_side_effects, 0);

	struct log_test_state state = { .valid = true, .action = DSSH_LOG_LOCAL_ONLY };
	ASSERT_EQ(dssh_session_set_log_cb(sess, log_test_cb, &state), 0);
	ASSERT_NOT_NULL(sess->log_forward);
	DSSH_LOGF(sess, DSSH_LOG_DEBUG, DSSH_ERROR_NONE, "suppressed %d", log_format_side_effect());
	ASSERT_EQ(log_format_side_effects, 0);
	ASSERT_EQ(state.calls, 0);

	DSSH_LOGF(sess, DSSH_LOG_ERROR, DSSH_ERROR_INIT, "operation failed");
	ASSERT_EQ(state.calls, 1);
	ASSERT_EQ(state.level, DSSH_LOG_ERROR);
	ASSERT_EQ(state.error_code, DSSH_ERROR_INIT);
	ASSERT_STR_EQ(state.message, "operation failed");

	ASSERT_EQ(dssh_session_set_log_level(sess, DSSH_LOG_DEBUG), 0);
	DSSH_LOGF(sess, DSSH_LOG_DEBUG, DSSH_ERROR_NONE, "enabled %d", log_format_side_effect());
	ASSERT_EQ(log_format_side_effects, 1);
	ASSERT_EQ(state.calls, 2);
	ASSERT_STR_EQ(state.message, "enabled 42");
	dssh_log_level bad_level = (dssh_log_level)99;
	ASSERT_EQ(dssh_session_set_log_level(sess, bad_level), DSSH_ERROR_INVALID);

	state.action = DSSH_LOG_SEND_DEBUG_ALWAYS_DISPLAY;
	DSSH_LOGF(sess, DSSH_LOG_WARNING, DSSH_ERROR_TIMEOUT, "forward me");
	ASSERT_EQ(sess->log_forward->count, (size_t)1);
	ASSERT_TRUE(sess->log_forward->entries[0].always_display);

	ASSERT_TRUE(state.valid);
	ASSERT_EQ(dssh_session_set_log_cb(sess, NULL, NULL), 0);
	ASSERT_TRUE(sess->log_forward == NULL);
	selftest_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Timeout tests (items 65 + 66)
 * ================================================================ */

/*
 * Verify dssh_session_set_timeout(NULL, ...) does not crash,
 * and that the default is 75000ms.
 */
static int
test_set_timeout(void)
{
	/* NULL session: no crash */
	ASSERT_ERR(dssh_session_set_timeout(NULL, 1000), DSSH_ERROR_INVALID);

	/* Verify default */
	struct selftest_ctx ctx;
	ASSERT_EQ(selftest_standalone(&ctx), 0);
	dssh_session sess = ctx.client;
	ASSERT_EQ(sess->timeout_ms, 75000);
	struct timespec now = { .tv_sec = 100, .tv_nsec = 123 };
	struct timespec deadline = { .tv_sec = 42, .tv_nsec = 1 };
	dssh_test_clock_set(now);
	int ret = dssh_deadline_from_ms(&deadline, sess->timeout_ms);
	dssh_test_clock_reset();
	ASSERT_EQ(ret, 0);
	ASSERT_TRUE(deadline.tv_sec == 175);
	ASSERT_EQ(deadline.tv_nsec, 123);

	/* Set and verify */
	ASSERT_EQ(dssh_session_set_timeout(sess, 200), 0);
	ASSERT_EQ(sess->timeout_ms, 200);
	ASSERT_EQ(dssh_session_set_timeout(sess, INT_MAX), 0);
	ASSERT_EQ(sess->timeout_ms, INT_MAX);
	ASSERT_EQ(dssh_session_set_timeout(sess, INT_MIN), 0);
	ASSERT_EQ(sess->timeout_ms, INT_MIN);

	/* Disable timeout */
	ASSERT_EQ(dssh_session_set_timeout(sess, 0), 0);
	ASSERT_EQ(sess->timeout_ms, 0);

	selftest_cleanup(&ctx);
	return TEST_PASS;
}

/*
 * Verify dssh_session_set_terminate_cb() stores the callback and
 * that session_set_terminate() invokes it exactly once.
 */
static void
terminate_test_cb(dssh_session sess, void *cbdata)
{
	(void)sess;
	int *count = cbdata;
	if (*count == INT_MAX)
		abort();
	(*count)++;
}

static int
test_set_terminate_cb(void)
{
	struct selftest_ctx ctx;
	ASSERT_EQ(selftest_standalone(&ctx), 0);
	dssh_session sess = ctx.client;

	int count = 0;
	ASSERT_EQ(dssh_session_set_terminate_cb(sess, terminate_test_cb, &count), 0);

	/* Terminate fires the callback */
	ASSERT_TRUE(dssh_session_terminate(sess));
	ASSERT_EQ(count, 1);

	/* Second terminate is a no-op (single-fire) */
	ASSERT_FALSE(dssh_session_terminate(sess));
	ASSERT_EQ(count, 1);

	selftest_cleanup(&ctx);
	return TEST_PASS;
}

/*
 * open_session_channel times out when the server never responds
 * with CHANNEL_OPEN_CONFIRMATION.  The server's demux thread runs
 * but dssh_session_accept() is never called, so the queued
 * CHANNEL_OPEN is never answered.
 */
static bool
selftest_elapsed(const struct timespec *start)
{
	struct timespec end;
	double elapsed;
	return dssh_test_runner_clock(&end)
	    && dssh_test_runner_elapsed(difftime(end.tv_sec, start->tv_sec),
	        start->tv_nsec, end.tv_nsec, &elapsed)
	    && elapsed >= 100.0 && elapsed < 2000.0;
}

static bool
selftest_wait_expired(struct dssh_test_wait_observation result)
{
	return result.native && result.timed_out && !result.failed;
}

static int
test_open_channel_timeout(void)
{
	struct selftest_ctx ctx;
	if (selftest_setup(&ctx) < 0)
		return TEST_FAIL;

	/* Short timeout so the test completes quickly */
	ASSERT_EQ(dssh_session_set_timeout(ctx.client, 200), 0);

	ASSERT_EQ(dssh_session_start(ctx.client), 0);
	ASSERT_EQ(dssh_session_start(ctx.server), 0);

	struct timespec t0;
	ASSERT_TRUE(dssh_test_runner_clock(&t0));

	/* Server never calls accept, so no CONFIRMATION is sent */
	dssh_test_wait_observe_begin(NULL, NULL);
	dssh_test_channel_observe_begin(DSSH_TEST_CHANNEL_OPEN_WAIT);
	dssh_channel ch = open_exec(&ctx,"cmd");
	struct dssh_test_channel_observation channel = dssh_test_channel_observe_end();
	struct dssh_test_wait_observation wait = dssh_test_wait_observe_end();
	ASSERT_TRUE(dssh_test_channel_expired(wait, channel));
	ASSERT_NULL(ch);


	/* Should complete in ~200ms, allow generous slack but under 2s */
	ASSERT_TRUE(selftest_elapsed(&t0));

	selftest_cleanup(&ctx);
	return TEST_PASS;
}

/*
 * setup_recv times out when the client opens a channel but never
 * sends any CHANNEL_REQUEST (pty-req, shell, exec, etc.).
 * The server's accept_channel -> setup_recv path should time out.
 */
struct setup_timeout_server_arg {
	struct selftest_ctx *ctx;
	dssh_channel ch;
	int result;
};

static int
setup_timeout_server_thread(void *arg)
{
	struct setup_timeout_server_arg *a = arg;
	struct selftest_ctx *ctx = a->ctx;
	a->ch = NULL;

	dssh_test_wait_observe_begin(NULL, &ctx->server->accept_cnd);
	dssh_test_channel_observe_begin(DSSH_TEST_CHANNEL_ACCEPT_WAIT);
	a->ch = selftest_accept(ctx,
	    &selftest_accept_cbs, 30000);
	struct dssh_test_channel_observation channel = dssh_test_channel_observe_end();
	struct dssh_test_wait_observation wait = dssh_test_wait_observe_end();
	a->result = a->ch == NULL && dssh_test_channel_expired(wait, channel) ? 0 : -1;
	return 0;
}

static int
test_setup_recv_timeout(void)
{
	struct selftest_ctx ctx;
	if (selftest_setup(&ctx) < 0)
		return TEST_FAIL;

	/* Short timeout on the server (where setup_recv runs) */
	ASSERT_EQ(dssh_session_set_timeout(ctx.server, 200), 0);

	ASSERT_EQ(dssh_session_start(ctx.server), 0);

	/* Start server accept in a thread */
	struct setup_timeout_server_arg sarg = { .ctx = &ctx };
	ASSERT_EQ(selftest_start_thread(&ctx, setup_timeout_server_thread, &sarg), 0);

	/*
	 * Send a raw CHANNEL_OPEN from the client but never follow up
	 * with any CHANNEL_REQUEST.  The server accepts the channel open
	 * (sends CONFIRMATION) then blocks in setup_recv waiting for
	 * pty-req/shell/exec/subsystem -- which never arrive.
	 */
	ASSERT_EQ(dssh_session_set_timeout(ctx.client, 5000), 0);

	struct timespec t0;
	ASSERT_TRUE(dssh_test_runner_clock(&t0));

	uint8_t msg[64];
	struct fixture_writer writer = { .data = msg, .capacity = sizeof(msg), .valid = true };
	const uint8_t type_open[] = { SSH_MSG_CHANNEL_OPEN };
	const uint8_t channel_type[] = "session";
	ASSERT_TRUE(fixture_writer_bytes(&writer, type_open, sizeof(type_open)));
	ASSERT_TRUE(fixture_writer_string(&writer, channel_type, sizeof(channel_type) - 1));
	ASSERT_TRUE(fixture_writer_u32(&writer, 0));
	ASSERT_TRUE(fixture_writer_u32(&writer, 32768));
	ASSERT_TRUE(fixture_writer_u32(&writer, 32768));
	int res = send_packet(ctx.client, msg, writer.used, NULL);
	ASSERT_EQ(res, 0);
	/* Receive the real confirmation directly: the raw sender intentionally has
	 * no connection-layer channel or demux consuming its recipient ID. */
	uint8_t type;
	uint8_t *payload;
	size_t payload_length;
	ASSERT_EQ(recv_packet(ctx.client, &type, &payload, &payload_length), 0);
	ASSERT_EQ(type, SSH_MSG_CHANNEL_OPEN_CONFIRMATION);
	ASSERT_NOT_NULL(payload);
	ASSERT_EQ_U(payload_length, 17);
	ASSERT_EQ(payload[0], SSH_MSG_CHANNEL_OPEN_CONFIRMATION);
	const uint8_t expected_prefix[] = { SSH_MSG_CHANNEL_OPEN_CONFIRMATION, 0, 0, 0, 0 };
	ASSERT_MEM_EQ(payload, expected_prefix, sizeof(expected_prefix));


	/* Wait for server thread to complete (it should time out) */
	selftest_join(&ctx.server_thread, &ctx.server_thread_active);


	/* Server's setup_recv should have timed out in ~200ms */
	ASSERT_TRUE(selftest_elapsed(&t0));
	ASSERT_EQ(sarg.result, 0); /* ch == NULL, result set to 0 */
	ASSERT_NULL(sarg.ch);

	selftest_cleanup(&ctx);
	return TEST_PASS;
}

/*
 * send_packet blocks during rekey; with a session timeout set,
 * the rekey wait should time out and terminate the session.
 * We simulate this by setting rekey_in_progress = true without
 * actually performing a rekey, then sending an application message.
 */
static int
test_rekey_send_timeout(void)
{
	struct selftest_ctx ctx;
	if (selftest_setup(&ctx) < 0)
		return TEST_FAIL;

	/* Short timeout */
	ASSERT_EQ(dssh_session_set_timeout(ctx.client, 200), 0);

	/* Pretend a rekey is in progress (no actual KEX running) */
	ctx.client->trans.rekey_in_progress = true;

	struct timespec t0;
	ASSERT_TRUE(dssh_test_runner_clock(&t0));

	/* Try to send an application-layer message (type >= 50).
	 * This should block on the rekey wait, time out, terminate,
	 * and return DSSH_ERROR_TERMINATED. */
	uint8_t msg[8];
	msg[0] = SSH_MSG_CHANNEL_DATA; /* type 94, above threshold */
	msg[1] = 0;
	msg[2] = 0;
	msg[3] = 0;
	msg[4] = 0;

	dssh_test_wait_observe_begin(&ctx.client->trans.rekey_cnd, NULL);
	int res = send_packet(ctx.client, msg, 5, NULL);
	struct dssh_test_wait_observation wait = dssh_test_wait_observe_end();
	ASSERT_TRUE(selftest_wait_expired(wait));
	ASSERT_EQ(res, DSSH_ERROR_TERMINATED);


	ASSERT_TRUE(selftest_elapsed(&t0));
	ASSERT_TRUE(dssh_session_is_terminated(ctx.client));

	selftest_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Channel API integration tests
 * ================================================================ */

/*
 * Test: exec echo using the channel API on both sides.
 */
static int
test_self_chan_exec_echo(void)
{
	struct selftest_ctx ctx;

	if (selftest_setup(&ctx) < 0)
		return TEST_FAIL;

	ASSERT_EQ(dssh_session_start(ctx.client), 0);
	ASSERT_EQ(dssh_session_start(ctx.server), 0);

	struct server_echo_arg sarg = { .ctx = &ctx };
	ASSERT_EQ(selftest_start_thread(&ctx, server_echo_thread, &sarg), 0);

	/* Open exec channel using the params builder */
	struct dssh_chan_params *p = &ctx.params;

	ASSERT_EQ(selftest_params_init(&ctx, DSSH_CHAN_EXEC), 0);
	ASSERT_EQ(dssh_chan_params_set_command(p, "echo"), 0);
	ASSERT_EQ(dssh_chan_params_set_pty(p, false), 0);

	dssh_channel ch = selftest_open(&ctx, p);

	selftest_params_free(&ctx);
	ASSERT_NOT_NULL(ch);

	/* Verify getters */
	ASSERT_EQ(dssh_chan_get_type(ch), DSSH_CHAN_EXEC);
	ASSERT_STR_EQ(dssh_chan_get_command(ch), "echo");
	ASSERT_NULL(dssh_chan_get_subsystem(ch));
	ASSERT_FALSE(dssh_chan_has_pty(ch));

	/* Write channel data. */
	const uint8_t data[] = "Hello, dssh_chan!";
	ASSERT_TRUE(selftest_write_all(ch, data, sizeof(data) - 1, 5000));

	/* Read the complete echo. */
	uint8_t buf[256];
	size_t recvd = 0;

	for (int i = 0; i < 50; i++) {
		int ev = dssh_chan_poll(ch, DSSH_POLL_READ, 1000);
		ASSERT_TRUE(ev == 0 || ev == DSSH_POLL_READ);

		if (ev == 0)
			break;

		int64_t n = dssh_chan_read(ch, 0, buf + recvd,
		    sizeof(buf) - recvd);

		if (n <= 0)
			break;
		size_t count;
		ASSERT_TRUE(selftest_io_count(n, sizeof(buf) - recvd, &count));
		recvd += count;
		if (recvd >= sizeof(data) - 1)
			break;
	}
	ASSERT_EQ_U(recvd, sizeof(data) - 1);
	ASSERT_MEM_EQ(buf, data, sizeof(data) - 1);

	ASSERT_EQ(selftest_channel_close(&ctx, true, ch, -1), 0);
	selftest_join(&ctx.server_thread, &ctx.server_thread_active);
	ASSERT_EQ(sarg.result, 0);

	selftest_cleanup(&ctx);
	return TEST_PASS;
}

/*
 * Test: shell with pty using the channel API.
 */
static int
test_self_chan_shell_pty(void)
{
	struct selftest_ctx ctx;

	if (selftest_setup(&ctx) < 0)
		return TEST_FAIL;

	ASSERT_EQ(dssh_session_start(ctx.client), 0);
	ASSERT_EQ(dssh_session_start(ctx.server), 0);

	struct server_echo_arg sarg = { .ctx = &ctx };
	ASSERT_EQ(selftest_start_thread(&ctx, server_echo_thread, &sarg), 0);

	/* Open shell with pty using params builder */
	struct dssh_chan_params *p = &ctx.params;

	ASSERT_EQ(selftest_params_init(&ctx, DSSH_CHAN_SHELL), 0);
	ASSERT_EQ(dssh_chan_params_set_term(p, "xterm-256color"), 0);
	ASSERT_EQ(dssh_chan_params_set_size(p, 80, 24, 0, 0), 0);

	dssh_channel ch = selftest_open(&ctx, p);

	selftest_params_free(&ctx);
	ASSERT_NOT_NULL(ch);

	/* Verify getters */
	ASSERT_EQ(dssh_chan_get_type(ch), DSSH_CHAN_SHELL);
	ASSERT_TRUE(dssh_chan_has_pty(ch));
	ASSERT_NULL(dssh_chan_get_command(ch));

	/* Write and read echo */
	const uint8_t data[] = "pty-test";
	ASSERT_TRUE(selftest_write_all(ch, data, sizeof(data) - 1, 5000));

	uint8_t buf[256];
	size_t recvd = 0;

	for (int i = 0; i < 50; i++) {
		int ev = dssh_chan_poll(ch, DSSH_POLL_READ, 1000);
		ASSERT_TRUE(ev == 0 || ev == DSSH_POLL_READ);

		if (ev == 0)
			break;

		int64_t n = dssh_chan_read(ch, 0, buf + recvd,
		    sizeof(buf) - recvd);

		if (n <= 0)
			break;
		size_t count;
		ASSERT_TRUE(selftest_io_count(n, sizeof(buf) - recvd, &count));
		recvd += count;
		if (recvd >= sizeof(data) - 1)
			break;
	}
	ASSERT_EQ_U(recvd, sizeof(data) - 1);
	ASSERT_MEM_EQ(buf, data, sizeof(data) - 1);

	ASSERT_EQ(selftest_channel_close(&ctx, true, ch, 0), 0);
	selftest_join(&ctx.server_thread, &ctx.server_thread_active);
	ASSERT_EQ(sarg.result, 0);

	selftest_cleanup(&ctx);
	return TEST_PASS;
}

/*
 * Test: server accept and client open, with a complete roundtrip.
 */

/* Accept callbacks for channel API tests */
static int
test_accept_shell_cb(dssh_channel ch,
    const struct dssh_chan_params *p,
    struct dssh_chan_accept_result *result, void *cbdata)
{
	(void)ch; (void)p; (void)result; (void)cbdata;
	return 0; /* accept, stream mode, default window */
}

static int
test_accept_exec_cb(dssh_channel ch,
    const struct dssh_chan_params *p,
    struct dssh_chan_accept_result *result, void *cbdata)
{
	(void)ch; (void)result; (void)cbdata;
	/* Verify command is set */
	if (p->command == NULL)
		return -1;
	return 0;
}

struct server_chan_echo_arg {
	struct selftest_ctx *ctx;
	enum dssh_chan_type expected_type;
	int result;
};

static int
server_chan_echo_thread(void *arg)
{
	struct server_chan_echo_arg *a = arg;
	struct selftest_ctx *ctx = a->ctx;
	a->result = -1;
	struct dssh_chan_accept_cbs cbs = {
		.shell = test_accept_shell_cb,
		.exec = test_accept_exec_cb,
	};
	dssh_channel ch = selftest_accept(ctx, &cbs, 30000);
	if (ch == NULL)
		return 0;
	bool complete = dssh_chan_get_type(ch) == a->expected_type && selftest_echo(ch)
	    && !dssh_session_is_terminated(ctx->server);
	int ret = selftest_channel_close(ctx, false, ch, 0);
	if (complete && ret == 0)
		a->result = 0;
	return 0;
}


static int
test_self_chan_accept_exec(void)
{
	struct selftest_ctx ctx;

	if (selftest_setup(&ctx) < 0)
		return TEST_FAIL;

	ASSERT_EQ(dssh_session_start(ctx.client), 0);
	ASSERT_EQ(dssh_session_start(ctx.server), 0);

	struct server_chan_echo_arg sarg = {
		.ctx = &ctx,
		.expected_type = DSSH_CHAN_EXEC,
	};
	ASSERT_EQ(selftest_start_thread(&ctx, server_chan_echo_thread, &sarg), 0);

	/* Client: open exec */
	struct dssh_chan_params *p = &ctx.params;

	ASSERT_EQ(selftest_params_init(&ctx, DSSH_CHAN_EXEC), 0);
	ASSERT_EQ(dssh_chan_params_set_command(p, "test-cmd"), 0);
	ASSERT_EQ(dssh_chan_params_set_pty(p, false), 0);

	dssh_channel ch = selftest_open(&ctx, p);

	selftest_params_free(&ctx);
	ASSERT_NOT_NULL(ch);

	/* Write and read echo */
	const uint8_t data[] = "round-trip-test";
	ASSERT_TRUE(selftest_write_all(ch, data, sizeof(data) - 1, 5000));

	uint8_t buf[256];
	size_t recvd = 0;

	for (int i = 0; i < 50; i++) {
		int ev = dssh_chan_poll(ch, DSSH_POLL_READ, 1000);
		ASSERT_TRUE(ev == 0 || ev == DSSH_POLL_READ);

		if (ev == 0)
			break;

		int64_t n = dssh_chan_read(ch, 0, buf + recvd,
		    sizeof(buf) - recvd);

		if (n <= 0)
			break;
		size_t count;
		ASSERT_TRUE(selftest_io_count(n, sizeof(buf) - recvd, &count));
		recvd += count;
		if (recvd >= sizeof(data) - 1)
			break;
	}
	ASSERT_EQ_U(recvd, sizeof(data) - 1);
	ASSERT_MEM_EQ(buf, data, sizeof(data) - 1);

	ASSERT_EQ(selftest_channel_close(&ctx, true, ch, -1), 0);
	selftest_join(&ctx.server_thread, &ctx.server_thread_active);
	ASSERT_EQ(sarg.result, 0);

	selftest_cleanup(&ctx);
	return TEST_PASS;
}

/*
 * Test: shell with pty.
 * Verify server sees pty state via getters.
 */
struct server_chan_pty_arg {
	struct selftest_ctx *ctx;
	bool has_pty;
	int result;
};

static int
test_accept_pty_cb(dssh_channel ch,
    const struct dssh_chan_params *p, void *cbdata)
{
	bool *has_pty = cbdata;

	*has_pty = (p->flags & DSSH_PARAM_HAS_PTY) != 0;
	return 0; /* accept */
}

static int
server_chan_pty_thread(void *arg)
{
	struct server_chan_pty_arg *a = arg;
	struct selftest_ctx *ctx = a->ctx;
	a->result = -1;
	a->has_pty = false;
	struct dssh_chan_accept_cbs cbs = {
		.pty_req = test_accept_pty_cb,
		.shell = test_accept_shell_cb,
		.cbdata = &a->has_pty,
	};
	dssh_channel ch = selftest_accept(ctx, &cbs, 30000);
	if (ch == NULL)
		return 0;
	const uint8_t expected[] = "ping";
	uint8_t buf[sizeof(expected) - 1];
	bool complete = selftest_read_exact(ch, buf, sizeof(buf), 30000)
	    && memcmp(buf, expected, sizeof(buf)) == 0
	    && selftest_write_all(ch, buf, sizeof(buf), 30000);
	int ret = selftest_channel_close(ctx, false, ch, 0);
	if (complete && ret == 0)
		a->result = 0;
	return 0;
}


static int
test_self_chan_accept_shell_pty(void)
{
	struct selftest_ctx ctx;

	if (selftest_setup(&ctx) < 0)
		return TEST_FAIL;

	ASSERT_EQ(dssh_session_start(ctx.client), 0);
	ASSERT_EQ(dssh_session_start(ctx.server), 0);

	struct server_chan_pty_arg sarg = { .ctx = &ctx };
	ASSERT_EQ(selftest_start_thread(&ctx, server_chan_pty_thread, &sarg), 0);

	/* Client: open shell with pty */
	struct dssh_chan_params *p = &ctx.params;

	ASSERT_EQ(selftest_params_init(&ctx, DSSH_CHAN_SHELL), 0);
	ASSERT_EQ(dssh_chan_params_set_term(p, "vt100"), 0);
	ASSERT_EQ(dssh_chan_params_set_size(p, 132, 50, 0, 0), 0);

	dssh_channel ch = selftest_open(&ctx, p);

	selftest_params_free(&ctx);
	ASSERT_NOT_NULL(ch);

	const uint8_t data[] = "ping";

	ASSERT_TRUE(selftest_write_all(ch, data, sizeof(data) - 1, 5000));

	uint8_t buf[256];
	size_t recvd = 0;

	for (int i = 0; i < 50; i++) {
		int ev = dssh_chan_poll(ch, DSSH_POLL_READ, 1000);
		ASSERT_TRUE(ev == 0 || ev == DSSH_POLL_READ);

		if (ev == 0)
			break;

		int64_t n = dssh_chan_read(ch, 0, buf + recvd,
		    sizeof(buf) - recvd);

		if (n <= 0)
			break;
		size_t count;
		ASSERT_TRUE(selftest_io_count(n, sizeof(buf) - recvd, &count));
		recvd += count;
		if (recvd >= sizeof(data) - 1)
			break;
	}
	ASSERT_EQ_U(recvd, sizeof(data) - 1);
	ASSERT_MEM_EQ(buf, data, sizeof(data) - 1);

	ASSERT_EQ(selftest_channel_close(&ctx, true, ch, 0), 0);
	selftest_join(&ctx.server_thread, &ctx.server_thread_active);
	ASSERT_EQ(sarg.result, 0);
	ASSERT_TRUE(sarg.has_pty);

	selftest_cleanup(&ctx);
	return TEST_PASS;
}

/*
 * Test: exit-status event via the channel API.
 * Server sends data, then closes with exit_code=42.
 * Client reads data, then reads exit-status event.
 */
static int
server_chan_exit_thread(void *arg)
{
	struct server_chan_echo_arg *a = arg;
	struct selftest_ctx *ctx = a->ctx;
	a->result = -1;
	struct dssh_chan_accept_cbs cbs = {
		.shell = test_accept_shell_cb,
		.exec = test_accept_exec_cb,
	};
	dssh_channel ch = selftest_accept(ctx, &cbs, 30000);
	if (ch == NULL)
		return 0;
	const uint8_t msg[] = "hello";
	bool complete = selftest_write_all(ch, msg, sizeof(msg) - 1, 30000);
	int ret = selftest_channel_close(ctx, false, ch, 42);
	if (complete && ret == 0)
		a->result = 0;
	return 0;
}


static int
test_self_chan_event_exit_status(void)
{
	struct selftest_ctx ctx;
	if (selftest_setup(&ctx) != 0)
		return TEST_FAIL;
	ASSERT_EQ(dssh_session_start(ctx.client), 0);
	ASSERT_EQ(dssh_session_start(ctx.server), 0);
	struct server_chan_echo_arg arg = { .ctx = &ctx, .expected_type = DSSH_CHAN_EXEC };
	ASSERT_EQ(selftest_start_thread(&ctx, server_chan_exit_thread, &arg), 0);
	dssh_channel ch = open_exec(&ctx, "test");
	ASSERT_NOT_NULL(ch);
	const uint8_t expected[] = "hello";
	uint8_t received[sizeof(expected) - 1];
	ASSERT_TRUE(selftest_collect(ch, received, sizeof(received), NULL, 42));
	ASSERT_MEM_EQ(received, expected, sizeof(received));
	ASSERT_FALSE(dssh_session_is_terminated(ctx.client));
	ASSERT_EQ(selftest_channel_close(&ctx, true, ch, -1), 0);
	selftest_join(&ctx.server_thread, &ctx.server_thread_active);
	ASSERT_EQ(arg.result, 0);
	selftest_cleanup(&ctx);
	return TEST_PASS;
}


/* ================================================================
 * Phase 7: Additional coverage tests
 * ================================================================ */

/* Read peek: dssh_chan_read(ch, 0, NULL, 0) returns available bytes */
static int
test_self_chan_read_peek(void)
{
	struct selftest_ctx ctx;
	if (selftest_setup(&ctx) != 0)
		return TEST_FAIL;
	ASSERT_EQ(dssh_session_start(ctx.client), 0);
	ASSERT_EQ(dssh_session_start(ctx.server), 0);
	struct server_echo_arg arg = { .ctx = &ctx };
	ASSERT_EQ(selftest_start_thread(&ctx, server_echo_thread, &arg), 0);
	dssh_channel ch = open_exec(&ctx, "peek-test");
	ASSERT_NOT_NULL(ch);
	const uint8_t expected[] = "hello";
	ASSERT_TRUE(selftest_write_all(ch, expected, sizeof(expected) - 1, 30000));
	uint8_t received[sizeof(expected) - 1];
	size_t used = 0;
	while (used < sizeof(received)) {
		ASSERT_EQ(dssh_chan_poll(ch, DSSH_POLL_READ, 30000), DSSH_POLL_READ);
		int64_t available = dssh_chan_read(ch, 0, NULL, 0);
		size_t count;
		ASSERT_TRUE(selftest_io_count(available, sizeof(received) - used, &count));
		ASSERT_TRUE(count != 0);
		int64_t again = dssh_chan_read(ch, 0, NULL, 0);
		ASSERT_EQ(again, available);
		ASSERT_EQ(dssh_chan_read(ch, 0, received + used, count), available);
		used += count;
	}
	ASSERT_MEM_EQ(received, expected, sizeof(received));
	ASSERT_EQ(selftest_channel_close(&ctx, true, ch, -1), 0);
	selftest_join(&ctx.server_thread, &ctx.server_thread_active);
	ASSERT_EQ(arg.result, 0);
	selftest_cleanup(&ctx);
	return TEST_PASS;
}


/* Shutwr then close: EOF not sent twice */
static int
test_self_chan_shutwr_then_close(void)
{
	struct selftest_ctx ctx;

	if (selftest_setup(&ctx) < 0)
		return TEST_FAIL;

	ASSERT_EQ(dssh_session_start(ctx.client), 0);
	ASSERT_EQ(dssh_session_start(ctx.server), 0);

	struct server_echo_arg sarg = { .ctx = &ctx };
	ASSERT_EQ(selftest_start_thread(&ctx, server_echo_thread, &sarg), 0);

	dssh_channel ch = open_exec(&ctx, "shutwr-test");

	ASSERT_NOT_NULL(ch);

	/* shutwr sends EOF */
	ASSERT_EQ(dssh_chan_shutwr(ch), 0);

	/* close should not fail even though EOF already sent */
	ASSERT_EQ(selftest_channel_close(&ctx, true, ch, 0), 0);

	selftest_join(&ctx.server_thread, &ctx.server_thread_active);
	ASSERT_EQ(sarg.result, 0);

	selftest_cleanup(&ctx);
	return TEST_PASS;
}

/* A client must omit exit-status even when its close argument is 42. */
static int
server_close_observer(void *argument)
{
	struct server_echo_arg *arg = argument;
	struct selftest_ctx *ctx = arg->ctx;
	arg->result = -1;
	dssh_channel ch = selftest_accept(ctx, &selftest_accept_cbs, 30000);
	if (ch == NULL)
		return 0;
	bool complete = selftest_collect(ch, NULL, 0, NULL, -1)
	    && !dssh_session_is_terminated(ctx->server);
	int ret = selftest_channel_close(ctx, false, ch, -1);
	if (complete && ret == 0)
		arg->result = 0;
	return 0;
}

/* Close with exit code */
static int
test_self_chan_close_exit_code(void)
{
	struct selftest_ctx ctx;
	if (selftest_setup(&ctx) != 0)
		return TEST_FAIL;
	ASSERT_EQ(dssh_session_start(ctx.client), 0);
	ASSERT_EQ(dssh_session_start(ctx.server), 0);
	struct server_echo_arg arg = { .ctx = &ctx };
	ASSERT_EQ(selftest_start_thread(&ctx, server_close_observer, &arg), 0);
	dssh_channel ch = open_exec(&ctx, "client-status-omission");
	ASSERT_NOT_NULL(ch);
	ASSERT_EQ(selftest_channel_close(&ctx, true, ch, 42), 0);
	selftest_join(&ctx.server_thread, &ctx.server_thread_active);
	ASSERT_EQ(arg.result, 0);
	ASSERT_FALSE(dssh_session_is_terminated(ctx.server));
	selftest_cleanup(&ctx);
	return TEST_PASS;
}


/* Close without exit code (negative) */
static int
test_self_chan_close_no_exit_code(void)
{
	struct selftest_ctx ctx;
	if (selftest_setup(&ctx) != 0)
		return TEST_FAIL;
	ASSERT_EQ(dssh_session_start(ctx.client), 0);
	ASSERT_EQ(dssh_session_start(ctx.server), 0);
	struct server_echo_arg arg = { .ctx = &ctx };
	ASSERT_EQ(selftest_start_thread(&ctx, server_close_observer, &arg), 0);
	dssh_channel ch = open_exec(&ctx, "client-status-omission");
	ASSERT_NOT_NULL(ch);
	ASSERT_EQ(selftest_channel_close(&ctx, true, ch, -1), 0);
	selftest_join(&ctx.server_thread, &ctx.server_thread_active);
	ASSERT_EQ(arg.result, 0);
	ASSERT_FALSE(dssh_session_is_terminated(ctx.server));
	selftest_cleanup(&ctx);
	return TEST_PASS;
}


struct selftest_break_action {
	atomic_bool entered;
	atomic_bool duplicate;
	atomic_bool seen;
	atomic_bool valid;
};

static bool
selftest_break_action(dssh_channel ch, const struct dssh_chan_break_request *request,
    void *data)
{
	struct selftest_break_action *action = data;
	if (atomic_exchange(&action->entered, true)) {
		atomic_store(&action->duplicate, true);
		return false;
	}
	bool valid = ch != NULL && request != NULL && request->length == 500
	    && !request->want_reply && request->stdout_pos == 0 && request->stderr_pos == 0;
	atomic_store(&action->valid, valid);
	atomic_store(&action->seen, true);
	return valid;
}

/* Send break (RFC 4335) */
static int
test_self_chan_send_break(void)
{
	struct selftest_ctx ctx;
	if (selftest_setup(&ctx) != 0)
		return TEST_FAIL;
	struct selftest_break_action action;
	atomic_init(&action.entered, false);
	atomic_init(&action.duplicate, false);
	atomic_init(&action.seen, false);
	atomic_init(&action.valid, false);
	ASSERT_EQ(dssh_session_set_break_cb(ctx.server, selftest_break_action, &action), 0);
	atomic_store(&delayed_tx_sess, ctx.server);
	atomic_store(&delayed_tx_release, false);
	atomic_store(&delayed_tx_countdown, 2);
	ASSERT_EQ(dssh_session_start(ctx.client), 0);
	ASSERT_EQ(dssh_session_start(ctx.server), 0);
	struct server_echo_arg arg = { .ctx = &ctx };
	ASSERT_EQ(selftest_start_thread(&ctx, server_echo_thread, &arg), 0);
	dssh_channel ch = open_exec(&ctx, "break-test");
	ASSERT_NOT_NULL(ch);
	ASSERT_TRUE(dssh_test_wait_bool(&delayed_tx_seen, true));
	ASSERT_FALSE(atomic_load(&delayed_tx_done));
	ASSERT_EQ(dssh_chan_send_break(ch, 500), 0);
	ASSERT_TRUE(dssh_test_wait_bool(&action.seen, true));
	ASSERT_TRUE(atomic_load(&action.valid));
	ASSERT_FALSE(atomic_load(&delayed_tx_release));
	atomic_store(&delayed_tx_release, true);
	ASSERT_TRUE(dssh_test_wait_bool(&delayed_tx_done, true));
	ASSERT_EQ(selftest_channel_close(&ctx, true, ch, -1), 0);
	selftest_join(&ctx.server_thread, &ctx.server_thread_active);
	ASSERT_EQ(arg.result, 0);
	selftest_cleanup(&ctx);
	ASSERT_FALSE(atomic_load(&action.duplicate));
	return TEST_PASS;
}


/* Getters after accept: server verifies type, command, pty state */
struct server_getter_arg {
	struct selftest_ctx *ctx;
	enum dssh_chan_type type;
	bool command_matches;
	bool has_pty;
	int result;
};

static int
server_getter_thread(void *arg)
{
	struct server_getter_arg *a = arg;
	struct selftest_ctx *ctx = a->ctx;
	a->result = -1;

	struct dssh_chan_accept_cbs cbs = {
		.shell = test_accept_shell_cb,
		.exec = test_accept_exec_cb,
	};

	dssh_channel ch = selftest_accept(ctx, &cbs, 30000);

	if (ch == NULL)
		return 0;

	a->type = dssh_chan_get_type(ch);
	const char *command = dssh_chan_get_command(ch);
	a->command_matches = command != NULL && strcmp(command, "my-command") == 0;
	a->has_pty = dssh_chan_has_pty(ch);

	a->result = selftest_channel_close(ctx, false, ch, 0);
	return 0;
}

static int
test_self_chan_getters_after_accept(void)
{
	struct selftest_ctx ctx;

	if (selftest_setup(&ctx) < 0)
		return TEST_FAIL;

	ASSERT_EQ(dssh_session_start(ctx.client), 0);
	ASSERT_EQ(dssh_session_start(ctx.server), 0);

	struct server_getter_arg sarg = { .ctx = &ctx };
	ASSERT_EQ(selftest_start_thread(&ctx, server_getter_thread, &sarg), 0);

	struct dssh_chan_params *p = &ctx.params;

	ASSERT_EQ(selftest_params_init(&ctx, DSSH_CHAN_EXEC), 0);
	ASSERT_EQ(dssh_chan_params_set_command(p, "my-command"), 0);
	ASSERT_EQ(dssh_chan_params_set_pty(p, false), 0);

	dssh_channel ch = selftest_open(&ctx, p);

	selftest_params_free(&ctx);
	ASSERT_NOT_NULL(ch);

	ASSERT_EQ(selftest_channel_close(&ctx, true, ch, -1), 0);
	selftest_join(&ctx.server_thread, &ctx.server_thread_active);

	ASSERT_EQ(sarg.result, 0);
	ASSERT_EQ(sarg.type, DSSH_CHAN_EXEC);
	ASSERT_TRUE(sarg.command_matches);
	ASSERT_FALSE(sarg.has_pty);

	selftest_cleanup(&ctx);
	return TEST_PASS;
}

/* NULL ENV callback omits the requested environment; terminal setup succeeds. */
struct server_env_reject_arg {
	struct selftest_ctx *ctx;
	int result;
};

static int
selftest_env_omitted(dssh_channel ch, const struct dssh_chan_params *params,
    struct dssh_chan_accept_result *result, void *data)
{
	(void)ch;
	(void)result;
	(void)data;
	return params->env_count == 0 && params->command != NULL
	    && strcmp(params->command, "env-test") == 0 ? 0 : DSSH_ERROR_INVALID;
}

static int
server_env_reject_thread(void *arg)
{
	struct server_env_reject_arg *a = arg;
	struct selftest_ctx *ctx = a->ctx;
	a->result = -1;

	/* No ENV callback; verify omission at the terminal callback. */
	struct dssh_chan_accept_cbs cbs = {
		.exec = selftest_env_omitted,
	};

	dssh_channel ch = selftest_accept(ctx, &cbs, 30000);

	if (ch == NULL)
		return 0;

	/* Terminal setup completed with an empty accumulated environment. */
	a->result = selftest_channel_close(ctx, false, ch, 0);
	return 0;
}

static int
test_self_chan_accept_env_reject(void)
{
	struct selftest_ctx ctx;

	if (selftest_setup(&ctx) < 0)
		return TEST_FAIL;

	ASSERT_EQ(dssh_session_start(ctx.client), 0);
	ASSERT_EQ(dssh_session_start(ctx.server), 0);

	struct server_env_reject_arg sarg = { .ctx = &ctx };
	ASSERT_EQ(selftest_start_thread(&ctx, server_env_reject_thread, &sarg), 0);

	/* ENV uses want_reply=false; only terminal setup is acknowledged. */
	struct dssh_chan_params *p = &ctx.params;

	ASSERT_EQ(selftest_params_init(&ctx, DSSH_CHAN_EXEC), 0);
	ASSERT_EQ(dssh_chan_params_set_command(p, "env-test"), 0);
	ASSERT_EQ(dssh_chan_params_set_pty(p, false), 0);
	ASSERT_EQ(dssh_chan_params_add_env(p, "LANG", "en_US.UTF-8"), 0);
	ASSERT_EQ(dssh_chan_params_add_env(p, "TERM", "xterm"), 0);

	dssh_channel ch = selftest_open(&ctx, p);

	selftest_params_free(&ctx);
	ASSERT_NOT_NULL(ch);

	ASSERT_EQ(selftest_channel_close(&ctx, true, ch, -1), 0);
	selftest_join(&ctx.server_thread, &ctx.server_thread_active);
	ASSERT_EQ(sarg.result, 0);

	selftest_cleanup(&ctx);
	return TEST_PASS;
}

/* Transport callbacks run without a library mutex.  Only
 * dssh_session_is_terminated() for the callback's session is permitted;
 * public work on either the same or another session fails. */
static int
test_self_io_callback_contract(void)
{
	struct selftest_ctx ctx;

	if (selftest_setup(&ctx) < 0)
		return TEST_FAIL;

	atomic_store(&callback_probe_sess, ctx.server);
	atomic_store(&callback_probe_other_sess, ctx.client);
	atomic_store(&callback_probe_api_result, 0);
	atomic_store(&callback_probe_other_api_result, 0);
	atomic_store(&callback_probe_lock_result, thrd_error);
	atomic_store(&callback_probe_is_terminated, true);
	atomic_store(&callback_probe_terminate_result, true);
	atomic_store(&callback_probe_kind, CALLBACK_PROBE_TX);
	uint8_t msg[] = { SSH_MSG_SERVICE_REQUEST, 0 };

	ASSERT_EQ(send_packet(ctx.server, msg, sizeof(msg), NULL), 0);
	ASSERT_EQ(atomic_load(&callback_probe_kind), CALLBACK_PROBE_NONE);
	ASSERT_ERR(atomic_load(&callback_probe_api_result), DSSH_ERROR_INVALID);
	ASSERT_ERR(atomic_load(&callback_probe_other_api_result), DSSH_ERROR_INVALID);
	ASSERT_EQ(atomic_load(&callback_probe_lock_result), thrd_success);
	ASSERT_FALSE(atomic_load(&callback_probe_is_terminated));
	ASSERT_FALSE(atomic_load(&callback_probe_terminate_result));

	atomic_store(&callback_probe_sess, ctx.client);
	atomic_store(&callback_probe_other_sess, ctx.server);
	atomic_store(&callback_probe_api_result, 0);
	atomic_store(&callback_probe_other_api_result, 0);
	atomic_store(&callback_probe_lock_result, thrd_error);
	atomic_store(&callback_probe_is_terminated, true);
	atomic_store(&callback_probe_terminate_result, true);
	atomic_store(&callback_probe_kind, CALLBACK_PROBE_RX);
	ASSERT_EQ(dssh_session_start(ctx.client), 0);
	ASSERT_TRUE(dssh_test_wait_int(&callback_probe_kind, CALLBACK_PROBE_NONE));
	ASSERT_EQ(atomic_load(&callback_probe_kind), CALLBACK_PROBE_NONE);
	ASSERT_ERR(atomic_load(&callback_probe_api_result), DSSH_ERROR_INVALID);
	ASSERT_ERR(atomic_load(&callback_probe_other_api_result), DSSH_ERROR_INVALID);
	ASSERT_EQ(atomic_load(&callback_probe_lock_result), thrd_success);
	ASSERT_FALSE(atomic_load(&callback_probe_is_terminated));
	ASSERT_FALSE(atomic_load(&callback_probe_terminate_result));

	selftest_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Assertion cleanup runs before returning from the test, including joining
 * threads whose arguments are owned by that test's stack frame.
 * ================================================================ */

void
dssh_test_failure_cleanup(void)
{
	if (g_active_ctx != NULL) {
		selftest_cleanup(g_active_ctx);
		g_active_ctx = NULL;
	}
}

/* ================================================================
 * Test table and main
 * ================================================================ */

static struct dssh_test_entry tests[] = {
	{ "test_assertion_cleanup", test_assertion_cleanup },
	{ "test_self_handshake_curve25519",     test_self_handshake_curve25519 },
	{ "test_self_auth_password",            test_self_auth_password },
	{ "test_self_exec_echo",               test_self_exec_echo },
	{ "test_self_window_adjust_race",       test_self_window_adjust_race },
	{ "test_self_exec_exit_code",          test_self_exec_exit_code },
	{ "test_self_shell_echo",              test_self_shell_echo },
	{ "test_self_shell_large_data",        test_self_shell_large_data },
	{ "test_self_signal",                  test_self_signal },
	{ "test_self_window_change",           test_self_window_change },
	{ "test_self_multiple_channels",       test_self_multiple_channels },
	{ "test_self_graceful_disconnect",     test_self_graceful_disconnect },
	{ "test_self_eof_half_close",          test_self_eof_half_close },
	{ "test_self_rekey_during_data",       test_self_rekey_during_data },
	{ "test_self_rekey_manual",            test_self_rekey_manual },
	{ "test_self_rekey_preserves_channels", test_self_rekey_preserves_channels },
	{ "test_self_rekey_inflight_data",     test_self_rekey_inflight_data },
	{ "test_self_connection_drop",         test_self_connection_drop },
	{ "test_self_socket_boundaries", test_self_socket_boundaries },
	{ "test_null_session_api",             test_null_session_api },
	{ "test_log_api",                      test_log_api },
	{ "test_set_timeout",                  test_set_timeout },
	{ "test_set_terminate_cb",             test_set_terminate_cb },
	{ "test_open_channel_timeout",         test_open_channel_timeout },
	{ "test_setup_recv_timeout",           test_setup_recv_timeout },
	{ "test_rekey_send_timeout",           test_rekey_send_timeout },
	{ "test_self_chan_exec_echo",          test_self_chan_exec_echo },
	{ "test_self_chan_shell_pty",          test_self_chan_shell_pty },
	{ "test_self_chan_accept_exec",        test_self_chan_accept_exec },
	{ "test_self_chan_accept_shell_pty",   test_self_chan_accept_shell_pty },
	{ "test_self_chan_event_exit_status",  test_self_chan_event_exit_status },
	{ "test_self_chan_read_peek",          test_self_chan_read_peek },
	{ "test_self_chan_shutwr_then_close",  test_self_chan_shutwr_then_close },
	{ "test_self_chan_close_exit_code",    test_self_chan_close_exit_code },
	{ "test_self_chan_close_no_exit_code", test_self_chan_close_no_exit_code },
	{ "test_self_chan_send_break",         test_self_chan_send_break },
	{ "test_self_chan_getters_after_accept", test_self_chan_getters_after_accept },
	{ "test_self_chan_accept_env_reject",  test_self_chan_accept_env_reject },
	{ "test_self_io_callback_contract",   test_self_io_callback_contract },
};

DSSH_TEST_MAIN(tests)
