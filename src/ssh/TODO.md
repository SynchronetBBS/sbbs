# DeuceSSH — TODO / Known Issues

## Open

10. `auth_server_impl()` is too large — should be split into per-method
    handler functions (none, password, publickey, keyboard-interactive).
    Similarly `get_methods_impl()` and `auth_kbi_impl()` would benefit
    from extraction of message-building helpers.

11. Local `#define SER()` / `SD_SER()` / `MSG_SER()` / `KBI_SER()`
    macros are hard to read and have non-obvious names.  Splitting
    message building into helper functions (item 10) would eliminate
    the need for these.

12. KBI client-side allocation/cleanup duplication (lines 1469–1532).
    Five arrays allocated individually with cascading cleanup on each
    failure.  A single `goto cleanup` pattern would reduce ~60 lines
    to ~15.

13. ssh-auth.c is full of magic numbers for message fixed-overhead sizes:
    5 (msg_type + uint32), 9, 31, 40, 49, 55.  Each is a manually
    pre-computed sum of fixed-size fields — opaque without re-deriving
    the arithmetic from the wire format.  Replace with computed
    expressions or named constants.

15. `recv_packet_raw()` terminates the session on DSSH_ERROR_REKEY_NEEDED
    (falls through to `dssh_session_set_terminate()`).  `send_packet()`
    explicitly exempts REKEY_NEEDED from termination.  Asymmetric — recv
    hard limit kills the session rather than triggering a rekey.

16. Type-unsafe linked list traversal via
    `memcpy(&node, node, sizeof(void *))` in `dssh_test_build_namelist()`
    and `dssh_test_negotiate_algo()`.  Relies on `next` being the first
    field of every algorithm struct.  Silently breaks if any struct
    reorders its fields.

17. Cascading allocation/cleanup duplication in `dssh_transport_init()`
    (4 buffers + 3 sync primitives) and `dssh_transport_newkeys()`
    (6 key buffers).  Each new allocation requires cleanup of all prior.
    A `goto cleanup` pattern would simplify both.

18. Six near-identical `dssh_transport_register_*()` functions (kex,
    key_algo, enc, mac, comp, lang).  Same structure, different linked
    list and type.  Could be a single helper with type-specific wrappers.

19. `dssh_transport_kexinit()` is ~330 lines covering KEXINIT send,
    receive, peer name-list parsing, algorithm negotiation, and
    first_kex_packet_follows handling.  Should be decomposed into
    smaller functions.  Similarly `dssh_transport_newkeys()` (~340 lines)
    combines NEWKEYS exchange, K encoding, key derivation (6 calls),
    old context cleanup, new context init, and MAC buffer allocation.

20. Local `#define KEXINIT_SER_NL` and `FREE_LIST` macros, plus
    `if (0) { kexinit_fail: }` goto target pattern.  Same class of
    issue as item 11 — decomposition would eliminate these.

22. `dssh_test_derive_key()` chains six OpenSSL calls
    (DigestInit/Update×4/Final) into a single `if (... != 1 || ...)`
    condition.  Non-obvious control flow, impossible to distinguish which
    call failed, and the identical cleanup block (cleanse + ctx free +
    md free) is duplicated for the initial hash and the extension loop.
    Should be sequential checks with a single `goto cleanup`, or a
    helper that wraps the digest sequence.


26. `demux_dispatch()` (~240 lines) and `dssh_session_accept_channel()`
    (~230 lines) are too large.  demux_dispatch handles 8 message types
    with nested request sub-dispatch.  accept_channel mixes allocation,
    registration, mailbox loop, parsing, callback, and buffer init.

27. Timeout computation (timespec_get + ms addition + ns normalization)
    duplicated identically in `dssh_session_accept()`,
    `dssh_session_poll()`, and `dssh_channel_poll()`.  Extract a helper.

28. Near-duplicate read/write pairs: `dssh_session_read()` /
    `dssh_session_read_ext()` and `dssh_session_write()` /
    `dssh_session_write_ext()` differ only in which buffer or send
    function they use.

29. ssh-conn.c magic numbers: 9, 13, 10, 16, 20, 24 for message
    fixed-overhead sizes.  Same class of issue as item 13.

30. Local `#define PTY_SER()` and `#define SIG_SER()` macros in
    ssh-conn.c.  Same class of issue as item 11.

31. ssh-chan.c has no public API — every function is `DSSH_PRIVATE`.
    Only consumer is ssh-conn.c (and tests).  Consider inlining as
    static functions in ssh-conn.c, or at minimum question whether a
    separate translation unit and header (`ssh-chan.h`) is warranted
    for purely internal buffer primitives.

### Thread safety audit (items 51-59)

### Design / liveness audit (items 62-79)


64. **Poll/accept functions hold their mutex across the entire blocking
    wait, serializing concurrent callers and stacking timeouts.**
    `dssh_session_poll()` (line 1981) and `dssh_channel_poll()` (line
    2189) hold `buf_mtx` across `cnd_timedwait`.  A second thread
    polling the same channel blocks on `mtx_lock` until the first
    thread's wait completes, extending the effective timeout.  Same
    pattern in `dssh_session_accept()` (line 1097) with `accept_mtx`.
    Fix: compute the absolute deadline before acquiring the mutex;
    inside the loop, use the pre-computed deadline to cap the remaining
    wait.  Also use `cnd_broadcast` (see item 73) so all waiters
    re-evaluate.

