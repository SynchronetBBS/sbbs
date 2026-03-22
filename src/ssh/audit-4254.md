# RFC 4254 Conformance Audit — DeuceSSH

Systematic audit of every MUST, MUST NOT, REQUIRED, SHALL, SHALL NOT,
SHOULD, SHOULD NOT, RECOMMENDED, MAY, and OPTIONAL keyword in RFC 4254
(SSH Connection Protocol) against the DeuceSSH library code.

DeuceSSH currently provides a thin client-side connection API
(`ssh-conn.c`) and server-side channel handling is done directly by
the application (`server.c`).  This audit identifies what exists, what's
missing, and what should be library functions vs. application code.

Legend:
- **CONFORMS** — library satisfies the requirement
- **PARTIAL** — partially implemented
- **NOT IMPLEMENTED** — feature not present
- **N/A** — not applicable or not planned
- **APPLICATION** — application's responsibility

---

## Section 4: Global Requests

### 4-1
> Both the client and server **MAY** send global requests at any time,
> and the receiver **MUST** respond appropriately.

**CONFORMS** — `recv_packet()` intercepts SSH_MSG_GLOBAL_REQUEST,
parses the request name and `want_reply` flag, and for `want_reply=true`
sends SSH_MSG_REQUEST_FAILURE (or SUCCESS if the optional
`global_request_cb` callback returns >= 0).  Requests are consumed
transparently — the application never sees them unless it sets a
callback.

### 4-2
> Replies to SSH_MSG_GLOBAL_REQUESTS **MUST** be sent in the same order
> as the corresponding request messages.

**CONFORMS** — Global requests are handled inline in `recv_packet()`
one at a time.  Each request is fully processed (callback + reply)
before the next packet is read, preserving order.

---

## Section 5.1: Opening a Channel

### 5.1-1 (Client sends CHANNEL_OPEN)

**CONFORMS** — `deuce_ssh_conn_open_session()` sends
SSH_MSG_CHANNEL_OPEN with channel type "session", sender channel ID,
initial window size, and maximum packet size.

### 5.1-2 (Client processes CHANNEL_OPEN_CONFIRMATION)

**CONFORMS** — `deuce_ssh_conn_open_session()` receives and parses
CHANNEL_OPEN_CONFIRMATION, extracting remote_id, remote_window, and
remote_max_packet.

### 5.1-3 (CHANNEL_OPEN_FAILURE handling)

**CONFORMS** — The demux thread handles CHANNEL_OPEN_FAILURE by
setting `close_received` and waking the poll condition, causing
`open_session_channel()` to return an error.

### 5.1-4 (Server responds to CHANNEL_OPEN)

**CONFORMS** — `deuce_ssh_session_accept()` receives incoming
CHANNEL_OPEN (queued by the demux thread).
`deuce_ssh_channel_accept_raw()` sends CHANNEL_OPEN_CONFIRMATION.
`deuce_ssh_session_reject()` sends CHANNEL_OPEN_FAILURE.  The demux
thread auto-rejects forbidden channel types.

---

## Section 5.2: Data Transfer

### 5.2-1
> Implementations **MUST** correctly handle window sizes of up to
> 2^32 - 1 bytes.

**CONFORMS** — `local_window` and `remote_window` in
`struct deuce_ssh_channel_s` are `uint32_t`, supporting the full range.

### 5.2-2
> The window **MUST NOT** be increased above 2^32 - 1 bytes.

**CONFORMS** — All window arithmetic uses `window_add()`, a saturating
add that clamps at `UINT32_MAX`.  This applies to `local_window` (in
`send_window_adjust`), `remote_window` (in `conn_recv` and
`request_exec` when handling WINDOW_ADJUST), preventing overflow.

### 5.2-3
> The implementation **MUST NOT** advertise a maximum packet size that
> would result in transport packets larger than its transport layer is
> willing to receive.

