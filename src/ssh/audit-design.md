# Design Conformance Audit: design-channel-io-api.md

Audited implementation against every item in the design document.

## PASS

- `dssh_chan_` prefix on all channel functions (line 132)
- `dssh_chan_zc_` prefix on ZC functions (line 137)
- Two separate open functions: stream + ZC (lines 139-155)
- `enum dssh_chan_type` with SHELL/EXEC/SUBSYSTEM (lines 157-168)
- All 10 params builder functions (lines 191-213)
- Init defaults: term="dumb", dims 0x0, pty on for shell only, zero modes (lines 215-222)
- Mode dedup last-wins (line 224)
- Mode encoding: 1-159 before 160+, then TTY_OP_END (lines 225-226)
- Open sequence order: env, pty-req, terminal request (lines 253-255)
- Stream API signatures: read/write return int64_t, stream param, ch-only (lines 296-309)
- ZC API signatures: getbuf/send/cancel (lines 311-324)
- zc_getbuf stashes stream, zc_send uses it (lines 326-328)
- All 7 shared control functions present (lines 330-357)
- All 7 event types defined (lines 360-387)
- Event struct union members match design (lines 372-387)
- Event discard on re-poll (lines 413-415)
- Both callback typedefs match design signatures (lines 429-461)
- Peek via `dssh_chan_read(ch, stream, NULL, 0)` (lines 479-482)
- Data and events separate, signalfd model (lines 484-495)
- ZC getbuf: acquires tx_mtx, rekey wait, min(window,max_pkt), correct offset (lines 584-591)
- ZC send: fills header at tx_packet[9], calls tx_finalize (lines 598-608)
- buf_mtx released before ZC/event callbacks (lines 658-662)
- `_Thread_local in_zc_rx` exists and guards ZC RX callback (lines 682-688)
- Half-close: dssh_chan_shutwr sends EOF (lines 722-728)
- Close: int64_t exit_code, negative = no exit-status (lines 730-744)
- Accept callback struct matches design exactly (lines 867-893)
- NULL pty_req = auto-accept (line 957)
- NULL env/shell/exec/subsystem = auto-reject (lines 959-964)
- Second pty-req = disconnect (line 996)
- Post-terminal env = CHANNEL_FAILURE (lines 992-993)
- One terminal request per channel (lines 994-995)
- All "functions deleted" items gone from public header (lines 1037-1063)
- All "new functions" items present except dssh_chan_get_pty (lines 1065-1079)
- Stream write uses ZC internals: zc_getbuf_inner + memcpy + zc_send_inner (lines 798-802)
- Stream RX uses internal stream_zc_cb copying to ring buffer (lines 786-791)
- tx_mac_scratch and rx_mac_scratch eliminated (lines 620-635)
- tx_packet/rx_packet 4-byte seq prefix for contiguous MAC input (lines 626-632)

## FAIL

1. **initial_window=0 (lines 118, 251, 928)**
   Design: both sides send `initial_window=0` in CHANNEL_OPEN and
   CHANNEL_OPEN_CONFIRMATION.  Implementation: sends INITIAL_WINDOW_SIZE
   (0x200000 = 2 MiB).  `open_session_channel` sets
   `ch->local_window = INITIAL_WINDOW_SIZE` and sends it on the wire.
   `accept_channel_init` does the same in the confirmation.

2. **WINDOW_ADJUST after setup (lines 257, 943)**
   Design: step 7 of open is "Send WINDOW_ADJUST to open the window."
   Accept step 5: "library allocates buffers, sends WINDOW_ADJUST."
   Implementation: neither `dssh_chan_open` nor `dssh_chan_accept` sends
   WINDOW_ADJUST after the terminal request succeeds.  Data flows
   immediately because initial_window is non-zero (consequence of item 1).

3. **Buffer allocation timing (line 256)**
   Design: step 6 "Allocate I/O buffers (stream: ring buffers; zc: none)"
   happens after the terminal request succeeds.  Client-side
   `dssh_chan_open` calls `init_session_channel` (which allocates
   bytebufs) BEFORE `open_session_channel`, i.e., before CHANNEL_OPEN is
   even sent.  Server-side `dssh_chan_accept` allocates after the setup
   loop -- correct for accept, wrong for open.

