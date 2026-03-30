# Channel I/O API Redesign

Replaces TODO items 95 and 98.  Pre-release — no backward compat needed.

## Problem

The current API splits channel I/O into two families with different names,
return types, and peek behavior:

| Operation  | Session (stream)           | Raw (message)           |
|------------|---------------------------|-------------------------|
| read       | `dssh_session_read` → i64  | `dssh_channel_read` → i64 |
| write      | `dssh_session_write` → i64 | `dssh_channel_write` → **int** |
| read ext   | `dssh_session_read_ext`    | *(none)*                |
| write ext  | `dssh_session_write_ext`   | *(none)*                |
| peek       | rejected (NULL → error)    | supported (NULL,0 → size) |
| poll       | `dssh_session_poll`        | `dssh_channel_poll`     |

Issues:
1. **Return type mismatch**: session write returns byte count (`int64_t`),
   raw write returns 0/error (`int`).
2. **Naming confusion**: `dssh_session_*` conflates "SSH session handle"
   with "session channel type" — the first parameter is already
   `dssh_session sess`.
3. **Peek inconsistency**: raw supports it, session doesn't.
4. **No discoverability**: caller must know which prefix to use based on
   how the channel was opened.
5. **Per-packet malloc on raw receive**: `msgqueue_push()` does
   `malloc(sizeof(entry) + len)` for every inbound CHANNEL_DATA.
6. **Per-packet malloc on all sends**: `conn_send_data()` and
   `send_extended_data()` malloc a temporary buffer just to prepend a
   9/13-byte header before `send_packet_inner()` copies it into the
   pre-allocated `tx_packet`.  See TODO item 101.

## RFC 4254 analysis

RFC 4254 is remarkably underspecified about channel lifecycle.  The
protocol defines CHANNEL_OPEN as a bidirectional byte pipe and
CHANNEL_REQUEST as a bag of named messages with no ordering, multiplicity,
or lifecycle constraints.  The conventions everyone follows — one pty-req,
then exactly one of shell/exec/subsystem, then data flows — are de facto
behavior, not protocol requirements.

What the RFC does specify (buried in the text):
- "Only one of these requests can succeed per channel" (s6.5) —
  exactly one terminal request (shell/exec/subsystem) per channel
- "Environment variables may be passed to the shell/command to be
  started later" (s6.4) — env requests come before the terminal request
- Terminal mode opcodes 160-255 "cause parsing to stop (they should
  only be used after any other data)" (s8) — weak ordering constraint
  on mode encoding; opcodes 1-159 can be in any order
- RFC 8160 adds IUTF8 (opcode 42) and clarifies: "the client SHOULD
  transmit a value for this mode if it knows about one" — don't guess