**CONFORMS** — `deuce_ssh_conn_open_session()` advertises
`MAX_PACKET_SIZE` (0x8000 = 32768), which fits within the default
transport buffer (33280 bytes) with room for packet headers.

### 5.2-4
> [The implementation] **MUST NOT** generate data packets larger than
> its transport layer is willing to send, even if the remote end would
> be willing to accept very large packets.

**CONFORMS** — `deuce_ssh_conn_send_data()` checks
`len > ch->remote_max_packet` before sending.

### 5.2-5 (CHANNEL_DATA sending)

**CONFORMS** — `deuce_ssh_conn_send_data()` sends SSH_MSG_CHANNEL_DATA
with proper format (channel ID + data string).

### 5.2-6 (CHANNEL_DATA receiving)

**CONFORMS** — `deuce_ssh_conn_recv()` parses SSH_MSG_CHANNEL_DATA,
extracting the data payload and decrementing `local_window`.

### 5.2-7 (CHANNEL_EXTENDED_DATA receiving)

**CONFORMS** — `deuce_ssh_conn_recv()` parses
SSH_MSG_CHANNEL_EXTENDED_DATA, extracting the data_type_code and data
payload, decrementing `local_window`.

### 5.2-8 (CHANNEL_EXTENDED_DATA sending)

**CONFORMS** — `deuce_ssh_conn_send_extended_data()` sends
CHANNEL_EXTENDED_DATA with a specified data_type_code (typically 1
for stderr).  Same window constraints as send_data.

### 5.2-9 (WINDOW_ADJUST sending)

**CONFORMS** — `deuce_ssh_conn_send_window_adjust()` sends
SSH_MSG_CHANNEL_WINDOW_ADJUST and adds bytes to `local_window`.

### 5.2-10 (WINDOW_ADJUST receiving)

**CONFORMS** — `deuce_ssh_conn_recv()` handles
SSH_MSG_CHANNEL_WINDOW_ADJUST by adding bytes to `remote_window`.

---

## Section 5.3: Closing a Channel

### 5.3-1
> When a party will no longer send more data to a channel, it **SHOULD**
> send SSH_MSG_CHANNEL_EOF.

**CONFORMS** — `deuce_ssh_conn_send_eof()` sends CHANNEL_EOF.
Tracks `eof_sent` to prevent double-send.

### 5.3-2
> Upon receiving [SSH_MSG_CHANNEL_CLOSE], a party **MUST** send back an
> SSH_MSG_CHANNEL_CLOSE unless it has already sent this message for the
> channel.

**CONFORMS** — `deuce_ssh_conn_close()` tracks `close_sent` and is a
no-op if already sent (prevents double-close).  `deuce_ssh_conn_recv()`
sets `close_received` when the peer sends CLOSE, and returns the
message to the application.  The application cleans up its resources
and then calls `conn_close()` to send the reciprocal CLOSE (which
the library skips if it already sent one).  The channel struct tracks
both `close_sent` and `close_received`.

### 5.3-3
> A party **MAY** send SSH_MSG_CHANNEL_CLOSE without having sent or
> received SSH_MSG_CHANNEL_EOF.

**CONFORMS** — `deuce_ssh_conn_close()` can be called at any time.

### 5.3-4
> It is **RECOMMENDED** that all data sent before this message be
> delivered to the actual destination, if possible.

**APPLICATION** — The library transmits immediately; delivery to the
final destination is the application's responsibility.

---

## Section 5.4: Channel-Specific Requests

### 5.4-1 (Client sends CHANNEL_REQUEST)

**CONFORMS** — Library functions send CHANNEL_REQUEST with
`want_reply=TRUE` for: pty-req (`session_open_shell`), shell
(`session_open_shell`), exec (`session_open_exec`), subsystem
(`channel_open_subsystem`), env (declared in header).  Window-change
uses `want_reply=FALSE` per RFC.  All requests that use `want_reply=TRUE`
wait for SUCCESS/FAILURE via the `send_channel_request_wait()` mechanism.

