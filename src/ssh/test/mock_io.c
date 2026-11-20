/*
 * mock_io.c -- Bidirectional mock I/O using native connected sockets.
 *
 * Each direction (c2s, s2c) is a Unix socketpair or a Windows loopback pair.  Blocking
 * read/write with natural close-unblocks-peer behavior -- no
 * condvars or timed waits needed.
 *
 * Socket handle fields are atomic.  The close helpers use shutdown() to unblock
 * concurrent I/O without racing descriptor lifetime; pipe_free() performs
 * the actual atomic-exchange + close after worker threads have joined.
 */

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include "mock_io.h"

#ifdef _WIN32
#include "mock_io_win32.h"
#else
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#endif

#ifndef _WIN32
static intmax_t
mock_native_send(mock_io_handle fd, const uint8_t *data, size_t count)
{
	return send(fd, data, count, MSG_NOSIGNAL);
}

static intmax_t
mock_native_read(mock_io_handle fd, uint8_t *data, size_t count)
{
	return read(fd, data, count);
}

static intmax_t
mock_native_drain(mock_io_handle fd, uint8_t *data, size_t count)
{
	return recv(fd, data, count, MSG_DONTWAIT);
}

static int
mock_native_shutdown(mock_io_handle fd)
{
	return shutdown(fd, SHUT_RDWR);
}
#endif

static _Thread_local void (*read_retry_hook)(void *);
static _Thread_local void *read_retry_data;

void
mock_io_set_read_retry_hook(void (*callback)(void *), void *opaque)
{
	read_retry_hook = callback;
	read_retry_data = callback == NULL ? NULL : opaque;
}

static void
read_retry(void)
{
	void (*callback)(void *) = read_retry_hook;
	void *opaque = read_retry_data;
	read_retry_hook = NULL;
	read_retry_data = NULL;
	if (callback != NULL) {
		int saved_errno = errno;
		callback(opaque);
		errno = saved_errno;
	}
}

/* ================================================================
 * Pipe operations (socketpair wrapper)
 * ================================================================ */

/* Only numeric counts cross this boundary; no synthetic buffer is needed to
 * exercise either representation limit. */
static size_t
pipe_request(size_t length)
{
#ifdef _WIN32
	uintmax_t maximum = INT_MAX;
#else
	uintmax_t maximum = (uintmax_t)SSIZE_MAX;
#endif
	if (length > maximum) {
		/* maximum < length <= SIZE_MAX proves this conversion fits. */
		size_t bounded = (size_t)maximum;
		return bounded;
	}
	return length;
}

static bool
pipe_count(intmax_t returned, size_t requested, size_t *count)
{
	if (returned <= 0)
		return false;
	uintmax_t value = (uintmax_t)returned;
	if (value > requested)
		return false;
	size_t safe = (size_t)value;
	*count = safe;
	return true;
}

static bool
pipe_line_next(size_t have, size_t *next)
{
	if (have == SIZE_MAX)
		return false;
	*next = have + 1;
	return true;
}

static void
close_owned(mock_io_handle fd)
{
	/* Do not retry a release whose native outcome is unknown. */
#ifdef _WIN32
	if (closesocket(fd) != 0)
#else
	if (close(fd) < 0)
#endif
		abort();
}

