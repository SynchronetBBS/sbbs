/* Private controls for the actual included integration socket callbacks.
 * Only the controlling thread is armed. Other fixture calls forward unchanged. */
enum owner_socket_operation { OWNER_POLL, OWNER_TX, OWNER_RX, OWNER_LINE };
enum owner_socket_fault {
	OWNER_NATIVE, OWNER_EINTR, OWNER_BEFORE, OWNER_AFTER, OWNER_ZERO,
	OWNER_OVERSIZE, OWNER_SHORT, OWNER_SHORT_ERROR, OWNER_POLL_ZERO,
	OWNER_POLL_COUNT, OWNER_POLL_NVAL, OWNER_POLL_HUP, OWNER_POLL_ERR,
	OWNER_POLL_OTHER, OWNER_TERMINATED, OWNER_FULL, OWNER_CAPACITY,
	OWNER_EOF, OWNER_EMPTY_EOF
};

struct owner_socket_case {
	const char *name;
	enum owner_socket_operation operation;
	enum owner_socket_fault fault;
	int result;
	unsigned calls, native, hits;
	size_t bytes, received;
};

static const struct owner_socket_case owner_socket_cases[] = {
	{ "poll-native", OWNER_POLL, OWNER_NATIVE, 0, 1, 1, 0, 0, 0 },
	{ "poll-eintr", OWNER_POLL, OWNER_EINTR, 0, 2, 1, 1, 0, 0 },
	{ "poll-error-before", OWNER_POLL, OWNER_BEFORE, -1, 1, 0, 1, 0, 0 },
	{ "poll-error-after", OWNER_POLL, OWNER_AFTER, -1, 1, 1, 1, 0, 0 },
	{ "poll-zero", OWNER_POLL, OWNER_POLL_ZERO, 0, 2, 2, 1, 0, 0 },
	{ "poll-count", OWNER_POLL, OWNER_POLL_COUNT, -1, 1, 1, 1, 0, 0 },
	{ "poll-nval", OWNER_POLL, OWNER_POLL_NVAL, -1, 1, 1, 1, 0, 0 },
	{ "poll-hup", OWNER_POLL, OWNER_POLL_HUP, 0, 1, 1, 1, 0, 0 },
	{ "poll-err", OWNER_POLL, OWNER_POLL_ERR, 0, 1, 1, 1, 0, 0 },
	{ "poll-other", OWNER_POLL, OWNER_POLL_OTHER, 0, 2, 2, 1, 0, 0 },
	{ "poll-terminated", OWNER_POLL, OWNER_TERMINATED, -1, 0, 0, 0, 0, 0 },
	{ "send-native", OWNER_TX, OWNER_NATIVE, 0, 1, 1, 0, 6, 0 },
	{ "send-eintr", OWNER_TX, OWNER_EINTR, 0, 2, 1, 1, 6, 0 },
	{ "send-error-before", OWNER_TX, OWNER_BEFORE, -1, 1, 0, 1, 0, 0 },
	{ "send-error-after", OWNER_TX, OWNER_AFTER, -1, 1, 1, 1, 6, 0 },
	{ "send-zero", OWNER_TX, OWNER_ZERO, -1, 1, 0, 1, 0, 0 },
	{ "send-oversize", OWNER_TX, OWNER_OVERSIZE, -1, 1, 1, 1, 6, 0 },
	{ "send-short", OWNER_TX, OWNER_SHORT, 0, 2, 2, 1, 6, 0 },
	{ "send-short-error", OWNER_TX, OWNER_SHORT_ERROR, -1, 2, 1, 2, 2, 0 },
	{ "recv-native", OWNER_RX, OWNER_NATIVE, 0, 1, 1, 0, 6, 0 },
	{ "recv-eintr", OWNER_RX, OWNER_EINTR, 0, 2, 1, 1, 6, 0 },
	{ "recv-error-before", OWNER_RX, OWNER_BEFORE, -1, 1, 0, 1, 0, 0 },
	{ "recv-error-after", OWNER_RX, OWNER_AFTER, -1, 1, 1, 1, 6, 0 },
	{ "recv-zero", OWNER_RX, OWNER_ZERO, -1, 1, 0, 1, 0, 0 },
	{ "recv-oversize", OWNER_RX, OWNER_OVERSIZE, -1, 1, 1, 1, 6, 0 },
	{ "recv-short", OWNER_RX, OWNER_SHORT, 0, 2, 2, 1, 6, 0 },
	{ "recv-short-error", OWNER_RX, OWNER_SHORT_ERROR, -1, 2, 1, 2, 2, 0 },
	{ "recv-eof", OWNER_RX, OWNER_EOF, -1, 1, 1, 0, 0, 0 },
	{ "line-native", OWNER_LINE, OWNER_NATIVE, 0, 2, 2, 0, 2, 2 },
	{ "line-eintr", OWNER_LINE, OWNER_EINTR, 0, 3, 2, 1, 2, 2 },
	{ "line-error-before", OWNER_LINE, OWNER_BEFORE, -1, 1, 0, 1, 0, 0 },
	{ "line-error-after", OWNER_LINE, OWNER_AFTER, -1, 1, 1, 1, 1, 0 },
	{ "line-zero", OWNER_LINE, OWNER_ZERO, -1, 1, 0, 1, 0, 0 },
	{ "line-oversize", OWNER_LINE, OWNER_OVERSIZE, -1, 1, 1, 1, 1, 0 },
	{ "line-full", OWNER_LINE, OWNER_FULL, -1, 3, 3, 0, 3, 0 },
	{ "line-capacity", OWNER_LINE, OWNER_CAPACITY, 0, 3, 3, 0, 3, 3 },
	{ "line-eof", OWNER_LINE, OWNER_EOF, -1, 2, 2, 0, 1, 0 },
	{ "line-empty-eof", OWNER_LINE, OWNER_EMPTY_EOF, -1, 1, 1, 0, 0, 0 }
};

