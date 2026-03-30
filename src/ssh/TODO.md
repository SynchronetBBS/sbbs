# DeuceSSH — TODO / Known Issues

## Open

84. **DH-GEX group size vs cipher strength mismatch.**
    The client's GEX_REQUEST parameters (`GEX_MIN 2048`, `GEX_N 4096`,
    `GEX_MAX 8192`) are compile-time constants that don't account for the
    negotiated cipher.  A 2048-bit group provides ~112 bits of security,
    which is a poor match for AES-256 (256-bit).  OpenSSH has the same
    mismatch, but we should investigate whether to:
    - Bump `GEX_MIN` to 3072 (NIST 128-bit floor)
    - Tie `GEX_N` (preferred) to the cipher's key size
    - Consider that cipher negotiation happens in the same KEXINIT as KEX
      negotiation, so the client doesn't know the cipher result before
      sending GEX_REQUEST — the server could enforce a floor, but the
      client can only guess
    - Decide whether the built-in `default_select_group()` should enforce
      a minimum based on cipher strength, and if so, how it learns the
      cipher (it currently only sees min/preferred/max from the client)
    This needs interactive discussion — there are compatibility and
    protocol-ordering trade-offs that don't have obvious right answers.
    **Not suitable for automatic planning.**

### Thread safety audit (items 51-59)

### Design / liveness audit (items 62-79)

### API definition gaps (items 92-100)

### Malformed message reply audit (items 102, 102a-c)

### Selftest failures under parallel load (items 104, 106)

106. **`dssh_self_dhgex` intermittent failure under `-j16`.**
    Observed once during 10× `--repeat until-fail` with all 8 variants
    running simultaneously under `-j16`.  Did not reproduce in 5
    consecutive isolated reruns of `dssh_self_dhgex` alone, nor in 3
    subsequent full 10× `-j16` runs.  Only the `dhgex` (non-RSA)
    variant was affected.  No output captured — needs `--output-on-failure`
    to identify which test function and assertion.  May be a different
    manifestation of scheduling contention (DH-GEX is the slowest KEX
    due to prime generation), or an unrelated race.  Needs investigation.

## Closed

- Selftest echo tests failing under `-j16` (was item 104).
  `dssh_self_rsa` and `dssh_self_mlkem_rsa` variants failed ~40% of the
  time.  Root cause: both sides open channels with `initial_window=0`
  (correct for deferred channel type), then send WINDOW_ADJUST after
  setup.  The client's `dssh_chan_open` sends WINDOW_ADJUST and returns;
  the server's `dssh_chan_accept` independently sends its own.  Under
  `-j16` contention (especially RSA keygen), the server's WINDOW_ADJUST
  hadn't been processed by the client's demux thread before the test
  called `dssh_chan_write`, which returned 0 (non-blocking, by design —
  `remote_window` still 0).  `test_self_exec_echo` asserted `w > 0` on
  the return; `test_self_shell_echo` ignored it, so no data reached the
  server.  The server side was unaffected: it processes the client's
  WINDOW_ADJUST inline during the accept setup loop, so `remote_window`
  is already populated by the time `dssh_chan_accept` returns.  Fix:
  added `dssh_chan_poll(DSSH_POLL_WRITE)` before the first write in both
  tests (matching the pattern already used by `test_self_shell_large_data`).
  Made `server_echo_thread` (and the chan_accept variant) use a
  poll-then-retry write loop to avoid silently dropping data on partial
  writes.  Library unchanged — the non-blocking write API is correct;
  callers must poll for `DSSH_POLL_WRITE` before writing.

- `send_channel_request_wait` race: CHANNEL_CLOSE clobbers successful
  response (was item 105).  `test_self_exec_exit_code` failed under
  `-j16` because `dssh_chan_open()` returned NULL.  Root cause: in
  `send_channel_request_wait()` (ssh-conn.c), after the wait loop exits,
  the code checked `ch->close_received` unconditionally and returned
  `DSSH_ERROR_TERMINATED` even when `request_responded` was true.  When
  the server accepted a channel request (CHANNEL_SUCCESS) and then
  immediately closed the channel (CHANNEL_CLOSE), the client's demux
  thread could process both messages before the client thread woke up.
  The client saw `request_responded=true` (exited the loop correctly)
  but then the post-loop `close_received` check discarded the successful
  response.  Fix: capture `responded` under `buf_mtx`, check it first;
  only return `DSSH_ERROR_TERMINATED` when the loop exited without a
  response.  RSA variants were disproportionately affected because RSA
  keygen creates CPU contention that widens the scheduling window.

- Selftest race: cleanup while server echo thread still sending (was
  item 103).  Two bugs: (1) the server thread handle (`thrd_t st`) was
  a local variable lost when an ASSERT failed mid-test, so
  `dssh_test_after_each` cleanup could not join it; (2) `g_active_ctx`
  pointed to a stack-local `selftest_ctx` whose frame was popped on
  test return, so cleanup's deeper function calls (terminate, join,
  session_cleanup) grew the stack and overwrote the dangling ctx data.
  Fix: added `server_thread` and `server_thread_active` fields to
  `selftest_ctx`; added `selftest_start_thread()` helper; restructured
  `selftest_cleanup()` to snapshot all ctx fields into a local copy
  before any function calls, then terminate both sessions, join the
  server thread, and finally cleanup sessions.  All 27 test functions
  updated to use the helper and clear the active flag after joining.

