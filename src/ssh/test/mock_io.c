/*
 * mock_io.c — Bidirectional mock I/O for DeuceSSH layer tests.
 */

#include <stdlib.h>
#include <string.h>

#include "mock_io.h"

/* ================================================================
 * Pipe operations (circular buffer with condvar)
 * ================================================================ */

static int
pipe_init(struct mock_io_pipe *p, size_t capacity)
{
	p->buf = malloc(capacity);
	if (p->buf == NULL)
		return -1;
	p->capacity = capacity;
	p->head = 0;
	p->tail = 0;
	p->used = 0;
	p->closed = false;
	if (mtx_init(&p->mtx, mtx_plain) != thrd_success) {
		free(p->buf);
		return -1;
	}
	if (cnd_init(&p->cnd) != thrd_success) {
		mtx_destroy(&p->mtx);
		free(p->buf);
		return -1;
	}
	return 0;
}

static void
pipe_free(struct mock_io_pipe *p)
{
	if (p->buf) {
		cnd_destroy(&p->cnd);
		mtx_destroy(&p->mtx);
		free(p->buf);
		p->buf = NULL;
	}
}

static void
pipe_close(struct mock_io_pipe *p)
{
	mtx_lock(&p->mtx);
	p->closed = true;
	cnd_broadcast(&p->cnd);
	mtx_unlock(&p->mtx);
}

/*
 * Write data into the pipe.  Blocks until all bytes are written
 * or the session is terminated.  Returns 0 on success.
 */
static int
pipe_write(struct mock_io_pipe *p, const uint8_t *data, size_t len,
    dssh_session sess)
{
	size_t written = 0;
	mtx_lock(&p->mtx);
	while (written < len) {
		while (p->used >= p->capacity && !p->closed) {
			if (sess && dssh_session_is_terminated(sess)) {
				mtx_unlock(&p->mtx);
				return -1;
			}
			cnd_wait(&p->cnd, &p->mtx);
		}
		if (p->closed) {
			mtx_unlock(&p->mtx);
			return -1;
		}
		size_t space = p->capacity - p->used;
		size_t chunk = len - written;
		if (chunk > space)
			chunk = space;
		for (size_t i = 0; i < chunk; i++) {
			p->buf[p->tail] = data[written + i];
			p->tail = (p->tail + 1) % p->capacity;
		}
		p->used += chunk;
		written += chunk;
		cnd_broadcast(&p->cnd);
	}
	mtx_unlock(&p->mtx);
	return 0;
}

/*
 * Read exactly 'len' bytes from the pipe.  Blocks until all bytes
 * are available or pipe is closed / session terminated.
 */
static int
pipe_read(struct mock_io_pipe *p, uint8_t *buf, size_t len,
    dssh_session sess)
{
	size_t have = 0;
	mtx_lock(&p->mtx);
	while (have < len) {
		while (p->used == 0 && !p->closed) {
			if (sess && dssh_session_is_terminated(sess)) {
				mtx_unlock(&p->mtx);
				return -1;
			}
			cnd_wait(&p->cnd, &p->mtx);
		}
		if (p->used == 0 && p->closed) {
			mtx_unlock(&p->mtx);
			return -1;
		}
		size_t chunk = len - have;
		if (chunk > p->used)
			chunk = p->used;
		for (size_t i = 0; i < chunk; i++) {
			buf[have + i] = p->buf[p->head];
			p->head = (p->head + 1) % p->capacity;
		}
		p->used -= chunk;
		have += chunk;
		cnd_broadcast(&p->cnd);
	}
	mtx_unlock(&p->mtx);
	return 0;
}

/*
 * Read a line (up to and including CR-LF) from the pipe.
 */
