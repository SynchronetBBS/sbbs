# DeuceSSH — TODO / Known Issues

## Open

1. Remove dead `dssh_bytearray` type.  Not an RFC 4251 data type, unused
   anywhere in the library or modules.  Remove the typedef, parse/serialize/
   serialized_length functions, `_Generic` entries, and tests.

2. Serialize overflow checks in ssh-arch.c can wrap `size_t` on 32-bit
   platforms.  `*pos + val->length` (serialize_bytearray:56) and
   `*pos + 4 + val->length` (serialize_string:193) should use the safe
   form `bufsz - *pos < needed` (after verifying `*pos <= bufsz`).

3. Remove `dssh_parse_namelist_next` from the public API.  Unused by the
   library (negotiation uses its own C-string iteration).  Only exercised
   by tests.  Return value `val->length + 4` mimics wire-format convention
   but doesn't reflect actual bytes consumed.

4. Every function needs `DSSH_PUBLIC` or `DSSH_PRIVATE` annotation.
   ssh-arch.c has ~30 functions in a public header with no visibility
   markup at all.  Clarify which are part of the consumer API (for
   packet-level parse/serialize) and which are internal helpers.

5. deucessh-arch.h includes `<openssl/bn.h>` — likely for mpint — but
   public headers should not expose OpenSSL.  If mpint needs BIGNUM for
   consumers, document why; otherwise remove the include and keep the
   OpenSSL dependency internal.

6. ssh-arch.c public functions have no parameter validation.  NULL `buf`,
   `val`, or `pos` pointers crash immediately.  Functions confirmed as
   `DSSH_PUBLIC` need NULL checks and appropriate error returns.

7. `send_auth_failure()` stack buffer overflow.  256-byte stack buffer
   `msg[256]`, but `methods` comes from the app's `cbs->methods_str`
   with no length cap.  Methods string > 250 bytes overflows via memcpy.

8. `auth_server_impl()` SERVICE_ACCEPT stack buffer overflow.  64-byte
   stack buffer `accept[64]`.  Service name `slen` is attacker-controlled
   (from client packet), checked against `payload_len` but not against
   `sizeof(accept)`.  Malicious client with service name > 59 bytes
   overflows the stack.

9. `flush_pending_banner()` truncates `strlen()` to `uint32_t` without
   validation (lines 319, 322).  Inconsistent with the overflow-checking
   pattern used elsewhere.

10. `dssh_auth_server()` username output has no buffer size parameter.
    Copies up to 255 bytes into `username_out` (capped by internal
    `saved_user[256]`), but caller cannot specify output buffer size.

11. Client auth functions return `DSSH_ERROR_INIT` for auth rejection
    (USERAUTH_FAILURE).  Confusing — reads as initialization error, not
    auth failure.  Should be a distinct error code.

12. `auth_server_impl()` is too large — should be split into per-method
    handler functions (none, password, publickey, keyboard-interactive).
    Similarly `get_methods_impl()` and `auth_kbi_impl()` would benefit
    from extraction of message-building helpers.

13. Local `#define SER()` / `SD_SER()` / `MSG_SER()` / `KBI_SER()`
    macros are hard to read and have non-obvious names.  Splitting
    message building into helper functions (item 12) would eliminate
    the need for these.

14. KBI client-side allocation/cleanup duplication (lines 1469–1532).
    Five arrays allocated individually with cascading cleanup on each
    failure.  A single `goto cleanup` pattern would reduce ~60 lines
    to ~15.

15. ssh-auth.c is full of magic numbers for message fixed-overhead sizes:
    5 (msg_type + uint32), 9, 31, 40, 49, 55.  Each is a manually
    pre-computed sum of fixed-size fields — opaque without re-deriving
    the arithmetic from the wire format.  Replace with computed
    expressions or named constants.

