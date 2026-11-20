// RFC 4254: SSH Connection Protocol

#ifndef DSSH_CONN_H
#define DSSH_CONN_H

#include "deucessh.h"
#include "deucessh-result.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Opaque channel handle.  Created by open/accept, freed by close.
*
* Lifetime rules:
*   - After an ordinary dssh_chan_close call, the handle MUST NOT be used
*     again, regardless of its return value or exit_code. Calls forbidden by
*     callback/TX context do not change ownership. Setup callback cancellation
*     defers physical retirement until the callback returns (see below).
*   - After dssh_session_stop() successfully joins the worker, all channel
*     handles are freed (see the shutdown conditions below).
*   - Passing a previously-closed handle is undefined behavior. */
typedef struct dssh_channel_s *dssh_channel;

/* Poll event flags */
#define DSSH_POLL_READ    0x01 /* stdout data, EOF, or retained read error */
#define DSSH_POLL_READEXT 0x02 /* stderr data, EOF, or retained read error */
#define DSSH_POLL_WRITE   0x04 /* send window has space */
/* 0x08 reserved */

/*
 * Start the demux thread.  Call after handshake and auth are complete.
 */
DSSH_PUBLIC int dssh_session_start(dssh_session sess) DSSH_ABI(dssh_session_start);

/*
 * Stop the demux thread and free all channel/demux resources.
 * Called automatically by dssh_session_cleanup(). Stop application threads
 * using the session or channels first, and serialize lifecycle calls.
 * A failed join of the valid joinable demux is an unrecoverable internal
 * ownership failure and calls abort(). Calls from the session's
 * callbacks or a thread owning its zero-copy TX reservation only request
 * termination; call stop again outside that context to release resources.
 * Callback restrictions documented in deucessh.h still apply.
 */
DSSH_PUBLIC void dssh_session_stop(dssh_session sess) DSSH_ABI(dssh_session_stop);

/* ================================================================
 * Channel types and builder functions.
 * See README.md for complete usage.
 * ================================================================ */

/* Channel type: one terminal request per channel (RFC 4254 s6.5) */
enum dssh_chan_type {
	DSSH_CHAN_SHELL,
	DSSH_CHAN_EXEC,
	DSSH_CHAN_SUBSYSTEM
};

/* Environment variable name/value pair */
struct dssh_chan_env {
	char *name;
	char *value;
};

/* Terminal mode opcode/value pair (RFC 4254 s8) */
struct dssh_chan_mode_entry {
	/* 4-byte */
	uint32_t value;

	/* 1-byte */
	uint8_t opcode;
};

/* Params flag bits */
#define DSSH_PARAM_HAS_PTY            0x01
/*
 * Accept CHANNEL_DATA / CHANNEL_EXTENDED_DATA that arrives before
 * the terminal-request response, even though the local window is
 * still 0.  Workaround for Cryptlib-based servers that don't wait
 * for CRYPT_SESSINFO_SSH_CHANNEL_TYPE to be set before sending
 * (such as Mystic BBS).  Only valid with DSSH_CHAN_SHELL or
 * DSSH_CHAN_EXEC; rejected with DSSH_ERROR_INVALID (NULL channel)
 * for DSSH_CHAN_SUBSYSTEM.  Do not enable unless you actually need
 * it.
 */
#define DSSH_PARAM_ACCEPT_EARLY_DATA  0x02

/*
 * Channel parameters — populated via builder API, consumed at open time.
 * Caller-created builder storage is released with dssh_chan_params_free()
 * after open returns. Server setup callback parameters are borrowed;
 * applications must not free them.
 */
struct dssh_chan_params {
	/* Pointers */
	char                        *command;   /* DSSH_CHAN_EXEC (copied) */
	char                        *subsystem; /* DSSH_CHAN_SUBSYSTEM (copied) */
	struct dssh_chan_env        *env;       /* malloc'd array (copied) */
	struct dssh_chan_mode_entry *modes;     /* malloc'd array */
						/* Pointer-sized */
	size_t env_count;
	size_t env_capacity;
	size_t mode_count;
	size_t mode_capacity;

