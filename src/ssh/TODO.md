# DeuceSSH — TODO / Known Issues

## Open

6. ssh-arch.c public functions have no parameter validation.  NULL `buf`,
   `val`, or `pos` pointers crash immediately.  Functions confirmed as
   `DSSH_PUBLIC` need NULL checks and appropriate error returns.

8. `dssh_auth_server()` username output has no buffer size parameter.
   Copies up to 255 bytes into `username_out` (capped by internal
   `saved_user[256]`), but caller cannot specify output buffer size.

9. Client auth functions return `DSSH_ERROR_INIT` for auth rejection
   (USERAUTH_FAILURE).  Confusing — reads as initialization error, not
   auth failure.  Should be a distinct error code.

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

14. Magic numbers 80, 81, 82 in `dssh_transport_recv_packet()` for
    SSH_MSG_GLOBAL_REQUEST / REQUEST_SUCCESS / REQUEST_FAILURE.
    Should use named constants.

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

32. Callback setters in ssh.c (`dssh_session_set_debug_cb`,
    `set_banner_cb`, `set_unimplemented_cb`, `set_global_request_cb`)
    store a function pointer and cbdata in two separate non-atomic
    stores with no synchronization.  The demux thread reads these
    concurrently.  Either document as set-once-before-start or protect
    with the session mutex.

35. `DSSH_CHAN_SESSION` / `DSSH_CHAN_RAW` defined in both `ssh-chan.h`
    (lines 140–141) and `ssh-internal.h` (lines 96–97).  Same values
    but changing one without the other is a silent mismatch.

36. `SSH_OPEN_ADMINISTRATIVELY_PROHIBITED` defined in both `ssh-conn.c`
    (line 166) and `ssh-internal.h` (line 115).  Same duplication hazard.

43. Source files contain non-ASCII characters in comments (em dashes,
    arrows, etc.).  Replace with ASCII equivalents for strict C17
    source character set conformance.

## Closed

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
