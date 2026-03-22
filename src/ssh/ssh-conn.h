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
 * 1. Low-level (conn_open_session / conn_recv / conn_send_data):
 *    Application drives send/recv directly.  No demux thread.
 *    Only the wire-level fields are used.
 *
 * 2. Buffered (session_open_shell / session_read / channel_poll):
 *    Demux thread feeds internal buffers.  Application uses
 *    poll/read/write.  chan_type determines buffer layout.
 *
 * The struct is the same for both modes; chan_type == 0 means
 * low-level mode (no internal buffers allocated).
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
	int chan_type;           /* 0=low-level, DEUCE_SSH_CHAN_SESSION, DEUCE_SSH_CHAN_RAW */
	mtx_t buf_mtx;          /* protects all buffer state below */
	cnd_t poll_cnd;          /* wakes poll() waiters */

	/* Consumed byte counters (for signal mark tracking) */
	size_t stdout_consumed;  /* total bytes read from stdout by app */
	size_t stderr_consumed;  /* total bytes read from stderr by app */

	/* Window replenishment threshold */
	uint32_t window_max;     /* target window size — library refills to this */

	/* Exit status from peer (session channels) */
	uint32_t exit_code;
	bool exit_code_received;

	/* Channel request response (for send_channel_request_wait) */
	bool request_pending;    /* a want_reply request is in flight */
	bool request_responded;  /* demux has delivered the response */
	bool request_success;    /* true = SUCCESS, false = FAILURE */

	/* Setup mailbox (for session_accept_channel) */
	bool setup_mode;         /* true during server-side setup */
	bool setup_ready;        /* demux has delivered a message */
	uint8_t setup_msg_type;
	uint8_t *setup_payload;  /* malloc'd copy, freed by consumer */
	size_t setup_payload_len;

	/* Window-change callback (server-side, post-setup) */
	void (*window_change_cb)(uint32_t cols, uint32_t rows,
	    uint32_t wpx, uint32_t hpx, void *cbdata);
	void *window_change_cbdata;

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
 * Client-side channel operations
 * ================================================================ */

/*
 * Open a "session" channel (RFC 4254 s6.1).
 * Sends CHANNEL_OPEN and waits for CHANNEL_OPEN_CONFIRMATION.
 * Populates ch->remote_id, remote_window, remote_max_packet.
 * Returns 0 on success.
 */
int deuce_ssh_conn_open_session(deuce_ssh_session sess,
    struct deuce_ssh_channel_s *ch, uint32_t local_id);

/*
 * Request remote command execution on an open channel (RFC 4254 s6.5).
 * Sends CHANNEL_REQUEST "exec" with want_reply=true and waits for
 * CHANNEL_SUCCESS.  Handles WINDOW_ADJUST received while waiting.
 * Returns 0 on success.
 */
int deuce_ssh_conn_request_exec(deuce_ssh_session sess,
    struct deuce_ssh_channel_s *ch, const char *command);

/*
 * Request a pseudo-terminal on a session channel (RFC 4254 s6.2).
 * Sends CHANNEL_REQUEST "pty-req" with want_reply=true.
 * term: TERM environment variable (e.g., "xterm-256color").
 * cols/rows: terminal dimensions in characters.
 * wpx/hpx: terminal dimensions in pixels (0 if unknown).
 * modes/modes_len: encoded terminal modes (NULL/0 for no modes).
 * Returns 0 on success.
 */
int deuce_ssh_conn_request_pty(deuce_ssh_session sess,
    struct deuce_ssh_channel_s *ch, const char *term,
    uint32_t cols, uint32_t rows, uint32_t wpx, uint32_t hpx,
    const uint8_t *modes, size_t modes_len);

/*
 * Request a shell on a session channel (RFC 4254 s6.5).
 * Sends CHANNEL_REQUEST "shell" with want_reply=true and waits for
 * CHANNEL_SUCCESS.  Returns 0 on success.
 */
int deuce_ssh_conn_request_shell(deuce_ssh_session sess,
    struct deuce_ssh_channel_s *ch);

/*
 * Request a subsystem on a session channel (RFC 4254 s6.5).
 * Sends CHANNEL_REQUEST "subsystem" with want_reply=true and waits
 * for CHANNEL_SUCCESS.  Returns 0 on success.
 */