	/* 4-byte */
	enum dssh_chan_type type;
	uint32_t            flags;
	uint32_t            max_window; /* 0 = library default (2 MiB) */
	uint32_t            cols, rows;
	uint32_t            wpx, hpx;

	/* Char array — dynamically allocated, freed by dssh_chan_params_free */
	char *term;
};

/* Params builder API.  All strings are copied in.
 * dssh_chan_params_free() releases heap storage. */
DSSH_PUBLIC int  dssh_chan_params_init(struct dssh_chan_params *p, enum dssh_chan_type type) DSSH_ABI(dssh_chan_params_init);
DSSH_PUBLIC void dssh_chan_params_free(struct dssh_chan_params *p) DSSH_ABI(dssh_chan_params_free);
DSSH_PUBLIC int  dssh_chan_params_set_max_window(struct dssh_chan_params *p, uint32_t max_window) DSSH_ABI(dssh_chan_params_set_max_window);
DSSH_PUBLIC int  dssh_chan_params_set_pty(struct dssh_chan_params *p, bool enable) DSSH_ABI(dssh_chan_params_set_pty);
DSSH_PUBLIC int  dssh_chan_params_set_accept_early_data(struct dssh_chan_params *p, bool enable) DSSH_ABI(dssh_chan_params_set_accept_early_data);
DSSH_PUBLIC int  dssh_chan_params_set_term(struct dssh_chan_params *p, const char *term) DSSH_ABI(dssh_chan_params_set_term);
DSSH_PUBLIC int  dssh_chan_params_set_size(struct dssh_chan_params *p, uint32_t cols, uint32_t rows, uint32_t wpx,
     uint32_t hpx) DSSH_ABI(dssh_chan_params_set_size);
DSSH_PUBLIC int  dssh_chan_params_set_mode(struct dssh_chan_params *p, uint8_t opcode, uint32_t value) DSSH_ABI(dssh_chan_params_set_mode);
DSSH_PUBLIC int  dssh_chan_params_set_command(struct dssh_chan_params *p, const char *command) DSSH_ABI(dssh_chan_params_set_command);
DSSH_PUBLIC int  dssh_chan_params_set_subsystem(struct dssh_chan_params *p, const char *name) DSSH_ABI(dssh_chan_params_set_subsystem);
/* Env requests are sent with want_reply=false; peer acceptance is not reported. */
DSSH_PUBLIC int  dssh_chan_params_add_env(struct dssh_chan_params *p, const char *name, const char *value) DSSH_ABI(dssh_chan_params_add_env);

/* Event poll flag (in addition to DSSH_POLL_READ etc.) */
#define DSSH_POLL_EVENT 0x10

/* Event types */
#define DSSH_EVENT_SIGNAL        1
#define DSSH_EVENT_WINDOW_CHANGE 2
#define DSSH_EVENT_EOF           3
#define DSSH_EVENT_CLOSE         4
#define DSSH_EVENT_EXIT_STATUS   5
#define DSSH_EVENT_EXIT_SIGNAL   6

/* Event struct — returned by dssh_chan_read_event().
 * Text fields are borrowed byte views of the complete accepted wire fields,
 * including embedded zero bytes. No terminator or normalization is promised.
 * Empty views have NULL data and zero len. Views remain valid until the next
 * dssh_chan_read_event() or dssh_chan_poll() on the same channel. In an event
 * callback, the event and its views are valid only for that invocation.
 * Copy bytes needed beyond that interval; never free/release these views. */
struct dssh_chan_event {
	/* Pointer-sized */
	size_t stdout_pos; /* callback: cumulative received; poll: unread bytes */
	size_t stderr_pos; /* corresponding stderr position */
			   /* 4-byte */
	int type;

	/* Union (mixed sizes, largest member determines layout) */
	union {
		struct {
			struct dssh_bytes name;
		} signal;

		struct {
			uint32_t cols, rows, wpx, hpx;
		} window_change;

		struct {
			uint32_t exit_code;
		} exit_status;