### 5.4-2 (CHANNEL_SUCCESS/FAILURE handling)

**CONFORMS** — The demux thread routes SUCCESS/FAILURE to the
per-channel `request_pending`/`request_responded`/`request_success`
fields.  `send_channel_request_wait()` blocks on `poll_cnd` until
the response arrives.

### 5.4-3 (Server responds to CHANNEL_REQUEST)

**CONFORMS** — The demux thread handles incoming CHANNEL_REQUEST:
signals are queued with stream marks, exit-status is stored,
window-change is consumed.  Unrecognized requests with `want_reply`
get CHANNEL_FAILURE.  Low-level `conn_send_success()` and
`conn_send_failure()` are available for server-side use.

---

## Section 6.1: Opening a Session

### 6.1-1
> Client implementations **SHOULD** reject any session channel open
> requests to make it more difficult for a corrupt server to attack the
> client.

**CONFORMS** — The demux thread auto-rejects CHANNEL_OPEN "session"
when `sess->trans.client == true`, sending CHANNEL_OPEN_FAILURE with
reason SSH_OPEN_ADMINISTRATIVELY_PROHIBITED.

---

## Section 6.2: Requesting a Pseudo-Terminal

### 6.2-1
> Zero dimension parameters **MUST** be ignored.

**APPLICATION** — PTY handling is application-level.  No library
function for pty-req exists yet.

### 6.2-2
> The client **SHOULD** ignore pty requests.

**APPLICATION** — Same.

---

## Section 6.3: X11 Forwarding

### 6.3-1
> The 'x11 authentication cookie' **MUST** be hexadecimal encoded.

**N/A** — X11 forwarding is not implemented and not planned.

### 6.3-2
> Implementations **MUST** reject any X11 channel open requests if they
> have not requested X11 forwarding.

**CONFORMS** — The demux thread auto-rejects CHANNEL_OPEN "x11".

---

## Section 6.4: Environment Variable Passing

**CONFORMS** — Server-side env callback declared in
`deuce_ssh_server_session_cbs`.  Client-side `conn_request_env()`
sends "env" CHANNEL_REQUEST.

---

## Section 6.5: Starting a Shell or a Command

### 6.5-1
> Normal precautions **MUST** be taken to prevent the execution of
> unauthorized commands.

**APPLICATION** — Command authorization is the server application's
responsibility.

### 6.5-2
> The server **SHOULD NOT** halt the execution of the protocol stack
> when starting a shell or a program.

**APPLICATION** — The library does not start programs.

### 6.5-3
> It is **RECOMMENDED** that the reply to these messages be requested
> and checked.

**CONFORMS** — `deuce_ssh_conn_request_exec()` sets `want_reply=TRUE`
and checks the response.

### 6.5-4
> The client **SHOULD** ignore these messages [shell/exec/subsystem
> requests from the server].

**CONFORMS** — The demux thread sends CHANNEL_FAILURE for any
unrecognized CHANNEL_REQUEST with `want_reply`, which covers
server-sent shell/exec/subsystem requests on client-owned channels.

---

## Section 6.7: Window Dimension Change Message

### 6.7-1
> A response **SHOULD NOT** be sent to this message.

**N/A** — No window-change handling exists.  When implemented, this
is straightforward — `want_reply=FALSE` is specified in the message.

---

## Section 6.8: Local Flow Control

### 6.8-1
> The client **MAY** ignore this message.

**N/A** — Not implemented.

---

## Section 6.9: Signals

### 6.9-1
> Some systems lack signal implementation and **SHOULD** ignore this
> message.

**N/A** — Not implemented.

---

## Section 6.10: Returning Exit Status

### 6.10-1
> Returning the status is **RECOMMENDED**.

**CONFORMS** — `deuce_ssh_conn_send_exit_status()` sends exit-status.
`deuce_ssh_session_close(sess, ch, exit_code)` sends exit-status + EOF
+ CLOSE in the correct order.