int deuce_ssh_conn_request_subsystem(deuce_ssh_session sess,
    struct deuce_ssh_channel_s *ch, const char *subsystem);

/*
 * Send a window dimension change notification (RFC 4254 s6.7).
 * Sends CHANNEL_REQUEST "window-change" with want_reply=false.
 */
int deuce_ssh_conn_send_window_change(deuce_ssh_session sess,
    struct deuce_ssh_channel_s *ch,
    uint32_t cols, uint32_t rows, uint32_t wpx, uint32_t hpx);

/*
 * Set an environment variable on a session channel (RFC 4254 s6.4).
 * Sends CHANNEL_REQUEST "env" with want_reply=true.
 * Returns 0 on success (the server may reject it).
 */
int deuce_ssh_conn_request_env(deuce_ssh_session sess,
    struct deuce_ssh_channel_s *ch,
    const char *name, const char *value);

/* ================================================================
 * Server-side channel operations
 * ================================================================ */

/*
 * Wait for and accept a CHANNEL_OPEN from the client (server-side).
 * Receives CHANNEL_OPEN, sends CHANNEL_OPEN_CONFIRMATION.
 * Populates ch with both local and remote channel info.
 * channel_type_out (if not NULL) receives the channel type string
 * (points into session buffer — copy if needed).
 * Returns 0 on success.
 */
int deuce_ssh_conn_accept(deuce_ssh_session sess,
    struct deuce_ssh_channel_s *ch, uint32_t local_id,
    const char **channel_type_out);

/*
 * Reject a CHANNEL_OPEN with a failure response (server-side).
 * Call this instead of conn_accept() to refuse a channel.
 * recipient_channel is from the CHANNEL_OPEN message.
 */
int deuce_ssh_conn_reject(deuce_ssh_session sess,
    uint32_t recipient_channel, uint32_t reason_code,
    const char *description);

/*
 * Receive and parse a CHANNEL_REQUEST (server-side).
 * On return, *request_type points to the request type string
 * (not NUL-terminated, valid until next recv call),
 * *request_type_len is its length, *want_reply is the flag,
 * and *req_data, *req_data_len point to the type-specific data.
 * Returns 0 on success.  Returns DEUCE_SSH_ERROR_NOMORE if the
 * message was not a CHANNEL_REQUEST (msg_type set for caller).
 */
int deuce_ssh_conn_recv_request(deuce_ssh_session sess,
    struct deuce_ssh_channel_s *ch, uint8_t *msg_type,
    const uint8_t **request_type, size_t *request_type_len,
    bool *want_reply,
    const uint8_t **req_data, size_t *req_data_len);

/*
 * Send CHANNEL_SUCCESS for a channel request (server-side).
 */
int deuce_ssh_conn_send_success(deuce_ssh_session sess,
    struct deuce_ssh_channel_s *ch);

/*
 * Send CHANNEL_FAILURE for a channel request (server-side).
 */
int deuce_ssh_conn_send_failure(deuce_ssh_session sess,
    struct deuce_ssh_channel_s *ch);

/*
 * Parse a "pty-req" CHANNEL_REQUEST's type-specific data.
 * req_data/req_data_len come from deuce_ssh_conn_recv_request().
 * On success, *term points into the data buffer (not NUL-terminated),
 * dimensions are set, and *modes, *modes_len point to the raw
 * encoded terminal modes blob.
 */
int deuce_ssh_conn_parse_pty_req(const uint8_t *req_data, size_t req_data_len,
    const uint8_t **term, size_t *term_len,
    uint32_t *cols, uint32_t *rows, uint32_t *wpx, uint32_t *hpx,
    const uint8_t **modes, size_t *modes_len);

/*
 * Parse an "exec" CHANNEL_REQUEST's type-specific data.
 * On success, *command points into the data buffer.
 */
int deuce_ssh_conn_parse_exec(const uint8_t *req_data, size_t req_data_len,
    const uint8_t **command, size_t *command_len);

/* ================================================================
 * Common channel operations (client and server)
 * ================================================================ */

/*
 * Send channel data (RFC 4254 s5.2).
 * Checks that len does not exceed remote_window or remote_max_packet.
 * Decrements remote_window by len.  Returns 0 on success.
 */