		struct {
			struct dssh_bytes signal_name;
			bool        core_dumped;
			struct dssh_bytes error_message;
			struct dssh_bytes language;
		} exit_signal;
	};
};

/* BREAK action request. All fields are borrowed for the callback. Duration
 * is the exact peer uint32 value; positions count prior received stream bytes,
 * saturating at SIZE_MAX. want_reply is the peer flag, normalized to bool. */
struct dssh_chan_break_request {
	size_t stdout_pos, stderr_pos;
	uint32_t length;
	bool want_reply;
};

/* After channel setup, runs on the demux thread with no library mutex held.
 * Return true iff a
 * BREAK of any kind was performed; false iff none was performed. Reply, when
 * requested, follows return. No handler means no action and a failure reply.
 * This replaces BREAK event delivery; no general event is published.
 * Keep the borrowed channel/request only for this invocation. Follow event
 * callback restrictions: no channel I/O/poll/TX/close/open/accept. Waiting here
 * pauses receive progress for every channel in this session. */
typedef bool (*dssh_chan_break_cb)(dssh_channel ch,
    const struct dssh_chan_break_request *request, void *cbdata);

/* ZC data callback — fires on demux thread for each inbound
 * CHANNEL_DATA / EXTENDED_DATA.  Returns bytes consumed for
 * WINDOW_ADJUST. stream is 0 for ordinary data and 1 for stderr.
 * Unsupported nonzero extended data types are discarded with their window
 * credit returned; they are not passed to the callback.
 *
 * `len` may be smaller than the wire packet's payload (or zero)
 * either because the library truncated to what the advertised
 * window allows when the local window was exhausted, or because
 * the peer sent a zero-byte data message (legal per RFC 4254).
 * Callbacks MUST handle len == 0 as a no-op; it is NOT EOF.  EOF
 * is delivered separately as DSSH_EVENT_EOF on the event queue.
 * A return value greater than len is clamped to len, so a callback
 * cannot grant the peer more receive-window credit than it consumed.
 * The exception is a channel opened with DSSH_PARAM_ACCEPT_EARLY_
 * DATA, which delivers full pre-setup wire payloads without
 * window truncation. */
typedef uint32_t (*dssh_chan_zc_cb)(dssh_channel ch, int stream, const uint8_t *data, size_t len, void *cbdata);

/* Event callback — receives events on the demux thread in wire order.
 * An event is consumed by this callback or queued for poll/read_event, never
 * both. Positions count bytes delivered before the event, saturating at
 * SIZE_MAX. The event and strings are valid only until this callback returns.
 * No library mutex is held. Do not call channel I/O, poll/read_event, open,
 * accept or TX functions here; enqueue application work for another thread. */
typedef void (*dssh_chan_event_cb)(dssh_channel ch, const struct dssh_chan_event *event, void *cbdata);

/* Server accept: result struct filled by terminal request callback */
struct dssh_chan_accept_result {
	/* Pointers */
	dssh_chan_zc_cb zc_cb; /* NULL = stream mode */
	void           *zc_cbdata;

	/* 4-byte */
	uint32_t max_window; /* 0 = default */
};

/* ---- Channel I/O API ---- */

/* Open a stream-mode channel.  Performs the full setup sequence:
 * CHANNEL_OPEN, env, pty-req, shell/exec/subsystem, WINDOW_ADJUST.
 * Returns the channel on success, NULL on failure. */
DSSH_PUBLIC dssh_channel dssh_chan_open(dssh_session sess, const struct dssh_chan_params *params) DSSH_ABI(dssh_chan_open);

