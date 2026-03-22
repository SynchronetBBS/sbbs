// RFC 4254: SSH Connection Protocol

#ifndef DEUCE_SSH_CONN_H
#define DEUCE_SSH_CONN_H

#include "deucessh.h"

#define SSH_MSG_GLOBAL_REQUEST            UINT8_C(80)
#define SSH_MSG_REQUEST_SUCCESS           UINT8_C(81)
#define SSH_MSG_REQUEST_FAILURE           UINT8_C(82)
#define SSH_MSG_CHANNEL_OPEN              UINT8_C(90)
#define SSH_MSG_CHANNEL_OPEN_CONFIRMATION UINT8_C(91)
#define SSH_MSG_CHANNEL_OPEN_FAILURE      UINT8_C(92)
#define SSH_MSG_CHANNEL_WINDOW_ADJUST     UINT8_C(93)
#define SSH_MSG_CHANNEL_DATA              UINT8_C(94)
#define SSH_MSG_CHANNEL_EXTENDED_DATA     UINT8_C(95)
#define SSH_MSG_CHANNEL_EOF               UINT8_C(96)
#define SSH_MSG_CHANNEL_CLOSE             UINT8_C(97)
#define SSH_MSG_CHANNEL_REQUEST           UINT8_C(98)
#define SSH_MSG_CHANNEL_SUCCESS           UINT8_C(99)
#define SSH_MSG_CHANNEL_FAILURE           UINT8_C(100)

/*
 * Channel state.  Two modes of operation:
 *
 * 1. Session channel (DEUCE_SSH_CHAN_SESSION) — stream-based, with
 *    stdout/stderr byte buffers and signal synchronization.
 *    Opened via session_open_shell() or session_open_exec().
 *
 * 2. Raw channel (DEUCE_SSH_CHAN_RAW) — message-based, with a
 *    message queue.  Opened via channel_open_subsystem().
 *
 * Both types are managed by the demux thread (started via
 * session_start) which handles all receiving, windowing, and
 * protocol-level concerns.  The application uses poll/read/write.
 */
typedef struct deuce_ssh_channel_s {
	/* Wire-level fields */
	uint32_t local_id;
	uint32_t remote_id;
	uint32_t local_window;
	uint32_t remote_window;
	uint32_t remote_max_packet;
	bool open;
	bool close_sent;
	bool close_received;
	bool eof_sent;
	bool eof_received;

	/* Buffered mode fields (chan_type > 0) */
	int chan_type;           /* 0=uninit, DEUCE_SSH_CHAN_SESSION, DEUCE_SSH_CHAN_RAW */
	mtx_t buf_mtx;          /* protects all buffer state below */
	cnd_t poll_cnd;          /* wakes poll() waiters */

	/* Consumed byte counters (for signal mark tracking) */
	size_t stdout_consumed;
	size_t stderr_consumed;

	/* Window replenishment threshold */
	uint32_t window_max;

	/* Exit status from peer (session channels) */
	uint32_t exit_code;
	bool exit_code_received;

	/* Channel request response (for send_channel_request_wait) */
	bool request_pending;
	bool request_responded;
	bool request_success;

	/* Setup mailbox (for session_accept_channel) */
	bool setup_mode;
	bool setup_ready;
	uint8_t setup_msg_type;
	uint8_t *setup_payload;
	size_t setup_payload_len;

	/* Window-change callback (server-side, post-setup) */
	void (*window_change_cb)(uint32_t cols, uint32_t rows,
	    uint32_t wpx, uint32_t hpx, void *cbdata);
	void *window_change_cbdata;

	/* Per-channel string buffers (avoids static storage) */
	char last_signal[32];
	char req_type[32];
	char req_data[1024];

	union {
		struct {
			struct deuce_ssh_bytebuf stdout_buf;
			struct deuce_ssh_bytebuf stderr_buf;
			struct deuce_ssh_signal_queue signals;
		} session;
		struct {
			struct deuce_ssh_msgqueue queue;
		} raw;
	} buf;
} *deuce_ssh_channel;

/* ================================================================
 * Demux thread and high-level channel API
 *
 * After handshake and auth, call deuce_ssh_session_start() to launch
 * the demux thread.  All subsequent channel I/O uses the buffered
 * API below.
 * ================================================================ */

