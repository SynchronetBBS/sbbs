#ifndef DEUCE_SSH_H
#define DEUCE_SSH_H

#include <assert.h>

#ifdef __STDC_NO_ATOMICS__
static_assert(0, "stdatomic.h support required");
#endif

#ifdef __STDC_NO_THREADS__
static_assert(0, "threads.h support required");
#endif

#include <inttypes.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <threads.h>

#define DEUCE_SSH_ERROR_NONE           0
#define DEUCE_SSH_ERROR_PARSE         -1
#define DEUCE_SSH_ERROR_INVALID       -2
#define DEUCE_SSH_ERROR_ALLOC         -3
#define DEUCE_SSH_ERROR_INIT          -4
#define DEUCE_SSH_ERROR_TERMINATED    -5
#define DEUCE_SSH_ERROR_TOOLATE       -6
#define DEUCE_SSH_ERROR_TOOMANY       -7
#define DEUCE_SSH_ERROR_TOOLONG       -8
#define DEUCE_SSH_ERROR_MUST_BE_NULL  -9
#define DEUCE_SSH_ERROR_NOMORE       -10

typedef struct deuce_ssh_transport_state_s *deuce_ssh_transport_state;
typedef int (*deuce_ssh_transport_io_cb)(uint8_t *buf, size_t bufsz, atomic_bool *terminate, void *cbdata);
typedef int (*deuce_ssh_transport_rxline_cb)(uint8_t *buf, size_t bufsz, size_t *bytes_received, atomic_bool *terminate, void *cbdata);
typedef int (*deuce_ssh_transport_extra_line_cb)(uint8_t *buf, size_t bufsz, void *cbdata);

typedef struct deuce_ssh_session_s *deuce_ssh_session;

/*
 * Optional callback for SSH_MSG_DEBUG (RFC 4253 s11.3).
 * always_display: the sender's hint about whether to show the message.
 * message/message_len: the debug text (UTF-8, not NUL-terminated).
 */
/*
 * Optional callback for SSH_MSG_DEBUG (RFC 4253 s11.3).
 * always_display: the sender's hint about whether to show the message.
 * message/message_len: the debug text (UTF-8, not NUL-terminated).
 */
typedef void (*deuce_ssh_debug_cb)(bool always_display,
    const uint8_t *message, size_t message_len, void *cbdata);

/*
 * Optional callback for SSH_MSG_UNIMPLEMENTED (RFC 4253 s11.4).
 * rejected_seq: the sequence number of the packet the peer did not understand.
 * The application can correlate this with the sequence numbers returned by
 * deuce_ssh_transport_send_packet().
 */
typedef void (*deuce_ssh_unimplemented_cb)(uint32_t rejected_seq, void *cbdata);

#include "ssh-arch.h"
#include "ssh-trans.h"

struct deuce_ssh_session_s {
	/* Global */
	mtx_t mtx;
	atomic_bool initialized;
	atomic_bool terminate;

	void *tx_cbdata;
	void *rx_cbdata;
	void *rx_line_cbdata;
	void *extra_line_cbdata;

	deuce_ssh_debug_cb debug_cb;
	void *debug_cbdata;
	deuce_ssh_unimplemented_cb unimplemented_cb;
	void *unimplemented_cbdata;

	struct deuce_ssh_transport_state_s trans;
};

int deuce_ssh_session_init(deuce_ssh_session sess);
bool deuce_ssh_session_terminate(deuce_ssh_session sess);
void deuce_ssh_session_cleanup(deuce_ssh_session sess);

int deuce_ssh_transport_set_callbacks(deuce_ssh_transport_io_cb tx, deuce_ssh_transport_io_cb rx, deuce_ssh_transport_rxline_cb rx_line, deuce_ssh_transport_extra_line_cb extra_line_cb);

#endif