/* Stream I/O.  stream: 0=stdout/stdin, 1=stderr.
 *
 * Both read and write follow non-blocking POSIX semantics:
 *
 *   read returns bytes read (>0), 0 on EOF (peer sent CHANNEL_EOF /
 *   CHANNEL_CLOSE, or the session is terminated and the bytebuf is
 *   drained), DSSH_ERROR_NOMORE when the bytebuf is empty but no
 *   EOF has arrived (would-block analog of -1/EAGAIN), and other
 *   negative DSSH_ERROR_* on error.
 *   Once bytes have been delivered, read returns their count. A later
 *   receive-credit error is retained for the next non-NULL read with
 *   nonzero capacity on either stream; that call reports the error once
 *   without copying or consuming data. Availability peeks do not consume
 *   a retained error. Requested read directions poll ready until it is read.
 *   A subsequent positive-capacity read retries retained receive credit,
 *   including when the stream buffer is empty. Release another session's
 *   same-thread TX reservation before reading data; peeks remain available.
 *
 *   write returns bytes sent (>0), 0 only when bufsz == 0,
 *   DSSH_ERROR_NOMORE when the remote window is full (would-block
 *   analog of -1/EAGAIN), and other negative DSSH_ERROR_* on error.
 *
 * Readiness is a snapshot. Another caller may consume buffered data
 * or send-window credit after poll returns, so the next read/write
 * must still handle DSSH_ERROR_NOMORE.
 *
 * Peek: dssh_chan_read(ch, stream, NULL, 0) returns available bytes
 * (always >= 0; never NOMORE). */
DSSH_PUBLIC int64_t dssh_chan_read(dssh_channel ch, int stream, uint8_t *buf, size_t bufsz) DSSH_ABI(dssh_chan_read);
DSSH_PUBLIC int64_t dssh_chan_write(dssh_channel ch, int stream, const uint8_t *buf, size_t bufsz) DSSH_ABI(dssh_chan_write);
/*
 * Wait until any of the requested events fire, or until timeout_ms
 * elapses (negative = wait forever, 0 = non-blocking peek).
 *
 * Returns the bitwise-OR of fired event flags.  0 means "no events
 * fired before the timeout" -- the caller should poll again.
 *
 * Session termination behaves like POSIX POLLHUP: every requested
 * data flag (DSSH_POLL_READ, DSSH_POLL_READEXT, DSSH_POLL_WRITE) is
 * surfaced as ready, regardless of buffer state.  The caller's next
 * dssh_chan_read reports any retained read error, then drains buffered
 * bytes before returning 0 (EOF); dssh_chan_write returns a negative
 * error. This means a poll loop
 * naturally exits via subsequent read/write rather than spinning on
 * a 0 return.  DSSH_POLL_EVENT is not affected by termination --
 * use the data flags or the terminate callback to detect it.
 *
 * Returns DSSH_ERROR_INVALID for a NULL or non-stream channel.
 */
DSSH_PUBLIC int     dssh_chan_poll(dssh_channel ch, int events, int timeout_ms) DSSH_ABI(dssh_chan_poll);

/* Read the next frozen event (from the most recent poll with
 * DSSH_POLL_EVENT).  Returns 0 on success, DSSH_ERROR_NOMORE if
 * no event is frozen.  String pointers in the event are valid
 * until the next dssh_chan_read_event or dssh_chan_poll call. */
DSSH_PUBLIC int dssh_chan_read_event(dssh_channel ch, struct dssh_chan_event *event) DSSH_ABI(dssh_chan_read_event);

/* Half-close: send EOF, keep reading. */
DSSH_PUBLIC int dssh_chan_shutwr(dssh_channel ch) DSSH_ABI(dssh_chan_shutwr);

/* Close and consume the channel handle. On a server, exit_code in
 * [0, UINT32_MAX] selects an exit-status packet. Every other int64_t value
 * means omit status and still close; a client does not send exit-status.
 * No exit_code value returns DSSH_ERROR_INVALID or prevents channel disposal.
 * NULL/forbidden callback or TX-reservation contexts return DSSH_ERROR_INVALID.
 * Finish or cancel the caller's same-session TX reservation before closing.
 * An ordinary close consumes ch on every return; do not retry or reuse it.
 * A previously committed local CLOSE emits no further channel packets.
 * Otherwise attempt the selected exit-status, EOF and CLOSE in that order.
 * Detachment failure returns DSSH_ERROR_TERMINATED: the terminated session
 * retains the storage until cleanup; the application must not retry ch.
 * Packet sends are best effort: return 0 for completed local retirement,
 * including packet-send failure. Normal session-termination policy still applies.
 * Zero does not acknowledge packet delivery, peer receipt or remote completion.
 * In a server setup callback this records cancellation and returns 0;
 * physical retirement is deferred until the callback returns. See below. */
