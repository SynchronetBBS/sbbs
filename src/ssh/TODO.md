# DeuceSSH — TODO / Known Issues

## Open

12. `dssh_bytebuf_write` crashed with SIGFPE (`% 0`) in the demux
    thread — a channel with `chan_type == 0` (type not yet determined)
    received CHANNEL_DATA.  The `chan_type == 0` early return in
    `demux_dispatch` drops the data but doesn't replenish the window,
    so the peer stalls.  The bytebuf capacity-0 guard prevents the
    crash but doesn't fix the stall.  Investigate why the demux thread
    can deliver data before the channel type is set.

11. `alloc_channel_id()` (ssh-conn.c) is a bare `next_channel_id++`
    with no collision detection.  If a session opens and closes 2^32
    channels, the counter wraps and the next ID could collide with a
    still-open channel.  Fix: scan the active channel table and skip
    IDs already in use, or return an error if all 2^32 IDs are
    exhausted.

## Fixed

7. Removed all 31 `#ifndef DSSH_TESTING` dead-code guards.
   Only one legitimate contract-violation guard remains in
   `dssh_parse_string()` (ssh-arch.c:157).

1. Handshake/rekey failure did not set terminate flag.
   Fixed in: handshake, rekey, send_packet, recv_packet_raw.

2. send_packet buffer overflow with large MAC digest.
   Fixed: mac_len added to buffer size check.

3. curve25519 OPENSSL_cleanse on NULL raw_secret.
   In x25519_exchange (client side), malloc failure for raw_secret
   fell through to OPENSSL_cleanse(NULL, len) — segfault.
   Fixed: separate the NULL check from the EVP_PKEY_derive check.

4. curve25519 double-free of shared_secret on exchange_hash alloc failure.
   When exchange_hash malloc failed, the error path freed ss_copy but
   left sess->trans.shared_secret pointing to it.  dssh_transport_cleanup
   then freed the same pointer again.  Found by valgrind.
   Fixed: NULL out shared_secret/shared_secret_sz on the error path.

5. curve25519-sha256.c, dh-gex-sha256.c: `ka->pubkey()` error checks
   incorrectly wrapped in `#ifndef DSSH_TESTING`, causing uninitialized
   `k_s_len` to be used on failure (segfault).  Fixed in `dfa73f16f8`.
   Same commit fixed 4 `serialize_bn_mpint()` error checks in dh-gex.

6. ssh-conn.c: dead x11 channel type check — `type_len == 2` with
   `memcmp(ctype, "x11", 3)` could never match.  Deleted.

9. `dssh_session_set_banner_cb()` and `set_global_request_cb()` took
   `void *cb` instead of typed function pointers.  Fixed: both now use
   proper typedefs (`dssh_auth_banner_cb`, `dssh_global_request_cb`).
   Full audit found no other cases.

10. DH-GEX rekey selftests intermittent SIGPIPE.  Test harness was
    not ignoring SIGPIPE; send() to a closed socketpair killed the
    process.  Fixed: `signal(SIGPIPE, SIG_IGN)` in test main.
