/*
 * mock_io.h — Bidirectional mock I/O for DeuceSSH layer tests.
 *
 * Provides circular buffers with C11 mutex/condvar synchronization
 * that implement the dssh_transport I/O callbacks.  Supports both
 * two-sided testing (client+server in separate threads) and
 * single-sided testing (inject/drain bytes manually).
 */

#ifndef DSSH_MOCK_IO_H
#define DSSH_MOCK_IO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <threads.h>

#include "deucessh.h"

#define MOCK_IO_DEFAULT_CAPACITY (256 * 1024)

struct mock_io_pipe {
	uint8_t *buf;
	size_t capacity;
	size_t head;     /* next read position */
	size_t tail;     /* next write position */
	size_t used;
	mtx_t mtx;
	cnd_t cnd;
	bool closed;     /* writer has closed this pipe */
};

struct mock_io_state {
	struct mock_io_pipe c2s;  /* client writes, server reads */
	struct mock_io_pipe s2c;  /* server writes, client reads */

	/* Fault injection */
	bool inject_tx_error;     /* next tx returns -1 */
	bool inject_rx_error;     /* next rx returns -1 */

	/* Statistics */
	size_t total_c2s_bytes;
	size_t total_s2c_bytes;
};

/*
 * Initialize mock I/O state.  Each pipe gets 'capacity' bytes
 * (pass 0 for MOCK_IO_DEFAULT_CAPACITY).  Returns 0 on success.
 */
int mock_io_init(struct mock_io_state *io, size_t capacity);

/*
 * Free all resources.
 */
void mock_io_free(struct mock_io_state *io);

/*
 * Close one direction (unblocks any reader waiting on that pipe).
 */
void mock_io_close_c2s(struct mock_io_state *io);
void mock_io_close_s2c(struct mock_io_state *io);

/*
 * DeuceSSH I/O callbacks for the client side.
 * cbdata must point to struct mock_io_state.
 */
int mock_tx_client(uint8_t *buf, size_t bufsz,
    dssh_session sess, void *cbdata);
int mock_rx_client(uint8_t *buf, size_t bufsz,
    dssh_session sess, void *cbdata);
int mock_rxline_client(uint8_t *buf, size_t bufsz,
    size_t *bytes_received, dssh_session sess, void *cbdata);

/*
 * DeuceSSH I/O callbacks for the server side.
 * cbdata must point to struct mock_io_state.
 */
int mock_tx_server(uint8_t *buf, size_t bufsz,
    dssh_session sess, void *cbdata);
int mock_rx_server(uint8_t *buf, size_t bufsz,
    dssh_session sess, void *cbdata);
int mock_rxline_server(uint8_t *buf, size_t bufsz,
    size_t *bytes_received, dssh_session sess, void *cbdata);

/*
 * Single-sided helpers.
 * Inject bytes into a pipe (for feeding crafted data to the reader).
 * Drain bytes from a pipe (for inspecting what the writer sent).
 */
size_t mock_io_inject(struct mock_io_pipe *pipe, const uint8_t *data, size_t len);
size_t mock_io_drain(struct mock_io_pipe *pipe, uint8_t *buf, size_t bufsz);

/*
 * Extra-line callback (no-op, returns 0).
 */
int mock_extra_line_cb(uint8_t *buf, size_t bufsz, void *cbdata);

#endif