DSSH_PUBLIC int dssh_chan_close(dssh_channel ch, int64_t exit_code) DSSH_ABI(dssh_chan_close);

/* Control: signal, window-change, break (RFC 4335). */
DSSH_PUBLIC int dssh_chan_send_signal(dssh_channel ch, const char *signal_name) DSSH_ABI(dssh_chan_send_signal);
DSSH_PUBLIC int dssh_chan_send_window_change(dssh_channel ch, uint32_t cols, uint32_t rows, uint32_t wpx,
    uint32_t hpx) DSSH_ABI(dssh_chan_send_window_change);
DSSH_PUBLIC int dssh_chan_send_break(dssh_channel ch, uint32_t length) DSSH_ABI(dssh_chan_send_break);

/* Getters -- valid after open or accept until close, and on the borrowed
 * handle during its server setup callback as described below. */
DSSH_PUBLIC enum dssh_chan_type            dssh_chan_get_type(dssh_channel ch) DSSH_ABI(dssh_chan_get_type);
DSSH_PUBLIC const char                    *dssh_chan_get_command(dssh_channel ch) DSSH_ABI(dssh_chan_get_command);
DSSH_PUBLIC const char                    *dssh_chan_get_subsystem(dssh_channel ch) DSSH_ABI(dssh_chan_get_subsystem);
DSSH_PUBLIC bool                           dssh_chan_has_pty(dssh_channel ch) DSSH_ABI(dssh_chan_has_pty);
DSSH_PUBLIC const struct dssh_chan_params *dssh_chan_get_pty(dssh_channel ch) DSSH_ABI(dssh_chan_get_pty);

/* ---- Zero-copy API ---- */

/* Open a zero-copy channel.  cb fires from the demux thread for each
 * inbound CHANNEL_DATA/EXTENDED_DATA with a pointer directly into
 * rx_packet.  The callback MUST NOT call any TX function. */
DSSH_PUBLIC dssh_channel dssh_chan_zc_open(dssh_session sess, const struct dssh_chan_params *params,
    dssh_chan_zc_cb cb, void *cbdata) DSSH_ABI(dssh_chan_zc_open);

/* Get pointer into the packet-engine data area.  Reserves exclusive TX ownership
 * without retaining a mutex across the caller's fill interval.
 * Caller MUST call dssh_chan_zc_send or dssh_chan_zc_cancel promptly. */
DSSH_PUBLIC int dssh_chan_zc_getbuf(dssh_channel ch, int stream, uint8_t **buf, size_t *max_len) DSSH_ABI(dssh_chan_zc_getbuf);

/* Fill channel header, MAC, encrypt, send.  Releases TX ownership. */
DSSH_PUBLIC int dssh_chan_zc_send(dssh_channel ch, size_t len) DSSH_ABI(dssh_chan_zc_send);

/* Release TX ownership without sending. */
DSSH_PUBLIC int dssh_chan_zc_cancel(dssh_channel ch) DSSH_ABI(dssh_chan_zc_cancel);

/* ---- Event callbacks (optional, alternative to poll + read_event) ---- */
/* Replaces the callback/cbdata pair used by future delivery snapshots. An
 * already snapshotted invocation may finish after this setter returns: keep
 * its code and cbdata alive until it finishes. Existing queued/frozen poll
 * events remain available; setting NULL routes future events to that queue. */
DSSH_PUBLIC int dssh_chan_set_event_cb(dssh_channel ch, dssh_chan_event_cb cb, void *cbdata) DSSH_ABI(dssh_chan_set_event_cb);

/* Must be set before dssh_session_start(); returns DSSH_ERROR_TOOLATE
 * after start.  Copied to each new channel at open/accept time. */
DSSH_PUBLIC int dssh_session_set_event_cb(dssh_session sess, dssh_chan_event_cb cb, void *cbdata) DSSH_ABI(dssh_session_set_event_cb);