Specific gaps in the RFC:
- No restriction on sending data before any CHANNEL_REQUEST
- No restriction on multiple pty-req requests (OpenSSH disconnects
  on a second one, but the RFC doesn't prohibit it)
- No defined relationship between EOF, exit-status, and CLOSE
- Signal and window-change are independent mechanisms with no defined
  interaction (SIGWINCH vs window-change are the same event split into
  two unrelated request types)
- Flow control (SSH window) has no defined relationship to process I/O;
  the RFC doesn't address what happens when the window fills and a
  process is still producing output

### What OpenSSH actually does (the de facto spec)

**Server side** (`session.c`, `channels.c`, `serverloop.c`):

1. CHANNEL_OPEN received → create channel in setup state with
   **`initial_window=0`** — no data can flow
2. Send CHANNEL_OPEN_CONFIRMATION with `initial_window=0`
3. While in setup: accept setup requests (pty-req, env, x11-req,
   auth-agent-req, shell, exec, subsystem)
4. shell/exec/subsystem → `do_exec()` → fork process →
   `channel_set_fds()` → transition to open, set window to
   `CHAN_SES_WINDOW_DEFAULT` (2 MiB), send WINDOW_ADJUST
5. After open: only window-change, break, signal accepted
6. Second pty-req = **protocol disconnect** (not reject — kills the
   whole session)

(OpenSSH calls the setup state "LARVAL"; DeuceSSH uses `setup_mode`.)

**Client side** (`ssh.c`):

- Sends `CHAN_SES_WINDOW_DEFAULT` (2 MiB) in CHANNEL_OPEN — non-zero,
  unlike the server
- Halves window and packet size for PTY sessions (1 MiB / 16 KiB)
- No setup state — fds attached at channel creation, data written
  to stdout as soon as it arrives
- **Vulnerability**: a malicious server could send data before any
  channel request is processed; the client would blindly write it
  to the terminal

**Window sizes** (`channels.h`):

- `CHAN_SES_PACKET_DEFAULT` = 32 KiB
- `CHAN_SES_WINDOW_DEFAULT` = 64 × 32 KiB = 2 MiB
- `CHAN_TCP_WINDOW_DEFAULT` = 2 MiB (same)
- `CHAN_X11_WINDOW_DEFAULT` = 64 KiB

**Exit status handling** (`clientloop.c`):

- `exit_status` initialized to `-1`
- If exit-status CHANNEL_REQUEST received: `exit_status = exitval`
- If no exit-status received: client exits with 255 (-1 as unsigned byte)
- `exitval` is `u_int` (wire: `uint32`), stored in `int exit_status`,
  returned from `main()` — only low 8 bits survive POSIX exit.
  Exit-status 256 = exit code 0.  Completely broken for >255 values.

### Design implications

DeuceSSH follows the OpenSSH server model (setup + zero window) on
**both** sides.  This is strictly safer than the OpenSSH client
behavior and matches what the dominant server implementation expects:

- Both sides send `initial_window=0` in CHANNEL_OPEN / CONFIRMATION
- No data can flow until the channel is finalized
- No buffer allocation until the I/O model is committed
- No trust that the peer will behave

The library enforces a stricter lifecycle than the RFC requires, because
that's what real-world interop demands.  Specifically:
- Setup requests (pty-req, env) only accepted before terminal request
- Exactly one terminal request (shell/exec/subsystem) per channel
- Data flow begins only after terminal request succeeds
- Post-setup: only window-change, break, signal accepted

## Design Decisions

### Naming: `dssh_chan_` prefix

All channel functions use the `dssh_chan_` prefix.  Shorter than
`dssh_channel_`, avoids conflation with `dssh_session`.

The zero-copy API adds `zc_`: `dssh_chan_zc_`.

### Two distinct I/O models, two distinct APIs

Stream and zero-copy are **separate APIs** with separate open functions,
not one function with behavioral differences based on a NULL check or
flag.  They allocate different internal buffers and unlock different
I/O functions.

- **Zero-copy** (`dssh_chan_zc_*`): the core implementation.  Data
  callback for inbound data; outbound writes go directly into the
  transport packet buffer; inbound data delivered as pointer into the
  decrypted packet buffer.  No library-side copies or accumulation
  buffers.  Events delivered via the same event queue/callback as
  stream mode.
- **Stream** (`dssh_chan_*`): convenience layer built on top of the ZC
  internals.  Adds ring buffers and POSIX read/write/poll semantics.
  One memcpy per direction (ring buffer on RX, memcpy into tx_packet
  on TX).

### Channel type: `enum dssh_chan_type`

```c
enum dssh_chan_type {
    DSSH_CHAN_SHELL,
    DSSH_CHAN_EXEC,
    DSSH_CHAN_SUBSYSTEM
};
```

Closed set — RFC s6.5: "only one of these requests can succeed per
channel."  Library maps enum to wire strings internally.

### Channel params: builder pattern

Channel configuration is expressed via `struct dssh_chan_params`,
populated through a builder API.  The type is part of the struct.
PTY is orthogonal to type (exec can have a pty; shell can omit one).
The library does what it's told — subsystem with a pty is allowed
if the protocol allows it.

```c
struct dssh_chan_params {
    enum dssh_chan_type type;
    uint32_t           flags;       /* DSSH_PARAM_HAS_PTY, etc. */
    uint32_t           max_window;  /* 0 = library default (2 MiB) */
    struct dssh_pty_req pty;        /* valid if HAS_PTY */
    char               *command;    /* DSSH_CHAN_EXEC (copied) */
    char               *subsystem;  /* DSSH_CHAN_SUBSYSTEM (copied) */
    struct dssh_chan_env *env;       /* malloc'd array (copied) */
    size_t              env_count;
};
```

Builder API (all strings are copied in, `free` releases them):

```c
int  dssh_chan_params_init(struct dssh_chan_params *p,
         enum dssh_chan_type type);
void dssh_chan_params_free(struct dssh_chan_params *p);

int  dssh_chan_params_set_max_window(struct dssh_chan_params *p,
         uint32_t max_window);
int  dssh_chan_params_set_pty(struct dssh_chan_params *p, bool enable);
int  dssh_chan_params_set_term(struct dssh_chan_params *p,
         const char *term);
int  dssh_chan_params_set_size(struct dssh_chan_params *p,
         uint32_t cols, uint32_t rows, uint32_t wpx, uint32_t hpx);
int  dssh_chan_params_set_mode(struct dssh_chan_params *p,
         uint8_t opcode, uint32_t value);
int  dssh_chan_params_set_command(struct dssh_chan_params *p,
         const char *command);
int  dssh_chan_params_set_subsystem(struct dssh_chan_params *p,
         const char *name);
int  dssh_chan_params_add_env(struct dssh_chan_params *p,
         const char *name, const char *value);
```

`init` sets the type and defaults: `term="dumb"`, dimensions `0x0`,
pty enabled for shell (disabled for exec/subsystem), no modes, no
env.  **Zero terminal mode opcodes by default** — the library cannot
know the app's terminal state (RFC 8160: "transmit a value if it
knows about one").  The app calls `set_mode()` to populate modes
from its actual terminal state (e.g. by reading `struct termios`
and translating flags to SSH opcodes — POSIX-specific, so the
translation is the app's responsibility, not the library's).

`set_mode()` accumulates opcode/value pairs.  Duplicates: last-wins.
Encoding (at open time) emits opcodes 1-159 first, then 160+, then
TTY_OP_END (RFC s8 ordering constraint).

The params struct is consumed at open time — the library reads
everything, sends the wire messages, and does not keep references.
After open returns, the app calls `free` and the struct is dead.

Env vars are sent as individual CHANNEL_REQUEST "env" before the
terminal request (RFC s6.4: "passed to the shell/command to be
started later").

### Open functions

```c
dssh_channel dssh_chan_open(dssh_session sess,
    const struct dssh_chan_params *params);

dssh_channel dssh_chan_zc_open(dssh_session sess,
    const struct dssh_chan_params *params,
    dssh_chan_zc_cb cb, void *cbdata);
```

### Open does the full setup → open sequence

Both open functions perform the complete channel lifecycle internally:

1. Send CHANNEL_OPEN "session" with `initial_window=0`
2. Wait for CHANNEL_OPEN_CONFIRMATION
3. Send env requests (from params, in order)
4. Send pty-req (if `DSSH_PARAM_HAS_PTY` flag set)
5. Send terminal request (shell/exec/subsystem per type)
6. Allocate I/O buffers (stream: ring buffers; zc: none)
7. Send WINDOW_ADJUST to open the window
8. Return live channel

The app calls one function and gets a ready-to-use channel.  The
internal implementation is decomposed into steps so a lower-level
API can be exposed later if needed, without rewriting the guts.

### Max packet size: stays in `dssh_session_init()`

Max packet size is a transport-layer concern (packet framing), not a
channel concern.  It's used immediately at session init to allocate
transport buffers.  Stays as the second arg to `dssh_session_init()`.

### Max window size: per-channel, in params

`max_window` in the params struct controls buffer allocation (stream
mode) and the WINDOW_ADJUST sent after the terminal request succeeds.
`init` sets it to 0; `set_max_window` overrides.  0 means library
default (currently 2 MiB, matching OpenSSH's
`CHAN_SES_WINDOW_DEFAULT`).

### Stream select via parameter, not separate functions

Read and write take a `stream` parameter instead of having `_ext`
variants.  Consistent across both stream and zero-copy APIs:

- `stream == 0`: stdout/stdin (SSH_MSG_CHANNEL_DATA)
- `stream == 1`: stderr (SSH_MSG_CHANNEL_EXTENDED_DATA)

## API summary

All functions except the two open calls take `dssh_channel` only —
the channel carries its session internally.  This prevents
mismatched sess/ch pairs.  If the app needs the session handle,
a `dssh_chan_get_session(ch)` getter can be added later.

All functions that can fail return an error code or typed result.
Infallible operations (e.g. `dssh_chan_params_free`) may return void.

### Stream-only

```c
dssh_channel dssh_chan_open(dssh_session sess,
    const struct dssh_chan_params *params);

int64_t dssh_chan_read(dssh_channel ch, int stream,
    uint8_t *buf, size_t bufsz);

int64_t dssh_chan_write(dssh_channel ch, int stream,
    const uint8_t *buf, size_t bufsz);

int dssh_chan_poll(dssh_channel ch, int events, int timeout_ms);
```

### ZC-only

```c
dssh_channel dssh_chan_zc_open(dssh_session sess,
    const struct dssh_chan_params *params,
    dssh_chan_zc_cb cb, void *cbdata);

int dssh_chan_zc_getbuf(dssh_channel ch, int stream,
    uint8_t **buf, size_t *max_len);

int dssh_chan_zc_send(dssh_channel ch, size_t len);

int dssh_chan_zc_cancel(dssh_channel ch);
```

`zc_getbuf` stashes the stream selection on the channel struct.
`zc_send` and `zc_cancel` use it — no redundant `stream` parameter
that could mismatch.

### Shared (both stream and ZC)

```c
/* Sending */
int dssh_chan_shutwr(dssh_channel ch);
int dssh_chan_close(dssh_channel ch, int64_t exit_code);
int dssh_chan_send_signal(dssh_channel ch,
    const char *signal_name);
int dssh_chan_send_window_change(dssh_channel ch,
    uint32_t cols, uint32_t rows, uint32_t wpx, uint32_t hpx);
int dssh_chan_send_break(dssh_channel ch, uint32_t length);

/* Events — separate from data, signalfd()-style.
 * Events are queued by the demux thread.  The app polls for
 * DSSH_POLL_EVENT and pulls events with read_event().
 * Each event carries advisory stream positions (bytes of
 * unread stdout/stderr at poll time) so the app can
 * correlate with the data stream if it wants to.
 * Data always flows freely — events never gate reads. */
int dssh_chan_read_event(dssh_channel ch,
    struct dssh_chan_event *event);

/* Optional event callback — fires from the demux thread when
 * an event is queued.  If set, the app can handle events
 * immediately without polling.  Optional — poll + read_event
 * covers all traffic patterns without callbacks. */
int dssh_chan_set_event_cb(dssh_channel ch,
    dssh_chan_event_cb cb, void *cbdata);
```

### Event struct

```c
/* Event types */
#define DSSH_EVENT_SIGNAL       1
#define DSSH_EVENT_WINDOW_CHANGE 2
#define DSSH_EVENT_BREAK        3
#define DSSH_EVENT_EOF          4
#define DSSH_EVENT_CLOSE        5
#define DSSH_EVENT_EXIT_STATUS  6
#define DSSH_EVENT_EXIT_SIGNAL  7

struct dssh_chan_event {
    int    type;
    size_t stdout_pos;  /* bytes of unread stdout at poll time */
    size_t stderr_pos;  /* bytes of unread stderr at poll time */
    union {
        struct { const char *name; } signal;
        struct { uint32_t cols, rows, wpx, hpx; } window_change;
        struct { uint32_t length; } brk;
        struct { uint32_t exit_code; } exit_status;
        struct {
            const char *signal_name;
            bool        core_dumped;
            const char *error_message;
        } exit_signal;
    };
};
```

### Event lifecycle: poll freezes, read_event consumes

`dssh_chan_poll()` with `DSSH_POLL_EVENT` computes and freezes the
next pending event's stream positions (bytes of unread stdout/stderr
at poll time).  The frozen event is guaranteed available until the
next poll.  `dssh_chan_read_event()` returns that frozen event.

**One event per poll/read_event cycle.**  If multiple events are
pending, the next `dssh_chan_poll()` returns `DSSH_POLL_EVENT` again
immediately with the next event's positions.  The app handles one
event at a time:

```c
if (ev & DSSH_POLL_EVENT) {
    struct dssh_chan_event event;
    dssh_chan_read_event(ch, &event);
    drain_stdout(ch, event.stdout_pos);
    drain_stderr(ch, event.stderr_pos);
    handle_event(&event);
    continue;  /* poll again — might be more events */
}
```

**Event discard**: events not consumed via `read_event()` between
polls are discarded — the app chose to ignore them by polling again
without reading.  No memory leak, no unbounded queue growth.

**Positions are advisory**: the app CAN drain exactly N/M bytes
before handling the event (position-aware), or ignore positions
entirely and just process the event (position-unaware).  Data
always flows freely — `dssh_chan_read()` never stops at event
boundaries, never returns fake EOF, never does short reads due to
events.

String pointers in events (signal name, exit-signal name,
error message) point into channel-owned buffers, valid from
`dssh_chan_read_event()` until the next `dssh_chan_read_event()`
or `dssh_chan_poll()` call on the same channel.

### Callback types

```c
/* ZC data callback — fires on every inbound CHANNEL_DATA or
 * CHANNEL_EXTENDED_DATA for this channel.  Receives a pointer
 * directly into the decrypted rx_packet (valid only for the
 * duration of the callback).  Runs on demux thread with no
 * library mutex held.  CANNOT call any TX function (deadlock
 * risk — see locking section; enforced via _Thread_local guard).
 * Must not block for long.
 *
 * Return: number of bytes consumed for WINDOW_ADJUST, or 0 to
 * defer window replenishment. */
typedef uint32_t (*dssh_chan_zc_cb)(dssh_channel ch,
    int stream, const uint8_t *data, size_t len,
    void *cbdata);

/* Event callback — fires from the demux thread when an event
 * is queued (both stream and ZC modes).  Optional — poll +
 * read_event covers all traffic patterns without callbacks.
 * The event struct is populated with stream positions computed
 * at queue time (the moment the demux thread processes the
 * event's packet).  Valid for the duration of the callback.
 * Same restrictions as ZC callbacks: cannot call TX functions,
 * must not block.
 *
 * If the app uses callbacks, it doesn't need poll+read_event
 * for events — the callback delivers everything.  The two
 * paths have different position scopes: callback positions are
 * computed at queue time, poll positions at poll time. */
typedef void (*dssh_chan_event_cb)(dssh_channel ch,
    const struct dssh_chan_event *event, void *cbdata);
```

## Stream API details

### Return type: all read/write return `int64_t`

- `> 0`: bytes transferred
- `0`: EOF (read only — all data drained and peer sent CHANNEL_EOF)
- `< 0`: `DSSH_ERROR_*`

POSIX semantics: read returns what's available (up to bufsz), write
returns what was accepted (may be less than bufsz due to window
clamping).  Caller loops for complete delivery.

Read returning 0 means EOF, same as POSIX `read()`.  "No data yet"
is not a read return value — the app uses `dssh_chan_poll` to wait
for readiness before calling read.

### Peek: `dssh_chan_read(ch, stream, NULL, 0)`

Returns bytes available in the specified stream's buffer without
consuming.

### Data and events are separate channels

Data and events are independent.  This is the `signalfd()` model —
events are a separate pollable channel, not inline in the data
stream.

**Data** arrives in two streams (stdout, stderr) via `dssh_chan_read`.
Read never stops at event boundaries.  Data always flows freely.

**Events** (signal, window-change, break, EOF, close, exit-status,
exit-signal) are queued separately.  `DSSH_POLL_EVENT` tells the app
one is ready.  `dssh_chan_read_event()` pulls it off the queue.
Each event carries advisory stream positions (bytes of unread
stdout/stderr at the time of the event) so the app CAN correlate
if it wants to, but is not required to.

This avoids the two-pointer problem: with independent read pointers
for stdout and stderr, stream-position callbacks can't work — a
CHANNEL_REQUEST has one wire position but two independent buffer
positions.  Gating reads on events (fake EOF, short reads at event
boundaries) creates state confusion.  Separating data and events
eliminates all of this.

### Receive-side lifecycle (stream mode)

**Event-driven I/O (poll + read + read_event, no callbacks needed):**

**Position-aware** (e.g. terminal emulator processing SIGWINCH):

```c
for (;;) {
    int ev = dssh_chan_poll(ch,
        DSSH_POLL_READ | DSSH_POLL_READEXT
        | DSSH_POLL_EVENT, -1);

    if (ev & DSSH_POLL_EVENT) {
        struct dssh_chan_event event;
        dssh_chan_read_event(ch, &event);
        drain_stdout(ch, event.stdout_pos);
        drain_stderr(ch, event.stderr_pos);
        handle_event(&event);
        continue;  /* poll again — might be more events */
    }
    if (ev & DSSH_POLL_READ) {
        n = dssh_chan_read(ch, 0, buf, sizeof(buf));
        if (n == 0) break; /* EOF */
        process_stdout(buf, n);
    }
    if (ev & DSSH_POLL_READEXT) {
        n = dssh_chan_read(ch, 1, buf, sizeof(buf));
        if (n > 0) process_stderr(buf, n);
    }
}
```

The `continue` is the key: after handling one event, go back to poll.
If another event is pending, poll returns immediately with
`DSSH_POLL_EVENT`.  If not, poll returns with `DSSH_POLL_READ` or
whatever else is ready.

**Position-unaware** (e.g. simple exec, SFTP): same loop but skip
the drain calls — just read events and handle them, read data
freely.  Positions are ignored.

**Optional event callback:** if set via `dssh_chan_set_event_cb`,
fires from the demux thread when an event is queued, with the
event struct passed directly (positions computed at queue time).
The app handles the event in the callback and doesn't need
poll+read_event for events at all.  Not required for any traffic
pattern — poll+read_event is the alternative.

## Zero-copy API details

### Inbound data: direct pointer into rx_packet

The ZC data callback receives inbound data as parameters: `stream`,
`data` (pointer directly into decrypted `rx_packet`), and `len`.
No library-side accumulation buffer, no copies.  The pointer is valid
only for the duration of the callback (the demux thread reuses
`rx_packet` for the next packet).

The data starts at offset 9 (CHANNEL_DATA) or 13
(CHANNEL_EXTENDED_DATA) within `rx_packet`, past the SSH headers.

For protocols where messages fit in one packet: truly zero-copy.
For protocols that need reassembly across packets (e.g. SFTP messages
larger than max_packet): the app manages its own accumulation buffer.
The library isn't in a better position to do that copy than the app.

Events use the same model as stream mode: queued separately,
pulled via `dssh_chan_read_event()`, with advisory stream positions.
The optional `dssh_chan_event_cb` fires from the demux thread when
an event is queued (same as in stream mode).  All events fire in
wire order.

### Outbound data: direct buffer access

The app builds data directly in the transport packet buffer via
`dssh_chan_zc_getbuf()` / `dssh_chan_zc_send()`.

**`dssh_chan_zc_getbuf(ch, stream, &buf, &max_len)`**:
1. Acquires `tx_mtx`
2. If `rekey_in_progress`, waits on `rekey_cnd` (same as `send_packet`)
3. Computes `max_len = min(remote_window, remote_max_packet)`
4. If `max_len == 0`: releases `tx_mtx`, returns error (window full)
5. Stashes `stream` on the channel struct
6. Sets `*buf = &tx_packet[4 + 5 + chan_header]`; sets `*max_len`
7. Returns 0

The returned pointer is past the seq prefix (4), packet_length (4),
padding_length (1), and channel data header (9 for CHANNEL_DATA,
13 for CHANNEL_EXTENDED_DATA).  The app writes up to `max_len` bytes.
**Must not block between getbuf and send** — `tx_mtx` is held.

**`dssh_chan_zc_send(ch, len)`**:
1. Fills channel header bytes (msg type, channel ID, data_type_code,
   length) at `&tx_packet[4 + 5]` using the stashed stream
2. Fills packet_length and padding_length at `tx_packet[4]`
3. Generates random padding
4. Writes seq at `tx_packet[0]`
5. Computes MAC over contiguous `tx_packet[0 .. 4 + total)` — no copy
6. Encrypts in-place `tx_packet[4 .. 4 + total)` — no copy
7. Sends `tx_packet[4 .. 4 + total + mac_len)` via I/O callback
8. Deducts `len` from `remote_window` (atomic)
9. Releases `tx_mtx`

**`dssh_chan_zc_cancel(ch)`**:
1. Releases `tx_mtx` without sending

**Zero memcpy on the send path.**  The current code has two copies
that are both eliminated:

1. **Payload copy** (`memcpy(tx_packet, payload)` at line 532):
   eliminated by `zc_getbuf` — the app writes directly at the right
   offset in `tx_packet`.

2. **MAC input copy** (`memcpy(tx_mac_scratch, tx_packet)` at line
   546): the MAC input is `seq(4) || unencrypted_packet`, and the
   4-byte sequence number wasn't contiguous with the packet.
   Eliminated by allocating `tx_packet` with 4 extra bytes at the
   front:

```
offset:  0       4                  9               9+payloadlen
         [seq]   [pkt_length(4)]   [pad_len(1)]   [payload...]  [padding]  [MAC]
         |<---- MAC input (contiguous) ---------->|
                 |<---- encrypt in-place -------->|
                 |<---- send on wire ---------------------------------------->|
```

   `tx_mac_scratch` is eliminated entirely.  The same optimization
   applies to non-ZC sends and the RX MAC path (`rx_mac_scratch`).

### Locking

**Lock order**: `channel_mtx` → `buf_mtx` → `cb_mtx` → `tx_mtx`

**TX path** (`zc_getbuf` → `zc_send`): app holds `tx_mtx` for the
duration of filling one packet.  This is bounded (generating at most
`remote_max_packet` bytes, typically 32 KiB).  During this time:

- Other application senders block on `tx_mtx` — acceptable, the
  transport is inherently serialized
- Demux fire-and-forget sends (CHANNEL_FAILURE, etc.) go to the
  `tx_queue` via `mtx_trylock` — already handled by existing
  `send_or_queue` mechanism
- Rekey cannot start (needs `tx_mtx` to send KEXINIT) — bounded
  stall, not a security concern at rekey thresholds (1 GiB /
  2^31 packets)

Contract: **getbuf and send/cancel must be called in quick
succession.**  Violating this degrades throughput for other channels
but doesn't cause correctness issues.

**RX path** (demux → callback): the ZC data callback and event
callbacks run on the demux thread with **no library mutex held**.
The demux thread is the sole writer of everything the callback reads
(rx_packet data, local_window, channel state flags).  No contention,
no lock needed.

The one shared variable between the demux thread and the app's TX
thread is `remote_window` (incremented by demux on
CHANNEL_WINDOW_ADJUST from peer, decremented by app in `zc_send`).
This is `atomic_uint32_t` — no mutex needed.

Channel state flags (`eof_received`, `close_received`, etc.) are
also shared with the app thread and are atomic.

**The RX callback CANNOT call any TX function** (`zc_getbuf`,
`zc_send`, `zc_cancel`, `send_signal`, `send_window_change`,
`send_break`, `shutwr`, `close`).  Doing so risks deadlock:

- `zc_getbuf` acquires `tx_mtx` and waits for rekey to complete.
  Rekey needs the demux thread to process KEXINIT/NEWKEYS packets.
  The demux thread is blocked in the callback.  **Deadlock.**
- Even without rekey, `zc_send` calls the I/O tx callback under
  `tx_mtx`.  If the network is congested, the demux thread stalls.

Enforced at runtime via `_Thread_local bool in_zc_rx`.  The demux
thread sets it before calling the callback and clears it after.
All TX-path functions check it and return `DSSH_ERROR_INVALID` if
set.  From a worker thread the flag is always false — TX always
works.  The guard only fires when the app calls TX from inside the
callback (same thread as demux), which is always a programming
error, never a transient condition.

The app must queue work from the callback and send from another
thread.  Typical pattern for SFTP: callback copies the request
(or just notes its type and parameters), returns.  A worker thread
picks up the queued request, calls `zc_getbuf`, builds the response
directly in `tx_packet`, calls `zc_send`.

**Callback restrictions** (ZC data callback and event callback):
- **Can** read callback parameters (data pointer, length, stream)
- **Can** return consumed byte count for WINDOW_ADJUST (zc_cb only)
- **Cannot** call any TX function (enforced by `in_zc_rx` flag)
- **Cannot** call `dssh_chan_read` / `write` / `poll` /
  `read_event` / `close`
- **Must be fast** — the demux thread is stalled for ALL channels

**Callback pointer protection**: `cb_mtx` is a per-channel mutex
protecting all callback function pointer + cbdata pairs.  The demux
thread (or `dssh_chan_read`) acquires `cb_mtx` briefly to read the
pointer, then calls the callback outside the lock.  The setter
acquires `cb_mtx` alone.

### Window management in ZC mode

The ZC callback return value controls WINDOW_ADJUST.  The callback
returns the number of bytes consumed (typically the data length if the
app processed the packet, 0 if it copied the data for later
processing and will manage flow control separately).

The library sends WINDOW_ADJUST for the consumed amount after the
callback returns.  This requires `tx_mtx`.  No library mutex is held
when the callback returns, so the demux thread acquires `tx_mtx`
directly (or uses `send_or_queue` if contended).

## Shared function details

### Half-close: `dssh_chan_shutwr()`

Sends SSH_MSG_CHANNEL_EOF.  Stops writes, keeps reading.
`shutdown(fd, SHUT_WR)` semantics — the name makes the direction
unambiguous (unlike `dssh_chan_eof()` which could mean send or receive).

### Close: `dssh_chan_close()`

- `exit_code >= 0 && exit_code <= UINT32_MAX`: send exit-status +
  EOF + CLOSE
- `exit_code < 0`: send EOF + CLOSE (no exit-status)

Sends EOF (if not already sent via `shutwr`), then CLOSE.  Frees the
channel handle.

Rationale for `int64_t`: the wire type is `uint32_t` (0–4294967295),
so the full valid range fits in the non-negative half.  Negative values
are an unambiguous out-of-band sentinel for "no exit-status."  This
avoids the OpenSSH bug where exit-status 0 and no-exit-status are
conflated (OpenSSH client uses -1/255 as sentinel, which collides
with legitimate exit code 255).

### Callback defaults from session

Session-level event callback setter establishes the default inherited
by all channels created on that session.  Set before opening channels;
`dssh_chan_open` / `dssh_chan_zc_open` copies it to the new channel
at creation time (before the window opens, so no race).  Per-channel
`dssh_chan_set_event_cb` overrides after open.

```c
int dssh_session_set_event_cb(dssh_session sess,
    dssh_chan_event_cb cb, void *cbdata);
```

## Implementation architecture

The stream API is built on top of the ZC internals, not a separate
implementation.  One code path handles the transport interaction;
two interfaces sit on top.  Bugs fixed in the ZC layer fix stream
mode too.

### Public vs internal split

Each public ZC function has an internal counterpart that skips
parameter validation:

```c
/* Public — validates ch, checks in_zc_rx, verifies state */
DSSH_PUBLIC int
dssh_chan_zc_getbuf(dssh_channel ch, int stream,
    uint8_t **buf, size_t *max_len);

/* Internal — called by stream layer, skips checks */
DSSH_TESTABLE int
zc_getbuf_inner(dssh_channel ch, int stream,
    uint8_t **buf, size_t *max_len);
```

The stream layer calls the inner functions directly — it created
the channel and knows it's valid.

### Stream receive path

The library registers an internal `dssh_chan_zc_cb` on stream
channels.  This callback copies data from `rx_packet` into the
per-channel ring buffer and broadcasts `poll_cnd`.  The app's
`dssh_chan_read` drains the ring buffer.

Events (signal, window-change, break, EOF, close, exit-status,
exit-signal) are queued by the internal callback with the current
ring buffer positions for both streams.  The app pulls them via
`dssh_chan_read_event()`.

### Stream send path

`dssh_chan_write` calls `zc_getbuf_inner`, memcpys the app's data
into `tx_packet`, calls `zc_send_inner`.  One memcpy, same cost as
today (minus the eliminated malloc and MAC scratch copy).

### Stream poll

Waits on `poll_cnd` for ring buffer state changes (data available,
space available, EOF/close).  The internal ZC callback broadcasts
`poll_cnd` after updating the ring buffer.

## Allocation summary (current vs new)

### Current

| Path | Stream (shell/exec) | Raw (subsystem) |
|------|-------------------|-----------------|
| **Receive** | 0 mallocs, 2 copies | 1 malloc+free, 3 copies |
| **Send** | 1 malloc+free, 2 copies | 1 malloc+free, 2 copies |

Receive (stream): decrypt→rx_packet (pre-alloc), rx_packet→bytebuf
(pre-alloc ring buffer).  Zero mallocs.

Receive (raw): decrypt→rx_packet, rx_packet→msgqueue_entry (malloc'd),
entry→app buffer on read (free'd).  One malloc per packet.

Send (both): app→temp buffer (malloc'd to prepend 9/13-byte header),
temp→tx_packet (pre-alloc), tx_packet→tx_mac_scratch (MAC input copy),
free temp.  One malloc + two memcpys per packet.

### New (stream)

Built on ZC internals.  Ring buffers add one memcpy per direction.
The send-path malloc (TODO item 101) and MAC scratch copy are both
eliminated by the tx_packet layout change (4-byte seq prefix).

| Path | Stream (new) |
|------|-------------|
| **Receive** | 0 mallocs, 1 copy (rx_packet → ring buffer) |
| **Send** | 0 mallocs, 1 copy (app buf → tx_packet via zc_getbuf_inner) |

### New (zero-copy)

| Path | Zero-copy |
|------|-----------|
| **Receive** | 0 mallocs, 0 copies |
| **Send** | 0 mallocs, 0 copies |

Receive: decrypt in-place→rx_packet, callback gets pointer directly
into rx_packet at data offset (9 or 13 bytes past start).  Zero
copies.

Send: app writes directly into tx_packet via `zc_getbuf()`.
`zc_send()` fills headers, writes seq prefix at `tx_packet[0]`,
computes MAC over contiguous buffer (no copy to `tx_mac_scratch`),
encrypts in-place, sends.  Zero copies.

## Server-side API

### Accept: callback-driven setup with shared params struct

The server receives incoming channels via `dssh_chan_accept()`.
The library drives the setup state machine, populating a
`struct dssh_chan_params` as setup requests arrive — the same
struct type the client uses for `dssh_chan_open()`.  Each callback
receives the accumulated params so the app can see everything
the peer has sent so far.

```c
struct dssh_chan_accept_result {
    uint32_t        max_window;  /* 0 = default */
    dssh_chan_zc_cb  zc_cb;      /* NULL = stream mode */
    void            *zc_cbdata;
};

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

dssh_channel dssh_chan_accept(dssh_session sess,
    const struct dssh_chan_accept_cbs *cbs,
    int timeout_ms);
```

### Shared params struct

The library builds a `dssh_chan_params` internally during accept,
using the same builder functions the client app would use.  As
each setup request arrives, the library populates the struct:

- pty-req → `set_pty(true)`, `set_term()`, `set_size()`, modes
  populated via `set_mode()` for each opcode
- env → `add_env()` for each variable
- shell/exec/subsystem → type set, command/name stored

Every callback receives `const struct dssh_chan_params *params`
showing the accumulated state.  The pty_req callback sees the pty
fields just populated.  The env callback sees the full env array
so far.  The terminal request callback sees the complete picture.

After accept returns, the channel owns the populated params.
The getters (`get_type`, `get_command`, `get_pty`, etc.) read
from this struct.  Same struct, same accessors, regardless of
whether the channel was opened by the client or accepted by the
server.

This also benefits testing: in a selftest where client and server
are in the same process, the client's builder-constructed params
and the server's wire-populated params can be compared directly —
round-trip verification that the library serialized and parsed
correctly.

### Accept flow

1. `dssh_chan_accept()` blocks until CHANNEL_OPEN arrives
2. Library creates channel in setup mode, sends
   CHANNEL_OPEN_CONFIRMATION with `initial_window=0`
3. Library receives setup requests, populates params, calls
   app callbacks:
   - `pty_req` → return 0 to accept, negative to reject
     (CHANNEL_FAILURE sent, setup continues — channel works
     without a pty)
   - `env` → return 0 to accept, negative to reject
     (CHANNEL_FAILURE sent, setup continues)
4. Library receives terminal request, populates params, calls
   the matching callback:
   - `shell` / `exec` / `subsystem` → return 0 to accept, negative
     to reject
   - `result` is pre-filled with defaults (`max_window=0`,
     `zc_cb=NULL`).  The callback sets what it cares about.
     Return 0 without touching result = stream mode, default window.
5. On accept: library allocates buffers, sends WINDOW_ADJUST,
   returns live channel
6. On terminal request reject: channel closed, accept keeps waiting
   for the next CHANNEL_OPEN
7. On timeout or session terminate: returns NULL

The I/O model decision (stream vs ZC, max_window) is deferred to
the terminal request callback — the last possible moment, when the
app has full context: pty state, env vars, and channel purpose.

### Callback semantics

**NULL callback behavior depends on the request type:**

- `pty_req = NULL`: **auto-accept**.  Benign setup request; the
  app can query pty state via `dssh_chan_get_pty()` after accept.
- `env = NULL`: **auto-reject** (CHANNEL_FAILURE).  Uncontrolled
  env vars are a security hazard (RFC s6.4: `LD_PRELOAD`, `PATH`,
  etc.).  App must provide a callback and whitelist what it accepts.
- `shell = NULL`: **auto-reject** (CHANNEL_FAILURE).
- `exec = NULL`: **auto-reject** (CHANNEL_FAILURE).
- `subsystem = NULL`: **auto-reject** (CHANNEL_FAILURE).

Only `pty_req` is benign enough to auto-accept.  Everything else
defaults to reject so the app must explicitly declare what it
accepts.  The minimal useful server sets one terminal callback:

```c
struct dssh_chan_accept_cbs cbs = {
    .shell = my_shell_cb,
};
dssh_channel ch = dssh_chan_accept(sess, &cbs, -1);
```

This accepts shell channels (with whatever pty and env the peer
sends) and rejects exec and subsystem.

For ZC mode, the terminal request callback is required — it's the
only way to provide the `dssh_chan_accept_result` with `zc_cb`.

**Callbacks run on the app's thread** (inside `dssh_chan_accept`),
not the demux thread.  No `in_zc_rx` restriction.  But the channel
is in setup mode — read/write/poll return errors until accept
returns the live channel.

### Lifecycle enforcement

The library enforces the RFC and OpenSSH conventions during setup:

- **Env before terminal request** (RFC s6.4): env requests after
  the terminal request are rejected with CHANNEL_FAILURE
- **One terminal request per channel** (RFC s6.5): second
  shell/exec/subsystem is rejected with CHANNEL_FAILURE
- **Second pty-req**: reject and disconnect (matches OpenSSH)
- **Post-setup**: only window-change, break, signal accepted
  (delivered via the event queue, same as client)

### Getters

The channel carries its populated `dssh_chan_params` after setup.
These work for both client-opened and server-accepted channels:

```c
enum dssh_chan_type dssh_chan_get_type(dssh_channel ch);
const char *dssh_chan_get_command(dssh_channel ch);
const char *dssh_chan_get_subsystem(dssh_channel ch);
const struct dssh_pty_req *dssh_chan_get_pty(dssh_channel ch);
```

`get_command` returns NULL if the type isn't exec; `get_subsystem`
returns NULL if not subsystem; `get_pty` returns NULL if no pty
was requested.  All returned pointers are into channel-owned storage
and remain valid for the lifetime of the channel.

`dssh_chan_accept` also copies the session-level event callback
default (set via `dssh_session_set_event_cb`) to the new channel,
same as `dssh_chan_open` does.  After accept returns, post-setup
events (window-change, break, signal, EOF, close, exit-status,
exit-signal) arrive through the normal event queue / event callback,
same as for client-opened channels.

### Server initiating channels

Either side can open channels.  The server uses `dssh_chan_open()`
/ `dssh_chan_zc_open()` to initiate a channel to the peer — same
functions the client uses.  `dssh_chan_accept()` is only for
receiving incoming channels.

The `dssh_chan_` API is side-neutral.  "Client" and "server" only
matter for `dssh_session_init(client=true/false)` and the
auth layer.  The channel API works the same either way.

## Functions deleted (old → new mapping)

### Client channel I/O
- `dssh_session_read` → `dssh_chan_read(ch, 0, ...)`
- `dssh_session_read_ext` → `dssh_chan_read(ch, 1, ...)`
- `dssh_session_write` → `dssh_chan_write(ch, 0, ...)`
- `dssh_session_write_ext` → `dssh_chan_write(ch, 1, ...)`
- `dssh_session_poll` → `dssh_chan_poll(ch, ...)`
- `dssh_channel_poll` → `dssh_chan_poll(ch, ...)`
- `dssh_session_open_shell` → `dssh_chan_open(sess, &params)`
- `dssh_session_open_exec` → `dssh_chan_open(sess, &params)`
- `dssh_channel_open_subsystem` → `dssh_chan_open` or `dssh_chan_zc_open`
- `dssh_session_close` → `dssh_chan_close(ch, ...)`
- `dssh_channel_close` → `dssh_chan_close(ch, ...)`
- `dssh_channel_read` (old raw) → zc data callback
- `dssh_channel_write` (old raw) → `dssh_chan_zc_getbuf(ch, ...)` + `dssh_chan_zc_send(ch, ...)`
- `dssh_session_read_signal` → `dssh_chan_read_event(ch, ...)`
- `dssh_session_send_signal` → `dssh_chan_send_signal(ch, ...)`
- `dssh_session_send_window_change` → `dssh_chan_send_window_change(ch, ...)`

### Server accept
- `dssh_session_accept` → `dssh_chan_accept(sess, &cbs, timeout)`
- `dssh_session_reject` → eliminated (callback return values)
- `dssh_session_accept_channel` → `dssh_chan_accept`
- `dssh_channel_accept_raw` → `dssh_chan_accept` with `zc_cb` in result
- `dssh_parse_pty_req_data` → eliminated (library populates params)
- `dssh_parse_env_data` → eliminated (same)
- `dssh_parse_exec_data` → eliminated (same)
- `dssh_parse_subsystem_data` → eliminated (same)

New functions with no old equivalent:
- `dssh_chan_params_init` / `set_*` / `free` — builder API (client open)
- `dssh_chan_accept(sess, &cbs, timeout)` — server accept with callbacks
- `dssh_chan_get_type(ch)` — query channel type after accept
- `dssh_chan_get_command(ch)` — query exec command after accept
- `dssh_chan_get_subsystem(ch)` — query subsystem name after accept
- `dssh_chan_get_pty(ch)` — query pty state after accept
- `dssh_chan_shutwr(ch)` — half-close (send EOF)
- `dssh_chan_send_break(ch, ...)` — RFC 4335
- `dssh_chan_read_event(ch, ...)` — pull next event from queue
- `dssh_chan_set_event_cb(ch, ...)` — optional event notification callback
- `dssh_session_set_event_cb(sess, ...)` — session-level event callback default
- `dssh_chan_zc_getbuf(ch, ...)` — get pointer into tx_packet data area
- `dssh_chan_zc_send(ch, ...)` — fill header and send
- `dssh_chan_zc_cancel(ch)` — release tx_packet without sending

## Open items

### Client-side

None — all items resolved.

### Server-side

None — all items resolved.

## Resolved items

- **Channel type representation**: `enum dssh_chan_type` with values
  `DSSH_CHAN_SHELL`, `DSSH_CHAN_EXEC`, `DSSH_CHAN_SUBSYSTEM`.  Closed
  set (RFC s6.5: "only one of these requests can succeed per channel").
  Library maps enum to wire strings internally.

- **Callback setter timing**: both before and after open.  Session-
  level default callbacks set before opening channels; open copies
  session defaults to the new channel at creation time (before the
  window opens, so no race).  Per-channel overrides after open,
  protected by `cb_mtx`.

- **ZC status struct**: eliminated.  The `dssh_chan_zc_cb` takes data
  parameters directly (`stream`, `data`, `len`).  Events use a
  unified queue + `dssh_chan_read_event()` model, same for both
  stream and ZC modes.  No shared struct needed.

- **Receive-side lifecycle**: data and events are separate channels
  (signalfd model).  Data flows freely via read(); events are
  queued and pulled via poll(DSSH_POLL_EVENT) + read_event().
  Events carry advisory stream positions (bytes of unread
  stdout/stderr at poll time).  Data never stops at event boundaries.
  Optional event callback provides push notification but is never
  required.  All event types (signal, window-change, break, EOF,
  close, exit-status, exit-signal) go through the same queue.
  Eliminates the two-pointer problem (independent stdout/stderr
  read pointers can't support stream-position callbacks).

- **Per-event-type callbacks eliminated**: replaced by unified
  `dssh_chan_event_cb` (receives full event struct with positions)
  + `dssh_chan_read_event` (pulls typed event struct for poll-based
  path).  Simplifies callback surface from 7 setter pairs
  (session + channel) to 1.

- **RFC 4254 ordering constraints** (found on closer reading):
  - "Only one of these requests can succeed per channel" (s6.5) —
    one terminal request per channel
  - Env before terminal request (s6.4: "started later")
  - Terminal mode opcodes 160+ must come after 1-159 (s8)
  - RFC 8160: IUTF8 opcode 42; "transmit a value if it knows about
    one" — library should not guess mode defaults

- **Params struct design**: builder pattern with `dssh_chan_params`.
  Type in struct (enum).  PTY orthogonal to type.  All strings
  copied in, `free` releases them.  `init` sets type + defaults
  (term="dumb", 0x0, pty on for shell, off for exec/subsystem,
  zero terminal modes).  Consumed at open time, library keeps no
  references.  Modes: `set_mode` accumulates, dedup last-wins,
  encodes 1-159 then 160+ then TTY_OP_END.

- **Event lifecycle**: poll-freezes model.  `dssh_chan_poll()` with
  `DSSH_POLL_EVENT` computes and freezes the next event's stream
  positions.  `dssh_chan_read_event()` returns it.  One event per
  poll/read_event cycle; multiple events require multiple cycles
  (poll returns immediately if more are pending).  Events not
  consumed between polls are discarded — no memory leak, no
  unbounded queue.  String pointers valid until next read_event
  or poll.  Replaces the stale expiry model (which had race
  conditions between read and read_event).  Event callback
  (`dssh_chan_event_cb`) is an alternative path: receives the full
  event struct with positions computed at queue time.  Two scopes,
  both correct — app picks one model.

- **Error/termination mid-operation**: `zc_send` and `zc_cancel`
  always release `tx_mtx` on every code path, including errors.
  If the session terminates while the app is between getbuf and
  send, `zc_send` checks the terminate flag, releases `tx_mtx`,
  and returns `DSSH_ERROR_TERMINATED`.  If the app forgets to call
  send or cancel after getbuf, that's an app bug (same as
  forgetting to call `free`) — the library documents "getbuf must
  be followed by send or cancel."

- **Server-side accept API**: callback-driven setup via
  `dssh_chan_accept()`.  Per-request callbacks (pty_req, env,
  shell, exec, subsystem) — library drives the state machine,
  app just provides handlers.  NULL = auto-reject for everything
  except pty_req (benign).  App must explicitly declare what it
  accepts — no accidental open-everything servers, no unfiltered
  env vars (RFC s6.4 security hazard).  Terminal request callbacks
  receive
  `dssh_chan_accept_result` for I/O model selection (stream/ZC,
  max_window).  Callbacks run on app's thread, not demux.
  Library populates a `dssh_chan_params` during accept using the
  same builder functions the client uses — every callback receives
  the accumulated params.  Channel owns the params after accept.
  Getters read from it.  Same struct round-trips through the wire
  for testability.

- **Second pty-req**: reject and disconnect (matches OpenSSH).

- **Post-terminal-request env**: rejected with CHANNEL_FAILURE.
  Library enforces RFC s6.4 ordering (env before terminal request).

- **Params struct lifetime**: one-shot, consumed at open time.
  Mid-session pty-req/env re-sending is not supported — the
  library enforces one terminal request per channel and rejects
  setup requests after the terminal request.

- **Server initiating channels**: uses `dssh_chan_open()` /
  `dssh_chan_zc_open()` — same as client.  The channel API is
  side-neutral.

- **Parse helpers eliminated**: `dssh_parse_pty_req_data`,
  `dssh_parse_env_data`, `dssh_parse_exec_data`,
  `dssh_parse_subsystem_data` no longer needed — the library
  parses setup requests internally and passes structured data
  to accept callbacks.
