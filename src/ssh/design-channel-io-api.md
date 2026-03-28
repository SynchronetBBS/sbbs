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

## Design Decisions

### Two distinct I/O models, not one unified API

The original proposal unified both channel types under `dssh_channel_read`/
`dssh_channel_write` with behavioral differences based on channel type.
On reflection, session channels and subsystem channels have fundamentally
different usage patterns, and pretending they're the same thing conflates
two separate abstractions:

- **Session channels** (shell/exec): byte stream.  App reads/writes at
  its own pace from a window buffer.  Backpressure via the buffer.
- **Subsystem channels** (sftp, etc.): zero-copy callback.  Demux thread
  delivers CHANNEL_DATA payloads directly to the app's callback.  No
  intermediate buffer or queue.

The old "raw/message" model was wrong in a different way: it imposed SSH
message framing on what's really a byte stream at the channel layer.
CHANNEL_DATA carries an arbitrary byte blob; subsystem protocols (e.g.
SFTP's length-prefixed messages) do their own framing on top.  A callback
model lets the subsystem protocol handle its own reassembly.

### Max packet size: per-session, set once

`dssh_session_set_max_packet_size(sess, max_packet_size)` — called before
any channels are opened.  Used in every CHANNEL_OPEN and
CHANNEL_OPEN_CONFIRMATION.  Sizes internal buffers, so can only be set
once.

The application can't meaningfully choose per-channel: the protocol
requires max_packet_size in CHANNEL_OPEN / CHANNEL_OPEN_CONFIRMATION,
which happens before the CHANNEL_REQUEST that reveals the channel's
purpose (shell vs exec vs subsystem).  A single session-wide value avoids
over-engineering something the protocol can't support anyway.

### Max window size: per-channel, deferred to channel request

Unlike max packet size, the max window size *can* be deferred.  The
channel open exchange uses `initial_window=0`; no data flows until the
window is explicitly opened.  The CHANNEL_REQUEST for shell/exec/subsystem
is the first point where both sides know the channel's purpose.

This means the app can choose the window size with full knowledge:
- Interactive shell: modest window (e.g. 2 MiB)
- SFTP transfer: large window (e.g. 16 MiB)

Max window size is set once per channel.  It sizes internal buffers
(for stream channels) and cannot be changed after.

### Channel accept is the finalization point

For the **server**, the channel request callback is where accept/reject
happens.  This is the natural place to commit the window size:

- Channel request callback fires with channel info (type, command, etc.)
- App decides accept/reject, provides `max_window`
- Library allocates buffers, sends WINDOW_ADJUST, channel is live

For the **client**, a finalize function after sending the channel request
serves the same role — the client knows what the channel is for and
commits the window size.

Two paths, same result:

- **Server**: fill in `max_window` in the channel request callback
  (pointer-to-fill or struct field), return accept/reject.
- **Client**: `dssh_channel_open_shell(sess, ch, max_window)` or similar —
  sends CHANNEL_REQUEST and finalizes the window in one call.

### Subsystem data callback

For subsystem channels, the data callback is registered at the same
accept/finalize point.  Signature:

```c
int (*dssh_channel_data_cb)(dssh_session sess, dssh_channel ch,
    uint32_t data_type_code, const uint8_t *buf, size_t len,
    void *cbdata);
```

- `data_type_code == 0`: normal data (SSH_MSG_CHANNEL_DATA)
- `data_type_code == 1` (`SSH_EXTENDED_DATA_STDERR`): extended data
- Callback runs on the demux thread — must not block for long
- Return value signals consumed vs accumulating (see below)

For stream (session) channels, no callback — the app uses the buffered
read/write API.

### Window buffer as linear accumulation buffer

For subsystem channels, the window buffer is a **linear** (not ring)
buffer.  Incoming CHANNEL_DATA payloads are appended at the write
pointer.  The callback receives the entire accumulated contents as a
single contiguous pointer each time new data arrives:

1. CHANNEL_DATA payload written into window buffer at write pointer
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
The callback peeks at the accumulating data each time, and signals
consumed only when it has a complete protocol message.

Flow control falls out naturally: if the callback is accumulating a
large message, the window fills, WINDOW_ADJUST stops, and the sender
blocks.  No separate backpressure mechanism needed.

**Constraint**: a single subsystem protocol message cannot exceed the
max window size.  This is the app's choice (set at accept/finalize)
and a reasonable contract — the app knows its protocol's max message
size when it chooses the window.

## Stream channel API (shell/exec)

```c
int64_t dssh_channel_read(dssh_session sess,
    dssh_channel ch, uint8_t *buf, size_t bufsz);

int64_t dssh_channel_write(dssh_session sess,
    dssh_channel ch, const uint8_t *buf, size_t bufsz);

int64_t dssh_channel_read_ext(dssh_session sess,
    dssh_channel ch, uint8_t *buf, size_t bufsz);

int64_t dssh_channel_write_ext(dssh_session sess,
    dssh_channel ch, const uint8_t *buf, size_t bufsz);

int dssh_channel_poll(dssh_session sess,
    dssh_channel ch, int events, int timeout_ms);
```

### Return type: all read/write return `int64_t`

- `>= 0`: bytes transferred
- `< 0`: `DSSH_ERROR_*`

**Read:** returns up to `bufsz` bytes from the window buffer.  Returns 0
when empty (not EOF — EOF signaled via poll).

**Write:** may return less than `bufsz` (window clamping).  Caller must
loop for complete delivery.

### Peek: `dssh_channel_read(sess, ch, NULL, 0)`

Returns bytes available in stdout buffer without consuming.
`dssh_channel_read_ext(sess, ch, NULL, 0)`: same for stderr.

Distinct from poll (peek is instantaneous + returns byte count;
poll waits + returns event mask).

### Signal and window-change

`dssh_session_read_signal`, `dssh_session_send_signal`,
`dssh_session_send_window_change` stay as-is — these are
session-channel-specific operations, not read/write.

Consider renaming to `dssh_channel_read_signal` etc. for prefix
consistency, returning `DSSH_ERROR_INVALID` on subsystem channels.

### Close

**Needs further design work.**  Session close sends exit-status + EOF +
CLOSE; subsystem close sends EOF + CLOSE.

Options under consideration:
1. **Separate `dssh_channel_send_exit_status()` + unified
   `dssh_channel_close(sess, ch)`.**  Most explicit.  Caller sends
   exit-status before close on session channels.
2. **`dssh_channel_close(sess, ch, exit_code)` where subsystem ignores
   it.**  Simpler call site but a parameter that sometimes does nothing.
3. **Keep two close functions** with consistent naming
   (`dssh_channel_close` / `dssh_channel_close_session`).

## Functions deleted

- `dssh_session_read` — replaced by `dssh_channel_read`
- `dssh_session_read_ext` — replaced by `dssh_channel_read_ext`
- `dssh_session_write` — replaced by `dssh_channel_write`
- `dssh_session_write_ext` — replaced by `dssh_channel_write_ext`
- `dssh_session_poll` — replaced by `dssh_channel_poll`
- `dssh_channel_read` (old raw message-based) — replaced by callback
- `dssh_channel_write` (old raw message-based) — subsystem writes TBD
- `dssh_channel_poll` (old raw-only) — merged into unified poll
- `dssh_session_close` — see Close section above
- `dssh_channel_close` (old raw-only) — see Close section above

## Open items

- **Full channel lifecycle walkthrough**: end-to-end flow for both
  server and client, covering channel open → channel request →
  accept/finalize → I/O → close, for both stream and callback models.
- **Subsystem write API**: callback handles inbound data; outbound
  writes from subsystem code need a corresponding send function.
- **Client-side finalize API shape**: function calls vs struct, naming.
- **Close unification**: pick one of the three options above.
- **Poll on subsystem channels**: does it make sense when data arrives
  via callback?  Possibly only for EOF/close signaling.