/* BREAK handler replacement uses the same captured callback/data lifetime
 * rules as event callback replacement. A captured invocation may finish after
 * replacement returns; keep its code/data alive until it finishes. */
DSSH_PUBLIC int dssh_chan_set_break_cb(dssh_channel ch, dssh_chan_break_cb cb, void *cbdata) DSSH_ABI(dssh_chan_set_break_cb);

/* Set before session start; returns DSSH_ERROR_TOOLATE afterwards.
 * Copied to newly opened/accepted channels. */
DSSH_PUBLIC int dssh_session_set_break_cb(dssh_session sess, dssh_chan_break_cb cb, void *cbdata) DSSH_ABI(dssh_session_set_break_cb);

/* ---- Event queue limits ---- */

/* Set the maximum number of queued events per channel (default 64).
 * Session-level: must be set before dssh_session_start(); applies to
 * all channels created after this call. */
DSSH_PUBLIC int dssh_session_set_max_events(dssh_session sess, size_t max_events) DSSH_ABI(dssh_session_set_max_events);

/* Per-channel override.  Returns DSSH_ERROR_INVALID if max_events is
 * less than the current number of queued events (drain events first).
 * Pass 0 to disable the event-count cap. Retained event memory is still bounded:
 * the internal per-channel envelope uses the session packet limit and the
 * configured count (64 for count 0), rounded up to at least eight power-of-two
 * slots, plus frozen/replacement storage and ring metadata. Exceeding this
 * envelope closes the channel as for count overflow. Lowering a limit does not
 * evict events or immediately discard reusable storage. */
DSSH_PUBLIC int dssh_chan_set_max_events(dssh_channel ch, size_t max_events) DSSH_ABI(dssh_chan_set_max_events);

/* Server accept callbacks run on the thread calling dssh_chan_accept(),
 * with no library mutex held. ch and params are borrowed for the invocation;
 * do not pass the handle to other threads or retain parameter views beyond it.
 * Getters on ch reflect the current accumulated params during the callback.
 * Event/BREAK callback and event-limit setters are permitted. Same-session channel
 * I/O, poll/read_event, TX/reservation and nested open/accept are rejected;
 * work on independent sessions is unaffected. Session termination/lifecycle
 * requests retain their documented deferred-cleanup behavior.
 * Negative pty_req/env results reject that request; negative terminal results
 * reject the channel. Return values otherwise retain their existing meanings.
 * Calling dssh_chan_close(ch, exit_code) cancels this setup channel. It returns
 * 0 for a recorded cancellation; exit_code is unused before program startup.
 * Do not use ch again. After the callback returns, the acceptor sends any
 * required rejection/CLOSE, retires the channel, and continues accepting.
 * A cancellation takes precedence over a nonnegative callback return.
 */
struct dssh_chan_accept_cbs {
	int (*pty_req)(dssh_channel ch, const struct dssh_chan_params *params, void *cbdata);
	int (*env)(dssh_channel ch, const struct dssh_chan_params *params, void *cbdata);
	int (*shell)(dssh_channel ch, const struct dssh_chan_params *params,
	    struct dssh_chan_accept_result *result, void *cbdata);
	int (*exec)(dssh_channel ch, const struct dssh_chan_params *params, struct dssh_chan_accept_result *result,
	    void *cbdata);
	int (*subsystem)(dssh_channel ch, const struct dssh_chan_params *params,
	    struct dssh_chan_accept_result *result, void *cbdata);
	void *cbdata;
};

/* Server accept: callback-driven setup.  Blocks until a channel is
 * fully set up (terminal request accepted) or timeout/error.
 * Returns the channel on success, NULL on failure. */
DSSH_PUBLIC dssh_channel dssh_chan_accept(dssh_session sess, const struct dssh_chan_accept_cbs *cbs,
    int timeout_ms) DSSH_ABI(dssh_chan_accept);

#ifdef __cplusplus
}
#endif

#endif   // ifndef DSSH_CONN_H