4. **`term` field truncation (line 183)**
   Design: `struct dssh_pty_req pty` with `const char *term` (dynamic
   string, no length limit).  Implementation: `char term[64]` fixed
   buffer with silent truncation.  RFC 4254 s6.2 defines TERM as an SSH
   `string` with no length limit.  A 64-byte cap silently loses data.

5. **`dssh_chan_get_pty` getter (lines 1009, 1071)**
   Design: `const struct dssh_pty_req *dssh_chan_get_pty(dssh_channel ch)`
   returns NULL if no pty, or pointer to full PTY details (term, size,
   modes).  Implementation: `bool dssh_chan_has_pty(dssh_channel ch)`.
   No way to retrieve PTY details (term, cols, rows, modes) after accept.

6. **`remote_window` not atomic (lines 607, 666-667)**
   Design: "`remote_window`... `atomic_uint32_t` -- no mutex needed."
   ZC send deducts atomically.  Implementation: `uint32_t remote_window`
   (not atomic) protected by `buf_mtx`.  The demux thread increments it
   (WINDOW_ADJUST) under buf_mtx, and zc_send_inner deducts under
   buf_mtx.  Functionally correct with the mutex but differs from the
   design's lock-free approach.

7. **`cb_mtx` per-channel mutex (lines 639, 704-708)**
   Design: "cb_mtx is a per-channel mutex protecting all callback
   function pointer + cbdata pairs."  Lock order includes it:
   `channel_mtx -> buf_mtx -> cb_mtx -> tx_mtx`.  Not implemented.
   Callback pointers (`zc_cb`, `event_cb`) are read/written without
   synchronization.

8. **`in_zc_rx` guard on all TX functions (lines 672-688)**
   Design: "All TX-path functions check it and return DSSH_ERROR_INVALID
   if set."  Lists: `zc_getbuf`, `zc_send`, `zc_cancel`, `send_signal`,
   `send_window_change`, `send_break`, `shutwr`, `close`.
   Implementation: only `dssh_chan_zc_getbuf` and `dssh_chan_zc_send`
   check `in_zc_rx`.  The other 6 TX functions do not.

9. **WINDOW_ADJUST from ZC callback return (lines 710-720)**
   Design: "The library sends WINDOW_ADJUST for the consumed amount
   after the callback returns.  This requires tx_mtx."
   Implementation: the callback return value is stored in `consumed`
   and deducted from `local_window`, but no WINDOW_ADJUST is sent.
   Replenishment only happens via `maybe_replenish_window` which is
   called from `dssh_chan_read` (stream path only, not ZC path).

10. **`dssh_session_set_event_cb` stub (lines 746-757)**
    Design: stores session-level default; `dssh_chan_open`/`dssh_chan_zc_open`
    copies to new channel at creation time.
    Implementation: function exists but ignores arguments (`(void)cb;
    (void)cbdata;`) with a TODO comment.  No field in `dssh_session_s`.
    Session-level event callback default not stored or propagated.

11. **Event positions: poll-time vs queue-time (lines 392-394)**
    Design: "`dssh_chan_poll()` with `DSSH_POLL_EVENT` computes and
    freezes the next pending event's stream positions (bytes of unread
    stdout/stderr at poll time)."
    Implementation: positions are computed at queue time (when the demux
    thread pushes the event, using `stdout_buf.total`/`stderr_buf.total`)
    and frozen as-is at poll time.  These are total-bytes-received, not
    bytes-of-unread.

12. **Accept loops on terminal reject (line 945-946)**
    Design: "On terminal request reject: channel closed, accept keeps
    waiting for the next CHANNEL_OPEN."
    Implementation: `dssh_chan_accept` returns NULL when
    `chan_accept_setup_loop` returns DSSH_ERROR_REJECTED.  Does not
    loop back to wait for the next incoming channel.
