/*
 * mock_io.h -- Bidirectional mock I/O for DeuceSSH layer tests.
 *
 * Uses native connected sockets for blocking/unblocking behavior.
 * When one side closes, the other gets an error immediately --
 * no timed waits or condvar signaling needed.
 */

#ifndef DSSH_MOCK_IO_H
#define DSSH_MOCK_IO_H

#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "deucessh.h"

#ifdef _WIN32
typedef uintptr_t mock_io_handle;
#define MOCK_IO_INVALID UINTPTR_MAX
#else
typedef int mock_io_handle;
#define MOCK_IO_INVALID (-1)
#endif

struct mock_io_pipe {
	_Atomic mock_io_handle rfd;    /* read end */
	_Atomic mock_io_handle wfd;    /* write end */
	atomic_bool read_closed;
	atomic_bool write_closed;
};

struct mock_io_state {
	struct mock_io_pipe c2s;  /* client writes, server reads */
	struct mock_io_pipe s2c;  /* server writes, client reads */
};

/*
 * Initialize mock I/O state.  Creates two connected socket pairs.
 * The capacity parameter is ignored; native sockets supply their own buffering.
 * Returns 0 on success.
 */
int mock_io_init(struct mock_io_state *io, size_t capacity);

/*
 * Free resources after every I/O worker has joined. Close each owned fd once;
 * an unexpected native release failure aborts this test process.
 */
void mock_io_free(struct mock_io_state *io);

/*
 * Shut down one direction (unblocks any reader on that pipe).  Descriptors
 * remain allocated until mock_io_free() so concurrent I/O cannot race reuse.
 * Repeated shutdown is idempotent. Unexpected native shutdown failure aborts;
 * ENOTCONN means the stream is already disconnected.
 */
void mock_io_close_c2s(struct mock_io_state *io);
void mock_io_close_c2s_write(struct mock_io_state *io);
void mock_io_close_s2c(struct mock_io_state *io);
void mock_io_close_s2c_write(struct mock_io_state *io);

/*
 * DeuceSSH I/O callbacks for the client side.
 * cbdata must point to struct mock_io_state. Line readers delimit on LF;
 * protocol grammar is checked separately. On overflow they stop after the first
 * excess byte and report that bounded count; remaining line bytes stay queued.
 */
int mock_tx_client(uint8_t *buf, size_t bufsz,
    dssh_session sess, void *cbdata);
int mock_rx_client(uint8_t *buf, size_t bufsz,
    dssh_session sess, void *cbdata);
int mock_rxline_client(uint8_t *buf, size_t bufsz,
    size_t *bytes_received, dssh_session sess, void *cbdata);

/*
 * DeuceSSH I/O callbacks for the server side.
 * cbdata must point to struct mock_io_state. Line readers delimit on LF;
 * protocol grammar is checked separately. On overflow they stop after the first
 * excess byte and report that bounded count; remaining line bytes stay queued.
 */
int mock_tx_server(uint8_t *buf, size_t bufsz,
    dssh_session sess, void *cbdata);
int mock_rx_server(uint8_t *buf, size_t bufsz,
    dssh_session sess, void *cbdata);
int mock_rxline_server(uint8_t *buf, size_t bufsz,
    size_t *bytes_received, dssh_session sess, void *cbdata);

/*
 * Single-sided helpers.
 * Inject bytes into a pipe's write end.
 * Drain bytes from a pipe's read end (non-blocking).
 */
size_t mock_io_inject(struct mock_io_pipe *pipe, const uint8_t *data, size_t len);
size_t mock_io_drain(struct mock_io_pipe *pipe, uint8_t *buf, size_t bufsz);

/* One-shot, thread-local observation after a native read returns EAGAIN or
 * EWOULDBLOCK, before the mock checks its close flags. NULL disables it. */
void mock_io_set_read_retry_hook(void (*callback)(void *), void *opaque);

/*
 * Extra-line callback (no-op, returns 0).
 */
int mock_extra_line_cb(uint8_t *buf, size_t bufsz, void *cbdata);

#endif