/* Poll event flags */
#define DEUCE_SSH_POLL_READ    0x01  /* stdout data (or EOF) */
#define DEUCE_SSH_POLL_READEXT 0x02  /* stderr data (or EOF) */
#define DEUCE_SSH_POLL_WRITE   0x04  /* send window has space */
#define DEUCE_SSH_POLL_SIGNAL  0x08  /* signal ready (streams drained to mark) */

/* PTY request parameters (client -> server) */
struct deuce_ssh_pty_req {
	const char *term;
	uint32_t cols, rows;
	uint32_t wpx, hpx;
	const uint8_t *modes;
	size_t modes_len;
};

/* Incoming channel open (from session_accept) */
struct deuce_ssh_incoming_open;

/* Server-side session setup callbacks */
struct deuce_ssh_server_session_cbs {
	int (*pty_req)(const struct deuce_ssh_pty_req *pty, void *cbdata);
	int (*env)(const uint8_t *name, size_t name_len,
	    const uint8_t *value, size_t value_len, void *cbdata);
	void (*window_change)(uint32_t cols, uint32_t rows,
	    uint32_t wpx, uint32_t hpx, void *cbdata);
	void *cbdata;
};

/*
 * Start the demux thread.  Call after handshake and auth are complete.
 * Returns 0 on success.
 */
DEUCE_SSH_PUBLIC int deuce_ssh_session_start(deuce_ssh_session sess);

/*
 * Stop the demux thread and free all channel/demux resources.
 * Called automatically by deuce_ssh_session_cleanup() if the demux
 * was started.  Safe to call multiple times.
 */
DEUCE_SSH_PUBLIC void deuce_ssh_session_stop(deuce_ssh_session sess);

/*
 * Wait for an incoming CHANNEL_OPEN from the peer.
 * Returns 0 on success (inc is populated), negative on error.
 * timeout_ms: -1 = block, 0 = non-blocking.
 * The caller must accept, reject, or free the incoming open.
 */
DEUCE_SSH_PUBLIC int deuce_ssh_session_accept(deuce_ssh_session sess,
    struct deuce_ssh_incoming_open **inc, int timeout_ms);

/*
 * Reject an incoming channel open.  Sends CHANNEL_OPEN_FAILURE
 * and frees the incoming open struct.
 */
DEUCE_SSH_PUBLIC void deuce_ssh_session_reject(deuce_ssh_session sess,
    struct deuce_ssh_incoming_open *inc, uint32_t reason_code,
    const char *description);

/* --- Client-side channel open --- */

/*
 * Open a shell session with a PTY.
 * Sends CHANNEL_OPEN + pty-req + shell, waits for confirmations.
 * On success, ch is ready for session_read/write/poll.
 */
DEUCE_SSH_PUBLIC int deuce_ssh_session_open_shell(deuce_ssh_session sess,
    struct deuce_ssh_channel_s *ch,
    const struct deuce_ssh_pty_req *pty);

/*
 * Open an exec session (no PTY).
 * Sends CHANNEL_OPEN + exec, waits for confirmations.
 */
DEUCE_SSH_PUBLIC int deuce_ssh_session_open_exec(deuce_ssh_session sess,
    struct deuce_ssh_channel_s *ch,
    const char *command);

/*
 * Open a raw channel for a subsystem.
 * Sends CHANNEL_OPEN + subsystem, waits for confirmations.
 */
DEUCE_SSH_PUBLIC int deuce_ssh_channel_open_subsystem(deuce_ssh_session sess,
    struct deuce_ssh_channel_s *ch,
    const char *subsystem);

/* --- Server-side channel accept --- */

/*
 * Accept an incoming session channel.  Processes pty-req, env, and
 * waits for shell/exec/subsystem.  Fires callbacks as appropriate.
 * On return, *request_type is "shell", "exec", or "subsystem",
 * and *request_data is the command/subsystem name (for exec/subsystem).
 * ch is ready for session_read/write/poll.
 */
DEUCE_SSH_PUBLIC int deuce_ssh_session_accept_channel(deuce_ssh_session sess,
    struct deuce_ssh_incoming_open *inc,
    struct deuce_ssh_channel_s *ch,
    const struct deuce_ssh_server_session_cbs *cbs,
    const char **request_type, const char **request_data);

