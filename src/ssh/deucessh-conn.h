// RFC 4254: SSH Connection Protocol

#ifndef DSSH_CONN_H
#define DSSH_CONN_H

#include "deucessh.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Opaque channel handle.  Created by open/accept, freed by close.
 *
 * Lifetime rules:
 *   - After dssh_session_close(ch) or dssh_channel_close(ch),
 *     the handle is freed and MUST NOT be used again.
 *   - After dssh_session_stop(), ALL channel handles are freed.
 *   - Passing a previously-closed handle is undefined behavior. */
typedef struct dssh_channel_s *dssh_channel;

/* Poll event flags */
#define DSSH_POLL_READ 0x01    /* stdout data (or EOF) */
#define DSSH_POLL_READEXT 0x02 /* stderr data (or EOF) */
#define DSSH_POLL_WRITE 0x04   /* send window has space */
#define DSSH_POLL_SIGNAL 0x08  /* signal ready (streams drained to mark) */

/* PTY request parameters (client -> server) */
struct dssh_pty_req {
	const char    *term;
	const uint8_t *modes;
	size_t         modes_len;
	uint32_t       cols, rows;
	uint32_t       wpx, hpx;
};

/* Incoming channel open (from session_accept) */
struct dssh_incoming_open;

/*
 * Channel request callback -- called for every SSH_MSG_CHANNEL_REQUEST
 * during session setup (pty-req, env, shell, exec, subsystem, etc.).
 *
 * type/type_len: request type string (not NUL-terminated)
 * want_reply:    peer expects SUCCESS/FAILURE
 * data/data_len: type-specific payload after the want_reply byte
 *
 * Return 0 to accept, negative to reject.
 * When a terminal request (shell/exec/subsystem) is accepted, setup
 * ends and the channel is returned to the caller.
 *
 * Use dssh_parse_uint32() from deucessh.h and the helpers below
 * to parse the type-specific data.
 */
typedef int (*dssh_channel_request_cb)(const char *type, size_t type_len,
    bool want_reply,
    const uint8_t *data, size_t data_len,
    void *cbdata);

/*
 * Session setup callbacks.
 * request_cb: called for every CHANNEL_REQUEST during setup.
 *             If NULL, all requests are accepted.
 * window_change: called for window-change requests AFTER setup
 *                completes (post-channel establishment).
 *                May be NULL.
 */
struct dssh_server_session_cbs {
	dssh_channel_request_cb request_cb;
	void                    (*window_change)(uint32_t cols, uint32_t rows,
	    uint32_t wpx, uint32_t hpx, void *cbdata);
	void                   *cbdata;
};

/* Parse helpers for well-known channel request payloads.
 * Call these from your request_cb to extract structured data.
 * All return 0 on success, negative on parse error. */

/* Parse pty-req data.  Pointers in pty->term point into data[]. */
DSSH_PUBLIC int dssh_parse_pty_req_data(const uint8_t *data, size_t data_len,
    struct dssh_pty_req *pty);

/* Parse env data.  *name and *value point into data[]. */
DSSH_PUBLIC int dssh_parse_env_data(const uint8_t *data, size_t data_len,
    const uint8_t **name, size_t *name_len,
    const uint8_t **value, size_t *value_len);

/* Parse exec data.  *command points into data[]. */
DSSH_PUBLIC int dssh_parse_exec_data(const uint8_t *data, size_t data_len,
    const uint8_t **command, size_t *command_len);

/* Parse subsystem data.  *name points into data[]. */
DSSH_PUBLIC int dssh_parse_subsystem_data(const uint8_t *data, size_t data_len,
    const uint8_t **name, size_t *name_len);

/*
 * Start the demux thread.  Call after handshake and auth are complete.
 */
DSSH_PUBLIC int dssh_session_start(dssh_session sess);

/*
 * Stop the demux thread and free all channel/demux resources.
 * Called automatically by dssh_session_cleanup().
 */
DSSH_PUBLIC void dssh_session_stop(dssh_session sess);

/*
 * Wait for an incoming CHANNEL_OPEN from the peer.
 * timeout_ms: -1 = block, 0 = non-blocking.
 */
