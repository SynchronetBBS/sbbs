/*
 * mock_io.h — Bidirectional mock I/O for DeuceSSH layer tests.
 *
 * Uses socketpair() for natural blocking/unblocking behavior.
 * When one side closes, the other gets an error immediately —
 * no timed waits or condvar signaling needed.
 */

#ifndef DSSH_MOCK_IO_H
#define DSSH_MOCK_IO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "deucessh.h"

struct mock_io_pipe {
	int rfd;    /* read end */
	int wfd;    /* write end */
};

struct mock_io_state {
	struct mock_io_pipe c2s;  /* client writes, server reads */
	struct mock_io_pipe s2c;  /* server writes, client reads */
};

/*
 * Initialize mock I/O state.  Creates two socketpairs.
 * The capacity parameter is ignored (kept for API compat).
 * Returns 0 on success.
 */
int mock_io_init(struct mock_io_state *io, size_t capacity);

/*
 * Free all resources (close any remaining fds).
 */
void mock_io_free(struct mock_io_state *io);

/*
 * Close one direction (unblocks any reader on that pipe).
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
 * Inject bytes into a pipe's write end.
 * Drain bytes from a pipe's read end (non-blocking).
 */
size_t mock_io_inject(struct mock_io_pipe *pipe, const uint8_t *data, size_t len);
size_t mock_io_drain(struct mock_io_pipe *pipe, uint8_t *buf, size_t bufsz);

/*
 * Extra-line callback (no-op, returns 0).
 */
int mock_extra_line_cb(uint8_t *buf, size_t bufsz, void *cbdata);

#endif
