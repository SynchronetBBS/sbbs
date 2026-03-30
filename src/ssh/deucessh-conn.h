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
/* 0x08 reserved (was DSSH_POLL_SIGNAL) */

/*
 * Start the demux thread.  Call after handshake and auth are complete.
 */
DSSH_PUBLIC int dssh_session_start(dssh_session sess);

/*
 * Stop the demux thread and free all channel/demux resources.
 * Called automatically by dssh_session_cleanup().
 */
DSSH_PUBLIC void dssh_session_stop(dssh_session sess);

/* ================================================================
 * New channel API (dssh_chan_*) — types and builder functions.
 * See design-channel-io-api.md for the full design.
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
	uint8_t  opcode;
};

/* Params flag bits */
#define DSSH_PARAM_HAS_PTY 0x01

/*
 * Channel parameters — populated via builder API, consumed at open time.
 * After dssh_chan_open() or dssh_chan_accept() returns, call
 * dssh_chan_params_free() to release heap storage.
 */
struct dssh_chan_params {
	/* Pointers */
	char                       *command;    /* DSSH_CHAN_EXEC (copied) */
	char                       *subsystem;  /* DSSH_CHAN_SUBSYSTEM (copied) */
	struct dssh_chan_env        *env;        /* malloc'd array (copied) */
	struct dssh_chan_mode_entry *modes;      /* malloc'd array */
	/* Pointer-sized */
	size_t                      env_count;
	size_t                      env_capacity;
	size_t                      mode_count;
	size_t                      mode_capacity;
	/* 4-byte */
	enum dssh_chan_type          type;
	uint32_t                    flags;
	uint32_t                    max_window; /* 0 = library default (2 MiB) */
	uint32_t                    cols, rows;
	uint32_t                    wpx, hpx;
	/* Char array — dynamically allocated, freed by dssh_chan_params_free */
	char                       *term;
};

/* Params builder API.  All strings are copied in.
 * dssh_chan_params_free() releases heap storage. */
DSSH_PUBLIC int dssh_chan_params_init(struct dssh_chan_params *p,
    enum dssh_chan_type type);
DSSH_PUBLIC void dssh_chan_params_free(struct dssh_chan_params *p);
DSSH_PUBLIC int dssh_chan_params_set_max_window(struct dssh_chan_params *p,
    uint32_t max_window);
DSSH_PUBLIC int dssh_chan_params_set_pty(struct dssh_chan_params *p,
    bool enable);
DSSH_PUBLIC int dssh_chan_params_set_term(struct dssh_chan_params *p,
    const char *term);
DSSH_PUBLIC int dssh_chan_params_set_size(struct dssh_chan_params *p,
    uint32_t cols, uint32_t rows, uint32_t wpx, uint32_t hpx);
DSSH_PUBLIC int dssh_chan_params_set_mode(struct dssh_chan_params *p,
    uint8_t opcode, uint32_t value);
DSSH_PUBLIC int dssh_chan_params_set_command(struct dssh_chan_params *p,
    const char *command);
DSSH_PUBLIC int dssh_chan_params_set_subsystem(struct dssh_chan_params *p,
    const char *name);
DSSH_PUBLIC int dssh_chan_params_add_env(struct dssh_chan_params *p,
    const char *name, const char *value);

/* Event poll flag (in addition to DSSH_POLL_READ etc.) */
#define DSSH_POLL_EVENT 0x10

/* Event types */
#define DSSH_EVENT_SIGNAL        1
#define DSSH_EVENT_WINDOW_CHANGE 2
#define DSSH_EVENT_BREAK         3
#define DSSH_EVENT_EOF           4
#define DSSH_EVENT_CLOSE         5
#define DSSH_EVENT_EXIT_STATUS   6
#define DSSH_EVENT_EXIT_SIGNAL   7

/* Event struct — returned by dssh_chan_read_event().
 * String pointers are valid until the next dssh_chan_read_event() or
 * dssh_chan_poll() on the same channel. */
struct dssh_chan_event {
	/* Pointer-sized */
	size_t stdout_pos; /* bytes of stdout received before event */
	size_t stderr_pos; /* bytes of stderr received before event */
	/* 4-byte */
	int    type;
	/* Union (mixed sizes, largest member determines layout) */
	union {
		struct {
			const char *name;
		} signal;
		struct {
			uint32_t cols, rows, wpx, hpx;
		} window_change;
		struct {
			uint32_t length;
		} brk;
		struct {
			uint32_t exit_code;
		} exit_status;
		struct {
			const char *signal_name;
			bool        core_dumped;
			const char *error_message;
		} exit_signal;
	};
};

/* ZC data callback — fires on demux thread for each inbound
 * CHANNEL_DATA/EXTENDED_DATA.  Returns bytes consumed for
 * WINDOW_ADJUST. */
typedef uint32_t (*dssh_chan_zc_cb)(dssh_channel ch,
    int stream, const uint8_t *data, size_t len,
    void *cbdata);

/* Event callback — fires from demux thread when an event is queued.
 * Optional alternative to poll + read_event. */
typedef void (*dssh_chan_event_cb)(dssh_channel ch,
    const struct dssh_chan_event *event, void *cbdata);

/* Server accept: result struct filled by terminal request callback */
struct dssh_chan_accept_result {
	/* Pointers */
	dssh_chan_zc_cb zc_cb;      /* NULL = stream mode */
	void           *zc_cbdata;
	/* 4-byte */
	uint32_t        max_window; /* 0 = default */
};