---

## Section 7: TCP/IP Port Forwarding

### 7.1-1
> Client implementations **SHOULD** reject these messages [tcpip-forward
> global requests from the server]; they are normally only sent by the
> client.

**CONFORMS** — Global requests are handled in `recv_packet()`, which
auto-replies REQUEST_FAILURE for all unrecognized requests including
tcpip-forward.

### 7.2-1
> Implementations **MUST** reject [forwarded-tcpip channel opens] unless
> they have previously requested a remote TCP/IP port forwarding with
> the given port number.

**CONFORMS** — The demux thread auto-rejects CHANNEL_OPEN
"forwarded-tcpip" since port forwarding is never requested.

### 7.2-2
> Client implementations **SHOULD** reject direct TCP/IP open requests
> for security reasons.

**CONFORMS** — The demux thread auto-rejects CHANNEL_OPEN
"direct-tcpip".

---

## Section 8: Encoding of Terminal Modes

**N/A** — Terminal mode encoding/decoding is an application-level
concern.  The library could provide helpers but this is not a protocol
conformance issue.

---

## Section 11: Security Considerations

### 11-1
> It is **RECOMMENDED** that implementations disable all the
> potentially dangerous features (e.g., agent forwarding, X11
> forwarding, and TCP/IP forwarding) if the host key has changed without
> notice or explanation.

**APPLICATION** — Host key change detection and policy response are the
application's responsibility.

---

## Summary

### Conformance Status

| Status | Count |
|--------|-------|
| CONFORMS | 31 |
| PARTIAL | 0 |
| NOT IMPLEMENTED | 0 |
| N/A | 5 |
| APPLICATION | 5 |

### Resolved (since initial audit)

- **Global request handling** (4-1, 4-2) — handled transparently in
  `recv_packet()`, auto-replies REQUEST_FAILURE, optional callback.
- **CHANNEL_CLOSE reciprocal** (5.3-2) — `close_sent`/`close_received`
  tracking, idempotent `conn_close()`, demux auto-sends reciprocal.
- **Window overflow** (5.2-2) — saturating `window_add()`.
- **CHANNEL_EOF sending** (5.3-1) — `conn_send_eof()` with tracking.
- **Channel request types** (5.4-1) — pty-req, shell, exec, subsystem,
  window-change all implemented with proper wait-for-response mechanism.
- **SUCCESS/FAILURE handling** (5.4-2) — demux thread delivers responses
  via per-channel `request_pending`/`request_responded`/`request_success`.
- **Server CHANNEL_REQUEST dispatch** (5.4-3) — demux thread handles
  signals, exit-status, window-change; `channel_accept_raw()` for
  subsystem channels.
- **Client rejects session opens** (6.1-1) — demux auto-rejects
  "session" CHANNEL_OPEN when `sess.trans.client == true`.
- **X11 channel opens rejected** (6.3-2) — demux auto-rejects "x11".
- **Environment variables** (6.4) — server callback in session_cbs.
- **Exit status** (6.10-1) — `conn_send_exit_status()` and
  `session_close(exit_code)`.
- **Port forwarding rejected** (7.1-1, 7.2-1, 7.2-2) — global requests
  auto-fail; "forwarded-tcpip" and "direct-tcpip" auto-rejected.
- **Extended data sending** — `conn_send_extended_data()`.

### Remaining

No remaining issues.  All items resolved:

- **Server session_accept_channel** — fully implemented with
  callback-driven setup (pty-req, env, window-change callbacks;
  shell/exec/subsystem detection), setup mailbox for demux-to-setup
  communication, transition to buffered mode after setup.
- **Client rejects shell/exec/subsystem from server** — covered by
  the demux thread's generic FAILURE response for unrecognized
  CHANNEL_REQUESTs.
- **Window-change callback delivery** — stored on the channel struct
  after setup, fired by the demux thread during normal operation.