int deuce_ssh_conn_send_data(deuce_ssh_session sess,
    struct deuce_ssh_channel_s *ch, const uint8_t *data, size_t len);

/*
 * Send extended channel data (RFC 4254 s5.2).
 * Typically used for stderr (data_type_code = 1).
 * Same window constraints as send_data.
 */
int deuce_ssh_conn_send_extended_data(deuce_ssh_session sess,
    struct deuce_ssh_channel_s *ch, uint32_t data_type_code,
    const uint8_t *data, size_t len);

/*
 * Send exit-status for a channel (RFC 4254 s6.10).
 * Sends CHANNEL_REQUEST "exit-status" with want_reply=false.
 * Call this before conn_send_eof() and conn_close().
 */
int deuce_ssh_conn_send_exit_status(deuce_ssh_session sess,
    struct deuce_ssh_channel_s *ch, uint32_t exit_code);

/*
 * Receive the next channel message.
 * On return, *msg_type is the SSH message type.  For CHANNEL_DATA
 * and CHANNEL_EXTENDED_DATA, *data and *data_len point to the
 * payload content (valid until the next recv call).  For
 * CHANNEL_WINDOW_ADJUST, the channel's remote_window is updated
 * automatically.  For other message types (EOF, CLOSE, etc.),
 * *data points to the raw payload.
 */
int deuce_ssh_conn_recv(deuce_ssh_session sess,
    struct deuce_ssh_channel_s *ch, uint8_t *msg_type,
    uint8_t **data, size_t *data_len);

/*
 * Send CHANNEL_WINDOW_ADJUST to increase the peer's send window.
 * Adds bytes to local_window.  Call this when local_window gets low.
 */
int deuce_ssh_conn_send_window_adjust(deuce_ssh_session sess,
    struct deuce_ssh_channel_s *ch, uint32_t bytes);

/*
 * Send CHANNEL_EOF for a channel (RFC 4254 s5.3).
 * Indicates no more data will be sent.  The channel remains open.
 * No-op if EOF was already sent.  Returns 0 on success.
 */
int deuce_ssh_conn_send_eof(deuce_ssh_session sess,
    struct deuce_ssh_channel_s *ch);

/*
 * Send CHANNEL_CLOSE for a channel (RFC 4254 s5.3).
 * No-op if CLOSE was already sent (prevents double-close).
 * Sets ch->close_sent to true and ch->open to false.
 *
 * When the application receives CHANNEL_CLOSE from the peer via
 * conn_recv(), it should clean up its resources and then call this
 * function to send the reciprocal CLOSE (the library will skip the
 * send if it already sent one).
 */
int deuce_ssh_conn_close(deuce_ssh_session sess,
    struct deuce_ssh_channel_s *ch);

/* ================================================================
 * Demux thread and high-level channel API
 *
 * After handshake and auth, call deuce_ssh_session_start() to launch
 * the demux thread.  All subsequent channel I/O uses the buffered
 * API below.  The low-level conn_recv() must NOT be used after
 * session_start().
 * ================================================================ */

/* Poll event flags */
#define DEUCE_SSH_POLL_READ    0x01  /* stdout data (or EOF) */
#define DEUCE_SSH_POLL_READEXT 0x02  /* stderr data (or EOF) */
#define DEUCE_SSH_POLL_WRITE   0x04  /* send window has space */
#define DEUCE_SSH_POLL_SIGNAL  0x08  /* signal ready (streams drained to mark) */

/* PTY request parameters (client → server) */
struct deuce_ssh_pty_req {
	const char *term;
	uint32_t cols, rows;
	uint32_t wpx, hpx;
	const uint8_t *modes;
	size_t modes_len;
};

/* Incoming channel open (from session_accept) */
struct deuce_ssh_incoming_open;  /* opaque — use fields via accessors or cast */

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
int deuce_ssh_session_start(deuce_ssh_session sess);

/*
 * Wait for an incoming CHANNEL_OPEN from the peer.
 * Returns 0 on success (inc is populated), negative on error.
 * timeout_ms: -1 = block, 0 = non-blocking.
 * The caller must accept, reject, or free the incoming open.
 */
