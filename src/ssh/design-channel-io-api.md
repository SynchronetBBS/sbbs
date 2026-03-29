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

Specific gaps in the RFC:
- No restriction on sending data before any CHANNEL_REQUEST
- No restriction on multiple pty-req, shell, exec, or subsystem requests
- No defined relationship between EOF, exit-status, and CLOSE
- Signal and window-change are independent mechanisms with no defined
  interaction (SIGWINCH vs window-change are the same event split into
  two unrelated request types)
- Flow control (SSH window) has no defined relationship to process I/O;
  the RFC doesn't address what happens when the window fills and a
  process is still producing output

### What OpenSSH actually does (the de facto spec)

**Server side** (`session.c`, `channels.c`, `serverloop.c`):

1. CHANNEL_OPEN received → create channel in **LARVAL** state with
   **`initial_window=0`** — no data can flow
2. Send CHANNEL_OPEN_CONFIRMATION with `initial_window=0`
3. While LARVAL: accept setup requests (pty-req, env, x11-req,
   auth-agent-req, shell, exec, subsystem)
4. shell/exec/subsystem → `do_exec()` → fork process →
   `channel_set_fds()` → transition to OPEN, set window to
   `CHAN_SES_WINDOW_DEFAULT` (2 MiB), send WINDOW_ADJUST
5. After OPEN: only window-change, break, signal accepted
6. Second pty-req = **protocol disconnect** (not reject — kills the
   whole session)

**Client side** (`ssh.c`):

- Sends `CHAN_SES_WINDOW_DEFAULT` (2 MiB) in CHANNEL_OPEN — non-zero,
  unlike the server
- Halves window and packet size for PTY sessions (1 MiB / 16 KiB)
- No LARVAL state — fds attached at channel creation, data written
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

DeuceSSH should follow the OpenSSH server model (LARVAL + zero window)
on **both** sides.  This is strictly safer than the OpenSSH client
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

- **Stream** (`dssh_chan_*`): byte-stream with ring buffers.  App
  reads/writes at its own pace.  POSIX read/write semantics.
- **Zero-copy** (`dssh_chan_zc_*`): inbound data delivered via callback;
  outbound writes go directly into the transport packet buffer.
  No intermediate copies.

### Channel type as parameter, not separate functions

`dssh_chan_open()` and `dssh_chan_zc_open()` each take a channel type
constant (shell/exec/subsystem) and a union pointer for type-specific
parameters.  NULL means defaults.  This replaces the current
`open_shell` / `open_exec` / `open_subsystem` split.

```c
dssh_channel dssh_chan_open(dssh_session sess,
    int type, const union dssh_chan_params *params,
    uint32_t max_window);

dssh_channel dssh_chan_zc_open(dssh_session sess,
    int type, const union dssh_chan_params *params,
    uint32_t max_window,
    dssh_chan_zc_data_cb data_cb, void *cbdata);
```

Type constants (representation TBD — could be int, could be
`const char[]` matching the wire name):

- `DSSH_CHAN_SHELL` — pty-req (from params or defaults) + shell request
- `DSSH_CHAN_EXEC` — exec request with command string
- `DSSH_CHAN_SUBSYSTEM` — subsystem request with name

The params union:

```c
union dssh_chan_params {
    struct dssh_pty_req shell;    /* DSSH_CHAN_SHELL */
    struct {                      /* DSSH_CHAN_EXEC */
        const char *command;
    } exec;
    struct {                      /* DSSH_CHAN_SUBSYSTEM */
        const char *name;
    } subsystem;
};
```