65. **Unbounded waits with no timeout in channel open / request /
    setup paths.**  `open_session_channel()` (line 1262) uses
    `cnd_wait()` with no timeout waiting for CHANNEL_OPEN_CONFIRMATION.
    `send_channel_request_wait()` (line 1337) similarly waits for
    `request_responded`.  `setup_recv()` (line 1647) waits for setup
    messages in the server accept path.  If the peer never responds,
    the caller hangs indefinitely.  The only escape is
    `set_terminate()`, but that requires another thread.  Fix: add
    timeout parameters or use `cnd_timedwait()` with a configurable
    or session-level default.

66. **Unbounded blocking in `send_packet()` during rekey.**  Line
    488-489 blocks on `cnd_wait(&rekey_cnd, &tx_mtx)` while
    `rekey_in_progress`.  Duration depends entirely on the remote
    peer's cooperation during KEX (multiple network round-trips).
    No timeout, no escape except `set_terminate()`.  A slow or
    malicious peer can stall all application senders for the
    duration of the rekey.  Fix: use `cnd_timedwait` with a
    configurable upper bound.  Note: `set_terminate()` uses
    `mtx_trylock` on `tx_mtx` (to avoid self-deadlock when called
    from `send_packet` itself), so if a *different* sender holds
    `tx_mtx`, the `cnd_broadcast` fires without the lock and the
    original lost-wakeup window (item 55) still exists for that
    case.  Switching to `cnd_timedwait` here would eliminate the
    concern — the sender wakes on timeout regardless.

67. **Setup mode single-slot mailbox blocks the demux thread.**
    In `demux_dispatch()` (line 616-631), when a channel is in
    `setup_mode`, the demux thread delivers via a single-slot mailbox
    (`setup_payload`/`setup_ready`).  If the previous message has
    not been consumed, the demux thread blocks on `cnd_wait` until
    the application's `request_cb` callback returns.  During this
    time, ALL other channels on the same session are stalled: no
    data delivery, no window adjusts, no new channel opens.  Fix:
    use a queue instead of a single slot, or document that
    `request_cb` must return promptly.

69. **Self-initiated rekey silently drops application data.**
    In `dssh_transport_kexinit()` (lines 1183-1195), after sending
    our KEXINIT, the code loops calling `recv_packet` waiting for
    the peer's KEXINIT.  Non-KEXINIT messages received during this
    loop are silently discarded.  The peer may have sent CHANNEL_DATA
    that was in-flight before seeing our KEXINIT.  That data is
    permanently lost, and the peer's window accounting becomes wrong.
    Fix: route non-KEXINIT messages through the normal demux dispatch,
    or buffer them for replay after rekey completes.

74. **Bytebuf `dssh_bytebuf_write()` silently truncates when full.**
    `ssh-chan.c:48-49` returns a short count when the buffer is full.
    The caller in `demux_dispatch()` (line 657) ignores the return
    value and decrements `local_window` by the full `dlen`.  Data
    is lost from the stream.  This should not happen normally because
    the window and buffer are the same size, but if the window-add
    race (item 51) causes window accounting drift, or if
    `maybe_replenish_window` grants more window than free buffer
    space, truncation occurs silently.  Fix: compute replenishment
    based on free buffer space, not just `window_max - local_window`,
    and check/assert the return value of `bytebuf_write`.

75. **Msgqueue unbounded per-message overhead for raw channels.**
    `dssh_msgqueue_push()` does a separate `malloc` per message with
    `sizeof(dssh_msgqueue_entry)` overhead (~24 bytes).  The SSH
    window limits total bytes, but a pathological peer sending many
    1-byte messages creates ~2M linked-list nodes consuming ~48MB
    instead of the 2MB window.  Fix: consider coalescing small
    messages, or decrementing `local_window` by message overhead
    in addition to data bytes.

76. **I/O callbacks called under `tx_mtx` create head-of-line
    blocking.**  `send_packet()` holds `tx_mtx` for its entire
    duration, including the `gconf.tx()` I/O callback (line 597).
    On a congested link, a slow send holds `tx_mtx` for the full
    I/O timeout duration, blocking all other senders including the
    demux thread's protocol responses.  Fix: split `send_packet`
    into a prepare phase (under `tx_mtx`, builds encrypted packet)
    and a transmit phase (outside `tx_mtx`, calls I/O callback).
    Alternatively, document that I/O callbacks should have short
    timeouts.


83. **`dssh_session_cleanup()` can hang indefinitely on `thrd_join`.**
    `dssh_session_cleanup()` (ssh.c line 98) calls
    `dssh_session_terminate()` then `dssh_session_stop()`, which calls
    `thrd_join()` (ssh-conn.c line 1060).  The demux thread exits when
    `recv_packet` returns an error, but `recv_packet` blocks on the
    I/O callback.  If the callback doesn't respect the `terminate` flag
    promptly (e.g., blocks on `read()` with no timeout on an open
    socket), `cleanup` hangs forever.  Fix: document that the
    application MUST close the underlying socket (or set a short
    timeout) before calling `cleanup`, or provide a non-blocking
    `session_stop` variant.

## Closed

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