- Malformed message parse failures now send required replies (was items
  102, 102a-c).  Audited all SSH message types that require a response:
  GLOBAL_REQUEST (want_reply), CHANNEL_REQUEST (want_reply), and
  CHANNEL_OPEN (always requires CONFIRMATION or FAILURE).  Four parse-
  failure paths silently dropped the required reply because `want_reply`
  was never extracted from the truncated payload.  Fix: each path now
  sends the appropriate failure reply (REQUEST_FAILURE, CHANNEL_FAILURE,
  or CHANNEL_OPEN_FAILURE) then disconnects with PROTOCOL_ERROR.  The
  disconnect is necessary because a speculative reply when `want_reply`
  was actually false would corrupt the reply ordering (RFC 4254 s4, s5.4
  match replies by order, not content).  CHANNEL_OPEN is different —
  OPEN_FAILURE carries the peer's channel ID, so it's matched by ID,
  but the session is still terminated since truncated messages indicate
  a broken peer.  For CHANNEL_OPEN truncations where the sender-channel
  cannot be extracted (too short), only disconnect is sent.  Updated
  audit-4254.md sections 4-1, 5.1-4, and 5.4-3.

- Callback setters after `dssh_session_start()` now rejected (was item 99).
  All 8 session-level setters return `int` and check `sess->demux_running`;
  return `DSSH_ERROR_TOOLATE` after start.  NULL cb intentionally allowed
  (means "no callback").

- Signal pointer lifetime now documented (was item 96).
  `dssh_session_read_signal()` no longer exists — the channel I/O
  redesign (item 101) replaced it with event-based signal delivery via
  `dssh_chan_read_event()`.  Pointer lifetime is documented in
  `deucessh-conn.h` (lines 127-129, 198-201), `README.md`
  (lines 1156-1158), and `design-channel-io-api.md` (lines 424-427):
  string pointers in events are valid until the next
  `dssh_chan_read_event()` or `dssh_chan_poll()` on the same channel.