int deuce_ssh_session_accept(deuce_ssh_session sess,
    struct deuce_ssh_incoming_open **inc, int timeout_ms);

/*
 * Free an incoming open without accepting or rejecting it.
 * Sends CHANNEL_OPEN_FAILURE to the peer.
 */
void deuce_ssh_session_reject(deuce_ssh_session sess,
    struct deuce_ssh_incoming_open *inc, uint32_t reason_code,
    const char *description);

/* --- Client-side session channel open --- */

/*
 * Open a shell session with a PTY.
 * Sends CHANNEL_OPEN + pty-req + shell, waits for confirmations.
 * On success, ch is ready for session_read/write/poll.
 */
int deuce_ssh_session_open_shell(deuce_ssh_session sess,
    struct deuce_ssh_channel_s *ch,
    const struct deuce_ssh_pty_req *pty);

/*
 * Open an exec session (no PTY).
 * Sends CHANNEL_OPEN + exec, waits for confirmations.
 */
int deuce_ssh_session_open_exec(deuce_ssh_session sess,
    struct deuce_ssh_channel_s *ch,
    const char *command);

/*
 * Open a raw channel for a subsystem.
 * Sends CHANNEL_OPEN + subsystem, waits for confirmations.
 */
int deuce_ssh_channel_open_subsystem(deuce_ssh_session sess,
    struct deuce_ssh_channel_s *ch,
    const char *subsystem);

/* --- Server-side session channel accept --- */

/*
 * Accept an incoming session channel.  Processes pty-req, env, and
 * waits for shell/exec/subsystem.  Fires callbacks as appropriate.
 * On return, *request_type is "shell", "exec", or "subsystem",
 * and *request_data is the command/subsystem name (for exec/subsystem).
 * ch is ready for session_read/write/poll.
 */
int deuce_ssh_session_accept_channel(deuce_ssh_session sess,
    struct deuce_ssh_incoming_open *inc,
    struct deuce_ssh_channel_s *ch,
    const struct deuce_ssh_server_session_cbs *cbs,
    const char **request_type, const char **request_data);

/*
 * Accept an incoming channel as a raw (message-based) channel.
 */
int deuce_ssh_channel_accept_raw(deuce_ssh_session sess,
    struct deuce_ssh_incoming_open *inc,
    struct deuce_ssh_channel_s *ch);

/* --- Session channel I/O (stream-based) --- */

int deuce_ssh_session_poll(deuce_ssh_session sess,
    struct deuce_ssh_channel_s *ch, int events, int timeout_ms);

ssize_t deuce_ssh_session_read(deuce_ssh_session sess,
    struct deuce_ssh_channel_s *ch, uint8_t *buf, size_t bufsz);

ssize_t deuce_ssh_session_read_ext(deuce_ssh_session sess,
    struct deuce_ssh_channel_s *ch, uint8_t *buf, size_t bufsz);

ssize_t deuce_ssh_session_write(deuce_ssh_session sess,
    struct deuce_ssh_channel_s *ch, const uint8_t *buf, size_t bufsz);

int deuce_ssh_session_read_signal(deuce_ssh_session sess,
    struct deuce_ssh_channel_s *ch, const char **signal_name);

int deuce_ssh_session_close(deuce_ssh_session sess,
    struct deuce_ssh_channel_s *ch, uint32_t exit_code);

/* --- Raw channel I/O (message-based) --- */

int deuce_ssh_channel_poll(deuce_ssh_session sess,
    struct deuce_ssh_channel_s *ch, int events, int timeout_ms);

ssize_t deuce_ssh_channel_read(deuce_ssh_session sess,
    struct deuce_ssh_channel_s *ch, uint8_t *buf, size_t bufsz);

int deuce_ssh_channel_write(deuce_ssh_session sess,
    struct deuce_ssh_channel_s *ch, const uint8_t *buf, size_t len);

int deuce_ssh_channel_close(deuce_ssh_session sess,
    struct deuce_ssh_channel_s *ch);

/* --- Window change (client-side, after open) --- */

int deuce_ssh_session_send_window_change(deuce_ssh_session sess,
    struct deuce_ssh_channel_s *ch,
    uint32_t cols, uint32_t rows, uint32_t wpx, uint32_t hpx);

#endif