static _Thread_local struct {
	const struct owner_socket_case *selected;
	int fd;
	unsigned calls, native, hits;
	size_t bytes;
	bool valid;
} owner_socket;
static _Thread_local mtx_t *owner_unlock_target;

/* Bounded controls fail rather than spin if the intended retry never ends. */
static unsigned
owner_socket_call(void)
{
	if (owner_socket.calls >= 32)
		abort();
	return owner_socket.calls++;
}

static bool
owner_socket_refuse(unsigned index)
{
	enum owner_socket_fault fault = owner_socket.selected->fault;
	if ((index == 0 && (fault == OWNER_BEFORE || fault == OWNER_EINTR || fault == OWNER_ZERO))
	    || (index == 1 && fault == OWNER_SHORT_ERROR)) {
		owner_socket.hits++;
		errno = fault == OWNER_EINTR ? EINTR : EIO;
		return true;
	}
	return false;
}

static int
owner_socket_poll(struct pollfd *fds, nfds_t count, int timeout)
{
	if (owner_socket.selected == NULL || owner_socket.selected->operation != OWNER_POLL)
		return poll(fds, count, timeout);
	unsigned index = owner_socket_call();
	if (fds == NULL || count != 1 || fds[0].fd != owner_socket.fd
	    || fds[0].events != POLLIN || timeout != 100) {
		owner_socket.valid = false;
		return -1;
	}
	if (owner_socket_refuse(index))
		return -1;
	owner_socket.native++;
	int ret = poll(fds, count, timeout);
	if (ret != 1 || (fds[0].revents & POLLIN) == 0) {
		owner_socket.valid = false;
		return ret;
	}
	if (index != 0)
		return ret;
	enum owner_socket_fault fault = owner_socket.selected->fault;
	if (fault == OWNER_NATIVE)
		return ret;
	owner_socket.hits++;
	if (fault == OWNER_AFTER) {
		errno = EIO;
		return -1;
	}
	if (fault == OWNER_POLL_ZERO)
		return 0;
	if (fault == OWNER_POLL_COUNT)
		return 2;
	fds[0].revents = fault == OWNER_POLL_NVAL ? POLLNVAL
	    : fault == OWNER_POLL_HUP ? POLLHUP
	    : fault == OWNER_POLL_ERR ? POLLERR : POLLPRI;
	return ret;
}

