# RFC 4254 Connection Layer — API Design

## Implementation Status

All items below are implemented in `ssh-conn.c` and `ssh-chan.c`
unless noted otherwise.

- [x] Buffer primitives (bytebuf, msgqueue, signal queue, accept queue)
- [x] Channel struct with session/raw union, buf_mtx, poll_cnd
- [x] Demux thread (session_start, packet dispatch, channel table)
- [x] Auto-reject forbidden channel types (x11, forwarding, session-from-server)
- [x] Incoming CHANNEL_OPEN queued for session_accept()
- [x] CHANNEL_OPEN_CONFIRMATION handling in demux
- [x] Channel request response mechanism (request_pending/responded/success)
- [x] Client: session_open_shell (CHANNEL_OPEN + pty-req + shell)
- [x] Client: session_open_exec (CHANNEL_OPEN + exec)
- [x] Client: channel_open_subsystem (CHANNEL_OPEN + subsystem, raw)
- [x] Server: channel_accept_raw (CONFIRMATION + raw buffers)
- [x] Session poll/read/read_ext/write with signal synchronization
- [x] Raw channel poll/read(message-based)/write
- [x] session_close (exit-status + EOF + CLOSE)
- [x] channel_close (EOF + CLOSE)
- [x] session_send_window_change
- [x] Automatic window replenishment after reads
- [x] send_exit_status, send_extended_data (low-level)
- [x] Server: session_accept_channel (pty/env/shell/exec callbacks, setup mailbox)
- [x] Window-change callback delivery to server application (stored on channel, fired by demux)

Note: both `deuce_ssh_session_channel` and `deuce_ssh_raw_channel`
use the same `struct deuce_ssh_channel_s` with `chan_type` discriminator.

## Object Model

```
deuce_ssh_session
  ├── owns the demux thread (reads all packets, dispatches to channels)
  ├── channel listener (accept incoming CHANNEL_OPEN from peer)
  ├── can open outgoing channels (session or raw)
  │
  ├── deuce_ssh_session_channel  ("popen" — shell/exec)
  │     ├── read()      — stdout
  │     ├── read_ext()  — stderr
  │     ├── write()     — stdin
  │     ├── poll()      — readiness for read/readext/write/signal
  │     ├── read_signal()
  │     └── close()     — sends exit-status + EOF + CLOSE
  │
  └── deuce_ssh_raw_channel  ("socket" — subsystem)
        ├── read()
        ├── write()
        ├── poll()      — readiness for read/write
        └── close()     — sends EOF + CLOSE
```

## Session Lifecycle

### Client

```c
/* 1. Transport setup (unchanged) */
deuce_ssh_transport_set_callbacks(tx, rx, rxline, NULL);
register_curve25519_sha256();
/* ... register algorithms ... */

struct deuce_ssh_session_s sess;
memset(&sess, 0, sizeof(sess));
sess.trans.client = true;
deuce_ssh_session_init(&sess);

/* 2. Handshake + auth (unchanged) */
deuce_ssh_transport_version_exchange(&sess);
deuce_ssh_transport_kexinit(&sess);
deuce_ssh_transport_kex(&sess);
deuce_ssh_transport_newkeys(&sess);
deuce_ssh_auth_request_service(&sess, "ssh-userauth");
deuce_ssh_auth_password(&sess, user, pass, NULL, NULL);

/* 3. Start the demux thread */
deuce_ssh_session_start(&sess);

/* 4. Open a session channel (shell with pty) */
struct deuce_ssh_pty_req pty = {
    .term = "xterm-256color",
    .cols = 80, .rows = 24,
    .wpx = 0, .hpx = 0,
};
struct deuce_ssh_session_channel_s sch;
deuce_ssh_session_open_shell(&sess, &sch, &pty);
/* sch is now a "popen" — read/write/poll ready */

/* 5. Open a raw channel (subsystem) */
struct deuce_ssh_raw_channel_s rch;
deuce_ssh_channel_open_subsystem(&sess, &rch, "sftp");
/* rch is now a "socket" — read/write/poll ready */

/* 6. Accept incoming channels from server */
struct deuce_ssh_incoming_channel inc;
while (deuce_ssh_session_accept(&sess, &inc, timeout_ms) == 0) {
    /* inc.channel_type tells us what the server wants */
    /* accept or reject, get a raw_channel back */
}
```

### Server

