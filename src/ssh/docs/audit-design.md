# Design Conformance Audit: design-channel-io-api.md

Fresh re-audit of implementation against every design claim.
Previous items 1-5, 7-10 fixed; 6, 11 resolved; 12 noted.

## PASS

### Naming (design lines 130-137)
- `dssh_chan_` prefix on all channel functions
- `dssh_chan_zc_` prefix on ZC functions

### Two I/O models (design lines 139-155)
- Separate open functions: `dssh_chan_open` (stream) and `dssh_chan_zc_open` (ZC)
- Stream mode registers internal `stream_zc_cb` (ssh-conn.c:2517)
- ZC mode uses app-supplied callback (ssh-conn.c:2882)

### Channel type (design lines 157-168)
- `enum dssh_chan_type` with SHELL/EXEC/SUBSYSTEM (deucessh-conn.h:44-48)

### Params builder (design lines 170-235)
- `dssh_chan_params_init` sets type and defaults: term="dumb", dims 0x0,
  pty on for shell only, zero modes (ssh-conn.c:366-378)
- `dssh_chan_params_free` releases all heap storage (ssh-conn.c:381-403)
- All 10 setter functions present (ssh-conn.c:406-531)
- Mode dedup last-wins (ssh-conn.c:457-461)
- Mode encoding: opcodes 1-159 before 160+, then TTY_OP_END (ssh-conn.c:2347-2362)

### Open functions (design lines 237-268)
- Both open functions perform full setup sequence:
  CHANNEL_OPEN(initial_window=0) → env → pty-req → terminal request →
  allocate buffers → WINDOW_ADJUST (ssh-conn.c:2528-2558, 2894-2918)
- initial_window=0 in both CHANNEL_OPEN and CHANNEL_OPEN_CONFIRMATION

### Max packet/window (design lines 264-276)
- Max packet stays in `dssh_session_init` (transport concern)
- Max window per-channel in params; default INITIAL_WINDOW_SIZE = 2 MiB (ssh-conn.c:535)

### Stream select (design lines 278-284)
- `stream` parameter (0=stdout, 1=stderr) instead of `_ext` variants

### API summary — stream (design lines 296-309)
- `dssh_chan_open`, `dssh_chan_read`, `dssh_chan_write`, `dssh_chan_poll`
- All return types match design (int64_t for read/write, int for poll)
- All take `dssh_channel` only (no session handle)

### API summary — ZC (design lines 311-328)
- `dssh_chan_zc_open`, `dssh_chan_zc_getbuf`, `dssh_chan_zc_send`, `dssh_chan_zc_cancel`
- `zc_getbuf` stashes stream, `zc_send` uses it (ssh-conn.c:2245, 2257)

### Shared functions (design lines 330-358)
- All 7 present: shutwr, close, send_signal, send_window_change,
  send_break, read_event, set_event_cb
- `dssh_chan_read_event` returns frozen event (ssh-conn.c:2681)
- `dssh_chan_set_event_cb` protected by cb_mtx (ssh-conn.c:2955)

### Event struct (design lines 360-388)
- All 7 event types defined (deucessh-conn.h:120-128)
- Union members match design: signal, window_change, brk, exit_status, exit_signal
- stdout_pos/stderr_pos fields present

### Event lifecycle (design lines 390-428)
- Poll-freezes model: `event_queue_freeze` copies entry (ssh-conn.c:351)
- Poll overwrites frozen positions with current `.used` (unread bytes at poll time)
- One event per poll/read_event cycle
- Event discard on re-poll (stale frozen event replaced)
- String pointers valid until next read_event or poll

### Callback types (design lines 429-461)
- `dssh_chan_zc_cb` typedef matches design (deucessh-conn.h:161)
- `dssh_chan_event_cb` typedef matches design (deucessh-conn.h:167)

### Stream API details (design lines 463-553)
- Read/write return int64_t: >0 bytes, 0 EOF, <0 error
- Peek: `dssh_chan_read(ch, stream, NULL, 0)` returns available (ssh-conn.c:2600)
- Data and events separate (signalfd model)
- Position-aware and position-unaware usage patterns both supported

### ZC API details (design lines 555-720)
- Inbound: callback receives pointer into rx_packet, valid only during callback
- Outbound: `zc_getbuf` acquires tx_mtx, computes max_len from
  min(remote_window, remote_max_packet, transport buffer) (ssh-conn.c:2212-2218)
- tx_packet layout: [seq(4)][pkt_len(4)][pad_len(1)][chan_hdr][data][pad][mac]
- tx_mac_scratch and rx_mac_scratch eliminated; contiguous MAC input via seq prefix
- Lock order: channel_mtx → buf_mtx → cb_mtx → tx_mtx (ssh-internal.h:262)
- `_Thread_local in_zc_rx` guards all TX functions from RX callback context
- WINDOW_ADJUST sent for ZC callback consumed bytes
- `remote_window` is `atomic_uint_least32_t` with CAS saturating helpers;
  `zc_send_inner` deducts atomically without buf_mtx

### Shared function details (design lines 722-757)
- `dssh_chan_shutwr` sends EOF (ssh-conn.c:2706)
- `dssh_chan_close(ch, exit_code)`: exit_code ≥ 0 sends exit-status + EOF + CLOSE;
  exit_code < 0 sends EOF + CLOSE only (ssh-conn.c:2710-2718)
- `dssh_session_set_event_cb` stores default; propagated to new channels at
  open/accept time (ssh-conn.c:2518, 2884, 3151)
- Per-channel override via `dssh_chan_set_event_cb` protected by cb_mtx

### Implementation architecture (design lines 759-855)
- Public ZC functions validate + call inner; stream layer calls inner directly
- Stream RX: internal `stream_zc_cb` copies into ring buffer, broadcasts poll_cnd
- Stream TX: `dssh_chan_write` → `zc_getbuf_inner` + memcpy + `zc_send_inner`
- Stream poll: waits on poll_cnd for ring buffer state changes
- buf_mtx released before ZC and event callbacks (demux thread)

### Server accept — structure (design lines 857-999)
- `dssh_chan_accept_cbs` struct matches design (deucessh-conn.h:256-271)
- `dssh_chan_accept_result` struct present (deucessh-conn.h:171-177)
- NULL pty_req = auto-accept; NULL env/shell/exec/subsystem = auto-reject
- Second pty-req = protocol disconnect (ssh-conn.c:3069)
- Post-terminal env = CHANNEL_FAILURE (ssh-conn.c:3081)
- One terminal request per channel (ssh-conn.c:3089, 3099, 3110)
- Library populates dssh_chan_params during accept using builder functions
- Callbacks run on app thread (inside dssh_chan_accept), not demux thread
- Session event callback default inherited by accepted channels (ssh-conn.c:3151)
- `result.zc_cb` non-NULL selects ZC mode; NULL selects stream mode
  (ssh-conn.c:3188-3192).  Ring buffers allocated only for stream mode.

### Getters (design lines 1000-1016)
- `dssh_chan_get_type`, `dssh_chan_get_command`, `dssh_chan_get_subsystem`,
  `dssh_chan_has_pty`, `dssh_chan_get_pty` all implemented (ssh-conn.c:2809-2815)
- Work for both client-opened and server-accepted channels (params stored on channel)

### Functions deleted (design lines 1035-1080)
- All old API functions removed from public headers
- All new API functions present

## DEVIATIONS

None.
