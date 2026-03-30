/*
 * mock_io.c -- Bidirectional mock I/O using socketpair().
 *
 * Each direction (c2s, s2c) is a Unix socketpair.  Blocking
 * read/write with natural close-unblocks-peer behavior -- no
 * condvars or timed waits needed.
 *
 * Fd fields are _Atomic int so that concurrent close from two
 * threads (e.g. both handshake threads failing) uses atomic_exchange
 * to guarantee exactly one close() per fd.
 */

#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "mock_io.h"

/* ================================================================
 * Pipe operations (socketpair wrapper)
 * ================================================================ */

static int
pipe_init(struct mock_io_pipe *p)
{
	int fds[2];
	if (socketpair(AF_UNIX, SOCK_STREAM, 0, fds) < 0)
		return -1;
	atomic_store(&p->wfd, fds[0]);
	atomic_store(&p->rfd, fds[1]);
	return 0;
}

static void
pipe_free(struct mock_io_pipe *p)
{
	int fd;
	fd = atomic_exchange(&p->rfd, -1);
	if (fd >= 0)
		close(fd);
	fd = atomic_exchange(&p->wfd, -1);
	if (fd >= 0)
		close(fd);
}

static void
pipe_close_write(struct mock_io_pipe *p)
{
	int fd = atomic_exchange(&p->wfd, -1);
	if (fd >= 0)
		close(fd);
}

static void
pipe_close_read(struct mock_io_pipe *p)
{
	int fd = atomic_exchange(&p->rfd, -1);
	if (fd >= 0)
		close(fd);
}

/*
 * Write all bytes.  Returns 0 on success, -1 on error/close.
 */
static int
pipe_write(struct mock_io_pipe *p, const uint8_t *data, size_t len)
{
	int fd = atomic_load(&p->wfd);
	if (fd < 0)
		return -1;
	size_t written = 0;
	while (written < len) {
		ssize_t n = send(fd, data + written, len - written,
		    MSG_NOSIGNAL);
		if (n < 0 && errno == EINTR)
			continue;
		if (n <= 0)
			return -1;
		written += (size_t)n;
	}
	return 0;
}

/*
 * Read exactly len bytes.  Returns 0 on success, -1 on error/close.
 */
static int
pipe_read(struct mock_io_pipe *p, uint8_t *buf, size_t len)
{
	int fd = atomic_load(&p->rfd);
	if (fd < 0)
		return -1;
	size_t have = 0;
	while (have < len) {
		ssize_t n = read(fd, buf + have, len - have);
		if (n < 0 && errno == EINTR)
			continue;
		if (n <= 0)
			return -1;
		have += (size_t)n;
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
	int fd = atomic_load(&p->rfd);
	if (fd < 0)
		return -1;
	size_t have = 0;
	for (;;) {
		uint8_t ch;
		ssize_t n = read(fd, &ch, 1);
		if (n < 0 && errno == EINTR)
			continue;
		if (n <= 0)
			return -1;
		if (have < bufsz)
			buf[have] = ch;
		have++;
		if (ch == '\n') {
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
	memset(io, 0, sizeof(*io));
	atomic_store(&io->c2s.rfd, -1);
	atomic_store(&io->c2s.wfd, -1);
	atomic_store(&io->s2c.rfd, -1);
	atomic_store(&io->s2c.wfd, -1);
	if (pipe_init(&io->c2s) < 0)
		return -1;
	if (pipe_init(&io->s2c) < 0) {
		pipe_free(&io->c2s);
		return -1;
	}
	return 0;
}

void
mock_io_free(struct mock_io_state *io)
{
	pipe_free(&io->c2s);
	pipe_free(&io->s2c);
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
	struct mock_io_state *io = cbdata;
	return pipe_write(&io->c2s, buf, bufsz);
}

int
mock_rx_client(uint8_t *buf, size_t bufsz, dssh_session sess, void *cbdata)
{
	(void)sess;
	struct mock_io_state *io = cbdata;
	return pipe_read(&io->s2c, buf, bufsz);
}

int
mock_rxline_client(uint8_t *buf, size_t bufsz, size_t *bytes_received,
    dssh_session sess, void *cbdata)
{
	(void)sess;
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
	struct mock_io_state *io = cbdata;
	return pipe_write(&io->s2c, buf, bufsz);
}

int
mock_rx_server(uint8_t *buf, size_t bufsz, dssh_session sess, void *cbdata)
{
	(void)sess;
	struct mock_io_state *io = cbdata;
	return pipe_read(&io->c2s, buf, bufsz);
}

int
mock_rxline_server(uint8_t *buf, size_t bufsz, size_t *bytes_received,
    dssh_session sess, void *cbdata)
{
	(void)sess;
	struct mock_io_state *io = cbdata;
	return pipe_readline(&io->c2s, buf, bufsz, bytes_received);
}

/* ================================================================
 * Single-sided helpers
 * ================================================================ */

size_t
mock_io_inject(struct mock_io_pipe *p, const uint8_t *data, size_t len)
{
	int fd = atomic_load(&p->wfd);
	if (fd < 0)
		return 0;
	ssize_t n = send(fd, data, len, MSG_NOSIGNAL);
	return n > 0 ? (size_t)n : 0;
}

size_t
mock_io_drain(struct mock_io_pipe *p, uint8_t *buf, size_t bufsz)
{
	int fd = atomic_load(&p->rfd);
	if (fd < 0)
		return 0;
	ssize_t n = recv(fd, buf, bufsz, MSG_DONTWAIT);
	return n > 0 ? (size_t)n : 0;
}

int
mock_extra_line_cb(uint8_t *buf, size_t bufsz, void *cbdata)
{
	(void)buf;
	(void)bufsz;
	(void)cbdata;
	return 0;
}