static ssize_t
owner_socket_transfer(int fd, const void *source, void *destination, size_t requested, int flags)
{
	unsigned index = owner_socket_call();
	bool sending = owner_socket.selected->operation == OWNER_TX;
	if (fd != owner_socket.fd || requested == 0 || requested > 6 || flags != 0
	    || (sending ? source == NULL : destination == NULL)) {
		owner_socket.valid = false;
		return -1;
	}
	if (owner_socket_refuse(index))
		return owner_socket.selected->fault == OWNER_ZERO ? 0 : -1;
	enum owner_socket_fault fault = owner_socket.selected->fault;
	size_t actual = requested;
	if (index == 0 && (fault == OWNER_SHORT || fault == OWNER_SHORT_ERROR)) {
		if (requested <= 2) {
			owner_socket.valid = false;
			return -1;
		}
		actual = 2;
		owner_socket.hits++;
	}
	owner_socket.native++;
	ssize_t ret = sending ? send(fd, source, actual, flags) : recv(fd, destination, actual, flags);
	if (ret < 0 || (ret > 0 && !socket_count(ret, actual, &actual))) {
		owner_socket.valid = false;
		return -1;
	}
	if (ret > 0) {
		if (actual > 6 - owner_socket.bytes) {
			owner_socket.valid = false;
			return -1;
		}
		owner_socket.bytes += actual;
	}
	if (index == 0 && (fault == OWNER_AFTER || fault == OWNER_OVERSIZE)) {
		owner_socket.hits++;
		if (fault == OWNER_AFTER) {
			errno = EIO;
			return -1;
		}
		/* Requests are bounded above by6; both values fit any ssize_t. */
		return requested == 1 ? 2 : 7;
	}
	return ret;
}

static ssize_t
owner_socket_send(int fd, const void *buffer, size_t length, int flags)
{
	if (owner_socket.selected == NULL || owner_socket.selected->operation != OWNER_TX)
		return send(fd, buffer, length, flags);
	return owner_socket_transfer(fd, buffer, NULL, length, flags);
}

static ssize_t
owner_socket_recv(int fd, void *buffer, size_t length, int flags)
{
	if (owner_socket.selected == NULL || (owner_socket.selected->operation != OWNER_RX
	    && owner_socket.selected->operation != OWNER_LINE))
		return recv(fd, buffer, length, flags);
	return owner_socket_transfer(fd, NULL, buffer, length, flags);
}

static int
owner_callback_unlock(mtx_t *mutex)
{
	if (mutex != owner_unlock_target)
		return native_unlock(mutex);
	bool completed = strcmp(owner_mode, "callback-unlock-released") == 0;
	if (completed && native_unlock(mutex) != thrd_success)
		abort();
	owner_note("SELFTEST_CALLBACK_UNLOCK mode=%s native=%u reported_error=1\n",
	    owner_mode, completed ? 1U : 0U);
	return thrd_error;
}

/* Read only currently available native bytes. There is no blocking fallback
 * when a callback failed to perform the expected transfer. */
static bool
owner_socket_drain(int fd, const uint8_t *expected, size_t length)
{
	uint8_t received[8];
	size_t used = 0;
	for (;;) {
		/* This standalone session has no demux or other socket reader.
		 * Ready bytes cannot be consumed between this poll and recv. */
		struct pollfd ready = { .fd = fd, .events = POLLIN };
		int polled = poll(&ready, 1, 0);
		if (polled < 0 && errno == EINTR)
			continue;
		if (polled == 0)
			break;
		if (polled != 1 || (ready.revents & POLLNVAL) != 0
		    || (ready.revents & (POLLIN | POLLERR | POLLHUP)) == 0)
			return false;
		ssize_t ret = recv(fd, received + used, sizeof(received) - used, 0);
		if (ret < 0 && errno == EINTR)
			continue;
		if (ret < 0)
			return false;
		if (ret == 0)
			break;
		size_t count;
		if (!socket_count(ret, sizeof(received) - used, &count))
			return false;
		used += count;
		if (used == sizeof(received))
			return false;
	}
	return used == length && memcmp(received, expected, length) == 0;
}

