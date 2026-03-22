// RFC 4254: SSH Connection Protocol

#ifndef DSSH_CONN_H
#define DSSH_CONN_H

#include "deucessh.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Opaque channel handle.  Created by open/accept, freed by close. */
typedef struct dssh_channel_s *dssh_channel;

/* Poll event flags */
#define DSSH_POLL_READ    0x01  /* stdout data (or EOF) */
#define DSSH_POLL_READEXT 0x02  /* stderr data (or EOF) */
#define DSSH_POLL_WRITE   0x04  /* send window has space */
#define DSSH_POLL_SIGNAL  0x08  /* signal ready (streams drained to mark) */

/* PTY request parameters (client -> server) */
struct dssh_pty_req {
	const char *term;
	uint32_t cols, rows;
	uint32_t wpx, hpx;
	const uint8_t *modes;
	size_t modes_len;
};

/* Incoming channel open (from session_accept) */
struct dssh_incoming_open;

/* Server-side session setup callbacks */
struct dssh_server_session_cbs {
	int (*pty_req)(const struct dssh_pty_req *pty, void *cbdata);
	int (*env)(const uint8_t *name, size_t name_len,
	    const uint8_t *value, size_t value_len, void *cbdata);
	void (*window_change)(uint32_t cols, uint32_t rows,
	    uint32_t wpx, uint32_t hpx, void *cbdata);
	void *cbdata;
};

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
DSSH_PUBLIC void dssh_session_reject(dssh_session sess,
    struct dssh_incoming_open *inc, uint32_t reason_code,
    const char *description);

/* --- Client-side channel open --- */

/*
 * Open a shell session with a PTY.
 * Returns the channel on success, NULL on failure.
 */
DSSH_PUBLIC dssh_channel dssh_session_open_shell(
    dssh_session sess, const struct dssh_pty_req *pty);

/*
 * Open an exec session (no PTY).
 * Returns the channel on success, NULL on failure.
 */
DSSH_PUBLIC dssh_channel dssh_session_open_exec(
    dssh_session sess, const char *command);

/*
 * Open a raw channel for a subsystem.
 * Returns the channel on success, NULL on failure.
 */
DSSH_PUBLIC dssh_channel dssh_channel_open_subsystem(
    dssh_session sess, const char *subsystem);

/* --- Server-side channel accept --- */

/*
 * Accept an incoming session channel.  Processes pty-req, env, and
 * waits for shell/exec/subsystem.
 * Returns the channel on success, NULL on failure.
 * *request_type is "shell", "exec", or "subsystem".
 * *request_data is the command/subsystem name (for exec/subsystem).
 */
DSSH_PUBLIC dssh_channel dssh_session_accept_channel(
    dssh_session sess, struct dssh_incoming_open *inc,
    const struct dssh_server_session_cbs *cbs,
    const char **request_type, const char **request_data);

/*
 * Accept an incoming channel as a raw (message-based) channel.
 * Returns the channel on success, NULL on failure.
 */
DSSH_PUBLIC dssh_channel dssh_channel_accept_raw(
    dssh_session sess, struct dssh_incoming_open *inc);

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
    dssh_channel ch);

/* --- Signals and window change --- */

DSSH_PUBLIC int dssh_session_send_signal(dssh_session sess,
    dssh_channel ch, const char *signal_name);

DSSH_PUBLIC int dssh_session_send_window_change(
    dssh_session sess, dssh_channel ch,
    uint32_t cols, uint32_t rows, uint32_t wpx, uint32_t hpx);

#ifdef __cplusplus
}
#endif

#endif
