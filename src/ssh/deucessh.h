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

/* Error codes returned by library functions.  Zero is success;
 * negative values are errors. */
#define DEUCE_SSH_ERROR_NONE           0
#define DEUCE_SSH_ERROR_PARSE         -1   /* Malformed packet or field */
#define DEUCE_SSH_ERROR_INVALID       -2   /* Valid parse but invalid value (MAC mismatch, bad signature) */
#define DEUCE_SSH_ERROR_ALLOC         -3   /* Memory allocation failure */
#define DEUCE_SSH_ERROR_INIT          -4   /* Initialization or crypto failure */
#define DEUCE_SSH_ERROR_TERMINATED    -5   /* Session terminated (peer disconnected) */
#define DEUCE_SSH_ERROR_TOOLATE       -6   /* Operation not allowed after session started */
#define DEUCE_SSH_ERROR_TOOMANY       -7   /* Too many registered algorithms */
#define DEUCE_SSH_ERROR_TOOLONG       -8   /* Data exceeds buffer or protocol limit */
#define DEUCE_SSH_ERROR_MUST_BE_NULL  -9   /* Linked list next pointer was not NULL */
#define DEUCE_SSH_ERROR_NOMORE       -10   /* No more items in name-list */
#define DEUCE_SSH_ERROR_REKEY_NEEDED -11   /* Must rekey before sending more packets */

typedef struct deuce_ssh_transport_state_s *deuce_ssh_transport_state;
typedef struct deuce_ssh_session_s *deuce_ssh_session;

/*
 * I/O callback: send or receive exactly bufsz bytes.
 * Must block until all bytes are transferred or *terminate becomes true.
 * Returns 0 on success, negative error code on failure.
 */
typedef int (*deuce_ssh_transport_io_cb)(uint8_t *buf, size_t bufsz, atomic_bool *terminate, void *cbdata);

/*
 * Line-oriented receive callback for version exchange.
 * Reads until CR-LF is found.  Sets *bytes_received to the number
 * of bytes including the CR-LF.  Returns 0 on success.
 */
typedef int (*deuce_ssh_transport_rxline_cb)(uint8_t *buf, size_t bufsz, size_t *bytes_received, atomic_bool *terminate, void *cbdata);

/*
 * Called for non-SSH lines received before the version string.
 * buf is NUL-terminated, bufsz is the length without NUL.
 * Return 0 to continue, negative to abort.
 */
typedef int (*deuce_ssh_transport_extra_line_cb)(uint8_t *buf, size_t bufsz, void *cbdata);

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
#include "ssh-chan.h"

/*
 * SSH session state.  Allocate one per connection.  Set trans.client
 * to true for client mode, false for server, before calling
 * deuce_ssh_session_init().  Set the callback fields (debug_cb, etc.)
 * and cbdata pointers before or after init as needed.
 */
struct deuce_ssh_session_s {
	mtx_t mtx;
	atomic_bool initialized;
	atomic_bool terminate;

	/* Per-session I/O callback data (passed to the global I/O callbacks) */
	void *tx_cbdata;
	void *rx_cbdata;
	void *rx_line_cbdata;
	void *extra_line_cbdata;

	/* Optional notification callbacks */
	deuce_ssh_debug_cb debug_cb;
	void *debug_cbdata;
	deuce_ssh_unimplemented_cb unimplemented_cb;
	void *unimplemented_cbdata;

	/* Auth banner callback (set before authentication) */
	void *banner_cb;      /* deuce_ssh_auth_banner_cb — void* to avoid circular include */
	void *banner_cbdata;

	/*
	 * Optional global request callback (RFC 4254 s4).
	 * If set, called for every SSH_MSG_GLOBAL_REQUEST.  The callback
	 * receives the request name, want_reply flag, and the request-
	 * specific data.  Return 0 to send REQUEST_SUCCESS (with no
	 * response data), positive to send REQUEST_SUCCESS (caller fills
	 * response), or negative to send REQUEST_FAILURE.  If the
	 * callback is NULL, REQUEST_FAILURE is sent automatically for
	 * requests with want_reply=true.
	 *
	 * Actual type: int (*)(const uint8_t *name, size_t name_len,
	 *     bool want_reply, const uint8_t *data, size_t data_len,
	 *     void *cbdata)
	 * Stored as void* to avoid circular include with ssh-conn.h.
	 */
	void *global_request_cb;
	void *global_request_cbdata;

	/* Transport layer state */
	struct deuce_ssh_transport_state_s trans;

	/* Demux thread and channel table (initialized by session_start).
	 * struct deuce_ssh_channel_s is defined in ssh-conn.h. */
	thrd_t demux_thread;
	atomic_bool demux_running;

	struct deuce_ssh_channel_s **channels;
	size_t channel_count;
	size_t channel_capacity;
	mtx_t channel_mtx;          /* protects channels array */

	/* Incoming channel open queue + condition variable */
	struct deuce_ssh_accept_queue accept_queue;
	mtx_t accept_mtx;
	cnd_t accept_cnd;

	/* Next auto-assigned local channel ID */
	uint32_t next_channel_id;
};

/*
 * Initialize a session.  The caller must have already set trans.client
 * and zeroed the struct (e.g., via memset).  Allocates packet buffers
 * and initializes mutexes.  Returns 0 on success.
 */
DEUCE_SSH_PUBLIC int deuce_ssh_session_init(deuce_ssh_session sess);

/*
 * Signal a session to terminate.  Sets terminate flag and returns true
 * if the session was initialized, false if already terminated.
 * Does not block — the session's I/O callbacks will see the flag.
 */
DEUCE_SSH_PUBLIC bool deuce_ssh_session_terminate(deuce_ssh_session sess);

/*
 * Clean up all session resources.  Calls terminate if needed, frees
 * all transport state, algorithm contexts, and packet buffers.
 * The session struct itself is NOT freed (caller owns it).
 */
DEUCE_SSH_PUBLIC void deuce_ssh_session_cleanup(deuce_ssh_session sess);

/*
 * Set the global I/O callbacks and optional extra-line callback.
 * Must be called before any session is initialized (before
 * deuce_ssh_session_init).  All sessions share the same callback
 * functions; per-session differentiation is via the cbdata pointers
 * in the session struct.
 *
 * tx: send callback — must send exactly bufsz bytes
 * rx: receive callback — must receive exactly bufsz bytes
 * rx_line: line-oriented receive for version exchange
 * extra_line_cb: called for non-SSH lines before version (may be NULL)
 */
DEUCE_SSH_PUBLIC int deuce_ssh_transport_set_callbacks(deuce_ssh_transport_io_cb tx, deuce_ssh_transport_io_cb rx, deuce_ssh_transport_rxline_cb rx_line, deuce_ssh_transport_extra_line_cb extra_line_cb);

#endif