static int
pipe_init(struct mock_io_pipe *p)
{
#ifdef _WIN32
	mock_io_handle fds[2];
	if (mock_win_pair(fds) != 0)
		return -1;
#else
	int fds[2];
	if (socketpair(AF_UNIX, SOCK_STREAM, 0, fds) < 0)
		return -1;
	struct timeval tv = {
		.tv_sec = 0,
		.tv_usec = 100000,
	};
	if (setsockopt(fds[0], SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0
	    || setsockopt(fds[0], SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) < 0
	    || setsockopt(fds[1], SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0
	    || setsockopt(fds[1], SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) < 0) {
		close_owned(fds[0]);
		close_owned(fds[1]);
		return -1;
	}
#endif
	atomic_store(&p->wfd, fds[0]);
	atomic_store(&p->rfd, fds[1]);
	atomic_store(&p->read_closed, false);
	atomic_store(&p->write_closed, false);
	return 0;
}

static void
pipe_free(struct mock_io_pipe *p)
{
	atomic_store(&p->read_closed, true);
	atomic_store(&p->write_closed, true);
	mock_io_handle fd = atomic_exchange(&p->rfd, MOCK_IO_INVALID);
	if (fd != MOCK_IO_INVALID)
		close_owned(fd);
	fd = atomic_exchange(&p->wfd, MOCK_IO_INVALID);
	if (fd != MOCK_IO_INVALID)
		close_owned(fd);
}

static void
pipe_close_write(struct mock_io_pipe *p)
{
	if (atomic_exchange(&p->write_closed, true))
		return;
	mock_io_handle fd = atomic_load(&p->wfd);
	if (fd != MOCK_IO_INVALID && mock_native_shutdown(fd) < 0 && errno != ENOTCONN)
		abort();
}

static void
pipe_close_read(struct mock_io_pipe *p)
{
	if (atomic_exchange(&p->read_closed, true))
		return;
	mock_io_handle fd = atomic_load(&p->rfd);
	if (fd != MOCK_IO_INVALID && mock_native_shutdown(fd) < 0 && errno != ENOTCONN)
		abort();
}

/*
 * Write all bytes.  Returns 0 on success, -1 on error/close.
 */
static int
pipe_write(struct mock_io_pipe *p, const uint8_t *data, size_t len)
{
	if (len > 0 && data == NULL)
		return -1;
	mock_io_handle fd = atomic_load(&p->wfd);
	if (fd == MOCK_IO_INVALID || atomic_load(&p->write_closed)
	    || atomic_load(&p->read_closed))
		return -1;
	size_t written = 0;
	while (written < len) {
		size_t requested = pipe_request(len - written);
		intmax_t n = mock_native_send(fd, data + written, requested);
		if (n < 0 && errno == EINTR)
			continue;
		if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
			if (atomic_load(&p->write_closed)
			    || atomic_load(&p->read_closed))
				return -1;
			continue;
		}
		size_t count;
		if (!pipe_count(n, requested, &count))
			return -1;
		written += count;
	}
	return 0;
}

/*
 * Read exactly len bytes.  Returns 0 on success, -1 on error/close.
 */
static int
pipe_read(struct mock_io_pipe *p, uint8_t *buf, size_t len)
{
	if (len > 0 && buf == NULL)
		return -1;
	mock_io_handle fd = atomic_load(&p->rfd);
	if (fd == MOCK_IO_INVALID || atomic_load(&p->read_closed))
		return -1;
	size_t have = 0;
	while (have < len) {
		/* A close observed after EAGAIN can follow a completed write.
		 * Only an already-closed writer makes this read's empty result final. */
		bool writer_closed = atomic_load(&p->write_closed);
		size_t requested = pipe_request(len - have);
		intmax_t n = mock_native_read(fd, buf + have, requested);
		if (n < 0 && errno == EINTR)
			continue;
		if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
			read_retry();
			if (atomic_load(&p->read_closed)
			    || writer_closed)
				return -1;
			continue;
		}
		size_t count;
		if (!pipe_count(n, requested, &count))
			return -1;
		have += count;
	}
	return 0;
}

/*
 * Read a line (up to and including CR-LF).
 */
static int
pipe_readline(struct mock_io_pipe *p, uint8_t *buf, size_t bufsz,
    size_t *bytes_received)
{
	if (bytes_received == NULL)
		return -1;
	*bytes_received = 0;
	if (bufsz > 0 && buf == NULL)
		return -1;
	mock_io_handle fd = atomic_load(&p->rfd);
	if (fd == MOCK_IO_INVALID || atomic_load(&p->read_closed))
		return -1;
	size_t have = 0;
	for (;;) {
		uint8_t ch;
		bool writer_closed = atomic_load(&p->write_closed);
		intmax_t n = mock_native_read(fd, &ch, 1);
		if (n < 0 && errno == EINTR)
			continue;
		if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
			read_retry();
			if (atomic_load(&p->read_closed)
			    || writer_closed)
				return -1;
			continue;
		}
		size_t count, next;
		if (!pipe_count(n, 1, &count) || !pipe_line_next(have, &next))
			return -1;
		if (have < bufsz)
			buf[have] = ch;
		have = next;
		if (have > bufsz || ch == '\n') {
			*bytes_received = have;
			return 0;
		}
	}
}

/* ================================================================
 * Public API
 * ================================================================ */

int
mock_io_init(struct mock_io_state *io, size_t capacity)
{
	(void)capacity;
	if (io == NULL)
		return -1;
#ifdef _WIN32
	WSADATA data;
	if (WSAStartup(MAKEWORD(2, 2), &data) != 0)
		return -1;
#endif
	struct mock_io_pipe *pipes[] = {&io->c2s, &io->s2c};
	for (size_t i = 0; i < sizeof(pipes) / sizeof(pipes[0]); i++) {
		atomic_init(&pipes[i]->rfd, MOCK_IO_INVALID);
		atomic_init(&pipes[i]->wfd, MOCK_IO_INVALID);
		atomic_init(&pipes[i]->read_closed, true);
		atomic_init(&pipes[i]->write_closed, true);
	}
	if (pipe_init(&io->c2s) < 0) {
#ifdef _WIN32
		if (WSACleanup() != 0)
			abort();
#endif
		return -1;
	}
	if (pipe_init(&io->s2c) < 0) {
		pipe_free(&io->c2s);
#ifdef _WIN32
		if (WSACleanup() != 0)
			abort();
#endif
		return -1;
	}
	return 0;
}