/* ---- New dssh_chan_* API ---- */

/* Open a stream-mode channel.  Performs the full setup sequence:
 * CHANNEL_OPEN, env, pty-req, shell/exec/subsystem, WINDOW_ADJUST.
 * Returns the channel on success, NULL on failure. */
DSSH_PUBLIC dssh_channel dssh_chan_open(dssh_session sess,
    const struct dssh_chan_params *params);

/* Stream I/O.  stream: 0=stdout/stdin, 1=stderr.
 * read returns bytes read (>0), 0 on EOF, negative on error.
 * Peek: dssh_chan_read(ch, stream, NULL, 0) returns available bytes.
 * write returns bytes sent (>0), negative on error. */
DSSH_PUBLIC int64_t dssh_chan_read(dssh_channel ch, int stream,
    uint8_t *buf, size_t bufsz);
DSSH_PUBLIC int64_t dssh_chan_write(dssh_channel ch, int stream,
    const uint8_t *buf, size_t bufsz);
DSSH_PUBLIC int dssh_chan_poll(dssh_channel ch, int events,
    int timeout_ms);

/* Read the next frozen event (from the most recent poll with
 * DSSH_POLL_EVENT).  Returns 0 on success, DSSH_ERROR_NOMORE if
 * no event is frozen.  String pointers in the event are valid
 * until the next dssh_chan_read_event or dssh_chan_poll call. */
DSSH_PUBLIC int dssh_chan_read_event(dssh_channel ch,
    struct dssh_chan_event *event);

/* Half-close: send EOF, keep reading. */
DSSH_PUBLIC int dssh_chan_shutwr(dssh_channel ch);

/* Close: send exit-status (if exit_code >= 0) + EOF + CLOSE.
 * Frees the channel handle. */
DSSH_PUBLIC int dssh_chan_close(dssh_channel ch, int64_t exit_code);

/* Control: signal, window-change, break (RFC 4335). */
DSSH_PUBLIC int dssh_chan_send_signal(dssh_channel ch,
    const char *signal_name);
DSSH_PUBLIC int dssh_chan_send_window_change(dssh_channel ch,
    uint32_t cols, uint32_t rows, uint32_t wpx, uint32_t hpx);
DSSH_PUBLIC int dssh_chan_send_break(dssh_channel ch,
    uint32_t length);

/* Getters -- valid after open or accept, until close. */
DSSH_PUBLIC enum dssh_chan_type dssh_chan_get_type(dssh_channel ch);
DSSH_PUBLIC const char *dssh_chan_get_command(dssh_channel ch);
DSSH_PUBLIC const char *dssh_chan_get_subsystem(dssh_channel ch);
DSSH_PUBLIC bool dssh_chan_has_pty(dssh_channel ch);
DSSH_PUBLIC const struct dssh_chan_params *dssh_chan_get_pty(
    dssh_channel ch);

/* ---- Zero-copy API ---- */

/* Open a zero-copy channel.  cb fires from the demux thread for each
 * inbound CHANNEL_DATA/EXTENDED_DATA with a pointer directly into
 * rx_packet.  The callback MUST NOT call any TX function. */
DSSH_PUBLIC dssh_channel dssh_chan_zc_open(dssh_session sess,
    const struct dssh_chan_params *params,
    dssh_chan_zc_cb cb, void *cbdata);

/* Get pointer into tx_packet data area.  Acquires tx_mtx.
 * Caller MUST call dssh_chan_zc_send or dssh_chan_zc_cancel promptly. */
DSSH_PUBLIC int dssh_chan_zc_getbuf(dssh_channel ch, int stream,
    uint8_t **buf, size_t *max_len);

/* Fill channel header, MAC, encrypt, send.  Releases tx_mtx. */
DSSH_PUBLIC int dssh_chan_zc_send(dssh_channel ch, size_t len);

/* Release tx_mtx without sending. */
DSSH_PUBLIC int dssh_chan_zc_cancel(dssh_channel ch);

/* ---- Event callbacks (optional, alternative to poll + read_event) ---- */

DSSH_PUBLIC int dssh_chan_set_event_cb(dssh_channel ch,
    dssh_chan_event_cb cb, void *cbdata);
DSSH_PUBLIC int dssh_session_set_event_cb(dssh_session sess,
    dssh_chan_event_cb cb, void *cbdata);

/* Server accept: callback struct */
struct dssh_chan_accept_cbs {
	int (*pty_req)(dssh_channel ch,
	    const struct dssh_chan_params *params, void *cbdata);
	int (*env)(dssh_channel ch,
	    const struct dssh_chan_params *params, void *cbdata);
	int (*shell)(dssh_channel ch,
	    const struct dssh_chan_params *params,
	    struct dssh_chan_accept_result *result, void *cbdata);
	int (*exec)(dssh_channel ch,
	    const struct dssh_chan_params *params,
	    struct dssh_chan_accept_result *result, void *cbdata);
	int (*subsystem)(dssh_channel ch,
	    const struct dssh_chan_params *params,
	    struct dssh_chan_accept_result *result, void *cbdata);
	void *cbdata;
};

/* Server accept: callback-driven setup.  Blocks until a channel is
 * fully set up (terminal request accepted) or timeout/error.
 * Returns the channel on success, NULL on failure. */
DSSH_PUBLIC dssh_channel dssh_chan_accept(dssh_session sess,
    const struct dssh_chan_accept_cbs *cbs, int timeout_ms);

#ifdef __cplusplus
}
#endif

#endif // ifndef DSSH_CONN_H