DSSH_PUBLIC int dssh_session_accept(dssh_session sess,
    struct dssh_incoming_open **inc, int timeout_ms);

/*
 * Reject an incoming channel open.
 */
DSSH_PUBLIC int dssh_session_reject(dssh_session sess,
    struct dssh_incoming_open *inc, uint32_t reason_code,
    const char *description);

/* --- Client-side channel open --- */

/*
 * Open a shell session with a PTY.
 * Returns the channel on success, NULL on failure.
 */
DSSH_PUBLIC dssh_channel dssh_session_open_shell(dssh_session sess, const struct dssh_pty_req *pty);

/*
 * Open an exec session (no PTY).
 * Returns the channel on success, NULL on failure.
 */
DSSH_PUBLIC dssh_channel dssh_session_open_exec(dssh_session sess, const char *command);

/*
 * Open a raw channel for a subsystem.
 * Returns the channel on success, NULL on failure.
 */
DSSH_PUBLIC dssh_channel dssh_channel_open_subsystem(dssh_session sess, const char *subsystem);

/* --- Server-side channel accept --- */

/*
 * Accept an incoming session channel.  Processes pty-req, env, and
 * waits for shell/exec/subsystem.
 * Returns the channel on success, NULL on failure.
 * *request_type is "shell", "exec", or "subsystem".
 * *request_data is the command/subsystem name (for exec/subsystem).
 */
DSSH_PUBLIC dssh_channel dssh_session_accept_channel(dssh_session sess, struct dssh_incoming_open *inc,
    const struct dssh_server_session_cbs *cbs,
    const char **request_type, const char **request_data);

/*
 * Accept an incoming channel as a raw (message-based) channel.
 * Returns the channel on success, NULL on failure.
 */
DSSH_PUBLIC dssh_channel dssh_channel_accept_raw(dssh_session sess, struct dssh_incoming_open *inc);

/* --- Session channel I/O (stream-based) --- */
DSSH_PUBLIC int dssh_session_poll(dssh_session sess,
    dssh_channel ch, int events, int timeout_ms);
DSSH_PUBLIC int64_t dssh_session_read(dssh_session sess,
    dssh_channel ch, uint8_t *buf, size_t bufsz);
DSSH_PUBLIC int64_t dssh_session_read_ext(dssh_session sess,
    dssh_channel ch, uint8_t *buf, size_t bufsz);
DSSH_PUBLIC int64_t dssh_session_write(dssh_session sess,
    dssh_channel ch, const uint8_t *buf, size_t bufsz);
DSSH_PUBLIC int64_t dssh_session_write_ext(dssh_session sess,
    dssh_channel ch, const uint8_t *buf, size_t bufsz);
DSSH_PUBLIC int dssh_session_read_signal(dssh_session sess,
    dssh_channel ch, const char **signal_name);

/*
 * Graceful session close: sends exit-status + EOF + CLOSE.
 * Frees the channel.
 */
DSSH_PUBLIC int dssh_session_close(dssh_session sess,
    dssh_channel ch, uint32_t exit_code);

/* --- Raw channel I/O (message-based) --- */
DSSH_PUBLIC int dssh_channel_poll(dssh_session sess,
    dssh_channel ch, int events, int timeout_ms);
DSSH_PUBLIC int64_t dssh_channel_read(dssh_session sess,
    dssh_channel ch, uint8_t *buf, size_t bufsz);
DSSH_PUBLIC int dssh_channel_write(dssh_session sess,
    dssh_channel ch, const uint8_t *buf, size_t len);

/*
 * Close a raw channel: sends EOF + CLOSE.  Frees the channel.
 */
DSSH_PUBLIC int dssh_channel_close(dssh_session sess,
    dssh_channel                                ch);

/* --- Signals and window change --- */
DSSH_PUBLIC int dssh_session_send_signal(dssh_session sess,
    dssh_channel ch, const char *signal_name);
DSSH_PUBLIC int dssh_session_send_window_change(dssh_session sess, dssh_channel ch,
    uint32_t cols, uint32_t rows, uint32_t wpx, uint32_t hpx);

#ifdef __cplusplus
}
#endif

#endif // ifndef DSSH_CONN_H
