# DeuceSSH — Bugs found during branch coverage work

5. `dssh_session_set_banner_cb()` takes `void *cb` instead of a
   typed function pointer.  All the other callback setters
   (`set_debug_cb`, `set_global_request_cb`, etc.) should be
   checked too — the interface should be type-safe.

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