static int
pipe_readline(struct mock_io_pipe *p, uint8_t *buf, size_t bufsz,
    size_t *bytes_received, dssh_session sess)
{
	size_t have = 0;
	mtx_lock(&p->mtx);
	for (;;) {
		while (p->used == 0 && !p->closed) {
			if (sess && dssh_session_is_terminated(sess)) {
				mtx_unlock(&p->mtx);
				return -1;
			}
			cnd_wait(&p->cnd, &p->mtx);
		}
		if (p->used == 0 && p->closed) {
			mtx_unlock(&p->mtx);
			return -1;
		}
		uint8_t ch = p->buf[p->head];
		p->head = (p->head + 1) % p->capacity;
		p->used--;
		if (have < bufsz)
			buf[have] = ch;
		have++;
		if (ch == '\n') {
			*bytes_received = have;
			cnd_broadcast(&p->cnd);
			mtx_unlock(&p->mtx);
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
	memset(io, 0, sizeof(*io));
	if (capacity == 0)
		capacity = MOCK_IO_DEFAULT_CAPACITY;
	if (pipe_init(&io->c2s, capacity) < 0)
		return -1;
	if (pipe_init(&io->s2c, capacity) < 0) {
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

void mock_io_close_c2s(struct mock_io_state *io) { pipe_close(&io->c2s); }
void mock_io_close_s2c(struct mock_io_state *io) { pipe_close(&io->s2c); }

/* ================================================================
 * Client callbacks: TX→c2s, RX←s2c
 * ================================================================ */

int
mock_tx_client(uint8_t *buf, size_t bufsz, dssh_session sess, void *cbdata)
{
	struct mock_io_state *io = cbdata;
	if (io->inject_tx_error) {
		io->inject_tx_error = false;
		return -1;
	}
	int ret = pipe_write(&io->c2s, buf, bufsz, sess);
	if (ret == 0)
		io->total_c2s_bytes += bufsz;
	return ret;
}

int
mock_rx_client(uint8_t *buf, size_t bufsz, dssh_session sess, void *cbdata)
{
	struct mock_io_state *io = cbdata;
	if (io->inject_rx_error) {
		io->inject_rx_error = false;
		return -1;
	}
	return pipe_read(&io->s2c, buf, bufsz, sess);
}

int
mock_rxline_client(uint8_t *buf, size_t bufsz, size_t *bytes_received,
    dssh_session sess, void *cbdata)
{
	struct mock_io_state *io = cbdata;
	return pipe_readline(&io->s2c, buf, bufsz, bytes_received, sess);
}

/* ================================================================
 * Server callbacks: TX→s2c, RX←c2s
 * ================================================================ */

int
mock_tx_server(uint8_t *buf, size_t bufsz, dssh_session sess, void *cbdata)
{
	struct mock_io_state *io = cbdata;
	if (io->inject_tx_error) {
		io->inject_tx_error = false;
		return -1;
	}
	int ret = pipe_write(&io->s2c, buf, bufsz, sess);
	if (ret == 0)
		io->total_s2c_bytes += bufsz;
	return ret;
}

int
mock_rx_server(uint8_t *buf, size_t bufsz, dssh_session sess, void *cbdata)
{
	struct mock_io_state *io = cbdata;
	if (io->inject_rx_error) {
		io->inject_rx_error = false;
		return -1;
	}
	return pipe_read(&io->c2s, buf, bufsz, sess);
}

int
mock_rxline_server(uint8_t *buf, size_t bufsz, size_t *bytes_received,
    dssh_session sess, void *cbdata)
{
	struct mock_io_state *io = cbdata;
	return pipe_readline(&io->c2s, buf, bufsz, bytes_received, sess);
}

/* ================================================================
 * Single-sided helpers
 * ================================================================ */

size_t
mock_io_inject(struct mock_io_pipe *p, const uint8_t *data, size_t len)
{
	mtx_lock(&p->mtx);
	size_t written = 0;
	while (written < len && p->used < p->capacity) {
		p->buf[p->tail] = data[written];
		p->tail = (p->tail + 1) % p->capacity;
		p->used++;
		written++;
	}
	cnd_broadcast(&p->cnd);
	mtx_unlock(&p->mtx);
	return written;
}

size_t
mock_io_drain(struct mock_io_pipe *p, uint8_t *buf, size_t bufsz)
{
	mtx_lock(&p->mtx);
	size_t drained = 0;
	while (drained < bufsz && p->used > 0) {
		buf[drained] = p->buf[p->head];
		p->head = (p->head + 1) % p->capacity;
		p->used--;
		drained++;
	}
	cnd_broadcast(&p->cnd);
	mtx_unlock(&p->mtx);
	return drained;
}

int
mock_extra_line_cb(uint8_t *buf, size_t bufsz, void *cbdata)
{
	(void)buf;
	(void)bufsz;
	(void)cbdata;
	return 0;
}
