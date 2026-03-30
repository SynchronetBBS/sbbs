# Design Conformance Audit: design-channel-io-api.md

Audited implementation against every item in the design document.
Updated after fix pass (items 1-5, 7-10 fixed; 6, 11-12 noted).

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
- initial_window=0 in CHANNEL_OPEN and CHANNEL_OPEN_CONFIRMATION (lines 118, 251, 928)
- WINDOW_ADJUST sent after terminal request succeeds (lines 257, 943)
- Buffer allocation deferred until after terminal request (line 256)
- `term` field: `char *` with strdup, no truncation (line 183)
- `dssh_chan_get_pty` getter: returns `const struct dssh_chan_params *` (lines 1009, 1071)
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
- `in_zc_rx` guard on all TX functions (lines 672-688)
- WINDOW_ADJUST sent for ZC callback consumed bytes (lines 710-720)
- `dssh_session_set_event_cb` stores in session, propagates to new channels (lines 746-757)
- `cb_mtx` per-channel mutex protecting callback pointers (lines 639, 704-708)
- Half-close: dssh_chan_shutwr sends EOF (lines 722-728)
- Close: int64_t exit_code, negative = no exit-status (lines 730-744)
- Accept callback struct matches design exactly (lines 867-893)
- NULL pty_req = auto-accept (line 957)
- NULL env/shell/exec/subsystem = auto-reject (lines 959-964)
- Second pty-req = disconnect (line 996)
- Post-terminal env = CHANNEL_FAILURE (lines 992-993)
- One terminal request per channel (lines 994-995)
- All "functions deleted" items gone from public header (lines 1037-1063)
- All "new functions" items present (lines 1065-1079)
- Stream write uses ZC internals: zc_getbuf_inner + memcpy + zc_send_inner (lines 798-802)
- Stream RX uses internal stream_zc_cb copying to ring buffer (lines 786-791)
- tx_mac_scratch and rx_mac_scratch eliminated (lines 620-635)
- tx_packet/rx_packet 4-byte seq prefix for contiguous MAC input (lines 626-632)
- Event queue initialized before channel registration (immune to early demux dispatch)

## DELIBERATE DEVIATIONS

6. **`remote_window` not atomic (lines 607, 666-667)**
   Design: "`remote_window`... `atomic_uint32_t` -- no mutex needed."
   Implementation: `uint32_t remote_window` protected by `buf_mtx`.
   Functionally correct — both demux (WINDOW_ADJUST increment) and
   ZC send (deduction) hold `buf_mtx`.  The design's lock-free approach
   is a performance optimization for the ZC send path that would allow
   deducting without acquiring `buf_mtx`.  Deferred: converting to
   atomic requires careful review of all `remote_window` access sites.

11. **Event positions: design inconsistency (lines 347 vs 392)**
    DESIGN ERROR — the design contradicts itself:
    - Line 347: "bytes of stdout/stderr received before the event"
      (total received at queue time)
    - Line 392: "bytes of unread stdout/stderr at poll time"
      (currently buffered at poll time)
    These are different values.  The usage example (lines 402-410)
    uses `drain_stdout(ch, event.stdout_pos)` which makes sense only
    with the line 347 interpretation.
    Implementation follows line 347: positions are total bytes received
    at queue time.  This is correct for the intended usage pattern.
    Line 392 wording should be corrected in the design doc.

12. **Accept loops on terminal reject (line 945-946)**
    Design: "On terminal request reject: channel closed, accept keeps
    waiting for the next CHANNEL_OPEN."
    Implementation: `dssh_chan_accept` returns NULL on reject.  The
    looping version requires waiting for the peer's reciprocal CLOSE
    before freeing the channel, which needs careful demux synchronization
    to avoid use-after-free.  Noted with TODO in code.