16. Peer KEXINIT parsing OOB read.  `ppos` is set to
    `1 + DSSH_KEXINIT_COOKIE_SIZE` (17) before verifying `pk_len` is
    at least that large.  A short malformed KEXINIT causes unsigned
    wraparound in `pk_len - ppos`, leading to out-of-bounds reads.

17. Magic numbers 80, 81, 82 in `dssh_transport_recv_packet()` for
    SSH_MSG_GLOBAL_REQUEST / REQUEST_SUCCESS / REQUEST_FAILURE.
    Should use named constants.

18. `recv_packet_raw()` terminates the session on DSSH_ERROR_REKEY_NEEDED
    (falls through to `dssh_session_set_terminate()`).  `send_packet()`
    explicitly exempts REKEY_NEEDED from termination.  Asymmetric — recv
    hard limit kills the session rather than triggering a rekey.

19. Type-unsafe linked list traversal via
    `memcpy(&node, node, sizeof(void *))` in `dssh_test_build_namelist()`
    and `dssh_test_negotiate_algo()`.  Relies on `next` being the first
    field of every algorithm struct.  Silently breaks if any struct
    reorders its fields.

20. Cascading allocation/cleanup duplication in `dssh_transport_init()`
    (4 buffers + 3 sync primitives) and `dssh_transport_newkeys()`
    (6 key buffers).  Each new allocation requires cleanup of all prior.
    A `goto cleanup` pattern would simplify both.

21. Six near-identical `dssh_transport_register_*()` functions (kex,
    key_algo, enc, mac, comp, lang).  Same structure, different linked
    list and type.  Could be a single helper with type-specific wrappers.

22. `dssh_transport_kexinit()` is ~330 lines covering KEXINIT send,
    receive, peer name-list parsing, algorithm negotiation, and
    first_kex_packet_follows handling.  Should be decomposed into
    smaller functions.  Similarly `dssh_transport_newkeys()` (~340 lines)
    combines NEWKEYS exchange, K encoding, key derivation (6 calls),
    old context cleanup, new context init, and MAC buffer allocation.

23. Local `#define KEXINIT_SER_NL` and `FREE_LIST` macros, plus
    `if (0) { kexinit_fail: }` goto target pattern.  Same class of
    issue as item 13 — decomposition would eliminate these.

24. `serialize_namelist_from_str()` has the same `*pos + len > bufsz`
    32-bit overflow pattern as item 2.

25. `dssh_test_derive_key()` chains six OpenSSL calls
    (DigestInit/Update×4/Final) into a single `if (... != 1 || ...)`
    condition.  Non-obvious control flow, impossible to distinguish which
    call failed, and the identical cleanup block (cleanse + ctx free +
    md free) is duplicated for the initial hash and the extension loop.
    Should be sequential checks with a single `goto cleanup`, or a
    helper that wraps the digest sequence.

26. Use-after-free race in `dssh_session_close()` /
    `dssh_channel_close()`.  Both call `unregister_channel()` →
    `cleanup_channel_buffers()` → `free(ch)`, but the demux thread
    may hold a `ch` pointer from `find_channel()` between releasing
    `channel_mtx` and acquiring `ch->buf_mtx`.

27. Data race in `dssh_session_write()`: reads `ch->remote_window`
    without holding `buf_mtx`.  Demux thread updates it concurrently
    via WINDOW_ADJUST.  Not dangerous (send_data re-checks under the
    lock), but technically undefined behavior.

28. `dssh_session_accept_channel()` fail path (lines 1928–1934) does
    not free `ch->setup_payload`.  If the demux thread delivered a
    message that hasn't been consumed by `setup_recv`, it leaks.

29. `demux_dispatch()` CHANNEL_DATA handler silently truncates data
    when declared `dlen` exceeds `payload_len` (line 647–648).
    Delivers partial data to the application without indication.
    Should be a parse error or at minimum flag the condition.

30. Stale comment in `open_session_channel()` (lines 1227–1244) says
    "send the CHANNEL_OPEN, register the channel" but code registers
    first, then sends.