- Channel I/O redesign fully implemented (was items 95, 101).  Complete
  replacement of the channel API per `design-channel-io-api.md`.
  `dssh_chan_*` prefix for all channel functions; stream API
  (open/read/write/poll) built on ZC internals; ZC API with zero-copy
  TX (getbuf/send) and RX (callback into rx_packet); events separated
  from data (signalfd model) with poll + read_event or event callback;
  `dssh_chan_accept()` for server with callback-driven setup; shared
  `dssh_chan_params` struct with builder functions; `dssh_chan_close()`
  with exit code and `dssh_chan_shutwr()` for half-close.  tx_mac_scratch
  eliminated via 4-byte seq prefix in tx_packet; send-path malloc
  eliminated; msgqueue eliminated (ZC: no buffer; stream: ring buffer);
  setup mailbox eliminated (accept runs on app's thread).  Old API
  (`dssh_session_read/write/poll`, `dssh_channel_read/write`,
  `dssh_session_accept_channel`) completely removed.  Subsumes items 67
  (setup mailbox blocking demux), 75 (msgqueue per-message malloc), and
  95 (channel I/O API unification).

- `dssh_transport_register_lang()` moved to public header (was item 93).
  Created `deucessh-lang.h` following the pattern of other module headers
  (`deucessh-comp.h`, etc.).  Moved `dssh_language_s` struct, typedef,
  `_Static_assert`, and `dssh_transport_register_lang()` declaration from
  `ssh-trans.h` into the new public header.  Languages are external-only
  (apps parse language tags from strings), so the function belongs in a
  consumer-facing header alongside the other `register_*` functions.

- `dssh_parse_uint32()` return value documented (was item 97).  Added
  doc comments to both `dssh_parse_uint32()` and `dssh_serialize_uint32()`
  declarations in `deucessh.h`.  `dssh_parse_uint32()` returns `int64_t`:
  4 (bytes consumed) on success, negative `DSSH_ERROR_*` on failure.
  `dssh_serialize_uint32()` returns `int`: 0 on success, negative on
  failure.  Updated `README.md` with the same information.

- Opaque-handle typedefs replaced with struct pointers in internal code
  (was item 100).  All internal library files (`ssh-internal.h`, `ssh.c`,
  `ssh-trans.c`, `ssh-auth.c`, `ssh-conn.c`) now use `struct dssh_session_s *`
  and `struct dssh_channel_s *` directly instead of the `dssh_session` and
  `dssh_channel` typedefs.  Public headers (`deucessh.h`, `deucessh-conn.h`)
  retain the typedefs for the external API.  Removed `#include "deucessh-conn.h"`
  from `ssh-trans.c`; added a forward declaration of `dssh_session_stop()`
  in `ssh-internal.h` so internal code doesn't need consumer headers.

- `dssh_channel_read()` peek mode fixed (was item 92).  Changed NULL
  check from `buf == NULL` to `buf == NULL && bufsz != 0`, allowing
  `dssh_channel_read(sess, ch, NULL, 0)` to reach `msgqueue_pop()` for
  peek (returns next message size without consuming).  Added `buf != NULL`
  guard on `maybe_replenish_window()` to avoid spurious window replenish
  on peek path.  3 new tests: peek empty queue, peek with data (twice
  to verify non-consuming), and NULL buf with non-zero bufsz rejection.

- Wrong channel type now detected at runtime (was item 94).  Added
  `chan_type` checks to all 10 channel I/O functions: `dssh_session_poll`,
  `dssh_session_read`, `dssh_session_read_ext`, `dssh_session_write`,
  `dssh_session_write_ext`, `dssh_session_read_signal`,
  `dssh_session_close` (require `DSSH_CHAN_SESSION`);
  `dssh_channel_poll`, `dssh_channel_read`, `dssh_channel_write`,
  `dssh_channel_close` (require `DSSH_CHAN_RAW`).  Returns
  `DSSH_ERROR_INVALID` on mismatch.  2 new tests:
  `type/session_funcs_reject_raw` and `type/channel_funcs_reject_session`.
  Updated 6 existing write-path tests to set `chan_type`.

- Redundant `dssh_*` typedefs removed (was item 91).  Deleted all 7
  unused type aliases from `deucessh.h`: `dssh_byte` (`uint8_t`),
  `dssh_boolean` (`bool`), `dssh_uint32_t` (`uint32_t`), `dssh_uint64_t`
  (`uint64_t`), `dssh_string` / `dssh_mpint` (pointer-to-struct), and
  `dssh_namelist` (pointer-to-struct).  The struct types (`dssh_string_s`,
  `dssh_namelist_s`) were also unused — removed entirely.  Updated
  `dssh_parse_uint32()` and `dssh_serialize_uint32()` signatures in
  `deucessh.h`, `ssh.c`, and `test/test_chan.c` to use `uint32_t` directly.

- Near-duplicate read/write pairs merged (was item 28).
  `session_stdout_readable()` and `session_stderr_readable()` merged into
  `session_readable(ch, ext)`.  `dssh_session_read()` and
  `dssh_session_read_ext()` bodies merged into `session_read_impl()`;
  public functions are now thin wrappers.  `dssh_session_poll()` updated
  to call `session_readable()`.  Write pair left as-is — `send_data()`
  and `send_extended_data()` build different wire messages
  (`SSH_MSG_CHANNEL_DATA` vs `SSH_MSG_CHANNEL_EXTENDED_DATA` with extra
  u32 field), so the duplication is structural, not accidental.

- Symbol prefix cleanup (was item 90).  Established consistent naming:
  `dssh_` prefix for `DSSH_PUBLIC` symbols only, `dssh_test_` prefix for
  symbols inside `#ifdef DSSH_TESTING` blocks, no prefix for internal
  symbols (`DSSH_PRIVATE`, `DSSH_TESTABLE`).  Renamed ~50 functions:
  10 `dssh_test_*` DSSH_TESTABLE functions in ssh-trans.c (e.g.
  `dssh_test_build_namelist` -> `build_namelist`), 14 `dssh_transport_*`
  DSSH_PRIVATE functions in ssh-trans.c/ssh-trans.h (e.g.
  `dssh_transport_send_packet` -> `send_packet`), 6 `dssh_conn_*` /
  `dssh_test_*` DSSH_TESTABLE functions in ssh-conn.c, 19 `dssh_*`
  DSSH_PRIVATE functions in ssh-chan.c/ssh-chan.h (e.g.
  `dssh_bytebuf_write` -> `bytebuf_write`), and `dssh_session_set_terminate`
  in ssh.c/ssh-internal.h.  Updated all call sites in library code
  (ssh-auth.c, ssh-conn.c, ssh.c) and test code.  Functions inside
  `#ifdef DSSH_TESTING` (`dssh_test_reset_global_config`,
  `dssh_test_set_sw_version`, `dssh_test_set_version_comment`) retained
  their `dssh_test_` prefix.

- Type-unsafe linked list traversal (was item 16).  Added
  `_Static_assert(!offsetof(..., next))` to all 6 algorithm struct
  headers (`dssh_kex_s`, `dssh_key_algo_s`, `dssh_enc_s`, `dssh_mac_s`,
  `dssh_comp_s`, `dssh_language_s`) and the test struct
  `test_algo_node`.  Added "must be first" comments on all `next`
  fields.  Updated `FREE_LIST()` macro to take a type parameter and
  use typed `->next` access instead of `memcpy` cast.

- Six near-identical `dssh_transport_register_*()` functions (was item 18).
  Replaced with `DEFINE_REGISTER(func_name, param_type, head, tail,
  entries)` macro.  ~140 lines of duplicated code reduced to ~25.
  Type-safe via typed `param_type` parameter accessing `item->name`
  and `item->next` directly.

- Demux head-of-line blocking under `tx_mtx` (was item 76).
  `send_packet()` held `tx_mtx` for its entire duration including the
  blocking `gconf.tx()` I/O callback.  On a congested link, the demux
  thread blocked on `tx_mtx` for fire-and-forget protocol responses
  (CHANNEL_FAILURE, OPEN_FAILURE, REQUEST_SUCCESS/FAILURE), stalling all
  incoming packet processing.  The original fix proposal (split
  send_packet into prepare + transmit phases) doesn't work cleanly
  because SSH MACs use implicit sequence numbers: packets must arrive on
  the wire in sequence-number order, so any split requires a second
  ordering mechanism that effectively re-serializes the I/O.  Fix:
  extracted `send_packet_inner()` (core build/MAC/encrypt/I/O logic),
  added `dssh_transport_send_or_queue()` for non-blocking demux sends.
  Uses `mtx_trylock` on `tx_mtx` -- fast path sends immediately,
  slow path enqueues the payload (linked list under `tx_queue_mtx`).
  `send_packet()` drains the queue under `tx_mtx` before each send,
  preserving sequence-number ordering.  3 demux call sites updated:
  `handle_channel_request()` CHANNEL_FAILURE, `demux_channel_open()`
  OPEN_FAILURE, and `recv_packet()` GLOBAL_REQUEST reply.

- `dssh_session_cleanup()` hang on `thrd_join` (was item 83).  Added
  `dssh_terminate_cb` callback typedef and `dssh_session_set_terminate_cb()`
  setter.  The callback fires exactly once when the session terminates
  (fatal error, peer disconnect, or explicit `dssh_session_terminate()`),
  before condvar broadcasts, so the application can close sockets or
  signal its event loop to unblock I/O callbacks.  Made
  `dssh_session_set_terminate()` single-fire via `atomic_exchange` —
  previously it unconditionally re-broadcast all condvars on every call.
  Updated I/O callback documentation to state that callbacks MUST return
  promptly when `dssh_session_is_terminated()` is true.

- `dssh_session_set_terminate()` called redundantly (was item 83,
  related).  The internal function ran all condvar broadcasts on every
  call, even when `terminate` was already true.  The `atomic_exchange`
  guard now returns early on subsequent calls.  All 15+ internal callers
  tolerate the no-op (they check `terminate` independently).

- `demux_dispatch()` and `dssh_session_accept_channel()` decomposed
  (was items 26, 30).  `demux_dispatch()` (~240 lines) split into 4
  helpers: `handle_channel_data()` (CHANNEL_DATA to stdout/raw queue),
  `handle_channel_extended_data()` (CHANNEL_EXTENDED_DATA to stderr),
  `handle_channel_request()` (signal/exit-status/window-change dispatch
  + FAILURE reply), and `dssh_test_parse_channel_request()` (DSSH_TESTABLE
  pure parser for the common CHANNEL_REQUEST wire format).  Switch body
  reduced from ~140 lines to ~15 lines.
  `dssh_session_accept_channel()` (~230 lines) split into
  `accept_channel_init()` (alloc, init sync, register, send confirmation)
  and `accept_setup_loop()` (for-loop processing setup requests via the
  shared parser).  Main function reduced to ~50 lines: null check, init,
  setup loop, transition, return.  Both sites now use the shared parser
  instead of duplicating the CHANNEL_REQUEST parse logic.
  `PTY_SER()` and `SIG_SER()` local macros replaced with direct
  `DSSH_PUT_U32()` calls (11 sites).  6 new unit tests for the parser
  (valid, empty type, no data, truncated type length, truncated type,
  truncated want_reply).

- Cascading allocation/cleanup duplication (was item 17).
  `dssh_transport_init()` used cascading free/destroy calls at each of 7
  allocation failure points (4 buffers + 3 sync primitives); replaced
  with a single `goto init_cleanup` label that frees all non-NULL buffers
  and destroys only initialized sync primitives (tracked by bool flags).
  `dssh_transport_newkeys()` had the same pattern for 6 key buffer mallocs
  plus a duplicate cleanse_free block after key derivation; replaced with
  NULL-initialized pointers and `goto keys_cleanup` (reusing the existing
  label).  Eliminates ~50 lines of duplicated cleanup code across 8 error
  paths.

- Self-initiated rekey data loss (was item 69).  The kexinit wait loop
  silently discarded non-KEXINIT messages received between sending our
  KEXINIT and receiving the peer's.  RFC 4253 s7.1 restricts the SENDER
  only; the peer may have valid in-flight connection-layer messages.
  Fix: added `dssh_rekey_msg` queue in `dssh_transport_state_s`.  The
  kexinit wait loop buffers non-KEXINIT messages; `recv_packet()` replays
  them after rekey completes (guarded by `!rekey_in_progress`).  Also
  fixed a latent bug where `recv_packet`'s default case set
  `rekey_pending` during an active rekey, which would have caused nested
  rekey attempts.  Cleanup in `dssh_transport_cleanup()` frees any
  residual queue entries.  Added regression test
  `test_self_rekey_inflight_data` (server burst-sends 500 bytes; client
  triggers auto-rekey on first packet; all 500 bytes arrive).

- Session-wide inactivity timeout (was items 65, 66).  Added
  `dssh_session_set_timeout()` and `DSSH_ERROR_TIMEOUT`.  Default
  75000ms (standard BSD TCP connect timeout).  Converted 4 unbounded
  `cnd_wait()` sites to `cnd_timedwait()`: `open_session_channel()`,
  `send_channel_request_wait()`, and `setup_recv()` in ssh-conn.c
  return `DSSH_ERROR_TIMEOUT`; `send_packet()` rekey wait in
  ssh-trans.c terminates the session on timeout (rekey failure is
  fatal).  Moved `deadline_from_ms()` to ssh-internal.h as shared
  `dssh_deadline_from_ms()` static inline.  4 new tests:
  `test_set_timeout`, `test_open_channel_timeout`,
  `test_setup_recv_timeout`, `test_rekey_send_timeout`.

- Poll/accept deadline reset on spurious wakeup (was item 64).
  `dssh_session_poll()`, `dssh_channel_poll()`, and `dssh_session_accept()`
  recomputed the absolute deadline from "now" on every loop iteration.
  With `cnd_broadcast` (closed item 73), each spurious wakeup reset the
  timeout, causing indefinite blocking.  Fix: compute deadline once before
  acquiring the mutex; reuse the pre-computed `struct timespec` for all
  `cnd_timedwait` calls in the loop.  Added regression test
  `test_poll_deadline_not_reset` that broadcasts `poll_cnd` every 50ms
  and verifies a 500ms poll returns within 1200ms.

- KBI client-side allocation cleanup (was item 12).  Replaced cascading
  5-calloc/free pattern with `goto kbi_cleanup`.  Eliminates ~45 lines
  of duplicated `free()` calls across 7 error paths.

- ssh-auth.c magic numbers replaced with named macros (was item 13).
  Added `AUTH_FAILURE_FIXED`, `PASSWD_CHANGEREQ_FIXED`, `AUTH_NONE_FIXED`,
  `AUTH_PASSWORD_FIXED`, `AUTH_KBI_FIXED`, `AUTH_PUBLICKEY_FIXED` — each
  is a computed expression showing field breakdown.  Added
  `service_connection` static global, replacing 5 per-function
  `static const char service[]` declarations.

- Timeout computation helper extracted (was item 27).  Added
  `deadline_from_ms()` in ssh-conn.c, replacing 3 identical
  timespec_get + ms-to-ns + normalize blocks in `dssh_session_accept()`,
  `dssh_session_poll()`, and `dssh_channel_poll()`.

- ssh-conn.c magic numbers replaced with computed expressions (was
  item 29).  6 sites: `send_data` (9), `send_extended_data` (13),
  `send_channel_request_wait` (10), PTY request (24), signal (20),
  window-change (16).  Each now uses a named local or inline expression
  showing the field breakdown.

- Symbol visibility audit complete (was item 88).  Audited all 17 library
  .c files.  Added `DSSH_PRIVATE` to 9 unannotated non-static functions:
  3 in `kex/mlkem768.c` (crypto_kem_mlkem768_{keypair,enc,dec}),
  3 in `kex/sntrup761.c` (crypto_kem_sntrup761_{keypair,enc,dec}),
  3 in `ssh-trans.c` (dssh_test_{reset_global_config,set_sw_version,
  set_version_comment}).  Updated corresponding headers (`mlkem768.h`,
  `sntrup761.h`) with `DSSH_PRIVATE` declarations and
  `#include "deucessh-portable.h"`.  Moved stale `extern` declarations
  from `test_transport.c` to `dssh_test_internal.h`.  All other symbols
  were already correctly annotated.  The shared library `.dynsym` was
  already clean due to `-fvisibility=hidden` in the CMake build.


- C11 threading return values now checked (was items 85, 86).
  Added `dssh_thrd_check()` inline wrapper in `ssh-internal.h` that
  calls the real C11 function, checks the result, and calls
  `set_terminate()` on failure (`thrd_error`/`thrd_nomem`).  All ~133
  `mtx_lock`/`mtx_unlock`/`cnd_wait`/`cnd_timedwait`/`cnd_broadcast`/
  `cnd_signal` calls across `ssh-conn.c` (113), `ssh-trans.c` (10),
  and `ssh.c` (13) wrapped.  Library code ignores the return value;
  the wrapper handles termination internally.  `set_terminate()` is
  the one exception: it checks returns to skip blocks whose lock was
  not acquired (best-effort wakeup).  No recursion risk because
  `terminate` is set before any lock operations.  Test injection uses
  a separate countdown (`dssh_test_thrd_fail_after`) from the existing
  OpenSSL countdown.  6 new tests in `test_thread_errors.c` (48 CTest
  entries across 8 algo variants).

- Shutdown path already tolerates prior lock/condvar failures (was
  item 87).  `dssh_thrd_check()` (items 85-86) sets `terminate = true`
  before any lock operations in `set_terminate()`, preventing recursion.
  Every lock/broadcast/unlock in `set_terminate()` checks its return
  value and skips on failure (best-effort wakeup).  `mtx_trylock` on
  `tx_mtx` handles the self-deadlock case.  No additional
  `internal_error` flag needed — the `terminate` atomic already serves
  as the recursion guard.

- Channel close use-after-free with demux thread (was items 62, 79).
  `dssh_session_close()` and `dssh_channel_close()` freed the channel
  while the demux thread held `buf_mtx`.  The window-change callback
  pattern (unlock for callback, relock after) created an even wider race
  window.  Fix: added `atomic_bool closing` to channel struct.  Close
  functions set `closing`, unregister the channel, then acquire/release
  `buf_mtx` to ensure the demux has exited its critical section before
  cleanup.  `demux_dispatch()` checks `closing` after each unlock-relock
  window (window-change callback, send CHANNEL_FAILURE) and bails out
  without touching the channel.  Added regression test
  `test_close_during_wc_cb`.

- Dead code removal (was item 89).  Removed 14 `DSSH_PRIVATE` functions
  with no library callers (test-only): `dssh_parse_byte`,
  `dssh_serialize_byte`, `dssh_parse_boolean`, `dssh_serialize_boolean`,
  `dssh_parse_uint64`, `dssh_serialize_uint64`, `dssh_parse_string`,
  `dssh_serialize_string`, `dssh_parse_mpint`, `dssh_serialize_mpint`,
  `dssh_parse_namelist`, `dssh_serialize_namelist` (all from ssh-arch.c),
  and `dssh_msgqueue_peek_size` (from ssh-chan.c).  Removed declarations
  from ssh-arch.h and ssh-chan.h, and corresponding test cases.

- Data race: rekey counters (was item 53).  Split `bytes_since_rekey`
  into `tx_bytes_since_rekey` and `rx_bytes_since_rekey`.  Made
  `tx_since_rekey` (`atomic_uint_fast32_t`) and `tx_bytes_since_rekey`
  (`atomic_uint_fast64_t`) atomic so `rekey_needed()` (called from recv
  thread) can read them lock-free without acquiring `tx_mtx`.  rx
  counters remain non-atomic (protected by `rx_mtx` held by caller).
  `newkeys()` uses `atomic_store` for tx counters.  Added test
  `rekey/needed_bytes_split_sum` verifying sum-of-halves threshold.

- Data race: algorithm query functions (was item 57).  Made all ten
  `*_selected` pointer fields in `dssh_transport_state_s` `_Atomic`.
  `dssh_transport_get_kex_name()` etc. now perform implicit atomic
  loads, eliminating UB during rekey.  Zero runtime cost on x86-64
  (compiler barrier only).

- Data race: `dssh_key_algo_set_ctx()` global registry (was item 60).
  Added `gconf.used` check — returns `DSSH_ERROR_TOOLATE` after the
  first `dssh_session_init()` call, same gate as algorithm registration.
  Host keys must be loaded before creating any session.  Added test
  `register/set_ctx_toolate`.

- Data race: `dssh_dh_gex_set_provider()` (was item 61).  Documented
  as must-call-before-handshake.  The `thrd_create` in
  `dssh_session_start()` provides the C11 happens-before guarantee.
  **Update:** `dssh_dh_gex_set_provider()` removed.  DH-GEX now uses a
  built-in RFC 3526 default provider; override via `dssh_kex_set_ctx()`
  (global, pre-init, same gate as `dssh_key_algo_set_ctx()`).

- Data race: callback setters (was item 32).  Documented as
  must-call-before-start.  The `thrd_create` in `dssh_session_start()`
  provides the C11 happens-before guarantee.  Calling after start is
  undefined behavior (function pointer + cbdata pair cannot be set
  atomically without a mutex, which is over-engineering for a
  set-once-before-start pattern).

- Data race: setup-to-normal transition in `dssh_session_accept_channel()`
  (was item 52).  Lines writing `chan_type`, buffer union, `window_max`,
  and `window_change_cb` now execute under `buf_mtx`.  `setup_mode` set
  to `false` last inside the critical section, so the demux thread either
  sees `setup_mode == true` (mailbox path) or a fully initialized channel.
  Fail path also protected.

- `dssh_auth_server()` username buffer size (was item 8).  Made
  `username_out_len` an in/out parameter: on input `*username_out_len` is
  the caller's buffer capacity; on output it is the number of bytes written.
  Usernames longer than the buffer are truncated.  Internal 255-byte cap
  retained.  All callers updated to initialize capacity before calling.
  Added truncation test (`auth/server/small_username_buffer`).

- Data race: unlocked flag pre-checks in write functions (was items 58,
  59).  Moved `!ch->open || ch->eof_sent || ch->close_received` checks
  into `dssh_conn_send_data()` and `dssh_conn_send_extended_data()`,
  which already hold `buf_mtx`.  Removed unlocked pre-checks from
  `dssh_session_write()`, `dssh_session_write_ext()`, and
  `dssh_channel_write()`.

- `cnd_signal` on `poll_cnd` changed to `cnd_broadcast` (was item 73).
  All 6 sites in `ssh-conn.c` and `ssh.c` now use `cnd_broadcast` so
  multiple threads polling the same channel are all woken.

- `dssh_session_start()` double-start guard fixed (was item 68).
  Guard changed from `demux_running` to `conn_initialized`.
  `dssh_session_stop()` now clears `conn_initialized` after cleanup.

- Server auth `DSSH_AUTH_DISCONNECT` callback return value (was item 70).
  Added `DSSH_AUTH_DISCONNECT = -2` constant.  Any server auth callback
  can return it to disconnect the client (sends SSH_MSG_DISCONNECT with
  reason `BY_APPLICATION`).  Application controls attempt-limit policy.
  Handled in all 6 callback dispatch sites in `auth_server_impl()`.

- NULL parameter validation for public API functions (was items 6, 45,
  46, 49, 50).  Added NULL checks returning `DSSH_ERROR_INVALID` (or
  `DSSH_ERROR_INIT` in auth) to: `dssh_parse_uint32()` and
  `dssh_serialize_uint32()` (`buf`/`val`/`pos`),
  `dssh_auth_get_methods()` (`methods`),
  `dssh_parse_env_data()` (`name_len`, `value_len`),
  `dssh_parse_exec_data()` (`command_len`),
  `dssh_parse_subsystem_data()` (`name_len`),
  `dssh_session_read/read_ext/write/write_ext()` (`buf`),
  `dssh_channel_read/write()` (`buf`),
  `dssh_session_read_signal()` (`signal_name`).
  Items 47 and 48 were already safe (`dssh_session_reject()` uses a
  ternary for NULL `description`; `dssh_session_accept_channel()`
  already checks `request_type`/`request_data` before dereferencing).

- Data race and stale-window bugs in window accounting (was items 51,
  63, 84).  `dssh_conn_send_window_adjust()` now updates `local_window`
  under `buf_mtx` and only after `send_packet()` succeeds.
  `dssh_conn_send_data()` and `dssh_conn_send_extended_data()` gained a
  `size_t *sentp` parameter for clamp-under-lock mode.
  `dssh_session_write()` / `write_ext()` pass `bufsz` directly, letting
  the inner function clamp atomically under `buf_mtx` (eliminates the
  double-lock snapshot gap).  `dssh_channel_write()` racy pre-check of
  `remote_window` / `remote_max_packet` removed; `dssh_conn_send_data()`
  performs the check under lock.

- Non-ASCII characters in comments replaced with ASCII equivalents
  (was item 43).  Em/en dashes, arrows, math operators, Greek letters,
  sub/superscripts, floor/ceil brackets, and accented letters across
  all 50 .c/.h files including vendored sntrup761 and libcrux headers.

- Error code accuracy audit (was items 9 + 44).  Added
  `DSSH_ERROR_AUTH_REJECTED` (-12) and `DSSH_ERROR_REJECTED` (-13).
  Changed ~50 sites: auth rejection -> AUTH_REJECTED (3), peer channel
  rejection -> REJECTED (2), unexpected message type -> PARSE (3),
  NULL-arg checks -> INVALID (~36), wrong-state writes -> TERMINATED (3),
  channel ID exhaustion -> TOOMANY (1), packet_length<2 -> PARSE (1),
  negotiation failure -> INVALID (1), empty registration name -> INVALID (6).
  Updated doxygen in deucessh-auth.h and all test assertions.

- Data race in `dssh_session_write()` / `write_ext()` (was item 23).
  Now reads `remote_window` and `remote_max_packet` under `buf_mtx`.

- `dssh_session_accept_channel()` fail path leaked `setup_payload`
  (was item 24).  Added `free(ch->setup_payload)` before `free(ch)`.

- `dssh_session_set_terminate()` and demux cleanup skipped channels
  with `chan_type == 0` (was item 33).  Removed the guard; `buf_mtx`
  and `poll_cnd` are initialized before registration, so signaling
  setup-mode channels is safe.

- `send_auth_failure()` stack buffer overflow (was item 7).  Switched
  from 256-byte stack buffer to dynamic allocation.

- `auth_server_impl()` SERVICE_ACCEPT stack buffer overflow (was item 8).
  Switched from 64-byte stack buffer to dynamic allocation.

- Peer KEXINIT parsing OOB read (was item 16).  Added minimum length
  check before setting `ppos`.

- Use-after-free race in channel close (was item 26).  `find_channel()`
  now acquires `buf_mtx` before releasing `channel_mtx` (hand-over-hand
  locking), so the demux thread never holds an unprotected channel
  pointer.

- `demux_dispatch()` CHANNEL_DATA/EXTENDED_DATA silent truncation
  (was item 29).  Malformed packets with declared length exceeding
  payload are now rejected instead of silently truncated.

- Serialize overflow checks can wrap `size_t` on 32-bit (was item 2).
  All `*pos + N > bufsz` checks in ssh-arch.c (6 functions),
  ssh-trans.c (`serialize_namelist_from_str`, `build_namelist`), and
  kex/dh-gex-sha256.c (`serialize_bn_mpint`) converted to subtraction
  form with `*pos > bufsz` guards.

- `flush_pending_banner()` truncates `strlen()` to `uint32_t` without
  validation (was item 7).  Now uses `size_t` with `#if SIZE_MAX > UINT32_MAX`
  guarded range check before narrowing; returns `DSSH_ERROR_TOOLONG`.

- `serialize_namelist_from_str()` overflow and silent truncation
  (was item 21).  Overflow check converted to subtraction form; silent
  truncation to `UINT32_MAX` replaced with `DSSH_ERROR_TOOLONG` error.

- Dead `dssh_bytearray` type removed (was item 1).  Typedef,
  parse/serialize/serialized_length functions, `_Generic` entries,
  and tests all removed.

- `dssh_parse_namelist_next` removed from public API (was item 3).
  Function, `dssh_namelist_s.next` field, and tests all removed.
  Library negotiation uses its own C-string iteration.

- Unused `dssh_transport_packet_s` struct removed (was item 38).

- Stale comment in `open_session_channel()` fixed (was item 25).
  Now accurately describes register-then-send order.

- `dssh_auth_server()` doc fixed (was item 40).  Comment incorrectly
  said username points into session buffer; it is copied to caller.

- Message type 60 aliasing comment added (was item 42).  Explains
  that PK_OK, PASSWD_CHANGEREQ, and INFO_REQUEST share value 60
  per RFC 4252 s7 / RFC 4256 s5.

- All ssh-arch.c functions already have correct visibility annotations
  (was item 4).  14 functions: 2 DSSH_PUBLIC (parse/serialize_uint32),
  12 DSSH_PRIVATE.  deucessh-arch.h declares only the 2 public ones.

- deucessh-arch.h does not include `<openssl/bn.h>` (was item 5).
  Item was inaccurate; the header was already clean.

- `deucessh-algorithms.h` no longer includes `<openssl/pem.h>`
  (was item 34).  Replaced `pem_password_cb` with library-owned
  `dssh_pem_password_cb` typedef (identical signature).  Key algorithm
  module headers (ssh-ed25519.h, rsa-sha2-256.h) updated to match.

- Duplicate transport function declarations removed from ssh-trans.h
  (was item 37).  deucessh.h is the authoritative public header;
  ssh-trans.h now has a comment referencing them.

- Unnecessary `<threads.h>` include removed from ssh-chan.h (was
  item 39).  No structures in that header use thread types.

- `extra_line_cb` in `dssh_transport_global_config` now uses the
  `dssh_transport_extra_line_cb` typedef (was item 41).

- `dssh_session_accept_channel()` and `dssh_channel_accept_raw()`
  leaked the `inc` parameter on early-return error paths (was item 71).
  Added `free(inc)` to all error returns after the NULL-arg check.

- `dssh_transport_init()` leaked `tx_mtx` when `rx_mtx` init failed
  (was item 72).  Split the combined `mtx_init` `||` into two
  separate checks with `mtx_destroy(&tx_mtx)` in the second.

- DH-GEX `dhgex_handler()` leaked BIGNUM `p` on malformed GEX_GROUP
  size-check failures (was item 77).  Added `BN_free(p)` before the
  two early returns after `parse_bn_mpint` succeeds.

- Missing `ka`/`ka->verify`/`ka->pubkey`/`ka->sign` NULL checks in
  sntrup761x25519-sha512 and mlkem768x25519-sha256 KEX handlers
  (was item 78).  Added the same guards that curve25519-sha256 and
  dh-gex-sha256 already had — both client (verify) and server
  (pubkey + sign) paths.

- Setup mailbox `malloc` failure in `demux_dispatch()` silently
  dropped the message, leaving `setup_recv()` blocked forever
  (was item 80).  Added `setup_error` flag to the channel struct;
  on malloc failure the demux sets the flag and signals.
  `setup_recv()` checks it and returns `DSSH_ERROR_ALLOC`.

- Auth banner handling only drained one `SSH_MSG_USERAUTH_BANNER`
  before the expected response (was item 82).  Changed `if` to
  `while` in `get_methods_impl()` and `auth_server_impl()` KBI path.
  `auth_publickey_impl()` already used a `continue` loop correctly.

- Magic numbers 80, 81, 82 in `dssh_transport_recv_packet()` replaced
  with `SSH_MSG_GLOBAL_REQUEST`, `SSH_MSG_REQUEST_SUCCESS`,
  `SSH_MSG_REQUEST_FAILURE` (was item 14).

- Duplicate `DSSH_CHAN_SESSION` / `DSSH_CHAN_RAW` definitions removed
  from `ssh-internal.h` (was item 35).  Canonical definitions remain
  in `ssh-chan.h`, which `ssh-internal.h` includes.

- Duplicate `SSH_OPEN_ADMINISTRATIVELY_PROHIBITED` definition removed
  from `ssh-internal.h` (was item 36).  Canonical definition (with all
  four reason codes) remains in `ssh-conn.c`, the only consumer.

- `SSH_MSG_UNIMPLEMENTED` callback already invoked (was item 44).
  `recv_packet` at ssh-trans.c dispatches to `unimplemented_cb` when
  set and payload is valid.  The TODO description was stale.

- Data race: `rekey_in_progress` changed from `bool` to `atomic_bool`
  (was item 54).  The demux thread reads it without `tx_mtx` to decide
  if a received KEXINIT is peer-initiated or self-initiated; the atomic
  eliminates the UB.

- Lost wakeup: `set_terminate()` now uses `mtx_trylock` on `tx_mtx`
  around `cnd_broadcast(&rekey_cnd)` (was item 55).  Prevents the
  lost-wakeup when no other thread holds `tx_mtx`, and avoids
  self-deadlock when called from `send_packet()` (which already holds
  it).  Residual: if a *different* sender holds `tx_mtx`, `trylock`
  fails and the broadcast fires without the lock — see item 66 for
  the full fix (switch to `cnd_timedwait`).

- Data race: `conn_initialized` changed from `bool` to `atomic_bool`
  (was item 56).  Read by `set_terminate()` from any thread, written by
  `session_start()` / `session_stop()`.

- `recv_packet_raw()` asymmetric termination on REKEY_NEEDED fixed
  (was item 15).  `send_packet()` already exempted REKEY_NEEDED from
  session termination; `recv_packet_raw()` now does the same.  The
  hard packet-count limit is recoverable — the session remains usable
  for the application to initiate a rekey.

- Bytebuf write truncation caused window accounting drift (was item 74).
  `demux_dispatch()` CHANNEL_DATA and CHANNEL_EXTENDED_DATA now use the
  return value of `dssh_bytebuf_write()` to deduct only the bytes
  actually written from `local_window`.  `maybe_replenish_window()` now
  caps the WINDOW_ADJUST amount by free buffer space (minimum of stdout
  and stderr free space for session channels), preventing window grants
  that exceed what the buffers can absorb.

- Accept queue size is not a library concern (was item 81).  The single
  demux thread serializes all packet processing.  While blocked on the
  setup mailbox (delivering CHANNEL_REQUESTs to the application's
  callback), it cannot process new CHANNEL_OPENs.  The accept queue can
  only grow while the demux thread is running freely, and the application
  controls drain rate via `dssh_session_accept()`.  Queue depth management
  is the application's responsibility.

- `auth_server_impl()` decomposed into per-method handlers (was items 10,
  11).  Extracted 4 static functions: `handle_auth_none()`,
  `handle_auth_password()`, `handle_auth_kbi()`,
  `handle_auth_publickey()`, plus a `password_dispatch()` helper for the
  shared password/change result handling.  `auth_server_impl()` reduced
  from ~575 lines to ~80 lines of dispatch using `AUTH_HANDLER_CONTINUE`
  / `AUTH_HANDLER_SUCCESS` return macros.  All 5 local `SER`/`SD_SER`/
  `MSG_SER`/`KBI_SER` macros replaced with direct `DSSH_PUT_U32()` calls.
  Client-side USERAUTH_REQUEST prefix extracted into
  `build_userauth_request()` helper, used in `get_methods_impl()`,
  `send_password_request()`, `auth_kbi_impl()`, and
  `auth_publickey_impl()`.  Eliminates ~60 lines of duplicated
  serialization code.

- `dssh_transport_kexinit()` and `dssh_transport_newkeys()` decomposed
  (was items 19, 20, 22).  `kexinit()` (~330 lines) split into 4
  static helpers: `build_kexinit_packet()` (packet construction),
  `receive_peer_kexinit()` (receive + rekey buffering),
  `dssh_test_parse_peer_kexinit()` (DSSH_TESTABLE pure parser for
  10 name-lists + validation), `negotiate_algorithms()` (8-way
  negotiation + first_kex_packet_follows discard).  Reduces `kexinit()`
  to ~30 lines of linear calls.  Eliminates `KEXINIT_SER_NL` macro
  and `if (0) { kexinit_fail: }` goto pattern.  `newkeys()` (~280
  lines) split into `dssh_test_encode_k_wire()` (DSSH_TESTABLE pure
  K wire encoder for mpint/string), `derive_and_apply_keys()` (derive
  6 keys, init cipher/MAC, alloc MAC bufs, reset counters).  Reduces
  `newkeys()` to ~40 lines.  `dssh_test_derive_key()` refactored:
  chained `||` OpenSSL calls replaced with sequential checks, 3
  duplicated cleanup blocks unified via `goto cleanup`.  11 new unit
  tests: 6 for `parse_peer_kexinit` (valid, control char, name too
  long, truncated, too short, first_kex_follows), 5 for
  `encode_k_wire` (mpint no pad, mpint sign pad, mpint empty, string,
  string empty).  Previously-SKIP `kexinit/peer_trunc_namelist` test
  now implemented via extracted parser.

- ssh-chan.c/h folded into ssh-conn.c (was item 31).  All 19
  `DSSH_PRIVATE` buffer primitives (bytebuf, msgqueue, sigqueue,
  acceptqueue) moved into ssh-conn.c as `DSSH_TESTABLE` functions.
  Struct definitions moved into ssh-internal.h.  ssh-chan.c and
  ssh-chan.h deleted.  test/test_chan.c updated to include
  dssh_test_internal.h; declarations added there.

- ssh-arch.c/h and deucessh-arch.h eliminated.  `dssh_parse_uint32()`
  and `dssh_serialize_uint32()` moved into ssh.c.  deucessh-arch.h
  content (type definitions and function declarations) inlined into
  deucessh.h.  All three files deleted.  test/test_arch.c tests
  (16 tests: uint32 parse/serialize, cleanse, NULL checks) absorbed
  into test/test_chan.c.  dssh_test_arch test executable removed.