static int
owner_socket_control(void)
{
	struct selftest_ctx ctx;
	if (selftest_standalone(&ctx) != 0)
		return 2;
	int pair[2];
	if (socketpair(AF_UNIX, SOCK_STREAM, 0, pair) != 0) {
		selftest_cleanup(&ctx);
		return 2;
	}
	ctx.client_fd = pair[0];
	ctx.server_fd = pair[1];
	if (strncmp(owner_mode, "callback-unlock-", 16) == 0) {
		bool plain = strcmp(owner_mode, "callback-unlock-native") == 0;
		if (!plain && strcmp(owner_mode, "callback-unlock-retained") != 0
		    && strcmp(owner_mode, "callback-unlock-released") != 0)
			return owner_finish(&ctx, false);
		atomic_store(&callback_probe_sess, ctx.client);
		atomic_store(&callback_probe_other_sess, ctx.client);
		atomic_store(&callback_probe_kind, CALLBACK_PROBE_TX);
		owner_unlock_target = plain ? NULL : &ctx.client->trans.tx_mtx;
		run_callback_probe(ctx.client, CALLBACK_PROBE_TX);
		owner_unlock_target = NULL;
		bool valid = plain && atomic_load(&callback_probe_kind) == CALLBACK_PROBE_NONE
		    && atomic_load(&callback_probe_lock_result) == thrd_success;
		return owner_finish(&ctx, valid);
	}
	const struct owner_socket_case *selected = NULL;
	for (size_t i = 0; i < sizeof(owner_socket_cases) / sizeof(owner_socket_cases[0]); i++) {
		if (strcmp(owner_mode + 7, owner_socket_cases[i].name) == 0)
			selected = &owner_socket_cases[i];
	}
	if (selected == NULL)
		return owner_finish(&ctx, false);
	uint8_t input[] = { 'A', 'B', 'C', 'D', 'E', 'F' };
	size_t available = selected->operation == OWNER_TX ? 0
	    : selected->operation == OWNER_POLL ? 1
	    : selected->operation == OWNER_LINE ? 3 : sizeof(input);
	if (selected->operation == OWNER_LINE) {
		if (selected->fault == OWNER_EOF)
			available = 1;
		else if (selected->fault == OWNER_CAPACITY)
			input[2] = '\n';
		else if (selected->fault != OWNER_FULL) {
			input[1] = '\n';
			input[2] = 'Z';
		}
	}
	if (selected->fault == OWNER_EMPTY_EOF
	    || (selected->operation == OWNER_RX && selected->fault == OWNER_EOF))
		available = 0;
	if (available != 0) {
		ssize_t written = send(pair[1], input, available, 0);
		size_t count;
		if (!socket_count(written, available, &count) || count != available)
			return owner_finish(&ctx, false);
	}
	if ((selected->fault == OWNER_EOF || selected->fault == OWNER_EMPTY_EOF)
	    && shutdown(pair[1], SHUT_WR) != 0)
		return owner_finish(&ctx, false);
	if (selected->fault == OWNER_TERMINATED && !dssh_session_terminate(ctx.client))
		return owner_finish(&ctx, false);
	uint8_t output[sizeof(input)];
	memset(output, 0xa5, sizeof(output));
	size_t received = 42;
	owner_socket.selected = selected;
	owner_socket.fd = pair[0];
	owner_socket.valid = true;
	int ret;
	if (selected->operation == OWNER_POLL)
		ret = socket_wait(pair[0], POLLIN, ctx.client);
	else if (selected->operation == OWNER_TX)
		ret = socket_tx(input, sizeof(input), ctx.client, &ctx.client_fd);
	else if (selected->operation == OWNER_RX)
		ret = socket_rx(output, sizeof(output), ctx.client, &ctx.client_fd);
	else
		ret = socket_rxline(output, 3, &received, ctx.client, &ctx.client_fd);
	owner_socket.selected = NULL;
	bool valid = owner_socket.valid && ret == selected->result
	    && owner_socket.calls == selected->calls && owner_socket.native == selected->native
	    && owner_socket.hits == selected->hits && owner_socket.bytes == selected->bytes;
	if (selected->operation == OWNER_LINE)
		valid &= received == selected->received;
	if (selected->operation == OWNER_RX || selected->operation == OWNER_LINE) {
		valid &= owner_socket.bytes <= sizeof(output) && owner_socket.bytes <= available;
		if (valid) {
			valid &= memcmp(output, input, owner_socket.bytes) == 0;
			for (size_t i = owner_socket.bytes; i < sizeof(output); i++)
				valid &= output[i] == 0xa5;
			valid &= owner_socket_drain(pair[0], input + owner_socket.bytes,
			    available - owner_socket.bytes);
		}
	}
	else if (selected->operation == OWNER_TX)
		valid &= owner_socket_drain(pair[1], input, owner_socket.bytes);
	else
		valid &= owner_socket_drain(pair[0], input, available);
	owner_note("SELFTEST_SOCKET mode=%s calls=%u native=%u hits=%u bytes=%zu result=%d line_count=%zu valid=%u\n",
	    owner_mode, owner_socket.calls, owner_socket.native, owner_socket.hits,
	    owner_socket.bytes, ret, selected->operation == OWNER_LINE ? received : 0,
	    valid ? 1U : 0U);
	return owner_finish(&ctx, valid);
}