```c
/* 1-2. Transport setup + handshake (unchanged, server mode) */

/* 3. Auth */
deuce_ssh_auth_server(&sess, &auth_cbs, user, &user_len);

/* 4. Start the demux thread */
deuce_ssh_session_start(&sess);

/* 5. Accept incoming channels from client */
struct deuce_ssh_incoming_channel inc;
while (deuce_ssh_session_accept(&sess, &inc, -1) == 0) {
    if (inc is session) {
        /* fires pty/env/shell/exec callbacks */
        struct deuce_ssh_session_channel_s sch;
        deuce_ssh_session_accept_channel(&sess, &inc, &sch, &cbs);
        /* hand sch off to worker thread */
    }
    else if (inc is subsystem) {
        struct deuce_ssh_raw_channel_s rch;
        deuce_ssh_channel_accept_raw(&sess, &inc, &rch);
        /* hand rch off to worker thread */
    }
    else {
        deuce_ssh_channel_reject(&sess, &inc, reason);
    }
}

/* 6. Server can also open channels TO the client */
struct deuce_ssh_raw_channel_s upload_ch;
deuce_ssh_channel_open_subsystem(&sess, &upload_ch, "file-upload");
```

## Data Structures

### PTY request (client → server)

```c
struct deuce_ssh_pty_req {
    const char *term;
    uint32_t cols, rows;
    uint32_t wpx, hpx;
    const uint8_t *modes;   /* raw encoded terminal modes, NULL for none */
    size_t modes_len;
};
```

### Incoming channel (from accept)

```c
struct deuce_ssh_incoming_channel {
    uint32_t peer_channel;
    uint32_t peer_window;
    uint32_t peer_max_packet;
    const char *channel_type;      /* "session", or subsystem-specific */
    size_t channel_type_len;
    /* raw payload for type-specific data */
    uint8_t *type_data;
    size_t type_data_len;
};
```

### Server session callbacks

```c
struct deuce_ssh_server_session_cbs {
    /* Called for "pty-req" — return 0 to accept */
    int (*pty_req)(const struct deuce_ssh_pty_req *pty, void *cbdata);

    /* Called for "env" — return 0 to accept this variable */
    int (*env)(const uint8_t *name, size_t name_len,
              const uint8_t *value, size_t value_len, void *cbdata);

    /* Called for "window-change" */
    void (*window_change)(uint32_t cols, uint32_t rows,
                          uint32_t wpx, uint32_t hpx, void *cbdata);

    void *cbdata;
};
```

Note: shell/exec/subsystem determination happens at the accept
level, not via callbacks — the incoming channel tells you what the
client wants, and you call the appropriate accept function.

## Poll Interface

```c
#define DEUCE_SSH_POLL_READ    0x01  /* stdout data available (or EOF) */
#define DEUCE_SSH_POLL_READEXT 0x02  /* stderr data available (or EOF) */
#define DEUCE_SSH_POLL_WRITE   0x04  /* send window has space */
#define DEUCE_SSH_POLL_SIGNAL  0x08  /* signal ready (both streams drained to mark) */

/* Session channel poll — all four events */
int deuce_ssh_session_poll(deuce_ssh_session sess,
    struct deuce_ssh_session_channel_s *ch,
    int events, int timeout_ms);

/* Raw channel poll — READ and WRITE only */
int deuce_ssh_channel_poll(deuce_ssh_session sess,
    struct deuce_ssh_raw_channel_s *ch,
    int events, int timeout_ms);
```

Returns bitmask of ready events.  0 on timeout.  Negative on error.

A read that returns 0 bytes means EOF or closed — the application
checks `ch->close_received` or `ch->eof_received` to distinguish
if it cares.

## Read/Write Interface

### Session channels

```c
/* Returns bytes read, 0 for EOF/closed, negative for error */
ssize_t deuce_ssh_session_read(deuce_ssh_session sess,
    struct deuce_ssh_session_channel_s *ch,
    uint8_t *buf, size_t bufsz);

ssize_t deuce_ssh_session_read_ext(deuce_ssh_session sess,
    struct deuce_ssh_session_channel_s *ch,
    uint8_t *buf, size_t bufsz);

/* Returns bytes written (may be short), 0 if window full, negative for error.
 * Application should poll for WRITE before retrying. */
ssize_t deuce_ssh_session_write(deuce_ssh_session sess,
    struct deuce_ssh_session_channel_s *ch,
    const uint8_t *buf, size_t bufsz);

/* Consume the next signal.  Only callable when POLL_SIGNAL is ready.
 * Returns signal name (e.g., "INT", "TERM") — valid until next call. */
int deuce_ssh_session_read_signal(deuce_ssh_session sess,
    struct deuce_ssh_session_channel_s *ch,
    const char **signal_name);

/* Graceful close: exit-status + EOF + CLOSE */
int deuce_ssh_session_close(deuce_ssh_session sess,
    struct deuce_ssh_session_channel_s *ch,
    uint32_t exit_code);
```