`max_window` with value 0 means library default (currently 2 MiB,
matching OpenSSH's `CHAN_SES_WINDOW_DEFAULT`).

### Open does the full LARVAL → OPEN sequence

Both `dssh_chan_open()` and `dssh_chan_zc_open()` perform the complete
channel lifecycle internally:

1. Send CHANNEL_OPEN "session" with `initial_window=0`
2. Wait for CHANNEL_OPEN_CONFIRMATION
3. Send setup requests (pty-req for shell, etc.) from params
4. Send terminal request (shell/exec/subsystem)
5. Allocate I/O buffers (stream: ring buffers; zc: linear buffer)
6. Send WINDOW_ADJUST to open the window
7. Return live channel

The app calls one function and gets a ready-to-use channel.  The
internal implementation is decomposed into steps so a lower-level
API can be exposed later if needed, without rewriting the guts.

### Max packet size: stays in `dssh_session_init()`

Max packet size is a transport-layer concern (packet framing), not a
channel concern.  It's used immediately at session init to allocate
transport buffers.  Stays as the second arg to `dssh_session_init()`.

### Max window size: per-channel, at open time

The `max_window` parameter on `dssh_chan_open()` / `dssh_chan_zc_open()`
controls buffer allocation and the WINDOW_ADJUST sent after the
terminal request succeeds.  Pass 0 for the library default.

## Stream API (`dssh_chan_*`)

```c
dssh_channel dssh_chan_open(dssh_session sess,
    int type, const union dssh_chan_params *params,
    uint32_t max_window);

int64_t dssh_chan_read(dssh_session sess,
    dssh_channel ch, uint8_t *buf, size_t bufsz);

int64_t dssh_chan_write(dssh_session sess,
    dssh_channel ch, const uint8_t *buf, size_t bufsz);

int64_t dssh_chan_read_ext(dssh_session sess,
    dssh_channel ch, uint8_t *buf, size_t bufsz);

int64_t dssh_chan_write_ext(dssh_session sess,
    dssh_channel ch, const uint8_t *buf, size_t bufsz);

int dssh_chan_poll(dssh_session sess,
    dssh_channel ch, int events, int timeout_ms);
```

### Return type: all read/write return `int64_t`

- `>= 0`: bytes transferred
- `< 0`: `DSSH_ERROR_*`

POSIX semantics: read returns what's available (up to bufsz), write
returns what was accepted (may be less than bufsz due to window
clamping).  Caller loops for complete delivery.

### Peek: `dssh_chan_read(sess, ch, NULL, 0)`

Returns bytes available in stdout buffer without consuming.
`dssh_chan_read_ext(sess, ch, NULL, 0)`: same for stderr.

### Half-close: `dssh_chan_shutwr()`

```c
int dssh_chan_shutwr(dssh_session sess, dssh_channel ch);
```

Sends SSH_MSG_CHANNEL_EOF.  Stops writes, keeps reading.
`shutdown(fd, SHUT_WR)` semantics — the name makes the direction
unambiguous (unlike `dssh_chan_eof()` which could mean send or receive).

### Close: `dssh_chan_close()`

```c
int dssh_chan_close(dssh_session sess, dssh_channel ch,
    int64_t exit_code);
```

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

### Stream-position callbacks: signal, window-change, break

All three post-setup channel requests are delivered via callbacks that
fire at the correct **stream position** during `dssh_chan_read()`.
This matters because the data before and after a window-change may be
formatted for different terminal dimensions — the resize must be
processed between the right bytes, not when the wire message arrives.

```c
typedef void (*dssh_chan_signal_cb)(dssh_session sess,
    dssh_channel ch, const char *signal_name, void *cbdata);

typedef void (*dssh_chan_window_change_cb)(dssh_session sess,
    dssh_channel ch, uint32_t cols, uint32_t rows,
    uint32_t wpx, uint32_t hpx, void *cbdata);

typedef void (*dssh_chan_break_cb)(dssh_session sess,
    dssh_channel ch, uint32_t length, void *cbdata);
```

Setters (call before or after open — TBD):

```c
void dssh_chan_set_signal_cb(dssh_session sess, dssh_channel ch,
    dssh_chan_signal_cb cb, void *cbdata);

void dssh_chan_set_window_change_cb(dssh_session sess, dssh_channel ch,
    dssh_chan_window_change_cb cb, void *cbdata);

void dssh_chan_set_break_cb(dssh_session sess, dssh_channel ch,
    dssh_chan_break_cb cb, void *cbdata);
```

The library records each event with a stream position mark (byte offset
in the stdout buffer at the time the CHANNEL_REQUEST arrives).  When
`dssh_chan_read()` advances the read pointer past a mark, the
corresponding callback fires before returning the data after that point.

Window-change = SIGWINCH: it has a position in the stream and must be
delivered between the data formatted for the old and new dimensions.
Signal and break follow the same model.

Outbound signal/window-change/break are sent via:

```c
int dssh_chan_send_signal(dssh_session sess, dssh_channel ch,
    const char *signal_name);

int dssh_chan_send_window_change(dssh_session sess, dssh_channel ch,
    uint32_t cols, uint32_t rows, uint32_t wpx, uint32_t hpx);

int dssh_chan_send_break(dssh_session sess, dssh_channel ch,
    uint32_t length);
```

## Zero-copy API (`dssh_chan_zc_*`)

### Open

```c
dssh_channel dssh_chan_zc_open(dssh_session sess,
    int type, const union dssh_chan_params *params,
    uint32_t max_window,
    dssh_chan_zc_data_cb data_cb, void *cbdata);
```

Same LARVAL → OPEN sequence as `dssh_chan_open()`, but allocates a
linear accumulation buffer instead of ring buffers, and stores the
data callback.

### Inbound data: callback

```c
typedef int (*dssh_chan_zc_data_cb)(dssh_session sess,
    dssh_channel ch, uint32_t data_type_code,
    const uint8_t *buf, size_t len, void *cbdata);
```

- `data_type_code == 0`: stdout (SSH_MSG_CHANNEL_DATA)
- `data_type_code == 1`: stderr (SSH_MSG_CHANNEL_EXTENDED_DATA)
- Callback runs on the demux thread — must not block for long
- Return value signals consumed vs accumulating (see linear
  accumulation buffer below)

**Open issue**: the current linear accumulation buffer design does not
track stdout vs stderr separately.  CHANNEL_DATA and
CHANNEL_EXTENDED_DATA arrive interleaved and share the same window, but
the callback needs to know which stream each byte belongs to.  TBD:
may need separate accumulation for each stream, or deliver each
CHANNEL_DATA/EXTENDED_DATA payload individually without accumulation.

### Outbound data: direct buffer access

Instead of the app providing data that the library copies into a
packet, the app writes directly into the transport packet buffer:

```c
uint8_t *dssh_chan_zc_write_buf(dssh_session sess,
    dssh_channel ch, int stream);

int dssh_chan_zc_send(dssh_session sess,
    dssh_channel ch, int stream, size_t len);
```

`dssh_chan_zc_write_buf()` returns a pointer to the data area of the
pre-allocated `tx_packet`, after the header bytes.  The app writes
payload data directly there.  `dssh_chan_zc_send()` fills in the
header (msg type, channel ID, data_type_code if ext, length) and
sends the packet.  `stream` selects stdout (0) or stderr (1).

Zero memcpy on the send path (app writes directly into the packet
buffer; `send_packet_inner` encrypts in-place).

### Linear accumulation buffer

For zero-copy channels, the window buffer is a **linear** (not ring)
buffer.  Incoming CHANNEL_DATA payloads are appended at the write
pointer.  The callback receives the entire accumulated contents as a
single contiguous pointer each time new data arrives:

1. CHANNEL_DATA payload written into buffer at write pointer
   (single copy from network — unavoidable)
2. Write pointer advances
3. Callback fires with `(buf, total_accumulated_len)`
4. Callback returns **consumed**: write pointer resets to 0, library
   sends WINDOW_ADJUST for the consumed amount, full buffer available
   again
5. Callback returns **not yet**: data stays in buffer, next
   CHANNEL_DATA appends after it, callback fires again with the
   larger accumulated view

This means the subsystem protocol (e.g. SFTP) doesn't need its own
reassembly buffer — the window buffer *is* the reassembly buffer.
Flow control falls out naturally: if the callback is accumulating a
large message, the window fills, WINDOW_ADJUST stops, and the sender
blocks.

**Constraint**: a single subsystem protocol message cannot exceed the
max window size.

### Close

`dssh_chan_close()` works on both stream and zero-copy channels.
Same `int64_t exit_code` semantics.

### Shared functions

The following work on both stream and zero-copy channels:

- `dssh_chan_poll()` — for zero-copy, useful for EOF/close signaling
  and write-readiness (DSSH_POLL_READ may not be meaningful)
- `dssh_chan_shutwr()`
- `dssh_chan_close()`
- `dssh_chan_send_signal()`
- `dssh_chan_send_window_change()`
- `dssh_chan_send_break()`

Signal/window-change/break callbacks also work on zero-copy channels
(stream position marks are tracked against the accumulation buffer).

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
temp→tx_packet (pre-alloc), free temp.  One malloc per packet.

### New (stream)

Same as current stream.  TODO item 101 will eliminate the send-path
malloc by teaching `send_packet_inner()` to accept scatter (header +
data).

### New (zero-copy)

| Path | Zero-copy |
|------|-----------|
| **Receive** | 0 mallocs, 1 copy |
| **Send** | 0 mallocs, 0 copies |

Receive: decrypt→rx_packet, rx_packet→linear buffer, callback gets
pointer into linear buffer.  One copy (unavoidable: decryption
requires the rx_packet buffer).

Send: app writes directly into tx_packet via `zc_write_buf()`,
`zc_send()` fills header and encrypts in-place.  Zero copies.

## Server-side API

TBD — to be designed separately.  Key differences from client:

- Channel type comes *out* of the accept, not in
- Either side can initiate channels (server/client naming breaks down)
- Accept path needs to expose the LARVAL → OPEN transition
  with the type determined by the incoming terminal request

The parse helpers (`dssh_parse_pty_req_data`, `dssh_parse_env_data`,
`dssh_parse_exec_data`, `dssh_parse_subsystem_data`) remain useful
for server-side setup request processing.

## Functions deleted (old → new mapping)

- `dssh_session_read` → `dssh_chan_read`
- `dssh_session_read_ext` → `dssh_chan_read_ext`
- `dssh_session_write` → `dssh_chan_write`
- `dssh_session_write_ext` → `dssh_chan_write_ext`
- `dssh_session_poll` → `dssh_chan_poll`
- `dssh_channel_poll` → `dssh_chan_poll`
- `dssh_session_open_shell` → `dssh_chan_open(sess, DSSH_CHAN_SHELL, ...)`
- `dssh_session_open_exec` → `dssh_chan_open(sess, DSSH_CHAN_EXEC, ...)`
- `dssh_channel_open_subsystem` → `dssh_chan_open` or `dssh_chan_zc_open`
- `dssh_session_close` → `dssh_chan_close`
- `dssh_channel_close` → `dssh_chan_close`
- `dssh_channel_read` (old raw) → `dssh_chan_zc_open` callback
- `dssh_channel_write` (old raw) → `dssh_chan_zc_write_buf` + `dssh_chan_zc_send`
- `dssh_session_read_signal` → stream-position callback
- `dssh_session_send_signal` → `dssh_chan_send_signal`
- `dssh_session_send_window_change` → `dssh_chan_send_window_change`

New functions with no old equivalent:
- `dssh_chan_shutwr` — half-close (send EOF)
- `dssh_chan_send_break` — RFC 4335
- `dssh_chan_set_signal_cb` — replaces pull-based `read_signal`
- `dssh_chan_set_window_change_cb` — replaces direct send
- `dssh_chan_set_break_cb` — new
- `dssh_chan_zc_write_buf` — direct packet buffer access
- `dssh_chan_zc_send` — send with header fill

## Open items

- **Server-side accept API**: type comes out, LARVAL → OPEN transition,
  either side can initiate.  Design separately.
- **Zero-copy inbound stdout/stderr separation**: the linear
  accumulation buffer doesn't track which stream each byte belongs to.
  Options: separate buffers per stream, or deliver each payload
  individually to the callback without accumulation.
- **Channel type representation**: `int` constants, `const char[]`
  matching wire names, or something else.
- **Callback setter timing**: before open only, or also after?
- **`dssh_chan_zc_write_buf` locking**: must hold tx_mtx to write into
  `tx_packet`.  How does this interact with the rekey wait?
