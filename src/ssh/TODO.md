# DeuceSSH — Bugs found during branch coverage work

(none outstanding)

## Fixed

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