### Raw channels (message-based)

Raw channels are message-based, not stream-based.  Each SSH
CHANNEL_DATA packet is one message, preserved as a discrete unit.
No partial reads or writes.

```c
/* Returns message length, 0 for EOF/closed, negative for error.
 * Always returns a complete message.
 *
 * If bufsz is too small, returns DEUCE_SSH_ERROR_TOOLONG — the
 * message remains queued and can be retried with a larger buffer.
 *
 * To query the size of the next message without consuming it,
 * pass buf=NULL, bufsz=0.  Returns the message length.
 *
 * POLL_READ means at least one complete message is queued. */
ssize_t deuce_ssh_channel_read(deuce_ssh_session sess,
    struct deuce_ssh_raw_channel_s *ch,
    uint8_t *buf, size_t bufsz);

/* Sends a complete message.  Returns 0 on success, negative on
 * error.  If the message exceeds the remote window or max packet
 * size, returns DEUCE_SSH_ERROR_TOOLONG — the application should
 * poll for WRITE and retry.  No partial sends.
 *
 * POLL_WRITE means the remote window is nonzero, but a write can
 * still fail if the specific message is larger than the available
 * window. */
int deuce_ssh_channel_write(deuce_ssh_session sess,
    struct deuce_ssh_raw_channel_s *ch,
    const uint8_t *buf, size_t len);

/* Close: EOF + CLOSE */
int deuce_ssh_channel_close(deuce_ssh_session sess,
    struct deuce_ssh_raw_channel_s *ch);
```

Typical peek-then-read pattern:

```c
ssize_t len = deuce_ssh_channel_read(sess, &ch, NULL, 0);
if (len > 0) {
    uint8_t *msg = malloc(len);
    deuce_ssh_channel_read(sess, &ch, msg, len);
    process(msg, len);
    free(msg);
}
```

## Signal Synchronization

The demux thread maintains a byte counter for each buffer (stdout,
stderr).  When a "signal" CHANNEL_REQUEST arrives:

1. Record current byte positions of stdout and stderr buffers
   as a "signal mark"
2. Queue the signal name

Poll/read behavior:

- `POLL_READ` / `POLL_READEXT` report data available only up to
  the nearest signal mark
- `session_read()` / `session_read_ext()` return data only up
  to the nearest signal mark (may return short)
- Once BOTH buffers are drained to the mark, `POLL_SIGNAL`
  becomes set
- `session_read_signal()` consumes the signal and unlocks the
  next segment of data (up to the next mark, or end of buffer)
- Multiple signals queue up with independent marks

## Windowing

- Application can set a max window size per channel at creation
  (or use a sensible default)
- The library sends WINDOW_ADJUST automatically when the
  application drains the read buffer
- The library tracks remote window for writes — `POLL_WRITE`
  reflects available send window
- Window arithmetic uses saturating add (clamped at 2^32-1)
- The application never sees WINDOW_ADJUST messages

### Session channels vs. raw channels

Session channels (stream-based): the library sends WINDOW_ADJUST
as the application drains the byte buffer.  Smooth flow.

Raw channels (message-based): the library sends WINDOW_ADJUST
after each complete message is consumed by the application's
`channel_read()`.  The internal buffer is a message queue; the
window represents the total bytes available across all queued
messages.

## Transparent Handling (demux thread)

These are consumed by the demux thread and never reach the
application:

- CHANNEL_WINDOW_ADJUST → updates send window, wakes POLL_WRITE
- CHANNEL_EOF → sets flag, wakes POLL_READ (read returns 0)
- CHANNEL_CLOSE → sends reciprocal CLOSE, sets flags, wakes poll
- CHANNEL_REQUEST "window-change" → server callback
- CHANNEL_REQUEST "xon-xoff" → ignored
- CHANNEL_REQUEST "signal" → queued with stream position marks
- CHANNEL_REQUEST "exit-status" → stored for retrieval
- GLOBAL_REQUEST → auto-reply FAILURE (or callback)
- SSH_MSG_KEXINIT → rekey
- SSH_MSG_IGNORE/DEBUG/UNIMPLEMENTED → existing handling

## What's NOT Supported

Rejected by the library:
- X11 forwarding ("x11-req", "x11" channel opens)
- TCP/IP port forwarding (global requests, "forwarded-tcpip",
  "direct-tcpip" channel opens)
- Local flow control ("xon-xoff")

Not implemented:
- Terminal mode interpretation (raw modes blob passed through)
- Host-based authentication
