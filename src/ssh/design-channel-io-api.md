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

## Proposed API

Unify under `dssh_channel_*`.  The channel already knows its type.

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

Raw write returns `bufsz` on success (entire message sent), never partial.

### Behavioral differences (same function, documented)

**Read:**
- Session (stream): returns up to `bufsz` bytes from buffer.  Returns 0
  when empty (not EOF — EOF signaled via poll).
- Raw (message): returns one complete message up to `bufsz`.  Returns 0
  when queue empty.

**Write:**
- Session (stream): may return less than `bufsz` (window clamping).
  Caller must loop for complete delivery.
- Raw (message): atomic — sends `bufsz` as one message or returns error.
  Never partial.

**`_ext` on raw channels:** returns `DSSH_ERROR_INVALID`.  Extended data
is a session-channel concept (SSH_MSG_CHANNEL_EXTENDED_DATA).

### Peek: `dssh_channel_read(sess, ch, NULL, 0)`

- Raw: returns next message size without consuming (already works).
- Session: returns bytes available in stdout buffer (new).
- `dssh_channel_read_ext(sess, ch, NULL, 0)`: same for stderr.

### Poll

Merge `dssh_session_poll` and `dssh_channel_poll` into one
`dssh_channel_poll`.  Works on either channel type.
`DSSH_POLL_READEXT` and `DSSH_POLL_SIGNAL` never fire on raw channels
(return 0 in those bits, not an error).

### Signal and window-change

`dssh_session_read_signal`, `dssh_session_send_signal`,
`dssh_session_send_window_change` stay as-is — these are
session-channel-specific operations, not read/write.

Consider renaming to `dssh_channel_read_signal` etc. for prefix
consistency, returning `DSSH_ERROR_INVALID` on raw channels.

### Close

**Needs further design work.**  The split is genuine: session close
sends exit-status + EOF + CLOSE; raw close sends EOF + CLOSE.

Options under consideration:
1. **Separate `dssh_channel_send_exit_status()` + unified
   `dssh_channel_close(sess, ch)`.**  Most explicit.  Caller sends
   exit-status before close on session channels.
2. **`dssh_channel_close(sess, ch, exit_code)` where raw ignores it.**
   Simpler call site but a parameter that sometimes does nothing.
3. **Keep two close functions** with consistent naming
   (`dssh_channel_close` / `dssh_channel_close_session`).

## Functions deleted

- `dssh_session_read` — replaced by `dssh_channel_read`
- `dssh_session_read_ext` — replaced by `dssh_channel_read_ext`
- `dssh_session_write` — replaced by `dssh_channel_write`
- `dssh_session_write_ext` — replaced by `dssh_channel_write_ext`
- `dssh_session_poll` — replaced by `dssh_channel_poll`
- `dssh_session_close` — see Close section above
- `dssh_channel_close` (old raw-only) — see Close section above

## Implementation notes

- `dssh_channel_read` dispatches on `ch->chan_type`: bytebuf path for
  session, msgqueue path for raw.  This is the existing
  `session_read_impl` vs `dssh_channel_read` logic unified behind one
  entry point.
- `dssh_channel_write` similarly dispatches: `send_data` with `&sent`
  for session, `send_data` with `NULL` for raw (but change raw to also
  use `&sent` and return `(int64_t)sent`).
- Session peek: `bytebuf_readable()` under `buf_mtx`, return count.
  Distinct from poll (peek is instantaneous + returns byte count;
  poll waits + returns event mask).