/*
 * Accept an incoming channel as a raw (message-based) channel.
 */
DEUCE_SSH_PUBLIC int deuce_ssh_channel_accept_raw(deuce_ssh_session sess,
    struct deuce_ssh_incoming_open *inc,
    struct deuce_ssh_channel_s *ch);

/* --- Session channel I/O (stream-based) --- */

DEUCE_SSH_PUBLIC int deuce_ssh_session_poll(deuce_ssh_session sess,
    struct deuce_ssh_channel_s *ch, int events, int timeout_ms);

DEUCE_SSH_PUBLIC ssize_t deuce_ssh_session_read(deuce_ssh_session sess,
    struct deuce_ssh_channel_s *ch, uint8_t *buf, size_t bufsz);

DEUCE_SSH_PUBLIC ssize_t deuce_ssh_session_read_ext(deuce_ssh_session sess,
    struct deuce_ssh_channel_s *ch, uint8_t *buf, size_t bufsz);

DEUCE_SSH_PUBLIC ssize_t deuce_ssh_session_write(deuce_ssh_session sess,
    struct deuce_ssh_channel_s *ch, const uint8_t *buf, size_t bufsz);

/*
 * Write to stderr (SSH_EXTENDED_DATA_STDERR).
 * Same semantics as session_write — returns bytes written (may be
 * short), 0 if window full, negative on error.
 */
DEUCE_SSH_PUBLIC ssize_t deuce_ssh_session_write_ext(deuce_ssh_session sess,
    struct deuce_ssh_channel_s *ch, const uint8_t *buf, size_t bufsz);

DEUCE_SSH_PUBLIC int deuce_ssh_session_read_signal(deuce_ssh_session sess,
    struct deuce_ssh_channel_s *ch, const char **signal_name);

/*
 * Graceful session close: sends exit-status + EOF + CLOSE.
 * Unregisters the channel and frees buffers.
 */
DEUCE_SSH_PUBLIC int deuce_ssh_session_close(deuce_ssh_session sess,
    struct deuce_ssh_channel_s *ch, uint32_t exit_code);

/* --- Raw channel I/O (message-based) --- */

DEUCE_SSH_PUBLIC int deuce_ssh_channel_poll(deuce_ssh_session sess,
    struct deuce_ssh_channel_s *ch, int events, int timeout_ms);

/*
 * Read a complete message.  Returns message length, 0 for EOF/closed.
 * If bufsz is too small, returns DEUCE_SSH_ERROR_TOOLONG (message
 * stays queued).  Pass buf=NULL, bufsz=0 to peek at the next
 * message size without consuming it.
 */
DEUCE_SSH_PUBLIC ssize_t deuce_ssh_channel_read(deuce_ssh_session sess,
    struct deuce_ssh_channel_s *ch, uint8_t *buf, size_t bufsz);

/*
 * Send a complete message.  Returns 0 on success.
 * Returns DEUCE_SSH_ERROR_TOOLONG if the message exceeds the remote
 * window or max packet size.  No partial sends.
 */
DEUCE_SSH_PUBLIC int deuce_ssh_channel_write(deuce_ssh_session sess,
    struct deuce_ssh_channel_s *ch, const uint8_t *buf, size_t len);

/*
 * Close a raw channel: sends EOF + CLOSE.
 * Unregisters the channel and frees buffers.
 */
DEUCE_SSH_PUBLIC int deuce_ssh_channel_close(deuce_ssh_session sess,
    struct deuce_ssh_channel_s *ch);

/* --- Window change (client-side, after open) --- */

/*
 * Send a signal to the peer (RFC 4254 s6.9).
 * signal_name is without the "SIG" prefix (e.g., "INT", "TERM", "USR1").
 * Sends CHANNEL_REQUEST "signal" with want_reply=false.
 */
DEUCE_SSH_PUBLIC int deuce_ssh_session_send_signal(deuce_ssh_session sess,
    struct deuce_ssh_channel_s *ch, const char *signal_name);

DEUCE_SSH_PUBLIC int deuce_ssh_session_send_window_change(deuce_ssh_session sess,
    struct deuce_ssh_channel_s *ch,
    uint32_t cols, uint32_t rows, uint32_t wpx, uint32_t hpx);

#endif