void
mock_io_free(struct mock_io_state *io)
{
	pipe_free(&io->c2s);
	pipe_free(&io->s2c);
#ifdef _WIN32
	if (WSACleanup() != 0)
		abort();
#endif
}

void
mock_io_close_c2s(struct mock_io_state *io)
{
	pipe_close_write(&io->c2s);
	pipe_close_read(&io->c2s);
}

void
mock_io_close_c2s_write(struct mock_io_state *io)
{
	pipe_close_write(&io->c2s);
}

void
mock_io_close_s2c(struct mock_io_state *io)
{
	pipe_close_write(&io->s2c);
	pipe_close_read(&io->s2c);
}

void
mock_io_close_s2c_write(struct mock_io_state *io)
{
	pipe_close_write(&io->s2c);
}

/* ================================================================
 * Client callbacks: TX->c2s, RX<-s2c
 * ================================================================ */

int
mock_tx_client(uint8_t *buf, size_t bufsz, dssh_session sess, void *cbdata)
{
	(void)sess;
	if (cbdata == NULL)
		return -1;
	struct mock_io_state *io = cbdata;
	return pipe_write(&io->c2s, buf, bufsz);
}

int
mock_rx_client(uint8_t *buf, size_t bufsz, dssh_session sess, void *cbdata)
{
	(void)sess;
	if (cbdata == NULL)
		return -1;
	struct mock_io_state *io = cbdata;
	return pipe_read(&io->s2c, buf, bufsz);
}

int
mock_rxline_client(uint8_t *buf, size_t bufsz, size_t *bytes_received,
    dssh_session sess, void *cbdata)
{
	(void)sess;
	if (cbdata == NULL)
		return -1;
	struct mock_io_state *io = cbdata;
	return pipe_readline(&io->s2c, buf, bufsz, bytes_received);
}

/* ================================================================
 * Server callbacks: TX->s2c, RX<-c2s
 * ================================================================ */

int
mock_tx_server(uint8_t *buf, size_t bufsz, dssh_session sess, void *cbdata)
{
	(void)sess;
	if (cbdata == NULL)
		return -1;
	struct mock_io_state *io = cbdata;
	return pipe_write(&io->s2c, buf, bufsz);
}

int
mock_rx_server(uint8_t *buf, size_t bufsz, dssh_session sess, void *cbdata)
{
	(void)sess;
	if (cbdata == NULL)
		return -1;
	struct mock_io_state *io = cbdata;
	return pipe_read(&io->c2s, buf, bufsz);
}

int
mock_rxline_server(uint8_t *buf, size_t bufsz, size_t *bytes_received,
    dssh_session sess, void *cbdata)
{
	(void)sess;
	if (cbdata == NULL)
		return -1;
	struct mock_io_state *io = cbdata;
	return pipe_readline(&io->c2s, buf, bufsz, bytes_received);
}

/* ================================================================
 * Single-sided helpers
 * ================================================================ */

size_t
mock_io_inject(struct mock_io_pipe *p, const uint8_t *data, size_t len)
{
	if (len > 0 && data == NULL)
		return 0;
	mock_io_handle fd = atomic_load(&p->wfd);
	if (fd == MOCK_IO_INVALID)
		return 0;
	size_t requested = pipe_request(len);
	intmax_t n = mock_native_send(fd, data, requested);
	size_t count;
	return pipe_count(n, requested, &count) ? count : 0;
}

size_t
mock_io_drain(struct mock_io_pipe *p, uint8_t *buf, size_t bufsz)
{
	if (bufsz > 0 && buf == NULL)
		return 0;
	mock_io_handle fd = atomic_load(&p->rfd);
	if (fd == MOCK_IO_INVALID)
		return 0;
	size_t requested = pipe_request(bufsz);
	intmax_t n = mock_native_drain(fd, buf, requested);
	size_t count;
	return pipe_count(n, requested, &count) ? count : 0;
}

int
mock_extra_line_cb(uint8_t *buf, size_t bufsz, void *cbdata)
{
	(void)buf;
	(void)bufsz;
	(void)cbdata;
	return 0;
}