31. `demux_dispatch()` (~240 lines) and `dssh_session_accept_channel()`
    (~230 lines) are too large.  demux_dispatch handles 8 message types
    with nested request sub-dispatch.  accept_channel mixes allocation,
    registration, mailbox loop, parsing, callback, and buffer init.

32. Timeout computation (timespec_get + ms addition + ns normalization)
    duplicated identically in `dssh_session_accept()`,
    `dssh_session_poll()`, and `dssh_channel_poll()`.  Extract a helper.

33. Near-duplicate read/write pairs: `dssh_session_read()` /
    `dssh_session_read_ext()` and `dssh_session_write()` /
    `dssh_session_write_ext()` differ only in which buffer or send
    function they use.

34. ssh-conn.c magic numbers: 9, 13, 10, 16, 20, 24 for message
    fixed-overhead sizes.  Same class of issue as item 15.

35. Local `#define PTY_SER()` and `#define SIG_SER()` macros in
    ssh-conn.c.  Same class of issue as item 13.

36. ssh-chan.c has no public API — every function is `DSSH_PRIVATE`.
    Only consumer is ssh-conn.c (and tests).  Consider inlining as
    static functions in ssh-conn.c, or at minimum question whether a
    separate translation unit and header (`ssh-chan.h`) is warranted
    for purely internal buffer primitives.

37. Callback setters in ssh.c (`dssh_session_set_debug_cb`,
    `set_banner_cb`, `set_unimplemented_cb`, `set_global_request_cb`)
    store a function pointer and cbdata in two separate non-atomic
    stores with no synchronization.  The demux thread reads these
    concurrently.  Either document as set-once-before-start or protect
    with the session mutex.

38. `dssh_session_set_terminate()` channel wakeup loop skips channels
    with `chan_type == 0` (line 32).  During `dssh_session_accept_channel`,
    a channel is registered with `chan_type == 0` in setup mode.  The
    guard skips signaling it, potentially delaying `setup_recv`'s
    detection of termination.

39. `deucessh-algorithms.h` includes `<openssl/pem.h>` — exposes
    `pem_password_cb` to consumers.  Same class as item 5: public
    headers should not require OpenSSL on the consumer's include path.

40. `DSSH_CHAN_SESSION` / `DSSH_CHAN_RAW` defined in both `ssh-chan.h`
    (lines 140–141) and `ssh-internal.h` (lines 96–97).  Same values
    but changing one without the other is a silent mismatch.

41. `SSH_OPEN_ADMINISTRATIVELY_PROHIBITED` defined in both `ssh-conn.c`
    (line 166) and `ssh-internal.h` (line 115).  Same duplication hazard.

42. `dssh_transport_handshake`, `dssh_transport_disconnect`, and the
    `get_*_name` query functions are declared in both `deucessh.h`
    and `ssh-trans.h`.  Harmless but unclear which is authoritative.

43. `dssh_transport_packet_s` struct in `ssh-trans.h` (lines 64–70)
    appears unused anywhere in the implementation.  Potentially dead.

44. `ssh-chan.h` includes `<threads.h>` but none of its structures use
    thread types — those are in `dssh_channel_s` in `ssh-internal.h`.

45. `deucessh-auth.h` comment for `dssh_auth_server()` (line 232) says
    username output "points into session buffer — valid until next
    recv_packet."  Implementation copies into `saved_user[256]` then
    into `username_out` — data IS copied, doc is wrong.

46. `extra_line_cb` in `dssh_transport_global_config` (ssh-trans.h:216)
    uses a raw function pointer instead of the existing
    `dssh_transport_extra_line_cb` typedef.  Inconsistent.

47. `SSH_MSG_USERAUTH_PK_OK`, `PASSWD_CHANGEREQ`, and `INFO_REQUEST`
    all defined to value 60 in `deucessh-auth.h` (lines 17–19).
    Correct per RFCs (context-dependent aliases) but confusing for
    consumers.  Add a comment explaining the aliasing.
